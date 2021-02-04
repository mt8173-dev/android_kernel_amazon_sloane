#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>	/* udelay */
#include <linux/platform_device.h>


#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_typedefs.h>

/* #include <mach/mt_spm_cpu.h> *//* TOFIX: no use for MT8173 */
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <mach/hotplug.h>
#include <mach/mt_ptp2.h>
#include <mach/mt_chip.h>
#include <drivers/clk/mediatek/clk-mtk.h>

#ifdef CONFIG_OF

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

static void __iomem *infracfg_ao_base;

#define TOPAXI_PROT_EN          (infracfg_ao_base + 0x0220)
#define TOPAXI_PROT_STA1        (infracfg_ao_base + 0x0228)
#define TOPAXI_PROT_EN1         (infracfg_ao_base + 0x0250)
#define TOPAXI_PROT_STA3        (infracfg_ao_base + 0x0258)
#define TOPAXI_DCMCTL           (infracfg_ao_base + 0x0010)

static void __iomem *mcucfg_base;

#define CA15L_MISCDBG		(mcucfg_base + 0x20c)

#endif				/* CONFIG_OF */


/**************************************
 * for CPU MTCMOS
 **************************************/

static DEFINE_SPINLOCK(spm_cpu_lock);

int __init mt_spm_mtcmos_init(void);

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}


typedef int (*spm_cpu_mtcmos_ctrl_func) (int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] = {
	spm_mtcmos_ctrl_cpu0,
	spm_mtcmos_ctrl_cpu1,
	spm_mtcmos_ctrl_cpu2,
	spm_mtcmos_ctrl_cpu3,
	spm_mtcmos_ctrl_cpu4,
	spm_mtcmos_ctrl_cpu5,
	spm_mtcmos_ctrl_cpu6,
	spm_mtcmos_ctrl_cpu7
};

int __init spm_mtcmos_cpu_init(void)
{
	mt_spm_mtcmos_init();

	pr_info("CPU num: %d\n", num_possible_cpus());

	if (num_possible_cpus() == 4) {
		spm_cpu_mtcmos_ctrl_funcs[0] = spm_mtcmos_ctrl_cpu0;
		spm_cpu_mtcmos_ctrl_funcs[1] = spm_mtcmos_ctrl_cpu1;
		spm_cpu_mtcmos_ctrl_funcs[2] = spm_mtcmos_ctrl_cpu4;
		spm_cpu_mtcmos_ctrl_funcs[3] = spm_mtcmos_ctrl_cpu5;
	}
	ptp2_pre_iomap();
	return 0;
}


int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
	return (*spm_cpu_mtcmos_ctrl_funcs[cpu]) (state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU0_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) | L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != CA7_CPU0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != CA7_CPU0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) & ~L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU1_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) | L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != CA7_CPU1)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != CA7_CPU1));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) & ~L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU2_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) | L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != CA7_CPU2)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != CA7_CPU2));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) & ~L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU3_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) | L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != CA7_CPU3)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != CA7_CPU3));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) & ~L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		turn_off_SPARK("turn off SPARK when cpu4 power down");
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU0_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) !=
		       CPU0_CA15_L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON_2ND);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_RST_B);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);

		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {		/* STA_POWER_ON */

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != CA15_CPU0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != CA15_CPU0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		turn_off_SPARK("turn off SPARK when cpu5 power down");
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU1_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) !=
		       CPU1_CA15_L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON_2ND);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_RST_B);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);

		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {		/* STA_POWER_ON */

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != CA15_CPU1);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != CA15_CPU1);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU2_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) !=
		       CPU2_CA15_L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON_2ND);

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_RST_B);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);

		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU2 | CA15_CPU1 | CA15_CPU3)) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU2 | CA15_CPU1 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {		/* STA_POWER_ON */

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != CA15_CPU2);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != CA15_CPU2);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU3_STANDBYWFI) == 0);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) !=
		       CPU3_CA15_L1_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON_2ND);

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_RST_B);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);

		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU3 | CA15_CPU1 | CA15_CPU2)) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU3 | CA15_CPU1 | CA15_CPU2)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {		/* STA_POWER_ON */

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != CA15_CPU3);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != CA15_CPU3);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

#if 0				/* There is no dbgsys wrapper in ca53 cpusys */
int spm_mtcmos_ctrl_dbg0(int state)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ISO);
		spm_write(SPM_CA7_DBG_PWR_CON,
			  (spm_read(SPM_CA7_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != CA7_DBG)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != CA7_DBG));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_dbg1(int state)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ISO);
		spm_write(SPM_MP1_DBG_PWR_CON,
			  (spm_read(SPM_MP1_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != MP1_DBG)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != MP1_DBG));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}
