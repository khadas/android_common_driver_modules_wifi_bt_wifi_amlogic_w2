#include <linux/sort.h>
#include <linux/math64.h>
#include "aml_iwpriv_cmds.h"
#include "aml_mod_params.h"
#include "aml_debugfs.h"
#include "aml_main.h"
#include "aml_msg_tx.h"
#include "aml_platform.h"
#include "reg_access.h"
#include "wifi_debug.h"
#include "aml_fw_trace.h"
#include "aml_utils.h"
#include "aml_compat.h"
#include "aml_recy.h"
#ifdef SDIO_SPEED_DEBUG
#include "sg_common.h"
#endif
#include "sdio_common.h"


#define RC_AUTO_RATE_INDEX -1
#define MAX_CHAR_SIZE 40
#define PRINT_BUF_SIZE 1024
#define LA_BUF_SIZE 2048
#define LA_MEMORY_BASE_ADDRESS 0x60830000
static char *mactrace_path = "/data/la_dump/mactrace";
static char *reg_path = "/data/dumpinfo";
#define REG_DUMP_SIZE 2048

unsigned long long g_dbg_modules = 0;

//hx add
typedef unsigned char   U8;
#define MACBYP_TXV_ADDR 0x60C06200
#define MACBYP_TXV_04 0x04
#define MACBYP_TXV_08 0x08
#define MACBYP_TXV_0c 0x0c
#define MACBYP_TXV_10 0x10
#define MACBYP_TXV_14 0x14
#define MACBYP_TXV_18 0x18
#define MACBYP_TXV_1c 0x1c
#define MACBYP_TXV_20 0x20
#define MACBYP_TXV_24 0x24
#define MACBYP_TXV_28 0x28
#define MACBYP_TXV_2c 0x2c
#define MACBYP_TXV_30 0x30
#define MACBYP_TXV_34 0x34
#define MACBYP_TXV_38 0x38
#define MACBYP_TXV_3c 0x3c
#define MACBYP_TXV_40 0x40
#define MACBYP_TXV_44 0x44
#define MACBYP_CTRL_ADDR 0x60C06000
#define MACBYP_CTRL_80 0x80
#define MACBYP_CTRL_84 0x84
#define MACBYP_CTRL_88 0x88
#define MACBYP_CTRL_8C 0x8C
#define RF_CTRL_ADDR 0X80000000
#define RF_CTRL_08 0x08
#define RF_CTRL_1008 0x1008
#define RF_ANTTA_ACTIVE 0X60C0B500
#define MACBYP_RIU_EN 0X60C0B004
#define MACBYP_AP_BW 0X60C00800
#define MACBYP_DIG_GAIN 0X60C0B100
#define MACBYP_PKT_ADDR 0X60C06010
#define MACBYP_PAYLOAD_ADDR 0x60C06004
#define MACBYP_TRIGGER_ADDR 0x60C06008
#define MACBYP_CLKEN_ADDR 0x60C0600C
#define MACBYP_INTERFRAME_DELAY_ADDR 0x60C06048
#define CPU_CLK_REG_ADDR 0x00a0d090
#define MPF_CLK_REG_ADDR 0x00a0d084
#define CRM_CLKRST_CNTL_ADDR 0x60805008
#define CRM_CLKGATEPHYFCTRL0_ADDR 0x60805010
#define CRM_CLKGATEPHYFCTRL1_ADDR 0x60805014
#define PCIE_BAR4_TABLE5_EP_BASE_MIMO 0x60c0088C
#define AGC_ADDR_HT 0x60c0b104
#define XOSC_CTUNE_BASE 0x00f01024
#define POWER_OFFSET_BASE_WF0 0x00a0e658
#define POWER_OFFSET_BASE_WF1 0x00a0f658
#define EFUSE_BASE_1A 0X1A
#define EFUSE_BASE_1B 0X1B
#define EFUSE_BASE_1C 0X1C
#define EFUSE_BASE_1E 0X1E
#define EFUSE_BASE_1F 0X1F
#define EFUSE_BASE_05 0X05
#define EFUSE_BASE_01 0x1
#define EFUSE_BASE_02 0x2
#define EFUSE_BASE_03 0x3


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
static int aml_get_mcs_rate_index(enum aml_iwpriv_subcmd type,  unsigned int nss,
    unsigned int mcs, unsigned int bw, unsigned int gi)
{
    int rate_index = RC_AUTO_RATE_INDEX;

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

    return -1;
}

static int aml_set_fixed_rate(struct net_device *dev,  int fixed_rate_idx)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_sta *sta = NULL;
    union aml_rate_ctrl_info rate_config;
    int i = 0, error = 0;

     /* Convert rate index into rate configuration */
    if ((fixed_rate_idx < 0) || (fixed_rate_idx >= (N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU + N_HE_MU + N_HE_ER))) {
        // disable fixed rate
        rate_config.value = (u32)-1;
    } else {
        idx_to_rate_cfg(fixed_rate_idx, (union aml_rate_ctrl_info *)&rate_config, NULL);
    }

    // Forward the request to the LMAC
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = aml_hw->sta_table + i;

        if (sta && sta->valid && (aml_vif->vif_index == sta->vif_idx)) {
             if ((error = aml_send_me_rc_set_rate(aml_hw, sta->sta_idx,
                (u16)rate_config.value)) != 0) {
                return error;
            }

            aml_hw->debugfs.rc_config[sta->sta_idx] = (int)rate_config.value;
        }
    }

    return error;
}

static int aml_set_p2p_noa(struct net_device *dev, int count, int interval, int duration)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct mm_set_p2p_noa_cfm cfm;
    if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_GO) {
        /* Forward request to the embedded and wait for confirmation */
        aml_send_p2p_noa_req(aml_hw, aml_vif, count, interval, duration, 0,  &cfm);
    }
    return 0;
}

static int aml_set_mcs_fixed_rate(struct net_device *dev, enum aml_iwpriv_subcmd type,
    unsigned int nss_mcs, unsigned int bw, unsigned int gi)
{
    int fix_rate_idx = 0;
    unsigned int nss = 0;
    unsigned int mcs = 0;

    nss = (nss_mcs >> 16) & 0xff;
    mcs = nss_mcs & 0xff;

    printk("set fix_rate[nss:%d mcs:%d bw:%d gi:%d]\n", nss, mcs, bw, gi);

    fix_rate_idx = aml_get_mcs_rate_index(type, nss, mcs, bw, gi);

    return aml_set_fixed_rate(dev,fix_rate_idx);
}

static int aml_set_legacy_rate(struct net_device *dev, int legacy, int pre_type)
{
    int fix_rate_idx = 0;
    int legacy_rate_idx = 0;

    legacy_rate_idx = aml_legacy_rate_to_index(legacy);

    if (legacy_rate_idx < 0)
    {
        printk("Operation failed! Please enter the correct format\n");
        return 0;
    }

    if (legacy_rate_idx < N_CCK/2)
        fix_rate_idx = (legacy_rate_idx * 2) + pre_type;
    else
        fix_rate_idx = 8 + (legacy_rate_idx - 4);

    return aml_set_fixed_rate(dev,fix_rate_idx);
}

static int aml_set_scan_hang(struct net_device *dev, int scan_hang)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
/*
    if (aml_vif->sta.scan_hang == scan_hang) {
       return 0;
    }

*/
    aml_vif->sta.scan_hang = scan_hang;

    aml_scan_hang(aml_vif, scan_hang);

    return 0;
}

static int aml_set_limit_power_status(struct net_device *dev, int limit_power_switch)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    if (limit_power_switch > 0x2)
    {
        AML_INFO("param error \n")
    }

    return aml_set_limit_power(aml_hw, limit_power_switch);
}

static int aml_set_scan_time(struct net_device *dev, int scan_duration)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("set scan duration to %d us \n", scan_duration);
    aml_vif->sta.scan_duration = scan_duration;

    return 0;
}

int aml_get_reg_2(struct net_device *dev, unsigned int addr,union iwreq_data *wrqu, char *extra)
{
    unsigned int reg_val = 0;
    if (aml_bus_type == PCIE_MODE) {
         u8 *map_address = NULL;
         if (addr & 3) {
             reg_val = 0xdead5555;
         }
         else {
             map_address = aml_pci_get_map_address(dev, addr);
             if (map_address) {
                 reg_val = readl(map_address);
             }
         }
     } else {
         struct aml_vif *aml_vif = netdev_priv(dev);
         struct aml_hw *aml_hw = aml_vif->aml_hw;
         struct aml_plat *aml_plat = aml_hw->plat;
         reg_val = AML_REG_READ(aml_plat, 0, addr);
     }
    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&0x%08x", reg_val);
    wrqu->data.length++;

    printk("reg_val: 0x%08x", reg_val);
    //printk("Get reg addr: 0x%08x value: 0x%08x\n", addr, reg_val);
    return reg_val;
}

int aml_get_reg(struct net_device *dev, char *str_addr, union iwreq_data *wrqu, char *extra)
{
    unsigned int addr = 0;
    unsigned int reg_val = 0;

    addr = simple_strtol(str_addr, NULL, 0);

    if (aml_bus_type == PCIE_MODE) {
        u8 *map_address = NULL;
        if (addr & 3) {
            reg_val = 0xdead5555;
        }
        else {
            map_address = aml_pci_get_map_address(dev, addr);
            if (map_address) {
                reg_val = readl(map_address);
            }
        }
    } else {
        struct aml_vif *aml_vif = netdev_priv(dev);
        struct aml_hw *aml_hw = aml_vif->aml_hw;
        struct aml_plat *aml_plat = aml_hw->plat;
        reg_val = AML_REG_READ(aml_plat, 0, addr);
    }

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&0x%08x", reg_val);
    wrqu->data.length++;

//    printk("Get reg addr: 0x%08x value: 0x%08x\n", addr, reg_val);
    return 0;
}

int aml_set_reg(struct net_device *dev, int addr, int val)
{

    if (aml_bus_type != PCIE_MODE) {
        struct aml_vif *aml_vif = netdev_priv(dev);
        struct aml_hw *aml_hw = aml_vif->aml_hw;
        struct aml_plat *aml_plat = aml_hw->plat;
        AML_REG_WRITE(val, aml_plat, 0, addr);
    } else {
        u8* map_address = NULL;
        if (addr & 3) {
            printk("Set Fail addr error: 0x%08x\n", addr);
            return -1;
        }
        map_address = aml_pci_get_map_address(dev, addr);
        if (map_address) {
            writel(val, map_address);
        }
    }

    printk("Set reg addr: 0x%08x value:0x%08x\n", addr, val);
    return 0;
}

int aml_sdio_start_test(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    unsigned int temperature = 0;
    unsigned int i = 0;

    unsigned char set_buf[100] = {0};
    unsigned char get_buf[100] = {0};
    unsigned char rst_buf[100] = {0};
    memset(set_buf, 0x66, 100);

    printk("sdio stress testing start\n");

    while (1) {
        if (aml_bus_type == USB_MODE) {
            aml_hw->plat->hif_ops->hi_write_sram((unsigned char *)set_buf, (unsigned char *)0x6000f4f4, 100, USB_EP4);
            aml_hw->plat->hif_ops->hi_read_sram((unsigned char *)get_buf, (unsigned char *)0x6000f4f4, 100, USB_EP4); //RXU_STAT_DESC_POOL
        } else if (aml_bus_type == SDIO_MODE) {
            aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)set_buf, (unsigned char *)0x6000f4f4, 100); //RXU_STAT_DESC_POOL
            aml_hw->plat->hif_sdio_ops->hi_random_ram_read((unsigned char *)get_buf, (unsigned char *)0x6000f4f4, 100); //RXU_STAT_DESC_POOL
        }

        if (memcmp(set_buf, get_buf, 100)) {
            if (aml_bus_type == USB_MODE) {
                temperature = aml_hw->plat->hif_ops->hi_read_word(0x00a04940, USB_EP4);
            } else if (aml_bus_type == SDIO_MODE) {
                temperature = aml_hw->plat->hif_sdio_ops->hi_random_word_read(0x00a04940);
            }
            printk(" test NG, temperature is 0x%08x\n", temperature & 0x0000ffff);
        } else {
            if (aml_bus_type == USB_MODE) {
                temperature = aml_hw->plat->hif_ops->hi_read_word(0x00a04940,USB_EP4);
            } else if (aml_bus_type == SDIO_MODE) {
                temperature = aml_hw->plat->hif_sdio_ops->hi_random_word_read(0x00a04940);
            }

            i++;
            if (i == 1000) {
                printk(" test OK, temperature is 0x%08x\n", temperature & 0x0000ffff);
                i = 0;
            }
        }
        memset(get_buf, 0x77, 100);
        if (aml_bus_type == USB_MODE) {
            aml_hw->plat->hif_ops->hi_write_sram((unsigned char *)rst_buf, (unsigned char *)0x6000f4f4, 100, USB_EP4); //RXU_STAT_DESC_POOL
        } else if (aml_bus_type == SDIO_MODE) {
            aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)rst_buf, (unsigned char *)0x6000f4f4, 100); //RXU_STAT_DESC_POOL
        }
    }

    printk("sdio stress testing end\n");
    return 0;
}


int aml_enable_wf(struct net_device *dev, int wfflag)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("aml_enable wf: 0x%08x\n", wfflag);

    _aml_enable_wf(aml_vif, wfflag);

    return 0;
}

int aml_get_efuse(struct net_device *dev, char *str_addr, union iwreq_data *wrqu, char *extra)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    unsigned int reg_val = 0;
    unsigned int addr = 0;

    addr = simple_strtol(str_addr, NULL, 0);

    printk("Get efuse addr: 0x%08x\n", addr);

    reg_val = _aml_get_efuse(aml_vif, addr);

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&0x%08x", reg_val);
    wrqu->data.length++;

    return 0;
}

int aml_set_efuse(struct net_device *dev, int addr, int val)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("Set reg addr: 0x%08x value:0x%08x\n", addr, val);

    _aml_set_efuse(aml_vif, addr, val);

    return 0;
}

int reg_cca_cond_get(struct aml_hw *aml_hw)
{
    struct aml_plat *aml_plat = aml_hw->plat;

    printk("CCA Check [%d], CCA BUSY: Prim20: %08d Second20: %08d Second40: %08d\n",
        AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, AGCCCCACAL0_ADDR_CT) & 0xfffff,
        AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, AGCCCCACAL1_ADDR_CT) & 0xffff,
        AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, AGCCCCACAL1_ADDR_CT) >> 16 & 0xffff,
        AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, AGCCCCACAL2_ADDR_CT) & 0xffff);
    return 0;
}

int aml_get_rf_reg(struct net_device *dev, char *str_addr, union iwreq_data *wrqu, char *extra)
{
    unsigned int addr = 0;
    unsigned int reg_val = 0;

    addr = simple_strtol(str_addr, NULL, 0);

    reg_val = aml_rf_reg_read(dev, addr);

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&0x%08x", reg_val);
    wrqu->data.length++;

//    printk("Get reg addr: 0x%08x value: 0x%08x\n", addr, reg_val);
    return 0;
}

