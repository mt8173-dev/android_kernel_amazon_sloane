/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_gpufreq.h"
/* #include "mach/upmu_common.h" */
#include "mach/sync_write.h"

#include "mach/mt_freqhopping.h"
/* path ??? */
#include "mach/pmic_mt6331_6332_sw.h"


/**************************************************
* Define register write function
***************************************************/
#define mt_gpufreq_reg_write(val, addr)        mt_reg_sync_writel((val), ((void *)addr))

#define OPPS_ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

/***************************
* Operate Point Definition
****************************/
#define GPUOP(khz, volt, idx)       \
{                           \
    .gpufreq_khz = khz,     \
    .gpufreq_volt = volt,   \
    .gpufreq_idx = idx,   \
}

#define GPU_DVFS_VOLT1     (112500)	/* mV x 100 */
#define GPU_DVFS_VOLT2     (100000)	/* mV x 100 */
#define GPU_DVFS_VOLT3     (90000)	/* mV x 100 */

#define GPU_DVFS_CTRL_VOLT     (2)

/***************************
* debug message
****************************/
#define dprintk(fmt, args...)                                           \
do {                                                                    \
    if (mt_gpufreq_debug) {                                             \
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", fmt, ##args);   \
    }                                                                   \
} while (0)

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_gpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = NULL,
	.resume = NULL,
};
#endif

static sampler_func g_pFreqSampler;
static sampler_func g_pVoltSampler;


/***************************
* MT6595 GPU Frequency Table
****************************/
static struct mt_gpufreq_table_info mt6595_gpufreqs_0[] = {
	GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 0),
	GPUOP(GPU_DVFS_FREQ3, GPU_DVFS_VOLT1, 1),
	GPUOP(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
	GPUOP(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 3),
	GPUOP(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 4),
};

/***************************
* MT6595 GPU Power Table
****************************/
/* power ??? */
static struct mt_gpufreq_power_table_info mt_gpufreqs_golden_power[] = {
	{.gpufreq_khz = GPU_DVFS_FREQ1, .gpufreq_power = 824},
	{.gpufreq_khz = GPU_DVFS_FREQ2, .gpufreq_power = 726},
	{.gpufreq_khz = GPU_DVFS_FREQ3, .gpufreq_power = 628},
	{.gpufreq_khz = GPU_DVFS_FREQ4, .gpufreq_power = 391},
	{.gpufreq_khz = GPU_DVFS_FREQ5, .gpufreq_power = 314},
	{.gpufreq_khz = GPU_DVFS_FREQ6, .gpufreq_power = 277},
};

/***************************
* external function
****************************/
/* extern int spm_dvfs_ctrl_volt(u32 value); */
extern unsigned int mt_get_mfgclk_freq(void);
/* extern unsigned int ckgen_meter(int val); */

/**************************
* enable GPU DVFS count
***************************/
static int g_gpufreq_dvfs_disable_count;

static unsigned int g_cur_gpu_freq = 455000;	/* ??? */
static unsigned int g_cur_gpu_volt = 100000;	/* ??? */
static unsigned int g_cur_gpu_idx = 0xFF;	/* ??? */

static unsigned int g_cur_freq_init_keep;
/* static unsigned int g_volt_set_init_step_1 = 0; */
/* static unsigned int g_volt_set_init_step_2 = 0; */
/* static unsigned int g_volt_set_init_step_3 = 0; */
/* static unsigned int g_freq_new_init_keep = 0; */
/* static unsigned int g_volt_new_init_keep = 0; */

static bool mt_gpufreq_ready;

/* In default settiing, freq_table[0] is max frequency, freq_table[num-1] is min frequency,*/
static unsigned int g_gpufreq_max_id;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id;
static unsigned int g_limited_min_id;

static bool mt_gpufreq_debug;
static bool mt_gpufreq_pause;
static bool mt_gpufreq_keep_max_frequency_state;
static bool mt_gpufreq_keep_opp_frequency_state;
static unsigned int mt_gpufreq_keep_opp_frequency;
static unsigned int mt_gpufreq_keep_opp_index;
static bool mt_gpufreq_fixed_freq_volt_state;
static unsigned int mt_gpufreq_fixed_frequency;
static unsigned int mt_gpufreq_fixed_voltage;

static unsigned int mt_gpufreq_volt_enable;
static unsigned int mt_gpufreq_volt_enable_state;

/* static DEFINE_SPINLOCK(mt_gpufreq_lock); */
static DEFINE_MUTEX(mt_gpufreq_lock);

static unsigned int mt_gpufreqs_num;
static struct mt_gpufreq_table_info *mt_gpufreqs;
static struct mt_gpufreq_power_table_info *mt_gpufreqs_power;
/* static struct mt_gpufreq_power_table_info *mt_gpufreqs_default_power; */

static void mt_gpu_clock_switch(unsigned int freq_new);
static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new);


/******************************
* Extern Function Declaration
*******************************/
extern int mtk_gpufreq_register(struct mt_gpufreq_power_table_info *freqs, int num);	/* ??? */

