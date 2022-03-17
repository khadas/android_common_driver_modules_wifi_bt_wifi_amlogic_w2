/**
 ******************************************************************************
 *
 * @file rwnx_defs.h
 *
 * @brief Main driver structure declarations for fullmac driver
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */

#ifndef _RWNX_DEFS_H_
#define _RWNX_DEFS_H_

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <linux/slab.h>

#include "rwnx_mod_params.h"
#include "rwnx_debugfs.h"
#include "rwnx_tx.h"
#include "rwnx_rx.h"
#include "rwnx_radar.h"
#include "rwnx_utils.h"
#include "rwnx_mu_group.h"
#include "rwnx_platform.h"
#include "rwnx_cmds.h"
#include "ipc_host.h"

#define WPI_HDR_LEN    18
#define WPI_PN_LEN     16
#define WPI_PN_OFST     2
#define WPI_MIC_LEN    16
#define WPI_KEY_LEN    32
#define WPI_SUBKEY_LEN 16 // WPI key is actually two 16bytes key

#define LEGACY_PS_ID   0
#define UAPSD_ID       1

#define PS_SP_INTERRUPTED  255

// Maximum number of AP_VLAN interfaces allowed.
// At max we can have one AP_VLAN per station, but we also limit the
// maximum number of interface to 16 (to fit in avail_idx_map)
#define MAX_AP_VLAN_ITF (((16 - NX_VIRT_DEV_MAX) > NX_REMOTE_STA_MAX) ? \
    NX_REMOTE_STA_MAX : (16 - NX_VIRT_DEV_MAX))

// Maximum number of interface at driver level
#define NX_ITF_MAX (NX_VIRT_DEV_MAX + MAX_AP_VLAN_ITF)

/**
 * struct rwnx_bcn - Information of the beacon in used (AP mode)
 *
 * @head: head portion of beacon (before TIM IE)
 * @tail: tail portion of beacon (after TIM IE)
 * @ies: extra IEs (not used ?)
 * @head_len: length of head data
 * @tail_len: length of tail data
 * @ies_len: length of extra IEs data
 * @tim_len: length of TIM IE
 * @len: Total beacon len (head + tim + tail + extra)
 * @dtim: dtim period
 */
struct rwnx_bcn {
    u8 *head;
    u8 *tail;
    u8 *ies;
    size_t head_len;
    size_t tail_len;
    size_t ies_len;
    size_t tim_len;
    size_t len;
    u8 dtim;
};

/**
 * struct rwnx_key - Key information
 *
 * @hw_idx: Index of the key from hardware point of view
 */
struct rwnx_key {
    u8 hw_idx;
};

/**
 * struct rwnx_mesh_path - Mesh Path information
 *
 * @list: List element of rwnx_vif.mesh_paths
 * @path_idx: Path index
 * @tgt_mac_addr: MAC Address it the path target
 * @nhop_sta: Next Hop STA in the path
 */
struct rwnx_mesh_path {
    struct list_head list;
    u8 path_idx;
    struct mac_addr tgt_mac_addr;
    struct rwnx_sta *nhop_sta;
};

/**
 * struct rwnx_mesh_path - Mesh Proxy information
 *
 * @list: List element of rwnx_vif.mesh_proxy
 * @ext_sta_addr: Address of the External STA
 * @proxy_addr: Proxy MAC Address
 * @local: Indicate if interface is a proxy for the device
 */
struct rwnx_mesh_proxy {
    struct list_head list;
    struct mac_addr ext_sta_addr;
    struct mac_addr proxy_addr;
    bool local;
};

/**
 * struct rwnx_csa - Information for CSA (Channel Switch Announcement)
 *
 * @vif: Pointer to the vif doing the CSA
 * @bcn: Beacon to use after CSA
 * @buf: IPC buffer to send the new beacon to the fw
 * @chandef: defines the channel to use after the switch
 * @count: Current csa counter
 * @status: Status of the CSA at fw level
 * @ch_idx: Index of the new channel context
 * @work: work scheduled at the end of CSA
 */
