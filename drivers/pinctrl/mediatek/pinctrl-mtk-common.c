/*
 * mt65xx pinctrl driver based on Allwinner A1X pinctrl driver.
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
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <dt-bindings/pinctrl/mt65xx.h>
#include <mach/mt_pmic_wrap.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinctrl-utils.h"
#include "pinctrl-mtk-common.h"

#define APPLY_NEW_DTS_FORMAT 0

#define PINMUX_MAX_VAL 8
#define MAX_GPIO_MODE_PER_REG 5
#define GPIO_MODE_BITS        3

static const char * const mtk_gpio_functions[] = {
	"func0", "func1", "func2", "func3",
	"func4", "func5", "func6", "func7",
};

static void __iomem *mtk_get_base_addr(struct mtk_pinctrl *pctl,
		unsigned long pin)
{
	if (pin >= pctl->devdata->type1_start && pin < pctl->devdata->type1_end)
		return pctl->membase2;
	return pctl->membase1;
}

static int mtk_pctrl_write_reg(struct mtk_pinctrl *pctl,
		void __iomem *reg, unsigned int wdata)
{
	int err = 0;
	/*printk(KERN_EMERG "dandan function(%s), line(%d), chip_type (%d), reg(0x%x), wdata(0x%x)\n",
	__FUNCTION__, __LINE__, pctl->devdata->chip_type, reg, wdata);*/

	if (pctl->devdata->chip_type == MTK_CHIP_TYPE_BASE) {
		writel_relaxed(wdata, reg);
		return 0;
	}

	err = pwrap_write((unsigned long)reg, wdata);

	return err ? -EIO : 0;
}

static int mtk_pctrl_read_reg(struct mtk_pinctrl *pctl,
		void __iomem *reg, unsigned int *rdata)
{
	int err = 0;

	if (pctl->devdata->chip_type == MTK_CHIP_TYPE_BASE) {
		*rdata = readl_relaxed(reg);
		return 0;
	}

	err = pwrap_read((unsigned long)reg, rdata);
	return err ? -EIO : 0;
}

static unsigned int mtk_get_port(struct mtk_pinctrl *pctl, unsigned long pin)
{
	/* Different SoC has different mask and port shift. */
	return ((pin >> 4) & pctl->devdata->port_mask)
			<< pctl->devdata->port_shf;
}

static int mtk_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range, unsigned offset,
			bool input)
{
	unsigned int reg_addr;
	unsigned int bit;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->dir_offset;
	bit = BIT(offset & 0xf);

	if (input)
		/* Different SoC has different alignment offset. */
		reg_addr = CLR_ADDR(reg_addr, pctl);
	else
		reg_addr = SET_ADDR(reg_addr, pctl);

	mtk_pctrl_write_reg(pctl, pctl->membase1 + reg_addr, bit);
	return 0;
}

static void mtk_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	unsigned int reg_addr;
	unsigned int bit;
	struct mtk_pinctrl *pctl = dev_get_drvdata(chip->dev);

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->dout_offset;
	bit = BIT(offset & 0xf);

	if (value)
		reg_addr = SET_ADDR(reg_addr, pctl);
	else
		reg_addr = CLR_ADDR(reg_addr, pctl);

	mtk_pctrl_write_reg(pctl, pctl->membase1 + reg_addr, bit);
}

static void mtk_pconf_set_ies(struct mtk_pinctrl *pctl,
		unsigned pin, int value)
{
	unsigned int i;
	unsigned int reg_addr;
	unsigned int bit;
	bool find = false;

	if (pctl->devdata->ies_smt_pins) {
		for (i = 0; i < pctl->devdata->n_ies_smt_pins; i++) {
			if (pin >= pctl->devdata->ies_smt_pins[i].start &&
				pin <= pctl->devdata->ies_smt_pins[i].end) {
				find = true;
				break;
			}
		}

		if (find) {
			if (value)
				reg_addr = SET_ADDR(
					pctl->devdata->ies_smt_pins[i].offset,
						pctl);
			else
				reg_addr = CLR_ADDR(
					pctl->devdata->ies_smt_pins[i].offset,
						pctl);

			bit = BIT(pctl->devdata->ies_smt_pins[i].bit);
			writel_relaxed(bit, mtk_get_base_addr(pctl, pin) +
					reg_addr);
			return;
		}
	}

	bit = BIT(pin & 0xf);
	if (value)
		reg_addr = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->ies_offset, pctl);
	else
		reg_addr = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->ies_offset, pctl);

	writel_relaxed(bit, mtk_get_base_addr(pctl, pin) + reg_addr);
}

