#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/cpufreq.h>
#include <linux/sort.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/reserve.h>
#include <linux/taskmon.h>
#include <linux/energy.h>
#include <linux/spinlock.h>

#include "sysclock.h"

#define UTIL_BOUND_MAX 20 /* accurate utilization bound test for 20 tasks */
#define UTIL_BOUND_INF 6932 /* rough utilization bound */

_enabled_attribute_t *ptr_taskmon_enabled_attribute = NULL;

struct kobject *rtes_kobj = NULL;
struct kobject *taskmon_kobj = NULL;
struct kobject *util_kobj = NULL;

struct mutex enabled_mutex;
int enabled_mutex_init = 0;

/* partition policy for assigning tasks to processors */
char* partition_policy = "NFD";

unsigned long long taskmon_acc_time;

int util_bound[] = { 0, 10000, 8285, 7798, 7569, 7435, 7348, 7287, 7241, 7206, 7178, 7155, 7136, 7120, 7106, 7095, 7084, 7075, 7067, 7060, 7053 };

/* pointers to reserved tasks */
struct task_struct* cpu_tasks[4][TASK_SCHEDULING_LENGTH];

/* the number of tasks reserved on each CPU */
int cpu_tasks_length[4] = { 0 };

/* utilization of each CPU */
int cpu_util[4] = { 0 };

/* whether the bin is open or close */
int cpu_open[4] = { 0 };

/* cpu frequency */
unsigned int cpu_freq[4] = { 0 };

extern int sysclock_all_set();



static ssize_t tid_show(struct kobject *kobj, _util_attribute_t *attr, char *buf) {
    pid_t tid = simple_strtol(attr->attr.name, NULL, 10);
    struct task_struct *t;
    int size = 0;
    int integer_part = 0;
    unsigned long flags;
    if ((t = find_task_by_vpid(tid)) == NULL) return ESRCH;

    spin_lock_irqsave(&(t->reserve.lock), flags);

    //while (t->reserve.periods[t->reserve.start] != 0) {
    while (t->reserve.start != t->reserve.end + 1 || t->reserve.buffer_full) {
        size += scnprintf(buf + size, PAGE_SIZE, "%llu ", t->reserve.periods[t->reserve.start]);

        if (t->reserve.utilization[t->reserve.start] < 10) {
            size += scnprintf(buf + size, PAGE_SIZE, "0.0%d\n", t->reserve.utilization[t->reserve.start]);
        }
        else if (t->reserve.utilization[t->reserve.start] < 100) {
            size += scnprintf(buf + size, PAGE_SIZE, "0.%d\n", t->reserve.utilization[t->reserve.start]);
        }
        /* utilization may be greater than or equal to 100% */
        else {
            while (t->reserve.utilization[t->reserve.start] >= 100) {
                t->reserve.utilization[t->reserve.start] -= 100;
                ++integer_part;
            }
            size += scnprintf(buf + size, PAGE_SIZE, "%d.%d\n", integer_part, t->reserve.utilization[t->reserve.start]);
        }

        //t->reserve.periods[t->reserve.start] = 0;

        ++(t->reserve.start);
        if (t->reserve.start >= BUF_LENGTH) {
            t->reserve.start -= BUF_LENGTH;
        }

        t->reserve.buffer_full = 0;
    }

    spin_unlock_irqrestore(&(t->reserve.lock), flags);

    return size;
}


enum hrtimer_restart c_timer_callback(struct hrtimer *timer) {
    struct timespec *acc, *C;
    unsigned long flags;
    ktime_t cur_time, interval;
    struct siginfo info;
    struct task_struct *cur = container_of(container_of(timer, _reserve_t, c_timer), struct task_struct, reserve);

    /* calculate accumulator of this context switch */
    acc = &(cur->reserve.acc_single_run);
    acc->tv_nsec += RESERVE_INCREMENT_NS;
    if (acc->tv_nsec > NS_PER_S) {
        acc->tv_nsec -= NS_PER_S;
        acc->tv_sec++;
    }

