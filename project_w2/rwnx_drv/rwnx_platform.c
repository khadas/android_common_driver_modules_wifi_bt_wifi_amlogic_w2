/**
 ******************************************************************************
 *
 * @file rwnx_platform.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#include "rwnx_platform.h"
#include "reg_access.h"
#include "hal_desc.h"
#include "rwnx_main.h"
#include "rwnx_pci.h"
#ifndef CONFIG_RWNX_FHOST
#include "ipc_host.h"
#endif /* !CONFIG_RWNX_FHOST */

#include "chip_pmu_reg.h"
#include "rwnx_irqs.h"

extern unsigned char auc_driver_insmoded;
extern struct usb_device *g_udev;
extern struct auc_hif_ops g_auc_hif_ops;

extern struct rwnx_plat_pci *g_rwnx_plat_pci;
extern unsigned char g_pci_driver_insmoded;
extern unsigned char g_pci_after_probe;

struct pci_dev *g_pci_dev = NULL;

int wifi_fw_download(void);
int start_wifi(void);

#ifdef CONFIG_RWNX_TL4
/**
 * rwnx_plat_tl4_fw_upload() - Load the requested FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a hex file, into the specified address
 */
static int rwnx_plat_tl4_fw_upload(struct rwnx_plat *rwnx_plat, u8* fw_addr,
                                   char *filename)
{
    struct device *dev = rwnx_platform_get_dev(rwnx_plat);
    const struct firmware *fw;
    int err = 0;
    u32 *dst;
    u8 const *file_data;
    char typ0, typ1;
    u32 addr0, addr1;
    u32 dat0, dat1;
    int remain;

    err = request_firmware(&fw, filename, dev);
    if (err) {
        return err;
    }
    file_data = fw->data;
    remain = fw->size;

    /* Copy the file on the Embedded side */
    dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

    /* Walk through all the lines of the configuration file */
    while (remain >= 16) {
        u32 data, offset;

        if (sscanf(file_data, "%c:%08X %04X", &typ0, &addr0, &dat0) != 3)
            break;
        if ((addr0 & 0x01) != 0) {
            addr0 = addr0 - 1;
            dat0 = 0;
        } else {
            file_data += 16;
            remain -= 16;
        }
        if ((remain < 16) ||
            (sscanf(file_data, "%c:%08X %04X", &typ1, &addr1, &dat1) != 3) ||
            (typ1 != typ0) || (addr1 != (addr0 + 1))) {
            typ1 = typ0;
            addr1 = addr0 + 1;
            dat1 = 0;
        } else {
            file_data += 16;
            remain -= 16;
        }

        if (typ0 == 'C') {
            offset = 0x00200000;
            if ((addr1 % 4) == 3)
                offset += 2*(addr1 - 3);
            else
                offset += 2*(addr1 + 1);

            data = dat1 | (dat0 << 16);
        } else {
            offset = 2*(addr1 - 1);
            data = dat0 | (dat1 << 16);
        }
        dst = (u32 *)(fw_addr + offset);
        *dst = data;
    }

    release_firmware(fw);

    return err;
}
#endif

/**
 * rwnx_plat_bin_fw_upload() - Load the requested binary FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a binary file, into the specified address
 */
static int rwnx_plat_bin_fw_upload(struct rwnx_plat *rwnx_plat, u8* fw_addr,
                               char *filename)
{
    const struct firmware *fw = NULL;
    struct device *dev = rwnx_platform_get_dev(rwnx_plat);
    int err = 0;
    unsigned int i, size;
    u32 *src, *dst;

    printk("%s:%d\n", __func__, __LINE__);
    err = request_firmware(&fw, filename, dev);
    if (err) {
        return err;
    }

    /* Copy the file on the Embedded side */
    dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

    src = (u32 *)fw->data;
    dst = (u32 *)fw_addr;
    size = (unsigned int)fw->size;

    printk("%s:%d, size %d\n", __func__, __LINE__, size);
    printk("%s:%d, src %x\n", __func__, __LINE__, *src);
    /* check potential platform bug on multiple stores vs memcpy */
    for (i = 0; i < size; i += 4) {
        *dst++ = *src++;
    }

    release_firmware(fw);

    return err;
}

//sj
#define ICCM_ROM_LEN (256 * 1024)
#define ICCM_RAM_LEN (256 * 1024)
#define ICCM_ALL_LEN (ICCM_ROM_LEN + ICCM_RAM_LEN)
#define DCCM_ALL_LEN (160 * 1024)
#define ICCM_ROM_ADDR (0x00100000)
#define ICCM_RAM_ADDR (0x00100000 + ICCM_ROM_LEN)
#define DCCM_RAM_ADDR (0x00d00000)
#define DCCM_RAM_OFFSET (0x00700000) //0x00800000 - 0x00100000, in fw_flash
#define BYTE_IN_LINE (9)

#define RAM_BIN_LEN (1024 * 512 * 2)


#define IHEX_READ32(_val) {                                  \
        hex_buff[8] = 0;                                     \
        strncpy(hex_buff, (char *)src, 8);                   \
        if (kstrtouint(hex_buff, 16, &_val)) {               \
            printk("%s:%d, goto end\n", __func__, __LINE__); \
            goto end;                                        \
        }                                                    \
        src += BYTE_IN_LINE;                                 \
    }

static int rwnx_plat_fw_upload(struct rwnx_plat *rwnx_plat, u8* fw_addr,
                               char *filename)
{
    const struct firmware *fw = NULL;
    struct device *dev = rwnx_platform_get_dev(rwnx_plat);
    int err = 0;
    unsigned int i, size;
    u32 *dst;
    char hex_buff[9];
    u8 const *src;
    u32 data = 0;

    printk("%s:%d, \n", __func__, __LINE__);
    err = request_firmware(&fw, filename, dev);
    if (err) {
        return err;
    }

    src = (u8 *)fw->data;
    if (fw->size < RAM_BIN_LEN) {
        dst = (u32 *)(fw_addr + ICCM_ROM_LEN);
        size = ICCM_RAM_LEN;

    } else {
        dst = (u32 *)fw_addr;
        /* download iccm rom and ram */
        size = ICCM_ALL_LEN;
    }

    printk("%s:%d iccm dst %x\n", __func__, __LINE__, dst);
    printk("%s:%d iccm len %d\n", __func__, __LINE__, size/1024);
    for (i = 1; i <= size / 4; i += 1) {
        IHEX_READ32(data);
        *dst++ = __swab32(data);
    }

    /* download dccm */
    src = (u8 *)(fw->data) + (size / 4) * BYTE_IN_LINE;
    size = DCCM_ALL_LEN;
    dst = (u32 *)RWNX_ADDR(rwnx_plat, RWNX_ADDR_AON, DCCM_RAM_ADDR);//new pcie need use bar2

    printk("%s:%d dccm dst %x, size %d\n", __func__, __LINE__, dst, size/1024);
    for (i = 1; i <= size / 4; i += 1) {
        IHEX_READ32(data);
        *dst++ = __swab32(data);
    }

#if 0
    dst = (u32 *)fw_addr;
    for (i = 1; i < 50; i++)
        printk("%s:%d iccm check addr %x data %x\n", __func__, __LINE__, dst, *dst++);

    dst = (u32 *)RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, DCCM_RAM_ADDR);
    for (i = 1; i < 50; i++)
        printk("%s:%d dccm check addr %x data %x\n", __func__, __LINE__, dst, *dst++);
