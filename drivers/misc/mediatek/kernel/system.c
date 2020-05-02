#include <linux/kernel.h>
#include <linux/string.h>
#include <mach/system.h>
#include <mach/mtk_rtc.h>
#include <mach/wd_api.h>
#include <mach/ext_wd_drv.h>
#include <asm/system_misc.h>

void arch_reset(char mode, const char *cmd)
{
	char reboot = 0;
	int res = 0;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	pr_info("arch_reset: cmd = %s\n", cmd ? : "NULL");

	if (cmd && !strcmp(cmd, "charger")) {
		/* do nothing now */
		/* reserved for future use */
	}
#ifdef CONFIG_MTK_RTC
	else if (cmd && !strcmp(cmd, "recovery")) {
		rtc_mark_recovery();
	} else if (cmd && !strcmp(cmd, "bootloader")) {
		rtc_mark_fast();
	} else if (cmd && !strcmp(cmd, "rpmbp")) {
		rtc_mark_rpmbp();
	}
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	else if (cmd && !strcmp(cmd, "kpoc")) {
		rtc_mark_kpoc();
	} else if (cmd && !strcmp(cmd, "enter_kpoc")) {
		rtc_mark_enter_kpoc();
	}
#endif
#endif /* CONFIG_MTK_RTC */
	else
		reboot = 1;

	if (res)
		pr_info("arch_reset, get wd api error %d\n", res);
	else
		wd_api->wd_sw_reset(reboot);
}

static int __init register_restart_poweroff(void)
{
	arm_pm_restart = arch_reset;
	pm_power_off = mt_power_off;
	return 0;
}
early_initcall(register_restart_poweroff);
