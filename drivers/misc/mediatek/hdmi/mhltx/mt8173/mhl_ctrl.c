#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <mach/mt_boot.h>
#include <linux/io.h>

#include "inter_mhl_drv.h"
#include "mhl_ctrl.h"
#include "mhl_edid.h"
#include "mhl_hdcp.h"
#include "mhl_table.h"
#include "mhl_cbus.h"
#include "mhl_cbus_ctrl.h"
#include "mhl_avd.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
#include "hdmi_ca.h"
#endif

HDMI_AV_INFO_T _stAvdAVInfo = { 0 };

static unsigned char _bAudInfoFm[5];
static unsigned char _bAviInfoFm[5];
static unsigned char _bSpdInf[25] = { 0 };

static unsigned char _bAcpType;
static unsigned char _bAcpData[16] = { 0 };
static unsigned char _bIsrc1Data[16] = { 0 };

static unsigned int _u4NValue;
extern u8 _bflagvideomute;
extern u8 _bflagaudiomute;
extern unsigned char _bsvpvideomute;
extern unsigned char _bsvpaudiomute;
extern unsigned char _bHdcpOff;

static const char *szHdmiResStr[HDMI_VIDEO_RESOLUTION_NUM] = {
	"RES_480P",
	"RES_576P",
	"RES_720P60HZ",
	"RES_720P50HZ",
	"RES_1080I60HZ",
	"RES_1080I50HZ",
	"RES_1080P30HZ",
	"RES_1080P25HZ",
	"RES_1080P24HZ",
	"RES_1080P23_976HZ",
	"RES_1080P29_97HZ",
	"RES_1080P60HZ",
	"RES_1080P50HZ",
	"RES_720P60HZ_3D",
	"RES_720P50HZ_3D",
	"RES_1080P24HZ_3D",
	"RES_1080P23_976HZ_3D"
};

static const char *cHdmiAudFsStr[7] = {
	"HDMI_FS_32K",
	"HDMI_FS_44K",
	"HDMI_FS_48K",
	"HDMI_FS_88K",
	"HDMI_FS_96K",
	"HDMI_FS_176K",
	"HDMI_FS_192K"
};


static const char *cAudCodingTypeStr[16] = {
	"Refer to Stream Header",
	"PCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"ONE Bit Audio",
	"Dolby Digital+",
	"DTS-HD",
	"MAT(MLP)",
	"DST",
	"WMA Pro",
	"Reserved",
};

static const char *cAudChCountStr[8] = {
	"Refer to Stream Header",
	"2ch",
	"3ch",
	"4ch",
	"5ch",
	"6ch",
	"7ch",
	"8ch",

};

static const char *cAudFsStr[8] = {
	"Refer to Stream Header",
	"32 khz",
	"44.1 khz",
	"48 khz",
	"88.2 khz",
	"96 khz",
	"176.4 khz",
	"192 khz"
};


static const char *cAudChMapStr[32] = {
	"FR,FL",
	"LFE,FR,FL",
	"FC,FR,FL",
	"FC,LFE,FR,FL",
	"RC,FR,FL",
	"RC,LFE,FR,FL",
	"RC,FC,FR,FL",
	"RC,FC,LFE,FR,FL",
	"RR,RL,FR,FL",
	"RR,RL,LFE,FR,FL",
	"RR,RL,FC,FR,FL",
	"RR,RL,FC,LFE,FR,FL",
	"RC,RR,RL,FR,FL",
	"RC,RR,RL,LFE,FR,FL",
	"RC,RR,RL,FC,FR,FL",
	"RC,RR,RL,FC,LFE,FR,FL",
	"RRC,RLC,RR,RL,FR,FL",
	"RRC,RLC,RR,RL,LFE,FR,FL",
	"RRC,RLC,RR,RL,FC,FR,FL",
	"RRC,RLC,RR,RL,FC,LFE,FR,FL",
	"FRC,FLC,FR,FL",
	"FRC,FLC,LFE,FR,FL",
	"FRC,FLC,FC,FR,FL",
	"FRC,FLC,FC,LFE,FR,FL",
	"FRC,FLC,RC,FR,FL",
	"FRC,FLC,RC,LFE,FR,FL",
	"FRC,FLC,RC,FC,FR,FL",
	"FRC,FLC,RC,FC,LFE,FR,FL",
	"FRC,FLC,RR,RL,FR,FL",
	"FRC,FLC,RR,RL,LFE,FR,FL",
	"FRC,FLC,RR,RL,FC,FR,FL",
	"FRC,FLC,RR,RL,FC,LFE,FR,FL",
};

static const char *cAudDMINHStr[2] = {
	"Permiited down mixed stereo or no information",
	"Prohibited down mixed stereo"
};

static const char *cAudSampleSizeStr[4] = {
	"Refer to Stream Header",
	"16 bit",
	"20 bit",
	"24 bit"
};

static const char *cAviRgbYcbcrStr[4] = {
	"RGB",
	"YCbCr 4:2:2",
	"YCbCr 4:4:4",
	"Future"
};

static const char *cAviActivePresentStr[2] = {
	"No data",
	"Actuve Format(R0..R3) Valid",

};

static const char *cAviBarStr[4] = {
	"Bar data not valid",
	"Vert. Bar info valid",
	"Horiz. Bar info valid",
	"Vert. and Horiz Bar info valid",
};

static const char *cAviScanStr[4] = {
	"No data",
	"Overscanned display",
	"underscanned display",
	"Future",
};

static const char *cAviColorimetryStr[4] = {
	"no data",
	"ITU601",
	"ITU709",
	"Extended Colorimetry infor valid",
};

static const char *cAviAspectStr[4] = {
	"No data",
	"4:3",
	"16:9",
	"Future",
};


static const char *cAviActiveStr[16] = {
	"reserved",
	"reserved",
	"box 16:9(top)",
	"box 14:9(top)",
	"box > 16:9(center)",
	"reserved",
	"reserved",
	"reserved",
	"Same as picture aspect ratio",
	"4:3(Center)",
	"16:9(Center)",
	"14:9(Center)",
	"reserved",
	"4:3(with shoot & protect 14:9 center)",
	"16:9(with shoot & protect 14:9 center)",
	"16:3(with shoot & protect 4:3 center)"
};

static const char *cAviItContentStr[2] = {
	"no data",
	"IT Content"
};

static const char *cAviExtColorimetryStr[2] = {
	"xvYCC601",
	"xvYCC709",
};

static const char *cAviRGBRangeStr[4] = {
	"depends on video format",
	"Limit range",
	"FULL range",
	"Reserved",
};

static const char *cAviScaleStr[4] = {
	"Unkown non-uniform scaling",
	"Picture has been scaled horizontally",
	"Picture has been scaled vertically",
	"Picture has been scaled horizontally and vertically",
};



static const char *cSPDDeviceStr[16] = {
	"unknown",
	"Digital STB",
	"DVD Player",
	"D-VHS",
	"HDD Videorecorder",
	"DVC",
	"DSC",
	"Video CD",
	"Game",
	"PC General",
	"Blu-Ray Disc",
	"Super Audio CD",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
};

/* ///////////////////////////////////////////////////// */
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
void vWriteByteHdmiGRL(unsigned int dAddr, unsigned int dVal)
{
	MHL_DRV_LOG("[W]addr = 0x%04x, data = 0x%08x\n", dAddr, dVal);
	vCaHDMIWriteReg(dAddr, dVal);
}
#endif
unsigned char fgIsAcpEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ACP_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsVSEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & VS_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsISRC1Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC1_EN)
		return TRUE;
	else
		return FALSE;
}


unsigned char fgIsISRC2Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC2_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char bCheckStatus(void)
{
	unsigned char bStatus = 0;
	/* MHL_PLUG_FUNC(); */
	bStatus = bReadByteHdmiGRL(GRL_STATUS);

	if (bStatus & STATUS_PORD) {
		return TRUE;
	} else {
		return FALSE;
	}
}

unsigned char bCheckPordHotPlug(void)
{

	return bCheckStatus();

}

void MuteHDMIAudio(void)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	MHL_AUDIO_FUNC();

	fgCaHDMIAudioUnMute(FALSE);
#else
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData |= AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
#endif
}

void mhl_black_pattern_set(unsigned char cs)
{
	MHL_DRV_FUNC();

	if ((cs == HDMI_RGB) || (cs == HDMI_RGB_FULL)) {
		vWriteByteHdmiGRL(HDMI_BGB_VALUE1, 0x00);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE2, 0x01);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE3, 0x10);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE4, 0x00);
		vWriteHdmiGRLMsk(VIDEO_SOURCE_PATH, 0x01, 0x0f);
	} else {
		vWriteByteHdmiGRL(HDMI_BGB_VALUE1, 0x00);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE2, 0x08);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE3, 0x10);
		vWriteByteHdmiGRL(HDMI_BGB_VALUE4, 0x00);
		vWriteHdmiGRLMsk(VIDEO_SOURCE_PATH, 0x08, 0x0f);
	}
}

void vBlackHDMIOnly(void)
{
	MHL_DRV_FUNC();
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	fgCaHDMIVideoUnMute(FALSE);
#else
	vWriteHdmiGRLMsk(VIDEO_SOURCE_PATH, 0, VIDEO_SOURCE_SEL);
#endif

}


void vHDMIAVMute(void)
{
	MHL_AUDIO_FUNC();

	vBlackHDMIOnly();
	MuteHDMIAudio();
}

void vSetCTL0BeZero(unsigned char fgBeZero)
{
	unsigned char bTemp;

	MHL_VIDEO_FUNC();

	if (fgBeZero == TRUE) {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp |= (1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	} else {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp &= ~(1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	}

}

void vUnBlackHDMIOnly(void)
{
	MHL_DRV_FUNC();
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	if (get_boot_mode() != FACTORY_BOOT)
		fgCaHDMIVideoUnMute(TRUE);
	else
		vWriteHdmiGRLMsk(VIDEO_SOURCE_PATH, VIDEO_SOURCE_SEL, VIDEO_SOURCE_SEL);
#else
	vWriteHdmiGRLMsk(VIDEO_SOURCE_PATH, VIDEO_SOURCE_SEL, VIDEO_SOURCE_SEL);
#endif
}

void UnMuteHDMIAudio(void)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	MHL_AUDIO_FUNC();

	fgCaHDMIAudioUnMute(TRUE);
#else
	unsigned char bData;
	MHL_AUDIO_FUNC();

	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
#endif
}

void vMhlAnalogInit(void)
{
	vWriteHdmiANA(HDMI_CON0, 0x40110000);
	vWriteHdmiANA(HDMI_CON1, 0x1F810000);
	vWriteHdmiANA(HDMI_CON2, 0x00000000);
	vWriteHdmiANA(HDMI_CON3, 0x00000000);
	vWriteHdmiANA(HDMI_CON4, 0x05050505);
	vWriteHdmiANA(HDMI_CON5, 0x0A0A0A0A);
	vWriteHdmiANA(HDMI_CON6, 0x36363636);
	vWriteHdmiANA(HDMI_CON7, 0xE0CC00F0);
	vWriteHdmiANA(HDMI_CON8, 0x00000000);
	vWriteHdmiANA(HDMI_CON9, 0x000003E0);
}

void vMhlAnalogPD(void)
{
	vWriteHdmiANAMsk(HDMI_CON1, 0, RG_HDMITX_PLL_BIAS_EN_MASK);

	vWriteHdmiANAMsk(HDMI_CON0, 0, RG_HDMITX_PLL_EN_MASK);

	vWriteHdmiANAMsk(HDMI_CON1, 0, RG_HDMITX_PLL_BIAS_LPF_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON1, 0, RG_HDMITX_PLL_TXDIV_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_SER_EN), RG_HDMITX_SER_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_PRD_EN), RG_HDMITX_PRD_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_DRV_EN), RG_HDMITX_DRV_EN_MASK);
}

