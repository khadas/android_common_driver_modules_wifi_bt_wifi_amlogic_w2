/**
 ******************************************************************************
 *
 * @file rwnx_rx.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#include <linux/dma-mapping.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>

#include "rwnx_defs.h"
#include "rwnx_rx.h"
#include "rwnx_tx.h"
#include "rwnx_prof.h"
#include "ipc_host.h"
#include "rwnx_utils.h"
#include "rwnx_events.h"
#include "rwnx_compat.h"
#include "share_mem_map.h"
#include "reg_ipc_app.h"

struct vendor_radiotap_hdr {
    u8 oui[3];
    u8 subns;
    u16 len;
    u8 data[];
};

/**
 * rwnx_rx_get_vif - Return pointer to the destination vif
 *
 * @rwnx_hw: main driver data
 * @vif_idx: vif index present in rx descriptor
 *
 * Select the vif that should receive this frame. Returns NULL if the destination
 * vif is not active or vif is not specified in the descriptor.
 */
static inline
struct rwnx_vif *rwnx_rx_get_vif(struct rwnx_hw *rwnx_hw, int vif_idx)
{
    struct rwnx_vif *rwnx_vif = NULL;

    if (vif_idx < NX_VIRT_DEV_MAX) {
        rwnx_vif = rwnx_hw->vif_table[vif_idx];
        if (!rwnx_vif || !rwnx_vif->up)
            return NULL;
    }

    return rwnx_vif;
}

/**
 * rwnx_rx_statistic - save some statistics about received frames
 *
 * @rwnx_hw: main driver data.
 * @hw_rxhdr: Rx Hardware descriptor of the received frame.
 * @sta: STA that sent the frame.
 */
static void rwnx_rx_statistic(struct rwnx_hw *rwnx_hw, struct hw_rxhdr *hw_rxhdr,
                              struct rwnx_sta *sta)
{
    struct rwnx_stats *stats = &rwnx_hw->stats;
#ifdef CONFIG_RWNX_DEBUGFS
    struct rwnx_rx_rate_stats *rate_stats = &sta->stats.rx_rate;
    struct rx_vector_1 *rxvect = &hw_rxhdr->hwvect.rx_vect1;
    int mpdu, ampdu, mpdu_prev, rate_idx;

    /* update ampdu rx stats */
    mpdu = hw_rxhdr->hwvect.mpdu_cnt;
    ampdu = hw_rxhdr->hwvect.ampdu_cnt;
    mpdu_prev = stats->ampdus_rx_map[ampdu];

    /* work-around, for MACHW that incorrectly return 63 for last MPDU of A-MPDU or S-MPDU */
    if (mpdu == 63) {
        if (ampdu == stats->ampdus_rx_last)
            mpdu = mpdu_prev + 1;
        else
            mpdu = 0;
    }

    if (ampdu != stats->ampdus_rx_last) {
        stats->ampdus_rx[mpdu_prev]++;
        stats->ampdus_rx_miss += mpdu;
    } else {
        if (mpdu <= mpdu_prev) {
            /* lost 4 (or a multiple of 4) complete A-MPDU/S-MPDU */
            stats->ampdus_rx_miss += mpdu;
        } else {
            stats->ampdus_rx_miss += mpdu - mpdu_prev - 1;
        }
    }

    stats->ampdus_rx_map[ampdu] = mpdu;
    stats->ampdus_rx_last = ampdu;

    /* update rx rate statistic */
    if (!rate_stats->size)
        return;

    if (rxvect->format_mod > FORMATMOD_NON_HT_DUP_OFDM) {
        int mcs;
        int bw = rxvect->ch_bw;
        int sgi;
        int nss;
        switch (rxvect->format_mod) {
            case FORMATMOD_HT_MF:
            case FORMATMOD_HT_GF:
                mcs = rxvect->ht.mcs % 8;
                nss = rxvect->ht.mcs / 8;
                sgi = rxvect->ht.short_gi;
                rate_idx = N_CCK + N_OFDM + nss * 32 + mcs * 4 +  bw * 2 + sgi;
                break;
            case FORMATMOD_VHT:
                mcs = rxvect->vht.mcs;
                nss = rxvect->vht.nss;
                sgi = rxvect->vht.short_gi;
                rate_idx = N_CCK + N_OFDM + N_HT + nss * 80 + mcs * 8 + bw * 2 + sgi;
                break;
            case FORMATMOD_HE_SU:
                mcs = rxvect->he.mcs;
                nss = rxvect->he.nss;
                sgi = rxvect->he.gi_type;
                rate_idx = N_CCK + N_OFDM + N_HT + N_VHT + nss * 144 + mcs * 12 + bw * 3 + sgi;
                break;
            case FORMATMOD_HE_MU:
                mcs = rxvect->he.mcs;
                nss = rxvect->he.nss;
                sgi = rxvect->he.gi_type;
                rate_idx = N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU
                    + nss * 216 + mcs * 18 + rxvect->he.ru_size * 3 + sgi;
                break;
            default:
                mcs = rxvect->he.mcs;
                sgi = rxvect->he.gi_type;
                rate_idx = N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU + N_HE_MU
                    + rxvect->he.ru_size * 9 + mcs * 3 + sgi;
        }
    } else {
        int idx = legrates_lut[rxvect->leg_rate].idx;
        if (idx < 4) {
            rate_idx = idx * 2 + rxvect->pre_type;
        } else {
            rate_idx = N_CCK + idx - 4;
        }
    }
    if (rate_idx < rate_stats->size) {
        if (!rate_stats->table[rate_idx])
            rate_stats->rate_cnt++;
        rate_stats->table[rate_idx]++;
        rate_stats->cpt++;
    } else {
        wiphy_err(rwnx_hw->wiphy, "RX: Invalid index conversion => %d/%d\n",
                  rate_idx, rate_stats->size);
    }
#endif

    /* Always save complete hwvect */
    sta->stats.last_rx = hw_rxhdr->hwvect;

    sta->stats.rx_pkts ++;
    sta->stats.rx_bytes += hw_rxhdr->hwvect.len;
    sta->stats.last_act = jiffies;
}

/**
 * rwnx_rx_defer_skb - Defer processing of a SKB
 *
 * @rwnx_hw: main driver data
 * @rwnx_vif: vif that received the buffer
 * @skb: buffer to defer
 */
void rwnx_rx_defer_skb(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                       struct sk_buff *skb)
{
    struct rwnx_defer_rx_cb *rx_cb = (struct rwnx_defer_rx_cb *)skb->cb;

    // for now don't support deferring the same buffer on several interfaces
    if (skb_shared(skb))
        return;

    // Increase ref count to avoid freeing the buffer until it is processed
    skb_get(skb);

    rx_cb->vif = rwnx_vif;
    skb_queue_tail(&rwnx_hw->defer_rx.sk_list, skb);
    schedule_work(&rwnx_hw->defer_rx.work);
}

/**
 * rwnx_rx_data_skb - Process one data frame
 *
 * @rwnx_hw: main driver data
 * @rwnx_vif: vif that received the buffer
 * @skb: skb received
 * @rxhdr: HW rx descriptor
 * @return: true if buffer has been forwarded to upper layer
 *
 * If buffer is amsdu , it is first split into a list of skb.
 * Then each skb may be:
 * - forwarded to upper layer
 * - resent on wireless interface
 *
 * When vif is a STA interface, every skb is only forwarded to upper layer.
 * When vif is an AP interface, multicast skb are forwarded and resent, whereas
 * skb for other BSS's STA are only resent.
 */