int aml_get_csi_status_com(struct net_device *dev)
{
    unsigned int ret = 0;
    struct csi_status_com_get_ind ind;

    memset(&ind, 0, sizeof(struct csi_status_com_get_ind));
    ret = aml_csi_status_com_read(dev, &ind);

    if (ret == 0)
    {
        printk("time_stamp: %llu \n", ind.time_stamp);
        printk("mac_ra:     %02X%02X-%02X%02X-%02X%02X \n", ind.mac_ra[0], ind.mac_ra[1], ind.mac_ra[2], ind.mac_ra[3], ind.mac_ra[4], ind.mac_ra[5]);
        printk("mac_ta:     %02X%02X-%02X%02X-%02X%02X \n", ind.mac_ta[0], ind.mac_ta[1], ind.mac_ta[2], ind.mac_ta[3], ind.mac_ta[4], ind.mac_ta[5]);
        printk("freq_band:  %u \n", ind.frequency_band);
        printk("bw:         %u \n", ind.bw);
        printk("rssi:       %d \n", ind.rssi[0]);
        printk("snr:        %u \n", ind.snr[0]);
        printk("noise:      %u %u %u %u\n", ind.noise[0], ind.noise[1], ind.noise[2], ind.noise[3]);
        printk("phase_incr: %d \n", ind.phase_incr[0]);
        printk("pro_mode:   0x%x \n", ind.protocol_mode);
        printk("frame_type: 0x%x \n", ind.frame_type);
        printk("chain_num:  %u \n", ind.chain_num);
        printk("csi_len:    %u \n", ind.csi_len);
        printk("chan_index: %u \n", ind.primary_channel_index);
        printk("phyerr:     %u \n", ind.phyerr);
        printk("rate:       %u \n", ind.rate);
        printk("reserved:   %u %u \n", ind.reserved[0], ind.reserved[1]);
        printk("agc_code:   %u \n", ind.agc_code);
        printk("channel:    %u \n", ind.channel);
        printk("packet_idx: %u \n", ind.packet_idx);
    }

    return 0;
}

int aml_get_csi_status_sp(struct net_device *dev, int csi_mode, int csi_time_interval)
{
    int i;
    unsigned int ret = 0;
    struct csi_status_sp_get_ind ind;

    memset(&ind, 0, sizeof(struct csi_status_sp_get_ind));
    ret = aml_csi_status_sp_read(dev, csi_mode, csi_time_interval, &ind);

    if (ret == 0)
    {
        for (i = 0; i <= 255; i = i + 4)
        {
            printk("Num: 0x%02x:0x%08x--0x%02x:0x%08x--0x%02x:0x%08x--0x%02x:0x%08x \n", \
                    i, ind.csi[i], i + 1, ind.csi[i + 1], i + 2, ind.csi[i + 2], i + 3, ind.csi[i + 3]);
        }
    }

    return 0;
}

int aml_iwpriv_set_debug_switch(char *switch_str)
{
    int debug_switch = 0;
    if (strstr(switch_str,"_off") != NULL)
        debug_switch = AML_DBG_OFF;
    else if (strstr(switch_str,"_on") != NULL)
        debug_switch = AML_DBG_ON;
    else
        ERROR_DEBUG_OUT("input error\n");
    return debug_switch;
}



int aml_set_debug(struct net_device *dev, char *debug_str)
{
    if (debug_str == NULL || strlen(debug_str) <= 0) {
        ERROR_DEBUG_OUT("debug modules is NULL\n");
        return -1;
    }
    if (strstr(debug_str,"tx") != NULL) {
        if (aml_iwpriv_set_debug_switch(debug_str) == AML_DBG_OFF) {
            g_dbg_modules &= ~(AML_DBG_MODULES_TX);
            return 0;
        }
        g_dbg_modules |= AML_DBG_MODULES_TX;
    } else if (strstr(debug_str,"rx") != NULL){
       if (aml_iwpriv_set_debug_switch(debug_str) == AML_DBG_OFF) {
            g_dbg_modules &= ~(AML_DBG_MODULES_RX);
            return 0;
        }
        g_dbg_modules |= AML_DBG_MODULES_RX;
    }else {
        ERROR_DEBUG_OUT("input error\n");
    }
    return 0;
}

static int aml_set_p2p_oppps(struct net_device *dev, int ctw)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;
    struct mm_set_p2p_oppps_cfm cfm;
    if (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_P2P_GO) {
        /* Forward request to the embedded and wait for confirmation */
        aml_send_p2p_oppps_req(aml_hw, aml_vif, (u8)ctw, &cfm);
    }
    return 0;
}

static int aml_set_amsdu_max(struct net_device *dev, int amsdu_max)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    aml_hw->mod_params->amsdu_maxnb = amsdu_max;
    aml_adjust_amsdu_maxnb(aml_hw);
    return 0;
}

static int aml_set_amsdu_tx(struct net_device *dev, int amsdu_tx)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    if (aml_hw->mod_params->amsdu_tx == amsdu_tx) {
        printk("amsdu tx did not change, ignore\n");
        return 0;
    }

    aml_hw->mod_params->amsdu_tx = amsdu_tx;
    printk("set amsdu_tx:0x%x success\n", amsdu_tx);
    return _aml_set_amsdu_tx(aml_hw, amsdu_tx);
}

static int aml_set_ldpc(struct net_device *dev, int ldpc)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    if (aml_hw->mod_params->ldpc_on == ldpc) {
        printk("ldpc did not change, ignore\n");
        return 0;
    }
    printk("set ldpc: 0x%x success\n", ldpc);
    aml_hw->mod_params->ldpc_on = ldpc;
    // LDPC is mandatory for HE40 and above, so if LDPC is not supported, then disable
    // support for 40 and 80MHz
    if (aml_hw->mod_params->he_on && !aml_hw->mod_params->ldpc_on)
    {
        aml_hw->mod_params->use_80 = false;
        aml_hw->mod_params->use_2040 = false;
    }
    aml_set_he_capa(aml_hw, aml_hw->wiphy);
    aml_set_vht_capa(aml_hw, aml_hw->wiphy);
    aml_set_ht_capa(aml_hw, aml_hw->wiphy);

    return aml_set_ldpc_tx(aml_hw, aml_vif);
}

static int aml_set_tx_lft(struct net_device *dev, int tx_lft)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    if (aml_hw->mod_params->tx_lft == tx_lft) {
        printk("tx_lft did not change, ignore\n");
        return 0;
    }

    aml_hw->mod_params->tx_lft= tx_lft;
    printk("set tx_lft:0x%x success\n", tx_lft);
    return _aml_set_tx_lft(aml_hw, tx_lft);
}

static int aml_set_ps_mode(struct net_device *dev, int ps_mode)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;
    if ((ps_mode != MM_PS_MODE_OFF) && (ps_mode != MM_PS_MODE_ON) && (ps_mode != MM_PS_MODE_ON_DYN))
    {
        printk("param err, please reset\n");
        return -1;
    }
    printk("set ps_mode:0x%x success\n", ps_mode);
    return aml_send_me_set_ps_mode(aml_hw, ps_mode);
}

static int aml_send_twt_req(struct net_device *dev, char *str_param, union iwreq_data *wrqu, char *extra)
{
    struct twt_conf_tag twt_conf;
    struct twt_setup_cfm twt_setup_cfm;
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    u8 setup_type = MAC_TWT_SETUP_REQ;
    u8 vif_idx = aml_vif->vif_index;

    twt_conf.flow_type = 0;
    twt_conf.wake_int_exp = 0;
    twt_conf.wake_dur_unit = 0;
    twt_conf.min_twt_wake_dur = 0;
    twt_conf.wake_int_mantissa = 0;

    if (sscanf(str_param, "%d %d %d %d %d %d", &setup_type,
        &twt_conf.flow_type, &twt_conf.wake_int_exp, &twt_conf.wake_dur_unit, &twt_conf.min_twt_wake_dur, &twt_conf.wake_int_mantissa) != 6) {
        printk("param erro \n");
    }

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "setup_type=%d flow_type=%d wake_int_exp=%d wake_dur_unit=%d min_twt_wake_dur=%d wake_int_mantissa=%d",
        setup_type, twt_conf.flow_type, twt_conf.wake_int_exp,  twt_conf.wake_dur_unit,  twt_conf.min_twt_wake_dur, twt_conf.wake_int_mantissa);
    wrqu->data.length++;

    printk("%s [%s]\n", __func__, extra);
    return aml_send_twt_request(aml_hw, setup_type, vif_idx, &twt_conf, &twt_setup_cfm);
}

void aml_print_buf(char *buf, int len)
{
    char tmp[PRINT_BUF_SIZE] = {0};
    while (1) {
        if (len > PRINT_BUF_SIZE) {
            memcpy(tmp, buf, PRINT_BUF_SIZE);
            printk("%s", tmp);
            len -= PRINT_BUF_SIZE;
            buf += PRINT_BUF_SIZE;
        } else {
            printk("%s\n", buf);
            break;
        }
    }
}

int aml_print_acs_info(struct aml_hw *priv)
{
    struct wiphy *wiphy = priv->wiphy;
    char *buf = NULL;
    int buf_size = (SCAN_CHANNEL_MAX + 1) * 43;
    int survey_cnt = 0;
    int len = 0;
    int band, chan_cnt;

    if ((buf = vmalloc(buf_size)) == NULL)
        return -1;

    mutex_lock(&priv->dbgdump.mutex);
    len += scnprintf(buf, buf_size - 1,  "FREQ      TIME(ms)     BUSY(ms)     NOISE(dBm)\n");

    for (band = NL80211_BAND_2GHZ; band <= NL80211_BAND_5GHZ; band++) {
        for (chan_cnt = 0; chan_cnt < wiphy->bands[band]->n_channels; chan_cnt++) {
            struct aml_survey_info *p_survey_info = &priv->survey[survey_cnt];
            struct ieee80211_channel *p_chan = &wiphy->bands[band]->channels[chan_cnt];

            if (p_survey_info->filled) {
                len += scnprintf(&buf[len], buf_size - len - 1,
                                  "%d    %03d         %03d          %d\n",
                                  p_chan->center_freq,
                                  p_survey_info->chan_time_ms,
                                  p_survey_info->chan_time_busy_ms,
                                  p_survey_info->noise_dbm);
            } else {
                len += scnprintf(&buf[len], buf_size -len -1,
                                  "%d    NOT AVAILABLE\n",
                                  p_chan->center_freq);
            }

            survey_cnt++;
        }
    }

    mutex_unlock(&priv->dbgdump.mutex);
    aml_print_buf(buf, len);
    vfree(buf);
    return 0;
}

int aml_print_last_rx_info(struct aml_hw *priv, struct aml_sta *sta)
{
#ifdef CONFIG_AML_DEBUGFS
    struct aml_rx_rate_stats *rate_stats;
    char *buf;
    int bufsz, i, len = 0;
    unsigned int fmt, pre, bw, nss, mcs, gi, dcm = 0;
    struct rx_vector_1 *last_rx;
    char hist[] = "##################################################";
    int hist_len = sizeof(hist) - 1;
    u8 nrx;
    rate_stats = &sta->stats.rx_rate;
    bufsz = (rate_stats->rate_cnt * ( 50 + hist_len) + 200);
    buf = kmalloc(bufsz + 1, GFP_ATOMIC);
    if (buf == NULL)
        return 0;

    // Get number of RX paths
    nrx = (priv->version_cfm.version_phy_1 & MDM_NRX_MASK) >> MDM_NRX_LSB;

    len += scnprintf(buf, bufsz,
                        "\nRX rate info for %02X:%02X:%02X:%02X:%02X:%02X:\n",
                        sta->mac_addr[0], sta->mac_addr[1], sta->mac_addr[2],
                        sta->mac_addr[3], sta->mac_addr[4], sta->mac_addr[5]);

    // Display Statistics
    for (i = 0 ; i < rate_stats->size ; i++ ) {
        if (rate_stats->table[i]) {
            union aml_rate_ctrl_info rate_config;
            u64 percent = div_u64(rate_stats->table[i] * 1000, rate_stats->cpt);
            u64 p;
            int ru_size;
            u32 rem;

            idx_to_rate_cfg(i, &rate_config, &ru_size);
            len += print_rate_from_cfg(&buf[len], bufsz - len,
                                       rate_config.value, NULL, ru_size);
            p = div_u64((percent * hist_len), 1000);
            len += scnprintf(&buf[len], bufsz - len, ": %9d(%2d.%1d%%)%.*s\n",
                             rate_stats->table[i],
                             div_u64_rem(percent, 10, &rem), rem, p, hist);
        }
    }

    // Display detailed info of the last received rate
    last_rx = &sta->stats.last_rx.rx_vect1;
    len += scnprintf(&buf[len], bufsz - len,"\nLast received rate\n"
                     "type               rate     LDPC STBC BEAMFM DCM DOPPLER %s\n",
                     (nrx > 1) ? "rssi1(dBm) rssi2(dBm)" : "rssi(dBm)");

    fmt = last_rx->format_mod;
    bw = last_rx->ch_bw;
    pre = last_rx->pre_type;
    if (fmt >= FORMATMOD_HE_SU) {
        mcs = last_rx->he.mcs;
        nss = last_rx->he.nss;
        gi = last_rx->he.gi_type;
        if ((fmt == FORMATMOD_HE_MU) || (fmt == FORMATMOD_HE_ER))
            bw = last_rx->he.ru_size;
        dcm = last_rx->he.dcm;
    } else if (fmt == FORMATMOD_VHT) {
        mcs = last_rx->vht.mcs;
        nss = last_rx->vht.nss;
        gi = last_rx->vht.short_gi;
    } else if (fmt >= FORMATMOD_HT_MF) {
        mcs = last_rx->ht.mcs % 8;
        nss = last_rx->ht.mcs / 8;
        gi = last_rx->ht.short_gi;
    } else {
        BUG_ON((mcs = legrates_lut[last_rx->leg_rate].idx) == -1);
        nss = 0;
        gi = 0;
    }

    len += print_rate(&buf[len], bufsz - len, fmt, nss, mcs, bw, gi, pre, dcm, NULL);

    /* flags for HT/VHT/HE */
    if (fmt >= FORMATMOD_HE_SU) {
        len += scnprintf(&buf[len], bufsz - len, "  %c    %c     %c    %c     %c",
                         last_rx->he.fec ? 'L' : ' ',
                         last_rx->he.stbc ? 'S' : ' ',
                         last_rx->he.beamformed ? 'B' : ' ',
                         last_rx->he.dcm ? 'D' : ' ',
                         last_rx->he.doppler ? 'D' : ' ');
    } else if (fmt == FORMATMOD_VHT) {
        len += scnprintf(&buf[len], bufsz - len, "  %c    %c     %c           ",
                         last_rx->vht.fec ? 'L' : ' ',
                         last_rx->vht.stbc ? 'S' : ' ',
                         last_rx->vht.beamformed ? 'B' : ' ');
    } else if (fmt >= FORMATMOD_HT_MF) {
        len += scnprintf(&buf[len], bufsz - len, "  %c    %c                  ",
                         last_rx->ht.fec ? 'L' : ' ',
                         last_rx->ht.stbc ? 'S' : ' ');
    } else {
        len += scnprintf(&buf[len], bufsz - len, "                         ");
    }
    if (nrx > 1) {
        /* coverity[assigned_value] - len is used */
        len += scnprintf(&buf[len], bufsz - len, "       %-4d       %d\n",
                         last_rx->rssi1, last_rx->rssi1);
    } else {
        /* coverity[assigned_value] - len is used */
        len += scnprintf(&buf[len], bufsz - len, "      %d\n", last_rx->rssi1);
    }

    aml_print_buf(buf, len);
    kfree(buf);
#endif
    return 0;
}

