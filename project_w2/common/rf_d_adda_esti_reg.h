#ifdef RF_D_ADDA_ESTI_REG
#else
#define RF_D_ADDA_ESTI_REG


#define RF_D_ADDA_ESTI_REG_BASE                   (0x0)

#define RG_ESTI_A0                                (RF_D_ADDA_ESTI_REG_BASE + 0x0)
// Bit 31  :0      rg_esti_cfg0                   U     RW        default = 'h0
typedef union RG_ESTI_A0_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_esti_cfg0 : 32;
  } b;
} RG_ESTI_A0_FIELD_T;

#define RG_ESTI_A1                                (RF_D_ADDA_ESTI_REG_BASE + 0x4)
// Bit 31  :0      rg_esti_cfg1                   U     RW        default = 'h0
typedef union RG_ESTI_A1_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_esti_cfg1 : 32;
  } b;
} RG_ESTI_A1_FIELD_T;

#define RG_ESTI_A2                                (RF_D_ADDA_ESTI_REG_BASE + 0x8)
// Bit 15  :0      rg_esti_soft_rst_ctrl          U     RW        default = 'h0
typedef union RG_ESTI_A2_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_esti_soft_rst_ctrl : 16;
    unsigned int rsvd_0 : 16;
  } b;
} RG_ESTI_A2_FIELD_T;

#define RG_ESTI_A3                                (RF_D_ADDA_ESTI_REG_BASE + 0xc)
// Bit 14  :0      ro_rxirr_eup_0                 U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_eup_1                 U     RO        default = 'h0
typedef union RG_ESTI_A3_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_eup_0 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_eup_1 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A3_FIELD_T;

#define RG_ESTI_A4                                (RF_D_ADDA_ESTI_REG_BASE + 0x10)
// Bit 14  :0      ro_rxirr_eup_2                 U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_eup_3                 U     RO        default = 'h0
typedef union RG_ESTI_A4_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_eup_2 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_eup_3 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A4_FIELD_T;

#define RG_ESTI_A5                                (RF_D_ADDA_ESTI_REG_BASE + 0x14)
// Bit 14  :0      ro_rxirr_pup_0                 U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_pup_1                 U     RO        default = 'h0
typedef union RG_ESTI_A5_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_pup_0 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_pup_1 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A5_FIELD_T;

#define RG_ESTI_A6                                (RF_D_ADDA_ESTI_REG_BASE + 0x18)
// Bit 14  :0      ro_rxirr_pup_2                 U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_pup_3                 U     RO        default = 'h0
typedef union RG_ESTI_A6_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_pup_2 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_pup_3 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A6_FIELD_T;

#define RG_ESTI_A7                                (RF_D_ADDA_ESTI_REG_BASE + 0x1c)
// Bit 14  :0      ro_rxirr_elow_0                U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_elow_1                U     RO        default = 'h0
typedef union RG_ESTI_A7_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_elow_0 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_elow_1 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A7_FIELD_T;

#define RG_ESTI_A8                                (RF_D_ADDA_ESTI_REG_BASE + 0x20)
// Bit 14  :0      ro_rxirr_elow_2                U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_elow_3                U     RO        default = 'h0
typedef union RG_ESTI_A8_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_elow_2 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_elow_3 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A8_FIELD_T;

#define RG_ESTI_A9                                (RF_D_ADDA_ESTI_REG_BASE + 0x24)
// Bit 14  :0      ro_rxirr_plow_0                U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_plow_1                U     RO        default = 'h0
typedef union RG_ESTI_A9_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_plow_0 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_plow_1 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A9_FIELD_T;

#define RG_ESTI_A10                               (RF_D_ADDA_ESTI_REG_BASE + 0x28)
// Bit 14  :0      ro_rxirr_plow_2                U     RO        default = 'h0
// Bit 30  :16     ro_rxirr_plow_3                U     RO        default = 'h0
typedef union RG_ESTI_A10_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_plow_2 : 15;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rxirr_plow_3 : 15;
    unsigned int rsvd_1 : 1;
  } b;
} RG_ESTI_A10_FIELD_T;

#define RG_ESTI_A11                               (RF_D_ADDA_ESTI_REG_BASE + 0x2c)
// Bit 0           rg_iqib_enable                 U     RW        default = 'h0
// Bit 1           rg_iqib_hold                   U     RW        default = 'h0
// Bit 7   :4      rg_iqib_period                 U     RW        default = 'h0
// Bit 11  :8      rg_iqib_step_a                 U     RW        default = 'h1
// Bit 15  :12     rg_iqib_step_b                 U     RW        default = 'h1
// Bit 18  :16     rg_iqib_shift                  U     RW        default = 'h0
// Bit 20          rg_iqib_man_en                 U     RW        default = 'h0
// Bit 24          rg_rxirr_read_response         U     RW        default = 'h0
// Bit 28          rg_rxirr_read_response_bypass     U     RW        default = 'h0
// Bit 31          ro_rxirr_ready                 U     RO        default = 'h0
typedef union RG_ESTI_A11_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_iqib_enable : 1;
    unsigned int rg_iqib_hold : 1;
    unsigned int rsvd_0 : 2;
    unsigned int rg_iqib_period : 4;
    unsigned int rg_iqib_step_a : 4;
    unsigned int rg_iqib_step_b : 4;
    unsigned int rg_iqib_shift : 3;
    unsigned int rsvd_1 : 1;
    unsigned int rg_iqib_man_en : 1;
    unsigned int rsvd_2 : 3;
    unsigned int rg_rxirr_read_response : 1;
    unsigned int rsvd_3 : 3;
    unsigned int rg_rxirr_read_response_bypass : 1;
    unsigned int rsvd_4 : 2;
    unsigned int ro_rxirr_ready : 1;
  } b;
} RG_ESTI_A11_FIELD_T;

#define RG_ESTI_A12                               (RF_D_ADDA_ESTI_REG_BASE + 0x30)
// Bit 21  :0      rg_set_b                       U     RW        default = 'h0
// Bit 31          rg_set_val                     U     RW        default = 'h0
typedef union RG_ESTI_A12_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_set_b : 22;
    unsigned int rsvd_0 : 9;
    unsigned int rg_set_val : 1;
  } b;
} RG_ESTI_A12_FIELD_T;

#define RG_ESTI_A13                               (RF_D_ADDA_ESTI_REG_BASE + 0x34)
// Bit 18  :0      rg_set_a                       U     RW        default = 'h0
typedef union RG_ESTI_A13_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_set_a : 19;
    unsigned int rsvd_0 : 13;
  } b;
} RG_ESTI_A13_FIELD_T;

#define RG_ESTI_A14                               (RF_D_ADDA_ESTI_REG_BASE + 0x38)
// Bit 3   :0      rg_dc_rm_leaky_factor          U     RW        default = 'h0
// Bit 28  :4      rg_spectrum_ana_angle_man      U     RW        default = 'h0
// Bit 31          rg_spectrum_ana_angle_man_en     U     RW        default = 'h0
typedef union RG_ESTI_A14_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_dc_rm_leaky_factor : 4;
    unsigned int rg_spectrum_ana_angle_man : 25;
    unsigned int rsvd_0 : 2;
    unsigned int rg_spectrum_ana_angle_man_en : 1;
  } b;
} RG_ESTI_A14_FIELD_T;

#define RG_ESTI_A15                               (RF_D_ADDA_ESTI_REG_BASE + 0x3c)
// Bit 31  :0      rg_esti_cfg2                   U     RW        default = 'h0
typedef union RG_ESTI_A15_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_esti_cfg2 : 32;
  } b;
} RG_ESTI_A15_FIELD_T;

#define RG_ESTI_A16                               (RF_D_ADDA_ESTI_REG_BASE + 0x40)
// Bit 31  :0      rg_esti_cfg3                   U     RW        default = 'h0
typedef union RG_ESTI_A16_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_esti_cfg3 : 32;
  } b;
} RG_ESTI_A16_FIELD_T;

#define RG_ESTI_A17                               (RF_D_ADDA_ESTI_REG_BASE + 0x44)
// Bit 6   :0      ro_dcoc_code_i                 U     RO        default = 'h0
// Bit 14  :8      ro_dcoc_code_q                 U     RO        default = 'h0
// Bit 16          ro_dcoc_code_ready             U     RO        default = 'h0
// Bit 24          ro_dcoc_finish                 U     RO        default = 'h0
// Bit 28          rg_dcoc_read_response_bypass     U     RW        default = 'h0
// Bit 31          rg_dcoc_read_response          U     RW        default = 'h0
typedef union RG_ESTI_A17_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_dcoc_code_i : 7;
    unsigned int rsvd_0 : 1;
    unsigned int ro_dcoc_code_q : 7;
    unsigned int rsvd_1 : 1;
    unsigned int ro_dcoc_code_ready : 1;
    unsigned int rsvd_2 : 7;
    unsigned int ro_dcoc_finish : 1;
    unsigned int rsvd_3 : 3;
    unsigned int rg_dcoc_read_response_bypass : 1;
    unsigned int rsvd_4 : 2;
    unsigned int rg_dcoc_read_response : 1;
  } b;
} RG_ESTI_A17_FIELD_T;

#define RG_ESTI_A18                               (RF_D_ADDA_ESTI_REG_BASE + 0x48)
// Bit 3   :0      ro_lnatank_esti_code           U     RO        default = 'h0
// Bit 8           ro_lnatank_esti_code_ready     U     RO        default = 'h0
// Bit 16          ro_lnatank_esti_finish         U     RO        default = 'h0
// Bit 28          rg_lnatank_read_response_bypass     U     RW        default = 'h0
// Bit 31          rg_lnatank_read_response       U     RW        default = 'h0
typedef union RG_ESTI_A18_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_lnatank_esti_code : 4;
    unsigned int rsvd_0 : 4;
    unsigned int ro_lnatank_esti_code_ready : 1;
    unsigned int rsvd_1 : 7;
    unsigned int ro_lnatank_esti_finish : 1;
    unsigned int rsvd_2 : 11;
    unsigned int rg_lnatank_read_response_bypass : 1;
    unsigned int rsvd_3 : 2;
    unsigned int rg_lnatank_read_response : 1;
  } b;
} RG_ESTI_A18_FIELD_T;

