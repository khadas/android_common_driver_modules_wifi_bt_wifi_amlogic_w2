/**
 ******************************************************************************
 *
 * @file aml_scc.c
 *
 * @brief handle sta + softap scc modes
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#include "aml_msg_tx.h"
#include "aml_mod_params.h"
#include "reg_access.h"
#include "aml_compat.h"
#include "fi_cmd.h"
#include "share_mem_map.h"
#include "aml_scc.h"

u8 bcn_save[1000];
u8 probe_rsp_save[1000];
u32 sap_init_band;
u32 beacon_need_update = 0;
static u8 *scc_bcn_buf = NULL;

/**
 * This function is used to check new vif's chan is same or diff with exit vif's chan
 *
 * Return vif's idx of diff chan
 */
u8 aml_scc_get_confilct_vif_idx(struct aml_vif *sap_vif)
{
    u32 i;
    for (i = 0;i < NX_VIRT_DEV_MAX;i++) {
        struct aml_vif *sta_vif = sap_vif->aml_hw->vif_table[i];
        if (sta_vif && sta_vif->up && sta_vif->vif_index != sap_vif->vif_index && sta_vif->ch_index != AML_CH_NOT_SET && sap_vif->ch_index != AML_CH_NOT_SET) {
            if (sta_vif->ch_index != sap_vif->ch_index) {
                AML_INFO("scc channel confilct,sta:[%d,%d],softap:[%d,%d] \n",
                    sta_vif->vif_index,sta_vif->ch_index,
                    sap_vif->vif_index,sap_vif->ch_index
                    );
                return sta_vif->vif_index;
            }
        }
    }
    return 0xff;
}

u8* aml_get_beacon_ie_addr(u8* buf, int frame_len,u8 eid)
{
    int len;
    u8 *var_pos;
    int var_offset = offsetof(struct ieee80211_mgmt, u.beacon.variable);
    len = frame_len - var_offset;
    var_pos = buf + var_offset;
    return (u8*)cfg80211_find_ie(eid, var_pos, len);
}

u8* aml_get_probe_rsp_ie_addr(u8* buf, int frame_len,u8 eid)
{
    int len;
    u8 *var_pos;
    int var_offset = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
    len = frame_len - var_offset;
    var_pos = buf + var_offset;
    return (u8*)cfg80211_find_ie(eid, var_pos, len);
}

/**
 * This function refresh beacon IEs by probe rsp frame
 *
 */

int aml_scc_change_beacon(struct aml_hw *aml_hw)
{
    struct aml_vif *vif = NULL;
    AML_DBG(AML_FN_ENTRY_STR);
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        switch (AML_VIF_TYPE(vif)) {
            case NL80211_IFTYPE_AP:
            {
                struct net_device *dev = vif->ndev;
                struct aml_bcn *bcn = &vif->ap.bcn;
                struct aml_ipc_buf buf = {0};
                int error;
                unsigned int addr;
                u8* bcn_ie_addr;
                u32 offset;
                u8 e_id;
                u8 e_len;
                // Build the beacon
                if (scc_bcn_buf == NULL) {
                    scc_bcn_buf = kmalloc(bcn->len, GFP_KERNEL);
                    if (!scc_bcn_buf)
                        return -ENOMEM;
                }
                memcpy(scc_bcn_buf,bcn_save,bcn->len);
                bcn_ie_addr = aml_get_beacon_ie_addr(scc_bcn_buf,bcn->len,WLAN_EID_SSID);
                *(bcn_ie_addr - 1 ) = 0;
                offset = bcn_ie_addr - scc_bcn_buf;
                while (offset < bcn->len) {
                    e_id = scc_bcn_buf[offset];
                    e_len = scc_bcn_buf[offset + 1];
                    if ((e_id != WLAN_EID_TIM) && (e_id != WLAN_EID_SSID)) {
                        u8* src_ie_addr = aml_get_probe_rsp_ie_addr(probe_rsp_save,500,e_id);
                        if (src_ie_addr != NULL) {
                            u8 src_ie_len = *(src_ie_addr + 1);
                            if (src_ie_len == e_len) {
                                memcpy(&scc_bcn_buf[offset],src_ie_addr,e_len + 2);
                            }
                        }
                    }
                    offset += e_len + 2;
                }

                // Sync buffer for FW
                if (aml_bus_type == PCIE_MODE) {
                    if ((error = aml_ipc_buf_a2e_init(aml_hw, &buf, scc_bcn_buf, bcn->len))) {
                        netdev_err(dev, "Failed to allocate IPC buf for new beacon\n");
                        return error;
                    }
                } else if (aml_bus_type == USB_MODE) {
                    addr = TXL_BCN_POOL  + (vif->vif_index * (BCN_TXLBUF_TAG_LEN + NX_BCNFRAME_LEN)) + BCN_TXLBUF_TAG_LEN;
                    aml_hw->plat->hif_ops->hi_write_sram((unsigned char *)scc_bcn_buf, (unsigned char *)(unsigned long)addr, bcn->len, USB_EP4);
                } else if (aml_bus_type == SDIO_MODE) {
                    addr = TXL_BCN_POOL  + (vif->vif_index * (BCN_TXLBUF_TAG_LEN + NX_BCNFRAME_LEN)) + BCN_TXLBUF_TAG_LEN;
                    aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)scc_bcn_buf, (unsigned char *)(unsigned long)addr, bcn->len);
                }

                // Forward the information to the LMAC
                error = aml_send_bcn_change(aml_hw, vif->vif_index, buf.dma_addr,
                bcn->len, bcn->head_len, bcn->tim_len, NULL);

                if (buf.addr)
                    dma_unmap_single(aml_hw->dev, buf.dma_addr, buf.size, DMA_TO_DEVICE);

                return error;
            }
            default:
                break;
        }
    }
    return -1;
}

