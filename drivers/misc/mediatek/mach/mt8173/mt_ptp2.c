#define __MT_PTP2_C__



#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <mach/pmic_mt6320_sw.h>
#include <mach/mt_spm.h>
#include <mach/mt_boot.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include "mach/mt_ptp2.h"



#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "PTP2"

#define ptp2_err(fmt, args...)       \
    xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define ptp2_warn(fmt, args...)      \
    xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define ptp2_info(fmt, args...)      \
    xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define ptp2_dbg(fmt, args...)       \
    xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define ptp2_ver(fmt, args...)       \
    xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else				/* USING_XLOG */

#define TAG     "[PTP2] "

#define ptp2_err(fmt, args...)       \
    printk(KERN_ERR TAG KERN_CONT fmt, ##args)
#define ptp2_warn(fmt, args...)      \
    printk(KERN_WARNING TAG KERN_CONT fmt, ##args)
#define ptp2_info(fmt, args...)      \
    printk(KERN_NOTICE TAG KERN_CONT fmt, ##args)
#define ptp2_dbg(fmt, args...)       \
    printk(KERN_INFO TAG KERN_CONT fmt, ##args)
#define ptp2_ver(fmt, args...)       \
    printk(KERN_DEBUG TAG KERN_CONT fmt, ##args)

#endif				/* USING_XLOG */



#ifdef CONFIG_OF
void __iomem *ptp2_base;	/* 0x10200000 */
#endif

extern u32 get_devinfo_with_index(u32 index);	/* TODO: FIXME #include "devinfo.h" */


/* enable debug message */
#define DEBUG   0
#define FBB_ENABLE_BY_EFUSE 1



/* For small */
static struct PTP2_data ptp2_data;
static struct PTP2_trig ptp2_trig;
static unsigned int ptp2_lo_enable = 1;
static unsigned int ptp2_ctrl_lo[2];



/* For big */
static struct PTP2_big_data ptp2_big_data;
static struct PTP2_big_trig ptp2_big_trig;
static volatile unsigned int ptp2_big_regs[6] = { 0 };

static unsigned int ptp2_big_status;
static unsigned int ptp2_big_lo_enable = 1;
static unsigned int ptp2_big_fbb_enable;
static unsigned int ptp2_big_spark_enable = 1;
static unsigned int ptp2_big_vfbb = 2;	/* default: 300mv */
static unsigned int ptp2_big_spark_count;
static unsigned int ptp2_big_initialized;



static void ptp2_reset_data(struct PTP2_data *data)
{
	memset((void *)data, 0, sizeof(struct PTP2_data));
}



static void ptp2_big_reset_data(struct PTP2_big_data *data)
{
	memset((void *)data, 0, sizeof(struct PTP2_big_data));
}



static void ptp2_reset_trig(struct PTP2_trig *trig)
{
	memset((void *)trig, 0, sizeof(struct PTP2_trig));
}



static void ptp2_big_reset_trig(struct PTP2_big_trig *trig)
{
	memset((void *)trig, 0, sizeof(struct PTP2_big_trig));
}



static int ptp2_set_rampstart(struct PTP2_data *data, unsigned int rampstart)
{
	if (rampstart & ~(0x3)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"3\"\n", __func__,
			 rampstart);

		return -1;
	}

	data->RAMPSTART = rampstart;

	return 0;
}



static int ptp2_big_set_rampstart(struct PTP2_big_data *data, unsigned int rampstart)
{
	if (rampstart & ~(0x3)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"3\"\n", __func__,
			 rampstart);

		return -1;
	}

	data->BIG_RAMPSTART = rampstart;

	return 0;
}



static int ptp2_set_rampstep(struct PTP2_data *data, unsigned int rampstep)
{
	if (rampstep & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 rampstep);

		return -1;
	}

	data->RAMPSTEP = rampstep;

	return 0;
}



static int ptp2_big_set_rampstep(struct PTP2_big_data *data, unsigned int rampstep)
{
	if (rampstep & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 rampstep);

		return -1;
	}

	data->BIG_RAMPSTEP = rampstep;

	return 0;
}


static int ptp2_set_delay(struct PTP2_data *data, unsigned int delay)
{
	if (delay & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 delay);

		return -1;
	}

	data->DELAY = delay;

	return 0;
}



static int ptp2_big_set_delay(struct PTP2_big_data *data, unsigned int delay)
{
	if (delay & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 delay);

		return -1;
	}

	data->BIG_DELAY = delay;

	return 0;
}



static int ptp2_set_autoStopBypass_enable(struct PTP2_data *data, unsigned int autostop_enable)
{
	if (autostop_enable & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 autostop_enable);

		return -1;
	}

	data->AUTO_STOP_BYPASS_ENABLE = autostop_enable;

	return 0;
}



static int ptp2_big_set_autoStopBypass_enable(struct PTP2_big_data *data,
					      unsigned int autostop_enable)
{
	if (autostop_enable & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 autostop_enable);

		return -1;
	}

	data->BIG_AUTOSTOPENABLE = autostop_enable;

	return 0;
}