static bool rwnx_rx_data_skb(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                             struct sk_buff *skb,  struct hw_rxhdr *rxhdr)
{
    struct sk_buff_head list;
    struct sk_buff *rx_skb;
    bool amsdu = rxhdr->flags_is_amsdu;
    bool resend = false, forward = true;
    int skip_after_eth_hdr = 0;

    skb->dev = rwnx_vif->ndev;

    __skb_queue_head_init(&list);

    if (amsdu) {
        int count;
        ieee80211_amsdu_to_8023s(skb, &list, rwnx_vif->ndev->dev_addr,
                                 RWNX_VIF_TYPE(rwnx_vif), 0, NULL, NULL);

        count = skb_queue_len(&list);
        if (count > ARRAY_SIZE(rwnx_hw->stats.amsdus_rx))
            count = ARRAY_SIZE(rwnx_hw->stats.amsdus_rx);
        if (count > 0)
            rwnx_hw->stats.amsdus_rx[count - 1]++;
    } else {
        rwnx_hw->stats.amsdus_rx[0]++;
        __skb_queue_head(&list, skb);
    }

    if (((RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_AP) ||
         (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_AP_VLAN) ||
         (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_P2P_GO)) &&
        !(rwnx_vif->ap.flags & RWNX_AP_ISOLATE)) {
        const struct ethhdr *eth = NULL;
        rx_skb = skb_peek(&list);
        if (rx_skb != NULL) {
            skb_reset_mac_header(rx_skb);
            eth = eth_hdr(rx_skb);
        }

        if (eth && unlikely(is_multicast_ether_addr(eth->h_dest))) {
            /* broadcast pkt need to be forwared to upper layer and resent
               on wireless interface */
            resend = true;
        } else {
            /* unicast pkt for STA inside the BSS, no need to forward to upper
               layer simply resend on wireless interface */
            if (rxhdr->flags_dst_idx != RWNX_INVALID_STA)
            {
                struct rwnx_sta *sta = &rwnx_hw->sta_table[rxhdr->flags_dst_idx];
                if (sta->valid && (sta->vlan_idx == rwnx_vif->vif_index))
                {
                    forward = false;
                    resend = true;
                }
            }
        }
    } else if (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_MESH_POINT) {
        const struct ethhdr *eth = NULL;
        rx_skb = skb_peek(&list);
        if (rx_skb != NULL) {
            skb_reset_mac_header(rx_skb);
            eth = eth_hdr(rx_skb);
        }

        if (rxhdr->flags_dst_idx != RWNX_INVALID_STA)
        {
            resend = true;

            if (eth && is_multicast_ether_addr(eth->h_dest)) {
                // MC/BC frames are uploaded with mesh control and LLC/snap
                // (so they can be mesh forwarded) that need to be removed.
                uint8_t *mesh_ctrl = (uint8_t *)(eth + 1);
                skip_after_eth_hdr = 8 + 6;

                if ((*mesh_ctrl & MESH_FLAGS_AE) == MESH_FLAGS_AE_A4)
                    skip_after_eth_hdr += ETH_ALEN;
                else if ((*mesh_ctrl & MESH_FLAGS_AE) == MESH_FLAGS_AE_A5_A6)
                    skip_after_eth_hdr += 2 * ETH_ALEN;
            } else {
                forward = false;
            }
        }
    }

    while (!skb_queue_empty(&list)) {
        rx_skb = __skb_dequeue(&list);

        /* resend pkt on wireless interface */
        if (resend) {
            struct sk_buff *skb_copy;
            /* always need to copy buffer even when forward=0 to get enough headrom for tsdesc */
            skb_copy = skb_copy_expand(rx_skb, RWNX_TX_MAX_HEADROOM, 0, GFP_ATOMIC);
            if (skb_copy) {
                int res;
                skb_copy->protocol = htons(ETH_P_802_3);
                skb_reset_network_header(skb_copy);
                skb_reset_mac_header(skb_copy);

                rwnx_vif->is_resending = true;
                res = dev_queue_xmit(skb_copy);
                rwnx_vif->is_resending = false;
                /* note: buffer is always consummed by dev_queue_xmit */
                if (res == NET_XMIT_DROP) {
                    rwnx_vif->net_stats.rx_dropped++;
                    rwnx_vif->net_stats.tx_dropped++;
                } else if (res != NET_XMIT_SUCCESS) {
                    netdev_err(rwnx_vif->ndev,
                               "Failed to re-send buffer to driver (res=%d)",
                               res);
                    rwnx_vif->net_stats.tx_errors++;
                }
            } else {
                netdev_err(rwnx_vif->ndev, "Failed to copy skb");
            }
        }

        /* forward pkt to upper layer */
        if (forward) {
            if (rx_skb != NULL) {
                rx_skb->protocol = eth_type_trans(rx_skb, rwnx_vif->ndev);
                memset(rx_skb->cb, 0, sizeof(rx_skb->cb));
            }

            // Special case for MESH when BC/MC is uploaded and resend
            if (unlikely(skip_after_eth_hdr)) {
                memmove(skb_mac_header(rx_skb) + skip_after_eth_hdr,
                        skb_mac_header(rx_skb), sizeof(struct ethhdr));
                __skb_pull(rx_skb, skip_after_eth_hdr);
                skb_reset_mac_header(rx_skb);
                skip_after_eth_hdr = 0;
            }

            /* Update statistics */
            rwnx_vif->net_stats.rx_packets++;
            rwnx_vif->net_stats.rx_bytes += rx_skb->len;
            REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_IEEE80211RX);
            netif_receive_skb(rx_skb);
            REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_IEEE80211RX);

        }
    }

    return forward;
}

/**
 * rwnx_rx_mgmt - Process one 802.11 management frame
 *
 * @rwnx_hw: main driver data
 * @rwnx_vif: vif to upload the buffer to
 * @skb: skb received
 * @rxhdr: HW rx descriptor
 *
 * Forward the management frame to a given interface.
 */
static void rwnx_rx_mgmt(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                         struct sk_buff *skb,  struct hw_rxhdr *hw_rxhdr)
{
    struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
    struct rx_vector_1 *rxvect = &hw_rxhdr->hwvect.rx_vect1;

    if (ieee80211_is_beacon(mgmt->frame_control)) {
        if ((RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_MESH_POINT) &&
            hw_rxhdr->flags_new_peer) {
            cfg80211_notify_new_peer_candidate(rwnx_vif->ndev, mgmt->sa,
                                               mgmt->u.beacon.variable,
                                               skb->len - offsetof(struct ieee80211_mgmt,
                                                                   u.beacon.variable),
                                               rxvect->rssi1, GFP_ATOMIC);
        } else {
            cfg80211_report_obss_beacon(rwnx_hw->wiphy, skb->data, skb->len,
                                        hw_rxhdr->phy_info.phy_prim20_freq,
                                        rxvect->rssi1);
        }
    } else if ((ieee80211_is_deauth(mgmt->frame_control) ||
                ieee80211_is_disassoc(mgmt->frame_control)) &&
               (mgmt->u.deauth.reason_code == WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA ||
                mgmt->u.deauth.reason_code == WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA)) {
        cfg80211_rx_unprot_mlme_mgmt(rwnx_vif->ndev, skb->data, skb->len);
    } else if ((RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_STATION) &&
               (ieee80211_is_action(mgmt->frame_control) &&
                (mgmt->u.action.category == 6))) {
        // Wpa_supplicant will ignore the FT action frame if reported via cfg80211_rx_mgmt
        // and cannot call cfg80211_ft_event from atomic context so defer message processing
        rwnx_rx_defer_skb(rwnx_hw, rwnx_vif, skb);
    } else {
        cfg80211_rx_mgmt(&rwnx_vif->wdev, hw_rxhdr->phy_info.phy_prim20_freq,
                         rxvect->rssi1, skb->data, skb->len, 0);
    }
}

/**
 * rwnx_rx_mgmt_any - Process one 802.11 management frame
 *
 * @rwnx_hw: main driver data
 * @skb: skb received
 * @rxhdr: HW rx descriptor
 *
 * Process the management frame and free the corresponding skb.
 * If vif is not specified in the rx descriptor, the the frame is uploaded
 * on all active vifs.
 */
static void rwnx_rx_mgmt_any(struct rwnx_hw *rwnx_hw, struct sk_buff *skb,
                             struct hw_rxhdr *hw_rxhdr)
{
    struct rwnx_vif *rwnx_vif;
    int vif_idx = hw_rxhdr->flags_vif_idx;

    trace_mgmt_rx(hw_rxhdr->phy_info.phy_prim20_freq, vif_idx,
                  hw_rxhdr->flags_sta_idx, (struct ieee80211_mgmt *)skb->data);

    if (vif_idx == RWNX_INVALID_VIF) {
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (! rwnx_vif->up)
                continue;
            rwnx_rx_mgmt(rwnx_hw, rwnx_vif, skb, hw_rxhdr);
        }
    } else {
        rwnx_vif = rwnx_rx_get_vif(rwnx_hw, vif_idx);
        if (rwnx_vif)
            rwnx_rx_mgmt(rwnx_hw, rwnx_vif, skb, hw_rxhdr);
    }

    dev_kfree_skb(skb);
}

/**
 * rwnx_rx_rtap_hdrlen - Return radiotap header length
 *
 * @rxvect: Rx vector used to fill the radiotap header
 * @has_vend_rtap: boolean indicating if vendor specific data is present
 *
 * Compute the length of the radiotap header based on @rxvect and vendor
 * specific data (if any).
 */
