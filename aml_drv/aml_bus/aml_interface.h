#ifndef _AML_INTERFACE_H_
#define _AML_INTERFACE_H_

#define WIFI_CHIP1_TYPE_ADDR (0x00a070bc)    /* this reg indicate chip is w1 or w2.if the reg value is 0 ,chip is w1,0x00100000 is w2 and w1u.*/
#define WIFI_CHIP2_TYPE_ADDR (0x00f00004)
/* sdio interface: this reg indicate chip is w1u or w2. if the reg value is 0x0ffbf2f0, chip is w1 and w1u, 0xfffffff0 is w2
   usb interface: this reg indicate chip is w1u or w2. if the reg value is 0x0ffbf2f0, chip is w1 and w1u, 0xfffdfff0 is w2*/

#define WIFI_CHIP_TYPE_W2   0xfffffff0
#define WIFI_CHIP_TYPE_W2_USB 0xfffdfff0
#define WIFI_CHIP_TYPE_W1   0x00000000
#define WIFI_CHIP_TYPE_W1U 0x0ffbf2f0
enum interface_type {
    SDIO_MODE,
    USB_MODE,
    PCIE_MODE
};

typedef enum {
    WIFI_CHIP_W2,
    WIFI_CHIP_W1,
    WIFI_CHIP_UNKNOWN,
}chip_id_type;


#endif
