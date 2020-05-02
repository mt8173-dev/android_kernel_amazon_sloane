/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 HI704_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hi704raw_Sensor.h"


/****************************Modify Following Strings for Debug****************************/
#define PFX "[HI704raw]"
#define LOG_1 LOG_INF("HI704,PARALLEL\n")
#define LOG_2 LOG_INF("preview parallel 640*480@30fps; video 640*480@30fps; capture parallel 640*480@30fps\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define hi704raw_valid_framelength 500
#define hi704raw_valid_linelength 656

static kal_uint32 video_nightmode;
kal_uint32 iExp_video_night = 0;
static kal_uint16 set_video_mode_flag;


static imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI704RAW_SENSOR_ID,

	.checksum_value = 0x65a7ee32,

	.pre = {
		.pclk = 12000000,
		.linelength = (hi704raw_valid_linelength + 0x70),	/* 768  */
		.framelength = (hi704raw_valid_framelength + 0x15),	/* 521 */
		.startx = 2,	/* startx of grabwindow */
		.starty = 3,	/* starty of grabwindow */
		.grabwindow_width = (640 - 0),	/* width of grabwindow */
		.grabwindow_height = (480 - 0),	/* height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 14,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 12000000,	/*  pclk */
		.linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		.framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
		.startx = 2,	/*  startx of grabwindow */
		.starty = 3,	/*  starty of grabwindow */
		.grabwindow_width = (640 - 0),	/*  width of grabwindow */
		.grabwindow_height = (480 - 0),	/*  height of grabwindow */

		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
		},
	.cap1 = {
		 .pclk = 12000000,	/*  pclk */
		 .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		 .framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
		 .startx = 2,	/*  startx of grabwindow */
		 .starty = 3,	/*  starty of grabwindow */
		 .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		 .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		 .mipi_data_lp2hs_settle_dc = 14,
		 .max_framerate = 300,
		 },
	.normal_video = {
			 .pclk = 12000000,	/*  pclk */
			 .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
			 .framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
			 .startx = 2,	/*  startx of grabwindow */
			 .starty = 3,	/*  starty of grabwindow */
			 .grabwindow_width = (640 - 0),	/*  width of grabwindow */
			 .grabwindow_height = (480 - 0),	/*  height of grabwindow */

			 .mipi_data_lp2hs_settle_dc = 14,
			 .max_framerate = 300,
			 },
	.hs_video = {
		     .pclk = 12000000,	/*  pclk */
		     .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		     .framelength = (hi704raw_valid_framelength + 0x15),	/* 521, framelength */
		     .startx = 2,	/*  startx of grabwindow */
		     .starty = 3,	/*  starty of grabwindow */
		     .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		     .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		     .mipi_data_lp2hs_settle_dc = 14,
		     .max_framerate = 300,
		     },
	.slim_video = {
		       .pclk = 12000000,	/*  pclk */
		       .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		       .framelength = (hi704raw_valid_framelength + 0x15),	/* 521  , framelength */
		       .startx = 2,	/*  startx of grabwindow */
		       .starty = 3,	/*  starty of grabwindow */
		       .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		       .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		       .mipi_data_lp2hs_settle_dc = 14,
		       .max_framerate = 300,
		       },
	.custom1 = {
		    .pclk = 12000000,	/*  pclk */
		    .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		    .framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
		    .startx = 2,	/*  startx of grabwindow */
		    .starty = 3,	/*  starty of grabwindow */
		    .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		    .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		    .mipi_data_lp2hs_settle_dc = 14,
		    .max_framerate = 300,

		    },
	.custom2 = {
		    .pclk = 12000000,	/*  pclk */
		    .linelength = (hi704raw_valid_linelength + 0x70),	/* 768 , linelength */
		    .framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
		    .startx = 2,	/*  startx of grabwindow */
		    .starty = 3,	/*  starty of grabwindow */
		    .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		    .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		    .mipi_data_lp2hs_settle_dc = 14,
		    .max_framerate = 300,

		    },
	.custom3 = {
		    .pclk = 12000000,	/*  pclk */
		    .linelength = (hi704raw_valid_linelength + 0x70),	/* 768, linelength */
		    .framelength = (hi704raw_valid_framelength + 0x15),	/* 521 , framelength */
		    .startx = 2,	/*  startx of grabwindow */
		    .starty = 3,	/*  starty of grabwindow */
		    .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		    .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		    .mipi_data_lp2hs_settle_dc = 14,
		    .max_framerate = 300,

		    },
	.custom4 = {
		    .pclk = 12000000,	/*  pclk */
		    .linelength = (hi704raw_valid_linelength + 0x70),	/* 768, linelength */
		    .framelength = (hi704raw_valid_framelength + 0x15),	/* 521, framelength */
		    .startx = 2,	/*  startx of grabwindow */
		    .starty = 3,	/*  starty of grabwindow */
		    .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		    .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		    .mipi_data_lp2hs_settle_dc = 14,
		    .max_framerate = 300,

		    },
	.custom5 = {
		    .pclk = 12000000,	/*  pclk */
		    .linelength = (hi704raw_valid_linelength + 0x70),	/* 768, linelength */
		    .framelength = (hi704raw_valid_framelength + 0x15),	/* 521, framelength */
		    .startx = 2,	/*  startx of grabwindow */
		    .starty = 3,	/*  starty of grabwindow */
		    .grabwindow_width = (640 - 0),	/*  width of grabwindow */
		    .grabwindow_height = (480 - 0),	/*  height of grabwindow */

		    .mipi_data_lp2hs_settle_dc = 14,
		    .max_framerate = 300,

		    },


	/* .margin = 4, */
	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 1,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */
	.hs_video_delay_frame = 3,	/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_2MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_PARALLEL,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW8_Gr,
	.mclk = 24,		/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_2_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x60, 0x20, 0xff},
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x60,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* Preview */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* capture */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* video */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* hight speed video */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 1600, 480},/* slim video */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},/* Custom1(defaultuse preview)*/
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* Custom2 */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* Custom3 */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},	/* Custom4 */
	{640, 480, 0, 0, 640, 480, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480}
};				/* Custom5 */

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 1, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	LOG_INF("get_byte 0x%x\n", get_byte);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);

	return;
}

