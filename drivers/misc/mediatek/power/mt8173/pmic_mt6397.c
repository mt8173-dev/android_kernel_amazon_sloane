/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic_mt6397.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines PMIC functions
 *
 * Author:
 * -------
 * James Lo
 *
 ****************************************************************************/
#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/syscore_ops.h>
#include <linux/mfd/mt6397/core.h>
#include <mach/pmic_mt6397_sw.h>
#include <drivers/misc/mediatek/power/mt8173/upmu_common.h>
#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_gpio_def.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <linux/power/mt_charging.h>
#include <linux/power/mt_battery_common.h>
#include <linux/power/mt_battery_meter.h>

#include <mtk_kpd.h>

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mach/mt_boot.h>
#include <mach/system.h>
#include "mach/mt_gpt.h"
#endif


#include <drivers/misc/mediatek/power/mt8173/da9212.h>

#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
extern void vMhlTriggerIntTask(void);
#endif

#include <mach/board.h>
#include <mach/mt_pmic_irq.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

#include <linux/gpio.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

/* #define CONFIG_PMIC_IMPLEMENT_UNUSED_EVENT_HANDLERS */

/* ============================================================================== */
/* PMIC related define */
/* ============================================================================== */
#define VOLTAGE_FULL_RANGE     1200
#define ADC_PRECISE         1024	/* 10 bits */
#define TEST_PMIC_PRINT     _IO('k', 0)
#define PMIC_READ           _IOW('k', 1, int)
#define PMIC_WRITE          _IOW('k', 2, int)
#define PMIC_DCXO_WRITE     _IOW('k', 3, int)

static DEFINE_MUTEX(pmic_lock_mutex);
static DEFINE_MUTEX(pmic_adc_mutex);
static DEFINE_MUTEX(pmic_access_mutex);
static DEFINE_SPINLOCK(pmic_smp_spinlock);


struct mt6397_event_stat {
	u64 last;
	int count;
	bool blocked:1;
	bool wake_blocked:1;
};

static struct mt_wake_event mt6397_event = {
	.domain = "PMIC"
};

static struct mt_wake_event_map pwrkey_wev = {
	.domain = "PMIC",
	.code = RG_INT_STATUS_PWRKEY,
	.we = WEV_PWR,
	.irq = PMIC_IRQ(RG_INT_STATUS_PWRKEY)
};

static struct mt_wake_event_map rtc_wev = {
	.domain = "PMIC",
	.code = RG_INT_STATUS_RTC,
	.we = WEV_RTC,
	.irq = PMIC_IRQ(RG_INT_STATUS_RTC)
};
static struct mt_wake_event_map charger_wev = {
	.domain = "PMIC",
	.code = RG_INT_STATUS_CHRDET,
	.we = WEV_CHARGER,
	.irq = PMIC_IRQ(RG_INT_STATUS_CHRDET)
};

struct mt6397_chip_priv {
	struct device *dev;
	struct irq_domain *domain;
	unsigned long event_mask;
	unsigned long wake_mask;
	u32 saved_mask;
	u16 int_con[2];
	u16 int_stat[2];
	int irq;
	int irq_base;
	int num_int;
	int irq_hw_id;
	bool suspended:1;
	u32 wakeup_event;
	struct mt6397_event_stat stat[32];
};

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static kal_bool long_pwrkey_press;
static unsigned long timer_pre;
static unsigned long timer_pos;
#define LONG_PWRKEY_PRESS_TIME		(500*1000000)	/* 500ms */
#endif

static struct mt6397_chip_priv *mt6397_chip;

/* ============================================================================== */
/* PMIC lock/unlock APIs */
/* ============================================================================== */
void pmic_lock(void)
{
	mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
	mutex_unlock(&pmic_lock_mutex);
}

void pmic_smp_lock(void)
{
	spin_lock(&pmic_smp_spinlock);
}

void pmic_smp_unlock(void)
{
	spin_unlock(&pmic_smp_spinlock);
}

kal_uint32 upmu_get_reg_value(kal_uint32 reg)
{
	U32 ret = 0;
	U32 reg_val = 0;

	/* pr_info("[upmu_get_reg_value]\n"); */
	ret = pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}

void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val)
{
	U32 ret = 0;

	/* pr_info("[upmu_set_reg_value]\n"); */
	ret = pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

/* ============================================================================== */
/* PMIC-AUXADC */
/* ============================================================================== */

int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd)
{
	kal_int32 u4Sample_times = 0;
	kal_int32 u4channel[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	kal_int32 adc_result = 0;
	kal_int32 adc_result_temp = 0;
	kal_int32 r_val_temp = 0;
	kal_int32 count = 0;
	kal_int32 count_time_out = 1000;
	kal_int32 ret_data = 0;

	mutex_lock(&pmic_adc_mutex);

	if (dwChannel == 1) {
		upmu_set_reg_value(0x0020, 0x0801);
		upmu_set_rg_source_ch0_norm_sel(1);
		upmu_set_rg_source_ch0_lbat_sel(1);
		dwChannel = 0;
		mdelay(1);
	}

	/*
	   0 : V_BAT
	   1 : V_I_Sense
	   2 : V_Charger
	   3 : V_TBAT
	   4~7 : reserved
	 */
	upmu_set_rg_auxadc_chsel(dwChannel);

	/* upmu_set_rg_avg_num(0x3); */

	if (dwChannel == 3) {
		upmu_set_rg_buf_pwd_on(1);
		upmu_set_rg_buf_pwd_b(1);
		upmu_set_baton_tdet_en(1);
		msleep(20);
	}

	if (dwChannel == 4) {
		upmu_set_rg_vbuf_en(0);
		upmu_set_rg_vbuf_byp(1);

		if (trimd == 2) {
			upmu_set_rg_vbuf_calen(0);	/* For T_PMIC */
			upmu_set_rg_spl_num(0x10);
			/* upmu_set_rg_spl_num(0x1E); */
			/* upmu_set_rg_avg_num(0x6); */
			trimd = 1;
		} else {
			upmu_set_rg_vbuf_calen(1);	/* For T_BAT */
		}
	}
	if (dwChannel == 5) {
#ifdef CONFIG_MTK_ACCDET
		accdet_auxadc_switch(1);
#endif
	}
	u4Sample_times = 0;

	do {
		upmu_set_rg_auxadc_start(0);
		upmu_set_rg_auxadc_start(1);

		/* Duo to HW limitation */
		udelay(30);

		count = 0;
		ret_data = 0;

		switch (dwChannel) {
		case 0:
			while (upmu_get_rg_adc_rdy_c0() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c0_trim();
			else
				ret_data = upmu_get_rg_adc_out_c0();

			break;
		case 1:
			while (upmu_get_rg_adc_rdy_c1() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c1_trim();
			else
				ret_data = upmu_get_rg_adc_out_c1();

			break;
		case 2:
			while (upmu_get_rg_adc_rdy_c2() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c2_trim();
			else
				ret_data = upmu_get_rg_adc_out_c2();

			break;
		case 3:
			while (upmu_get_rg_adc_rdy_c3() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c3_trim();
			else
				ret_data = upmu_get_rg_adc_out_c3();

			break;
		case 4:
			while (upmu_get_rg_adc_rdy_c4() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c4_trim();
			else
				ret_data = upmu_get_rg_adc_out_c4();

			break;
		case 5:
			while (upmu_get_rg_adc_rdy_c5() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c5_trim();
			else
				ret_data = upmu_get_rg_adc_out_c5();

			break;
		case 6:
			while (upmu_get_rg_adc_rdy_c6() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c6_trim();
			else
				ret_data = upmu_get_rg_adc_out_c6();

			break;
		case 7:
			while (upmu_get_rg_adc_rdy_c7() != 1) {
				if ((count++) > count_time_out) {
					pr_debug("[Power/PMIC]" "[IMM_GetOneChannelValue_PMIC] "
						 "(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c7_trim();
			else
				ret_data = upmu_get_rg_adc_out_c7();

			break;
		default:
			pr_info("[Power/PMIC][AUXADC] " "Invalid channel value(%d,%d)\n", dwChannel, trimd);
			mutex_unlock(&pmic_adc_mutex);
			return -1;
		}

		u4channel[dwChannel] += ret_data;

		u4Sample_times++;

		if (Enable_BATDRV_LOG == 1) {
			/* debug */
			pr_debug("[Power/PMIC][AUXADC] " "u4Sample_times=%d, ret_data=%d, " "u4channel[%d]=%d.\n",
				 u4Sample_times, ret_data, dwChannel, u4channel[dwChannel]);
		}

	} while (u4Sample_times < deCount);

	/* Value averaging  */
	u4channel[dwChannel] = u4channel[dwChannel] / deCount;
	adc_result_temp = u4channel[dwChannel];

	switch (dwChannel) {

	case 0:
		r_val_temp = g_R_BAT_SENSE;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 1:
		r_val_temp = g_R_I_SENSE;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 2:
		r_val_temp = (((g_R_CHARGER_1 + g_R_CHARGER_2) * 100) / g_R_CHARGER_2);
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 3:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 4:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 5:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 6:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 7:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	default:
		pr_info("[Power/PMIC][AUXADC] " "Invalid channel value(%d,%d)\n", dwChannel, trimd);
		mutex_unlock(&pmic_adc_mutex);
		return -1;
	}

	if (Enable_BATDRV_LOG == 1) {
		/* debug */
		pr_debug("[Power/PMIC][AUXADC] " "adc_result_temp=%d, adc_result=%d, r_val_temp=%d.\n",
			 adc_result_temp, adc_result, r_val_temp);
	}

	count = 0;

	if (dwChannel == 0) {
		upmu_set_rg_source_ch0_norm_sel(0);
		upmu_set_rg_source_ch0_lbat_sel(0);
	}

	if (dwChannel == 3) {
		upmu_set_baton_tdet_en(0);
		upmu_set_rg_buf_pwd_b(0);
		upmu_set_rg_buf_pwd_on(0);
	}

	if (dwChannel == 4) {
		/* upmu_set_rg_vbuf_en(0); */
		/* upmu_set_rg_vbuf_byp(0); */
		upmu_set_rg_vbuf_calen(0);
	}
	if (dwChannel == 5) {
#ifdef CONFIG_MTK_ACCDET
		accdet_auxadc_switch(0);
#endif
	}

	upmu_set_rg_spl_num(0x1);

	mutex_unlock(&pmic_adc_mutex);

	return adc_result;

}

/* ============================================================================== */
/* Low Battery Protect */
/* ============================================================================== */
#if defined(CONFIG_MTK_BATTERY_PROTECT)
#define LBCB_NUM 16

#define HV_THD   0x2D5		/* 3.4V  -> 0x2D5 */
#define LV_1_THD 0x2B5		/* 3.25V -> 0x2B5 */
#define LV_2_THD 0x298		/* 3.1V  -> 0x298 */

int g_low_battery_level = 0;
int g_low_battery_stop = 0;

struct low_battery_callback_table {
	void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};

void (*low_battery_callback) (LOW_BATTERY_LEVEL);

void register_low_battery_notify(void (*low_battery_callback) (LOW_BATTERY_LEVEL),
				 LOW_BATTERY_PRIO prio_val)
{
	pr_info("[Power/PMIC][register_low_battery_notify] start\n");

	lbcb_tb[prio_val].lbcb = low_battery_callback;

	pr_info("[Power/PMIC][register_low_battery_notify] prio_val=%d\n", prio_val);
}

void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level)
{				/* 0:no limit */
	int i = 0;

	if (g_low_battery_stop == 1) {
		pr_info("[Power/PMIC]" "[exec_low_battery_callback] g_low_battery_stop=%d\n", g_low_battery_stop);
	} else {
		for (i = 0; i < LBCB_NUM; i++) {
			if (lbcb_tb[i].lbcb != NULL) {
				low_battery_callback = lbcb_tb[i].lbcb;
				low_battery_callback(low_battery_level);
				pr_info("[Power/PMIC]" "[exec_low_battery_callback] prio_val=%d,low_battery=%d\n",
					i, low_battery_level);
			}
		}
	}
}

static inline void upmu_set_rg_lbat_en_min_nolock(U32 val)
{
	pmic_config_interface_nolock((U32) (AUXADC_CON6),
				     (U32) (val),
				     (U32) (PMIC_RG_LBAT_EN_MIN_MASK),
				     (U32) (PMIC_RG_LBAT_EN_MIN_SHIFT)
	    );
}

static inline void upmu_set_rg_lbat_irq_en_min_nolock(U32 val)
{
	pmic_config_interface_nolock((U32) (AUXADC_CON6),
				     (U32) (val),
				     (U32) (PMIC_RG_LBAT_IRQ_EN_MIN_MASK),
				     (U32) (PMIC_RG_LBAT_IRQ_EN_MIN_SHIFT)
	    );
}

static void upmu_set_rg_lbat_en_max_nolock(U32 val)
{
	pmic_config_interface_nolock((U32) (AUXADC_CON5),
				     (U32) (val),
				     (U32) (PMIC_RG_LBAT_EN_MAX_MASK),
				     (U32) (PMIC_RG_LBAT_EN_MAX_SHIFT)
	    );
}

static void upmu_set_rg_lbat_irq_en_max_nolock(U32 val)
{
	pmic_config_interface_nolock((U32) (AUXADC_CON5),
				     (U32) (val),
				     (U32) (PMIC_RG_LBAT_IRQ_EN_MAX_MASK),
				     (U32) (PMIC_RG_LBAT_IRQ_EN_MAX_SHIFT)
	    );
}

void lbat_min_en_setting_nolock(int en_val)
{
	upmu_set_rg_lbat_en_min_nolock(en_val);
	upmu_set_rg_lbat_irq_en_min_nolock(en_val);
}

void lbat_max_en_setting_nolock(int en_val)
{
	upmu_set_rg_lbat_en_max_nolock(en_val);
	upmu_set_rg_lbat_irq_en_max_nolock(en_val);
}

void lbat_min_en_setting(int en_val)
{
	upmu_set_rg_lbat_en_min(en_val);
	upmu_set_rg_lbat_irq_en_min(en_val);
}

void lbat_max_en_setting(int en_val)
{
	upmu_set_rg_lbat_en_max(en_val);
	upmu_set_rg_lbat_irq_en_max(en_val);
}

static irqreturn_t bat_l_int_handler(int irq, void *dev_id)
{
	pr_notice("@@@ %s:\n", __func__);

	g_low_battery_level++;
	if (g_low_battery_level > 2)
		g_low_battery_level = 2;

	if (g_low_battery_level == 1)
		exec_low_battery_callback(LOW_BATTERY_LEVEL_1);
	else if (g_low_battery_level == 2)
		exec_low_battery_callback(LOW_BATTERY_LEVEL_2);

	upmu_set_rg_lbat_volt_min(LV_2_THD);
	mdelay(1);

	pr_debug("[Power/PMIC] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", AUXADC_CON6,
		 upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5, upmu_get_reg_value(AUXADC_CON5));

	return IRQ_HANDLED;
}

static irqreturn_t bat_h_int_handler(int irq, void *dev_id)
{
	pr_notice("@@@ %s:\n", __func__);

	g_low_battery_level = 0;
	exec_low_battery_callback(LOW_BATTERY_LEVEL_0);

	upmu_set_rg_lbat_volt_min(LV_1_THD);
	mdelay(1);

	pr_debug("[Power/PMIC] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", AUXADC_CON6,
		 upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5, upmu_get_reg_value(AUXADC_CON5));

	return IRQ_HANDLED;
}

void low_battery_protect_enable(void)
{
	upmu_set_rg_lbat_volt_max(HV_THD);
	upmu_set_rg_lbat_volt_min(LV_1_THD);
	lbat_min_en_setting(1);
	lbat_max_en_setting(0);
}

void low_battery_protect_init(void)
{
	/* default setting */
	upmu_set_rg_lbat_debt_min(0);
	upmu_set_rg_lbat_debt_max(0);
	upmu_set_rg_lbat_det_prd_15_0(1);
	upmu_set_rg_lbat_det_prd_19_16(0);

	pr_debug("[Power/PMIC][low_battery_protect_init] Done\n");
}
#endif
/* ============================================================================== */
/* PMIC Interrupt service */
/* ============================================================================== */
int pmic_thread_timeout = 0;

#define WAKE_LOCK_INITIALIZED            (1U << 8)

#if defined(CONFIG_POWER_EXT)
/*
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
*/
#endif

/* mt6397 irq chip clear event status for given event mask. */
static void mt6397_ack_events_locked(struct mt6397_chip_priv *chip, u32 event_mask)
{
	U32 val;
	pwrap_write(chip->int_stat[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_stat[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_stat[0], &val);
	pwrap_read(chip->int_stat[1], &val);
}

/* mt6397 irq chip event read. */
static u32 mt6397_get_events(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	u32 events[2] = { 0 };

	pmic_lock();
	pwrap_read(chip->int_stat[0], &events[0]);
	pwrap_read(chip->int_stat[1], &events[1]);
	pmic_unlock();

	return (events[1] << 16) | (events[0] & 0xFFFF);
}

/* mt6397 irq chip event mask read: debugging only */
static u32 mt6397_get_event_mask_locked(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	u32 event_mask[2] = { 0 };

	pwrap_read(chip->int_con[0], &event_mask[0]);
	pwrap_read(chip->int_con[1], &event_mask[1]);

	return (event_mask[1] << 16) | (event_mask[0] & 0xFFFF);
}

static u32 mt6397_get_event_mask(struct mt6397_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	u32 res;

	pmic_lock();
	res = mt6397_get_event_mask_locked(chip);
	pmic_unlock();

	return res;
}

/* mt6397 irq chip event mask write: initial setup */
static void mt6397_set_event_mask_locked(struct mt6397_chip_priv *chip, u32 event_mask)
{
	U32 val;
	pwrap_write(chip->int_con[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_con[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_con[0], &val);
	pwrap_read(chip->int_con[1], &val);
	chip->event_mask = event_mask;
}

static void mt6397_set_event_mask(struct mt6397_chip_priv *chip, u32 event_mask)
{
	pmic_lock();
	mt6397_set_event_mask_locked(chip, event_mask);
	pmic_unlock();
}

/* this function is only called by generic IRQ framework, and it is always
 * called with pmic_lock held by IRQ framework. */
static void mt6397_irq_mask_unmask_locked(struct irq_data *d, bool enable)
{
	struct mt6397_chip_priv *mt_chip = d->chip_data;
	int hw_irq = d->hwirq;
	u16 port = (hw_irq >> 4) & 1;
	u32 val;

	if (enable)
		set_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);
	else
		clear_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);

	if (port) {
		pwrap_write(mt_chip->int_con[1], (mt_chip->event_mask >> 16) & 0xFFFF);
		pwrap_read(mt_chip->int_con[1], &val);
	} else {
		pwrap_write(mt_chip->int_con[0], mt_chip->event_mask & 0xFFFF);
		pwrap_read(mt_chip->int_con[0], &val);

#if defined(CONFIG_MTK_BATTERY_PROTECT)
		if (!enable) {
			if (hw_irq == RG_INT_STATUS_BAT_L)
				lbat_min_en_setting_nolock(0);
			else if (hw_irq == RG_INT_STATUS_BAT_H)
				lbat_max_en_setting_nolock(0);
		} else {
			if (hw_irq == RG_INT_STATUS_BAT_L) {
				/* toggle L_BAT to enable L_BAT */
				if (g_low_battery_level < 2) {
					lbat_min_en_setting_nolock(0);
					lbat_min_en_setting_nolock(1);
				}
				/* enable H_BAT when hit L_BAT */
				lbat_max_en_setting_nolock(0);
				lbat_max_en_setting_nolock(1);
			} else if (hw_irq == RG_INT_STATUS_BAT_H) {
				/* enable L_BAT when hit H_BAT */
				lbat_min_en_setting_nolock(0);
				lbat_min_en_setting_nolock(1);
			}
		}
#endif
	}
}

static void mt6397_irq_mask_locked(struct irq_data *d)
{
	mt6397_irq_mask_unmask_locked(d, false);
	mdelay(5);
}

static void mt6397_irq_unmask_locked(struct irq_data *d)
{
	mt6397_irq_mask_unmask_locked(d, true);
}

static void mt6397_irq_ack_locked(struct irq_data *d)
{
	struct mt6397_chip_priv *chip = irq_data_get_irq_chip_data(d);
	mt6397_ack_events_locked(chip, 1 << d->hwirq);
}


#ifdef CONFIG_PMIC_IMPLEMENT_UNUSED_EVENT_HANDLERS
static irqreturn_t pwrkey_rstb_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_int_en_pwrkey_rstb(0);

	return IRQ_HANDLED;
}

static irqreturn_t hdmi_sifm_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_int_en_hdmi_sifm(0);

#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
	vMhlTriggerIntTask();
#endif

	return IRQ_HANDLED;
}

static irqreturn_t hdmi_cec_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_int_en_hdmi_cec(0);

	return IRQ_HANDLED;
}

static irqreturn_t vsrmca15_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vsrmca15(0);

	return IRQ_HANDLED;
}

static irqreturn_t vcore_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vcore(0);

	return IRQ_HANDLED;
}

static irqreturn_t vio18_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vio18(0);

	return IRQ_HANDLED;
}

static irqreturn_t vpca7_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vpca7(0);

	return IRQ_HANDLED;
}

static irqreturn_t vsrmca7_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vsrmca7(0);

	return IRQ_HANDLED;
}

static irqreturn_t vdrm_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vdrm(0);

	return IRQ_HANDLED;
}

static irqreturn_t vca15_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vca15(0);

	return IRQ_HANDLED;
}

static irqreturn_t vgpu_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);
	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vgpu(0);

	return IRQ_HANDLED;
}

