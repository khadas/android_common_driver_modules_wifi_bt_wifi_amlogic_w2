/**
****************************************************************************************
*
* @file aml_prealloc.h
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022).
*
* @brief Declaration of the preallocing buffer.
*
****************************************************************************************
*/

#ifndef _AML_PREALLOC_H_
#define _AML_PREALLOC_H_

#ifdef CONFIG_AML_PREALLOC_BUF_STATIC
#define PREALLOC_BUF_DUMP_SIZE  (1290 * 1024)
#define FW_VERBOSE_RING_SIZE        (256 * 1024)
#define PREALLOC_BUF_INFO_SIZE  (36 * 1024)

/* inner buffer type */
enum prealloc_buf_type {
    PREALLOC_BUF_TYPE_MSG = 0,
    PREALLOC_BUF_TYPE_DBG,
    PREALLOC_BUF_TYPE_RADAR,
    PREALLOC_BUF_TYPE_TXCFM,
    PREALLOC_BUF_TYPE_TXPATTERN,
    PREALLOC_BUF_TYPE_BEACON,
    PREALLOC_BUF_TYPE_DUMP,
    PREALLOC_BUF_TYPE_RX,
    PREALLOC_BUF_TYPE_TXQ,
    PREALLOC_BUF_TYPE_MAX,
};
void *aml_prealloc_get(int buf_type, size_t buf_size, size_t *alloc_size);

/* outer buffer type */
enum bcmdhd_mem_type {
    BCMDHD_MEM_DUMP_RAM = 11,
    DHD_PREALLOC_DHD_INFO = 7,
    DHD_PREALLOC_FW_VERBOSE_RING = 20,
};
extern void *bcmdhd_mem_prealloc(int section, unsigned long size);
#endif  // CONFIG_PREALLOC_BUF_STATIC

#ifdef CONFIG_AML_PREALLOC_BUF_SKB
/* prealloc rxbuf definition and structure */
#define PREALLOC_RXBUF_SIZE     (32 + 32)
#define PREALLOC_RXBUF_FACTOR   (16)

struct aml_prealloc_rxbuf {
    struct list_head list;
    struct sk_buff *skb;
};

void aml_prealloc_rxbuf_init(struct aml_hw *aml_hw, uint32_t rxbuf_sz);
void aml_prealloc_rxbuf_deinit(struct aml_hw *aml_hw);
struct aml_prealloc_rxbuf *aml_prealloc_get_free_rxbuf(struct aml_hw *aml_hw);
struct aml_prealloc_rxbuf *aml_prealloc_get_used_rxbuf(struct aml_hw *aml_hw);

#endif  // CONFIG_PREALLOC_BUF_SKB
#endif  // _AML_PREALLOC_H_