static int ptp2_set_triggerPulDelay(struct PTP2_data *data, unsigned int triggerPulDelay)
{
	if (triggerPulDelay & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 triggerPulDelay);

		return -1;
	}
	data->TRIGGER_PUL_DELAY = triggerPulDelay;

	return 0;
}



static int ptp2_big_set_triggerPulDelay(struct PTP2_big_data *data, unsigned int triggerPulDelay)
{
	if (triggerPulDelay & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 triggerPulDelay);

		return -1;
	}
	data->BIG_TRIGGER_PUL_DELAY = triggerPulDelay;

	return 0;
}



static int ptp2_set_ctrl_enable(struct PTP2_data *data, unsigned int ctrlEnable)
{
	if (ctrlEnable & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 ctrlEnable);

		return -1;
	}
	data->CTRL_ENABLE = ctrlEnable;

	return 0;
}



static int ptp2_set_det_enable(struct PTP2_data *data, unsigned int detEnable)
{
	if (detEnable & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 detEnable);

		return -1;
	}
	data->DET_ENABLE = detEnable;

	return 0;
}



static int ptp2_big_set_nocpuenable(struct PTP2_big_data *data, unsigned int noCpuEnable)
{
	if (noCpuEnable & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 noCpuEnable);

		return -1;
	}
	data->BIG_NOCPUENABLE = noCpuEnable;

	return 0;

}



static int ptp2_big_set_cpuenable(struct PTP2_big_data *data, unsigned int cpuEnable)
{
	if (cpuEnable & ~(0x3)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"3\"\n", __func__,
			 cpuEnable);

		return -1;
	}

	data->BIG_CPUENABLE = cpuEnable;

	return 0;
}



static int ptp2_set_mp0_nCORERESET(struct PTP2_trig *trig, unsigned int mp0_nCoreReset)
{
	if (mp0_nCoreReset & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 mp0_nCoreReset);

		return -1;
	}
	trig->mp0_nCORE_RESET = mp0_nCoreReset;

	return 0;
}



static int ptp2_set_mp0_STANDBYWFE(struct PTP2_trig *trig, unsigned int mp0_StandbyWFE)
{
	if (mp0_StandbyWFE & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 mp0_StandbyWFE);

		return -1;
	}
	trig->mp0_STANDBY_WFE = mp0_StandbyWFE;

	return 0;
}



static int ptp2_set_mp0_STANDBYWFI(struct PTP2_trig *trig, unsigned int mp0_StandbyWFI)
{
	if (mp0_StandbyWFI & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"15\"\n", __func__,
			 mp0_StandbyWFI);

		return -1;
	}
	trig->mp0_STANDBY_WFI = mp0_StandbyWFI;

	return 0;
}



static int ptp2_set_mp0_STANDBYWFIL2(struct PTP2_trig *trig, unsigned int mp0_StandbyWFIL2)
{
	if (mp0_StandbyWFIL2 & ~(0x1)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"1\"\n", __func__,
			 mp0_StandbyWFIL2);

		return -1;
	}
	trig->mp0_STANDBY_WFIL2 = mp0_StandbyWFIL2;

	return 0;
}


static int ptp2_big_set_NoCPU(struct PTP2_big_trig *trig, unsigned int noCpu_lo_setting)
{
	if (noCpu_lo_setting & ~(0x33338)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"0x33338\"\n",
			 __func__, noCpu_lo_setting);

		return -1;
	}

	trig->ctrl_regs[0] = (noCpu_lo_setting << 12);

	return 0;
}



static int ptp2_big_set_CPU0(struct PTP2_big_trig *trig, unsigned int cpu0_lo_setting)
{
	if (cpu0_lo_setting & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"F\"\n", __func__,
			 cpu0_lo_setting);

		return -1;
	}

	trig->ctrl_regs[1] = (cpu0_lo_setting << 28);

	return 0;
}



static int ptp2_big_set_CPU1(struct PTP2_big_trig *trig, unsigned int cpu1_lo_setting)
{
	if (cpu1_lo_setting & ~(0xF)) {
		ptp2_err("[%s] bad argument!! (%d), argument should be \"0\" ~ \"F\"\n", __func__,
			 cpu1_lo_setting);

		return -1;
	}

	trig->ctrl_regs[2] = (cpu1_lo_setting << 28);

	return 0;
}



