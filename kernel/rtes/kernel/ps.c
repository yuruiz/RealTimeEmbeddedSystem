#include <linux/module.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <asm/uaccess.h>
#include "ps.h"
#define RT_UP_LIMIT
#define RL_LOW_LIMIT

SYSCALL_DEFINE0(count_rt_threads){
	struct task_struct *p;
	long threads_count = 0;

	printk("Start count rt thread\n");
	rcu_read_lock();
	for_each_process(p){
	
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR){
			threads_count++;
		}
	}
	rcu_read_unlock();

	return threads_count;
}


SYSCALL_DEFINE2(list_rt_threads, struct proc_struct*, list, int, len){
	struct task_struct *p;
	long threads_count = 0;
	rcu_read_lock();
	for_each_process(p)
	{
		if(len == 0){
			break;
		}
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR){
			copy_to_user(&list[threads_count].pid, &p->pid, 4);
			copy_to_user(&list[threads_count].tgid, &p->tgid, 4);
			copy_to_user(&list[threads_count].rt_priority, &p->rt_priority, 4);
			copy_to_user(list[threads_count].comm, p->comm, 16); 
			threads_count++;
			len--;
		}
	}
	rcu_read_unlock();
    return 0;
}
