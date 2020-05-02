#include <linux/pm.h>
#include <linux/bug.h>
#include <linux/memblock.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/page.h>
#include <mach/mt_reg_base.h>
#include <mach/irqs.h>

extern struct smp_operations mt_smp_ops;
extern void __init mt_timer_init(void);
extern void mt_fixup(struct tag *tags, char **cmdline, struct meminfo *mi);
extern void mt_reserve(void);

/* FIXME: need to remove */
extern void arm_machine_restart(char mode, const char *cmd);

void __init mt_init(void)
{
	/* FIXME: need to check if this setting is required */
	/* enable bus out of order command queue to enhance boot time */
#if 1
	volatile unsigned int opt;
	opt = readl(IOMEM(BUS_DBG_BASE));
	opt |= 0x1;
	writel(opt, IOMEM(BUS_DBG_BASE));
	dsb();
#endif
}

static struct map_desc mt_io_desc[] __initdata = {
/* #if defined(CONFIG_MTK_FPGA) */
	{
	 .virtual = CKSYS_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CKSYS_BASE)),
	 .length = SZ_16M * 2,
	 .type = MT_DEVICE,
	 },
	{
	 .virtual = HAN_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(HAN_BASE)),
	 .length = SZ_16M * 6,
	 .type = MT_DEVICE,
	 },
	{
	 /* virtual 0xF9000000, physical 0x00100000 */
	 .virtual = SYSRAM_BASE,
	 .pfn = __phys_to_pfn(BOOTSRAM_BASE),
	 .length = SZ_64K,
	 .type = MT_MEMORY_NONCACHED},
	/* FIXME: comment out for early porting */
#if 0
	{
	 .virtual = G3D_CONFIG_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(G3D_CONFIG_BASE)),
	 .length = SZ_128K,
	 .type = MT_DEVICE},
	{
	 .virtual = DISPSYS_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(DISPSYS_BASE)),
	 .length = SZ_16M,
	 .type = MT_DEVICE},
	{
	 .virtual = IMGSYS_CONFG_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(IMGSYS_CONFG_BASE)),
	 .length = SZ_16M,
	 .type = MT_DEVICE},
	{
	 .virtual = VDEC_GCON_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VDEC_GCON_BASE)),
	 .length = SZ_16M,
	 .type = MT_DEVICE},
	{
	 /* virtual 0xF7000000, physical 0x08000000 */
	 .virtual = DEVINFO_BASE,
	 .pfn = __phys_to_pfn(0x08000000),
	 .length = SZ_64K,
	 .type = MT_DEVICE},
	{
	 .virtual = CONN_BTSYS_PKV_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(CONN_BTSYS_PKV_BASE)),
	 .length = SZ_1M,
	 .type = MT_DEVICE},
	{
	 /* virtual 0xF9000000, physical 0x00100000 */
	 .virtual = INTER_SRAM,
	 .pfn = __phys_to_pfn(0x00100000),
	 .length = SZ_64K,
	 .type = MT_MEMORY_NONCACHED},
#endif
/* #endif */
};

void __init mt_map_io(void)
{
	iotable_init(mt_io_desc, ARRAY_SIZE(mt_io_desc));
}


MACHINE_START(MT8173, "MT8173")
    .atag_offset = 0x00000100, .map_io = mt_map_io, .smp = smp_ops(mt_smp_ops), .init_irq =
    mt_init_irq, .init_time = mt_timer_init, .init_machine = mt_init, .fixup = mt_fixup,
    /* FIXME: need to implement the restart function */
    .restart = arm_machine_restart, .reserve = mt_reserve, MACHINE_END
