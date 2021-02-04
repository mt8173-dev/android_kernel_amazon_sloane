/*
 *  mtk_ts_all_ts.c - MTK ALL TS thermal zone driver.
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include "mach/pmic_sw.h"

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define MTKTSALLTS_TEMP_CRIT 117000	/* 117.000 degree Celsius */
#define NUM_MTKTSALLTS 4
static DEFINE_MUTEX(therm_lock);

struct mtktsallts_thermal_zone {
	struct thermal_zone_device *tz[NUM_MTKTSALLTS];
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

static int tsallts_get_ts1_temp(void)
{
	int t_ret = 0;
	t_ret = get_immediate_ts2_wrap();
	return t_ret;
}

static int tsallts_get_ts2_temp(void)
{
	int t_ret = 0;
	t_ret = get_immediate_ts2_wrap();
	return t_ret;
}

static int tsallts_get_ts3_temp(void)
{
	int t_ret = 0;
	t_ret = get_immediate_ts3_wrap();
	return t_ret;
}

static int tsallts_get_ts4_temp(void)
{
	int t_ret = 0;
	t_ret = get_immediate_ts4_wrap();
	return t_ret;
}

static int mtktsallts1_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int curr_temp = tsallts_get_ts1_temp();
	if (curr_temp < -30000)
		pr_err("[Power/ALLTS_Thermal] TS4 T=%d\n", curr_temp);
	*t = (unsigned long) curr_temp;
	return 0;
}
static int mtktsallts2_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int curr_temp = tsallts_get_ts2_temp();
	if (curr_temp < -30000)
		pr_err("[Power/ALLTS_Thermal] TS4 T=%d\n", curr_temp);
	*t = (unsigned long) curr_temp;
	return 0;
}
static int mtktsallts3_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int curr_temp = tsallts_get_ts3_temp();
	if (curr_temp < -30000)
		pr_err("[Power/ALLTS_Thermal] TS4 T=%d\n", curr_temp);
	*t = (unsigned long) curr_temp;
	return 0;
}
static int mtktsallts4_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int curr_temp = tsallts_get_ts4_temp();
	if (curr_temp < -30000)
		pr_err("[Power/ALLTS_Thermal] TS4 T=%d\n", curr_temp);
	*t = (unsigned long) curr_temp;
	return 0;
}
static int mtktsallts_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}
static int mtktsallts_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktsallts_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}
static int mtktsallts_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}
static int mtktsallts_set_trip_temp(struct thermal_zone_device *thermal,
				    int trip,
				    unsigned long t)
{
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}
static int mtktsallts_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktsallts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	for (i = 0; i < pdata->num_trips; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*t = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static struct thermal_zone_device_ops mtktsallts_dev_ops[NUM_MTKTSALLTS] = {
	{
		.get_temp = mtktsallts1_get_temp,
		.get_mode = mtktsallts_get_mode,
		.set_mode = mtktsallts_set_mode,
		.get_trip_type = mtktsallts_get_trip_type,
		.get_trip_temp = mtktsallts_get_trip_temp,
		.set_trip_temp = mtktsallts_set_trip_temp,
		.get_crit_temp = mtktsallts_get_crit_temp,
	},
	{
		.get_temp = mtktsallts2_get_temp,
		.get_mode = mtktsallts_get_mode,
		.set_mode = mtktsallts_set_mode,
		.get_trip_type = mtktsallts_get_trip_type,
		.get_trip_temp = mtktsallts_get_trip_temp,
		.set_trip_temp = mtktsallts_set_trip_temp,
		.get_crit_temp = mtktsallts_get_crit_temp,
	},
	{
		.get_temp = mtktsallts3_get_temp,
		.get_mode = mtktsallts_get_mode,
		.set_mode = mtktsallts_set_mode,
		.get_trip_type = mtktsallts_get_trip_type,
		.get_trip_temp = mtktsallts_get_trip_temp,
		.set_trip_temp = mtktsallts_set_trip_temp,
		.get_crit_temp = mtktsallts_get_crit_temp,
	},
	{
		.get_temp = mtktsallts4_get_temp,
		.get_mode = mtktsallts_get_mode,
		.set_mode = mtktsallts_set_mode,
		.get_trip_type = mtktsallts_get_trip_type,
		.get_trip_temp = mtktsallts_get_trip_temp,
		.set_trip_temp = mtktsallts_set_trip_temp,
		.get_crit_temp = mtktsallts_get_crit_temp,
	},
};

static void mtktsallts_work(struct work_struct *work)
{
	struct mtktsallts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;
	int i;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktsallts_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		for (i = 0; i < NUM_MTKTSALLTS; i++)
			thermal_zone_device_update(tzone->tz[i]);
	mutex_unlock(&therm_lock);
}

static struct mtk_thermal_platform_data mtktsallts_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {
		.temp = MTKTSALLTS_TEMP_CRIT,
		.type = THERMAL_TRIP_CRITICAL,
				.hyst = 0
	},
};

static int mtktsallts_probe(struct platform_device *pdev)
{
	int i = 0;
	struct mtktsallts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktsallts_thermal_data;
	char type[THERMAL_NAME_LENGTH];

	if (!pdata)
		return -EINVAL;
	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;
	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;

	for (i = 0; i < NUM_MTKTSALLTS; i++) {
		snprintf(type, THERMAL_NAME_LENGTH, "mtktsallts%d", i+1);
		tzone->tz[i] = thermal_zone_device_register(type,
							    pdata->num_trips,
							    1,
							    tzone,
							    &mtktsallts_dev_ops[i],
							    NULL,
							    0,
							    pdata->polling_delay);
		if (IS_ERR(tzone->tz)) {
			pr_err("%s Failed to register mtktsallts thermal zone device\n", __func__);
			return -EINVAL;
		}
	}

	INIT_WORK(&tzone->therm_work, mtktsallts_work);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktsallts_remove(struct platform_device *pdev)
{
	struct mtktsallts_thermal_zone *tzone = platform_get_drvdata(pdev);
	int i = 0;
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		for (i = 0; i < NUM_MTKTSALLTS; i++)
			if (tzone->tz[i])
				thermal_zone_device_unregister(tzone->tz[i]);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktsallts_driver = {
	.probe = mtktsallts_probe,
	.remove = mtktsallts_remove,
	.driver     = {
		.name  = "mtktsallts",
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktsallts_device = {
	.name = "mtktsallts",
	.id = -1,
	.dev = {
		.platform_data = &mtktsallts_thermal_data,
	},
};

static int __init mtktsallts_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktsallts_driver);
	if (ret) {
		pr_err("Unable to register mtktsallts thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktsallts_device);
	if (ret) {
		pr_err("Unable to register mtktsallts device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktsallts_exit(void)
{
	platform_driver_unregister(&mtktsallts_driver);
	platform_device_unregister(&mtktsallts_device);
}


module_init(mtktsallts_init);
module_exit(mtktsallts_exit);
