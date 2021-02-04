/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_battery_throttle.c
 *
 * Description:
 * ------------
 *   This Module defines functions of battery throttling algorithm
 *   for updating the CPU power budget
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/aee.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/xlog.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <mach/charging.h>
#include "mt_battery_throttle.h"
/* #include "cust_battery_throttle.h" */
#define BATTERY_MAX_BUDGET  4200
#define BATTERY_MIN_BUDGET  430
#define BATTERY_MAX_BUDGET_FACTOR  10
#define BATTERY_MIN_BUDGET_FACTOR  0
#define BATTERY_BUDGET_MIN_VOLTAGE 3450
#define BATTERY_BUDGET_TOLERANCE_VOLTAGE 50
#define BATTERY_BUDGET_PERIOD  10

typedef struct _BATTERY_THROTTLE_PROFILE_STRUC {
	int voltage;
	int power_budget;
} BATTERY_THROTTLE_PROFILE_STRUC, *BATTERY_THROTTLE_PROFILE_STRUC_P;

BATTERY_THROTTLE_PROFILE_STRUC battery_throttle_profile[] = {
/* {voltage, power_budget}, */
	{3801, BATTERY_MAX_BUDGET},
	{3761, 4000},
	{3708, 3500},
	{3666, 3000},
	{3600, 2250},
	{0, 1500}
};

static int g_test_flag;
static int g_test_budget = BATTERY_MAX_BUDGET;
static kal_bool bat_budget_thread_timeout = KAL_FALSE;
static battery_throttle_dev_t *tdev;
static struct hrtimer battery_budget_kthread_timer;
static DECLARE_WAIT_QUEUE_HEAD(bat_budget_thread_wq);

static BATTERY_THROTTLE_DATA_STRUC mtk_Pbudget_data = {
	.previous_power_budget_factor = 0,
	.previous_power_budget = 0,
	.previous_battery_voltage = 0,
	.power_budget_factor = 0,
	.battery_voltage = 0,
	.power_budget = BATTERY_MAX_BUDGET
};

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/*
 *  Read Battery Information.
 *
 *  "cat /sys/devices/platform/battery_meter/FG_g_fg_dbg_bat_zcv"
 *  "cat /sys/devices/platform/battery_meter/FG_g_fg_dbg_bat_volt"
 *  "cat /sys/class/power_supply/battery/InstatVolt"
 */
static int get_battery_info(char *dev)
{
	mm_segment_t oldfs;
	int nRet = 0, fd = 0, nReadSize = 0;
	char *pvalue = NULL;
	char buf[64];

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(dev, O_RDONLY, 0);
	if (fd < 0) {
		battery_xlog_printk(BAT_LOG_CRTI, "[get_battery_info] open fail dev:%s fd:%d\n",
				    dev, fd);
		set_fs(oldfs);
		return fd;
	}

	nReadSize = sys_read(fd, buf, sizeof(buf) - 1);
	battery_xlog_printk(BAT_LOG_FULL, "[get_battery_info] nReadSize:%d\n", nReadSize);
	nRet = simple_strtol(buf, &pvalue, 10);

	set_fs(oldfs);
	sys_close(fd);

	return nRet;
}

static int battery_budget_get_saddles(void)
{
	return sizeof(battery_throttle_profile) / sizeof(BATTERY_THROTTLE_PROFILE_STRUC);
}

