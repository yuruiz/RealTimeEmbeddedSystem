/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 1 Team 6
 *
 * Native Android Application
 *
 * A small application printing hello world to the console. 
 */

#include <stdio.h>
#include <linux/unistd.h>


int main(int argc, char *argv[]) {
    printf("Hello, world! It's warm and cozy here in user-space.\n");
    return 0;
}
