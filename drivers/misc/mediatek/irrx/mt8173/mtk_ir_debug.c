


 /*
    this file is for sysfs debug
  */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include "mtk_ir_core.h"

int ir_log_debug_on = 0;

extern char last_hang_place[];
extern u32 detect_hung_timers;


#define SPRINTF_DEV_ATTR(fmt, arg...)  \
	do { \
		 temp_len = sprintf(buf, fmt, ##arg);	 \
		 buf += temp_len; \
		 len += temp_len; \
	} while (0)


static ssize_t mtk_ir_core_show_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct attribute *pattr = &(attr->attr);

	int len = 0;
	int temp_len = 0;
	unsigned long vregstart = IRRX_BASE_VIRTUAL;
	struct mtk_ir_core_platform_data *pdata = NULL;

	if (strcmp(pattr->name, "switch_dev") == 0) {
		len = sprintf(buf, "used to switch ir device\n");
	}
	if (strcmp(pattr->name, "debug_log") == 0) {
		SPRINTF_DEV_ATTR("0: debug_log off\n 1: debug_log on\n");
		SPRINTF_DEV_ATTR("cur debug_log (%d)\n ", ir_log_debug_on);
	}
	if (strcmp(pattr->name, "register") == 0) {	/* here used to dump register */
		SPRINTF_DEV_ATTR("-------------dump ir register-----------\n");

		for (; vregstart <= IRRX_BASE_VIRTUAL_END;) {
			SPRINTF_DEV_ATTR("0x%lx = %x\n", vregstart, REGISTER_READ(vregstart));
			vregstart += IRRX_REGISTER_BYTES;
		}

	}

	#if MTK_IR_DEVICE_NODE
	if (strcmp(pattr->name, "log_to") == 0) {
		SPRINTF_DEV_ATTR("log_to=(%d)\n", mtk_ir_get_log_to());
		mtk_ir_dev_show_info(&buf, &len);
		}
	#endif

	if (strcmp(pattr->name, "hungup") == 0) {
		SPRINTF_DEV_ATTR("%s\n", last_hang_place);
		}

	if (strcmp(pattr->name, "cuscode") == 0) {

		if (mtk_rc_core.dev_current != NULL) {
			pdata = mtk_rc_core.dev_current->dev.platform_data;
			SPRINTF_DEV_ATTR("read cuscode(0x%08x)\n", pdata->get_customer_code());
		}
	}

	if (strcmp(pattr->name, "timer") == 0) {

		SPRINTF_DEV_ATTR("detect_hung_timers = %d\n", detect_hung_timers);
	}


	return len;
}

