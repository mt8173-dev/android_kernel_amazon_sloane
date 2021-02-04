#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/hdmitx.h>

#include <linux/debugfs.h>

#include <mach/mt_typedefs.h>
#include "extd_kernel_drv.h"
#include "extd_ddp.h"
#include "ddp_debug.h"
/*#include "extd_drv.h"*/
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
#include "internal_hdmi_drv.h"
#elif defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
#include "inter_mhl_drv.h"
#include "mhl_dbg.h"
#else
#include "hdmi_drv.h"
#endif


void DBG_Init(void);
void DBG_Deinit(void);

extern void hdmi_log_enable(int enable);
extern void hdmi_cable_fake_plug_in(void);
extern void hdmi_cable_fake_plug_out(void);
/* extern void hdmi_force_resolution(int res); */
extern void hdmi_mmp_enable(int enable);
extern void hdmi_hwc_enable(int enable);


static int hdmi_frc_log_level;

int hdmi_get_frc_log_level(void)
{
	return hdmi_frc_log_level;
}

static int hdmi_ovl_pts_log_level;

int hdmi_get_ovl_pts_log_level(void)
{
	return hdmi_ovl_pts_log_level;
}

static int hdmi_rdma_pts_log_level;

int hdmi_get_rdma_pts_log_level(void)
{
	return hdmi_rdma_pts_log_level;
}

/* --------------------------------------------------------------------------- */
/* External variable declarations */
/* --------------------------------------------------------------------------- */

/* extern LCM_DRIVER *lcm_drv; */
/* --------------------------------------------------------------------------- */
/* Debug Options */
/* --------------------------------------------------------------------------- */


static char STR_HELP[] =
    "\n"
    "USAGE\n"
    "        echo [ACTION]... > hdmi\n"
    "\n" "ACTION\n" "        hdmitx:[on|off]\n" "             enable hdmi video output\n" "\n";

extern void hdmi_log_enable(int enable);
/* TODO: this is a temp debug solution */
/* extern void hdmi_cable_fake_plug_in(void); */
/* extern int hdmi_drv_init(void); */
static void process_dbg_opt(const char *opt)
{
	if (0) {

	}
#if defined(CONFIG_MTK_HDMI_SUPPORT)
	else if (0 == strncmp(opt, "on", 2)) {
		hdmi_power_on();
	} else if (0 == strncmp(opt, "off", 3)) {
		hdmi_power_off();
	} else if (0 == strncmp(opt, "suspend", 7)) {
		hdmi_suspend();
	} else if (0 == strncmp(opt, "resume", 6)) {
		hdmi_resume();
	} else if (0 == strncmp(opt, "colorbar", 8)) {

	} else if (0 == strncmp(opt, "ldooff", 6)) {

	} else if (0 == strncmp(opt, "log:", 4)) {
		if (0 == strncmp(opt + 4, "on", 2)) {
			hdmi_log_enable(true);
		} else if (0 == strncmp(opt + 4, "off", 3)) {
			hdmi_log_enable(false);
		} else {
			goto Error;
		}
	} else if (0 == strncmp(opt, "fakecablein:", 12)) {
		if (0 == strncmp(opt + 12, "enable", 6)) {
			hdmi_cable_fake_plug_in();
		} else if (0 == strncmp(opt + 12, "disable", 7)) {
			hdmi_cable_fake_plug_out();
		} else {
			goto Error;
		}
	} else if (0 == strncmp(opt, "force_res:", 10)) {
		char *p = (char *)opt + 10;
		unsigned int res;
		STR_CONVERT(&p, &res, uint, goto Error);
		hdmi_force_resolution(res);
	} else if (0 == strncmp(opt, "switch_res:", 11)) {
		char *p = (char *)opt + 11;
		unsigned int res;
		STR_CONVERT(&p, &res, uint, goto Error);
		hdmi_switch_resolution(res);
	} else if (0 == strncmp(opt, "video_fps:", 10)) {
		char *p = (char *)opt + 10;
		unsigned int fps;
		STR_CONVERT(&p, &fps, uint, goto Error);
		hdmi_set_video_fps(fps);
	} else if (0 == strncmp(opt, "frc_log:", 8)) {
		char *p = (char *)opt + 8;
		unsigned int enable;
		STR_CONVERT(&p, &enable, uint, goto Error);
		hdmi_frc_log_level = enable;
	} else if (0 == strncmp(opt, "pts_ovl_log:", 12)) {
		char *p = (char *)opt + 12;
		unsigned int enable;
		STR_CONVERT(&p, &enable, uint, goto Error);
		hdmi_ovl_pts_log_level = enable;
	} else if (0 == strncmp(opt, "pts_rdma_log:", 13)) {
		char *p = (char *)opt + 13;
		unsigned int enable;
		STR_CONVERT(&p, &enable, uint, goto Error);
		hdmi_rdma_pts_log_level = enable;
	}
#if defined(MTK_INTERNAL_HDMI_SUPPORT)
	else if ((0 == strncmp(opt, "dbgtype:", 8)) ||
		 (0 == strncmp(opt, "hdmiw:", 6)) ||
		 (0 == strncmp(opt, "hdmir:", 6)) ||
		 (0 == strncmp(opt, "cecw:", 6)) ||
		 (0 == strncmp(opt, "cecr:", 6)) ||
		 (0 == strncmp(opt, "hdcp:", 5)) ||
		 (0 == strncmp(opt, "status", 6)) ||
		 (0 == strncmp(opt, "help", 4)) || (0 == strncmp(opt, "edid", 4)))
 || (0 == strncmp(opt, "testmode:on", 11))) ||
		    (0 == strncmp(opt, "testmode:off", 12))) ||
		    (0 == strncmp(opt, "plugin", 6)) ||
		    (0 == strncmp(opt, "plugout", 7)) ||
		    (0 == strncmp(opt, "hdmidump", 8)) || (0 == strncmp(opt, "cecdump", 7)) {
		mt_hdmi_show_info(opt);
		}
