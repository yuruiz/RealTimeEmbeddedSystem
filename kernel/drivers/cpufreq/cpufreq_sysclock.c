
/*
 *  linux/drivers/cpufreq/cpufreq_sysclock.c
 *  18-648 Lab-4
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reserve.h>
#include <asm/uaccess.h>
/**
 * A few values needed by the sysclock governor
 */
static DEFINE_PER_CPU(unsigned int, cpu_cur_freq); /* current CPU freq */
static DEFINE_MUTEX(sysclock_mutex);
static int cpus_using_sysclock_governor;

/*
int sysclock_all_set()
{
    if (cpus_using_sysclock_governor == num_online_cpus())
        return 1;

    return 0;
}

EXPORT_SYMBOL(sysclock_all_set);
*/

/**
 * cpufreq_set - set the CPU frequency
 * @policy: pointer to policy struct where freq is being set
 * @freq: target frequency in kHz
 *
 * Sets the CPU frequency to freq.
 */
static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
    int ret = -EINVAL;

    printk("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);

    mutex_lock(&sysclock_mutex);

    /*
     * We're safe from concurrent calls to ->target() here
     * as we hold the sysclock_mutex lock. If we were calling
     * cpufreq_driver_target, a deadlock situation might occur:
     * A: cpufreq_set (lock sysclock_mutex) ->
     *      cpufreq_driver_target(lock policy->lock)
     * B: cpufreq_set_policy(lock policy->lock) ->
     *      __cpufreq_governor ->
     *         cpufreq_governor_sysclock (lock sysclock_mutex)
     */
    ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

    per_cpu(cpu_cur_freq, policy->cpu) = freq;

    mutex_unlock(&sysclock_mutex);

    return ret;
}


static ssize_t show_speed(struct cpufreq_policy *policy, char *buf)
{
    return sprintf(buf, "%u\n", per_cpu(cpu_cur_freq, policy->cpu));
}

static int cpufreq_governor_sysclock(struct cpufreq_policy *policy,
        unsigned int event)
{
    unsigned int cpu = policy->cpu;
    int rc = 0;

    switch (event) {
        case CPUFREQ_GOV_START:
            if (!cpu_online(cpu))
                return -EINVAL;

            mutex_lock(&sysclock_mutex);
            cpus_using_sysclock_governor++;
            
            per_cpu(cpu_cur_freq, cpu) = policy->cur;

            mutex_unlock(&sysclock_mutex);

            if(cpus_using_sysclock_governor == num_online_cpus()){
                /* when all online cpu is with sysclock, set freq onece */
                sysclock_set_frequency();
            }


            break;
        case CPUFREQ_GOV_STOP:
            mutex_lock(&sysclock_mutex);
            cpus_using_sysclock_governor--;
            mutex_unlock(&sysclock_mutex);
            break;
        case CPUFREQ_GOV_LIMITS:
            /* we don't need limit event for sysclock */
            break;
    }

    return rc;
}


struct cpufreq_governor cpufreq_gov_sysclock = {
    .name		= "sysclock",
    .governor	= cpufreq_governor_sysclock,
    .store_setspeed	= cpufreq_set,
    .show_setspeed	= show_speed,
    .owner		= THIS_MODULE,
};

static int __init cpufreq_gov_sysclock_init(void)
{
    return cpufreq_register_governor(&cpufreq_gov_sysclock);
}


static void __exit cpufreq_gov_sysclock_exit(void)
{
    cpufreq_unregister_governor(&cpufreq_gov_sysclock);
}


MODULE_AUTHOR("Siqi Wang <siqiw@andrew.cmu.edu>");
MODULE_DESCRIPTION("CPUfreq policy governor 'sysclock'");
MODULE_LICENSE("GPL");

module_init(cpufreq_gov_sysclock_init);
module_exit(cpufreq_gov_sysclock_exit);
