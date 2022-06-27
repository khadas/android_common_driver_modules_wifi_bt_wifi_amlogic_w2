/**
 ******************************************************************************
 *
 * @file rwnx_tx.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <uapi/linux/sched/types.h>

#include "rwnx_defs.h"
#include "rwnx_tx.h"
#include "rwnx_msg_tx.h"
#include "rwnx_mesh.h"
#include "rwnx_events.h"
#include "rwnx_compat.h"
#include "share_mem_map.h"

/******************************************************************************
 * Power Save functions
 *****************************************************************************/
/**
 * rwnx_set_traffic_status - Inform FW if traffic is available for STA in PS
 *
 * @rwnx_hw: Driver main data
 * @sta: Sta in PS mode
 * @available: whether traffic is buffered for the STA
 * @ps_id: type of PS data requested (@LEGACY_PS_ID or @UAPSD_ID)
  */
void rwnx_set_traffic_status(struct rwnx_hw *rwnx_hw,
                             struct rwnx_sta *sta,
                             bool available,
                             u8 ps_id)
{
    if (sta->tdls.active) {
        rwnx_send_tdls_peer_traffic_ind_req(rwnx_hw,
                                            rwnx_hw->vif_table[sta->vif_idx]);
    } else {
        bool uapsd = (ps_id != LEGACY_PS_ID);
        rwnx_send_me_traffic_ind(rwnx_hw, sta->sta_idx, uapsd, available);
        trace_ps_traffic_update(sta->sta_idx, available, uapsd);
    }
}

/**
 * rwnx_ps_bh_enable - Enable/disable PS mode for one STA
 *
 * @rwnx_hw: Driver main data
 * @sta: Sta which enters/leaves PS mode
 * @enable: PS mode status
 *
 * This function will enable/disable PS mode for one STA.
 * When enabling PS mode:
 *  - Stop all STA's txq for RWNX_TXQ_STOP_STA_PS reason
 *  - Count how many buffers are already ready for this STA
 *  - For BC/MC sta, update all queued SKB to use hw_queue BCMC
 *  - Update TIM if some packet are ready
 *
 * When disabling PS mode:
 *  - Start all STA's txq for RWNX_TXQ_STOP_STA_PS reason
 *  - For BC/MC sta, update all queued SKB to use hw_queue AC_BE
 *  - Update TIM if some packet are ready (otherwise fw will not update TIM
 *    in beacon for this STA)
 *
 * All counter/skb updates are protected from TX path by taking tx_lock
 *
 * NOTE: _bh_ in function name indicates that this function is called
 * from a bottom_half tasklet.
 */
void rwnx_ps_bh_enable(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta,
                       bool enable)
{
    struct rwnx_txq *txq;

    if (enable) {
        trace_ps_enable(sta);

        spin_lock(&rwnx_hw->tx_lock);
        sta->ps.active = true;
        sta->ps.sp_cnt[LEGACY_PS_ID] = 0;
        sta->ps.sp_cnt[UAPSD_ID] = 0;
        rwnx_txq_sta_stop(sta, RWNX_TXQ_STOP_STA_PS, rwnx_hw);

        if (is_multicast_sta(sta->sta_idx)) {
            txq = rwnx_txq_sta_get(sta, 0, rwnx_hw);
            sta->ps.pkt_ready[LEGACY_PS_ID] = skb_queue_len(&txq->sk_list);
            sta->ps.pkt_ready[UAPSD_ID] = 0;
            txq->hwq = &rwnx_hw->hwq[RWNX_HWQ_BE];
        } else {
            int i;
            sta->ps.pkt_ready[LEGACY_PS_ID] = 0;
            sta->ps.pkt_ready[UAPSD_ID] = 0;
            foreach_sta_txq(sta, txq, i, rwnx_hw) {
                sta->ps.pkt_ready[txq->ps_id] += skb_queue_len(&txq->sk_list);
            }
        }

        spin_unlock(&rwnx_hw->tx_lock);

        if (sta->ps.pkt_ready[LEGACY_PS_ID])
            rwnx_set_traffic_status(rwnx_hw, sta, true, LEGACY_PS_ID);

        if (sta->ps.pkt_ready[UAPSD_ID])
            rwnx_set_traffic_status(rwnx_hw, sta, true, UAPSD_ID);
    } else {
        trace_ps_disable(sta->sta_idx);

        spin_lock(&rwnx_hw->tx_lock);
        sta->ps.active = false;

        if (is_multicast_sta(sta->sta_idx)) {
            txq = rwnx_txq_sta_get(sta, 0, rwnx_hw);
            txq->hwq = &rwnx_hw->hwq[RWNX_HWQ_BE];
            txq->push_limit = 0;
        } else {
            int i;
            foreach_sta_txq(sta, txq, i, rwnx_hw) {
                txq->push_limit = 0;
            }
        }

        rwnx_txq_sta_start(sta, RWNX_TXQ_STOP_STA_PS, rwnx_hw);
        spin_unlock(&rwnx_hw->tx_lock);

        if (sta->ps.pkt_ready[LEGACY_PS_ID])
            rwnx_set_traffic_status(rwnx_hw, sta, false, LEGACY_PS_ID);

        if (sta->ps.pkt_ready[UAPSD_ID])
            rwnx_set_traffic_status(rwnx_hw, sta, false, UAPSD_ID);
    }
}

/**
 * rwnx_ps_bh_traffic_req - Handle traffic request for STA in PS mode
 *
 * @rwnx_hw: Driver main data
 * @sta: Sta which enters/leaves PS mode
 * @pkt_req: number of pkt to push
 * @ps_id: type of PS data requested (@LEGACY_PS_ID or @UAPSD_ID)
 *
 * This function will make sure that @pkt_req are pushed to fw
 * whereas the STA is in PS mode.
 * If request is 0, send all traffic
 * If request is greater than available pkt, reduce request
 * Note: request will also be reduce if txq credits are not available
 *
 * All counter updates are protected from TX path by taking tx_lock
 *
 * NOTE: _bh_ in function name indicates that this function is called
 * from the bottom_half tasklet.
 */
void rwnx_ps_bh_traffic_req(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta,
                            u16 pkt_req, u8 ps_id)
{
    int pkt_ready_all;
    struct rwnx_txq *txq;

    if (WARN(!sta->ps.active, "sta %pM is not in Power Save mode",
             sta->mac_addr))
        return;

    trace_ps_traffic_req(sta, pkt_req, ps_id);

    spin_lock(&rwnx_hw->tx_lock);

    /* Fw may ask to stop a service period with PS_SP_INTERRUPTED. This only
       happens for p2p-go interface if NOA starts during a service period */
    if ((pkt_req == PS_SP_INTERRUPTED) && (ps_id == UAPSD_ID)) {
        int tid;
        sta->ps.sp_cnt[ps_id] = 0;
        foreach_sta_txq(sta, txq, tid, rwnx_hw) {
            txq->push_limit = 0;
        }
        goto done;
    }

    pkt_ready_all = (sta->ps.pkt_ready[ps_id] - sta->ps.sp_cnt[ps_id]);

    /* Don't start SP until previous one is finished or we don't have
       packet ready (which must not happen for U-APSD) */
    if (sta->ps.sp_cnt[ps_id] || pkt_ready_all <= 0) {
        goto done;
    }

    /* Adapt request to what is available. */
    if (pkt_req == 0 || pkt_req > pkt_ready_all) {
        pkt_req = pkt_ready_all;
    }

    /* Reset the SP counter */
    sta->ps.sp_cnt[ps_id] = 0;

    /* "dispatch" the request between txq */
    if (is_multicast_sta(sta->sta_idx)) {
        txq = rwnx_txq_sta_get(sta, 0, rwnx_hw);
        if (txq->credits <= 0)
            goto done;
        if (pkt_req > txq->credits)
            pkt_req = txq->credits;
        txq->push_limit = pkt_req;
        sta->ps.sp_cnt[ps_id] = pkt_req;
        rwnx_txq_add_to_hw_list(txq);
    } else {
        int i, tid;

        foreach_sta_txq_prio(sta, txq, tid, i, rwnx_hw) {
            u16 txq_len = skb_queue_len(&txq->sk_list);

            if (txq->ps_id != ps_id)
                continue;

            if (txq_len > txq->credits)
                txq_len = txq->credits;

            if (txq_len == 0)
                continue;

            if (txq_len < pkt_req) {
                /* Not enough pkt queued in this txq, add this
                   txq to hwq list and process next txq */
                pkt_req -= txq_len;
                txq->push_limit = txq_len;
                sta->ps.sp_cnt[ps_id] += txq_len;
                rwnx_txq_add_to_hw_list(txq);
            } else {
                /* Enough pkt in this txq to comlete the request
                   add this txq to hwq list and stop processing txq */
                txq->push_limit = pkt_req;
                sta->ps.sp_cnt[ps_id] += pkt_req;
                rwnx_txq_add_to_hw_list(txq);
                break;
            }
        }
    }

  done:
    spin_unlock(&rwnx_hw->tx_lock);
}

