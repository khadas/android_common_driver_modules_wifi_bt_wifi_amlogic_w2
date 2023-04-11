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
#include "aml_main.h"
#include "aml_scc.h"

#include "reg_access.h"
#include "wifi_intf_addr.h"
#include "chip_pmu_reg.h"

#ifdef CONFIG_AML_RECOVERY

struct aml_recy *aml_recy;
static int recy_dbg = 1;

void aml_recy_flags_set(u32 flags)
{
    aml_recy->flags |= flags;
    RECY_DBG("set flags(0x%0x), new flags(0x%0x)", flags, aml_recy->flags);
}

void aml_recy_flags_clr(u32 flags)
{
    aml_recy->flags &= ~(flags);
    RECY_DBG("clr flags(0x%0x), new flags(0x%0x)", flags, aml_recy->flags);
}

bool aml_recy_flags_chk(u32 flags)
{
    //RECY_DBG("chk flags(0x%0x), original flags(0x%0x)", flags, aml_recy->flags);
    return (!!(aml_recy->flags & flags));
}

void aml_recy_save_assoc_info(struct cfg80211_connect_params *sme)
{
    uint8_t bcst_bssid[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t *pos;
    int ssid_len;
    int ies_len;

    if (!sme || !sme->ie || !sme->ie_len || !sme->ssid || !sme->ssid_len)
        return;

    RECY_DBG("save assoc info");

    if (sme->bssid) {
        memcpy(&aml_recy->assoc_info.bssid, sme->bssid, ETH_ALEN);
    } else {
        memcpy(&aml_recy->assoc_info.bssid, bcst_bssid, ETH_ALEN);
    }
    if (sme->prev_bssid) {
        memcpy(&aml_recy->assoc_info.prev_bssid, sme->prev_bssid, ETH_ALEN);
    }
    memcpy(&aml_recy->assoc_info.crypto,
            &sme->crypto, sizeof(struct cfg80211_crypto_settings));
    aml_recy->assoc_info.auth_type = sme->auth_type;
    aml_recy->assoc_info.mfp = sme->mfp;
    aml_recy->assoc_info.key_idx = sme->key_idx;
    aml_recy->assoc_info.key_len = sme->key_len;

#define AML_RECY_MEMCPY(dst, src, len) do { \
    if (src && len) { \
        if (dst && (sizeof(*dst) != len)) { kfree(dst); dst = NULL; } \
        if (!dst) { dst = kmalloc(len, GFP_KERNEL); } \
        if (!dst) { AML_INFO("kmalloc failed"); return; } \
        memcpy(dst, src, len); \
    } \
} while (0);

    AML_RECY_MEMCPY(aml_recy->assoc_info.chan,
            sme->channel, sizeof(struct ieee80211_channel));
    AML_RECY_MEMCPY(aml_recy->assoc_info.key_buf, sme->key, sme->key_len);
#undef AML_RECY_MEMCPY

    ssid_len = (sme->ssid_len > MAC_SSID_LEN) ? MAC_SSID_LEN : sme->ssid_len;
    ies_len = sme->ie_len + ssid_len + 2;
    if (!aml_recy->assoc_info.ies_buf) {
        aml_recy->assoc_info.ies_buf = kmalloc(ies_len, GFP_KERNEL);
        if (!aml_recy->assoc_info.ies_buf) {
            AML_INFO("kmalloc ies buf failed");
            return;
        }
    } else if (aml_recy->assoc_info.ies_len < ies_len) {
        kfree(aml_recy->assoc_info.ies_buf);
        aml_recy->assoc_info.ies_buf = kmalloc(ies_len, GFP_KERNEL);
        if (!aml_recy->assoc_info.ies_buf) {
            AML_INFO("kmalloc ies buf failed");
            return;
        }
    }
    pos = aml_recy->assoc_info.ies_buf;
    *pos++ = WLAN_EID_SSID;
    *pos++ = ssid_len;
    memcpy(pos, sme->ssid, ssid_len);
    pos += ssid_len;
    memcpy(pos, sme->ie, sme->ie_len);
    aml_recy->assoc_info.ies_len = ies_len;
    aml_recy_flags_set(AML_RECY_ASSOC_INFO_SAVED);
}