    /* calculate accumulator */
    acc = &(cur->reserve.acc);
    acc->tv_nsec += RESERVE_INCREMENT_NS;
    if (acc->tv_nsec > NS_PER_S) {
        acc->tv_nsec -= NS_PER_S;
        acc->tv_sec++;
    }


    /* test for time limit */
    C = &(cur->reserve.C);
    if (acc->tv_sec > C->tv_sec || (acc->tv_sec == C->tv_sec && acc->tv_nsec > C->tv_nsec)) {
        if (!cur->reserve.sig_sent) {
            /* send SIGEXCESS signal */
			/*
            cur->reserve.sig_sent = 1;
            memset(&info, 0, sizeof(struct siginfo));
            info.si_signo = SIGEXCESS;
            info.si_code = SI_KERNEL;
            send_sig_info(SIGEXCESS, &info, cur);
			*/
        }

        spin_lock_irqsave(&(cur->reserve.s_lock), flags);
        if (!cur->reserve.suspend) {
            cur->state = TASK_UNINTERRUPTIBLE;
            task_thread_info(cur)->flags |= _TIF_NEED_RESCHED;
            cur->reserve.suspend = 1;
            printk(KERN_WARNING "task suspended due to out of budget\n");
        }
        spin_unlock_irqrestore(&(cur->reserve.s_lock), flags);

        return HRTIMER_NORESTART;
    }

    /* restart timer */
    cur_time = hrtimer_cb_get_time(timer);
    interval = ktime_set(0, RESERVE_INTERVAL_NS);
    hrtimer_forward(timer, cur_time, interval);

    return HRTIMER_RESTART;
}

enum hrtimer_restart p_timer_callback(struct hrtimer *timer) {
    struct timespec *acc;
    ktime_t cur_time, interval;
    struct task_struct *cur = container_of(container_of(timer, _reserve_t, p_timer), struct task_struct, reserve);

    int computation_time = 0;
    int prev, cpu;
    unsigned long flags;
    //unsigned long increment;
    //unsigned int power;

    /* reset accumulator */
    acc = &(cur->reserve.acc);

    printk(KERN_WARNING "period end: %d sec %d nsec \n", (int)acc->tv_sec, (int)acc->tv_nsec);

    /* update cpu frequency */
    for_each_online_cpu(cpu) {
        cpu_freq[cpu] = cpufreq_get(cpu) / KHZ_PER_MHZ;
    }

    /* calculate energy */
/*
    if (cur->reserve.tracking_energy) {
        power = freq_to_power[cpufreq_get(cur->reserve.cpu) / KHZ_PER_MHZ - MIN_FREQ];
        increment = power * acc->tv_sec + power * (acc->tv_nsec / NS_PER_MILLI_S) / MILLI_S_PER_S;
        cur->reserve.energy += increment;

        printk("team6: period end: %d sec %d nsec \n", (int)acc->tv_sec, (int)acc->tv_nsec);
        printk("team6: cpu%d: freq: %u power: %u\n", cur->reserve.cpu, cpufreq_get(cur->reserve.cpu) / KHZ_PER_MHZ, power);
        printk("team6: new energy: %lu\n", increment);
        printk("team6: total energy: %lu\n", cur->reserve.energy);
*/
        /* update system energy */
  /*      spin_lock_irqsave(&total_energy_lock, flags);
        total_energy += increment;
        spin_unlock_irqrestore(&total_energy_lock, flags);
        printk("team6: total system energy: %lu\n", total_energy);
    }
*/
    /* reset SIGEXCESS signal */
    cur->reserve.sig_sent = 0;

    /* increase the number of period that has passed */
    ++(cur->reserve.num_p);
    /* increase the total time */
    cur->reserve.total_time += cur->reserve.period;

    /* output utilization into buffer */
    //mutex_lock(&enabled_mutex);

