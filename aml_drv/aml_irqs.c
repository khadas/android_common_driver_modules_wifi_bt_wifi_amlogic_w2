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
#include "aml_msg_tx.h"
#include "aml_scc.h"

int aml_irq_usb_hdlr(struct urb *urb)
{
    struct aml_hw *aml_hw = (struct aml_hw *)(urb->context);
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

int aml_task(void *data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    u32 status;
    int ret = 0;
    struct sched_param sch_param;

    sch_param.sched_priority = 93;
    sched_setscheduler(current,SCHED_FIFO,&sch_param);
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
            enable_irq(aml_hw->irq);

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

#ifdef CONFIG_AML_USE_TASK
#define AML_TASK_IRQHDLR(data, name, pri)   do { \
    struct aml_hw *aml_hw = (struct aml_hw *)data; \
    struct sched_param param = {0};   \
    u32 status; \
    param.sched_priority = pri; \
    sched_setscheduler(current, SCHED_FIFO, &param); \
    while (!aml_hw->name->task_quit) { \
        if (down_interruptible(&aml_hw->name->task_sem) != 0) { \
            enable_irq(aml_platform_get_irq(aml_hw->plat)); \
            break; \
        } \
        if (aml_hw->name->task_quit) break; \
        while ((status = ipc_host_get_status(aml_hw->ipc_env))) { \
            ipc_host_irq_ext(aml_hw->ipc_env, status); \
        } \
        aml_hw->plat->ack_irq(aml_hw); \
        aml_spin_lock(&aml_hw->tx_lock); \
        aml_hwq_process_all(aml_hw); \
        aml_spin_unlock(&aml_hw->tx_lock); \
        enable_irq(aml_platform_get_irq(aml_hw->plat)); \
    } \
    complete_and_exit(&aml_hw->name->task_cmpl, 0); \
} while (0);

void aml_handle_misc_events(struct aml_hw *aml_hw)
{
    if (aml_hw->send_sync_cmd == 1) {
        aml_send_sync_trace(aml_hw);
    }
    if (AML_SCC_BEACON_WAIT_DOWNLOAD()) {
        if (!aml_scc_change_beacon(aml_hw)) {
            AML_SCC_CLEAR_BEACON_UPDATE();
        }
    }
}
#define AML_TASK_FUNC(data, name, pri)   do { \
    struct aml_hw *aml_hw = (struct aml_hw *)data; \
    struct sched_param param = {0};   \
    param.sched_priority = pri; \
    sched_setscheduler(current, SCHED_FIFO, &param); \
    while (!aml_hw->name->task_quit) { \
        if (down_interruptible(&aml_hw->name->task_sem) != 0) break; \
        if (aml_hw->name->task_quit) break; \
        ipc_host_##name##_handler(aml_hw->ipc_env); \
    } \
    complete_and_exit(&aml_hw->name->task_cmpl, 0); \
} while (0);

#define AML_TASK_MISC(data, name, pri)   do { \
    struct aml_hw *aml_hw = (struct aml_hw *)data; \
    struct sched_param param = {0};   \
    param.sched_priority = pri; \
    sched_setscheduler(current, SCHED_FIFO, &param); \
    while (!aml_hw->name->task_quit) { \
        if (down_interruptible(&aml_hw->name->task_sem) != 0) break; \
        if (aml_hw->name->task_quit) break; \
        aml_handle_misc_events(aml_hw);\
    } \
    complete_and_exit(&aml_hw->name->task_cmpl, 0); \
} while (0);

int aml_pcie_irqhdlr_task(void *data)
{
    AML_TASK_IRQHDLR(data, irqhdlr, 20);
    return 0;
}

int aml_pcie_rxdesc_task(void *data)
{
    AML_TASK_FUNC(data, rxdesc, 20);
    return 0;
}

int aml_pcie_txcfm_task(void *data)
{
    AML_TASK_FUNC(data, txcfm, 20);
    return 0;
}

int aml_pcie_misc_task(void *data)
{
    AML_TASK_MISC(data, misc, 20);
}

#else
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
#endif

int aml_misc_task(void *data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    struct sched_param sch_param;

    sch_param.sched_priority = 91;
    sched_setscheduler(current,SCHED_FIFO,&sch_param);

    while (1) {
        /* wait for work */
        if (down_interruptible(&aml_hw->aml_misc_sem) != 0) {
            /* interrupted, exit */
            printk("%s:%d wait aml_misc_sem fail!\n", __func__, __LINE__);
            break;
        }

        aml_handle_misc_events(aml_hw);
    }

    return 0;
}

