/*
 *  mtk_ts_cpu.c - MTK CPU thermal zone driver.
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/mtk_gpu_utility.h>
#include <linux/time.h>

#include <mach/sync_write.h>
#include <mach/mt_irq.h>
#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include "mach/mt_cpufreq.h"
#include <mach/mt_spm.h>
#include <mach/mt_ptp.h>
#include <mach/wd_api.h>
#include <mach/upmu_hw.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#define __MT_MTK_TS_CPU_C__

#define THERMAL_NAME "mtk-thermal"

#ifdef CONFIG_OF
u32 thermal_irq_number = 0;
void __iomem *thermal_base;
void __iomem *auxadc_ts_base;
void __iomem *pericfg_base;
void __iomem *apmixed_ts_base;

int thermal_phy_base;
int auxadc_ts_phy_base;
int apmixed_phy_base;

struct clk *clk_peri_therm;
#endif

/*==============*/
/*Configurations*/
/*==============*/
/* 1: turn on supports to MET logging; 0: turn off */
#define CONFIG_SUPPORT_MET_MTKTSCPU         (0)

/* Thermal controller HW filtering function.
Only 1, 2, 4, 8, 16 are valid values,
they means one reading is a avg of X samples */
#define THERMAL_CONTROLLER_HW_FILTER        (1)	/* 1, 2, 4, 8, 16 */

/* 1: turn on SW filtering in this sw module; 0: turn off */
#define MTK_TS_CPU_SW_FILTER                (1)

static void tscpu_fast_initial_sw_workaround(void);

#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

/*==============*/
/*Variables*/
/*==============*/

#define thermal_readl(addr)         DRV_Reg32(addr)
#define thermal_writel(addr, val)   mt_reg_sync_writel((val), ((void *)addr))
#define thermal_setl(addr, val)     mt_reg_sync_writel(thermal_readl(addr) | (val), ((void *)addr))
#define thermal_clrl(addr, val)     mt_reg_sync_writel(thermal_readl(addr) & ~(val), ((void *)addr))

#define MTKTSCPU_TEMP_CRIT 117000	/* 117.000 degree Celsius */
#define MASK (0x0FFF)

static int mtktscpu_debug_log;
static int tc_mid_trip = -275000;

