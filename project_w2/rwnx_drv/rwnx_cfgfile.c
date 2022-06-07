/**
 ****************************************************************************************
 *
 * @file rwnx_configparse.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ****************************************************************************************
 */
#include <linux/firmware.h>
#include <linux/if_ether.h>

#include "rwnx_defs.h"
#include "rwnx_cfgfile.h"

/**
 *
 */

#define MAC_FMT "MAC_ADDR=%02x:%02x:%02x:%02x:%02x:%02x\n"
#define MAC_ARG(x) ((unsigned char*)(x))[0],((unsigned char*)(x))[1],\
                    ((unsigned char*)(x))[2],((unsigned char*)(x))[3],\
                    ((unsigned char*)(x))[4],((unsigned char*)(x))[5]

static int openFile(struct file **fpp, const char *path, int flag, int mode)
{
    struct file *fp;

    fp = filp_open(path, flag, mode);
    if (IS_ERR(fp)) {
        *fpp = NULL;
        return PTR_ERR(fp);
    } else {
        *fpp = fp;
        return 0;
    }
}

/*
* Close the file with the specific @param fp
* @param fp the pointer of struct file to close
* @return always 0
*/
static int closeFile(struct file *fp)
{
    filp_close(fp, NULL);
    return 0;
}

static int readFile(struct file *fp, char *buf, int len)
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

static int writeFile(struct file *fp, char *buf, int len)
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
        if (wlen > 0) {
            sum += wlen;
        } else if (0 != wlen) {
            return wlen;
        } else {
            break;
        }
    }
    return sum;
}

static int retriveFromFile(const char *path, u8 *buf, u32 sz)
{
    int ret = -1;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    mm_segment_t oldfs;
#endif
    struct file *fp;

    if (path && buf) {
        ret = openFile(&fp, path, O_RDONLY, 0);
        if (0 == ret) {
            printk("openFile path:%s fp=%p\n", path , fp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            oldfs = get_fs();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
            set_fs(KERNEL_DS);
#else
            set_fs(get_ds());
#endif
#endif // 5.15
            ret = readFile(fp, buf, sz);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            set_fs(oldfs);
#endif
            closeFile(fp);

            printk("readFile, ret:%d\n", ret);
        } else {
            ERROR_DEBUG_OUT("openFile path:%s Fail, ret:%d\n", path, ret);
        }
    } else {
        ERROR_DEBUG_OUT("NULL pointer\n");
        ret = -EINVAL;
    }
    return ret;
}

/*
* Open the file with @param path and wirte @param sz byte of data starting from @param buf into the file
* @param path the path of the file to open and write
* @param buf the starting address of the data to write into file
* @param sz how many bytes to write at most
* @return the byte we've written, or Linux specific error code
*/
static int storeToFile(const char *path, u8 *buf, u32 sz)
{
    int ret = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    mm_segment_t oldfs;
#endif
    struct file *fp;

    if (path && buf) {
        ret = openFile(&fp, path, O_CREAT | O_WRONLY, 0666);
        if (0 == ret) {
            printk("openFile path:%s fp=%p\n", path , fp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            oldfs = get_fs();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0))
            set_fs(KERNEL_DS);
#else
            set_fs(get_ds());
#endif
#endif//5.15
            ret = writeFile(fp, buf, sz);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
            set_fs(oldfs);
#endif
            closeFile(fp);
            printk("writeFile, ret:%d\n", ret);
        } else {
            ERROR_DEBUG_OUT("openFile path:%s Fail, ret:%d\n", path, ret);
        }
    } else {
        ERROR_DEBUG_OUT("NULL pointer\n");
        ret =  -EINVAL;
    }
    return ret;
}

/*
* Open the file with @param path and retrive the file content into memory starting from @param buf for @param sz at most
* @param path the path of the file to open and read
* @param buf the starting address of the buffer to store file content
* @param sz how many bytes to read at most
* @return the byte we've read
*/
int aml_retrieve_from_file(const char *path, u8 *buf, u32 sz)
{
    int ret = retriveFromFile(path, buf, sz);
    return ret >= 0 ? ret : 0;
}

