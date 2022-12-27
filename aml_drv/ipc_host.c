/**
 ******************************************************************************
 *
 * @file ipc_host.c
 *
 * @brief IPC module.
 *
 * Copyright (C) Amlogic 2011-2021
 *
 ******************************************************************************
 */

/*
 * INCLUDE FILES
 ******************************************************************************
 */
#include <linux/spinlock.h>
#include "aml_defs.h"
#include "aml_prof.h"
#include "reg_ipc_app.h"
#include "ipc_host.h"
#include "share_mem_map.h"
#include "ipc_shared.h"

/*
 * TYPES DEFINITION
 ******************************************************************************
 */

const int nx_txdesc_cnt[] =
{
    NX_TXDESC_CNT0,
    NX_TXDESC_CNT1,
    NX_TXDESC_CNT2,
    NX_TXDESC_CNT3,
    #if NX_TXQ_CNT == 5
    NX_TXDESC_CNT4,
    #endif
};

const int nx_txuser_cnt[] =
{
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    #if NX_TXQ_CNT == 5
    1,
    #endif
};


/*
 * FUNCTIONS DEFINITIONS
 ******************************************************************************
 */
/**
 * ipc_host_rxdesc_handler() - Handle the reception of a Rx Descriptor
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_RXDESC is set
 */
void ipc_host_rxdesc_handler(struct ipc_host_env_tag *env)
{
    // For profiling
    REG_SW_SET_PROFILING(env->pthis, SW_PROF_IRQ_E2A_RXDESC);

    // LMAC has triggered an IT saying that a reception has occurred.
    // Then we first need to check the validity of the current hostbuf, and the validity
    // of the next hostbufs too, because it is likely that several hostbufs have been
    // filled within the time needed for this irq handling
    while (1) {
        #ifdef CONFIG_AML_FULLMAC
        // call the external function to indicate that a RX descriptor is received
        if (env->cb.recv_data_ind(env->pthis, env->rxdesc[env->rxdesc_idx]) != 0)
        #else
        // call the external function to indicate that a RX packet is received
        if (env->cb.recv_data_ind(env->pthis, env->rxbuf[env->rxbuf_idx]) != 0)
        #endif //(CONFIG_AML_FULLMAC)
            break;
    }

    // For profiling
    REG_SW_CLEAR_PROFILING(env->pthis, SW_PROF_IRQ_E2A_RXDESC);
}

/**
 * ipc_host_radar_handler() - Handle the reception of radar events
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_RADAR is set
 */
static void ipc_host_radar_handler(struct ipc_host_env_tag *env)
{
#ifdef CONFIG_AML_RADAR
    struct aml_hw *aml_hw = env->pthis;
    struct radar_pulse_array_desc *pulses = env->radar[env->radar_idx]->addr;

    if (aml_bus_type != PCIE_MODE) {
        pulses = &aml_hw->g_pulses;
    }
    if (aml_bus_type == USB_MODE) {
        aml_hw->plat->hif_ops->hi_read_sram((unsigned char *)pulses,
            (unsigned char *)(unsigned long)(RADAR_EVENT_DESC_ARRAY + aml_hw->radar_pulse_index * 56),
            sizeof(struct radar_pulse_array_desc), USB_EP4);
    } else if (aml_bus_type == SDIO_MODE) {
        aml_hw->plat->hif_sdio_ops->hi_random_ram_read((unsigned char *)pulses,
            (unsigned char *)(unsigned long)(RADAR_EVENT_DESC_ARRAY + aml_hw->radar_pulse_index * 56),
            sizeof(struct radar_pulse_array_desc));
    }

    aml_spin_lock(&((struct aml_hw *)env->pthis)->radar.lock);
    while (env->cb.recv_radar_ind(env->pthis,
                                  env->radar[env->radar_idx]) == 0)
        ;
    aml_spin_unlock(&((struct aml_hw *)env->pthis)->radar.lock);
#endif /* CONFIG_AML_RADAR */
}

/**
 * ipc_host_unsup_rx_vec_handler() - Handle the reception of unsupported rx vector
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_UNSUP_RX_VEC is set
 */
static void ipc_host_unsup_rx_vec_handler(struct ipc_host_env_tag *env)
{
    while (env->cb.recv_unsup_rx_vec_ind(env->pthis,
                                         env->unsuprxvec[env->unsuprxvec_idx]) == 0)
        ;
}