/**
 * This function change beacon frame's HT IE primary chan info
 *
 */
int aml_scc_change_beacon_ht_ie(struct wiphy *wiphy, struct net_device *dev,struct aml_vif *target_vif)
{
    struct aml_hw *aml_hw = wiphy_priv(wiphy);
    struct aml_vif *vif = netdev_priv(dev);
    struct aml_bcn *bcn = &vif->ap.bcn;
    struct aml_ipc_buf buf = {0};
    int error;
    unsigned int addr;
    u8* tmp;
    u8 len;
    u8 *var_pos;
    int var_offset = offsetof(struct ieee80211_mgmt, u.beacon.variable);

    AML_INFO("change beacon start \n");
    // Build the beacon
    if (scc_bcn_buf == NULL) {
        scc_bcn_buf = kmalloc(bcn->len, GFP_KERNEL);
        if (!scc_bcn_buf)
            return -ENOMEM;
    }
    memcpy(scc_bcn_buf,bcn_save,bcn->len);
    /*change saved buffer ht INFO CHAN*/
    len = bcn->len - var_offset;
    var_pos = scc_bcn_buf + var_offset;
    tmp = (u8*)cfg80211_find_ie(WLAN_EID_HT_OPERATION, var_pos, len);
    if (tmp && tmp[1] >= sizeof(struct ieee80211_ht_operation)) {
        struct ieee80211_ht_operation *htop = (void *)(tmp + 2);
        u8 target_chan_idx = target_vif->ch_index;
        u32 target_freq = vif->aml_hw->chanctx_table[target_chan_idx].chan_def.chan->center_freq;
        u8 chan_no = aml_ieee80211_freq_to_chan(target_freq, vif->aml_hw->chanctx_table[target_chan_idx].chan_def.chan->band);
        htop->primary_chan = chan_no;
        AML_INFO("scc change ap channel num to %d \n",chan_no);
    }

    // Sync buffer for FW
    if (aml_bus_type == PCIE_MODE) {
        if ((error = aml_ipc_buf_a2e_init(aml_hw, &buf, scc_bcn_buf, bcn->len))) {
            netdev_err(dev, "Failed to allocate IPC buf for new beacon\n");
            return error;
        }
    } else if (aml_bus_type == USB_MODE) {
        addr = TXL_BCN_POOL  + (vif->vif_index * (BCN_TXLBUF_TAG_LEN + NX_BCNFRAME_LEN)) + BCN_TXLBUF_TAG_LEN;
        aml_hw->plat->hif_ops->hi_write_sram((unsigned char *)scc_bcn_buf, (unsigned char *)(unsigned long)addr, bcn->len, USB_EP4);
    } else if (aml_bus_type == SDIO_MODE) {
        addr = TXL_BCN_POOL  + (vif->vif_index * (BCN_TXLBUF_TAG_LEN + NX_BCNFRAME_LEN)) + BCN_TXLBUF_TAG_LEN;
        aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)scc_bcn_buf, (unsigned char *)(unsigned long)addr, bcn->len);
    }

    // Forward the information to the LMAC
    error = aml_send_bcn_change(aml_hw, vif->vif_index, buf.dma_addr,
                                 bcn->len, bcn->head_len, bcn->tim_len, NULL);

    if (buf.addr)
        dma_unmap_single(aml_hw->dev, buf.dma_addr, buf.size, DMA_TO_DEVICE);

    return error;
}

