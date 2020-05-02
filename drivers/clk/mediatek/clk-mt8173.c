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
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-gate.h"
#include "clk-mt8173-pll.h"

#include <dt-bindings/clock/mt8173-clk.h>

/*
 * platform clocks
 */

/* ROOT */
#define clk_null		"clk_null"
#define clk26m			"clk26m"
#define clk32k			"clk32k"

#define clkph_mck_o		"clkph_mck_o"
#define dpi_ck			"dpi_ck"
#define usb_syspll_125m		"usb_syspll_125m"
#define hdmitx_dig_cts		"hdmitx_dig_cts"

/* PLL */
#define armca15pll		"armca15pll"
#define armca7pll		"armca7pll"
#define mainpll			"mainpll"
#define univpll			"univpll"
#define mmpll			"mmpll"
#define msdcpll			"msdcpll"
#define vencpll			"vencpll"
#define tvdpll			"tvdpll"
#define mpll			"mpll"
#define vcodecpll		"vcodecpll"
#define apll1			"apll1"
#define apll2			"apll2"
#define lvdspll			"lvdspll"
#define msdcpll2		"msdcpll2"

#define armca7pll_754m		"armca7pll_754m"
#define armca7pll_502m		"armca7pll_502m"
#define apll1_180p633m		apll1
#define apll2_196p608m		apll2
#define mmpll_455m		mmpll
#define msdcpll_806m		msdcpll
#define main_h546m		"main_h546m"
#define main_h364m		"main_h364m"
#define main_h218p4m		"main_h218p4m"
#define main_h156m		"main_h156m"
#define tvdpll_445p5m		"tvdpll_445p5m"
#define tvdpll_594m		"tvdpll_594m"
#define univ_624m		"univ_624m"
#define univ_416m		"univ_416m"
#define univ_249p6m		"univ_249p6m"
#define univ_178p3m		"univ_178p3m"
#define univ_48m		"univ_48m"
#define vcodecpll_370p5		"vcodecpll_370p5"
#define vcodecpll_494m		vcodecpll
#define vencpll_380m		vencpll
#define lvdspll_ck		lvdspll

/* DIV */
#define clkrtc_ext		"clkrtc_ext"
#define clkrtc_int		"clkrtc_int"
#define fpc_ck			"fpc_ck"
#define hdmitxpll_d2		"hdmitxpll_d2"
#define hdmitxpll_d3		"hdmitxpll_d3"
#define armca7pll_d2		"armca7pll_d2"
#define armca7pll_d3		"armca7pll_d3"
#define apll1_ck		"apll1_ck"
#define apll2_ck		"apll2_ck"
#define dmpll_ck		"dmpll_ck"
#define dmpll_d2		"dmpll_d2"
#define dmpll_d4		"dmpll_d4"
#define dmpll_d8		"dmpll_d8"
#define dmpll_d16		"dmpll_d16"
#define lvdspll_d2		"lvdspll_d2"
#define lvdspll_d4		"lvdspll_d4"
#define lvdspll_d8		"lvdspll_d8"
#define mmpll_ck		"mmpll_ck"
#define mmpll_d2		"mmpll_d2"
#define msdcpll_ck		"msdcpll_ck"
#define msdcpll_d2		"msdcpll_d2"
#define msdcpll_d4		"msdcpll_d4"
#define msdcpll2_ck		"msdcpll2_ck"
#define msdcpll2_d2		"msdcpll2_d2"
#define msdcpll2_d4		"msdcpll2_d4"
#define ssusb_phyd_125m_ck	usb_syspll_125m
#define syspll_d2		"syspll_d2"
#define syspll1_d2		"syspll1_d2"
#define syspll1_d4		"syspll1_d4"
#define syspll1_d8		"syspll1_d8"
#define syspll1_d16		"syspll1_d16"
#define syspll_d3		"syspll_d3"
#define syspll2_d2		"syspll2_d2"
#define syspll2_d4		"syspll2_d4"
#define syspll_d5		"syspll_d5"
#define syspll3_d2		"syspll3_d2"
#define syspll3_d4		"syspll3_d4"
#define syspll_d7		"syspll_d7"
#define syspll4_d2		"syspll4_d2"
#define syspll4_d4		"syspll4_d4"
#define tvdpll_445p5m_ck	tvdpll_445p5m
#define tvdpll_ck		"tvdpll_ck"
#define tvdpll_d2		"tvdpll_d2"
#define tvdpll_d4		"tvdpll_d4"
#define tvdpll_d8		"tvdpll_d8"
#define tvdpll_d16		"tvdpll_d16"
#define univpll_d2		"univpll_d2"
#define univpll1_d2		"univpll1_d2"
#define univpll1_d4		"univpll1_d4"
#define univpll1_d8		"univpll1_d8"
#define univpll_d3		"univpll_d3"
#define univpll2_d2		"univpll2_d2"
#define univpll2_d4		"univpll2_d4"
#define univpll2_d8		"univpll2_d8"
#define univpll_d5		"univpll_d5"
#define univpll3_d2		"univpll3_d2"
#define univpll3_d4		"univpll3_d4"
#define univpll3_d8		"univpll3_d8"
#define univpll_d7		"univpll_d7"
#define univpll_d26		"univpll_d26"
#define univpll_d52		"univpll_d52"
#define vcodecpll_ck		"vcodecpll_ck"
#define vencpll_ck		"vencpll_ck"
#define vencpll_d2		"vencpll_d2"
#define vencpll_d4		"vencpll_d4"

/* TOP */
#define axi_sel			"axi_sel"
#define mem_sel			"mem_sel"
#define ddrphycfg_sel		"ddrphycfg_sel"
#define mm_sel			"mm_sel"
#define pwm_sel			"pwm_sel"
#define vdec_sel		"vdec_sel"
#define venc_sel		"venc_sel"
#define mfg_sel			"mfg_sel"
#define camtg_sel		"camtg_sel"
#define uart_sel		"uart_sel"
#define spi_sel			"spi_sel"
#define usb20_sel		"usb20_sel"
#define usb30_sel		"usb30_sel"
#define msdc50_0_h_sel		"msdc50_0_h_sel"
#define msdc50_0_sel		"msdc50_0_sel"
#define msdc30_1_sel		"msdc30_1_sel"
#define msdc30_2_sel		"msdc30_2_sel"
#define msdc30_3_sel		"msdc30_3_sel"
#define audio_sel		"audio_sel"
#define aud_intbus_sel		"aud_intbus_sel"
#define pmicspi_sel		"pmicspi_sel"
#define scp_sel			"scp_sel"
#define atb_sel			"atb_sel"
#define venclt_sel		"venclt_sel"
#define dpi0_sel		"dpi0_sel"
#define irda_sel		"irda_sel"
#define cci400_sel		"cci400_sel"
#define aud_1_sel		"aud_1_sel"
#define aud_2_sel		"aud_2_sel"
#define mem_mfg_in_sel		"mem_mfg_in_sel"
#define axi_mfg_in_sel		"axi_mfg_in_sel"
#define scam_sel		"scam_sel"
#define spinfi_ifr_sel		"spinfi_ifr_sel"
#define hdmi_sel		"hdmi_sel"
#define dpilvds_sel		"dpilvds_sel"
#define msdc50_2_h_sel		"msdc50_2_h_sel"
#define hdcp_sel		"hdcp_sel"
#define hdcp_24m_sel		"hdcp_24m_sel"
#define rtc_sel			"rtc_sel"

#define axi_ck			axi_sel
#define mfg_ck			mfg_sel

/* INFRA */
#define infra_pmicwrap		"infra_pmicwrap"
#define infra_pmicspi		"infra_pmicspi"
#define infra_cec		"infra_cec"
#define infra_kp		"infra_kp"
#define infra_cpum		"infra_cpum"
#define infra_m4u		"infra_m4u"
#define infra_l2c_sram		"infra_l2c_sram"
#define infra_gce		"infra_gce"
#define infra_audio		"infra_audio"
#define infra_smi		"infra_smi"
#define infra_dbgclk		"infra_dbgclk"