/**
 * ipc_host_msg_handler() - Handler for firmware message
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_MSG is set
 */
static void ipc_host_msg_handler(struct ipc_host_env_tag *env)
{
    if (aml_bus_type != PCIE_MODE) {
        env->cb.recv_msg_ind(env->pthis, env->msgbuf[env->msgbuf_idx]);
    } else {
        while (env->cb.recv_msg_ind(env->pthis, env->msgbuf[env->msgbuf_idx]) == 0) {
            ;
        }
    }
}

static void ipc_usb_host_msgack_handler(struct ipc_host_env_tag *env)
{
    void *hostid = env->msga2e_hostid;
    struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;
    volatile struct ipc_a2e_msg msg_a2e_buf = {0};

    aml_hw->plat->hif_ops->hi_read_sram((unsigned char *)&msg_a2e_buf, (unsigned char *)&env->shared->msg_a2e_buf, sizeof(struct ipc_a2e_msg), USB_EP4);

    ASSERT_ERR(hostid);
    ASSERT_ERR(env->msga2e_cnt == (((struct lmac_msg *)(&msg_a2e_buf.msg))->src_id & 0xFF));

    env->msga2e_hostid = NULL;
    env->msga2e_cnt++;
    env->cb.recv_msgack_ind(env->pthis, hostid);
}

static void ipc_sdio_host_msgack_handler(struct ipc_host_env_tag *env)
{
    void *hostid = env->msga2e_hostid;
    struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;
    volatile struct ipc_a2e_msg msg_a2e_buf = {0};
    aml_hw->plat->hif_sdio_ops->hi_random_ram_read((unsigned char *)&msg_a2e_buf, (unsigned char *)&env->shared->msg_a2e_buf, sizeof(struct ipc_a2e_msg));
    ASSERT_ERR(hostid);
    ASSERT_ERR(env->msga2e_cnt == (((struct lmac_msg *)(&msg_a2e_buf.msg))->src_id & 0xFF));

    printk("%s %p\n", __func__, hostid);
    env->msga2e_hostid = NULL;
    env->msga2e_cnt++;
    env->cb.recv_msgack_ind(env->pthis, hostid);
}

/**
 * ipc_host_msgack_handler() - Handle the reception of message acknowledgement
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_MSG_ACK is set
 */
static void ipc_pci_host_msgack_handler(struct ipc_host_env_tag *env)
{
    void *hostid = env->msga2e_hostid;

    AML_INFO("a2e msg hostid=0x%lx count=%d\n", env->msga2e_hostid, env->msga2e_cnt);
    if (!hostid) {
        struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;
        struct aml_vif *aml_vif = aml_hw->vif_table[0];
        AML_INFO("error: a2e msg hostid is null\n");
        if (aml_vif != NULL) {
            struct net_device * dev = aml_hw->vif_table[0]->ndev;
            AML_INFO("0x6080001c 0x%x\n", aml_read_reg(dev, 0x6080001c));
            AML_INFO("0x60800100 0x%x\n", aml_read_reg(dev, 0x60800100));
            AML_INFO("0x6080000c 0x%x\n", aml_read_reg(dev, 0x6080000c));
            AML_INFO("0x60000004 0x%x\n", aml_read_reg(dev, 0x60000004));
        }
        return;
    }

    env->msga2e_hostid = NULL;
    env->msga2e_cnt++;
    env->cb.recv_msgack_ind(env->pthis, hostid);
}

static void ipc_host_msgack_handler(struct ipc_host_env_tag *env)
{
    if (aml_bus_type == USB_MODE) {
        ipc_usb_host_msgack_handler(env);
    } else if (aml_bus_type == SDIO_MODE) {
        ipc_sdio_host_msgack_handler(env);
    } else {
        ipc_pci_host_msgack_handler(env);
    }
}

/**
 * ipc_host_dbg_handler() - Handle the reception of Debug event
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_DBG is set
 */
static void ipc_host_dbg_handler(struct ipc_host_env_tag *env)
{
    if (aml_bus_type != PCIE_MODE) {
       env->cb.recv_dbg_ind(env->pthis, env->dbgbuf[env->dbgbuf_idx]);
    } else {
        while(env->cb.recv_dbg_ind(env->pthis,
            env->dbgbuf[env->dbgbuf_idx]) == 0);
    }
}