static void set_dummy(void)
{
	kal_uint32 iExp = 4 * 96;
	kal_uint32 OPCLK_Oneline = imgsensor.line_length / 8;	/* 768/8=96 */

	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	iExp = (imgsensor.frame_length + 1) * OPCLK_Oneline + 24 / 3;

	if (video_nightmode == 1) {
		iExp = (imgsensor.frame_length + 1) * OPCLK_Oneline + 24 / 3;
		if (iExp_video_night != iExp) {
			write_cmos_sensor(0x3, 0);		/* For No Fixed Framerate Bit[2] */
			write_cmos_sensor(0x11, 0x94);	/* For No Fixed Framerate Bit[2] */
			write_cmos_sensor(0x03, 0x20);
			/* write_cmos_sensor(0x83, (iExp >> 16) & 0xFF); */
			/* write_cmos_sensor(0x84, (iExp >>  8) & 0xFF); */
			/* write_cmos_sensor(0x85, (iExp >>  0) & 0xFF); */


			write_cmos_sensor(0x91, (iExp >> 16) & 0xFF);
			write_cmos_sensor(0x92, (iExp >> 8) & 0xFF);
			write_cmos_sensor(0x93, (iExp >> 0) & 0xFF);
			iExp_video_night = iExp;
			LOG_INF("video night imgsensor.frame_length=%d, iExp = 0x%x, OPCLK_Oneline = %d\n",
					imgsensor.frame_length, iExp, OPCLK_Oneline);
		}
	}


	LOG_INF("iExp = 0x%x, OPCLK_Oneline = %d\n", iExp, OPCLK_Oneline);

	/*if(imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO)
	   {
	   write_cmos_sensor(0x03, 0);
	   write_cmos_sensor(0x40, (imgsensor.line_length-hi704raw_valid_linelength) >> 8);
	   write_cmos_sensor(0x41, (imgsensor.line_length-hi704raw_valid_linelength) & 0xFF);
	   write_cmos_sensor(0x42, (imgsensor.frame_length-hi704raw_valid_framelength) >> 8);
	   write_cmos_sensor(0x43, (imgsensor.frame_length-hi704raw_valid_framelength) & 0xFF);
	   } */

}				/*      set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength (%d) should enable?\n", framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*    set_max_framerate  */



static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	/*kal_uint32 frame_length = 0; */
	/*kal_uint32 real_shutter = 0; */
	kal_uint32 iExp = shutter;

	kal_uint32 OPCLK_Oneline = imgsensor.line_length / 8;	/* 768/8=96 */


	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length, should extend frame length first */

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
		imgsensor.frame_length = shutter + imgsensor_info.margin;
		shutter -= 0;
	} else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;
#if 1
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* write_cmos_sensor(0x03, 0); */
			/* write_cmos_sensor(0x42, (imgsensor.frame_length-hi704raw_valid_framelength) >> 8); */
			/* write_cmos_sensor(0x43, (imgsensor.frame_length-hi704raw_valid_framelength) & 0xFF); */
			if (video_nightmode == 1) {
				iExp = (imgsensor.frame_length + 1) * OPCLK_Oneline + 24 / 3;

				if (iExp_video_night != iExp) {
					write_cmos_sensor(0x3, 0);	/* For No Fixed Framerate Bit[2] */
					write_cmos_sensor(0x11, 0x94);/* For No Fixed Framerate Bit[2] */

					write_cmos_sensor(0x03, 0x20);

					write_cmos_sensor(0x91, (iExp >> 16) & 0xFF);
					write_cmos_sensor(0x92, (iExp >> 8) & 0xFF);
					write_cmos_sensor(0x93, (iExp >> 0) & 0xFF);
					LOG_INF
					    ("video night shutter =%d, framelength =%d,iExp =%x\n",
					     shutter, imgsensor.frame_length, iExp);
					iExp_video_night = iExp;
				}

			}

		}
	} else {
		/* Extend frame length */
/* write_cmos_sensor(0x03, 0); */
/* write_cmos_sensor(0x42, (imgsensor.frame_length-hi704raw_valid_framelength) >> 8); */
/* write_cmos_sensor(0x43, (imgsensor.frame_length-hi704raw_valid_framelength) & 0xFF); */
		if (video_nightmode == 1) {
			iExp = (imgsensor.frame_length + 1) * OPCLK_Oneline + 24 / 3;

			if (iExp_video_night != iExp) {
				write_cmos_sensor(0x3, 0);	/* For No Fixed Framerate Bit[2] */
				write_cmos_sensor(0x11, 0x94);/* For No Fixed Framerate Bit[2] */

				write_cmos_sensor(0x03, 0x20);

				write_cmos_sensor(0x91, (iExp >> 16) & 0xFF);
				write_cmos_sensor(0x92, (iExp >> 8) & 0xFF);
				write_cmos_sensor(0x93, (iExp >> 0) & 0xFF);

				LOG_INF("video night shutter =%d, framelength =%d,iExp =%x\n",
					shutter, imgsensor.frame_length, iExp);
				iExp_video_night = iExp;
			}
		}
	}
#endif

	/* Update Shutter */
	iExp = ((shutter) * OPCLK_Oneline);
	write_cmos_sensor(0x03, 0x20);
	write_cmos_sensor(0x83, (iExp >> 16) & 0xFF);
	write_cmos_sensor(0x84, (iExp >> 8) & 0xFF);
	write_cmos_sensor(0x85, (iExp >> 0) & 0xFF);

	/* write_cmos_sensor(0x83, (0 >> 16) & 0xFF); */
	/* write_cmos_sensor(0x84, (0xc000 >>  8) & 0xFF); */
	/* write_cmos_sensor(0x85, (0 >>  0) & 0xFF); */
	LOG_INF("shutter =%d, framelength =%d,iExp =%x\n", shutter, imgsensor.frame_length, iExp);

	/* LOG_INF("frame_length = %d ", frame_length); */

}				/*      write_shutter  */



/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	/* write_cmos_sensor(0x0104, 0x01); */
	write_shutter(shutter);
	/* write_cmos_sensor(0x0104, 0x00); */
}				/*      set_shutter */


static kal_uint8 gain2reg(const kal_uint16 iGain)
{
	/*******************gain=2^Reg_Cgain +Reg_Dgain/64*****************************/
	/* LOG_INF("[HI704]mycat enter HI704Reg2Gain function iGain=%d\n",iGain); */
	kal_uint16 gain_tmp0 = 0;
	LOG_INF("[S], iGain = %d", iGain);
	/* AG = 0.5 +B[7:0]/32 B[7:0]=( (64*AG)-32 )2 */
	gain_tmp0 = (iGain - 32) / 2;
	write_cmos_sensor(0x03, 0x20);
	write_cmos_sensor(0xb0, gain_tmp0);

	LOG_INF("[E], digital gain_tmp0 = 0x%x", gain_tmp0);
	return (kal_uint16) gain_tmp0;

}