/* PERI0 */
#define peri_nfiecc		"peri_nfiecc"
#define peri_i2c5		"peri_i2c5"
#define peri_spi0		"peri_spi0"
#define peri_auxadc		"peri_auxadc"
#define peri_i2c4		"peri_i2c4"
#define peri_i2c3		"peri_i2c3"
#define peri_i2c2		"peri_i2c2"
#define peri_i2c1		"peri_i2c1"
#define peri_i2c0		"peri_i2c0"
#define peri_uart3		"peri_uart3"
#define peri_uart2		"peri_uart2"
#define peri_uart1		"peri_uart1"
#define peri_uart0		"peri_uart0"
#define peri_irda		"peri_irda"
#define peri_nli_arb		"peri_nli_arb"
#define peri_msdc30_3		"peri_msdc30_3"
#define peri_msdc30_2		"peri_msdc30_2"
#define peri_msdc30_1		"peri_msdc30_1"
#define peri_msdc30_0		"peri_msdc30_0"
#define peri_ap_dma		"peri_ap_dma"
#define peri_usb1		"peri_usb1"
#define peri_usb0		"peri_usb0"
#define peri_pwm		"peri_pwm"
#define peri_pwm7		"peri_pwm7"
#define peri_pwm6		"peri_pwm6"
#define peri_pwm5		"peri_pwm5"
#define peri_pwm4		"peri_pwm4"
#define peri_pwm3		"peri_pwm3"
#define peri_pwm2		"peri_pwm2"
#define peri_pwm1		"peri_pwm1"
#define peri_therm		"peri_therm"
#define peri_nfi		"peri_nfi"

/* PERI1 */
#define peri_i2c6		"peri_i2c6"
#define peri_irrx		"peri_irrx"
#define peri_spi		"peri_spi"

/* MFG */
#define mfg_axi			"mfg_axi"
#define mfg_mem			"mfg_mem"
#define mfg_g3d			"mfg_g3d"
#define mfg_26m			"mfg_26m"

/* IMG */
#define img_fd			"img_fd"
#define img_cam_sv		"img_cam_sv"
#define img_sen_cam		"img_sen_cam"
#define img_sen_tg		"img_sen_tg"
#define img_cam_cam		"img_cam_cam"
#define img_cam_smi		"img_cam_smi"
#define img_larb2_smi		"img_larb2_smi"

/* MM0 */
#define mm_smi_common		"mm_smi_common"
#define mm_smi_larb0		"mm_smi_larb0"
#define mm_cam_mdp		"mm_cam_mdp"
#define mm_mdp_rdma0		"mm_mdp_rdma0"
#define mm_mdp_rdma1		"mm_mdp_rdma1"
#define mm_mdp_rsz0		"mm_mdp_rsz0"
#define mm_mdp_rsz1		"mm_mdp_rsz1"
#define mm_mdp_rsz2		"mm_mdp_rsz2"
#define mm_mdp_tdshp0		"mm_mdp_tdshp0"
#define mm_mdp_tdshp1		"mm_mdp_tdshp1"
#define mm_mdp_wdma		"mm_mdp_wdma"
#define mm_mdp_wrot0		"mm_mdp_wrot0"
#define mm_mdp_wrot1		"mm_mdp_wrot1"
#define mm_fake_eng		"mm_fake_eng"
#define mm_mutex_32k		"mm_mutex_32k"
#define mm_disp_ovl0		"mm_disp_ovl0"
#define mm_disp_ovl1		"mm_disp_ovl1"
#define mm_disp_rdma0		"mm_disp_rdma0"
#define mm_disp_rdma1		"mm_disp_rdma1"
#define mm_disp_rdma2		"mm_disp_rdma2"
#define mm_disp_wdma0		"mm_disp_wdma0"
#define mm_disp_wdma1		"mm_disp_wdma1"
#define mm_disp_color0		"mm_disp_color0"
#define mm_disp_color1		"mm_disp_color1"
#define mm_disp_aal		"mm_disp_aal"
#define mm_disp_gamma		"mm_disp_gamma"
#define mm_disp_ufoe		"mm_disp_ufoe"
#define mm_disp_split0		"mm_disp_split0"
#define mm_disp_split1		"mm_disp_split1"
#define mm_disp_merge		"mm_disp_merge"
#define mm_disp_od		"mm_disp_od"

/* MM1 */
#define mm_disp_pwm0mm		"mm_disp_pwm0mm"
#define mm_disp_pwm026m		"mm_disp_pwm026m"
#define mm_disp_pwm1mm		"mm_disp_pwm1mm"
#define mm_disp_pwm126m		"mm_disp_pwm126m"
#define mm_dsi0_engine		"mm_dsi0_engine"
#define mm_dsi0_digital		"mm_dsi0_digital"
#define mm_dsi1_engine		"mm_dsi1_engine"
#define mm_dsi1_digital		"mm_dsi1_digital"
#define mm_dpi_pixel		"mm_dpi_pixel"
#define mm_dpi_engine		"mm_dpi_engine"
#define mm_dpi1_pixel		"mm_dpi1_pixel"
#define mm_dpi1_engine		"mm_dpi1_engine"
#define mm_hdmi_pixel		"mm_hdmi_pixel"
#define mm_hdmi_pllck		"mm_hdmi_pllck"
#define mm_hdmi_audio		"mm_hdmi_audio"
#define mm_hdmi_spdif		"mm_hdmi_spdif"
#define mm_lvds_pixel		"mm_lvds_pixel"
#define mm_lvds_cts		"mm_lvds_cts"
#define mm_smi_larb4		"mm_smi_larb4"
#define mm_hdmi_hdcp		"mm_hdmi_hdcp"
#define mm_hdmi_hdcp24m		"mm_hdmi_hdcp24m"

/* VDEC */
#define vdec_cken		"vdec_cken"
#define vdec_larb_cken		"vdec_larb_cken"

/* VENC */
#define venc_cke0		"venc_cke0"
#define venc_cke1		"venc_cke1"
#define venc_cke2		"venc_cke2"
#define venc_cke3		"venc_cke3"

/* VENCLT */
#define venclt_cke0		"venclt_cke0"
#define venclt_cke1		"venclt_cke1"

/* AUDIO */
#define aud_ahb_idle_in		"aud_ahb_idle_in"
#define aud_ahb_idle_ex		"aud_ahb_idle_ex"
#define aud_tml			"aud_tml"
#define aud_dac_predis		"aud_dac_predis"
#define aud_dac			"aud_dac"
#define aud_adc			"aud_adc"
#define aud_adda2		"aud_adda2"
#define aud_adda3		"aud_adda3"
#define aud_spdf		"aud_spdf"
#define aud_hdmi		"aud_hdmi"
#define aud_apll_tnr		"aud_apll_tnr"
#define aud_apll2_tnr		"aud_apll2_tnr"
#define aud_spdf2		"aud_spdf2"
#define aud_24m			"aud_24m"
#define aud_22m			"aud_22m"
#define aud_i2s			"aud_i2s"
#define aud_afe			"aud_afe"

struct mtk_fixed_factor {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
};

#define FACTOR(_id, _name, _parent, _mult, _div) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
	}

static void __init init_factors(struct mtk_fixed_factor *clks, int num,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < num; i++) {
		struct mtk_fixed_factor *ff = &clks[i];

		clk = clk_register_fixed_factor(NULL, ff->name, ff->parent_name,
				0, ff->mult, ff->div);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					ff->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[ff->id] = clk;

		pr_debug("factor %3d: %s\n", i, ff->name);
	}
}

static struct mtk_fixed_factor root_clk_alias[] __initdata = {
	FACTOR(TOP_CLKPH_MCK_O, clkph_mck_o, clk_null, 1, 1),
	FACTOR(TOP_DPI_CK, dpi_ck, clk_null, 1, 1),
	FACTOR(TOP_USB_SYSPLL_125M, usb_syspll_125m, clk_null, 1, 1),
	FACTOR(TOP_HDMITX_DIG_CTS, hdmitx_dig_cts, clk_null, 1, 1),
};

static void __init init_clk_root_alias(struct clk_onecell_data *clk_data)
{
	init_factors(root_clk_alias, ARRAY_SIZE(root_clk_alias), clk_data);
}

static struct mtk_fixed_factor top_divs[] __initdata = {
	FACTOR(TOP_ARMCA7PLL_754M, armca7pll_754m, armca7pll, 1, 2),
	FACTOR(TOP_ARMCA7PLL_502M, armca7pll_502m, armca7pll, 1, 3),

	FACTOR(TOP_MAIN_H546M, main_h546m, mainpll, 1, 2),
	FACTOR(TOP_MAIN_H364M, main_h364m, mainpll, 1, 3),
	FACTOR(TOP_MAIN_H218P4M, main_h218p4m, mainpll, 1, 5),
	FACTOR(TOP_MAIN_H156M, main_h156m, mainpll, 1, 7),

