/**
 ****************************************************************************************
 *
 * @file rwnx_regdom.h
 *
 * Copyright (C) Amlogic, Inc. All rights reserved (2022).
 *
 * @brief Declaration of the preallocing buffer.
 *
 ****************************************************************************************
 */

#ifndef __RWNX_REGDOM_H__
#define __RWNX_REGDOM_H__

#include <net/cfg80211.h>
#include <linux/types.h>

struct rwnx_regdom {
    char country_code[2];
    const struct ieee80211_regdomain *regdom;
};

void rwnx_apply_custom_regdom(struct wiphy *wiphy, char *alpha2);

#endif
