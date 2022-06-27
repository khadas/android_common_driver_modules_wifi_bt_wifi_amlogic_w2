#include <linux/sort.h>
#include "rwnx_iwpriv_cmds.h"
#include "rwnx_mod_params.h"
#include "rwnx_debugfs.h"
#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "rwnx_platform.h"

#define RC_AUTO_RATE_INDEX -1
#define MAX_CHAR_SIZE 40
#define PRINT_BUF_SIZE 1024
#define LA_BUF_SIZE 2048
#define LA_MEMORY_BASE_ADDRESS 0x60830000
static char *mactrace_path = "/data/la_dump/mactrace";

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

        if (sta && sta->valid && (rwnx_vif->vif_index == sta->vif_idx)) {
             if ((error = rwnx_send_me_rc_set_rate(rwnx_hw, sta->sta_idx,
                (u16)rate_config.value)) != 0) {
                return error;
            }

            rwnx_hw->debugfs.rc_config[sta->sta_idx] = (int)rate_config.value;
        }
    }

    return error;
}

static int aml_set_p2p_noa(struct net_device *dev, int count, int interval, int duration)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct mm_set_p2p_noa_cfm cfm;
    if (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_P2P_GO) {
        /* Forward request to the embedded and wait for confirmation */
        rwnx_send_p2p_noa_req(rwnx_hw, rwnx_vif, count, interval, duration, 0,  &cfm);
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

    aml_scan_hang(rwnx_vif, scan_hang);

    return 0;
}

static int aml_set_scan_time(struct net_device *dev, int scan_duration)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("set scan duration to %d us \n", scan_duration);
    rwnx_vif->sta.scan_duration = scan_duration;

    return 0;
}

int aml_get_reg(struct net_device *dev, char *str_addr, union iwreq_data *wrqu, char *extra)
{
    unsigned int addr = 0;
    unsigned int reg_val = 0;
#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    u8 *map_address = NULL;
#else
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
#endif

    addr = simple_strtol(str_addr, NULL, 0);

#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    map_address = rwnx_pci_get_map_address(dev, addr);

    if (map_address) {
       reg_val = readl(map_address);
    }
#else
    reg_val = RWNX_REG_READ(rwnx_plat, 0, addr);
#endif

    wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "&0x%08x", reg_val);
    wrqu->data.length++;

//    printk("Get reg addr: 0x%08x value: 0x%08x\n", addr, reg_val);
    return 0;
}

int aml_set_reg(struct net_device *dev, int addr, int val)
{
#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
#endif

#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    u8* map_address = NULL;
    map_address = rwnx_pci_get_map_address(dev, addr);

    if (map_address) {
        writel(val, map_address);
    }

#else
    RWNX_REG_WRITE(val, rwnx_plat, 0, addr);
#endif

    printk("Set reg addr: 0x%08x value:0x%08x\n", addr, val);
    return 0;
}

int aml_get_efuse(struct net_device *dev, int addr)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("Get efuse addr: 0x%08x\n", addr);

    rwnx_get_efuse(rwnx_vif, addr);

    return 0;
}

int aml_set_efuse(struct net_device *dev, int addr, int val)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("Set reg addr: 0x%08x value:0x%08x\n", addr, val);

    rwnx_set_efuse(rwnx_vif, addr, val);

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

static int aml_set_p2p_oppps(struct net_device *dev, int ctw)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;
    struct mm_set_p2p_oppps_cfm cfm;
    if (RWNX_VIF_TYPE(rwnx_vif) == NL80211_IFTYPE_P2P_GO) {
        /* Forward request to the embedded and wait for confirmation */
        rwnx_send_p2p_oppps_req(rwnx_hw, rwnx_vif, (u8)ctw, &cfm);
    }
    return 0;
}

static int aml_set_amsdu_max(struct net_device *dev, int amsdu_max)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;

    rwnx_hw->mod_params->amsdu_maxnb = amsdu_max;
    rwnx_adjust_amsdu_maxnb(rwnx_hw);
    return 0;
}

static int aml_set_amsdu_tx(struct net_device *dev, int amsdu_tx)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;

    if (rwnx_hw->mod_params->amsdu_tx == amsdu_tx) {
        printk("amsdu tx did not change, ignore\n");
        return 0;
    }

    rwnx_hw->mod_params->amsdu_tx = amsdu_tx;
    printk("set amsdu_tx:0x%x success\n", amsdu_tx);
    return rwnx_set_amsdu_tx(rwnx_hw, amsdu_tx);
}

