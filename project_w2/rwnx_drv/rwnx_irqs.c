/**
 ******************************************************************************
 *
 * @file rwnx_irqs.c
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */
#include <linux/interrupt.h>

#include "rwnx_defs.h"
#include "ipc_host.h"
#include "rwnx_prof.h"

#if !defined(CONFIG_RWNX_PCIE_MODE)
#if defined(CONFIG_RWNX_USB_MODE)
int rwnx_irq_hdlr(struct urb *urb)
{
    int ret = 0;
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)(urb->context);

    /*submit urb*/
    ret = usb_submit_urb(rwnx_hw->g_urb, GFP_ATOMIC);
    if (ret < 0) {
        ERROR_DEBUG_OUT("usb_submit_urb failed %d\n", ret);
        return ret;
    }

    up(&rwnx_hw->rwnx_task_sem);
    return IRQ_HANDLED;
}

#else

irqreturn_t rwnx_irq_hdlr(int irq, void *dev_id)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)dev_id;
    disable_irq_nosync(irq);
    up(&rwnx_hw->rwnx_task_sem);
    return IRQ_HANDLED;
}

#endif

int rwnx_task(void *data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    u32 status;

    while (1) {
        /* wait for work */
        if (down_interruptible(&rwnx_hw->rwnx_task_sem) != 0) {
            /* interrupted, exit */
            printk("%s:%d wait rwnx_task_sem fail!\n", __func__, __LINE__);
            break;
        }

        REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_RWNX_IPC_IRQ_HDLR);
        rwnx_hw->plat->ack_irq(rwnx_hw->plat);

        while ((status = ipc_host_get_status(rwnx_hw->ipc_env))) {
            /* All kinds of IRQs will be handled in one shot (RX, MSG, DBG, ...)
             * this will ack IPC irqs not the cfpga irqs */
            ipc_host_irq(rwnx_hw->ipc_env, status);
        }

        spin_lock_bh(&rwnx_hw->tx_lock);
        rwnx_hwq_process_all(rwnx_hw);
        spin_unlock_bh(&rwnx_hw->tx_lock);

#if defined(CONFIG_RWNX_SDIO_MODE)
        enable_irq(rwnx_hw->irq);
#endif

        REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_RWNX_IPC_IRQ_HDLR);
    }

    return 0;
}

#else
/**
 * rwnx_irq_hdlr - IRQ handler
 *
 * Handler registerd by the platform driver
 */
irqreturn_t rwnx_irq_hdlr(int irq, void *dev_id)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)dev_id;
    disable_irq_nosync(irq);
    tasklet_schedule(&rwnx_hw->task);
    return IRQ_HANDLED;
}

/**
 * rwnx_task - Bottom half for IRQ handler
 *
 * Read irq status and process accordingly
 */
void rwnx_task(unsigned long data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
    u32 status;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_RWNX_IPC_IRQ_HDLR);

    /* Ack unconditionnally in case ipc_host_get_status does not see the irq */
    rwnx_plat->ack_irq(rwnx_plat);

    while ((status = ipc_host_get_status(rwnx_hw->ipc_env))) {
        /* All kinds of IRQs will be handled in one shot (RX, MSG, DBG, ...)
         * this will ack IPC irqs not the cfpga irqs */
        ipc_host_irq(rwnx_hw->ipc_env, status);

        rwnx_plat->ack_irq(rwnx_plat);
    }

    spin_lock(&rwnx_hw->tx_lock);
    rwnx_hwq_process_all(rwnx_hw);
    spin_unlock(&rwnx_hw->tx_lock);

    enable_irq(rwnx_platform_get_irq(rwnx_plat));

    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_RWNX_IPC_IRQ_HDLR);
}
#endif
