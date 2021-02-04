/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora.Fu <flora.fu@mediatek.com>
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/mt6397-regulator.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>

#define MT6397_BUCK_MODE_AUTO	0
#define MT6397_BUCK_MODE_FORCE_PWM	1

struct mt6397_buck_conf {
	unsigned int vosel_reg;
	unsigned int voselon_reg;
	unsigned int nivosel_reg;
	unsigned int vosel_mask;
	unsigned int modeset_reg;
	unsigned int modeset_mask;
	unsigned int modeset_shift;
};

struct mt6397_ldo_conf {
	unsigned int is_fixed;
	unsigned int vosel_reg;
	unsigned int vosel_mask;
	unsigned int vosel_shift;
};

struct mt6397_regulator_info {
	struct regulator_desc desc;
	struct mt6397_buck_conf buck_conf;
	struct mt6397_ldo_conf ldo_conf;
	unsigned int qi;
	unsigned int qi_mask;
};

struct mt6397_regulator_data {
	int id;
	const char *name;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct mt6397_regulator_priv {
	struct mt6397_regulator_data *regulators;
	unsigned int num_regulators;
	struct regulator_dev *rdev[MT6397_MAX_REGULATOR];
};

#define MT6397_BUCK(vreg, min, max, step,			\
		enreg, vosel, voselon, nivosel, _modeset_reg,	\
		_modeset_mask, _modeset_shift)			\
[MT6397_ID_##vreg] = {						\
	.desc = {						\
		.name = #vreg,					\
		.ops = &mt6397_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6397_ID_##vreg,				\
		.owner = THIS_MODULE,				\
		.n_voltages	= (max - min)/step + 1,		\
		.enable_reg = enreg,				\
		.enable_mask = BIT(0),				\
		.min_uV = min,					\
		.uV_step = step,				\
		.linear_min_sel = 0,				\
	},							\
	.qi = enreg,						\
	.qi_mask = BIT(13),					\
	.buck_conf = {						\
		.vosel_reg = vosel,				\
		.voselon_reg = voselon,				\
		.nivosel_reg = nivosel,				\
		.vosel_mask = 0x7f,				\
		.modeset_reg = _modeset_reg,			\
		.modeset_mask = _modeset_mask,			\
		.modeset_shift = _modeset_shift			\
	},							\
}

#define MT6397_BUCK_TABLE(vreg, buck_volt_table,		\
		enreg, vosel, voselon, nivosel, _modeset_reg,	\
		_modeset_mask, _modeset_shift)			\
[MT6397_ID_##vreg] = {						\
	.desc = {						\
		.name = #vreg,					\
		.ops = &mt6397_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6397_ID_##vreg,				\
		.owner = THIS_MODULE,				\
		.n_voltages = 1,				\
		.volt_table = buck_volt_table,			\
		.enable_reg = enreg,				\
		.enable_mask = BIT(0),				\
	},							\
	.qi = enreg,						\
	.qi_mask = BIT(13),					\
	.buck_conf = {						\
		.vosel_reg = vosel,				\
		.voselon_reg = voselon,				\
		.nivosel_reg = nivosel,				\
		.vosel_mask = 0x1f,				\
		.modeset_reg = _modeset_reg,			\
		.modeset_mask = _modeset_mask,			\
		.modeset_shift = _modeset_shift			\
	},							\
}

#define MT6397_LDO(vreg, ldo_volt_table,			\
		enreg, enbit, fixed, vosel, _vosel_mask,	\
		_vosel_shift)					\
[MT6397_ID_##vreg] = {						\
	.desc = {						\
		.name = #vreg,					\
		.ops = &mt6397_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6397_ID_##vreg,				\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),	\
		.volt_table = ldo_volt_table,			\
		.enable_reg = enreg,				\
		.enable_mask = BIT(enbit),			\
	},							\
	.qi = enreg,						\
	.qi_mask = BIT(15),					\
	.ldo_conf = {						\
		.is_fixed = fixed,				\
		.vosel_reg = vosel,				\
		.vosel_mask = _vosel_mask,			\
		.vosel_shift = _vosel_shift,			\
	},							\
}

static const unsigned int fixed_1800000_voltage[] = {
	1800000,
};

static const unsigned int fixed_2800000_voltage[] = {
	2800000,
};

static const unsigned int fixed_3300000_voltage[] = {
	3300000,
};

static const unsigned int ldo_volt_table1[] = {
	1500000, 1800000, 2500000, 2800000,
};