#endif
#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	else if ((0 == strncmp(opt, "dbgtype:", 8)) ||
		 (0 == strncmp(opt, "w:", 2)) ||
		 (0 == strncmp(opt, "r:", 2)) ||
		 (0 == strncmp(opt, "w6:", 3)) ||
		 (0 == strncmp(opt, "r6:", 3)) ||
		 (0 == strncmp(opt, "hdcp:", 5)) ||
		 (0 == strncmp(opt, "dump6", 5)) ||
		 (0 == strncmp(opt, "base:", 5)) ||
		 (0 == strncmp(opt, "basex", 5)) ||
		 (0 == strncmp(opt, "status", 6)) ||
		 (0 == strncmp(opt, "help", 4)) ||
		 (0 == strncmp(opt, "res:", 4)) ||
		 (0 == strncmp(opt, "po", 2)) ||
		 (0 == strncmp(opt, "pn", 2)) || (0 == strncmp(opt, "edid", 4))) {
		mt_hdmi_debug_write(opt);
	}
#endif
#endif
	else if (0 == strncmp(opt, "hdmimmp:", 8)) {
		if (0 == strncmp(opt + 8, "on", 2)) {
			hdmi_mmp_enable(1);
		} else if (0 == strncmp(opt + 8, "off", 3)) {
			hdmi_mmp_enable(0);
		} else if (0 == strncmp(opt + 8, "img", 3)) {
			hdmi_mmp_enable(7);
		} else {
			goto Error;
		}
	} else if (0 == strncmp(opt, "hdmireg", 7)) {
		ext_disp_diagnose();
	} else if (0 == strncmp(opt, "enablehwc:", 10)) {
		if (0 == strncmp(opt + 10, "on", 2)) {
			hdmi_hwc_enable(1);
		} else if (0 == strncmp(opt + 10, "off", 3)) {
			hdmi_hwc_enable(0);
		}
	} else if (0 == strncmp(opt, "dumphdmidcbuf", 13)) {
		extd_disp_dump_decouple_buffer();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#if 0
	} else if (0 == strncmp(opt, "gDebugSvpHdmiAlwaysSec:", 23)) {
		char *p = (char *)opt + 23;
		STR_CONVERT(&p, &gDebugSvpHdmiAlwaysSec, uint, goto Error);
#endif
#endif
	} else {
		goto Error;
	}

	return;

 Error:
	printk("[hdmitx] parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd) {
	char *tok;

	printk("[hdmitx] %s\n", cmd);

	while ((tok = strsep(&cmd, "&&")) != NULL)
		process_dbg_opt(tok);
}

/* --------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* --------------------------------------------------------------------------- */

struct dentry *hdmitx_dbgfs = NULL;

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static char debug_buffer[4096];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
	int n = 0;
	n = strlen(debug_buffer);
	debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}


static ssize_t debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax) {
		count = debug_bufmax;
	}

	if (copy_from_user(&debug_buffer, ubuf, count)) {
		return -EFAULT;
	}

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}


static const struct file_operations debug_fops = {
.read = debug_read, .write = debug_write, .open = debug_open,};


void HDMI_DBG_Init(void) {
	hdmitx_dbgfs = debugfs_create_file("hdmi", S_IFREG | S_IRUGO, NULL, (void *)0, &debug_fops);
}


void HDMI_DBG_Deinit(void) {
	debugfs_remove(hdmitx_dbgfs);
}

#endif
