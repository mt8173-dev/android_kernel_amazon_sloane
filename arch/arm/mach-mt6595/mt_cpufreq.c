/**
* @file    mt_cpufreq.c
* @brief   Driver for CPU DVFS
*
*/

#define __MT_CPUFREQ_C__

/*=============================================================*/
/* Include files                                               */
/*=============================================================*/

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
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
#include <linux/bitops.h>
#include <linux/uaccess.h>

#include <asm/system.h>

/* project includes */
#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"

#include "mach/irqs.h"
#include "mach/mt_irq.h"
#include "mach/mt_thermal.h"
#include "mach/mt_spm_idle.h"
#include "mach/mt_pmic_wrap.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_freqhopping.h"
#include "mach/mt_spm.h"
#include "mach/mt_ptp.h"

#include "mach/mtk_rtc_hal.h"
#include "mach/mt_rtc_hw.h"

/* local includes */
#include "mach/mt_cpufreq.h"

/* forward references */
extern int is_ext_buck_sw_ready(void);	/* TODO: ask James to provide the head file */
extern int ext_buck_vosel(unsigned long val);	/* TODO: ask James to provide the head file */
extern int da9210_read_byte(kal_uint8 cmd, kal_uint8 *ret_data);	/* TODO: ask James to provide the head file */
/* extern u32 get_devinfo_with_index(u32 index); /* TODO: ask sbchk_base.c owner to provide head file */ */
extern int mtktscpu_get_Tj_temp(void);	/* TODO: ask Jerry to provide the head file */
extern void (*cpufreq_freq_check) (enum mt_cpu_dvfs_id id);	/* TODO: ask Marc to provide the head file (pass big or little???) */
extern void hp_limited_cpu_num(int num);	/* TODO: ask Marc to provide the head file (pass big or little???) */


/*=============================================================*/
/* Macro definition                                            */
/*=============================================================*/

/*
 * CONFIG
 */
#define FIXME 0

#define CONFIG_CPU_DVFS_SHOWLOG 1

/* #define CONFIG_CPU_DVFS_BRINGUP 1               /* for bring up */ */
/* #define CONFIG_CPU_DVFS_RANDOM_TEST 1           /* random test for UT/IT */ */
/* #define CONFIG_CPU_DVFS_FFTT_TEST 1             /* FF TT SS volt test */ */
/* #define CONFIG_CPU_DVFS_TURBO 1                 /* turbo mode */ */
#define CONFIG_CPU_DVFS_DOWNGRADE_FREQ 1	/* downgrade freq */

#define PMIC_SETTLE_TIME(old_mv, new_mv) ((((old_mv) > (new_mv)) ? ((old_mv) - (new_mv)) : ((new_mv) - (old_mv))) * 2 / 25 + 25 + 1)	/* us, PMIC settle time, should not be changed */
#define PLL_SETTLE_TIME         (30)            /* us, PLL settle time, should not be changed */	/* TODO: sync with DE, 20us or 30us??? */
#define RAMP_DOWN_TIMES         (2)	/* RAMP DOWN TIMES to postpone frequency degrade */
#define FHCTL_CHANGE_FREQ       (1000000)       /* if cross 1GHz when DFS, don't used FHCTL */ /* TODO: rename CPUFREQ_BOUNDARY_FOR_FHCTL */

#define DEFAULT_VOLT_VGPU       (1125)
#define DEFAULT_VOLT_VCORE_AO   (1125)
#define DEFAULT_VOLT_VCORE_PDN  (1125)

			 /* for DVFS OPP table */* / TODO: necessary or just specify in opp table directly??? */ */

#define DVFS_BIG_F0 (1898000)	/* KHz */
#define DVFS_BIG_F1 (1495000)	/* KHz */
#define DVFS_BIG_F2 (1365000)	/* KHz */
#define DVFS_BIG_F3 (1248000)	/* KHz */
#define DVFS_BIG_F4 (1144000)	/* KHz */
#define DVFS_BIG_F5 (1001000)	/* KHz */
#define DVFS_BIG_F6 (806000)	/* KHz */
#define DVFS_BIG_F7 (403000)	/* KHz */

#if defined(SLT_VMAX)
#define DVFS_BIG_V0 (1150)	/* mV */
#else
#define DVFS_BIG_V0 (1100)	/* mV */
#endif
#define DVFS_BIG_V1 (1079)	/* mV */
#define DVFS_BIG_V2 (1050)	/* mV */
#define DVFS_BIG_V3 (1032)	/* mV */
#define DVFS_BIG_V4 (1000)	/* mV */
#define DVFS_BIG_V5 (963)	/* mV */
#define DVFS_BIG_V6 (914)	/* mV */
#define DVFS_BIG_V7 (814)	/* mV */

#define DVFS_LITTLE_F0 (1690000)	/* KHz */
#define DVFS_LITTLE_F1 (1495000)	/* KHz */
#define DVFS_LITTLE_F2 (1365000)	/* KHz */
#define DVFS_LITTLE_F3 (1248000)	/* KHz */
#define DVFS_LITTLE_F4 (1144000)	/* KHz */
#define DVFS_LITTLE_F5 (1001000)	/* KHz */
#define DVFS_LITTLE_F6 (806000)	/* KHz */
#define DVFS_LITTLE_F7 (403000)	/* KHz */

#if defined(SLT_VMAX)
#define DVFS_LITTLE_V0 (1150)	/* mV */
#else
#define DVFS_LITTLE_V0 (1125)	/* mV */
#endif
#define DVFS_LITTLE_V1 (1079)	/* mV */
#define DVFS_LITTLE_V2 (1050)	/* mV */
#define DVFS_LITTLE_V3 (1023)	/* mV */
#define DVFS_LITTLE_V4 (1000)	/* mV */
#define DVFS_LITTLE_V5 (963)	/* mV */
#define DVFS_LITTLE_V6 (914)	/* mV */
#define DVFS_LITTLE_V7 (814)	/* mV */

/*
 * LOG
 */
/* #define USING_XLOG */

#define HEX_FMT "0x%08x"
#undef TAG

#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "Power/cpufreq"

#define cpufreq_err(fmt, args...)       \
	xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define cpufreq_ver(fmt, args...)       \
	xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else				/* USING_XLOG */

#define TAG     "[Power/cpufreq] "

#define cpufreq_err(fmt, args...)       \
	printk(KERN_ERR TAG KERN_CONT fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	printk(KERN_WARNING TAG KERN_CONT fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	printk(KERN_NOTICE TAG KERN_CONT fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	printk(KERN_INFO TAG KERN_CONT fmt, ##args)
#define cpufreq_ver(fmt, args...)       \
	printk(KERN_DEBUG TAG KERN_CONT fmt, ##args)

#endif				/* USING_XLOG */

#define FUNC_LV_MODULE          BIT(0)	/* module, platform driver interface */
#define FUNC_LV_CPUFREQ         BIT(1)	/* cpufreq driver interface          */
#define FUNC_LV_API             BIT(2)	/* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL           BIT(3)	/* mt_cpufreq driver lcaol function  */
#define FUNC_LV_HELP            BIT(4)	/* mt_cpufreq driver help function   */

static unsigned int func_lv_mask;	/* (FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API | FUNC_LV_LOCAL | FUNC_LV_HELP); */

#if defined(CONFIG_CPU_DVFS_SHOWLOG)
#define FUNC_ENTER(lv)          do { if ((lv) & func_lv_mask) cpufreq_dbg(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv)           do { if ((lv) & func_lv_mask) cpufreq_dbg("<< %s():%d\n", __func__, __LINE__); } while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif				/* CONFIG_CPU_DVFS_SHOWLOG */

/*
 * BIT Operation
 */
#define _BIT_(_bit_)                    (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_)           ((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * REG ACCESS
 */
#define cpufreq_read(addr)                  DRV_Reg32(addr)
#define cpufreq_write(addr, val)            mt_reg_sync_writel(val, addr)
#define cpufreq_write_mask(addr, mask, val) cpufreq_write(addr, (cpufreq_read(addr) & ~(_BITMASK_(mask))) | _BITS_(mask, val))


/*=============================================================*/
/* Local type definition                                       */
/*=============================================================*/


/*=============================================================*/
/* Local variable definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Local function definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Gobal function definition                                   */
/*=============================================================*/

/*
 * LOCK
 */
#if 0   /* spinlock */		/* TODO: FIXME, it would cause warning @ big because of i2c access with atomic operation */
static DEFINE_SPINLOCK(cpufreq_lock);
#define cpufreq_lock(flags) spin_lock_irqsave(&cpufreq_lock, flags)
#define cpufreq_unlock(flags) spin_unlock_irqrestore(&cpufreq_lock, flags)
#else				/* mutex */
static DEFINE_MUTEX(cpufreq_mutex);
bool is_in_cpufreq = 0;
#define cpufreq_lock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_lock(&cpufreq_mutex); \
		is_in_cpufreq = 1; \
		spm_mcdi_wakeup_all_cores(); \
	} while (0)

#define cpufreq_unlock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		is_in_cpufreq = 0; \
		mutex_unlock(&cpufreq_mutex); \
	} while (0)
#endif

/*
 * EFUSE
 */
#define CPUFREQ_EFUSE_INDEX     (18)	/* TODO: confirm CPU efuse */

#define CPU_LEVEL_0             (0x3)
#define CPU_LEVEL_1             (0x2)
#define CPU_LEVEL_2             (0x1)

#define CPU_LV_TO_OPP_IDX(lv)   ((lv)-1)	/* cpu_level to opp_idx */

static u32 get_devinfo_with_index(u32 index)
{				/* TODO: remove it latter */
 return _BITS_(9:8, CPU_LEVEL_0);
}

static unsigned int read_efuse_cpu_speed(void)
{				/* TODO: remove it latter */
 return _GET_BITS_VAL_(9:8, get_devinfo_with_index(CPUFREQ_EFUSE_INDEX));
}

/*
 * PMIC_WRAP
 */
/* TODO: defined @ pmic head file??? */
#define VOLT_TO_PMIC_VAL(volt)  ((((volt) - 700) * 100 + 625 - 1) / 625)
#define PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) / 100 + 700)	/* (((pmic) * 625 + 100 - 1) / 100 + 700) */

#define VOLT_TO_EXTBUCK_VAL(volt) (((((volt) - 300) + 9) / 10) & 0x7F)
#define EXTBUCK_VAL_TO_VOLT(val)  (300 + ((val) & 0x7F) * 10)

		     /* PMIC WRAP ADDR // TODO: include other head file */
#define PMIC_WRAP_DVFS_ADR0     (PWRAP_BASE + 0x0E8)
#define PMIC_WRAP_DVFS_WDATA0   (PWRAP_BASE + 0x0EC)
#define PMIC_WRAP_DVFS_ADR1     (PWRAP_BASE + 0x0F0)
#define PMIC_WRAP_DVFS_WDATA1   (PWRAP_BASE + 0x0F4)
#define PMIC_WRAP_DVFS_ADR2     (PWRAP_BASE + 0x0F8)
#define PMIC_WRAP_DVFS_WDATA2   (PWRAP_BASE + 0x0FC)
#define PMIC_WRAP_DVFS_ADR3     (PWRAP_BASE + 0x100)
#define PMIC_WRAP_DVFS_WDATA3   (PWRAP_BASE + 0x104)
#define PMIC_WRAP_DVFS_ADR4     (PWRAP_BASE + 0x108)
#define PMIC_WRAP_DVFS_WDATA4   (PWRAP_BASE + 0x10C)
#define PMIC_WRAP_DVFS_ADR5     (PWRAP_BASE + 0x110)
#define PMIC_WRAP_DVFS_WDATA5   (PWRAP_BASE + 0x114)
#define PMIC_WRAP_DVFS_ADR6     (PWRAP_BASE + 0x118)
#define PMIC_WRAP_DVFS_WDATA6   (PWRAP_BASE + 0x11C)
#define PMIC_WRAP_DVFS_ADR7     (PWRAP_BASE + 0x120)
#define PMIC_WRAP_DVFS_WDATA7   (PWRAP_BASE + 0x124)

		/* PMIC ADDR // TODO: include other head file */
#define PMIC_ADDR_VPROC_CA7_VOSEL_ON      0x847C	/* [6:0]                     */
#define PMIC_ADDR_VPROC_CA7_VOSEL_SLEEP   0x847E	/* [6:0]                     */
#define PMIC_ADDR_VPROC_CA7_EN            0x8476	/* [0] (shared with others)  */
#define PMIC_ADDR_VSRAM_CA7_EN            0x8CBC	/* [10] (shared with others) */

#define PMIC_ADDR_VSRAM_CA15L_VOSEL_ON    0x0264	/* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA15L_VOSEL_SLEEP 0x0266	/* [6:0]                     */
#define PMIC_ADDR_VSRAM_CA15L_EN          0x0524	/* [10] (shared with others) */

#define PMIC_ADDR_VGPU_VOSEL_ON           0x02B0	/* [6:0]                     */
#define PMIC_ADDR_VGPU_VOSEL_SLEEP        0x02B2	/* [6:0]                     */
#define PMIC_ADDR_VCORE_AO_VOSEL_ON       0x036C	/* [6:0]                     */
#define PMIC_ADDR_VCORE_AO_VOSEL_SLEEP    0x036E	/* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_VOSEL_ON      0x024E	/* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_VOSEL_SLEEP   0x0250	/* [6:0]                     */
#define PMIC_ADDR_VCORE_PDN_EN_CTRL       0x0244	/* [0] (shared with others)  */

#define NR_PMIC_WRAP_CMD 8	/* num of pmic wrap cmd (fixed value) */

struct pmic_wrap_setting {
	enum pmic_wrap_phase_id phase;

	struct {
		const unsigned int cmd_addr;
		const unsigned int cmd_wdata;
	} addr[NR_PMIC_WRAP_CMD];

	struct {
		struct {
			const unsigned int cmd_addr;
			unsigned int cmd_wdata;
		} _[NR_PMIC_WRAP_CMD];
		const int nr_idx;
	} set[NR_PMIC_WRAP_PHASE];
};

static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
	.addr = {
		 {PMIC_WRAP_DVFS_ADR0, PMIC_WRAP_DVFS_WDATA0,},
		 {PMIC_WRAP_DVFS_ADR1, PMIC_WRAP_DVFS_WDATA1,},
		 {PMIC_WRAP_DVFS_ADR2, PMIC_WRAP_DVFS_WDATA2,},
		 {PMIC_WRAP_DVFS_ADR3, PMIC_WRAP_DVFS_WDATA3,},
		 {PMIC_WRAP_DVFS_ADR4, PMIC_WRAP_DVFS_WDATA4,},
		 {PMIC_WRAP_DVFS_ADR5, PMIC_WRAP_DVFS_WDATA5,},
		 {PMIC_WRAP_DVFS_ADR6, PMIC_WRAP_DVFS_WDATA6,},
		 {PMIC_WRAP_DVFS_ADR7, PMIC_WRAP_DVFS_WDATA7,},
		 },

	.set[PMIC_WRAP_PHASE_NORMAL] = {
					._[IDX_NM_VSRAM_CA15L] = {PMIC_ADDR_VSRAM_CA15L_VOSEL_ON,
								  VOLT_TO_PMIC_VAL(DVFS_BIG_V0),},
					._[IDX_NM_VPROC_CA7] = {PMIC_ADDR_VPROC_CA7_VOSEL_ON,
								VOLT_TO_PMIC_VAL(DVFS_LITTLE_V0),},
					._[IDX_NM_VGPU] = {PMIC_ADDR_VGPU_VOSEL_ON,
							   VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VGPU),},
					._[IDX_NM_VCORE_AO] = {PMIC_ADDR_VCORE_AO_VOSEL_ON,
							       VOLT_TO_PMIC_VAL
							       (DEFAULT_VOLT_VCORE_AO),},
					._[IDX_NM_VCORE_PDN] = {PMIC_ADDR_VCORE_PDN_VOSEL_ON,
								VOLT_TO_PMIC_VAL
								(DEFAULT_VOLT_VCORE_PDN),},
					.nr_idx = NR_IDX_NM,
					},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
 ._[IDX_SP_VSRAM_CA15L_PWR_ON] = {PMIC_ADDR_VSRAM_CA15L_EN, _BITS_(11:10, 1) | _BIT_(8),},
					 /* RG_VSRAM_DVFS1_ON_CTRL = 1'b0, RG_VSRAM_DVFS1_EN = 1'b1, RG_VSRAM_DVFS1_STBTD = 1'b1 */
 ._[IDX_SP_VSRAM_CA15L_SHUTDOWN] = {PMIC_ADDR_VSRAM_CA15L_EN, _BITS_(11:10, 0) | _BIT_(8),},
					 /* RG_VSRAM_DVFS1_ON_CTRL = 1'b0, RG_VSRAM_DVFS1_EN = 1'b0, RG_VSRAM_DVFS1_STBTD = 1'b1 */
 ._[IDX_SP_VPROC_CA7_PWR_ON] = {PMIC_ADDR_VPROC_CA7_EN, _BITS_(0:0, 1),},
					 /* VDVFS2_EN = 1'b1                                                                     */
 ._[IDX_SP_VPROC_CA7_SHUTDOWN] = {PMIC_ADDR_VPROC_CA7_EN, _BITS_(0:0, 0),},
					 /* VDVFS2_EN = 1'b0                                                                     */
 ._[IDX_SP_VSRAM_CA7_PWR_ON] = {PMIC_ADDR_VSRAM_CA7_EN, _BITS_(11:10, 1) | _BIT_(8),},
					 /* RG_VSRAM_DVFS2_ON_CTRL = 1'b0, RG_VSRAM_DVFS2_EN = 1'b1, RG_VSRAM_DVFS2_STBTD = 1'b1 */
 ._[IDX_SP_VSRAM_CA7_SHUTDOWN] = {PMIC_ADDR_VSRAM_CA7_EN, _BITS_(11:10, 0) | _BIT_(8),},
					 /* RG_VSRAM_DVFS2_ON_CTRL = 1'b0, RG_VSRAM_DVFS2_EN = 1'b0, RG_VSRAM_DVFS2_STBTD = 1'b1 */
 ._[IDX_SP_VCORE_PDN_EN_HW_MODE] = {PMIC_ADDR_VCORE_PDN_EN_CTRL, _BITS_(1:0, 3),},
					 /* VDVFS11_VOSEL_CTRL = 1'b1, VDVFS11_EN_CTRL = 1'b1                                    */
 ._[IDX_SP_VCORE_PDN_EN_SW_MODE] = {PMIC_ADDR_VCORE_PDN_EN_CTRL, _BITS_(1:0, 2),},
					 /* VDVFS11_VOSEL_CTRL = 1'b1, VDVFS11_EN_CTRL = 1'b0                                    */
					 .nr_idx = NR_IDX_SP,
					 },

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
					  ._[IDX_DI_VSRAM_CA15L_NORMAL] =
					  {PMIC_ADDR_VSRAM_CA15L_VOSEL_ON, VOLT_TO_PMIC_VAL(1000),},
					  ._[IDX_DI_VSRAM_CA15L_SLEEP] =
					  {PMIC_ADDR_VSRAM_CA15L_VOSEL_ON, VOLT_TO_PMIC_VAL(700),},
					  ._[IDX_DI_VPROC_CA7_NORMAL] =
					  {PMIC_ADDR_VPROC_CA7_VOSEL_ON, VOLT_TO_PMIC_VAL(1000),},
					  ._[IDX_DI_VPROC_CA7_SLEEP] =
					  {PMIC_ADDR_VPROC_CA7_VOSEL_ON, VOLT_TO_PMIC_VAL(700),},
					  ._[IDX_DI_VCORE_AO_NORMAL] =
					  {PMIC_ADDR_VCORE_AO_VOSEL_ON, VOLT_TO_PMIC_VAL(1125),},
					  ._[IDX_DI_VCORE_AO_SLEEP] =
					  {PMIC_ADDR_VCORE_AO_VOSEL_ON, VOLT_TO_PMIC_VAL(900),},
					  ._[IDX_DI_VCORE_PDN_NORMAL] =
					  {PMIC_ADDR_VCORE_PDN_VOSEL_ON, VOLT_TO_PMIC_VAL(1125),},
					  ._[IDX_DI_VCORE_PDN_SLEEP] =
					  {PMIC_ADDR_VCORE_PDN_VOSEL_ON, VOLT_TO_PMIC_VAL(900),},
					  .nr_idx = NR_IDX_DI,
					  },
};

