/**
 ****************************************************************************************
 *
 * @file rwnx_msg_tx.h
 *
 * @brief TX function declarations
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ****************************************************************************************
 */

#ifndef _RWNX_MSG_TX_H_
#define _RWNX_MSG_TX_H_

#include "rwnx_defs.h"

int rwnx_send_reset(struct rwnx_hw *rwnx_hw);
int rwnx_send_start(struct rwnx_hw *rwnx_hw);
int rwnx_send_version_req(struct rwnx_hw *rwnx_hw, struct mm_version_cfm *cfm);
int rwnx_send_add_if(struct rwnx_hw *rwnx_hw, const unsigned char *mac,
                     enum nl80211_iftype iftype, bool p2p, struct mm_add_if_cfm *cfm);
int rwnx_send_remove_if(struct rwnx_hw *rwnx_hw, u8 vif_index);
int rwnx_send_set_channel(struct rwnx_hw *rwnx_hw, int phy_idx,
                          struct mm_set_channel_cfm *cfm);
int rwnx_send_key_add(struct rwnx_hw *rwnx_hw, u8 vif_idx, u8 sta_idx, bool pairwise,
                      u8 *key, u8 key_len, u8 key_idx, u8 cipher_suite,
                      struct mm_key_add_cfm *cfm);
int rwnx_send_key_del(struct rwnx_hw *rwnx_hw, uint8_t hw_key_idx);
int rwnx_send_bcn_change(struct rwnx_hw *rwnx_hw, u8 vif_idx, u32 bcn_addr,
                         u16 bcn_len, u16 tim_oft, u16 tim_len, u16 *csa_oft);
int rwnx_send_tim_update(struct rwnx_hw *rwnx_hw, u8 vif_idx, u16 aid,
                         u8 tx_status);
int rwnx_send_roc(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                  struct ieee80211_channel *chan, unsigned int duration);
int rwnx_send_cancel_roc(struct rwnx_hw *rwnx_hw);
int rwnx_send_set_power(struct rwnx_hw *rwnx_hw,  u8 vif_idx, s8 pwr,
                        struct mm_set_power_cfm *cfm);
int rwnx_send_set_edca(struct rwnx_hw *rwnx_hw, u8 hw_queue, u32 param,
                       bool uapsd, u8 inst_nbr);
int rwnx_send_tdls_chan_switch_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                                   struct rwnx_sta *rwnx_sta, bool sta_initiator,
                                   u8 oper_class, struct cfg80211_chan_def *chandef,
                                   struct tdls_chan_switch_cfm *cfm);
int rwnx_send_tdls_cancel_chan_switch_req(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_vif *rwnx_vif,
                                          struct rwnx_sta *rwnx_sta,
                                          struct tdls_cancel_chan_switch_cfm *cfm);

#ifdef CONFIG_RWNX_P2P_DEBUGFS
int rwnx_send_p2p_oppps_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                            u8 ctw, struct mm_set_p2p_oppps_cfm *cfm);
int rwnx_send_p2p_noa_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                          int count, int interval, int duration,
                          bool dyn_noa, struct mm_set_p2p_noa_cfm *cfm);
#endif /* CONFIG_RWNX_P2P_DEBUGFS */

#ifdef CONFIG_RWNX_SOFTMAC
int rwnx_send_sta_add(struct rwnx_hw *rwnx_hw, struct ieee80211_sta *sta,
                      u8 inst_nbr, struct mm_sta_add_cfm *cfm);
int rwnx_send_sta_del(struct rwnx_hw *rwnx_hw, u8 sta_idx);
int rwnx_send_set_filter(struct rwnx_hw *rwnx_hw, uint32_t filter);
int rwnx_send_add_chanctx(struct rwnx_hw *rwnx_hw,
                          struct ieee80211_chanctx_conf *ctx,
                          struct mm_chan_ctxt_add_cfm *cfm);
int rwnx_send_del_chanctx(struct rwnx_hw *rwnx_hw, u8 index);
int rwnx_send_link_chanctx(struct rwnx_hw *rwnx_hw, u8 vif_idx, u8 chan_idx,
                           u8 chan_switch);
