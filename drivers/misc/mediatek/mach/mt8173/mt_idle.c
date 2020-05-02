#define _MT_IDLE_C

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mtk_gpu_utility.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>

#ifdef CONFIG_OF
#define __MT_REG_BASE	/* workaround for mt_reg_base.h build warnings */
#endif

#include <mach/mt_cirq.h>
#include <asm/system_misc.h>
#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_dcm.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpuxgpt.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_sleep.h>
#include <mach/hotplug.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_power_gs.h>
#include <mach/mt_ptp.h>
#include <mach/mt_timer.h>
#include <mach/irqs.h>
#include <mach/mt_thermal.h>
#include <mach/mt_boot.h>
#include <mach/mt_cpuxgpt.h>

#include "include/mach/mt_idle.h"

#define IDLE_HAVE_SODI		1
#define IDLE_HAVE_MCDI		1
#define IDLE_HAVE_CPUFREQ	1
#define IDLE_HAVE_CLKMGR	1
#define IDLE_HAVE_THERMAL	1
#define IDLE_HAVE_GPUDVFS	0
#define IDLE_HAVE_HOTPLUG	0
#define IDLE_HAVE_LOCALTIMER	0

#define CCI400_CLOCK_SWITCH	0
#define AUDINTBUS_CLOCK_SWITCH	1
#define DPIDLE_BUS_DCM		0

/* MCDI/SODI DVT Test */
#define MCDI_DVT_IPI 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_mcdi.c */
#define MCDI_DVT_CPUxGPT 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_mcdi.c */

#define SODI_DVT_APxGPT 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_sodi.c */
#define SODI_DVT_APxGPT_TimerCount_5S 0

#if MCDI_DVT_IPI || MCDI_DVT_CPUxGPT
#include <linux/delay.h>
#endif

#define IDLE_TAG	"[Power/idle] "

#define idle_err(fmt, args...)	pr_err(IDLE_TAG fmt, ##args)
#define idle_warn(fmt, args...)	pr_warn(IDLE_TAG fmt, ##args)
#define idle_info(fmt, args...)	pr_notice(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)	pr_info(IDLE_TAG fmt, ##args)
#define idle_ver(fmt, args...)	pr_debug(IDLE_TAG fmt, ##args)

#define idle_gpt GPT4

#define idle_readl(addr)	DRV_Reg32(addr)
#define idle_writel(addr, val)	mt65xx_reg_sync_writel(val, addr)
#define idle_setl(addr, val)	idle_writel(addr, idle_readl(addr) | (val))
#define idle_clrl(addr, val)	idle_writel(addr, idle_readl(addr) & ~(val))

static unsigned long rgidle_cnt[NR_CPUS] = {0};
static bool mt_idle_chk_golden;
static bool mt_dpidle_chk_golden;

#define USER_CMD_SIZE		32

static char cmd_buf[USER_CMD_SIZE];

enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO = 1,
	IDLE_TYPE_MC = 2,
	IDLE_TYPE_SL = 3,
	IDLE_TYPE_RG = 4,
	NR_TYPES = 5,
};

enum {
	BY_CPU = 0,
	BY_CLK = 1,
	BY_TMR = 2,
	BY_OTH = 3,
	BY_VTG = 4,
	NR_REASONS = 5
};

/*Idle handler on/off*/
static int idle_switch[NR_TYPES] = {
	1,	/* dpidle switch */
	1,	/* soidle switch */
	0,	/* mcidle switch */
	1,	/* slidle switch */
	1,	/* rgidle switch */
};

static u32 dpidle_condition_mask[NR_GRPS] = {
	0x6ff41ffc, /* PERI0 */
	0x00000006, /* PERI1 */
	0x00000080, /* INFRA */
	0xffff7fff, /* DISP0 */
	0x000003ff, /* DISP1 */
	0x00000be1, /* IMAGE */
	0x0000000f, /* MFG */
	0x00000000, /* AUDIO */
	0x00000001, /* VDEC0 */
	0x00000001, /* VDEC1 */
	0x00001111, /* VENC_JPEG */
	0x00000003, /* VENC_LT */
};

static u32 soidle_condition_mask[NR_GRPS] = {
	0x6ff40ffc, /* PERI0 */
	0x00000006, /* PERI1 */
	0x00000080, /* INFRA */
	0x757a7ffc, /* DISP0 */
	0x000003cc, /* DISP1 */
	0x00000be1, /* IMAGE */
	0x0000000f, /* MFG */
	0x00000000, /* AUDIO */
	0x00000001, /* VDEC0 */
	0x00000001, /* VDEC1 */
	0x00001111, /* VENC_JPEG */
	0x00000003, /* VENC_LT */
};

static u32 slidle_condition_mask[NR_GRPS] = {
	0x4f801000, /* PERI0 */
	0x00000004, /* PERI1 */
	0x00000000, /* INFRA */
	0x00000000, /* DISP0 */
	0x00000000, /* DISP1 */
	0x00000000, /* IMAGE */
	0x00000000, /* MFG */
	0x00000000, /* AUDIO */
	0x00000000, /* VDEC0 */
	0x00000000, /* VDEC1 */
	0x00000000, /* VENC_JPEG */
	0x00000000, /* VENC_LT */
};

static const char *idle_name[NR_TYPES] = {
	"dpidle",
	"soidle",
	"mcidle",
	"slidle",
	"rgidle",
};

static const char *reason_name[NR_REASONS] = {
	"by_cpu",
	"by_clk",
	"by_tmr",
	"by_oth",
	"by_vtg",
};

/* Slow Idle */
static u32		slidle_block_mask[NR_GRPS];
static unsigned long	slidle_cnt[NR_CPUS];
static unsigned long	slidle_block_cnt[NR_REASONS];