#endif

end:
#undef IHEX_READ32
    release_firmware(fw);
    return err;
}
#ifndef CONFIG_RWNX_TL4
#define IHEX_REC_DATA           0
#define IHEX_REC_EOF            1
#define IHEX_REC_EXT_SEG_ADD    2
#define IHEX_REC_START_SEG_ADD  3
#define IHEX_REC_EXT_LIN_ADD    4
#define IHEX_REC_START_LIN_ADD  5

/**
 * rwnx_plat_ihex_fw_upload() - Load the requested intel hex 8 FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a ihex file, into the specified address.
 */
 #if 0
static int rwnx_plat_ihex_fw_upload(struct rwnx_plat *rwnx_plat, u8* fw_addr,
                                    char *filename)
{
    const struct firmware *fw;
    struct device *dev = rwnx_platform_get_dev(rwnx_plat);
    u8 const *src, *end;
    u32 *dst;
    u16 haddr, segaddr, addr;
    u32 hwaddr;
    u8 load_fw, byte_count, checksum, csum, rec_type;
    int err, rec_idx;
    char hex_buff[9];

    err = request_firmware(&fw, filename, dev);
    if (err) {
        return err;
    }

    /* Copy the file on the Embedded side */
    dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

    src = fw->data;
    end = src + (unsigned int)fw->size;
    haddr = 0;
    segaddr = 0;
    load_fw = 1;
    err = -EINVAL;
    rec_idx = 0;
    hwaddr = 0;

#define IHEX_READ8(_val, _cs) {                  \
        hex_buff[2] = 0;                         \
        strncpy(hex_buff, src, 2);               \
        if (kstrtou8(hex_buff, 16, &_val))       \
            goto end;                            \
        src += 2;                                \
        if (_cs)                                 \
            csum += _val;                        \
    }

#define IHEX_READ16(_val) {                        \
        hex_buff[4] = 0;                           \
        strncpy(hex_buff, src, 4);                 \
        if (kstrtou16(hex_buff, 16, &_val))        \
            goto end;                              \
        src += 4;                                  \
        csum += (_val & 0xff) + (_val >> 8);       \
    }

#define IHEX_READ32(_val) {                              \
        hex_buff[8] = 0;                                 \
        strncpy(hex_buff, src, 8);                       \
        if (kstrtouint(hex_buff, 16, &_val))             \
            goto end;                                    \
        src += 8;                                        \
        csum += (_val & 0xff) + ((_val >> 8) & 0xff) +   \
            ((_val >> 16) & 0xff) + (_val >> 24);        \
    }

#define IHEX_READ32_PAD(_val, _nb) {                    \
        memset(hex_buff, '0', 8);                       \
        hex_buff[8] = 0;                                \
        strncpy(hex_buff, src, (2 * _nb));              \
        if (kstrtouint(hex_buff, 16, &_val))            \
            goto end;                                   \
        src += (2 * _nb);                               \
        csum += (_val & 0xff) + ((_val >> 8) & 0xff) +  \
            ((_val >> 16) & 0xff) + (_val >> 24);       \
}

    /* loop until end of file is read*/
    while (load_fw) {
        rec_idx++;
        csum = 0;

        /* Find next colon start code */
        while (*src != ':') {
            src++;
            if ((src + 3) >= end) /* 3 = : + rec_len */
                goto end;
        }
        src++;

        /* Read record len */
        IHEX_READ8(byte_count, 1);
        if ((src + (byte_count * 2) + 8) >= end) /* 8 = rec_addr + rec_type + chksum */
            goto end;

        /* Read record addr */
        IHEX_READ16(addr);

        /* Read record type */
        IHEX_READ8(rec_type, 1);

        switch(rec_type) {
            case IHEX_REC_DATA:
            {
                /* Update destination address */
                dst = (u32 *) (fw_addr + hwaddr + addr);

                while (byte_count) {
                    u32 val;
                    if (byte_count >= 4) {
                        IHEX_READ32(val);
                        byte_count -= 4;
                    } else {
                        IHEX_READ32_PAD(val, byte_count);
                        byte_count = 0;
                    }
                    *dst++ = __swab32(val);
                }
                break;
            }
            case IHEX_REC_EOF:
            {
                load_fw = 0;
                err = 0;
                break;
            }
            case IHEX_REC_EXT_SEG_ADD: /* Extended Segment Address */
            {
                IHEX_READ16(segaddr);
                hwaddr = (haddr << 16) + (segaddr << 4);
                break;
            }
            case IHEX_REC_EXT_LIN_ADD: /* Extended Linear Address */
            {
                IHEX_READ16(haddr);
                hwaddr = (haddr << 16) + (segaddr << 4);
                break;
            }
            case IHEX_REC_START_LIN_ADD: /* Start Linear Address */
            {
                u32 val;
                IHEX_READ32(val); /* need to read for checksum */
                break;
            }
            case IHEX_REC_START_SEG_ADD:
            default:
            {
                dev_err(dev, "ihex: record type %d not supported\n", rec_type);
                load_fw = 0;
            }
        }

        /* Read and compare checksum */
        IHEX_READ8(checksum, 0);
        if (checksum != (u8)(~csum + 1))
            goto end;
    }

#undef IHEX_READ8
#undef IHEX_READ16
#undef IHEX_READ32
#undef IHEX_READ32_PAD

  end:
    release_firmware(fw);

    if (err)
        dev_err(dev, "%s: Invalid ihex record around line %d\n", filename, rec_idx);

    return err;
}
#endif
#endif /* CONFIG_RWNX_TL4 */

#ifndef CONFIG_RWNX_SDM
/**
 * rwnx_plat_get_rf() - Retrun the RF used in the platform
 *
 * @rwnx_plat: pointer to platform structure
 */
static u32 rwnx_plat_get_rf(struct rwnx_plat *rwnx_plat)
{
    u32 ver;
    ver = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, MDM_HDMCONFIG_ADDR);

    ver = __MDM_PHYCFG_FROM_VERS(ver);
    WARN(((ver != MDM_PHY_CONFIG_TRIDENT) &&
          (ver != MDM_PHY_CONFIG_CATAXIA) &&
          (ver != MDM_PHY_CONFIG_KARST)),
         "Unknown PHY version 0x%08x\n", ver);

    return ver;
}

/**
 * rwnx_plat_get_clkctrl_addr() - Return the clock control register address
 *
 * @rwnx_plat: platform data
 */
