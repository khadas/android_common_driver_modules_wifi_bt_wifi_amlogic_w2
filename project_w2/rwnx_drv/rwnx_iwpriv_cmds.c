#include "rwnx_iwpriv_cmds.h"
#include "rwnx_mod_params.h"
#include "rwnx_debugfs.h"
#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "rwnx_platform.h"

#define RC_AUTO_RATE_INDEX -1

/** To calculate the index:
# NON-HT CCK : idx = (RATE_INDEX*2) + pre_type
# NON-HT OFDM: idx = 8 + (RATE_INDEX-4)
# HT: 16 + NSS*32 + MCS*4 + BW*2 + GI
# VHT: 144 + NSS*80 + MCS*8 + BW*2 + GI
# HE: 784 + NSS*144 + MCS*12 + BW*3 + GI
#
# Where:
#
# RATE_INDEX=[0-11] (1,2,5.5,11,6,9,12,18,24,36,48,54)
# pre_type=1: only long preamble, pre_type=0: short and long preamble
# NSS=0: 1 spatial stream, NSS=1: 2 spatial streams, ...
# MCS=0: MCS0, MCS=1: MCS1, ... MCS11
# BW=0: 20 MHz, BW=1: 40 MHz, BW=2: 80 MHz, BW=3: 160 MHz
# GI=0: long guard interval, GI=1: short guard interval (for HT/VHT)
# GI=0: 0.8us guard interval, GI=1: 1.6us guard interval, GI=2: 3.2us guard interval (for HE)
**/
static int aml_get_mcs_rate_index(enum aml_iwpriv_subcmd type,
    unsigned int mcs, unsigned int bw, unsigned int gi)
{
    int rate_index = RC_AUTO_RATE_INDEX;
    unsigned int nss = rwnx_mod_params.nss;

    switch (type) {
        case AML_IWP_SET_RATE_HT:
            rate_index = 16 + (nss * 32) + (mcs * 4) + (bw * 2) + gi;
            break;

        case AML_IWP_SET_RATE_VHT:
            rate_index = 144 + (nss * 80) + (mcs * 8) + (bw * 2) + gi;
            break;

        case AML_IWP_SET_RATE_HE:
            rate_index = 784 + (nss * 144) + (mcs * 12) + (bw * 3) + gi;
            break;

        case AML_IWP_SET_RATE_AUTO:
        default:
            rate_index = RC_AUTO_RATE_INDEX;
            break;
    }

    return rate_index;
}

static int aml_legacy_rate_to_index(int legacy)
{
    int i = 0;
    int legacy_rate_map[12]={1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54};

    for (i = 0; i < 12; i++) {
        if (legacy_rate_map[i] == legacy) {
            return i;
        }
    }

    return 0;
}

static int aml_set_fixed_rate(struct net_device *dev,  int fixed_rate_idx)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_sta *sta = NULL;
    union rwnx_rate_ctrl_info rate_config;
    int i = 0, error = 0;

     /* Convert rate index into rate configuration */
    if ((fixed_rate_idx < 0) || (fixed_rate_idx >= (N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU + N_HE_MU + N_HE_ER))) {
        // disable fixed rate
        rate_config.value = (u32)-1;
    } else {
        idx_to_rate_cfg(fixed_rate_idx, (union rwnx_rate_ctrl_info *)&rate_config, NULL);
    }

    // Forward the request to the LMAC
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = &rwnx_hw->sta_table[i];

        if (sta->valid && (rwnx_vif->vif_index == sta->vif_idx)) {
             if ((error = rwnx_send_me_rc_set_rate(rwnx_hw, sta->sta_idx,
                (u16)rate_config.value)) != 0) {
                return error;
            }

            rwnx_hw->debugfs.rc_config[sta->sta_idx] = (int)rate_config.value;
        }
    }

    return error;
}

static int aml_set_mcs_fixed_rate(struct net_device *dev, enum aml_iwpriv_subcmd type,
    unsigned int mcs, unsigned int bw, unsigned int gi)
{
    int fix_rate_idx = 0;

    fix_rate_idx = aml_get_mcs_rate_index(type, mcs, bw, gi);

    return aml_set_fixed_rate(dev,fix_rate_idx);
}

static int aml_set_legacy_rate_cck(struct net_device *dev, int legacy, int pre_type)
{
    int fix_rate_idx = 0;
    int legacy_rate_idx = 0;

    legacy_rate_idx = aml_legacy_rate_to_index(legacy);

    fix_rate_idx = (legacy_rate_idx * 2) + pre_type;

    return aml_set_fixed_rate(dev,fix_rate_idx);
}

static int aml_set_legacy_rate_ofdm(struct net_device *dev, int legacy)
{
    int fix_rate_idx = 0;
    int legacy_rate_idx =0;

    legacy_rate_idx = aml_legacy_rate_to_index(legacy);

    fix_rate_idx = 8 + (legacy_rate_idx - 4);

    return aml_set_fixed_rate(dev,fix_rate_idx);
}

