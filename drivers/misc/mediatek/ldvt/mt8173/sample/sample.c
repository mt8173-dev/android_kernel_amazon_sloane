/*
 * Copyright (C) 2010 MediaTek, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>

static int __init sample_init(void)
{
	return 0;
}

/* should never be called */
static void __exit sample_exit(void)
{
}
module_init(sample_init);
module_exit(sample_exit);

MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("sample driver");
MODULE_LICENSE("GPL");
