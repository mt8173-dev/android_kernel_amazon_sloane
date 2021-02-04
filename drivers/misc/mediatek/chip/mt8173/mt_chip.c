#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>

#include <mach/mt_chip.h>
#include <mach/mt_typedefs.h>
#include <asm/setup.h>

void __iomem *APHW_CHIPID;

enum {
	CID_UNINIT = 0,
	CID_INITIALIZING = 1,
	CID_INITIALIZED = 2,
};

atomic_t g_cid_init = ATOMIC_INIT(CID_UNINIT);

void __init init_chip_id(unsigned int line)
{
	if (CID_INITIALIZED == atomic_read(&g_cid_init))
		return;

	if (CID_INITIALIZING == atomic_read(&g_cid_init)) {
		pr_warn("%s (%d) state(%d)\n", __func__, line, atomic_read(&g_cid_init));
		return;
	}

	atomic_set(&g_cid_init, CID_INITIALIZING);
#ifdef CONFIG_OF
	{
		struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,CHIPID");
		if (node) {
			APHW_CHIPID = of_iomap(node, 0);
			WARN(!APHW_CHIPID, "unable to map APHW_CHIPID registers\n");
			atomic_set(&g_cid_init, CID_INITIALIZED);
		} else {
			atomic_set(&g_cid_init, CID_UNINIT);
			pr_warn("node not found\n");
		}
	}
#endif
	pr_alert("0x%04X 0x%04X 0x%04X 0x%04X (%04d)\n",
		readl(IOMEM(APHW_CHIPID)), readl(IOMEM(APHW_CHIPID) + 4), readl(IOMEM(APHW_CHIPID) + 8),
		(get_devinfo_with_index(4) & (1 << 28)) ? CHIP_SW_VER_03 : readl(IOMEM(APHW_CHIPID) + 12), line);
}

/* return hardware version */
static unsigned int __chip_hw_code(void)
{
	return (APHW_CHIPID) ? readl(IOMEM(APHW_CHIPID)) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_hw_ver(void)
{
	return (APHW_CHIPID) ? readl(IOMEM(APHW_CHIPID) + 8) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_sw_ver(void)
{
	if (get_devinfo_with_index(4) & (1 << 28))
		return CHIP_SW_VER_03;
	return (APHW_CHIPID) ? readl(IOMEM(APHW_CHIPID) + 12) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_hw_subcode(void)
{
	return (APHW_CHIPID) ? readl(IOMEM(APHW_CHIPID) + 4) : (C_UNKNOWN_CHIP_ID);
}

unsigned int mt_get_chip_id(void)
{
	unsigned int chip_id = __chip_hw_code();
	/*convert id if necessary*/
	return chip_id;
}
EXPORT_SYMBOL(mt_get_chip_id);

unsigned int mt_get_chip_hw_code(void)
{
	return __chip_hw_code();
}
EXPORT_SYMBOL(mt_get_chip_hw_code);

unsigned int mt_get_chip_hw_ver(void)
{
	return __chip_hw_ver();
}
EXPORT_SYMBOL(mt_get_chip_hw_ver);

unsigned int mt_get_chip_hw_subcode(void)
{
	return __chip_hw_subcode();
}
EXPORT_SYMBOL(mt_get_chip_hw_subcode);

unsigned int mt_get_chip_sw_ver(void)
{
	return __chip_sw_ver();
}
EXPORT_SYMBOL(mt_get_chip_sw_ver);

static unsigned int (*g_cbs[CHIP_INFO_MAX])(void) = {
	NULL,
	mt_get_chip_hw_code,
	mt_get_chip_hw_subcode,
	mt_get_chip_hw_ver,
	mt_get_chip_sw_ver,

	__chip_hw_code,
	__chip_hw_subcode,
	__chip_hw_ver,
	__chip_sw_ver,
};

unsigned int mt_get_chip_info(unsigned int id)
{
	if ((id <= CHIP_INFO_NONE) || (id >= CHIP_INFO_MAX))
		return 0;
	else if (NULL == g_cbs[id])
		return 0;
	return g_cbs[id]();
}
EXPORT_SYMBOL(mt_get_chip_info);

int __init chip_mod_init(void)
{
	struct mt_chip_drv *p_drv = get_mt_chip_drv();

	init_chip_id(0);

	pr_alert("CODE = %04x %04x %04x %04x, %04X %04X\n",
		__chip_hw_code(), __chip_hw_subcode(), __chip_hw_ver(),
		__chip_sw_ver(), mt_get_chip_hw_ver(), mt_get_chip_sw_ver());

	p_drv->info_bit_mask |= CHIP_INFO_BIT(CHIP_INFO_HW_CODE) |
		CHIP_INFO_BIT(CHIP_INFO_HW_SUBCODE) |
		CHIP_INFO_BIT(CHIP_INFO_HW_VER) |
		CHIP_INFO_BIT(CHIP_INFO_SW_VER);

	p_drv->get_chip_info = mt_get_chip_info;

	pr_alert("CODE = %08X %p", p_drv->info_bit_mask, p_drv->get_chip_info);

	return 0;
}

core_initcall(chip_mod_init);
MODULE_DESCRIPTION("MTK Chip Information");
MODULE_LICENSE("GPL");
