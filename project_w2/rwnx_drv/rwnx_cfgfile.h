/**
 ****************************************************************************************
 *
 * @file rwnx_cfgfile.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 * Copyright (C) Amlogic 2022
 *
 ****************************************************************************************
 */

#ifndef _RWNX_CFGFILE_H_
#define _RWNX_CFGFILE_H_

#define RWNX_CFGFILE_DEFAULT_PATH   "/data/vendor/wifi/wifi_conf.txt"
#define RWNX_CFGFILE_LBUF_MAXLEN    64
#define RWNX_CFGFILE_FBUF_MAXLEN    (RWNX_CFGFILE_LBUF_MAXLEN * 16)

#define RWNX_CFGFILE_CHIPID_LOW     0x8
#define RWNX_CFGFILE_CHIPID_HIGH    0x9

enum rwnx_cfgfile_flag {
    RWNX_CFGFILE_ERROR,
    RWNX_CFGFILE_EXIST,
    RWNX_CFGFILE_CREATE,
};

struct rwnx_cfgfile {
    u32 chip_id;
    u8 vif0_mac[ETH_ALEN];
    u8 vif1_mac[ETH_ALEN];
};

struct rwnx_cfgfile_phy {
    struct phy_trd_cfg_tag trd;
    struct phy_karst_cfg_tag karst;
    struct phy_cataxia_cfg_tag cataxia;
};

int rwnx_cfgfile_parse(struct rwnx_hw *rwnx_hw, struct rwnx_cfgfile *cfg);
int rwnx_cfgfile_parse_phy(struct rwnx_hw *rwnx_hw, const char *filename,
        struct rwnx_cfgfile_phy *cfg, int path);

#endif /* _RWNX_CFGFILE_H_ */
