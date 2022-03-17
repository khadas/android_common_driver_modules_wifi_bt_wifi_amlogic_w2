/**
 ******************************************************************************
 *
 * @file rwnx_pci.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#ifndef _RWNX_PCI_H_
#define _RWNX_PCI_H_

int rwnx_pci_register_drv(void);
void rwnx_pci_unregister_drv(void);
int aml_pci_insmod(void);
void aml_pci_rmmod(void);

#endif /* _RWNX_PCI_H_ */