#define tscpu_dprintk(fmt, args...)   \
do {								\
	if (mtktscpu_debug_log) {					\
		xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal", fmt, ##args); \
	}								\
} while (0)

#define tscpu_printk(fmt, args...) xlog_printk(ANDROID_LOG_INFO, "Power/CPU_Thermal", fmt, ##args);

static int mtktscpu_switch_bank(thermal_bank_name bank);
static void tscpu_reset_thermal(void);
static S32 temperature_to_raw_room(U32 ret);
static void set_tc_trigger_hw_protect(int temperature, int temperature2);
static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2);
static void thermal_initial_all_bank(void);
static void read_each_bank_TS(thermal_bank_name bank_num);
/* static void thermal_fast_initial_all_bank(void); */
/*
Bank 0 : CA7  (TS2 TS3)
Bank 1 : CA15 (TS2 TS4)
Bank 2 : GPU  (TS1 TS2 TSABB)
Bank 3 : CORE (TS2)
*/
static int CA7_TS2_T = 0, CA7_TS3_T;
static int CA15_TS2_T = 0, CA15_TS4_T;
static int GPU_TS1_T = 0, GPU_TS2_T = 0, GPU_TSABB_T;
static int CORE_TS2_T;

static int CA7_TS2_R = 0, CA7_TS3_R;
static int CA15_TS2_R = 0, CA15_TS4_R;
static int GPU_TS1_R = 0, GPU_TS2_R = 0, GPU_TSABB_R;
static int CORE_TS2_R;

int last_abb_t = 0;
int last_CPU1_t = 0;
int last_CPU2_t = 0;

static int g_tc_resume;		/* default=0,read temp */

static S32 g_adc_ge_t;
static S32 g_adc_oe_t;
static S32 g_o_vtsmcu1;
static S32 g_o_vtsmcu2;
static S32 g_o_vtsmcu3;
static S32 g_o_vtsmcu4;
static S32 g_o_vtsabb;
static S32 g_degc_cali;
static S32 g_adc_cali_en_t;
static S32 g_o_slope;
static S32 g_o_slope_sign;
static S32 g_id;

static S32 g_ge;
static S32 g_oe;
static S32 g_gain;
static S32 g_x_roomt[THERMAL_SENSOR_NUM] = { 0 };

int mtktscpu_limited_dmips = 0;
static bool talking_flag;
static int thermal_fast_init(void);

void set_taklking_flag(bool flag)
{
	talking_flag = flag;
	return;
}

void tscpu_thermal_clock_on(void)
{
	clk_prepare(clk_peri_therm);
	clk_enable(clk_peri_therm);
}

void tscpu_thermal_clock_off(void)
{
	clk_disable(clk_peri_therm);
	clk_unprepare(clk_peri_therm);
}

void get_thermal_all_register(void)
{
	tscpu_dprintk("get_thermal_all_register\n");

	tscpu_dprintk("TEMPMSR1			  = 0x%8x\n", DRV_Reg32(TEMPMSR1));
	tscpu_dprintk("TEMPMSR2            = 0x%8x\n", DRV_Reg32(TEMPMSR2));

	tscpu_dprintk("TEMPMONCTL0	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL0));
	tscpu_dprintk("TEMPMONCTL1	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL1));
	tscpu_dprintk("TEMPMONCTL2	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL2));
	tscpu_dprintk("TEMPMONINT	  = 0x%8x\n", DRV_Reg32(TEMPMONINT));
	tscpu_dprintk("TEMPMONINTSTS	  = 0x%8x\n", DRV_Reg32(TEMPMONINTSTS));
	tscpu_dprintk("TEMPMONIDET0	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET0));

	tscpu_dprintk("TEMPMONIDET1	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET1));
	tscpu_dprintk("TEMPMONIDET2	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET2));
	tscpu_dprintk("TEMPH2NTHRE	  = 0x%8x\n", DRV_Reg32(TEMPH2NTHRE));
	tscpu_dprintk("TEMPHTHRE	  = 0x%8x\n", DRV_Reg32(TEMPHTHRE));
	tscpu_dprintk("TEMPCTHRE	  = 0x%8x\n", DRV_Reg32(TEMPCTHRE));
	tscpu_dprintk("TEMPOFFSETH	  = 0x%8x\n", DRV_Reg32(TEMPOFFSETH));

	tscpu_dprintk("TEMPOFFSETL	  = 0x%8x\n", DRV_Reg32(TEMPOFFSETL));
	tscpu_dprintk("TEMPMSRCTL0	  = 0x%8x\n", DRV_Reg32(TEMPMSRCTL0));
	tscpu_dprintk("TEMPMSRCTL1	  = 0x%8x\n", DRV_Reg32(TEMPMSRCTL1));
	tscpu_dprintk("TEMPAHBPOLL	  = 0x%8x\n", DRV_Reg32(TEMPAHBPOLL));
	tscpu_dprintk("TEMPAHBTO	  = 0x%8x\n", DRV_Reg32(TEMPAHBTO));
	tscpu_dprintk("TEMPADCPNP0	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP0));

	tscpu_dprintk("TEMPADCPNP1	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP1));
	tscpu_dprintk("TEMPADCPNP2	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP2));
	tscpu_dprintk("TEMPADCMUX	  = 0x%8x\n", DRV_Reg32(TEMPADCMUX));
	tscpu_dprintk("TEMPADCEXT	  = 0x%8x\n", DRV_Reg32(TEMPADCEXT));
	tscpu_dprintk("TEMPADCEXT1	  = 0x%8x\n", DRV_Reg32(TEMPADCEXT1));
	tscpu_dprintk("TEMPADCEN	  = 0x%8x\n", DRV_Reg32(TEMPADCEN));


	tscpu_dprintk("TEMPPNPMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPPNPMUXADDR));
	tscpu_dprintk("TEMPADCMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPADCMUXADDR));
	tscpu_dprintk("TEMPADCEXTADDR      = 0x%8x\n", DRV_Reg32(TEMPADCEXTADDR));
	tscpu_dprintk("TEMPADCEXT1ADDR     = 0x%8x\n", DRV_Reg32(TEMPADCEXT1ADDR));
	tscpu_dprintk("TEMPADCENADDR       = 0x%8x\n", DRV_Reg32(TEMPADCENADDR));
	tscpu_dprintk("TEMPADCVALIDADDR    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDADDR));

	tscpu_dprintk("TEMPADCVOLTADDR     = 0x%8x\n", DRV_Reg32(TEMPADCVOLTADDR));
	tscpu_dprintk("TEMPRDCTRL          = 0x%8x\n", DRV_Reg32(TEMPRDCTRL));
	tscpu_dprintk("TEMPADCVALIDMASK    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDMASK));
	tscpu_dprintk("TEMPADCVOLTAGESHIFT = 0x%8x\n", DRV_Reg32(TEMPADCVOLTAGESHIFT));
	tscpu_dprintk("TEMPADCWRITECTRL    = 0x%8x\n", DRV_Reg32(TEMPADCWRITECTRL));
	tscpu_dprintk("TEMPMSR0            = 0x%8x\n", DRV_Reg32(TEMPMSR0));


	tscpu_dprintk("TEMPIMMD0           = 0x%8x\n", DRV_Reg32(TEMPIMMD0));
	tscpu_dprintk("TEMPIMMD1           = 0x%8x\n", DRV_Reg32(TEMPIMMD1));
	tscpu_dprintk("TEMPIMMD2           = 0x%8x\n", DRV_Reg32(TEMPIMMD2));
	tscpu_dprintk("TEMPPROTCTL         = 0x%8x\n", DRV_Reg32(TEMPPROTCTL));

	tscpu_dprintk("TEMPPROTTA          = 0x%8x\n", DRV_Reg32(TEMPPROTTA));
	tscpu_dprintk("TEMPPROTTB		  = 0x%8x\n", DRV_Reg32(TEMPPROTTB));
	tscpu_dprintk("TEMPPROTTC		  = 0x%8x\n", DRV_Reg32(TEMPPROTTC));
	tscpu_dprintk("TEMPSPARE0		  = 0x%8x\n", DRV_Reg32(TEMPSPARE0));
	tscpu_dprintk("TEMPSPARE1		  = 0x%8x\n", DRV_Reg32(TEMPSPARE1));
	tscpu_dprintk("TEMPSPARE2		  = 0x%8x\n", DRV_Reg32(TEMPSPARE2));
	tscpu_dprintk("TEMPSPARE3		  = 0x%8x\n", DRV_Reg32(TEMPSPARE3));
	tscpu_dprintk("0x11001040		  = 0x%8x\n", DRV_Reg32(0xF1001040));

}

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info, thermal_TS_name ts_name)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	S32 x_roomt;

	switch (ts_name) {
	case THERMAL_CA7:	/* TS3 */
		x_roomt = g_x_roomt[2];
		break;
	case THERMAL_CA15:	/* TS4 */
		x_roomt = g_x_roomt[3];
		break;
	case THERMAL_GPU:	/* TS1 */
		x_roomt = g_x_roomt[0];
		break;
	case THERMAL_CORE:	/* TS2 */
		x_roomt = g_x_roomt[1];
		break;
	default:		/* THERMAL_CA7 */
		x_roomt = g_x_roomt[2];
		break;
	}

	/* temp0 = (10000*100000/4096/g_gain)*15/18; */
	temp0 = (10000 * 100000 / g_gain) * 15 / 18;
	/* tscpu_printk("temp0=%d\n", temp0); */
	if (g_o_slope_sign == 0)
		temp1 = temp0 / (165 + g_o_slope);
	else
		temp1 = temp0 / (165 - g_o_slope);
	/* tscpu_printk("temp1=%d\n", temp1); */
	/* ts_ptpod.ts_MTS = temp1 - (2*temp1) + 2048; */
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + x_roomt * 10) * 15 / 18;
	/* tscpu_printk("temp1=%d\n", temp1); */
	if (g_o_slope_sign == 0)
		temp2 = temp1 * 10 / (165 + g_o_slope);
	else
		temp2 = temp1 * 10 / (165 - g_o_slope);
	/* tscpu_printk("temp2=%d\n", temp2); */
	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;

	/* ts_info = &ts_ptpod; */
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	tscpu_printk("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);

	return;
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

static void thermal_interrupt_handler(int bank)
{
	U32 ret = 0;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(bank);

	ret = DRV_Reg32(TEMPMONINTSTS);
	/* printk("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"); */
	tscpu_printk("thermal_interrupt_handler,bank=0x%08x,ret=0x%08x\n", bank, ret);
	/* printk("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"); */

	/* ret2 = DRV_Reg32(THERMINTST); */
	/* printk("thermal_interrupt_handler : THERMINTST = 0x%x\n", ret2); */


	/* for SPM reset debug */
	/* dump_spm_reg(); */

	/* tscpu_printk("thermal_isr: [Interrupt trigger]: status = 0x%x\n", ret); */
	if (ret & THERMAL_MON_CINTSTS0)
		tscpu_printk("thermal_isr: thermal sensor point 0 - cold interrupt trigger\n");
	if (ret & THERMAL_MON_HINTSTS0)
		tscpu_printk("<<<thermal_isr>>>: thermal sensor point 0 - hot interrupt trigger\n");
	if (ret & THERMAL_MON_HINTSTS1)
		tscpu_printk("<<<thermal_isr>>>: thermal sensor point 1 - hot interrupt trigger\n");
	if (ret & THERMAL_MON_HINTSTS2)
		tscpu_printk("<<<thermal_isr>>>: thermal sensor point 2 - hot interrupt trigger\n");
	if (ret & THERMAL_tri_SPM_State0)
		tscpu_printk("thermal_isr: Thermal state0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		tscpu_printk("thermal_isr: Thermal state1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		tscpu_printk("thermal_isr: Thermal state2 to trigger SPM state2\n");

	mt_ptp_unlock(&flags);
}

static irqreturn_t thermal_all_bank_interrupt_handler(int irq, void *dev_id)
{
	U32 ret = 0;

	ret = DRV_Reg32(THERMINTST);
	ret = ret & 0xF;
#if 1
	if ((ret & 0x1) == 0)	/* check bit0 */
		thermal_interrupt_handler(THERMAL_BANK0);

	if ((ret & 0x2) == 0)	/* check bit1 */
		thermal_interrupt_handler(THERMAL_BANK1);

	if ((ret & 0x4) == 0)	/* check bit2 */
		thermal_interrupt_handler(THERMAL_BANK2);

	if ((ret & 0x8) == 0)	/* check bit3 */
		thermal_interrupt_handler(THERMAL_BANK3);


	/* thermal_interrupt_handler(ret); */
#else
	switch (ret) {
	case 0xE:		/* 1110,bank0 */
		thermal_interrupt_handler(THERMAL_BANK0);
		break;
	case 0xD:		/* 1101,bank1 */
		thermal_interrupt_handler(THERMAL_BANK1);
		break;
	case 0xB:		/* 1011,bank2 */
		thermal_interrupt_handler(THERMAL_BANK2);
		break;
	case 0x7:		/* 0111,bank3 */
		thermal_interrupt_handler(THERMAL_BANK3);
		break;
	default:
		thermal_interrupt_handler(THERMAL_BANK0);
		break;
	}
#endif
	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(void)
{

	/* printk( "thermal_reset_and_initial\n"); */


	/* Calculating period unit in Module clock x 256, and the Module clock */
	/* will be changed to 26M when Infrasys enters Sleep mode. */
	/* THERMAL_WRAP_WR32(0x000003FF, TEMPMONCTL1);    // counting unit is 1023 * 15.15ns ~ 15.5us */


	/* THERMAL_WRAP_WR32(0x00000004, TEMPMONCTL1);
	// bus clock 66M counting unit is 4*15.15ns* 256 = 15513.6 ms=15.5us */

	/* bus clock 66M counting unit is 12*15.15ns* 256 = 46.540us */
	THERMAL_WRAP_WR32(0x0000000C, TEMPMONCTL1);
	/* THERMAL_WRAP_WR32(0x000001FF, TEMPMONCTL1);
	// bus clock 66M counting unit is 4*15.15ns* 256 = 15513.6 ms=15.5us */

#if THERMAL_CONTROLLER_HW_FILTER == 2
	THERMAL_WRAP_WR32(0x07E007E0, TEMPMONCTL2);	/* both filt and sen interval is 2016*15.5us = 31.25ms */
	THERMAL_WRAP_WR32(0x001F7972, TEMPAHBPOLL);	/* poll is set to 31.25ms */
	THERMAL_WRAP_WR32(0x00000049, TEMPMSRCTL0);	/* temperature sampling control, 2 out of 4 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	THERMAL_WRAP_WR32(0x050A050A, TEMPMONCTL2);	/* both filt and sen interval is 20ms */
	THERMAL_WRAP_WR32(0x001424C4, TEMPAHBPOLL);	/* poll is set to 20ms */
	THERMAL_WRAP_WR32(0x000000DB, TEMPMSRCTL0);	/* temperature sampling control, 4 out of 6 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	THERMAL_WRAP_WR32(0x03390339, TEMPMONCTL2);	/* both filt and sen interval is 12.5ms */
	THERMAL_WRAP_WR32(0x000C96FA, TEMPAHBPOLL);	/* poll is set to 12.5ms */
	THERMAL_WRAP_WR32(0x00000124, TEMPMSRCTL0);	/* temperature sampling control, 8 out of 10 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	THERMAL_WRAP_WR32(0x01C001C0, TEMPMONCTL2);	/* both filt and sen interval is 6.94ms */
	THERMAL_WRAP_WR32(0x0006FE8B, TEMPAHBPOLL);	/* poll is set to 458379*15.15= 6.94ms */
	THERMAL_WRAP_WR32(0x0000016D, TEMPMSRCTL0);	/* temperature sampling control, 16 out of 18 samples */
#else				/* default 1 */
	THERMAL_WRAP_WR32(0x000101AD, TEMPMONCTL2);
	/* filt interval is 1 * 46.540us = 46.54us, sen interval is 429 * 46.540us = 19.96ms */
	/* THERMAL_WRAP_WR32(0x0001035A, TEMPMONCTL2);
	// filt interval is 1 * 46.540us = 46.54us, sen interval is 858 * 46.540us = 39.93ms */
	/* THERMAL_WRAP_WR32(0x00010507, TEMPMONCTL2);
	// filt interval is 1 * 46.540us = 46.54us, sen interval is 1287 * 46.540us = 59.89 ms */
	/* THERMAL_WRAP_WR32(0x00000001, TEMPAHBPOLL);  // poll is set to 1 * 46.540us = 46.540us */
	THERMAL_WRAP_WR32(0x00000300, TEMPAHBPOLL);	/* poll is set to 10u */
	THERMAL_WRAP_WR32(0x00000000, TEMPMSRCTL0);	/* temperature sampling control, 1 sample */
#endif

	THERMAL_WRAP_WR32(0xFFFFFFFF, TEMPAHBTO);	/* exceed this polling time, IRQ would be inserted */

	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET0);	/* times for interrupt occurrance */
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET1);	/* times for interrupt occurrance */

	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw */
	THERMAL_WRAP_WR32(0x800, TEMPADCMUX);
	/* AHB address for auxadc mux selection */
	THERMAL_WRAP_WR32((auxadc_ts_phy_base + 0x00C), TEMPADCMUXADDR);

	THERMAL_WRAP_WR32(0x800, TEMPADCEN);	/* AHB value for auxadc enable */
	/* AHB address for auxadc enable (channel 0 immediate mode selected) */
	THERMAL_WRAP_WR32((auxadc_ts_phy_base + 0x008), TEMPADCENADDR);
	/* this value will be stored to TEMPADCENADDR automatically by hw */

	/* AHB address for auxadc valid bit */
	THERMAL_WRAP_WR32((auxadc_ts_phy_base + 0x040), TEMPADCVALIDADDR);
	/* AHB address for auxadc voltage output */
	THERMAL_WRAP_WR32((auxadc_ts_phy_base + 0x040), TEMPADCVOLTADDR);
	THERMAL_WRAP_WR32(0x0, TEMPRDCTRL);	/* read valid & voltage are at the same register */
	/* indicate where the valid bit is (the 12th bit is valid bit and 1 is valid) */
	THERMAL_WRAP_WR32(0x0000002C, TEMPADCVALIDMASK);
	THERMAL_WRAP_WR32(0x0, TEMPADCVOLTAGESHIFT);	/* do not need to shift */
	THERMAL_WRAP_WR32(0x2, TEMPADCWRITECTRL);	/* enable auxadc mux write transaction */
}


static void thermal_enable_all_periodoc_sensing_point_Bank0(void)
{
	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);	/* enable periodoc temperature sensing point 0, point 1 */
}

static void thermal_enable_all_periodoc_sensing_point_Bank1(void)
{
	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);	/* enable periodoc temperature sensing point 0, point 1 */
}

static void thermal_enable_all_periodoc_sensing_point_Bank2(void)
{
	THERMAL_WRAP_WR32(0x00000007, TEMPMONCTL0);	/* enable periodoc temperature sensing point 0, 1, 2 */
}

static void thermal_enable_all_periodoc_sensing_point_Bank3(void)
{
	THERMAL_WRAP_WR32(0x00000001, TEMPMONCTL0);	/* enable periodoc temperature sensing point 0*/
}


/*
Bank 0 : CA7  (TS2 TS3)
Bank 1 : CA15 (TS2 TS4)
Bank 2 : GPU  (TS1 TS2 TSABB)
Bank 3 : CORE (TS2)
*/

static void thermal_config_Bank0_TS(void)
{
	tscpu_dprintk("thermal_config_Bank0_TS:\n");


	/* Bank0:CA7(TS2 and TS3) */
	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b000 */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP0);

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b001 */
	THERMAL_WRAP_WR32(0x2, TEMPADCPNP1);

	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32((apmixed_phy_base + 0x0604), TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
}

static void thermal_config_Bank1_TS(void)
{

	tscpu_dprintk("thermal_config_Bank1_TS\n");

	/* Bank1:CA15(TS2 and TS4) */
	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b000 */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP0);

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b010 */
	THERMAL_WRAP_WR32(0x3, TEMPADCPNP1);

	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32((apmixed_phy_base + 0x0604), TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
}

static void thermal_config_Bank2_TS(void)
{

	tscpu_dprintk("thermal_config_Bank2_TS\n");

	/* Bank1:GPU(TS1 and TS2 and TSABB) */

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b010 */
	THERMAL_WRAP_WR32(0x0, TEMPADCPNP0);

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b011 */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP1);

	/* TSCON1[5:4]=2'b01 */
	/* TSCON1[2:0]=3'b000 */
	THERMAL_WRAP_WR32(0x10, TEMPADCPNP2);

	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32((apmixed_phy_base + 0x0604), TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
}

static void thermal_config_Bank3_TS(void)
{
	tscpu_dprintk("thermal_config_Bank3_TS\n");


	/* Bank1:CORE(TS2) */
	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b001 */
	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP0);
	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32((apmixed_phy_base + 0x0604), TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
}

static void thermal_config_TS_in_banks(thermal_bank_name bank_num)
{
	tscpu_dprintk("thermal_config_TS_in_banks bank_num=%d\n", bank_num);

	switch (bank_num) {
	case THERMAL_BANK0:	/* bank0,CA7 (TS2 TS3) */
		thermal_config_Bank0_TS();
		break;
	case THERMAL_BANK1:	/* bank1,CA15 (TS2 TS4) */
		thermal_config_Bank1_TS();
		break;
	case THERMAL_BANK2:	/* bank2,GPU (TS1 TS2 TSABB) */
		thermal_config_Bank2_TS();
		break;
	case THERMAL_BANK3:	/* bank3,CORE (TS2) */
		thermal_config_Bank3_TS();
		break;
	default:
		thermal_config_Bank0_TS();
		break;
	}
}

static void thermal_enable_all_periodoc_sensing_point(thermal_bank_name bank_num)
{
	tscpu_dprintk("thermal_config_TS_in_banks bank_num=%d\n", bank_num);

	switch (bank_num) {
	case THERMAL_BANK0:	/* bank0,CA7 (TS2 TS3) */
		thermal_enable_all_periodoc_sensing_point_Bank0();
		break;
	case THERMAL_BANK1:	/* bank1,CA15 (TS2 TS4) */
		thermal_enable_all_periodoc_sensing_point_Bank1();
		break;
	case THERMAL_BANK2:	/* bank2,GPU (TS1 TS2 TSABB) */
		thermal_enable_all_periodoc_sensing_point_Bank2();
		break;
	case THERMAL_BANK3:	/* bank3,CORE (TS2) */
		thermal_enable_all_periodoc_sensing_point_Bank3();
		break;
	default:
		thermal_enable_all_periodoc_sensing_point_Bank0();
		break;
	}
}

/**
 *  temperature2 to set the middle threshold for interrupting CPU. -275000 to disable it.
 */

static void set_tc_trigger_hw_protect(int temperature, int temperature2)
{

	int temp = 0;
	int raw_high, raw_middle, raw_low;


	/* temperature2=80000;  test only */
	tscpu_dprintk("set_tc_trigger_hw_protect t1=%d t2=%d\n", temperature, temperature2);


	/* temperature to trigger SPM state2 */
	raw_high = temperature_to_raw_room(temperature);
	if (temperature2 > -275000)
		raw_middle = temperature_to_raw_room(temperature2);
	raw_low = temperature_to_raw_room(5000);


	temp = DRV_Reg32(TEMPMONINT);
	/* tscpu_printk("set_tc_trigger_hw_protect 1 TEMPMONINT:temp=0x%x\n",temp); */
	/* THERMAL_WRAP_WR32(temp & 0x1FFFFFFF, TEMPMONINT);     // disable trigger SPM interrupt */
	THERMAL_WRAP_WR32(temp & 0x00000000, TEMPMONINT);	/* disable trigger SPM interrupt */

	/* set hot to wakeup event control */
	THERMAL_WRAP_WR32(0x20000, TEMPPROTCTL);

	THERMAL_WRAP_WR32(raw_low, TEMPPROTTA);
	if (temperature2 > -275000)
		/* register will remain unchanged if -275000... */
		THERMAL_WRAP_WR32(raw_middle, TEMPPROTTB);

	THERMAL_WRAP_WR32(raw_high, TEMPPROTTC);	/* set hot to HOT wakeup event */


	/*trigger cold ,normal and hot interrupt */
	/* remove for temp       THERMAL_WRAP_WR32(temp | 0xE0000000, TEMPMONINT);
	// enable trigger SPM interrupt */
	/*Only trigger hot interrupt */
	if (temperature2 > -275000)
		/* enable trigger middle & Hot SPM interrupt */
		THERMAL_WRAP_WR32(temp | 0xC0000000, TEMPMONINT);
	else
		/* enable trigger Hot SPM interrupt */
		THERMAL_WRAP_WR32(temp | 0x80000000, TEMPMONINT);
}

void mtkts_dump_cali_info(void)
{
	tscpu_dprintk("[calibration] g_adc_ge_t      = 0x%x\n", g_adc_ge_t);
	tscpu_dprintk("[calibration] g_adc_oe_t      = 0x%x\n", g_adc_oe_t);
	tscpu_dprintk("[calibration] g_degc_cali     = 0x%x\n", g_degc_cali);
	tscpu_dprintk("[calibration] g_adc_cali_en_t = 0x%x\n", g_adc_cali_en_t);
	tscpu_dprintk("[calibration] g_o_slope       = 0x%x\n", g_o_slope);
	tscpu_dprintk("[calibration] g_o_slope_sign  = 0x%x\n", g_o_slope_sign);
	tscpu_dprintk("[calibration] g_id            = 0x%x\n", g_id);

	tscpu_dprintk("[calibration] g_o_vtsmcu2     = 0x%x\n", g_o_vtsmcu2);
	tscpu_dprintk("[calibration] g_o_vtsmcu3     = 0x%x\n", g_o_vtsmcu3);
	tscpu_dprintk("[calibration] g_o_vtsmcu4     = 0x%x\n", g_o_vtsmcu4);
}


static void thermal_cal_prepare(void)
{
	U32 temp0 = 0, temp1 = 0, temp2 = 0;
#if 1
	/* Thermal       kernel  0x10206528      Jerry   28 */
	/* Thermal       kernel  0x1020652C      Jerry   29 */
	/* Thermal       kernel  0x10206530      Jerry   30 */


	temp0 = get_devinfo_with_index(29);
	temp1 = get_devinfo_with_index(28);
	temp2 = get_devinfo_with_index(30);

	/* temp2 = get_devinfo_with_index(18); */
	/* temp2 = get_devinfo_with_index(19); */
#else

	temp0 = DRV_Reg32(0xF020652C);	/* 95 */
	temp1 = DRV_Reg32(0xF0206528);	/* 95 */
	temp2 = DRV_Reg32(0xF0206530);	/* 95 */
#endif


	tscpu_printk("[calibration] temp0=0x%x, temp1=0x%x\n", temp0, temp1);
	/* mtktscpu_dprintk("thermal_cal_prepare\n"); */

	g_adc_ge_t = ((temp0 & 0xFFC00000) >> 22);	/* ADC_GE_T    [9:0] *(0xF020652C)[31:22] */
	g_adc_oe_t = ((temp0 & 0x003FF000) >> 12);	/* ADC_OE_T    [9:0] *(0xF020652C)[21:12] */

	g_o_vtsmcu1 = (temp1 & 0x03FE0000) >> 17;	/* O_VTSMCU1    (9b) *(0xF0206528)[25:17] */
	g_o_vtsmcu2 = (temp1 & 0x0001FF00) >> 8;	/* O_VTSMCU2    (9b) *(0xF0206528)[16:8] */
	g_o_vtsmcu3 = (temp0 & 0x000001FF);	/* O_VTSMCU3    (9b) *(0xF020652C)[8:0] */
	g_o_vtsmcu4 = (temp2 & 0xFF800000) >> 23;	/* O_VTSMCU4    (9b) *(0xF0206530)[31:23] */
	g_o_vtsabb = (temp2 & 0x007FC000) >> 14;	/* O_VTSABB     (9b) *(0xF0206530)[22:14] */

	g_degc_cali = (temp1 & 0x0000007E) >> 1;	/* DEGC_cali    (6b) *(0xF0206528)[6:1] */
	g_adc_cali_en_t = (temp1 & 0x00000001);	/* ADC_CALI_EN_T(1b) *(0xF0206528)[0] */
	g_o_slope_sign = (temp1 & 0x00000080) >> 7;	/* O_SLOPE_SIGN (1b) *(0xF0206528)[7] */
	g_o_slope = (temp1 & 0xFC000000) >> 26;	/* O_SLOPE      (6b) *(0xF0206528)[31:26] */

	g_id = (temp0 & 0x00000200) >> 9;	/* ID           (1b) *(0xF020652C)[9] */

	/*
	   Check ID bit
	   If ID=0 (TSMC sample)    , ignore O_SLOPE EFuse value and set O_SLOPE=0.
	   If ID=1 (non-TSMC sample), read O_SLOPE EFuse value for following calculation.
	 */
	if (g_id == 0)
		g_o_slope = 0;

	/* g_adc_cali_en_t=0;//test only */
	if (g_adc_cali_en_t == 1)
		/* thermal_enable = true; */
		tscpu_dprintk("This sample is not Thermal calibrated\n");
	else {
		tscpu_printk("This sample is not Thermal calibrated\n");
#if 1				/* default */
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_degc_cali = 40;
		g_o_slope = 0;
		g_o_slope_sign = 0;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		g_o_vtsmcu3 = 260;
		g_o_vtsmcu4 = 260;
		g_o_vtsabb = 260;
#endif

	}
}

static void thermal_cal_prepare_2(U32 ret)
{
	S32 format_1 = 0, format_2 = 0, format_3 = 0, format_4 = 0, format_5 = 0;

	tscpu_printk("thermal_cal_prepare_2\n");

	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format_1 = (g_o_vtsmcu1 + 3350 - g_oe);
	format_2 = (g_o_vtsmcu2 + 3350 - g_oe);
	format_3 = (g_o_vtsmcu3 + 3350 - g_oe);
	format_4 = (g_o_vtsmcu4 + 3350 - g_oe);
	format_5 = (g_o_vtsabb + 3350 - g_oe);

	g_x_roomt[0] = (((format_1 * 10000) / 4096) * 10000) / g_gain;	/* g_x_roomt1 * 10000 */
	g_x_roomt[1] = (((format_2 * 10000) / 4096) * 10000) / g_gain;	/* g_x_roomt2 * 10000 */
	g_x_roomt[2] = (((format_3 * 10000) / 4096) * 10000) / g_gain;	/* g_x_roomt3 * 10000 */
	g_x_roomt[3] = (((format_4 * 10000) / 4096) * 10000) / g_gain;	/* g_x_roomt4 * 10000 */
	g_x_roomt[4] = (((format_5 * 10000) / 4096) * 10000) / g_gain;	/* g_x_roomtabb * 10000 */

	tscpu_dprintk("[calibration] g_ge         = 0x%x\n", g_ge);
	tscpu_dprintk("[calibration] g_gain       = 0x%x\n", g_gain);

	tscpu_dprintk("[calibration] g_x_roomt1   = 0x%x\n", g_x_roomt[0]);
	tscpu_dprintk("[calibration] g_x_roomt2   = 0x%x\n", g_x_roomt[1]);
	tscpu_dprintk("[calibration] g_x_roomt3   = 0x%x\n", g_x_roomt[2]);
	tscpu_dprintk("[calibration] g_x_roomt4   = 0x%x\n", g_x_roomt[3]);
	tscpu_dprintk("[calibration] g_x_roomtabb = 0x%x\n", g_x_roomt[4]);

}

static S32 temperature_to_raw_room(U32 ret)
{
	/* Ycurr = [(Tcurr - DEGC_cali/2)*(165+O_slope)*(18/15)*(1/10000)+X_roomtabb]*Gain*4096 + OE */

	S32 t_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3[THERMAL_SENSOR_NUM] = { 0 };
	S32 format_4[THERMAL_SENSOR_NUM] = { 0 };
	S32 i, index = 0, temp = 0;


	tscpu_dprintk("temperature_to_raw_room\n");

	if (g_o_slope_sign == 0) {	/* O_SLOPE is Positive. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 + g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain) / 100000 + g_oe;
			tscpu_dprintk
			    ("[Temperature_to_raw_roomt][roomt%d] format_1=%d, format_2=%d, format_3=%d, format_4=%d\n",
			     i, format_1, format_2, format_3[i], format_4[i]);
		}
	} else {		/* O_SLOPE is Negative. */

		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 - g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain) / 100000 + g_oe;
			tscpu_dprintk
			    ("[Temperature_to_raw_roomt][roomt%d] format_1=%d, format_2=%d, format_3=%d, format_4=%d\n",
			     i, format_1, format_2, format_3[i], format_4[i]);
		}
	}

	temp = 0;
	for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
		if (temp < format_4[i]) {
			temp = format_4[i];
			index = i;
		}
	}

	tscpu_dprintk("[Temperature_to_raw_roomt] temperature=%d, raw[%d]=%d", ret, index,
		      format_4[index]);
	return format_4[index];

}

static S32 raw_to_temperature_roomt(U32 ret, thermal_sensor_name ts_name)
{
	S32 t_current = 0;
	S32 y_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;
	S32 xtoomt = 0;


	xtoomt = g_x_roomt[ts_name];

	tscpu_dprintk("raw_to_temperature_room,ts_name=%d,xtoomt=%d\n", ts_name, xtoomt);

	if (ret == 0)
		return 0;

	/* format_1 = (g_degc_cali / 2); */
	/* format_1 = (g_degc_cali*10 / 2); */
	format_1 = ((g_degc_cali * 10) >> 1);
	format_2 = (y_curr - g_oe);
	/* format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - xtoomt; */
	format_3 = (((((format_2) * 10000) >> 12) * 10000) / g_gain) - xtoomt;
	format_3 = format_3 * 15 / 18;

	/* format_4 = ((format_3 * 100) / 139); // uint = 0.1 deg */
	if (g_o_slope_sign == 0)
		/* format_4 = ((format_3 * 100) / (139+g_o_slope)); // uint = 0.1 deg */
		format_4 = ((format_3 * 100) / (165 + g_o_slope));	/* uint = 0.1 deg */
	else
		/* format_4 = ((format_3 * 100) / (139-g_o_slope)); // uint = 0.1 deg */
		format_4 = ((format_3 * 100) / (165 - g_o_slope));	/* uint = 0.1 deg */

	/* format_4 = format_4 - (2 * format_4); */
	format_4 = format_4 - (format_4 << 1);

	/* t_current = (format_1 * 10) + format_4; // uint = 0.1 deg */
	t_current = format_1 + format_4;	/* uint = 0.1 deg */

	tscpu_dprintk("raw_to_temperature_room,t_current=%d\n", t_current);
	return t_current;
}

static void thermal_calibration(void)
{
	if (g_adc_cali_en_t == 0)
		pr_info("#####  Not Calibration  ######\n");

	/* tscpu_dprintk("thermal_calibration\n"); */
	thermal_cal_prepare_2(0);
}

int get_immediate_temp2_wrap(void)
{
	int curr_temp;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(THERMAL_BANK2);
	read_each_bank_TS(THERMAL_BANK2);

	curr_temp = GPU_TSABB_T;

	mt_ptp_unlock(&flags);

	tscpu_dprintk("get_immediate_temp2_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

/*
Bank 0 : CA7  (TS2 TS3)
Bank 1 : CA15 (TS2 TS4)
Bank 2 : GPU  (TS1 TS2 TSABB)
Bank 3 : CORE (TS2)
*/
int get_immediate_ts1_wrap(void)
{
	int curr_temp;
	unsigned long flags;

	mt_ptp_lock(&flags);


	mtktscpu_switch_bank(THERMAL_BANK2);
	read_each_bank_TS(THERMAL_BANK2);

	curr_temp = GPU_TS1_T;

	mt_ptp_unlock(&flags);

	tscpu_dprintk("get_immediate_ts1_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp, curr_temp2;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(THERMAL_BANK0);
	read_each_bank_TS(THERMAL_BANK0);
	mtktscpu_switch_bank(THERMAL_BANK1);
	read_each_bank_TS(THERMAL_BANK1);
	mtktscpu_switch_bank(THERMAL_BANK2);
	read_each_bank_TS(THERMAL_BANK2);
	mtktscpu_switch_bank(THERMAL_BANK3);
	read_each_bank_TS(THERMAL_BANK3);
	curr_temp = MAX(CA7_TS2_T, CA15_TS2_T);
	curr_temp2 = MAX(GPU_TS2_T, CORE_TS2_T);
	curr_temp = MAX(curr_temp, curr_temp2);

	mt_ptp_unlock(&flags);
	tscpu_dprintk("get_immediate_ts2_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(THERMAL_BANK0);
	read_each_bank_TS(THERMAL_BANK0);
	curr_temp = CA7_TS3_T;

	mt_ptp_unlock(&flags);

	tscpu_dprintk("get_immediate_ts3_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts4_wrap(void)
{
	int curr_temp;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(THERMAL_BANK1);
	read_each_bank_TS(THERMAL_BANK1);
	curr_temp = CA15_TS4_T;

	mt_ptp_unlock(&flags);

	tscpu_dprintk("get_immediate_ts4_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

static int read_tc_raw_and_temp(u32 *tempmsr_name,
				thermal_sensor_name ts_name,
				int *ts_raw)
{
	int temp = 0, raw = 0;

	tscpu_dprintk("read_tc_raw_temp,tempmsr_name=0x%x,ts_name=%d\n", tempmsr_name, ts_name);

	raw = (tempmsr_name != 0) ? (DRV_Reg32(tempmsr_name) & 0x0fff) : 0;
	temp = (tempmsr_name != 0) ? raw_to_temperature_roomt(raw, ts_name) : 0;

	*ts_raw = raw;
	tscpu_dprintk("read_tc_raw_temp,ts_raw=%d,temp=%d\n", *ts_raw, temp * 100);

	return temp*100;
}

static void read_each_bank_TS(thermal_bank_name bank_num)
{

	tscpu_dprintk("read_each_bank_TS,bank_num=%d\n", bank_num);

	switch (bank_num) {
	case THERMAL_BANK0:
		/* Bank 0 : CA7  (TS2 TS3) */
		CA7_TS2_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR0, THERMAL_SENSOR2, &CA7_TS2_R);
		CA7_TS3_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR1, THERMAL_SENSOR3, &CA7_TS3_R);
		break;
	case THERMAL_BANK1:
		/* Bank 1 : CA15 (TS2 TS4) */
		CA15_TS2_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR0, THERMAL_SENSOR2, &CA15_TS2_R);
		CA15_TS4_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR1, THERMAL_SENSOR4, &CA15_TS4_R);
		break;
	case THERMAL_BANK2:
		/* Bank 2 : GPU  (TS1 TS2 TSABB) */
		GPU_TS1_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR0, THERMAL_SENSOR1, &GPU_TS1_R);
		GPU_TS2_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR1, THERMAL_SENSOR2, &GPU_TS2_R);
		GPU_TSABB_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR2, THERMAL_SENSORABB, &GPU_TSABB_R);
		break;
	case THERMAL_BANK3:
		/* Bank 3 : CORE (TS2) */
		CORE_TS2_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR0, THERMAL_SENSOR2, &CORE_TS2_R);
		break;
	default:
		CA7_TS2_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR0, THERMAL_SENSOR2, &CA7_TS2_R);
		CA7_TS3_T =
		    read_tc_raw_and_temp((u32 *)TEMPMSR1, THERMAL_SENSOR3, &CA7_TS3_R);
		break;
	}
}

static void read_all_bank_temperature(void)
{
	int i = 0;
	unsigned long flags;
	tscpu_dprintk("read_all_bank_temperature\n");


	mt_ptp_lock(&flags);

	for (i = 0; i < ROME_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		read_each_bank_TS(i);
	}

	mt_ptp_unlock(&flags);
}

int tscpu_get_temp(unsigned long *t)
{
	int ret = 0;
	int curr_temp;
	int curr_temp1;
	int curr_temp2;

	int temp_temp;
	int bank0_T;
	int bank1_T;
	int bank2_T;
	int bank3_T;

	static int last_cpu_real_temp;

	read_all_bank_temperature();

	bank0_T = MAX(CA7_TS2_T, CA7_TS3_T);
	bank1_T = MAX(CA15_TS2_T, CA15_TS4_T);
	bank2_T = MAX(GPU_TS1_T, GPU_TS2_T);
	bank2_T = MAX(bank2_T, GPU_TSABB_T);
	bank3_T = CORE_TS2_T;
	curr_temp1 = MAX(bank0_T, bank1_T);
	curr_temp2 = MAX(bank2_T, bank3_T);
	curr_temp = MAX(curr_temp1, curr_temp2);

	if ((curr_temp > (MTKTSCPU_TEMP_CRIT - 15000)) || (curr_temp < -30000) || (curr_temp > 85000)
	    || (curr_temp2 > 85000))
		tscpu_printk("CPU T=%d\n", curr_temp);

	temp_temp = curr_temp;
	if (curr_temp != 0) {/* not resumed from suspensio... */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {/* invalid range */
			tscpu_printk("CPU temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_cpu_real_temp != 0) {
			/* delta 40C, invalid change */
			if ((curr_temp - last_cpu_real_temp > 40000) || (last_cpu_real_temp - curr_temp > 40000)) {
				tscpu_printk("CPU temp float hugely temp=%d, lasttemp=%d\n",
					     curr_temp, last_cpu_real_temp);
				/* tscpu_printk("RAW_TS2 = %d,RAW_TS3 = %d,RAW_TS4 = %d\n",RAW_TS2,RAW_TS3,RAW_TS4); */
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_cpu_real_temp = curr_temp;
	curr_temp = temp_temp;
	*t = (unsigned long)curr_temp;
	return ret;
}
EXPORT_SYMBOL(tscpu_get_temp);

/* pause ALL periodoc temperature sensing point */
static void thermal_pause_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;

	tscpu_printk("thermal_pause_all_periodoc_temp_sensing\n");

	mt_ptp_lock(&flags);

	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		temp = DRV_Reg32(TEMPMSRCTL1);
		/* set bit1=bit2=bit3=1 to pause sensing point 0,1,2 */
		DRV_WriteReg32(TEMPMSRCTL1, (temp | 0x10E));
	}

	mt_ptp_unlock(&flags);
}

/* release ALL periodoc temperature sensing point */
static void thermal_release_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;

	mt_ptp_lock(&flags);

	/*config bank0,1,2 */
	for (i = 0; i < ROME_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		temp = DRV_Reg32(TEMPMSRCTL1);
		/* set bit1=bit2=bit3=0 to release sensing point 0,1,2 */
		DRV_WriteReg32(TEMPMSRCTL1, ((temp & (~0x10E))));
	}

	mt_ptp_unlock(&flags);

}

/* disable ALL periodoc temperature sensing point */
static void thermal_disable_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	mt_ptp_lock(&flags);

	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);
	}

	mt_ptp_unlock(&flags);
}

static void tscpu_clear_all_temp(void)
{
	CA7_TS2_T = 0;
	CA7_TS3_T = 0;
	CA15_TS2_T = 0;
	CA15_TS4_T = 0;
	GPU_TS1_T = 0;
	GPU_TS2_T = 0;
	GPU_TSABB_T = 0;
	CORE_TS2_T = 0;
}

static int mtktscpu_switch_bank(thermal_bank_name bank)
{
	tscpu_dprintk("mtktscpu_switch_bank =bank=%d\n", bank);

	switch (bank) {
	case THERMAL_BANK0:	/* bank0,CA7 (TS2 TS3) */
		thermal_clrl(PTPCORESEL, 0xF);	/* bank0 */
		break;
	case THERMAL_BANK1:	/* bank1,CA15 (TS2 TS4) */
		thermal_clrl(PTPCORESEL, 0xF);
		thermal_setl(PTPCORESEL, 0x1);	/* bank1 */
		break;
	case THERMAL_BANK2:	/* bank2,GPU (TS1 TS2 TSABB) */
		thermal_clrl(PTPCORESEL, 0xF);
		thermal_setl(PTPCORESEL, 0x2);	/* bank2 */
		break;
	case THERMAL_BANK3:	/* bank3,CORE (TS2) */
		thermal_clrl(PTPCORESEL, 0xF);
		thermal_setl(PTPCORESEL, 0x3);	/* bank3 */
		break;
	default:
		thermal_clrl(PTPCORESEL, 0xF);	/* bank0 */
		break;
	}
	return 0;
}

static void thermal_initial_all_bank(void)
{
	int i = 0;
	unsigned long flags;
	UINT32 temp = 0;
	tscpu_printk("thermal_initial_all_bank,ROME_BANK_NUM=%d\n", ROME_BANK_NUM);

	mt_ptp_lock(&flags);

	/* AuxADC Initialization,ref MT6592_AUXADC.doc // TODO: check this line */
	temp = DRV_Reg32(AUXADC_CON0_V);	/* Auto set enable for CH11 */
	temp &= 0xFFFFF7FF;	/* 0: Not AUTOSET mode */
	THERMAL_WRAP_WR32(temp, AUXADC_CON0_V);	/* disable auxadc channel 11 synchronous mode */
	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_CLR_V);	/* disable auxadc channel 11 immediate mode */


	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_reset_and_initial();
		thermal_config_TS_in_banks(i);
	}

	/* enable auxadc channel 11 immediate mode */
	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_SET_V);

	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_enable_all_periodoc_sensing_point(i);
	}
	mt_ptp_unlock(&flags);
}

static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2)
{
	int i = 0, wd_api_ret;
	unsigned long flags;
	struct wd_api *wd_api;

	tscpu_dprintk("tscpu_config_all_tc_hw_protect,temperature=%d,temperature2=%d,\n",
		      temperature, temperature2);
	/*spend 860~1463 us */
	/*Thermal need to config to direct reset mode
	   this API provide by Weiqi Fu(RGU SW owner). */
	wd_api_ret = get_wd_api(&wd_api);
	if (wd_api_ret >= 0)
		wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);	/* reset mode */
	else {
		tscpu_printk("%d FAILED TO GET WD API\n", __LINE__);
		BUG();
	}

	mt_ptp_lock(&flags);

	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		set_tc_trigger_hw_protect(temperature, temperature2);	/* Move thermal HW protection ahead... */
	}

	mt_ptp_unlock(&flags);


	/*Thermal need to config to direct reset mode
	   this API provide by Weiqi Fu(RGU SW owner). */
	wd_api->wd_thermal_direct_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);	/* reset mode */

}

