/*
 * CMU 18-648 Embedded Real-Time Systems
 * Team 6
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/cpufreq.h>
#include <linux/reserve.h>
#include <linux/taskmon.h>
#include <linux/energy.h>

#define MODULE_NAME "taskmon"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TEAM 6");

extern _enabled_attribute_t *ptr_taskmon_enabled_attribute;

extern struct kobject *rtes_kobj;
extern struct kobject *taskmon_kobj;
extern struct kobject *util_kobj;
struct kobject *config_kobj = NULL;
extern struct kobject *tasks_kobj;

extern struct mutex enabled_mutex;
extern int enabled_mutex_init;

struct hrtimer taskmon_timer;


/* time passed in millisecond */
extern unsigned long long taskmon_acc_time;

int count = 0;


enum hrtimer_restart taskmon_timer_callback(struct hrtimer *timer) {
    ktime_t cur_time, interval;

    /* compute time in millisecond */
    if (++count == 10) {
        count = 0;
        ++taskmon_acc_time;
    }

    /* restart timer */
    cur_time = hrtimer_cb_get_time(timer);
    interval = ktime_set(0, RESERVE_INTERVAL_NS);
    hrtimer_forward(timer, cur_time, interval);

    return HRTIMER_RESTART;
}


static ssize_t enabled_show(struct kobject *kobj, _enabled_attribute_t *attr, char *buf) {
 //   ssize_t size = 0;
 //   mutex_lock(&enabled_mutex);
 //   scnprintf(buf, PAGE_SIZE, "%d\n", attr->enabled);
 //   mutex_unlock(&enabled_mutex);
 //   return size;
    return scnprintf(buf, PAGE_SIZE, "%d\n", attr->enabled);
}

 
static ssize_t enabled_store(struct kobject *kobj, _enabled_attribute_t *attr, const char *buf, size_t count) {
//    mutex_lock(&enabled_mutex);
    ktime_t ktime;

    sscanf(buf, "%d", &attr->enabled);

    if (attr->enabled) {
        /* start timer */
        ktime = ktime_set(0, RESERVE_INTERVAL_NS);
        hrtimer_start(&taskmon_timer, ktime, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
        taskmon_acc_time = 0;
    }
    else {
        /* stop timer */
        hrtimer_cancel(&taskmon_timer);
    }
//    mutex_unlock(&enabled_mutex);
    return 1;//sizeof(int);
}

static _enabled_attribute_t taskmon_enabled_attribute = __ATTR(enabled, 0666, enabled_show, enabled_store);


static ssize_t reserves_show(struct kobject *kobj, _reserves_attribute_t *attr, char *buf) {
    struct task_struct *p;
    ssize_t retval = 0;
    retval += scnprintf(buf, PAGE_SIZE, "TID\tPID\tPRIO\tCPU\tNAME\n");

    for_each_process(p) {
        if (p->reserve.flag) {
            retval += scnprintf(buf + retval, PAGE_SIZE, "%d\t%d\t%u\t%u\t%s\n", p->pid, p->pid, p->rt_priority, p->reserve.cpu, p->comm);
        }
    }

    return retval;
}

static _reserves_attribute_t reserves_attribute = __ATTR(reserves, 0444, reserves_show, NULL);


static ssize_t partition_policy_show(struct kobject *kobj, _partition_policy_attribute_t *attr, char *buf) {
    return scnprintf(buf, PAGE_SIZE, "%s\n", partition_policy);
}

static ssize_t partition_policy_store(struct kobject *kobj, _partition_policy_attribute_t *attr, const char *buf, size_t count) {
    int i;

    /* check whether there are active reservations */
    for (i = 0; i < 4; ++i) {
        if (cpu_tasks_length[i]) return EBUSY;
    }

    if (!strcmp(buf, "FFD\n") || !strcmp(buf, "NFD\n") || !strcmp(buf, "BFD\n") || !strcmp(buf, "WFD\n") || !strcmp(buf, "LST\n")) {
        strncpy(partition_policy, buf, 3);
        return count;
    }
    else {
        return EINVAL;
    }
}

static _partition_policy_attribute_t partition_policy_attribute = __ATTR(parition_policy, 0666, partition_policy_show, partition_policy_store);


static ssize_t freq_show(struct kobject *kobj, _freq_attribute_t *attr, char *buf) {
    int retval = 0, cpu;
    for_each_online_cpu(cpu) {
        retval = scnprintf(buf, PAGE_SIZE, "%u\n", cpufreq_get(cpu) / KHZ_PER_MHZ);
        break;
    }
    return retval;
}

static _freq_attribute_t freq_attribute = __ATTR(freq, 0444, freq_show, NULL);


static ssize_t power_show(struct kobject *kobj, _power_attribute_t *attr, char *buf) {
    int retval, cpu;
    unsigned int freq = 0;

    for_each_online_cpu(cpu) {
        freq = cpufreq_get(cpu) / KHZ_PER_MHZ;
        break;
    }

    if (freq >= MIN_FREQ && freq <= MAX_FREQ) {
        retval = scnprintf(buf, PAGE_SIZE, "%u\n", freq_to_power[freq - MIN_FREQ] * num_online_cpus() / MICRO_J_PER_MILLI_J);
    }
    else {
        retval = scnprintf(buf, PAGE_SIZE, "0\n");
    }

    return retval;
}

static _power_attribute_t power_attribute = __ATTR(power, 0444, power_show, NULL);


static ssize_t energy_tracking_show(struct kobject *kobj, _energy_tracking_attribute_t *attr, char *buf) {
    return scnprintf(buf, PAGE_SIZE, "%d\n", energy_tracking);
}

static ssize_t energy_tracking_store(struct kobject *kobj, _energy_tracking_attribute_t *attr, const char *buf, size_t count) {
    /* initialize the spin lock if necessary */
    if (!lock_initialized) {
        total_energy_lock = __SPIN_LOCK_UNLOCKED();
        lock_initialized = 1;
    }

    sscanf(buf, "%d", &energy_tracking);
    return 1;
}

static _energy_tracking_attribute_t energy_tracking_attribute = __ATTR(energy, 0666, energy_tracking_show, energy_tracking_store);


static ssize_t total_energy_show(struct kobject *kobj, _total_energy_attribute_t *attr, char *buf) {
    return scnprintf(buf, PAGE_SIZE, "%lu\n", total_energy / MICRO_J_PER_MILLI_J);
}
 
static ssize_t total_energy_store(struct kobject *kobj, _total_energy_attribute_t *attr, const char *buf, size_t count) {
    unsigned long flags;

    /* initialize the spin lock if necessary */
    if (!lock_initialized) {
        total_energy_lock = __SPIN_LOCK_UNLOCKED();
        lock_initialized = 1;
    }

    spin_lock_irqsave(&total_energy_lock, flags);
    total_energy = 0;
    spin_unlock_irqrestore(&total_energy_lock, flags);
    return 1;
}

static _total_energy_attribute_t total_energy_attribute = __ATTR(energy, 0666, total_energy_show, total_energy_store);


static int taskmon_init(void) {
    int retval = 0;

/*
    if (!enabled_mutex_init) {
        mutex_init(&enabled_mutex);
        enabled_mutex_init = 1;
    }

    mutex_lock(&enabled_mutex);
*/
    rtes_kobj = kobject_create_and_add("rtes", NULL);
    if (!rtes_kobj) {
//        mutex_unlock(&enabled_mutex);
        return ENOMEM;
    }

    taskmon_kobj = kobject_create_and_add("taskmon", rtes_kobj);
    if (!taskmon_kobj) {
//        mutex_unlock(&enabled_mutex);
        return ENOMEM;
    }

    util_kobj = kobject_create_and_add("util", taskmon_kobj);
    if (!util_kobj) {
//        mutex_unlock(&enabled_mutex);
        return ENOMEM;
    }

    config_kobj = kobject_create_and_add("config", rtes_kobj);
    if (!config_kobj) {
        return ENOMEM;
    }

    tasks_kobj = kobject_create_and_add("tasks", rtes_kobj);
    if (!tasks_kobj) {
        return ENOMEM;
    }

    retval = sysfs_create_file(taskmon_kobj, (const struct attribute *) &taskmon_enabled_attribute.attr);
    if (retval) {
        return retval;
    }

    retval = sysfs_create_file(rtes_kobj, (const struct attribute *) &reserves_attribute.attr);
    if (retval) {
        return retval;
    }

    retval = sysfs_create_file(rtes_kobj, (const struct attribute *) &partition_policy_attribute.attr);
    if (retval) {
        return retval;
    }

    retval = sysfs_create_file(rtes_kobj, (const struct attribute *) &freq_attribute.attr);
    if (retval) {
        return retval;
    }

    retval = sysfs_create_file(rtes_kobj, (const struct attribute *) &power_attribute.attr);
    if (retval) {
        return retval;
    }
  
    retval = sysfs_create_file(config_kobj, (const struct attribute *) &energy_tracking_attribute.attr);
    if (retval) {
        return retval;
    }

    retval = sysfs_create_file(rtes_kobj, (const struct attribute *) &total_energy_attribute.attr);
    if (retval) {
        return retval;
    }

    ptr_taskmon_enabled_attribute = &taskmon_enabled_attribute;

    /* initialize timer */
    hrtimer_init(&taskmon_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
    taskmon_timer.function = &taskmon_timer_callback;

//    mutex_unlock(&enabled_mutex);

    return retval;
}


static void taskmon_exit(void) {
//    mutex_lock(&enabled_mutex);
    kobject_put(util_kobj);
    util_kobj = NULL;
    kobject_put(taskmon_kobj);
    taskmon_kobj = NULL;
    kobject_put(config_kobj);
    config_kobj = NULL;
    kobject_put(tasks_kobj);
    tasks_kobj = NULL;
    kobject_put(rtes_kobj);
    rtes_kobj = NULL;

    ptr_taskmon_enabled_attribute->enabled = 0;
    ptr_taskmon_enabled_attribute = NULL;

    /* stop timer */
    hrtimer_cancel(&taskmon_timer);
//    mutex_unlock(&enabled_mutex);
}


module_init(taskmon_init);
module_exit(taskmon_exit);
