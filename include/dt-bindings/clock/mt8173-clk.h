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

#ifndef _DT_BINDINGS_CLK_MT8173_H
#define _DT_BINDINGS_CLK_MT8173_H

/* TOPCKGEN */

#define TOP_CLKPH_MCK_O		1
#define TOP_DPI_CK		2
#define TOP_USB_SYSPLL_125M	3
#define TOP_HDMITX_DIG_CTS	4
#define TOP_ARMCA7PLL_754M	5
#define TOP_ARMCA7PLL_502M	6
#define TOP_MAIN_H546M		7
#define TOP_MAIN_H364M		8
#define TOP_MAIN_H218P4M	9
#define TOP_MAIN_H156M		10
#define TOP_TVDPLL_445P5M	11
#define TOP_TVDPLL_594M		12
#define TOP_UNIV_624M		13
#define TOP_UNIV_416M		14
#define TOP_UNIV_249P6M		15
#define TOP_UNIV_178P3M		16
#define TOP_UNIV_48M		17
#define TOP_CLKRTC_EXT		18
#define TOP_CLKRTC_INT		19
#define TOP_FPC_CK		20
#define TOP_HDMITXPLL_D2	21
#define TOP_HDMITXPLL_D3	22
#define TOP_ARMCA7PLL_D2	23
#define TOP_ARMCA7PLL_D3	24
#define TOP_APLL1_CK		25
#define TOP_APLL2_CK		26
#define TOP_DMPLL_CK		27
#define TOP_DMPLL_D2		28
#define TOP_DMPLL_D4		29
#define TOP_DMPLL_D8		30
#define TOP_DMPLL_D16		31
#define TOP_LVDSPLL_D2		32
#define TOP_LVDSPLL_D4		33
#define TOP_LVDSPLL_D8		34
#define TOP_MMPLL_CK		35
#define TOP_MMPLL_D2		36
#define TOP_MSDCPLL_CK		37
#define TOP_MSDCPLL_D2		38
#define TOP_MSDCPLL_D4		39
#define TOP_MSDCPLL2_CK		40
#define TOP_MSDCPLL2_D2		41
#define TOP_MSDCPLL2_D4		42
#define TOP_SYSPLL_D2		43
#define TOP_SYSPLL1_D2		44
#define TOP_SYSPLL1_D4		45
#define TOP_SYSPLL1_D8		46
#define TOP_SYSPLL1_D16		47
#define TOP_SYSPLL_D3		48
#define TOP_SYSPLL2_D2		49
#define TOP_SYSPLL2_D4		50
#define TOP_SYSPLL_D5		51
#define TOP_SYSPLL3_D2		52
#define TOP_SYSPLL3_D4		53
#define TOP_SYSPLL_D7		54
#define TOP_SYSPLL4_D2		55
#define TOP_SYSPLL4_D4		56
#define TOP_TVDPLL_CK		57
#define TOP_TVDPLL_D2		58
#define TOP_TVDPLL_D4		59
#define TOP_TVDPLL_D8		60
#define TOP_TVDPLL_D16		61
#define TOP_UNIVPLL_D2		62
#define TOP_UNIVPLL1_D2		63
#define TOP_UNIVPLL1_D4		64
#define TOP_UNIVPLL1_D8		65
#define TOP_UNIVPLL_D3		66
#define TOP_UNIVPLL2_D2		67
#define TOP_UNIVPLL2_D4		68
#define TOP_UNIVPLL2_D8		69
#define TOP_UNIVPLL_D5		70
#define TOP_UNIVPLL3_D2		71
#define TOP_UNIVPLL3_D4		72
#define TOP_UNIVPLL3_D8		73
#define TOP_UNIVPLL_D7		74
#define TOP_UNIVPLL_D26		75
#define TOP_UNIVPLL_D52		76
#define TOP_VCODECPLL_CK	77
#define TOP_VCODECPLL_370P5	78
#define TOP_VENCPLL_CK		79
#define TOP_VENCPLL_D2		80
#define TOP_VENCPLL_D4		81
#define TOP_AXI_SEL		82
#define TOP_MEM_SEL		83
#define TOP_DDRPHYCFG_SEL	84
#define TOP_MM_SEL		85
#define TOP_PWM_SEL		86
#define TOP_VDEC_SEL		87
#define TOP_VENC_SEL		88
#define TOP_MFG_SEL		89
#define TOP_CAMTG_SEL		90
#define TOP_UART_SEL		91
#define TOP_SPI_SEL		92
#define TOP_USB20_SEL		93
#define TOP_USB30_SEL		94
#define TOP_MSDC50_0_H_SEL	95
#define TOP_MSDC50_0_SEL	96
#define TOP_MSDC30_1_SEL	97
#define TOP_MSDC30_2_SEL	98
#define TOP_MSDC30_3_SEL	99
#define TOP_AUDIO_SEL		100
#define TOP_AUD_INTBUS_SEL	101
#define TOP_PMICSPI_SEL		102
#define TOP_SCP_SEL		103
#define TOP_ATB_SEL		104
#define TOP_VENC_LT_SEL		105
#define TOP_DPI0_SEL		106
#define TOP_IRDA_SEL		107
#define TOP_CCI400_SEL		108
#define TOP_AUD_1_SEL		109
#define TOP_AUD_2_SEL		110
#define TOP_MEM_MFG_IN_SEL	111
#define TOP_AXI_MFG_IN_SEL	112
#define TOP_SCAM_SEL		113
#define TOP_SPINFI_IFR_SEL	114
#define TOP_HDMI_SEL		115
#define TOP_DPILVDS_SEL		116
#define TOP_MSDC50_2_H_SEL	117
#define TOP_HDCP_SEL		118
#define TOP_HDCP_24M_SEL	119
#define TOP_RTC_SEL		120
#define TOP_NR_CLK		121

