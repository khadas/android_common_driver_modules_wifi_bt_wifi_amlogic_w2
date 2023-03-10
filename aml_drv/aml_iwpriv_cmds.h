#ifndef __AML_IWPRIV_CMD_H
#define __AML_IWPRIV_CMD_H
#include <net/iw_handler.h>
#include "wifi_debug.h"

extern struct iw_handler_def iw_handle;
extern int aml_get_txq(struct net_device *dev);

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
    AML_IWP_GET_EFUSE = 17,
    AML_IWP_SET_EFUSE = 18,
    AML_IWP_RECY_CTRL = 19,
    AML_IWP_SET_AMSDU_MAX = 20,
    AML_IWP_GET_RATE_INFO = 21,
    AML_IWP_SET_AMSDU_TX = 22,
    AML_IWP_SET_LDPC = 23,
    AML_IWP_GET_STATS = 24,
    AML_IWP_SET_P2P_NOA = 25,
    AML_IWP_SET_P2P_OPPPS = 26,
    AML_IWP_GET_ACS_INFO = 27,
    AML_IWP_GET_AMSDU_MAX = 28,
    AML_IWP_GET_AMSDU_TX = 29,
    AML_IWP_GET_LDPC = 30,
    AML_IWP_GET_TXQ = 31,
    AML_IWP_SET_TX_LFT = 32,
    AML_IWP_GET_TX_LFT = 33,
    AML_IWP_GET_LAST_RX = 34,
    AML_IWP_CLEAR_LAST_RX = 35,
    AML_IWP_SET_MACBYPASS = 36,
    AML_IWP_SET_STOP_MACBYPASS = 37,
    AML_IWP_SET_STBC = 38,
    AML_IWP_GET_STBC = 39,
    AML_IWP_PCIE_TEST = 40,
    AML_COEX_CMD = 41,
    AML_LA_DUMP = 42,
    AML_IWP_SET_PT_CALIBRATION = 43,
    AML_IWP_GET_CHAN_LIST = 44,
    AML_IWP_BUS_START_TEST = 45,
    AML_MEM_DUMP = 46,
    AML_IWP_CCA_CHECK = 47,
    AML_IWP_SET_DEBUG = 48,
    AML_PCIE_STATUS = 49,
    AML_IWP_ENABLE_WF = 50,
    AML_IWP_GET_CLK = 51,
    AML_IWP_GET_MSGIND = 52,
    AML_IWP_GET_FW_LOG = 53,
    AML_IWP_GET_RXIND = 54,
    AML_IWP_SET_RX = 55,
    AML_IWP_SET_RX_START = 56,
    AML_IWP_SET_RX_END = 57,
    AML_IWP_SET_TX_PROT = 58,
    AML_IWP_SET_TX_END = 59,
    AML_IWP_SET_TX_PATH = 60,
    AML_IWP_SET_TX_MODE = 61,
    AML_IWP_SET_TX_BW = 62,
    AML_IWP_SET_TX_RATE = 63,
    AML_IWP_SET_TX_LEN = 64,
    AML_IWP_SET_TX_PWR = 65,
    AML_IWP_SET_TX_START = 66,
    AML_IWP_SET_OLPC_PWR = 67,
    AML_IWP_SET_XOSC_CTUNE = 68,
    AML_IWP_SET_POWER_OFFSET = 69,
    AML_IWP_SET_RAM_EFUSE = 70,
    AML_IWP_SET_XOSC_EFUSE = 71,
    AML_IWP_GET_BUF_STATE = 72,
    AML_IWP_SET_TXPAGE_ONCE = 73,
    AML_IWP_SET_TXCFM_TRI_TX = 74,
    AML_IWP_SET_WIFI_MAC_EFUSE =75,
    AML_IWP_GET_WIFI_MAC_FROM_EFUSE = 76,
    AML_IWP_SET_BT_MAC_EFUSE =77,
    AML_IWP_GET_BT_MAC_FROM_EFUSE = 78,
    AML_IWP_SET_LIMIT_POWER = 79,
    AML_IWP_ENABLE_SDIO_CAL_SPEED = 80,
    AML_IWP_GET_XOSC_OFFSET = 81,
    AML_IWP_GET_CSI_STATUS_COM = 82,
    AML_IWP_GET_CSI_STATUS_SP = 83,
};

#endif