#if 0				/* spinlock */
static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)
#else				/* mutex */
static DEFINE_MUTEX(pmic_wrap_mutex);

#define pmic_wrap_lock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_lock(&pmic_wrap_mutex); \
	} while (0)

#define pmic_wrap_unlock(flags) \
	do { \
		/* to fix compile warning */  \
		flags = (unsigned long)&flags; \
		mutex_unlock(&pmic_wrap_mutex); \
	} while (0)
#endif

static int _spm_dvfs_ctrl_volt(u32 value)
{
#define MAX_RETRY_COUNT (100)

	u32 ap_dvfs_con;
	int retry = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));	/* TODO: FIXME */

	ap_dvfs_con = spm_read(SPM_AP_DVFS_CON_SET);
	spm_write(SPM_AP_DVFS_CON_SET, (ap_dvfs_con & ~(0x7)) | value);
	udelay(5);

	while ((spm_read(SPM_AP_DVFS_CON_SET) & (0x1 << 31)) == 0) {
		if (retry >= MAX_RETRY_COUNT) {
			cpufreq_err("FAIL: no response from PMIC wrapper\n");
			return -1;
		}

		retry++;
		cpufreq_dbg("wait for ACK signal from PMIC wrapper, retry = %d\n", retry);

		udelay(5);
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase)
{
	int i;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);

#if 0				/* TODO: FIXME, check IPO-H case */

	if (pw.phase == phase)
		return;

#endif

	pmic_wrap_lock(flags);

	pw.phase = phase;

	for (i = 0; i < pw.set[phase].nr_idx; i++) {
		cpufreq_write(pw.addr[i].cmd_addr, pw.set[phase]._[i].cmd_addr);
		cpufreq_write(pw.addr[i].cmd_wdata, pw.set[phase]._[i].cmd_wdata);
	}

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_set_pmic_phase);

void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata)
{				/* just set wdata value */
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);
	BUG_ON(idx >= pw.set[phase].nr_idx);

	pmic_wrap_lock(flags);

	pw.set[phase]._[idx].cmd_wdata = cmd_wdata;

	if (pw.phase == phase)
		cpufreq_write(pw.addr[idx].cmd_wdata, cmd_wdata);

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_set_pmic_cmd);

void mt_cpufreq_apply_pmic_cmd(int idx)
{				/* kick spm */
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(idx >= pw.set[pw.phase].nr_idx);

	pmic_wrap_lock(flags);

	_spm_dvfs_ctrl_volt(idx);

	pmic_wrap_unlock(flags);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_apply_pmic_cmd);

/* cpu voltage sampler */
static cpuVoltsampler_func g_pCpuVoltSampler;

void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_registerCB);

/* SDIO */
void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled)
{
	/* empty function */
}

void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt)
{				/* unit: mv x 100 */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_AO,
				VOLT_TO_PMIC_VAL(volt / 100));
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_PDN,
				VOLT_TO_PMIC_VAL(volt / 100));
}

unsigned int mt_vcore_dvfs_volt_get_by_sdio(void)
{
	unsigned int rdata;

	pwrap_read(PMIC_ADDR_VCORE_AO_VOSEL_ON, &rdata);

	return PMIC_VAL_TO_VOLT(rdata) * 100;
}

/*
 * mt_cpufreq driver
 */
#define MAX_CPU_NUM 4		/* for limited_max_ncpu */

#define OP(khz, volt) {                 \
	.cpufreq_khz = khz,             \
	.cpufreq_volt = volt,           \
	.cpufreq_volt_org = volt,       \
	}

struct mt_cpu_freq_info {
	const unsigned int cpufreq_khz;
	unsigned int cpufreq_volt;
	const unsigned int cpufreq_volt_org;
};

struct mt_cpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_ncpu;
	unsigned int cpufreq_power;
};

struct mt_cpu_dvfs;

struct mt_cpu_dvfs_ops {
	/* for thermal */
	void (*protect) (struct mt_cpu_dvfs *p, unsigned int limited_power);	/* set power limit by thermal */* / TODO: sync with mt_cpufreq_thermal_protect() */ */
	unsigned int (*get_temp) (struct mt_cpu_dvfs *p);	/* return temperature         */* / TODO: necessary??? */ */
	int (*setup_power_table) (struct mt_cpu_dvfs *p);

	/* for freq change (PLL/MUX) */
	unsigned int (*get_cur_phy_freq) (struct mt_cpu_dvfs *p);	/* return (physical) freq (KHz) */
	void (*set_cur_freq) (struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz);	/* set freq  */

	/* for volt change (PMICWRAP/extBuck) */
	unsigned int (*get_cur_volt) (struct mt_cpu_dvfs *p);	/* return volt (mV)                        */
	int (*set_cur_volt) (struct mt_cpu_dvfs *p, unsigned int mv);	/* set volt, return 0 (success), -1 (fail) */
};

struct mt_cpu_dvfs {
	const char *name;
	unsigned int cpu_id;	/* for cpufreq */
	struct mt_cpu_dvfs_ops *ops;

	/* opp (freq) table */
	struct mt_cpu_freq_info *opp_tbl;	/* OPP table */
	int nr_opp_tbl;		/* size for OPP table */
	int idx_opp_tbl;	/* current OPP idx */
	int idx_normal_max_opp;	/* idx for normal max OPP */

	struct cpufreq_frequency_table *freq_tbl_for_cpufreq;	/* freq table for cpufreq */

	/* power table */
	struct mt_cpu_power_info *power_tbl;
	unsigned int nr_power_tbl;

	/* enable/disable DVFS function */
	int dvfs_disable_count;
	bool cpufreq_pause;
	bool dvfs_disable_by_ptpod;
	bool limit_max_freq_early_suspend;	/* TODO: rename it to dvfs_disable_by_early_suspend */
	bool is_fixed_freq;	/* TODO: FIXME */

	/* limit for thermal */
	unsigned int limited_max_ncpu;
	unsigned int limited_max_freq;
	/* unsigned int limited_min_freq; // TODO: remove it??? */

	unsigned int thermal_protect_limited_power;

	/* limit for HEVC (via. sysfs) */
	unsigned int limited_freq_by_hevc;

	/* for ramp down */
	int ramp_down_count;

	/* param for micro throttling */
	bool downgrade_freq_for_ptpod;

	int over_max_cpu;
	int ptpod_temperature_limit_1;
	int ptpod_temperature_limit_2;
	int ptpod_temperature_time_1;
	int ptpod_temperature_time_2;

	int pre_online_cpu;
	unsigned int pre_freq;
	unsigned int downgrade_freq;

	unsigned int downgrade_freq_counter;
	unsigned int downgrade_freq_counter_return;

	unsigned int downgrade_freq_counter_limit;
	unsigned int downgrade_freq_counter_return_limit;
};

/* for thermal */
static int setup_power_table(struct mt_cpu_dvfs *p);

/* for freq change (PLL/MUX) */
static unsigned int get_cur_phy_freq(struct mt_cpu_dvfs *p);
static void set_cur_freq(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz);

/* for volt change (PMICWRAP/extBuck) */
static unsigned int get_cur_volt_little(struct mt_cpu_dvfs *p);
static int set_cur_volt_little(struct mt_cpu_dvfs *p, unsigned int mv);

static unsigned int get_cur_volt_big(struct mt_cpu_dvfs *p);
static int set_cur_volt_big(struct mt_cpu_dvfs *p, unsigned int mv);

