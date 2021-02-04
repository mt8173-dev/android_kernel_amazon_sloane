/* rc-dvb0700-big.c - Keytable for devices in dvb0700
 *
 * Copyright (c) 2010 by Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * TODO: This table is a real mess, as it merges RC codes from several
 * devices into a big table. It also has both RC-5 and NEC codes inside.
 * It should be broken into small tables, and the protocols should properly
 * be indentificated.
 *
 * The table were imported from dib0700_devices.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <mach/mt_gpio.h>

#include "mtk_ir_common.h"
#include "mtk_ir_regs.h"
#include "mtk_ir_core.h"



unsigned long IRRX_BASE_PHY = 0;	/* this value is getted from device tree */
unsigned long IRRX_BASE_PHY_END = 0;	/*  */
unsigned long IRRX_BASE_VIRTUAL = 0;	/* this value is ioremap from device tree's IRRX_BASE_PHY */
unsigned long IRRX_BASE_VIRTUAL_END = 0;	/* this value is ioremap from device tree's IRRX_BASE_PHY */

static struct clk *mtk_ir_clk;
static struct clk *clk_irda;	/* 8173 default close irda clock */
static struct regulator *mtk_ir_power;	/* 8173 default close irda clock */



#define IRRX_CLK_EN  0x10003014	/* // set bit[1] to  1 to enable  clock */
#define IRRX_CLK_STAT 0x1000301C	/* bit[1]=0: clock is enable , bit[1] = 1 : clock is disable */
#define IRRX_CLK_DIS   0x1000300C
/* set bit[1] to  1 to disable  clock , if irrx is be closed ,irrx can not send interrupt and clear data*/

#define IRDA_CLK_EN  0x10003010	/*  set bit[18] to  1 to enable  clock */
#define IRDA_CLK_STAT 0x10003018	/* bit[18]=0: clock is enable , bit[18] = 1 : clock is disable */
#define IRDA_CLK_DIS   0x10003008
/* set bit[18] to  1 to disable  clock, be careful ,if irda is be closed ,irrx can not work */

#define IRRX_MODULE_RST 0x10003000	/* set bit[4] = 1 to reset ir module register to default value */



int mtk_ir_hal_clock_probe(void)
{
	const char *clk_name = NULL;
	int ret = 0;
	struct platform_device *pdev = mtk_rc_core.dev_parent;
	ASSERT(pdev != NULL);

	ret = of_property_read_string_index(pdev->dev.of_node, "clock-names", 1, &clk_name);	/* get irda clk */
	BUG_ON(ret);

	IR_LOG_ALWAYS("clock-names = %s\n", clk_name);
	clk_irda = devm_clk_get(&(pdev->dev), clk_name);
	BUG_ON(IS_ERR(clk_irda));

	mt_ir_reg_remap(IRDA_CLK_STAT, 0, false);

	ret = of_property_read_string_index(pdev->dev.of_node, "clock-names", 0, &clk_name);	/* get irrx clk */
	BUG_ON(ret);

	IR_LOG_ALWAYS("clock-names = %s\n", clk_name);
	mtk_ir_clk = devm_clk_get(&(pdev->dev), clk_name);
	BUG_ON(IS_ERR(mtk_ir_clk));
	return 0;

}


void mt_ir_reg_remap(unsigned long pyaddress, unsigned long value, bool bWrite)
{
	void __iomem *reg_remap = NULL;
	reg_remap = ioremap(pyaddress, 0x4);	/* set bit[1] to  1 to enable  clock */
	if (reg_remap) {

		IR_LOG_ALWAYS("reg_remap (0x%lx) value=(0x%x)\n", (unsigned long)pyaddress,
			      REGISTER_READ((unsigned long)reg_remap));
		if (bWrite) {
			REGISTER_WRITE(reg_remap, value);
			IR_LOG_ALWAYS("reg_remap (0x%lx) value=(0x%x)\n", (unsigned long)pyaddress,
				      REGISTER_READ((unsigned long)reg_remap));
		}
		iounmap(reg_remap);
	} else {
		IR_LOG_ALWAYS("reg_remap (0x%lx) error\n", (unsigned long)pyaddress);
	}
}


/* enable or disable clock
if disable ,irrx module can not send interrupt and can not clear data.

*/
int mt_ir_core_clock_enable(bool bEnable)
{

	if (mtk_ir_clk == NULL) {
		IR_LOG_ALWAYS("pls first probe irrx clk\n");
		return -1;
	}

	if (bEnable) {
		IR_LOG_ALWAYS("enable IRRX clock\n");
		clk_prepare(mtk_ir_clk);
		clk_enable(mtk_ir_clk);

	} else {
		IR_LOG_ALWAYS("disable IRRX clock\n");
		clk_disable(mtk_ir_clk);
		clk_unprepare(mtk_ir_clk);
	}
	mt_ir_reg_remap(IRRX_CLK_STAT, 0, false);
	return 0;
}

