/*
* Copyright (c) 2014 MediaTek Inc.
* Author: James Liao <jamesjj.liao@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include "clk-mtk.h"
#include "clk-mt8173-pg.h"

#include <dt-bindings/clock/mt8173-clk.h>

#define	CHECK_PWR_ST	0

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#define clk_readl(addr)		readl(addr)
#define clk_writel(val, addr)	\
	do { writel(val, addr); wmb(); } while (0)	/* sync_write */
#define clk_setl(mask, addr)	clk_writel(clk_readl(addr) | (mask), addr)
#define clk_clrl(mask, addr)	clk_writel(clk_readl(addr) & ~(mask), addr)

/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define PWR_DOWN	0
#define PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*enable)(struct subsys *sys);
	int (*disable)(struct subsys *sys);
	int (*get_state)(struct subsys *sys);
};

struct subsys {
	const char		*name;
	uint32_t		sta_mask;
	void __iomem		*ctl_addr;
	uint32_t		sram_pdn_bits;
	uint32_t		sram_pdn_ack_bits;
	uint32_t		bus_prot_mask;
	struct subsys_ops	*ops;
};

static struct subsys_ops general_sys_ops;

static void __iomem *infracfg_base;
static void __iomem *spm_base;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)

#define SPM_PWR_STATUS			SPM_REG(0x060c)
#define SPM_PWR_STATUS_2ND		SPM_REG(0x0610)
#define SPM_VDE_PWR_CON			SPM_REG(0x0210)
#define SPM_MFG_PWR_CON			SPM_REG(0x0214)
#define SPM_VEN_PWR_CON			SPM_REG(0x0230)
#define SPM_ISP_PWR_CON			SPM_REG(0x0238)
#define SPM_DIS_PWR_CON			SPM_REG(0x023c)
#define SPM_VEN2_PWR_CON		SPM_REG(0x0298)
#define SPM_AUDIO_PWR_CON		SPM_REG(0x029c)
#define SPM_MFG_2D_PWR_CON		SPM_REG(0x02c0)
#define SPM_MFG_ASYNC_PWR_CON		SPM_REG(0x02c4)
#define SPM_USB_PWR_CON			SPM_REG(0x02cc)

#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTSTA1	INFRACFG_REG(0x0228)

#define SPM_PROJECT_CODE		0xb16

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)

#define DIS_PWR_STA_MASK		BIT(3)
#define MFG_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define VDE_PWR_STA_MASK		BIT(7)
#define VEN2_PWR_STA_MASK		BIT(20)
#define VEN_PWR_STA_MASK		BIT(21)
#define MFG_2D_PWR_STA_MASK		BIT(22)
#define MFG_ASYNC_PWR_STA_MASK		BIT(23)
#define AUDIO_PWR_STA_MASK		BIT(24)
#define USB_PWR_STA_MASK		BIT(25)

#define DISP_PROT_MASK			(BIT(1) | BIT(2))
#define MFG_2D_PROT_MASK		(BIT(14) | GENMASK(23, 21))

static struct subsys syss[] =	/* NR_SYSS */
{
	[SYS_VDE] = {
		.name = __stringify(SYS_VDE),
		.sta_mask = VDE_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VDE_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG] = {
		.name = __stringify(SYS_MFG),
		.sta_mask = MFG_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_PWR_CON, */
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(21, 16),
		.bus_prot_mask = MFG_2D_PROT_MASK,
		.ops = &general_sys_ops,
	},
	[SYS_VEN] = {
		.name = __stringify(SYS_VEN),
		.sta_mask = VEN_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VEN_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_ISP] = {
		.name = __stringify(SYS_ISP),
		.sta_mask = ISP_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_ISP_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_DIS] = {
		.name = __stringify(SYS_DIS),
		.sta_mask = DIS_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_DIS_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = DISP_PROT_MASK,
		.ops = &general_sys_ops,
	},
	[SYS_VEN2] = {
		.name = __stringify(SYS_VEN2),
		.sta_mask = VEN2_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_VEN2_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_AUDIO] = {
		.name = __stringify(SYS_AUDIO),
		.sta_mask = AUDIO_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_AUDIO_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG_2D] = {
		.name = __stringify(SYS_MFG_2D),
		.sta_mask = MFG_2D_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_2D_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12), /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_MFG_ASYNC] = {
		.name = __stringify(SYS_MFG_ASYNC),
		.sta_mask = MFG_ASYNC_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_MFG_ASYNC_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0, /* GENMASK(15, 12), */
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
	[SYS_USB] = {
		.name = __stringify(SYS_USB),
		.sta_mask = USB_PWR_STA_MASK,
		.ctl_addr = NULL, /* SPM_USB_PWR_CON, */
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = 0,
		.ops = &general_sys_ops,
	},
};

