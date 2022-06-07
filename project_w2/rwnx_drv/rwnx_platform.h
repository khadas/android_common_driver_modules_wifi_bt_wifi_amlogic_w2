/**
 ******************************************************************************
 *
 * @file rwnx_platorm.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#ifndef _RWNX_PLAT_H_
#define _RWNX_PLAT_H_

#include <linux/pci.h>
#include <linux/netdevice.h>
#include "usb_common.h"
#include "sdio_common.h"

#define RWNX_CONFIG_FW_NAME             "rwnx_settings.ini"
#define RWNX_WIFI_MAC_ADDR_PATH          "/data/vendor/wifi/wifimac.txt"

#define RWNX_PHY_CONFIG_TRD_NAME        "rwnx_trident.ini"
#define RWNX_PHY_CONFIG_KARST_NAME      "rwnx_karst.ini"
#define RWNX_AGC_FW_NAME                "agcram.bin"
#define RWNX_LDPC_RAM_NAME              "ldpcram.bin"
#define RWNX_CATAXIA_FW_NAME            "cataxia.fw"
#ifdef CONFIG_RWNX_SOFTMAC
#define RWNX_MAC_FW_BASE_NAME           "lmacfw"
#elif defined CONFIG_RWNX_FULLMAC
#define RWNX_MAC_FW_BASE_NAME           "fmacfw"
#elif defined CONFIG_RWNX_FHOST
#define RWNX_MAC_FW_BASE_NAME           "fhostfw"
#endif /* CONFIG_RWNX_SOFTMAC */

#ifdef CONFIG_RWNX_TL4
#define RWNX_MAC_FW_NAME RWNX_MAC_FW_BASE_NAME".hex"
#else
#define RWNX_MAC_FW_NAME  RWNX_MAC_FW_BASE_NAME".ihex"
#define RWNX_MAC_FW_NAME2 RWNX_MAC_FW_BASE_NAME".bin"
#define RWNX_MAC_FW_NAME3 "wifi_w2_fw.bin"
#endif

#define RWNX_FCU_FW_NAME                "fcuram.bin"
#define MAC_SRAM_BASE 0x00a10000

#define CPU_CLK_REG_ADDR 0x00a0d090
#define CPU_CLK_VALUE 0x4f530033 // 240M

#ifndef CONFIG_RWNX_FPGA_PCIE
#define BAR2_TABLE_NUM 0x4
#define BAR4_TABLE_NUM 0x6
#define PCIE_TABLE_NUM (BAR2_TABLE_NUM + BAR4_TABLE_NUM)
#define ISTATUS_HOST 0x00a0218c

// PCIE EP AHB_ADDR RANGE
#define PCIE_BAR2_TABLE0_EP_BASE_ADDR 0x00200000
#define PCIE_BAR2_TABLE0_EP_END_ADDR 0x002fffff
#define PCIE_BAR2_TABLE1_EP_BASE_ADDR 0x00000000
#define PCIE_BAR2_TABLE1_EP_END_ADDR 0x0007ffff
#define PCIE_BAR2_TABLE2_EP_BASE_ADDR 0x00d00000
#define PCIE_BAR2_TABLE2_EP_END_ADDR 0x00d3ffff
#define PCIE_BAR2_TABLE3_EP_BASE_ADDR 0x00400000
#define PCIE_BAR2_TABLE3_EP_END_ADDR 0x0041ffff
#define PCIE_BAR2_TABLE4_EP_BASE_ADDR 0x00a00000
#define PCIE_BAR2_TABLE4_EP_END_ADDR 0x00a0ffff
#define PCIE_BAR2_TABLE5_EP_BASE_ADDR 0x00a10000
#define PCIE_BAR2_TABLE5_EP_END_ADDR 0x00a17fff
#define PCIE_BAR2_TABLE6_EP_BASE_ADDR 0x00ac0000
#define PCIE_BAR2_TABLE6_EP_END_ADDR 0x00ac7fff

