/**
 ******************************************************************************
 *
 * @file rwnx_main.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#ifndef _RWNX_MAIN_H_
#define _RWNX_MAIN_H_

#include "rwnx_defs.h"
#include "rwnx_tx.h"

int rwnx_cfg80211_init(struct rwnx_plat *rwnx_plat, void **platform_data);
void rwnx_cfg80211_deinit(struct rwnx_hw *rwnx_hw);
int rwnx_cfg80211_change_iface(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      enum nl80211_iftype type,
                                      struct vif_params *params);

void rwnx_get_version(void);
void rwnx_cfg80211_sched_scan_results(struct wiphy *wiphy, uint64_t reqid);
int rwnx_cancel_scan(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif);

#endif /* _RWNX_MAIN_H_ */