/* APMIXED_SYS */

#define APMIXED_ARMCA15PLL	1
#define APMIXED_ARMCA7PLL	2
#define APMIXED_MAINPLL		3
#define APMIXED_UNIVPLL		4
#define APMIXED_MMPLL		5
#define APMIXED_MSDCPLL		6
#define APMIXED_VENCPLL		7
#define APMIXED_TVDPLL		8
#define APMIXED_MPLL		9
#define APMIXED_VCODECPLL	10
#define APMIXED_APLL1		11
#define APMIXED_APLL2		12
#define APMIXED_LVDSPLL		13
#define APMIXED_MSDCPLL2	14
#define APMIXED_NR_CLK		15

/* INFRA_SYS */

#define INFRA_DBGCLK		1
#define INFRA_SMI		2
#define INFRA_AUDIO		3
#define INFRA_GCE		4
#define INFRA_L2C_SRAM		5
#define INFRA_M4U		6
#define INFRA_CPUM		7
#define INFRA_KP		8
#define INFRA_CEC		9
#define INFRA_PMICSPI		10
#define INFRA_PMICWRAP		11
#define INFRA_NR_CLK		12

/* PERI_SYS */

#define PERI_NFI		1
#define PERI_THERM		2
#define PERI_PWM1		3
#define PERI_PWM2		4
#define PERI_PWM3		5
#define PERI_PWM4		6
#define PERI_PWM5		7
#define PERI_PWM6		8
#define PERI_PWM7		9
#define PERI_PWM		10
#define PERI_USB0		11
#define PERI_USB1		12
#define PERI_AP_DMA		13
#define PERI_MSDC30_0		14
#define PERI_MSDC30_1		15
#define PERI_MSDC30_2		16
#define PERI_MSDC30_3		17
#define PERI_NLI_ARB		18
#define PERI_IRDA		19
#define PERI_UART0		20
#define PERI_UART1		21
#define PERI_UART2		22
#define PERI_UART3		23
#define PERI_I2C0		24
#define PERI_I2C1		25
#define PERI_I2C2		26
#define PERI_I2C3		27
#define PERI_I2C4		28
#define PERI_AUXADC		29
#define PERI_SPI0		30
#define PERI_I2C5		31
#define PERI_NFIECC		32
#define PERI_SPI		33
#define PERI_IRRX		34
#define PERI_I2C6		35
#define PERI_NR_CLK		36

/* MFG_SYS */

#define MFG_AXI			1
#define MFG_MEM			2
#define MFG_G3D			3
#define MFG_26M			4
#define MFG_NR_CLK		5

/* IMG_SYS */

