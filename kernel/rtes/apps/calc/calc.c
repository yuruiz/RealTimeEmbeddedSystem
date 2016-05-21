/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 1 Team 6
 *
 * Calculator Application
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/unistd.h>


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: calc [+-]?[0-9]+ [+-*/] [+-]?[0-9]+\n");
        return -1;
    }
    
    float f1 = atof(argv[1]);
    float f2 = atof(argv[3]);
    char op = argv[2][0];

    //use memcpy to avoid type conversion of arguments
    unsigned int i1, i2;
    memcpy(&i1, &f1, sizeof(float));
    memcpy(&i2, &f2, sizeof(float));
    
    unsigned int res_i = syscall(__NR_calc, i1, i2, op);
    float res_f;
    memcpy(&res_f, &res_i, sizeof(float));

    if (res_i == EINVAL)
        printf("nan\n");
    else {
        printf("%f\n", res_f);
    }
    return 0;
}