static const unsigned int ldo_volt_table2[] = {
	1800000, 3300000,
};

static const unsigned int ldo_volt_table3[] = {
	3000000, 3300000,
};

static const unsigned int ldo_volt_table4[] = {
	1220000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table5[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table5_v2[] = {
	1200000, 1000000, 1500000, 1800000, 2500000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table6[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 2000000,
};

static const unsigned int ldo_volt_table7[] = {
	1300000, 1500000, 1800000, 2000000, 2500000, 2800000, 3000000, 3300000,
};

static int mt6397_buck_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	int ret;
	unsigned int vosel;
	unsigned int voselon;
	unsigned int vosel_mask;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	vosel = info->buck_conf.vosel_reg;
	voselon = info->buck_conf.voselon_reg;
	vosel_mask = info->buck_conf.vosel_mask;

	dev_dbg(&rdev->dev, "mt6397 buck set_voltage_sel 0x%x, 0x%x, 0x%x\n",
		   vosel, voselon, sel);

	ret = regmap_update_bits(rdev->regmap, vosel, vosel_mask, sel);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to update vosel: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(rdev->regmap, voselon, vosel_mask, sel);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to update vosel_on: %d\n", ret);
		return ret;
	}
	return 0;
}

static int mt6397_buck_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int nivosel;
	unsigned int vosel_mask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	nivosel = info->buck_conf.nivosel_reg;
	vosel_mask = info->buck_conf.vosel_mask;

	ret = regmap_read(rdev->regmap, nivosel, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n", ret);
		return ret;
	}

	regval &= vosel_mask;
	regval >>= ffs(vosel_mask) - 1;

	return regval;
}


static int mt6397_ldo_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	int ret;
	unsigned int is_fixed;
	unsigned int vosel;
	unsigned int vosel_mask;
	unsigned int vosel_shift;

	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	is_fixed = info->ldo_conf.is_fixed;
	vosel = info->ldo_conf.vosel_reg;
	vosel_mask = info->ldo_conf.vosel_mask;
	vosel_shift = info->ldo_conf.vosel_shift;

	if (is_fixed)
		return 0;

	dev_dbg(&rdev->dev, "mt6397 ldo set_voltage_sel 0x%x, 0x%x, 0x%x, 0x%x\n",
		   vosel, vosel_mask, vosel_shift, sel);

	sel <<= vosel_shift;
	ret = regmap_update_bits(rdev->regmap, vosel, vosel_mask, sel);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to update vosel: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6397_ldo_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int vosel;
	unsigned int vosel_mask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	vosel = info->ldo_conf.vosel_reg;
	vosel_mask = info->ldo_conf.vosel_mask;

	ret = regmap_read(rdev->regmap, vosel, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n", ret);
		return ret;
	}

	regval &= vosel_mask;
	regval >>= ffs(vosel_mask) - 1;

	return regval;
}

static int mt6397_regulator_is_enabled(struct regulator_dev *rdev)
{
	int ret;
	unsigned int regaddr;
	unsigned int regmask;
	unsigned int regval;
	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	regaddr = info->qi;
	regmask = info->qi_mask;

	ret = regmap_read(rdev->regmap, regaddr, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get vosel: %d\n", ret);
		return ret;
	}

	return (regval & info->qi_mask) ? 1 : 0;
}

static int mt6397_regulator_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	int ret;
	unsigned int modeset_reg;
	unsigned int modeset_mask;
	unsigned int modeset_shift;

	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		mode = MT6397_BUCK_MODE_FORCE_PWM;
		break;
	case REGULATOR_MODE_NORMAL:
	case REGULATOR_MODE_IDLE:
	case REGULATOR_MODE_STANDBY:
		mode = MT6397_BUCK_MODE_AUTO;
		break;
	}

	modeset_reg = info->buck_conf.modeset_reg;
	modeset_mask = info->buck_conf.modeset_mask;
	modeset_shift = info->buck_conf.modeset_shift;

	dev_warn(&rdev->dev, "mt6397 buck set_mode 0x%x, 0x%x, 0x%x, 0x%x\n",
		   modeset_reg, modeset_mask, modeset_shift, mode);

	mode <<= modeset_shift;
	ret = regmap_update_bits(rdev->regmap, modeset_reg, modeset_mask, mode);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to set mt6397 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static unsigned int mt6397_regulator_get_mode(struct regulator_dev *rdev)
{
	int ret, regval;
	unsigned int modeset_reg;
	unsigned int modeset_mask;
	unsigned int mode = 0;

	struct mt6397_regulator_info *info = rdev_get_drvdata(rdev);

	modeset_reg = info->buck_conf.modeset_reg;
	modeset_mask = info->buck_conf.modeset_mask;

	ret = regmap_read(rdev->regmap, modeset_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get mt6397 buck mode: %d\n", ret);
		return ret;
	}

	switch ((regval & modeset_mask) ? 1 : 0) {
	case MT6397_BUCK_MODE_AUTO:
		mode = REGULATOR_MODE_NORMAL;
		break;
	case MT6397_BUCK_MODE_FORCE_PWM:
		mode = REGULATOR_MODE_FAST;
		break;
	}

	return mode;
}