/******************************************************************************
 * TX functions
 *****************************************************************************/
#define PRIO_STA_NULL 0xAA

static const int rwnx_down_hwq2tid[3] = {
    [RWNX_HWQ_BK] = 2,
    [RWNX_HWQ_BE] = 3,
    [RWNX_HWQ_VI] = 5,
};

static void rwnx_downgrade_ac(struct rwnx_sta *sta, struct sk_buff *skb)
{
    int8_t ac = rwnx_tid2hwq[skb->priority];

    if (WARN((ac > RWNX_HWQ_VO),
             "Unexepcted ac %d for skb before downgrade", ac))
        ac = RWNX_HWQ_VO;

    while (sta->acm & BIT(ac)) {
        if (ac == RWNX_HWQ_BK) {
            skb->priority = 1;
            return;
        }
        ac--;
        skb->priority = rwnx_down_hwq2tid[ac];
    }
}

static void rwnx_tx_statistic(struct rwnx_vif *vif, struct rwnx_txq *txq,
                              union rwnx_hw_txstatus status, unsigned int data_len)
{
    struct rwnx_sta *sta = txq->sta;

    if (!status.acknowledged) {
        if (sta)
            sta->stats.tx_fails++;
        return;
    }
    vif->net_stats.tx_packets++;
    vif->net_stats.tx_bytes += data_len;

    if (!sta)
        return;

    sta->stats.tx_pkts++;
    sta->stats.tx_bytes += data_len;
    sta->stats.last_act = jiffies;
}

u16 rwnx_select_txq(struct rwnx_vif *rwnx_vif, struct sk_buff *skb)
{
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct wireless_dev *wdev = &rwnx_vif->wdev;
    struct rwnx_sta *sta = NULL;
    struct rwnx_txq *txq;
    u16 netdev_queue;
    bool tdls_mgmgt_frame = false;

    switch (wdev->iftype) {
    case NL80211_IFTYPE_STATION:
    case NL80211_IFTYPE_P2P_CLIENT:
    {
        struct ethhdr *eth;
        eth = (struct ethhdr *)skb->data;
        if (eth->h_proto == cpu_to_be16(ETH_P_TDLS)) {
            tdls_mgmgt_frame = true;
        }
        if ((rwnx_vif->tdls_status == TDLS_LINK_ACTIVE) &&
            (rwnx_vif->sta.tdls_sta != NULL) &&
            (memcmp(eth->h_dest, rwnx_vif->sta.tdls_sta->mac_addr, ETH_ALEN) == 0))
            sta = rwnx_vif->sta.tdls_sta;
        else
            sta = rwnx_vif->sta.ap;
        break;
    }
    case NL80211_IFTYPE_AP_VLAN:
        if (rwnx_vif->ap_vlan.sta_4a) {
            sta = rwnx_vif->ap_vlan.sta_4a;
            break;
        }

        /* AP_VLAN interface is not used for a 4A STA,
           fallback searching sta amongs all AP's clients */
        rwnx_vif = rwnx_vif->ap_vlan.master;
    case NL80211_IFTYPE_AP:
    case NL80211_IFTYPE_P2P_GO:
    {
        struct rwnx_sta *cur;
        struct ethhdr *eth = (struct ethhdr *)skb->data;

        if (is_multicast_ether_addr(eth->h_dest)) {
            sta = &rwnx_hw->sta_table[rwnx_vif->ap.bcmc_index];
        } else {
            list_for_each_entry(cur, &rwnx_vif->ap.sta_list, list) {
                if (!memcmp(cur->mac_addr, eth->h_dest, ETH_ALEN)) {
                    sta = cur;
                    break;
                }
            }
        }

        break;
    }
    case NL80211_IFTYPE_MESH_POINT:
    {
        struct ethhdr *eth = (struct ethhdr *)skb->data;

        if (!rwnx_vif->is_resending) {
            /*
             * If ethernet source address is not the address of a mesh wireless interface, we are proxy for
             * this address and have to inform the HW
             */
            if (memcmp(&eth->h_source[0], &rwnx_vif->ndev->perm_addr[0], ETH_ALEN)) {
                /* Check if LMAC is already informed */
                if (!rwnx_get_mesh_proxy_info(rwnx_vif, (u8 *)&eth->h_source, true)) {
                    rwnx_send_mesh_proxy_add_req(rwnx_hw, rwnx_vif, (u8 *)&eth->h_source);
                }
            }
        }

        if (is_multicast_ether_addr(eth->h_dest)) {
            sta = &rwnx_hw->sta_table[rwnx_vif->ap.bcmc_index];
        } else {
            /* Path to be used */
            struct rwnx_mesh_path *p_mesh_path = NULL;
            struct rwnx_mesh_path *p_cur_path;
            /* Check if destination is proxied by a peer Mesh STA */
            struct rwnx_mesh_proxy *p_mesh_proxy = rwnx_get_mesh_proxy_info(rwnx_vif, (u8 *)&eth->h_dest, false);
            /* Mesh Target address */
            struct mac_addr *p_tgt_mac_addr;

            if (p_mesh_proxy) {
                p_tgt_mac_addr = &p_mesh_proxy->proxy_addr;
            } else {
                p_tgt_mac_addr = (struct mac_addr *)&eth->h_dest;
            }

            /* Look for path with provided target address */
            list_for_each_entry(p_cur_path, &rwnx_vif->ap.mpath_list, list) {
                if (!memcmp(&p_cur_path->tgt_mac_addr, p_tgt_mac_addr, ETH_ALEN)) {
                    p_mesh_path = p_cur_path;
                    break;
                }
            }

            if (p_mesh_path) {
                sta = p_mesh_path->nhop_sta;
            } else {
                rwnx_send_mesh_path_create_req(rwnx_hw, rwnx_vif, (u8 *)p_tgt_mac_addr);
            }
        }

        break;
    }
    default:
        break;
    }

    if (sta && sta->qos)
    {
        if (tdls_mgmgt_frame) {
            skb_set_queue_mapping(skb, NX_STA_NDEV_IDX(skb->priority, sta->sta_idx));
        } else {
            /* use the data classifier to determine what 802.1d tag the
             * data frame has */
            skb->priority = cfg80211_classify8021d(skb, NULL) & IEEE80211_QOS_CTL_TAG1D_MASK;
        }
        if (sta->acm)
            rwnx_downgrade_ac(sta, skb);

        txq = rwnx_txq_sta_get(sta, skb->priority, rwnx_hw);
        netdev_queue = txq->ndev_idx;
    }
    else if (sta)
    {
        skb->priority = 0xFF;
        txq = rwnx_txq_sta_get(sta, 0, rwnx_hw);
        netdev_queue = txq->ndev_idx;
    }
    else
    {
        /* This packet will be dropped in xmit function, still need to select
           an active queue for xmit to be called. As it most likely to happen
           for AP interface, select BCMC queue
           (TODO: select another queue if BCMC queue is stopped) */
        skb->priority = PRIO_STA_NULL;
        netdev_queue = NX_BCMC_TXQ_NDEV_IDX;
    }

    BUG_ON(netdev_queue >= NX_NB_NDEV_TXQ);

    return netdev_queue;
}

/**
 * rwnx_set_more_data_flag - Update MORE_DATA flag in tx sw desc
 *
 * @rwnx_hw: Driver main data
 * @sw_txhdr: Header for pkt to be pushed
 *
 * If STA is in PS mode
 *  - Set EOSP in case the packet is the last of the UAPSD service period
 *  - Set MORE_DATA flag if more pkt are ready for this sta
 *  - Update TIM if this is the last pkt buffered for this sta
 *
 * note: tx_lock already taken.
 */
static inline void rwnx_set_more_data_flag(struct rwnx_hw *rwnx_hw,
                                           struct rwnx_sw_txhdr *sw_txhdr)
{
    struct rwnx_sta *sta = sw_txhdr->rwnx_sta;
    struct rwnx_vif *vif = sw_txhdr->rwnx_vif;
    struct rwnx_txq *txq = sw_txhdr->txq;