static int aml_set_ldpc(struct net_device *dev, int ldpc)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;

    if (rwnx_hw->mod_params->ldpc_on == ldpc) {
        printk("ldpc did not change, ignore\n");
        return 0;
    }
    printk("set ldpc: 0x%x success\n", ldpc);
    rwnx_hw->mod_params->ldpc_on = ldpc;
    // LDPC is mandatory for HE40 and above, so if LDPC is not supported, then disable
    // support for 40 and 80MHz
    if (rwnx_hw->mod_params->he_on && !rwnx_hw->mod_params->ldpc_on)
    {
        rwnx_hw->mod_params->use_80 = false;
        rwnx_hw->mod_params->use_2040 = false;
    }
    rwnx_set_he_capa(rwnx_hw, rwnx_hw->wiphy);
    rwnx_set_vht_capa(rwnx_hw, rwnx_hw->wiphy);
    rwnx_set_ht_capa(rwnx_hw, rwnx_hw->wiphy);

    return rwnx_set_ldpc_tx(rwnx_hw, rwnx_vif);
}

static int aml_set_tx_lft(struct net_device *dev, int tx_lft)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw * rwnx_hw = rwnx_vif->rwnx_hw;

    if (rwnx_hw->mod_params->tx_lft == tx_lft) {
        printk("tx_lft did not change, ignore\n");
        return 0;
    }

    rwnx_hw->mod_params->tx_lft= tx_lft;
    printk("set tx_lft:0x%x success\n", tx_lft);
    return rwnx_set_tx_lft(rwnx_hw, tx_lft);
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

int aml_print_acs_info(struct rwnx_hw *priv)
{
    struct wiphy *wiphy = priv->wiphy;
    char buf[(SCAN_CHANNEL_MAX + 1) * 43];
    int survey_cnt = 0;
    int len = 0;
    int band, chan_cnt;

    mutex_lock(&priv->dbgdump.mutex);
    len += scnprintf(buf, sizeof(buf) - 1,  "FREQ      TIME(ms)     BUSY(ms)     NOISE(dBm)\n");

    for (band = NL80211_BAND_2GHZ; band <= NL80211_BAND_5GHZ; band++) {
        for (chan_cnt = 0; chan_cnt < wiphy->bands[band]->n_channels; chan_cnt++) {
            struct rwnx_survey_info *p_survey_info = &priv->survey[survey_cnt];
            struct ieee80211_channel *p_chan = &wiphy->bands[band]->channels[chan_cnt];

            if (p_survey_info->filled) {
                len += scnprintf(&buf[len], sizeof(buf) - len - 1,
                                  "%d    %03d         %03d          %d\n",
                                  p_chan->center_freq,
                                  p_survey_info->chan_time_ms,
                                  p_survey_info->chan_time_busy_ms,
                                  p_survey_info->noise_dbm);
            } else {
                len += scnprintf(&buf[len], sizeof(buf) -len -1,
                                  "%d    NOT AVAILABLE\n",
                                  p_chan->center_freq);
            }

            survey_cnt++;
        }
    }

    mutex_unlock(&priv->dbgdump.mutex);
    aml_print_buf(buf, len);
    return 0;
}

int aml_print_last_rx_info(struct rwnx_hw *priv, struct rwnx_sta *sta)
{
    struct rwnx_rx_rate_stats *rate_stats;
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
            union rwnx_rate_ctrl_info rate_config;
            int percent = (((u64)rate_stats->table[i]) * 1000) / rate_stats->cpt;
            int p;
            int ru_size;

            idx_to_rate_cfg(i, &rate_config, &ru_size);
            len += print_rate_from_cfg(&buf[len], bufsz - len,
                                       rate_config.value, NULL, ru_size);
            p = (percent * hist_len) / 1000;
            len += scnprintf(&buf[len], bufsz - len, ": %9d(%2d.%1d%%)%.*s\n",
                             rate_stats->table[i],
                             percent / 10, percent % 10, p, hist);
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
    return 0;
}

