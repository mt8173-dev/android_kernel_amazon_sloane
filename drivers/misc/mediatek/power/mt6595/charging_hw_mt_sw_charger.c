#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/charging.h>
#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_gpio.h>
#include <mach/upmu_hw.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>
#include <cust_gpio_usage.h>
#include <cust_charging.h>
#include <cust_pmic.h>

/* ============================================================ // */
/* Define */
/* ============================================================ // */
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/* ============================================================ // */
/* Global variable */
/* ============================================================ // */

kal_bool chargin_hw_init_done = KAL_TRUE;

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
#define WIRELESS_CHARGER_EXIST_STATE 0

#if defined(GPIO_PWR_AVAIL_WLC)
kal_uint32 wireless_charger_gpio_number = GPIO_PWR_AVAIL_WLC;
#else
kal_uint32 wireless_charger_gpio_number = 0;
#endif

#endif

static CHARGER_TYPE g_charger_type = CHARGER_UNKNOWN;

kal_bool charging_type_det_done = KAL_TRUE;

const kal_uint32 VBAT_CV_VTH[] = {
	BATTERY_VOLT_04_600000_V, BATTERY_VOLT_04_550000_V, BATTERY_VOLT_04_500000_V,
	BATTERY_VOLT_04_450000_V,
	BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_350000_V, BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_250000_V,
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_150000_V, BATTERY_VOLT_04_100000_V,
	BATTERY_VOLT_04_050000_V,
	BATTERY_VOLT_04_000000_V, BATTERY_VOLT_03_950000_V, BATTERY_VOLT_03_900000_V,
	BATTERY_VOLT_03_850000_V,
	BATTERY_VOLT_03_800000_V, BATTERY_VOLT_03_750000_V, BATTERY_VOLT_03_700000_V,
	BATTERY_VOLT_03_650000_V,
	BATTERY_VOLT_03_600000_V, BATTERY_VOLT_03_550000_V, BATTERY_VOLT_03_500000_V,
	BATTERY_VOLT_03_450000_V,
	BATTERY_VOLT_03_400000_V, BATTERY_VOLT_03_350000_V, BATTERY_VOLT_03_300000_V
};

const kal_uint32 CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA, CHARGE_CURRENT_125_00_MA, CHARGE_CURRENT_200_00_MA,
	CHARGE_CURRENT_300_00_MA,
	CHARGE_CURRENT_450_00_MA, CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_600_00_MA,
	CHARGE_CURRENT_700_00_MA,
	CHARGE_CURRENT_800_00_MA, CHARGE_CURRENT_900_00_MA, CHARGE_CURRENT_1000_00_MA,
	CHARGE_CURRENT_1100_00_MA,
	CHARGE_CURRENT_1200_00_MA, CHARGE_CURRENT_1300_00_MA, CHARGE_CURRENT_1400_00_MA,
	CHARGE_CURRENT_1500_00_MA,
	CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1700_00_MA, CHARGE_CURRENT_1800_00_MA,
	CHARGE_CURRENT_1900_00_MA,
	CHARGE_CURRENT_2000_00_MA, CHARGE_CURRENT_2100_00_MA, CHARGE_CURRENT_2200_00_MA,
	CHARGE_CURRENT_2300_00_MA,
	CHARGE_CURRENT_2400_00_MA, CHARGE_CURRENT_2500_00_MA, CHARGE_CURRENT_2600_00_MA,
	CHARGE_CURRENT_2700_00_MA,
	CHARGE_CURRENT_2800_00_MA, CHARGE_CURRENT_2900_00_MA, CHARGE_CURRENT_3000_00_MA,
	CHARGE_CURRENT_3100_00_MA,
	CHARGE_CURRENT_MAX
};