#endif				/* There is no dbgsys wrapper in ca53 cpusys */

int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		/* TODO: add per cpu power status check? */

		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPUTOP_STANDBYWFI) == 0);

		/* XXX: no dbg0 mtcmos on k2 */
		/* spm_mtcmos_ctrl_dbg0(state); */

		/* XXX: async adb on mt8173 */
		spm_topaxi_prot_l2(L2_PDN_REQ, 1);
		spm_topaxi_prot(CA7_PDN_REQ, 1);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_CKISO);
#if 1
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			  spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) | L2_SRAM_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != L2_SRAM_PDN_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
		ndelay(1500);
#else
		ndelay(100);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			  spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPUTOP_L2_SLEEP,
			  spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & ~L2_SRAM_SLEEP_B);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
#endif

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != 0));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON_2ND);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != CA7_CPUTOP)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != CA7_CPUTOP));
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ISO);

#if 1
		spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) & ~L2_SRAM_PDN);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != 0);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
#else
		spm_write(SPM_CA7_CPUTOP_L2_SLEEP,
			  spm_read(SPM_CA7_CPUTOP_L2_SLEEP) | L2_SRAM_SLEEP_B);
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) !=
		       L2_SRAM_SLEEP_B_ACK);
#endif				/* #ifndef CONFIG_FPGA_EARLY_PORTING */
#endif
		ndelay(900);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		ndelay(100);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

		/* XXX: async adb on mt8173 */
		spm_topaxi_prot_l2(L2_PDN_REQ, 0);
		spm_topaxi_prot(CA7_PDN_REQ, 0);

		/* XXX: no dbg0 mtcmos on k2 */
		/* spm_mtcmos_ctrl_dbg0(state); */
	}

	return 0;
}

int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		turn_off_SPARK("spm_mtcmos_ctrl_cpsys1-----0");
		/* assert ACINCATM before wait for WFIL2 */
		spm_write(CA15L_MISCDBG, spm_read(CA15L_MISCDBG) | 0x1);


		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPUTOP_STANDBYWFI) == 0);

		spm_topaxi_prot(CA15_PDN_REQ, 1);

		spm_mtcmos_cpu_lock(&flags);

		/*turn_off_FBB();*/

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);

#if 1
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != CA15_L2_PDN_ACK);
#else
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~L2_SRAM_SLEEP_B);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) != 0);
#endif
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_RST_B);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_CLK_DIS);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != 0));

		spm_mtcmos_cpu_unlock(&flags);

		if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) == 0) {

			/* enable dcm for low power */
			if (CHIP_SW_VER_01 == mt_get_chip_sw_ver())
				spm_write(TOPAXI_DCMCTL, spm_read(TOPAXI_DCMCTL) | 0x00000771);

			spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON,
				  spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) | VCA15_PWR_ISO);
		}
	} else {		/* STA_POWER_ON */

		if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) == VCA15_PWR_ISO) {
			spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON,
				  spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & ~VCA15_PWR_ISO);

			/* disable dcm for performance */
			if (CHIP_SW_VER_01 == mt_get_chip_sw_ver()) {
				spm_write(TOPAXI_DCMCTL, spm_read(TOPAXI_DCMCTL) | 0x00000001);
				spm_write(TOPAXI_DCMCTL, spm_read(TOPAXI_DCMCTL) & ~0x00000770);
			}
		}

		/* enable L2 cache hardware reset: Default CA15L_L2RSTDISABLE init is 0 */
		/* spm_write(CA15L_RST_CTL, spm_read(CA15L_RST_CTL) & ~CA15L_L2RSTDISABLE); */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != CA15_CPUTOP);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON_2ND);
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != CA15_CPUTOP);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);

#if 1
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != 0);
#else
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | L2_SRAM_SLEEP_B);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) !=
		       L2_SRAM_SLEEP_B_ACK);
#endif

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_ISOINT_B);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ISO);

		ptp2_pre_init();

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

		spm_topaxi_prot(CA15_PDN_REQ, 0);
	}

	return 0;

}

