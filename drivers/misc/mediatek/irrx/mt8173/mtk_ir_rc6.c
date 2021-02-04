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
#include <linux/interrupt.h>
#include <linux/kernel.h>

#include "mtk_ir_common.h"

#include "mtk_ir_core.h"
#include "mtk_ir_cus_rc6.h"	/* include customer's key map */
#include "mtk_ir_regs.h"	/* include ir registers */




#define MTK_RC6_CONFIG   (IRRX_CH_END_15 | IRRX_CH_IGSYN | IRRX_CH_HWIR  | IRRX_CH_ORDINV | IRRX_CH_RC5)
#define MTK_RC6_SAPERIOD    (0xe)	/*  */
#define MTK_RC6_THRESHOLD   (0x1)


#define MTK_RC6_BITCNT   0x1e
#define MTK_RC6_LEADER   0x8
#define MTK_RC6_TOGGLE0  0x1
#define MTK_RC6_TOGGLE1  0x2
#define MTK_RC6_CUSTOM   0x32


#define RC6_INFO_TO_BITCNT(u4Info)      ((u4Info & IRRX_CH_BITCNT_MASK)    >> IRRX_CH_BITCNT_BITSFT)

#define MTK_RC6_GET_LEADER(bdata0) ((bdata0>>4))
#define MTK_RC6_GET_TOGGLE(bdata0) ((bdata0 & 0xc)>>2)
#define MTK_RC6_GET_CUSTOM(bdata0, bdata1) (((bdata0 & 0x3) << 6) | bdata1 >> 2)
#define MTK_RC6_GET_KEYCODE(bdata1, bdata2)  \
		(((bdata2>>2) | ((bdata1 & 0x3)<<6)) & 0xff)


static int mtk_ir_rc6_init_hw(void);
static int mtk_ir_rc6_uninit_hw(void);
static u32 mtk_ir_rc6_decode(void *preserve);



static u32 _u4Rc6_customer_code = MTK_IR_RC6_CUSTOMER_CODE;


static struct rc_map_list mtk_rc6_map = {
	.map = {
		.scan = mtk_rc6_table,
		.size = ARRAY_SIZE(mtk_rc6_table),
		.rc_type = RC_BIT_RC6_0,
		.name = RC_MAP_MTK_RC6,
		}
};

static u32 mtk_ir_rc6_get_customer_code(void)
{
	return _u4Rc6_customer_code;
}