/* USB connector (USB or AC adaptor) */
const kal_uint32 INPUT_CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA, CHARGE_CURRENT_125_00_MA, CHARGE_CURRENT_200_00_MA,
	CHARGE_CURRENT_300_00_MA,
	CHARGE_CURRENT_450_00_MA, CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_600_00_MA,
	CHARGE_CURRENT_700_00_MA,
	CHARGE_CURRENT_800_00_MA, CHARGE_CURRENT_900_00_MA, CHARGE_CURRENT_1000_00_MA,
	CHARGE_CURRENT_1100_00_MA,
	CHARGE_CURRENT_1200_00_MA, CHARGE_CURRENT_1300_00_MA, CHARGE_CURRENT_1400_00_MA,
	CHARGE_CURRENT_1500_00_MA,
	CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1700_00_MA, CHARGE_CURRENT_1800_00_MA,
	CHARGE_CURRENT_1900_00_MA,
	CHARGE_CURRENT_2000_00_MA, CHARGE_CURRENT_2100_00_MA, CHARGE_CURRENT_2200_00_MA,
	CHARGE_CURRENT_2300_00_MA,
	CHARGE_CURRENT_2400_00_MA, CHARGE_CURRENT_2500_00_MA, CHARGE_CURRENT_2600_00_MA,
	CHARGE_CURRENT_2700_00_MA,
	CHARGE_CURRENT_2800_00_MA, CHARGE_CURRENT_2900_00_MA, CHARGE_CURRENT_3000_00_MA,
	CHARGE_CURRENT_3100_00_MA,
	CHARGE_CURRENT_MAX
};

const kal_uint32 VCDT_HV_VTH[] = {
	/* HW Fixed value */
};

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */

/* ============================================================ // */
/* extern variable */
/* ============================================================ // */

/* ============================================================ // */
/* extern function */
/* ============================================================ // */
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charging_get_charger_type(void);
extern unsigned int get_pmic_mt6332_cid(void);
extern void mt_power_off(void);

/* ============================================================ // */
kal_uint32 charging_value_to_parameter(const kal_uint32 *parameter, const kal_uint32 array_size,
				       const kal_uint32 val)
{
	if (val < array_size) {
		return parameter[val];
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "Can't find the parameter \r\n");
		return parameter[0];
	}
}


