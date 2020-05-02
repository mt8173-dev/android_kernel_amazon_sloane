#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT

#ifndef _MT8135_MHL_REG_H_
#define _MT8135_MHL_REG_H_
/* ////////////////////////////////////// */
/* #define HDMI_GRL_BASE                         0x14025000 */
/* #define HDMI_SYS_BASE                         0xF4000000 */
/* #define HDMI_ANALOG_BASE              0xF0209000 */
/* #define HDMI_ANALOG_CKGEN_BASE                0xF0000000 */
/* #define HDMI_EFUSE_BASE                               0xF0206000 */

extern void __iomem *hdmi_tx_reg_addr;
extern void __iomem *hdmi_tvd_reg_addr;
extern void __iomem *hdmi_analog_reg_addr;
extern void __iomem *hdmi_dpi0_reg_addr;
extern void __iomem *hdmi_mmsys_reg_addr;
extern void __iomem *hdmi_ckgen_reg_addr;
extern void __iomem *hdmi_efuse_reg_addr;

/* 0x14025000 */
#define HDMI_GRL_BASE                       hdmi_tx_reg_addr
/* 0x14000000 */
#define HDMI_SYS_BASE                       hdmi_mmsys_reg_addr
/* 0x10209000 */
#define HDMI_ANALOG_BASE                hdmi_analog_reg_addr
/* 0x10000000 */
#define HDMI_ANALOG_CKGEN_BASE  hdmi_ckgen_reg_addr
/* 0x10206000 */
#define HDMI_EFUSE_BASE                   hdmi_efuse_reg_addr
/* 0x1401d000 */
#define HDMI_DPI0_BASE                      hdmi_dpi0_reg_addr

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
void vWriteByteHdmiGRL(unsigned int dAddr, unsigned int dVal);
#else
#define vWriteByteHdmiGRL(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_GRL_BASE + dAddr)) = (dVal))
#endif

#define bReadByteHdmiGRL(dAddr)         (*((volatile unsigned int *)(HDMI_GRL_BASE + dAddr)))
#define vWriteHdmiGRLMsk(dAddr, dVal, dMsk) vWriteByteHdmiGRL((dAddr), (bReadByteHdmiGRL(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

#define vWriteHdmiSYS(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_SYS_BASE + dAddr)) = (dVal))
#define dReadHdmiSYS(dAddr)         (*((volatile unsigned int *)(HDMI_SYS_BASE + dAddr)))
#define vWriteHdmiSYSMsk(dAddr, dVal, dMsk) (vWriteHdmiSYS((dAddr), (dReadHdmiSYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiANA(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_ANALOG_BASE + dAddr)) = (dVal))
#define dReadHdmiANA(dAddr)         (*((volatile unsigned int *)(HDMI_ANALOG_BASE + dAddr)))
#define vWriteHdmiANAMsk(dAddr, dVal, dMsk) (vWriteHdmiANA((dAddr), (dReadHdmiANA(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiEfuse(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_EFUSE_BASE + dAddr)) = (dVal))
#define dReadHdmiEfuse(dAddr)         (*((volatile unsigned int *)(HDMI_EFUSE_BASE + dAddr)))
#define vWriteHdmiEfuseMsk(dAddr, dVal, dMsk) (vWriteHdmiEfuse((dAddr), (dReadHdmiEfuse(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiCKANA(dAddr, dVal)  (*((volatile unsigned int *)(HDMI_ANALOG_CKGEN_BASE + dAddr)) = (dVal))
#define dReadHdmiCKANA(dAddr)         (*((volatile unsigned int *)(HDMI_ANALOG_CKGEN_BASE + dAddr)))
#define vWriteHdmiCKANAMsk(dAddr, dVal, dMsk) (vWriteHdmiCKANA((dAddr), (dReadHdmiCKANA(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
/* ////////////////////////////////////// */
#define PORD_MODE     (1<<0)
#define HOTPLUG_MODE  (1<<1)
/* ////////////////////////////////////// */
#define AV_INFO_HD_ITU709           0x80
#define AV_INFO_SD_ITU601           0x40
#define AV_INFO_4_3_OUTPUT          0x10
#define AV_INFO_16_9_OUTPUT         0x20

/* AVI Info Frame */
#define AVI_TYPE            0x82
#define AVI_VERS            0x02
#define AVI_LEN             0x0d

/* Audio Info Frame */
#define AUDIO_TYPE             0x84
#define AUDIO_VERS             0x01
#define AUDIO_LEN              0x0A

