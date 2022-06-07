#include "usb_common.h"
#include "../../common/chip_ana_reg.h"
#include "../../common/wifi_intf_addr.h"

 /* memory mapping for wifi space */
#define MAC_ICCM_AHB_BASE    0x00000000
#define MAC_SRAM_BASE        0x00a10000
#define MAC_REG_BASE         0x00a00000
#define MAC_DCCM_AHB_BASE    0x00d00000

#define ICCM_ROM_LEN (256 * 1024)
#define ICCM_RAM_LEN (256 * 1024)
#define ICCM_ALL_LEN (ICCM_ROM_LEN + ICCM_RAM_LEN)
#define DCCM_ALL_LEN (160 * 1024)
#define ICCM_ROM_ADDR (0x00100000)
#define ICCM_RAM_ADDR (0x00100000 + ICCM_ROM_LEN)
#define DCCM_RAM_ADDR (0x00d00000)
#define DCCM_RAM_OFFSET (0x00700000) //0x00800000 - 0x00100000, in fw_flash
#define BYTE_IN_LINE (9)

#define ICCM_ROM

#define WIFI_TOP (0xa07000)
#define RG_WIFI_RST_CTRL (WIFI_TOP + 0x00)

#define WIFI_READ_CMD   0
#define BT_READ_CMD     1

struct auc_hif_ops g_auc_hif_ops;
struct usb_device *g_udev = NULL;
struct aml_hwif_usb g_hwif_usb;
unsigned char auc_driver_insmoded;
unsigned char auc_wifi_in_insmod;
struct crg_msc_cbw *g_cmd_buf = NULL;
struct mutex auc_usb_mutex;
unsigned char *g_kmalloc_buf;
unsigned char *ipc_basic_address = 0;

typedef unsigned long SYS_TYPE;

void auc_build_cbw(struct crg_msc_cbw *cbw_buf,
                               unsigned char dir,
                               unsigned int len,
                               unsigned char cdb1,
                               unsigned int cdb2,
                               unsigned long cdb3,
                               SYS_TYPE cdb4)
{
    cbw_buf->sig = AML_SIG_CBW;
    cbw_buf->tag = 0x5da729a0;
    cbw_buf->data_len = len;
    cbw_buf->flag = dir; //direction
    cbw_buf->len = 16; //command length
    cbw_buf->lun = 0;

    cbw_buf->cdb[0] = cdb1;
    cbw_buf->cdb[1] = cdb2; // read or write addr
    cbw_buf->cdb[2] = (unsigned int)(unsigned long)cdb3;
    cbw_buf->cdb[3] = cdb4; //read or write data length
}

int auc_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
    void *data, int len, int *actual_length, int timeout)
{
    int ret = 0;

    ret = usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout);

    return ret;
}

int auc_send_cmd_ep1(unsigned int addr, unsigned int len)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len, CMD_DOWNLOAD_BT, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP2), g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        PRINT("%s:%d, Failed to usb_bulk_msg, ret %d\n", __func__, __LINE__, ret);
        USB_END_LOCK();
        return ret;
    }

    USB_END_LOCK();
    return 0;
}

unsigned int auc_read_reg_ep1(unsigned int addr, unsigned int len)
{
    int ret = 0;
    int actual_length = 0;
    unsigned int reg_data;
    struct usb_device *udev = g_udev;
    unsigned char *data = NULL;

    USB_BEGIN_LOCK();

    data = (unsigned char *)ZMALLOC(len,"reg tmp",GFP_DMA | GFP_ATOMIC);
    if(!data) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        USB_END_LOCK();
        return -ENOMEM;
    }

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_HOST, len, CMD_READ_REG, addr, BT_INTR_TRANS_FLAG, len);

    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP2),(void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(data,"reg tmp");
        USB_END_LOCK();
        return ret;
    }
    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvintpipe(udev, USB_EP1), (void *)data, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(data,"reg tmp");
        USB_END_LOCK();
        return ret;
    }

    memcpy(&reg_data, data, actual_length);
    FREE(data,"reg tmp");
    USB_END_LOCK();

    return reg_data;
}

