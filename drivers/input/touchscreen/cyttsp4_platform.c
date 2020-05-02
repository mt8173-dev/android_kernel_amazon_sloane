/*
 * cyttsp4_platform.c
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <mach/board.h>
#include <mach/devs.h>
#include <mach/mt_spm_sleep.h>

#include <linux/gpio.h>
#include <uapi/linux/input.h>

#include <mach/mt_gpio_def.h>
#include <mach/mt_pm_ldo.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#include "cust_gpio_usage.h"
/* cyttsp */
#include <uapi/linux/input.h>
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_i2c.h>
#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_mt.h>

struct lab_i2c_board_info {
	struct i2c_board_info bi;
	//	int (*init)(struct lab_i2c_board_info *);
};

static int touch_power_enable(u16 rst_gpio, bool enable)
{
	mt_set_gpio_dir(rst_gpio, GPIO_DIR_OUT);
	mt_set_gpio_out(rst_gpio, GPIO_OUT_ZERO);
	if (enable) {
		hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_1800, "TP");
		hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3000, "TP");
		msleep(20);

		mt_set_gpio_out(rst_gpio, GPIO_OUT_ONE);
	} else {
		hwPowerDown(MT65XX_POWER_LDO_VGP4, "TP");
		hwPowerDown(MT65XX_POWER_LDO_VGP6, "TP");
		mt_set_gpio_dir(rst_gpio, GPIO_DIR_IN);
	}
	return 0;
}

static int touch_power_init(u16 rst_gpio, bool enable)
{
	touch_power_enable(rst_gpio, false);
	if (likely(enable)) {
//		touch_power_adjust();
		touch_power_enable(rst_gpio, true);
	}
	return 0;
}

static int cyttsp4_xres_abc123(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
	touch_power_enable(pdata->rst_gpio, false);
//	touch_power_adjust();
	msleep(100);
	touch_power_enable(pdata->rst_gpio, true);

	return 0;
}

static int cyttsp4_power_abc123(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
	return touch_power_enable(pdata->rst_gpio, on);
}

/* driver-specific init (power on) */
static int cyttsp4_init_abc123(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
	return touch_power_init(pdata->rst_gpio, on);
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
#include <linux/cyttsp4_img.h>
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp4_params.h"
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *) & cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *) & cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

static struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.ttconfig = &cyttsp4_ttconfig,
	.flags = CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION,
};

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	//	.irq_gpio = TOUCH_IRQ_GPIO,
//	.irq_gpio = GPIO_CTP_EINT_PIN,
	.irq_gpio = 302,
	.rst_gpio = GPIO_CTP_RST_PIN,
	.xres = cyttsp4_xres_abc123,
	.init = cyttsp4_init_abc123,
	.power = cyttsp4_power_abc123,
	.detect = NULL,
	.flags = CY_CORE_FLAG_NONE,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
	.loader_pdata = &_cyttsp4_loader_platform_data,
};

static struct cyttsp4_core_info cyttsp4_core_info = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.platform_data = &_cyttsp4_core_platform_data,
};

#define CY_MAXX 800
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *) &cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = CY_MT_FLAG_NO_TOUCH_ON_LO,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

struct cyttsp4_device_info cyttsp4_mt_device_info = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_platform_data,
};

static struct lab_i2c_board_info i2c_bus0[] = {
	{
		.bi = {
			I2C_BOARD_INFO(CYTTSP4_I2C_NAME, 0x24),
//			.irq = GPIO_CTP_EINT_PIN,
//			.irq = _cyttsp4_core_platform_data.irq_gpio,
			.irq = 302,
			.platform_data = CYTTSP4_I2C_NAME,
		},
	},
};

#if 0
#define LAB_I2C_BUS_CFG(_bus_id, _bus_devs, _bus_speed) \
	.name = #_bus_devs, \
.info = (_bus_devs), \
.bus_id = (_bus_id), \
.speed = (_bus_speed), \
.n_dev = ARRAY_SIZE(_bus_devs)

static struct i2c_board_cfg abc123_i2c_config[] = {
	{
		LAB_I2C_BUS_CFG(0, i2c_bus0, 100),
		.max_rev =   0x0002,
	},
}
#endif

/* I2C init method for cypress touch */
static int __init touch_i2c_init_cypress(void)
{
	int err;
	struct lab_i2c_board_info *info;
	info = &i2c_bus0[0];

	err = i2c_register_board_info(
			0, &info->bi, 1);

	if (err) {
		pr_err("i2c_register_board_info fail!!!\n");
		return 0;
	}
	cyttsp4_register_core_device(&cyttsp4_core_info);
	cyttsp4_register_device(&cyttsp4_mt_device_info);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);

	return 0;
}

early_initcall(touch_i2c_init_cypress);

static void cyttsp4_platform_exit(void)
{
	pr_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress Platform device");

