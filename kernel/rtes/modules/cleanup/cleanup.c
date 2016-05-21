/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 1 Team 6
 *
 * Override syscalls at runtime
 *
 * An LKM that when loaded intercepts when a process exits
 * and reports the paths of files it left open to the kernel log.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/dcache.h>
#include <linux/slab.h>


#define MODULE_NAME "cleanup"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TEAM 6");

// only spy on the processes whose names contain an user-specified substring
static char* comm = "";
module_param(comm, charp, 0);
MODULE_PARM_DESC(comm, "processes whose names contain a substring to spy on");

// base address of the system call table
extern void *sys_call_table[];

asmlinkage int (*original_exit)(int);
asmlinkage int (*original_exit_group)(int);


/*
 * This function checks the file descriptors a process left open when it exits.
 */
asmlinkage void check_file_descriptors(void) {
    struct fdtable *files_table;
    char *path;
    char *buf = (char *) kmalloc(GFP_KERNEL, 100 * sizeof(char));
    int i, first_time = 0;

    // check whether the process has a substring specified by user
    if (strstr(current->comm, comm) != NULL) {
        files_table = files_fdtable(current->files);

        // skip standard input, standard output and standard error
        for (i = 3; i < files_table->max_fds; ++i) {
            if (files_table->fd[i] != NULL) {
                if (++first_time == 1) {
                    printk(KERN_ALERT "%s: process '%s' (PID %d) did not close files:",
                           MODULE_NAME, current->comm, current->pid);
                }

                // retrieve the actual path
                path = d_path(&(files_table->fd[i]->f_path), buf, 100 * sizeof(char));
                printk(KERN_ALERT "%s: %s", MODULE_NAME, path);
            }
        }
    }
}


/*
 * This function is a wrapper arround the original __NR_exit system call
 * and writes the the paths of files a process left open to the kernel log.
 */
asmlinkage int custom_exit(int error_code) {
    check_file_descriptors();

    // call the original __NR_exit system call to maintain functionality
    return original_exit(error_code);
}


/*
 * This function is a wrapper arround the original __NR_exit_group system call
 * and writes the the paths of files a process left open to the kernel log.
 */
asmlinkage int custom_exit_group(int error_code) {
    check_file_descriptors();

    // call the original __NR_exit_group system call to maintain functionality
    return original_exit_group(error_code);
}


/* This function is called when the module is loaded */
static int cleanup_init(void) {
    // check whether user has passed a parameter
    if (strcmp(comm, "") == 0) {
        return -1;
    }

    // store references to the original system calls
    original_exit = sys_call_table[__NR_exit];
    original_exit_group = sys_call_table[__NR_exit_group];

    // manipulate sys_call_table to call custom methods instead of original ones
    sys_call_table[__NR_exit] = custom_exit;
    sys_call_table[__NR_exit_group] = custom_exit_group;

    return 0;
}


/* This function is called when the module is unloaded */
static void cleanup_exit(void) {
    // restore original system calls
    sys_call_table[__NR_exit] = original_exit;
    sys_call_table[__NR_exit_group] = original_exit_group;
}


module_init(cleanup_init);
module_exit(cleanup_exit);
