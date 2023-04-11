/**
 ****************************************************************************************
 *
 * @file aml_cfgfile.h
 *
 * Copyright (C) Amlogic 2012-2021
 * Copyright (C) Amlogic 2022
 *
 ****************************************************************************************
 */

#ifndef _AML_CFGFILE_H_
#define _AML_CFGFILE_H_

#ifdef CONFIG_LINUXPC_VERSION
#define AML_CFGFILE_DEFAULT_PATH   "/lib/firmware/wifi_conf.txt"
#else
#define AML_CFGFILE_DEFAULT_PATH   "/data/vendor/wifi/wifi_conf.txt"
#endif
#define AML_CFGFILE_LBUF_MAXLEN    64
#define AML_CFGFILE_FBUF_MAXLEN    (AML_CFGFILE_LBUF_MAXLEN * 16)

#define AML_CFGFILE_CHIPID_LOW     0x8
#define AML_CFGFILE_CHIPID_HIGH    0x9
#define AML_CFGFILE_CHIPID_LEN     12

#define AML_CFGFILE_MACADDR_LOW    0x1
#define AML_CFGFILE_MACADDR_HIGH   0x2

enum aml_cfgfile_flag {
    AML_CFGFILE_ERROR,
    AML_CFGFILE_EXIST,
    AML_CFGFILE_CREATE,
};

struct aml_cfgfile {
    u32 chip_id;
    u8 vif0_mac[ETH_ALEN];
    u8 vif1_mac[ETH_ALEN];
};

struct aml_cfgfile_phy {
    struct phy_trd_cfg_tag trd;
    struct phy_karst_cfg_tag karst;
    struct phy_cataxia_cfg_tag cataxia;
};

int aml_cfgfile_parse(struct aml_hw *aml_hw, struct aml_cfgfile *cfg);
int aml_cfgfile_parse_phy(struct aml_hw *aml_hw, const char *filename,
        struct aml_cfgfile_phy *cfg, int path);
void aml_cfgfile_update_rps(void);

#endif /* _AML_CFGFILE_H_ */