/* SODI */
static u32		soidle_block_mask[NR_GRPS];
static u32		soidle_timer_left;
static u32		soidle_timer_left2;
#ifndef CONFIG_SMP
static u32		soidle_timer_cmp;
#endif
static u32		soidle_time_critera = 26000; /* FIXME: need fine tune */
static unsigned long	soidle_cnt[NR_CPUS];
static unsigned long	soidle_block_cnt[NR_CPUS][NR_REASONS];

/* DeepIdle */
static u32		dpidle_block_mask[NR_GRPS];
static u32		dpidle_timer_left;
static u32		dpidle_timer_left2;
#ifndef CONFIG_SMP
static u32		dpidle_timer_cmp;
#endif
static u32		dpidle_time_critera = 26000;
static u64		dpidle_block_time_critera = 30000; /* default 30sec */
static unsigned long	dpidle_cnt[NR_CPUS];
static unsigned long	dpidle_block_cnt[NR_REASONS];
static u64		dpidle_block_prev_time;

/* MCDI */
static u32		mcidle_timer_left[NR_CPUS];
static u32		mcidle_timer_left2[NR_CPUS];
static u32		mcidle_time_critera = 39000; /* 3ms */
static unsigned long	mcidle_cnt[NR_CPUS];
static unsigned long	mcidle_block_cnt[NR_CPUS][NR_REASONS];
u64			mcidle_timer_before_wfi[NR_CPUS];
static unsigned int	idle_spm_lock;

static void __iomem *topckgen_base;
static void __iomem *infrasys_base;
static void __iomem *perisys_base;
static void __iomem *audiosys_base;
static void __iomem *mfgsys_base;
static void __iomem *mmsys_base;
static void __iomem *imgsys_base;
static void __iomem *vdecsys_base;
static void __iomem *vencsys_base;
static void __iomem *vencltsys_base;
static void __iomem *scpsys_base;

#define TOPCKGEN_REG(ofs)	(topckgen_base + ofs)
#define INFRA_REG(ofs)		(infrasys_base + ofs)
#define PREI_REG(ofs)		(perisys_base + ofs)
#define AUDIO_REG(ofs)		(audiosys_base + ofs)
#define MFG_REG(ofs)		(mfgsys_base + ofs)
#define MM_REG(ofs)		(mmsys_base + ofs)
#define IMG_REG(ofs)		(imgsys_base + ofs)
#define VDEC_REG(ofs)		(vdecsys_base + ofs)
#define VENC_REG(ofs)		(vencsys_base + ofs)
#define VENCLT_REG(ofs)		(vencltsys_base + ofs)
#define SCP_REG(ofs)		(scpsys_base + ofs)

#ifdef SPM_PWR_STATUS
#undef SPM_PWR_STATUS
#endif

#ifdef SPM_PWR_STATUS_2ND
#undef SPM_PWR_STATUS_2ND
#endif

#define CLK_CFG_4		TOPCKGEN_REG(0x080)
#define CLK_CFG_6		TOPCKGEN_REG(0x0A0)
#define INFRA_PDN_STA		INFRA_REG(0x0048)
#define PERI_PDN0_STA		PREI_REG(0x0018)
#define PERI_PDN1_STA		PREI_REG(0x001C)
#define AUDIO_TOP_CON0		AUDIO_REG(0x0000)
#define MFG_CG_CON		MFG_REG(0)
#define DISP_CG_CON0		MM_REG(0x100)
#define DISP_CG_CON1		MM_REG(0x110)
#define IMG_CG_CON		IMG_REG(0x0000)
#define VDEC_CKEN_SET		VDEC_REG(0x0000)
#define LARB_CKEN_SET		VDEC_REG(0x0008)
#define VENC_CG_CON		VENC_REG(0x0)
#define VENCLT_CG_CON		VENCLT_REG(0x0)
#define SPM_PWR_STATUS		SCP_REG(0x060c)
#define SPM_PWR_STATUS_2ND	SCP_REG(0x0610)

#define DIS_PWR_STA_MASK		BIT(3)
#define MFG_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define VDE_PWR_STA_MASK		BIT(7)
#define VEN2_PWR_STA_MASK		BIT(20)
#define VEN_PWR_STA_MASK		BIT(21)
#define MFG_2D_PWR_STA_MASK		BIT(22)
#define MFG_ASYNC_PWR_STA_MASK		BIT(23)
#define AUDIO_PWR_STA_MASK		BIT(24)
#define USB_PWR_STA_MASK		BIT(25)

enum subsys_id {
	SYS_VDE,
	SYS_MFG,
	SYS_VEN,
	SYS_ISP,
	SYS_DIS,
	SYS_VEN2,
	SYS_AUDIO,
	SYS_MFG_2D,
	SYS_MFG_ASYNC,
	SYS_USB,
	NR_SYSS,
};

static int sys_is_on(enum subsys_id id)
{
	u32 pwr_sta_mask[] = {
		VDE_PWR_STA_MASK,
		MFG_PWR_STA_MASK,
		VEN_PWR_STA_MASK,
		ISP_PWR_STA_MASK,
		DIS_PWR_STA_MASK,
		VEN2_PWR_STA_MASK,
		AUDIO_PWR_STA_MASK,
		MFG_2D_PWR_STA_MASK,
		MFG_ASYNC_PWR_STA_MASK,
		USB_PWR_STA_MASK,
	};

	u32 mask = pwr_sta_mask[id];
	u32 sta = idle_readl(SPM_PWR_STATUS);
	u32 sta_s = idle_readl(SPM_PWR_STATUS_2ND);

	return (sta & mask) && (sta_s & mask);
}

