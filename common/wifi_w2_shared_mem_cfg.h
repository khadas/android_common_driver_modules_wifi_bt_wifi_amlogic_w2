#ifndef __WIFI_W2_SHARED_MEM_CFG_H__
#define __WIFI_W2_SHARED_MEM_CFG_H__

#ifdef CONFIG_AML_LA
#define LA
#undef TX_BUF_DYNA
#endif

/* original patch_rxdesc bigger than mini patch_rxdesc 136 byte */
/* reference code patch_fwmain.c line 400 */
#define TX_RX_MINISIZE_BUF_OFFSET        (6500 + 136)
#define TX_RX_BUF_OFFSET                 (6500) /* 6500 Byte */
#define SDIO_BUF_TAG4_OFFSET                 (7*1024) /* 0x1c00 Byte */

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
#define HW_RXBUF2_START_ADDR             (0x60019944) /* size 0x2BC */
#define HW_RXBUF1_START_ADDR             (0x60019C00)
/*
LA OFF: rx buffer large size 0x40000, small size: 0x20000;
LA ON: rx buffer large size 0x30000, small size: 0x20000
*/

#define TXLBUF_TAG_SDIO_4                (0x60017d44) /* size 0x1c00*/
#define RXBUF_START_ADDR                 (0x60019C00)


#define TXBUF_START_ADDR                 (0x60040000)
#define RXBUF_END_ADDR_SMALL             (0x60040000)
#define RXBUF_END_ADDR_LARGE             (0x60069800)

#define USB_TXBUF_START_ADDR                 (0x60024000)
#define USB_RXBUF_END_ADDR_SMALL             (0x60024000)
#define USB_RXBUF_END_ADDR_LARGE             (0x60067788)
#define TRACE_START_ADDR                 (0x60080000) /* trace size: 0x6400 */
#define TRACE_END_ADDR                   (0x60080000)



#define RX_BUFFER_LEN_SMALL              (RXBUF_END_ADDR_SMALL - RXBUF_START_ADDR)
#define RX_BUFFER_LEN_LARGE              (RXBUF_END_ADDR_LARGE - RXBUF_START_ADDR)
#define USB_RX_BUFFER_LEN_SMALL              (USB_RXBUF_END_ADDR_SMALL - RXBUF_START_ADDR)
#define USB_RX_BUFFER_LEN_LARGE              (USB_RXBUF_END_ADDR_LARGE - RXBUF_START_ADDR)

#define SDIO_USB_EXTEND_E2A_IRQ_STATUS CMD_DOWN_FIFO_FDN_ADDR

/* SDIO USB E2A EXTEND IRQ TYPE */
enum sdio_usb_e2a_irq_type {
    DYNAMIC_BUF_HOST_TX_STOP  = 1,
    DYNAMIC_BUF_HOST_TX_START,
    DYNAMIC_BUF_NOTIFY_FW_TX_STOP,
    DYNAMIC_BUF_LA_SWITCH_FINSH,
};

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
    unsigned int hwwr_switch_addr;
};
extern struct sdio_buffer_control sdio_buffer_ctrl;

#define BUFFER_TX_USED             BIT(0)
#define BUFFER_RX_USED             BIT(1)
#define BUFFER_TX_NEED_ENLARGE     BIT(2)
#define BUFFER_RX_NEED_ENLARGE     BIT(3)
#define BUFFER_RX_REDUCED          BIT(4)
#define BUFFER_RX_HOST_NOTIFY      BIT(5)
#define BUFFER_RX_REDUCE_FLAG      BIT(6)
#define BUFFER_RX_ENLARGE_FINSH    BIT(7)
#define BUFFER_RX_FORCE_REDUCE     BIT(8)
#define BUFFER_RX_FORCE_ENLARGE    BIT(9)
#define BUFFER_RX_FORBID_REDUCE    BIT(10)
#define BUFFER_RX_FORBID_ENLARGE   BIT(11)
#define BUFFER_RX_WAIT_READ_DATA   BIT(12)
#define BUFFER_TX_STOP_FLAG        BIT(13)
#define BUFFER_RX_ENLARGE_FLAG     BIT(14)

#define RX_ENLARGE_READ_RX_DATA_FINSH BIT(25)
#define HOST_RXBUF_ENLARGE_FINSH      BIT(26)
#define RX_REDUCE_READ_RX_DATA_FINSH  BIT(27)
#define HOST_RXBUF_REDUCE_FINSH       BIT(28)

#define CHAN_SWITCH_IND_MSG_ADDR (0xa17fc0)
#define SDIO_IRQ_E2A_CHAN_SWITCH_IND_MSG           CO_BIT(15)

#define UNWRAP_SIZE (56)
#endif
