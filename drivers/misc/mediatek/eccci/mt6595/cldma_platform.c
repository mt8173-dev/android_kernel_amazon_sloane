#include <linux/platform_device.h>
#include <linux/interrupt.h>
#ifndef MT6290
#include <mach/mt_spm_sleep.h>
#endif
#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"

#ifdef MT6290
int md_power_on(int id)
{
	return 0;
}

int md_power_off(int id, unsigned int timeout)
{
	return 0;
}

void mt_irq_set_sens(unsigned int irq, unsigned int sens)
{
}

void mt_irq_set_polarity(unsigned int irq, unsigned int polarity)
{
}

#define MT_EDGE_SENSITIVE 0
#define MT_LEVEL_SENSITIVE 1
#define MT_POLARITY_LOW   0
#define MT_POLARITY_HIGH  1
#else
extern int md_power_on(int);
extern int md_power_off(int, unsigned);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
#endif

int md_cd_power_on(struct ccci_modem *md)
{
#ifndef MT6290
	int ret = 0;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	/* power on MD TOP and INFRASYS */
	ret = md_power_on(md->index);
	if (ret)
		return ret;
	/* disable MD WDT */
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
#endif
	return 0;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
#ifndef MT6290
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	if (ccci_get_md_debug_mode(md) & DBG_FLAG_JTAG)
		return -1;
	CCCI_INF_MSG(md->index, "mcd", "set MD boot slave\n");
	/* set the start address to let modem to run */
	cldma_write32(md_ctrl->md_boot_slave_Key, 0, 0x3567C766);	/* make boot vector programmable */
	cldma_write32(md_ctrl->md_boot_slave_Vector, 0, 0x00000001);	/* after remap, MD ROM address is 0 from MD's view, MT6595 uses Thumb code */
	cldma_write32(md_ctrl->md_boot_slave_En, 0, 0xA3B66175);	/* make boot vector take effect */
#endif
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	ret = md_power_off(md->index, timeout);
	return ret;
}

void md_cd_lock_cldma_clock_src(int locked)
{
#ifndef MT6290
	spm_ap_mdsrc_req(locked);
#endif
}

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
#ifndef MT6290
	cldma_write32(AP_CCIF0_BASE, APCCIF_CON, 0x01);	/* arbitration */
#endif
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	BUG_ON(pdev == NULL);

	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	BUG_ON(pdev == NULL);

	return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
	/* IPO-H */
	/* restore IRQ */
#ifdef FEATURE_PM_IPO_H
	mt_irq_set_sens(CLDMA_AP_IRQ, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CLDMA_AP_IRQ, MT_POLARITY_HIGH);
	mt_irq_set_sens(MD_WDT_IRQ, MT_EDGE_SENSITIVE);
	mt_irq_set_polarity(MD_WDT_IRQ, MT_POLARITY_LOW);
#endif
	/* set flag for next md_start */
	md->config.setting |= MD_SETTING_RELOAD;
	return 0;
}
