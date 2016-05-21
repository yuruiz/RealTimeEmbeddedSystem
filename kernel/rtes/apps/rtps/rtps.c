/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 1 Team 6
 *
 * Native Android Application
 */

#include <stdio.h>
#include <linux/unistd.h>
#include <rtes/kernel/ps.h>
#include <stdlib.h>

static int cmp(const void*a, const void*b){
    return ((struct proc_struct*)a)->rt_priority - ((struct proc_struct*)b)->rt_priority;
}

int main(int argc, char *argv[]) {
    int i = 0;
    while(1){
        int count = syscall(__NR_count_rt_threads);
        printf("tid\tpid\tpr\tname\n");
        struct proc_struct* rt_proc_list = malloc(sizeof(struct proc_struct) * count);

        syscall(__NR_list_rt_threads, rt_proc_list, count);
        
        qsort(rt_proc_list, count, sizeof(struct proc_struct), cmp);

        for(i = 0; i < count; i++){
            printf("%d\t%d\t%d\t%s\n", rt_proc_list[i].pid, rt_proc_list[i].tgid, rt_proc_list[i].rt_priority, rt_proc_list[i].comm);
        }

        free(rt_proc_list);
        sleep(2);
    }
    return 0;
}