static void ptp2_apply(struct PTP2_data *data, struct PTP2_trig *trig)
{
	volatile unsigned int val_0 = BITS(PTP2_DET_RAMPSTART, data->RAMPSTART) |
	    BITS(PTP2_DET_RAMPSTEP, data->RAMPSTEP) |
	    BITS(PTP2_DET_DELAY, data->DELAY) |
	    BITS(PTP2_DET_AUTO_STOP_BYPASS_ENABLE, data->AUTO_STOP_BYPASS_ENABLE) |
	    BITS(PTP2_DET_TRIGGER_PUL_DELAY, data->TRIGGER_PUL_DELAY) |
	    BITS(PTP2_CTRL_ENABLE, data->CTRL_ENABLE) | BITS(PTP2_DET_ENABLE, data->DET_ENABLE);

	volatile unsigned int val_1 = BITS(PTP2_MP0_nCORERESET, trig->mp0_nCORE_RESET) |
	    BITS(PTP2_MP0_STANDBYWFE, trig->mp0_STANDBY_WFE) |
	    BITS(PTP2_MP0_STANDBYWFI, trig->mp0_STANDBY_WFI) |
	    BITS(PTP2_MP0_STANDBYWFIL2, trig->mp0_STANDBY_WFIL2);

	ptp2_ctrl_lo[0] = val_0;
	ptp2_ctrl_lo[1] = val_1;
	ptp2_write(PTP2_CTRL_REG_0, val_0);
	ptp2_write(PTP2_CTRL_REG_1, val_1);
	/* Apply software reset that apply the PTP2 LO value to system */
	ptp2_write_field(PTP2_CTRL_REG_0, PTP2_DET_SWRST, 1);
	udelay(1000);
	ptp2_write_field(PTP2_CTRL_REG_0, PTP2_DET_SWRST, 0);
}



static void ptp2_big_apply(struct PTP2_big_data *data, struct PTP2_big_trig *trig)
{
	volatile unsigned int val_2 =
	    BITS(PTP2_BIG_DET_TRIGGER_PUL_DELAY, data->BIG_TRIGGER_PUL_DELAY) |
	    BITS(PTP2_BIG_DET_RAMPSTART, data->BIG_RAMPSTART) |
	    BITS(PTP2_BIG_DET_DELAY, data->BIG_DELAY) |
	    BITS(PTP2_BIG_DET_RAMPSTEP, data->BIG_RAMPSTEP) |
	    BITS(PTP2_BIG_DET_AUTOSTOP_ENABLE, data->BIG_AUTOSTOPENABLE) |
	    BITS(PTP2_BIG_DET_NOCPU_ENABLE, data->BIG_NOCPUENABLE) |
	    BITS(PTP2_BIG_DET_CPU_ENABLE, data->BIG_CPUENABLE);

	volatile unsigned int nocpu = trig->ctrl_regs[0];
	volatile unsigned int cpu0 = trig->ctrl_regs[1];
	volatile unsigned int cpu1 = trig->ctrl_regs[2];

	ptp2_big_regs[2] = nocpu;
	ptp2_big_regs[3] = cpu0;
	ptp2_big_regs[4] = cpu1;
	ptp2_big_regs[5] = val_2;

	ptp2_write(PTP2_DET_CPU_ENABLE_ADDR, val_2);
	ptp2_write(PTP2_BIG_CTRL_REG_2, nocpu);
	ptp2_write(PTP2_BIG_CTRL_REG_3, cpu0);
	ptp2_write(PTP2_BIG_CTRL_REG_4, cpu1);
}


/* config_LO_CTRL(PTP2_RAMPSTART_3, 9, 13, 0, 0, 1, 1, 0xF, 0xF, 0xF, 1) => For all on and worst case */
/* config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0xF, 0xF, 0xF, 1) => For all on and best case */
/* config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0x8, 0x8, 0x8, 1) => For >= 4 core on and best case */
/* config_LO_CTRL(PTP2_RAMPSTART_3, 0, 0, 0, 0, 0, 0, 0x0, 0x0, 0x0, 0) => For all off */
static void config_LO_CTRL(unsigned int rampStart,
			   unsigned int rampStep,
			   unsigned int delay,
			   unsigned int autoStopEnable,
			   unsigned int triggerPulDelay,
			   unsigned int ctrlEnable,
			   unsigned int detEnable,
			   unsigned int mp0_nCoreReset,
			   unsigned int mp0_StandbyWFE,
			   unsigned int mp0_StandbyWFI, unsigned int mp0_StandbyWFIL2)
{
	ptp2_reset_data(&ptp2_data);
	smp_mb();
	ptp2_set_rampstart(&ptp2_data, rampStart);
	ptp2_set_rampstep(&ptp2_data, rampStep);
	ptp2_set_delay(&ptp2_data, delay);
	ptp2_set_autoStopBypass_enable(&ptp2_data, autoStopEnable);
	ptp2_set_triggerPulDelay(&ptp2_data, triggerPulDelay);
	ptp2_set_ctrl_enable(&ptp2_data, ctrlEnable);
	ptp2_set_det_enable(&ptp2_data, detEnable);

	ptp2_reset_trig(&ptp2_trig);
	smp_mb();
	ptp2_set_mp0_nCORERESET(&ptp2_trig, mp0_nCoreReset);
	ptp2_set_mp0_STANDBYWFE(&ptp2_trig, mp0_StandbyWFE);
	ptp2_set_mp0_STANDBYWFI(&ptp2_trig, mp0_StandbyWFI);
	ptp2_set_mp0_STANDBYWFIL2(&ptp2_trig, mp0_StandbyWFIL2);
	smp_mb();
	ptp2_apply(&ptp2_data, &ptp2_trig);
}