static u8 rwnx_rx_rtap_hdrlen(struct rx_vector_1 *rxvect,
                              bool has_vend_rtap)
{
    u8 rtap_len;

    /* Compute radiotap header length */
    rtap_len = sizeof(struct ieee80211_radiotap_header) + 8;

    // Check for multiple antennas
    if (hweight32(rxvect->antenna_set) > 1)
        // antenna and antenna signal fields
        rtap_len += 4 * hweight8(rxvect->antenna_set);

    // TSFT
    if (!has_vend_rtap) {
        rtap_len = ALIGN(rtap_len, 8);
        rtap_len += 8;
    }

    // IEEE80211_HW_SIGNAL_DBM
    rtap_len++;

    // Check if single antenna
    if (hweight32(rxvect->antenna_set) == 1)
        rtap_len++; //Single antenna

    // padding for RX FLAGS
    rtap_len = ALIGN(rtap_len, 2);

    // Check for HT frames
    if ((rxvect->format_mod == FORMATMOD_HT_MF) ||
        (rxvect->format_mod == FORMATMOD_HT_GF))
        rtap_len += 3;

    // Check for AMPDU
    if (!(has_vend_rtap) && ((rxvect->format_mod >= FORMATMOD_VHT) ||
                             ((rxvect->format_mod > FORMATMOD_NON_HT_DUP_OFDM) &&
                                                     (rxvect->ht.aggregation)))) {
        rtap_len = ALIGN(rtap_len, 4);
        rtap_len += 8;
    }

    // Check for VHT frames
    if (rxvect->format_mod == FORMATMOD_VHT) {
        rtap_len = ALIGN(rtap_len, 2);
        rtap_len += 12;
    }

    // Check for HE frames
    if (rxvect->format_mod == FORMATMOD_HE_SU) {
        rtap_len = ALIGN(rtap_len, 2);
        rtap_len += sizeof(struct ieee80211_radiotap_he);
    }

    // Check for multiple antennas
    if (hweight32(rxvect->antenna_set) > 1) {
        // antenna and antenna signal fields
        rtap_len += 2 * hweight8(rxvect->antenna_set);
    }

    // Check for vendor specific data
    if (has_vend_rtap) {
        /* vendor presence bitmap */
        rtap_len += 4;
        /* alignment for fixed 6-byte vendor data header */
        rtap_len = ALIGN(rtap_len, 2);
    }

    return rtap_len;
}

/**
 * rwnx_rx_add_rtap_hdr - Add radiotap header to sk_buff
 *
 * @rwnx_hw: main driver data
 * @skb: skb received (will include the radiotap header)
 * @rxvect: Rx vector
 * @phy_info: Information regarding the phy
 * @hwvect: HW Info (NULL if vendor specific data is available)
 * @rtap_len: Length of the radiotap header
 * @vend_rtap_len: radiotap vendor length (0 if not present)
 * @vend_it_present: radiotap vendor present
 *
 * Builds a radiotap header and add it to @skb.
 */
static void rwnx_rx_add_rtap_hdr(struct rwnx_hw* rwnx_hw,
                                 struct sk_buff *skb,
                                 struct rx_vector_1 *rxvect,
                                 struct phy_channel_info_desc *phy_info,
                                 struct hw_vect *hwvect,
                                 int rtap_len,
                                 u8 vend_rtap_len,
                                 u32 vend_it_present)
{
    struct ieee80211_radiotap_header *rtap;
    u8 *pos, rate_idx;
    __le32 *it_present;
    u32 it_present_val = 0;
    bool fec_coding = false;
    bool short_gi = false;
    bool stbc = false;
    bool aggregation = false;

    rtap = (struct ieee80211_radiotap_header *)skb_push(skb, rtap_len);
    memset((u8*) rtap, 0, rtap_len);

    rtap->it_version = 0;
    rtap->it_pad = 0;
    rtap->it_len = cpu_to_le16(rtap_len + vend_rtap_len);

    it_present = &rtap->it_present;

    // Check for multiple antennas
    if (hweight32(rxvect->antenna_set) > 1) {
        int chain;
        unsigned long chains = rxvect->antenna_set;

        for_each_set_bit(chain, &chains, IEEE80211_MAX_CHAINS) {
            it_present_val |=
                BIT(IEEE80211_RADIOTAP_EXT) |
                BIT(IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
            put_unaligned_le32(it_present_val, it_present);
            it_present++;
            it_present_val = BIT(IEEE80211_RADIOTAP_ANTENNA) |
                             BIT(IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
        }
    }

    // Check if vendor specific data is present
    if (vend_rtap_len) {
        it_present_val |= BIT(IEEE80211_RADIOTAP_VENDOR_NAMESPACE) |
                          BIT(IEEE80211_RADIOTAP_EXT);
        put_unaligned_le32(it_present_val, it_present);
        it_present++;
        it_present_val = vend_it_present;
    }

    /* coverity[overrun-local] - will not overrunning it_present */
    put_unaligned_le32(it_present_val, it_present);
    pos = (void *)(it_present + 1);

    // IEEE80211_RADIOTAP_TSFT
    if (hwvect) {
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_TSFT);
        // padding
        while ((pos - (u8 *)rtap) & 7)
            *pos++ = 0;
        put_unaligned_le64((((u64)le32_to_cpu(hwvect->tsf_hi) << 32) +
                            (u64)le32_to_cpu(hwvect->tsf_lo)), pos);
        pos += 8;
    }

    // IEEE80211_RADIOTAP_FLAGS
    rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_FLAGS);
    if (hwvect && (!hwvect->status.frm_successful_rx))
        *pos |= IEEE80211_RADIOTAP_F_BADFCS;
    if (!rxvect->pre_type
            && (rxvect->format_mod <= FORMATMOD_NON_HT_DUP_OFDM))
        *pos |= IEEE80211_RADIOTAP_F_SHORTPRE;
    pos++;

    // IEEE80211_RADIOTAP_RATE
    // check for HT, VHT or HE frames
    if (rxvect->format_mod >= FORMATMOD_HE_SU) {
        rate_idx = rxvect->he.mcs;
        fec_coding = rxvect->he.fec;
        stbc = rxvect->he.stbc;
        aggregation = true;
        *pos = 0;
    } else if (rxvect->format_mod == FORMATMOD_VHT) {
        rate_idx = rxvect->vht.mcs;
        fec_coding = rxvect->vht.fec;
        short_gi = rxvect->vht.short_gi;
        stbc = rxvect->vht.stbc;
        aggregation = true;
        *pos = 0;
    } else if (rxvect->format_mod > FORMATMOD_NON_HT_DUP_OFDM) {
        rate_idx = rxvect->ht.mcs;
        fec_coding = rxvect->ht.fec;
        short_gi = rxvect->ht.short_gi;
        stbc = rxvect->ht.stbc;
        aggregation = rxvect->ht.aggregation;
        *pos = 0;
    } else {
        struct ieee80211_supported_band* band =
                rwnx_hw->wiphy->bands[phy_info->phy_band];
        s16 legrates_idx = legrates_lut[rxvect->leg_rate].idx;
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
        BUG_ON(legrates_idx == -1);
        rate_idx = legrates_idx;
        if (phy_info->phy_band == NL80211_BAND_5GHZ)
            rate_idx -= 4;  /* rwnx_ratetable_5ghz[0].hw_value == 4 */
        *pos = DIV_ROUND_UP(band->bitrates[rate_idx].bitrate, 5);
    }
    pos++;

    // IEEE80211_RADIOTAP_CHANNEL
    rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_CHANNEL);
    put_unaligned_le16(phy_info->phy_prim20_freq, pos);
    pos += 2;

    if (phy_info->phy_band == NL80211_BAND_5GHZ)
        put_unaligned_le16(IEEE80211_CHAN_OFDM | IEEE80211_CHAN_5GHZ, pos);
    else if (rxvect->format_mod > FORMATMOD_NON_HT_DUP_OFDM)
        put_unaligned_le16(IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ, pos);
    else
        put_unaligned_le16(IEEE80211_CHAN_CCK | IEEE80211_CHAN_2GHZ, pos);
    pos += 2;

    if (hweight32(rxvect->antenna_set) == 1) {
        // IEEE80211_RADIOTAP_DBM_ANTSIGNAL
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
        *pos++ = rxvect->rssi1;

        // IEEE80211_RADIOTAP_ANTENNA
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_ANTENNA);
        *pos++ = rxvect->antenna_set;
    }

    // IEEE80211_RADIOTAP_LOCK_QUALITY is missing
    // IEEE80211_RADIOTAP_DB_ANTNOISE is missing

    // IEEE80211_RADIOTAP_RX_FLAGS
    rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RX_FLAGS);
    // 2 byte alignment
    if ((pos - (u8 *)rtap) & 1)
        *pos++ = 0;
    put_unaligned_le16(0, pos);
    //Right now, we only support fcs error (no RX_FLAG_FAILED_PLCP_CRC)
    pos += 2;

    // Check if HT
    if ((rxvect->format_mod == FORMATMOD_HT_MF) ||
        (rxvect->format_mod == FORMATMOD_HT_GF)) {
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
        *pos++ = (IEEE80211_RADIOTAP_MCS_HAVE_MCS |
                  IEEE80211_RADIOTAP_MCS_HAVE_GI |
                  IEEE80211_RADIOTAP_MCS_HAVE_BW |
                  IEEE80211_RADIOTAP_MCS_HAVE_FMT |
                  IEEE80211_RADIOTAP_MCS_HAVE_FEC |
                  IEEE80211_RADIOTAP_MCS_HAVE_STBC);

        pos++;
        *pos = 0;
        if (short_gi)
            *pos |= IEEE80211_RADIOTAP_MCS_SGI;
        if (rxvect->ch_bw  == PHY_CHNL_BW_40)
            *pos |= IEEE80211_RADIOTAP_MCS_BW_40;
        if (rxvect->format_mod == FORMATMOD_HT_GF)
            *pos |= IEEE80211_RADIOTAP_MCS_FMT_GF;
        if (fec_coding)
            *pos |= IEEE80211_RADIOTAP_MCS_FEC_LDPC;
        *pos++ |= stbc << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
        *pos++ = rate_idx;
    }

    // check for HT or VHT frames
    if (aggregation && hwvect) {
        // 4 byte alignment
        while ((pos - (u8 *)rtap) & 3)
            pos++;
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_AMPDU_STATUS);
        put_unaligned_le32(hwvect->ampdu_cnt, pos);
        pos += 4;
        put_unaligned_le32(0, pos);
        pos += 4;
    }

    // Check for VHT frames
    if (rxvect->format_mod == FORMATMOD_VHT) {
        u16 vht_details = IEEE80211_RADIOTAP_VHT_KNOWN_GI |
                          IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH;
        u8 vht_nss = rxvect->vht.nss + 1;

        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_VHT);

        if ((rxvect->ch_bw == PHY_CHNL_BW_160)
                && phy_info->phy_center2_freq)
            vht_details &= ~IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH;
        put_unaligned_le16(vht_details, pos);
        pos += 2;

        // flags
        if (short_gi)
            *pos |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
        if (stbc)
            *pos |= IEEE80211_RADIOTAP_VHT_FLAG_STBC;
        pos++;

        // bandwidth
        if (rxvect->ch_bw == PHY_CHNL_BW_40)
            *pos++ = 1;
        if (rxvect->ch_bw == PHY_CHNL_BW_80)
            *pos++ = 4;
        else if ((rxvect->ch_bw == PHY_CHNL_BW_160)
                && phy_info->phy_center2_freq)
            *pos++ = 0; //80P80
        else if  (rxvect->ch_bw == PHY_CHNL_BW_160)
            *pos++ = 11;
        else // 20 MHz
            *pos++ = 0;

        // MCS/NSS
        *pos++ = (rate_idx << 4) | vht_nss;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
        if (fec_coding)
            *pos |= IEEE80211_RADIOTAP_CODING_LDPC_USER0;
        pos++;
        // group ID
        pos++;
        // partial_aid
        pos += 2;
    }

    // Check for HE frames
    if (rxvect->format_mod >= FORMATMOD_HE_SU) {
        struct ieee80211_radiotap_he he = {0};
        #define HE_PREP(f, val) cpu_to_le16(FIELD_PREP(IEEE80211_RADIOTAP_HE_##f, val))
        #define D1_KNOWN(f) cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_##f##_KNOWN)
        #define D2_KNOWN(f) cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_##f##_KNOWN)

        he.data1 = D1_KNOWN(BSS_COLOR) | D1_KNOWN(BEAM_CHANGE) |
                   D1_KNOWN(UL_DL) | D1_KNOWN(STBC) |
                   D1_KNOWN(DOPPLER) | D1_KNOWN(DATA_DCM);
        he.data2 = D2_KNOWN(GI) | D2_KNOWN(TXBF) | D2_KNOWN(TXOP);

        he.data3 |= HE_PREP(DATA3_BSS_COLOR, rxvect->he.bss_color);
        he.data3 |= HE_PREP(DATA3_BEAM_CHANGE, rxvect->he.beam_change);
        he.data3 |= HE_PREP(DATA3_UL_DL, rxvect->he.uplink_flag);
        he.data3 |= HE_PREP(DATA3_BSS_COLOR, rxvect->he.bss_color);
        he.data3 |= HE_PREP(DATA3_DATA_DCM, rxvect->he.dcm);

        he.data5 |= HE_PREP(DATA5_GI, rxvect->he.gi_type);
        he.data5 |= HE_PREP(DATA5_TXBF, rxvect->he.beamformed);
        he.data5 |= HE_PREP(DATA5_LTF_SIZE, rxvect->he.he_ltf_type + 1);

        he.data6 |= HE_PREP(DATA6_DOPPLER, rxvect->he.doppler);
        he.data6 |= HE_PREP(DATA6_TXOP, rxvect->he.txop_duration);

        if (rxvect->format_mod != FORMATMOD_HE_TB) {
            he.data1 |= (D1_KNOWN(DATA_MCS) | D1_KNOWN(CODING) |
                         D1_KNOWN(SPTL_REUSE) | D1_KNOWN(BW_RU_ALLOC));

            if (stbc) {
                he.data6 |= HE_PREP(DATA6_NSTS, 2);
                he.data3 |= HE_PREP(DATA3_STBC, 1);
            } else {
                he.data6 |= HE_PREP(DATA6_NSTS, rxvect->he.nss);
            }

            he.data3 |= HE_PREP(DATA3_DATA_MCS, rxvect->he.mcs);
            he.data3 |= HE_PREP(DATA3_CODING, rxvect->he.fec);

            he.data4 = HE_PREP(DATA4_SU_MU_SPTL_REUSE, rxvect->he.spatial_reuse);

            if (rxvect->format_mod == FORMATMOD_HE_MU) {
                he.data1 |= IEEE80211_RADIOTAP_HE_DATA1_FORMAT_MU;
                he.data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                                    rxvect->he.ru_size +
                                    IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_26T);
            } else {
                if (rxvect->format_mod == FORMATMOD_HE_SU)
                    he.data1 |= IEEE80211_RADIOTAP_HE_DATA1_FORMAT_SU;
                else
                    he.data1 |= IEEE80211_RADIOTAP_HE_DATA1_FORMAT_EXT_SU;

                switch (rxvect->ch_bw) {
                    case PHY_CHNL_BW_20:
                        he.data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                                            IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_20MHZ);
                        break;
                    case PHY_CHNL_BW_40:
                        he.data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                                            IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_40MHZ);
                        break;
                    case PHY_CHNL_BW_80:
                        he.data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                                            IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_80MHZ);
                        break;
                    case PHY_CHNL_BW_160:
                        he.data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                                            IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_160MHZ);
                        break;
                    default:
                        WARN_ONCE(1, "Invalid SU BW %d\n", rxvect->ch_bw);
                }
            }
        } else {
            he.data1 |= IEEE80211_RADIOTAP_HE_DATA1_FORMAT_TRIG;
        }

        /* ensure 2 bytes alignment */
        while ((pos - (u8 *)rtap) & 1)
            pos++;
        rtap->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_HE);
        memcpy(pos, &he, sizeof(he));
        pos += sizeof(he);
    }

    // Rx Chains
    if (hweight32(rxvect->antenna_set) > 1) {
        int chain;
        unsigned long chains = rxvect->antenna_set;
        u8 rssis[4] = {rxvect->rssi1, rxvect->rssi1, rxvect->rssi1, rxvect->rssi1};

        for_each_set_bit(chain, &chains, IEEE80211_MAX_CHAINS) {
            *pos++ = rssis[chain];
            *pos++ = chain;
        }
    }
}

