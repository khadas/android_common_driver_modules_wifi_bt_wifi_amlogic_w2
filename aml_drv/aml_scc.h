#ifndef _AML_SCC_H_
#define _AML_SCC_H_

#include "aml_defs.h"
#include "aml_wq.h"

enum
{
    BEACON_NO_UPDATE,
    BEACON_UPDATE_WAIT_PROBE,
    BEACON_UPDATE_WAIT_DOWNLOAD
};

#define AML_SCC_BEACON_SET_STATUS(x)        (beacon_need_update = x)
#define AML_SCC_BEACON_WAIT_PROBE()         (beacon_need_update == BEACON_UPDATE_WAIT_PROBE)
#define AML_SCC_BEACON_WAIT_DOWNLOAD()      (beacon_need_update == BEACON_UPDATE_WAIT_DOWNLOAD)
#define AML_SCC_CLEAR_BEACON_UPDATE()       (beacon_need_update = 0)

extern u8 bcn_save[];
extern u32 beacon_need_update;

int aml_scc_change_beacon(struct aml_hw *aml_hw, struct aml_vif *vif);
u8 aml_scc_get_conflict_vif_idx(struct aml_vif *incoming_vif);
void aml_scc_save_bcn_buf(u8 *bcn_buf, size_t len);
bool aml_handle_scc_chan_switch(struct aml_vif *vif, struct aml_vif *target_vif);
void aml_scc_check_chan_conflict(struct aml_hw *aml_hw);
void aml_scc_save_init_band(u32 init_band);
void aml_scc_save_probe_rsp(struct aml_vif *vif, u8 *buf, u32 buf_len);
void aml_scc_init(void);
void aml_scc_deinit(void);
void aml_scc_sync_bcn(struct aml_hw *aml_hw, struct aml_wq *aml_wq);

#endif /* _AML_SCC_H_ */