/**************************************
* Convert pmic wrap register to voltage
***************************************/
static unsigned int mt_gpufreq_pmic_wrap_to_volt(unsigned int pmic_wrap_value)
{
	unsigned int volt = 0;

	volt = (pmic_wrap_value * 625) + 70000;

	dprintk("Power/GPU_DVFS", "mt_gpufreq_pmic_wrap_to_volt, volt = %d\n", volt);

	if (volt > 149375) {	/* 1.49375V */
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "[ERROR]mt_gpufreq_pmic_wrap_to_volt, volt > 1.49375v!\n");
		return 149375;
	}

	return volt;
}

/**************************************
* Convert voltage to pmic wrap register
***************************************/
static unsigned int mt_gpufreq_volt_to_pmic_wrap(unsigned int volt)
{
	unsigned int RegVal = 0;

	RegVal = (volt - 70000) / 625;

	dprintk("mt_gpufreq_volt_to_pmic_wrap, RegVal = %d\n", RegVal);

	if (RegVal > 0x7F) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "[ERROR]mt_gpufreq_volt_to_pmic_wrap, RegVal > 0x7F!\n");
		return 0x7F;
	}

	return RegVal;
}

/* Set frequency and voltage at driver probe function */
static void mt_gpufreq_set_initial(unsigned int index)
{
	/* unsigned long flags; */

	mutex_lock(&mt_gpufreq_lock);

	mt_gpu_volt_switch(90000, mt_gpufreqs[index].gpufreq_volt);

	mt_gpu_clock_switch(mt_gpufreqs[index].gpufreq_khz);

	g_cur_gpu_freq = mt_gpufreqs[index].gpufreq_khz;
	g_cur_gpu_volt = mt_gpufreqs[index].gpufreq_volt;
	g_cur_gpu_idx = mt_gpufreqs[index].gpufreq_idx;

	mutex_unlock(&mt_gpufreq_lock);
}

/* Set VGPU enable/disable when GPU clock be switched on/off */
unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	/* unsigned long flags; */
	unsigned int delay = 0;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if (enable == 1)
		pmic_config_interface(0x02AA, 0x1, 0x1, 0x0);	/* Set VDVFS13_EN[0] */
	else
		pmic_config_interface(0x02AA, 0x0, 0x1, 0x0);	/* Set VDVFS13_EN[0] */

	/* ??? variable */
	mt_gpufreq_volt_enable_state = enable;

	dprintk("mt_gpufreq_voltage_enable_set, enable = %x\n", enable);

	/* ??? delay time */
	delay = (g_cur_gpu_volt / 1250) + 26;

	dprintk("mt_gpufreq_voltage_enable_set, delay = %d\n", delay);

	udelay(delay);

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_enable_set);


/* Set voltage because PTP-OD modified voltage table by PMIC wrapper */
unsigned int mt_gpufreq_voltage_set_by_ptpod(unsigned int pmic_volt[], unsigned int array_size)
{
#if 1
	int i;			/* , idx; */
	/* unsigned long flags; */
	unsigned volt = 0;

	mutex_lock(&mt_gpufreq_lock);

	for (i = 0; i < array_size; i++) {
		volt = mt_gpufreq_pmic_wrap_to_volt(pmic_volt[i]);
		mt_gpufreqs[i].gpufreq_volt = volt;
		dprintk("mt_gpufreqs[%d].gpufreq_volt = %x\n", i, mt_gpufreqs[i].gpufreq_volt);
	}

	mt_gpu_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_idx].gpufreq_volt);

	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_idx].gpufreq_volt;

	mutex_unlock(&mt_gpufreq_lock);

