#ifndef _FI_W2_SDIO_H
#define _FI_W2_SDIO_H

 /* txcfm start address in sram  */
#define   SRAM_TXCFM_START_ADDR  (0xa17000)

 /* flag start base address for store some flag info in sram */
#define   SRAM_FLAG_MEM_BASE (0xa17fc0)

 /* rxdesc index address */
#define   APP2EMB_RXDESC_IDX (SRAM_FLAG_MEM_BASE) // len = 4 bytes

#define howmanypage(x,y) (((x - 12) + ((y - 12) -1) )/ (y - 12))

#define SDIO_PAGE_MAX    60
#define USB_PAGE_MAX    40
#define SDIO_FRAME_TAIL_LEN    12
#define SDIO_PAGE_LEN    1024
#define USB_PAGE_LEN    1880

#define SDIO_DATA_OFFSET   112 //12 + 28 + 72
#define SDIO_TXHEADER_LEN   104 //4 + 28 + 72
#define SDIO_TXDESC_OFFSET  40  //12 + 28
#define SECOND_PAGE_DATA_OFFSET 12
#define TXHEADER_RESERVE_LEN 240
#define TXDESC_OFFSET        32 // 4 + 28
#define USB_TXDESC_OFFSET    272 // 4 + 28 + 240
#define USB_DATA_OFFSET (USB_TXDESC_OFFSET+72) //payload

#define SDIO_TX_PAGE_SMALL_SKIP_NUM 166
#define SDIO_TX_PAGE_NUM_SMALL 90
#define SDIO_TX_PAGE_NUM_LARGE 255
#define SDIO_DYNA_PAGE_NUM  165
#define USB_TX_PAGE_SMALL_SKIP_NUM 147
#define USB_TX_PAGE_NUM_SMALL 53   //250 * 1024 / 1880
#define USB_TX_PAGE_NUM_LARGE 200
#define USB_DYNA_PAGE_NUM 147
#define SDIO_LA_PAGE_NUM  64
#define USB_LA_PAGE_NUM  34
#define USB_TX_ADDRESSTABLE_NUM  TX_PAGE_NUM_SMALL/2

//When setting bit4 to 1, enable auto tx of func4, otherwise disable
#define RG_SCFG_FUNC1_AUTO_TX  0x8181


#define WIFI_SDIO_IF    (0xa05000)

/*BIT(0): TX DONE intr, BIT(1): RX DONE intr*/
#define RG_SDIO_IF_INTR2CPU_ENABLE    (WIFI_SDIO_IF+0x30)


#endif


