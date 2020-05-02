#ifndef MT_DBG_V71_H
#define MT_DBG_V71_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/proc-fns.h>
#include <asm/system.h>
#if !defined(__KERNEL__)	/* || defined (__CTP__) */
#include "reg_base.H"
#else				/* #if !defined (__KERNEL__) //|| defined(__CTP__) */
#include <mach/mt_reg_base.h>
#endif				/* #if !defined (__KERNEL__) //|| defined(__CTP__) */


#define DIDR_VERSION_SHIFT 16
#define DIDR_VERSION_MASK  0xF
#define DIDR_VERSION_7_1   5
#define DIDR_BP_SHIFT      24
#define DIDR_BP_MASK       0xF
#define DIDR_WP_SHIFT      28
#define DIDR_WP_MASK       0xF
#define CLAIMCLR_CLEAR_ALL 0xff

#define DRAR_VALID_MASK   0x00000003
#define DSAR_VALID_MASK   0x00000003
#define DRAR_ADDRESS_MASK 0xFFFFF000
#define DSAR_ADDRESS_MASK 0xFFFFF000
#define OSLSR_OSLM_MASK   0x00000009
#define OSLAR_UNLOCKED    0x00000000
#define OSLAR_LOCKED      0xC5ACCE55
#define LAR_UNLOCKED      0xC5ACCE55
#define LAR_LOCKED        0x00000000
#define OSDLR_UNLOCKED    0x00000000
#define OSDLR_LOCKED      0x00000001

#define DBGDSCR_RXFULL	  (1<<30)
#define DBGDSCR_TXFULL	  (1<<29)


#define DBGREG_BP_VAL     0x0
#define DBGREG_WP_VAL     0x1
#define DBGREG_BP_CTRL    0x2
#define DBGREG_WP_CTRL    0x3
#define DBGREG_BP_XVAL    0x4



inline unsigned read_dbgdidr(void)
{
	register unsigned ret;
	__asm__ __volatile("mrc p14, 0, %0, c0, c0, 0\n\t":"=r"(ret));

	return ret;
}


inline unsigned read_dbgosdlr(void)
{
	register unsigned ret;
	__asm__ __volatile("mrc p14, 0, %0, c1, c3, 4\n\t":"=r"(ret));

	return ret;
}

inline unsigned write_dbgosdlr(unsigned data)
{
	__asm__ __volatile("mcr p14, 0, %0, c1, c3, 4 \n\t" :  : "r"(data));

	return data;
}

inline void write_dbgoslar(unsigned key)
{
	__asm__ __volatile("mcr p14, 0, %0, c1, c0, 4 \n\t" :  : "r"(key));
}



inline unsigned read_dbgdscr(void)
{
	register unsigned ret;
	__asm__ __volatile("mrc p14, 0, %0, c0, c1, 0\n\t":"=r"(ret));

	return ret;
}


inline void write_dbgdscr(unsigned key)
{
	__asm__ __volatile("mrc p14, 0, %0, c0, c1, 0 \n\t" :  : "r"(key));
}


inline unsigned int *save_dbg_regs(unsigned int *p, unsigned int dbg_base)
{
	unsigned int osdlr, dscr;

	/***************************************************/
	/* Test DBGDSCRext for halt mode:                  */
	/* if in debug mode, do not go through,            */
	/* otherwise, die at oslock.                       */
	/***************************************************/
	if (*(volatile unsigned long *)(dbg_base + 0x4) & 1)
		return p;

	/* oslock */
	write_dbgoslar(OSLAR_LOCKED);
	isb();

	/* save register */
	__asm__ __volatile__("mrc p14, 0, %1, c0, c1, 0	@DBGDSCR\n\t"
			     "movw	r3, #:lower16:0x6c30fc3c\n\t"
			     "movt	r3, #:upper16:0x6c30fc3c\n\t"
			     "and r4, %1, r5\n\t"
			     "mrc p14, 0, r5, c0, c6, 0	@DBGWFAR\n\t"
			     "mrc p14, 0, r6, c0, c7, 0	@DBGBCR\n\t"
			     "stm %0!, {r4-r6}\n\t"
			     "mrc p14, 0, r4, c0, c0, 4	@DBGBVR\n\t"
			     "mrc p14, 0, r5, c0, c1, 4	@DBGBVR\n\t"
			     "mrc p14, 0, r6, c0, c2, 4	@DBGBVR\n\t"
			     "mrc p14, 0, r7, c0, c3, 4	@DBGBVR\n\t"
			     "mrc p14, 0, r8, c0, c4, 4	@DBGBVR\n\t"
			     "mrc p14, 0, r9, c0, c5, 4	@DBGBVR\n\t"
			     "stm %0!, {r4-r9}\n\t"
			     "mrc p14, 0, r4, c1, c4, 1	@DBGBXVR\n\t"
			     "mrc p14, 0, r5, c1, c5, 1	@DBGBXVR\n\t"
			     "stm %0!, {r4-r5}\n\t"
			     "mrc p14, 0, r4, c0, c0, 6	@DBGWVR\n\t"
			     "mrc p14, 0, r5, c0, c1, 6	@DBGWVR\n\t"
			     "mrc p14, 0, r6, c0, c2, 6	@DBGWVR\n\t"
			     "mrc p14, 0, r7, c0, c3, 6	@DBGWVR\n\t"
			     "stm %0!, {r4-r7}\n\t"
			     "mrc p14, 0, r4, c0, c7, 0	@DBGVCR\n\t"
			     "mrc p14, 0, r5, c7, c9, 6	@DBGCLAIMCLR\n\t"
			     "mrc p14, 0, r5, c7, c9, 6	@DBGCLAIMCLR\n\t"
			     "stm %0!, {r4-r7}\n\t":"+r"(p), "=r"(dscr)
 : );

	if (dscr & DBGDSCR_TXFULL) {
		*p = *(volatile unsigned long *)(dbg_base + 0x8c);	/* DTRTXext, TXFULL cleared */

		/* write back in internal view to restore TXFULL bit */
		__asm__ __volatile__("mcr p14, 0, %0, c0, c7, 0 	@DBGVCR \n\t" :  : "r"(p[0]));
	}
	p++;

	if (dscr & DBGDSCR_RXFULL) {
		*p = *(volatile unsigned long *)(dbg_base + 0x14);	/* DTRRXext */
	}
	p++;

	/* double lock */
	osdlr = read_dbgosdlr();
	write_dbgosdlr(osdlr | OSDLR_LOCKED);

	return p;
}