/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X      */
	/* [4:9] = M meams M X           */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}
	/* write_cmos_sensor(0x0104, 0x01); */

	reg_gain = gain2reg(gain);
	/* reg_gain = gain2reg_test(gain); */

	/* write_cmos_sensor(0x0104, 0x00); */
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	/* write_cmos_sensor(0x0205, reg_gain >> 8); */

	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length = imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;


		/* Extend frame length */
		/* write_cmos_sensor(0x380e, imgsensor.frame_length >> 8); */
		/* write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF); */

		/* write_cmos_sensor(0x3502, (le << 4) & 0xFF); */
		/* write_cmos_sensor(0x3501, (le >> 4) & 0xFF); */
		/* write_cmos_sensor(0x3500, (le >> 12) & 0x0F); */

		/* write_cmos_sensor(0x3508, (se << 4) & 0xFF); */
		/* write_cmos_sensor(0x3507, (se >> 4) & 0xFF); */
		/* write_cmos_sensor(0x3506, (se >> 12) & 0x0F); */

		/* set_gain(gain); */
	}

}


#if 0	/* Temp mark for build warning: [-Wunused-function] */
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	   *
	   *   0x3820[2] ISP Vertical flip
	   *   0x3820[1] Sensor Vertical flip
	   *
	   *   0x3821[2] ISP Horizontal mirror
	   *   0x3821[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
		/* write_cmos_sensor(0x0101,0x00); */
		break;
	case IMAGE_H_MIRROR:
		/* write_cmos_sensor(0x0101,0x02); */
		break;
	case IMAGE_V_MIRROR:
		/* write_cmos_sensor(0x0101,0x01); */
		break;
	case IMAGE_HV_MIRROR:
		/* write_cmos_sensor(0x0101,0x03); */
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}

}
#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}				/*      night_mode      */