void vMhlTriggerPLL(void)
{
	vWriteHdmiANAMsk(HDMI_CON1, (1 << RG_HDMITX_PLL_AUTOK_EN), RG_HDMITX_PLL_AUTOK_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON0, (0 << RG_HDMITX_PLL_POSDIV), RG_HDMITX_PLL_POSDIV_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (1 << RG_HDMITX_MHLCK_EN), RG_HDMITX_MHLCK_EN_MASK);

	vMhlAnalogPD();

	/* step1 */
	vWriteHdmiANAMsk(HDMI_CON1, (1 << RG_HDMITX_PLL_BIAS_EN), RG_HDMITX_PLL_BIAS_EN_MASK);
	udelay(2);
	/* step2 */
	vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_EN), RG_HDMITX_PLL_EN_MASK);
	udelay(100);
	/* step3 */
	vWriteHdmiANAMsk(HDMI_CON1, (1 << RG_HDMITX_PLL_BIAS_LPF_EN),
			 RG_HDMITX_PLL_BIAS_LPF_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON1, (1 << RG_HDMITX_PLL_TXDIV_EN), RG_HDMITX_PLL_TXDIV_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (8 << RG_HDMITX_SER_EN), RG_HDMITX_SER_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON3, (8 << RG_HDMITX_PRD_EN), RG_HDMITX_PRD_EN_MASK);
	/* vWriteHdmiANAMsk(HDMI_CON3,(8 << RG_HDMITX_DRV_EN),RG_HDMITX_DRV_EN_MASK); */

}

void vMhlSignalOff(bool fgEn)
{
	if (fgEn)
		vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_DRV_EN), RG_HDMITX_DRV_EN_MASK);
	else
		vWriteHdmiANAMsk(HDMI_CON3, (8 << RG_HDMITX_DRV_EN), RG_HDMITX_DRV_EN_MASK);
}

void vMhlSetPLL(MHL_RES_PLL res)
{
	vMhlAnalogPD();

#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0 << RG_TVDPLL_EN), RG_TVDPLL_EN_MASK);

	vWriteHdmiANAMsk(MHL_AP_PLL_CON0, 0, RG_CLKSQ_LPF_EN_MASK);

	vWriteHdmiANAMsk(MHL_TVDPLL_PWR_CON0, RG_TVDPLL_SDM_PWR_ON_MASK, RG_TVDPLL_SDM_PWR_ON_MASK);

	udelay(5);
	vWriteHdmiANAMsk(MHL_AP_PLL_CON0, RG_CLKSQ_LPF_EN_MASK, RG_CLKSQ_LPF_EN_MASK);

	/* TVD PLL */
	switch (res) {
	case MHL_PLL_27:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (816679 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK),
				 (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK));

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (1 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN_MASK);
		vWriteHdmiANAMsk(0x40, (3 << 16), (0x07 << 16));
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN_MASK, RG_TVDPLL_EN_MASK);
		break;
	case MHL_PLL_74175:
	case MHL_PLL_74175PP:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1121800 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK),
				 (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK));

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN_MASK);
		vWriteHdmiANAMsk(0x40, (3 << 16), (0x07 << 16));
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN_MASK, RG_TVDPLL_EN_MASK);
		break;
	case MHL_PLL_7425:
	case MHL_PLL_7425PP:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1122934 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK),
				 (RG_TVDPLL_DIV4_EN_MASK | RG_TVDPLL_DIV3_EN_MASK));

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);

		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN_MASK);
		vWriteHdmiANAMsk(0x40, (3 << 16), (0x07 << 16));
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN_MASK, RG_TVDPLL_EN_MASK);
		break;
	default:
		break;
	}
	udelay(20);
#endif

	switch (res) {
	case MHL_PLL_27:
		/* HDMITX PLL feq */
		vWriteHdmiANAMsk(HDMI_CON0, (0 << RG_HDMITX_PLL_PREDIV), RG_HDMITX_PLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (2 << RG_HDMITX_PLL_TXDIV), RG_HDMITX_PLL_TXDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_FBKSEL), RG_HDMITX_PLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (29 << RG_HDMITX_PLL_FBKDIV),
				 RG_HDMITX_PLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (0 << RG_HDMITX_PLL_POSDIV), RG_HDMITX_PLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_MHLCK_PPIX_EN),
				 RG_HDMITX_MHLCK_PPIX_EN_MASK);
		/* HDMITX PLL performance */
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IC), RG_HDMITX_PLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IR), RG_HDMITX_PLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (1 << RG_HDMITX_PLL_DIVEN), RG_HDMITX_PLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (14 << RG_HDMITX_PLL_BP), RG_HDMITX_PLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (3 << RG_HDMITX_PLL_BC), RG_HDMITX_PLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_BR), RG_HDMITX_PLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_PRD_IMP_EN), RG_HDMITX_PRD_IMP_EN_MASK);
		vWriteHdmiANAMsk(HDMI_CON4, (3 << RG_HDMITX_PRD_IBIAS_CLK),
				 RG_HDMITX_PRD_IBIAS_CLK_MASK);
		break;
	case MHL_PLL_74175:
	case MHL_PLL_7425:
		/* HDMITX PLL feq */
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_PREDIV), RG_HDMITX_PLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (0 << RG_HDMITX_PLL_TXDIV), RG_HDMITX_PLL_TXDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_FBKSEL), RG_HDMITX_PLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (14 << RG_HDMITX_PLL_FBKDIV),
				 RG_HDMITX_PLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (0 << RG_HDMITX_PLL_POSDIV), RG_HDMITX_PLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_MHLCK_PPIX_EN),
				 RG_HDMITX_MHLCK_PPIX_EN_MASK);
		/* HDMITX PLL performance */
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IC), RG_HDMITX_PLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IR), RG_HDMITX_PLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (2 << RG_HDMITX_PLL_DIVEN), RG_HDMITX_PLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (15 << RG_HDMITX_PLL_BP), RG_HDMITX_PLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (3 << RG_HDMITX_PLL_BC), RG_HDMITX_PLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (2 << RG_HDMITX_PLL_BR), RG_HDMITX_PLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON3, (0 << RG_HDMITX_PRD_IMP_EN), RG_HDMITX_PRD_IMP_EN_MASK);
		vWriteHdmiANAMsk(HDMI_CON4, (3 << RG_HDMITX_PRD_IBIAS_CLK),
				 RG_HDMITX_PRD_IBIAS_CLK_MASK);
		break;
	case MHL_PLL_74175PP:
	case MHL_PLL_7425PP:
		/* HDMITX PLL feq */
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_PREDIV), RG_HDMITX_PLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (0 << RG_HDMITX_PLL_TXDIV), RG_HDMITX_PLL_TXDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_FBKSEL), RG_HDMITX_PLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (19 << RG_HDMITX_PLL_FBKDIV),
				 RG_HDMITX_PLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (0 << RG_HDMITX_PLL_POSDIV), RG_HDMITX_PLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON3, (1 << RG_HDMITX_MHLCK_PPIX_EN),
				 RG_HDMITX_MHLCK_PPIX_EN_MASK);
		/* HDMITX PLL performance */
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IC), RG_HDMITX_PLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_IR), RG_HDMITX_PLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON1, (2 << RG_HDMITX_PLL_DIVEN), RG_HDMITX_PLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (12 << RG_HDMITX_PLL_BP), RG_HDMITX_PLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (2 << RG_HDMITX_PLL_BC), RG_HDMITX_PLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON0, (1 << RG_HDMITX_PLL_BR), RG_HDMITX_PLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON3, (8 << RG_HDMITX_PRD_IMP_EN), RG_HDMITX_PRD_IMP_EN_MASK);
		vWriteHdmiANAMsk(HDMI_CON4, (6 << RG_HDMITX_PRD_IBIAS_CLK),
				 RG_HDMITX_PRD_IBIAS_CLK_MASK);
		break;
	default:
		break;
	}
	vWriteHdmiANAMsk(HDMI_CON3, (0x08 << RG_HDMITX_DRV_IMP_EN), RG_HDMITX_DRV_IMP_EN_MASK);
	vWriteHdmiANAMsk(HDMI_CON6, (0x26 << RG_HDMITX_DRV_IMP_CLK), RG_HDMITX_DRV_IMP_CLK_MASK);
	vWriteHdmiANAMsk(HDMI_CON5, (0x1d << RG_HDMITX_DRV_IBIAS_CLK),
			 RG_HDMITX_DRV_IBIAS_CLK_MASK);
	vWriteHdmiANAMsk(HDMI_CON7, (0x1c << RG_HDMITX_MHLCK_DRV_IBIAS),
			 RG_HDMITX_MHLCK_DRV_IBIAS_MASK);


	vMhlTriggerPLL();
}

void vMhlSetDigital(MHL_RES_PLL res)
{
#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG6, (0 << CFG4_CLK_DPI0_EN), CFG4_CLK_DPI0_EN_MASK);
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (0 << CFG5_CLK_HDMI_EN), CFG5_CLK_HDMI_EN_MASK);
#else
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (0 << CFG5_CLK_HDMI_EN), CFG5_CLK_HDMI_EN_MASK);
#endif
	udelay(100);
	switch (res) {
	case MHL_PLL_27:
		/* id_pll,AD_TVDPLL_CK/8, 216/8=27mhz */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG6, (0x05 << CFG4_CLK_DPI0_SEL),
				   CFG4_CLK_DPI0_SEL_MASK);
		/* pll_clk */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (0x03 << CFG5_CLK_HDMI_SEL),
				   CFG5_CLK_HDMI_SEL_MASK);
		break;
	case MHL_PLL_74175:
	case MHL_PLL_7425:
		/* id_pll,AD_TVDPLL_CK/8, 594/8=74mhz */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG6, (0x05 << CFG4_CLK_DPI0_SEL),
				   CFG4_CLK_DPI0_SEL_MASK);
		/* pll_clk */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (0x03 << CFG5_CLK_HDMI_SEL),
				   CFG5_CLK_HDMI_SEL_MASK);
		break;
	case MHL_PLL_74175PP:
	case MHL_PLL_7425PP:
		/* id_pll,AD_TVDPLL_CK/4, 594/4=148mhz */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG6, (0x02 << CFG4_CLK_DPI0_SEL),
				   CFG4_CLK_DPI0_SEL_MASK);
		/* pll_clk */
		vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (0x02 << CFG5_CLK_HDMI_SEL),
				   CFG5_CLK_HDMI_SEL_MASK);
		break;
	default:
		break;
	}
	udelay(100);
#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteHdmiSYSMsk(DISP_CG_CON1, 0, (1 << 8) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));
#else
	vWriteHdmiSYSMsk(DISP_CG_CON1, 0, (1 << 8) | (1 << 12) | (1 << 13));
#endif
	udelay(100);
}

void vMhlFifoEnable(unsigned char bResIndex)
{
	MHL_RES_PLL res = MHL_PLL_27;

	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		res = MHL_PLL_27;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
		res = MHL_PLL_74175;
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		res = MHL_PLL_7425;
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
		res = MHL_PLL_74175PP;
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		res = MHL_PLL_7425PP;
		break;
	default:
		printk("can not support resolution\n");
		break;
	}

	udelay(100);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_MODE_ON, MHL_MODE_ON);
	vWriteHdmiGRLMsk(GRL_CFG4, 0, CFG4_MHL_MODE);

	/* *(unsigned int*)0x1400f0a8 = 0; */
	switch (res) {
	case MHL_PLL_27:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		break;
	case MHL_PLL_74175:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		break;
	case MHL_PLL_7425:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		break;
	case MHL_PLL_74175PP:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_PP_MODE, MHL_PP_MODE);
		vWriteHdmiGRLMsk(GRL_CFG4, CFG4_MHL_MODE, CFG4_MHL_MODE);
		/* *(unsigned int*)0x1400f0a8 = 0x00000020; */
		break;
	case MHL_PLL_7425PP:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_PP_MODE, MHL_PP_MODE);
		vWriteHdmiGRLMsk(GRL_CFG4, CFG4_MHL_MODE, CFG4_MHL_MODE);
		/* *(unsigned int*)0x1400f0a8 = 0x00000020; */
		break;
	default:
		break;
	}
	udelay(100);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, HDMI_OUT_FIFO_EN, HDMI_OUT_FIFO_EN);

}

void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable)
{
	MHL_DRV_FUNC();

	if (fgHdmiTmdsEnable == 1) {
		vMhlSignalOff(0);
	} else {
		vHDMIAVMute();
		mdelay(1);
		vHDCPReset();
		vMhlSignalOff(1);
		mdelay(100);
	}
}

void vVideoPLLInit(void)
{
	MHL_DRV_FUNC();
	/* init analog part */
	vMhlAnalogInit();
}

void vSetHDMITxPLL(unsigned char bResIndex)
{
	MHL_PLL_FUNC();

	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		vMhlSetPLL(MHL_PLL_27);
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
		vMhlSetPLL(MHL_PLL_74175);
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		vMhlSetPLL(MHL_PLL_7425);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
		vMhlSetPLL(MHL_PLL_74175PP);
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		vMhlSetPLL(MHL_PLL_7425PP);
		break;
	default:
		printk("[MHL]can not support resolution\n");
		break;

	}

}

