#ifndef _MT_CLKMGR_H
#define _MT_CLKMGR_H

#include <linux/list.h>
#include "mach/mt_typedefs.h"

#define CLKMGR_ENABLE_IN_BRINGUP	0
#define CLKMGR_6595_WORKAROUND		1

#define CONFIG_CLKMGR_STAT

enum {
	CG_INFRA	= 0,
	CG_PERI0	= 1,
	CG_PERI1	= 2,
	CG_MFG		= 3,
	CG_IMAGE	= 4,
	CG_DISP0	= 5,
	CG_DISP1	= 6,
	CG_VDEC0	= 7,
	CG_VDEC1	= 8,
	CG_VENC		= 9,
	CG_VENCLT	= 10,
	CG_AUDIO	= 11,
	NR_GRPS		= 12,
};

enum cg_clk_id {

	MT_CG_INFRA_DBGCLK		= 0,
	MT_CG_INFRA_SMI			= 1,
	MT_CG_INFRA_AUDIO		= 5,
	MT_CG_INFRA_GCE			= 6,
	MT_CG_INFRA_L2C_SRAM		= 7,
	MT_CG_INFRA_M4U			= 8,
	MT_CG_INFRA_CPUM		= 15,
	MT_CG_INFRA_KP			= 16,
	MT_CG_INFRA_CEC_PDN		= 18,
	MT_CG_INFRA_PMICSPI		= 22,
	MT_CG_INFRA_PMICWRAP		= 23,

	MT_CG_PERI0_NFI			= 32,
	MT_CG_PERI0_THERM		= 33,
	MT_CG_PERI0_PWM1		= 34,
	MT_CG_PERI0_PWM2		= 35,
	MT_CG_PERI0_PWM3		= 36,
	MT_CG_PERI0_PWM4		= 37,
	MT_CG_PERI0_PWM5		= 38,
	MT_CG_PERI0_PWM6		= 39,
	MT_CG_PERI0_PWM7		= 40,
	MT_CG_PERI0_PWM			= 41,
	MT_CG_PERI0_USB0		= 42,
	MT_CG_PERI0_USB1		= 43,
	MT_CG_PERI0_AP_DMA		= 44,
	MT_CG_PERI0_MSDC30_0		= 45,
	MT_CG_PERI0_MSDC30_1		= 46,
	MT_CG_PERI0_MSDC30_2		= 47,
	MT_CG_PERI0_MSDC30_3		= 48,
	MT_CG_PERI0_NLI			= 49,
	MT_CG_PERI0_IRDA		= 50,
	MT_CG_PERI0_UART0		= 51,
	MT_CG_PERI0_UART1		= 52,
	MT_CG_PERI0_UART2		= 53,
	MT_CG_PERI0_UART3		= 54,
	MT_CG_PERI0_I2C0		= 55,
	MT_CG_PERI0_I2C1		= 56,
	MT_CG_PERI0_I2C2		= 57,
	MT_CG_PERI0_I2C3		= 58,
	MT_CG_PERI0_I2C4		= 59,
	MT_CG_PERI0_AUXADC		= 60,
	MT_CG_PERI0_SPI0		= 61,
	MT_CG_PERI0_I2C5		= 62,
	MT_CG_PERI0_NFIECC		= 63,

	MT_CG_PERI1_SPI			= 64,
	MT_CG_PERI1_IRRX		= 65,
	MT_CG_PERI1_I2C6		= 66,

	MT_CG_MFG_AXI			= 96,
	MT_CG_MFG_MEM			= 97,
	MT_CG_MFG_G3D			= 98,
	MT_CG_MFG_26M			= 99,

	MT_CG_IMAGE_LARB2_SMI		= 128,
	MT_CG_IMAGE_CAM_SMI		= 133,
	MT_CG_IMAGE_CAM_CAM		= 134,
	MT_CG_IMAGE_SEN_TG		= 135,
	MT_CG_IMAGE_SEN_CAM		= 136,
	MT_CG_IMAGE_CAM_SV		= 137,
	MT_CG_IMAGE_FD			= 139,