#define VS_TYPE            0x81
#define VS_VERS            0x01
#define VS_LEN             0x04
#define VS_PB_LEN          0x0b	/* VS_LEN+1 include checksum */
/* GAMUT Data */
#define GAMUT_TYPE             0x0A
#define GAMUT_PROFILE          0x81
#define GAMUT_SEQ              0x31

/* ACP Info Frame */
#define ACP_TYPE            0x04
/* #define ACP_VERS            0x02 */
#define ACP_LEN             0x00

/* ISRC1 Info Frame */
#define ISRC1_TYPE            0x05
/* #define ACP_VERS            0x02 */
#define ISRC1_LEN             0x00

/* SPD Info Frame */
#define SPD_TYPE            0x83
#define SPD_VERS            0x01
#define SPD_LEN             0x19


#define SV_ON        (1)
#define SV_OFF       (0)

#define TMDS_CLK_X1 1
#define TMDS_CLK_X1_25  2
#define TMDS_CLK_X1_5  3
#define TMDS_CLK_X2  4

#define RJT_24BIT 0
#define RJT_16BIT 1
#define LJT_24BIT 2
#define LJT_16BIT 3
#define I2S_24BIT 4
#define I2S_16BIT 5

/* ///////////////////////////////////// */
#define GRL_INT              0x14
#define INT_MDI            (0x1 << 0)
#define INT_HDCP           (0x1 << 1)
#define INT_FIFO_O         (0x1 << 2)
#define INT_FIFO_U         (0x1 << 3)
#define INT_IFM_ERR        (0x1 << 4)
#define INT_INF_DONE       (0x1 << 5)
#define INT_NCTS_DONE      (0x1 << 6)
#define INT_CTRL_PKT_DONE  (0x1 << 7)
#define GRL_INT_MASK              0x18
#define GRL_CTRL             0x1C
#define CTRL_GEN_EN        (0x1 << 2)
#define CTRL_SPD_EN        (0x1 << 3)
#define CTRL_MPEG_EN       (0x1 << 4)
#define CTRL_AUDIO_EN      (0x1 << 5)
#define CTRL_AVI_EN        (0x1 << 6)
#define CTRL_AVMUTE        (0x1 << 7)
#define GRL_STATUS           0x20
#define STATUS_HTPLG       (0x1 << 0)
#define STATUS_PORD        (0x1 << 1)

#define GRL_CFG0             0x24
#define CFG0_I2S_MODE_RTJ  0x1
#define CFG0_I2S_MODE_LTJ  0x0
#define CFG0_I2S_MODE_I2S  0x2
#define CFG0_I2S_MODE_24Bit 0x00
#define CFG0_I2S_MODE_16Bit 0x10

#define GRL_CFG1             0x28
#define CFG1_EDG_SEL       (0x1 << 0)
#define CFG1_SPDIF         (0x1 << 1)
#define CFG1_DVI           (0x1 << 2)
#define CFG1_HDCP_DEBUG    (0x1 << 3)
#define GRL_CFG2             0x2c
#define CFG2_MHL_DE_SEL			(0x1 << 3)
#define CFG2_MHL_FAKE_DE_SEL	(0x1 << 4)
#define CFG2_MHL_DATA_REMAP	(0x1 << 5)
#define CFG2_NOTICE_EN     (0x1 << 6)
#define GRL_CFG3             0x30
#define CFG3_AES_KEY_INDEX_MASK    0x3f
#define CFG3_CONTROL_PACKET_DELAY  (0x1 << 6)
#define CFG3_KSV_LOAD_START        (0x1 << 7)
#define GRL_CFG4             0x34
#define CFG4_AES_KEY_LOAD  (0x1 << 4)
#define CFG4_AV_UNMUTE_EN  (0x1 << 5)
#define CFG4_AV_UNMUTE_SET (0x1 << 6)
#define CFG4_MHL_MODE		(1 << 7)
#define GRL_CFG5             0x38
#define CFG5_CD_RATIO_MASK 0x8F
#define CFG5_FS128         (0x1 << 4)
#define CFG5_FS256         (0x2 << 4)
#define CFG5_FS384         (0x3 << 4)
#define CFG5_FS512         (0x4 << 4)
#define CFG5_FS768         (0x6 << 4)
#define GRL_WR_BKSV0         0x40
#define GRL_WR_AN0           0x54
#define GRL_RD_AKSV0         0x74
#define GRL_RI_0             0x88
#define GRL_KEY_PORT         0x90
#define GRL_KSVLIST          0x94
#define GRL_HDCP_STA         0xB8
#define HDCP_STA_RI_RDY    (0x1 << 2)
#define HDCP_STA_V_MATCH   (0x1 << 3)
#define HDCP_STA_V_RDY     (0x1 << 4)


