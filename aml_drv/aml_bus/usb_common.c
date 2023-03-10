#include "usb_common.h"
#include "chip_ana_reg.h"
#include "wifi_intf_addr.h"
#include "sg_common.h"
#include "fi_sdio.h"
#include "w1_usb.h"
#include "w2_usb.h"
#include "aml_interface.h"
#include "fi_w2_sdio.h"

extern chip_id_type aml_get_chip_type(void);
extern void aml_w1_deinit_wlan_mem(void);

struct auc_hif_ops g_auc_hif_ops;
struct usb_device *g_udev = NULL;
struct aml_hwif_usb g_hwif_usb;
unsigned char auc_driver_insmoded;
unsigned char auc_wifi_in_insmod;
struct crg_msc_cbw *g_cmd_buf = NULL;
struct mutex auc_usb_mutex;
unsigned char *g_kmalloc_buf;

extern void auc_w1_ops_init(void);
extern void auc_w2_ops_init(void);

extern unsigned int chip_id;

void auc_reg_read_wifi_chip(unsigned int addr)
{
    unsigned int reg_data;
    int len = 4;
    int ret;
    unsigned char *data;
    struct usb_device *udev = g_udev;
    unsigned short addr_h;
    unsigned short addr_l;

    data = (unsigned char *)ZMALLOC(len, "reg tmp", GFP_DMA | GFP_ATOMIC);
    if (!data) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        return;
    }

    addr_h = (addr >> 16) & 0xffff;
    addr_l = addr & 0xffff;

    ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, USB_EP0), CMD_READ_REG, USB_CTRL_IN_REQTYPE,
                          addr_h, addr_l, data, len, AML_USB_CONTROL_MSG_TIMEOUT);

    if (ret < 0) {
        ERROR_DEBUG_OUT("Failed to usb_control_msg, ret %d\n", ret);
        return;
    }

    memcpy(&reg_data,data,len);
    FREE(data,"reg tmp");

    chip_id |= reg_data;
    printk("%s: ********get chip_id:0x%08x \n", __func__, chip_id);

    return;
}

void aml_usb_read_chip_type(void)
{

    auc_reg_read_wifi_chip(WIFI_CHIP1_TYPE_ADDR);
    auc_reg_read_wifi_chip(WIFI_CHIP2_TYPE_ADDR);
    printk("%s: ********get chipid:0x%08x \n", __func__, chip_id);
}

static int auc_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    g_udev = usb_get_dev(interface_to_usbdev(interface));

    USB_LOCK_INIT();

    g_cmd_buf = ZMALLOC(sizeof(*g_cmd_buf),"cmd stage",GFP_DMA | GFP_ATOMIC);
    if (!g_cmd_buf) {
        PRINT("g_cmd_buf malloc fail\n");
        return -ENOMEM;
    }

    g_kmalloc_buf = (unsigned char *)ZMALLOC(20*1024,"reg tmp",GFP_DMA | GFP_ATOMIC);
    if (!g_kmalloc_buf) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        FREE(g_cmd_buf,"cmd stage");
        return -ENOMEM;
    }
    memset(g_kmalloc_buf,0,1024*20);
    memset(g_cmd_buf,0,sizeof(struct crg_msc_cbw ));

    aml_usb_read_chip_type();

    if (aml_get_chip_type() == WIFI_CHIP_W1) {
        aml_w1_deinit_wlan_mem();
    }
    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        auc_w2_ops_init();
    } else if (aml_get_chip_type() == WIFI_CHIP_W1) {
        auc_w1_ops_init();
    } else {
        printk("%s: Error!!!! wifi chip is unknown\n", __func__);
        return -1;
    }

    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        g_auc_hif_ops.hi_enable_scat();
    }
    PRINT("%s(%d)\n",__func__,__LINE__);
    return 0;
}


static void auc_disconnect(struct usb_interface *interface)
{
    USB_LOCK_DESTROY();
    FREE(g_kmalloc_buf, "usb_read_sram");
    FREE(g_cmd_buf,"cmd stage");
    usb_set_intfdata(interface, NULL);
    usb_put_dev(g_udev);
    PRINT("--------aml_usb:disconnect-------\n");
}

#ifdef CONFIG_PM
static int auc_reset_resume(struct usb_interface *interface)
{
    return 0;
}

static int auc_suspend(struct usb_interface *interface,pm_message_t state)
{
    return 0;
}

static int auc_resume(struct usb_interface *interface)
{
    return 0;
}
#endif

static const struct usb_device_id auc_devices[] =
{
    {USB_DEVICE(W2_VENDOR,W2_PRODUCT)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2u_PRODUCT_A_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2u_PRODUCT_B_AMLOGIC_EFUSE)},
    {USB_DEVICE(W1u_VENDOR,W1u_PRODUCT)},
    {USB_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1uu_A_PRODUCT_AMLOGIC_EFUSE)},
    {USB_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1uu_B_PRODUCT_AMLOGIC_EFUSE)},
    {USB_DEVICE(W1u_VENDOR_AMLOGIC_EFUSE,W1uu_C_PRODUCT_AMLOGIC_EFUSE)},
    {}
};

MODULE_DEVICE_TABLE(usb, auc_devices);

static struct usb_driver aml_usb_common_driver = {

    .name = "aml_usb_common",
    .id_table = auc_devices,
    .probe = auc_probe,
    .disconnect = auc_disconnect,
#ifdef CONFIG_PM
    .reset_resume = auc_reset_resume,
    .suspend = auc_suspend,
    .resume = auc_resume,
#endif
};


int aml_usb_insmod(void)
{
    int err = 0;

    err = usb_register(&aml_usb_common_driver);
    auc_driver_insmoded = 1;
    auc_wifi_in_insmod = 0;
    PRINT("%s(%d) aml common driver insmod\n",__func__, __LINE__);

    if (err) {
        PRINT("failed to register usb driver: %d \n", err);
    }

    return err;
}

void aml_usb_rmmod(void)
{
    usb_deregister(&aml_usb_common_driver);
    auc_driver_insmoded = 0;
    PRINT("%s(%d) aml common driver rmsmod\n",__func__, __LINE__);
}

EXPORT_SYMBOL(aml_usb_insmod);
EXPORT_SYMBOL(aml_usb_rmmod);
EXPORT_SYMBOL(g_cmd_buf);
EXPORT_SYMBOL(g_auc_hif_ops);
EXPORT_SYMBOL(g_udev);
EXPORT_SYMBOL(auc_driver_insmoded);
EXPORT_SYMBOL(auc_wifi_in_insmod);
EXPORT_SYMBOL(auc_usb_mutex);

