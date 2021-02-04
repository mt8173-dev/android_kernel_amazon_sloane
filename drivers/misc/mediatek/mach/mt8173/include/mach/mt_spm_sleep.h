#ifndef _MT_SPM_SLEEP_
#define _MT_SPM_SLEEP_

#include <linux/kernel.h>
#include <mach/mt_spm.h>
/*
 * for suspend
 */
extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern u32 spm_get_sleep_wakesrc(void);
extern wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data);

extern void spm_set_wakeup_src_check(void);
extern bool spm_check_wakeup_src(void);
extern void spm_poweron_config_set(void);
extern void spm_md32_sram_con(u32 value);

extern void spm_output_sleep_option(void);
extern void spm_suspend_init(void);

#endif
