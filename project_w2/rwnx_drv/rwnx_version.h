#ifndef _RWNX_VERSION_H_
#define _RWNX_VERSION_H_

#include "rwnx_version_gen.h"

static inline void rwnx_print_version(void)
{
    printk(RWNX_VERS_BANNER"\n");
    printk(RWNX_DRIVER_COMPILE_INFO"\n");
    printk(FIRMWARE_INFO"\n");
    printk(COMMON_INFO"\n");
}

#endif /* _RWNX_VERSION_H_ */