int rwnx_send_unlink_chanctx(struct rwnx_hw *rwnx_hw, u8 vif_idx);
int rwnx_send_update_chanctx(struct rwnx_hw *rwnx_hw,
                             struct ieee80211_chanctx_conf *ctx);
int rwnx_send_sched_chanctx(struct rwnx_hw *rwnx_hw, u8 vif_idx, u8 chan_idx,
                            u8 type);

int rwnx_send_dtim_req(struct rwnx_hw *rwnx_hw, u8 dtim_period);
int rwnx_send_set_br(struct rwnx_hw *rwnx_hw, u32 basic_rates, u8 vif_idx, u8 band);
int rwnx_send_set_beacon_int(struct rwnx_hw *rwnx_hw, u16 beacon_int, u8 vif_idx);
int rwnx_send_set_bssid(struct rwnx_hw *rwnx_hw, const u8 *bssid, u8 vif_idx);
int rwnx_send_set_vif_state(struct rwnx_hw *rwnx_hw, bool active,
                            u16 aid, u8 vif_idx);
int rwnx_send_set_mode(struct rwnx_hw *rwnx_hw, u8 abgmode);
int rwnx_send_set_idle(struct rwnx_hw *rwnx_hw, int idle);
int rwnx_send_set_ps_mode(struct rwnx_hw *rwnx_hw, u8 ps_mode);
int rwnx_send_set_ps_options(struct rwnx_hw *rwnx_hw, bool listen_bcmc,
                             u16 listen_interval, u8 vif_idx);
int rwnx_send_set_slottime(struct rwnx_hw *rwnx_hw, int use_short_slot);
int rwnx_send_ba_add(struct rwnx_hw *rwnx_hw, uint8_t type, uint8_t sta_idx,
                     u16 tid, uint8_t bufsz, uint16_t ssn,
                     struct mm_ba_add_cfm *cfm);
int rwnx_send_ba_del(struct rwnx_hw *rwnx_hw, uint8_t sta_idx, u16 tid,
                     struct mm_ba_del_cfm *cfm);
int rwnx_send_scan_req(struct rwnx_hw *rwnx_hw, struct ieee80211_vif *vif,
                       struct cfg80211_scan_request *param,
                       struct scan_start_cfm *cfm);
int rwnx_send_scan_cancel_req(struct rwnx_hw *rwnx_hw,
                              struct scan_cancel_cfm *cfm);
void rwnx_send_tdls_ps(struct rwnx_hw *rwnx_hw, bool ps_mode);
#endif /* CONFIG_RWNX_SOFTMAC */

#ifdef CONFIG_RWNX_FULLMAC
int rwnx_send_me_config_req(struct rwnx_hw *rwnx_hw);
int rwnx_send_me_chan_config_req(struct rwnx_hw *rwnx_hw);
int rwnx_send_me_set_control_port_req(struct rwnx_hw *rwnx_hw, bool opened,
                                      u8 sta_idx);
int rwnx_send_me_sta_add(struct rwnx_hw *rwnx_hw, struct station_parameters *params,
                         const u8 *mac, u8 inst_nbr, struct me_sta_add_cfm *cfm);
int rwnx_send_me_sta_del(struct rwnx_hw *rwnx_hw, u8 sta_idx, bool tdls_sta);
int rwnx_send_me_traffic_ind(struct rwnx_hw *rwnx_hw, u8 sta_idx, bool uapsd, u8 tx_status);
int rwnx_send_twt_request(struct rwnx_hw *rwnx_hw,
                          u8 setup_type, u8 vif_idx,
                          struct twt_conf_tag *conf,
                          struct twt_setup_cfm *cfm);
int rwnx_send_twt_teardown(struct rwnx_hw *rwnx_hw,
                           struct twt_teardown_req *twt_teardown,
                           struct twt_teardown_cfm *cfm);
