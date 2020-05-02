#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/reboot.h>
#include <linux/switch.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <mach/mt_typedefs.h>
#include <linux/power/bq25890.h>
#include <linux/power/mt_charging.h>
#include <drivers/misc/mediatek/power/mt8173/upmu_common.h>
#include <mach/mt_gpio_def.h>
#include <mach/upmu_hw.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>
#include <mach/mt_gpio.h>

/**********************************************************
 *
 *    [Define]
 *
 **********************************************************/

#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))
#define bq25890_REG_NUM 20
#define bq25890_SLAVE_ADDR_WRITE   0xD4
#define bq25890_SLAVE_ADDR_READ    0xD5


/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/

static struct i2c_client *new_client;
static struct switch_dev bq25890_fault_reg;
static kal_int32 charging_int_gpio;
static kal_bool charging_type_det_done = KAL_TRUE;
static kal_uint8 bq25890_reg[bq25890_REG_NUM] = { 0 };

static kal_uint8 g_reg_value_bq25890;

/**********************************************************
  *
  *   [I2C Function For Read/Write bq25890]
  *
  *********************************************************/
int bq25890_read_byte(kal_uint8 cmd, kal_uint8 *data)
{
	int ret;

	struct i2c_msg msg[2];

	if (!new_client) {
		pr_err("error: access bq25890 before driver ready\n");
		return 0;
	}

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

int bq25890_write_byte(kal_uint8 cmd, kal_uint8 data)
{
	char buf[2];
	int ret;

	if (!new_client) {
		pr_err("error: access bq25890 before driver ready\n");
		return 0;
	}

	buf[0] = cmd;
	buf[1] = data;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_err("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

kal_uint32 bq25890_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK,
				  kal_uint8 SHIFT)
{
	kal_uint8 bq25890_reg = 0;
	int ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	bq25890_reg &= (MASK << SHIFT);
	*val = (bq25890_reg >> SHIFT);

	return ret;
}

kal_uint32 bq25890_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK,
				    kal_uint8 SHIFT)
{
	kal_uint8 bq25890_reg = 0;
	int ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	bq25890_reg &= ~(MASK << SHIFT);
	bq25890_reg |= (val << SHIFT);

	ret = bq25890_write_byte(RegNum, bq25890_reg);
	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/

/* CON0---------------------------------------------------- */
void bq25890_set_en_hiz(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON0),
			(kal_uint8) (val),
			(kal_uint8) (CON0_EN_HIZ_MASK),
			(kal_uint8) (CON0_EN_HIZ_SHIFT)
		);
}

void bq25890_set_iinlim(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON0),
			(kal_uint8) (val),
			(kal_uint8) (CON0_IINLIM_MASK),
			(kal_uint8) (CON0_IINLIM_SHIFT)
		);
}

kal_uint32 bq25890_get_iinlim(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON0),
			(&val),
			(kal_uint8) (CON0_IINLIM_MASK), (kal_uint8) (CON0_IINLIM_SHIFT)
		);
	return val;
}

/* CON1---------------------------------------------------- */

/* CON2---------------------------------------------------- */
void bq25890_set_force_dpdm(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON2),
			(kal_uint8) (val),
			(kal_uint8) (CON2_FORCE_DPDM_MASK),
			(kal_uint8) (CON2_FORCE_DPDM_SHIFT)
		);
}

void bq25890_set_auto_dpdm_en(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON2),
			(kal_uint8) (val),
			(kal_uint8) (CON2_AUTO_DPDM_MASK),
			(kal_uint8) (CON2_AUTO_DPDM_SHIFT)
		);
}

kal_uint32 bq25890_get_dpdm_status(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON2),
			(&val),
			(kal_uint8) (CON2_FORCE_DPDM_MASK),
			(kal_uint8) (CON2_FORCE_DPDM_SHIFT)
		);
	return val;
}

/* CON3---------------------------------------------------- */
void bq25890_set_wdt_rst(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON3),
			(kal_uint8) (val),
			(kal_uint8) (CON3_WDT_RST_MASK),
			(kal_uint8) (CON3_WDT_RST_SHIFT)
		);
}

void bq25890_set_otg_config(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON3),
			(kal_uint8) (val),
			(kal_uint8) (CON3_OTG_CONFIG_MASK),
			(kal_uint8) (CON3_OTG_CONFIG_SHIFT)
		);
}

