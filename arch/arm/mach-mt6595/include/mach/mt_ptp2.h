#ifndef __MT_PTP2_H__
#define __MT_PTP2_H__

extern void turn_on_FBB(void);
extern void turn_off_FBB(void);
extern void turn_on_SPARK(void);
extern void turn_off_SPARK(void);
extern void turn_on_LO(void);
extern void turn_off_LO(void);
extern u32 get_devinfo_with_index(u32 index);	/* TODO: FIXME #include "devinfo.h" */

#endif				/* __MT_PTP2_H__ */
