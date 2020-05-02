#ifndef __MT_CPU_DORMANT_H_
#define __MT_CPU_DORMANT_H_

extern void save_cp15(void *pointer);
extern void save_control_registers(void *pointer, int is_secure);
extern void save_mmu(void *pointer);
extern void save_fault_status(void *pointer);
extern void save_generic_timer(void *pointer, int is_hyp);
extern void restore_control_registers(void *pointer, int is_secure);
extern void restore_cp15(void *pointer);
extern void restore_mmu(void *pointer);
extern void restore_fault_status(void *pointer);
extern void restore_generic_timer(void *pointer, int is_hyp);

#endif				/* __MT_CPU_DORMANT_H_ */
