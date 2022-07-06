/**
 * rwnx_utils.c
 *
 * IPC utility function definitions
 *
 * Copyright (C) RivieraWaves 2012-2021
 */
#include <uapi/linux/sched/types.h>

#include "rwnx_utils.h"
#include "rwnx_defs.h"
#include "rwnx_rx.h"
#include "rwnx_tx.h"
#include "rwnx_msg_rx.h"
#include "rwnx_debugfs.h"
#include "rwnx_prof.h"
#include "ipc_host.h"
#include "share_mem_map.h"
#include "rwnx_prealloc.h"
#include "reg_ipc_app.h"

#ifdef CONFIG_RWNX_SOFTMAC
#define FW_STR  "lmac"
#elif defined CONFIG_RWNX_FULLMAC
#define FW_STR  "fmac"
#endif

/**
 * rwnx_ipc_buf_pool_alloc() - Allocate and push to fw a pool of IPC buffer.
 *
 * @rwnx_hw: Main driver structure
 * @pool: Pool to allocate
 * @nb: Size of the pool to allocate
 * @buf_size: Size of one pool element
 * @pool_name: Name of the pool
 * @push: Function to push one pool buffer to fw
 *
 * This function will allocate an array to store the list of IPC buffers,
 * a dma pool and @nb element in the dma pool.
 * Each buffer is initialized with '0' and then pushed to fw using the @push function.
 *
 * Return: 0 on success and <0 upon error. If error is returned any allocated
 * memory is NOT freed and rwnx_ipc_buf_pool_dealloc() must be called.
 */
#if (defined(CONFIG_RWNX_PCIE_MODE))
static int rwnx_ipc_buf_pool_alloc(struct rwnx_hw *rwnx_hw,
                                   struct rwnx_ipc_buf_pool *pool,
                                   int nb, size_t buf_size, char *pool_name,
                                   int (*push)(struct ipc_host_env_tag *,
                                               struct rwnx_ipc_buf *))
{
    struct rwnx_ipc_buf *buf;
    int i;

    pool->nb = 0;

    /* allocate buf array */
    pool->buffers = kmalloc(nb * sizeof(struct rwnx_ipc_buf), GFP_KERNEL);
    if (!pool->buffers) {
        dev_err(rwnx_hw->dev, "Allocation of buffer array for %s failed\n",
                pool_name);
        return -ENOMEM;
    }

    /* allocate dma pool */
    pool->pool = dma_pool_create(pool_name, rwnx_hw->dev, buf_size,
                                 cache_line_size(), 0);
    if (!pool->pool) {
        dev_err(rwnx_hw->dev, "Allocation of dma pool %s failed\n",
                pool_name);
        return -ENOMEM;
    }

    for (i = 0, buf = pool->buffers; i < nb; buf++, i++) {
        /* allocate a buffer */
        buf->size = buf_size;
        buf->addr = dma_pool_alloc(pool->pool, GFP_KERNEL, &buf->dma_addr);
        if (!buf->addr) {
            dev_err(rwnx_hw->dev, "Allocation of block %d/%d in %s failed\n",
                    (i + 1), nb, pool_name);
            return -ENOMEM;
        }
        pool->nb++;

        /* reset the buffer */
        memset(buf->addr, 0, buf_size);

        /* push it to FW */
        push(rwnx_hw->ipc_env, buf);
    }

    return 0;
}
#endif

/**
 * rwnx_ipc_buf_pool_dealloc() - Free all memory allocated for a pool
 *
 * @pool: Pool to free
 *
 * Must be call once after rwnx_ipc_buf_pool_alloc(), even if it returned
 * an error
 */
static void rwnx_ipc_buf_pool_dealloc(struct rwnx_ipc_buf_pool *pool)
{
    struct rwnx_ipc_buf *buf;
    int i;

    for (i = 0, buf = pool->buffers; i < pool->nb ; buf++, i++) {
        dma_pool_free(pool->pool, buf->addr, buf->dma_addr);
    }
    pool->nb = 0;

    if (pool->pool)
        dma_pool_destroy(pool->pool);
    pool->pool = NULL;

    if (pool->buffers)
        kfree(pool->buffers);
    pool->buffers = NULL;
}

/**
 * rwnx_ipc_buf_alloc - Alloc a single ipc buffer and MAP it for DMA access
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to allocate
 * @buf_size: Size of the buffer to allocate
 * @dir: DMA direction
 * @init: Pointer to initial data to write in buffer before DMA sync. Used
 * only if direction is DMA_TO_DEVICE and it must be at least @buf_size long
 *
 * It allocates a buffer, initializes it if @init is set, and map it for DMA
 * Use @rwnx_ipc_buf_dealloc when this buffer is no longer needed.
 *
 * @return: 0 on success and <0 upon error. If error is returned any allocated
 * memory has been freed.
 */
int rwnx_ipc_buf_alloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                       size_t buf_size, enum dma_data_direction dir, const void *init)
{
    buf->addr = kmalloc(buf_size, GFP_KERNEL);
    if (!buf->addr)
        return -ENOMEM;

    buf->size = buf_size;

    if ((dir == DMA_TO_DEVICE) && init) {
        memcpy(buf->addr, init, buf_size);
    }

#if (!defined(CONFIG_RWNX_SDIO_MODE))
    buf->dma_addr = dma_map_single(rwnx_hw->dev, buf->addr, buf_size, dir);
    if (dma_mapping_error(rwnx_hw->dev, buf->dma_addr)) {
        kfree(buf->addr);
        buf->addr = NULL;
        return -EIO;
    }
#endif
    return 0;
}

/**
 * rwnx_ipc_buf_prealloc - Requesting prealloc a single ipc buffer and MAP it for DMA access
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to allocate
 * @buf_size: Size of the buffer to allocate
 * @buf_type: Type of the buffer to allocate
 * @dir: DMA direction
 * @init: Pointer to initial data to write in buffer before DMA sync. Used
 * only if direction is DMA_TO_DEVICE and it must be at least @buf_size long
 *
 * It allocates a buffer, initializes it if @init is set, and map it for DMA
 * Use @rwnx_ipc_buf_dealloc when this buffer is no longer needed.
 *
 * @return: 0 on success and <0 upon error. If error is returned any allocated
 * memory has been freed.
 */
#ifdef CONFIG_RWNX_USE_PREALLOC_BUF
int rwnx_ipc_buf_prealloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                          size_t buf_size, int buf_type,
                          enum dma_data_direction dir, const void *init)
{
    size_t out_size;
    buf->addr = rwnx_prealloc_get(buf_type, buf_size, &out_size);
    if (!buf->addr)
        return -ENOMEM;

    buf->size = out_size;
    if ((dir == DMA_TO_DEVICE) && init) {
        memcpy(buf->addr, init, buf->size);
    }

#if (!defined(CONFIG_RWNX_SDIO_MODE))
    buf->dma_addr = dma_map_single(rwnx_hw->dev, buf->addr, buf->size, dir);
    if (dma_mapping_error(rwnx_hw->dev, buf->dma_addr)) {
        buf->addr = NULL;
        return -EIO;
    }
#endif
    return 0;
}
#endif

/**
 * rwnx_ipc_buf_dealloc() - Free memory allocated for a single ipc buffer
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to free
 *
 * IPC buffer must have been allocated by @rwnx_ipc_buf_alloc() or initialized
 * by @rwnx_ipc_buf_init() and pointing to a buffer allocated by kmalloc.
 */
void rwnx_ipc_buf_dealloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    if (!buf->addr)
        return;
    dma_unmap_single(rwnx_hw->dev, buf->dma_addr, buf->size, DMA_TO_DEVICE);
    kfree(buf->addr);
    buf->addr = NULL;
}