static void config_big_LO_CTRL(unsigned int triggerPulDelay,
			       unsigned int rampStart,
			       unsigned int delay,
			       unsigned int rampStep,
			       unsigned int autoStopEnable,
			       unsigned int noCpuEnable,
			       unsigned int cpuEnable,
			       unsigned int noCpu_lo_setting,
			       unsigned int cpu0_lo_setting, unsigned int cpu1_lo_setting)
{
	ptp2_big_reset_data(&ptp2_big_data);
	smp_mb();
	ptp2_big_set_triggerPulDelay(&ptp2_big_data, triggerPulDelay);
	ptp2_big_set_rampstart(&ptp2_big_data, rampStart);
	ptp2_big_set_delay(&ptp2_big_data, delay);
	ptp2_big_set_rampstep(&ptp2_big_data, rampStep);
	ptp2_big_set_autoStopBypass_enable(&ptp2_big_data, autoStopEnable);
	ptp2_big_set_nocpuenable(&ptp2_big_data, noCpuEnable);
	ptp2_big_set_cpuenable(&ptp2_big_data, cpuEnable);

	ptp2_big_reset_trig(&ptp2_big_trig);
	smp_mb();
	ptp2_big_set_NoCPU(&ptp2_big_trig, noCpu_lo_setting);
	ptp2_big_set_CPU0(&ptp2_big_trig, cpu0_lo_setting);
	ptp2_big_set_CPU1(&ptp2_big_trig, cpu1_lo_setting);
	smp_mb();
	ptp2_big_apply(&ptp2_big_data, &ptp2_big_trig);
}



static void enable_LO(void)
{
	if ((ptp2_ctrl_lo[0] & 0x03) == 0)
		config_LO_CTRL(PTP2_RAMPSTART_3, 1, 1, 0, 0, 1, 1, 0xE, 0xE, 0xE, 1);
	else {
		config_LO_CTRL((ptp2_ctrl_lo[0] >> 12) & 0x03,
			       (ptp2_ctrl_lo[0] >> 8) & 0x0F,
			       (ptp2_ctrl_lo[0] >> 4) & 0x0F,
			       (ptp2_ctrl_lo[0] >> 3) & 0x01,
			       (ptp2_ctrl_lo[0] >> 2) & 0x01,
			       (ptp2_ctrl_lo[0] >> 1) & 0x01,
			       ptp2_ctrl_lo[0] & 0x01,
			       (ptp2_ctrl_lo[1] >> 28) & 0x0f,
			       (ptp2_ctrl_lo[1] >> 24) & 0x0f,
			       (ptp2_ctrl_lo[1] >> 20) & 0x0f, (ptp2_ctrl_lo[1] >> 19) & 0x01);
	}
}



static void enable_big_LO(void)
{
	if ((ptp2_big_regs[5] & 0x03) == 0)
		config_big_LO_CTRL(0, PTP2_RAMPSTART_3, 1, 1, 0, 1, 3, 0x33338, 0xF, 0xF);
	else {
		config_big_LO_CTRL((ptp2_big_regs[5] >> 16) & 0x01,
				   (ptp2_big_regs[5] >> 14) & 0x03,
				   (ptp2_big_regs[5] >> 10) & 0x0F,
				   (ptp2_big_regs[5] >> 6) & 0x0F,
				   (ptp2_big_regs[5] >> 5) & 0x01,
				   (ptp2_big_regs[5] >> 4) & 0x01,
				   ptp2_big_regs[5] & 0x0F,
				   ptp2_big_regs[2] >> 12,
				   ptp2_big_regs[3] >> 28, ptp2_big_regs[4] >> 28);
	}
}




static void disable_LO(void)
{
	config_LO_CTRL(0, 0, 0, 0, 0, 0, 0, 0x0, 0x0, 0x0, 0);	/* => For all off */
}



static void disable_big_LO(void)
{
	config_big_LO_CTRL(0, 0, 0, 0, 0, 0, 0, 0x0, 0x0, 0x0);	/* => For all off */
}


/*
static int get_LO_status(void)
{
	volatile unsigned int val = 0;

	val = ptp2_read(PTP2_CTRL_REG_0);
	val &= 0x01;
	if (val != 0)
		return 1;

	return 0;
}



static int get_big_LO_status(void)
{
	volatile unsigned int val = 0;

	val = ptp2_read(PTP2_DET_CPU_ENABLE_ADDR);
	val &= 0x03;
	if (val != 0)
		return 1;

	return 0;
}
*/



static void set_VFBB(unsigned int enable)
{
	unsigned int val;
	if (0 == enable) {
		val = ptp2_read(PTP2_BIG_CTRL_VFBB_1) & ~0x07;
		ptp2_write(PTP2_BIG_CTRL_VFBB_1, val);
	} else {
		val = ptp2_read(PTP2_BIG_CTRL_VFBB_1) | 0x03;
		ptp2_write(PTP2_BIG_CTRL_VFBB_1, val);
		smp_mb(); /* ... */
		udelay(5);
		val = ptp2_read(PTP2_BIG_CTRL_VFBB_1) | 0x04;
		ptp2_write(PTP2_BIG_CTRL_VFBB_1, val);
	}
}