/**
 * rwnx_rx_monitor - Build radiotap header for skb an send it to netdev
 *
 * @rwnx_hw: main driver data
 * @rwnx_vif: vif that received the buffer
 * @skb: sk_buff received
 * @hw_rxhdr_ptr: Pointer to HW RX header
 * @rtap_len: Radiotap Header length
 *
 * Add radiotap header to the receved skb and send it to netdev
 */
static int rwnx_rx_monitor(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                           struct sk_buff *skb,  struct hw_rxhdr *hw_rxhdr_ptr,
                           u8 rtap_len)
{
    skb->dev = rwnx_vif->ndev;

    if (rwnx_vif->wdev.iftype != NL80211_IFTYPE_MONITOR) {
        netdev_err(rwnx_vif->ndev, "not a monitor vif\n");
        return -1;
    }

    /* Add RadioTap Header */
    rwnx_rx_add_rtap_hdr(rwnx_hw, skb, &hw_rxhdr_ptr->hwvect.rx_vect1,
                         &hw_rxhdr_ptr->phy_info, &hw_rxhdr_ptr->hwvect,
                         rtap_len, 0, 0);

    skb_reset_mac_header(skb);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);

    netif_receive_skb(skb);

    return 0;
}

/**
 * rwnx_unsup_rx_vec_ind() - IRQ handler callback for %IPC_IRQ_E2A_UNSUP_RX_VEC
 *
 * FMAC has triggered an IT saying that a rx vector of an unsupported frame has been
 * captured and sent to upper layer.
 * If no monitor interace is active ignore it, otherwise add a radiotap header with a
 * vendor specific header and upload it on the monitor interface.
 *
 * @pthis: Pointer to main driver data
 * @arg: Pointer to IPC buffer
 */