/**
 * rwnx_ipc_buf_a2e_init - Initialize an Application to Embedded IPC buffer
 * with a pre-allocated buffer
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to initialize
 * @data: Data buffer to use for the IPC buffer.
 * @buf_size: Size of the buffer the @data buffer
 *
 * Initialize the IPC buffer with the provided buffer and map it for DMA transfer.
 * The mapping direction is always DMA_TO_DEVICE as this an "a2e" buffer.
 * Use @rwnx_ipc_buf_dealloc() when this buffer is no longer needed.
 *
 * @return: 0 on success and <0 upon error. If error is returned the @data buffer
 * is freed.
 */
int rwnx_ipc_buf_a2e_init(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                          void *data, size_t buf_size)
{
    buf->addr = data;
    buf->size = buf_size;
#if (!defined(CONFIG_RWNX_SDIO_MODE))
    buf->dma_addr = dma_map_single(rwnx_hw->dev, buf->addr, buf_size,
                                   DMA_TO_DEVICE);
    if (dma_mapping_error(rwnx_hw->dev, buf->dma_addr)) {
        buf->addr = NULL;
        return -EIO;
    }
#endif
    return 0;
}

/**
 * rwnx_ipc_buf_release() - Release DMA mapping for an IPC buffer
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to release
 * @dir: DMA direction.
 *
 * This also "release" the IPC buffer structure (i.e. its addr field is reset)
 * so that it cannot be re-used except to map another buffer.
 */
void rwnx_ipc_buf_release(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                          enum dma_data_direction dir)
{
    if (!buf->addr)
        return;
    dma_unmap_single(rwnx_hw->dev, buf->dma_addr, buf->size, dir);
    buf->addr = NULL;
}

/**
 * rwnx_ipc_buf_e2a_sync() - Synchronize all (or part) of an IPC buffer before
 * reading content written by the embedded
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to sync
 * @len: Length to read, 0 means the whole buffer
 */
void rwnx_ipc_buf_e2a_sync(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                           size_t len)
{
    if (!len)
        len = buf->size;

    dma_sync_single_for_cpu(rwnx_hw->dev, buf->dma_addr, len, DMA_FROM_DEVICE);
}

/**
 * rwnx_ipc_buf_e2a_sync_back() - Synchronize back all (or part) of an IPC buffer
 * to allow embedded updating its content.
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to sync
 * @len: Length to sync back, 0 means the whole buffer
 *
 * Must be called after each call to rwnx_ipc_buf_e2a_sync() even if host didn't
 * modified the content of the buffer.
 */
void rwnx_ipc_buf_e2a_sync_back(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                            size_t len)
{
    if (!len)
        len = buf->size;

    dma_sync_single_for_device(rwnx_hw->dev, buf->dma_addr, len, DMA_FROM_DEVICE);
}

/**
 * rwnx_ipc_rxskb_alloc() - Allocate a skb for RX path
 *
 * @rwnx_hw: Main driver data
 * @buf: rwnx_ipc_buf structure to store skb address
 * @skb_size: Size of the buffer to allocate
 *
 * Allocate a skb for RX path, meaning that the data buffer is written by the firmware
 * and needs then to be DMA mapped.
 *
 * Note that even though the result is stored in a struct rwnx_ipc_buf, in this case the
 * rwnx_ipc_buf.addr points to skb structure whereas the rwnx_ipc_buf.dma_addr is the
 * DMA address of the skb data buffer (i.e. skb->data)
 */
static int rwnx_ipc_rxskb_alloc(struct rwnx_hw *rwnx_hw,
                                struct rwnx_ipc_buf *buf, size_t skb_size)
{
    struct sk_buff *skb = dev_alloc_skb(skb_size);

    if (unlikely(!skb)) {
        dev_err(rwnx_hw->dev, "Allocation of RX skb failed\n");
        buf->addr = NULL;
        return -ENOMEM;
    }

#if (!defined(CONFIG_RWNX_SDIO_MODE))
    buf->dma_addr = dma_map_single(rwnx_hw->dev, skb->data, skb_size,
                                   DMA_FROM_DEVICE);
    if (unlikely(dma_mapping_error(rwnx_hw->dev, buf->dma_addr))) {
        dev_err(rwnx_hw->dev, "DMA mapping of RX skb failed\n");
        dev_kfree_skb(skb);
        buf->addr = NULL;
        return -EIO;
    }
#endif

    buf->addr = skb;
    buf->size = skb_size;

    return 0;
}

/**
 * rwnx_ipc_rxskb_reset_pattern() - Reset pattern in a RX skb or unsupported
 * RX vector buffer
 *
 * @rwnx_hw: Main driver data
 * @buf: RX skb to reset
 * @pattern_offset: Pattern location, in byte from the start of the buffer
 *
 * Reset the pattern in a RX/unsupported RX vector skb buffer to inform embedded
 * that it has been processed by the host.
 * Pattern in a 32bit value.
 */
static void rwnx_ipc_rxskb_reset_pattern(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_ipc_buf *buf,
                                         size_t pattern_offset)
{
    struct sk_buff *skb = buf->addr;
    u32 *pattern = (u32 *)(skb->data + pattern_offset);

    *pattern = 0;
    dma_sync_single_for_device(rwnx_hw->dev, buf->dma_addr + pattern_offset,
                               sizeof(u32), DMA_FROM_DEVICE);
}

/**
 * rwnx_ipc_rxskb_dealloc() - Free a skb allocated for the RX path
 *
 * @rwnx_hw: Main driver data
 * @buf: Rx skb to free
 *
 * Free a RX skb allocated by @rwnx_ipc_rxskb_alloc
 */
static void rwnx_ipc_rxskb_dealloc(struct rwnx_hw *rwnx_hw,
                                   struct rwnx_ipc_buf *buf)
{
    if (!buf->addr)
        return;

    dma_unmap_single(rwnx_hw->dev, buf->dma_addr, buf->size, DMA_TO_DEVICE);
    dev_kfree_skb((struct sk_buff *)buf->addr);
    buf->addr = NULL;
}


/**
 * rwnx_ipc_unsup_rx_vec_elem_allocs() - Allocate and push an unsupported
 *                                       RX vector buffer for the FW
 *
 * @rwnx_hw: Main driver data
 * @elem: Pointer to the skb elem that will contain the address of the buffer
 */
int rwnx_ipc_unsuprxvec_alloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    int err;

    err = rwnx_ipc_rxskb_alloc(rwnx_hw, buf, rwnx_hw->ipc_env->unsuprxvec_sz);
    if (err)
        return err;

    rwnx_ipc_rxskb_reset_pattern(rwnx_hw, buf,
                                 offsetof(struct rx_vector_desc, pattern));
    ipc_host_unsuprxvec_push(rwnx_hw->ipc_env, buf);
    return 0;
}

/**
 * rwnx_ipc_unsuprxvec_repush() - Reset and repush an already allocated buffer
 * for unsupported RX vector
 *
 * @rwnx_hw: Main driver data
 * @buf: Buf to repush
 */
void rwnx_ipc_unsuprxvec_repush(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    rwnx_ipc_rxskb_reset_pattern(rwnx_hw, buf,
                                 offsetof(struct rx_vector_desc, pattern));
    ipc_host_unsuprxvec_push(rwnx_hw->ipc_env, buf);
}

