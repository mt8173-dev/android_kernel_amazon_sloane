#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>

#include "da9212.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define DA9212_SLAVE_ADDR_WRITE   0xD0
#define DA9212_SLAVE_ADDR_READ    0xD1

#ifdef I2C_EXT_BUCK_CHANNEL
#define da9212_BUSNUM I2C_EXT_BUCK_CHANNEL
#else
#define da9212_BUSNUM 1
#endif

unsigned int g_vproc_en_gpio_number = 0;
unsigned int g_vproc_vsel_gpio_number = 0;

void ext_buck_vproc_vsel(int val)
{
	da9212_buck_set_switch(DA9212_BUCK_B, val);
}

void ext_buck_vproc_en(int val)
{
	da9212_buck_set_en(DA9212_BUCK_A, val);
}

static struct i2c_client *new_client;
static const struct i2c_device_id da9212_i2c_id[] = { {"da9212", 0}, {} };

static int da9212_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
#ifdef CONFIG_OF
static const struct of_device_id da9212_of_match[] = {
	{.compatible = "dlg,da9212",},
	{},
};

MODULE_DEVICE_TABLE(of, da9212_of_match);
#endif
static struct i2c_driver da9212_driver = {
	.driver = {
		   .name = "da9212",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(da9212_of_match),
#endif
		   },
	.probe = da9212_driver_probe,
	.id_table = da9212_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
static DEFINE_MUTEX(da9212_i2c_access);

int g_da9212_driver_ready = 0;
int g_da9212_hw_exist = 0;
/**********************************************************
  *
  *   [I2C Function For Read/Write da9212]
  *
  *********************************************************/
int da9212_read_byte(kal_uint8 cmd, kal_uint8 *data)
{
	int ret;

	struct i2c_msg msg[2];

	msg[0].addr = new_client->addr;
	msg[0].buf = &cmd;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = new_client->addr;
	msg[1].buf = data;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(new_client->adapter, msg, 2);

	if (ret != 2)
		pr_err("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

int da9212_write_byte(kal_uint8 cmd, kal_uint8 data)
{
	char buf[2];
	int ret;

	buf[0] = cmd;
	buf[1] = data;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_err("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
kal_uint32 da9212_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
	kal_uint8 da9212_reg = 0;
	int ret = 0;

	ret = da9212_read_byte(RegNum, &da9212_reg);
	da9212_reg &= (MASK << SHIFT);
	*val = (da9212_reg >> SHIFT);

	return ret;
}

kal_uint32 da9212_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
	kal_uint8 da9212_reg = 0;
	int ret = 0;

	ret = da9212_read_byte(RegNum, &da9212_reg);
	da9212_reg &= ~(MASK << SHIFT);
	da9212_reg |= (val << SHIFT);

	ret = da9212_write_byte(RegNum, da9212_reg);

	return ret;
}

kal_uint32 da9212_get_reg_value(kal_uint32 reg)
{
	kal_uint32 ret = 0;
	kal_uint8 reg_val = 0;

	ret = da9212_read_interface((kal_uint8) reg, &reg_val, 0xFF, 0x0);

	return reg_val;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void da9212_dump_register(void)
{
	kal_uint8 i = 0;

	pr_info("[da9212] page 0,1: ");
	pr_info("[da9212] [0x%x]=0x%x ", 0x0, da9212_get_reg_value(0x0));
	for (i = DA9212_REG_STATUS_A; i <= DA9212_REG_BUCKB_CONT; i++)
		pr_info("[da9212] [0x%x]=0x%x\n", i, da9212_get_reg_value(i));

	for (i = DA9212_REG_BUCK_ILIM; i <= DA9212_REG_VBUCKB_B; i++)
		pr_info("[da9212] [0x%x]=0x%x\n", i, da9212_get_reg_value(i));

	pr_info("[da9212] page 2,3: ");
	for (i = 0x05; i <= 0x06; i++) {
		/* select to page 2,3 */
		da9212_config_interface(DA9212_REG_PAGE_CON, 0x2, 0xF, 0);
		pr_info("[da9212] [0x%x]=0x%x\n", i, da9212_get_reg_value(i));
	}
	for (i = 0x43; i <= 0x4F; i++) {
		/* select to page 2,3 */
		da9212_config_interface(DA9212_REG_PAGE_CON, 0x2, 0xF, 0);
		pr_info("[da9212] [0x%x]=0x%x\n", i, da9212_get_reg_value(i));
	}
	/* select to page 0,1 */
	da9212_config_interface(DA9212_REG_PAGE_CON, 0x0, 0xF, 0);
}

int da9212_buck_set_en(kal_uint8 buck, int en_bit)
{
	int ret = 0;
	kal_uint8 reg = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	reg = buck ? DA9212_REG_BUCKB_CONT : DA9212_REG_BUCKA_CONT;
	if (buck == DA9212_BUCK_A) {
		ret = da9212_config_interface(reg, en_bit, 0x1, DA9212_BUCK_EN_SHIFT);
	} else {
		ret = da9212_config_interface(reg, en_bit, 0x1, DA9212_BUCK_EN_SHIFT);
	}
	return ret;
}

int da9212_buck_get_en(kal_uint8 buck)
{
	int ret = 0;
	kal_uint8 reg = 0;
	kal_uint8 val = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	reg = buck ? DA9212_REG_BUCKB_CONT : DA9212_REG_BUCKA_CONT;
	ret = da9212_read_interface(reg, &val, 0x1, DA9212_BUCK_EN_SHIFT);

	return val;
}

void da9212_hw_init(void)
{
	kal_uint32 ret = 0;
	/* page reverts to 0 after one access */
	ret = da9212_config_interface(DA9212_REG_PAGE_CON, 0x1, 0x1, DA9212_PEG_PAGE_REVERT_SHIFT);
	/* BUCKA_EN = 1 */
	ret = da9212_config_interface(DA9212_REG_BUCKA_CONT,
				      DA9212_BUCK_ON, 0x1, DA9212_BUCK_EN_SHIFT);
	/* BUCKB_EN = 1 */
	ret = da9212_config_interface(DA9212_REG_BUCKB_CONT,
				      DA9212_BUCK_ON, 0x1, DA9212_BUCK_EN_SHIFT);
	/* GPIO setting */
	ret = da9212_config_interface(DA9212_REG_GPIO_0_1, 0x4, 0xF, DA9212_GPIO0_PIN_SHIFT);
	ret = da9212_config_interface(DA9212_REG_GPIO_0_1, 0x4, 0xF, DA9212_GPIO1_PIN_SHIFT);
	ret = da9212_config_interface(DA9212_REG_GPIO_2_3, 0x7, 0xF, DA9212_GPIO2_PIN_SHIFT);
	ret = da9212_config_interface(DA9212_REG_GPIO_2_3, 0x7, 0xF, DA9212_GPIO3_PIN_SHIFT);
	ret = da9212_config_interface(DA9212_REG_GPIO_4, 0x04, 0xFF, DA9212_GPIO4_PIN_SHIFT);
	/* BUCKA_GPI = GPIO0 */
	ret = da9212_config_interface(DA9212_REG_BUCKA_CONT, 0x01, 0x03, DA9212_BUCK_GPI_SHIFT);
	/* BUCKB_GPI = None */
	ret = da9212_config_interface(DA9212_REG_BUCKB_CONT, 0x00, 0x03, DA9212_BUCK_GPI_SHIFT);
	/* VBUCKA_A */
	ret = da9212_config_interface(DA9212_REG_BUCKA_CONT, 0x00, 0x01, DA9212_VBUCK_SEL_SHIFT);
	/* VBUCKB_A */
	ret = da9212_config_interface(DA9212_REG_BUCKB_CONT, 0x00, 0x01, DA9212_VBUCK_SEL_SHIFT);
	/* VBUCKA_GPI = None */
	ret = da9212_config_interface(DA9212_REG_BUCKA_CONT, 0x00, 0x03, DA9212_VBUCK_GPI_SHIFT);
	/* VBUCKB_GPI = GPIO1 */
	ret = da9212_config_interface(DA9212_REG_BUCKB_CONT, 0x01, 0x03, DA9212_VBUCK_GPI_SHIFT);
	/* Disable force PWM mode (this is reserve register) */
	ret = da9212_config_interface(DA9212_REG_BUCKA_CONF,
				      DA9212_BUCK_MODE_PWM, 0x3, DA9212_BUCK_MODE_SHIFT);
	/* Disable force PWM mode (this is reserve register) */
	ret = da9212_config_interface(DA9212_REG_BUCKB_CONF,
				      DA9212_BUCK_MODE_PWM, 0x3, DA9212_BUCK_MODE_SHIFT);

	ext_buck_vproc_en(1);
	ext_buck_vproc_vsel(1);

	/* PWM mode/1.0V, Setting VBUCKA_B = 1.0V */
	ret = da9212_config_interface(DA9212_REG_VBUCKA_B, 0x46, 0xFF, DA9212_VBUCK_SHIFT);
	/* PWM mode/1.0V, Setting VBUCKB_B = 1.13V */
	ret = da9212_config_interface(DA9212_REG_VBUCKB_B, 0x53, 0xFF, DA9212_VBUCK_SHIFT);
}

void da9212_hw_component_detect(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;
	/* page reverts to 0 after one access */
	ret = da9212_config_interface(DA9212_REG_PAGE_CON, 0x1, 0x1, DA9212_PEG_PAGE_REVERT_SHIFT);
	/* select to page 2,3 */
	ret = da9212_config_interface(DA9212_REG_PAGE_CON, 0x2, 0xF, DA9212_PEG_PAGE_SHIFT);

	ret = da9212_read_interface(0x5, &val, 0xF, 4);

	/* check default SPEC. value */
	if (val == 0xD)
		g_da9212_hw_exist = 1;
	else
		g_da9212_hw_exist = 0;

	pr_info("%s: val = %d\n", __func__, val);
}

int is_da9212_sw_ready(void)
{
	return g_da9212_driver_ready;
}

int is_da9212_exist(void)
{
	return g_da9212_hw_exist;
}

int da9212_buck_set_mode(kal_uint8 buck, int mode)
{
	int ret = 0;
	kal_uint8 reg = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	reg = buck ? DA9212_REG_BUCKB_CONF : DA9212_REG_BUCKA_CONF;

	ret = da9212_config_interface(reg, mode, DA9212_BUCK_MODE_MASK, DA9212_BUCK_MODE_SHIFT);

	return ret;
}

int da9212_buck_get_mode(kal_uint8 buck)
{
	int ret = 0;
	kal_uint8 reg = 0;
	kal_uint8 val = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	reg = buck ? DA9212_REG_BUCKB_CONF : DA9212_REG_BUCKA_CONF;
	ret = da9212_read_interface(reg, &val, DA9212_BUCK_MODE_MASK, DA9212_BUCK_MODE_SHIFT);
	return val;
}

int da9212_buck_set_switch(kal_uint8 buck, int val)
{
	int ret = 0;
	kal_uint8 reg = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	if (buck == DA9212_BUCK_B) {
		reg = buck ? DA9212_REG_BUCKB_CONT : DA9212_REG_BUCKA_CONT;
		ret = da9212_config_interface(reg, val, 0x1, DA9212_VBUCK_SEL_SHIFT);
	} else {
		reg = buck ? DA9212_REG_BUCKB_CONT : DA9212_REG_BUCKA_CONT;
		ret = da9212_config_interface(reg, val, 0x1, DA9212_VBUCK_SEL_SHIFT);
	}
	return ret;
}

int da9212_buck_get_switch(kal_uint8 buck)
{
	int ret = 0;
	kal_uint8 reg = 0;
	kal_uint8 val = 0;

	if (!is_da9212_sw_ready()) {
		pr_err("%s: da9212 driver is not exist\n", __func__);
		return 0;
	}
	if (!is_da9212_exist()) {
		pr_err("%s: da9212 is not exist\n", __func__);
		return 0;
	}
	reg = buck ? DA9212_REG_BUCKB_CONT : DA9212_REG_BUCKA_CONT;
	ret = da9212_read_interface(reg, &val, 0x1, DA9212_VBUCK_SEL_SHIFT);

	return val;
}

int da9212_buck_set_voltage(kal_uint8 buck, unsigned long voltage)
{
	int ret = 1;
	kal_uint8 vol_sel = 0;
	kal_uint8 reg = 0;
	/* 300mV~1570mV, step=10mV */
	vol_sel = ((voltage) - DA9212_MIN_MV * 1000) / (DA9212_STEP_MV * 1000);
	if (vol_sel > 127)
		vol_sel = 127;
	reg = buck + DA9212_REG_VBUCKA_A;
	ret = da9212_write_byte(reg, vol_sel);

	pr_info("%s: voltage = %lu, vol_sel = %x, get = 0x%x\n", __func__,
		voltage, vol_sel, da9212_get_reg_value(reg));

	return ret;
}

unsigned long da9212_buck_get_voltage(kal_uint8 buck)
{
	kal_uint8 vol_sel = 0;
	kal_uint8 reg = 0;
	unsigned long voltage = 0;

	reg = buck + DA9212_REG_VBUCKA_A;
	da9212_read_interface(reg, &vol_sel, DA9212_VBUCK_MASK, DA9212_VBUCK_SHIFT);
	voltage = (DA9212_MIN_MV + vol_sel * DA9212_STEP_MV) * 1000;
	return voltage;
}

#ifdef CONFIG_OF
static int of_get_da9212_platform_data(struct device *dev)
{
	int ret, num;

	if (dev->of_node) {
		const struct of_device_id *match;
		match = of_match_device(of_match_ptr(da9212_of_match), dev);
		if (!match) {
			dev_err(dev, "Error: No device match found\n");
			return -ENODEV;
		}
	}
	ret = of_property_read_u32(dev->of_node, "vbuck-gpio", &num);
	if (!ret)
		g_vproc_vsel_gpio_number = num;
	ret = of_property_read_u32(dev->of_node, "en-gpio", &num);
	if (!ret)
		g_vproc_en_gpio_number = num;

	dev_err(dev, "g_vproc_en_gpio_number %d\n", g_vproc_en_gpio_number);
	dev_err(dev, "g_vproc_vsel_gpio_number %d\n", g_vproc_vsel_gpio_number);
	return 0;
}
#else
static int of_get_da9212_platform_data(struct device *dev)
{
	return 0;
}
#endif
static int da9212_driver_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int err = 0;

	of_get_da9212_platform_data(&i2c->dev);
	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}
	memset(new_client, 0, sizeof(struct i2c_client));

	new_client = i2c;

	da9212_hw_component_detect();

	if (g_da9212_hw_exist == 1) {
		da9212_hw_init();
		da9212_dump_register();
	}
	g_da9212_driver_ready = 1;

	if (g_da9212_hw_exist == 0) {
		dev_err(&i2c->dev, "[da9212_driver_probe] return err\n");
		return err;
	}

	return 0;

 exit:
	return err;

}

#if 0
/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
kal_uint8 g_reg_value_da9212 = 0;
static ssize_t show_da9212_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", g_reg_value_da9212);
}

static ssize_t store_da9212_access(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	if (buf != NULL && size != 0) {
		reg_address = simple_strtoul(buf, &pvalue, 16);

		if (size > 4) {
			reg_value = simple_strtoul((pvalue + 1), NULL, 16);

			if (reg_address < 0x100) {
				da9212_config_interface(0x0, 0x0, 0xF, 0);
			} else {
				da9212_config_interface(0x0, 0x2, 0xF, 0);
				reg_address = reg_address & 0xFF;
			}
			ret = da9212_config_interface(reg_address, reg_value, 0xFF, 0x0);

			/* restore to page 0,1 */
			da9212_config_interface(0x0, 0x0, 0xF, 0);
		} else {
			if (reg_address < 0x100) {
				da9212_config_interface(0x0, 0x0, 0xF, 0);
			} else {
				/* select to page 2,3 */
				da9212_config_interface(0x0, 0x2, 0xF, 0);
				reg_address = reg_address & 0xFF;
			}
			ret = da9212_read_interface(reg_address, &g_reg_value_da9212, 0xFF, 0x0);

			/* restore to page 0,1 */
			da9212_config_interface(0x0, 0x0, 0xF, 0);
		}
	}
	return size;
}

static DEVICE_ATTR(access, 0664, show_da9212_access, store_da9212_access);

static ssize_t show_da9212_buck_en(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x, %x\n",
		       da9212_buck_get_en(DA9212_BUCK_A), da9212_buck_get_en(DA9212_BUCK_B));
}

