/* rc-dvb0700-big.c - Keytable for devices in dvb0700
 *
 * Copyright (c) 2010 by Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * TODO: This table is a real mess, as it merges RC codes from several
 * devices into a big table. It also has both RC-5 and NEC codes inside.
 * It should be broken into small tables, and the protocols should properly
 * be indentificated.
 *
 * The table were imported from dib0700_devices.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>

#include "mtk_ir_core.h"

#include "mtk_ir_cus_nec.h"


#define MTK_NEC_CONFIG      (IRRX_CH_END_15 + IRRX_CH_IGSYN + IRRX_CH_HWIR)
#define MTK_NEC_SAPERIOD    (0x00F)	/*  */
#define MTK_NEC_THRESHOLD   (0x1)

#define MTK_NEC_EXP_POWE_KEY1   0x00000000
#define MTK_NEC_EXP_POWE_KEY2   0x00000000

/*
for BD     (8/3MHZ) * MTK_NEC_SAPERIOD
for 8127 (1/32KHZ)*MTK_NEC_SAPERIOD
*/


#define MTK_NEC_1ST_PULSE_REPEAT  (3)
#define MTK_NEC_BITCNT_NORMAL    (33)
#define MTK_NEC_BITCNT_REPEAT    (1)
#define MTK_NEC_BIT8_VERIFY      (0xff)

/* set deglitch with the min number. when glitch < (33*6 = 198us,ignore) */




#define NEC_INFO_TO_BITCNT(u4Info)      ((u4Info & IRRX_CH_BITCNT_MASK)    >> IRRX_CH_BITCNT_BITSFT)
#define NEC_INFO_TO_1STPULSE(u4Info)    ((u4Info & IRRX_CH_1ST_PULSE_MASK) >> IRRX_CH_1ST_PULSE_BITSFT)
#define NEC_INFO_TO_2NDPULSE(u4Info)    ((u4Info & IRRX_CH_2ND_PULSE_MASK) >> IRRX_CH_2ND_PULSE_BITSFT)
#define NEC_INFO_TO_3RDPULSE(u4Info)    ((u4Info & IRRX_CH_3RD_PULSE_MASK) >> IRRX_CH_3RD_PULSE_BITSFT)



static int mtk_ir_nec_init_hw(void);
static int mtk_ir_nec_uninit_hw(void);
static u32 mtk_ir_nec_decode(void *preserve);
static void mtk_ir_timer_function(unsigned long v);


static u32 _u4PrevKey = BTN_NONE;	/* pre key */
static u32 _u4Nec_customer_code = MTK_IR_NEC_CUSTOMER_CODE;

static struct timer_list g_ir_timer;	/* used for check pulse cnt register for workaound to solve clear ir stat */
#define TIMER_PERIOD HZ		/* 1s */

u32 detect_hung_timers = 0;
bool timer_log = false;



static struct rc_map_list mtk_nec_map = {
	.map = {
		.scan = mtk_nec_table,	/* here for custom to modify */
		.size = ARRAY_SIZE(mtk_nec_table),
		.rc_type = RC_BIT_NEC,
		.name = RC_MAP_MTK_NEC,
		}
};

static u32 mtk_ir_nec_get_customer_code(void)
{
	return _u4Nec_customer_code;
}