/* enable or disable irda clock if enable ,irrx can work ,if disable , */
/*irrx can not work and register can not be setted*/
int mt_ir_core_irda_clock_enable(bool bEnable)
{

	if (clk_irda == NULL) {
		IR_LOG_ALWAYS("pls first probe irda clk\n");
		return -1;
	}

	if (bEnable) {
		IR_LOG_ALWAYS("enable irda clock\n");
		clk_prepare(clk_irda);
		clk_enable(clk_irda);

	} else {
		IR_LOG_ALWAYS("disable irda clock\n");
		clk_disable(clk_irda);
		clk_unprepare(clk_irda);
	}
	mt_ir_reg_remap(IRDA_CLK_STAT, 0, false);
	return 0;
}

/*
some pcb's irrx power pin use pmic to control
*/
int mt_ir_hal_power_probe(void)
{
	int ret = 0;
	struct platform_device *pdev = mtk_rc_core.dev_parent;
	ASSERT(pdev != NULL);
	mtk_ir_power = devm_regulator_get(&(pdev->dev), "irrx-power");
	if (IS_ERR(mtk_ir_power)) {
		mtk_ir_power = NULL;
		IR_LOG_ALWAYS("devm_regulator_get fail  pls check dts fail!!!\n");
		return ret;
	}

	ret = regulator_set_voltage(mtk_ir_power, 3300000, 3300000);
	if (ret != 0) {
		IR_LOG_ALWAYS("regulator_set_voltage fail  pls check !\n");
		return ret;
	} else {
		IR_LOG_ALWAYS("irrx set voltage = 3300000uV\n");
	}
	ret = regulator_get_voltage(mtk_ir_power);
	IR_LOG_ALWAYS("irrx init voltage = %d uV\n", ret);
	return ret;
}

/* enable or disable irrx pmu power */

int mt_ir_hal_power_enable(bool bEnable)
{
	int ret = 0;
	if (mtk_ir_power == NULL) {
		IR_LOG_ALWAYS("pls first probe irrx pmu regulator power\n");
		return -1;
	}

	if (bEnable) {
		IR_LOG_ALWAYS("enable irrx regulator\n");
		ret = regulator_enable(mtk_ir_power);
		if (ret) {
			IR_LOG_ALWAYS("fail to enable irrx pmu regulator ret = %d V\n", ret);
			return ret;
		}
		ret = regulator_is_enabled(mtk_ir_power);
		IR_LOG_ALWAYS("regulator_is_enabled status(%d)\n", ret);
	} else {
		IR_LOG_ALWAYS("disable irrx  regulator\n");
		ret = regulator_disable(mtk_ir_power);
		if (ret) {
			IR_LOG_ALWAYS("fail to disable irrx pmu regulator ret = %d V\n", ret);
			return ret;
		}
		ret = regulator_is_enabled(mtk_ir_power);
		IR_LOG_ALWAYS("regulator_is_enabled status(%d)\n", ret);
	}

	return 0;
}



