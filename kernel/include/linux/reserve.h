/*
 * 18-648
 * Team 6
 *
 * header for reserving function
 */

#ifndef _RESERVE_H_
#define _RESERVE_H_

#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/taskmon.h>
#include <asm/page.h>

#define RESERVE_INTERVAL_NS  500000 /* 500 mus resolution */
#define RESERVE_INCREMENT_NS 495000 /* offset overhead in measurement */
#define NS_PER_S 1000000000 /* 10^9 nano sec per sec */
#define NS_PER_MILLI_S 1000000 /* 10^6 nano sec per milli sec */
#define MILLI_S_PER_S 1000 /* 10^3 milli sec per sec */
#define BUF_LENGTH PAGE_SIZE /* max data points */
#define ATTR_NAME_BUF_LENGTH 6 /* max length of sysfs util file, related to max tid */
#define UTIL_MULTIPLE 10000 /* util_in_struct = 10000 * C / T */
#define TASK_SCHEDULING_LENGTH 1000 /* length of array to hold tasks to be shceduled */

/* pointers to reserved tasks */
extern struct task_struct* cpu_tasks[4][TASK_SCHEDULING_LENGTH];

/* the number of tasks reserved on each CPU */
extern int cpu_tasks_length[4];

/* utilization of each CPU */
extern int cpu_util[4];


/* partition policy for assigning tasks to processors */
extern char* partition_policy;

/* whether the bin is open or close */
extern int cpu_open[4];

/* cpu frequency */
extern unsigned int cpu_freq[4];


typedef struct _reserve_s {
    int flag;   /* 0 for reserve disabled, 1 for reserve enabled */
    int suspend; /* 0 for not suspended, 1 for suspended */
    struct timespec C; /* time limit for each period */
    struct timespec T; /* time period */

    int cpu; /* binded cpu id */
    int budget; /* budget in microseconds */
    int period; /* period in microseconds */
    int util; /* theoretical utilization */
    
    struct timespec acc; /* time accumulator */
    struct hrtimer c_timer; /* timer for running time */
    struct hrtimer p_timer; /* timer for period */
    //int p_start; /* 0 for p_timer not started, 1 for started */
    unsigned long long num_p; /* record number of period that has passed */

    unsigned long long total_time; /* total time */

    int sig_sent; /* whether the signal is sent in this period */

    int taskmon_enabled; /* whether taskmon enabled */
    _util_attribute_t tid_attr; /* sysfs virtual file  */
    unsigned long long* periods; /* timestamp */
    int utilization[BUF_LENGTH]; /* decimal part of utilization */
    short start; /* start of the buffer */
    short end; /* end of the buffer */
    int buffer_full; /* whether the buffer is full */
    spinlock_t lock; /* spin lock for buffer  */
    spinlock_t s_lock; /* spin lock for suspend  */

    struct timespec acc_single_run; /* time accumulator during this context switch */
    int tracking_energy; /* 0 for not tracking energy, 1 for tracking energy */
    unsigned long energy; /* energy consumed by this task (muJ) */
    struct kobject *pid_kobj; /* sysfs virtual folder */
    _energy_attribute_t *energy_attr; /* sysfs virtual file  */
} _reserve_t;


/* put it here so govenor can find it */
int sysclock_set_frequency(void);


#endif /* _RESERVE_H_ */