#endif


static irqreturn_t pwrkey_int_handler(int irq, void *dev_id)
{
	U32 pwrkey_deb = 0;

#ifdef CONFIG_AMAZON_METRICS_LOG
	char *action;
#define METRICS_STR_LEN 128
	char buf[METRICS_STR_LEN];
#undef METRICS_STR_LEN
#endif

	pr_info("%s:\n", __func__);

	pwrkey_deb = upmu_get_pwrkey_deb();

#ifdef CONFIG_AMAZON_METRICS_LOG
	action = (pwrkey_deb == 1) ? "release" : "press";
	sprintf(buf, "%s:powi%c:report_action_is_%s=1;CT;1:NR", __func__, action[0], action);
	log_to_metrics(ANDROID_LOG_INFO, "PowerKeyEvent", buf);
#endif

	if (pwrkey_deb == 1) {
		pr_info("[Power/PMIC]" "[pwrkey_int_handler] Release pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT) {
			timer_pos = sched_clock();
			if (timer_pos - timer_pre >= LONG_PWRKEY_PRESS_TIME)
				long_pwrkey_press = true;

			pr_info("[Power/PMIC]" "pos = %ld, pre = %ld, pos-pre = %ld, long_pwrkey_press = %d\r\n",
				timer_pos, timer_pre, timer_pos - timer_pre, long_pwrkey_press);
			if (long_pwrkey_press) {	/* 500ms */
				pr_info("[Power/PMIC]" "Pwrkey Pressed during kpoc, reboot OS\r\n");
				arch_reset(0, NULL);
			}
		}
#endif
		kpd_pwrkey_pmic_handler(0x0);
	} else {
		pr_info("[Power/PMIC][pwrkey_int_handler] Press pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
			timer_pre = sched_clock();

#endif
		kpd_pwrkey_pmic_handler(0x1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t homekey_int_handler(int irq, void *dev_id)
{
	pr_info("%s:\n", __func__);

	if (upmu_get_homekey_deb() == 1) {
		pr_info("[Power/PMIC]" "[homekey_int_handler] Release HomeKey\r\n");
		kpd_pmic_rstkey_handler(0x0);

	} else {
		pr_info("[Power/PMIC]" "[homekey_int_handler] Press HomeKey\r\n");

		kpd_pmic_rstkey_handler(0x1);

	}

	return IRQ_HANDLED;
}

static irqreturn_t rtc_int_handler(int irq, void *dev_id)
{
	rtc_irq_handler();
	msleep(100);

	return IRQ_HANDLED;
}

#ifdef CONFIG_MTK_ACCDET
static irqreturn_t accdet_int_handler(int irq, void *dev_id)
{
	kal_uint32 ret = 0;

	pr_info("%s:\n", __func__);

	ret = accdet_irq_handler();
	if (0 == ret)
		pr_info("[Power/PMIC]" "[accdet_int_handler] don't finished\n");
	return IRQ_HANDLED;
}
#endif

/* ============================================================================== */
/* PMIC read/write APIs */
/* ============================================================================== */
#define CONFIG_PMIC_HW_ACCESS_EN

#define PMIC_REG_NUM 0xFFFF

/* U32 pmic6320_reg[PMIC_REG_NUM] = {0}; */

U32 pmic_read_interface(U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
	U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	U32 pmic_reg = 0;
	U32 rdata;

	mutex_lock(&pmic_access_mutex);

	/* mt6320_read_byte(RegNum, &pmic6320_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);
	/* pr_debug("[Power/PMIC][pmic_read_interface] val=0x%x\n", *val); */

	mutex_unlock(&pmic_access_mutex);
#else
	pr_info("[Power/PMIC]" "[pmic_read_interface] Can not access HW PMIC\n");
#endif

	return return_value;
}

U32 pmic_config_interface(U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
	U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	U32 pmic_reg = 0;
	U32 rdata;

	mutex_lock(&pmic_access_mutex);

	/* 1. mt6320_read_byte(RegNum, &pmic_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	/* 2. mt6320_write_byte(RegNum, pmic_reg); */
	return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg); */

#if 0
	/* 3. Double Check */
	/* mt6320_read_byte(RegNum, &pmic_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	pr_debug("[Power/PMIC][pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
#endif

	mutex_unlock(&pmic_access_mutex);
#else
	pr_info("[Power/PMIC]" "[pmic_config_interface] Can not access HW PMIC\n");
#endif

	return return_value;
}

/* ============================================================================== */
/* PMIC read/write APIs : nolock */
/* ============================================================================== */
U32 pmic_read_interface_nolock(U32 RegNum, U32 *val, U32 MASK, U32 SHIFT)
{
	U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	U32 pmic_reg = 0;
	U32 rdata;

	/* mt6320_read_byte(RegNum, &pmic6320_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);
	/* pr_debug("[Power/PMIC][pmic_read_interface] val=0x%x\n", *val); */
#else
	pr_info("[Power/PMIC]" "[pmic_read_interface] Can not access HW PMIC\n");
#endif

	return return_value;
}

U32 pmic_config_interface_nolock(U32 RegNum, U32 val, U32 MASK, U32 SHIFT)
{
	U32 return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	U32 pmic_reg = 0;
	U32 rdata;

	/* 1. mt6320_read_byte(RegNum, &pmic_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	/* 2. mt6320_write_byte(RegNum, pmic_reg); */
	return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/* pr_debug("[Power/PMIC][pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg); */

#if 0
	/* 3. Double Check */
	/* mt6320_read_byte(RegNum, &pmic_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[Power/PMIC]" "[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
		return return_value;
	}
	pr_debug("[Power/PMIC][pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
#endif

#else
	pr_info("[Power/PMIC]" "[pmic_config_interface] Can not access HW PMIC\n");
#endif

	return return_value;
}

/* ============================================================================== */
/* mt-pmic dev_attr APIs */
/* ============================================================================== */
U32 g_reg_value = 0;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("[Power/PMIC][show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	U32 reg_value = 0;
	U32 reg_address = 0;
	pr_info("[Power/PMIC][store_pmic_access]\n");
	if (buf != NULL && size != 0) {
		pr_info("[Power/PMIC]" "[store_pmic_access] buf is %s and size is %lu\n", buf, size);
		reg_address = simple_strtoul(buf, &pvalue, 16);

#ifdef CONFIG_PM_DEBUG
		if ((size >= 10) && (strncmp(buf, "hard_reset", 10) == 0)) {
			pr_info("[Power/PMIC]" "[store_pmic_access] Simulate long press Power Key\n");
			arch_reset(0, NULL);
		} else
#endif
		if (size > 5) {
			reg_value = simple_strtoul((pvalue + 1), NULL, 16);
			pr_info("[Power/PMIC]" "[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_info("[Power/PMIC]" "[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value);
			pr_info("[Power/PMIC]" "[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/* 664 */

#if defined(CONFIG_MTK_BATTERY_PROTECT)
/* ============================================================================== */
/* Low Battery Protect UT */
/* ============================================================================== */
static ssize_t show_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	pr_info("[Power/PMIC]" "[show_low_battery_protect_ut] g_low_battery_level=%d\n", g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	char *pvalue = NULL;
	U32 val = 0;

	pr_info("[Power/PMIC][store_low_battery_protect_ut]\n");

	if (buf != NULL && size != 0) {
		pr_info("[Power/PMIC] buf is %s and size is %lu\n", buf, size);
		val = simple_strtoul(buf, &pvalue, 16);
		if (val <= 2) {
			pr_info("[Power/PMIC]" "[store_low_battery_protect_ut] your input is %d\n", val);
			exec_low_battery_callback(val);
		} else {
			pr_info("[Power/PMIC]" "[store_low_battery_protect_ut] wrong number (%d)\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_ut, 0664, show_low_battery_protect_ut,
							store_low_battery_protect_ut);	/* 664 */
/* ============================================================================== */
/* low battery protect stop */
/* ============================================================================== */
static ssize_t show_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	pr_info("[Power/PMIC]" "[show_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
	return sprintf(buf, "%u\n", g_low_battery_stop);
}

static ssize_t store_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	char *pvalue = NULL;
	U32 val = 0;

	pr_info("[Power/PMIC][store_low_battery_protect_stop]\n");

	if (buf != NULL && size != 0) {
		pr_info("[Power/PMIC] buf is %s and size is %lu\n", buf, size);
		val = simple_strtoul(buf, &pvalue, 16);
		if ((val != 0) && (val != 1))
			val = 0;
		g_low_battery_stop = val;
		pr_info("[Power/PMIC]" "[store_low_battery_protect_stop] g_low_battery_stop=%d\n",
			g_low_battery_stop);
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_stop, 0664, show_low_battery_protect_stop,
							store_low_battery_protect_stop);	/* 664 */

/* ============================================================================== */
/* low battery protect level */
/* ============================================================================== */
static ssize_t show_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	pr_info("[Power/PMIC]" "[show_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t size)
{
	pr_info("[Power/PMIC]" "[store_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);

	return size;
}

static DEVICE_ATTR(low_battery_protect_level, 0664, show_low_battery_protect_level,
								store_low_battery_protect_level);	/* 664 */
#endif
/* ============================================================================== */
/* LDO EN APIs */
/* ============================================================================== */
static void dct_pmic_VIO28_enable(kal_bool dctEnable)
{
	pr_info("[Power/PMIC][dct_pmic_VIO28_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_vio28_en(1);
	else
		upmu_set_vio28_en(0);

}

static void dct_pmic_VUSB_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VUSB_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vusb_en(1);
	else
		upmu_set_rg_vusb_en(0);

}

static void dct_pmic_VMC_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VMC_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vmc_en(1);
	else
		upmu_set_rg_vmc_en(0);

}

static void dct_pmic_VMCH_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VMCH_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vmch_en(1);
	else
		upmu_set_rg_vmch_en(0);

}

static void dct_pmic_VEMC_3V3_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VEMC_3V3_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vemc_3v3_en(1);
	else
		upmu_set_rg_vemc_3v3_en(0);

}

static void dct_pmic_VGP1_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP1_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp1_sw_en(1);
	else
		upmu_set_rg_vgp1_sw_en(0);

}

static void dct_pmic_VGP2_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP2_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp2_sw_en(1);
	else
		upmu_set_rg_vgp2_sw_en(0);

}

static void dct_pmic_VGP3_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP3_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp3_sw_en(1);
	else
		upmu_set_rg_vgp3_sw_en(0);

}

static void dct_pmic_VGP4_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP4_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp4_sw_en(1);
	else
		upmu_set_rg_vgp4_sw_en(0);

}

static void dct_pmic_VGP5_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP5_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp5_sw_en(1);
	else
		upmu_set_rg_vgp5_sw_en(0);

}

static void dct_pmic_VGP6_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VGP6_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vgp6_sw_en(1);
	else
		upmu_set_rg_vgp6_sw_en(0);

}

static void dct_pmic_VIBR_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VIBR_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vibr_en(1);
	else
		upmu_set_rg_vibr_en(0);

}

static void dct_pmic_VRTC_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VRTC_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_vrtc_en(1);
	else
		upmu_set_vrtc_en(0);

}

static void dct_pmic_VTCXO_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VTCXO_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vtcxo_en(1);
	else
		upmu_set_rg_vtcxo_en(0);

}

static void dct_pmic_VA28_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VA28_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_va28_en(1);
	else
		upmu_set_rg_va28_en(0);

}

static void dct_pmic_VCAMA_enable(kal_bool dctEnable)
{
	pr_debug("[Power/PMIC][dct_pmic_VCAMA_enable] %d\n", dctEnable);

	if (dctEnable == KAL_TRUE)
		upmu_set_rg_vcama_en(1);
	else
		upmu_set_rg_vcama_en(0);

}

/* ============================================================================== */
/* LDO SEL APIs */
/* ============================================================================== */
static void dct_pmic_VIO28_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC]" "[dct_pmic_VIO28_enable] No voltage can setting!\n");
}

