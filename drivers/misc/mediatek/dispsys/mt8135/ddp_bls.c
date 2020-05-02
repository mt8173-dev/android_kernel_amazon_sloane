#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/xlog.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_ddp_bls.h>

#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_debug.h"
#include "disp_drv.h"
#include "ddp_hal.h"

#include <cust_leds.h>
#include <cust_leds_def.h>

/* collect all globals in this structure
 * put all configurables in the platform data */
struct mt_bls_device {
	struct platform_device *pdev;
	struct mutex backlight_mutex;
	#if !defined(CONFIG_MTK_AAL_SUPPORT)
	int mutex_id;
	int power_on;
	#endif
	int pwm_div;
	int max_level;
};

#define POLLING_TIME_OUT 1000

#define PWM_LOW_LIMIT 1		/* PWM output lower bound = 8 */

#define PWM_DEFAULT_DIV_VALUE 0x24

#if defined(ONE_WIRE_PULSE_COUNTING)
#define MAX_PWM_WAVENUM 16
#define PWM_TIME_OUT (1000*100)
static int g_previous_wavenum;
static int g_previous_level;
#endif

static struct mt_bls_device g_bls = {
	.pwm_div = PWM_DEFAULT_DIV_VALUE,
};

static DISPLAY_PWM_T g_pwm_lut;
static DISPLAY_GAMMA_T g_gamma_lut;
static DISPLAY_GAMMA_T g_gamma_index = {
{
	 {
	  0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88,
	  92, 96,
	  100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168,
	  172, 176, 180, 184, 188, 192, 196,
	  200, 204, 208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 260, 264, 268,
	  272, 276, 280, 284, 288, 292, 296,
	  300, 304, 308, 312, 316, 320, 324, 328, 332, 336, 340, 344, 348, 352, 356, 360, 364, 368,
	  372, 376, 380, 384, 388, 392, 396,
	  400, 404, 408, 412, 416, 420, 424, 428, 432, 436, 440, 444, 448, 452, 456, 460, 464, 468,
	  472, 476, 480, 484, 488, 492, 496,
	  500, 504, 508, 512, 516, 520, 524, 528, 532, 536, 540, 544, 548, 552, 556, 560, 564, 568,
	  572, 576, 580, 584, 588, 592, 596,
	  600, 604, 608, 612, 616, 620, 624, 628, 632, 636, 640, 644, 648, 652, 656, 660, 664, 668,
	  672, 676, 680, 684, 688, 692, 696,
	  700, 704, 708, 712, 716, 720, 724, 728, 732, 736, 740, 744, 748, 752, 756, 760, 764, 768,
	  772, 776, 780, 784, 788, 792, 796,
	  800, 804, 808, 812, 816, 820, 824, 828, 832, 836, 840, 844, 848, 852, 856, 860, 864, 868,
	  872, 876, 880, 884, 888, 892, 896,
	  900, 904, 908, 912, 916, 920, 924, 928, 932, 936, 940, 944, 948, 952, 956, 960, 964, 968,
	  972, 976, 980, 984, 988, 992, 996,
	  1000, 1004, 1008, 1012, 1016, 1020, 1023},
	 {
	  0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88,
	  92, 96,
	  100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168,
	  172, 176, 180, 184, 188, 192, 196,
	  200, 204, 208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 260, 264, 268,
	  272, 276, 280, 284, 288, 292, 296,
	  300, 304, 308, 312, 316, 320, 324, 328, 332, 336, 340, 344, 348, 352, 356, 360, 364, 368,
	  372, 376, 380, 384, 388, 392, 396,
	  400, 404, 408, 412, 416, 420, 424, 428, 432, 436, 440, 444, 448, 452, 456, 460, 464, 468,
	  472, 476, 480, 484, 488, 492, 496,
	  500, 504, 508, 512, 516, 520, 524, 528, 532, 536, 540, 544, 548, 552, 556, 560, 564, 568,
	  572, 576, 580, 584, 588, 592, 596,
	  600, 604, 608, 612, 616, 620, 624, 628, 632, 636, 640, 644, 648, 652, 656, 660, 664, 668,
	  672, 676, 680, 684, 688, 692, 696,
	  700, 704, 708, 712, 716, 720, 724, 728, 732, 736, 740, 744, 748, 752, 756, 760, 764, 768,
	  772, 776, 780, 784, 788, 792, 796,
	  800, 804, 808, 812, 816, 820, 824, 828, 832, 836, 840, 844, 848, 852, 856, 860, 864, 868,
	  872, 876, 880, 884, 888, 892, 896,
	  900, 904, 908, 912, 916, 920, 924, 928, 932, 936, 940, 944, 948, 952, 956, 960, 964, 968,
	  972, 976, 980, 984, 988, 992, 996,
	  1000, 1004, 1008, 1012, 1016, 1020, 1023},
	 {
	  0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88,
	  92, 96,
	  100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168,
	  172, 176, 180, 184, 188, 192, 196,
	  200, 204, 208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 260, 264, 268,
	  272, 276, 280, 284, 288, 292, 296,
	  300, 304, 308, 312, 316, 320, 324, 328, 332, 336, 340, 344, 348, 352, 356, 360, 364, 368,
	  372, 376, 380, 384, 388, 392, 396,
	  400, 404, 408, 412, 416, 420, 424, 428, 432, 436, 440, 444, 448, 452, 456, 460, 464, 468,
	  472, 476, 480, 484, 488, 492, 496,
	  500, 504, 508, 512, 516, 520, 524, 528, 532, 536, 540, 544, 548, 552, 556, 560, 564, 568,
	  572, 576, 580, 584, 588, 592, 596,
	  600, 604, 608, 612, 616, 620, 624, 628, 632, 636, 640, 644, 648, 652, 656, 660, 664, 668,
	  672, 676, 680, 684, 688, 692, 696,
	  700, 704, 708, 712, 716, 720, 724, 728, 732, 736, 740, 744, 748, 752, 756, 760, 764, 768,
	  772, 776, 780, 784, 788, 792, 796,
	  800, 804, 808, 812, 816, 820, 824, 828, 832, 836, 840, 844, 848, 852, 856, 860, 864, 868,
	  872, 876, 880, 884, 888, 892, 896,
	  900, 904, 908, 912, 916, 920, 924, 928, 932, 936, 940, 944, 948, 952, 956, 960, 964, 968,
	  972, 976, 980, 984, 988, 992, 996,
	  1000, 1004, 1008, 1012, 1016, 1020, 1023}
	 }
};