void aml_recy_save_ap_info(struct cfg80211_ap_settings *settings)
{
    if (!settings || !settings->chandef.chan) {
        AML_INFO("settings or chandef.chan is null");
        return;
    }

    RECY_DBG("save ap info");
    aml_recy->ap_info.band = settings->chandef.chan->band;
    cfg80211_to_aml_chan(&settings->chandef, &aml_recy->ap_info.chan);
    memcpy(aml_recy->ap_info.settings, settings, sizeof(struct cfg80211_ap_settings));
    aml_recy_flags_set(AML_RECY_AP_INFO_SAVED);
}

void aml_recy_save_bcn_info(uint8_t *bcn, size_t bcn_len)
{
    RECY_DBG("save bcn info");
    if (!aml_recy->ap_info.bcn) {
        aml_recy->ap_info.bcn = kmalloc(bcn_len, GFP_KERNEL);
        if (!aml_recy->ap_info.bcn) {
            AML_INFO("kmalloc beacon info failed");
            return;
        }
    }
    memcpy(aml_recy->ap_info.bcn, bcn, bcn_len);
    aml_recy->ap_info.bcn_len = bcn_len;
    aml_recy_flags_set(AML_RECY_BCN_INFO_SAVED);
}

int aml_recy_sta_connect(struct aml_hw *aml_hw, uint8_t *status)
{
    struct aml_vif *aml_vif;
    struct cfg80211_connect_params sme;
    struct sm_connect_cfm cfm;
    int ret = 0;
    /* if no connection, do nothing */
    if (!aml_hw || !(aml_vif = aml_hw->vif_table[0]) || !(aml_vif->ndev)
        || !(aml_recy->flags & AML_RECY_ASSOC_INFO_SAVED)
        || (AML_VIF_TYPE(aml_vif) != NL80211_IFTYPE_STATION))
        return ret;

    RECY_DBG("sta connect start");
    if (aml_hw->scan_request) {
        aml_scan_abort(aml_hw);
    }

    memset(&sme, 0, sizeof(sme));
    sme.bssid = aml_recy->assoc_info.bssid;
    sme.prev_bssid = aml_recy->assoc_info.prev_bssid;
    sme.channel = aml_recy->assoc_info.chan;
    memcpy(&sme.crypto, &aml_recy->assoc_info.crypto, sizeof(struct cfg80211_crypto_settings));
    sme.auth_type = aml_recy->assoc_info.auth_type;
    sme.mfp = aml_recy->assoc_info.mfp;
    sme.key_idx = aml_recy->assoc_info.key_idx;
    sme.key_len = aml_recy->assoc_info.key_len;
    memcpy((void *)sme.key, aml_recy->assoc_info.key_buf, aml_recy->assoc_info.key_len);

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

    sme.ssid_len = aml_recy->assoc_info.ies_buf[1];
    sme.ssid = &aml_recy->assoc_info.ies_buf[2];
    sme.ie = &aml_recy->assoc_info.ies_buf[2 + sme.ssid_len];
    sme.ie_len = aml_recy->assoc_info.ies_len - (2 + sme.ssid_len);
    ret = aml_send_sm_connect_req(aml_hw, aml_vif, &sme, &cfm);
    if (ret) {
        RECY_DBG("sta connect cmd fail");
    }
    else if (status != NULL) {
        *status = cfm.status;
    }
    return ret;
}
extern int aml_sdio_platform_on(struct aml_hw *aml_hw, void *config);
extern void aml_platform_off(struct aml_hw *aml_hw, void **config);
extern void extern_wifi_set_enable(int is_on);
extern unsigned char g_usb_after_probe;
static int aml_recy_fw_reload_for_usb_sdio(struct aml_hw * aml_hw)
{
    int ret;


    RECY_DBG("reload fw start");

    aml_recy_flags_set(AML_RECY_FW_ONGOING);
    aml_platform_off(aml_hw, NULL);
    if (aml_bus_type == USB_MODE) {
       g_usb_after_probe = 0;
       extern_wifi_set_enable(0);
       extern_wifi_set_enable(1);
       while (!g_usb_after_probe) {
           msleep(20);
       };
    }
    aml_sdio_platform_on(aml_hw, NULL);
    aml_send_reset(aml_hw);
    aml_send_me_config_req(aml_hw);
    aml_send_me_chan_config_req(aml_hw);
    if ((ret = aml_send_start(aml_hw))) {
        AML_INFO("reload fw failed");
        return -1;
    }

    aml_recy_flags_clr(AML_RECY_FW_ONGOING);
    return 0;
}
static int aml_recy_fw_reload_for_pcie(struct aml_hw *aml_hw)
{
    struct aml_plat *aml_plat = aml_hw->plat;
    unsigned int mac_clk_reg;
    int ret;
    u32 regval;

    if (!aml_hw->plat->enabled)
        return 0;

    RECY_DBG("reload fw start");

    aml_recy_flags_set(AML_RECY_FW_ONGOING);

    aml_ipc_stop(aml_hw);
    if (aml_hw->plat->disable)
        aml_hw->plat->disable(aml_hw);

    aml_ipc_deinit(aml_hw);
    aml_platform_reset(aml_hw->plat);
    regval = AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, NXMAC_MAC_CNTRL_2_ADDR);
    AML_REG_WRITE(regval | NXMAC_SOFT_RESET_BIT, aml_plat, AML_ADDR_SYSTEM, NXMAC_MAC_CNTRL_2_ADDR);
    aml_hw->plat->enabled = false;

    /* stop firmware */
    AML_REG_WRITE(0x00070000 | PHY_RESET | MAC_RESET | CPU_RESET, aml_plat, AML_ADDR_AON, RG_PMU_A22);
    msleep(10);
    AML_REG_WRITE(0x00070000 | CPU_RESET, aml_plat, AML_ADDR_AON, RG_PMU_A22);
    /* config cpu clock to 240Mhz */
    AML_REG_WRITE(CPU_CLK_VALUE, aml_plat, AML_ADDR_MAC_PHY, CPU_CLK_REG_ADDR);
    /* change mac clock to 240M */
    mac_clk_reg = AML_REG_READ(aml_plat, AML_ADDR_MAC_PHY, RG_INTF_MACCORE_CLK);
    mac_clk_reg |= 0x30000;
    AML_REG_WRITE(mac_clk_reg, aml_plat, AML_ADDR_MAC_PHY, RG_INTF_MACCORE_CLK);
    aml_plat_mpif_sel(aml_plat);
    aml_hw->phy.cnt = 1;
    aml_hw->rxbuf_idx = 0;
    aml_plat_lmac_load(aml_plat);

    aml_ipc_init(aml_hw, (u8 *)AML_ADDR(aml_plat, AML_ADDR_SYSTEM, SHARED_RAM_PCI_START_ADDR),
            (u8 *)AML_ADDR(aml_plat, AML_ADDR_SYSTEM, SHARED_RAM_HOST_RXBUF_ADDR),
            (u8 *)AML_ADDR(aml_plat, AML_ADDR_SYSTEM, SHARED_RAM_HOST_RXDESC_ADDR));
    if ((ret = aml_plat->enable(aml_hw)))
        return 0;

    AML_REG_WRITE(BOOTROM_ENABLE, aml_plat, AML_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
    /* start firmware */
    AML_REG_WRITE(0x00070000, aml_plat, AML_ADDR_AON, RG_PMU_A22);
    AML_REG_WRITE(CPU_CLK_VALUE, aml_plat, AML_ADDR_MAC_PHY, CPU_CLK_REG_ADDR);
    /* wait for chip ready */
    while (!(AML_REG_READ(aml_plat, AML_ADDR_MAC_PHY, REG_OF_VENDOR_ID)
            == W2p_VENDOR_AMLOGIC_EFUSE)) {};
    aml_ipc_start(aml_hw);
    aml_plat->enabled = true;

    aml_send_reset(aml_hw);
    aml_send_me_config_req(aml_hw);
    aml_send_me_chan_config_req(aml_hw);

    if ((ret = aml_send_start(aml_hw))) {
        AML_INFO("reload fw failed");
        return -1;
    }

    aml_recy_flags_clr(AML_RECY_FW_ONGOING);

    return ret;
}
static int aml_recy_fw_reload(struct aml_hw *aml_hw)
{
    int ret = 0;

    if (aml_bus_type != PCIE_MODE) {
        ret = aml_recy_fw_reload_for_usb_sdio(aml_hw);
    } else {
        ret = aml_recy_fw_reload_for_pcie(aml_hw);
    }

    return ret;
}