static void get_all_clock_state(u32 clks[NR_GRPS])
{
	int i;

	for (i = 0; i < NR_GRPS; i++)
		clks[i] = 0;

	clks[0] = ~idle_readl(PERI_PDN0_STA);		/* PERI0 */
	clks[1] = ~idle_readl(PERI_PDN1_STA);		/* PERI1 */
	clks[2] = ~idle_readl(INFRA_PDN_STA);		/* INFRA */

	if (sys_is_on(SYS_DIS)) {
		clks[3] = ~idle_readl(DISP_CG_CON0);	/* DISP0 */
		clks[4] = ~idle_readl(DISP_CG_CON1);	/* DISP1 */
	}

	if (sys_is_on(SYS_ISP))
		clks[5] = ~idle_readl(IMG_CG_CON);	/* IMAGE */

	if (sys_is_on(SYS_MFG))
		clks[6] = ~idle_readl(MFG_CG_CON);	/* MFG */

	if (sys_is_on(SYS_AUDIO))
		clks[7] = ~idle_readl(AUDIO_TOP_CON0);	/* AUDIO */

	if (sys_is_on(SYS_VDE)) {
		clks[8] = idle_readl(VDEC_CKEN_SET);	/* VDEC0 */
		clks[9] = idle_readl(LARB_CKEN_SET);	/* VDEC1 */
	}

	if (sys_is_on(SYS_VEN))
		clks[10] = idle_readl(VENC_CG_CON);	/* VENC_JPEG */

	if (sys_is_on(SYS_VEN2))
		clks[11] = idle_readl(VENCLT_CG_CON);	/* VENC_LT */
}

static bool clkmgr_idle_can_enter(
		unsigned int *condition_mask, unsigned int *block_mask)
{
	int i;
	u32 clks[NR_GRPS];
	u32 r = 0;

	get_all_clock_state(clks);

	for (i = 0; i < NR_GRPS; i++) {
		block_mask[i] = condition_mask[i] & clks[i];
		r |= block_mask[i];
	}

	return r == 0;
}

static const char *grp_get_name(int id)
{
	const char *grp_name[] = {
		"PERI0",
		"PERI1",
		"INFRA",
		"DISP0",
		"DISP1",
		"IMAGE",
		"MFG",
		"AUDIO",
		"VDEC0",
		"VDEC1",
		"VENC",
		"VENC_LT",
	};

	return grp_name[id];
}

#define INVALID_GRP_ID(grp) (grp < 0 || grp >= NR_GRPS)

#if !IDLE_HAVE_LOCALTIMER

#include <asm/arch_timer.h>

static bool arch_timer_use_virtual; /* = false; */

static inline void set_next_event(int access, u32 evt)
{
	u32 ctrl;

	ctrl = arch_timer_reg_read(access, ARCH_TIMER_REG_CTRL);
	ctrl |= ARCH_TIMER_CTRL_ENABLE;
	ctrl &= ~ARCH_TIMER_CTRL_IT_MASK;
	arch_timer_reg_write(access, ARCH_TIMER_REG_TVAL, evt);
	arch_timer_reg_write(access, ARCH_TIMER_REG_CTRL, ctrl);
}

static void idle_localtimer_set_next_event(u32 evt)
{
	if (arch_timer_use_virtual)
		set_next_event(ARCH_TIMER_VIRT_ACCESS, evt);
	else
		set_next_event(ARCH_TIMER_PHYS_ACCESS, evt);
}

static u32 idle_localtimer_get_counter(void)
{
	u32 evt;

	if (arch_timer_use_virtual)
		evt = arch_timer_reg_read(ARCH_TIMER_VIRT_ACCESS,
				ARCH_TIMER_REG_TVAL);
	else
		evt = arch_timer_reg_read(ARCH_TIMER_PHYS_ACCESS,
				ARCH_TIMER_REG_TVAL);

	return evt;
}

static u64 localtimer_get_counter(void)
{
	return idle_localtimer_get_counter();
}

static void localtimer_set_next_event(u32 evt)
{
	idle_localtimer_set_next_event(evt);
}

#endif /* !IDLE_HAVE_LOCALTIMER */

static u64 idle_get_current_time_ms(void)
{
	struct timeval t;
	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;
	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock |= (1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;
	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock &= ~(1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

static int only_one_cpu_online(void)
{
#ifdef CONFIG_SMP
#if IDLE_HAVE_HOTPLUG
	return (atomic_read(&is_in_hotplug) == 0) &&
		(atomic_read(&hotplug_cpu_count) == 1);
#else
	return !atomic_read(&is_in_hotplug) && num_online_cpus() == 1;
#endif
#else /* !CONFIG_SMP */
	return true;
#endif /* CONFIG_SMP */
}

/************************************************
 * SODI part
 ************************************************/
static DEFINE_MUTEX(soidle_locked);

static void enable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&soidle_locked);
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] |= mask;
	mutex_unlock(&soidle_locked);
}

void enable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	enable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	disable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_soidle_by_bit);

bool soidle_can_enter(int cpu)
{
	int reason = NR_REASONS;

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto out;
	}

#if !SODI_DVT_APxGPT
#if IDLE_HAVE_SODI
	if (spm_get_sodi_en() == 0) {
		reason = BY_OTH;
		goto out;
	}