void auc_read_sram_ep1(unsigned char *pdata, unsigned int addr, unsigned int len)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;

    USB_BEGIN_LOCK();

    auc_build_cbw(g_cmd_buf,  AML_XFER_TO_HOST, len, CMD_READ_SRAM, addr, BT_INTR_TRANS_FLAG, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP2), (void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_read_sram", GFP_DMA|GFP_ATOMIC);
    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return;
    }

    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvintpipe(udev, USB_EP1),(void *)kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return;
    }

    memcpy(pdata, kmalloc_buf, actual_length);
    FREE(kmalloc_buf, "usb_read_sram");

    USB_END_LOCK();
}

void usb_isoc_callback(struct urb * urb)
{
    printk("urb: 0x%p; iso callback is called!\n", urb);
}

int auc_write_reg_ep3(unsigned int addr, unsigned int value, unsigned int len)
{
    int ret = 0;
    struct usb_device *udev = g_udev;
    struct urb *urb;

    USB_BEGIN_LOCK();

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        USB_END_LOCK();
        return -ENOMEM;
    }

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, 0, CMD_WRITE_REG, addr, value, len);

    urb->dev = udev;
    urb->pipe = usb_sndisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = g_cmd_buf;
    urb->transfer_buffer_length = sizeof(*g_cmd_buf);

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = sizeof(*g_cmd_buf);

    /* cmd stage */
    ret = usb_submit_urb(urb, GFP_ATOMIC);//GFP_KERNEL
    if (ret) {
        ERROR_DEBUG_OUT("Failed to submit urb, ret %d\n", ret);
        usb_free_urb(urb);
        USB_END_LOCK();
        return ret;
    }
    usb_free_urb(urb);
    USB_END_LOCK();

    return ret;
}

unsigned int auc_read_reg_ep3(unsigned int addr, unsigned int len)
{
    unsigned int reg_data;
    int ret = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;
    struct urb *urb;

    USB_BEGIN_LOCK();

    urb= usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        USB_END_LOCK();
        return -ENOMEM;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_read_sram", GFP_DMA|GFP_ATOMIC);

    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return -1;
    }

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_HOST, len, CMD_READ_REG, addr, 0, len);

    urb->dev = udev;
    urb->pipe = usb_sndisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = g_cmd_buf;
    urb->transfer_buffer_length = sizeof(*g_cmd_buf);

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = sizeof(*g_cmd_buf);

    ret = usb_submit_urb(urb, GFP_ATOMIC); //GFP_KERNEL
    if (ret) {
        ERROR_DEBUG_OUT("EP3: Failed to submit urb, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return ret;
    }

    usb_free_urb(urb);
    udelay(1000);

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return -ENOMEM;
    }

    urb->dev= udev;
    urb->pipe = usb_rcvisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = kmalloc_buf;
    urb->transfer_buffer_length = len;
    urb->number_of_packets = 1;
    urb->interval = 8;
    urb->complete = usb_isoc_callback;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = len;

    ret = usb_submit_urb(urb, GFP_ATOMIC); //GFP_KERNEL
    if (ret) {
        ERROR_DEBUG_OUT("EP3: Failed to submit urb, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return ret;
    }

    memcpy(&reg_data, kmalloc_buf, len);
    FREE(kmalloc_buf, "usb_read_sram");
    usb_free_urb(urb);
    USB_END_LOCK();

    return reg_data;
}

void auc_write_sram_ep3(unsigned char *pdata, unsigned int addr, unsigned int len)
{
    int ret = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;
    struct urb *urb;

    USB_BEGIN_LOCK();

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        USB_END_LOCK();
        return;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_write_sram", GFP_DMA | GFP_ATOMIC);//virt_to_phys(fwICCM);
    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return;
    }

    memcpy(kmalloc_buf, pdata, len);

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len + 4, CMD_WRITE_SRAM, addr, 0, len + 4);

    urb->dev = udev;
    urb->pipe = usb_sndisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = g_cmd_buf;
    urb->transfer_buffer_length = sizeof(*g_cmd_buf);

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = sizeof(*g_cmd_buf);

    /* cmd stage */
    ret = usb_submit_urb(urb, GFP_ATOMIC);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return;
    }

    usb_free_urb(urb);
    udelay(1000);

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return;
    }

    urb->dev = udev;
    urb->pipe = usb_sndisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = kmalloc_buf;
    urb->transfer_buffer_length = len;

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = len;

    /* data stage */
    ret = usb_submit_urb(urb, GFP_ATOMIC);//GFP_KERNEL
    if (ret) {
        ERROR_DEBUG_OUT("EP3: Failed to submit urb, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return;
    }

    FREE(kmalloc_buf, "usb_write_sram");
    usb_free_urb(urb);
    USB_END_LOCK();
}