	FACTOR(TOP_TVDPLL_445P5M, tvdpll_445p5m, tvdpll, 1, 4),
	FACTOR(TOP_TVDPLL_594M, tvdpll_594m, tvdpll, 1, 3),

	FACTOR(TOP_UNIV_624M, univ_624m, univpll, 1, 2),
	FACTOR(TOP_UNIV_416M, univ_416m, univpll, 1, 3),
	FACTOR(TOP_UNIV_249P6M, univ_249p6m, univpll, 1, 5),
	FACTOR(TOP_UNIV_178P3M, univ_178p3m, univpll, 1, 7),
	FACTOR(TOP_UNIV_48M, univ_48m, univpll, 1, 26),

	FACTOR(TOP_CLKRTC_EXT, clkrtc_ext, clk32k, 1, 1),
	FACTOR(TOP_CLKRTC_INT, clkrtc_int, clk26m, 1, 793),
	FACTOR(TOP_FPC_CK, fpc_ck, clk26m, 1, 1),

	FACTOR(TOP_HDMITXPLL_D2, hdmitxpll_d2, hdmitx_dig_cts, 1, 2),
	FACTOR(TOP_HDMITXPLL_D3, hdmitxpll_d3, hdmitx_dig_cts, 1, 3),

	FACTOR(TOP_ARMCA7PLL_D2, armca7pll_d2, armca7pll_754m, 1, 1),
	FACTOR(TOP_ARMCA7PLL_D3, armca7pll_d3, armca7pll_502m, 1, 1),

	FACTOR(TOP_APLL1_CK, apll1_ck, apll1_180p633m, 1, 1),
	FACTOR(TOP_APLL2_CK, apll2_ck, apll2_196p608m, 1, 1),

	FACTOR(TOP_DMPLL_CK, dmpll_ck, clkph_mck_o, 1, 1),
	FACTOR(TOP_DMPLL_D2, dmpll_d2, clkph_mck_o, 1, 2),
	FACTOR(TOP_DMPLL_D4, dmpll_d4, clkph_mck_o, 1, 4),
	FACTOR(TOP_DMPLL_D8, dmpll_d8, clkph_mck_o, 1, 8),
	FACTOR(TOP_DMPLL_D16, dmpll_d16, clkph_mck_o, 1, 16),

	FACTOR(TOP_LVDSPLL_D2, lvdspll_d2, lvdspll, 1, 2),
	FACTOR(TOP_LVDSPLL_D4, lvdspll_d4, lvdspll, 1, 4),
	FACTOR(TOP_LVDSPLL_D8, lvdspll_d8, lvdspll, 1, 8),

	FACTOR(TOP_MMPLL_CK, mmpll_ck, mmpll_455m, 1, 1),
	FACTOR(TOP_MMPLL_D2, mmpll_d2, mmpll_455m, 1, 2),

	FACTOR(TOP_MSDCPLL_CK, msdcpll_ck, msdcpll_806m, 1, 1),
	FACTOR(TOP_MSDCPLL_D2, msdcpll_d2, msdcpll_806m, 1, 2),
	FACTOR(TOP_MSDCPLL_D4, msdcpll_d4, msdcpll_806m, 1, 4),
	FACTOR(TOP_MSDCPLL2_CK, msdcpll2_ck, msdcpll2, 1, 1),
	FACTOR(TOP_MSDCPLL2_D2, msdcpll2_d2, msdcpll2, 1, 2),
	FACTOR(TOP_MSDCPLL2_D4, msdcpll2_d4, msdcpll2, 1, 4),

	FACTOR(TOP_SYSPLL_D2, syspll_d2, main_h546m, 1, 1),
	FACTOR(TOP_SYSPLL1_D2, syspll1_d2, main_h546m, 1, 2),
	FACTOR(TOP_SYSPLL1_D4, syspll1_d4, main_h546m, 1, 4),
	FACTOR(TOP_SYSPLL1_D8, syspll1_d8, main_h546m, 1, 8),
	FACTOR(TOP_SYSPLL1_D16, syspll1_d16, main_h546m, 1, 16),
	FACTOR(TOP_SYSPLL_D3, syspll_d3, main_h364m, 1, 1),
	FACTOR(TOP_SYSPLL2_D2, syspll2_d2, main_h364m, 1, 2),
	FACTOR(TOP_SYSPLL2_D4, syspll2_d4, main_h364m, 1, 4),
	FACTOR(TOP_SYSPLL_D5, syspll_d5, main_h218p4m, 1, 1),
	FACTOR(TOP_SYSPLL3_D2, syspll3_d2, main_h218p4m, 1, 2),
	FACTOR(TOP_SYSPLL3_D4, syspll3_d4, main_h218p4m, 1, 4),
	FACTOR(TOP_SYSPLL_D7, syspll_d7, main_h156m, 1, 1),
	FACTOR(TOP_SYSPLL4_D2, syspll4_d2, main_h156m, 1, 2),
	FACTOR(TOP_SYSPLL4_D4, syspll4_d4, main_h156m, 1, 4),

	FACTOR(TOP_TVDPLL_CK, tvdpll_ck, tvdpll_594m, 1, 1),
	FACTOR(TOP_TVDPLL_D2, tvdpll_d2, tvdpll_594m, 1, 2),
	FACTOR(TOP_TVDPLL_D4, tvdpll_d4, tvdpll_594m, 1, 4),
	FACTOR(TOP_TVDPLL_D8, tvdpll_d8, tvdpll_594m, 1, 8),
	FACTOR(TOP_TVDPLL_D16, tvdpll_d16, tvdpll_594m, 1, 16),

	FACTOR(TOP_UNIVPLL_D2, univpll_d2, univ_624m, 1, 1),
	FACTOR(TOP_UNIVPLL1_D2, univpll1_d2, univ_624m, 1, 2),
	FACTOR(TOP_UNIVPLL1_D4, univpll1_d4, univ_624m, 1, 4),
	FACTOR(TOP_UNIVPLL1_D8, univpll1_d8, univ_624m, 1, 8),
	FACTOR(TOP_UNIVPLL_D3, univpll_d3, univ_416m, 1, 1),
	FACTOR(TOP_UNIVPLL2_D2, univpll2_d2, univ_416m, 1, 2),
	FACTOR(TOP_UNIVPLL2_D4, univpll2_d4, univ_416m, 1, 4),
	FACTOR(TOP_UNIVPLL2_D8, univpll2_d8, univ_416m, 1, 8),
	FACTOR(TOP_UNIVPLL_D5, univpll_d5, univ_249p6m, 1, 1),
	FACTOR(TOP_UNIVPLL3_D2, univpll3_d2, univ_249p6m, 1, 2),
	FACTOR(TOP_UNIVPLL3_D4, univpll3_d4, univ_249p6m, 1, 4),
	FACTOR(TOP_UNIVPLL3_D8, univpll3_d8, univ_249p6m, 1, 8),
	FACTOR(TOP_UNIVPLL_D7, univpll_d7, univ_178p3m, 1, 1),
	FACTOR(TOP_UNIVPLL_D26, univpll_d26, univ_48m, 1, 1),
	FACTOR(TOP_UNIVPLL_D52, univpll_d52, univ_48m, 1, 2),

	FACTOR(TOP_VCODECPLL_CK, vcodecpll_ck, vcodecpll, 1, 3),
	FACTOR(TOP_VCODECPLL_370P5, vcodecpll_370p5, vcodecpll, 1, 4),

	FACTOR(TOP_VENCPLL_CK, vencpll_ck, vencpll_380m, 1, 1),
	FACTOR(TOP_VENCPLL_D2, vencpll_d2, vencpll_380m, 1, 2),
	FACTOR(TOP_VENCPLL_D4, vencpll_d4, vencpll_380m, 1, 4),
};

static void __init init_clk_top_div(struct clk_onecell_data *clk_data)
{
	init_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
}

static const char *axi_parents[] __initconst = {
		clk26m,
		syspll1_d2,
		syspll_d5,
		syspll1_d4,
		univpll_d5,
		univpll2_d2,
		dmpll_d2,
		dmpll_d4};

static const char *mem_parents[] __initconst = {
		clk26m,
		dmpll_ck};

static const char *ddrphycfg_parents[] __initconst = {
		clk26m,
		syspll1_d8};