int aml_print_stats(struct rwnx_hw *priv)
{
    char *buf;
    int ret;
    int i, skipped;
#ifdef CONFIG_RWNX_SPLIT_TX_BUF
    int per;
#endif
    int bufsz = (NX_TXQ_CNT) * 20 + (ARRAY_SIZE(priv->stats.amsdus_rx) + 1) * 40
        + (ARRAY_SIZE(priv->stats.ampdus_tx) * 30);

    buf = kmalloc(bufsz, GFP_ATOMIC);
    if (buf == NULL)
        return 0;

    ret = scnprintf(buf, bufsz, "TXQs CFM balances ");
    for (i = 0; i < NX_TXQ_CNT; i++)
        ret += scnprintf(&buf[ret], bufsz - ret,
                            "    [%1d]:%3d", i,
                            priv->stats.cfm_balance[i]);

    ret += scnprintf(&buf[ret], bufsz - ret, "\n");

#ifdef CONFIG_RWNX_SPLIT_TX_BUF
    ret += scnprintf(&buf[ret], bufsz - ret,
                       "\nAMSDU[len]         done          failed   received\n");
    for (i = skipped = 0; i < NX_TX_PAYLOAD_MAX; i++) {
        if (priv->stats.amsdus[i].done) {
            per = DIV_ROUND_UP((priv->stats.amsdus[i].failed) *
                                100, priv->stats.amsdus[i].done);
        } else if (priv->stats.amsdus_rx[i]) {
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
                          priv->stats.amsdus[i].done,
                          priv->stats.amsdus[i].failed, per,
                          priv->stats.amsdus_rx[i]);
    }

    for (; i < ARRAY_SIZE(priv->stats.amsdus_rx); i++) {
        if (!priv->stats.amsdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret, "     ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                          "     [%2d]                                %10d\n",
                          i + 1, priv->stats.amsdus_rx[i]);
    }
#else
    ret += scnprintf(&buf[ret], bufsz - ret,
                      "\nAMSDU[len]	 received\n");
    for (i = skipped = 0; i < ARRAY_SIZE(priv->stats.amsdus_rx); i++) {
        if (!priv->stats.amsdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret,  " ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                          "     [%2d]      %10d\n",
                          i + 1, priv->stats.amsdus_rx[i]);
    }

#endif /* CONFIG_RWNX_SPLIT_TX_BUF */

    ret += scnprintf(&buf[ret], bufsz - ret,
                       "\nAMPDU[len]       done  received\n");
    for (i = skipped = 0; i < ARRAY_SIZE(priv->stats.ampdus_tx); i++) {
        if (!priv->stats.ampdus_tx[i] && !priv->stats.ampdus_rx[i]) {
            skipped = 1;
            continue;
        }
        if (skipped) {
            ret += scnprintf(&buf[ret], bufsz - ret,  "   ...\n");
            skipped = 0;
        }

        ret += scnprintf(&buf[ret], bufsz - ret,
                         "     [%2d]     %9d %9d\n", i ? i + 1 : i,
                         priv->stats.ampdus_tx[i], priv->stats.ampdus_rx[i]);
    }
    /* coverity[assigned_value] - len is used */
    ret += scnprintf(&buf[ret], bufsz - ret,
                     "#mpdu missed          %9d\n",
                     priv->stats.ampdus_rx_miss);

    aml_print_buf(buf, ret);
    kfree(buf);
    return 0;

}

int aml_print_rate_info( struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta)
{
    char *buf;
    int bufsz, len = 0;
    int i = 0;
    int error = 0;
    struct me_rc_stats_cfm me_rc_stats_cfm;
    unsigned int no_samples;
    struct st *st;
     /* Forward the information to the LMAC */
    if ((error = rwnx_send_me_rc_stats(rwnx_hw, sta->sta_idx, &me_rc_stats_cfm)))
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
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    u8 i = 0;
    struct rwnx_sta *sta = NULL;
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = &rwnx_hw->sta_table[i];
        if (sta && sta->valid && (rwnx_vif->vif_index == sta->vif_idx)) {
            aml_print_rate_info(rwnx_hw, sta);
        }
    }
    return 0;
}

static int aml_get_stats(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;

    aml_print_stats(rwnx_hw);
    return 0;
}

