/**
 ******************************************************************************
 *
 * @file rwnx_debugfs.h
 *
 * @brief Miscellaneous utility function definitions
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */


#ifndef _RWNX_DEBUGFS_H_
#define _RWNX_DEBUGFS_H_

#include <linux/workqueue.h>
#include <linux/if_ether.h>
#include "hal_desc.h"
#include "rwnx_fw_trace.h"

struct rwnx_hw;
struct rwnx_sta;
struct rwnx_vif;
struct rwnx_txq;

#define DEBUGFS_ADD_FILE(name, parent, mode) do {                  \
        struct dentry *__tmp;                                      \
        __tmp = debugfs_create_file(#name, mode, parent, rwnx_hw,  \
                                    &rwnx_dbgfs_##name##_ops);     \
        if (IS_ERR_OR_NULL(__tmp))                                 \
            goto err;                                              \
    } while (0)

#define DEBUGFS_ADD_BOOL(name, parent, ptr) do {                            \
        struct dentry *__tmp;                                               \
        __tmp = debugfs_create_bool(#name, S_IWUSR | S_IRUSR, parent, ptr); \
        if (IS_ERR_OR_NULL(__tmp))                                          \
            goto err;                                                       \
    } while (0)

#define DEBUGFS_ADD_X64(name, parent, ptr) do {                         \
        debugfs_create_x64(#name, S_IWUSR | S_IRUSR,parent, ptr);       \
    } while (0)

#define DEBUGFS_ADD_U64(name, parent, ptr, mode) do {           \
        debugfs_create_u64(#name, mode, parent, ptr);           \
    } while (0)

#define DEBUGFS_ADD_X32(name, parent, ptr) do {                         \
        debugfs_create_x32(#name, S_IWUSR | S_IRUSR, parent, ptr);      \
    } while (0)

#define DEBUGFS_ADD_U32(name, parent, ptr, mode) do {           \
        debugfs_create_u32(#name, mode, parent, ptr);           \
    } while (0)


/* file operation */
#define DEBUGFS_READ_FUNC(name)                                         \
    static ssize_t rwnx_dbgfs_##name##_read(struct file *file,          \
                                            char __user *user_buf,      \
                                            size_t count, loff_t *ppos);

#define DEBUGFS_WRITE_FUNC(name)                                         \
    static ssize_t rwnx_dbgfs_##name##_write(struct file *file,          \
                                             const char __user *user_buf,\
                                             size_t count, loff_t *ppos);

#define DEBUGFS_OPEN_FUNC(name)                              \
    static int rwnx_dbgfs_##name##_open(struct inode *inode, \
                                        struct file *file);

#define DEBUGFS_RELEASE_FUNC(name)                              \
    static int rwnx_dbgfs_##name##_release(struct inode *inode, \
                                           struct file *file);

#define DEBUGFS_READ_FILE_OPS(name)                             \
    DEBUGFS_READ_FUNC(name);                                    \
static const struct file_operations rwnx_dbgfs_##name##_ops = { \
    .read   = rwnx_dbgfs_##name##_read,                         \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_WRITE_FILE_OPS(name)                            \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations rwnx_dbgfs_##name##_ops = { \
    .write  = rwnx_dbgfs_##name##_write,                        \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_READ_WRITE_FILE_OPS(name)                       \
    DEBUGFS_READ_FUNC(name);                                    \
    DEBUGFS_WRITE_FUNC(name);                                   \
static const struct file_operations rwnx_dbgfs_##name##_ops = { \
    .write  = rwnx_dbgfs_##name##_write,                        \
    .read   = rwnx_dbgfs_##name##_read,                         \
    .open   = simple_open,                                      \
    .llseek = generic_file_llseek,                              \
};

#define DEBUGFS_READ_WRITE_OPEN_RELEASE_FILE_OPS(name)              \
    DEBUGFS_READ_FUNC(name);                                        \
    DEBUGFS_WRITE_FUNC(name);                                       \
    DEBUGFS_OPEN_FUNC(name);                                        \
    DEBUGFS_RELEASE_FUNC(name);                                     \