static struct pg_callbacks *g_pgcb;

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	struct pg_callbacks *old_pgcb = g_pgcb;

	g_pgcb = pgcb;
	return old_pgcb;
}

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

static void set_bus_protect(int en, uint32_t mask, unsigned long expired)
{
	if (!mask)
		return;

	if (en) {
		clk_setl(mask, INFRA_TOPAXI_PROTECTEN);

#if !DUMMY_REG_TEST
		while ((clk_readl(INFRA_TOPAXI_PROTECTSTA1) & mask) != mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
#endif /* !DUMMY_REG_TEST */
	} else {
		clk_clrl(mask, INFRA_TOPAXI_PROTECTEN);

#if !DUMMY_REG_TEST
		while (clk_readl(INFRA_TOPAXI_PROTECTSTA1) & mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
#endif /* !DUMMY_REG_TEST */
	}
}

static int spm_mtcmos_power_off_general_locked(struct subsys *sys,
		int wait_power_ack, int ext_pwr_delay)
{
	unsigned long expired = jiffies + HZ / 10;
	void __iomem *ctl_addr = sys->ctl_addr;
#if !DUMMY_REG_TEST
	uint32_t sram_pdn_ack = sys->sram_pdn_ack_bits;
#endif /* !DUMMY_REG_TEST */

	pr_debug("sys: %s\n", sys->name);

	/* BUS_PROTECT */
	set_bus_protect(1, sys->bus_prot_mask, expired);

	/* SRAM_PDN */
	clk_setl(sys->sram_pdn_bits, ctl_addr);

	/* wait until SRAM_PDN_ACK all 1 */
#if !DUMMY_REG_TEST
	while (((clk_readl(ctl_addr) & sram_pdn_ack) != sram_pdn_ack)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}
#endif /* !DUMMY_REG_TEST */

	clk_setl(PWR_ISO_BIT, ctl_addr);
	clk_clrl(PWR_RST_B_BIT, ctl_addr);
	clk_setl(PWR_CLK_DIS_BIT, ctl_addr);

	clk_clrl(PWR_ON_BIT, ctl_addr);
	clk_clrl(PWR_ON_2ND_BIT, ctl_addr);

	/* extra delay after power off */
	if (ext_pwr_delay > 0)
		udelay(ext_pwr_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 0 */
#if !DUMMY_REG_TEST
		while ((clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
			|| (clk_readl(SPM_PWR_STATUS_2ND) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
#endif /* !DUMMY_REG_TEST */
	}

	return 0;
}

static int spm_mtcmos_power_on_general_locked(
		struct subsys *sys, int wait_power_ack, int ext_pwr_delay)
{
	unsigned long expired = jiffies + HZ / 10;
	void __iomem *ctl_addr = sys->ctl_addr;
#if !DUMMY_REG_TEST
	uint32_t sram_pdn_ack = sys->sram_pdn_ack_bits;
#endif /* !DUMMY_REG_TEST */

	pr_debug("sys: %s\n", sys->name);

	clk_setl(PWR_ON_BIT, ctl_addr);
	clk_setl(PWR_ON_2ND_BIT, ctl_addr);

	/* extra delay after power on */
	if (ext_pwr_delay > 0)
		udelay(ext_pwr_delay);

	if (wait_power_ack) {
		/* wait until PWR_ACK = 1 */
#if !DUMMY_REG_TEST
		while (!(clk_readl(SPM_PWR_STATUS) & sys->sta_mask)
			|| !(clk_readl(SPM_PWR_STATUS_2ND) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;
			}
		}
#endif /* !DUMMY_REG_TEST */
	}

	clk_clrl(PWR_CLK_DIS_BIT, ctl_addr);
	clk_clrl(PWR_ISO_BIT, ctl_addr);
	clk_setl(PWR_RST_B_BIT, ctl_addr);

	/* SRAM_PDN */
	clk_clrl(sys->sram_pdn_bits, ctl_addr);

	/* wait until SRAM_PDN_ACK all 0 */
#if !DUMMY_REG_TEST
	while (sram_pdn_ack && (clk_readl(ctl_addr) & sram_pdn_ack)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;
		}
	}
#endif /* !DUMMY_REG_TEST */

	/* BUS_PROTECT */
	set_bus_protect(0, sys->bus_prot_mask, expired);

	return 0;
}

static int general_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_power_on_general_locked(sys, 1, 0);
}

static int general_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_power_off_general_locked(sys, 1, 0);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(SPM_PWR_STATUS);
	unsigned int sta_s = clk_readl(SPM_PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static struct subsys_ops general_sys_ops = {
	.enable = general_sys_enable_op,
	.disable = general_sys_disable_op,
	.get_state = sys_get_state_op,
};

static int subsys_is_on(enum subsys_id id)
{
	int r;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	r = sys->ops->get_state(sys);

	pr_debug("%d: sys: %s\n", r, sys->name);

	return r;
}

static int enable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == PWR_ON) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif /* CHECK_PWR_ST */

	r = sys->ops->enable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	if (g_pgcb && g_pgcb->after_on)
		g_pgcb->after_on(id);

	return r;
}

static int disable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	BUG_ON(!sys);

	/* TODO: check all clocks related to this subsys are off */
	/* could be power off or not */

	if (g_pgcb && g_pgcb->before_off)
		g_pgcb->before_off(id);

	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == PWR_DOWN) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif /* CHECK_PWR_ST */

	r = sys->ops->disable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	return r;
}