static void tscpu_reset_thermal(void)
{
	int temp = 0;
	/* reset thremal ctrl */
	temp = DRV_Reg32(PERI_GLOBALCON_RST0);	/* MT6592_PERICFG.xml // TODO: check this line */
	temp |= 0x00010000;	/* 1: Reset THERM */
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp &= 0xFFFEFFFF;	/* 0: Not reset THERM */
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	tscpu_thermal_clock_on();
}

static void tscpu_fast_initial_sw_workaround(void)
{
	int i = 0;
	unsigned long flags;

	mt_ptp_lock(&flags);

	/*config bank0,1,2,3 */
	for (i = 0; i < ROME_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_fast_init();
	}

	mt_ptp_unlock(&flags);
}

int tscpu_get_cpu_temp_met(MTK_THERMAL_SENSOR_CPU_ID_MET id)
{
	return 0;
}
EXPORT_SYMBOL(tscpu_get_cpu_temp_met);

static int thermal_fast_init(void)
{
	UINT32 temp = 0;
	UINT32 cunt = 0;
	/* UINT32 temp1 = 0,temp2 = 0,temp3 = 0,count=0; */
	temp = 0xDA1;
	DRV_WriteReg32(PTPSPARE2, (0x00001000 + temp));	/* write temp to spare register */


	DRV_WriteReg32(TEMPMONCTL1, 1);	/* counting unit is 320 * 31.25us = 10ms */
	DRV_WriteReg32(TEMPMONCTL2, 1);	/* sensing interval is 200 * 10ms = 2000ms */
	DRV_WriteReg32(TEMPAHBPOLL, 1);	/* polling interval to check if temperature sense is ready */

	DRV_WriteReg32(TEMPAHBTO, 0x000000FF);	/* exceed this polling time, IRQ would be inserted */
	DRV_WriteReg32(TEMPMONIDET0, 0x00000000);	/* times for interrupt occurrance */
	DRV_WriteReg32(TEMPMONIDET1, 0x00000000);	/* times for interrupt occurrance */

	DRV_WriteReg32(TEMPMSRCTL0, 0x0000000);	/* temperature measurement sampling control */

	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw */
	DRV_WriteReg32(TEMPADCPNP0, 0x1);
	DRV_WriteReg32(TEMPADCPNP1, 0x2);
	DRV_WriteReg32(TEMPADCPNP2, 0x3);
	DRV_WriteReg32(TEMPADCPNP3, 0x4);

	/* AHB address for pnp sensor mux selection */
	DRV_WriteReg32(TEMPPNPMUXADDR, (thermal_phy_base + 0x420));
	/* AHB address for auxadc mux selection */
	DRV_WriteReg32(TEMPADCMUXADDR, (thermal_phy_base + 0x420));
	/* AHB address for auxadc enable */
	DRV_WriteReg32(TEMPADCENADDR, (thermal_phy_base + 0x424));
	/* AHB address for auxadc valid bit */
	DRV_WriteReg32(TEMPADCVALIDADDR, (thermal_phy_base + 0x428));
	/* AHB address for auxadc voltage output */
	DRV_WriteReg32(TEMPADCVOLTADDR, (thermal_phy_base + 0x428));

	/* read valid & voltage are at the same register */
	DRV_WriteReg32(TEMPRDCTRL, 0x0);
	/* indicate where the valid bit is (the 12th bit is valid bit and 1 is valid) */
	DRV_WriteReg32(TEMPADCVALIDMASK, 0x0000002C);
	DRV_WriteReg32(TEMPADCVOLTAGESHIFT, 0x0);	/* do not need to shift */
	DRV_WriteReg32(TEMPADCWRITECTRL, 0x3);	/* enable auxadc mux & pnp write transaction */

	/* enable all interrupt except filter sense and immediate sense interrupt */
	DRV_WriteReg32(TEMPMONINT, 0x00000000);

	/* enable all sensing point (sensing point 2 is unused) */
	DRV_WriteReg32(TEMPMONCTL0, 0x0000000F);

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR3) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		temp = DRV_Reg32(TEMPMSR3) & 0x0fff;
	}

	return 0;
}