#define RG_ESTI_A19                               (RF_D_ADDA_ESTI_REG_BASE + 0x4c)
// Bit 7   :0      ro_txirr_dc_i                  U     RO        default = 'h0
// Bit 19  :12     ro_txirr_dc_q                  U     RO        default = 'h0
// Bit 24          ro_txirr_dc_ready              U     RO        default = 'h0
// Bit 25          ro_txirr_code_ready            U     RO        default = 'h0
// Bit 26          ro_txirr_mode_ready            U     RO        default = 'h0
// Bit 28          rg_txirr_code_continue         U     RW        default = 'h0
// Bit 29          rg_txirr_code_continue_bypass     U     RW        default = 'h1
// Bit 30          rg_txirr_mode_continue         U     RW        default = 'h0
// Bit 31          rg_txirr_mode_continue_bypass     U     RW        default = 'h1
typedef union RG_ESTI_A19_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txirr_dc_i : 8;
    unsigned int rsvd_0 : 4;
    unsigned int ro_txirr_dc_q : 8;
    unsigned int rsvd_1 : 4;
    unsigned int ro_txirr_dc_ready : 1;
    unsigned int ro_txirr_code_ready : 1;
    unsigned int ro_txirr_mode_ready : 1;
    unsigned int rsvd_2 : 1;
    unsigned int rg_txirr_code_continue : 1;
    unsigned int rg_txirr_code_continue_bypass : 1;
    unsigned int rg_txirr_mode_continue : 1;
    unsigned int rg_txirr_mode_continue_bypass : 1;
  } b;
} RG_ESTI_A19_FIELD_T;

#define RG_ESTI_A20                               (RF_D_ADDA_ESTI_REG_BASE + 0x50)
// Bit 7   :0      ro_txirr_alpha                 U     RO        default = 'h0
// Bit 19  :12     ro_txirr_theta                 U     RO        default = 'h0
// Bit 24          ro_txirr_irr_ready             U     RO        default = 'h0
// Bit 28          rg_txirr_read_response_bypass     U     RW        default = 'h0
// Bit 31          rg_txirr_read_response         U     RW        default = 'h0
typedef union RG_ESTI_A20_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txirr_alpha : 8;
    unsigned int rsvd_0 : 4;
    unsigned int ro_txirr_theta : 8;
    unsigned int rsvd_1 : 4;
    unsigned int ro_txirr_irr_ready : 1;
    unsigned int rsvd_2 : 3;
    unsigned int rg_txirr_read_response_bypass : 1;
    unsigned int rsvd_3 : 2;
    unsigned int rg_txirr_read_response : 1;
  } b;
} RG_ESTI_A20_FIELD_T;

#define RG_ESTI_A21                               (RF_D_ADDA_ESTI_REG_BASE + 0x54)
// Bit 7   :0      rg_tx_dcmax_i                  U     RW        default = 'h33
// Bit 15  :8      rg_tx_dcmax_q                  U     RW        default = 'h33
// Bit 23  :16     rg_tx_alpha_max                U     RW        default = 'h7b
// Bit 31  :24     rg_tx_theta_max                U     RW        default = 'h6b
typedef union RG_ESTI_A21_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_dcmax_i : 8;
    unsigned int rg_tx_dcmax_q : 8;
    unsigned int rg_tx_alpha_max : 8;
    unsigned int rg_tx_theta_max : 8;
  } b;
} RG_ESTI_A21_FIELD_T;

#define RG_ESTI_A22                               (RF_D_ADDA_ESTI_REG_BASE + 0x58)
// Bit 7   :0      rg_tx_dcmin_i                  U     RW        default = 'hcd
// Bit 15  :8      rg_tx_dcmin_q                  U     RW        default = 'hcd
// Bit 23  :16     rg_tx_alpha_min                U     RW        default = 'h85
// Bit 31  :24     rg_tx_theta_min                U     RW        default = 'h95
typedef union RG_ESTI_A22_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_dcmin_i : 8;
    unsigned int rg_tx_dcmin_q : 8;
    unsigned int rg_tx_alpha_min : 8;
    unsigned int rg_tx_theta_min : 8;
  } b;
} RG_ESTI_A22_FIELD_T;

#define RG_ESTI_A23                               (RF_D_ADDA_ESTI_REG_BASE + 0x5c)
// Bit 3   :0      rg_tx_dc_i_step                U     RW        default = 'h5
// Bit 7   :4      rg_tx_dc_q_step                U     RW        default = 'h5
// Bit 11  :8      rg_tx_alpha_step               U     RW        default = 'h3
// Bit 15  :12     rg_tx_theta_step               U     RW        default = 'h4
// Bit 19  :16     rg_txirr_oper_bnd              U     RW        default = 'h1
// Bit 20          rg_txirr_dyna_step_en          U     RW        default = 'h0
typedef union RG_ESTI_A23_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_dc_i_step : 4;
    unsigned int rg_tx_dc_q_step : 4;
    unsigned int rg_tx_alpha_step : 4;
    unsigned int rg_tx_theta_step : 4;
    unsigned int rg_txirr_oper_bnd : 4;
    unsigned int rg_txirr_dyna_step_en : 1;
    unsigned int rsvd_0 : 11;
  } b;
} RG_ESTI_A23_FIELD_T;

#define RG_ESTI_A24                               (RF_D_ADDA_ESTI_REG_BASE + 0x60)
// Bit 24  :0      rg_spectrum_ana_angle_dcoc     U     RW        default = 'h0
typedef union RG_ESTI_A24_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_dcoc : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A24_FIELD_T;

#define RG_ESTI_A25                               (RF_D_ADDA_ESTI_REG_BASE + 0x64)
// Bit 24  :0      rg_spectrum_ana_angle_lna      U     RW        default = 'h1f99999
typedef union RG_ESTI_A25_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_lna : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A25_FIELD_T;

#define RG_ESTI_A26                               (RF_D_ADDA_ESTI_REG_BASE + 0x68)
// Bit 24  :0      rg_spectrum_ana_angle_txirr     U     RW        default = 'h1c00000
typedef union RG_ESTI_A26_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_txirr : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A26_FIELD_T;

#define RG_ESTI_A27                               (RF_D_ADDA_ESTI_REG_BASE + 0x6c)
// Bit 24  :0      rg_spectrum_ana_angle_txdc     U     RW        default = 'h1e00000
typedef union RG_ESTI_A27_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_txdc : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A27_FIELD_T;

#define RG_ESTI_A28                               (RF_D_ADDA_ESTI_REG_BASE + 0x70)
// Bit 24  :0      rg_spectrum_ana_angle_dpd      U     RW        default = 'h0
typedef union RG_ESTI_A28_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_dpd : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A28_FIELD_T;

#define RG_ESTI_A29                               (RF_D_ADDA_ESTI_REG_BASE + 0x74)
// Bit 24  :0      rg_spectrum_ana_angle_pwc      U     RW        default = 'h0
typedef union RG_ESTI_A29_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_spectrum_ana_angle_pwc : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A29_FIELD_T;

#define RG_ESTI_A30                               (RF_D_ADDA_ESTI_REG_BASE + 0x78)
// Bit 24  :0      rg_irr_tone_freq_0             U     RW        default = 'h4ccccc
typedef union RG_ESTI_A30_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A30_FIELD_T;

#define RG_ESTI_A31                               (RF_D_ADDA_ESTI_REG_BASE + 0x7c)
// Bit 24  :0      rg_irr_tone_freq_1             U     RW        default = 'ha66666
typedef union RG_ESTI_A31_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A31_FIELD_T;

#define RG_ESTI_A32                               (RF_D_ADDA_ESTI_REG_BASE + 0x80)
// Bit 24  :0      rg_irr_tone_freq_2             U     RW        default = 'hd33333
typedef union RG_ESTI_A32_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_2 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A32_FIELD_T;

#define RG_ESTI_A33                               (RF_D_ADDA_ESTI_REG_BASE + 0x84)
// Bit 24  :0      rg_irr_tone_freq_3             U     RW        default = 'hf00000
typedef union RG_ESTI_A33_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_3 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A33_FIELD_T;

#define RG_ESTI_A34                               (RF_D_ADDA_ESTI_REG_BASE + 0x88)
// Bit 24  :0      rg_irr_tone_freq_4             U     RW        default = 'h1100000
typedef union RG_ESTI_A34_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_4 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A34_FIELD_T;

#define RG_ESTI_A35                               (RF_D_ADDA_ESTI_REG_BASE + 0x8c)
// Bit 24  :0      rg_irr_tone_freq_5             U     RW        default = 'h12ccccc
typedef union RG_ESTI_A35_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_5 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A35_FIELD_T;

#define RG_ESTI_A36                               (RF_D_ADDA_ESTI_REG_BASE + 0x90)
// Bit 24  :0      rg_irr_tone_freq_6             U     RW        default = 'h1599999
typedef union RG_ESTI_A36_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_6 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A36_FIELD_T;

#define RG_ESTI_A37                               (RF_D_ADDA_ESTI_REG_BASE + 0x94)
// Bit 24  :0      rg_irr_tone_freq_7             U     RW        default = 'h1b33333
typedef union RG_ESTI_A37_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_7 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A37_FIELD_T;

#define RG_ESTI_A38                               (RF_D_ADDA_ESTI_REG_BASE + 0x98)
// Bit 15  :0      rg_irr_interp_coef_up0         U     RW        default = 'hfd76
// Bit 31  :16     rg_irr_interp_coef_up1         U     RW        default = 'h2ae
typedef union RG_ESTI_A38_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_up0 : 16;
    unsigned int rg_irr_interp_coef_up1 : 16;
  } b;
} RG_ESTI_A38_FIELD_T;

#define RG_ESTI_A39                               (RF_D_ADDA_ESTI_REG_BASE + 0x9c)
// Bit 15  :0      rg_irr_interp_coef_up2         U     RW        default = 'hfec5
// Bit 31  :16     rg_irr_interp_coef_up3         U     RW        default = 'hb1
typedef union RG_ESTI_A39_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_up2 : 16;
    unsigned int rg_irr_interp_coef_up3 : 16;
  } b;
} RG_ESTI_A39_FIELD_T;

#define RG_ESTI_A40                               (RF_D_ADDA_ESTI_REG_BASE + 0xa0)
// Bit 15  :0      rg_irr_interp_coef_up4         U     RW        default = 'h10a7
// Bit 31  :16     rg_irr_interp_coef_up5         U     RW        default = 'hf1ec
typedef union RG_ESTI_A40_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_up4 : 16;
    unsigned int rg_irr_interp_coef_up5 : 16;
  } b;
} RG_ESTI_A40_FIELD_T;

#define RG_ESTI_A41                               (RF_D_ADDA_ESTI_REG_BASE + 0xa4)
// Bit 15  :0      rg_irr_interp_coef_up6         U     RW        default = 'h806
// Bit 31  :16     rg_irr_interp_coef_up7         U     RW        default = 'hfcd1
typedef union RG_ESTI_A41_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_up6 : 16;
    unsigned int rg_irr_interp_coef_up7 : 16;
  } b;
} RG_ESTI_A41_FIELD_T;

#define RG_ESTI_A42                               (RF_D_ADDA_ESTI_REG_BASE + 0xa8)
// Bit 15  :0      rg_irr_interp_coef_up8         U     RW        default = 'he24b
// Bit 31  :16     rg_irr_interp_coef_up9         U     RW        default = 'h1919
typedef union RG_ESTI_A42_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_up8 : 16;
    unsigned int rg_irr_interp_coef_up9 : 16;
  } b;
} RG_ESTI_A42_FIELD_T;

