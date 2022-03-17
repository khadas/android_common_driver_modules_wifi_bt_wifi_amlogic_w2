/**
 ******************************************************************************
 *
 * @file rwnx_v7.c - Support for v7 platform
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#include "rwnx_v7.h"


struct rwnx_v7
{
    u8 *pci_bar0_vaddr;
    u8 *pci_bar1_vaddr;
    u8 *pci_bar2_vaddr;
    u8 *pci_bar3_vaddr;
    u8 *pci_bar4_vaddr;
    u8 *pci_bar5_vaddr;
};

u8* ipc_basic_address= 0;


static void rwnx_v7_platform_deinit(struct rwnx_plat_pci *rwnx_plat_pci)
{
    struct rwnx_v7 *rwnx_v7 = (struct rwnx_v7 *)rwnx_plat_pci->priv;

    pci_disable_device(rwnx_plat_pci->pci_dev);
    iounmap(rwnx_v7->pci_bar0_vaddr);
    iounmap(rwnx_v7->pci_bar1_vaddr);
    pci_release_regions(rwnx_plat_pci->pci_dev);
    pci_clear_master(rwnx_plat_pci->pci_dev);
    pci_disable_msi(rwnx_plat_pci->pci_dev);
    kfree(rwnx_plat_pci);
}

/**
 * rwnx_v7_platform_init - Initialize the DINI platform
 *
 * @pci_dev PCI device
 * @rwnx_plat Pointer on struct rwnx_stat * to be populated
 *
 * @return 0 on success, < 0 otherwise
 *
 * Allocate and initialize a rwnx_plat structure for the dini platform.
 */
int rwnx_v7_platform_init(struct pci_dev *pci_dev, struct rwnx_plat_pci **rwnx_plat_pci)
{
    struct rwnx_v7 *rwnx_v7;
    u16 pci_cmd = 0;
    int ret = 0;

    *rwnx_plat_pci = kzalloc(sizeof(struct rwnx_plat_pci) + sizeof(struct rwnx_v7),
                        GFP_KERNEL);
    printk("%s:%d \n", __func__, __LINE__);
    if (!*rwnx_plat_pci)
        return -ENOMEM;

    rwnx_v7 = (struct rwnx_v7 *)(*rwnx_plat_pci)->priv;

    /* Hotplug fixups */
    pci_read_config_word(pci_dev, PCI_COMMAND, &pci_cmd);
    pci_cmd |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR;
    pci_write_config_word(pci_dev, PCI_COMMAND, pci_cmd);
    pci_write_config_byte(pci_dev, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES >> 2);

    printk("%s:%d \n", __func__, __LINE__);
    if ((ret = pci_enable_device(pci_dev))) {
        dev_err(&(pci_dev->dev), "pci_enable_device failed\n");
        goto out_enable;
    }

    pci_set_master(pci_dev);

    if ((ret = pci_request_regions(pci_dev, KBUILD_MODNAME))) {
        dev_err(&(pci_dev->dev), "pci_request_regions failed\n");
        goto out_request;
    }

    if (pci_enable_msi(pci_dev))
    {
        dev_err(&(pci_dev->dev), "pci_enable_msi failed\n");
        goto out_msi;

    }

    if (!(rwnx_v7->pci_bar0_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 0))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 0);
        ret = -ENOMEM;
        goto out_bar0;
    }
    printk("%s:%d, bar0 %x\n", __func__, __LINE__, rwnx_v7->pci_bar0_vaddr);
    if (!(rwnx_v7->pci_bar1_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 1))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 1);
        ret = -ENOMEM;
        goto out_bar1;
    }
    printk("%s:%d, bar1 %x\n", __func__, __LINE__, rwnx_v7->pci_bar1_vaddr);

    if (!(rwnx_v7->pci_bar2_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 2))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 2);
        ret = -ENOMEM;
        goto out_bar2;
    }
    printk("%s:%d, bar2 %x\n", __func__, __LINE__, rwnx_v7->pci_bar2_vaddr);

    if (!(rwnx_v7->pci_bar3_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 3))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 3);
        ret = -ENOMEM;
        goto out_bar3;
    }
    printk("%s:%d, bar3 %x\n", __func__, __LINE__, rwnx_v7->pci_bar3_vaddr);

    if (!(rwnx_v7->pci_bar4_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 4))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 4);
        ret = -ENOMEM;
        goto out_bar4;
    }
    printk("%s:%d, bar4 %x\n", __func__, __LINE__, rwnx_v7->pci_bar4_vaddr);

    if (!(rwnx_v7->pci_bar5_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 5))) {
        dev_err(&(pci_dev->dev), "pci_ioremap_bar(%d) failed\n", 5);
        ret = -ENOMEM;
        goto out_bar5;
    }
    ipc_basic_address = rwnx_v7->pci_bar5_vaddr;

    (*rwnx_plat_pci)->deinit = rwnx_v7_platform_deinit;

    return 0;

  out_bar5:
     iounmap(rwnx_v7->pci_bar4_vaddr);
     iounmap(rwnx_v7->pci_bar3_vaddr);
     iounmap(rwnx_v7->pci_bar2_vaddr);
     iounmap(rwnx_v7->pci_bar1_vaddr);
     iounmap(rwnx_v7->pci_bar0_vaddr);
  out_bar4:
    iounmap(rwnx_v7->pci_bar3_vaddr);
    iounmap(rwnx_v7->pci_bar2_vaddr);
    iounmap(rwnx_v7->pci_bar1_vaddr);
    iounmap(rwnx_v7->pci_bar0_vaddr);
  out_bar3:
    iounmap(rwnx_v7->pci_bar2_vaddr);
    iounmap(rwnx_v7->pci_bar1_vaddr);
    iounmap(rwnx_v7->pci_bar0_vaddr);
  out_bar2:
    iounmap(rwnx_v7->pci_bar1_vaddr);
    iounmap(rwnx_v7->pci_bar0_vaddr);

  out_bar1:
    iounmap(rwnx_v7->pci_bar0_vaddr);
  out_bar0:
    pci_disable_msi(pci_dev);
  out_msi:
    pci_release_regions(pci_dev);
  out_request:
    pci_clear_master(pci_dev);
    pci_disable_device(pci_dev);
  out_enable:
    kfree(*rwnx_plat_pci);
    return ret;
}

EXPORT_SYMBOL(ipc_basic_address);

