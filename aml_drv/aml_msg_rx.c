/**
 ****************************************************************************************
 *
 * @file aml_msg_rx.c
 *
 * @brief RX function definitions
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ****************************************************************************************
 */
#include "aml_defs.h"
#include "aml_prof.h"
#include "aml_tx.h"
#ifdef CONFIG_AML_BFMER
#include "aml_bfmer.h"
#endif //(CONFIG_AML_BFMER)
#ifdef CONFIG_AML_FULLMAC
#include "aml_debugfs.h"
#include "aml_msg_tx.h"
#include "aml_tdls.h"
#endif /* CONFIG_AML_FULLMAC */
#include "aml_events.h"
#include "aml_compat.h"
#include "aml_main.h"
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include "aml_defs.h"

static int aml_freq_to_idx(struct aml_hw *aml_hw, int freq)
{
    struct ieee80211_supported_band *sband;
    int band, ch, idx = 0;

    for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
#ifdef CONFIG_AML_SOFTMAC
        sband = aml_hw->hw->wiphy->bands[band];
#else
        sband = aml_hw->wiphy->bands[band];
#endif /* CONFIG_AML_SOFTMAC */
        if (!sband) {
            continue;
        }

        for (ch = 0; ch < sband->n_channels; ch++, idx++) {
            if (sband->channels[ch].center_freq == freq) {
                goto exit;
            }
        }
    }

    BUG_ON(1);

exit:
    // Channel has been found, return the index
    return idx;
}

/***************************************************************************
 * Messages from MM task
 **************************************************************************/
static inline int aml_rx_chan_pre_switch_ind(struct aml_hw *aml_hw,
                                              struct aml_cmd *cmd,
                                              struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_AML_SOFTMAC
    struct aml_chanctx *chan_ctxt;
#endif
    struct aml_vif *aml_vif;
    int chan_idx = ((struct mm_channel_pre_switch_ind *)msg->param)->chan_index;

    //AML_DBG(AML_FN_ENTRY_STR);

    REG_SW_SET_PROFILING_CHAN(aml_hw, SW_PROF_CHAN_CTXT_PSWTCH_BIT);

#ifdef CONFIG_AML_SOFTMAC
    list_for_each_entry(chan_ctxt, &aml_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = false;
            break;
        }
    }

    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->chanctx && (aml_vif->chanctx->index == chan_idx)) {
            aml_txq_vif_stop(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
        }
    }
#else
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->up && aml_vif->ch_index == chan_idx) {
            aml_txq_vif_stop(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
        }
    }
#endif /* CONFIG_AML_SOFTMAC */

    REG_SW_CLEAR_PROFILING_CHAN(aml_hw, SW_PROF_CHAN_CTXT_PSWTCH_BIT);

    return 0;
}

static inline int aml_rx_chan_switch_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_AML_SOFTMAC
    struct aml_chanctx *chan_ctxt;
    struct aml_sta *aml_sta;
#endif
    struct aml_vif *aml_vif;
    int chan_idx = ((struct mm_channel_switch_ind *)msg->param)->chan_index;
    bool roc_req = ((struct mm_channel_switch_ind *)msg->param)->roc;
    bool roc_tdls = ((struct mm_channel_switch_ind *)msg->param)->roc_tdls;

    //AML_INFO("chan_idx:%d", chan_idx);

    REG_SW_SET_PROFILING_CHAN(aml_hw, SW_PROF_CHAN_CTXT_SWTCH_BIT);

#ifdef CONFIG_AML_SOFTMAC
    if (roc_tdls) {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;
        // Enable traffic only for TDLS station
        list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
            if (aml_vif->vif_index == vif_index) {
                list_for_each_entry(aml_sta, &aml_vif->stations, list) {
                    if (aml_sta->tdls.active) {
                        aml_vif->roc_tdls = true;
                        aml_txq_tdls_sta_start(aml_sta, AML_TXQ_STOP_CHAN, aml_hw);
                        break;
                    }
                }
                break;
            }
        }
    } else if (!roc_req) {
        list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
            if (aml_vif->chanctx && (aml_vif->chanctx->index == chan_idx)) {
                aml_txq_vif_start(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
            }
        }
    } else {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;

        // Inform the host that the offchannel period has been started
        ieee80211_ready_on_channel(aml_hw->hw);

        // Enable traffic for associated VIF (roc may happen without chanctx)
        list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
            if (aml_vif->vif_index == vif_index) {
                aml_txq_vif_start(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
            }
        }
    }

    /* keep cur_chan up to date */
    list_for_each_entry(chan_ctxt, &aml_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = true;
            aml_hw->cur_freq = chan_ctxt->ctx->def.center_freq1;
            aml_hw->cur_band = chan_ctxt->ctx->def.chan->band;
            if (chan_ctxt->ctx->def.chan->flags & IEEE80211_CHAN_RADAR) {
                aml_radar_detection_enable(&aml_hw->radar,
                                            AML_RADAR_DETECT_REPORT,
                                            AML_RADAR_RIU);
            } else {
                aml_radar_detection_enable(&aml_hw->radar,
                                            AML_RADAR_DETECT_DISABLE,
                                            AML_RADAR_RIU);
            }
            break;
        }
    }

#else
    if (roc_tdls) {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;
        list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
            if (aml_vif->vif_index == vif_index) {
                aml_vif->roc_tdls = true;
                aml_txq_tdls_sta_start(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
            }
        }
    } else if (!roc_req) {
        list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
            if (aml_vif->up && aml_vif->ch_index == chan_idx) {
                aml_txq_vif_start(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
            }
        }
    } else {
        struct aml_roc *roc = aml_hw->roc;

        if (!roc)
            return 0;

        aml_vif = roc->vif;

        trace_switch_roc(aml_vif->vif_index);

        if (!roc->internal) {
            // If RoC has been started by the user space, inform it that we have
            // switched on the requested off-channel
            cfg80211_ready_on_channel(&aml_vif->wdev, (u64)(roc),
                                      roc->chan, roc->duration, GFP_ATOMIC);
        }

        // Keep in mind that we have switched on the channel
        roc->on_chan = true;
        // Enable traffic on OFF channel queue
        aml_txq_offchan_start(aml_hw);
    }

    aml_hw->cur_chanctx = chan_idx;
    aml_radar_detection_enable_on_cur_channel(aml_hw);

#endif /* CONFIG_AML_SOFTMAC */

    REG_SW_CLEAR_PROFILING_CHAN(aml_hw, SW_PROF_CHAN_CTXT_SWTCH_BIT);

    return 0;
}

static inline int aml_rx_tdls_chan_switch_cfm(struct aml_hw *aml_hw,
                                                struct aml_cmd *cmd,
                                                struct ipc_e2a_msg *msg)
{
    return 0;
}

static inline int aml_rx_tdls_chan_switch_ind(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_AML_SOFTMAC
    struct aml_chanctx *chan_ctxt;
    u8 chan_idx = ((struct tdls_chan_switch_ind *)msg->param)->chan_ctxt_index;

    AML_DBG(AML_FN_ENTRY_STR);

    // Enable channel context
    list_for_each_entry(chan_ctxt, &aml_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = true;
            aml_hw->cur_freq = chan_ctxt->ctx->def.center_freq1;
            aml_hw->cur_band = chan_ctxt->ctx->def.chan->band;
        }
    }

    return 0;
#else
    // Enable traffic on OFF channel queue
    aml_txq_offchan_start(aml_hw);

    return 0;
#endif
}

static inline int aml_rx_tdls_chan_switch_base_ind(struct aml_hw *aml_hw,
                                                    struct aml_cmd *cmd,
                                                    struct ipc_e2a_msg *msg)
{
    struct aml_vif *aml_vif;
    u8 vif_index = ((struct tdls_chan_switch_base_ind *)msg->param)->vif_index;
#ifdef CONFIG_AML_SOFTMAC
    struct aml_sta *aml_sta;
#endif

    AML_DBG(AML_FN_ENTRY_STR);

#ifdef CONFIG_AML_SOFTMAC
    // Disable traffic for associated VIF
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == vif_index) {
            if (aml_vif->chanctx)
                aml_vif->chanctx->active = false;
            list_for_each_entry(aml_sta, &aml_vif->stations, list) {
                if (aml_sta->tdls.active) {
                    aml_vif->roc_tdls = false;
                    aml_txq_tdls_sta_stop(aml_sta, AML_TXQ_STOP_CHAN, aml_hw);
                    break;
                }
            }
            break;
        }
    }
    return 0;
