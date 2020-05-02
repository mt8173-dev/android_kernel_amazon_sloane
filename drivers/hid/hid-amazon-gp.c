/*
 *  HID driver for Amazon.com game controllers
 *  Copyright 2012-2014 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

#define map_led(c)	hid_map_usage(hidinput, usage, bit, max, EV_LED, (c))

static int amazon_input_mapping(struct hid_device *hdev,
	struct hid_input *hidinput, struct hid_field *field,
	struct hid_usage *usage, unsigned long **bit, int *max)
{
	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_LED)
		return 0;

	switch (usage->hid & 0xff) {
	case 0x00:
		map_led(LED_NUML);
		break;
	case 0x01:
		map_led(LED_CAPSL);
		break;
	case 0x02:
		map_led(LED_SCROLLL);
		break;
	case 0x03:
		map_led(LED_COMPOSE);
		break;
	default:
		return 0;
	}
	return 1;
}

static const struct hid_device_id amazon_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_AMAZON, USB_DEVICE_ID_AMAZON_GAMEPAD_WIFI) },
	{ }
};
MODULE_DEVICE_TABLE(hid, amazon_devices);

static struct hid_driver amazon_driver = {
	.name = "amazon-gamepad",
	.id_table = amazon_devices,
	.input_mapping = amazon_input_mapping,
};

static int __init amazon_init(void)
{
	return hid_register_driver(&amazon_driver);
}

static void __exit amazon_exit(void)
{
	hid_unregister_driver(&amazon_driver);
}

module_init(amazon_init);
module_exit(amazon_exit);
MODULE_LICENSE("Proprietary");

