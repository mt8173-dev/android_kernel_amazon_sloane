/*
 * mtk_gpufreq_cooling.c - MTK gpufreq works as cooling device.
 *
 * Copyright (C) 2015 Amazon, Inc. or its Affiliates. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <thermal_core.h>

#include <mach/mt_gpufreq.h>

#define MAX_STATE 4

/**
 * struct gpufreq_cooling_device - data for cooling device with gpufreq
 * @cdev: thermal_cooling_device pointer to keep track of the
 *registered cooling device.
 * @gpufreq_state: integer value representing the current state of gpufreq
 *cooling devices.
 * This structure is required for keeping information of each
 * gpufreq_cooling_device registered.
 */

struct gpufreq_cooling_device {
	struct thermal_cooling_device *cdev;
	unsigned int gpufreq_state;
};

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_instance *instance;
	int lfreq, ufreq;
	int offset = 0;
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);

	if (!cdev)
		return -EINVAL;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		lfreq = mt_gpufreq_get_frequency_by_level(instance->lower);
		ufreq = mt_gpufreq_get_frequency_by_level(instance->upper);
		offset += sprintf(buf + offset, "state=%d upper=%d lower=%d\n", instance->trip, ufreq, lfreq);
	}
	mutex_unlock(&cdev->lock);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	unsigned int freq, trip, prev_trip;
	struct thermal_instance *instance;
	struct thermal_instance *prev_instance = NULL;
	unsigned long level;

	if (!cdev)
		return -EINVAL;

	if (sscanf(buf, "%d %d\n", &trip, &freq) != 2)
		return -EINVAL;
	if (trip >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	prev_trip = trip ? trip-1 : trip;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (instance->trip == prev_trip)
			prev_instance = instance;
		if (instance->trip != trip)
			continue;
		level = mt_gpufreq_get_level_by_frequency(freq);
		if (level == THERMAL_CSTATE_INVALID)
			return -EINVAL;
		instance->upper = level;
		if (trip == 0)
			instance->lower = 0;
		else {
			instance->lower = instance->upper ? prev_instance->upper : instance->upper;
			if (instance->lower > instance->upper)
				instance->lower = instance->upper;
		}
	}
	mutex_unlock(&cdev->lock);
	return count;
}

static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

/**
 * gpufreq_apply_cooling - function to apply frequency clipping.
 * @gpufreq_device: gpufreq_cooling_device pointer containing frequency
 *clipping data.
 * @cooling_state: value of the cooling state.
 *
 * Function used to make sure the gpufreq layer is aware of current thermal
 * limits. The limits are applied by updating the gpufreq policy.
 *
 * Return: 0 on success, an error code otherwise (-EINVAL in case wrong
 * cooling state).
 */
static int gpufreq_apply_cooling(struct gpufreq_cooling_device *gpufreq_device,
				 unsigned long cooling_state)
{
	struct mt_gpufreq_power_table_info *table = NULL;
	unsigned int power;

	/* Check if the old cooling action is same as new cooling action */
	if (gpufreq_device->gpufreq_state == cooling_state)
		return 0;

	table = gpufreq_power_get_table();
	if (!table) {
		pr_err("Failed to get gpufreq power table\n");
		return 0;
	}
	power = table[cooling_state].gpufreq_power;
	mt_gpufreq_thermal_protect(power);
	gpufreq_device->gpufreq_state = cooling_state;
	return 0;
}

/* gpufreq cooling device callback functions are defined below */
/**
 * gpufreq_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the gpufreq
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int gpufreq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = MAX_STATE;
	return 0;
}

/**
 * gpufreq_get_cur_state - callback function to get the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the gpufreq
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int gpufreq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct gpufreq_cooling_device *gpufreq_device = cdev->devdata;
	*state = gpufreq_device->gpufreq_state;
	return 0;
}

/**
 * gpufreq_set_cur_state - callback function to set the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the gpufreq
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int gpufreq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct gpufreq_cooling_device *gpufreq_device = cdev->devdata;
	return gpufreq_apply_cooling(gpufreq_device, state);
}

/* Bind gpufreq callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const gpufreq_cooling_ops = {
	.get_max_state = gpufreq_get_max_state,
	.get_cur_state = gpufreq_get_cur_state,
	.set_cur_state = gpufreq_set_cur_state,
};

static int mtk_gpufreq_cooling_probe(struct platform_device *pdev)
{
	struct gpufreq_cooling_device *gpufreq_dev = NULL;

	gpufreq_dev = kzalloc(sizeof(struct gpufreq_cooling_device),
			      GFP_KERNEL);
	if (!gpufreq_dev)
		return -ENOMEM;
	gpufreq_dev->cdev = thermal_cooling_device_register("thermal-gpufreq",
							    gpufreq_dev,
							    &gpufreq_cooling_ops);

	if (IS_ERR(gpufreq_dev->cdev)) {
		dev_err(&pdev->dev, "Failed to register gpufreq cooling device\n");
		return PTR_ERR(gpufreq_dev->cdev);
	}
	device_create_file(&gpufreq_dev->cdev->device, &dev_attr_levels);
	platform_set_drvdata(pdev, gpufreq_dev);
	return 0;
}

static int mtk_gpufreq_cooling_remove(struct platform_device *pdev)
{
	struct gpufreq_cooling_device *gpufreq_dev = platform_get_drvdata(pdev);
	thermal_cooling_device_unregister(gpufreq_dev->cdev);
	return 0;
}

static int mtk_gpufreq_cooling_suspend(struct platform_device *pdev,
				       pm_message_t state)
{
	return 0;
}

static int mtk_gpufreq_cooling_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mtk_gpufreq_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mtk-gpufreq-cooling",
	},
	.probe = mtk_gpufreq_cooling_probe,
	.suspend = mtk_gpufreq_cooling_suspend,
	.resume = mtk_gpufreq_cooling_resume,
	.remove = mtk_gpufreq_cooling_remove,
};

static struct platform_device mtk_gpufreq_cooling_device = {
	.name = "mtk-gpufreq-cooling",
	.id = -1,
};

static int __init mtk_gpufreq_cooling_init(void)
{
	int ret;
	ret = platform_driver_register(&mtk_gpufreq_cooling_driver);
	if (ret) {
		pr_err("Unable to register mtk_gpufreq_cooling cooling driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtk_gpufreq_cooling_device);
	if (ret) {
		pr_err("Unable to register mtk_gpufreq_cooling cooling device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtk_gpufreq_cooling_exit(void)
{
	platform_driver_unregister(&mtk_gpufreq_cooling_driver);
	platform_device_unregister(&mtk_gpufreq_cooling_device);
}

module_init(mtk_gpufreq_cooling_init);
module_exit(mtk_gpufreq_cooling_exit);

MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_DESCRIPTION("MTK gpufreq cooling driver");
MODULE_LICENSE("GPL");