static ssize_t store_da9212_buck_en(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int buck = 0;

	if (buf != NULL && size != 0) {

		buck = simple_strtoul(buf, &pvalue, 8);
		reg_value = simple_strtoul((pvalue + 1), NULL, 16);
		/* select to page 0,1 */
		da9212_config_interface(0x0, 0x0, 0xF, 0);
		da9212_buck_set_en(buck, reg_value);

	}
	return size;
}

static DEVICE_ATTR(enable, 0664, show_da9212_buck_en, store_da9212_buck_en);

static ssize_t show_da9212_buck_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x, %x\n",
		       da9212_buck_get_mode(DA9212_BUCK_A), da9212_buck_get_mode(DA9212_BUCK_B));
}

static ssize_t store_da9212_buck_mode(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int buck = 0;

	if (buf != NULL && size != 0) {

		buck = simple_strtoul(buf, &pvalue, 8);
		reg_value = simple_strtoul((pvalue + 1), NULL, 16);
		/* select to page 0,1 */
		da9212_config_interface(0x0, 0x0, 0xF, 0);
		da9212_buck_set_mode(buck, reg_value);

	}
	return size;
}

static DEVICE_ATTR(mode, 0664, show_da9212_buck_mode, store_da9212_buck_mode);

static ssize_t show_da9212_buck_switch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x, %x\n",
		       da9212_buck_get_switch(DA9212_BUCK_A),
		       da9212_buck_get_switch(DA9212_BUCK_B));
}