/**
* rwnx_ipc_unsuprxvecs_alloc() - Allocate and push all unsupported RX
* vector buffers for the FW
*
* @rwnx_hw: Main driver data
*/
#if (defined(CONFIG_RWNX_PCIE_MODE))
static int rwnx_ipc_unsuprxvecs_alloc(struct rwnx_hw *rwnx_hw)
{
   struct rwnx_ipc_buf *buf;
   int i;

   memset(rwnx_hw->unsuprxvecs, 0, sizeof(rwnx_hw->unsuprxvecs));

   for (i = 0, buf = rwnx_hw->unsuprxvecs; i < ARRAY_SIZE(rwnx_hw->unsuprxvecs); i++, buf++)
   {
       if (rwnx_ipc_unsuprxvec_alloc(rwnx_hw, buf)) {
           dev_err(rwnx_hw->dev, "Failed to allocate unsuprxvec buf %d\n", i + 1);
           return -ENOMEM;
       }
   }

   return 0;
}
#endif

/**
 * rwnx_ipc_unsuprxvecs_dealloc() - Free all unsupported RX vector buffers
 * allocated for the FW
 *
 * @rwnx_hw: Main driver data
 */
static void rwnx_ipc_unsuprxvecs_dealloc(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_ipc_buf *buf;
    int i;

    for (i = 0, buf = rwnx_hw->unsuprxvecs; i < ARRAY_SIZE(rwnx_hw->unsuprxvecs); i++, buf++)
    {
        rwnx_ipc_rxskb_dealloc(rwnx_hw, buf);
    }
}

/**
 * rwnx_ipc_rxbuf_alloc() - Allocate and push a rx buffer for the FW
 *
 * @rwnx_hw: Main driver data
 * @buf: IPC buffer where to store address of the skb. In fullmac this
 * parameter is not available so look for the first free IPC buffer
 */
int rwnx_ipc_rxbuf_alloc(struct rwnx_hw *rwnx_hw
#ifdef CONFIG_RWNX_SOFTMAC
                         ,struct rwnx_ipc_buf *buf
#endif
                         )
{
    int err;

#ifdef CONFIG_RWNX_FULLMAC
    struct rwnx_ipc_buf *buf;
    int nb = 0, idx = rwnx_hw->rxbuf_idx;

    while (rwnx_hw->rxbufs[idx].addr && (nb < RWNX_RXBUFF_MAX)) {
        idx = ( idx + 1 ) % RWNX_RXBUFF_MAX;
        nb++;
    }
    if (nb == RWNX_RXBUFF_MAX) {
        dev_err(rwnx_hw->dev, "No more free space for rxbuff");
        return -ENOMEM;
    }

    buf = &rwnx_hw->rxbufs[idx];

    /* Save idx so that on next push the free slot will be found quicker */
    rwnx_hw->rxbuf_idx = ( idx + 1 ) % RWNX_RXBUFF_MAX;
#endif

    err = rwnx_ipc_rxskb_alloc(rwnx_hw, buf, rwnx_hw->ipc_env->rxbuf_sz);
    if (err)
        return err;

#if (!defined(CONFIG_RWNX_SDIO_MODE))
    rwnx_ipc_rxskb_reset_pattern(rwnx_hw, buf, offsetof(struct hw_rxhdr, pattern));
#endif

#ifdef CONFIG_RWNX_FULLMAC
    RWNX_RXBUFF_HOSTID_SET(buf, RWNX_RXBUFF_IDX_TO_HOSTID(idx));
#endif
    ipc_host_rxbuf_push(rwnx_hw->ipc_env, buf);

    return 0;
}

/**
 * rwnx_ipc_rxbuf_dealloc() - Free a RX buffer for the FW
 *
 * @rwnx_hw: Main driver data
 * @buf: IPC buffer associated to the RX buffer to free
 */
void rwnx_ipc_rxbuf_dealloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    rwnx_ipc_rxskb_dealloc(rwnx_hw, buf);
}

/**
 * rwnx_ipc_rxbuf_repush() - Reset and repush an already allocated RX buffer
 *
 * @rwnx_hw: Main driver data
 * @buf: Buf to repush
 *
 * In case a skb is not forwarded to upper layer it can be re-used.
 */
void rwnx_ipc_rxbuf_repush(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    rwnx_ipc_rxskb_reset_pattern(rwnx_hw, buf, offsetof(struct hw_rxhdr, pattern));
    ipc_host_rxbuf_push(rwnx_hw->ipc_env, buf);
}

/**
 * rwnx_ipc_rxbufs_alloc() - Allocate and push all RX buffer for the FW
 *
 * @rwnx_hw: Main driver data
 */
static int rwnx_ipc_rxbufs_alloc(struct rwnx_hw *rwnx_hw)
{
    int i, nb = rwnx_hw->ipc_env->rxbuf_nb;

    memset(rwnx_hw->rxbufs, 0, sizeof(rwnx_hw->rxbufs));

    for (i = 0; i < nb; i++) {
        if (rwnx_ipc_rxbuf_alloc(rwnx_hw
#ifdef CONFIG_RWNX_SOFTMAC
                                 ,&rwnx_hw->rxbufs[i]
#endif
                                 )) {
            dev_err(rwnx_hw->dev, "Failed to allocate rx buf %d/%d\n",
                    i + 1, nb);
            return -ENOMEM;
        }
    }

    return 0;
}

/**
 * rwnx_ipc_rxbufs_dealloc() - Free all RX buffer allocated for the FW
 *
 * @rwnx_hw: Main driver data
 */
static void rwnx_ipc_rxbufs_dealloc(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_ipc_buf *buf;
    int i;

    for (i = 0, buf = rwnx_hw->rxbufs; i < ARRAY_SIZE(rwnx_hw->rxbufs); i++, buf++) {
        rwnx_ipc_rxskb_dealloc(rwnx_hw, buf);
    }
}

#ifdef CONFIG_RWNX_FULLMAC
/**
 * rwnx_ipc_rxdesc_repush() - Repush a RX descriptor to FW
 *
 * @rwnx_hw: Main driver data
 * @buf: RX descriptor to repush
 *
 * Once RX buffer has been received, the RX descriptor used by FW to upload this
 * buffer can be re-used for another RX buffer.
 */
void rwnx_ipc_rxdesc_repush(struct rwnx_hw *rwnx_hw,
                            struct rwnx_ipc_buf *buf)
{
    struct rxdesc_tag *rxdesc = buf->addr;
    rxdesc->status = 0;
    dma_sync_single_for_device(rwnx_hw->dev, buf->dma_addr,
                               sizeof(struct rxdesc_tag), DMA_BIDIRECTIONAL);
    ipc_host_rxdesc_push(rwnx_hw->ipc_env, buf);
}

/**
 * rwnx_ipc_rxbuf_from_hostid() - Return IPC buffer of a RX buffer from a hostid
 *
 * @rwnx_hw: Main driver data
 * @hostid: Hostid of the RX buffer
 * @return: Pointer to the RX buffer with the provided hostid and NULL if the
 * hostid is invalid or no buffer is associated.
 */
struct rwnx_ipc_buf *rwnx_ipc_rxbuf_from_hostid(struct rwnx_hw *rwnx_hw, u32 hostid)
{
    int rxbuf_idx = RWNX_RXBUFF_HOSTID_TO_IDX(hostid);

    if (RWNX_RXBUFF_VALID_IDX(rxbuf_idx)) {
        struct rwnx_ipc_buf *buf = &rwnx_hw->rxbufs[rxbuf_idx];
        if (buf->addr && (RWNX_RXBUFF_HOSTID_GET(buf) == hostid))
            return buf;

        dev_err(rwnx_hw->dev, "Invalid Rx buff: hostid=%d addr=%p hostid_in_buff=%d\n",
                hostid, buf->addr, (buf->addr) ? RWNX_RXBUFF_HOSTID_GET(buf): -1);

        if (buf->addr)
            rwnx_ipc_rxbuf_dealloc(rwnx_hw, buf);
    }

