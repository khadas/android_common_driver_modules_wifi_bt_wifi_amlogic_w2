/**
****************************************************************************************
*
* @file aml_wq.c
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief workqueue API implementation.
*
****************************************************************************************
*/

#include "aml_wq.h"
#include "aml_recy.h"


struct aml_wq *aml_wq_alloc(int len)
{
    struct aml_wq *aml_wq = NULL;
    int size = sizeof(struct aml_wq) + len;

    aml_wq = kzalloc(size, GFP_ATOMIC);
    if (aml_wq) {
        INIT_LIST_HEAD(&aml_wq->list);
        aml_wq->len = len;
    }
    return aml_wq;
}

static struct aml_wq *aml_wq_get(struct aml_hw *aml_hw)
{
    struct aml_wq *aml_wq = NULL;

    spin_lock_bh(&aml_hw->wq_lock);
    if (!list_empty(&aml_hw->work_list)) {
        aml_wq = list_first_entry(&aml_hw->work_list,
                struct aml_wq, list);
        list_del(&aml_wq->list);
    }
    spin_unlock_bh(&aml_hw->wq_lock);

    return aml_wq;
}

void aml_wq_add(struct aml_hw *aml_hw, struct aml_wq *aml_wq)
{
    spin_lock_bh(&aml_hw->wq_lock);
    list_add_tail(&aml_wq->list, &aml_hw->work_list);
    spin_unlock_bh(&aml_hw->wq_lock);

    if (!work_pending(&aml_hw->work))
        queue_work(aml_hw->wq, &aml_hw->work);
}

void aml_wq_del(struct aml_hw *aml_hw)
{
    struct aml_wq *aml_wq, *tmp;

    spin_lock_bh(&aml_hw->wq_lock);
    list_for_each_entry_safe(aml_wq, tmp, &aml_hw->work_list, list) {
        list_del(&aml_wq->list);
        kfree(aml_wq);
    }
    spin_unlock_bh(&aml_hw->wq_lock);

    flush_work(&aml_hw->work);
}

static void aml_wq_doit(struct work_struct *work)
{
    struct aml_wq *aml_wq = NULL;
    struct aml_hw *aml_hw = container_of(work, struct aml_hw, work);

    while (1) {
        aml_wq = aml_wq_get(aml_hw);
        if (!aml_wq)
            return;

        AML_INFO("wq type(%d) do it", aml_wq->id);
        switch (aml_wq->id) {
#ifdef CONFIG_AML_RECOVERY
            case AML_WQ_RECY_CMDCSH:
                aml_recy_cmdcsh_doit(aml_hw);
                break;

            case AML_WQ_RECY_TXHANG:
                aml_recy_txhang_doit(aml_hw);
                break;

            case AML_WQ_RECY_PCIERR:
                aml_recy_pcierr_doit(aml_hw);
                break;
#endif
        default:
            AML_INFO("wq type(%d) unknow", aml_wq->id);
            break;
        }
        kfree(aml_wq);
    }
}

int aml_wq_init(struct aml_hw *aml_hw)
{
    AML_INFO("workqueue init");
    spin_lock_init(&aml_hw->wq_lock);
    INIT_LIST_HEAD(&aml_hw->work_list);
    INIT_WORK(&aml_hw->work, aml_wq_doit);

    aml_hw->wq = alloc_ordered_workqueue("w2_wq",
            WQ_HIGHPRI | WQ_CPU_INTENSIVE |
            WQ_MEM_RECLAIM);
    if (!aml_hw->wq) {
        AML_INFO("wq create failed");
        return -ENOMEM;
    }
    return 0;
}

void aml_wq_deinit(struct aml_hw *aml_hw)
{
    struct aml_wq *aml_wq, *tmp;

    AML_INFO("workqueue deinit");
    cancel_work_sync(&aml_hw->work);

    spin_lock_bh(&aml_hw->wq_lock);
    list_for_each_entry_safe(aml_wq, tmp, &aml_hw->work_list, list) {
        list_del(&aml_wq->list);
        kfree(aml_wq);
    }
    spin_unlock_bh(&aml_hw->wq_lock);

    flush_workqueue(aml_hw->wq);
    destroy_workqueue(aml_hw->wq);
}