#else
	int i;			/* , idx; */
	unsigned long flags;
	unsigned int PMIC_WRAP_DVFS_WDATA_array[8] =
	    { PMIC_WRAP_DVFS_WDATA0, PMIC_WRAP_DVFS_WDATA1, PMIC_WRAP_DVFS_WDATA2,
		PMIC_WRAP_DVFS_WDATA3, PMIC_WRAP_DVFS_WDATA4, PMIC_WRAP_DVFS_WDATA5,
		PMIC_WRAP_DVFS_WDATA6, PMIC_WRAP_DVFS_WDATA7
	};

	spin_lock_irqsave(&mt_gpufreq_lock, flags);

	/* Update voltage setting by PTPOD request. */
	for (i = 0; i < array_size; i++) {
		mt_cpufreq_reg_write(pmic_volt[i], PMIC_WRAP_DVFS_WDATA_array[i]);
	}

	/* For SPM voltage setting in deep idle. */
	/* Need to sync PMIC_WRAP_DVFS_WDATA in mt_cpufreq_pdrv_probe() */
	if ((g_cpufreq_get_ptp_level >= 0) && (g_cpufreq_get_ptp_level <= 4)) {
		mt_cpufreq_reg_write(pmic_volt[2], PMIC_WRAP_DVFS_WDATA_array[5]);
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_reg_write(pmic_volt[3], PMIC_WRAP_DVFS_WDATA_array[6]);
#endif
	} else if ((g_cpufreq_get_ptp_level >= 5) && (g_cpufreq_get_ptp_level <= 6)) {
		mt_cpufreq_reg_write(pmic_volt[1], PMIC_WRAP_DVFS_WDATA_array[5]);
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_reg_write(pmic_volt[2], PMIC_WRAP_DVFS_WDATA_array[6]);
#endif
	} else {
		mt_cpufreq_reg_write(pmic_volt[2], PMIC_WRAP_DVFS_WDATA_array[5]);
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_reg_write(pmic_volt[3], PMIC_WRAP_DVFS_WDATA_array[6]);
#endif
	}

	for (i = 0; i < array_size; i++) {
		mt_cpufreq_pmic_volt[i] = pmic_volt[i];
		dprintk("mt_cpufreq_pmic_volt[%d] = %x\n", i, mt_cpufreq_pmic_volt[i]);
	}

	/* For SPM voltage setting in deep idle. */
	if ((g_cpufreq_get_ptp_level >= 0) && (g_cpufreq_get_ptp_level <= 4)) {
		mt_cpufreq_pmic_volt[5] = pmic_volt[2];
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_pmic_volt[6] = pmic_volt[3];
#endif
	} else if ((g_cpufreq_get_ptp_level >= 5) && (g_cpufreq_get_ptp_level <= 6)) {
		mt_cpufreq_pmic_volt[5] = pmic_volt[1];
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_pmic_volt[6] = pmic_volt[2];
#endif
	} else {
		mt_cpufreq_pmic_volt[5] = pmic_volt[2];
#ifdef MT_DVFS_LOW_VOLTAGE_SUPPORT
		mt_cpufreq_pmic_volt[6] = pmic_volt[3];
#endif
	}

	dprintk
	    ("mt_cpufreq_voltage_set_by_ptpod: Set voltage directly by PTP-OD request! mt_cpufreq_ptpod_voltage_down = %d\n",
	     mt_cpufreq_ptpod_voltage_down);

	mt_gpu_volt_switch(g_cur_cpufreq_volt);

	spin_unlock_irqrestore(&mt_gpufreq_lock, flags);
#endif
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_set_by_ptpod);

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return mt_gpufreqs_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

unsigned int mt_gpufreq_get_frequency_by_level(unsigned int num)
{
	if (num < mt_gpufreqs_num) {
		dprintk("Power/GPU_DVFS",
			"mt_gpufreq_get_frequency_by_level:num = %d, frequency= %d\n", num,
			mt_gpufreqs[num].gpufreq_khz);
		return mt_gpufreqs[num].gpufreq_khz;
	}


	dprintk("Power/GPU_DVFS",
		"mt_gpufreq_get_frequency_by_level:num = %d, NOT found! return 0!\n", num);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_frequency_by_level);


static void mt_setup_gpufreqs_power_table(int num)
{
	int i = 0, j = 0;

	mt_gpufreqs_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);
	if (mt_gpufreqs_power == NULL) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "GPU power table memory allocation fail\n");
		return;
	}

	for (i = 0; i < num; i++) {
		mt_gpufreqs_power[i].gpufreq_khz = mt_gpufreqs[i].gpufreq_khz;

		for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
			if (mt_gpufreqs[i].gpufreq_khz == mt_gpufreqs_golden_power[j].gpufreq_khz) {
				mt_gpufreqs_power[i].gpufreq_power =
				    mt_gpufreqs_golden_power[j].gpufreq_power;
				break;
			}
		}

		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreqs_power[%d].gpufreq_khz = %u\n", i,
			    mt_gpufreqs_power[i].gpufreq_khz);
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreqs_power[%d].gpufreq_power = %u\n", i,
			    mt_gpufreqs_power[i].gpufreq_power);
	}

	/* enable ??? */
#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_power, num);
#endif
}

/***********************************************
* register frequency table to gpufreq subsystem
************************************************/
static int mt_setup_gpufreqs_table(struct mt_gpufreq_table_info *freqs, int num)
{
	int i = 0;

	mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	if (mt_gpufreqs == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs[i].gpufreq_idx = freqs[i].gpufreq_idx;

		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_khz = %u\n", i,
			    freqs[i].gpufreq_khz);
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_volt = %u\n", i,
			    freqs[i].gpufreq_volt);
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "freqs[%d].gpufreq_idx = %u\n", i,
			    freqs[i].gpufreq_idx);
	}

	mt_gpufreqs_num = num;

	/* Initial frequency and voltage was already set in mt_gpufreq_set_initial() */
#if 0				/* 1 */
	g_cur_gpu_freq = freqs[0].gpufreq_khz;	/* ??? */
	g_cur_gpu_volt = freqs[0].gpufreq_volt;
#endif
	g_limited_max_id = 0;
	g_limited_min_id = mt_gpufreqs_num - 1;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_setup_gpufreqs_table, g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n",
		    g_cur_gpu_freq, g_cur_gpu_volt);

	mt_setup_gpufreqs_power_table(num);

	return 0;
}



/**************************************
* check if maximum frequency is needed
***************************************/
static int mt_gpufreq_keep_max_freq(unsigned int freq_old, unsigned int freq_new)
{
	if (mt_gpufreq_keep_max_frequency_state == true)
		return 1;

	return 0;
}

