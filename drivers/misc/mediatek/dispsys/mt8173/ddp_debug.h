#ifndef __DDP_DEBUG_H__
#define __DDP_DEBUG_H__

#include <linux/kernel.h>
#include "ddp_mmp.h"
#include "ddp_dump.h"
#include "ddp_log.h"

#define STR_CONVERT(p, val, base, action)\
	do {			\
		int ret = 0;	\
		const char *tmp;	\
		tmp = strsep(p, ","); \
		if (tmp == NULL) \
			break; \
		if (strcmp(#base, "int") == 0)\
			ret = kstrtoint(tmp, 0, (int *)val); \
		else if (strcmp(#base, "uint") == 0)\
			ret = kstrtouint(tmp, 0, (unsigned int *)val); \
		else if (strcmp(#base, "ul") == 0)\
			ret = kstrtoul(tmp, 0, (unsigned long *)val); \
		if (0 != ret) {\
			DDPMSG("[ERROR]kstrtoint/kstrtouint/kstrtoul return error: %d\n" \
				"  file : %s, line : %d\n",		\
				ret, __FILE__, __LINE__);\
			action; \
		} \
	} while (0)

extern unsigned int disp_low_power_reduse_fps;
extern unsigned int disp_low_power_reduse_clock;
extern unsigned int disp_low_power_disable_ddp_clock;
extern unsigned int disp_low_power_adjust_vfp;
extern unsigned int disp_low_power_remove_ovl;
extern unsigned int gSkipIdleDetect;

extern unsigned int gEnableSODIControl;
extern unsigned int gPrefetchControl;
extern unsigned int gDisableSODIForTriggerLoop;
extern unsigned int gESDEnableSODI;
extern unsigned int gEnableMutexRisingEdge;

extern unsigned int gDumpESDCMD;
extern unsigned int gDumpConfigCMD;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
extern unsigned int gDebugSvp;
extern unsigned int gDebugSvpOption;
#endif

void ddp_debug_init(void);
void ddp_debug_exit(void);
unsigned int ddp_debug_analysis_to_buffer(void);
unsigned int ddp_debug_dbg_log_level(void);
unsigned int ddp_debug_irq_log_level(void);
int ddp_mem_test(void);
int ddp_lcd_test(void);

extern int disp_create_session(disp_session_config *config);
extern int disp_destroy_session(disp_session_config *config);

#endif				/* __DDP_DEBUG_H__ */
