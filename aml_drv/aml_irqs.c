/**
 ******************************************************************************
 *
 * @file aml_irqs.c
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */
#include <linux/interrupt.h>
#include "aml_defs.h"
#include "ipc_host.h"
#include "aml_prof.h"

extern bool b_usb_suspend;
int aml_irq_usb_hdlr(struct urb *urb)
{
    struct aml_hw *aml_hw = (struct aml_hw *)(urb->context);

    if (b_usb_suspend)
    {
        return IRQ_HANDLED;
    }

    up(&aml_hw->aml_task_sem);
    return IRQ_HANDLED;
}

irqreturn_t aml_irq_sdio_hdlr(int irq, void *dev_id)
{
    struct aml_hw *aml_hw = (struct aml_hw *)dev_id;
    disable_irq_nosync(irq);
    up(&aml_hw->aml_task_sem);
    return IRQ_HANDLED;
}

void aml_irq_sdio_hdlr_for_pt(struct sdio_func *func)
{
    struct aml_hw *aml_hw = dev_get_drvdata(&func->dev);
    up(&aml_hw->aml_task_sem);
}

int aml_task(void *data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    u32 status;
    int ret = 0;
    struct sched_param sch_param;

    sch_param.sched_priority = 93;
#ifndef CONFIG_PT_MODE
    sched_setscheduler(current,SCHED_FIFO,&sch_param);
#endif
    while (1) {
        /* wait for work */
        if (down_interruptible(&aml_hw->aml_task_sem) != 0) {
            /* interrupted, exit */
            printk("%s:%d wait aml_task_sem fail!\n", __func__, __LINE__);
            break;
        }

        REG_SW_SET_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
        while ((status = aml_hw->plat->ack_irq(aml_hw))) {
            ipc_host_irq(aml_hw->ipc_env, status);
        }

        spin_lock_bh(&aml_hw->tx_lock);
        aml_hwq_process_all(aml_hw);
        spin_unlock_bh(&aml_hw->tx_lock);

        if (aml_bus_type == SDIO_MODE) {
#ifndef CONFIG_PT_MODE
             enable_irq(aml_hw->irq);
#endif
        } else if (aml_bus_type == USB_MODE) {
            ret = usb_submit_urb(aml_hw->g_urb, GFP_ATOMIC);
            if (ret < 0) {
                ERROR_DEBUG_OUT("usb_submit_urb failed %d\n", ret);
                return ret;
            }
        }

        REG_SW_CLEAR_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
    }

    return 0;
}


/**
 * aml_irq_hdlr - IRQ handler
 *
 * Handler registerd by the platform driver
 */
irqreturn_t aml_irq_pcie_hdlr(int irq, void *dev_id)
{
    struct aml_hw *aml_hw = (struct aml_hw *)dev_id;
    disable_irq_nosync(irq);
#ifdef CONFIG_AML_USE_TASK
    up(&aml_hw->irqhdlr->task_sem);
#else
    tasklet_schedule(&aml_hw->task);
#endif
    return IRQ_HANDLED;
}

/**
 * aml_task - Bottom half for IRQ handler
 *
 * Read irq status and process accordingly
 */
void aml_pcie_task(unsigned long data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    struct aml_plat *aml_plat = aml_hw->plat;
    u32 status;

    REG_SW_SET_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);

    /* Ack unconditionnally in case ipc_host_get_status does not see the irq */
    aml_plat->ack_irq(aml_hw);

    while ((status = ipc_host_get_status(aml_hw->ipc_env))) {
        /* All kinds of IRQs will be handled in one shot (RX, MSG, DBG, ...)
         * this will ack IPC irqs not the cfpga irqs */
        ipc_host_irq(aml_hw->ipc_env, status);

        aml_plat->ack_irq(aml_hw);
    }

    aml_spin_lock(&aml_hw->tx_lock);
    aml_hwq_process_all(aml_hw);
    aml_spin_unlock(&aml_hw->tx_lock);

    enable_irq(aml_platform_get_irq(aml_plat));
    REG_SW_CLEAR_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
}