#ifndef CONFIG_RWNX_SDM
static u32 rwnx_plat_get_clkctrl_addr(struct rwnx_plat *rwnx_plat)
{
    u32 regval;

    if (rwnx_plat_get_rf(rwnx_plat) ==  MDM_PHY_CONFIG_TRIDENT)
        return MDM_MEMCLKCTRL0_ADDR;

    /* Get the FPGA signature */
    regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);

    if (__FPGA_TYPE(regval) == 0xC0CA)
        return CRM_CLKGATEFCTRL0_ADDR;
    else
        return MDM_CLKGATEFCTRL0_ADDR;
}
#endif /* CONFIG_RWNX_SDM */

/**
 * rwnx_plat_stop_agcfsm() - Stop a AGC state machine
 *
 * @rwnx_plat: pointer to platform structure
 * @agg_reg: Address of the agccntl register (within RWNX_ADDR_SYSTEM)
 * @agcctl: Updated with value of the agccntl rgister before stop
 * @memclk: Updated with value of the clock register before stop
 * @agc_ver: Version of the AGC load procedure
 * @clkctrladdr: Indicates which AGC clock register should be accessed
 */
static void rwnx_plat_stop_agcfsm(struct rwnx_plat *rwnx_plat, int agc_reg,
                                  u32 *agcctl, u32 *memclk, u8 agc_ver,
                                  u32 clkctrladdr)
{
    /* First read agcctnl and clock registers */
    *memclk = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, clkctrladdr);

    /* Stop state machine : xxAGCCNTL0[AGCFSMRESET]=1 */
    *agcctl = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);
    RWNX_REG_WRITE((*agcctl) | BIT(12), rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);
    printk("%s:%d, read 0x%x, 0x%x\n", __func__, __LINE__, agc_reg, RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,agc_reg));

    /* Force clock */
    if (agc_ver > 0) {
        /* CLKGATEFCTRL0[AGCCLKFORCE]=1 */
        RWNX_REG_WRITE((*memclk) | BIT(29), rwnx_plat, RWNX_ADDR_SYSTEM,
                       clkctrladdr);
        printk("%s:%d, read 0x%x, 0x%x\n", __func__, __LINE__, clkctrladdr, RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,clkctrladdr));
    } else {
        /* MEMCLKCTRL0[AGCMEMCLKCTRL]=0 */
        RWNX_REG_WRITE((*memclk) & ~BIT(3), rwnx_plat, RWNX_ADDR_SYSTEM,
                       clkctrladdr);
    }
}


/**
 * rwnx_plat_start_agcfsm() - Restart a AGC state machine
 *
 * @rwnx_plat: pointer to platform structure
 * @agg_reg: Address of the agccntl register (within RWNX_ADDR_SYSTEM)
 * @agcctl: value of the agccntl register to restore
 * @memclk: value of the clock register to restore
 * @agc_ver: Version of the AGC load procedure
 * @clkctrladdr: Indicates which AGC clock register should be accessed
 */
static void rwnx_plat_start_agcfsm(struct rwnx_plat *rwnx_plat, int agc_reg,
                                   u32 agcctl, u32 memclk, u8 agc_ver,
                                   u32 clkctrladdr)
{

    /* Release clock */
    if (agc_ver > 0)
        /* CLKGATEFCTRL0[AGCCLKFORCE]=0 */
        RWNX_REG_WRITE(memclk & ~BIT(29), rwnx_plat, RWNX_ADDR_SYSTEM,
                       clkctrladdr);
    else
        /* MEMCLKCTRL0[AGCMEMCLKCTRL]=1 */
        RWNX_REG_WRITE(memclk | BIT(3), rwnx_plat, RWNX_ADDR_SYSTEM,
                       clkctrladdr);

    /* Restart state machine: xxAGCCNTL0[AGCFSMRESET]=0 */
    RWNX_REG_WRITE(agcctl & ~BIT(12), rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);
}
#endif

/**
 * rwnx_plat_get_agc_load_version() - Return the agc load protocol version and the
 * address of the clock control register
 *
 * @rwnx_plat: platform data
 * @rf: rf in used
 * @clkctrladdr: returned clock control register address
 *
 * c.f Modem UM (AGC/CCA initialization)
 */
#ifndef CONFIG_RWNX_SDM
static u8 rwnx_plat_get_agc_load_version(struct rwnx_plat *rwnx_plat, u32 rf,
                                         u32 *clkctrladdr)
{
    u8 agc_load_ver = 0;
    u32 agc_ver;
//    u32 regval;

    *clkctrladdr = rwnx_plat_get_clkctrl_addr(rwnx_plat);

    /* Trident PHY use old method */
    if (rf ==  MDM_PHY_CONFIG_TRIDENT)
        return 0;

    /* Get the FPGA signature */
//    regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);

    /* Read RIU version register */
    agc_ver = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, RIU_RWNXVERSION_ADDR);
    agc_load_ver = __RIU_AGCLOAD_FROM_VERS(agc_ver);

    return agc_load_ver;
}
#endif /* CONFIG_RWNX_SDM */

/**
 * rwnx_plat_agc_load() - Load AGC ucode
 *
 * @rwnx_plat: platform data
 * c.f Modem UM (AGC/CCA initialization)
 */
static int rwnx_plat_agc_load(struct rwnx_plat *rwnx_plat)
{
    int ret = 0;
#ifndef CONFIG_RWNX_SDM
    u32 agc = 0, agcctl, memclk;
    u32 clkctrladdr;
    u32 rf = rwnx_plat_get_rf(rwnx_plat);
    u8 agc_ver;

    switch (rf) {
        case MDM_PHY_CONFIG_TRIDENT:
            agc = AGC_RWNXAGCCNTL_ADDR;
            break;
        case MDM_PHY_CONFIG_CATAXIA:
        case MDM_PHY_CONFIG_KARST:
            agc = RIU_RWNXAGCCNTL_ADDR;
            break;
        default:
            return -1;
    }

    agc_ver = rwnx_plat_get_agc_load_version(rwnx_plat, rf, &clkctrladdr);

    rwnx_plat_stop_agcfsm(rwnx_plat, agc, &agcctl, &memclk, agc_ver, clkctrladdr);
    printk("%s:%d, agc_addr %x, memclk %x, agc_ver %x, clkctrladdr %x\n", __func__, __LINE__, agc, memclk, agc_ver, clkctrladdr);

#if 1
    ret = rwnx_plat_bin_fw_upload(rwnx_plat,
                              RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, PHY_AGC_UCODE_ADDR),
                              RWNX_AGC_FW_NAME);
#else
    RWNX_REG_WRITE(0x33445566, rwnx_plat, RWNX_ADDR_SYSTEM, 0x00c0a000);
    RWNX_REG_WRITE(0x33445566, rwnx_plat, RWNX_ADDR_SYSTEM, 0x00c0a004);
    RWNX_REG_WRITE(0x33445566, rwnx_plat, RWNX_ADDR_SYSTEM, 0x00c0a008);
    RWNX_REG_WRITE(0x33445566, rwnx_plat, RWNX_ADDR_SYSTEM, 0x00c0a00c);
