#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/console.h>
#include <linux/aee.h>
#include <linux/proc_fs.h>
#include <linux/wakelock.h>

#include <mach/sync_write.h>
#include <mach/mt_sleep.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_clkmgr.h>

#if defined(CONFIG_AMAZON_METRICS_LOG)
#include <linux/metricslog.h>
#endif /* CONFIG_AMAZON_METRICS_LOG	*/


/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     1
#define SLP_SUSPEND_LOG_EN          1
#else
#define SLP_SLEEP_DPIDLE_EN         0
#define SLP_REPLACE_DEF_WAKESRC     0
#define SLP_SUSPEND_LOG_EN          0
#endif


/**************************************
 * SW code for suspend
 **************************************/
#define slp_read(addr)              (*(volatile u32 *)(addr))
#define slp_write(addr, val)        mt65xx_reg_sync_writel(val, addr)

#define slp_emerg(fmt, args...)     pr_emerg("[SLP] " fmt, ##args)
#define slp_alert(fmt, args...)     pr_alert("[SLP] " fmt, ##args)
#define slp_crit(fmt, args...)      pr_crit("[SLP] " fmt, ##args)
#define slp_error(fmt, args...)     pr_err("[SLP] " fmt, ##args)
#define slp_warning(fmt, args...)   pr_warn("[SLP] " fmt, ##args)
#define slp_notice(fmt, args...)    pr_notice("[SLP] " fmt, ##args)
#define slp_info(fmt, args...)      pr_info("[SLP] " fmt, ##args)
#define slp_debug(fmt, args...)     pr_debug("[SLP] " fmt, ##args)

