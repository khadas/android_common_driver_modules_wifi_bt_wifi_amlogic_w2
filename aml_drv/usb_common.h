#ifndef _USB_COMMON_H_
#define _USB_COMMON_H_
#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/firmware.h>

#define OS_LOCK spinlock_t


#define PRINT(...)      do {printk("aml_usb_common->");printk( __VA_ARGS__ );}while(0)
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


extern struct mutex auc_usb_mutex;

#define USB_BEGIN_LOCK() do {\
    mutex_lock(&auc_usb_mutex);\
} while (0)

#define USB_END_LOCK() do {\
    mutex_unlock(&auc_usb_mutex);\
} while (0)

#define USB_LOCK_INIT()  mutex_init(&auc_usb_mutex)
#define USB_LOCK_DESTROY() mutex_destroy(&auc_usb_mutex);

#define ZMALLOC(size, name, gfp) kzalloc(size, gfp)
#define FREE(a, name) kfree(a)


#define W2_PRODUCT  0x4c55
#define W2_VENDOR  0x414D

#define W2u_VENDOR_AMLOGIC_EFUSE 0x1B8E
#define W2u_PRODUCT_AMLOGIC_EFUSE 0x0601
/*Macro for Write/Read reg via endpoin 0*/
//#define REG_CTRL_EP0

#define AML_USB_CONTROL_MSG_TIMEOUT 3000

/* USB request types */
#define AML_USB_REQUEST          ( USB_TYPE_VENDOR | USB_RECIP_DEVICE )
#define AML_USB_REQUEST_IN       ( USB_DIR_IN | AML_USB_REQUEST )
#define AML_USB_REQUEST_OUT      ( USB_DIR_OUT | AML_USB_REQUEST )

#define AML_SIG_CBW             0x43425355
#define AML_XFER_TO_DEVICE      0
#define AML_XFER_TO_HOST        0x80
#define USB_MAX_TRANS_SIZE (64 * 1024)
#define USB_CTRL_IN_REQTYPE (USB_DIR_IN | USB_TYPE_VENDOR | (USB_RECIP_ENDPOINT & 0x1f))
#define USB_CTRL_OUT_REQTYPE (USB_DIR_OUT | USB_TYPE_VENDOR | (USB_RECIP_ENDPOINT & 0x1f))

#define BT_INTR_TRANS_FLAG 0xc6a780c2

enum wifi_cmd {
    CMD_DOWNLOAD_WIFI = 0xC1,
    CMD_START_WIFI,
    CMD_STOP_WIFI,
    CMD_READ_REG,
    CMD_WRITE_REG,
    CMD_READ_PACKET,
    CMD_WRITE_PACKET,
    CMD_WRITE_SRAM,
    CMD_READ_SRAM,
    CMD_DOWNLOAD_BT,
    CMD_GET_TX_CFM,
    CMD_OTHER_CMD,
    CMD_USB_IRQ
};


enum usb_endpoint_num{
    USB_EP0 = 0x0,
    USB_EP1,
    USB_EP2,
    USB_EP3,
    USB_EP4,
    USB_EP5,
    USB_EP6,
    USB_EP7,
};

struct crg_msc_cbw {
    uint32_t sig;
    uint32_t tag;
    uint32_t data_len;
    uint8_t flag;
    uint8_t lun;
    uint8_t len;
    uint32_t cdb[4];
    unsigned char resv[481];
}__attribute__ ((packed));

struct usb_hub {
    struct device       *intfdev;   /* the "interface" device */
    struct usb_device   *hdev;
};

struct aml_hwif_usb {
    bool scatter_enabled;
    /* protects access to scat_req */
    OS_LOCK scat_lock;
    /* scatter request list head */
    struct amlw_hif_scatter_req *scat_req;
};

/*auc--amlogic usb common*/
struct auc_hif_ops {
    int (*hi_send_cmd)(unsigned int addr, unsigned int len);
    void (*hi_write_word)(unsigned int addr,unsigned int data, unsigned int ep);
    unsigned int (*hi_read_word)(unsigned int addr, unsigned int ep);
    void (*hi_write_sram)(unsigned char* buf, unsigned char* addr, unsigned int len, unsigned int ep);
    void (*hi_read_sram)(unsigned char* buf, unsigned char* addr, unsigned int len, unsigned int ep);

    void (*hi_rx_buffer_read)(unsigned char* buf, unsigned char* addr, unsigned int len, unsigned int ep);

    /*bt use*/
    void (*hi_write_word_for_bt)(unsigned int addr,unsigned int data, unsigned int ep);
    unsigned int (*hi_read_word_for_bt)(unsigned int addr, unsigned int ep);
    void (*hi_write_sram_for_bt)(unsigned char* buf, unsigned char* addr, unsigned int len, unsigned int ep);
    void (*hi_read_sram_for_bt)(unsigned char* buf, unsigned char* addr, unsigned int len, unsigned int ep);

    int (*hi_enable_scat)(void);
    void (*hi_cleanup_scat)(void);
    struct amlw_hif_scatter_req * (*hi_get_scatreq)(void);
    int (*hi_scat_rw)(struct scatterlist *sg_list, unsigned int sg_num, unsigned int blkcnt,
        unsigned char func_num, unsigned int addr, unsigned char write);

    int (*hi_send_frame)(struct amlw_hif_scatter_req *scat_req);
    void (*hi_rcv_frame)(unsigned char* buf, unsigned char* addr, unsigned long len);
};

int aml_usb_insmod(void);
int wifi_fw_download(char *firmware_filename);
int start_wifi(void);
#endif