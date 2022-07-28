#ifndef SDIO_COMMON_H
#define SDIO_COMMON_H

#include <linux/mmc/sdio_func.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>    /* udelay */
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/irqreturn.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h> /* printk() */
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/gpio.h> //mach
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include "wifi_sdio_cfg_addr.h"

#ifndef ASSERT
#define ASSERT(exp) do{    \
                if (!(exp)) {   \
                        printk("=>=>=>=>=>assert %s,%d\n",__func__,__LINE__);   \
                        /*BUG();        while(1);   */  \
                }                       \
        } while (0);
#endif

#define ERROR_DEBUG_OUT(format,...) do {    \
                 printk("FUNCTION: %s LINE: %d:"format"",__FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } while (0)

#define W2_PRODUCT_AMLOGIC  0x8888
#define W2_VENDOR_AMLOGIC  0x8888

//sdio manufacturer code, usually vendor ID, 'a'=0x61, 'm'=0x6d
#define W2_VENDOR_AMLOGIC_EFUSE ('a'|('m'<<8))
//sdio manufacturer info, usually product ID
#define W2_PRODUCT_AMLOGIC_EFUSE (0x9007)

#define W2s_VENDOR_AMLOGIC_EFUSE 0x1B8E
#define W2s_PRODUCT_AMLOGIC_EFUSE 0x0600

#define WIFI_SDIO_IF    (0x5000)

/* APB domain, checksum error status, checksum enable, frame flag bypass*/
#define RG_SDIO_IF_MISC_CTRL (WIFI_SDIO_IF+0x80)
#define RG_SDIO_IF_MISC_CTRL2 (WIFI_SDIO_IF+0x84)
#define SDIO_ADDR_MASK (128 * 1024 - 1)
#define SDIO_OPMODE_INCREMENT 1
#define SDIO_OPMODE_FIXED 0
#define SDIO_WRITE 1
#define SDIO_READ 0
#define SDIOH_API_RC_SUCCESS (0x00)
#define SDIOH_API_RC_FAIL (0x01)

#define FUNCNUM_SDIO_LAST SDIO_FUNC7
#define SDIO_FUNCNUM_MAX (FUNCNUM_SDIO_LAST+1)
#define OS_LOCK spinlock_t

#define SDIO_MAX_SG_ENTRIES (160)
#define FUNC4_BLKSIZE 512
#define SDIO_CCCR_IOABORT 6

#define MAC_ICCM_AHB_BASE    0x00000000
#define MAC_SRAM_BASE        0x00a10000
#define MAC_REG_BASE         0x00a00000
#define MAC_DCCM_AHB_BASE    0x00d00000

#define SHARE_MEM_BASE_1 0x60000000
#define SHARE_MEM_BASE_2 0x60020000
#define SHARE_MEM_BASE_3 0x60040000
#define SHARE_MEM_BASE_4 0x60060000

#define SRAM_MAX_LEN (1024 * 128)
#define SRAM_LEN (32 * 1024)
#define ICCM_ROM_LEN (256 * 1024)
#define ICCM_RAM_LEN (256 * 1024)
#define ICCM_ALL_LEN (ICCM_ROM_LEN + ICCM_RAM_LEN)
#define DCCM_ALL_LEN (256 * 1024)
#define ICCM_ROM_ADDR (0x00100000)
#define ICCM_RAM_ADDR (0x00100000 + ICCM_ROM_LEN)
#define DCCM_RAM_ADDR (0x00d00000)
#define DCCM_RAM_OFFSET (0x00700000) //0x00800000 - 0x00100000, in fw_flash
#define BYTE_IN_LINE (9)
#define ICCM_BUFFER_RD_LEN  (ICCM_RAM_LEN)
#define ICCM_CHECK_LEN      (ICCM_RAM_LEN)
#define DCCM_CHECK_LEN      (DCCM_ALL_LEN)

#define ICCM_CHECK
#define ICCM_ROM

#define ZMALLOC(size, name, gfp) kzalloc(size, gfp)
#define FREE(a, name) kfree(a)

typedef unsigned long SYS_TYPE;

enum SDIO_STD_FUNNUM {
    SDIO_FUNC0=0,
    SDIO_FUNC1,
    SDIO_FUNC2,
    SDIO_FUNC3,
    SDIO_FUNC4,
    SDIO_FUNC5,
    SDIO_FUNC6,
    SDIO_FUNC7,
};

