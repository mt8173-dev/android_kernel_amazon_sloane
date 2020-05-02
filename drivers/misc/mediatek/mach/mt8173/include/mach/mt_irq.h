#ifndef __MT_IRQ_H
#define __MT_IRQ_H
#include<linux/compiler.h>
/*
 * Define hadware registers.
 * use dts.
 */
#ifndef CONFIG_OF
#include "mt_reg_base.h"
#define GIC_DIST_BASE       (CA9_BASE + 0x1000)
#define GIC_CPU_BASE        (CA9_BASE + 0x2000)
#define INT_POL_CTL0        (MCUCFG_BASE + 0x620)
#else
extern void __iomem *ca9base;
extern void __iomem *mcucfgbase;
extern void __iomem *gic_cpubase;
#undef GIC_DIST_BASE
#undef GIC_CPU_BASE
#undef INT_POL_CTL0
#define INT_POL_CTL0        (mcucfgbase)
#define GIC_DIST_BASE       (ca9base)
#define GIC_CPU_BASE        (gic_cpubase)
#endif
/**irq  */
#define GIC_ICDISR (GIC_DIST_BASE + 0x80)
#define GIC_ICDISER0 (GIC_DIST_BASE + 0x100)
#define GIC_ICDISER1 (GIC_DIST_BASE + 0x104)
#define GIC_ICDISER2 (GIC_DIST_BASE + 0x108)
#define GIC_ICDISER3 (GIC_DIST_BASE + 0x10C)
#define GIC_ICDISER4 (GIC_DIST_BASE + 0x110)
#define GIC_ICDISER5 (GIC_DIST_BASE + 0x114)
#define GIC_ICDISER6 (GIC_DIST_BASE + 0x118)
#define GIC_ICDISER7 (GIC_DIST_BASE + 0x11C)
#define GIC_ICDISER8 (GIC_DIST_BASE + 0x120)
#define GIC_ICDICER0 (GIC_DIST_BASE + 0x180)
#define GIC_ICDICER1 (GIC_DIST_BASE + 0x184)
#define GIC_ICDICER2 (GIC_DIST_BASE + 0x188)
#define GIC_ICDICER3 (GIC_DIST_BASE + 0x18C)
#define GIC_ICDICER4 (GIC_DIST_BASE + 0x190)
#define GIC_ICDICER5 (GIC_DIST_BASE + 0x194)
#define GIC_ICDICER6 (GIC_DIST_BASE + 0x198)
#define GIC_ICDICER7 (GIC_DIST_BASE + 0x19C)
#define GIC_ICDICER8 (GIC_DIST_BASE + 0x1A0)

#define GIC_PRIVATE_SIGNALS     (32)
#define NR_GIC_SGI              (16)
#define NR_GIC_PPI              (16)
#define MT_NR_SPI               (234)
#define NR_MT_IRQ_LINE          (32 * (((NR_GIC_SGI + NR_GIC_PPI + MT_NR_SPI) + 31) / 32))

#define GIC_PPI_OFFSET          (27)
#define GIC_PPI_GLOBAL_TIMER    (GIC_PPI_OFFSET + 0)
#define GIC_PPI_LEGACY_FIQ      (GIC_PPI_OFFSET + 1)
#define GIC_PPI_PRIVATE_TIMER   (GIC_PPI_OFFSET + 2)
#define GIC_PPI_WATCHDOG_TIMER  (GIC_PPI_OFFSET + 3)
#define GIC_PPI_LEGACY_IRQ      (GIC_PPI_OFFSET + 4)
#define MT_DMA_BTIF_TX_IRQ_ID	(GIC_PRIVATE_SIGNALS + 103)
#define MT_DMA_BTIF_RX_IRQ_ID	(GIC_PRIVATE_SIGNALS + 104)


#if !defined(__ASSEMBLY__)
#define X_DEFINE_IRQ(__name, __num, __pol, __sens)  __name = __num,
enum {
#include "x_define_irq.h"
};
#undef X_DEFINE_IRQ

#endif

/* FIXME: Marcos Add for name alias (may wrong!!!!!) */
#define MT_SPM_IRQ_ID SLEEP_IRQ_BIT0_ID
#define MT_SPM1_IRQ_ID SLEEP_IRQ_BIT1_ID
#define MT_KP_IRQ_ID KP_IRQ_BIT_ID
#define MT_WDT_IRQ_ID WDT_IRQ_BIT_ID
#define MT_CIRQ_IRQ_ID SYS_CIRQ_IRQ_BIT_ID
#define MT_USB0_IRQ_ID USB_MCU_IRQ_BIT1_ID
#define MT_UART4_IRQ_ID UART3_IRQ_BIT_ID
/* assign a random number since it won't be used */

#endif				/*  !__IRQ_H__ */


