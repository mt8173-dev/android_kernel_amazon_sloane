/* system header files */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/mtd/nand.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/atomic.h>

#include <mach/system.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_bt.h>
#include <mach/eint.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_typedefs.h>
#include "board-custom.h"
#include <mach/upmu_hw.h>
#include <mach/pmic_sw.h>
#include <mach/battery_common.h>

#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
static msdc_sdio_irq_handler_t mtk_wcn_cmb_sdio_eirq_handler;
int mtk_wcn_sdio_irq_flag_set(int falg);
static atomic_t sdio_irq_enable_flag;
static msdc_pm_callback_t mtk_wcn_cmb_sdio_pm_cb;
static void *mtk_wcn_cmb_sdio_pm_data;
static void *mtk_wcn_cmb_sdio_eirq_data;

static const struct mtk_wifi_sdio_data *mtk_wifi_sdio_data;
/*
index: port number of combo chip (1:SDIO1, 2:SDIO2, no SDIO0)
value: slot power status of  (0:off, 1:on, 0xFF:invalid)
*/
#if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
static unsigned char combo_port_pwr_map[4] = { 0x0, 0xFF, 0xFF, 0xFF };
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
static unsigned char combo_port_pwr_map[4] = { 0xFF, 0x0, 0xFF, 0xFF };
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
static unsigned char combo_port_pwr_map[4] = { 0xFF, 0xFF, 0x0, 0xFF };
#elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
static unsigned char combo_port_pwr_map[4] = { 0xFF, 0xFF, 0xFF, 0x0 };
#else
#error "unsupported CONFIG_MTK_WCN_CMB_SDIO_SLOT" CONFIG_MTK_WCN_CMB_SDIO_SLOT
#endif
#else
static msdc_sdio_irq_handler_t mt_wifi_irq_handler;
static pm_message_t mt_wifi_pm_state = {.event = PM_EVENT_HIBERNATE };

static msdc_pm_callback_t mt_wifi_pm_cb;
static void *mt_wifi_pm_data;
static void *mt_wifi_irq_data;
static int mt_wifi_pm_late_cb;
#endif

extern void bq24297_set_watchdog(kal_uint32 val);
extern void bq24297_set_batfet_disable(kal_uint32 val);

/*=======================================================================*/
/* Board Specific Devices Power Management                               */
/*=======================================================================*/
void mt_power_off(void)
{
	int ret_val = 0;
	int reg_val = 0;
	int bbpu_down = 0;

	pr_info("mt_power_off\n");

	/* Enable CA15 by default for different PMIC behavior */
	pmic_config_interface(VCA15_CON7, 0x1, PMIC_VCA15_EN_MASK, PMIC_VCA15_EN_SHIFT);
	pmic_config_interface(VSRMCA15_CON7, 0x1, PMIC_VSRMCA15_EN_MASK, PMIC_VSRMCA15_EN_SHIFT);
	udelay(200);

	ret_val = pmic_read_interface(VCA15_CON7, &reg_val, 0xFFFF, 0);
	pr_info("Reg[0x%x]=0x%x\n", VCA15_CON7, reg_val);
	ret_val = pmic_read_interface(VSRMCA15_CON7, &reg_val, 0xFFFF, 0);
	pr_info("Reg[0x%x]=0x%x\n", VSRMCA15_CON7, reg_val);

	bq24297_set_watchdog(0x0);
	udelay(200);
	bq24297_set_batfet_disable(0x1);
#ifdef CONFIG_MTK_AUTO_POWER_ON_WITH_CHARGER
	if (pmic_chrdet_status() == KAL_TRUE)
		rtc_mark_enter_kpoc();
#endif

	/* pull PWRBB low */
	if (pmic_chrdet_status() == KAL_FALSE) {
		bbpu_down = 1;
		rtc_bbpu_power_down();
	}

	while (1) {
		pr_info("check charger\n");
		if (pmic_chrdet_status() == KAL_TRUE) {
#ifdef CONFIG_MTK_AUTO_POWER_ON_WITH_CHARGER
			arch_reset(0, "enter_kpoc");
#else
			arch_reset(0, "charger");
#endif
		} else if (bbpu_down == 0) {
			bbpu_down = 1;
			rtc_bbpu_power_down();
		}
	}
}

