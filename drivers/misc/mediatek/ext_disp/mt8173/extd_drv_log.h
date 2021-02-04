#ifndef __EXTDISP_DRV_LOG_H__
#define __EXTDISP_DRV_LOG_H__
#include <linux/hdmitx.h>

/* /for kernel */
#include <linux/xlog.h>
/*#include "extd_drv.h"*/

#define HDMI_LOG(fmt, arg...) \
	do { \
		if (hdmi_log_on) \
			printk("[EXTD]#%d "fmt, __LINE__, ##arg); \
	} while (0)

#define HDMI_FUNC() \
	do { \
		if (hdmi_log_on) \
			HDMI_LOG("[EXTD] %s\n", __func__); \
	} while (0)

#define HDMI_LINE() \
	do { \
		if (hdmi_log_on) \
			printk("[EXTD]%s,%d ", __func__, __LINE__); \
	} while (0)

#define HDMI_FRC_LOG(fmt, arg...) \
	do { \
		if (hdmi_get_frc_log_level()) \
			printk("[EXTD]"fmt, ##arg); \
	} while (0)

#define HDMI_OVL_PTS_LOG(fmt, arg...) \
	do { \
		if (hdmi_get_ovl_pts_log_level()) \
			printk("[EXTD]"fmt, ##arg); \
	} while (0)

#define HDMI_RDMA_PTS_LOG(fmt, arg...) \
	do { \
		if (hdmi_get_rdma_pts_log_level()) \
			printk("[EXTD]"fmt, ##arg); \
	} while (0)


#define DISP_LOG_PRINT(level, sub_module, fmt, arg...) \
	do { \
		xlog_printk(level, "EXTD/"sub_module, fmt, ##arg); \
	} while (0)

#define LOG_PRINT(level, module, fmt, arg...) \
	do { \
		xlog_printk(level, module, fmt, ##arg); \
	} while (0)

#define DISPMSG(string, args...) printk("[EXTD]"string, ##args)	/* default on, important msg, not err */
#define DISPDBG(string, args...)	/* printk("[DISP]"string, ##args)  // default on, important msg, not err */
#define DISPERR(string, args...) printk("[EXTD][%s #%d]ERROR:"string, __func__, __LINE__, ##args)
#define DISPFUNC() printk("[EXTD]func|%s\n", __func__)	/* default on, err msg */
#define DISPDBGFUNC() DISPDBG("[EXTD]func|%s\n", __func__)	/* default on, err msg */

#define DISPCHECK(string, args...) printk("[EXTD_CHK] #%d "string, __LINE__,  ##args)

#endif				/* __DISP_DRV_LOG_H__ */