	MT_CG_DISP0_SMI_COMMON		= 160,
	MT_CG_DISP0_SMI_LARB0		= 161,
	MT_CG_DISP0_CAM_MDP		= 162,
	MT_CG_DISP0_MDP_RDMA0		= 163,
	MT_CG_DISP0_MDP_RDMA1		= 164,
	MT_CG_DISP0_MDP_RSZ0		= 165,
	MT_CG_DISP0_MDP_RSZ1		= 166,
	MT_CG_DISP0_MDP_RSZ2		= 167,
	MT_CG_DISP0_MDP_TDSHP0		= 168,
	MT_CG_DISP0_MDP_TDSHP1		= 169,
	MT_CG_DISP0_MDP_WDMA		= 171,
	MT_CG_DISP0_MDP_WROT0		= 172,
	MT_CG_DISP0_MDP_WROT1		= 173,
	MT_CG_DISP0_FAKE_ENG		= 174,
	MT_CG_DISP0_MUTEX_32K		= 175,
	MT_CG_DISP0_DISP_OVL0		= 176,
	MT_CG_DISP0_DISP_OVL1		= 177,
	MT_CG_DISP0_DISP_RDMA0		= 178,
	MT_CG_DISP0_DISP_RDMA1		= 179,
	MT_CG_DISP0_DISP_RDMA2		= 180,
	MT_CG_DISP0_DISP_WDMA0		= 181,
	MT_CG_DISP0_DISP_WDMA1		= 182,
	MT_CG_DISP0_DISP_COLOR0		= 183,
	MT_CG_DISP0_DISP_COLOR1		= 184,
	MT_CG_DISP0_DISP_AAL		= 185,
	MT_CG_DISP0_DISP_GAMMA		= 186,
	MT_CG_DISP0_DISP_UFOE		= 187,
	MT_CG_DISP0_DISP_SPLIT0		= 188,
	MT_CG_DISP0_DISP_SPLIT1		= 189,
	MT_CG_DISP0_DISP_MERGE		= 190,
	MT_CG_DISP0_DISP_OD		= 191,

	MT_CG_DISP1_DISP_PWM0_MM	= 192,
	MT_CG_DISP1_DISP_PWM0_26M	= 193,
	MT_CG_DISP1_DISP_PWM1_MM	= 194,
	MT_CG_DISP1_DISP_PWM1_26M	= 195,
	MT_CG_DISP1_DSI0_ENGINE		= 196,
	MT_CG_DISP1_DSI0_DIGITAL	= 197,
	MT_CG_DISP1_DSI1_ENGINE		= 198,
	MT_CG_DISP1_DSI1_DIGITAL	= 199,
	MT_CG_DISP1_DPI_PIXEL		= 200,
	MT_CG_DISP1_DPI_ENGINE		= 201,
	MT_CG_DISP1_DPI1_PIXEL		= 202,
	MT_CG_DISP1_DPI1_ENGINE		= 203,
	MT_CG_DISP1_HDMI_PIXEL		= 204,
	MT_CG_DISP1_HDMI_PLLCK		= 205,
	MT_CG_DISP1_HDMI_AUDIO		= 206,
	MT_CG_DISP1_HDMI_SPDIF		= 207,
	MT_CG_DISP1_LVDS_PIXEL		= 208,
	MT_CG_DISP1_LVDS_CTS		= 209,
	MT_CG_DISP1_SMI_LARB4		= 210,

	MT_CG_VDEC0_VDEC		= 224,
	MT_CG_VDEC1_LARB		= 256,

	MT_CG_VENC_CKE0			= 288,
	MT_CG_VENC_CKE1			= 292,
	MT_CG_VENC_CKE2			= 296,
	MT_CG_VENC_CKE3			= 300,

	MT_CG_VENCLT_CKE0		= 320,
	MT_CG_VENCLT_CKE1		= 324,

	MT_CG_AUDIO_AFE			= 354,
	MT_CG_AUDIO_I2S			= 358,
	MT_CG_AUDIO_22M			= 360,
	MT_CG_AUDIO_24M			= 361,
	MT_CG_AUDIO_SPDF2		= 363,
	MT_CG_AUDIO_APLL2_TUNER		= 370,
	MT_CG_AUDIO_APLL_TUNER		= 371,
	MT_CG_AUDIO_HDMI		= 372,
	MT_CG_AUDIO_SPDF		= 373,
	MT_CG_AUDIO_ADDA3		= 374,
	MT_CG_AUDIO_ADDA2		= 375,
	MT_CG_AUDIO_ADC			= 376,
	MT_CG_AUDIO_DAC			= 377,
	MT_CG_AUDIO_DAC_PREDIS		= 378,
	MT_CG_AUDIO_TML			= 379,
	MT_CG_AUDIO_IDLE_EN_EXT		= 381,
	MT_CG_AUDIO_IDLE_EN_INT		= 382,