static struct mt_cpu_dvfs_ops little_ops = {
	.setup_power_table = setup_power_table,

	.get_cur_phy_freq = get_cur_phy_freq,
	.set_cur_freq = set_cur_freq,

	.get_cur_volt = get_cur_volt_little,
	.set_cur_volt = set_cur_volt_little,
};

static struct mt_cpu_dvfs_ops big_ops = {
	.setup_power_table = setup_power_table,

	.get_cur_phy_freq = get_cur_phy_freq,
	.set_cur_freq = set_cur_freq,

	.get_cur_volt = get_cur_volt_big,
	.set_cur_volt = set_cur_volt_big,
};

static struct mt_cpu_dvfs cpu_dvfs[] = {	/* TODO: FIXME, big/LITTLE exclusive, NR_MT_CPU_DVFS */
	[MT_CPU_DVFS_LITTLE] = {
				.name = __stringify(MT_CPU_DVFS_LITTLE),
				.cpu_id = MT_CPU_DVFS_LITTLE,	/* TODO: FIXME */
				.ops = &little_ops,

				.over_max_cpu = 4,
				.ptpod_temperature_limit_1 = 45000,
				.ptpod_temperature_limit_2 = 65000,
				.ptpod_temperature_time_1 = 1,
				.ptpod_temperature_time_2 = 4,
				.pre_online_cpu = 0,
				.pre_freq = 0,
				.downgrade_freq = 0,
				.downgrade_freq_counter = 0,
				.downgrade_freq_counter_return = 0,
				.downgrade_freq_counter_limit = 0,
				.downgrade_freq_counter_return_limit = 0,
				},

	[MT_CPU_DVFS_BIG] = {
			     .name = __stringify(MT_CPU_DVFS_BIG),
			     .cpu_id = MT_CPU_DVFS_BIG,	/* TODO: FIXME */
			     .ops = &big_ops,

			     .over_max_cpu = 4,
			     .ptpod_temperature_limit_1 = 45000,
			     .ptpod_temperature_limit_2 = 65000,
			     .ptpod_temperature_time_1 = 1,
			     .ptpod_temperature_time_2 = 4,
			     .pre_online_cpu = 0,
			     .pre_freq = 0,
			     .downgrade_freq = 0,
			     .downgrade_freq_counter = 0,
			     .downgrade_freq_counter_return = 0,
			     .downgrade_freq_counter_limit = 0,
			     .downgrade_freq_counter_return_limit = 0,
			     },
};

#define for_each_cpu_dvfs(i, p)                 for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])
#define cpu_dvfs_is(p, id)                      (p == &cpu_dvfs[id])
#define cpu_dvfs_is_availiable(p)               (p->opp_tbl)
#define cpu_dvfs_get_name(p)                    (p->name)

#define cpu_dvfs_get_cur_freq(p)                (p->opp_tbl[p->idx_opp_tbl].cpufreq_khz)
#define cpu_dvfs_get_freq_by_idx(p, idx)        (p->opp_tbl[idx].cpufreq_khz)
#define cpu_dvfs_get_max_freq(p)                (p->opp_tbl[0].cpufreq_khz)
#define cpu_dvfs_get_normal_max_freq(p)         (p->opp_tbl[p->idx_normal_max_opp].cpufreq_khz)
#define cpu_dvfs_get_min_freq(p)                (p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz)

#define cpu_dvfs_get_cur_volt(p)                (p->opp_tbl[p->idx_opp_tbl].cpufreq_volt)
#define cpu_dvfs_get_volt_by_idx(p, idx)        (p->opp_tbl[idx].cpufreq_volt)

static struct mt_cpu_dvfs *id_to_cpu_dvfs(enum mt_cpu_dvfs_id id)
{
	return (id < NR_MT_CPU_DVFS) ? &cpu_dvfs[id] : NULL;
}

/* DVFS OPP table */

#define NR_MAX_OPP_TBL  8	/* TODO: refere to PTP-OD */
#define NR_MAX_CPU      4	/* TODO: one cluster, any kernel define for this - CONFIG_NR_CPU??? */

/* LITTLE CPU LEVEL 0 */
static struct mt_cpu_freq_info opp_tbl_little_e1_0[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 1 */
static struct mt_cpu_freq_info opp_tbl_little_e1_1[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* LITTLE CPU LEVEL 2 */
static struct mt_cpu_freq_info opp_tbl_little_e1_2[] = {
	OP(DVFS_LITTLE_F0, DVFS_LITTLE_V0),
	OP(DVFS_LITTLE_F1, DVFS_LITTLE_V1),
	OP(DVFS_LITTLE_F2, DVFS_LITTLE_V2),
	OP(DVFS_LITTLE_F3, DVFS_LITTLE_V3),
	OP(DVFS_LITTLE_F4, DVFS_LITTLE_V4),
	OP(DVFS_LITTLE_F5, DVFS_LITTLE_V5),
	OP(DVFS_LITTLE_F6, DVFS_LITTLE_V6),
	OP(DVFS_LITTLE_F7, DVFS_LITTLE_V7),
};

/* big CPU LEVEL 0 */
static struct mt_cpu_freq_info opp_tbl_big_e1_0[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

/* big CPU LEVEL 1 */
static struct mt_cpu_freq_info opp_tbl_big_e1_1[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

/* big CPU LEVEL 2 */
static struct mt_cpu_freq_info opp_tbl_big_e1_2[] = {
	OP(DVFS_BIG_F0, DVFS_BIG_V0),
	OP(DVFS_BIG_F1, DVFS_BIG_V1),
	OP(DVFS_BIG_F2, DVFS_BIG_V2),
	OP(DVFS_BIG_F3, DVFS_BIG_V3),
	OP(DVFS_BIG_F4, DVFS_BIG_V4),
	OP(DVFS_BIG_F5, DVFS_BIG_V5),
	OP(DVFS_BIG_F6, DVFS_BIG_V6),
	OP(DVFS_BIG_F7, DVFS_BIG_V7),
};

struct opp_tbl_info {
	struct mt_cpu_freq_info *const opp_tbl;
	const int size;
};

#define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)

static struct opp_tbl_info opp_tbls_little[] = {
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = {ARRAY_AND_SIZE(opp_tbl_little_e1_0),},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = {ARRAY_AND_SIZE(opp_tbl_little_e1_1),},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = {ARRAY_AND_SIZE(opp_tbl_little_e1_2),},
};

static struct opp_tbl_info opp_tbls_big[] = {
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)] = {ARRAY_AND_SIZE(opp_tbl_big_e1_0),},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_1)] = {ARRAY_AND_SIZE(opp_tbl_big_e1_1),},
	[CPU_LV_TO_OPP_IDX(CPU_LEVEL_2)] = {ARRAY_AND_SIZE(opp_tbl_big_e1_2),},
};

/* for PTP-OD */

static int _set_cur_volt(struct mt_cpu_dvfs *p, unsigned int mv)
{
	int ret = -1;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}
	/* update for deep idle */* / TODO: why need this??? */ */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
				cpu_dvfs_is(p,
					    MT_CPU_DVFS_LITTLE) ? IDX_DI_VPROC_CA7_NORMAL :
				IDX_DI_VSRAM_CA15L_NORMAL,
				VOLT_TO_PMIC_VAL(cpu_dvfs_get_volt_by_idx(p, p->idx_normal_max_opp))
	    );

	/* set volt */
	ret = p->ops->set_cur_volt(p, mv);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

static int _restore_default_volt(struct mt_cpu_dvfs *p)
{
	unsigned long flags;
	int i;
	int ret = -1;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_HELP);
		return 0;
	}

	cpufreq_lock(flags);	/* TODO: is it necessary??? */

	/* restore to default volt */
	for (i = 0; i < p->nr_opp_tbl; i++)
		p->opp_tbl[i].cpufreq_volt = p->opp_tbl[i].cpufreq_volt_org;

	cpufreq_unlock(flags);	/* TODO: is it necessary??? */

	/* set volt */
	ret = _set_cur_volt(p, cpu_dvfs_get_cur_volt(p));

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

unsigned int mt_cpufreq_max_frequency_by_DVS(enum mt_cpu_dvfs_id id, int idx)
{				/* TODO: rename to mt_cpufreq_get_freq_by_idx() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	BUG_ON(idx >= p->nr_opp_tbl);

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
}
EXPORT_SYMBOL(mt_cpufreq_max_frequency_by_DVS);

int mt_cpufreq_voltage_set_by_ptpod(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl)
{				/* TODO: rename to mt_cpufreq_update_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	/* unsigned long flags; // Fixed for build warning */
	int i;
	int ret = -1;

	FUNC_ENTER(FUNC_LV_API);

#if 0				/* TODO: remove it latter */
	if (id != 0)
		return 0;	/* TODO: FIXME, just for E1 */
#endif				/* TODO: remove it latter */

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	BUG_ON(nr_volt_tbl > p->nr_opp_tbl);

	/* cpufreq_lock(flags); // TODO: is it necessary??? */

	/* update volt table */
	for (i = 0; i < nr_volt_tbl; i++)
		p->opp_tbl[i].cpufreq_volt = PMIC_VAL_TO_VOLT(volt_tbl[i]);

	/* cpufreq_unlock(flags); // TODO: is it necessary??? */

	/* set volt */
	ret = _set_cur_volt(p, cpu_dvfs_get_cur_volt(p));

	FUNC_EXIT(FUNC_LV_API);

	return ret;
}
EXPORT_SYMBOL(mt_cpufreq_voltage_set_by_ptpod);

void mt_cpufreq_return_default_DVS_by_ptpod(enum mt_cpu_dvfs_id id)
{				/* TODO: rename to mt_cpufreq_restore_default_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_availiable(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return;
	}

	_restore_default_volt(p);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_return_default_DVS_by_ptpod);

/* for freq change (PLL/MUX) */

/* #define PLL_MAX_FREQ            (1989000)       /* KHz */ // TODO: check max freq */ */
#define PLL_MIN_FREQ            (130000)	/* KHz */
#define PLL_DIV1_FREQ           (1001000)	/* KHz */
#define PLL_DIV2_FREQ           (520000)	/* KHz */
#define PLL_DIV4_FREQ           (260000)	/* KHz */
#define PLL_DIV8_FREQ           (PLL_MIN_FREQ)	/* KHz */

#define DDS_DIV1_FREQ           (0x0009A000)	/* 1001MHz */
#define DDS_DIV2_FREQ           (0x010A0000)	/* 520MHz  */
#define DDS_DIV4_FREQ           (0x020A0000)	/* 260MHz  */
#define DDS_DIV8_FREQ           (0x030A0000)	/* 130MHz  */

static unsigned int _cpu_freq_calc(unsigned int con1, unsigned int ckdiv1)
{
	unsigned int freq = 0;

#if 0				/* method 1 */
	static const unsigned int pll_vcodivsel_map[2] = { 1, 2 };
	static const unsigned int pll_prediv_map[4] = { 1, 2, 4, 4 };
	static const unsigned int pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };
	static const unsigned int pll_fbksel_map[4] = { 1, 2, 4, 4 };
	static const unsigned int pll_n_info_map[14] = {	/* assume fin = 26MHz */
		13000000,
		6500000,
		3250000,
		1625000,
		812500,
		406250,
		203125,
		101563,
		50782,
		25391,
		12696,
		6348,
		3174,
		1587,
	};

 unsigned int posdiv = _GET_BITS_VAL_(26:24, con1);
	unsigned int vcodivsel = 0;	/* _GET_BITS_VAL_(19 : 19, con0); *//* XXX: always zero */
	unsigned int prediv = 0;	/* _GET_BITS_VAL_(5 : 4, con0);   *//* XXX: always zero */
 unsigned int n_info_i = _GET_BITS_VAL_(20:14, con1);
 unsigned int n_info_f = _GET_BITS_VAL_(13:0, con1);

	int i;
	unsigned int mask;
	unsigned int vco_i = 0;
	unsigned int vco_f = 0;

	posdiv = pll_posdiv_map[posdiv];
	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	vco_i = 26 * n_info_i;

	for (i = 0; i < 14; i++) {
		mask = 1U << (13 - i);

		if (n_info_f & mask) {
			vco_f += pll_n_info_map[i];

			if (!(n_info_f & (mask - 1)))	/* could break early if remaining bits are 0 */
				break;
		}
	}

	vco_f = (vco_f + 1000000 / 2) / 1000000;	/* round up */

	freq = (vco_i + vco_f) * 1000 * vcodivsel / prediv / posdiv;	/* KHz */
#else				/* method 2 */
 con1 &= _BITMASK_(26:0);

	if (con1 >= DDS_DIV8_FREQ) {
		freq = DDS_DIV8_FREQ;
		freq = PLL_DIV8_FREQ + (((con1 - freq) / 0x2000) * 13000 / 8);
	} else if (con1 >= DDS_DIV4_FREQ) {
		freq = DDS_DIV4_FREQ;
		freq = PLL_DIV4_FREQ + (((con1 - freq) / 0x2000) * 13000 / 4);
	} else if (con1 >= DDS_DIV2_FREQ) {
		freq = DDS_DIV2_FREQ;
		freq = PLL_DIV2_FREQ + (((con1 - freq) / 0x2000) * 13000 / 2);
	} else if (con1 >= DDS_DIV1_FREQ) {
		freq = DDS_DIV1_FREQ;
		freq = PLL_DIV1_FREQ + (((con1 - freq) / 0x2000) * 13000);
	} else
		BUG();

#endif

	FUNC_ENTER(FUNC_LV_HELP);

	switch (ckdiv1) {
	case 9:
		freq = freq * 3 / 4;
		break;

	case 10:
		freq = freq * 2 / 4;
		break;

	case 11:
		freq = freq * 1 / 4;
		break;

	case 17:
		freq = freq * 4 / 5;
		break;

	case 18:
		freq = freq * 3 / 5;
		break;

	case 19:
		freq = freq * 2 / 5;
		break;

	case 20:
		freq = freq * 1 / 5;
		break;

	case 25:
		freq = freq * 5 / 6;
		break;

	case 26:
		freq = freq * 4 / 6;
		break;

	case 27:
		freq = freq * 3 / 6;
		break;

	case 28:
		freq = freq * 2 / 6;
		break;

	case 29:
		freq = freq * 1 / 6;
		break;

	case 8:
	case 16:
	case 24:
		break;

	default:
		/* BUG(); // TODO: FIXME */
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return freq;		/* TODO: adjust by ptp level??? */
}

static unsigned int get_cur_phy_freq(struct mt_cpu_dvfs *p)
{
	unsigned int con1;
	unsigned int ckdiv1;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	con1 =
	    cpufreq_read(4 +
			 (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON0 : ARMCA15PLL_CON0));

	ckdiv1 = cpufreq_read(TOP_CKDIV1);
 ckdiv1 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? _GET_BITS_VAL_(4 : 0, ckdiv1) : _GET_BITS_VAL_(9:5, ckdiv1);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return _cpu_freq_calc(con1, ckdiv1);
}

static unsigned int _mt_cpufreq_get_cur_phy_freq(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return p->ops->get_cur_phy_freq(p);
}

static unsigned int _cpu_dds_calc(unsigned int khz)
{				/* XXX: NOT OK FOR 1007.5MHz */
	unsigned int dds;

	FUNC_ENTER(FUNC_LV_HELP);

	if (khz >= PLL_DIV1_FREQ)
		dds = DDS_DIV1_FREQ + ((khz - PLL_DIV1_FREQ) / 13000) * 0x2000;
	else if (khz >= PLL_DIV2_FREQ)
		dds = DDS_DIV2_FREQ + ((khz - PLL_DIV2_FREQ) * 2 / 13000) * 0x2000;
	else if (khz >= PLL_DIV4_FREQ)
		dds = DDS_DIV4_FREQ + ((khz - PLL_DIV4_FREQ) * 4 / 13000) * 0x2000;
	else if (khz >= PLL_DIV8_FREQ)
		dds = DDS_DIV8_FREQ + ((khz - PLL_DIV8_FREQ) * 8 / 13000) * 0x2000;
	else
		BUG();

	FUNC_EXIT(FUNC_LV_HELP);

	return dds;
}

static void _cpu_clock_switch(struct mt_cpu_dvfs *p, enum top_ckmuxsel sel)
{
	unsigned int val = cpufreq_read(TOP_CKMUXSEL);
 unsigned int mask = _BITMASK_(1:0);

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(sel >= NR_TOP_CKMUXSEL);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG)) {
		sel <<= 2;
		mask <<= 2;	/* _BITMASK_(3 : 2) */
	}

	cpufreq_write(TOP_CKMUXSEL, (val & ~mask) | sel);

	FUNC_EXIT(FUNC_LV_HELP);
}

