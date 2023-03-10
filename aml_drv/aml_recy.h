/**
****************************************************************************************
*
* @file aml_recy.h
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief Declaration of the recovery mechanism.
*
****************************************************************************************
*/

#ifndef __AML_RECY__
#define __AML_RECY__

#include "aml_defs.h"
#include "lmac_mac.h"
#include "wifi_debug.h"

#ifdef CONFIG_AML_RECOVERY

struct aml_recy_assoc_info {
    u8 bssid[ETH_ALEN];
    u8 prev_bssid[ETH_ALEN];
    struct ieee80211_channel *chan;
    struct cfg80211_crypto_settings crypto;
    enum nl80211_auth_type auth_type;
    enum nl80211_mfp mfp;
    u8 key_idx;
    u8 key_len;
    u8 *key_buf;
    size_t ies_len;
    u8 *ies_buf;
};

struct aml_recy_ap_info {
    struct cfg80211_ap_settings *settings;
    struct mac_chan_op chan;
    enum nl80211_band band;
    size_t bcn_len;
    u8 *bcn;
};

#define AML_RECY_ASSOC_INFO_SAVED   BIT(0)
#define AML_RECY_EXTAUTH_REQUIRED   BIT(1)
#define AML_RECY_BCN_INFO_SAVED     BIT(2)
#define AML_RECY_AP_INFO_SAVED      BIT(3)
#define AML_RECY_STATE_ONGOING      BIT(4)
#define AML_RECY_FW_ONGOING         BIT(5)

struct aml_recy {
   /* AML_RECY_x flags */
    u8 flags;
    struct aml_hw *aml_hw;
    struct aml_recy_assoc_info assoc_info;
    struct aml_recy_ap_info ap_info;

#define AML_RECY_MON_INTERVAL       (4 * HZ)
    struct timer_list timer;
};

extern struct aml_recy *aml_recy;

void aml_recy_enable(void);
void aml_recy_disable(void);

void aml_recy_flags_set(u8 flags);
void aml_recy_flags_clr(u8 flags);
bool aml_recy_flags_chk(u8 flags);

void aml_recy_save_assoc_info(struct cfg80211_connect_params *sme);
void aml_recy_save_ap_info(struct cfg80211_ap_settings *settings);
void aml_recy_save_bcn_info(u8 *bcn, size_t bcn_len);

int aml_recy_doit(struct aml_hw *aml_hw);
int aml_recy_init(struct aml_hw *aml_hw);
int aml_recy_deinit(void);

#define RECY_DBG(fmt, ...) do { \
    if (recy_dbg) { \
        printk("[recy_dbg][%-20.20s %4d] "fmt, __func__, __LINE__, ##__VA_ARGS__); \
    } \
} while (0);


#endif  // CONFIG_AML_RECOVERY
#endif  //__AML_RECY__