#define RG_ESTI_A43                               (RF_D_ADDA_ESTI_REG_BASE + 0xac)
// Bit 15  :0      rg_irr_interp_coef_upa         U     RW        default = 'hf1fe
// Bit 31  :16     rg_irr_interp_coef_upb         U     RW        default = 'h4a2
typedef union RG_ESTI_A43_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_upa : 16;
    unsigned int rg_irr_interp_coef_upb : 16;
  } b;
} RG_ESTI_A43_FIELD_T;

#define RG_ESTI_A44                               (RF_D_ADDA_ESTI_REG_BASE + 0xb0)
// Bit 15  :0      rg_irr_interp_coef_upc         U     RW        default = 'h1099
// Bit 31  :16     rg_irr_interp_coef_upd         U     RW        default = 'hf24d
typedef union RG_ESTI_A44_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_upc : 16;
    unsigned int rg_irr_interp_coef_upd : 16;
  } b;
} RG_ESTI_A44_FIELD_T;

#define RG_ESTI_A45                               (RF_D_ADDA_ESTI_REG_BASE + 0xb4)
// Bit 15  :0      rg_irr_interp_coef_upe         U     RW        default = 'h737
// Bit 31  :16     rg_irr_interp_coef_upf         U     RW        default = 'hfddc
typedef union RG_ESTI_A45_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_upe : 16;
    unsigned int rg_irr_interp_coef_upf : 16;
  } b;
} RG_ESTI_A45_FIELD_T;

#define RG_ESTI_A46                               (RF_D_ADDA_ESTI_REG_BASE + 0xb8)
// Bit 15  :0      rg_irr_interp_coef_low0        U     RW        default = 'h1099
// Bit 31  :16     rg_irr_interp_coef_low1        U     RW        default = 'hf24d
typedef union RG_ESTI_A46_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_low0 : 16;
    unsigned int rg_irr_interp_coef_low1 : 16;
  } b;
} RG_ESTI_A46_FIELD_T;

#define RG_ESTI_A47                               (RF_D_ADDA_ESTI_REG_BASE + 0xbc)
// Bit 15  :0      rg_irr_interp_coef_low2        U     RW        default = 'h737
// Bit 31  :16     rg_irr_interp_coef_low3        U     RW        default = 'hfddc
typedef union RG_ESTI_A47_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_low2 : 16;
    unsigned int rg_irr_interp_coef_low3 : 16;
  } b;
} RG_ESTI_A47_FIELD_T;

#define RG_ESTI_A48                               (RF_D_ADDA_ESTI_REG_BASE + 0xc0)
// Bit 15  :0      rg_irr_interp_coef_low4        U     RW        default = 'he24b
// Bit 31  :16     rg_irr_interp_coef_low5        U     RW        default = 'h1919
typedef union RG_ESTI_A48_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_low4 : 16;
    unsigned int rg_irr_interp_coef_low5 : 16;
  } b;
} RG_ESTI_A48_FIELD_T;

#define RG_ESTI_A49                               (RF_D_ADDA_ESTI_REG_BASE + 0xc4)
// Bit 15  :0      rg_irr_interp_coef_low6        U     RW        default = 'hf1fe
// Bit 31  :16     rg_irr_interp_coef_low7        U     RW        default = 'h4a2
typedef union RG_ESTI_A49_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_low6 : 16;
    unsigned int rg_irr_interp_coef_low7 : 16;
  } b;
} RG_ESTI_A49_FIELD_T;

#define RG_ESTI_A50                               (RF_D_ADDA_ESTI_REG_BASE + 0xc8)
// Bit 15  :0      rg_irr_interp_coef_low8        U     RW        default = 'h10a7
// Bit 31  :16     rg_irr_interp_coef_low9        U     RW        default = 'hf1ec
typedef union RG_ESTI_A50_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_low8 : 16;
    unsigned int rg_irr_interp_coef_low9 : 16;
  } b;
} RG_ESTI_A50_FIELD_T;

#define RG_ESTI_A51                               (RF_D_ADDA_ESTI_REG_BASE + 0xcc)
// Bit 15  :0      rg_irr_interp_coef_lowa        U     RW        default = 'h806
// Bit 31  :16     rg_irr_interp_coef_lowb        U     RW        default = 'hfcd1
typedef union RG_ESTI_A51_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_lowa : 16;
    unsigned int rg_irr_interp_coef_lowb : 16;
  } b;
} RG_ESTI_A51_FIELD_T;

#define RG_ESTI_A52                               (RF_D_ADDA_ESTI_REG_BASE + 0xd0)
// Bit 15  :0      rg_irr_interp_coef_lowc        U     RW        default = 'hfd76
// Bit 31  :16     rg_irr_interp_coef_lowd        U     RW        default = 'h2ae
typedef union RG_ESTI_A52_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_lowc : 16;
    unsigned int rg_irr_interp_coef_lowd : 16;
  } b;
} RG_ESTI_A52_FIELD_T;

#define RG_ESTI_A53                               (RF_D_ADDA_ESTI_REG_BASE + 0xd4)
// Bit 15  :0      rg_irr_interp_coef_lowe        U     RW        default = 'hfec5
// Bit 31  :16     rg_irr_interp_coef_lowf        U     RW        default = 'hb1
typedef union RG_ESTI_A53_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_interp_coef_lowe : 16;
    unsigned int rg_irr_interp_coef_lowf : 16;
  } b;
} RG_ESTI_A53_FIELD_T;

#define RG_ESTI_A54                               (RF_D_ADDA_ESTI_REG_BASE + 0xd8)
// Bit 3   :0      rg_rxirr_esti_len_avg_0        U     RW        default = 'h2
// Bit 7   :4      rg_rxirr_esti_miu_0            U     RW        default = 'h0
// Bit 15  :8      rg_rxirr_esti_navg_0           U     RW        default = 'h3
// Bit 19  :16     rg_rxirr_esti_len_avg_1        U     RW        default = 'h3
// Bit 23  :20     rg_rxirr_esti_miu_1            U     RW        default = 'h1
// Bit 31  :24     rg_rxirr_esti_navg_1           U     RW        default = 'h7
typedef union RG_ESTI_A54_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_esti_len_avg_0 : 4;
    unsigned int rg_rxirr_esti_miu_0 : 4;
    unsigned int rg_rxirr_esti_navg_0 : 8;
    unsigned int rg_rxirr_esti_len_avg_1 : 4;
    unsigned int rg_rxirr_esti_miu_1 : 4;
    unsigned int rg_rxirr_esti_navg_1 : 8;
  } b;
} RG_ESTI_A54_FIELD_T;

#define RG_ESTI_A55                               (RF_D_ADDA_ESTI_REG_BASE + 0xdc)
// Bit 3   :0      rg_rxirr_esti_len_avg_2        U     RW        default = 'h4
// Bit 7   :4      rg_rxirr_esti_miu_2            U     RW        default = 'h2
// Bit 15  :8      rg_rxirr_esti_navg_2           U     RW        default = 'hb
// Bit 19  :16     rg_rxirr_esti_len_avg_3        U     RW        default = 'h4
// Bit 23  :20     rg_rxirr_esti_miu_3            U     RW        default = 'h3
// Bit 31  :24     rg_rxirr_esti_navg_3           U     RW        default = 'h13
typedef union RG_ESTI_A55_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_esti_len_avg_2 : 4;
    unsigned int rg_rxirr_esti_miu_2 : 4;
    unsigned int rg_rxirr_esti_navg_2 : 8;
    unsigned int rg_rxirr_esti_len_avg_3 : 4;
    unsigned int rg_rxirr_esti_miu_3 : 4;
    unsigned int rg_rxirr_esti_navg_3 : 8;
  } b;
} RG_ESTI_A55_FIELD_T;

#define RG_ESTI_A56                               (RF_D_ADDA_ESTI_REG_BASE + 0xe0)
// Bit 13  :0      ro_rxirr_interp_e_0            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_e_1            U     RO        default = 'h0
typedef union RG_ESTI_A56_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_e_0 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_e_1 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A56_FIELD_T;

#define RG_ESTI_A57                               (RF_D_ADDA_ESTI_REG_BASE + 0xe4)
// Bit 13  :0      ro_rxirr_interp_e_2            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_e_3            U     RO        default = 'h0
typedef union RG_ESTI_A57_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_e_2 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_e_3 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A57_FIELD_T;

#define RG_ESTI_A58                               (RF_D_ADDA_ESTI_REG_BASE + 0xe8)
// Bit 13  :0      ro_rxirr_interp_e_4            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_e_5            U     RO        default = 'h0
typedef union RG_ESTI_A58_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_e_4 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_e_5 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A58_FIELD_T;

#define RG_ESTI_A59                               (RF_D_ADDA_ESTI_REG_BASE + 0xec)
// Bit 13  :0      ro_rxirr_interp_e_6            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_e_7            U     RO        default = 'h0
typedef union RG_ESTI_A59_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_e_6 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_e_7 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A59_FIELD_T;

#define RG_ESTI_A60                               (RF_D_ADDA_ESTI_REG_BASE + 0xf0)
// Bit 13  :0      ro_rxirr_interp_p_0            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_p_1            U     RO        default = 'h0
typedef union RG_ESTI_A60_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_p_0 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_p_1 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A60_FIELD_T;

#define RG_ESTI_A61                               (RF_D_ADDA_ESTI_REG_BASE + 0xf4)
// Bit 13  :0      ro_rxirr_interp_p_2            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_p_3            U     RO        default = 'h0
typedef union RG_ESTI_A61_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_p_2 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_p_3 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A61_FIELD_T;

#define RG_ESTI_A62                               (RF_D_ADDA_ESTI_REG_BASE + 0xf8)
// Bit 13  :0      ro_rxirr_interp_p_4            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_p_5            U     RO        default = 'h0
typedef union RG_ESTI_A62_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_p_4 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_p_5 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A62_FIELD_T;

#define RG_ESTI_A63                               (RF_D_ADDA_ESTI_REG_BASE + 0xfc)
// Bit 13  :0      ro_rxirr_interp_p_6            U     RO        default = 'h0
// Bit 29  :16     ro_rxirr_interp_p_7            U     RO        default = 'h0
typedef union RG_ESTI_A63_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rxirr_interp_p_6 : 14;
    unsigned int rsvd_0 : 2;
    unsigned int ro_rxirr_interp_p_7 : 14;
    unsigned int rsvd_1 : 2;
  } b;
} RG_ESTI_A63_FIELD_T;

#define RG_ESTI_A64                               (RF_D_ADDA_ESTI_REG_BASE + 0x100)
// Bit 23  :0      rg_adda_wait_count             U     RW        default = 'h50
typedef union RG_ESTI_A64_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_adda_wait_count : 24;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A64_FIELD_T;

#define RG_ESTI_A65                               (RF_D_ADDA_ESTI_REG_BASE + 0x104)
// Bit 14  :0      rg_adda_calc_count             U     RW        default = 'h50
typedef union RG_ESTI_A65_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_adda_calc_count : 15;
    unsigned int rsvd_0 : 17;
  } b;
} RG_ESTI_A65_FIELD_T;

