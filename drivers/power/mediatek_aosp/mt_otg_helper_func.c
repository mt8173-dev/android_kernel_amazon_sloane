#include <linux/xlog.h>
#include <mach/mt_typedefs.h>
#include <linux/power/mt_battery_common.h>

void tbl_charger_otg_vbus(int mode)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[tbl_charger_otg_vbus] mode = %d\n", mode);

	if (mode & 0xFF)
		bat_charger_boost_enable(true);
	else
		bat_charger_boost_enable(false);
}