u8 rwnx_unsup_rx_vec_ind(void *pthis, void *arg) {
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_buf *ipc_buf = arg;
    struct rx_vector_desc *rx_desc;
    struct sk_buff *skb;
    struct rx_vector_1 *rx_vect1;
    struct phy_channel_info_desc *phy_info;
    struct vendor_radiotap_hdr *rtap;
    u16 ht_length;
    struct rwnx_vif *rwnx_vif;
    struct rx_vector_desc rx_vect_desc;
    u8 rtap_len, vend_rtap_len = sizeof(*rtap);

    rwnx_ipc_buf_e2a_sync(rwnx_hw, ipc_buf, sizeof(struct rx_vector_desc));

    skb = ipc_buf->addr;
    if (((struct rx_vector_desc *)(skb->data))->pattern == 0) {
        rwnx_ipc_buf_e2a_sync_back(rwnx_hw, ipc_buf, sizeof(struct rx_vector_desc));
        return -1;
    }

    if (rwnx_hw->monitor_vif == RWNX_INVALID_VIF) {
        rwnx_ipc_unsuprxvec_repush(rwnx_hw, ipc_buf);
        return -1;
    }

    rwnx_vif = rwnx_hw->vif_table[rwnx_hw->monitor_vif];
    skb->dev = rwnx_vif->ndev;
    memcpy(&rx_vect_desc, skb->data, sizeof(rx_vect_desc));
    rx_desc = &rx_vect_desc;

    rx_vect1 = (struct rx_vector_1 *) (rx_desc->rx_vect1);
    rwnx_rx_vector_convert(rwnx_hw->machw_type, rx_vect1, NULL);
    phy_info = (struct phy_channel_info_desc *) (&rx_desc->phy_info);
    if (rx_vect1->format_mod >= FORMATMOD_VHT)
        ht_length = 0;
    else
        ht_length = (u16) le32_to_cpu(rx_vect1->ht.length);

    // Reserve space for radiotap
    skb_reserve(skb, RADIOTAP_HDR_MAX_LEN);

    /* Fill vendor specific header with fake values */
    rtap = (struct vendor_radiotap_hdr *) skb->data;
    rtap->oui[0] = 0x00;
    rtap->oui[1] = 0x25;
    rtap->oui[2] = 0x3A;
    rtap->subns  = 0;
    rtap->len = sizeof(ht_length);
    put_unaligned_le16(ht_length, rtap->data);
    vend_rtap_len += rtap->len;
    skb_put(skb, vend_rtap_len);

    /* Copy fake data */
    put_unaligned_le16(0, skb->data + vend_rtap_len);
    skb_put(skb, UNSUP_RX_VEC_DATA_LEN);

    /* Get RadioTap Header length */
    rtap_len = rwnx_rx_rtap_hdrlen(rx_vect1, true);

    /* Check headroom space */
    if (skb_headroom(skb) < rtap_len) {
        netdev_err(rwnx_vif->ndev, "not enough headroom %d need %d\n",
                   skb_headroom(skb), rtap_len);
        rwnx_ipc_unsuprxvec_repush(rwnx_hw, ipc_buf);
        return -1;
    }

    /* Add RadioTap Header */
    rwnx_rx_add_rtap_hdr(rwnx_hw, skb, rx_vect1, phy_info, NULL,
                         rtap_len, vend_rtap_len, BIT(0));

    skb_reset_mac_header(skb);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);

    rwnx_ipc_buf_e2a_release(rwnx_hw, ipc_buf);
    netif_receive_skb(skb);

    /* Allocate and push a new buffer to fw to replace this one */
    rwnx_ipc_unsuprxvec_alloc(rwnx_hw, ipc_buf);
    return 0;
}

/**
 * rwnx_rxdataind - Process rx buffer
 *
 * @pthis: Pointer to the object attached to the IPC structure
 *         (points to struct rwnx_hw is this case)
 * @arg: Address of the RX descriptor
 *
 * This function is called for each buffer received by the fw
 *
 */
struct list_head rxdata_list;
struct list_head free_rxdata_list;

void rwnx_rxdata_init(void)
{
    int i = 0;
    struct rxdata *rxdata = NULL;

    INIT_LIST_HEAD(&rxdata_list);
    INIT_LIST_HEAD(&free_rxdata_list);

    for (i = 0; i < IPC_RXDESC_CNT; i++) {
        rxdata = kmalloc(sizeof(struct rxdata), GFP_ATOMIC);
        if (!rxdata) {
            ASSERT_ERR(0);
            return;
        }
        list_add_tail(&rxdata->list, &free_rxdata_list);
    }
}

void rwnx_rxdata_deinit(void)
{
    struct rxdata *rxdata = NULL;
    struct rxdata *rxdata_tmp = NULL;

    list_for_each_entry_safe(rxdata, rxdata_tmp, &free_rxdata_list, list)
        kfree(rxdata);
}

struct rxdata *rwnx_get_rxdata_from_free_list(void)
{
    struct rxdata *rxdata = NULL;

    if (!list_empty(&free_rxdata_list)) {
        rxdata = list_first_entry(&free_rxdata_list, struct rxdata, list);
        list_del(&rxdata->list);
    } else {
        ASSERT_ERR(0);
    }

    return rxdata;
}

void rwnx_put_rxdata_back_to_free_list(struct rxdata *rxdata)
{
    list_add_tail(&rxdata->list, &free_rxdata_list);
}

