/**
 ******************************************************************************
 *
 * @file aml_pci.c
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>

#include "aml_w2_v7.h"
#include "usb_common.h"
#include "aml_interface.h"
#ifdef CONFIG_AML_POWER_SAVE_MODE
#include "chip_intf_reg.h"
#endif

#define W2p_VENDOR_AMLOGIC_EFUSE 0x1F35
#define W2p_PRODUCT_AMLOGIC_EFUSE 0x0602

#define W2pRevB_PRODUCT_AMLOGIC_EFUSE 0x0642

struct aml_plat_pci *g_aml_plat_pci;
unsigned char g_pci_driver_insmoded;
unsigned char g_pci_after_probe;
unsigned char g_pci_shutdown;

uint32_t aml_pci_read_for_bt(int base, u32 offset);

static const struct pci_device_id aml_pci_ids[] =
{
    {PCI_DEVICE(0x0, 0x0)},
    {PCI_DEVICE(W2p_VENDOR_AMLOGIC_EFUSE, W2p_PRODUCT_AMLOGIC_EFUSE)},
    {PCI_DEVICE(W2p_VENDOR_AMLOGIC_EFUSE, W2pRevB_PRODUCT_AMLOGIC_EFUSE)},
    {0,}
};

#ifndef CONFIG_AML_FPGA_PCIE
struct pcie_mem_map_struct pcie_ep_addr_range[PCIE_TABLE_NUM] =
{
    // bar1 EP addr range
    {AML_ADDR_CPU,     PCIE_BAR2, PCIE_BAR2_TABLE1_EP_BASE_ADDR, PCIE_BAR2_TABLE1_EP_END_ADDR, PCIE_BAR2_TABLE1_OFFSET},
    {AML_ADDR_CPU,     PCIE_BAR2, PCIE_BAR2_TABLE2_EP_BASE_ADDR, PCIE_BAR2_TABLE2_EP_END_ADDR, PCIE_BAR2_TABLE2_OFFSET},
    {AML_ADDR_MAC_PHY, PCIE_BAR2, PCIE_BAR2_TABLE4_EP_BASE_ADDR, PCIE_BAR2_TABLE4_EP_END_ADDR, PCIE_BAR2_TABLE4_OFFSET},
    {AML_ADDR_MAC_PHY, PCIE_BAR2, PCIE_BAR2_TABLE5_EP_BASE_ADDR, PCIE_BAR2_TABLE5_EP_END_ADDR, PCIE_BAR2_TABLE5_OFFSET},

    // bar2 EP addr range
    {AML_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE0_EP_BASE_ADDR, PCIE_BAR4_TABLE0_EP_END_ADDR, PCIE_BAR4_TABLE0_OFFSET},
    {AML_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE2_EP_BASE_ADDR, PCIE_BAR4_TABLE2_EP_END_ADDR, PCIE_BAR4_TABLE2_OFFSET},
    {AML_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE3_EP_BASE_ADDR, PCIE_BAR4_TABLE3_EP_END_ADDR, PCIE_BAR4_TABLE3_OFFSET},
    {AML_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE4_EP_BASE_ADDR, PCIE_BAR4_TABLE4_EP_END_ADDR, PCIE_BAR4_TABLE4_OFFSET},
    {AML_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE5_EP_BASE_ADDR, PCIE_BAR4_TABLE5_EP_END_ADDR, PCIE_BAR4_TABLE5_OFFSET},
    {AML_ADDR_AON,    PCIE_BAR4, PCIE_BAR4_TABLE6_EP_BASE_ADDR, PCIE_BAR4_TABLE6_EP_END_ADDR, PCIE_BAR4_TABLE6_OFFSET},
};
#endif

/* Uncomment this for depmod to create module alias */
/* We don't want this on development platform */
//MODULE_DEVICE_TABLE(pci, aml_pci_ids);