static enum top_ckmuxsel _get_cpu_clock_switch(struct mt_cpu_dvfs *p)
{
	unsigned int val = cpufreq_read(TOP_CKMUXSEL);
 unsigned int mask = _BITMASK_(1:0);

	FUNC_ENTER(FUNC_LV_HELP);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_BIG))
		val = (val & (mask << 2)) >> 2;	/* _BITMASK_(3 : 2) */
	else
		val &= mask;	/* _BITMASK_(1 : 0) */

	FUNC_EXIT(FUNC_LV_HELP);

	return val;
}

int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	if (!p)
		return -1;

	_cpu_clock_switch(p, sel);

	return 0;
}

enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	if (!p)
		return -1;

	return _get_cpu_clock_switch(p);
}

static void set_cur_freq(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz)
{
	unsigned int addr_con1;
	unsigned int dds;
	unsigned int is_fhctl_used;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (cur_khz < PLL_DIV1_FREQ && PLL_DIV1_FREQ < target_khz) {
		set_cur_freq(p, cur_khz, PLL_DIV1_FREQ);
		cur_khz = PLL_DIV1_FREQ;
	}

	addr_con1 = 4 + (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_CON0 : ARMCA15PLL_CON0);

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	is_fhctl_used = 0;
#else
	is_fhctl_used = (PLL_DIV1_FREQ < target_khz) ? 1 : 0;
#endif

	/* calc dds */
	dds = _cpu_dds_calc(target_khz);

	if (!is_fhctl_used) {
		/* enable_clock(MT_CG_MPLL_D2, "CPU_DVFS"); */
		_cpu_clock_switch(p, TOP_CKMUXSEL_MAINPLL);
	}

	/* set dds */
	if (!is_fhctl_used)
		cpufreq_write(addr_con1, dds | _BIT_(31));	/* CHG */
	else {
 BUG_ON(dds & _BITMASK_(26:24));
		/* should not use posdiv */
#ifndef __KERNEL__
		freqhopping_dvt_dvfs_enable(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? ARMCA7PLL_ID :
					    ARMCA15PLL_ID, target_khz);
#else				/* __KERNEL__ */
		mt_dfs_armpll(cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? FH_ARMCA7_PLLID :
			      FH_ARMCA15_PLLID, dds);
#endif				/* ! __KERNEL__ */
	}

	udelay(PLL_SETTLE_TIME);

	if (!is_fhctl_used) {
		_cpu_clock_switch(p, TOP_CKMUXSEL_ARMPLL);
		/* disable_clock(MT_CG_MPLL_D2, "CPU_DVFS"); */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* for volt change (PMICWRAP/extBuck) */

static unsigned int get_cur_volt_little(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VPROC_CA7_EN, &rdata);

 rdata &= _BITMASK_(0:0);	/* enable or disable (i.e. 0mv or not) */

	if (rdata) {		/* enabled i.e. not 0mv */
		pwrap_read(PMIC_ADDR_VPROC_CA7_VOSEL_ON, &rdata);

		rdata = PMIC_VAL_TO_VOLT(rdata);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata;		/* mv: vproc */
}

static unsigned int get_cur_vsram_big(struct mt_cpu_dvfs *p)
{
	unsigned int rdata;

	FUNC_ENTER(FUNC_LV_LOCAL);

	pwrap_read(PMIC_ADDR_VSRAM_CA15L_EN, &rdata);

 rdata &= _BITMASK_(10:10);	/* enable or disable (i.e. 0mv or not) */

	if (rdata) {		/* enabled i.e. not 0mv */
		pwrap_read(PMIC_ADDR_VSRAM_CA15L_VOSEL_ON, &rdata);

		rdata = PMIC_VAL_TO_VOLT(rdata);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return rdata;		/* mv: vproc */
}

static unsigned int get_cur_volt_big(struct mt_cpu_dvfs *p)
{
	unsigned char ret_val;
	unsigned int ret_mv;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (!da9210_read_byte(0xD8, &ret_val)) {	/* TODO: FIXME, it is better not to access da9210 directly */
		cpufreq_err("%s(), fail to read ext buck volt\n", __func__);
		ret_mv = 0;
	} else
		ret_mv = EXTBUCK_VAL_TO_VOLT(ret_val);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret_mv;
}

unsigned int mt_cpufreq_cur_vproc(enum mt_cpu_dvfs_id id)
{				/* TODO: rename it to mt_cpufreq_get_cur_volt() */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->ops);

	FUNC_EXIT(FUNC_LV_API);

	return p->ops->get_cur_volt(p);
}
EXPORT_SYMBOL(mt_cpufreq_cur_vproc);

static int set_cur_volt_little(struct mt_cpu_dvfs *p, unsigned int mv)
{				/* mv: vproc */
	unsigned int cur_mv = get_cur_volt_little(p);

	FUNC_ENTER(FUNC_LV_LOCAL);

	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VPROC_CA7, VOLT_TO_PMIC_VAL(mv));
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VPROC_CA7);

	/* delay for scaling up */
	if (mv > cur_mv)
		udelay(PMIC_SETTLE_TIME(cur_mv, mv));

	if (NULL != g_pCpuVoltSampler) {
		g_pCpuVoltSampler(MT_CPU_DVFS_LITTLE, mv);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

static int set_cur_volt_big(struct mt_cpu_dvfs *p, unsigned int mv)
{				/* mv: vproc */
#define MIN_DIFF_VSRAM_PROC     10
#define NORMAL_DIFF_VRSAM_VPROC 50
#define MAX_DIFF_VSRAM_VPROC    200

	unsigned int cur_vsram_mv = get_cur_vsram_big(p);
	unsigned int cur_vproc_mv = get_cur_volt_big(p);
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))));	/* <-XXX */

	/* UP */
	if (mv > cur_vproc_mv) {
		unsigned int target_vsram_mv = mv + NORMAL_DIFF_VRSAM_VPROC;
		unsigned int next_vsram_mv;

		do {
			next_vsram_mv = min((MAX_DIFF_VSRAM_VPROC + cur_vproc_mv), target_vsram_mv);

			/* update vsram */
			cur_vsram_mv = next_vsram_mv;
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L,
						VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);

			BUG_ON(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))));	/* <-XXX */

			/* update vproc */
			cur_vproc_mv = cur_vsram_mv - NORMAL_DIFF_VRSAM_VPROC;

			if (!ext_buck_vosel(cur_vproc_mv * 100)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}

			udelay(PMIC_SETTLE_TIME(cur_vproc_mv - MAX_DIFF_VSRAM_VPROC, cur_vproc_mv));	/* TODO: always fix max gap <- refine it??? */
		} while (target_vsram_mv > cur_vsram_mv);
	}
	/* DOWN */
	else if (mv < cur_vproc_mv) {
		int next_vproc_mv;

		do {
			next_vproc_mv = max((cur_vsram_mv - MAX_DIFF_VSRAM_VPROC), mv);

			/* update vproc */
			cur_vproc_mv = next_vproc_mv;

			if (!ext_buck_vosel(cur_vproc_mv * 100)) {
				cpufreq_err("%s(), fail to set ext buck volt\n", __func__);
				ret = -1;
			}

			BUG_ON(!((cur_vsram_mv > cur_vproc_mv) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram_mv - cur_vproc_mv))));	/* <-XXX */

			/* update vsram */
			cur_vsram_mv = cur_vproc_mv + NORMAL_DIFF_VRSAM_VPROC;
			mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VSRAM_CA15L,
						VOLT_TO_PMIC_VAL(cur_vsram_mv));
			mt_cpufreq_apply_pmic_cmd(IDX_NM_VSRAM_CA15L);

			/* udelay(PMIC_SETTLE_TIME(cur_vproc_mv, cur_vproc_mv + MAX_DIFF_VSRAM_VPROC)); // TODO: always fix max gap <- refine it??? */
		} while (cur_vproc_mv > mv);
	}

	if (NULL != g_pCpuVoltSampler) {
		g_pCpuVoltSampler(MT_CPU_DVFS_BIG, mv);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}

/* cpufreq set (freq & volt) */

static unsigned int _search_available_volt(struct mt_cpu_dvfs *p, unsigned int target_khz)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* search available voltage */
	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (target_khz <= cpu_dvfs_get_freq_by_idx(p, i))
			break;
	}

	BUG_ON(i < 0);		/* i.e. target_khz > p->opp_tbl[0].cpufreq_khz */

	FUNC_EXIT(FUNC_LV_HELP);

	return cpu_dvfs_get_volt_by_idx(p, i);
}

static int _cpufreq_set(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz)
{
	unsigned int mv;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	mv = _search_available_volt(p, target_khz);

	/* set volt (UP) */
	if (cur_khz < target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret)	/* set volt fail */
			goto fail;
	}

	/* set freq (UP/DOWN) */
	if (cur_khz != target_khz)
		p->ops->set_cur_freq(p, cur_khz, target_khz);

	/* set volt (DOWN) */
	if (cur_khz > target_khz) {
		ret = p->ops->set_cur_volt(p, mv);

		if (ret)	/* set volt fail */
			goto fail;
	}

	FUNC_EXIT(FUNC_LV_HELP);
 fail:
	return ret;
}

static void _mt_cpufreq_set(enum mt_cpu_dvfs_id id, int new_opp_idx)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int cur_freq;
	unsigned int target_freq;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);
	BUG_ON(new_opp_idx >= p->nr_opp_tbl);

	cur_freq = p->ops->get_cur_phy_freq(p);
	target_freq = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);

	_cpufreq_set(p, cur_freq, target_freq);

	p->idx_opp_tbl = new_opp_idx;

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static void _set_no_limited(struct mt_cpu_dvfs *p)
{
	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	p->limited_max_freq = cpu_dvfs_get_max_freq(p);
	p->limited_max_ncpu = MAX_CPU_NUM;

	FUNC_EXIT(FUNC_LV_HELP);
}

static void _downgrade_freq_check(enum mt_cpu_dvfs_id id)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int temp = 0;

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	/* if not CPU_LEVEL0 */
	if (cpu_dvfs_get_max_freq(p) < ((MT_CPU_DVFS_LITTLE == id) ? DVFS_LITTLE_F0 : DVFS_BIG_F0))
		goto out;

	/* get temp */
#if 0				/* TODO: FIXME */

	if (mt_ptp_status((MT_CPU_DVFS_LITTLE == id) ? PTP_DET_LITTLE : PTP_DET_BIG) == 1)
		temp = (((DRV_Reg32(PTP_TEMP) & 0xff)) + 25) * 1000;	/* TODO: mt_ptp.c provide mt_ptp_get_temp() */
	else
		temp = mtktscpu_get_Tj_temp();	/* TODO: FIXME, what is the difference for big & LITTLE */

#else
	temp = tscpu_get_bL_temp((MT_CPU_DVFS_LITTLE == id) ? THERMAL_BANK0 : THERMAL_BANK1);	/* TODO: mt_ptp.c provide mt_ptp_get_temp() */
