#ifndef __MTK_IR_CUS_NEC_DEFINE_H__
#define __MTK_IR_CUS_NEC_DEFINE_H__

#include "mtk_ir_cus_define.h"

#ifdef MTK_LK_IRRX_SUPPORT

#include <platform/mtk_ir_lk_core.h>
#else
#include "mtk_ir_core.h"
#endif


#ifdef MTK_LK_IRRX_SUPPORT

/* this table is using in lk,  for lk boot_menu select*/
static struct mtk_ir_lk_msg mtk_nec_lk_table[] = {
{0x44, KEY_UP},
	{0x1d, KEY_DOWN},
	{0x5c, KEY_ENTER},
	};

#else

/*this table is used in factory mode, for factory_mode menu select*/
static struct rc_map_table mtk_nec_factory_table[] = {
	{0x44, KEY_VOLUMEUP},
	{0x1d, KEY_VOLUMEDOWN},
	{0x5c, KEY_POWER},
	};

/*this table is used in normal mode, for normal_boot */
static struct rc_map_table mtk_nec_table[] = {
	{0x00, KEY_X},
	{0x01, KEY_BACK},
	{0x05, KEY_0},
	{0x13, KEY_1},
	{0x10, KEY_2},
	{0x11, KEY_3},
	{0x0f, KEY_4},
	{0x0c, KEY_5},
	{0x0d, KEY_6},
	{0x0b, KEY_7},
	{0x08, KEY_8},
	{0x09, KEY_9},
	{0x1c, KEY_LEFT},
	{0x48, KEY_RIGHT},
	{0x44, KEY_UP},
	{0x1d, KEY_DOWN},
	{0x5c, KEY_ENTER},
	{0x06, KEY_V},
	{0x1E, KEY_B},
	{0x0e, KEY_POWER},
	{0x4c, KEY_HOMEPAGE},
	{0x45, KEY_BACKSPACE},
	{0x5D, KEY_FASTFORWARD},
	{0x12, KEY_T},
	{0x50, KEY_Y},
	{0x54, KEY_U},
	{0x16, KEY_I},
	{0x18, KEY_PREVIOUSSONG},
	{0x47, KEY_P},
	{0x4b, KEY_A},
	{0x19, KEY_NEXTSONG},
	{0x1f, KEY_REWIND},
	{0x1b, KEY_PAUSECD},
	{0x51, KEY_PLAYCD},
	{0x03, KEY_J},
	{0x04, KEY_K},
	{0x59, KEY_STOPCD},
	{0x49, KEY_M},
	{0x07, KEY_L},
	{0x02, KEY_W},
	{0x58, KEY_Z},
	{0x14, KEY_N},
#if MTK_IRRX_AS_MOUSE_INPUT
{0xffff, KEY_HELP},   /*be carefule this key is used to send,but no response*/
#endif
};

#define MTK_IR_NEC_CUSTOMER_CODE  0xff00 /*here is nec's customer code*/

#define MTK_IR_MOUSE_NEC_SWITCH_CODE 0x16

#define MTK_IR_NEC_KEYPRESS_TIMEOUT 140

#endif
#endif