#endif

    if (!ret && (agc_ver == 1)) {
        /* Run BIST to ensure that the AGC RAM was correctly loaded */
        RWNX_REG_WRITE(BIT(28), rwnx_plat, RWNX_ADDR_SYSTEM,
                       RIU_RWNXDYNAMICCONFIG_ADDR);
        while (RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
                             RIU_RWNXDYNAMICCONFIG_ADDR) & BIT(28));

        if (!(RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
                            RIU_AGCMEMBISTSTAT_ADDR) & BIT(0))) {
            dev_err(rwnx_platform_get_dev(rwnx_plat),
                    "AGC RAM not loaded correctly 0x%08x\n",
                    RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
                                  RIU_AGCMEMSIGNATURESTAT_ADDR));
            ret = -EIO;
        }
    }
    rwnx_plat_start_agcfsm(rwnx_plat, agc, agcctl, memclk, agc_ver, clkctrladdr);

#endif
    return ret;
}

/**
 * rwnx_ldpc_load() - Load LDPC RAM
 *
 * @rwnx_hw: Main driver data
 * c.f Modem UM (LDPC initialization)
 */
 #if 0
static int rwnx_ldpc_load(struct rwnx_hw *rwnx_hw)
{
#ifndef CONFIG_RWNX_SDM
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    u32 rf = rwnx_plat_get_rf(rwnx_plat);
    u32 phy_feat = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, MDM_HDMCONFIG_ADDR);
    u32 phy_vers = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, MDM_HDMVERSION_ADDR);

    if (((rf !=  MDM_PHY_CONFIG_KARST) && (rf !=  MDM_PHY_CONFIG_CATAXIA)) ||
        (phy_feat & (MDM_LDPCDEC_BIT | MDM_LDPCENC_BIT)) !=
        (MDM_LDPCDEC_BIT | MDM_LDPCENC_BIT)) {
        goto disable_ldpc;
    }

    // No need to load the LDPC RAM anymore on modems starting from version v31
    if (__MDM_VERSION(phy_vers) > 30) {
        return 0;
    }

    if (rwnx_plat_bin_fw_upload(rwnx_plat,
                            RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, PHY_LDPC_RAM_ADDR),
                            RWNX_LDPC_RAM_NAME)) {
        goto disable_ldpc;
    }

    return 0;

  disable_ldpc:
    rwnx_hw->mod_params->ldpc_on = false;

#endif /* CONFIG_RWNX_SDM */
    return 0;
}
#endif

/**
 * rwnx_plat_lmac_load() - Load FW code
 *
 * @rwnx_plat: platform data
 */
static int rwnx_plat_lmac_load(struct rwnx_plat *rwnx_plat)
{
    int ret;

    #ifdef CONFIG_RWNX_TL4
    ret = rwnx_plat_tl4_fw_upload(rwnx_plat,
                                  RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, RAM_LMAC_FW_ADDR),
                                  RWNX_MAC_FW_NAME);
    #else
    ret = rwnx_plat_fw_upload(rwnx_plat,
            RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, RAM_LMAC_FW_ADDR),
            RWNX_MAC_FW_NAME3);
    #endif

    return ret;
}

/**
 * rwnx_rf_fw_load() - Load RF FW if any
 *
 * @rwnx_hw: Main driver data
 */
 #if 0
static int rwnx_plat_rf_fw_load(struct rwnx_hw *rwnx_hw)
{
#ifndef CONFIG_RWNX_SDM
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    u32 rf = rwnx_plat_get_rf(rwnx_plat);
    struct device *dev = rwnx_platform_get_dev(rwnx_plat);
    const struct firmware *fw;
    int err = 0;
    u8 const *file_data;
    int remain;
    u32 clkforce;
    u32 clkctrladdr;

    // Today only Cataxia has a FW to load
    if (rf !=  MDM_PHY_CONFIG_CATAXIA)
        return 0;

    err = request_firmware(&fw, RWNX_CATAXIA_FW_NAME, dev);
    if (err)
    {
        dev_err(dev, "Make sure your board has up-to-date packages.");
        dev_err(dev, "Run \"sudo smart update\" \"sudo smart upgrade\" commands.\n");
        return err;
    }

    file_data = fw->data;
    remain = fw->size;

    // Get address of clock control register
    clkctrladdr = rwnx_plat_get_clkctrl_addr(rwnx_plat);

    // Force RC clock
    clkforce = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, clkctrladdr);
    RWNX_REG_WRITE(clkforce | BIT(27), rwnx_plat, RWNX_ADDR_SYSTEM, clkctrladdr);
    mdelay(1);

    // Reset RC
    RWNX_REG_WRITE(0x00003100, rwnx_plat, RWNX_ADDR_SYSTEM, RC_SYSTEM_CONFIGURATION_ADDR);
    mdelay(20);

    // Reset RF
    RWNX_REG_WRITE(0x00123100, rwnx_plat, RWNX_ADDR_SYSTEM, RC_SYSTEM_CONFIGURATION_ADDR);
    mdelay(20);

    // Select trx 2 HB
    RWNX_REG_WRITE(0x00113100, rwnx_plat, RWNX_ADDR_SYSTEM, RC_SYSTEM_CONFIGURATION_ADDR);
    mdelay(50);

    // Set ASP freeze
    RWNX_REG_WRITE(0xC1010001, rwnx_plat, RWNX_ADDR_SYSTEM, RC_ACCES_TO_CATAXIA_REG_ADDR);
    mdelay(1);

    /* Walk through all the lines of the FW file */
    while (remain >= 10) {
        u32 data;

        if (sscanf(file_data, "0x%08X", &data) != 1)
        {
            // Corrupted FW file
            err = -1;
            break;
        }
        file_data += 11;
        remain -= 11;

        RWNX_REG_WRITE(data, rwnx_plat, RWNX_ADDR_SYSTEM, RC_ACCES_TO_CATAXIA_REG_ADDR);
        udelay(50);
    }

    // Clear ASP freeze
    RWNX_REG_WRITE(0xE0010011, rwnx_plat, RWNX_ADDR_SYSTEM, RC_ACCES_TO_CATAXIA_REG_ADDR);
    mdelay(1);

    // Unforce RC clock
    RWNX_REG_WRITE(clkforce, rwnx_plat, RWNX_ADDR_SYSTEM, clkctrladdr);

    release_firmware(fw);

#endif /* CONFIG_RWNX_SDM */
    return err;
}
#endif

/**
 * rwnx_plat_mpif_sel() - Select the MPIF according to the FPGA signature
 *
 * @rwnx_plat: platform data
 */