/**
 * This function is called when start ap,and save bcn_buf
 *
 */
void aml_scc_save_bcn_buf(u8* bcn_buf,size_t len)
{
    memcpy(bcn_save,bcn_buf,len);
}

void aml_scc_save_probe_rsp(struct aml_vif *vif,u8* buf)
{
    if (AML_SCC_BEACON_WAIT_PROBE() && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_AP)) {
        struct aml_hw *aml_hw = vif->aml_hw;
        memcpy(probe_rsp_save,buf,1000);
        AML_SCC_BEACON_SET_STATUS(BEACON_UPDATE_WAIT_DOWNLOAD);
#ifdef CONFIG_AML_USE_TASK
        UP_MISC_SEM(aml_hw);
#endif
    }
}

/**
 * This function is called when start ap,and save band info
 *
 */
void aml_scc_save_init_band(u32 init_band)
{
    sap_init_band = init_band;
}

u32 aml_scc_get_init_band(void)
{
    return sap_init_band;
}

/**
 * This function is called when softap mode need do chan switch to ensure in scc mode
 * 1.change beacon ie
 * 2.change chanctx link info
 * 3.nofify cfg80211
 *
 */
bool aml_handle_scc_chan_switch(struct aml_vif *vif,struct aml_vif *target_vif)
{
    struct net_device *dev = vif->ndev;
    struct cfg80211_chan_def chdef = vif->aml_hw->chanctx_table[target_vif->ch_index].chan_def;
    if ((aml_scc_get_init_band() == NL80211_BAND_2GHZ) && (chdef.chan->band > NL80211_BAND_2GHZ)) {
        AML_INFO("init band 2G,cannot switch to 5G \n");
        return false;
    }
    if (aml_scc_change_beacon_ht_ie(vif->aml_hw->wiphy,dev,target_vif)) {
        // if beacon buffer ie change fail
        AML_INFO("bcn change ht ie fail \n");
        return false;
    }
    aml_chanctx_unlink(vif);
    aml_chanctx_link(vif, target_vif->ch_index, &chdef);
#ifdef CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT
    cfg80211_ch_switch_notify(dev, &chdef, 0);
#else
    cfg80211_ch_switch_notify(dev, &chdef);
#endif
    AML_SCC_BEACON_SET_STATUS(BEACON_UPDATE_WAIT_PROBE);
    return true;
}

/**
 * This function is called when start softap or sta_mode get ip success
 *
 */
void aml_scc_check_chan_confilct(struct aml_hw *aml_hw)
{
    struct aml_vif *vif;
    AML_DBG(AML_FN_ENTRY_STR);
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        switch (AML_VIF_TYPE(vif)) {
            case NL80211_IFTYPE_AP:
            {
                u8 target_vif_idx = aml_scc_get_confilct_vif_idx(vif);
                AML_INFO("chan conflict,target_vif_idx:%d",target_vif_idx);
                if (target_vif_idx != 0xff) {
                    if (aml_handle_scc_chan_switch(vif,vif->aml_hw->vif_table[target_vif_idx])) {
                        aml_send_scc_conflict_nofify(vif,target_vif_idx);//notify fw
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void aml_scc_init(void)
{
    scc_bcn_buf = NULL;
}

void aml_scc_deinit(void)
{
    if (scc_bcn_buf) {
        kfree(scc_bcn_buf);
    }
    AML_SCC_CLEAR_BEACON_UPDATE();
}