void bq25890_set_chg_config(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON3),
			(kal_uint8) (val),
			(kal_uint8) (CON3_CHG_CONFIG_MASK),
			(kal_uint8) (CON3_CHG_CONFIG_SHIFT)
		);
}

void bq25890_set_sys_min(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON3),
			(kal_uint8) (val),
			(kal_uint8) (CON3_SYS_MIN_MASK),
			(kal_uint8) (CON3_SYS_MIN_SHIFT)
		);
}

/* CON4---------------------------------------------------- */
void bq25890_set_ichg(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON4),
			(kal_uint8) (val),
			(kal_uint8) (CON4_ICHG_MASK),
			(kal_uint8) (CON4_ICHG_SHIFT)
		);
}

kal_uint32 bq25890_get_ichg(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON4),
			(&val),
			(kal_uint8) (CON4_ICHG_MASK),
			(kal_uint8) (CON4_ICHG_SHIFT)
		);
	return val;
}
/* CON5---------------------------------------------------- */
void bq25890_set_iprechg(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON5),
			(kal_uint8) (val),
			(kal_uint8) (CON5_IPRECHG_MASK),
			(kal_uint8) (CON5_IPRECHG_SHIFT)
		);
}

void bq25890_set_iterm(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON5),
			(kal_uint8) (val),
			(kal_uint8) (CON5_ITERM_MASK),
			(kal_uint8) (CON5_ITERM_SHIFT)
		);
}

/* CON6---------------------------------------------------- */
void bq25890_set_vreg(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON6),
			(kal_uint8) (val),
			(kal_uint8) (CON6_VREG_MASK),
			(kal_uint8) (CON6_VREG_SHIFT)
		);
}

void bq25890_set_batlowv(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON6),
			(kal_uint8) (val),
			(kal_uint8) (CON6_BATLOWV_MASK),
			(kal_uint8) (CON6_BATLOWV_SHIFT)
		);
}

void bq25890_set_vrechg(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON6),
			(kal_uint8) (val),
			(kal_uint8) (CON6_VRECHG_MASK),
			(kal_uint8) (CON6_VRECHG_SHIFT)
		);
}

/* CON7---------------------------------------------------- */
void bq25890_set_en_term(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON7),
			(kal_uint8) (val),
			(kal_uint8) (CON7_EN_TERM_MASK),
			(kal_uint8) (CON7_EN_TERM_SHIFT)
		);
}

void bq25890_set_watchdog(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON7),
			(kal_uint8) (val),
			(kal_uint8) (CON7_WATCHDOG_MASK),
			(kal_uint8) (CON7_WATCHDOG_SHIFT)
		);
}

void bq25890_set_en_timer(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON7),
			(kal_uint8) (val),
			(kal_uint8) (CON7_EN_TIMER_MASK),
			(kal_uint8) (CON7_EN_TIMER_SHIFT)
		);
}

void bq25890_set_chg_timer(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON7),
			(kal_uint8) (val),
			(kal_uint8) (CON7_CHG_TIMER_MASK),
			(kal_uint8) (CON7_CHG_TIMER_SHIFT)
	);
}

/* CON8---------------------------------------------------- */
void bq25890_set_treg(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON8),
			(kal_uint8) (val),
			(kal_uint8) (CON8_TREG_MASK),
			(kal_uint8) (CON8_TREG_SHIFT)
		);
}

/* CON9---------------------------------------------------- */
void bq25890_set_batfet_disable(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON9),
			(kal_uint8) (val),
			(kal_uint8) (CON9_BATFET_DIS_MASK),
			(kal_uint8) (CON9_BATFET_DIS_SHIFT)
		);
}

/* CON10---------------------------------------------------- */
void bq25890_set_boost_lim(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON10),
			(kal_uint8) (val),
			(kal_uint8) (CON10_BOOST_LIM_MASK),
			(kal_uint8) (CON10_BOOST_LIM_SHIFT)
		);
}