static void mtk_pconf_set_smt(struct mtk_pinctrl *pctl,
		unsigned pin, int value)
{
	unsigned int i;
	unsigned int reg_addr;
	unsigned int bit;
	bool find = false;

	if (pctl->devdata->ies_smt_pins) {
		for (i = 0; i < pctl->devdata->n_ies_smt_pins; i++) {
			if (pin >= pctl->devdata->ies_smt_pins[i].start &&
				pin <= pctl->devdata->ies_smt_pins[i].end) {
				find = true;
				break;
			}
		}

		if (find) {
			if (value)
				reg_addr = SET_ADDR(
					pctl->devdata->ies_smt_pins[i].offset,
						pctl);
			else
				reg_addr = CLR_ADDR(
					pctl->devdata->ies_smt_pins[i].offset,
						pctl);

			bit = BIT(pctl->devdata->ies_smt_pins[i].bit);
			writel_relaxed(bit,  mtk_get_base_addr(pctl, pin) +
					reg_addr);
			return;
		}
	}

	bit = BIT(pin & 0xf);

	if (value)
		reg_addr = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->smt_offset, pctl);
	else
		reg_addr = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->smt_offset, pctl);

	writel_relaxed(bit, mtk_get_base_addr(pctl, pin) + reg_addr);
}

static const struct mtk_pin_drv_grp *mtk_find_pin_drv_grp_by_pin(
		struct mtk_pinctrl *pctl,  unsigned long pin) {
	int i;

	for (i = 0; i < pctl->devdata->n_pin_drv_grps; i++) {
		const struct mtk_pin_drv_grp *pin_drv =
				pctl->devdata->pin_drv_grp + i;
		if (pin == pin_drv->pin)
			return pin_drv;
	}

	return NULL;
}

static int mtk_pconf_set_driving(struct mtk_pinctrl *pctl,
		unsigned long pin, unsigned char driving)
{
	const struct mtk_pin_drv_grp *pin_drv;
	unsigned int nib;
	unsigned long flags, val;
	unsigned int bits, port;

	if (pin >= pctl->devdata->npins)
		return -EINVAL;

	pin_drv = mtk_find_pin_drv_grp_by_pin(pctl, pin);
	if (!pin_drv || pin_drv->grp > pctl->devdata->n_grp_cls)
		return -EINVAL;

	if (driving >= pctl->devdata->grp_desc[pin_drv->grp].min_drv &&
		driving <= pctl->devdata->grp_desc[pin_drv->grp].max_drv
		&& !(driving % pctl->devdata->grp_desc[pin_drv->grp].step)) {

		nib = driving / pctl->devdata->grp_desc[pin_drv->grp].step - 1;
		bits = pctl->devdata->grp_desc[pin_drv->grp].high_bit -
			   pctl->devdata->grp_desc[pin_drv->grp].low_bit + 1;
		if (bits == 1)
			port = 0x1;
		else if (bits == 2)
			port = 0x3;
		else
			port = 0x7;

		spin_lock_irqsave(&pctl->lock, flags);
		val = readl_relaxed(mtk_get_base_addr(pctl, pin) +
				pin_drv->offset);
		val = (val & (~(port << (pin_drv->bit +
			  pctl->devdata->grp_desc[pin_drv->grp].low_bit)))) |
			  (nib << (pin_drv->bit +
			   pctl->devdata->grp_desc[pin_drv->grp].low_bit));
		writel_relaxed(val, mtk_get_base_addr(pctl, pin) +
				pin_drv->offset);
		spin_unlock_irqrestore(&pctl->lock, flags);

		return 0;
	}

	return -EINVAL;
}