static void rwnx_plat_mpif_sel(struct rwnx_plat *rwnx_plat)
{
#ifndef CONFIG_RWNX_SDM
    u32 regval;
    u32 type;

    /* Get the FPGA signature */
    regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);
    type = __FPGA_TYPE(regval);

    /* Check if we need to switch to the old MPIF or not */
    if ((type != 0xCAFE) && (type != 0XC0CA) && (regval & 0xF) < 0x3)
    {
        /* A old FPGA A is used, so configure the FPGA B to use the old MPIF */
        RWNX_REG_WRITE(0x3, rwnx_plat, RWNX_ADDR_SYSTEM, FPGAB_MPIF_SEL_ADDR);
    }
#endif
}


/**
 * rwnx_platform_reset() - Reset the platform
 *
 * @rwnx_plat: platform data
 */
static int rwnx_platform_reset(struct rwnx_plat *rwnx_plat)
{
    u32 regval_rwnx;
    u32 regval_cpu;

    /* the doc states that SOFT implies FPGA_B_RESET
     * adding FPGA_B_RESET is clearer */
    regval_rwnx = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
    printk("%s:%d, offset %x regval %x\n", __func__, __LINE__,
           SYSCTRL_MISC_CNTL_ADDR, regval_rwnx);
    RWNX_REG_WRITE(SOFT_RESET | FPGA_B_RESET, rwnx_plat,
                   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);

    regval_cpu = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_AON, RG_PMU_A22);
    regval_cpu |= CPU_RESET;
    RWNX_REG_WRITE(regval_cpu, rwnx_plat, RWNX_ADDR_AON, RG_PMU_A22);

    msleep(100);

    regval_rwnx = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
    regval_cpu = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_AON, RG_PMU_A22);
    printk("%s:%d regval_rwnx:%x, regval_cpu:%x\n", __func__, __LINE__, regval_rwnx, regval_cpu);

    if ((regval_rwnx & SOFT_RESET) || (!(regval_cpu & CPU_RESET))) {
        dev_err(rwnx_platform_get_dev(rwnx_plat), "reset: failed\n");
        return -EIO;
    }

    RWNX_REG_WRITE(regval_rwnx & ~FPGA_B_RESET, rwnx_plat,
                   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
    printk("%s:%d\n", __func__, __LINE__);
    msleep(100);
    return 0;
}

/**
 * rwmx_platform_save_config() - Save hardware config before reload
 *
 * @rwnx_plat: Pointer to platform data
 *
 * Return configuration registers values.
 */
static void* rwnx_term_save_config(struct rwnx_plat *rwnx_plat)
{
    const u32 *reg_list = NULL;
    u32 *reg_value = NULL, *res = NULL;
    int i, size = 0;

    if (rwnx_plat->get_config_reg) {
        size = rwnx_plat->get_config_reg(rwnx_plat, &reg_list);
    }

    if (size <= 0)
        return NULL;

    res = kmalloc(sizeof(u32) * size, GFP_KERNEL);
    if (!res)
        return NULL;

    reg_value = res;
    for (i = 0; i < size; i++) {
        *reg_value++ = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
                                     *reg_list++);
    }

    return res;
}

/**
 * rwmx_platform_restore_config() - Restore hardware config after reload
 *
 * @rwnx_plat: Pointer to platform data
 * @reg_value: Pointer of value to restore
 * (obtained with rwmx_platform_save_config())
 *
 * Restore configuration registers value.
 */
static void rwnx_term_restore_config(struct rwnx_plat *rwnx_plat,
                                     u32 *reg_value)
{
    const u32 *reg_list = NULL;
    int i, size = 0;

    if (!reg_value || !rwnx_plat->get_config_reg)
        return;

    size = rwnx_plat->get_config_reg(rwnx_plat, &reg_list);

    for (i = 0; i < size; i++) {
        RWNX_REG_WRITE(*reg_value++, rwnx_plat, RWNX_ADDR_SYSTEM,
                       *reg_list++);
    }
}

#ifndef CONFIG_RWNX_FHOST
static int rwnx_check_fw_compatibility(struct rwnx_hw *rwnx_hw)
{
    struct ipc_shared_env_tag *shared = rwnx_hw->ipc_env->shared;
    #ifdef CONFIG_RWNX_SOFTMAC
    struct wiphy *wiphy = rwnx_hw->hw->wiphy;
    #else //CONFIG_RWNX_SOFTMAC
    struct wiphy *wiphy = rwnx_hw->wiphy;
    #endif //CONFIG_RWNX_SOFTMAC
    #ifdef CONFIG_RWNX_OLD_IPC
    int ipc_shared_version = 10;
    #else //CONFIG_RWNX_OLD_IPC
    int ipc_shared_version = 11;
    #endif //CONFIG_RWNX_OLD_IPC
    int res = 0;

    if (shared->comp_info.ipc_shared_version != ipc_shared_version)
    {
        wiphy_err(wiphy, "Different versions of IPC shared version between driver and FW (%d != %d)\n ",
                  ipc_shared_version, shared->comp_info.ipc_shared_version);
        res = -1;
    }

    if (shared->comp_info.radarbuf_cnt != IPC_RADARBUF_CNT)
    {
        wiphy_err(wiphy, "Different number of host buffers available for Radar events handling "\
                  "between driver and FW (%d != %d)\n", IPC_RADARBUF_CNT,
                  shared->comp_info.radarbuf_cnt);
        res = -1;
    }

    if (shared->comp_info.unsuprxvecbuf_cnt != IPC_UNSUPRXVECBUF_CNT)
    {
        wiphy_err(wiphy, "Different number of host buffers available for unsupported Rx vectors "\
                  "handling between driver and FW (%d != %d)\n", IPC_UNSUPRXVECBUF_CNT,
                  shared->comp_info.unsuprxvecbuf_cnt);
        res = -1;
    }

    #ifdef CONFIG_RWNX_FULLMAC
    if (shared->comp_info.rxdesc_cnt != IPC_RXDESC_CNT)
    {
        wiphy_err(wiphy, "Different number of shared descriptors available for Data RX handling "\
                  "between driver and FW (%d != %d)\n", IPC_RXDESC_CNT,
                  shared->comp_info.rxdesc_cnt);
        res = -1;
    }
    #endif /* CONFIG_RWNX_FULLMAC */

    if (shared->comp_info.rxbuf_cnt != IPC_RXBUF_CNT)
    {
        wiphy_err(wiphy, "Different number of host buffers available for Data Rx handling "\
                  "between driver and FW (%d != %d)\n", IPC_RXBUF_CNT,
                  shared->comp_info.rxbuf_cnt);
        res = -1;
    }

    if (shared->comp_info.msge2a_buf_cnt != IPC_MSGE2A_BUF_CNT)
    {
        wiphy_err(wiphy, "Different number of host buffers available for Emb->App MSGs "\
                  "sending between driver and FW (%d != %d)\n", IPC_MSGE2A_BUF_CNT,
                  shared->comp_info.msge2a_buf_cnt);
        res = -1;
    }

    if (shared->comp_info.dbgbuf_cnt != IPC_DBGBUF_CNT)
    {
        wiphy_err(wiphy, "Different number of host buffers available for debug messages "\
                  "sending between driver and FW (%d != %d)\n", IPC_DBGBUF_CNT,
                  shared->comp_info.dbgbuf_cnt);
        res = -1;
    }

    if (shared->comp_info.bk_txq != NX_TXDESC_CNT0)
    {
        wiphy_err(wiphy, "Driver and FW have different sizes of BK TX queue (%d != %d)\n",
                  NX_TXDESC_CNT0, shared->comp_info.bk_txq);
        res = -1;
    }

    if (shared->comp_info.be_txq != NX_TXDESC_CNT1)
    {
        wiphy_err(wiphy, "Driver and FW have different sizes of BE TX queue (%d != %d)\n",
                  NX_TXDESC_CNT1, shared->comp_info.be_txq);
        res = -1;
    }

    if (shared->comp_info.vi_txq != NX_TXDESC_CNT2)
    {
        wiphy_err(wiphy, "Driver and FW have different sizes of VI TX queue (%d != %d)\n",
                  NX_TXDESC_CNT2, shared->comp_info.vi_txq);
        res = -1;
    }

    if (shared->comp_info.vo_txq != NX_TXDESC_CNT3)
    {
        wiphy_err(wiphy, "Driver and FW have different sizes of VO TX queue (%d != %d)\n",
                  NX_TXDESC_CNT3, shared->comp_info.vo_txq);
        res = -1;
    }

    #if NX_TXQ_CNT == 5
    if (shared->comp_info.bcn_txq != NX_TXDESC_CNT4)
    {
        wiphy_err(wiphy, "Driver and FW have different sizes of BCN TX queue (%d != %d)\n",
                NX_TXDESC_CNT4, shared->comp_info.bcn_txq);
        res = -1;
    }
    #else
    if (shared->comp_info.bcn_txq > 0)
    {
        wiphy_err(wiphy, "BCMC enabled in firmware but disabled in driver\n");
        res = -1;
    }
    #endif /* NX_TXQ_CNT == 5 */

    if (shared->comp_info.ipc_shared_size != sizeof(ipc_shared_env))
    {
        wiphy_err(wiphy, "Different sizes of IPC shared between driver and FW (%zd != %d)\n",
                  sizeof(ipc_shared_env), shared->comp_info.ipc_shared_size);
        res = -1;
    }

    if (shared->comp_info.msg_api != MSG_API_VER)
    {
        wiphy_err(wiphy, "Different supported message API versions between "\
                  "driver and FW (%d != %d)\n", MSG_API_VER, shared->comp_info.msg_api);
        res = -1;
    }

    return res;
}
#endif /* !CONFIG_RWNX_FHOST */

