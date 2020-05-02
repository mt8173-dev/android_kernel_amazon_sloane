#ifndef _MT_SPM_MTCMOS_
#define _MT_SPM_MTCMOS_

#include <linux/kernel.h>

#define STA_POWER_DOWN  0
#define STA_POWER_ON    1

/*
 * 1. for CPU MTCMOS: CPU0-7, DBG1, CPUSYS1
 * 2. call spm_mtcmos_cpu_lock/unlock() before/after any operations
 */
extern int spm_mtcmos_cpu_init(void);

extern void spm_mtcmos_cpu_lock(unsigned long *flags);
extern void spm_mtcmos_cpu_unlock(unsigned long *flags);

extern int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn);

extern int spm_mtcmos_ctrl_dbg0(int state);

extern int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn);
extern int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn);

extern bool spm_cpusys0_can_power_down(void);
extern bool spm_cpusys1_can_power_down(void);

extern void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state);


/*
 * 1. for non-CPU MTCMOS: VDEC, VENC, ISP, DISP, MFG, INFRA, DDRPHY,
 * 2. call spm_mtcmos_noncpu_lock/unlock() before/after any operations
 */

extern int spm_mtcmos_ctrl_vdec(int state);
extern int spm_mtcmos_ctrl_venc(int state);
extern int spm_mtcmos_ctrl_isp(int state);
extern int spm_mtcmos_ctrl_disp(int state);
extern int spm_mtcmos_ctrl_mfg(int state);
extern int spm_mtcmos_ctrl_mfg_2D(int state);
extern int spm_mtcmos_ctrl_mfg_ASYNC(int state);
extern int spm_mtcmos_ctrl_ven2(int state);
extern int spm_mtcmos_ctrl_aud(int state);
extern int spm_mtcmos_ctrl_usb(int state);

extern int spm_topaxi_prot(int bit, int en);
extern int spm_topaxi_prot_l2(int bit, int en);

extern int __init mt_spm_mtcmos_init(void);

#endif