#else
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == vif_index) {
            aml_vif->roc_tdls = false;
            aml_txq_tdls_sta_stop(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
        }
    }
    return 0;
#endif
}

static inline int aml_rx_tdls_peer_ps_ind(struct aml_hw *aml_hw,
                                           struct aml_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    struct aml_vif *aml_vif;
    u8 vif_index = ((struct tdls_peer_ps_ind *)msg->param)->vif_index;
    bool ps_on = ((struct tdls_peer_ps_ind *)msg->param)->ps_on;

#ifdef CONFIG_AML_SOFTMAC
    u8 sta_idx = ((struct tdls_peer_ps_ind *)msg->param)->sta_idx;
    struct aml_sta *aml_sta;
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == vif_index) {
            list_for_each_entry(aml_sta, &aml_vif->stations, list) {
                if (aml_sta->sta_idx == sta_idx) {
                    aml_sta->tdls.ps_on = ps_on;
                    if (ps_on) {
                        // disable TXQ for TDLS peer
                        aml_txq_tdls_sta_stop(aml_sta, AML_TXQ_STOP_STA_PS, aml_hw);
                    } else {
                        // Enable TXQ for TDLS peer
                        aml_txq_tdls_sta_start(aml_sta, AML_TXQ_STOP_STA_PS, aml_hw);
                    }
                    break;
                }
            }
            break;
        }
    }
    return 0;
#else
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == vif_index) {
            aml_vif->sta.tdls_sta->tdls.ps_on = ps_on;
            // Update PS status for the TDLS station
            aml_ps_bh_enable(aml_hw, aml_vif->sta.tdls_sta, ps_on);
        }
    }

    return 0;
#endif
}

static inline int aml_rx_remain_on_channel_exp_ind(struct aml_hw *aml_hw,
                                                    struct aml_cmd *cmd,
                                                    struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_AML_SOFTMAC
    struct aml_vif *aml_vif;
    u8 vif_index = ((struct mm_remain_on_channel_exp_ind *)msg->param)->vif_index;

    AML_DBG(AML_FN_ENTRY_STR);

    ieee80211_remain_on_channel_expired(aml_hw->hw);

    // Disable traffic for associated VIF
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == vif_index) {
            if (aml_vif->chanctx)
                aml_vif->chanctx->active = false;

            aml_txq_vif_stop(aml_vif, AML_TXQ_STOP_CHAN, aml_hw);
            break;
        }
    }

    return 0;

#else
    struct aml_roc *roc = aml_hw->roc;
    struct aml_vif *aml_vif;

    if (!aml_hw->roc)
        return 0;

    aml_vif= roc->vif;

    trace_roc_exp(aml_vif->vif_index);

    AML_INFO("roc internal=%d, on_chan=%d");
    if (!roc->internal && roc->on_chan) {
        // If RoC has been started by the user space and hasn't been cancelled,
        // inform it that off-channel period has expired
        cfg80211_remain_on_channel_expired(&aml_vif->wdev, (u64)(roc),
                                           roc->chan, GFP_ATOMIC);
    }

    aml_txq_offchan_deinit(aml_vif);

    kfree(roc);
    aml_hw->roc = NULL;

#endif /* CONFIG_AML_SOFTMAC */
    return 0;
}

static inline int aml_rx_p2p_vif_ps_change_ind(struct aml_hw *aml_hw,
                                                struct aml_cmd *cmd,
                                                struct ipc_e2a_msg *msg)
{
    int vif_idx  = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->vif_index;
    int ps_state = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->ps_state;
    struct aml_vif *vif_entry;

    AML_DBG(AML_FN_ENTRY_STR);

#ifdef CONFIG_AML_SOFTMAC
    // Look for VIF entry
    list_for_each_entry(vif_entry, &aml_hw->vifs, list) {
        if (vif_entry->vif_index == vif_idx) {
            goto found_vif;
        }
    }
#else
    vif_entry = aml_hw->vif_table[vif_idx];

    if (vif_entry) {
        goto found_vif;
    }
#endif /* CONFIG_AML_SOFTMAC */

    goto exit;

found_vif:

#ifdef CONFIG_AML_SOFTMAC
    if (ps_state == MM_PS_MODE_OFF) {
        aml_txq_vif_start(vif_entry, AML_TXQ_STOP_VIF_PS, aml_hw);
    }
    else {
        aml_txq_vif_stop(vif_entry, AML_TXQ_STOP_VIF_PS, aml_hw);
    }
#else
    if (ps_state == MM_PS_MODE_OFF) {
        // Start TX queues for provided VIF
        aml_txq_vif_start(vif_entry, AML_TXQ_STOP_VIF_PS, aml_hw);
    }
    else {
        // Stop TX queues for provided VIF
        aml_txq_vif_stop(vif_entry, AML_TXQ_STOP_VIF_PS, aml_hw);
    }
#endif /* CONFIG_AML_SOFTMAC */

exit:
    return 0;
}

static inline int aml_rx_channel_survey_ind(struct aml_hw *aml_hw,
                                             struct aml_cmd *cmd,
                                             struct ipc_e2a_msg *msg)
{
    struct mm_channel_survey_ind *ind = (struct mm_channel_survey_ind *)msg->param;
    // Get the channel index
    int idx = aml_freq_to_idx(aml_hw, ind->freq);
    // Get the survey
    struct aml_survey_info *aml_survey;

    //AML_DBG(AML_FN_ENTRY_STR);

    if (idx >  ARRAY_SIZE(aml_hw->survey))
        return 0;

    aml_survey = &aml_hw->survey[idx];

    // Store the received parameters
    aml_survey->chan_time_ms = ind->chan_time_ms;
    aml_survey->chan_time_busy_ms = ind->chan_time_busy_ms;
    aml_survey->noise_dbm = ind->noise_dbm;
    aml_survey->filled = (SURVEY_INFO_TIME |
                           SURVEY_INFO_TIME_BUSY);

    if (ind->noise_dbm != 0) {
        aml_survey->filled |= SURVEY_INFO_NOISE_DBM;
    }

    return 0;
}

static inline int aml_rx_p2p_noa_upd_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    return 0;
}

static inline int aml_rx_rssi_status_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_rssi_status_ind *ind = (struct mm_rssi_status_ind *)msg->param;
    int vif_idx  = ind->vif_index;
    bool rssi_status = ind->rssi_status;

    struct aml_vif *vif_entry;

    AML_DBG(AML_FN_ENTRY_STR);

#ifdef CONFIG_AML_SOFTMAC
    list_for_each_entry(vif_entry, &aml_hw->vifs, list) {
        if (vif_entry->vif_index == vif_idx) {
            ieee80211_cqm_rssi_notify(vif_entry->vif,
                                      rssi_status ? NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
                                                    NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
                                      ind->rssi, GFP_ATOMIC);
        }
    }
#else
    vif_entry = aml_hw->vif_table[vif_idx];
    if (vif_entry) {
        cfg80211_cqm_rssi_notify(vif_entry->ndev,
                                 rssi_status ? NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
                                               NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
                                 ind->rssi, GFP_ATOMIC);
    }
#endif /* CONFIG_AML_SOFTMAC */

    return 0;
}

static inline int aml_rx_pktloss_notify_ind(struct aml_hw *aml_hw,
                                             struct aml_cmd *cmd,
                                             struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_AML_FULLMAC
    struct mm_pktloss_ind *ind = (struct mm_pktloss_ind *)msg->param;
    struct aml_vif *vif_entry;
    int vif_idx  = ind->vif_index;

    AML_DBG(AML_FN_ENTRY_STR);

    vif_entry = aml_hw->vif_table[vif_idx];
    if (vif_entry) {
        cfg80211_cqm_pktloss_notify(vif_entry->ndev, (const u8 *)ind->mac_addr.array,
                                    ind->num_packets, GFP_ATOMIC);
    }
#endif /* CONFIG_AML_FULLMAC */

    return 0;
}

static inline int aml_rx_csa_counter_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_csa_counter_ind *ind = (struct mm_csa_counter_ind *)msg->param;
    struct aml_vif *vif;
    bool found = false;

    AML_DBG(AML_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
#ifdef CONFIG_AML_SOFTMAC
        if (ind->csa_count == 1)
            ieee80211_csa_finish(vif->vif);
        else
            ieee80211_beacon_update_cntdwn(vif->vif);
#else
        if (vif->ap.csa)
            vif->ap.csa->count = ind->csa_count;
        else
            netdev_err(vif->ndev, "CSA counter update but no active CSA");

#endif
    }

    return 0;
}

