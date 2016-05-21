#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "resdev"
#define BUF_LEN 200

static int Major;
static int Device_Open = 0;
static char msg[BUF_LEN];
static char *msg_Ptr;
static int process_cnt;

int set_reserve(pid_t tid, struct timespec *C, struct timespec *T, int cpuid);
int cancel_reserve(pid_t tid);

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};


_enabled_attribute_t *ptr_taskmon_enabled_attribute = NULL;

struct kobject *rtes_kobj = NULL;
struct kobject *taskmon_kobj = NULL;
struct kobject *util_kobj = NULL;

struct mutex enabled_mutex;
int enabled_mutex_init = 0;


static ssize_t tid_show(struct kobject *kobj, _util_attribute_t *attr, char *buf) {
    pid_t tid = simple_strtol(attr->attr.name, NULL, 10);
    struct task_struct *t;
    int size = 0;
    int integer_part = 0;
    int i = 0;
    unsigned long flags;
    if ((t = find_task_by_vpid(tid)) == NULL) return -ESRCH;

    spin_lock_irqsave(&(t->reserve.lock), flags);

    while (t->reserve.periods[t->reserve.start] != 0) {
        size += scnprintf(buf + size, PAGE_SIZE, "%llu ", t->reserve.periods[t->reserve.start]);

        if (t->reserve.utilization[t->reserve.start] < 100) {
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

        t->reserve.periods[t->reserve.start] = 0;

        ++(t->reserve.start);
        if (t->reserve.start >= BUF_LENGTH) {
            t->reserve.start -= BUF_LENGTH;
        }
    }

    spin_unlock_irqrestore(&(t->reserve.lock), flags);

    return size;
}


enum hrtimer_restart c_timer_callback(struct hrtimer *timer) {
    struct timespec *acc, *C;
    ktime_t cur_time, interval;

    struct siginfo info;

    /* calculate accumulator */
    acc = &(current->reserve.acc);   
    acc->tv_nsec += RESERVE_INTERVAL_NS;
    if (acc->tv_nsec > MUS_PER_S) {
        acc->tv_nsec -= MUS_PER_S;
        acc->tv_sec++;
    }

    /* test for time limit */
    C = &(current->reserve.C);   
    if (acc->tv_sec > C->tv_sec || (acc->tv_sec == C->tv_sec && acc->tv_nsec >= C->tv_nsec)) {
        if (!current->reserve.sig_sent) {
            /* send SIGEXCESS signal */
            current->reserve.sig_sent = 1;
            memset(&info, 0, sizeof(struct siginfo));
            info.si_signo = SIGEXCESS;
            info.si_code = SI_KERNEL;
            send_sig_info(SIGEXCESS, &info, current);
        }
    }
    
    if (timer == NULL) {
        printk(KERN_WARNING "c_timer null timer?\n");
    }

    /* restart timer */
    cur_time = hrtimer_cb_get_time(timer);
    interval = ktime_set(0 , RESERVE_INTERVAL_NS); 
    hrtimer_forward(timer, cur_time , interval);

    return HRTIMER_RESTART;
}

enum hrtimer_restart p_timer_callback(struct hrtimer *timer) {
    struct timespec *acc;
    ktime_t cur_time, interval;
    
    int computation_time = 0;
    int hundredfold_computation_time = 0;
    int i;
    unsigned long flags;

    /* reset accumulator */
    acc = &(current->reserve.acc);

    printk(KERN_WARNING "period end: %d sec %d nsec \n", acc->tv_sec, acc->tv_nsec);

    /* reset SIGEXCESS signal */
    current->reserve.sig_sent = 0;

    /* increase the number of period that has passed */
    ++(current->reserve.num_p);
    /* increase the total time */
    current->reserve.total_time += current->reserve.period;

    /* output utilization into buffer */
    //mutex_lock(&enabled_mutex);

    if (ptr_taskmon_enabled_attribute && ptr_taskmon_enabled_attribute->enabled) {

        spin_lock_irqsave(&(current->reserve.lock), flags);

        /* find the place to insert data and maintain the circular list */
        ++(current->reserve.end);
        if (current->reserve.end >= BUF_LENGTH) {
            current->reserve.end -= BUF_LENGTH;
        }
        if (current->reserve.end == current->reserve.start
            && current->reserve.periods[current->reserve.start]) {
            ++(current->reserve.start);
            if (current->reserve.start >= BUF_LENGTH) {
                current->reserve.start -= BUF_LENGTH;
            }
        }

        current->reserve.periods[current->reserve.end] = current->reserve.total_time;

        /* calculate computation time in milliseconds */
        for (i = 0; i < acc->tv_sec; ++i) {
            computation_time += 1000;
        }
        for (i = 0; i < acc->tv_nsec; i += 1000000) {
            computation_time += 1;
        }
        for (i = 0; i < 100; ++i) {
            hundredfold_computation_time += computation_time;
        }
        current->reserve.utilization[current->reserve.end] = 0;
        for (i = 0; i < hundredfold_computation_time; i += current->reserve.period) {
            ++(current->reserve.utilization[current->reserve.end]);
        }

        spin_unlock_irqrestore(&(current->reserve.lock), flags);
    }

    //mutex_unlock(&enabled_mutex);


    acc->tv_nsec = 0;
    acc->tv_sec = 0;

    if (timer == NULL) {
        printk(KERN_WARNING "p_timer null timer?\n");
    }
    printk(KERN_WARNING "next interval: %d sec %d nsec \n", current->reserve.T.tv_sec , current->reserve.T.tv_nsec);

    /* restart timer */
    cur_time = hrtimer_cb_get_time(timer);
    interval = ktime_set(current->reserve.T.tv_sec , current->reserve.T.tv_nsec); 
    hrtimer_forward(timer, cur_time , interval);
    
    return HRTIMER_RESTART;
}

int bufreadline(char* srcbuf, int srcsize, char* destbuf, int destsize){
    int i;

    for (i = 0; i < srcsize && i < destsize; ++i) {
	        char c;
	        c = srcbuf[i];
	        destbuf[i] = c;
	        if (c == '\n') {
			            i++;
			            break;
			        }
	        else if (c == '\0') {
			            break;
			        }
	    }

    return i;
}

int init_module(void)
{
        Major = register_chrdev(98, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Device Initialization failed.\n");
	  return Major;
	}

	printk(KERN_INFO "Device Initialization Success at major number %d\n", Major);

	return SUCCESS;
}

void cleanup_module(void)
{
	unregister_chrdev(99, DEVICE_NAME);
	printk(KERN_INFO "The device rmoved Successfully\n");
}

static int device_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
	{
		printk(KERN_INFO "Device already opened by other process, access failed!\n");
		return -EBUSY;
	}

	Device_Open++;
	
	process_cnt = 0;
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);
	printk(KERN_INFO "device opened successfully!\n");

	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;

	module_put(THIS_MODULE);
	printk(KERN_INFO "device released successfully!\n");
	return 0;
}


