#ifndef _KD_CAMERA_HW_H_
#define _KD_CAMERA_HW_H_


/*#include <cust_gpio_usage.h>*/
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_ext.h>	/* for gpio on mt6397 */
#include <linux/gpio.h>

#ifdef MTK_MT6306_SUPPORT
#include <mach/dcl_sim_gpio.h>
#endif


#include <mach/mt_pm_ldo.h>
/*#include <pmic_drv.h>*/



/*  */
/* Power */
/*
	Remain power pin defines for non-Regulator power control
*/
#define CAMERA_POWER_VCAM_A		MT65XX_POWER_LDO_VCAMA	/* AVDD ,  VCAMA */
#define CAMERA_POWER_VCAM_D		MT65XX_POWER_LDO_VGP1	/* DVDD ,  VCAMD */
#define CAMERA_POWER_VCAM_A2		MT65XX_POWER_LDO_VGP3	/* AFVDD , VCAMAF */
#define CAMERA_POWER_VCAM_D2		MT65XX_POWER_LDO_VGP2	/* DOVDD , VCAMIO */
#define SUB_CAMERA_POWER_VCAM_D		MT65XX_POWER_LDO_VGP1


/* Aligned with : ../kernel/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct/cust_gpio_usage */
/* Main sensor */
#ifdef GPIO_CAMERA_CMRST_PIN
#define CAMERA_CMRST_PIN            (GPIO_CAMERA_CMRST_PIN & (~(0x80000000)))
#define CAMERA_CMRST_PIN_M_GPIO     GPIO_CAMERA_CMRST_PIN_M_GPIO
#else
#define CAMERA_CMRST_PIN            GPIO_CAMERA_INVALID
#define CAMERA_CMRST_PIN_M_GPIO     GPIO_CAMERA_INVALID
#endif


#ifdef GPIO_CAMERA_CMPDN_PIN
#define CAMERA_CMPDN_PIN            (GPIO_CAMERA_CMPDN_PIN & (~(0x80000000)))
#define CAMERA_CMPDN_PIN_M_GPIO     GPIO_CAMERA_CMPDN_PIN_M_GPIO
#else
#define CAMERA_CMPDN_PIN            GPIO_CAMERA_INVALID
#define CAMERA_CMPDN_PIN_M_GPIO     GPIO_CAMERA_INVALID
#endif

/* FRONT sensor */
#ifdef GPIO_CAMERA_2_CMRST_PIN
#define CAMERA_CMRST1_PIN            (GPIO_CAMERA_2_CMRST_PIN & (~(0x80000000)))
#define CAMERA_CMRST1_PIN_M_GPIO     GPIO_CAMERA_2_CMRST_PIN_M_GPIO
#else
#define CAMERA_CMRST1_PIN            GPIO_CAMERA_INVALID
#define CAMERA_CMRST1_PIN_M_GPIO     GPIO_CAMERA_INVALID
#endif


#ifdef GPIO_CAMERA_2_CMPDN_PIN
#define CAMERA_CMPDN1_PIN            (GPIO_CAMERA_2_CMPDN_PIN & (~(0x80000000)))
#define CAMERA_CMPDN1_PIN_M_GPIO     GPIO_CAMERA_2_CMPDN_PIN_M_GPIO
#else
#define CAMERA_CMPDN1_PIN            GPIO_CAMERA_INVALID
#define CAMERA_CMPDN1_PIN_M_GPIO     GPIO_CAMERA_INVALID
#endif


/* Define I2C Bus Num */
#define SUPPORT_I2C_BUS_NUM1        2
#define SUPPORT_I2C_BUS_NUM2        3


typedef enum {
	VDD_None,
	PDN,
	RST,
	SensorMCLK,

#ifdef MTKCAM_USING_PWRREG	/* Power Regulator Framework */
	AVDD,
	DVDD,
	DOVDD,
	AFVDD
#else
	AVDD = CAMERA_POWER_VCAM_A,
	DVDD = CAMERA_POWER_VCAM_D,
	DOVDD = CAMERA_POWER_VCAM_D2,
	AFVDD = CAMERA_POWER_VCAM_A2
#endif
} PowerType;

typedef enum {
	Vol_Low = 0,
	Vol_High = 1,
	Vol_900 = VOL_0900,
	Vol_1000 = VOL_1000,
	Vol_1100 = VOL_1100,
	Vol_1200 = VOL_1200,
	Vol_1300 = VOL_1300,
	Vol_1350 = VOL_1350,
	Vol_1500 = VOL_1500,
	Vol_1800 = VOL_1800,
	Vol_2000 = VOL_2000,
	Vol_2100 = VOL_2100,
	Vol_2500 = VOL_2500,
	Vol_2800 = VOL_2800,
	Vol_3000 = VOL_3000,
	Vol_3300 = VOL_3300,
	Vol_3400 = VOL_3400,
	Vol_3500 = VOL_3500,
	Vol_3600 = VOL_3600
} Voltage;


typedef struct {
	PowerType PowerType;
	Voltage Voltage;
	u32 Delay;
} PowerInformation;


typedef struct {
	char *SensorName;
	PowerInformation PowerInfo[12];
} PowerSequence;

typedef struct {
	PowerSequence PowerSeq[16];
} PowerUp;

typedef struct {
	u32 Gpio_Pin;
	u32 Gpio_Mode;
	Voltage Voltage;
} PowerCustInfo;

typedef struct {
	PowerCustInfo PowerCustInfo[6];
} PowerCust;


extern void ISP_MCLK1_EN(BOOL En);
extern void ISP_MCLK2_EN(BOOL En);
extern void ISP_MCLK3_EN(BOOL En);

/* Camera Regulator Power Control */
#ifdef MTKCAM_USING_PWRREG	/* Power Regulator Framework */

extern BOOL CAMERA_Regulator_poweron(int PinIdx, int PwrType, int Voltage);
extern BOOL CAMERA_Regulator_powerdown(int PinIdx, int PwrType);

#endif

#endif