int aml_print_stats(struct aml_hw *priv)
{
    char *buf;
    int ret;
    int i, skipped;
#ifdef CONFIG_AML_SPLIT_TX_BUF
    int per;
#endif
    int bufsz = (NX_TXQ_CNT) * 20 + (ARRAY_SIZE(priv->stats->amsdus_rx) + 1) * 40
        + (ARRAY_SIZE(priv->stats->ampdus_tx) * 30);

    buf = kmalloc(bufsz, GFP_ATOMIC);
    if (buf == NULL)
        return 0;

    ret = scnprintf(buf, bufsz, "TXQs CFM balances ");
    for (i = 0; i < NX_TXQ_CNT; i++)
        ret += scnprintf(&buf[ret], bufsz - ret,
                            "    [%1d]:%3d", i,
                            priv->stats->cfm_balance[i]);

    ret += scnprintf(&buf[ret], bufsz - ret, "\n");

#ifdef CONFIG_AML_SPLIT_TX_BUF
    ret += scnprintf(&buf[ret], bufsz - ret,
                       "\nAMSDU[len]         done          failed   received\n");
    for (i = skipped = 0; i < NX_TX_PAYLOAD_MAX; i++) {
        if (priv->stats->amsdus[i].done) {
            per = DIV_ROUND_UP((priv->stats->amsdus[i].failed) *
                                100, priv->stats->amsdus[i].done);
        } else if (priv->stats->amsdus_rx[i]) {
            per = 0;
        } else {
            per = 0;
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret, "     ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                          "     [%2d]      %10d %8d(%3d%%) %10d\n",    i ? i + 1 : i,
                          priv->stats->amsdus[i].done,
                          priv->stats->amsdus[i].failed, per,
                          priv->stats->amsdus_rx[i]);
    }

    for (; i < ARRAY_SIZE(priv->stats->amsdus_rx); i++) {
        if (!priv->stats->amsdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret, "     ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                          "     [%2d]                                %10d\n",
                          i + 1, priv->stats->amsdus_rx[i]);
    }
#else
    ret += scnprintf(&buf[ret], bufsz - ret,
                      "\nAMSDU[len]     received\n");
    for (i = skipped = 0; i < ARRAY_SIZE(priv->stats->amsdus_rx); i++) {
        if (!priv->stats->amsdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret,  " ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                          "     [%2d]      %10d\n",
                          i + 1, priv->stats->amsdus_rx[i]);
    }

#endif /* CONFIG_AML_SPLIT_TX_BUF */

    ret += scnprintf(&buf[ret], bufsz - ret,
                       "\nAMPDU[len]       done  received\n");
    for (i = skipped = 0; i < ARRAY_SIZE(priv->stats->ampdus_tx); i++) {
        if (!priv->stats->ampdus_tx[i] && !priv->stats->ampdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret,  "   ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                         "     [%2d]     %9d %9d\n", i ? i + 1 : i,
                         priv->stats->ampdus_tx[i], priv->stats->ampdus_rx[i]);
    }
    /* coverity[assigned_value] - len is used */
    ret += scnprintf(&buf[ret], bufsz - ret,
                     "#mpdu missed          %9d\n",
                     priv->stats->ampdus_rx_miss);

    aml_print_buf(buf, ret);
    kfree(buf);
    return 0;

}

int aml_print_rate_info( struct aml_hw *aml_hw, struct aml_sta *sta)
{
    char *buf;
    int bufsz, len = 0;
    int i = 0;
    int error = 0;
    struct me_rc_stats_cfm me_rc_stats_cfm;
    unsigned int no_samples;
    struct st *st;
     /* Forward the information to the LMAC */
    if ((error = aml_send_me_rc_stats(aml_hw, sta->sta_idx, &me_rc_stats_cfm)))
        return error;

    no_samples = me_rc_stats_cfm.no_samples;
    if (no_samples == 0)
        return 0;

    bufsz = no_samples * LINE_MAX_SZ + 500;

    buf = kmalloc(bufsz + 1, GFP_ATOMIC);
    if (buf == NULL)
        return 0;

    st = kmalloc(sizeof(struct st) * no_samples, GFP_ATOMIC);
    if (st == NULL) {
        kfree(buf);
        return 0;
    }

    for (i = 0; i < no_samples; i++) {
        unsigned int tp, eprob;
        len = print_rate_from_cfg(st[i].line, LINE_MAX_SZ,
                                  me_rc_stats_cfm.rate_stats[i].rate_config,
                                  (int *)&st[i].r_idx, 0);

        if (me_rc_stats_cfm.sw_retry_step != 0) {
            len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len,  "%c",
                    me_rc_stats_cfm.retry_step_idx[me_rc_stats_cfm.sw_retry_step] == i ? '*' : ' ');
        }
        else {
            len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, " ");
        }
        len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c",
                me_rc_stats_cfm.retry_step_idx[0] == i ? 'T' : ' ');
        len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c",
                me_rc_stats_cfm.retry_step_idx[1] == i ? 't' : ' ');
        len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c ",
                me_rc_stats_cfm.retry_step_idx[2] == i ? 'P' : ' ');

        tp = me_rc_stats_cfm.tp[i] / 10;
        len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, " %4u.%1u",
                         tp / 10, tp % 10);

        eprob = ((me_rc_stats_cfm.rate_stats[i].probability * 1000) >> 16) + 1;
        scnprintf(&st[i].line[len],LINE_MAX_SZ - len,
                         "  %4u.%1u %5u(%6u)  %6u",
                         eprob / 10, eprob % 10,
                         me_rc_stats_cfm.rate_stats[i].success,
                         me_rc_stats_cfm.rate_stats[i].attempts,
                         me_rc_stats_cfm.rate_stats[i].sample_skipped);
    }
    len = scnprintf(buf, bufsz ,
                     "\nTX rate info for %02X:%02X:%02X:%02X:%02X:%02X:\n",
                     sta->mac_addr[0], sta->mac_addr[1], sta->mac_addr[2],
                     sta->mac_addr[3], sta->mac_addr[4], sta->mac_addr[5]);

    len += scnprintf(&buf[len], bufsz - len,
            "   # type               rate             tpt   eprob    ok(   tot)   skipped\n");

    // add sorted statistics to the buffer
    sort(st, no_samples, sizeof(st[0]), compare_idx, NULL);
    for (i = 0; i < no_samples; i++) {
        len += scnprintf(&buf[len], bufsz - len, "%s\n", st[i].line);
    }

    // display HE TB statistics if any
    if (me_rc_stats_cfm.rate_stats[RC_HE_STATS_IDX].rate_config != 0) {
        unsigned int tp, eprob;
        struct rc_rate_stats *rate_stats = &me_rc_stats_cfm.rate_stats[RC_HE_STATS_IDX];
        int ru_index = rate_stats->ru_and_length & 0x07;
        int ul_length = rate_stats->ru_and_length >> 3;

        len += scnprintf(&buf[len], bufsz - len,
                         "\nHE TB rate info:\n");

        len += scnprintf(&buf[len], bufsz - len,
                "     type               rate             tpt   eprob    ok(   tot)   ul_length\n     ");
        len += print_rate_from_cfg(&buf[len], bufsz - len, rate_stats->rate_config,
                                   NULL, ru_index);

        tp = me_rc_stats_cfm.tp[RC_HE_STATS_IDX] / 10;
        len += scnprintf(&buf[len], bufsz - len, "      %4u.%1u",
                         tp / 10, tp % 10);

        eprob = ((rate_stats->probability * 1000) >> 16) + 1;
        len += scnprintf(&buf[len],bufsz - len,
                         "  %4u.%1u %5u(%6u)  %6u\n",
                         eprob / 10, eprob % 10,
                         rate_stats->success,
                         rate_stats->attempts,
                         ul_length);
    }

    len += scnprintf(&buf[len], bufsz - len, "\n MPDUs AMPDUs AvLen trialP");
    /* coverity[assigned_value] - len is used */
    len += scnprintf(&buf[len], bufsz - len, "\n%6u %6u %3d.%1d %6u\n",
                     me_rc_stats_cfm.ampdu_len,
                     me_rc_stats_cfm.ampdu_packets,
                     me_rc_stats_cfm.avg_ampdu_len >> 16,
                     ((me_rc_stats_cfm.avg_ampdu_len * 10) >> 16) % 10,
                     me_rc_stats_cfm.sample_wait);

    aml_print_buf(buf, len);
    kfree(buf);
    kfree(st);

    return 0;
}
static int aml_get_rate_info(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    u8 i = 0;
    struct aml_sta *sta = NULL;
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = aml_hw->sta_table + i;
        if (sta && sta->valid && (aml_vif->vif_index == sta->vif_idx)) {
            aml_print_rate_info(aml_hw, sta);
        }
    }
    return 0;
}

static int aml_cca_check(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    reg_cca_cond_get(aml_hw);
    return 0;
}

static int aml_get_tx_stats(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;

    aml_print_stats(aml_hw);
    return 0;
}

static int aml_get_acs_info(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;

    aml_print_acs_info(aml_hw);
    return 0;
}

static int aml_get_clock(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_plat *aml_plat = aml_hw->plat;
    uint32_t temp_value =0;


    if (PCIE_MODE == aml_bus_type)
    {
        AML_REG_WRITE(0xffffffff, aml_plat, AML_ADDR_SYSTEM, CLK_ADDR0 );
        AML_REG_WRITE(0xffffffff, aml_plat, AML_ADDR_SYSTEM, CLK_ADDR1 );
        temp_value = AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, ENA_CLK_ADDR);
        temp_value |= BIT(1);
        AML_REG_WRITE(temp_value, aml_plat, AML_ADDR_SYSTEM, ENA_CLK_ADDR );
        printk("clock measure start (MHz):\n 0x60805344 dac\t: %d \n 0x60805340 plf\t: %d \n 0x6080533c macwt\t: %d \n 0x60805338 macdaccore: %d \n\
               0x60805334 la\t: %d\n 0x60805330 mpif\t: %d\n 0x6080532c phy\t: %d\n 0x60805328 vtb\t: %d\n 0x60805324 feref\t: %d\n\
               0x60805320 ref80\t: %d\n 0x6080531c ref40\t: %d\n 0x60805314 ldpc_rx\t: %d \n 0x60805310 ref_44\t: %d\nclock measure end",
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, DAC_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, PLF_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, MACWT_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, MACCORE_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, LA_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, MPIF_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, PHY_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, VTB_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, FEREF_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, REF80_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, REF40_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, LDPC_RX_CLK_ADDR)/1000,
               AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, REF_44_CLK_ADDR)/1000);
    }
    return 0;
}


static int aml_get_chan_list_info(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct wiphy *wiphy = aml_hw->wiphy;
    int i;
    const struct ieee80211_reg_rule *reg_rule;

    if (wiphy->bands[NL80211_BAND_2GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_2GHZ];
        printk("2.4G channels\n");
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_DISABLED)
                continue;
            reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(b->channels[i].center_freq));
            if (IS_ERR(reg_rule))
                continue;
            printk("channel:%d\tfrequency:%d\tmax_bandwidth:%dMHz\t\n",
                aml_ieee80211_freq_to_chan(b->channels[i].center_freq, NL80211_BAND_2GHZ),
                b->channels[i].center_freq, KHZ_TO_MHZ(reg_rule->freq_range.max_bandwidth_khz));
            if (i == MAC_DOMAINCHANNEL_24G_MAX)
                break;
        }
    }

    if (wiphy->bands[NL80211_BAND_5GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_5GHZ];
        printk("5G channels:\n");
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_DISABLED)
                continue;
            reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(b->channels[i].center_freq));
            if (IS_ERR(reg_rule))
                continue;
            printk("channel:%d\tfrequency:%d\tmax_bandwidth:%dMHz\t\n",
                aml_ieee80211_freq_to_chan(b->channels[i].center_freq, NL80211_BAND_5GHZ),
                b->channels[i].center_freq, KHZ_TO_MHZ(reg_rule->freq_range.max_bandwidth_khz));
            if (i == MAC_DOMAINCHANNEL_5G_MAX)
                break;
        }
    }

    return 0;
}

