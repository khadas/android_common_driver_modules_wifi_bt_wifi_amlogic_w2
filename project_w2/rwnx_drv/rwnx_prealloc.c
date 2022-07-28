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

void rwnx_prealloc_rxbuf_init(struct rwnx_hw *rwnx_hw, uint32_t rxbuf_sz)
{
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf = NULL;
    int i = 0;

    rwnx_hw->prealloc_rxbuf_count = 0;
    spin_lock_init(&rwnx_hw->prealloc_rxbuf_lock);
    INIT_LIST_HEAD(&rwnx_hw->prealloc_rxbuf_free);
    INIT_LIST_HEAD(&rwnx_hw->prealloc_rxbuf_used);

    for (i = 0; i < PREALLOC_RXBUF_SIZE; i++) {
        prealloc_rxbuf = kmalloc(sizeof(struct rwnx_prealloc_rxbuf), GFP_ATOMIC);
        if (!prealloc_rxbuf) {
            ASSERT_ERR(0);
            return;
        }
        prealloc_rxbuf->skb = dev_alloc_skb(rxbuf_sz);
        if (prealloc_rxbuf->skb) {
            list_add_tail(&prealloc_rxbuf->list, &rwnx_hw->prealloc_rxbuf_free);
        } else {
            kfree(prealloc_rxbuf);
            i--;
        }
    }
}

void rwnx_prealloc_rxbuf_deinit(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf = NULL;
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf_tmp = NULL;

    spin_lock_bh(&rwnx_hw->prealloc_rxbuf_lock);
    list_for_each_entry_safe(prealloc_rxbuf,
            prealloc_rxbuf_tmp, &rwnx_hw->prealloc_rxbuf_free, list) {
        if (prealloc_rxbuf->skb) {
            dev_kfree_skb(prealloc_rxbuf->skb);
        }
        kfree(prealloc_rxbuf);
    }
    spin_unlock_bh(&rwnx_hw->prealloc_rxbuf_lock);
}

struct rwnx_prealloc_rxbuf *rwnx_prealloc_get_free_rxbuf(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf = NULL;

    spin_lock_bh(&rwnx_hw->prealloc_rxbuf_lock);
    if (!list_empty(&rwnx_hw->prealloc_rxbuf_free)) {
        prealloc_rxbuf = list_first_entry(&rwnx_hw->prealloc_rxbuf_free,
                struct rwnx_prealloc_rxbuf, list);
        list_del(&prealloc_rxbuf->list);
        list_add_tail(&prealloc_rxbuf->list, &rwnx_hw->prealloc_rxbuf_used);
        RWNX_INFO("prealloc: get free skb=%p", prealloc_rxbuf->skb);
    }
    spin_unlock_bh(&rwnx_hw->prealloc_rxbuf_lock);
    return prealloc_rxbuf;
}

struct rwnx_prealloc_rxbuf *rwnx_prealloc_get_used_rxbuf(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf = NULL;
    struct rwnx_prealloc_rxbuf *prealloc_rxbuf_tmp = NULL;

    list_for_each_entry_safe(prealloc_rxbuf,
            prealloc_rxbuf_tmp, &rwnx_hw->prealloc_rxbuf_used, list) {
        spin_lock_bh(&rwnx_hw->prealloc_rxbuf_lock);
        list_del(&prealloc_rxbuf->list);
        list_add_tail(&prealloc_rxbuf->list, &rwnx_hw->prealloc_rxbuf_free);
        spin_unlock_bh(&rwnx_hw->prealloc_rxbuf_lock);
        RWNX_INFO("prealloc: get used skb=%p", prealloc_rxbuf->skb);
        return prealloc_rxbuf;
    }
    return NULL;
}

#endif