#ifdef CONFIG_AML_SOFTMAC
static inline int aml_rx_connection_loss_ind(struct aml_hw *aml_hw,
                                              struct aml_cmd *cmd,
                                              struct ipc_e2a_msg *msg)
{
    struct aml_vif *aml_vif;
    u8 inst_nbr;

    AML_DBG(AML_FN_ENTRY_STR);

    inst_nbr = ((struct mm_connection_loss_ind *)msg->param)->inst_nbr;

    /* Search the VIF entry corresponding to the instance number */
    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->vif_index == inst_nbr) {
            ieee80211_connection_loss(aml_vif->vif);
            break;
        }
    }

    return 0;
}


#ifdef CONFIG_AML_BCN
static inline int aml_rx_prm_tbtt_ind(struct aml_hw *aml_hw,
                                       struct aml_cmd *cmd,
                                       struct ipc_e2a_msg *msg)
{
    AML_DBG(AML_FN_ENTRY_STR);

    aml_tx_bcns(aml_hw);

    return 0;
}
#endif

#else /* !CONFIG_AML_SOFTMAC */
static inline int aml_rx_csa_finish_ind(struct aml_hw *aml_hw,
                                         struct aml_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct mm_csa_finish_ind *ind = (struct mm_csa_finish_ind *)msg->param;
    struct aml_vif *vif;
    bool found = false;

    AML_DBG(AML_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
        if (AML_VIF_TYPE(vif) == NL80211_IFTYPE_AP ||
            AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO) {
            if (vif->ap.csa) {
                vif->ap.csa->status = ind->status;
                vif->ap.csa->ch_idx = ind->chan_idx;
                schedule_work(&vif->ap.csa->work);
            } else
                netdev_err(vif->ndev, "CSA finish indication but no active CSA");
        } else {
            if (ind->status == 0) {
                aml_chanctx_unlink(vif);
                aml_chanctx_link(vif, ind->chan_idx, NULL);
                if (aml_hw->cur_chanctx == ind->chan_idx) {
                    aml_radar_detection_enable_on_cur_channel(aml_hw);
                    aml_txq_vif_start(vif, AML_TXQ_STOP_CHAN, aml_hw);
                } else
                    aml_txq_vif_stop(vif, AML_TXQ_STOP_CHAN, aml_hw);
            }
        }
    }

    return 0;
}

static inline int aml_rx_csa_traffic_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_csa_traffic_ind *ind = (struct mm_csa_traffic_ind *)msg->param;
    struct aml_vif *vif;
    bool found = false;

    AML_DBG(AML_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
        if (ind->enable)
            aml_txq_vif_start(vif, AML_TXQ_STOP_CSA, aml_hw);
        else
            aml_txq_vif_stop(vif, AML_TXQ_STOP_CSA, aml_hw);
    }

    return 0;
}

static inline int aml_rx_ps_change_ind(struct aml_hw *aml_hw,
                                        struct aml_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    struct mm_ps_change_ind *ind = (struct mm_ps_change_ind *)msg->param;
    struct aml_sta *sta = &aml_hw->sta_table[ind->sta_idx];

    AML_DBG(AML_FN_ENTRY_STR);

    if (ind->sta_idx >= (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) {
        wiphy_err(aml_hw->wiphy, "Invalid sta index reported by fw %d\n",
                  ind->sta_idx);
        return 1;
    }

    AML_INFO("Sta %d, change PS mode to %s", sta->sta_idx,
               ind->ps_state ? "ON" : "OFF");

    if (sta->valid) {
        aml_ps_bh_enable(aml_hw, sta, ind->ps_state);
    } else if (test_bit(AML_DEV_ADDING_STA, (void*)&aml_hw->flags)) {
        sta->ps.active = ind->ps_state ? true : false;
    } else {
        AML_INFO("Ignore PS mode change on invalid sta\n");
    }

    return 0;
}


static inline int aml_rx_traffic_req_ind(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_traffic_req_ind *ind = (struct mm_traffic_req_ind *)msg->param;
    struct aml_sta *sta = &aml_hw->sta_table[ind->sta_idx];

    AML_DBG(AML_FN_ENTRY_STR);

    netdev_dbg(aml_hw->vif_table[sta->vif_idx]->ndev,
               "Sta %d, asked for %d pkt", sta->sta_idx, ind->pkt_cnt);

    aml_ps_bh_traffic_req(aml_hw, sta, ind->pkt_cnt,
                           ind->uapsd ? UAPSD_ID : LEGACY_PS_ID);

    return 0;
}
#endif /* CONFIG_AML_SOFTMAC */

/***************************************************************************
 * Messages from SCAN task
 **************************************************************************/
#ifdef CONFIG_AML_SOFTMAC
static inline int aml_rx_scan_done_ind(struct aml_hw *aml_hw,
                                        struct aml_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    struct cfg80211_scan_info info = {
        .aborted = false,
    };
#endif
    AML_DBG(AML_FN_ENTRY_STR);

    aml_ipc_buf_dealloc(aml_hw, &aml_hw->scan_ie);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    ieee80211_scan_completed(aml_hw->hw, &info);
#else
    ieee80211_scan_completed(aml_hw->hw, false);
#endif

    return 0;
}
#endif /* CONFIG_AML_SOFTMAC */

/***************************************************************************
 * Messages from SCANU task
 **************************************************************************/
#ifdef CONFIG_AML_FULLMAC
static inline int aml_rx_scanu_start_cfm(struct aml_hw *aml_hw,
                                          struct aml_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    AML_DBG(AML_FN_ENTRY_STR);

    aml_ipc_buf_dealloc(aml_hw, &aml_hw->scan_ie);
    if (aml_hw->scan_request) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
        struct cfg80211_scan_info info = {
            .aborted = false,
        };

        cfg80211_scan_done(aml_hw->scan_request, &info);
#else
        cfg80211_scan_done(aml_hw->scan_request, false);
#endif
    }

    aml_hw->scan_request = NULL;

    return 0;
}

static inline int aml_sdio_rx_scanu_result_ind(struct aml_hw *aml_hw)
{
    struct cfg80211_bss *bss = NULL;
    struct ieee80211_channel *chan;
    u8* ie = NULL;
    u64 timestamp;
    struct ieee80211_mgmt *mgmt = NULL;
    struct scan_results *scan_res,*next;

    spin_lock_bh(&aml_hw->scan_lock);
    list_for_each_entry_safe(scan_res, next, &aml_hw->scan_res_list, list) {
        struct sdio_scanu_result_ind *ind = &scan_res->scanu_res_ind;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
        struct timespec ts;
        get_monotonic_boottime(&ts);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
        timestamp = ktime_to_us(ktime_get_boottime());
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
        timestamp = ((u64)ts.tv_sec * 1000000) + ts.tv_nsec / 1000;
#endif

        mgmt = (struct ieee80211_mgmt *)ind->payload;
        mgmt->u.probe_resp.timestamp = timestamp;
        ie = mgmt->u.probe_resp.variable;
        /*
        if ((ie[0] == WLAN_EID_SSID) && (ie + 2 + ie[1] < (u8 *) mgmt + ind->length)) {
        printk("ssid:%-32.32s bssid:%02x:%02x:%02x:%02x:%02x:%02x, frm ctrl 0x%08x",
                ssid_sprintf(&ie[2], ie[1]),
                mgmt->bssid[0],
                mgmt->bssid[1],
                mgmt->bssid[2],
                mgmt->bssid[3],
                mgmt->bssid[4],
                mgmt->bssid[5], mgmt->frame_control);
        }
        */
        //AML_DBG(AML_FN_ENTRY_STR);

        chan = ieee80211_get_channel(aml_hw->wiphy, ind->center_freq);

        if (chan != NULL)
        bss = cfg80211_inform_bss_frame(aml_hw->wiphy, chan,
                                        mgmt,
                                        ind->length, ind->rssi * 100, GFP_ATOMIC);

        if (bss != NULL)
        cfg80211_put_bss(aml_hw->wiphy, bss);

        list_del(&scan_res->list);
        list_add_tail(&scan_res->list, &aml_hw->scan_res_avilable_list);
    }
    spin_unlock_bh(&aml_hw->scan_lock);

    return 0;
}


