/**
 ****************************************************************************************
 *
 * @file rwnx_msg_rx.c
 *
 * @brief RX function definitions
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ****************************************************************************************
 */
#include "rwnx_defs.h"
#include "rwnx_prof.h"
#include "rwnx_tx.h"
#ifdef CONFIG_RWNX_BFMER
#include "rwnx_bfmer.h"
#endif //(CONFIG_RWNX_BFMER)
#ifdef CONFIG_RWNX_FULLMAC
#include "rwnx_debugfs.h"
#include "rwnx_msg_tx.h"
#include "rwnx_tdls.h"
#endif /* CONFIG_RWNX_FULLMAC */
#include "rwnx_events.h"
#include "rwnx_compat.h"

static int rwnx_freq_to_idx(struct rwnx_hw *rwnx_hw, int freq)
{
    struct ieee80211_supported_band *sband;
    int band, ch, idx = 0;

    for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
#ifdef CONFIG_RWNX_SOFTMAC
        sband = rwnx_hw->hw->wiphy->bands[band];
#else
        sband = rwnx_hw->wiphy->bands[band];
#endif /* CONFIG_RWNX_SOFTMAC */
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
static inline int rwnx_rx_chan_pre_switch_ind(struct rwnx_hw *rwnx_hw,
                                              struct rwnx_cmd *cmd,
                                              struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_RWNX_SOFTMAC
    struct rwnx_chanctx *chan_ctxt;
#endif
    struct rwnx_vif *rwnx_vif;
    int chan_idx = ((struct mm_channel_pre_switch_ind *)msg->param)->chan_index;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    REG_SW_SET_PROFILING_CHAN(rwnx_hw, SW_PROF_CHAN_CTXT_PSWTCH_BIT);

#ifdef CONFIG_RWNX_SOFTMAC
    list_for_each_entry(chan_ctxt, &rwnx_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = false;
            break;
        }
    }

    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->chanctx && (rwnx_vif->chanctx->index == chan_idx)) {
            rwnx_txq_vif_stop(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
        }
    }
#else
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->up && rwnx_vif->ch_index == chan_idx) {
            rwnx_txq_vif_stop(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
        }
    }
#endif /* CONFIG_RWNX_SOFTMAC */

    REG_SW_CLEAR_PROFILING_CHAN(rwnx_hw, SW_PROF_CHAN_CTXT_PSWTCH_BIT);

    return 0;
}

static inline int rwnx_rx_chan_switch_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_RWNX_SOFTMAC
    struct rwnx_chanctx *chan_ctxt;
    struct rwnx_sta *rwnx_sta;
#endif
    struct rwnx_vif *rwnx_vif;
    int chan_idx = ((struct mm_channel_switch_ind *)msg->param)->chan_index;
    bool roc_req = ((struct mm_channel_switch_ind *)msg->param)->roc;
    bool roc_tdls = ((struct mm_channel_switch_ind *)msg->param)->roc_tdls;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    REG_SW_SET_PROFILING_CHAN(rwnx_hw, SW_PROF_CHAN_CTXT_SWTCH_BIT);

#ifdef CONFIG_RWNX_SOFTMAC
    if (roc_tdls) {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;
        // Enable traffic only for TDLS station
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (rwnx_vif->vif_index == vif_index) {
                list_for_each_entry(rwnx_sta, &rwnx_vif->stations, list) {
                    if (rwnx_sta->tdls.active) {
                        rwnx_vif->roc_tdls = true;
                        rwnx_txq_tdls_sta_start(rwnx_sta, RWNX_TXQ_STOP_CHAN, rwnx_hw);
                        break;
                    }
                }
                break;
            }
        }
    } else if (!roc_req) {
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (rwnx_vif->chanctx && (rwnx_vif->chanctx->index == chan_idx)) {
                rwnx_txq_vif_start(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            }
        }
    } else {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;

        // Inform the host that the offchannel period has been started
        ieee80211_ready_on_channel(rwnx_hw->hw);

        // Enable traffic for associated VIF (roc may happen without chanctx)
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (rwnx_vif->vif_index == vif_index) {
                rwnx_txq_vif_start(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            }
        }
    }

    /* keep cur_chan up to date */
    list_for_each_entry(chan_ctxt, &rwnx_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = true;
            rwnx_hw->cur_freq = chan_ctxt->ctx->def.center_freq1;
            rwnx_hw->cur_band = chan_ctxt->ctx->def.chan->band;
            if (chan_ctxt->ctx->def.chan->flags & IEEE80211_CHAN_RADAR) {
                rwnx_radar_detection_enable(&rwnx_hw->radar,
                                            RWNX_RADAR_DETECT_REPORT,
                                            RWNX_RADAR_RIU);
            } else {
                rwnx_radar_detection_enable(&rwnx_hw->radar,
                                            RWNX_RADAR_DETECT_DISABLE,
                                            RWNX_RADAR_RIU);
            }
            break;
        }
    }

#else
    if (roc_tdls) {
        u8 vif_index = ((struct mm_channel_switch_ind *)msg->param)->vif_index;
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (rwnx_vif->vif_index == vif_index) {
                rwnx_vif->roc_tdls = true;
                rwnx_txq_tdls_sta_start(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            }
        }
    } else if (!roc_req) {
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (rwnx_vif->up && rwnx_vif->ch_index == chan_idx) {
                rwnx_txq_vif_start(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            }
        }
    } else {
        struct rwnx_roc *roc = rwnx_hw->roc;
        rwnx_vif = roc->vif;

        trace_switch_roc(rwnx_vif->vif_index);

        if (!roc->internal) {
            // If RoC has been started by the user space, inform it that we have
            // switched on the requested off-channel
            cfg80211_ready_on_channel(&rwnx_vif->wdev, (u64)(roc),
                                      roc->chan, roc->duration, GFP_ATOMIC);
        }

        // Keep in mind that we have switched on the channel
        roc->on_chan = true;
        // Enable traffic on OFF channel queue
        rwnx_txq_offchan_start(rwnx_hw);
    }

    rwnx_hw->cur_chanctx = chan_idx;
    rwnx_radar_detection_enable_on_cur_channel(rwnx_hw);

#endif /* CONFIG_RWNX_SOFTMAC */

    REG_SW_CLEAR_PROFILING_CHAN(rwnx_hw, SW_PROF_CHAN_CTXT_SWTCH_BIT);

    return 0;
}

