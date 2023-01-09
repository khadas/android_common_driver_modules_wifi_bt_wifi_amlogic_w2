/**
****************************************************************************************
*
* @file aml_recy.c
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief Recovery implementation.
*
****************************************************************************************
*/
#include <linux/list.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <net/cfg80211.h>

#include "aml_wq.h"
#include "aml_recy.h"
#include "aml_cmds.h"
#include "aml_defs.h"
#include "aml_utils.h"
#include "aml_msg_tx.h"
#include "aml_platform.h"

#ifdef CONFIG_AML_RECOVERY
int aml_recy_save_info(struct aml_vif *aml_vif, struct cfg80211_connect_params *sme)
{
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_recy_info *recy_info = aml_hw->recy_info;
    int ies_len = sme->ie_len + sme->ssid_len + 2;
    uint8_t bcst_bssid[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t *pos;

#define RECY_INFO_MEMCPY(dst, src, len) do { \
    if (src && len) { \
        if (!dst) dst = kmalloc(len, GFP_KERNEL); \
        if (!dst) {AML_INFO("kmalloc failed"); return -1;} \
        memcpy(dst, src, len); \
    } \
} while (0);

    AML_INFO("recovery save info");
    aml_recy_conn_update(aml_hw, 1);
    recy_info->state = AML_RECY_STATE_INIT;
    if (sme->bssid) {
        memcpy(&recy_info->bssid, sme->bssid, ETH_ALEN);
    } else {
        memcpy(&recy_info->bssid, bcst_bssid, ETH_ALEN);
    }
    if (sme->prev_bssid) {
        memcpy(&recy_info->prev_bssid, sme->prev_bssid, ETH_ALEN);
    }
    memcpy(&recy_info->crypto, &sme->crypto, sizeof(struct cfg80211_crypto_settings));
    recy_info->auth_type = sme->auth_type;
    recy_info->mfp = sme->mfp;
    recy_info->key_idx = sme->key_idx;
    recy_info->key_len = sme->key_len;
    RECY_INFO_MEMCPY(recy_info->channel, sme->channel, sizeof(struct ieee80211_channel));
    RECY_INFO_MEMCPY(recy_info->key_buf, sme->key, sme->key_len);

    if (!recy_info->ies_buf) {
        recy_info->ies_buf = kmalloc(ies_len, GFP_KERNEL);
    }
    if (!recy_info->ies_buf) {
        AML_INFO("alloc ies buf failed");
        return -1;
    }
    pos = recy_info->ies_buf;
    *pos++ = WLAN_EID_SSID;
    *pos++ = sme->ssid_len;
    memcpy(pos, sme->ssid, sme->ssid_len);
    pos += sme->ssid_len;
    memcpy(pos, sme->ie, sme->ie_len);
    recy_info->ies_len = ies_len;

    return 0;
}

static int aml_recy_connect(struct aml_vif *aml_vif, struct sm_connect_cfm *cfm)
{
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_recy_info *recy_info = aml_hw->recy_info;
    struct cfg80211_connect_params sme;

    if (!aml_hw->is_connected)
        return 0;

    AML_INFO("recovery connection");
    if (aml_hw->scan_request) {
        aml_scan_abort(aml_hw);
    }

    memset(&sme, 0, sizeof(sme));
    sme.bssid = recy_info->bssid;
    sme.prev_bssid = recy_info->prev_bssid;
    sme.channel = recy_info->channel;
    memcpy(&sme.crypto, &recy_info->crypto,
            sizeof(struct cfg80211_crypto_settings));
    sme.auth_type = recy_info->auth_type;
    sme.mfp = recy_info->mfp;
    sme.key_idx = recy_info->key_idx;
    sme.key_len = recy_info->key_len;
    memcpy((void *)sme.key, recy_info->key_buf, recy_info->key_len);

    /* install key for shared-key authentication */
    if (!(sme.crypto.wpa_versions & (NL80211_WPA_VERSION_1
            | NL80211_WPA_VERSION_2 | NL80211_WPA_VERSION_3))
        && sme.key && sme.key_len
        && ((sme.auth_type == NL80211_AUTHTYPE_SHARED_KEY)
            || (sme.auth_type == NL80211_AUTHTYPE_AUTOMATIC)
            || (sme.crypto.n_ciphers_pairwise &
            (WLAN_CIPHER_SUITE_WEP40 | WLAN_CIPHER_SUITE_WEP104)))) {
        struct key_params key_params;

        if (sme.auth_type == NL80211_AUTHTYPE_AUTOMATIC) {
            sme.auth_type = NL80211_AUTHTYPE_SHARED_KEY;
        }
        key_params.key = sme.key;
        key_params.seq = NULL;
        key_params.key_len = sme.key_len;
        key_params.seq_len = 0;
        key_params.cipher = sme.crypto.cipher_group;
#ifdef CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT
        aml_cfg80211_add_key(aml_hw->wiphy, aml_vif->ndev,
                0, sme.key_idx, false, NULL, &key_params);
#else
        aml_cfg80211_add_key(aml_hw->wiphy, aml_vif->ndev,
                sme.key_idx, false, NULL, &key_params);
#endif
    }

    sme.ssid_len = recy_info->ies_buf[1];
    sme.ssid = &recy_info->ies_buf[2];
    sme.ie = &recy_info->ies_buf[2 + sme.ssid_len];
    sme.ie_len = recy_info->ies_len - (2 + sme.ssid_len);
    aml_send_sm_connect_req(aml_hw, aml_vif, &sme, cfm);

    return 0;
}