#ifdef CONFIG_RWNX_USB_MODE
int rwnx_platform_on(struct rwnx_hw *rwnx_hw, void *config)
{
    u8 *shared_ram;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    int ret;

    if (rwnx_plat->enabled)
        return 0;

    if (rwnx_platform_reset(rwnx_plat))
        return -1;

    rwnx_plat_mpif_sel(rwnx_plat);

#ifndef CONFIG_RWNX_FHOST
    /* By default, we consider that there is only one RF in the system */
    rwnx_hw->phy.cnt = 1;
#endif // CONFIG_RWNX_FHOST

    if ((ret = wifi_fw_download()))
        return ret;

    if ((ret =  start_wifi()))
        return ret;

    shared_ram = RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, SHARED_RAM_START_ADDR);
    if ((ret = rwnx_ipc_init(rwnx_hw, shared_ram)))
        return ret;

    //if ((ret = rwnx_plat->enable(rwnx_hw)))
        //return ret;
    RWNX_REG_WRITE(BOOTROM_ENABLE, rwnx_plat,
                   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);

    //change cpu clock
    RWNX_REG_WRITE(0x4f770033, rwnx_plat,
                   RWNX_ADDR_MAC_PHY, 0x00a0d090);

    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, rwnx_plat->hif_ops->hi_read_word(RWNX_ADDR_MAC_PHY + 0x00a070b4, USB_EP4));
    msleep(10);
    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, rwnx_plat->hif_ops->hi_read_word(RWNX_ADDR_MAC_PHY + 0x00a070b4, USB_EP4));
    msleep(10);
    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, rwnx_plat->hif_ops->hi_read_word(RWNX_ADDR_MAC_PHY + 0x00a070b4, USB_EP4 ));
    msleep(10);

    //start firmware cpu
    RWNX_REG_WRITE(0x00070000, rwnx_plat, RWNX_ADDR_AON, RG_PMU_A22);
    //printk("%s:%d, value %x", __func__, __LINE__, readl(rwnx_plat->get_address(rwnx_plat, RWNX_ADDR_MAC_PHY, 0x00a070b4)));
    rwnx_fw_trace_config_filters(rwnx_get_shared_trace_buf(rwnx_hw),
                                 rwnx_ipc_fw_trace_desc_get(rwnx_hw),
                                 rwnx_hw->mod_params->ftl);

    #ifndef CONFIG_RWNX_FHOST
    if ((ret = rwnx_check_fw_compatibility(rwnx_hw)))
    {
        if (rwnx_hw->plat->disable)
            rwnx_hw->plat->disable(rwnx_hw);
        tasklet_kill(&rwnx_hw->task);
        rwnx_ipc_deinit(rwnx_hw);
        return ret;
    }
    #endif /* !CONFIG_RWNX_FHOST */

    if (config)
        rwnx_term_restore_config(rwnx_plat, config);

    rwnx_ipc_start(rwnx_hw);

    rwnx_plat->enabled = true;

    return 0;
}
#else
/**
 * rwnx_platform_on() - Start the platform
 *
 * @rwnx_hw: Main driver data
 * @config: Config to restore (NULL if nothing to restore)
 *
 * It starts the platform :
 * - load fw and ucodes
 * - initialize IPC
 * - boot the fw
 * - enable link communication/IRQ
 *
 * Called by 802.11 part
 */