static int aml_pci_probe(struct pci_dev *pci_dev,
                          const struct pci_device_id *pci_id)
{
    int ret = -ENODEV;

    printk("%s:%d %x\n", __func__, __LINE__, pci_id->vendor);

    if (pci_id->vendor == PCI_VENDOR_ID_XILINX) {
        printk("%s:%d %x\n", __func__, __LINE__, PCI_VENDOR_ID_XILINX);
        ret = aml_v7_platform_init(pci_dev, &g_aml_plat_pci);
    }
    else if ((pci_id->vendor == 0) || (pci_id->vendor == W2p_VENDOR_AMLOGIC_EFUSE) || (pci_id->vendor == W2pRevB_PRODUCT_AMLOGIC_EFUSE))
    {
        printk("%s:%d pcie vendor id %x\n", __func__, __LINE__, pci_id->vendor);
        ret = aml_v7_platform_init(pci_dev, &g_aml_plat_pci);
    }

    if (ret)
        return ret;

    g_aml_plat_pci->pci_dev = pci_dev;
    g_pci_after_probe = 1;

    return ret;
}

static void aml_pci_remove(struct pci_dev *pci_dev)
{
    printk("%s:%d \n", __func__, __LINE__);
    g_aml_plat_pci->deinit(g_aml_plat_pci);
}
static int aml_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
    int ret;

    printk("%s\n", __func__);
    //aml_suspend_dump_cfgregs(bus, "BEFORE_EP_SUSPEND");
    pci_save_state(pdev);
    pci_enable_wake(pdev, PCI_D0, 1);
    if (pci_is_enabled(pdev))
        pci_disable_device(pdev);

    ret = pci_set_power_state(pdev, PCI_D3hot);
    if (ret) {
        ERROR_DEBUG_OUT("pci_set_power_state error %d\n", ret);
    }
    printk("%s ok exit\n", __func__);
    //aml_suspend_dump_cfgregs(bus, "AFTER_EP_SUSPEND");
    return ret;
}

static int aml_pci_resume(struct pci_dev *pdev)
{
    int err;
#ifdef CONFIG_AML_POWER_SAVE_MODE
    unsigned int wake_flag;
#endif
    printk("%s\n", __func__);
    pci_restore_state(pdev);

    err = pci_enable_device(pdev);
    if (err) {
        ERROR_DEBUG_OUT("pci_enable_device error %d \n", err);
        goto out;
    }

    pci_set_master(pdev);
    err = pci_set_power_state(pdev, PCI_D0);
    if (err) {
        ERROR_DEBUG_OUT("pci_set_power_state error %d \n", err);
        goto out;
    }
#ifdef CONFIG_AML_POWER_SAVE_MODE
    wake_flag = aml_pci_read_for_bt(AML_ADDR_AON, RG_AON_A55);
    printk("%s %d wake_flag = 0x%x\n", __func__, __LINE__, wake_flag);
    while (!((wake_flag != 0xffffffff) && (wake_flag & BIT(0))))
    {
        err = pci_set_power_state(pdev, PCI_D0);
        if (err) {
            ERROR_DEBUG_OUT("pci_set_power_state error %d \n", err);
            goto out;
        }
        wake_flag = aml_pci_read_for_bt(AML_ADDR_AON, RG_AON_A55);
        udelay(10);
    }
#endif
    printk("%s ok exit\n", __func__);
out:
    return err;
}

static void aml_pci_shutdown(struct pci_dev *pdev)
{
    g_pci_shutdown = 1;

    if (pci_is_enabled(pdev)) {
        msleep(100);
        pci_disable_device(pdev);
    }
    printk("%s\n", __func__);
}

static struct pci_driver aml_pci_drv = {
    .name     = KBUILD_MODNAME,
    .id_table = aml_pci_ids,
    .probe    = aml_pci_probe,
    .remove   = aml_pci_remove,
    .suspend  = aml_pci_suspend,
    .resume   = aml_pci_resume,
    .shutdown = aml_pci_shutdown,
};

int aml_pci_insmod(void)
{
    int err = 0;

    err = pci_register_driver(&aml_pci_drv);
    g_pci_driver_insmoded = 1;
    g_pci_shutdown = 0;

    if (err) {
        printk("failed to register pci driver: %d \n", err);
    }

    printk("%s:%d \n", __func__, __LINE__);
    return err;
}

void aml_pci_rmmod(void)
{
    pci_unregister_driver(&aml_pci_drv);
    g_pci_driver_insmoded = 0;
    g_pci_after_probe = 0;

    printk("%s(%d) aml common driver rmsmod\n",__func__, __LINE__);
}

