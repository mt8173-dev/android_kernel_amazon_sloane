
#ifndef __MTK_IR_CUS_RC6_DEFINE_H__
#define __MTK_IR_CUS_RC6_DEFINE_H__

#include "mtk_ir_cus_define.h"

#ifdef MTK_LK_IRRX_SUPPORT
#include <platform/mtk_ir_lk_core.h>
#else
#include "mtk_ir_core.h"
#endif


#ifdef MTK_LK_IRRX_SUPPORT
/*this table is using in lk,  for lk boot_menu select*/
static struct mtk_ir_lk_msg mtk_rc6_lk_table[] = {
	{0x58, KEY_UP},
	{0x59, KEY_DOWN},
	{0x5c, KEY_ENTER},
	};

#else
/*this table is used in factory mode, for factory_mode menu select*/

static struct rc_map_table mtk_rc6_factory_table[] = {
	{0x58, KEY_VOLUMEUP},
	{0x59, KEY_VOLUMEDOWN},
	{0x5c, KEY_POWER},
	};

/*this table is used in normal mode, for normal_boot */
static struct rc_map_table mtk_rc6_table[] = {
	{0x00, KEY_0},
	{0x01, KEY_1},
	{0x02, KEY_2},
	{0x03, KEY_3},
	{0x04, KEY_4},
	{0x05, KEY_5},
	{0x06, KEY_6},
	{0x07, KEY_7},
	{0x08, KEY_8},
	{0x09, KEY_9},
	{0x5a, KEY_LEFT},
	{0x5b, KEY_RIGHT},
	{0x58, KEY_UP},
	{0x59, KEY_DOWN},
	{0x5c, KEY_ENTER},
	{0xc7, KEY_POWER},
	{0x4c, KEY_HOMEPAGE},
	{0x83, KEY_BACKSPACE},
	{0x92, KEY_BACK},
	#if MTK_IRRX_AS_MOUSE_INPUT
	{0xffff, KEY_HELP},
    /* be carefule this key is used to send to kown whether key was repeated or released,but no response*/
 #endif
};
#define MTK_IR_RC6_CUSTOMER_CODE  0x32 /*here is rc6's customer code*/

#define MTK_IR_MOUSE_RC6_SWITCH_CODE 0x70
#define MTK_IR_RC6_KEYPRESS_TIMEOUT 140

#endif
#endif