/* CON11---------------------------------------------------- */
kal_uint32 bq25890_get_vbus_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON11),
			(&val),
			(kal_uint8) (CON11_VBUS_STAT_MASK),
			(kal_uint8) (CON11_VBUS_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_chrg_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON11),
			(&val),
			(kal_uint8) (CON11_CHRG_STAT_MASK),
			(kal_uint8) (CON11_CHRG_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_pg_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON11),
			(&val),
			(kal_uint8) (CON11_PG_STAT_MASK),
			(kal_uint8) (CON11_PG_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_vsys_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON11),
			(&val),
			(kal_uint8) (CON11_VSYS_STAT_MASK),
			(kal_uint8) (CON11_VSYS_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_system_status(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON11),
			(&val),
			(kal_uint8) (0xFF),
			(kal_uint8) (0x0)
		);
	return val;
}

/* CON13---------------------------------------------------- */
void bq25890_set_force_vindpm(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON13),
			(kal_uint8) (val),
			(kal_uint8) (CON13_FORCE_VINDPM_MASK),
			(kal_uint8) (CON13_FORCE_VINDPM_SHIFT)
		);
}

void bq25890_set_vindpm(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON13),
			(kal_uint8) (val),
			(kal_uint8) (CON13_VINDPM_MASK),
			(kal_uint8) (CON13_VINDPM_SHIFT)
		);
}

/* CON19---------------------------------------------------- */
kal_uint32 bq25890_get_vdpm_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON19),
			(&val),
			(kal_uint8) (CON19_VDPM_STAT_MASK),
			(kal_uint8) (CON19_VDPM_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_idpm_stat(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON19),
			(&val),
			(kal_uint8) (CON19_IDPM_STAT_MASK),
			(kal_uint8) (CON19_IDPM_STAT_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_current_iinlim(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON19),
			(&val),
			(kal_uint8) (CON19_IDPM_LIM_MASK),
			(kal_uint8) (CON19_IDPM_LIM_SHIFT)
		);
	return val;
}

/* CON20---------------------------------------------------- */
void bq25890_set_reg_rst(kal_uint32 val)
{
	bq25890_config_interface((kal_uint8) (bq25890_CON20),
			(kal_uint8) (val),
			(kal_uint8) (CON20_REG_RST_MASK),
			(kal_uint8) (CON20_REG_RST_SHIFT)
		);
}

kal_uint32 bq25890_get_pn(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON20),
			(&val),
			(kal_uint8) (CON20_PN_MASK),
			(kal_uint8) (CON20_PN_SHIFT)
		);
	return val;
}

kal_uint32 bq25890_get_rev(void)
{
	kal_uint8 val = 0;
	bq25890_read_interface((kal_uint8) (bq25890_CON20),
			(&val),
			(kal_uint8) (CON20_REV_MASK),
			(kal_uint8) (CON20_REV_SHIFT)
		);
	return val;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/

static kal_uint32 charging_hw_init(void *data)
{
	kal_uint32 status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1);	/* BC11_BB_CTRL */
	upmu_set_rg_bc11_rst(1);	/* BC11_RST */

	bq25890_set_en_hiz(0x0);
	bq25890_set_force_vindpm(0x1); /* Run absolute VINDPM */
	bq25890_set_vindpm(0x14);	/* VIN DPM check 4.60V */
	bq25890_set_reg_rst(0x0);
	bq25890_set_wdt_rst(0x1);	/* Kick watchdog */
	bq25890_set_sys_min(0x5);	/* Minimum system voltage 3.5V */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_iprechg(0x3);	/* Precharge current 256mA */
#else
	bq25890_set_iprechg(0x7);	/* Precharge current 512mA */
#endif
	bq25890_set_iterm(0x1);	/* Termination current 128mA */

#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_vreg(0x17);	/* VREG 4.208V */
#endif
	bq25890_set_batlowv(0x1);	/* BATLOWV 3.0V */
	bq25890_set_vrechg(0x0);	/* VRECHG 0.1V (4.108V) */
	bq25890_set_en_term(0x1);	/* Enable termination */
	bq25890_set_watchdog(0x1);	/* WDT 40s */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_en_timer(0x0);	/* Disable charge timer */
#endif

	return status;
}

static kal_uint32 charging_dump_register(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_dump_register\r\n");

	bq25890_dump_register();

	return status;
}

static kal_uint32 charging_enable(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32 *) (data);

	if (KAL_TRUE == enable) {
		bq25890_set_en_hiz(0x0);
		bq25890_set_chg_config(0x1);
	} else
		bq25890_set_chg_config(0x0);

	return status;
}

