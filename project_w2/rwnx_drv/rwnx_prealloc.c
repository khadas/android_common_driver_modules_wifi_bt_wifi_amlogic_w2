/**
****************************************************************************************
*
* @file rwnx_prealloc.c
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022).
*
* @brief Preallocing buffer implementation.
*
****************************************************************************************
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#ifdef CONFIG_RWNX_USE_PREALLOC_BUF
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
#include <linux/wlan_plat.h>
#else
#include <linux/amlogic/wlan_plat.h>
#endif
#endif
#include "rwnx_defs.h"
#include "rwnx_prealloc.h"

#ifdef CONFIG_RWNX_USE_PREALLOC_BUF
void *rwnx_prealloc_get(int type, size_t size, size_t *out_size)
{
    void *prealloc_buf = NULL;
    switch (type) {
        case PREALLOC_BUF_TYPE_DUMP:
            if (size > PREALLOC_BUF_DUMP_SIZE) {
                printk("not enough pre-alloc buffer size(%ld) for dump\n", size);
                return NULL;
            }
            prealloc_buf = bcmdhd_mem_prealloc(BCMDHD_MEM_DUMP_RAM, size);
            *out_size = PREALLOC_BUF_DUMP_SIZE;
            break;
        default:
            printk("not support pre-alloc buffer type(%d)\n", type);
            break;
    }
    return prealloc_buf;
}
#endif
