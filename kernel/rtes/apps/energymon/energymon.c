/*
 * CMU 18-648 Embedded Real-Time Systems
 * Lab 4 Team 6
 *
 * Energy Monitor Native Application
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <rtes/kernel/energy_struct.h>


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: energymon tid\n");
        return -1;
    }

    int retval = 0;
    int tid = atoi(argv[1]);

    if (tid < 0) {
        printf("invalid tid\n");
        return -1;
    }

    struct energy_struct energy_info;
    printf("FREQ (MHZ)\tPOWER (mW)\tENERGY (mJ)\n");
    while (1) {
        if (retval = syscall(__NR_get_energy_info, (pid_t) tid, &energy_info)) {
            printf("Failed to get energy information!\n");
            return retval;
        }
        
        printf("%u\t%u\t%lu\n", energy_info.freq, energy_info.power, energy_info.energy);
        sleep(1);
    }

    return 0;
}