#ifdef MHL_INTER_PATTERN_FOR_DBG
void dpi_set_res(unsigned char res)
{
	DPI_STRUCT dpi;

	memset((void *)&dpi, 0, sizeof(DPI_STRUCT));

	dpi.rgb2yuv_en = 0;
	dpi.yuv422_en = 0;

	dpi.vsync_pol = 1;
	dpi.hsync_pol = 1;
	switch (res) {
	case HDMI_VIDEO_720x480p_60Hz:	/* 480p */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsync_pol = 0;
		dpi.hsync_pol = 0;

		dpi.vsize = 480;
		dpi.hsize = 720;
		dpi.hpw = 62;
		dpi.hfp = 16;
		dpi.hbp = 60;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 6;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 9;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 30;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:	/* 720p */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 720;
		dpi.hsize = 1280;
		dpi.hpw = 40;
		dpi.hfp = 110;
		dpi.hbp = 220;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 5;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 20;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080i_60Hz:	/* 1080i60 */
	case HDMI_VIDEO_1920x1080i3d_sbs_60Hz:	/* 1080i60 */
		dpi.tdfp_en = 0;
		dpi.intl_en = 1;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 1;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 540;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 88;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 2;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 15;
		dpi.vpw_half_leven = 1;
		dpi.vpw_leven = 5;
		dpi.vfp_half_leven = 1;
		dpi.vfp_leven = 2;
		dpi.vbp_half_leven = 1;
		dpi.vbp_leven = 16;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:	/* 1080p60 */
		/* 422 */
		dpi.rgb2yuv_en = 1;
		dpi.yuv422_en = 1;

		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 1080;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 88;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 4;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 36;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_720x576p_50Hz:	/* 480p */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsync_pol = 0;
		dpi.hsync_pol = 0;

		dpi.vsize = 576;
		dpi.hsize = 720;
		dpi.hpw = 64;
		dpi.hfp = 12;
		dpi.hbp = 68;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 5;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 39;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1280x720p_50Hz:	/* 720p */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 720;
		dpi.hsize = 1280;
		dpi.hpw = 40;
		dpi.hfp = 440;
		dpi.hbp = 220;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 5;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 20;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080i_50Hz:	/* 1080i60 */
	case HDMI_VIDEO_1920x1080i3d_sbs_50Hz:	/* 1080i60 */
		dpi.tdfp_en = 0;
		dpi.intl_en = 1;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 1;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 540;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 528;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 2;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 15;
		dpi.vpw_half_leven = 1;
		dpi.vpw_leven = 5;
		dpi.vfp_half_leven = 1;
		dpi.vfp_leven = 2;
		dpi.vbp_half_leven = 1;
		dpi.vbp_leven = 16;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:	/* 1080p60 */
		/* 422 */
		dpi.rgb2yuv_en = 1;
		dpi.yuv422_en = 1;

		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 1080;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 528;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 4;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 36;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080p_23Hz:	/* 1080p24 */
	case HDMI_VIDEO_1920x1080p_24Hz:	/* 1080p24 */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 1080;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 638;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 4;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 36;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080p_29Hz:	/* 1080p24 */
	case HDMI_VIDEO_1920x1080p_30Hz:	/* 1080p24 */
		dpi.tdfp_en = 0;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsize = 1080;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 88;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 4;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 36;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 0;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 0;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 0;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1280x720p3d_60Hz:	/* 720p 3d */
		/* 422 */
		dpi.rgb2yuv_en = 1;
		dpi.yuv422_en = 1;

		dpi.tdfp_en = 1;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsync_pol = 0;
		dpi.hsync_pol = 0;

		dpi.vsize = 720;
		dpi.hsize = 1280;
		dpi.hpw = 40;
		dpi.hfp = 110;
		dpi.hbp = 220;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 5;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 20;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 5;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 5;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 20;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1280x720p3d_50Hz:	/* 720p 3d */
		/* 422 */
		dpi.rgb2yuv_en = 1;
		dpi.yuv422_en = 1;

		dpi.tdfp_en = 1;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsync_pol = 0;
		dpi.hsync_pol = 0;

		dpi.vsize = 720;
		dpi.hsize = 1280;
		dpi.hpw = 40;
		dpi.hfp = 440;
		dpi.hbp = 220;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 5;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 20;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 5;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 5;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 20;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	case HDMI_VIDEO_1920x1080p3d_23Hz:	/* 1080p23 3d */
	case HDMI_VIDEO_1920x1080p3d_24Hz:	/* 1080p24 3d */
		/* 422 */
		dpi.rgb2yuv_en = 1;
		dpi.yuv422_en = 1;

		dpi.tdfp_en = 1;
		dpi.intl_en = 0;
		dpi.vs_reven_en = 0;
		dpi.vs_rodd_en = 0;
		dpi.vs_leven_en = 0;
		dpi.vs_lodd_en = 1;

		dpi.vsync_pol = 0;
		dpi.hsync_pol = 0;

		dpi.vsize = 1080;
		dpi.hsize = 1920;
		dpi.hpw = 44;
		dpi.hfp = 638;
		dpi.hbp = 148;
		dpi.vpw_half_lodd = 0;
		dpi.vpw_lodd = 5;
		dpi.vfp_half_lodd = 0;
		dpi.vfp_lodd = 4;
		dpi.vbp_half_lodd = 0;
		dpi.vbp_lodd = 36;
		dpi.vpw_half_leven = 0;
		dpi.vpw_leven = 0;
		dpi.vfp_half_leven = 0;
		dpi.vfp_leven = 0;
		dpi.vbp_half_leven = 0;
		dpi.vbp_leven = 0;
		dpi.vpw_half_rodd = 0;
		dpi.vpw_rodd = 5;
		dpi.vfp_half_rodd = 0;
		dpi.vfp_rodd = 4;
		dpi.vbp_half_rodd = 0;
		dpi.vbp_rodd = 36;
		dpi.vpw_half_reven = 0;
		dpi.vpw_reven = 0;
		dpi.vfp_half_reven = 0;
		dpi.vfp_reven = 0;
		dpi.vbp_half_reven = 0;
		dpi.vbp_reven = 0;
		break;
	default:
		printk("dpi can not support resolution\n");
		break;
	}

	/* mhl */
	dpi.fake_de_reven_en = 0;
	dpi.fake_de_rodd_en = 1;
	dpi.fake_de_leven_en = 0;
	dpi.fake_de_lodd_en = 0;

	dpi_en_addr = 0;

	dpi_con_addr = (DPI_CON & 0xFF00FF93)
	    | (dpi.fake_de_rodd_en << 22)
	    | (dpi.vs_reven_en << 19)
	    | (dpi.vs_rodd_en << 18)
	    | (dpi.vs_leven_en << 17)
	    | (dpi.vs_lodd_en << 16)
	    | (dpi.rgb2yuv_en << 6)
	    | (dpi.yuv422_en << 5)
	    | (dpi.tdfp_en << 3)
	    | (dpi.intl_en << 2);

	dpi_clkcon_addr = (1 << 25) | (0 << 5);
	dpi_output_setting_addr =
	    (dpi_output_setting_addr & ~((1 << 14) | (1 << 13))) | (dpi.vsync_pol << 14) | (dpi.
											    hsync_pol
											    << 13);

	dpi_size_addr = (dpi.vsize << 16) | (dpi.hsize << 0);
	dpi_hwidth_addr = (dpi.hpw);
	dpi_hporch_addr = (dpi.hfp << 16) | (dpi.hbp);
	dpi_vwidth_lodd_addr = (dpi.vpw_half_lodd << 16) | (dpi.vpw_lodd);
	dpi_vporch_lodd_addr = (dpi.vfp_lodd << 16) | (dpi.vbp_lodd);
	dpi_vwidth_leven_addr = (dpi.vpw_half_leven << 16) | (dpi.vpw_leven);
	dpi_vporch_leven_addr = (dpi.vfp_leven << 16) | (dpi.vbp_leven);
	dpi_vwidth_rodd_addr = (dpi.vpw_half_rodd << 16) | (dpi.vpw_rodd);
	dpi_vporch_rodd_addr = (dpi.vfp_rodd << 16) | (dpi.vbp_rodd);
	dpi_vwidth_reven_addr = (dpi.vpw_half_reven << 16) | (dpi.vpw_reven);
	dpi_vporch_reven_addr = (dpi.vfp_reven << 16) | (dpi.vbp_reven);

	if (u1MhlDpi1Pattern == 1)
		dpi_pattern_gen_addr = 0x10101051;
	else if (u1MhlDpi1Pattern == 2)
		dpi_pattern_gen_addr = 0xf0f0f051;
	else if (u1MhlDpi1Pattern == 3)
		dpi_pattern_gen_addr = 0xf0f0f051;
	else if (u1MhlDpi1Pattern == 4)
		dpi_pattern_gen_addr = 0xf0f0f001;
	else
		dpi_pattern_gen_addr = 0x21;

	dpi_rst_addr = 1;
	msleep(30);
	dpi_rst_addr = 0;
	dpi_en_addr = 1;
}
#endif
void vSetHDMITxDigital(unsigned char bResIndex)
{
	MHL_PLL_FUNC();

#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteByteHdmiGRL(GRL_ABIST_CTL1, 0x00);
	dpi_set_res(bResIndex);
#endif

	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		vMhlSetDigital(MHL_PLL_27);
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
		vMhlSetDigital(MHL_PLL_74175);
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		vMhlSetDigital(MHL_PLL_7425);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
		vMhlSetDigital(MHL_PLL_74175PP);
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		vMhlSetDigital(MHL_PLL_7425PP);
		break;
	default:
		printk("can not support resolution\n");
		break;
	}


}

void vResetHDMI(unsigned char bRst)
{
	MHL_DRV_FUNC();
	if (bRst) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, HDMI_RST, HDMI_RST);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, 0, HDMI_RST);
		vWriteHdmiGRLMsk(GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);	/* Designer suggest adjust Control packet deliver time */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, ANLG_ON, ANLG_ON);
	}
}

void vMhlDigitalPD(void)
{
	vWriteHdmiSYSMsk(DISP_CG_CON1, (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11),
			 (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11));
}

void vChangeVpll(unsigned char bRes)
{
	vResetHDMI(1);

	/* not hdmi2.0 */
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, HDMI2P0_EN);

#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG6, (1 << CFG4_CLK_DPI0_EN), CFG4_CLK_DPI0_EN_MASK);
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (1 << CFG5_CLK_HDMI_EN), CFG5_CLK_HDMI_EN_MASK);
#else
	vWriteHdmiCKANAMsk(ANA_CKGEN_CFG5, (1 << CFG5_CLK_HDMI_EN), CFG5_CLK_HDMI_EN_MASK);
#endif

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, HDMI_OUT_FIFO_EN);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_MODE_ON);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
	vWriteHdmiGRLMsk(GRL_CFG4, 0, CFG4_MHL_MODE);
#ifdef MHL_INTER_PATTERN_FOR_DBG
	vWriteHdmiSYSMsk(DISP_CG_CON1, (1 << 8) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15),
			 (1 << 8) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));
#else
	vWriteHdmiSYSMsk(DISP_CG_CON1, (1 << 8) | (1 << 12) | (1 << 13),
			 (1 << 8) | (1 << 12) | (1 << 13));
#endif
	udelay(100);

	vSetHDMITxPLL(bRes);	/* set PLL */

	vSetHDMITxDigital(bRes);

#ifdef MHL_INTER_PATTERN_FOR_DBG
	/* enable hdmi abist pattern */
	/* vWriteByteHdmiGRL(GRL_ABIST_CTL1,0x80); */
	/* disable hdmi abist pattern */
	vWriteByteHdmiGRL(GRL_ABIST_CTL1, 0x00);

	/* hdmi abist pattern */
	switch (bRes) {
	case HDMI_VIDEO_720x480p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((0 << 7) | (0 << 6) | (0x02)));	/* 480p */
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x03)));	/* 720p */
		break;
	case HDMI_VIDEO_1920x1080i_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x04)));	/* 1080i */
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0A)));	/* 1080p */
		break;
	case HDMI_VIDEO_720x576p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((0 << 7) | (0 << 6) | (0x0b)));	/* 576p */
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0c)));	/* 720p50 */
		break;
	case HDMI_VIDEO_1920x1080i_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0d)));	/* 1080i50 */
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x13)));	/* 1080p50 */
		break;
	default:
		printk("abist can not support resolution\n");
		break;
	}
#endif

}