#define GRL_HDCP_CTL         0xBC
#define HDCP_CTL_ENC_EN    (0x1 << 0)
#define HDCP_CTL_AUTHEN_EN (0x1 << 1)
#define HDCP_CTL_CP_RSTB   (0x1 << 2)
#define HDCP_CTL_AN_STOP   (0x1 << 3)
#define HDCP_CTRL_RX_RPTR  (0x1 << 4)
#define HDCP_CTL_HOST_KEY  (0x1 << 6)
#define HDCP_CTL_SHA_EN    (0x1 << 7)

#define GRL_REPEATER_HASH    0xC0
#define GRL_I2S_C_STA0             0x140
#define GRL_I2S_C_STA1             0x144
#define GRL_I2S_C_STA2             0x148
#define GRL_I2S_C_STA3             0x14C	/* including sampling frequency information. */
#define GRL_I2S_C_STA4             0x150
#define GRL_I2S_UV           0x154
#define GRL_ACP_ISRC_CTRL    0x158
#define VS_EN              (0x01<<0)
#define ACP_EN             (0x01<<1)
#define ISRC1_EN           (0x01<<2)
#define ISRC2_EN           (0x01<<3)
#define GAMUT_EN           (0x01<<4)
#define GRL_CTS_CTRL         0x160
#define CTS_CTRL_SOFT      (0x1 << 0)

#define GRL_CTS0             0x164
#define GRL_CTS1             0x168
#define GRL_CTS2             0x16c

#define GRL_DIVN             0x170
#define NCTS_WRI_ANYTIME   (0x01<<6)

#define GRL_MHL_ECO	0x174
#define GRL_MHL_VIDEO_ENC		(0x1 << 0)
#define GRL_MHL_PACKET_VIDEO_ENC	(0x1 << 1)
#define GRL_VSYNC_DEL_SEL		(0x1 << 2)

#define GRL_DIV_RESET        0x178
#define SWAP_YC  (0x01 << 0)
#define UNSWAP_YC  (0x00 << 0)

#define GRL_AUDIO_CFG        0x17C
#define AUDIO_ZERO          (0x01<<0)
#define HIGH_BIT_RATE      (0x01<<1)
#define SACD_DST           (0x01<<2)
#define DST_NORMAL_DOUBLE  (0x01<<3)
#define DSD_INV            (0x01<<4)
#define LR_INV             (0x01<<5)
#define LR_MIX             (0x01<<6)
#define SACD_SEL           (0x01<<7)

#define GRL_NCTS             0x184

#define GRL_IFM_PORT         0x188
#define GRL_CH_SW0           0x18C
#define GRL_CH_SW1           0x190
#define GRL_CH_SW2           0x194
#define GRL_CH_SWAP          0x198
#define LR_SWAP             (0x01<<0)
#define LFE_CC_SWAP         (0x01<<1)
#define LSRS_SWAP           (0x01<<2)
#define RLS_RRS_SWAP        (0x01<<3)
#define LR_STATUS_SWAP      (0x01<<4)

#define GRL_INFOFRM_VER      0x19C
#define GRL_INFOFRM_TYPE     0x1A0
#define GRL_INFOFRM_LNG      0x1A4
#define GRL_SHIFT_R2         0x1B0
#define AUDIO_PACKET_OFF      (0x01 << 6)
#define GRL_MIX_CTRL         0x1B4
#define MIX_CTRL_SRC_EN    (0x1 << 0)
#define BYPASS_VOLUME    (0x1 << 1)
#define MIX_CTRL_FLAT      (0x1 << 7)
#define GRL_IIR_FILTER       0x1B8
#define GRL_SHIFT_L1         0x1C0
#define GRL_AOUT_BNUM_SEL    0x1C4
#define AOUT_24BIT         0x00
#define AOUT_20BIT         0x02
#define AOUT_16BIT         0x03
#define HIGH_BIT_RATE_PACKET_ALIGN (0x3 << 6)

