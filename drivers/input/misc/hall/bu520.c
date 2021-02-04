/*
 * Hall sensor driver capable of dealing with more than one sensor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/suspend.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>

#include <mach/mt_gpio_def.h>
#include <mach/eint.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input/bu520.h>
#include <linux/wakelock.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#endif

#define HALL_BUTTON_PRESS_DELAY_MS		20
#define KEY_PRESSED 1
#define KEY_RELEASED 0

enum display_status {
	DISPLAY_OFF = 0,
	DISPLAY_ON
};
enum cover_status {
	COVER_CLOSED = 0,
	COVER_OPENED
};
struct hall_priv {
	struct work_struct work;
	struct mutex data_lock;
	enum   display_status display;
	enum   cover_status cover;
	struct input_dev *input;
	struct hall_sensor_data *sensor_data;
	int hall_sensor_num;
	spinlock_t irq_lock;
	struct wake_lock hall_wake_lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#endif
};

#ifdef CONFIG_PROC_FS
#define HALL_PROC_FILE "driver/hall_sensor"
static int hall_show(struct seq_file *m, void *v)
{
	struct hall_priv *priv = m->private;
	struct hall_sensor_data *sensor_data = priv->sensor_data;

	u8 reg_val = 0;
	int i;
	for (i = 0; i < priv->hall_sensor_num; i++)
		reg_val = reg_val | (sensor_data[i].gpio_state << i);

	seq_printf(m, "0x%x\n", reg_val);
	return 0;
}

static int hall_open(struct inode *inode, struct file *file)
{
	return single_open(file, hall_show, PDE_DATA(inode));
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = hall_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_hall_proc_file(struct hall_priv *priv)
{
	struct proc_dir_entry *entry;
	entry = proc_create_data(HALL_PROC_FILE, 0600, NULL, &proc_fops, priv);
	if (!entry)
		pr_err("%s: Error creating %s\n", __func__, HALL_PROC_FILE);
}

static void remove_hall_proc_file(void)
{
	remove_proc_entry(HALL_PROC_FILE, NULL);
}
#endif

/*
 * Simulate we press the power button hold it for a little amount of
 * time and then release it
 */
static void hall_report_event(struct hall_priv *priv)
{
	struct input_dev *input = priv->input;
	unsigned long gpio_pin = priv->sensor_data[0].gpio_pin;
	priv->sensor_data[0].gpio_state = gpio_get_value(gpio_pin);
	priv->cover = priv->sensor_data[0].gpio_state ? COVER_OPENED : COVER_CLOSED;
	mutex_lock(&priv->data_lock);
	pr_debug("## %s %s ##\n", __func__, priv->cover ? "COVER_OPENED" : "COVER_CLOSED");
	pr_debug("## %s %s ##\n", __func__, priv->display ? "DISPLAY_ON" : "DISPLAY_OFF");

	if (((priv->cover == COVER_OPENED) && (priv->display == DISPLAY_OFF)) ||
	    ((priv->cover == COVER_CLOSED) && (priv->display == DISPLAY_ON))) {
		input_event(input, EV_KEY, KEY_POWER, KEY_PRESSED);
		input_sync(input);
		mdelay(HALL_BUTTON_PRESS_DELAY_MS);
		input_event(input, EV_KEY, KEY_POWER, KEY_RELEASED);
		input_sync(input);
		/* Hold a wake lock only when waking up */
		if ((priv->cover == COVER_OPENED) && (priv->display == DISPLAY_OFF))
			wake_lock_timeout(&priv->hall_wake_lock, HZ/2);
	}
	mutex_unlock(&priv->data_lock);
}
static void hall_work_func(struct work_struct *work)
{
	struct hall_priv *priv = container_of(work, struct hall_priv, work);
	hall_report_event(priv);
}
static irqreturn_t hall_sensor_isr(int irq, void *data)
{
	struct hall_priv *priv =
			(struct hall_priv *)data;
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;
	unsigned long gpio_pin;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		if (irq == sensor_data[i].irq) {
			gpio_pin = sensor_data[i].gpio_pin;
			priv->sensor_data[i].gpio_state = gpio_get_value(gpio_pin);
			irq_set_irq_type(irq, priv->sensor_data[i].gpio_state ?
					 IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
			schedule_work(&priv->work);
			break;
		}
	}

	return IRQ_HANDLED;
}

static int configure_irqs(struct hall_priv *priv)
{
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;
	int ret = 0;
	unsigned long irqflags = 0;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		int gpio_pin = sensor_data[i].gpio_pin;
#ifndef CONFIG_OF
		sensor_data[i].irq = gpio_to_irq(gpio_pin);
#endif
		sensor_data[i].gpio_state = gpio_get_value(gpio_pin);
		priv->cover = sensor_data[i].gpio_state ? COVER_OPENED : COVER_CLOSED;
		enable_irq_wake(sensor_data[i].irq);
		irqflags = sensor_data[i].gpio_state ?
			IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
		irqflags |= IRQF_ONESHOT;
		ret = request_threaded_irq(sensor_data[i].irq, NULL,
			hall_sensor_isr, irqflags,
			sensor_data[i].name, priv);

		if (ret < 0)
			break;
	}

	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hall_earlysuspend(struct early_suspend *handler)
{
	struct hall_priv *priv =
	    container_of(handler, struct hall_priv, es);

	mutex_lock(&priv->data_lock);
	priv->display = DISPLAY_OFF;
	mutex_unlock(&priv->data_lock);
}