#if 1

/*****************************
* set GPU DVFS status
******************************/
int mt_gpufreq_state_set(int enabled)
{
	if (enabled) {
		if (!mt_gpufreq_pause) {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "gpufreq already enabled\n");
			return 0;
		}

	/*****************
	* enable GPU DVFS
	******************/
		g_gpufreq_dvfs_disable_count--;
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "enable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
			    g_gpufreq_dvfs_disable_count);

	/***********************************************
	* enable DVFS if no any module still disable it
	************************************************/
		if (g_gpufreq_dvfs_disable_count <= 0) {
			mt_gpufreq_pause = false;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "someone still disable gpufreq, cannot enable it\n");
		}
	} else {
	/******************
	* disable GPU DVFS
	*******************/
		g_gpufreq_dvfs_disable_count++;
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
			    g_gpufreq_dvfs_disable_count);

		if (mt_gpufreq_pause) {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "gpufreq already disabled\n");
			return 0;
		}

		mt_gpufreq_pause = true;
	}

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_state_set);
#endif

/* FH API ??? */
static void mt_gpu_clock_switch(unsigned int freq_new)
{
	unsigned int freq_meter = 0;
	/* unsigned int freq_meter_new = 0; */

	switch (freq_new) {
	case GPU_DVFS_FREQ1:	/* 695500 KHz */
		mt_dfs_mmpll(2782000);
		break;
	case GPU_DVFS_FREQ2:	/* 598000 KHz */
		mt_dfs_mmpll(2392000);
		break;
	case GPU_DVFS_FREQ3:	/* 494000 KHz */
		mt_dfs_mmpll(1976000);
		break;
	case GPU_DVFS_FREQ4:	/* 396500 KHz */
		mt_dfs_mmpll(1586000);
		break;
	case GPU_DVFS_FREQ5:	/* 299000 KHz */
		mt_dfs_mmpll(1196000);
		break;
	case GPU_DVFS_FREQ6:	/* 253500 KHz */
		mt_dfs_mmpll(1014000);
		break;
	default:
		if (mt_gpufreq_fixed_freq_volt_state == true) {
			mt_dfs_mmpll(freq_new * 4);
		}
		break;
	}

	freq_meter = mt_get_mfgclk_freq();	/* ??? */
	/* freq_meter_new = ckgen_meter(9); */

	if (NULL != g_pFreqSampler) {
		g_pFreqSampler(freq_new);
	}

	dprintk("mt_gpu_clock_switch, freq_meter = %d\n", freq_meter);
	/* dprintk("mt_gpu_clock_switch, freq_meter_new = %d\n", freq_meter_new); */
	dprintk("mt_gpu_clock_switch, freq_new = %d\n", freq_new);
}


static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int RegVal = 0;
	unsigned int delay = 0;
	unsigned int RegValGet = 0;

	dprintk("mt_gpu_volt_switch, volt_new = %d\n", volt_new);

	/* mt_gpufreq_reg_write(0x02B0, PMIC_WRAP_DVFS_ADR2); */

	RegVal = mt_gpufreq_volt_to_pmic_wrap(volt_new);

#if 1

	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, RegVal);

	mt_cpufreq_apply_pmic_cmd(IDX_NM_VGPU);

	pmic_read_interface(0x02B0, &RegValGet, 0x7F, 0x0);	/* Get VDVFS13_EN[0] */
	dprintk("Power/GPU_DVFS", "0x02B0 = %d\n", RegValGet);

#else
	mt_gpufreq_reg_write(RegVal, PMIC_WRAP_DVFS_WDATA2);

	spm_dvfs_ctrl_volt(GPU_DVFS_CTRL_VOLT);

#endif

	if (volt_new > volt_old) {
		delay = ((volt_new - volt_old) / 1250) + 26;
	} else {
		delay = ((volt_old - volt_new) / 1250) + 26;
	}

	dprintk("mt_gpu_volt_switch, delay = %d\n", delay);

	udelay(delay);

	if (NULL != g_pVoltSampler) {
		g_pVoltSampler(volt_new);
	}

}


/*****************************************
* frequency ramp up and ramp down handler
******************************************/
/***********************************************************
* [note]
* 1. frequency ramp up need to wait voltage settle
* 2. frequency ramp down do not need to wait voltage settle
************************************************************/
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new, unsigned int volt_old,
			   unsigned int volt_new)
{
	if (freq_new > freq_old) {
		/* if(volt_old != volt_new) // ??? */
		/* { */
		mt_gpu_volt_switch(volt_old, volt_new);
		/* } */

		mt_gpu_clock_switch(freq_new);
	} else {
		mt_gpu_clock_switch(freq_new);

		/* if(volt_old != volt_new) */
		/* { */
		mt_gpu_volt_switch(volt_old, volt_new);
		/* } */
	}

	g_cur_gpu_freq = freq_new;
	g_cur_gpu_volt = volt_new;

}