int tscpu_get_bL_temp(thermal_TS_name ts_name)
{
	int bank_T = 0;

	if (ts_name == THERMAL_CA7)
		bank_T = MAX(CA7_TS2_T, CA7_TS3_T);
	else if (ts_name == THERMAL_CA15)
		bank_T = MAX(CA15_TS2_T, CA15_TS4_T);
	else if (ts_name == THERMAL_GPU) {
		bank_T = MAX(GPU_TS1_T, GPU_TS2_T);
		bank_T = MAX(bank_T, GPU_TSABB_T);
	} else if (ts_name == THERMAL_CORE)
		bank_T = CORE_TS2_T;

	return bank_T;
}

void tscpu_start_thermal_timer(void)
{
	return;
}

void tscpu_cancel_thermal_timer(void)
{
	return;
}

#ifdef CONFIG_OF
static int get_io_reg_base(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,THERM_CTRL");
	BUG_ON(node == 0);
	if (node)
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);

	of_property_read_u32(node, "reg", &thermal_phy_base);

	/* get thermal irq num */
	thermal_irq_number = irq_of_parse_and_map(node, 0);

	if (!thermal_irq_number)
		return 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,AUXADC");
	BUG_ON(node == 0);
	if (node)
		/* Setup IO addresses */
		auxadc_ts_base = of_iomap(node, 0);

	of_property_read_u32(node, "reg", &auxadc_ts_phy_base);

	node = of_find_compatible_node(NULL, NULL, "mediatek,PERICFG");
	BUG_ON(node == 0);
	if (node)
		/* Setup IO addresses */
		pericfg_base = of_iomap(node, 0);

	node = of_find_compatible_node(NULL, NULL, "mediatek,APMIXED");
	BUG_ON(node == 0);
	if (node)
		/* Setup IO addresses */
		apmixed_ts_base = of_iomap(node, 0);
	of_property_read_u32(node, "reg", &apmixed_phy_base);
	return 1;
}
#endif

