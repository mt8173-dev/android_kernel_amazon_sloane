#ifndef __ARCH_MT_PLATFORM_H_
#define __ARCH_MT_PLATFORM_H_


enum mt_platform_flags {
	/*flags 1..0x80 are reserved for platform-wide use */
	MT_WRAPPER_BUS = 1,
	/*flags 0x100 are device-specific */
	MT_DRIVER_FIRST = 0x100,
	/*0x10 is mt6595 64bit */
	MT_I2C_6595 = 0x10,
	MT_I2C_8127 = 0x20,
	MT_I2C_8135 = 0x40,
};
typedef u32 addr_t;

#endif				/* __ARCH_MT_PLATFORM_H_ */