    if (ptr_taskmon_enabled_attribute && ptr_taskmon_enabled_attribute->enabled) {
        spin_lock_irqsave(&(cur->reserve.lock), flags);

        prev = cur->reserve.end;

        /* find the place to insert data and maintain the circular list */
        ++(cur->reserve.end);
        if (cur->reserve.end >= BUF_LENGTH) {
            cur->reserve.end -= BUF_LENGTH;
        }
        if (cur->reserve.end == cur->reserve.start
            && cur->reserve.periods[cur->reserve.start]) {
            cur->reserve.buffer_full = 1;
            ++(cur->reserve.start);
            if (cur->reserve.start >= BUF_LENGTH) {
                cur->reserve.start -= BUF_LENGTH;
            }
        }

        if (cur->reserve.taskmon_enabled && prev != -1) {
            //            cur->reserve.periods[cur->reserve.end] = cur->reserve.periods[prev] + cur->reserve.period;
            cur->reserve.periods[cur->reserve.end] = taskmon_acc_time;
        }
        else {
            cur->reserve.taskmon_enabled = 1;
            cur->reserve.periods[cur->reserve.end] = taskmon_acc_time;
        }

        /* calculate computation time in milliseconds */
        computation_time += 1000 * acc->tv_sec;
        computation_time += acc->tv_nsec / 1000000;
        cur->reserve.utilization[cur->reserve.end] = computation_time * 100 / cur->reserve.period;

        spin_unlock_irqrestore(&(cur->reserve.lock), flags);
    }
    else {
        cur->reserve.taskmon_enabled = 0;
    }

    acc->tv_nsec = 0;
    acc->tv_sec = 0;

    /* remove energy-related sysfs virtual files if taskmon module is removed */
    if (!rtes_kobj && cur->reserve.pid_kobj) {
        printk("team6ptimer: try to remove kobj\n");
        sysfs_remove_file(cur->reserve.pid_kobj, (const struct attribute *) &(cur->reserve.energy_attr->attr));
        kfree(cur->reserve.energy_attr);
        kobject_put(cur->reserve.pid_kobj);
        cur->reserve.pid_kobj = NULL;
        cur->reserve.tracking_energy = 0;
        printk("team6ptimer: remove kobj\n");
    }

    /* resume suspended task */
    spin_lock_irqsave(&(cur->reserve.s_lock), flags);
    if (cur->reserve.suspend) {
        wake_up_process(cur);
        cur->reserve.suspend = 0;
    }
    spin_unlock_irqrestore(&(cur->reserve.s_lock), flags);

    /* restart timer */
    cur_time = hrtimer_cb_get_time(timer);
    interval = ktime_set(cur->reserve.T.tv_sec, cur->reserve.T.tv_nsec);
    hrtimer_forward(timer, cur_time, interval);

    printk(KERN_WARNING "End of period handler \n");

    return HRTIMER_RESTART;
}

/* try to bind a task to one CPU */
int try_bind(struct task_struct *t, int cpuid) {
    int i = cpu_tasks_length[cpuid];
    int util, pos;
    struct task_struct **tasks = cpu_tasks[cpuid];

    /* RT test variables */
    int j, k, curr_budget, prev_budget, appearance;

    /* the cpu is already full of tasks */
    if (i == TASK_SCHEDULING_LENGTH) {
        return EBUSY;
    }

    /* total utilization won't exceed 100% */
    if (t->reserve.util + cpu_util[cpuid] > UTIL_MULTIPLE) {
        return EBUSY;
    }

    /* put this task into task array of related CPU */
    for (i = cpu_tasks_length[cpuid]; i > 0; --i) {
        if (tasks[i - 1]->reserve.period > t->reserve.period) {
            tasks[i] = tasks[i - 1];
        }
        else {
            break;
        }
    }
    tasks[i] = t;
    pos = i; // store the position in case the task need to be removed
    ++cpu_tasks_length[cpuid];
    cpu_util[cpuid] += t->reserve.util;


    /* utilization bound test */
    i = cpu_tasks_length[cpuid];
    util = cpu_util[cpuid];
    while ((i <= UTIL_BOUND_MAX && util_bound[i] < util) || (i > UTIL_BOUND_MAX && UTIL_BOUND_INF < util)) {
        --i;
        util -= tasks[i]->reserve.util;
    }

    /* pass UB test */
    if (i == cpu_tasks_length[cpuid]) return 0;


    /* response time test */
    for (j = i; j < cpu_tasks_length[cpuid]; ++j) {
        prev_budget = 0;
        curr_budget = 0;

        for (k = 0; k <= j; ++k) {
            prev_budget += tasks[k]->reserve.budget;
        }

        while (curr_budget <= tasks[j]->reserve.period && curr_budget != prev_budget) {
            prev_budget = curr_budget;
            curr_budget = tasks[j]->reserve.budget;
            for (k = 0; k < j; ++k) {
                appearance = prev_budget / tasks[k]->reserve.period + (prev_budget % tasks[k]->reserve.period ? 1 : 0);
                curr_budget += appearance * tasks[k]->reserve.budget;
            }
            printk("adding: %d, prev: %d curr: %d\n", tasks[j]->reserve.period, prev_budget, curr_budget);
        }

        /* fail RT-test */
        if (curr_budget > tasks[j]->reserve.period) {
            /* remove the task from the task array of related CPU */
            for (; pos <= cpu_tasks_length[cpuid] - 2; ++pos) {
                tasks[pos] = tasks[pos + 1];
            }
            --cpu_tasks_length[cpuid];
            cpu_util[cpuid] -= t->reserve.util;

            return EBUSY;
        }
    }

    return 0;
}