static void sensor_init(void)
{
	LOG_INF("E\n");


	/* 0502 */
	/* saturation a0 a0-->c0 c0 */
	/* 140423 */
	/* adjust CMC,Saturation, 2285->56, 1514 ->40, 1516->35 */
	/* 2211 ->2e(AD CMC) */
	/* 130422 */
	/* shading & gamma */
	/* GMax 70->c0 */


	/* [SENSOR_INITIALIZATION] */
	/* DISP_DATE = "2014-04-09 10:14:45" */
	/* DISP_WIDTH = 640 */
	/* DISP_HEIGHT = 480 */
	/* DISP_FORMAT = BAYER10 //YUV422 */
	/* DISP_DATAORDER = YUYV */
	/* MCLK = 24.00 */
	/* PLL = 1.00 */
	/* BEGIN */
	/* ;PAGE0 */
	/* ;@60hz   write_cmos_sensor */
	write_cmos_sensor(0x03, 0x00);	/* PAGEMODE(0x03) */
	write_cmos_sensor(0x01, 0xf1);	/*  , */
	write_cmos_sensor(0x01, 0xf3);	 /* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	write_cmos_sensor(0x01, 0xf1);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x11, 0x90);	 /* For No Fixed Framerate Bit[2] */

	/*write_cmos_sensor(0x11, 0x94)*/	/* For No Fixed Framerate Bit[2] */

	write_cmos_sensor(0x12, 0x04);	   /* PCLK INV */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x20, 0x00);	/*  , */
	write_cmos_sensor(0x21, 0x04);	/* Window start X */
	write_cmos_sensor(0x22, 0x00);	/*  , */
	write_cmos_sensor(0x23, 0x04);	/* Window start Y */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x24, 0x01);	/*  , */
	write_cmos_sensor(0x25, 0xe8);	/* Window height : 0x1e8 = 488 */
	write_cmos_sensor(0x26, 0x02);	/*  , */
	write_cmos_sensor(0x27, 0x84);	 /* Window width  : 0x284 = 644 */
	/* write_cmos_sensor(          ) // */
	/* write_cmos_sensor(          ) // */
	write_cmos_sensor(0x40, 0x00);	 /* HBLANK: 0x70 = 112 */
	write_cmos_sensor(0x41, 0x70);	/*  , */
	write_cmos_sensor(0x42, 0x00);	/* VBLANK: 0x40 = 64 */
	write_cmos_sensor(0x43, 0x15);	 /* 0x04 -> 0x40: For Max Framerate = 30fps */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x02);	/*  , */
	write_cmos_sensor(0x13, 0x40);	/*  , */
	write_cmos_sensor(0x14, 0x04);	/*  , */
	write_cmos_sensor(0x18, 0x1c);	/*  , */
	write_cmos_sensor(0x19, 0x00);	/*  , */
	write_cmos_sensor(0x1a, 0x00);	/*  , */
	write_cmos_sensor(0x1b, 0x08);	/*  , */
	write_cmos_sensor(0x20, 0x33);	/*  , */
	write_cmos_sensor(0x21, 0xaa);	/*  , */
	write_cmos_sensor(0x22, 0xa7);	/*  , */
	write_cmos_sensor(0x23, 0x30);	/*  , */
	write_cmos_sensor(0x24, 0x4a);	/*  , */
	write_cmos_sensor(0x28, 0x0c);	/*  , */
	write_cmos_sensor(0x29, 0x80);	/*  , */
	write_cmos_sensor(0x31, 0x99);	/*  , */
	write_cmos_sensor(0x32, 0x00);	/*  , */
	write_cmos_sensor(0x33, 0x00);	/*  , */
	write_cmos_sensor(0x34, 0x3c);	/*  , */
	write_cmos_sensor(0x35, 0x01);	/*  , */
	write_cmos_sensor(0x3b, 0x48);	/*  , */
	write_cmos_sensor(0x50, 0x21);	/*  , */
	write_cmos_sensor(0x52, 0xa2);	/*  , */
	write_cmos_sensor(0x53, 0x0a);	/*  , */
	write_cmos_sensor(0x54, 0x30);	/*  , */
	write_cmos_sensor(0x55, 0x10);	/*  , */
	write_cmos_sensor(0x56, 0x08);	/*  ,make for sync Margin */
	write_cmos_sensor(0x59, 0x0F);	/*  , */
	write_cmos_sensor(0x60, 0x63);	/*  ,//101120 */
	write_cmos_sensor(0x61, 0x70);	/*  , */
	write_cmos_sensor(0x62, 0x64);	/*  , */
	write_cmos_sensor(0x63, 0x6e);	/*  , */
	write_cmos_sensor(0x64, 0x64);	/*  , */
	write_cmos_sensor(0x65, 0x6e);	/*  , */
	write_cmos_sensor(0x72, 0x65);	/*  , */
	write_cmos_sensor(0x73, 0x6d);	/*  , */
	write_cmos_sensor(0x74, 0x65);	/*  , */
	write_cmos_sensor(0x75, 0x6d);	/*  , */
	write_cmos_sensor(0x80, 0x02);	/*  , */
	write_cmos_sensor(0x81, 0x57);	/*  , */
	write_cmos_sensor(0x82, 0x07);	/*  , */
	write_cmos_sensor(0x83, 0x14);	/*  , */
	write_cmos_sensor(0x84, 0x07);	/*  , */
	write_cmos_sensor(0x85, 0x14);	/*  , */
	write_cmos_sensor(0x92, 0x2c);	/*  , */
	write_cmos_sensor(0x93, 0x3c);	/*  , */
	write_cmos_sensor(0x94, 0x2c);	/*  , */
	write_cmos_sensor(0x95, 0x3c);	/*  , */
	write_cmos_sensor(0xa0, 0x03);	/*  , */
	write_cmos_sensor(0xa1, 0x55);	/*  , */
	write_cmos_sensor(0xa4, 0x55);	/*  , */
	write_cmos_sensor(0xa5, 0x03);	/*  , */
	write_cmos_sensor(0xa8, 0x18);	/*  , */
	write_cmos_sensor(0xa9, 0x28);	/*  , */
	write_cmos_sensor(0xaa, 0x40);	/*  , */
	write_cmos_sensor(0xab, 0x50);	/*  , */
	write_cmos_sensor(0xac, 0x10);	/*  , */
	write_cmos_sensor(0xad, 0x0e);	/*  , */
	write_cmos_sensor(0xb8, 0x65);	/*  , */
	write_cmos_sensor(0xb9, 0x69);	/*  , */
	write_cmos_sensor(0xbc, 0x05);	/*  , */
	write_cmos_sensor(0xbd, 0x09);	/*  , */
	write_cmos_sensor(0xc0, 0x6f);	/*  , */
	write_cmos_sensor(0xc1, 0x77);	/*  , */
	write_cmos_sensor(0xc2, 0x6f);	/*  , */
	write_cmos_sensor(0xc3, 0x77);	/*  , */
	write_cmos_sensor(0xc4, 0x70);	/*  , */
	write_cmos_sensor(0xc5, 0x76);	/*  , */
	write_cmos_sensor(0xc6, 0x70);	/*  , */
	write_cmos_sensor(0xc7, 0x76);	/*  , */
	write_cmos_sensor(0xc8, 0x71);	/*  , */
	write_cmos_sensor(0xc9, 0x75);	/*  , */
	write_cmos_sensor(0xcc, 0x72);	/*  , */
	write_cmos_sensor(0xcd, 0x74);	/*  , */
	write_cmos_sensor(0xca, 0x71);	/*  , */
	write_cmos_sensor(0xcb, 0x75);	/*  , */
	write_cmos_sensor(0xce, 0x72);	/*  , */
	write_cmos_sensor(0xcf, 0x74);	/*  , */
	write_cmos_sensor(0xd0, 0x63);	/*  , */
	write_cmos_sensor(0xd1, 0x70);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x10);	/*  , */
	/* write_cmos_sensor(0x10, 0x6b); byaer10 */
	write_cmos_sensor(0x10, 0x63);	/*  ,//byaer10 */
	write_cmos_sensor(0x11, 0x43);	/*  , */
	write_cmos_sensor(0x12, 0x30);	/*  , */
	write_cmos_sensor(0x40, 0x00);	/*  , */
	write_cmos_sensor(0x41, 0xd0);	/*  ,//10->14 //10->d0 130507 */
	write_cmos_sensor(0x48, 0x84);	/*  , */
	write_cmos_sensor(0x50, 0x60);	/*  ,//disable Dark_Y_offset Y131212 */
	write_cmos_sensor(0x60, 0x47);	/*  ,//0509 adjust */
	write_cmos_sensor(0x61, 0x0c);	/*  ,//0509 adjust */
	write_cmos_sensor(0x62, 0xc0);	/*  , */
	write_cmos_sensor(0x63, 0xc0);	/*  , */
	write_cmos_sensor(0x64, 0x30);	/*  ,//0509 adjust */
	write_cmos_sensor(0x65, 0x84);	/*  ,//0509 adjust */
	write_cmos_sensor(0x66, 0x50);	/*  ,//0509 adjust */
	write_cmos_sensor(0x67, 0xf6);	/*  , */
	write_cmos_sensor(0x68, 0xff);	/*  ,//0509 adjust */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x11);	/*  , */
	write_cmos_sensor(0x10, 0x25);	/*  ,//25->21 Zara low pass filter */
	write_cmos_sensor(0x11, 0x1f);	/*  , */
	write_cmos_sensor(0x20, 0x00);	/*  , */
	write_cmos_sensor(0x21, 0x50);	/*  , */
	write_cmos_sensor(0x23, 0x0a);	/*  , */
	write_cmos_sensor(0x60, 0x10);	/*  , */
	write_cmos_sensor(0x61, 0x82);	/*  , */
	write_cmos_sensor(0x62, 0x00);	/*  , */
	write_cmos_sensor(0x63, 0x80);	/*  , // 87->83 for green noise, chage by frank */
	write_cmos_sensor(0x64, 0x83);	/*  , */
	write_cmos_sensor(0x67, 0xF0);	/*  , */
	write_cmos_sensor(0x68, 0x80);	/*  ,//30 -> 60 */
	write_cmos_sensor(0x69, 0x10);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x12);	/*  , */
	write_cmos_sensor(0x40, 0xeB);	/*  , */
	write_cmos_sensor(0x41, 0x19);	/*  , */
	write_cmos_sensor(0x50, 0x18);	/*  , */
	write_cmos_sensor(0x51, 0x24);	/*  , */
	write_cmos_sensor(0x70, 0x3f);	/*  , */
	write_cmos_sensor(0x71, 0x00);	/*  , */
	write_cmos_sensor(0x72, 0x00);	/*  , */
	write_cmos_sensor(0x73, 0x00);	/*  , */
	write_cmos_sensor(0x74, 0x10);	/*  , */
	write_cmos_sensor(0x75, 0x10);	/*  , */
	write_cmos_sensor(0x76, 0x20);	/*  , */
	write_cmos_sensor(0x77, 0x80);	/*  , */
	write_cmos_sensor(0x78, 0x88);	/*  , */
	write_cmos_sensor(0x79, 0x18);	/*  , */
	write_cmos_sensor(0x90, 0x3d);	/*  , */
	write_cmos_sensor(0x91, 0x34);	/*  , */
	write_cmos_sensor(0x99, 0x28);	/*  , */
	write_cmos_sensor(0x9c, 0x14);	/*  , */
	write_cmos_sensor(0x9d, 0x15);	/*  , */
	write_cmos_sensor(0x9e, 0x28);	/*  , */
	write_cmos_sensor(0x9f, 0x28);	/*  , */
	write_cmos_sensor(0xb0, 0x7d);	/*  , */
	write_cmos_sensor(0xb5, 0x44);	/*  , */
	write_cmos_sensor(0xb6, 0x82);	/*  , */
	write_cmos_sensor(0xb7, 0x52);	/*  , */
	write_cmos_sensor(0xb8, 0x44);	/*  , */
	write_cmos_sensor(0xb9, 0x15);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x13);	/*  , */
	write_cmos_sensor(0x10, 0x01);	/*  , */
	write_cmos_sensor(0x11, 0x89);	/*  ,//89->81 scale of sharpness Y131212 */
	write_cmos_sensor(0x12, 0x14);	/*  , */
	write_cmos_sensor(0x13, 0x19);	/*  , */
	write_cmos_sensor(0x14, 0x08);	/*  , */
	write_cmos_sensor(0x20, 0x05);	/*  , //  0410 confirm */
	write_cmos_sensor(0x21, 0x05);	/*  , //  0410 confirm */
	write_cmos_sensor(0x23, 0x30);	/*  , */
	write_cmos_sensor(0x24, 0x33);	/*  , */
	write_cmos_sensor(0x25, 0x08);	/*  , */
	write_cmos_sensor(0x26, 0x18);	/*  , */
	write_cmos_sensor(0x27, 0x00);	/*  , */
	write_cmos_sensor(0x28, 0x08);	/*  , */
	write_cmos_sensor(0x29, 0x50);	/*  , */
	write_cmos_sensor(0x2a, 0xe0);	/*  , */
	write_cmos_sensor(0x2b, 0x10);	/*  , */
	write_cmos_sensor(0x2c, 0x28);	/*  , */
	write_cmos_sensor(0x2d, 0x40);	/*  , */
	write_cmos_sensor(0x2e, 0x00);	/*  , */
	write_cmos_sensor(0x2f, 0x00);	/*  , */
	write_cmos_sensor(0x30, 0x11);	/*  , */
	write_cmos_sensor(0x80, 0x03);	/*  , */
	write_cmos_sensor(0x81, 0x07);	/*  , */
	write_cmos_sensor(0x90, 0x03);	/*  , //  0410 confirm */
	write_cmos_sensor(0x91, 0x02);	/*  , //  0410 confirm */
	write_cmos_sensor(0x92, 0x00);	/*  , */
	write_cmos_sensor(0x93, 0x20);	/*  , */
	write_cmos_sensor(0x94, 0x41);	/*  , */
	write_cmos_sensor(0x95, 0x60);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x14);	/*  , */
	write_cmos_sensor(0x10, 0x00);	/*  ,//turn off LSC */
	write_cmos_sensor(0x20, 0x80);	/*  , */
	write_cmos_sensor(0x21, 0x80);	/*  , */
	write_cmos_sensor(0x22, 0xac);	/*  ,//0x66 */
	write_cmos_sensor(0x23, 0x70);	/*  , */
	write_cmos_sensor(0x24, 0x72);	/*  ,//0x59 */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x15);	/*  , */
	write_cmos_sensor(0x10, 0x0f);	/*  , */
	write_cmos_sensor(0x14, 0x36);	/*  ,    //CMCOFSGM */
	write_cmos_sensor(0x16, 0x28);	/*  ,    //CMCOFSGL */
	write_cmos_sensor(0x17, 0x2d);	/*  ,    //CMC SIGN */
	/* write_cmos_sensor(/CMC      )  */
	write_cmos_sensor(0x30, 0x4c);	/*  , */
	write_cmos_sensor(0x31, 0x21);	/*  , */
	write_cmos_sensor(0x32, 0x15);	/*  , */
	write_cmos_sensor(0x33, 0x0e);	/*  , */
	write_cmos_sensor(0x34, 0x53);	/*  , */
	write_cmos_sensor(0x35, 0x05);	/*  , */
	write_cmos_sensor(0x36, 0x07);	/*  , */
	write_cmos_sensor(0x37, 0x2c);	/*  , */
	write_cmos_sensor(0x38, 0x65);	/*  , */
	/* write_cmos_sensor(/CMC OFS  )  */
	write_cmos_sensor(0x40, 0x15);	/*  , */
	write_cmos_sensor(0x41, 0x94);	/*  , */
	write_cmos_sensor(0x42, 0x01);	/*  , */
	write_cmos_sensor(0x43, 0x83);	/*  , */
	write_cmos_sensor(0x44, 0x94);	/*  , */
	write_cmos_sensor(0x45, 0x16);	/*  , */
	write_cmos_sensor(0x46, 0x86);	/*  , */
	write_cmos_sensor(0x47, 0x9e);	/*  , */
	write_cmos_sensor(0x48, 0x2b);	/*  , */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x16);	/*  , */
	write_cmos_sensor(0x30, 0x00);	/*  , */
	write_cmos_sensor(0x31, 0x07);	/*  , */
	write_cmos_sensor(0x32, 0x18);	/*  , */
	write_cmos_sensor(0x33, 0x35);	/*  , */
	write_cmos_sensor(0x34, 0x68);	/*  , */
	write_cmos_sensor(0x35, 0x82);	/*  , */
	write_cmos_sensor(0x36, 0x96);	/*  , */
	write_cmos_sensor(0x37, 0xa8);	/*  , */
	write_cmos_sensor(0x38, 0xb7);	/*  , */
	write_cmos_sensor(0x39, 0xc2);	/*  , */
	write_cmos_sensor(0x3a, 0xcf);	/*  , */
	write_cmos_sensor(0x3b, 0xdc);	/*  , */
	write_cmos_sensor(0x3c, 0xe8);	/*  , */
	write_cmos_sensor(0x3d, 0xf5);	/*  , */
	write_cmos_sensor(0x3e, 0xff);	/*  , */
	/* write_cmos_sensor(          )  */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x17);	/*  , */
	write_cmos_sensor(0xc0, 0x01);	/*  , */
	write_cmos_sensor(0xc4, 0x4E);	/*  , */
	write_cmos_sensor(0xc5, 0x41);	/*  , */
	/* write_cmos_sensor(        )  */
	/* write_cmos_sensor(        )  */
	write_cmos_sensor(0x03, 0x20);	/*  , */
	write_cmos_sensor(0x10, 0x0c);	/*  , */
	write_cmos_sensor(0x11, 0x04);	/*  , */
	write_cmos_sensor(0x20, 0x01);	/*  , */
	write_cmos_sensor(0x28, 0x27);	/*  , */
	write_cmos_sensor(0x29, 0xa1);	/*  , */
	write_cmos_sensor(0x2a, 0xf0);	/*  , */
	write_cmos_sensor(0x2b, 0x34);	/*  , */
	write_cmos_sensor(0x2c, 0x2b);	/*  , */
	write_cmos_sensor(0x30, 0x78);	/*  , */
	write_cmos_sensor(0x39, 0x22);	/*  , */
	write_cmos_sensor(0x3a, 0xde);	/*  , */
	write_cmos_sensor(0x3b, 0x22);	/*  , */
	write_cmos_sensor(0x3c, 0xde);	/*  , */
	write_cmos_sensor(0x60, 0xC0);	/*  , //  0410 confirm */
	write_cmos_sensor(0x68, 0x28);	/*  , //  0410 confirm */
	write_cmos_sensor(0x69, 0x78);	/*  , //  0410 confirm */
	write_cmos_sensor(0x6A, 0x3c);	/*  , //  0410 confirm */
	write_cmos_sensor(0x6B, 0xb4);	/*  , //  0410 confirm */
	write_cmos_sensor(0x70, 0x30);	/*  , //  0410 confirm */
	write_cmos_sensor(0x76, 0x22);	/*  , */
	write_cmos_sensor(0x77, 0x02);	/*  , */
	write_cmos_sensor(0x78, 0x12);	/*  , */
	write_cmos_sensor(0x79, 0x25);	/*  , */
	write_cmos_sensor(0x7a, 0x23);	/*  , */
	write_cmos_sensor(0x7c, 0x1d);	/*  , */
	write_cmos_sensor(0x7d, 0x22);	/*  , */
	/* write_cmos_sensor(0x83, 0x01)  EXP Normal 20.00 fps */
	/* write_cmos_sensor(0x84, 0x86)  , */
	/* write_cmos_sensor(0x85, 0x00)  , */

	write_cmos_sensor(0x83, 0x00);	/*  , //EXP Normal 20.00 fps */
	write_cmos_sensor(0x84, 0xc3);	/*  , */
	write_cmos_sensor(0x85, 0x50);	/*  , */


	write_cmos_sensor(0x86, 0x00);	/*  , //EXPMin 7812.50 fps */
	write_cmos_sensor(0x87, 0xc0);	/*  , */
	write_cmos_sensor(0x88, 0x02);	/*  , //EXP Max 10 fps */
	write_cmos_sensor(0x89, 0x49);	/*  , */
	write_cmos_sensor(0x8a, 0x00);	/*  , */
	write_cmos_sensor(0x8B, 0x3a);	/*  , //EXP100 */
	write_cmos_sensor(0x8C, 0x80);	/*  , */
	write_cmos_sensor(0x8D, 0x30);	/*  , //EXP120 */
	write_cmos_sensor(0x8E, 0xc0);	/*  , */


	write_cmos_sensor(0x91, 0x00);	/*  , */
	write_cmos_sensor(0x92, 0xc3);	/*  , */
	write_cmos_sensor(0x93, 0x50);	/*  , */


	write_cmos_sensor(0x94, 0x01);	/*  , */
	write_cmos_sensor(0x95, 0xb7);	/*  , */
	write_cmos_sensor(0x96, 0x74);	/*  , */
	write_cmos_sensor(0x98, 0x8C);	/*  , */
	write_cmos_sensor(0x99, 0x23);	/*  , */
	write_cmos_sensor(0x9c, 0x03);	/*  , EXP Limit 976.56 fps */
	write_cmos_sensor(0x9d, 0xc0);	/*  , */
	write_cmos_sensor(0x9e, 0x00);	/*  , EXP Unit */
	write_cmos_sensor(0x9f, 0xc0);	/*  , */
	write_cmos_sensor(0xb0, 0x10);	/*  ,//20131212 (b0~bd) */
	write_cmos_sensor(0xb1, 0x14);	/*  ,//Analog gain min:1x */
	write_cmos_sensor(0xb2, 0xf0);	/*  ,//Analog gain max:3.5x */
	write_cmos_sensor(0xb3, 0x14);	/*  , */
	write_cmos_sensor(0xb4, 0x14);	/*  , */
	write_cmos_sensor(0xb5, 0x39);	/*  , */
	write_cmos_sensor(0xb6, 0x26);	/*  , */
	write_cmos_sensor(0xb7, 0x20);	/*  , */
	write_cmos_sensor(0xb8, 0x1d);	/*  , */
	write_cmos_sensor(0xb9, 0x1b);	/*  , */
	write_cmos_sensor(0xba, 0x1a);	/*  , */
	write_cmos_sensor(0xbb, 0x19);	/*  , */
	write_cmos_sensor(0xbc, 0x18);	/*  , */
	write_cmos_sensor(0xbd, 0x18);	/*  , */
	write_cmos_sensor(0xc0, 0x1a);	/*  , */
	write_cmos_sensor(0xc3, 0xe0);	/*  ,//48->58 20131212 */
	write_cmos_sensor(0xc4, 0xe0);	/*  ,//48->58 20131212 */
	/* write_cmos_sensor(          )  */
	/* write_cmos_sensor(          )  */
	write_cmos_sensor(0x03, 0x22);	/*  , */
	write_cmos_sensor(0x10, 0x7b);	/*  , //fb}, */
	write_cmos_sensor(0x11, 0x2e);	/*  , */
	write_cmos_sensor(0x30, 0x80);	/*  , */
	write_cmos_sensor(0x31, 0x80);	/*  , */
	write_cmos_sensor(0x38, 0x12);	/*  , */
	write_cmos_sensor(0x39, 0x33);	/*  , */
	write_cmos_sensor(0x3a, 0x88);	/*  , */
	write_cmos_sensor(0x3b, 0xc4);	/*  , */
	write_cmos_sensor(0x40, 0xf0);	/*  , */
	write_cmos_sensor(0x41, 0x33);	/*  , */
	write_cmos_sensor(0x42, 0x33);	/*  , */
	write_cmos_sensor(0x43, 0xf3);	/*  , */
	write_cmos_sensor(0x44, 0x55);	/*  , */
	write_cmos_sensor(0x45, 0x44);	/*  , */
	write_cmos_sensor(0x46, 0x00);	/*  , */
	write_cmos_sensor(0x60, 0x00);	/*  , */
	write_cmos_sensor(0x61, 0x00);	/*  , */
	write_cmos_sensor(0x80, 0x20);	/*  , */
	write_cmos_sensor(0x81, 0x20);	/*  , */
	write_cmos_sensor(0x82, 0x20);	/*  , */
	write_cmos_sensor(0x83, 0x55);	/*  , */
	write_cmos_sensor(0x84, 0x1b);	/*  , */
	write_cmos_sensor(0x85, 0x56);	/*  , */
	write_cmos_sensor(0x86, 0x20);	/*  , */
	write_cmos_sensor(0x87, 0x30);	/*  , */
	write_cmos_sensor(0x88, 0x25);	/*  , */
	write_cmos_sensor(0x89, 0x3f);	/*  , */
	write_cmos_sensor(0x8a, 0x36);	/*  , */
	write_cmos_sensor(0x8b, 0x00);	/*  , */
	write_cmos_sensor(0x8d, 0x14);	/*  , */
	write_cmos_sensor(0x8e, 0x41);	/*  , */
	write_cmos_sensor(0x8f, 0x5d);	/*  , //0x63}, */
	write_cmos_sensor(0x90, 0x58);	/*  , //0x60}, */
	write_cmos_sensor(0x91, 0x50);	/*  , //0x5a}, */
	write_cmos_sensor(0x92, 0x47);	/*  , //0x52}, */
	write_cmos_sensor(0x93, 0x41);	/*  , //0x48}, */
	write_cmos_sensor(0x94, 0x3c);	/*  , //0x3d}, */
	write_cmos_sensor(0x95, 0x39);	/*  , */
	write_cmos_sensor(0x96, 0x34);	/*  , */
	write_cmos_sensor(0x97, 0x31);	/*  , */
	write_cmos_sensor(0x98, 0x2f);	/*  , */
	write_cmos_sensor(0x99, 0x29);	/*  , */
	write_cmos_sensor(0x9a, 0x26);	/*  , */
	write_cmos_sensor(0x9b, 0x09);	/*  , */
	write_cmos_sensor(0xb0, 0x30);	/*  , */
	write_cmos_sensor(0xb1, 0x48);	/*  , */
	/* write_cmos_sensor(          ) // */
	write_cmos_sensor(0x03, 0x20);	/*  , */
	write_cmos_sensor(0x10, 0x0c);	/*  , */
	write_cmos_sensor(0x03, 0x00);	/*  , */
	write_cmos_sensor(0x01, 0xf0);	/*  ,//f0 */

	/* END */
	/* [END] */


	/* The register only need to enable 1 time. */

}				/*      sensor_init  */