#if 0 //TODO
static int aml_recy_reset_ipc(struct aml_hw *aml_hw)
{
    return 0;
}

static int aml_recy_reset_fw(struct aml_hw *aml_hw)
{
    aml_recy_reset_ipc(aml_hw);
    return 0;
}
#endif

static int aml_recy_reset_all(struct aml_hw *aml_hw)
{
    struct aml_vif *aml_vif;
    struct net_device *dev;
    struct mm_add_if_cfm cfm;
    int err = 0;

    if (!aml_hw || !aml_hw->vif_table[0] || !aml_hw->vif_table[0]->ndev)
        return 0;

    AML_INFO("reset all");
    aml_vif = aml_hw->vif_table[0];
    dev = aml_vif->ndev;

#if 0 //TODO
#else
    {
        struct aml_cmd_mgr *cmd_mgr = &aml_hw->cmd_mgr;
        spin_lock_bh(&cmd_mgr->lock);
        cmd_mgr->state = AML_CMD_MGR_STATE_INITED;
        spin_unlock_bh(&cmd_mgr->lock);
    }
#endif

    aml_radar_cancel_cac(&aml_hw->radar);

    if (aml_hw->scan_request &&
        aml_hw->scan_request->wdev == &aml_vif->wdev) {
        aml_scan_abort(aml_hw);
    }

    aml_send_remove_if(aml_hw, aml_vif->vif_index);

    if (aml_hw->roc && (aml_hw->roc->vif == aml_vif)) {
        kfree(aml_hw->roc);
        aml_hw->roc = NULL;
    }

    spin_lock_bh(&aml_hw->cb_lock);
    aml_vif->up = false;
    if (netif_carrier_ok(dev)) {
        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_STATION ||
            AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_CLIENT) {
            if (aml_vif->sta.ap) {
                aml_txq_sta_deinit(aml_hw, aml_vif->sta.ap);
                aml_txq_tdls_vif_deinit(aml_vif);
            }
            netif_tx_stop_all_queues(dev);
            netif_carrier_off(dev);
        } else if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP_VLAN) {
            netif_carrier_off(dev);
        }
    }
    aml_hw->vif_table[aml_vif->vif_index] = NULL;
    spin_unlock_bh(&aml_hw->cb_lock);

    aml_chanctx_unlink(aml_vif);

    if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_MONITOR)
        aml_hw->monitor_vif = AML_INVALID_VIF;

    aml_hw->vif_started--;
    if (aml_hw->vif_started == 0) {
        /* reset both IPC sides and remain in sync */
        aml_ipc_tx_drain(aml_hw);
        aml_send_reset(aml_hw);
        aml_send_me_config_req(aml_hw);
        aml_send_me_chan_config_req(aml_hw);

        /* restart firmware */
        if ((err = aml_send_start(aml_hw))) {
            AML_INFO("restart firmware failed");
            goto fatal;
        }
    }

    if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP_VLAN) {
        cfm.inst_nbr = aml_vif->drv_vif_index;
        netif_tx_stop_all_queues(dev);
    } else {
        if ((err = aml_send_add_if(aml_hw,
                    dev->dev_addr, AML_VIF_TYPE(aml_vif), false, &cfm))) {
            AML_INFO("add interface failed");
            goto fatal;
        }

        if (cfm.status != 0) {
            AML_INFO("add interface failed, status(%d)", cfm.status);
            goto fatal;
        }
    }

    spin_lock_bh(&aml_hw->cb_lock);
    aml_vif->vif_index = cfm.inst_nbr;
    aml_vif->up = true;
    aml_hw->vif_started++;
    aml_hw->vif_table[cfm.inst_nbr] = aml_vif;
    spin_unlock_bh(&aml_hw->cb_lock);

    if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_MONITOR) {
        aml_hw->monitor_vif = aml_vif->vif_index;
        if (aml_vif->ch_index != AML_CH_NOT_SET) {
            err = aml_send_config_monitor_req(aml_hw,
                    &aml_hw->chanctx_table[aml_vif->ch_index].chan_def,
                    NULL);
        }
    }

    netif_carrier_off(dev);

