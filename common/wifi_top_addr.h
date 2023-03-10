#ifndef __WIFI_TOP_ADDR_H__
#define __WIFI_TOP_ADDR_H__

#define WIFI_TOP (0xa07000)

#define RG_WIFI_RST_CTRL (WIFI_TOP + 0x00)
#define RG_WIFI_RST_TIMER0 (WIFI_TOP + 0x04)
#define RG_WIFI_RST_TIMER1 (WIFI_TOP + 0x08)
#define RG_WIFI_MAC_ARC_CTRL (WIFI_TOP + 0x20)
/*coverity[bad_macro_redef]*/
#define MAC_AHBABT_CONTROL0        (WIFI_TOP + 0x28)
/*coverity[bad_macro_redef]*/
#define MAC_AHBABT_CONTROL1        (WIFI_TOP + 0x2c)

#define RG_WIFI_IF_FW2HST_CLR     (WIFI_TOP+0x64)
#define RG_WIFI_IF_FW2HST_MASK    (WIFI_TOP+0x68)
#define RG_WIFI_IF_FW2HST_IRQ_CFG (WIFI_TOP+0x6c)
#define RG_WIFI_IF_HOST_IRQ_ST    (WIFI_TOP+0x70)

#define RG_WIFI_IF_RXPAGE_BUF_RDPTR (WIFI_TOP+0x80)
#define RG_WIFI_IF_MAC_TXTABLE_BSADDR (WIFI_TOP+0x90)
#define RG_WIFI_IF_MAC_TXPAGE_BSADDR (WIFI_TOP+0x94)
#define RG_WIFI_IF_MAC_TXTABLE_WT_ID (WIFI_TOP+0x9c)
#define RG_WIFI_IF_MAC_TXTABLE_RD_ID (WIFI_TOP+0xa0)
#define RG_WIFI_IF_MAC_TXTABLE_PAGE_NUM (WIFI_TOP+0x98)
#define RG_WIFI_IF_MAC_TXTABLE_OBSERVE   (WIFI_TOP+0xa8)

#define RG_WIFI_IF_INT_CIRCLE (WIFI_TOP+0x74)//TODO
#define RG_WIFI_IF_SDIO_FW2HST_CTRL_REG (WIFI_TOP+0x78)//TODO
#define RG_WIFI_IF_GPIO_IRQ_CNF (WIFI_TOP+0x7c)//TODO

#define RG_WIFI_CPU_CTRL (WIFI_TOP+0xb0)

#endif