#define slp_crit2(fmt, args...)     \
do {                                \
    aee_sram_printk(fmt, ##args);   \
    slp_crit(fmt, ##args);          \
} while (0)

static DEFINE_SPINLOCK(slp_lock);

static wake_reason_t slp_wake_reason = WR_NONE;

struct wake_lock sleep_wake_lock;

bool slp_ck26m_on = false;

/*
 * SLEEP_DPIDLE_EN:1 && slp_ck26m_on:true
 *    1 = CPU dormant
 *    0 = CPU standby
 * SLEEP_DPIDLE_EN:0
 *    1 = CPU shutdown
 *    0 = CPU standby
 */
bool slp_cpu_pdn = 1;

/*
 * 1 = INFRA/DDRPHY power down
 * 0 = keep INFRA/DDRPHY power
 */

#ifdef CONFIG_PURE_AP_USE_EXTERNAL_MODEM	/* no infra power down for 3g till usb runtime suspend ready (WK13.24) */
bool slp_infra_pdn = 0;		/* FIXME */
#else
bool slp_infra_pdn = 1;
#endif

bool slp_chk_golden = 0;
bool slp_dump_gpio = 0;
bool slp_dump_regs = 0;
int slp_pcm_timer = 0;

int success_flag = 0;

static void slp_dump_pm_regs(void)
{
	/* PLL register */
	slp_debug("AP_PLL_CON0     0x%x = 0x%x\n", AP_PLL_CON0, slp_read(AP_PLL_CON0));
	slp_debug("AP_PLL_CON1     0x%x = 0x%x\n", AP_PLL_CON1, slp_read(AP_PLL_CON1));
	slp_debug("AP_PLL_CON2     0x%x = 0x%x\n", AP_PLL_CON2, slp_read(AP_PLL_CON2));
	slp_debug("MAINPLL_CON0    0x%x = 0x%x\n", MAINPLL_CON0, slp_read(MAINPLL_CON0));
	slp_debug("MAINPLL_PWR_CON 0x%x = 0x%x\n", MAINPLL_PWR_CON0, slp_read(MAINPLL_PWR_CON0));
	slp_debug("MMPLL_CON0      0x%x = 0x%x\n", MMPLL_CON0, slp_read(MMPLL_CON0));
/* slp_debug("ISPPLL_CON0     0x%x = 0x%x\n", ISPPLL_CON0                , slp_read(ISPPLL_CON0)); */
	slp_debug("MSDCPLL_CON0    0x%x = 0x%x\n", MSDCPLL_CON0, slp_read(MSDCPLL_CON0));
	slp_debug("MSDCPLL_PWR_CON 0x%x = 0x%x\n", MSDCPLL_PWR_CON0, slp_read(MSDCPLL_PWR_CON0));
	slp_debug("TVDPLL_CON0     0x%x = 0x%x\n", TVDPLL_CON0, slp_read(TVDPLL_CON0));
	slp_debug("TVDPLL_PWR_CON  0x%x = 0x%x\n", TVDPLL_PWR_CON0, slp_read(TVDPLL_PWR_CON0));
	slp_debug("LVDSPLL_CON0    0x%x = 0x%x\n", LVDSPLL_CON0, slp_read(LVDSPLL_CON0));
	slp_debug("LVDSPLL_PWR_CON 0x%x = 0x%x\n", LVDSPLL_PWR_CON0, slp_read(LVDSPLL_PWR_CON0));

	/* INFRA/PERICFG register */
	slp_debug("INFRA_PDN_STA   0x%x = 0x%x\n", INFRA_PDN_STA, slp_read(INFRA_PDN_STA));
	slp_debug("PERI_PDN0_STA   0x%x = 0x%x\n", PERI_PDN0_STA, slp_read(PERI_PDN0_STA));
	slp_debug("PERI_PDN1_STA   0x%x = 0x%x\n", PERI_PDN1_STA, slp_read(PERI_PDN1_STA));

	/* SPM register */
	slp_debug("POWER_ON_VAL0   0x%x = 0x%x\n", SPM_POWER_ON_VAL0, slp_read(SPM_POWER_ON_VAL0));
	slp_debug("POWER_ON_VAL1   0x%x = 0x%x\n", SPM_POWER_ON_VAL1, slp_read(SPM_POWER_ON_VAL1));
	slp_debug("PCM_PWR_IO_EN   0x%x = 0x%x\n", SPM_PCM_PWR_IO_EN, slp_read(SPM_PCM_PWR_IO_EN));
	slp_debug("PCM_REG0_DATA   0x%x = 0x%x\n", SPM_PCM_REG0_DATA, slp_read(SPM_PCM_REG0_DATA));
	slp_debug("PCM_REG7_DATA   0x%x = 0x%x\n", SPM_PCM_REG7_DATA, slp_read(SPM_PCM_REG7_DATA));
	slp_debug("PCM_REG13_DATA  0x%x = 0x%x\n", SPM_PCM_REG13_DATA,
		  slp_read(SPM_PCM_REG13_DATA));
	slp_debug("CLK_CON         0x%x = 0x%x\n", SPM_CLK_CON, slp_read(SPM_CLK_CON));
	slp_debug("AP_DVFS_CON     0x%x = 0x%x\n", SPM_AP_DVFS_CON_SET,
		  slp_read(SPM_AP_DVFS_CON_SET));
	slp_debug("PWR_STATUS      0x%x = 0x%x\n", SPM_PWR_STATUS, slp_read(SPM_PWR_STATUS));
	slp_debug("PWR_STATUS_S    0x%x = 0x%x\n", SPM_PWR_STATUS_S, slp_read(SPM_PWR_STATUS_S));
	slp_debug("SLEEP_TIMER_STA 0x%x = 0x%x\n", SPM_SLEEP_TIMER_STA,
		  slp_read(SPM_SLEEP_TIMER_STA));
}

#ifdef CONFIG_AMAZON_METRICS_LOG
static struct work_struct metrics_work_offmode;
static char metrics_buf_offmode[50];
static char metrics_buf_pwrdm_state[128];
static int suspend_success;

static void wokeup_metrics_offmode(struct work_struct *work)
{
	/* Log suspend state failure or success */
	log_to_metrics(ANDROID_LOG_INFO, "kernel", metrics_buf_offmode);
	if (!suspend_success)
		log_to_metrics(ANDROID_LOG_INFO, "kernel",
			metrics_buf_pwrdm_state);

	suspend_success = success_flag;
}
#endif /* CONFIG_AMAZON_METRICS_LOG	*/

static int slp_suspend_ops_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM;
}

static int slp_suspend_ops_begin(suspend_state_t state)
{
	/* legacy log */
	slp_debug("Chip_pm_begin(%u)(%u)\n", slp_cpu_pdn, slp_infra_pdn);

	slp_wake_reason = WR_NONE;

	success_flag = 0;

	return 0;
}

static int slp_suspend_ops_prepare(void)
{
	/* legacy log */
	slp_debug("Chip_pm_prepare\n");

#if 0
	if (slp_chk_golden)
		Golden_Setting_Compare_for_Suspend();
#endif
	return 0;
}

static int slp_suspend_ops_enter(suspend_state_t state)
{
	/* legacy log */
	slp_debug("Chip_pm_enter\n");

#if 0
	if (slp_dump_gpio)
		gpio_dump_regs();
#endif
#if 1
	if (slp_chk_golden)
		Golden_Setting_Compare_for_Suspend();
#endif
	if (slp_dump_regs)
		slp_dump_pm_regs();

#if 0
	if (!spm_cpusys_can_power_down()) {
		slp_error("CANNOT SLEEP DUE TO CPU1/2/3 PON\n");
		return -EPERM;
	}
#endif

	if (slp_infra_pdn && !slp_cpu_pdn) {
		slp_error("CANNOT SLEEP DUE TO INFRA PDN BUT CPU PON\n");
		return -EPERM;
	}
#if SLP_SLEEP_DPIDLE_EN
	if (slp_ck26m_on)
		slp_wake_reason = spm_go_to_sleep_dpidle(slp_cpu_pdn, 0);
	else
#endif
		slp_wake_reason = spm_go_to_sleep(slp_cpu_pdn, slp_infra_pdn);

	success_flag = 1;

#if defined(CONFIG_AMAZON_METRICS_LOG)
	suspend_success = success_flag;
	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		snprintf(metrics_buf_offmode, sizeof(metrics_buf_offmode),
			"system_resume:off_mode:%s=1;CT;1:NR",
			suspend_success == 1 ? "success" : "fail");
		schedule_work(&metrics_work_offmode);
		break;
	}
#endif /* CONFIG_AMAZON_METRICS_LOG	*/
	return 0;
}