    if (unlikely(sta->ps.active)) {
        sta->ps.pkt_ready[txq->ps_id]--;
        sta->ps.sp_cnt[txq->ps_id]--;

        trace_ps_push(sta);

        if (((txq->ps_id == UAPSD_ID) || (vif->wdev.iftype == NL80211_IFTYPE_MESH_POINT) || (sta->tdls.active))
                && !sta->ps.sp_cnt[txq->ps_id]) {
            sw_txhdr->desc.api.host.flags |= TXU_CNTRL_EOSP;
        }

        if (sta->ps.pkt_ready[txq->ps_id]) {
            sw_txhdr->desc.api.host.flags |= TXU_CNTRL_MORE_DATA;
        } else {
            rwnx_set_traffic_status(rwnx_hw, sta, false, txq->ps_id);
        }
    }
}

/**
 * rwnx_get_tx_info - Get STA and tid for one skb
 *
 * @rwnx_vif: vif ptr
 * @skb: skb
 * @tid: pointer updated with the tid to use for this skb
 *
 * @return: pointer on the destination STA (may be NULL)
 *
 * skb has already been parsed in rwnx_select_queue function
 * simply re-read information form skb.
 */
static struct rwnx_sta *rwnx_get_tx_info(struct rwnx_vif *rwnx_vif,
                                         struct sk_buff *skb,
                                         u8 *tid)
{
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_sta *sta;
    int sta_idx;

    *tid = skb->priority;
    if (unlikely(skb->priority == PRIO_STA_NULL)) {
        return NULL;
    } else {
        int ndev_idx = skb_get_queue_mapping(skb);

        if (ndev_idx == NX_BCMC_TXQ_NDEV_IDX)
            sta_idx = NX_REMOTE_STA_MAX + master_vif_idx(rwnx_vif);
        else
            sta_idx = ndev_idx / NX_NB_TID_PER_STA;

        sta = &rwnx_hw->sta_table[sta_idx];
    }

    return sta;
}

/**
 * rwnx_prep_dma_tx - Prepare buffer for DMA transmission
 *
 * @rwnx_hw: Driver main data
 * @sw_txhdr: Software Tx descriptor
 * @frame_start: Pointer to the beginning of the frame that needs to be DMA mapped
 * @return: 0 on success, -1 on error
 *
 * Map the frame for DMA transmission and save its ipc address in the tx descriptor
 */
#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
static int rwnx_prep_dma_tx(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                             void *frame_start)
{
    struct txdesc_api *desc = &sw_txhdr->desc.api;

    //if (rwnx_ipc_buf_a2e_init(rwnx_hw, &sw_txhdr->ipc_data, frame_start,
    //                          sw_txhdr->frame_len))
    //    return -1;

    /* Update DMA addresses and length in tx descriptor */
    desc->host.packet_len[0] = sw_txhdr->frame_len;
    //desc->host.packet_addr[0] = sw_txhdr->ipc_data.dma_addr;
    desc->host.packet_addr[0] = 1;
#ifdef CONFIG_RWNX_SPLIT_TX_BUF
    desc->host.packet_cnt = 1;
#endif

    return 0;
}

#else
static int rwnx_prep_dma_tx(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                            void *frame_start)
{
    struct txdesc_api *desc = &sw_txhdr->desc.api;

    if (rwnx_ipc_buf_a2e_init(rwnx_hw, &sw_txhdr->ipc_data, frame_start,
                              sw_txhdr->frame_len))
        return -1;

    /* Update DMA addresses and length in tx descriptor */
    desc->host.packet_len[0] = sw_txhdr->frame_len;
    desc->host.packet_addr[0] = sw_txhdr->ipc_data.dma_addr;
#ifdef CONFIG_RWNX_SPLIT_TX_BUF
    desc->host.packet_cnt = 1;
#endif

    return 0;
}
#endif
/**
 *  rwnx_tx_push - Push one packet to fw
 *
 * @rwnx_hw: Driver main data
 * @txhdr: tx desc of the buffer to push
 * @flags: push flags (see @rwnx_push_flags)
 *
 * Push one packet to fw. Sw desc of the packet has already been updated.
 * Only MORE_DATA flag will be set if needed.
 */
void rwnx_tx_push(struct rwnx_hw *rwnx_hw, struct rwnx_txhdr *txhdr, int flags)
{
    struct rwnx_sw_txhdr *sw_txhdr = txhdr->sw_hdr;
    struct sk_buff *skb = sw_txhdr->skb;
    struct rwnx_txq *txq = sw_txhdr->txq;
    u16 hw_queue = txq->hwq->id;
    int user = 0;

    lockdep_assert_held(&rwnx_hw->tx_lock);

    /* RETRY flag is not always set so retest here */
    if (txq->nb_retry) {
        flags |= RWNX_PUSH_RETRY;
        txq->nb_retry--;
        if (txq->nb_retry == 0) {
            WARN(skb != txq->last_retry_skb,
                 "last retry buffer is not the expected one");
            txq->last_retry_skb = NULL;
        }
    } else if (!(flags & RWNX_PUSH_RETRY)) {
        txq->pkt_sent++;
    }

#ifdef CONFIG_RWNX_AMSDUS_TX
    if (txq->amsdu == sw_txhdr) {
        WARN((flags & RWNX_PUSH_RETRY), "End A-MSDU on a retry");
        rwnx_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].done++;
        txq->amsdu = NULL;
    } else if (!(flags & RWNX_PUSH_RETRY) &&
               !(sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU)) {
        rwnx_hw->stats.amsdus[0].done++;
    }
#endif /* CONFIG_RWNX_AMSDUS_TX */

    /* Wait here to update hw_queue, as for multicast STA hwq may change
       between queue and push (because of PS) */
    sw_txhdr->hw_queue = hw_queue;

#ifdef CONFIG_RWNX_MUMIMO_TX
    /* MU group is only selected during hwq processing */
    sw_txhdr->desc.api.host.mumimo_info = txq->mumimo_info;
    user = RWNX_TXQ_POS_ID(txq);
#endif /* CONFIG_RWNX_MUMIMO_TX */

    if (sw_txhdr->rwnx_sta) {
        /* only for AP mode */
        rwnx_set_more_data_flag(rwnx_hw, sw_txhdr);
    }

    trace_push_desc(skb, sw_txhdr, flags);
    txq->credits--;
    txq->pkt_pushed[user]++;
    if (txq->credits <= 0)
        rwnx_txq_stop(txq, RWNX_TXQ_STOP_FULL);

    if (txq->push_limit)
        txq->push_limit--;

    rwnx_ipc_txdesc_push(rwnx_hw, sw_txhdr, skb, hw_queue);
    txq->hwq->credits[user]--;
    rwnx_hw->stats.cfm_balance[hw_queue]++;
}



/**
 * rwnx_tx_retry - Re-queue a pkt that has been postponed by firmware
 *
 * @rwnx_hw: Driver main data
 * @skb: pkt to re-push
 * @sw_txhdr: software TX desc of the pkt to re-push
 * @status: Status on the transmission
 *
 * Called when a packet needs to be repushed to the firmware, because firmware
 * wasn't able to process it when first pushed (e.g. the station enter PS after
 * the driver first pushed this packet to the firmware).
 */
static void rwnx_tx_retry(struct rwnx_hw *rwnx_hw, struct sk_buff *skb,
                          struct rwnx_sw_txhdr *sw_txhdr,
                          union rwnx_hw_txstatus status)
{
    struct rwnx_txq *txq = sw_txhdr->txq;

    /* MORE_DATA will be re-set if needed when pkt will be repushed */
    sw_txhdr->desc.api.host.flags &= ~TXU_CNTRL_MORE_DATA;

    if (status.retry_required) {
        // Firmware already tried to send the buffer but cannot retry it now
        // On next push, firmware needs to re-use the same SN
        sw_txhdr->desc.api.host.flags |= TXU_CNTRL_REUSE_SN;
        sw_txhdr->desc.api.host.sn_for_retry = status.sn;
    }

    txq->credits++;
    trace_skb_retry(skb, txq, (status.retry_required) ? status.sn : 4096);
    if (txq->credits > 0)
        rwnx_txq_start(txq, RWNX_TXQ_STOP_FULL);

    /* Queue the buffer */
    rwnx_txq_queue_skb(skb, txq, rwnx_hw, true, NULL);
}


#ifdef CONFIG_RWNX_AMSDUS_TX
/**
 * rwnx_amsdu_subframe_length() - return size of A-MSDU subframe including
 * header but without padding
 *
 * @eth: Ethernet Header of the frame
 * @frame_len: Length of the ethernet frame (including ethernet header)
 * @return: length of the A-MSDU subframe
 */
static inline int rwnx_amsdu_subframe_length(struct ethhdr *eth, int frame_len)
{
    /* ethernet header is replaced with amdsu header that have the same size
       Only need to check if LLC/SNAP header will be added */
    int len = frame_len;

    if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
        len += sizeof(rfc1042_header) + 2;
    }

    return len;
}

