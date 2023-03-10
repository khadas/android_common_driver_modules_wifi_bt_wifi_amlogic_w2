#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include "usb_common.h"

//#define OS_LOCK spinlock_t

#define USB_MAXSG_SIZE 32
#define USB_MAX_SG_ENTRIES (USB_MAXSG_SIZE+2)

#define W1u_PRODUCT  0x4c55
#define W1u_VENDOR  0x414D

#define W1u_VENDOR_AMLOGIC_EFUSE  0x1B8E
#define W1uu_C_PRODUCT_AMLOGIC_EFUSE  0x0541
#define W1uu_B_PRODUCT_AMLOGIC_EFUSE  0x0501
#define W1uu_A_PRODUCT_AMLOGIC_EFUSE  0x04C1

typedef unsigned long SYS_TYPE_U;

#define PRINT_U(...)      do {printk("aml_usb_common->");printk( __VA_ARGS__ );}while(0)

#if defined (HAL_SIM_VER)
#define CRG_XFER_TO_DEVICE   0
#define CRG_XFER_TO_HOST   0x80
#endif

/*auc--amlogic usb common*/
struct auc_w1_hif_ops {
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
    void (*hi_write_sram)(unsigned char*buf, unsigned char* addr, SYS_TYPE_U len);
    void (*hi_read_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE_U len);
    void (*hi_write_word)(unsigned int addr,unsigned int data);
    unsigned int (*hi_read_word)(unsigned int addr);

    void (*hi_rcv_frame)(unsigned char* buf, unsigned char* addr, SYS_TYPE_U len);

    int (*hi_enable_scat)(void);
    void (*hi_cleanup_scat)(void);
    struct amlw_hif_scatter_req * (*hi_get_scatreq)(void);
    int (*hi_scat_rw)(struct scatterlist *sg_list, unsigned int sg_num, unsigned int blkcnt,
        unsigned char func_num, unsigned int addr, unsigned char write);
    int (*hi_send_frame)(struct amlw_hif_scatter_req *scat_req);

    /*bt use*/
    void (*bt_hi_write_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE_U len);
    void (*bt_hi_read_sram)(unsigned char* buf, unsigned char* addr, SYS_TYPE_U len);
    void (*bt_hi_write_word)(unsigned int addr,unsigned int data);
    unsigned int (*bt_hi_read_word)(unsigned int addr);

    void (*hif_get_sts)(unsigned int op_code, unsigned int ctrl_code);
    void (*hif_pt_rx_start)(unsigned int qos);
    void (*hif_pt_rx_stop)(void);

    int (*hif_suspend)(unsigned int suspend_enable);
};

void auc_w1_ops_init(void);

