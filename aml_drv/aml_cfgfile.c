/**
 ****************************************************************************************
 *
 * @file aml_cfgfile.c
 *
 * Copyright (C) Amlogic 2012-2021
 * Copyright (C) Amlogic 2022
 *
 ****************************************************************************************
 */
#include <linux/firmware.h>
#include <linux/if_ether.h>

#include "aml_defs.h"
#include "aml_cfgfile.h"
#include "aml_msg_tx.h"
#include "aml_utils.h"

static struct file *aml_cfgfile_open(const char *path, int flag, int mode)
{
    struct file *fp;
    fp = FILE_OPEN(path, flag, mode);
    if (IS_ERR(fp)) {
        return NULL;
    }
    return fp;
}

static void aml_cfgfile_close(struct file *fp)
{
    FILE_CLOSE(fp, NULL);
}

static int aml_cfgfile_read(struct file *fp, char *buf, int len)
{
    int rlen = 0, sum = 0;

    while (sum < len) {
        rlen = FILE_READ(fp, buf + sum, len - sum, &fp->f_pos);
        if (rlen > 0) {
            sum += rlen;
        } else if (0 != rlen) {
            return rlen;
        } else {
            break;
        }
    }
    return sum;
}

static int aml_cfgfile_write(struct file *fp, char *buf, int len)
{
    int wlen = 0, sum = 0;

    while (sum < len) {
        wlen = FILE_WRITE(fp, buf + sum, len - sum, &fp->f_pos);
        if (wlen > 0)
            sum += wlen;
        else if (0 != wlen)
            return wlen;
        else
            break;
    }
    return sum;
}

static int aml_cfgfile_retrieve(const char *path, u8 *buf, u32 len)
{
    struct file *fp;
    int ret = -1;

    if (path && buf) {
        fp = aml_cfgfile_open(path, O_RDONLY, 0);
        if (fp) {
            ret = aml_cfgfile_read(fp, buf, len);
            aml_cfgfile_close(fp);
        }
    } else {
        ret = -EINVAL;
    }
    return (ret >= 0 ? ret : 0);
}

static int aml_cfgfile_create(const char *path, struct file **fpp)
{
    struct file *fp;

    fp = aml_cfgfile_open(path, O_RDONLY, 0);
    if (!fp) {
        fp = aml_cfgfile_open(path, O_CREAT | O_RDWR, 0666);
        if (!fp) {
            return AML_CFGFILE_ERROR;
        }
        *fpp = fp;
        return AML_CFGFILE_CREATE;
    }
    *fpp = fp;
    return AML_CFGFILE_EXIST;
}

static const u8 *aml_cfgfile_find_tag(const u8 *file_data, unsigned int file_size,
        const char *tag_name, unsigned int tag_len)
{
    unsigned int curr, line_start = 0, line_size;

    while (line_start < file_size) {
        for (curr = line_start; curr < file_size; curr++)
            if (file_data[curr] == '\n')
                break;

        line_size = curr - line_start;
        if ((line_size == (strlen(tag_name) + tag_len)) &&
            (!strncmp(&file_data[line_start], tag_name, strlen(tag_name)))) {
            return (&file_data[line_start + strlen(tag_name)]);
        }

        line_start = curr + 1;
    }

    return NULL;
}

static void aml_cfgfile_store_tag(struct file *fp, const char *tag_name, const char *tag_value)
{
    unsigned char lbuf[AML_CFGFILE_LBUF_MAXLEN];
    unsigned char fbuf[AML_CFGFILE_FBUF_MAXLEN];

    if (!fp || !tag_name || !tag_value)
        return;

    if (strlen(tag_name) + strlen(tag_value) > AML_CFGFILE_LBUF_MAXLEN) {
        AML_INFO("store tag name exceed max line len(%d)", AML_CFGFILE_LBUF_MAXLEN);
        return;
    }

    sprintf(lbuf, "%s=%s\n", tag_name, tag_value);
    AML_INFO("store %s=%s(len=%d) tag to file\n", tag_name, tag_value, strlen(lbuf));

    aml_cfgfile_read(fp, fbuf, AML_CFGFILE_FBUF_MAXLEN);
    if (strlen(fbuf) + strlen(lbuf) > AML_CFGFILE_FBUF_MAXLEN) {
        AML_INFO("store tag name exceed max file len(%d)", AML_CFGFILE_FBUF_MAXLEN);
        goto out;
    }
    sprintf(fbuf, "%s", lbuf);
    aml_cfgfile_write(fp, fbuf, strlen(fbuf));

out:
    return;
}