static void preview_setting(void)
{

/* write_cmos_sensor(0x0100,0x00); */
/* mDELAY(10); */

/* write_cmos_sensor(0x0100,0x01); */
/* mDELAY(10); */
/*	write_cmos_sensor(0x03, 0);
	write_cmos_sensor(0x40, (imgsensor.line_length-hi704raw_valid_linelength) >> 8);
	write_cmos_sensor(0x41, (imgsensor.line_length-hi704raw_valid_linelength) & 0xFF);
	write_cmos_sensor(0x42, (imgsensor.frame_length-hi704raw_valid_framelength) >> 8);
	write_cmos_sensor(0x43, (imgsensor.frame_length-hi704raw_valid_framelength) & 0xFF);
*/

	write_cmos_sensor(0x3, 0);	/* For No Fixed Framerate Bit[2] */
	write_cmos_sensor(0x01, 0xf1);	/* f0 */
	write_cmos_sensor(0x11, 0x90);	/* For No Fixed Framerate Bit[2] */

	write_cmos_sensor(0x3, 0);	/* For No Fixed Framerate Bit[2] */
	write_cmos_sensor(0x01, 0xf0);	/* f0 */

	mDELAY(20);

}				/*      preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	if (currefps == 240) {	/* 24fps for PIP */
		/* @@full_132PCLK_24.75 */
