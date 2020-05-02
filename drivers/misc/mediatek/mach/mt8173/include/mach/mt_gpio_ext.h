#ifndef _MT_GPIO_EXT_H_
#define _MT_GPIO_EXT_H_

#include <mach/mt_pmic_wrap.h>

#define GPIOEXT_WR(addr, data)   pwrap_write((U32)(long)addr, data)
#define GPIOEXT_RD(addr)         ({ \
		U32 ext_data; \
		(pwrap_read((U32)(long)addr, &ext_data) != 0) ?  -1:ext_data; })
#define GPIOEXT_SET_BITS(BIT, REG)   (GPIOEXT_WR(REG, (U32)(BIT)))
#define GPIOEXT_CLR_BITS(BIT, REG)    ({ \
		U32 ext_data; \
		int ret; \
		ret = GPIOEXT_RD(REG);\
		ext_data = ret;\
		(ret < 0) ?  -1:(GPIOEXT_WR(REG, ext_data & ~((U32)(BIT))))})

#define MT_GPIO_EXT_START MT_GPIO_BASE_MAX

#if 0				/* mark to avoid other module set gpio incorrectly, find problem in build phase */
typedef enum GPIO_PIN_EXT {
	GPIO135 = MT_GPIO_EXT_START,
	GPIO204, GPIO205, GPIO206, GPIO207, GPIO208, GPIO209, GPIO210,
	GPIO211, GPIO212, GPIO213, GPIO214, GPIO215, GPIO216, GPIO217, GPIO218,
	GPIO219, GPIO220, GPIO221, GPIO222, GPIO223, GPIO224, GPIO225, GPIO226,
	GPIO227, GPIO228, GPIO229, GPIO230, GPIO231, GPIO232, GPIO233, GPIO234,
	GPIO235, GPIO236, GPIO237, GPIO238, GPIO239, GPIO240, GPIO241, GPIO242,
	GPIO243, GPIO_MAX
} GPIO_PIN_EXT;
#endif

typedef enum GPIO_PIN_EXT1 {
	GPIOEXT0 = MT_GPIO_EXT_START,
	GPIOEXT1, GPIOEXT2, GPIOEXT3, GPIOEXT4, GPIOEXT5, GPIOEXT6, GPIOEXT7,
	GPIOEXT8, GPIOEXT9, GPIOEXT10, GPIOEXT11, GPIOEXT12, GPIOEXT13, GPIOEXT14, GPIOEXT15,
	GPIOEXT16, GPIOEXT17, GPIOEXT18, GPIOEXT19, GPIOEXT20, GPIOEXT21, GPIOEXT22, GPIOEXT23,
	GPIOEXT24, GPIOEXT25, GPIOEXT26, GPIOEXT27, GPIOEXT28, GPIOEXT29, GPIOEXT30, GPIOEXT31,
	GPIOEXT32, GPIOEXT33, GPIOEXT34, GPIOEXT35, GPIOEXT36, GPIOEXT37, GPIOEXT38, GPIOEXT39,
	GPIOEXT40, MT_GPIO_EXT_MAX
} GPIO_PIN_EXT1;
#define MT_GPIO_MAX_PIN MT_GPIO_EXT_MAX

/*----------------------------------------------------------------------------*/
typedef struct {
	unsigned short val;
	unsigned short set;
	unsigned short rst;
	unsigned short _align;
} EXT_VAL_REGS;
/*----------------------------------------------------------------------------*/
typedef struct {
	EXT_VAL_REGS dir[4];	/*0x0000 ~ 0x001F: 32 bytes */
	EXT_VAL_REGS pullen[4];	/*0x0020 ~ 0x003F: 32 bytes */
	EXT_VAL_REGS pullsel[4];	/*0x0040 ~ 0x005F: 32 bytes */
	EXT_VAL_REGS dinv[4];	/*0x0060 ~ 0x007F: 32 bytes */
	EXT_VAL_REGS dout[4];	/*0x0080 ~ 0x009F: 32 bytes */
	EXT_VAL_REGS din[4];	/*0x00A0 ~ 0x00BF: 32 bytes */
	EXT_VAL_REGS mode[10];	/*0x00C0 ~ 0x010F: 80 bytes */
} GPIOEXT_REGS;

#endif				/* _MT_GPIO_EXT_H_ */