void auc_read_sram_ep3(unsigned char *pdata, unsigned int addr, unsigned int len)
{
    int ret = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;
    struct urb *urb;

    USB_BEGIN_LOCK();

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        USB_END_LOCK();
        return;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_read_sram", GFP_DMA|GFP_ATOMIC);
    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return;
    }

    auc_build_cbw(g_cmd_buf,  AML_XFER_TO_HOST, len, CMD_READ_SRAM, addr, 0, len);

    urb->dev = udev;
    urb->pipe = usb_sndisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;

    urb->transfer_buffer = g_cmd_buf;
    urb->transfer_buffer_length = sizeof(*g_cmd_buf);

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;

    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = sizeof(*g_cmd_buf);

    /* cmd stage */
    ret = usb_submit_urb(urb, GFP_ATOMIC);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return;
    }

    usb_free_urb(urb);
    udelay(1000);

    urb = usb_alloc_urb(1, GFP_ATOMIC);

    if (!urb) {
        printk("alloc urb failed!\n");
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return;
    }

    urb->dev = udev;
    urb->pipe = usb_rcvisocpipe(udev, USB_EP3);
    urb->transfer_flags = URB_ISO_ASAP;
    urb->transfer_buffer = kmalloc_buf;
    urb->transfer_buffer_length = len;

    urb->complete = usb_isoc_callback;
    urb->number_of_packets = 1;
    urb->interval = 8;
    urb->iso_frame_desc[0].offset = 0;
    urb->iso_frame_desc[0].length = len;

    /* data stage */
    ret = usb_submit_urb(urb, GFP_ATOMIC); //GFP_KERNEL
    if (ret) {
        ERROR_DEBUG_OUT("Failed to submit urb, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        usb_free_urb(urb);
        USB_END_LOCK();
        return;
    }

    udelay(1000);
    memcpy(pdata, kmalloc_buf, /*urb->actual_length*/len);
    udelay(1000);

    FREE(kmalloc_buf, "usb_read_sram");
    usb_free_urb(urb);

    USB_END_LOCK();
}

int auc_write_reg_by_ep(unsigned int addr, unsigned int value, unsigned int len, unsigned int ep)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, 0, CMD_WRITE_REG, addr, value, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, ep),(void *) g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return ret;
    }
    USB_END_LOCK();

    return actual_length; //bt write maybe use the value
}

unsigned int auc_read_reg_by_ep(unsigned int addr, unsigned int len, unsigned int ep, unsigned int mode)
{
    int ret = 0;
    int actual_length = 0;
    unsigned int reg_data;
    struct usb_device *udev = g_udev;
    unsigned char *data = NULL;

    USB_BEGIN_LOCK();

    data = (unsigned char *)ZMALLOC(len,"reg tmp",GFP_DMA | GFP_ATOMIC);

    if(!data) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        USB_END_LOCK();
        return -ENOMEM;
    }

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_HOST, len, CMD_READ_REG, addr, 0, len);

    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, ep),(void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(data,"reg tmp");
        USB_END_LOCK();
        return ret;
    }

    if (mode == WIFI_READ_CMD)
        ep = USB_EP4;

    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvbulkpipe(udev, ep), (void *)data, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(data,"reg tmp");
        USB_END_LOCK();
        return ret;
    }

    memcpy(&reg_data, data, actual_length);
    FREE(data,"reg tmp");
    USB_END_LOCK();

    return reg_data;
}