struct rwnx_csa {
    struct rwnx_vif *vif;
    struct rwnx_bcn bcn;
    struct rwnx_ipc_buf buf;
    struct cfg80211_chan_def chandef;
    int count;
    int status;
    int ch_idx;
    struct work_struct work;
};

/**
 * enum tdls_status_tag - States of the TDLS link
 *
 * @TDLS_LINK_IDLE: TDLS link is not active (no TDLS peer connected)
 * @TDLS_SETUP_REQ_TX: TDLS Setup Request transmitted
 * @TDLS_SETUP_RSP_TX: TDLS Setup Response transmitted
 * @TDLS_LINK_ACTIVE: TDLS link is active (TDLS peer connected)
 */
enum tdls_status_tag {
        TDLS_LINK_IDLE,
        TDLS_SETUP_REQ_TX,
        TDLS_SETUP_RSP_TX,
        TDLS_LINK_ACTIVE,
        TDLS_STATE_MAX
};

/**
 * struct rwnx_tdls - Information relative to the TDLS peer
 *
 * @active: Whether TDLS link is active or not
 * @initiator: Whether TDLS peer is the TDLS initiator or not
 * @chsw_en: Whether channel switch is enabled or not
 * @chsw_allowed: Whether TDLS channel switch is allowed or not
 * @last_tid: TID of the latest MPDU transmitted over the TDLS link
 * @last_sn: Sequence number of the latest MPDU transmitted over the TDLS link
 * @ps_on: Whether power save mode is enabled on the TDLS peer or not
 */
struct rwnx_tdls {
    bool active;
    bool initiator;
    bool chsw_en;
    u8 last_tid;
    u16 last_sn;
    bool ps_on;
    bool chsw_allowed;
};

/**
 * enum rwnx_ap_flags - AP flags
 *
 * @RWNX_AP_ISOLATE: Isolate clients (i.e. Don't bridge packets transmitted by
 * one client to another one
 * @RWNX_AP_USER_MESH_PM: Mesh Peering Management is done by user space
 * @RWNX_AP_CREATE_MESH_PATH: A Mesh path is currently being created at fw level
 */
enum rwnx_ap_flags {
    RWNX_AP_ISOLATE = BIT(0),
    RWNX_AP_USER_MESH_PM = BIT(1),
    RWNX_AP_CREATE_MESH_PATH = BIT(2),
};

/**
 * enum rwnx_sta_flags - STATION flags
 *
 * @RWNX_STA_EXT_AUTH: External authentication is in progress
 */
enum rwnx_sta_flags {
    RWNX_STA_EXT_AUTH = BIT(0),
    RWNX_STA_FT_OVER_DS = BIT(1),
    RWNX_STA_FT_OVER_AIR = BIT(2),
};