void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state)
{

	if (state == STA_POWER_DOWN) {
		spm_mtcmos_ctrl_cpu7(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu6(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu5(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu4(STA_POWER_DOWN, 0);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_ctrl_cpu4(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu5(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu6(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu7(STA_POWER_ON, 1);
		/* spm_mtcmos_ctrl_dbg1(STA_POWER_ON); */
	}

	/* unsigned long flags; */
	/*  */
	/* enable register control */
	/* spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0)); */
	/*  */
	/* spm_mtcmos_cpu_lock(&flags); */
	/*  */
	/* spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_CLK_DIS); */
	/* spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B); */
	/* spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_CLK_DIS); */
	/* spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B); */
	/* spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_CLK_DIS); */
	/* spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B); */
	/* spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_CLK_DIS); */
	/* spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B); */
	/*  */
	/* spm_mtcmos_cpu_unlock(&flags); */
	/*  */
	/* spm_mtcmos_ctrl_dbg1(STA_POWER_ON); */
}

bool spm_cpusys0_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2
		  | CA7_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2
		  | CA7_CPU3));
}

bool spm_cpusys1_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3));
}


/**************************************
 * for non-CPU MTCMOS
 **************************************/
#if 0
void spm_mtcmos_noncpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_noncpu_lock, *flags);
}

void spm_mtcmos_noncpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_noncpu_lock, *flags);
}
#else
#define spm_mtcmos_noncpu_lock(flags)   \
do {    \
	mtk_clk_lock(flags);	\
} while (0)

#define spm_mtcmos_noncpu_unlock(flags) \
do {    \
	mtk_clk_unlock(flags);	\
} while (0)

#endif

#define USB_PWR_STA_MASK    (0x1 << 25)
#define AUD_PWR_STA_MASK    (0x1 << 24)
#define MFG_ASYNC_PWR_STA_MASK (0x1 << 23)
#define MFG_2D_PWR_STA_MASK (0x1 << 22)
#define VEN_PWR_STA_MASK    (0x1 << 21)
#define VEN2_PWR_STA_MASK   (0x1 << 20)
#define VDE_PWR_STA_MASK    (0x1 << 7)
/* #define IFR_PWR_STA_MASK    (0x1 << 6) */
#define ISP_PWR_STA_MASK    (0x1 << 5)
#define MFG_PWR_STA_MASK    (0x1 << 4)
#define DIS_PWR_STA_MASK    (0x1 << 3)
/* #define DPY_PWR_STA_MASK    (0x1 << 2) */

#if 0
#define PWR_RST_B           (0x1 << 0)
#define PWR_ISO             (0x1 << 1)
#define PWR_ON              (0x1 << 2)
#define PWR_ON_2ND          (0x1 << 3)
#define PWR_CLK_DIS         (0x1 << 4)
#endif

#define SRAM_PDN            (0xf << 8)
#define MFG_SRAM_PDN        (0x3f << 8)

/*
#define VDE_SRAM_ACK        (0x1 << 12)
#define IFR_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0xf << 12)
#define MFG_SRAM_ACK        (0x1 << 12)
*/
#define VDE_SRAM_ACK        (0x1 << 12)
#define VEN_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0x1 << 12)
#define MFG_SRAM_ACK        (0x3f << 16)
#define MFG_2D_SRAM_ACK     (0x3 << 12)
#define VEN2_SRAM_ACK       (0xf << 12)
#define AUD_SRAM_ACK        (0xf << 12)
#define USB_SRAM_ACK        (0xf << 12)

/* #define DISP_PROT_MASK    0x0046	//bit 1,2,6 */
#define DISP_PROT_MASK    0x0006	/* bit 1,2 */
#define MFG_2D_PROT_MASK  0x00E04000	/* bit 14,21,22,23 */


int spm_mtcmos_ctrl_vdec(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK) != VDE_SRAM_ACK) {
		}
#endif

		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_VDE_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_VDE_PWR_CON, val);

		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON);
		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK)
		       || !(spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_RST_B);

		spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_venc(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK) != VEN_SRAM_ACK) {
		}
#endif

		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_VEN_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_VEN_PWR_CON, val);

		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON);
		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK)
		       || !(spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_RST_B);

		spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_isp(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK) != ISP_SRAM_ACK) {
		}
#endif

		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_ISP_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_ISP_PWR_CON, val);

		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON);
		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
		       || !(spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_RST_B);

		spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

#ifdef CONFIG_FPGA_EARLY_PORTING
int spm_mtcmos_ctrl_disp(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
		while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
		}

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);
#if 0
		while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
		}
#endif
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_DIS_PWR_CON);
		/* val = (val & ~PWR_RST_B) | PWR_CLK_DIS; */
		val = val | PWR_CLK_DIS;
		spm_write(SPM_DIS_PWR_CON, val);

		/* spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND)); */