/*
* Open the file with @param path and wirte @param sz byte of data starting from @param buf into the file
* @param path the path of the file to open and write
* @param buf the starting address of the data to write into file
* @param sz how many bytes to write at most
* @return the byte we've written
*/
int aml_store_to_file(const char *path, u8 *buf, u32 sz)
{
    int ret = storeToFile(path, buf, sz);
    return ret >= 0 ? ret : 0;
}

/**
* IsHexDigit -
*
* Return TRUE if chTmp is represent for hex digit
* FALSE otherwise.
*/
bool aml_char_is_hex_digit(char chTmp)
{
    if ((chTmp >= '0' && chTmp <= '9') ||
        (chTmp >= 'a' && chTmp <= 'f') ||
        (chTmp >= 'A' && chTmp <= 'F')) {
        return true;
    } else {
        return false;
    }
}

u32 aml_read_macaddr_from_file(const char *path, u8 *buf)
{
    u32 i;
    u8 temp[3];
    u32 ret = false;

    u8 file_data[FILE_DATA_LEN];
    u32 read_size;
    u8 addr[ETH_ALEN];

    read_size = aml_retrieve_from_file(path, file_data, FILE_DATA_LEN);
    if (read_size != FILE_DATA_LEN) {
        printk("%s read from %s fail\n", __func__, path);
        goto exit;
    }

    temp[2] = 0; /* end of string '\0' */

    for (i = 0 ; i < ETH_ALEN ; i++) {
        if (aml_char_is_hex_digit(file_data[PARSE_DIGIT_BASE + i * 3]) == false
            || aml_char_is_hex_digit(file_data[PARSE_DIGIT_BASE + i * 3 + 1]) == false) {
            printk("%s invalid 8-bit hex format for address offset:%u\n", __func__, i);
            goto exit;
        }

        if (i < ETH_ALEN - 1 && file_data[PARSE_DIGIT_BASE + i * 3 + 2] != ':') {
            printk("%s invalid separator after address offset:%u\n", __func__, i);
            goto exit;
        }

        temp[0] = file_data[PARSE_DIGIT_BASE + i * 3];
        temp[1] = file_data[PARSE_DIGIT_BASE + i * 3 + 1];
        if (sscanf(temp, "%hhx", &addr[i]) != 1) {
            printk("%s sscanf fail for address offset:0x%03x\n", __func__, i);
            goto exit;
        }
    }
    memset(buf, '\0', ETH_ALEN);
    memcpy(buf, addr, ETH_ALEN);

    ret = true;

exit:
    return ret;
}

static const u8 *rwnx_find_tag(const u8 *file_data, unsigned int file_size,
                                 const char *tag_name, unsigned int tag_len)
{
    unsigned int curr, line_start = 0, line_size;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* Walk through all the lines of the configuration file */
    while (line_start < file_size) {
        /* Search the end of the current line (or the end of the file) */
        for (curr = line_start; curr < file_size; curr++)
            if (file_data[curr] == '\n')
                break;

        /* Compute the line size */
        line_size = curr - line_start;

        /* Check if this line contains the expected tag */
        if ((line_size == (strlen(tag_name) + tag_len)) &&
            (!strncmp(&file_data[line_start], tag_name, strlen(tag_name))))
            return (&file_data[line_start + strlen(tag_name)]);

        /* Move to next line */
        line_start = curr + 1;
    }

    /* Tag not found */
    return NULL;
}

#if 0
/**
 * Parse the Config file used at init time
 */