static void ipc_host_trace_handler(struct ipc_host_env_tag *env)
{
    if (aml_bus_type != PCIE_MODE) {
       env->cb.recv_trace_ind(env->pthis, 1);
    }
}

/**
 * ipc_host_tx_cfm_handler() - Handle the reception of TX confirmation
 *
 * @env: pointer to the IPC Host environment
 * @queue_idx: index of the hardware on which the confirmation has been received
 * @user_pos: index of the user position
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_TXCFM is set.
 * Process confirmations in order until:
 * - There is no more buffer pushed (no need to check confirmation in this case)
 * - The confirmation has not been updated by firmware
 */
static void ipc_host_tx_cfm_handler(struct ipc_host_env_tag *env,
                                    const int queue_idx, const int user_pos)
{
    while (!list_empty(&env->tx_hostid_pushed)) {
        if (env->cb.send_data_cfm(env->pthis, env->txcfm[env->txcfm_idx]))
            break;
        env->txcfm_idx++;
        if (env->txcfm_idx == IPC_TXCFM_CNT)
            env->txcfm_idx = 0;
    }
}

#ifdef CONFIG_AML_USE_TASK
void ipc_host_txcfm_handler(struct ipc_host_env_tag *env)
{
    struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;
    int i, j;

    aml_spin_lock(&aml_hw->tx_lock);
    for (i = 0; i < IPC_TXQUEUE_CNT; i++) {
        j = 0;
#ifdef CONFIG_AML_MUMIMO_TX
        for (; j < nx_txuser_cnt[i]; j++)
#endif
        {
            uint32_t q_bit = CO_BIT(j + i * CONFIG_USER_MAX + IPC_IRQ_E2A_TXCFM_POS);
            if (aml_hw->txcfm_status & q_bit) {
                ipc_host_tx_cfm_handler(env, i, j);
            }
        }
    }
    aml_hw->txcfm_status = 0;
    aml_spin_unlock(&aml_hw->tx_lock);
}
#endif


/**
 ******************************************************************************
 */
bool ipc_host_tx_frames_pending(struct ipc_host_env_tag *env)
{
    return !list_empty(&env->tx_hostid_pushed);
}

/**
 ******************************************************************************
 */
void *ipc_host_tx_flush(struct ipc_host_env_tag *env)
{
    struct ipc_hostid *tx_hostid;
    tx_hostid = list_first_entry_or_null(&env->tx_hostid_pushed,
                                         struct ipc_hostid, list);

    if (!tx_hostid)
        return NULL;

    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
    return tx_hostid->hostptr;
}

struct ipc_shared_rx_buf *g_host_rxbuf = NULL;
static void ipc_host_rxbuf_ext_init(struct ipc_shared_rx_buf *shared_host_rxbuf)
{
    unsigned int i, size, *dst;

    if (!shared_host_rxbuf)
        return;

    size = (unsigned int)sizeof(struct ipc_shared_rx_buf) * IPC_RXBUF_CNT_EXT;
    dst = (unsigned int *)shared_host_rxbuf;

    for (i = 0; i < size; i += 4) {
        writel(0, dst);
        dst++;
    }
    g_host_rxbuf = (struct ipc_shared_rx_buf *)shared_host_rxbuf;
}

struct ipc_shared_rx_desc *g_host_rxdesc = NULL;
static void ipc_host_rxdesc_ext_init(struct ipc_shared_rx_desc *shared_host_rxdesc)
{
    unsigned int i, size, *dst;

    if (!shared_host_rxdesc)
        return;

    size = (unsigned int)sizeof(struct ipc_shared_rx_desc) * IPC_RXDESC_CNT_EXT;
    dst = (unsigned int *)shared_host_rxdesc;

    for (i = 0; i < size; i += 4) {
        writel(0, dst);
        dst++;
    }
    g_host_rxdesc = (struct ipc_shared_rx_desc *)shared_host_rxdesc;
}


/**
 ******************************************************************************
 */