static void config_FBB(void)
{
	unsigned int val;

	val = (ptp2_read(PTP2_BIG_CTRL_VFBB_0) & ~(0x07 << 14)) | (ptp2_big_vfbb << 14);
	ptp2_write(PTP2_BIG_CTRL_VFBB_0, val);	/* 300mv */

	val = ptp2_read(PTP2_BIG_CTRL_REG_0) | 0x10;
	ptp2_write(PTP2_BIG_CTRL_REG_0, val);
}



static void enable_FBB_SW(void)
{
	unsigned int val;

	if (0 == (ptp2_big_status & PTP2_ENABLE_FBB_SW)) {
		udelay(1000);
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) | 0xa;
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		set_VFBB(1);
		ptp2_big_status |= PTP2_ENABLE_FBB_SW;
	}
	udelay(1);
}



void enable_FBB_SPM(void)
{
	volatile unsigned int val_1;
	volatile unsigned int val_2;

	if (0 == (ptp2_big_status & PTP2_ENABLE_FBB_SPM)) {
		val_1 = ptp2_read(PTP2_BIG_CTRL_REG_0) | 0x10;
		val_2 = ptp2_read(PTP2_BIG_CTRL_REG_1) | 0x1;

		/*SPM FBBEN signal in ca15lcputop has to be set */
		spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) | 0x100));
		ptp2_write(PTP2_BIG_CTRL_REG_0, val_1);
		ptp2_write(PTP2_BIG_CTRL_REG_1, val_2);
		set_VFBB(1);
		ptp2_big_status |= PTP2_ENABLE_FBB_SPM;
	}
	udelay(1);
}




static void disable_FBB_SW(void)
{
	volatile unsigned int val;

	if (ptp2_big_status & PTP2_ENABLE_FBB_SW) {
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) & ~0x8;
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		set_VFBB(0);
		ptp2_big_status &= ~PTP2_ENABLE_FBB_SW;
	}
	udelay(1);
}



void disable_FBB_SPM(void)
{
	volatile unsigned int val;

	if (ptp2_big_status & PTP2_ENABLE_FBB_SPM) {
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) & ~0x1;
		/* SPM FBBEN signal in ca15lcputop has to be set */
		spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) & ~0x100));
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		val = ptp2_read(PTP2_BIG_CTRL_REG_0) & ~0x10;
		ptp2_write(PTP2_BIG_CTRL_REG_0, val);
		set_VFBB(0);
		ptp2_big_status &= ~PTP2_ENABLE_FBB_SPM;
	}
	udelay(1);
}



static void config_SPARK(void)
{
	unsigned int val;

	val = ptp2_read(PTP2_BIG_CTRL_REG_0) | 0x3;
	ptp2_write(PTP2_BIG_CTRL_REG_0, val);
}



static void enable_SPARK_SW(void)
{
	unsigned int val;

	if (0 == (ptp2_big_status & PTP2_ENABLE_SPARK_SW)) {
		/*Config SPARK Enable*/
		val = ptp2_read(PTP2_BIG_CTRL_REG_0) | 0x3;
		ptp2_write(PTP2_BIG_CTRL_REG_0, val);
		/*SW SPAEK Enable */
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) | 0x6;
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		ptp2_big_status |= PTP2_ENABLE_SPARK_SW;
	}
}



void enable_SPARK_SPM(void)
{
	unsigned int val;

	if (0 == (ptp2_big_status & PTP2_ENABLE_SPARK_SPM)) {
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) | 0x1;

		/* SPM FBBEN signal in ca15lcputop has to be set */
		spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) | 0x200));
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		ptp2_big_status |= PTP2_ENABLE_SPARK_SPM;
	}
}



static void disable_SPARK_SW(void)
{
	volatile unsigned int val;

	if (ptp2_big_status & PTP2_ENABLE_SPARK_SW) {
		/*SW SPARK disable*/
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) & ~0x4;
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		/*Config SPARK disable*/
		val = ptp2_read(PTP2_BIG_CTRL_REG_0) & ~0x3;
		ptp2_write(PTP2_BIG_CTRL_REG_0, val);
		ptp2_big_status &= ~PTP2_ENABLE_SPARK_SW;
	}
}



void disable_SPARK_SPM(void)
{
	volatile unsigned int val;

	if (ptp2_big_status & PTP2_ENABLE_SPARK_SPM) {
		val = ptp2_read(PTP2_BIG_CTRL_REG_1) & ~0x1;

		/* SPM FBBEN signal in ca15lcputop has to be set */
		spm_write(SPM_SLEEP_PTPOD2_CON, (spm_read(SPM_SLEEP_PTPOD2_CON) & ~0x200));
		ptp2_write(PTP2_BIG_CTRL_REG_1, val);
		ptp2_big_status &= ~PTP2_ENABLE_SPARK_SPM;
	}
}



void turn_on_FBB(void)
{
	if (0 == ptp2_big_fbb_enable)
		return;

	ptp2_dbg("turn on FBB\n");
	enable_FBB_SW();
}



void turn_off_FBB(void)
{
	if (0 == ptp2_big_fbb_enable)
		return;

	ptp2_dbg("turn off FBB\n");
	disable_FBB_SW();
}