DISPLAY_GAMMA_T *get_gamma_index(void)
{
	DISP_DBG("get_gamma_index!\n");
	return &g_gamma_index;
}

DISPLAY_PWM_T *get_pwm_lut(void)
{
	DISP_DBG("get_pwm_lut!\n");
	return &g_pwm_lut;
}


void disp_onConfig_bls(DISP_AAL_PARAM *param)
{
	unsigned long prevSetting = DISP_REG_GET(DISP_REG_BLS_BLS_SETTING);
	unsigned long regVal = 0;

	DISP_DBG("disp_onConfig_bls!\n");

	DISP_DBG("pwm duty = %lu\n", param->pwmDuty);
	if (param->pwmDuty == 0)
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY, 0);
	else if (param->pwmDuty > g_bls.max_level)
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY,
			(PWM_LOW_LIMIT << 19) | g_bls.max_level);
	else
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY,
			(PWM_LOW_LIMIT << 19) | param->pwmDuty);

	DISP_DBG("bls setting = %lu\n", param->setting);
	if (param->setting & ENUM_FUNC_GAMMA)
		regVal |= 0x7;
	else
		regVal &= ~0x7;

	if (param->setting & ENUM_FUNC_BLS)
		regVal |= 0x11D00;
	else
		regVal &= ~0x11D00;
	DISP_REG_SET(DISP_REG_BLS_BLS_SETTING, regVal);

	if (param->setting & ENUM_FUNC_BLS) {
		DISP_DBG("distion threshold = %lu\n", param->maxClrDistThd);
		DISP_DBG("predistion threshold = %lu\n", param->preDistThd);
		DISP_DBG("scene change threshold = %lu\n", param->scDiffThd);
		DISP_DBG("scene change bin = %lu\n", param->scBinThd);

		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0024, 0x00000000);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0028, 0x00010001);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0040, param->maxClrLimit);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0044, param->preDistThd);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0048, param->maxClrDistThd);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0068, param->scDiffThd);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x006C, param->scBinThd);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0070, 0x01A201A2);	/* F_3db = 1/60 Hz */
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0074, 0x00003CBC);
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x0078, 0x01500150);	/* F_3db = 1/75 Hz */
		DISP_REG_SET(DISPSYS_BLS_BASE + 0x007C, 0x00003D60);
	}

	if (prevSetting & 0x11D00) {
		unsigned char autoMaxClr, autoMaxClrFlt, autoDp, autoDpFlt;
		regVal = DISP_REG_GET(DISPSYS_BLS_BASE + 0x0204);
		autoMaxClr = regVal & 0xFF;
		autoMaxClrFlt = (regVal >> 8) & 0xFF;
		autoDp = (regVal >> 16) & 0xFF;
		autoDpFlt = (regVal >> 24) & 0xFF;

		DISP_DBG("MaxClr=%u, MaxClrFlt=%u, Dp=%u, DpFlt=%u\n", autoMaxClr, autoMaxClrFlt,
			 autoDp, autoDpFlt);

		if (autoMaxClr != autoMaxClrFlt || autoDp != autoDpFlt)
			disp_set_aal_alarm(1);
		else
			disp_set_aal_alarm(0);

	} else if (param->setting & ENUM_FUNC_BLS) {
		disp_set_aal_alarm(1);
	}

	if (aal_debug_flag == 0)
		DISP_REG_SET(DISP_REG_BLS_EN, 0x80010001);
	else
		DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);
}


