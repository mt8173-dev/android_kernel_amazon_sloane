#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H

#define LOGD(fmt, msg...)	pr_notice(fmt, ##msg)
#define LOGV(fmt, msg...)
#define LOGI	LOGD
#define LOGE(fmt, msg...)	pr_err(fmt, ##msg)
#define LOGW	LOGE

int get_memory_size(void);

int in_fiq_handler(void);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom, unsigned long top);

#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show(void);
#endif

#ifdef CONFIG_MTK_AEE_IPANIC
extern void aee_dumpnative(void);
#endif

/* wdt-handler.c */
extern int dump_localtimer_info(char *buffer, int size);
extern int dump_idle_info(char *buffer, int size);
#ifdef CONFIG_SCHED_DEBUG
extern int sysrq_sched_debug_show_at_KE(void);
#endif

#ifdef CONFIG_SMP
extern void dump_log_idle(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
#endif
/* extern void mt_fiq_printf(const char *fmt, ...); */
extern int debug_locks;
#endif				/* AEE_COMMON_H */
