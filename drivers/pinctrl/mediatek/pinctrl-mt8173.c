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
#include "pinctrl-mtk-mt8173.h"

static const struct mtk_pin_spec_pupd_set mt8173_spec_pupd[] = {
	MTK_PIN_PUPD_SPEC(119, 0xe00, 2, 1, 0),  /* KROW0 */
	MTK_PIN_PUPD_SPEC(120, 0xe00, 6, 5, 4),  /* KROW1 */
	MTK_PIN_PUPD_SPEC(121, 0xe00, 10, 9, 8), /* KROW2 */
	MTK_PIN_PUPD_SPEC(122, 0xe10, 2, 1, 0),  /* KCOL0 */
	MTK_PIN_PUPD_SPEC(123, 0xe10, 6, 5, 4),  /* KCOL1 */
	MTK_PIN_PUPD_SPEC(124, 0xe10, 10, 9, 8), /* KCOL2 */

	MTK_PIN_PUPD_SPEC(67, 0xd10, 2, 1, 0),   /* ms0 DS */
	MTK_PIN_PUPD_SPEC(68, 0xd00, 2, 1, 0),   /* ms0 RST */
	MTK_PIN_PUPD_SPEC(66, 0xc10, 2, 1, 0),   /* ms0 cmd */
	MTK_PIN_PUPD_SPEC(65, 0xc00, 2, 1, 0),   /* ms0 clk */
	MTK_PIN_PUPD_SPEC(57, 0xc20, 2, 1, 0),   /* ms0 data0 */
	MTK_PIN_PUPD_SPEC(58, 0xc20, 2, 1, 0),   /* ms0 data1 */
	MTK_PIN_PUPD_SPEC(59, 0xc20, 2, 1, 0),   /* ms0 data2 */
	MTK_PIN_PUPD_SPEC(60, 0xc20, 2, 1, 0),   /* ms0 data3 */
	MTK_PIN_PUPD_SPEC(61, 0xc20, 2, 1, 0),   /* ms0 data4 */
	MTK_PIN_PUPD_SPEC(62, 0xc20, 2, 1, 0),   /* ms0 data5 */
	MTK_PIN_PUPD_SPEC(63, 0xc20, 2, 1, 0),   /* ms0 data6 */
	MTK_PIN_PUPD_SPEC(64, 0xc20, 2, 1, 0),   /* ms0 data7 */

	MTK_PIN_PUPD_SPEC(78, 0xc50, 2, 1, 0),    /* ms1 cmd */
	MTK_PIN_PUPD_SPEC(73, 0xd20, 2, 1, 0),    /* ms1 dat0 */
	MTK_PIN_PUPD_SPEC(74, 0xd20, 6, 5, 4),    /* ms1 dat1 */
	MTK_PIN_PUPD_SPEC(75, 0xd20, 10, 9, 8),   /* ms1 dat2 */
	MTK_PIN_PUPD_SPEC(76, 0xd20, 14, 13, 12), /* ms1 dat3 */
	MTK_PIN_PUPD_SPEC(77, 0xc40, 2, 1, 0),    /* ms1 clk */

	MTK_PIN_PUPD_SPEC(100, 0xd40, 2, 1, 0),    /* ms2 dat0 */
	MTK_PIN_PUPD_SPEC(101, 0xd40, 6, 5, 4),    /* ms2 dat1 */
	MTK_PIN_PUPD_SPEC(102, 0xd40, 10, 9, 8),   /* ms2 dat2 */
	MTK_PIN_PUPD_SPEC(103, 0xd40, 14, 13, 12), /* ms2 dat3 */
	MTK_PIN_PUPD_SPEC(104, 0xc80, 2, 1, 0),    /* ms2 clk */
	MTK_PIN_PUPD_SPEC(105, 0xc90, 2, 1, 0),    /* ms2 cmd */

	MTK_PIN_PUPD_SPEC(22, 0xd60, 2, 1, 0),    /* ms3 dat0 */
	MTK_PIN_PUPD_SPEC(23, 0xd60, 6, 5, 4),    /* ms3 dat1 */
	MTK_PIN_PUPD_SPEC(24, 0xd60, 10, 9, 8),   /* ms3 dat2 */
	MTK_PIN_PUPD_SPEC(25, 0xd60, 14, 13, 12), /* ms3 dat3 */
	MTK_PIN_PUPD_SPEC(26, 0xcc0, 2, 1, 0),    /* ms3 clk */
	MTK_PIN_PUPD_SPEC(27, 0xcd0, 2, 1, 0)     /* ms3 cmd */
};