void turn_on_SPARK(void)
{
	if (0 == ptp2_big_spark_enable)
		return;

	ptp2_dbg("turn on SPARK\n");
	ptp2_big_spark_count++;
	enable_SPARK_SW();
}



void turn_off_SPARK(char *message)
{
	if (0 == ptp2_big_spark_enable)
		return;

	ptp2_dbg("turn off SPARK (%s)\n", message);
	disable_SPARK_SW();
}



void turn_on_LO(void)
{
	if (0 == ptp2_lo_enable)
		return;

	enable_LO();
}



void turn_on_big_LO(void)
{
	enable_big_LO();
}



void turn_off_LO(void)
{
	if (0 == ptp2_lo_enable)
		return;

	disable_LO();
}



void turn_off_big_LO(void)
{
	disable_big_LO();
}



/* Device infrastructure */
static int ptp2_remove(struct platform_device *pdev)
{
	return 0;
}



static int ptp2_probe(struct platform_device *pdev)
{
	return 0;
}



static int ptp2_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	   kthread_stop(ptp2_thread);
	 */

	return 0;
}



static int ptp2_resume(struct platform_device *pdev)
{
	/*
	   ptp2_thread = kthread_run(ptp2_thread_handler, 0, "ptp2 xxx");
	   if (IS_ERR(ptp2_thread))
	   {
	   printk("[%s]: failed to create ptp2 xxx thread\n", __func__);
	   }
	 */

	return 0;
}



#ifdef CONFIG_OF
static const struct of_device_id mt_ptp2_of_match[] = {
	{.compatible = "mediatek,MCUCFG",},
	{},
};
#endif
static struct platform_driver ptp2_driver = {
	.remove = ptp2_remove,
	.shutdown = NULL,
	.probe = ptp2_probe,
	.suspend = ptp2_suspend,
	.resume = ptp2_resume,
	.driver = {
		   .name = "mt-ptp2",
#ifdef CONFIG_OF
		   .of_match_table = mt_ptp2_of_match,
#endif
		   },
};



#ifdef CONFIG_PROC_FS
static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}



/* ptp2_lo_enable */
static int ptp2_lo_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_lo_enable = %d\n", ptp2_lo_enable);

	return 0;
}



static int ptp2_big_lo_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_lo_enable = %d\n", ptp2_big_lo_enable);

	return 0;
}



static ssize_t ptp2_lo_enable_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{
	int val = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%d", &val);
	if (val == 1) {
		enable_LO();
		ptp2_lo_enable = 1;
	} else {
		ptp2_lo_enable = 0;
		disable_LO();
	}

	free_page((unsigned long)buf);

	return count;
}



static ssize_t ptp2_big_lo_enable_proc_write(struct file *file, const char __user *buffer,
					     size_t count, loff_t *pos)
{
	int val = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%d", &val);

	ptp2_big_initialized = 0;
	if (0 == val)
		ptp2_big_lo_enable = 0;
	else
		ptp2_big_lo_enable = 1;

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_ctrl_lo_0 */
static int ptp2_ctrl_lo_0_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_ctrl_lo_0 = %x\n", ptp2_ctrl_lo[0]);
	seq_printf(m, "[print by register] ptp2_ctrl_lo_0 = %08lx\n",
		   (unsigned long)ptp2_read(PTP2_CTRL_REG_0));
	return 0;
}



static ssize_t ptp2_ctrl_lo_0_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_ctrl_lo);
	config_LO_CTRL((ptp2_ctrl_lo[0] >> 12) & 0x03,
		       (ptp2_ctrl_lo[0] >> 8) & 0x0F,
		       (ptp2_ctrl_lo[0] >> 4) & 0x0F,
		       (ptp2_ctrl_lo[0] >> 3) & 0x01,
		       (ptp2_ctrl_lo[0] >> 2) & 0x01,
		       (ptp2_ctrl_lo[0] >> 1) & 0x01,
		       ptp2_ctrl_lo[0] & 0x01,
		       (ptp2_ctrl_lo[1] >> 28) & 0x0f,
		       (ptp2_ctrl_lo[1] >> 24) & 0x0f,
		       (ptp2_ctrl_lo[1] >> 20) & 0x0f, (ptp2_ctrl_lo[1] >> 19) & 0x01);

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_ctrl_lo_1*/
static int ptp2_ctrl_lo_1_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_ctrl_lo_1 = %x\n", ptp2_ctrl_lo[1]);
	seq_printf(m, "[print by register] ptp2_ctrl_lo_1 = %08lx\n",
		   (unsigned long)ptp2_read(PTP2_CTRL_REG_1));
	return 0;
}



