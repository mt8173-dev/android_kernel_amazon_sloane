#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT

#ifndef _MHL_DBG_H_
#define _MHL_DBG_H_

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

extern unsigned int mhl_log_on;

#define hdmiplllog         (0x1)
#define hdmidgilog         (0x2)
#define hdmitxhotpluglog   (0x4)
#define hdmitxvideolog     (0x8)
#define hdmitxaudiolog     (0x10)
#define hdmihdcplog        (0x20)
#define hdmiceclog         (0x40)
#define hdmiddclog         (0x80)
#define hdmiedidlog        (0x100)
#define hdmidrvlog         (0x200)
#define hdmicbuslog         (0x400)
#define hdmicbustxrx         (0x800)
#define hdmicbusint         (0x1000)
#define hdmicbuserr         (0x2000)

#define hdmialllog   (0xffff)

#define CBUS_INT_LOG_ON	(0x01 << 0)
#define CBUS_RXTX_LOG_ON	(0x01 << 1)
#define CBUS_DDC_LOG_ON	(0x01 << 2)
#define CBUS_MSC_REQ_LOG_ON	(0x01 << 3)
#define CBUS_MSC_RESP_LOG_ON	(0x01 << 4)

/* ////////////////////////////////////////////PLL////////////////////////////////////////////////////// */
#define MHL_PLL_LOG(fmt, arg...) \
	do { \
		if (mhl_log_on&hdmiplllog) {printk("[mhl_pll]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
	} while (0)

#define MHL_PLL_FUNC()	\
	do { \
		if (mhl_log_on&hdmiplllog) {printk("[mhl_pll] %s\n", __func__); } \
	} while (0)

/* ////////////////////////////////////////////PLUG////////////////////////////////////////////////////// */

#define MHL_PLUG_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxhotpluglog) {printk("[mhl_plug]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_PLUG_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxhotpluglog) {printk("[mhl_plug] %s\n", __func__); } \
    } while (0)


/* //////////////////////////////////////////////VIDEO//////////////////////////////////////////////////// */

#define MHL_VIDEO_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxvideolog) {printk("[mhl_video]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_VIDEO_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxvideolog) {printk("[mhl_video] %s\n", __func__); } \
    } while (0)

/* //////////////////////////////////////////////AUDIO//////////////////////////////////////////////////// */

#define MHL_AUDIO_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxaudiolog) {printk("[mhl_audio]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_AUDIO_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxaudiolog) {printk("[mhl_audio] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////HDCP/////////////////////////////////////////////////// */

#define MHL_HDCP_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmihdcplog) {printk("[mhl_hdcp]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_HDCP_FUNC()	\
    do { \
        if (mhl_log_on&hdmihdcplog) {printk("[mhl_hdcp] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////EDID////////////////////////////////////////////////// */
#define MHL_EDID_LOG(fmt, arg...) \
	do { \
		if (mhl_log_on&hdmiedidlog) {printk("[mhl_edid]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
	} while (0)

#define MHL_EDID_FUNC()	\
	do { \
		if (mhl_log_on&hdmiedidlog) {printk("[mhl_edid] %s\n", __func__); } \
	} while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */

#define MHL_DRV_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmidrvlog) {printk("[mhl_drv]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_DRV_FUNC()	\
    do { \
        if (mhl_log_on&hdmidrvlog) {printk("[mhl_drv] %s\n", __func__); } \
    } while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */
#define MHL_CBUS_LOG(fmt, arg...) \
    do { \
		if (mhl_log_on&hdmicbuslog) {printk("[mhl] "); printk(fmt, ##arg); } \
    } while (0)
#define MHL_CBUS_ERR(fmt, arg...) \
			do { \
				if (mhl_log_on&hdmicbuserr) {printk("[mhl_err] "); printk(fmt, ##arg); } \
			} while (0)
#define MHL_CBUS_TXRX(fmt, arg...) \
			do { \
				if (mhl_log_on&hdmicbustxrx) {printk("[mhl_tr] "); printk(fmt, ##arg); } \
			} while (0)
#define MHL_CBUS_INT(fmt, arg...) \
			do { \
				if (mhl_log_on&hdmicbusint) {printk("[mhl_int] "); printk(fmt, ##arg); } \
			} while (0)
#define MHL_CBUS_FUNC()	\
    do { \
        if (mhl_log_on&hdmicbuslog) {printk("[mhl] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */

#define RETNULL(cond)       if ((cond)) {MHL_DRV_LOG("return in %d\n", __LINE__); return; }
#define RETINT(cond, rslt)       if ((cond)) {MHL_DRV_LOG("return in %d\n", __LINE__); return (rslt); }
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */
extern void mt_hdmi_debug_write(char *buf);

#endif
#endif
