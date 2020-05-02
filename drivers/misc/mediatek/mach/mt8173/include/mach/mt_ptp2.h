#ifndef __MT_PTP2_H__
#define __MT_PTP2_H__



#include <linux/kernel.h>
#include <mach/sync_write.h>
/* #include "mach/mt_reg_base.h" */
#include "mach/mt_typedefs.h"



#ifdef __MT_PTP2_C__
#define PTP2_EXTERN
#else
#define PTP2_EXTERN extern
#endif



extern void __iomem *ptp2_base;	/* 0x10200000 */

/* PTP2_BASEADDR            (0x10200600) */
#define PTP2_BASEADDR            (ptp2_base + 0x600)
#define PTP2_CTRL_REG_BASEADDR   (PTP2_BASEADDR + 0x70)
#define PTP2_CTRL_REG_0          (PTP2_CTRL_REG_BASEADDR + 0x8)	/* 0x78 */
#define PTP2_CTRL_REG_1          (PTP2_CTRL_REG_BASEADDR + 0xc)	/* 0x7C */
#define PTP2_CTRL_REG_2          (PTP2_CTRL_REG_BASEADDR + 0x10)	/* 0x80 */
#define PTP2_CTRL_REG_3          (PTP2_CTRL_REG_BASEADDR + 0x14)	/* 0x84 */
#define PTP2_CTRL_REG_4          (PTP2_CTRL_REG_BASEADDR + 0x18)	/* 0x88 */
#define PTP2_REG_NUM             2



/**
 * PTP2 control register 0
 */
/* 0x10200678 */
#define PTP2_DET_SWRST                     31:31
#define PTP2_DET_RAMPSTART                 13:12
#define PTP2_DET_RAMPSTEP                  11:8
#define PTP2_DET_DELAY                      7:4
#define PTP2_DET_AUTO_STOP_BYPASS_ENABLE    3:3
#define PTP2_DET_TRIGGER_PUL_DELAY          2:2
#define PTP2_CTRL_ENABLE                    1:1
#define PTP2_DET_ENABLE                     0:0



/*
 * PTP2 control register 1
 */
/* 0x120067C */
#define PTP2_MP0_nCORERESET        31:28
#define PTP2_MP0_STANDBYWFE        27:24
#define PTP2_MP0_STANDBYWFI        23:20
#define PTP2_MP0_STANDBYWFIL2      19:19



/* PTP2_BIG_BASEADDR           (0x10200200) */
#define PTP2_BIG_BASEADDR        (ptp2_base + 0x200)
#define PTP2_BIG_CTRL_REG_BASEADDR   (PTP2_BIG_BASEADDR + 0x70)
#define PTP2_BIG_CTRL_REG_0          (PTP2_BIG_CTRL_REG_BASEADDR)	/* 0x70 */
#define PTP2_BIG_CTRL_REG_1          (PTP2_BIG_CTRL_REG_BASEADDR + 0x4)	/* 0x74 */
#define PTP2_BIG_CTRL_REG_2          (PTP2_BIG_CTRL_REG_BASEADDR + 0x8)	/* 0x78 */
#define PTP2_BIG_CTRL_REG_3          (PTP2_BIG_CTRL_REG_BASEADDR + 0xC)	/* 0x7C */
#define PTP2_BIG_CTRL_REG_4          (PTP2_BIG_CTRL_REG_BASEADDR + 0x10)	/* 0x80 */
#define PTP2_BIG_CTRL_REG_5          (PTP2_BIG_CTRL_REG_BASEADDR + 0x14)	/* 0x84 */
#define PTP2_BIG_CTRL_REG_6          (PTP2_BIG_CTRL_REG_BASEADDR + 0x18)	/* 0x88 */
#define PTP2_DET_CPU_ENABLE_ADDR     (PTP2_BIG_CTRL_REG_BASEADDR + 0x1C)	/* 0x8C */
#define PTP2_BIG_CTRL_VFBB_0         (PTP2_BIG_CTRL_REG_BASEADDR + 0x50)    /* 0xC0 */
#define PTP2_BIG_CTRL_VFBB_1         (PTP2_BIG_CTRL_REG_BASEADDR + 0x54)    /* 0xC4 */
#define PTP2_BIG_REG_NUM             10



/* 0x1020028C */
#define PTP2_BIG_DET_TRIGGER_PUL_DELAY    16:16
#define PTP2_BIG_DET_RAMPSTART            15:14
#define PTP2_BIG_DET_DELAY                13:10
#define PTP2_BIG_DET_RAMPSTEP              9:6
#define PTP2_BIG_DET_AUTOSTOP_ENABLE       5:5
#define PTP2_BIG_DET_NOCPU_ENABLE          4:4
#define PTP2_BIG_DET_CPU_ENABLE            3:0



#define PTP2_BIG_CTRL_FBB_ENABLE        4:4
#define PTP2_BIG_CTRL_FBB_SW_ACK        4:4
#define PTP2_BIG_CTRL_FBB_SW_ENABLE     3:3



#define PTP2_BIG_CTRL_SPARK_SW_ENABLE   2:2
#define PTP2_BIG_CTRL_SW_ENABLE         1:1
#define PTP2_BIG_CTRL_SPM_ENABLE        0:0



