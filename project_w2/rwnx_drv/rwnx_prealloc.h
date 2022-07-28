/**
****************************************************************************************
*
* @file rwnx_prealloc.h
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022).
*
* @brief Declaration of the preallocing buffer.
*
****************************************************************************************
*/

#ifndef _RWNX_PREALLOC_H_
#define _RWNX_PREALLOC_H_

#ifdef CONFIG_RWNX_USE_PREALLOC_BUF
#define PREALLOC_BUF_DUMP_SIZE  (1290 * 1024)

/* inner buffer type */
enum prealloc_buf_type {
    PREALLOC_BUF_TYPE_MSG = 0,
    PREALLOC_BUF_TYPE_DBG,
    PREALLOC_BUF_TYPE_RADAR,
    PREALLOC_BUF_TYPE_TXCFM,
    PREALLOC_BUF_TYPE_TXPATTERN,
    PREALLOC_BUF_TYPE_BEACON,
    PREALLOC_BUF_TYPE_DUMP,

    PREALLOC_BUF_TYPE_MAX,
};
void *rwnx_prealloc_get(int buf_type, size_t buf_size, size_t *alloc_size);

/* outer buffer type */
enum bcmdhd_mem_type {
    BCMDHD_MEM_DUMP_RAM = 11,
};
extern void *bcmdhd_mem_prealloc(int section, unsigned long size);

/* prealloc rxbuf definition and structure */
#define PREALLOC_RXBUF_SIZE     (128 + 32)
#define PREALLOC_RXBUF_FACTOR   (32)

struct rwnx_prealloc_rxbuf {
    struct list_head list;
    struct sk_buff *skb;
};

void rwnx_prealloc_rxbuf_init(struct rwnx_hw *rwnx_hw, uint32_t rxbuf_sz);
void rwnx_prealloc_rxbuf_deinit(struct rwnx_hw *rwnx_hw);
struct rwnx_prealloc_rxbuf *rwnx_prealloc_get_free_rxbuf(struct rwnx_hw *rwnx_hw);
struct rwnx_prealloc_rxbuf *rwnx_prealloc_get_used_rxbuf(struct rwnx_hw *rwnx_hw);

#endif

#endif