static int aml_recy_vif_reset(struct aml_hw *aml_hw)
{
    struct aml_vif *aml_vif;
    struct net_device *dev;
    int i;

    for (i = 0; i < aml_hw->vif_started; i++) {
        aml_vif = aml_hw->vif_table[i];
        RECY_DBG("reset vif(%d) interface", i);

        if (!aml_vif) {
            AML_INFO("retrieve vif or dev failed, aml vif is null");
            return -1;
        }

        dev = aml_vif->ndev;
        if (!dev) {
            AML_INFO("retrieve vif or dev failed, dev is null");
            return -1;
        }

        spin_lock_bh(&aml_hw->cb_lock);
        aml_chanctx_unlink(aml_vif);
        spin_unlock_bh(&aml_hw->cb_lock);

        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP) {
            /* delete any remaining STA before stopping the AP which aims to
             * flush pending TX buffers */
            while (!list_empty(&aml_vif->ap.sta_list)) {
                aml_cfg80211_del_station(aml_hw->wiphy, dev, NULL);
            }
        }

        aml_radar_cancel_cac(&aml_hw->radar);

        spin_lock_bh(&aml_hw->roc_lock);
        if (aml_hw->roc && (aml_hw->roc->vif == aml_vif)) {
            kfree(aml_hw->roc);
            aml_hw->roc = NULL;
        }
        spin_unlock_bh(&aml_hw->roc_lock);

        spin_lock_bh(&aml_hw->cb_lock);
        aml_vif->up = false;
        if (netif_carrier_ok(dev)) {
            if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_STATION ||
                AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_CLIENT) {
                if (aml_vif->sta.ft_assoc_ies) {
                    kfree(aml_vif->sta.ft_assoc_ies);
                    aml_vif->sta.ft_assoc_ies = NULL;
                    aml_vif->sta.ft_assoc_ies_len = 0;
                }
                if (aml_vif->sta.ap) {
                    aml_txq_sta_deinit(aml_hw, aml_vif->sta.ap);
                    aml_txq_tdls_vif_deinit(aml_vif);
                }
                netif_tx_stop_all_queues(dev);
                netif_carrier_off(dev);
            } else if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP_VLAN) {
                netif_carrier_off(dev);
            } else if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP ||
                    AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_GO) {
                aml_txq_vif_deinit(aml_hw, aml_vif);
                aml_scc_deinit();
                netif_tx_stop_all_queues(dev);
                netif_carrier_off(dev);
            }
        }
        spin_unlock_bh(&aml_hw->cb_lock);

        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_MONITOR)
            aml_hw->monitor_vif = AML_INVALID_VIF;
    }

    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        struct aml_sta *sta = aml_hw->sta_table + i;
        if (sta)
        {
            sta->valid = false;
        }
    }

    return 0;
}

