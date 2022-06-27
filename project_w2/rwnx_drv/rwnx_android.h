/**
 ****************************************************************************************
 *
 * @file rwnx_android.h
 *
 * Copyright (C) Amlogic, Inc. All rights reserved (2022).
 *
 * @brief Declaration of Android WPA driver API implementation.
 *
 ****************************************************************************************
 */
#include <linux/compat.h>
#include "rwnx_defs.h"

#ifndef __RWNX_ANDROID_H__
#define __RWNX_ANDROID_H__

#define RWNX_ANDROID_CMD_MAX_LEN    8192
#define RWNX_ANDROID_CMD(_id, _str) {   \
    .id     = (_id),    \
    .str    = (_str),   \
}

struct rwnx_android_priv_cmd {
    char *buf;
    int used_len;
    int total_len;
};

#ifdef CONFIG_COMPAT
struct rwnx_android_compat_priv_cmd {
    compat_caddr_t buf;
    int used_len;
    int total_len;
};
#endif

enum rwnx_android_cmdid {
    CMDID_RSSI = 4,
    CMDID_COUNTRY = 17,
    CMDID_P2P_SET_NOA,
    CMDID_P2P_GET_NOA,
    CMDID_P2P_SET_PS,
    CMDID_SET_AP_WPS_P2P_IE,
};

struct rwnx_android_cmd {
    int id;
    char *str;
};

int rwnx_android_priv_ioctl(struct rwnx_vif *rwnx_vif, struct ifreq *ifr);

#endif