void vHDMIAVUnMute(void)
{
	MHL_AUDIO_FUNC();

	if ((_bflagvideomute == FALSE) && (_bsvpvideomute == FALSE))
		vUnBlackHDMIOnly();
	if ((_bflagaudiomute == FALSE) && (_bsvpaudiomute == FALSE))
		UnMuteHDMIAudio();
}

void vEnableNotice(unsigned char bOn)
{
	unsigned char bData;
	MHL_VIDEO_FUNC();
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x40;	/* temp. solve 720p issue. to avoid audio packet jitter problem */
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x40;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}


void vEnableHdmiMode(unsigned char bOn)
{
	unsigned char bData;
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData &= ~CFG1_DVI;	/* enable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData |= CFG1_DVI;	/* disable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
	}

}

/* xubo */
void vEnableNCTSAutoWrite(void)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_DIVN);
	bData |= NCTS_WRI_ANYTIME;	/* enabel N-CTS can be written in any time */
	vWriteByteHdmiGRL(GRL_DIVN, bData);

}

void vMHLECOSetting(unsigned char ui1resindex)
{
	MHL_DRV_FUNC();

	vWriteHdmiGRLMsk(GRL_CFG2, 0, CFG2_MHL_DATA_REMAP);
	vWriteHdmiGRLMsk(GRL_CFG2, CFG2_MHL_FAKE_DE_SEL, CFG2_MHL_FAKE_DE_SEL);
	vWriteHdmiGRLMsk(GRL_CFG2, CFG2_MHL_DE_SEL, CFG2_MHL_DE_SEL);
	vWriteHdmiGRLMsk(GRL_MHL_ECO, GRL_MHL_VIDEO_ENC, GRL_MHL_VIDEO_ENC);
	vWriteHdmiGRLMsk(GRL_MHL_ECO, 0, GRL_MHL_PACKET_VIDEO_ENC);
	vWriteHdmiGRLMsk(GRL_MHL_ECO, GRL_VSYNC_DEL_SEL, GRL_VSYNC_DEL_SEL);

	switch (ui1resindex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		break;
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_60Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_50Hz:
		vWriteHdmiGRLMsk(GRL_CFG2, 0, CFG2_MHL_DE_SEL);
		break;
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		vWriteHdmiGRLMsk(GRL_CFG2, 0, CFG2_MHL_FAKE_DE_SEL);
		vWriteHdmiGRLMsk(GRL_CFG2, 0, CFG2_MHL_DE_SEL);
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
		vWriteHdmiGRLMsk(GRL_CFG2, CFG2_MHL_DATA_REMAP, CFG2_MHL_DATA_REMAP);
		vWriteHdmiGRLMsk(GRL_MHL_ECO, GRL_MHL_PACKET_VIDEO_ENC, GRL_MHL_PACKET_VIDEO_ENC);
		break;
	default:
		printk("can not support resolution\n");
		break;
	}
}

void vHDMIResetGenReg(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	MHL_DRV_FUNC();
	vResetHDMI(1);
	vResetHDMI(0);
	vEnableNotice(TRUE);

	vMHLECOSetting(ui1resindex);
	mhl_black_pattern_set(ui1colorspace);

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == TRUE)
		vEnableHdmiMode(TRUE);
	else
		vEnableHdmiMode(FALSE);

	vMhlFifoEnable(ui1resindex);

	vEnableNCTSAutoWrite();
}

void vAudioPacketOff(unsigned char bOn)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_SHIFT_R2);
	if (bOn)
		bData |= 0x40;
	else
		bData &= ~0x40;

	vWriteByteHdmiGRL(GRL_SHIFT_R2, bData);
}

void vSetChannelSwap(unsigned char u1SwapBit)
{
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CH_SWAP, u1SwapBit, 0xff);
}

void vEnableIecTxRaw(void)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData |= MIX_CTRL_FLAT;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
}

void vSetHdmiI2SDataFmt(unsigned char bFmt)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG0);
	bData &= ~0x33;
	switch (bFmt) {
	case RJT_24BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_24Bit);
		break;

	case RJT_16BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_16Bit);
		break;

	case LJT_24BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_24Bit);
		break;

	case LJT_16BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_16Bit);
		break;

	case I2S_24BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_24Bit);
		break;

	case I2S_16BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_16Bit);
		break;

	}


	vWriteByteHdmiGRL(GRL_CFG0, bData);
}

void vAOUT_BNUM_SEL(unsigned char bBitNum)
{
	MHL_AUDIO_FUNC();
	vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bBitNum);

}

void vSetHdmiHighBitrate(unsigned char fgHighBitRate)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	if (fgHighBitRate == TRUE) {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData |= HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);
		udelay(100);	/* 1ms */
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData &= ~HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);

		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}


}

void vDSTNormalDouble(unsigned char fgEnable)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vEnableDSTConfig(unsigned char fgEnable)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vDisableDsdConfig(void)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~SACD_SEL;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);

}

void vSetHdmiI2SChNum(unsigned char bChNum, unsigned char bChMapping)
{
	unsigned char bData, bData1, bData2, bData3;
	MHL_AUDIO_FUNC();
	if (bChNum == 2)	/* I2S 2ch */
	{
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 3) || (bChNum == 4))	/* I2S 2ch */
	{
		if ((bChNum == 4) && (bChMapping == 0x08)) {
			bData = 0x14;	/* 4ch data */

		} else {
			bData = 0x0c;	/* 4ch data */
		}
		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 6) || (bChNum == 5))	/* I2S 5.1ch */
	{
		if ((bChNum == 6) && (bChMapping == 0x0E)) {
			bData = 0x3C;	/* 6.0 ch data */
			bData1 = 0x50;	/* data0 */
		} else {
			bData = 0x1C;	/* 5.1ch data, 5/0ch */
			bData1 = 0x50;	/* data0 */
		}


	} else if (bChNum == 8)	/* I2S 5.1ch */
	{
		bData = 0x3C;	/* 7.1ch data */
		bData1 = 0x50;	/* data0 */
	} else if (bChNum == 7)	/* I2S 6.1ch */
	{
		bData = 0x3C;	/* 6.1ch data */
		bData1 = 0x50;	/* data0 */
	} else {
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */
	}

	bData2 = 0xc6;
	bData3 = 0xfa;

	vWriteByteHdmiGRL(GRL_CH_SW0, bData1);
	vWriteByteHdmiGRL(GRL_CH_SW1, bData2);
	vWriteByteHdmiGRL(GRL_CH_SW2, bData3);
	vWriteByteHdmiGRL(GRL_I2S_UV, bData);

	/* vDisableDsdConfig(); */

}

void vSetHdmiIecI2s(unsigned char bIn)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	if (bIn == SV_SPDIF) {

		bData = bReadByteHdmiGRL(GRL_CFG1);

		if ((bData & CFG1_SPDIF) == 0) {
			bData |= CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		if (bData & CFG1_SPDIF) {
			bData &= ~CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
		bData = bReadByteHdmiGRL(GRL_CFG1);
	}
}

void vUpdateAudSrcChType(unsigned char u1SrcChType)
{
	_stAvdAVInfo.ui2_aud_out_ch.word = 0;

	switch (u1SrcChType) {
	case AUD_INPUT_1_0:	/* TYPE_MONO */
	case AUD_INPUT_2_0:	// STEREO /* 2/0 */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 2;
		break;

	case AUD_INPUT_1_1:
	case AUD_INPUT_2_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 3;
		break;

	case AUD_INPUT_3_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 3;
		break;

	case AUD_INPUT_3_0_LRS:	/* L,R, RC -> L,R, RR, RL */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_3_1_LRS:	/* L, R, RC, LFE -> expand S to RR,RL to be  L, R, RR, RL, LFE */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_4_0_CLRS:	/* L, R, FC, RC -> expand S to RR,RL to be  L, R, FC, RR, RL */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_4_1_CLRS:	/* L, R, FC, RC, LFE -> expand S to RR,RL to be  L, R, FC, RR, RL, LFE */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_3_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_4_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_4_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_5_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_5_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_6_0:	/* 6/0 */
	case AUD_INPUT_6_0_Cs:
	case AUD_INPUT_6_0_Ch:
	case AUD_INPUT_6_0_Oh:
	case AUD_INPUT_6_0_Chr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_6_1:	/* 6/1 */
	case AUD_INPUT_6_1_Cs:	/* 6/1 */
	case AUD_INPUT_6_1_Ch:
	case AUD_INPUT_6_1_Oh:
	case AUD_INPUT_6_1_Chr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 7;
		break;

	case AUD_INPUT_7_0:	/* 5/2 */
	case AUD_INPUT_7_0_Lh_Rh:
	case AUD_INPUT_7_0_Lsr_Rsr:
	case AUD_INPUT_7_0_Lc_Rc:
	case AUD_INPUT_7_0_Lw_Rw:
	case AUD_INPUT_7_0_Lsd_Rsd:	/* Dolby new type. -- Water 20091102 */
	case AUD_INPUT_7_0_Lss_Rss:
	case AUD_INPUT_7_0_Lhs_Rhs:
	case AUD_INPUT_7_0_Cs_Ch:
	case AUD_INPUT_7_0_Cs_Oh:
	case AUD_INPUT_7_0_Cs_Chr:
	case AUD_INPUT_7_0_Ch_Oh:
	case AUD_INPUT_7_0_Ch_Chr:
	case AUD_INPUT_7_0_Oh_Chr:
	case AUD_INPUT_7_0_Lss_Rss_Lsr_Rsr:
	case AUD_INPUT_8_0_Lh_Rh_Cs:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RRC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RLC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 7;
		break;

	case AUD_INPUT_7_1:	/* 5/2 */
	case AUD_INPUT_7_1_Lh_Rh:
	case AUD_INPUT_7_1_Lsr_Rsr:
	case AUD_INPUT_7_1_Lc_Rc:
	case AUD_INPUT_7_1_Lw_Rw:
	case AUD_INPUT_7_1_Lsd_Rsd:	/* Dolby new type. -- Water 20091102 */
	case AUD_INPUT_7_1_Lss_Rss:
	case AUD_INPUT_7_1_Lhs_Rhs:
	case AUD_INPUT_7_1_Cs_Ch:
	case AUD_INPUT_7_1_Cs_Oh:
	case AUD_INPUT_7_1_Cs_Chr:
	case AUD_INPUT_7_1_Ch_Oh:
	case AUD_INPUT_7_1_Ch_Chr:
	case AUD_INPUT_7_1_Oh_Chr:
	case AUD_INPUT_7_1_Lss_Rss_Lsr_Rsr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RRC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RLC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 8;
		break;

	default:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 2;
		break;
	}

}

unsigned char bGetChannelMapping(void)
{
	unsigned char bChannelMap = 0x00;
	MHL_AUDIO_FUNC();

	vUpdateAudSrcChType(_stAvdAVInfo.u1Aud_Input_Chan_Cnt);

	switch (_stAvdAVInfo.ui1_aud_out_ch_number) {
	case 8:

		break;

	case 7:

		break;

	case 6:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RC == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 0)
		    ) {
			bChannelMap = 0x0E;	/* 6.0 */
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RC == 0) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)
		    ) {
			bChannelMap = 0x0B;	/* 5.1 */

		}
		break;

	case 5:
		break;

	case 4:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 0)) {
			bChannelMap = 0x08;
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)) {
			bChannelMap = 0x03;
		}
		break;

	case 3:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1)) {
			bChannelMap = 0x02;
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)
		    ) {
			bChannelMap = 0x01;
		}

		break;

	case 2:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1)) {
			bChannelMap = 0x00;
		}
		break;

	default:
		break;
	}


	return bChannelMap;
}

void vSetHDMIAudioIn(void)
{
	unsigned char bI2sFmt, bData2, bChMapping = 0;
	MHL_AUDIO_FUNC();

	vSetChannelSwap(LFE_CC_SWAP);
	vEnableIecTxRaw();

	bData2 = vCheckPcmBitSize(0);

	bI2sFmt = _stAvdAVInfo.e_I2sFmt;

	if ((_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) && (_stAvdAVInfo.e_aud_code == AVD_DST)) {
		vAOUT_BNUM_SEL(AOUT_24BIT);
	} else if (_stAvdAVInfo.e_I2sFmt == HDMI_LJT_24BIT) {
		if (!(bData2 == PCM_24BIT))
			bI2sFmt = HDMI_LJT_16BIT;
	}
	vSetHdmiI2SDataFmt(bI2sFmt);
	if (bData2 == PCM_24BIT)
		vAOUT_BNUM_SEL(AOUT_24BIT);
	else
		vAOUT_BNUM_SEL(AOUT_16BIT);

	vSetHdmiHighBitrate(FALSE);
	vDSTNormalDouble(FALSE);
	vEnableDSTConfig(FALSE);

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) {
		vDisableDsdConfig();

		if ((_stAvdAVInfo.e_aud_code == AVD_DST)) {
			vEnableDSTConfig(TRUE);
			vDSTNormalDouble(TRUE);
		}

		vSetHdmiI2SChNum(2, bChMapping);
		vSetHdmiIecI2s(SV_SPDIF);
	} else			/* I2S input */
	{
		bChMapping = bGetChannelMapping();

		vDisableDsdConfig();

		vSetHdmiI2SChNum(_stAvdAVInfo.ui1_aud_out_ch_number, bChMapping);
		vSetHdmiIecI2s(SV_I2S);
	}
}