#endif

	if (temp < 0 || 125000 < temp) {
		cpufreq_dbg("%d (temp) < 0 || 12500 < %d (temp)\n", temp, temp);
		goto out;
	}

	if (temp <= p->ptpod_temperature_limit_1) {
		p->downgrade_freq_for_ptpod = false;
		cpufreq_dbg("%d (temp) < %d (limit_1)\n", temp, p->ptpod_temperature_limit_1);
		goto out;
	} else if ((temp > p->ptpod_temperature_limit_1) && (temp < p->ptpod_temperature_limit_2)) {
		p->downgrade_freq_counter_return_limit =
		    p->downgrade_freq_counter_limit * p->ptpod_temperature_time_1;
		cpufreq_dbg("%d (temp) > %d (limit_1)\n", temp, p->ptpod_temperature_limit_1);
	} else {
		p->downgrade_freq_counter_return_limit =
		    p->downgrade_freq_counter_limit * p->ptpod_temperature_time_2;
		cpufreq_dbg("%d (temp) > %d (limit_2)\n", temp, p->ptpod_temperature_limit_2);
	}

	if (p->downgrade_freq_for_ptpod == false) {
		if ((num_online_cpus() == p->pre_online_cpu)
		    && (cpu_dvfs_get_cur_freq(p) == p->pre_freq)) {
			if ((num_online_cpus() >= p->over_max_cpu) && (p->idx_opp_tbl == 0)) {
				p->downgrade_freq_counter++;
				cpufreq_dbg("downgrade_freq_counter_limit = %d\n",
					    p->downgrade_freq_counter_limit);
				cpufreq_dbg("downgrade_freq_counter = %d\n",
					    p->downgrade_freq_counter);

				if (p->downgrade_freq_counter >= p->downgrade_freq_counter_limit) {
					p->downgrade_freq = cpu_dvfs_get_freq_by_idx(p, 1);

					p->downgrade_freq_for_ptpod = true;
					p->downgrade_freq_counter = 0;

					cpufreq_dbg("freq limit, downgrade_freq_for_ptpod = %d\n",
						    p->downgrade_freq_for_ptpod);

					policy = cpufreq_cpu_get(p->cpu_id);

					if (!policy)
						goto out;

					cpufreq_driver_target(policy, p->downgrade_freq,
							      CPUFREQ_RELATION_L);

					cpufreq_cpu_put(policy);
				}
			} else
				p->downgrade_freq_counter = 0;
		} else {
			p->pre_online_cpu = num_online_cpus();
			p->pre_freq = cpu_dvfs_get_cur_freq(p);

			p->downgrade_freq_counter = 0;
		}
	} else {
		p->downgrade_freq_counter_return++;

		cpufreq_dbg("downgrade_freq_counter_return_limit = %d\n",
			    p->downgrade_freq_counter_return_limit);
		cpufreq_dbg("downgrade_freq_counter_return = %d\n",
			    p->downgrade_freq_counter_return);

		if (p->downgrade_freq_counter_return >= p->downgrade_freq_counter_return_limit) {
			p->downgrade_freq_for_ptpod = false;
			p->downgrade_freq_counter_return = 0;

			cpufreq_dbg("Release freq limit, downgrade_freq_for_ptpod = %d\n",
				    p->downgrade_freq_for_ptpod);
		}
	}

 out:
	FUNC_EXIT(FUNC_LV_API);
}

static void _init_downgrade(struct mt_cpu_dvfs *p, unsigned int cpu_level)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (cpu_level) {
	case CPU_LEVEL_0:
	case CPU_LEVEL_1:
	case CPU_LEVEL_2:
	default:
		p->downgrade_freq_counter_limit = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 10 : 10;
		p->ptpod_temperature_time_1 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 2 : 1;
		p->ptpod_temperature_time_2 = cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE) ? 8 : 4;
		break;
	}

	/* install callback */
	cpufreq_freq_check = _downgrade_freq_check;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int _sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	int ret = -1;
	unsigned int freq;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->opp_tbl);
	BUG_ON(NULL == p->ops);

	freq = p->ops->get_cur_phy_freq(p);

	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (freq <= cpu_dvfs_get_freq_by_idx(p, i)) {
			p->idx_opp_tbl = i;
			break;
		}

	}

	if (i >= 0) {
		cpufreq_dbg("%s freq = %d\n", cpu_dvfs_get_name(p), cpu_dvfs_get_cur_freq(p));

		/* TODO: apply correct voltage??? */

		ret = 0;
	} else
		cpufreq_warn("%s can't find freq = %d\n", cpu_dvfs_get_name(p), freq);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

static void _mt_cpufreq_sync_opp_tbl_idx(void)
{
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_cpu_dvfs(i, p) {
		if (cpu_dvfs_is_availiable(p))
			_sync_opp_tbl_idx(p);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		if (p->cpu_id == cpu_id)
			break;
	}

	BUG_ON(i >= NR_MT_CPU_DVFS);

	return i;
}

int mt_cpufreq_state_set(int enabled)
{				/* TODO: state set by id??? */
	bool set_normal_max_opp = false;
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_API);

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_availiable(p))
			continue;

		cpufreq_lock(flags);

		if (enabled) {
			/* enable CPU DVFS */
			if (p->cpufreq_pause) {
				p->dvfs_disable_count--;
				cpufreq_dbg("enable %s DVFS: dvfs_disable_count = %d\n", p->name,
					    p->dvfs_disable_count);

				if (p->dvfs_disable_count <= 0)
					p->cpufreq_pause = false;
				else
					cpufreq_dbg
					    ("someone still disable %s DVFS and cant't enable it\n",
					     p->name);
			} else
				cpufreq_dbg("%s DVFS already enabled\n", p->name);
		} else {
			/* disable DVFS */
			p->dvfs_disable_count++;

			if (p->cpufreq_pause)
				cpufreq_dbg("%s DVFS already disabled\n", p->name);
			else {
				p->cpufreq_pause = true;
				set_normal_max_opp = true;
			}
		}

		cpufreq_unlock(flags);

		if (set_normal_max_opp) {
			struct cpufreq_policy *policy = cpufreq_cpu_get(p->cpu_id);

			if (policy)
				cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
						      CPUFREQ_RELATION_L);
			else {
				cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n",
					     p->name);
				ret = -1;
			}

			cpufreq_cpu_put(policy);
		}

		set_normal_max_opp = false;
	}

	FUNC_EXIT(FUNC_LV_API);

	return ret;
}
EXPORT_SYMBOL(mt_cpufreq_state_set);

/* Power Table */
#if 0
#define P_MCU_L         (1243)	/* MCU Leakage Power          */
#define P_MCU_T         (2900)	/* MCU Total Power            */
#define P_CA7_L         (110)	/* CA7 Leakage Power          */
#define P_CA7_T         (305)	/* Single CA7 Core Power      */

#define P_MCL99_105C_L  (1243)	/* MCL99 Leakage Power @ 105C */
#define P_MCL99_25C_L   (93)	/* MCL99 Leakage Power @ 25C  */
#define P_MCL50_105C_L  (587)	/* MCL50 Leakage Power @ 105C */
#define P_MCL50_25C_L   (35)	/* MCL50 Leakage Power @ 25C  */

#define T_105           (105)	/* Temperature 105C           */
#define T_65            (65)	/* Temperature 65C            */
#define T_25            (25)	/* Temperature 25C            */

#define P_MCU_D ((P_MCU_T - P_MCU_L) - 8 * (P_CA7_T - P_CA7_L))	/* MCU dynamic power except of CA7 cores */

#define P_TOTAL_CORE_L ((P_MCL99_105C_L  * 27049) / 100000)	/* Total leakage at T_65 */
#define P_EACH_CORE_L  ((P_TOTAL_CORE_L * ((P_CA7_L * 1000) / P_MCU_L)) / 1000)	/* 1 core leakage at T_65 */

#define P_CA7_D_1_CORE ((P_CA7_T - P_CA7_L) * 1)	/* CA7 dynamic power for 1 cores turned on */
#define P_CA7_D_2_CORE ((P_CA7_T - P_CA7_L) * 2)	/* CA7 dynamic power for 2 cores turned on */
#define P_CA7_D_3_CORE ((P_CA7_T - P_CA7_L) * 3)	/* CA7 dynamic power for 3 cores turned on */
#define P_CA7_D_4_CORE ((P_CA7_T - P_CA7_L) * 4)	/* CA7 dynamic power for 4 cores turned on */

#define A_1_CORE (P_MCU_D + P_CA7_D_1_CORE)	/* MCU dynamic power for 1 cores turned on */
#define A_2_CORE (P_MCU_D + P_CA7_D_2_CORE)	/* MCU dynamic power for 2 cores turned on */
#define A_3_CORE (P_MCU_D + P_CA7_D_3_CORE)	/* MCU dynamic power for 3 cores turned on */
#define A_4_CORE (P_MCU_D + P_CA7_D_4_CORE)	/* MCU dynamic power for 4 cores turned on */