void ipc_host_init(struct ipc_host_env_tag *env,
                  struct ipc_host_cb_tag *cb,
                  struct ipc_shared_env_tag *shared_env_ptr,
                  struct ipc_shared_rx_buf *shared_host_rxbuf,
                  struct ipc_shared_rx_desc *shared_host_rxdesc,
                  void *pthis)
{
    unsigned int i;
    struct ipc_hostid *tx_hostid;

    // Reset the environments
#if 0
    /* check potential platform bug on multiple stores */
    memset(shared_env_ptr, 0, sizeof(struct ipc_shared_env_tag));
#else
    // Reset the IPC Shared memory
    if (aml_bus_type == PCIE_MODE) {
        unsigned int size = (unsigned int)sizeof(struct ipc_shared_env_tag);
        unsigned int *dst = (unsigned int *)shared_env_ptr;
        for (i=0; i < size; i+=4) {
            writel(0, dst++);
        }
    }
#endif

    ipc_host_rxbuf_ext_init(shared_host_rxbuf);
    ipc_host_rxdesc_ext_init(shared_host_rxdesc);

    // Reset the IPC Host environment
    memset(env, 0, sizeof(struct ipc_host_env_tag));

    // Initialize the shared environment pointer
    env->shared = shared_env_ptr;

    // Save the callbacks in our own environment
    env->cb = *cb;

    // Save the pointer to the register base
    env->pthis = pthis;

    // Initialize buffers numbers and buffers sizes needed for DMA Receptions
    env->rxbuf_nb = (aml_bus_type == PCIE_MODE) ? (IPC_RXBUF_CNT + IPC_RXBUF_CNT_EXT) : IPC_RXBUF_CNT;
#ifdef CONFIG_AML_FULLMAC
    env->rxdesc_nb = (aml_bus_type == PCIE_MODE) ? (IPC_RXDESC_CNT + IPC_RXDESC_CNT_EXT) : IPC_RXDESC_CNT;
#endif //(CONFIG_AML_FULLMAC)
    env->unsuprxvec_sz = max(sizeof(struct rx_vector_desc), (size_t) RADIOTAP_HDR_MAX_LEN) +
        RADIOTAP_HDR_VEND_MAX_LEN +  UNSUP_RX_VEC_DATA_LEN;

    // Initialize the pointer to the TX DMA descriptor arrays
    env->txdmadesc = shared_env_ptr->txdmadesc;

    INIT_LIST_HEAD(&env->tx_hostid_available);
    INIT_LIST_HEAD(&env->tx_hostid_pushed);
    tx_hostid = env->tx_hostid;
    for (i = 0; i < ARRAY_SIZE(env->tx_hostid); i++, tx_hostid++) {
        tx_hostid->hostid = i + 1; // +1 so that 0 is not a valid value
        list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
    }
}

/**
 ******************************************************************************
 */
void ipc_host_pattern_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    env->shared->pattern_addr = buf->dma_addr;
}

/**
 ******************************************************************************
 */
#ifdef DEBUG_CODE
extern uint32_t addr_null_happen;
struct debug_push_rxbuff_info debug_push_rxbuff[DEBUG_RX_BUF_CNT];
u16 debug_push_rxbuff_idx = 0;
void record_push_rx_buf(u32 dma_addr,u32 host_id, u16 rxbuf_idx)
{
    debug_push_rxbuff[debug_push_rxbuff_idx].addr = dma_addr;
    debug_push_rxbuff[debug_push_rxbuff_idx].idx = rxbuf_idx;
    debug_push_rxbuff[debug_push_rxbuff_idx].hostid = host_id;
    debug_push_rxbuff[debug_push_rxbuff_idx].time = jiffies;
    debug_push_rxbuff_idx++;
    if (debug_push_rxbuff_idx == DEBUG_RX_BUF_CNT) {
        debug_push_rxbuff_idx = 0;
    }
}
#endif

int ipc_host_rxbuf_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_rx_buf *host_rxbuf;

    if (aml_bus_type == PCIE_MODE) {
        host_rxbuf = g_host_rxbuf + env->rxbuf_idx;
    } else {
        host_rxbuf = (struct ipc_shared_rx_buf *)&env->shared->host_rxbuf[env->rxbuf_idx];
    }

    host_rxbuf->hostid = AML_RXBUFF_HOSTID_GET(buf);
    host_rxbuf->dma_addr = buf->dma_addr;
    if ((host_rxbuf->hostid == 0) || (host_rxbuf->hostid > AML_RXBUFF_MAX)) {
        AML_INFO("hostid invalid:%x", host_rxbuf->hostid);
    }