static int mtk_pconf_set_pull_select(struct mtk_pinctrl *pctl,
		unsigned long pin, bool enable, bool isup, unsigned int arg)
{
	unsigned int bit, i;
	unsigned int reg_pullen, reg_pullsel;
	unsigned int bit_r0, bit_r1;
	unsigned int reg_set, reg_rst;
	bool find = false;

	if (pctl->devdata->pupu_spec_pins) {
		for (i = 0; i < pctl->devdata->n_pupu_spec_pins; i++) {
			if (pin == pctl->devdata->pupu_spec_pins[i].pin) {
				find = true;
				break;
			}
		}

		if (find) {
			if (isup)
				reg_pullsel = CLR_ADDR(
				pctl->devdata->pupu_spec_pins[i].offset, pctl);
			else
				reg_pullsel = SET_ADDR(
				pctl->devdata->pupu_spec_pins[i].offset, pctl);

			bit = BIT(pctl->devdata->pupu_spec_pins[i].pupd_bit);
			writel_relaxed(bit, mtk_get_base_addr(pctl, pin)
					+ reg_pullsel);

			bit_r0 = BIT(pctl->devdata->pupu_spec_pins[i].r0_bit);
			bit_r1 = BIT(pctl->devdata->pupu_spec_pins[i].r1_bit);
			reg_set = SET_ADDR(
				pctl->devdata->pupu_spec_pins[i].offset, pctl);
			reg_rst = CLR_ADDR(
				pctl->devdata->pupu_spec_pins[i].offset, pctl);

			switch (arg) {
			case MTK_PUPD_SET_R1R0_00:
				writel_relaxed(bit_r0,
					mtk_get_base_addr(pctl, pin) + reg_rst);
				writel_relaxed(bit_r1,
					mtk_get_base_addr(pctl, pin) + reg_rst);
				break;
			case MTK_PUPD_SET_R1R0_01:
				writel_relaxed(bit_r0,
					mtk_get_base_addr(pctl, pin) + reg_set);
				writel_relaxed(bit_r1,
					mtk_get_base_addr(pctl, pin) + reg_rst);
				break;
			case MTK_PUPD_SET_R1R0_10:
				writel_relaxed(bit_r0,
					mtk_get_base_addr(pctl, pin) + reg_rst);
				writel_relaxed(bit_r1,
					mtk_get_base_addr(pctl, pin) + reg_set);
				break;
			case MTK_PUPD_SET_R1R0_11:
				writel_relaxed(bit_r0,
					mtk_get_base_addr(pctl, pin) + reg_set);
				writel_relaxed(bit_r1,
					mtk_get_base_addr(pctl, pin) + reg_set);
				break;
			}

			return 0;
		}
	}

	bit = BIT(pin & 0xf);
	if (enable)
		reg_pullen = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullen_offset, pctl);
	else
		reg_pullen = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullen_offset, pctl);

	if (isup)
		reg_pullsel = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullsel_offset, pctl);
	else
		reg_pullsel = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullsel_offset, pctl);

	mtk_pctrl_write_reg(pctl, mtk_get_base_addr(pctl, pin) +
			reg_pullen, bit);
	mtk_pctrl_write_reg(pctl, mtk_get_base_addr(pctl, pin) +
			reg_pullsel, bit);

	return 0;
}

static int mtk_pconf_parse_conf(struct pinctrl_dev *pctldev,
		unsigned long pin, enum pin_config_param param,
		enum pin_config_param arg)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		mtk_pconf_set_pull_select(pctl, pin, false, false, arg);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mtk_pconf_set_pull_select(pctl, pin, true, true, arg);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mtk_pconf_set_pull_select(pctl, pin, true, false, arg);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		mtk_pconf_set_ies(pctl, pin, arg);
		break;
	case PIN_CONFIG_OUTPUT:
		mtk_gpio_set(pctl->chip, pin, arg);
		mtk_pmx_gpio_set_direction(pctldev, NULL, pin, false);
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		mtk_pconf_set_smt(pctl, pin, arg);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mtk_pconf_set_driving(pctl, pin, arg);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_pconf_group_get(struct pinctrl_dev *pctldev,
				 unsigned group,
				 unsigned long *config)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*config = pctl->groups[group].config;

	return 0;
}

static int mtk_pconf_group_set(struct pinctrl_dev *pctldev, unsigned group,
				 unsigned long *configs, unsigned num_configs)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *g = &pctl->groups[group];
	int i;

	for (i = 0; i < num_configs; i++) {
		mtk_pconf_parse_conf(pctldev, g->pin,
				pinconf_to_config_param(configs[i]),
				pinconf_to_config_argument(configs[i]));

		g->config = configs[i];
	}

	return 0;
}

