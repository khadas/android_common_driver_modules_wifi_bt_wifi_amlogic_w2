/**
 ****************************************************************************************
 *
 * @file aml_strs.h
 *
 * @brief Miscellaneous debug strings
 *
 * Copyright (C) Amlogic 2014-2021
 *
 ****************************************************************************************
 */

#ifndef _AML_STRS_H_
#define _AML_STRS_H_

#ifdef CONFIG_AML_FHOST

#define AML_ID2STR(tag) "Cmd"

#else
#include "lmac_msg.h"

#define AML_ID2STR(tag) (((MSG_T(tag) < ARRAY_SIZE(aml_id2str)) &&        \
                           (aml_id2str[MSG_T(tag)]) &&          \
                           ((aml_id2str[MSG_T(tag)])[MSG_I(tag)])) ?   \
                          (aml_id2str[MSG_T(tag)])[MSG_I(tag)] : "unknown")

extern const char *const *aml_id2str[TASK_LAST_EMB + 1];
#endif /* CONFIG_AML_FHOST */

#endif /* _AML_STRS_H_ */