static inline bool rwnx_amsdu_is_aggregable(struct sk_buff *skb)
{
    /* need to add some check on buffer to see if it can be aggregated ? */
    return true;
}

/**
 * rwnx_amsdu_del_subframe_header - remove AMSDU header
 *
 * @amsdu_txhdr: amsdu tx descriptor
 *
 * Move back the ethernet header at the "beginning" of the data buffer.
 * (which has been moved in @rwnx_amsdu_add_subframe_header)
 */
static void rwnx_amsdu_del_subframe_header(struct rwnx_amsdu_txhdr *amsdu_txhdr)
{
    struct sk_buff *skb = amsdu_txhdr->skb;
    struct ethhdr *eth;
    u8 *pos;

    BUG_ON(skb == NULL);
    pos = skb->data;
    pos += sizeof(struct rwnx_amsdu_txhdr);
    eth = (struct ethhdr*)pos;
    pos += amsdu_txhdr->pad + sizeof(struct ethhdr);

    if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
        pos += sizeof(rfc1042_header) + 2;
    }

    memmove(pos, eth, sizeof(*eth));
    skb_pull(skb, (pos - skb->data));
}

/**
 * rwnx_amsdu_add_subframe_header - Add AMSDU header and link subframe
 *
 * @rwnx_hw Driver main data
 * @skb Buffer to aggregate
 * @sw_txhdr Tx descriptor for the first A-MSDU subframe
 *
 * return 0 on sucess, -1 otherwise
 *
 * This functions Add A-MSDU header and LLC/SNAP header in the buffer
 * and update sw_txhdr of the first subframe to link this buffer.
 * If an error happens, the buffer will be queued as a normal buffer.
 *
 *
 *            Before           After
 *         +-------------+  +-------------+
 *         | HEADROOM    |  | HEADROOM    |
 *         |             |  +-------------+ <- data
 *         |             |  | amsdu_txhdr |
 *         |             |  | * pad size  |
 *         |             |  +-------------+
 *         |             |  | ETH hdr     | keep original eth hdr
 *         |             |  |             | to restore it once transmitted
 *         |             |  +-------------+ <- packet_addr[x]
 *         |             |  | Pad         |
 *         |             |  +-------------+
 * data -> +-------------+  | AMSDU HDR   |
 *         | ETH hdr     |  +-------------+
 *         |             |  | LLC/SNAP    |
 *         +-------------+  +-------------+
 *         | DATA        |  | DATA        |
 *         |             |  |             |
 *         +-------------+  +-------------+
 *
 * Called with tx_lock hold
 */
static int rwnx_amsdu_add_subframe_header(struct rwnx_hw *rwnx_hw,
                                          struct sk_buff *skb,
                                          struct rwnx_sw_txhdr *sw_txhdr)
{
    struct rwnx_amsdu *amsdu = &sw_txhdr->amsdu;
    struct rwnx_amsdu_txhdr *amsdu_txhdr;
    struct ethhdr *amsdu_hdr, *eth = (struct ethhdr *)skb->data;
    int headroom_need, msdu_len, amsdu_len;
    u8 *pos, *amsdu_start;

    msdu_len = skb->len - sizeof(*eth);
    headroom_need = sizeof(*amsdu_txhdr) + amsdu->pad +
        sizeof(*amsdu_hdr);
    if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
        headroom_need += sizeof(rfc1042_header) + 2;
        msdu_len += sizeof(rfc1042_header) + 2;
    }
    amsdu_len = msdu_len + sizeof(*amsdu_hdr) + amsdu->pad;

    /* we should have enough headroom (checked in xmit) */
    if (WARN_ON(skb_headroom(skb) < headroom_need)) {
        return -1;
    }

    /* allocate headroom */
    pos = skb_push(skb, headroom_need);
    amsdu_txhdr = (struct rwnx_amsdu_txhdr *)pos;
    pos += sizeof(*amsdu_txhdr);

    /* move eth header */
    memmove(pos, eth, sizeof(*eth));
    eth = (struct ethhdr *)pos;
    pos += sizeof(*eth);

    /* Add padding from previous subframe */
    amsdu_start = pos;
    memset(pos, 0, amsdu->pad);
    pos += amsdu->pad;

    /* Add AMSDU hdr */
    amsdu_hdr = (struct ethhdr *)pos;
    memcpy(amsdu_hdr->h_dest, eth->h_dest, ETH_ALEN);
    memcpy(amsdu_hdr->h_source, eth->h_source, ETH_ALEN);
    amsdu_hdr->h_proto = htons(msdu_len);
    pos += sizeof(*amsdu_hdr);

    if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
        memcpy(pos, rfc1042_header, sizeof(rfc1042_header));
        pos += sizeof(rfc1042_header) + 2;
        // +2 is for protocol ID which is already here (i.e. just before the data)
    }

    /* Prepare IPC buffer for DMA transfer */
    if (rwnx_ipc_buf_a2e_init(rwnx_hw, &amsdu_txhdr->ipc_data, amsdu_start, amsdu_len)) {
        netdev_err(skb->dev, "Failed to add A-MSDU header\n");
        pos -= sizeof(*eth);
        memmove(pos, eth, sizeof(*eth));
        skb_pull(skb, headroom_need);
        return -1;
    }

    /* update amdsu_txhdr */
    amsdu_txhdr->skb = skb;
    amsdu_txhdr->pad = amsdu->pad;
    amsdu_txhdr->msdu_len = msdu_len;

    /* update rwnx_sw_txhdr (of the first subframe) */
    BUG_ON(amsdu->nb != sw_txhdr->desc.api.host.packet_cnt);
    sw_txhdr->desc.api.host.packet_addr[amsdu->nb] = amsdu_txhdr->ipc_data.dma_addr;
    sw_txhdr->desc.api.host.packet_len[amsdu->nb] = amsdu_len;
    sw_txhdr->desc.api.host.packet_cnt++;
    amsdu->nb++;

    amsdu->pad = AMSDU_PADDING(amsdu_len - amsdu->pad);
    list_add_tail(&amsdu_txhdr->list, &amsdu->hdrs);
    amsdu->len += amsdu_len;

#if (defined(CONFIG_RWNX_PCIE_MODE))
    rwnx_ipc_sta_buffer(rwnx_hw, sw_txhdr->txq->sta,
                        sw_txhdr->txq->tid, msdu_len);
#endif

    trace_amsdu_subframe(sw_txhdr);
    return 0;
}

/**
 * rwnx_amsdu_add_subframe - Add this buffer as an A-MSDU subframe if possible
 *
 * @rwnx_hw Driver main data
 * @skb Buffer to aggregate if possible
 * @sta Destination STA
 * @txq sta's txq used for this buffer
 *
 * Try to aggregate the buffer in an A-MSDU. If it succeed then the
 * buffer is added as a new A-MSDU subframe with AMSDU and LLC/SNAP
 * headers added (so FW won't have to modify this subframe).
 *
 * To be added as subframe :
 * - sta must allow amsdu
 * - buffer must be aggregable (to be defined)
 * - at least one other aggregable buffer is pending in the queue
 *  or an a-msdu (with enough free space) is currently in progress
 *
 * returns true if buffer has been added as A-MDSP subframe, false otherwise
 *
 */
static bool rwnx_amsdu_add_subframe(struct rwnx_hw *rwnx_hw, struct sk_buff *skb,
                                    struct rwnx_sta *sta, struct rwnx_txq *txq)
{
    bool res = false;
    struct ethhdr *eth;
#if !defined(CONFIG_RWNX_PCIE_MODE)
    rwnx_hw->mod_params->amsdu_maxnb = 1;
#endif
    /* Adjust the maximum number of MSDU allowed in A-MSDU */
    rwnx_adjust_amsdu_maxnb(rwnx_hw);

    /* immediately return if amsdu are not allowed for this sta */
    if (!txq->amsdu_len || rwnx_hw->mod_params->amsdu_maxnb < 2 ||
        !rwnx_amsdu_is_aggregable(skb)
       )
        return false;