static const struct pinconf_ops mtk_pconf_ops = {
	.pin_config_group_get	= mtk_pconf_group_get,
	.pin_config_group_set	= mtk_pconf_group_set,
};

static struct mtk_pinctrl_group *
mtk_pctrl_find_group_by_pin(struct mtk_pinctrl *pctl, u32 pin)
{
	int i;

	for (i = 0; i < pctl->ngroups; i++) {
		struct mtk_pinctrl_group *grp = pctl->groups + i;

		if (grp->pin == pin)
			return grp;
	}

	return NULL;
}

static bool mtk_pctrl_is_function_valid(struct mtk_pinctrl *pctl,
		u32 pin_num, u32 fnum)
{
	int i;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;

		if (pin->pin.number == pin_num) {
			struct mtk_desc_function *func = pin->functions + fnum;

			if (func->name)
				return true;

			break;
		}
	}

	return false;
}
#if APPLY_NEW_DTS_FORMAT
static int mtk_pctrl_dt_node_to_map_func(struct mtk_pinctrl *pctl,
		u32 pin, u32 fnum, struct mtk_pinctrl_group *grp,
		struct pinctrl_map **map, unsigned *reserved_maps,
		unsigned *num_maps)
{
	bool ret;

	if (*num_maps == *reserved_maps)
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = grp->name;

	ret = mtk_pctrl_is_function_valid(pctl, pin, fnum);
	if (!ret) {
		dev_err(pctl->dev, "invalid function %d on pin %d .\n",
				fnum, pin);
		return -EINVAL;
	}

	(*map)[*num_maps].data.mux.function = mtk_gpio_functions[fnum];
	(*num_maps)++;

	return 0;
}
static int mtk_pctrl_dt_subnode_to_map(struct pinctrl_dev *pctldev,
				      struct device_node *node,
				      struct pinctrl_map **map,
				      unsigned *reserved_maps,
				      unsigned *num_maps)
{
	struct property *pins;
	u32 pinfunc, pin, func;
	int num_pins, num_funcs, maps_per_pin;
	unsigned long *configs;
	unsigned int num_configs;
	bool has_config = 0;
	int i, err;
	unsigned reserve = 0;
	struct mtk_pinctrl_group *grp;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	pins = of_find_property(node, "pins", NULL);
	if (!pins) {
		dev_err(pctl->dev, "missing pins property in node %s .\n",
				node->name);
		return -EINVAL;
	}

	err = pinconf_generic_parse_dt_config(node, &configs, &num_configs);
	if (num_configs)
		has_config = 1;

	num_pins = pins->length / sizeof(u32);
	num_funcs = num_pins;
	maps_per_pin = 0;
	if (num_funcs)
		maps_per_pin++;
	if (has_config && num_pins >= 1)
		maps_per_pin++;

	if (!num_pins || !maps_per_pin)
		return -EINVAL;

	reserve = num_pins * maps_per_pin;

	err = pinctrl_utils_reserve_map(pctldev, map,
			reserved_maps, num_maps, reserve);
	if (err < 0)
		goto fail;

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(node, "pins",
				i, &pinfunc);
		if (err)
			goto fail;

		pin = MTK_GET_PIN_NO(pinfunc);
		func = MTK_GET_PIN_FUNC(pinfunc);

		if (pin >= pctl->devdata->npins ||
				func >= ARRAY_SIZE(mtk_gpio_functions)) {
			dev_err(pctl->dev, "invalid pins value.\n");
			err = -EINVAL;
			goto fail;
		}

		grp = mtk_pctrl_find_group_by_pin(pctl, pin);
		if (!grp) {
			dev_err(pctl->dev, "unable to match pin %d to group\n",
					pin);
			return -EINVAL;
		}

		err = mtk_pctrl_dt_node_to_map_func(pctl, pin, func, grp, map,
				reserved_maps, num_maps);
		if (err < 0)
			goto fail;

		if (has_config) {
			err = pinctrl_utils_add_map_configs(pctldev, map,
					reserved_maps, num_maps, grp->name,
					configs, num_configs,
					PIN_MAP_TYPE_CONFIGS_GROUP);
			if (err < 0)
				goto fail;
		}
	}

	return 0;

