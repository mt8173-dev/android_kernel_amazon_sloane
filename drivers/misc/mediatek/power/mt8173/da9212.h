/*****************************************************************************
*
* Filename:
* ---------
*   da9212.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   da9212 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _da9212_SW_H_
#define _da9212_SW_H_

extern void da9212_dump_register(void);
extern kal_uint32 da9212_read_interface
    (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);
extern kal_uint32 da9212_config_interface
    (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);
extern int is_da9212_exist(void);
extern int is_da9212_sw_ready(void);
extern int da9212_buck_set_en(kal_uint8 buck, int en_bit);
extern int da9212_buck_get_en(kal_uint8 buck);
extern int da9212_buck_set_mode(kal_uint8 buck, int mode);
extern int da9212_buck_get_mode(kal_uint8 buck);
extern int da9212_buck_set_switch(kal_uint8 buck, int reg);
extern int da9212_buck_get_switch(kal_uint8 buck);
extern int da9212_buck_set_voltage(kal_uint8 buck, unsigned long vol);
extern unsigned long da9212_buck_get_voltage(kal_uint8 buck);
#define DA9212_MIN_MV	300
#define DA9212_MAX_MV	1570
#define DA9212_STEP_MV	10
#define	DA9212_BUCK_A				0x00
#define	DA9212_BUCK_B				0x01

#define	DA9212_VBUCKA_A				0x00
#define	DA9212_VBUCKA_B				0x01
#define	DA9212_VBUCKB_A				0x02
#define	DA9212_VBUCKB_B				0x03

/* Page selection */
#define	DA9212_REG_PAGE_CON			0x00

/* System Control and Event Registers */
#define	DA9212_REG_STATUS_A			0x50
#define	DA9212_REG_STATUS_B			0x51
#define	DA9212_REG_EVENT_A			0x52
#define	DA9212_REG_EVENT_B			0x53
#define	DA9212_REG_MASK_A			0x54
#define	DA9212_REG_MASK_B			0x55
#define	DA9212_REG_CONTROL_A		0x56

/* GPIO Control Registers */
#define	DA9212_REG_GPIO_0_1			0x58
#define	DA9212_REG_GPIO_2_3			0x59
#define	DA9212_REG_GPIO_4			0x5A

/* Regulator Registers */
#define	DA9212_REG_BUCKA_CONT		0x5D
#define	DA9212_REG_BUCKB_CONT		0x5E
#define	DA9212_REG_BUCK_ILIM		0xD0
#define	DA9212_REG_BUCKA_CONF		0xD1
#define	DA9212_REG_BUCKB_CONF		0xD2
#define	DA9212_REG_BUCK_CONF		0xD3
#define	DA9212_REG_VBUCKA_MAX		0xD5
#define	DA9212_REG_VBUCKB_MAX		0xD6
#define	DA9212_REG_VBUCKA_A			0xD7
#define	DA9212_REG_VBUCKA_B			0xD8
#define	DA9212_REG_VBUCKB_A			0xD9
#define	DA9212_REG_VBUCKB_B			0xDA

/* I2C Interface Settings */
#define DA9212_REG_INTERFACE		0x105

/* OTP */
#define	DA9212_REG_OPT_COUNT		0x140
#define	DA9212_REG_OPT_ADDR			0x141
#define	DA9212_REG_OPT_DATA			0x142

/* Customer Trim and Configuration */
#define	DA9212_REG_CONFIG_A			0x143
#define	DA9212_REG_CONFIG_B			0x144
#define	DA9212_REG_CONFIG_C			0x145
#define	DA9212_REG_CONFIG_D			0x146
#define	DA9212_REG_CONFIG_E			0x147
#define	DA9212_REG_INTERFACE3		0x149

/*
 * Registers bits
 */
/* DA9212_REG_PAGE_CON (addr=0x00) */
#define	DA9212_PEG_PAGE_SHIFT			0
#define	DA9212_PEG_PAGE_REVERT_SHIFT	7
#define	DA9212_REG_PAGE_MASK			x0F
/* On I2C registers 0x00 - 0xFF */
#define	DA9212_REG_PAGE0				0
/* On I2C registers 0x100 - 0x1FF */
#define	DA9212_REG_PAGE2				2
#define	DA9212_PAGE_WRITE_MODE			0x00
#define	DA9212_REPEAT_WRITE_MODE		0x40
#define	DA9212_PAGE_REVERT				0x80

/* DA9212_REG_STATUS_A (addr=0x50) */
#define	DA9212_GPI0					0x01
#define	DA9212_GPI1					0x02
#define	DA9212_GPI2					0x04
#define	DA9212_GPI3					0x08
#define	DA9212_GPI4					0x10