static unsigned int brightness_mapping(unsigned int level)
{
	unsigned int mapped_level;

#if defined(ONE_WIRE_PULSE_COUNTING)
	mapped_level = (level + (MAX_PWM_WAVENUM / 2)) / MAX_PWM_WAVENUM;

	if (level != 0 && mapped_level == 0)
		mapped_level = 1;

	if (mapped_level > MAX_PWM_WAVENUM)
		mapped_level = MAX_PWM_WAVENUM;
#else
	mapped_level = level;

	if (mapped_level > g_bls.max_level)
		mapped_level = g_bls.max_level;
#endif
	return mapped_level;
}

#if !defined(CONFIG_MTK_AAL_SUPPORT)
static int disp_poll_for_reg(unsigned int addr, unsigned int value, unsigned int mask,
			     unsigned int timeout)
{
	unsigned int cnt = 0;

	while ((DISP_REG_GET(addr) & mask) != value) {
		usleep_range(10 , 1000);  /* msleep(1); */
		cnt++;
		if (cnt > timeout)
			return -1;
	}

	return 0;
}

static int disp_bls_get_mutex(void)
{
	if (g_bls.mutex_id < 0)
		return -1;

	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(g_bls.mutex_id), 1);
	if (disp_poll_for_reg(DISP_REG_CONFIG_MUTEX(g_bls.mutex_id),
		0x2, 0x2, POLLING_TIME_OUT)) {
		DISP_ERR("get mutex timeout!\n");
		disp_dump_reg(DISP_MODULE_CONFIG);
		return -1;
	}
	return 0;
}

static int disp_bls_release_mutex(void)
{
	if (g_bls.mutex_id < 0)
		return -1;

	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(g_bls.mutex_id), 0);
	if (disp_poll_for_reg(DISP_REG_CONFIG_MUTEX(g_bls.mutex_id),
		0, 0x2, POLLING_TIME_OUT)) {
		DISP_ERR("release mutex timeout!\n");
		disp_dump_reg(DISP_MODULE_CONFIG);
		return -1;
	}
	return 0;
}
#endif