#define PCIE_BAR4_TABLE0_EP_BASE_ADDR 0x60000000
#define PCIE_BAR4_TABLE0_EP_END_ADDR 0x6007ffff
#define PCIE_BAR4_TABLE1_EP_BASE_ADDR 0x00300000
#define PCIE_BAR4_TABLE1_EP_END_ADDR 0x0037ffff
#define PCIE_BAR4_TABLE2_EP_BASE_ADDR 0x60800000
#define PCIE_BAR4_TABLE2_EP_END_ADDR 0x6083ffff
#define PCIE_BAR4_TABLE3_EP_BASE_ADDR 0x60400000
#define PCIE_BAR4_TABLE3_EP_END_ADDR 0x6043ffff
#define PCIE_BAR4_TABLE4_EP_BASE_ADDR 0x60b00000
#define PCIE_BAR4_TABLE4_EP_END_ADDR 0x60b08fff
#define PCIE_BAR4_TABLE5_EP_BASE_ADDR 0x60c00000
#define PCIE_BAR4_TABLE5_EP_END_ADDR 0x60c1ffff
#define PCIE_BAR4_TABLE6_EP_BASE_ADDR 0x00f00000
#define PCIE_BAR4_TABLE6_EP_END_ADDR 0x00f07fff
#define PCIE_BAR4_TABLE7_EP_BASE_ADDR 0x00500000
#define PCIE_BAR4_TABLE7_EP_END_ADDR 0x0051ffff

#define PCIE_BAR2_TABLE0_OFFSET 0x00000000
#define PCIE_BAR2_TABLE1_OFFSET 0x00100000
#define PCIE_BAR2_TABLE2_OFFSET 0x00180000
#define PCIE_BAR2_TABLE3_OFFSET 0x001c0000
#define PCIE_BAR2_TABLE4_OFFSET 0x001e0000
#define PCIE_BAR2_TABLE5_OFFSET 0x001f0000
#define PCIE_BAR2_TABLE6_OFFSET 0x001f8000

#define PCIE_BAR4_TABLE0_OFFSET 0x00000000
#define PCIE_BAR4_TABLE1_OFFSET 0x00080000
#define PCIE_BAR4_TABLE2_OFFSET 0x00100000
#define PCIE_BAR4_TABLE3_OFFSET 0x00140000
#define PCIE_BAR4_TABLE4_OFFSET 0x00180000
#define PCIE_BAR4_TABLE5_OFFSET 0x00190000
#define PCIE_BAR4_TABLE6_OFFSET 0x001b0000
#define PCIE_BAR4_TABLE7_OFFSET 0x001c0000

enum
{
    PCIE_BAR0,
    PCIE_BAR1,
    PCIE_BAR2,
    PCIE_BAR3,
    PCIE_BAR4,
    PCIE_BAR5,
    PCIE_BAR_MAX,
};

struct pcie_mem_map_struct
{
    unsigned char mem_domain;
    unsigned int pcie_bar_index;
    unsigned int pcie_bar_table_base_addr;
    unsigned int pcie_bar_table_high_addr;
    unsigned int pcie_bar_table_offset;
};
#endif

/**
 * Type of memory to access (cf rwnx_plat.get_address)
 *
 * @RWNX_ADDR_CPU To access memory of the embedded CPU
 * @RWNX_ADDR_SYSTEM To access memory/registers of one subsystem of the
 * embedded system
 *
 */
enum rwnx_platform_addr {
    RWNX_ADDR_CPU,
    RWNX_ADDR_AON,
    RWNX_ADDR_MAC_PHY,
    RWNX_ADDR_SYSTEM,
    RWNX_ADDR_MAX,
};

struct rwnx_hw;

/**
 * struct rwnx_plat - Operation pointers for RWNX PCI platform
 *
 * @pci_dev: pointer to pci dev
 * @enabled: Set if embedded platform has been enabled (i.e. fw loaded and
 *          ipc started)
 * @enable: Configure communication with the fw (i.e. configure the transfers
 *         enable and register interrupt)
 * @disable: Stop communication with the fw
 * @deinit: Free all ressources allocated for the embedded platform
 * @get_address: Return the virtual address to access the requested address on
 *              the platform.
 * @ack_irq: Acknowledge the irq at link level.
 * @get_config_reg: Return the list (size + pointer) of registers to restore in
 * order to reload the platform while keeping the current configuration.
 *
 * @priv Private data for the link driver
 */