void vHwNCTSOnOff(unsigned char bHwNctsOn)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CTS_CTRL);

	if (bHwNctsOn == TRUE)
		bData &= ~CTS_CTRL_SOFT;
	else
		bData |= CTS_CTRL_SOFT;

	vWriteByteHdmiGRL(GRL_CTS_CTRL, bData);

}

void vHalHDMI_NCTS(unsigned char bAudioFreq, unsigned char bPix)
{
	unsigned char bTemp, bData, bData1[NCTS_BYTES];
	unsigned int u4Temp, u4NTemp = 0;

	MHL_AUDIO_FUNC();
	MHL_AUDIO_LOG("bAudioFreq=%d,  bPix=%d\n", bAudioFreq, bPix);

	bData = 0;
	vWriteByteHdmiGRL(GRL_NCTS, bData);	/* YT suggest 3 dummy N-CTS */
	vWriteByteHdmiGRL(GRL_NCTS, bData);
	vWriteByteHdmiGRL(GRL_NCTS, bData);

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData1[bTemp] = 0;
	}

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {

		if ((bAudioFreq < 7) && (bPix < 9))

			bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
	}

	u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
	u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);	/* CTS */

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData = bData1[bTemp];
		vWriteByteHdmiGRL(GRL_NCTS, bData);
	}

	_u4NValue = u4NTemp;
}

void vHDMI_NCTS(unsigned char bHDMIFsFreq, unsigned char bResolution)
{
	unsigned char bPix;

	MHL_AUDIO_FUNC();

	vWriteHdmiGRLMsk(DUMMY_304, AUDIO_I2S_NCTS_SEL_64, AUDIO_I2S_NCTS_SEL);

	switch (bResolution) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
	default:
		bPix = 0;
		break;

	case HDMI_VIDEO_1280x720p_60Hz:	/* 74.175M pixel clock */
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
		bPix = 2;
		break;

	case HDMI_VIDEO_1280x720p_50Hz:	/* 74.25M pixel clock */
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080i3d_sbs_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
		bPix = 3;
		break;
	}

	vHalHDMI_NCTS(bHDMIFsFreq, bPix);

}

void vSetHdmiClkRefIec2(unsigned char fgSyncIec2Clock)
{
	MHL_AUDIO_FUNC();
}

void vSetHDMISRCOff(void)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData &= ~MIX_CTRL_SRC_EN;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	bData = 0x00;
	vWriteByteHdmiGRL(GRL_SHIFT_L1, bData);
}

void vHalSetHDMIFS(unsigned char bFs)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG5);
	bData &= CFG5_CD_RATIO_MASK;
	bData |= bFs;
	vWriteByteHdmiGRL(GRL_CFG5, bData);

}

void vHdmiAclkInv(unsigned char bInv)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	if (bInv == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}

void vSetHDMIFS(unsigned char bFs, unsigned char fgAclInv)
{
	MHL_AUDIO_FUNC();
	vHalSetHDMIFS(bFs);

	if (fgAclInv) {
		vHdmiAclkInv(TRUE);	/* //fix 192kHz, SPDIF downsample noise issue, ACL iNV */
	} else {
		vHdmiAclkInv(FALSE);	/* fix 192kHz, SPDIF downsample noise issue */
	}

}

void vReEnableSRC(void)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	if (bData & MIX_CTRL_SRC_EN) {
		bData &= ~MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
		udelay(255);
		bData |= MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	}

}

void vHDMIAudioSRC(unsigned char ui1hdmifs, unsigned char ui1resindex)
{

	MHL_AUDIO_FUNC();

	vHwNCTSOnOff(FALSE);

	vSetHdmiClkRefIec2(FALSE);

	switch (ui1hdmifs) {
	case HDMI_FS_32K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	case HDMI_FS_44K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	case HDMI_FS_48K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	case HDMI_FS_88K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;

	case HDMI_FS_96K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	case HDMI_FS_192K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	default:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;
	}

	vHDMI_NCTS(ui1hdmifs, ui1resindex);
	vReEnableSRC();

}

void vHwSet_Hdmi_I2S_C_Status(unsigned char *prLChData, unsigned char *prRChData)
{
	unsigned char bData;
	MHL_AUDIO_FUNC();
	bData = prLChData[0];

	vWriteByteHdmiGRL(GRL_I2S_C_STA0, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_0, bData);

	bData = prRChData[0];

	vWriteByteHdmiGRL(GRL_R_STATUS_0, bData);

	bData = prLChData[1];
	vWriteByteHdmiGRL(GRL_I2S_C_STA1, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_1, bData);
	bData = prRChData[1];
	vWriteByteHdmiGRL(GRL_R_STATUS_1, bData);

	bData = prLChData[2];
	vWriteByteHdmiGRL(GRL_I2S_C_STA2, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_2, bData);
	bData = prRChData[2];
	vWriteByteHdmiGRL(GRL_R_STATUS_2, bData);

	bData = prLChData[3];
	vWriteByteHdmiGRL(GRL_I2S_C_STA3, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_3, bData);
	bData = prRChData[3];
	vWriteByteHdmiGRL(GRL_R_STATUS_3, bData);

	bData = prLChData[4];
	vWriteByteHdmiGRL(GRL_I2S_C_STA4, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_4, bData);
	bData = prRChData[4];
	vWriteByteHdmiGRL(GRL_R_STATUS_4, bData);

	for (bData = 0; bData < 19; bData++) {
		vWriteByteHdmiGRL(GRL_L_STATUS_5 + bData * 4, 0);
		vWriteByteHdmiGRL(GRL_R_STATUS_5 + bData * 4, 0);

	}
}

void vHDMI_I2S_C_Status(void)
{
	unsigned char bData = 0;
	unsigned char bhdmi_RCh_status[5];
	unsigned char bhdmi_LCh_status[5];

	MHL_AUDIO_FUNC();

	bhdmi_LCh_status[0] = _stAvdAVInfo.bhdmiLChstatus[0];
	bhdmi_LCh_status[1] = _stAvdAVInfo.bhdmiLChstatus[1];
	bhdmi_LCh_status[2] = _stAvdAVInfo.bhdmiLChstatus[2];
	bhdmi_RCh_status[0] = _stAvdAVInfo.bhdmiRChstatus[0];
	bhdmi_RCh_status[1] = _stAvdAVInfo.bhdmiRChstatus[1];
	bhdmi_RCh_status[2] = _stAvdAVInfo.bhdmiRChstatus[2];


	bhdmi_LCh_status[0] &= ~0x02;
	bhdmi_RCh_status[0] &= ~0x02;

	bData = _stAvdAVInfo.bhdmiLChstatus[3] & 0xf0;

	switch (_stAvdAVInfo.e_hdmi_fs) {
	case HDMI_FS_32K:
		bData |= 0x03;
		break;

	case HDMI_FS_44K:
		break;

	case HDMI_FS_88K:
		bData |= 0x08;
		break;

	case HDMI_FS_96K:
		bData |= 0x0A;
		break;

	case HDMI_FS_48K:
	default:
		bData |= 0x02;
		break;
	}


	bhdmi_LCh_status[3] = bData;
	bhdmi_RCh_status[3] = bData;

	bData = _stAvdAVInfo.bhdmiLChstatus[4];

	bData |= ((~(bhdmi_LCh_status[3] & 0x0f)) << 4);

	bhdmi_LCh_status[4] = bData;
	bhdmi_RCh_status[4] = bData;

	vHwSet_Hdmi_I2S_C_Status(&bhdmi_LCh_status[0], &bhdmi_RCh_status[0]);

}

void vHalSendAudioInfoFrame(unsigned char bData1, unsigned char bData2, unsigned char bData4,
			    unsigned char bData5)
{
	unsigned char bAUDIO_CHSUM;
	unsigned char bData = 0;
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AUDIO_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AUDIO_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AUDIO_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AUDIO_LEN);

	bAUDIO_CHSUM = AUDIO_TYPE + AUDIO_VERS + AUDIO_LEN;

	bAUDIO_CHSUM += bData1;
	bAUDIO_CHSUM += bData2;
	bAUDIO_CHSUM += bData4;
	bAUDIO_CHSUM += bData5;

	bAUDIO_CHSUM = 0x100 - bAUDIO_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAUDIO_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);	/* bData2 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, 0);	/* bData3 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData = 0; bData < 5; bData++) {
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
	}
	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AUDIO_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSendAudioInfoFrame(void)
{
	MHL_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) {
		_bAudInfoFm[0] = 0x00;	/* CC as 0, */
		_bAudInfoFm[3] = 0x00;	/* CA 2ch */
	} else			/* pcm */
	{
		switch (_stAvdAVInfo.ui2_aud_out_ch.word & 0x7fb) {
		case 0x03:	/* FL/FR */
			_bAudInfoFm[0] = 0x01;
			_bAudInfoFm[3] = 0x00;
			break;

		case 0x0b:	/* FL/FR/FC */
			_bAudInfoFm[0] = 0x02;
			_bAudInfoFm[3] = 0x02;
			break;

		case 0x13:	/* FL/FR/RC */
			_bAudInfoFm[0] = 0x02;
			_bAudInfoFm[3] = 0x04;
			break;

		case 0x1b:	/* FL/FR/FC/RC */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x06;
			break;

		case 0x33:	/* FL/FR/RL/RR */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x08;
			break;

		case 0x3b:	/* FL/FR/FC/RL/RR */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x0A;
			break;

		case 0x73:	/* FL/FR/RL/RR/RC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x0C;
			break;

		case 0x7B:	/* FL/FR/FC/RL/RR/RC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x0E;
			break;

		case 0x633:	/* FL/FR/RL/RR/RLC/RRC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x10;
			break;

		case 0x63B:	/* FL/FR/FC/RL/RR/RLC/RRC */
			_bAudInfoFm[0] = 0x06;
			_bAudInfoFm[3] = 0x12;
			break;

		case 0x183:	/* FL/FR/FLC/FRC */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x14;
			break;

		case 0x18B:	/* FL/FR/FC/FLC/FRC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x16;
			break;

		case 0x1C3:	/* FL/FR/RC/FLC/FRC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x18;
			break;

		case 0x1CB:	/* FL/FR/FC/RC/FLC/FRC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x1A;
			break;

		default:
			_bAudInfoFm[0] = 0x01;
			_bAudInfoFm[3] = 0x00;
			break;
		}

		if (_stAvdAVInfo.ui2_aud_out_ch.word & 0x04)	/* LFE */
		{
			_bAudInfoFm[0]++;
			_bAudInfoFm[3]++;
		}
	}

	_bAudInfoFm[1] = 0;
	_bAudInfoFm[4] = 0x0;

	vHalSendAudioInfoFrame(_bAudInfoFm[0], _bAudInfoFm[1], _bAudInfoFm[3], _bAudInfoFm[4]);

}

void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex)
{
	unsigned int ui4Index;

	MHL_AUDIO_FUNC();
	vWriteHdmiSYSMsk(HDMI_SYS_CFG0, HDMI_SPDIF_ON, HDMI_SPDIF_ON);
	MuteHDMIAudio();
	vAudioPacketOff(TRUE);
	vSetHDMIAudioIn();
	vHDMIAudioSRC(ui1hdmifs, ui1resindex);

	vHDMI_I2S_C_Status();
	vSendAudioInfoFrame();

	for (ui4Index = 0; ui4Index < 5; ui4Index++) {
		udelay(5);
	}
	vHwNCTSOnOff(TRUE);
	UnMuteHDMIAudio();
	vAudioPacketOff(FALSE);
}

void vDisableGamut(void)
{
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, GAMUT_EN);
}