    spin_lock_bh(&rwnx_hw->tx_lock);
    if (txq->amsdu) {
        /* aggreagation already in progress, add this buffer if enough space
           available, otherwise end the current amsdu */
        struct rwnx_sw_txhdr *sw_txhdr = txq->amsdu;
        eth = (struct ethhdr *)(skb->data);

        if (((sw_txhdr->amsdu.len + sw_txhdr->amsdu.pad +
              rwnx_amsdu_subframe_length(eth, skb->len)) > txq->amsdu_len) ||
            rwnx_amsdu_add_subframe_header(rwnx_hw, skb, sw_txhdr)) {
            txq->amsdu = NULL;
            goto end;
        }

        if (sw_txhdr->amsdu.nb >= rwnx_hw->mod_params->amsdu_maxnb) {
            rwnx_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].done++;
            /* max number of subframes reached */
            txq->amsdu = NULL;
        }
    } else {
        /* Check if a new amsdu can be started with the previous buffer
           (if any) and this one */
        struct sk_buff *skb_prev = skb_peek_tail(&txq->sk_list);
        struct rwnx_txhdr *txhdr;
        struct rwnx_sw_txhdr *sw_txhdr;
        int len1, len2;

        if (!skb_prev || !rwnx_amsdu_is_aggregable(skb_prev))
            goto end;

        txhdr = (struct rwnx_txhdr *)skb_prev->data;
        sw_txhdr = txhdr->sw_hdr;
        if ((sw_txhdr->amsdu.len) ||
            (sw_txhdr->desc.api.host.flags & TXU_CNTRL_RETRY))
            /* previous buffer is already a complete amsdu or a retry */
            goto end;

        eth = (struct ethhdr *)skb_mac_header(skb_prev);
        len1 = rwnx_amsdu_subframe_length(eth, (sw_txhdr->frame_len +
                                                sizeof(struct ethhdr)));

        eth = (struct ethhdr *)(skb->data);
        len2 = rwnx_amsdu_subframe_length(eth, skb->len);

        if (len1 + AMSDU_PADDING(len1) + len2 > txq->amsdu_len)
            /* not enough space to aggregate those two buffers */
            goto end;

        /* Add subframe header.
           Note: Fw will take care of adding AMDSU header for the first
           subframe while generating 802.11 MAC header */
        INIT_LIST_HEAD(&sw_txhdr->amsdu.hdrs);
        sw_txhdr->amsdu.len = len1;
        sw_txhdr->amsdu.nb = 1;
        sw_txhdr->amsdu.pad = AMSDU_PADDING(len1);
        if (rwnx_amsdu_add_subframe_header(rwnx_hw, skb, sw_txhdr))
            goto end;

        sw_txhdr->desc.api.host.flags |= TXU_CNTRL_AMSDU;

        if (sw_txhdr->amsdu.nb < rwnx_hw->mod_params->amsdu_maxnb)
            txq->amsdu = sw_txhdr;
        else
            rwnx_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].done++;
    }

    res = true;

  end:
    spin_unlock_bh(&rwnx_hw->tx_lock);
    return res;
}

/**
 * rwnx_amsdu_dismantle - Dismantle an already formatted A-MSDU
 *
 * @rwnx_hw Driver main data
 * @sw_txhdr_main Software descriptor of the A-MSDU to dismantle.
 *
 * The a-mdsu is always fully dismantled (i.e don't try to reduce it's size to
 * fit the new limit).
 * The DMA mapping can be re-used as rwnx_amsdu_add_subframe_header ensure that
 * enough data in the skb bufer are 'DMA mapped'.
 * It would have been slightly simple to unmap/re-map but it is a little faster like this
 * and not that much more complicated to read.
 */
static void rwnx_amsdu_dismantle(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr_main)
{
    struct rwnx_amsdu_txhdr *amsdu_txhdr, *next;
    struct sk_buff *skb_prev = sw_txhdr_main->skb;
#if (defined(CONFIG_RWNX_PCIE_MODE))
    struct rwnx_txq *txq =  sw_txhdr_main->txq;
#endif
    trace_amsdu_dismantle(sw_txhdr_main);

    rwnx_hw->stats.amsdus[sw_txhdr_main->amsdu.nb - 1].done--;
    sw_txhdr_main->amsdu.len = 0;
    sw_txhdr_main->amsdu.nb = 0;
    sw_txhdr_main->desc.api.host.flags &= ~TXU_CNTRL_AMSDU;
    sw_txhdr_main->desc.api.host.packet_cnt = 1;

    list_for_each_entry_safe(amsdu_txhdr, next, &sw_txhdr_main->amsdu.hdrs, list) {
        struct sk_buff *skb = amsdu_txhdr->skb;
        struct rwnx_txhdr *txhdr;
        struct rwnx_sw_txhdr *sw_txhdr;
        size_t frame_len;
        size_t data_oft;

        list_del(&amsdu_txhdr->list);
#if (defined(CONFIG_RWNX_PCIE_MODE))
        rwnx_ipc_sta_buffer(rwnx_hw, txq->sta, txq->tid, -amsdu_txhdr->msdu_len);
#endif
        rwnx_amsdu_del_subframe_header(amsdu_txhdr);

        frame_len = RWNX_TX_DMA_MAP_LEN(skb);

        sw_txhdr = kmem_cache_alloc(rwnx_hw->sw_txhdr_cache, GFP_ATOMIC);
        if (unlikely((skb_headroom(skb) < RWNX_TX_HEADROOM) ||
                     (sw_txhdr == NULL) || (frame_len > amsdu_txhdr->ipc_data.size))) {
            dev_err(rwnx_hw->dev, "Failed to dismantle A-MSDU\n");
            if (sw_txhdr)
                kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
            rwnx_ipc_buf_a2e_release(rwnx_hw, &amsdu_txhdr->ipc_data);
            dev_kfree_skb_any(skb);
            /* coverity[leaked_storage] - variable "sw_txhdr" was free in kmem_cache_free */
            continue;
        }

        // Offset between DMA mapping for an A-MSDU subframe and a simple MPDU
        data_oft = amsdu_txhdr->ipc_data.size - frame_len;

        memcpy(sw_txhdr, sw_txhdr_main, sizeof(*sw_txhdr));
        sw_txhdr->frame_len = frame_len;
        sw_txhdr->skb = skb;
        sw_txhdr->ipc_data = amsdu_txhdr->ipc_data; // It's OK to re-use amsdu_txhdr ptr
        sw_txhdr->desc.api.host.packet_addr[0] = sw_txhdr->ipc_data.dma_addr + data_oft;
        sw_txhdr->desc.api.host.packet_len[0] = frame_len;
        sw_txhdr->desc.api.host.packet_cnt = 1;

        txhdr = (struct rwnx_txhdr *)skb_push(skb, RWNX_TX_HEADROOM);
        txhdr->sw_hdr = sw_txhdr;

        if (rwnx_txq_queue_skb(skb, sw_txhdr->txq, rwnx_hw, false, skb_prev)) {
            ;
        }
        skb_prev = skb;
    }
}


/**
 * rwnx_amsdu_update_len - Update length allowed for A-MSDU on a TXQ
 *
 * @rwnx_hw Driver main data.
 * @txq The TXQ.
 * @amsdu_len New length allowed ofr A-MSDU.
 *
 * If this is a TXQ linked to a STA and the allowed A-MSDU size is reduced it is
 * then necessary to disassemble all A-MSDU currently queued on all STA' txq that
 * are larger than this new limit.
 * Does nothing if the A-MSDU limit increase or stay the same.
 */
static void rwnx_amsdu_update_len(struct rwnx_hw *rwnx_hw, struct rwnx_txq *txq,
                                  u16 amsdu_len)
{
    struct rwnx_sta *sta = txq->sta;
    int tid;

    if (amsdu_len != txq->amsdu_len)
        trace_amsdu_len_update(txq->sta, amsdu_len);

    if (amsdu_len >= txq->amsdu_len) {
        txq->amsdu_len = amsdu_len;
        return;
    }

    if (!sta) {
        netdev_err(txq->ndev, "Non STA txq(%d) with a-amsdu len %d\n",
                   txq->idx, amsdu_len);
        txq->amsdu_len = 0;
        return;
    }

    /* A-MSDU size has been reduced by the firmware, need to dismantle all
       queued a-msdu that are too large. Need to do this for all txq of the STA. */
    foreach_sta_txq(sta, txq, tid, rwnx_hw) {
        struct sk_buff *skb, *skb_next;

        if (txq->amsdu_len <= amsdu_len)
            continue;

        if (txq->last_retry_skb)
            skb = txq->last_retry_skb->next;
        else
            skb = txq->sk_list.next;

        skb_queue_walk_from_safe(&txq->sk_list, skb, skb_next) {
            struct rwnx_txhdr *txhdr = (struct rwnx_txhdr *)skb->data;
            struct rwnx_sw_txhdr *sw_txhdr = txhdr->sw_hdr;
            if ((sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU) &&
                (sw_txhdr->amsdu.len > amsdu_len))
                rwnx_amsdu_dismantle(rwnx_hw, sw_txhdr);

            if (txq->amsdu == sw_txhdr)
                txq->amsdu = NULL;
        }

        txq->amsdu_len = amsdu_len;
    }
}

#endif /* CONFIG_RWNX_AMSDUS_TX */