static inline int rwnx_rx_tdls_chan_switch_cfm(struct rwnx_hw *rwnx_hw,
                                                struct rwnx_cmd *cmd,
                                                struct ipc_e2a_msg *msg)
{
    return 0;
}

static inline int rwnx_rx_tdls_chan_switch_ind(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_RWNX_SOFTMAC
    struct rwnx_chanctx *chan_ctxt;
    u8 chan_idx = ((struct tdls_chan_switch_ind *)msg->param)->chan_ctxt_index;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    // Enable channel context
    list_for_each_entry(chan_ctxt, &rwnx_hw->chan_ctxts, list) {
        if (chan_ctxt->index == chan_idx) {
            chan_ctxt->active = true;
            rwnx_hw->cur_freq = chan_ctxt->ctx->def.center_freq1;
            rwnx_hw->cur_band = chan_ctxt->ctx->def.chan->band;
        }
    }

    return 0;
#else
    // Enable traffic on OFF channel queue
    rwnx_txq_offchan_start(rwnx_hw);

    return 0;
#endif
}

static inline int rwnx_rx_tdls_chan_switch_base_ind(struct rwnx_hw *rwnx_hw,
                                                    struct rwnx_cmd *cmd,
                                                    struct ipc_e2a_msg *msg)
{
    struct rwnx_vif *rwnx_vif;
    u8 vif_index = ((struct tdls_chan_switch_base_ind *)msg->param)->vif_index;
#ifdef CONFIG_RWNX_SOFTMAC
    struct rwnx_sta *rwnx_sta;
#endif

    RWNX_DBG(RWNX_FN_ENTRY_STR);

#ifdef CONFIG_RWNX_SOFTMAC
    // Disable traffic for associated VIF
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == vif_index) {
            if (rwnx_vif->chanctx)
                rwnx_vif->chanctx->active = false;
            list_for_each_entry(rwnx_sta, &rwnx_vif->stations, list) {
                if (rwnx_sta->tdls.active) {
                    rwnx_vif->roc_tdls = false;
                    rwnx_txq_tdls_sta_stop(rwnx_sta, RWNX_TXQ_STOP_CHAN, rwnx_hw);
                    break;
                }
            }
            break;
        }
    }
    return 0;
#else
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == vif_index) {
            rwnx_vif->roc_tdls = false;
            rwnx_txq_tdls_sta_stop(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
        }
    }
    return 0;
#endif
}

static inline int rwnx_rx_tdls_peer_ps_ind(struct rwnx_hw *rwnx_hw,
                                           struct rwnx_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    struct rwnx_vif *rwnx_vif;
    u8 vif_index = ((struct tdls_peer_ps_ind *)msg->param)->vif_index;
    bool ps_on = ((struct tdls_peer_ps_ind *)msg->param)->ps_on;

#ifdef CONFIG_RWNX_SOFTMAC
    u8 sta_idx = ((struct tdls_peer_ps_ind *)msg->param)->sta_idx;
    struct rwnx_sta *rwnx_sta;
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == vif_index) {
            list_for_each_entry(rwnx_sta, &rwnx_vif->stations, list) {
                if (rwnx_sta->sta_idx == sta_idx) {
                    rwnx_sta->tdls.ps_on = ps_on;
                    if (ps_on) {
                        // disable TXQ for TDLS peer
                        rwnx_txq_tdls_sta_stop(rwnx_sta, RWNX_TXQ_STOP_STA_PS, rwnx_hw);
                    } else {
                        // Enable TXQ for TDLS peer
                        rwnx_txq_tdls_sta_start(rwnx_sta, RWNX_TXQ_STOP_STA_PS, rwnx_hw);
                    }
                    break;
                }
            }
            break;
        }
    }
    return 0;
#else
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == vif_index) {
            rwnx_vif->sta.tdls_sta->tdls.ps_on = ps_on;
            // Update PS status for the TDLS station
            rwnx_ps_bh_enable(rwnx_hw, rwnx_vif->sta.tdls_sta, ps_on);
        }
    }

    return 0;
#endif
}

static inline int rwnx_rx_remain_on_channel_exp_ind(struct rwnx_hw *rwnx_hw,
                                                    struct rwnx_cmd *cmd,
                                                    struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_RWNX_SOFTMAC
    struct rwnx_vif *rwnx_vif;
    u8 vif_index = ((struct mm_remain_on_channel_exp_ind *)msg->param)->vif_index;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    ieee80211_remain_on_channel_expired(rwnx_hw->hw);

    // Disable traffic for associated VIF
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == vif_index) {
            if (rwnx_vif->chanctx)
                rwnx_vif->chanctx->active = false;

            rwnx_txq_vif_stop(rwnx_vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            break;
        }
    }

    return 0;

#else
    struct rwnx_roc *roc = rwnx_hw->roc;
    struct rwnx_vif *rwnx_vif = roc->vif;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    trace_roc_exp(rwnx_vif->vif_index);

    if (!roc->internal && roc->on_chan) {
        // If RoC has been started by the user space and hasn't been cancelled,
        // inform it that off-channel period has expired
        cfg80211_remain_on_channel_expired(&rwnx_vif->wdev, (u64)(roc),
                                           roc->chan, GFP_ATOMIC);
    }

    rwnx_txq_offchan_deinit(rwnx_vif);

    kfree(roc);
    rwnx_hw->roc = NULL;

#endif /* CONFIG_RWNX_SOFTMAC */
    return 0;
}

static inline int rwnx_rx_p2p_vif_ps_change_ind(struct rwnx_hw *rwnx_hw,
                                                struct rwnx_cmd *cmd,
                                                struct ipc_e2a_msg *msg)
{
    int vif_idx  = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->vif_index;
    int ps_state = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->ps_state;
    struct rwnx_vif *vif_entry;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

#ifdef CONFIG_RWNX_SOFTMAC
    // Look for VIF entry
    list_for_each_entry(vif_entry, &rwnx_hw->vifs, list) {
        if (vif_entry->vif_index == vif_idx) {
            goto found_vif;
        }
    }
#else
    vif_entry = rwnx_hw->vif_table[vif_idx];

    if (vif_entry) {
        goto found_vif;
    }
#endif /* CONFIG_RWNX_SOFTMAC */