#define GRL_L_STATUS_0        0x200
#define GRL_L_STATUS_1        0x204
#define GRL_L_STATUS_2        0x208
#define GRL_L_STATUS_3        0x20c
#define GRL_L_STATUS_4        0x210
#define GRL_L_STATUS_5        0x214
#define GRL_L_STATUS_6        0x218
#define GRL_L_STATUS_7        0x21c
#define GRL_L_STATUS_8        0x220
#define GRL_L_STATUS_9        0x224
#define GRL_L_STATUS_10        0x228
#define GRL_L_STATUS_11        0x22c
#define GRL_L_STATUS_12        0x230
#define GRL_L_STATUS_13        0x234
#define GRL_L_STATUS_14        0x238
#define GRL_L_STATUS_15        0x23c
#define GRL_L_STATUS_16        0x240
#define GRL_L_STATUS_17        0x244
#define GRL_L_STATUS_18        0x248
#define GRL_L_STATUS_19        0x24c
#define GRL_L_STATUS_20        0x250
#define GRL_L_STATUS_21        0x254
#define GRL_L_STATUS_22        0x258
#define GRL_L_STATUS_23        0x25c
#define GRL_R_STATUS_0        0x260
#define GRL_R_STATUS_1        0x264
#define GRL_R_STATUS_2        0x268
#define GRL_R_STATUS_3        0x26c
#define GRL_R_STATUS_4        0x270
#define GRL_R_STATUS_5        0x274
#define GRL_R_STATUS_6        0x278
#define GRL_R_STATUS_7        0x27c
#define GRL_R_STATUS_8        0x280
#define GRL_R_STATUS_9        0x284
#define GRL_R_STATUS_10        0x288
#define GRL_R_STATUS_11        0x28c
#define GRL_R_STATUS_12        0x290
#define GRL_R_STATUS_13        0x294
#define GRL_R_STATUS_14        0x298
#define GRL_R_STATUS_15        0x29c
#define GRL_R_STATUS_16        0x2a0
#define GRL_R_STATUS_17        0x2a4
#define GRL_R_STATUS_18        0x2a8
#define GRL_R_STATUS_19        0x2ac
#define GRL_R_STATUS_20        0x2b0
#define GRL_R_STATUS_21        0x2b4
#define GRL_R_STATUS_22        0x2b8
#define GRL_R_STATUS_23        0x2bc

#define GRL_ABIST_CTL0		0x2d4
#define GRL_ABIST_CTL1		0x2d8

#define DUMMY_304    0x304
#define CHMO_SEL  (0x3<<2)
#define CHM1_SEL  (0x3<<4)
#define CHM2_SEL  (0x3<<6)
#define AUDIO_I2S_NCTS_SEL   (1<<1)
#define AUDIO_I2S_NCTS_SEL_64   (1<<1)
#define AUDIO_I2S_NCTS_SEL_128  (0<<1)
#define NEW_GCP_CTRL (1<<0)
#define NEW_GCP_CTRL_MERGE (1<<0)
#define NEW_GCP_CTRL_ORIG  (0<<0)

#define CRC_CTRL 0x310
#define clr_crc_result (1<<1)
#define init_crc  (1<<0)

#define CRC_RESULT_L 0x314

#define CRC_RESULT_H 0x318

#define HDMI_BGB_VALUE1	0x380
#define HDMI_BGB_VALUE2	0x384
#define HDMI_BGB_VALUE3	0x388
#define HDMI_BGB_VALUE4	0x38c

#define VIDEO_SOURCE_PATH	0x390
#define VIDEO_SOURCE_SEL	(1 << 7)
    /* ////////////////////////////////////////////////// */
#define DISP_CG_CON1	0x110
#define HDMI_SYS_CFG0	0x900
#define HDMI_SPDIF_ON	(0x01 << 0)
#define HDMI_RST            (0x01 << 1)
#define ANLG_ON           (0x01 << 2)
#define CFG10_DVI          (0x01 << 3)
#define SYS_KEYMASK1       (0xff<<8)
#define SYS_KEYMASK2       (0xff<<16)
#define AES_EFUSE   (0x01 << 31)

#define HDMI_SYS_CFG1	0x904
#define DEEP_COLOR_MODE_MASK (3 << 1)
#define COLOR_8BIT_MODE	 (0 << 1)
#define COLOR_10BIT_MODE	 (1 << 1)
#define COLOR_12BIT_MODE	 (2 << 1)
#define COLOR_16BIT_MODE	 (3 << 1)
#define DEEP_COLOR_EN		 (1 << 0)
#define HDMI_AUDIO_TEST_SEL		(0x01 << 8)
#define HDMI2P0_EN                              (0x1 << 11)
#define HDMI_OUT_FIFO_EN			(0x01 << 16)
#define HDMI_OUT_FIFO_CLK_INV		(0x01 << 17)
#define MHL_MODE_ON				(0x01 << 28)
#define MHL_PP_MODE				(0x01 << 29)
#define MHL_SYNC_AUTO_EN			(0x01 << 30)
#define MHL_RISC_CLK_EN			(0x01 << 31)
    /* //////////////////////////////////////////////////////////// */