#define RG_ESTI_A66                               (RF_D_ADDA_ESTI_REG_BASE + 0x108)
// Bit 27  :0      rg_adda_esti_count             U     RW        default = 'h40000
typedef union RG_ESTI_A66_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_adda_esti_count : 28;
    unsigned int rsvd_0 : 4;
  } b;
} RG_ESTI_A66_FIELD_T;

#define RG_ESTI_A67                               (RF_D_ADDA_ESTI_REG_BASE + 0x10c)
// Bit 0           rg_txpwc_read_response         U     RW        default = 'h0
// Bit 4           rg_txpwc_read_response_bypass     U     RW        default = 'h1
// Bit 16  :8      rg_txpwc_ph                    U     RW        default = 'h0
// Bit 28  :20     rg_txpwc_pl                    U     RW        default = 'h0
typedef union RG_ESTI_A67_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txpwc_read_response : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_txpwc_read_response_bypass : 1;
    unsigned int rsvd_1 : 3;
    unsigned int rg_txpwc_ph : 9;
    unsigned int rsvd_2 : 3;
    unsigned int rg_txpwc_pl : 9;
    unsigned int rsvd_3 : 3;
  } b;
} RG_ESTI_A67_FIELD_T;

#define RG_ESTI_A68                               (RF_D_ADDA_ESTI_REG_BASE + 0x110)
// Bit 7   :0      rg_txpwc_vh_amp                U     RW        default = 'h0
// Bit 15  :8      rg_txpwc_vl_amp                U     RW        default = 'h0
// Bit 25  :24     rg_txpwc_dc_cnt                U     RW        default = 'h0
typedef union RG_ESTI_A68_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txpwc_vh_amp : 8;
    unsigned int rg_txpwc_vl_amp : 8;
    unsigned int rsvd_0 : 8;
    unsigned int rg_txpwc_dc_cnt : 2;
    unsigned int rsvd_1 : 6;
  } b;
} RG_ESTI_A68_FIELD_T;

#define RG_ESTI_A69                               (RF_D_ADDA_ESTI_REG_BASE + 0x114)
// Bit 8   :0      ro_txpwc_slope                 U     RO        default = 'h0
// Bit 26  :12     ro_txpwc_offset                U     RO        default = 'h0
// Bit 31          ro_txpwc_ready                 U     RO        default = 'h0
typedef union RG_ESTI_A69_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txpwc_slope : 9;
    unsigned int rsvd_0 : 3;
    unsigned int ro_txpwc_offset : 15;
    unsigned int rsvd_1 : 4;
    unsigned int ro_txpwc_ready : 1;
  } b;
} RG_ESTI_A69_FIELD_T;

#define RG_ESTI_A70                               (RF_D_ADDA_ESTI_REG_BASE + 0x118)
// Bit 0           rg_txdpd_esti_read_response     U     RW        default = 'h0
// Bit 2           rg_txdpd_esti_read_response_bypass     U     RW        default = 'h0
// Bit 11  :4      rg_txdpd_esti_alpha0           U     RW        default = 'h33
// Bit 18  :12     rg_txdpd_esti_sync_gap         U     RW        default = 'h20
// Bit 22          ro_txdpd_esti_ready            U     RO        default = 'h0
// Bit 31  :24     rg_txdpd_esti_mp_pow_thresh     U     RW        default = 'h6
typedef union RG_ESTI_A70_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_read_response : 1;
    unsigned int rsvd_0 : 1;
    unsigned int rg_txdpd_esti_read_response_bypass : 1;
    unsigned int rsvd_1 : 1;
    unsigned int rg_txdpd_esti_alpha0 : 8;
    unsigned int rg_txdpd_esti_sync_gap : 7;
    unsigned int rsvd_2 : 3;
    unsigned int ro_txdpd_esti_ready : 1;
    unsigned int rsvd_3 : 1;
    unsigned int rg_txdpd_esti_mp_pow_thresh : 8;
  } b;
} RG_ESTI_A70_FIELD_T;

#define RG_ESTI_A72                               (RF_D_ADDA_ESTI_REG_BASE + 0x11c)
// Bit 11  :0      ro_txdpd_esti_coef_i_00        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_00        U     RO        default = 'h0
typedef union RG_ESTI_A72_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_00 : 12;
    unsigned int ro_txdpd_esti_coef_q_00 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A72_FIELD_T;

#define RG_ESTI_A73                               (RF_D_ADDA_ESTI_REG_BASE + 0x120)
// Bit 11  :0      ro_txdpd_esti_coef_i_01        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_01        U     RO        default = 'h0
typedef union RG_ESTI_A73_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_01 : 12;
    unsigned int ro_txdpd_esti_coef_q_01 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A73_FIELD_T;

#define RG_ESTI_A74                               (RF_D_ADDA_ESTI_REG_BASE + 0x124)
// Bit 11  :0      ro_txdpd_esti_coef_i_02        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_02        U     RO        default = 'h0
typedef union RG_ESTI_A74_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_02 : 12;
    unsigned int ro_txdpd_esti_coef_q_02 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A74_FIELD_T;

#define RG_ESTI_A75                               (RF_D_ADDA_ESTI_REG_BASE + 0x128)
// Bit 11  :0      ro_txdpd_esti_coef_i_03        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_03        U     RO        default = 'h0
typedef union RG_ESTI_A75_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_03 : 12;
    unsigned int ro_txdpd_esti_coef_q_03 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A75_FIELD_T;

#define RG_ESTI_A76                               (RF_D_ADDA_ESTI_REG_BASE + 0x12c)
// Bit 11  :0      ro_txdpd_esti_coef_i_04        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_04        U     RO        default = 'h0
typedef union RG_ESTI_A76_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_04 : 12;
    unsigned int ro_txdpd_esti_coef_q_04 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A76_FIELD_T;

#define RG_ESTI_A77                               (RF_D_ADDA_ESTI_REG_BASE + 0x130)
// Bit 11  :0      ro_txdpd_esti_coef_i_05        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_05        U     RO        default = 'h0
typedef union RG_ESTI_A77_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_05 : 12;
    unsigned int ro_txdpd_esti_coef_q_05 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A77_FIELD_T;

#define RG_ESTI_A78                               (RF_D_ADDA_ESTI_REG_BASE + 0x134)
// Bit 11  :0      ro_txdpd_esti_coef_i_06        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_06        U     RO        default = 'h0
typedef union RG_ESTI_A78_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_06 : 12;
    unsigned int ro_txdpd_esti_coef_q_06 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A78_FIELD_T;

#define RG_ESTI_A79                               (RF_D_ADDA_ESTI_REG_BASE + 0x138)
// Bit 11  :0      ro_txdpd_esti_coef_i_07        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_07        U     RO        default = 'h0
typedef union RG_ESTI_A79_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_07 : 12;
    unsigned int ro_txdpd_esti_coef_q_07 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A79_FIELD_T;

#define RG_ESTI_A80                               (RF_D_ADDA_ESTI_REG_BASE + 0x13c)
// Bit 11  :0      ro_txdpd_esti_coef_i_08        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_08        U     RO        default = 'h0
typedef union RG_ESTI_A80_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_08 : 12;
    unsigned int ro_txdpd_esti_coef_q_08 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A80_FIELD_T;

#define RG_ESTI_A81                               (RF_D_ADDA_ESTI_REG_BASE + 0x140)
// Bit 11  :0      ro_txdpd_esti_coef_i_09        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_09        U     RO        default = 'h0
typedef union RG_ESTI_A81_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_09 : 12;
    unsigned int ro_txdpd_esti_coef_q_09 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A81_FIELD_T;

#define RG_ESTI_A82                               (RF_D_ADDA_ESTI_REG_BASE + 0x144)
// Bit 11  :0      ro_txdpd_esti_coef_i_10        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_10        U     RO        default = 'h0
typedef union RG_ESTI_A82_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_10 : 12;
    unsigned int ro_txdpd_esti_coef_q_10 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A82_FIELD_T;

#define RG_ESTI_A83                               (RF_D_ADDA_ESTI_REG_BASE + 0x148)
// Bit 11  :0      ro_txdpd_esti_coef_i_11        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_11        U     RO        default = 'h0
typedef union RG_ESTI_A83_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_11 : 12;
    unsigned int ro_txdpd_esti_coef_q_11 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A83_FIELD_T;

#define RG_ESTI_A84                               (RF_D_ADDA_ESTI_REG_BASE + 0x14c)
// Bit 11  :0      ro_txdpd_esti_coef_i_12        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_12        U     RO        default = 'h0
typedef union RG_ESTI_A84_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_12 : 12;
    unsigned int ro_txdpd_esti_coef_q_12 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A84_FIELD_T;

#define RG_ESTI_A85                               (RF_D_ADDA_ESTI_REG_BASE + 0x150)
// Bit 11  :0      ro_txdpd_esti_coef_i_13        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_13        U     RO        default = 'h0
typedef union RG_ESTI_A85_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_13 : 12;
    unsigned int ro_txdpd_esti_coef_q_13 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A85_FIELD_T;

#define RG_ESTI_A86                               (RF_D_ADDA_ESTI_REG_BASE + 0x154)
// Bit 11  :0      ro_txdpd_esti_coef_i_14        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_14        U     RO        default = 'h0
typedef union RG_ESTI_A86_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_14 : 12;
    unsigned int ro_txdpd_esti_coef_q_14 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A86_FIELD_T;

#define RG_ESTI_A87                               (RF_D_ADDA_ESTI_REG_BASE + 0x158)
// Bit 0           rg_snr_esti_en                 U     RW        default = 'h1
// Bit 6   :4      rg_snr_esti_calctime           U     RW        default = 'h0
// Bit 10  :8      rg_snr_alpha                   U     RW        default = 'h0
// Bit 18  :16     rg_txdpd_snr_esti_alpha        U     RW        default = 'h0
typedef union RG_ESTI_A87_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_snr_esti_en : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_snr_esti_calctime : 3;
    unsigned int rsvd_1 : 1;
    unsigned int rg_snr_alpha : 3;
    unsigned int rsvd_2 : 5;
    unsigned int rg_txdpd_snr_esti_alpha : 3;
    unsigned int rsvd_3 : 13;
  } b;
} RG_ESTI_A87_FIELD_T;

#define RG_ESTI_A88                               (RF_D_ADDA_ESTI_REG_BASE + 0x15c)
// Bit 9   :0      ro_adda_snr_db                 U     RO        default = 'h0
// Bit 12          ro_adda_db_vld                 U     RO        default = 'h0
// Bit 25  :16     ro_adda_pwr_db                 U     RO        default = 'h0
typedef union RG_ESTI_A88_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_adda_snr_db : 10;
    unsigned int rsvd_0 : 2;
    unsigned int ro_adda_db_vld : 1;
    unsigned int rsvd_1 : 3;
    unsigned int ro_adda_pwr_db : 10;
    unsigned int rsvd_2 : 6;
  } b;
} RG_ESTI_A88_FIELD_T;