fatal:
    return err;
}

int aml_recy_cmdcsh_doit(struct aml_hw *aml_hw)
{
    struct aml_recy_info *recy_info;
    struct aml_vif *aml_vif;
    struct sm_connect_cfm cfm;
    int ret = 0;

    if (!aml_hw || !aml_hw->vif_table[0] || !aml_hw->vif_table[0]->ndev)
        return 0;

    aml_vif = aml_hw->vif_table[0];
    recy_info = aml_hw->recy_info;

    if (recy_info->state != AML_RECY_STATE_STARTED)
        return 0;

    if ((ret = aml_recy_reset_all(aml_hw))) {
        AML_INFO("reset all failed");
        goto fatal;
    }

    if ((ret = aml_recy_connect(aml_vif, &cfm))) {
        AML_INFO("connect req failed");
        goto fatal;
    }

    switch (cfm.status) {
        case CO_OK:
            AML_INFO("recovery success");
            recy_info->state = AML_RECY_STATE_DONE;
            break;
        default:
            AML_INFO("recovery failed(%d)", cfm.status);
            aml_recy_conn_update(aml_hw, 0);
            recy_info->state = AML_RECY_STATE_FAIL;
            break;
    }

fatal:
    return ret;
}

static void aml_recy_cmdcsh_cb(struct timer_list *t)
{
    struct aml_hw *aml_hw = from_timer(aml_hw, t, cmdcsh_timer);
    struct aml_recy_info *recy_info = aml_hw->recy_info;
    struct aml_cmd_mgr *cmd_mgr = &aml_hw->cmd_mgr;
    enum aml_wq_type type = AML_WQ_RECY_CMDCSH;
    struct aml_wq *aml_wq;
    bool crashed = false;

    spin_lock_bh(&cmd_mgr->lock);
    if (cmd_mgr->state == AML_CMD_MGR_STATE_CRASHED) {
        crashed = true;
    }
    spin_unlock_bh(&cmd_mgr->lock);

    if (crashed && recy_info->state != AML_RECY_STATE_STARTED) {
        recy_info->state = AML_RECY_STATE_STARTED;
        aml_wq = aml_wq_alloc(1);
        if (!aml_wq) {
            AML_INFO("alloc wq out of memory");
            return;
        }
        aml_wq->id = AML_WQ_RECY_CMDCSH;
        memcpy(aml_wq->data, &type, 1);
        aml_wq_add(aml_hw, aml_wq);
    }
    mod_timer(&aml_hw->cmdcsh_timer, jiffies + AML_RECY_CMDCSH_MON_INTERVAL);
}

int aml_recy_txhang_doit(struct aml_hw *aml_hw)
{
    return 0;
}

