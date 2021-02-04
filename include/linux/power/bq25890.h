/*****************************************************************************
*
* Filename:
* ---------
*   bq25890.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq25890 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq25890_SW_H_
#define _bq25890_SW_H_

#include <linux/power/mt_charging.h>

/* #define HIGH_BATTERY_VOLTAGE_SUPPORT */

#define bq25890_CON0	0x00
#define bq25890_CON1	0x01
#define bq25890_CON2	0x02
#define bq25890_CON3	0x03
#define bq25890_CON4	0x04
#define bq25890_CON5	0x05
#define bq25890_CON6	0x06
#define bq25890_CON7	0x07
#define bq25890_CON8	0x08
#define bq25890_CON9	0x09
#define bq25890_CON10	0x0A
#define bq25890_CON11	0x0B
#define bq25890_CON12	0x0C
#define bq25890_CON13	0x0D
#define bq25890_CON14	0x0E
#define bq25890_CON15	0x0F
#define bq25890_CON16	0x10
#define bq25890_CON17	0x11
#define bq25890_CON18	0x12
#define bq25890_CON19	0x13
#define bq25890_CON20	0x14

/**********************************************************
  *
  *   [MASK/SHIFT]
  *
  *********************************************************/
/* CON0 */
#define CON0_EN_HIZ_MASK   0x01
#define CON0_EN_HIZ_SHIFT  7

#define CON0_IINLIM_MASK   0x3F
#define CON0_IINLIM_SHIFT  0

/* CON1 */
#define CON1_VINDPM_OS_MASK 0x1F
#define CON1_VINDPM_OS_SHIFT 0

/* CON2 */
#define CON2_FORCE_DPDM_MASK 0x01
#define CON2_FORCE_DPDM_SHIFT 1

#define CON2_AUTO_DPDM_MASK 0x01
#define CON2_AUTO_DPDM_SHIFT 0

/* CON3 */
#define CON3_WDT_RST_MASK     0x01
#define CON3_WDT_RST_SHIFT    6

#define CON3_OTG_CONFIG_MASK        0x01
#define CON3_OTG_CONFIG_SHIFT       5

#define CON3_CHG_CONFIG_MASK        0x01
#define CON3_CHG_CONFIG_SHIFT       4

#define CON3_SYS_MIN_MASK        0x07
#define CON3_SYS_MIN_SHIFT       1

/* CON4 */
#define CON4_ICHG_MASK    0x7F
#define CON4_ICHG_SHIFT   0

/* CON5 */
#define CON5_IPRECHG_MASK   0x0F
#define CON5_IPRECHG_SHIFT  4

#define CON5_ITERM_MASK           0x0F
#define CON5_ITERM_SHIFT          0

/* CON6 */
#define CON6_VREG_MASK     0x3F
#define CON6_VREG_SHIFT    2

#define CON6_BATLOWV_MASK     0x01
#define CON6_BATLOWV_SHIFT    1

#define CON6_VRECHG_MASK    0x01
#define CON6_VRECHG_SHIFT   0

/* CON7 */
#define CON7_EN_TERM_MASK      0x01
#define CON7_EN_TERM_SHIFT     7

#define CON7_WATCHDOG_MASK     0x03
#define CON7_WATCHDOG_SHIFT    4

#define CON7_EN_TIMER_MASK      0x01
#define CON7_EN_TIMER_SHIFT     3

#define CON7_CHG_TIMER_MASK           0x03
#define CON7_CHG_TIMER_SHIFT          1

/* CON8 */
#define CON8_TREG_MASK     0x03
#define CON8_TREG_SHIFT    0

/* CON9 */
#define CON9_BATFET_DIS_MASK      0x01
#define CON9_BATFET_DIS_SHIFT     5

#define CON9_JEITA_VSET_MASK 0x01
#define CON9_JEITA_VSET_SHIFT 4

/* CON10 */
#define CON10_BOOST_LIM_MASK   0x07
#define CON10_BOOST_LIM_SHIFT  0

#define CON10_BOOSTV_MASK 0x0F
#define CON10_BOOSTV_SHIFT 4

/* CON11 */
#define CON11_VBUS_STAT_MASK      0x07
#define CON11_VBUS_STAT_SHIFT     5

#define CON11_CHRG_STAT_MASK           0x03
#define CON11_CHRG_STAT_SHIFT          3

#define CON11_PG_STAT_MASK           0x01
#define CON11_PG_STAT_SHIFT          2

#define CON11_SDP_STAT_MASK           0x01
#define CON11_SDP_STAT_SHIFT          1

#define CON11_VSYS_STAT_MASK           0x01
#define CON11_VSYS_STAT_SHIFT          0

/* CON12 */
#define CON12_WATCHDOG_FAULT_MASK      0x01
#define CON12_WATCHDOG_FAULT_SHIFT     7

#define CON12_OTG_FAULT_MASK           0x01
#define CON12_OTG_FAULT_SHIFT          6

#define CON12_CHRG_FAULT_MASK           0x03
#define CON12_CHRG_FAULT_SHIFT          4

#define CON12_CHRG_INPUT_FAULT_MASK     0x01
#define CON12_CHRG_THERMAL_SHUTDOWN_FAULT_MASK     0x02
#define CON12_CHRG_TIMER_EXPIRATION_FAULT_MASK     0x03