static ENUM_PBUDGET_FACTOR _get_battery_budget_factor(BATTERY_THROTTLE_DATA_STRUC *pPbudget)
{
	int Pbudget_case = PBUDGET_FACTOR_NUM;

	if (0 == pPbudget->previous_power_budget) {
		/* case 0: initialization */
		Pbudget_case = PBUDGET_FACTOR_INIT;
	} else if (upmu_is_chr_det() == KAL_TRUE) {
		/* case 1: Charger */
		Pbudget_case = PBUDGET_FACTOR_MAX;
	} else if (BATTERY_MAX_BUDGET_FACTOR == pPbudget->previous_power_budget_factor) {
		if (pPbudget->battery_voltage > BATTERY_BUDGET_MIN_VOLTAGE) {
			/* case 2: previous budget is max power budget, current voltage is higher than 3400 mV */
			Pbudget_case = PBUDGET_FACTOR_MAX;
		} else {
			/* case 3: previous budget is max power budget, current voltage is lower than 3400 mV */
			Pbudget_case = PBUDGET_FACTOR_MEDIUM;
		}
	} else {
		if (pPbudget->battery_voltage > BATTERY_BUDGET_MIN_VOLTAGE) {
			if (pPbudget->battery_voltage - BATTERY_BUDGET_MIN_VOLTAGE < BATTERY_BUDGET_TOLERANCE_VOLTAGE ||
			    pPbudget->battery_average_vol < BATTERY_BUDGET_MIN_VOLTAGE){
				/* case 4.1: previous budget is limited power budget,
				current voltage alomst equal to 3450 mV */
				/* case 4.2: previous budget is limited power budget,
				battery average voltage lower than 3450 mV */
				Pbudget_case = PBUDGET_FACTOR_NO_CHANGE;
			} else {
				/* case 4.3: previous budget is limited power budget,
				current voltage is higher than 3450 mV */
				Pbudget_case = PBUDGET_FACTOR_ADD;
			}
		} else {
			/* case 5: previous budget is limited power budget, current voltage is lower than 3450 mV */
			Pbudget_case = PBUDGET_FACTOR_SUBTRACT;
		}
	}

	return Pbudget_case;
}

static bool _adaptive_battery_budget(int bat_ocv, int bat_vol, int bat_avg)
{
	int saddles = 0, i = 0;
	int battery_budget = 0, max_battery_budget = 0, Pbudget_factor_seq = 0;
	ENUM_PBUDGET_FACTOR Pbudget_factor_case = PBUDGET_FACTOR_NUM;
	BATTERY_THROTTLE_PROFILE_STRUC_P profile_p;
	bool change_budget = 1;

	profile_p = &battery_throttle_profile[0];

	saddles = battery_budget_get_saddles();

/*1. save previous data*/
	if (0 != mtk_Pbudget_data.battery_voltage)
		mtk_Pbudget_data.previous_power_budget = mtk_Pbudget_data.power_budget;

	mtk_Pbudget_data.previous_battery_voltage = mtk_Pbudget_data.battery_voltage;
	mtk_Pbudget_data.previous_power_budget_factor = mtk_Pbudget_data.power_budget_factor;
	mtk_Pbudget_data.battery_average_vol = bat_avg;
	mtk_Pbudget_data.battery_voltage = bat_vol;

	battery_xlog_printk(BAT_LOG_FULL, "previous power budget: %d, previous battery voltage: %d",
			    mtk_Pbudget_data.previous_power_budget,
			    mtk_Pbudget_data.previous_battery_voltage);

/*2. get current max power budget from ocv*/
	for (i = 0; i < saddles; i++) {
		if ((bat_ocv >= (profile_p + i)->voltage)) {
			max_battery_budget = (profile_p + i)->power_budget;
			break;
		}
	}
	battery_xlog_printk(BAT_LOG_FULL, "saddles: %d, max power budget: %d", saddles,
			    max_battery_budget);

/*3. set current power budget factor*/
	Pbudget_factor_case = _get_battery_budget_factor(&mtk_Pbudget_data);
	Pbudget_factor_seq = (max_battery_budget - BATTERY_MIN_BUDGET) / BATTERY_MAX_BUDGET_FACTOR;
	switch (Pbudget_factor_case) {
	case PBUDGET_FACTOR_INIT:
	case PBUDGET_FACTOR_MAX:
		mtk_Pbudget_data.power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
		break;

	case PBUDGET_FACTOR_MEDIUM:
		mtk_Pbudget_data.power_budget_factor =
		    (BATTERY_MAX_BUDGET_FACTOR + BATTERY_MIN_BUDGET_FACTOR) / 2;
		break;

	case PBUDGET_FACTOR_ADD:
		mtk_Pbudget_data.power_budget_factor =
		    mtk_Pbudget_data.previous_power_budget_factor + 1;
		break;

	case PBUDGET_FACTOR_SUBTRACT:
		mtk_Pbudget_data.power_budget_factor =
		    mtk_Pbudget_data.previous_power_budget_factor - 1;
		break;

	case PBUDGET_FACTOR_NO_CHANGE:
		mtk_Pbudget_data.power_budget_factor =
		    mtk_Pbudget_data.previous_power_budget_factor;
		break;

	case PBUDGET_FACTOR_MIN:
	default:
		mtk_Pbudget_data.power_budget_factor = BATTERY_MIN_BUDGET_FACTOR;
		break;
	}

	battery_xlog_printk(BAT_LOG_FULL, "Pbudget_factor_case: %d, Pbudget_factor_seq: %d",
			    Pbudget_factor_case, Pbudget_factor_seq);

/*4. check power budget boundary*/
	if (mtk_Pbudget_data.power_budget_factor > BATTERY_MAX_BUDGET_FACTOR)
		mtk_Pbudget_data.power_budget_factor = BATTERY_MAX_BUDGET_FACTOR;
	else if (mtk_Pbudget_data.power_budget_factor < BATTERY_MIN_BUDGET_FACTOR)
		mtk_Pbudget_data.power_budget_factor = BATTERY_MIN_BUDGET_FACTOR;

	battery_budget =
	    BATTERY_MIN_BUDGET + Pbudget_factor_seq * mtk_Pbudget_data.power_budget_factor;

	if (battery_budget > max_battery_budget)
		battery_budget = max_battery_budget;
	else if (battery_budget < BATTERY_MIN_BUDGET)
		battery_budget = BATTERY_MIN_BUDGET;

/*5. set power budget*/
	mtk_Pbudget_data.power_budget = battery_budget;

	if (mtk_Pbudget_data.power_budget == mtk_Pbudget_data.previous_power_budget)
		change_budget = 0;

	battery_xlog_printk(BAT_LOG_CRTI, "adaptive_battery_budget: %d, %d, %d, %d, %d, %d, %d, %d",
			    change_budget, max_battery_budget,
			    mtk_Pbudget_data.previous_power_budget_factor,
			    mtk_Pbudget_data.previous_power_budget,
			    mtk_Pbudget_data.previous_battery_voltage,
			    mtk_Pbudget_data.battery_voltage, mtk_Pbudget_data.power_budget,
			    mtk_Pbudget_data.power_budget_factor);

	return change_budget;
}

