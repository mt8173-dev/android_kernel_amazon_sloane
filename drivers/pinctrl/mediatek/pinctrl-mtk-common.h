/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
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

#ifndef __PINCTRL_MTK_COMMON_H
#define __PINCTRL_MTK_COMMON_H

#include <linux/pinctrl/pinctrl.h>
#include <linux/spinlock.h>


#define MTK_NO_EINT_SUPPORT    255
#define MTK_CHIP_TYPE_BASE     0
#define MTK_CHIP_TYPE_PMIC     1

struct mtk_desc_function {
	const char *name;
	unsigned char muxval;
	unsigned char irqnum;
};

struct mtk_desc_pin {
	struct pinctrl_pin_desc	pin;
	const char *chip;
	struct mtk_desc_function	*functions;
};

#define MTK_PIN(_pin, _pad, _chip, ...)				\
	{							\
		.pin = _pin,					\
		.chip = _chip,					\
		.functions = (struct mtk_desc_function[]){	\
			__VA_ARGS__, { } },			\
	}

#define MTK_FUNCTION(_val, _name)				\
	{							\
		.name = _name,					\
		.muxval = _val,					\
		.irqnum = MTK_NO_EINT_SUPPORT,				\
	}

#define MTK_FUNCTION_IRQ(_val, _name, _irq)			\
	{							\
		.name = _name,					\
		.muxval = _val,					\
		.irqnum = _irq,					\
	}

#define SET_ADDR(x, y)  (x + (y->devdata->port_align))
#define CLR_ADDR(x, y)  (x + (y->devdata->port_align << 1))

struct mtk_pinctrl_group {
	const char	*name;
	unsigned long	config;
	unsigned	pin;
};

/**
 * struct mtk_drv_group_desc - Provide driving group data.
 * @max_drv: The maximum current of this group.
 * @min_drv: The minimum current of this group.
 * @low_bit: The lowest bit of this group.
 * @high_bit: The highest bit of this group.
 * @step: The step current of this group.
 */
struct mtk_drv_group_desc {
	unsigned char min_drv;
	unsigned char max_drv;
	unsigned char low_bit;
	unsigned char high_bit;
	unsigned char step;
};

#define MTK_DRV_GRP(_min, _max, _low, _high, _step)	\
	{	\
		.min_drv = _min,	\
		.max_drv = _max,	\
		.low_bit = _low,	\
		.high_bit = _high,	\
		.step = _step,		\
	}

/**
 * struct mtk_pin_drv_grp - Provide each pin driving info.
 * @pin: The pin number.
 * @offset: The offset of driving register for this pin.
 * @bit: The bit of driving register for this pin.
 * @grp: The group for this pin belongs to.
 */
struct mtk_pin_drv_grp {
	unsigned int pin;
	unsigned int offset;
	unsigned char bit;
	unsigned char grp;
};

#define MTK_PIN_DRV_GRP(_pin, _offset, _bit, _grp)	\
	{	\
		.pin = _pin,	\
		.offset = _offset,	\
		.bit = _bit,	\
		.grp = _grp,	\
	}

/**
 * struct mtk_pin_ies_smt_set - For special pins' ies and smt setting.
 * @start: The start pin number of those special pins.
 * @end: The end pin number of those special pins.
 * @offset: The offset of special setting register.
 * @bit: The bit of special setting register.
 */
struct mtk_pin_ies_smt_set {
	unsigned int start;
	unsigned int end;
	unsigned int offset;
	unsigned char bit;
};

#define MTK_PIN_IES_SMT_SET(_start, _end, _offset, _bit)	\
	{	\
		.start = _start,	\
		.end = _end,	\
		.bit = _bit,	\
		.offset = _offset,	\
	}

/**
 * struct mtk_pin_spec_pupd_set - For special pins' pull up/down setting.
 * @pin: The pin number.
 * @offset: The offset of special pull up/down setting register.
 * @pupd_bit: The pull up/down bit in this register.
 * @r0_bit: The r0 bit of pull resistor.
 * @r1_bit: The r1 bit of pull resistor.
 */
struct mtk_pin_spec_pupd_set {
	unsigned int pin;
	unsigned int offset;
	unsigned char pupd_bit;
	unsigned char r1_bit;
	unsigned char r0_bit;
};

#define MTK_PIN_PUPD_SPEC(_pin, _offset, _pupd, _r1, _r0)	\
	{	\
		.pin = _pin,	\
		.offset = _offset,	\
		.pupd_bit = _pupd,	\
		.r1_bit = _r1,		\
		.r0_bit = _r0,		\
	}

/**
 * struct mtk_pinctrl_devdata - Provide HW GPIO related data.
 * @pins: An array describing all pins the pin controller affects.
 * @grp_desc: The driving group info.
 * @pin_drv_grp: The driving group for all pins.
 * @ies_smt_pins: The list of special pins for ies and smt setting,
 *  due to some pins are irregular, so need to create a separate table.
 * @pupu_spec_pins: The list of special pins for pull up/down setting,
 *  some pins's pull setting are very different, they have separate pull
 *  up/down bit, R0 and R1 resistor bit.
 *
 *  irregular, so need to create a separate table.
 * @npins: The number of entries in @pins.
 * @dir_offset: The direction register offset.
 * @pullen_offset: The pull-up/pull-down enable register offset.
 * @pinmux_offset: The pinmux register offset.
 *
 * @type1_start: Some chips have two base addresses for pull select register,
 *  that means some pins use the first address and others use the second. This
 *  member record the start of pin number to use the second address.
 * @type1_end: The end of pin number to use the second address.
 *
 * @port_shf: The shift between two registers.
 * @port_mask: The mask of register.
 * @port_align: Provide clear register and set register step.
 */
struct mtk_pinctrl_devdata {
	const struct mtk_desc_pin	*pins;
	unsigned int	base;
	unsigned int	npins;
	const struct mtk_drv_group_desc	*grp_desc;
	unsigned int	n_grp_cls;
	const struct mtk_pin_drv_grp	*pin_drv_grp;
	unsigned int	n_pin_drv_grps;
	const struct mtk_pin_ies_smt_set	*ies_smt_pins;
	unsigned int	n_ies_smt_pins;
	const struct mtk_pin_spec_pupd_set	*pupu_spec_pins;
	unsigned int	n_pupu_spec_pins;
	unsigned int	dir_offset;
	unsigned int	ies_offset;
	unsigned int	smt_offset;
	unsigned int	pullen_offset;
	unsigned int	pullsel_offset;
	unsigned int	drv_offset;
	unsigned int	invser_offset;
	unsigned int	dout_offset;
	unsigned int	din_offset;
	unsigned int	pinmux_offset;
	unsigned short	type1_start;
	unsigned short	type1_end;
	unsigned char	port_shf;
	unsigned char	port_mask;
	unsigned char	port_align;
	unsigned char	chip_type;
};

struct mtk_pinctrl {
	void __iomem            *membase1;
	void __iomem            *membase2;
	struct device           *dev;
	struct gpio_chip	*chip;
	spinlock_t              lock;
	struct mtk_pinctrl_group	*groups;
	unsigned			ngroups;
	const char          **grp_names;
	struct pinctrl_dev      *pctl_dev;
	const struct mtk_pinctrl_devdata  *devdata;
};

int mtk_pctrl_init(struct platform_device *pdev,
		const struct mtk_pinctrl_devdata *data);

int mtk_pctrl_remove(struct platform_device *pdev);

#endif /* __PINCTRL_MTK_COMMON_H */