/**
 * struct rwnx_vif - VIF information
 *
 * @list: List element for rwnx_hw->vifs
 * @rwnx_hw: Pointer to driver main data
 * @wdev: Wireless device
 * @ndev: Pointer to the associated net device
 * @net_stats: Stats structure for the net device
 * @key: Conversion table between protocol key index and MACHW key index
 * @drv_vif_index: VIF index at driver level (only use to identify active
 * vifs in rwnx_hw->avail_idx_map)
 * @vif_index: VIF index at fw level (used to index rwnx_hw->vif_table, and
 * rwnx_sta->vif_idx)
 * @ch_index: Channel context index (within rwnx_hw->chanctx_table)
 * @up: Indicate if associated netdev is up (i.e. Interface is created at fw level)
 * @use_4addr: Whether 4address mode should be use or not
 * @is_resending: Whether a frame is being resent on this interface
 * @roc_tdls: Indicate if the ROC has been called by a TDLS station
 * @tdls_status: Status of the TDLS link
 * @tdls_chsw_prohibited: Whether TDLS Channel Switch is prohibited or not
 * @generation: Generation ID. Increased each time a sta is added/removed
 *
 * STA / P2P_CLIENT interfaces
 * @flags: see rwnx_sta_flags
 * @ap: Pointer to the peer STA entry allocated for the AP
 * @tdls_sta: Pointer to the TDLS station
 * @ft_assoc_ies: Association Request Elements (only allocated for FT connection)
 * @ft_assoc_ies_len: Size, in bytes, of the Association request elements.
 * @ft_target_ap: Target AP for a BSS transition for FT over DS
 *
 * AP/P2P GO/ MESH POINT interfaces
 * @flags: see rwnx_ap_flags
 * @sta_list: List of station connected to the interface
 * @bcn: Beacon data
 * @bcn_interval: beacon interval in TU
 * @bcmc_index: Index of the BroadCast/MultiCast station
 * @csa: Information about current Channel Switch Announcement (NULL if no CSA)
 * @mpath_list: List of Mesh Paths (MESH Point only)
 * @proxy_list: List of Proxies Information (MESH Point only)
 * @mesh_pm: Mesh power save mode currently set in firmware
 * @next_mesh_pm: Mesh power save mode for next peer
 *
 * AP_VLAN interfaces
 * @mater: Pointer to the master interface
 * @sta_4a: When AP_VLAN interface are used for WDS (i.e. wireless connection
 * between several APs) this is the 'gateway' sta to 'master' AP
 */
struct rwnx_vif {
    struct list_head list;
    struct rwnx_hw *rwnx_hw;
    struct wireless_dev wdev;
    struct net_device *ndev;
    struct net_device_stats net_stats;
    struct rwnx_key key[6];
    u8 drv_vif_index;
    u8 vif_index;
    u8 ch_index;
    bool up;
    bool use_4addr;
    bool is_resending;
    bool roc_tdls;
    u8 tdls_status;
    bool tdls_chsw_prohibited;
    int generation;
    u32 filter;
    union
    {
        struct
        {
            u32 flags;
            struct rwnx_sta *ap;
            struct rwnx_sta *tdls_sta;
            u8 *ft_assoc_ies;
            int ft_assoc_ies_len;
            u8 ft_target_ap[ETH_ALEN];
            u8 scan_hang;
            u16 scan_duration;
            u8 cancel_scan_cfm;
        } sta;
        struct
        {
            u32 flags;
            struct list_head sta_list;
            struct rwnx_bcn bcn;
            int bcn_interval;
            u8 bcmc_index;
            struct rwnx_csa *csa;

            struct list_head mpath_list;
            struct list_head proxy_list;
            enum nl80211_mesh_power_mode mesh_pm;
            enum nl80211_mesh_power_mode next_mesh_pm;
        } ap;
        struct
        {
            struct rwnx_vif *master;
            struct rwnx_sta *sta_4a;
        } ap_vlan;
    };
};

#define RWNX_INVALID_VIF 0xFF
#define RWNX_VIF_TYPE(vif) (vif->wdev.iftype)

/**
 * Structure used to store information relative to PS mode.
 *
 * @active: True when the sta is in PS mode.
 *          If false, other values should be ignored
 * @pkt_ready: Number of packets buffered for the sta in drv's txq
 *             (1 counter for Legacy PS and 1 for U-APSD)
 * @sp_cnt: Number of packets that remain to be pushed in the service period.
 *          0 means that no service period is in progress
 *          (1 counter for Legacy PS and 1 for U-APSD)
 */
struct rwnx_sta_ps {
    bool active;
    u16 pkt_ready[2];
    u16 sp_cnt[2];
};

/**
 * struct rwnx_rx_rate_stats - Store statistics for RX rates
 *
 * @table: Table indicating how many frame has been receive which each
 * rate index. Rate index is the same as the one used by RC algo for TX
 * @size: Size of the table array
 * @cpt: number of frames received
 * @rate_cnt: number of rate for which at least one frame has been received
 */
struct rwnx_rx_rate_stats {
    int *table;
    int size;
    int cpt;
    int rate_cnt;
};