static kal_uint32 charging_set_cv_voltage(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint16 register_value;
	kal_uint32 cv_value = *(kal_uint32 *) (data);

	if (cv_value == BATTERY_VOLT_04_200000_V)
		cv_value = 4208000; /* use nearest value */

	if (cv_value <= 384000)
		cv_value = 384000;

	register_value = (cv_value/100 - 3840)/16;
	bq25890_set_vreg(register_value);

	return status;
}

static kal_uint32 charging_get_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 data_val = 0;

	data_val = bq25890_get_ichg() * 64;

	/* mA, might need to change to CHR_CURRENT_ENUM */
	*(kal_uint32 *) data = data_val;

	return status;
}

static kal_uint32 charging_set_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 register_value;
	kal_uint32 current_value = (*(kal_uint32 *) data)/100;
	register_value = current_value/64;

	if (register_value > 0x4F)
		register_value = 0x4F;

	bq25890_set_ichg(register_value);
	return status;
}


static kal_uint32 charging_set_input_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 register_value;

	set_chr_current = (*(kal_uint32 *) data)/100;
	register_value = (set_chr_current-100)/50;

	if (register_value > 0x3F)
		register_value = 0x3F;

	bq25890_set_iinlim(register_value);
	return status;
}

static kal_uint32 charging_get_input_current(void *data)
{
	kal_uint32 register_value;
	register_value = bq25890_get_iinlim();
	*(kal_uint32 *) data = (100 + 50*register_value)*100; /* to CHR_CURRENT_ENUM */
	return STATUS_OK;
}

static kal_uint32 charging_get_charging_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 ret_val;

	ret_val = bq25890_get_chrg_stat();

	if (ret_val == 0x3)
		*(kal_uint32 *) data = KAL_TRUE;
	else
		*(kal_uint32 *) data = KAL_FALSE;

	return status;
}

static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_FULL, "charging_reset_watch_dog_timer\r\n");
	bq25890_set_wdt_rst(0x1);	/* Kick watchdog */
	return status;
}

static kal_uint32 charging_set_hv_threshold(void *data)
{
	return STATUS_OK;
}

static kal_uint32 charging_get_hv_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_bool *) (data) = upmu_get_rgs_vcdt_hv_det();
	return status;
}

static kal_uint32 charging_get_battery_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	/* upmu_set_baton_tdet_en(1); */
	upmu_set_rg_baton_en(1);
	*(kal_bool *) (data) = upmu_get_rgs_baton_undet();

	return status;
}


static kal_uint32 charging_get_charger_det_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_bool *) (data) = upmu_get_rgs_chrdet();

	return status;
}