/*
 * power_gate
 */

struct mt_power_gate {
	struct clk_hw	hw;
	struct clk	*pre_clk;
	enum subsys_id	pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

static int pg_enable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s, id: %u\n", __clk_get_name(hw->clk), pg->pd_id);

	return enable_subsys(pg->pd_id);
}

static void pg_disable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s, id: %u\n", __clk_get_name(hw->clk), pg->pd_id);

	disable_subsys(pg->pd_id);
}

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	int r;
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s, pre_clk: %s\n", __clk_get_name(hw->clk),
		pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");

	if (pg->pre_clk) {
		r = clk_prepare_enable(pg->pre_clk);
		if (r)
			return r;
	}

	return pg_enable(hw);

}

void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	pr_debug("%s, pre_clk: %s\n", __clk_get_name(hw->clk),
		pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");

	pg_disable(hw);

	if (pg->pre_clk)
		clk_disable_unprepare(pg->pre_clk);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare	= pg_prepare,
	.unprepare	= pg_unprepare,
	.is_enabled	= pg_is_enabled,
};

struct clk *mt_clk_register_power_gate(
		const char *name,
		const char *parent_name,
		struct clk *pre_clk,
		enum subsys_id pd_id)
{
	struct mt_power_gate *pg;
	struct clk *clk;
	struct clk_init_data init;

	pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;