#endif /* IDLE_HAVE_SODI */

	memset(soidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
	if (!clkmgr_idle_can_enter(soidle_condition_mask, soidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}
#endif

#ifdef CONFIG_SMP
	soidle_timer_left = localtimer_get_counter();
	if (soidle_timer_left < soidle_time_critera ||
			(s32)soidle_timer_left < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	gpt_get_cnt(GPT1, &soidle_timer_left);
	gpt_get_cmp(GPT1, &soidle_timer_cmp);
	if ((soidle_timer_cmp - soidle_timer_left) < soidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif

out:
	if (reason < NR_REASONS) {
		soidle_block_cnt[cpu][reason]++;
		return false;
	} else
		return true;
}

void soidle_before_wfi(int cpu)
{
#ifdef CONFIG_SMP
	#if SODI_DVT_APxGPT && SODI_DVT_APxGPT_TimerCount_5S
	soidle_timer_left2 = 65000000;
	#else
	soidle_timer_left2 = localtimer_get_counter();
	#endif

	if ((s32)soidle_timer_left2 <= 0)
		gpt_set_cmp(idle_gpt, 1); /* Trigger idle_gpt Timerout */
	else
		gpt_set_cmp(idle_gpt, soidle_timer_left2);

	start_gpt(idle_gpt);
#else
	gpt_get_cnt(GPT1, &soidle_timer_left2);
#endif
}

void soidle_after_wfi(int cpu)
{
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(idle_gpt))
		localtimer_set_next_event(1);
	else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;
		gpt_get_cnt(idle_gpt, &cnt);
		gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
					__func__, idle_gpt + 1, cnt, cmp);
			BUG();
		}

		localtimer_set_next_event(cmp - cnt);
		stop_gpt(idle_gpt);
	}
#endif
#if SODI_DVT_APxGPT
	pr_info("soidle_cnt:%d\n", soidle_cnt[cpu]);
#endif
	soidle_cnt[cpu]++;
}

/************************************************
 * multi-core idle part
 ************************************************/
static DEFINE_MUTEX(mcidle_locked);
bool mcidle_can_enter(int cpu)
{
	int reason = NR_REASONS;

	if (cpu == 0 || topology_physical_package_id(cpu) == 1) {
		reason = BY_VTG;
		goto mcidle_out;
	}

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto mcidle_out;
	}

	if (only_one_cpu_online()) {
		reason = BY_CPU;
		goto mcidle_out;
	}

#if IDLE_HAVE_MCDI
	if (spm_mcdi_can_enter() == 0) {
		reason = BY_OTH;
		goto mcidle_out;
	}
#endif /* IDLE_HAVE_MCDI */

	#if !MCDI_DVT_IPI && !MCDI_DVT_CPUxGPT
	mcidle_timer_left[cpu] = localtimer_get_counter();
	if (mcidle_timer_left[cpu] < mcidle_time_critera ||
			(s32)mcidle_timer_left[cpu] < 0) {
		reason = BY_TMR;
		goto mcidle_out;
	}
	#endif

mcidle_out:
	if (reason < NR_REASONS) {
		mcidle_block_cnt[cpu][reason]++;
		return false;
	} else
		return true;
}
bool spm_mcdi_xgpt_timeout[NR_CPUS];

void mcidle_before_wfi(int cpu)
{
#if !MCDI_DVT_IPI
	u64 set_count = 0;

	spm_mcdi_xgpt_timeout[cpu] = 0;

	#if MCDI_DVT_CPUxGPT
	localtimer_set_next_event(130000000);
	mcidle_timer_left2[cpu] = 65000000;
	#else
	mcidle_timer_left2[cpu] = localtimer_get_counter();
	#endif
	mcidle_timer_before_wfi[cpu] = localtimer_get_phy_count();

	set_count = mcidle_timer_before_wfi[cpu] + (s32)mcidle_timer_left2[cpu];

	cpu_xgpt_set_cmp(cpu, set_count);
#endif
}

int mcdi_xgpt_wakeup_cnt[NR_CPUS];

void mcidle_after_wfi(int cpu)
{
#if !MCDI_DVT_IPI
	u64 cmp;

	cpu_xgpt_irq_dis(cpu); /* ack cpuxgpt, api need refine from Weiqi */
#if !MCDI_DVT_CPUxGPT
	cmp = (localtimer_get_phy_count() - mcidle_timer_before_wfi[cpu]);

	if (cmp < (s32)mcidle_timer_left2[cpu])
		localtimer_set_next_event(mcidle_timer_left2[cpu] - cmp);
	else
		localtimer_set_next_event(1);
#endif
#endif
}

unsigned int g_pre_SPM_MCDI_Abnormal_WakeUp = 0;

static bool go_to_mcidle(int cpu)
{
#if IDLE_HAVE_MCDI
	bool ret = 0;
	if (spm_mcdi_wfi(cpu) == 1) {
		mcidle_cnt[cpu] += 1;
		#if MCDI_DVT_CPUxGPT || MCDI_DVT_IPI
		mdelay(1);
		pr_info("CPU %d awake %d\n", cpu, mcidle_cnt[cpu]);
		#endif
		ret = 1;
	}

	if (g_SPM_MCDI_Abnormal_WakeUp != g_pre_SPM_MCDI_Abnormal_WakeUp) {
		pr_err("SPM-MCDI Abnormal %x\n", g_SPM_MCDI_Abnormal_WakeUp);
		g_pre_SPM_MCDI_Abnormal_WakeUp = g_SPM_MCDI_Abnormal_WakeUp;
	}
	return ret;
#else /* !IDLE_HAVE_MCDI */
	return false;
#endif /* IDLE_HAVE_MCDI */
}

static void mcidle_idle_pre_handler(int cpu)
{
	if (cpu == 0) {
#if IDLE_HAVE_MCDI
		if (only_one_cpu_online() ||
			(idle_switch[IDLE_TYPE_MC] == 0))
			spm_mcdi_switch_on_off(SPM_MCDI_IDLE, 0);
		else
			spm_mcdi_switch_on_off(SPM_MCDI_IDLE, 1);
#endif /* IDLE_HAVE_MCDI */
	}
}


