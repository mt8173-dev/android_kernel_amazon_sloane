/*
 *  mtk_ts_abb.c - MTK ABB thermal zone driver.
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

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define MTK_TS_ABB_SW_FILTER         (1)
#define MTKTSABB_TEMP_CRIT 117000	/* 117.000 degree Celsius */
static DEFINE_MUTEX(therm_lock);

struct mtktsabb_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};


extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);
/* extern int last_abb_t; */
/* extern int last_CPU2_t; */
extern int get_immediate_temp2_wrap(void);
extern void mtkts_dump_cali_info(void);

static int mtktsabb_get_hw_temp(void)
{
	int t_ret = 0;

	t_ret = get_immediate_temp2_wrap();
	return t_ret;
}

static int mtktsabb_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
#if MTK_TS_ABB_SW_FILTER == 1
	int curr_temp;
	int temp_temp;
	int ret = 0;
	static int last_abb_read_temp;

	curr_temp = mtktsabb_get_hw_temp();
	pr_debug("mtktsabb_get_temp TSABB =%d\n", curr_temp);

	if ((curr_temp < -30000) || (curr_temp > 85000))	/* abnormal high temp */
		printk("[Power/ABB_Thermal] ABB T=%d\n", curr_temp);

	temp_temp = curr_temp;
	if (curr_temp != 0) {	/* not the first temp read after resume from suspension */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {	/* invalid range */
			printk("[Power/ABB_Thermal] ABB temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_abb_read_temp != 0) {
			if ((curr_temp - last_abb_read_temp > 20000) || (last_abb_read_temp - curr_temp > 20000)) {	/* delta 20C, invalid change */
				printk
				    ("[Power/ABB_Thermal] ABB temp float hugely temp=%d, lasttemp=%d\n",
				     curr_temp, last_abb_read_temp);
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_abb_read_temp = curr_temp;
	curr_temp = temp_temp;
	*t = (unsigned long)curr_temp;
	return ret;
#else
	int curr_temp;
	curr_temp = mtktsabb_get_hw_temp();
	pr_debug(" mtktsabb_get_temp CPU T2=%d\n", curr_temp);

	if (curr_temp > (curr_temp < -30000))
		printk("[Power/ABB_Thermal] ABB T=%d\n", curr_temp);

	*t = curr_temp;

	return 0;
#endif
}

static int mtktsabb_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktsabb_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktsabb_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktsabb_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktsabb_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktsabb_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktsabb_thermal_zone *tzone = thermal->devdata;
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

static struct thermal_zone_device_ops mtktsabb_dev_ops = {
	.get_temp = mtktsabb_get_temp,
	.get_mode = mtktsabb_get_mode,
	.set_mode = mtktsabb_set_mode,
	.get_trip_type = mtktsabb_get_trip_type,
	.get_trip_temp = mtktsabb_get_trip_temp,
	.set_trip_temp = mtktsabb_set_trip_temp,
	.get_crit_temp = mtktsabb_get_crit_temp,
};

static void mtktsabb_work(struct work_struct *work)
{
	struct mtktsabb_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktsabb_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static struct mtk_thermal_platform_data mtktsabb_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSABB_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static int mtktsabb_probe(struct platform_device *pdev)
{
	struct mtktsabb_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktsabb_thermal_data;

	if (!pdata)
		return -EINVAL;

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktsabb",
						 pdata->num_trips,
						 1,
						 tzone,
						 &mtktsabb_dev_ops,
						 NULL,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktsabb thermal zone device\n", __func__);
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktsabb_work);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktsabb_remove(struct platform_device *pdev)
{
	struct mtktsabb_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktsabb_driver = {
	.probe = mtktsabb_probe,
	.remove = mtktsabb_remove,
	.driver     = {
		.name  = "mtktsabb",
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktsabb_device = {
	.name = "mtktsabb",
	.id = -1,
	.dev = {
		.platform_data = &mtktsabb_thermal_data,
	},
};

static int __init mtktsabb_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktsabb_driver);
	if (ret) {
		pr_err("Unable to register mtktsabb thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktsabb_device);
	if (ret) {
		pr_err("Unable to register mtktsabb device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktsabb_exit(void)
{
	platform_driver_unregister(&mtktsabb_driver);
	platform_device_unregister(&mtktsabb_device);
}


module_init(mtktsabb_init);
module_exit(mtktsabb_exit);