/**
 * struct rwnx_sta_stats - Structure Used to store statistics specific to a STA
 *
 * @rx_pkts: Number of MSDUs and MMPDUs received from this STA
 * @tx_pkts: Number of MSDUs and MMPDUs sent to this STA
 * @rx_bytes: Total received bytes (MPDU length) from this STA
 * @tx_bytes: Total transmitted bytes (MPDU length) to this STA
 * @last_act: Timestamp (jiffies) when the station was last active (i.e. sent a
 frame: data, mgmt or ctrl )
 * @last_rx: Hardware vector of the last received frame
 * @rx_rate: Statistics of the received rates
 */
struct rwnx_sta_stats {
    u32 rx_pkts;
    u32 tx_pkts;
    u64 rx_bytes;
    u64 tx_bytes;
    unsigned long last_act;
    struct hw_vect last_rx;
#ifdef CONFIG_RWNX_DEBUGFS
    struct rwnx_rx_rate_stats rx_rate;
#endif
};

/**
 * struct rwnx_sta - Managed STATION information
 *
 * @list: List element of rwnx_vif->ap.sta_list
 * @valid: Flag indicating if the entry is valid
 * @mac_addr:  MAC address of the station
 * @aid: association ID
 * @sta_idx: Firmware identifier of the station
 * @vif_idx: Firmware of the VIF the station belongs to
 * @vlan_idx: Identifier of the VLAN VIF the station belongs to (= vif_idx if
 * no vlan in used)
 * @band: Band (only used by TDLS, can be replaced by channel context)
 * @width: Channel width
 * @center_freq: Center frequency
 * @center_freq1: Center frequency 1
 * @center_freq2: Center frequency 2
 * @ch_idx: Identifier of the channel context linked to the station
 * @qos: Flag indicating if the station supports QoS
 * @acm: Bitfield indicating which queues have AC mandatory
 * @uapsd_tids: Bitfield indicating which tids are subject to UAPSD
 * @key: Information on the pairwise key install for this station
 * @ps: Information when STA is in PS (AP only)
 * @bfm_report: Beamforming report to be used for VHT TX Beamforming
 * @group_info: MU grouping information for the STA
 * @ht: Flag indicating if the station supports HT
 * @vht: Flag indicating if the station supports VHT
 * @ac_param[AC_MAX]: EDCA parameters
 * @tdls: TDLS station information
 * @stats: Station statistics
 * @mesh_pm: link-specific mesh power save mode
 * @listen_interval: listen interval (only for AP client)
 */
struct rwnx_sta {
    struct list_head list;
    bool valid;
    u8 mac_addr[ETH_ALEN];
    u16 aid;
    u8 sta_idx;
    u8 vif_idx;
    u8 vlan_idx;
    enum nl80211_band band;
    enum nl80211_chan_width width;
    u16 center_freq;
    u32 center_freq1;
    u32 center_freq2;
    u8 ch_idx;
    bool qos;
    u8 acm;
    u16 uapsd_tids;
    struct rwnx_key key;
    struct rwnx_sta_ps ps;
#ifdef CONFIG_RWNX_BFMER
    struct rwnx_bfmer_report *bfm_report;
#ifdef CONFIG_RWNX_MUMIMO_TX
    struct rwnx_sta_group_info group_info;
#endif /* CONFIG_RWNX_MUMIMO_TX */
#endif /* CONFIG_RWNX_BFMER */
    bool ht;
    bool vht;
    u32 ac_param[AC_MAX];
    struct rwnx_tdls tdls;
    struct rwnx_sta_stats stats;
    enum nl80211_mesh_power_mode mesh_pm;
    int listen_interval;
    struct twt_setup_ind twt_ind; /*TWT Setup indication*/
};

#define RWNX_INVALID_STA 0xFF

/**
 * rwnx_sta_addr - Return MAC address of a STA
 *
 * @sta: Station whose address is returned
 */
