#include <mu3phy/mtk-phy.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_clkmgr.h>
#include <asm/io.h>
#include <mu3phy/mtk-phy-asic.h>
#include <mu3d/hal/mu3d_hal_osal.h>

struct u3p_project_regs {
	long dummy;
};
static struct u3p_dummy_regs g_dummy_regs;


#define RG_SSUSB_VUSB10_ON (1<<5)
#define RG_SSUSB_VUSB10_ON_OFST (5)

/*This "power on/initial" sequence refer to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
PHY_INT32 phy_init_soc_dummy(struct u3phy_info *info)
{
	os_printk(K_INFO, "%s+\n", __func__);
	os_printk(K_INFO, "%s-\n", __func__);
	return PHY_TRUE;
}

PHY_INT32 u2_slew_rate_calibration_dummy(struct u3phy_info *info)
{
	return 0;
}

/*This "save current" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_savecurrent_dummy(struct u3phy_info *info, unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);
	os_printk(K_INFO, "%s-\n", __func__);
}

/*This "recovery" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_recover_dummy(struct u3phy_info *info, unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	os_printk(K_INFO, "%s-\n", __func__);
}

/* BC1.2 */
void Charger_Detect_Init(void)
{
	os_printk(K_INFO, "%s+\n", __func__);
	os_printk(K_INFO, "%s-\n", __func__);
}

void Charger_Detect_Release(void)
{
	os_printk(K_INFO, "%s+\n", __func__);
	os_printk(K_INFO, "%s-\n", __func__);
}

static const struct u3phy_operator u3p_project_ops = {
	.init = phy_init_soc_dummy,
	.u2_slew_rate_calibration = u2_slew_rate_calibration_dummy,
	.usb_phy_savecurrent = usb_phy_savecurrent_dummy,
	.usb_phy_recover = usb_phy_recover_dummy,
};

int u3p_project_init(struct u3phy_info *info)
{
	g_u3p_regs.dummy = 0x0;
	info->reg_info = (void *)&g_u3p_regs;
	info->u3p_ops = &u3p_project_ops;
}