int rwnx_platform_on(struct rwnx_hw *rwnx_hw, void *config)
{
    u8 *shared_ram;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    int ret;

    if (rwnx_plat->enabled)
        return 0;

    if (rwnx_platform_reset(rwnx_plat))
        return -1;

    rwnx_plat_mpif_sel(rwnx_plat);

#ifndef CONFIG_RWNX_FHOST
    /* By default, we consider that there is only one RF in the system */
    rwnx_hw->phy.cnt = 1;
#endif // CONFIG_RWNX_FHOST

    if ((ret = rwnx_plat_agc_load(rwnx_plat)))
        return ret;

    if ((ret = rwnx_plat_lmac_load(rwnx_plat)))
        return ret;

    shared_ram = RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, SHARED_RAM_START_ADDR);
    if ((ret = rwnx_ipc_init(rwnx_hw, shared_ram)))
        return ret;

    if ((ret = rwnx_plat->enable(rwnx_hw)))
        return ret;
    RWNX_REG_WRITE(BOOTROM_ENABLE, rwnx_plat,
                   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);

    //change cpu clock
    RWNX_REG_WRITE(0x4f770033, rwnx_plat,
                   RWNX_ADDR_MAC_PHY, 0x00a0d090);

    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, readl(rwnx_plat->get_address(rwnx_plat, RWNX_ADDR_MAC_PHY, 0x00a070b4 )));
    msleep(10);
    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, readl(rwnx_plat->get_address(rwnx_plat, RWNX_ADDR_MAC_PHY, 0x00a070b4 )));
    msleep(10);
    printk("%s:%d, reg:0xa070b4 : value %x", __func__, __LINE__, readl(rwnx_plat->get_address(rwnx_plat, RWNX_ADDR_MAC_PHY, 0x00a070b4 )));
    msleep(10);

    //start firmware cpu
    RWNX_REG_WRITE(0x00070000, rwnx_plat, RWNX_ADDR_AON, RG_PMU_A22);
    printk("%s:%d, value %x", __func__, __LINE__, readl(rwnx_plat->get_address(rwnx_plat, RWNX_ADDR_MAC_PHY, 0x00a070b4)));
    rwnx_fw_trace_config_filters(rwnx_get_shared_trace_buf(rwnx_hw),
                                 rwnx_ipc_fw_trace_desc_get(rwnx_hw),
                                 rwnx_hw->mod_params->ftl);

    #ifndef CONFIG_RWNX_FHOST
    if ((ret = rwnx_check_fw_compatibility(rwnx_hw)))
    {
        if (rwnx_hw->plat->disable)
            rwnx_hw->plat->disable(rwnx_hw);
        tasklet_kill(&rwnx_hw->task);
        rwnx_ipc_deinit(rwnx_hw);
        return ret;
    }
    #endif /* !CONFIG_RWNX_FHOST */

    if (config)
        rwnx_term_restore_config(rwnx_plat, config);

    rwnx_ipc_start(rwnx_hw);

    rwnx_plat->enabled = true;

    return 0;
}
#endif
/**
 * rwnx_platform_off() - Stop the platform
 *
 * @rwnx_hw: Main driver data
 * @config: Updated with pointer to config, to be able to restore it with
 * rwnx_platform_on(). It's up to the caller to free the config. Set to NULL
 * if configuration is not needed.
 *
 * Called by 802.11 part
 */
void rwnx_platform_off(struct rwnx_hw *rwnx_hw, void **config)
{
    if (!rwnx_hw->plat->enabled) {
        if (config)
            *config = NULL;
        return;
    }

    rwnx_ipc_stop(rwnx_hw);

    if (config)
        *config = rwnx_term_save_config(rwnx_hw->plat);

    if (rwnx_hw->plat->disable)
        rwnx_hw->plat->disable(rwnx_hw);

    tasklet_kill(&rwnx_hw->task);

    rwnx_ipc_deinit(rwnx_hw);

    rwnx_platform_reset(rwnx_hw->plat);

    rwnx_hw->plat->enabled = false;
}

/**
 * rwnx_platform_init() - Initialize the platform
 *
 * @rwnx_plat: platform data (already updated by platform driver)
 * @platform_data: Pointer to store the main driver data pointer (aka rwnx_hw)
 *                That will be set as driver data for the platform driver
 * Return: 0 on success, < 0 otherwise
 *
 * Called by the platform driver after it has been probed
 */
int rwnx_platform_init(struct rwnx_plat *rwnx_plat, void **platform_data)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_plat->enabled = false;

#if defined CONFIG_RWNX_SOFTMAC
    return rwnx_mac80211_init(rwnx_plat, platform_data);
#elif defined CONFIG_RWNX_FULLMAC
    return rwnx_cfg80211_init(rwnx_plat, platform_data);
#elif defined CONFIG_RWNX_FHOST
    return rwnx_fhost_init(rwnx_plat, platform_data);
#endif
}

/**
 * rwnx_platform_deinit() - Deinitialize the platform
 *
 * @rwnx_hw: main driver data
 *
 * Called by the platform driver after it is removed
 */
void rwnx_platform_deinit(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

#if defined CONFIG_RWNX_SOFTMAC
    rwnx_mac80211_deinit(rwnx_hw);
#elif defined CONFIG_RWNX_FULLMAC
    rwnx_cfg80211_deinit(rwnx_hw);
#elif defined CONFIG_RWNX_FHOST
    rwnx_fhost_deinit(rwnx_hw);
#endif
}

#ifdef CONFIG_RWNX_USB_MODE
int rwnx_platform_register_drv(void)
{
    int ret = 0;
    struct rwnx_plat *rwnx_plat;
    void *drv_data = NULL;

    if (!auc_driver_insmoded) {
        ret = aml_common_insmod();
    }

    rwnx_plat = kzalloc(sizeof(struct rwnx_plat), GFP_KERNEL);
    if (!rwnx_plat)
        return -ENOMEM;

    printk("%s:%d \n", __func__, __LINE__);

    rwnx_plat->usb_dev = g_udev;
    rwnx_plat->hif_ops = &g_auc_hif_ops;

    rwnx_platform_init(rwnx_plat, &drv_data);
    dev_set_drvdata(&rwnx_plat->usb_dev->dev, drv_data);

    return ret;
}

void rwnx_platform_unregister_drv(void)
{
    struct rwnx_hw *rwnx_hw;
    struct rwnx_plat *rwnx_plat;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_hw = dev_get_drvdata(&g_udev->dev);
    rwnx_plat = rwnx_hw->plat;

    rwnx_platform_deinit(rwnx_hw);
    kfree(rwnx_plat);

    dev_set_drvdata(&g_udev->dev, NULL);
}
#else

//fpga bar0 0x6000_0000~0x603f_ffff 4M
//fpga bar1 0x0020_0000~0x004f_ffff 4M
//fpga bar2 0x00c0_0000~0x00ff_ffff 4M
//fpga bar3 0x00a0_0000~0x00af_ffff 1M
//fpga bar4 0x0000_0000~0x0007_ffff 512K
//fpga bar5 0x6080_0000~0x60ff_ffff 8M
u8* rwnx_pci_get_map_address(struct net_device *dev, unsigned int offset)
{
    struct rwnx_vif *rwnx_vif = netdev_priv(dev);
    struct rwnx_hw *rwnx_hw = rwnx_vif->rwnx_hw;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    struct rwnx_pci *rwnx_pci = (struct rwnx_pci *)rwnx_plat->priv;

    if (!rwnx_pci) {
        return NULL;
    }
    printk("%s,%d\n", __func__, __LINE__);
    if (offset >= 0x60000000 && offset <= 0x603fffff) {
        return ( rwnx_pci->pci_bar0_vaddr + (offset - 0x60000000));

    } else if (offset >= 0x00200000 && offset <= 0x004fffff) {
       return ( rwnx_pci->pci_bar1_vaddr + (offset - 0x00200000));

    } else if (offset >= 0x00c00000 && offset <= 0x00ffffff) {
        return ( rwnx_pci->pci_bar2_vaddr + (offset - 0x00c00000));

    } else if (offset >= 0x00a00000 && offset <= 0x00afffff) {
        return ( rwnx_pci->pci_bar3_vaddr + (offset - 0x00a00000));

    } else if (offset <= 0x0007ffff) {
        return ( rwnx_pci->pci_bar4_vaddr + offset);

    } else if (offset >= 0x60800000 && offset <= 0x60ffffff) {
        return ( rwnx_pci->pci_bar5_vaddr + (offset - 0x60800000));

    } else {
        printk("offset error \n");
        return NULL;
    }
}