static void aml_get_rx_regvalue(struct aml_plat *aml_plat)
{
    printk("<-------------------rx reg value start --------------------->\n");
    printk("rx_end     :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc06088));
    printk("frame_ok   :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc06080));
    printk("frame_bad  :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc06084));
    printk("rx_error   :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc0608c));
    printk("phy_error  :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc06098));

    printk("rxbuffer1--->:\n");
    printk("start      :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081c8));
    printk("end        :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081cc));
    printk("read       :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081d0));
    printk("write      :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081d4));

    printk("rxbuffer2--->:\n");
    printk("start      :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081d8));
    printk("end        :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081dc));
    printk("read       :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081e0));
    printk("write      :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xb081e4));

    printk("SNR        :0x%x\n", AML_REG_READ(aml_plat, AML_ADDR_SYSTEM, 0xc0005c)&0xfff);

    printk("data avg rssi :%d dBm\n", ((AML_REG_READ(aml_plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_RSSI)&0xffff0000) >> 16) - 256);

    printk("bcn  avg rssi :%d dBm\n", (AML_REG_READ(aml_plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_RSSI)&0xffff) - 256);

    printk("<-------------------rx reg value end ---------------------->\n");
}
static int aml_get_last_rx(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_plat *aml_plat = aml_hw->plat;
    u8 i = 0;
    struct aml_sta *sta = NULL;
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = aml_hw->sta_table + i;
        if (sta && sta->valid && (aml_vif->vif_index == sta->vif_idx)) {
            aml_print_last_rx_info(aml_hw, sta);
            aml_get_rx_regvalue(aml_plat);
        }
    }
    return 0;
}

static int aml_clear_last_rx(struct net_device *dev)
{
#ifdef CONFIG_AML_DEBUGFS
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    u8 i = 0;
    struct aml_sta *sta = NULL;

    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = aml_hw->sta_table + i;
        if (sta && sta->valid && (aml_vif->vif_index == sta->vif_idx)) {
            /* Prevent from interrupt preemption as these statistics are updated under
             * interrupt */
            spin_lock_bh(&aml_hw->tx_lock);
            memset(sta->stats.rx_rate.table, 0,
                   sta->stats.rx_rate.size * sizeof(sta->stats.rx_rate.table[0]));
            sta->stats.rx_rate.cpt = 0;
            sta->stats.rx_rate.rate_cnt = 0;
            spin_unlock_bh(&aml_hw->tx_lock);
        }
    }
#endif
    return 0;
}


static int aml_get_amsdu_max(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    printk("current amsdu_max: %d\n", aml_hw->mod_params->amsdu_maxnb);
    return 0;
}

static int aml_get_amsdu_tx(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    printk("current amsdu_tx: %d\n", aml_hw->mod_params->amsdu_tx);
    return 0;
}

static int aml_get_ldpc(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    printk("current ldpc: %d\n", aml_hw->mod_params->ldpc_on);
    return 0;
}

static int aml_get_tx_lft(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    printk("current tx_lft: %d\n", aml_hw->mod_params->tx_lft);
    return 0;
}


int aml_get_txq(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_vif *vif;
    char *buf;
    int idx, res;
    size_t bufsz = ((NX_VIRT_DEV_MAX * (VIF_HDR_MAX_LEN + 2 * VIF_SEP_LEN)) +
                    (NX_REMOTE_STA_MAX * STA_HDR_MAX_LEN) +
                    ((NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX + NX_NB_TXQ) *
                     TXQ_HDR_MAX_LEN) + CAPTION_LEN);

    buf = kmalloc(bufsz, GFP_ATOMIC);
    if (buf == NULL)
        return 0;

    bufsz--;
    idx = 0;

    res = scnprintf(&buf[idx], bufsz, CAPTION);
    idx += res;
    bufsz -= res;

    //spin_lock_bh(&aml_hw->tx_lock);
    list_for_each_entry(vif, &aml_hw->vifs, list) {
        res = scnprintf(&buf[idx], bufsz, "\n"VIF_SEP);
        idx += res;
        bufsz -= res;
        res = aml_dbgfs_txq_vif(&buf[idx], bufsz, vif, aml_hw);
        idx += res;
        bufsz -= res;
        res = scnprintf(&buf[idx], bufsz, VIF_SEP);
        idx += res;
        bufsz -= res;
    }
    //spin_unlock_bh(&aml_hw->tx_lock);

    printk("%s\n", buf);
    kfree(buf);

    return 0;
}

static int aml_get_buf_state(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    if (aml_hw->rx_buf_state & FW_BUFFER_EXPAND) {
        printk("rxbuf: 256k, txbuf: 128k\n");
    } else if (aml_hw->rx_buf_state & FW_BUFFER_NARROW) {
        printk("rxbuf: 128k, txbuf: 256k\n");
    } else {
        printk("err: rx_buf_state[%x]\n", aml_hw->rx_buf_state);
    }

    return 0;
}

static int aml_set_txpage_once(struct net_device *dev, int txpage)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    if (aml_hw->g_tx_param.tx_page_once == txpage) {
        printk("txpage did not change, ignore\n");
        return 0;
    }

    aml_hw->g_tx_param.tx_page_once = txpage;
    printk("set tx_page_once:0x%x success\n", txpage);
    return 0;
}

static int aml_set_txcfm_tri_tx(struct net_device *dev, int tri_tx_thr)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw * aml_hw = aml_vif->aml_hw;

    if (aml_hw->g_tx_param.txcfm_trigger_tx_thr == tri_tx_thr) {
        printk("tri_tx_thr did not change, ignore\n");
        return 0;
    }

    aml_hw->g_tx_param.txcfm_trigger_tx_thr = tri_tx_thr;
    printk("set tri_tx_thr:0x%x success\n", tri_tx_thr);
    return 0;
}


static int aml_send_twt_teardown(struct net_device *dev, int id)
{
    struct twt_teardown_req twt_teardown;
    struct twt_teardown_cfm twt_teardown_cfm;
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;

    twt_teardown.neg_type = 0; //Individual TWT
    twt_teardown.all_twt = 0;
    twt_teardown.vif_idx = aml_vif->vif_index;
    twt_teardown.id = id;

    AML_INFO("flow id:%d\n", twt_teardown.id);
    return _aml_send_twt_teardown(aml_hw, &twt_teardown, &twt_teardown_cfm);
}

int aml_set_macbypass(struct net_device *dev, int format_type, int bandwidth, int rate, int siso_or_mimo)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("set macbypass, format_type:%d, bandwidth:%d, rate:%d, siso_or_mimo:%d!\n",
           format_type, bandwidth, rate, siso_or_mimo);

    _aml_set_macbypass(aml_vif, format_type, bandwidth, rate, siso_or_mimo);

    return 0;
}

int aml_set_stop_macbypass(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("Stop macbypass!\n");

    _aml_set_stop_macbypass(aml_vif);

    return 0;
}

int aml_set_stbc(struct net_device *dev, int stbc_on)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;

    if (aml_hw->mod_params->stbc_on == stbc_on) {
        printk("stbc_on did not change, ignore\n");
        return 0;
    }

    aml_hw->mod_params->stbc_on = stbc_on;
    printk("set stbc_on:%d success\n", stbc_on);

    /* Set VHT capabilities */
    aml_set_vht_capa(aml_hw, aml_hw->wiphy);

    /* Set HE capabilities */
    aml_set_he_capa(aml_hw,  aml_hw->wiphy);

    /* Set HT capabilities */
    aml_set_ht_capa(aml_hw,  aml_hw->wiphy);

    return _aml_set_stbc(aml_hw, aml_vif->vif_index, stbc_on);
}

static int aml_get_stbc(struct net_device *dev)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    printk("current stbc: %d\n", aml_hw->mod_params->stbc_on);
    return 0;
}

static int aml_dump_reg(struct net_device *dev, int addr, int size)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_plat *aml_plat = aml_hw->plat;
    struct file *fp = NULL;

    int len = 0, i = 0;
    char *la_buf = NULL;

    u8 *map_address = NULL;
    u8 *address = (u8 *)(unsigned long)addr;
    map_address = aml_pci_get_map_address(dev, addr);
    if (!map_address) {
        printk("%s: map_address erro\n", __func__);
        return 0;
    }

    la_buf = kmalloc(REG_DUMP_SIZE, GFP_ATOMIC);
    if (!la_buf) {
         printk("%s: malloc buf erro\n", __func__);
         return 0;
    }

    fp = FILE_OPEN(reg_path, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, 0644);
    if (IS_ERR(fp)) {
        printk("%s: mactrace file open failed: PTR_ERR(fp) = %d\n", __func__, PTR_ERR(fp));
        goto err;
    }
    fp->f_pos = 0;

    memset(la_buf, 0, REG_DUMP_SIZE);
    len += scnprintf(&la_buf[len], (REG_DUMP_SIZE - len), "========dump range [0x%x ---- 0x%x], Size 0x%x========\n",
            address, address + size, size);

    if (aml_bus_type == PCIE_MODE) {
        for (i = 0; i < size / 4; i++) {
            len += scnprintf(&la_buf[len], (REG_DUMP_SIZE - len), "addr 0x%x ----- value 0x%x\n",
                address + i * 4, readl(map_address+ i*4));

            if ((REG_DUMP_SIZE - len) < 38) {
                FILE_WRITE(fp, la_buf, len, &fp->f_pos);

                len = 0;
                memset(la_buf, 0, REG_DUMP_SIZE);
            }
        }
    } else {
        for (i = 0; i < size / 4; i++) {
            len += scnprintf(&la_buf[len], (REG_DUMP_SIZE - len), "addr 0x%x ----- value 0x%x\n",
                address + i * 4, AML_REG_READ(aml_plat, 0, (u64)(address + i*4)));

            if ((REG_DUMP_SIZE - len) < 38) {
                FILE_WRITE(fp, la_buf, len, &fp->f_pos);

                len = 0;
                memset(la_buf, 0, REG_DUMP_SIZE);
            }
        }
    }
    if (len != 0) {
        FILE_WRITE(fp, la_buf, len, &fp->f_pos);
    }

    FILE_CLOSE(fp, NULL);

err:
    kfree(la_buf);
    return 0;
}

static int aml_emb_la_dump(struct net_device *dev)
{
    struct file *fp = NULL;
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_plat *aml_plat = aml_hw->plat;

    int len = 0, i = 0;
    char *la_buf = NULL;
    u8 *map_address = NULL;
    map_address = aml_pci_get_map_address(dev, LA_MEMORY_BASE_ADDRESS);
    if (!map_address) {
        printk("%s: map_address erro\n", __func__);
        return 0;
    }

    la_buf = kmalloc(LA_BUF_SIZE, GFP_ATOMIC);
    if (!la_buf) {
         printk("%s: malloc buf erro\n", __func__);
         return 0;
    }

    fp = FILE_OPEN(mactrace_path, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, 0644);
    if (IS_ERR(fp)) {
        printk("%s: mactrace file open failed: PTR_ERR(fp) = %d\n", __func__, PTR_ERR(fp));
        goto err;
    }
    fp->f_pos = 0;

    memset(la_buf, 0, LA_BUF_SIZE);

    if (aml_bus_type == PCIE_MODE) {
        for (i=0; i < 0x3fff; i+=2) {
            len += scnprintf(&la_buf[len], (LA_BUF_SIZE - len), "%08x%08x\n",
                readl(map_address+((1+i)*4)), readl(map_address+(i*4)));

            if ((LA_BUF_SIZE - len) < 20) {
                FILE_WRITE(fp, la_buf, len, &fp->f_pos);

                len = 0;
                memset(la_buf, 0, LA_BUF_SIZE);
            }
        }

        if (len != 0) {
            FILE_WRITE(fp, la_buf, len, &fp->f_pos);
        }
    } else {
         for (i=0; i < 0x3fff; i+=2) {
             len += scnprintf(&la_buf[len], (LA_BUF_SIZE - len), "%08x%08x\n",
                 AML_REG_READ(aml_plat, 0, LA_MEMORY_BASE_ADDRESS+((1+i)*4)),
                 AML_REG_READ(aml_plat, 0, LA_MEMORY_BASE_ADDRESS+(i*4)));

             if ((LA_BUF_SIZE - len) < 20) {
                 FILE_WRITE(fp, la_buf, len, &fp->f_pos);

                 len = 0;
                 memset(la_buf, 0, LA_BUF_SIZE);
             }
         }

         if (len != 0) {
             FILE_WRITE(fp, la_buf, len, &fp->f_pos);
         }
    }

    FILE_CLOSE(fp, NULL);

err:
    kfree(la_buf);
    return 0;
}

int aml_set_pt_calibration(struct net_device *dev, int pt_cali_val)
{
    struct aml_vif *aml_vif = netdev_priv(dev);

    printk("set pt calibration, pt calibration conf:%x\n", pt_cali_val);

    _aml_set_pt_calibration(aml_vif, pt_cali_val);

    return 0;
}

int aml_get_xosc_offset(struct net_device *dev,union iwreq_data *wrqu, char *extra)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    unsigned int reg_val = 0;
    U8 xosc_ctune = 0;
    U8 offset_power_wf0_2g_l = 0;
    U8 offset_power_wf0_2g_m = 0;
    U8 offset_power_wf0_2g_h = 0;
    U8 offset_power_wf0_5200 = 0;
    U8 offset_power_wf0_5300 = 0;
    U8 offset_power_wf0_5530 = 0;
    U8 offset_power_wf0_5660 = 0;
    U8 offset_power_wf0_5780 = 0;
    U8 offset_power_wf1_2g_l = 0;
    U8 offset_power_wf1_2g_m = 0;
    U8 offset_power_wf1_2g_h = 0;
    U8 offset_power_wf1_5200 = 0;
    U8 offset_power_wf1_5300 = 0;
    U8 offset_power_wf1_5530 = 0;
    U8 offset_power_wf1_5660 = 0;
    U8 offset_power_wf1_5780 = 0;

    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_05);
    xosc_ctune = (reg_val & 0xFF000000) >> 24;
    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1A);
    offset_power_wf0_2g_l = (reg_val & 0x1F0000) >> 16;
    offset_power_wf0_2g_m = (reg_val & 0x1F000000) >> 24;
    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1B);
    offset_power_wf0_2g_h = (reg_val & 0x1F);
    offset_power_wf0_5200 = (reg_val & 0x1F00) >> 8;
    offset_power_wf0_5300 = (reg_val & 0x1F0000) >> 16;
    offset_power_wf0_5530 = (reg_val & 0x1F000000) >> 24;
    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1C);
    offset_power_wf0_5660 = (reg_val & 0x1F);
    offset_power_wf0_5780 = (reg_val & 0x1F00) >> 8;
    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1E);
    offset_power_wf1_2g_l = (reg_val & 0x1F);
    offset_power_wf1_2g_m = (reg_val & 0x1F00) >> 8;
    offset_power_wf1_2g_h = (reg_val & 0x1F0000) >> 16;
    offset_power_wf1_5200 = (reg_val & 0x1F000000) >> 24;
    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1F);
    offset_power_wf1_5300 = (reg_val & 0x1F);
    offset_power_wf1_5530 = (reg_val & 0x1F00) >> 8;
    offset_power_wf1_5660 = (reg_val & 0x1F0000) >> 16;
    offset_power_wf1_5780 = (reg_val & 0x1F000000) >> 24;

    printk("xosc_ctune=0x%02x\n", xosc_ctune);
    printk("offset_power_wf0_2g_l=0x%02x,offset_power_wf0_2g_m=0x%02x,offset_power_wf0_2g_h=0x%02x\n",
           offset_power_wf0_2g_l, offset_power_wf0_2g_m, offset_power_wf0_2g_h);
    printk("offset_power_wf0_5200=0x%02x,offset_power_wf0_5300=0x%02x,offset_power_wf0_5530=0x%02x,offset_power_wf0_5660=0x%02x,offset_power_wf0_5780=0x%02x\n",
           offset_power_wf0_5200, offset_power_wf0_5300, offset_power_wf0_5530,offset_power_wf0_5660, offset_power_wf0_5780);
    printk("offset_power_wf1_2g_l=0x%02x,offset_power_wf1_2g_m=0x%02x,offset_power_wf1_2g_h=0x%02x\n",
           offset_power_wf1_2g_l, offset_power_wf1_2g_m, offset_power_wf1_2g_h);
    printk("offset_power_wf1_5200=0x%02x,offset_power_wf1_5300=0x%02x,offset_power_wf1_5530=0x%02x,offset_power_wf1_5660=0x%02x,offset_power_wf1_5780=0x%02x\n",
           offset_power_wf1_5200, offset_power_wf1_5300, offset_power_wf1_5530,offset_power_wf1_5660, offset_power_wf1_5780);

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&xosc_ctune=0x%02x\n\
        offset_power_wf0_2g_l=0x%02x,offset_power_wf0_2g_m=0x%02x,offset_power_wf0_2g_h=0x%02x\n\
        offset_power_wf0_5200=0x%02x,offset_power_wf0_5300=0x%02x,offset_power_wf0_5530=0x%02x,offset_power_wf0_5660=0x%02x,offset_power_wf0_5780=0x%02x\n\
        offset_power_wf1_2g_l=0x%02x,offset_power_wf1_2g_m=0x%02x,offset_power_wf1_2g_h=0x%02x\n\
        offset_power_wf1_5200=0x%02x,offset_power_wf1_5300=0x%02x,offset_power_wf1_5530=0x%02x,offset_power_wf1_5660=0x%02x,offset_power_wf1_5780=0x%02x\n",
        xosc_ctune,offset_power_wf0_2g_l, offset_power_wf0_2g_m, offset_power_wf0_2g_h,
        offset_power_wf0_5200, offset_power_wf0_5300, offset_power_wf0_5530,offset_power_wf0_5660, offset_power_wf0_5780,
        offset_power_wf1_2g_l, offset_power_wf1_2g_m, offset_power_wf1_2g_h,
        offset_power_wf1_5200, offset_power_wf1_5300, offset_power_wf1_5530,offset_power_wf1_5660, offset_power_wf1_5780);
    wrqu->data.length++;
    return 0;
}
int aml_set_rx_start(struct net_device *dev)
{
    aml_set_reg(dev, 0X60C0600C, 0x00000001);
    aml_set_reg(dev, 0X60C06000, 0x00000117);
    printk("PT Rx Start\n");

    return 0;
}

