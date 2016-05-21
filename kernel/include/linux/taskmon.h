/*
 * CMU 18-648 Embedded Real-Time Systems
 * Team 6
 */

#ifndef _TASKMON_H_
#define _TASKMON_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>


typedef struct _energy_tracking_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _energy_tracking_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _energy_tracking_attribute_s *attr, const char *buf, size_t count);
} _energy_tracking_attribute_t;


typedef struct _freq_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _freq_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _freq_attribute_s *attr, const char *buf, size_t count);
} _freq_attribute_t;


typedef struct _power_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _power_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _power_attribute_s *attr, const char *buf, size_t count);
} _power_attribute_t;


typedef struct _partition_policy_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _partition_policy_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _partition_policy_attribute_s *attr, const char *buf, size_t count);
} _partition_policy_attribute_t;


typedef struct _reserves_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _reserves_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _reserves_attribute_s *attr, const char *buf, size_t count);
} _reserves_attribute_t;


typedef struct _enabled_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _enabled_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _enabled_attribute_s *attr, const char *buf, size_t count);
    int enabled;
} _enabled_attribute_t;


typedef struct _util_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _util_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _util_attribute_s *attr, const char *buf, size_t count);
    pid_t tid;
} _util_attribute_t;


typedef struct _energy_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _energy_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _energy_attribute_s *attr, const char *buf, size_t count);
    pid_t tid;
    struct task_struct *task;
} _energy_attribute_t;


typedef struct _total_energy_attribute_s {
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct _total_energy_attribute_s *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct _total_energy_attribute_s *attr, const char *buf, size_t count);
} _total_energy_attribute_t;


extern _enabled_attribute_t *ptr_taskmon_enabled_attribute;

extern struct kobject *rtes_kobj;
extern struct kobject *taskmon_kobj;
extern struct kobject *util_kobj;
extern struct kobject *tasks_kobj;

extern struct mutex enabled_mutex;
extern int enabled_mutex_init;

/* partition policy for assigning tasks to processors */
extern char* partition_policy;

extern unsigned long long taskon_acc_time;

extern unsigned long total_energy;

#endif  // _TASKMON_H_