#define RG_ESTI_A89                               (RF_D_ADDA_ESTI_REG_BASE + 0x160)
// Bit 8   :0      ro_txdpd_esti_snr_db           U     RO        default = 'h0
typedef union RG_ESTI_A89_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_snr_db : 9;
    unsigned int rsvd_0 : 23;
  } b;
} RG_ESTI_A89_FIELD_T;

#define RG_ESTI_A90                               (RF_D_ADDA_ESTI_REG_BASE + 0x164)
// Bit 29  :0      ro_dcoc_dc_i                   U     RO        default = 'h0
typedef union RG_ESTI_A90_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_dcoc_dc_i : 30;
    unsigned int rsvd_0 : 2;
  } b;
} RG_ESTI_A90_FIELD_T;

#define RG_ESTI_A91                               (RF_D_ADDA_ESTI_REG_BASE + 0x168)
// Bit 29  :0      ro_dcoc_dc_q                   U     RO        default = 'h0
typedef union RG_ESTI_A91_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_dcoc_dc_q : 30;
    unsigned int rsvd_0 : 2;
  } b;
} RG_ESTI_A91_FIELD_T;

#define RG_ESTI_A92                               (RF_D_ADDA_ESTI_REG_BASE + 0x16c)
// Bit 31  :0      ro_txirr_dc_pow_low            U     RO        default = 'h0
typedef union RG_ESTI_A92_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txirr_dc_pow_low : 32;
  } b;
} RG_ESTI_A92_FIELD_T;

#define RG_ESTI_A93                               (RF_D_ADDA_ESTI_REG_BASE + 0x170)
// Bit 22  :0      ro_txirr_dc_pow_high           U     RO        default = 'h0
typedef union RG_ESTI_A93_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txirr_dc_pow_high : 23;
    unsigned int rsvd_0 : 9;
  } b;
} RG_ESTI_A93_FIELD_T;

#define RG_ESTI_A94                               (RF_D_ADDA_ESTI_REG_BASE + 0x174)
// Bit 31  :0      ro_lnatank_esti_pow            U     RO        default = 'h0
typedef union RG_ESTI_A94_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_lnatank_esti_pow : 32;
  } b;
} RG_ESTI_A94_FIELD_T;

#define RG_ESTI_A96                               (RF_D_ADDA_ESTI_REG_BASE + 0x17c)
// Bit 11  :0      ro_txdpd_gain                  U     RO        default = 'h0
// Bit 27  :16     rg_txdpd_esti_gain_man         U     RW        default = 'h0
// Bit 31          rg_txdpd_esti_gain_man_mode     U     RW        default = 'h0
typedef union RG_ESTI_A96_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_gain : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_gain_man : 12;
    unsigned int rsvd_1 : 3;
    unsigned int rg_txdpd_esti_gain_man_mode : 1;
  } b;
} RG_ESTI_A96_FIELD_T;

#define RG_ESTI_A140                              (RF_D_ADDA_ESTI_REG_BASE + 0x22c)
// Bit 27  :0      rg_txdpd_esti_step0            U     RW        default = 'h0
typedef union RG_ESTI_A140_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_step0 : 28;
    unsigned int rsvd_0 : 4;
  } b;
} RG_ESTI_A140_FIELD_T;

#define RG_ESTI_A141                              (RF_D_ADDA_ESTI_REG_BASE + 0x230)
// Bit 27  :0      rg_txdpd_esti_step1            U     RW        default = 'h0
typedef union RG_ESTI_A141_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_step1 : 28;
    unsigned int rsvd_0 : 4;
  } b;
} RG_ESTI_A141_FIELD_T;

#define RG_ESTI_A142                              (RF_D_ADDA_ESTI_REG_BASE + 0x234)
// Bit 27  :0      rg_txdpd_esti_step2            U     RW        default = 'h0
typedef union RG_ESTI_A142_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_step2 : 28;
    unsigned int rsvd_0 : 4;
  } b;
} RG_ESTI_A142_FIELD_T;

#define RG_ESTI_A143                              (RF_D_ADDA_ESTI_REG_BASE + 0x238)
// Bit 7   :0      rg_txdpd_esti_alpha1           U     RW        default = 'h33
// Bit 15  :8      rg_txdpd_esti_alpha2           U     RW        default = 'h33
// Bit 23  :16     rg_txdpd_esti_alpha3           U     RW        default = 'h33
// Bit 24          rg_txdpd_esti_recip_force_one     U     RW        default = 'h0
typedef union RG_ESTI_A143_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_alpha1 : 8;
    unsigned int rg_txdpd_esti_alpha2 : 8;
    unsigned int rg_txdpd_esti_alpha3 : 8;
    unsigned int rg_txdpd_esti_recip_force_one : 1;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A143_FIELD_T;

#define RG_ESTI_A144                              (RF_D_ADDA_ESTI_REG_BASE + 0x23c)
// Bit 11  :0      rg_txdpd_esti_coef_ini_i_m0p1     U     RW        default = 'h400
// Bit 27  :16     rg_txdpd_esti_coef_ini_q_m0p1     U     RW        default = 'h0
typedef union RG_ESTI_A144_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_coef_ini_i_m0p1 : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_coef_ini_q_m0p1 : 12;
    unsigned int rsvd_1 : 4;
  } b;
} RG_ESTI_A144_FIELD_T;

#define RG_ESTI_A145                              (RF_D_ADDA_ESTI_REG_BASE + 0x240)
// Bit 11  :0      rg_txdpd_esti_coef_ini_i_m1p1     U     RW        default = 'h0
// Bit 27  :16     rg_txdpd_esti_coef_ini_q_m1p1     U     RW        default = 'h0
typedef union RG_ESTI_A145_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_coef_ini_i_m1p1 : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_coef_ini_q_m1p1 : 12;
    unsigned int rsvd_1 : 4;
  } b;
} RG_ESTI_A145_FIELD_T;

#define RG_ESTI_A146                              (RF_D_ADDA_ESTI_REG_BASE + 0x244)
// Bit 11  :0      rg_txdpd_esti_coef_ini_i_m2p1     U     RW        default = 'h0
// Bit 27  :16     rg_txdpd_esti_coef_ini_q_m2p1     U     RW        default = 'h0
typedef union RG_ESTI_A146_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_coef_ini_i_m2p1 : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_coef_ini_q_m2p1 : 12;
    unsigned int rsvd_1 : 4;
  } b;
} RG_ESTI_A146_FIELD_T;

#define RG_ESTI_A147                              (RF_D_ADDA_ESTI_REG_BASE + 0x248)
// Bit 11  :0      rg_txdpd_esti_coef_ini_i_m3p1     U     RW        default = 'h0
// Bit 27  :16     rg_txdpd_esti_coef_ini_q_m3p1     U     RW        default = 'h0
typedef union RG_ESTI_A147_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_coef_ini_i_m3p1 : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_coef_ini_q_m3p1 : 12;
    unsigned int rsvd_1 : 4;
  } b;
} RG_ESTI_A147_FIELD_T;

#define RG_ESTI_A148                              (RF_D_ADDA_ESTI_REG_BASE + 0x24c)
// Bit 11  :0      rg_txdpd_esti_coef_ini_i_m4p1     U     RW        default = 'h0
// Bit 27  :16     rg_txdpd_esti_coef_ini_q_m4p1     U     RW        default = 'h0
typedef union RG_ESTI_A148_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdpd_esti_coef_ini_i_m4p1 : 12;
    unsigned int rsvd_0 : 4;
    unsigned int rg_txdpd_esti_coef_ini_q_m4p1 : 12;
    unsigned int rsvd_1 : 4;
  } b;
} RG_ESTI_A148_FIELD_T;

#define RG_ESTI_A149                              (RF_D_ADDA_ESTI_REG_BASE + 0x250)
// Bit 1   :0      rg_snr_single_path_sel         U     RW        default = 'h0
// Bit 4           rg_snr_sig_alpha_double        U     RW        default = 'h0
// Bit 8           rg_snr_rssi_sel                U     RW        default = 'h0
typedef union RG_ESTI_A149_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_snr_single_path_sel : 2;
    unsigned int rsvd_0 : 2;
    unsigned int rg_snr_sig_alpha_double : 1;
    unsigned int rsvd_1 : 3;
    unsigned int rg_snr_rssi_sel : 1;
    unsigned int rsvd_2 : 23;
  } b;
} RG_ESTI_A149_FIELD_T;

#define RG_ESTI_A150                              (RF_D_ADDA_ESTI_REG_BASE + 0x254)
// Bit 11  :0      ro_txdpd_esti_coef_i_15        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_15        U     RO        default = 'h0
typedef union RG_ESTI_A150_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_15 : 12;
    unsigned int ro_txdpd_esti_coef_q_15 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A150_FIELD_T;

#define RG_ESTI_A151                              (RF_D_ADDA_ESTI_REG_BASE + 0x258)
// Bit 11  :0      ro_txdpd_esti_coef_i_16        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_16        U     RO        default = 'h0
typedef union RG_ESTI_A151_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_16 : 12;
    unsigned int ro_txdpd_esti_coef_q_16 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A151_FIELD_T;

#define RG_ESTI_A152                              (RF_D_ADDA_ESTI_REG_BASE + 0x25c)
// Bit 11  :0      ro_txdpd_esti_coef_i_17        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_17        U     RO        default = 'h0
typedef union RG_ESTI_A152_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_17 : 12;
    unsigned int ro_txdpd_esti_coef_q_17 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A152_FIELD_T;

#define RG_ESTI_A153                              (RF_D_ADDA_ESTI_REG_BASE + 0x260)
// Bit 11  :0      ro_txdpd_esti_coef_i_18        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_18        U     RO        default = 'h0
typedef union RG_ESTI_A153_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_18 : 12;
    unsigned int ro_txdpd_esti_coef_q_18 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A153_FIELD_T;

#define RG_ESTI_A154                              (RF_D_ADDA_ESTI_REG_BASE + 0x264)
// Bit 11  :0      ro_txdpd_esti_coef_i_19        U     RO        default = 'h0
// Bit 23  :12     ro_txdpd_esti_coef_q_19        U     RO        default = 'h0
typedef union RG_ESTI_A154_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdpd_esti_coef_i_19 : 12;
    unsigned int ro_txdpd_esti_coef_q_19 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A154_FIELD_T;

