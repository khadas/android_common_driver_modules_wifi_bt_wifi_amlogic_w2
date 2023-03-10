#include "w1_usb.h"

struct auc_w1_hif_ops g_w1_auc_hif_ops;
extern struct usb_device *g_udev;
//struct aml_hwif_usb g_hwif_usb;
extern unsigned char auc_driver_insmoded;
//unsigned char auc_wifi_in_insmod;
extern struct crg_msc_cbw *g_cmd_buf;
//struct mutex auc_usb_metux;
extern unsigned char *g_kmalloc_buf;

static void auc_build_cbw(struct crg_msc_cbw *cbw_buf,
                               unsigned char dir,
                               unsigned int len,
                               unsigned char cdb1,
                               unsigned int cdb2,
                               unsigned long cdb3,
                               SYS_TYPE_U cdb4)
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


static int auc_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
    void *data, int len, int *actual_length, int timeout)
{
    int ret;
    ret = usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout);
    return ret;
}

int auc_send_cmd(unsigned int addr, unsigned int len)
{
    int ret;
    struct usb_device *udev = g_udev;
    unsigned int actual_length;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_HOST, len, CMD_OTHER_CMD, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        PRINT_U("%s:%d, Failed to usb_bulk_msg, ret %d\n", __func__, __LINE__, ret);
        USB_END_LOCK();
        return ret;
    }

    USB_END_LOCK();
    return 0;
}


static unsigned int auc_reg_read(unsigned int addr, unsigned int len)
{
    unsigned int reg_data = 0;
 //   unsigned int actual_length;
 //   int ret;
 //   struct usb_device *udev = g_udev;

#if defined (HAL_FPGA_VER)
    int ret = 0;
    unsigned int actual_length;
    struct usb_device *udev = g_udev;

    USB_BEGIN_LOCK();
#if 0
    data = (unsigned char *)ZMALLOC(len,"reg tmp",GFP_DMA | GFP_ATOMIC);
    if (!data) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        return -ENOMEM;
    }
#endif

#ifdef REG_CTRL_EP0
    uint16_t addr_h;
    uint16_t addr_l;

    addr_h = (addr >> 16) & 0xffff;
    addr_l = addr & 0xffff;

    ret = aml_usb_control_msg(udev, usb_rcvctrlpipe(udev, USB_EP0), CMD_READ_REG, USB_CTRL_IN_REQTYPE,
                          addr_h, addr_l,g_kmalloc_buf, len, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret < 0)
    {
        ERROR_DEBUG_OUT("Failed to usb_control_msg, ret %d\n", ret);

        return ret;
    }
#else


    auc_build_cbw(g_cmd_buf, AML_XFER_TO_HOST, len, CMD_READ_REG, addr, 0, len);

    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1),(void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return ret;
    }
    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvbulkpipe(udev, USB_EP1), (void *)g_kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);

        USB_END_LOCK();
        return ret;
    }

#endif
    memcpy(&reg_data,g_kmalloc_buf,actual_length);
    USB_END_LOCK();
#elif defined (HAL_SIM_VER)

#ifdef REG_CTRL_EP0
    unsigned int req_buf = addr;
    aml_usb_control_transfer(0, &req_buf, 0, CMD_READ_REG,&reg_data, len);
    PRINT_U("[USB_HOST]reg_read value: %x\n", reg_data);
#else
    crg_msc_request(len, CRG_XFER_TO_HOST, CMD_READ_REG, addr, 0, len, &reg_data);
#endif
#endif

    return reg_data;
}

static int auc_reg_write(unsigned int addr, unsigned int value, unsigned int len)
{
#if defined (HAL_FPGA_VER)
    int ret;
    struct usb_device *udev = g_udev;

#ifdef REG_CTRL_EP0
    unsigned int req_buf[2];
    req_buf[0] = addr;
    req_buf[1] = value;

    ret = aml_usb_control_msg(udev, usb_sndctrlpipe(udev, USB_EP0), CMD_WRITE_REG, USB_CTRL_OUT_REQTYPE,
                          0, 0, req_buf, sizeof(req_buf), AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret < 0)
    {
        PRINT_U("%s:%d, Failed to usb_control_msg, ret %d\n", __func__, __LINE__, ret);
        return ret;
    }
#else
    unsigned int actual_length;
    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, 0, CMD_WRITE_REG, addr, value, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1),(void *) g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return ret;
    }
    USB_END_LOCK();
#endif

    return actual_length; //bt write maybe use the value

#elif defined (HAL_SIM_VER)

#ifdef REG_CTRL_EP0
    uint32_t req_buf[2];
    req_buf[0] = addr;
    req_buf[1] = value;
    aml_usb_control_transfer(CMD_WRITE_REG, req_buf, 0x8, 0, 0, NULL);
    return 0;
