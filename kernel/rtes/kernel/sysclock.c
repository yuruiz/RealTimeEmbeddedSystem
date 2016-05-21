/*
 *  sysclock implementation
 *  lab 4
 */

#include <linux/reserve.h>
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

#define MAX_TASK 100
#define N_FREQ 8
#define SYSCLOCK_MAX_FREQ 1200
#define SYSCLOCK_FREQ_FACTOR 1000
#define SCALE_FACTOR 100000 /*five digit*/

/* entry for calculation */
typedef struct sysclock_entry_s {
    unsigned long C;
    unsigned long T;
} sysclock_entry_t;

//unsigned int freq_list[]={340, 475, 640, 760, 860, 1000, 1100, 1200};


unsigned int sysclock_compute(void) {
    int i, j, l, n;
    int cpu;
    int n_task;
    unsigned long c_sum, t_sum, tmp_scale = 1 * SCALE_FACTOR;
    unsigned long max_scale;
    struct task_struct *task;
    sysclock_entry_t t_list[MAX_TASK];
    sysclock_entry_t tmp_t;

    /* compute sysclock */
    /* takes the max scale of all cpu */
    max_scale = 0;
    for (cpu = 0; cpu < 4; cpu++) {
        n_task = 0;

        for_each_process(task) {
            if (task->reserve.flag && task->reserve.cpu == cpu) {
                t_list[n_task].C = task->reserve.C.tv_nsec/NS_PER_MILLI_S + task->reserve.C.tv_sec*MILLI_S_PER_S;
                t_list[n_task].T = task->reserve.T.tv_nsec/NS_PER_MILLI_S + task->reserve.T.tv_sec*MILLI_S_PER_S;
                n_task++;

            }
        }

        /* sort according to T */
        for (i = 0; i < n_task; i++) {
            for (j = i + 1; j < n_task; j++) {
                if (t_list[i].T > t_list[j].T) {
                    tmp_t = t_list[i];
                    t_list[i] = t_list[j];
                    t_list[j] = tmp_t;
                }
            }
        }

        /* debug */
        for (i = 0; i < n_task; i++) {
            printk("sysclock: find a task C: %lu u: %lu cpu: %d\n", t_list[i].C, t_list[i].T, cpu);
        }

        /* compute sysclock */
        for (i = 0; i < n_task; i++) {
            tmp_scale = 1 * SCALE_FACTOR;
            for (j = 0; j <= i; j++) {
                for (n = 1; n*t_list[j].T <= t_list[i].T; n++) {/* n times the smaller T */
                    c_sum = 0;
                    t_sum = n*t_list[j].T;
                    for (l = 0; l <= i; l++) {
                        if (t_sum%t_list[l].T != 0) {
                            c_sum += (t_sum/t_list[l].T + 1)*t_list[l].C;
                        } else {
                            c_sum += (t_sum/t_list[l].T)*t_list[l].C;
                        }
                    }

                    printk("sysclock: tmp scale %lu, new scale: %lu cpu: %d\n", tmp_scale, (SCALE_FACTOR*c_sum)/t_sum, cpu);
                    if (((SCALE_FACTOR*c_sum)/t_sum) < tmp_scale) {
                        tmp_scale = (SCALE_FACTOR*c_sum)/t_sum;
                    }
                }
            }
            if (tmp_scale > max_scale) {
                max_scale = tmp_scale;
            }
        }

        printk("sysclock: scale %lu, cpu: %d\n", max_scale, cpu);
    }
    if (max_scale == 0 || max_scale > SCALE_FACTOR) {
        max_scale = SCALE_FACTOR;
    }
    printk("sysclock: final scale %lu, freq: %lu\n", max_scale, (max_scale*SYSCLOCK_MAX_FREQ)/SCALE_FACTOR);

    /*
    for (i = 0; i < N_FREQ - 1; i++) {
        if (((max_scale*SYSCLOCK_MAX_FREQ)/SCALE_FACTOR) <= freq_list[i]) {
            break;
        }
    }
    printk("sysclock: applied freq: %d", freq_list[i]);

    return freq_list[i];
    */
    return ((max_scale*SYSCLOCK_MAX_FREQ)/SCALE_FACTOR)*SYSCLOCK_FREQ_FACTOR;
}



int sysclock_set_frequency(void){
    struct cpufreq_policy policy;
    int cpu;
    int ret = 0;

    for(cpu = 0; cpu < 4; cpu++){
        if (!cpu_online(cpu)) {
            printk("sysclock set: offline %d\n",cpu);     
            continue;
        }

        if(cpufreq_get_policy(&policy,cpu)){
            printk("sysclock set: cannot get policy %d\n",cpu);     
            continue;
        }
        
        if (!strncmp(policy.governor->name, "sysclock",9 )) {
            printk("sysclock set: is sysclock governer %d\n",cpu);     
        } else {
            printk("sysclock set: not sysclock governer %d\n",cpu);     
            continue;
        }
        
        
        if((ret = policy.governor->store_setspeed(&policy,sysclock_compute())) == 0)
            printk( "sysclock set: success\n");
        else
            printk( "sysclock set: fail\n");
            
    }


    return ret;
}