#if 0
		udelay(1);
		if (spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) {
			err = 1;
		}
#else
		/* while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) */
		/* || (spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) { */
		/* } */
#endif
	} else {		/* STA_POWER_ON */
		/* spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON); */
		/* spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND); */
#if 0
		udelay(1);
#else
		/* while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) */
		/* || !(spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) { */
		/* } */
#endif
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
		/* spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B); */

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

#if 0
		while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
		}
#endif

#if 0
		udelay(1);
		if (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)) {
			err = 1;
		}
#endif
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
		while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
		}
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

#else

int spm_mtcmos_ctrl_disp(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
		while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
		}

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
		}
#endif

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_DIS_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		/* val = val | PWR_CLK_DIS; */
		spm_write(SPM_DIS_PWR_CON, val);

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
		       || !(spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

		spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
		}
#endif

		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
		while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
		}
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}
#endif

int spm_mtcmos_ctrl_mfg(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;
/* return 0; */
	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_2D_PROT_MASK);
		while ((spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) != MFG_2D_PROT_MASK) {
		}

		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | MFG_SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
		}
#endif

		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_MFG_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_MFG_PWR_CON, val);

		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON);
		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) ||
		       !(spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_RST_B);

		spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~MFG_SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK)) {
		}
#endif

		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_2D_PROT_MASK);
		while (spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) {
		}
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_mfg_2D(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;
/* return 0; */
	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
/*
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_2D_PROT_MASK);
		while ((spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) != MFG_2D_PROT_MASK) {
		}

		spm_write(TOPAXI_SI0_CTL, spm_read(TOPAXI_SI0_CTL) & ~MFG_SI0_MASK);
*/
		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK) != MFG_2D_SRAM_ACK) {
		}
#endif

		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_MFG_2D_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_MFG_2D_PWR_CON, val);

		spm_write(SPM_MFG_2D_PWR_CON,
			  spm_read(SPM_MFG_2D_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON);
		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK) ||
		       !(spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_RST_B);

		spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK)) {
		}
#endif
/*
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_2D_PROT_MASK);
		while (spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) {
		}

		spm_write(TOPAXI_SI0_CTL, spm_read(TOPAXI_SI0_CTL) | MFG_SI0_MASK);
*/
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_mfg_ASYNC(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;
/* return 0; */
	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | SRAM_PDN);
/*
		while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
		}
*/
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_MFG_ASYNC_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_MFG_ASYNC_PWR_CON, val);

		spm_write(SPM_MFG_ASYNC_PWR_CON,
			  spm_read(SPM_MFG_ASYNC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON);
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) ||
		       !(spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_RST_B);

		spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~SRAM_PDN);
/*
		while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK)) {
		}
*/
	}
	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_ven2(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {

		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VEN2_PWR_CON) & VEN2_SRAM_ACK) != VEN2_SRAM_ACK) {
		}
#endif

		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_VEN2_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_VEN2_PWR_CON, val);

		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & VEN2_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & VEN2_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) | PWR_ON);
		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & VEN2_PWR_STA_MASK) ||
		       !(spm_read(SPM_PWR_STATUS_2ND) & VEN2_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) | PWR_RST_B);

		spm_write(SPM_VEN2_PWR_CON, spm_read(SPM_VEN2_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_VEN2_PWR_CON) & VEN2_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_aud(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {

		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK) != AUD_SRAM_ACK) {
		}
#endif

		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_AUDIO_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_AUDIO_PWR_CON, val);

		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON);
		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) ||
		       !(spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_RST_B);

		spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_mtcmos_ctrl_usb(int state)
{
	int err = 0;
	volatile unsigned int val;
	unsigned long flags;

	spm_mtcmos_noncpu_lock(flags);

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) | SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_USB_PWR_CON) & USB_SRAM_ACK) != USB_SRAM_ACK) {
		}
#endif

		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) | PWR_ISO);

		val = spm_read(SPM_USB_PWR_CON);
		val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
		spm_write(SPM_USB_PWR_CON, val);

		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_PWR_STATUS) & USB_PWR_STA_MASK)
		       || (spm_read(SPM_PWR_STATUS_2ND) & USB_PWR_STA_MASK)) {
		}