#else
    return crg_msc_request(0, CRG_XFER_TO_DEVICE, CMD_WRITE_REG, addr, value, len, NULL);
#endif

#endif
    return 0;
}

void usb_write_sram(unsigned int addr, unsigned char *pdata, unsigned int len)
{
#if defined (HAL_FPGA_VER)
    int ret;
    unsigned int actual_length;

    struct usb_device *udev = g_udev;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf, AML_XFER_TO_DEVICE, len, CMD_WRITE_SRAM, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void*)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);

        USB_END_LOCK();
        return;
    }
#if 0
    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_write_sram", GFP_DMA | GFP_ATOMIC);//virt_to_phys(fwICCM);
    if (kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        FREE(g_cmd_buf,"cmd stage");
        USB_END_LOCK();
        return;
    }
#endif
    if (len > 1024*20) {
        PRINT_U("write sram len is overflow %d\n",len);
        return;
    }
    memcpy(g_kmalloc_buf, pdata, len);
    /* data stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void *)g_kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return;
    }
    USB_END_LOCK();

#elif defined (HAL_SIM_VER)
    PRINT_U("[HOST]AML_USB_WRITE_SRAM: addr:0x%x,len:%d\n", addr, len);
    crg_msc_request(len, CRG_XFER_TO_DEVICE, CMD_WRITE_SRAM, addr, (unsigned long)pdata, len, NULL);
#endif
}

void usb_read_sram(unsigned int addr, unsigned char *pdata, unsigned int len)
{
#if defined (HAL_FPGA_VER)
    int ret;
    unsigned int actual_length;
    struct usb_device *udev = g_udev;

    USB_BEGIN_LOCK();
    auc_build_cbw(g_cmd_buf,  AML_XFER_TO_HOST, len, CMD_READ_SRAM, addr, 0, len);
    /* cmd stage */
    ret = auc_bulk_msg(udev, usb_sndbulkpipe(udev, USB_EP1), (void *)g_cmd_buf, sizeof(*g_cmd_buf), &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return;
    }
#if 0
    kmalloc_buf = (unsigned char *)ZMALLOC(len, "usb_read_sram", GFP_DMA|GFP_ATOMIC);
    if (kmalloc_buf == NULL)
    {
        ERROR_DEBUG_OUT("kmalloc buf fail\n");
        FREE(g_cmd_buf,"cmd stage");
        USB_END_LOCK();
        return;
    }
#endif
    if (len > 1024*20) {
        PRINT_U("Read sram len is overflow %d\n",len);
        return;
    }

    /* data stage */
    ret = auc_bulk_msg(udev, usb_rcvbulkpipe(udev, USB_EP1),(void *)g_kmalloc_buf, len, &actual_length, AML_USB_CONTROL_MSG_TIMEOUT);
    if (ret) {
        ERROR_DEBUG_OUT("Failed to usb_bulk_msg, ret %d\n", ret);
        USB_END_LOCK();
        return;
    }
    memcpy(pdata, g_kmalloc_buf, actual_length);
    USB_END_LOCK();
#elif defined (HAL_SIM_VER)
    PRINT_U("[HOST]AML_USB_READ_SRAM:addr:0x%x,len:%d\n", addr, len);
    crg_msc_request(len, CRG_XFER_TO_HOST, CMD_READ_SRAM, addr, 0, len, (unsigned int *)pdata);
#endif

}

void auc_write_sram(unsigned char *buf, unsigned char *sram_addr, SYS_TYPE_U len)
{
    unsigned int addr = (unsigned int)(unsigned long)sram_addr;
    usb_write_sram(addr, buf, len);
}
void auc_read_sram(unsigned char *buf,unsigned char *sram_addr, SYS_TYPE_U len)
{
    unsigned int addr = (unsigned int)(unsigned long)sram_addr;
    usb_read_sram(addr, buf, len);
}

void auc_write_word(unsigned int addr,unsigned int data)
{
    int len = 4;

    auc_reg_write(addr, data, len);
}

unsigned int auc_read_word(unsigned int addr)
{
    int len = 4;

    return auc_reg_read(addr, len);
}

void auc_w1_ops_init(void)
{
    struct auc_w1_hif_ops *ops = &g_w1_auc_hif_ops;

    ops->bt_hi_write_sram = auc_write_sram;
    ops->bt_hi_read_sram = auc_read_sram;
    ops->bt_hi_write_word = auc_write_word;
    ops->bt_hi_read_word = auc_read_word;

    auc_driver_insmoded = 1;
}

EXPORT_SYMBOL(g_w1_auc_hif_ops);
EXPORT_SYMBOL(auc_send_cmd);