static u8* aml_pci_get_address_for_bt(struct aml_plat_pci *aml_plat, int addr_name,
                               unsigned int offset)
{
#ifndef CONFIG_AML_FPGA_PCIE
    unsigned int i;
    unsigned int addr;
#endif
    struct aml_v7 *aml_pci = (struct aml_v7 *)aml_plat->priv;

    if (WARN(addr_name >= AML_ADDR_MAX, "Invalid address %d", addr_name))
        return NULL;

#ifdef CONFIG_AML_FPGA_PCIE

    if (addr_name == AML_ADDR_CPU) //0x00000000-0x0007ffff (ICCM)
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, aml_pci->pci_bar4_vaddr + offset);
        return aml_pci->pci_bar4_vaddr + offset;
    }
    else if (addr_name == AML_ADDR_MAC_PHY) //0x00a00000-0x00afffff
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, aml_pci->pci_bar3_vaddr + offset);
        return aml_pci->pci_bar3_vaddr + offset - 0x00a00000;
    }
    else if (addr_name == AML_ADDR_AON)// 0x00c00000 - 0x00ffffff (AON & DCCM)
    {
        printk("%s:%d, address %x\n", __func__, __LINE__, aml_pci->pci_bar2_vaddr + offset);
        return aml_pci->pci_bar2_vaddr + offset - 0x00c00000;
    }
    else if (addr_name == AML_ADDR_SYSTEM)
    {
        if (offset >= IPC_REG_BASE_ADDR)
        {
            printk("%s:%d, bar5 %x, address %x\n", __func__, __LINE__, aml_pci->pci_bar5_vaddr, aml_pci->pci_bar5_vaddr + offset - IPC_REG_BASE_ADDR);
            return aml_pci->pci_bar5_vaddr + offset - IPC_REG_BASE_ADDR;
        }
        else
        {
            printk("%s:%d, address %x\n", __func__, __LINE__, aml_pci->pci_bar0_vaddr + offset);
            return aml_pci->pci_bar0_vaddr + offset;
        }
    }
    else
    {
        printk("%s:%d, error addr_name\n", __func__,__LINE__);
        return NULL;
    }

#else

    if (addr_name == AML_ADDR_SYSTEM)
    {
        addr = offset + PCIE_BAR4_TABLE0_EP_BASE_ADDR;
    }
    else
    {
        addr = offset;
    }

    for (i = 0; i < PCIE_TABLE_NUM; i++)
    {
        if ((addr_name == pcie_ep_addr_range[i].mem_domain) &&
            (addr >= pcie_ep_addr_range[i].pcie_bar_table_base_addr) &&
            (addr <= pcie_ep_addr_range[i].pcie_bar_table_high_addr))
        {
            if (pcie_ep_addr_range[i].pcie_bar_index == PCIE_BAR2)
            {
                return aml_pci->pci_bar2_vaddr + pcie_ep_addr_range[i].pcie_bar_table_offset + (addr - pcie_ep_addr_range[i].pcie_bar_table_base_addr);
            }
            else
            {
                return aml_pci->pci_bar4_vaddr + pcie_ep_addr_range[i].pcie_bar_table_offset + (addr - pcie_ep_addr_range[i].pcie_bar_table_base_addr);
            }
        }
    }

    printk("%s:%d, addr(0x%x) or addr_name(0x%x) err\n", __func__,__LINE__, offset, addr_name);
    return NULL;

#endif //CONFIG_AML_FPGA_PCIE
}

uint32_t aml_pci_read_for_bt(int base, u32 offset)
{
    u8 *addr;
    addr = aml_pci_get_address_for_bt(g_aml_plat_pci, base, offset);

    return readl(addr);
}

void aml_pci_write_for_bt(u32 val, int base, u32 offset)
{
    u8 *addr;
    addr = aml_pci_get_address_for_bt(g_aml_plat_pci, base, offset);

    writel(val, addr);
}

EXPORT_SYMBOL(aml_pci_read_for_bt);
EXPORT_SYMBOL(aml_pci_write_for_bt);

EXPORT_SYMBOL(aml_pci_insmod);
EXPORT_SYMBOL(aml_pci_rmmod);
EXPORT_SYMBOL(g_aml_plat_pci);
EXPORT_SYMBOL(g_pci_driver_insmoded);
EXPORT_SYMBOL(g_pci_after_probe);
EXPORT_SYMBOL(g_pci_shutdown);
#ifndef CONFIG_AML_FPGA_PCIE
EXPORT_SYMBOL(pcie_ep_addr_range);
#endif
