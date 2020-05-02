
#ifndef __MTK_IR_REGS_H__
#define __MTK_IR_REGS_H__



/*#define IRRX_BASE_PHY        (unsigned long)0x1100c000*/
/*#define IRRX_BASE_VIRTUAL    (unsigned long)(IO_PHYS_TO_VIRT(IRRX_BASE_PHY))*/

#define IRRX_REGISTER_BYTES 4   /*  how many bytes per register has*/

extern unsigned long IRRX_BASE_PHY;
/* this value is getted from device tree , the base ohy address*/
extern unsigned long IRRX_BASE_PHY_END;
extern unsigned long IRRX_BASE_VIRTUAL;
/*  this value is of_ iomap from device tree's IRRX_BASE_PHY*/
extern unsigned long IRRX_BASE_VIRTUAL_END;


/*   IRRX register define**/
#define IRRX_COUNT_HIGH_REG        ((unsigned long)0x0000)
#define IRRX_CH_BITCNT_MASK        ((unsigned long)0x0000003f)
#define IRRX_CH_BITCNT_BITSFT      ((unsigned long)0)
#define IRRX_CH_1ST_PULSE_MASK     ((unsigned long)0x0000ff00)
#define IRRX_CH_1ST_PULSE_BITSFT   ((unsigned long)8)
#define IRRX_CH_2ND_PULSE_MASK     ((unsigned long)0x00ff0000)
#define IRRX_CH_2ND_PULSE_BITSFT   ((unsigned long)16)
#define IRRX_CH_3RD_PULSE_MASK     ((unsigned long)0xff000000)
#define IRRX_CH_3RD_PULSE_BITSFT   ((unsigned long)24)
#define IRRX_COUNT_MID_REG        ((unsigned long)0x0004)
#define IRRX_COUNT_LOW_REG        ((unsigned long)0x0008)
#define IRRX_CONFIG_HIGH_REG     ((unsigned long)0x000c)
#define IRRX_CH_DISPD        ((unsigned long)(1 << 15))
#define IRRX_CH_IGB0         ((unsigned long)(1 << 14))
#define IRRX_CH_CHKEN        ((unsigned long)(1 << 13)) /*enable puse width*/
#define IRRX_CH_DISCH        ((unsigned long)(1 << 7))
#define IRRX_CH_DISCL        ((unsigned long)(1 << 6))
#define IRRX_CH_IGSYN        ((unsigned long)(1 << 5))
#define IRRX_CH_ORDINV       ((unsigned long)(1 << 4))
#define IRRX_CH_RC5_1ST      ((unsigned long)(1 << 3))
#define IRRX_CH_RC5          ((unsigned long)(1 << 2))
#define IRRX_CH_IRI          ((unsigned long)(1 << 1))
#define IRRX_CH_HWIR         ((unsigned long)(1 << 0))


#define IRRX_CH_END_7        ((unsigned long)(0x07 << 16))
#define IRRX_CH_END_15       ((unsigned long)(0x0f << 16))  /*[22:16]*/
#define IRRX_CH_END_23		 ((unsigned long)(0x17 << 16))
#define IRRX_CH_END_31		 ((unsigned long)(0x1f << 16))
#define IRRX_CH_END_39		 ((unsigned long)(0x27 << 16))
#define IRRX_CH_END_47		 ((unsigned long)(0x2f << 16))
#define IRRX_CH_END_55		 ((unsigned long)(0x07 << 16))
#define IRRX_CH_END_63		 ((unsigned long)(0x0f << 16))

#define IRRX_CONFIG_LOW_REG       ((unsigned long)0x0010)   /*IRCFGL*/
#define IRRX_SAPERIOD_MASK        ((unsigned long)0xff<<0)  /*[7:0]   sampling period*/
#define IRRX_SAPERIOD_OFFSET      ((unsigned long)0)
#define IRRX_CHK_PERIOD_MASK      ((unsigned long)0x1fff<<8) /*[20:8]   ir pulse width sample period*/
#define IRRX_CHK_PERIOD_OFFSET    ((unsigned long)8)
#define IRRX_THRESHOLD_REG       ((unsigned long) 0x0014)
#define IRRX_THRESHOLD_MASK      ((unsigned long)0x7f<<0)
#define IRRX_THRESHOLD_OFFSET     ((unsigned long)0)

#define IRRX_ICLR_MASK          ((unsigned long)1<<7) /* interrupt clear reset ir*/
#define IRRX_ICLR_OFFSET         ((unsigned long)7)

#define IRRX_DGDEL_MASK          ((unsigned long)3<<8) /*de-glitch select*/
#define IRRX_DGDEL_OFFSET        ((unsigned long)8)

