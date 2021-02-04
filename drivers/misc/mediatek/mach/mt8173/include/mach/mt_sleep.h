#ifndef _MT_SLEEP_
#define _MT_SLEEP_

#include <linux/kernel.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_sleep.h>

#define WAKE_SRC_CFG_KEY            (1U << 31)

extern int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on);

extern wake_reason_t slp_get_wake_reason(void);
extern bool slp_will_infra_pdn(void);
extern void slp_pasr_en(bool en, u32 value);
extern void slp_dpd_en(bool en);

extern void slp_module_init(void);

extern void mt_power_gs_dump_suspend(void);

/* PASR/DPD Preliminary operations */
extern void mtkpasr_phaseone_ops(void);

/* PASR/DPD SW operations */
extern int configure_mrw_pasr(u32 segment_rank0, u32 segment_rank1);
extern int pasr_enter(u32 *sr, u32 *dpd);
extern int pasr_exit(void);
extern unsigned long mtkpasr_enable_sr;
extern void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1);
extern void exit_pasr_dpd_config(void);
#endif