    goto exit;

found_vif:

#ifdef CONFIG_RWNX_SOFTMAC
    if (ps_state == MM_PS_MODE_OFF) {
        rwnx_txq_vif_start(vif_entry, RWNX_TXQ_STOP_VIF_PS, rwnx_hw);
    }
    else {
        rwnx_txq_vif_stop(vif_entry, RWNX_TXQ_STOP_VIF_PS, rwnx_hw);
    }
#else
    if (ps_state == MM_PS_MODE_OFF) {
        // Start TX queues for provided VIF
        rwnx_txq_vif_start(vif_entry, RWNX_TXQ_STOP_VIF_PS, rwnx_hw);
    }
    else {
        // Stop TX queues for provided VIF
        rwnx_txq_vif_stop(vif_entry, RWNX_TXQ_STOP_VIF_PS, rwnx_hw);
    }
#endif /* CONFIG_RWNX_SOFTMAC */

exit:
    return 0;
}

static inline int rwnx_rx_channel_survey_ind(struct rwnx_hw *rwnx_hw,
                                             struct rwnx_cmd *cmd,
                                             struct ipc_e2a_msg *msg)
{
    struct mm_channel_survey_ind *ind = (struct mm_channel_survey_ind *)msg->param;
    // Get the channel index
    int idx = rwnx_freq_to_idx(rwnx_hw, ind->freq);
    // Get the survey
    struct rwnx_survey_info *rwnx_survey;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (idx >  ARRAY_SIZE(rwnx_hw->survey))
        return 0;

    rwnx_survey = &rwnx_hw->survey[idx];

    // Store the received parameters
    rwnx_survey->chan_time_ms = ind->chan_time_ms;
    rwnx_survey->chan_time_busy_ms = ind->chan_time_busy_ms;
    rwnx_survey->noise_dbm = ind->noise_dbm;
    rwnx_survey->filled = (SURVEY_INFO_TIME |
                           SURVEY_INFO_TIME_BUSY);

    if (ind->noise_dbm != 0) {
        rwnx_survey->filled |= SURVEY_INFO_NOISE_DBM;
    }

    return 0;
}

static inline int rwnx_rx_p2p_noa_upd_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    return 0;
}

static inline int rwnx_rx_rssi_status_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_rssi_status_ind *ind = (struct mm_rssi_status_ind *)msg->param;
    int vif_idx  = ind->vif_index;
    bool rssi_status = ind->rssi_status;

    struct rwnx_vif *vif_entry;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

#ifdef CONFIG_RWNX_SOFTMAC
    list_for_each_entry(vif_entry, &rwnx_hw->vifs, list) {
        if (vif_entry->vif_index == vif_idx) {
            ieee80211_cqm_rssi_notify(vif_entry->vif,
                                      rssi_status ? NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
                                                    NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
                                      ind->rssi, GFP_ATOMIC);
        }
    }
#else
    vif_entry = rwnx_hw->vif_table[vif_idx];
    if (vif_entry) {
        cfg80211_cqm_rssi_notify(vif_entry->ndev,
                                 rssi_status ? NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
                                               NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
                                 ind->rssi, GFP_ATOMIC);
    }
#endif /* CONFIG_RWNX_SOFTMAC */

    return 0;
}

static inline int rwnx_rx_pktloss_notify_ind(struct rwnx_hw *rwnx_hw,
                                             struct rwnx_cmd *cmd,
                                             struct ipc_e2a_msg *msg)
{
#ifdef CONFIG_RWNX_FULLMAC
    struct mm_pktloss_ind *ind = (struct mm_pktloss_ind *)msg->param;
    struct rwnx_vif *vif_entry;
    int vif_idx  = ind->vif_index;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    vif_entry = rwnx_hw->vif_table[vif_idx];
    if (vif_entry) {
        cfg80211_cqm_pktloss_notify(vif_entry->ndev, (const u8 *)ind->mac_addr.array,
                                    ind->num_packets, GFP_ATOMIC);
    }
#endif /* CONFIG_RWNX_FULLMAC */

    return 0;
}

static inline int rwnx_rx_csa_counter_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_csa_counter_ind *ind = (struct mm_csa_counter_ind *)msg->param;
    struct rwnx_vif *vif;
    bool found = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &rwnx_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
#ifdef CONFIG_RWNX_SOFTMAC
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

#ifdef CONFIG_RWNX_SOFTMAC
static inline int rwnx_rx_connection_loss_ind(struct rwnx_hw *rwnx_hw,
                                              struct rwnx_cmd *cmd,
                                              struct ipc_e2a_msg *msg)
{
    struct rwnx_vif *rwnx_vif;
    u8 inst_nbr;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    inst_nbr = ((struct mm_connection_loss_ind *)msg->param)->inst_nbr;

    /* Search the VIF entry corresponding to the instance number */
    list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
        if (rwnx_vif->vif_index == inst_nbr) {
            ieee80211_connection_loss(rwnx_vif->vif);
            break;
        }
    }

    return 0;
}


#ifdef CONFIG_RWNX_BCN
static inline int rwnx_rx_prm_tbtt_ind(struct rwnx_hw *rwnx_hw,
                                       struct rwnx_cmd *cmd,
                                       struct ipc_e2a_msg *msg)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_tx_bcns(rwnx_hw);

    return 0;
}
#endif

#else /* !CONFIG_RWNX_SOFTMAC */
static inline int rwnx_rx_csa_finish_ind(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct mm_csa_finish_ind *ind = (struct mm_csa_finish_ind *)msg->param;
    struct rwnx_vif *vif;
    bool found = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &rwnx_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
        if (RWNX_VIF_TYPE(vif) == NL80211_IFTYPE_AP ||
            RWNX_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO) {
            if (vif->ap.csa) {
                vif->ap.csa->status = ind->status;
                vif->ap.csa->ch_idx = ind->chan_idx;
                schedule_work(&vif->ap.csa->work);
            } else
                netdev_err(vif->ndev, "CSA finish indication but no active CSA");
        } else {
            if (ind->status == 0) {
                rwnx_chanctx_unlink(vif);
                rwnx_chanctx_link(vif, ind->chan_idx, NULL);
                if (rwnx_hw->cur_chanctx == ind->chan_idx) {
                    rwnx_radar_detection_enable_on_cur_channel(rwnx_hw);
                    rwnx_txq_vif_start(vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
                } else
                    rwnx_txq_vif_stop(vif, RWNX_TXQ_STOP_CHAN, rwnx_hw);
            }
        }
    }

    return 0;
}