s8 rwnx_sdio_rxdataind(void *pthis, void *arg)
{
    struct rwnx_hw *rwnx_hw = pthis;
    struct hw_rxhdr *hw_rxhdr;
    struct rwnx_vif *rwnx_vif;
    struct sk_buff *skb = NULL;
    int msdu_offset = 0;
    u16_l status;
    size_t sync_len;
    uint8_t eth_hdr_offset = 0;
    struct rxdata *rxdata = NULL;
    struct rxdata *rxdata_tmp = NULL;
    u8 i = 0;
    u8 result = 0;
    u8 rxdesc_read_cnt = 0;
    static u8 rxdesc_cur_idx = 0;
    u8 rxdesc_start_idx = 0;
    u8 rxdesc_handle_cnt = 0;
    struct drv_stat_desc rx_stat_desc[RXDESC_CNT_READ_ONCE] = {0};
    struct drv_stat_desc *rx_stat_desc_addr = (struct drv_stat_desc *)RXU_STAT_DESC_POOL;
    uint32_t upload_len = 0;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_RWNXDATAIND);

    if (rxdesc_cur_idx <= (IPC_RXDESC_CNT - RXDESC_CNT_READ_ONCE)) {
        rxdesc_read_cnt = RXDESC_CNT_READ_ONCE;
    } else {
        rxdesc_read_cnt = IPC_RXDESC_CNT - rxdesc_cur_idx;
    }

    rxdesc_start_idx = rxdesc_cur_idx;
    if (aml_bus_type == USB_MODE) {
        rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)rx_stat_desc,
            (unsigned char *)(unsigned long)&rx_stat_desc_addr[rxdesc_cur_idx], sizeof(struct drv_stat_desc) * rxdesc_read_cnt, USB_EP4);

    } else if (aml_bus_type == SDIO_MODE) {
        rwnx_hw->plat->hif_sdio_ops->hi_desc_read((unsigned char *)rx_stat_desc,
            (unsigned char *)(unsigned long)&rx_stat_desc_addr[rxdesc_cur_idx], sizeof(struct drv_stat_desc) * rxdesc_read_cnt);
    }

    for (i = 0; i < rxdesc_read_cnt; i++, rxdesc_cur_idx = (rxdesc_cur_idx + 1) % IPC_RXDESC_CNT) {
        status = rx_stat_desc[i].rx_stat_val.status;
        rx_stat_desc[i].rx_stat_val.status = 0;

        if (!status) {
            result = -1;
            break;
        }

        rxdesc_handle_cnt++;
        /* Check if we need to delete the buffer */
        if (status & RX_STAT_DELETE) {
            list_for_each_entry_safe(rxdata, rxdata_tmp, &rxdata_list, list) {
                if (rxdata->host_id == rx_stat_desc[i].rx_stat_val.host_id) {
                    list_del(&rxdata->list);
                    dev_kfree_skb(rxdata->skb);
                    rwnx_put_rxdata_back_to_free_list(rxdata);
                    break;
                }
            }
            continue;
        }

        eth_hdr_offset = rx_stat_desc[i].rx_stat_val.frame_len & 0xff;
        rx_stat_desc[i].rx_stat_val.frame_len >>= 8;
        msdu_offset = RX_HEADER_TO_PAYLOAD_LEN + eth_hdr_offset;

        if (status == RX_STAT_FORWARD) {
            list_for_each_entry_safe(rxdata, rxdata_tmp, &rxdata_list, list) {
                if (rxdata->host_id == rx_stat_desc[i].rx_stat_val.host_id) {
                    list_del(&rxdata->list);
                    rx_stat_desc[i].rx_stat_val.frame_len = rxdata->frame_len;
                    skb = rxdata->skb;
                    rwnx_put_rxdata_back_to_free_list(rxdata);
                    break;
                }
            }

        } else {
            skb = dev_alloc_skb(rx_stat_desc[i].rx_stat_val.frame_len + msdu_offset);

            if (skb == NULL) {
                printk("%s,%d, skb == NULL\n", __func__, __LINE__);
                return -ENOMEM;
            }

            printk("%s rx_stat_val.host_id:%08x, status:%d\n", __func__, rx_stat_desc[i].rx_stat_val.host_id, rx_stat_desc[i].rx_stat_val.status);
            if (rx_stat_desc[i].rx_stat_val.host_id + RX_HEADER_OFFSET + msdu_offset + rx_stat_desc[i].rx_stat_val.frame_len > RXBUF_END_ADDR) {
                upload_len = RXBUF_END_ADDR - rx_stat_desc[i].rx_stat_val.host_id - RX_HEADER_OFFSET + 1;
                if (aml_bus_type == USB_MODE) {
                    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)skb->data, (unsigned char *)(unsigned long)rx_stat_desc[i].rx_stat_val.host_id + RX_HEADER_OFFSET, upload_len, USB_EP4);
                    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)skb->data + upload_len, (unsigned char *)RXBUF_START_ADDR, msdu_offset + rx_stat_desc[i].rx_stat_val.frame_len - upload_len, USB_EP4);
                } else if (aml_bus_type == SDIO_MODE) {
                    rwnx_hw->plat->hif_sdio_ops->hi_rx_buffer_read((unsigned char *)skb->data, (unsigned char *)(unsigned long)rx_stat_desc[i].rx_stat_val.host_id + RX_HEADER_OFFSET, upload_len);
                    rwnx_hw->plat->hif_sdio_ops->hi_rx_buffer_read((unsigned char *)skb->data + upload_len, (unsigned char *)RXBUF_START_ADDR, msdu_offset + rx_stat_desc[i].rx_stat_val.frame_len - upload_len);
                }
            } else {
                if (aml_bus_type == USB_MODE) {
                    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)skb->data, (unsigned char *)(unsigned long)rx_stat_desc[i].rx_stat_val.host_id + RX_HEADER_OFFSET, msdu_offset + rx_stat_desc[i].rx_stat_val.frame_len, USB_EP4);
                } else if (aml_bus_type == SDIO_MODE) {
                    rwnx_hw->plat->hif_sdio_ops->hi_rx_buffer_read((unsigned char *)skb->data, (unsigned char *)(unsigned long)rx_stat_desc[i].rx_stat_val.host_id + RX_HEADER_OFFSET, msdu_offset + rx_stat_desc[i].rx_stat_val.frame_len);
                }
            }
        }

        if (status == RX_STAT_ALLOC) {
            rxdata = rwnx_get_rxdata_from_free_list();
            rxdata->host_id = rx_stat_desc[i].rx_stat_val.host_id;
            rxdata->frame_len = rx_stat_desc[i].rx_stat_val.frame_len;
            rxdata->skb = skb;
            list_add_tail(&rxdata->list, &rxdata_list);
            continue;
        }

        spin_lock_bh(&rwnx_hw->rx_lock);
        hw_rxhdr = (struct hw_rxhdr *)skb->data;

        /* Check if we need to forward the buffer coming from a monitor interface */
        if (status & RX_STAT_MONITOR) {
            struct sk_buff *skb_monitor;
            struct hw_rxhdr hw_rxhdr_copy;
            u8 rtap_len;
            u16 frm_len;

            //Check if monitor interface exists and is open
            rwnx_vif = rwnx_rx_get_vif(rwnx_hw, rwnx_hw->monitor_vif);
            if (!rwnx_vif) {
                dev_err(rwnx_hw->dev, "Received monitor frame but there is no monitor interface open\n");
                goto check_len_update;
            }

            hw_rxhdr = (struct hw_rxhdr *)skb->data;
            rwnx_rx_vector_convert(rwnx_hw->machw_type,
                                   &hw_rxhdr->hwvect.rx_vect1,
                                   &hw_rxhdr->hwvect.rx_vect2);
            rtap_len = rwnx_rx_rtap_hdrlen(&hw_rxhdr->hwvect.rx_vect1, false);

            // Move skb->data pointer to MAC Header or Ethernet header
            skb->data += msdu_offset;

            //Save frame length
            frm_len = le32_to_cpu(hw_rxhdr->hwvect.len);

            // Reserve space for frame
            skb->len = frm_len;

            if (status == RX_STAT_MONITOR) {
                //rwnx_ipc_buf_e2a_release(rwnx_hw, ipc_buf);

                //Check if there is enough space to add the radiotap header
                if (skb_headroom(skb) > rtap_len) {

                    skb_monitor = skb;

                    //Duplicate the HW Rx Header to override with the radiotap header
                    memcpy(&hw_rxhdr_copy, hw_rxhdr, sizeof(hw_rxhdr_copy));

                    hw_rxhdr = &hw_rxhdr_copy;
                } else {
                    //Duplicate the skb and extend the headroom
                    skb_monitor = skb_copy_expand(skb, rtap_len, 0, GFP_ATOMIC);

                    //Reset original skb->data pointer
                    skb->data = (void*) hw_rxhdr;
                }
            }
            else
            {
            #ifdef CONFIG_RWNX_MON_DATA
                // Check if MSDU
                if (!hw_rxhdr->flags_is_80211_mpdu) {
                    // MSDU
                    //Extract MAC header
                    u16 machdr_len = hw_rxhdr->mac_hdr_backup.buf_len;
                    u8* machdr_ptr = hw_rxhdr->mac_hdr_backup.buffer;

                    //Pull Ethernet header from skb
                    skb_pull(skb, sizeof(struct ethhdr));

                    // Copy skb and extend for adding the radiotap header and the MAC header
                    skb_monitor = skb_copy_expand(skb, rtap_len + machdr_len, 0, GFP_ATOMIC);

                    //Reserve space for the MAC Header
                    skb_push(skb_monitor, machdr_len);

                    //Copy MAC Header
                    memcpy(skb_monitor->data, machdr_ptr, machdr_len);

                    //Update frame length
                    frm_len += machdr_len - sizeof(struct ethhdr);
                } else {
                    // MPDU
                    skb_monitor = skb_copy_expand(skb, rtap_len, 0, GFP_ATOMIC);
                }

               //Reset original skb->data pointer
                skb->data = (void*) hw_rxhdr;
            #else
                //Reset original skb->data pointer
                skb->data = (void*) hw_rxhdr;

                wiphy_err(rwnx_hw->wiphy, "RX status %d is invalid when MON_DATA is disabled\n", status);
                goto check_len_update;
            #endif
            }

            skb_reset_tail_pointer(skb);
            skb->len = 0;
            if (skb_monitor != NULL) {
                skb_reset_tail_pointer(skb_monitor);
                skb_monitor->len = 0;

                skb_put(skb_monitor, frm_len);
                /* coverity[remediation] -  hwvect won't cross the line*/
                if (rwnx_rx_monitor(rwnx_hw, rwnx_vif, skb_monitor, hw_rxhdr, rtap_len))
                    dev_kfree_skb(skb_monitor);
            }

            if (status == RX_STAT_MONITOR) {
                status |= RX_STAT_ALLOC;
                if (skb_monitor != skb) {
                    dev_kfree_skb(skb);
                }
            }
        }