void vHalSendAVIInfoFrame(unsigned char *pr_bData)
{
	unsigned char bAVI_CHSUM;
	unsigned char bData1 = 0, bData2 = 0, bData3 = 0, bData4 = 0, bData5 = 0;
	unsigned char bData;
	MHL_VIDEO_FUNC();
	bData1 = *pr_bData;
	bData2 = *(pr_bData + 1);
	bData3 = *(pr_bData + 2);
	bData4 = *(pr_bData + 3);
	bData5 = *(pr_bData + 4);

	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AVI_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AVI_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AVI_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AVI_LEN);

	bAVI_CHSUM = AVI_TYPE + AVI_VERS + AVI_LEN;

	bAVI_CHSUM += bData1;
	bAVI_CHSUM += bData2;
	bAVI_CHSUM += bData3;
	bAVI_CHSUM += bData4;
	bAVI_CHSUM += bData5;
	bAVI_CHSUM = 0x100 - bAVI_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAVI_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData3);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData2 = 0; bData2 < 8; bData2++) {
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
	}
	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AVI_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);
}

void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	MHL_VIDEO_FUNC();

	if (ui1colorspace == HDMI_YCBCR_444) {
		_bAviInfoFm[0] = 0x40;
	} else if (ui1colorspace == HDMI_YCBCR_422) {
		_bAviInfoFm[0] = 0x20;
	} else {
		_bAviInfoFm[0] = 0x00;
	}

	_bAviInfoFm[0] |= 0x10;	/* A0=1, Active format (R0~R3) inf valid */

	_bAviInfoFm[1] = 0x0;	/* bData2 */

	if (ui1resindex == HDMI_VIDEO_1280x720p3d_60Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p3d_50Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_50Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p3d_24Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_24Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p3d_23Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_23Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_60Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_50Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_50Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_sbs_60Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_sbs_50Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_50Hz;

	if ((ui1resindex == HDMI_VIDEO_720x480p_60Hz) || (ui1resindex == HDMI_VIDEO_720x576p_50Hz)) {
		_bAviInfoFm[1] |= AV_INFO_SD_ITU601;
	} else {
		_bAviInfoFm[1] |= AV_INFO_HD_ITU709;
	}

	_bAviInfoFm[1] |= 0x20;
	_bAviInfoFm[1] |= 0x08;
	_bAviInfoFm[2] = 0;	/* bData3 */
	_bAviInfoFm[2] |= 0x0;
	_bAviInfoFm[3] = HDMI_VIDEO_ID_CODE[ui1resindex];	/* bData4 */

	if ((_bAviInfoFm[1] & AV_INFO_16_9_OUTPUT)
	    && ((ui1resindex == HDMI_VIDEO_720x480p_60Hz)
		|| (ui1resindex == HDMI_VIDEO_720x576p_50Hz))) {
		_bAviInfoFm[3] = _bAviInfoFm[3] + 1;
	}

	_bAviInfoFm[4] = 0x00;

	vHalSendAVIInfoFrame(&_bAviInfoFm[0]);

}

/************************************************************************
     Function :void vHalSendSPDInfoFrame(BYTE *pr_bData)
  Description : Setting SPD infoframe according to EIA/CEA-861-B
  Parameter   : None
  Return      : None
************************************************************************/
void vHalSendVendorSpecificInfoFrame(bool fg3DRes, unsigned char b3dstruct)
{
	unsigned char bVS_CHSUM;
	unsigned char bPB1, bPB2, bPB3, bPB4;
	/* bPB5 */
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, VS_EN);

	if (fg3DRes == FALSE)
		return;

	vWriteByteHdmiGRL(GRL_INFOFRM_VER, VS_VERS);	/* HB1 */

	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, VS_TYPE);	/* HB0 */

	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, VS_LEN);	/* HB2 */
	bPB1 = 0x1D;
	bPB2 = 0xA6;
	bPB3 = 0x7C;

	bPB4 = b3dstruct;

	bVS_CHSUM = VS_VERS + VS_TYPE + VS_LEN + bPB1 + bPB2 + bPB3 + bPB4;

	bVS_CHSUM = 0x100 - bVS_CHSUM;

	vWriteByteHdmiGRL(GRL_IFM_PORT, bVS_CHSUM);	/* check sum */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB1);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB2);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB3);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB4);	/* HDMI_Video_Format */

	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, VS_EN, VS_EN);
	printk(" vendor: bVS_CHSUM = %d, bPB1 = %d, bPB2 = %d, bPB3 = %d, bPB4 = %d\n", bVS_CHSUM,
	       bPB1, bPB2, bPB3, bPB4);
}

void vSendVendorSpecificInfoFrame(void)
{
	unsigned char b3DStruct;
	bool fg3DRes;

	b3DStruct = 0;

	if ((_stAvdAVInfo.e_resolution == HDMI_VIDEO_1280x720p3d_60Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1280x720p3d_50Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080p3d_24Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080p3d_23Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_60Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_50Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_sbs_60Hz)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_sbs_50Hz)) {
		fg3DRes = TRUE;
		if ((_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_sbs_60Hz)
		    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080i3d_sbs_50Hz))
			b3DStruct = 0x09;
		else
			b3DStruct = 0x01;
	} else
		fg3DRes = FALSE;

	if (fg3DRes == TRUE)
		vHalSendVendorSpecificInfoFrame(TRUE, b3DStruct);
	else
		vHalSendVendorSpecificInfoFrame(0, 0);
}

void vHalSendSPDInfoFrame(unsigned char *pr_bData)
{
	unsigned char bSPD_CHSUM, bData;
	unsigned char i = 0;

	MHL_VIDEO_FUNC();
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_SPD_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, SPD_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, SPD_TYPE);

	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, SPD_LEN);
	bSPD_CHSUM = SPD_TYPE + SPD_VERS + SPD_LEN;

	for (i = 0; i < SPD_LEN; i++) {
		bSPD_CHSUM += (*(pr_bData + i));
	}

	bSPD_CHSUM = 0x100 - bSPD_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bSPD_CHSUM);
	for (i = 0; i < SPD_LEN; i++)
		vWriteByteHdmiGRL(GRL_IFM_PORT, *(pr_bData + i));

	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_SPD_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSend_AVUNMUTE(void)
{
	unsigned char bData;
	MHL_VIDEO_FUNC();

	bData = bReadByteHdmiGRL(GRL_CFG4);
	bData |= CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData &= ~CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);
	udelay(30);

	bData &= ~CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData |= CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);

}

void vChgHDMIVideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
			     unsigned char ui1hdmifs)
{
	unsigned int u4Index;
	MHL_VIDEO_FUNC();

	vHDMIAVMute();
	vHDMIResetGenReg(ui1resindex, ui1colorspace);

	vChgHDMIAudioOutput(ui1hdmifs, ui1resindex);
	for (u4Index = 0; u4Index < 5; u4Index++) {
		udelay(10);
	}

	vDisableGamut();
	vSendAVIInfoFrame(ui1resindex, ui1colorspace);
	/* vHalSendSPDInfoFrame(&_bSpdInf[0]); */
	vSendVendorSpecificInfoFrame();
	vSend_AVUNMUTE();
	if (get_boot_mode() == FACTORY_BOOT)
		vHDMIAVUnMute();
}

void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft, unsigned char ui1hdmifs)
{
	MHL_AUDIO_FUNC();

	vHwNCTSOnOff(ui1audiosoft);	/* change to software NCTS; */
	vHDMI_NCTS(ui1hdmifs, ui1resindex);

}

void vShowHpdRsenStatus(void)
{
	if (bCheckPordHotPlug() == TRUE)
		printk("[HDMI]RSEN ON\n");
	else
		printk("[HDMI]RSEN OFF\n");

}

void vShowOutputVideoResolution(void)
{
	printk("[HDMI]HDMI output resolution = %s\n", szHdmiResStr[_stAvdAVInfo.e_resolution]);	/*  */

}

void vShowColorSpace(void)
{
	if (_stAvdAVInfo.e_video_color_space == HDMI_RGB)
		printk("[HDMI]HDMI output colorspace = HDMI_RGB\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_RGB_FULL)
		printk("[HDMI]HDMI output colorspace = HDMI_RGB_FULL\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_444)
		printk("[HDMI]HDMI output colorspace = HDMI_YCBCR_444\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_422)
		printk("[HDMI]HDMI output colorspace = HDMI_YCBCR_422\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_XV_YCC)
		printk("[HDMI]HDMI output colorspace = HDMI_XV_YCC\n");
	else
		printk("[HDMI]HDMI output colorspace error\n");

}

void vShowInforFrame(void)
{
	printk("====================Audio inforFrame Start ====================================\n");
	printk("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAudInfoFm[0], _bAudInfoFm[1],
	       _bAudInfoFm[2], _bAudInfoFm[3], _bAudInfoFm[4]);
	printk("CC2~ CC0: 0x%x, %s\n", _bAudInfoFm[0] & 0x07,
	       cAudChCountStr[_bAudInfoFm[0] & 0x07]);
	printk("CT3~ CT0: 0x%x, %s\n", (_bAudInfoFm[0] >> 4) & 0x0f,
	       cAudCodingTypeStr[(_bAudInfoFm[0] >> 4) & 0x0f]);
	printk("SS1, SS0: 0x%x, %s\n", _bAudInfoFm[1] & 0x03,
	       cAudSampleSizeStr[_bAudInfoFm[1] & 0x03]);
	printk("SF2~ SF0: 0x%x, %s\n", (_bAudInfoFm[1] >> 2) & 0x07,
	       cAudFsStr[(_bAudInfoFm[1] >> 2) & 0x07]);
	printk("CA7~ CA0: 0x%x, %s\n", _bAudInfoFm[3] & 0xff, cAudChMapStr[_bAudInfoFm[3] & 0xff]);
	printk("LSV3~LSV0: %d db\n", (_bAudInfoFm[4] >> 3) & 0x0f);
	printk("DM_INH: 0x%x ,%s\n", (_bAudInfoFm[4] >> 7) & 0x01,
	       cAudDMINHStr[(_bAudInfoFm[4] >> 7) & 0x01]);
	printk("====================Audio inforFrame End ======================================\n");

	printk("====================AVI inforFrame Start ====================================\n");
	printk("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAviInfoFm[0], _bAviInfoFm[1],
	       _bAviInfoFm[2], _bAviInfoFm[3], _bAviInfoFm[4]);
	printk("S1,S0: 0x%x, %s\n", _bAviInfoFm[0] & 0x03, cAviScanStr[_bAviInfoFm[0] & 0x03]);
	printk("B1,S0: 0x%x, %s\n", (_bAviInfoFm[0] >> 2) & 0x03,
	       cAviBarStr[(_bAviInfoFm[0] >> 2) & 0x03]);
	printk("A0: 0x%x, %s\n", (_bAviInfoFm[0] >> 4) & 0x01,
	       cAviActivePresentStr[(_bAviInfoFm[0] >> 4) & 0x01]);
	printk("Y1,Y0: 0x%x, %s\n", (_bAviInfoFm[0] >> 5) & 0x03,
	       cAviRgbYcbcrStr[(_bAviInfoFm[0] >> 5) & 0x03]);
	printk("R3~R0: 0x%x, %s\n", (_bAviInfoFm[1]) & 0x0f,
	       cAviActiveStr[(_bAviInfoFm[1]) & 0x0f]);
	printk("M1,M0: 0x%x, %s\n", (_bAviInfoFm[1] >> 4) & 0x03,
	       cAviAspectStr[(_bAviInfoFm[1] >> 4) & 0x03]);
	printk("C1,C0: 0x%x, %s\n", (_bAviInfoFm[1] >> 6) & 0x03,
	       cAviColorimetryStr[(_bAviInfoFm[1] >> 6) & 0x03]);
	printk("SC1,SC0: 0x%x, %s\n", (_bAviInfoFm[2]) & 0x03,
	       cAviScaleStr[(_bAviInfoFm[2]) & 0x03]);
	printk("Q1,Q0: 0x%x, %s\n", (_bAviInfoFm[2] >> 2) & 0x03,
	       cAviRGBRangeStr[(_bAviInfoFm[2] >> 2) & 0x03]);
	if (((_bAviInfoFm[2] >> 4) & 0x07) <= 1)
		printk("EC2~EC0: 0x%x, %s\n", (_bAviInfoFm[2] >> 4) & 0x07,
		       cAviExtColorimetryStr[(_bAviInfoFm[2] >> 4) & 0x07]);
	else
		printk("EC2~EC0: resevered\n");
	printk("ITC: 0x%x, %s\n", (_bAviInfoFm[2] >> 7) & 0x01,
	       cAviItContentStr[(_bAviInfoFm[2] >> 7) & 0x01]);
	printk("====================AVI inforFrame End ======================================\n");

	printk("====================SPD inforFrame Start ====================================\n");
	printk("Data Byte (1~8)  = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[0],
	       _bSpdInf[1], _bSpdInf[2], _bSpdInf[3], _bSpdInf[4], _bSpdInf[5], _bSpdInf[6],
	       _bSpdInf[7]);
	printk("Data Byte (9~16) = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[8],
	       _bSpdInf[9], _bSpdInf[10], _bSpdInf[11], _bSpdInf[12], _bSpdInf[13], _bSpdInf[14],
	       _bSpdInf[15]);
	printk("Data Byte (17~24)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[16],
	       _bSpdInf[17], _bSpdInf[18], _bSpdInf[19], _bSpdInf[20], _bSpdInf[21], _bSpdInf[22],
	       _bSpdInf[23]);
	printk("Data Byte  25    = 0x%x\n", _bSpdInf[24]);
	printk("Source Device information is %s\n", cSPDDeviceStr[_bSpdInf[24]]);
	printk("====================SPD inforFrame End ======================================\n");

	if (fgIsAcpEnable()) {
		printk
		    ("====================ACP inforFrame Start ====================================\n");
		printk("Acp type =0x%x\n", _bAcpType);

		if (_bAcpType == 0) {
			printk("Generic Audio\n");
			printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			       _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 1) {
			printk("IEC 60958-Identified Audio\n");
			printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			       _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 2) {
			printk("DVD Audio\n");
			printk("DVD-AUdio_TYPE_Dependent Generation = 0x%x\n", _bAcpData[0]);
			printk("Copy Permission = 0x%x\n", (_bAcpData[1] >> 6) & 0x03);
			printk("Copy Number = 0x%x\n", (_bAcpData[1] >> 3) & 0x07);
			printk("Quality = 0x%x\n", (_bAcpData[1] >> 1) & 0x03);
			printk("Transaction = 0x%x\n", _bAcpData[1] & 0x01);

		} else if (_bAcpType == 3) {
			printk("SuperAudio CD\n");
			printk("CCI_1 Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk
			    ("CCI_1 Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);

		}
		printk
		    ("====================ACP inforFrame End ======================================\n");
	}

	if (fgIsISRC1Enable()) {
		printk
		    ("====================ISRC1 inforFrame Start ====================================\n");
		printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		       _bIsrc1Data[0], _bIsrc1Data[1], _bIsrc1Data[2], _bIsrc1Data[3],
		       _bIsrc1Data[4], _bIsrc1Data[5], _bIsrc1Data[6], _bIsrc1Data[7]);
		printk
		    ("====================ISRC1 inforFrame End ======================================\n");
	}

	if (fgIsISRC2Enable()) {
		printk
		    ("====================ISRC2 inforFrame Start ====================================\n");
		printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		       _bIsrc1Data[8], _bIsrc1Data[9], _bIsrc1Data[10], _bIsrc1Data[11],
		       _bIsrc1Data[12], _bIsrc1Data[13], _bIsrc1Data[14], _bIsrc1Data[15]);
		printk
		    ("====================ISRC2 inforFrame End ======================================\n");
	}
}

unsigned int u4ReadNValue(void)
{
	return _u4NValue;
}

unsigned int u4ReadCtsValue(void)
{
	unsigned int u4Data;

	u4Data = bReadByteHdmiGRL(GRL_CTS0) & 0xff;
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS1) & 0xff) << 8);
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS2) & 0x0f) << 16);

	return u4Data;
}