#ifdef DEBUG_CODE
    if (addr_null_happen) {
        printk("push rxbuf, idx:%d, host_id:0x%x, buf:%08x, addr:%08x, dma_addr:%08x\n",
                env->rxbuf_idx, host_rxbuf->hostid, buf, buf->addr, buf->dma_addr);
    }
    if ((aml_bus_type == PCIE_MODE) && (host_rxbuf->dma_addr != buf->dma_addr)) {
        printk("err: host_rxbuf=0x%x, dma_addr=0x%x, rxbuf_idx=%d, hostid=%d",
                host_rxbuf->dma_addr, buf->dma_addr, env->rxbuf_idx, host_rxbuf->hostid);
    }

    if (aml_bus_type == PCIE_MODE) {
        record_push_rx_buf(host_rxbuf->dma_addr, host_rxbuf->hostid, env->rxbuf_idx);
    }
#endif

    // Signal to the embedded CPU that at least one buffer is available
    ipc_app2emb_trigger_set(env->pthis, IPC_IRQ_A2E_RXBUF_BACK);

    // Increment the array index
    if (aml_bus_type == PCIE_MODE) {
        env->rxbuf_idx = (env->rxbuf_idx + 1) % (IPC_RXBUF_CNT + IPC_RXBUF_CNT_EXT);
    } else {
        env->rxbuf_idx = (env->rxbuf_idx + 1) % IPC_RXBUF_CNT;
    }

    return 0;
}

#ifdef CONFIG_AML_FULLMAC
/**
 ******************************************************************************
 */
int ipc_host_rxdesc_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;
    struct ipc_shared_rx_desc *host_rxdesc;

    if (env->rxdesc_idx < IPC_RXDESC_CNT) {
        host_rxdesc = (struct ipc_shared_rx_desc *)&shared_env->host_rxdesc[env->rxdesc_idx];
    } else if (g_host_rxdesc && (env->rxdesc_idx < (IPC_RXDESC_CNT + IPC_RXDESC_CNT_EXT))) {
        host_rxdesc = g_host_rxdesc + (env->rxdesc_idx - IPC_RXDESC_CNT);
    } else {
        AML_INFO("host push rxdesc idx is illegal");
        return -1;
    }

    host_rxdesc->dma_addr = buf->dma_addr;

    env->rxdesc[env->rxdesc_idx] = buf;

    // Signal to the embedded CPU that at least one descriptor is available
    ipc_app2emb_trigger_set(env->pthis, IPC_IRQ_A2E_RXDESC_BACK);
    if (aml_bus_type == PCIE_MODE) {
        env->rxdesc_idx = (env->rxdesc_idx + 1) % (IPC_RXDESC_CNT + IPC_RXDESC_CNT_EXT);
    } else {
        env->rxdesc_idx = (env->rxdesc_idx + 1) % IPC_RXDESC_CNT;
    }

    return 0;
}
#endif /* CONFIG_AML_FULLMAC */

/**
 ******************************************************************************
 */
int ipc_host_radar_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    // Copy the DMA address in the ipc shared memory
    shared_env->radarbuf_hostbuf[env->radar_idx] = buf->dma_addr;

    // Save Ipc buffer in host env
    env->radar[env->radar_idx] = buf;

    // Increment the array index
    env->radar_idx = (env->radar_idx + 1) % IPC_RADARBUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_unsuprxvec_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env_ptr = env->shared;

    shared_env_ptr->unsuprxvecbuf_hostbuf[env->unsuprxvec_idx] = buf->dma_addr;

    env->unsuprxvec[env->unsuprxvec_idx] = buf;

    env->unsuprxvec_idx = (env->unsuprxvec_idx + 1) % IPC_UNSUPRXVECBUF_CNT;

    return 0;
}

