/**
****************************************************************************************
*
* @file aml_task.c
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief task API implementation.
*
****************************************************************************************
*/

#include "aml_task.h"
#include "aml_wq.h"
#include "aml_scc.h"
#include "ipc_host.h"
#include "aml_msg_tx.h"

#ifdef CONFIG_AML_USE_TASK

#define AML_TASK_FUNC(data, name, func) do { \
    struct aml_hw *aml_hw = (struct aml_hw *)data; \
    struct sched_param param = {0};   \
    param.sched_priority = AML_TASK_PRI; \
    sched_setscheduler(current, SCHED_FIFO, &param); \
    while (!aml_hw->name->task_quit) { \
        if (down_interruptible(&aml_hw->name->task_sem) != 0) break; \
        if (aml_hw->name->task_quit) break; \
        func(aml_hw); \
    } \
    complete_and_exit(&aml_hw->name->task_cmpl, 0); \
} while (0);

#define AML_TASK_INIT(aml_hw, name)  do { \
    aml_hw->name = kmalloc(sizeof(struct aml_task), GFP_KERNEL); \
    spin_lock_init(&aml_hw->name->lock); \
    sema_init(&aml_hw->name->task_sem, 0); \
    aml_hw->name->task_quit = 0; \
    aml_hw->name->task = kthread_run(aml_task_##name, aml_hw, "aml_task_"#name); \
    if (IS_ERR(aml_hw->name->task)) { kfree(aml_hw->name); } \
} while (0);

#define AML_TASK_DEINIT(aml_hw, name) do { \
    init_completion(&aml_hw->name->task_cmpl); \
    aml_hw->name->task_quit = 1; \
    up(&aml_hw->name->task_sem); \
    kthread_stop(aml_hw->name->task); \
    wait_for_completion(&aml_hw->name->task_cmpl); \
    kfree(aml_hw->name); \
} while (0);


int aml_task_irqhdlr(void *data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    struct sched_param param = {0};
    u32 status;

    param.sched_priority = AML_TASK_PRI;
    sched_setscheduler(current, SCHED_FIFO, &param);
    while (!aml_hw->irqhdlr->task_quit) {
        if (down_interruptible(&aml_hw->irqhdlr->task_sem) != 0) {
            enable_irq(aml_platform_get_irq(aml_hw->plat));
            break;
        }
        if (aml_hw->irqhdlr->task_quit) break;
        while ((status = ipc_host_get_status(aml_hw->ipc_env))) {
            ipc_host_irq_ext(aml_hw->ipc_env, status);
        }
        aml_spin_lock(&aml_hw->tx_lock);
        aml_hwq_process_all(aml_hw);
        aml_spin_unlock(&aml_hw->tx_lock);
        enable_irq(aml_platform_get_irq(aml_hw->plat));
        aml_hw->plat->ack_irq(aml_hw);
    }
    complete_and_exit(&aml_hw->irqhdlr->task_cmpl, 0);
    return 0;
}

void aml_rxdesc_hdlr(struct aml_hw *aml_hw)
{
    ipc_host_rxdesc_handler(aml_hw->ipc_env);
}

int aml_task_rxdesc(void *data)
{
    AML_TASK_FUNC(data, rxdesc, aml_rxdesc_hdlr);
    return 0;
}

void aml_txcfm_hdlr(struct aml_hw *aml_hw)
{
    ipc_host_txcfm_handler(aml_hw->ipc_env);
}


int aml_task_txcfm(void *data)
{
    AML_TASK_FUNC(data, txcfm, aml_txcfm_hdlr);
    return 0;
}

void aml_task_init(struct aml_hw *aml_hw)
{
    if (aml_bus_type != PCIE_MODE)
        return;

    AML_INFO("aml task init");
    AML_TASK_INIT(aml_hw, irqhdlr);
    AML_TASK_INIT(aml_hw, rxdesc);
    AML_TASK_INIT(aml_hw, txcfm);
}

void aml_task_deinit(struct aml_hw *aml_hw)
{
    if (aml_bus_type != PCIE_MODE)
        return;

    AML_INFO("aml task deinit");
    AML_TASK_DEINIT(aml_hw, irqhdlr);
    AML_TASK_DEINIT(aml_hw, rxdesc);
    AML_TASK_DEINIT(aml_hw, txcfm);
}
#endif
