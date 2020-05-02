/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <linux/ieee80211.h>
#include <linux/netdevice.h>
#include "ozpd.h"
#include "ozproto.h"
#include "ozcdev.h"
#include "oztrace.h"


/*------------------------------------------------------------------------------
 * The name of the 802.11 mac device. Empty string is the default value but a
 * value can be supplied as a parameter to the module. An empty string means
 * bind to nothing. '*' means bind to all netcards - this includes non-802.11
 * netcards. Bindings can be added later using an IOCTL.
 */
char *g_net_dev = "";
int is_registered = 0;


/*
 * Register ozwpan when a p2p-p2p0-* interface is up.
 */
static int ozwpan_register(void)
{
	if (!is_registered) {
		printk("OZWPAN: register\n");

		if (oz_protocol_init(g_net_dev))
			return -1;

		is_registered = 1;
		oz_cdev_register();
		oz_app_enable(OZ_APPID_USB, 1);
		oz_apps_init();
		printk(KERN_DEBUG "p->oz_protocol_init = 0x%p\n", oz_protocol_init);
	}

	return 0;
}


/*
 * Deregister ozwpan when a p2p-p2p0-* interface is down.
 */
static int ozwpan_deregister(void)
{
	if (is_registered) {
		is_registered = 0;
		printk("OZWPAN: deregister\n");
		oz_protocol_term();
		oz_apps_term();
		oz_cdev_deregister();
	}

	return 0;
}


/*
 * A listener for changes of status of p2p-p2p0-*.
 */
static int ozwpan_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

	printk("OZWPAN: receive %lu from %s\n", event, dev->name);
	if (strcmp(dev->name, g_net_dev) == 0) {
		switch (event) {
		case NETDEV_UP:
			ozwpan_register();
			break;

		case NETDEV_GOING_DOWN:
			ozwpan_deregister();
			break;

		default:
			/* ignore */
			break;
		}

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}


static struct notifier_block ozwpan_netdev_notifier = {
	.notifier_call = ozwpan_netdev_event,
};

/*------------------------------------------------------------------------------
 * Context: process
 */
static int __init ozwpan_init(void)
{
	/* init ozwpan driver */
	if (ozwpan_register())
		return -1;

	/* register netdev event */
	register_netdevice_notifier(&ozwpan_netdev_notifier);

	return 0;
}

/*------------------------------------------------------------------------------
 * Context: process
 */
static void __exit ozwpan_exit(void)
{
	/* deregister ozwpan first before exit */
	if (is_registered) {
		ozwpan_deregister();
	}

	/* unregister netdev event */
	unregister_netdevice_notifier(&ozwpan_netdev_notifier);
}


/*------------------------------------------------------------------------------
 */
module_param(g_net_dev, charp, S_IRUGO);
module_init(ozwpan_init);
module_exit(ozwpan_exit);

MODULE_AUTHOR("Chris Kelly");
MODULE_DESCRIPTION("Ozmo Devices USB over WiFi hcd driver");
MODULE_VERSION("10.00.01.02.12");
MODULE_LICENSE("GPL");

