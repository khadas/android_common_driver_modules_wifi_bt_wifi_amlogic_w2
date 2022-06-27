/**
 ******************************************************************************
 *
 * @file rwnx_rx.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#ifndef _RWNX_RX_H_
#define _RWNX_RX_H_

#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include "hal_desc.h"
#include "ipc_shared.h"

#define STAT_VAL_OFFSET     36
#define RX_STAT_DESC_LEN    48
#define RX_HEADER_OFFSET    28
#define RX_PAYLOAD_OFFSET   144

 struct drv_stat_val
{
    /// Host Buffer Address
    uint32_t host_id;
    /// Length
    uint32_t frame_len;
    /// Status (@ref rx_status_bits)
    uint16_t status;
};

enum rx_status_bits
{
    /// The buffer can be forwarded to the networking stack
    RX_STAT_FORWARD = 1 << 0,
    /// A new buffer has to be allocated
    RX_STAT_ALLOC = 1 << 1,
    /// The buffer has to be deleted
    RX_STAT_DELETE = 1 << 2,
    /// The length of the buffer has to be updated
    RX_STAT_LEN_UPDATE = 1 << 3,
    /// The length in the Ethernet header has to be updated
    RX_STAT_ETH_LEN_UPDATE = 1 << 4,
    /// Simple copy
    RX_STAT_COPY = 1 << 5,
    /// Spurious frame (inform upper layer and discard)
    RX_STAT_SPURIOUS = 1 << 6,
    /// packet for monitor interface
    RX_STAT_MONITOR = 1 << 7,
};

/* Maximum number of rx buffer the fw may use at the same time
   (must be at least IPC_RXBUF_CNT) */
#define RWNX_RXBUFF_MAX ((64 * NX_REMOTE_STA_MAX) < IPC_RXBUF_CNT ?     \
                         IPC_RXBUF_CNT : (64 * NX_REMOTE_STA_MAX))

/**
 * struct rwnx_skb_cb - Control Buffer structure for RX buffer
 *
 * @hostid: Buffer identifier. Written back by fw in RX descriptor to identify
 * the associated rx buffer
 */
struct rwnx_skb_cb {
    uint32_t hostid;
};

#define RWNX_RXBUFF_HOSTID_SET(buf, val)                                \
    ((struct rwnx_skb_cb *)((struct sk_buff *)buf->addr)->cb)->hostid = val

#define RWNX_RXBUFF_HOSTID_GET(buf)                                        \
    ((struct rwnx_skb_cb *)((struct sk_buff *)buf->addr)->cb)->hostid

#define RWNX_RXBUFF_VALID_IDX(idx) ((idx) < RWNX_RXBUFF_MAX)

/* Used to ensure that hostid set to fw is never 0 */
#define RWNX_RXBUFF_IDX_TO_HOSTID(idx) ((idx) + 1)
#define RWNX_RXBUFF_HOSTID_TO_IDX(hostid) ((hostid) - 1)

#define RX_MACHDR_BACKUP_LEN    64

/// MAC header backup descriptor
struct mon_machdrdesc
{
    /// Length of the buffer
    u32 buf_len;
    /// Buffer containing mac header, LLC and SNAP
    u8 buffer[RX_MACHDR_BACKUP_LEN];
};

struct hw_rxhdr {
    /** RX vector */
    struct hw_vect hwvect;

    /** PHY channel information */
    struct phy_channel_info_desc phy_info;

    /** RX flags */
    u32    flags_is_amsdu     : 1;
    u32    flags_is_80211_mpdu: 1;
    u32    flags_is_4addr     : 1;
    u32    flags_new_peer     : 1;
    u32    flags_user_prio    : 3;
    u32    flags_rsvd0        : 1;
    u32    flags_vif_idx      : 8;    // 0xFF if invalid VIF index
    u32    flags_sta_idx      : 8;    // 0xFF if invalid STA index
    u32    flags_dst_idx      : 8;    // 0xFF if unknown destination STA
#ifdef CONFIG_RWNX_MON_DATA
    /// MAC header backup descriptor (used only for MSDU when there is a monitor and a data interface)
    struct mon_machdrdesc mac_hdr_backup;
#endif
    /** Pattern indicating if the buffer is available for the driver */
    u32    pattern;
};

/**
 * struct rwnx_defer_rx - Defer rx buffer processing
 *
 * @skb: List of deferred buffers
 * @work: work to defer processing of this buffer
 */
struct rwnx_defer_rx {
    struct sk_buff_head sk_list;
    struct work_struct work;
};

/**
 * struct rwnx_defer_rx_cb - Control buffer for deferred buffers
 *
 * @vif: VIF that received the buffer
 */
struct rwnx_defer_rx_cb {
    struct rwnx_vif *vif;
};

struct rxdata {
    struct list_head list;
    unsigned int host_id;
    unsigned int frame_len;
    struct sk_buff *skb;
};

u8 rwnx_unsup_rx_vec_ind(void *pthis, void *hostid);
u8 rwnx_rxdataind(void *pthis, void *hostid);
void rwnx_rx_deferred(struct work_struct *ws);
void rwnx_rx_defer_skb(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                       struct sk_buff *skb);
void rwnx_rxdata_init(void);
void rwnx_rxdata_deinit(void);

#endif /* _RWNX_RX_H_ */