/************************************************
 * deep idle part
 ************************************************/
static DEFINE_MUTEX(dpidle_locked);

#if AUDINTBUS_CLOCK_SWITCH

static u32 clk_cfg_4;

static void backup_audintbus_26MHz(void)
{
	if (clk_cfg_4)
		return;

	clk_cfg_4 = idle_readl(CLK_CFG_4);
	idle_writel(CLK_CFG_4, clk_cfg_4 & 0xF8FFFFFF);
}

static void restore_audintbus(void)
{
	if (!clk_cfg_4)
		return;

	idle_writel(CLK_CFG_4, clk_cfg_4);
	clk_cfg_4 = 0;
}

#endif /* AUDINTBUS_CLOCK_SWITCH */

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] |= mask;
	mutex_unlock(&dpidle_locked);
}

void enable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	enable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	disable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);

static bool dpidle_can_enter(void)
{
	int reason = NR_REASONS;
	int i = 0;
	u64 dpidle_block_curr_time = 0;

#if IDLE_HAVE_CPUFREQ
	if (!mt_cpufreq_earlysuspend_status_get()) {
		reason = BY_VTG;
		goto out;
	}
#endif /* IDLE_HAVE_CPUFREQ */

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto out;
	}

	memset(dpidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
	if (!clkmgr_idle_can_enter(dpidle_condition_mask, dpidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}

#ifdef CONFIG_SMP
	dpidle_timer_left = localtimer_get_counter();
	if (dpidle_timer_left < dpidle_time_critera ||
			(s32)dpidle_timer_left < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	gpt_get_cnt(GPT1, &dpidle_timer_left);
	gpt_get_cmp(GPT1, &dpidle_timer_cmp);
	if ((dpidle_timer_cmp-dpidle_timer_left) < dpidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif

out:
	if (reason >= NR_REASONS) {
		dpidle_block_prev_time = idle_get_current_time_ms();
		return true;
	}

	dpidle_block_cnt[reason]++;

	if (dpidle_block_prev_time == 0)
		dpidle_block_prev_time = idle_get_current_time_ms();

	dpidle_block_curr_time = idle_get_current_time_ms();
	if ((dpidle_block_curr_time - dpidle_block_prev_time) <=
		dpidle_block_time_critera)
		return false;

	if ((smp_processor_id() != 0))
		return false;

	for (i = 0; i < nr_cpu_ids; i++) {
		idle_ver("dpidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
				i, dpidle_cnt[i], i, rgidle_cnt[i]);
	}

	for (i = 0; i < NR_REASONS; i++) {
		idle_ver("[%d]dpidle_block_cnt[%s]=%lu\n", i, reason_name[i],
				dpidle_block_cnt[i]);
	}

#if IDLE_HAVE_CLKMGR
	for (i = 0; i < NR_GRPS; i++) {
		idle_ver(
			"[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\tdpidle_block_mask[%-7s]=0x%08x\n",
			i,
			grp_get_name(i), dpidle_condition_mask[i],
			grp_get_name(i), dpidle_block_mask[i]);
	}
#endif /* IDLE_HAVE_CLKMGR */

	memset(dpidle_block_cnt, 0, sizeof(dpidle_block_cnt));
	dpidle_block_prev_time = idle_get_current_time_ms();

	return false;
}

void spm_dpidle_before_wfi(void)
{
	if (TRUE == mt_dpidle_chk_golden) {
		/* FIXME: */
#if 0
		mt_power_gs_dump_dpidle();
#endif
	}

#if DPIDLE_BUS_DCM
	bus_dcm_enable();
#endif

#if AUDINTBUS_CLOCK_SWITCH
	backup_audintbus_26MHz();
#endif

#ifdef CONFIG_SMP
	dpidle_timer_left2 = localtimer_get_counter();

	if ((s32)dpidle_timer_left2 <= 0)
		gpt_set_cmp(idle_gpt, 1); /* Trigger GPT4 Timerout imediately */
	else
		gpt_set_cmp(idle_gpt, dpidle_timer_left2);

	start_gpt(idle_gpt);
#else
	gpt_get_cnt(idle_gpt, &dpidle_timer_left2);
#endif
}

void spm_dpidle_after_wfi(void)
{
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(idle_gpt)) {
		/* waked up by WAKEUP_GPT */
		localtimer_set_next_event(1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;
		gpt_get_cnt(idle_gpt, &cnt);
		gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
				__func__, idle_gpt + 1, cnt, cmp);
			BUG();
		}

		localtimer_set_next_event(cmp - cnt);
		stop_gpt(idle_gpt);
		/* GPT_ClearCount(WAKEUP_GPT); */
	}
#endif

#if AUDINTBUS_CLOCK_SWITCH
	restore_audintbus();
#endif

#if DPIDLE_BUS_DCM
	bus_dcm_disable();
#endif

	dpidle_cnt[0]++;
}


/************************************************
 * slow idle part
 ************************************************/
static DEFINE_MUTEX(slidle_locked);


static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(void)
{
	int reason = NR_REASONS;

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	memset(slidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
	if (!clkmgr_idle_can_enter(slidle_condition_mask, slidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}

#if EN_PTP_OD
	if (ptp_data[0]) {
		reason = BY_OTH;
		goto out;
	}
#endif

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

#if CCI400_CLOCK_SWITCH

static u32 clk_cfg_6;

static void backup_cci400_26MHz(void)
{
	struct mtk_irq_mask mask;

	if (!only_one_cpu_online() || clk_cfg_6)
		return;

	mt_irq_mask_all(&mask);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	clk_cfg_6 = idle_readl(CLK_CFG_6);
	idle_writel(CLK_CFG_6, clk_cfg_6 & 0xFFF8FFFF);

	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
}

static void restore_cci400(void)
{
	struct mtk_irq_mask mask;

	if (!clk_cfg_6)
		return;

	mt_irq_mask_all(&mask);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	idle_writel(CLK_CFG_6, clk_cfg_6);
	clk_cfg_6 = 0;

	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
}

#endif /* CCI400_CLOCK_SWITCH */

static void slidle_before_wfi(int cpu)
{
	bus_dcm_enable();

#if CCI400_CLOCK_SWITCH
	backup_cci400_26MHz();
#endif
}

static void slidle_after_wfi(int cpu)
{
#if CCI400_CLOCK_SWITCH
	restore_cci400();
#endif

	bus_dcm_disable();
	slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	dsb();
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}


/************************************************
 * regular idle part
 ************************************************/
static void rgidle_before_wfi(int cpu)
{
}

static void rgidle_after_wfi(int cpu)
{
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);

	dsb();
	__asm__ __volatile__("wfi" : : : "memory");

	rgidle_after_wfi(cpu);
}

/************************************************
 * idle task flow part
 ************************************************/

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */

#if MCDI_DVT_IPI
DEFINE_SPINLOCK(__mcdi_lock);

unsigned int core0_IPI_issue_count = 0;
u8 mcdi_enter = 0;

static void empty_function(void *info)
{
	unsigned long flags;
	int cpu = smp_processor_id();
	spin_lock_irqsave(&__mcdi_lock, flags);
	mcdi_enter &= ~(1 << cpu);
	spin_unlock_irqrestore(&__mcdi_lock, flags);
	mdelay(1);
	pr_info(
		"core %x ipi received, WFI count %d, core IPI command count: %d\n",
		cpu, mcidle_cnt[cpu], core0_IPI_issue_count);
}

static inline void mcidle_DVT_IPI_handler(int cpu)
{
	int cpu0_enter_forLoop = 0;
	unsigned long flags;

	if (!idle_switch[IDLE_TYPE_MC])
		return;

	for (;;) {
		if (cpu == 0) {
			if ((spm_read(SPM_SLEEP_TIMER_STA) & 0xfe)) {
				mdelay(1);
				smp_call_function(empty_function, NULL, 0);
				pr_info("core0 IPI\n");
				core0_IPI_issue_count++;
			}
		} else {
			if (go_to_mcidle(cpu) == 0)
				return;

			mdelay(1);
		}
	}
}
#endif

#if MCDI_DVT_CPUxGPT
unsigned int test_cpu = 0;

static inline void mcidle_DVT_CPUxGPT_handler(int cpu)
{
	if (!idle_switch[IDLE_TYPE_MC])
		return;

	for (;;) {
		if (cpu != 0) {
			if (mcidle_can_enter(cpu)) {
				if (go_to_mcidle(cpu) == 0)
					return;
			}
		}
	}
}
#endif

static inline int mcidle_handler(int cpu)
{
#if MCDI_DVT_IPI
	mcidle_DVT_IPI_handler(cpu);
#elif MCDI_DVT_CPUxGPT
	mcidle_DVT_CPUxGPT_handler(cpu);
#else
	if (idle_switch[IDLE_TYPE_MC]) {
		if (mcidle_can_enter(cpu)) {
			go_to_mcidle(cpu);
			return 1;
		}
	}
#endif
	return 0;
}

#if IDLE_HAVE_SODI

static u32 slp_spm_SODI_flags = {
	/* SPM_CPU_DVS_DIS */ /* not verfication yet */
	0
};

#endif /* IDLE_HAVE_SODI */

static inline int soidle_handler(int cpu)
{
#if IDLE_HAVE_SODI
	if (!idle_switch[IDLE_TYPE_SO])
		return 0;

	if (!soidle_can_enter(cpu))
		return 0;

	#if SODI_DVT_APxGPT
	pr_info("SPM-Enter SODI+\n");
	#endif
	spm_go_to_sodi(slp_spm_SODI_flags, 0);
	#if SODI_DVT_APxGPT
	pr_info("SPM-Enter SODI-\n");
	#endif
#ifdef CONFIG_SMP
	idle_ver("SO:timer_left=%u, timer_left2=%u, delta=%u\n",
		soidle_timer_left, soidle_timer_left2,
		soidle_timer_left - soidle_timer_left2);
#else
	idle_ver("SO:timer_left=%u, timer_left2=%u, delta=%lu,timeout val=%u\n",
		soidle_timer_left, soidle_timer_left2,
		soidle_timer_left2 - soidle_timer_left,
		soidle_timer_cmp - soidle_timer_left);
#endif

	return 1;
#else /* !IDLE_HAVE_SODI */
	return 0;
#endif /* IDLE_HAVE_SODI */
}

static u32 slp_spm_deepidle_flags = {
	SPM_SCREEN_OFF
};

static inline void dpidle_pre_handler(void)
{
#if IDLE_HAVE_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();
#endif /* IDLE_HAVE_THERMAL */

#if IDLE_HAVE_GPUDVFS
	/* disable gpu dvfs timer */
	mtk_enable_gpu_dvfs_timer(false);
#endif /* IDLE_HAVE_GPUDVFS */
}

static inline void dpidle_post_handler(void)
{
#if IDLE_HAVE_GPUDVFS
	/* enable gpu dvfs timer */
	mtk_enable_gpu_dvfs_timer(true);
#endif /* IDLE_HAVE_GPUDVFS */

#if IDLE_HAVE_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();
#endif /* IDLE_HAVE_THERMAL */
}

static inline int dpidle_handler(int cpu)
{
	int ret = 0;

	if (!idle_switch[IDLE_TYPE_DP])
		return ret;

	if (!dpidle_can_enter())
		return ret;

	dpidle_pre_handler();
	spm_go_to_dpidle(slp_spm_deepidle_flags, 0);
	dpidle_post_handler();
	ret = 1;

#ifdef CONFIG_SMP
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left - dpidle_timer_left2);
#else
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u, timeout val=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left2 - dpidle_timer_left,
		dpidle_timer_cmp - dpidle_timer_left);
#endif

	return ret;
}

static inline int slidle_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SL]) {
		if (slidle_can_enter()) {
			go_to_slidle(cpu);
			ret = 1;
		}
	}

	return ret;
}