/**
 * netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
 *                               struct net_device *dev);
 *	Called when a packet needs to be transmitted.
 *	Must return NETDEV_TX_OK , NETDEV_TX_BUSY.
 *        (can also return NETDEV_TX_LOCKED if NETIF_F_LLTX)
 *
 *  - Initialize the desciptor for this pkt (stored in skb before data)
 *  - Push the pkt in the corresponding Txq
 *  - If possible (i.e. credit available and not in PS) the pkt is pushed
 *    to fw
 */
netdev_tx_t rwnx_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_txhdr *txhdr;
    struct rwnx_sw_txhdr *sw_txhdr = NULL;
    struct ethhdr *eth;
    struct txdesc_api *desc;
    struct rwnx_sta *sta;
    struct rwnx_txq *txq;
    u8 tid;
    struct rwnx_eap_hdr *eap_temp;

    sk_pacing_shift_update(skb->sk, rwnx_hw->tcp_pacing_shift);

    // If buffer is shared (or may be used by another interface) need to make a
    // copy as TX infomration is stored inside buffer's headroom
    if (skb_shared(skb) || (skb_headroom(skb) < RWNX_TX_MAX_HEADROOM) ||
        (skb_cloned(skb) && (dev->priv_flags & IFF_BRIDGE_PORT))) {
        struct sk_buff *newskb = skb_copy_expand(skb, RWNX_TX_MAX_HEADROOM, 0, GFP_ATOMIC);
        if (unlikely(newskb == NULL))
            goto free;

        dev_kfree_skb_any(skb);
        skb = newskb;
    }

    /* Get the STA id and TID information */
    sta = rwnx_get_tx_info(rwnx_vif, skb, &tid);
    if (!sta)
        goto free;

    txq = rwnx_txq_sta_get(sta, tid, rwnx_hw);
    if (txq->idx == TXQ_INACTIVE)
        goto free;

#ifdef CONFIG_RWNX_AMSDUS_TX
    if (rwnx_amsdu_add_subframe(rwnx_hw, skb, sta, txq))
        return NETDEV_TX_OK;
#endif

    sw_txhdr = kmem_cache_alloc(rwnx_hw->sw_txhdr_cache, GFP_ATOMIC);
    if (unlikely(sw_txhdr == NULL))
        goto free;

    sw_txhdr->txq       = txq;
    sw_txhdr->frame_len = RWNX_TX_DMA_MAP_LEN(skb);
    sw_txhdr->rwnx_sta  = sta;
    sw_txhdr->rwnx_vif  = rwnx_vif;
    sw_txhdr->skb       = skb;
    sw_txhdr->jiffies   = jiffies;
#ifdef CONFIG_RWNX_AMSDUS_TX
    sw_txhdr->amsdu.len = 0;
    sw_txhdr->amsdu.nb = 0;
#endif

    /* Prepare IPC buffer for DMA transfer */
    eth = (struct ethhdr *)skb->data;
    if (unlikely(rwnx_prep_dma_tx(rwnx_hw, sw_txhdr, eth + 1)))
        goto free;


    /* Fill-in the API descriptor for the MACSW */
    desc = &sw_txhdr->desc.api;
    memcpy(&desc->host.eth_dest_addr, eth->h_dest, ETH_ALEN);
    memcpy(&desc->host.eth_src_addr, eth->h_source, ETH_ALEN);
    desc->host.ethertype = eth->h_proto;
    desc->host.staid = sta->sta_idx;
    desc->host.tid = tid;
    if (unlikely(rwnx_vif->wdev.iftype == NL80211_IFTYPE_AP_VLAN))
        desc->host.vif_idx = rwnx_vif->ap_vlan.master->vif_index;
    else
        desc->host.vif_idx = rwnx_vif->vif_index;
    desc->host.flags = 0;

    if (rwnx_vif->use_4addr && (sta->sta_idx < NX_REMOTE_STA_MAX))
        desc->host.flags |= TXU_CNTRL_USE_4ADDR;

    if ((rwnx_vif->tdls_status == TDLS_LINK_ACTIVE) &&
        rwnx_vif->sta.tdls_sta &&
        (memcmp(desc->host.eth_dest_addr.array, rwnx_vif->sta.tdls_sta->mac_addr, ETH_ALEN) == 0)) {
        desc->host.flags |= TXU_CNTRL_TDLS;
        rwnx_vif->sta.tdls_sta->tdls.last_tid = desc->host.tid;
        rwnx_vif->sta.tdls_sta->tdls.last_sn = 0; //TODO: set this on confirm ?
    }

    if ((rwnx_vif->wdev.iftype == NL80211_IFTYPE_MESH_POINT) &&
        (rwnx_vif->is_resending)) {
        desc->host.flags |= TXU_CNTRL_MESH_FWD;
    }

    /**
     * filer special frame,reuse TXU_CNTRL_MESH_FWD
     * TBD,use own flag in next rom version
     */
    eap_temp = (struct rwnx_eap_hdr *)((uint8_t *)(skb->data) + sizeof(struct ethhdr));
    if (skb->protocol == cpu_to_be16(ETH_P_PAE) || skb->protocol == cpu_to_be16(WAPI_TYPE)) {
        desc->host.flags |= TXU_CNTRL_MESH_FWD;
        printk("filter special frame,ethertype:%x, tid:%x, vif_idx:%x\n", desc->host.ethertype, desc->host.tid, desc->host.vif_idx);
    }

    /* store Tx info in skb headroom */
    txhdr = (struct rwnx_txhdr *)skb_push(skb, RWNX_TX_HEADROOM);
    txhdr->sw_hdr = sw_txhdr;

#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
    printk("%s, ethertype:%x, tid:%x, vif_idx:%x\n", __func__, desc->host.ethertype, desc->host.tid, desc->host.vif_idx);
#endif

    /* queue the buffer */
    spin_lock_bh(&rwnx_hw->tx_lock);
    if (rwnx_txq_queue_skb(skb, txq, rwnx_hw, false, NULL))
        rwnx_hwq_process(rwnx_hw, txq->hwq);
    spin_unlock_bh(&rwnx_hw->tx_lock);

    return NETDEV_TX_OK;

free:
    if (sw_txhdr)
        kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
    dev_kfree_skb_any(skb);
    /* coverity[leaked_storage] - variable "sw_txhdr" was free in kmem_cache_free */
    return NETDEV_TX_OK;
}

/**
 * rwnx_start_mgmt_xmit - Transmit a management frame
 *
 * @vif: Vif that send the frame
 * @sta: Destination of the frame. May be NULL if the destiantion is unknown
 *       to the AP.
 * @params: Mgmt frame parameters
 * @offchan: Indicate whether the frame must be send via the offchan TXQ.
 *           (is is redundant with params->offchan ?)
 * @cookie: updated with a unique value to identify the frame with upper layer
 *
 */