struct debug_push_msginfo debug_push_msgbug[DEBUG_MSGE2A_BUF_CNT];
u8 debug_push_idx = 0;
void record_push_msg_buf(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;
    debug_push_msgbug[debug_push_idx].addr = buf->dma_addr;
    debug_push_msgbug[debug_push_idx].next_addr = shared_env->msg_e2a_hostbuf_addr[(env->msgbuf_idx + 1) % IPC_MSGE2A_BUF_CNT];
    debug_push_msgbug[debug_push_idx].idx = env->msgbuf_idx;
    debug_push_msgbug[debug_push_idx].time = jiffies;
    debug_push_idx++;
    if (debug_push_idx == DEBUG_MSGE2A_BUF_CNT) {
        debug_push_idx = 0;
    }
}

/**
 ******************************************************************************
 */
int ipc_host_msgbuf_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    shared_env->msg_e2a_hostbuf_addr[env->msgbuf_idx] = buf->dma_addr;
    env->msgbuf[env->msgbuf_idx] = buf;
    if (shared_env->msg_e2a_hostbuf_addr[env->msgbuf_idx] != buf->dma_addr) {
        printk("error:msg_e2a_hostbuf_addr=0x%X,dma_addr=0x%x,msgbuf_idx=%d\n",shared_env->msg_e2a_hostbuf_addr[env->msgbuf_idx],buf->dma_addr,env->msgbuf_idx);
    }
    record_push_msg_buf(env,buf);
    env->msgbuf_idx = (env->msgbuf_idx + 1) % IPC_MSGE2A_BUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_dbgbuf_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    shared_env->dbg_hostbuf_addr[env->dbgbuf_idx] = buf->dma_addr;

    env->dbgbuf[env->dbgbuf_idx] = buf;

    env->dbgbuf_idx = (env->dbgbuf_idx + 1) % IPC_DBGBUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_txcfm_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    shared_env->txcfm_hostbuf_addr[env->txcfm_idx] = buf->dma_addr;
    env->txcfm[env->txcfm_idx] = buf;

    env->txcfm_idx++;
    if (env->txcfm_idx == IPC_TXCFM_CNT)
        env->txcfm_idx = 0;

    return 0;
}

/**
 ******************************************************************************
 */
void ipc_host_dbginfo_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    shared_env->la_dbginfo_addr = buf->dma_addr;
}

/**
 ******************************************************************************
 */
void ipc_host_txdesc_push(struct ipc_host_env_tag *env, struct aml_ipc_buf *buf)
{
    if (aml_bus_type == PCIE_MODE) {
        uint32_t dma_idx = env->txdmadesc_idx;
        volatile struct dma_desc *dmadesc_pushed;

        dmadesc_pushed = &env->txdmadesc[dma_idx++];

        // Write DMA address to the descriptor
        dmadesc_pushed->src = buf->dma_addr;

        wmb();

        if (dma_idx == IPC_TXDMA_DESC_CNT)
            env->txdmadesc_idx = 0;
        else
            env->txdmadesc_idx = dma_idx;
    }

    // trigger interrupt to firmware
    ipc_app2emb_trigger_setf(env->pthis, IPC_IRQ_A2E_TXDESC);
}

/**
 * ipc_host_tx_host_ptr_to_id() - Save and convert host pointer to host id
 *
 * @env: pointer to the IPC Host environment
 * @host_ptr: host pointer to save in the ipc_hostid element ()
 * @return: uint32_t value associated to this host buffer.
 *
 * Move a free ipc_hostid from the tx_hostid_available list to the tx_hostid_pushed list.
 * The element is initialized with the host pointer and the associated 32bits value is
 * returned.
 * It is expected that list tx_hostid_available contains at least one element.
 */
uint32_t ipc_host_tx_host_ptr_to_id(struct ipc_host_env_tag *env, void *host_ptr)
{
    struct ipc_hostid *tx_hostid;
    tx_hostid = list_first_entry_or_null(&env->tx_hostid_available,
                                         struct ipc_hostid, list);
    if (!tx_hostid)
        return 0;

    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_pushed);
    tx_hostid->hostptr = host_ptr;
    return tx_hostid->hostid;
}

/**
 * ipc_host_tx_host_id_to_ptr() - Retrieve host ptr from host id
 *
 * @env: pointer to the IPC Host environment
 * @hostid: hostid present in the confirmation
 * @return: pointer saved via ipc_host_tx_host_ptr_to_id()
 *
 * Allow to retrieve the host ptr (to the tx buffer) form the host id found in
 * the confirmation.
 * Move back the tx_hostid element from the tx_hostid_pushed list to the
 * tx_hostid_available list.
 */