static int mtktscpu_suspend(struct platform_device *dev, pm_message_t state)
{
	int cnt = 0;
	int temp = 0;

	tscpu_printk("%s\n", __func__);
	g_tc_resume = 1;	/* set "1", don't read temp during suspend */

	if (talking_flag == false) {
		tscpu_dprintk("%s no talking\n", __func__);

		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 20)
				pr_err("THAHBST0 = 0x%x,cnt=%d, %d\n", temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing point 0~2 */
				thermal_pause_all_periodoc_temp_sensing();	/* TEMPMSRCTL1 */
				break;
			}
			udelay(2);
			cnt++;
		}

		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

		tscpu_thermal_clock_off();

		/*TSCON1[5:4]=2'b11, Buffer off */
		/* turn off the sensor buffer to save power */
		THERMAL_WRAP_WR32(DRV_Reg32(TS_CON1) | 0x00000030, TS_CON1);
	}
	return 0;
}

static int mtktscpu_resume(struct platform_device *dev)
{
	int cnt = 0;
	int temp = 0;

	tscpu_printk("%s\n", __func__);

	g_tc_resume = 1;	/* set "1", don't read temp during start resume */

	if (talking_flag == false) {

		tscpu_reset_thermal();

		temp = DRV_Reg32(TS_CON1);
		temp &= ~(0x00000030);	/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
		THERMAL_WRAP_WR32(temp, TS_CON1);	/* read abb need */
		/* RG_TS2AUXADC < set from 2'b11 to 2'b00 when resume
		   wait 100uS than turn on thermal controller.
		*/

		udelay(200);

		/*add this function to read all temp first to avoid
		   write TEMPPROTTC first time will issue an fake signal to RGU */
		tscpu_fast_initial_sw_workaround();

		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 20)
				pr_err("THAHBST0 = 0x%x,cnt=%d, %d\n", temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing point 0~2 */
				thermal_pause_all_periodoc_temp_sensing();	/* TEMPMSRCTL1 */
				break;
			}
			udelay(2);
			cnt++;
		}
		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

		/*Normal initial */
		thermal_initial_all_bank();

		thermal_release_all_periodoc_temp_sensing();	/* must release before start */

		tscpu_clear_all_temp();

		tscpu_config_all_tc_hw_protect(MTKTSCPU_TEMP_CRIT, tc_mid_trip);


	}

	g_tc_resume = 2;	/* set "2", resume finish,can read temp */

	return 0;
}