check_len_update:
        /* Check if we need to update the length */
        if (status & RX_STAT_LEN_UPDATE) {
            sync_len = msdu_offset + sizeof(struct ethhdr);

            hw_rxhdr = (struct hw_rxhdr *)skb->data;
            hw_rxhdr->hwvect.len = rx_stat_desc[i].rx_stat_val.frame_len;

            if (status & RX_STAT_ETH_LEN_UPDATE) {
                /* Update Length Field inside the Ethernet Header */
                struct ethhdr *hdr = (struct ethhdr *)((u8 *)hw_rxhdr + msdu_offset);
                hdr->h_proto = htons(rx_stat_desc[i].rx_stat_val.frame_len - sizeof(struct ethhdr));
            }

            dev_kfree_skb(skb);
            spin_unlock_bh(&rwnx_hw->rx_lock);
            continue;
        }

        /* Check if it must be discarded after informing upper layer */
        if (status & RX_STAT_SPURIOUS) {
            struct ieee80211_hdr *hdr;
            sync_len =  msdu_offset + sizeof(*hdr);

            /* Read mac header to obtain Transmitter Address */
            //rwnx_ipc_buf_e2a_sync(rwnx_hw, ipc_buf, sync_len);

            hw_rxhdr = (struct hw_rxhdr *)skb->data;
            hdr = (struct ieee80211_hdr *)(skb->data + msdu_offset);
            rwnx_vif = rwnx_rx_get_vif(rwnx_hw, hw_rxhdr->flags_vif_idx);
            if (rwnx_vif) {
                cfg80211_rx_spurious_frame(rwnx_vif->ndev, hdr->addr2, GFP_ATOMIC);
            }

            dev_kfree_skb(skb);
            spin_unlock_bh(&rwnx_hw->rx_lock);
            continue;
        }

        /* Check if we need to forward the buffer */
        if (status & RX_STAT_FORWARD) {
            struct rwnx_sta *sta = NULL;

            hw_rxhdr = (struct hw_rxhdr *)skb->data;
            rwnx_rx_vector_convert(rwnx_hw->machw_type,
                               &hw_rxhdr->hwvect.rx_vect1,
                               &hw_rxhdr->hwvect.rx_vect2);

            skb_reserve(skb, msdu_offset);
            hw_rxhdr->hwvect.len = rx_stat_desc[i].rx_stat_val.frame_len;
            skb_put(skb, le32_to_cpu(hw_rxhdr->hwvect.len));

            printk("%s:%08x %08x %08x %08x\n", __func__, *(unsigned int *)(skb->data), *(unsigned int *)(skb->data + 4),
                *(unsigned int *)(skb->data + 8), *(unsigned int *)(skb->data + 12));

            if (hw_rxhdr->flags_sta_idx != RWNX_INVALID_STA &&
                hw_rxhdr->flags_sta_idx < (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) {
                sta = &rwnx_hw->sta_table[hw_rxhdr->flags_sta_idx];
                /* coverity[remediation] -  hwvect won't cross the line*/
                rwnx_rx_statistic(rwnx_hw, hw_rxhdr, sta);
            }

            if (hw_rxhdr->flags_is_80211_mpdu) {
                rwnx_rx_mgmt_any(rwnx_hw, skb, hw_rxhdr);
            } else {
                rwnx_vif = rwnx_rx_get_vif(rwnx_hw, hw_rxhdr->flags_vif_idx);

                if (!rwnx_vif) {
                    dev_err(rwnx_hw->dev, "Frame received but no active vif (%d)",
                            hw_rxhdr->flags_vif_idx);
                    dev_kfree_skb(skb);
                    spin_unlock_bh(&rwnx_hw->rx_lock);
                    continue;
                }

                if (sta) {
                    if (sta->vlan_idx != rwnx_vif->vif_index) {
                        rwnx_vif = rwnx_hw->vif_table[sta->vlan_idx];
                        if (!rwnx_vif) {
                            dev_kfree_skb(skb);
                            spin_unlock_bh(&rwnx_hw->rx_lock);
                            continue;
                        }
                    }

                    if (hw_rxhdr->flags_is_4addr && !rwnx_vif->use_4addr) {
                        cfg80211_rx_unexpected_4addr_frame(rwnx_vif->ndev,
                                                           sta->mac_addr, GFP_ATOMIC);
                    }
                }

                skb->priority = 256 + hw_rxhdr->flags_user_prio;
                /* coverity[remediation] -  hwvect won't cross the line*/
                if (!rwnx_rx_data_skb(rwnx_hw, rwnx_vif, skb, hw_rxhdr))
                    dev_kfree_skb(skb);
            }
        }

        spin_unlock_bh(&rwnx_hw->rx_lock);
    }

    printk("rxdesc_cur_idx:%x, rxdesc_handle_cnt:%x, rxdesc_start_idx:%x, rxdesc_read_cnt=%x, addr:%08x\n",
        rxdesc_cur_idx, rxdesc_handle_cnt, rxdesc_start_idx, rxdesc_read_cnt, &rx_stat_desc_addr[rxdesc_start_idx]);
 #if 0
    if (rxdesc_handle_cnt != 0) {
        if (aml_bus_type == USB_MODE) {
            rwnx_hw->plat->hif_ops->hi_write_sram((unsigned char *)rx_stat_desc,
                (unsigned char *)(unsigned long)&rx_stat_desc_addr[rxdesc_start_idx], sizeof(struct drv_stat_desc) * rxdesc_handle_cnt, USB_EP4);

        } else if (aml_bus_type == SDIO_MODE) {
            rwnx_hw->plat->hif_sdio_ops->hi_write_ipc_sram((unsigned char *)rx_stat_desc,
                (unsigned char *)(unsigned long)&rx_stat_desc_addr[rxdesc_start_idx], sizeof(struct drv_stat_desc) * rxdesc_handle_cnt);
        }
    }
#endif


    if (aml_bus_type == USB_MODE) {
        rwnx_hw->plat->hif_ops->hi_write_word((unsigned int)APP2EMB_RXDESC_IDX, rxdesc_cur_idx, USB_EP4);
    } else if (aml_bus_type == SDIO_MODE) {
        rwnx_hw->plat->hif_sdio_ops->hi_random_word_write((unsigned int)APP2EMB_RXDESC_IDX, rxdesc_cur_idx);
    }
    ipc_app2emb_trigger_set(rwnx_hw, IPC_IRQ_A2E_RXDESC_BACK);

    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_RWNXDATAIND);

    if (rxdesc_handle_cnt == rxdesc_read_cnt) {
        result = 1;
    }

    return result;
}

/**
 * rwnx_rxdataind - Process rx buffer
 *
 * @pthis: Pointer to the object attached to the IPC structure
 *         (points to struct rwnx_hw is this case)
 * @arg: Address of the RX descriptor
 *
 * This function is called for each buffer received by the fw
 *
 */
u8 rwnx_pci_rxdataind(void *pthis, void *arg)
{
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_buf *ipc_desc = arg;
    struct rwnx_ipc_buf *ipc_buf;
    struct hw_rxhdr *hw_rxhdr;
    struct rxdesc_tag *rxdesc;
    struct rwnx_vif *rwnx_vif;
    struct sk_buff *skb;
    int msdu_offset = sizeof(struct hw_rxhdr) + 2;
    u16_l status;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_RWNXDATAIND);

    rwnx_ipc_buf_e2a_sync(rwnx_hw, ipc_desc, sizeof(struct rxdesc_tag));

    rxdesc = ipc_desc->addr;
    status = rxdesc->status;

    if (!status){
        /* frame is not completely uploaded, give back ownership of the descriptor */
        rwnx_ipc_buf_e2a_sync_back(rwnx_hw, ipc_desc, sizeof(struct rxdesc_tag));
        return -1;
    }

    ipc_buf = rwnx_ipc_rxbuf_from_hostid(rwnx_hw, rxdesc->host_id);
    if (!ipc_buf) {
        goto check_alloc;
    }
    skb = ipc_buf->addr;

    /* Check if we need to delete the buffer */
    if (status & RX_STAT_DELETE) {
        rwnx_ipc_rxbuf_dealloc(rwnx_hw, ipc_buf);
        goto check_alloc;
    }

    /* Check if we need to forward the buffer coming from a monitor interface */
    if (status & RX_STAT_MONITOR) {
        struct sk_buff *skb_monitor;
        struct hw_rxhdr hw_rxhdr_copy;
        u8 rtap_len;
        u16 frm_len;

        //Check if monitor interface exists and is open
        rwnx_vif = rwnx_rx_get_vif(rwnx_hw, rwnx_hw->monitor_vif);
        if (!rwnx_vif) {
            dev_err(rwnx_hw->dev, "Received monitor frame but there is no monitor interface open\n");
            goto check_len_update;
        }

        hw_rxhdr = (struct hw_rxhdr *)skb->data;
        rwnx_rx_vector_convert(rwnx_hw->machw_type,
                               &hw_rxhdr->hwvect.rx_vect1,
                               &hw_rxhdr->hwvect.rx_vect2);
        rtap_len = rwnx_rx_rtap_hdrlen(&hw_rxhdr->hwvect.rx_vect1, false);

        // Move skb->data pointer to MAC Header or Ethernet header
        skb->data += msdu_offset;

        //Save frame length
        frm_len = le32_to_cpu(hw_rxhdr->hwvect.len);

        // Reserve space for frame
        skb->len = frm_len;

        if (status == RX_STAT_MONITOR) {
            rwnx_ipc_buf_e2a_release(rwnx_hw, ipc_buf);

            //Check if there is enough space to add the radiotap header
            if (skb_headroom(skb) > rtap_len) {

                skb_monitor = skb;

                //Duplicate the HW Rx Header to override with the radiotap header
                memcpy(&hw_rxhdr_copy, hw_rxhdr, sizeof(hw_rxhdr_copy));

                hw_rxhdr = &hw_rxhdr_copy;
            } else {
                //Duplicate the skb and extend the headroom
                skb_monitor = skb_copy_expand(skb, rtap_len, 0, GFP_ATOMIC);

                //Reset original skb->data pointer
                skb->data = (void*) hw_rxhdr;
            }
        }
        else
        {
        #ifdef CONFIG_RWNX_MON_DATA
            // Check if MSDU
            if (!hw_rxhdr->flags_is_80211_mpdu) {
                // MSDU
                //Extract MAC header
                u16 machdr_len = hw_rxhdr->mac_hdr_backup.buf_len;
                u8* machdr_ptr = hw_rxhdr->mac_hdr_backup.buffer;

                //Pull Ethernet header from skb
                skb_pull(skb, sizeof(struct ethhdr));

                // Copy skb and extend for adding the radiotap header and the MAC header
                skb_monitor = skb_copy_expand(skb, rtap_len + machdr_len, 0, GFP_ATOMIC);

                //Reserve space for the MAC Header
                skb_push(skb_monitor, machdr_len);

                //Copy MAC Header
                memcpy(skb_monitor->data, machdr_ptr, machdr_len);

                //Update frame length
                frm_len += machdr_len - sizeof(struct ethhdr);
            } else {
                // MPDU
                skb_monitor = skb_copy_expand(skb, rtap_len, 0, GFP_ATOMIC);
            }

            //Reset original skb->data pointer
            skb->data = (void*) hw_rxhdr;
        #else
            //Reset original skb->data pointer
            skb->data = (void*) hw_rxhdr;

            wiphy_err(rwnx_hw->wiphy, "RX status %d is invalid when MON_DATA is disabled\n", status);
            goto check_len_update;
        #endif
        }

        skb_reset_tail_pointer(skb);
        skb->len = 0;
        if (skb_monitor != NULL) {
            skb_reset_tail_pointer(skb_monitor);
            skb_monitor->len = 0;

            skb_put(skb_monitor, frm_len);
            /* coverity[remediation] -  hwvect won't cross the line*/
            if (rwnx_rx_monitor(rwnx_hw, rwnx_vif, skb_monitor, hw_rxhdr, rtap_len))
                dev_kfree_skb(skb_monitor);
        }

        if (status == RX_STAT_MONITOR) {
            status |= RX_STAT_ALLOC;
            if (skb_monitor != skb) {
                dev_kfree_skb(skb);
            }
        }
    }