int aml_set_rx_end(struct net_device *dev,union iwreq_data *wrqu, char *extra)
{
    int fcs_ok = 0;
    int fcs_err = 0;
    int fcs_rx_end = 0;
    int rx_err = 0;

    fcs_ok = aml_get_reg_2(dev, 0x60c06080, wrqu, extra);
    fcs_err = aml_get_reg_2(dev, 0x60c06084, wrqu, extra);
    fcs_rx_end = aml_get_reg_2(dev, 0x60c06088, wrqu, extra);
    rx_err = aml_get_reg_2(dev, 0x60c0608c, wrqu, extra);
    printk("PT Rx result:fcs_ok=%d, fcs_err=%d, fcs_rx_end=%d, rx_err=%d\n", fcs_ok, fcs_err, fcs_rx_end, rx_err);

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "fcs_ok=%d, fcs_err=%d, fcs_rx_end=%d, rx_err=%d\n", fcs_ok, fcs_err, fcs_rx_end, rx_err);
    wrqu->data.length++;
    return 0;
}

int aml_set_rx(struct net_device *dev, int antenna, int channel)
{
    printk("set antenna :%x\n", antenna);
    switch (antenna) {
        case 1:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393917);
                aml_rf_reg_write(dev, 0x80001008, 0x01393914);
                aml_set_reg(dev, 0x60c0b500, 0x00041000);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393913);
                aml_rf_reg_write(dev, 0x80001008, 0x00393911);
            }
            break;
        case 2:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393915);
                aml_rf_reg_write(dev, 0x80001008, 0x01393917);
                aml_set_reg(dev, 0x60c0b500, 0x00041030);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393910);
                aml_rf_reg_write(dev, 0x80001008, 0x00393913);
            }
            break;
        case 3:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393900);
                aml_rf_reg_write(dev, 0x80001008, 0x01393900);
                aml_set_reg(dev, 0x60c0b500, 0x00041000);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393900);
                aml_rf_reg_write(dev, 0x80001008, 0x00393900);
            }
            break;
        default:
            printk("set antenna error :%x\n", antenna);
            break;
    }
    return 0;
}

static unsigned int tx_path = 0;
static unsigned int tx_mode = 0;
static unsigned int tx_bw = 0;
static unsigned int tx_len = 0;
static unsigned int tx_len1 = 0;
static unsigned int tx_len2 = 0;
static unsigned int tx_pwr = 0;
static unsigned int tx_start = 0;
static unsigned int tx_rate = 0;
static unsigned int tx_param = 0;
static unsigned int tx_param1 = 0;

int aml_11b_siso_wf0_tx(struct net_device *dev, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    if (rate == 1) {
        rate = 0x0;
    } else if (rate == 2) {
        rate = 0x1;
    } else if (rate == 5) {
        rate = 0x2;
    } else if (rate == 11) {
        rate = 0x3;
    } else {
        printk("11b_siso_wf0_tx rate error :%x\n", rate);
        return -1;
    }
    if (length1 > 0xff) {
        printk("11b_siso_wf0_tx length1 error :%x\n", length1);
        return -1;
    }
    if (length > 0xf) {
        printk("11b_siso_wf0_tx length error :%x\n", length);
        return -1;
    }
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    aml_set_reg(dev, 0x60805008, 0x00001100);
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000001);
    aml_set_reg(dev, 0x60c0b390, 0x00010101);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    aml_set_reg(dev, 0x60c00800, 0x00000000);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    aml_set_reg(dev, 0x60c06200, 0x00000080);
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06214, 0x00000000 | (rate << 4) | (length & 0x0f));
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000000);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("11b_wf0_tx:rate = %d,length=0x%x, tx_pwr=%d\n", rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_11b_siso_wf1_tx(struct net_device *dev, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    if (rate == 1) {
        rate = 0x0;
    } else if (rate == 2) {
        rate = 0x1;
    } else if (rate == 5) {
        rate = 0x2;
    } else if (rate == 11) {
        rate = 0x3;
    } else {
        printk("11b_siso_wf1_tx rate error :%x\n", rate);
        return -1;
    }
    if (length1 > 0xff) {
        printk("11b_siso_wf0_tx length1 error :%x\n", length1);
        return -1;
    }
    if (length > 0xf) {
        printk("11b_siso_wf0_tx length error :%x\n", length);
        return -1;
    }
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    aml_set_reg(dev, 0x60805008, 0x00001100);
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000002);
    aml_set_reg(dev, 0x60c0b390, 0x00010102);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    aml_set_reg(dev, 0x60c00800, 0x00000000);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    aml_set_reg(dev, 0x60c06200, 0x00000080);
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06214, 0x00000000 | (rate << 4) | (length & 0x0f));
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000000);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("11b_wf1_tx:rate = %d, length=0x%x, tx_pwr=%d\n", rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_11ag_siso_wf0_tx(struct net_device *dev, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    if (rate == 6) {//6M
        rate = 0xb;
    } else if (rate == 9) {//9M
        rate = 0xf;
    } else if (rate == 12) {//12M
        rate = 0xa;
    } else if (rate == 18) {//18M
        rate = 0xe;
    } else if (rate == 24) {//24M
        rate = 0x9;
    } else if (rate == 36) {//36M
        rate = 0xd;
    } else if (rate == 48) {//48M
        rate = 0x8;
    } else if (rate == 54) {//54M
        rate = 0xc;
    } else {
        printk("11ag_siso_wf0_tx rate error :%x\n", rate);
        return -1;
    }
    if (length1 > 0xff) {
        printk("11b_siso_wf0_tx length1 error :%x\n", length1);
        return -1;
    }
    if (length > 0xff) {
        printk("11b_siso_wf0_tx length error :%x\n", length);
        return -1;
    }
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    aml_set_reg(dev, 0x60805008, 0x00001100);
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000001);
    aml_set_reg(dev, 0x60c0b390, 0x00010101);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    aml_set_reg(dev, 0x60c00800, 0x00000000);
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    aml_set_reg(dev, 0x60c06200, 0x00000080);
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06214, 0x00000000 | (rate << 4) | length);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000000);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000010);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("11ag_tx:rate = %d, length=0x%x, tx_pwr=%d\n", rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_11ag_siso_wf1_tx(struct net_device *dev, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    if (rate == 6) {
        rate = 0xb;
    } else if (rate == 9) {
        rate = 0xf;
    } else if (rate == 12) {
        rate = 0xa;
    } else if (rate == 18) {
        rate = 0xe;
    } else if (rate == 24) {
        rate = 0x9;
    } else if (rate == 36) {
        rate = 0xd;
    } else if (rate == 48) {
        rate = 0x8;
    } else if (rate == 54) {
        rate = 0xc;
    } else {
        printk("11ag_siso_wf1_tx rate error :%x\n", rate);
        return -1;
    }
    if (length1 > 0xff) {
        printk("11b_siso_wf0_tx length1 error :%x\n", length1);
        return -1;
    }
    if (length > 0xff) {
        printk("11b_siso_wf0_tx length error :%x\n", length);
        return -1;
    }
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    aml_set_reg(dev, 0x60805008, 0x00001100);
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000002);
    aml_set_reg(dev, 0x60c0b390, 0x00010102);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    aml_set_reg(dev, 0x60c00800, 0x00000000);
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    aml_set_reg(dev, 0x60c06200, 0x00000080);
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06214, 0x00000000 | (rate << 4) | length);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000000);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000010);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("11ag_tx:rate = %d, length=0x%x, tx_pwr=%d\n", rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_ht_siso_wf0_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {//bw 20M
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000001);
    aml_set_reg(dev, 0x60c0b390, 0x00010101);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000002);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000012);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x000000fc);
    aml_set_reg(dev, 0x60c06214, 0x00000001);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06220, 0x00000008);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06220, 0x0000000c);
    }
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c0622c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06230, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_siso_ht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

//set ht 20/40m rate bw tx_pwr length
int aml_ht_mimo_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000003);
    aml_set_reg(dev, 0x00a0b00c, 0x00000003);
    aml_set_reg(dev, 0x60c0b390, 0x00010103);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003000);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00484848);
    aml_set_reg(dev, 0x60c0b104, 0x00484848);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000002);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000012);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000001);
    aml_set_reg(dev, 0x60c06210, 0x000000fc);
    aml_set_reg(dev, 0x60c06214, 0x00000001);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x0000000c);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000080 | (rate + 8));
    aml_set_reg(dev, 0x60c0622c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06230, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_mimo_ht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_ht_siso_wf1_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000002);
    aml_set_reg(dev, 0x60c0b390, 0x00010102);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if(bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000002);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000012);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x000000fc);
    aml_set_reg(dev, 0x60c06214, 0x00000001);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06220, 0x00000008);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06220, 0x0000000c);
    }
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c0622c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06230, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080);
    aml_set_reg(dev, 0x60c0623c, 0x00000000);
    aml_set_reg(dev, 0x60c06240, 0x00000000);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_wf1_ht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_vht_siso_wf0_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000001);
    aml_set_reg(dev, 0x60c0b390, 0x00010101);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000004);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000014);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000024);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000);
    aml_set_reg(dev, 0x60c06214, 0x00000000);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x0000000c);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000000);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c06238, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_siso_vht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_vht_mimo_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000003);
    aml_set_reg(dev, 0x00a0b00c, 0x00000003);
    aml_set_reg(dev, 0x60c0b390, 0x00010103);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003000);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0b100, 0x00484848);
    aml_set_reg(dev, 0x60c0b104, 0x00484848);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x0000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000004);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000014);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000024);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000001);
    aml_set_reg(dev, 0x60c06210, 0x00000000);
    aml_set_reg(dev, 0x60c06214, 0x00000000);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000008);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000000);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000090 | rate);
    aml_set_reg(dev, 0x60c06238, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_mimo_vht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_vht_siso_wf1_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000002);
    aml_set_reg(dev, 0x60c0b390, 0x00010102);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    aml_set_reg(dev, 0x60c06004, 0x00020000);
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x0000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000004);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000014);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000024);
    }
    aml_set_reg(dev, 0x60c06204, 0x00000001);
    aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
    aml_set_reg(dev, 0x60c0620c, 0x00000000);
    aml_set_reg(dev, 0x60c06210, 0x00000000);
    aml_set_reg(dev, 0x60c06214, 0x00000000);
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000008);
    aml_set_reg(dev, 0x60c06224, 0x00000000);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000000);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c06238, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_wf1_vht_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_hesu_siso_wf0_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if(bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if(bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000001);
    aml_set_reg(dev, 0x60c0b390, 0x00010101);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    if (bw == 0x0 || bw == 0x1) {
        aml_set_reg(dev, 0x60c06004, 0x00020000);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06004, 0x00010000);
    }
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000005);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000000);
        aml_set_reg(dev, 0x60c06210, 0x00000000);
        aml_set_reg(dev, 0x60c06214, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000015);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000000);
        aml_set_reg(dev, 0x60c06210, 0x000000fc);
        aml_set_reg(dev, 0x60c06214, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000025);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000020);
        aml_set_reg(dev, 0x60c06210, 0x000000fc);
        aml_set_reg(dev, 0x60c06214, 0x00000001);
    }
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000008);
    aml_set_reg(dev, 0x60c06224, 0x00000012);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06244, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_siso_hesu_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_hesu_mimo_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000003);
    aml_set_reg(dev, 0x00a0b00c, 0x00000003);
    aml_set_reg(dev, 0x60c0b390, 0x00010103);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003000);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00484848);
    aml_set_reg(dev, 0x60c0b104, 0x00484848);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    if (bw == 0x0 || bw == 0x1) {
        aml_set_reg(dev, 0x60c06004, 0x00020000);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06004, 0x00010000);
    }
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000005);
        aml_set_reg(dev, 0x60c06204, 0x00000003);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000001);
        aml_set_reg(dev, 0x60c06210, 0x00000067);
        aml_set_reg(dev, 0x60c06214, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000015);
        aml_set_reg(dev, 0x60c06204, 0x00000003);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000001);
        aml_set_reg(dev, 0x60c06210, 0x000000fc);
        aml_set_reg(dev, 0x60c06214, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000025);
        aml_set_reg(dev, 0x60c06204, 0x00000003);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000001);
        aml_set_reg(dev, 0x60c06210, 0x00000000);
        aml_set_reg(dev, 0x60c06214, 0x00000000);
    }
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000008);
    aml_set_reg(dev, 0x60c06224, 0x00000012);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000090 | rate);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06244, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_mimo_hesu_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n", bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_hesu_siso_wf1_tx(struct net_device *dev, U8 bw, U8 rate, U8 tx_pwr, U8 length, U8 length1, U8 length2)
{
    aml_set_reg(dev, 0x00a0d084, 0x00050001);
    aml_set_reg(dev, 0x00a0d090, 0x4f5300f3);
    aml_set_reg(dev, 0x60805010, 0xb9d70242);
    aml_set_reg(dev, 0x60805014, 0x0000013f);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60805008, 0x00001100);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60805008, 0x00001150);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60805008, 0x000011a0);
    }
    aml_set_reg(dev, 0x60c0b004, 0x00000001);
    aml_set_reg(dev, 0x00a0b00c, 0x00000002);
    aml_set_reg(dev, 0x60c0b390, 0x00010102);
    aml_set_reg(dev, 0x00a0b1b8, 0xc0003100);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c00800, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c00800, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c00800, 0x00000002);
    }
    aml_set_reg(dev, 0x60c0088c, 0x00005050);
    aml_set_reg(dev, 0x60c0b100, 0x00303030);
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    if (bw == 0x0 || bw == 0x1) {
        aml_set_reg(dev, 0x60c06004, 0x00020000);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06004, 0x00010000);
    }
    aml_set_reg(dev, 0x60c06008, 0x00000012);
    aml_set_reg(dev, 0x60c0600c, 0x00000001);
    aml_set_reg(dev, 0x60c06048, 0x00000780);
    if (bw == 0x0) {
        aml_set_reg(dev, 0x60c06200, 0x00000005);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000000);
        aml_set_reg(dev, 0x60c06210, 0x00000000);
        aml_set_reg(dev, 0x60c06214, 0x00000000);
    } else if (bw == 0x1) {
        aml_set_reg(dev, 0x60c06200, 0x00000015);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000000);
        aml_set_reg(dev, 0x60c06210, 0x000000fc);
        aml_set_reg(dev, 0x60c06214, 0x00000001);
    } else if (bw == 0x2) {
        aml_set_reg(dev, 0x60c06200, 0x00000025);
        aml_set_reg(dev, 0x60c06204, 0x00000001);
        aml_set_reg(dev, 0x60c06208, 0x00000000 | tx_pwr);
        aml_set_reg(dev, 0x60c0620c, 0x00000020);
        aml_set_reg(dev, 0x60c06210, 0x000000fc);
        aml_set_reg(dev, 0x60c06214, 0x00000001);
    }
    aml_set_reg(dev, 0x60c06218, 0x00000000);
    aml_set_reg(dev, 0x60c0621c, 0x00000000);
    aml_set_reg(dev, 0x60c06220, 0x00000008);
    aml_set_reg(dev, 0x60c06224, 0x00000012);
    aml_set_reg(dev, 0x60c06228, 0x00000000);
    aml_set_reg(dev, 0x60c0622c, 0x00000001);
    aml_set_reg(dev, 0x60c06230, 0x00000000);
    aml_set_reg(dev, 0x60c06234, 0x00000000);
    aml_set_reg(dev, 0x60c06238, 0x00000080 | rate);
    aml_set_reg(dev, 0x60c0623c, 0x00000000 | length1);
    aml_set_reg(dev, 0x60c06240, 0x00000000 | length);
    aml_set_reg(dev, 0x60c06244, 0x00000000 | length2);
    aml_set_reg(dev, 0x60c06010, 0x00000000);
    aml_set_reg(dev, 0x60c06000, 0x00000317);
    printk("2g_5g_wf1_hesu_tx:bw=%d, rate = %d, length=0x%x, tx_pwr=%d\n",bw, rate, (tx_param1 >> 8), tx_pwr);
    return 0;
}

