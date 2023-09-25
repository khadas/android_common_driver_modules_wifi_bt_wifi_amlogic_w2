/**
 ******************************************************************************
 *
 * @file aml_rps.c
 *
 * @brief Entry point of the AML driver
 *
 * Copyright (C) Amlogic 2012-2023
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/inetdevice.h>
#include <net/cfg80211.h>
#include <net/ip.h>
#include <linux/etherdevice.h>
#include <net/addrconf.h>
#include "aml_rps.h"
#include "aml_utils.h"

static int aml_rps_map_set(struct netdev_rx_queue *queue, char *buf, size_t len)
{
    struct rps_map *old_map, *map;
    cpumask_var_t mask;
    int err, cpu, i;
    static DEFINE_MUTEX(rps_map_mutex);

    if (!alloc_cpumask_var(&mask, GFP_KERNEL)) {
        AML_INFO("alloc_cpumask_var fail.\n");
        return -ENOMEM;
    }

    err = bitmap_parse(buf, len, cpumask_bits(mask), nr_cpumask_bits);
    if (err) {
        free_cpumask_var(mask);
        AML_INFO("bitmap_parse fail");
        return err;
    }

    map = kzalloc(max_t(unsigned int,
                RPS_MAP_SIZE(cpumask_weight(mask)), L1_CACHE_BYTES), GFP_KERNEL);
    if (!map) {
        free_cpumask_var(mask);
        AML_INFO("map malloc fail.\n");
        return -ENOMEM;
    }

    i = 0;
    for_each_cpu_and(cpu, mask, cpu_online_mask)
        map->cpus[i++] = cpu;
    if (i) {
        map->len = i;
    } else {
        kfree(map);
        map = NULL;
        free_cpumask_var(mask);
        AML_INFO("mapping cpu fail");
        return -1;
    }

    mutex_lock(&rps_map_mutex);
    old_map = rcu_dereference_protected(queue->rps_map, mutex_is_locked(&rps_map_mutex));
    rcu_assign_pointer(queue->rps_map, map);
    if (map)
        static_branch_inc(&rps_needed);
    if (old_map)
        static_branch_dec(&rps_needed);
    mutex_unlock(&rps_map_mutex);

    if (old_map)
        kfree_rcu(old_map, rcu);
    free_cpumask_var(mask);

    return map->len;
}

int aml_rps_cpus_enable(struct net_device *net)
{
    char* rps_buf = "ff";
    int rx_idx = 0;

    if (net && net->_rx) {
        AML_INFO("set rps_cpus as [%s]\n", rps_buf);
        for (rx_idx = 0; rx_idx < net->num_rx_queues; rx_idx++)
            aml_rps_map_set(net->_rx + rx_idx, rps_buf, strlen(rps_buf));
    }
    return 0;
}

static int aml_xps_map_set(struct net_device *net, char *buf, size_t len)
{
    cpumask_var_t mask;
    int tx_idx = 0;
    int err = 0;

    if (!alloc_cpumask_var(&mask, GFP_KERNEL)) {
        AML_INFO("alloc_cpumask_var fail.\n");
        return -ENOMEM;
    }

    err = bitmap_parse(buf, len, cpumask_bits(mask), nr_cpumask_bits);
    if (err) {
        free_cpumask_var(mask);
        AML_INFO("bitmap_parse fail.\n");
        return err;
    }

    for (tx_idx = 0; (tx_idx < net->num_tx_queues) && (err == 0); tx_idx++)
        err = netif_set_xps_queue(net, mask, tx_idx);
    free_cpumask_var(mask);

    if (err != 0)
        AML_INFO("XPS mapping cpu fail\n");
    return err;
}

int aml_xps_cpus_enable(struct net_device *net)
{
    char* xps_buf = "ff";

    if (net) {
        AML_INFO("set xps_cpus as [%s]\n", xps_buf);
        aml_xps_map_set(net, xps_buf, strlen(xps_buf));
    }
    return 0;
}

static void aml_rps_dev_flow_table_release(struct rcu_head *rcu)
{
    struct rps_dev_flow_table *table = container_of(rcu, struct rps_dev_flow_table, rcu);

    vfree(table);
}

static int aml_rps_dev_flow_table_set(struct netdev_rx_queue *queue, char *buf, size_t len)
{
    unsigned long mask, count;
    struct rps_dev_flow_table *table, *old_table;
    static DEFINE_SPINLOCK(rps_dev_flow_lock);
    int rc;

    if (!capable(CAP_NET_ADMIN))
    {
        AML_INFO("check capable fail.\n");
        return -EPERM;
    }

    rc = kstrtoul(buf, 0, &count);
    if (rc < 0)
        return rc;

    if (count) {
        mask = count - 1;
        /* mask = roundup_pow_of_two(count) - 1;
         * without overflows...
         */
        while ((mask | (mask >> 1)) != mask)
            mask |= (mask >> 1);
        /* On 64 bit arches, must check mask fits in table->mask (u32),
         * and on 32bit arches, must check
         * RPS_DEV_FLOW_TABLE_SIZE(mask + 1) doesn't overflow.
         */