static inline int aml_pcie_rx_scanu_result_ind(struct aml_hw *aml_hw,
                                           struct aml_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    struct cfg80211_bss *bss = NULL;
    struct ieee80211_channel *chan;
    u64 timestamp;
    struct ieee80211_mgmt *mgmt = NULL;
    struct scanu_result_ind *ind = (struct scanu_result_ind *)msg->param;
    u8* ie = NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
    struct timespec ts;
    get_monotonic_boottime(&ts);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
    timestamp = ktime_to_us(ktime_get_boottime());
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
    timestamp = ((u64)ts.tv_sec * 1000000) + ts.tv_nsec / 1000;
#endif
    mgmt = (struct ieee80211_mgmt *)ind->payload;
    mgmt->u.probe_resp.timestamp = timestamp;
    ie = mgmt->u.probe_resp.variable;
    /*
    if ((ie[0] == WLAN_EID_SSID) && (ie + 2 + ie[1] < (u8 *) mgmt + ind->length)) {
        printk("ssid:%-32.32s bssid:%02x:%02x:%02x:%02x:%02x:%02x",
                ssid_sprintf(&ie[2], ie[1]),
                mgmt->bssid[0],
                mgmt->bssid[1],
                mgmt->bssid[2],
                mgmt->bssid[3],
                mgmt->bssid[4],
                mgmt->bssid[5]);
    }
    */
    //AML_DBG(AML_FN_ENTRY_STR);

    chan = ieee80211_get_channel(aml_hw->wiphy, ind->center_freq);

    if (chan != NULL)
        bss = cfg80211_inform_bss_frame(aml_hw->wiphy, chan,
                                        mgmt,
                                        ind->length, ind->rssi * 100, GFP_ATOMIC);

    if (bss != NULL)
        cfg80211_put_bss(aml_hw->wiphy, bss);

    return 0;
}


static inline int aml_rx_scanu_result_ind(struct aml_hw *aml_hw,
                                           struct aml_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    if (aml_bus_type == PCIE_MODE)
        aml_pcie_rx_scanu_result_ind(aml_hw, cmd, msg);
    else
        aml_sdio_rx_scanu_result_ind(aml_hw);

    return 0;
}

#endif /* CONFIG_AML_FULLMAC */

/***************************************************************************
 * Messages from ME task
 **************************************************************************/
#ifdef CONFIG_AML_FULLMAC
static inline int aml_rx_me_tkip_mic_failure_ind(struct aml_hw *aml_hw,
                                                  struct aml_cmd *cmd,
                                                  struct ipc_e2a_msg *msg)
{
    struct me_tkip_mic_failure_ind *ind = (struct me_tkip_mic_failure_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct net_device *dev = aml_vif->ndev;

    AML_DBG(AML_FN_ENTRY_STR);

    cfg80211_michael_mic_failure(dev, (u8 *)&ind->addr, (ind->ga?NL80211_KEYTYPE_GROUP:
                                 NL80211_KEYTYPE_PAIRWISE), ind->keyid,
                                 (u8 *)&ind->tsc, GFP_ATOMIC);

    return 0;
}

static inline int aml_rx_me_tx_credits_update_ind(struct aml_hw *aml_hw,
                                                   struct aml_cmd *cmd,
                                                   struct ipc_e2a_msg *msg)
{
    struct me_tx_credits_update_ind *ind = (struct me_tx_credits_update_ind *)msg->param;

    AML_DBG(AML_FN_ENTRY_STR);

    aml_txq_credit_update(aml_hw, ind->sta_idx, ind->tid, ind->credits);