	CG_INFRA_FROM			= MT_CG_INFRA_DBGCLK,
	CG_INFRA_TO			= MT_CG_INFRA_PMICWRAP,
	NR_INFRA_CLKS			= 9,

	CG_PERI0_FROM			= MT_CG_PERI0_NFI,
	CG_PERI0_TO			= MT_CG_PERI0_NFIECC,
	NR_PERI0_CLKS			= 32,

	CG_PERI1_FROM			= MT_CG_PERI1_SPI,
	CG_PERI1_TO			= MT_CG_PERI1_I2C6,
	NR_PERI1_CLKS			= 3,

	CG_MFG_FROM			= MT_CG_MFG_AXI,
	CG_MFG_TO			= MT_CG_MFG_26M,
	NR_MFG_CLKS			= 4,

	CG_IMAGE_FROM			= MT_CG_IMAGE_LARB2_SMI,
	CG_IMAGE_TO			= MT_CG_IMAGE_FD,
	NR_IMAGE_CLKS			= 7,

	CG_DISP0_FROM			= MT_CG_DISP0_SMI_COMMON,
	CG_DISP0_TO			= MT_CG_DISP0_DISP_OD,
	NR_DISP0_CLKS			= 31,

	CG_DISP1_FROM			= MT_CG_DISP1_DISP_PWM0_MM,
	CG_DISP1_TO			= MT_CG_DISP1_SMI_LARB4,
	NR_DISP1_CLKS			= 19,

	CG_VDEC0_FROM			= MT_CG_VDEC0_VDEC,
	CG_VDEC0_TO			= MT_CG_VDEC0_VDEC,
	NR_VDEC0_CLKS			= 1,

	CG_VDEC1_FROM			= MT_CG_VDEC1_LARB,
	CG_VDEC1_TO			= MT_CG_VDEC1_LARB,
	NR_VDEC1_CLKS			= 1,

	CG_VENC_FROM			= MT_CG_VENC_CKE0,
	CG_VENC_TO			= MT_CG_VENC_CKE3,
	NR_VENC_CLKS			= 4,

	CG_VENCLT_FROM			= MT_CG_VENCLT_CKE0,
	CG_VENCLT_TO			= MT_CG_VENCLT_CKE1,
	NR_VENCLT_CLKS			= 2,

	CG_AUDIO_FROM			= MT_CG_AUDIO_AFE,
	CG_AUDIO_TO			= MT_CG_AUDIO_IDLE_EN_INT,
	NR_AUDIO_CLKS			= 17,

	NR_CLKS				= 383,
};

#if CLKMGR_6595_WORKAROUND
#define MT_CG_PERI_NFI		MT_CG_PERI0_NFI
#define MT_CG_PERI_THERM	MT_CG_PERI0_THERM
#define MT_CG_PERI_PWM1		MT_CG_PERI0_PWM1
#define MT_CG_PERI_PWM2		MT_CG_PERI0_PWM2
#define MT_CG_PERI_PWM3		MT_CG_PERI0_PWM3
#define MT_CG_PERI_PWM4		MT_CG_PERI0_PWM4
#define MT_CG_PERI_PWM5		MT_CG_PERI0_PWM5
#define MT_CG_PERI_PWM6		MT_CG_PERI0_PWM6
#define MT_CG_PERI_PWM7		MT_CG_PERI0_PWM7
#define MT_CG_PERI_PWM		MT_CG_PERI0_PWM
#define MT_CG_PERI_USB0		MT_CG_PERI0_USB0
#define MT_CG_PERI_USB1		MT_CG_PERI0_USB1
#define MT_CG_PERI_AP_DMA	MT_CG_PERI0_AP_DMA
#define MT_CG_PERI_MSDC30_0	MT_CG_PERI0_MSDC30_0
#define MT_CG_PERI_MSDC30_1	MT_CG_PERI0_MSDC30_1
#define MT_CG_PERI_MSDC30_2	MT_CG_PERI0_MSDC30_2
#define MT_CG_PERI_MSDC30_3	MT_CG_PERI0_MSDC30_3
#define MT_CG_PERI_NLI		MT_CG_PERI0_NLI
#define MT_CG_PERI_IRDA		MT_CG_PERI0_IRDA
#define MT_CG_PERI_UART0	MT_CG_PERI0_UART0
#define MT_CG_PERI_UART1	MT_CG_PERI0_UART1
#define MT_CG_PERI_UART2	MT_CG_PERI0_UART2
#define MT_CG_PERI_UART3	MT_CG_PERI0_UART3
#define MT_CG_PERI_I2C0		MT_CG_PERI0_I2C0
#define MT_CG_PERI_I2C1		MT_CG_PERI0_I2C1
#define MT_CG_PERI_I2C2		MT_CG_PERI0_I2C2
#define MT_CG_PERI_I2C3		MT_CG_PERI0_I2C3
#define MT_CG_PERI_I2C4		MT_CG_PERI0_I2C4
#define MT_CG_PERI_AUXADC	MT_CG_PERI0_AUXADC
#define MT_CG_PERI_SPI0		MT_CG_PERI0_SPI0
#define MT_CG_PERI_I2C5		MT_CG_PERI0_I2C5
#define MT_CG_PERI_NFIECC	MT_CG_PERI0_NFIECC
#endif /* CLKMGR_6595_WORKAROUND */