int rwnx_send_me_rc_stats(struct rwnx_hw *rwnx_hw, u8 sta_idx,
                          struct me_rc_stats_cfm *cfm);
int rwnx_send_me_rc_set_rate(struct rwnx_hw *rwnx_hw,
                             u8 sta_idx,
                             u16 rate_idx);
int rwnx_send_me_set_ps_mode(struct rwnx_hw *rwnx_hw, u8 ps_mode);
int rwnx_send_sm_connect_req(struct rwnx_hw *rwnx_hw,
                             struct rwnx_vif *rwnx_vif,
                             struct cfg80211_connect_params *sme,
                             struct sm_connect_cfm *cfm);
int rwnx_send_sm_disconnect_req(struct rwnx_hw *rwnx_hw,
                                struct rwnx_vif *rwnx_vif,
                                u16 reason);
int rwnx_send_sm_external_auth_required_rsp(struct rwnx_hw *rwnx_hw,
                                            struct rwnx_vif *rwnx_vif,
                                            u16 status);
int rwnx_send_sm_ft_auth_rsp(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                             uint8_t *ie, int ie_len);
int rwnx_send_apm_start_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                            struct cfg80211_ap_settings *settings,
                            struct apm_start_cfm *cfm);
int rwnx_send_apm_stop_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif);
int rwnx_send_apm_probe_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                            struct rwnx_sta *sta, struct apm_probe_client_cfm *cfm);
int rwnx_send_scanu_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif,
                        struct cfg80211_scan_request *param);
int rwnx_send_apm_start_cac_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                                struct cfg80211_chan_def *chandef,
                                struct apm_start_cac_cfm *cfm);
int rwnx_send_apm_stop_cac_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif);
int rwnx_send_tdls_peer_traffic_ind_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif);
int rwnx_send_config_monitor_req(struct rwnx_hw *rwnx_hw,
                                 struct cfg80211_chan_def *chandef,
                                 struct me_config_monitor_cfm *cfm);
int rwnx_send_mesh_start_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                             const struct mesh_config *conf, const struct mesh_setup *setup,
                             struct mesh_start_cfm *cfm);
int rwnx_send_mesh_stop_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                            struct mesh_stop_cfm *cfm);
int rwnx_send_mesh_update_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                              u32 mask, const struct mesh_config *p_mconf, struct mesh_update_cfm *cfm);
int rwnx_send_mesh_peer_info_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                                 u8 sta_idx, struct mesh_peer_info_cfm *cfm);
void rwnx_send_mesh_peer_update_ntf(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                                    u8 sta_idx, u8 mlink_state);
void rwnx_send_mesh_path_create_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, u8 *tgt_addr);
int rwnx_send_mesh_path_update_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, const u8 *tgt_addr,
                                   const u8 *p_nhop_addr, struct mesh_path_update_cfm *cfm);
void rwnx_send_mesh_proxy_add_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, u8 *ext_addr);
#endif /* CONFIG_RWNX_FULLMAC */

#ifdef CONFIG_RWNX_BFMER
#ifdef CONFIG_RWNX_SOFTMAC
void rwnx_send_bfmer_enable(struct rwnx_hw *rwnx_hw, struct ieee80211_sta *sta);
#else
void rwnx_send_bfmer_enable(struct rwnx_hw *rwnx_hw, struct rwnx_sta *rwnx_sta,
                            const struct ieee80211_vht_cap *vht_cap);
#endif /* CONFIG_RWNX_SOFTMAC*/
#ifdef CONFIG_RWNX_MUMIMO_TX
int rwnx_send_mu_group_update_req(struct rwnx_hw *rwnx_hw, struct rwnx_sta *rwnx_sta);
#endif /* CONFIG_RWNX_MUMIMO_TX */
#endif /* CONFIG_RWNX_BFMER */

/* Debug messages */
int rwnx_send_dbg_trigger_req(struct rwnx_hw *rwnx_hw, char *msg);
int rwnx_send_dbg_mem_read_req(struct rwnx_hw *rwnx_hw, u32 mem_addr,
                               struct dbg_mem_read_cfm *cfm);