static inline int rgidle_handler(int cpu)
{
	int ret = 0;
	if (idle_switch[IDLE_TYPE_RG]) {
		go_to_rgidle(cpu);
		ret = 1;
	}

	return ret;
}

static int (*idle_handlers[NR_TYPES])(int) = {
	dpidle_handler,
	soidle_handler,
	mcidle_handler,
	slidle_handler,
	rgidle_handler,
};

void arch_idle(void)
{
	int cpu = smp_processor_id();
	int i;

	/* dynamic on/offload between single/multi core deepidles */
	mcidle_idle_pre_handler(cpu);

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_handlers[i](cpu))
			break;
	}
}

#define idle_attr(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr = {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show = _name##_show,			\
	.store = _name##_store,			\
}

static ssize_t mcidle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int cpus, reason;

	p += sprintf(p, "*********** multi-core idle state ************\n");
	p += sprintf(p, "mcidle_time_critera=%u\n", mcidle_time_critera);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		p += sprintf(p, "cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			p += sprintf(p, "[%d]mcidle_block_cnt[%s]=%lu\n",
				reason,
				reason_name[reason],
				mcidle_block_cnt[cpus][reason]);
		}
		p += sprintf(p, "\n");
	}

	p += sprintf(p, "\n********** mcidle command help **********\n");
	p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
	p += sprintf(p,
		"switch on/off: echo [mcidle] 1/0 > /sys/power/mcidle_state\n");
	p += sprintf(p,
		"modify tm_cri: echo time value > /sys/power/mcidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t mcidle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);

	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "mcidle", sizeof("mcidle")))
		idle_switch[IDLE_TYPE_MC] = param;
	else if (!strncmp(cmd_str, "time", sizeof("time")))
		mcidle_time_critera = param;
	else
		return -EINVAL;

	return count;
}
idle_attr(mcidle_state);