static void aml_get_chip_id_from_efuse(struct aml_hw *aml_hw, char *chipid_buf)
{
    unsigned int chipid_low = 0;
    unsigned int chipid_high = 0;

    chipid_low = aml_efuse_read(aml_hw, AML_CFGFILE_CHIPID_LOW);
    chipid_high = aml_efuse_read(aml_hw, AML_CFGFILE_CHIPID_HIGH);
    sprintf(chipid_buf, "%04x%08x", chipid_high & 0xffff, chipid_low);
}

static void aml_cfgfile_store_chipid(struct aml_hw *aml_hw, struct file *fp, char *chipid_buf)
{
    aml_cfgfile_store_tag(fp, "CHIP_ID", chipid_buf);
}

static void aml_cfgfile_store_randmac(struct aml_hw *aml_hw, struct file *fp)
{
#define AML_CFGFILE_TOGGLE_BIT(b, n) (b ^= (1 << n))
    u8 vif0_mac[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x00, 0x00, 0x00 };
    u8 vif1_mac[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x00, 0x00, 0x00 };
    char mac_str[strlen("00:00:00:00:00:00") + 1];
    u64 rand_num = 0;
    unsigned int efuse_data_l = 0;
    unsigned int efuse_data_h = 0;
    unsigned int mac_vld = 0;

    mac_vld = aml_efuse_read(aml_hw, AML_CFGFILE_MACADDR_VLD);
    mac_vld = (mac_vld >> 17) & 0x1;
    if (mac_vld == 1)
    {
        efuse_data_l = aml_efuse_read(aml_hw, AML_CFGFILE_MACADDR_LOW1);
        efuse_data_h = aml_efuse_read(aml_hw, AML_CFGFILE_MACADDR_HIGH1);
    }
    else
    {
        efuse_data_l = aml_efuse_read(aml_hw, AML_CFGFILE_MACADDR_LOW);
        efuse_data_h = aml_efuse_read(aml_hw, AML_CFGFILE_MACADDR_HIGH);
    }
    if (efuse_data_l != 0 && efuse_data_h != 0) {
        AML_INFO("efuse WIFI MAC addr is: "MACFMT"\n",
            (efuse_data_h & 0xff00) >> 8, efuse_data_h & 0x00ff, (efuse_data_l & 0xff000000) >> 24,
            (efuse_data_l & 0x00ff0000) >> 16, (efuse_data_l & 0xff00) >> 8, efuse_data_l & 0xff);

        vif0_mac[0] = (efuse_data_h & 0xff00) >> 8;
        vif0_mac[1] = efuse_data_h & 0x00ff;
        vif0_mac[2] = (efuse_data_l & 0xff000000) >> 24;
        vif0_mac[3] = (efuse_data_l & 0x00ff0000) >> 16;
        vif0_mac[4] = (efuse_data_l & 0xff00) >> 8;
        vif0_mac[5] = efuse_data_l & 0xff;
    } else {
        get_random_bytes(&rand_num, sizeof(u64));

        vif0_mac[3] = (rand_num & 0xff);
        vif0_mac[4] = ((rand_num >> 8) & 0xff);
        vif0_mac[5] = ((rand_num >> 16) & 0xff);
    }

    if (vif0_mac[0] & 0x3) {
        printk("change the mac addr from [0x%x] ", vif0_mac[0]);
        vif0_mac[0] &= ~0x3;
        printk("to [0x%x] \n", vif0_mac[0]);
    }

    sprintf(mac_str, MACFMT, MACARG(vif0_mac));
    aml_cfgfile_store_tag(fp, "VIF0_MACADDR", mac_str);

    memcpy(vif1_mac, vif0_mac, ETH_ALEN);
    AML_CFGFILE_TOGGLE_BIT(vif1_mac[5], 0);
    AML_CFGFILE_TOGGLE_BIT(vif1_mac[5], 1);
    sprintf(mac_str, MACFMT, MACARG(vif1_mac));
    aml_cfgfile_store_tag(fp, "VIF1_MACADDR", mac_str);
}