#define RG_ESTI_A155                              (RF_D_ADDA_ESTI_REG_BASE + 0x268)
// Bit 0           rg_rx_irr_fiiq_cali_man_en     U     RW        default = 'h0
// Bit 1           rg_rx_irr_fiiq_cali_enable     U     RW        default = 'h0
// Bit 4           rg_rx_irr_fdiq_cali_man_en     U     RW        default = 'h0
// Bit 5           rg_rx_irr_fdiq_cali_enable     U     RW        default = 'h0
// Bit 8           rg_rx_dc_cali_man_en           U     RW        default = 'h0
// Bit 9           rg_rx_dc_cali_enable           U     RW        default = 'h0
// Bit 12          rg_tx_dc_cali_man_en           U     RW        default = 'h0
// Bit 13          rg_tx_dc_cali_enable           U     RW        default = 'h0
// Bit 16          rg_tx_alpha_cali_man_en        U     RW        default = 'h0
// Bit 17          rg_tx_alpha_cali_enable        U     RW        default = 'h0
// Bit 20          rg_tx_theata_cali_man_en       U     RW        default = 'h0
// Bit 21          rg_tx_theata_cali_enable       U     RW        default = 'h0
// Bit 24          rg_tx_olpc_offset_cali_man_en     U     RW        default = 'h0
// Bit 25          rg_tx_olpc_offset_cali_enable     U     RW        default = 'h0
// Bit 28          rg_tx_olpc_table_cali_man_en     U     RW        default = 'h0
// Bit 29          rg_tx_olpc_table_cali_enable     U     RW        default = 'h0
typedef union RG_ESTI_A155_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fiiq_cali_man_en : 1;
    unsigned int rg_rx_irr_fiiq_cali_enable : 1;
    unsigned int rsvd_0 : 2;
    unsigned int rg_rx_irr_fdiq_cali_man_en : 1;
    unsigned int rg_rx_irr_fdiq_cali_enable : 1;
    unsigned int rsvd_1 : 2;
    unsigned int rg_rx_dc_cali_man_en : 1;
    unsigned int rg_rx_dc_cali_enable : 1;
    unsigned int rsvd_2 : 2;
    unsigned int rg_tx_dc_cali_man_en : 1;
    unsigned int rg_tx_dc_cali_enable : 1;
    unsigned int rsvd_3 : 2;
    unsigned int rg_tx_alpha_cali_man_en : 1;
    unsigned int rg_tx_alpha_cali_enable : 1;
    unsigned int rsvd_4 : 2;
    unsigned int rg_tx_theata_cali_man_en : 1;
    unsigned int rg_tx_theata_cali_enable : 1;
    unsigned int rsvd_5 : 2;
    unsigned int rg_tx_olpc_offset_cali_man_en : 1;
    unsigned int rg_tx_olpc_offset_cali_enable : 1;
    unsigned int rsvd_6 : 2;
    unsigned int rg_tx_olpc_table_cali_man_en : 1;
    unsigned int rg_tx_olpc_table_cali_enable : 1;
    unsigned int rsvd_7 : 2;
  } b;
} RG_ESTI_A155_FIELD_T;

#define RG_ESTI_A157                              (RF_D_ADDA_ESTI_REG_BASE + 0x270)
// Bit 0           rg_irr_cal_module_ddc_angle_man_mode     U     RW        default = 'h0
// Bit 28  :4      rg_irr_cal_module_ddc_angle     U     RW        default = 'h0
typedef union RG_ESTI_A157_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_cal_module_ddc_angle_man_mode : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_irr_cal_module_ddc_angle : 25;
    unsigned int rsvd_1 : 3;
  } b;
} RG_ESTI_A157_FIELD_T;

#define RG_ESTI_A158                              (RF_D_ADDA_ESTI_REG_BASE + 0x274)
// Bit 0           rg_irr_cal_module_ddc_bypass_man_mode     U     RW        default = 'h0
// Bit 4           rg_irr_cal_module_ddc_bypass     U     RW        default = 'h0
// Bit 8           rg_irr_cal_module_avg_bypass_man_mode     U     RW        default = 'h0
// Bit 12          rg_irr_cal_module_avg_bypass     U     RW        default = 'h0
// Bit 16          rg_irr_cal_module_loop_phase_bypass_man_mode     U     RW        default = 'h0
// Bit 20          rg_irr_cal_module_loop_phase_bypass     U     RW        default = 'h0
// Bit 27  :24     rg_irr_cal_module_period       U     RW        default = 'h0
typedef union RG_ESTI_A158_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_cal_module_ddc_bypass_man_mode : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_irr_cal_module_ddc_bypass : 1;
    unsigned int rsvd_1 : 3;
    unsigned int rg_irr_cal_module_avg_bypass_man_mode : 1;
    unsigned int rsvd_2 : 3;
    unsigned int rg_irr_cal_module_avg_bypass : 1;
    unsigned int rsvd_3 : 3;
    unsigned int rg_irr_cal_module_loop_phase_bypass_man_mode : 1;
    unsigned int rsvd_4 : 3;
    unsigned int rg_irr_cal_module_loop_phase_bypass : 1;
    unsigned int rsvd_5 : 3;
    unsigned int rg_irr_cal_module_period : 4;
    unsigned int rsvd_6 : 4;
  } b;
} RG_ESTI_A158_FIELD_T;

#define RG_ESTI_A159                              (RF_D_ADDA_ESTI_REG_BASE + 0x278)
// Bit 0           rg_rxirr_fiiq_read_response     U     RW        default = 'h0
// Bit 1           rg_rxirr_fiiq_read_response_bypass     U     RW        default = 'h1
// Bit 4           rg_rxirr_fdiq_read_response     U     RW        default = 'h0
// Bit 5           rg_rxirr_fdiq_read_response_bypass     U     RW        default = 'h1
// Bit 8           rg_rxdc_read_response          U     RW        default = 'h0
// Bit 9           rg_rxdc_read_response_bypass     U     RW        default = 'h1
// Bit 12          rg_txirr_dc_read_response      U     RW        default = 'h0
// Bit 13          rg_txirr_dc_read_response_bypass     U     RW        default = 'h1
// Bit 16          rg_txirr_alpha_read_response     U     RW        default = 'h0
// Bit 17          rg_txirr_alpha_read_response_bypass     U     RW        default = 'h1
// Bit 20          rg_txirr_theta_read_response     U     RW        default = 'h0
// Bit 21          rg_txirr_theta_read_response_bypass     U     RW        default = 'h1
// Bit 24          rg_tx_olpc_offset_read_response     U     RW        default = 'h0
// Bit 25          rg_tx_olpc_offset_read_response_bypass     U     RW        default = 'h1
// Bit 28          rg_tx_olpc_table_read_response     U     RW        default = 'h0
// Bit 29          rg_tx_olpc_table_read_response_bypass     U     RW        default = 'h1
typedef union RG_ESTI_A159_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fiiq_read_response : 1;
    unsigned int rg_rxirr_fiiq_read_response_bypass : 1;
    unsigned int rsvd_0 : 2;
    unsigned int rg_rxirr_fdiq_read_response : 1;
    unsigned int rg_rxirr_fdiq_read_response_bypass : 1;
    unsigned int rsvd_1 : 2;
    unsigned int rg_rxdc_read_response : 1;
    unsigned int rg_rxdc_read_response_bypass : 1;
    unsigned int rsvd_2 : 2;
    unsigned int rg_txirr_dc_read_response : 1;
    unsigned int rg_txirr_dc_read_response_bypass : 1;
    unsigned int rsvd_3 : 2;
    unsigned int rg_txirr_alpha_read_response : 1;
    unsigned int rg_txirr_alpha_read_response_bypass : 1;
    unsigned int rsvd_4 : 2;
    unsigned int rg_txirr_theta_read_response : 1;
    unsigned int rg_txirr_theta_read_response_bypass : 1;
    unsigned int rsvd_5 : 2;
    unsigned int rg_tx_olpc_offset_read_response : 1;
    unsigned int rg_tx_olpc_offset_read_response_bypass : 1;
    unsigned int rsvd_6 : 2;
    unsigned int rg_tx_olpc_table_read_response : 1;
    unsigned int rg_tx_olpc_table_read_response_bypass : 1;
    unsigned int rsvd_7 : 2;
  } b;
} RG_ESTI_A159_FIELD_T;

#define RG_ESTI_A160                              (RF_D_ADDA_ESTI_REG_BASE + 0x27c)
// Bit 0           rg_rxirr_fiiq_alpha_theta_avg_bypass     U     RW        default = 'h1
// Bit 4           rg_txirr_theta_avg_bypass      U     RW        default = 'h1
typedef union RG_ESTI_A160_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fiiq_alpha_theta_avg_bypass : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_txirr_theta_avg_bypass : 1;
    unsigned int rsvd_1 : 27;
  } b;
} RG_ESTI_A160_FIELD_T;

#define RG_ESTI_A161                              (RF_D_ADDA_ESTI_REG_BASE + 0x280)
// Bit 0           rg_tx_olpc_ft_mode             U     RW        default = 'h0
// Bit 8   :4      rg_tx_olpc_offset_index        U     RW        default = 'h9
// Bit 16  :12     rg_tx_olpc_table_index_bnd     U     RW        default = 'h1d
// Bit 22  :18     rg_tx_olpc_table_index_init     U     RW        default = 'h0
// Bit 28  :24     rg_tx_olpc_table_index_fina     U     RW        default = 'h0
typedef union RG_ESTI_A161_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_olpc_ft_mode : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_tx_olpc_offset_index : 5;
    unsigned int rsvd_1 : 3;
    unsigned int rg_tx_olpc_table_index_bnd : 5;
    unsigned int rsvd_2 : 1;
    unsigned int rg_tx_olpc_table_index_init : 5;
    unsigned int rsvd_3 : 1;
    unsigned int rg_tx_olpc_table_index_fina : 5;
    unsigned int rsvd_4 : 3;
  } b;
} RG_ESTI_A161_FIELD_T;

#define RG_ESTI_A162                              (RF_D_ADDA_ESTI_REG_BASE + 0x284)
// Bit 3   :0      rg_fdiq_bw20_freq_bnd          U     RW        default = 'h8
// Bit 11  :8      rg_fdiq_bw40_freq_bnd          U     RW        default = 'h7
// Bit 19  :16     rg_fdiq_bw80_freq_bnd          U     RW        default = 'h8
typedef union RG_ESTI_A162_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_fdiq_bw20_freq_bnd : 4;
    unsigned int rsvd_0 : 4;
    unsigned int rg_fdiq_bw40_freq_bnd : 4;
    unsigned int rsvd_1 : 4;
    unsigned int rg_fdiq_bw80_freq_bnd : 4;
    unsigned int rsvd_2 : 12;
  } b;
} RG_ESTI_A162_FIELD_T;

#define RG_ESTI_A163                              (RF_D_ADDA_ESTI_REG_BASE + 0x288)
// Bit 0           rg_irr_tone_freq_man_mode      U     RW        default = 'h0
// Bit 28  :4      rg_irr_tone_freq_man           U     RW        default = 'h0
typedef union RG_ESTI_A163_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_irr_tone_freq_man_mode : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_irr_tone_freq_man : 25;
    unsigned int rsvd_1 : 3;
  } b;
} RG_ESTI_A163_FIELD_T;

#define RG_ESTI_A164                              (RF_D_ADDA_ESTI_REG_BASE + 0x28c)
// Bit 12  :0      rg_rxirr_fiiq_freq             U     RW        default = 'h0
typedef union RG_ESTI_A164_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fiiq_freq : 13;
    unsigned int rsvd_0 : 19;
  } b;
} RG_ESTI_A164_FIELD_T;