static void battery_budget_notify(int bat_ocv, int bat_vol, int bat_avg)
{
	int change_budget = 1;
	change_budget = _adaptive_battery_budget(bat_ocv, bat_vol, bat_avg);

	if (g_test_flag)
		change_budget = 1;

	if (0 == tdev->flag)
		tdev->flag = change_budget;

	wake_up_interruptible(&tdev->queue);
}

static void bat_budget_thread_wakeup(void)
{
	battery_xlog_printk(BAT_LOG_FULL,
			    "******** battery budget : bat_budget_thread_wakeup  ********\n");

	bat_budget_thread_timeout = KAL_TRUE;

	wake_up(&bat_budget_thread_wq);
}

enum hrtimer_restart battery_budget_kthread_hrtimer_func(struct hrtimer *timer)
{
	bat_budget_thread_wakeup();

	return HRTIMER_NORESTART;
}

static void battery_budget_kthread_hrtimer_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(5, 0);	/* 5s, 5*1000 ms, wait for battery initialization */
	hrtimer_init(&battery_budget_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_budget_kthread_timer.function = battery_budget_kthread_hrtimer_func;
	hrtimer_start(&battery_budget_kthread_timer, ktime, HRTIMER_MODE_REL);

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget_kthread_hrtimer_init : done\n");
}

int bat_budget_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* initialize */
	int bat_ocv = 0, bat_vol = 0, bat_avg = 0;

	/* Run on a process content */
	while (1) {
		battery_xlog_printk(BAT_LOG_FULL, "bat_budget_thread_kthread: wait event\n");

		wait_event(bat_budget_thread_wq, (KAL_TRUE == bat_budget_thread_timeout));

		bat_ocv = get_battery_info(BATTERY_FG_BAT_ZCV);
		bat_vol = get_battery_info(BATTERY_FG_BAT_VOL);
		bat_avg = get_battery_info(BATTERY_BAT_AVG_VOL);

		battery_xlog_printk(BAT_LOG_FULL, "bat_avg: %d, bat_ocv: %d, bat_ovl: %d\n",
				    bat_avg, bat_ocv, bat_vol);

		battery_budget_notify(bat_ocv, bat_vol, bat_avg);

		bat_budget_thread_timeout = KAL_FALSE;
		ktime = ktime_set(BATTERY_BUDGET_PERIOD, 0);	/* 10s, 10* 1000 ms */
		hrtimer_start(&battery_budget_kthread_timer, ktime, HRTIMER_MODE_REL);
	}

	return 0;
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