void mtk_ir_core_disable_hwirq(void)
{
	/* disable interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x0);
	dsb();

}

/*
be careful if irda clock is be closed ,irrx register can not be clear,then here will hung up,
so I add mtk_ir_core_direct_clear_irq() function
*/
void mtk_ir_core_clear_hwirq(void)
{

	u32 info = IR_READ(IRRX_COUNT_HIGH_REG);

	IR_WRITE_MASK(IRRX_IRCLR, IRRX_IRCLR_MASK, IRRX_IRCLR_OFFSET, 0x1);	/* clear irrx state machine */
	dsb();

	IR_WRITE_MASK(IRRX_IRINT_CLR, IRRX_INTCLR_MASK, IRRX_INTCLR_OFFSET, 0x1);	/* clear irrx eint stat */
	dsb();
	do {
		info = IR_READ(IRRX_COUNT_HIGH_REG);

	} while (info != 0);

	/* enable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x1);
	dsb();
}

/*
directly clear hw irq stat and state machine with out loop for whether data has been cleared

because for 8173 if irda_clock is disabled , all register are default value and they can not be writed
*/
void mtk_ir_core_direct_clear_hwirq(void)
{
	IR_WRITE_MASK(IRRX_IRCLR, IRRX_IRCLR_MASK, IRRX_IRCLR_OFFSET, 0x1);	/* clear irrx state machine */
	dsb();
	IR_WRITE_MASK(IRRX_IRINT_CLR, IRRX_INTCLR_MASK, IRRX_INTCLR_OFFSET, 0x1);	/* clear irrx eint stat */
	dsb();
	/* enable ir interrupt */
	IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK, IRRX_INTCLR_OFFSET, 0x1);
	dsb();
}

int mtk_ir_core_pin_set(void)
{
/* set multifunction pin set */
	int ret = 0;
	void __iomem *reg_gpio = NULL;
	u32 gpio_mode[2] = { 0, 0 };
	struct platform_device *pdev = mtk_rc_core.dev_parent;
	/* 8173 using new pinctrl api */
	struct pinctrl *mtk_ir_pinctrl = NULL;
	struct pinctrl_state *mtk_ir_state = NULL;
	ASSERT(pdev != NULL);
	mtk_ir_pinctrl = devm_pinctrl_get(&(pdev->dev));
	if (IS_ERR(mtk_ir_pinctrl)) {
		IR_LOG_ALWAYS("irrx pinctrl devm_pinctrl_get, please check dts file!!!!!!\n");
		return ret;
	}
	mt_ir_reg_remap(0x10005600, 1, false);
	/* box_using */
	mtk_ir_state = pinctrl_lookup_state(mtk_ir_pinctrl, "box_using");
	if (IS_ERR(mtk_ir_state)) {
		IR_LOG_ALWAYS("irrx pinctrl no box_using state, please check dts file!!!!!!\n");
		return ret;
	}
	ret = pinctrl_select_state(mtk_ir_pinctrl, mtk_ir_state);	/*select  current pin state */
	if (ret) {
		IR_LOG_ALWAYS("pinctrl_select_state fail!!! ret = %d\n", ret);
		return ret;
	}
	devm_pinctrl_put(mtk_ir_pinctrl);
	ret = of_property_read_u32_array(pdev->dev.of_node, "gpio_set", gpio_mode, 2);
	BUG_ON(ret);
	reg_gpio = ioremap(gpio_mode[0], 4);	/* here remap gpio register to get virtual address; */

	IR_LOG_ALWAYS("gpio_address = 0x%x, gpio_mode[1] = 0x%x\n", gpio_mode[0], gpio_mode[1]);
	ASSERT(reg_gpio != NULL);
	IR_LOG_ALWAYS("irrx gpio virtual address(0x%lx) = value(0x%x)\n", (unsigned long)reg_gpio,
		      REGISTER_READ(reg_gpio));
	iounmap(reg_gpio);
	return 0;
}

/*check ir's check cnt, if check cnt is full ,then here must be cleared*/
int mtk_ir_check_cnt(void)
{
	u32 chk_cnt = 0;
	chk_cnt = (IR_READ(IRRX_EXPBCNT) >> IRRX_IRCHK_CNT_OFFSET);
	chk_cnt = (chk_cnt & IRRX_IRCHK_CNT);
	if (chk_cnt == IRRX_IRCHK_CNT) {
		mtk_ir_core_direct_clear_hwirq();
		return 1;
	}
	return 0;
}

void __iomem *reg_gic = NULL;
void __iomem *reg_mtk = NULL;
unsigned long gic_address = 0;
unsigned long mtk_address = 0;
u32 bit_gic = 0;
u32 bit_mtk = 0;

void mtk_ir_check_interrupt(void)
{

	reg_gic = ioremap(0x10220000, 0x8000);
	reg_mtk = ioremap(0x10200000, 0x1000);

	if ((reg_gic == NULL) || (reg_mtk == NULL)) {
		IR_LOG_ALWAYS("return interrupt\n");
		return;
	}


	gic_address = (unsigned long)reg_gic + 0x1000 + 0xc00 + (140 / 16) * 4;
	mtk_address = (unsigned long)reg_mtk + 0x620 + ((140 - 32) >> 5) * 4;
	bit_gic = (0x2 << ((140 % 16) * 2));
	bit_mtk = (1 << ((140 - 32) & 0x1f));

	IR_LOG_ALWAYS("gic_address(0x%lx) value=(0x%x)\n", gic_address, REGISTER_READ(gic_address));
	IR_LOG_ALWAYS("bit_gic = 0x%x\n", bit_gic);
	IR_LOG_ALWAYS("REGISTER_READ(gic_address) & bit_gic = (0x%x)\n",
		      REGISTER_READ(gic_address) & bit_gic);



	IR_LOG_ALWAYS("mtk_address(0x%lx) value=(0x%x)\n", mtk_address, REGISTER_READ(mtk_address));
	IR_LOG_ALWAYS("bit_mtk = 0x%x\n", bit_mtk);
	IR_LOG_ALWAYS("REGISTER_READ(mtk_address) & bit_mtk = (0x%x)\n",
		      REGISTER_READ(mtk_address) & bit_mtk);

	iounmap(reg_gic);
	iounmap(reg_mtk);
}