void disp_bls_update_gamma_lut(void)
{
	int index, i;
	unsigned long regValue;
	unsigned long CurVal, Count;

#ifndef NEW_GAMMA_ARRAY_ARRANGEMENT
	/* make it build fail for EPC rule after MP */
	DISP_MSG("disp_bls_update_gamma_lut!\n");
#endif
	DISP_DBG("disp_bls_update_gamma_lut!\n");

	/* init gamma table */
	for (index = 0; index < 3; index++) {
		for (Count = 0; Count < 257; Count++)
			g_gamma_lut.entry[index][Count] = g_gamma_index.entry[index][Count];

	}

	/* program SRAM */
	regValue = DISP_REG_GET(DISP_REG_BLS_EN);
	if (regValue & 0x1) {
		DISP_ERR("update GAMMA LUT while BLS func enabled!\n");
		disp_dump_reg(DISP_MODULE_BLS);
	}
	/* DISP_REG_SET(DISP_REG_BLS_EN, (regValue & 0x80000000)); */
	DISP_REG_SET(DISP_REG_BLS_LUT_UPDATE, 0x1);

	for (i = 0; i < 256; i++) {
		CurVal =
		    (((g_gamma_lut.entry[0][i] & 0x3FF) << 20) | ((g_gamma_lut.
								   entry[1][i] & 0x3FF) << 10) |
		     (g_gamma_lut.entry[2][i] & 0x3FF));
		DISP_REG_SET(DISP_REG_BLS_GAMMA_LUT(i), CurVal);
		DISP_DBG("[%d] GAMMA LUT = 0x%x, (%lu, %lu, %lu)\n", i,
			 DISP_REG_GET(DISP_REG_BLS_GAMMA_LUT(i)), g_gamma_lut.entry[0][i],
			 g_gamma_lut.entry[1][i], g_gamma_lut.entry[2][i]);
	}

	/* Set Gamma Last point */
	DISP_REG_SET(DISP_REG_BLS_GAMMA_SETTING, 0x00000001);

	/* set gamma last index */
	CurVal =
	    (((g_gamma_lut.entry[0][256] & 0x3FF) << 20) | ((g_gamma_lut.
							     entry[1][256] & 0x3FF) << 10) |
	     (g_gamma_lut.entry[2][256] & 0x3FF));
	DISP_REG_SET(DISP_REG_BLS_GAMMA_BOUNDARY, CurVal);

	DISP_REG_SET(DISP_REG_BLS_LUT_UPDATE, 0);
	/* DISP_REG_SET(DISP_REG_BLS_EN, regValue); */
}

void disp_bls_update_pwm_lut(void)
{
	int i, j;
	unsigned int regValue;

	DISP_DBG("disp_bls_update_pwm_lut!\n");

	/* program SRAM */
	regValue = DISP_REG_GET(DISP_REG_BLS_EN);
	if (regValue & 0x1) {
		DISP_ERR("update PWM LUT while BLS func enabled!\n");
		disp_dump_reg(DISP_MODULE_BLS);
	}
	/* DISP_REG_SET(DISP_REG_BLS_EN, (regValue & 0x80000000)); */
	DISP_REG_SET(DISP_REG_BLS_LUT_UPDATE, 0x4);

	for (i = 0; i < PWM_LUT_ENTRY; i++) {
		/* select LUT row index */
		DISP_REG_SET(DISP_REG_BLS_PWM_LUT_SEL, i);
		for (j = 0; j < PWM_LUT_ENTRY; j++) {
			DISP_REG_SET(DISP_REG_BLS_PWM_LUT(j), g_pwm_lut.entry[i][j]);
			DISP_DBG("[%d][%d] PWM LUT = 0x%x (%lu)\n", i, j,
				 DISP_REG_GET(DISP_REG_BLS_PWM_LUT(j)), g_pwm_lut.entry[i][j]);
		}
	}

	DISP_REG_SET(DISP_REG_BLS_LUT_UPDATE, 0);
	/* DISP_REG_SET(DISP_REG_BLS_EN, regValue); */
}