int rwnx_start_mgmt_xmit(struct rwnx_vif *vif, struct rwnx_sta *sta,
                         struct cfg80211_mgmt_tx_params *params, bool offchan,
                         u64 *cookie)
{
    struct rwnx_hw *rwnx_hw = vif->rwnx_hw;
    struct rwnx_txhdr *txhdr;
    struct rwnx_sw_txhdr *sw_txhdr;
    struct txdesc_api *desc;
    struct sk_buff *skb;
    size_t frame_len;
    u8 *data;
    struct rwnx_txq *txq;
    bool robust;

    frame_len = params->len;

    /* Set TID and Queues indexes */
    if (sta) {
        txq = rwnx_txq_sta_get(sta, 8, rwnx_hw);
    } else {
        if (offchan)
            txq = &rwnx_hw->txq[NX_OFF_CHAN_TXQ_IDX];
        else
            txq = rwnx_txq_vif_get(vif, NX_UNK_TXQ_TYPE);
    }

    /* Ensure that TXQ is active */
    if (txq->idx == TXQ_INACTIVE) {
        netdev_dbg(vif->ndev, "TXQ inactive\n");
        return -EBUSY;
    }

    /* Create a SK Buff object that will contain the provided data */
    skb = dev_alloc_skb(RWNX_TX_HEADROOM + frame_len);
    if (!skb)
        return -ENOMEM;
    *cookie = (unsigned long)skb;

    sw_txhdr = kmem_cache_alloc(rwnx_hw->sw_txhdr_cache, GFP_ATOMIC);
    if (unlikely(sw_txhdr == NULL)) {
        dev_kfree_skb(skb);
        return -ENOMEM;
    }

    /* Reserve headroom in skb. Do this so that we can easily re-use ieee80211
       functions that take skb with 802.11 frame as parameter */
    skb_reserve(skb, RWNX_TX_HEADROOM);
    skb_reset_mac_header(skb);

    /* Copy data in skb buffer */
    data = skb_put(skb, frame_len);
    memcpy(data, params->buf, frame_len);
    robust = ieee80211_is_robust_mgmt_frame(skb);

    /* Update CSA counter if present */
    if (unlikely(params->n_csa_offsets) &&
        vif->wdev.iftype == NL80211_IFTYPE_AP &&
        vif->ap.csa) {
        int i;
        for (i = 0; i < params->n_csa_offsets ; i++) {
            data[params->csa_offsets[i]] = vif->ap.csa->count;
        }
    }

    sw_txhdr->txq = txq;
    sw_txhdr->frame_len = frame_len;
    sw_txhdr->rwnx_sta = sta;
    sw_txhdr->rwnx_vif = vif;
    sw_txhdr->skb = skb;
    sw_txhdr->jiffies = jiffies;
#ifdef CONFIG_RWNX_AMSDUS_TX
    sw_txhdr->amsdu.len = 0;
    sw_txhdr->amsdu.nb = 0;
#endif

    /* Prepare IPC buffer for DMA transfer */
    if (unlikely(rwnx_prep_dma_tx(rwnx_hw, sw_txhdr, data))) {
        kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
        dev_kfree_skb(skb);
        /* coverity[leaked_storage] - variable "sw_txhdr" was free in kmem_cache_free */
        return -EBUSY;
    }

    /* Fill-in the API Descriptor for the MACSW */
    desc = &sw_txhdr->desc.api;
    desc->host.staid = (sta) ? sta->sta_idx : 0xFF;
    desc->host.vif_idx = vif->vif_index;
    desc->host.tid = 0xFF;
    desc->host.flags = TXU_CNTRL_MGMT;

    if (robust)
        desc->host.flags |= TXU_CNTRL_MGMT_ROBUST;

    if (params->no_cck)
        desc->host.flags |= TXU_CNTRL_MGMT_NO_CCK;

    /* store Tx info in skb headroom */
    txhdr = (struct rwnx_txhdr *)skb_push(skb, RWNX_TX_HEADROOM);
    txhdr->sw_hdr = sw_txhdr;

    /* queue the buffer */
    spin_lock_bh(&rwnx_hw->tx_lock);
    if (rwnx_txq_queue_skb(skb, txq, rwnx_hw, false, NULL))
        rwnx_hwq_process(rwnx_hw, txq->hwq);
    spin_unlock_bh(&rwnx_hw->tx_lock);

    return 0;
}
#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
#define TX_CFM_LEN  96
int rwnx_tx_cfm_task(void *data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    struct sk_buff *skb = NULL;
    struct tx_cfm_tag cfm;
    struct rwnx_sw_txhdr *sw_txhdr;
    struct rwnx_hwq *hwq;
    struct rwnx_txq *txq;
    struct tx_cfm_tag reset = {0};
    struct sched_param sch_param;
    unsigned int *drv_txcfm_idx = &rwnx_hw->ipc_env->txcfm_idx;
    unsigned int addr;

    sch_param.sched_priority = 91;
    sched_setscheduler(current,SCHED_RR,&sch_param);

    while (1) {
        /* wait for work */
        if (down_interruptible(&rwnx_hw->rwnx_tx_cfm_sem) != 0)
        {
            /* interrupted, exit */
            printk("%s:%d wait rwnx_tx_cfm_sem fail!\n", __func__, __LINE__);
            break;
        }

        while(1) {
            addr = SRAM_TXCFM_START_ADDR + (*drv_txcfm_idx * sizeof(struct tx_cfm_tag));
#if defined(CONFIG_RWNX_USB_MODE)
            rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)&cfm, (unsigned char *)(unsigned long)addr, sizeof(cfm), USB_EP4);

#elif defined(CONFIG_RWNX_SDIO_MODE)
            rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)&cfm, (unsigned char *)(unsigned long)addr, sizeof(cfm));

#endif
            //printk("%s, addr=%x, hostid=%x, idx=%x\n", __func__, addr, cfm.hostid, *drv_txcfm_idx);
            *drv_txcfm_idx = (*drv_txcfm_idx + 1) % SRAM_TXCFM_CNT;

            /* Check host id in the confirmation. */
            /* If 0 it means that this confirmation has not yet been updated by firmware */

            skb = ipc_host_tx_host_id_to_ptr(rwnx_hw->ipc_env, cfm.hostid);

            if (!skb) {
                if (*drv_txcfm_idx > 0)
                    *drv_txcfm_idx = (*drv_txcfm_idx - 1) % SRAM_TXCFM_CNT;
                else
                    *drv_txcfm_idx = SRAM_TXCFM_CNT - 1;

                break;
            } else {
#if defined(CONFIG_RWNX_USB_MODE)
                rwnx_hw->plat->hif_ops->hi_write_sram((unsigned char *)&reset, (unsigned char *)(unsigned long)addr, sizeof(cfm), USB_EP4);
#elif defined(CONFIG_RWNX_SDIO_MODE)
                rwnx_hw->plat->hif_ops->hi_write_ipc_sram((unsigned char *)&reset, (unsigned char *)(unsigned long)addr, sizeof(cfm));
#endif
            }

            sw_txhdr = ((struct rwnx_txhdr *)skb->data)->sw_hdr;
            txq = sw_txhdr->txq;
            /* don't use txq->hwq as it may have changed between push and confirm */
            hwq = &rwnx_hw->hwq[sw_txhdr->hw_queue];

            rwnx_txq_confirm_any(rwnx_hw, txq, hwq, sw_txhdr);

            /* Update txq and HW queue credits */
            if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_MGMT) {
                struct ieee80211_mgmt *mgmt;
                trace_mgmt_cfm(sw_txhdr->rwnx_vif->vif_index,
                               (sw_txhdr->rwnx_sta) ? sw_txhdr->rwnx_sta->sta_idx : 0xFF,
                               cfm.status.acknowledged);
                mgmt = (struct ieee80211_mgmt *)(skb->data + RWNX_TX_HEADROOM);
                if ((ieee80211_is_deauth(mgmt->frame_control)) && (sw_txhdr->rwnx_vif->is_disconnect == 1)) {
                    sw_txhdr->rwnx_vif->is_disconnect = 0;
                }
                /* Confirm transmission to CFG80211 */
                cfg80211_mgmt_tx_status(&sw_txhdr->rwnx_vif->wdev,
                                        (unsigned long)skb, skb_mac_header(skb),
                                        sw_txhdr->frame_len,
                                        cfm.status.acknowledged,
                                        GFP_ATOMIC);
            } else if ((txq->idx != TXQ_INACTIVE) && cfm.status.sw_retry_required) {
                /* firmware postponed this buffer */
                rwnx_tx_retry(rwnx_hw, skb, sw_txhdr, cfm.status);
                break;
            }

            trace_skb_confirm(skb, txq, hwq, &cfm);

            /* STA may have disconnect (and txq stopped) when buffers were stored
                            in fw. In this case do nothing when they're returned */
            if (txq->idx != TXQ_INACTIVE) {
                if (cfm.credits) {
                    txq->credits += cfm.credits;
                    printk("RWNX_TXQ_STOP_FULL, %s, txq->credits=%d\n", __func__, txq->credits );
                    if (txq->credits <= 0) {
                        rwnx_txq_stop(txq, RWNX_TXQ_STOP_FULL);
                    }
                    else if (txq->credits > 0)
                      rwnx_txq_start(txq, RWNX_TXQ_STOP_FULL);
                }

                /* continue service period */
                if (unlikely(txq->push_limit && !rwnx_txq_is_full(txq))) {
                    rwnx_txq_add_to_hw_list(txq);
                }
            }
            /* coverity[result_independent_of_operands] - Enhance code robustness */
            if (cfm.ampdu_size && (cfm.ampdu_size < IEEE80211_MAX_AMPDU_BUF))
                rwnx_hw->stats.ampdus_tx[cfm.ampdu_size - 1]++;

    #ifdef CONFIG_RWNX_AMSDUS_TX
          if (!cfm.status.acknowledged) {
              if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU)
                  rwnx_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].failed++;
              else if (!sw_txhdr->rwnx_sta || !is_multicast_sta(sw_txhdr->rwnx_sta->sta_idx))
                  rwnx_hw->stats.amsdus[0].failed++;
        }

          rwnx_amsdu_update_len(rwnx_hw, txq, cfm.amsdu_size);
    #endif

      /* Release SKBs */
      #ifdef CONFIG_RWNX_AMSDUS_TX
          if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU) {
              struct rwnx_amsdu_txhdr *amsdu_txhdr, *tmp;
              list_for_each_entry_safe(amsdu_txhdr, tmp, &sw_txhdr->amsdu.hdrs, list) {
                  rwnx_amsdu_del_subframe_header(amsdu_txhdr);
                  rwnx_ipc_buf_a2e_release(rwnx_hw, &amsdu_txhdr->ipc_data);
#if (defined(CONFIG_RWNX_PCIE_MODE))
                  rwnx_ipc_sta_buffer(rwnx_hw, txq->sta, txq->tid,
                                      -amsdu_txhdr->msdu_len);
#endif
            rwnx_tx_statistic(sw_txhdr->rwnx_vif, txq, cfm.status, amsdu_txhdr->msdu_len);
                          consume_skb(amsdu_txhdr->skb);
                    }
          }
    #endif /* CONFIG_RWNX_AMSDUS_TX */

        rwnx_ipc_buf_a2e_release(rwnx_hw, &sw_txhdr->ipc_data);