kal_uint32 charging_parameter_to_value(const kal_uint32 *parameter, const kal_uint32 array_size,
				       const kal_uint32 val)
{
	kal_uint32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i)) {
			return i;
		}
	}

	battery_xlog_printk(BAT_LOG_CRTI, "NO register value match \r\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}


static kal_uint32 bmt_find_closest_level(const kal_uint32 *pList, kal_uint32 number,
					 kal_uint32 level)
{
	kal_uint32 i;
	kal_uint32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		for (i = (number - 1); i != 0; i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				return pList[i];
			}
		}

		battery_xlog_printk(BAT_LOG_CRTI,
				    "Can't find closest level, small value first \r\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		for (i = 0; i < number; i++) {	/* max value in the first element */
			if (pList[i] <= level) {
				return pList[i];
			}
		}

		battery_xlog_printk(BAT_LOG_CRTI,
				    "Can't find closest level, large value first \r\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}


static kal_uint32 is_chr_det(void)
{
	kal_uint32 val = 0;
	pmic_config_interface(0x10A, 0x1, 0xF, 8);
	pmic_config_interface(0x10A, 0x17, 0xFF, 0);
	pmic_read_interface(0x108, &val, 0x1, 1);

	battery_xlog_printk(BAT_LOG_CRTI, "[is_chr_det] %d\n", val);

	return val;
}


static void swchr_dump_register(void)
{
#if 1
	battery_xlog_printk(BAT_LOG_CRTI,
			    "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
			    0x8052, upmu_get_reg_value(0x8052), 0x80E0, upmu_get_reg_value(0x80E0),
			    0x807A, upmu_get_reg_value(0x807A), 0x8074, upmu_get_reg_value(0x8074),
			    0x8078, upmu_get_reg_value(0x8078), 0x803C, upmu_get_reg_value(0x803C),
			    0x803A, upmu_get_reg_value(0x803A), 0x804C, upmu_get_reg_value(0x804C),
			    0x806C, upmu_get_reg_value(0x806C), 0x8170, upmu_get_reg_value(0x8170),
			    0x8166, upmu_get_reg_value(0x8166), 0x8080, upmu_get_reg_value(0x8080),
			    0x8040, upmu_get_reg_value(0x8040), 0x8042, upmu_get_reg_value(0x8042),
			    0x8050, upmu_get_reg_value(0x8050), 0x805E, upmu_get_reg_value(0x805E)
	    );
#else
	kal_uint32 i = 0;

	battery_xlog_printk(BAT_LOG_CRTI, "[charging_dump_register] -------------------\n");

	for (i = 0x8034; i <= 0x8092; i += 0x2) {
		battery_xlog_printk(BAT_LOG_CRTI, "Reg[0x%x]=0x%x ", i, upmu_get_reg_value(i));
	}
	battery_xlog_printk(BAT_LOG_CRTI, "\n");

	battery_xlog_printk(BAT_LOG_CRTI, "Reg[0x%x]=0x%x\n", 0xE0, upmu_get_reg_value(0xE0));

	for (i = 0x8164; i <= 0x8178; i += 0x2) {
		battery_xlog_printk(BAT_LOG_CRTI, "Reg[0x%x]=0x%x ", i, upmu_get_reg_value(i));
	}
	battery_xlog_printk(BAT_LOG_CRTI, "\n");
#endif
}


static kal_uint32 charging_hw_init(void *data)
{
	kal_uint32 status = STATUS_OK;

	/* Reg[0x8166] */
	mt6332_upmu_set_rg_m3_oc_db_sel(0);	/* M3 OC debounce 0.8us, 1'b1-->4us */
	mt6332_upmu_set_rg_ovpfet_sw_db_off(0);	/* OC debounce 4us */
	/* mt6332_upmu_set_rg_ovpfet_sw_target(0);      // off2on needs 0.8us */
	mt6332_upmu_set_rg_ovpfet_sw_nowait(0);	/* disbale fast switch NO WAIT */
	mt6332_upmu_set_rg_ovpfet_pri_chg(0);	/* DCIN */
	mt6332_upmu_set_rg_ovpfet_sw_dis(0);	/* Enable switching function */
	/* mt6332_upmu_set_rg_ovpfet_sw_fast(0);        // disbale fast switch */

	/* Reg[0x8168] */
	mt6332_upmu_set_rg_ovpfet_db_target(7);	/* UV/OV debounce time= value*13.33us */
	mt6332_upmu_set_rg_ovpfet_2p5m_db_target(0xF);	/* UV/OV Fast switch debounce time= value*0.4us */

	/* Reg[0x806C] */
	mt6332_upmu_set_rg_ch_complete_det_off(0);	/* Enable charge complete detection */
	/* mt6332_upmu_set_rg_ch_complete_pwm_off(0);   // PWM oN after charge complete detected */
	/* mt6332_upmu_set_rg_ch_complete_m3_off(0);    // M3 on after charge complete detected */
	mt6332_upmu_set_rg_term_timer(0);	/* 10sec */

	/* Reg[0x8080] */
	/* mt6332_upmu_set_rg_ich_sfstr_step(0);        // ICH soft start step, 32us */
	mt6332_upmu_set_rg_cvvth_sfstr_step(0);	/* CV soft start step, 32us */

	/* Reg[0x8170] */
	mt6332_upmu_set_rg_vin_dpm_mode_off(0);	/* ON */
	/* mt6332_upmu_set_rg_adaptive_cv_mode_off(0);  // ON */
	mt6332_upmu_set_rg_thermal_reg_mode_off(0);	/* ON */
	mt6332_upmu_set_rg_force_dcin_pp(1);	/* Force DCIN support powerpath, 1'b0 --> DCIN support PP depend on BC12 */


/* From HQA/Tim, 20140206 */
	/* Reg[0x8074] */
	mt6332_upmu_set_rg_csbat_vsns(1);
	/* Reg[0x803C] */
	mt6332_upmu_set_rg_osc_trim(0x4);
	/* Reg[0x804C] */
	mt6332_upmu_set_rg_swchr_trev(0x80);
	/* Reg[0x806C] */
	mt6332_upmu_set_rg_ch_complete_pwm_off(1);
	mt6332_upmu_set_rg_ch_complete_m3_off(1);
	/* Reg[0x803A] */
	mt6332_upmu_set_rg_iterm_sel(0);
	mt6332_upmu_set_rg_ics_loop(0);
	mt6332_upmu_set_rg_hfdet_en(1);
	mt6332_upmu_set_rg_gdri_minoff_dis(1);
	mt6332_upmu_set_rg_cv_comprc(1);
	/* Reg[0x8170] */
	mt6332_upmu_set_rg_adaptive_cv_mode_off(1);
	/* Reg[0x8166] */
	mt6332_upmu_set_rg_ovpfet_sw_fast(1);
	mt6332_upmu_set_rg_ovpfet_sw_target(0x4);
	/* Reg[0x8080] */
	mt6332_upmu_set_rg_ich_sfstr_step(0x3);
	/* Reg[0x8040] */
	mt6332_upmu_set_rg_swchr_vrampcc(0x2);
	/* Reg[0x8042] */
	mt6332_upmu_set_rg_swchr_vrampslp(0xE);
	/* Reg[0x8050] */
	mt6332_upmu_set_rg_swchr_rccomp_tune(1);

	swchr_dump_register();

	return status;
}


static kal_uint32 charging_dump_register(void *data)
{
	kal_uint32 status = STATUS_OK;

	swchr_dump_register();

	return status;
}


static kal_uint32 charging_enable(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32 *) (data);
	kal_uint32 is_m3_en = 0;

	pmic_read_interface(0x805E, &is_m3_en, 0x1, 2);	/* RGS_M3_EN */
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_enable] read RGS_M3_EN=%d\n", is_m3_en);

	if (KAL_TRUE == enable) {
		if (is_m3_en == 1) {
			mt6332_upmu_set_rg_pwm_en(1);
			battery_xlog_printk(BAT_LOG_CRTI,
					    "[charging_enable] mt6332_upmu_set_rg_pwm_en(1)\n");
		}
	} else {
#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if (mt_usb_is_device())
#endif
		{
			if (is_m3_en == 1) {
				mt6332_upmu_set_rg_pwm_en(0);
				battery_xlog_printk(BAT_LOG_CRTI,
						    "[charging_enable] mt6332_upmu_set_rg_pwm_en(0)\n");
			}
		}
	}

	return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint16 register_value;

	register_value =
	    charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH),
					*(kal_uint32 *) (data));

	/* --------------------------------- */
