#ifndef __WIFI_SHARED_MEM_CFG_H__
#define __WIFI_SHARED_MEM_CFG_H__

#ifdef CONFIG_AML_LA
#define LA
#undef TX_BUF_DYNA
#endif
#define TX_RX_BUF_OFFSET                 (6500) /* 6500 Byte */

#define SHARED_MEM_BASE_ADDR             (0x60000000)

#define TXLBUF_TAG_SDIO_3                (0x60000928) /* size 0x1AB0 */
#define TXLBUF_TAG_SDIO_1                (0x6000f4f4) /* size 0x1800*/

#define TXPAGE_DESC_ADDR                 (0x60010f40) /* size 0x114 */
#define DMA_DESC_TEST_ADDR               (0x60011054) /* size 0x10 */
#define DMA_DESC_PATTERN_ADDR            (0x60011064) /* size 0x10 */
#define TXL_SPEC_FRAME_BUFFER_CTRL_ADDR  (0x60011074) /* size 0x40 */

#define TXLBUF_TAG_SDIO                  (0x600110b4) /* size 0x1c14 */
#define E2A_MSG_EXT_BUF                  (0x60012cc8) /* size 0x40c */
#define TXL_TBD_START                    (0x600130d4) /*size 0x2d00*/

#define TXLBUF_TAG_SDIO_2                (0x600163e0) /* size 0x1964 */
#define HW_RXBUF2_START_ADDR             (0x60017d44) /* size 0x2BC */
#define HW_RXBUF1_START_ADDR             (0x60018000)
/*
LA OFF: rx buffer large size 0x40000, small size: 0x20000;
LA ON: rx buffer large size 0x30000, small size: 0x20000
*/
#define RXBUF_START_ADDR                 (0x60018000)

#ifdef LA

#if 1 /* for tx large */
#define TXBUF_START_ADDR                 (0x60028400)
#define RXBUF_END_ADDR_SMALL             (0x60028400)
#define RXBUF_END_ADDR_LARGE             (0x60028400)
#define TRACE_START_ADDR                 (0x60068000) /* trace size: 0x8000 */
#define TRACE_END_ADDR                   (0x60070000)
#else /* for rx large */
#define TXBUF_START_ADDR                 (0x60058000)
#define RXBUF_END_ADDR_SMALL             (0x60058000)
#define RXBUF_END_ADDR_LARGE             (0x60058000)
#define TRACE_START_ADDR                 (0x60068000) /* trace size: 0x8000 */
#define TRACE_END_ADDR                   (0x60070000)
#endif

#else
#define TXBUF_START_ADDR                 (0x60038000)
#define RXBUF_END_ADDR_SMALL             (0x60038000)
#define RXBUF_END_ADDR_LARGE             (0x60057C00)
#define TRACE_START_ADDR                 (0x60078000) /* trace size: 0x8000 */
#define TRACE_END_ADDR                   (0x60080000)
#endif

#define RX_BUFFER_LEN_SMALL              (RXBUF_END_ADDR_SMALL - RXBUF_START_ADDR)
#define RX_BUFFER_LEN_LARGE              (RXBUF_END_ADDR_LARGE - RXBUF_START_ADDR)


struct sdio_buffer_control
{
    unsigned char flag;
    unsigned int tx_start_time;
    unsigned int tx_total_len;
    unsigned int rx_start_time;
    unsigned int rx_total_len;
    unsigned int tx_rate;
    unsigned int rx_rate;
    unsigned int buffer_status;
};
extern struct sdio_buffer_control sdio_buffer_ctrl;

#define BUFFER_TX_USED BIT(0)
#define BUFFER_RX_USED BIT(1)
#define BUFFER_TX_NEED_ENLARGE BIT(2)
#define BUFFER_RX_NEED_ENLARGE BIT(3)
#define BUFFER_RX_ENLARGED BIT(4)
#define BUFFER_RX_REDUCED BIT(5)

#define CHAN_SWITCH_IND_MSG_ADDR (0xa17fc0)
#define SDIO_IRQ_E2A_CHAN_SWITCH_IND_MSG           CO_BIT(15)
#endif
