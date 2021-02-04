#ifndef _MT_PMIC_UPMU_SW_H_
#define _MT_PMIC_UPMU_SW_H_

#include <mach/pmic_mt6331_6332_sw.h>

/* ============================================================================== */
/* Low battery level define */
/* ============================================================================== */
typedef enum LOW_BATTERY_LEVEL_TAG {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
} LOW_BATTERY_LEVEL;

typedef enum LOW_BATTERY_PRIO_TAG {
	LOW_BATTERY_PRIO_CPU = 0,
	LOW_BATTERY_PRIO_MD = 1,
	LOW_BATTERY_PRIO_FLASHLIGHT = 2,
	LOW_BATTERY_PRIO_BACKLIGHT = 3
} LOW_BATTERY_PRIO;

extern void (*low_battery_callback) (LOW_BATTERY_LEVEL);
extern void register_low_battery_notify(void (*low_battery_callback) (LOW_BATTERY_LEVEL),
					LOW_BATTERY_PRIO prio_val);

/*==============================================================================
   Battery OC level define
============================================================================== */
typedef enum BATTERY_OC_LEVEL_TAG {
	BATTERY_OC_LEVEL_0 = 0,
	BATTERY_OC_LEVEL_1 = 1
} BATTERY_OC_LEVEL;

typedef enum BATTERY_OC_PRIO_TAG {
	BATTERY_OC_PRIO_CPU_B = 0,
	BATTERY_OC_PRIO_CPU_L = 1,
	BATTERY_OC_PRIO_GPU = 2
} BATTERY_OC_PRIO;

extern void (*battery_oc_callback) (BATTERY_OC_LEVEL);
extern void register_battery_oc_notify(void (*battery_oc_callback) (BATTERY_OC_LEVEL),
				       BATTERY_OC_PRIO prio_val);

/*==============================================================================
   Battery percent define
============================================================================== */
typedef enum BATTERY_PERCENT_LEVEL_TAG {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
} BATTERY_PERCENT_LEVEL;

typedef enum BATTERY_PERCENT_PRIO_TAG {
	BATTERY_PERCENT_PRIO_CPU_B = 0,
	BATTERY_PERCENT_PRIO_CPU_L = 1,
	BATTERY_PERCENT_PRIO_GPU = 2,
	BATTERY_PERCENT_PRIO_MD = 3,
	BATTERY_PERCENT_PRIO_MD5 = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO = 6,
	BATTERY_PERCENT_PRIO_WIFI = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT = 8
} BATTERY_PERCENT_PRIO;

extern void (*battery_percent_callback) (BATTERY_PERCENT_LEVEL);
extern void register_battery_percent_notify(void (*battery_percent_callback) (BATTERY_PERCENT_LEVEL)
					    , BATTERY_PERCENT_PRIO prio_val);


extern int Enable_BATDRV_LOG;

/* ============================================================================== */
/* Extern */
/* ============================================================================== */
extern void pmu_drv_tool_customization_init(void);
extern void pmic_auxadc_init(void);
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
extern void mt_power_off(void);
#endif

extern kal_uint32 bat_get_ui_percentage(void);

extern void kpd_pwrkey_pmic_handler(unsigned long pressed);

extern void kpd_pmic_rstkey_handler(unsigned long pressed);

#if defined(CONFIG_MTK_ACCDET)
extern int accdet_irq_handler(void);
#endif

#ifdef MTK_PMIC_DVT_SUPPORT
extern void mt6332_bat_int_close(void);
#endif

#ifdef MTK_PMIC_DVT_SUPPORT
extern void tc_bif_1008_step_1(void);	/* DVT */
#endif

#ifdef CONFIG_MTK_BQ24160_SUPPORT
extern int is_bq24160_exist(void);
#endif

extern int is_tps6128x_sw_ready(void);
extern int is_tps6128x_exist(void);

extern int is_da9210_sw_ready(void);
extern int is_da9210_exist(void);
extern int da9210_vosel(unsigned long val);
extern int get_da9210_i2c_ch_num(void);



#endif				/* _MT_PMIC_UPMU_SW_H_ */
