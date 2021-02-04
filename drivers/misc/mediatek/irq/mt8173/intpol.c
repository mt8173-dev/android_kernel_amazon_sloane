#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>

#ifdef CONFIG_OF
void __iomem *INT_POL_CTL0;

/*
 * This macro must be used by the different irqchip drivers to declare
 * the association between their DT compatible string and their
 * initialization function.
 *
 * @name: name that must be unique accross all IRQCHIP_DECLARE of the
 * same file.
 * @compstr: compatible string of the irqchip driver
 * @fn: initialization function
 */
#define IRQCHIP_DECLARE(name, compstr, fn)				\
	static const struct of_device_id irqchip_of_match_##name	\
	__used __section(__irqchip_of_table)				\
	= { .compatible = compstr, .data = fn }
#endif

#define GIC_HW_IRQ_BASE  32
#define INT_POL_INDEX(a)   ((a) - GIC_HW_IRQ_BASE)

static int mtk_int_pol_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int irq = d->hwirq;
	u32 offset, reg_index, value;

	offset = INT_POL_INDEX(irq) & 0x1F;
	reg_index = INT_POL_INDEX(irq) >> 5;

	/* This arch extension was called with irq_controller_lock held,
	   so the read-modify-write will be atomic */
	value = readl(INT_POL_CTL0 + reg_index * 4);
	if (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING)
		value |= (1 << offset);
	else
		value &= ~(1 << offset);
	writel(value, INT_POL_CTL0 + reg_index * 4);

	return 0;
}

#ifdef CONFIG_OF
int __init mt_intpol_of_init(struct device_node *node, struct device_node *parent)
{

	INT_POL_CTL0 = of_iomap(node, 0);
	WARN(!INT_POL_CTL0, "unable to map pol registers\n");

	gic_arch_extn.irq_set_type = mtk_int_pol_set_type;

	return 0;
}

IRQCHIP_DECLARE(mt_intpol, "mediatek,mt6577-intpol", mt_intpol_of_init);
#endif