#if BITS_PER_LONG > 32
        if (mask > (unsigned long)(u32)mask)
        {
            AML_INFO("check mask fail %x\n", mask);
            return -EINVAL;
        }
#else
        if (mask > (ULONG_MAX - RPS_DEV_FLOW_TABLE_SIZE(1)) / sizeof(struct rps_dev_flow)) {
            AML_INFO("check mask fail %x, max %x\n", mask,
                    (ULONG_MAX - RPS_DEV_FLOW_TABLE_SIZE(1)) / sizeof(struct rps_dev_flow));
            /* Enforce a limit to prevent overflow */
            return -EINVAL;
        }
#endif
        table = vmalloc(RPS_DEV_FLOW_TABLE_SIZE(mask + 1));
        if (!table)
        {
            AML_INFO("alloc table fail\n");
            return -ENOMEM;
        }
        table->mask = mask;
        for (count = 0; count <= mask; count++)
            table->flows[count].cpu = RPS_NO_CPU;
    } else {
        table = NULL;
    }

    spin_lock(&rps_dev_flow_lock);
    old_table = rcu_dereference_protected(queue->rps_flow_table,
            lockdep_is_held(&rps_dev_flow_lock));
    rcu_assign_pointer(queue->rps_flow_table, table);
    spin_unlock(&rps_dev_flow_lock);

    if (old_table)
        call_rcu(&old_table->rcu, aml_rps_dev_flow_table_release);

    return len;
}

int aml_rps_dev_flow_table_enable(struct net_device *net)
{
    char* rps_dev_flow = "4096";
    int rx_idx = 0;

    if (net && net->_rx) {
        AML_INFO("set rps_cpus as [%s]\n", rps_dev_flow);
        for (rx_idx = 0; rx_idx < net->num_rx_queues; rx_idx++)
            aml_rps_dev_flow_table_set(net->_rx + rx_idx, rps_dev_flow, strlen(rps_dev_flow));
    }
    return 0;
}

static int aml_rps_sock_flow_sysctl_set(char *buffer)
{
    unsigned int orig_size;
    unsigned long size = 0;
    int ret = 0, i;
    struct rps_sock_flow_table *orig_sock_table = NULL, *sock_table = NULL;
    static DEFINE_MUTEX(sock_flow_mutex);

    ret = kstrtoul(buffer, 0, &size);
    if (ret < 0 || size == 0)
        return -1;

    mutex_lock(&sock_flow_mutex);
    orig_sock_table = rcu_dereference_protected(rps_sock_flow_table, lockdep_is_held(&sock_flow_mutex));
    orig_size = orig_sock_table ? orig_sock_table->mask + 1 : 0;

    if (size > 1 << 29) {
        /* Enforce limit to prevent overflow */
        mutex_unlock(&sock_flow_mutex);
        AML_INFO("size is large %d\n", size);
        return -EINVAL;
    }
    size = roundup_pow_of_two(size);
    if (size != orig_size) {
        sock_table = vmalloc(RPS_SOCK_FLOW_TABLE_SIZE(size));
        if (!sock_table) {
            mutex_unlock(&sock_flow_mutex);
            AML_INFO("alloc table fail\n");
            return -ENOMEM;
        }
        rps_cpu_mask = roundup_pow_of_two(nr_cpu_ids) - 1;
        sock_table->mask = size - 1;
    } else {
        mutex_unlock(&sock_flow_mutex);
        AML_INFO("Done, new size is same as orig_size, %d\n", orig_size);
        return 0;
    }

    for (i = 0; i < size; i++)
        sock_table->ents[i] = RPS_NO_CPU;

    rcu_assign_pointer(rps_sock_flow_table, sock_table);
    if (sock_table) {
        static_branch_inc(&rps_needed);
        static_branch_inc(&rfs_needed);
    }
    if (orig_sock_table) {
        static_branch_dec(&rps_needed);
        static_branch_dec(&rfs_needed);
        synchronize_rcu();
        vfree(orig_sock_table);
    }
    mutex_unlock(&sock_flow_mutex);
    AML_INFO("Done, new size is [%d]\n", size);
    return 0;
}

int aml_rps_sock_flow_sysctl_enable(void)
{
    char* sock_flow = "16384";

    aml_rps_sock_flow_sysctl_set(sock_flow);
    return 0;
}