void auc_write_sram_by_ep(unsigned char *pdata, unsigned int addr, unsigned int len, unsigned int ep)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len, CMD_WRITE_SRAM, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, ep), (void*)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);

        USB_END_LOCK();
        return;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_write_sram", GFP_DMA | GFP_ATOMIC);//virt_to_phys(fwICCM);
    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return;
    }

    memcpy(kmalloc_buf, pdata, len);
    /* data stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, ep), (void *)kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return;
    }

    FREE(kmalloc_buf, "usb_read_sram");
    USB_END_LOCK();

}

void auc_read_sram_by_ep(unsigned char *pdata, unsigned int addr, unsigned int len, unsigned int ep, unsigned int mode)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;
    unsigned char *kmalloc_buf = NULL;

    USB_BEGIN_LOCK();

    auc_build_cbw(g_cmd_buf,  AML_XFER_TO_HOST, len, CMD_READ_SRAM, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, ep), (void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return;
    }

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_read_sram", GFP_DMA|GFP_ATOMIC);
    if(kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        USB_END_LOCK();
        return;
    }

    if (mode == WIFI_READ_CMD)
        ep = USB_EP4;

    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvbulkpipe(udev, ep),(void *)kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        FREE(kmalloc_buf, "usb_read_sram");
        USB_END_LOCK();
        return;
    }

    memcpy(pdata, kmalloc_buf, actual_length);
    FREE(kmalloc_buf, "usb_read_sram");

    USB_END_LOCK();
}

void auc_write_word_by_ep_for_wifi(unsigned int addr,unsigned int data, unsigned int ep)
{
    int len = 4;

    if ((ep == USB_EP4) || (ep == USB_EP5) || (ep == USB_EP6) || (ep == USB_EP7)) {
        ep = USB_EP1;
        auc_write_reg_by_ep(addr, data, len, ep);
    } else {
        printk("write_word: ep-%d unsupported\n", ep);
    }
}

unsigned int auc_read_word_by_ep_for_wifi(unsigned int addr, unsigned int ep)
{
    int len = 4;
    unsigned int value = 0;

    if ((ep == USB_EP4) || (ep == USB_EP5) || (ep == USB_EP6) || (ep == USB_EP7)) {
        ep = USB_EP1;
        value = auc_read_reg_by_ep(addr, len, ep, WIFI_READ_CMD);
    } else {
        printk("Read_word: ep-%d unsupported!\n", ep);
    }

    return value;
}

void auc_write_sram_by_ep_for_wifi(unsigned char *buf, unsigned char *sram_addr, unsigned int len, unsigned int ep)
{
    if ((ep == USB_EP4) || (ep == USB_EP5) || (ep == USB_EP6) || (ep == USB_EP7)) {
        ep = USB_EP1;
        auc_write_sram_by_ep(buf, (unsigned int)(unsigned long)sram_addr, len, ep);
    } else {
        printk("write_word: ep-%d unsupported\n", ep);
    }
}

void auc_read_sram_by_ep_for_wifi(unsigned char *buf,unsigned char *sram_addr, unsigned int len, unsigned int ep)
{
    if ((ep == USB_EP4) || (ep == USB_EP5) || (ep == USB_EP6) || (ep == USB_EP7)) {
        ep = USB_EP1;
        auc_read_sram_by_ep(buf, (unsigned int)(unsigned long)sram_addr, len, ep, WIFI_READ_CMD);
    } else {
        printk("write_word: ep-%d unsupported\n", ep);
    }
}

void auc_write_word_by_ep_for_bt(unsigned int addr,unsigned int data, unsigned int ep)
{
    int len = 4;

    switch(ep) {
        case USB_EP2:
            auc_write_reg_by_ep(addr, data, len, ep);
            break;
        case USB_EP3:
            auc_write_reg_ep3(addr, data, len);
            break;
        default:
            printk("EP-%d unsupported!\n", ep);
            break;
    }
    printk("Write word: EP-%d finished!\n", ep);
}

unsigned int auc_read_word_by_ep_for_bt(unsigned int addr, unsigned int ep)
{
    int len = 4;
    unsigned int value = 0;

    switch(ep) {
        case USB_EP1:
            value = auc_read_reg_ep1(addr, len);
            break;
        case USB_EP2:
            value = auc_read_reg_by_ep(addr, len, ep, BT_READ_CMD);
            break;
        case USB_EP3:
            value = auc_read_reg_ep3(addr, len);
            break;
        default:
            printk("EP-%d unsupported!\n", ep);
            break;
    }
    printk("Read word: EP-%d finished!\n", ep);

    return value;
}

void auc_write_sram_by_ep_for_bt(unsigned char *buf, unsigned char *sram_addr, unsigned int len, unsigned int ep)
{
    switch(ep) {
        case USB_EP2:
            auc_write_sram_by_ep(buf, (unsigned int)(unsigned long)sram_addr, len, ep);
            break;
        case USB_EP3:
            auc_write_sram_ep3(buf, (unsigned int)(unsigned long)sram_addr, len);
            break;
        default:
            printk("EP-%d unsupported!\n", ep);
            break;
    }
    printk("Write sram: EP-%d finished!\n", ep);
}
void auc_read_sram_by_ep_for_bt(unsigned char *buf,unsigned char *sram_addr, unsigned int len, unsigned int ep)
{
    switch(ep) {
        case USB_EP1:
            auc_read_sram_ep1(buf, (unsigned int)(unsigned long)sram_addr, len);
            break;
        case USB_EP2:
            auc_read_sram_by_ep(buf, (unsigned int)(unsigned long)sram_addr, len, ep, BT_READ_CMD);
            break;
        case USB_EP3:
            auc_read_sram_ep3(buf, (unsigned int)(unsigned long)sram_addr, len);
            break;
        default:
            printk("EP-%d unsupported!\n", ep);
            break;
    }
    printk("Read sram: EP-%d finished!\n", ep);
}

void auc_ops_init(void)
{
    struct auc_hif_ops *ops = &g_auc_hif_ops;

    ops->hi_send_cmd = auc_send_cmd_ep1;
    ops->hi_write_word = auc_write_word_by_ep_for_wifi;
    ops->hi_read_word = auc_read_word_by_ep_for_wifi;
    ops->hi_write_sram = auc_write_sram_by_ep_for_wifi;
    ops->hi_read_sram = auc_read_sram_by_ep_for_wifi;

    ops->hi_write_word_for_bt = auc_write_word_by_ep_for_bt;
    ops->hi_read_word_for_bt = auc_read_word_by_ep_for_bt;
    ops->hi_write_sram_for_bt = auc_write_sram_by_ep_for_bt;
    ops->hi_read_sram_for_bt = auc_read_sram_by_ep_for_bt;

    auc_driver_insmoded = 1;
}

static int auc_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    g_udev = usb_get_dev(interface_to_usbdev(interface));

    USB_LOCK_INIT();

    g_cmd_buf = ZMALLOC(sizeof(*g_cmd_buf),"cmd stage",GFP_DMA | GFP_ATOMIC);
    if(!g_cmd_buf) {
        PRINT("g_cmd_buf malloc fail\n");
        return -ENOMEM;;
    }

    g_kmalloc_buf = (unsigned char *)ZMALLOC(20*1024,"reg tmp",GFP_DMA | GFP_ATOMIC);
    if(!g_kmalloc_buf) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        FREE(g_cmd_buf,"cmd stage");
        return -ENOMEM;
    }
    memset(g_kmalloc_buf,0,1024*20);
    memset(g_cmd_buf,0,sizeof(struct crg_msc_cbw ));

    auc_ops_init();

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
    {}
};

MODULE_DEVICE_TABLE(usb, auc_devices);

static struct usb_driver aml_usb_common_driver = {

    .name = "aml_usb_common",
    .id_table = auc_devices,
    .probe = auc_probe,
    .disconnect = auc_disconnect,
#ifdef CONFIG_PM
    .suspend = auc_suspend,
    .resume = auc_resume,
#endif
};

unsigned int aml_aon_read_reg(unsigned int addr)
{
    unsigned int regdata = 0;
    struct auc_hif_ops *hif_ops = &g_auc_hif_ops;

    regdata = hif_ops->hi_read_word((addr), USB_EP4);

    return regdata;
}


int wifi_iccm_download(unsigned char* addr, unsigned int len)
{
#ifdef ICCM_ROM
    unsigned int base_addr = MAC_ICCM_AHB_BASE + ICCM_ROM_LEN;
#else
    unsigned int base_addr = MAC_ICCM_AHB_BASE;
#endif
    unsigned int offset = 0;
    unsigned int trans_len = 0;
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;
    struct auc_hif_ops *hif_ops = &g_auc_hif_ops;
    unsigned char *buf_tmp = (unsigned char *)ZMALLOC(len, "usb_write", GFP_DMA | GFP_ATOMIC);

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len, CMD_DOWNLOAD_WIFI, base_addr, 0, len);

    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        FREE(buf_tmp, "usb_write");
        return 1;
    }

    while (offset < len) {
        if ((len - offset) > USB_MAX_TRANS_SIZE) {
            trans_len = USB_MAX_TRANS_SIZE;
        } else {
            trans_len = len - offset;
        }

        /* data stage */
        ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void*)addr+offset, trans_len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
        if (ret) {
            ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
            USB_END_LOCK();
            FREE(buf_tmp, "usb_write");
            return 1;
        }

        PRINT("wifi_iccm_download actual_length = 0x%x; len: 0x%x; offset: 0x%x\n", actual_length, len, offset);
        offset += actual_length;
    }

    USB_END_LOCK();

        hif_ops->hi_read_sram(buf_tmp,
                        (unsigned char*)(SYS_TYPE)(base_addr), len, USB_EP4);

    if(memcmp(buf_tmp, addr, len)) {
        PRINT("%s, %d: write ICCM ERROR!!!! \n", __func__, __LINE__);
    } else {
        PRINT("%s, %d: write ICCM SUCCESS!!!! \n", __func__, __LINE__);
    }
    FREE(buf_tmp, "usb_write");

    return 0;
}