static int aml_recy_vif_restart(struct aml_hw *aml_hw)
{
    struct aml_vif *aml_vif;
    struct net_device *dev;
    struct mm_add_if_cfm cfm;
    uint8_t i, status;
    int err = 0;

    for (i = 0; i < aml_hw->vif_started; i++) {
        aml_vif = aml_hw->vif_table[i];
        RECY_DBG("restart vif(%d) interface", i);
        if (!aml_vif) {
            AML_INFO("retrieve vif or dev failed, aml vif is null");
            return -1;
        }
        dev = aml_vif->ndev;
        if (!dev) {
            AML_INFO("retrieve vif or dev failed, dev is null");
            return -1;
        }

        err = aml_send_add_if(aml_hw, dev->dev_addr, AML_VIF_TYPE(aml_vif), false, &cfm);
        if (err || (cfm.status != 0)) {
            AML_INFO("add interface %d failed", i);
            return -1;
        }

        spin_lock_bh(&aml_hw->cb_lock);
        aml_vif->vif_index = cfm.inst_nbr;
        aml_vif->up = true;
        aml_hw->vif_table[cfm.inst_nbr] = aml_vif;
        spin_unlock_bh(&aml_hw->cb_lock);

        /* should keep STA operations before AP mode */
        if ((AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_STATION) &&
                (aml_recy->flags & AML_RECY_ASSOC_INFO_SAVED)) {
            err = aml_recy_sta_connect(aml_hw, &status);
            if (err || status) {
                AML_INFO("sta connect failed");
                return -1;
            }
            aml_recy->reconnect_rest = RECY_RETRY_CONNECT_MAX;
            aml_recy_flags_set(AML_RECY_CHECK_SCC);
        }

        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_CLIENT) {
            RECY_DBG("gc recovery ");
            cfg80211_disconnected(dev, WLAN_REASON_UNSPECIFIED, NULL, 0, 1, GFP_ATOMIC);
        }

        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_GO) {
            RECY_DBG("go recovery ");
            aml_recy_flags_set(AML_RECY_GO_ONGOING);
            while (!list_empty(&aml_vif->ap.sta_list)) {
                aml_cfg80211_del_station(aml_hw->wiphy, dev, NULL);
            }
        }

        /* should keep AP operations after STA mode */
        if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_AP) {
            aml_cfg80211_change_iface(aml_hw->wiphy, dev, NL80211_IFTYPE_AP, NULL);
            err = aml_cfg80211_start_ap(aml_vif->aml_hw->wiphy, dev, aml_recy->ap_info.settings);
            if (err) {
                AML_INFO("restart ap failed");
                return -1;
            }
            /* delete any remaining STA to let it reconnect */
            while (!list_empty(&aml_vif->ap.sta_list)) {
                aml_cfg80211_del_station(aml_hw->wiphy, dev, NULL);
            }
        }
    }

    return 0;
}