#endif
	} else {		/* STA_POWER_ON */
		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) | PWR_ON);
		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) | PWR_ON_2ND);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while (!(spm_read(SPM_PWR_STATUS) & USB_PWR_STA_MASK)
		       || !(spm_read(SPM_PWR_STATUS_2ND) & USB_PWR_STA_MASK)) {
		}
#endif

		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) | PWR_RST_B);

		spm_write(SPM_USB_PWR_CON, spm_read(SPM_USB_PWR_CON) & ~SRAM_PDN);

#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(SPM_USB_PWR_CON) & USB_SRAM_ACK)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return err;
}

int spm_topaxi_prot(int bit, int en)
{
	unsigned long flags;
	spm_mtcmos_noncpu_lock(flags);

	if (en == 1) {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | (1 << bit));
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(TOPAXI_PROT_STA1) & (1 << bit)) != (1 << bit)) {
		}
#endif
	} else {
		spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~(1 << bit));
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (spm_read(TOPAXI_PROT_STA1) & (1 << bit)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return 0;
}

int spm_topaxi_prot_l2(int bit, int en)
{
	unsigned long flags;
	spm_mtcmos_noncpu_lock(flags);

	if (en == 1) {
		spm_write(TOPAXI_PROT_EN1, spm_read(TOPAXI_PROT_EN1) | (1 << bit));
#ifndef CONFIG_FPGA_EARLY_PORTING
		while ((spm_read(TOPAXI_PROT_STA3) & (1 << bit)) != (1 << bit)) {
		}
#endif
	} else {
		spm_write(TOPAXI_PROT_EN1, spm_read(TOPAXI_PROT_EN1) & ~(1 << bit));
#ifndef CONFIG_FPGA_EARLY_PORTING
		while (spm_read(TOPAXI_PROT_STA3) & (1 << bit)) {
		}
#endif
	}

	spm_mtcmos_noncpu_unlock(flags);

	return 0;
}


int __init mt_spm_mtcmos_init(void)
{
	static int init;
#ifdef CONFIG_OF		/* TOFIX: move init to earlier phase spm_mtcmos_cpu_init */
	struct device_node *spm_node;
	struct device_node *infracfg_ao_node;
	struct device_node *mcucfg_node;
#endif				/* CONFIG_OF */

	if (init)
		return 0;

#ifdef CONFIG_OF		/* TOFIX: move init to earlier phase spm_mtcmos_cpu_init */


	spm_node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");

	if (!spm_node) {
		pr_err("spm_node not found!\n");
		return -1;
	}

	infracfg_ao_node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");

	if (!infracfg_ao_node) {
		pr_err("infracfg_ao_node not found!\n");
		return -1;
	}

	mcucfg_node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");

	if (!mcucfg_node) {
		pr_err("infracfg_ao_node not found!\n");
		return -1;
	}

	spm_base = of_iomap(spm_node, 0);
	pr_info("spm_base: 0x%08lx\n", (unsigned long)spm_base);

	if (!spm_base) {
		pr_err("spm_base map failed!\n");
		return -1;
	}

	infracfg_ao_base = of_iomap(infracfg_ao_node, 0);
	pr_info("infracfg_ao_base: 0x%08lx\n", (unsigned long)infracfg_ao_base);

	if (!infracfg_ao_base) {
		pr_err("infracfg_ao_base map failed!\n");
		return -1;
	}

	mcucfg_base = of_iomap(mcucfg_node, 0);
	pr_info("mcucfg_base: 0x%08lx\n", (unsigned long)mcucfg_base);

	if (!mcucfg_base) {
		pr_err("mcucfg_base map failed!\n");
		return -1;
	}
#endif				/* CONFIG_OF */

#ifdef CONFIG_FPGA_EARLY_PORTING
	spm_mtcmos_ctrl_vdec(STA_POWER_ON);
	spm_mtcmos_ctrl_venc(STA_POWER_ON);
	spm_mtcmos_ctrl_isp(STA_POWER_ON);
	spm_mtcmos_ctrl_disp(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg_2D(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg_ASYNC(STA_POWER_ON);
	spm_mtcmos_ctrl_ven2(STA_POWER_ON);
	spm_mtcmos_ctrl_aud(STA_POWER_ON);
	spm_mtcmos_ctrl_usb(STA_POWER_ON);
#endif				/* CONFIG_FPGA_EARLY_PORTING */

#ifdef CONFIG_OF
	/* keep the bases in case some one call the mtcmos_ctrl functions */
	/* iounmap(spm_base); */
	/* iounmap(infracfg_ao_base); */
#endif

	init = 1;

	return 0;
}

arch_initcall(mt_spm_mtcmos_init);