int rwnx_send_dbg_mem_write_req(struct rwnx_hw *rwnx_hw, u32 mem_addr,
                                u32 mem_data);
int rwnx_send_dbg_set_mod_filter_req(struct rwnx_hw *rwnx_hw, u32 filter);
int rwnx_send_dbg_set_sev_filter_req(struct rwnx_hw *rwnx_hw, u32 filter);
int rwnx_send_dbg_get_sys_stat_req(struct rwnx_hw *rwnx_hw,
                                   struct dbg_get_sys_stat_cfm *cfm);
int rwnx_send_cfg_rssi_req(struct rwnx_hw *rwnx_hw, u8 vif_index, int rssi_thold, u32 rssi_hyst);
#ifdef TEST_MODE
int aml_pcie_prssr_test(struct net_device *dev, int start_addr, int len, u32_l payload);
int aml_pcie_dl_malloc_test(struct rwnx_hw *rwnx_hw, int start_addr, int len, u32_l payload);
int aml_pcie_ul_malloc_test(struct rwnx_hw *rwnx_hw, int start_addr, int len, u32_l payload);
#endif
int aml_rf_reg_write(struct net_device *dev, int addr, int value);
int aml_rf_reg_read(struct net_device *dev, int addr);
unsigned int aml_efuse_read(struct rwnx_hw *rwnx_hw, u32 addr);
int aml_scan_hang(struct rwnx_vif *rwnx_vif, int scan_hang);
int rwnx_send_suspend_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, enum wifi_suspend_state state);
int rwnx_send_wow_pattern (struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                        struct cfg80211_pkt_pattern *param, int id);
int rwnx_send_scanu_cancel_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, struct scanu_cancel_cfm *cfm);
int rwnx_send_arp_agent_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, u8 enable, u32 ipv4, u8 *ipv6);
int rwnx_set_rekey_data(struct rwnx_vif *rwnx_vif, const u8 *kek, const u8 *kck, const u8 *replay_ctr);
int rwnx_tko_config_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif,
                        u16 interval, u16 retry_interval, u16 retry_count);
int rwnx_tko_activate_req(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, u8 active);
int rwnx_set_cali_param_req(struct rwnx_hw *rwnx_hw, struct Cali_Param *cali_param);
int rwnx_get_efuse(struct rwnx_vif *rwnx_vif, u32 addr);
int rwnx_set_efuse(struct rwnx_vif *rwnx_vif, u32 addr, u32 value);
int rwnx_recovery(struct rwnx_vif *rwnx_vif);
int rwnx_set_macbypass(struct rwnx_vif *rwnx_vif, int format_type, int bandwidth, int rate, int siso_or_mimo);
int rwnx_set_stop_macbypass(struct rwnx_vif *rwnx_vif);

int rwnx_send_sched_scan_req(struct rwnx_vif *rwnx_vif,
    struct cfg80211_sched_scan_request *request);
int rwnx_send_sched_scan_stop_req(struct rwnx_vif *rwnx_vif, u64 reqid);
int rwnx_set_amsdu_tx(struct rwnx_hw *rwnx_hw, u8 amsdu_tx);
int rwnx_set_tx_lft(struct rwnx_hw *rwnx_hw, u32 tx_lft);
int rwnx_set_ldpc_tx(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif);
int rwnx_set_stbc(struct rwnx_hw *rwnx_hw, u8 vif_idx, u8 stbc_on);
int aml_coex_cmd(struct net_device *dev, u32_l coex_cmd, u32_l cmd_ctxt_1, u32_l cmd_ctxt_2);
int rwnx_tko_activate(struct rwnx_hw *rwnx_hw, struct rwnx_vif *vif, u8 active);
int rwnx_set_pt_calibration(struct rwnx_vif *rwnx_vif, int pt_cali_val);
int rwnx_send_notify_ip(struct rwnx_vif *rwnx_vif,u8_l ip_ver,u8_l*ip_addr);

#endif /* _RWNX_MSG_TX_H_ */
