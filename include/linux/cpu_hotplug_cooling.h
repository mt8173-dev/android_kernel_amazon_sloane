/*
 *  linux/include/linux/cpu_hotplug_cooling.h
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#ifndef __CPU_HOTPLUG_COOLING_H__
#define __CPU_HOTPLUG_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>

#ifdef CONFIG_CPU_THERMAL
/**
 * cpu_hotplug_cooling_register - function to create cpufreq cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *cpu_hotplug_cooling_register(unsigned int cpu);

/**
 * cpu_hotplug_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpu_hotplug_cooling_unregister(struct thermal_cooling_device *cdev);
#else /* !CONFIG_CPU_THERMAL */
static inline struct thermal_cooling_device *cpu_hotplug_cooling_register(unsigned int cpu)
{
	return NULL;
}
static inline
void cpu_hotplug_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif	/* CONFIG_CPU_THERMAL */

#endif /* __CP__HOTPLUG_COOLING_H__ */
