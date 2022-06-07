/**
 ******************************************************************************
 *
 * @file rwnx_pci.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>

#include "rwnx_v7.h"
#include "usb_common.h"

#define PCI_VENDOR_ID_DINIGROUP              0x17DF
#define PCI_DEVICE_ID_DINIGROUP_DNV6_F2PCIE  0x1907

#define PCI_DEVICE_ID_XILINX_CEVA_VIRTEX7    0x8011

#ifndef CONFIG_RWNX_PLATFORM_AH212
#define PCI_DEVICE_ID_PLDA    0x0601
#define PCI_VENDOR_ID_PLDA    0x1B8E
#else
#define PCI_DEVICE_ID_PLDA    0x0
#define PCI_VENDOR_ID_PLDA    0x0
#endif
struct rwnx_plat_pci *g_rwnx_plat_pci;
unsigned char g_pci_driver_insmoded;
unsigned char g_pci_after_probe;


static const struct pci_device_id rwnx_pci_ids[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_DINIGROUP, PCI_DEVICE_ID_DINIGROUP_DNV6_F2PCIE)},
    {PCI_DEVICE(PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_XILINX_CEVA_VIRTEX7)},
    {PCI_DEVICE(0x1B8E, 0x0601)},
    {PCI_DEVICE(0x0, 0x0)},
    {0,}
};

#ifndef CONFIG_RWNX_FPGA_PCIE
struct pcie_mem_map_struct pcie_ep_addr_range[PCIE_TABLE_NUM] =
{
    // bar1 EP addr range
    {RWNX_ADDR_CPU,     PCIE_BAR2, PCIE_BAR2_TABLE1_EP_BASE_ADDR, PCIE_BAR2_TABLE1_EP_END_ADDR, PCIE_BAR2_TABLE1_OFFSET},
    {RWNX_ADDR_CPU,     PCIE_BAR2, PCIE_BAR2_TABLE2_EP_BASE_ADDR, PCIE_BAR2_TABLE2_EP_END_ADDR, PCIE_BAR2_TABLE2_OFFSET},
    {RWNX_ADDR_MAC_PHY, PCIE_BAR2, PCIE_BAR2_TABLE4_EP_BASE_ADDR, PCIE_BAR2_TABLE4_EP_END_ADDR, PCIE_BAR2_TABLE4_OFFSET},
    {RWNX_ADDR_MAC_PHY, PCIE_BAR2, PCIE_BAR2_TABLE5_EP_BASE_ADDR, PCIE_BAR2_TABLE5_EP_END_ADDR, PCIE_BAR2_TABLE5_OFFSET},

    // bar2 EP addr range
    {RWNX_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE0_EP_BASE_ADDR, PCIE_BAR4_TABLE0_EP_END_ADDR, PCIE_BAR4_TABLE0_OFFSET},
    {RWNX_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE2_EP_BASE_ADDR, PCIE_BAR4_TABLE2_EP_END_ADDR, PCIE_BAR4_TABLE2_OFFSET},
    {RWNX_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE3_EP_BASE_ADDR, PCIE_BAR4_TABLE3_EP_END_ADDR, PCIE_BAR4_TABLE3_OFFSET},
    {RWNX_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE4_EP_BASE_ADDR, PCIE_BAR4_TABLE4_EP_END_ADDR, PCIE_BAR4_TABLE4_OFFSET},
    {RWNX_ADDR_SYSTEM, PCIE_BAR4, PCIE_BAR4_TABLE5_EP_BASE_ADDR, PCIE_BAR4_TABLE5_EP_END_ADDR, PCIE_BAR4_TABLE5_OFFSET},
    {RWNX_ADDR_AON,    PCIE_BAR4, PCIE_BAR4_TABLE6_EP_BASE_ADDR, PCIE_BAR4_TABLE6_EP_END_ADDR, PCIE_BAR4_TABLE6_OFFSET},
};
#endif

/* Uncomment this for depmod to create module alias */
/* We don't want this on development platform */
//MODULE_DEVICE_TABLE(pci, rwnx_pci_ids);

static int rwnx_pci_probe(struct pci_dev *pci_dev,
                          const struct pci_device_id *pci_id)
{
    int ret = -ENODEV;

    printk("%s:%d %x\n", __func__, __LINE__, pci_id->vendor);

    if (pci_id->vendor == PCI_VENDOR_ID_XILINX) {
        printk("%s:%d %x\n", __func__, __LINE__, PCI_VENDOR_ID_XILINX);
        ret = rwnx_v7_platform_init(pci_dev, &g_rwnx_plat_pci);
    }
    else if ((pci_id->vendor == 0x1B8E) || (pci_id->vendor == 0))
    {
        printk("%s:%d pcie vendor id %x\n", __func__, __LINE__, pci_id->vendor);
        ret = rwnx_v7_platform_init(pci_dev, &g_rwnx_plat_pci);
    }

    if (ret)
        return ret;

    g_rwnx_plat_pci->pci_dev = pci_dev;
    g_pci_after_probe = 1;

    return ret;
}

static void rwnx_pci_remove(struct pci_dev *pci_dev)
{
    printk("%s:%d \n", __func__, __LINE__);
    g_rwnx_plat_pci->deinit(g_rwnx_plat_pci);
}
static int rwnx_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
    int ret;

    printk("%s\n", __func__);
    //rwnx_suspend_dump_cfgregs(bus, "BEFORE_EP_SUSPEND");
    pci_save_state(pdev);
    pci_enable_wake(pdev, PCI_D0, 1);
    if (pci_is_enabled(pdev))
        pci_disable_device(pdev);

    ret = pci_set_power_state(pdev, PCI_D3hot);
    if (ret) {
        ERROR_DEBUG_OUT("pci_set_power_state error %d\n", ret);
    }

    //rwnx_suspend_dump_cfgregs(bus, "AFTER_EP_SUSPEND");
    return ret;
}

static int rwnx_pci_resume(struct pci_dev *pdev)
{
    int err;
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

out:
    return err;
}

static struct pci_driver rwnx_pci_drv = {
    .name     = KBUILD_MODNAME,
    .id_table = rwnx_pci_ids,
    .probe    = rwnx_pci_probe,
    .remove   = rwnx_pci_remove,
    .suspend  = rwnx_pci_suspend,
    .resume   = rwnx_pci_resume,
};

int aml_pci_insmod(void)
{
    int err = 0;

    err = pci_register_driver(&rwnx_pci_drv);
    g_pci_driver_insmoded = 1;

    if(err) {
        printk("failed to register pci driver: %d \n", err);
    }

    printk("%s:%d \n", __func__, __LINE__);
    return err;
}

void aml_pci_rmmod(void)
{
    pci_unregister_driver(&rwnx_pci_drv);
    g_pci_driver_insmoded = 0;
    g_pci_after_probe = 0;

    printk("%s(%d) aml common driver rmsmod\n",__func__, __LINE__);
}


EXPORT_SYMBOL(aml_pci_insmod);
EXPORT_SYMBOL(g_rwnx_plat_pci);
EXPORT_SYMBOL(g_pci_driver_insmoded);
EXPORT_SYMBOL(g_pci_after_probe);
#ifndef CONFIG_RWNX_FPGA_PCIE
EXPORT_SYMBOL(pcie_ep_addr_range);
#endif
module_init(aml_pci_insmod);
module_exit(aml_pci_rmmod);

MODULE_LICENSE("GPL");

