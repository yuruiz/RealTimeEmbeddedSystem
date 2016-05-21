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

int main(int argc, char *argv[]) {
    if (argc != 6 && argc != 3) {
        printf("Usaage: reserve set tid C T cpu\n");
        printf("        reserve cancel tid\n");
        return -1;
    }
    
    if (argc == 6 && strcmp(argv[1], "set") == 0) {
        int tid = atoi(argv[2]);
        int c = atoi(argv[3]);
        int t = atoi(argv[4]);
        int cpu = atoi(argv[5]);
        
        struct timespec C, T;
        C.tv_sec = c/1000;
        C.tv_nsec = (c%1000)*1000000;
        T.tv_sec = t/1000;
        T.tv_nsec = (t%1000)*1000000;

        if (c > t) {
            printf("Usage: c should be smaller than t\n");
            return -1;
        }
        if (cpu < 0 || cpu > 3) {
            printf("Usaage: invalid cpu\n");
            return -1;
        }
        if (syscall(__NR_set_reserve, (pid_t)tid, &C, &T, cpu)) {
            printf("set reserve failed\n");
            return -1;
        }
        
    }
    else if (argc == 3 && strcmp(argv[1], "cancel") == 0) {
        int tid = atoi(argv[2]);
        if (syscall(__NR_cancel_reserve, (pid_t)tid)) {
            printf("cancel reserve failed\n");
            return -1;
        }
    }


    return 0;
}