#define HDMI_CON0    0x100
#define RG_HDMITX_PLL_EN_MASK    (0x01 << 31)
#define RG_HDMITX_PLL_FBKDIV_MASK    (0x7F << 24)
#define RG_HDMITX_PLL_FBKSEL_MASK    (0x03 << 22)
#define RG_HDMITX_PLL_PREDIV_MASK    (0x03 << 20)
#define RG_HDMITX_PLL_POSDIV_MASK    (0x03 << 18)
#define RG_HDMITX_PLL_RST_DLY_MASK    (0x03 << 16)
#define RG_HDMITX_PLL_IR_MASK    (0x0F << 12)
#define RG_HDMITX_PLL_IC_MASK    (0x0F << 8)
#define RG_HDMITX_PLL_BP_MASK    (0x0F << 4)
#define RG_HDMITX_PLL_BR_MASK    (0x03 << 2)
#define RG_HDMITX_PLL_BC_MASK    (0x03 << 0)
#define RG_HDMITX_PLL_EN	31
#define RG_HDMITX_PLL_FBKDIV	24
#define RG_HDMITX_PLL_FBKSEL	22
#define RG_HDMITX_PLL_PREDIV	20
#define RG_HDMITX_PLL_POSDIV	18
#define RG_HDMITX_PLL_RST_DLY	16
#define RG_HDMITX_PLL_IR	12
#define RG_HDMITX_PLL_IC	8
#define RG_HDMITX_PLL_BP	4
#define RG_HDMITX_PLL_BR	2
#define RG_HDMITX_PLL_BC	0

#define HDMI_CON1    0x104
#define RG_HDMITX_PLL_DIVEN_MASK    (0x07 << 29)
#define RG_HDMITX_PLL_AUTOK_EN_MASK    (0x01 << 28)
#define RG_HDMITX_PLL_AUTOK_KF_MASK    (0x03 << 26)
#define RG_HDMITX_PLL_AUTOK_KS_MASK    (0x03 << 24)
#define RG_HDMITX_PLL_AUTOK_LOAD_MASK    (0x01 << 23)
#define RG_HDMITX_PLL_BAND_MASK    (0x7F << 16)
#define RG_HDMITX_PLL_REF_SEL_MASK    (0x01 << 15)
#define RG_HDMITX_PLL_BIAS_EN_MASK    (0x01 << 14)
#define RG_HDMITX_PLL_BIAS_LPF_EN_MASK    (0x01 << 13)
#define RG_HDMITX_PLL_TXDIV_EN_MASK    (0x01 << 12)
#define RG_HDMITX_PLL_TXDIV_MASK    (0x03 << 10)
#define RG_HDMITX_PLL_LVROD_EN_MASK    (0x01 << 9)
#define RG_HDMITX_PLL_MONVC_EN_MASK    (0x01 << 8)
#define RG_HDMITX_PLL_MONCK_EN_MASK    (0x01 << 7)
#define RG_HDMITX_PLL_MONREF_EN_MASK    (0x01 << 6)
#define RG_HDMITX_PLL_TST_EN_MASK    (0x01 << 5)
#define RG_HDMITX_PLL_TST_CK_EN_MASK    (0x01 << 4)
#define RG_HDMITX_PLL_TST_SEL_MASK    (0x0F << 0)
#define RG_HDMITX_PLL_DIVEN	29
#define RG_HDMITX_PLL_AUTOK_EN	28
#define RG_HDMITX_PLL_AUTOK_KF	26
#define RG_HDMITX_PLL_AUTOK_KS	24
#define RG_HDMITX_PLL_AUTOK_LOAD	23
#define RG_HDMITX_PLL_BAND	16
#define RG_HDMITX_PLL_REF_SEL	15
#define RG_HDMITX_PLL_BIAS_EN	14
#define RG_HDMITX_PLL_BIAS_LPF_EN	13
#define RG_HDMITX_PLL_TXDIV_EN	12
#define RG_HDMITX_PLL_TXDIV	10
#define RG_HDMITX_PLL_LVROD_EN	9
#define RG_HDMITX_PLL_MONVC_EN	8
#define RG_HDMITX_PLL_MONCK_EN	7
#define RG_HDMITX_PLL_MONREF_EN	6
#define RG_HDMITX_PLL_TST_EN	5
#define RG_HDMITX_PLL_TST_CK_EN	4
#define RG_HDMITX_PLL_TST_SEL	0

