/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*********************************
* include
**********************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/earlysuspend.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/hotplug.h>




/*********************************
* macro
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND

#define STATE_INIT                  0
#define STATE_ENTER_EARLY_SUSPEND   1
#define STATE_ENTER_LATE_RESUME     2

#define SAMPLE_MS_EARLY_SUSPEND "sample_ms=3000"
#define SAMPLE_MS_LATE_RESUME   "sample_ms=200"

#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */

/* #define HOTPLUG_PROC */

/*********************************
* glabal variable
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static int g_enable;
static struct early_suspend mt_hotplug_mechanism_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
	.suspend = NULL,
	.resume = NULL,
};

static int g_cur_state = STATE_ENTER_LATE_RESUME;
static struct work_struct hotplug_sample_ms_adj_work;
#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */

#ifdef HOTPLUG_PROC
static int g_test0;
static int g_test1;
#endif
static int g_limited_ca7_ncpu = 2;
static int g_limited_ca15_ncpu = 2;



/*********************************
* extern function
**********************************/



/*********************************
* early suspend callback function
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mt_hotplug_mechanism_sample_ms_adjust(struct work_struct *data)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;

	fp = filp_open("/data/data/hotplug/cmd", O_WRONLY, 0);
	if (IS_ERR(fp)) {
		HOTPLUG_INFO("open /data/data/hotplug/cmd failed\n");
		return;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	if (STATE_ENTER_EARLY_SUSPEND == g_cur_state) {
		fp->f_op->write(fp, SAMPLE_MS_EARLY_SUSPEND, strlen(SAMPLE_MS_EARLY_SUSPEND),
				&fp->f_pos);
	} else {
		fp->f_op->write(fp, SAMPLE_MS_LATE_RESUME, strlen(SAMPLE_MS_LATE_RESUME),
				&fp->f_pos);
	}

	set_fs(old_fs);

	HOTPLUG_INFO("sample freq is changed %s\n",
		     (STATE_ENTER_EARLY_SUSPEND ==
		      g_cur_state) ? SAMPLE_MS_EARLY_SUSPEND : SAMPLE_MS_LATE_RESUME);

	if (fp) {
		filp_close(fp, NULL);
	}
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void __ref mt_hotplug_mechanism_early_suspend(struct early_suspend *h)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_early_suspend");

	if (g_enable) {
		int i = 0;

		for (i = (num_possible_cpus() - 1); i > 0; i--) {
			if (cpu_online(i))
				cpu_down(i);
		}
	}
	g_cur_state = STATE_ENTER_EARLY_SUSPEND;
	schedule_work(&hotplug_sample_ms_adj_work);
	return;
}
#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */



/*******************************
* late resume callback function
********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void __ref mt_hotplug_mechanism_late_resume(struct early_suspend *h)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_late_resume");

	if (g_enable) {
		/* temp solution for turn on 4 cores */
		int i = 1;

		for (i = 1; i < num_possible_cpus(); i++) {
			if (!cpu_online(i))
				cpu_up(i);
		}
	}
	g_cur_state = STATE_ENTER_LATE_RESUME;
	schedule_work(&hotplug_sample_ms_adj_work);
	return;
}
#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */



#ifdef HOTPLUG_PROC
/**************************************************************
* mt hotplug mechanism control interface for procfs test0
***************************************************************/
static int mt_hotplug_mechanism_read_test0(char *buf, char **start, off_t off, int count, int *eof,
					   void *data)
{
	char *p = buf;

	p += sprintf(p, "%d\n", g_test0);
	*eof = 1;

	HOTPLUG_INFO("mt_hotplug_mechanism_read_test0, hotplug_cpu_count: %d\n",
		     atomic_read(&hotplug_cpu_count));

	return p - buf;
}

static int mt_hotplug_mechanism_write_test0(struct file *file, const char *buffer,
					    unsigned long count, void *data)
{
	int len = 0, test0 = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &test0) == 1) {
		g_test0 = test0;
		return count;
	} else {
		HOTPLUG_INFO("mt_hotplug_mechanism_write_test0, bad argument\n");
	}

	return -EINVAL;
}



/**************************************************************
* mt hotplug mechanism control interface for procfs test1
***************************************************************/
static int mt_hotplug_mechanism_read_test1(char *buf, char **start, off_t off, int count, int *eof,
					   void *data)
{
	char *p = buf;

	p += sprintf(p, "%d\n", g_test1);
	*eof = 1;

	return p - buf;
}

static int mt_hotplug_mechanism_write_test1(struct file *file, const char *buffer,
					    unsigned long count, void *data)
{
	int len = 0, test1 = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &test1) == 1) {
		g_test1 = test1;
		return count;
	} else {
		HOTPLUG_INFO("mt_hotplug_mechanism_write_test1, bad argument\n");
	}

	return -EINVAL;
}
#endif


/*******************************
* kernel module init function
********************************/
static int __init mt_hotplug_mechanism_init(void)
{

#ifdef HOTPLUG_PROC
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mt_hotplug_test_dir = NULL;
#endif

	HOTPLUG_INFO("mt_hotplug_mechanism_init");

#ifdef HOTPLUG_PROC
	mt_hotplug_test_dir = proc_mkdir("mt_hotplug_test", NULL);
	if (!mt_hotplug_test_dir) {
		HOTPLUG_INFO("mkdir /proc/mt_hotplug_test failed");
	} else {
		entry = create_proc_entry("test0", S_IRUGO | S_IWUSR, mt_hotplug_test_dir);
		if (entry) {
			entry->read_proc = mt_hotplug_mechanism_read_test0;
			entry->write_proc = mt_hotplug_mechanism_write_test0;
		}
		entry = create_proc_entry("test1", S_IRUGO | S_IWUSR, mt_hotplug_test_dir);
		if (entry) {
			entry->read_proc = mt_hotplug_mechanism_read_test1;
			entry->write_proc = mt_hotplug_mechanism_write_test1;
		}
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	mt_hotplug_mechanism_early_suspend_handler.suspend = mt_hotplug_mechanism_early_suspend;
	mt_hotplug_mechanism_early_suspend_handler.resume = mt_hotplug_mechanism_late_resume;
	register_early_suspend(&mt_hotplug_mechanism_early_suspend_handler);
	INIT_WORK(&hotplug_sample_ms_adj_work, mt_hotplug_mechanism_sample_ms_adjust);
#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */

	return 0;
}
module_init(mt_hotplug_mechanism_init);



/*******************************
* kernel module exit function
********************************/
static void __exit mt_hotplug_mechanism_exit(void)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_exit");
}
module_exit(mt_hotplug_mechanism_exit);



/**************************************************************
* mt hotplug mechanism control interface for thermal protect
***************************************************************/
void mt_hotplug_mechanism_thermal_protect(int limited_ca7_cpus, int limited_ca15_cpus)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_thermal_protect\n");
	g_limited_ca7_ncpu = limited_ca7_cpus;
	g_limited_ca15_ncpu = limited_ca15_cpus;
}
EXPORT_SYMBOL(mt_hotplug_mechanism_thermal_protect);



#ifdef CONFIG_HAS_EARLYSUSPEND
module_param(g_enable, int, 0644);
module_param(g_limited_ca7_ncpu, int, 0644);
module_param(g_limited_ca15_ncpu, int, 0644);
#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */



MODULE_DESCRIPTION("MediaTek CPU Hotplug Mechanism");
MODULE_LICENSE("GPL");