int rwnx_parse_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                          struct rwnx_conf_file *config)
{
    const struct firmware *config_fw = NULL;
    u8 dflt_mac[ETH_ALEN] = { 0, 111, 111, 111, 111, 0 };
    int ret;
    const u8 *tag_ptr;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if ((ret = request_firmware(&config_fw, filename, rwnx_hw->dev))) {
        printk(KERN_CRIT "%s: Failed to get %s (%d)\n", __func__, filename, ret);
        return ret;
    }

    /* Get MAC Address */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "MAC_ADDR=", strlen("00:00:00:00:00:00"));
    if (tag_ptr != NULL) {
        u8 *addr = config->mac_addr;
        if (sscanf((const char *)tag_ptr,
                   "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   addr + 0, addr + 1, addr + 2,
                   addr + 3, addr + 4, addr + 5) != ETH_ALEN)
            memcpy(config->mac_addr, dflt_mac, ETH_ALEN);
    } else
        memcpy(config->mac_addr, dflt_mac, ETH_ALEN);

    RWNX_DBG("MAC Address is:\n%pM\n", config->mac_addr);

    /* Release the configuration file */
    release_firmware(config_fw);

    return 0;
}
#endif

/**
 * Parse the Config file WiFi mac addr used at init time
 */
int rwnx_parse_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                          struct rwnx_conf_file *config)