static void dct_pmic_VUSB_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC]" "[dct_pmic_VUSB_sel] No voltage can setting!\n");
}

static void dct_pmic_VMC_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VMC_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vmc_vosel(1);
	else if (volt == VOL_3300)
		upmu_set_rg_vmc_vosel(1);
	else if (volt == VOL_1800)
		upmu_set_rg_vmc_vosel(0);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VMCH_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VMCH_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vmch_vosel(1);
	else if (volt == VOL_3000)
		upmu_set_rg_vmch_vosel(0);
	else if (volt == VOL_3300)
		upmu_set_rg_vmch_vosel(1);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VEMC_3V3_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VEMC_3V3_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vemc_3v3_vosel(1);
	else if (volt == VOL_3000)
		upmu_set_rg_vemc_3v3_vosel(0);
	else if (volt == VOL_3300)
		upmu_set_rg_vemc_3v3_vosel(1);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP1_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VCAMD_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp1_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp1_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp1_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp1_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp1_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp1_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp1_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp1_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vgp1_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP2_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VCAMIO_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp2_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp2_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp2_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp2_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp2_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp2_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp2_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp2_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vgp2_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP3_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VCAMAF_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp3_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp3_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp3_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp3_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp3_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp3_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp3_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp3_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vgp3_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP4_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VGP4_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp4_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp4_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp4_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp4_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp4_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp4_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp4_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp4_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vgp4_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP5_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VGP5_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp5_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp5_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp5_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp5_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp5_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp5_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp5_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp5_vosel(6);
	else if (volt == VOL_2000)
		upmu_set_rg_vgp5_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VGP6_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VGP6_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vgp6_vosel(5);
	else if (volt == VOL_1200)
		upmu_set_rg_vgp6_vosel(0);
	else if (volt == VOL_1300)
		upmu_set_rg_vgp6_vosel(1);
	else if (volt == VOL_1500)
		upmu_set_rg_vgp6_vosel(2);
	else if (volt == VOL_1800)
		upmu_set_rg_vgp6_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vgp6_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vgp6_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vgp6_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vgp6_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VIBR_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VIBR_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vibr_vosel(6);
	else if (volt == VOL_1300)
		upmu_set_rg_vibr_vosel(0);
	else if (volt == VOL_1500)
		upmu_set_rg_vibr_vosel(1);
	else if (volt == VOL_1800)
		upmu_set_rg_vibr_vosel(2);
	else if (volt == VOL_2000)
		upmu_set_rg_vibr_vosel(3);
	else if (volt == VOL_2500)
		upmu_set_rg_vibr_vosel(4);
	else if (volt == VOL_2800)
		upmu_set_rg_vibr_vosel(5);
	else if (volt == VOL_3000)
		upmu_set_rg_vibr_vosel(6);
	else if (volt == VOL_3300)
		upmu_set_rg_vibr_vosel(7);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void dct_pmic_VRTC_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC]" "[dct_pmic_VRTC_sel] No voltage can setting!\n");
}

static void dct_pmic_VTCXO_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC]" "[dct_pmic_VTCXO_sel] No voltage can setting!\n");
}

static void dct_pmic_VA28_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC]" "[dct_pmic_VA28_sel] No voltage can setting!\n");
}

static void dct_pmic_VCAMA_sel(kal_uint32 volt)
{
	pr_debug("[Power/PMIC] ****[dct_pmic_VCAMA_sel] value=%d\n", volt);

	if (volt == VOL_DEFAULT)
		upmu_set_rg_vcama_vosel(3);
	else if (volt == VOL_1500)
		upmu_set_rg_vcama_vosel(0);
	else if (volt == VOL_1800)
		upmu_set_rg_vcama_vosel(1);
	else if (volt == VOL_2500)
		upmu_set_rg_vcama_vosel(2);
	else if (volt == VOL_2800)
		upmu_set_rg_vcama_vosel(3);
	else
		pr_err("[Power/PMIC] Error Setting %d. DO nothing.\r\n", volt);
}

static void pmic_VGP4_vocal(kal_int32 mVolt)
{
	pr_debug("[Power/PMIC] ****[pmic_VGP4_cal] value=%d\n", mVolt);
	switch (mVolt) {
	case VOLCAL_Minus_20:
		upmu_set_rg_vgp4_cal(0x1);
		break;
	case VOLCAL_Minus_40:
		upmu_set_rg_vgp4_cal(0x2);
		break;
	case VOLCAL_Minus_60:
		upmu_set_rg_vgp4_cal(0x3);
		break;
	case VOLCAL_Minus_80:
		upmu_set_rg_vgp4_cal(0x4);
		break;
	case VOLCAL_Minus_100:
		upmu_set_rg_vgp4_cal(0x5);
		break;
	case VOLCAL_Minus_120:
		upmu_set_rg_vgp4_cal(0x6);
		break;
	case VOLCAL_Minus_140:
		upmu_set_rg_vgp4_cal(0x7);
		break;
	case VOLCAL_Plus_160:
		upmu_set_rg_vgp4_cal(0x8);
		break;
	case VOLCAL_Plus_140:
		upmu_set_rg_vgp4_cal(0x9);
		break;
	case VOLCAL_Plus_120:
		upmu_set_rg_vgp4_cal(0xA);
		break;
	case VOLCAL_Plus_100:
		upmu_set_rg_vgp4_cal(0xB);
		break;
	case VOLCAL_Plus_80:
		upmu_set_rg_vgp4_cal(0xC);
		break;
	case VOLCAL_Plus_60:
		upmu_set_rg_vgp4_cal(0xD);
		break;
	case VOLCAL_Plus_40:
		upmu_set_rg_vgp4_cal(0xE);
		break;
	case VOLCAL_Plus_20:
		upmu_set_rg_vgp4_cal(0xF);
		break;
	default:
		upmu_set_rg_vgp4_cal(0x0);
		break;
	}
}

static void pmic_VGP5_vocal(kal_int32 mVolt)
{
	pr_debug("[Power/PMIC] ****[pmic_VGP5_cal] value=%d\n", mVolt);
	/* not implemented */
}

static void pmic_VGP6_vocal(kal_int32 mVolt)
{
	pr_debug("[Power/PMIC] ****[pmic_VGP6_cal] value=%d\n", mVolt);
	switch (mVolt) {
	case VOLCAL_Minus_20:
		upmu_set_rg_vgp6_cal(0x1);
		break;
	case VOLCAL_Minus_40:
		upmu_set_rg_vgp6_cal(0x2);
		break;
	case VOLCAL_Minus_60:
		upmu_set_rg_vgp6_cal(0x3);
		break;
	case VOLCAL_Minus_80:
		upmu_set_rg_vgp6_cal(0x4);
		break;
	case VOLCAL_Minus_100:
		upmu_set_rg_vgp6_cal(0x5);
		break;
	case VOLCAL_Minus_120:
		upmu_set_rg_vgp6_cal(0x6);
		break;
	case VOLCAL_Minus_140:
		upmu_set_rg_vgp6_cal(0x7);
		break;
	case VOLCAL_Plus_160:
		upmu_set_rg_vgp6_cal(0x8);
		break;
	case VOLCAL_Plus_140:
		upmu_set_rg_vgp6_cal(0x9);
		break;
	case VOLCAL_Plus_120:
		upmu_set_rg_vgp6_cal(0xA);
		break;
	case VOLCAL_Plus_100:
		upmu_set_rg_vgp6_cal(0xB);
		break;
	case VOLCAL_Plus_80:
		upmu_set_rg_vgp6_cal(0xC);
		break;
	case VOLCAL_Plus_60:
		upmu_set_rg_vgp6_cal(0xD);
		break;
	case VOLCAL_Plus_40:
		upmu_set_rg_vgp6_cal(0xE);
		break;
	case VOLCAL_Plus_20:
		upmu_set_rg_vgp6_cal(0xF);
		break;
	default:
		upmu_set_rg_vgp6_cal(0x0);
		break;
	}
}


