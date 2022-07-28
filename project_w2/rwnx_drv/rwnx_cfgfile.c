/**
 ****************************************************************************************
 *
 * @file rwnx_cfgfile.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 * Copyright (C) Amlogic 2022
 *
 ****************************************************************************************
 */
#include <linux/firmware.h>
#include <linux/if_ether.h>

#include "rwnx_defs.h"
#include "rwnx_cfgfile.h"
#include "rwnx_msg_tx.h"
#include "rwnx_utils.h"

static struct file *rwnx_cfgfile_open(const char *path, int flag, int mode)
{
    struct file *fp;
    fp = filp_open(path, flag, mode);
    if (IS_ERR(fp)) {
        return NULL;
    }
    return fp;
}

static void rwnx_cfgfile_close(struct file *fp)
{
    filp_close(fp, NULL);
}

static int rwnx_cfgfile_read(struct file *fp, char *buf, int len)
{
    int rlen = 0, sum = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
    if (!(fp->f_mode & FMODE_CAN_READ)) {
#else
    if (!fp->f_op || !fp->f_op->read) {
#endif
        return -EPERM;
    }

    while (sum < len) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
        rlen = kernel_read(fp, buf + sum, len - sum, &fp->f_pos);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
        rlen = __vfs_read(fp, buf + sum, len - sum, &fp->f_pos);
#else
        rlen = fp->f_op->read(fp, buf + sum, len - sum, &fp->f_pos);
#endif
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

static int rwnx_cfgfile_write(struct file *fp, char *buf, int len)
{
    int wlen = 0, sum = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
    if (!(fp->f_mode & FMODE_CAN_WRITE)) {
#else
    if (!fp->f_op || !fp->f_op->write) {
#endif
        return -EPERM;
    }

    while (sum < len) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
        wlen = kernel_write(fp, buf + sum, len - sum, &fp->f_pos);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
        wlen = __vfs_write(fp, buf + sum, len - sum, &fp->f_pos);
#else
        wlen = fp->f_op->write(fp, buf + sum, len - sum, &fp->f_pos);
#endif
        if (wlen > 0)
            sum += wlen;
        else if (0 != wlen)
            return wlen;
        else
            break;
    }
    return sum;
}

static int rwnx_cfgfile_retrieve(const char *path, u8 *buf, u32 len)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    mm_segment_t oldfs;
#endif
    struct file *fp;
    int ret = -1;

    if (path && buf) {
        fp = rwnx_cfgfile_open(path, O_RDONLY, 0);
        if (fp) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            oldfs = get_fs();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
            set_fs(KERNEL_DS);
#else
            set_fs(get_ds());
#endif
#endif
            ret = rwnx_cfgfile_read(fp, buf, len);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            set_fs(oldfs);
#endif
            rwnx_cfgfile_close(fp);
        }
    } else {
        ret = -EINVAL;
    }
    return (ret >= 0 ? ret : 0);
}

static int rwnx_cfgfile_create(const char *path, struct file **fpp)
{
    struct file *fp;

    fp = rwnx_cfgfile_open(path, O_RDONLY, 0);
    if (!fp) {
        fp = rwnx_cfgfile_open(path, O_CREAT | O_RDWR, 0666);
        if (!fp) {
            return RWNX_CFGFILE_ERROR;
        }
        *fpp = fp;
        return RWNX_CFGFILE_CREATE;
    }
    *fpp = fp;
    return RWNX_CFGFILE_EXIST;
}

static const u8 *rwnx_cfgfile_find_tag(const u8 *file_data, unsigned int file_size,
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

static void rwnx_cfgfile_store_tag(struct file *fp, const char *tag_name, const char *tag_value)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    mm_segment_t oldfs;
#endif
    unsigned char lbuf[RWNX_CFGFILE_LBUF_MAXLEN];
    unsigned char fbuf[RWNX_CFGFILE_FBUF_MAXLEN];

    if (!fp || !tag_name || !tag_value)
        return;

    if (strlen(tag_name) + strlen(tag_value) > RWNX_CFGFILE_LBUF_MAXLEN) {
        RWNX_INFO("store tag name exceed max line len(%d)", RWNX_CFGFILE_LBUF_MAXLEN);
        return;
    }

    sprintf(lbuf, "%s=%s\n", tag_name, tag_value);
    RWNX_INFO("store %s=%s(len=%d) tag to file\n", tag_name, tag_value, strlen(lbuf));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    oldfs = get_fs();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
    set_fs(KERNEL_DS);
#else
    set_fs(get_ds());
#endif
#endif
    rwnx_cfgfile_read(fp, fbuf, RWNX_CFGFILE_FBUF_MAXLEN);
    if (strlen(fbuf) + strlen(lbuf) > RWNX_CFGFILE_FBUF_MAXLEN) {
        RWNX_INFO("store tag name exceed max file len(%d)", RWNX_CFGFILE_FBUF_MAXLEN);
        goto out;
    }
    sprintf(fbuf, "%s", lbuf);
    rwnx_cfgfile_write(fp, fbuf, strlen(fbuf));

out:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    set_fs(oldfs);
#endif
    return;
}

