/*
 * metricslog.h
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its Affiliates. All rights reserved.
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

#ifndef _LINUX_METRICSLOG_H
#define _LINUX_METRICSLOG_H

#ifdef CONFIG_AMAZON_METRICS_LOG

#include <linux/xlog.h>

typedef enum {
	VITALS_NORMAL = 0,
	VITALS_FGTRACKING,
	VITALS_TIME_BUCKET,
} vitals_type;


void log_to_metrics(enum android_log_priority priority,
	const char *domain, char *logmsg);

void log_counter_to_vitals(enum android_log_priority priority,
	const char *domain, const char *program,
	const char *source, const char *key,
	long counter_value, const char *unit,
	const char *metadata, vitals_type type);
void log_timer_to_vitals(enum android_log_priority priority,
	const char *domain, const char *program,
	const char *source, const char *key,
	long timer_value, const char *unit, vitals_type type);
#endif

#ifdef CONFIG_AMAZON_LOG
void log_to_amzmain(enum android_log_priority priority,
		const char *domain, const char *logmsg);
#endif

#endif /* _LINUX_METRICSLOG_H */