#if 0				/* move to swchr_flow */
	mt6332_upmu_set_rg_cv_sel(register_value);
	mt6332_upmu_set_rg_cv_pp_sel(register_value);

	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON11, upmu_get_reg_value(MT6332_CHR_CON11));
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON13, upmu_get_reg_value(MT6332_CHR_CON13));
#endif

	return status;
}


static kal_uint32 charging_get_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 array_size;
	kal_uint32 reg_value;

	/* Get current level */
	array_size = GETARRAYNUM(CS_VTH);
	pmic_read_interface(MT6332_CHR_CON10, &reg_value, 0x1F, 0);

	*(kal_uint32 *) data = charging_value_to_parameter(CS_VTH, array_size, reg_value);

	return status;
}


static void swchr_flow_nromal(kal_uint32 chr_cur_val)
{
	/* set ICH */
	mt6332_upmu_set_rg_ich_sel_swen(1);
	mt6332_upmu_set_rg_ich_sel(chr_cur_val);

	/* set CV_VTH (ex=4.2) and RG_CV_PP_SEL (ex=4.3) */
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_nromal] HIGH_BATTERY_VOLTAGE_SUPPORT\n");
	mt6332_upmu_set_rg_cv_sel(0x5);	/* 4.35V */
	mt6332_upmu_set_rg_cv_pp_sel(0x3);	/* 4.45V */
#else
	mt6332_upmu_set_rg_cv_sel(0x8);	/* 4.2V */
	mt6332_upmu_set_rg_cv_pp_sel(0x6);	/* 4.3V */
#endif
}