static inline const u8 *rwnx_sta_addr(struct rwnx_sta *sta) {
    return sta->mac_addr;
}

#ifdef CONFIG_RWNX_SPLIT_TX_BUF
/**
 * struct rwnx_amsdu_stats - A-MSDU statistics
 *
 * @done: Number of A-MSDU push the firmware
 * @failed: Number of A-MSDU that failed to transit
 */
struct rwnx_amsdu_stats {
    int done;
    int failed;
};
#endif

/**
 * struct rwnx_stats - Global statistics
 *
 * @cfm_balance: Number of buffer currently pushed to firmware per HW queue
 * @ampdus_tx: Number of A-MPDU transmitted (indexed by A-MPDU length)
 * @ampdus_rx: Number of A-MPDU received (indexed by A-MPDU length)
 * @ampdus_rx_map: Internal variable to distinguish A-MPDU
 * @ampdus_rx_miss: Number of MPDU non missing while receiving a-MPDU
 * @ampdu_rx_last: Index (of ampdus_rx_map) of the last A-MPDU received.
 * @amsdus: statistics of a-MSDU transmitted
 * @amsdus_rx: Number of A-MSDU received (indexed by A-MSDU length)
 */
struct rwnx_stats {
    int cfm_balance[NX_TXQ_CNT];
    int ampdus_tx[IEEE80211_MAX_AMPDU_BUF];
    int ampdus_rx[IEEE80211_MAX_AMPDU_BUF];
    int ampdus_rx_map[4];
    int ampdus_rx_miss;
    int ampdus_rx_last;
#ifdef CONFIG_RWNX_SPLIT_TX_BUF
    struct rwnx_amsdu_stats amsdus[NX_TX_PAYLOAD_MAX];
#endif
    int amsdus_rx[64];
};


// maximum number of TX frame per RoC
#define NX_ROC_TX 5
/**
 * struct rwnx_roc - Remain On Channel information
 *
 * @vif: VIF for which RoC is requested
 * @chan: Channel to remain on
 * @duration: Duration in ms
 * @internal: Whether RoC has been started internally by the driver (e.g. to send
 * mgmt frame) or requested by user space
 * @on_chan: Indicate if we have switch on the RoC channel
 * @tx_cnt: Number of MGMT frame sent using this RoC
 * @tx_cookie: Cookie as returned by rwnx_cfg80211_mgmt_tx if Roc has been started
 * to send mgmt frame (valid only if internal is true)
 */
struct rwnx_roc {
    struct rwnx_vif *vif;
    struct ieee80211_channel *chan;
    unsigned int duration;
    bool internal;
    bool on_chan;
    int tx_cnt;
    u64 tx_cookie[NX_ROC_TX];
};

/**
 * struct rwnx_survey_info - Channel Survey Information
 *
 * @filled: filled bitfield as per struct survey_info
 * @chan_time_ms: Amount of time in ms the radio spent on the channel
 * @chan_time_busy_ms: Amount of time in ms the primary channel was sensed busy
 * @noise_dbm: Noise in dbm
 */
struct rwnx_survey_info {
    u32 filled;
    u32 chan_time_ms;
    u32 chan_time_busy_ms;
    s8 noise_dbm;
};

/**
 * rwnx_chanctx - Channel context information
 *
 * @chan_def: Channel description
 * @count: number of vif using this channel context
 */
struct rwnx_chanctx {
    struct cfg80211_chan_def chan_def;
    u8 count;
};

#define RWNX_CH_NOT_SET 0xFF

/**
 * rwnx_phy_info - Phy information
 *
 * @cnt: Number of phy interface
 * @cfg: Configuration send to firmware
 * @sec_chan: Channel configuration of the second phy interface (if phy_cnt > 1)
 * @limit_bw: Set to true to limit BW on requested channel. Only set to use
 * VHT with old radio that don't support 80MHz (deprecated)
 */
struct rwnx_phy_info {
    u8 cnt;
    struct phy_cfg_tag cfg;
    struct mac_chan_op sec_chan;
    bool limit_bw;
};