/**********************************
* gpufreq target callback function
***********************************/
/*************************************************
* [note]
* 1. handle frequency change request
* 2. call mt_gpufreq_set to set target frequency
**************************************************/
unsigned int mt_gpufreq_target(unsigned int idx)
{
	/* unsigned long flags; */
	unsigned long target_freq, target_volt, target_idx;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if (mt_gpufreq_volt_enable_state == 0) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_volt_enable_state == 0! return\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

    /**********************************
    * look up for the target GPU OPP
    ***********************************/
	target_freq = mt_gpufreqs[idx].gpufreq_khz;
	target_volt = mt_gpufreqs[idx].gpufreq_volt;
	target_idx = idx;

    /**********************************
    * Check if need to keep max frequency
    ***********************************/
	if (mt_gpufreq_keep_max_freq(g_cur_gpu_freq, target_freq)) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		target_idx = mt_gpufreqs[g_gpufreq_max_id].gpufreq_idx;
		dprintk("Keep MAX frequency %d !\n", target_freq);
	}
#if 0
    /****************************************************
    * If need to raise frequency, raise to max frequency
    *****************************************************/
	if (target_freq > g_cur_freq) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		dprintk("Need to raise frequency, raise to MAX frequency %d !\n", target_freq);
	}
#endif

    /************************************************
    * If /proc command keep opp frequency.
    *************************************************/
	if (mt_gpufreq_keep_opp_frequency_state == true) {
		target_freq = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz;
		target_volt = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_idx;
		dprintk("Keep opp! opp frequency %d, opp voltage %d, opp idx %d\n", target_freq,
			target_volt, target_idx);
	}

    /************************************************
    * If /proc command fix the frequency.
    *************************************************/
	if (mt_gpufreq_fixed_freq_volt_state == true) {
		target_freq = mt_gpufreq_fixed_frequency;
		target_volt = mt_gpufreq_fixed_voltage;
		target_idx = 0;
		dprintk("Fixed! fixed frequency %d, fixed voltage %d\n", target_freq, target_volt);
	}

    /************************************************
    * Thermal limit
    *************************************************/
	if (g_limited_max_id != 0) {
		if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz) {
		/*********************************************
		* target_freq > limited_freq, need to adjust
		**********************************************/
			target_freq = mt_gpufreqs[g_limited_max_id].gpufreq_khz;
			target_volt = mt_gpufreqs[g_limited_max_id].gpufreq_volt;
			target_idx = mt_gpufreqs[g_limited_max_id].gpufreq_idx;
			dprintk("Limit! Thermal limit frequency %d\n",
				mt_gpufreqs[g_limited_max_id].gpufreq_khz);
		}
	}

    /************************************************
    * target frequency == current frequency, skip it
    *************************************************/
	if (g_cur_gpu_freq == target_freq) {
		mutex_unlock(&mt_gpufreq_lock);
		dprintk("GPU frequency from %d KHz to %d KHz (skipped) due to same frequency\n",
			g_cur_gpu_freq, target_freq);
		return 0;
	}

	dprintk("GPU current frequency %d KHz, target frequency %d KHz\n", g_cur_gpu_freq,
		target_freq);

    /******************************
    * set to the target frequency
    *******************************/
	mt_gpufreq_set(g_cur_gpu_freq, target_freq, g_cur_gpu_volt, target_volt);

	g_cur_gpu_idx = target_idx;

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_target);


#if 1
/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;
	unsigned int found = 0;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_thermal_protect, GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return;
	}

	if (mt_gpufreqs_num == 0) {
		mutex_unlock(&mt_gpufreq_lock);
		return;
	}

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_thermal_protect, limited_power = %d", limited_power);

	if (limited_power == 0) {
		g_limited_max_id = 0;
	} else {
		/* g_limited_max_id = mt_gpufreqs_num - 1; */

		for (i = 0; i < ARRAY_SIZE(mt_gpufreqs_golden_power); i++) {
			if (mt_gpufreqs_golden_power[i].gpufreq_power <= limited_power) {
				limited_freq = mt_gpufreqs_golden_power[i].gpufreq_khz;
				found = 1;
				break;
			}
		}

		if (found == 0) {
			limited_freq = mt_gpufreqs[mt_gpufreqs_num - 1].gpufreq_khz;
		}

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
				g_limited_max_id = i;
				break;
			}
		}
	}

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "Thermal limit frequency upper bound to id = %d, frequency = %d\n",
		    g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);
	/* mt_gpufreq_target(g_limited_max_id); ??? */

	mutex_unlock(&mt_gpufreq_lock);

	return;
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);
#endif

#if 1
/************************************************
* return current GPU frequency index
*************************************************/
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	dprintk("current GPU frequency index is %d\n", g_cur_gpu_idx);
	return g_cur_gpu_idx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/************************************************
* return current GPU frequency
*************************************************/
unsigned int mt_gpufreq_get_cur_freq(void)
{
	dprintk("current GPU frequency is %d MHz\n", g_cur_gpu_freq / 1000);
	return g_cur_gpu_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/************************************************
* return current GPU voltage
*************************************************/
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return g_cur_gpu_volt;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

#endif

#if 1

/************************************************
* register / unregister set GPU freq CB
*************************************************/
void mt_gpufreq_setfreq_registerCB(sampler_func pCB)
{
	g_pFreqSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setfreq_registerCB);

/************************************************
* register / unregister set GPU volt CB
*************************************************/
void mt_gpufreq_setvolt_registerCB(sampler_func pCB)
{
	g_pVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setvolt_registerCB);

#endif

#if 1

/***************************
* show current debug status
****************************/
static int mt_gpufreq_debug_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_debug)
		seq_puts(m, "gpufreq debug enabled\n");
	else
		seq_puts(m, "gpufreq debug disabled\n");

	return 0;
}

