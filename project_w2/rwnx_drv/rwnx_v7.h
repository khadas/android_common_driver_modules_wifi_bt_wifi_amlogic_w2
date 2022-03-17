/**
 ******************************************************************************
 *
 * @file rwnx_v7.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#ifndef _RWNX_V7_H_
#define _RWNX_V7_H_

#include <linux/pci.h>

/**
 * struct rwnx_plat_pci - Operation pointers for RWNX PCI platform
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
struct rwnx_plat_pci {
    struct pci_dev *pci_dev;
    bool enabled;

    int (*enable)(void *rwnx_hw);
    int (*disable)(void *rwnx_hw);
    void (*deinit)(struct rwnx_plat_pci *rwnx_plat_pci);
    u8* (*get_address)(struct rwnx_plat_pci *rwnx_plat_pci, int addr_name,
                       unsigned int offset);
    void (*ack_irq)(struct rwnx_plat_pci *rwnx_plat_pci);
    int (*get_config_reg)(struct rwnx_plat_pci *rwnx_plat_pci, const u32 **list);

    u8 priv[0] __aligned(sizeof(void *));
};


int rwnx_v7_platform_init(struct pci_dev *pci_dev,
                          struct rwnx_plat_pci **rwnx_plat);

#endif /* _RWNX_V7_H_ */