#if (defined(CONFIG_RWNX_PCIE_MODE))
        rwnx_ipc_sta_buffer(rwnx_hw, txq->sta, txq->tid, -sw_txhdr->frame_len);
#endif
          rwnx_tx_statistic(sw_txhdr->rwnx_vif, txq, cfm.status, sw_txhdr->frame_len);

          kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
          skb_pull(skb, RWNX_TX_HEADROOM);
          consume_skb(skb);

        }
    }
     return 0;

}


int rwnx_txdatacfm(void *pthis, void *arg)
{
    struct rwnx_hw *rwnx_hw = pthis;

    up(&rwnx_hw->rwnx_tx_cfm_sem);
    return 0;
}
#else
/**
 * rwnx_txdatacfm - FW callback for TX confirmation
 *
 * @pthis: Pointer to the object attached to the IPC structure
 *         (points to struct rwnx_hw is this case)
 * @arg: IPC buffer with the TX confirmation
 *
 * This function is called for each confimration of transmission by the fw.
 * Called with tx_lock hold
 *
 */
int rwnx_txdatacfm(void *pthis, void *arg)
{
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_buf *ipc_cfm = arg;
    struct tx_cfm_tag *cfm = ipc_cfm->addr;
    struct sk_buff *skb;
    struct rwnx_sw_txhdr *sw_txhdr;
    struct rwnx_hwq *hwq;
    struct rwnx_txq *txq;

    skb = rwnx_ipc_get_skb_from_cfm(rwnx_hw, ipc_cfm);
    if (!skb)
        return -1;

    sw_txhdr = ((struct rwnx_txhdr *)skb->data)->sw_hdr;
    txq = sw_txhdr->txq;
    /* don't use txq->hwq as it may have changed between push and confirm */
    hwq = &rwnx_hw->hwq[sw_txhdr->hw_queue];

    rwnx_txq_confirm_any(rwnx_hw, txq, hwq, sw_txhdr);

    /* Update txq and HW queue credits */
    if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_MGMT) {
        struct ieee80211_mgmt *mgmt;
        trace_mgmt_cfm(sw_txhdr->rwnx_vif->vif_index,
                       (sw_txhdr->rwnx_sta) ? sw_txhdr->rwnx_sta->sta_idx : 0xFF,
                       cfm->status.acknowledged);
        mgmt = (struct ieee80211_mgmt *)(skb->data + RWNX_TX_HEADROOM);
        if ((ieee80211_is_deauth(mgmt->frame_control)) && (sw_txhdr->rwnx_vif->is_disconnect == 1)) {
            sw_txhdr->rwnx_vif->is_disconnect = 0;
        }
        /* Confirm transmission to CFG80211 */
        cfg80211_mgmt_tx_status(&sw_txhdr->rwnx_vif->wdev,
                                (unsigned long)skb, skb_mac_header(skb),
                                sw_txhdr->frame_len,
                                cfm->status.acknowledged,
                                GFP_ATOMIC);
    } else if ((txq->idx != TXQ_INACTIVE) && cfm->status.sw_retry_required) {
        /* firmware postponed this buffer */
        rwnx_tx_retry(rwnx_hw, skb, sw_txhdr, cfm->status);
        return 0;
    }

    trace_skb_confirm(skb, txq, hwq, cfm);

    /* STA may have disconnect (and txq stopped) when buffers were stored
       in fw. In this case do nothing when they're returned */
    if (txq->idx != TXQ_INACTIVE) {
        if (cfm->credits) {
            txq->credits += cfm->credits;
            if (txq->credits <= 0)
                rwnx_txq_stop(txq, RWNX_TXQ_STOP_FULL);
            else if (txq->credits > 0)
                rwnx_txq_start(txq, RWNX_TXQ_STOP_FULL);
        }

        /* continue service period */
        if (unlikely(txq->push_limit && !rwnx_txq_is_full(txq))) {
            rwnx_txq_add_to_hw_list(txq);
        }
    }
    /* coverity[result_independent_of_operands] - Enhance code robustness */
    if (cfm->ampdu_size && (cfm->ampdu_size < IEEE80211_MAX_AMPDU_BUF))
        rwnx_hw->stats.ampdus_tx[cfm->ampdu_size - 1]++;

#ifdef CONFIG_RWNX_AMSDUS_TX
    if (!cfm->status.acknowledged) {
        if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU)
            rwnx_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].failed++;
        else if (!sw_txhdr->rwnx_sta || !is_multicast_sta(sw_txhdr->rwnx_sta->sta_idx))
            rwnx_hw->stats.amsdus[0].failed++;
    }

    rwnx_amsdu_update_len(rwnx_hw, txq, cfm->amsdu_size);
#endif

    /* Release SKBs */
#ifdef CONFIG_RWNX_AMSDUS_TX
    if (sw_txhdr->desc.api.host.flags & TXU_CNTRL_AMSDU) {
        struct rwnx_amsdu_txhdr *amsdu_txhdr, *tmp;
        list_for_each_entry_safe(amsdu_txhdr, tmp, &sw_txhdr->amsdu.hdrs, list) {
            rwnx_amsdu_del_subframe_header(amsdu_txhdr);
            rwnx_ipc_buf_a2e_release(rwnx_hw, &amsdu_txhdr->ipc_data);
#if defined(CONFIG_RWNX_PCIE_MODE)
            rwnx_ipc_sta_buffer(rwnx_hw, txq->sta, txq->tid,
                                -amsdu_txhdr->msdu_len);
#endif
            rwnx_tx_statistic(sw_txhdr->rwnx_vif, txq, cfm->status, amsdu_txhdr->msdu_len);
            consume_skb(amsdu_txhdr->skb);
        }
    }
#endif /* CONFIG_RWNX_AMSDUS_TX */

    rwnx_ipc_buf_a2e_release(rwnx_hw, &sw_txhdr->ipc_data);
#if defined(CONFIG_RWNX_PCIE_MODE)
    rwnx_ipc_sta_buffer(rwnx_hw, txq->sta, txq->tid, -sw_txhdr->frame_len);
#endif
    rwnx_tx_statistic(sw_txhdr->rwnx_vif, txq, cfm->status, sw_txhdr->frame_len);

    kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
    skb_pull(skb, RWNX_TX_HEADROOM);
    consume_skb(skb);

    return 0;
}
#endif
/**
 * rwnx_txq_credit_update - Update credit for one txq
 *
 * @rwnx_hw: Driver main data
 * @sta_idx: STA idx
 * @tid: TID
 * @update: offset to apply in txq credits
 *
 * Called when fw send ME_TX_CREDITS_UPDATE_IND message.
 * Apply @update to txq credits, and stop/start the txq if needed
 */
void rwnx_txq_credit_update(struct rwnx_hw *rwnx_hw, int sta_idx, u8 tid, s8 update)
{
    struct rwnx_sta *sta = &rwnx_hw->sta_table[sta_idx];
    struct rwnx_txq *txq;
    struct sk_buff *tx_skb;

    txq = rwnx_txq_sta_get(sta, tid, rwnx_hw);

    spin_lock(&rwnx_hw->tx_lock);

    if (txq->idx != TXQ_INACTIVE) {
        txq->credits += update;
        trace_credit_update(txq, update);
        if (txq->credits <= 0)
            rwnx_txq_stop(txq, RWNX_TXQ_STOP_FULL);
        else
            rwnx_txq_start(txq, RWNX_TXQ_STOP_FULL);
    }

    // Drop all the retry packets of a BA that was deleted
    if (update < NX_TXQ_INITIAL_CREDITS) {
        int packet;

        for (packet = 0; packet < txq->nb_retry; packet++) {
            tx_skb = skb_peek(&txq->sk_list);
            if (tx_skb != NULL) {
                rwnx_txq_drop_skb(txq, tx_skb, rwnx_hw, true);
            }
        }
    }

    spin_unlock(&rwnx_hw->tx_lock);
}