void vShowHdmiAudioStatus(void)
{
	printk("[HDMI]HDMI output audio Channel Number =%d\n", _stAvdAVInfo.ui1_aud_out_ch_number);
	printk("[HDMI]HDMI output Audio Fs = %s\n", cHdmiAudFsStr[_stAvdAVInfo.e_hdmi_fs]);
	printk("[HDMI]HDMI MCLK =%d\n", _stAvdAVInfo.u1HdmiI2sMclk);
	printk("[HDMI]HDMI output ACR N= %d, CTS = %d\n", u4ReadNValue(), u4ReadCtsValue());
}

void vCheckHDMICRC(void)
{
	unsigned short u4Data = 0xffff, i;

	for (i = 0; i < 10; i++) {
		vWriteHdmiGRLMsk(CRC_CTRL, clr_crc_result, clr_crc_result | init_crc);
		vWriteHdmiGRLMsk(CRC_CTRL, init_crc, clr_crc_result | init_crc);

		mdelay(40);

		if (i == 0)
			u4Data =
			    (bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			    ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8);
		else {
			if ((u4Data !=
			     ((bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			      ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8))) || (u4Data == 0)) {
				printk("[HDMI]number = %d, u4Data = 0x%x\n", i, u4Data);
				printk("[HDMI]hdmi crc error\n");
				return;
			}
		}
	}
	printk("[HDMI]hdmi crc pass\n");
}

void vCheckHDMICLKPIN(void)
{
/*
  unsigned int u4Data, i;

  for(i=0; i<5; i++)
  {
  vWriteHdmiSYSMsk(HDMI_SYS_FMETER,TRI_CAL|CALSEL,TRI_CAL|CALSEL);
  vWriteHdmiSYSMsk(HDMI_SYS_FMETER,CLK_EXC,CLK_EXC);

  while(!(dReadHdmiSYS(HDMI_SYS_FMETER)&&CAL_OK));

  u4Data = ((dReadHdmiSYS(HDMI_SYS_FMETER)&(0xffff0000))>>16)*26000/1024;

  printk("[HDMI]hdmi pin clk = %d.%dM\n", (u4Data/1000), (u4Data-((u4Data/1000)*1000)));
  }
  */
}

void vShowHdmiDrmHdcpStatus(void)
{
	printk("[HDMI]DrmHdcp _bflagvideomute =%d\n", _bflagvideomute);
	printk("[HDMI]DrmHdcp _bflagaudiomute =%d\n", _bflagaudiomute);
	printk("[HDMI]DrmHdcp _bsvpvideomute =%d\n", _bsvpvideomute);
	printk("[HDMI]DrmHdcp _bsvpaudiomute =%d\n", _bsvpaudiomute);
	printk("[HDMI]DrmHdcp _bNeedSwHdcp =%d\n", _bHdcpOff);
	printk("[HDMI]DrmHdcp _bHdcpOff =%d\n", _bHdcpOff);
}

void mhl_status(void)
{
	vShowHpdRsenStatus();
	vShowOutputVideoResolution();
	vShowColorSpace();
	vShowInforFrame();
	vShowHdmiAudioStatus();
	vShowHdmiDrmHdcpStatus();
	vShowEdidRawData();
	vShowEdidInformation();
	vDcapParser();
	vShowHdcpRawData();
	vCheckHDMICRC();
	vCheckHDMICLKPIN();
}

/* ////////////////////////////////////////////////// */
/* / */
/* /             mhl infoframe debug */
/* / */
/* ////////////////////////////////////////////////// */
static unsigned int pdMpegInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000dE,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static unsigned int pdMpegInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d4,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000052,
	0x188, 0x00000003,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000004
};


static unsigned int pdSpdInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000d1,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static unsigned int pdSpdInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000cf,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000002
};



static unsigned int pdAudioInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x0000006a,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static unsigned int pdAudioInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x00000055,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000005,
	0x188, 0x00000006
};


static unsigned int pdVendorSpecInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000E2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static unsigned int pdVendorSpecInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006,
	0x188, 0x00000000
};


static unsigned int pdGenericInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000DC,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static unsigned int pdGenericInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000BA,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000010,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006
};


static unsigned int pdACPInfoReg[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static unsigned int pdACPInfoReg2[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000001,
	0x188, 0x00000001
};


static unsigned int pdISRC1InfoReg[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static unsigned int pdISRC1InfoReg2[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d0,	/* check sum, PB0 */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000003,
	0x188, 0x00000001
};


static unsigned int pdISRC2InfoReg[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static unsigned int pdISRC2InfoReg2[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d6,	/* check sum, PB0 */
	0x188, 0x00000042,
	0x188, 0x00000052,
	0x188, 0x00000002,
	0x188, 0x00000004
};


static unsigned int pdGamutInfoReg[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000000	/* PB0 */
};

static unsigned int pdGamutInfoReg2[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000001	/* PB0 */
};

void mhl_InfoframeSetting(unsigned char i1typemode, unsigned char i1typeselect)
{
	unsigned int u4Ind;
	unsigned char bData;

	if ((i1typemode == 0xff) && (i1typeselect == 0xff)) {
		printk("Arg1: Infoframe output type\n");
		printk("1: AVi, 2: Mpeg, 3: SPD\n");
		printk("4: Vendor, 5: Audio, 6: ACP\n");
		printk("7: ISRC1, 8: ISRC2,  9:GENERIC\n");
		printk("10:GAMUT\n");

		printk("Arg2: Infoframe data select\n");
		printk("0: old(default), 1: new;\n");
		return;
	}

	if (i1typeselect == 0) {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 1);
			mdelay(50);
			/* break; */

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg[u4Ind], pdMpegInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));
			mdelay(50);
			/* break; */

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg[u4Ind], pdSpdInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			/* break; */
			mdelay(50);

		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg[u4Ind],
						  pdVendorSpecInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			/* break; */
			mdelay(50);

		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg[u4Ind], pdAudioInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));
			mdelay(50);
			/* break; */

			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg[u4Ind], pdACPInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			/* break; */
			mdelay(50);
			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg[u4Ind], pdISRC1InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			/* break; */
			mdelay(50);
		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg[u4Ind], pdISRC2InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			/* break; */
			mdelay(50);
		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg[u4Ind],
						  pdGenericInfoReg[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			/* break; */
			mdelay(50);
		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg[u4Ind], pdGamutInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			/* vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz,  HDMI_XV_YCC); */

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;
			/* c : unmute */
		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;
			/* d mute */
		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}
	} else {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 2);

			break;

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg2[u4Ind], pdMpegInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));

			break;

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg2[u4Ind], pdSpdInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			break;


		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg2[u4Ind],
						  pdVendorSpecInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			break;


		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg2[u4Ind],
						  pdAudioInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));

			break;


			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg2[u4Ind], pdACPInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			break;

			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg2[u4Ind],
						  pdISRC1InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			break;

		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg2[u4Ind],
						  pdISRC2InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			break;

		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg2[u4Ind],
						  pdGenericInfoReg2[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			break;

		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg2[u4Ind],
						  pdGamutInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz, HDMI_XV_YCC);

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;

		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;

		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);

			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}

	}
}

void vMhlInit(void)
{

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_RISC_CLK_EN, MHL_RISC_CLK_EN);
	vMhlAnalogPD();
	vVideoPLLInit();
}