static ssize_t ptp2_ctrl_lo_1_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_ctrl_lo + 1);
	config_LO_CTRL((ptp2_ctrl_lo[0] >> 12) & 0x03,
		       (ptp2_ctrl_lo[0] >> 8) & 0x0F,
		       (ptp2_ctrl_lo[0] >> 4) & 0x0F,
		       (ptp2_ctrl_lo[0] >> 3) & 0x01,
		       (ptp2_ctrl_lo[0] >> 2) & 0x01,
		       (ptp2_ctrl_lo[0] >> 1) & 0x01,
		       ptp2_ctrl_lo[0] & 0x01,
		       (ptp2_ctrl_lo[1] >> 28) & 0x0f,
		       (ptp2_ctrl_lo[1] >> 24) & 0x0f,
		       (ptp2_ctrl_lo[1] >> 20) & 0x0f, (ptp2_ctrl_lo[1] >> 19) & 0x01);

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_big_vfbb */
static int ptp2_vfbb_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_vfbb = %d\n", ptp2_big_vfbb);
	seq_puts(m,
		 "0: 200mv\n1: 250mv\n2: 300mv\n3: 350mv\n4: 400mv\n5: 450mv\n6: 500mv\n7: 550mv\n");

	return 0;
}

static ssize_t ptp2_vfbb_proc_write(struct file *file, const char __user *buffer, size_t count,
				    loff_t *pos)
{
	int val = ptp2_big_vfbb;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%d", &val);
	if (val >= 0 && val <= 7) {
		ptp2_big_vfbb = val;
	} else {
		ptp2_err("wrong ptp2_big_vfbb value %d\n", val);
	}

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_fbb_enable */
static int ptp2_fbb_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_fbb_enable = %d\n", ptp2_big_fbb_enable);

	return 0;
}

static ssize_t ptp2_fbb_enable_proc_write(struct file *file, const char __user *buffer,
					  size_t count, loff_t *pos)
{
	int val = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%d", &val);
	if (val == 1) {
		ptp2_big_fbb_enable = 1;
		enable_FBB_SW();
	} else {
		ptp2_big_fbb_enable = 0;
		disable_FBB_SW();
	}

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_spark_enable */
static int ptp2_spark_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_spark_enable = %d\n", ptp2_big_spark_enable);
	seq_printf(m, "check spark register = %d\n", ptp2_read(PTP2_BIG_CTRL_REG_1)&0x6);
	return 0;
}

static ssize_t ptp2_spark_enable_proc_write(struct file *file, const char __user *buffer,
					    size_t count, loff_t *pos)
{
	int val = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%d", &val);
	if (val == 1) {
		ptp2_big_spark_enable = 1;
		enable_SPARK_SW();
	} else {
		ptp2_big_spark_enable = 0;
		disable_SPARK_SW();
	}

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_spark_count */
static int ptp2_spark_count_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_spark_count = %d\n", ptp2_big_spark_count);

	return 0;
}



/* ptp2_big_ctrl_lo */
static int ptp2_big_ctrl_lo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_ctrl_lo = %08lx\n", (unsigned long)ptp2_big_regs[5]);
	return 0;
}



static ssize_t ptp2_big_ctrl_lo_proc_write(struct file *file, const char __user *buffer,
					   size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_big_regs + 5);	/* 0xC453 */

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_big_ctrl_trig_0 */
static int ptp2_big_ctrl_trig_0_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_ctrl_trig_0 = %x\n", ptp2_big_regs[2]);
	seq_printf(m, "[Reg]ptp2_big_ctrl_trig_0 = %08lx\n",
		   (unsigned long)ptp2_read(PTP2_BIG_CTRL_REG_0 + 8));
	return 0;
}



static ssize_t ptp2_big_ctrl_trig_0_proc_write(struct file *file, const char __user *buffer,
					       size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_big_regs + 2);	/* 0x33338000 */

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_big_ctrl_trig_1 */
static int ptp2_big_ctrl_trig_1_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_ctrl_trig_1 = %x\n", ptp2_big_regs[3]);
	seq_printf(m, "[Reg]ptp2_big_ctrl_trig_1 = %08lx\n",
		   (unsigned long)ptp2_read(PTP2_BIG_CTRL_REG_0 + 12));
	return 0;
}



static ssize_t ptp2_big_ctrl_trig_1_proc_write(struct file *file, const char __user *buffer,
					       size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_big_regs + 3);	/* 0xF0000000 */

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_big_ctrl_trig_2 */
static int ptp2_big_ctrl_trig_2_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ptp2_big_ctrl_trig_2 = %x\n", ptp2_big_regs[4]);
	seq_printf(m, "[Reg]ptp2_big_ctrl_trig_2 = %08lx\n",
		   (unsigned long)ptp2_read(PTP2_BIG_CTRL_REG_0 + 16));
	return 0;
}



static ssize_t ptp2_big_ctrl_trig_2_proc_write(struct file *file, const char __user *buffer,
					       size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	sscanf(buf, "%x", ptp2_big_regs + 4);	/* 0xF0000000 */

	free_page((unsigned long)buf);

	return count;
}



/* ptp2_dump */
static int ptp2_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < PTP2_REG_NUM; i++)
		seq_printf(m, "%08lx\n", (unsigned long)ptp2_read(PTP2_CTRL_REG_0 + (i << 2)));

	return 0;
}