#define IRRX_RCMM_THD_REG        ((unsigned long)0x0018)

#define IRRX_RCMM_ENABLE_MASK      ((unsigned long)0x1<<31)
#define IRRX_RCMM_ENABLE_OFFSET    ((unsigned long)31)    /* 1 enable rcmm , 0 disable rcmm*/

#define IRRX_RCMM_THD_00_MASK      ((unsigned long)0x7f<<0)
#define IRRX_RCMM_THD_00_OFFSET    ((unsigned long)0)
#define IRRX_RCMM_THD_01_MASK      ((unsigned long)0x7f<<7)
#define IRRX_RCMM_THD_01_OFFSET    ((unsigned long)7)

#define IRRX_RCMM_THD_10_MASK      ((unsigned long)0x7f<<14)
#define IRRX_RCMM_THD_10_OFFSET    ((unsigned long)14)
#define IRRX_RCMM_THD_11_MASK      ((unsigned long)0x7f<<21)
#define IRRX_RCMM_THD_11_OFFSET    ((unsigned long)21)


#define IRRX_RCMM_THD_REG0        ((unsigned long)0x001c)
#define IRRX_RCMM_THD_20_MASK      ((unsigned long)0x7f<<0)
#define IRRX_RCMM_THD_20_OFFSET    ((unsigned long)0)
#define IRRX_RCMM_THD_21_MASK      ((unsigned long)0x7f<<7)
#define IRRX_RCMM_THD_21_OFFSET    ((unsigned long)7)



#define IRRX_IRCLR               ((unsigned long)0x0020)
#define IRRX_IRCLR_MASK           ((unsigned long)0x1<<0)
#define IRRX_IRCLR_OFFSET         ((unsigned long)0)


#define IRRX_EXPBCNT               ((unsigned long)0x0028)
#define IRRX_IRCHK_CNT           ((unsigned long)0x7f)
#define IRRX_IRCHK_CNT_OFFSET         ((unsigned long)6)


/*
#define IRRX_CHKDATA0               0x0088
#define IRRX_CHKDATA1               0x008C
#define IRRX_CHKDATA2               0x0090
#define IRRX_CHKDATA3               0x0094
#define IRRX_CHKDATA4               0x0098
#define IRRX_CHKDATA5               0x009C
#define IRRX_CHKDATA6               0x00a0
#define IRRX_CHKDATA7               0x00a4
#define IRRX_CHKDATA8               0x00a8
#define IRRX_CHKDATA9               0x00ac
#define IRRX_CHKDATA10              0x00b0
#define IRRX_CHKDATA11              0x00b4
#define IRRX_CHKDATA12              0x00b8
#define IRRX_CHKDATA13              0x00bc
#define IRRX_CHKDATA14              0x00c0
#define IRRX_CHKDATA15              0x00c4
#define IRRX_CHKDATA16              0x00c8
*/
#define IRRX_IRINT_EN              ((unsigned long)0x00cc)
#define IRRX_INTEN_MASK           ((unsigned long)0x1<<0)
#define IRRX_INTEN_OFFSET         ((unsigned long)0)


#define IRRX_IRINT_CLR             ((unsigned long) 0x00d0)
#define IRRX_INTCLR_MASK           ((unsigned long)0x1<<0)
#define IRRX_INTCLR_OFFSET         ((unsigned long)0)


#define IRRX_INTSTAT_REG        ((unsigned long)0x00d4) /*here must be care, 8127 */
#define IRRX_INTSTAT_OFFSET     ((unsigned long)18)




#define REGISTER_WRITE(u4Addr, u4Val)     (*((volatile unsigned int*)(u4Addr)) = (u4Val))
#define REGISTER_READ(u4Addr)             (*((volatile unsigned int*)(u4Addr)))

#define IO_WRITE(base, offset, u4Val)		(*((volatile unsigned int*)(base + offset)) = (u4Val))
#define IO_READ(base, offset)              (*((volatile unsigned int*)(base + offset)))

#define IR_READ(u4Addr)          IO_READ(IRRX_BASE_VIRTUAL, u4Addr)
#define IR_WRITE(u4Addr, u4Val)  IO_WRITE(IRRX_BASE_VIRTUAL, u4Addr, u4Val)

#define IR_WRITE_MASK(u4Addr, u4Mask, u4Offet, u4Val)  IR_WRITE(u4Addr, ((IR_READ(u4Addr) & (~(u4Mask))) | (((u4Val) << (u4Offet)) & (u4Mask))))
#define IR_READ_MASK(u4Addr, u4Mask, u4Offet)  ((IR_READ(u4Addr) & (u4Mask)) >> (u4Offet))
#endif /* __IRRX_VRF_HW_H__ */
