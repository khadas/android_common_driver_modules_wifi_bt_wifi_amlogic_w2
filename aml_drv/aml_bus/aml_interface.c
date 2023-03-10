#include <linux/init.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "aml_static_buf.h"
#include "aml_interface.h"

char *bus_type = "pci";
unsigned int aml_bus_type;
unsigned int chip_id = 0;
EXPORT_SYMBOL(bus_type);
EXPORT_SYMBOL(aml_bus_type);
extern int aml_usb_insmod(void);
extern int aml_usb_rmmod(void);
extern int aml_sdio_insmod(void);
extern int aml_sdio_rmmod(void);
extern int aml_pci_insmod(void);
extern int aml_pci_rmmod(void);

int aml_bus_intf_insmod(void)
{
    int ret;
    if (aml_init_wlan_mem()) {
        printk("aml_init_wlan_mem fail\n");
    }
    if (strncmp(bus_type,"usb",3) == 0) {
        aml_bus_type = USB_MODE;
        ret = aml_usb_insmod();
        if (ret) {
            printk("aml usb bus init fail\n");
        }
    } else if (strncmp(bus_type,"sdio",4) == 0) {
        aml_bus_type = SDIO_MODE;
        ret = aml_sdio_insmod();
        if (ret) {
            printk("aml sdio bus init fail\n");
        }
    } else if (strncmp(bus_type,"pci",3) == 0) {
        aml_bus_type = PCIE_MODE;
        ret = aml_pci_insmod();
        if (ret) {
            printk("aml sdio bus init fail\n");
        }
    }
    return 0;
}

chip_id_type aml_get_chip_type(void)
{
    if (aml_bus_type != PCIE_MODE) {
        if (chip_id == WIFI_CHIP_TYPE_W2 || chip_id == WIFI_CHIP_TYPE_W2_USB) {
            return WIFI_CHIP_W2;
        } else if (chip_id == WIFI_CHIP_TYPE_W1 || chip_id == WIFI_CHIP_TYPE_W1U) {
            return WIFI_CHIP_W1;
        } else {
            return WIFI_CHIP_UNKNOWN;
        }
    } else {
        return WIFI_CHIP_W2;
    }
}

void aml_bus_intf_rmmod(void)
{
    if (strncmp(bus_type,"usb",3) == 0) {
        aml_usb_rmmod();
    } else if (strncmp(bus_type,"sdio",4) == 0) {
        aml_sdio_rmmod();
    } else if (strncmp(bus_type,"pci",3) == 0) {
        aml_pci_rmmod();
    }
    if (aml_get_chip_type() == WIFI_CHIP_W2) {
        aml_deinit_wlan_mem();
    }
}

EXPORT_SYMBOL(aml_get_chip_type);

module_param(bus_type, charp,S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(bus_type,"A string variable to adjust pci or sdio or usb bus interface");
module_init(aml_bus_intf_insmod);
module_exit(aml_bus_intf_rmmod);

MODULE_LICENSE("GPL");
