/*
 *  mtk_ts_pmic.c - MTK PMIC thermal zone driver.
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

#define MTKTSPMIC_TEMP_CRIT 145000      /* 145.000 degree Celsius */
static DEFINE_MUTEX(therm_lock);

struct mtktspmic_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};


static kal_int32 g_adc_ge;
static kal_int32 g_adc_oe;
static kal_int32 g_o_vts;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;

#define y_pmic_repeat_times	1

static u16 pmic_read(u16 addr)
{
	u32 rdata = 0;
	pwrap_read((u32) addr, &rdata);
	return (u16) rdata;
}

static void pmic_cali_prepare(void)
{
	kal_uint32 temp0, temp1, temp2, sign;

	temp0 = pmic_read(0x1E2);
	temp1 = pmic_read(0x1EA);
	temp2 = pmic_read(0x1EC);

	pr_debug("Power/PMIC_Thermal: Reg(0x1E2)=0x%x, Reg(0x1EA)=0x%x, Reg(0x1EC)=0x%x\n", temp0,
		temp1, temp2);

	g_adc_ge = (temp0 >> 1) & 0x007f;
	g_adc_oe = (temp0 >> 8) & 0x003f;
	g_o_vts = ((temp2 & 0x0001) << 8) + ((temp1 >> 8) & 0x00ff);
	g_degc_cali = (temp1 >> 2) & 0x003f;
	g_adc_cali_en = (temp1 >> 1) & 0x0001;
	g_o_slope_sign = (temp2 >> 1) & 0x0001;
	g_o_slope = (temp2 >> 2) & 0x003f;
	g_id = (temp2 >> 8) & 0x0001;

	sign = (temp0 >> 7) & 0x0001;
	if (sign == 1)
		g_adc_ge = g_adc_ge - 0x80;

	sign = (temp0 >> 13) & 0x0001;
	if (sign == 1)
		g_adc_oe = g_adc_oe - 0x40;

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en != 1) {
		/* no cali, use default vaule */
		g_adc_ge = 0;
		g_adc_oe = 0;
		/* g_o_vts = 608; */
		g_o_vts = 352;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
	}

	pr_debug("Power/PMIC_Thermal: ");
	pr_debug("g_adc_ge = 0x%x, g_adc_oe = 0x%x, g_o_vts = 0x%x, g_degc_cali = 0x%x,",
		g_adc_ge, g_adc_oe, g_o_vts, g_degc_cali);
	pr_debug("g_adc_cali_en = 0x%x, g_o_slope = 0x%x, g_o_slope_sign = 0x%x, g_id = 0x%x\n",
		g_adc_cali_en, g_o_slope, g_o_slope_sign, g_id);
}

static kal_int32 thermal_cal_exec(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	if (ret == 0)
		return 0;

	format_1 = (g_degc_cali * 1000 / 2);
	format_2 = (g_adc_ge + 1024) * (g_o_vts + 256) + g_adc_oe * 1024;
	format_3 = (format_2 * 1200) / 1024 * 100 / 1024;
	pr_debug("format1=%d, format2=%d, format3=%d\n", format_1, format_2, format_3);

	if (g_o_slope_sign == 0) {
		/* format_4 = ((format_3 * 1000) / (164+g_o_slope));//unit = 0.001 degress */
		/* format_4 = (y_curr*1000 - format_3)*100 / (164+g_o_slope); */
		format_4 = (y_curr * 100 - format_3) * 1000 / (171 + g_o_slope);
	} else {
		/* format_4 = ((format_3 * 1000) / (164-g_o_slope)); */
		/* format_4 = (y_curr*1000 - format_3)*100 / (164-g_o_slope); */
		format_4 = (y_curr * 100 - format_3) * 1000 / (171 - g_o_slope);
	}
	format_4 = format_4 - (2 * format_4);
	t_current = (format_1) + format_4;	/* unit = 0.001 degress */
/* pr_info("[mtktspmic_get_hw_temp] T_PMIC=%d\n",t_current); */
	return t_current;
}