    dev_err(rwnx_hw->dev, "RX Buff invalid hostid [%d]\n", hostid);
    return NULL;
}
#endif /* CONFIG_RWNX_FULLMAC */

/**
 * rwnx_elems_deallocs() - Deallocate IPC storage elements.
 * @rwnx_hw: Main driver data
 *
 * This function deallocates all the elements required for communications with
 * LMAC, such as Rx Data elements, MSGs elements, ...
 * This function should be called in correspondence with the allocation function.
 */
static void rwnx_elems_deallocs(struct rwnx_hw *rwnx_hw)
{
    rwnx_ipc_rxbufs_dealloc(rwnx_hw);
    rwnx_ipc_unsuprxvecs_dealloc(rwnx_hw);
#ifdef CONFIG_RWNX_FULLMAC
    rwnx_ipc_buf_pool_dealloc(&rwnx_hw->rxdesc_pool);
#endif
    rwnx_ipc_buf_pool_dealloc(&rwnx_hw->msgbuf_pool);
    rwnx_ipc_buf_pool_dealloc(&rwnx_hw->dbgbuf_pool);
    rwnx_ipc_buf_pool_dealloc(&rwnx_hw->radar_pool);
    rwnx_ipc_buf_pool_dealloc(&rwnx_hw->txcfm_pool);
    rwnx_ipc_buf_dealloc(rwnx_hw, &rwnx_hw->tx_pattern);

#ifndef CONFIG_RWNX_USE_PREALLOC_BUF
    rwnx_ipc_buf_dealloc(rwnx_hw, &rwnx_hw->dbgdump.buf);
#endif
}

/**
 * rwnx_elems_allocs() - Allocate IPC storage elements.
 * @rwnx_hw: Main driver data
 *
 * This function allocates all the elements required for communications with
 * LMAC, such as Rx Data elements, MSGs elements, ...
 * This function should be called in correspondence with the deallocation function.
 */
static int rwnx_elems_allocs(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

#if (defined(CONFIG_RWNX_PCIE_MODE))
    if (dma_set_coherent_mask(rwnx_hw->dev, DMA_BIT_MASK(32)))
        goto err_alloc;

    if (rwnx_ipc_buf_pool_alloc(rwnx_hw, &rwnx_hw->msgbuf_pool,
                                IPC_MSGE2A_BUF_CNT,
                                sizeof(struct ipc_e2a_msg),
                                "rwnx_ipc_msgbuf_pool",
                                ipc_host_msgbuf_push))
        goto err_alloc;

    if (rwnx_ipc_buf_pool_alloc(rwnx_hw, &rwnx_hw->dbgbuf_pool,
                                IPC_DBGBUF_CNT,
                                sizeof(struct ipc_dbg_msg),
                                "rwnx_ipc_dbgbuf_pool",
                                ipc_host_dbgbuf_push))
        goto err_alloc;

    if (rwnx_ipc_buf_pool_alloc(rwnx_hw, &rwnx_hw->radar_pool,
                                IPC_RADARBUF_CNT,
                                sizeof(struct radar_pulse_array_desc),
                                "rwnx_ipc_radar_pool",
                                ipc_host_radar_push))
        goto err_alloc;

    if (rwnx_ipc_buf_pool_alloc(rwnx_hw, &rwnx_hw->txcfm_pool,
                                IPC_TXCFM_CNT,
                                sizeof(struct tx_cfm_tag),
                                "rwnx_ipc_txcfm_pool",
                                ipc_host_txcfm_push))
        goto err_alloc;

    if (rwnx_ipc_unsuprxvecs_alloc(rwnx_hw))
        goto err_alloc;

    if (rwnx_ipc_buf_a2e_alloc(rwnx_hw, &rwnx_hw->tx_pattern, sizeof(u32),
                               &rwnx_tx_pattern))
        goto err_alloc;
    ipc_host_pattern_push(rwnx_hw->ipc_env, &rwnx_hw->tx_pattern);

    /*
     * Note that the RX buffers are no longer allocated here as their size depends on the
     * FW configuration, which is not available at that time.
     * They will be allocated when checking the parameter compatibility between the driver
     * and the underlying components (i.e. during the rwnx_handle_dynparams() execution)
     */
#ifdef CONFIG_RWNX_FULLMAC
    if (rwnx_ipc_buf_pool_alloc(rwnx_hw, &rwnx_hw->rxdesc_pool,
                                 rwnx_hw->ipc_env->rxdesc_nb,
                                 sizeof(struct rxdesc_tag),
                                 "rwnx_ipc_rxdesc_pool",
                                 ipc_host_rxdesc_push))
        goto err_alloc;

#endif /* CONFIG_RWNX_FULLMAC */
#endif


#ifdef CONFIG_RWNX_USE_PREALLOC_BUF
        if (rwnx_ipc_buf_e2a_prealloc(rwnx_hw, &rwnx_hw->dbgdump.buf,
                                   sizeof(struct dbg_debug_dump_tag),
                                   PREALLOC_BUF_TYPE_DUMP))
#else
        if (rwnx_ipc_buf_e2a_alloc(rwnx_hw, &rwnx_hw->dbgdump.buf,
                                   sizeof(struct dbg_debug_dump_tag)))
#endif
            goto err_alloc;

#if (defined(CONFIG_RWNX_PCIE_MODE))
        ipc_host_dbginfo_push(rwnx_hw->ipc_env, &rwnx_hw->dbgdump.buf);
#endif

    return 0;

err_alloc:
    dev_err(rwnx_hw->dev, "Error while allocating IPC buffers\n");
    rwnx_elems_deallocs(rwnx_hw);
    return -ENOMEM;
}

#if !defined(CONFIG_RWNX_PCIE_MODE)
unsigned int tx_desc_index;

void rwnx_txbuf_list_init(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_txbuf *txbuf = NULL;
    int i = 0;

    spin_lock_init(&rwnx_hw->tx_buf_free_lock);
    spin_lock_init(&rwnx_hw->tx_buf_used_lock);
    INIT_LIST_HEAD(&rwnx_hw->tx_buf_free_list);
    INIT_LIST_HEAD(&rwnx_hw->tx_buf_used_list);
    for (i = 0; i < TX_BUF_CNT; i++) {
        txbuf = kmalloc(sizeof(struct rwnx_txbuf), GFP_ATOMIC);
        if (!txbuf) {
            ASSERT_ERR(0);
            return;
        }
        txbuf->index = i;
        txbuf->skb = NULL;
        list_add_tail(&txbuf->list, &rwnx_hw->tx_buf_free_list);
    }
}

struct rwnx_txbuf *rwnx_get_from_free_txbuf(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_txbuf *txbuf = NULL;

    if (!list_empty(&rwnx_hw->tx_buf_free_list)) {
        txbuf = list_first_entry(&rwnx_hw->tx_buf_free_list, struct rwnx_txbuf, list);
        spin_lock_bh(&rwnx_hw->tx_buf_free_lock);
        list_del(&txbuf->list);
        spin_unlock_bh(&rwnx_hw->tx_buf_free_lock);
    } else {
        ASSERT_ERR(0);
    }

    return txbuf;
}

struct sk_buff *rwnx_get_skb_from_used_txbuf(struct rwnx_hw *rwnx_hw, u32_l hostid)
{
    struct rwnx_txbuf *txbuf = NULL;
    struct rwnx_txbuf *txbuf_tmp = NULL;