/***********************
* enable debug message
************************/
static ssize_t mt_gpufreq_debug_write(struct file *file, const char __user *buffer, size_t count,
				      loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &debug) == 1) {
		if (debug == 0) {
			mt_gpufreq_debug = 0;
		} else if (debug == 1) {
			mt_gpufreq_debug = 1;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	}

	return count;
}

/****************************
* show current limited power
*****************************/
static int mt_gpufreq_limited_power_read(struct seq_file *m, void *v)
{

	seq_printf(m, "g_limited_max_id = %d, thermal want to limit frequency = %d\n",
		   g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	return 0;
}

/**********************************
* limited power for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_power_write(struct file *file, const char __user *buffer,
					      size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%u", &power) == 1) {
		mt_gpufreq_thermal_protect(power);
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! please provide the maximum limited power\n");
	}

	return count;
}


/******************************
* show current GPU DVFS stauts
*******************************/
static int mt_gpufreq_state_read(struct seq_file *m, void *v)
{
	if (!mt_gpufreq_pause)
		seq_puts(m, "GPU DVFS enabled\n");
	else
		seq_puts(m, "GPU DVFS disabled\n");

	return 0;
}

/****************************************
* set GPU DVFS stauts by sysfs interface
*****************************************/
static ssize_t mt_gpufreq_state_write(struct file *file, const char __user *buffer, size_t count,
				      loff_t *data)
{
	char desc[32];
	int len = 0;

	int enabled = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &enabled) == 1) {
		if (enabled == 1) {
			mt_gpufreq_keep_max_frequency_state = false;
			mt_gpufreq_state_set(1);
		} else if (enabled == 0) {
			/* Keep MAX frequency when GPU DVFS disabled. */
			mt_gpufreq_keep_max_frequency_state = true;
			mt_gpufreq_target(g_gpufreq_max_id);
			mt_gpufreq_state_set(0);
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! argument should be \"1\" or \"0\"\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! argument should be \"1\" or \"0\"\n");
	}

	return count;
}

/********************
* show GPU OPP table
*********************/
static int mt_gpufreq_opp_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		seq_printf(m, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
		seq_printf(m, "idx = %d\n", mt_gpufreqs[i].gpufreq_idx);

#if 0
		for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
			if (mt_gpufreqs_golden_power[j].gpufreq_khz == mt_gpufreqs[i].gpufreq_khz) {
				p += sprintf(p, "power = %d\n",
					     mt_gpufreqs_golden_power[j].gpufreq_power);
				break;
			}
		}
#endif
	}

	return 0;
}

/********************
* show GPU power table
*********************/
static int mt_gpufreq_power_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_khz = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_khz);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_power = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_power);
	}

	return 0;
}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_opp_freq_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_keep_opp_frequency_state) {
		seq_puts(m, "gpufreq keep opp frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq keep opp frequency disabled\n");

	return 0;

}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_opp_freq_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int i = 0;
	int fixed_freq = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &fixed_freq) == 1) {
		if (fixed_freq == 0) {
			mt_gpufreq_keep_opp_frequency_state = false;
		} else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (fixed_freq == mt_gpufreqs[i].gpufreq_khz) {
					mt_gpufreq_keep_opp_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1) {
				mt_gpufreq_keep_opp_frequency_state = true;
				mt_gpufreq_keep_opp_frequency = fixed_freq;

				mt_gpufreq_target(mt_gpufreq_keep_opp_index);
			}

		}

	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
	}

	return count;

}

/********************
* show variable dump
*********************/
static int mt_gpufreq_var_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_cur_gpu_freq = %d, g_cur_gpu_volt = %d, g_cur_gpu_idx = %d\n",
		   g_cur_gpu_freq, g_cur_gpu_volt, g_cur_gpu_idx);
	seq_printf(m, "g_limited_max_id = %d\n", g_limited_max_id);

	return 0;
}