enum wifi_suspend_state {
    WIFI_SUSPEND_STATE_NONE,
    WIFI_SUSPEND_STATE_WOW,
    WIFI_SUSPEND_STATE_DEEPSLEEP,
};

#define WOW_MAX_PATTERNS 4

/* wake up filter in wow mode */
#define WOW_FILTER_OPTION_ANY BIT(1)
#define WOW_FILTER_OPTION_MAGIC_PACKET BIT(2)
#define WOW_FILTER_OPTION_EAP_REQ BIT(3)
#define WOW_FILTER_OPTION_4WAYHS BIT(4)
#define WOW_FILTER_OPTION_DISCONNECT BIT(5)
#define WOW_FILTER_OPTION_GTK_ERROR BIT(6)


/**
 * struct rwnx_hw - RWNX driver main data
 *
 * @dev: Device structure
 *
 * @plat: Underlying RWNX platform information
 * @phy: PHY Configuration
 * @version_cfm: MACSW/HW versions (as obtained via MM_VERSION_REQ)
 * @machw_type: Type of MACHW (see RWNX_MACHW_xxx)
 *
 * @mod_params: Driver parameters
 * @flags: Global flags, see enum rwnx_dev_flag
 * @task: Tasklet to execute firmware IRQ bottom half
 * @wiphy: Wiphy structure
 * @ext_capa: extended capabilities supported
 *
 * @vifs: List of VIFs currently created
 * @vif_table: Table of all possible VIFs, indexed with fw id.
 * (NX_REMOTE_STA_MAX extra elements for AP_VLAN interface)
 * @vif_started: Number of vif created at firmware level
 * @avail_idx_map: Bitfield of created interface (indexed by rwnx_vif.drv_vif_index)
 * @monitor_vif:  FW id of the monitor interface (RWNX_INVALID_VIF if no monitor vif)
 *
 * @sta_table: Table of all possible Stations
 * (NX_VIRT_DEV_MAX] extra elements for BroadCast/MultiCast stations)
 *
 * @chanctx_table: Table of all possible Channel contexts
 * @cur_chanctx: Currently active Channel context
 * @survey: Table of channel surveys
 * @roc: Information of current Remain on Channel request (NULL if Roc requested)
 * @scan_request: Information of current scan request
 * @radar: Radar detection information
 *
 * @tx_lock: Spinlock to protect TX path
 * @txq: Table of STA/TID TX queues
 * @hwq: Table of MACHW queues
 * @txq_cleanup: Timer list to drop packet queued too long in TXQs
 * @sw_txhdr_cache: Cache to allocate
 * @tcp_pacing_shift: TCP buffering configuration (buffering is ~ 2^(10-tps) ms)
 * @mu: Information for Multiuser TX groups
 *
 * @defer_rx: Work to defer RX processing out of IRQ tasklet. Only use for specific mgmt frames
 *
 * @ipc_env: IPC environment
 * @cmd_mgr: FW command manager
 * @cb_lock: Spinlock to protect code from FW confirmation/indication message
 *
 * @msgbuf_pool: Pool of shared buffers to retrieve FW messages
 * @dbgbuf_pool: Pool of shared buffers to retrieve FW debug messages
 * @radar_pool: Pool of shared buffers to retrieve FW radar events
 * @tx_pattern: Shared buffer for the FW to download end of TX buf pattern from
 * @dbgdump: Shared buffer to retrieve FW debug dump
 * @rxdesc_pool: Pool of shared buffers to retrieve RX descriptors
 * @rxbufs: Table of shared buffers to retrieve RX data
 * @rxbuf_idx: Index of the last allocated buffer in rxbufs table
 * @unsuprxvecs: Table of shared buffers to retrieve FW unsupported frames
 * @scan_ie: Shared buffer, allocated to push probe request elements to the FW
 *
 * @debugfs: Debug FS entries
 * @stats: global statistics
 */