static int aml_set_scan_hang(struct net_device *dev, int scan_hang)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    if (rwnx_vif->sta.scan_hang == scan_hang) {
        return 0;
    }

    rwnx_vif->sta.scan_hang = scan_hang;

    aml_scan_hang(dev, scan_hang);

    return 0;
}

static int aml_set_scan_time(struct net_device *dev, int scan_duration)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("set scan duration to %d us \n", scan_duration);
    rwnx_vif->sta.scan_duration = scan_duration;

    return 0;
}

int aml_get_reg(struct net_device *dev, int addr)
{
    u8* map_address = NULL;
    unsigned int reg_val = 0;
#ifndef CONFIG_RWNX_USB_MODE
    map_address = rwnx_pci_get_map_address(dev, addr);
#endif

    if (map_address) {
       reg_val = readl(map_address);
    }

    printk("Get reg addr: 0x%08x value:0x%08x\n", addr, reg_val);

    return 0;
}

int aml_set_reg(struct net_device *dev, int addr, int val)
{
    u8* map_address = NULL;

    printk("Set reg addr: 0x%08x value:0x%08x\n", addr, val);
#ifndef CONFIG_RWNX_USB_MODE
    map_address = rwnx_pci_get_map_address(dev, addr);
#endif
    if (map_address) {
        writel(val, map_address);
    }

    return 0;
}

static int aml_set_ps_mode(struct net_device *dev, int ps_mode)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;
    if ((ps_mode != MM_PS_MODE_OFF) && (ps_mode != MM_PS_MODE_ON) && (ps_mode != MM_PS_MODE_ON_DYN))
    {
        printk("param err, please reset\n");
        return -1;
    }
    printk("set ps_mode:0x%x success\n", ps_mode);
    return rwnx_send_me_set_ps_mode(rwnx_hw, ps_mode);
}

static int aml_send_twt_req(struct net_device *dev)
{
    struct twt_conf_tag twt_conf;
    struct twt_setup_cfm twt_setup_cfm;
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    u8 setup_type = MAC_TWT_SETUP_REQ;
    u8 vif_idx = rwnx_vif->vif_index;

    twt_conf.flow_type = 0;
    twt_conf.wake_int_exp = 0;
    twt_conf.wake_dur_unit = 0;
    twt_conf.min_twt_wake_dur = 0;
    twt_conf.wake_int_mantissa = 0;

    printk("%s %d\n", __func__, __LINE__);
    return rwnx_send_twt_request(rwnx_hw, setup_type, vif_idx, &twt_conf, &twt_setup_cfm);
}

static int aml_send_twt_teardown(struct net_device *dev)
{
    struct twt_teardown_req twt_teardown;
    struct twt_teardown_cfm twt_teardown_cfm;
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;

    twt_teardown.neg_type = 0;
    twt_teardown.all_twt = 0;
    twt_teardown.vif_idx = rwnx_vif->vif_index;
    twt_teardown.id = 0;

    printk("%s %d\n", __func__, __LINE__);
    return rwnx_send_twt_teardown(rwnx_hw, &twt_teardown, &twt_teardown_cfm);
}

#if defined(CONFIG_WEXT_PRIV)
static int aml_iwpriv_send_para1(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int *param = (int *)extra;
    int sub_cmd = param[0];
    int set1 = param[1];

    printk("%s cmd:%d set1:%d\n", __func__, sub_cmd, set1);

    switch (sub_cmd) {
        case AML_IWP_SET_RATE_LEGACY_OFDM:
            aml_set_legacy_rate_ofdm(dev, set1);
            break;
        case AML_IWP_GET_RF_REG:
            aml_rf_reg_read(dev, set1);
            break;
        case AML_IWP_SET_SCAN_HANG:
            aml_set_scan_hang(dev, set1);
            break;
        case AML_IWP_SET_SCAN_TIME:
            aml_set_scan_time(dev, set1);
            break;
        case AML_IWP_GET_REG:
            aml_get_reg(dev, set1);
            break;
        case AML_IWP_SET_PS_MODE:
            aml_set_ps_mode(dev, set1);
            break;
        default:
            printk("%s %d: param err\n", __func__, __LINE__);
            break;
    }

    return 0;
}

static int aml_iwpriv_send_para2(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int *param = (int *)extra;
    int sub_cmd = param[0];
    int set1 = param[1];
    int set2 = param[2];

    printk("%s cmd:%d set1:%d set2:%d\n", __func__, sub_cmd, set1, set2);

    switch (sub_cmd) {
        case AML_IWP_SET_RATE_LEGACY_CCK:
            aml_set_legacy_rate_cck(dev, set1, set2);
            break;
        case AML_IWP_SET_RF_REG:
            aml_rf_reg_write(dev, set1, set2);
            break;
         case AML_IWP_SET_REG:
            aml_set_reg(dev, set1, set2);
            break;
    }

    return 0;
}