static void mtk_ir_rc6_set_customer_code(void *preserve)
{

	if (preserve == NULL) {
		return;
	}
	_u4Rc6_customer_code = *((u32 *) preserve);

	IR_LOG_ALWAYS("_u4Rc6_customer_code = 0x%x\n", _u4Rc6_customer_code);
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_rc6_early_suspend(void *preserve)
{
	IR_LOG_ALWAYS("\n");
}

static void mtk_ir_rc6_late_resume(void *preserve)
{
	IR_LOG_ALWAYS("\n");
}
#else
#define mtk_ir_rc6_early_suspend NULL
#define mtk_ir_rc6_late_resume NULL

#endif


#ifdef CONFIG_PM_SLEEP

static int mtk_ir_rc6_suspend(void *preserve)
{
	IR_LOG_ALWAYS("\n");
	return 0;
}

static int mtk_ir_rc6_resume(void *preserve)
{
	IR_LOG_ALWAYS("\n");
	return 0;
}

#else

#define mtk_ir_rc6_suspend NULL
#define mtk_ir_rc6_resume NULL

#endif

static struct mtk_ir_core_platform_data mtk_ir_pdata_rc6 = {

	.input_name = MTK_INPUT_RC6_DEVICE_NAME,
	.p_map_list = &mtk_rc6_map,
	.i4_keypress_timeout = MTK_IR_RC6_KEYPRESS_TIMEOUT,
	.init_hw = mtk_ir_rc6_init_hw,
	.uninit_hw = mtk_ir_rc6_uninit_hw,
	.ir_hw_decode = mtk_ir_rc6_decode,
	.get_customer_code = mtk_ir_rc6_get_customer_code,
	.set_customer_code = mtk_ir_rc6_set_customer_code,

#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_rc6_early_suspend,
	.late_resume = mtk_ir_rc6_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_rc6_suspend,
	.resume = mtk_ir_rc6_resume,
#endif

};



struct mtk_ir_device mtk_ir_dev_rc6 = {
	.dev_platform = {
			 .name = MTK_IR_DRIVER_NAME,	/* here must be equal to */
			 .id = MTK_IR_ID_RC6,
			 .dev = {
				 .platform_data = &mtk_ir_pdata_rc6,
				 .release = release,
				 },
			 },

};

static u32 mtk_ir_rc6_decode(void *preserve)
{

	u32 _au4IrRxData[2];
	u32 _u4Info = IR_READ(IRRX_COUNT_HIGH_REG);
	char *pu1Data = NULL;
	u16 u4BitCnt = 0;
	u16 u2RC6Leader = 0;
	u16 u2RC6Custom = 0;
	u16 u2RC6Toggle = 0;
	u32 u4RC6key = 0;

	_au4IrRxData[0] = IR_READ(IRRX_COUNT_MID_REG);	/* NEC 's code data is in this register */
	_au4IrRxData[1] = IR_READ(IRRX_COUNT_LOW_REG);

	pu1Data = (char *)_au4IrRxData;
	u4BitCnt = RC6_INFO_TO_BITCNT(_u4Info);

	u2RC6Leader = MTK_RC6_GET_LEADER(pu1Data[0]);
	u2RC6Custom = MTK_RC6_GET_CUSTOM(pu1Data[0], pu1Data[1]);
	u2RC6Toggle = MTK_RC6_GET_TOGGLE(pu1Data[0]);
	u4RC6key = MTK_RC6_GET_KEYCODE(pu1Data[1], pu1Data[2]);



	IR_LOG_KEY("RxIsr Info:0x%08x data: 0x%08x%08x\n", _u4Info,
		   _au4IrRxData[1], _au4IrRxData[0]);

	IR_LOG_KEY
	    ("Bitcnt: 0x%02x, Leader: 0x%02x, Custom: 0x%02x, Toggle: 0x%02x, Keycode; 0x%02x\n",
	     u4BitCnt, u2RC6Leader, u2RC6Custom, u2RC6Toggle, u4RC6key);



	/* Check data. */
	if ((u4BitCnt != MTK_RC6_BITCNT)
	    || (pu1Data == NULL)
	    || (u2RC6Leader != MTK_RC6_LEADER)) {

		return BTN_INVALID_KEY;
	}

	if ((u32) u2RC6Custom != _u4Rc6_customer_code) {
		IR_LOG_KEY("invalid customer code 0x%x!!!", u2RC6Custom);
		return BTN_INVALID_KEY;
	}


	return u4RC6key;


}

static int mtk_ir_rc6_uninit_hw(void)
{


	/* disable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x0);
	mtk_ir_core_clear_hwirq();

	rc_map_unregister(&mtk_rc6_map);

	return 0;
}

static int mtk_ir_rc6_init_hw(void)
{

#if MTK_IRRX_AS_MOUSE_INPUT
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;
#endif

	MTK_IR_MODE ir_mode = mtk_ir_core_getmode();

	IR_LOG_ALWAYS("ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_rc6_map.map.scan = mtk_rc6_factory_table;
		mtk_rc6_map.map.size = ARRAY_SIZE(mtk_rc6_factory_table);
	} else {
		mtk_rc6_map.map.scan = mtk_rc6_table;
		mtk_rc6_map.map.size = ARRAY_SIZE(mtk_rc6_table);

#if MTK_IRRX_AS_MOUSE_INPUT
		p_table = mtk_rc6_map.map.scan;
		size = mtk_rc6_map.map.size;
		i = 0;
		memset(&(mtk_ir_pdata_rc6.mouse_code), 0xff, sizeof(mtk_ir_pdata_rc6.mouse_code));

		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT) {
				mtk_ir_pdata_rc6.mouse_code.scanleft = p_table[i].scancode;

			} else if (p_table[i].keycode == KEY_RIGHT) {
				mtk_ir_pdata_rc6.mouse_code.scanright = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_UP) {

				mtk_ir_pdata_rc6.mouse_code.scanup = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_DOWN) {

				mtk_ir_pdata_rc6.mouse_code.scandown = p_table[i].scancode;
			} else if (p_table[i].keycode == KEY_ENTER) {

				mtk_ir_pdata_rc6.mouse_code.scanenter = p_table[i].scancode;
			}

		}

		mtk_ir_pdata_rc6.mouse_code.scanswitch = MTK_IR_MOUSE_RC6_SWITCH_CODE;
		mtk_ir_pdata_rc6.mousename[0] = '\0';
		strcat(mtk_ir_pdata_rc6.mousename, mtk_ir_pdata_rc6.input_name);
		strcat(mtk_ir_pdata_rc6.mousename, "_Mouse");
#endif

	}

	rc_map_register(&mtk_rc6_map);


	/* disable interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x0);

	IR_WRITE(IRRX_CONFIG_HIGH_REG, MTK_RC6_CONFIG);	/* 0xf0021 */
	IR_WRITE(IRRX_CONFIG_LOW_REG, MTK_RC6_SAPERIOD);
	IR_WRITE(IRRX_THRESHOLD_REG, MTK_RC6_THRESHOLD);

	mtk_ir_core_direct_clear_hwirq();
	/* enable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x1);

	return 0;
}