static void mtk_ir_nec_set_customer_code(void *preserve)
{

	if (preserve == NULL) {
		return;
	}
	_u4Nec_customer_code = *((u32 *) preserve);

	IR_LOG_ALWAYS("_u4Nec_customer_code = 0x%x\n", _u4Nec_customer_code);
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void mtk_ir_nec_early_suspend(void *preserve)
{
	IR_LOG_ALWAYS("\n");
}

static void mtk_ir_nec_late_resume(void *preserve)
{
	IR_LOG_ALWAYS("\n");
}
#else

#define mtk_ir_nec_early_suspend NULL
#define mtk_ir_nec_late_resume NULL
#endif


#ifdef CONFIG_PM_SLEEP

static int mtk_ir_nec_suspend(void *preserve)
{
	IR_LOG_ALWAYS("\n");
	return 0;
}

static int mtk_ir_nec_resume(void *preserve)
{
	IR_LOG_ALWAYS("\n");
	return 0;
}

#else

#define mtk_ir_nec_suspend NULL
#define mtk_ir_nec_resume NULL

#endif

static struct mtk_ir_core_platform_data mtk_ir_pdata_nec = {

	.input_name = MTK_INPUT_NEC_DEVICE_NAME,
	.p_map_list = &mtk_nec_map,
	.i4_keypress_timeout = MTK_IR_NEC_KEYPRESS_TIMEOUT,
	.init_hw = mtk_ir_nec_init_hw,
	.uninit_hw = mtk_ir_nec_uninit_hw,
	.ir_hw_decode = mtk_ir_nec_decode,
	.get_customer_code = mtk_ir_nec_get_customer_code,
	.set_customer_code = mtk_ir_nec_set_customer_code,

#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_nec_early_suspend,
	.late_resume = mtk_ir_nec_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_nec_suspend,
	.resume = mtk_ir_nec_resume,
#endif

};


struct mtk_ir_device mtk_ir_dev_nec = {
	.dev_platform = {
			 .name = MTK_IR_DRIVER_NAME,	/* here must be equal to */
			 .id = MTK_IR_ID_NEC,
			 .dev = {
				 .platform_data = &mtk_ir_pdata_nec,
				 .release = release,
				 },
			 },

};

static int mtk_ir_nec_uninit_hw(void)
{

	/* disable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x0);
	mtk_ir_core_clear_hwirq();
	rc_map_unregister(&mtk_nec_map);
	del_timer(&g_ir_timer);

	return 0;
}

static int mtk_ir_nec_init_hw(void)
{
#if MTK_IRRX_AS_MOUSE_INPUT
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;
#endif

	MTK_IR_MODE ir_mode = mtk_ir_core_getmode();

	IR_LOG_ALWAYS("ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_nec_map.map.scan = mtk_nec_factory_table;
		mtk_nec_map.map.size = ARRAY_SIZE(mtk_nec_factory_table);
	} else {
		mtk_nec_map.map.scan = mtk_nec_table;
		mtk_nec_map.map.size = ARRAY_SIZE(mtk_nec_table);

#if MTK_IRRX_AS_MOUSE_INPUT
		p_table = mtk_nec_map.map.scan;
		size = mtk_nec_map.map.size;
		i = 0;
		memset(&(mtk_ir_pdata_nec.mouse_code), 0xff, sizeof(mtk_ir_pdata_nec.mouse_code));

		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT) {
				mtk_ir_pdata_nec.mouse_code.scanleft = p_table[i].scancode;

			} else if (p_table[i].keycode == KEY_RIGHT) {
				mtk_ir_pdata_nec.mouse_code.scanright = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_UP) {

				mtk_ir_pdata_nec.mouse_code.scanup = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_DOWN) {

				mtk_ir_pdata_nec.mouse_code.scandown = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_ENTER) {

				mtk_ir_pdata_nec.mouse_code.scanenter = p_table[i].scancode;
			}

		}

		mtk_ir_pdata_nec.mouse_code.scanswitch = MTK_IR_MOUSE_NEC_SWITCH_CODE;
		mtk_ir_pdata_nec.mousename[0] = '\0';
		strcat(mtk_ir_pdata_nec.mousename, mtk_ir_pdata_nec.input_name);
		strcat(mtk_ir_pdata_nec.mousename, "_Mouse");
#endif

	}

	rc_map_register(&mtk_nec_map);
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x0);
	IR_WRITE(IRRX_CONFIG_HIGH_REG, MTK_NEC_CONFIG);	/* 0xf0021 */
	IR_WRITE(IRRX_CONFIG_LOW_REG, MTK_NEC_SAPERIOD);
	IR_WRITE(IRRX_THRESHOLD_REG, MTK_NEC_THRESHOLD);
#if 0
	mt_ir_reg_remap(0x1100c00c, 0, false);
	mt_ir_reg_remap(0x1100c010, 0, false);
	mt_ir_reg_remap(0x1100c014, 0, false);
	mt_ir_reg_remap(0x1100c0d8, 0, false);
	mt_ir_reg_remap(0x1100c0dc, 0, false);
	mt_ir_reg_remap(0x1100c0e4, 0, false);
	mt_ir_reg_remap(0x1100c0e8, 0, false);
	mt_ir_reg_remap(0x1100c0ec, 0, false);
	mt_ir_reg_remap(0x1100c0f0, 0, false);
#endif


	mtk_ir_core_direct_clear_hwirq();

	if (g_ir_timer.function == NULL)	/* this patch for the workaround of nec protocol */
	{
		init_timer(&g_ir_timer);
		g_ir_timer.function = mtk_ir_timer_function;
		g_ir_timer.expires = jiffies + TIMER_PERIOD;
		add_timer(&g_ir_timer);
	}

	/* enable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x1);
	return 0;

}




u32 mtk_ir_nec_decode(void *preserve)
{
	u32 _au4IrRxData[2];
	u32 _u4Info = IR_READ(IRRX_COUNT_HIGH_REG);
	u32 u4BitCnt = NEC_INFO_TO_BITCNT(_u4Info);
	u32 u4GroupID = 0;
	char *pu1Data = (char *)_au4IrRxData;

#define NEC_REPEAT_MS 200
	static unsigned long last_jiffers;	/* this is only the patch for NEC disturbed repeat key */
	unsigned long current_jiffers = jiffies;

	_au4IrRxData[0] = IR_READ(IRRX_COUNT_MID_REG);	/* NEC 's code data is in this register */
	_au4IrRxData[1] = IR_READ(IRRX_COUNT_LOW_REG);

	if ((0 != _au4IrRxData[0]) || (0 != _au4IrRxData[1]) || _u4Info != 0) {

		IR_LOG_KEY("RxIsr Info:0x%08x data: 0x%08x%08x\n", _u4Info,
			   _au4IrRxData[1], _au4IrRxData[0]);
	} else {
		IR_LOG_KEY("invalid key!!!\n");
		return BTN_INVALID_KEY;
	}


	/* Check repeat key. */
	if ((u4BitCnt == MTK_NEC_BITCNT_REPEAT)) {
		if (((NEC_INFO_TO_1STPULSE(_u4Info) == MTK_NEC_1ST_PULSE_REPEAT) ||
		     (NEC_INFO_TO_1STPULSE(_u4Info) == MTK_NEC_1ST_PULSE_REPEAT - 1) ||
		     (NEC_INFO_TO_1STPULSE(_u4Info) == MTK_NEC_1ST_PULSE_REPEAT + 1)) &&
		    (NEC_INFO_TO_2NDPULSE(_u4Info) == 0) && (NEC_INFO_TO_3RDPULSE(_u4Info) == 0)) {

			/* repeat key ,may be disturbed waveform */
		} else {
			IR_LOG_KEY("invalid repeat key!!!\n");	/* may be disturbed key */
			_u4PrevKey = BTN_NONE;

		}

		if (time_after(current_jiffers, last_jiffers) > 0) {

			if (jiffies_to_msecs(current_jiffers - last_jiffers) > NEC_REPEAT_MS) {
				/*this repeat is not normal key press repeat key ,it is only NEC disturbed repeat key
				   so ignore it
				 */
				IR_LOG_KEY
				    ("### this is the ng repeat key repeat_out=%dms!!!!!!###\n",
				     jiffies_to_msecs(current_jiffers - last_jiffers));
				_u4PrevKey = BTN_NONE;
			}
		}

		goto end;


	}

	/* Check invalid pulse. */
	if (u4BitCnt != MTK_NEC_BITCNT_NORMAL) {
		IR_LOG_KEY("u4BitCnt(%d), should be(%d)!!!\n", u4BitCnt, MTK_NEC_BITCNT_NORMAL);
		_u4PrevKey = BTN_NONE;
		goto end;
	}
	/* Check invalid key. */
	if ((pu1Data[2] + pu1Data[3]) != MTK_NEC_BIT8_VERIFY) {
		IR_LOG_KEY("invalid nec key code!!!\n");
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	u4GroupID = ((pu1Data[1] << 8) + pu1Data[0]);


	/* Check GroupId. */
	if (u4GroupID != _u4Nec_customer_code) {
		IR_LOG_KEY("invalid customer code 0x%x!!!\n", u4GroupID);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	_u4PrevKey = pu1Data[2];

end:

	IR_LOG_KEY(" repeat_out=%dms\n", jiffies_to_msecs(current_jiffers - last_jiffers));
	last_jiffers = current_jiffers;

	return _u4PrevKey;

}



/* this timer function is only for workaround ir's random hung issue */
static void mtk_ir_timer_function(unsigned long v)
{
	if (mtk_ir_check_cnt())	{
		detect_hung_timers++;
		IR_LOG_ALWAYS
		    ("detect_hung_timers = %d, here workaroud to cleat ir stat register\n", detect_hung_timers);
		if (detect_hung_timers > 1000) {
			detect_hung_timers = 0;
		}
	}
	g_ir_timer.expires = jiffies + TIMER_PERIOD;
	add_timer(&g_ir_timer);
}