	pg->pre_clk = pre_clk;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

#define pg_vde			"pg_vde"
#define pg_mfg			"pg_mfg"
#define pg_ven			"pg_ven"
#define pg_isp			"pg_isp"
#define pg_dis			"pg_dis"
#define pg_ven2			"pg_ven2"
#define pg_audio		"pg_audio"
#define pg_mfg_2d		"pg_mfg_2d"
#define pg_mfg_async		"pg_mfg_async"
#define pg_usb			"pg_usb"

#define mm_sel			"mm_sel"
#define vdec_sel		"vdec_sel"
#define venc_sel		"venc_sel"
#define mfg_sel			"mfg_sel"
#define aud_intbus_sel		"aud_intbus_sel"
#define venc_lt_sel		"venc_lt_sel"


struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	const char *pre_clk_name;
	enum subsys_id pd_id;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

struct mtk_power_gate scp_clks[] __initdata = {
	PGATE(SCP_SYS_VDE, pg_vde, NULL, vdec_sel, SYS_VDE),
	PGATE(SCP_SYS_MFG, pg_mfg, pg_mfg_2d, mfg_sel, SYS_MFG),
	PGATE(SCP_SYS_VEN, pg_ven, NULL, venc_sel, SYS_VEN),
	PGATE(SCP_SYS_ISP, pg_isp, NULL, NULL, SYS_ISP),
	PGATE(SCP_SYS_DIS, pg_dis, NULL, mm_sel, SYS_DIS),
	PGATE(SCP_SYS_VEN2, pg_ven2, NULL, venc_lt_sel, SYS_VEN2),
	PGATE(SCP_SYS_AUDIO, pg_audio, NULL, NULL, SYS_AUDIO),
	PGATE(SCP_SYS_MFG_2D, pg_mfg_2d, pg_mfg_async, NULL, SYS_MFG_2D),
	PGATE(SCP_SYS_MFG_ASYNC, pg_mfg_async, NULL, NULL, SYS_MFG_ASYNC),
	PGATE(SCP_SYS_USB, pg_usb, NULL, NULL, SYS_USB),
};

static void __init init_clk_scpsys(
		void __iomem *infracfg_reg,
		void __iomem *spm_reg ,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk;

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;

	syss[SYS_VDE].ctl_addr = SPM_VDE_PWR_CON;
	syss[SYS_MFG].ctl_addr = SPM_MFG_PWR_CON;
	syss[SYS_VEN].ctl_addr = SPM_VEN_PWR_CON;
	syss[SYS_ISP].ctl_addr = SPM_ISP_PWR_CON;
	syss[SYS_DIS].ctl_addr = SPM_DIS_PWR_CON;
	syss[SYS_VEN2].ctl_addr = SPM_VEN2_PWR_CON;
	syss[SYS_AUDIO].ctl_addr = SPM_AUDIO_PWR_CON;
	syss[SYS_MFG_2D].ctl_addr = SPM_MFG_2D_PWR_CON;
	syss[SYS_MFG_ASYNC].ctl_addr = SPM_MFG_ASYNC_PWR_CON;
	syss[SYS_USB].ctl_addr = SPM_USB_PWR_CON;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
				__clk_lookup(pg->pre_clk_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name, pg->parent_name,
				pre_clk, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

		pr_debug("pgate %3d: %s\n", i, pg->name);
	}
}

/*
 * device tree support
 */

/* TODO: remove this function */
static struct clk_onecell_data *alloc_clk_data(unsigned int clk_num)
{
	int i;
	struct clk_onecell_data *clk_data;

	clk_data = kzalloc(sizeof(clk_data), GFP_KERNEL);
	if (!clk_data)
		return NULL;

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return NULL;
	}

	clk_data->clk_num = clk_num;

	for (i = 0; i < clk_num; ++i)
		clk_data->clks[i] = ERR_PTR(-ENOENT);

	return clk_data;
}

/* TODO: remove this function */
static void __iomem *get_reg(struct device_node *np, int index)
{
#if DUMMY_REG_TEST
	return kzalloc(PAGE_SIZE, GFP_KERNEL);
#else
	return of_iomap(np, index);
#endif
}

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	int r;

	infracfg_reg = get_reg(node, 0);
	spm_reg = get_reg(node, 1);

	if (!infracfg_reg || !spm_reg) {
		pr_err("clk-pg-mt8173: missing reg\n");
		return;
	}

	pr_debug("sys: %s, reg: 0x%p, 0x%p\n",
		node->name, infracfg_reg, spm_reg);

	clk_data = alloc_clk_data(SCP_NR_CLK);

	init_clk_scpsys(infracfg_reg, spm_reg, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_pg_regs, "mediatek,mt8173-scpsys", mt_scpsys_init);

#if CLK_DEBUG

/*
 * debug / unit test
 */

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

static char last_cmd[128] = "null";

static int test_pg_dump_regs(struct seq_file *s, void *v)
{
	int i;

	for (i = 0; i < NR_SYSS; i++) {
		if (!syss[i].ctl_addr)
			continue;

		seq_printf(s, "%10s: [0x%p]: 0x%08x\n", syss[i].name,
			syss[i].ctl_addr, clk_readl(syss[i].ctl_addr));
	}

	return 0;
}