static int rwnx_pci_platform_enable(struct rwnx_hw *rwnx_hw)
{
    int ret;

    /* sched_setscheduler on ONESHOT threaded irq handler for BCNs ? */
    ret = request_irq(rwnx_hw->plat->pci_dev->irq, rwnx_irq_hdlr, 0,
                      "rwnx", rwnx_hw);
    return ret;
}

static int rwnx_pci_platform_disable(struct rwnx_hw *rwnx_hw)
{
    free_irq(rwnx_hw->plat->pci_dev->irq, rwnx_hw);
    return 0;
}

static u8* rwnx_pci_get_address(struct rwnx_plat *rwnx_plat, int addr_name,
                               unsigned int offset)
{
    struct rwnx_pci *rwnx_pci = (struct rwnx_pci *)rwnx_plat->priv;

    if (WARN(addr_name >= RWNX_ADDR_MAX, "Invalid address %d", addr_name))
        return NULL;

    if (addr_name == RWNX_ADDR_CPU) //0x00000000-0x0007ffff (ICCM)
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, rwnx_pci->pci_bar4_vaddr + offset);
        return rwnx_pci->pci_bar4_vaddr + offset;
    }
    else if (addr_name == RWNX_ADDR_MAC_PHY) //0x00a00000-0x00afffff
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, rwnx_pci->pci_bar3_vaddr + offset);
        return rwnx_pci->pci_bar3_vaddr + offset - 0x00a00000;
    }
    else if (addr_name == RWNX_ADDR_AON)// 0x00c00000 - 0x00ffffff (AON & DCCM)
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, rwnx_pci->pci_bar2_vaddr + offset);
        return rwnx_pci->pci_bar2_vaddr + offset - 0x00c00000;
    }
    else if (addr_name == RWNX_ADDR_SYSTEM)
    {
        if (offset >= IPC_REG_BASE_ADDR)
        {
            printk("%s:%d, bar5 %x, address %x\n", __func__, __LINE__, rwnx_pci->pci_bar5_vaddr, rwnx_pci->pci_bar5_vaddr + offset - IPC_REG_BASE_ADDR);
            return rwnx_pci->pci_bar5_vaddr + offset - IPC_REG_BASE_ADDR;
        }
        else
        {
            printk("%s:%d, address %x\n", __func__, __LINE__, rwnx_pci->pci_bar0_vaddr + offset);
            return rwnx_pci->pci_bar0_vaddr + offset;
        }
    }
    else
    {
        printk("%s:%d, error addr_name\n", __func__,__LINE__);
        return NULL;
    }
}

static void rwnx_pci_ack_irq(struct rwnx_plat *rwnx_plat)
{

}

static const u32 rwnx_pci_config_reg[] = {
    NXMAC_DEBUG_PORT_SEL_ADDR,
    SYSCTRL_DIAG_CONF_ADDR,
    SYSCTRL_PHYDIAG_CONF_ADDR,
    SYSCTRL_RIUDIAG_CONF_ADDR,
    RF_V7_DIAGPORT_CONF1_ADDR,
};

static const u32 rwnx_pci_he_config_reg[] = {
    SYSCTRL_DIAG_CONF0,
    SYSCTRL_DIAG_CONF1,
    SYSCTRL_DIAG_CONF2,
    SYSCTRL_DIAG_CONF3,
};

static int rwnx_pci_get_config_reg(struct rwnx_plat *rwnx_plat, const u32 **list)
{
    u32 fpga_sign;

    if (!list)
        return 0;

    fpga_sign = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);
    if (__FPGA_TYPE(fpga_sign) == 0xc0ca) {
        *list = rwnx_pci_he_config_reg;
        return ARRAY_SIZE(rwnx_pci_he_config_reg);
    } else {
        *list = rwnx_pci_config_reg;
        return ARRAY_SIZE(rwnx_pci_config_reg);
    }
}

/**
 * rwnx_platform_register_drv() - Register all possible platform drivers
 */
int rwnx_platform_register_drv(void)
{
    int ret = 0;
    struct rwnx_plat *rwnx_plat = NULL;
    void *drv_data = NULL;
    printk("%s,%d, g_pci_driver_insmoded=%d\n", __func__, __LINE__, g_pci_driver_insmoded);

    if (!g_pci_driver_insmoded) {
        ret = aml_pci_insmod();
        msleep(100);
    }

    if (!g_pci_after_probe) {
        return -ENODEV;
    }

    rwnx_plat = kzalloc(sizeof(struct rwnx_plat) + sizeof(struct rwnx_pci), GFP_KERNEL);
    if (!rwnx_plat)
        return -ENOMEM;

    memcpy(rwnx_plat, g_rwnx_plat_pci, sizeof(struct rwnx_plat) + sizeof(struct rwnx_pci));

    rwnx_plat->enable = rwnx_pci_platform_enable;
    rwnx_plat->disable = rwnx_pci_platform_disable;
    rwnx_plat->get_address = rwnx_pci_get_address;
    rwnx_plat->ack_irq = rwnx_pci_ack_irq;
    rwnx_plat->get_config_reg = rwnx_pci_get_config_reg;

    g_pci_dev = rwnx_plat->pci_dev;
    ret = rwnx_platform_init(rwnx_plat, &drv_data);
    pci_set_drvdata(g_pci_dev, drv_data);

    return ret;
}

/**
 * rwnx_platform_unregister_drv() - Unegister all platform drivers
 */
void rwnx_platform_unregister_drv(void)
{
    struct rwnx_hw *rwnx_hw;
    struct rwnx_plat *rwnx_plat;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_hw = pci_get_drvdata(g_pci_dev);
    rwnx_plat = rwnx_hw->plat;

    rwnx_platform_deinit(rwnx_hw);
    kfree(rwnx_plat);
    printk("%s,%d\n", __func__, __LINE__);

    pci_set_drvdata(g_pci_dev, NULL);
}
#endif

#ifndef CONFIG_RWNX_SDM
MODULE_FIRMWARE(RWNX_AGC_FW_NAME);
MODULE_FIRMWARE(RWNX_FCU_FW_NAME);
MODULE_FIRMWARE(RWNX_LDPC_RAM_NAME);
#endif
MODULE_FIRMWARE(RWNX_MAC_FW_NAME);
#ifndef CONFIG_RWNX_TL4
MODULE_FIRMWARE(RWNX_MAC_FW_NAME2);
#endif
MODULE_FIRMWARE(RWNX_MAC_FW_NAME3);