int aml_set_tx_prot(struct net_device *dev, int tx_pam, int tx_pam1)
{
    int model = (tx_pam & 0x00f00000) >> 20; //0x1 wf0 0x2 wf1 0x3 mimo
    int prot = (tx_pam & 0x000f0000) >> 16; //11b/11ag/ht/vht/hesu
    U8 bw = (tx_pam & 0x0000ff00) >> 8;
    U8 rate = tx_pam & 0x000000ff;
    U8 length2 = (tx_pam1 & 0x0f000000) >> 24;
    U8 length = (tx_pam1 & 0x00ff0000) >> 16;
    U8 length1 = (tx_pam1 & 0x0000ff00) >> 8;
    U8 tx_pwr = tx_pam1 & 0x000000ff;

    if (prot == 0x0 && model == 0x1) {
        aml_11b_siso_wf0_tx(dev, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x0 && model == 0x2) {
        aml_11b_siso_wf1_tx(dev, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x1 && model == 0x1) {
        aml_11ag_siso_wf0_tx(dev, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x1 && model == 0x2) {
        aml_11ag_siso_wf1_tx(dev, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x2 && model == 0x1) {
        aml_ht_siso_wf0_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x2 && model == 0x2) {
        aml_ht_siso_wf1_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x2 && model == 0x3) {
        aml_ht_mimo_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x3 && model == 0x1) {
        aml_vht_siso_wf0_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x3 && model == 0x2) {
        aml_vht_siso_wf1_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x3 && model == 0x3) {
        aml_vht_mimo_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x4 && model == 0x1) {
        aml_hesu_siso_wf0_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x4 && model == 0x2) {
        aml_hesu_siso_wf1_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else if (prot == 0x4 && model == 0x3) {
        aml_hesu_mimo_tx(dev,bw, rate, tx_pwr, length, length1, length2);
    } else {
        printk("tx param error\n");
    }
    return 0;
}

int aml_set_tx_end(struct net_device *dev)
{
    aml_set_reg(dev, 0x60c06000, 0x00000000);
    tx_start = 0;
    printk("set_tx_end\n");
    return 0;
}

int aml_set_tx_start(struct net_device *dev)
{
    tx_start = 1;
    aml_set_tx_prot(dev, tx_param, tx_param1);
    printk("set_tx_start\n");
    return 0;
}

int aml_recy_ctrl(struct net_device *dev, int recy_id)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
#ifdef CONFIG_AML_RECOVERY
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_cmd_mgr *cmd_mgr = &aml_hw->cmd_mgr;
#endif

    switch (recy_id) {
#ifdef CONFIG_AML_RECOVERY
        case 0:
            AML_INFO("disable recovery detection");
            aml_recy_disable();
            break;
        case 1:
            AML_INFO("enable recovery detection");
            aml_recy_enable();
            break;
        case 2:
            AML_INFO("do simulate cmd queue crashed");
            spin_lock_bh(&cmd_mgr->lock);
            cmd_mgr->state = AML_CMD_MGR_STATE_CRASHED;
            spin_unlock_bh(&cmd_mgr->lock);
            break;
        case 3:
            AML_INFO("do recovery straightforward");
            aml_recy_doit(aml_hw);
            break;
#endif
        case 4:
            AML_INFO("do firmware soft reset");
            aml_fw_reset(aml_vif);
            break;

        default:
            AML_INFO("unknow recovery operation");
            break;
    }
    return 0;
}

int aml_set_tx_path(struct net_device *dev, int path, int channel)
{
    tx_path = path;//mode:wf0 0x1 wf1 0x2 mimo 0x3

    if (tx_path > 0x3) {
        printk("set_tx_path error:%d\n",tx_path);
        return -1;
    }
    tx_path = (tx_path & 0x0000000f) << 20;
    tx_param = tx_param & 0xff0fffff;
    tx_param = tx_param | tx_path;
    printk("aml_set_tx_path:%d\n", path);
    switch (path) {
        case 1:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393916);
                aml_rf_reg_write(dev, 0x80001008, 0x01393914);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393912);
                aml_rf_reg_write(dev, 0x80001008, 0x00393911);
            }
            break;
        case 2:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393915);
                aml_rf_reg_write(dev, 0x80001008, 0x01393916);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393910);
                aml_rf_reg_write(dev, 0x80001008, 0x00393912);
            }
            break;
        case 3:
            if (channel <= 14) {
                aml_rf_reg_write(dev, 0x80000008, 0x01393900);
                aml_rf_reg_write(dev, 0x80001008, 0x01393900);
                aml_set_reg(dev, 0x60c0b500, 0x00041000);
            } else {
                aml_rf_reg_write(dev, 0x80000008, 0x00393900);
                aml_rf_reg_write(dev, 0x80001008, 0x00393900);
            }
            break;
        default:
            printk("set antenna error :%x\n", path);
            break;
    }
    return 0;
}

int aml_set_tx_mode(struct net_device *dev, int mode)
{
    tx_mode = mode;

    if (tx_mode > 0x4) {
        printk("set_tx_mode error:%d\n", tx_mode);
        return -1;
    }
    tx_mode = (tx_mode & 0x0000000f) << 16;
    tx_param = tx_param & 0xfff0ffff;
    tx_param = tx_param | tx_mode;
    printk("set_tx_mode:%d\n", mode);
    return 0;
}

int aml_set_tx_bw(struct net_device *dev, int bw)
{
    tx_bw = bw;

    tx_bw = (tx_bw & 0x000000ff) << 8;
    tx_param = tx_param & 0xffff00ff;
    tx_param = tx_param | tx_bw;
    printk("set_tx_bw:%d\n", bw);
    return 0;
}

int aml_set_tx_rate(struct net_device *dev, int rate)
{
    tx_rate = rate;
    tx_rate = tx_rate & 0x000000ff;
    tx_param = tx_param & 0xffffff00;
    tx_param = tx_param | tx_rate;
    printk("set_tx_rate:%d\n", rate);
    return 0;
}

int aml_set_tx_len(struct net_device *dev, int len)
{
    if (len > 0xfffff) {
        printk("set_tx_len error:%d\n", len);
        return -1;
    }
    tx_len= (len & 0x000000ff) << 8;
    tx_len1 = (len & 0x0000ff00) << 8;
    tx_len2 = (len & 0x000f0000) << 8;
    tx_param1 = tx_param1 & 0xf00000ff;
    tx_param1 = tx_param1 | tx_len | tx_len1 | tx_len2;
    printk("aml_set_tx_len:0x%x\n", len);
    return 0;
}

int aml_set_tx_pwr(struct net_device *dev, int pwr)
{
    tx_pwr = pwr;

    if (tx_pwr > 0xff) {
        printk("set_tx_pwr error :%d\n", tx_pwr);
        return -1;
    }
    tx_pwr = tx_pwr & 0x000000ff;
    tx_param1 = tx_param1 & 0xffffff00;
    tx_param1 = tx_param1 | tx_pwr;
    printk("set_tx_pwr:%d\n", pwr);
    return 0;
}

int aml_set_olpc_pwr(struct net_device *dev,int tx_param1)
{
    int tx_pwr = tx_param1 & 0x000000ff;
    aml_set_reg(dev, MACBYP_TXV_ADDR + MACBYP_TXV_08, tx_pwr); //bit[7:0] : txv1_txpwr_level_idx
    printk("aml_set_olpc_pwr tx_pwr=0x%x\n", (tx_param1 & 0x000000ff));
    return 0;
}

int aml_set_xosc_ctune(struct net_device *dev,union iwreq_data *wrqu, char *extra, int xosc_param)
{
    unsigned int xosc = aml_get_reg_2(dev, XOSC_CTUNE_BASE, wrqu, extra);
    xosc = xosc & 0xfffff00f;
    if (xosc_param > 0x3f) {
        printk("aml_xosc_ctune error=0x%x\n", xosc_param);
        return -1;
    }

    xosc_param = (xosc_param & 0x000000ff) << 4;
    xosc = xosc | xosc_param;
    aml_set_reg(dev, XOSC_CTUNE_BASE, xosc);

    printk("aml_xosc_ctune=0x%x\n", (xosc & 0x00000ff0) >> 4);
    return 0;
}

int aml_set_power_offset(struct net_device *dev,union iwreq_data *wrqu, char *extra, int pwr_offset)
{
    unsigned int offset = pwr_offset & 0x0000001f;
    unsigned int reference_pw;
    unsigned int ret;
    if ((pwr_offset & 0xffffefff) > 0x1f) {//bit[12]:0 wf0;1 wf1
        printk("aml_set_power_offset error=0x%x\n", pwr_offset);
        return -1;
    }

    if (offset > 0xf) {
        reference_pw = (offset - 32) * 16 + 1024;
    } else {
        reference_pw = offset * 16;
    }

    if ((pwr_offset & 0x1000) == 0) {//wf0
        ret = aml_get_reg_2(dev, POWER_OFFSET_BASE_WF0, wrqu, extra);
        ret = ret & 0xfffffc00;
        reference_pw = reference_pw & 0x000003ff;
        ret = ret | reference_pw;
        aml_set_reg(dev, POWER_OFFSET_BASE_WF0, ret);
    } else {//wf1
        ret = aml_get_reg_2(dev, POWER_OFFSET_BASE_WF1, wrqu, extra);
        ret = ret & 0xfffffc00;
        reference_pw = reference_pw & 0x000003ff;
        ret = ret | reference_pw;
        aml_set_reg(dev, POWER_OFFSET_BASE_WF1, ret);
    }

    printk("aml_set_power_offset=0x%x\n", reference_pw);
    return 0;
}

int aml_set_ram_efuse(struct net_device *dev , unsigned int ram_efuse)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    unsigned int reg_val = 0;
    if ((ram_efuse & 0x0fffffff) > 0x1f) {
        printk("aml_set_ram_efuse error=0x%x\n", ram_efuse);
        return -1;
    }

    ram_efuse = ram_efuse & 0xf000001f;

    if (ram_efuse >> 28 == 0x0) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1A);
        reg_val = reg_val & 0xffe0ffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 16);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1A, reg_val);
    } else if (ram_efuse >> 28 == 0x1) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1A);
        reg_val = reg_val & 0xe0ffffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 24);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1A, reg_val);
    } else if (ram_efuse >> 28 == 0x2) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1B);
        reg_val = reg_val & 0xffffffe0;
        reg_val = reg_val | (ram_efuse & 0x0000001f);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1B, reg_val);
    } else if (ram_efuse >> 28 == 0x3) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1B);
        reg_val = reg_val & 0xffffe0ff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 8);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1B, reg_val);
    } else if (ram_efuse >> 28 == 0x4) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1B);
        reg_val = reg_val & 0xffe0ffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 16);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1B, reg_val);
    } else if (ram_efuse >> 28 == 0x5) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1B);
        reg_val = reg_val & 0xe0ffffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 24);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1B, reg_val);
    } else if (ram_efuse >> 28 == 0x6) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1C);
        reg_val = reg_val & 0xffffffe0;
        reg_val = reg_val | (ram_efuse & 0x0000001f);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1C, reg_val);
    } else if (ram_efuse >> 28 == 0x7) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1C);
        reg_val = reg_val & 0xffffe0ff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 8);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1C, reg_val);
    } else if (ram_efuse >> 28 == 0x8) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1E);
        reg_val = reg_val & 0xffffffe0;
        reg_val = reg_val | (ram_efuse & 0x0000001f);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1E, reg_val);
    } else if (ram_efuse >> 28 == 0x9) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1E);
        reg_val = reg_val & 0xffffe0ff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 8);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1E, reg_val);
    } else if (ram_efuse >> 28 == 0xa) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1E);
        reg_val = reg_val & 0xffe0ffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 16);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1E, reg_val);
    } else if (ram_efuse >> 28 == 0xb) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1E);
        reg_val = reg_val & 0xe0ffffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 24);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1E, reg_val);
    } else if (ram_efuse >> 28 == 0xc) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1F);
        reg_val = reg_val & 0xffffffe0;
        reg_val = reg_val | (ram_efuse & 0x0000001f);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1F, reg_val);
    }  else if (ram_efuse >> 28 == 0xd) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1F);
        reg_val = reg_val & 0xffffe0ff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 8);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1F, reg_val);
    } else if ((ram_efuse >> 28) == 0xe) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1F);
        reg_val = reg_val & 0xffe0ffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 16);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1F, reg_val);
    } else if (ram_efuse >> 28 == 0xf) {
        reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_1F);
        reg_val = reg_val & 0xe0ffffff;
        reg_val = reg_val | ((ram_efuse & 0x0000001f) << 24);
        _aml_set_efuse(aml_vif, EFUSE_BASE_1F, reg_val);
    }

    return 0;
}

int aml_set_xosc_efuse(struct net_device *dev , int xosc_efuse)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    unsigned int reg_val = 0;

    if (xosc_efuse > 0xff) {
        printk("aml_set_xosc_efuse error=0x%x\n", xosc_efuse);
        return -1;
    }

    reg_val = _aml_get_efuse(aml_vif, EFUSE_BASE_05);
    reg_val = reg_val & 0x00ffffff;
    reg_val = reg_val | (xosc_efuse<<24);
    _aml_set_efuse(aml_vif, EFUSE_BASE_05, reg_val);
    printk("aml_set_xosc_efuse=0x%x\n", xosc_efuse);
    return 0;
}


static int aml_pcie_lp_switch(struct net_device *dev, int status)
{
    int ret = 0;
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct aml_plat * aml_plat = aml_hw->plat;

    if (PS_D3_STATUS == status)
    {
        pci_save_state(aml_plat->pci_dev);
        pci_enable_wake(aml_plat->pci_dev, PCI_D0, 1);
        printk("--------------D3---------------\n");
        printk("pci->dev_flags = 0x%x state 0x%x d3_delay 0x%x\n",aml_plat->pci_dev->dev_flags, aml_plat->pci_dev->current_state, aml_plat->pci_dev->D3HOT_DELAY);
        ret = pci_set_power_state(aml_plat->pci_dev, PCI_D3hot);
    }
    else if (PS_D0_STATUS == status)
    {
        printk("%s %d pci_dev->current_state 0x%x\n", __func__,__LINE__,aml_plat->pci_dev->current_state);
        printk("--------------D0---------------\n");
        ret = pci_set_power_state(aml_plat->pci_dev, PCI_D0);
    }
    else
    {
        printk("%s %d: set param err\n", __func__, __LINE__);
        return 0;
    }
    if (ret) {
        ERROR_DEBUG_OUT("pci_set_power_state error %d\n", ret);
    }
    return ret;
}