    list_for_each_entry_safe(txbuf, txbuf_tmp, &rwnx_hw->tx_buf_used_list, list) {
        if (hostid == txbuf->index) {

            spin_lock_bh(&rwnx_hw->tx_buf_used_lock);
            list_del(&txbuf->list);
            spin_unlock_bh(&rwnx_hw->tx_buf_used_lock);

            spin_lock_bh(&rwnx_hw->tx_buf_free_lock);
            list_add_tail(&txbuf->list, &rwnx_hw->tx_buf_free_list);
            spin_unlock_bh(&rwnx_hw->tx_buf_free_lock);

            return txbuf->skb;
        }
    }

    return NULL;
}


void rwnx_txbuf_list_deinit(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_txbuf *txbuf = NULL;
    struct rwnx_txbuf *txbuf_tmp = NULL;

    list_for_each_entry_safe(txbuf, txbuf_tmp, &rwnx_hw->tx_buf_free_list, list) {
        kfree(txbuf);
    }
}


int rwnx_tx_task(void *data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    struct rwnx_tx_list * tx_list, *next;
    struct rwnx_sw_txhdr *sw_txhdr;
    struct txdesc_host *txdesc_host = NULL;
    struct sched_param sch_param;
    unsigned char *frm = NULL;
    uint32_t addr;
    struct rwnx_txbuf *txbuf = NULL;

    sch_param.sched_priority = 91;
    sched_setscheduler(current,SCHED_FIFO,&sch_param);

    while (1) {
        /* wait for work */
        if (down_interruptible(&rwnx_hw->rwnx_tx_task_sem) != 0) {
            /* interrupted, exit */
            printk("%s:%d wait rwnx_tx_task_sem fail!\n", __func__, __LINE__);
            break;
        }

        list_for_each_entry_safe(tx_list, next, &rwnx_hw->tx_desc_save, list) {
            sw_txhdr = tx_list->sw_txhdr;
            txdesc_host = &sw_txhdr->desc;

            txbuf = rwnx_get_from_free_txbuf(rwnx_hw);
            if (txbuf) {
                txdesc_host->api.host.hostid = txbuf->index;
                txbuf->skb = sw_txhdr->skb;

                spin_lock_bh(&rwnx_hw->tx_buf_used_lock);
                list_add_tail(&txbuf->list, &rwnx_hw->tx_buf_used_list);
                spin_unlock_bh(&rwnx_hw->tx_buf_used_lock);
            } else {
                printk("%s, ****************no free txbuff**************\n", __func__);
                break;
            }

            addr = TXBUF_START_ADDR + (txbuf->index * 2000) + STRUCT_BUFF_LEN + MAX_HEAD_LEN;
#ifdef CONFIG_RWNX_AMSDUS_TX
            if (txdesc_host->api.host.flags & TXU_CNTRL_AMSDU) {
                 //todo
            } else
#endif
            {
                frm = (unsigned char *)sw_txhdr->skb->data + sizeof(struct rwnx_txhdr);
                if ((txdesc_host->api.host.flags & TXU_CNTRL_MGMT) == 0) { //skip ethhdr for data frame
                    frm += sizeof(struct ethhdr);
                }

#if defined(CONFIG_RWNX_SDIO_MODE)
                rwnx_hw->plat->hif_ops->hi_write_ipc_sram(frm, (unsigned char *)(unsigned long)addr,  sw_txhdr->frame_len);
#else
                rwnx_hw->plat->hif_ops->hi_write_sram(frm, (unsigned char *)(unsigned long)addr,  sw_txhdr->frame_len, USB_EP4);
#endif
                txdesc_host->api.host.packet_addr[0] = addr;
                printk("%s: frame_len=%d, hwq=%d, index=%d, packet_addr[0]=%x\n", __func__, sw_txhdr->frame_len, txdesc_host->ctrl.hwq, txbuf->index, txdesc_host->api.host.packet_addr[0]);

            }
#if defined(CONFIG_RWNX_SDIO_MODE)
            rwnx_hw->plat->hif_ops->hi_write_ipc_sram((unsigned char *)txdesc_host, (unsigned char *)TXDESC_START_ADDR + (tx_desc_index * 72), sizeof(*txdesc_host));
#else
            rwnx_hw->plat->hif_ops->hi_write_sram((unsigned char *)txdesc_host, (unsigned char *)TXDESC_START_ADDR + (tx_desc_index * 72), sizeof(*txdesc_host), USB_EP4);
#endif
            printk("%s, tx_desc_index=%d, addr=%x, ipc_cnt=%x, skb=%p\n",
                __func__, tx_desc_index, TXDESC_START_ADDR + (tx_desc_index * 72), IPC_TXDMA_DESC_CNT, sw_txhdr->skb);
            tx_desc_index = (tx_desc_index + 1) % IPC_TXDMA_DESC_CNT;
            list_del(&tx_list->list);
            kfree(tx_list);

            ipc_host_txdesc_push(rwnx_hw->ipc_env, NULL);
        }
    }

    return 0;
}

#endif
/**
 * rwnx_ipc_msg_push() - Push a msg to IPC queue
 *
 * @rwnx_hw: Main driver data
 * @msg_buf: Pointer to message
 * @len: Size, in bytes, of message
 */
void rwnx_ipc_msg_push(struct rwnx_hw *rwnx_hw, void *msg_buf, uint16_t len)
{
    ipc_host_msg_push(rwnx_hw->ipc_env, msg_buf, len);
}

/**
 * rwnx_ipc_txdesc_push() - Push a txdesc to FW
 *
 * @rwnx_hw: Main driver data
 * @sw_txhdr: Pointer to the SW TX header associated to the descriptor to push
 * @skb: TX Buffer associated. Pointer saved in ipc env to retrieve it upon confirmation.
 * @hw_queue: Hw queue to push txdesc to
 */
#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
void rwnx_ipc_txdesc_push(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                          struct sk_buff *skb, int hw_queue)
{
    struct txdesc_host *txdesc_host = &sw_txhdr->desc;
    struct rwnx_tx_list *tx_list;

    txdesc_host->ctrl.hwq = hw_queue;
    txdesc_host->ready = 0xFFFFFFFF;

    //printk("%s,%d, hw_queue=%d\n", __func__, __LINE__, txdesc_host->ctrl.hwq);

    tx_list = kmalloc(sizeof(*tx_list), GFP_ATOMIC);
    if (tx_list) {
        tx_list->sw_txhdr = sw_txhdr;
        list_add_tail(&tx_list->list, &rwnx_hw->tx_desc_save);
    }
    up(&rwnx_hw->rwnx_tx_task_sem);
}
#else
void rwnx_ipc_txdesc_push(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                          struct sk_buff *skb, int hw_queue)
{
    struct txdesc_host *txdesc_host = &sw_txhdr->desc;
    struct rwnx_ipc_buf *ipc_desc = &sw_txhdr->ipc_desc;

    txdesc_host->ctrl.hwq = hw_queue;
    txdesc_host->api.host.hostid = ipc_host_tx_host_ptr_to_id(rwnx_hw->ipc_env, skb);
    txdesc_host->ready = 0xFFFFFFFF;
    if (!txdesc_host->api.host.hostid) {
        dev_err(rwnx_hw->dev, "No more tx_hostid available \n");
        return;
    }

    if (rwnx_ipc_buf_a2e_init(rwnx_hw, ipc_desc, txdesc_host, sizeof(*txdesc_host)))
        return ;

    ipc_host_txdesc_push(rwnx_hw->ipc_env, ipc_desc);
}
#endif