static void slp_suspend_ops_finish(void)
{
	/* legacy log */
	slp_debug("Chip_pm_finish\n");

	/* debug help */
	/* slp_notice("Battery_Voltage = %lu\n", BAT_Get_Battery_Voltage(0)); */
}

static void slp_suspend_ops_end(void)
{
	/* legacy log */
	slp_debug("Chip_pm_end\n");

	/* add wakelock before system performance optimized */
	if (success_flag == 1) {
		wake_lock_timeout(&sleep_wake_lock, HZ * 2);
		success_flag = 0;
	}
}

static const struct platform_suspend_ops slp_suspend_ops = {
	.valid = slp_suspend_ops_valid,
	.begin = slp_suspend_ops_begin,
	.prepare = slp_suspend_ops_prepare,
	.enter = slp_suspend_ops_enter,
	.finish = slp_suspend_ops_finish,
	.end = slp_suspend_ops_end,
};

/*
 * wakesrc : WAKE_SRC_XXX
 * enable  : enable or disable @wakesrc
 * ck26m_on: if true, mean @wakesrc needs 26M to work
 */
int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on)
{
	int r;
	unsigned long flags;

	slp_notice("wakesrc = 0x%x, enable = %u, ck26m_on = %u\n", wakesrc, enable, ck26m_on);

#if SLP_REPLACE_DEF_WAKESRC
	if (wakesrc & WAKE_SRC_CFG_KEY)
#else
	if (!(wakesrc & WAKE_SRC_CFG_KEY))
#endif
		return -EPERM;

	spin_lock_irqsave(&slp_lock, flags);
#if SLP_REPLACE_DEF_WAKESRC
	r = spm_set_sleep_wakesrc(wakesrc, enable, true);
#else
	r = spm_set_sleep_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY, enable, false);
#endif

	if (!r)
		slp_ck26m_on = ck26m_on;
	spin_unlock_irqrestore(&slp_lock, flags);

	return r;
}

wake_reason_t slp_get_wake_reason(void)
{
	return slp_wake_reason;
}

bool slp_will_infra_pdn(void)
{
	return slp_infra_pdn;
}

void __init slp_module_init(void)
{
	spm_fs_init();

	slp_notice("Sleep parameters for testing, init OK..\n");

	spm_output_sleep_option();

	slp_notice("SLEEP_DPIDLE_EN:%d, REPLACE_DEF_WAKESRC:%d, SUSPEND_LOG_EN:%d\n",
		   SLP_SLEEP_DPIDLE_EN, SLP_REPLACE_DEF_WAKESRC, SLP_SUSPEND_LOG_EN);

	suspend_set_ops(&slp_suspend_ops);

#if SLP_SUSPEND_LOG_EN
	console_suspend_enabled = 0;
#endif

	wake_lock_init(&sleep_wake_lock, WAKE_LOCK_SUSPEND, "sleep wakelock");

#if defined(CONFIG_AMAZON_METRICS_LOG)
	INIT_WORK(&metrics_work_offmode, wokeup_metrics_offmode);
#endif /* CONFIG_AMAZON_METRICS_LOG	*/
}

module_param(slp_ck26m_on, bool, 0644);

module_param(slp_cpu_pdn, bool, 0644);
module_param(slp_infra_pdn, bool, 0644);

module_param(slp_chk_golden, bool, 0644);
module_param(slp_dump_gpio, bool, 0644);
module_param(slp_dump_regs, bool, 0644);

module_param(slp_pcm_timer, int, 0644);

MODULE_DESCRIPTION("MT8135 SPM-Sleep Driver v0.9");