static ssize_t mtk_ir_core_store_info(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t count)
{
	int var;
	unsigned long reg;
	unsigned long val;
	int ret = 0;

	struct list_head *list_cursor;
	struct mtk_ir_device *entry;
	struct mtk_ir_core_platform_data *pdata;
	struct platform_device *pdev;
	struct attribute *pattr = &(attr->attr);

	if (strcmp(pattr->name, "switch_dev") == 0) {
		ret = sscanf(buf, "%d", &var);
		goto switch_device_process;
	}

	if (strcmp(pattr->name, "debug_log") == 0) {
		ret = sscanf(buf, "%d", &var);
		ir_log_debug_on = var;
		return count;
	}

	if (strcmp(pattr->name, "register") == 0) {
		ret = sscanf(buf, "%lx %lx", &reg, &val);
		IR_LOG_ALWAYS("write reg(0x%lx) =  val(0x%lx)\n", reg, val);
		IR_WRITE(reg, val);
		IR_LOG_ALWAYS("read  reg(0x%lx) =  val(0x%x)\n", reg, IR_READ(reg));
		return count;
	}
#if MTK_IR_DEVICE_NODE
	if (strcmp(pattr->name, "log_to") == 0) {
		ret = sscanf(buf, "%d", &var);
		mtk_ir_set_log_to(var);
		return count;
	}
#endif

	if (strcmp(pattr->name, "press_timeout") == 0) {
		ret = sscanf(buf, "%d", &var);
		if (mtk_rc_core.dev_current != NULL) {
			pdata = mtk_rc_core.dev_current->dev.platform_data;
			pdata->i4_keypress_timeout = var;
			IR_LOG_ALWAYS("%s, i4_keypress_timeout = %d\n ", pdata->input_name,
				      pdata->i4_keypress_timeout);
			rc_set_keypress_timeout(pdata->i4_keypress_timeout);
		}
		return count;
	}


	if (strcmp(pattr->name, "cuscode") == 0) {
		ret = sscanf(buf, "%lx ", &reg);
		IR_LOG_ALWAYS("write cuscode(0x%lx)\n", reg);
		if (mtk_rc_core.dev_current != NULL) {
			pdata = mtk_rc_core.dev_current->dev.platform_data;
			pdata->set_customer_code(&reg);
			IR_LOG_ALWAYS("read cuscode(0x%x)\n", pdata->get_customer_code());
		}
		return count;
	}

	if (strcmp(pattr->name, "remap") == 0) {
		int value = -2;
		ret = sscanf(buf, "%lx %lx %d", &reg, &val, &value);
		if (value == -1) {
			mt_ir_reg_remap(reg, (unsigned long)val, false);	/* only print reg value */
		} else if (value >= 0) {
			IR_LOG_ALWAYS("reg = 0x%lx, val = 0x%lx\n", reg, val);
			mt_ir_reg_remap(reg, (unsigned long)val, true);
		}
		return count;
	}



switch_device_process:

	if (!list_empty(&mtk_ir_device_list)) {
		list_for_each(list_cursor, &mtk_ir_device_list) {
			entry = list_entry(list_cursor, struct mtk_ir_device, list);
			pdev = &(entry->dev_platform);
			if (var == (pdev->id)) {	/* find the ok device to siwtch */
				pdata = ((entry->dev_platform).dev).platform_data;
				IR_LOG_ALWAYS("Matched devname(%s),id(%d)\n", pdev->name, pdev->id);
				IR_LOG_ALWAYS("input_devname(%s)\n", pdata->input_name);
				platform_device_register(pdev);
				break;
			}
		}
	}
	return count;

}


static ssize_t mtk_ir_core_store_debug(struct device *dev,
				       struct device_attribute *attr, const char *buf, size_t count)
{
	char *buffdata[24];
	int val;
	int ret = 0;
	struct attribute *pattr = &(attr->attr);
	memset(buffdata, 0, 24);

	if (strcmp(pattr->name, "clock") == 0) {
		ret = sscanf(buf, "%d", &val);
		if (val == 2) {
			mt_ir_reg_remap(0x10003014, 0x2, true);
			mt_ir_reg_remap(0x10003010, 0x40000, true);
			mt_ir_reg_remap(0x10005600, 0x8, true);
			} else {
			mt_ir_core_clock_enable(val);
			/* 1 enable ,0 disable */
		    }
	}

	if (strcmp(pattr->name, "swirq") == 0) {
		ret = sscanf(buf, "%d", &val);
		if (1 == val) {
			mtk_ir_core_register_swirq(IRQF_TRIGGER_LOW);
		} else if (2 == val) {
			mtk_ir_core_register_swirq(IRQF_TRIGGER_HIGH);
		} else {
			mtk_ir_core_free_swirq();
		}

	}
	if (strcmp(pattr->name, "hwirq") == 0) {
		mtk_ir_core_clear_hwirq();
	}


	return count;

}


