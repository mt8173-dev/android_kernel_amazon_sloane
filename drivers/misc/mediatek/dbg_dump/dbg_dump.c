#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/printk.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mach/mt_reg_dump.h>
#include <mach/dbg_dump.h>
#include <linux/slab.h>

static unsigned int is_reg_dump_device_registered;
static unsigned char *mcu_reg_base;
static struct platform_driver dbg_reg_dump_driver;

static int mt_reg_dump(char *buf)
{
	/* Get core numbers */
	int i, num_little, num_big;
	char *ptr = buf;
	struct device_node *node = NULL;
	unsigned long pc_value, fp_value, sp_value;
	unsigned long size = 0, offset = 0;
	char str[KSYM_SYMBOL_LEN];

	for (num_little = 0;; num_little++) {
		node = of_find_compatible_node(node, "cpu", "arm,cortex-a53");
		if (node == NULL)
			break;
	}

	num_big = num_possible_cpus() - num_little;

	if (is_reg_dump_device_registered) {
		/* Get PC, FP, SP and save to buf */
		for (i = 0; i < num_little; i++) {
			pc_value = readq(IOMEM(CORE_PC(i)));
			fp_value = readq(IOMEM(CORE_FP(i)));
			sp_value = readq(IOMEM(CORE_SP(i)));
			kallsyms_lookup(pc_value, &size, &offset, NULL, str);
			ptr +=
				sprintf(ptr, "CA53 CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n",
				i, pc_value, str, offset, fp_value, sp_value);
		}
		for (i = 0; i < num_big; i++) {
			writel(1 + (i << 3), IOMEM(MON_SEL));
			pc_value = readq(IOMEM(MON_OUT));
			kallsyms_lookup(pc_value, &size, &offset, NULL, str);
			ptr += sprintf(ptr, "CA72 CORE_%d PC = 0x%lx(%s + 0x%lx)\n", i, pc_value, str, offset);
		}
		pr_err("%s", ptr);
		return 0;
	}

	return -1;
}

static ssize_t last_pc_dump_show(struct device_driver *driver, char *buf)
{
	int ret = mt_reg_dump(buf);
	if (ret == -1)
		pr_crit("Dump error in %s, %d\n", __func__, __LINE__);

	return strlen(buf);
}

static ssize_t last_pc_dump_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(last_pc_dump, 0444, last_pc_dump_show, last_pc_dump_store);

static int __init dbg_reg_dump_probe(struct platform_device *pdev)
{
	int ret;
	char *buf;

#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,MCUCFG");
	if (node)
		mcu_reg_base = of_iomap(node, 0);

	if (!node || !mcu_reg_base) {
		pr_err("dbg_reg_dump_probe failed!\n");
		return -1;
	}
#else
	mcu_reg_base = data->mcu_regs;
#endif
	is_reg_dump_device_registered = 1;

	ret = driver_create_file(&dbg_reg_dump_driver.driver, &driver_attr_last_pc_dump);
	if (ret)
		pr_err("Fail to create mt_reg_dump_drv sysfs files");

	buf = kmalloc(4096, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	mt_reg_dump(buf);
	pr_err("%s", buf);
	kfree(buf);
	return 0;
}

static const struct of_device_id dbg_dump_of_ids[] __initconst = {
	{   .compatible = "mediatek,MCUCFG", },
	{}
};

static struct platform_driver dbg_reg_dump_driver __refdata = {
	.probe = dbg_reg_dump_probe,
	.driver = {
		.name = "dbg_reg_dump",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = dbg_dump_of_ids,
		},
};

/**
 * driver initialization entry point
 */
static int __init dbg_reg_dump_init(void)
{
	int err;

	err = platform_driver_register(&dbg_reg_dump_driver);
	if (err) {
		pr_err("dbg_reg_dump_init failed!!!\n");
		return err;
	}

	return 0;
}

/**
 * driver exit point
 */
static void __exit dbg_reg_dump_exit(void)
{
#ifdef CONFIG_OF
	if (mcu_reg_base) {
		iounmap(mcu_reg_base);
		mcu_reg_base = 0;
	}
#endif
}
module_init(dbg_reg_dump_init);
module_exit(dbg_reg_dump_exit);