int aml_set_fwlog_cmd(struct net_device *dev, int mode, int size)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    int ret = 0;

    if (aml_bus_type != PCIE_MODE) {
        if (mode == 0) {
            ret = aml_traceind(aml_vif->aml_hw->ipc_env->pthis, mode);
            if (ret < 0)
                return -1;
        }

        ret = aml_log_file_info_init(mode, size);
        if (ret < 0) {
            printk("aml_log_file_info_init fail\n");
            return -1;
        }

        aml_send_fwlog_cmd(aml_vif, mode);
    }
    return 0;
}

// Returns a char * arr [] and size is the length of the returned array
char **aml_cmd_char_phrase(char sep, const char *str, int *size)
{
    int count = 0;
    int i;
    char **ret;
    int lastindex = -1;
    int j = 0;

    for (i = 0; i < strlen(str); i++) {
        if (str[i] == sep) {
            count++;
        }
    }

    ret = (char **)kzalloc((++count) * sizeof(char *), GFP_KERNEL);

    for (i = 0; i < strlen(str); i++) {
        if (str[i] == sep) {
            // kzalloc the memory space of substring length + 1
            ret[j] = (char *)kzalloc((i - lastindex) * sizeof(char), GFP_KERNEL);
            memcpy(ret[j], str + lastindex + 1, i - lastindex - 1);
            j++;
            lastindex = i;
        }
    }
    // Processing the last substring
    if (lastindex <= strlen(str) - 1) {
        ret[j] = (char *)kzalloc((strlen(str) - lastindex) * sizeof(char), GFP_KERNEL);
        memcpy(ret[j], str + lastindex + 1, strlen(str) - 1 - lastindex);
        j++;
    }

    *size = j;

    return ret;
}

void aml_set_wifi_mac_addr(struct net_device *dev, char* arg_iw)
{
    char **mac_cmd;
    int cmd_arg;
    char sep = ':';
    unsigned int efuse_data_l = 0;
    unsigned int efuse_data_h = 0;
    struct aml_vif *aml_vif = netdev_priv(dev);

    mac_cmd = aml_cmd_char_phrase(sep, arg_iw, &cmd_arg);
    if (mac_cmd) {
        efuse_data_l = (simple_strtoul(mac_cmd[2], NULL,16) << 24) | (simple_strtoul(mac_cmd[3], NULL,16) << 16)
                       | (simple_strtoul(mac_cmd[4], NULL,16) << 8) | simple_strtoul(mac_cmd[5], NULL,16);
        efuse_data_h = (simple_strtoul(mac_cmd[0], NULL,16) << 8) | (simple_strtoul(mac_cmd[1], NULL,16));

        _aml_set_efuse(aml_vif, EFUSE_BASE_01, efuse_data_l);
        efuse_data_h = (efuse_data_h & 0xffff);
        _aml_set_efuse(aml_vif, EFUSE_BASE_02, efuse_data_h);

        printk("iwpriv write WIFI MAC addr is:  %02x:%02x:%02x:%02x:%02x:%02x\n",
                (efuse_data_h & 0xff00) >> 8, efuse_data_h & 0x00ff, (efuse_data_l & 0xff000000) >> 24,
                (efuse_data_l & 0x00ff0000) >> 16, (efuse_data_l & 0xff00) >> 8, efuse_data_l & 0xff);
    }
    kfree(mac_cmd);
}

int aml_get_mac_addr(struct net_device *dev,union iwreq_data *wrqu, char *extra)
{
    unsigned int efuse_data_l = 0;
    unsigned int efuse_data_h = 0;
    struct aml_vif *aml_vif = netdev_priv(dev);

    efuse_data_l = _aml_get_efuse(aml_vif, EFUSE_BASE_01);
    efuse_data_h = _aml_get_efuse(aml_vif, EFUSE_BASE_02);
    if (efuse_data_l != 0 && efuse_data_h != 0) {
        printk("efuse addr:%08x,%08x, MAC addr is: %02x:%02x:%02x:%02x:%02x:%02x\n", EFUSE_BASE_01, EFUSE_BASE_02,
            (efuse_data_h & 0xff00) >> 8,efuse_data_h & 0x00ff, (efuse_data_l & 0xff000000) >> 24,
            (efuse_data_l & 0x00ff0000) >> 16,(efuse_data_l & 0xff00) >> 8,efuse_data_l & 0xff);

        wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, " %02x:%02x:%02x:%02x:%02x:%02x\n",
            (efuse_data_h & 0xff00) >> 8,efuse_data_h & 0x00ff, (efuse_data_l & 0xff000000) >> 24,
            (efuse_data_l & 0x00ff0000) >> 16,(efuse_data_l & 0xff00) >> 8,efuse_data_l & 0xff);
        wrqu->data.length++;
    } else {
        printk("No mac address is written into efuse!");
    }
    return 0;
}

void aml_set_bt_mac_addr(struct net_device *dev, char* arg_iw)
{
    char **mac_cmd;
    int cmd_arg;
    char sep = ':';
    unsigned int efuse_data_l = 0;
    unsigned int efuse_data_h = 0;
    struct aml_vif *aml_vif = netdev_priv(dev);

    mac_cmd = aml_cmd_char_phrase(sep, arg_iw, &cmd_arg);
    if (mac_cmd) {
        efuse_data_h = (simple_strtoul(mac_cmd[0], NULL,16) << 24) | (simple_strtoul(mac_cmd[1], NULL,16) << 16)
                       | (simple_strtoul(mac_cmd[2], NULL,16) << 8) | simple_strtoul(mac_cmd[3], NULL,16);
        efuse_data_l = (simple_strtoul(mac_cmd[4], NULL,16) << 24) | (simple_strtoul(mac_cmd[5], NULL,16) << 16);

        _aml_set_efuse(aml_vif, EFUSE_BASE_03, efuse_data_h);
        efuse_data_l = (efuse_data_l & 0xffff0000);
        _aml_set_efuse(aml_vif, EFUSE_BASE_02, efuse_data_l);

        printk("iwpriv write BT MAC addr is:  %02x:%02x:%02x:%02x:%02x:%02x\n",
            (efuse_data_h & 0xff000000) >> 24,(efuse_data_h & 0x00ff0000) >> 16,
            (efuse_data_h & 0xff00) >> 8, efuse_data_h & 0xff,
            (efuse_data_l & 0xff000000) >> 24, (efuse_data_l & 0x00ff0000) >> 16);
    }
    kfree(mac_cmd);
}

int aml_get_bt_mac_addr(struct net_device *dev,union iwreq_data *wrqu, char *extra)
{
    unsigned int efuse_data_l = 0;
    unsigned int efuse_data_h = 0;
    struct aml_vif *aml_vif = netdev_priv(dev);

    efuse_data_l = _aml_get_efuse(aml_vif, EFUSE_BASE_02);
    efuse_data_h = _aml_get_efuse(aml_vif, EFUSE_BASE_03);
    if (efuse_data_l != 0 && efuse_data_h != 0) {
        printk("BT MAC addr is: %02x:%02x:%02x:%02x:%02x:%02x\n",
            (efuse_data_h & 0xff000000) >> 24,(efuse_data_h & 0x00ff0000) >> 16,
            (efuse_data_h & 0xff00) >> 8, efuse_data_h & 0xff,
            (efuse_data_l & 0xff000000) >> 24, (efuse_data_l & 0x00ff0000) >> 16);

        wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, " %02x:%02x:%02x:%02x:%02x:%02x\n",
            (efuse_data_h & 0xff000000) >> 24,(efuse_data_h & 0x00ff0000) >> 16,
            (efuse_data_h & 0xff00) >> 8, efuse_data_h & 0xff,
            (efuse_data_l & 0xff000000) >> 24, (efuse_data_l & 0x00ff0000) >> 16);
        wrqu->data.length++;
    } else {
        printk("No bt mac address is written into efuse!");
    }
    return 0;
}

#ifdef SDIO_SPEED_DEBUG
#define TEST_SDIO_ONLY
//#define SDIO_NORMAL_TX
#define SDIO_TX
//#define SDIO_SCATTER
#define DATA_LEN (1024*128)
static unsigned long payload_total = 0;
static unsigned char start_flag = 0;
static unsigned long in_time;
int g_test_times = 3;

void aml_datarate_monitor(void)
{
    static unsigned int sdio_speed = 0;

    if (time_after(jiffies, in_time + HZ)) {
        sdio_speed = payload_total >> 17;
        sdio_speed = (sdio_speed * HZ) / (jiffies - in_time);
        start_flag = 0;
        payload_total = 0;
        g_test_times--;
        printk(">>>sdio_speed :%d mbps, time:%d\n", sdio_speed, (jiffies - in_time));
    }
}

int aml_sdio_max_speed_test_cmd(struct net_device *dev, int enable)
{
    struct aml_vif *aml_vif = netdev_priv(dev);
    struct aml_hw *aml_hw = aml_vif->aml_hw;
    struct amlw_hif_scatter_req * scat_req = NULL;
    unsigned char *data = kmalloc(DATA_LEN, GFP_ATOMIC);
    if (!data) {
        ASSERT_ERR(0);
        return -1;
    }
    memset(data, 0x66, DATA_LEN);

    scat_req = aml_hw->plat->hif_sdio_ops->hi_get_scatreq(&g_hwif_sdio);
    if (scat_req != NULL) {
        scat_req->req = HIF_WRITE | HIF_ASYNCHRONOUS;
        scat_req->addr = 0x0;
    }

    g_test_times = 3;
    while (enable && g_test_times) {
        if (!start_flag) {
            start_flag = 1;
            in_time = jiffies;
        }
#ifndef TEST_SDIO_ONLY
#ifdef SDIO_NORMAL_TX
        unsigned int i = 0;
        for (i = 0; i < DATA_LEN / 10240; ++i) {
            scat_req->scat_list[i].packet = data + 10240 * i;
            scat_req->scat_list[i].page_num = 10;
            scat_req->scat_list[i].len = 10240;
            scat_req->scat_count++;
            scat_req->len += scat_req->scat_list[i].len;
        }

        //printk("count:%d, len:%d\n", scat_req->scat_count, scat_req->len);
        if (DATA_LEN % 10240) {
            scat_req->scat_list[i].packet = data + 10240 * i;
            scat_req->scat_list[i].page_num = ((DATA_LEN % 10240) % 1024) > 0 ? (DATA_LEN % 10240) / 1024 + 1: (DATA_LEN % 10240) / 1024;
            scat_req->scat_list[i].len = (DATA_LEN % 10240);
            scat_req->scat_count++;
            scat_req->len += scat_req->scat_list[i].len;
        }
        //printk("count:%d, len:%d\n", scat_req->scat_count, scat_req->len);
        aml_hw->plat->hif_sdio_ops->hi_send_frame(scat_req);

        //ack irq
        aml_hw->plat->hif_sdio_ops->hi_tx_buffer_read((unsigned char *)data, (unsigned char *)0x60038000, 8);
        //read cfm
        aml_hw->plat->hif_sdio_ops->hi_tx_buffer_read((unsigned char *)data, (unsigned char *)0x60038000, 1028);
#else
        //rx
        aml_hw->plat->hif_sdio_ops->hi_rx_buffer_read((unsigned char *)aml_hw->host_buf,
            (unsigned char *)(unsigned long)RXBUF_START_ADDR, DATA_LEN, 0);
        aml_hw->plat->hif_sdio_ops->hi_random_word_write((unsigned int)(SYS_TYPE)(0x60038000), (unsigned int)1);
        //ack irq
        aml_hw->plat->hif_sdio_ops->hi_tx_buffer_read((unsigned char *)data, (unsigned char *)0x60038000, 8);
#endif

#else
#ifdef SDIO_TX
#ifndef SDIO_SCATTER
        //need delete base addr set procedure
        aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)data, (unsigned char *)0x60038000, DATA_LEN);
        //aml_hw->plat->hif_sdio_ops->hi_random_word_write(0x00a070a0, 0x5678);// for revb irq trigger

#else
        unsigned int i = 0;
        for (i = 0; i < DATA_LEN / 10240; ++i) {
            scat_req->scat_list[i].packet = data + 10240 * i;
            scat_req->scat_list[i].page_num = 10;
            scat_req->scat_list[i].len = 10240;
            scat_req->scat_count++;
            scat_req->len += scat_req->scat_list[i].len;
        }

        if (DATA_LEN % 10240) {
            scat_req->scat_list[i].packet = data + 10240 * i;
            scat_req->scat_list[i].page_num = ((DATA_LEN % 10240) % 1024) > 0 ? (DATA_LEN % 10240) / 1024 + 1: (DATA_LEN % 10240) / 1024;
            scat_req->scat_list[i].len = (DATA_LEN % 10240);
            scat_req->scat_count++;
            scat_req->len += scat_req->scat_list[i].len;
        }
        aml_hw->plat->hif_sdio_ops->hi_send_frame(scat_req);
        //aml_hw->plat->hif_sdio_ops->hi_random_word_write(0x00a070a0, 0x5678);

#endif
#else
        aml_hw->plat->hif_sdio_ops->hi_tx_buffer_read((unsigned char *)data, (unsigned char *)0x60038000, DATA_LEN);
#endif
#endif
        payload_total += DATA_LEN;
        aml_datarate_monitor();
    }

    if (data) {
        FREE(data, "test data.");
        data = NULL;
    }
    return 0;
}
#endif

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
            aml_set_legacy_rate(dev, set1, 0);
            break;
        case AML_IWP_SET_SCAN_HANG:
            aml_set_scan_hang(dev, set1);
            break;
        case AML_IWP_SET_SCAN_TIME:
            aml_set_scan_time(dev, set1);
            break;
        case AML_IWP_SET_PS_MODE:
            aml_set_ps_mode(dev, set1);
            break;
        case AML_IWP_SET_AMSDU_MAX:
             aml_set_amsdu_max(dev, set1);
             break;
        case AML_IWP_SET_AMSDU_TX:
             aml_set_amsdu_tx(dev, set1);
             break;
        case AML_IWP_SET_LDPC:
             aml_set_ldpc(dev, set1);
             break;
        case AML_IWP_SET_P2P_OPPPS:
             aml_set_p2p_oppps(dev, set1);
             break;
        case AML_IWP_SET_TX_LFT:
             aml_set_tx_lft(dev, set1);
             break;
        case AML_IWP_SET_STBC:
            aml_set_stbc(dev, set1);
            break;
        case AML_IWP_SET_PT_CALIBRATION:
            aml_set_pt_calibration(dev, set1);
            break;
        case AML_PCIE_STATUS:
            aml_pcie_lp_switch(dev, set1);
            break;
        case AML_IWP_ENABLE_WF:
            aml_enable_wf(dev, set1);
            break;
