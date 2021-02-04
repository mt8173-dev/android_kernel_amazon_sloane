/*
 *  mtk_bts_bts.c - MTK BTS thermal zone driver.
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

#define MTKTSBTS_TEMP_CRIT 60000        /* 60.000 degree Celsius */
static DEFINE_MUTEX(therm_lock);

struct mtktsbts_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

typedef struct {
	INT32 APTemp;
	INT32 TemperatureR;
} AP_TEMPERATURE;

#define AUX_IN0_NTC (0)

static int g_RAP_pull_up_R = 39000;	/* 39K,pull up resister */
static int g_TAP_over_critical_low = 188500;	/* base on 10K NTC temp default value -40 deg */
static int g_RAP_pull_up_voltage = 1800;	/* 1.8V ,pull up voltage */
static int g_RAP_ntc_table = 4;	/* default is AP_NTC_10 */
static int g_RAP_ADC_channel = AUX_IN0_NTC;	/* from MT6595 REF_SCH */

static int g_AP_TemperatureR;
/* AP_TEMPERATURE AP_Temperature_Table[] = {0}; */

static AP_TEMPERATURE AP_Temperature_Table[] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0}
};

/* AP_NTC_BL197 */
AP_TEMPERATURE AP_Temperature_Table1[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};

/* AP_NTC_TSM_1 */
AP_TEMPERATURE AP_Temperature_Table2[] = {
	{-40, 70603},		/* FIX_ME */
	{-35, 70603},		/* FIX_ME */
	{-30, 70603},		/* FIX_ME */
	{-25, 70603},		/* FIX_ME */
	{-20, 70603},
	{-15, 55183},
	{-10, 43499},
	{-5, 34569},
	{0, 27680},
	{5, 22316},
	{10, 18104},
	{15, 14773},
	{20, 12122},
	{25, 10000},		/* 10K */
	{30, 8294},
	{35, 6915},
	{40, 5795},
	{45, 4882},
	{50, 4133},
	{55, 3516},
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004}		/* FIX_ME */
};

/* AP_NTC_10_SEN_1 */
AP_TEMPERATURE AP_Temperature_Table3[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};

#if 0
/* AP_NTC_10 */
AP_TEMPERATURE AP_Temperature_Table4[] = {
	{-20, 68237},
	{-15, 53650},
	{-10, 42506},
	{-5, 33892},
	{0, 27219},
	{5, 22021},
	{10, 17926},
	{15, 14674},
	{20, 12081},
	{25, 10000},
	{30, 8315},
	{35, 6948},
	{40, 5834},
	{45, 4917},
	{50, 4161},
	{55, 3535},
	{60, 3014}
};
#else
/* AP_NTC_10(TSM0A103F34D1RZ) */
AP_TEMPERATURE AP_Temperature_Table4[] = {
	{-40, 188500},
	{-35, 144290},
	{-30, 111330},
	{-25, 86560},
	{-20, 67790},
	{-15, 53460},
	{-10, 42450},
	{-5, 33930},
	{0, 27280},
	{5, 22070},
	{10, 17960},
	{15, 14700},
	{20, 12090},
	{25, 10000},		/* 10K */
	{30, 8310},
	{35, 6940},
	{40, 5830},
	{45, 4910},
	{50, 4160},
	{55, 3540},
	{60, 3020},
	{65, 2590},
	{70, 2230},
	{75, 1920},
	{80, 1670},
	{85, 1450},
	{90, 1270},
	{95, 1110},
	{100, 975},
	{105, 860},
	{110, 760},
	{115, 674},
	{120, 599},
	{125, 534}
};
#endif

/* AP_NTC_47 */
AP_TEMPERATURE AP_Temperature_Table5[] = {
	{-40, 483954},		/* FIX_ME */
	{-35, 483954},		/* FIX_ME */
	{-30, 483954},		/* FIX_ME */
	{-25, 483954},		/* FIX_ME */
	{-20, 483954},
	{-15, 360850},
	{-10, 271697},
	{-5, 206463},
	{0, 158214},
	{5, 122259},
	{10, 95227},
	{15, 74730},
	{20, 59065},
	{25, 47000},		/* 47K */
	{30, 37643},
	{35, 30334},
	{40, 24591},
	{45, 20048},
	{50, 16433},
	{55, 13539},
	{60, 11210},
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210}		/* FIX_ME */
};

