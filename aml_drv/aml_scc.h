#ifndef _AML_SCC_H_
#define _AML_SCC_H_

#include "aml_defs.h"

enum
{
    BEACON_NO_UPDATE,
    BEACON_UPDATE_WAIT_PROBE,
    BEACON_UPDATE_WAIT_DOWNLOAD
};

#define AML_SCC_BEACON_SET_STATUS(x)         (beacon_need_update = x)
#define AML_SCC_BEACON_WAIT_PROBE()         (beacon_need_update == BEACON_UPDATE_WAIT_PROBE)
#define AML_SCC_BEACON_WAIT_DOWNLOAD()      (beacon_need_update == BEACON_UPDATE_WAIT_DOWNLOAD)
#define AML_SCC_CLEAR_BEACON_UPDATE()       (beacon_need_update = 0)

extern u8 bcn_save[];
extern u32 beacon_need_update;

extern int aml_scc_change_beacon(struct aml_hw *aml_hw,struct aml_vif *vif);
extern u8 aml_scc_get_confilct_vif_idx(struct aml_vif *incoming_vif);
extern void aml_scc_save_bcn_buf(u8* bcn_buf,size_t len);
extern bool aml_handle_scc_chan_switch(struct aml_vif *vif,struct aml_vif *target_vif);
extern void aml_scc_check_chan_confilct(struct aml_hw *aml_hw);
extern void aml_scc_save_init_band(u32 init_band);
extern void aml_scc_save_probe_rsp(struct aml_vif *vif,u8* buf);
extern void aml_scc_init(void);
extern void aml_scc_deinit(void);

#endif /* _AML_SCC_H_ */