static void _power_calculation(struct mt_cpu_dvfs *p, int idx, int ncpu)
{
	int multi = 0, p_dynamic = 0, p_leakage = 0, freq_ratio = 0, volt_square_ratio = 0;
	int possible_cpu = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	volt_square_ratio = (((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
			     ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000)) / 100;
	freq_ratio = (p->opp_tbl[idx].cpufreq_khz / 1700);

	cpufreq_dbg("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio, volt_square_ratio);

	multi = ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
	    ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000) *
	    ((p->opp_tbl[idx].cpufreq_volt * 100) / 1000);

	switch (ncpu) {
	case 0:
		/* 1 core */
		p_dynamic = (((A_1_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 7 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 1:
		/* 2 core */
		p_dynamic = (((A_2_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 6 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 2:
		/* 3 core */
		p_dynamic = (((A_3_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 5 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	case 3:
		/* 4 core */
		p_dynamic = (((A_4_CORE * freq_ratio) / 1000) * volt_square_ratio) / 100;
		p_leakage = ((P_TOTAL_CORE_L - 4 * P_EACH_CORE_L) * (multi)) / (100 * 100 * 100);
		cpufreq_dbg("p_dynamic = %d, p_leakage = %d\n", p_dynamic, p_leakage);
		break;

	default:
		break;
	}

	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu = ncpu + 1;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz = p->opp_tbl[idx].cpufreq_khz;
	p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power = p_dynamic + p_leakage;

	cpufreq_dbg("p->power_tbl[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		    (idx * possible_cpu + ncpu),
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_ncpu,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_khz,
		    p->power_tbl[idx * possible_cpu + ncpu].cpufreq_power);

	FUNC_EXIT(FUNC_LV_HELP);
}

static int setup_power_table(struct mt_cpu_dvfs *p)
{
	static const unsigned int pwr_tbl_cgf[] = { 0, 0, 1, 0, 1, 0, 1, 0, };
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	unsigned int pwr_eff_num;
	int possible_cpu;
	int i, j;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	if (p->power_tbl)
		goto out;

	cpufreq_dbg("P_MCU_D = %d\n", P_MCU_D);
	cpufreq_dbg
	    ("P_CA7_D_1_CORE = %d, P_CA7_D_2_CORE = %d, P_CA7_D_3_CORE = %d, P_CA7_D_4_CORE = %d\n",
	     P_CA7_D_1_CORE, P_CA7_D_2_CORE, P_CA7_D_3_CORE, P_CA7_D_4_CORE);
	cpufreq_dbg("P_TOTAL_CORE_L = %d, P_EACH_CORE_L = %d\n", P_TOTAL_CORE_L, P_EACH_CORE_L);
	cpufreq_dbg("A_1_CORE = %d, A_2_CORE = %d, A_3_CORE = %d, A_4_CORE = %d\n", A_1_CORE,
		    A_2_CORE, A_3_CORE, A_4_CORE);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl =
	    kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	if (NULL == p->power_tbl) {
		ret = -ENOMEM;
		goto out;
	}

	/* setup power efficiency array */
	for (i = 0, pwr_eff_num = 0; i < possible_cpu; i++) {
		if (1 == pwr_tbl_cgf[i])
			pwr_eff_num++;
	}

	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (1 == pwr_tbl_cgf[j])
				pwr_eff_tbl[i][j] = 1;
		}
	}

	p->nr_power_tbl = p->nr_opp_tbl * (possible_cpu - pwr_eff_num);

	/* calc power and fill in power table */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (0 == pwr_eff_tbl[i][j])
				_power_calculation(p, i, j);
		}
	}

	/* sort power table */
	for (i = p->nr_opp_tbl * possible_cpu; i > 0; i--) {
		for (j = 1; j <= i; j++) {
			if (p->power_tbl[j - 1].cpufreq_power < p->power_tbl[j].cpufreq_power) {
				struct mt_cpu_power_info tmp;

				tmp.cpufreq_khz = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg("[%d] = { .khz = %d, .ncup = %d, .power = %d }\n",
			    p->power_tbl[i].cpufreq_khz,
			    p->power_tbl[i].cpufreq_ncpu, p->power_tbl[i].cpufreq_power);
	}

 out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#else
static void _power_calculation(struct mt_cpu_dvfs *p, int oppidx, int ncpu)
{
#define CA7_REF_POWER	715	/* mW  */
#define CA7_REF_FREQ	1696000	/* KHz */
#define CA7_REF_VOLT	1000	/* mV  */
#define CA15L_REF_POWER	3910	/* mW  */
#define CA15L_REF_FREQ	2093000	/* KHz */
#define CA15L_REF_VOLT	1020	/* mV  */

	int p_dynamic = 0, ref_freq, ref_volt;
	int possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	FUNC_ENTER(FUNC_LV_HELP);

	p_dynamic = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_POWER : CA15L_REF_POWER;
	ref_freq = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_FREQ : CA15L_REF_FREQ;
	ref_volt = (cpu_dvfs_is(p, MT_CPU_DVFS_LITTLE)) ? CA7_REF_VOLT : CA15L_REF_VOLT;

	p_dynamic = p_dynamic *
	    (p->opp_tbl[oppidx].cpufreq_khz / 1000) / (ref_freq / 1000) *
	    p->opp_tbl[oppidx].cpufreq_volt / ref_volt * p->opp_tbl[oppidx].cpufreq_volt / ref_volt;

	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_ncpu = ncpu + 1;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_khz =
	    p->opp_tbl[oppidx].cpufreq_khz;
	p->power_tbl[NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx].cpufreq_power =
	    p_dynamic * (ncpu + 1) / possible_cpu;

	FUNC_EXIT(FUNC_LV_HELP);
}

static int setup_power_table(struct mt_cpu_dvfs *p)
{
	static const unsigned int pwr_tbl_cgf[NR_MAX_CPU] = { 0, 0, 0, 0, };
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	unsigned int pwr_eff_num;
	int possible_cpu = num_possible_cpus();	/* TODO: FIXME */
	int i, j;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	if (p->power_tbl)
		goto out;

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl =
	    kzalloc(p->nr_opp_tbl * possible_cpu * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	if (NULL == p->power_tbl) {
		ret = -ENOMEM;
		goto out;
	}

	/* setup power efficiency array */
	for (i = 0, pwr_eff_num = 0; i < possible_cpu; i++) {
		if (1 == pwr_tbl_cgf[i])
			pwr_eff_num++;
	}

	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (1 == pwr_tbl_cgf[j])
				pwr_eff_tbl[i][j] = 1;
		}
	}

	p->nr_power_tbl = p->nr_opp_tbl * (possible_cpu - pwr_eff_num);

	/* calc power and fill in power table */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (0 == pwr_eff_tbl[i][j])
				_power_calculation(p, i, j);
		}
	}

	/* sort power table */
	for (i = p->nr_opp_tbl * possible_cpu; i > 0; i--) {
		for (j = 1; j <= i; j++) {
			if (p->power_tbl[j - 1].cpufreq_power < p->power_tbl[j].cpufreq_power) {
				struct mt_cpu_power_info tmp;

				tmp.cpufreq_khz = p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu = p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power = p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz = p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu = p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power = p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz = tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu = tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power = tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		cpufreq_dbg
		    ("[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d }\n", i,
		     p->power_tbl[i].cpufreq_khz, p->power_tbl[i].cpufreq_ncpu,
		     p->power_tbl[i].cpufreq_power);
	}

#if 0				/* def CONFIG_THERMAL // TODO: FIXME */
	mtk_cpufreq_register(p->power_tbl, p->nr_power_tbl);
#endif

 out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}
#endif

static int _mt_cpufreq_setup_freqs_table(struct cpufreq_policy *policy,
					 struct mt_cpu_freq_info *freqs, int num)
{
	struct mt_cpu_dvfs *p;
	struct cpufreq_frequency_table *table;
	int i, ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == policy);
	BUG_ON(NULL == freqs);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	if (NULL == p->freq_tbl_for_cpufreq) {
		table = kzalloc((num + 1) * sizeof(*table), GFP_KERNEL);

		if (NULL == table) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < num; i++) {
			table[i].index = i;
			table[i].frequency = freqs[i].cpufreq_khz;
		}

		table[num].index = i;	/* TODO: FIXME, why need this??? */
		table[num].frequency = CPUFREQ_TABLE_END;

		p->opp_tbl = freqs;
		p->nr_opp_tbl = num;
		p->freq_tbl_for_cpufreq = table;
	}

	ret = cpufreq_frequency_table_cpuinfo(policy, p->freq_tbl_for_cpufreq);

	if (!ret)
		cpufreq_frequency_table_get_attr(p->freq_tbl_for_cpufreq, policy->cpu);

	if (NULL == p->power_tbl)
		p->ops->setup_power_table(p);

 out:
	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	p->dvfs_disable_by_ptpod = false;

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_enable_by_ptpod);

unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	p->dvfs_disable_by_ptpod = true;

#if 0				/* XXX: BUG_ON(irqs_disabled()) @ __cpufreq_notify_transition() */
	{
		struct cpufreq_policy *policy;
		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
					      CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		} else
			cpufreq_warn("can't get cpufreq policy to disable %s DVFS\n", p->name);
	}
#else
	_mt_cpufreq_set(id, p->idx_normal_max_opp);
#endif

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_cur_freq(p);
}
EXPORT_SYMBOL(mt_cpufreq_disable_by_ptpod);

void mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int possible_cpu;
	int ncpu;
	int found = 0;
	unsigned long flag;
	int i;

	FUNC_ENTER(FUNC_LV_API);

	cpufreq_dbg("%s(): limited_power = %d\n", __func__, limited_power);

	policy = cpufreq_cpu_get(0);	/* TODO: FIXME, not always 0 (it is OK for E1 BUT) /* <- policy get */ */ */

	if (NULL == policy)
		goto no_policy;

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	BUG_ON(NULL == p);

	cpufreq_lock(flag);	/* <- lock */

	p->thermal_protect_limited_power = limited_power;
	possible_cpu = num_possible_cpus();	/* TODO: FIXME */

	/* no limited */
	if (0 == limited_power) {
		p->limited_max_ncpu = possible_cpu;
		p->limited_max_freq = cpu_dvfs_get_max_freq(p);
		/* limited */
	} else {
		for (ncpu = possible_cpu; ncpu > 0; ncpu--) {
			for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
				if (p->power_tbl[i].cpufreq_power < limited_power) {
					p->limited_max_ncpu = p->power_tbl[i].cpufreq_ncpu;
					p->limited_max_freq = p->power_tbl[i].cpufreq_khz;
					found = 1;
					ncpu = 0;	/* for break outer loop */
					break;
				}
			}
		}

		/* not found and use lowest power limit */
		if (!found) {
			p->limited_max_ncpu = p->power_tbl[p->nr_power_tbl - 1].cpufreq_ncpu;
			p->limited_max_freq = p->power_tbl[p->nr_power_tbl - 1].cpufreq_khz;
		}
	}

	cpufreq_dbg("found = %d, limited_max_freq = %d, limited_max_ncpu = %d\n", found,
		    p->limited_max_freq, p->limited_max_ncpu);

	cpufreq_unlock(flag);	/* <- unlock */

	cpufreq_driver_target(policy, p->limited_max_freq, CPUFREQ_RELATION_L);
#if defined(CONFIG_CPU_FREQ_GOV_HOTPLUG)	/* TODO: FIXME */
	hp_limited_cpu_num(p->limited_max_ncpu);	/* notify hotplug governor */
#endif

	cpufreq_cpu_put(policy);	/* <- policy put */

 no_policy:
	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_cpufreq_thermal_protect);

/* for ramp down */
static int _keep_max_freq(struct mt_cpu_dvfs *p, unsigned int freq_old, unsigned int freq_new)
{				/* TODO: inline @ mt_cpufreq_target() */
	int ret;

	FUNC_ENTER(FUNC_LV_HELP);

	if (p->cpufreq_pause)
		ret = 1;
	else {
		/* check if system is going to ramp down */
		if (freq_new < freq_old)
			p->ramp_down_count++;
		else
			p->ramp_down_count = 0;

		ret = (p->ramp_down_count < RAMP_DOWN_TIMES) ? 1 : 0;
	}

	FUNC_ENTER(FUNC_LV_HELP);

	return ret;
}

static int _search_available_freq_idx(struct mt_cpu_dvfs *p, unsigned int target_khz,
				      unsigned int relation)
{				/* return -1 (not found) */
	int new_opp_idx = -1;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	if (CPUFREQ_RELATION_L == relation) {
		for (i = (signed)(p->nr_opp_tbl - 1); i >= 0; i--) {
			if (cpu_dvfs_get_freq_by_idx(p, i) >= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	} else {		/* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed)p->nr_opp_tbl; i++) {
			if (cpu_dvfs_get_freq_by_idx(p, i) <= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static int _thermal_limited_verify(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	unsigned int target_khz = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);
	int possible_cpu = 0;
	unsigned int online_cpu = 0;
	int found = 0;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	possible_cpu = num_possible_cpus();	/* TODO: FIXME */
	online_cpu = num_online_cpus();	/* TODO: FIXME */

	cpufreq_dbg("%s(): begin, idx = %d, online_cpu = %d\n", __func__, new_opp_idx, online_cpu);

	/* no limited */
	if (0 == p->thermal_protect_limited_power)
		return new_opp_idx;

	for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {
		if (p->power_tbl[i].cpufreq_ncpu == p->limited_max_ncpu
		    && p->power_tbl[i].cpufreq_khz == p->limited_max_freq)
			break;
	}

	cpufreq_dbg("%s(): idx = %d, limited_max_ncpu = %d, limited_max_freq = %d\n", __func__, i,
		    p->limited_max_ncpu, p->limited_max_freq);

	for (; i < p->nr_opp_tbl * possible_cpu; i++) {
		if (p->power_tbl[i].cpufreq_ncpu == online_cpu) {
			if (target_khz >= p->power_tbl[i].cpufreq_khz) {
				found = 1;
				break;
			}
		}
	}

	if (found) {
		target_khz = p->power_tbl[i].cpufreq_khz;
		cpufreq_dbg("%s(): freq found, idx = %d, target_khz = %d, online_cpu = %d\n",
			    __func__, i, target_khz, online_cpu);
	} else {
		target_khz = p->limited_max_freq;
		cpufreq_dbg("%s(): freq not found, set to limited_max_freq = %d\n", __func__,
			    target_khz);
	}

	i = _search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_H);	/* TODO: refine this function for idx searching */

	FUNC_EXIT(FUNC_LV_HELP);

	return i;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	int idx;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* for ramp down */
	if (_keep_max_freq(p, cpu_dvfs_get_cur_freq(p), cpu_dvfs_get_freq_by_idx(p, new_opp_idx)))
		new_opp_idx = 0;

	/* HEVC */
	if (p->limited_freq_by_hevc) {
		idx = _search_available_freq_idx(p, p->limited_freq_by_hevc, CPUFREQ_RELATION_L);

		if (idx != -1) {
			new_opp_idx = idx;
			cpufreq_dbg("%s(): hevc limited freq, idx = %d\n", __func__, new_opp_idx);
		}
	}
#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)

	if (true == p->downgrade_freq_for_ptpod) {
		if (cpu_dvfs_get_freq_by_idx(p, new_opp_idx) > p->downgrade_freq) {
			idx = _search_available_freq_idx(p, p->downgrade_freq, CPUFREQ_RELATION_H);

			if (idx != -1) {
				new_opp_idx = idx;
				cpufreq_dbg("%s(): downgrade freq, idx = %d\n", __func__,
					    new_opp_idx);
			}
		}
	}
#endif				/* CONFIG_CPU_DVFS_DOWNGRADE_FREQ */

	/* search thermal limited freq */
	idx = _thermal_limited_verify(p, new_opp_idx);

	if (idx != -1) {
		new_opp_idx = idx;
		cpufreq_dbg("%s(): thermal limited freq, idx = %d\n", __func__, new_opp_idx);
	}

	/* for ptpod init */
	if (p->dvfs_disable_by_ptpod) {
		new_opp_idx = p->idx_normal_max_opp;
		cpufreq_dbg("%s(): for ptpod init, idx = %d\n", __func__, new_opp_idx);
	}

	/* for early suspend */
	if (p->limit_max_freq_early_suspend) {
		new_opp_idx = p->idx_normal_max_opp;
		cpufreq_dbg("%s(): for early suspend, idx = %d\n", __func__, new_opp_idx);
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

/*
 * cpufreq driver
 */
static int _mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct mt_cpu_dvfs *p;
	int ret = 0;		/* cpufreq_frequency_table_verify() always return 0 */

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	BUG_ON(NULL == p);

	ret = cpufreq_frequency_table_verify(policy, p->freq_tbl_for_cpufreq);

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq,
			      unsigned int relation)
{
	unsigned int cpu;
	struct cpufreq_freqs freqs;
	unsigned int new_opp_idx;

	enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);

	unsigned long flags;
	int ret = 0;		/* -EINVAL; */

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus()	/* TODO: FIXME */
 || cpufreq_frequency_table_target(policy, id_to_cpu_dvfs(id)->freq_tbl_for_cpufreq,
					     target_freq, relation, &new_opp_idx)
	    || (id_to_cpu_dvfs(id) && id_to_cpu_dvfs(id)->is_fixed_freq)
	    )
		return -EINVAL;

#if defined(CONFIG_CPU_DVFS_BRINGUP)
	new_opp_idx = id_to_cpu_dvfs(id)->idx_normal_max_opp;
#elif defined(CONFIG_CPU_DVFS_RANDOM_TEST)
	new_opp_idx = jiffies & 0x7;	/* 0~7 */
#else
	new_opp_idx = _calc_new_opp_idx(id_to_cpu_dvfs(id), new_opp_idx);
#endif

	freqs.old = policy->cur;
	freqs.new = mt_cpufreq_max_frequency_by_DVS(id, new_opp_idx);
	freqs.cpu = policy->cpu;

	for_each_online_cpu(cpu) {	/* TODO: big LITTLE issue (id mapping) */
		freqs.cpu = cpu;
		cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
	}

	cpufreq_lock(flags);

	_mt_cpufreq_set(id, new_opp_idx);

	cpufreq_unlock(flags);

	for_each_online_cpu(cpu) {	/* TODO: big LITTLE issue (id mapping) */
		freqs.cpu = cpu;
		cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret = -EINVAL;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (policy->cpu >= num_possible_cpus())	/* TODO: FIXME */
		return -EINVAL;

	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);

	/*******************************************************
	* 1 us, assumed, will be overwrited by min_sampling_rate
	********************************************************/
	policy->cpuinfo.transition_latency = 1000;

	/*********************************************
	* set default policy and cpuinfo, unit : Khz
	**********************************************/
	{
		enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
		struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
		int lv = read_efuse_cpu_speed();	/* i.e. g_cpufreq_get_ptp_level */
		struct opp_tbl_info *opp_tbl_info =
		    (MT_CPU_DVFS_BIG ==
		     id) ? &opp_tbls_big[CPU_LV_TO_OPP_IDX(lv)] :
		    &opp_tbls_little[CPU_LV_TO_OPP_IDX(lv)];

		BUG_ON(NULL == p);
		BUG_ON(!(lv == CPU_LEVEL_0 || lv == CPU_LEVEL_1 || lv == CPU_LEVEL_2));

		ret = _mt_cpufreq_setup_freqs_table(policy,
						    opp_tbl_info->opp_tbl, opp_tbl_info->size);

		policy->cpuinfo.max_freq = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->cpuinfo.min_freq = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		policy->cur = _mt_cpufreq_get_cur_phy_freq(id);	/* use cur phy freq is better */
		policy->max = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->min = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		if (_sync_opp_tbl_idx(p) >= 0)	/* sync p->idx_opp_tbl first before _restore_default_volt() */
			p->idx_normal_max_opp = p->idx_opp_tbl;

		/* restore default volt, sync opp idx, set default limit */
		_restore_default_volt(p);

		_set_no_limited(p);
#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)
		_init_downgrade(p, read_efuse_cpu_speed());
#endif
	}

	if (ret)
		cpufreq_err("failed to setup frequency table\n");

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static unsigned int _mt_cpufreq_get(unsigned int cpu)
{
	struct mt_cpu_dvfs *p;

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));

	BUG_ON(NULL == p);

	FUNC_EXIT(FUNC_LV_MODULE);

	return cpu_dvfs_get_cur_freq(p);
}

/*
 * Early suspend
 */
static void _mt_cpufreq_early_suspend(struct early_suspend *h)
{
	struct cpufreq_policy *policy;
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_cpufreq_state_set(0); // TODO: it is not necessary because of limit_max_freq_early_suspend */

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE); // TODO: move to deepidle driver */

	for_each_cpu_dvfs(i, p) {
		p->limit_max_freq_early_suspend = true;

		if (!cpu_dvfs_is_availiable(p))
			continue;

		policy = cpufreq_cpu_get(p->cpu_id);

		if (policy) {
			cpufreq_driver_target(policy, cpu_dvfs_get_normal_max_freq(p),
					      CPUFREQ_RELATION_L);
			cpufreq_cpu_put(policy);
		}
	}

	FUNC_EXIT(FUNC_LV_MODULE);
}

static void _mt_cpufreq_late_resume(struct early_suspend *h)
{
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	for_each_cpu_dvfs(i, p) {
		p->limit_max_freq_early_suspend = false;
	}

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); // TODO: move to deepidle driver */

	/* mt_cpufreq_state_set(1); // TODO: it is not necessary because of limit_max_freq_early_suspend */

	FUNC_EXIT(FUNC_LV_MODULE);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend _mt_cpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = _mt_cpufreq_early_suspend,
	.resume = _mt_cpufreq_late_resume,
};
#endif				/* CONFIG_HAS_EARLYSUSPEND */

