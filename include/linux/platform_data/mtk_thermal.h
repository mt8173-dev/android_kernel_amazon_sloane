#ifndef _MTK_THERMAL_H_
#define _MTK_THERMAL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/notifier.h>
#include <linux/leds.h>

struct mtk_cooler_platform_data {
	char type[THERMAL_NAME_LENGTH];
	unsigned long state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int level;
	int levels[THERMAL_MAX_TRIPS];
};

struct cdev_t {
	char type[THERMAL_NAME_LENGTH];
	unsigned long upper;
	unsigned long lower;
};

struct trip_t {
	unsigned long temp;
	enum thermal_trip_type type;
	unsigned long hyst;
	struct cdev_t cdev[THERMAL_MAX_TRIPS];
};

struct mtk_thermal_platform_data {
	int num_trips;
	enum thermal_device_mode mode;
	int polling_delay;
	struct thermal_zone_params tzp;
	struct trip_t trips[THERMAL_MAX_TRIPS];
};

#endif /* _MTK_THERMAL_H_ */