#ifdef SDIO_SPEED_DEBUG
        case AML_IWP_ENABLE_SDIO_CAL_SPEED:
            aml_sdio_max_speed_test_cmd(dev, set1);
            break;
#endif
        case AML_IWP_SEND_TWT_TEARDOWN:
            aml_send_twt_teardown(dev, set1);
            break;
        case AML_IWP_SET_TX_MODE:
            aml_set_tx_mode(dev,set1);
            break;
        case AML_IWP_SET_TX_BW:
            aml_set_tx_bw(dev,set1);
            break;
        case AML_IWP_SET_TX_RATE:
            aml_set_tx_rate(dev,set1);
            break;
        case AML_IWP_SET_TX_LEN:
            aml_set_tx_len(dev,set1);
            break;
        case AML_IWP_SET_TX_PWR:
            aml_set_tx_pwr(dev,set1);
            break;
        case AML_IWP_SET_OLPC_PWR:
            aml_set_olpc_pwr(dev, set1);
            break;
        case AML_IWP_SET_XOSC_CTUNE:
            aml_set_xosc_ctune(dev, wrqu, extra, set1);
            break;
        case AML_IWP_SET_POWER_OFFSET:
            aml_set_power_offset(dev, wrqu, extra, set1);
            break;
        case AML_IWP_SET_RAM_EFUSE:
            aml_set_ram_efuse(dev, set1);
            break;
        case AML_IWP_SET_XOSC_EFUSE:
            aml_set_xosc_efuse(dev,set1);
            break;
        case AML_IWP_SET_TXPAGE_ONCE:
            aml_set_txpage_once(dev,set1);
            break;
        case AML_IWP_SET_TXCFM_TRI_TX:
            aml_set_txcfm_tri_tx(dev,set1);
            break;
        case AML_IWP_RECY_CTRL:
            aml_recy_ctrl(dev, set1);
            break;
        case AML_IWP_SET_LIMIT_POWER:
            aml_set_limit_power_status(dev, set1);
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
            aml_set_legacy_rate(dev, set1, set2);
            break;
        case AML_IWP_SET_RF_REG:
            aml_rf_reg_write(dev, set1, set2);
            break;
        case AML_IWP_SET_REG:
            aml_set_reg(dev, set1, set2);
            break;
        case AML_IWP_SET_EFUSE:
            aml_set_efuse(dev, set1, set2);
            break;
        case AML_MEM_DUMP:
            aml_dump_reg(dev, set1, set2);
            break;
        case AML_IWP_GET_FW_LOG:
            aml_set_fwlog_cmd(dev, set1, set2);
            break;
        case AML_IWP_SET_RX:
            aml_set_rx(dev, set1, set2);
            break;
        case AML_IWP_SET_TX_PROT:
            aml_set_tx_prot(dev, set1, set2);
            break;
        case AML_IWP_SET_TX_PATH:
            aml_set_tx_path(dev, set1, set2);
            break;
        case AML_IWP_GET_CSI_STATUS_SP:
            aml_get_csi_status_sp(dev, set1, set2);
            break;
        default:
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
        case AML_IWP_SET_P2P_NOA:
            aml_set_p2p_noa(dev, set1, set2, set3);
            break;
#ifdef TEST_MODE
        case AML_IWP_PCIE_TEST:
            aml_pcie_prssr_test(dev, set1, set2, set3);
            break;
#endif
        case AML_COEX_CMD:
            aml_coex_cmd(dev, set1, set2, set3);
            break;
        default:
            break;
    }

    return 0;
}

static int aml_iwpriv_send_para4(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int *param = (int *)extra;
    int sub_cmd = param[0];
    int set1 = param[1];
    int set2 = param[2];
    int set3 = param[3];
    int set4 = param[4];

    printk("%s cmd:%d set1:%d set2:%d set3:%d set4:%d\n", __func__, sub_cmd, set1, set2, set3, set4);

    switch (sub_cmd) {
        case AML_IWP_SET_MACBYPASS:
            aml_set_macbypass(dev, set1, set2, set3, set4);
            break;
        default:
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
            aml_get_version();
            break;
        case AML_IWP_SET_RATE_AUTO:
            aml_set_fixed_rate(dev, RC_AUTO_RATE_INDEX);
            break;
        case AML_IWP_GET_RATE_INFO:
            aml_get_rate_info(dev);
            break;
        case AML_IWP_GET_STATS:
            aml_get_tx_stats(dev);
            break;
        case AML_IWP_GET_ACS_INFO:
            aml_get_acs_info(dev);
            break;
        case AML_IWP_GET_AMSDU_MAX:
            aml_get_amsdu_max(dev);
            break;
        case AML_IWP_GET_AMSDU_TX:
            aml_get_amsdu_tx(dev);
            break;
        case AML_IWP_GET_LDPC:
            aml_get_ldpc(dev);
            break;
        case AML_IWP_GET_TXQ:
            aml_get_txq(dev);
            aml_txq_unexpection(dev);
            break;
        case AML_IWP_GET_TX_LFT:
            aml_get_tx_lft(dev);
            break;
        case AML_IWP_GET_LAST_RX:
            aml_get_last_rx(dev);
            break;
        case AML_IWP_CLEAR_LAST_RX:
            aml_clear_last_rx(dev);
            break;
        case AML_IWP_SET_STOP_MACBYPASS:
            aml_set_stop_macbypass(dev);
            break;
        case AML_IWP_GET_STBC:
            aml_get_stbc(dev);
            break;
         case AML_LA_DUMP:
            aml_emb_la_dump(dev);
            break;
         case AML_IWP_CCA_CHECK:
            aml_cca_check(dev);
            break;
         case AML_IWP_GET_CHAN_LIST:
            aml_get_chan_list_info(dev);
            break;
        case AML_IWP_GET_CLK:
           aml_get_clock(dev);
           break;
        if (aml_bus_type != PCIE_MODE) {
            case AML_IWP_BUS_START_TEST:
                aml_sdio_start_test(dev);
                break;
        }
        case AML_IWP_GET_MSGIND:
            aml_get_proc_msg(dev);
            break;
        case AML_IWP_GET_RXIND:
            aml_get_proc_rxbuff(dev);
            break;
        case AML_IWP_SET_RX_START:
            aml_set_rx_start(dev);
            break;
        case AML_IWP_SET_TX_END:
            aml_set_tx_end(dev);
            break;
        case AML_IWP_SET_TX_START:
            aml_set_tx_start(dev);
            break;
        case AML_IWP_GET_BUF_STATE:
            aml_get_buf_state(dev);
            break;
        case AML_IWP_GET_CSI_STATUS_COM:
            aml_get_csi_status_com(dev);
            break;
        default:
            printk("%s %d param err\n", __func__, __LINE__);
            break;
    }

    return 0;
}


static int aml_iwpriv_get_char(struct net_device *dev,
    struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    int sub_cmd = wrqu->data.flags;

    char set[MAX_CHAR_SIZE];

    if (wrqu->data.length > 0) {
        if (copy_from_user(set,
            wrqu->data.pointer, wrqu->data.length)) {
            return -EFAULT;
        }
        set[wrqu->data.length] = '\0';
    }

    switch (sub_cmd) {
        case AML_IWP_GET_REG:
            aml_get_reg(dev, set, wrqu, extra);
            break;
        case AML_IWP_GET_RF_REG:
            aml_get_rf_reg(dev, set, wrqu, extra);
            break;
        case AML_IWP_SET_DEBUG:
            aml_set_debug(dev, set);
            break;
        case AML_IWP_SEND_TWT_SETUP_REQ:
            aml_send_twt_req(dev, set, wrqu, extra);
            break;
        case AML_IWP_GET_EFUSE:
            aml_get_efuse(dev, set, wrqu, extra);
            break;
        case AML_IWP_SET_WIFI_MAC_EFUSE:
            aml_set_wifi_mac_addr(dev, set);
        case AML_IWP_SET_RX_END:
            aml_set_rx_end(dev,wrqu, extra);
            break;
        case AML_IWP_GET_WIFI_MAC_FROM_EFUSE:
            aml_get_mac_addr(dev,wrqu, extra);
            break;
        case AML_IWP_SET_BT_MAC_EFUSE:
            aml_set_bt_mac_addr(dev, set);
            break;
        case AML_IWP_GET_BT_MAC_FROM_EFUSE:
            aml_get_bt_mac_addr(dev,wrqu, extra);
            break;
        case AML_IWP_GET_XOSC_OFFSET:
            aml_get_xosc_offset(dev,wrqu, extra);
            break;
    }

    return 0;
}

int iw_standard_set_mode(struct net_device *dev, struct iw_request_info *info,
    union iwreq_data *wrqu, char *extra)
{
    struct aml_vif *vif = netdev_priv(dev);
    struct aml_hw *aml_hw = vif->aml_hw;

    printk("%s:%d param:%d", __func__, __LINE__, wrqu->param.value);
    aml_cfg80211_change_iface(aml_hw->wiphy, dev,
            (enum nl80211_iftype)wrqu->param.value, NULL);

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
    aml_iwpriv_get_char,
    NULL,
    aml_iwpriv_send_para4,
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
        AML_IWP_RECY_CTRL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "recy_ctrl"},
    {
        AML_IWP_GET_RATE_INFO,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate_info"},
    {
        AML_IWP_GET_STATS,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_stats"},
    {
        AML_IWP_GET_ACS_INFO,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_acs"},
    {
        AML_IWP_GET_AMSDU_MAX,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_amsdu_max"},
    {
        AML_IWP_GET_AMSDU_TX,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_amsdu_tx"},
    {
        AML_IWP_GET_LDPC,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ldpc"},
    {
        AML_IWP_GET_TXQ,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_txq"},
    {
        AML_IWP_GET_TX_LFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tx_lft"},
    {
        AML_IWP_GET_LAST_RX,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_last_rx"},
    {
        AML_IWP_CLEAR_LAST_RX,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "clear_last_rx"},
    {
        AML_IWP_SET_STOP_MACBYPASS,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "stop_macbypass"},
    {
        AML_IWP_GET_STBC,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_stbc"},
    {
        AML_LA_DUMP,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "la_dump"},
    {
        AML_IWP_CCA_CHECK,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "cca_check"},
    {
        AML_IWP_BUS_START_TEST,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "usb_start_test"},
    {
        AML_IWP_BUS_START_TEST,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "sdio_start_test"},
    {
        AML_IWP_GET_MSGIND,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_msgind"},
    {
        AML_IWP_GET_RXIND,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rxind"},
    {
        AML_IWP_SET_RX_START,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "pt_rx_start"},
    {
        AML_IWP_SET_TX_END,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "pt_tx_end"},
    {
        AML_IWP_SET_TX_START,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "pt_tx_start"},
    {
        AML_IWP_GET_BUF_STATE,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_buf_state"},
    {
        SIOCIWFIRSTPRIV + 1,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""},
    {
        AML_IWP_SET_RATE_LEGACY_OFDM,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rate_ofdm"},
    {
        AML_IWP_SET_SCAN_HANG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_sc_hang"},
    {
        AML_IWP_SET_SCAN_TIME,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_sc_time"},
    {
        AML_IWP_SET_PS_MODE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ps_mode"},
    {
        AML_IWP_SET_AMSDU_MAX,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_amsdu_max"},
    {
        AML_IWP_SET_AMSDU_TX,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_amsdu_tx"},
    {
        AML_IWP_SET_LDPC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ldpc"},
    {
        AML_IWP_SET_P2P_OPPPS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_p2p_oppps"},
    {
        AML_IWP_SET_TX_LFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_lft"},
    {
        AML_IWP_SET_STBC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_stbc"},
    {
        AML_IWP_SET_PT_CALIBRATION,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_pt_cali"},
    {
        AML_PCIE_STATUS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_pcie_status"},
    {
        AML_IWP_ENABLE_WF,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "enable_wf"},
#ifdef SDIO_SPEED_DEBUG
    {
        AML_IWP_ENABLE_SDIO_CAL_SPEED,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sdio_speed"},
#endif
    {
        AML_IWP_SEND_TWT_TEARDOWN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "send_twt_td"},
    {
        AML_IWP_SET_TX_MODE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_mode"},
    {
        AML_IWP_SET_TX_BW,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_bw"},
    {
        AML_IWP_SET_TX_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_rate"},
    {
        AML_IWP_SET_TX_LEN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_len"},
    {
        AML_IWP_SET_TX_PWR,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tx_pwr"},
    {
        AML_IWP_SET_OLPC_PWR,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_olpc_pwr"},
    {
        AML_IWP_SET_XOSC_CTUNE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_xosc_ctune"},
    {
        AML_IWP_SET_POWER_OFFSET,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_pwr_ofset"},
    {
        AML_IWP_SET_RAM_EFUSE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ram_efuse"},
    {
        AML_IWP_SET_XOSC_EFUSE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_xosc_efuse"},
    {
        AML_IWP_SET_TXPAGE_ONCE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_txpage_once"},
    {
        AML_IWP_SET_TXCFM_TRI_TX,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tri_tx_thr"},
    {
        AML_IWP_SET_LIMIT_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_limit_power"},
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
        AML_IWP_SET_EFUSE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_efuse"},
    {
        AML_MEM_DUMP,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "mem_dump"},
    {
        AML_IWP_GET_FW_LOG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "get_fw_log"},
    {
        AML_IWP_SET_RX,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "pt_set_rx"},
    {
        AML_IWP_SET_TX_PROT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_tx_prot"},
    {
        AML_IWP_SET_TX_PATH,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_tx_path"},
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
    {
        AML_IWP_SET_P2P_NOA,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_p2p_noa"},
#ifdef TEST_MODE
    {
        AML_IWP_PCIE_TEST,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "pcie_test"},
#endif
    {
        AML_COEX_CMD,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "coex_cmd"},
    {
        SIOCIWFIRSTPRIV + 5,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, ""},
    {
        AML_IWP_GET_REG,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_reg"},
    {
        AML_IWP_GET_RF_REG,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_rf_reg"},
    {
        AML_IWP_SET_DEBUG,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "set_debug"},
    {
        AML_IWP_SEND_TWT_SETUP_REQ,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "send_twt_req"},
    {
        AML_IWP_GET_EFUSE,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_efuse"},
    {
        AML_IWP_SET_BT_MAC_EFUSE,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "set_bt_mac"},
    {
        AML_IWP_GET_BT_MAC_FROM_EFUSE,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_bt_mac"},
    {
        AML_IWP_SET_RX_END,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "pt_rx_end"},
    {
        AML_IWP_GET_XOSC_OFFSET,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_xosc_offset"},
    {
        SIOCIWFIRSTPRIV + 7,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 4, 0, ""},
    {
        AML_IWP_SET_MACBYPASS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "set_macbypass"},
    {
        AML_IWP_GET_CHAN_LIST,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chan_list"},
    {
        AML_IWP_GET_CLK,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "clk_msr"},
    {
        AML_IWP_SET_WIFI_MAC_EFUSE,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "set_wifi_mac"},
    {
        AML_IWP_GET_WIFI_MAC_FROM_EFUSE,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "get_wifi_mac"},
    {
        AML_IWP_GET_CSI_STATUS_COM,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_csi_com"},
    {
        AML_IWP_GET_CSI_STATUS_SP,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "get_csi_sp"},

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