static void aml_recy_txhang_cb(struct timer_list *t)
{
    struct aml_hw *aml_hw = from_timer(aml_hw, t, txhang_timer);
    struct aml_recy_info *recy_info = aml_hw->recy_info;
    enum aml_wq_type type = AML_WQ_RECY_TXHANG;
    struct aml_wq *aml_wq;
    bool txhang = false;

    //TODO: to be implement tx hang dectection
    if (txhang && recy_info->state != AML_RECY_STATE_STARTED) {
        recy_info->state = AML_RECY_STATE_STARTED;
        aml_wq = aml_wq_alloc(1);
        if (!aml_wq) {
            AML_INFO("alloc wq out of memory");
            return;
        }
        aml_wq->id = AML_WQ_RECY_TXHANG;
        memcpy(aml_wq->data, &type, 1);
        aml_wq_add(aml_hw, aml_wq);
    }
    mod_timer(&aml_hw->txhang_timer, jiffies + AML_RECY_TXHANG_MON_INTERVAL);
}

int aml_recy_pcierr_doit(struct aml_hw *aml_hw)
{
    return 0;
}

static void aml_recy_pcierr_cb(struct timer_list *t)
{
    struct aml_hw *aml_hw = from_timer(aml_hw, t, pcierr_timer);
    struct aml_recy_info *recy_info = aml_hw->recy_info;
    enum aml_wq_type type = AML_WQ_RECY_PCIERR;
    struct aml_wq *aml_wq;
    bool pcierr = false;

    //TODO: to be implement pci error dectection
    if (pcierr && recy_info->state != AML_RECY_STATE_STARTED) {
        recy_info->state = AML_RECY_STATE_STARTED;
        aml_wq = aml_wq_alloc(1);
        if (!aml_wq) {
            AML_INFO("alloc wq out of memory");
            return;
        }
        aml_wq->id = AML_WQ_RECY_PCIERR;
        memcpy(aml_wq->data, &type, 1);
        aml_wq_add(aml_hw, aml_wq);
    }
    mod_timer(&aml_hw->pcierr_timer, jiffies + AML_RECY_PCIERR_MON_INTERVAL);
}

void aml_recy_conn_update(struct aml_hw *aml_hw, bool is_connected)
{
    aml_hw->is_connected = is_connected;
}

int aml_recy_init(struct aml_hw *aml_hw)
{
    struct aml_recy_info *recy_info;

    if (aml_bus_type != PCIE_MODE)
        return 0;

    AML_INFO("recovery init");
    recy_info = kmalloc(sizeof(struct aml_recy_info), GFP_KERNEL);
    if (!recy_info) {
        AML_INFO("recy info alloc failed");
        return 0;
    }
    aml_hw->recy_info = recy_info;
    recy_info->state = AML_RECY_STATE_INIT;
    aml_recy_conn_update(aml_hw, 0);

    timer_setup(&aml_hw->cmdcsh_timer, aml_recy_cmdcsh_cb, 0);
    timer_setup(&aml_hw->txhang_timer, aml_recy_txhang_cb, 0);
    timer_setup(&aml_hw->pcierr_timer, aml_recy_pcierr_cb, 0);

    mod_timer(&aml_hw->cmdcsh_timer, jiffies + AML_RECY_CMDCSH_MON_INTERVAL);
    mod_timer(&aml_hw->txhang_timer, jiffies + AML_RECY_TXHANG_MON_INTERVAL);
    mod_timer(&aml_hw->pcierr_timer, jiffies + AML_RECY_PCIERR_MON_INTERVAL);

    return 0;
}

int aml_recy_deinit(struct aml_hw *aml_hw)
{
    struct aml_recy_info *recy_info = aml_hw->recy_info;

    if (aml_bus_type != PCIE_MODE)
        return 0;

    AML_INFO("recovery deinit");
    recy_info->state = AML_RECY_STATE_DEINIT;
    aml_recy_conn_update(aml_hw, 0);

    del_timer_sync(&aml_hw->cmdcsh_timer);
    del_timer_sync(&aml_hw->txhang_timer);
    del_timer_sync(&aml_hw->pcierr_timer);

    if (recy_info->channel)
        kfree(recy_info->channel);
    if (recy_info->key_buf)
        kfree(recy_info->key_buf);
    if (recy_info->ies_buf)
        kfree(recy_info->ies_buf);
    if (recy_info)
        kfree(aml_hw->recy_info);

    return 0;
}
#endif