/*=======================================================================*/
/* Board Specific Devices                                                */
/*=======================================================================*/
/*GPS driver*/
/*FIXME: remove mt3326 notation */
struct mt3326_gps_hardware mt3326_gps_hw = {
	.ext_power_on = NULL,
	.ext_power_off = NULL,
};

/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

#ifdef CONFIG_MTK_BT_SUPPORT
void mt_bt_power_on(void)
{
	pr_info("+mt_bt_power_on\n");

#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
	/* combo chip product */
	/*
	 * Ignore rfkill0/state call. Controll BT power on/off through device /dev/stpbt.
	 */
#else
	/* standalone product */
#endif

	pr_info("-mt_bt_power_on\n");
}
EXPORT_SYMBOL(mt_bt_power_on);

void mt_bt_power_off(void)
{
	pr_info("+mt_bt_power_off\n");

#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
	/* combo chip product */
	/*
	 * Ignore rfkill0/state call. Controll BT power on/off through device /dev/stpbt.
	 */
#else
	/* standalone product */
#endif

	pr_info("-mt_bt_power_off\n");
}
EXPORT_SYMBOL(mt_bt_power_off);

int mt_bt_suspend(pm_message_t state)
{
	pr_info("+mt_bt_suspend\n");
	pr_info("-mt_bt_suspend\n");
	return MT_BT_OK;
}

int mt_bt_resume(pm_message_t state)
{
	pr_info("+mt_bt_resume\n");
	pr_info("-mt_bt_resume\n");
	return MT_BT_OK;
}
#endif


#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
static void mtk_wcn_cmb_sdio_enable_eirq(void)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;

	if (!pdata)
		return;

	mt_eint_unmask(mt_gpio_to_eint(pdata->irq.pin));
}

static void mtk_wcn_cmb_sdio_disable_eirq(void)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;

	if (!pdata)
		return;

	mt_eint_mask(mt_gpio_to_eint(pdata->irq.pin));
}

static void mtk_wcn_cmb_sdio_eirq_handler_stub(void)
{
	if ((NULL != mtk_wcn_cmb_sdio_eirq_handler) &&(0 != atomic_read(&sdio_irq_enable_flag)))
		mtk_wcn_cmb_sdio_eirq_handler(mtk_wcn_cmb_sdio_eirq_data);
}

int mtk_wcn_sdio_irq_flag_set(int flag)
{

	if (0 != flag)
		atomic_set(&sdio_irq_enable_flag, 1);
	else
		atomic_set(&sdio_irq_enable_flag, 0);

	pr_debug("sdio_irq_enable_flag:%d\n", atomic_read(&sdio_irq_enable_flag));

	return atomic_read(&sdio_irq_enable_flag);
}
EXPORT_SYMBOL(mtk_wcn_sdio_irq_flag_set);

static void mtk_wcn_cmb_sdio_request_eirq(msdc_sdio_irq_handler_t irq_handler, void *data)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;
	int eint;

	if (!pdata)
		return;

	eint = mt_gpio_to_eint(pdata->irq.pin);

	mtk_wcn_sdio_irq_flag_set(0);
	mtk_wcn_cmb_sdio_eirq_data = data;
	mtk_wcn_cmb_sdio_eirq_handler = irq_handler;
	mt_pin_set_pull(pdata->irq.pin, MT_PIN_PULL_ENABLE_UP);
	mt_eint_registration(eint, pdata->irq.flags,
		mtk_wcn_cmb_sdio_eirq_handler_stub, 0);
	mt_eint_mask(eint);
}

static void mtk_wcn_cmb_sdio_register_pm(msdc_pm_callback_t pm_cb, void *data)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;

	if (!pdata)
		return;

	pr_debug("mtk_wcn_cmb_sdio_register_pm (0x%p, 0x%p)\n", pm_cb, data);
	/* register pm change callback */
	mtk_wcn_cmb_sdio_pm_cb = pm_cb;
	mtk_wcn_cmb_sdio_pm_data = data;
}