void disp_bls_init(unsigned int srcWidth, unsigned int srcHeight)
{
	UINT32 dither_bpp = DISP_GetOutputBPPforDithering();

	DISP_DBG("disp_bls_init : srcWidth = %d, srcHeight = %d\n", srcWidth, srcHeight);

	DISP_REG_SET(DISP_REG_BLS_SRC_SIZE, (srcHeight << 16) | srcWidth);
	DISP_REG_SET(DISP_REG_BLS_PWM_DUTY, DISP_REG_GET(DISP_REG_BLS_PWM_DUTY));
	DISP_REG_SET(DISP_REG_BLS_PWM_CON, 0x00050000 | g_bls.pwm_div);
	DISP_REG_SET(DISP_REG_BLS_PWM_DUTY_GAIN, 0x00000100);
	DISP_REG_SET(DISP_REG_BLS_BLS_SETTING, 0x0);
	DISP_REG_SET(DISP_REG_BLS_INTEN, 0xF);

	disp_bls_update_gamma_lut();
	disp_bls_update_pwm_lut();

	/* Dithering */
	DISP_REG_SET(DISP_REG_BLS_DITHER(6), 0x00003004);
	DISP_REG_SET(DISP_REG_BLS_DITHER(12), 0x00000011);
	DISP_REG_SET(DISP_REG_BLS_DITHER(13), 0x00000222);
	DISP_REG_SET(DISP_REG_BLS_DITHER(14), 0x00000000);
	DISP_REG_SET(DISP_REG_BLS_DITHER(17), 0x00000000);
	if (dither_bpp == 16) {/* 565 */
		DISP_REG_SET(DISP_REG_BLS_DITHER(15), 0x52520001);
		DISP_REG_SET(DISP_REG_BLS_DITHER(16), 0x52524242);
		DISP_REG_SET(DISP_REG_BLS_DITHER(0), 0x00000001);
	} else if (dither_bpp == 18) {/* 666 */
		DISP_REG_SET(DISP_REG_BLS_DITHER(15), 0x42420001);
		DISP_REG_SET(DISP_REG_BLS_DITHER(16), 0x42424242);
		DISP_REG_SET(DISP_REG_BLS_DITHER(0), 0x00000001);
	} else if (dither_bpp == 24) {/* 888 */
		DISP_REG_SET(DISP_REG_BLS_DITHER(15), 0x22220001);
		DISP_REG_SET(DISP_REG_BLS_DITHER(16), 0x22222222);
		DISP_REG_SET(DISP_REG_BLS_DITHER(0), 0x00000001);
	} else {
		DISP_MSG("error diter bpp = %d\n", dither_bpp);
		DISP_REG_SET(DISP_REG_BLS_DITHER(0), 0x00000000);
	}

	DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);	/* only enable PWM */
	DISP_REG_SET(DISPSYS_BLS_BASE + 0x000C, 0x00000003);	/* w/o inverse gamma */

	if (dbg_log)
		disp_dump_reg(DISP_MODULE_BLS);
}

int disp_bls_config(void)
{
#if !defined(CONFIG_MTK_AAL_SUPPORT)
	struct cust_mt65xx_led *cust_led_list = get_cust_led_list();
	struct cust_mt65xx_led *cust = NULL;
	struct PWM_config *config_data = NULL;

	if (cust_led_list) {
		cust = &cust_led_list[MT65XX_LED_TYPE_LCD];
		if ((strcmp(cust->name, "lcd-backlight") == 0)
		    && (cust->mode == MT65XX_LED_MODE_CUST_BLS_PWM)) {
			config_data = &cust->config_data;
			g_bls.pwm_div =
			    (config_data->div == 0) ? PWM_DEFAULT_DIV_VALUE : config_data->div;
			DISP_MSG("disp_bls_config : PWM config data (%d,%d)\n",
				 config_data->clock_source, config_data->div);
		}
	}

	if (!clock_is_on(MT_CG_DISP_BLS_DISP) || !g_bls.power_on) {
		DISP_MSG("disp_bls_config: enable clock\n");
		enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		enable_clock(MT_CG_DISP_BLS_DISP, "DDP");
		g_bls.power_on = 1;
	}

	DISP_MSG("disp_bls_config : g_bls.mutex_id = %d\n", g_bls.mutex_id);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(g_bls.mutex_id), 1);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(g_bls.mutex_id), 0);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(g_bls.mutex_id),
		0x200);	/* BLS */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(g_bls.mutex_id),
		0);	/* single mode */

	if (disp_bls_get_mutex() == 0) {
#if defined(ONE_WIRE_PULSE_COUNTING)
		g_previous_level = (DISP_REG_GET(DISP_REG_BLS_PWM_CON) & 0x80 > 7) * 0xFF;
		g_previous_wavenum = 0;
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY, 0x00000080);
		DISP_REG_SET(DISP_REG_BLS_PWM_CON,
			     (0x00050000 | g_bls.pwm_div) |
			     (DISP_REG_GET(DISP_REG_BLS_PWM_CON) & 0x80));
		DISP_REG_SET(DISP_REG_BLS_EN, 0x00000000);