int aml_recy_doit(struct aml_hw *aml_hw)
{
    int ret;

    aml_recy_flags_set(AML_RECY_STATE_ONGOING);

    ret = aml_recy_vif_reset(aml_hw);
    if (ret) {
        AML_INFO("vif reset failed");
        goto out;
    }

    ret = aml_recy_fw_reload(aml_hw);
    if (ret) {
        AML_INFO("fw reload failed");
        goto out;
    }

    ret = aml_recy_vif_restart(aml_hw);
    if (ret) {
        AML_INFO("vif restart failed");
        goto out;
    }

out:
    aml_recy_flags_clr(AML_RECY_STATE_ONGOING);
    return ret;
}

static int aml_recy_detection(void)
{
    struct aml_cmd_mgr *cmd_mgr;
    int ret = false;

    if (!aml_recy | !aml_recy->aml_hw)
        return 0;

    cmd_mgr = &aml_recy->aml_hw->cmd_mgr;
    spin_lock_bh(&cmd_mgr->lock);
    if (cmd_mgr->state == AML_CMD_MGR_STATE_CRASHED) {
        ret = true;
    }
    spin_unlock_bh(&cmd_mgr->lock);

    return ret;
}

void aml_recy_check_scc(void)
{
    if (aml_recy_flags_chk(AML_RECY_CHECK_SCC)) {
        struct aml_wq *aml_wq;
        enum aml_wq_type type = AML_WQ_CHECK_SCC;

        aml_recy_flags_clr(AML_RECY_CHECK_SCC);
        aml_wq = aml_wq_alloc(1);
        if (!aml_wq) {
            AML_INFO("alloc wq out of memory");
            return;
        }
        aml_wq->id = AML_WQ_CHECK_SCC;
        memcpy(aml_wq->data, &type, 1);
        aml_wq_add(aml_recy->aml_hw, aml_wq);
    }
}