static struct freq_attr *_mt_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver _mt_cpufreq_driver = {
	.verify = _mt_cpufreq_verify,
	.target = _mt_cpufreq_target,
	.init = _mt_cpufreq_init,
	.get = _mt_cpufreq_get,
	.name = "mt-cpufreq",
	.attr = _mt_cpufreq_attr,
};

/*
 * Platform driver
 */
static int _mt_cpufreq_suspend(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SUSPEND); // TODO: move to suspend driver */

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_resume(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL); // TODO: move to suspend driver */

	/* TODO: set big/LITTLE voltage??? */

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pm_restore_early(struct device *dev)	/* for IPO-H HW(freq) / SW(opp_tbl_idx) */* / TODO: DON'T CARE??? */ */
{
	FUNC_ENTER(FUNC_LV_MODULE);

	_mt_cpufreq_sync_opp_tbl_idx();

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* TODO: check extBuck init with James */

	/* register early suspend */
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&_mt_cpufreq_early_suspend_handler);
#endif

	/* init PMIC_WRAP & volt */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#if 0				/* TODO: FIXME */
	/* restore default volt, sync opp idx, set default limit */
	{
		struct mt_cpu_dvfs *p;
		int i;

		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_availiable(p))
				continue;

			_restore_default_volt(p);

			if (_sync_opp_tbl_idx(p) >= 0)
				p->idx_normal_max_opp = p->idx_opp_tbl;

			_set_no_limited(p);

#if defined(CONFIG_CPU_DVFS_DOWNGRADE_FREQ)
			_init_downgrade(p, read_efuse_cpu_speed());
#endif
		}
	}
#endif
	ret = cpufreq_register_driver(&_mt_cpufreq_driver);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	cpufreq_unregister_driver(&_mt_cpufreq_driver);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static const struct dev_pm_ops _mt_cpufreq_pm_ops = {
	.suspend = _mt_cpufreq_suspend,
	.resume = _mt_cpufreq_resume,
	.restore_early = _mt_cpufreq_pm_restore_early,
};

static struct platform_driver _mt_cpufreq_pdrv = {
	.probe = _mt_cpufreq_pdrv_probe,
	.remove = _mt_cpufreq_pdrv_remove,
	.driver = {
		   .name = "mt-cpufreq",
		   .pm = &_mt_cpufreq_pm_ops,
		   .owner = THIS_MODULE,
		   },
};

#ifndef __KERNEL__
/*
 * For CTP
 */
int mt_cpufreq_pdrv_probe(void)
{
	static struct cpufreq_policy policy_little;
	static struct cpufreq_policy policy_big;

	_mt_cpufreq_pdrv_probe(NULL);

	policy_little.cpu = cpu_dvfs[MT_CPU_DVFS_LITTLE].cpu_id;
	_mt_cpufreq_init(&policy_little);

	policy_big.cpu = cpu_dvfs[MT_CPU_DVFS_BIG].cpu_id;
	_mt_cpufreq_init(&policy_big);

	return 0;
}

int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx)
{
	static struct opp_tbl_info *info;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	switch (id) {
	case MT_CPU_DVFS_LITTLE:
		info = &opp_tbls_little[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)];
		break;

	case MT_CPU_DVFS_BIG:
	default:
		info = &opp_tbls_big[CPU_LV_TO_OPP_IDX(CPU_LEVEL_0)];
		break;
	}

	if (idx >= info->size)
		return -1;


	return _set_cur_volt(p, info->opp_tbl[idx].cpufreq_volt);
}

int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx)
{
	unsigned int cur_freq;
	unsigned int target_freq;
	int ret;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	cur_freq = p->ops->get_cur_phy_freq(p);
	target_freq = cpu_dvfs_get_freq_by_idx(p, idx);

	ret = _cpufreq_set(p, cur_freq, target_freq);

	if (ret < 0)
		return ret;

	return target_freq;
}

#include "dvfs.h"

/* MCUSYS Register */

/* APB Module ca15l_config */
#define CA15L_CONFIG_BASE (0x10200200)

#define IR_ROSC_CTL             (MCUCFG_BASE + 0x030)
#define CA15L_MON_SEL           (CA15L_CONFIG_BASE + 0x01C)
#define pminit_write(addr, val) mt65xx_reg_sync_writel((val), ((void *)addr))

static unsigned int _mt_get_bigcpu_freq(void)
{
	int output = 0;
	unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, clk26cali_1;
	unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl, ca15l_mon_sel;

	clk26cali_0 = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80);	/* enable fmeter_en */

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00);	/* select divider */

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8));	/* select abist_cksw */

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFF3) | (0x1 << 2));

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFC1F) | (0xb << 5));

	ca15l_mon_sel = DRV_Reg32(CA15L_MON_SEL);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel | 0x00000500);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x10000000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1);	/* start fmeter */

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n",
		       DRV_Reg32(CLK26CALI_0));
		/* mdelay(10); */
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4;	/* Khz */

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	/* print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output); */

	return output;
}

static unsigned int _mt_get_smallcpu_freq(void)
{
	int output = 0;
	unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, clk26cali_1;
	unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl;

	clk26cali_0 = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, clk26cali_0 | 0x80);	/* enable fmeter_en */

	clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
	pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00);	/* select divider */

	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	pminit_write(CLK_CFG_8, (46 << 8));	/* select armpll_occ_mon */

	top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
	pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFFC) | 0x1);

	top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
	pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFFE0) | 0xb);

	ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x08100000);

	temp = DRV_Reg32(CLK26CALI_0);
	pminit_write(CLK26CALI_0, temp | 0x1);	/* start fmeter */

	/* wait frequency meter finish */
	while (DRV_Reg32(CLK26CALI_0) & 0x1) {
		printf("wait for frequency meter finish, CLK26CALI = 0x%x\n",
		       DRV_Reg32(CLK26CALI_0));
		/* mdelay(10); */
	}

	temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000) / 1024) * 4;	/* Khz */

	pminit_write(CLK_CFG_8, clk_cfg_8);
	pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
	pminit_write(CLK26CALI_0, clk26cali_0);
	pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
	pminit_write(TOP_CKDIV1, top_ckdiv1);
	pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

	/* print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output); */

	return output;
}

unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id)
{
	return _mt_cpufreq_get_cur_phy_freq(id);
}

void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx;

	if (!p) {
		cpufreq_err("%s(%d, %d), id is wrong\n", __func__, id, freq);
		return;
	}

	idx = _search_available_freq_idx(p, freq, CPUFREQ_RELATION_H);

	if (-1 == idx) {
		cpufreq_err("%s(%d, %d), freq is wrong\n", __func__, id, freq);
		return;
	}

	mt_cpufreq_set_freq(id, idx);
}

unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id)
{
	return (MT_CPU_DVFS_LITTLE == id) ? _mt_get_smallcpu_freq() : _mt_get_bigcpu_freq();
}

void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int mv)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	cpufreq_dbg("%s(%d, %d)\n", __func__, id, mv);

	if (!p) {
		cpufreq_err("%s(%d, %d), id is wrong\n", __func__, id, mv);
		return;
	}

	if (_set_cur_volt(p, mv))
		cpufreq_err("%s(%d, %d), set volt fail\n", __func__, id, mv);
}

void dvfs_set_gpu_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VGPU, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VGPU);
}

void dvfs_set_vcore_ao_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_AO_NORMAL, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_DI_VCORE_AO_NORMAL);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}

void dvfs_set_vcore_pdn_volt(int pmic_val)
{
	cpufreq_dbg("%s(%d)\n", __func__, pmic_val);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_PDN_NORMAL, pmic_val);
	mt_cpufreq_apply_pmic_cmd(IDX_DI_VCORE_PDN_NORMAL);
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}

static unsigned int little_freq_backup;
static unsigned int big_freq_backup;
static unsigned int vgpu_backup;
static unsigned int vcore_ao_backup;
static unsigned int vcore_pdn_backup;

void dvfs_disable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__);	/* <-XXX */
	little_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_LITTLE);
	big_freq_backup = _mt_cpufreq_get_cur_phy_freq(MT_CPU_DVFS_BIG);
	pmic_read_interface(PMIC_ADDR_VGPU_VOSEL_ON, &vgpu_backup, 0x7F, 0);
	pmic_read_interface(PMIC_ADDR_VCORE_AO_VOSEL_ON, &vcore_ao_backup, 0x7F, 0);
	pmic_read_interface(PMIC_ADDR_VCORE_PDN_VOSEL_ON, &vcore_pdn_backup, 0x7F, 0);

	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, 1140000);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, 1140000);
	dvfs_set_gpu_volt(0x30);
	dvfs_set_vcore_ao_volt(0x38);
	dvfs_set_vcore_pdn_volt(0x30);
}

void dvfs_enable_by_ptpod(void)
{
	cpufreq_dbg("%s()\n", __func__);	/* <-XXX */
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_LITTLE, little_freq_backup);
	dvfs_set_cpu_freq_FH(MT_CPU_DVFS_BIG, big_freq_backup);
	dvfs_set_gpu_volt(vgpu_backup);
	dvfs_set_vcore_ao_volt(vcore_ao_backup);
	dvfs_set_vcore_pdn_volt(vcore_pdn_backup);
}
#endif				/* ! __KERNEL__ */

#ifdef CONFIG_PROC_FS
/*
 * PROC
 */

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

/* cpufreq_debug */
static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpufreq debug (log level) = %d\n"
		   "cpufreq debug (ptp level) = %d\n", func_lv_mask, read_efuse_cpu_speed()
	    );

	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file, const char __user *buffer, size_t count,
					loff_t *pos)
{
	int dbg_lv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &dbg_lv) == 1)
		func_lv_mask = dbg_lv;
	else
		cpufreq_err("echo dbg_lv (dec) > /proc/cpufreq/cpufreq_debug\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_downgrade_freq_info */
static int cpufreq_downgrade_freq_info_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "downgrade_freq_counter_limit = %d\n"
			   "ptpod_temperature_limit_1 = %d\n"
			   "ptpod_temperature_limit_2 = %d\n"
			   "ptpod_temperature_time_1 = %d\n"
			   "ptpod_temperature_time_2 = %d\n"
			   "downgrade_freq_counter_return_limit 1 = %d\n"
			   "downgrade_freq_counter_return_limit 2 = %d\n"
			   "over_max_cpu = %d\n",
			   p->name, i,
			   p->downgrade_freq_counter_limit,
			   p->ptpod_temperature_limit_1,
			   p->ptpod_temperature_limit_2,
			   p->ptpod_temperature_time_1,
			   p->ptpod_temperature_time_2,
			   p->ptpod_temperature_limit_1 * p->ptpod_temperature_time_1,
			   p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2,
			   p->over_max_cpu);
	}

	return 0;
}

/* cpufreq_downgrade_freq_counter_limit */
static int cpufreq_downgrade_freq_counter_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "downgrade_freq_counter_limit = %d\n",
			   p->name, i, p->downgrade_freq_counter_limit);
	}

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_limit_proc_write(struct file *file,
							       const char __user *buffer,
							       size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int downgrade_freq_counter_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &downgrade_freq_counter_limit) == 2
	    && (p = id_to_cpu_dvfs(id)))
		p->downgrade_freq_counter_limit = downgrade_freq_counter_limit;
	else
		cpufreq_err
		    ("echo id (dec) downgrade_freq_counter_limit (dec) > /proc/cpufreq/cpufreq_downgrade_freq_counter_limit\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_downgrade_freq_counter_return_limit */
static int cpufreq_downgrade_freq_counter_return_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "downgrade_freq_counter_return_limit = %d\n",
			   p->name, i, p->downgrade_freq_counter_return_limit);
	}

	return 0;
}