static void swchr_flow_main(kal_uint32 chr_cur_val)
{
	kal_uint32 is_pwm_en = 0;
	kal_uint32 is_m3_en = 0;
	kal_uint32 is_charge_complete = 0;
	kal_uint32 is_auto_recharge = 0;
	kal_uint32 volt_pp_to_normal = 3400;
	kal_uint32 volt_vbat = 0;
	kal_uint32 reg_val = 0;

	pmic_read_interface(0x805E, &is_pwm_en, 0x1, 6);	/* RGS_PWM_EN */
	battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] read RGS_PWM_EN=%d\n", is_pwm_en);

	pmic_read_interface(0x805E, &is_m3_en, 0x1, 2);	/* RGS_M3_EN */
	battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] read RGS_M3_EN=%d\n", is_m3_en);

	/* Notmal mode charging, ex. CC and CV */
	if (is_m3_en == 1) {
		battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] m3_en==1\n");

		/* Notmal mode charging */
		swchr_flow_nromal(chr_cur_val);
	}
	/* Power path mode */
	else {
		battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] m3_en==0\n");

		/* set ICH */
		mt6332_upmu_set_rg_ich_sel_swen(1);
		mt6332_upmu_set_rg_ich_sel(chr_cur_val);

		/* set PRECC */
		mt6332_upmu_set_rg_iprecc_swen(1);
		mt6332_upmu_set_rg_iprecc(0x3);	/* 450mA */

		/* monitor VBAT */
		volt_vbat = PMIC_IMM_GetOneChannelValue(AUX_BATSNS_AP, 2, 1);
		if (volt_vbat >= volt_pp_to_normal) {
			/* set CV_VTH=Vsys=3.4V */
			mt6332_upmu_set_rg_cv_sel(0x18);

			/* enable M3 */
			mt6332_upmu_set_rg_precc_m3_en(1);

			/* check M3 */
			pmic_read_interface(0x805E, &reg_val, 0x1, 2);	/* RGS_M3_EN */
			if (reg_val == 1) {
				battery_xlog_printk(BAT_LOG_CRTI,
						    "[swchr_flow_main] check m3_en==1 => OK\n");
				/* Notmal mode charging */
				swchr_flow_nromal(chr_cur_val);
			} else {
				battery_xlog_printk(BAT_LOG_CRTI,
						    "[swchr_flow_main] check m3_en==0 => FAIL\n");
			}

			/* turn off PRECC current */
			mt6332_upmu_set_rg_precc_en_swen(1);
			mt6332_upmu_set_rg_precc_en(0);
		}
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[swchr_flow_main] volt_vbat=%d, volt_pp_to_normal=%d\n",
				    volt_vbat, volt_pp_to_normal);
	}

	pmic_read_interface(0x805E, &is_charge_complete, 0x1, 10);	/* RGS_CHARGE_COMPLETE_DET */
	battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] read RGS_CHARGE_COMPLETE_DET=%d\n",
			    is_charge_complete);

	pmic_read_interface(0x805E, &is_auto_recharge, 0x1, 11);	/* RGS_AUTO_RECHARGE */
	battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] read RGS_AUTO_RECHARGE=%d\n",
			    is_auto_recharge);

	swchr_dump_register();
}


static kal_uint32 charging_set_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;
	kal_uint32 current_value = *(kal_uint32 *) data;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);

	/* --------------------------------- */
#if 0
	mt6332_upmu_set_rg_ich_sel_swen(1);
	mt6332_upmu_set_rg_ich_sel(register_value);

	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON8, upmu_get_reg_value(MT6332_CHR_CON8));
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON10, upmu_get_reg_value(MT6332_CHR_CON10));
#else
	swchr_flow_main(register_value);
#endif

	return status;
}


static kal_uint32 charging_set_input_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;
	kal_uint32 current_value = *(kal_uint32 *) data;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);

	/* --------------------------------- */
#if 0
	mt6332_upmu_set_rg_ich_sel_swen(1);
	mt6332_upmu_set_rg_ich_sel(register_value);

	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_input_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON8, upmu_get_reg_value(MT6332_CHR_CON8));
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_input_current] Reg[0x%x]=0x%x\n",
			    MT6332_CHR_CON10, upmu_get_reg_value(MT6332_CHR_CON10));
#else
	/* swchr_flow_main(register_value); //do at charging_set_current */
	battery_xlog_printk(BAT_LOG_CRTI,
			    "[charging_set_input_current] HW exec at charging_set_current\n");
