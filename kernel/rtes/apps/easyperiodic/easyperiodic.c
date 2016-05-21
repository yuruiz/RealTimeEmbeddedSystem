/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 2 Team 6
 *
 * Periodic Application
 */

#define _GNU_SOURCE /* for sched_setaffinity */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/time.h>
#include <linux/unistd.h>

/* just manually define it here */
#define CPU_SETSIZE 1024
#define __NCPUBITS  (8 * sizeof (unsigned long))
typedef struct
{
    unsigned long __bits[CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

#define CPU_SET(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu)/__NCPUBITS] |= (1UL << ((cpu) % __NCPUBITS)))
#define CPU_ZERO(cpusetp) \
    memset((cpusetp), 0, sizeof(cpu_set_t))


/* globals */
int t, c;
int work = 0;
struct itimerval w_timer, p_timer;

void work_handler (int signum) {
    syscall(__NR_end_job);
}

void period_handler (int signum) {
    w_timer.it_value.tv_sec = c/1000;
    w_timer.it_value.tv_usec = (c%1000)*1000;

    /* one time */
    w_timer.it_interval.tv_sec = 0;
    w_timer.it_interval.tv_usec = 0;

    /* virtual time for period */
    setitimer(ITIMER_VIRTUAL, &w_timer, NULL);
}

void excess_handler (int signum) {
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usaage: periodic C T cpu\n");
        return -1;
    }

    c = atoi(argv[1]);
    t = atoi(argv[2]);
    int cpu = atoi(argv[3]);

    if (c > t) {
        printf("Usaage: c should be smaller than t\n");
        return -1;
    }
    if (c > 60000 || t > 60000) {
        printf("Usaage: c and t must be less than 60,000\n");
        return -1;
    }
    if (cpu < 0 || cpu > 10) {//3) {
        printf("Usaage: invalid cpu\n");
        return -1;
    }

    printf("My tid is %d\n", syscall(__NR_gettid));


    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(syscall(__NR_gettid), sizeof(cpu_set_t), &set);

    struct sigaction sa_p;
    struct sigaction sa_w;
    struct sigaction sa_excess;

    /* set up handlers */
    memset(&sa_p, 0, sizeof (sa_p));
    memset(&sa_w, 0, sizeof (sa_w));
    memset(&sa_excess, 0, sizeof (sa_excess));
    sa_p.sa_handler = &period_handler;
    sa_w.sa_handler = &work_handler;
    sa_excess.sa_handler = &excess_handler;
    sigaction(SIGALRM, &sa_p, NULL);
    sigaction(SIGVTALRM, &sa_w, NULL);
    sigaction(SIGEXCESS, &sa_excess, NULL);

    /* immediately set off */
    p_timer.it_value.tv_sec = t/1000;
    p_timer.it_value.tv_usec = (t%1000)*1000;

    /* periodic */
    p_timer.it_interval.tv_sec = t/1000;
    p_timer.it_interval.tv_usec = (t%1000)*1000;

    /* real time for period */
    setitimer(ITIMER_REAL, &p_timer, NULL);

    struct timespec C, T;
    C.tv_sec = c/1000;
    C.tv_nsec = (c%1000)*1000000;
    T.tv_sec = t/1000;
    T.tv_nsec = (t%1000)*1000000;

    syscall(__NR_set_reserve, syscall(__NR_gettid), &C, &T, cpu);

    while (1) {
        work++; /* avoid optimization */
    }

    return 0;
    }