/**
 * rwnx_ipc_get_skb_from_cfm() - Retrieve the TX buffer associated to a confirmation buffer
 *
 * @rwnx_hw: Main driver data
 * @buf: IPC buffer for the confirmation buffer
 * @return: Pointer to TX buffer associated to this confirmation and NULL if confirmation
 * has not yet been updated by firmware
 *
 * To ensure that a confirmation has been processed by firmware check if the hostid field
 * has been updated. If this is the case retrieve TX buffer from it and reset it, otherwise
 * simply return NULL.
 */
struct sk_buff *rwnx_ipc_get_skb_from_cfm(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_ipc_buf *buf)
{
    struct sk_buff *skb = NULL;
    struct tx_cfm_tag *cfm = buf->addr;

    /* get ownership of confirmation */
    rwnx_ipc_buf_e2a_sync(rwnx_hw, buf, 0);

    /* Check host id in the confirmation. */
    /* If 0 it means that this confirmation has not yet been updated by firmware */
    if (cfm->hostid) {
        skb = ipc_host_tx_host_id_to_ptr(rwnx_hw->ipc_env, cfm->hostid);
        if (unlikely(!skb)) {
            dev_err(rwnx_hw->dev, "Cannot retrieve skb from cfm=%p/0x%llx, hostid %d in confirmation\n",
                    buf->addr, buf->dma_addr, cfm->hostid);
        } else {
            /* Unmap TX descriptor */
            struct rwnx_ipc_buf *ipc_desc = &((struct rwnx_txhdr *)skb->data)->sw_hdr->ipc_desc;
            rwnx_ipc_buf_a2e_release(rwnx_hw, ipc_desc);
        }

        cfm->hostid = 0;
    }

    /* always re-give ownership to firmware. */
    rwnx_ipc_buf_e2a_sync_back(rwnx_hw, buf, 0);

    return skb;
}

#if defined(CONFIG_RWNX_USB_MODE)
struct rwnx_fw_trace_ipc_desc ipc;
void *rwnx_ipc_fw_trace_desc_get(struct rwnx_hw *rwnx_hw)
{

    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)&ipc, (unsigned char *)&rwnx_hw->ipc_env->shared->trace_pattern, sizeof(ipc), USB_EP4);
    return (void *)&ipc;
}

#elif defined(CONFIG_RWNX_SDIO_MODE)
struct rwnx_fw_trace_ipc_desc ipc;
void *rwnx_ipc_fw_trace_desc_get(struct rwnx_hw *rwnx_hw)
{
    rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)&ipc, (unsigned char *)&rwnx_hw->ipc_env->shared->trace_pattern, sizeof(ipc));
    return (void *)&ipc;
}

#else
/**
 * rwnx_ipc_fw_trace_desc_get() - Return pointer to the start of trace
 * description in IPC environment
 *
 * @rwnx_hw: Main driver data
 */
void *rwnx_ipc_fw_trace_desc_get(struct rwnx_hw *rwnx_hw)
{
    return (void *)&(rwnx_hw->ipc_env->shared->trace_pattern);
}
#endif

/**
 * rwnx_ipc_sta_buffer - Update counter of buffered data for a given sta
 *
 * @rwnx_hw: Main driver data
 * @sta: Managed station
 * @tid: TID on which data has been added or removed
 * @size: Size of data to add (or remove if < 0) to STA buffer.
 */
 #if (defined(CONFIG_RWNX_PCIE_MODE))
void rwnx_ipc_sta_buffer(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta, int tid, int size)
{
    u32_l *buffered;

    if (!sta)
        return;

    if ((sta->sta_idx >= NX_REMOTE_STA_MAX) || (tid >= TID_MAX))
        return;

    buffered = &rwnx_hw->ipc_env->shared->buffered[sta->sta_idx][tid];

    if (size < 0) {
        size = -size;
        if (*buffered < size)
            *buffered = 0;
        else
            *buffered -= size;
    } else {
        // no test on overflow
        *buffered += size;
    }
}
#endif

#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
void rwnx_get_noparammsg_info(struct rwnx_hw *rwnx_hw, struct ipc_e2a_msg *msg)
{
    if (msg->id == DBG_ERROR_IND) {
        struct rwnx_ipc_buf *dbgdump_buf = &rwnx_hw->dbgdump.buf;

#if defined(CONFIG_RWNX_USB_MODE)
        rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)dbgdump_buf->addr,
            (unsigned char *)(unsigned long)DEBUG_INFO, dbgdump_buf->size, USB_EP4);

#elif defined(CONFIG_RWNX_SDIO_MODE)
        rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)dbgdump_buf->addr,
            (unsigned char *)(unsigned long)DEBUG_INFO, dbgdump_buf->size);
#endif
    }
}
#endif

/**
 * rwnx_msgind() - IRQ handler callback for %IPC_IRQ_E2A_MSG
 *
 * @pthis: Pointer to main driver data
 * @arg: Pointer to IPC buffer from msgbuf_pool
 */
static u8 rwnx_msgind(void *pthis, void *arg)
{
    u8 ret = 0;
    struct rwnx_hw *rwnx_hw = pthis;
#if (!defined(CONFIG_RWNX_USB_MODE) && !defined(CONFIG_RWNX_SDIO_MODE))
    struct rwnx_ipc_buf *buf = arg;
#endif

#if !defined(CONFIG_RWNX_PCIE_MODE)
    struct ipc_e2a_msg msg_rst = {0};
#endif

    struct ipc_e2a_msg *msg = NULL;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_MSGIND);

#if defined(CONFIG_RWNX_USB_MODE)
    msg = &rwnx_hw->g_msg;
    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)msg, 
        (unsigned char *)&(rwnx_hw->ipc_env->shared->msg_e2a_buf), sizeof(struct ipc_e2a_msg), USB_EP4);
    memcpy(&msg_rst, msg, sizeof(struct ipc_e2a_msg));
    msg_rst.id = 0;
    rwnx_hw->plat->hif_ops->hi_write_sram((unsigned char *)&msg_rst,
        (unsigned char *)&(rwnx_hw->ipc_env->shared->msg_e2a_buf), sizeof(struct ipc_e2a_msg), USB_EP4);
    rwnx_hw->plat->hif_ops->hi_write_word((unsigned int)CMD_DOWN_FIFO_FDH_ADDR, 1, USB_EP4);
#elif defined(CONFIG_RWNX_SDIO_MODE)
    msg = &rwnx_hw->g_msg;
    rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)msg,
        (unsigned char *)&(rwnx_hw->ipc_env->shared->msg_e2a_buf), sizeof(struct ipc_e2a_msg));
    memcpy(&msg_rst, msg, sizeof(struct ipc_e2a_msg));
    msg_rst.id = 0;
    rwnx_hw->plat->hif_ops->hi_write_ipc_sram((unsigned char *)&msg_rst,
        (unsigned char *)&(rwnx_hw->ipc_env->shared->msg_e2a_buf), sizeof(struct ipc_e2a_msg));
    rwnx_hw->plat->hif_ops->hi_write_ipc_word((unsigned int)CMD_DOWN_FIFO_FDH_ADDR, 1);
#else
    msg = buf->addr;
#endif

#if (defined(CONFIG_RWNX_USB_MODE) || defined(CONFIG_RWNX_SDIO_MODE))
    /*get info for msg no param*/
    rwnx_get_noparammsg_info(rwnx_hw, msg);
#endif
    if (rwnx_hw->scan_request == NULL) {
        RWNX_INFO("msg id=%d, pattern=0x%x\n",msg->id, msg->pattern);
    }

    /* Look for pattern which means that this hostbuf has been used for a MSG */
    if (msg->pattern != IPC_MSGE2A_VALID_PATTERN) {
        ret = -1;
        goto msg_no_push;
    }
    /* Relay further actions to the msg parser */
    rwnx_rx_handle_msg(rwnx_hw, msg);

    /* Reset the msg buffer and re-use it */
    msg->pattern = 0;
    wmb();

