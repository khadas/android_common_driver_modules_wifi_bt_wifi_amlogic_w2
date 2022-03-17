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

#define PCI_VENDOR_ID_DINIGROUP              0x17DF
#define PCI_DEVICE_ID_DINIGROUP_DNV6_F2PCIE  0x1907

#define PCI_DEVICE_ID_XILINX_CEVA_VIRTEX7    0x8011

struct rwnx_plat_pci *g_rwnx_plat_pci;
unsigned char g_pci_driver_insmoded;
unsigned char g_pci_after_probe;


static const struct pci_device_id rwnx_pci_ids[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_DINIGROUP, PCI_DEVICE_ID_DINIGROUP_DNV6_F2PCIE)},
    {PCI_DEVICE(PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_XILINX_CEVA_VIRTEX7)},
    {0,}
};


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

static struct pci_driver rwnx_pci_drv = {
    .name     = KBUILD_MODNAME,
    .id_table = rwnx_pci_ids,
    .probe    = rwnx_pci_probe,
    .remove   = rwnx_pci_remove
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

module_init(aml_pci_insmod);
module_exit(aml_pci_rmmod);

MODULE_LICENSE("GPL");

