/**
 ******************************************************************************
 *
 * @file aml_rps.h
 *
 * Copyright (C) Amlogic 2012-2023
 *
 ******************************************************************************
 */

#ifndef _AML_RPS_H_
#define _AML_RPS_H_

int aml_rps_cpus_enable(struct net_device *net);
static int aml_rps_map_set(struct netdev_rx_queue *queue, char *buf, size_t len);
int aml_xps_cpus_enable(struct net_device *net);
static int aml_xps_map_set(struct net_device *net, char *buf, size_t len);
int aml_rps_dev_flow_table_enable(struct net_device *net);
static int aml_rps_dev_flow_table_set(struct netdev_rx_queue *queue, char *buf, size_t len);
static void aml_rps_dev_flow_table_release(struct rcu_head *rcu);
static int aml_rps_sock_flow_sysctl_set(char *buffer);
int aml_rps_sock_flow_sysctl_enable(void);

#endif /* _AML_RPS_H_ */