static void hall_lateresume(struct early_suspend *handler)
{
	struct hall_priv *priv =
	    container_of(handler, struct hall_priv, es);

	mutex_lock(&priv->data_lock);
	priv->display = DISPLAY_ON;
	mutex_unlock(&priv->data_lock);
}

static void hall_setup_early_suspend(struct hall_priv *priv)
{
	priv->es.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	priv->es.suspend = hall_earlysuspend,
	priv->es.resume = hall_lateresume,

	register_early_suspend(&priv->es);
}
#endif

#ifdef CONFIG_PM
static int hall_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hall_priv *priv = platform_get_drvdata(pdev);

	mutex_lock(&priv->data_lock);
	priv->display = DISPLAY_OFF;
	mutex_unlock(&priv->data_lock);

	return 0;
}

static int hall_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hall_priv *priv = platform_get_drvdata(pdev);

	mutex_lock(&priv->data_lock);
	priv->display = DISPLAY_ON;
	mutex_unlock(&priv->data_lock);

	return 0;
}

static const struct dev_pm_ops hall_pm_ops = {
	.suspend = hall_pm_suspend,
	.resume = hall_pm_resume,
};
#endif

static int hall_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	struct hall_priv *priv;
	int error = 0;

#ifdef CONFIG_OF
	struct hall_sensor_data *sensor_data;
	struct device_node *of_node;
	int hall_sensor_num;
	int i;

	of_node = pdev->dev.of_node;

	error = of_property_read_u32(of_node, "sensor-num", &hall_sensor_num);
	if (error) {
		dev_err(dev, "Unable to read hall_sensor_num\n");
		return error;
	}

	sensor_data = kzalloc(sizeof(*sensor_data) * hall_sensor_num, GFP_KERNEL);
	if (!sensor_data) {
		dev_err(dev, "Failed to allocate hall sensor data\n");
		return -ENOMEM;
	}

	for (i = 0; i < hall_sensor_num; i++) {
		error = of_property_read_u32_index(of_node, "int-gpio", i,
					&sensor_data[i].gpio_pin);
		if (error) {
			dev_err(dev, "Unable to read int-gpio[%d]\n", i);
			goto fail0;
		}

		error = of_property_read_string_index(of_node, "sensor-name", i,
					(const char**)&sensor_data[i].name);
		if (error) {
			dev_err(dev, "Unable to read sensor-name[%d]\n", i);
			goto fail0;
		}

		sensor_data[i].irq = irq_of_parse_and_map(of_node, i);
		if (sensor_data[i].irq <= 0) {
			dev_err(dev, "Unable to get interrupt number[%d]\n", i);
			error = -EINVAL;
			goto fail0;
		}

		sensor_data[i].gpio_state = -1;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Failed to allocate driver data\n");
		error = -ENOMEM;
		goto fail0;
	}
#else
	struct hall_sensors *hall_sensors = pdev->dev.platform_data;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Failed to allocate driver data\n");
		return -ENOMEM;
	}
#endif

	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "Failed to allocate input device\n");
		error = -ENOMEM;
		goto fail1;
	}

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;
	priv->display = DISPLAY_OFF;
	priv->input = input;
#ifdef CONFIG_OF
	priv->sensor_data = sensor_data;
	priv->hall_sensor_num = hall_sensor_num;
#else
	priv->sensor_data = hall_sensors->hall_sensor_data;
	priv->hall_sensor_num = hall_sensors->hall_sensor_num;
#endif
	INIT_WORK(&priv->work, hall_work_func);
	mutex_init(&priv->data_lock);

	input_set_capability(input, EV_KEY, KEY_POWER);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Failed to register input dev, err=%d\n", error);
		goto fail2;
	}

	platform_set_drvdata(pdev, priv);
	spin_lock_init(&priv->irq_lock);

	error = configure_irqs(priv);
	if (error) {
		dev_err(dev, "Failed to configure interupts, err=%d\n", error);
		goto fail2;
	}
	wake_lock_init(&priv->hall_wake_lock, WAKE_LOCK_SUSPEND, "hall sensor wakelock");

#ifdef CONFIG_HAS_EARLYSUSPEND
	hall_setup_early_suspend(priv);
#endif

#ifdef CONFIG_PROC_FS
        create_hall_proc_file(priv);
#endif

	return 0;

fail2:
	input_free_device(priv->input);
fail1:
	kfree(priv);

#ifdef CONFIG_OF
fail0:
	kfree(sensor_data);
#endif

	return error;
}

static int hall_remove(struct platform_device *pdev)
{
	struct hall_priv *priv = platform_get_drvdata(pdev);
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		int irq = sensor_data[i].irq;
		free_irq(irq, priv);
	}

	input_unregister_device(priv->input);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&priv->es);
#endif
	wake_lock_destroy(&priv->hall_wake_lock);
#ifdef CONFIG_OF
	kfree(sensor_data);
#endif
	kfree(priv);

#ifdef CONFIG_PROC_FS
	remove_hall_proc_file();
#endif
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id hall_of_match[] = {
	{.compatible = "rohm,hall-bu520", },
	{ },
};

MODULE_DEVICE_TABLE(of, hall_of_match);
#endif

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
		   .name = "hall-bu520",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &hall_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = hall_of_match,
#endif
		   }
};

module_platform_driver(hall_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("Hall sensor driver");