static const char *mm_parents[] __initconst = {
		clk26m,
		vencpll_d2,
		main_h364m,
		syspll1_d2,
		syspll_d5,
		syspll1_d4,
		univpll1_d2,
		univpll2_d2,
		dmpll_d2};

static const char *pwm_parents[] __initconst = {
		clk26m,
		univpll2_d4,
		univpll3_d2,
		univpll1_d4};

static const char *vdec_parents[] __initconst = {
		clk26m,
		vcodecpll_ck,
		tvdpll_445p5m_ck,
		univpll_d3,
		vencpll_d2,
		syspll_d3,
		univpll1_d2,
		mmpll_d2,
		dmpll_d2,
		dmpll_d4};

static const char *venc_parents[] __initconst = {
		clk26m,
		vcodecpll_ck,
		tvdpll_445p5m_ck,
		univpll_d3,
		vencpll_d2,
		syspll_d3,
		univpll1_d2,
		univpll2_d2,
		dmpll_d2,
		dmpll_d4};

static const char *mfg_parents[] __initconst = {
		clk26m,
		mmpll_ck,
		dmpll_ck,
		clk26m,
		clk26m,
		clk26m,
		clk26m,
		clk26m,
		clk26m,
		syspll_d3,
		syspll1_d2,
		syspll_d5,
		univpll_d3,
		univpll1_d2,
		univpll_d5,
		univpll2_d2};

static const char *camtg_parents[] __initconst = {
		clk26m,
		univpll_d26,
		univpll2_d2,
		syspll3_d2,
		syspll3_d4,
		univpll1_d4};

static const char *uart_parents[] __initconst = {
		clk26m,
		univpll2_d8};

static const char *spi_parents[] __initconst = {
		clk26m,
		syspll3_d2,
		syspll1_d4,
		syspll4_d2,
		univpll3_d2,
		univpll2_d4,
		univpll1_d8};

static const char *usb20_parents[] __initconst = {
		clk26m,
		univpll1_d8,
		univpll3_d4};

static const char *usb30_parents[] __initconst = {
		clk26m,
		univpll3_d2,
		ssusb_phyd_125m_ck,
		univpll2_d4};

static const char *msdc50_0_h_parents[] __initconst = {
		clk26m,
		syspll1_d2,
		syspll2_d2,
		syspll4_d2,
		univpll_d5,
		univpll1_d4};

static const char *msdc50_0_parents[] __initconst = {
		clk26m,
		msdcpll_ck,
		msdcpll_d2,
		univpll1_d4,
		syspll2_d2,
		syspll_d7,
		msdcpll_d4,
		vencpll_d4,
		tvdpll_ck,
		univpll_d2,
		univpll1_d2,
		mmpll_ck,
		msdcpll2_ck,
		msdcpll2_d2,
		msdcpll2_d4};

static const char *msdc30_1_parents[] __initconst = {
		clk26m,
		univpll2_d2,
		msdcpll_d4,
		univpll1_d4,
		syspll2_d2,
		syspll_d7,
		univpll_d7,
		vencpll_d4};

static const char *msdc30_2_parents[] __initconst = {
		clk26m,
		univpll2_d2,
		msdcpll_d4,
		univpll1_d4,
		syspll2_d2,
		syspll_d7,
		univpll_d7,
		vencpll_d2};

static const char *msdc30_3_parents[] __initconst = {
		clk26m,
		msdcpll2_ck,
		msdcpll2_d2,
		univpll2_d2,
		msdcpll2_d4,
		msdcpll_d4,
		univpll1_d4,
		syspll2_d2,
		syspll_d7,
		univpll_d7,
		vencpll_d4,
		msdcpll_ck,
		msdcpll_d2,
		msdcpll_d4};

static const char *audio_parents[] __initconst = {
		clk26m,
		syspll3_d4,
		syspll4_d4,
		syspll1_d16};

static const char *aud_intbus_parents[] __initconst = {
		clk26m,
		syspll1_d4,
		syspll4_d2,
		univpll3_d2,
		univpll2_d8,
		dmpll_d4,
		dmpll_d8};

static const char *pmicspi_parents[] __initconst = {
		clk26m,
		syspll1_d8,
		syspll3_d4,
		syspll1_d16,
		univpll3_d4,
		univpll_d26,
		dmpll_d8,
		dmpll_d16};

static const char *scp_parents[] __initconst = {
		clk26m,
		syspll1_d2,
		univpll_d5,
		syspll_d5,
		dmpll_d2,
		dmpll_d4};

static const char *atb_parents[] __initconst = {
		clk26m,
		syspll1_d2,
		univpll_d5,
		dmpll_d2};

static const char *venc_lt_parents[] __initconst = {
		clk26m,
		univpll_d3,
		vcodecpll_ck,
		tvdpll_445p5m_ck,
		vencpll_d2,
		syspll_d3,
		univpll1_d2,
		univpll2_d2,
		syspll1_d2,
		univpll_d5,
		vcodecpll_370p5,
		dmpll_ck};

static const char *dpi0_parents[] __initconst = {
		clk26m,
		tvdpll_d2,
		tvdpll_d4,
		clk26m,
		clk26m,
		tvdpll_d8,
		tvdpll_d16};

static const char *irda_parents[] __initconst = {
		clk26m,
		univpll2_d4,
		syspll2_d4};

static const char *cci400_parents[] __initconst = {
		clk26m,
		vencpll_ck,
		armca7pll_754m,
		armca7pll_502m,
		univpll_d2,
		syspll_d2,
		msdcpll_ck,
		dmpll_ck};

static const char *aud_1_parents[] __initconst = {
		clk26m,
		apll1_ck,
		univpll2_d4,
		univpll2_d8};

static const char *aud_2_parents[] __initconst = {
		clk26m,
		apll2_ck,
		univpll2_d4,
		univpll2_d8};

static const char *mem_mfg_in_parents[] __initconst = {
		clk26m,
		mmpll_ck,
		dmpll_ck,
		clk26m};

static const char *axi_mfg_in_parents[] __initconst = {
		clk26m,
		axi_ck,
		dmpll_d2};

static const char *scam_parents[] __initconst = {
		clk26m,
		syspll3_d2,
		univpll2_d4,
		dmpll_d4};

static const char *spinfi_ifr_parents[] __initconst = {
		clk26m,
		univpll2_d8,
		univpll3_d4,
		syspll4_d2,
		univpll2_d4,
		univpll3_d2,
		syspll1_d4,
		univpll1_d4};

static const char *hdmi_parents[] __initconst = {
		clk26m,
		hdmitx_dig_cts,
		hdmitxpll_d2,
		hdmitxpll_d3};

static const char *dpilvds_parents[] __initconst = {
		clk26m,
		lvdspll_ck,
		lvdspll_d2,
		lvdspll_d4,
		lvdspll_d8,
		fpc_ck};

static const char *msdc50_2_h_parents[] __initconst = {
		clk26m,
		syspll1_d2,
		syspll2_d2,
		syspll4_d2,
		univpll_d5,
		univpll1_d4};

static const char *hdcp_parents[] __initconst = {
		clk26m,
		syspll4_d2,
		syspll3_d4,
		univpll2_d4};

static const char *hdcp_24m_parents[] __initconst = {
		clk26m,
		univpll_d26,
		univpll_d52,
		univpll2_d8};

static const char *rtc_parents[] __initconst = {
		clkrtc_int,
		clkrtc_ext,
		clk26m,
		univpll3_d8};

struct mtk_mux {
	int id;
	const char *name;
	uint32_t reg;
	int shift;
	int width;
	int gate;
	const char **parent_names;
	int num_parents;
};

#define MUX(_id, _name, _parents, _reg, _shift, _width, _gate) {	\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.shift = _shift,					\
		.width = _width,					\
		.gate = _gate,						\
		.parent_names = (const char **)_parents,		\
		.num_parents = ARRAY_SIZE(_parents),			\
	}