static inline int rwnx_rx_csa_traffic_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_csa_traffic_ind *ind = (struct mm_csa_traffic_ind *)msg->param;
    struct rwnx_vif *vif;
    bool found = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    // Look for VIF entry
    list_for_each_entry(vif, &rwnx_hw->vifs, list) {
        if (vif->vif_index == ind->vif_index) {
            found=true;
            break;
        }
    }

    if (found) {
        if (ind->enable)
            rwnx_txq_vif_start(vif, RWNX_TXQ_STOP_CSA, rwnx_hw);
        else
            rwnx_txq_vif_stop(vif, RWNX_TXQ_STOP_CSA, rwnx_hw);
    }

    return 0;
}

static inline int rwnx_rx_ps_change_ind(struct rwnx_hw *rwnx_hw,
                                        struct rwnx_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    struct mm_ps_change_ind *ind = (struct mm_ps_change_ind *)msg->param;
    struct rwnx_sta *sta = &rwnx_hw->sta_table[ind->sta_idx];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (ind->sta_idx >= (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) {
        wiphy_err(rwnx_hw->wiphy, "Invalid sta index reported by fw %d\n",
                  ind->sta_idx);
        return 1;
    }

    netdev_dbg(rwnx_hw->vif_table[sta->vif_idx]->ndev,
               "Sta %d, change PS mode to %s", sta->sta_idx,
               ind->ps_state ? "ON" : "OFF");

    if (sta->valid) {
        rwnx_ps_bh_enable(rwnx_hw, sta, ind->ps_state);
    } else if (test_bit(RWNX_DEV_ADDING_STA, (void*)&rwnx_hw->flags)) {
        sta->ps.active = ind->ps_state ? true : false;
    } else {
        netdev_err(rwnx_hw->vif_table[sta->vif_idx]->ndev,
                   "Ignore PS mode change on invalid sta\n");
    }

    return 0;
}


static inline int rwnx_rx_traffic_req_ind(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_traffic_req_ind *ind = (struct mm_traffic_req_ind *)msg->param;
    struct rwnx_sta *sta = &rwnx_hw->sta_table[ind->sta_idx];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    netdev_dbg(rwnx_hw->vif_table[sta->vif_idx]->ndev,
               "Sta %d, asked for %d pkt", sta->sta_idx, ind->pkt_cnt);

    rwnx_ps_bh_traffic_req(rwnx_hw, sta, ind->pkt_cnt,
                           ind->uapsd ? UAPSD_ID : LEGACY_PS_ID);

    return 0;
}
#endif /* CONFIG_RWNX_SOFTMAC */

/***************************************************************************
 * Messages from SCAN task
 **************************************************************************/
#ifdef CONFIG_RWNX_SOFTMAC
static inline int rwnx_rx_scan_done_ind(struct rwnx_hw *rwnx_hw,
                                        struct rwnx_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    struct cfg80211_scan_info info = {
        .aborted = false,
    };
#endif
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_ipc_buf_dealloc(rwnx_hw, &rwnx_hw->scan_ie);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    ieee80211_scan_completed(rwnx_hw->hw, &info);
#else
    ieee80211_scan_completed(rwnx_hw->hw, false);
#endif

    return 0;
}
#endif /* CONFIG_RWNX_SOFTMAC */

/***************************************************************************
 * Messages from SCANU task
 **************************************************************************/
#ifdef CONFIG_RWNX_FULLMAC
static inline int rwnx_rx_scanu_start_cfm(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_ipc_buf_dealloc(rwnx_hw, &rwnx_hw->scan_ie);
    if (rwnx_hw->scan_request) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
        struct cfg80211_scan_info info = {
            .aborted = false,
        };

        cfg80211_scan_done(rwnx_hw->scan_request, &info);
#else
        cfg80211_scan_done(rwnx_hw->scan_request, false);
#endif
    }

    rwnx_hw->scan_request = NULL;

    return 0;
}

static inline int rwnx_rx_scanu_result_ind(struct rwnx_hw *rwnx_hw,
                                           struct rwnx_cmd *cmd,
                                           struct ipc_e2a_msg *msg)
{
    struct cfg80211_bss *bss = NULL;
    struct ieee80211_channel *chan;
    struct scanu_result_ind *ind = (struct scanu_result_ind *)msg->param;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    chan = ieee80211_get_channel(rwnx_hw->wiphy, ind->center_freq);

    if (chan != NULL)
        bss = cfg80211_inform_bss_frame(rwnx_hw->wiphy, chan,
                                        (struct ieee80211_mgmt *)ind->payload,
                                        ind->length, ind->rssi * 100, GFP_ATOMIC);

    if (bss != NULL)
        cfg80211_put_bss(rwnx_hw->wiphy, bss);

    return 0;
}
#endif /* CONFIG_RWNX_FULLMAC */

/***************************************************************************
 * Messages from ME task
 **************************************************************************/
#ifdef CONFIG_RWNX_FULLMAC
static inline int rwnx_rx_me_tkip_mic_failure_ind(struct rwnx_hw *rwnx_hw,
                                                  struct rwnx_cmd *cmd,
                                                  struct ipc_e2a_msg *msg)
{
    struct me_tkip_mic_failure_ind *ind = (struct me_tkip_mic_failure_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct net_device *dev = rwnx_vif->ndev;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    cfg80211_michael_mic_failure(dev, (u8 *)&ind->addr, (ind->ga?NL80211_KEYTYPE_GROUP:
                                 NL80211_KEYTYPE_PAIRWISE), ind->keyid,
                                 (u8 *)&ind->tsc, GFP_ATOMIC);

    return 0;
}

static inline int rwnx_rx_me_tx_credits_update_ind(struct rwnx_hw *rwnx_hw,
                                                   struct rwnx_cmd *cmd,
                                                   struct ipc_e2a_msg *msg)
{
    struct me_tx_credits_update_ind *ind = (struct me_tx_credits_update_ind *)msg->param;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_txq_credit_update(rwnx_hw, ind->sta_idx, ind->tid, ind->credits);