#endif

	return status;
}


static kal_uint32 charging_get_charging_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 ret_val = 0;

	pmic_read_interface(0x805E, &ret_val, 0x1, 10);	/* RGS_CHARGE_COMPLETE_DET */
	battery_xlog_printk(BAT_LOG_CRTI,
			    "[charging_get_charging_status] read RGS_CHARGE_COMPLETE_DET=%d\n",
			    ret_val);

	if (ret_val == 0x1)
		*(kal_uint32 *) data = KAL_TRUE;
	else
		*(kal_uint32 *) data = KAL_FALSE;

	return status;
}


static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
	kal_uint32 status = STATUS_OK;

	mt6332_upmu_set_rg_chrwdt_en(1);
	mt6332_upmu_set_rg_chrwdt_wr(1);

	/* battery_xlog_printk(BAT_LOG_CRTI,"[charging_reset_watch_dog_timer] Reg[0x%x]=0x%x ", 0x80E0, upmu_get_reg_value(0x80E0)); */
	/* battery_xlog_printk(BAT_LOG_CRTI,"[charging_reset_watch_dog_timer] Reg[0x%x]=0x%x ", 0x80E2, upmu_get_reg_value(0x80E2)); */

	return status;
}


static kal_uint32 charging_set_hv_threshold(void *data)
{
	kal_uint32 status = STATUS_OK;

	/* HW Fixed value */

	return status;
}


static kal_uint32 charging_get_hv_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 val = 0;

#if defined(CONFIG_POWER_EXT)
	*(kal_bool *) (data) = 0;
#else
	if (get_pmic_mt6332_cid() == PMIC6332_E1_CID_CODE) {
		*(kal_bool *) (data) = 0;
	} else {
		val = mt6332_upmu_get_rgs_chr_hv_det();
		*(kal_bool *) (data) = val;
	}
#endif

	if (val == 1) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[charging_get_hv_status] HV detected by HW (%d)\n", val);
	}

	return status;
}


static kal_uint32 charging_get_battery_status(void *data)
{
	kal_uint32 status = STATUS_OK;

#if 0
	/* upmu_set_baton_tdet_en(1); */
	/* upmu_set_rg_baton_en(1); */
	/* *(kal_bool*)(data) = upmu_get_rgs_baton_undet(); */
	*(kal_bool *) (data) = 0;	/* battery exist */
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_battery_status] no HW function\n");
#else
	kal_uint32 ret = 0;

	pmic_config_interface(MT6332_BATON_CON0, 0x1, MT6332_PMIC_RG_BATON_EN_MASK,
			      MT6332_PMIC_RG_BATON_EN_SHIFT);
	pmic_config_interface(MT6332_TOP_CKPDN_CON0_CLR, 0x80C0, 0xFFFF, 0);	/* enable BIF clock */
	pmic_config_interface(MT6332_LDO_CON2, 0x1, MT6332_PMIC_RG_VBIF28_EN_MASK,
			      MT6332_PMIC_RG_VBIF28_EN_SHIFT);

	mdelay(1);
	ret = mt6332_upmu_get_bif_bat_lost();
	if (ret == 0) {
		*(kal_bool *) (data) = 0;	/* battery exist */
		battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_battery_status] battery exist.\n");
	} else {
		*(kal_bool *) (data) = 1;	/* battery NOT exist */
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[charging_get_battery_status] battery NOT exist.\n");
	}
#endif

	return status;
}


static kal_uint32 charging_get_charger_det_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	int is_valid_charger = 0;

#if 1
	kal_uint32 val = 0;
	pmic_config_interface(0x10A, 0x1, 0xF, 8);
	pmic_config_interface(0x10A, 0x17, 0xFF, 0);
	pmic_read_interface(0x108, &val, 0x1, 1);
	*(kal_bool *) (data) = val;
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_charger_det_status] CHRDET status = %d\n",
			    val);
#else
	/* *(kal_bool*)(data) = upmu_get_rgs_chrdet(); */
	*(kal_bool *) (data) = 1;
	battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_charger_det_status] no HW function\n");
