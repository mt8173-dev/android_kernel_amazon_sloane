
#ifndef __MTK_IR_CUS_RC5_DEFINE_H__
#define __MTK_IR_CUS_RC5_DEFINE_H__

#include "mtk_ir_cus_define.h"

#ifdef MTK_LK_IRRX_SUPPORT
#include <platform/mtk_ir_lk_core.h>
#else
#include "mtk_ir_core.h"
#endif

#ifdef MTK_LK_IRRX_SUPPORT
/* this table is using in lk,	for lk boot_menu select*/
static struct mtk_ir_lk_msg mtk_rc5_lk_table[] = {
	{0x06, KEY_UP},
	{0x07, KEY_DOWN},
	{0x5c, KEY_ENTER},
	};
#else
/*this table is used in factory mode, for factory_mode menu select*/

static struct rc_map_table mtk_rc5_factory_table[] = {
	{0x06, KEY_VOLUMEDOWN},
	{0x07, KEY_VOLUMEUP},
	{0x5c, KEY_POWER},
	};

/*this table is used in normal mode, for normal_boot */

static struct rc_map_table mtk_rc5_table[] = {
	{0x00, KEY_X},
	{0x01, KEY_ESC},
	{0x02, KEY_W},
	{0x03, KEY_LEFT},
	{0x04, KEY_SELECT},
	{0x05, KEY_RIGHT},
	{0x06, KEY_VOLUMEDOWN},
	{0x07, KEY_VOLUMEUP},
	{0x0e, KEY_POWER},
	{0x4c, KEY_HOMEPAGE},
	{0x5c, KEY_ENTER},
	#if MTK_IRRX_AS_MOUSE_INPUT
	{0xffff, KEY_HELP},
    /* be carefule this key is used to send,but no response*/
	#endif
};
#define MTK_IR_RC5_CUSTOMER_CODE  0x00 /*here is rc5's customer code*/

#define MTK_IR_MOUSE_RC5_SWITCH_CODE 0x00

#define MTK_IR_RC5_KEYPRESS_TIMEOUT 140


#endif

#endif


