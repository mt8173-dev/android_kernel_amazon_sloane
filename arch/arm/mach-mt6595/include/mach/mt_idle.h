#ifndef _MT_IDLE_H
#define _MT_IDLE_H
enum idle_lock_spm_id {
	IDLE_SPM_LOCK_VCORE_DVFS = 0,
};
extern void idle_lock_spm(enum idle_lock_spm_id id);
extern void idle_unlock_spm(enum idle_lock_spm_id id);

extern void enable_dpidle_by_bit(int id);
extern void disable_dpidle_by_bit(int id);
extern void enable_soidle_by_bit(int id);
extern void disable_soidle_by_bit(int id);

#ifdef _MT_IDLE_C

extern unsigned long localtimer_get_counter(void);
extern int localtimer_set_next_event(unsigned long evt);
extern void hp_enable_timer(int enable);

extern int disp_od_is_enabled(void);

extern bool is_in_cpufreq;

extern unsigned int g_SPM_MCDI_Abnormal_WakeUp;

#if defined(EN_PTP_OD) && EN_PTP_OD
extern u32 ptp_data[3];
#endif

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);

extern struct kobject *power_kobj;

#endif				/* _MT_IDLE_C */

#endif