fail:
	return err;
}
static int mtk_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				 struct device_node *np_config,
				 struct pinctrl_map **map, unsigned *num_maps)
{
	struct device_node *np;
	unsigned reserved_maps;
	int ret;

	*map = NULL;
	*num_maps = 0;
	reserved_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = mtk_pctrl_dt_subnode_to_map(pctldev, np, map,
				&reserved_maps, num_maps);
		if (ret < 0) {
			pinctrl_utils_dt_free_map(pctldev, *map, *num_maps);
			return ret;
		}
	}

	return 0;
}

#else
static int mtk_pctrl_dt_node_to_map_func(struct mtk_pinctrl *pctl, u32 pin,
		u32 fnum, struct pinctrl_map **maps)
{
	bool ret;
	struct mtk_pinctrl_group *grp;
	struct pinctrl_map *map = *maps;

	grp = mtk_pctrl_find_group_by_pin(pctl, pin);
	if (!grp) {
		dev_err(pctl->dev, "unable to match pin %d to group\n", pin);
		return -EINVAL;
	}

	map->type = PIN_MAP_TYPE_MUX_GROUP;
	map->data.mux.group = grp->name;

	ret = mtk_pctrl_is_function_valid(pctl, pin, fnum);
	if (!ret) {
		dev_err(pctl->dev, "invalid mediatek,function %d on pin %d .\n",
				fnum, pin);
		return -EINVAL;
	}

	map->data.mux.function = mtk_gpio_functions[fnum];
	(*maps)++;

	return 0;
}

static int mtk_pctrl_dt_node_to_map_config(struct mtk_pinctrl *pctl, u32 pin,
		unsigned long *configs, unsigned num_configs,
		struct pinctrl_map **maps)
{
	struct mtk_pinctrl_group *grp;
	unsigned long *cfgs;
	struct pinctrl_map *map = *maps;

	cfgs = kmemdup(configs, num_configs * sizeof(*cfgs),
		       GFP_KERNEL);
	if (cfgs == NULL)
		return -ENOMEM;

	grp = mtk_pctrl_find_group_by_pin(pctl, pin);
	if (!grp) {
		dev_err(pctl->dev, "unable to match pin %d to group\n", pin);
		return -EINVAL;
	}

	map->type = PIN_MAP_TYPE_CONFIGS_GROUP;
	map->data.configs.group_or_pin = grp->name;
	map->data.configs.configs = cfgs;
	map->data.configs.num_configs = num_configs;
	(*maps)++;

	return 0;
}

static void mtk_pctrl_dt_free_map(struct pinctrl_dev *pctldev,
				    struct pinctrl_map *map,
				    unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++) {
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);
	}

	kfree(map);
}

static int mtk_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				      struct device_node *node,
				      struct pinctrl_map **map,
				      unsigned *num_maps)
{
	struct pinctrl_map *maps, *cur_map;
	u32 pinfunc, pin, func;
	unsigned long *configs;
	unsigned int num_configs;
	int i, err, size;
	const __be32 *list;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*map = NULL;
	*num_maps = 0;

	list = of_get_property(node, "mediatek,pins", &size);
	size /= sizeof(*list);
	if (!size || size % 2) {
		dev_err(pctl->dev, "wrong pins number or pins and configs should be by 2\n");
		return -EINVAL;
	}

	cur_map = maps = kcalloc(1, size * sizeof(*maps), GFP_KERNEL);
	if (!maps)
		return -ENOMEM;

	for (i = 0; i < size; i += 2) {
		const __be32 *phandle;
		struct device_node *np_config;

		pinfunc = be32_to_cpu(*list++);
		pin = MTK_GET_PIN_NO(pinfunc);
		func = MTK_GET_PIN_FUNC(pinfunc);

		if (pin >= pctl->devdata->npins ||
				func >= ARRAY_SIZE(mtk_gpio_functions)) {
			dev_err(pctl->dev, "invalid mediatek,pinfunc value.\n");
			err = -EINVAL;
			goto fail;
		}

		err = mtk_pctrl_dt_node_to_map_func(pctl, pin, func, &cur_map);
		if (err)
			goto fail;

		phandle = list++;
		if (!phandle)
			return -EINVAL;

		np_config = of_find_node_by_phandle(be32_to_cpup(phandle));
		err = pinconf_generic_parse_dt_config(np_config,
				&configs, &num_configs);
		if (err)
			return err;

		err = mtk_pctrl_dt_node_to_map_config(pctl, pin,
				configs, num_configs, &cur_map);
		if (err)
			goto fail;
	}

	*map = maps;
	*num_maps = size;
	return 0;

fail:
	mtk_pctrl_dt_free_map(pctldev, maps, size);
	return err;
}
#endif
static int mtk_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *mtk_pctrl_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned group)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[group].name;
}

