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
#define FILE_DATA_LEN 82
#define BUF_LEN_128 128
#define PARSE_STA_MAC_DIGIT_BASE 34
#define PARSE_AP_MAC_DIGIT_BASE 64

#define CHIP_ID_F "CHIP_ID=%04x%08x\n"
#define CHIP_ID_EFUASE_L 0x20
#define CHIP_ID_EFUASE_H 0x24

enum mac_addr_type {
    MAC_STA = 0,
    MAC_AP = 1,
};

/*
 * Structure used to retrieve information from the PHY Config file used at Initialization time
 */
struct rwnx_phy_conf_file {
    struct phy_trd_cfg_tag trd;
    struct phy_karst_cfg_tag karst;
    struct phy_cataxia_cfg_tag cataxia;
};

int rwnx_parse_mac_addr_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                                   struct mac_addr *mac_addr_conf);
void rwnx_wiphy_addresses_free(struct wiphy *wiphy);

int rwnx_parse_phy_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                              struct rwnx_phy_conf_file *config, int path);
int aml_get_chip_id(struct rwnx_hw *rwnx_hw);

#endif /* _RWNX_CFGFILE_H_ */
