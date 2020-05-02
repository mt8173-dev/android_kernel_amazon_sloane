#ifndef _KD_CAMERA_HW_H_
#define _KD_CAMERA_HW_H_

#include <mach/mt_pm_ldo.h>
#include <kd_imgsensor.h>

typedef enum {
	VDD_None,
	PDN,
	RST,
	SensorMCLK,
	AVDD,
	DVDD,
	DOVDD,
	AFVDD,
	LDO
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


#ifndef BOOL
typedef unsigned char BOOL;
#endif

/* defined in kd_sensorlist.c */
extern void ISP_MCLK1_EN(BOOL En);
extern void ISP_MCLK2_EN(BOOL En);
extern int mtkcam_gpio_set(int PinIdx, int PwrType, int Val);
extern PowerUp PowerOnList;
extern const int camera_i2c_bus_num1;
extern const int camera_i2c_bus_num2;

/* Camera Power Regulator Control */
#ifdef MTKCAM_USING_PWRREG

extern BOOL CAMERA_Regulator_poweron(int PinIdx, int PwrType, int Voltage);
extern BOOL CAMERA_Regulator_powerdown(int PinIdx, int PwrType);

#endif


#endif