/* NTCG104EF104F(100K) */
AP_TEMPERATURE AP_Temperature_Table6[] = {
	{-40, 4251000},
	{-35, 3005000},
	{-30, 2149000},
	{-25, 1554000},
	{-20, 1135000},
	{-15, 837800},
	{-10, 624100},
	{-5, 469100},
	{0, 355600},
	{5, 271800},
	{10, 209400},
	{15, 162500},
	{20, 127000},
	{25, 100000},		/* 100K */
	{30, 79230},
	{35, 63180},
	{40, 50680},
	{45, 40900},
	{50, 33190},
	{55, 27090},
	{60, 22220},
	{65, 18320},
	{70, 15180},
	{75, 12640},
	{80, 10580},
	{85, 8887},
	{90, 7500},
	{95, 6357},
	{100, 5410},
	{105, 4623},
	{110, 3965},
	{115, 3415},
	{120, 2951},
	{125, 2560}
};

/* convert register to temperature  */
static INT16 APtThermistorConverTemp(INT32 Res)
{
	int i = 0;
	int asize = 0;
	INT32 RES1 = 0, RES2 = 0;
	INT32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;

	asize = (sizeof(AP_Temperature_Table) / sizeof(AP_TEMPERATURE));
	if (Res >= AP_Temperature_Table[0].TemperatureR)
		TAP_Value = -40;	/* min */
	else if (Res <= AP_Temperature_Table[asize - 1].TemperatureR)
		TAP_Value = 125;	/* max */
	else {
		RES1 = AP_Temperature_Table[0].TemperatureR;
		TMP1 = AP_Temperature_Table[0].APTemp;

		for (i = 0; i < asize; i++) {
			if (Res >= AP_Temperature_Table[i].TemperatureR) {
				RES2 = AP_Temperature_Table[i].TemperatureR;
				TMP2 = AP_Temperature_Table[i].APTemp;
				break;
			} else {
				RES1 = AP_Temperature_Table[i].TemperatureR;
				TMP1 = AP_Temperature_Table[i].APTemp;
			}
		}

		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}

	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static INT16 APtVoltToTemp(UINT32 dwVolt)
{
	INT32 TRes;
	INT32 dwVCriAP = 0;
	INT32 APTMP = -100;

	/* SW workaround----------------------------------------------------- */
	/* dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) / (TAP_OVER_CRITICAL_LOW + 39000); */
	/* dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) / (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R); */
	dwVCriAP =
		(g_TAP_over_critical_low * g_RAP_pull_up_voltage) / (g_TAP_over_critical_low +
								     g_RAP_pull_up_R);

	if (dwVolt > dwVCriAP)
		TRes = g_TAP_over_critical_low;
	else
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (g_RAP_pull_up_R * dwVolt) / (g_RAP_pull_up_voltage - dwVolt);

	/* ------------------------------------------------------------------ */

	g_AP_TemperatureR = TRes;

	/* convert register to temperature */
	APTMP = APtThermistorConverTemp(TRes);

	return APTMP;
}

static int get_hw_AP_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times = 1, Channel = g_RAP_ADC_channel;	/* 6595=0(AUX_IN0_NTC) */

	if (IMM_IsAdcInitReady() == 0) {
		pr_info("[thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--) {
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		pr_info("[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n", ret_temp);
		pr_info("[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n", ret_temp);
	}

#if 0
	Channel = 0;
	ret = 0;
	ret_temp = 0;
	i = times;
	while (i--) {
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		pr_info("[thermal_auxadc_get_data(ADCIN %d)]: ret_temp=%d\n", Channel, ret_temp);
	}

	Channel = 2;
	ret = 0;
	ret_temp = 0;
	i = times;
	while (i--) {
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		pr_info("[thermal_auxadc_get_data(ADCIN %d)]: ret_temp=%d\n", Channel, ret_temp);
	}
#endif

	/* ret = ret*1500/4096   ; */
	ret = ret * 1800 / 4096;	/* 82's ADC power */
	pr_info("APtery output mV = %d\n", ret);
	output = APtVoltToTemp(ret);
	pr_info("BTS output temperature = %d\n", output);
	return output;
}

static DEFINE_MUTEX(AP_lock);
int ts_AP_at_boot_time = 0;
int mtktsbts_get_hw_temp(void)
{
	int t_ret = 0;
/* static int AP[60]={0}; */
/* int i=0; */

	mutex_lock(&AP_lock);

	/* get HW AP temp (TSAP) */
	/* cat /sys/class/power_supply/AP/AP_temp */
	t_ret = get_hw_AP_temp();
	t_ret = t_ret * 1000;

	mutex_unlock(&AP_lock);

	if (t_ret > 60000)	/* abnormal high temp */
		pr_info("[Power/AP_Thermal] T_AP=%d\n", t_ret);

	pr_info("[mtktsbts_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

static int mtktsbts_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktsbts_get_hw_temp();
	return 0;
}

void mtktsbts_copy_table(AP_TEMPERATURE *des, AP_TEMPERATURE *src)
{
	int i = 0;
	int j = 0;

	j = (sizeof(AP_Temperature_Table) / sizeof(AP_TEMPERATURE));
	for (i = 0; i < j; i++)
		des[i] = src[i];
}

void mtktsbts_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table1);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table1));
		break;
	case 2:		/* AP_NTC_TSM_1 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table2);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table2));
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table3);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table3));
		break;
	case 4:		/* AP_NTC_10 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table4);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table4));
		break;
	case 5:		/* AP_NTC_47 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table5);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table5));
		break;
	case 6:		/* NTCG104EF104F */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table6);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table6));
		break;
	default:		/* AP_NTC_10 */
		mtktsbts_copy_table(AP_Temperature_Table, AP_Temperature_Table4);
		BUG_ON(sizeof(AP_Temperature_Table) != sizeof(AP_Temperature_Table4));
		break;
	}
}