#endif

	if (val == 0)
		g_charger_type = CHARGER_UNKNOWN;

	/* ------------------------------ */
	if (val == 1) {
		pmic_read_interface(0x8178, &is_valid_charger, 0x1, 11);
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[charging_get_charger_det_status] is_valid_charger = %d\n",
				    is_valid_charger);

		if (is_valid_charger == 0) {
			/* need check */
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x805E, upmu_get_reg_value(0x805E));
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x8056, upmu_get_reg_value(0x8056));
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x80E2, upmu_get_reg_value(0x80E2));
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x8062, upmu_get_reg_value(0x8062));
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x8178, upmu_get_reg_value(0x8178));
			battery_xlog_printk(BAT_LOG_CRTI, "[check invalid flag] Reg[0x%x]=0x%x\n",
					    0x8054, upmu_get_reg_value(0x8054));
		}
	}

	return status;
}


kal_bool charging_type_detection_done(void)
{
	return charging_type_det_done;
}


static kal_uint32 charging_get_charger_type(void *data)
{
	kal_uint32 status = STATUS_OK;
#if defined(CONFIG_POWER_EXT)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
	int wireless_state = 0;
	if (wireless_charger_gpio_number != 0) {
		wireless_state = mt_get_gpio_in(wireless_charger_gpio_number);
		if (wireless_state == WIRELESS_CHARGER_EXIST_STATE) {
			*(CHARGER_TYPE *) (data) = WIRELESS_CHARGER;
			battery_xlog_printk(BAT_LOG_CRTI, "WIRELESS_CHARGER!\n");
			return status;
		}
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "wireless_charger_gpio_number=%d\n",
				    wireless_charger_gpio_number);
	}

	if (g_charger_type != CHARGER_UNKNOWN && g_charger_type != WIRELESS_CHARGER) {
		*(CHARGER_TYPE *) (data) = g_charger_type;
		battery_xlog_printk(BAT_LOG_CRTI, "return %d!\n", g_charger_type);
		return status;
	}
#endif

	if (is_chr_det() == 0) {
		g_charger_type = CHARGER_UNKNOWN;
		*(CHARGER_TYPE *) (data) = CHARGER_UNKNOWN;
		battery_xlog_printk(BAT_LOG_CRTI,
				    "[charging_get_charger_type] return CHARGER_UNKNOWN\n");
		return status;
	}

	charging_type_det_done = KAL_FALSE;

	*(CHARGER_TYPE *) (data) = hw_charging_get_charger_type();

	charging_type_det_done = KAL_TRUE;

	g_charger_type = *(CHARGER_TYPE *) (data);

#endif
	return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
	kal_uint32 status = STATUS_OK;

#if 1
	if (slp_get_wake_reason() == WR_PCM_TIMER)
		*(kal_bool *) (data) = KAL_TRUE;
	else
		*(kal_bool *) (data) = KAL_FALSE;

	battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
#else
	*(kal_bool *) (data) = KAL_FALSE;
#endif

	return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");

	arch_reset(0, NULL);

	return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_uint32 *) (data) = get_boot_mode();

	battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static kal_uint32 charging_set_power_off(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_power_off=%d\n");
	mt_power_off();

	return status;
}

static kal_uint32(*const charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
charging_hw_init, charging_dump_register, charging_enable, charging_set_cv_voltage,
	    charging_get_current, charging_set_current, charging_set_input_current,
	    charging_get_charging_status, charging_reset_watch_dog_timer,
	    charging_set_hv_threshold, charging_get_hv_status, charging_get_battery_status,
	    charging_get_charger_det_status, charging_get_charger_type,
	    charging_get_is_pcm_timer_trigger, charging_set_platform_reset,
	    charging_get_platfrom_boot_mode, charging_set_power_off};


/*
* FUNCTION
*        Internal_chr_control_handler
*
* DESCRIPTION
*         This function is called to set the charger hw
*
* CALLS
*
* PARAMETERS
*        None
*
* RETURNS
*
*
* GLOBALS AFFECTED
*       None
*/
kal_int32 chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	kal_int32 status;
	if (cmd < CHARGING_CMD_NUMBER)
		status = charging_func[cmd] (data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}