{
    u8 mac_addr[ETH_ALEN] = { 0x1c, 0xa4, 0x10, 0x58, 0x00, 0xcc };
    u64 timestamp;
    u8 cbuf[50];

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (aml_read_macaddr_from_file(RWNX_WIFI_MAC_ADDR_PATH, mac_addr) == true) {
        printk("%s:%d "MAC_FMT"\n", __func__, __LINE__, MAC_ARG(mac_addr));
    } else {
        /*
            todo: read efuse create mac addr.
        */
        timestamp = get_jiffies_64();
        mac_addr[3] = (timestamp & 0xff);
        mac_addr[4] = ((timestamp >> 2) & 0xff);
        mac_addr[5] = ((timestamp >> 4) & 0xff);
        printk("%s:%d "MAC_FMT"\n", __func__, __LINE__, MAC_ARG(mac_addr));

        aml_retrieve_from_file(RWNX_WIFI_MAC_ADDR_PATH, cbuf, 48);
        sprintf(cbuf, MAC_FMT, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        if (aml_store_to_file(RWNX_WIFI_MAC_ADDR_PATH, cbuf, strlen(cbuf)) > 0) {
            printk("write the random mac to wifimac.txt\n");
        }
    }

    if (mac_addr[0] & 0x3) {
        printk("change the mac addr from [0x%x] ", mac_addr[0]);
        mac_addr[0] &= ~0x3;
        printk("to [0x%x] \n", mac_addr[0]);

        aml_retrieve_from_file(RWNX_WIFI_MAC_ADDR_PATH, cbuf, 48);
        sprintf(cbuf, MAC_FMT, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        if (aml_store_to_file(RWNX_WIFI_MAC_ADDR_PATH, cbuf, strlen(cbuf)) > 0) {
            printk("write the random mac to wifimac.txt\n");
        }
    }
    memcpy(config->mac_addr, mac_addr, ETH_ALEN);

    RWNX_DBG("MAC Address is:\n%pM\n", config->mac_addr);

    return 0;
}

/**
 * Parse the Config file used at init time
 */
int rwnx_parse_phy_configfile(struct rwnx_hw *rwnx_hw, const char *filename,
                              struct rwnx_phy_conf_file *config, int path)
{
    const struct firmware *config_fw = NULL;
    int ret;
    const u8 *tag_ptr;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if ((ret = request_firmware(&config_fw, filename, rwnx_hw->dev))) {
        printk(KERN_CRIT "%s: Failed to get %s (%d)\n", __func__, filename, ret);
        return ret;
    }

    /* Get Trident path mapping */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "TRD_PATH_MAPPING=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            config->trd.path_mapping = val;
        else
            config->trd.path_mapping = path;
    } else
        config->trd.path_mapping = path;

    RWNX_DBG("Trident path mapping is: %d\n", config->trd.path_mapping);

    /* Get DC offset compensation */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "TX_DC_OFF_COMP=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->trd.tx_dc_off_comp) != 1)
            config->trd.tx_dc_off_comp = 0;
    } else
        config->trd.tx_dc_off_comp = 0;

    RWNX_DBG("TX DC offset compensation is: %08X\n", config->trd.tx_dc_off_comp);

    /* Get Karst TX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.tx_iq_comp_2_4G[0]) != 1)
            config->karst.tx_iq_comp_2_4G[0] = 0x01000000;
    } else
        config->karst.tx_iq_comp_2_4G[0] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 0 on 2.4GHz is: %08X\n", config->karst.tx_iq_comp_2_4G[0]);

    /* Get Karst TX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_TX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.tx_iq_comp_2_4G[1]) != 1)
            config->karst.tx_iq_comp_2_4G[1] = 0x01000000;
    } else
        config->karst.tx_iq_comp_2_4G[1] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 1 on 2.4GHz is: %08X\n", config->karst.tx_iq_comp_2_4G[1]);

    /* Get Karst RX IQ compensation value for path0 on 2.4GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.rx_iq_comp_2_4G[0]) != 1)
            config->karst.rx_iq_comp_2_4G[0] = 0x01000000;
    } else
        config->karst.rx_iq_comp_2_4G[0] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 0 on 2.4GHz is: %08X\n", config->karst.rx_iq_comp_2_4G[0]);

    /* Get Karst RX IQ compensation value for path1 on 2.4GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_RX_IQ_COMP_2_4G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.rx_iq_comp_2_4G[1]) != 1)
            config->karst.rx_iq_comp_2_4G[1] = 0x01000000;
    } else
        config->karst.rx_iq_comp_2_4G[1] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 1 on 2.4GHz is: %08X\n", config->karst.rx_iq_comp_2_4G[1]);

    /* Get Karst TX IQ compensation value for path0 on 5GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.tx_iq_comp_5G[0]) != 1)
            config->karst.tx_iq_comp_5G[0] = 0x01000000;
    } else
        config->karst.tx_iq_comp_5G[0] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 0 on 5GHz is: %08X\n", config->karst.tx_iq_comp_5G[0]);

    /* Get Karst TX IQ compensation value for path1 on 5GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_TX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.tx_iq_comp_5G[1]) != 1)
            config->karst.tx_iq_comp_5G[1] = 0x01000000;
    } else
        config->karst.tx_iq_comp_5G[1] = 0x01000000;

    RWNX_DBG("Karst TX IQ compensation for path 1 on 5GHz is: %08X\n", config->karst.tx_iq_comp_5G[1]);

    /* Get Karst RX IQ compensation value for path0 on 5GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_0=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.rx_iq_comp_5G[0]) != 1)
            config->karst.rx_iq_comp_5G[0] = 0x01000000;
    } else
        config->karst.rx_iq_comp_5G[0] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 0 on 5GHz is: %08X\n", config->karst.rx_iq_comp_5G[0]);

    /* Get Karst RX IQ compensation value for path1 on 5GHz */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_RX_IQ_COMP_5G_PATH_1=", strlen("00000000"));
    if (tag_ptr != NULL) {
        if (sscanf((const char *)tag_ptr, "%08x", &config->karst.rx_iq_comp_5G[1]) != 1)
            config->karst.rx_iq_comp_5G[1] = 0x01000000;
    } else
        config->karst.rx_iq_comp_5G[1] = 0x01000000;

    RWNX_DBG("Karst RX IQ compensation for path 1 on 5GHz is: %08X\n", config->karst.rx_iq_comp_5G[1]);

    /* Get Karst default path */
    tag_ptr = rwnx_find_tag(config_fw->data, config_fw->size,
                            "KARST_DEFAULT_PATH=", strlen("00"));
    if (tag_ptr != NULL) {
        u8 val;
        if (sscanf((const char *)tag_ptr, "%hhx", &val) == 1)
            config->karst.path_used = val;
        else
            config->karst.path_used = path;
    } else
        config->karst.path_used = path;

    RWNX_DBG("Karst default path is: %d\n", config->karst.path_used);

    /* Release the configuration file */
    release_firmware(config_fw);

    return 0;
}