#define HDMI_CON2    0x108
#define RGS_HDMITX_PLL_AUTOK_BAND_MASK    (0x7F << 8)
#define RGS_HDMITX_PLL_AUTOK_FAIL_MASK    (0x01 << 1)
#define RGS_HDMITX_PLL_AUTOK_PASS_MASK    (0x01 << 0)
#define RGS_HDMITX_PLL_AUTOK_BAND	8
#define RGS_HDMITX_PLL_AUTOK_FAIL	1
#define RGS_HDMITX_PLL_AUTOK_PASS	0

#define HDMI_CON3    0x10C
#define RG_HDMITX_SER_EN_MASK    (0x0F << 28)
#define RG_HDMITX_PRD_EN_MASK    (0x0F << 24)
#define RG_HDMITX_PRD_IMP_EN_MASK    (0x0F << 20)
#define RG_HDMITX_DRV_EN_MASK    (0x0F << 16)
#define RG_HDMITX_DRV_IMP_EN_MASK    (0x0F << 12)
#define RG_HDMITX_MHLCK_FORCE_MASK    (0x01 << 10)
#define RG_HDMITX_MHLCK_PPIX_EN_MASK    (0x01 << 9)
#define RG_HDMITX_MHLCK_EN_MASK    (0x01 << 8)
#define RG_HDMITX_SER_DIN_SEL_MASK    (0x0F << 4)
#define RG_HDMITX_SER_5T1_BIST_EN_MASK    (0x01 << 3)
#define RG_HDMITX_SER_BIST_TOG_MASK    (0x01 << 2)
#define RG_HDMITX_SER_DIN_TOG_MASK    (0x01 << 1)
#define RG_HDMITX_SER_CLKDIG_INV_MASK    (0x01 << 0)
#define RG_HDMITX_SER_EN	28
#define RG_HDMITX_PRD_EN	24
#define RG_HDMITX_PRD_IMP_EN	20
#define RG_HDMITX_DRV_EN	16
#define RG_HDMITX_DRV_IMP_EN	12
#define RG_HDMITX_MHLCK_FORCE	10
#define RG_HDMITX_MHLCK_PPIX_EN	9
#define RG_HDMITX_MHLCK_EN	8
#define RG_HDMITX_SER_DIN_SEL	4
#define RG_HDMITX_SER_5T1_BIST_EN	3
#define RG_HDMITX_SER_BIST_TOG	2
#define RG_HDMITX_SER_DIN_TOG	1
#define RG_HDMITX_SER_CLKDIG_INV	0

#define HDMI_CON4    0x110
#define RG_HDMITX_PRD_IBIAS_CLK_MASK    (0x0F << 24)
#define RG_HDMITX_PRD_IBIAS_D2_MASK    (0x0F << 16)
#define RG_HDMITX_PRD_IBIAS_D1_MASK    (0x0F << 8)
#define RG_HDMITX_PRD_IBIAS_D0_MASK    (0x0F << 0)
#define RG_HDMITX_PRD_IBIAS_CLK	24
#define RG_HDMITX_PRD_IBIAS_D2	16
#define RG_HDMITX_PRD_IBIAS_D1	8
#define RG_HDMITX_PRD_IBIAS_D0	0

#define HDMI_CON5    0x114
#define RG_HDMITX_DRV_IBIAS_CLK_MASK    (0x3F << 24)
#define RG_HDMITX_DRV_IBIAS_D2_MASK    (0x3F << 16)
#define RG_HDMITX_DRV_IBIAS_D1_MASK    (0x3F << 8)
#define RG_HDMITX_DRV_IBIAS_D0_MASK    (0x3F << 0)
#define RG_HDMITX_DRV_IBIAS_CLK	24
#define RG_HDMITX_DRV_IBIAS_D2	16
#define RG_HDMITX_DRV_IBIAS_D1	8
#define RG_HDMITX_DRV_IBIAS_D0	0