bool aml_recy_connect_retry(void)
{
    struct aml_wq *aml_wq;
    enum aml_wq_type type = AML_WQ_RECY_CONNECT_RETRY;

    aml_wq = aml_wq_alloc(1);
    if (!aml_wq) {
        AML_INFO("alloc wq out of memory");
        return false;
    }
    aml_wq->id = AML_WQ_RECY_CONNECT_RETRY;
    memcpy(aml_wq->data, &type, 1);
    aml_wq_add(aml_recy->aml_hw, aml_wq);
    return true;
}

static void aml_recy_timer_cb(struct timer_list *t)
{
    struct aml_wq *aml_wq;
    enum aml_wq_type type = AML_WQ_RECY;
    int ret = 0;

    if ((ret = aml_recy_detection())) {
        aml_wq = aml_wq_alloc(1);
        if (!aml_wq) {
            AML_INFO("alloc wq out of memory");
            return;
        }
        aml_wq->id = AML_WQ_RECY;
        memcpy(aml_wq->data, &type, 1);
        aml_wq_add(aml_recy->aml_hw, aml_wq);
    }
    mod_timer(&aml_recy->timer, jiffies + AML_RECY_MON_INTERVAL);
}

void aml_recy_enable(void)
{
    mod_timer(&aml_recy->timer, jiffies + AML_RECY_MON_INTERVAL);
}

void aml_recy_disable(void)
{
    del_timer_sync(&aml_recy->timer);
}

int aml_recy_init(struct aml_hw *aml_hw)
{
    RECY_DBG("recovery func init");

    aml_recy = kzalloc(sizeof(struct aml_recy), GFP_KERNEL);
    if (!aml_recy) {
        AML_INFO("recy info alloc failed");
        return -ENOMEM;
    }
    aml_recy->ap_info.settings = kzalloc(sizeof(struct cfg80211_ap_settings), GFP_KERNEL);
    if (!aml_recy->ap_info.settings) {
        AML_INFO("kmalloc ap settings failed");
        kfree(aml_recy);
        return -ENOMEM;
    }
    aml_recy->aml_hw = aml_hw;

    timer_setup(&aml_recy->timer, aml_recy_timer_cb, 0);
#if 0 //disable recovery function as default
    aml_recy_enable();
#endif

    return 0;
}

int aml_recy_deinit(void)
{
    RECY_DBG("recovery func deinit");

    aml_recy_disable();

#define AML_RECY_FREE(a) do { \
    if (a) kfree(a); \
} while (0);

    AML_RECY_FREE(aml_recy->assoc_info.chan);
    AML_RECY_FREE(aml_recy->assoc_info.key_buf);
    AML_RECY_FREE(aml_recy->assoc_info.ies_buf);
    AML_RECY_FREE(aml_recy->ap_info.bcn);
    AML_RECY_FREE(aml_recy->ap_info.settings);
    AML_RECY_FREE(aml_recy);
#undef AML_RECY_FREE

    return 0;
}
#endif
