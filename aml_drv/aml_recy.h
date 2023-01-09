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

#ifdef CONFIG_AML_RECOVERY
#define AML_RECY_PCIERR_MON_INTERVAL    (3 * HZ)
#define AML_RECY_CMDCSH_MON_INTERVAL    (4 * HZ)
#define AML_RECY_TXHANG_MON_INTERVAL    (5 * HZ)

enum aml_recy_state {
    AML_RECY_STATE_INIT,
    AML_RECY_STATE_DEINIT,
    AML_RECY_STATE_STARTED,
    AML_RECY_STATE_FAIL,
    AML_RECY_STATE_DONE,
};

struct aml_recy_info {
    enum aml_recy_state state;
    uint8_t bssid[ETH_ALEN];
    uint8_t prev_bssid[ETH_ALEN];
    struct ieee80211_channel *channel;
    struct cfg80211_crypto_settings crypto;
    enum nl80211_auth_type auth_type;
    enum nl80211_mfp mfp;
    uint8_t key_idx;
    uint8_t key_len;
    uint8_t *key_buf;
    uint16_t ies_len;
    uint8_t *ies_buf;
};

int aml_recy_save_info(struct aml_vif *aml_vif, struct cfg80211_connect_params *sme);
int aml_recy_cmdcsh_doit(struct aml_hw *aml_hw);
int aml_recy_txhang_doit(struct aml_hw *aml_hw);
int aml_recy_pcierr_doit(struct aml_hw *aml_hw);
int aml_recy_init(struct aml_hw *aml_hw);
int aml_recy_deinit(struct aml_hw *aml_hw);
void aml_recy_conn_update(struct aml_hw *aml_hw, bool is_connected);

#endif  // CONFIG_AML_RECOVERY
#endif  //__AML_RECY__