static void mtk_wcn_cmb_sdio_on(int sdio_port_num)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;
	pm_message_t state = {.event = PM_EVENT_USER_RESUME };

	pr_debug("mtk_wcn_cmb_sdio_on (%d)\n", sdio_port_num);

	if (!pdata)
		return;

	/* 1. disable sdio eirq */
	mtk_wcn_cmb_sdio_disable_eirq();
	mt_pin_set_pull(pdata->irq.pin, MT_PIN_PULL_DISABLE);
	mt_pin_set_mode_eint(pdata->irq.pin);

	/* 2. call sd callback */
	if (mtk_wcn_cmb_sdio_pm_cb) {
		/* pr_info("mtk_wcn_cmb_sdio_pm_cb(PM_EVENT_USER_RESUME, 0x%p, 0x%p)\n",
		mtk_wcn_cmb_sdio_pm_cb, mtk_wcn_cmb_sdio_pm_data); */
		mtk_wcn_cmb_sdio_pm_cb(state, mtk_wcn_cmb_sdio_pm_data);
	} else {
		pr_warn("mtk_wcn_cmb_sdio_on no sd callback!!\n");
	}
}

static void mtk_wcn_cmb_sdio_off(int sdio_port_num)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;
	pm_message_t state = {.event = PM_EVENT_USER_SUSPEND };

	pr_debug("mtk_wcn_cmb_sdio_off (%d)\n", sdio_port_num);

	if (!pdata)
		return;

	/* 1. call sd callback */
	if (mtk_wcn_cmb_sdio_pm_cb) {
		/* pr_info("mtk_wcn_cmb_sdio_off(PM_EVENT_USER_SUSPEND, 0x%p, 0x%p)\n",
		mtk_wcn_cmb_sdio_pm_cb, mtk_wcn_cmb_sdio_pm_data); */
		mtk_wcn_cmb_sdio_pm_cb(state, mtk_wcn_cmb_sdio_pm_data);
	} else {
		pr_warn("mtk_wcn_cmb_sdio_off no sd callback!!\n");
	}

	/* 2. disable sdio eirq */
	mtk_wcn_cmb_sdio_disable_eirq();
	/*pr_info("[mt6620] set WIFI_EINT input pull down\n"); */
	mt_pin_set_mode_gpio(pdata->irq.pin);
	gpio_direction_input(pdata->irq.pin);
	mt_pin_set_pull(pdata->irq.pin, MT_PIN_PULL_ENABLE_UP);
}

void mtk_wifi_sdio_set_data(const struct mtk_wifi_sdio_data *pdata)
{
	if (pdata && pdata->irq.valid && !mtk_wifi_sdio_data) {
		int err = gpio_request(pdata->irq.pin, "WIFI-SDIO-IRQ");
		if (!err)
			mtk_wifi_sdio_data = pdata;
		else
			pr_err("%s: filed to request WiFi SDIO IRQ GPIO%d: err=%d\n",
				__func__, pdata->irq.pin, err);
	}
}

int board_sdio_ctrl(unsigned int sdio_port_num, unsigned int on)
{
	const struct mtk_wifi_sdio_data *pdata = mtk_wifi_sdio_data;
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
	sdio_port_num = CONFIG_MTK_WCN_CMB_SDIO_SLOT;
	pr_debug("mt_combo_sdio_ctrl: force set sdio port to (%d)\n", sdio_port_num);
#endif
	if ((sdio_port_num >= 4) || (combo_port_pwr_map[sdio_port_num] == 0xFF)) {
		/* invalid sdio port number or slot mapping */
		pr_warn("mt_mtk_wcn_cmb_sdio_ctrl invalid port(%d, %d)\n", sdio_port_num,
			combo_port_pwr_map[sdio_port_num]);
		return -1;
	}
	/*pr_info("mt_mtk_wcn_cmb_sdio_ctrl (%d, %d)\n", sdio_port_num, on); */

	if (!combo_port_pwr_map[sdio_port_num] && on) {
		pr_debug("board_sdio_ctrl force off before on\n");
		mtk_wcn_cmb_sdio_off(sdio_port_num);
		combo_port_pwr_map[sdio_port_num] = 0;
		/* off -> on */
		mtk_wcn_cmb_sdio_on(sdio_port_num);
		combo_port_pwr_map[sdio_port_num] = 1;
	} else if (combo_port_pwr_map[sdio_port_num] && !on) {
		/* on -> off */
		mtk_wcn_cmb_sdio_off(sdio_port_num);
		combo_port_pwr_map[sdio_port_num] = 0;
	} else {
		return -2;
	}
	if (pdata)
		irq_set_irq_wake(gpio_to_irq(pdata->irq.pin), on);
	return 0;
}
EXPORT_SYMBOL(board_sdio_ctrl);