    return 0;
}
#endif /* CONFIG_RWNX_FULLMAC */

/***************************************************************************
 * Messages from SM task
 **************************************************************************/
#ifdef CONFIG_RWNX_FULLMAC
static inline int rwnx_rx_sm_connect_ind(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct sm_connect_ind *ind = (struct sm_connect_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct net_device *dev = rwnx_vif->ndev;
    const u8 *req_ie, *rsp_ie;
    const u8 *extcap_ie;
    const struct ieee_types_extcap *extcap;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* Retrieve IE addresses and lengths */
    req_ie = (const u8 *)ind->assoc_ie_buf;
    rsp_ie = req_ie + ind->assoc_req_ie_len;

    // Fill-in the AP information
    if (ind->status_code == 0)
    {
        struct rwnx_sta *sta = &rwnx_hw->sta_table[ind->ap_idx];
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
        rwnx_vif->sta.ap = sta;
        rwnx_vif->generation++;
        chan = ieee80211_get_channel(rwnx_hw->wiphy, ind->chan.prim20_freq);
        cfg80211_chandef_create(&chandef, chan, NL80211_CHAN_NO_HT);
        if (!rwnx_hw->mod_params->ht_on)
            chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
        else
            chandef.width = (enum nl80211_chan_width)chnl2bw[ind->chan.type];
        chandef.center_freq1 = ind->chan.center1_freq;
        chandef.center_freq2 = ind->chan.center2_freq;
        rwnx_chanctx_link(rwnx_vif, ind->ch_idx, &chandef);
        memcpy(sta->mac_addr, ind->bssid.array, ETH_ALEN);
        if (ind->ch_idx == rwnx_hw->cur_chanctx) {
            txq_status = 0;
        } else {
            txq_status = RWNX_TXQ_STOP_CHAN;
        }
        memcpy(sta->ac_param, ind->ac_param, sizeof(sta->ac_param));
        rwnx_txq_sta_init(rwnx_hw, sta, txq_status);
        rwnx_dbgfs_register_sta(rwnx_hw, sta);
        rwnx_txq_tdls_vif_init(rwnx_vif);
        rwnx_mu_group_sta_init(sta, NULL);
        /* Look for TDLS Channel Switch Prohibited flag in the Extended Capability
         * Information Element*/
        extcap_ie = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, rsp_ie, ind->assoc_rsp_ie_len);
        if (extcap_ie && extcap_ie[1] >= 5) {
            extcap = (void *)(extcap_ie);
            rwnx_vif->tdls_chsw_prohibited = extcap->ext_capab[4] & WLAN_EXT_CAPA5_TDLS_CH_SW_PROHIBITED;
        }

#ifdef CONFIG_RWNX_BFMER
        /* If Beamformer feature is activated, check if features can be used
         * with the new peer device
         */
        if (rwnx_hw->mod_params->bfmer) {
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
                rwnx_send_bfmer_enable(rwnx_hw, sta, vht_cap);
            } while (0);
        }
#endif //(CONFIG_RWNX_BFMER)

#ifdef CONFIG_RWNX_MON_DATA
        // If there are 1 sta and 1 monitor interface active at the same time then
        // monitor interface channel context is always the same as the STA interface.
        // This doesn't work with 2 STA interfaces but we don't want to support it.
        if (rwnx_hw->monitor_vif != RWNX_INVALID_VIF) {
            struct rwnx_vif *rwnx_mon_vif = rwnx_hw->vif_table[rwnx_hw->monitor_vif];
            rwnx_chanctx_unlink(rwnx_mon_vif);
            rwnx_chanctx_link(rwnx_mon_vif, ind->ch_idx, NULL);
        }
#endif
    }

    if (ind->roamed) {
        struct cfg80211_roam_info info;
        memset(&info, 0, sizeof(info));

        if (rwnx_vif->ch_index < NX_CHAN_CTXT_CNT)
            info.channel = rwnx_hw->chanctx_table[rwnx_vif->ch_index].chan_def.chan;
        info.bssid = (const u8 *)ind->bssid.array;
        info.req_ie = req_ie;
        info.req_ie_len = ind->assoc_req_ie_len;
        info.resp_ie = rsp_ie;
        info.resp_ie_len = ind->assoc_rsp_ie_len;
        cfg80211_roamed(dev, &info, GFP_ATOMIC);
    } else {
        cfg80211_connect_result(dev, (const u8 *)ind->bssid.array, req_ie,
                                ind->assoc_req_ie_len, rsp_ie,
                                ind->assoc_rsp_ie_len, ind->status_code,
                                GFP_ATOMIC);
    }

    netif_tx_start_all_queues(dev);
    netif_carrier_on(dev);

    return 0;
}

static inline int rwnx_rx_sm_disconnect_ind(struct rwnx_hw *rwnx_hw,
                                            struct rwnx_cmd *cmd,
                                            struct ipc_e2a_msg *msg)
{
    struct sm_disconnect_ind *ind = (struct sm_disconnect_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct net_device *dev = rwnx_vif->ndev;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* if vif is not up, rwnx_close has already been called */
    if (rwnx_vif->up) {
        if (!ind->reassoc) {
            cfg80211_disconnected(dev, ind->reason_code, NULL, 0,
                                  (ind->reason_code <= 1), GFP_ATOMIC);

            if (rwnx_vif->sta.ft_assoc_ies) {
                kfree(rwnx_vif->sta.ft_assoc_ies);
                rwnx_vif->sta.ft_assoc_ies = NULL;
                rwnx_vif->sta.ft_assoc_ies_len = 0;
            }
        }
        netif_tx_stop_all_queues(dev);
        netif_carrier_off(dev);
    }

#ifdef CONFIG_RWNX_BFMER
    /* Disable Beamformer if supported */
    rwnx_bfmer_report_del(rwnx_hw, rwnx_vif->sta.ap);
#endif //(CONFIG_RWNX_BFMER)

    rwnx_txq_sta_deinit(rwnx_hw, rwnx_vif->sta.ap);
    rwnx_txq_tdls_vif_deinit(rwnx_vif);
    rwnx_dbgfs_unregister_sta(rwnx_hw, rwnx_vif->sta.ap);
    rwnx_vif->sta.ap->valid = false;
    rwnx_vif->sta.ap = NULL;
    rwnx_vif->generation++;
    rwnx_external_auth_disable(rwnx_vif);
    rwnx_chanctx_unlink(rwnx_vif);

    return 0;
}

static inline int rwnx_rx_sm_external_auth_required_ind(struct rwnx_hw *rwnx_hw,
                                                        struct rwnx_cmd *cmd,
                                                        struct ipc_e2a_msg *msg)
{
    struct sm_external_auth_required_ind *ind =
        (struct sm_external_auth_required_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
    struct net_device *dev = rwnx_vif->ndev;
    struct cfg80211_external_auth_params params;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    params.action = NL80211_EXTERNAL_AUTH_START;
    memcpy(params.bssid, ind->bssid.array, ETH_ALEN);
    params.ssid.ssid_len = ind->ssid.length;
    memcpy(params.ssid.ssid, ind->ssid.array,
           min_t(size_t, ind->ssid.length, sizeof(params.ssid.ssid)));
    params.key_mgmt_suite = ind->akm;

    if ((ind->vif_idx > NX_VIRT_DEV_MAX) || !rwnx_vif->up ||
        (RWNX_VIF_TYPE(rwnx_vif) != NL80211_IFTYPE_STATION) ||
        cfg80211_external_auth_request(dev, &params, GFP_ATOMIC)) {
        wiphy_err(rwnx_hw->wiphy, "Failed to start external auth on vif %d",
                  ind->vif_idx);
        rwnx_send_sm_external_auth_required_rsp(rwnx_hw, rwnx_vif,
                                                WLAN_STATUS_UNSPECIFIED_FAILURE);
        return 0;
    }

    rwnx_external_auth_enable(rwnx_vif);
#else
    rwnx_send_sm_external_auth_required_rsp(rwnx_hw, rwnx_vif,
                                            WLAN_STATUS_UNSPECIFIED_FAILURE);
#endif
    return 0;
}

static inline int rwnx_rx_sm_ft_auth_ind(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_cmd *cmd,
                                         struct ipc_e2a_msg *msg)
{
    struct sm_ft_auth_ind *ind = (struct sm_ft_auth_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct sk_buff *skb;
    size_t data_len = (offsetof(struct ieee80211_mgmt, u.auth.variable) +
                       ind->ft_ie_len);

    skb = dev_alloc_skb(data_len);
    if (skb) {
        struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb_put(skb, data_len);
        mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH);
        memcpy(mgmt->u.auth.variable, ind->ft_ie_buf, ind->ft_ie_len);
        rwnx_rx_defer_skb(rwnx_hw, rwnx_vif, skb);
        dev_kfree_skb(skb);
    } else {
        netdev_warn(rwnx_vif->ndev, "Allocation failed for FT auth ind\n");
    }