static ssize_t soidle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int cpus, reason, i;

	p += sprintf(p, "*********** Screen on idle state ************\n");
	p += sprintf(p, "soidle_time_critera=%u\n", soidle_time_critera);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		p += sprintf(p, "cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			p += sprintf(p, "[%d]soidle_block_cnt[%s]=%lu\n",
				reason, reason_name[reason],
				soidle_block_cnt[cpus][reason]);
		}
		p += sprintf(p, "\n");
	}

#if IDLE_HAVE_CLKMGR
	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p,
			"[%02d]soidle_condition_mask[%-8s]=0x%08x\t\tsoidle_block_mask[%-7s]=0x%08x\n",
			i,
			grp_get_name(i), soidle_condition_mask[i],
			grp_get_name(i), soidle_block_mask[i]);
	}
#endif /* IDLE_HAVE_CLKMGR */

	p += sprintf(p, "\n********** soidle command help **********\n");
	p += sprintf(p, "soidle help:   cat /sys/power/soidle_state\n");
	p += sprintf(p,
		"switch on/off: echo [soidle] 1/0 > /sys/power/soidle_state\n");
	p += sprintf(p,
		"en_so_by_bit:  echo enable id > /sys/power/soidle_state\n");
	p += sprintf(p,
		"dis_so_by_bit: echo disable id > /sys/power/soidle_state\n");
	p += sprintf(p,
		"modify tm_cri: echo time value > /sys/power/soidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t soidle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);

	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "soidle", sizeof("soidle")))
		idle_switch[IDLE_TYPE_SO] = param;
	else if (!strncmp(cmd_str, "enable", sizeof("enable")))
		enable_soidle_by_bit(param);
	else if (!strncmp(cmd_str, "disable", sizeof("disable")))
		disable_soidle_by_bit(param);
	else if (!strncmp(cmd_str, "time", sizeof("time")))
		soidle_time_critera = param;
	else
		return -EINVAL;

	return count;
}
idle_attr(soidle_state);

static ssize_t dpidle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "*********** deep idle state ************\n");
	p += sprintf(p, "dpidle_time_critera=%u\n", dpidle_time_critera);

	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]dpidle_block_cnt[%s]=%lu\n",
			i, reason_name[i], dpidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

#if IDLE_HAVE_CLKMGR
	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p,
			"[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\tdpidle_block_mask[%-7s]=0x%08x\n",
			i,
			grp_get_name(i), dpidle_condition_mask[i],
			grp_get_name(i), dpidle_block_mask[i]);
	}
#endif /* IDLE_HAVE_CLKMGR */

	p += sprintf(p, "\n*********** dpidle command help  ************\n");
	p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
	p += sprintf(p,
		"switch on/off: echo [dpidle] 1/0 > /sys/power/dpidle_state\n");
	p += sprintf(p,
		"cpupdn on/off: echo cpupdn 1/0 > /sys/power/dpidle_state\n");
	p += sprintf(p,
		"en_dp_by_bit:  echo enable id > /sys/power/dpidle_state\n");
	p += sprintf(p,
		"dis_dp_by_bit: echo disable id > /sys/power/dpidle_state\n");
	p += sprintf(p,
		"modify tm_cri: echo time value > /sys/power/dpidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t dpidle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);

	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "dpidle", sizeof("dpidle")))
		idle_switch[IDLE_TYPE_DP] = param;
	else if (!strncmp(cmd_str, "enable", sizeof("enable")))
		enable_dpidle_by_bit(param);
	else if (!strncmp(cmd_str, "disable", sizeof("disable")))
		disable_dpidle_by_bit(param);
	else if (!strncmp(cmd_str, "time", sizeof("time")))
		dpidle_time_critera = param;
	else
		return -EINVAL;

	return count;
}
idle_attr(dpidle_state);