static ssize_t store_da9212_buck_switch(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int buck = 0;

	if (buf != NULL && size != 0) {

		buck = simple_strtoul(buf, &pvalue, 8);
		reg_value = simple_strtoul((pvalue + 1), NULL, 16);
		/* select to page 0,1 */
		da9212_config_interface(0x0, 0x0, 0xF, 0);
		da9212_buck_set_switch(buck, reg_value);

	}
	return size;
}

static DEVICE_ATTR(buck_switch, 0664, show_da9212_buck_switch, store_da9212_buck_switch);

static ssize_t show_da9212_buck_voltage(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld, %ld, %ld, %ld\n",
		       da9212_buck_get_voltage(DA9212_VBUCKA_A),
		       da9212_buck_get_voltage(DA9212_VBUCKA_B),
		       da9212_buck_get_voltage(DA9212_VBUCKB_A),
		       da9212_buck_get_voltage(DA9212_VBUCKB_B));
}

static ssize_t store_da9212_buck_voltage(struct device *dev,
					 struct device_attribute *attr, const char *buf,
					 size_t size)
{
	char *pvalue = NULL;
	unsigned long reg_value = 0;
	unsigned int buck = 0;

	if (buf != NULL && size != 0) {

		buck = simple_strtoul(buf, &pvalue, 8);
		reg_value = simple_strtoul((pvalue + 1), NULL, 16);
		da9212_config_interface(0x0, 0x0, 0xF, 0);
		da9212_buck_set_voltage(buck, reg_value);
	}
	return size;
}