void *ipc_host_tx_host_id_to_ptr(struct ipc_host_env_tag *env, uint32_t hostid)
{
    struct ipc_hostid *tx_hostid;

    if (unlikely(!hostid || (hostid > ARRAY_SIZE(env->tx_hostid))))
        return NULL;

    tx_hostid = &env->tx_hostid[hostid - 1];
    if (aml_bus_type != PCIE_MODE) {
        struct ipc_hostid *tx_hostid_tmp, *next;
        u8 find = 0;
        list_for_each_entry_safe(tx_hostid_tmp, next, &env->tx_hostid_pushed, list) {
            if (!memcmp(tx_hostid, tx_hostid_tmp, sizeof(struct ipc_hostid))) {
                find = 1;
            }
        }
        if (!find) {
            return NULL;
        }
    }
    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
    return tx_hostid->hostptr;
}

#ifdef CONFIG_AML_USE_TASK
void ipc_host_irq_ext(struct ipc_host_env_tag *env, uint32_t status)
{
    struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;

    ipc_emb2app_ack_clear(env->pthis, status);
    ipc_emb2app_status_get(env->shared);
    if (status & IPC_IRQ_E2A_RXDESC)
    {
        up(&aml_hw->rxdesc->task_sem);
    }
    if (status & IPC_IRQ_E2A_MSG_ACK)
    {
        ipc_host_msgack_handler(env);
    }
    if (status & IPC_IRQ_E2A_MSG)
    {
        ipc_host_msg_handler(env);
    }
    if (status & IPC_IRQ_E2A_TXCFM)
    {
#if 1
        int i;
        aml_spin_lock(&((struct aml_hw *)env->pthis)->tx_lock);
        for (i = 0; i < IPC_TXQUEUE_CNT; i++) {
            int j = 0;
#ifdef CONFIG_AML_MUMIMO_TX
            for (; j < nx_txuser_cnt[i]; j++)
#endif
            {
                uint32_t q_bit = CO_BIT(j + i * CONFIG_USER_MAX + IPC_IRQ_E2A_TXCFM_POS);
                if (status & q_bit) {
                    ipc_host_tx_cfm_handler(env, i, j);
                }
            }
        }
        aml_spin_unlock(&((struct aml_hw *)env->pthis)->tx_lock);
#ifdef CONFIG_AML_POWER_SAVE_MODE
        aml_allow_fw_sleep(((struct aml_hw *)env->pthis)->plat, PS_TX_START);
#endif
#else
        aml_hw->txcfm_status = status;
        up(&aml_hw->txcfm->task_sem);
#endif
    }
    if (status & IPC_IRQ_E2A_RADAR)
    {
        ipc_host_radar_handler(env);
    }
    if (status & IPC_IRQ_E2A_UNSUP_RX_VEC)
    {
        ipc_host_unsup_rx_vec_handler(env);
    }
    if (status & IPC_IRQ_E2A_DBG)
    {
        ipc_host_dbg_handler(env);
    }
}
#endif

/**
 ******************************************************************************
 */