int aml_cfgfile_parse(struct aml_hw *aml_hw, struct aml_cfgfile *cfg)
{
    const char *path = AML_CFGFILE_DEFAULT_PATH;
    struct file *fp = NULL;
    u8 vif0_dft[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x11, 0x22, 0x33 };
    u8 vif1_dft[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x11, 0x22, 0x34 };
    u8 fbuf[AML_CFGFILE_FBUF_MAXLEN] = {0};
    const u8 *tag_ptr;
    int ret = -1;
    char chipid_buf[AML_CFGFILE_LBUF_MAXLEN];

    memcpy(cfg->vif0_mac, vif0_dft, ETH_ALEN);
    memcpy(cfg->vif1_mac, vif1_dft, ETH_ALEN);

    ret = aml_cfgfile_create(path, &fp);
    if (ret == AML_CFGFILE_ERROR) {
        AML_INFO("create config file failed\n");
        return -1;
    }

    aml_get_chip_id_from_efuse(aml_hw, chipid_buf);
    if (ret == AML_CFGFILE_CREATE) {
        aml_cfgfile_store_chipid(aml_hw, fp, chipid_buf);
        aml_cfgfile_store_randmac(aml_hw, fp);
    }
    aml_cfgfile_close(fp);

    ret = aml_cfgfile_retrieve(path, fbuf, AML_CFGFILE_FBUF_MAXLEN);
    if (ret >= AML_CFGFILE_FBUF_MAXLEN) {
        AML_INFO("retrieve file data error\n");
        return -1;
    }

    /* update chip_id */
    tag_ptr = aml_cfgfile_find_tag(fbuf, strlen(fbuf),
            "CHIP_ID=", AML_CFGFILE_CHIPID_LEN);
    if (tag_ptr && strncmp(tag_ptr, chipid_buf, AML_CFGFILE_CHIPID_LEN)) {
        memcpy((char *)tag_ptr, chipid_buf, AML_CFGFILE_CHIPID_LEN);
        fp = aml_cfgfile_open(path, O_RDWR, 0666);
        if (!fp) {
            return AML_CFGFILE_ERROR;
        }
        aml_cfgfile_write(fp, fbuf, strlen(fbuf));
        aml_cfgfile_close(fp);
    }

    /* Get vif0 mac address */
    tag_ptr = aml_cfgfile_find_tag(fbuf, strlen(fbuf),
            "VIF0_MACADDR=", strlen("00:00:00:00:00:00"));
    if (!tag_ptr || sscanf((const char *)tag_ptr,
                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", cfg->vif0_mac + 0,
                cfg->vif0_mac + 1, cfg->vif0_mac + 2, cfg->vif0_mac + 3,
                cfg->vif0_mac + 4, cfg->vif0_mac + 5) != ETH_ALEN) {
        memcpy(cfg->vif0_mac, vif0_dft, ETH_ALEN);
    }

    /* Get vif1 mac address */
    tag_ptr = aml_cfgfile_find_tag(fbuf, strlen(fbuf),
            "VIF1_MACADDR=", strlen("00:00:00:00:00:00"));
    if (!tag_ptr || sscanf((const char *)tag_ptr,
                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", cfg->vif1_mac + 0,
                cfg->vif1_mac + 1, cfg->vif1_mac + 2, cfg->vif1_mac + 3,
                cfg->vif1_mac + 4, cfg->vif1_mac + 5) != ETH_ALEN) {
        memcpy(cfg->vif1_mac, vif1_dft, ETH_ALEN);
    }
    return 0;
}

int aml_cfgfile_parse_phy(struct aml_hw *aml_hw, const char *filename,
        struct aml_cfgfile_phy *cfg, int path)
{
    const struct firmware *cfg_fw = NULL;
    int ret;
    const u8 *tag_ptr;

    AML_DBG(AML_FN_ENTRY_STR);

    if ((ret = request_firmware(&cfg_fw, filename, aml_hw->dev))) {
        printk(KERN_CRIT "%s: Failed to get %s (%d)\n", __func__, filename, ret);
        return ret;
    }