enum {
	/* CLK_CFG_0 */
	MT_MUX_MM		= 0,
	MT_MUX_DDRPHY		= 1,
	MT_MUX_MEM		= 2,
	MT_MUX_AXI		= 3,

	/* CLK_CFG_1 */
	MT_MUX_MFG		= 4,
	MT_MUX_VENC		= 5,
	MT_MUX_VDEC		= 6,
	MT_MUX_PWM		= 7,

	/* CLK_CFG_2 */
	MT_MUX_USB20		= 8,
	MT_MUX_SPI		= 9,
	MT_MUX_UART		= 10,
	MT_MUX_CAMTG		= 11,

	/* CLK_CFG_3 */
	MT_MUX_MSDC30_1		= 12,
	MT_MUX_MSDC50_0		= 13,
	MT_MUX_MSDC50_0_hclk	= 14,
	MT_MUX_USB30		= 15,

	/* CLK_CFG_4 */
	MT_MUX_AUDINTBUS	= 16,
	MT_MUX_AUDIO		= 17,
	MT_MUX_MSDC30_3		= 18,
	MT_MUX_MSDC30_2		= 19,

	/* CLK_CFG_5 */
	MT_MUX_VENCLT		= 20,
	MT_MUX_ATB		= 21,
	MT_MUX_SCP		= 22,
	MT_MUX_PMICSPI		= 23,

	/* CLK_CFG_6 */
	MT_MUX_AUD1		= 24,
	MT_MUX_CCI400		= 25,
	MT_MUX_IRDA		= 26,
	MT_MUX_DPI0		= 27,

	/* CLK_CFG_7 */
	MT_MUX_SCAM		= 28,
	MT_MUX_AXI_MFG_IN_AS	= 29,
	MT_MUX_MEM_MFG_IN_AS	= 30,
	MT_MUX_AUD2		= 31,

	/* CLK_CFG_12 */
	MT_MUX_DPILVDS		= 32,
	MT_MUX_HDMI		= 33,
	MT_MUX_SPINFI_INFRA_BCLK = 34,

	/* CLK_CFG_13 */
	MT_MUX_RTC		= 35,
	MT_MUX_HDCP_24M		= 36,
	MT_MUX_HDCP		= 37,
	MT_MUX_MSDC50_2_HCLK	= 38,

	NR_MUXS			= 39,
};

enum {
	ARMCA15PLL	= 0,
	ARMCA7PLL	= 1,
	MAINPLL		= 2,
	MSDCPLL		= 3,
	UNIVPLL		= 4,
	MMPLL		= 5,
	VENCPLL		= 6,
	TVDPLL		= 7,
	MPLL		= 8,
	VCODECPLL	= 9,
	APLL1		= 10,
	APLL2		= 11,
	LVDSPLL		= 12,
	MSDCPLL2	= 13,
	NR_PLLS		= 14,
};

enum {
	SYS_VDE		= 0,
	SYS_MFG		= 1,
	SYS_VEN		= 2,
	SYS_ISP		= 3,
	SYS_DIS		= 4,
	SYS_VEN2	= 5,
	SYS_AUD		= 6,
	SYS_MFG_2D	= 7,
	SYS_MFG_ASYNC	= 8,
	SYS_USB		= 9,
	NR_SYSS		= 10,
};

enum {
	MT_LARB_DISP = 0,
	MT_LARB_VDEC = 1,
	MT_LARB_IMG  = 2,
	MT_LARB_VENC = 3,
	MT_LARB_MJC  = 4,
};