static DEVICE_ATTR(voltage, 0664, show_da9212_buck_voltage, store_da9212_buck_voltage);

static ssize_t show_da9212_buck_vsel_test(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	return 1;
}

static ssize_t store_da9212_buck_vsel_test(struct device *dev,
					   struct device_attribute *attr, const char *buf,
					   size_t size)
{
	unsigned long reg_value = 0;
	unsigned long original_value = 0;
	unsigned int buck = 0;
	int i;
	if (sscanf(buf, "%u\n", &buck) > 0) {
		/* select to page 0,1 */
		da9212_config_interface(0x0, 0x0, 0xF, 0);
		original_value = da9212_buck_get_voltage(buck);
		for (i = 0; i <= DA9212_VBUCK_MASK; i++) {
			reg_value = (DA9212_MIN_MV + i * DA9212_STEP_MV) * 1000;
			da9212_buck_set_voltage(buck, reg_value);
			mdelay(1);
			if (reg_value != da9212_buck_get_voltage(buck))
				return i;
			mdelay(1000);
		}
		da9212_buck_set_voltage(buck, original_value);
		pr_info("%s: vbuck : %u, pass...\n", __func__, buck);
	}
	return size;
}

static DEVICE_ATTR(vsel_test, 0664, show_da9212_buck_vsel_test, store_da9212_buck_vsel_test);

static int da9212_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_access);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_enable);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_mode);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_buck_switch);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_voltage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_vsel_test);
	return 0;
}

struct platform_device da9212_user_space_device = {
	.name = "da9212-user",
	.id = -1,
};

static struct platform_driver da9212_user_space_driver = {
	.probe = da9212_user_space_probe,
	.driver = {
		   .name = "da9212-user",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(da9212_of_match),
#endif
		   },
};
#endif
static int __init da9212_init(void)
{
	pr_info("da9212 init start. ch=%d\n", da9212_BUSNUM);

	if (i2c_add_driver(&da9212_driver) != 0)
		pr_err("failed to register da9212 i2c driver.\n");
	else
		pr_info("Success to register da9212 i2c driver.\n");
#if 0
	/* da9212 user space access interface */
	ret = platform_device_register(&da9212_user_space_device);
	if (ret) {
		pr_err("Unable to device da9212 register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&da9212_user_space_driver);
	if (ret) {
		pr_err("Unable to register da9212 driver (%d)\n", ret);
		return ret;
	}
#endif
	return 0;
}

static void __exit da9212_exit(void)
{
	i2c_del_driver(&da9212_driver);
}
module_init(da9212_init);
module_exit(da9212_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C da9212 Driver");
