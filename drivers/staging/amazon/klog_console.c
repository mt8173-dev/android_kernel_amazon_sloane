/*
 * klog_console.c
 *
 * Copyright (C) 2014 Amazon.com, Inc. or its Affiliates. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/klog_console.h>

static void klog_console_write(struct console *con, const char *buf,
				unsigned count)
{
	if (count == 0)
		return;

	while (count) {
		unsigned int i;
		unsigned int lf;
		/* search for LF so we can insert CR if necessary */
		for (i = 0, lf = 0 ; i < count ; i++) {
			if (*(buf + i) == 10) {
				lf = 1;
				i++;
				break;
			}
		}

		logger_kmsg_write(buf, i);

		if (lf) {
			/* append CR after LF */
			unsigned char cr = 13;
			logger_kmsg_write(&cr, 1);
		}
		buf += i;
		count -= i;
	}

	return;
}

static struct console klog_console = {
	.name	= "klog",
	.write	= klog_console_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};


static int __init klog_console_init(void)
{
	printk(KERN_ERR "Registering kernel log console\n");
	register_console(&klog_console);
	return 0;
}

device_initcall(klog_console_init);
