/**
 ******************************************************************************
 *
 * @file aml_irqs.h
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */
#ifndef _AML_IRQS_H_
#define _AML_IRQS_H_

#include <linux/interrupt.h>

/* IRQ handler to be registered by platform driver */
int aml_irq_usb_hdlr(void *data);
int aml_task(void *data);
int aml_misc_task(void *data);

irqreturn_t aml_irq_sdio_hdlr(int irq, void *dev_id);
irqreturn_t aml_irq_pcie_hdlr(int irq, void *dev_id);
void aml_pcie_task(unsigned long data);

#ifdef CONFIG_AML_USE_TASK
int aml_pcie_irqhdlr_task(void *data);
int aml_pcie_rxdesc_task(void *data);
int aml_pcie_txcfm_task(void *data);
int aml_pcie_misc_task(void *data);
#endif

#endif /* _AML_IRQS_H_ */