#define IMG_LARB2_SMI		1
#define IMG_CAM_SMI		2
#define IMG_CAM_CAM		3
#define IMG_SEN_TG		4
#define IMG_SEN_CAM		5
#define IMG_CAM_SV		6
#define IMG_FD			7
#define IMG_NR_CLK		8

/* MM_SYS */

#define MM_SMI_COMMON		1
#define MM_SMI_LARB0		2
#define MM_CAM_MDP		3
#define MM_MDP_RDMA0		4
#define MM_MDP_RDMA1		5
#define MM_MDP_RSZ0		6
#define MM_MDP_RSZ1		7
#define MM_MDP_RSZ2		8
#define MM_MDP_TDSHP0		9
#define MM_MDP_TDSHP1		10
#define MM_MDP_WDMA		11
#define MM_MDP_WROT0		12
#define MM_MDP_WROT1		13
#define MM_FAKE_ENG		14
#define MM_MUTEX_32K		15
#define MM_DISP_OVL0		16
#define MM_DISP_OVL1		17
#define MM_DISP_RDMA0		18
#define MM_DISP_RDMA1		19
#define MM_DISP_RDMA2		20
#define MM_DISP_WDMA0		21
#define MM_DISP_WDMA1		22
#define MM_DISP_COLOR0		23
#define MM_DISP_COLOR1		24
#define MM_DISP_AAL		25
#define MM_DISP_GAMMA		26
#define MM_DISP_UFOE		27
#define MM_DISP_SPLIT0		28
#define MM_DISP_SPLIT1		29
#define MM_DISP_MERGE		30
#define MM_DISP_OD		31
#define MM_DISP_PWM0MM		32
#define MM_DISP_PWM026M		33
#define MM_DISP_PWM1MM		34
#define MM_DISP_PWM126M		35
#define MM_DSI0_ENGINE		36
#define MM_DSI0_DIGITAL		37
#define MM_DSI1_ENGINE		38
#define MM_DSI1_DIGITAL		39
#define MM_DPI_PIXEL		40
#define MM_DPI_ENGINE		41
#define MM_DPI1_PIXEL		42
#define MM_DPI1_ENGINE		43
#define MM_HDMI_PIXEL		44
#define MM_HDMI_PLLCK		45
#define MM_HDMI_AUDIO		46
#define MM_HDMI_SPDIF		47
#define MM_LVDS_PIXEL		48
#define MM_LVDS_CTS		49
#define MM_SMI_LARB4		50
#define MM_HDMI_HDCP		51
#define MM_HDMI_HDCP24M		52
#define MM_NR_CLK		53

/* VDEC_SYS */

#define VDEC_CKEN		1
#define VDEC_LARB_CKEN		2
#define VDEC_NR_CLK		3

/* VENC_SYS */

#define VENC_CKE0		1
#define VENC_CKE1		2
#define VENC_CKE2		3
#define VENC_CKE3		4
#define VENC_NR_CLK		5

/* VENCLT_SYS */

#define VENCLT_CKE0		1
#define VENCLT_CKE1		2
#define VENCLT_NR_CLK		3

/* AUDIO_SYS */

#define AUD_AFE			1
#define AUD_I2S			2
#define AUD_22M			3
#define AUD_24M			4
#define AUD_SPDF2		5
#define AUD_APLL2_TNR		6
#define AUD_APLL_TNR		7
#define AUD_HDMI		8
#define AUD_SPDF		9
#define AUD_ADDA3		10
#define AUD_ADDA2		11
#define AUD_ADC			12
#define AUD_DAC			13
#define AUD_DAC_PREDIS		14
#define AUD_TML			15
#define AUD_AHB_IDLE_EX		16
#define AUD_AHB_IDLE_IN		17
#define AUD_NR_CLK		18

/* SCP_SYS */

#define SCP_SYS_VDE		1
#define SCP_SYS_MFG		2
#define SCP_SYS_VEN		3
#define SCP_SYS_ISP		4
#define SCP_SYS_DIS		5
#define SCP_SYS_VEN2		6
#define SCP_SYS_AUDIO		7
#define SCP_SYS_MFG_2D		8
#define SCP_SYS_MFG_ASYNC	9
#define SCP_SYS_USB		10
#define SCP_NR_CLK		11

#endif				/* _DT_BINDINGS_CLK_MT8173_H */