static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	int tid, Csec, Cnsec, Tsec, Tnsec, cpuID, n;
	char linebuf[1000];
	char* buf;
	buf = kmalloc(len, GFP_KERNEL);
	if(!buf){
		printk("Kmalloc failed\n");
		return -EINVAL;
	}

	memset(buf, 0, len);

	copy_from_user(buf, buff, len);
	
	if((n =bufreadline(buf, len, linebuf, 1000)) == 0){
		printk("Cannot read from buffer\n");
		return -EINVAL;
	}

	if(strstr(buf, "set") == buf){
		if(sscanf(buf, "set %d %d %d %d %d %d", &tid, &Csec, &Cnsec, &Tsec, &Tnsec, &cpuID) == 6){
			if(cpuID < 0 || cpuID > 3){
				return -EINVAL;
			}
			struct timespec Cspec, Tspec;
			Cspec.tv_sec = Csec;
			Cspec.tv_nsec = Cnsec;
			Tspec.tv_sec = Tsec;
			Tspec.tv_nsec = Tnsec;
			if(set_reserve((pid_t)tid, &Cspec, &Tspec, cpuID)){
				printk("set reserve failed\n");
				return -EINVAL;
			};
			return n;
		}
	}else if(strstr(buf, "cancel") == buf){
		if(sscanf(buf, "cancel %d", &tid) == 1){
		
			if(cancel_reserve((pid_t)tid)){
				printk("cancel reserve failed\n");	
			}
			return n;
		}
	}

	printk("Invalid command\n");

	return -EINVAL; 
}

