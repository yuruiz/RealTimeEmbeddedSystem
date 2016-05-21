/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 1 Team 6
 *
 * Native Android Application
 *
 * Print "hello world" to the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    int fd = open("temp.txt", O_RDONLY);
    return 0;
}