static int mtk_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned group,
				      const unsigned **pins,
				      unsigned *num_pins)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned *)&pctl->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static const struct pinctrl_ops mtk_pctrl_ops = {
	.dt_node_to_map		= mtk_pctrl_dt_node_to_map,
#if APPLY_NEW_DTS_FORMAT
	.dt_free_map		= pinctrl_utils_dt_free_map,
#else
	.dt_free_map		= mtk_pctrl_dt_free_map,
#endif
	.get_groups_count	= mtk_pctrl_get_groups_count,
	.get_group_name		= mtk_pctrl_get_group_name,
	.get_group_pins		= mtk_pctrl_get_group_pins,
};

static int mtk_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(mtk_gpio_functions);
}

static const char *mtk_pmx_get_func_name(struct pinctrl_dev *pctldev,
					   unsigned selector)
{
	return mtk_gpio_functions[selector];
}

static int mtk_pmx_get_func_groups(struct pinctrl_dev *pctldev,
				     unsigned function,
				     const char * const **groups,
				     unsigned * const num_groups)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->grp_names;
	*num_groups = pctl->ngroups;

	return 0;
}

static int mtk_pmx_set_mode(struct pinctrl_dev *pctldev,
		unsigned long pin, unsigned long mode)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int val;
	unsigned long flags;
	unsigned int mask = (1L << GPIO_MODE_BITS) - 1;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	reg_addr = ((pin / MAX_GPIO_MODE_PER_REG) << pctl->devdata->port_shf)
			+ pctl->devdata->pinmux_offset;

	spin_lock_irqsave(&pctl->lock, flags);
	mtk_pctrl_read_reg(pctl, pctl->membase1 + reg_addr, &val);
	bit = pin % MAX_GPIO_MODE_PER_REG;
	val &= ~(mask << (GPIO_MODE_BITS * bit));
	val |= (mode << (GPIO_MODE_BITS * bit));
	mtk_pctrl_write_reg(pctl, pctl->membase1 + reg_addr, val);
	spin_unlock_irqrestore(&pctl->lock, flags);
	return 0;
}

static struct mtk_desc_function *
mtk_pctrl_desc_find_function_by_number(struct mtk_pinctrl *pctl,
					 const char *pin_name,
					 unsigned number)
{
	int i;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;

		if (!strcmp(pin->pin.name, pin_name)) {
			struct mtk_desc_function *func = pin->functions;

			return func + number;
		}
	}

	return NULL;
}

static struct mtk_desc_function *
mtk_pctrl_desc_find_irq_function_from_name(struct mtk_pinctrl *pctl,
					 const char *pin_name)
{
	int i, j;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;

		if (!strcmp(pin->pin.name, pin_name)) {
			struct mtk_desc_function *func = pin->functions;

			for (j = 0; j < PINMUX_MAX_VAL; j++) {
				if (func->irqnum != MTK_NO_EINT_SUPPORT)
					return func;

				func++;
			}
		}
	}

	return NULL;
}

static int mtk_pmx_set_mux(struct pinctrl_dev *pctldev,
			    unsigned function,
			    unsigned group)
{
	bool ret;
	struct mtk_desc_function *desc;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *g = pctl->groups + group;

	ret = mtk_pctrl_is_function_valid(pctl, g->pin, function);
	if (!ret) {
		dev_err(pctl->dev, "invaild function %d on group %d .\n",
				function, group);
		return -EINVAL;
	}

	desc = mtk_pctrl_desc_find_function_by_number(pctl, g->name, function);
	if (!desc)
		return -EINVAL;
	mtk_pmx_set_mode(pctldev, g->pin, desc->muxval);
	return 0;
}

