#ifndef __RWNX_IWPRIV_CMD_H
#define __RWNX_IWPRIV_CMD_H
#include <net/iw_handler.h>

extern struct iw_handler_def iw_handle;

enum aml_iwpriv_subcmd
{
    AML_IWP_PRINT_VERSION = 1,
    AML_IWP_SET_RATE_LEGACY_CCK = 2,
    AML_IWP_SET_RATE_LEGACY_OFDM = 3,
    AML_IWP_SET_RATE_HT = 4,
    AML_IWP_SET_RATE_VHT = 5,
    AML_IWP_SET_RATE_HE = 6,
    AML_IWP_SET_RATE_AUTO = 7,
    AML_IWP_GET_RF_REG = 8,
    AML_IWP_SET_RF_REG = 9,
    AML_IWP_SET_SCAN_HANG = 10,
    AML_IWP_SET_SCAN_TIME = 11,
    AML_IWP_GET_REG = 12,
    AML_IWP_SET_REG = 13,
    AML_IWP_SET_PS_MODE = 14,
    AML_IWP_SEND_TWT_SETUP_REQ = 15,
    AML_IWP_SEND_TWT_TEARDOWN = 16,
};


#endif
