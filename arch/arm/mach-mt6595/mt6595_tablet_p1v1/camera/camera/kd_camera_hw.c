#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
/* #define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg) */
#define PK_DBG_FUNC(fmt, arg...)    printk(fmt, ##arg)


#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
		do {    \
		   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
		} while (0)
#else
#define PK_DBG(a, ...)
#define PK_ERR(a, ...)
#define PK_XLOG_INFO(fmt, args...)
#endif

#ifndef BOOL
typedef unsigned char BOOL;
#endif

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On,
		       char *mode_name)
{

	u32 pinSetIdx = 0;	/* default main sensor */

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4
#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3


	u32 pinSet[2][8] = {
		/* for main sensor */
		{CAMERA_CMRST_PIN,
		 CAMERA_CMRST_PIN_M_GPIO,	/* mode */
		 GPIO_OUT_ONE,	/* ON state */
		 GPIO_OUT_ZERO,	/* OFF state */
		 CAMERA_CMPDN_PIN,
		 CAMERA_CMPDN_PIN_M_GPIO,
		 GPIO_OUT_ONE,
		 GPIO_OUT_ZERO,
		 },
		/* for sub sensor */
		{CAMERA_CMRST1_PIN,
		 CAMERA_CMRST1_PIN_M_GPIO,
		 GPIO_OUT_ONE,
		 GPIO_OUT_ZERO,
		 CAMERA_CMPDN1_PIN,
		 CAMERA_CMPDN1_PIN_M_GPIO,
		 GPIO_OUT_ZERO,
		 GPIO_OUT_ONE,
		 }
	};

	if ((DUAL_CAMERA_MAIN_SENSOR == SensorIdx) && currSensorName
	    && (0 == strcmp(SENSOR_DRVNAME_IMX135_MIPI_RAW, currSensorName))) {
		pinSetIdx = 0;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(SENSOR_DRVNAME_OV2659_YUV, currSensorName))) {
		pinSetIdx = 1;
	} else {
		PK_DBG("Not Match ! Bypass:  SensorIdx = %d (1:Main , 2:Sub), SensorName=%s\n",
		       SensorIdx, currSensorName);
		return -ENODEV;
	}



	/* power ON */
	if (On) {

		PK_DBG("[%s][CameraPowerON] %s camera : %s\n", __func__,
		       (pinSetIdx == 0 ? "Main" : "Sub"), currSensorName);

/*
	if(pinSetIdx == 0 ) {
	    ISP_MCLK1_EN(1);
	}
	else if (pinSetIdx == 1) {
	    ISP_MCLK3_EN(1);
	}
	else if (pinSetIdx == 2) {
	    ISP_MCLK2_EN(1);
	}
*/

		ISP_MCLK1_EN(1);
		ISP_MCLK3_EN(1);
		ISP_MCLK2_EN(1);


		/* Reset all pins first */
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
			PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
		}
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
			PK_DBG("[CAMERA LENS] set gpio failed!!\n");
		}

		if (mt_set_gpio_mode
		    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
		     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[1 - pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
		     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
			PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
		}
		if (mt_set_gpio_mode
		    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
		     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[1 - pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
		     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
			PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
		}

		if (pinSetIdx == 0) {	/* Main camera : imx135 */
			/* VCAM_IO */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			}
			/* VCAM_A */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			}
			/* DVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1000, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			}
			/* AF_VCC */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			}

			/* disable inactive sensor first */
			if (GPIO_CAMERA_INVALID != pinSet[1 - pinSetIdx][IDX_PS_CMRST]) {
				/* RST pin */
				if (mt_set_gpio_mode
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
				     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
					PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
				     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
					PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
				}

				if (mt_set_gpio_mode
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
				     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
					PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
				     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
					PK_DBG("[CAMERA LENS] set gpio failed!!\n");
				}	/* high == power down lens module */
			}
			/* enable active sensor */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				if (mt_set_gpio_mode
				    (pinSet[pinSetIdx][IDX_PS_CMRST],
				     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
					PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[pinSetIdx][IDX_PS_CMRST],
				     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
					PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
				}
				mdelay(10);
				if (mt_set_gpio_out
				    (pinSet[pinSetIdx][IDX_PS_CMRST],
				     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_ON])) {
					PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
				}
				mdelay(1);

				/* PDN pin */
				if (mt_set_gpio_mode
				    (pinSet[pinSetIdx][IDX_PS_CMPDN],
				     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
					PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[pinSetIdx][IDX_PS_CMPDN],
				     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON])) {
					PK_DBG("[CAMERA LENS] set gpio failed!!\n");
				}
			}
		} else if (pinSetIdx == 1) {	/* Sub camera: OV2659 */
			/* DOVDD */
			PK_DBG("[ON_general 1.8V]sensorIdx:%d\n", SensorIdx);
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			}
			mdelay(1);

			/* AVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			}
			/* DVDD      // Use different IO power, Sub cam : VGP3 */
			if (TRUE != hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1500, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			}
			mdelay(5);

			/* disable inactive sensor first */
			if (GPIO_CAMERA_INVALID != pinSet[1 - pinSetIdx][IDX_PS_CMRST]) {
				/* RST pin */
				if (mt_set_gpio_mode
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
				     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
					PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
				     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
					PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
				}

				if (mt_set_gpio_mode
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
				     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
					PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
				     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
					PK_DBG("[CAMERA LENS] set gpio failed!!\n");
				}	/* high == power down lens module */
			}
			/* enable active sensor */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				/* RST pin */
				if (mt_set_gpio_mode
				    (pinSet[pinSetIdx][IDX_PS_CMRST],
				     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
					PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[pinSetIdx][IDX_PS_CMRST],
				     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_ON])) {
					PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
				}
				mdelay(5);

				/* PDN pin */
				if (mt_set_gpio_mode
				    (pinSet[pinSetIdx][IDX_PS_CMPDN],
				     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
					PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
				}
				if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
					PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
				}
				if (mt_set_gpio_out
				    (pinSet[pinSetIdx][IDX_PS_CMPDN],
				     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON])) {
					PK_DBG("[CAMERA LENS] set gpio failed!!\n");
				}
				mdelay(5);
			}

			mdelay(5);

		}
	} else {		/* power OFF */
/*
	if(pinSetIdx == 0 ) {
	    ISP_MCLK1_EN(0);
	}
	else if (pinSetIdx == 1) {
	    ISP_MCLK3_EN(0);
	}
	else if (pinSetIdx == 2) {
	    ISP_MCLK2_EN(0);
	}
*/

		ISP_MCLK1_EN(0);
		ISP_MCLK3_EN(0);
		ISP_MCLK2_EN(0);

		/* PK_DBG("[OFF]sensorIdx:%d\n",SensorIdx); */
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
				PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
				PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
				PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
			}	/* low == reset sensor */
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
				PK_DBG("[CAMERA LENS] set gpio failed!!\n");
			}	/* high == power down lens module */
		}
		mdelay(5);

		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_A, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
		}
		if (pinSetIdx == 0) {
			if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
			}
		}
		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
		}
		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
		}
		mdelay(5);

	}			/*  */

	return 0;

}
EXPORT_SYMBOL(kdCISModulePowerOn);

/* !-- */
/*  */
