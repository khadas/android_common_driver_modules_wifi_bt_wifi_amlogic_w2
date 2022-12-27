#ifndef __WIFI_SDIO_IF_H__
#define __WIFI_SDIO_IF_H__

//#define WIFI_SDIO_IF    (0x5000)
#define WIFI_SDIO_IF    (0xa05000)

/*BIT(0): TX DONE intr, BIT(1): RX DONE intr*/
#define RG_SDIO_IF_INTR2CPU_ENABLE    (WIFI_SDIO_IF+0x30)
/* APB domain, checksum error status, checksum enable, frame flag bypass*/
#define RG_SDIO_IF_MISC_CTRL (WIFI_SDIO_IF+0x80)
#define RG_SDIO_IF_MISC_CTRL2 (WIFI_SDIO_IF+0x84)

#endif