/* /////////////////////////////////////////////////////////////////////////// */
void dump_reg(void)
{
	printk(KERN_ERR "------start dump all reg for mhl------\n");
	/* clk enable */
	printk(KERN_ERR "0xf00000a0:%08x\n",
	       *(volatile unsigned int *)(HDMI_ANALOG_CKGEN_BASE + 0xa0));
	printk(KERN_ERR "0xf00000c0:%08x\n",
	       *(volatile unsigned int *)(HDMI_ANALOG_CKGEN_BASE + 0xc0));
	/* analog */
	printk(KERN_ERR "0xf0209270:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x270));
	printk(KERN_ERR "0xf0209274:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x274));
	printk(KERN_ERR "0xf0209278:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x278));
	printk(KERN_ERR "0xf020927c:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x27c));

	printk(KERN_ERR "0xf0209100:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x100));
	printk(KERN_ERR "0xf0209104:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x104));
	printk(KERN_ERR "0xf0209108:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x108));
	printk(KERN_ERR "0xf020910c:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x10c));
	printk(KERN_ERR "0xf0209110:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x110));
	printk(KERN_ERR "0xf0209114:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x114));
	printk(KERN_ERR "0xf0209118:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x118));
	printk(KERN_ERR "0xf020911c:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x11c));
	printk(KERN_ERR "0xf0209120:%08x\n", *(volatile unsigned int *)(HDMI_ANALOG_BASE + 0x120));
	/* sys config */
	printk(KERN_ERR "0xf4000110:%08x\n", *(volatile unsigned int *)(HDMI_SYS_BASE + 0x110));
	printk(KERN_ERR "0xf4000900:%08x\n", *(volatile unsigned int *)(HDMI_SYS_BASE + 0x900));
	printk(KERN_ERR "0xf4000904:%08x\n", *(volatile unsigned int *)(HDMI_SYS_BASE + 0x904));

	/* hdmitx */
	printk(KERN_ERR "0xf4025020:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x020));
	printk(KERN_ERR "0xf4025024:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x024));
	printk(KERN_ERR "0xf4025028:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x028));
	printk(KERN_ERR "0xf4025034:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x034));
	printk(KERN_ERR "0xf4025038:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x038));
	printk(KERN_ERR "0xf40250bc:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x0bc));
	printk(KERN_ERR "0xf4025174:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x174));
	printk(KERN_ERR "0xf402517c:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x17c));
	printk(KERN_ERR "0xf40251b0:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x1b0));
	printk(KERN_ERR "0xf40252d4:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x2d4));
	printk(KERN_ERR "0xf40252d8:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x2d8));
	printk(KERN_ERR "0xf4025390:%08x\n", *(volatile unsigned int *)(HDMI_GRL_BASE + 0x390));

	/* dpi */
	printk(KERN_ERR "0xf401d000:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x000));
	printk(KERN_ERR "0xf401d004:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x004));
	printk(KERN_ERR "0xf401d010:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x010));
	printk(KERN_ERR "0xf401d014:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x014));
	printk(KERN_ERR "0xf401d018:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x018));
	printk(KERN_ERR "0xf401d020:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x020));
	printk(KERN_ERR "0xf401d024:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x024));
	printk(KERN_ERR "0xf401d028:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x028));
	printk(KERN_ERR "0xf401d02c:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x02c));
	printk(KERN_ERR "0xf401d068:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x068));
	printk(KERN_ERR "0xf401d06c:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x06c));
	printk(KERN_ERR "0xf401d070:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x070));
	printk(KERN_ERR "0xf401d074:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x074));
	printk(KERN_ERR "0xf401d078:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x078));
	printk(KERN_ERR "0xf401d07c:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x07c));
	printk(KERN_ERR "0xf401d0b0:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0x0b0));
	printk(KERN_ERR "0xf401df00:%08x\n", *(volatile unsigned int *)(HDMI_DPI0_BASE + 0xf00));
	printk(KERN_ERR "------end dump all reg for mhl------\n");
}

/********************************************************************

    mhl debug system

********************************************************************/
extern void mhl_cmd_proc(unsigned int u4Cmd, unsigned int u4Para, unsigned int u4Para1,
			 unsigned int u4Para2);
void __iomem *mhl_reg_base_index;

/*
//0x14025000
#define HDMI_GRL_BASE                       hdmi_tx_reg_addr
//0x14000000
#define HDMI_SYS_BASE                       hdmi_mmsys_reg_addr
//0x10209000
#define HDMI_ANALOG_BASE                hdmi_analog_reg_addr
//0x10000000
#define HDMI_ANALOG_CKGEN_BASE  hdmi_ckgen_reg_addr
//0x10206000
#define HDMI_EFUSE_BASE                   hdmi_efuse_reg_addr
//0x1401d000
#define HDMI_DPI0_BASE                      hdmi_dpi0_reg_addr
*/

void mt_hdmi_debug_write(char *buf)
{
	unsigned int reg;
	unsigned int hdcp_state;
	unsigned int val;
	unsigned int vadr_regstart;
	unsigned int vadr_regend;
	int ret = 0;

	if (strncmp(buf, "dbgtype:", 8) == 0) {
		sscanf(buf + 8, "%x", &val);
		mhl_log_on = val;
		printk(KERN_ERR "mhl_log_on = 0x%08x\n", mhl_log_on);
	} else if (strncmp(buf, "res:", 4) == 0) {
		sscanf(buf + 4, "%x", &val);
		printk(KERN_ERR "res=%d\n", val);
		mhl_cmd_proc(11, val, 0, 0);
	} else if (strncmp(buf, "dump6", 5) == 0) {
		sscanf(buf + 4, "%x", &val);
		printk(KERN_ERR "res=%d\n", val);
		mhl_cmd_proc(12, 0, 0, 0);
	} else if (strncmp(buf, "w:", 2) == 0) {
		ret = sscanf(buf + 2, "%x=%x", &reg, &val);
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	if ((reg & 0xFFFFF000) == 0x25000) {
		vCaHDMIWriteReg(reg, val);
	} else
#endif
	{
	if ((reg & 0xfffff000) == 0x14025000) {
		mhl_reg_base_index = hdmi_tx_reg_addr;
	} else if ((reg & 0xfffff000) == 0x10209000) {
		mhl_reg_base_index = hdmi_analog_reg_addr;
	} else if ((reg & 0xfffff000) == 0x1401d000) {
		mhl_reg_base_index = hdmi_dpi0_reg_addr;
	} else if ((reg & 0xfffff000) == 0x14000000) {
		mhl_reg_base_index = hdmi_mmsys_reg_addr;
	} else if ((reg & 0xfffff000) == 0x10000000) {
		mhl_reg_base_index = hdmi_ckgen_reg_addr;
	} else if ((reg & 0xfffff000) == 0x10206000) {
		mhl_reg_base_index = hdmi_efuse_reg_addr;
	} else {
		pr_info("[mhl]hdmi_tx_reg_addr(0x14025000)\n");
		pr_info("[mhl]hdmi_analog_reg_addr(0x10209000)\n");
		pr_info("[mhl]hdmi_dpi0_reg_addr(0x1401d000)\n");
		pr_info("[mhl]hdmi_mmsys_reg_addr(0x14000000)\n");
		pr_info("[mhl]hdmi_ckgen_reg_addr(0x10000000)\n");
		pr_info("[mhl]hdmi_efuse_reg_addr(0x10206000)\n");
		return;
	}
		reg &= 0xfff;
		writel(val, mhl_reg_base_index + reg);
		pr_info("w:0x%08x=0x%08x\n", reg, val);
	}
	} else if (strncmp(buf, "r:", 2) == 0) {
		ret = sscanf(buf + 2, "%x/%x", &vadr_regstart, &vadr_regend);
	if ((vadr_regstart & 0xfffff000) == 0x14025000) {
		mhl_reg_base_index = hdmi_tx_reg_addr;
	} else if ((vadr_regstart & 0xfffff000) == 0x10209000) {
		mhl_reg_base_index = hdmi_analog_reg_addr;
	} else if ((vadr_regstart & 0xfffff000) == 0x1401d000) {
		mhl_reg_base_index = hdmi_dpi0_reg_addr;
	} else if ((vadr_regstart & 0xfffff000) == 0x14000000) {
		mhl_reg_base_index = hdmi_mmsys_reg_addr;
	} else if ((vadr_regstart & 0xfffff000) == 0x10000000) {
		mhl_reg_base_index = hdmi_ckgen_reg_addr;
	} else if ((vadr_regstart & 0xfffff000) == 0x10206000) {
		mhl_reg_base_index = hdmi_efuse_reg_addr;
	} else {
		pr_info("[mhl]hdmi_tx_reg_addr=0x14025000\n");
		pr_info("[mhl]hdmi_analog_reg_addr(0x10209000)\n");
		pr_info("[mhl]hdmi_dpi0_reg_addr(0x1401d000)\n");
		pr_info("[mhl]hdmi_mmsys_reg_addr(0x14000000)\n");
		pr_info("[mhl]hdmi_ckgen_reg_addr(0x10000000)\n");
		pr_info("[mhl]hdmi_efuse_reg_addr(0x10206000)\n");
		return;
	}
	vadr_regstart &= 0xfff;
	vadr_regend &= 0x3ff;
	pr_info("r:0x%08x/0x%08x\n", vadr_regstart, vadr_regend);
	vadr_regend = vadr_regstart + vadr_regend;
	while (vadr_regstart <= vadr_regend) {
		ret = readl(mhl_reg_base_index + vadr_regstart);
		pr_info("0x%08x = 0x%08x\n", vadr_regstart, ret);
		vadr_regstart += 4;
	}
	} else if (strncmp(buf, "w6:", 3) == 0) {
		ret = sscanf(buf + 3, "%x=%x", &reg, &val);
		reg &= 0xFFFF;
		printk(KERN_ERR "w6:0x%08x=0x%08x\n", reg, val);
		vWriteCbus(reg, val);
	} else if (strncmp(buf, "r6:", 3) == 0) {
		sscanf(buf + 3, "%x/%x", &vadr_regstart, &vadr_regend);
		vadr_regstart &= 0xFFFF;
		printk(KERN_ERR "r6:0x%08x/0x%08x\n", vadr_regstart, vadr_regend);
		vadr_regend = vadr_regstart + vadr_regend;
		while (vadr_regstart <= vadr_regend) {
			printk(KERN_ERR "0x%08x = 0x%08x\n", vadr_regstart,
			u4ReadCbus(vadr_regstart));
			vadr_regstart += 4;
		}
	} else if (strncmp(buf, "hdcp:", 5) == 0) {
		sscanf(buf + 5, "%x", &hdcp_state);
		_bHdcpOff = (unsigned char)hdcp_state;
		printk(KERN_ERR "current hdcp status = %c\n", _bHdcpOff);
	} else if (strncmp(buf, "status", 6) == 0) {
		vCbusLinkStatus();
		vCbusCmdStatus();
		mhl_status();
		dump_reg();
	} else if (strncmp(buf, "edid", 4) == 0) {
		vShowEdidRawData();
		vShowEdidInformation();
	} else if (strncmp(buf, "po", 2) == 0) {
		mhl_cmd_proc(9, 0, 0, 0);
	} else if (strncmp(buf, "pn", 2) == 0) {
		mhl_cmd_proc(10, 0, 0, 0);
	} else if (strncmp(buf, "force:on", 8) == 0) {
		mhl_cmd_proc(4, 0xa5, 0, 0);
	} else if (strncmp(buf, "force:off", 9) == 0) {
		mhl_cmd_proc(4, 0, 0, 0);
	} else if (strncmp(buf, "help", 4) == 0) {
		printk(KERN_ERR "---hdmi debug system help---\n");
		printk(KERN_ERR "please go in to sys/kernel/debug\n");
		printk(KERN_ERR "[debug type] echo dbgtype:VALUE>hdmi\n");
		printk(KERN_ERR "  hdmiplllog         0x1\n");
		printk(KERN_ERR "  hdmidgilog         0x2\n");
		printk(KERN_ERR "  hdmitxhotpluglog   0x4\n");
		printk(KERN_ERR "  hdmitxvideolog     0x8\n");
		printk(KERN_ERR "  hdmitxaudiolog     0x10\n");
		printk(KERN_ERR "  hdmihdcplog        0x20\n");
		printk(KERN_ERR "  hdmiceclog         0x40\n");
		printk(KERN_ERR "  hdmiddclog         0x80\n");
		printk(KERN_ERR "  hdmiedidlog        0x100\n");
		printk(KERN_ERR "  hdmidrvlog         0x200\n");
		printk(KERN_ERR "  hdmicbuslog        0x400\n");
		printk(KERN_ERR "  hdmicbustxrx       0x800\n");
		printk(KERN_ERR "  hdmicbusint        0x1000\n");
		printk(KERN_ERR "  hdmicbuserr        0x2000\n");
		printk(KERN_ERR "[read reg] echo r:ADDR/LEN>hdmi\n");
		printk(KERN_ERR "[read reg] echo w:ADDR=VALUE>hdmi\n");
		printk(KERN_ERR
		       "  if open tz and hdcp config, hdmi reg addr is 0x15000+offset, otherwise addr is real PA\n");
		printk(KERN_ERR "[read 6397 reg] echo r6:ADDR/LEN>hdmi\n");
		printk(KERN_ERR "[read 6397 reg] echo w6:ADDR=VALUE>hdmi\n");
		printk(KERN_ERR "[hdcp on/off] echo hdcp:VALUE>hdmi\n");
		printk(KERN_ERR "[edid] echo edid>hdmi\n");
		printk(KERN_ERR "[hdmi status] echo status>hdmi\n");
		printk(KERN_ERR "[power on] echo on>hdmi\n");
		printk(KERN_ERR "[power off] echo off>hdmi\n");
		printk(KERN_ERR "[suspend] echo suspend>hdmi\n");
		printk(KERN_ERR "[resume] echo resume>hdmi\n");
		printk(KERN_ERR "[hdmitx log on] echo log:on>hdmi\n");
		printk(KERN_ERR "[hdmitx log off] echo log:off>hdmi\n");
		printk(KERN_ERR "[help] echo help>hdmi\n");
	}
}

#endif