static void rwnx_get_chip_id_from_efuse(struct rwnx_hw *rwnx_hw, char *chipid_buf)
{
    unsigned int chipid_low = 0;
    unsigned int chipid_high = 0;

    chipid_low = aml_efuse_read(rwnx_hw, RWNX_CFGFILE_CHIPID_LOW);
    chipid_high = aml_efuse_read(rwnx_hw, RWNX_CFGFILE_CHIPID_HIGH);
    sprintf(chipid_buf, "%04x%08x", chipid_high & 0xffff, chipid_low);
}

static void rwnx_cfgfile_store_chipid(struct rwnx_hw *rwnx_hw, struct file *fp, char *chipid_buf)
{
    rwnx_cfgfile_store_tag(fp, "CHIP_ID", chipid_buf);
}

static void rwnx_cfgfile_store_randmac(struct file *fp)
{
#define RWNX_CFGFILE_TOGGLE_BIT(b, n) (b ^= (1 << n))
    u8 vif0_mac[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x00, 0x00, 0x00 };
    u8 vif1_mac[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x00, 0x00, 0x00 };
    char mac_str[strlen("00:00:00:00:00:00") + 1];
    u64 rand_num = 0;

    get_random_bytes(&rand_num, sizeof(u64));

    vif0_mac[3] = (rand_num & 0xff);
    vif0_mac[4] = ((rand_num >> 8) & 0xff);
    vif0_mac[5] = ((rand_num >> 16) & 0xff);
    sprintf(mac_str, MACFMT, MACARG(vif0_mac));
    rwnx_cfgfile_store_tag(fp, "VIF0_MACADDR", mac_str);


    memcpy(vif1_mac, vif0_mac, ETH_ALEN);
    RWNX_CFGFILE_TOGGLE_BIT(vif1_mac[5], 0);
    RWNX_CFGFILE_TOGGLE_BIT(vif1_mac[5], 1);
    sprintf(mac_str, MACFMT, MACARG(vif1_mac));
    rwnx_cfgfile_store_tag(fp, "VIF1_MACADDR", mac_str);
}

int rwnx_cfgfile_parse(struct rwnx_hw *rwnx_hw, struct rwnx_cfgfile *cfg)
{
    const char *path = RWNX_CFGFILE_DEFAULT_PATH;
    struct file *fp = NULL;
    u8 vif0_dft[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x11, 0x22, 0x33 };
    u8 vif1_dft[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x11, 0x22, 0x34 };
    u8 fbuf[RWNX_CFGFILE_FBUF_MAXLEN] = {0};
    const u8 *tag_ptr;
    int ret = -1;
    char chipid_buf[RWNX_CFGFILE_LBUF_MAXLEN];

    ret = rwnx_cfgfile_create(path, &fp);
    if (ret == RWNX_CFGFILE_ERROR) {
        RWNX_INFO("create config file failed\n");
        return -1;
    }

    rwnx_get_chip_id_from_efuse(rwnx_hw, chipid_buf);
    if (ret == RWNX_CFGFILE_CREATE) {
        rwnx_cfgfile_store_chipid(rwnx_hw, fp, chipid_buf);
        rwnx_cfgfile_store_randmac(fp);
    }
    rwnx_cfgfile_close(fp);

    ret = rwnx_cfgfile_retrieve(path, fbuf, RWNX_CFGFILE_FBUF_MAXLEN);
    if (ret >= RWNX_CFGFILE_FBUF_MAXLEN) {
        RWNX_INFO("retrieve file data error\n");
        return -1;
    }

    /* update chip_id */
    tag_ptr = rwnx_cfgfile_find_tag(fbuf, strlen(fbuf),
            "CHIP_ID=", RWNX_CFGFILE_CHIPID_LEN);
    if (tag_ptr && strncmp(tag_ptr, chipid_buf, RWNX_CFGFILE_CHIPID_LEN)) {
        memcpy((char *)tag_ptr, chipid_buf, RWNX_CFGFILE_CHIPID_LEN);
        fp = rwnx_cfgfile_open(path, O_RDWR, 0666);
        if (!fp) {
            return RWNX_CFGFILE_ERROR;
        }
        rwnx_cfgfile_write(fp, fbuf, strlen(fbuf));
        rwnx_cfgfile_close(fp);
    }

    /* Get vif0 mac address */
    tag_ptr = rwnx_cfgfile_find_tag(fbuf, strlen(fbuf),
            "VIF0_MACADDR=", strlen("00:00:00:00:00:00"));
    if (!tag_ptr || sscanf((const char *)tag_ptr,
                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", cfg->vif0_mac + 0,
                cfg->vif0_mac + 1, cfg->vif0_mac + 2, cfg->vif0_mac + 3,
                cfg->vif0_mac + 4, cfg->vif0_mac + 5) != ETH_ALEN) {
        memcpy(cfg->vif0_mac, vif0_dft, ETH_ALEN);
    }

    /* Get vif1 mac address */
    tag_ptr = rwnx_cfgfile_find_tag(fbuf, strlen(fbuf),
            "VIF1_MACADDR=", strlen("00:00:00:00:00:00"));
    if (!tag_ptr || sscanf((const char *)tag_ptr,
                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", cfg->vif1_mac + 0,
                cfg->vif1_mac + 1, cfg->vif1_mac + 2, cfg->vif1_mac + 3,
                cfg->vif1_mac + 4, cfg->vif1_mac + 5) != ETH_ALEN) {
        memcpy(cfg->vif1_mac, vif1_dft, ETH_ALEN);
    }
    return 0;
}

