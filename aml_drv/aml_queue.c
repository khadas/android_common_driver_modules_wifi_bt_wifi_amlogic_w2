/**
 ******************************************************************************
 *
 * @file aml_queue.c
 *
 * @brief
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#include "aml_queue.h"
#include "aml_defs.h"

struct misc_msg_t misc_msg[AML_MISC_Q_NUM] = {0,};
int aml_misc_queue_init(struct msg_q_t *q)
{
    q->wt = 0;
    q->rd = 0;
    q->max = AML_MISC_Q_NUM;
    q->size = sizeof(struct misc_msg_t);
    q->q_buf = misc_msg;
    spin_lock_init(&(q->misc_lock));
    return true;
}

int aml_misc_queue_deinit(struct msg_q_t *q)
{
    memset(q,0,sizeof(struct msg_q_t));
    memset(misc_msg,0,sizeof(misc_msg));
    return true;
}

int aml_enqueue(struct msg_q_t *q, void *msg)
{
    spin_lock_irq(&q->misc_lock);
    if ((q->wt + 1) % (q->max) == q->rd) {
        AML_INFO("queue full\n");
        spin_unlock_irq(&q->misc_lock);
        return false;
    }
    memcpy((q->q_buf + (q->size) * (q->wt)), msg, q->size);
    q->wt = INC_QUEUE_INDX(q->wt, q->max);
    spin_unlock_irq(&q->misc_lock);
    return true;
}

int aml_dequeue(struct msg_q_t *q, void *msg)
{
    spin_lock_irq(&q->misc_lock);
    if (q->wt == q->rd)
    {
        spin_unlock_irq(&q->misc_lock);
        return false;
    }
    memcpy(msg, q->q_buf + (q->size) * (q->rd), q->size);
    q->rd = INC_QUEUE_INDX(q->rd, q->max);
    spin_unlock_irq(&q->misc_lock);
    return true;
}