#endif				/* end of defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) */

#if defined(CONFIG_WLAN)
#if !defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
static void mt_wifi_enable_irq(void)
{
	const struct mtk_wifi_sdio_custom *pdata = mtk_wifi_sdio_data;

	mt_eint_unmask(pdata->num);
}

static void mt_wifi_disable_irq(void)
{
	const struct mtk_wifi_sdio_custom *pdata = mtk_wifi_sdio_data;

	mt_eint_mask(pdata->num);
}

static void mt_wifi_eirq_handler(void)
{
	if (mt_wifi_irq_handler) {
		mt_wifi_irq_handler(mt_wifi_irq_data);
	}
}

static void mt_wifi_request_irq(msdc_sdio_irq_handler_t irq_handler, void *data)
{
	const struct mtk_wifi_sdio_custom *pdata = mtk_wifi_sdio_data;

	if (!pdata)
		return;

	if (pdata->debounce_en)
		gpio_set_debounce(pdata->pin, pdata->debounce_cn);
	mt_eint_registration(pdata->num,
			     pdata->type,
			     mt_wifi_eirq_handler, 0);
	mt_eint_mask(pdata->num);

	mt_wifi_irq_handler = irq_handler;
	mt_wifi_irq_data = data;
}

static void mt_wifi_register_pm(msdc_pm_callback_t pm_cb, void *data)
{
	const struct mtk_wifi_sdio_custom *pdata = mtk_wifi_sdio_data;

	if (!pdata)
		return;

	/* register pm change callback */
	mt_wifi_pm_cb = pm_cb;
	mt_wifi_pm_data = data;
}

#endif				/* end of !defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) */

int mt_wifi_resume(pm_message_t state)
{
	int evt = state.event;

	if (evt != PM_EVENT_USER_RESUME && evt != PM_EVENT_RESUME) {
		return -1;
	}

	/*pr_info("[WIFI] %s Resume\n", evt == PM_EVENT_RESUME ? "PM":"USR"); */

#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
	/* combo chip product: notify combo driver to turn on Wi-Fi */

	/* Use new mtk_wcn_cmb_stub APIs instead of old mt_combo ones */
	mtk_wcn_cmb_stub_func_ctrl(COMBO_FUNC_TYPE_WIFI, 1);
	/*mt_combo_func_ctrl(COMBO_FUNC_TYPE_WIFI, 1); */

#endif

	return 0;
}

int mt_wifi_suspend(pm_message_t state)
{
	int evt = state.event;
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
	static int is_1st_suspend_from_boot = 1;
#endif

	if (evt != PM_EVENT_USER_SUSPEND && evt != PM_EVENT_SUSPEND) {
		return -1;
	}
#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
	/* combo chip product: notify combo driver to turn on Wi-Fi */
	if (is_1st_suspend_from_boot) {
		pm_message_t state = {.event = PM_EVENT_USER_SUSPEND };

		if (mtk_wcn_cmb_sdio_pm_cb) {
			is_1st_suspend_from_boot = 0;
			/*              *** IMPORTANT DEPENDENDY***
			   RFKILL: set wifi and bt suspend by default in probe()
			   MT6573-SD: sd host is added to MMC stack and suspend is ZERO by default
			   (which means NOT suspended).

			   When boot up, RFKILL will set wifi off and this function gets
			   called. In order to successfully resume wifi at 1st time, pm_cb here
			   shall be called once to let MT6573-SD do sd host suspend and remove
			   sd host from MMC. Then wifi can be turned on successfully.

			   Boot->SD host added to MMC (suspend=0)->RFKILL set wifi off
			   ->SD host removed from MMC (suspend=1)->RFKILL set wifi on
			 */
			pr_info("1st mt_wifi_suspend (PM_EVENT_USER_SUSPEND)\n");
			(*mtk_wcn_cmb_sdio_pm_cb) (state, mtk_wcn_cmb_sdio_pm_data);
		} else {
			pr_warn("1st mt_wifi_suspend but no sd callback!!\n");
		}
	} else {
		/* combo chip product, notify combo driver */

		/* Use new mtk_wcn_cmb_stub APIs instead of old mt_combo ones */
		mtk_wcn_cmb_stub_func_ctrl(COMBO_FUNC_TYPE_WIFI, 0);
		/*mt_combo_func_ctrl(COMBO_FUNC_TYPE_WIFI, 0); */
	}
#endif
#endif
	return 0;
}