static struct regulator_ops mt6397_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.set_voltage_sel = mt6397_buck_set_voltage_sel,
	.get_voltage_sel = mt6397_buck_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = mt6397_regulator_is_enabled,
	.set_mode = mt6397_regulator_set_mode,
	.get_mode = mt6397_regulator_get_mode,
};

static struct regulator_ops mt6397_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6397_ldo_set_voltage_sel,
	.get_voltage_sel = mt6397_ldo_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = mt6397_regulator_is_enabled,
};

/* The array is indexed by id(MT6397_ID_XXX) */
static struct mt6397_regulator_info mt6397_regulators[] = {
	MT6397_BUCK(VPCA15, 700000, 1493750, 6250,
		    MT6397_VCA15_CON7, MT6397_VCA15_CON9,
		    MT6397_VCA15_CON10, MT6397_VCA15_CON12,
		    MT6397_VCA15_CON2, 0x800, 11),
	MT6397_BUCK(VPCA7, 700000, 1493750, 6250,
		    MT6397_VPCA7_CON7, MT6397_VPCA7_CON9,
		    MT6397_VPCA7_CON10, MT6397_VPCA7_CON12,
		    MT6397_VPCA7_CON2, 0x100, 8),
	MT6397_BUCK(VSRAMCA15, 700000, 1493750, 6250,
		    MT6397_VSRMCA15_CON7, MT6397_VSRMCA15_CON9,
		    MT6397_VSRMCA15_CON10, MT6397_VSRMCA15_CON12,
		    MT6397_VSRMCA15_CON2, 0x100, 8),
	MT6397_BUCK(VSRAMCA7, 700000, 1493750, 6250,
		    MT6397_VSRMCA7_CON7, MT6397_VSRMCA7_CON9,
		    MT6397_VSRMCA7_CON10, MT6397_VSRMCA7_CON12,
		    MT6397_VSRMCA7_CON2, 0x100, 8),
	MT6397_BUCK(VCORE, 700000, 1493750, 6250,
		    MT6397_VCORE_CON7, MT6397_VCORE_CON9,
		    MT6397_VCORE_CON10, MT6397_VCORE_CON12,
		    MT6397_VCORE_CON2, 0x100, 8),
	MT6397_BUCK(VGPU, 700000, 1493750, 6250,
		    MT6397_VGPU_CON7, MT6397_VGPU_CON9,
		    MT6397_VGPU_CON10, MT6397_VCORE_CON12,
		    MT6397_VGPU_CON2, 0x100, 8),
	MT6397_BUCK(VDRM, 800000, 1593750, 6250,
		    MT6397_VDRM_CON7, MT6397_VDRM_CON9,
		    MT6397_VDRM_CON10, MT6397_VDRM_CON12,
		    MT6397_VDRM_CON2, 0x100, 8),
	MT6397_BUCK_TABLE(VIO18, fixed_1800000_voltage,
			  MT6397_VIO18_CON7, MT6397_VIO18_CON9,
			  MT6397_VIO18_CON10, MT6397_VIO18_CON12,
			  MT6397_VIO18_CON2, 0x100, 8),
	MT6397_LDO(VTCXO, fixed_2800000_voltage,
		   MT6397_ANALDO_CON0, 10, 1, 0, 0, 0),
	MT6397_LDO(VA28, fixed_2800000_voltage,
		   MT6397_ANALDO_CON1, 14, 1, 0, 0, 0),
	MT6397_LDO(VCAMA, ldo_volt_table1,
		   MT6397_ANALDO_CON2, 15, 0, MT6397_ANALDO_CON6, 0xC0, 6),
	MT6397_LDO(VIO28, fixed_2800000_voltage,
		   MT6397_DIGLDO_CON0, 14, 1, 0, 0, 0),
	MT6397_LDO(USB, fixed_3300000_voltage,
		   MT6397_DIGLDO_CON1, 14, 1, 0, 0, 0),
	MT6397_LDO(VMC, ldo_volt_table2,
		   MT6397_DIGLDO_CON2, 12, 0, MT6397_DIGLDO_CON29, 0x10, 4),
	MT6397_LDO(VMCH, ldo_volt_table3,
		   MT6397_DIGLDO_CON3, 14, 0, MT6397_DIGLDO_CON17, 0x80, 7),
	MT6397_LDO(VEMC3V3, ldo_volt_table3,
		   MT6397_DIGLDO_CON4, 14, 0, MT6397_DIGLDO_CON18, 0x10, 4),
	MT6397_LDO(VCAMD, ldo_volt_table4,
		   MT6397_DIGLDO_CON5, 15, 0, MT6397_DIGLDO_CON19, 0xE0, 5),
	MT6397_LDO(VCAMIO, ldo_volt_table5,
		   MT6397_DIGLDO_CON6, 15, 0, MT6397_DIGLDO_CON20, 0xE0, 5),
	MT6397_LDO(VCAMAF, ldo_volt_table5,
		   MT6397_DIGLDO_CON7, 15, 0, MT6397_DIGLDO_CON21, 0xE0, 5),
	MT6397_LDO(VGP4, ldo_volt_table5,
		   MT6397_DIGLDO_CON8, 15, 0, MT6397_DIGLDO_CON22, 0xE0, 5),
	MT6397_LDO(VGP5, ldo_volt_table6,
		   MT6397_DIGLDO_CON9, 15, 0, MT6397_DIGLDO_CON23, 0xE0, 5),
	MT6397_LDO(VGP6, ldo_volt_table5,
		   MT6397_DIGLDO_CON10, 15, 0, MT6397_DIGLDO_CON33, 0xE0, 5),
	MT6397_LDO(VIBR, ldo_volt_table7,
		   MT6397_DIGLDO_CON24, 15, 0, MT6397_DIGLDO_CON25, 0xE00, 9),
};

