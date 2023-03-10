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
#include "sdio_common.h"


#define CHIP_BT_PMU_REG_BASE               (0xf03000)
#define RG_BT_PMU_A17                             (CHIP_BT_PMU_REG_BASE + 0x44)
#define RG_BT_PMU_A18                             (CHIP_BT_PMU_REG_BASE + 0x48)
#define RG_BT_PMU_A20                             (CHIP_BT_PMU_REG_BASE + 0x50)
#define RG_BT_PMU_A22                             (CHIP_BT_PMU_REG_BASE + 0x58)

#define W1_PRODUCT_AMLOGIC  0x8888
#define W1_VENDOR_AMLOGIC  0x8888

#define W1u_VENDOR_AMLOGIC_EFUSE  0x1B8E
#define W1us_C_PRODUCT_AMLOGIC_EFUSE  0x0540
#define W1us_B_PRODUCT_AMLOGIC_EFUSE  0x0500
#define W1us_A_PRODUCT_AMLOGIC_EFUSE  0x04C0
#define W1us_PRODUCT_AMLOGIC_EFUSE  0x0440

//sdio manufacturer code, usually vendor ID, 'a'=0x61, 'm'=0x6d
#define W1_VENDOR_AMLOGIC_EFUSE ('a'|('m'<<8))
//sdio manufacturer info, usually product ID
#define W1_PRODUCT_AMLOGIC_EFUSE (0x9007)

#define RG_SCFG_FUNC5_BADDR_A (0x8150)

#define RXFRAME_MAXLEN 4096

//#define SDIO_MAXSG_SIZE    32
//#define SDIO_MAX_SG_ENTRIES    (SDIO_MAXSG_SIZE+2)
#define SG_PAGE_MAX 80

#define SDIO_MAXSG_SIZE    (SG_PAGE_MAX * 2)
#define SDIO_MAX_SG_ENTRIES    (SDIO_MAXSG_SIZE+2)
#define FUNC4_BLKSIZE    512

struct amlw1_hif_ops {
    int (*hi_bottom_write8)(unsigned char  func_num, int addr, unsigned char data);
    unsigned char (*hi_bottom_read8)(unsigned char  func_num, int addr);
    int (*hi_bottom_read)(unsigned char func_num, int addr, void *buf, size_t len, int incr_addr);
    int (*hi_bottom_write)(unsigned char func_num, int addr, void *buf, size_t len, int incr_addr);

    unsigned char (*hi_read8_func0)(unsigned long sram_addr);
    void (*hi_write8_func0)(unsigned long sram_addr, unsigned char sramdata);

    unsigned long (*hi_read_reg8)(unsigned long sram_addr);
    void (*hi_write_reg8)(unsigned long sram_addr, unsigned long sramdata);
    unsigned long (*hi_read_reg32)(unsigned long sram_addr);
    int (*hi_write_reg32)(unsigned long sram_addr, unsigned long sramdata);

    void (*hi_write_cmd)(unsigned long sram_addr, unsigned long sramdata);
    void (*hi_write_sram)(unsigned char*buf, unsigned char* addr, SYS_TYPE len);
    void (*hi_read_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);
    void (*hi_write_word)(unsigned int addr,unsigned int data);
    unsigned int (*hi_read_word)(unsigned int addr);

    void (*hi_rcv_frame)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);

    int (*hi_enable_scat)(void);
    void (*hi_cleanup_scat)(void);
    struct amlw_hif_scatter_req * (*hi_get_scatreq)(void);
    int (*hi_scat_rw)(struct scatterlist *sg_list, unsigned int sg_num, unsigned int blkcnt,
        unsigned char func_num, unsigned int addr, unsigned char write);
    int (*hi_send_frame)(struct amlw_hif_scatter_req *scat_req);

    /*bt use*/
    void (*bt_hi_write_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);
    void (*bt_hi_read_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE len);
    void (*bt_hi_write_word)(unsigned int addr,unsigned int data);
    unsigned int (*bt_hi_read_word)(unsigned int addr);

    void (*hif_get_sts)(unsigned int op_code, unsigned int ctrl_code);
    void (*hif_pt_rx_start)(unsigned int qos);
    void (*hif_pt_rx_stop)(void);

    int (*hif_suspend)(unsigned int suspend_enable);
};

void aml_w1_sdio_init_ops(void);
void aml_sdio_shutdown(struct device *device);
int aml_init_wlan_mem(void);
extern void sdio_reinit(void);
extern void amlwifi_set_sdio_host_clk(int clk);
extern void set_usb_bt_power(int is_on);