void mt_wifi_power_on(void)
{
	pm_message_t state = {.event = PM_EVENT_USER_RESUME };

	(void)mt_wifi_resume(state);
}
EXPORT_SYMBOL(mt_wifi_power_on);

void mt_wifi_power_off(void)
{
	pm_message_t state = {.event = PM_EVENT_USER_SUSPEND };

	(void)mt_wifi_suspend(state);
}
EXPORT_SYMBOL(mt_wifi_power_off);

#endif				/* end of defined(CONFIG_WLAN) */
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
static const struct mtk_hdmi_data *mtk_hdmi_data;

void mtk_hdmi_set_data(const struct mtk_hdmi_data *pdata)
{
	if (pdata->pwr.valid)
		gpio_request(pdata->pwr.pin, "HDMI-PWR");
	mtk_hdmi_data = pdata;
}
void mt_hdmi_power_ctrl(bool fgen)
{
	const struct mtk_hdmi_data *pdata = mtk_hdmi_data;

	if (!pdata || !pdata->pwr.valid)
		return;

	mt_pin_set_mode_gpio(pdata->pwr.pin);
	gpio_direction_output(pdata->pwr.pin, (fgen) ? 1 : 0);
}
EXPORT_SYMBOL(mt_hdmi_power_ctrl);
#endif
/* Board Specific Devices                                                */
/*=======================================================================*/

/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

/*=======================================================================*/
/* Board Devices Capability                                              */
/*=======================================================================*/
#define MSDC_SDCARD_FLAG  (MSDC_SYS_SUSPEND | MSDC_REMOVABLE | MSDC_HIGHSPEED | MSDC_UHS1 | MSDC_DDR)
/* Please enable/disable SD card MSDC_CD_PIN_EN for customer request */
#define MSDC_SDIO_FLAG    (MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED|MSDC_REMOVABLE)
#define MSDC_EMMC_FLAG	  (MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 | MSDC_DDR)

#if defined(CFG_DEV_MSDC0)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
struct msdc_hw msdc0_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug*/
	/*MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,(this flag is for SD card) */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
	.request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
	.enable_sdio_eirq = mtk_wcn_cmb_sdio_enable_eirq,
	.disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
	.register_pm = mtk_wcn_cmb_sdio_register_pm,
};
#else
struct msdc_hw msdc0_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 4,
	.cmd_drv = 4,
	.dat_drv = 4,
	.data_pins = 8,
	.data_offset = 0,
	.flags = MSDC_EMMC_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_EMMC,
	.boot = MSDC_BOOT_EN,
};
#endif
#endif
#if defined(CFG_DEV_MSDC1)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
struct msdc_hw msdc1_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
	.request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
	.enable_sdio_eirq = mtk_wcn_cmb_sdio_enable_eirq,
	.disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
	.register_pm = mtk_wcn_cmb_sdio_register_pm,
};
#else

struct msdc_hw msdc1_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 5,
	.cmd_drv = 3,
	.dat_drv = 3,
	.clk_drv_sd_18 = 3,
	.cmd_drv_sd_18 = 3,
	.dat_drv_sd_18 = 3,
	.data_pins = 4,
	.data_offset = 0,
	.flags = MSDC_SDCARD_FLAG | MSDC_CD_PIN_EN | MSDC_REMOVABLE,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SD,
	.boot = 0,
	.cd_level = MSDC_CD_LOW,
};
#endif
#endif
#if defined(CFG_DEV_MSDC2)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
    /* MSDC2 settings for MT66xx combo connectivity chip */