int wifi_dccm_download(unsigned char* addr, unsigned int len)
{
    unsigned int base_addr = 0x00d00000;
    unsigned int offset = 0;
    unsigned int trans_len = 0;
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;
    struct auc_hif_ops *hif_ops = &g_auc_hif_ops;
    unsigned char *buf_tmp = (unsigned char *)ZMALLOC(len, "usb_write", GFP_DMA | GFP_ATOMIC);

    PRINT("dccm_downld, addr 0x%p, len %d \n", addr, len);
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len, CMD_DOWNLOAD_WIFI, base_addr, 0, len);
    USB_BEGIN_LOCK();
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void*)g_cmd_buf,sizeof(*g_cmd_buf),&actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        FREE(buf_tmp, "usb_write");
        return 1;
    }

    while (offset < len) {
        if ((len - offset) > USB_MAX_TRANS_SIZE) {
            trans_len = USB_MAX_TRANS_SIZE;
        } else {
            trans_len = len - offset;
        }

        /* data stage */
        ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1),(void *)addr+offset, trans_len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
        if (ret) {
            ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n",ret);
            USB_END_LOCK();
            FREE(buf_tmp, "usb_write");
            return 1;
        }

        PRINT("wifi_dccm_download actual_length = 0x%x; len: 0x%x; offset: 0x%x\n", actual_length, len, offset);
        offset += actual_length;
    }

    USB_END_LOCK();

        hif_ops->hi_read_sram(buf_tmp,
                        (unsigned char*)(SYS_TYPE)base_addr, len, USB_EP4);


    if(memcmp(buf_tmp, addr, len)) {
        PRINT("%s, %d: write DCCM ERROR!!!! \n", __func__, __LINE__);
    } else {
        PRINT("%s, %d: write DCCM SUCCESS!!!! \n", __func__, __LINE__);
    }
    FREE(buf_tmp, "usb_write");

    return 0;
}