static int aml_iwpriv_send_para3(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int *param = (int *)extra;
    int sub_cmd = param[0];
    int set1 = param[1];
    int set2 = param[2];
    int set3 = param[3];

    printk("%s cmd:%d set1:%d set2:%d set3:%d \n", __func__, sub_cmd, set1, set2, set3);

    switch (sub_cmd) {
        case AML_IWP_SET_RATE_HT:
        case AML_IWP_SET_RATE_VHT:
        case AML_IWP_SET_RATE_HE:
            aml_set_mcs_fixed_rate(dev, (enum aml_iwpriv_subcmd)sub_cmd, set1, set2, set3);
            break;
    }

    return 0;
}

static int aml_iwpriv_get(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int *param = (int *)extra;
    int sub_cmd = param[0];
    /*if we need feed back the value to user space, we need these 2 lines code, this is a sample*/
    //wrqu->data.length = sizeof(int);
    //*param = 110;

    switch (sub_cmd) {
        case AML_IWP_PRINT_VERSION:
            rwnx_get_version();
            break;
        case AML_IWP_SET_RATE_AUTO:
            aml_set_fixed_rate(dev, RC_AUTO_RATE_INDEX);
            break;
        case AML_IWP_SEND_TWT_SETUP_REQ:
            aml_send_twt_req(dev);
            break;
        case AML_IWP_SEND_TWT_TEARDOWN:
            aml_send_twt_teardown(dev);
            break;
        default:
            printk("%s %d param err\n", __func__, __LINE__);
            break;
    }

    return 0;
}

int iw_standard_set_mode(struct net_device *dev, struct iw_request_info *info,
    union iwreq_data *wrqu, char *extra)
{
    struct rwnx_vif *vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = vif->rwnx_hw;

    printk("%s:%d param:%d", __func__, __LINE__, wrqu->param.value);
    rwnx_cfg80211_change_iface(rwnx_hw->wiphy,dev,(enum nl80211_iftype)wrqu->param.value,NULL);

    return 0;
}

static const iw_handler standard_handler[] = {
    IW_HANDLER(SIOCSIWMODE,    (iw_handler)iw_standard_set_mode),
    NULL,
};

static iw_handler aml_iwpriv_private_handler[] = {
    /*if we need feed back the value to user space, we need jump command for large buffer*/
    aml_iwpriv_get,
    aml_iwpriv_send_para1,
    aml_iwpriv_send_para2,
    aml_iwpriv_send_para3,
    NULL,
};

static const struct iw_priv_args aml_iwpriv_private_args[] = {
    {
        /*if we need feed back the value to user space, we need jump command for large buffer*/
        SIOCIWFIRSTPRIV,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""},
    {
        AML_IWP_PRINT_VERSION,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_drv_ver"},
    {
        AML_IWP_SET_RATE_AUTO,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "set_rate_auto"},

    {
        AML_IWP_SEND_TWT_SETUP_REQ,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "send_twt_req"},
    {
        AML_IWP_SEND_TWT_TEARDOWN,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "send_twt_td"},

    {
        SIOCIWFIRSTPRIV + 1,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""},
    {
        AML_IWP_SET_RATE_LEGACY_OFDM,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rate_ofdm"},
    {
        AML_IWP_GET_RF_REG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_rf_reg"},
    {
        AML_IWP_SET_SCAN_HANG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_sc_hang"},
    {
        AML_IWP_SET_SCAN_TIME,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_sc_time"},
    {
        AML_IWP_GET_REG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_reg"},
    {
        AML_IWP_SET_PS_MODE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ps_mode"},

    {
        SIOCIWFIRSTPRIV + 2,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, ""},
    {
        AML_IWP_SET_RATE_LEGACY_CCK,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_rate_cck"},
    {
        AML_IWP_SET_RF_REG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_rf_reg"},
    {
        AML_IWP_SET_REG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_reg"},

    {
        SIOCIWFIRSTPRIV + 3,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, ""},
    {
        AML_IWP_SET_RATE_HT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_rate_ht"},
    {
        AML_IWP_SET_RATE_VHT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_rate_vht"},
    {
        AML_IWP_SET_RATE_HE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_rate_he"},
};
#endif


struct iw_handler_def iw_handle = {
#if defined(CONFIG_WEXT_PRIV)
    .num_standard = sizeof(standard_handler) / sizeof(standard_handler[0]),
    .num_private = ARRAY_SIZE(aml_iwpriv_private_handler),
    .num_private_args = ARRAY_SIZE(aml_iwpriv_private_args),
    .standard = (iw_handler *)standard_handler,
    .private = aml_iwpriv_private_handler,
    .private_args = aml_iwpriv_private_args,
#endif
};