/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_)                    (unsigned)(1 << (_bit_))
#define BITS(_bits_, _val_)           ((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))



/* #define ptp2_read(addr)          (*(volatile unsigned int *)(addr)) */
/* #define ptp2_write(addr, val)    (*(volatile unsigned int *)(addr) = (unsigned int)(val)) */
/**
 * Read/Write a field of a register.
 * @addr:       Address of the register
 * @range:      The field bit range in the form of MSB:LSB
 * @val:        The value to be written to the field
 */
#define ptp2_read(addr)	DRV_Reg32(addr)
#define ptp2_write(addr, val)    mt_reg_sync_writel(val, addr)
#define ptp2_write_field(addr, range, val)    ptp2_write(addr, (ptp2_read(addr) & ~(BITMASK(range))) | BITS(range, val))



/*
 * PTP2 rampstart rate
 */
enum {
	PTP2_RAMPSTART_0 = 0b00,
	PTP2_RAMPSTART_1 = 0b01,
	PTP2_RAMPSTART_2 = 0b10,
	PTP2_RAMPSTART_3 = 0b11
};



/**
 * PTP2 control register
 */
enum {
	PTP2_CTRL_CPU_SPARK_EN_REG = 0,
	PTP2_CTRL_SPM_EN_REG = 1,
	PTP2_CTRL_NOCPU_LO_TRIG_REG = 2,
	PTP2_CTRL_CPU0_LO_TRIG_REG = 3,
	PTP2_CTRL_CPU1_LO_TRIG_REG = 4,
	PTP2_CTRL_CPU2_LO_TRIG_REG = 5,
	PTP2_CTRL_CPU3_LO_TRIG_REG = 6,
	PTP2_CTRL_REG_NUM = 7
};



/**
 * PTP2 LO trigger
 */
enum {
	PTP2_CORE_RESET = 0,
	PTP2_DEBUG_RESET = 1,
	PTP2_STANDBYWFI = 2,
	PTP2_STANDBYWFE = 3,
	PTP2_STANDBYWFI2 = 4,
	PTP2_TRIG_NUM = 5
};



/*
 * PTP2 register setting
 */
struct PTP2_data {
	/* 10200678 */
	unsigned int SWRST;	/* 31:31 */
	unsigned int RAMPSTART;	/* 13:12 */
	unsigned int RAMPSTEP;	/* 11:8 */
	unsigned int DELAY;	/* 7:4 */
	unsigned int AUTO_STOP_BYPASS_ENABLE;	/* 3:3 */
	unsigned int TRIGGER_PUL_DELAY;	/* 2:2 */
	unsigned int CTRL_ENABLE;	/* 1:1 */
	unsigned int DET_ENABLE;	/* 0:0 */
};



struct PTP2_big_data {
	/* 1020028C */
	unsigned int BIG_TRIGGER_PUL_DELAY;	/* 16:16 */
	unsigned int BIG_RAMPSTART;	/* 15:14 */
	unsigned int BIG_DELAY;	/* 13:10 */
	unsigned int BIG_RAMPSTEP;	/* 9:6 */
	unsigned int BIG_AUTOSTOPENABLE;	/* 5:5 */
	unsigned int BIG_NOCPUENABLE;	//* 4:4 */* / L1 LO
	unsigned int BIG_CPUENABLE;	//* 3:0 */* / L2 LO
};



struct PTP2_trig {
	/* 1020067C */
	unsigned int mp0_nCORE_RESET;	/* 31:28 */
	unsigned int mp0_STANDBY_WFE;	/* 27:24 */
	unsigned int mp0_STANDBY_WFI;	/* 23:20 */
	unsigned int mp0_STANDBY_WFIL2;	/* 19:19 */
};



struct PTP2_big_trig {
	/*
	   31 : CPU PO RESET
	   30 : CORE RESET
	   29 : WFI
	   28 : WFE
	   0x10200278, 0x1020027C, 0x10200280
	 */
	unsigned int volatile ctrl_regs[3];
};



/* ptp2 FBB, SPARK status definition */
#define PTP2_ENABLE_FBB_SW          (1 << 0)
#define PTP2_ENABLE_FBB_SPM         (1 << 1)
#define PTP2_ENABLE_SPARK_SW        (1 << 2)
#define PTP2_ENABLE_SPARK_SPM       (1 << 3)
#define PTP2_ENABLE_DCC_AUTO_CAL    (1 << 4)
#define PTP2_ENABLE_DCC_CALIN       (1 << 5)


PTP2_EXTERN void turn_on_FBB(void);
PTP2_EXTERN void turn_off_FBB(void);
PTP2_EXTERN void turn_on_SPARK(void);
PTP2_EXTERN void turn_off_SPARK(char *message);
PTP2_EXTERN void turn_on_big_LO(void);
PTP2_EXTERN void turn_off_big_LO(void);
PTP2_EXTERN void turn_on_LO(void);
PTP2_EXTERN void turn_off_LO(void);

PTP2_EXTERN void ptp2_pre_iomap(void);
PTP2_EXTERN int ptp2_pre_init(void);


#undef PTP2_EXTERN
#endif				/* __MT_PTP2_H__ */
