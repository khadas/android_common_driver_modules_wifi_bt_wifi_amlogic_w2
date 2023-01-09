/**
****************************************************************************************
*
* @file aml_wq.h
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief Declaration of the workqueue API mechanism.
*
****************************************************************************************
*/

#ifndef __AML_WQ_H__
#define __AML_WQ_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include "aml_utils.h"
#include "aml_defs.h"

enum aml_wq_type {
    AML_WQ_RECY_NONE,
    AML_WQ_RECY_CMDCSH,
    AML_WQ_RECY_TXHANG,
    AML_WQ_RECY_PCIERR,
    AML_WQ_RECY_MAX,
};

struct aml_wq {
    struct list_head list;
    struct aml_hw *aml_hw;
    enum aml_wq_type id;
    uint8_t len;
    uint8_t data[0];
};

struct aml_wq *aml_wq_alloc(int len);
void aml_wq_add(struct aml_hw *aml_hw, struct aml_wq *aml_wq);
void aml_wq_del(struct aml_hw *aml_hw);
int aml_wq_init(struct aml_hw *aml_hw);
void aml_wq_deinit(struct aml_hw *aml_hw);

#endif
