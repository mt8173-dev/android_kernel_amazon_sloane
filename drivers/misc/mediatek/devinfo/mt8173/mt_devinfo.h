#ifndef __MT_DEVINFO_H
#define __MT_DEVINFO_H

#include <linux/types.h>

/* Device Information Data */
#define ATAG_DEVINFO_DATA       0x41000804
#define ATAG_DEVINFO_DATA_SIZE  50

struct tag_devinfo_data {
	u32 devinfo_data[ATAG_DEVINFO_DATA_SIZE];	/* Device Information */
	u32 devinfo_data_size;	/* Device Information Size */
};

extern u32 get_devinfo_with_index(u32 index);
extern u32 devinfo_get_size(void);

#endif