static int mtk_ts_cpu_init(struct platform_device *pdev)
{
	int err = 0;
	int cnt = 0;
	int temp = 0;

	int wd_api_ret = 0;
	struct wd_api *wd_api;

	tscpu_printk("%s\n", __func__);

	clk_peri_therm = devm_clk_get(&pdev->dev, "PERISYS_PERI_THERM");
	BUG_ON(IS_ERR(clk_peri_therm));

	/*
	   default is dule mode(irq/reset), if not to config this and hot happen,
	   system will reset after 30 secs

	   Thermal need to config to direct reset mode
	   this API provide by Weiqi Fu(RGU SW owner).
	 */
	wd_api_ret = get_wd_api(&wd_api);
	if (wd_api_ret >= 0)
		wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);	/* reset mode */
	else {
		tscpu_printk("FAILED TO GET WD API\n");
		BUG();
	}

#ifdef CONFIG_OF
	if (get_io_reg_base() == 0)
		return 0;
#endif

	thermal_cal_prepare();
	thermal_calibration();

	tscpu_reset_thermal();

	/*
	   TS_CON1 default is 0x30, this is buffer off
	   we should turn on this buffer berore we use thermal sensor,
	   or this buffer off will let TC read a very small value from auxadc
	   and this small value will trigger thermal reboot
	 */

	temp = DRV_Reg32(TS_CON1);
	tscpu_printk("%s :TS_CON1=0x%x\n", __func__, temp);
	temp &= ~(0x00000030);	/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	THERMAL_WRAP_WR32(temp, TS_CON1);	/* read abb need */

	/* RG_TS2AUXADC < set from 2'b11 to 2'b00 when resume
	   wait 100uS than turn on thermal controller.
	*/

	udelay(200);

	/*add this function to read all temp first to avoid
	   write TEMPPROTTC first will issue an fake signal to RGU */
	tscpu_fast_initial_sw_workaround();

	while (cnt < 50) {
		temp = (DRV_Reg32(THAHBST0) >> 16);
		if (cnt > 20)
			pr_err("THAHBST0 = 0x%x,cnt=%d, %d\n", temp, cnt, __LINE__);
		if (temp == 0x0) {
			/* pause all periodoc temperature sensing point 0~2 */
			thermal_pause_all_periodoc_temp_sensing();	/* TEMPMSRCTL1 */
			break;
		}
		udelay(2);
		cnt++;
	}

	/* disable periodic temp measurement on sensor 0~2 */
	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

	thermal_initial_all_bank();

	thermal_release_all_periodoc_temp_sensing();	/* must release before start */

	read_all_bank_temperature();

