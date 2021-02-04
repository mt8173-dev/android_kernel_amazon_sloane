#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <mach/upmu_sw.h>
#include <mach/mt_cpufreq.h>
#include <linux/power/mt_charging.h>
#include <linux/power/mt_battery_common.h>
#include <linux/power/mt_battery_meter.h>
#include "mt_battery_throttle.h"

static struct delayed_work battery_work;
static int bat_protect_level = LOW_BATTERY_LEVEL_0;
static int throttle_mode = TH_NORMAL;
static int test_budget;

static void lbat_protection_powerlimit(LOW_BATTERY_LEVEL level)
{
	bat_protect_level = level;
	switch (level) {
	case LOW_BATTERY_LEVEL_1:
		break;
	case LOW_BATTERY_LEVEL_2:
		mt_cpufreq_thermal_protect(BATTERY_MIN_BUDGET, BATTERY_LIMITOR);
		break;
	default:
		break;
	}
}

static int calculate_battery_budget(void)
{
	int bat_ocv = 0, budget, bat_r, ilim;
	static int vbat_limit = VBAT_LOWER_BOUND;
	unsigned int gpu_power = 0;

	bat_ocv = battery_meter_get_battery_zcv();
	bat_r = battery_meter_get_batteryR();

	/* wait valid zcv from battery meter */
	while (bat_ocv == 0) {
		msleep(100);
		bat_ocv = battery_meter_get_battery_zcv();
		bat_r = battery_meter_get_batteryR();
	}

	if (bat_ocv <= vbat_limit)
		return BATTERY_MIN_BUDGET;

	ilim = (bat_ocv - vbat_limit)*1000/bat_r;
	budget = ilim * vbat_limit / 1000;

	mtk_get_gpu_power_loading(&gpu_power);
	if (budget > gpu_power)
		budget -= gpu_power;

	if (budget < BATTERY_MIN_BUDGET)
		budget = BATTERY_MIN_BUDGET;

	return budget;
}

static void mt_battery_budget_check(struct work_struct *work)
{
	static int power_budget_factor = -1;
	static int power_budget = BATTERY_MAX_BUDGET;
	int bat_vol = 0, bat_avg = 0;
	int max_budget;

	bat_vol = battery_meter_get_battery_voltage_cached();
	bat_avg = get_bat_average_voltage();

	max_budget = calculate_battery_budget();

	/* init budget */
	if (power_budget_factor == -1) {
		power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
		power_budget = max_budget;

	/* no limit when there is power source */
	} else if (upmu_is_chr_det() == KAL_TRUE) {

		power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
		power_budget = BATTERY_MAX_BUDGET;

	/* cut to half budget first if throttle is needed */
	} else if (power_budget_factor == BATTERY_MAX_BUDGET_FACTOR) {

		if (bat_vol > BATTERY_BUDGET_MIN_VOLTAGE)
			power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
		else
			power_budget_factor = (BATTERY_MAX_BUDGET_FACTOR + BATTERY_MIN_BUDGET_FACTOR) / 2;

	/* increase/decrese budget according to voltage level */
	} else {

		if (bat_vol > BATTERY_BUDGET_MIN_VOLTAGE) {
			if (bat_vol - BATTERY_BUDGET_MIN_VOLTAGE < BATTERY_BUDGET_TOLERANCE_VOLTAGE ||
				bat_avg < BATTERY_BUDGET_MIN_VOLTAGE)
				power_budget_factor = power_budget_factor;
			else
				power_budget_factor++;
		} else
			power_budget_factor--;
	}

	if (power_budget_factor > BATTERY_MAX_BUDGET_FACTOR)
		power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
	if (power_budget_factor < BATTERY_MIN_BUDGET_FACTOR)
		power_budget_factor = BATTERY_MIN_BUDGET_FACTOR;

	if (power_budget != BATTERY_MAX_BUDGET) {
		power_budget = BATTERY_MIN_BUDGET +
			power_budget_factor * (max_budget - BATTERY_MIN_BUDGET) / BATTERY_MAX_BUDGET_FACTOR;
	} else {
		power_budget = 0;
	}

	if (throttle_mode == TH_BUDGET)
		power_budget = test_budget;
	else if (throttle_mode == TH_DISABLE) {
		power_budget = 0;
		power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
	} else if (bat_protect_level == LOW_BATTERY_LEVEL_2)
		power_budget = BATTERY_MIN_BUDGET;

	mt_cpufreq_thermal_protect(power_budget, BATTERY_LIMITOR);

	schedule_delayed_work(&battery_work, msecs_to_jiffies(WORK_INTERVAL));
}

static ssize_t show_battery_throttle_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", throttle_mode);
}

static ssize_t store_battery_throttle_mode(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t size)
{
	char *pvalue = NULL;
	unsigned int mode = 0;
	unsigned int budget = 0;

	if (buf != NULL && size != 0) {
		mode = simple_strtoul(buf, &pvalue, 16);
		budget = simple_strtoul((pvalue + 1), NULL, 16);
		pr_notice("set battery throttle mode %d, test budget %d\n", mode, budget);
		throttle_mode = mode;
		test_budget = budget;
	}
	return size;
}

static DEVICE_ATTR(battery_throttle_mode, S_IRUSR | S_IWUSR, show_battery_throttle_mode, store_battery_throttle_mode);

static int battery_probe(struct platform_device *pdev)
{
	device_create_file(&(pdev->dev), &dev_attr_battery_throttle_mode);

	INIT_DELAYED_WORK(&battery_work, mt_battery_budget_check);
	schedule_delayed_work(&battery_work, msecs_to_jiffies(1000));

#if defined(CONFIG_MTK_BATTERY_PROTECT)
	register_low_battery_notify(&lbat_protection_powerlimit, LOW_BATTERY_PRIO_CPU_L);
#endif
	return 0;
}

static int battery_remove(struct platform_device *dev)
{
	cancel_delayed_work_sync(&battery_work);
	return 0;
}

static void battery_shutdown(struct platform_device *pdev)
{
	cancel_delayed_work_sync(&battery_work);
}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	cancel_delayed_work_sync(&battery_work);
	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	schedule_delayed_work(&battery_work, msecs_to_jiffies(WORK_INTERVAL));
	return 0;
}

struct platform_device battery_throttle_device = {
	.name = "battery_throttle",
	.id = -1,
};

static struct platform_driver battery_throttle_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	.suspend = battery_suspend,
	.resume = battery_resume,
	.driver = {
		   .name = "battery_throttle",
		   },
};

static int __init battery_throttle_init(void)
{
	int ret;

	ret = platform_device_register(&battery_throttle_device);
	if (ret) {
		pr_err("[battery_throttle] Unable to device register(%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&battery_throttle_driver);
	if (ret) {
		pr_err("[battery_throttle] Unable to register driver (%d)\n", ret);
		return ret;
	}

	pr_info("[battery_throttle] Initialization : DONE !!\n");
	return 0;
}

static void __exit battery_throttle_exit(void)
{

}

late_initcall(battery_throttle_init);
module_exit(battery_throttle_exit);