    return 0;
}

/***************************************************************************
 * Messages from TWT task
 **************************************************************************/
static inline int rwnx_rx_twt_setup_ind(struct rwnx_hw *rwnx_hw,
                                        struct rwnx_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    struct twt_setup_ind *ind = (struct twt_setup_ind *)msg->param;
    struct rwnx_sta *rwnx_sta = &rwnx_hw->sta_table[ind->sta_idx];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    memcpy(&rwnx_sta->twt_ind, ind, sizeof(struct twt_setup_ind));
    return 0;
}

static inline int rwnx_rx_mesh_path_create_cfm(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_path_create_cfm *cfm = (struct mesh_path_create_cfm *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[cfm->vif_idx];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* Check we well have a Mesh Point Interface */
    if (rwnx_vif && (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_MESH_POINT))
        rwnx_vif->ap.flags &= ~RWNX_AP_CREATE_MESH_PATH;

    return 0;
}

static inline int rwnx_rx_mesh_peer_update_ind(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_peer_update_ind *ind = (struct mesh_peer_update_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct rwnx_sta *rwnx_sta = &rwnx_hw->sta_table[ind->sta_idx];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if ((ind->vif_idx >= NX_VIRT_DEV_MAX) ||
        (rwnx_vif && (RWNX_VIF_TYPE(rwnx_vif) != NL80211_IFTYPE_MESH_POINT)) ||
        (ind->sta_idx >= NX_REMOTE_STA_MAX))
        return 1;

    if (rwnx_vif && (rwnx_vif->ap.flags & RWNX_AP_USER_MESH_PM))
    {
        if (!ind->estab && rwnx_sta->valid) {
            /* There is no way to inform upper layer for lost of peer, still
               clean everything in the driver */
            rwnx_sta->ps.active = false;
            rwnx_sta->valid = false;

            /* Remove the station from the list of VIF's station */
            list_del_init(&rwnx_sta->list);

            rwnx_txq_sta_deinit(rwnx_hw, rwnx_sta);
            rwnx_dbgfs_unregister_sta(rwnx_hw, rwnx_sta);
        } else {
            WARN_ON(0);
        }
    } else {
        /* Check if peer link has been established or lost */
        if (ind->estab) {
            if (rwnx_vif && (!rwnx_sta->valid)) {
                u8 txq_status;

                rwnx_sta->valid = true;
                rwnx_sta->sta_idx = ind->sta_idx;
                rwnx_sta->ch_idx = rwnx_vif->ch_index;
                rwnx_sta->vif_idx = ind->vif_idx;
                rwnx_sta->vlan_idx = rwnx_sta->vif_idx;
                rwnx_sta->ps.active = false;
                rwnx_sta->qos = true;
                rwnx_sta->aid = ind->sta_idx + 1;
                //rwnx_sta->acm = ind->acm;
                memcpy(rwnx_sta->mac_addr, ind->peer_addr.array, ETH_ALEN);

                rwnx_chanctx_link(rwnx_vif, rwnx_sta->ch_idx, NULL);

                /* Add the station in the list of VIF's stations */
                INIT_LIST_HEAD(&rwnx_sta->list);
                list_add_tail(&rwnx_sta->list, &rwnx_vif->ap.sta_list);

                /* Initialize the TX queues */
                if (rwnx_sta->ch_idx == rwnx_hw->cur_chanctx) {
                    txq_status = 0;
                } else {
                    txq_status = RWNX_TXQ_STOP_CHAN;
                }

                rwnx_txq_sta_init(rwnx_hw, rwnx_sta, txq_status);
                rwnx_dbgfs_register_sta(rwnx_hw, rwnx_sta);

#ifdef CONFIG_RWNX_BFMER
                // TODO: update indication to contains vht capabilties
                if (rwnx_hw->mod_params->bfmer)
                    rwnx_send_bfmer_enable(rwnx_hw, rwnx_sta, NULL);

                rwnx_mu_group_sta_init(rwnx_sta, NULL);
#endif /* CONFIG_RWNX_BFMER */

            } else {
                WARN_ON(0);
            }
        } else {
            if (rwnx_sta->valid) {
                rwnx_sta->ps.active = false;
                rwnx_sta->valid = false;

                /* Remove the station from the list of VIF's station */
                list_del_init(&rwnx_sta->list);

                rwnx_txq_sta_deinit(rwnx_hw, rwnx_sta);
                rwnx_dbgfs_unregister_sta(rwnx_hw, rwnx_sta);
            } else {
                WARN_ON(0);
            }
        }
    }

    return 0;
}

static inline int rwnx_rx_mesh_path_update_ind(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_path_update_ind *ind = (struct mesh_path_update_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct rwnx_mesh_path *mesh_path;
    bool found = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (ind->vif_idx >= NX_VIRT_DEV_MAX)
        return 1;

    if (!rwnx_vif || (RWNX_VIF_TYPE(rwnx_vif) != NL80211_IFTYPE_MESH_POINT))
        return 0;

    /* Look for path with provided target address */
    list_for_each_entry(mesh_path, &rwnx_vif->ap.mpath_list, list) {
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
            mesh_path->nhop_sta = &rwnx_hw->sta_table[ind->nhop_sta_idx];
            trace_mesh_update_path(mesh_path);
        } else {
            // Allocate a Mesh Path structure
            mesh_path = kmalloc(sizeof(struct rwnx_mesh_path), GFP_ATOMIC);

            if (mesh_path) {
                INIT_LIST_HEAD(&mesh_path->list);

                mesh_path->path_idx = ind->path_idx;
                mesh_path->nhop_sta = &rwnx_hw->sta_table[ind->nhop_sta_idx];
                memcpy(&mesh_path->tgt_mac_addr, &ind->tgt_mac_addr, MAC_ADDR_LEN);

                // Insert the path in the list of path
                list_add_tail(&mesh_path->list, &rwnx_vif->ap.mpath_list);
                trace_mesh_create_path(mesh_path);
            }
        }
    }
    /* coverity[leaked_storage] - mesh_path have added to list */
    return 0;
}

static inline int rwnx_rx_mesh_proxy_update_ind(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct mesh_proxy_update_ind *ind = (struct mesh_proxy_update_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct rwnx_mesh_proxy *mesh_proxy;
    bool found = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (ind->vif_idx >= NX_VIRT_DEV_MAX)
        return 1;

    if (!rwnx_vif || (RWNX_VIF_TYPE(rwnx_vif) != NL80211_IFTYPE_MESH_POINT))
        return 0;

    /* Look for path with provided external STA address */
    list_for_each_entry(mesh_proxy, &rwnx_vif->ap.proxy_list, list) {
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
        mesh_proxy = (struct rwnx_mesh_proxy *)kmalloc(sizeof(*mesh_proxy),
                                                       GFP_ATOMIC);

        if (mesh_proxy) {
            INIT_LIST_HEAD(&mesh_proxy->list);

            memcpy(&mesh_proxy->ext_sta_addr, &ind->ext_sta_addr, MAC_ADDR_LEN);
            mesh_proxy->local = ind->local;

            if (!ind->local) {
                memcpy(&mesh_proxy->proxy_addr, &ind->proxy_mac_addr, MAC_ADDR_LEN);
            }

            /* Insert the path in the list of path */
            list_add_tail(&mesh_proxy->list, &rwnx_vif->ap.proxy_list);
        }
    }
    /* coverity[leaked_storage] - mesh_proxy have added to list */
    return 0;
}

/***************************************************************************
 * Messages from APM task
 **************************************************************************/
static inline int rwnx_rx_apm_probe_client_ind(struct rwnx_hw *rwnx_hw,
                                               struct rwnx_cmd *cmd,
                                               struct ipc_e2a_msg *msg)
{
    struct apm_probe_client_ind *ind = (struct apm_probe_client_ind *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[ind->vif_idx];
    struct rwnx_sta *rwnx_sta = &rwnx_hw->sta_table[ind->sta_idx];

    rwnx_sta->stats.last_act = jiffies;
    cfg80211_probe_status(rwnx_vif->ndev, rwnx_sta->mac_addr, (u64)ind->probe_id,
                          ind->client_present, 0, false, GFP_ATOMIC);

    return 0;
}

#endif /* CONFIG_RWNX_FULLMAC */

/***************************************************************************
 * Messages from DEBUG task
 **************************************************************************/
static inline int rwnx_rx_dbg_error_ind(struct rwnx_hw *rwnx_hw,
                                        struct rwnx_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_error_ind(rwnx_hw);

    return 0;
}

static inline int rwnx_rx_rf_regvalue(struct rwnx_hw *rwnx_hw,
                                      struct rwnx_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct rf_read_result_ind *ind = (struct rf_read_result_ind *)msg->param;

    printk("%s %d: rf_regaddr:%x, rf_regvalue:%x\n", __func__, __LINE__,
           ind->rf_addr, ind->rf_data);

    return 0;
}

static inline int rwnx_scanu_cancel_cfm(struct rwnx_hw *rwnx_hw,
                                      struct rwnx_cmd *cmd,
                                      struct ipc_e2a_msg *msg)
{
    struct scanu_cancel_cfm *cfm = (struct scanu_cancel_cfm *)msg->param;
    struct rwnx_vif *rwnx_vif = rwnx_hw->vif_table[cfm->vif_idx];

    printk("%s %d: status:%x, vif_id=%x\n", __func__, __LINE__, cfm->status, cfm->vif_idx);
    rwnx_ipc_buf_dealloc(rwnx_hw, &rwnx_hw->scan_ie);
    rwnx_hw->scan_request = NULL;
    rwnx_vif->sta.cancel_scan_cfm = 1;

    return 0;
}


#ifdef CONFIG_RWNX_SOFTMAC

static msg_cb_fct mm_hdlrs[MSG_I(MM_MAX)] = {
    [MSG_I(MM_CONNECTION_LOSS_IND)]       = rwnx_rx_connection_loss_ind,
    [MSG_I(MM_CHANNEL_SWITCH_IND)]        = rwnx_rx_chan_switch_ind,
    [MSG_I(MM_CHANNEL_PRE_SWITCH_IND)]    = rwnx_rx_chan_pre_switch_ind,
    [MSG_I(MM_REMAIN_ON_CHANNEL_EXP_IND)] = rwnx_rx_remain_on_channel_exp_ind,
#ifdef CONFIG_RWNX_BCN
    [MSG_I(MM_PRIMARY_TBTT_IND)]          = rwnx_rx_prm_tbtt_ind,
#endif
    [MSG_I(MM_P2P_VIF_PS_CHANGE_IND)]     = rwnx_rx_p2p_vif_ps_change_ind,
    [MSG_I(MM_CSA_COUNTER_IND)]           = rwnx_rx_csa_counter_ind,
    [MSG_I(MM_CHANNEL_SURVEY_IND)]        = rwnx_rx_channel_survey_ind,
    [MSG_I(MM_RSSI_STATUS_IND)]           = rwnx_rx_rssi_status_ind,
};

static msg_cb_fct scan_hdlrs[MSG_I(SCAN_MAX)] = {
    [MSG_I(SCAN_DONE_IND)]                = rwnx_rx_scan_done_ind,
};

#else  /* CONFIG_RWNX_FULLMAC */

static msg_cb_fct mm_hdlrs[MSG_I(MM_MAX)] = {
    [MSG_I(MM_CHANNEL_SWITCH_IND)]     = rwnx_rx_chan_switch_ind,
    [MSG_I(MM_CHANNEL_PRE_SWITCH_IND)] = rwnx_rx_chan_pre_switch_ind,
    [MSG_I(MM_REMAIN_ON_CHANNEL_EXP_IND)] = rwnx_rx_remain_on_channel_exp_ind,
    [MSG_I(MM_PS_CHANGE_IND)]          = rwnx_rx_ps_change_ind,
    [MSG_I(MM_TRAFFIC_REQ_IND)]        = rwnx_rx_traffic_req_ind,
    [MSG_I(MM_P2P_VIF_PS_CHANGE_IND)]  = rwnx_rx_p2p_vif_ps_change_ind,
    [MSG_I(MM_CSA_COUNTER_IND)]        = rwnx_rx_csa_counter_ind,
    [MSG_I(MM_CSA_FINISH_IND)]         = rwnx_rx_csa_finish_ind,
    [MSG_I(MM_CSA_TRAFFIC_IND)]        = rwnx_rx_csa_traffic_ind,
    [MSG_I(MM_CHANNEL_SURVEY_IND)]     = rwnx_rx_channel_survey_ind,
    [MSG_I(MM_P2P_NOA_UPD_IND)]        = rwnx_rx_p2p_noa_upd_ind,
    [MSG_I(MM_RSSI_STATUS_IND)]        = rwnx_rx_rssi_status_ind,
    [MSG_I(MM_PKTLOSS_IND)]            = rwnx_rx_pktloss_notify_ind,
};

static msg_cb_fct scan_hdlrs[MSG_I(SCANU_MAX)] = {
    [MSG_I(SCANU_START_CFM)]           = rwnx_rx_scanu_start_cfm,
    [MSG_I(SCANU_RESULT_IND)]          = rwnx_rx_scanu_result_ind,
};

static msg_cb_fct me_hdlrs[MSG_I(ME_MAX)] = {
    [MSG_I(ME_TKIP_MIC_FAILURE_IND)] = rwnx_rx_me_tkip_mic_failure_ind,
    [MSG_I(ME_TX_CREDITS_UPDATE_IND)] = rwnx_rx_me_tx_credits_update_ind,
};

static msg_cb_fct sm_hdlrs[MSG_I(SM_MAX)] = {
    [MSG_I(SM_CONNECT_IND)]    = rwnx_rx_sm_connect_ind,
    [MSG_I(SM_DISCONNECT_IND)] = rwnx_rx_sm_disconnect_ind,
    [MSG_I(SM_EXTERNAL_AUTH_REQUIRED_IND)] = rwnx_rx_sm_external_auth_required_ind,
    [MSG_I(SM_FT_AUTH_IND)] = rwnx_rx_sm_ft_auth_ind,
};

static msg_cb_fct apm_hdlrs[MSG_I(APM_MAX)] = {
    [MSG_I(APM_PROBE_CLIENT_IND)] = rwnx_rx_apm_probe_client_ind,
};

static msg_cb_fct twt_hdlrs[MSG_I(TWT_MAX)] = {
    [MSG_I(TWT_SETUP_IND)]    = rwnx_rx_twt_setup_ind,
};

static msg_cb_fct mesh_hdlrs[MSG_I(MESH_MAX)] = {
    [MSG_I(MESH_PATH_CREATE_CFM)]  = rwnx_rx_mesh_path_create_cfm,
    [MSG_I(MESH_PEER_UPDATE_IND)]  = rwnx_rx_mesh_peer_update_ind,
    [MSG_I(MESH_PATH_UPDATE_IND)]  = rwnx_rx_mesh_path_update_ind,
    [MSG_I(MESH_PROXY_UPDATE_IND)] = rwnx_rx_mesh_proxy_update_ind,
};

#endif /* CONFIG_RWNX_SOFTMAC */

static msg_cb_fct dbg_hdlrs[MSG_I(DBG_MAX)] = {
    [MSG_I(DBG_ERROR_IND)]                = rwnx_rx_dbg_error_ind,
};

static msg_cb_fct tdls_hdlrs[MSG_I(TDLS_MAX)] = {
    [MSG_I(TDLS_CHAN_SWITCH_CFM)] = rwnx_rx_tdls_chan_switch_cfm,
    [MSG_I(TDLS_CHAN_SWITCH_IND)] = rwnx_rx_tdls_chan_switch_ind,
    [MSG_I(TDLS_CHAN_SWITCH_BASE_IND)] = rwnx_rx_tdls_chan_switch_base_ind,
    [MSG_I(TDLS_PEER_PS_IND)] = rwnx_rx_tdls_peer_ps_ind,
};

static msg_cb_fct priv_hdlrs[MSG_I(TASK_PRIV)] = {
    [MSG_I(PRIV_RF_READ_RESULT)]    = rwnx_rx_rf_regvalue,
    [MSG_I(PRIV_SCANU_CANCEL_CFM)]  = rwnx_scanu_cancel_cfm,
};

static msg_cb_fct *msg_hdlrs[] = {
    [TASK_MM]    = mm_hdlrs,
    [TASK_PRIV]  = priv_hdlrs,
    [TASK_DBG]   = dbg_hdlrs,
#ifdef CONFIG_RWNX_SOFTMAC
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
#endif /* CONFIG_RWNX_SOFTMAC */
};

/**
 *
 */
void rwnx_rx_handle_msg(struct rwnx_hw *rwnx_hw, struct ipc_e2a_msg *msg)
{
    rwnx_hw->cmd_mgr.msgind(&rwnx_hw->cmd_mgr, msg,
                            msg_hdlrs[MSG_T(msg->id)][MSG_I(msg->id)]);
}
