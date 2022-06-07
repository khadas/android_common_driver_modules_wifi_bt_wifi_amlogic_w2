/**
 ****************************************************************************************
 *
 * @file rwnx_cfgfile.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ****************************************************************************************
 */

#ifndef _RWNX_CFGFILE_H_
#define _RWNX_CFGFILE_H_

/* mac addr string len and offset */
#define FILE_DATA_LEN 27
#define PARSE_DIGIT_BASE 9

/*
 * Structure used to retrieve information from the Config file used at Initialization time
 */
struct rwnx_conf_file {
    u8 mac_addr[ETH_ALEN];
};

/*
 * Structure used to retrieve information from the PHY Config file used at Initialization time
 */
struct rwnx_phy_conf_file {
    struct phy_trd_cfg_tag trd;
    struct phy_karst_cfg_tag karst;
    struct phy_cataxia_cfg_tag cataxia;
};

int rwnx_parse_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                          struct rwnx_conf_file *config);

int rwnx_parse_phy_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                              struct rwnx_phy_conf_file *config, int path);
#endif /* _RWNX_CFGFILE_H_ */