static int mtktsbts_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktsbts_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktsbts_get_trip_type(struct thermal_zone_device *thermal,
				   int trip,
				   enum thermal_trip_type *type)
{
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktsbts_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip,
				   unsigned long *t)
{
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktsbts_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktsbts_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;
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

static struct thermal_zone_device_ops mtktsbts_dev_ops = {
	.get_temp = mtktsbts_get_temp,
	.get_mode = mtktsbts_get_mode,
	.set_mode = mtktsbts_set_mode,
	.get_trip_type = mtktsbts_get_trip_type,
	.get_trip_temp = mtktsbts_get_trip_temp,
	.set_trip_temp = mtktsbts_set_trip_temp,
	.get_crit_temp = mtktsbts_get_crit_temp,
};

static void mtktsbts_work(struct work_struct *work)
{
	struct mtktsbts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktsbts_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtktsbts_read_temp(struct thermal_dev *tdev)
{
	return mtktsbts_get_hw_temp();
}
static struct thermal_dev_ops mtktsbts_fops = {
	.get_temp = mtktsbts_read_temp,
};

struct thermal_dev_params mtktsbts_tdp = {
	.offset = 1,
	.alpha = 1,
	.weight = 0
};

static ssize_t mtktsbts_show_params(struct device *dev,
				    struct device_attribute *devattr,
				    char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;
	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtktsbts_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktsbts_thermal_zone *tzone = thermal->devdata;

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

static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtktsbts_show_params, mtktsbts_store_params);

static struct mtk_thermal_platform_data mtktsbts_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSBTS_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};


static int mtktsbts_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktsbts_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktsbts_thermal_data;

	if (!pdata)
		return -EINVAL;

	mtktsbts_prepare_table(g_RAP_ntc_table);

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktsbts",
						 pdata->num_trips,
						 1,
						 tzone,
						 &mtktsbts_dev_ops,
						 NULL,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktsbts thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtktsbts";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtktsbts_fops;
	tzone->therm_fw->tdp = &mtktsbts_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("Error registering therml mtktsbts device\n");
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktsbts_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktsbts_remove(struct platform_device *pdev)
{
	struct mtktsbts_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktsbts_driver = {
	.probe = mtktsbts_probe,
	.remove = mtktsbts_remove,
	.driver     = {
		.name  = "mtktsbts",
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktsbts_device = {
	.name = "mtktsbts",
	.id = -1,
	.dev = {
		.platform_data = &mtktsbts_thermal_data,
	},
};

static int __init mtktsbts_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktsbts_driver);
	if (ret) {
		pr_err("Unable to register mtktsbts thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktsbts_device);
	if (ret) {
		pr_err("Unable to register mtktsbts device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktsbts_exit(void)
{
	platform_driver_unregister(&mtktsbts_driver);
	platform_device_unregister(&mtktsbts_device);
}

module_init(mtktsbts_init);
module_exit(mtktsbts_exit);