void ipc_host_irq(struct ipc_host_env_tag *env, uint32_t status)
{
    // Acknowledge the pending interrupts
    if (aml_bus_type == PCIE_MODE)
        ipc_emb2app_ack_clear(env->pthis, status);

    // Optimized for only one IRQ at a time
    if (status & IPC_IRQ_E2A_RXDESC)
    {
        // handle the RX descriptor reception
        ipc_host_rxdesc_handler(env);
    }
    if (status & IPC_IRQ_E2A_MSG_ACK)
    {
        ipc_host_msgack_handler(env);
    }
    if (((status & IPC_IRQ_E2A_MSG) && (aml_bus_type == PCIE_MODE))
        || ((status & SDIO_IRQ_E2A_MSG) && (aml_bus_type != PCIE_MODE)))
    {
        ipc_host_msg_handler(env);
    }
    if (status & IPC_IRQ_E2A_TXCFM)
    {
        int i;

        if (aml_bus_type != PCIE_MODE) {
            aml_update_tx_cfm(env->pthis);
        } else {
            // handle the TX confirmation reception
            aml_spin_lock(&((struct aml_hw *)env->pthis)->tx_lock);
            for (i = 0; i < IPC_TXQUEUE_CNT; i++) {
                int j = 0;
                #ifdef CONFIG_AML_MUMIMO_TX
                for (; j < nx_txuser_cnt[i]; j++)
                #endif
                {
                    uint32_t q_bit = CO_BIT(j + i * CONFIG_USER_MAX + IPC_IRQ_E2A_TXCFM_POS);
                    if (status & q_bit) {
                        // handle the confirmation
                        ipc_host_tx_cfm_handler(env, i, j);
                    }
                }
            }
            aml_spin_unlock(&((struct aml_hw *)env->pthis)->tx_lock);
#ifdef CONFIG_AML_POWER_SAVE_MODE
            aml_allow_fw_sleep(((struct aml_hw *)env->pthis)->plat, PS_TX_START);
#endif
        }
    }

    if (status & IPC_IRQ_E2A_RADAR)
    {
        // handle the radar event reception
        ipc_host_radar_handler(env);
    }

    if (status & IPC_IRQ_E2A_UNSUP_RX_VEC)
    {
        // handle the unsupported rx vector reception
        ipc_host_unsup_rx_vec_handler(env);
    }

    if (((status & IPC_IRQ_E2A_DBG) && (aml_bus_type == PCIE_MODE))
        || ((status & SDIO_IRQ_E2A_DBG) && (aml_bus_type != PCIE_MODE)))
    {
        ipc_host_dbg_handler(env);
    }

    if (((status & SDIO_IRQ_E2A_TRACE) && (aml_bus_type != PCIE_MODE)))
    {
        ipc_host_trace_handler(env);
    }
}

/**
 ******************************************************************************
 */
int ipc_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, uint16_t len)
{
    uint8_t *src;
    int i; uint8_t *dst;
    struct aml_hw *aml_hw = (struct aml_hw *)env->pthis;

    REG_SW_SET_PROFILING(env->pthis, SW_PROF_IPC_MSGPUSH);

    ASSERT_ERR(!env->msga2e_hostid);
    ASSERT_ERR(round_up(len, 4) <= sizeof(env->shared->msg_a2e_buf.msg));

    // Copy the message into the IPC MSG buffer
    src = (uint8_t*)((struct aml_cmd *)msg_buf)->a2e_msg;
    dst = (uint8_t*)&(env->shared->msg_a2e_buf.msg);

    // Copy the message in the IPC queue
    if (aml_bus_type == USB_MODE) {
        aml_hw->plat->hif_ops->hi_write_sram((unsigned char *)src, (unsigned char *)&(env->shared->msg_a2e_buf.msg), len, USB_EP4);
    } else if (aml_bus_type == SDIO_MODE) {
        aml_hw->plat->hif_sdio_ops->hi_random_ram_write((unsigned char *)src, (unsigned char *)&(env->shared->msg_a2e_buf.msg), len);
    } else {
        for (i = 0; i < len; i ++ ) {
            *dst++ = *src++;
        }
    }

    AML_INFO("a2e msg hostid=0x%lx\n", msg_buf);
    env->msga2e_hostid = msg_buf;

    // Trigger the irq to send the message to EMB
    ipc_app2emb_trigger_set(env->pthis, IPC_IRQ_A2E_MSG);

    REG_SW_CLEAR_PROFILING(env->pthis, SW_PROF_IPC_MSGPUSH);

    return (0);
}

/**
 ******************************************************************************
 */
void ipc_host_enable_irq(struct ipc_host_env_tag *env, uint32_t value)
{
    // Enable the handled interrupts
    ipc_emb2app_unmask_set(env->pthis, value);
}

/**
 ******************************************************************************
 */
void ipc_host_disable_irq(struct ipc_host_env_tag *env, uint32_t value)
{
    // Enable the handled interrupts
    ipc_emb2app_unmask_clear(env->pthis, value);
}

/**
 ******************************************************************************
 */
uint32_t ipc_host_get_status(struct ipc_host_env_tag *env)
{
    volatile uint32_t status;

    status = ipc_emb2app_status_get(env->pthis);
    return status;
}

/**
 ******************************************************************************
 */
uint32_t ipc_host_get_rawstatus(struct ipc_host_env_tag *env)
{
    volatile uint32_t rawstatus;

    rawstatus = ipc_emb2app_rawstatus_get(env->pthis);

    return rawstatus;
}