/* write_cmos_sensor(0x0100,0x00); */
/* mDELAY(10); */

/* write_cmos_sensor(0x0100,0x01); */
/* mDELAY(10); */

	} else {		/* 30fps for Normal capture & ZSD */
/* write_cmos_sensor(0x0100,0x00); */
/* mDELAY(10); */

/* write_cmos_sensor(0x0100,0x01); */
/* mDELAY(10); */


	}
	preview_setting();
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);

/* write_cmos_sensor(0x0100,0x00); */
/* mDELAY(10); */

/* write_cmos_sensor(0x0100,0x01); */
/* mDELAY(10); */
/*	write_cmos_sensor(0x03, 0);
	write_cmos_sensor(0x40, (imgsensor.line_length-hi704raw_valid_linelength) >> 8);
	write_cmos_sensor(0x41, (imgsensor.line_length-hi704raw_valid_linelength) & 0xFF);
	write_cmos_sensor(0x42, (imgsensor.frame_length-hi704raw_valid_framelength) >> 8);
	write_cmos_sensor(0x43, (imgsensor.frame_length-hi704raw_valid_framelength) & 0xFF);
*/
	preview_setting();
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}


/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	LOG_INF(" HI704_RAW get_imgsensor_id - start\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			LOG_INF(" HI704_RAW get_imgsensor_id - 1\n");
			write_cmos_sensor(0x01, 0xf1);
			LOG_INF(" HI704_RAW get_imgsensor_id - 2\n");
			write_cmos_sensor(0x01, 0xf3);
			LOG_INF(" HI704_RAW get_imgsensor_id - 3\n");
			write_cmos_sensor(0x01, 0xf1);
			*sensor_id = read_cmos_sensor(0x04);

			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,
				*sensor_id);

			if (*sensor_id == 0x96) {

				*sensor_id = imgsensor_info.sensor_id;
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
/* LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id); */
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	/* *sensor_id = imgsensor_info.sensor_id; */
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	LOG_INF(" HI704_RAW get_imgsensor_id - end\n");

	return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*    open
*
* DESCRIPTION
*    This function initialize the registers of CMOS sensor
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	LOG_1;
	LOG_2;

	LOG_INF(" HI704_RAW open - start\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			write_cmos_sensor(0x01, 0xf1);
			write_cmos_sensor(0x01, 0xf3);
			write_cmos_sensor(0x01, 0xf1);
			sensor_id = read_cmos_sensor(0x04);
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x, .h id=0x%x\n",
				imgsensor.i2c_write_id, sensor_id, imgsensor_info.sensor_id);
			/* sensor_id = imgsensor_info.sensor_id; */
			if (sensor_id == 0x96) {
				sensor_id = imgsensor_info.sensor_id;
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}

	/* sensor_id = imgsensor_info.sensor_id; */
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF(" HI704_RAW open - end\n");

	return ERROR_NONE;
}				/*    open  */