#define HDMI_CON6    0x118
#define RG_HDMITX_DRV_IMP_CLK_MASK    (0x3F << 24)
#define RG_HDMITX_DRV_IMP_D2_MASK    (0x3F << 16)
#define RG_HDMITX_DRV_IMP_D1_MASK    (0x3F << 8)
#define RG_HDMITX_DRV_IMP_D0_MASK    (0x3F << 0)
#define RG_HDMITX_DRV_IMP_CLK	24
#define RG_HDMITX_DRV_IMP_D2	16
#define RG_HDMITX_DRV_IMP_D1	8
#define RG_HDMITX_DRV_IMP_D0	0

#define HDMI_CON7    0x11C
#define RG_HDMITX_MHLCK_DRV_IBIAS_MASK    (0x1F << 27)
#define RG_HDMITX_SER_DIN_MASK    (0x3FF << 16)
#define RG_HDMITX_CHLDC_TST_MASK    (0x0F << 12)
#define RG_HDMITX_CHLCK_TST_MASK    (0x0F << 8)
#define RG_HDMITX_RESERVE_MASK    (0xFF << 0)
#define RG_HDMITX_MHLCK_DRV_IBIAS	27
#define RG_HDMITX_SER_DIN	16
#define RG_HDMITX_CHLDC_TST	12
#define RG_HDMITX_CHLCK_TST	8
#define RG_HDMITX_RESERVE	0

#define HDMI_CON8    0x120
#define RGS_HDMITX_2T1_LEV_MASK    (0x0F << 16)
#define RGS_HDMITX_2T1_EDG_MASK    (0x0F << 12)
#define RGS_HDMITX_5T1_LEV_MASK    (0x0F << 8)
#define RGS_HDMITX_5T1_EDG_MASK    (0x0F << 4)
#define RGS_HDMITX_PLUG_TST_MASK    (0x01 << 0)
#define RGS_HDMITX_2T1_LEV	16
#define RGS_HDMITX_2T1_EDG	12
#define RGS_HDMITX_5T1_LEV	8
#define RGS_HDMITX_5T1_EDG	4
#define RGS_HDMITX_PLUG_TST	0

#define HDMI_CON9    0x124
#define RG_HDMITX_DCLK_MASK    (0x3FF << 0)
#define RG_HDMITX_DCLK	0

      /* /////////////////////////////////////////////////////// */
#define MHL_AP_PLL_CON0 0
#define RG_CLKSQ_LPF_EN         1
#define RG_CLKSQ_LPF_EN_MASK         (1 << 1)
#define MHL_TVDPLL_CON0 0x270
#define RG_TVDPLL_DIV3_EN_MASK    (0x01 << 31)
#define RG_TVDPLL_DIV4_EN_MASK    (0x01 << 30)
#define RG_TVDPLL_CON0_RSV0_MASK    (0x7F << 9)
#define RG_TVDPLL_SDM_FRA_EN_MASK    (0x01 << 8)
#define RG_TVDPLL_POSDIV_MASK    (0x07 << 4)
#define RG_TVDPLL_EN_MASK    (0x01 << 0)
#define RG_TVDPLL_DIV3_EN	31
#define RG_TVDPLL_DIV4_EN	30
#define RG_TVDPLL_CON0_RSV0	9
#define RG_TVDPLL_SDM_FRA_EN	8
#define RG_TVDPLL_POSDIV	4
#define RG_TVDPLL_EN	0

#define MHL_TVDPLL_CON1 0x274
#define RG_TVDPLL_SDM_PCW_CHG_MASK    (0x01 << 31)
#define RG_TVDPLL_SDM_PCW_MASK    (0x1FFFFF << 0)
#define RG_TVDPLL_SDM_PCW_CHG	31
#define RG_TVDPLL_SDM_PCW	0

#define MHL_TVDPLL_CON2 0x278
#define RG_TVDPLL_SDM_PCW_RO_MASK    (0x1FFFFF << 0)
#define RG_TVDPLL_SDM_PCW_RO	0

#define MHL_TVDPLL_PWR_CON0 0x27C
#define RG_TVDPLL_SDM_PWR_ACK_MASK    (0x01 << 31)
#define RG_TVDPLL_SDM_ISO_EN_MASK    (0x01 << 1)
#define RG_TVDPLL_SDM_PWR_ON_MASK    (0x01 << 0)
#define RG_TVDPLL_SDM_PWR_ACK	31
#define RG_TVDPLL_SDM_ISO_EN	1
#define RG_TVDPLL_SDM_PWR_ON	0

    /* //////////////////////////////////////////////////////////// */
#define MHL_ANA_CKGEN_PORT	(*((volatile unsigned int *) 0x10000168))