struct rwnx_hw {
    struct device *dev;

    // Hardware info
    struct rwnx_plat *plat;
    struct rwnx_phy_info phy;
    struct mm_version_cfm version_cfm;
    int machw_type;

    // Global wifi config
    struct rwnx_mod_params *mod_params;
    unsigned long flags;
    struct wiphy *wiphy;
    u8 ext_capa[10];

    // VIFs
    struct list_head vifs;
    struct rwnx_vif *vif_table[NX_ITF_MAX];
    int vif_started;
    u16 avail_idx_map;
    u8 monitor_vif;
    enum wifi_suspend_state state;

    // Stations
    struct rwnx_sta sta_table[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];

    // Channels
    struct rwnx_chanctx chanctx_table[NX_CHAN_CTXT_CNT];
    u8 cur_chanctx;
    struct rwnx_survey_info survey[SCAN_CHANNEL_MAX];
    struct rwnx_roc *roc;
    struct cfg80211_scan_request *scan_request;
    struct rwnx_radar radar;

    // TX path
    spinlock_t tx_lock;
    struct rwnx_txq txq[NX_NB_TXQ];
    struct rwnx_hwq hwq[NX_TXQ_CNT];
    struct timer_list txq_cleanup;
    struct kmem_cache *sw_txhdr_cache;
    u32 tcp_pacing_shift;
#ifdef CONFIG_RWNX_MUMIMO_TX
    struct rwnx_mu_info mu;
#endif

    // RX path
    struct rwnx_defer_rx defer_rx;

    // IRQ
    struct tasklet_struct task;

    // IPC
    struct ipc_host_env_tag *ipc_env;
    struct rwnx_cmd_mgr cmd_mgr;
    spinlock_t cb_lock;

    // Shared buffers
    struct rwnx_ipc_buf_pool msgbuf_pool;
    struct rwnx_ipc_buf_pool dbgbuf_pool;
    struct rwnx_ipc_buf_pool radar_pool;
    struct rwnx_ipc_buf tx_pattern;
    struct rwnx_ipc_dbgdump dbgdump;
    struct rwnx_ipc_buf_pool rxdesc_pool;
    struct rwnx_ipc_buf rxbufs[RWNX_RXBUFF_MAX];
    int rxbuf_idx;
    struct rwnx_ipc_buf unsuprxvecs[IPC_UNSUPRXVECBUF_CNT];
    struct rwnx_ipc_buf scan_ie;
    struct rwnx_ipc_buf_pool txcfm_pool;

    // Debug FS and stats
    struct rwnx_debugfs debugfs;
    struct rwnx_stats stats;
};

u8 *rwnx_build_bcn(struct rwnx_bcn *bcn, struct cfg80211_beacon_data *new);

void rwnx_chanctx_link(struct rwnx_vif *vif, u8 idx,
                        struct cfg80211_chan_def *chandef);
void rwnx_chanctx_unlink(struct rwnx_vif *vif);
int  rwnx_chanctx_valid(struct rwnx_hw *rwnx_hw, u8 idx);

static inline bool is_multicast_sta(int sta_idx)
{
    return (sta_idx >= NX_REMOTE_STA_MAX);
}
struct rwnx_sta *rwnx_get_sta(struct rwnx_hw *rwnx_hw, const u8 *mac_addr);

static inline uint8_t master_vif_idx(struct rwnx_vif *vif)
{
    if (unlikely(vif->wdev.iftype == NL80211_IFTYPE_AP_VLAN)) {
        return vif->ap_vlan.master->vif_index;
    } else {
        return vif->vif_index;
    }
}

static inline void *rwnx_get_shared_trace_buf(struct rwnx_hw *rwnx_hw)
{
    return (void *)&(rwnx_hw->debugfs.fw_trace.buf);
}

void rwnx_external_auth_enable(struct rwnx_vif *vif);
void rwnx_external_auth_disable(struct rwnx_vif *vif);

#endif /* _RWNX_DEFS_H_ */
