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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt6397.h"
#include "pinctrl-mtk-mt8173.h"

static const struct mtk_pinctrl_devdata mt6397_pinctrl_data = {
	.pins = mtk_pins_mt6397,
	.base = ARRAY_SIZE(mtk_pins_mt8173),
	.npins = ARRAY_SIZE(mtk_pins_mt6397),
	.grp_desc = NULL,
	.n_grp_cls = 0,
	.pin_drv_grp = NULL,
	.n_pin_drv_grps = 0,
	.ies_smt_pins = NULL,
	.n_ies_smt_pins = 0,
	.pupu_spec_pins = NULL,
	.n_pupu_spec_pins = 0,
	.dir_offset = 0x000,
	.pullen_offset = 0x020,
	.pullsel_offset = 0x040,
	.invser_offset = 0x060,
	.dout_offset = 0x080,
	.din_offset = 0x0A0,
	.pinmux_offset = 0x0C0,
	.type1_start = 41,
	.type1_end = 41,
	.port_shf = 3,
	.port_mask = 0x3,
	.port_align = 2,
	.chip_type = MTK_CHIP_TYPE_PMIC,
};

static int mt6397_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_pctrl_init(pdev, &mt6397_pinctrl_data);
}

static int mt6397_pinctrl_remove(struct platform_device *pdev)
{
	return mtk_pctrl_remove(pdev);
}

static struct of_device_id mt6397_pctrl_match[] = {
	{
		.compatible = "mediatek,mt6397-pinctrl",
	}, {
	}
};
MODULE_DEVICE_TABLE(of, mt6397_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mt6397_pinctrl_probe,
	.remove = mt6397_pinctrl_remove,
	.driver = {
		.name = "mediatek-mt6397-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt6397_pctrl_match,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

static void __exit mtk_pinctrl_exit(void)
{
	platform_driver_unregister(&mtk_pinctrl_driver);
}

postcore_initcall(mtk_pinctrl_init);
module_exit(mtk_pinctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("Hongzhou Yang <hongzhou.yang@mediatek.com>");
