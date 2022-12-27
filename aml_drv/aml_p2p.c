/**
 ******************************************************************************
 *
 * @file aml_p2p.c
 *
 * @brief
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#include "aml_msg_tx.h"
#include "aml_mod_params.h"
#include "reg_access.h"
#include "aml_compat.h"
#include "aml_p2p.h"
#include "aml_tx.h"

/*Table 61—P2P public action frame type*/
char pub_action_trace[][30] = {
    "P2P NEG REQ",
    "P2P NEG RSP",
    "P2P NEG CFM",
    "P2P INV REQ",
    "P2P INV RSP",
    "P2P DEV DISCOVERY REQ",
    "P2P DEV DISCOVERY RSP",
    "P2P PROVISION REQ",
    "P2P PROVISION RSP",
    "P2P PUBLIC ACT REV"
};

/*Table 75—P2P action frame type*/
char p2p_action_trace[][30] = {
    "P2P NOA",
    "P2P PRECENCE REQ",
    "P2P PRECENCE RSP",
    "P2P GO DISCOVERY REQ",
    "P2P ACT REV"
};

u32 aml_get_p2p_ie_offset(const u8 *buf,u32 frame_len)
{
    u32 offset = MAC_SHORT_MAC_HDR_LEN + P2P_ACTION_HDR_LEN;
    u8 id;
    u8 len;

    while (offset < frame_len) {
        id = buf[offset];
        len = buf[offset + 1];
        if ((id == P2P_ATTR_VENDOR_SPECIFIC) &&
            (buf[offset + 2] == 0x50) && (buf[offset + 3] == 0x6f) && (buf[offset + 4] == 0x9a)) {
            return offset;
        }
        offset += len + 2;
    }
    return 0;
}
void aml_change_p2p_chanlist(struct aml_vif *vif, u8 *buf, u32 frame_len,u32* frame_len_offset,u8 chan_no)
{
    u8* p_ie_len;
    u32 offset;
    u8 id;
    u16 len;
    s32 len_diff;

    offset = aml_get_p2p_ie_offset(buf,frame_len);
    //idx pointer to wifi-direct ie
    if (offset != 0) {
        p_ie_len = &buf[offset + 1];
        offset += 6;
        while (offset < frame_len) {
            id = buf[offset];
            len = (buf[offset + 2] << 8) | (buf[offset + 1]);
            if (id == P2P_ATTR_CHANNEL_LIST) {
                break;
            }
            offset += len + 3;
        }
        //now offset pointer to channel list ie
        len_diff = len - 6;
        if (len_diff < 0)
        {
            //no enough chan num,do not change
            return;
        }
        if (chan_no <= 14) {
            buf[offset + 6] = 81;
            buf[offset + 7] = 1;
            buf[offset + 8] = chan_no;
        }
        else {
            buf[offset + 6] = 130;
            buf[offset + 7] = 1;
            buf[offset + 8] = chan_no;
        }
        *frame_len_offset = len_diff;
        //change ie len
        buf[offset + 1] = 6;
        buf[offset + 2] = 0;
        //change p2p ie len
        *p_ie_len = *p_ie_len - len_diff;
        memcpy(&buf[offset + 9], &buf[offset + len + 3], frame_len - offset - len - 3);
    }
}

void aml_change_p2p_intent(struct aml_vif *vif, u8 *buf, u32 frame_len,u32* frame_len_offset)
{
    u8* p_ie_len;
    u32 offset;
    u8 id;
    u16 len;
    offset = aml_get_p2p_ie_offset(buf,frame_len);
    //idx pointer to wifi-direct ie
    if (offset != 0) {
        p_ie_len = &buf[offset + 1];
        offset += 6;
        while (offset < frame_len) {
            id = buf[offset];
            len = (buf[offset + 2] << 8) | (buf[offset + 1]);
            if (id == P2P_ATTR_GROUP_OWNER_INTENT) {
                break;
            }
            offset += len + 3;
        }
        buf[offset + 3] = GO_INTENT_L;
    }
}

