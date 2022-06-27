/**
 ****************************************************************************************
 *
 * @file rwnx_android.c
 *
 * Copyright (C) Amlogic, Inc. All rights reserved (2022).
 *
 * @brief Android WPA driver API implementation.
 *
 ****************************************************************************************
 */
#include <linux/version.h>
#include <linux/compat.h>

#include "rwnx_utils.h"
#include "rwnx_android.h"

static struct rwnx_android_cmd rwnx_android_cmd_tbl[] = {
    RWNX_ANDROID_CMD(CMDID_RSSI, "RSSI"),
    RWNX_ANDROID_CMD(CMDID_COUNTRY, "COUNTRY"),
    RWNX_ANDROID_CMD(CMDID_P2P_SET_NOA, "P2P_SET_NOA"),
    RWNX_ANDROID_CMD(CMDID_P2P_GET_NOA, "P2P_GET_NOA"),
    RWNX_ANDROID_CMD(CMDID_P2P_SET_PS, "P2P_SET_PS"),
    RWNX_ANDROID_CMD(CMDID_SET_AP_WPS_P2P_IE, "SET_AP_WPS_P2P_IE")
};

static int rwnx_android_cmdstr2id(char *cmdstr)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(rwnx_android_cmd_tbl); i++) {
        const char *str = rwnx_android_cmd_tbl[i].str;
        if (!strncasecmp(cmdstr, str, strlen(str)))
            break;
    }
    return rwnx_android_cmd_tbl[i].id;
}

static int rwnx_android_get_rssi(struct rwnx_vif *vif, char *cmdstr, int len)
{
    int bytes = 0;
    if (RWNX_VIF_TYPE(vif) != NL80211_IFTYPE_STATION)
        return 0;

    if (vif->sta.ap) {
        bytes = snprintf(&cmdstr[bytes], len, "%s rssi %d\n",
                ssid_sprintf(vif->sta.assoc_ssid, vif->sta.assoc_ssid_len),
                vif->sta.ap->stats.last_rx.rx_vect1.rssi1);
    }
    return bytes;
}

int rwnx_android_priv_ioctl(struct rwnx_vif *vif, struct ifreq *ifr)
{
    struct rwnx_android_priv_cmd cmd;
    int id = 0, ret = 0;
    char *resp = NULL;
    int resp_len = 0;

    if (!ifr)
        return -EINVAL;

#ifdef CONFIG_COMPAT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
    if (in_compat_syscall())
#else
    if (is_compat_task())
#endif
    {
        struct rwnx_android_compat_priv_cmd compat_cmd;
        if (copy_from_user(&compat_cmd, ifr->ifr_data,
                    sizeof(struct rwnx_android_compat_priv_cmd)))
            return -EFAULT;

        cmd.buf = compat_ptr(compat_cmd.buf);
        cmd.used_len = compat_cmd.used_len;
        cmd.total_len = compat_cmd.total_len;
    } else
#endif
    {
        if (copy_from_user(&cmd, ifr->ifr_data, sizeof(struct rwnx_android_priv_cmd)))
            return -EFAULT;
    }

    if (cmd.total_len > RWNX_ANDROID_CMD_MAX_LEN || cmd.total_len < 0)
        return -EFAULT;

    resp = kmalloc(cmd.total_len, GFP_KERNEL);
    if (!resp) {
        ret = -ENOMEM;
        goto exit;
    }

    if (copy_from_user(resp, cmd.buf, cmd.total_len)) {
        ret = -EFAULT;
        goto exit;
    }

    RWNX_INFO("vif idx=%d cmd=%s\n", vif->vif_index, resp);
    id = rwnx_android_cmdstr2id(resp);
    switch (id) {
        case CMDID_RSSI:
            resp_len = rwnx_android_get_rssi(vif, resp, cmd.total_len);
            break;
        case CMDID_COUNTRY:
        /* FIXME: android will call p2p priv ioctl, if invokes failed,
         * it will cause the dynamic p2p interface create failed. */
        case CMDID_P2P_SET_NOA:
        case CMDID_P2P_GET_NOA:
        case CMDID_P2P_SET_PS:
        case CMDID_SET_AP_WPS_P2P_IE:
            ret = 0; /* do nothing in these cases */
            break;
        default:
            RWNX_INFO("not support id=%u\n", id);
            break;
    }

    if (resp_len >= 0) {
        if (resp_len == 0 && cmd.total_len > 0)
            resp[0] = '\0';
        if (resp_len >= cmd.total_len) {
            resp_len = cmd.total_len;
        } else {
            resp_len++;
        }
        cmd.used_len = resp_len;
        if (copy_to_user(cmd.buf, resp, resp_len))
        {
            RWNX_INFO("copy data to user buffer failed\n");
            ret = -EFAULT;
        }
    }

exit:
    if (resp)
        kfree(resp);
    return ret;
}