/*************************************************************************
* FUNCTION
*    close
*
* DESCRIPTION
*
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*    This function start the sensor preview.
*
* PARAMETERS
*    *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	video_nightmode = 0;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    preview   */

/*************************************************************************
* FUNCTION
*    capture
*
* DESCRIPTION
*    This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF
			    ("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			     imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	if (imgsensor.test_pattern == KAL_TRUE) {
		write_cmos_sensor(0x03, 0x00);
		write_cmos_sensor(0x50, 0x04);
	}
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	set_video_mode_flag = 1;	/*  */
	video_nightmode = 0;	/*  */
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    slim_video     */

/*************************************************************************
* FUNCTION
* Custom1
*
* DESCRIPTION
*   This function start the sensor Custom1.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom3   */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom4   */


static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/*  Custom5   */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;

	LOG_INF("L\n");
	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	/* sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10;*/ /* not use */
	/* sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10;*/ /* not use */
	/* imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate;*/ /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*    get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);	/* Custom1 */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);	/* Custom1 */
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	framerate = framerate * 10;
	LOG_INF("framerate = %d, set_video_mode_flag = %d\n ", framerate, set_video_mode_flag);
	/* SetVideoMode Function should fix framerate */

	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if (set_video_mode_flag == 0)
		return ERROR_NONE;
	if (framerate != 300) {
		video_nightmode = 1;

		write_cmos_sensor(0x3, 0);		/* For No Fixed Framerate Bit[2] */
		write_cmos_sensor(0x11, 0x94);	/* For No Fixed Framerate Bit[2] */
	} else {
		video_nightmode = 0;
		/*write_cmos_sensor(0x3, 0)  */   /* For No Fixed Framerate Bit[2] */
		/*write_cmos_sensor(0x11, 0x90)*/ /* For No Fixed Framerate Bit[2] */
	}
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);

	if (set_video_mode_flag == 1)
		set_max_framerate(imgsensor.current_fps, 1);
	set_video_mode_flag = 0;

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.normal_video.framelength) ? (frame_length -
								 imgsensor_info.normal_video.
								 framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
			    imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length >
			     imgsensor_info.cap1.framelength) ? (frame_length -
								 imgsensor_info.cap1.
								 framelength) : 0;
			imgsensor.frame_length =
			    imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF
				    ("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				     framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 /
			    imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length >
			     imgsensor_info.cap.framelength) ? (frame_length -
								imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
			    imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.hs_video.framelength) ? (frame_length -
							     imgsensor_info.hs_video.
							     framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.slim_video.framelength) ? (frame_length -
							       imgsensor_info.slim_video.
							       framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length =
		    imgsensor_info.custom1.pclk / framerate * 10 /
		    imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom1.framelength) ? (frame_length -
							    imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length =
		    imgsensor_info.custom2.pclk / framerate * 10 /
		    imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom2.framelength) ? (frame_length -
							    imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length =
		    imgsensor_info.custom3.pclk / framerate * 10 /
		    imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom3.framelength) ? (frame_length -
							    imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length =
		    imgsensor_info.custom4.pclk / framerate * 10 /
		    imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom4.framelength) ? (frame_length -
							    imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length =
		    imgsensor_info.custom5.pclk / framerate * 10 /
		    imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom5.framelength) ? (frame_length -
							    imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		LOG_INF("Warning: Invalid scenario_id = %d\n", scenario_id);
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x03, 0x00);
		write_cmos_sensor(0x50, 0x04);
	} else {
		write_cmos_sensor(0x03, 0x00);
		write_cmos_sensor(0x50, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/*unsigned long long *feature_return_para = (unsigned long long *)feature_para;*/

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	LOG_INF("++++++++++++\n");
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		LOG_INF("addr = %x, data = %x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		write_cmos_sensor(0x03, sensor_reg_data->RegAddr >> 8);
		write_cmos_sensor(sensor_reg_data->RegAddr & 0xff, sensor_reg_data->RegData);
		/* write_cmos_sensor(0x3, 0x20); */
		/* write_cmos_sensor(0x83, (sensor_reg_data->RegData >> 16) & 0xFF); */
		/* write_cmos_sensor(0x84, (sensor_reg_data->RegData >>  8) & 0xFF); */
		/* write_cmos_sensor(0x85, (sensor_reg_data->RegData >>  0) & 0xFF); */
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		write_cmos_sensor(0x03, sensor_reg_data->RegAddr >> 8);
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr & 0xff);

		LOG_INF("addr = %x, data = %x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		LOG_INF("=== SENSOR_FEATURE_CHECK_SENSOR_ID ===\n");
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) * feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *feature_data,
					      *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *(feature_data),
						  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (int)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (int)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (int)*feature_data);
		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1), (UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data, (UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	default:
		LOG_INF("Warning: invalid feature cmd: %d\n", feature_id);
		break;
	}
	LOG_INF("----------------\n");
	return ERROR_NONE;
}				/*    feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI704_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{

	LOG_INF(" HI704_RAW_SensorInit() - enter\n");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      HI704_MIPI_RAW_SensorInit       */