#define RG_ESTI_A165                              (RF_D_ADDA_ESTI_REG_BASE + 0x290)
// Bit 8   :0      rg_rxirr_fiiq_gain_lg          U     RW        default = 'h0
// Bit 20  :12     rg_rxirr_fiiq_gain_hg          U     RW        default = 'h0
typedef union RG_ESTI_A165_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fiiq_gain_lg : 9;
    unsigned int rsvd_0 : 3;
    unsigned int rg_rxirr_fiiq_gain_hg : 9;
    unsigned int rsvd_1 : 11;
  } b;
} RG_ESTI_A165_FIELD_T;

#define RG_ESTI_A166                              (RF_D_ADDA_ESTI_REG_BASE + 0x294)
// Bit 7   :0      rg_rxirr_fiiq_freq_offset_0     U     RW        default = 'h0
// Bit 15  :8      rg_rxirr_fiiq_freq_offset_1     U     RW        default = 'h0
typedef union RG_ESTI_A166_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fiiq_freq_offset_0 : 8;
    unsigned int rg_rxirr_fiiq_freq_offset_1 : 8;
    unsigned int rsvd_0 : 16;
  } b;
} RG_ESTI_A166_FIELD_T;

#define RG_ESTI_A167                              (RF_D_ADDA_ESTI_REG_BASE + 0x298)
// Bit 12  :0      rg_rxirr_fdiq_freq             U     RW        default = 'h0
typedef union RG_ESTI_A167_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fdiq_freq : 13;
    unsigned int rsvd_0 : 19;
  } b;
} RG_ESTI_A167_FIELD_T;

#define RG_ESTI_A168                              (RF_D_ADDA_ESTI_REG_BASE + 0x29c)
// Bit 7   :0      rg_rxirr_fdiq_freq_offset_0     U     RW        default = 'h0
// Bit 15  :8      rg_rxirr_fdiq_freq_offset_1     U     RW        default = 'h0
// Bit 23  :16     rg_rxirr_fdiq_freq_offset_2     U     RW        default = 'h0
// Bit 31  :24     rg_rxirr_fdiq_freq_offset_3     U     RW        default = 'h0
typedef union RG_ESTI_A168_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fdiq_freq_offset_0 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_1 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_2 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_3 : 8;
  } b;
} RG_ESTI_A168_FIELD_T;

#define RG_ESTI_A169                              (RF_D_ADDA_ESTI_REG_BASE + 0x2a0)
// Bit 7   :0      rg_rxirr_fdiq_freq_offset_4     U     RW        default = 'h0
// Bit 15  :8      rg_rxirr_fdiq_freq_offset_5     U     RW        default = 'h0
// Bit 23  :16     rg_rxirr_fdiq_freq_offset_6     U     RW        default = 'h0
// Bit 31  :24     rg_rxirr_fdiq_freq_offset_7     U     RW        default = 'h0
typedef union RG_ESTI_A169_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fdiq_freq_offset_4 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_5 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_6 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_7 : 8;
  } b;
} RG_ESTI_A169_FIELD_T;

#define RG_ESTI_A170                              (RF_D_ADDA_ESTI_REG_BASE + 0x2a4)
// Bit 7   :0      rg_rxirr_fdiq_freq_offset_8     U     RW        default = 'h0
// Bit 15  :8      rg_rxirr_fdiq_freq_offset_9     U     RW        default = 'h0
// Bit 23  :16     rg_rxirr_fdiq_freq_offset_10     U     RW        default = 'h0
typedef union RG_ESTI_A170_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxirr_fdiq_freq_offset_8 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_9 : 8;
    unsigned int rg_rxirr_fdiq_freq_offset_10 : 8;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A170_FIELD_T;

#define RG_ESTI_A171                              (RF_D_ADDA_ESTI_REG_BASE + 0x2a8)
// Bit 24  :0      rg_rxdc_freq                   U     RW        default = 'h0
typedef union RG_ESTI_A171_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rxdc_freq : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A171_FIELD_T;

#define RG_ESTI_A172                              (RF_D_ADDA_ESTI_REG_BASE + 0x2ac)
// Bit 24  :0      rg_txirr_dc_freq_0             U     RW        default = 'h0
typedef union RG_ESTI_A172_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_dc_freq_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A172_FIELD_T;

#define RG_ESTI_A173                              (RF_D_ADDA_ESTI_REG_BASE + 0x2b0)
// Bit 24  :0      rg_txirr_dc_freq_1             U     RW        default = 'h0
typedef union RG_ESTI_A173_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_dc_freq_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A173_FIELD_T;

#define RG_ESTI_A174                              (RF_D_ADDA_ESTI_REG_BASE + 0x2b4)
// Bit 7   :0      rg_txdc_cali_i                 U     RW        default = 'h0
// Bit 15  :8      rg_txdc_cali_q                 U     RW        default = 'h0
typedef union RG_ESTI_A174_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txdc_cali_i : 8;
    unsigned int rg_txdc_cali_q : 8;
    unsigned int rsvd_0 : 16;
  } b;
} RG_ESTI_A174_FIELD_T;

#define RG_ESTI_A175                              (RF_D_ADDA_ESTI_REG_BASE + 0x2b8)
// Bit 24  :0      rg_txirr_alpha_freq_0          U     RW        default = 'h0
typedef union RG_ESTI_A175_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_alpha_freq_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A175_FIELD_T;

#define RG_ESTI_A176                              (RF_D_ADDA_ESTI_REG_BASE + 0x2bc)
// Bit 24  :0      rg_txirr_alpha_freq_1          U     RW        default = 'h0
typedef union RG_ESTI_A176_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_alpha_freq_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A176_FIELD_T;

#define RG_ESTI_A177                              (RF_D_ADDA_ESTI_REG_BASE + 0x2c0)
// Bit 7   :0      rg_tx_alpha_calc_count         U     RW        default = 'h3f
typedef union RG_ESTI_A177_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_alpha_calc_count : 8;
    unsigned int rsvd_0 : 24;
  } b;
} RG_ESTI_A177_FIELD_T;

#define RG_ESTI_A178                              (RF_D_ADDA_ESTI_REG_BASE + 0x2c4)
// Bit 24  :0      rg_txirr_theta_freq_0          U     RW        default = 'h0
typedef union RG_ESTI_A178_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_theta_freq_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A178_FIELD_T;

#define RG_ESTI_A179                              (RF_D_ADDA_ESTI_REG_BASE + 0x2c8)
// Bit 24  :0      rg_txirr_theta_freq_1          U     RW        default = 'h0
typedef union RG_ESTI_A179_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_theta_freq_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A179_FIELD_T;

#define RG_ESTI_A180                              (RF_D_ADDA_ESTI_REG_BASE + 0x2cc)
// Bit 24  :0      rg_tx_olpc_offset_freq         U     RW        default = 'h0
typedef union RG_ESTI_A180_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_olpc_offset_freq : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A180_FIELD_T;

#define RG_ESTI_A181                              (RF_D_ADDA_ESTI_REG_BASE + 0x2d0)
// Bit 24  :0      rg_tx_olpc_table_freq          U     RW        default = 'h0
typedef union RG_ESTI_A181_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_olpc_table_freq : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A181_FIELD_T;

#define RG_ESTI_A182                              (RF_D_ADDA_ESTI_REG_BASE + 0x2d4)
// Bit 5   :0      rg_lpf_offset                  U     RW        default = 'h0
typedef union RG_ESTI_A182_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_lpf_offset : 6;
    unsigned int rsvd_0 : 26;
  } b;
} RG_ESTI_A182_FIELD_T;

#define RG_ESTI_A184                              (RF_D_ADDA_ESTI_REG_BASE + 0x2dc)
// Bit 11  :0      rg_rx_power_ideal              U     RW        default = 'h0
typedef union RG_ESTI_A184_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_power_ideal : 12;
    unsigned int rsvd_0 : 20;
  } b;
} RG_ESTI_A184_FIELD_T;

#define RG_ESTI_A185                              (RF_D_ADDA_ESTI_REG_BASE + 0x2e0)
// Bit 0           rg_rf_ttg_freq_man_mode        U     RW        default = 'h0
// Bit 16  :4      rg_rf_ttg_freq_man             U     RW        default = 'h0
typedef union RG_ESTI_A185_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rf_ttg_freq_man_mode : 1;
    unsigned int rsvd_0 : 3;
    unsigned int rg_rf_ttg_freq_man : 13;
    unsigned int rsvd_1 : 15;
  } b;
} RG_ESTI_A185_FIELD_T;

#define RG_ESTI_A186                              (RF_D_ADDA_ESTI_REG_BASE + 0x2e4)
// Bit 24  :0      rg_rx_irr_fiiq_ddc_angle_0     U     RW        default = 'h0
typedef union RG_ESTI_A186_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fiiq_ddc_angle_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A186_FIELD_T;

#define RG_ESTI_A187                              (RF_D_ADDA_ESTI_REG_BASE + 0x2e8)
// Bit 24  :0      rg_rx_irr_fiiq_ddc_angle_1     U     RW        default = 'h0
typedef union RG_ESTI_A187_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fiiq_ddc_angle_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A187_FIELD_T;

#define RG_ESTI_A188                              (RF_D_ADDA_ESTI_REG_BASE + 0x2ec)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_0     U     RW        default = 'h0
typedef union RG_ESTI_A188_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A188_FIELD_T;

#define RG_ESTI_A189                              (RF_D_ADDA_ESTI_REG_BASE + 0x2f0)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_1     U     RW        default = 'h0
typedef union RG_ESTI_A189_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A189_FIELD_T;

#define RG_ESTI_A190                              (RF_D_ADDA_ESTI_REG_BASE + 0x2f4)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_2     U     RW        default = 'h0
typedef union RG_ESTI_A190_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_2 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A190_FIELD_T;

#define RG_ESTI_A191                              (RF_D_ADDA_ESTI_REG_BASE + 0x2f8)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_3     U     RW        default = 'h0
typedef union RG_ESTI_A191_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_3 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A191_FIELD_T;

#define RG_ESTI_A192                              (RF_D_ADDA_ESTI_REG_BASE + 0x2fc)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_4     U     RW        default = 'h0
typedef union RG_ESTI_A192_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_4 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A192_FIELD_T;

#define RG_ESTI_A193                              (RF_D_ADDA_ESTI_REG_BASE + 0x300)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_5     U     RW        default = 'h0
typedef union RG_ESTI_A193_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_5 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A193_FIELD_T;

#define RG_ESTI_A194                              (RF_D_ADDA_ESTI_REG_BASE + 0x304)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_6     U     RW        default = 'h0
typedef union RG_ESTI_A194_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_6 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A194_FIELD_T;

#define RG_ESTI_A195                              (RF_D_ADDA_ESTI_REG_BASE + 0x308)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_7     U     RW        default = 'h0
typedef union RG_ESTI_A195_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_7 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A195_FIELD_T;

#define RG_ESTI_A196                              (RF_D_ADDA_ESTI_REG_BASE + 0x30c)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_8     U     RW        default = 'h0
typedef union RG_ESTI_A196_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_8 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A196_FIELD_T;