static kal_uint32 charging_get_charger_type(void *data)
{
	kal_uint32 status = STATUS_OK;
	CHARGER_TYPE charger_type = CHARGER_UNKNOWN;
	kal_uint32 ret_val;
	kal_uint32 vbus_state;
	kal_uint8 reg_val = 0;
	kal_uint32 count = 0;

#if 0				/*defined(CONFIG_POWER_EXT) */
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else

	charging_type_det_done = KAL_FALSE;

	battery_xlog_printk(BAT_LOG_CRTI, "use BQ25890 charger detection\r\n");

	Charger_Detect_Init();

	while (bq25890_get_pg_stat() == 0) {
		battery_xlog_printk(BAT_LOG_CRTI, "wait pg_state ready.\n");
		count++;
		msleep(20);
		if (count > 500) {
			pr_warn("wait BQ25890 pg_state ready timeout!\n");
			break;
		}

		if (!upmu_get_rgs_chrdet())
			break;
	}

	ret_val = bq25890_get_vbus_stat();

	/* if detection is not finished */
	if (ret_val == 0x0) {
		count = 0;
		bq25890_set_force_dpdm(1);
		while (bq25890_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			battery_xlog_printk(BAT_LOG_CRTI, "polling BQ25890 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	vbus_state = bq25890_get_vbus_stat();

	/* We might not be able to switch on RG_USB20_BC11_SW_EN in time. */
	/* We detect again to confirm its type */
	if (upmu_get_rgs_chrdet()) {
		count = 0;
		bq25890_set_force_dpdm(1);
		while (bq25890_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			battery_xlog_printk(BAT_LOG_CRTI,
					    "polling again BQ25890 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	ret_val = bq25890_get_vbus_stat();

	if (ret_val != vbus_state)
		pr_warn("Update VBUS state from %d to %d!\n", vbus_state, ret_val);

	switch (ret_val) {
	case 0x0:
		charger_type = CHARGER_UNKNOWN;
		break;
	case 0x1:
		charger_type = STANDARD_HOST;
		break;
	case 0x2:
		charger_type = CHARGING_HOST;
		break;
	case 0x3:
		charger_type = STANDARD_CHARGER;
		break;
	case 0x4:
		charger_type = STANDARD_CHARGER; /* MaxCharge DCP */
		break;
	case 0x5:
		charger_type = NONSTANDARD_CHARGER;
		break;
	case 0x6:
		charger_type = NONSTANDARD_CHARGER;
		break;
	case 0x7:
		charger_type = CHARGER_UNKNOWN; /* OTG */
		break;

	default:
		charger_type = CHARGER_UNKNOWN;
		break;
	}

	if (charger_type == NONSTANDARD_CHARGER) {
		reg_val = bq25890_get_iinlim();
		if (reg_val < 0x12) {
			battery_xlog_printk(BAT_LOG_CRTI,
					    "Set to Non-standard charger due to 1A input limit.\r\n");
			charger_type = NONSTANDARD_CHARGER;
		} else if (reg_val == 0x12) {	/* 1A charger */
			battery_xlog_printk(BAT_LOG_CRTI, "Set to APPLE_1_0A_CHARGER.\r\n");
			charger_type = APPLE_1_0A_CHARGER;
		} else if (reg_val >= 0x26) {	/* 2A/2.1A/2.4A charger */
			battery_xlog_printk(BAT_LOG_CRTI, "Set to APPLE_2_1A_CHARGER.\r\n");
			charger_type = APPLE_2_1A_CHARGER;
		}
	}

	Charger_Detect_Release();

	pr_warn("charging_get_charger_type = %d\n", charger_type);

	*(CHARGER_TYPE *) (data) = charger_type;

	charging_type_det_done = KAL_TRUE;
#endif
	return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
	kal_uint32 status = STATUS_OK;

	if (slp_get_wake_reason() == WR_PCM_TIMER)
		*(kal_bool *) (data) = KAL_TRUE;
	else
		*(kal_bool *) (data) = KAL_FALSE;

	battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());

	return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");

#if 0				/* need porting of orderly_reboot(). */
	if (system_state == SYSTEM_BOOTING)
		arch_reset(0, NULL);
	else
		orderly_reboot(true);
#endif
	arch_reset(0, NULL);

	return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_uint32 *) (data) = get_boot_mode();

	battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static kal_uint32 charging_enable_powerpath(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32 *) (data);

	if (KAL_TRUE == enable)
		bq25890_set_en_hiz(0x0);
	else
		bq25890_set_en_hiz(0x1);

	return status;
}

static kal_uint32 charging_boost_enable(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32 *) (data);

	if (KAL_TRUE == enable) {
		bq25890_set_boost_lim(0x2);	/* 1.1A on VBUS */
		bq25890_set_en_hiz(0x0);
		bq25890_set_chg_config(0);	/* Charge disabled */
		bq25890_set_otg_config(0x1);	/* OTG */
#ifdef CONFIG_POWER_EXT
		bq25890_set_watchdog(0);
#endif
	} else {
		bq25890_set_otg_config(0x0);	/* OTG & Charge disabled */
#ifdef CONFIG_POWER_EXT
		bq25890_set_watchdog(1);
#endif
	}

	return status;
}

static kal_uint32(*const charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
			charging_hw_init,
	    charging_dump_register,
	    charging_enable,
	    charging_set_cv_voltage,
	    charging_get_current,
	    charging_set_current,
	    charging_get_input_current,
	    charging_set_input_current,
	    charging_get_charging_status,
	    charging_reset_watch_dog_timer,
	    charging_set_hv_threshold,
	    charging_get_hv_status,
	    charging_get_battery_status,
	    charging_get_charger_det_status,
	    charging_get_charger_type,
	    charging_get_is_pcm_timer_trigger,
	    charging_set_platform_reset,
	    charging_get_platfrom_boot_mode,
	    charging_enable_powerpath,
	    charging_boost_enable
};

kal_int32 bq25890_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	kal_int32 status;
	if (cmd < CHARGING_CMD_NUMBER)
		status = charging_func[cmd] (data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}

void bq25890_dump_register(void)
{
	int i = 0;
	for (i = 0; i < bq25890_REG_NUM; i++) {
		bq25890_read_byte(i, &bq25890_reg[i]);
		battery_xlog_printk(BAT_LOG_FULL,
				    "[bq25890_dump_register] Reg[0x%X]=0x%X\n", i, bq25890_reg[i]);
	}
}

kal_uint8 bq25890_get_reg12_fault_type(kal_uint8 reg12_fault)
{
	kal_uint8 ret = 0;

	if ((reg12_fault & (CON12_OTG_FAULT_MASK << CON12_OTG_FAULT_SHIFT)) != 0) {
		ret = BQ_OTG_FAULT;
	} else if ((reg12_fault & (CON12_CHRG_FAULT_MASK << CON12_CHRG_FAULT_SHIFT)) != 0) {
		if ((reg12_fault & (CON12_CHRG_INPUT_FAULT_MASK << CON12_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_INPUT_FAULT;
		else if ((reg12_fault &
			  (CON12_CHRG_THERMAL_SHUTDOWN_FAULT_MASK << CON12_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if ((reg12_fault &
			  (CON12_CHRG_TIMER_EXPIRATION_FAULT_MASK << CON12_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if ((reg12_fault & (CON12_BAT_FAULT_MASK << CON12_BAT_FAULT_SHIFT)) != 0)
		ret = BQ_BAT_FAULT;
	else if ((reg12_fault & (CON12_NTC_FAULT_MASK << CON12_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_FAULT;

	return ret;
}

void bq25890_polling_reg12(void)
{
	int i, i2;
	kal_uint8 reg1;

	for (i2 = i = 0; i < 4 && i2 < 10; i++, i2++) {
		bq25890_read_byte((kal_uint8) (bq25890_CON12), &bq25890_reg[bq25890_CON12]);
		if ((bq25890_reg[bq25890_CON12] & 0x40) != 0) {	/* OTG_FAULT bit */
			/* Disable OTG */
			bq25890_read_byte(bq25890_CON3, &reg1);
			reg1 &= ~0x20;	/* 0 = OTG Disable */
			bq25890_write_byte(bq25890_CON3, reg1);
			msleep(1);
			/* Enable OTG */
			reg1 |= 0x20;	/* 1 = OTG Enable */
			bq25890_write_byte(bq25890_CON3, reg1);
		}
		if (bq25890_reg[bq25890_CON12] != 0) {
			i = 0;	/* keep on polling if reg9 is not 0. This is to filter noises */
			/* need filter fault type here */
			switch_set_state(&bq25890_fault_reg,
					 bq25890_get_reg12_fault_type(bq25890_reg[bq25890_CON12]));
		}
		msleep(10);
	}
	/* send normal fault state to UI */
	switch_set_state(&bq25890_fault_reg, BQ_NORMAL_FAULT);
}

static irqreturn_t ops_bq25890_int_handler(int irq, void *dev_id)
{
	bq25890_polling_reg12();
	return IRQ_HANDLED;
}

static int bq25890_driver_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_info("[bq25890_driver_suspend] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);

	return 0;
}

static int bq25890_driver_resume(struct i2c_client *client)
{
	pr_info("[bq25890_driver_resume] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		enable_irq(client->irq);

	return 0;
}

static void bq25890_driver_shutdown(struct i2c_client *client)
{
	pr_info("[bq25890_driver_shutdown] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);
}

#ifdef CONFIG_OF
static void bq_populate_pdata(struct i2c_client *client)
{
	struct device_node *of_node = client->dev.of_node;
	if (!of_node) {
		of_node = of_find_compatible_node(NULL, NULL, "ti,bq25890");

		if (!of_node) {
			pr_warn("can't find node for bq25890!\n");
			return;
		} else
			pr_info("find bq25890 node by name!\n");
	}

	charging_int_gpio = of_get_gpio(of_node, 0);
}
#endif

static int bq25890_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct regulator *i2c_reg = devm_regulator_get(&client->dev, "reg-i2c");

	pr_info("[bq25890_driver_probe]\n");

	new_client = client;

	if (!IS_ERR(i2c_reg)) {

		ret = regulator_set_voltage(i2c_reg, 1800000, 1800000);
		if (ret != 0)
			dev_err(&client->dev, "Fail to set 1.8V to reg-i2c: %d\n", ret);

		ret = regulator_get_voltage(i2c_reg);
		pr_info("bq25890 i2c voltage: %d\n", ret);

		ret = regulator_enable(i2c_reg);
		if (ret != 0)
			dev_err(&client->dev, "Fail to enable reg-i2c: %d\n", ret);
	}

	if (bq25890_get_pn() == 0x2) {
		pr_notice("BQ25890 device is found. register charger control.\n");
		bat_charger_register(bq25890_control_interface);
	} else {
		pr_notice("No BQ25890 device part number is found.\n");
		return 0;
	}

	bq25890_dump_register();

	bat_charger_register(bq25890_control_interface);

	bq25890_fault_reg.name = "bq25890_fault_reg";
	bq25890_fault_reg.index = 0;
	bq25890_fault_reg.state = 0;
	ret = switch_dev_register(&bq25890_fault_reg);

	if (ret < 0)
		pr_err("[bq25890_driver_probe] switch_dev_register() error(%d)\n", ret);

#ifdef CONFIG_OF
	bq_populate_pdata(client);
#endif

	if (client->irq > 0) {

		pr_notice("[bq25890_driver_probe] enable interrupt: %d\n", client->irq);
		/* make sure we clean REG12 before enable fault interrupt */
		bq25890_read_byte((kal_uint8) (bq25890_CON12), &bq25890_reg[bq25890_CON12]);
		if (bq25890_reg[bq25890_CON12] != 0)
			bq25890_polling_reg12();

		ret = request_threaded_irq(client->irq, NULL, ops_bq25890_int_handler,
					   IRQF_TRIGGER_FALLING |
					   IRQF_ONESHOT, "ops_bq25890_int_handler", NULL);
		if (ret)
			pr_err
			    ("[bq25890_driver_probe] fault interrupt registration failed err = %d\n",
			     ret);
	}

	return 0;
}

static const struct i2c_device_id bq25890_i2c_id[] = { {"bq25890", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id bq25890_id[] = {
	{.compatible = "ti,bq25890"},
	{},
};

MODULE_DEVICE_TABLE(of, bq25890_id);
#endif

static struct i2c_driver bq25890_driver = {
	.driver = {
		   .name = "bq25890",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(bq25890_id),
#endif
		   },
	.probe = bq25890_driver_probe,
	.shutdown = bq25890_driver_shutdown,
	.suspend = bq25890_driver_suspend,
	.resume = bq25890_driver_resume,
	.id_table = bq25890_i2c_id,
};

static ssize_t show_bq25890_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("[show_bq25890_access] 0x%x\n", g_reg_value_bq25890);
	return sprintf(buf, "0x%x\n", g_reg_value_bq25890);
}

static ssize_t store_bq25890_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[store_bq25890_access]\n");

	if (buf != NULL && size != 0) {
		pr_info("[store_bq25890_access] buf is %s and size is %d\n", buf, (int)size);
		reg_address = simple_strtoul(buf, &pvalue, 16);

		if (size > 3) {
			reg_value = simple_strtoul((pvalue + 1), NULL, 16);
			pr_info("[store_bq25890_access] write bq25890 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = bq25890_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = bq25890_read_interface(reg_address, &g_reg_value_bq25890, 0xFF, 0x0);
			pr_info("[store_bq25890_access] read bq25890 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq25890);
			pr_info
			    ("[store_bq25890_access] Please use \"cat bq25890_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq25890_access, S_IWUSR | S_IRUGO, show_bq25890_access, store_bq25890_access);

static int bq25890_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("bq25890_user_space_probe!\n");
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq25890_access);

	return 0;
}

struct platform_device bq25890_user_space_device = {
	.name = "bq25890-user",
	.id = -1,
};

static struct platform_driver bq25890_user_space_driver = {
	.probe = bq25890_user_space_probe,
	.driver = {
		   .name = "bq25890-user",
		   },
};

static int __init bq25890_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&bq25890_driver) != 0)
		pr_err("[bq25890_init] failed to register bq25890 i2c driver.\n");
	else
		pr_info("[bq25890_init] Success to register bq25890 i2c driver.\n");

	/* bq25890 user space access interface */
	ret = platform_device_register(&bq25890_user_space_device);
	if (ret) {
		pr_err("[bq25890_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq25890_user_space_driver);
	if (ret) {
		pr_err("[bq25890_init] Unable to register driver (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit bq25890_exit(void)
{
	i2c_del_driver(&bq25890_driver);
}
module_init(bq25890_init);
module_exit(bq25890_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25890 Driver");
MODULE_AUTHOR("MengHui Lin<menghui.lin@mediatek.com>");
