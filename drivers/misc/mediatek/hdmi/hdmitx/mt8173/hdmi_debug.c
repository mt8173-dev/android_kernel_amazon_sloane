#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
/*******************************************************************************
Author - MTKLinux HDMI Device Driver
 *
 * Copyright 2014-2015 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provids the debug functions for HDMI
 *
 ******************************************************************************/

#include "hdmictrl.h"
#include "hdmi_ctrl.h"
#include "hdmihdcp.h"
#include "hdmiedid.h"
#include "hdmicec.h"
#include <mach/mt_boot_common.h>
#include "hdmitable.h"
#include "hdmi_debug.h"
#include "hdmi_ca.h"
extern void hdmi_force_plug_out(void);
extern void hdmi_force_plug_in(void);
extern unsigned char hdmi_plug_test_mode;
unsigned int hdmi_plug_out_delay;
unsigned int hdmi_plug_in_delay;



#define HDMI_ATTR_SPRINTF(fmt, arg...)  \
	do { \
		 temp_len = sprintf(buf, fmt, ##arg);	 \
		 buf += temp_len; \
		 len += temp_len; \
	} while (0)

typedef enum {
	dbgtype_cmd = 0,
	hdmiw_cmd,
	hdmir_cmd,
	cecw_cmd,
	cecr_cmd,
	enable_hdcp_cmd,
	hdmi_status_cmd,
	show_edid_cmd,
	testmodeon_cmd,
	testmodeoff_cmd,
	force_plugin_cmd,
	force_plugout_cmd,
	hdmidump_cmd,
	cecdump_cmd,

	cmd_nubmer
} hdmitx_debug_mode;

typedef struct _TEXT3ENUM_T {
	char *szText;
	unsigned int i4Cmd;
	char *szHelp;
} TEXT3ENUM_T;


static TEXT3ENUM_T _DebugModeEnumTbl[] = {
	{"dbgtype:", dbgtype_cmd, "[debug type] echo dbgtype:VALUE>hdmi"},
	{"hdmiw:", hdmiw_cmd, "[write reg] echo hdmiw:ADDR VALUE>hdmi;cat hdmi"},
	{"hdmir:", hdmir_cmd, "[read reg] echo hdmir:ADDR>hdmi;cat hdmi"},
	{"cecw:", cecw_cmd, "[write reg] echo cecw:ADDR VALUE>hdmi;cat hdmi"},
	{"cecr:", cecr_cmd, "[read reg] echo cecr:ADDR>hdmi;cat hdmi"},
	{"hdcp:", enable_hdcp_cmd, "[hdcp on/off] echo hdcp:VALUE>hdmi"},
	{"status", hdmi_status_cmd, "[hdmi status] echo status>hdmi;cat hdmi"},
	{"edid", show_edid_cmd, "[edid] echo edid>hdmi;cat hdmi"},
	{"testmode:on", testmodeon_cmd, "[testmode] echo testmode:on>hdmi;cat hdmi"},
	{"testmode:off", testmodeoff_cmd, "[testmode] echo testmode:off>hdmi;cat hdmi"},
	{"plugin", force_plugin_cmd, "[pluging] echo plugin>hdmi;cat hdmi"},
	{"plugout", force_plugout_cmd, "[plugout] echo plugout>hdmi;cat hdmi"},
	{"hdmidump", hdmidump_cmd, "[hdmidump] echo hdmidump>hdmi;cat hdmi"},
	{"cecdump", cecdump_cmd, "[cecdump] echo cecdump>hdmi;cat hdmi"},

	{NULL, -1, NULL}
};

static inline void hdmi_debug_write(HDMI_REG_ENUM reg, u32 reg_offset, u32 value)
{
	/* writel(value, hdmi_reg[reg]+reg_offset ); */
	*(volatile unsigned long *)(hdmi_reg[reg] + reg_offset) = value;

}

static inline u32 hdmi_debug_read(HDMI_REG_ENUM reg, u32 reg_offset)
{
	/* return readl(hdev->regs + reg_id); */
	return (*(volatile unsigned long *)(hdmi_reg[reg] + reg_offset));

}


static inline int hdmidrv_dump_regs(HDMI_REG_ENUM Module, char *pbuf)
{
	UINT32 i;
	char *buf;
	int temp_len = 0;
	int len = 0;
	buf = pbuf;

	if (Module == HDMI_SHELL) {
		HDMI_ATTR_SPRINTF("---------- Start dump DPI registers ----------\n");
		for (i = 0; i <= 0x390; i += 4) {
			HDMI_ATTR_SPRINTF("HDMI+%04x : 0x%08x\n", i,
					  hdmi_debug_read(hdmi_reg[Module], i));
		}
	} else if (Module == HDMI_CEC) {
		HDMI_ATTR_SPRINTF("---------- Start dump DPI registers ----------\n");
		for (i = 0; i <= 0xbc; i += 4) {
			HDMI_ATTR_SPRINTF("HDMI+%04x : 0x%08x\n", i,
					  hdmi_debug_read(hdmi_reg[Module], i));
		}
	}
	return 0;
}


static HDMI_STATE mt_hdmi_get_state(void)
{
	HDMI_DRV_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE) {
		return HDMI_STATE_ACTIVE;
	} else {
		return HDMI_STATE_NO_DEVICE;
	}
}