int wifi_fw_download(char * firmware_filename)
{
    int i = 0, err = 0;
    unsigned int tmp_val = 0;
    unsigned int len = ICCM_RAM_LEN;
    char tmp_buf[9] = {0};
    unsigned char *src = NULL;
    unsigned char *kmalloc_buf = NULL;
    //unsigned int buf[10] = {0};
    const struct firmware *fw = NULL;
#ifndef ICCM_ROM
    unsigned int offset = 0;
#else
    unsigned int offset = ICCM_ROM_LEN;
#endif

    printk("%s: %d\n", __func__, __LINE__);
    err =request_firmware(&fw, firmware_filename, &g_udev->dev);
    if (err) {
        ERROR_DEBUG_OUT("request firmware fail!\n");
        return err;
    }

    src = (unsigned char *)fw->data + (offset / 4) * BYTE_IN_LINE;

#if 0
    if (fw->size < 1024 * 512 * 2) {
        len = ICCM_RAM_LEN;
    } else {
        len = ICCM_ALL_LEN;
    }
#endif

    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_write", GFP_DMA | GFP_ATOMIC);
    if (kmalloc_buf == NULL) {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        release_firmware(fw);
        return -ENOMEM;
    }

    for (i = 0; i < len /4; i++) {
        tmp_buf[8] = 0;
        strncpy(tmp_buf, (char *)src, 8);
        if ((err = kstrtouint(tmp_buf, 16, &tmp_val))) {
            release_firmware(fw);
            FREE(kmalloc_buf, "sdio_write");
            return err;
        }
        *(unsigned int *)&kmalloc_buf[4 * i] = __swab32(tmp_val);
        src += BYTE_IN_LINE;
    }

    printk("start iccm download!\n");
    wifi_iccm_download(kmalloc_buf, len);

    memset(kmalloc_buf, 0, len);
    src = (unsigned char *)fw->data + (ICCM_ALL_LEN / 4) * BYTE_IN_LINE;
    //len = ALIGN(DCCM_ALL_LEN, 4) - (6 * 1024/*stack*/ + 2 * 1024/*usb data*/);
    len = DCCM_ALL_LEN - (6 * 1024/*stack*/ + 2 * 1024/*usb data*/);
    for (i = 0; i < len /4; i++) {
        tmp_buf[8] = 0;
        strncpy(tmp_buf, (char *)src, 8);
        if ((err = kstrtouint(tmp_buf, 16, &tmp_val))) {
            release_firmware(fw);
            FREE(kmalloc_buf, "sdio_write");
            return err;
        }
        *(unsigned int *)&kmalloc_buf[4 * i] = __swab32(tmp_val);
        src += BYTE_IN_LINE;
    }

    printk("start dccm download!\n");
    wifi_dccm_download(kmalloc_buf, len);

    release_firmware(fw);
    FREE(kmalloc_buf, "usb_write");

    printk("finished fw downloading!\n");
    return 0;
 }