inline void mt_restore_dbg_regs(unsigned int *p, unsigned int dbg_base)
{
	unsigned int dscr;

	/***************************************************/
	/* Test DBGDSCRext for halt mode:                  */
	/* if in debug mode, do not go through,            */
	/* otherwise, die at oslock.                       */
	/***************************************************/
	if (*(volatile unsigned long *)(dbg_base + 0x4) & 1)
		return;

	/* oslock */
	write_dbgoslar(OSLAR_LOCKED);
	isb();

	/* restore register */
	__asm__ __volatile__("ldm %0!, {r4-r6}\n\t"
			     "mov %1, r4\n\t"
			     "mcr p14, 0, r4, c0, c1, 0	@DBGDSCR\n\t"
			     "mcr p14, 0, r5, c0, c6, 0	@DBGWFAR\n\t"
			     "mcr p14, 0, r6, c0, c7, 0	@DBGBCR\n\t"
			     "stm %0!, {r4-r9}\n\t"
			     "mcr p14, 0, r4, c0, c0, 4	@DBGBVR\n\t"
			     "mcr p14, 0, r5, c0, c1, 4	@DBGBVR\n\t"
			     "mcr p14, 0, r6, c0, c2, 4	@DBGBVR\n\t"
			     "mcr p14, 0, r7, c0, c3, 4	@DBGBVR\n\t"
			     "mcr p14, 0, r8, c0, c4, 4	@DBGBVR\n\t"
			     "mcr p14, 0, r9, c0, c5, 4	@DBGBVR\n\t"
			     "stm %0!, {r4-r5}\n\t"
			     "mcr p14, 0, r4, c1, c4, 1	@DBGBXVR\n\t"
			     "mcr p14, 0, r5, c1, c5, 1	@DBGBXVR\n\t"
			     "stm %0!, {r4-r7}\n\t"
			     "mcr p14, 0, r4, c0, c0, 6	@DBGWVR\n\t"
			     "mcr p14, 0, r5, c0, c1, 6	@DBGWVR\n\t"
			     "mcr p14, 0, r6, c0, c2, 6	@DBGWVR\n\t"
			     "mcr p14, 0, r7, c0, c3, 6	@DBGWVR\n\t"
			     "stm %0!, {r4-r7}\n\t"
			     "mcr p14, 0, r4, c0, c7, 0	@DBGVCR\n\t"
			     "mcr p14, 0, r5, c7, c9, 6	@DBGCLAIMCLR\n\t"
			     "mcr p14, 0, r5, c7, c9, 6	@DBGCLAIMCLR\n\t":"+r"(p), "=r"(dscr)
 : );

	if (dscr & DBGDSCR_TXFULL) {
		/* write back in internal view to restore TXFULL bit */
		__asm__ __volatile__("mcr p14, 0, %0, c0, c7, 0 	@DBGVCR \n\t" :  : "r"(p[0]));
	}
	p++;

	if (dscr & DBGDSCR_RXFULL) {
		*(volatile unsigned long *)(dbg_base + 0x14) = *p;	/* DTRRXext */
	}
	p++;



	/* os unlock */
	write_dbgoslar(OSLAR_UNLOCKED);

}



#endif