#if (defined(CONFIG_RWNX_PCIE_MODE))
    /* Push back the buffer to the LMAC */
    ipc_host_msgbuf_push(rwnx_hw->ipc_env, buf);
#endif

msg_no_push:
    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_MSGIND);
    return ret;
}

/**
 * rwnx_msgackind() - IRQ handler callback for %IPC_IRQ_E2A_MSG_ACK
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to command acknowledged
 */
static u8 rwnx_msgackind(void *pthis, void *hostid)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)pthis;

    RWNX_INFO("msg ack hostid=0x%lx\n", hostid);
    rwnx_hw->cmd_mgr.llind(&rwnx_hw->cmd_mgr, (struct rwnx_cmd *)hostid);
    return -1;
}

/**
 * rwnx_radarind() - IRQ handler callback for %IPC_IRQ_E2A_RADAR
 *
 * @pthis: Pointer to main driver data
 * @arg: Pointer to IPC buffer from radar_pool
 */
static u8 rwnx_radarind(void *pthis, void *arg)
{
#ifdef CONFIG_RWNX_RADAR
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_buf *buf = arg;
    struct radar_pulse_array_desc *pulses = buf->addr;
    u8 ret = 0;
    int i;

#if defined(CONFIG_RWNX_USB_MODE)
    pulses = &rwnx_hw->g_pulses;
    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)pulses,
        (unsigned char *)(unsigned long)(RADAR_EVENT_DESC_ARRAY + rwnx_hw->radar_pulse_index * 56),
        sizeof(struct radar_pulse_array_desc), USB_EP4);
#elif defined(CONFIG_RWNX_SDIO_MODE)
    pulses = &rwnx_hw->g_pulses;
    rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)pulses,
        (unsigned char *)(unsigned long)(RADAR_EVENT_DESC_ARRAY + rwnx_hw->radar_pulse_index * 56),
        sizeof(struct radar_pulse_array_desc));
#endif

#if !defined(CONFIG_RWNX_PCIE_MODE)
    rwnx_hw->radar_pulse_index = (rwnx_hw->radar_pulse_index + 1) % RADAR_EVENT_MAX;
#endif

    /* Look for pulse count meaning that this hostbuf contains RADAR pulses */
    if (pulses->cnt == 0) {
#if !defined(CONFIG_RWNX_PCIE_MODE)
        if (rwnx_hw->radar_pulse_index > 0)
            rwnx_hw->radar_pulse_index = (rwnx_hw->radar_pulse_index - 1) % RADAR_EVENT_MAX;
        else
            rwnx_hw->radar_pulse_index = RADAR_EVENT_MAX - 1;
#endif
        ret = -1;
        goto radar_no_push;
    }

    if (rwnx_radar_detection_is_enable(&rwnx_hw->radar, pulses->idx)) {
        /* Save the received pulses only if radar detection is enabled */
        for (i = 0; i < pulses->cnt; i++) {
            struct rwnx_radar_pulses *p = &rwnx_hw->radar.pulses[pulses->idx];

            p->buffer[p->index] = pulses->pulse[i];
            p->index = (p->index + 1) % RWNX_RADAR_PULSE_MAX;
            if (p->count < RWNX_RADAR_PULSE_MAX)
                p->count++;
        }

        /* Defer pulse processing in separate work */
        if (! work_pending(&rwnx_hw->radar.detection_work))
            schedule_work(&rwnx_hw->radar.detection_work);
    }

    /* Reset the radar bufent and re-use it */
    pulses->cnt = 0;
    wmb();

#if defined(CONFIG_RWNX_PCIE_MODE)
    /* Push back the buffer to the LMAC */
    ipc_host_radar_push(rwnx_hw->ipc_env, buf);
#endif

radar_no_push:
    return ret;
#else
    return -1;
#endif
}

/**
 * rwnx_dbgind() - IRQ handler callback for %IPC_IRQ_E2A_DBG
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to IPC buffer from dbgbuf_pool
 */
static u8 rwnx_dbgind(void *pthis, void *arg)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)pthis;
    struct rwnx_ipc_buf *buf = arg;
    struct ipc_dbg_msg *dbg_msg = buf->addr;
    u8 ret = 0;

#if !defined(CONFIG_RWNX_PCIE_MODE)
    struct ipc_dbg_msg dbg_rst;
#endif

#if defined(CONFIG_RWNX_USB_MODE)
    dbg_msg = &rwnx_hw->g_dbg_msg;
    rwnx_hw->plat->hif_ops->hi_read_sram((unsigned char *)dbg_msg, (unsigned char *)&(rwnx_hw->ipc_env->shared->dbg_buf), sizeof(struct ipc_dbg_msg), USB_EP4);

    memset(&dbg_rst, 0, sizeof(struct ipc_dbg_msg));
    dbg_rst.pattern = IPC_DBG_VALID_PATTERN;
    rwnx_hw->plat->hif_ops->hi_write_sram((unsigned char *)&dbg_rst, (unsigned char *)&(rwnx_hw->ipc_env->shared->dbg_buf),  sizeof(struct ipc_dbg_msg), USB_EP4);

    ipc_app2emb_trigger_set(rwnx_hw, IPC_IRQ_A2E_DBG);
#elif defined(CONFIG_RWNX_SDIO_MODE)
    dbg_msg = &rwnx_hw->g_dbg_msg;
    rwnx_hw->plat->hif_ops->hi_read_ipc_sram((unsigned char *)dbg_msg, (unsigned char *)&(rwnx_hw->ipc_env->shared->dbg_buf),  sizeof(struct ipc_dbg_msg));

    memset(&dbg_rst, 0, sizeof(struct ipc_dbg_msg));
    dbg_rst.pattern = IPC_DBG_VALID_PATTERN;
    rwnx_hw->plat->hif_ops->hi_write_ipc_sram((unsigned char *)&dbg_rst, (unsigned char *)&(rwnx_hw->ipc_env->shared->dbg_buf),  sizeof(struct ipc_dbg_msg));

    ipc_app2emb_trigger_set(rwnx_hw, IPC_IRQ_A2E_DBG);

#endif

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_DBGIND);

    /* Look for pattern which means that this hostbuf has been used for a MSG */
    if (dbg_msg->pattern != IPC_DBG_VALID_PATTERN) {
        ret = -1;
        goto dbg_no_push;
    }

    /* Display the string */
    if (strlen((const char *)dbg_msg->string) > 0)
        printk("%s %s", (char *)FW_STR, (char *)dbg_msg->string);

    /* Reset the msg buffer and re-use it */
    dbg_msg->pattern = 0;
    wmb();

#if defined(CONFIG_RWNX_PCIE_MODE)
    /* Push back the buffer to the LMAC */
    ipc_host_dbgbuf_push(rwnx_hw->ipc_env, buf);
#endif

dbg_no_push:
    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_DBGIND);

    return ret;
}

/**
 * rwnx_ipc_rxbuf_init() - Allocate and initialize RX buffers.
 *
 * @rwnx_hw: Main driver data
 * @rxbuf_sz: Size of the buffer to be allocated
 *
 * This function updates the RX buffer size according to the parameter and allocates the
 * RX buffers
 */
int rwnx_ipc_rxbuf_init(struct rwnx_hw *rwnx_hw, uint32_t rxbuf_sz)
{
    rwnx_hw->ipc_env->rxbuf_sz = rxbuf_sz;
    return rwnx_ipc_rxbufs_alloc(rwnx_hw);
}