static int aml_get_acs_info(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;

    aml_print_acs_info(rwnx_hw);
    return 0;
}

static int aml_get_chan_list_info(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct wiphy *wiphy = rwnx_hw->wiphy;
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

static int aml_get_last_rx(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    u8 i = 0;
    struct rwnx_sta *sta = NULL;
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = &rwnx_hw->sta_table[i];
        if (sta && sta->valid && (rwnx_vif->vif_index == sta->vif_idx)) {
            aml_print_last_rx_info(rwnx_hw, sta);
        }
    }
    return 0;
}

static int aml_clear_last_rx(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    u8 i = 0;
    struct rwnx_sta *sta = NULL;
    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        sta = &rwnx_hw->sta_table[i];
        if (sta && sta->valid && (rwnx_vif->vif_index == sta->vif_idx)) {
            /* Prevent from interrupt preemption as these statistics are updated under
             * interrupt */
            spin_lock_bh(&rwnx_hw->tx_lock);
            memset(sta->stats.rx_rate.table, 0,
                   sta->stats.rx_rate.size * sizeof(sta->stats.rx_rate.table[0]));
            sta->stats.rx_rate.cpt = 0;
            sta->stats.rx_rate.rate_cnt = 0;
            spin_unlock_bh(&rwnx_hw->tx_lock);
        }
    }
    return 0;
}


static int aml_get_amsdu_max(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    printk("current amsdu_max: %d\n", rwnx_hw->mod_params->amsdu_maxnb);
    return 0;
}

static int aml_get_amsdu_tx(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    printk("current amsdu_tx: %d\n", rwnx_hw->mod_params->amsdu_tx);
    return 0;
}

static int aml_get_ldpc(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    printk("current ldpc: %d\n", rwnx_hw->mod_params->ldpc_on);
    return 0;
}

static int aml_get_tx_lft(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    printk("current tx_lft: %d\n", rwnx_hw->mod_params->tx_lft);
    return 0;
}


static int aml_get_txq(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_vif *vif;
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

    //spin_lock_bh(&rwnx_hw->tx_lock);
    list_for_each_entry(vif, &rwnx_hw->vifs, list) {
        res = scnprintf(&buf[idx], bufsz, "\n"VIF_SEP);
        idx += res;
        bufsz -= res;
        res = rwnx_dbgfs_txq_vif(&buf[idx], bufsz, vif, rwnx_hw);
        idx += res;
        bufsz -= res;
        res = scnprintf(&buf[idx], bufsz, VIF_SEP);
        idx += res;
        bufsz -= res;
    }
    //spin_unlock_bh(&rwnx_hw->tx_lock);

    printk("%s\n", buf);
    kfree(buf);

    return 0;
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

int aml_recovery(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("Recovery!\n");

    rwnx_recovery(rwnx_vif);

    return 0;
}

int aml_set_macbypass(struct net_device *dev, int format_type, int bandwidth, int rate, int siso_or_mimo)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("set macbypass, format_type:%d, bandwidth:%d, rate:%d, siso_or_mimo:%d!\n",
           format_type, bandwidth, rate, siso_or_mimo);

    rwnx_set_macbypass(rwnx_vif, format_type, bandwidth, rate, siso_or_mimo);

    return 0;
}

int aml_set_stop_macbypass(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("Stop macbypass!\n");

    rwnx_set_stop_macbypass(rwnx_vif);

    return 0;
}

int aml_set_stbc(struct net_device *dev, int stbc_on)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;

    if (rwnx_hw->mod_params->stbc_on == stbc_on) {
        printk("stbc_on did not change, ignore\n");
        return 0;
    }

    rwnx_hw->mod_params->stbc_on = stbc_on;
    printk("set stbc_on:%d success\n", stbc_on);

    /* Set VHT capabilities */
    rwnx_set_vht_capa(rwnx_hw, rwnx_hw->wiphy);

    /* Set HE capabilities */
    rwnx_set_he_capa(rwnx_hw,  rwnx_hw->wiphy);

    /* Set HT capabilities */
    rwnx_set_ht_capa(rwnx_hw,  rwnx_hw->wiphy);

    return rwnx_set_stbc(rwnx_hw, rwnx_vif->vif_index, stbc_on);
}