    return 0;
}
#endif /* CONFIG_AML_FULLMAC */

/***************************************************************************
 * Messages from SM task
 **************************************************************************/
#ifdef CONFIG_AML_FULLMAC
extern const struct aml_legrate legrates_lut[16];

char format_mod_trace[][20] = {
    "NON_HT",
    "NON_HT_DUP_OFDM",
    "HT_MF",
    "HT_GF",
    "VHT",
    "HE_SU",
    "HE_MU",
    "HE_ER"
};
char no_ss_trace[][5] = {
    "1ss",
    "2ss"
};
char bw_width_trace[][15] = {
    "BW_20",
    "BW_40",
    "BW_80",
    "BW_160",
    "BW_80P80",
    "BW_OTHER"
};

static inline int aml_rx_sm_connect_ind_ex(struct aml_hw *aml_hw,
                                         struct aml_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct sm_connect_ind_ex *ind = (struct sm_connect_ind_ex *)msg->param;
    struct aml_sta *sta = &aml_hw->sta_table[ind->ap_idx];
    int i;

    sta->stats.bcn_interval = ind->bcn_interval;
    sta->stats.bw_max = ind->bw_max;
    //sta->stats.dtim = ind->dtim; //TODO dtim info
    sta->stats.mcs_max = ind->mcs_max;
    sta->stats.no_ss = ind->no_ss;
    sta->stats.format_mod = ind->format_mod;
    sta->stats.short_gi = ind->short_gi;
    AML_INFO("freq[%d %d %d],bw:%s max_mcs:%d format_mode:%s no_ss:%s short_gi:%d \n",
        sta->center_freq,
        sta->center_freq1,
        sta->center_freq2,
        bw_width_trace[sta->width],
        sta->stats.mcs_max,
        format_mod_trace[sta->stats.format_mod],
        no_ss_trace[sta->stats.no_ss],
        sta->stats.short_gi
        );

    for (i = 0; i < sizeof (legrates_lut) / sizeof(struct aml_legrate); i++) {
        if (ind->r_idx_max == legrates_lut[i].idx) {
            sta->stats.leg_rate = legrates_lut[i].rate;
            break;
        }
    }
    return 0;
}

static inline int aml_apm_handle_disconnect_sta(struct aml_hw *aml_hw,
                                         struct aml_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct apm_disconnect_sta_ind *ind = (struct apm_disconnect_sta_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct net_device *dev = aml_vif->ndev;

    AML_INFO("sta ageout --> %02x:%02x:%02x:%02x:%02x:%02x \n",
        ind->sta_mac[0],
        ind->sta_mac[1],
        ind->sta_mac[2],
        ind->sta_mac[3],
        ind->sta_mac[4],
        ind->sta_mac[5]);

    if (aml_hw->wiphy->flags & WIPHY_FLAG_HAVE_AP_SME) {
        cfg80211_del_sta(dev, ind->sta_mac, GFP_ATOMIC);
    }
    else {
        #define DEAUTH_FRAME_LEN 26
        #define DEAUTH_TYPE 0XC0
        #define DEAUTH_REASON 0X3
        struct sk_buff *skb;
        u8 *pos;
        skb = alloc_skb(DEAUTH_FRAME_LEN, GFP_ATOMIC);
        pos = (void *)skb_put(skb, DEAUTH_FRAME_LEN);

        *pos = DEAUTH_TYPE;
        pos += 2;
        memset(pos,0,2);//dur
        pos += 2;
        memcpy(pos,dev->dev_addr,MAC_ADDR_LEN);
        pos += MAC_ADDR_LEN;
        memcpy(pos,ind->sta_mac,MAC_ADDR_LEN);
        pos += MAC_ADDR_LEN;
        memcpy(pos,dev->dev_addr,MAC_ADDR_LEN);
        pos += MAC_ADDR_LEN;
        memset(pos,0,2);//seq
        pos += 2;
        *pos = DEAUTH_REASON;

        cfg80211_rx_mgmt(&aml_vif->wdev, ind->pri_freq, 0, skb->data, skb->len, 0);
        kfree_skb(skb);
    }
    return 0;
}

static inline int aml_rx_sm_connect_ind(struct aml_hw *aml_hw,
                                         struct aml_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct sm_connect_ind *ind = (struct sm_connect_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct net_device *dev = aml_vif->ndev;
    const u8 *req_ie, *rsp_ie;
    const u8 *extcap_ie;
    const struct ieee_types_extcap *extcap;

    AML_INFO("vif_idx:%d,status_code:%d, sta_idx:%d, center_freq:%d, center_freq1:%d, roamed:%d\n",
              ind->vif_idx,ind->status_code, ind->ap_idx, ind->chan.prim20_freq, ind->chan.center1_freq, ind->roamed);
    aml_vif->sta.scan_hang = 0;
    aml_hw->is_connectting = 0;
    /* Retrieve IE addresses and lengths */
    req_ie = (const u8 *)ind->assoc_ie_buf;
    rsp_ie = req_ie + ind->assoc_req_ie_len;

    // Fill-in the AP information
    if (ind->status_code == 0)
    {
        struct aml_sta *sta = &aml_hw->sta_table[ind->ap_idx];
        u8 txq_status;
        struct ieee80211_channel *chan;
        struct cfg80211_chan_def chandef = {0};

        sta->valid = true;
        sta->sta_idx = ind->ap_idx;
        sta->ch_idx = ind->ch_idx;
        sta->vif_idx = ind->vif_idx;
        sta->vlan_idx = sta->vif_idx;
        sta->qos = ind->qos;
        sta->acm = ind->acm;
        sta->ps.active = false;
        sta->aid = ind->aid;
        sta->band = (enum nl80211_band)ind->chan.band;
        sta->width = (enum nl80211_chan_width)ind->chan.type;
        sta->center_freq = ind->chan.prim20_freq;
        sta->center_freq1 = ind->chan.center1_freq;
        sta->center_freq2 = ind->chan.center2_freq;
        aml_vif->sta.ap = sta;
        aml_vif->generation++;
        chan = ieee80211_get_channel(aml_hw->wiphy, ind->chan.prim20_freq);

        cfg80211_chandef_create(&chandef, chan, NL80211_CHAN_NO_HT);
        if (!aml_hw->mod_params->ht_on)
            chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
        else
            chandef.width = (enum nl80211_chan_width)chnl2bw[ind->chan.type];
        chandef.center_freq1 = ind->chan.center1_freq;
        chandef.center_freq2 = ind->chan.center2_freq;
        aml_chanctx_link(aml_vif, ind->ch_idx, &chandef);
        memcpy(sta->mac_addr, ind->bssid.array, ETH_ALEN);
        if (ind->ch_idx == aml_hw->cur_chanctx) {
            txq_status = 0;
        } else {
            txq_status = AML_TXQ_STOP_CHAN;
        }
        memcpy(sta->ac_param, ind->ac_param, sizeof(sta->ac_param));
        aml_txq_sta_init(aml_hw, sta, txq_status);
        aml_dbgfs_register_sta(aml_hw, sta);
        aml_txq_tdls_vif_init(aml_vif);
        aml_mu_group_sta_init(sta, NULL);
        /* Look for TDLS Channel Switch Prohibited flag in the Extended Capability
         * Information Element*/
        extcap_ie = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, rsp_ie, ind->assoc_rsp_ie_len);
        if (extcap_ie && extcap_ie[1] >= 5) {
            extcap = (void *)(extcap_ie);
            aml_vif->tdls_chsw_prohibited = extcap->ext_capab[4] & WLAN_EXT_CAPA5_TDLS_CH_SW_PROHIBITED;
        }

#ifdef CONFIG_AML_BFMER
        /* If Beamformer feature is activated, check if features can be used
         * with the new peer device
         */
        if (aml_hw->mod_params->bfmer) {
            const u8 *vht_capa_ie;
            const struct ieee80211_vht_cap *vht_cap;

            do {
                /* Look for VHT Capability Information Element */
                vht_capa_ie = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, rsp_ie,
                                               ind->assoc_rsp_ie_len);

                /* Stop here if peer device does not support VHT */
                if (!vht_capa_ie) {
                    break;
                }

                vht_cap = (const struct ieee80211_vht_cap *)(vht_capa_ie + 2);

                /* Send MM_BFMER_ENABLE_REQ message if needed */
                aml_send_bfmer_enable(aml_hw, sta, vht_cap);
            } while (0);
        }
#endif //(CONFIG_AML_BFMER)

#ifdef CONFIG_AML_MON_DATA
        // If there are 1 sta and 1 monitor interface active at the same time then
        // monitor interface channel context is always the same as the STA interface.
        // This doesn't work with 2 STA interfaces but we don't want to support it.
        if (aml_hw->monitor_vif != AML_INVALID_VIF) {
            struct aml_vif *aml_mon_vif = aml_hw->vif_table[aml_hw->monitor_vif];
            aml_chanctx_unlink(aml_mon_vif);
            aml_chanctx_link(aml_mon_vif, ind->ch_idx, NULL);
        }
#endif
    } else {
        aml_external_auth_disable(aml_vif);
    }

    if (ind->roamed) {
        struct cfg80211_roam_info info;
        memset(&info, 0, sizeof(info));

        if (aml_vif->ch_index < NX_CHAN_CTXT_CNT)
            LINK_PARAMS(info, channel) = aml_hw->chanctx_table[aml_vif->ch_index].chan_def.chan;
        LINK_PARAMS(info, bssid) = (const u8 *)ind->bssid.array;
        info.req_ie = req_ie;
        info.req_ie_len = ind->assoc_req_ie_len;
        info.resp_ie = rsp_ie;
        info.resp_ie_len = ind->assoc_rsp_ie_len;
        cfg80211_roamed(dev, &info, GFP_ATOMIC);
    } else {
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
            struct cfg80211_bss *bss = NULL;
            struct ieee80211_channel *notify_channel = NULL;

            notify_channel = ieee80211_get_channel(aml_hw->wiphy, ind->chan.prim20_freq);
            if (notify_channel == NULL) {
                AML_INFO("channel is null\n");
                return -1;
            }
            bss = cfg80211_get_bss(aml_hw->wiphy, notify_channel, (const u8 *)ind->bssid.array, (const u8 *)aml_vif->sta.assoc_ssid, aml_vif->sta.assoc_ssid_len,
                                IEEE80211_BSS_TYPE_ANY, IEEE80211_PRIVACY_ANY);

            if (bss != NULL) {
                AML_INFO("ssid:%-32.32s, bss_freq:%d\n", ssid_sprintf(aml_vif->sta.assoc_ssid, aml_vif->sta.assoc_ssid_len), bss->channel->center_freq);
                cfg80211_connect_bss(dev, (const u8 *)ind->bssid.array, bss, req_ie, ind->assoc_req_ie_len, rsp_ie,
                #if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 12, 0)
                                    ind->assoc_rsp_ie_len, WLAN_STATUS_SUCCESS, GFP_ATOMIC);
                #else
                                    ind->assoc_rsp_ie_len, WLAN_STATUS_SUCCESS, GFP_ATOMIC,NL80211_TIMEOUT_UNSPECIFIED);
                #endif
            }else {
                AML_INFO("can't find bss in kernel\n");
            }
    #else
            cfg80211_connect_result(dev, (const u8 *)ind->bssid.array, req_ie,
                                    ind->assoc_req_ie_len, rsp_ie,
                                    ind->assoc_rsp_ie_len, ind->status_code,
                                    GFP_ATOMIC);
    #endif
    }

    netif_tx_start_all_queues(dev);
    netif_carrier_on(dev);

    return 0;
}