void mt_hdmi_show_edid_info(char *pbuf)
{
	unsigned int u4Res = 0;
	unsigned char bInx = 0;
	char *buf;
	int temp_len = 0;
	int len = 0;
	buf = pbuf;

	printk(">> mt_hdmi_show_edid_info\n");

	HDMI_ATTR_SPRINTF("[HDMI]EDID ver:%d/rev:%d\n", _HdmiSinkAvCap.ui1_Edid_Version,
			  _HdmiSinkAvCap.ui1_Edid_Revision);
	HDMI_ATTR_SPRINTF("[HDMI]EDID Extend Rev:%d \n", _HdmiSinkAvCap.ui1_ExtEdid_Revision);
	if (_HdmiSinkAvCap.b_sink_support_hdmi_mode) {
		HDMI_ATTR_SPRINTF("[HDMI]SINK Device is HDMI\n");
	} else {
		HDMI_ATTR_SPRINTF("[HDMI]SINK Device is DVI\n");
	}

	if (_HdmiSinkAvCap.b_sink_support_hdmi_mode)
		HDMI_ATTR_SPRINTF("[HDMI]CEC ADDRESS:%x \n", _HdmiSinkAvCap.ui2_sink_cec_address);

	HDMI_ATTR_SPRINTF("[HDMI]max clock limit : %d\n", _HdmiSinkAvCap.ui1_sink_max_tmds_clock);

	u4Res = (_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_cea_pal_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution);

	if (u4Res & SINK_480I)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080P 59.94hz\n");

	if (u4Res & SINK_576I)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT 1920x1080P 25hz\n");

	u4Res =
	    (_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution | _HdmiSinkAvCap.
	     ui4_sink_native_pal_resolution);
	HDMI_ATTR_SPRINTF("[HDMI]NTSC Native =%x\n",
			  _HdmiSinkAvCap.ui4_sink_native_ntsc_resolution);
	HDMI_ATTR_SPRINTF("[HDMI]PAL Native =%x\n", _HdmiSinkAvCap.ui4_sink_native_pal_resolution);
	if (u4Res & SINK_480I)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080P 59.94hz\n");
	if (u4Res & SINK_576I)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		HDMI_ATTR_SPRINTF("[HDMI]Native resolution is 1920x1080P 25hz\n");


	HDMI_ATTR_SPRINTF("[HDMI]SUPPORT RGB\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_YCBCR_444) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT YCBCR 444\n");
	}

	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_YCBCR_422) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT YCBCR 422\n");
	}

	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_XV_YCC709) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT xvYCC 709\n");
	}

	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_XV_YCC601) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT xvYCC 601\n");
	}

	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA0) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT metadata P0\n");
	}
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA1) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT metadata P1\n");
	}
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA2) {
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT metadata P2\n");
	}


	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT YCBCR 30 Bits Deep Color\n");

	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT YCBCR 36 Bits Deep Color\n");

	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT YCBCR 48 Bits Deep Color\n");

	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		HDMI_ATTR_SPRINTF("[HDMI]Not SUPPORT YCBCR Deep Color\n");

	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT RGB 30 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT RGB 36 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT RGB 48 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		HDMI_ATTR_SPRINTF("[HDMI]Not SUPPORT RGB Deep Color\n");

	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_LPCM)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT LPCM\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_AC3)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT AC3 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG1)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT MPEG1 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MP3)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT AC3 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG2)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT MPEG2 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_AAC)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT AAC Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DTS)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT DTS Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_ATRAC)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT ATRAC Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DSD)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT SACD DSD Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DOLBY_PLUS)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT Dolby Plus Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DTS_HD)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT DTS HD Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MAT_MLP)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT MAT MLP Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DST)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT SACD DST Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_WMA)
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT  WMA Decode\n");

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);


		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0] >> bInx) & 0x01) {
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
			}
		}
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT PCM Max 2CH, Fs is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] >> bInx) & 0x01) {
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
			}
		}
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT PCM Max 6CH Fs is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5] >> bInx) & 0x01) {
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
			}
		}
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT PCM Max 7CH Fs is: %s\n", &cDstStr[0]);
	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6] >> bInx) & 0x01) {
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
			}
		}
		HDMI_ATTR_SPRINTF("[HDMI]SUPPORT PCM Max 8CH, FS is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FL_FR)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker FL/FR allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_LFE)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker LFE allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FC)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker FC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RL_RR)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker RL/RR allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RC)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker RC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FLC_FRC)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker FLC/FRC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RLC_RRC)
		HDMI_ATTR_SPRINTF("[HDMI]Speaker RLC/RRC allocated\n");

	HDMI_ATTR_SPRINTF("[HDMI]HDMI edid support content type =%x\n",
			  _HdmiSinkAvCap.ui1_sink_content_cnc);
	HDMI_ATTR_SPRINTF("[HDMI]Lip Sync Progressive audio latency = %d\n",
			  _HdmiSinkAvCap.ui1_sink_p_audio_latency);
	HDMI_ATTR_SPRINTF("[HDMI]Lip Sync Progressive video latency = %d\n",
			  _HdmiSinkAvCap.ui1_sink_p_video_latency);
	if (_HdmiSinkAvCap.ui1_sink_i_latency_present) {
		HDMI_ATTR_SPRINTF("[HDMI]Lip Sync Interlace audio latency = %d\n",
				  _HdmiSinkAvCap.ui1_sink_i_audio_latency);
		HDMI_ATTR_SPRINTF("[HDMI]Lip Sync Interlace video latency = %d\n",
				  _HdmiSinkAvCap.ui1_sink_i_video_latency);
	}

	if (_HdmiSinkAvCap.ui1_sink_support_ai == 1)
		HDMI_ATTR_SPRINTF("[HDMI]Support AI\n");
	else
		HDMI_ATTR_SPRINTF("[HDMI]Not Support AI\n");

	HDMI_ATTR_SPRINTF("[HDMI]Monitor Max horizontal size = %d\n",
			  _HdmiSinkAvCap.ui1_Display_Horizontal_Size);
	HDMI_ATTR_SPRINTF("[HDMI]Monitor Max vertical size = %d\n",
			  _HdmiSinkAvCap.ui1_Display_Vertical_Size);


	if (_HdmiSinkAvCap.b_sink_hdmi_video_present == TRUE)
		HDMI_ATTR_SPRINTF("[HDMI]HDMI_Video_Present\n");
	else
		HDMI_ATTR_SPRINTF("[HDMI]No HDMI_Video_Present\n");

	if (_HdmiSinkAvCap.b_sink_3D_present == TRUE)
		HDMI_ATTR_SPRINTF("[HDMI]3D_present\n");
	else
		HDMI_ATTR_SPRINTF("[HDMI]No 3D_present\n");


	printk("<< mt_hdmi_show_edid_info\n");

}