static int aml_get_stbc(struct net_device *dev)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    printk("current stbc: %d\n", rwnx_hw->mod_params->stbc_on);
    return 0;
}

static int aml_emb_la_dump(struct net_device *dev)
{
    struct file *fp = NULL;
    mm_segment_t old_fs;

    int len = 0, i = 0;
    char *la_buf = NULL;

#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    u8 *map_address = NULL;
    map_address = rwnx_pci_get_map_address(dev, LA_MEMORY_BASE_ADDRESS);
    if (!map_address) {
        printk("%s: map_address erro\n", __func__);
        return 0;
    }
#else
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
#endif

    la_buf = kmalloc(LA_BUF_SIZE, GFP_ATOMIC);
    if (!la_buf) {
         printk("%s: malloc buf erro\n", __func__);
         return 0;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(mactrace_path, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, 0644);
    if (IS_ERR(fp)) {
        printk("%s: mactrace file open failed: PTR_ERR(fp) = %d\n", __func__, PTR_ERR(fp));
        goto err;
    }
    fp->f_pos = 0;

    memset(la_buf, 0, LA_BUF_SIZE);

#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    for (i=0; i < 0x3fff; i+=2) {
        len += scnprintf(&la_buf[len], (LA_BUF_SIZE - len), "%08x%08x\n",
            readl(map_address+((1+i)*4)), readl(map_address+(i*4)));

        if ((LA_BUF_SIZE - len) < 20) {
            vfs_write(fp, la_buf, len, &fp->f_pos);

            len = 0;
            memset(la_buf, 0, LA_BUF_SIZE);
        }
    }

    if (len != 0) {
        vfs_write(fp, la_buf, len, &fp->f_pos);
    }
#else
    for (i=0; i < 0x3fff; i+=2) {
        len += scnprintf(&la_buf[len], (LA_BUF_SIZE - len), "%08x%08x\n",
            RWNX_REG_READ(rwnx_plat, 0, LA_MEMORY_BASE_ADDRESS+((1+i)*4)),
            RWNX_REG_READ(rwnx_plat, 0, LA_MEMORY_BASE_ADDRESS+(i*4)));

        if ((LA_BUF_SIZE - len) < 20) {
            vfs_write(fp, la_buf, len, &fp->f_pos);

            len = 0;
            memset(la_buf, 0, LA_BUF_SIZE);
        }
    }

    if (len != 0) {
        vfs_write(fp, la_buf, len, &fp->f_pos);
    }
#endif

    filp_close(fp, NULL);

err:
    set_fs(old_fs);
    kfree(la_buf);
    return 0;
}

int aml_set_pt_calibration(struct net_device *dev, int pt_cali_val)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);

    printk("set pt calibration, pt calibration conf:%x\n", pt_cali_val);

    rwnx_set_pt_calibration(rwnx_vif, pt_cali_val);

    return 0;
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
        case AML_IWP_SET_SCAN_HANG:
            aml_set_scan_hang(dev, set1);
            break;
        case AML_IWP_SET_SCAN_TIME:
            aml_set_scan_time(dev, set1);
            break;
        case AML_IWP_SET_PS_MODE:
            aml_set_ps_mode(dev, set1);
            break;
        case AML_IWP_GET_EFUSE:
             aml_get_efuse(dev, set1);
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
         case AML_IWP_SET_EFUSE:
            aml_set_efuse(dev, set1, set2);
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
        case AML_IWP_RECOVERY:
            aml_recovery(dev);
            break;
        case AML_IWP_GET_RATE_INFO:
            aml_get_rate_info(dev);
            break;
        case AML_IWP_GET_STATS:
            aml_get_stats(dev);
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
         case AML_IWP_GET_CHAN_LIST:
            aml_get_chan_list_info(dev);
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
        AML_IWP_SEND_TWT_SETUP_REQ,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "send_twt_req"},
    {
        AML_IWP_SEND_TWT_TEARDOWN,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "send_twt_td"},
    {
        AML_IWP_RECOVERY,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "recovery"},
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
        AML_IWP_GET_EFUSE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_efuse"},
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
        SIOCIWFIRSTPRIV + 7,
        IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 4, 0, ""},
    {
        AML_IWP_SET_MACBYPASS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "set_macbypass"},
    {
        AML_IWP_GET_CHAN_LIST,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chan_list"},
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

