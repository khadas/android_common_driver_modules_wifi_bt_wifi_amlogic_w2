/**
 ******************************************************************************
 *
 * @file aml_main.h
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#ifndef _AML_MAIN_H_
#define _AML_MAIN_H_

#include "aml_defs.h"
#include "aml_tx.h"


int aml_cfg80211_init(struct aml_plat *aml_plat, void **platform_data);
void aml_cfg80211_deinit(struct aml_hw *aml_hw);
int aml_cfg80211_change_iface(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      enum nl80211_iftype type,
                                      struct vif_params *params);

void aml_get_version(void);
void aml_cfg80211_sched_scan_results(struct wiphy *wiphy, uint64_t reqid);
int aml_cancel_scan(struct aml_hw *aml_hw, struct aml_vif *vif);
void aml_config_cali_param(struct aml_hw *aml_hw);
void aml_tx_rx_buf_init(struct aml_hw *aml_hw);

#endif /* _AML_MAIN_H_ */