#else
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY, DISP_REG_GET(DISP_REG_BLS_PWM_DUTY));
		DISP_REG_SET(DISP_REG_BLS_PWM_CON, 0x00050000 | g_bls.pwm_div);
		DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);
#endif
		DISP_REG_SET(DISP_REG_BLS_PWM_DUTY_GAIN, 0x00000100);

		if (disp_bls_release_mutex() == 0)
			return 0;
	}
	return -1;
#endif
	return 0;
}

int disp_bls_set_max_backlight_(unsigned int level)
{
	mutex_lock(&g_bls.backlight_mutex);
	DISP_MSG("disp_bls_set_max_backlight: level=%d, current level=%d\n",
		level, g_bls.max_level);
	g_bls.max_level = level;
	mutex_unlock(&g_bls.backlight_mutex);
	return 0;
}

#if !defined(CONFIG_MTK_AAL_SUPPORT)
#if defined(ONE_WIRE_PULSE_COUNTING)
int disp_bls_set_backlight(unsigned int level)
{
	int ret = 0;
	unsigned int wavenum = 0;
	unsigned int required_wavenum = 0;

#if defined(CPT_CLAA101FP01_DSI_VDO)
	if (level != 0)
		level = 52;
#endif

	if (!level && !clock_is_on(MT_CG_DISP_BLS_DISP))
		return 0;

	mutex_lock(&g_bls.backlight_mutex);
	disp_bls_config();

	wavenum = 0;
	if (level > 0)
		wavenum = MAX_PWM_WAVENUM - brightness_mapping(level);

	DISP_MSG("disp_bls_set_backlight: level = %d (%d), previous level = %d (%d), PWM div %d\n",
		 level, wavenum, g_previous_level,
		 g_previous_wavenum, g_bls.pwm_div);


	if (level && (!clock_is_on(MT_CG_DISP_BLS_DISP) || !g_bls.power_on))
		disp_bls_config();

	/* [Case 1] y => 0 */
	/* disable PWM, idle value set to low */
	/* [Case 2] 0 => max */
	/* disable PWM, idle value set to high */
	/* [Case 3] 0 => x or y => x */
	/* idle value keep high */
	/* disable PWM to reset wavenum */
	/* re-enable PWM, set wavenum */

	if (g_previous_level != level) {
		DISP_REG_SET(DISP_REG_PWM_WAVE_NUM, 0x0);
		disp_bls_get_mutex();
		if (level == 0)
			DISP_REG_SET(DISP_REG_BLS_PWM_CON,
				     DISP_REG_GET(DISP_REG_BLS_PWM_CON) & ~0x80);
		if (g_previous_level == 0)
			DISP_REG_SET(DISP_REG_BLS_PWM_CON,
				     DISP_REG_GET(DISP_REG_BLS_PWM_CON) | 0x80);

		DISP_REG_SET(DISP_REG_BLS_EN, 0x0);
		disp_bls_release_mutex();

		/* poll for PWM_SEND_WAVENUM to be clear */
		if (disp_poll_for_reg(DISP_REG_PWM_SEND_WAVENUM, 0, 0xFFFFFFFF, POLLING_TIME_OUT)) {
			DISP_MSG("fail to clear wavenum! PWM_SEND_WAVENUM = %d\n",
				 DISP_REG_GET(DISP_REG_PWM_SEND_WAVENUM));
			ret = -1;
			goto Exit;
		}
		/* y => x or 0 => x */
		/* y > x: change level from high to low, */
		/* x > y: change level from low to high, rounding to max */
		if (g_previous_wavenum > wavenum)
			required_wavenum = (MAX_PWM_WAVENUM - g_previous_wavenum) + wavenum;
		else
			required_wavenum = wavenum - g_previous_wavenum;

		if (required_wavenum != 0) {
			disp_bls_get_mutex();

			/* re-enable PWM */
			DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);
			disp_bls_release_mutex();
			DISP_REG_SET(DISP_REG_PWM_WAVE_NUM, required_wavenum);


			/* poll for wave num to be generated completely */
			if (disp_poll_for_reg
			    (DISP_REG_PWM_SEND_WAVENUM, required_wavenum, 0xFFFFFFFF,
			     POLLING_TIME_OUT)) {
				DISP_ERR("fail to set wavenum! PWM_SEND_WAVENUM = %d\n",
					 DISP_REG_GET(DISP_REG_PWM_SEND_WAVENUM));
				g_previous_wavenum = DISP_REG_GET(DISP_REG_PWM_SEND_WAVENUM);
				ret = -1;
				goto Exit;
			}

			DISP_MSG("send wavenum = %d\n", required_wavenum);
		}

		g_previous_level = level;
		g_previous_wavenum = wavenum;
	}

	if (!level && (clock_is_on(MT_CG_DISP_BLS_DISP) && g_bls.power_on)) {
		DISP_MSG("disp_bls_set_backlight: disable clock\n");
		disable_clock(MT_CG_DISP_BLS_DISP, "DDP");
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		g_bls.power_on = 0;
	}

 Exit:
	mutex_unlock(&g_bls.backlight_mutex);
	return ret;
}
#else
int disp_bls_set_backlight(unsigned int level)
{
	DISP_MSG("disp_bls_set_backlight=%d, power_on=%d, pwm_div=%d\n",
		level, g_bls.power_on, g_bls.pwm_div);

#if defined(CPT_CLAA101FP01_DSI_VDO)
	if (level != 0)
		level = 52;
#endif

	if (!level && !clock_is_on(MT_CG_DISP_BLS_DISP))
		return 0;

	mutex_lock(&g_bls.backlight_mutex);

	if (level && (!clock_is_on(MT_CG_DISP_BLS_DISP) || !g_bls.power_on))
		disp_bls_config();


	disp_bls_get_mutex();
	DISP_REG_SET(DISP_REG_BLS_PWM_DUTY, brightness_mapping(level));
	disp_bls_release_mutex();

	if (!level && (clock_is_on(MT_CG_DISP_BLS_DISP) && g_bls.power_on)) {
		DISP_MSG("disp_bls_set_backlight: disable clock\n");
		disable_clock(MT_CG_DISP_BLS_DISP, "DDP");
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		g_bls.power_on = 0;
	}

	mutex_unlock(&g_bls.backlight_mutex);
	return 0;
}
#endif
#else
int disp_bls_set_backlight(unsigned int level)
{
	DISP_AAL_PARAM *param;
	DISP_MSG("disp_bls_set_backlight=%d, pwm_div=%d\n",
		level, g_bls.pwm_div);

#if defined(CPT_CLAA101FP01_DSI_VDO)
	if (level != 0)
		level = 52;
#endif

	mutex_lock(&g_bls.backlight_mutex);
	disp_aal_lock();
	param = get_aal_config();
	param->pwmDuty = brightness_mapping(level);
	disp_aal_unlock();
	mutex_unlock(&g_bls.backlight_mutex);
	return 0;
}
#endif
EXPORT_SYMBOL(disp_bls_set_backlight);

static int ddp_bls_probe(struct platform_device *pdev)
{
	struct mt_bls_data *pdata = pdev->dev.platform_data;
	g_bls.pdev = pdev;
#if !defined(CONFIG_MTK_AAL_SUPPORT)
	g_bls.mutex_id = 3;
#endif
	g_bls.pwm_div = pdata->pwm_config.div ?
		pdata->pwm_config.div : PWM_DEFAULT_DIV_VALUE;
	g_bls.max_level = 255;
	mutex_init(&g_bls.backlight_mutex);
	return 0;
}

static struct platform_driver ddp_bls_driver = {
	.probe = ddp_bls_probe,
	.driver = {
		.name = "mt-ddp-bls",
		.owner = THIS_MODULE,
	},
};

static int __init ddp_bls_init(void)
{
	return platform_driver_register(&ddp_bls_driver);
}
postcore_initcall(ddp_bls_init);
