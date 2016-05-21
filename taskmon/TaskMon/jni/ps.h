#ifndef PS_H
#define PS_H

struct proc_struct{
	pid_t pid;
	pid_t tgid;
	unsigned int rt_priority;
	char comm[16];
};

#endif
