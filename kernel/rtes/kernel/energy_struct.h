/*
 * 18-648
 * Team 6
 *
 * header for energy struct
 */

#ifndef _ENERGY_STRUCT_H_
#define _ENERGY_STRUCT_H_

struct energy_struct {
    unsigned int freq; /* MHz */
    unsigned int power; /* mW */
    unsigned long energy; /* mJ */
};

#endif /* _ENERGY_STRUCT_H_ */
