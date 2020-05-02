#ifndef __MT6575_SYSTEM_H__
#define __MT6575_SYSTEM_H__

extern void arch_idle(void);
extern void arch_reset(char mode, const char *cmd);
extern void mt_power_off(void);

#endif				/* !__MT6575_SYSTEM_H__ */