/* ptp2_dump */
static int ptp2_big_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < PTP2_BIG_REG_NUM; i++) {
		if (i < 8)
			seq_printf(m, "%08lx\n",
				   (unsigned long)ptp2_read(PTP2_BIG_CTRL_REG_0 + (i << 2)));
		else
			seq_printf(m, "%08lx\n",
				   (unsigned long)ptp2_read(PTP2_BIG_CTRL_VFBB_0 + ((i - 8) << 2)));
	}


	return 0;
}



#define PROC_FOPS_RW(name)                          \
    static int name ## _proc_open(struct inode *inode, struct file *file)   \
    {                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
    }                                   \
    static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
	.write          = name ## _proc_write,              \
    }

#define PROC_FOPS_RO(name)                          \
    static int name ## _proc_open(struct inode *inode, struct file *file)   \
    {                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
    }                                   \
    static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
    }

#define PROC_ENTRY(name)    {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(ptp2_lo_enable);
PROC_FOPS_RW(ptp2_big_lo_enable);
PROC_FOPS_RW(ptp2_ctrl_lo_0);
PROC_FOPS_RW(ptp2_ctrl_lo_1);
PROC_FOPS_RW(ptp2_vfbb);
PROC_FOPS_RW(ptp2_fbb_enable);
PROC_FOPS_RW(ptp2_spark_enable);
PROC_FOPS_RO(ptp2_spark_count);
PROC_FOPS_RW(ptp2_big_ctrl_lo);
PROC_FOPS_RW(ptp2_big_ctrl_trig_0);
PROC_FOPS_RW(ptp2_big_ctrl_trig_1);
PROC_FOPS_RW(ptp2_big_ctrl_trig_2);
PROC_FOPS_RO(ptp2_dump);
PROC_FOPS_RO(ptp2_big_dump);


static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(ptp2_lo_enable),
		PROC_ENTRY(ptp2_big_lo_enable),
		PROC_ENTRY(ptp2_ctrl_lo_0),
		PROC_ENTRY(ptp2_ctrl_lo_1),
		PROC_ENTRY(ptp2_vfbb),
		PROC_ENTRY(ptp2_fbb_enable),
		PROC_ENTRY(ptp2_spark_enable),
		PROC_ENTRY(ptp2_spark_count),
		PROC_ENTRY(ptp2_big_ctrl_lo),
		PROC_ENTRY(ptp2_big_ctrl_trig_0),
		PROC_ENTRY(ptp2_big_ctrl_trig_1),
		PROC_ENTRY(ptp2_big_ctrl_trig_2),
		PROC_ENTRY(ptp2_dump),
		PROC_ENTRY(ptp2_big_dump)
	};

	dir = proc_mkdir("ptp2", NULL);

	if (!dir) {
		ptp2_err("fail to create /proc/ptp2 @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			ptp2_err("%s(), create /proc/ptp2/%s failed\n", __func__, entries[i].name);
	}

	return 0;
}

#endif				/* CONFIG_PROC_FS */

void ptp2_pre_iomap(void)
{
	struct device_node *node = NULL;
#ifdef CONFIG_OF
	if (ptp2_base == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");
		if (node) {
			/* Setup IO addresses */
			ptp2_base = of_iomap(node, 0);
			printk("[PTP2] ptp2_base=0x%16p\n", (void *)ptp2_base);
		}
	}
#endif
}

int ptp2_pre_init(void)
{
	int val = 0;

	/*printk("[PTP2] I-Chang ptp2_base = 0x%16p\n", (void *)ptp2_base);*/
	if (ptp2_big_initialized == 1)
		return 0;

	 /*FBB*/ config_FBB();
	val = get_devinfo_with_index(15);	/* M_HW_RES4 */
#if FBB_ENABLE_BY_EFUSE
	if (val & 0x4) {
		ptp2_big_fbb_enable = 1;
	}
#endif

	 /*SPARK*/ config_SPARK();

	/*PTP2_BIG_LO */
	if (1 == ptp2_big_lo_enable)
		turn_on_big_LO();
	else
		turn_off_big_LO();
	ptp2_big_initialized = 1;
	return 1;
}


/*
 * Module driver
 */
static int __init ptp2_init(void)
{
	int err = 0;
	struct device_node *node = NULL;

#if 1				/* #ifdef CONFIG_OF */
	if (ptp2_base == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");
		if (node) {
			/* Setup IO addresses */
			ptp2_base = of_iomap(node, 0);
			/* printk("[PTP2] ptp2_base=0x%8x\n",ptp2_base); */
		}
	}
#endif
	err = platform_driver_register(&ptp2_driver);
	if (err) {
		ptp2_err("%s(), PTP2 driver callback register failed..\n", __func__);
		return err;
	}
	turn_on_LO();

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (_create_procfs()) {
		err = -ENOMEM;
		goto out;
	}
#endif				/* CONFIG_PROC_FS */

out:
	return err;
}



static void __exit ptp2_exit(void)
{
	ptp2_info("PTP2 de-initialization\n");
}



module_init(ptp2_init);
module_exit(ptp2_exit);

MODULE_DESCRIPTION("MediaTek PTP2 Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MT_PTP2_C__