static void dump_pg_state(const char *clkname, struct seq_file *s)
{
	struct clk *c = __clk_lookup(clkname);
	struct clk *p = IS_ERR_OR_NULL(c) ? NULL : __clk_get_parent(c);

	if (IS_ERR_OR_NULL(c)) {
		seq_printf(s, "[%17s: NULL]\n", clkname);
		return;
	}

	seq_printf(s, "[%17s: %3s, %3d, %3d, %10ld, %17s]\n",
		__clk_get_name(c),
		__clk_is_enabled(c) ? "ON" : "off",
		__clk_get_prepare_count(c),
		__clk_get_enable_count(c),
		__clk_get_rate(c),
		p ? __clk_get_name(p) : "");

		clk_put(c);
}

static int test_pg_dump_state_all(struct seq_file *s, void *v)
{
	static const char * const clks[] = {
		pg_vde,
		pg_mfg,
		pg_ven,
		pg_isp,
		pg_dis,
		pg_ven2,
		pg_audio,
		pg_mfg_2d,
		pg_mfg_async,
		pg_usb,
	};

	int i;

	pr_debug("\n");
	for (i = 0; i < ARRAY_SIZE(clks); i++)
		dump_pg_state(clks[i], s);

	return 0;
}

static struct {
	const char	*name;
	struct clk	*clk;
} g_clks[] = {
	{.name = pg_vde},
	{.name = pg_ven},
	{.name = pg_mfg},
};

static int test_pg_1(struct seq_file *s, void *v)
{
	int i;

	pr_debug("\n");

	for (i = 0; i < ARRAY_SIZE(g_clks); i++) {
		g_clks[i].clk = __clk_lookup(g_clks[i].name);
		if (IS_ERR_OR_NULL(g_clks[i].clk)) {
			seq_printf(s, "clk_get(%s): NULL\n",
				g_clks[i].name);
			continue;
		}

		clk_prepare_enable(g_clks[i].clk);
		seq_printf(s, "clk_prepare_enable(%s)\n",
			__clk_get_name(g_clks[i].clk));
	}

	return 0;
}

static int test_pg_2(struct seq_file *s, void *v)
{
	int i;

	pr_debug("\n");

	for (i = 0; i < ARRAY_SIZE(g_clks); i++) {
		if (IS_ERR_OR_NULL(g_clks[i].clk)) {
			seq_printf(s, "(%s).clk: NULL\n",
				g_clks[i].name);
			continue;
		}

		seq_printf(s, "clk_disable_unprepare(%s)\n",
			__clk_get_name(g_clks[i].clk));
		clk_disable_unprepare(g_clks[i].clk);
		clk_put(g_clks[i].clk);
	}

	return 0;
}

static int test_pg_show(struct seq_file *s, void *v)
{
	static const struct {
		int (*fn)(struct seq_file *, void *);
		const char	*cmd;
	} cmds[] = {
		{.cmd = "dump_regs",	.fn = test_pg_dump_regs},
		{.cmd = "dump_state",	.fn = test_pg_dump_state_all},
		{.cmd = "1",		.fn = test_pg_1},
		{.cmd = "2",		.fn = test_pg_2},
	};

	int i;

	pr_debug("last_cmd: %s\n", last_cmd);

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		if (strcmp(cmds[i].cmd, last_cmd) == 0)
			return cmds[i].fn(s, v);
	}

	return 0;
}

static int test_pg_open(struct inode *inode, struct file *file)
{
	return single_open(file, test_pg_show, NULL);
}

static ssize_t test_pg_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *data)
{
	char desc[sizeof(last_cmd)];
	int len = 0;

	pr_debug("count: %zu\n", count);
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	strcpy(last_cmd, desc);
	if (last_cmd[len - 1] == '\n')
		last_cmd[len - 1] = 0;

	return count;
}

static const struct file_operations test_pg_fops = {
	.owner		= THIS_MODULE,
	.open		= test_pg_open,
	.read		= seq_read,
	.write		= test_pg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init debug_init(void)
{
	static int init;
	struct proc_dir_entry *entry;

	pr_debug("init: %d\n", init);

	if (init)
		return 0;

	++init;

	entry = proc_create("test_pg", 0, 0, &test_pg_fops);
	if (!entry)
		return -ENOMEM;

	++init;
	return 0;
}

static void __exit debug_exit(void)
{
	remove_proc_entry("test_pg", NULL);
}

module_init(debug_init);
module_exit(debug_exit);

#endif /* CLK_DEBUG */