void hdmistatuslog(char *pbuf)
{
	char *buf;
	int temp_len = 0;
	int len = 0;
	buf = pbuf;

	if (hdmi_powerenable)
		HDMI_ATTR_SPRINTF("[hdmi]hdmi power on\n");
	else
		HDMI_ATTR_SPRINTF("[hdmi]hdmi power off\n");

	HDMI_ATTR_SPRINTF("[hdmi]current connect state : %d\n ", mt_hdmi_get_state());
	HDMI_ATTR_SPRINTF("[hdmi]dbgtype : %lx\n", hdmidrv_log_on);
	if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)
		HDMI_ATTR_SPRINTF("[hdmi]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_ONLY\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		HDMI_ATTR_SPRINTF
		    ("[hdmi]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_AND_SINK_POWER_ON\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_OUT)
		HDMI_ATTR_SPRINTF("[hdmi]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_OUT\n");
	else
		HDMI_ATTR_SPRINTF("[hdmi]SI_HDMI_RECEIVER_STATUS error\n");
	if (_bHdcpOff)
		HDMI_ATTR_SPRINTF("[hdmi]hdcp off\n");
	else
		HDMI_ATTR_SPRINTF("[hdmi]hdcp on\n");
	HDMI_ATTR_SPRINTF("[hdmi]video resolution : %d\n", _stAvdAVInfo.e_resolution);
	HDMI_ATTR_SPRINTF("[hdmi]video color space : %d\n", _stAvdAVInfo.e_video_color_space);
	HDMI_ATTR_SPRINTF("[hdmi]video deep color : %d\n", _stAvdAVInfo.e_deep_color_bit);
	HDMI_ATTR_SPRINTF("[hdmi]audio fs : %d\n", _stAvdAVInfo.e_hdmi_fs);
	if (vIsDviMode())
		HDMI_ATTR_SPRINTF("[hdmi]dvi Mode\n");
	else
		HDMI_ATTR_SPRINTF("[hdmi]hdmi Mode\n");

	hdmi_hdmistatus();

}

static unsigned int icomparestr(const char *szText, const TEXT3ENUM_T *prText2Enum)
{
	if ((NULL == szText) || (NULL == prText2Enum)) {
		return 0;
	}

	while (prText2Enum->szText) {
		if ((strlen(prText2Enum->szText) == strlen(szText))
		    && (strncmp(prText2Enum->szText, szText, strlen(prText2Enum->szText)) == 0)) {
			break;
		} else {
			prText2Enum++;
		}
	}

	return prText2Enum->i4Cmd;
}

void mt_hdmi_show_info(char *pbuf)
{
	int var;
	u32 reg;
	unsigned int hdcp_state;
	unsigned int val, i4cmd;
	unsigned int reg_offset;
	TEXT3ENUM_T *prTextEnum;

	char *buf;
	int temp_len = 0;
	int len = 0;
	buf = pbuf;


	prTextEnum = _DebugModeEnumTbl;
	i4cmd = icomparestr(buf, prTextEnum);
	prTextEnum += i4cmd;

	switch (i4cmd) {
	case dbgtype_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x", &var);
		hdmidrv_log_on = var;
		printk("hdmidrv_log_on = 0x%lx\n", hdmidrv_log_on);

		break;

	case hdmiw_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x=%x", &reg, &val);
		printk("w:0x%lx=0x%x\n", hdmi_reg[HDMI_SHELL] + reg, val);
		/* *(volatile unsigned long*)(reg) = val; */
		hdmi_debug_write(HDMI_SHELL, reg, val);

		break;

	case hdmir_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x", &reg_offset);
		printk("r:0x%lx\n", reg_offset + hdmi_reg[HDMI_SHELL]);
		val = hdmi_debug_read(HDMI_CEC, reg_offset);
		HDMI_ATTR_SPRINTF("%08x\n", val);

		break;

	case cecw_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x=%x", &reg, &val);
		printk("w:0x%lx=0x%08x\n", hdmi_reg[HDMI_CEC] + reg, val);
		/* *(volatile unsigned long*)(reg) = val; */
		hdmi_debug_write(HDMI_CEC, reg, val);

		break;

	case cecr_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x", &reg_offset);
		printk("r:0x%lx\n", reg_offset + hdmi_reg[HDMI_CEC]);
		val = hdmi_debug_read(HDMI_CEC, reg_offset);
		HDMI_ATTR_SPRINTF("%08x\n", val);

		break;

	case enable_hdcp_cmd:
		sscanf(buf + strlen(prTextEnum->szText), "%x", &hdcp_state);
		_bHdcpOff = (unsigned char)hdcp_state;
		printk("current hdcp status = %c\n", _bHdcpOff);

		break;

	case hdmi_status_cmd:
		hdmistatuslog(pbuf);

		break;

	case show_edid_cmd:
		mt_hdmi_show_edid_info(pbuf);

		break;

	case testmodeon_cmd:
		HDMI_ATTR_SPRINTF("[hdmi] hdmi plug test mode\n");
		HDMI_ATTR_SPRINTF("[hdmi] please connect TV and Tablet by hdmi cable\n");
		hdmi_plug_test_mode = 0x1;
		hdmi_plug_out_delay = 1000;
		hdmi_plug_in_delay = 6000;

		break;

	case testmodeoff_cmd:
		HDMI_ATTR_SPRINTF("[hdmi] abist off for debug, please chang resolution again\n");
		hdmi_plug_test_mode = 0;

		break;

	case force_plugin_cmd:
		HDMI_ATTR_SPRINTF("[hdmi]hdmi test : plug in,,delay %dms\n", hdmi_plug_in_delay);
		if ((hdmi_powerenable == 0) || (hdmi_plug_test_mode != 0x1)) {
			HDMI_ATTR_SPRINTF("[hdmi]hdmi power off, return\n");
			return;
		}
		hdmi_force_plug_in();
		msleep(hdmi_plug_in_delay);

		break;

	case force_plugout_cmd:
		HDMI_ATTR_SPRINTF("[hdmi]hdmi test : plug out,delay %dms\n", hdmi_plug_out_delay);
		if ((hdmi_powerenable == 0) || (hdmi_plug_test_mode != 0xa5)) {
			HDMI_ATTR_SPRINTF("[hdmi]hdmi power off or test mode off, return\n");
			return;
		}
		hdmi_force_plug_out();
		msleep(hdmi_plug_out_delay);

		break;

	case hdmidump_cmd:
		hdmidrv_dump_regs(HDMI_SHELL, buf);

		break;

	case cecdump_cmd:
		hdmidrv_dump_regs(HDMI_CEC, buf);

		break;

	default:
		printk("---hdmi debug system help---\n");
		HDMI_ATTR_SPRINTF("---hdmi debug system help---\n");
		HDMI_ATTR_SPRINTF("please go in to sys/kernel/debug/hdmi\n");
		HDMI_ATTR_SPRINTF
		    ("==============================================================\n");
		while (prTextEnum->szText) {
			HDMI_ATTR_SPRINTF("Example:echo %s>sys/kernel/debug/hdmi\n",
					  prTextEnum->szHelp);
			prTextEnum++;
		}
		HDMI_ATTR_SPRINTF
		    ("==============================================================\n");

		break;

	}


}
#endif