int start_wifi(void)
{
    int ret = 0;
    int actual_length = 0;
    struct usb_device *udev = g_udev;

    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, 0, CMD_START_WIFI, 0, 0, 0);
    USB_BEGIN_LOCK();
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1),(void *) g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    USB_END_LOCK();
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        return 1;
    }

    printk("start_wifi finished!\n");

    return 0;
}

int aml_common_insmod(void)
{
    int err = 0;

    err = usb_register(&aml_usb_common_driver);
    auc_driver_insmoded = 1;
    auc_wifi_in_insmod = 0;
    PRINT("%s(%d) aml common driver insmod\n",__func__, __LINE__);

    if(err) {
        PRINT("failed to register usb driver: %d \n", err);
    }

    return err;
}

void aml_common_rmmod(void)
{
    usb_deregister(&aml_usb_common_driver);
    auc_driver_insmoded = 0;
    PRINT("%s(%d) aml common driver rmsmod\n",__func__, __LINE__);
}

EXPORT_SYMBOL(aml_common_insmod);
EXPORT_SYMBOL(g_cmd_buf);
EXPORT_SYMBOL(g_auc_hif_ops);
EXPORT_SYMBOL(g_udev);
EXPORT_SYMBOL(auc_driver_insmoded);
EXPORT_SYMBOL(auc_wifi_in_insmod);
EXPORT_SYMBOL(auc_usb_mutex);
EXPORT_SYMBOL(wifi_fw_download);
EXPORT_SYMBOL(start_wifi);
EXPORT_SYMBOL(ipc_basic_address);

module_init(aml_common_insmod);
module_exit(aml_common_rmmod);

MODULE_LICENSE("GPL");