/* DA9212_REG_EVENT_A (addr=0x52) */
#define	DA9212_E_GPI0				0x01
#define	DA9212_E_GPI1				0x02
#define	DA9212_E_GPI2				0x04
#define	DA9212_E_GPI3				0x08
#define	DA9212_E_GPI4				0x10

/* DA9212_REG_EVENT_B (addr=0x53) */
#define	DA9212_E_NPWRGOOD_A			0x01
#define	DA9212_E_NPWRGOOD_B			0x02
#define	DA9212_E_TEMP_WARN			0x04
#define	DA9212_E_TEMP_CRIT			0x08
#define	DA9212_E_OVCURR_A			0x10
#define	DA9212_E_OVCURR_B			0x20

/* DA9212_REG_MASK_A (addr=0x54) */
#define	DA9212_M_GPI0				0x01
#define	DA9212_M_GPI1				0x02
#define	DA9212_M_GPI2				0x04
#define	DA9212_M_GPI3				0x08
#define	DA9212_M_GPI4				0x10
#define	DA9212_M_UVLO_IO			0x40

/* DA9212_REG_MASK_B (addr=0x55) */
#define	DA9212_M_NPWRGOOD_A			0x01
#define	DA9212_M_NPWRGOOD_B			0x02
#define	DA9212_M_TEMP_WARN			0x04
#define	DA9212_M_TEMP_CRIT			0x08
#define	DA9212_M_OVCURR_A			0x10
#define	DA9212_M_OVCURR_B			0x20

/* DA9212_REG_CONTROL_A (addr=0x56) */
#define	DA9212_DEBOUNCING_SHIFT		0
#define	DA9212_DEBOUNCING_MASK		0x07
#define	DA9212_SLEW_RATE_A_SHIFT	3
#define	DA9212_SLEW_RATE_A_MASK		0x18
#define	DA9212_SLEW_RATE_B_SHIFT	5
#define	DA9212_SLEW_RATE_B_MASK		0x60
#define	DA9212_V_LOCK				0x80

/* DA9212_REG_GPIO_0_1 (addr=0x58) */
#define	DA9212_GPIO0_PIN_SHIFT		0
#define	DA9212_GPIO0_PIN_MASK		0x03
#define	DA9212_GPIO0_PIN_GPI		0x00
#define	DA9212_GPIO0_PIN_GPO_OD		0x02
#define	DA9212_GPIO0_PIN_GPO		0x03
#define	DA9212_GPIO0_TYPE			0x04
#define	DA9212_GPIO0_TYPE_GPI		0x00
#define	DA9212_GPIO0_TYPE_GPO		0x04
#define	DA9212_GPIO0_MODE			0x08
#define	DA9212_GPIO1_PIN_SHIFT		4
#define	DA9212_GPIO1_PIN_MASK		0x30
#define	DA9212_GPIO1_PIN_GPI		0x00
#define	DA9212_GPIO1_PIN_VERROR		0x10
#define	DA9212_GPIO1_PIN_GPO_OD		0x20
#define	DA9212_GPIO1_PIN_GPO		0x30
#define	DA9212_GPIO1_TYPE_SHIFT		0x40
#define	DA9212_GPIO1_TYPE_GPI		0x00
#define	DA9212_GPIO1_TYPE_GPO		0x40
#define	DA9212_GPIO1_MODE			0x80

/* DA9212_REG_GPIO_2_3 (addr=0x59) */
#define	DA9212_GPIO2_PIN_SHIFT		0
#define	DA9212_GPIO2_PIN_MASK		0x03
#define	DA9212_GPIO2_PIN_GPI		0x00
#define	DA9212_GPIO5_PIN_BUCK_CLK	0x10
#define	DA9212_GPIO2_PIN_GPO_OD		0x02
#define	DA9212_GPIO2_PIN_GPO		0x03
#define	DA9212_GPIO2_TYPE			0x04
#define	DA9212_GPIO2_TYPE_GPI		0x00
#define	DA9212_GPIO2_TYPE_GPO		0x04
#define	DA9212_GPIO2_MODE			0x08
#define	DA9212_GPIO3_PIN_SHIFT		4
#define	DA9212_GPIO3_PIN_MASK		0x30
#define	DA9212_GPIO3_PIN_GPI		0x00
#define	DA9212_GPIO3_PIN_IERROR		0x10
#define	DA9212_GPIO3_PIN_GPO_OD		0x20
#define	DA9212_GPIO3_PIN_GPO		0x30
#define	DA9212_GPIO3_TYPE_SHIFT		0x40
#define	DA9212_GPIO3_TYPE_GPI		0x00
#define	DA9212_GPIO3_TYPE_GPO		0x40
#define	DA9212_GPIO3_MODE			0x80