static ssize_t slidle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "*********** slow idle state ************\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]slidle_block_cnt[%s]=%lu\n",
				i, reason_name[i], slidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

#if IDLE_HAVE_CLKMGR
	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p,
			"[%02d]slidle_condition_mask[%-8s]=0x%08x\t\tslidle_block_mask[%-7s]=0x%08x\n",
			i,
			grp_get_name(i), slidle_condition_mask[i],
			grp_get_name(i), slidle_block_mask[i]);
	}
#endif /* IDLE_HAVE_CLKMGR */

	p += sprintf(p, "\n********** slidle command help **********\n");
	p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
	p += sprintf(p,
		"switch on/off: echo [slidle] 1/0 > /sys/power/slidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t slidle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);

	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "slidle", sizeof("slidle")))
		idle_switch[IDLE_TYPE_SL] = param;
	else if (!strncmp(cmd_str, "enable", sizeof("enable")))
		enable_slidle_by_bit(param);
	else if (!strncmp(cmd_str, "disable", sizeof("disable")))
		disable_slidle_by_bit(param);
	else
		return -EINVAL;

	return count;
}
idle_attr(slidle_state);

static ssize_t rgidle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "*********** regular idle state ************\n");
	p += sprintf(p, "\n********** rgidle command help **********\n");
	p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");
	p += sprintf(p,
		"switch on/off: echo [rgidle] 1/0 > /sys/power/rgidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t rgidle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);

	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "rgidle", sizeof("rgidle")))
		idle_switch[IDLE_TYPE_RG] = param;
	else
		return -EINVAL;

	return count;
}
idle_attr(rgidle_state);

static ssize_t idle_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		p += sprintf(p,
			"soidle_cnt[%d]=%lu, dpidle_cnt[%d]=%lu, mcidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
			i, soidle_cnt[i], i, dpidle_cnt[i],
			i, mcidle_cnt[i], i, slidle_cnt[i],
			i, rgidle_cnt[i]);
	}

	p += sprintf(p, "\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		p += sprintf(p, "%s_switch=%d, ", idle_name[i], idle_switch[i]);

	p += sprintf(p, "\n");

	p += sprintf(p, "\n********** idle command help **********\n");
	p += sprintf(p, "status help:   cat /sys/power/idle_state\n");
	p += sprintf(p,
		"switch on/off: echo switch mask > /sys/power/idle_state\n");

	p += sprintf(p, "soidle help:   cat /sys/power/soidle_state\n");
	p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
	p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
	p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
	p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t idle_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	int idx;
	size_t count;
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(n, sizeof(cmd_buf) - 1);
	memcpy(cmd_buf, buf, count);
	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");
	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);
	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "switch", sizeof("switch")))
		for (idx = 0; idx < NR_TYPES; idx++)
			idle_switch[idx] = (param & (1U << idx)) ? 1 : 0;
	else
		return -EINVAL;

	return count;
}
idle_attr(idle_state);

static int __init get_base_from_node(
	const char *cmp, void __iomem **pbase, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		idle_err("node '%s' not found!\n", cmp);
		return -1;
	}

	*pbase = of_iomap(node, idx);

	return 0;
}

static void __init init_iomap(void)
{
	get_base_from_node("mediatek,mt8173-topckgen", &topckgen_base, 0);
	get_base_from_node("mediatek,mt8173-infrasys", &infrasys_base, 0);
	get_base_from_node("mediatek,mt8173-perisys", &perisys_base, 0);
	get_base_from_node("mediatek,mt8173-audiosys", &audiosys_base, 0);
	get_base_from_node("mediatek,mt8173-mfgsys", &mfgsys_base, 0);
	get_base_from_node("mediatek,mt8173-mmsys", &mmsys_base, 0);
	get_base_from_node("mediatek,mt8173-imgsys", &imgsys_base, 0);
	get_base_from_node("mediatek,mt8173-vdecsys", &vdecsys_base, 0);
	get_base_from_node("mediatek,mt8173-vencsys", &vencsys_base, 0);
	get_base_from_node("mediatek,mt8173-vencltsys", &vencltsys_base, 0);
	get_base_from_node("mediatek,mt8173-scpsys", &scpsys_base, 1);
}

void __init mt_idle_init(void)
{
	int err = 0;
	int i;

	idle_info("[%s]entry!!\n", __func__);

	init_iomap();

	err = request_gpt(idle_gpt, GPT_ONE_SHOT, GPT_CLK_SRC_SYS,
		GPT_CLK_DIV_1, 0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_err("[%s]fail to request GPT%d\n",
			__func__, idle_gpt + 1);

	err = 0;

	for (i = 0; i < num_possible_cpus(); i++)
		err |= cpu_xgpt_register_timer(i, NULL);

	if (err)
		idle_err("[%s]fail to request cpuxgpt\n", __func__);

	err = sysfs_create_file(power_kobj, &idle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &soidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &mcidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &dpidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &slidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &rgidle_state_attr.attr);

	if (err)
		idle_err("[%s]: fail to create sysfs\n", __func__);

	arm_pm_idle = arch_idle;
}

module_param(mt_idle_chk_golden, bool, 0644);
module_param(mt_dpidle_chk_golden, bool, 0644);