#define MT6397_REGULATOR_OF_MATCH(_name, _id)			\
[MT6397_ID_##_id] = {						\
	.name = #_name,						\
	.driver_data = &mt6397_regulators[MT6397_ID_##_id],	\
}

static struct of_regulator_match mt6397_regulator_matches[] = {
	MT6397_REGULATOR_OF_MATCH(buck_vpca15, VPCA15),
	MT6397_REGULATOR_OF_MATCH(buck_vpca7, VPCA7),
	MT6397_REGULATOR_OF_MATCH(buck_vsramca15, VSRAMCA15),
	MT6397_REGULATOR_OF_MATCH(buck_vsramca7, VSRAMCA7),
	MT6397_REGULATOR_OF_MATCH(buck_vcore, VCORE),
	MT6397_REGULATOR_OF_MATCH(buck_vgpu, VGPU),
	MT6397_REGULATOR_OF_MATCH(buck_vdrm, VDRM),
	MT6397_REGULATOR_OF_MATCH(buck_vio18, VIO18),
	MT6397_REGULATOR_OF_MATCH(ldo_vtcxo, VTCXO),
	MT6397_REGULATOR_OF_MATCH(ldo_va28, VA28),
	MT6397_REGULATOR_OF_MATCH(ldo_vcama, VCAMA),
	MT6397_REGULATOR_OF_MATCH(ldo_vio28, VIO28),
	MT6397_REGULATOR_OF_MATCH(ldo_usb, USB),
	MT6397_REGULATOR_OF_MATCH(ldo_vmc, VMC),
	MT6397_REGULATOR_OF_MATCH(ldo_vmch, VMCH),
	MT6397_REGULATOR_OF_MATCH(ldo_vemc3v3, VEMC3V3),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamd, VCAMD),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamio, VCAMIO),
	MT6397_REGULATOR_OF_MATCH(ldo_vcamaf, VCAMAF),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp4, VGP4),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp5, VGP5),
	MT6397_REGULATOR_OF_MATCH(ldo_vgp6, VGP6),
	MT6397_REGULATOR_OF_MATCH(ldo_vibr, VIBR),
};

