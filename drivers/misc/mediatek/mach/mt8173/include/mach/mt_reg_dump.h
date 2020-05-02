#ifndef __MT_REG_DUMP_H
#define __MT_REG_DUMP_H

#define CORE_PC(i) (mcu_reg_base + 0x410 + (i << 5))
#define CORE_FP(i) (mcu_reg_base + 0x420 + (i << 5))
#define CORE_SP(i) (mcu_reg_base + 0x428 + (i << 5))

#define MON_SEL (mcu_reg_base + 0x21C)
#define MON_OUT (mcu_reg_base + 0x258)

struct mt_reg_dump {
	unsigned int pc;
	unsigned int fp;
	unsigned int sp;
	unsigned int core_id;
};
/* undef to avoid build error in HE platform mt_reg_dump.h:15:31: error: 'dbg_reg_dump_driver' defined but not used [-Werror=unused-variable] */
/* static struct platform_driver dbg_reg_dump_driver; */

#endif