/***************************
* show current voltage enable status
****************************/
static int mt_gpufreq_volt_enable_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_volt_enable)
		seq_puts(m, "gpufreq voltage enabled\n");
	else
		seq_puts(m, "gpufreq voltage disabled\n");

	return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_volt_enable_write(struct file *file, const char __user *buffer,
					    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &enable) == 1) {
		if (enable == 0) {
			mt_gpufreq_voltage_enable_set(0);
			mt_gpufreq_volt_enable = false;
		} else if (enable == 1) {
			mt_gpufreq_voltage_enable_set(1);
			mt_gpufreq_volt_enable = true;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
	}

	return count;

}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_fixed_freq_volt_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_fixed_freq_volt_state) {
		seq_puts(m, "gpufreq fixed frequency enabled\n");
		seq_printf(m, "fixed frequency = %d\n", mt_gpufreq_fixed_frequency);
		seq_printf(m, "fixed voltage = %d\n", mt_gpufreq_fixed_voltage);
	} else
		seq_puts(m, "gpufreq fixed frequency disabled\n");

	return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_fixed_freq_volt_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int fixed_freq = 0;
	int fixed_volt = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		if ((fixed_freq == 0) || (fixed_volt == 0)) {
			mt_gpufreq_fixed_freq_volt_state = false;
		} else {
			if ((fixed_freq >= GPU_DVFS_FREQ6) && (fixed_freq <= GPU_DVFS_FREQ1)) {
				mt_gpufreq_fixed_frequency = fixed_freq;
				mt_gpufreq_fixed_voltage = fixed_volt * 100;
				mt_gpufreq_fixed_freq_volt_state = true;

				mt_gpufreq_target(0);
			} else {
				xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
					    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
			}
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
	}

	return count;

}

#endif


/*********************************
* early suspend callback function
**********************************/
void mt_gpufreq_early_suspend(struct early_suspend *h)
{
	/* mt_gpufreq_state_set(0); */

}

/*******************************
* late resume callback function
********************************/
void mt_gpufreq_late_resume(struct early_suspend *h)
{
	/* mt_gpufreq_check_freq_and_set_pll(); */

	/* mt_gpufreq_state_set(1); */
}

static int mt_gpufreq_pm_restore_early(struct device *dev)
{
#if 0
	int i = 0;

	g_cur_freq = mt_cpufreq_dvfs_get_cpu_freq();

	for (i = 0; i < mt_cpu_freqs_num; i++) {
		if (g_cur_freq == mt_cpu_freqs[i].cpufreq_khz) {
			g_cur_cpufreq_OPPidx = i;
			xlog_printk(ANDROID_LOG_ERROR, "Power/DVFS",
				    "match g_cur_cpufreq_OPPidx: %d\n", g_cur_cpufreq_OPPidx);
			break;
		}
	}

	xlog_printk(ANDROID_LOG_ERROR, "Power/DVFS", "CPU freq SW/HW: %d/%d\n", g_cur_freq,
		    mt_cpufreq_dvfs_get_cpu_freq());
	xlog_printk(ANDROID_LOG_ERROR, "Power/DVFS", "g_cur_cpufreq_OPPidx: %d\n",
		    g_cur_cpufreq_OPPidx);
#endif

	return 0;
}

#if 1
static int mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	unsigned int RegVal = 0;
	unsigned int RegValGet = 0;
	int i = 0, init_idx = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	mt_gpufreq_early_suspend_handler.suspend = mt_gpufreq_early_suspend;
	mt_gpufreq_early_suspend_handler.resume = mt_gpufreq_late_resume;
	register_early_suspend(&mt_gpufreq_early_suspend_handler);
#endif

	/* TEST TEST */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

    /**********************
    * setup gpufreq table
    ***********************/
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "setup gpufreqs table\n");

	mt_setup_gpufreqs_table(OPPS_ARRAY_AND_SIZE(mt6595_gpufreqs_0));

    /**********************
    * setup PMIC wrapper
    ***********************/
	/* same API ??? , default value ??? */
	pmic_config_interface(0x02A6, 0x1, 0x1, 0x1);	/* Set VDVFS13_VOSEL_CTRL[1] to HW control ??? */
	pmic_config_interface(0x02A6, 0x0, 0x1, 0x0);	/* Set VDVFS13_EN_CTRL[0] SW control to 0 */
	pmic_config_interface(0x02AA, 0x1, 0x1, 0x0);	/* Set VDVFS13_EN[0] */

	mt_gpufreq_volt_enable_state = 1;

	pmic_read_interface(0x02A6, &RegValGet, 0x1, 0x1);	/* Get VDVFS13_VOSEL_CTRL[1] to HW control ??? */
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_VOSEL_CTRL[1] = %d\n", RegValGet);
	pmic_read_interface(0x02A6, &RegValGet, 0x1, 0x0);	/* Get VDVFS13_EN_CTRL[0] SW control to 0 */
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_EN_CTRL[0] = %d\n", RegValGet);
	pmic_read_interface(0x02AA, &RegValGet, 0x1, 0x0);	/* Get VDVFS13_EN[0] */
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "VDVFS13_EN[0] = %d\n", RegValGet);

#if 1

	RegVal = mt_gpufreq_volt_to_pmic_wrap(mt_gpufreqs[0].gpufreq_volt);

	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, RegVal);

#else
	mt_gpufreq_reg_write(0x02B0, PMIC_WRAP_DVFS_ADR2);

	RegVal = mt_gpufreq_volt_to_pmic_wrap(mt_gpufreqs[0].gpufreq_volt);
	mt_gpufreq_reg_write(RegVal, PMIC_WRAP_DVFS_WDATA2);	/* 1.125V */