/* ============================================================================== */
/* LDO EN & SEL common API */
/* ============================================================================== */
struct mt_ldo_ops {
	void (*vosel) (kal_uint32);
	void (*vocal) (kal_int32);
	void (*enable) (kal_bool);
};

struct mt_ldo_ops ldo_ops[] = {
	[MT65XX_POWER_LDO_VIO28] = {
				    .vosel = dct_pmic_VIO28_sel,
				    .vocal = NULL,
				    .enable = dct_pmic_VIO28_enable,
				    },
	[MT65XX_POWER_LDO_VUSB] = {
				   .vosel = dct_pmic_VUSB_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VUSB_enable,
				   },
	[MT65XX_POWER_LDO_VMC] = {
				  .vosel = dct_pmic_VMC_sel,
				  .vocal = NULL,
				  .enable = dct_pmic_VMC_enable,
				  },
	[MT65XX_POWER_LDO_VMCH] = {
				   .vosel = dct_pmic_VMCH_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VMCH_enable,
				   },
	[MT65XX_POWER_LDO_VEMC_3V3] = {
				       .vosel = dct_pmic_VEMC_3V3_sel,
				       .vocal = NULL,
				       .enable = dct_pmic_VEMC_3V3_enable,
				       },
	[MT65XX_POWER_LDO_VGP1] = {
				   .vosel = dct_pmic_VGP1_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VGP1_enable,
				   },
	[MT65XX_POWER_LDO_VGP2] = {
				   .vosel = dct_pmic_VGP2_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VGP2_enable,
				   },
	[MT65XX_POWER_LDO_VGP3] = {
				   .vosel = dct_pmic_VGP3_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VGP3_enable,
				   },
	[MT65XX_POWER_LDO_VGP4] = {
				   .vosel = dct_pmic_VGP4_sel,
				   .vocal = pmic_VGP4_vocal,
				   .enable = dct_pmic_VGP4_enable,
				   },
	[MT65XX_POWER_LDO_VGP5] = {
				   .vosel = dct_pmic_VGP5_sel,
				   .vocal = pmic_VGP5_vocal,
				   .enable = dct_pmic_VGP5_enable,
				   },
	[MT65XX_POWER_LDO_VGP6] = {
				   .vosel = dct_pmic_VGP6_sel,
				   .vocal = pmic_VGP6_vocal,
				   .enable = dct_pmic_VGP6_enable,
				   },
	[MT65XX_POWER_LDO_VIBR] = {
				   .vosel = dct_pmic_VIBR_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VIBR_enable,
				   },
	[MT65XX_POWER_LDO_VRTC] = {
				   .vosel = dct_pmic_VRTC_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VRTC_enable,
				   },
	[MT65XX_POWER_LDO_VTCXO] = {
				    .vosel = dct_pmic_VTCXO_sel,
				    .enable = dct_pmic_VTCXO_enable,
				    },
	[MT65XX_POWER_LDO_VA28] = {
				   .vosel = dct_pmic_VA28_sel,
				   .vocal = NULL,
				   .enable = dct_pmic_VA28_enable,
				   },
	[MT65XX_POWER_LDO_VCAMA] = {
				    .vosel = dct_pmic_VCAMA_sel,
				    .vocal = NULL,
				    .enable = dct_pmic_VCAMA_enable,
				    },
};

static void pmic_ldo_enable(MT65XX_POWER powerId, kal_bool powerEnable)
{
	pr_debug("%s: Receive powerId %d, action is %d\n", __func__, powerId, powerEnable);

	if (powerId >= ARRAY_SIZE(ldo_ops) || powerId < 0 || !ldo_ops[powerId].enable)
		return;

	ldo_ops[powerId].enable(powerEnable);
}

static void pmic_ldo_vol_sel(MT65XX_POWER powerId, MT65XX_POWER_VOLTAGE powerVolt)
{
	pr_debug("%s: Receive powerId %d, action is %d\n", __func__, powerId, powerVolt);

	if (powerId >= ARRAY_SIZE(ldo_ops) || powerId < 0 || !ldo_ops[powerId].vosel)
		return;

	ldo_ops[powerId].vosel(powerVolt);
}

static void pmic_ldo_vol_cal(MT65XX_POWER powerId, enum MT65XX_POWER_VOLCAL powermVolt)
{
	pr_debug("%s: Receive powerId %d, action is %d\n", __func__, powerId, powermVolt);

	if (powerId >= ARRAY_SIZE(ldo_ops) || powerId < 0 || !ldo_ops[powerId].vocal)
		return;

	ldo_ops[powerId].vocal(powermVolt);
}

static kal_int32 pmic_internal_ldo_vosel(void *data)
{
	kal_int32 status = STATUS_OK;
	struct power_vosel_data ldo_vosel;
	ldo_vosel = *(struct power_vosel_data *)data;
	pmic_ldo_vol_sel(ldo_vosel.powerId, ldo_vosel.powerVolt);
	return status;
}

static kal_int32 pmic_internal_ldo_vocal(void *data)
{
	kal_int32 status = STATUS_OK;
	struct power_vocal_data ldo_vocal;
	ldo_vocal = *(struct power_vocal_data *)data;
	pmic_ldo_vol_cal(ldo_vocal.powerId, ldo_vocal.powermVolt);
	return status;
}

static kal_int32 pmic_internal_ldo_enable(void *data)
{
	kal_int32 status = STATUS_OK;
	struct power_enable_data ldo_enable;
	ldo_enable = *(struct power_enable_data *)data;
	pmic_ldo_enable(ldo_enable.powerId, ldo_enable.powerEnable);
	return status;
}

static kal_int32(*const pmic_power_func[PMIC_POWER_CMD_MAX]) (void *data) = {
pmic_internal_ldo_vosel, pmic_internal_ldo_vocal, pmic_internal_ldo_enable};

kal_int32 power_control_interface(enum PMIC_POWER_CTRL_CMD cmd, void *data)
{
	kal_int32 status;
	if (cmd < PMIC_POWER_CMD_MAX)
		status = pmic_power_func[cmd] (data);
	else
		return PMIC_CMD_UNSUPPORTED;

	return status;
}