int set_reserve(pid_t tid, struct timespec *C, struct timespec *T, int cpuid) {
    struct cpumask mask;
    struct task_struct *t;
    ktime_t ktime;

    int retval = 0;
    char *buf;
    unsigned long long i = 0;

    if (cpuid < 0 || cpuid > 3) return EINVAL;
    if (C == NULL || T == NULL) return EINVAL;
    if ((t = find_task_by_vpid(tid)) == NULL) return ESRCH;

    /* bind to cpu */
    cpumask_clear(&mask);
    cpumask_set_cpu(cpuid, &mask);
    if (sched_setaffinity(tid, &mask)) {
        printk(KERN_WARNING "sched_setaffinity failed \n");
        return EINVAL;
    }

    /* set parameter */
    t->reserve.C.tv_nsec = C->tv_nsec;
    t->reserve.C.tv_sec = C->tv_sec;
    t->reserve.T.tv_nsec = T->tv_nsec;
    t->reserve.T.tv_sec = T->tv_sec;

    /* convert back to milliseconds */
    for (i = 0; i < t->reserve.T.tv_sec; ++i) {
        t->reserve.period += 1000;
    }
    for (i = 0; i < t->reserve.T.tv_nsec; i += 1000000) {
        t->reserve.period += 1;
    }

    /* initialize timer */
    if (t->reserve.flag == 0) {
        hrtimer_init(&(t->reserve.c_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
        hrtimer_init(&(t->reserve.p_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
        t->reserve.c_timer.function = &c_timer_callback;
        t->reserve.p_timer.function = &p_timer_callback;
        t->reserve.flag = 1;
    }
    
    /* reset p flag */
    t->reserve.p_start = 0;


    /* init mutex */
/*
    if (!enabled_mutex_init) {
        mutex_init(&enabled_mutex);
        enabled_mutex_init = 1;
    }
*/

    /* set up a file as /sys/rtes/taskmon/util/tid  */
    if (util_kobj) {
        buf = (char*) kmalloc(sizeof(char) * ATTR_NAME_BUF_LENGTH, GFP_KERNEL);
        if (!buf) {
            return -ENOMEM;
        }
        snprintf(buf, ATTR_NAME_BUF_LENGTH, "%d", tid);
        t->reserve.tid_attr.attr.name = buf;
        t->reserve.tid_attr.attr.mode = 0666;
        t->reserve.tid_attr.show = tid_show;
        t->reserve.tid_attr.store = NULL;
        retval = sysfs_create_file(util_kobj, (const struct attribute *) &(t->reserve.tid_attr.attr));
        if (retval) {
            return retval;
        }

        /* initialize buffer-related variables */
        memset(t->reserve.periods, 0, sizeof(unsigned long long) * BUF_LENGTH);
        t->reserve.start = 0;
        t->reserve.end = -1;
    }


    return retval;
}

int cancel_reserve(pid_t tid) {
    struct cpumask mask;
    struct task_struct *t;

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
        t->reserve.p_start = 0;
    }


    /* remove the file in /sys/rtes/taskmon/util/tid  */
    if (util_kobj) {
        sysfs_remove_file(util_kobj, (const struct attribute *) &(t->reserve.tid_attr.attr));
    }
    kfree(t->reserve.tid_attr.attr.name);
    return 0;
}
