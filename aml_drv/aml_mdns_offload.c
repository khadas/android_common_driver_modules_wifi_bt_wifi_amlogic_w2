/**
****************************************************************************************
*
* @file aml_mdns_offload.c
*
* Copyright (C) Amlogic, Inc. All rights reserved (2022-2023).
*
* @brief android mDNS offload
*
****************************************************************************************
*/

#include "aml_mdns_offload.h"

static u32_boolean setOffloadState(u32_boolean enabled)
{
    printk("enter: %s\n", __func__);
    return 0;
}

static void resetAll()
{
    printk("enter: %s\n", __func__);
}

static int addProtocolResponses(char *networkInterface,
    mdnsProtocolData *offloadData)
{
    printk("enter: %s\n", __func__);
    return -1;
}

static void removeProtocolResponses(int recordKey)
{
    printk("enter: %s\n", __func__);
}

static int getAndResetHitCounter(int recordKey)
{
    printk("enter: %s\n", __func__);
    return -1;
}

static int getAndResetMissCounter()
{
    printk("enter: %s\n", __func__);
    return -1;
}

static u32_boolean addToPassthroughList(char *networkInterface,
    char *qname)
{
    printk("enter: %s\n", __func__);
    return 0;
}

static void removeFromPassthroughList(char *networkInterface,
    char *qname)
{
    printk("enter: %s\n", __func__);
}

static void setPassthroughBehavior(char *networkInterface,
    passthroughBehavior behavior)
{
    printk("enter: %s\n", __func__);
}

ANDROID_MDNS_OFFLOAD_VENDOR_IMPL = {
    .setOffloadState = setOffloadState,
    .resetAll = resetAll,
    .addProtocolResponses = addProtocolResponses,
    .removeProtocolResponses = removeProtocolResponses,
    .getAndResetHitCounter = getAndResetHitCounter,
    .getAndResetMissCounter = getAndResetMissCounter,
    .addToPassthroughList = addToPassthroughList,
    .removeFromPassthroughList = removeFromPassthroughList,
    .setPassthroughBehavior = setPassthroughBehavior,
};

