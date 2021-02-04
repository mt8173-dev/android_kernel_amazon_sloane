/*
 * virtual_sensor_hotplug_cooling.c - Virtual sensor cpu_hotplug works as cooling device.
 *
 * Copyright 2015 Amazon.com, Inc. or its Affiliates. All rights reserved.
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

#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/cpu_hotplug_cooling.h>
#include <linux/device.h>
#include <linux/thermal.h>
#include <thermal_core.h>

static struct thermal_cooling_device *cdev[NR_CPUS];

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct thermal_instance *instance;
	int offset = 0;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		offset += sprintf(buf + offset,
				  "state=%d upper=%ld lower=%ld\n",
				  instance->trip,
				  instance->upper,
				  instance->lower);
	}
	mutex_unlock(&cdev->lock);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct thermal_instance *instance;
	struct thermal_instance *prev_instance = NULL;
	unsigned int trip, prev_trip, state;

	if (!cdev)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &trip, &state) != 2)
		return -EINVAL;
	if (trip >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	if ((state < 0) || (state > 1))
		return -EINVAL;
	prev_trip = trip ? trip-1 : trip;
	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (instance->trip == prev_trip)
			prev_instance = instance;
		if (instance->trip != trip)
			continue;
		instance->upper = state;
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

static int virtual_sensor_hotplug_cooling_probe(struct platform_device *pdev)
{
	int i;
	int nr_cpus = num_possible_cpus();

	for (i = 1; i < nr_cpus; i++) {
		cdev[i] = cpu_hotplug_cooling_register(i);

		if (IS_ERR(cdev[i])) {
			dev_err(&pdev->dev, "Failed to register cooling device\n");
			return PTR_ERR(cdev[i]);
		}
	}

	platform_set_drvdata(pdev, cdev);

	for (i = 1; i < nr_cpus; i++) {
		device_create_file(&cdev[i]->device, &dev_attr_levels);
		dev_info(&pdev->dev, "Cooling device registered: %s\n",	cdev[i]->type);
	}
	return 0;
}

static int virtual_sensor_hotplug_cooling_remove(struct platform_device *pdev)
{
	int i;
	struct thermal_cooling_device **cdev = platform_get_drvdata(pdev);
	int nr_cpus = num_possible_cpus();

	for (i = 1; i < nr_cpus; i++)
		cpu_hotplug_cooling_unregister(cdev[i]);

	return 0;
}

static struct platform_driver virtual_sensor_hotplug_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "virtual_sensor-cpu-hotplug-cooling",
	},
	.probe = virtual_sensor_hotplug_cooling_probe,
	.remove = virtual_sensor_hotplug_cooling_remove,
};

static struct platform_device virtual_sensor_hotplug_cooling_device = {
	.name = "virtual_sensor-cpu-hotplug-cooling",
	.id = -1,
};

static int __init virtual_sensor_hotplug_cooling_init(void)
{
	int ret;
	ret = platform_driver_register(&virtual_sensor_hotplug_cooling_driver);
	if (ret) {
		pr_err("Unable to register VIRTUAL_SENSOR cpu_hotplug cooling driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&virtual_sensor_hotplug_cooling_device);
	if (ret) {
		pr_err("Unable to register VIRTUAL_SENSOR cpu_hotplug cooling device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit virtual_sensor_hotplug_cooling_exit(void)
{
	platform_driver_unregister(&virtual_sensor_hotplug_cooling_driver);
	platform_device_unregister(&virtual_sensor_hotplug_cooling_device);
}

module_init(virtual_sensor_hotplug_cooling_init);
module_exit(virtual_sensor_hotplug_cooling_exit);

MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_DESCRIPTION("Virtual sensor cpu_hotplug cooling driver");
MODULE_LICENSE("GPL");