static struct mtk_mux top_muxes[] __initdata = {
	/* CLK_CFG_0 */
	MUX(TOP_AXI_SEL, axi_sel, axi_parents,
		0x0040, 0, 3, INVALID_MUX_GATE_BIT /* 7 */),
	MUX(TOP_MEM_SEL, mem_sel, mem_parents,
		0x0040, 8, 1, INVALID_MUX_GATE_BIT /* 15 */),
	MUX(TOP_DDRPHYCFG_SEL, ddrphycfg_sel, ddrphycfg_parents,
		0x0040, 16, 1, 23),
	MUX(TOP_MM_SEL, mm_sel, mm_parents, 0x0040, 24, 4, 31),
	/* CLK_CFG_1 */
	MUX(TOP_PWM_SEL, pwm_sel, pwm_parents, 0x0050, 0, 2, 7),
	MUX(TOP_VDEC_SEL, vdec_sel, vdec_parents, 0x0050, 8, 4, 15),
	MUX(TOP_VENC_SEL, venc_sel, venc_parents, 0x0050, 16, 4, 23),
	MUX(TOP_MFG_SEL, mfg_sel, mfg_parents, 0x0050, 24, 4, 31),
	/* CLK_CFG_2 */
	MUX(TOP_CAMTG_SEL, camtg_sel, camtg_parents, 0x0060, 0, 3, 7),
	MUX(TOP_UART_SEL, uart_sel, uart_parents, 0x0060, 8, 1, 15),
	MUX(TOP_SPI_SEL, spi_sel, spi_parents, 0x0060, 16, 3, 23),
	MUX(TOP_USB20_SEL, usb20_sel, usb20_parents, 0x0060, 24, 2, 31),
	/* CLK_CFG_3 */
	MUX(TOP_USB30_SEL, usb30_sel, usb30_parents, 0x0070, 0, 2, 7),
	MUX(TOP_MSDC50_0_H_SEL, msdc50_0_h_sel, msdc50_0_h_parents,
		0x0070, 8, 3, 15),
	MUX(TOP_MSDC50_0_SEL, msdc50_0_sel, msdc50_0_parents,
		0x0070, 16, 4, 23),
	MUX(TOP_MSDC30_1_SEL, msdc30_1_sel, msdc30_1_parents,
		0x0070, 24, 3, 31),
	/* CLK_CFG_4 */
	MUX(TOP_MSDC30_2_SEL, msdc30_2_sel, msdc30_2_parents, 0x0080, 0, 3, 7),
	MUX(TOP_MSDC30_3_SEL, msdc30_3_sel, msdc30_3_parents, 0x0080, 8, 4, 15),
	MUX(TOP_AUDIO_SEL, audio_sel, audio_parents, 0x0080, 16, 2, 23),
	MUX(TOP_AUD_INTBUS_SEL, aud_intbus_sel, aud_intbus_parents,
		0x0080, 24, 3, 31),
	/* CLK_CFG_5 */
	MUX(TOP_PMICSPI_SEL, pmicspi_sel, pmicspi_parents,
		0x0090, 0, 3, 7 /* 7:5 */),
	MUX(TOP_SCP_SEL, scp_sel, scp_parents, 0x0090, 8, 3, 15),
	MUX(TOP_ATB_SEL, atb_sel, atb_parents, 0x0090, 16, 2, 23),
	MUX(TOP_VENC_LT_SEL, venclt_sel, venc_lt_parents, 0x0090, 24, 4, 31),
	/* CLK_CFG_6 */
	MUX(TOP_DPI0_SEL, dpi0_sel, dpi0_parents, 0x00a0, 0, 3, 7),
	MUX(TOP_IRDA_SEL, irda_sel, irda_parents, 0x00a0, 8, 2, 15),
	MUX(TOP_CCI400_SEL, cci400_sel, cci400_parents, 0x00a0, 16, 3, 23),
	MUX(TOP_AUD_1_SEL, aud_1_sel, aud_1_parents, 0x00a0, 24, 2, 31),
	/* CLK_CFG_7 */
	MUX(TOP_AUD_2_SEL, aud_2_sel, aud_2_parents, 0x00b0, 0, 2, 7),
	MUX(TOP_MEM_MFG_IN_SEL, mem_mfg_in_sel, mem_mfg_in_parents,
		0x00b0, 8, 2, 15),
	MUX(TOP_AXI_MFG_IN_SEL, axi_mfg_in_sel, axi_mfg_in_parents,
		0x00b0, 16, 2, 23),
	MUX(TOP_SCAM_SEL, scam_sel, scam_parents, 0x00b0, 24, 2, 31),
	/* CLK_CFG_12 */
	MUX(TOP_SPINFI_IFR_SEL, spinfi_ifr_sel, spinfi_ifr_parents,
		0x00c0, 0, 3, 7),
	MUX(TOP_HDMI_SEL, hdmi_sel, hdmi_parents, 0x00c0, 8, 2, 15),
	MUX(TOP_DPILVDS_SEL, dpilvds_sel, dpilvds_parents, 0x00c0, 24, 3, 31),
	/* CLK_CFG_13 */
	MUX(TOP_MSDC50_2_H_SEL, msdc50_2_h_sel, msdc50_2_h_parents,
		0x00d0, 0, 3, 7),
	MUX(TOP_HDCP_SEL, hdcp_sel, hdcp_parents, 0x00d0, 8, 2, 15),
	MUX(TOP_HDCP_24M_SEL, hdcp_24m_sel, hdcp_24m_parents,
		0x00d0, 16, 2, 23),
	MUX(TOP_RTC_SEL, rtc_sel, rtc_parents,
		0x00d0, 24, 2, INVALID_MUX_GATE_BIT /* 31 */),
};

static void __init init_clk_topckgen(void __iomem *top_base,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(top_muxes); i++) {
		struct mtk_mux *mux = &top_muxes[i];

		clk = mtk_clk_register_mux(mux->name,
			mux->parent_names, mux->num_parents,
			top_base + mux->reg, mux->shift, mux->width, mux->gate);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					mux->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mux->id] = clk;

		pr_debug("mux %3d: %s\n", i, mux->name);
	}
}

struct mtk_pll {
	int id;
	const char *name;
	const char *parent_name;
	uint32_t reg;
	uint32_t pwr_reg;
	uint32_t en_mask;
	unsigned int flags;
	const struct clk_ops *ops;
};

#define PLL(_id, _name, _parent, _reg, _pwr_reg, _en_mask, _flags, _ops) { \
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.ops = _ops,						\
	}

static struct mtk_pll plls[] __initdata = {
	PLL(APMIXED_ARMCA15PLL, armca15pll, clk26m, 0x0200, 0x020c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_arm_pll_ops),
	PLL(APMIXED_ARMCA7PLL, armca7pll, clk26m, 0x0210, 0x021c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_arm_pll_ops),
	PLL(APMIXED_MAINPLL, mainpll, clk26m, 0x0220, 0x022c, 0xf0000101,
		HAVE_PLL_HP | HAVE_RST_BAR, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_UNIVPLL, univpll, clk26m, 0x0230, 0x023c, 0xfe000001,
		HAVE_RST_BAR | HAVE_FIX_FRQ | PLL_AO, &mt_clk_univ_pll_ops),
	PLL(APMIXED_MMPLL, mmpll, clk26m, 0x0240, 0x024c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_mm_pll_ops),
	PLL(APMIXED_MSDCPLL, msdcpll, clk26m, 0x0250, 0x025c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_VENCPLL, vencpll, clk26m, 0x0260, 0x026c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_TVDPLL, tvdpll, clk26m, 0x0270, 0x027c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_MPLL, mpll, clk26m, 0x0280, 0x028c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_VCODECPLL, vcodecpll, clk26m, 0x0290, 0x029c, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_APLL1, apll1, clk26m, 0x02a0, 0x02b0, 0x00000001,
		HAVE_PLL_HP, &mt_clk_aud_pll_ops),
	PLL(APMIXED_APLL2, apll2, clk26m, 0x02b4, 0x02c4, 0x00000001,
		HAVE_PLL_HP, &mt_clk_aud_pll_ops),
	PLL(APMIXED_LVDSPLL, lvdspll, clk26m, 0x02d0, 0x02dc, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
	PLL(APMIXED_MSDCPLL2, msdcpll2, clk26m, 0x02f0, 0x02fc, 0x00000001,
		HAVE_PLL_HP, &mt_clk_sdm_pll_ops),
};

static void __init init_clk_apmixedsys(void __iomem *apmixed_base,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		struct mtk_pll *pll = &plls[i];

		clk = mtk_clk_register_pll(pll->name, pll->parent_name,
				apmixed_base + pll->reg,
				apmixed_base + pll->pwr_reg,
				pll->en_mask, pll->flags, pll->ops);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pll->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pll->id] = clk;

		pr_debug("pll %3d: %s\n", i, pll->name);
	}
}

struct mtk_gate_regs {
	u32 sta_ofs;
	u32 clr_ofs;
	u32 set_ofs;
};

