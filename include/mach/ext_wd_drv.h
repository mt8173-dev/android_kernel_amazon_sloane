#ifndef __EXT_WD_DRV_H
#define __EXT_WD_DRV_H
#include <mach/mt_typedefs.h>
#include "wd_api.h"

/* direct api */
int mtk_wdt_request_mode_set(int mark_bit, WD_REQ_MODE mode);
int mtk_wdt_request_en_set(int mark_bit, WD_REQ_CTL en);
void wdt_arch_reset(unsigned char mode);
void mtk_wdt_restart(enum wd_restart_type type);
void mtk_wdt_mode_config(BOOL dual_mode_en, BOOL irq, BOOL ext_en, BOOL ext_pol, BOOL wdt_en);
void mtk_wdt_set_time_out_value(unsigned int value);
int mtk_wdt_confirm_hwreboot(void);
int mtk_wdt_enable(enum wk_wdt_en en);
void mtk_wd_resume(void);
void mtk_wd_suspend(void);
void wdt_dump_reg(void);
int mtk_wdt_swsysret_config(int bit, int set_value);

/* end */

#endif