    /* Get Trident path mapping */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "TRD_PATH_MAPPING=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            cfg->trd.path_mapping = val;
        else
            cfg->trd.path_mapping = path;
    } else
        cfg->trd.path_mapping = path;

    AML_DBG("Trident path mapping is: %d\n", cfg->trd.path_mapping);

    /* Get DC offset compensation */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "TX_DC_OFF_COMP=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->trd.tx_dc_off_comp) != 1)
            cfg->trd.tx_dc_off_comp = 0;
    } else
        cfg->trd.tx_dc_off_comp = 0;

    AML_DBG("TX DC offset compensation is: %08X\n", cfg->trd.tx_dc_off_comp);

    /* Get Karst TX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_2_4G[0]) != 1)
            cfg->karst.tx_iq_comp_2_4G[0] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_2_4G[0] = 0x01000000;

    AML_DBG("Karst TX IQ compensation for path 0 on 2.4GHz is: %08X\n", cfg->karst.tx_iq_comp_2_4G[0]);

    /* Get Karst TX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_2_4G[1]) != 1)
            cfg->karst.tx_iq_comp_2_4G[1] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_2_4G[1] = 0x01000000;

    AML_DBG("Karst TX IQ compensation for path 1 on 2.4GHz is: %08X\n", cfg->karst.tx_iq_comp_2_4G[1]);

    /* Get Karst RX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_2_4G[0]) != 1)
            cfg->karst.rx_iq_comp_2_4G[0] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_2_4G[0] = 0x01000000;

    AML_DBG("Karst RX IQ compensation for path 0 on 2.4GHz is: %08X\n", cfg->karst.rx_iq_comp_2_4G[0]);

    /* Get Karst RX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_2_4G[1]) != 1)
            cfg->karst.rx_iq_comp_2_4G[1] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_2_4G[1] = 0x01000000;

    AML_DBG("Karst RX IQ compensation for path 1 on 2.4GHz is: %08X\n", cfg->karst.rx_iq_comp_2_4G[1]);

    /* Get Karst TX IQ compensation value for path0 on 5GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_5G[0]) != 1)
            cfg->karst.tx_iq_comp_5G[0] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_5G[0] = 0x01000000;

    AML_DBG("Karst TX IQ compensation for path 0 on 5GHz is: %08X\n", cfg->karst.tx_iq_comp_5G[0]);

    /* Get Karst TX IQ compensation value for path1 on 5GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_5G[1]) != 1)
            cfg->karst.tx_iq_comp_5G[1] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_5G[1] = 0x01000000;

    AML_DBG("Karst TX IQ compensation for path 1 on 5GHz is: %08X\n", cfg->karst.tx_iq_comp_5G[1]);

    /* Get Karst RX IQ compensation value for path0 on 5GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_5G[0]) != 1)
            cfg->karst.rx_iq_comp_5G[0] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_5G[0] = 0x01000000;

    AML_DBG("Karst RX IQ compensation for path 0 on 5GHz is: %08X\n", cfg->karst.rx_iq_comp_5G[0]);

    /* Get Karst RX IQ compensation value for path1 on 5GHz */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_5G[1]) != 1)
            cfg->karst.rx_iq_comp_5G[1] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_5G[1] = 0x01000000;

    AML_DBG("Karst RX IQ compensation for path 1 on 5GHz is: %08X\n", cfg->karst.rx_iq_comp_5G[1]);

    /* Get Karst default path */
    tag_ptr = aml_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_DEFAULT_PATH=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            cfg->karst.path_used = val;
        else
            cfg->karst.path_used = path;
    } else
        cfg->karst.path_used = path;

    AML_DBG("Karst default path is: %d\n", cfg->karst.path_used);

    /* Release the configuration file */
    release_firmware(cfg_fw);

    return 0;
}

#define AML_CFGFILE_RPS_CPUS(is_sta)  do { \
    char path[128]; \
    struct file *fp; \
    int i;\
    for (i = 0; i < 4; i++) { \
        sprintf(path, "/sys/class/net/%s/queues/rx-%d/rps_cpus", \
                (is_sta == 1) ? "wlan0": "ap0", i); \
        fp = aml_cfgfile_open(path, O_RDWR, 0666); \
        if (!fp) return; \
        aml_cfgfile_write(fp, "f", 1); \
        aml_cfgfile_close(fp); \
    } \
} while (0);

#define AML_CFGFILE_RPS_FLOW(is_sta)  do { \
    char path[128]; \
    struct file *fp; \
    int i;\
    for (i = 0; i < 4; i++) { \
        sprintf(path, "/sys/class/net/%s/queues/rx-%d/rps_flow_cnt", \
                (is_sta == 1) ? "wlan0": "ap0", i); \
        fp = aml_cfgfile_open(path, O_RDWR, 0666); \
        if (!fp) return; \
        aml_cfgfile_write(fp, "4096", strlen("4096")); \
        aml_cfgfile_close(fp); \
    } \
} while (0);

#define AML_CFGFILE_RPS_SOCK()  do { \
    char path[128]; \
    struct file *fp; \
    sprintf(path, "/proc/sys/net/core/rps_sock_flow_entries"); \
    fp = aml_cfgfile_open(path, O_RDWR, 0666); \
    if (!fp) return; \
    aml_cfgfile_write(fp, "16384", strlen("16384")); \
    aml_cfgfile_close(fp); \
} while (0);

void aml_cfgfile_update_rps(void)
{
    /* wlan0 interface */
    AML_CFGFILE_RPS_CPUS(1);
    AML_CFGFILE_RPS_FLOW(1);

    /* ap0 interface */
    AML_CFGFILE_RPS_CPUS(0);
    AML_CFGFILE_RPS_FLOW(0);

    AML_CFGFILE_RPS_SOCK();
}