#define CON12_BAT_FAULT_MASK           0x01
#define CON12_BAT_FAULT_SHIFT          3

#define CON12_NTC_FAULT_MASK           0x07
#define CON12_NTC_FAULT_SHIFT          0

/* CON13 */
#define CON13_FORCE_VINDPM_MASK 0x01
#define CON13_FORCE_VINDPM_SHIFT 7

#define CON13_VINDPM_MASK 0x7F
#define CON13_VINDPM_SHIFT 0

/* CON19 */
#define CON19_VDPM_STAT_MASK           0x01
#define CON19_VDPM_STAT_SHIFT          7

#define CON19_IDPM_STAT_MASK           0x01
#define CON19_IDPM_STAT_SHIFT          6

#define CON19_IDPM_LIM_MASK 0x3F
#define CON19_IDPM_LIM_SHIFT 0

/* CON20 */
#define CON20_REG_RST_MASK 0x01
#define CON20_REG_RST_SHIFT 7

#define CON20_PN_MASK      0x07
#define CON20_PN_SHIFT     3

#define CON20_REV_MASK           0x03
#define CON20_REV_SHIFT          0

/* REG12 status */
enum BQ_FAULT {
	BQ_NORMAL_FAULT = 0,
	BQ_WATCHDOG_FAULT,
	BQ_OTG_FAULT,
	BQ_CHRG_NORMAL_FAULT,
	BQ_CHRG_INPUT_FAULT,
	BQ_CHRG_THERMAL_FAULT,
	BQ_CHRG_TIMER_EXPIRATION_FAULT,
	BQ_BAT_FAULT,
	BQ_NTC_FAULT,
	BQ_FAULT_MAX
};

/**********************************************************
  *
  *   [Extern Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */
extern void bq25890_set_en_hiz(kal_uint32 val);
extern void bq25890_set_iinlim(kal_uint32 val);
extern kal_uint32 bq25890_get_iinlim(void);
/* CON1---------------------------------------------------- */
/* CON2---------------------------------------------------- */
extern void bq25890_set_force_dpdm(kal_uint32 val);
extern void bq25890_set_auto_dpdm_en(kal_uint32 val);
extern kal_uint32 bq25890_get_dpdm_status(void);
/* CON3---------------------------------------------------- */
extern void bq25890_set_wdt_rst(kal_uint32 val);
extern void bq25890_set_otg_config(kal_uint32 val);
extern void bq25890_set_chg_config(kal_uint32 val);
extern void bq25890_set_sys_min(kal_uint32 val);
/* CON4---------------------------------------------------- */
extern void bq25890_set_ichg(kal_uint32 val);
extern kal_uint32 bq25890_get_ichg(void);
/* CON5---------------------------------------------------- */
extern void bq25890_set_iprechg(kal_uint32 val);
extern void bq25890_set_iterm(kal_uint32 val);
/* CON6---------------------------------------------------- */
extern void bq25890_set_vreg(kal_uint32 val);
extern void bq25890_set_batlowv(kal_uint32 val);
extern void bq25890_set_vrechg(kal_uint32 val);
/* CON7---------------------------------------------------- */
extern void bq25890_set_en_term(kal_uint32 val);
extern void bq25890_set_watchdog(kal_uint32 val);
extern void bq25890_set_en_timer(kal_uint32 val);
extern void bq25890_set_chg_timer(kal_uint32 val);
/* CON8---------------------------------------------------- */
extern void bq25890_set_treg(kal_uint32 val);
/* CON9---------------------------------------------------- */
extern void bq25890_set_batfet_disable(kal_uint32 val);
/* CON10---------------------------------------------------- */
extern void bq25890_set_boost_lim(kal_uint32 val);
/* CON11---------------------------------------------------- */
extern kal_uint32 bq25890_get_system_status(void);
extern kal_uint32 bq25890_get_vbus_stat(void);
extern kal_uint32 bq25890_get_chrg_stat(void);
extern kal_uint32 bq25890_get_pg_stat(void);
extern kal_uint32 bq25890_get_vsys_stat(void);
/* CON13---------------------------------------------------- */
extern void bq25890_set_vindpm(kal_uint32 val);
/* CON19---------------------------------------------------- */
extern kal_uint32 bq25890_get_vdpm_stat(void);
extern kal_uint32 bq25890_get_idpm_stat(void);
extern kal_uint32 bq25890_get_current_iinlim(void);
/* CON20---------------------------------------------------- */
extern void bq25890_set_reg_rst(kal_uint32 val);
extern kal_uint32 bq25890_get_pn(void);
extern kal_uint32 bq25890_get_rev(void);
/* --------------------------------------------------------- */

/*
extern void bq25890_set_term_stat(kal_uint32 val);
extern void bq25890_set_tmr2x_en(kal_uint32 val);
extern void bq25890_set_int_mask(kal_uint32 val);
extern void bq25890_set_force_20pct(kal_uint32 val);
*/

extern void bq25890_dump_register(void);
extern kal_uint32 bq25890_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK,
					 kal_uint8 SHIFT);
kal_int32 bq25890_control_interface(CHARGING_CTRL_CMD cmd, void *data);

extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

#endif				/* _bq25890_SW_H_ */