static const struct pinmux_ops mtk_pmx_ops = {
	.get_functions_count	= mtk_pmx_get_funcs_cnt,
	.get_function_name	= mtk_pmx_get_func_name,
	.get_function_groups	= mtk_pmx_get_func_groups,
	.enable			= mtk_pmx_set_mux,
	.gpio_set_direction	= mtk_pmx_gpio_set_direction,
};

static int mtk_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return pinctrl_request_gpio(chip->base + offset);
}

static void mtk_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	pinctrl_free_gpio(chip->base + offset);
}

static int mtk_gpio_direction_input(struct gpio_chip *chip,
					unsigned offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int mtk_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	mtk_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int mtk_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;

	struct mtk_pinctrl *pctl = dev_get_drvdata(chip->dev);

	reg_addr =  mtk_get_port(pctl, offset) + pctl->devdata->dir_offset;
	bit = BIT(offset & 0xf);
	mtk_pctrl_read_reg(pctl, pctl->membase1 + reg_addr, &read_val);
	return !!(read_val & bit);
}

static int mtk_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mtk_pinctrl *pctl = dev_get_drvdata(chip->dev);

	if (mtk_gpio_get_direction(chip, offset))
		reg_addr = mtk_get_port(pctl, offset) +
			pctl->devdata->dout_offset;
	else
		reg_addr = mtk_get_port(pctl, offset) +
			pctl->devdata->din_offset;

	bit = BIT(offset & 0xf);
	mtk_pctrl_read_reg(pctl, pctl->membase1 + reg_addr, &read_val);
	return !!(read_val & bit);
}

static int mtk_gpio_of_xlate(struct gpio_chip *gc,
				const struct of_phandle_args *gpiospec,
				u32 *flags)
{
	int pin;

	pin = gpiospec->args[0];

	if (pin > (gc->base + gc->ngpio))
		return -EINVAL;

	if (flags)
		*flags = gpiospec->args[1];

	return pin;
}

static int mtk_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(chip->dev);
	struct mtk_pinctrl_group *g = pctl->groups + offset;
	struct mtk_desc_function *desc =
			mtk_pctrl_desc_find_irq_function_from_name(
					pctl, g->name);
	if (!desc)
		return -EINVAL;

	mtk_pmx_set_mode(pctl->pctl_dev, g->pin, desc->muxval);
	return desc->irqnum;
}

static struct gpio_chip mtk_gpio_chip = {
	.owner			= THIS_MODULE,
	.request		= mtk_gpio_request,
	.free			= mtk_gpio_free,
	.direction_input	= mtk_gpio_direction_input,
	.direction_output	= mtk_gpio_direction_output,
	.get			= mtk_gpio_get,
	.set			= mtk_gpio_set,
	.of_xlate		= mtk_gpio_of_xlate,
	.to_irq			= mtk_gpio_to_irq,
	.of_gpio_n_cells	= 2,
};

static void mt_gpio_chip_set(struct gpio_chip *out_gc)
{
	struct gpio_chip *gc = &mtk_gpio_chip;
	out_gc->owner = gc->owner;
	out_gc->request = gc->request;
	out_gc->free = gc->free;
	out_gc->direction_input = gc->direction_input;
	out_gc->direction_output = gc->direction_output;
	out_gc->get = gc->get;
	out_gc->set = gc->set;
	out_gc->of_xlate = gc->of_xlate;
	out_gc->to_irq = gc->to_irq;
	out_gc->of_gpio_n_cells = gc->of_gpio_n_cells;
}

static int mtk_pctrl_build_state(struct platform_device *pdev)
{
	struct mtk_pinctrl *pctl = platform_get_drvdata(pdev);
	int i;

	pctl->ngroups = pctl->devdata->npins;

	/* Allocate groups */
	pctl->groups = devm_kzalloc(&pdev->dev,
				    pctl->ngroups * sizeof(*pctl->groups),
				    GFP_KERNEL);
	if (!pctl->groups)
		return -ENOMEM;

	/* We assume that one pin is one group, use pin name as group name. */
	pctl->grp_names = devm_kzalloc(&pdev->dev,
				    pctl->ngroups * sizeof(*pctl->grp_names),
				    GFP_KERNEL);
	if (!pctl->grp_names)
		return -ENOMEM;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;
		struct mtk_pinctrl_group *group = pctl->groups + i;
		const char **func_grp;

		group->name = pin->pin.name;
		group->pin = pin->pin.number;

		func_grp = pctl->grp_names;
		while (*func_grp)
			func_grp++;

		*func_grp = pin->pin.name;
	}

	return 0;
}

