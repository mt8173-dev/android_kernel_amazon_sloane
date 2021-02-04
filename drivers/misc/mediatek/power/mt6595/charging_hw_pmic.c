#include <mach/charging.h>
#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>

#include "cust_battery_meter.h"
#include <cust_charging.h>
#include <cust_pmic.h>

 /* ============================================================ // */
 /* define */
 /* ============================================================ // */
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))


 /* ============================================================ // */
 /* global variable */
 /* ============================================================ // */
kal_bool chargin_hw_init_done = KAL_TRUE;
kal_bool charging_type_det_done = KAL_TRUE;

const kal_uint32 VBAT_CV_VTH[] = {
	BATTERY_VOLT_04_200000_V
};

const kal_uint32 CS_VTH[] = {
	CHARGE_CURRENT_450_00_MA
};


const kal_uint32 VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_200000_V
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
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);
extern int hw_charging_get_charger_type(void);

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

		battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level \r\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		for (i = 0; i < number; i++) {	/* max value in the first element */
			if (pList[i] <= level) {
				return pList[i];
			}
		}

		battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level \r\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}

static kal_uint32 charging_hw_init(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
}


static kal_uint32 charging_dump_register(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
}


static kal_uint32 charging_enable(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	kal_uint32 enable = *(kal_uint32 *) (data);

	if (KAL_TRUE == enable) {
		/* HW not support */
	} else {
		/* HW not support */
	}
#endif

	return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
}


static kal_uint32 charging_get_current(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
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

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
}


static kal_uint32 charging_set_input_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	return status;
}

static kal_uint32 charging_get_charging_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	return status;
}


static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

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

#if defined(CONFIG_MTK_FPGA)
#else
	/* HW not support */
#endif

	return status;
}


static kal_uint32 charging_get_charger_det_status(void *data)
{
	kal_uint32 status = STATUS_OK;

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

	return status;
}


kal_bool charging_type_detection_done(void)
{
	return charging_type_det_done;
}


static kal_uint32 charging_get_charger_type(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#endif

	return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	if (slp_get_wake_reason() == WR_PCM_TIMER)
		*(kal_bool *) (data) = KAL_TRUE;
	else
		*(kal_bool *) (data) = KAL_FALSE;

	battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
#endif

	return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");

	arch_reset(0, NULL);
#endif

	return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
	kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
#else
	*(kal_uint32 *) (data) = get_boot_mode();

	battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
#endif

	return status;
}

static kal_uint32 charging_set_power_off(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_power_off=%d\n");
	/* mt_power_off(); */

	return status;
}

static kal_uint32(*charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
	charging_hw_init, charging_dump_register, charging_enable, charging_set_cv_voltage, charging_get_current, charging_set_current, charging_set_input_current	/* not support, empty function */
	    , charging_get_charging_status	/* not support, empty function */
, charging_reset_watch_dog_timer, charging_set_hv_threshold, charging_get_hv_status,
	    charging_get_battery_status, charging_get_charger_det_status,
	    charging_get_charger_type, charging_get_is_pcm_timer_trigger,
	    charging_set_platform_reset, charging_get_platfrom_boot_mode, charging_set_power_off};


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