struct mtk_gate {
	int id;
	const char *name;
	const char *parent_name;
	struct mtk_gate_regs *regs;
	int shift;
	uint32_t flags;
};

#define GATE(_id, _name, _parent, _regs, _shift, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
	}

static void __init init_clk_gates(
		void __iomem *reg_base,
		struct mtk_gate *clks, int num,
		struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < num; i++) {
		struct mtk_gate *gate = &clks[i];

		clk = mtk_clk_register_gate(gate->name, gate->parent_name,
				reg_base + gate->regs->set_ofs,
				reg_base + gate->regs->clr_ofs,
				reg_base + gate->regs->sta_ofs,
				gate->shift, gate->flags);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					gate->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[gate->id] = clk;

		pr_debug("gate %3d: %s\n", i, gate->name);
	}
}

static struct mtk_gate_regs infra_cg_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x0048,
};

static struct mtk_gate infra_clks[] __initdata = {
	GATE(INFRA_DBGCLK, infra_dbgclk, axi_sel, infra_cg_regs, 0, 0),
	GATE(INFRA_SMI, infra_smi, mm_sel, infra_cg_regs, 1, 0),
	GATE(INFRA_AUDIO, infra_audio, aud_intbus_sel, infra_cg_regs, 5, 0),
	GATE(INFRA_GCE, infra_gce, axi_sel, infra_cg_regs, 6, 0),
	GATE(INFRA_L2C_SRAM, infra_l2c_sram, axi_sel, infra_cg_regs, 7, 0),
	GATE(INFRA_M4U, infra_m4u, mem_sel, infra_cg_regs, 8, 0),
	GATE(INFRA_CPUM, infra_cpum, clk_null, infra_cg_regs, 15, 0),
	GATE(INFRA_KP, infra_kp, axi_sel, infra_cg_regs, 16, 0),
	GATE(INFRA_CEC, infra_cec, clk26m, infra_cg_regs, 18, 0),
	GATE(INFRA_PMICSPI, infra_pmicspi, pmicspi_sel, infra_cg_regs, 22, 0),
	GATE(INFRA_PMICWRAP, infra_pmicwrap, axi_sel, infra_cg_regs, 23, 0),
};

static void __init init_clk_infrasys(void __iomem *infrasys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init infrasys gates:\n");
	init_clk_gates(infrasys_base, infra_clks, ARRAY_SIZE(infra_clks),
		clk_data);
}

static struct mtk_gate_regs peri0_cg_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x0010,
	.sta_ofs = 0x0018,
};

static struct mtk_gate_regs peri1_cg_regs = {
	.set_ofs = 0x000c,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x001c,
};

static struct mtk_gate peri_clks[] __initdata = {
	/* PERI0 */
	GATE(PERI_NFI, peri_nfi, axi_sel, peri0_cg_regs, 0, 0),
	GATE(PERI_THERM, peri_therm, axi_sel, peri0_cg_regs, 1, 0),
	GATE(PERI_PWM1, peri_pwm1, axi_sel, peri0_cg_regs, 2, 0),
	GATE(PERI_PWM2, peri_pwm2, axi_sel, peri0_cg_regs, 3, 0),
	GATE(PERI_PWM3, peri_pwm3, axi_sel, peri0_cg_regs, 4, 0),
	GATE(PERI_PWM4, peri_pwm4, axi_sel, peri0_cg_regs, 5, 0),
	GATE(PERI_PWM5, peri_pwm5, axi_sel, peri0_cg_regs, 6, 0),
	GATE(PERI_PWM6, peri_pwm6, axi_sel, peri0_cg_regs, 7, 0),
	GATE(PERI_PWM7, peri_pwm7, axi_sel, peri0_cg_regs, 8, 0),
	GATE(PERI_PWM, peri_pwm, axi_sel, peri0_cg_regs, 9, 0),
	GATE(PERI_USB0, peri_usb0, usb20_sel, peri0_cg_regs, 10, 0),
	GATE(PERI_USB1, peri_usb1, usb20_sel, peri0_cg_regs, 11, 0),
	GATE(PERI_AP_DMA, peri_ap_dma, axi_sel, peri0_cg_regs, 12, 0),
	GATE(PERI_MSDC30_0, peri_msdc30_0, msdc50_0_sel, peri0_cg_regs, 13, 0),
	GATE(PERI_MSDC30_1, peri_msdc30_1, msdc30_1_sel, peri0_cg_regs, 14, 0),
	GATE(PERI_MSDC30_2, peri_msdc30_2, msdc30_2_sel, peri0_cg_regs, 15, 0),
	GATE(PERI_MSDC30_3, peri_msdc30_3, msdc30_3_sel, peri0_cg_regs, 16, 0),
	GATE(PERI_NLI_ARB, peri_nli_arb, axi_sel, peri0_cg_regs, 17, 0),
	GATE(PERI_IRDA, peri_irda, irda_sel, peri0_cg_regs, 18, 0),
	GATE(PERI_UART0, peri_uart0, uart_sel, peri0_cg_regs, 19, 0),
	GATE(PERI_UART1, peri_uart1, uart_sel, peri0_cg_regs, 20, 0),
	GATE(PERI_UART2, peri_uart2, uart_sel, peri0_cg_regs, 21, 0),
	GATE(PERI_UART3, peri_uart3, uart_sel, peri0_cg_regs, 22, 0),
	GATE(PERI_I2C0, peri_i2c0, axi_sel, peri0_cg_regs, 23, 0),
	GATE(PERI_I2C1, peri_i2c1, axi_sel, peri0_cg_regs, 24, 0),
	GATE(PERI_I2C2, peri_i2c2, axi_sel, peri0_cg_regs, 25, 0),
	GATE(PERI_I2C3, peri_i2c3, axi_sel, peri0_cg_regs, 26, 0),
	GATE(PERI_I2C4, peri_i2c4, axi_sel, peri0_cg_regs, 27, 0),
	GATE(PERI_AUXADC, peri_auxadc, clk26m, peri0_cg_regs, 28, 0),
	GATE(PERI_SPI0, peri_spi0, spi_sel, peri0_cg_regs, 29, 0),
	GATE(PERI_I2C5, peri_i2c5, axi_sel, peri0_cg_regs, 30, 0),
	GATE(PERI_NFIECC, peri_nfiecc, axi_sel, peri0_cg_regs, 31, 0),
	/* PERI1 */
	GATE(PERI_SPI, peri_spi, spi_sel, peri1_cg_regs, 0, 0),
	GATE(PERI_IRRX, peri_irrx, spi_sel, peri1_cg_regs, 1, 0),
	GATE(PERI_I2C6, peri_i2c6, axi_sel, peri1_cg_regs, 2, 0),
};

static void __init init_clk_perisys(void __iomem *perisys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init perisys gates:\n");
	init_clk_gates(perisys_base, peri_clks, ARRAY_SIZE(peri_clks),
		clk_data);
}

static struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate mfg_clks[] __initdata = {
	GATE(MFG_AXI, mfg_axi, axi_sel, mfg_cg_regs, 0, 0),
	GATE(MFG_MEM, mfg_mem, mem_sel, mfg_cg_regs, 1, 0),
	GATE(MFG_G3D, mfg_g3d, mfg_sel, mfg_cg_regs, 2, 0),
	GATE(MFG_26M, mfg_26m, clk26m, mfg_cg_regs, 3, 0),
};

static void __init init_clk_mfgsys(void __iomem *mfgsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init mfgsys gates:\n");
	init_clk_gates(mfgsys_base, mfg_clks, ARRAY_SIZE(mfg_clks), clk_data);
}

static struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate img_clks[] __initdata = {
	GATE(IMG_LARB2_SMI, img_larb2_smi, mm_sel, img_cg_regs, 0, 0),
	GATE(IMG_CAM_SMI, img_cam_smi, mm_sel, img_cg_regs, 5, 0),
	GATE(IMG_CAM_CAM, img_cam_cam, mm_sel, img_cg_regs, 6, 0),
	GATE(IMG_SEN_TG, img_sen_tg, camtg_sel, img_cg_regs, 7, 0),
	GATE(IMG_SEN_CAM, img_sen_cam, mm_sel, img_cg_regs, 8, 0),
	GATE(IMG_CAM_SV, img_cam_sv, mm_sel, img_cg_regs, 9, 0),
	GATE(IMG_FD, img_fd, mm_sel, img_cg_regs, 11, 0),
};