void sortCPU(int Asced, int* sortResult){
    int maxUtil = -1;
    int minUtil = UTIL_MULTIPLE + 1;
    int maxID, minID, sedMax, sedMin;
    int i, a = -1, b = -1;

    for (i = 0; i < 4; i++){
        if (cpu_util[i] > maxUtil){
            maxUtil = cpu_util[i];
            maxID = i;
        }

        if (cpu_util[i] < minUtil){
            minUtil = cpu_util[i];
            minID = i;
        }
    }

    for (i = 0; i < 4; i++){
        if (i != maxID && i != minID){
            if (a == -1){
                a = i;
            }
            else{
                b = i;
            }
        }
    }

    if (cpu_util[a] > cpu_util[b]){
        sedMax = a;
        sedMin = b;
    }
    else{
        sedMax = b;
        sedMin = a;
    }

    if (Asced == 1){
        sortResult[0] = minID;
        sortResult[1] = sedMin;
        sortResult[2] = sedMax;
        sortResult[3] = maxID;
    }
    else{
        sortResult[0] = maxID;
        sortResult[1] = sedMax;
        sortResult[2] = sedMin;
        sortResult[3] = minID;
    }
}

SYSCALL_DEFINE4(set_reserve, pid_t, tid, struct timespec, *C, struct timespec, *T, int, cpuid) {
    struct cpumask mask;
    struct task_struct *t;

    ktime_t ktime;

    int retval = 0;
    char *buf;

    int computation_time = 0;
    struct task_struct **tasks;
    int period, util, old_cpuid, old_period, old_util, old_budget;
    int i, j, closed_bin = -1;

    int cpu_online[4] = { 0 };
    int cpu;

    printk("Set Reserve Started\n");

/* DEBUG */
/*
        int di, len, dj;
        if (cpuid == 9) {
        for (di = 0; di < 4; ++di) {
        printk("team6: CPU: %d, open:%d\n", di, cpu_open[di]);
        len = cpu_tasks_length[di];
        for (dj = 0; dj < len; ++dj) {
        printk("team6: pid: %d, period: %d, util: %d\n", cpu_tasks[di][dj]->pid, cpu_tasks[di][dj]->reserve.period, cpu_tasks[di][dj]->reserve.util);
        }
        printk("team6: total length: %d, util: %d\n", cpu_tasks_length[di], cpu_util[di]);
        }
        }
*/

    if (cpuid < -1 || cpuid > 3) return EINVAL;
    if (C == NULL || T == NULL) return EINVAL;
    if ((t = find_task_by_vpid(tid)) == NULL) return ESRCH;


    /* convert back to milliseconds */
    period = T->tv_sec * 1000 + T->tv_nsec / 1000000;
    computation_time = C->tv_sec * 1000 + C->tv_nsec / 1000000;

    /* compute theoretical utilization */
    util = computation_time * UTIL_MULTIPLE / period;

    printk("Now is %s\n", partition_policy);

    /* revise a reservation  */
    if (t->reserve.flag) {
        old_cpuid = t->reserve.cpu;
        old_period = t->reserve.period;
        old_util = t->reserve.util;
        old_budget = t->reserve.budget;

        t->reserve.period = period;
        t->reserve.util = util;
        t->reserve.budget = computation_time;

        /* remove the task from the task array of related CPU */
        tasks = cpu_tasks[cpuid];
        for (i = 0; i < cpu_tasks_length[old_cpuid]; ++i) {
            if (tasks[i] == t) {
                break;
            }
        }
        for (; i <= cpu_tasks_length[old_cpuid] - 2; ++i) {
            tasks[i] = tasks[i + 1];
        }
        --cpu_tasks_length[old_cpuid];
        cpu_util[old_cpuid] -= old_util;


        /* try to bind task to one CPU */
        if (cpuid == -1) {
            if (partition_policy[0] == 'F'){

                for (j = 0; j < 4; j++){
                    if (cpu_open[j]) {
                        if ((retval = try_bind(t, j)) == 0){
                            cpuid = j;
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = j;
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }
                }
            }
            else if (partition_policy[0] == 'N'){
                printk("In the NFD1\n");
                printk("Now is %s\n", partition_policy);
                static int lastCpuID = 0;
                for (j = 0; j < 4; j++){
                    if (cpu_open[lastCpuID]) {
                        if ((retval = try_bind(t, lastCpuID)) == 0){
                            cpuid = lastCpuID;
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = lastCpuID;
                    }

                    lastCpuID++;

                    if (lastCpuID == 4){
                        lastCpuID = 0;
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                        lastCpuID = closed_bin;
                    }

                }
            }
            else if (partition_policy[0] == 'B'){
                int sortID[4];
                sortCPU(0, sortID);

                for (j = 0; j < 4; j++){
                    if (cpu_open[sortID[j]]) {
                        if ((retval = try_bind(t, sortID[j])) == 0){
                            cpuid = sortID[j];
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = sortID[j];
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }

                }
            }
            else if (partition_policy[0] == 'W'){
                printk("In the WFD\n");
                int sortID[4];
                sortCPU(1, sortID);

                for (j = 0; j < 4; j++){
                    if (cpu_open[sortID[j]]) {
                        if ((retval = try_bind(t, sortID[j])) == 0){
                            cpuid = sortID[j];
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = sortID[j];
                    }


                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }
                }
            }
            else if (partition_policy[0] == 'L'){
                printk("In the LTD\n");
                int sortID[4];
                sortCPU(1, sortID);

                for (j = 0; j < 4; j++){
                    if ((retval = try_bind(t, sortID[j])) == 0){
                        cpuid = sortID[j];
                        break;
                    }
                }
            }
        }
        else {
            retval = try_bind(t, cpuid);
        }

        if (retval) {
            /* put the task back to its original CPU */
            t->reserve.util = old_util;
            t->reserve.period = old_period;
            t->reserve.budget = old_budget;
            try_bind(t, old_cpuid);
            return retval;
        }
    }
    /* create a new reservation */
    else {
        t->reserve.util = util;
        t->reserve.budget = computation_time;
        t->reserve.period = period;

        /* try to bind task to one CPU */
        if (cpuid == -1) {
            if (partition_policy[0] == 'F'){

                for (j = 0; j < 4; j++){
                    if (cpu_open[j]) {
                        if ((retval = try_bind(t, j)) == 0){
                            cpuid = j;
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = j;
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }
                }
            }
            else if (partition_policy[0] == 'N'){
                printk("In the NFD2\n");
                printk("Now is %s\n", partition_policy);
                static int lastCpuID = 0;
                for (j = 0; j < 4; j++){
                    if (cpu_open[lastCpuID]) {
                        if ((retval = try_bind(t, lastCpuID)) == 0){
                            cpuid = lastCpuID;
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = lastCpuID;
                    }

                    lastCpuID++;

                    if (lastCpuID == 4){
                        lastCpuID = 0;
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                        lastCpuID = closed_bin;
                    }

                }
            }
            else if (partition_policy[0] == 'B'){
                int sortID[4];
                sortCPU(0, sortID);

                for (j = 0; j < 4; j++){
                    if (cpu_open[sortID[j]]) {
                        if ((retval = try_bind(t, sortID[j])) == 0){
                            cpuid = sortID[j];
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = sortID[j];
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }

                }
            }
            else if (partition_policy[0] == 'W'){
                printk("In the WFD\n");
                int sortID[4];
                sortCPU(1, sortID);

                for (j = 0; j < 4; j++){
                    if (cpu_open[sortID[j]]) {
                        if ((retval = try_bind(t, sortID[j])) == 0){
                            cpuid = sortID[j];
                            break;
                        }
                    }
                    else if (closed_bin == -1) {
                        closed_bin = sortID[j];
                    }

                    /* need open a new bin */
                    if (j == 3 && closed_bin != -1) {
                        retval = try_bind(t, closed_bin);
                        cpuid = closed_bin;
                    }
                }
            }
            else if (partition_policy[0] == 'L'){
                printk("In the LTD\n");
                int sortID[4];
                sortCPU(1, sortID);

                for (j = 0; j < 4; j++){
                    if ((retval = try_bind(t, sortID[j])) == 0){
                        cpuid = sortID[j];
                        break;
                    }
                }
            }
        }
        else {
            retval = try_bind(t, cpuid);
        }

        if (retval) {
            return retval;
        }
    }

    if (!t->reserve.flag) {
        /* only when not previously reserved */
        t->reserve.suspend = 0;
        t->reserve.taskmon_enabled = 0;
        t->reserve.lock = __SPIN_LOCK_UNLOCKED();
        t->reserve.s_lock = __SPIN_LOCK_UNLOCKED();
    }


    /* bind to cpu */
    cpumask_clear(&mask);
    cpumask_set_cpu(cpuid, &mask);
    /* check which cpus are online */
    for_each_online_cpu (cpu) {
        cpu_online[cpu] = 1;
 //       printk("team6cpu: %d\n", cpu);
    }
    /* online the cpu if necessary */
    if (!cpu_online[cpuid]) {
        cpu_up(cpuid);
        cpu_online[cpuid] = 1;
    }
 /*   for_each_online_cpu (cpu) {
        printk("team6cpu after up: %d\n", cpu);
    }*/
    if (sched_setaffinity(tid, &mask)) {
        /* remove the task from the task array of related CPU */
        tasks = cpu_tasks[cpuid];
        for (i = 0; i < cpu_tasks_length[cpuid]; ++i) {
            if (tasks[i] == t) {
                break;
            }
        }
        for (; i <= cpu_tasks_length[cpuid] - 2; ++i) {
            tasks[i] = tasks[i + 1];
        }
        --cpu_tasks_length[cpuid];
        cpu_util[cpuid] -= t->reserve.util;

        /* put the task back to its original CPU when revising a reservation */
        if (t->reserve.flag) {
            t->reserve.util = old_util;
            t->reserve.period = old_period;
            t->reserve.budget = old_budget;
            try_bind(t, old_cpuid);
        }

        printk(KERN_WARNING "sched_setaffinity failed \n");
        return EINVAL;
    }

    /* assign CPU ID */
    t->reserve.cpu = cpuid;

    /* the bin is open */
    //cpu_open[cpuid] = 1;
    for (i = 0; i < 4; ++i) {
        if (cpu_util[i]) cpu_open[i] = 1;
        else cpu_open[i] = 0;
    }

/*for_each_online_cpu(cpu) {
printk("team6cpu: %d\n", cpu);
}*/

    /* offline cpu if necessary */
    /* skip cpu0 */
    for (i = 0; i < 4; ++i) {
//printk("team6cpu_util: cpu%d: cpu_util: %d, cpu_open: %d\n", i, cpu_util[i], cpu_open[i]);
        if (!cpu_open[i] && cpu_online[i] && i) {
//printk("team6cpu: try down %d\n", i);
            cpu_down(i);
        }
    }
/*
for_each_online_cpu(cpu) {
printk("team6cpu after down: %d\n", cpu);
}
*/


    /* set parameter */
    t->reserve.C.tv_nsec = C->tv_nsec;
    t->reserve.C.tv_sec = C->tv_sec;
    t->reserve.T.tv_nsec = T->tv_nsec;
    t->reserve.T.tv_sec = T->tv_sec;

    t->reserve.acc_single_run.tv_nsec = 0;
    t->reserve.acc_single_run.tv_sec = 0;

    /* initialize timer */
    if (t->reserve.flag) {
        /* if already reserved */
        hrtimer_cancel(&(t->reserve.c_timer));
        hrtimer_cancel(&(t->reserve.p_timer));
    }
    hrtimer_init(&(t->reserve.c_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
    hrtimer_init(&(t->reserve.p_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
    t->reserve.c_timer.function = &c_timer_callback;
    t->reserve.p_timer.function = &p_timer_callback;
    /* start p timer right away */
    ktime = ktime_set(t->reserve.T.tv_sec, t->reserve.T.tv_nsec);
    hrtimer_start(&(t->reserve.p_timer), ktime, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);

    /* set up a file as /sys/rtes/taskmon/util/tid  */
    if (util_kobj && !t->reserve.flag) {
        buf = (char*) kmalloc(sizeof(char) * ATTR_NAME_BUF_LENGTH, GFP_KERNEL);
        if (!buf) {
            printk("Set Reserve: failed to allocate memory\n");
            return ENOMEM;
        }
        snprintf(buf, ATTR_NAME_BUF_LENGTH, "%d", tid);
        t->reserve.tid_attr.attr.name = buf;
        t->reserve.tid_attr.attr.mode = 0666;
        t->reserve.tid_attr.show = tid_show;
        t->reserve.tid_attr.store = NULL;
        retval = sysfs_create_file(util_kobj, (const struct attribute *) &(t->reserve.tid_attr.attr));
        if (retval) {
            printk("Set Reserve: failed to create file\n");
            return retval;
        }

        /* initialize buffer-related variables */
        t->reserve.periods = (unsigned long long*) kmalloc(sizeof(unsigned long long) * BUF_LENGTH, GFP_KERNEL);
        if (!(t->reserve.periods)) {
            printk("Set Reserve: failed to allocate memory\n");
            return ENOMEM;
        }
        memset(t->reserve.periods, 0, sizeof(unsigned long long) * BUF_LENGTH);
        t->reserve.start = 0;
        t->reserve.end = -1;
        t->reserve.buffer_full = 0;
    }

    /* set up energy-related sysfs virtual files */
    if (tasks_kobj && !t->reserve.pid_kobj && energy_tracking) {
        buf = (char*) kmalloc(sizeof(char) * ATTR_NAME_BUF_LENGTH, GFP_KERNEL);
        if (!buf) {
            printk("Set Reserve: failed to allocate memory\n");
            return ENOMEM;
        }
        snprintf(buf, ATTR_NAME_BUF_LENGTH, "%d", tid);

        t->reserve.pid_kobj = kobject_create_and_add(buf, tasks_kobj);
        if (!(t->reserve.pid_kobj)) {
            printk("Set Reserve: failed to create kobject\n");
            return ENOMEM;
        }

        t->reserve.energy_attr = (_energy_attribute_t *) kmalloc(sizeof(_energy_attribute_t), GFP_KERNEL);
        if (!t->reserve.energy_attr) {
            printk("Set Reserve: failed to allocate memory\n");
            return ENOMEM;
        }
        t->reserve.energy_attr->attr.name = "energy";
        t->reserve.energy_attr->attr.mode = 0444;
        t->reserve.energy_attr->show = energy_show;
        t->reserve.energy_attr->store = NULL;
        t->reserve.energy_attr->task = t;
        retval = sysfs_create_file(t->reserve.pid_kobj, (const struct attribute *) &(t->reserve.energy_attr->attr));
        if (retval) {
            printk("Set Reserve: failed to create file\n");
            return retval;
        }
    }
    else if (tasks_kobj && !energy_tracking && t->reserve.pid_kobj) {
        sysfs_remove_file(t->reserve.pid_kobj, (const struct attribute *) &(t->reserve.energy_attr->attr));
        kfree(t->reserve.energy_attr);
        kobject_put(t->reserve.pid_kobj);
        t->reserve.pid_kobj = NULL;
    }

    t->reserve.energy = 0;
    t->reserve.tracking_energy = energy_tracking;

    t->reserve.flag = 1;
    
    /* sysclock */
    sysclock_set_frequency();


    return 0;
}

SYSCALL_DEFINE1(cancel_reserve, pid_t, tid) {
    int cpuid, i;
    struct cpumask mask;
    struct task_struct *t;
    struct task_struct **tasks;

    if ((t = find_task_by_vpid(tid)) == NULL) return ESRCH;

    if (t->reserve.flag) {
        /* stop timer */
        hrtimer_cancel(&(t->reserve.c_timer));
        hrtimer_cancel(&(t->reserve.p_timer));

        /* unbind */
        cpumask_clear(&mask);
        cpumask_set_cpu(0, &mask);
        cpumask_set_cpu(1, &mask);
        cpumask_set_cpu(2, &mask);
        cpumask_set_cpu(3, &mask);
        if (sched_setaffinity(tid, &mask)) {
            printk(KERN_WARNING "sched_setaffinity failed \n");
            return EINVAL;
        }

        t->reserve.flag = 0;

        /* remove the task from the task array of related CPU */
        cpuid = t->reserve.cpu;
        tasks = cpu_tasks[cpuid];
        for (i = 0; i < cpu_tasks_length[cpuid]; ++i) {
            if (tasks[i] == t) {
                break;
            }
        }
        for (; i <= cpu_tasks_length[cpuid] - 2; ++i) {
            tasks[i] = tasks[i + 1];
        }
        --cpu_tasks_length[cpuid];
        cpu_util[cpuid] -= t->reserve.util;

        /* close the bin if necessary */
        if (!cpu_util[cpuid]) {
            cpu_open[cpuid] = 0;
        }

        /* offline the cpu if necessary */
        /* skip cpu0 */
        if (!cpu_open[cpuid] && cpuid) {
            //printk("team6cpu: try down: %d\n", cpuid);
            cpu_down(cpuid);
        }
    }


    /* remove the file in /sys/rtes/taskmon/util/tid  */
    if (util_kobj) {
        kfree(t->reserve.tid_attr.attr.name);
        kfree(t->reserve.periods);
        sysfs_remove_file(util_kobj, (const struct attribute *) &(t->reserve.tid_attr.attr));
    }

    /* remove energy-related sysfs virtual files  */
    if (tasks_kobj && t->reserve.pid_kobj) {
        sysfs_remove_file(t->reserve.pid_kobj, (const struct attribute *) &(t->reserve.energy_attr->attr));
        kfree(t->reserve.energy_attr);
        kobject_put(t->reserve.pid_kobj);
        t->reserve.pid_kobj = NULL;
        t->reserve.tracking_energy = 0;
    }

    /* sysclock */
    sysclock_set_frequency();

    return 0;
}

SYSCALL_DEFINE0(end_job) {
    unsigned long flags;

    if (!current->reserve.flag) return -1;

    //printk(KERN_WARNING "In end_job \n");

    spin_lock_irqsave(&(current->reserve.s_lock), flags);
    if (!current->reserve.suspend) {
        set_current_state(TASK_UNINTERRUPTIBLE);
        //task_thread_info(current)->flags |= _TIF_NEED_RESCHED;
        current->reserve.suspend = 1;
    }
    spin_unlock_irqrestore(&(current->reserve.s_lock), flags);
    schedule();
    //printk(KERN_WARNING "End of end_job \n");
    return 0;
}