int rwnx_cfgfile_parse_phy(struct rwnx_hw *rwnx_hw, const char *filename,
        struct rwnx_cfgfile_phy *cfg, int path)
{
    const struct firmware *cfg_fw = NULL;
    int ret;
    const u8 *tag_ptr;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if ((ret = request_firmware(&cfg_fw, filename, rwnx_hw->dev))) {
        printk(KERN_CRIT "%s: Failed to get %s (%d)\n", __func__, filename, ret);
        return ret;
    }

    /* Get Trident path mapping */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "TRD_PATH_MAPPING=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            cfg->trd.path_mapping = val;
        else
            cfg->trd.path_mapping = path;
    } else
        cfg->trd.path_mapping = path;

    RWNX_DBG("Trident path mapping is: %d\n", cfg->trd.path_mapping);

    /* Get DC offset compensation */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "TX_DC_OFF_COMP=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->trd.tx_dc_off_comp) != 1)
            cfg->trd.tx_dc_off_comp = 0;
    } else
        cfg->trd.tx_dc_off_comp = 0;

    RWNX_DBG("TX DC offset compensation is: %08X\n", cfg->trd.tx_dc_off_comp);

    /* Get Karst TX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_2_4G[0]) != 1)
            cfg->karst.tx_iq_comp_2_4G[0] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_2_4G[0] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 0 on 2.4GHz is: %08X\n", cfg->karst.tx_iq_comp_2_4G[0]);

    /* Get Karst TX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_2_4G[1]) != 1)
            cfg->karst.tx_iq_comp_2_4G[1] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_2_4G[1] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 1 on 2.4GHz is: %08X\n", cfg->karst.tx_iq_comp_2_4G[1]);

    /* Get Karst RX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_2_4G[0]) != 1)
            cfg->karst.rx_iq_comp_2_4G[0] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_2_4G[0] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 0 on 2.4GHz is: %08X\n", cfg->karst.rx_iq_comp_2_4G[0]);

    /* Get Karst RX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_2_4G[1]) != 1)
            cfg->karst.rx_iq_comp_2_4G[1] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_2_4G[1] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 1 on 2.4GHz is: %08X\n", cfg->karst.rx_iq_comp_2_4G[1]);

    /* Get Karst TX IQ compensation value for path0 on 5GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_5G[0]) != 1)
            cfg->karst.tx_iq_comp_5G[0] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_5G[0] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 0 on 5GHz is: %08X\n", cfg->karst.tx_iq_comp_5G[0]);

    /* Get Karst TX IQ compensation value for path1 on 5GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.tx_iq_comp_5G[1]) != 1)
            cfg->karst.tx_iq_comp_5G[1] = 0x01000000;
    } else
        cfg->karst.tx_iq_comp_5G[1] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 1 on 5GHz is: %08X\n", cfg->karst.tx_iq_comp_5G[1]);

    /* Get Karst RX IQ compensation value for path0 on 5GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_5G[0]) != 1)
            cfg->karst.rx_iq_comp_5G[0] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_5G[0] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 0 on 5GHz is: %08X\n", cfg->karst.rx_iq_comp_5G[0]);

    /* Get Karst RX IQ compensation value for path1 on 5GHz */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &cfg->karst.rx_iq_comp_5G[1]) != 1)
            cfg->karst.rx_iq_comp_5G[1] = 0x01000000;
    } else
        cfg->karst.rx_iq_comp_5G[1] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 1 on 5GHz is: %08X\n", cfg->karst.rx_iq_comp_5G[1]);

    /* Get Karst default path */
    tag_ptr = rwnx_cfgfile_find_tag(cfg_fw->data, cfg_fw->size,
                            "KARST_DEFAULT_PATH=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            cfg->karst.path_used = val;
        else
            cfg->karst.path_used = path;
    } else
        cfg->karst.path_used = path;

    RWNX_DBG("Karst default path is: %d\n", cfg->karst.path_used);

    /* Release the configuration file */
    release_firmware(cfg_fw);

    return 0;
}