/* ============================================================================== */
/* EM */
/* ============================================================================== */
static ssize_t show_BUCK_VPCA7_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address = VPCA7_CON7; */
	/* kal_uint32 reg_val=0; */

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13); */
	/* ret_value = reg_val; */
	ret_value = upmu_get_qi_vpca7_en();

	pr_debug("[Power/PMIC][EM] BUCK_VPCA7_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VPCA7_STATUS(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VPCA7_STATUS, 0664, show_BUCK_VPCA7_STATUS, store_BUCK_VPCA7_STATUS);

static ssize_t show_BUCK_VSRMCA7_STATUS(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x23A;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vsrmca7_en();

	pr_debug("[Power/PMIC][EM] BUCK_VSRMCA7_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VSRMCA7_STATUS(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VSRMCA7_STATUS, 0664, show_BUCK_VSRMCA7_STATUS, store_BUCK_VSRMCA7_STATUS);

static ssize_t show_BUCK_VCA15_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address = VCA15_CON7; */
	/* kal_uint32 reg_val=0; */

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13); */
	/* ret_value = reg_val; */
	ret_value = upmu_get_qi_vca15_en();

	pr_debug("[Power/PMIC][EM] BUCK_VCA15_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VCA15_STATUS(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VCA15_STATUS, 0664, show_BUCK_VCA15_STATUS, store_BUCK_VCA15_STATUS);

static ssize_t show_BUCK_VSRMCA15_STATUS(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x23A;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vsrmca15_en();

	pr_debug("[Power/PMIC][EM] BUCK_VSRMCA15_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VSRMCA15_STATUS(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VSRMCA15_STATUS, 0664, show_BUCK_VSRMCA15_STATUS,
		   store_BUCK_VSRMCA15_STATUS);

static ssize_t show_BUCK_VCORE_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x266;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vcore_en();

	pr_debug("[Power/PMIC][EM] BUCK_VCORE_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VCORE_STATUS(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VCORE_STATUS, 0664, show_BUCK_VCORE_STATUS, store_BUCK_VCORE_STATUS);

static ssize_t show_BUCK_VDRM_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x28C;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vdrm_en();

	pr_debug("[Power/PMIC][EM] BUCK_VDRM_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VDRM_STATUS(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VDRM_STATUS, 0664, show_BUCK_VDRM_STATUS, store_BUCK_VDRM_STATUS);

static ssize_t show_BUCK_VIO18_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x30E;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vio18_en();

	pr_debug("[Power/PMIC][EM] BUCK_VIO18_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VIO18_STATUS(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VIO18_STATUS, 0664, show_BUCK_VIO18_STATUS, store_BUCK_VIO18_STATUS);

static ssize_t show_BUCK_VGPU_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x388;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 13);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vgpu_en();

	pr_debug("[Power/PMIC][EM] BUCK_VGPU_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VGPU_STATUS(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VGPU_STATUS, 0664, show_BUCK_VGPU_STATUS, store_BUCK_VGPU_STATUS);

static ssize_t show_LDO_VIO28_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x420;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vio28_en();

	pr_debug("[Power/PMIC][EM] LDO_VIO28_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VIO28_STATUS(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VIO28_STATUS, 0664, show_LDO_VIO28_STATUS, store_LDO_VIO28_STATUS);

static ssize_t show_LDO_VUSB_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x422;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vusb_en();

	pr_debug("[Power/PMIC][EM] LDO_VUSB_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VUSB_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VUSB_STATUS, 0664, show_LDO_VUSB_STATUS, store_LDO_VUSB_STATUS);

static ssize_t show_LDO_VMC_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x424;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vmc_en();

	pr_debug("[Power/PMIC][EM] LDO_VMC_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VMC_STATUS(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VMC_STATUS, 0664, show_LDO_VMC_STATUS, store_LDO_VMC_STATUS);

static ssize_t show_LDO_VMCH_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x426;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vmch_en();

	pr_debug("[Power/PMIC][EM] LDO_VMCH_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VMCH_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VMCH_STATUS, 0664, show_LDO_VMCH_STATUS, store_LDO_VMCH_STATUS);

static ssize_t show_LDO_VEMC_3V3_STATUS(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x428;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vemc_3v3_en();

	pr_debug("[Power/PMIC][EM] LDO_VEMC_3V3_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VEMC_3V3_STATUS(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VEMC_3V3_STATUS, 0664, show_LDO_VEMC_3V3_STATUS, store_LDO_VEMC_3V3_STATUS);

static ssize_t show_LDO_VGP1_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x42A;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp1_en();

	pr_debug("[Power/PMIC][EM] LDO_VCAMD_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP1_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMD_STATUS, 0664, show_LDO_VGP1_STATUS, store_LDO_VGP1_STATUS);

static ssize_t show_LDO_VGP2_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x42C;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp2_en();

	pr_debug("[Power/PMIC][EM] LDO_VCAMIO_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP2_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMIO_STATUS, 0664, show_LDO_VGP2_STATUS, store_LDO_VGP2_STATUS);

static ssize_t show_LDO_VGP3_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x42E;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp3_en();

	pr_debug("[Power/PMIC][EM] LDO_VCAMAF_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP3_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMAF_STATUS, 0664, show_LDO_VGP3_STATUS, store_LDO_VGP3_STATUS);

static ssize_t show_LDO_VGP4_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x430;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp4_en();

	pr_debug("[Power/PMIC][EM] LDO_VGP4_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP4_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP4_STATUS, 0664, show_LDO_VGP4_STATUS, store_LDO_VGP4_STATUS);

static ssize_t show_LDO_VGP5_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x432;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp5_en();

	pr_debug("[Power/PMIC][EM] LDO_VGP5_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP5_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP5_STATUS, 0664, show_LDO_VGP5_STATUS, store_LDO_VGP5_STATUS);

static ssize_t show_LDO_VGP6_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x434;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vgp6_en();

	pr_debug("[Power/PMIC][EM] LDO_VGP6_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP6_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP6_STATUS, 0664, show_LDO_VGP6_STATUS, store_LDO_VGP6_STATUS);

static ssize_t show_LDO_VIBR_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x466;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vibr_en();

	pr_debug("[Power/PMIC][EM] LDO_VIBR_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VIBR_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VIBR_STATUS, 0664, show_LDO_VIBR_STATUS, store_LDO_VIBR_STATUS);

static ssize_t show_LDO_VRTC_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x43A;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vrtc_en();

	pr_debug("[Power/PMIC][EM] LDO_VRTC_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VRTC_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VRTC_STATUS, 0664, show_LDO_VRTC_STATUS, store_LDO_VRTC_STATUS);

static ssize_t show_LDO_VTCXO_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x402;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_vtcxo_en();

	pr_debug("[Power/PMIC][EM] LDO_VTCXO_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VTCXO_STATUS(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VTCXO_STATUS, 0664, show_LDO_VTCXO_STATUS, store_LDO_VTCXO_STATUS);

static ssize_t show_LDO_VA28_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x406;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_qi_va28_en();

	pr_debug("[Power/PMIC][EM] LDO_VA28_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VA28_STATUS(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VA28_STATUS, 0664, show_LDO_VA28_STATUS, store_LDO_VA28_STATUS);

static ssize_t show_LDO_VCAMA_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;
	/*
	   kal_uint32 ret=0;
	   kal_uint32 reg_address=0x408;
	   kal_uint32 reg_val=0;

	   ret = pmic_read_interface(reg_address, &reg_val, 0x01, 15);
	   ret_value = reg_val;
	 */
	ret_value = upmu_get_status_vcama_en();

	pr_debug("[Power/PMIC][EM] LDO_VCAMA_STATUS : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VCAMA_STATUS(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMA_STATUS, 0664, show_LDO_VCAMA_STATUS, store_LDO_VCAMA_STATUS);

/* voltage --------------------------------------------------------------------------------- */

static ssize_t show_BUCK_VPCA7_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x21E; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vpca7_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VPCA7_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VPCA7_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VPCA7_VOLTAGE, 0664, show_BUCK_VPCA7_VOLTAGE, store_BUCK_VPCA7_VOLTAGE);

static ssize_t show_BUCK_VSRMCA7_VOLTAGE(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x244; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vsrmca7_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VSRMCA7_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VSRMCA7_VOLTAGE(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VSRMCA7_VOLTAGE, 0664, show_BUCK_VSRMCA7_VOLTAGE,
		   store_BUCK_VSRMCA7_VOLTAGE);

static ssize_t show_BUCK_VCA15_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x21E; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vca15_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VCA15_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VCA15_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VCA15_VOLTAGE, 0664, show_BUCK_VCA15_VOLTAGE, store_BUCK_VCA15_VOLTAGE);

static ssize_t show_BUCK_VSRMCA15_VOLTAGE(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x244; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vsrmca15_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VSRMCA15_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VSRMCA15_VOLTAGE(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VSRMCA15_VOLTAGE, 0664, show_BUCK_VSRMCA15_VOLTAGE,
		   store_BUCK_VSRMCA15_VOLTAGE);

static ssize_t show_BUCK_VCORE_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x270; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vcore_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VCORE_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VCORE_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VCORE_VOLTAGE, 0664, show_BUCK_VCORE_VOLTAGE, store_BUCK_VCORE_VOLTAGE);

static ssize_t show_BUCK_VDRM_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x296; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x7F, 0); */
	reg_val = upmu_get_ni_vdrm_vosel();
	ret_value = 80000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VDRM_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VDRM_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VDRM_VOLTAGE, 0664, show_BUCK_VDRM_VOLTAGE, store_BUCK_VDRM_VOLTAGE);

static ssize_t show_BUCK_VIO18_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x318; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x1F, 0); */
	reg_val = upmu_get_ni_vio18_vosel();
	ret_value = 1500 + (reg_val * 20);

	pr_debug("[Power/PMIC][EM] BUCK_VIO18_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VIO18_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VIO18_VOLTAGE, 0664, show_BUCK_VIO18_VOLTAGE, store_BUCK_VIO18_VOLTAGE);

static ssize_t show_BUCK_VGPU_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	/* kal_uint32 ret=0; */
	/* kal_uint32 reg_address=0x392; */
	kal_uint32 reg_val = 0;

	/* ret = pmic_read_interface(reg_address, &reg_val, 0x1F, 0); */
	reg_val = upmu_get_ni_vgpu_vosel();
	ret_value = 70000 + (reg_val * 625);
	ret_value = ret_value / 100;

	pr_debug("[Power/PMIC][EM] BUCK_VGPU_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_BUCK_VGPU_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(BUCK_VGPU_VOLTAGE, 0664, show_BUCK_VGPU_VOLTAGE, store_BUCK_VGPU_VOLTAGE);

static ssize_t show_LDO_VIO28_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	ret_value = 2800;

	pr_debug("[Power/PMIC][EM] LDO_VIO28_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VIO28_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VIO28_VOLTAGE, 0664, show_LDO_VIO28_VOLTAGE, store_LDO_VIO28_VOLTAGE);

static ssize_t show_LDO_VUSB_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VUSB_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VUSB_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VUSB_VOLTAGE, 0664, show_LDO_VUSB_VOLTAGE, store_LDO_VUSB_VOLTAGE);

static ssize_t show_LDO_VMC_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x44A;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x01, 4);
	if (reg_val == 0)
		ret_value = 1800;
	else if (reg_val == 1)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VMC_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VMC_VOLTAGE(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VMC_VOLTAGE, 0664, show_LDO_VMC_VOLTAGE, store_LDO_VMC_VOLTAGE);

static ssize_t show_LDO_VMCH_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x432;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x01, 7);
	if (reg_val == 0)
		ret_value = 3000;
	else if (reg_val == 1)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VMCH_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VMCH_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VMCH_VOLTAGE, 0664, show_LDO_VMCH_VOLTAGE, store_LDO_VMCH_VOLTAGE);

static ssize_t show_LDO_VEMC_3V3_VOLTAGE(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x434;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x01, 4);
	if (reg_val == 0)
		ret_value = 3000;
	else if (reg_val == 1)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VEMC_3V3_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VEMC_3V3_VOLTAGE(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VEMC_3V3_VOLTAGE, 0664, show_LDO_VEMC_3V3_VOLTAGE,
		   store_LDO_VEMC_3V3_VOLTAGE);

static ssize_t show_LDO_VCAMD_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x436;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VCAMD_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VCAMD_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMD_VOLTAGE, 0664, show_LDO_VCAMD_VOLTAGE, store_LDO_VCAMD_VOLTAGE);

static ssize_t show_LDO_VCAMIO_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x438;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VCAMIO_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VCAMIO_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMIO_VOLTAGE, 0664, show_LDO_VCAMIO_VOLTAGE, store_LDO_VCAMIO_VOLTAGE);

static ssize_t show_LDO_VCAMAF_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x43A;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VCAMAF_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VCAMAF_VOLTAGE(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMAF_VOLTAGE, 0664, show_LDO_VCAMAF_VOLTAGE, store_LDO_VCAMAF_VOLTAGE);

static ssize_t show_LDO_VGP4_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x43C;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VGP4_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP4_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP4_VOLTAGE, 0664, show_LDO_VGP4_VOLTAGE, store_LDO_VGP4_VOLTAGE);

static ssize_t show_LDO_VGP5_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x43E;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 2000;

	pr_debug("[Power/PMIC][EM] LDO_VGP5_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP5_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP5_VOLTAGE, 0664, show_LDO_VGP5_VOLTAGE, store_LDO_VGP5_VOLTAGE);

static ssize_t show_LDO_VGP6_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x45A;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 5);
	if (reg_val == 0)
		ret_value = 1200;
	else if (reg_val == 1)
		ret_value = 1300;
	else if (reg_val == 2)
		ret_value = 1500;
	else if (reg_val == 3)
		ret_value = 1800;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VGP6_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VGP6_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VGP6_VOLTAGE, 0664, show_LDO_VGP6_VOLTAGE, store_LDO_VGP6_VOLTAGE);

static ssize_t show_LDO_VIBR_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x442;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x07, 9);
	if (reg_val == 0)
		ret_value = 1300;
	else if (reg_val == 1)
		ret_value = 1500;
	else if (reg_val == 2)
		ret_value = 1800;
	else if (reg_val == 3)
		ret_value = 2000;
	else if (reg_val == 4)
		ret_value = 2500;
	else if (reg_val == 5)
		ret_value = 2800;
	else if (reg_val == 6)
		ret_value = 3000;
	else if (reg_val == 7)
		ret_value = 3300;

	pr_debug("[Power/PMIC][EM] LDO_VIBR_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VIBR_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VIBR_VOLTAGE, 0664, show_LDO_VIBR_VOLTAGE, store_LDO_VIBR_VOLTAGE);

static ssize_t show_LDO_VRTC_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	ret_value = 2800;

	pr_debug("[Power/PMIC][EM] LDO_VRTC_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VRTC_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VRTC_VOLTAGE, 0664, show_LDO_VRTC_VOLTAGE, store_LDO_VRTC_VOLTAGE);

static ssize_t show_LDO_VTCXO_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	ret_value = 2800;

	pr_debug("[Power/PMIC][EM] LDO_VTCXO_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VTCXO_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VTCXO_VOLTAGE, 0664, show_LDO_VTCXO_VOLTAGE, store_LDO_VTCXO_VOLTAGE);

static ssize_t show_LDO_VA28_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	ret_value = 2800;

	pr_debug("[Power/PMIC][EM] LDO_VA28_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VA28_VOLTAGE(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VA28_VOLTAGE, 0664, show_LDO_VA28_VOLTAGE, store_LDO_VA28_VOLTAGE);

static ssize_t show_LDO_VCAMA_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_uint32 ret_value = 0;

	kal_uint32 ret = 0;
	kal_uint32 reg_address = 0x40C;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg_address, &reg_val, 0x03, 6);
	if (reg_val == 0)
		ret_value = 1500;
	else if (reg_val == 1)
		ret_value = 1800;
	else if (reg_val == 2)
		ret_value = 2500;
	else if (reg_val == 3)
		ret_value = 2800;

	pr_debug("[Power/PMIC][EM] LDO_VCAMA_VOLTAGE : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_VCAMA_VOLTAGE(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC][EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(LDO_VCAMA_VOLTAGE, 0664, show_LDO_VCAMA_VOLTAGE, store_LDO_VCAMA_VOLTAGE);


/* ============================================================================== */
/* PMIC6397 device driver */
/* ============================================================================== */
void ldo_service_test(void)
{
#ifdef CONFIG_REGULATOR
#else
	hwPowerOn(MT65XX_POWER_LDO_VIO28, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VUSB, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VMC, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VMCH, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VEMC_3V3, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP1, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP3, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VIBR, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VRTC, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VTCXO, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VA28, VOL_DEFAULT, "ldo_test");
	hwPowerOn(MT65XX_POWER_LDO_VCAMA, VOL_DEFAULT, "ldo_test");

	hwPowerDown(MT65XX_POWER_LDO_VIO28, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VUSB, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VMC, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VMCH, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VEMC_3V3, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP1, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP3, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP4, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP5, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VGP6, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VIBR, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VRTC, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VTCXO, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VA28, "ldo_test");
	hwPowerDown(MT65XX_POWER_LDO_VCAMA, "ldo_test");
#endif
}

int is_ext_buck_sw_ready(void)
{
	if ((is_da9212_sw_ready() == 1))
		return 1;
	else
		return 0;
}

int ext_buck_vosel(unsigned long val)
{
	int ret = 1;		/* 1:I2C success, 0:I2C fail */

	if (is_ext_buck_sw_ready() == 1) {
		if (is_da9212_exist() == 1) {
			ret = da9212_buck_set_voltage(DA9212_BUCK_A, val);
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC",
				    "[ext_buck_vosel] no ext buck ?!\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC",
			    "[ext_buck_vosel] ext buck sw not ready\n");
	}

	return ret;
}


void PMIC_INIT_SETTING_V1(void)
{
	U32 chip_version = 0;
	U32 ret = 0;

	/* put init setting from DE/SA */
	chip_version = upmu_get_cid();

	switch (chip_version & 0xFF) {
	case 0x91:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xC, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x0, 0x3, 10);
		break;
	case 0x97:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x1, 0x3, 10);
		break;
	default:
		pr_err("[Power/PMIC] Error chip ID %d\r\n", chip_version);
		break;
	}

	ret = pmic_config_interface(0xC, 0x1, 0x7, 1);	/* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
	ret = pmic_config_interface(0x24, 0x1, 0x1, 1);	/* [1:1]: RG_BC11_RST; */
	ret = pmic_config_interface(0x2A, 0x0, 0x7, 4);	/* [6:4]: RG_CSDAC_STP; align to 6250's setting */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 7);	/* [7:7]: RG_ULC_DET_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 6);	/* [6:6]: RG_HWCV_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 2);	/* [2:2]: RG_CSDAC_MODE; */
	ret = pmic_config_interface(0x102, 0x0, 0x1, 3);	/* [3:3]: RG_PWMOC_CK_PDN; For OC protection */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 9);	/* [9:9]: RG_SRCVOLT_HW_AUTO_EN; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 8);	/* [8:8]: RG_OSC_SEL_AUTO; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 6);	/* [6:6]: RG_SMPS_DIV2_SRC_AUTOFF_DIS; */
	ret = pmic_config_interface(0x128, 0x1, 0x1, 5);	/* [5:5]: RG_SMPS_AUTOFF_DIS; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 7);	/* [7:7]: VDRM_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 6);	/* [6:6]: VSRMCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 5);	/* [5:5]: VPCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 4);	/* [4:4]: VIO18_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 3);	/* [3:3]: VGPU_DEG_EN; For OC protection */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 2);	/* [2:2]: VCORE_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 1);	/* [1:1]: VSRMCA15_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 0);	/* [0:0]: VCA15_DEG_EN; */
	ret = pmic_config_interface(0x206, 0x600, 0x0FFF, 0);	/* [12:0]: BUCK_RSV; for OC protection */
	/* [7:6]: QI_VSRMCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 6);
	/* [5:4]: QI_VSRMCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 4);
	/* [3:2]: QI_VPCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 2);
	/* [1:0]: QI_VCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 0);
	/* [13:12]: RG_VCA15_CSL2; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 12);
	/* [11:10]: RG_VCA15_CSL1; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 10);
	/* [15:15]: VCA15_SFCHG_REN; soft change rising enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 15);
	/* [14:8]: VCA15_SFCHG_RRATE; soft change rising step=0.5us */
	ret = pmic_config_interface(0x224, 0x5, 0x7F, 8);
	/* [7:7]: VCA15_SFCHG_FEN; soft change falling enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 7);
	/* [6:0]: VCA15_SFCHG_FRATE; soft change falling step=2us */
	ret = pmic_config_interface(0x224, 0x17, 0x7F, 0);
	/* [6:0]: VCA15_VOSEL_SLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x22A, 0x0, 0x7F, 0);
	/* [8:8]: VCA15_VSLEEP_EN; set sleep mode reference voltage from R2R to V2V */
	ret = pmic_config_interface(0x238, 0x1, 0x1, 8);
	/* [5:4]: VCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x238, 0x3, 0x3, 4);
	ret = pmic_config_interface(0x244, 0x1, 0x1, 5);	/* [5:5]: VSRMCA15_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x246, 0x0, 0x3, 4);	/* [5:4]: VSRMCA15_VOSEL_SEL; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1, 15);	/* [15:15]: VSRMCA15_SFCHG_REN; */
	ret = pmic_config_interface(0x24A, 0x5, 0x7F, 8);	/* [14:8]: VSRMCA15_SFCHG_RRATE; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1, 7);	/* [7:7]: VSRMCA15_SFCHG_FEN; */
	ret = pmic_config_interface(0x24A, 0x17, 0x7F, 0);	/* [6:0]: VSRMCA15_SFCHG_FRATE; */
	/* [6:0]: VSRMCA15_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x250, 0x00, 0x7F, 0);
	/* [8:8]: VSRMCA15_VSLEEP_EN; set sleep mode reference voltage from R2R to V2V */
	ret = pmic_config_interface(0x25E, 0x1, 0x1, 8);
	/* [5:4]: VSRMCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x25E, 0x3, 0x3, 4);
	/* [1:1]: VCORE_VOSEL_CTRL; sleep mode voltage control follow SRCLKEN */
	ret = pmic_config_interface(0x270, 0x1, 0x1, 1);
	ret = pmic_config_interface(0x272, 0x0, 0x3, 4);	/* [5:4]: VCORE_VOSEL_SEL; */
	ret = pmic_config_interface(0x276, 0x1, 0x1, 15);	/* [15:15]: VCORE_SFCHG_REN; */
	ret = pmic_config_interface(0x276, 0x5, 0x7F, 8);	/* [14:8]: VCORE_SFCHG_RRATE; */
	ret = pmic_config_interface(0x276, 0x17, 0x7F, 0);	/* [6:0]: VCORE_SFCHG_FRATE; */
	/* [6:0]: VCORE_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x27C, 0x0, 0x7F, 0);
	/* [8:8]: VCORE_VSLEEP_EN; Sleep mode HW control  R2R to VtoV */
	ret = pmic_config_interface(0x28A, 0x1, 0x1, 8);
	/* [5:4]: VCORE_VOSEL_TRANS_EN; Follows MT6320 VCORE setting. */
	ret = pmic_config_interface(0x28A, 0x0, 0x3, 4);
	ret = pmic_config_interface(0x28A, 0x3, 0x3, 0);	/* [1:0]: VCORE_TRANSTD; */
	ret = pmic_config_interface(0x28E, 0x1, 0x3, 8);	/* [9:8]: RG_VGPU_CSL; for OC protection */
	ret = pmic_config_interface(0x29C, 0x1, 0x1, 15);	/* [15:15]: VGPU_SFCHG_REN; */
	ret = pmic_config_interface(0x29C, 0x5, 0x7F, 8);	/* [14:8]: VGPU_SFCHG_RRATE; */
	ret = pmic_config_interface(0x29C, 0x17, 0x7F, 0);	/* [6:0]: VGPU_SFCHG_FRATE; */
	ret = pmic_config_interface(0x2B0, 0x0, 0x3, 4);	/* [5:4]: VGPU_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x2B0, 0x3, 0x3, 0);	/* [1:0]: VGPU_TRANSTD; */
	ret = pmic_config_interface(0x332, 0x0, 0x3, 4);	/* [5:4]: VPCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x336, 0x1, 0x1, 15);	/* [15:15]: VPCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x336, 0x5, 0x7F, 8);	/* [14:8]: VPCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x336, 0x1, 0x1, 7);	/* [7:7]: VPCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x336, 0x17, 0x7F, 0);	/* [6:0]: VPCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x33C, 0x18, 0x7F, 0);	/* [6:0]: VPCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x34A, 0x1, 0x1, 8);	/* [8:8]: VPCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x34A, 0x3, 0x3, 4);	/* [5:4]: VPCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 5);	/* [5:5]: VSRMCA7_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x358, 0x0, 0x3, 4);	/* [5:4]: VSRMCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1, 15);	/* [15:15]: VSRMCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x35C, 0x5, 0x7F, 8);	/* [14:8]: VSRMCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1, 7);	/* [7:7]: VSRMCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x35C, 0x17, 0x7F, 0);	/* [6:0]: VSRMCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x362, 0x18, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x370, 0x1, 0x1, 8);	/* [8:8]: VSRMCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x370, 0x3, 0x3, 4);	/* [5:4]: VSRMCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x39C, 0x1, 0x1, 8);	/* [8:8]: VDRM_VSLEEP_EN; */
	ret = pmic_config_interface(0x440, 0x1, 0x1, 2);	/* [2:2]: VIBR_THER_SHEN_EN; */
	ret = pmic_config_interface(0x500, 0x1, 0x1, 5);	/* [5:5]: THR_HWPDN_EN; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 3);	/* [3:3]: RG_RST_DRVSEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 2);	/* [2:2]: RG_EN_DRVSEL; */
	ret = pmic_config_interface(0x508, 0x1, 0x1, 1);	/* [1:1]: PWRBB_DEB_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 12);	/* [12:12]: VSRMCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 11);	/* [11:11]: VPCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 10);	/* [10:10]: VCORE_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 9);	/* [9:9]: VSRMCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 8);	/* [8:8]: VPCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 1);	/* [1:1]: STRUP_PWROFF_PREOFF_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 0);	/* [0:0]: STRUP_PWROFF_SEQ_EN; */
	ret = pmic_config_interface(0x55E, 0xFC, 0xFF, 8);	/* [15:8]: RG_ADC_TRIM_CH_SEL; */
	ret = pmic_config_interface(0x560, 0x1, 0x1, 1);	/* [1:1]: FLASH_THER_SHDN_EN; */
	ret = pmic_config_interface(0x566, 0x1, 0x1, 1);	/* [1:1]: KPLED_THER_SHDN_EN; */
	ret = pmic_config_interface(0x600, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_L_EN; */
	ret = pmic_config_interface(0x604, 0x1, 0x1, 0);	/* [0:0]: RG_SPK_INTG_RST_L; */
	ret = pmic_config_interface(0x606, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_R_EN; */
	ret = pmic_config_interface(0x60A, 0x1, 0xF, 11);	/* [14:11]: RG_SPKPGA_GAINR; */
	ret = pmic_config_interface(0x612, 0x1, 0xF, 8);	/* [11:8]: RG_SPKPGA_GAINL; */
	ret = pmic_config_interface(0x632, 0x1, 0x1, 8);	/* [8:8]: FG_SLP_EN; */
	ret = pmic_config_interface(0x638, 0xFFC2, 0xFFFF, 0);	/* [15:0]: FG_SLP_CUR_TH; */
	ret = pmic_config_interface(0x63A, 0x14, 0xFF, 0);	/* [7:0]: FG_SLP_TIME; */
	ret = pmic_config_interface(0x63C, 0xFF, 0xFF, 8);	/* [15:8]: FG_DET_TIME; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 7);	/* [7:7]: RG_LCLDO_ENC_REMOTE_SENSE_VA28; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 4);	/* [4:4]: RG_LCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 1);	/* [1:1]: RG_HCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x71A, 0x1, 0x1, 15);	/* [15:15]: RG_NCP_REMOTE_SENSE_VA18; */
	ret = pmic_config_interface(0x260, 0x10, 0x7F, 8);	/* [14:8]: VSRMCA15_VOSEL_OFFSET; set offset=100mV */
	ret = pmic_config_interface(0x260, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(0x262, 0x48, 0x7F, 8);	/* [14:8]: VSRMCA15_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(0x262, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_ON_LB; set LB=0.7V */
	ret = pmic_config_interface(0x264, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA15_VOSEL_SLEEP_LB; set sleep LB=0.7V */
	ret = pmic_config_interface(0x372, 0x4, 0x7F, 8);	/* [14:8]: VSRMCA7_VOSEL_OFFSET; set offset=25mV */
	ret = pmic_config_interface(0x372, 0x0, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(0x374, 0x48, 0x7F, 8);	/* [14:8]: VSRMCA7_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(0x374, 0x38, 0x7F, 0);	/* [6:0]: VSRMCA7_VOSEL_ON_LB; set LB=1.05000V */
	ret = pmic_config_interface(0x376, 0x18, 0x7F, 0);	/* [6:0]: set sleep LB=0.85000V */
	ret = pmic_config_interface(0x21E, 0x3, 0x3, 0);	/* [1:1]: DVS HW control by SRCLKEN */
	ret = pmic_config_interface(0x244, 0x3, 0x3, 0);	/* [1:1]: VSRMCA15_VOSEL_CTRL, VSRMCA15_EN_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1, 1);	/* [1:1]: VPCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 1);	/* [1:1]: VSRMCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(0x21E, 0x1, 0x1, 4);	/* [4:4]: VCA15_TRACK_ON_CTRL; DVFS tracking enable */
	ret = pmic_config_interface(0x244, 0x1, 0x1, 4);	/* [4:4]: VSRMCA15_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1, 4);	/* [4:4]: VPCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1, 4);	/* [4:4]: VSRMCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 14);	/* [15:14]: VGPU OC; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 2);	/* [3:2]: VCA15 OC; */
}


void PMIC_CUSTOM_SETTING_V1(void)
{
#if 0
	U32 ret = 0, reg_val = 0;
#endif

	/* enable HW control DCXO 26MHz on-off, request by SPM module */
	upmu_set_rg_srcvolt_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_dbb_reg_en(0);

	/* enable HW control DCXO RF clk on-off, request by low power module task */
	upmu_set_rg_srclkperi_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_rf1_reg_en(0);

#ifndef CONFIG_MTK_PMIC_RF2_26M_ALWAYS_ON
	/* upmu_set_rg_dcxo_ldo_rf2_reg_en(0); */
	/* disable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x0);	/* clock off for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x0);	/* clock off for external 32K */
#else
	/* enable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x1);	/* clock on for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x1);	/* clock on for external 32K */
#endif

#if 0
	/* config vcore to HW control mode by deep idle PIC(YT Lee) request */
	upmu_set_vcore_vosel_ctrl(1);

	/* enable VGP6 by default */
	upmu_set_rg_vgp6_sw_en(1);
	upmu_set_rg_vgp6_vosel(0x7);
	ret = pmic_read_interface(DIGLDO_CON33, &reg_val, 0xFFFF, 0);
	pr_info("[Power/PMIC][%s] Reg[0x%x] = 0x%x\n", __func__, DIGLDO_CON33, reg_val);
#endif
}

void pmic_low_power_setting(void)
{
	U32 ret = 0;

	pr_info("[Power/PMIC]" "[pmic_low_power_setting] CLKCTL - 20121018 by Juinn-Ting\n");
	/* ret = pmic_config_interface(TOP_CKCON1  , 0x0F20 , 0xAF20 ,0); */


	upmu_set_vio18_vsleep_en(1);
	/* top */
	ret = pmic_config_interface(0x102, 0x8000, 0x8000, 0);
	ret = pmic_config_interface(0x108, 0x0882, 0x0882, 0);
	ret = pmic_config_interface(0x12a, 0x0000, 0x8c00, 0);	/* reg_ck:24MHz */
	ret = pmic_config_interface(0x206, 0x0060, 0x0060, 0);
	ret = pmic_config_interface(0x402, 0x0001, 0x0001, 0);

	/*chip_version > PMIC6397_E1_CID_CODE*/
	/* printk("@@@@@@@@0x128 chip_version=0x%x\n", chip_version); */
	ret = pmic_config_interface(0x128, 0x0000, 0x0060, 0);

	/* VTCXO control */
	/*chip_version > PMIC6397_E1_CID_CODE*/
	/* enter low power mode when suspend */
	/* printk("@@@@@@@@0x400 0x446 chip_version=0x%x\n", chip_version); */
	ret = pmic_config_interface(0x400, 0x4400, 0x6c01, 0);
	ret = pmic_config_interface(0x446, 0x0100, 0x0100, 0);
	pr_debug("[Power/PMIC][pmic_low_power_setting] Done\n");
}

void pmic_setting_depends_rtc(void)
{
	U32 ret = 0;

#if (!defined(CONFIG_POWER_EXT) && defined(CONFIG_MTK_RTC))
	if (crystal_exist_status()) {
#else
	if (0) {
#endif
		/* with 32K */
		ret = pmic_config_interface(ANALDO_CON1, 3, 0x7, 12);	/* [14:12]=3(VTCXO_SRCLK_EN_SEL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 11);	/* [11]=1(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 1);	/* [1]=0(VTCXO_LP_SET), */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 0);	/* [0]=0(VTCXO_LP_SEL), */

		pr_info("[Power/PMIC]" "[pmic_setting_depends_rtc] With 32K. Reg[0x%x]=0x%x\n", ANALDO_CON1,
			upmu_get_reg_value(ANALDO_CON1));
	} else {
		/* without 32K */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 11);	/* [11]=0(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 10);	/* [10]=1(RG_VTCXO_EN), */
		ret = pmic_config_interface(ANALDO_CON1, 3, 0x7, 4);	/* [6:4]=3(VTCXO_SRCLK_MODE_SEL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 0);	/* [0]=1(VTCXO_LP_SEL), */

		pr_info("[Power/PMIC]" "[pmic_setting_depends_rtc] Without 32K. Reg[0x%x]=0x%x\n", ANALDO_CON1,
			upmu_get_reg_value(ANALDO_CON1));
	}
}

int g_gpu_status_bit = 1;

int pmic_get_gpu_status_bit_info(void)
{
	return g_gpu_status_bit;
}
EXPORT_SYMBOL(pmic_get_gpu_status_bit_info);

int get_spm_gpu_status(void)
{
	pr_info("[Power/PMIC]" "[get_spm_gpu_status] wait spm driver service ready\n");

	return 0;
}

void pmic_gpu_power_enable(int power_en)
{
	if (g_gpu_status_bit == 1) {
		pr_info("[Power/PMIC]" "[pmic_gpu_power_enable] gpu is not powered by VRF18_2\n");
	} else {
		if (power_en == 1)
			upmu_set_vgpu_en(1);
		else
			upmu_set_vgpu_en(0);

		pr_info("[Power/PMIC]" "[pmic_gpu_power_enable] Reg[0x%x]=%x\n", VGPU_CON7,
			upmu_get_reg_value(VGPU_CON7));
	}
}
EXPORT_SYMBOL(pmic_gpu_power_enable);

int g_pmic_cid = 0;

struct mt6397_irq_data {
	const char *name;
	unsigned int irq_id;
	 irqreturn_t(*action_fn) (int irq, void *dev_id);
	bool enabled:1;
	bool wake_src:1;
};

static struct mt6397_irq_data mt6397_irqs[] = {
	{
	 .name = "mt6397_pwrkey",
	 .irq_id = RG_INT_STATUS_PWRKEY,
	 .action_fn = pwrkey_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
	{
	 .name = "mt6397_homekey",
	 .irq_id = RG_INT_STATUS_HOMEKEY,
	 .action_fn = homekey_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6397_rtc",
	 .irq_id = RG_INT_STATUS_RTC,
	 .action_fn = rtc_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
#ifdef CONFIG_MTK_ACCDET
	{
	 .name = "mt6397_accdet",
	 .irq_id = RG_INT_STATUS_ACCDET,
	 .action_fn = accdet_int_handler,
	 .enabled = true,
	 },
#endif
#if defined(CONFIG_MTK_BATTERY_PROTECT)
	{
	 .name = "mt6397_bat_l",
	 .irq_id = RG_INT_STATUS_BAT_L,
	 .action_fn = bat_l_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6397_bat_h",
	 .irq_id = RG_INT_STATUS_BAT_H,
	 .action_fn = bat_h_int_handler,
	 .enabled = true,
	 },
#endif
#ifdef CONFIG_PMIC_IMPLEMENT_UNUSED_EVENT_HANDLERS
	{
	 .name = "mt6397_vca15",
	 .irq_id = RG_INT_STATUS_VCA15,
	 .action_fn = vca15_int_handler,
	 },
	{
	 .name = "mt6397_vgpu",
	 .irq_id = RG_INT_STATUS_VGPU,
	 .action_fn = vgpu_int_handler,
	 },
	{
	 .name = "mt6397_pwrkey_rstb",
	 .irq_id = RG_INT_STATUS_PWRKEY_RSTB,
	 .action_fn = pwrkey_rstb_int_handler,
	 },
	{
	 .name = "mt6397_hdmi_sifm",
	 .irq_id = RG_INT_STATUS_HDMI_SIFM,
	 .action_fn = hdmi_sifm_int_handler,
	 },
	{
	 .name = "mt6397_hdmi_cec",
	 .irq_id = RG_INT_STATUS_HDMI_CEC,
	 .action_fn = hdmi_cec_int_handler,
	 },
	{
	 .name = "mt6397_srmvca15",
	 .irq_id = RG_INT_STATUS_VSRMCA15,
	 .action_fn = vsrmca15_int_handler,
	 },
	{
	 .name = "mt6397_vcore",
	 .irq_id = RG_INT_STATUS_VCORE,
	 .action_fn = vcore_int_handler,
	 },
	{
	 .name = "mt6397_vio18",
	 .irq_id = RG_INT_STATUS_VIO18,
	 .action_fn = vio18_int_handler,
	 },
	{
	 .name = "mt6397_vpca7",
	 .irq_id = RG_INT_STATUS_VPCA7,
	 .action_fn = vpca7_int_handler,
	 },
	{
	 .name = "mt6397_vsram7",
	 .irq_id = RG_INT_STATUS_VSRMCA7,
	 .action_fn = vsrmca7_int_handler,
	 },
	{
	 .name = "mt6397_vdrm",
	 .irq_id = RG_INT_STATUS_VDRM,
	 .action_fn = vdrm_int_handler,
	 },
#endif
};

static inline bool mt6397_bandlimit_event(struct mt6397_chip_priv *chip, u32 event)
{
	u64 now;
	u64 last;
	u32 diff;
	struct mt6397_event_stat *irq_stat = &chip->stat[event];
	bool detect = false;

	now = sched_clock();

	if (!irq_stat->count)
		irq_stat->last = now;

	last = irq_stat->last;

	if (time_after64(last + 100 * NSEC_PER_MSEC, now))	/* less than 100ms passed */
		irq_stat->count++;
	else if (time_after64(last + 200 * NSEC_PER_MSEC, now)) {	/* more than 100ms, less than 200ms */
		if (irq_stat->count > 1)
			irq_stat->count--;
	} else if (time_before64(last + 1000 * NSEC_PER_MSEC, now))	/* more than 1000ms */
		irq_stat->count = 1;

	diff = (u32) div_u64(now - last + 500000, 1000000);
	pr_debug("%s: event=%d; count=%d; diff=%d (ms)\n", __func__, event, irq_stat->count, diff);

	irq_stat->last = now;

	if (irq_stat->count > 10) {
		int event_irq = event + chip->irq_base;

		if (!irq_stat->blocked) {
			irq_stat->blocked = true;
			disable_irq(event_irq);
			pr_err("%s: offending PMIC event %d is blocked\n", __func__, event);
		}
		if (test_bit(event, (unsigned long *)&chip->wake_mask)
		    && !irq_stat->wake_blocked) {
			irq_stat->wake_blocked = true;
			irq_set_irq_wake(event_irq, false);
			pr_err("%s: offending PMIC wakeup source %d is blocked\n", __func__, event);
		}
		detect = true;
	}

	return detect;
}

static inline void mt6397_do_handle_events(struct mt6397_chip_priv *chip, u32 events)
{
	int event_hw_irq;
	for (event_hw_irq = __ffs(events); events;
	     events &= ~(1 << event_hw_irq), event_hw_irq = __ffs(events)) {
		int event_irq = chip->irq_base + event_hw_irq;

		pr_debug("%s: event=%d\n", __func__, event_hw_irq);

		if (WARN_ON(mt6397_bandlimit_event(chip, event_hw_irq))) {
			struct irq_data *d;
			pmic_lock();
			d = irq_get_irq_data(event_irq);
			mt6397_irq_mask_locked(d);
			mt6397_irq_ack_locked(d);
			pmic_unlock();
			continue;
		}

		{
			unsigned long flags;
			/* simulate HW irq */
			local_irq_save(flags);
			generic_handle_irq(event_irq);
			local_irq_restore(flags);
		}
	}
}

static inline void mt6397_set_suspended(struct mt6397_chip_priv *chip, bool suspended)
{
	chip->suspended = suspended;
	smp_wmb();		/* matching barrier is in mt6397_is_suspended */
}

static inline bool mt6397_is_suspended(struct mt6397_chip_priv *chip)
{
	smp_rmb();		/* matching barrier is in mt6397_set_suspended */
	return chip->suspended;
}

static inline bool mt6397_do_irq(struct mt6397_chip_priv *chip)
{
	u32 events = mt6397_get_events(chip);

	if (!events)
		return false;

	/* if event happens when it is masked, it is a HW bug,
	 * unless it is a wakeup interrupt */
	if (events & ~(chip->event_mask | chip->wake_mask)) {
		pr_err("%s: PMIC is raising events %08X which are not enabled\n"
		       "\t(mask 0x%lx, wakeup 0x%lx). HW BUG. Stop\n",
		       __func__, events, chip->event_mask, chip->wake_mask);
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6397_get_event_mask_locked(chip), mt6397_get_events(chip));
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6397_get_event_mask_locked(chip), mt6397_get_events(chip));
		BUG();
	}

	mt6397_do_handle_events(chip, events);

	return true;
}

static irqreturn_t mt6397_irq(int irq, void *d)
{
	struct mt6397_chip_priv *chip = (struct mt6397_chip_priv *)d;

	while (!mt6397_is_suspended(chip) && mt6397_do_irq(chip))
		continue;

	return IRQ_HANDLED;
}

static void mt6397_irq_bus_lock(struct irq_data *d)
{
	pmic_lock();
}

static void mt6397_irq_bus_sync_unlock(struct irq_data *d)
{
	pmic_unlock();
}

static void mt6397_irq_chip_suspend(struct mt6397_chip_priv *chip)
{
	pmic_lock();

	chip->saved_mask = mt6397_get_event_mask_locked(chip);
	pr_debug("%s: read event mask=%08X\n", __func__, chip->saved_mask);
	mt6397_set_event_mask_locked(chip, chip->wake_mask);
	pr_debug("%s: write event mask= 0x%lx\n", __func__, chip->wake_mask);

	pmic_unlock();
}

static void mt6397_irq_chip_resume(struct mt6397_chip_priv *chip)
{
	struct mt_wake_event *we = spm_get_wakeup_event();
	u32 events = mt6397_get_events(chip);
	int event = __ffs(events);

	mt6397_set_event_mask(chip, chip->saved_mask);

	if (events && we && we->domain && !strcmp(we->domain, "EINT") && we->code == chip->irq_hw_id) {
		spm_report_wakeup_event(&mt6397_event, event);
		chip->wakeup_event = events;
	}
}

static int mt6397_irq_set_wake_locked(struct irq_data *d, unsigned int on)
{
	struct mt6397_chip_priv *chip = irq_data_get_irq_chip_data(d);
	if (on)
		set_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	else
		clear_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	return 0;
}

static struct irq_chip mt6397_irq_chip = {
	.name = "mt6397-irqchip",
	.irq_ack = mt6397_irq_ack_locked,
	.irq_mask = mt6397_irq_mask_locked,
	.irq_unmask = mt6397_irq_unmask_locked,
	.irq_set_wake = mt6397_irq_set_wake_locked,
	.irq_bus_lock = mt6397_irq_bus_lock,
	.irq_bus_sync_unlock = mt6397_irq_bus_sync_unlock,
};

static int mt6397_irq_init(struct mt6397_chip_priv *chip)
{
	int i;
	int ret = irq_alloc_descs(chip->irq_base, chip->irq_base, chip->num_int, numa_node_id());
	if (ret != chip->irq_base) {
		pr_info("%s: PMIC alloc desc err: %d\n", __func__, ret);
		if (ret >= 0)
			ret = -EBUSY;
		return ret;
	}

	chip->domain = irq_domain_add_legacy(NULL, chip->num_int, chip->irq_base, 0,
					     &irq_domain_simple_ops, chip);
	if (!chip->domain) {
		ret = -EFAULT;
		pr_info("%s: PMIC domain add err: %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < chip->num_int; i++) {
		int idx = i + chip->irq_base;
		irq_set_chip_data(idx, chip);
		irq_set_chip_and_handler(idx, &mt6397_irq_chip, handle_level_irq);
		set_irq_flags(idx, IRQF_VALID);
	}

	mt6397_set_event_mask(chip, 0);
	pr_debug("%s: PMIC: event_mask=%08X; events=%08X\n",
		 __func__, mt6397_get_event_mask(chip), mt6397_get_events(chip));

	ret = request_threaded_irq(chip->irq, NULL, mt6397_irq,
				    IRQF_ONESHOT, mt6397_irq_chip.name, chip);
	if (ret < 0) {
		pr_info("%s: PMIC master irq request err: %d\n", __func__, ret);
		goto err_free_domain;
	}

	irq_set_irq_wake(chip->irq, true);
	return 0;
 err_free_domain:
	irq_domain_remove(chip->domain);
	return ret;
}

static int mt6397_irq_handler_init(struct mt6397_chip_priv *chip)
{
	int i;
	/*AP:
	 * I register all the non-default vectors,
	 * and disable all vectors that were not enabled by original code;
	 * threads are created for all non-default vectors.
	 */
	for (i = 0; i < ARRAY_SIZE(mt6397_irqs); i++) {
		int ret, irq;
		struct mt6397_irq_data *data = &mt6397_irqs[i];

		irq = data->irq_id + chip->irq_base;
		/* irq = irq_find_mapping(chip->domain, RG_INT_STATUS_PWRKEY); */
		ret = request_threaded_irq(irq, NULL, data->action_fn,
					   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, data->name, chip);
		if (ret) {
			pr_info("%s: failed to register irq=%d (%d); name='%s'; err: %d\n",
				__func__, irq, data->irq_id, data->name, ret);
			continue;
		}
		if (!data->enabled)
			disable_irq(irq);
		if (data->wake_src)
			irq_set_irq_wake(irq, 1);
		pr_info("%s: registered irq=%d (%d); name='%s'; enabled: %d\n",
			__func__, irq, data->irq_id, data->name, data->enabled);
	}
	return 0;
}

void pmic_charger_watchdog_enable(bool enable)
{
	int arg = enable ? 1 : 0;

/*	upmu_set_rg_chrwdt_td(tmo);*/
	upmu_set_rg_chrwdt_en(arg);
	upmu_set_rg_chrwdt_int_en(arg);
	upmu_set_rg_chrwdt_wr(arg);
	upmu_set_rg_chrwdt_flag_wr(1);
}

static int mt6397_syscore_suspend(void)
{
	mt6397_irq_chip_suspend(mt6397_chip);
	return 0;
}

static void mt6397_syscore_resume(void)
{
	mt6397_irq_chip_resume(mt6397_chip);
}

static struct syscore_ops mt6397_syscore_ops = {
	.suspend = mt6397_syscore_suspend,
	.resume = mt6397_syscore_resume,
};

static int pmic_mt6397_probe(struct platform_device *pdev)
{
	U32 ret_val = 0;
	struct mtk_pmic_eint *data;
	struct mt6397_chip_priv *chip;
	int ret;

	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);

	pr_debug("[Power/PMIC] ******** MT6397 pmic driver probe!! ********\n");

	/* get PMIC CID */
	ret_val = upmu_get_cid();
	g_pmic_cid = ret_val;
	pr_debug("[Power/PMIC] MT6397 PMIC CID=0x%x\n", ret_val);

	/* VRF18_2 usage protection */
	/* pmic_vrf18_2_usage_protection(); */

	/* enable rtc 32k to pmic */
	rtc_gpio_enable_32k(RTC_GPIO_USER_PMIC);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_INIT_SETTING_V1] Done\n");
	PMIC_CUSTOM_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_CUSTOM_SETTING_V1] Done\n");

	/* pmic low power setting */
	pmic_low_power_setting();

	/* pmic setting with RTC */
	/* pmic_setting_depends_rtc(); */

	upmu_set_rg_pwrkey_int_sel(1);
	upmu_set_rg_homekey_int_sel(1);
	upmu_set_rg_homekey_puen(1);

	pmic_charger_watchdog_enable(false);

	chip = kzalloc(sizeof(struct mt6397_chip_priv), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	data = devm_kzalloc(&pdev->dev, sizeof(struct mtk_pmic_eint), GFP_KERNEL);
	if (!data) {
		devm_kfree(&pdev->dev, chip);
		return -ENOMEM;
	}

	pdev->dev.platform_data = data;
	/* PMIC Interrupt Service */
	/* chip->irq_inf.w = data->irq.w; */

	/* NR_MT_IRQ_LINE for EINT0 */

	chip->irq = mt6397->irq; /* hw irq of EINT */
	chip->irq_hw_id = (int)irqd_to_hwirq(irq_get_irq_data(mt6397->irq)); /* EINT num */
	spm_register_wakeup_event(&pwrkey_wev);
	spm_register_wakeup_event(&rtc_wev);
	spm_register_wakeup_event(&charger_wev);

	chip->irq_base = PMIC_IRQ_BASE;
	chip->num_int = PMIC_INT_MAX_NUM;

	chip->int_con[0] = INT_CON0;
	chip->int_con[1] = INT_CON1;
	chip->int_stat[0] = INT_STATUS0;
	chip->int_stat[1] = INT_STATUS1;

	dev_set_drvdata(chip->dev, chip);

	device_init_wakeup(chip->dev, true);

	pr_debug("[Power/PMIC][PMIC_EINT_SETTING] Done\n");

	ret = mt6397_irq_init(chip);
	if (ret)
		return ret;

	ret = mt6397_irq_handler_init(chip);
	if (ret)
		return ret;

#if defined(CONFIG_POWER_EXT)
	ret_val = pmic_config_interface(0x002E, 0x0010, 0x00FF, 0);
	pr_info("[Power/PMIC][pmic_thread_kthread] add for EVB\n");
#endif

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	low_battery_protect_init();
	low_battery_protect_enable();
#else
	pr_info("[Power/PMIC][PMIC] no define LOW_BATTERY_PROTECT\n");
#endif

	mt6397_chip = chip;
	register_syscore_ops(&mt6397_syscore_ops);

	return 0;
}

static int pmic_mt6397_remove(struct platform_device *dev)
{
	pr_debug("[Power/PMIC] " "******** MT6397 pmic driver remove!! ********\n");

	return 0;
}

static void pmic_mt6397_shutdown(struct platform_device *dev)
{
	pr_debug("[Power/PMIC] " "******** MT6397 pmic driver shutdown!! ********\n");
}

static int pmic_mt6397_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mt6397_chip_priv *chip = dev_get_drvdata(&pdev->dev);
	u32 ret = 0;
	u32 events;

	mt6397_set_suspended(chip, true);
	disable_irq(chip->irq);

	events = mt6397_get_events(chip);
	if (events)
		dev_err(&pdev->dev, "%s: PMIC events: %08X\n", __func__, events);

	/* pr_info("[Power/PMIC] ******** MT6397 pmic driver suspend!! ********\n" ); */
#if 0				/* MT6320 config */
	/* Set PMIC register 0x022A bit[5:4] =00 before system into sleep mode. */
	ret = pmic_config_interface(0x22A, 0x0, 0x3, 4);
	pr_info("[Power/PMIC] Suspend: Reg[0x%x]=0x%x\n", 0x22A, upmu_get_reg_value(0x22A));
#endif

	/* Set PMIC CA7, CA15 TRANS_EN to disable(0x0) before system into sleep mode. */
	ret =
	    pmic_config_interface(VCA15_CON18, 0x0, PMIC_VCA15_VOSEL_TRANS_EN_MASK,
				  PMIC_VCA15_VOSEL_TRANS_EN_SHIFT);
	ret =
	    pmic_config_interface(VPCA7_CON18, 0x0, PMIC_VPCA7_VOSEL_TRANS_EN_MASK,
				  PMIC_VPCA7_VOSEL_TRANS_EN_SHIFT);
	pr_info("[Power/PMIC] Suspend: Reg[0x%x]=0x%x\n", VCA15_CON18,
		upmu_get_reg_value(VCA15_CON18));
	pr_info("[Power/PMIC] Suspend: Reg[0x%x]=0x%x\n", VPCA7_CON18,
		upmu_get_reg_value(VPCA7_CON18));

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);

	pr_info("[Power/PMIC][low bat protect] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		AUXADC_CON6, upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5,
		upmu_get_reg_value(AUXADC_CON5)
	    );
#endif

	return 0;
}

static int pmic_mt6397_resume(struct platform_device *pdev)
{
	struct mt6397_chip_priv *chip = dev_get_drvdata(&pdev->dev);
	u32 ret = 0;
	int i;

	/* pr_info("[Power/PMIC] ******** MT6397 pmic driver resume!! ********\n" ); */

#if 0				/* MT6320 config */
	/* Set PMIC register 0x022A bit[5:4] =01 after system resume. */
	ret = pmic_config_interface(0x22A, 0x1, 0x3, 4);
	pr_info("[Power/PMIC] Resume: Reg[0x%x]=0x%x\n", 0x22A, upmu_get_reg_value(0x22A));
#endif

	/* Set PMIC CA7, CA15 TRANS_EN to falling enable(0x1) after system resume. */
	ret =
	    pmic_config_interface(VCA15_CON18, 0x1, PMIC_VCA15_VOSEL_TRANS_EN_MASK,
				  PMIC_VCA15_VOSEL_TRANS_EN_SHIFT);
	ret =
	    pmic_config_interface(VPCA7_CON18, 0x1, PMIC_VPCA7_VOSEL_TRANS_EN_MASK,
				  PMIC_VPCA7_VOSEL_TRANS_EN_SHIFT);
	pr_info("[Power/PMIC] Resume: Reg[0x%x]=0x%x\n", VCA15_CON18,
		upmu_get_reg_value(VCA15_CON18));
	pr_info("[Power/PMIC] Resume: Reg[0x%x]=0x%x\n", VPCA7_CON18,
		upmu_get_reg_value(VPCA7_CON18));

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
	mdelay(1);

	if (g_low_battery_level == 1) {
		lbat_min_en_setting(1);
		lbat_max_en_setting(1);
	} else if (g_low_battery_level == 2) {
		/* lbat_min_en_setting(0); */
		lbat_max_en_setting(1);
	} else {		/* 0 */
		lbat_min_en_setting(1);
		/* lbat_max_en_setting(0); */
	}

	pr_info
	    ("[Power/PMIC][low bat protect] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, g_low_battery_level=%d\n",
	     AUXADC_CON6, upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5,
	     upmu_get_reg_value(AUXADC_CON5), g_low_battery_level);
#endif

	mt6397_set_suspended(chip, false);

	/* amnesty for all blocked events on resume */
	for (i = 0; i < ARRAY_SIZE(chip->stat); ++i) {
		if (chip->stat[i].blocked) {
			chip->stat[i].blocked = false;
			enable_irq(i + chip->irq_base);
			pr_debug("%s: restored blocked PMIC event%d\n", __func__, i);
		}
		if (chip->stat[i].wake_blocked) {
			chip->stat[i].wake_blocked = false;
			irq_set_irq_wake(i + chip->irq_base, true);
			pr_debug("%s: restored blocked PMIC wake src %d\n", __func__, i);
		}
	}

	if (chip->wakeup_event) {
		mt6397_do_handle_events(chip, chip->wakeup_event);
		chip->wakeup_event = 0;
	}

	enable_irq(chip->irq);

	return 0;
}

static const struct of_device_id mt6397_pmic_id[] = {
	{   .compatible = "mediatek,pmic-mt6397" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6397_pmic_id);

static struct platform_driver pmic_mt6397_driver = {
	.probe = pmic_mt6397_probe,
	.remove = pmic_mt6397_remove,
	.shutdown = pmic_mt6397_shutdown,
	/* #ifdef CONFIG_PM */
	.suspend = pmic_mt6397_suspend,
	.resume = pmic_mt6397_resume,
	/* #endif */
	.driver = {
		.name = "pmic-mt6397",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6397_pmic_id),
	},
};

static int pmic_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int pmic_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long pmic_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* Add ioctl here. */
	int ret = 0;
	int pmic_dcxo_data[2];
	bool flag = TRUE;
	pr_info("[Power/PMIC] cmd = %d", cmd);
	switch (cmd) {
	case PMIC_DCXO_WRITE:
		{
			if (copy_from_user(pmic_dcxo_data, (void *)arg, sizeof(pmic_dcxo_data)))
				return -1;

			pr_info("[Power/PMIC] data[0] =%d,data[1] = %d",
				pmic_dcxo_data[0], pmic_dcxo_data[1]);
			if (pmic_dcxo_data[0] == 0x1)
				flag = FALSE;

			pr_info("[Power/PMIC] flag =%d,data[1] = %d", flag, pmic_dcxo_data[1]);
			hal_write_dcxo_c2((u16) pmic_dcxo_data[1], flag);
			ret = 0;
			break;
		}
	default:
		ret = -1;
	}
	return ret;
}


static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pmic_dev_ioctl,
	.open = pmic_dev_open,
	.release = pmic_dev_release,
};

static struct miscdevice pmic_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtk_pmic",
	.fops = &dev_fops,
};


/* ============================================================================== */
/* PMIC6397 device driver */
/* ============================================================================== */
static int mt_pmic_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_debug("[Power/PMIC] ******** mt_pmic_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_access);

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPCA7_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSRMCA7_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCA15_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSRMCA15_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRM_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC_3V3_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMIO_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMAF_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP4_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP5_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP6_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIBR_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VA28_STATUS);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_STATUS);

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VPCA7_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSRMCA7_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCA15_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VSRMCA15_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VCORE_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VDRM_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VIO18_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BUCK_VGPU_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIO28_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VUSB_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMC_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VMCH_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VEMC_3V3_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMD_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMIO_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMAF_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP4_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP5_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VGP6_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VIBR_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VRTC_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VTCXO_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VA28_VOLTAGE);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_LDO_VCAMA_VOLTAGE);

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_level);
#endif

	return 0;
}

struct platform_device mt_pmic_device = {
	.name = "mt-pmic",
	.id = -1,
};

static struct platform_driver mt_pmic_driver = {
	.probe = mt_pmic_probe,
	.driver = {
		   .name = "mt-pmic",
		   },
};

/* ============================================================================== */
/* PMIC6397 mudule init/exit */
/* ============================================================================== */
static int __init pmic_mt6397_init(void)
{
	int ret;
	ret = platform_driver_register(&pmic_mt6397_driver);
	if (ret) {
		pr_info("[Power/PMIC] " "****[pmic_mt6397_init] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* PMIC user space access interface */
	ret = platform_device_register(&mt_pmic_device);
	if (ret) {
		pr_info("[Power/PMIC] " "****[pmic_mt6397_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&mt_pmic_driver);
	if (ret) {
		pr_info("[Power/PMIC] " "****[pmic_mt6397_init] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* PMIC file operations */
	ret = misc_register(&pmic_miscdev);
	if (ret < 0) {
		pr_info("[Power/PMIC] " "****[pmic_mt6397_init] MISC Register fail !!\n");
		return ret;
	}

	pr_debug("[Power/PMIC] " "****[pmic_mt6397_init] Initialization : DONE !!\n");

	return 0;
}

static void __exit pmic_mt6397_exit(void)
{
	misc_deregister(&pmic_miscdev);
}
fs_initcall(pmic_mt6397_init);

/* module_init(pmic_mt6397_init); */
module_exit(pmic_mt6397_exit);

MODULE_AUTHOR("Tank Hung");
MODULE_DESCRIPTION("MT6397 PMIC Device Driver");
MODULE_LICENSE("GPL");