static inline int aml_rx_sm_disconnect_ind(struct aml_hw *aml_hw,
                                            struct aml_cmd *cmd,
                                            struct ipc_e2a_msg *msg)
{
    struct sm_disconnect_ind *ind = (struct sm_disconnect_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct net_device *dev;

    if (aml_vif == NULL)
        return 0;

    AML_DBG(AML_FN_ENTRY_STR);

    dev = aml_vif->ndev;

    /* if vif is not up, aml_close has already been called */
    if (aml_vif->up) {
        if (!ind->reassoc) {
            cfg80211_disconnected(dev, ind->reason_code, NULL, 0,
                                  (ind->reason_code <= 1), GFP_ATOMIC);

            if (aml_vif->sta.ft_assoc_ies) {
                kfree(aml_vif->sta.ft_assoc_ies);
                aml_vif->sta.ft_assoc_ies = NULL;
                aml_vif->sta.ft_assoc_ies_len = 0;
            }
        }
        netif_tx_stop_all_queues(dev);
        netif_carrier_off(dev);
    }
    if (aml_vif->sta.ap) {
#ifdef CONFIG_AML_BFMER
        /* Disable Beamformer if supported */
        aml_bfmer_report_del(aml_hw, aml_vif->sta.ap);
#endif //(CONFIG_AML_BFMER)

        aml_txq_sta_deinit(aml_hw, aml_vif->sta.ap);
        aml_txq_tdls_vif_deinit(aml_vif);
        aml_dbgfs_unregister_sta(aml_hw, aml_vif->sta.ap);
        aml_vif->sta.ap->valid = false;
        aml_vif->sta.ap = NULL;
    }
    aml_vif->generation++;
    aml_external_auth_disable(aml_vif);
    aml_chanctx_unlink(aml_vif);
    aml_vif->sta.scan_hang = 0;

    return 0;
}

static inline int aml_rx_sm_external_auth_required_ind(struct aml_hw *aml_hw,
                                                        struct aml_cmd *cmd,
                                                        struct ipc_e2a_msg *msg)
{
    struct sm_external_auth_required_ind *ind =
        (struct sm_external_auth_required_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
    struct net_device *dev = aml_vif->ndev;
    struct cfg80211_external_auth_params params;

    AML_DBG(AML_FN_ENTRY_STR);

    params.action = NL80211_EXTERNAL_AUTH_START;
    memcpy(params.bssid, ind->bssid.array, ETH_ALEN);
    params.ssid.ssid_len = ind->ssid.length;
    memcpy(params.ssid.ssid, ind->ssid.array,
           min_t(size_t, ind->ssid.length, sizeof(params.ssid.ssid)));
    params.key_mgmt_suite = ind->akm;

    if ((ind->vif_idx > NX_VIRT_DEV_MAX) || !aml_vif->up ||
        (AML_VIF_TYPE(aml_vif) != NL80211_IFTYPE_STATION) ||
        cfg80211_external_auth_request(dev, &params, GFP_ATOMIC)) {
        wiphy_err(aml_hw->wiphy, "Failed to start external auth on vif %d",
                  ind->vif_idx);
        aml_send_sm_external_auth_required_rsp(aml_hw, aml_vif,
                                                WLAN_STATUS_UNSPECIFIED_FAILURE);
        return 0;
    }

    aml_external_auth_enable(aml_vif);
#else
    aml_send_sm_external_auth_required_rsp(aml_hw, aml_vif,
                                            WLAN_STATUS_UNSPECIFIED_FAILURE);
#endif
    return 0;
}

static inline int aml_rx_sm_ft_auth_ind(struct aml_hw *aml_hw,
                                         struct aml_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct sm_ft_auth_ind *ind = (struct sm_ft_auth_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct sk_buff *skb;
    size_t data_len = (offsetof(struct ieee80211_mgmt, u.auth.variable) +
                       ind->ft_ie_len);

    skb = dev_alloc_skb(data_len);
    if (skb) {
        struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb_put(skb, data_len);
        mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH);
        memcpy(mgmt->u.auth.variable, ind->ft_ie_buf, ind->ft_ie_len);
        aml_rx_defer_skb(aml_hw, aml_vif, skb);
        dev_kfree_skb(skb);
    } else {
        netdev_warn(aml_vif->ndev, "Allocation failed for FT auth ind\n");
    }

    return 0;
}

/***************************************************************************
 * Messages from TWT task
 **************************************************************************/
static inline int aml_rx_twt_setup_ind(struct aml_hw *aml_hw,
                                        struct aml_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    struct twt_setup_ind *ind = (struct twt_setup_ind *)msg->param;
    struct aml_sta *aml_sta = &aml_hw->sta_table[ind->sta_idx];

    AML_DBG(AML_FN_ENTRY_STR);

    memcpy(&aml_sta->twt_ind, ind, sizeof(struct twt_setup_ind));
    return 0;
}

static inline int aml_rx_mesh_path_create_cfm(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_path_create_cfm *cfm = (struct mesh_path_create_cfm *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[cfm->vif_idx];

    AML_DBG(AML_FN_ENTRY_STR);

    /* Check we well have a Mesh Point Interface */
    if (aml_vif && (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_MESH_POINT))
        aml_vif->ap.flags &= ~AML_AP_CREATE_MESH_PATH;

    return 0;
}

static inline int aml_rx_mesh_peer_update_ind(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_peer_update_ind *ind = (struct mesh_peer_update_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct aml_sta *aml_sta = &aml_hw->sta_table[ind->sta_idx];

    AML_DBG(AML_FN_ENTRY_STR);

    if ((ind->vif_idx >= NX_VIRT_DEV_MAX) ||
        (aml_vif && (AML_VIF_TYPE(aml_vif) != NL80211_IFTYPE_MESH_POINT)) ||
        (ind->sta_idx >= NX_REMOTE_STA_MAX))
        return 1;

    if (aml_vif && (aml_vif->ap.flags & AML_AP_USER_MESH_PM))
    {
        if (!ind->estab && aml_sta->valid) {
            /* There is no way to inform upper layer for lost of peer, still
               clean everything in the driver */
            aml_sta->ps.active = false;
            aml_sta->valid = false;

            /* Remove the station from the list of VIF's station */
            list_del_init(&aml_sta->list);

            aml_txq_sta_deinit(aml_hw, aml_sta);
            aml_dbgfs_unregister_sta(aml_hw, aml_sta);
        } else {
            WARN_ON(0);
        }
    } else {
        /* Check if peer link has been established or lost */
        if (ind->estab) {
            if (aml_vif && (!aml_sta->valid)) {
                u8 txq_status;

                aml_sta->valid = true;
                aml_sta->sta_idx = ind->sta_idx;
                aml_sta->ch_idx = aml_vif->ch_index;
                aml_sta->vif_idx = ind->vif_idx;
                aml_sta->vlan_idx = aml_sta->vif_idx;
                aml_sta->ps.active = false;
                aml_sta->qos = true;
                aml_sta->aid = ind->sta_idx + 1;
                //aml_sta->acm = ind->acm;
                memcpy(aml_sta->mac_addr, ind->peer_addr.array, ETH_ALEN);

                aml_chanctx_link(aml_vif, aml_sta->ch_idx, NULL);

                /* Add the station in the list of VIF's stations */
                INIT_LIST_HEAD(&aml_sta->list);
                list_add_tail(&aml_sta->list, &aml_vif->ap.sta_list);

                /* Initialize the TX queues */
                if (aml_sta->ch_idx == aml_hw->cur_chanctx) {
                    txq_status = 0;
                } else {
                    txq_status = AML_TXQ_STOP_CHAN;
                }

                aml_txq_sta_init(aml_hw, aml_sta, txq_status);
                aml_dbgfs_register_sta(aml_hw, aml_sta);

#ifdef CONFIG_AML_BFMER
                // TODO: update indication to contains vht capabilties
                if (aml_hw->mod_params->bfmer)
                    aml_send_bfmer_enable(aml_hw, aml_sta, NULL);

                aml_mu_group_sta_init(aml_sta, NULL);
#endif /* CONFIG_AML_BFMER */

            } else {
                WARN_ON(0);
            }
        } else {
            if (aml_sta->valid) {
                aml_sta->ps.active = false;
                aml_sta->valid = false;

                /* Remove the station from the list of VIF's station */
                list_del_init(&aml_sta->list);

                aml_txq_sta_deinit(aml_hw, aml_sta);
                aml_dbgfs_unregister_sta(aml_hw, aml_sta);
            } else {
                WARN_ON(0);
            }
        }
    }

    return 0;
}

static inline int aml_rx_mesh_path_update_ind(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_path_update_ind *ind = (struct mesh_path_update_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct aml_mesh_path *mesh_path;
    bool found = false;

    AML_DBG(AML_FN_ENTRY_STR);

    if (ind->vif_idx >= NX_VIRT_DEV_MAX)
        return 1;

    if (!aml_vif || (AML_VIF_TYPE(aml_vif) != NL80211_IFTYPE_MESH_POINT))
        return 0;

    /* Look for path with provided target address */
    list_for_each_entry(mesh_path, &aml_vif->ap.mpath_list, list) {
        if (mesh_path->path_idx == ind->path_idx) {
            found = true;
            break;
        }
    }

    /* Check if element has been deleted */
    if (ind->delete) {
        if (found) {
            trace_mesh_delete_path(mesh_path);
            /* Remove element from list */
            list_del_init(&mesh_path->list);
            /* Free the element */
            kfree(mesh_path);
        }
    }
    else {
        if (found) {
            // Update the Next Hop STA
            mesh_path->nhop_sta = &aml_hw->sta_table[ind->nhop_sta_idx];
            trace_mesh_update_path(mesh_path);
        } else {
            // Allocate a Mesh Path structure
            mesh_path = kmalloc(sizeof(struct aml_mesh_path), GFP_ATOMIC);

            if (mesh_path) {
                INIT_LIST_HEAD(&mesh_path->list);

                mesh_path->path_idx = ind->path_idx;
                mesh_path->nhop_sta = &aml_hw->sta_table[ind->nhop_sta_idx];
                memcpy(&mesh_path->tgt_mac_addr, &ind->tgt_mac_addr, MAC_ADDR_LEN);

                // Insert the path in the list of path
                list_add_tail(&mesh_path->list, &aml_vif->ap.mpath_list);
                trace_mesh_create_path(mesh_path);
            }
        }
    }
    /* coverity[leaked_storage] - mesh_path have added to list */
    return 0;
}

static inline int aml_rx_mesh_proxy_update_ind(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_proxy_update_ind *ind = (struct mesh_proxy_update_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct aml_mesh_proxy *mesh_proxy;
    bool found = false;

    AML_DBG(AML_FN_ENTRY_STR);

    if (ind->vif_idx >= NX_VIRT_DEV_MAX)
        return 1;

    if (!aml_vif || (AML_VIF_TYPE(aml_vif) != NL80211_IFTYPE_MESH_POINT))
        return 0;

    /* Look for path with provided external STA address */
    list_for_each_entry(mesh_proxy, &aml_vif->ap.proxy_list, list) {
        if (!memcmp(&ind->ext_sta_addr, &mesh_proxy->ext_sta_addr, ETH_ALEN)) {
            found = true;
            break;
        }
    }

    if (ind->delete && found) {
        /* Delete mesh path */
        list_del_init(&mesh_proxy->list);
        kfree(mesh_proxy);
    } else if (!ind->delete && !found) {
        /* Allocate a Mesh Path structure */
        mesh_proxy = (struct aml_mesh_proxy *)kmalloc(sizeof(*mesh_proxy),
                                                       GFP_ATOMIC);

        if (mesh_proxy) {
            INIT_LIST_HEAD(&mesh_proxy->list);

            memcpy(&mesh_proxy->ext_sta_addr, &ind->ext_sta_addr, MAC_ADDR_LEN);
            mesh_proxy->local = ind->local;

            if (!ind->local) {
                memcpy(&mesh_proxy->proxy_addr, &ind->proxy_mac_addr, MAC_ADDR_LEN);
            }

            /* Insert the path in the list of path */
            list_add_tail(&mesh_proxy->list, &aml_vif->ap.proxy_list);
        }
    }
    /* coverity[leaked_storage] - mesh_proxy have added to list */
    return 0;
}

/***************************************************************************
 * Messages from APM task
 **************************************************************************/
static inline int aml_rx_apm_probe_client_ind(struct aml_hw *aml_hw,
                                               struct aml_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct apm_probe_client_ind *ind = (struct apm_probe_client_ind *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[ind->vif_idx];
    struct aml_sta *aml_sta = &aml_hw->sta_table[ind->sta_idx];

    aml_sta->stats.last_act = jiffies;
    cfg80211_probe_status(aml_vif->ndev, aml_sta->mac_addr, (u64)ind->probe_id,
                          ind->client_present, 0, false, GFP_ATOMIC);

    return 0;
}

#endif /* CONFIG_AML_FULLMAC */

/***************************************************************************
 * Messages from DEBUG task
 **************************************************************************/
static inline int aml_rx_dbg_error_ind(struct aml_hw *aml_hw,
                                        struct aml_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    AML_DBG(AML_FN_ENTRY_STR);

    aml_error_ind(aml_hw);

    return 0;
}

static inline int aml_rx_rf_regvalue(struct aml_hw *aml_hw,
                                      struct aml_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct rf_read_result_ind *ind = (struct rf_read_result_ind *)msg->param;

    printk("%s %d: rf_regaddr:%x, rf_regvalue:%x\n", __func__, __LINE__,
           ind->rf_addr, ind->rf_data);

    return 0;
}

#ifdef TEST_MODE
#define RX_PAYLOAD_DESC_PATTERN 0xC0DEDBAD
static inline int aml_dma_ul_result_ind(struct aml_hw *aml_hw,
                                      struct aml_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct dma_ul_result_ind *ind = (struct dma_ul_result_ind *)msg->param;
    u32_l * read_addr;
    u8_l result = 1;
    u32_l read_cnt = 0;
    read_addr = (u32_l *)aml_hw->pcie_prssr_ul_addr;
    printk("%s %d: dma_ul_info: scr_addr: 0x%x, buffer_len:%d, payload:%x",
            __func__, __LINE__, ind->dest_addr, ind->len, ind->payload);

    printk("%s %d:,dest_addr: %x, upload: %x",__func__, __LINE__, ((u32_l)(read_addr)), (*read_addr));

    while (*(read_addr + (ind->len/4 -1)) != RX_PAYLOAD_DESC_PATTERN)
    {
        read_cnt++;
        if (read_cnt > 20480)
        {
            printk("%s %d: dma upload incomplete!",__func__, __LINE__);
            break;
        }
        msleep(1);
    }

    while (*(read_addr) != RX_PAYLOAD_DESC_PATTERN)
    {
        if (*(read_addr) != ind->payload)
        {
            printk("wrong dma_add:%x, wrong_payload:%x", read_addr, *(read_addr));
            result = false;
        }
        read_addr = read_addr + 1;
    }

    aml_ipc_buf_dealloc(aml_hw, &aml_hw->pcie_prssr_test);
    printk("pcie dma ul test_done,%s %d, result: %d", __func__, __LINE__,result);

    return 0;
}

static inline int aml_dma_dl_result_ind(struct aml_hw *aml_hw,
                                      struct aml_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct dma_dl_result_ind *ind = (struct dma_dl_result_ind *)msg->param;

    printk("%s %d: dma_dl_result:%x, start_addr: 0x%x, buffer_len:%d, payload:%x",
            __func__, __LINE__, ind->result, ind->start_addr, ind->len, ind->payload);

    aml_ipc_buf_dealloc(aml_hw, &aml_hw->pcie_prssr_test);
    printk("pcie dma dl test_done,%s %d", __func__, __LINE__);

    return 0;
}
#endif

static inline int aml_scanu_cancel_cfm(struct aml_hw *aml_hw,
                                      struct aml_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct scanu_cancel_cfm *cfm = (struct scanu_cancel_cfm *)msg->param;
    struct aml_vif *aml_vif = aml_hw->vif_table[cfm->vif_idx];

    printk("%s %d: status:%x, vif_id=%x\n", __func__, __LINE__, cfm->status, cfm->vif_idx);
    aml_ipc_buf_dealloc(aml_hw, &aml_hw->scan_ie);
    if (aml_hw->scan_request) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
        struct cfg80211_scan_info info = {
            .aborted = false,
        };

        cfg80211_scan_done(aml_hw->scan_request, &info);
#else
        cfg80211_scan_done(aml_hw->scan_request, false);
#endif
    }
    aml_hw->scan_request = NULL;
    if (aml_vif != NULL) {
        aml_vif->sta.cancel_scan_cfm = 1;
    }
    return 0;
}

static inline int aml_tko_conn_dead_ind(struct aml_hw *aml_hw,
                                    struct aml_cmd *cmd,
                                    struct ipc_e2a_msg *msg)
{
    struct tko_conn_dead_ind *ind = (struct tko_conn_dead_ind *)msg->param;

    printk("%s %d: vif_id=%u src_ip=%u src_port=%u dst_ip=%u dst_port=%u\n",
            __func__, __LINE__, ind->vif_idx, ind->src_ip, ind->src_port,
            ind->dst_ip, ind->dst_port);

    return 0;
}

static inline int aml_sched_scan_cfm(struct aml_hw *aml_hw,
                                      struct aml_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct scanu_start_cfm *cfm = (struct scanu_start_cfm *)msg->param;

    printk("%s %d: status:%x, vif_id=%x\n", __func__, __LINE__, cfm->status, cfm->vif_idx);

    if (cfm->result_cnt) {
        aml_cfg80211_sched_scan_results(aml_hw->wiphy, 0);
    }

    return 0;
}

static inline int aml_suspend_ind(struct aml_hw *aml_hw,
                                  struct aml_cmd *cmd,
                                  struct ipc_e2a_msg *msg)
{
    aml_hw->suspend_ind = 1;
    printk("%s %d\n", __func__, __LINE__);
    return 0;
}

static int aml_dhcp_offload_ind(struct aml_hw *aml_hw,
                                           struct aml_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    struct dhcp_to_host_ind *fw_event = (struct dhcp_to_host_ind *)msg->param;
    struct in_device *in_dev;
    struct in_ifaddr *ifa_v4;
    unsigned int local_ip = 0;
    struct aml_vif *aml_vif;

    if (fw_event == NULL) {
        return -1;
    }

    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif == NULL) {
          printk("%s %d\n", __func__, __LINE__);
        }

       if (!aml_vif->up || aml_vif->ndev == NULL) {
           continue;
       }
       if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_STATION) {
           break;
       }
    }

    if (aml_vif->ndev == NULL) {
        printk("%s %d\n", __func__, __LINE__);
        return -1;
    }

    local_ip = *(__be32 *)(&fw_event->local_ip);
    in_dev = __in_dev_get_rtnl(aml_vif->ndev);

    if (in_dev) {
        ifa_v4 = in_dev->ifa_list;
        if (ifa_v4 != NULL && local_ip != 0) {
            ifa_v4->ifa_local = local_ip;
            ifa_v4->ifa_address = *(__be32 *)(&fw_event->server_ip);
            ifa_v4->ifa_mask = *(__be32 *)(&fw_event->subnet_mask);
            printk("%s %x %x %x\n", __func__, ifa_v4->ifa_local,ifa_v4->ifa_address,ifa_v4->ifa_mask);
        }
    }

    return 0;
}

#ifdef CONFIG_AML_SOFTMAC

static msg_cb_fct mm_hdlrs[MSG_I(MM_MAX)] = {
    [MSG_I(MM_CONNECTION_LOSS_IND)]       = aml_rx_connection_loss_ind,
    [MSG_I(MM_CHANNEL_SWITCH_IND)]        = aml_rx_chan_switch_ind,
    [MSG_I(MM_CHANNEL_PRE_SWITCH_IND)]    = aml_rx_chan_pre_switch_ind,
    [MSG_I(MM_REMAIN_ON_CHANNEL_EXP_IND)] = aml_rx_remain_on_channel_exp_ind,
#ifdef CONFIG_AML_BCN
    [MSG_I(MM_PRIMARY_TBTT_IND)]          = aml_rx_prm_tbtt_ind,
#endif
    [MSG_I(MM_P2P_VIF_PS_CHANGE_IND)]     = aml_rx_p2p_vif_ps_change_ind,
    [MSG_I(MM_CSA_COUNTER_IND)]           = aml_rx_csa_counter_ind,
    [MSG_I(MM_CHANNEL_SURVEY_IND)]        = aml_rx_channel_survey_ind,
    [MSG_I(MM_RSSI_STATUS_IND)]           = aml_rx_rssi_status_ind,
};

static msg_cb_fct scan_hdlrs[MSG_I(SCAN_MAX)] = {
    [MSG_I(SCAN_DONE_IND)]                = aml_rx_scan_done_ind,
};

#else  /* CONFIG_AML_FULLMAC */

static msg_cb_fct mm_hdlrs[MSG_I(MM_MAX)] = {
    [MSG_I(MM_CHANNEL_SWITCH_IND)]     = aml_rx_chan_switch_ind,
    [MSG_I(MM_CHANNEL_PRE_SWITCH_IND)] = aml_rx_chan_pre_switch_ind,
    [MSG_I(MM_REMAIN_ON_CHANNEL_EXP_IND)] = aml_rx_remain_on_channel_exp_ind,
    [MSG_I(MM_PS_CHANGE_IND)]          = aml_rx_ps_change_ind,
    [MSG_I(MM_TRAFFIC_REQ_IND)]        = aml_rx_traffic_req_ind,
    [MSG_I(MM_P2P_VIF_PS_CHANGE_IND)]  = aml_rx_p2p_vif_ps_change_ind,
    [MSG_I(MM_CSA_COUNTER_IND)]        = aml_rx_csa_counter_ind,
    [MSG_I(MM_CSA_FINISH_IND)]         = aml_rx_csa_finish_ind,
    [MSG_I(MM_CSA_TRAFFIC_IND)]        = aml_rx_csa_traffic_ind,
    [MSG_I(MM_CHANNEL_SURVEY_IND)]     = aml_rx_channel_survey_ind,
    [MSG_I(MM_P2P_NOA_UPD_IND)]        = aml_rx_p2p_noa_upd_ind,
    [MSG_I(MM_RSSI_STATUS_IND)]        = aml_rx_rssi_status_ind,
    [MSG_I(MM_PKTLOSS_IND)]            = aml_rx_pktloss_notify_ind,
};

static msg_cb_fct scan_hdlrs[MSG_I(SCANU_MAX)] = {
    [MSG_I(SCANU_START_CFM)]           = aml_rx_scanu_start_cfm,
    [MSG_I(SCANU_RESULT_IND)]          = aml_rx_scanu_result_ind,
};

static msg_cb_fct me_hdlrs[MSG_I(ME_MAX)] = {
    [MSG_I(ME_TKIP_MIC_FAILURE_IND)] = aml_rx_me_tkip_mic_failure_ind,
    [MSG_I(ME_TX_CREDITS_UPDATE_IND)] = aml_rx_me_tx_credits_update_ind,
};

static msg_cb_fct sm_hdlrs[MSG_I(SM_MAX)] = {
    [MSG_I(SM_CONNECT_IND)]    = aml_rx_sm_connect_ind,
    [MSG_I(SM_DISCONNECT_IND)] = aml_rx_sm_disconnect_ind,
    [MSG_I(SM_EXTERNAL_AUTH_REQUIRED_IND)] = aml_rx_sm_external_auth_required_ind,
    [MSG_I(SM_FT_AUTH_IND)] = aml_rx_sm_ft_auth_ind,
};

static msg_cb_fct apm_hdlrs[MSG_I(APM_MAX)] = {
    [MSG_I(APM_PROBE_CLIENT_IND)] = aml_rx_apm_probe_client_ind,
};

static msg_cb_fct twt_hdlrs[MSG_I(TWT_MAX)] = {
    [MSG_I(TWT_SETUP_IND)]    = aml_rx_twt_setup_ind,
};

static msg_cb_fct mesh_hdlrs[MSG_I(MESH_MAX)] = {
    [MSG_I(MESH_PATH_CREATE_CFM)]  = aml_rx_mesh_path_create_cfm,
    [MSG_I(MESH_PEER_UPDATE_IND)]  = aml_rx_mesh_peer_update_ind,
    [MSG_I(MESH_PATH_UPDATE_IND)]  = aml_rx_mesh_path_update_ind,
    [MSG_I(MESH_PROXY_UPDATE_IND)] = aml_rx_mesh_proxy_update_ind,
};

#endif /* CONFIG_AML_SOFTMAC */

static msg_cb_fct dbg_hdlrs[MSG_I(DBG_MAX)] = {
    [MSG_I(DBG_ERROR_IND)]                = aml_rx_dbg_error_ind,
};

static msg_cb_fct tdls_hdlrs[MSG_I(TDLS_MAX)] = {
    [MSG_I(TDLS_CHAN_SWITCH_CFM)] = aml_rx_tdls_chan_switch_cfm,
    [MSG_I(TDLS_CHAN_SWITCH_IND)] = aml_rx_tdls_chan_switch_ind,
    [MSG_I(TDLS_CHAN_SWITCH_BASE_IND)] = aml_rx_tdls_chan_switch_base_ind,
    [MSG_I(TDLS_PEER_PS_IND)] = aml_rx_tdls_peer_ps_ind,
};

static msg_cb_fct priv_hdlrs[MSG_I(TASK_PRIV)] = {
    [MSG_I(PRIV_SCANU_CANCEL_CFM)]  = aml_scanu_cancel_cfm,
    [MSG_I(PRIV_TKO_CONN_DEAD_IND)] = aml_tko_conn_dead_ind,
    [MSG_I(PRIV_SCHED_SCAN_CFM)]    = aml_sched_scan_cfm,
    [MSG_I(PRIV_DHCP_OFFLOAD_IND)]  = aml_dhcp_offload_ind,
#ifdef TEST_MODE
    [MSG_I(PRIV_DMA_UL_RESULT)]     = aml_dma_ul_result_ind,
    [MSG_I(PRIV_DMA_DL_RESULT)]     = aml_dma_dl_result_ind,
#endif
    [MSG_I(PRIV_CONNECT_INFO)]      = aml_rx_sm_connect_ind_ex,
    [MSG_I(PRIV_SET_SUSPEND_IND)]   = aml_suspend_ind,
    [MSG_I(PRIV_APM_DIS_STA_IND)]   = aml_apm_handle_disconnect_sta,
};

static msg_cb_fct *msg_hdlrs[] = {
    [TASK_MM]    = mm_hdlrs,
    [TASK_PRIV]  = priv_hdlrs,
    [TASK_DBG]   = dbg_hdlrs,
#ifdef CONFIG_AML_SOFTMAC
    [TASK_SCAN]  = scan_hdlrs,
    [TASK_TDLS]  = tdls_hdlrs,
#else
    [TASK_TDLS]  = tdls_hdlrs,
    [TASK_SCANU] = scan_hdlrs,
    [TASK_ME]    = me_hdlrs,
    [TASK_SM]    = sm_hdlrs,
    [TASK_APM]   = apm_hdlrs,
    [TASK_MESH]  = mesh_hdlrs,
    [TASK_TWT]   = twt_hdlrs,
#endif /* CONFIG_AML_SOFTMAC */
};

/**
 *
 */
void aml_rx_handle_msg(struct aml_hw *aml_hw, struct ipc_e2a_msg *msg)
{
    aml_hw->cmd_mgr.msgind(&aml_hw->cmd_mgr, msg,
                            msg_hdlrs[MSG_T(msg->id)][MSG_I(msg->id)]);
}