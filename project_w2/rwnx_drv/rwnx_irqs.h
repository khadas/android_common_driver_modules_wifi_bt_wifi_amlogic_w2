/**
 ******************************************************************************
 *
 * @file rwnx_irqs.h
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#ifndef _RWNX_IRQS_H_
#define _RWNX_IRQS_H_

#include <linux/interrupt.h>

/* IRQ handler to be registered by platform driver */
int rwnx_irq_usb_hdlr(void *data);
int rwnx_task(void *data);

irqreturn_t rwnx_irq_sdio_hdlr(int irq, void *dev_id);


irqreturn_t rwnx_irq_pcie_hdlr(int irq, void *dev_id);
void rwnx_pcie_task(unsigned long data);
#endif /* _RWNX_IRQS_H_ */