static ssize_t cpufreq_downgrade_freq_counter_return_limit_proc_write(struct file *file,
								      const char __user *buffer,
								      size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int downgrade_freq_counter_return_limit;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &downgrade_freq_counter_return_limit) == 2
	    && (p = id_to_cpu_dvfs(id))) {
		p->downgrade_freq_counter_return_limit = downgrade_freq_counter_return_limit;	/* TODO: p->ptpod_temperature_limit_1 * p->ptpod_temperature_time_1 or p->ptpod_temperature_limit_2 * p->ptpod_temperature_time_2 */
	} else
		cpufreq_err
		    ("echo id (dec) downgrade_freq_counter_return_limit (dec) > /proc/cpufreq/cpufreq_downgrade_freq_counter_return_limit\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_fftt_test */
static int cpufreq_fftt_test_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cpufreq_fftt_test_proc_write(struct file *file, const char __user *buffer,
					    size_t count, loff_t *pos)
{
	return count;
}

/* cpufreq_limited_freq_by_hevc */
static int cpufreq_limited_freq_by_hevc_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "limited_freq_by_hevc = %d\n", p->name, i, p->limited_freq_by_hevc);
	}

	return 0;
}

static ssize_t cpufreq_limited_freq_by_hevc_proc_write(struct file *file,
						       const char __user *buffer, size_t count,
						       loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int limited_freq_by_hevc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &limited_freq_by_hevc) == 2 && (p = id_to_cpu_dvfs(id)))
		p->limited_freq_by_hevc = limited_freq_by_hevc;
	else
		cpufreq_err
		    ("echo id (dec) limited_freq_by_hevc (dec) > /proc/cpufreq/cpufreq_limited_freq_by_hevc\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_limited_power */
static int cpufreq_limited_power_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "limited_max_freq = %d\n"
			   "limited_max_ncpu = %d\n",
			   p->name, i, p->limited_max_freq, p->limited_max_ncpu);
	}

	return 0;
}

static ssize_t cpufreq_limited_power_proc_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *pos)
{
	int limited_power;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &limited_power) == 1)
		mt_cpufreq_thermal_protect(limited_power);	/* TODO: specify limited_power by id??? */
	else
		cpufreq_err("echo limited_power (dec) > /proc/cpufreq/cpufreq_limited_power\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_over_max_cpu */
static int cpufreq_over_max_cpu_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n" "over_max_cpu = %d\n", p->name, i, p->over_max_cpu);
	}

	return 0;
}

static ssize_t cpufreq_over_max_cpu_proc_write(struct file *file, const char __user *buffer,
					       size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int over_max_cpu;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &over_max_cpu) == 2 && (p = id_to_cpu_dvfs(id)))
		p->over_max_cpu = over_max_cpu;
	else
		cpufreq_err
		    ("echo id (dec) over_max_cpu (dec) > /proc/cpufreq/cpufreq_over_max_cpu\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_power_dump */
static int cpufreq_power_dump_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i, j;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n", p->name, i);

		for (j = 0; j < p->nr_power_tbl; j++) {
			seq_printf(m,
				   "[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d, },\n",
				   j, p->power_tbl[j].cpufreq_khz, p->power_tbl[j].cpufreq_ncpu,
				   p->power_tbl[j].cpufreq_power);
		}
	}

	return 0;
}

/* cpufreq_ptpod_freq_volt */
static int cpufreq_ptpod_freq_volt_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i, j;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n", p->name, i);

		for (j = 0; j < p->nr_opp_tbl; j++) {
			seq_printf(m,
				   "[%d] = { .cpufreq_khz = %d,\t.cpufreq_volt = %d,\t.cpufreq_volt_org = %d, },\n",
				   j, p->opp_tbl[j].cpufreq_khz, p->opp_tbl[j].cpufreq_volt,
				   p->opp_tbl[j].cpufreq_volt_org);
		}
	}

	return 0;
}

/* cpufreq_ptpod_temperature_limit */
static int cpufreq_ptpod_temperature_limit_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "ptpod_temperature_limit_1 = %d\n"
			   "ptpod_temperature_limit_2 = %d\n",
			   p->name, i, p->ptpod_temperature_limit_1, p->ptpod_temperature_limit_2);
	}

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_limit_proc_write(struct file *file,
							  const char __user *buffer, size_t count,
							  loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int ptpod_temperature_limit_1;
	int ptpod_temperature_limit_2;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d", &id, &ptpod_temperature_limit_1, &ptpod_temperature_limit_2) ==
	    3 && (p = id_to_cpu_dvfs(id))) {
		p->ptpod_temperature_limit_1 = ptpod_temperature_limit_1;
		p->ptpod_temperature_limit_2 = ptpod_temperature_limit_2;
	} else
		cpufreq_err
		    ("echo id (dec) ptpod_temperature_limit_1 (dec) ptpod_temperature_limit_2 (dec) > /proc/cpufreq/cpufreq_ptpod_temperature_limit\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_ptpod_temperature_time */
static int cpufreq_ptpod_temperature_time_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "ptpod_temperature_time_1 = %d\n"
			   "ptpod_temperature_time_2 = %d\n",
			   p->name, i, p->ptpod_temperature_time_1, p->ptpod_temperature_time_2);
	}

	return 0;
}

static ssize_t cpufreq_ptpod_temperature_time_proc_write(struct file *file,
							 const char __user *buffer, size_t count,
							 loff_t *pos)
{
	struct mt_cpu_dvfs *p;
	int id;
	int ptpod_temperature_time_1;
	int ptpod_temperature_time_2;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d", &id, &ptpod_temperature_time_1, &ptpod_temperature_time_2) == 3
	    && (p = id_to_cpu_dvfs(id))) {
		p->ptpod_temperature_time_1 = ptpod_temperature_time_1;
		p->ptpod_temperature_time_2 = ptpod_temperature_time_2;
	} else
		cpufreq_err
		    ("echo id (dec) ptpod_temperature_time_1 (dec) ptpod_temperature_time_2 (dec) > /proc/cpufreq/cpufreq_ptpod_temperature_time\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_ptpod_test */
static int cpufreq_ptpod_test_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t cpufreq_ptpod_test_proc_write(struct file *file, const char __user *buffer,
					     size_t count, loff_t *pos)
{
	return count;
}

/* cpufreq_state */
static int cpufreq_state_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n" "cpufreq_pause = %d\n", p->name, i, p->cpufreq_pause);
	}

	return 0;
}

static ssize_t cpufreq_state_proc_write(struct file *file, const char __user *buffer, size_t count,
					loff_t *pos)
{
	int enable;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &enable) == 1)
		mt_cpufreq_state_set(enable);
	else
		cpufreq_err("echo 1/0 > /proc/cpufreq/cpufreq_state\n");

	free_page((unsigned int)buf);
	return count;
}

/* cpufreq_oppidx */
static int cpufreq_oppidx_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p;
	int i, j;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "cpufreq_oppidx = %d\n", p->name, p->cpu_id, p->idx_opp_tbl);

		for (j = 0; j < p->nr_opp_tbl; j++) {
			seq_printf(m, "\tOP(%d, %d),\n",
				   cpu_dvfs_get_freq_by_idx(p, j), cpu_dvfs_get_volt_by_idx(p, j)
			    );
		}
	}

	return 0;
}

static ssize_t cpufreq_oppidx_proc_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);	/* TOOD: FIXME, MT_CPU_DVFS_LITTLE, MT_CPU_DVFS_BIG */
	int oppidx;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (sscanf(buf, "%d", &oppidx) == 1 && 0 <= oppidx && oppidx < p->nr_opp_tbl) {
		p->is_fixed_freq = true;
		_mt_cpufreq_set(0, oppidx);	/* TOOD: FIXME, MT_CPU_DVFS_LITTLE, MT_CPU_DVFS_BIG */
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo oppidx > /proc/cpufreq/cpufreq_oppidx (0 <= %d < %d)\n", oppidx,
			    p->nr_opp_tbl);
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_freq */
static int cpufreq_freq_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "cpufreq_freq = %d\n", p->name, p->cpu_id, p->ops->get_cur_phy_freq(p)
		    );
	}

	return 0;
}

static ssize_t cpufreq_freq_proc_write(struct file *file, const char __user *buffer, size_t count,
				       loff_t *pos)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);	/* TOOD: FIXME, MT_CPU_DVFS_LITTLE, MT_CPU_DVFS_BIG */
	unsigned int cur_freq;
	int freq;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);

	if (sscanf(buf, "%d", &freq) == 1) {
		p->is_fixed_freq = true;	/* TODO: FIXME */
		cur_freq = p->ops->get_cur_phy_freq(p);
		p->ops->set_cur_freq(p, cur_freq, freq);
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo khz > /proc/cpufreq/cpufreq_freq\n");
	}

	free_page((unsigned int)buf);

	return count;
}

/* cpufreq_volt */
static int cpufreq_volt_proc_show(struct seq_file *m, void *v)
{				/* <-XXX */
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		seq_printf(m, "[%s/%d]\n"
			   "cpufreq_volt = %d\n", p->name, p->cpu_id, p->ops->get_cur_volt(p)
		    );
	}

	return 0;
}

static ssize_t cpufreq_volt_proc_write(struct file *file, const char __user *buffer, size_t count,
				       loff_t *pos)
{				/* <-XXX */
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);	/* TOOD: FIXME, MT_CPU_DVFS_LITTLE, MT_CPU_DVFS_BIG */
	int mv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &mv) == 1) {
		p->is_fixed_freq = true;	/* TODO: FIXME */
		_set_cur_volt(p, mv);
	} else {
		p->is_fixed_freq = false;	/* TODO: FIXME */
		cpufreq_err("echo mv > /proc/cpufreq/cpufreq_volt\n");
	}

	free_page((unsigned int)buf);

	return count;
}

#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name ## _proc_show, NULL);		\
	}									\
	static const struct file_operations name ## _proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name ## _proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
		.write          = name ## _proc_write,				\
	}

#define PROC_FOPS_RO(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name ## _proc_show, NULL);		\
	}									\
	static const struct file_operations name ## _proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name ## _proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(cpufreq_debug);
PROC_FOPS_RO(cpufreq_downgrade_freq_info);
PROC_FOPS_RW(cpufreq_downgrade_freq_counter_limit);
PROC_FOPS_RW(cpufreq_downgrade_freq_counter_return_limit);
PROC_FOPS_RW(cpufreq_fftt_test);
PROC_FOPS_RW(cpufreq_limited_freq_by_hevc);
PROC_FOPS_RW(cpufreq_limited_power);
PROC_FOPS_RW(cpufreq_over_max_cpu);
PROC_FOPS_RO(cpufreq_power_dump);
PROC_FOPS_RO(cpufreq_ptpod_freq_volt);
PROC_FOPS_RW(cpufreq_ptpod_temperature_limit);
PROC_FOPS_RW(cpufreq_ptpod_temperature_time);
PROC_FOPS_RW(cpufreq_ptpod_test);
PROC_FOPS_RW(cpufreq_state);
PROC_FOPS_RW(cpufreq_oppidx);	/* <-XXX */
PROC_FOPS_RW(cpufreq_freq);	/* <-XXX */
PROC_FOPS_RW(cpufreq_volt);	/* <-XXX */

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	const struct {
		const char *name;
		const struct file_operations *fops;
	} entries[] = {
		PROC_ENTRY(cpufreq_debug), PROC_ENTRY(cpufreq_downgrade_freq_info), PROC_ENTRY(cpufreq_downgrade_freq_counter_limit), PROC_ENTRY(cpufreq_downgrade_freq_counter_return_limit), PROC_ENTRY(cpufreq_fftt_test), PROC_ENTRY(cpufreq_limited_freq_by_hevc), PROC_ENTRY(cpufreq_limited_power), PROC_ENTRY(cpufreq_over_max_cpu), PROC_ENTRY(cpufreq_power_dump), PROC_ENTRY(cpufreq_ptpod_freq_volt), PROC_ENTRY(cpufreq_ptpod_temperature_limit), PROC_ENTRY(cpufreq_ptpod_temperature_time), PROC_ENTRY(cpufreq_ptpod_test), PROC_ENTRY(cpufreq_state), PROC_ENTRY(cpufreq_oppidx),	/* <-XXX */
		    PROC_ENTRY(cpufreq_freq),	/* <-XXX */
		    PROC_ENTRY(cpufreq_volt),	/* <-XXX */
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		cpufreq_err("fail to create /proc/cpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}
#endif				/* CONFIG_PROC_FS */

/*
 * Module driver
 */
static int __init _mt_cpufreq_pdrv_init(void)
{
	int ret = 0;

	FUNC_ENTER(FUNC_LV_MODULE);

#ifdef CONFIG_PROC_FS

	/* init proc */
	if (_create_procfs())
		goto out;

#endif				/* CONFIG_PROC_FS */

	/* register platform driver */
	ret = platform_driver_register(&_mt_cpufreq_pdrv);

	if (ret)
		cpufreq_err("fail to register cpufreq driver @ %s()\n", __func__);

 out:
	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static void __exit _mt_cpufreq_pdrv_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	platform_driver_unregister(&_mt_cpufreq_pdrv);

	FUNC_EXIT(FUNC_LV_MODULE);
}

#if defined(CONFIG_CPU_DVFS_BRINGUP)
#else				/* CONFIG_CPU_DVFS_BRINGUP */
module_init(_mt_cpufreq_pdrv_init);
module_exit(_mt_cpufreq_pdrv_exit);
#endif				/* CONFIG_CPU_DVFS_BRINGUP */

MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3");
MODULE_LICENSE("GPL");
