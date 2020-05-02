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

#include <linux/netdevice.h>

#include <mach/mtk_wcn_cmb_stub.h>

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define MTKTSWMT_TEMP_CRIT 120000
static DEFINE_MUTEX(therm_lock);

struct mtktswmt_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

static int mtktswmt_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int temp = mtk_wcn_cmb_stub_query_ctrl();
	if (temp > 100 || temp < -30)
		return -EINVAL;
	*t = (temp < 0) ? 0 : (unsigned long)temp * 1000;
	return 0;
}

static int mtktswmt_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktswmt_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktswmt_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktswmt_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktswmt_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktswmt_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;
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

static struct thermal_zone_device_ops mtktswmt_dev_ops = {
	.get_temp = mtktswmt_get_temp,
	.get_mode = mtktswmt_get_mode,
	.set_mode = mtktswmt_set_mode,
	.get_trip_type = mtktswmt_get_trip_type,
	.get_trip_temp = mtktswmt_get_trip_temp,
	.set_trip_temp = mtktswmt_set_trip_temp,
	.get_crit_temp = mtktswmt_get_crit_temp,
};

static void mtktswmt_work(struct work_struct *work)
{
	struct mtktswmt_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktswmt_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int mtktswmt_read_temp(struct thermal_dev *tdev)
{
	int temp = mtk_wcn_cmb_stub_query_ctrl();
	if (temp > 100 || temp < -30)
		return -EINVAL;
	temp = (temp < 0) ? 0 : (temp * 1000);
	return temp;
}
static struct thermal_dev_ops mtktswmt_fops = {
	.get_temp = mtktswmt_read_temp,
};

struct thermal_dev_params mtktswmt_tdp = {
	.offset = 1,
	.alpha = 1,
	.weight = 0
};

static ssize_t mtktswmt_show_params(struct device *dev,
				struct device_attribute *devattr,
				char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;

	if (!tzone)
		return -EINVAL;

	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       tzone->therm_fw->tdp->offset,
		       tzone->therm_fw->tdp->alpha,
		       tzone->therm_fw->tdp->weight);
}

static ssize_t mtktswmt_store_params(struct device *dev,
				     struct device_attribute *devattr,
				     const char *buf,
				     size_t count)
{
	char param[20];
	int value = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktswmt_thermal_zone *tzone = thermal->devdata;

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
static DEVICE_ATTR(params, S_IRUGO | S_IWUSR, mtktswmt_show_params, mtktswmt_store_params);

static struct mtk_thermal_platform_data mtktswmt_thermal_data = {
	.num_trips = 1,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.trips[0] = {.temp = MTKTSWMT_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static int mtktswmt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktswmt_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktswmt_thermal_data;

	if (!pdata)
		return -EINVAL;

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktswmt",
							pdata->num_trips,
							1,
							tzone,
							&mtktswmt_dev_ops,
							NULL,
							0,
							pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktswmt thermal zone device\n", __func__);
		return -EINVAL;
	}
	tzone->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (!tzone->therm_fw)
		return -ENOMEM;
	tzone->therm_fw->name = "mtktswmt";
	tzone->therm_fw->dev = &(pdev->dev);
	tzone->therm_fw->dev_ops = &mtktswmt_fops;
	tzone->therm_fw->tdp = &mtktswmt_tdp;

	ret = thermal_dev_register(tzone->therm_fw);
	if (ret) {
		pr_err("Error registering therml mtktswmt device\n");
		return -EINVAL;
	}

	INIT_WORK(&tzone->therm_work, mtktswmt_work);
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktswmt_remove(struct platform_device *pdev)
{
	struct mtktswmt_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver mtktswmt_driver = {
	.probe = mtktswmt_probe,
	.remove = mtktswmt_remove,
	.driver = {
		.name  = "mtktswmt",
		.owner = THIS_MODULE,
	},
};

static struct platform_device mtktswmt_device = {
	.name = "mtktswmt",
	.id = -1,
	.dev = {
		.platform_data = &mtktswmt_thermal_data,
	},
};

static int __init mtktswmt_init(void)
{
	int ret;
	ret = platform_driver_register(&mtktswmt_driver);
	if (ret) {
		pr_err("Unable to register mtktswmt thermal driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtktswmt_device);
	if (ret) {
		pr_err("Unable to register mtktswmt device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtktswmt_exit(void)
{
	platform_driver_unregister(&mtktswmt_driver);
	platform_device_unregister(&mtktswmt_device);
}

late_initcall(mtktswmt_init);
module_exit(mtktswmt_exit);