static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1 = 0, PMIC_counter;

static int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = PMIC_IMM_GetOneChannelValue(4, y_pmic_repeat_times, 2);
	temp1 = thermal_cal_exec(temp);

	pr_debug("[mtktspmic_get_hw_temp] PMIC_IMM_GetOneChannel 4=%d, T=%d\n",
		temp, temp1);

/* pmic_thermal_dump_reg(); // test */

	if ((temp1 > 100000) || (temp1 < -30000)) {
		pr_info("[Power/PMIC_Thermal] raw=%d, PMIC T=%d", temp, temp1);
/* pmic_thermal_dump_reg(); */
	}

	if ((temp1 > 150000) || (temp1 < -50000)) {
		pr_info("[Power/PMIC_Thermal] drop this data\n");
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0)
		   && (((pre_temp1 - temp1) > 30000) || ((temp1 - pre_temp1) > 30000))) {
		pr_info("[Power/PMIC_Thermal] drop this data 2\n");
		temp1 = pre_temp1;
	} else {
		/* update previous temp */
		pre_temp1 = temp1;
		pr_debug("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}

	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int mtktspmic_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktspmic_get_hw_temp();
	return 0;
}

static int mtktspmic_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktspmic_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktspmic_get_trip_type(struct thermal_zone_device *thermal,
				   int trip,
				   enum thermal_trip_type *type)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktspmic_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long *t)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktspmic_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long t)
{
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktspmic_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;
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

static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.set_trip_temp = mtktspmic_set_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
};

static void mtktspmic_work(struct work_struct *work)
{
	struct mtktspmic_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktspmic_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtktspmic_read_temp(struct thermal_dev *tdev)
{
	return mtktspmic_get_hw_temp();
}
static struct thermal_dev_ops mtktspmic_fops = {
	.get_temp = mtktspmic_read_temp,
};

struct thermal_dev_params mtktspmic_tdp = {
	.offset = 1,
	.alpha = 1,
	.weight = 0
};

static ssize_t mtktspmic_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtktspmic_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktspmic_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	if (sscanf(buf, "%s %d", param, &value) == 2) {
		if (!strcmp(param, "offset"))
			tzone->therm_fw->tdp->offset = value;
		if (!strcmp(param, "alpha"))
			tzone->therm_fw->tdp->alpha = value;
		if (!strcmp(param, "weight"))
			tzone->therm_fw->tdp->weight = value;
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtktspmic_show_params, mtktspmic_store_params);

static struct mtk_thermal_platform_data mtktspmic_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSPMIC_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static int mtktspmic_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktspmic_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktspmic_thermal_data;

	if (!pdata)
		return -EINVAL;

	pmic_cali_prepare();

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktspmic",
						 pdata->num_trips,
						 1,
						 tzone,
						 &mtktspmic_dev_ops,
						 NULL,
							0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktspmic thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtktspmic";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtktspmic_fops;
	tzone->therm_fw->tdp = &mtktspmic_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("Error registering therml mtktspmic device\n");
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktspmic_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktspmic_remove(struct platform_device *pdev)
{
	struct mtktspmic_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktspmic_driver = {
	.probe = mtktspmic_probe,
	.remove = mtktspmic_remove,
	.driver     = {
		.name  = "mtktspmic",
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktspmic_device = {
	.name = "mtktspmic",
	.id = -1,
	.dev = {
		.platform_data = &mtktspmic_thermal_data,
	},
};

static int __init mtktspmic_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktspmic_driver);
	if (ret) {
		pr_err("Unable to register mtktspmic thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktspmic_device);
	if (ret) {
		pr_err("Unable to register mtktspmic device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktspmic_exit(void)
{
	platform_driver_unregister(&mtktspmic_driver);
	platform_device_unregister(&mtktspmic_device);
}

module_init(mtktspmic_init);
module_exit(mtktspmic_exit);