/* larb	monitor	mechanism definition*/
enum {
	LARB_MONITOR_LEVEL_HIGH		= 10,
	LARB_MONITOR_LEVEL_MEDIUM	= 20,
	LARB_MONITOR_LEVEL_LOW		= 30,
};

struct larb_monitor {
	struct list_head link;
	int level;
	void (*backup)(struct larb_monitor *h, int larb_idx);
		/* called before disable larb clock */
	void (*restore)(struct larb_monitor *h, int larb_idx);
		/* called after enable larb clock */
};

enum monitor_clk_sel_0 {
	no_clk_0		= 0,
	AD_UNIV_624M_CK		= 5,
	AD_UNIV_416M_CK		= 6,
	AD_UNIV_249P6M_CK	= 7,
	AD_UNIV_178P3M_CK_0	= 8,
	AD_UNIV_48M_CK		= 9,
	AD_USB_48M_CK		= 10,
	rtc32k_ck_i_0		= 20,
	AD_SYS_26M_CK_0		= 21,
};

enum monitor_clk_sel {
	no_clk			= 0,
	AD_SYS_26M_CK		= 1,
	rtc32k_ck_i		= 2,
	clkph_MCLK_o		= 7,
	AD_DPICLK		= 8,
	AD_MSDCPLL_CK		= 9,
	AD_MMPLL_CK		= 10,
	AD_UNIV_178P3M_CK	= 11,
	AD_MAIN_H156M_CK	= 12,
	AD_VENCPLL_CK		= 13,
};

enum ckmon_sel {
	clk_ckmon0		= 0,
	clk_ckmon1		= 1,
	clk_ckmon2		= 2,
	clk_ckmon3		= 3,
};

extern void register_larb_monitor(struct larb_monitor *handler);
extern void unregister_larb_monitor(struct larb_monitor *handler);

/* clock API */
extern int enable_clock(enum cg_clk_id id, char *mod_name);
extern int disable_clock(enum cg_clk_id id, char *mod_name);
extern int mt_enable_clock(enum cg_clk_id id, char *mod_name);
extern int mt_disable_clock(enum cg_clk_id id, char *mod_name);

extern int enable_clock_ext_locked(int id, char *mod_name);
extern int disable_clock_ext_locked(int id, char *mod_name);

extern int clock_is_on(int id);

extern int clkmux_sel(int id, unsigned int clksrc, char *name);
extern void enable_mux(int id, char *name);
extern void disable_mux(int id, char *name);

extern void clk_set_force_on(int id);
extern void clk_clr_force_on(int id);
extern int clk_is_force_on(int id);

/* pll API */
extern int enable_pll(int id, char *mod_name);
extern int disable_pll(int id, char *mod_name);

extern int pll_hp_switch_on(int id, int hp_on);
extern int pll_hp_switch_off(int id, int hp_off);

extern int pll_fsel(int id, unsigned int value);
extern int pll_is_on(int id);

/* subsys API */
extern int enable_subsys(int id, char *mod_name);
extern int disable_subsys(int id, char *mod_name);

extern int subsys_is_on(int id);
extern int md_power_on(int id);
extern int md_power_off(int id, unsigned int timeout);

/* other API */

extern void set_mipi26m(int en);
extern void set_ada_ssusb_xtal_ck(int en);

const char *grp_get_name(int id);
extern int clkmgr_is_locked(void);

/* init */
extern int __init mt_clkmgr_init(void);

extern int clk_monitor_0(enum ckmon_sel ckmon,
		enum monitor_clk_sel_0 sel, int div);
extern int clk_monitor(enum ckmon_sel ckmon, enum monitor_clk_sel sel, int div);

extern void cci400_sel_for_ddp(void);
extern void clk_stat_check(int id);

/* Vcore DVFS*/
extern u32 get_axi_khz(void);
extern void clkmux_sel_for_vcorefs(bool mode);

#ifdef _MT_CLKMGR_C

/* TODO: remove it */

#ifdef CONFIG_MTK_MMC
extern void msdc_clk_status(int *status);
#else
void msdc_clk_status(int *status) { *status = 0; }
#endif

extern void mt_cirq_enable(void);
extern void mt_cirq_disable(void);
extern void mt_cirq_clone_gic(void);
extern void mt_cirq_flush(void);
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);

#endif /* _MT_CLKMGR_C */

#endif