static ssize_t mtk_ir_show_alldev_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct list_head *list_cursor;
	struct mtk_ir_device *entry;
	struct mtk_ir_core_platform_data *pdata;
	struct rc_dev *rcdev;
	struct lirc_driver *drv;
	struct rc_map *pmap;
	struct rc_map_table *pscan;
	int index = 0;
	int len = 0;
	int temp_len = 0;

	/* show all mtk_ir_device's info */
	if (!list_empty(&mtk_ir_device_list)) {

		list_for_each(list_cursor, &mtk_ir_device_list) {
			entry = list_entry(list_cursor, struct mtk_ir_device, list);

			SPRINTF_DEV_ATTR("------- devname(%s),id(%d)---------\n",
					 entry->dev_platform.name, entry->dev_platform.id);


			pdata = ((entry->dev_platform).dev).platform_data;
			pmap = &(pdata->p_map_list->map);
			pscan = pmap->scan;

			SPRINTF_DEV_ATTR("input_devname(%s)\n", pdata->input_name);

			SPRINTF_DEV_ATTR("rc_map name(%s)\n", pmap->name);
			SPRINTF_DEV_ATTR("i4_keypress_timeout(%d)ms\n", pdata->i4_keypress_timeout);

			SPRINTF_DEV_ATTR("rc_map_items:\n");
			/* here show customer's map, not really the finally map */
			for (index = 0; index < (pmap->size); index++) {
				SPRINTF_DEV_ATTR("{0x%x, %d}\n",
						 pscan[index].scancode, pscan[index].keycode);
			}
			ASSERT(pdata->get_customer_code != NULL);
			SPRINTF_DEV_ATTR("customer code = 0x%x\n", pdata->get_customer_code());
		}
	}
	SPRINTF_DEV_ATTR("irq = %d\n", mtk_rc_core.irq);
	SPRINTF_DEV_ATTR("irq_register = %s\n", mtk_rc_core.irq_register ? "true" : "false");
	if (mtk_rc_core.dev_current != NULL) {
		SPRINTF_DEV_ATTR("current activing devname(%s),id(%d)\n",
				 mtk_rc_core.dev_current->name, mtk_rc_core.dev_current->id);

	} else {
		SPRINTF_DEV_ATTR("no activing ir device\n");
	}
	rcdev = mtk_rc_core.rcdev;
	drv = mtk_rc_core.drv;

	if (rcdev != NULL) {
		SPRINTF_DEV_ATTR("current rc_dev devname(%s),id(%d)\n",
				 mtk_rc_core.dev_current->name, mtk_rc_core.dev_current->id);
		SPRINTF_DEV_ATTR("current rc_dev input_devname(%s)\n", rcdev->input_name);
		/* show current activiting device's really mappiing */
		if (likely((drv != NULL) && (rcdev != NULL))) {	/* transfer code to     /dev/lirc */
			pmap = &(rcdev->rc_map);
			pscan = pmap->scan;
			for (index = 0; index < (pmap->len); index++) {
				SPRINTF_DEV_ATTR("{0x%x, %d}\n",
						 pscan[index].scancode, pscan[index].keycode);
			}
		}
	}
#ifdef MTK_IR_WAKEUP
	SPRINTF_DEV_ATTR("clock = enable\n");
#else
	SPRINTF_DEV_ATTR("clock = disable\n");
#endif

	return len;
}

#if MTK_IRRX_AS_MOUSE_INPUT
static ssize_t mtk_ir_core_show_mouse_info(struct device *dev,
					   struct device_attribute *attr, char *buf)
{

	struct mtk_ir_core_platform_data *pdata = NULL;
	struct mtk_ir_mouse_code *p_mousecode = NULL;
	struct list_head *list_cursor;
	struct mtk_ir_device *entry;
	int len = 0;
	int temp_len = 0;

	SPRINTF_DEV_ATTR("g_ir_device_mode = %d\n", mtk_ir_mouse_get_device_mode());

	if (!list_empty(&mtk_ir_device_list)) {

		list_for_each(list_cursor, &mtk_ir_device_list) {
			entry = list_entry(list_cursor, struct mtk_ir_device, list);

			SPRINTF_DEV_ATTR("------- devname(%s),id(%d)---------\n",
					 entry->dev_platform.name, entry->dev_platform.id);
			pdata = ((entry->dev_platform).dev).platform_data;
			SPRINTF_DEV_ATTR("input_mousename(%s)\n", pdata->mousename);
			p_mousecode = &(pdata->mouse_code);
			SPRINTF_DEV_ATTR("scanleft =  0x%x\n", p_mousecode->scanleft);
			SPRINTF_DEV_ATTR("scanright = 0x%x\n", p_mousecode->scanright);
			SPRINTF_DEV_ATTR("scanup = 0x%x\n", p_mousecode->scanup);
			SPRINTF_DEV_ATTR("scandown = 0x%x\n", p_mousecode->scandown);
			SPRINTF_DEV_ATTR("scanenter = 0x%x\n", p_mousecode->scanenter);
			SPRINTF_DEV_ATTR("scanswitch = 0x%x\n", p_mousecode->scanswitch);
		}
	}