struct rwnx_plat {
#if defined(CONFIG_RWNX_USB_MODE)
    struct usb_device *usb_dev;
    struct auc_hif_ops *hif_ops;
#elif defined(CONFIG_RWNX_SDIO_MODE)
    struct device *dev;
    struct aml_hif_sdio_ops *hif_ops;
#else
    struct pci_dev *pci_dev;
#endif

    bool enabled;

    int (*enable)(struct rwnx_hw *rwnx_hw);
    int (*disable)(struct rwnx_hw *rwnx_hw);
    void (*deinit)(struct rwnx_plat *rwnx_plat);
    u8 *(*get_address)(struct rwnx_plat *rwnx_plat, int addr_name,
                       unsigned int offset);
    void (*ack_irq)(struct rwnx_plat *rwnx_plat);
    int (*get_config_reg)(struct rwnx_plat *rwnx_plat, const u32 **list);
    u8 priv[0] __aligned(sizeof(void *));
};

#if defined(CONFIG_RWNX_USB_MODE)
#define RWNX_ADDR(plat, base, offset)           \
    plat->get_address(plat, base, offset)

#define RWNX_REG_READ(plat, base, offset)               \
    plat->hif_ops->hi_read_word((unsigned int)(unsigned long)RWNX_ADDR(plat, base, offset), USB_EP4)

#define RWNX_REG_WRITE(val, plat, base, offset)         \
    plat->hif_ops->hi_write_word((unsigned int)(unsigned long)RWNX_ADDR(plat, base, offset), val, USB_EP4)

static inline struct device *rwnx_platform_get_dev(struct rwnx_plat *rwnx_plat)
{
    return &(rwnx_plat->usb_dev->dev);
}

#elif defined(CONFIG_RWNX_SDIO_MODE)

#define RWNX_ADDR(plat, base, offset)           \
    plat->get_address(plat, base, offset)

#define RWNX_REG_READ(plat, base, offset)               \
    plat->hif_ops->hi_read_ipc_word((unsigned int)(unsigned long)RWNX_ADDR(plat, base, offset))

#define RWNX_REG_WRITE(val, plat, base, offset)         \
    plat->hif_ops->hi_write_ipc_word((unsigned int)(unsigned long)RWNX_ADDR(plat, base, offset), val)

static inline struct device *rwnx_platform_get_dev(struct rwnx_plat *rwnx_plat)
{
    return rwnx_plat->dev;
}
#else

#define RWNX_ADDR(plat, base, offset)           \
    plat->get_address(plat, base, offset)

#define RWNX_REG_READ(plat, base, offset)               \
    readl(plat->get_address(plat, base, offset))

#define RWNX_REG_WRITE(val, plat, base, offset)         \
    writel(val, plat->get_address(plat, base, offset))

struct rwnx_pci
{
    u8 *pci_bar0_vaddr;
    u8 *pci_bar1_vaddr;
    u8 *pci_bar2_vaddr;
    u8 *pci_bar3_vaddr;
    u8 *pci_bar4_vaddr;
    u8 *pci_bar5_vaddr;
};

static inline struct device *rwnx_platform_get_dev(struct rwnx_plat *rwnx_plat)
{
    return &(rwnx_plat->pci_dev->dev);
}
static inline unsigned int rwnx_platform_get_irq(struct rwnx_plat *rwnx_plat)
{
    return rwnx_plat->pci_dev->irq;
}
u8* rwnx_pci_get_map_address(struct net_device *dev, unsigned int offset);
#endif


int rwnx_platform_init(struct rwnx_plat *rwnx_plat, void **platform_data);
void rwnx_platform_deinit(struct rwnx_hw *rwnx_hw);

int rwnx_platform_on(struct rwnx_hw *rwnx_hw, void *config);
void rwnx_platform_off(struct rwnx_hw *rwnx_hw, void **config);

int rwnx_platform_register_drv(void);
void rwnx_platform_unregister_drv(void);

#endif /* _RWNX_PLAT_H_ */