#endif

	g_cur_freq_init_keep = g_cur_gpu_freq;

	for (i = 0; i < mt_gpufreqs_num; i++) {

		if (mt_gpufreqs[i].gpufreq_khz == GPU_DVFS_FREQ3) {
			init_idx = i;
			break;
		}
	}

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "init_idx = %d\n", init_idx);

	mt_gpufreq_set_initial(mt_gpufreqs[init_idx].gpufreq_idx);	/* ??? */

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_pdrv_probe, g_cur_gpu_freq = %d, g_cur_gpu_volt = %d, g_cur_gpu_idx = %d\n",
		    g_cur_gpu_freq, g_cur_gpu_volt, g_cur_gpu_idx);

	mt_gpufreq_ready = true;

	return 0;
}

/***************************************
* this function should never be called
****************************************/
static int mt_gpufreq_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}


struct dev_pm_ops mt_gpufreq_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.restore_early = mt_gpufreq_pm_restore_early,
};

static struct platform_driver mt_gpufreq_pdrv = {
	.probe = mt_gpufreq_pdrv_probe,
	.remove = mt_gpufreq_pdrv_remove,
	.driver = {
		   .name = "mt-gpufreq",
		   .pm = &mt_gpufreq_pm_ops,
		   .owner = THIS_MODULE,
		   },
};
#endif


static int mt_gpufreq_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_debug_read, NULL);
}

static const struct file_operations mt_gpufreq_debug_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_debug_write,
	.release = single_release,
};

static int mt_gpufreq_limited_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_power_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_power_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_power_write,
	.release = single_release,
};

static int mt_gpufreq_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_state_read, NULL);
}

static const struct file_operations mt_gpufreq_state_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_state_write,
	.release = single_release,
};

static int mt_gpufreq_opp_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_opp_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_opp_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_power_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_power_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_power_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_power_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_opp_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_opp_freq_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_freq_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_opp_freq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_opp_freq_write,
	.release = single_release,
};

static int mt_gpufreq_var_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_var_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_var_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_var_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_volt_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_volt_enable_read, NULL);
}

static const struct file_operations mt_gpufreq_volt_enable_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_volt_enable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_volt_enable_write,
	.release = single_release,
};

static int mt_gpufreq_fixed_freq_volt_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_fixed_freq_volt_read, NULL);
}

static const struct file_operations mt_gpufreq_fixed_freq_volt_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_fixed_freq_volt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_fixed_freq_volt_write,
	.release = single_release,
};

/**********************************
* mediatek gpufreq initialization
***********************************/
static int __init mt_gpufreq_init(void)
{
#if 1
	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_gpufreq_dir = NULL;
	int ret = 0;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init\n");

	mt_gpufreq_dir = proc_mkdir("gpufreq", NULL);
	if (!mt_gpufreq_dir) {
		pr_err("[%s]: mkdir /proc/gpufreq failed\n", __func__);
	} else {

		mt_entry =
		    proc_create("gpufreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_debug_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_limited_power", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_gpufreq_dir, &mt_gpufreq_limited_power_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_state_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_opp_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_opp_dump_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_power_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_power_dump_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_opp_freq", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_opp_freq_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_var_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_var_dump_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_volt_enable", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_volt_enable_fops);
		if (mt_entry) {
		}

		mt_entry =
		    proc_create("gpufreq_fixed_freq_volt", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_gpufreq_dir, &mt_gpufreq_fixed_freq_volt_fops);
		if (mt_entry) {
		}

	}
#endif

#if 0
	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x00070000) >> 16;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init, clk_cfg_0 = %d\n",
		    clk_cfg_0);

	switch (clk_cfg_0) {
	case 0x5:		/* 476Mhz */
		g_cur_freq = GPU_MMPLL_D3;
		break;
	case 0x2:		/* 403Mhz */
		g_cur_freq = GPU_SYSPLL_D2;
		break;
	case 0x6:		/* 357Mhz */
		g_cur_freq = GPU_MMPLL_D4;
		break;
	case 0x4:		/* 312Mhz */
		g_cur_freq = GPU_UNIVPLL1_D2;
		break;
	case 0x7:		/* 286Mhz */
		g_cur_freq = GPU_MMPLL_D5;
		break;
	case 0x3:		/* 268Mhz */
		g_cur_freq = GPU_SYSPLL_D3;
		break;
	case 0x1:		/* 238Mhz */
		g_cur_freq = GPU_MMPLL_D6;
		break;
	case 0x0:		/* 156Mhz */
		g_cur_freq = GPU_UNIVPLL1_D4;
		break;
	default:
		break;
	}


	g_cur_freq_init_keep = g_cur_gpu_freq;
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_init, g_cur_freq_init_keep = %d\n", g_cur_freq_init_keep);
#endif

	ret = platform_driver_register(&mt_gpufreq_pdrv);	/* ??? */

	if (ret) {
		xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS",
			    "failed to register gpufreq driver\n");
		return ret;
	} else {
		xlog_printk(ANDROID_LOG_ERROR, "Power/GPU_DVFS",
			    "gpufreq driver registration done\n");
		return 0;
	}

}

static void __exit mt_gpufreq_exit(void)
{

}
module_init(mt_gpufreq_init);
module_exit(mt_gpufreq_exit);

MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