static const struct file_operations rwnx_dbgfs_##name##_ops = {     \
    .write   = rwnx_dbgfs_##name##_write,                           \
    .read    = rwnx_dbgfs_##name##_read,                            \
    .open    = rwnx_dbgfs_##name##_open,                            \
    .release = rwnx_dbgfs_##name##_release,                         \
    .llseek  = generic_file_llseek,                                 \
};


#define TXQ_STA_PREF "tid|"
#define TXQ_STA_PREF_FMT "%3d|"

#ifdef CONFIG_RWNX_FULLMAC
#define TXQ_VIF_PREF "type|"
#define TXQ_VIF_PREF_FMT "%4s|"
#else
#define TXQ_VIF_PREF "AC|"
#define TXQ_VIF_PREF_FMT "%2s|"
#endif /* CONFIG_RWNX_FULLMAC */

#define TXQ_HDR "idx|  status|credit|ready|retry|pushed"
#define TXQ_HDR_FMT "%3d|%s%s%s%s%s%s%s%s|%6d|%5d|%5d|%6d"

#ifdef CONFIG_RWNX_AMSDUS_TX
#ifdef CONFIG_RWNX_FULLMAC
#define TXQ_HDR_SUFF "|amsdu"
#define TXQ_HDR_SUFF_FMT "|%5d"
#else
#define TXQ_HDR_SUFF "|amsdu-ht|amdsu-vht"
#define TXQ_HDR_SUFF_FMT "|%8d|%9d"
#endif /* CONFIG_RWNX_FULLMAC */
#else
#define TXQ_HDR_SUFF ""
#define TXQ_HDR_SUF_FMT ""
#endif /* CONFIG_RWNX_AMSDUS_TX */

#define TXQ_HDR_MAX_LEN (sizeof(TXQ_STA_PREF) + sizeof(TXQ_HDR) + sizeof(TXQ_HDR_SUFF) + 1)

#ifdef CONFIG_RWNX_FULLMAC
#define PS_HDR  "Legacy PS: ready=%d, sp=%d / UAPSD: ready=%d, sp=%d"
#define PS_HDR_LEGACY "Legacy PS: ready=%d, sp=%d"
#define PS_HDR_UAPSD  "UAPSD: ready=%d, sp=%d"
#define PS_HDR_MAX_LEN  sizeof("Legacy PS: ready=xxx, sp=xxx / UAPSD: ready=xxx, sp=xxx\n")
#else
#define PS_HDR ""
#define PS_HDR_MAX_LEN 0
#endif /* CONFIG_RWNX_FULLMAC */

#define STA_HDR "** STA %d (%pM)\n"
#define STA_HDR_MAX_LEN sizeof("- STA xx (xx:xx:xx:xx:xx:xx)\n") + PS_HDR_MAX_LEN

#ifdef CONFIG_RWNX_FULLMAC
#define VIF_HDR "* VIF [%d] %s\n"
#define VIF_HDR_MAX_LEN sizeof(VIF_HDR) + IFNAMSIZ
#else
#define VIF_HDR "* VIF [%d]\n"
#define VIF_HDR_MAX_LEN sizeof(VIF_HDR)
#endif


#ifdef CONFIG_RWNX_AMSDUS_TX

#ifdef CONFIG_RWNX_FULLMAC
#define VIF_SEP "---------------------------------------\n"
#else
#define VIF_SEP "----------------------------------------------------\n"
#endif /* CONFIG_RWNX_FULLMAC */

#else /* ! CONFIG_RWNX_AMSDUS_TX */
#define VIF_SEP "---------------------------------\n"
#endif /* CONFIG_RWNX_AMSDUS_TX*/

#define VIF_SEP_LEN sizeof(VIF_SEP)

#define CAPTION "status: L=in hwq list, F=stop full, P=stop sta PS, V=stop vif PS,\
 C=stop channel, S=stop CSA, M=stop MU, N=Ndev queue stopped"
#define CAPTION_LEN sizeof(CAPTION)

#define STA_TXQ 0
#define VIF_TXQ 1

#define LINE_MAX_SZ 150

struct st {
    char line[LINE_MAX_SZ + 1];
    unsigned int r_idx;
};


#ifdef CONFIG_RWNX_DEBUGFS

struct rwnx_debugfs {
    unsigned long long rateidx;
    struct dentry *dir;
    struct dentry *dir_stas;
    bool trace_prst;