struct msdc_hw msdc2_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	.flags = MSDC_SDIO_FLAG,/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
	.request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
	.enable_sdio_eirq = mtk_wcn_cmb_sdio_enable_eirq,
	.disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
	.register_pm = mtk_wcn_cmb_sdio_register_pm,
};
#else

struct msdc_hw msdc2_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 5,
	.cmd_drv = 3,
	.dat_drv = 3,
	.clk_drv_sd_18 = 3,
	.cmd_drv_sd_18 = 3,
	.dat_drv_sd_18 = 3,
	.data_pins = 4,
	.data_offset = 0,
	.flags = MSDC_SDCARD_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SD,
	.boot = 0,
	.cd_level = MSDC_CD_LOW,
};
#endif
#endif
#if defined(CFG_DEV_MSDC3)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
    /* MSDC3 settings for MT66xx combo connectivity chip */
struct msdc_hw msdc3_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 3,
	.cmd_drv = 3,
	.dat_drv = 3,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	.flags = MSDC_SDIO_FLAG,/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
	.request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
	.enable_sdio_eirq = mtk_wcn_cmb_sdio_enable_eirq,
	.disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
	.register_pm = mtk_wcn_cmb_sdio_register_pm,
};

#endif
#endif
#if defined(CFG_DEV_MSDC4)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
struct msdc_hw msdc4_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,(this flag is for SD card) */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
	.request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
	.enable_sdio_eirq = mtk_wcn_cmb_sdio_enable_eirq,
	.disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
	.register_pm = mtk_wcn_cmb_sdio_register_pm,
};
#else
struct msdc_hw msdc4_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 2,
	.cmd_drv = 2,
	.dat_drv = 2,
	.data_pins = 8,
	.data_offset = 0,
	.flags = MSDC_EMMC_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_EMMC,
	.boot = MSDC_BOOT_EN,
};
#endif
#endif

/* MT6575 NAND Driver */
#if defined(CONFIG_MTK_MTD_NAND)
struct mt6575_nand_host_hw mt6575_nand_hw = {
	.nfi_bus_width = 8,
	.nfi_access_timing = NFI_DEFAULT_ACCESS_TIMING,
	.nfi_cs_num = NFI_CS_NUM,
	.nand_sec_size = 512,
	.nand_sec_shift = 9,
	.nand_ecc_size = 2048,
	.nand_ecc_bytes = 32,
	.nand_ecc_mode = NAND_ECC_HW,
};
#endif


/************************* Vibrator Customization ****************************/
#include <cust_vibrator.h>

static struct vibrator_pdata vibrator_pdata = {
	.vib_timer = 30,
};

static struct platform_device vibrator_device = {
	.name = "mtk_vibrator",
	.id = -1,
	.dev = {
		.platform_data = &vibrator_pdata,
	},
};

static int __init vibr_init(void)
{
	int ret;

	ret = platform_device_register(&vibrator_device);
	if (ret) {
		pr_info("Unable to register vibrator device (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int vibr_exit(void)
{
	platform_device_unregister(&vibrator_device);

	return 0;
}
/*****************************************************************************/

static struct mtk_combo_data wmt_data;

static struct platform_device mtk_wmt_detect_device = {
	.name = "mtk-wmt-detect",
};

static struct platform_device mtk_wmt_device = {
	.name = "mtk-wmt",
};

int __init mtk_combo_init(struct mtk_wcn_combo_gpio *pin_info)
{
	if (!pin_info)
		return -EINVAL;
	wmt_data.pin_info = pin_info;
	mtk_wmt_detect_device.dev.platform_data = &pin_info->pwr;
	mtk_wmt_device.dev.platform_data = &wmt_data;
	platform_device_register(&mtk_wmt_detect_device);
	platform_device_register(&mtk_wmt_device);
	return 0;
}
static int __init board_common_init(void)
{
	vibr_init();

	return 0;
}

static void board_common_exit(void)
{
	vibr_exit();
}

module_init(board_common_init);
module_exit(board_common_exit);