struct amlw_hif_scatter_item {
    struct sk_buff *skbbuf;
    int len;
    int page_num;
    void *packet;
};

struct amlw_hif_scatter_req {
    /* address for the read/write operation */
    unsigned int addr;
    /* request flags */
    unsigned int req;
    /* total length of entire transfer */
    unsigned int len;

    void (*complete) (struct sk_buff *);

    bool free;
    int result;
    int scat_count;

    struct scatterlist sgentries[SDIO_MAX_SG_ENTRIES];
    struct amlw_hif_scatter_item scat_list[SDIO_MAX_SG_ENTRIES];
};

struct aml_hwif_sdio {
    struct sdio_func * sdio_func_if[SDIO_FUNCNUM_MAX];
    bool scatter_enabled;

    /* protects access to scat_req */
    OS_LOCK scat_lock;

    /* scatter request list head */
    struct amlw_hif_scatter_req *scat_req;
};

struct aml_hif_sdio_ops {
    //sdio func1 for self define domain, cmd52
    int (*hi_self_define_domain_write8)(int addr, unsigned char data);
    unsigned char (*hi_self_define_domain_read8)(int addr);
    int (*hi_self_define_domain_write32)(unsigned long sram_addr, unsigned long sramdata);
    unsigned long (*hi_self_define_domain_read32)(unsigned long sram_addr);

    //sdio func2 for random ram
    void (*hi_random_word_write)(unsigned int addr, unsigned int data);
    unsigned int (*hi_random_word_read)(unsigned int addr);
    void (*hi_random_ram_write)(unsigned char *buf, unsigned char *addr, size_t len);
    void (*hi_random_ram_read)(unsigned char *buf, unsigned char *addr, size_t len);

    //sdio func3 for sram
    void (*hi_sram_word_write)(unsigned int addr, unsigned int data);
    unsigned int (*hi_sram_word_read)(unsigned int addr);
    void (*hi_sram_write)(unsigned char *buf, unsigned char *addr, size_t len);
    void (*hi_sram_read)(unsigned char *buf, unsigned char *addr, size_t len);

    //sdio func4 for tx buffer
    void (*hi_tx_buffer_write)(unsigned char *buf, unsigned char *addr, size_t len);
    void (*hi_tx_buffer_read)(unsigned char *buf, unsigned char *addr, size_t len);

    //sdio func5 for rxdesc/txdesc/tx cfm
    void (*hi_desc_write)(unsigned char *buf, unsigned char *addr, size_t len);
    void (*hi_desc_read)(unsigned char *buf, unsigned char *addr, size_t len);

    //sdio func6 for rx buffer
    void (*hi_rx_buffer_read)(unsigned char* buf, unsigned char* addr, size_t len);

    //scatter list operation
    int (*hi_enable_scat)(void);
    void (*hi_cleanup_scat)(void);
    struct amlw_hif_scatter_req * (*hi_get_scatreq)(void);
    int (*hi_scat_rw)(struct scatterlist *sg_list, unsigned int sg_num, unsigned int blkcnt,
        unsigned char func_num, unsigned int addr, unsigned char write);
    int (*hi_send_frame)(struct amlw_hif_scatter_req *scat_req);

    //sdio func7 for bt
    void (*bt_hi_write_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);
    void (*bt_hi_read_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);
    void (*bt_hi_write_word)(unsigned int addr,unsigned int data);
    unsigned int (*bt_hi_read_word)(unsigned int addr);

    //suspend & resume
    int (*hif_suspend)(unsigned int suspend_enable);
};

int aml_sdio_init(void);
void aml_sdio_calibration(void);
unsigned char aml_download_wifi_fw_img(char *firmware_filename);
extern void sdio_reinit(void);
extern void amlwifi_set_sdio_host_clk(int clk);
extern void set_usb_bt_power(int is_on);
extern struct sdio_func *aml_priv_to_func(int func_n);

extern struct aml_hif_sdio_ops g_hif_sdio_ops;
extern unsigned char g_sdio_driver_insmoded;

extern void aml_wifi_sdio_power_lock(void);
extern void aml_wifi_sdio_power_unlock(void);

#endif