/**
 * rwnx_ipc_init() - Initialize IPC interface.
 *
 * @rwnx_hw: Main driver data
 * @shared_ram: Pointer to shared memory that contains IPC shared struct
 *
 * This function initializes IPC interface by registering callbacks, setting
 * shared memory area and calling IPC Init function.
 * It should be called only once during driver's lifetime.
 */
int rwnx_ipc_init(struct rwnx_hw *rwnx_hw, u8 *shared_ram)
{
    struct ipc_host_cb_tag cb;
    int res = 0;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* initialize the API interface */
    cb.recv_data_ind   = rwnx_rxdataind;
    cb.recv_radar_ind  = rwnx_radarind;
    cb.recv_msg_ind    = rwnx_msgind;
    cb.recv_msgack_ind = rwnx_msgackind;
    cb.recv_dbg_ind    = rwnx_dbgind;
    cb.send_data_cfm   = rwnx_txdatacfm;
    cb.recv_unsup_rx_vec_ind = rwnx_unsup_rx_vec_ind;

    /* set the IPC environment */
    rwnx_hw->ipc_env = (struct ipc_host_env_tag *)
                       kzalloc(sizeof(struct ipc_host_env_tag), GFP_KERNEL);

    if (!rwnx_hw->ipc_env)
        return -ENOMEM;

    /* call the initialization of the IPC */
    ipc_host_init(rwnx_hw->ipc_env, &cb,
                  (struct ipc_shared_env_tag *)shared_ram, rwnx_hw);

    rwnx_cmd_mgr_init(&rwnx_hw->cmd_mgr);

    res = rwnx_elems_allocs(rwnx_hw);
    if (res) {
        kfree(rwnx_hw->ipc_env);
        rwnx_hw->ipc_env = NULL;
    }

    return res;
}

/**
 * rwnx_ipc_deinit() - Release IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_deinit(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_ipc_tx_drain(rwnx_hw);
    rwnx_cmd_mgr_deinit(&rwnx_hw->cmd_mgr);
    rwnx_elems_deallocs(rwnx_hw);
    if (rwnx_hw->ipc_env) {
        kfree(rwnx_hw->ipc_env);
        rwnx_hw->ipc_env = NULL;
    }
}

/**
 * rwnx_ipc_start() - Start IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_start(struct rwnx_hw *rwnx_hw)
{
    ipc_host_enable_irq(rwnx_hw->ipc_env, IPC_IRQ_E2A_ALL);
}

/**
 * rwnx_ipc_stop() - Stop IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_stop(struct rwnx_hw *rwnx_hw)
{
    ipc_host_disable_irq(rwnx_hw->ipc_env, IPC_IRQ_E2A_ALL);
}

/**
 * rwnx_ipc_tx_drain() - Flush IPC TX buffers
 *
 * @rwnx_hw: Main driver data
 *
 * This assumes LMAC is still (tx wise) and there's no TX race until LMAC is up
 * tx wise.
 * This also lets both IPC sides remain in sync before resetting the LMAC,
 * e.g with rwnx_send_reset.
 */
void rwnx_ipc_tx_drain(struct rwnx_hw *rwnx_hw)
{
    struct sk_buff *skb;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (!rwnx_hw->ipc_env) {
        printk(KERN_CRIT "%s: bypassing (restart must have failed)\n", __func__);
        return;
    }

    while ((skb = ipc_host_tx_flush(rwnx_hw->ipc_env))) {
        struct rwnx_sw_txhdr *sw_txhdr = ((struct rwnx_txhdr *)skb->data)->sw_hdr;

#ifdef CONFIG_RWNX_AMSDUS_TX
        if (sw_txhdr->desc.api.host.packet_cnt > 1) {
            struct rwnx_amsdu_txhdr *amsdu_txhdr;
            list_for_each_entry(amsdu_txhdr, &sw_txhdr->amsdu.hdrs, list) {
                rwnx_ipc_buf_a2e_release(rwnx_hw, &amsdu_txhdr->ipc_data);
                dev_kfree_skb_any(amsdu_txhdr->skb);
            }
        }
#endif
        kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
        rwnx_ipc_buf_a2e_release(rwnx_hw, &sw_txhdr->ipc_data);
#ifdef CONFIG_RWNX_SOFTMAC
        skb_pull(skb, sw_txhdr->headroom);
        ieee80211_free_txskb(rwnx_hw->hw, skb);
#else
        skb_pull(skb, RWNX_TX_HEADROOM);
        dev_kfree_skb_any(skb);
#endif /* CONFIG_RWNX_SOFTMAC */
    }
}

/**
 * rwnx_ipc_tx_pending() - Check if TX frames are pending at FW level
 *
 * @rwnx_hw: Main driver data
 */
bool rwnx_ipc_tx_pending(struct rwnx_hw *rwnx_hw)
{
    return ipc_host_tx_frames_pending(rwnx_hw->ipc_env);
}

/**
 * rwnx_error_ind() - %DBG_ERROR_IND message callback
 *
 * @rwnx_hw: Main driver data
 *
 * This function triggers the UMH script call that will indicate to the user
 * space the error that occurred and stored the debug dump. Once the UMH script
 * is executed, the rwnx_umh_done() function has to be called.
 */
void rwnx_error_ind(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_ipc_buf *buf = &rwnx_hw->dbgdump.buf;
    struct dbg_debug_dump_tag *dump = buf->addr;

    rwnx_ipc_buf_e2a_sync(rwnx_hw, buf, 0);
    dev_err(rwnx_hw->dev, "(type %d): dump received\n",
            dump->dbg_info.error_type);
    rwnx_hw->debugfs.trace_prst = true;
    rwnx_trigger_um_helper(&rwnx_hw->debugfs);
}

/**
 * rwnx_umh_done() - Indicate User Mode helper finished
 *
 * @rwnx_hw: Main driver data
 *
 */
void rwnx_umh_done(struct rwnx_hw *rwnx_hw)
{
    if (!test_bit(RWNX_DEV_STARTED, (void*)&rwnx_hw->flags))
        return;

    /* this assumes error_ind won't trigger before ipc_host_dbginfo_push
       is called and so does not irq protect (TODO) against error_ind */
    rwnx_hw->debugfs.trace_prst = false;
#if (defined(CONFIG_RWNX_PCIE_MODE))
    ipc_host_dbginfo_push(rwnx_hw->ipc_env, &rwnx_hw->dbgdump.buf);
#endif
}

const char *ssid_sprintf(const unsigned char *ssid, unsigned char ssid_len) {
    static unsigned char slssid[MAC_SSID_LEN+1];
    memset(slssid, 0, MAC_SSID_LEN+1);
    if (ssid && ssid_len>0 && ssid_len<=MAC_SSID_LEN)
        memcpy(slssid, ssid, ssid_len);
    return slssid;
}

u32 aml_ieee80211_chan_to_freq(u32 chan, u32 band)
{
    if (band == NL80211_BAND_5GHZ) {
        if (chan >= 182 && chan <= 196) {
            return 4000 + chan * 5;
        } else {
            return 5000 + chan * 5;
        }
    } else {
        if (chan == 14) {
            return 2484;
        } else if (chan < 14) {
            return 2407 + chan * 5;
        } else {
            return 0;
        }
    }
}
u32 aml_ieee80211_freq_to_chan(u32 freq, u32 band)
{
    if (band == NL80211_BAND_5GHZ) {
        if (freq >= 4910 && freq <= 4980) {
            return (freq - 4000) / 5;
        } else {
            return (freq - 5000) / 5;
        }
    } else {
        if (freq == 2484) {
            return 14;
        } else if (freq < 2484) {
            return (freq - 2407) / 5;
        } else {
            return 0;
        }
    }
}