static int mt6397_regulator_dt_init(struct platform_device *pdev,
				    struct mt6397_regulator_priv *priv)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np, *regulators;
	struct mt6397_regulator_data *rdata;
	struct regulation_constraints	*c;
	int matched, i, j, ret;
	unsigned int reg_value;

	if (!priv) {
		dev_err(&pdev->dev, "regulator private data missing\n");
		return -EINVAL;
	}

	np = of_node_get(pdev->dev.parent->of_node);
	if (!np)
		return -EINVAL;

	regulators = of_get_child_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&pdev->dev, "regulators node not found\n");
		ret = -EINVAL;
		goto out;
	}
	of_node_put(np);
	matched = of_regulator_match(&pdev->dev, regulators,
				     mt6397_regulator_matches,
				     ARRAY_SIZE(mt6397_regulator_matches));
	of_node_put(regulators);
	if (matched < 0) {
		dev_err(&pdev->dev, "Error parsing regulator init data: %d\n", matched);
		return matched;
	}

	priv->num_regulators = matched;
	priv->regulators = devm_kzalloc(&pdev->dev,
					(sizeof(struct mt6397_regulator_data) *
					 ARRAY_SIZE(mt6397_regulator_matches)), GFP_KERNEL);
	if (!priv->regulators)
		return -ENOMEM;

	/* Read PMIC chip revision to update constraints and voltage table */
	if (regmap_read(mt6397->regmap, MT6397_CID, &reg_value) < 0)
		dev_err(&pdev->dev, "Failed to read Chip ID\n");
	else
		dev_info(&pdev->dev, "Chip ID = 0x%x\n", reg_value);

	if ((reg_value & 0xFF) == MT6397_REGULATOR_ID91) {
		dev_err(&pdev->dev, "regulator Chip ID=0x%x\n", reg_value);
		j = MT6397_ID_VCAMIO;
		mt6397_regulator_matches[j].init_data->constraints.min_uV =
		1000000;
		mt6397_regulators[j].desc.volt_table = ldo_volt_table5_v2;
	}

	/* Constrain board-specific capabilities according to what
	 * this driver and the chip itself can actually do.
	 */
	for (i = 0; i < ARRAY_SIZE(mt6397_regulator_matches); i++) {
		c = &mt6397_regulator_matches[i].init_data->constraints;
		c->valid_modes_mask |= REGULATOR_MODE_FAST | REGULATOR_MODE_NORMAL;
		c->valid_ops_mask |= REGULATOR_CHANGE_MODE;
	}

	rdata = priv->regulators;
	for (i = 0; i < ARRAY_SIZE(mt6397_regulator_matches); i++) {
		rdata->id = i;
		rdata->name = mt6397_regulator_matches[i].name;
		rdata->initdata = mt6397_regulator_matches[i].init_data;
		rdata->reg_node = mt6397_regulator_matches[i].of_node;
		rdata++;
	}
	return 0;

 out:
	of_node_put(np);
	return ret;
}

static int mt6397_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	int ret, i, id, size;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct mt6397_regulator_priv *priv;

	pr_notice("mt6397_regulator_probe\n");
	size = sizeof(struct mt6397_regulator_priv);
	priv = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	ret = mt6397_regulator_dt_init(pdev, priv);
	if (ret)
		return ret;

	for (i = 0; i < MT6397_ID_RG_MAX; i++) {
		id = priv->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = priv->regulators[i].initdata;
		config.of_node = priv->regulators[i].reg_node;
		config.driver_data = mt6397_regulator_matches[i].driver_data;
		config.regmap = mt6397->regmap;

		rdev = regulator_register(&mt6397_regulators[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s\n",
				mt6397_regulators[id].desc.name);
			ret = PTR_ERR(rdev);
			goto err;
		}
		priv->rdev[i] = rdev;
	}
	return 0;

 err:
	while (--i >= 0)
		regulator_unregister(priv->rdev[i]);
	return ret;
}

static int mt6397_regulator_remove(struct platform_device *pdev)
{
	struct mt6397_regulator_priv *priv = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < priv->num_regulators; i++)
		regulator_unregister(priv->rdev[i]);

	return 0;
}

static const struct platform_device_id mt6397_regulator_id[] = {
	{"mt6397-regulator", 0},
	{},
};

static struct platform_driver mt6397_regulator_driver = {
	.driver = {
		   .name = "mt6397-regulator",
		   .owner = THIS_MODULE,
		   },
	.probe = mt6397_regulator_probe,
	.remove = mt6397_regulator_remove,
	.id_table = mt6397_regulator_id,
};

static int __init mt6397_regulator_init(void)
{
	pr_notice("mt6397_regulator_init\n");
	return platform_driver_register(&mt6397_regulator_driver);
}
subsys_initcall(mt6397_regulator_init);

MODULE_AUTHOR("Flora Fu <flora.fu@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6397-regulator");