check_len_update:
    /* Check if we need to update the length */
    if (status & RX_STAT_LEN_UPDATE) {
        int sync_len = msdu_offset + sizeof(struct ethhdr);

        rwnx_ipc_buf_e2a_sync(rwnx_hw, ipc_buf, sync_len);

        hw_rxhdr = (struct hw_rxhdr *)skb->data;
        hw_rxhdr->hwvect.len = rxdesc->frame_len;

        if (status & RX_STAT_ETH_LEN_UPDATE) {
            /* Update Length Field inside the Ethernet Header */
            struct ethhdr *hdr = (struct ethhdr *)((u8 *)hw_rxhdr + msdu_offset);
            hdr->h_proto = htons(rxdesc->frame_len - sizeof(struct ethhdr));
        }

        rwnx_ipc_buf_e2a_sync_back(rwnx_hw, ipc_buf, sync_len);
        goto end;
    }

    /* Check if it must be discarded after informing upper layer */
    if (status & RX_STAT_SPURIOUS) {
        struct ieee80211_hdr *hdr;
        size_t sync_len =  msdu_offset + sizeof(*hdr);

        /* Read mac header to obtain Transmitter Address */
        rwnx_ipc_buf_e2a_sync(rwnx_hw, ipc_buf, sync_len);

        hw_rxhdr = (struct hw_rxhdr *)skb->data;
        hdr = (struct ieee80211_hdr *)(skb->data + msdu_offset);
        rwnx_vif = rwnx_rx_get_vif(rwnx_hw, hw_rxhdr->flags_vif_idx);
        if (rwnx_vif) {
            cfg80211_rx_spurious_frame(rwnx_vif->ndev, hdr->addr2, GFP_ATOMIC);
        }
        rwnx_ipc_buf_e2a_sync_back(rwnx_hw, ipc_buf, sync_len);
        rwnx_ipc_rxbuf_repush(rwnx_hw, ipc_buf);
        goto end;
    }

    /* Check if we need to forward the buffer */
    if (status & RX_STAT_FORWARD) {
        struct rwnx_sta *sta = NULL;

        rwnx_ipc_buf_e2a_release(rwnx_hw, ipc_buf);

        hw_rxhdr = (struct hw_rxhdr *)skb->data;
        rwnx_rx_vector_convert(rwnx_hw->machw_type,
                               &hw_rxhdr->hwvect.rx_vect1,
                               &hw_rxhdr->hwvect.rx_vect2);
        skb_reserve(skb, msdu_offset);
        skb_put(skb, le32_to_cpu(hw_rxhdr->hwvect.len));

        if (hw_rxhdr->flags_sta_idx != RWNX_INVALID_STA &&
            hw_rxhdr->flags_sta_idx < (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) {
            sta = &rwnx_hw->sta_table[hw_rxhdr->flags_sta_idx];
            /* coverity[remediation] -  hwvect won't cross the line*/
            rwnx_rx_statistic(rwnx_hw, hw_rxhdr, sta);
        }

        if (hw_rxhdr->flags_is_80211_mpdu) {
            rwnx_rx_mgmt_any(rwnx_hw, skb, hw_rxhdr);
        } else {
            rwnx_vif = rwnx_rx_get_vif(rwnx_hw, hw_rxhdr->flags_vif_idx);

            if (!rwnx_vif) {
                dev_err(rwnx_hw->dev, "Frame received but no active vif (%d)",
                        hw_rxhdr->flags_vif_idx);
                dev_kfree_skb(skb);
                goto check_alloc;
            }

            if (sta) {
                if (sta->vlan_idx != rwnx_vif->vif_index) {
                    rwnx_vif = rwnx_hw->vif_table[sta->vlan_idx];
                    if (!rwnx_vif) {
                        dev_kfree_skb(skb);
                        goto check_alloc;
                    }
                }

                if (hw_rxhdr->flags_is_4addr && !rwnx_vif->use_4addr) {
                    cfg80211_rx_unexpected_4addr_frame(rwnx_vif->ndev,
                                                       sta->mac_addr, GFP_ATOMIC);
                }
            }

            skb->priority = 256 + hw_rxhdr->flags_user_prio;
            /* coverity[remediation] -  hwvect won't cross the line*/
            if (!rwnx_rx_data_skb(rwnx_hw, rwnx_vif, skb, hw_rxhdr))
                dev_kfree_skb(skb);
        }
    }

check_alloc:
    /* Check if we need to allocate a new buffer */
    if (status & RX_STAT_ALLOC)
        rwnx_ipc_rxbuf_alloc(rwnx_hw);

end:
    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_RWNXDATAIND);

    /* Reset and repush descriptor to FW */
    rwnx_ipc_rxdesc_repush(rwnx_hw, ipc_desc);

    return 0;
}

u8 rwnx_rxdataind(void *pthis, void *arg)
{
    s8 ret;

    if (aml_bus_type != PCIE_MODE) {
        do {
            ret = rwnx_sdio_rxdataind(pthis, arg);
        } while(ret > 0);

    } else {
        ret = rwnx_pci_rxdataind(pthis, arg);
    }
    return ret;
}

/**
 * rwnx_rx_deferred - Work function to defer processing of buffer that cannot be
 * done in rwnx_rxdataind (that is called in atomic context)
 *
 * @ws: work field within struct rwnx_defer_rx
 */
void rwnx_rx_deferred(struct work_struct *ws)
{
    struct rwnx_defer_rx *rx = container_of(ws, struct rwnx_defer_rx, work);
    struct sk_buff *skb;

    while ((skb = skb_dequeue(&rx->sk_list)) != NULL) {
        // Currently only management frame can be deferred
        struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
        struct rwnx_defer_rx_cb *rx_cb = (struct rwnx_defer_rx_cb *)skb->cb;

        if (ieee80211_is_action(mgmt->frame_control) &&
            (mgmt->u.action.category == 6)) {
            struct cfg80211_ft_event_params ft_event;
            struct rwnx_vif *vif = rx_cb->vif;
            u8 *action_frame = (u8 *)&mgmt->u.action;
            u8 action_code = action_frame[1];
            /* coverity[overrun-local] - compute status code from ft action frame */
            u16 status_code = *((u16 *)&action_frame[2 + 2 * ETH_ALEN]);

            if ((action_code == 2) && (status_code == 0)) {
                ft_event.target_ap = action_frame + 2 + ETH_ALEN;
                ft_event.ies = action_frame + 2 + 2 * ETH_ALEN + 2;
                ft_event.ies_len = skb->len - (ft_event.ies - (u8 *)mgmt);
                ft_event.ric_ies = NULL;
                ft_event.ric_ies_len = 0;
                cfg80211_ft_event(rx_cb->vif->ndev, &ft_event);
                vif->sta.flags |= RWNX_STA_FT_OVER_DS;
                memcpy(vif->sta.ft_target_ap, ft_event.target_ap, ETH_ALEN);
            }
        } else if (ieee80211_is_auth(mgmt->frame_control)) {
            struct cfg80211_ft_event_params ft_event;
            struct rwnx_vif *vif = rx_cb->vif;
            ft_event.target_ap = vif->sta.ft_target_ap;
            ft_event.ies = mgmt->u.auth.variable;
            ft_event.ies_len = (skb->len -
                                offsetof(struct ieee80211_mgmt, u.auth.variable));
            ft_event.ric_ies = NULL;
            ft_event.ric_ies_len = 0;
            cfg80211_ft_event(rx_cb->vif->ndev, &ft_event);
            vif->sta.flags |= RWNX_STA_FT_OVER_AIR;
        } else {
            netdev_warn(rx_cb->vif->ndev, "Unexpected deferred frame fctl=0x%04x",
                        mgmt->frame_control);
        }

        dev_kfree_skb(skb);
    }
}