#define RG_ESTI_A197                              (RF_D_ADDA_ESTI_REG_BASE + 0x310)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_9     U     RW        default = 'h0
typedef union RG_ESTI_A197_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_9 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A197_FIELD_T;

#define RG_ESTI_A198                              (RF_D_ADDA_ESTI_REG_BASE + 0x314)
// Bit 24  :0      rg_rx_irr_fdiq_ddc_angle_10     U     RW        default = 'h0
typedef union RG_ESTI_A198_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_rx_irr_fdiq_ddc_angle_10 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A198_FIELD_T;

#define RG_ESTI_A199                              (RF_D_ADDA_ESTI_REG_BASE + 0x318)
// Bit 24  :0      rg_txirr_alpha_ddc_angle_0     U     RW        default = 'h0
typedef union RG_ESTI_A199_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_alpha_ddc_angle_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A199_FIELD_T;

#define RG_ESTI_A200                              (RF_D_ADDA_ESTI_REG_BASE + 0x31c)
// Bit 24  :0      rg_txirr_alpha_ddc_angle_1     U     RW        default = 'h0
typedef union RG_ESTI_A200_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_alpha_ddc_angle_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A200_FIELD_T;

#define RG_ESTI_A201                              (RF_D_ADDA_ESTI_REG_BASE + 0x320)
// Bit 24  :0      rg_txirr_theta_ddc_angle_0     U     RW        default = 'h0
typedef union RG_ESTI_A201_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_theta_ddc_angle_0 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A201_FIELD_T;

#define RG_ESTI_A202                              (RF_D_ADDA_ESTI_REG_BASE + 0x324)
// Bit 24  :0      rg_txirr_theta_ddc_angle_1     U     RW        default = 'h0
typedef union RG_ESTI_A202_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_txirr_theta_ddc_angle_1 : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A202_FIELD_T;

#define RG_ESTI_A203                              (RF_D_ADDA_ESTI_REG_BASE + 0x328)
// Bit 24  :0      rg_tx_olpc_offset_ddc_angle     U     RW        default = 'h0
typedef union RG_ESTI_A203_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_olpc_offset_ddc_angle : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A203_FIELD_T;

#define RG_ESTI_A204                              (RF_D_ADDA_ESTI_REG_BASE + 0x32c)
// Bit 24  :0      rg_tx_olpc_table_ddc_angle     U     RW        default = 'h0
typedef union RG_ESTI_A204_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int rg_tx_olpc_table_ddc_angle : 25;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A204_FIELD_T;

#define RG_ESTI_A205                              (RF_D_ADDA_ESTI_REG_BASE + 0x330)
// Bit 10  :0      ro_rx_irr_alpha_fiiq_lg        U     RO        default = 'h0
// Bit 20  :12     ro_rx_irr_theta_fiiq_lg        U     RO        default = 'h0
typedef union RG_ESTI_A205_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_alpha_fiiq_lg : 11;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rx_irr_theta_fiiq_lg : 9;
    unsigned int rsvd_1 : 11;
  } b;
} RG_ESTI_A205_FIELD_T;

#define RG_ESTI_A206                              (RF_D_ADDA_ESTI_REG_BASE + 0x334)
// Bit 10  :0      ro_rx_irr_alpha_fiiq_hg        U     RO        default = 'h0
// Bit 20  :12     ro_rx_irr_theta_fiiq_hg        U     RO        default = 'h0
// Bit 24          ro_rx_irr_fiiq_ready           U     RO        default = 'h0
typedef union RG_ESTI_A206_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_alpha_fiiq_hg : 11;
    unsigned int rsvd_0 : 1;
    unsigned int ro_rx_irr_theta_fiiq_hg : 9;
    unsigned int rsvd_1 : 3;
    unsigned int ro_rx_irr_fiiq_ready : 1;
    unsigned int rsvd_2 : 7;
  } b;
} RG_ESTI_A206_FIELD_T;

#define RG_ESTI_A207                              (RF_D_ADDA_ESTI_REG_BASE + 0x338)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_0     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_0     U     RO        default = 'h0
typedef union RG_ESTI_A207_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_0 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_0 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A207_FIELD_T;

#define RG_ESTI_A208                              (RF_D_ADDA_ESTI_REG_BASE + 0x33c)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_1     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_1     U     RO        default = 'h0
typedef union RG_ESTI_A208_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_1 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_1 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A208_FIELD_T;

#define RG_ESTI_A209                              (RF_D_ADDA_ESTI_REG_BASE + 0x340)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_2     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_2     U     RO        default = 'h0
typedef union RG_ESTI_A209_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_2 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_2 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A209_FIELD_T;

#define RG_ESTI_A210                              (RF_D_ADDA_ESTI_REG_BASE + 0x344)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_3     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_3     U     RO        default = 'h0
typedef union RG_ESTI_A210_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_3 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_3 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A210_FIELD_T;

#define RG_ESTI_A211                              (RF_D_ADDA_ESTI_REG_BASE + 0x348)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_4     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_4     U     RO        default = 'h0
typedef union RG_ESTI_A211_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_4 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_4 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A211_FIELD_T;

#define RG_ESTI_A212                              (RF_D_ADDA_ESTI_REG_BASE + 0x34c)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_5     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_5     U     RO        default = 'h0
typedef union RG_ESTI_A212_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_5 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_5 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A212_FIELD_T;

#define RG_ESTI_A213                              (RF_D_ADDA_ESTI_REG_BASE + 0x350)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_6     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_6     U     RO        default = 'h0
typedef union RG_ESTI_A213_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_6 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_6 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A213_FIELD_T;

#define RG_ESTI_A214                              (RF_D_ADDA_ESTI_REG_BASE + 0x354)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_7     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_7     U     RO        default = 'h0
typedef union RG_ESTI_A214_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_7 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_7 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A214_FIELD_T;

#define RG_ESTI_A215                              (RF_D_ADDA_ESTI_REG_BASE + 0x358)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_8     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_8     U     RO        default = 'h0
typedef union RG_ESTI_A215_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_8 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_8 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A215_FIELD_T;

#define RG_ESTI_A216                              (RF_D_ADDA_ESTI_REG_BASE + 0x35c)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_9     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_9     U     RO        default = 'h0
typedef union RG_ESTI_A216_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_9 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_9 : 12;
    unsigned int rsvd_0 : 8;
  } b;
} RG_ESTI_A216_FIELD_T;

#define RG_ESTI_A217                              (RF_D_ADDA_ESTI_REG_BASE + 0x360)
// Bit 11  :0      ro_rx_irr_fdiq_fd_coeff_i_10     U     RO        default = 'h0
// Bit 23  :12     ro_rx_irr_fdiq_fd_coeff_q_10     U     RO        default = 'h0
// Bit 24          ro_rx_irr_fdiq_ready           U     RO        default = 'h0
typedef union RG_ESTI_A217_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_irr_fdiq_fd_coeff_i_10 : 12;
    unsigned int ro_rx_irr_fdiq_fd_coeff_q_10 : 12;
    unsigned int ro_rx_irr_fdiq_ready : 1;
    unsigned int rsvd_0 : 7;
  } b;
} RG_ESTI_A217_FIELD_T;

#define RG_ESTI_A218                              (RF_D_ADDA_ESTI_REG_BASE + 0x364)
// Bit 15  :0      ro_rx_dc_i                     U     RO        default = 'h0
// Bit 31  :16     ro_rx_dc_q                     U     RO        default = 'h0
typedef union RG_ESTI_A218_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_dc_i : 16;
    unsigned int ro_rx_dc_q : 16;
  } b;
} RG_ESTI_A218_FIELD_T;

#define RG_ESTI_A219                              (RF_D_ADDA_ESTI_REG_BASE + 0x368)
// Bit 0           ro_rx_dc_ready                 U     RO        default = 'h0
typedef union RG_ESTI_A219_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_rx_dc_ready : 1;
    unsigned int rsvd_0 : 31;
  } b;
} RG_ESTI_A219_FIELD_T;

#define RG_ESTI_A220                              (RF_D_ADDA_ESTI_REG_BASE + 0x36c)
// Bit 7   :0      ro_txdc_comp_i                 U     RO        default = 'h0
// Bit 15  :8      ro_txdc_comp_q                 U     RO        default = 'h0
// Bit 16          ro_txdc_comp_ready             U     RO        default = 'h0
typedef union RG_ESTI_A220_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_txdc_comp_i : 8;
    unsigned int ro_txdc_comp_q : 8;
    unsigned int ro_txdc_comp_ready : 1;
    unsigned int rsvd_0 : 15;
  } b;
} RG_ESTI_A220_FIELD_T;

#define RG_ESTI_A221                              (RF_D_ADDA_ESTI_REG_BASE + 0x370)
// Bit 11  :0      ro_loop_phase_i                U     RO        default = 'h0
// Bit 23  :12     ro_loop_phase_q                U     RO        default = 'h0
// Bit 31  :24     ro_tx_alpha                    U     RO        default = 'h0
typedef union RG_ESTI_A221_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_loop_phase_i : 12;
    unsigned int ro_loop_phase_q : 12;
    unsigned int ro_tx_alpha : 8;
  } b;
} RG_ESTI_A221_FIELD_T;

#define RG_ESTI_A222                              (RF_D_ADDA_ESTI_REG_BASE + 0x374)
// Bit 0           ro_tx_alpha_ready              U     RO        default = 'h0
typedef union RG_ESTI_A222_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_tx_alpha_ready : 1;
    unsigned int rsvd_0 : 31;
  } b;
} RG_ESTI_A222_FIELD_T;

#define RG_ESTI_A223                              (RF_D_ADDA_ESTI_REG_BASE + 0x378)
// Bit 8   :0      ro_tx_theta                    U     RO        default = 'h0
// Bit 12          ro_tx_theta_ready              U     RO        default = 'h0
typedef union RG_ESTI_A223_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_tx_theta : 9;
    unsigned int rsvd_0 : 3;
    unsigned int ro_tx_theta_ready : 1;
    unsigned int rsvd_1 : 19;
  } b;
} RG_ESTI_A223_FIELD_T;

#define RG_ESTI_A224                              (RF_D_ADDA_ESTI_REG_BASE + 0x37c)
// Bit 0           ro_en_lpf_off                  U     RO        default = 'h0
// Bit 4           ro_en_lpf_off_ready            U     RO        default = 'h0
// Bit 20  :8      ro_olpc_ft_pow_out             U     RO        default = 'h0
typedef union RG_ESTI_A224_FIELD
{
  unsigned int data;
  struct
  {
    unsigned int ro_en_lpf_off : 1;
    unsigned int rsvd_0 : 3;
    unsigned int ro_en_lpf_off_ready : 1;
    unsigned int rsvd_1 : 3;
    unsigned int ro_olpc_ft_pow_out : 13;
    unsigned int rsvd_2 : 11;
  } b;
} RG_ESTI_A224_FIELD_T;

#endif