static const struct mtk_pin_ies_smt_set mt8173_ies_smt_set[] = {
	MTK_PIN_IES_SMT_SET(0, 4, 0x930, 1),
	MTK_PIN_IES_SMT_SET(5, 9, 0x930, 2),
	MTK_PIN_IES_SMT_SET(10, 13, 0x930, 10),
	MTK_PIN_IES_SMT_SET(14, 15, 0x940, 10),
	MTK_PIN_IES_SMT_SET(16, 16, 0x930, 0),
	MTK_PIN_IES_SMT_SET(17, 17, 0x950, 2),
	MTK_PIN_IES_SMT_SET(18, 21, 0x940, 3),
	MTK_PIN_IES_SMT_SET(29, 32, 0x930, 3),
	MTK_PIN_IES_SMT_SET(33, 33, 0x930, 4),
	MTK_PIN_IES_SMT_SET(34, 36, 0x930, 5),
	MTK_PIN_IES_SMT_SET(37, 38, 0x930, 6),
	MTK_PIN_IES_SMT_SET(39, 39, 0x930, 7),
	MTK_PIN_IES_SMT_SET(40, 41, 0x930, 9),
	MTK_PIN_IES_SMT_SET(42, 42, 0x940, 0),
	MTK_PIN_IES_SMT_SET(43, 44, 0x930, 11),
	MTK_PIN_IES_SMT_SET(45, 46, 0x930, 12),
	MTK_PIN_IES_SMT_SET(57, 64, 0xc20, 13),
	MTK_PIN_IES_SMT_SET(65, 65, 0xc10, 13),
	MTK_PIN_IES_SMT_SET(66, 66, 0xc00, 13),
	MTK_PIN_IES_SMT_SET(67, 67, 0xd10, 13),
	MTK_PIN_IES_SMT_SET(68, 68, 0xd00, 13),
	MTK_PIN_IES_SMT_SET(69, 72, 0x940, 14),
	MTK_PIN_IES_SMT_SET(73, 76, 0xc60, 13),
	MTK_PIN_IES_SMT_SET(77, 77, 0xc40, 13),
	MTK_PIN_IES_SMT_SET(78, 78, 0xc50, 13),
	MTK_PIN_IES_SMT_SET(79, 82, 0x940, 15),
	MTK_PIN_IES_SMT_SET(83, 83, 0x950, 0),
	MTK_PIN_IES_SMT_SET(84, 85, 0x950, 1),
	MTK_PIN_IES_SMT_SET(86, 91, 0x950, 2),
	MTK_PIN_IES_SMT_SET(92, 92, 0x930, 13),
	MTK_PIN_IES_SMT_SET(93, 95, 0x930, 14),
	MTK_PIN_IES_SMT_SET(96, 99, 0x930, 15),
	MTK_PIN_IES_SMT_SET(100, 103, 0xca0, 13),
	MTK_PIN_IES_SMT_SET(104, 104, 0xc80, 13),
	MTK_PIN_IES_SMT_SET(105, 105, 0xc90, 13),
	MTK_PIN_IES_SMT_SET(106, 107, 0x940, 4),
	MTK_PIN_IES_SMT_SET(108, 112, 0x940, 1),
	MTK_PIN_IES_SMT_SET(113, 116, 0x940, 2),
	MTK_PIN_IES_SMT_SET(117, 118, 0x940, 5),
	MTK_PIN_IES_SMT_SET(119, 124, 0x940, 6),
	MTK_PIN_IES_SMT_SET(125, 126, 0x940, 7),
	MTK_PIN_IES_SMT_SET(127, 127, 0x940, 0),
	MTK_PIN_IES_SMT_SET(128, 128, 0x950, 8),
	MTK_PIN_IES_SMT_SET(129, 130, 0x950, 9),
	MTK_PIN_IES_SMT_SET(131, 132, 0x950, 8),
	MTK_PIN_IES_SMT_SET(133, 134, 0x910, 8)
};

static const struct mtk_pinctrl_devdata mt8173_pinctrl_data = {
	.pins = mtk_pins_mt8173,
	.base = 0,
	.npins = ARRAY_SIZE(mtk_pins_mt8173),
	.grp_desc = NULL,
	.n_grp_cls = 0,
	.pin_drv_grp = NULL,
	.n_pin_drv_grps = 0,
	.ies_smt_pins = mt8173_ies_smt_set,
	.n_ies_smt_pins = ARRAY_SIZE(mt8173_ies_smt_set),
	.pupu_spec_pins = mt8173_spec_pupd,
	.n_pupu_spec_pins = ARRAY_SIZE(mt8173_spec_pupd),
	.dir_offset = 0x0000,
	.pullen_offset = 0x0100,
	.pullsel_offset = 0x0200,
	.dout_offset = 0x0400,
	.din_offset = 0x0500,
	.pinmux_offset = 0x0600,
	.type1_start = 135,
	.type1_end = 135,
	.port_shf = 4,
	.port_mask = 0xf,
	.port_align = 4,
	.chip_type = MTK_CHIP_TYPE_BASE,
};

static int mt8173_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_pctrl_init(pdev, &mt8173_pinctrl_data);
}

static int mt8173_pinctrl_remove(struct platform_device *pdev)
{
	return mtk_pctrl_remove(pdev);
}

static struct of_device_id mt8173_pctrl_match[] = {
	{
		.compatible = "mediatek,mt8173-pinctrl",
	}, {
	}
};
MODULE_DEVICE_TABLE(of, mt8173_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mt8173_pinctrl_probe,
	.remove = mt8173_pinctrl_remove,
	.driver = {
		.name = "mediatek-mt8173-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt8173_pctrl_match,
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
