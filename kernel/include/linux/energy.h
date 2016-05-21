/*
 * 18-648
 * Team 6
 *
 * header for energy tracking
 */

#ifndef _ENERGY_H_
#define _ENERGY_H_

#include <linux/spinlock.h>
#include <linux/taskmon.h>

/* minimum cpu frequency (MHz) */
#define MIN_FREQ 51

/* maximum cpu frequency (MHz) */
#define MAX_FREQ 1300

/* 1000 KHz per MHz */
#define KHZ_PER_MHZ 1000

/* 1000 muJ per mJ */
#define MICRO_J_PER_MILLI_J 1000

/* 1000 muW per mW */
#define MICRO_W_PER_MILLI_W 1000

/* 0 for not tracing energy, 1 for tracking */
extern int energy_tracking;

/* total energy consumed by the system (muJ) */
extern unsigned long total_energy;

/* spin lock for total_energy */
extern spinlock_t total_energy_lock;

/* 0 for not initializing spin lock yet, 1 for already initializing */
extern int lock_initialized;

/* lookup table from cpu frequency (MHz) to power consumption (muW) */
extern unsigned int freq_to_power[];

ssize_t energy_show(struct kobject *kobj, _energy_attribute_t *attr, char *buf);

#endif /* _ENERGY_H_ */