static void __init init_clk_imgsys(void __iomem *imgsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init imgsys gates:\n");
	init_clk_gates(imgsys_base, img_clks, ARRAY_SIZE(img_clks), clk_data);
}

static struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x0104,
	.clr_ofs = 0x0108,
	.sta_ofs = 0x0100,
};

static struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x0114,
	.clr_ofs = 0x0118,
	.sta_ofs = 0x0110,
};

static struct mtk_gate mm_clks[] __initdata = {
	/* MM0 */
	GATE(MM_SMI_COMMON, mm_smi_common, mm_sel, mm0_cg_regs, 0, 0),
	GATE(MM_SMI_LARB0, mm_smi_larb0, mm_sel, mm0_cg_regs, 1, 0),
	GATE(MM_CAM_MDP, mm_cam_mdp, mm_sel, mm0_cg_regs, 2, 0),
	GATE(MM_MDP_RDMA0, mm_mdp_rdma0, mm_sel, mm0_cg_regs, 3, 0),
	GATE(MM_MDP_RDMA1, mm_mdp_rdma1, mm_sel, mm0_cg_regs, 4, 0),
	GATE(MM_MDP_RSZ0, mm_mdp_rsz0, mm_sel, mm0_cg_regs, 5, 0),
	GATE(MM_MDP_RSZ1, mm_mdp_rsz1, mm_sel, mm0_cg_regs, 6, 0),
	GATE(MM_MDP_RSZ2, mm_mdp_rsz2, mm_sel, mm0_cg_regs, 7, 0),
	GATE(MM_MDP_TDSHP0, mm_mdp_tdshp0, mm_sel, mm0_cg_regs, 8, 0),
	GATE(MM_MDP_TDSHP1, mm_mdp_tdshp1, mm_sel, mm0_cg_regs, 9, 0),
	GATE(MM_MDP_WDMA, mm_mdp_wdma, mm_sel, mm0_cg_regs, 11, 0),
	GATE(MM_MDP_WROT0, mm_mdp_wrot0, mm_sel, mm0_cg_regs, 12, 0),
	GATE(MM_MDP_WROT1, mm_mdp_wrot1, mm_sel, mm0_cg_regs, 13, 0),
	GATE(MM_FAKE_ENG, mm_fake_eng, mm_sel, mm0_cg_regs, 14, 0),
	GATE(MM_MUTEX_32K, mm_mutex_32k, rtc_sel, mm0_cg_regs, 15, 0),
	GATE(MM_DISP_OVL0, mm_disp_ovl0, mm_sel, mm0_cg_regs, 16, 0),
	GATE(MM_DISP_OVL1, mm_disp_ovl1, mm_sel, mm0_cg_regs, 17, 0),
	GATE(MM_DISP_RDMA0, mm_disp_rdma0, mm_sel, mm0_cg_regs, 18, 0),
	GATE(MM_DISP_RDMA1, mm_disp_rdma1, mm_sel, mm0_cg_regs, 19, 0),
	GATE(MM_DISP_RDMA2, mm_disp_rdma2, mm_sel, mm0_cg_regs, 20, 0),
	GATE(MM_DISP_WDMA0, mm_disp_wdma0, mm_sel, mm0_cg_regs, 21, 0),
	GATE(MM_DISP_WDMA1, mm_disp_wdma1, mm_sel, mm0_cg_regs, 22, 0),
	GATE(MM_DISP_COLOR0, mm_disp_color0, mm_sel, mm0_cg_regs, 23, 0),
	GATE(MM_DISP_COLOR1, mm_disp_color1, mm_sel, mm0_cg_regs, 24, 0),
	GATE(MM_DISP_AAL, mm_disp_aal, mm_sel, mm0_cg_regs, 25, 0),
	GATE(MM_DISP_GAMMA, mm_disp_gamma, mm_sel, mm0_cg_regs, 26, 0),
	GATE(MM_DISP_UFOE, mm_disp_ufoe, mm_sel, mm0_cg_regs, 27, 0),
	GATE(MM_DISP_SPLIT0, mm_disp_split0, mm_sel, mm0_cg_regs, 28, 0),
	GATE(MM_DISP_SPLIT1, mm_disp_split1, mm_sel, mm0_cg_regs, 29, 0),
	GATE(MM_DISP_MERGE, mm_disp_merge, mm_sel, mm0_cg_regs, 30, 0),
	GATE(MM_DISP_OD, mm_disp_od, mm_sel, mm0_cg_regs, 31, 0),
	/* MM1 */
	GATE(MM_DISP_PWM0MM, mm_disp_pwm0mm, mm_sel, mm1_cg_regs, 0, 0),
	GATE(MM_DISP_PWM026M, mm_disp_pwm026m, clk26m, mm1_cg_regs, 1, 0),
	GATE(MM_DISP_PWM1MM, mm_disp_pwm1mm, mm_sel, mm1_cg_regs, 2, 0),
	GATE(MM_DISP_PWM126M, mm_disp_pwm126m, clk26m, mm1_cg_regs, 3, 0),
	GATE(MM_DSI0_ENGINE, mm_dsi0_engine, mm_sel, mm1_cg_regs, 4, 0),
	GATE(MM_DSI0_DIGITAL, mm_dsi0_digital, clk_null, mm1_cg_regs, 5, 0),
	GATE(MM_DSI1_ENGINE, mm_dsi1_engine, mm_sel, mm1_cg_regs, 6, 0),
	GATE(MM_DSI1_DIGITAL, mm_dsi1_digital, clk_null, mm1_cg_regs, 7, 0),
	GATE(MM_DPI_PIXEL, mm_dpi_pixel, dpi0_sel, mm1_cg_regs, 8, 0),
	GATE(MM_DPI_ENGINE, mm_dpi_engine, mm_sel, mm1_cg_regs, 9, 0),
	GATE(MM_DPI1_PIXEL, mm_dpi1_pixel, clk_null, mm1_cg_regs, 10, 0),
	GATE(MM_DPI1_ENGINE, mm_dpi1_engine, mm_sel, mm1_cg_regs, 11, 0),
	GATE(MM_HDMI_PIXEL, mm_hdmi_pixel, dpi0_sel, mm1_cg_regs, 12, 0),
	GATE(MM_HDMI_PLLCK, mm_hdmi_pllck, hdmi_sel, mm1_cg_regs, 13, 0),
	GATE(MM_HDMI_AUDIO, mm_hdmi_audio, apll1, mm1_cg_regs, 14, 0),
	GATE(MM_HDMI_SPDIF, mm_hdmi_spdif, apll2, mm1_cg_regs, 15, 0),
	GATE(MM_LVDS_PIXEL, mm_lvds_pixel, clk_null, mm1_cg_regs, 16, 0),
	GATE(MM_LVDS_CTS, mm_lvds_cts, clk_null, mm1_cg_regs, 17, 0),
	GATE(MM_SMI_LARB4, mm_smi_larb4, mm_sel, mm1_cg_regs, 18, 0),
	GATE(MM_HDMI_HDCP, mm_hdmi_hdcp, hdcp_sel, mm1_cg_regs, 19, 0),
	GATE(MM_HDMI_HDCP24M, mm_hdmi_hdcp24m, hdcp_24m_sel,
		mm1_cg_regs, 20, 0),
};

static void __init init_clk_mmsys(void __iomem *mmsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init mmsys gates:\n");
	init_clk_gates(mmsys_base, mm_clks, ARRAY_SIZE(mm_clks),
		clk_data);
}

static struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0004,
	.sta_ofs = 0x0000,
};

static struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000c,
	.sta_ofs = 0x0008,
};

static struct mtk_gate vdec_clks[] __initdata = {
	GATE(VDEC_CKEN, vdec_cken, vdec_sel,
		vdec0_cg_regs, 0, CLK_GATE_INVERSE),
	GATE(VDEC_LARB_CKEN, vdec_larb_cken, mm_sel,
		vdec1_cg_regs, 0, CLK_GATE_INVERSE),
};

static void __init init_clk_vdecsys(void __iomem *vdecsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init vdecsys gates:\n");
	init_clk_gates(vdecsys_base, vdec_clks, ARRAY_SIZE(vdec_clks),
		clk_data);
}

static struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate venc_clks[] __initdata = {
	GATE(VENC_CKE0, venc_cke0, mm_sel, venc_cg_regs, 0, CLK_GATE_INVERSE),
	GATE(VENC_CKE1, venc_cke1, venc_sel, venc_cg_regs, 4, CLK_GATE_INVERSE),
	GATE(VENC_CKE2, venc_cke2, venc_sel, venc_cg_regs, 8, CLK_GATE_INVERSE),
	GATE(VENC_CKE3, venc_cke3, venc_sel,
		venc_cg_regs, 12, CLK_GATE_INVERSE),
};

static void __init init_clk_vencsys(void __iomem *vencsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init vencsys gates:\n");
	init_clk_gates(vencsys_base, venc_clks, ARRAY_SIZE(venc_clks),
		clk_data);
}

static struct mtk_gate_regs venclt_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

static struct mtk_gate venclt_clks[] __initdata = {
	GATE(VENCLT_CKE0, venclt_cke0, mm_sel,
		venclt_cg_regs, 0, CLK_GATE_INVERSE),
	GATE(VENCLT_CKE1, venclt_cke1, venclt_sel,
		venclt_cg_regs, 4, CLK_GATE_INVERSE),
};

static void __init init_clk_vencltsys(void __iomem *vencltsys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init vencltsys gates:\n");
	init_clk_gates(vencltsys_base, venclt_clks, ARRAY_SIZE(venclt_clks),
		clk_data);
}

static struct mtk_gate_regs aud_cg_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0000,
	.sta_ofs = 0x0000,
};

static struct mtk_gate audio_clks[] __initdata = {
	GATE(AUD_AFE, aud_afe, audio_sel,
		aud_cg_regs, 2, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_I2S, aud_i2s, clk_null,
		aud_cg_regs, 6, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_22M, aud_22m, apll1_ck,
		aud_cg_regs, 8, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_24M, aud_24m, apll2_ck,
		aud_cg_regs, 9, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_SPDF2, aud_spdf2, clk_null,
		aud_cg_regs, 11, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_APLL2_TNR, aud_apll2_tnr, apll2_ck,
		aud_cg_regs, 18, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_APLL_TNR, aud_apll_tnr, apll1_ck,
		aud_cg_regs, 19, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_HDMI, aud_hdmi, clk_null,
		aud_cg_regs, 20, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_SPDF, aud_spdf, clk_null,
		aud_cg_regs, 21, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_ADDA3, aud_adda3, audio_sel,
		aud_cg_regs, 22, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_ADDA2, aud_adda2, audio_sel,
		aud_cg_regs, 23, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_ADC, aud_adc, audio_sel,
		aud_cg_regs, 24, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_DAC, aud_dac, audio_sel,
		aud_cg_regs, 25, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_DAC_PREDIS, aud_dac_predis,
		audio_sel, aud_cg_regs, 26, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_TML, aud_tml, audio_sel,
		aud_cg_regs, 27, CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_AHB_IDLE_EX, aud_ahb_idle_ex, axi_sel,
		aud_cg_regs, 29, CLK_GATE_INVERSE | CLK_GATE_NO_SETCLR_REG),
	GATE(AUD_AHB_IDLE_IN, aud_ahb_idle_in, axi_sel,
		aud_cg_regs, 30, CLK_GATE_INVERSE | CLK_GATE_NO_SETCLR_REG),
};

static void __init init_clk_audiosys(void __iomem *audiosys_base,
		struct clk_onecell_data *clk_data)
{
	pr_debug("init audiosys gates:\n");
	init_clk_gates(audiosys_base, audio_clks, ARRAY_SIZE(audio_clks),
		clk_data);
}

/*
 * device tree support
 */

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

static void __iomem *get_reg(struct device_node *np, int index)
{
#if DUMMY_REG_TEST
	return kzalloc(PAGE_SIZE, GFP_KERNEL);
#else
	return of_iomap(np, index);
#endif
}

static void __init mt_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap topckgen failed\n");
		return;
	}

	clk_data = alloc_clk_data(TOP_NR_CLK);

	init_clk_root_alias(clk_data);
	init_clk_top_div(clk_data);
	init_clk_topckgen(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_topckgen, "mediatek,mt8173-topckgen", mt_topckgen_init);

static void __init mt_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap apmixedsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(APMIXED_NR_CLK);

	init_clk_apmixedsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_apmixedsys, "mediatek,mt8173-apmixedsys",
		mt_apmixedsys_init);

static void __init mt_infrasys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap infrasys failed\n");
		return;
	}

	clk_data = alloc_clk_data(INFRA_NR_CLK);

	init_clk_infrasys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_infrasys, "mediatek,mt8173-infrasys", mt_infrasys_init);

static void __init mt_perisys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap perisys failed\n");
		return;
	}

	clk_data = alloc_clk_data(PERI_NR_CLK);

	init_clk_perisys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_perisys, "mediatek,mt8173-perisys", mt_perisys_init);

static void __init mt_mfgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap mfgsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(MFG_NR_CLK);

	init_clk_mfgsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_mfgsys, "mediatek,mt8173-mfgsys", mt_mfgsys_init);

static void __init mt_imgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap imgsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(IMG_NR_CLK);

	init_clk_imgsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_imgsys, "mediatek,mt8173-imgsys", mt_imgsys_init);

static void __init mt_mmsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap mmsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(MM_NR_CLK);

	init_clk_mmsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_mmsys, "mediatek,mt8173-mmsys", mt_mmsys_init);

static void __init mt_vdecsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap vdecsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(VDEC_NR_CLK);

	init_clk_vdecsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_vdecsys, "mediatek,mt8173-vdecsys", mt_vdecsys_init);

static void __init mt_vencsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap vencsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(VENC_NR_CLK);

	init_clk_vencsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_vencsys, "mediatek,mt8173-vencsys", mt_vencsys_init);

static void __init mt_vencltsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap vencltsys failed\n");
		return;
	}

	clk_data = alloc_clk_data(VENCLT_NR_CLK);

	init_clk_vencltsys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_vencltsys, "mediatek,mt8173-vencltsys", mt_vencltsys_init);

static void __init mt_audiosys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_debug("%s: %s\n", __func__, node->name);

	base = get_reg(node, 0);
	if (!base) {
		pr_err("ioremap audiosys failed\n");
		return;
	}

	clk_data = alloc_clk_data(AUD_NR_CLK);

	init_clk_audiosys(base, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("could not register clock provide\n");
}
CLK_OF_DECLARE(mtk_audiosys, "mediatek,mt8173-audiosys", mt_audiosys_init);

#if CLK_DEBUG && 0

#include <linux/init.h>
#include <linux/module.h>

struct mtk_root_clk {
	const char *name;
	unsigned long rate;
};

#define ROOTCLK(_name, _rate) {			\
		.name = _name,			\
		.rate = _rate,			\
	}

static struct mtk_root_clk root_clks[] __initdata = {
	ROOTCLK(clk_null, 0),
	ROOTCLK(clk26m, 26000000),
	ROOTCLK(clk32k, 32000),
};

static void __init init_clk_root(void)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(root_clks); i++) {
		struct mtk_root_clk *rc = &root_clks[i];

		clk = clk_register_fixed_rate(NULL, rc->name, NULL,
			CLK_IS_ROOT, rc->rate);

		if (IS_ERR(clk))
			pr_err("Failed to register clk %s: %ld\n",
					rc->name, PTR_ERR(clk));

		pr_debug("root %3d: %s\n", i, rc->name);
	}
}

static int __init clks_init(void)
{
	init_clk_root(NULL);
	init_clk_root_alias(NULL);
	init_clk_apmixedsys((void __iomem *)0xf0209000, NULL);
	init_clk_top_div(NULL);
	init_clk_topckgen((void __iomem *)0xf0000000, NULL);
	init_clk_infrasys((void __iomem *)0xf0001000, NULL);
	init_clk_perisys((void __iomem *)0xf0003000, NULL);
	init_clk_mfgsys((void __iomem *)0xf0206000, NULL);
	init_clk_imgsys((void __iomem *)0xf5000000, NULL);
	init_clk_mmsys((void __iomem *)0xf4000000, NULL);
	init_clk_vdecsys((void __iomem *)0xf6000000, NULL);
	init_clk_vencsys((void __iomem *)0xf8000000, NULL);
	init_clk_vencltsys((void __iomem *)0xf9000000, NULL);
	init_clk_audiosys((void __iomem *)0xf2070000, NULL);

	return 0;
}

arch_initcall(clks_init);

#endif /* CLK_DEBUG */