#ifdef CONFIG_OF
	err =
		request_irq(thermal_irq_number,
			    thermal_all_bank_interrupt_handler,
			    IRQF_TRIGGER_LOW,
			    THERMAL_NAME,
			    NULL);
	if (err)
		tscpu_printk("%s IRQ register fail\n", __func__);
#else
	err =
		request_irq(THERM_CTRL_IRQ_BIT_ID,
			    thermal_all_bank_interrupt_handler,
			    IRQF_TRIGGER_LOW,
			    THERMAL_NAME,
			    NULL);
	if (err)
		tscpu_printk("%s IRQ register fail\n", __func__);
#endif

	tscpu_config_all_tc_hw_protect(MTKTSCPU_TEMP_CRIT, tc_mid_trip);

	return err;
}

/*
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * THERMAL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static DEFINE_MUTEX(therm_lock);

struct mtktscpu_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *therm_fw;
};

static struct mtk_thermal_platform_data mtktscpu_thermal_data = {
	.num_trips = THERMAL_MAX_TRIPS,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 500,
	.trips[0] = {.temp = 95000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 1, .lower = 0},
	},
	.trips[1] = {.temp = 97000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 2, .lower = 1},
	},
	.trips[2] = {.temp = 99000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 3, .lower = 2},
	},
	.trips[3] = {.temp = 101000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 4, .lower = 3},
	},
	.trips[4] = {.temp = 103000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 1, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 5, .lower = 4},
	},
	.trips[5] = {.temp = 105000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 2, .lower = 1},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 6, .lower = 5},
	},
	.trips[6] = {.temp = 107000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 3, .lower = 2},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 6},
	},
	.trips[7] = {.temp = 109000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 4, .lower = 3},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[8] = {.temp = 111000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 5, .lower = 4},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[9] = {.temp = 113000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 6, .lower = 5},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[10] = {.temp = 115000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 7, .lower = 6},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[11] = {.temp = MTKTSCPU_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL, .hyst = 0},
};

static int mtktscpu_match_cdev(struct thermal_cooling_device *cdev,
			       struct trip_t *trip,
			       int *index)
{
	int i;
	if (!strlen(cdev->type))
		return -EINVAL;

	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		if (!strcmp(cdev->type, trip->cdev[i].type)) {
			*index = i;
			return 0;
		}
	return -ENODEV;
}

static int mtktscpu_cdev_bind(struct thermal_zone_device *thermal,
			      struct thermal_cooling_device *cdev)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	struct trip_t *trip = NULL;
	int index = -1;
	struct cdev_t *cool_dev;
	unsigned long max_state, upper, lower;
	int i, ret = -EINVAL;

	cdev->ops->get_max_state(cdev, &max_state);

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];

		if (mtktscpu_match_cdev(cdev, trip, &index))
			continue;

		if (index == -1)
			return -EINVAL;

		cool_dev = &(trip->cdev[index]);
		lower = cool_dev->lower;
		upper =  cool_dev->upper > max_state ? max_state : cool_dev->upper;
		ret = thermal_zone_bind_cooling_device(thermal,
						       i,
						       cdev,
						       upper,
						       lower);
		dev_info(&cdev->device, "%s bind to %d: idx=%d: u=%ld: l=%ld: %d-%s\n", cdev->type,
			 i, index, upper, lower, ret, ret ? "fail" : "succeed");
	}
	return ret;
}

static int mtktscpu_cdev_unbind(struct thermal_zone_device *thermal,
				struct thermal_cooling_device *cdev)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	struct trip_t *trip;
	int i, ret = -EINVAL;
	int index = -1;

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];
		if (mtktscpu_match_cdev(cdev, trip, &index))
			continue;
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n", cdev->type,
			 i, ret ? "fail" : "succeed");
	}
	return ret;
}

static int mtktscpu_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	return tscpu_get_temp(t);
}

static int mtktscpu_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int mtktscpu_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tzone->therm_work);
	return 0;
}

static int mtktscpu_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int mtktscpu_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int mtktscpu_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int mtktscpu_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	int i;
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	for (i = 0; i < pdata->num_trips; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*t = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static int mtktscpu_get_trip_hyst(struct thermal_zone_device *thermal,
						int trip,
						unsigned long *hyst)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*hyst = pdata->trips[trip].hyst;
	return 0;
}
static int mtktscpu_set_trip_hyst(struct thermal_zone_device *thermal,
						int trip,
						unsigned long hyst)
{
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].hyst = hyst;
	return 0;
}

static struct thermal_zone_device_ops mtktscpu_dev_ops = {
	.bind = mtktscpu_cdev_bind,
	.unbind = mtktscpu_cdev_unbind,
	.get_temp = mtktscpu_get_temp,
	.get_mode = mtktscpu_get_mode,
	.set_mode = mtktscpu_set_mode,
	.get_trip_type = mtktscpu_get_trip_type,
	.get_trip_temp = mtktscpu_get_trip_temp,
	.set_trip_temp = mtktscpu_set_trip_temp,
	.get_crit_temp = mtktscpu_get_crit_temp,
	.get_trip_hyst = mtktscpu_get_trip_hyst,
	.set_trip_hyst = mtktscpu_set_trip_hyst,
};

static void mtktscpu_work(struct work_struct *work)
{
	struct mtktscpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct mtktscpu_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static ssize_t trips_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->trips);
}
static ssize_t trips_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int trips = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	if (sscanf(buf, "%d\n", &trips) != 1)
		return -EINVAL;
	if (trips < 0)
		return -EINVAL;

	pdata->num_trips = trips;
	thermal->trips = pdata->num_trips;
	return count;
}
static ssize_t polling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->polling_delay);
}
static ssize_t polling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int polling_delay = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtktscpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	pdata->polling_delay = polling_delay;
	thermal->polling_delay = pdata->polling_delay;
	thermal_zone_device_update(thermal);
	return count;
}

static DEVICE_ATTR(trips, S_IRUGO | S_IWUSR, trips_show, trips_store);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR, polling_show, polling_store);

static int mtktscpu_create_sysfs(struct mtktscpu_thermal_zone *tzone)
{
	int ret = 0;
	ret = device_create_file(&tzone->tz->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);
	return ret;
}

static int mtktscpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtktscpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = &mtktscpu_thermal_data;

	if (!pdata)
		return -EINVAL;

	ret = mtk_ts_cpu_init(pdev);
	if (ret) {
		pr_err("%s Error mtk_ts_cpu_init\n", __func__);
		return -EINVAL;
	}

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;

	memset(tzone, 0, sizeof(*tzone));
	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register("mtktscpu",
						 pdata->num_trips,
						 MASK,
						 tzone,
						 &mtktscpu_dev_ops,
						 NULL,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register mtktscpu thermal zone device\n", __func__);
		return -EINVAL;
	}

	ret = mtktscpu_create_sysfs(tzone);
	INIT_WORK(&tzone->therm_work, mtktscpu_work);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return 0;
}

static int mtktscpu_remove(struct platform_device *pdev)
{
	struct mtktscpu_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_thermal_of_match[] = {
	{.compatible = "mediatek,THERM_CTRL",},
	{},
};
#endif

static struct platform_driver mtk_thermal_driver = {
	.probe = mtktscpu_probe,
	.remove = mtktscpu_remove,
	.suspend = mtktscpu_suspend,
	.resume = mtktscpu_resume,
	.driver = {
		.name = THERMAL_NAME,
#ifdef CONFIG_OF
		.of_match_table = mt_thermal_of_match,
#endif
	},
};

static int __init mtktscpu_init(void)
{
	return platform_driver_register(&mtk_thermal_driver);
}

static void __exit mtktscpu_exit(void)
{
	platform_driver_unregister(&mtk_thermal_driver);
}


module_init(mtktscpu_init);
module_exit(mtktscpu_exit);