	SPRINTF_DEV_ATTR("x_small_step = %d\n", mtk_ir_mouse_get_x_smallstep());
	SPRINTF_DEV_ATTR("y_small_step = %d\n", mtk_ir_mouse_get_y_smallstep());
	SPRINTF_DEV_ATTR("x_large_step = %d\n", mtk_ir_mouse_get_x_largestep());
	SPRINTF_DEV_ATTR("y_large_step = %d\n", mtk_ir_mouse_get_y_largestep());
	return len;
}

static ssize_t mtk_ir_core_store_mouse_info(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int varx = -1;
	int vary = -1;
	struct attribute *pattr = &(attr->attr);
	if (strcmp(pattr->name, "mouse_x_y_small") == 0) {
		sscanf(buf, "%d %d", &varx, &vary);
		mtk_ir_mouse_set_x_smallstep(varx);
		mtk_ir_mouse_set_y_smallstep(vary);

	}
	if (strcmp(pattr->name, "mouse_x_y_large") == 0) {
		sscanf(buf, "%d %d", &varx, &vary);
		mtk_ir_mouse_set_x_largestep(varx);
		mtk_ir_mouse_set_y_largestep(vary);
	}

	return count;

}

#endif

/* show all  device  info */
static DEVICE_ATTR(alldev_info, 0444, mtk_ir_show_alldev_info, NULL);

/* switch device protocol */
static DEVICE_ATTR(switch_dev, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);

static DEVICE_ATTR(debug_log, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);

static DEVICE_ATTR(register, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);

static DEVICE_ATTR(clock, 0220, NULL, mtk_ir_core_store_debug);

static DEVICE_ATTR(swirq, 0220, NULL, mtk_ir_core_store_debug);

static DEVICE_ATTR(hwirq, 0220, NULL, mtk_ir_core_store_debug);
static DEVICE_ATTR(press_timeout, 0220, NULL, mtk_ir_core_store_info);
static DEVICE_ATTR(hungup, 0444, mtk_ir_core_show_info, NULL);
static DEVICE_ATTR(cuscode, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);
static DEVICE_ATTR(timer, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);
static DEVICE_ATTR(remap, 0220, NULL, mtk_ir_core_store_info);



#if MTK_IR_DEVICE_NODE
static DEVICE_ATTR(log_to, 0664, mtk_ir_core_show_info, mtk_ir_core_store_info);
#endif

#if MTK_IRRX_AS_MOUSE_INPUT
static DEVICE_ATTR(mouse, 0444, mtk_ir_core_show_mouse_info, NULL);
static DEVICE_ATTR(mouse_x_y_small, 0220, NULL, mtk_ir_core_store_mouse_info);
static DEVICE_ATTR(mouse_x_y_large, 0220, NULL, mtk_ir_core_store_mouse_info);


#endif


static struct device_attribute *ir_attr_list[] = {
	/* &dev_attr_curdev_info, */
	&dev_attr_alldev_info,
	&dev_attr_switch_dev,
	&dev_attr_debug_log,
	&dev_attr_register,
	&dev_attr_clock,
	&dev_attr_swirq,
	&dev_attr_hwirq,
	&dev_attr_press_timeout,
	&dev_attr_hungup,
	&dev_attr_cuscode,
	&dev_attr_timer,
	&dev_attr_remap,


#if MTK_IR_DEVICE_NODE
	&dev_attr_log_to,
#endif

#if MTK_IRRX_AS_MOUSE_INPUT
	&dev_attr_mouse,
	&dev_attr_mouse_x_y_small,
	&dev_attr_mouse_x_y_large,
#endif

};

int mtk_ir_core_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(ir_attr_list) / sizeof(ir_attr_list[0]));
	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, ir_attr_list[idx]);
		if (err) {
			IR_LOG_ALWAYS("IRRX idx(%d) err(%d)\n", idx, err);
			break;
		}
	}
	return err;
}

void mtk_ir_core_remove_attr(struct device *dev)
{
	int idx;
	int num = (int)(sizeof(ir_attr_list) / sizeof(ir_attr_list[0]));
	if (!dev)
		return;

	for (idx = 0; idx < num; idx++) {
		device_remove_file(dev, ir_attr_list[idx]);
	}
}