static struct pinctrl_desc mtk_pctrl_desc = {
	.confops	= &mtk_pconf_ops,
	.pctlops	= &mtk_pctrl_ops,
	.pmxops		= &mtk_pmx_ops,
};

int mtk_pctrl_init(struct platform_device *pdev,
		const struct mtk_pinctrl_devdata *data)
{
	struct pinctrl_pin_desc *pins;
	struct mtk_pinctrl *pctl;
	struct resource *res;
	int i, ret;

	pctl = devm_kzalloc(&pdev->dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	platform_set_drvdata(pdev, pctl);

	spin_lock_init(&pctl->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (MTK_CHIP_TYPE_PMIC == data->chip_type) {
		pctl->membase1 = (void *)res->start;
	} else {
		pctl->membase1 = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(pctl->membase1))
			return PTR_ERR(pctl->membase1);
	}

	if (pdev->num_resources > 1) {
		/* Only 8135 has two base addr, other SoCs have only one. */
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		pctl->membase2 = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(pctl->membase2))
			return PTR_ERR(pctl->membase2);
	}

	pctl->devdata = data;
	ret = mtk_pctrl_build_state(pdev);
	if (ret) {
		dev_err(&pdev->dev, "build state failed: %d\n", ret);
		return -EINVAL;
	}

	pins = devm_kzalloc(&pdev->dev,
			    pctl->devdata->npins * sizeof(*pins),
			    GFP_KERNEL);
	if (!pins)
		return -ENOMEM;

	for (i = 0; i < pctl->devdata->npins; i++)
		pins[i] = pctl->devdata->pins[i].pin;
	mtk_pctrl_desc.name = dev_name(&pdev->dev);
	mtk_pctrl_desc.owner = THIS_MODULE;
	mtk_pctrl_desc.pins = pins;
	mtk_pctrl_desc.npins = pctl->devdata->npins;
	pctl->dev = &pdev->dev;
	pctl->pctl_dev = pinctrl_register(&mtk_pctrl_desc, &pdev->dev, pctl);
	if (!pctl->pctl_dev) {
		dev_err(&pdev->dev, "couldn't register pinctrl driver\n");
		return -EINVAL;
	}

	pctl->chip = devm_kzalloc(&pdev->dev, sizeof(*pctl->chip), GFP_KERNEL);
	if (!pctl->chip) {
		ret = -ENOMEM;
		goto pctrl_error;
	}

	mt_gpio_chip_set(pctl->chip);

	pctl->chip->ngpio = pctl->devdata->npins;
	pctl->chip->label = dev_name(&pdev->dev);
	pctl->chip->dev = &pdev->dev;
	pctl->chip->base = pctl->devdata->base;
	dev_info(&pdev->dev, "base: %d, ngpio: %d\n",
			pctl->chip->base, pctl->chip->ngpio);

	ret = gpiochip_add(pctl->chip);
	if (ret) {
		ret = -EINVAL;
		goto pctrl_error;
	}

	/* Register the GPIO to pin mappings. */
	ret = gpiochip_add_pin_range(pctl->chip, dev_name(&pdev->dev),
			0, 0, pctl->devdata->npins);
	if (ret) {
		ret = -EINVAL;
		goto chip_error;
	}

	dev_info(&pdev->dev, "Pinctrl Driver Probe Success.\n");
	return 0;

chip_error:
	if (gpiochip_remove(pctl->chip))
		dev_err(&pdev->dev, "failed to remove gpio chip\n");

pctrl_error:
	pinctrl_unregister(pctl->pctl_dev);
	return ret;
}

int mtk_pctrl_remove(struct platform_device *pdev)
{
	int ret;
	struct mtk_pinctrl *pctl = platform_get_drvdata(pdev);

	pinctrl_unregister(pctl->pctl_dev);

	ret = gpiochip_remove(pctl->chip);
	if (ret) {
		dev_err(&pdev->dev, "%s failed, %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("Hongzhou Yang <hongzhou.yang@mediatek.com>");