/* DA9212_REG_GPIO_4_5 (addr=0x5A) */
#define	DA9212_GPIO4_PIN_SHIFT		0
#define	DA9212_GPIO4_PIN_MASK		0x03
#define	DA9212_GPIO4_PIN_GPI		0x00
#define	DA9212_GPIO4_PIN_GPO_OD		0x02
#define	DA9212_GPIO4_PIN_GPO		0x03
#define	DA9212_GPIO4_TYPE			0x04
#define	DA9212_GPIO4_TYPE_GPI		0x00
#define	DA9212_GPIO4_TYPE_GPO		0x04
#define	DA9212_GPIO4_MODE			0x08

/* DA9212_REG_BUCKA/B_CONT (addr=0x5D/0x5E) */
#define	DA9212_BUCK_EN_SHIFT		0
#define	DA9212_BUCK_OFF				0x00
#define	DA9212_BUCK_ON				0x01
#define	DA9212_BUCK_EN				0x01
#define	DA9212_BUCK_GPI_SHIFT		1
#define DA9212_BUCK_GPI_MASK		0x06
#define	DA9212_BUCK_GPI_OFF			0x00
#define	DA9212_BUCK_GPI_GPIO0		0x02
#define	DA9212_BUCK_GPI_GPIO1		0x04
#define	DA9212_BUCK_GPI_GPIO3		0x06
#define	DA9212_BUCK_PD_DIS			0x08
#define	DA9212_VBUCK_SEL_SHIFT		4
#define	DA9212_VBUCK_SEL			0x10
#define	DA9212_VBUCK_SEL_A			0x00
#define	DA9212_VBUCK_SEL_B			0x10
#define	DA9212_VBUCK_GPI_SHIFT		5
#define	DA9212_VBUCK_GPI_MASK		0x60
#define	DA9212_VBUCK_GPI_OFF		0x00
#define	DA9212_VBUCK_GPI_GPIO1		0x20
#define	DA9212_VBUCK_GPI_GPIO2		0x40
#define	DA9212_VBUCK_GPI_GPIO4		0x60

/* DA9212_REG_BUCK_ILIM (addr=0xD0) */
#define DA9212_BUCK_ILIM_SHIFT		0
#define DA9212_BUCK_ILIM_MASK		0x0F
#define DA9212_BUCK_IALARM			0x10

/* DA9212_REG_BUCKA/B_CONF (addr=0xD1/0xD2) */
#define DA9212_BUCK_MODE_SHIFT				0
#define DA9212_BUCK_MODE_MASK				0x03
#define	DA9212_BUCK_MODE_MANUAL				0x00
#define	DA9212_BUCK_MODE_PFM				0x01
#define	DA9212_BUCK_MODE_PWM				0x02
#define	DA9212_BUCK_MODE_AUTO				0x03
#define DA9212_BUCK_STARTUP_CTRL_SHIFT		2
#define DA9212_BUCK_STARTUP_CTRL_MASK		0x1C
#define DA9212_BUCK_PWR_DOWN_CTRL_SHIFT		5
#define DA9212_BUCK_PWR_DOWN_CTRL_MASK		0xE0

/* DA9212_REG_BUCK_CONF (addr=0xD3) */
#define DA9212_PHASE_SEL_A_SHIFT		0
#define DA9212_PHASE_SEL_A_MASK			0x03
#define DA9212_PHASE_SEL_B_SHIFT		2
#define DA9212_PHASE_SEL_B_MASK			0x04
#define DA9212_PH_SH_EN_A				3
#define DA9212_PH_SH_EN_A_MASK			0x08
#define DA9212_PH_SH_EN_B				4
#define DA9212_PH_SH_EN_B_MASK			0x10

/* DA9212_REG_VBUCKA/B_MAX (addr=0xD5/0xD6) */
#define DA9212_VBUCK_MAX_SHIFT		0
#define DA9212_VBUCK_MAX_MASK		0x7F

/* DA9212_REG_VBUCK_A/B (addr=0xD7/0xD8/0xD9/0xDA) */
#define DA9212_VBUCK_SHIFT			0
#define DA9212_VBUCK_MASK			0x7F
#define DA9212_VBUCK_BIAS			0
#define DA9212_BUCK_SL				0x80

/* DA9212_REG_INTERFACE (addr=0x105) */
#define DA9212_IF_BASE_ADDR_SHIFT	4
#define DA9212_IF_BASE_ADDR_MASK	0xF0

/* DA9212_REG_CONFIG_E (addr=0x147) */
#define DA9212_STAND_ALONE			0x01

#endif