static int _battery_budget_open(struct inode *inod, struct file *filp)
{
	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device opened\n");
	return 0;
}

static ssize_t _battery_budget_write(struct file *filp, const char __user *buf, size_t len,
				     loff_t *off)
{
	int budget = 0;
	int ret;

	ret = kstrtoint_from_user(buf, len, 10, &budget);

	if (budget < 0) {
		/* For initialization */
		tdev->flag = 1;
	} else {
		/* For Test CMD */
		g_test_flag = 0x1;
		g_test_budget = budget;
		battery_budget_notify(g_test_flag, g_test_budget, g_test_budget);
	}

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device write, %d\n", budget);
	return len;
}

static ssize_t _battery_budget_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	char budget[30];
	int s;

	wait_event_interruptible(tdev->queue, (1 == tdev->flag));
	tdev->flag = 0;

	if (g_test_flag)
		s = snprintf(budget, 30, "%d\n", g_test_budget);
	else
		s = snprintf(budget, 30, "%d\n", mtk_Pbudget_data.power_budget);

	if (s > len)
		s = len;
	if (copy_to_user(buf, budget, s))
		return -EFAULT;

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device read, CMD: %d, budget: %s\n",
			    g_test_flag, budget);

	return s;
}

static unsigned int _battery_budget_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device polled\n");

	poll_wait(filp, &tdev->queue, wait);
	if (tdev->flag == 1)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static int _battery_budget_release(struct inode *inod, struct file *filp)
{
	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device closed\n");
	return 0;
}

static const struct file_operations _battery_budget_fops = {
	.owner = THIS_MODULE,
	.open = _battery_budget_open,
	.read = _battery_budget_read,
	.write = _battery_budget_write,
	.poll = _battery_budget_poll,
	.release = _battery_budget_release,
};

static int __init battery_budget_init(void)
{
	return 0;
}

static void _setup_battery_budget_device(void)
{
	int result = 0;

	tdev = kzalloc(sizeof(battery_throttle_dev_t), GFP_KERNEL);
	result = alloc_chrdev_region(&tdev->devno, tdev->minor, 1, BATTERY_BUDGET);
	tdev->major = MAJOR(tdev->devno);
	tdev->minor = MINOR(tdev->devno);

	cdev_init(&tdev->cdev, &_battery_budget_fops);
	tdev->cdev.owner = THIS_MODULE;
	tdev->cdev.ops = &_battery_budget_fops;
	init_waitqueue_head(&tdev->queue);
	result = cdev_add(&tdev->cdev, tdev->devno, 1);

	if (result)
		battery_xlog_printk(BAT_LOG_CRTI,
				    "battery_budget: Error %d adding battery_budget\n", result);

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget_late_init: %d, %d\n", tdev->major,
			    tdev->minor);

	tdev->cls = class_create(THIS_MODULE, BATTERY_NOTIFY);
	if (IS_ERR(tdev->cls))
		battery_xlog_printk(BAT_LOG_CRTI, "battery_budget: failed in creating class\n");

	device_create(tdev->cls, NULL, tdev->devno, NULL, BATTERY_BUDGET);
}

static int __init battery_budget_late_init(void)
{
	_setup_battery_budget_device();

	/* battery budget kernel thread for 10s check and charger in/out event */
	battery_budget_kthread_hrtimer_init();
	kthread_run(bat_budget_thread_kthread, NULL, "bat_budget_thread_kthread");

	return 0;
}

static void __exit battery_budget_exit(void)
{
	cdev_del(&tdev->cdev);
	unregister_chrdev_region(tdev->devno, 1);

	device_destroy(tdev->cls, tdev->devno);
	class_destroy(tdev->cls);

	battery_xlog_printk(BAT_LOG_CRTI, "battery_budget exit\n");

	if (tdev)
		kfree(tdev);
}
module_init(battery_budget_init);
module_exit(battery_budget_exit);

device_initcall(battery_budget_late_init);