    char helper_cmd[64];
    struct work_struct helper_work;
    bool helper_scheduled;
    spinlock_t umh_lock;
    bool unregistering;

#ifndef CONFIG_RWNX_FHOST
    struct rwnx_fw_trace fw_trace;
#endif /* CONFIG_RWNX_FHOST */

#ifdef CONFIG_RWNX_FULLMAC
    struct work_struct sta_work;
    struct dentry *dir_sta[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];
    uint8_t sta_idx;
    struct dentry *dir_rc;
    struct dentry *dir_rc_sta[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];
    int rc_config[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];
    struct list_head rc_config_save;
    struct dentry *dir_twt;
    struct dentry *dir_twt_sta[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];
#endif
};

#ifdef CONFIG_RWNX_FULLMAC

// Max duration in msecs to save rate config for a sta after disconnection
#define RC_CONFIG_DUR 600000

struct rwnx_rc_config_save {
    struct list_head list;
    unsigned long timestamp;
    int rate;
    u8 mac_addr[ETH_ALEN];
};
#endif

int rwnx_dbgfs_register(struct rwnx_hw *rwnx_hw, const char *name);
void rwnx_dbgfs_unregister(struct rwnx_hw *rwnx_hw);
int rwnx_um_helper(struct rwnx_debugfs *rwnx_debugfs, const char *cmd);
int rwnx_trigger_um_helper(struct rwnx_debugfs *rwnx_debugfs);
void rwnx_wait_um_helper(struct rwnx_hw *rwnx_hw);
#ifdef CONFIG_RWNX_FULLMAC
void rwnx_dbgfs_register_sta(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta);
void rwnx_dbgfs_unregister_sta(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta);
#endif

int rwnx_dbgfs_register_fw_dump(struct rwnx_hw *rwnx_hw,
                                struct dentry *dir_drv,
                                struct dentry *dir_diags);
void rwnx_dbgfs_trigger_fw_dump(struct rwnx_hw *rwnx_hw, char *reason);

void rwnx_fw_trace_dump(struct rwnx_hw *rwnx_hw);
void rwnx_fw_trace_reset(struct rwnx_hw *rwnx_hw);
void idx_to_rate_cfg(int idx, union rwnx_rate_ctrl_info *r_cfg, int *ru_size);
int rwnx_dbgfs_txq(char *buf, size_t size, struct rwnx_txq *txq, int type, int tid, char *name);
int rwnx_dbgfs_txq_sta(char *buf, size_t size, struct rwnx_sta *rwnx_sta, struct rwnx_hw *rwnx_hw);
int rwnx_dbgfs_txq_vif(char *buf, size_t size, struct rwnx_vif *rwnx_vif, struct rwnx_hw *rwnx_hw);
int print_rate(char *buf, int size, int format, int nss, int mcs, int bw,
                      int sgi, int pre, int dcm, int *r_idx);
int print_rate_from_cfg(char *buf, int size, u32 rate_config, int *r_idx, int ru_size);
int compare_idx(const void *st1, const void *st2);

#else

struct rwnx_debugfs {
};

static inline int rwnx_dbgfs_register(struct rwnx_hw *rwnx_hw, const char *name) { return 0; }
static inline void rwnx_dbgfs_unregister(struct rwnx_hw *rwnx_hw) {}
static inline int rwnx_um_helper(struct rwnx_debugfs *rwnx_debugfs, const char *cmd) { return 0; }
static inline int rwnx_trigger_um_helper(struct rwnx_debugfs *rwnx_debugfs) {return 0;}
static inline void rwnx_wait_um_helper(struct rwnx_hw *rwnx_hw) {}
#ifdef CONFIG_RWNX_FULLMAC
static inline void rwnx_dbgfs_register_sta(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta)  {}
static inline void rwnx_dbgfs_unregister_sta(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta)  {}
#endif

void rwnx_fw_trace_dump(struct rwnx_hw *rwnx_hw) {}
void rwnx_fw_trace_reset(struct rwnx_hw *rwnx_hw) {}


#endif /* CONFIG_RWNX_DEBUGFS */


#endif /* _RWNX_DEBUGFS_H_ */