#define ANA_CKGEN_CFG5			0xC0
#define CFG5_CLK_HDMI_EN		(15)
#define CFG5_CLK_HDMI_EN_MASK		(1 << 15)
#define CFG5_CLK_HDMI_SEL			8
#define CFG5_CLK_HDMI_SEL_MASK		(0x03 << 8)
#define ANA_CKGEN_CFG6			0xA0
#define CFG4_CLK_DPI0_EN		(7)
#define CFG4_CLK_DPI0_EN_MASK		(1 << 7)
#define CFG4_CLK_DPI0_SEL			0
#define CFG4_CLK_DPI0_SEL_MASK		(0x07 << 0)
/* ////////////////////////////////////////////////////////////// */

#define DPI_EN							(0x00)
#define DPI_RST							(0x04)
#define DPI_INTEN						(0x08)
#define DPI_INTSTA						(0x0C)
#define DPI_CON							(0x10)
#define DPI_OUTPUT_SETTING				(0x14)
#define DPI_SIZE							(0x18)
#define DPI_DDR_SETTING					(0x1C)
#define DPI_TGEN_HWIDTH				(0x20)
#define DPI_TGEN_HPORCH				(0x24)
#define DPI_TGEN_VWIDTH				(0x28)
#define DPI_TGEN_VPORCH				(0x2C)
#define DPI_BG_HCNTL					(0x30)
#define DPI_BG_VCNTL					(0x34)
#define DPI_BG_COLOR					(0x38)
#define DPI_FIFO_CTL						(0x3C)
#define DPI_STATUS						(0x40)
#define DPI_TMODE						(0x44)
#define DPI_CHKSUM						(0x48)
#define DPI_DUMMY						(0x50)
#define DPI_TGEN_VWIDTH_LEVEN			(0x68)
#define DPI_TGEN_VPORCH_LEVEN			(0x6C)
#define DPI_TGEN_VWIDTH_RODD			(0x70)
#define DPI_TGEN_VPORCH_RODD			(0x74)
#define DPI_TGEN_VWIDTH_REVEN			(0x78)
#define DPI_TGEN_VPORCH_REVEN			(0x7C)
#define DPI_ESAV_VTIM_LODD				(0x80)
#define DPI_ESAV_VTIM_LEVEN			(0x84)
#define DPI_ESAV_VTIM_RODD				(0x88)
#define DPI_ESAV_VTIM_REVEN			(0x8C)
#define DPI_ESAV_FTIM					(0x90)
#define DPI_CLPF_SETTING				(0x94)
#define DPI_Y_LIMIT						(0x98)
#define DPI_C_LIMIT						(0x9C)
#define DPI_YUV422_SETTING				(0xA0)
#define DPI_EMBSYNC_SETTING			(0xA4)
#define DPI_ESAV_CODE_SET0				(0xA8)
#define DPI_ESAV_CODE_SET1				(0xAC)
#define DPI_CLKCON						(0xE0)
#define DPI_PATTERN						(0xF00)

#define dpi_en_addr				(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_EN)))	/* 0 */
#define dpi_rst_addr				(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_RST)))	/* 0x04 */
#define dpi_con_addr				(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_CON)))	/* 0x10 */
#define dpi_output_setting_addr	(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_OUTPUT_SETTING)))	/* 0x14 */
#define dpi_clkcon_addr			(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_CLKCON)))	/* 0xb0 */
#define dpi_intsta_addr			(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_INTSTA)))	/* 0x0c */
#define dpi_size_addr				(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_SIZE)))	/* 0x18 */

#define dpi_hwidth_addr			(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_HWIDTH)))	/* 0x20 */
#define dpi_hporch_addr			(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_HPORCH)))	/* 0x24 */
#define dpi_vwidth_lodd_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VWIDTH)))	/* 0x28 */
#define dpi_vporch_lodd_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VPORCH)))	/* 0x2c */
#define dpi_vwidth_leven_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VWIDTH_LEVEN)))	/* 0x68 */
#define dpi_vporch_leven_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VPORCH_LEVEN)))	/* 0x6c */
#define dpi_vwidth_rodd_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VWIDTH_RODD)))	/* 0x70 */
#define dpi_vporch_rodd_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VPORCH_RODD)))	/* 0x74 */
#define dpi_vwidth_reven_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VWIDTH_REVEN)))	/* 0x78 */
#define dpi_vporch_reven_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_TGEN_VPORCH_REVEN)))	/* 0x7c */

#define dpi_pattern_gen_addr		(*((volatile unsigned int *)(HDMI_DPI0_BASE + DPI_PATTERN)))

#endif
#endif
