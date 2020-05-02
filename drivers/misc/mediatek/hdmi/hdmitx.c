/*****************************************************************************/
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307       */
/* USA.                                                                      */
/*                                                                           */
/*****************************************************************************/
#if defined(CONFIG_MTK_HDMI_SUPPORT)
#define TMFL_TDA19989

#define _tx_c_
#include <generated/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#include <mach/m4u.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_boot.h>

#include "linux/hdmitx.h"
#include "linux/mtkfb.h"
#include "../video/mtkfb_info.h"

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
#include "internal_hdmi_drv.h"
#elif defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
#include "inter_mhl_drv.h"
#else
#include "hdmi_drv.h"
#endif
#include "hdmi_utils.h"

#include "dpi_reg.h"
#include "mach/eint.h"
#include "mach/irqs.h"

#include "disp_drv_platform.h"
#include "ddp_reg.h"

#include "dpi1_drv.h"
#include <ddp_dpfd.h>
#include "disp_drv.h"


#ifdef I2C_DBG
#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_local.h"
#endif

#ifdef CONFIG_SLIMPORT_ANX3618
#include <misc/dongle_hdmi.h>
#endif /*CONFIG_SLIMPORT_ANX3618*/
/* Need to be fixed */
/* HDMI Buffer, OVL Buffer were still be used at user space after buffer released at hdni_drv_deinit */
/* User sacpce will call m4u ioctl function "cache_naintain" during releasing buffers, it causes system crash */
/* Define HDMI_BUFFER_KEEP_USE will use temp solution flow to avoid this issue, still need to check DDP flow in feature. */
#define HDMI_BUFFER_KEEP_USE
#define ENABLE_MULTI_DISPLAY
#define ENABLE_SECURE_HDMI
#define ENABLE_HDMI_MTK_FENCE

#ifdef ENABLE_SECURE_HDMI
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#ifdef ENABLE_HDMI_MTK_FENCE
#include <sw_sync.h>
spinlock_t hdmi_lock;
DEFINE_SPINLOCK(hdmi_lock);
int hdmi_rdma_address_config(bool enable, hdmi_video_buffer_info buffer_info);
#endif


#define HDMI_DEVNAME "hdmitx"

#undef OUTREG32
#define OUTREG32(x, y) {/*pr_info("[hdmi]write 0x%08x to 0x%08x\n", (y), (x)); */__OUTREG32((x), (y))}
#define __OUTREG32(x, y) {*(unsigned int *)(x) = (y); }

#define RETIF(cond, rslt)       { if ((cond)) {HDMI_LOG_DBG("return in %d\n", __LINE__); return (rslt); } }
#define RET_VOID_IF(cond)       { if ((cond)) {HDMI_LOG_DBG("return in %d\n", __LINE__); return; } }
#define RETIF_NOLOG(cond, rslt)       if ((cond)) {return (rslt); }
#define RET_VOID_IF_NOLOG(cond)       if ((cond)) {return; }
#define RETIFNOT(cond, rslt)    { if (!(cond)) {HDMI_LOG_DBG("return in %d\n", __LINE__); return (rslt); } }

#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
#define HDMI_DPI(suffix)        DPI1 ## suffix
#define HMID_DEST_DPI			DISP_MODULE_DPI1
static int hdmi_bpp = 3;
#else
#define HDMI_DPI(suffix)        DPI  ## suffix
#define HMID_DEST_DPI			DISP_MODULE_DPI0
static int hdmi_bpp = 3;
#endif


#ifdef ENABLE_MULTI_DISPLAY
/* static int wdma1_bpp = 2; */
#else
static int wdma1_bpp = 3;
static int rmda1_bpp = 3;
#endif


#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

extern const HDMI_DRIVER *HDMI_GetDriver(void);
extern void HDMI_DBG_Init(void);

extern UINT32 DISP_GetScreenHeight(void);
extern UINT32 DISP_GetScreenWidth(void);
extern BOOL DISP_IsVideoMode(void);
extern int disp_lock_mutex(void);
extern int disp_unlock_mutex(int id);
extern unsigned int hdmi_audio_event;


#ifdef ENABLE_SECURE_HDMI
extern void disp_register_intr(unsigned int irq, unsigned int secure);
extern KREE_SESSION_HANDLE ddp_session_handle(void);
/* extern unsigned int gRdma1Secure; */

unsigned int gRDMASecure = 0;
#endif

static size_t hdmi_log_on = 1;
static size_t hdmi_log_lv = HDMI_LOG_LEVEL_ERR;
static unsigned long hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
static struct switch_dev hdmi_switch_data;
static struct switch_dev hdmi_audio_switch_data;
#ifdef ENABLE_MULTI_DISPLAY
static struct switch_dev hdmires_switch_data;
#endif

HDMI_PARAMS _s_hdmi_params = { 0 };

HDMI_PARAMS *hdmi_params = &_s_hdmi_params;
static HDMI_DRIVER *hdmi_drv;

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
static size_t hdmi_colorspace = HDMI_RGB;
int flag_resolution_interlace(HDMI_VIDEO_RESOLUTION resolution)
{
	if ((resolution == HDMI_VIDEO_1920x1080i_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i_50Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz))
		return true;
	else
		return false;
}

int flag_resolution_3d(HDMI_VIDEO_RESOLUTION resolution)
{
	if ((resolution == HDMI_VIDEO_1280x720p3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1280x720p3d_50Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080p3d_24Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080p3d_23Hz))
		return true;
	else
		return false;
}
#endif

void hdmi_log_enable(int enable)
{
	pr_info("hdmi log %s\n", enable ? "enabled" : "disabled");
	hdmi_log_on = enable;
	hdmi_drv->log_enable(enable);
}

void hdmi_log_level(int loglv)
{
	hdmi_log_lv = loglv;
	HDMI_LOG_DBG("Set HDMI log level = %d\n", hdmi_log_lv);
}

static DEFINE_SEMAPHORE(hdmi_update_mutex);
typedef struct {
	bool is_reconfig_needed;	/* whether need to reset HDMI memory */
	bool is_enabled;	/* whether HDMI is enabled or disabled by user */
	bool is_force_disable;	/* used for camera scenario. */
	bool is_clock_on;	/* DPI is running or not */
	bool is_rdma_clock_on; /*rdma1 clock is running or not*/
	atomic_t state;		/* HDMI_POWER_STATE state */
	int lcm_width;		/* LCD write buffer width */
	int lcm_height;		/* LCD write buffer height */
	bool lcm_is_video_mode;
	int hdmi_width;		/* DPI read buffer width */
	int hdmi_height;	/* DPI read buffer height */
	HDMI_VIDEO_RESOLUTION output_video_resolution;
	HDMI_AUDIO_FORMAT output_audio_format;
	int orientation;/* MDP's orientation, 0 means 0 degree, 1 means 90 degree, 2 means 180 degree, 3 means 270 */
	HDMI_OUTPUT_MODE output_mode;
	int scaling_factor;
	int is_security_output;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	int is_svp_output;	/* temporary solution for hdmi svp p1, always mute hdmi for svp */
#endif
} _t_hdmi_context;

typedef enum {
	insert_new,
	reg_configed,
	reg_updated,
	buf_read_done
} BUFFER_STATE;

typedef struct _hdmi_buffer_list {
	hdmi_video_buffer_info buffer_info;
	BUFFER_STATE buf_state;
#ifdef ENABLE_HDMI_MTK_FENCE
	unsigned int idx;	/* /fence count */
	/* int fencefd;          ///fence fd */
	struct sync_fence *fence;
	/* struct ion_handle *hnd; */
	/* unsigned int mva; */
	/* unsigned int va; */
#endif
	struct list_head list;
} hdmi_video_buffer_list;

/* static struct list_head hdmi_video_mode_buffer_list; */
DEFINE_SEMAPHORE(hdmi_video_mode_mutex);
/* static atomic_t hdmi_video_mode_flag = ATOMIC_INIT(0); */
/* #define IS_HDMI_IN_VIDEO_MODE()        atomic_read(&hdmi_video_mode_flag) */
/* #define SET_HDMI_TO_VIDEO_MODE()       atomic_set(&hdmi_video_mode_flag, 1) */
/* #define SET_HDMI_LEAVE_VIDEO_MODE()    atomic_set(&hdmi_video_mode_flag, 0)  */
/* static wait_queue_head_t hdmi_video_mode_wq; */
/* static atomic_t hdmi_video_mode_event = ATOMIC_INIT(0); */
/* static atomic_t hdmi_video_mode_dpi_change_address = ATOMIC_INIT(0); */
#define IS_HDMI_VIDEO_MODE_DPI_IN_CHANGING_ADDRESS()    atomic_read(&hdmi_video_mode_dpi_change_address)
#define SET_HDMI_VIDEO_MODE_DPI_CHANGE_ADDRESS()        atomic_set(&hdmi_video_mode_dpi_change_address, 1)
#define SET_HDMI_VIDEO_MODE_DPI_CHANGE_ADDRESS_DONE()   atomic_set(&hdmi_video_mode_dpi_change_address, 0)

static _t_hdmi_context hdmi_context;
static _t_hdmi_context *p = &hdmi_context;
#ifdef ENABLE_HDMI_MTK_FENCE
static struct list_head HDMI_Buffer_List;
#endif

#define IS_HDMI_ON()			(HDMI_POWER_STATE_ON == atomic_read(&p->state))
#define IS_HDMI_OFF()			(HDMI_POWER_STATE_OFF == atomic_read(&p->state))
#define IS_HDMI_STANDBY()	    (HDMI_POWER_STATE_STANDBY == atomic_read(&p->state))

#define IS_HDMI_NOT_ON()		(HDMI_POWER_STATE_ON != atomic_read(&p->state))
#define IS_HDMI_NOT_OFF()		(HDMI_POWER_STATE_OFF != atomic_read(&p->state))
#define IS_HDMI_NOT_STANDBY()	(HDMI_POWER_STATE_STANDBY != atomic_read(&p->state))

#define SET_HDMI_ON()	        atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()	        atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY()	    atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)

int hdmi_allocate_hdmi_buffer(void);
int hdmi_free_hdmi_buffer(void);

static int dp_mutex_src = -1, dp_mutex_dst = -1;
static unsigned int /*temp_mva_r, */ temp_mva_w, temp_va, hdmi_va, hdmi_mva_r /*, hdmi_mva_w */;


static dev_t hdmi_devno;
static struct cdev *hdmi_cdev;
static struct class *hdmi_class;


/* --------------------------------------------------------------------------- */
/* Information Dump Routines */
/* --------------------------------------------------------------------------- */

/* static int hdmi_default_width = 1280; */
/* static int hdmi_default_height = 720; */

#define ENABLE_HDMI_FPS_CONTROL_LOG 0
#if ENABLE_HDMI_FPS_CONTROL_LOG
static unsigned int hdmi_fps_control_fps_wdma0;
static unsigned long hdmi_fps_control_time_base_wdma0;
static unsigned int hdmi_fps_control_fps_wdma1;
static unsigned long hdmi_fps_control_time_base_wdma1;
static unsigned int hdmi_fps_control_fps_rdma1;
static unsigned long hdmi_fps_control_time_base_rdma1;
#endif

typedef enum {
	HDMI_OVERLAY_STATUS_STOPPED,
	HDMI_OVERLAY_STATUS_STOPPING,
	HDMI_OVERLAY_STATUS_STARTING,
	HDMI_OVERLAY_STATUS_STARTED,
} HDMI_OVERLAY_STATUS;

static unsigned int hdmi_fps_control_dpi;
static unsigned int hdmi_fps_control_overlay;
/* static HDMI_OVERLAY_STATUS hdmi_overlay_status = HDMI_OVERLAY_STATUS_STOPPED; */
static unsigned int hdmi_rdma_switch_count;

static int hdmi_buffer_write_id;
static int hdmi_buffer_read_id;
static int hdmi_buffer_read_id_tmp;
static int hdmi_buffer_lcdw_id;
/* static int hdmi_buffer_lcdw_id_tmp; */

static DPI_POLARITY clk_pol, de_pol, hsync_pol, vsync_pol;
static unsigned int dpi_clk_div, dpi_clk_duty, hsync_pulse_width, hsync_back_porch,
    hsync_front_porch, vsync_pulse_width, vsync_back_porch, vsync_front_porch,
    intermediat_buffer_num;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
static BOOL fg3DFrame, fgInterlace;
static unsigned int hdmi_res = HDMI_VIDEO_1280x720p_50Hz;
#endif

static HDMI_COLOR_ORDER rgb_order;


/* static wait_queue_head_t hdmi_update_wq; */
/* static atomic_t hdmi_update_event = ATOMIC_INIT(0); */

#ifndef ENABLE_MULTI_DISPLAY
static struct task_struct *hdmi_overlay_config_task;
#endif
/* static wait_queue_head_t hdmi_overlay_config_wq; */
/* static atomic_t hdmi_overlay_config_event = ATOMIC_INIT(0); */

/* rdma_config_task is used in normal mirror mode, or Extension mode with Fence */
#if (!defined(ENABLE_MULTI_DISPLAY) || defined(ENABLE_HDMI_MTK_FENCE))
static struct task_struct *hdmi_rdma_config_task;
#endif

static struct task_struct *hdmi_rdma_update_task;

static wait_queue_head_t hdmi_rdma_config_wq;
static atomic_t hdmi_rdma_config_event = ATOMIC_INIT(0);

static wait_queue_head_t hdmi_rdma_update_wq;
static atomic_t hdmi_rdma_update_event = ATOMIC_INIT(0);

/* static wait_queue_head_t reg_update_wq; */
/* static atomic_t reg_update_event = ATOMIC_INIT(0); */

/* static wait_queue_head_t dst_reg_update_wq; */
/* static atomic_t dst_reg_update_event = ATOMIC_INIT(0); */

static unsigned int hdmi_resolution_param_table[][3] = {
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)
	{720, 480, 60},
	{720, 576, 50},
	{1280, 720, 60},
	{1280, 720, 50},

	{1920, 1080, 60},
	{1920, 1080, 50},
	{1920, 1080, 30},
	{1920, 1080, 25},
	{1920, 1080, 24},
	{1920, 1080, 23},
	{1920, 1080, 29},

	{1920, 1080, 60},
	{1920, 1080, 50},
#elif defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	{720, 480, 60},
	{720, 576, 50},
	{1280, 720, 60},
	{1280, 720, 50},

	{1920, 1080, 60},
	{1920, 1080, 50},
	{1920, 1080, 30},
	{1920, 1080, 25},
	{1920, 1080, 24},
	{1920, 1080, 23},
	{1920, 1080, 29},

	{1920, 1080, 60},
	{1920, 1080, 50},
#else
	{720, 480, 60},
	{1280, 720, 60},
	{1920, 1080, 30},
#endif
};

#define ENABLE_HDMI_BUFFER_LOG 1
#if ENABLE_HDMI_BUFFER_LOG
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...) \
	do { \
	if (enable_hdmi_buffer_log) {	\
		pr_info("[hdmi_buffer] ");	\
		pr_info(fmt, ##arg);	\
	}	\
	} while (0)
#else
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...)
#endif

int hdmi_rdma_buffer_switch_mode = 0;	/* 0: switch in rdma1 frame done interrupt, 1: switch after DDPK_Bitblt done */
static int hdmi_buffer_num;
static int *hdmi_buffer_available;
static int *hdmi_buffer_queue;
static int hdmi_buffer_end;
static int hdmi_buffer_start;
static int hdmi_buffer_fill_count;
static DEFINE_SEMAPHORE(hdmi_buffer_mutex);

#ifdef ENABLE_HDMI_MTK_FENCE
#define FENCE_STEP_COUNTER 1
DEFINE_MUTEX(FenceMutex);
static atomic_t timeline_counter = ATOMIC_INIT(0);
static atomic_t fence_counter = ATOMIC_INIT(0);
static struct sw_sync_timeline *hdmi_timeline;
/**
 * fence_counter records counter of last fence created.
 * fence_counter will be increased by FENCE_STEP_COUNTER
 */
static unsigned int hdmi_get_fence_counter(void)
{
	return atomic_add_return(FENCE_STEP_COUNTER, &fence_counter);
}


static struct sw_sync_timeline *hdmi_create_timeline(void)
{
	char name[32];
	const char *prefix = "hdmi_timeline";
	sprintf(name, "%s", prefix);

	/* hdmi_timeline = timeline_create(name); */
	hdmi_timeline = sw_sync_timeline_create(name);

	if (hdmi_timeline == NULL)
		HDMI_LOG_ERR("error: cannot create timeline!\n");
	else
		HDMI_LOG_DBG("Timeline name=%s created!", name);


	return hdmi_timeline;
}

void hdmi_destroy_timeline(struct sw_sync_timeline *obj)
{
	sync_timeline_destroy(&obj->obj);
}

static int hdmi_create_fence(int *pfence, unsigned int *pvalue)
{
	/* int fenceFd = MTK_HDMI_NO_FENCE_FD; */
	/* struct fence_data data; */
	const char *prefix = "hdmi_fence";
    unsigned int timeline_cnt = atomic_read(&timeline_counter);
	unsigned int value;
	char name[30];

	*pfence = MTK_HDMI_NO_FENCE_FD;

	spin_lock_bh(&hdmi_lock);
	/* data.value = hdmi_get_fence_counter(); */
	value = hdmi_get_fence_counter();
	spin_unlock_bh(&hdmi_lock);
	/* sprintf(data.name, "%s-%d", prefix,  data.value); */
	sprintf(name, "%s-%d", prefix, value);

	if (hdmi_timeline != NULL) {
		int fd = get_unused_fd();
		struct sync_pt *pt;
		struct sync_fence *fence;
		if (fd < 0) {
			HDMI_LOG_ERR("could not get a file description!\n");
			return fd;
		}

		pt = sw_sync_pt_create(hdmi_timeline, value);
		if (pt == NULL) {
			HDMI_LOG_ERR("could not create sync point!\n");
			put_unused_fd(fd);
			return -ENOMEM;
		}

		name[sizeof(name) - 1] = '\0';
		fence = sync_fence_create(name, pt);
		if (fence == NULL) {
			HDMI_LOG_ERR("could not create fence!\n");
			sync_pt_free(pt);
			put_unused_fd(fd);
			return -ENOMEM;
		}

		sync_fence_install(fence, fd);
		/* fenceFd = fd; */

		*pfence = fd;
		*pvalue = value;
	} else {
		HDMI_LOG_ERR("error: there is no Timeline to create Fence!\n");
		return -1;
	}
	if ((value - timeline_cnt) >= 5)
		HDMI_LOG_ERR("hdmi create fence max=%d, timeline=%d\n", value, timeline_cnt);

	return 0;
}


static void hdmi_release_fence(void)
{
	int inc = atomic_read(&fence_counter) - atomic_read(&timeline_counter);

	if (inc <= 0)
		return;

	if (hdmi_timeline != NULL) {
		/* timeline_inc(hdmi_timeline, inc); */
		sw_sync_timeline_inc(hdmi_timeline, inc);
	}

	atomic_add(inc, &timeline_counter);
}


/**
 * timeline_counter records counter of this timeline.
 * It should be always posterior to fence_counter when enable is true, otherwise
 * they're equaled
 * timeline_counter will step forward and present current hw used buff counter
 * NOTICE:
 *     Frame dropping maybe happen, we has no cache FIFO now!
 *     When a new buffer is coming, all prior to it will be released
 *     Buf will be released immediately if ovl_layer is disabled
 */
unsigned int hdmi_timeline_inc(void)
{
	unsigned int fence_cnt, timeline_cnt, inc;

	spin_lock_bh(&hdmi_lock);
	fence_cnt = atomic_read(&fence_counter);
	timeline_cnt = atomic_read(&timeline_counter);
	inc = fence_cnt - timeline_cnt;
	spin_unlock_bh(&hdmi_lock);

	if (inc < 0 || inc > 5) {
		HDMI_LOG_ERR("fence error: inc=%d, fence_cnt=%d, timeline_cnt=%d!\n", inc, fence_cnt,
			 timeline_cnt);
		inc = 0;
	}

	spin_lock_bh(&hdmi_lock);
	atomic_add(1, &timeline_counter);
	spin_unlock_bh(&hdmi_lock);
	return atomic_read(&timeline_counter);
}

/**
 * step forward timeline
 * all fence(sync_point) will be signaled prior to it's counter
 * refer to {@link sw_sync_timeline_inc}
 */
static void hdmi_signal_fence(void)
{
	unsigned inc = 0;
	if (hdmi_timeline != NULL) {
		inc = 1;	/* /hdmi_get_timeline_counter_inc(); */
		/* timeline_inc(hdmi_timeline, inc); */
		sw_sync_timeline_inc(hdmi_timeline, inc);
	} else {
		HDMI_LOG_ERR("no Timeline to inc tl %d, fd %d\n", atomic_read(&timeline_counter),
			 atomic_read(&fence_counter));
	}
}

static void hdmi_sync_init(void)
{
	/* /spin_lock_init(&hdmi_lock); */
	hdmi_create_timeline();
	/* Reset all counter to 0 */
	atomic_set(&timeline_counter, 0);
	atomic_set(&fence_counter, 0);
}

static void hdmi_sync_destroy(void)
{
	if (hdmi_timeline != NULL) {
		HDMI_LOG_DBG("destroy timeline %s:%d\n", hdmi_timeline->obj.name,
			 hdmi_timeline->value);
		hdmi_destroy_timeline(hdmi_timeline);
		hdmi_timeline = NULL;
	}
	/* Reset all counter to 0 */
	atomic_set(&timeline_counter, 0);
	atomic_set(&fence_counter, 0);
}

#endif

static void hdmi_buffer_init(int num)
{
	int i;

	if (down_interruptible(&hdmi_buffer_mutex)) {
		HDMI_LOG_ERR("Can't get semaphore in %s()\n", __func__);
		return;
	}

	HDMI_FUNC();

	hdmi_buffer_num = num;
	hdmi_buffer_start = 0;
	hdmi_buffer_end = 0;
	hdmi_buffer_fill_count = 0;

	hdmi_buffer_write_id = 0;
	hdmi_buffer_read_id = 0;
	hdmi_buffer_lcdw_id = 0;
	/* hdmi_buffer_lcdr_id = 0; */

	hdmi_buffer_available = (int *)vmalloc(hdmi_buffer_num * sizeof(int));
	hdmi_buffer_queue = (int *)vmalloc(hdmi_buffer_num * sizeof(int));

	for (i = 0; i < hdmi_buffer_num; i++) {
		hdmi_buffer_available[i] = 1;
		hdmi_buffer_queue[i] = -1;
	}

	up(&hdmi_buffer_mutex);
}

static void hdmi_buffer_deinit(void)
{
	if (down_interruptible(&hdmi_buffer_mutex)) {
		HDMI_LOG_ERR("Can't get semaphore in %s()\n", __func__);
		return;
	}

	HDMI_FUNC();

	hdmi_buffer_start = 0;
	hdmi_buffer_end = 0;
	hdmi_buffer_fill_count = 0;

	if (hdmi_buffer_available) {
		vfree((const void *)hdmi_buffer_available);
		hdmi_buffer_available = 0;
	}

	if (hdmi_buffer_queue) {
		vfree((const void *)hdmi_buffer_queue);
		hdmi_buffer_queue = 0;
	}

	up(&hdmi_buffer_mutex);
}

static void hdmi_dump_buffer_queue(void)
{
	HDMI_BUFFER_LOG
	    ("[hdmi] available={%d,%d,%d,%d} queue={%d,%d,%d,%d}, {start,end}={%d,%d} count=%d\n",
	     hdmi_buffer_available[0], hdmi_buffer_available[1], hdmi_buffer_available[2],
	     hdmi_buffer_available[3], hdmi_buffer_queue[hdmi_buffer_start],
	     hdmi_buffer_queue[(hdmi_buffer_start + 1) % hdmi_buffer_num],
	     hdmi_buffer_queue[(hdmi_buffer_start + 2) % hdmi_buffer_num],
	     hdmi_buffer_queue[(hdmi_buffer_start + 3) % hdmi_buffer_num], hdmi_buffer_start,
	     hdmi_buffer_end, hdmi_buffer_fill_count);
}

static int hdmi_is_buffer_empty(void)
{
	return hdmi_buffer_fill_count == 0;
}

static void hdmi_release_buffer(int index)
{
	/* down_interruptible(&hdmi_buffer_mutex); */

	HDMI_BUFFER_LOG("[hdmi] hdmi_release_buffer: %d\n", index);
	hdmi_buffer_available[index] = 1;
	hdmi_dump_buffer_queue();

	/* up(&hdmi_buffer_mutex); */
}

static int hdmi_acquire_buffer(void)
{
	int index = -1;

	if (down_interruptible(&hdmi_buffer_mutex)) {
		HDMI_LOG_ERR("Can't get semaphore in %s()\n", __func__);
		return -1;
	}

	if (!hdmi_is_buffer_empty()) {
		index = hdmi_buffer_queue[hdmi_buffer_start];

		HDMI_BUFFER_LOG("[hdmi] hdmi_acquire_buffer: %d\n", index);

		hdmi_buffer_queue[hdmi_buffer_start] = -1;
		hdmi_buffer_start = (hdmi_buffer_start + 1) % hdmi_buffer_num;

		hdmi_buffer_fill_count--;

		hdmi_dump_buffer_queue();
	}

	up(&hdmi_buffer_mutex);
	return index;
}

#ifndef ENABLE_MULTI_DISPLAY
static int hdmi_dequeue_buffer(void)
{
	int i;
	if (down_interruptible(&hdmi_buffer_mutex)) {
		HDMI_LOG_ERR("Can't get semaphore in %s()\n", __func__);
		return -1;
	}

	for (i = 0; i < hdmi_buffer_num; i++) {
		if (hdmi_buffer_available[i]) {
			hdmi_buffer_available[i] = 0;
			HDMI_BUFFER_LOG("[hdmi] hdmi_dequeue_buffer: %d\n", i);

			hdmi_dump_buffer_queue();
			up(&hdmi_buffer_mutex);
			return i;
		}
	}

	/* if no available buffer, return the last buffer in queue */
	if (!hdmi_is_buffer_empty()) {
		int index = hdmi_buffer_queue[hdmi_buffer_end];
		HDMI_BUFFER_LOG("[hdmi] hdmi_dequeue_buffer(last): %d\n", index);

		hdmi_buffer_queue[hdmi_buffer_end] = -1;
		hdmi_buffer_end = (hdmi_buffer_end + (hdmi_buffer_num - 1)) % hdmi_buffer_num;
		hdmi_buffer_fill_count--;

		hdmi_dump_buffer_queue();
		up(&hdmi_buffer_mutex);
		return index;
	}

	up(&hdmi_buffer_mutex);
	HDMI_LOG_ERR("no available buffer\n");
	return -1;
}

static void hdmi_enqueue_buffer(int index)
{
	if (down_interruptible(&hdmi_buffer_mutex)) {
		HDMI_LOG_ERR("Can't get semaphore in %s()\n", __func__);
		hdmi_release_buffer(index);
		return;
	}

	HDMI_BUFFER_LOG("[hdmi] hdmi_enqueue_buffer: %d\n", index);

	/* hdmi_buffer_available[index] = 1; */
	hdmi_buffer_end = (hdmi_buffer_start + hdmi_buffer_fill_count) % hdmi_buffer_num;
	hdmi_buffer_queue[hdmi_buffer_end] = index;

	if (hdmi_buffer_fill_count == hdmi_buffer_num)
		hdmi_buffer_start = (hdmi_buffer_start + 1) % hdmi_buffer_num;
	else
		hdmi_buffer_fill_count++;

	hdmi_dump_buffer_queue();

	up(&hdmi_buffer_mutex);
}
#endif

#if ENABLE_HDMI_FPS_CONTROL_LOG
static unsigned long get_current_time_us(void)
{
	struct timeval t;
	do_gettimeofday(&t);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}
#endif

static void hdmi_udelay(unsigned int us)
{
	udelay(us);
}

static void hdmi_mdelay(unsigned int ms)
{
	msleep(ms);
}

static unsigned int hdmi_get_width(HDMI_VIDEO_RESOLUTION r)
{
	ASSERT(r < HDMI_VIDEO_RESOLUTION_NUM);
	return hdmi_resolution_param_table[r][0];
}

static unsigned int hdmi_get_height(HDMI_VIDEO_RESOLUTION r)
{
	ASSERT(r < HDMI_VIDEO_RESOLUTION_NUM);
	return hdmi_resolution_param_table[r][1];
}


static atomic_t hdmi_fake_in = ATOMIC_INIT(false);
#define IS_HDMI_FAKE_PLUG_IN()  (true == atomic_read(&hdmi_fake_in))
#define SET_HDMI_FAKE_PLUG_IN() (atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()     (atomic_set(&hdmi_fake_in, false))

/* For Debugfs */
void hdmi_cable_fake_plug_in(void)
{
	SET_HDMI_FAKE_PLUG_IN();
	HDMI_LOG_INFO("[HDMIFake]Cable Plug In\n");
	if (p->is_force_disable == false) {
		if (IS_HDMI_STANDBY()) {
			hdmi_resume();
			/* msleep(1000); */
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			switch_set_state(&hdmi_audio_switch_data, 1);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		}
	}
}

/* For Debugfs */
void hdmi_cable_fake_plug_out(void)
{
	SET_HDMI_NOT_FAKE();
	HDMI_LOG_INFO("[HDMIFake]Disable\n");
	if (p->is_force_disable == false) {
		if (IS_HDMI_ON()) {
			if (hdmi_drv->get_state() != HDMI_STATE_ACTIVE) {
				hdmi_suspend();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, 0);
			}
		}
	}
}

void hdmi_set_mode(unsigned char ucMode)
{
	HDMI_FUNC();

	hdmi_drv->set_mode(ucMode);

	return;
}

void hdmi_reg_dump(void)
{
	hdmi_drv->dump();
}

void hdmi_read_reg(unsigned char u8Reg)
{
}

void hdmi_write_reg(unsigned char u8Reg, unsigned char u8Data)
{

}





/* Will be called in LCD Interrupt handler to check whether HDMI is actived */
bool is_hdmi_active(void)
{
	return IS_HDMI_ON();
}

int get_hdmi_dev_info(HDMI_QUERY_TYPE type)
{
	switch (type) {
	case HDMI_CHARGE_CURRENT:
		{
			if ((p->is_enabled == false)
			    || hdmi_params->cabletype == HDMI_CABLE) {
				return 0;
			} else if (hdmi_params->cabletype == MHL_CABLE)
				return 500;
			else if (hdmi_params->cabletype == MHL_2_CABLE)
				return 900;

		}
	default:
		return 0;
	}

}

void hdmi_wdma1_done(void)
{
	return;
}

void hdmi_wdma0_done(void)
{
	return;
}

void hdmi_rdma1_done(void)
{
	return;
}


/* Switch LCD write buffer, will be called in LCD Interrupt handler */
void hdmi_source_buffer_switch(void)
{
	return;
}

void hdmi_set_rdma_address(int bufferIndex)
{
	unsigned int hdmiSourceAddr;
	disp_path_get_mutex_(dp_mutex_dst);
	hdmi_buffer_read_id_tmp = bufferIndex;
	hdmiSourceAddr = hdmi_mva_r + p->hdmi_width * p->hdmi_height * hdmi_bpp * bufferIndex;
	if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution) ||
		(HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution)) {
		/* top & bottom exchange for shadow register update in next vsync */
		if (DPI1_IS_TOP_FIELD())
			hdmiSourceAddr += p->hdmi_width * hdmi_bpp;
	}
	DISP_REG_SET(0x1000 + DISP_REG_RDMA_MEM_START_ADDR, hdmiSourceAddr);
	disp_path_release_mutex_(dp_mutex_dst);

	HDMI_BUFFER_LOG("rdma1 address set done, hdmi_r=%d\n", bufferIndex);
}

void hdmi_rdma_buffer_switch(void)
{
	int acquired = hdmi_acquire_buffer();

	if (acquired != -1) {
		unsigned int hdmiSourceAddr;

		disp_path_get_mutex_(dp_mutex_dst);

		if (hdmi_buffer_read_id != hdmi_buffer_read_id_tmp) {
			/* if hdmi_buffer_read_id_tmp has not be writen to working register, drop it */
			HDMI_BUFFER_LOG("drop %d\n", hdmi_buffer_read_id_tmp);
			hdmi_release_buffer(hdmi_buffer_read_id_tmp);
			return;
		}

		hdmi_buffer_read_id_tmp = acquired;
		hdmiSourceAddr =
		    hdmi_mva_r +
		    p->hdmi_width * p->hdmi_height * hdmi_bpp * hdmi_buffer_read_id_tmp;
		if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution)
		    || (HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution)) {
			if (DPI1_IS_TOP_FIELD())
				hdmiSourceAddr += p->hdmi_width * hdmi_bpp;
		}
		DISP_REG_SET(0x1000 + DISP_REG_RDMA_MEM_START_ADDR, hdmiSourceAddr);
		disp_path_release_mutex_(dp_mutex_dst);

		HDMI_BUFFER_LOG("rdma1 address set done, hdmi_r=%d\n", hdmi_buffer_read_id_tmp);


		hdmi_rdma_switch_count++;
	} else {
		HDMI_BUFFER_LOG("no available buffer, wait buffer...\n");
		hdmi_set_rdma_address(hdmi_buffer_read_id_tmp);
	}
}

/* Switch DPI read buffer, will be called in DPI Interrupt handler */
void hdmi_update_buffer_switch(void)
{
	/* HDMI_LOG("DPI read buffer:%d\n", hdmi_buffer_read_id); */

	RET_VOID_IF_NOLOG(IS_HDMI_NOT_ON());
	RET_VOID_IF_NOLOG(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS);

		atomic_set(&hdmi_rdma_config_event, 1);
		wake_up_interruptible(&hdmi_rdma_config_wq);
}

void hdmi_buffer_to_RDMA(void)
{
	if (!list_empty(&HDMI_Buffer_List)) {
		hdmi_video_buffer_list *pBuffList = NULL;
		spin_lock_bh(&hdmi_lock);
		pBuffList = list_first_entry(&HDMI_Buffer_List, hdmi_video_buffer_list, list);

		while (pBuffList->buf_state != insert_new) {
			if (list_is_last(&pBuffList->list, &HDMI_Buffer_List))
				break;
			pBuffList = list_entry(pBuffList->list.next, hdmi_video_buffer_list, list);
		}

		spin_unlock_bh(&hdmi_lock);

		if ((pBuffList == NULL) || (pBuffList->buf_state != insert_new)
			|| (sync_fence_wait(pBuffList->fence, 1000) < 0)
			|| (hdmi_rdma_address_config(true, pBuffList->buffer_info) < 0)) {
			if ((pBuffList != NULL) && (pBuffList->buf_state == insert_new))
				pBuffList->buf_state = buf_read_done;
		} else {
			spin_lock_bh(&hdmi_lock);
			pBuffList->buf_state = reg_configed;
			spin_unlock_bh(&hdmi_lock);
		}
	} else
		HDMI_LOG_DBG("rdma config buffer is NULL\n");
}

void hdmi_buffer_state_update(void)
{
	int buf_sequence = 0;
	hdmi_video_buffer_list *pUpdateList = NULL;
	int remove_buffer_cnt = 0;

	if (!list_empty(&HDMI_Buffer_List)) {
		hdmi_video_buffer_list *pBuffList = NULL;

		spin_lock_bh(&hdmi_lock);
		pBuffList = list_first_entry(&HDMI_Buffer_List, hdmi_video_buffer_list, list);

		while (pBuffList) {
			if (pBuffList->buf_state == insert_new)
				break;

			else if (pBuffList->buf_state == reg_configed) {
				buf_sequence++;
				pBuffList->buf_state = reg_updated;
				if (buf_sequence > 1)
					pUpdateList->buf_state = buf_read_done;
				pUpdateList = pBuffList;
			} else if (pBuffList->buf_state == reg_updated)
				pBuffList->buf_state = buf_read_done;

			if (!list_is_last(&pBuffList->list, &HDMI_Buffer_List))
				pBuffList = list_entry(pBuffList->list.next, hdmi_video_buffer_list, list);
			else
				pBuffList = NULL;
		}

		pBuffList = NULL;
		pBuffList = list_first_entry(&HDMI_Buffer_List, hdmi_video_buffer_list, list);
		spin_unlock_bh(&hdmi_lock);

		while (!list_is_last(&pBuffList->list, &HDMI_Buffer_List)) {
			if (pBuffList && (pBuffList->buf_state == buf_read_done)) {
				spin_lock_bh(&hdmi_lock);
				sync_fence_put(pBuffList->fence);
				list_del(&pBuffList->list);
				kfree(pBuffList);
				pBuffList = NULL;
				spin_unlock_bh(&hdmi_lock);

				hdmi_timeline_inc();
				hdmi_signal_fence();

				remove_buffer_cnt++;

				spin_lock_bh(&hdmi_lock);
				pBuffList = list_first_entry(&HDMI_Buffer_List, hdmi_video_buffer_list, list);
				spin_unlock_bh(&hdmi_lock);
			} else
				break;
		}

		if (remove_buffer_cnt > 1)
			HDMI_LOG_DBG("%s, remove two buffer one time", __func__);

	}
}

/*this function removes all buffer in buffer list*/
void hdmi_remove_buffers(void)
{
	hdmi_video_buffer_list *pBuffList = NULL;

	while (!list_empty(&HDMI_Buffer_List)) {
		spin_lock_bh(&hdmi_lock);
		pBuffList = list_first_entry(&HDMI_Buffer_List, hdmi_video_buffer_list, list);
		spin_unlock_bh(&hdmi_lock);

		spin_lock_bh(&hdmi_lock);
		sync_fence_put(pBuffList->fence);
		list_del(&pBuffList->list);
		kfree(pBuffList);
		pBuffList = NULL;
		spin_unlock_bh(&hdmi_lock);
	}

	hdmi_release_fence();
	HDMI_LOG_DBG("fence stop rdma done\n");
}


static int hdmi_rdma_config_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_rdma_config_wq, atomic_read(&hdmi_rdma_config_event));
		atomic_set(&hdmi_rdma_config_event, 0);

		if (down_interruptible(&hdmi_update_mutex)) {
			HDMI_LOG_ERR("can't get semaphore in\n");
			continue;
		}

		if (p->is_clock_on == true && IS_HDMI_ON())	/* remove the first head here */
			hdmi_buffer_to_RDMA();

		up(&hdmi_update_mutex);

		if (kthread_should_stop())
			break;
	}
	return 0;
}


static int hdmi_rdma_update_kthread(void *data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_rdma_update_wq, atomic_read(&hdmi_rdma_update_event));
		atomic_set(&hdmi_rdma_update_event, 0);

		if (down_interruptible(&hdmi_update_mutex)) {
			HDMI_LOG_ERR("can't get semaphore in\n");
			continue;
		}

		if (p->is_clock_on == true && IS_HDMI_ON())
			hdmi_buffer_state_update();
		else
			hdmi_remove_buffers();

		up(&hdmi_update_mutex);

		if (kthread_should_stop())
			break;

	}
	return 0;
}


extern void DBG_OnTriggerHDMI(void);
extern void DBG_OnHDMIDone(void);

/* hdmi update api, will be called in LCD Interrupt handler */
void hdmi_update(void)
{
	return;
}


/* --------------------------FIXME------------------------------- */
DPI_STATUS hdmi_config_pll(HDMI_VIDEO_RESOLUTION resolution)
{
	unsigned int con1, con0;

	switch (resolution) {
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		{
			con1 = 0xc2762762;
			con0 = 0x80000101;
			break;
		}
	case HDMI_VIDEO_1920x1080p_30Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
		{
			con1 = 0xadb13b14;
			con0 = 0x800000c1;
			break;
		}

	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		{
			con1 = 0xada592ab;
			con0 = 0x800000c1;
			break;
		}
#else
	case HDMI_VIDEO_720x480p_60Hz:
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT
	case HDMI_VIDEO_720x576p_50Hz:
#endif
		{
			con1 = 0x80109d89;
			con0 = 0x80800081;
			break;
		}
	case HDMI_VIDEO_1920x1080p_30Hz:
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
#endif
		{
			con1 = 0x800b6c4e;
			con0 = 0x80000081;
			break;
		}

	case HDMI_VIDEO_1280x720p_60Hz:
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
#endif
		{
			con1 = 0x800b6964;
			con0 = 0x80000081;
			break;
		}
#endif
	default:
		{
			HDMI_LOG_ERR("not supported format, format = %d\n", resolution);
			return DPI_STATUS_ERROR;
		}
	}



	if (enable_pll(TVDPLL, "hdmi_dpi")) {
		HDMI_LOG_ERR("enable_pll fail\n");
		return DPI_STATUS_ERROR;
	}
	/* FIXME: TVDPLL_CON0 is always the same when use mt_clkmgr api */
	OUTREG32(TVDPLL_CON0, con0);

	if (pll_fsel(TVDPLL, con1)) {
		HDMI_LOG_ERR("pll_fsel fail\n");
		return DPI_STATUS_ERROR;
	}


	return DPI_STATUS_OK;
}

static void _rdma1_irq_handler(unsigned int param)
{
	RET_VOID_IF_NOLOG(!is_hdmi_active());

	if (param & 0x20) {	/* taget line 0x20 */
		atomic_set(&hdmi_rdma_config_event, 1);
		wake_up_interruptible(&hdmi_rdma_config_wq);
	}

	if (param & 1) {		/* rdma1 register updated */
		atomic_set(&hdmi_rdma_update_event, 1);
		wake_up_interruptible(&hdmi_rdma_update_wq);
	}
}


/* Allocate memory, set M4U, LCD, MDP, DPI */
/* LCD overlay to memory -> MDP resize and rotate to memory -> DPI read to HDMI */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_init(void)
{
	int lcm_width, lcm_height;
	int tmpBufferSize;
	M4U_PORT_STRUCT m4uport;

	HDMI_FUNC();

	RETIF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS, 0);

	/* Fix klocwork */
	if (hdmi_params->init_config.vformat >= HDMI_VIDEO_RESOLUTION_NUM)
		hdmi_params->init_config.vformat = HDMI_VIDEO_RESOLUTION_NUM;

	p->hdmi_width = hdmi_get_width(hdmi_params->init_config.vformat);
	p->hdmi_height = hdmi_get_height(hdmi_params->init_config.vformat);

	lcm_width = DISP_GetScreenWidth();
	lcm_height = DISP_GetScreenHeight();

	/* pr_info("[hdmi]%s, hdmi_width=%d, hdmi_height=%d\n", __func__, p->hdmi_width, p->hdmi_height); */
	HDMI_LOG_INFO("lcm_width=%d, lcm_height=%d\n", lcm_width, lcm_height);

	tmpBufferSize = lcm_width * lcm_height * hdmi_bpp * hdmi_params->intermediat_buffer_num;
#ifdef HDMI_BUFFER_KEEP_USE
	if (temp_mva_w == 0) {
#endif
		temp_va = (unsigned int)vmalloc(tmpBufferSize);
		if (((void *)temp_va) == NULL) {
			HDMI_LOG_DBG("vmalloc %dbytes fail\n", tmpBufferSize);
			return -1;
		}
		/* WDMA1 */
		if (m4u_alloc_mva(M4U_CLNTMOD_WDMA, temp_va, tmpBufferSize, 0, 0, &temp_mva_w)) {
			HDMI_LOG_DBG("m4u_alloc_mva for temp_mva_w fail\n");
			return -1;
		}
		m4u_dma_cache_maint(M4U_CLNTMOD_WDMA,
			    (void const *)temp_va, tmpBufferSize, DMA_BIDIRECTIONAL);

		m4uport.ePortID = M4U_PORT_WDMA1;
		m4uport.Virtuality = 1;
		m4uport.Security = 0;
		m4uport.domain = 0;	/* domain : 0 1 2 3 */
		m4uport.Distance = 1;
		m4uport.Direction = 0;
		m4u_config_port(&m4uport);

		HDMI_LOG_DBG("temp_va=0x%08x, temp_mva_w=0x%08x\n", temp_va, temp_mva_w);
#ifdef HDMI_BUFFER_KEEP_USE
	}
#endif


	p->lcm_width = lcm_width;
	p->lcm_height = lcm_height;
	p->lcm_is_video_mode = DISP_IsVideoMode();
	p->output_video_resolution = hdmi_params->init_config.vformat;
	p->output_audio_format = hdmi_params->init_config.aformat;
	p->scaling_factor = hdmi_params->scaling_factor < 10 ? hdmi_params->scaling_factor : 10;
	p->is_rdma_clock_on = false;
	hdmi_buffer_init(hdmi_params->intermediat_buffer_num);

	hdmi_dpi_config_clock();	/* configure dpi clock */

	hdmi_dpi_power_switch(false);	/* but dpi power is still off */

	disp_register_irq(DISP_MODULE_RDMA1, _rdma1_irq_handler);

	if (!hdmi_rdma_config_task) {
		hdmi_rdma_config_task = kthread_create(hdmi_rdma_config_kthread, NULL, "hdmi_rdma_config_kthread");
		wake_up_process(hdmi_rdma_config_task);
	}

	if (!hdmi_rdma_update_task) {
		hdmi_rdma_update_task = kthread_create(hdmi_rdma_update_kthread, NULL, "hdmi_rdma_update_kthread");
		wake_up_process(hdmi_rdma_update_task);
	}

	return HDMI_STATUS_OK;
}

/* free IRQ */
/*static*/ void hdmi_dpi_free_irq(void)
{
	RET_VOID_IF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS);
	DPI_CHECK_RET(HDMI_DPI(_FreeIRQ) ());
}

/* Release memory */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_deinit(void)
{
#ifndef HDMI_BUFFER_KEEP_USE
	int temp_va_size;
#endif

	HDMI_FUNC();
	RETIF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS, 0);

	disp_unregister_irq(DISP_MODULE_RDMA1, _rdma1_irq_handler);

	hdmi_dpi_power_switch(false);

	hdmi_buffer_deinit();

	/* free temp_va & temp_mva */
#ifdef HDMI_BUFFER_KEEP_USE
	HDMI_LOG_DBG("Skip Free temp_va and temp_mva\n");
#else
	HDMI_LOG_DBG("Free temp_va and temp_mva\n");
	temp_va_size =
	    p->lcm_width * p->lcm_height * hdmi_bpp * hdmi_params->intermediat_buffer_num;
	if (temp_mva_w) {
		M4U_PORT_STRUCT m4uport;
		m4uport.ePortID = M4U_PORT_WDMA1;
		m4uport.Virtuality = 0;
		m4uport.domain = 0;
		m4uport.Security = 0;
		m4uport.Distance = 1;
		m4uport.Direction = 0;
		m4u_config_port(&m4uport);

		m4u_dealloc_mva(M4U_CLNTMOD_WDMA, temp_va, temp_va_size, temp_mva_w);
		temp_mva_w = 0;
	}

	if (temp_va) {
		vfree((void *)temp_va);
		temp_va = 0;
	}
#endif

	hdmi_free_hdmi_buffer();

	hdmi_dpi_free_irq();
	return HDMI_STATUS_OK;
}

static void hdmi_dpi_config_update(void)
{
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	DPI_CHECK_RET(HDMI_DPI(_SW_Reset) (0x1));
#endif

	DPI_CHECK_RET(HDMI_DPI(_ConfigPixelClk) (clk_pol, dpi_clk_div, dpi_clk_duty));

	DPI_CHECK_RET(HDMI_DPI(_ConfigDataEnable) (de_pol));	/* maybe no used */

	DPI_CHECK_RET(HDMI_DPI(_ConfigHsync)
		      (hsync_pol, hsync_pulse_width, hsync_back_porch, hsync_front_porch));

	DPI_CHECK_RET(HDMI_DPI(_ConfigVsync)
		      (vsync_pol, vsync_pulse_width, vsync_back_porch, vsync_front_porch));

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	if (fgInterlace) {
		if (fg3DFrame) {
			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_LEVEN)
				      (vsync_pulse_width, vsync_back_porch, vsync_front_porch,
				       fgInterlace));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_RODD)
				      (vsync_pulse_width, vsync_back_porch, vsync_front_porch));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_REVEN)
				      (vsync_pulse_width, vsync_back_porch, vsync_front_porch,
				       fgInterlace));
		} else {
			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_LEVEN)
				      (vsync_pulse_width, vsync_back_porch, vsync_front_porch,
				       fgInterlace));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_RODD) (0, 0, 0));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_REVEN) (0, 0, 0, 0));
		}
	} else {
		if (fg3DFrame) {
			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_LEVEN) (0, 0, 0, 0));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_RODD)
				      (vsync_pulse_width, vsync_back_porch, vsync_front_porch));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_REVEN) (0, 0, 0, 0));
		} else {
			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_LEVEN) (0, 0, 0, 0));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_RODD) (0, 0, 0));

			DPI_CHECK_RET(HDMI_DPI(_ConfigVsync_REVEN) (0, 0, 0, 0));
		}
	}
	DPI_CHECK_RET(HDMI_DPI(_Config_Ctrl) (fg3DFrame, fgInterlace));	/* config 3D and Interlace */
#endif
	if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution) ||
	    (HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution)) {
		DPI_CHECK_RET(HDMI_DPI(_FBSetSize) (p->hdmi_width, p->hdmi_height / 2));
	} else {
		DPI_CHECK_RET(HDMI_DPI(_FBSetSize) (p->hdmi_width, p->hdmi_height));
	}

#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT
	/* FIXME */
	{
		/* the following are sample codes */
		DPI_CHECK_RET(DPI1_ESAVVTimingControlLeft(0, 0x1E, 0, 0));
		DPI_CHECK_RET(DPI1_MatrixCoef
			      (0x1F53, 0x1EAD, 0x0200, 0x0132, 0x0259, 0x0075, 0x0200, 0x1E53,
			       0x1FA0));
		DPI_CHECK_RET(DPI1_MatrixPreOffset(0, 0, 0));
		DPI_CHECK_RET(DPI1_MatrixPostOffset(0x0800, 0, 0x0800));
		DPI_CHECK_RET(DPI1_CLPFSetting(0, FALSE));
		DPI_CHECK_RET(DPI1_SetChannelLimit(0x0010, 0x0FE0, 0x0010, 0x0FE0));
		DPI_CHECK_RET(DPI1_EmbeddedSyncSetting
			      (TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE));
		DPI_CHECK_RET(DPI1_OutputSetting
			      (DPI_OUTPUT_BIT_NUM_8BITS, FALSE, DPI_OUTPUT_CHANNEL_SWAP_RGB,
			       DPI_OUTPUT_YC_MAP_CY));
		/* DPI_CHECK_RET(DPI1_EnableColorBar()); */
		/* DPI_CHECK_RET(DPI1_EnableBlackScreen()); */
	}
#endif
	{
#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
		DPI_CHECK_RET(DPI1_SetChannelLimit(0x0100, 0xFE00, 0x0100, 0xFE00));
#endif

		/* DPI_CHECK_RET(DPI_FBSetAddress(DPI_FB_0, hdmi_mva));//?????????????????????? */
		DPI_CHECK_RET(HDMI_DPI(_FBSetPitch) (DPI_FB_0, p->hdmi_width * 3));	/* do nothing */
		DPI_CHECK_RET(HDMI_DPI(_FBEnable) (DPI_FB_0, TRUE));	/* do nothing */
	}

	/* OUTREG32(0xF208C090, 0x41); */
	DPI_CHECK_RET(HDMI_DPI(_FBSetFormat) (DPI_FB_FORMAT_RGB888));	/* do nothing */

	if (HDMI_COLOR_ORDER_BGR == rgb_order)
		DPI_CHECK_RET(HDMI_DPI(_SetRGBOrder) (DPI_RGB_ORDER_RGB, DPI_RGB_ORDER_BGR));	/* do nothing */
	else
		DPI_CHECK_RET(HDMI_DPI(_SetRGBOrder) (DPI_RGB_ORDER_RGB, DPI_RGB_ORDER_RGB));	/* do nothing */

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace) (hdmi_colorspace, hdmi_res));
	DPI_CHECK_RET(HDMI_DPI(_SW_Reset) (0x0));
#endif
}


/* Will only be used in hdmi_drv_init(), this means that will only be use in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
/*static*/ void hdmi_dpi_config_clock(void)
{
	int ret = 0;

	RET_VOID_IF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS);

	ret = enable_pll(TVDPLL, "HDMI");
	p->is_clock_on = true;
	if (ret)
		HDMI_LOG_ERR("enable_pll fail!!\n");

	switch (hdmi_params->init_config.vformat) {
	case HDMI_VIDEO_720x480p_60Hz:
		{
			HDMI_LOG_INFO("480p\n");
			/* ret = pll_fsel(TVDPLL, 0x1C7204C7); */
			ASSERT(!ret);

			dpi_clk_div = 2;
			dpi_clk_duty = 1;

			break;
		}
	case HDMI_VIDEO_1280x720p_60Hz:
		{
			HDMI_LOG_INFO("720p 60Hz\n");
			/* ret = pll_fsel(TVDPLL, 0xDBCD0119); */
			ASSERT(!ret);

			dpi_clk_div = 2;
			dpi_clk_duty = 1;

			break;
		}
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_1280x720p_50Hz:{
			HDMI_LOG_INFO("720p 50Hz\n");
			/* ret = pll_fsel(TVDPLL, 0x1C7204C7); */
			ASSERT(!ret);

			dpi_clk_div = 2;
			dpi_clk_duty = 1;

			break;
		}
#endif
	default:
		{
			HDMI_LOG_ERR("not supported format, format = %d\n",
				 hdmi_params->init_config.vformat);
			break;
		}
	}

	clk_pol = hdmi_params->clk_pol;
	de_pol = hdmi_params->de_pol;
	hsync_pol = hdmi_params->hsync_pol;
	vsync_pol = hdmi_params->vsync_pol;

	hsync_pulse_width = hdmi_params->hsync_pulse_width;
	vsync_pulse_width = hdmi_params->vsync_pulse_width;
	hsync_back_porch = hdmi_params->hsync_back_porch;
	vsync_back_porch = hdmi_params->vsync_back_porch;
	hsync_front_porch = hdmi_params->hsync_front_porch;
	vsync_front_porch = hdmi_params->vsync_front_porch;

	rgb_order = hdmi_params->rgb_order;
	intermediat_buffer_num = hdmi_params->intermediat_buffer_num;


	DPI_CHECK_RET(HDMI_DPI(_Init) (FALSE));
}


int hdmi_allocate_hdmi_buffer(void)
{
	M4U_PORT_STRUCT m4uport;
#ifdef HDMI_BUFFER_KEEP_USE
	int hdmiPixelSize = 1920 * 1080;
#else
	int hdmiPixelSize = p->hdmi_width * p->hdmi_height;
#endif
	int hdmiDataSize = hdmiPixelSize * 4;	/* /hdmi_bpp; */
	int hdmiBufferSize = hdmiDataSize * hdmi_params->intermediat_buffer_num;

	HDMI_FUNC();

#ifdef HDMI_BUFFER_KEEP_USE
	if (hdmi_va != 0 && hdmi_mva_r != 0) {
		HDMI_LOG_DBG("HDMI Buffer is already allocated, hdmi_va=0x%08x, hdmi_mva_r=0x%08x\n",
			 hdmi_va, hdmi_mva_r);
		return 0;
	}
#endif

	hdmi_va = (unsigned int)vmalloc(hdmiBufferSize);
	if (((void *)hdmi_va) == NULL) {
		HDMI_LOG_DBG("vmalloc %dbytes fail!!!\n", hdmiBufferSize);
		return -1;
	}

	memset((void *)hdmi_va, 0, hdmiBufferSize);

	/* RDMA1 */
	if (m4u_alloc_mva(M4U_CLNTMOD_RDMA, hdmi_va, hdmiBufferSize, 0, 0, &hdmi_mva_r)) {
		HDMI_LOG_DBG("m4u_alloc_mva for hdmi_mva_r fail\n");
		return -1;
	}
	memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
	m4uport.ePortID = M4U_PORT_RDMA1;
	m4uport.Virtuality = 1;
	m4uport.domain = 0;
	m4uport.Security = 0;
	m4uport.Distance = 1;
	m4uport.Direction = 0;
	m4u_config_port(&m4uport);

	HDMI_LOG_DBG("hdmi_va=0x%08x, hdmi_mva_r=0x%08x\n", hdmi_va, hdmi_mva_r);

	return 0;
}

int hdmi_free_hdmi_buffer(void)
{
#ifdef HDMI_BUFFER_KEEP_USE
	HDMI_LOG_DBG("SKIP Free hdmi_va and hdmi_mva, hdmi_va=0x%08x, hdmi_mva_r=0x%08x\n", hdmi_va,
		 hdmi_mva_r);
#else
	int hdmi_va_size =
	    p->hdmi_width * p->hdmi_height * hdmi_bpp * hdmi_params->intermediat_buffer_num;

	/* free hdmi_va & hdmi_mva */
	HDMI_LOG_DBG("Free hdmi_va and hdmi_mva, hdmi_va=0x%08x, hdmi_mva_r=0x%08x\n", hdmi_va,
		 hdmi_mva_r);

	if (hdmi_mva_r) {
		M4U_PORT_STRUCT m4uport;
		m4uport.ePortID = M4U_PORT_RDMA1;
		m4uport.Virtuality = 0;
		m4uport.domain = 0;
		m4uport.Security = 0;
		m4uport.Distance = 1;
		m4uport.Direction = 0;
		m4u_config_port(&m4uport);

		m4u_dealloc_mva(M4U_CLNTMOD_RDMA, hdmi_va, hdmi_va_size, hdmi_mva_r);
		hdmi_mva_r = 0;
	}

	if (hdmi_va) {
		vfree((void *)hdmi_va);
		hdmi_va = 0;
	}
#endif
	return 0;
}

static int rdmafpscnt;
int hdmi_rdma_address_config(bool enable, hdmi_video_buffer_info buffer_info)
{
	unsigned int offset;
	unsigned int hdmiSourceAddr;
	struct disp_path_config_struct config = { 0 };

	HDMI_FUNC();

	if (enable) {
		/* /hdmi_bpp == 2 ? RDMA_INPUT_FORMAT_UYVY : RDMA_INPUT_FORMAT_RGB888; */
		int rdmaInputFormat = RDMA_INPUT_FORMAT_RGB888;
		unsigned int rdmaInputsize = 3;
		bool need_config = true;

		if (p->is_clock_on == false || IS_HDMI_NOT_ON()) {
			HDMI_LOG_DBG("clock stoped enable(%d), is_clock_on(%d)\n", enable, p->is_clock_on);
			return -1;
		}
/*
	if(buffer_info.src_fmt == MTK_FB_FORMAT_ARGB8888)
	{
	    rdmaInputsize = 4;
	    rdmaInputFo1rmat = RDMA_INPUT_FORMAT_ARGB;
	}
	else if(buffer_info.src_fmt == MTK_FB_FORMAT_BGR888)
	{
	    rdmaInputsize = 3;
	    rdmaInputFormat = RDMA_INPUT_FORMAT_RGB888;
	}
*/
		offset = (buffer_info.src_pitch - buffer_info.src_width) / 2 * rdmaInputsize;
#ifdef ENABLE_SECURE_HDMI
		if (buffer_info.security == 1) {
			hdmiSourceAddr = (unsigned int)buffer_info.src_phy_addr;/* This will be a secure buffer handle */
			/* HDMI_LOG("HDMI Get secure buffer, handle = %d\n", hdmiSourceAddr); */
		} else
#endif
		{
			hdmiSourceAddr = (unsigned int)buffer_info.src_phy_addr
			    + buffer_info.src_offset_y * buffer_info.src_pitch * rdmaInputsize
			    + buffer_info.src_offset_x * rdmaInputsize + offset;
			/* HDMI_LOG("HDMI Get normal buffer, address = 0x%X\n", hdmiSourceAddr); */
		}

		/* Config RDMA->DPI1 */
		config.addr = hdmiSourceAddr;
		config.srcWidth = buffer_info.src_width;
		config.srcHeight = buffer_info.src_height;
		config.srcModule = DISP_MODULE_RDMA1;
		config.inFormat = rdmaInputFormat;
		config.pitch = buffer_info.src_pitch * 3;
		config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
		config.dstModule = HMID_DEST_DPI;

		if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution) ||
		    (HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution)) {
			config.pitch *= 2;
			config.srcHeight /= 2;
			if (DPI1_IS_TOP_FIELD())
				config.addr += p->hdmi_width * hdmi_bpp;
		}

		if (dp_mutex_dst <= 0) {
			dp_mutex_dst = 2;
			rdmafpscnt = 0;
		} else {
			need_config = false;
		}

		rdmafpscnt++;

		disp_path_get_mutex_(dp_mutex_dst);

#ifdef ENABLE_SECURE_HDMI
		if ((true == need_config) || (gRDMASecure != buffer_info.security))
#else
		if (true == need_config)
#endif
		{
#ifdef ENABLE_SECURE_HDMI
			if (gRDMASecure != buffer_info.security) {
				/* HDMI_LOG("Disable HDMI DPI Clock\n"); */
				DPI_CHECK_RET(HDMI_DPI(_DisableClk) ());
				gRDMASecure = buffer_info.security;
			}
			disp_path_config_(&config, dp_mutex_dst);
			if (buffer_info.security) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;
				/* HDMI_LOG("Config M4U Security path\n"); */
				disp_register_intr(MT8135_DISP_RDMA1_IRQ_ID, 0);
				param[0].value.a = M4U_PORT_RDMA1;
				param[1].value.a = buffer_info.security;
				paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_SET_SECURE_MODE, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					HDMI_LOG_ERR
					    ("KREE_TeeServiceCall(TZCMD_DDP_SET_SECURE_MODE) fail, ret=%d\n",
					     ret);
				}
			} else {
#endif
				M4U_PORT_STRUCT m4uport;
				HDMI_LOG_DBG("Config M4U normal path\n");
				memset((void *)&m4uport, 0, sizeof(M4U_PORT_STRUCT));
				m4uport.ePortID = M4U_PORT_RDMA1;
				m4uport.Virtuality = 1;
				m4uport.domain = 0;
				m4uport.Security = 0;
				m4uport.Distance = 1;
				m4uport.Direction = 0;
				m4u_config_port(&m4uport);
#ifdef ENABLE_SECURE_HDMI
			}
#endif

#ifndef ENABLE_SECURE_HDMI
			disp_path_config_(&config, dp_mutex_dst);
#endif
			DPI_CHECK_RET(HDMI_DPI(_EnableClk) ());

		} else {
#ifdef ENABLE_SECURE_HDMI
			if (buffer_info.security) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;

				param[0].value.a = (uint32_t) config.addr;
				paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);
				HDMI_LOG_DBG("Rdma config handle=0x%x\n", param[0].value.a);

				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_RDMA1_ADDR_CONFIG, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS)
					HDMI_LOG_ERR("TZCMD_DDP_RDMA_ADDR_CONFIG fail, ret=%d\n", ret);
			} else
#endif
				DISP_REG_SET(0x1000 + DISP_REG_RDMA_MEM_START_ADDR, config.addr);
		}

		/* /disp_path_config_(&config, dp_mutex_dst); */
		disp_path_release_mutex_(dp_mutex_dst);


	} else {
		if (-1 != dp_mutex_dst) {
			/* FIXME: release mutex timeout */
			HDMI_LOG_DBG("Stop RDMA1>DPI1\n");
			disp_path_get_mutex_(dp_mutex_dst);

			DISP_REG_SET_FIELD(1 << dp_mutex_src, DISP_REG_CONFIG_MUTEX_INTEN, 1);
			RDMAStop(1);
			RDMAReset(1);
			disp_path_release_mutex_(dp_mutex_dst);

			/* disp_unlock_mutex(dp_mutex_dst); */
			dp_mutex_dst = -1;
		}
	}

	return 0;
}


/* Switch DPI Power for HDMI Driver */
/*static*/ void hdmi_dpi_power_switch(bool enable)
{
	int ret;

	HDMI_LOG_DBG("DPI clock:%d\n", enable);

	RET_VOID_IF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS);

	if (enable) {
		if (p->is_clock_on == true) {
			HDMI_LOG_DBG("power on request while already powered on!\n");
			return;
		}

		ret = enable_pll(TVDPLL, "HDMI");
		if (ret) {
			HDMI_LOG_ERR("enable_pll fail!!\n");
			return;
		}
		HDMI_DPI(_PowerOn) ();
		HDMI_DPI(_EnableIrq) ();
		DPI_CHECK_RET(HDMI_DPI(_EnableClk) ());

#ifndef ENABLE_MULTI_DISPLAY
		p->is_clock_on = true;
#endif
	} else {
		if (p->is_clock_on == false) {
			HDMI_LOG_DBG("power off request while already powered off!\n");
			return;
		}

		p->is_clock_on = false;

		atomic_set(&hdmi_rdma_update_event, 1);
		wake_up_interruptible(&hdmi_rdma_update_wq);

		HDMI_DPI(_DisableIrq) ();
		HDMI_DPI(_DisableClk) ();
		HDMI_DPI(_PowerOff) ();

		ret = disable_pll(TVDPLL, "HDMI");
		if (ret) {
			HDMI_LOG_ERR("disable_pll fail!!\n");
			/* return; */
		}

	}
}

/* Configure video attribute */
static int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
			     HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	HDMI_FUNC();
	RETIF(IS_HDMI_NOT_ON(), 0);

	hdmi_fps_control_overlay = 0;
	hdmi_fps_control_dpi = 0;

	return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
int hdmi_audio_config(int samplerate)
{
	HDMI_FUNC();
	RETIF(!p->is_enabled, 0);
	RETIF(IS_HDMI_NOT_ON(), 0);

	HDMI_LOG_DBG("sample rate=%d\n", samplerate);
	if (samplerate == 48000)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_48000;
	else if (samplerate == 44100)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_44100;
	else if (samplerate == 32000)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_32000;
	else
		HDMI_LOG_DBG("samplerate not support:%d\n", samplerate);


	hdmi_drv->audio_config(p->output_audio_format);

	return 0;
}

/* No one will use this function */
/*static*/ int hdmi_video_enable(bool enable)
{
	HDMI_FUNC();

	return hdmi_drv->video_enable(enable);
}

/* No one will use this function */
/*static*/ int hdmi_audio_enable(bool enable)
{
	HDMI_FUNC();

	return hdmi_drv->audio_enable(enable);
}

struct timer_list timer;
void __timer_isr(unsigned long n)
{
	HDMI_FUNC();
	if (hdmi_drv->audio_enable)
		hdmi_drv->audio_enable(true);

	del_timer(&timer);
}

int hdmi_audio_delay_mute(int latency)
{
	HDMI_FUNC();
	memset((void *)&timer, 0, sizeof(timer));
	timer.expires = jiffies + (latency * HZ / 1000);
	timer.function = __timer_isr;
	init_timer(&timer);
	add_timer(&timer);
	if (hdmi_drv->audio_enable)
		hdmi_drv->audio_enable(false);
	return 0;
}

#if (!defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
/* Reset HDMI Driver state */
static void hdmi_state_reset(void)
{
	HDMI_FUNC();

	if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
		switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
		switch_set_state(&hdmi_audio_switch_data, 1);
		hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
	} else {
		switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
		switch_set_state(&hdmi_audio_switch_data, 0);
		switch_set_state(&hdmires_switch_data, 0);
	}
}
#endif

static void dpi_setting_res(u8 arg);
unsigned long hdmi_get_resulotion(HDMI_EDID_INFO_T pv_get_info)
{
	unsigned long resolution = HDMI_VIDEO_720x480p_60Hz;
	int preResolution = pv_get_info.ui4_ntsc_resolution | pv_get_info.ui4_pal_resolution;
	HDMI_LOG_DBG("hdmi_get_resulotion, ntsc: %d, pal: %d\n",
				pv_get_info.ui4_ntsc_resolution, pv_get_info.ui4_pal_resolution);

	if (preResolution & (1 << 3))
		resolution = HDMI_VIDEO_1920x1080p_60Hz;
	else if (preResolution & (1 << 13))
		resolution = HDMI_VIDEO_1920x1080p_50Hz;
	else if (preResolution & (1 << 2))
		resolution = HDMI_VIDEO_1920x1080i_60Hz;
	else if (preResolution & (1 << 12))
		resolution = HDMI_VIDEO_1920x1080i_50Hz;
	else if (preResolution & (1 << 1))
		resolution = HDMI_VIDEO_1280x720p_60Hz;
	else if (preResolution & (1 << 13))
		resolution = HDMI_VIDEO_1280x720p_50Hz;
	else if (preResolution & (1 << 0))
		resolution = HDMI_VIDEO_720x480p_60Hz;
	else if (preResolution & (1 << 10))
		resolution = HDMI_VIDEO_720x576p_50Hz;


	HDMI_LOG_DBG("hdmi_get_resulotion resolution: %ld\n", resolution);
	return resolution;
}

int hdmi_set_video_config_ex(unsigned long resolution)
{
	int switchres;
	int tmp = 0;
	int security = p->is_security_output;
	hdmi_video_buffer_info temp;

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	/* temporary solution for hdmi svp p1, always mute hdmi for svp */
	int svp = p->is_svp_output;
#endif
	HDMI_LOG_DBG("video resolution configuration, arg=%ld\n", resolution);

#if (!defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
	if (resolution > 1) {
		resolution = 1;
		HDMI_LOG_DBG("video resolution configuration, reset arg=%ld\n", arg);
	}
#endif

	RETIF(!p->is_enabled, 0);
	RETIF(IS_HDMI_NOT_ON(), 0);

	if (hdmi_reschange == resolution) {
		HDMI_LOG_DBG("hdmi_reschange=%ld\n", hdmi_reschange);
		return -1;
	}

	if (hdmi_drv->mutehdmi)
		hdmi_drv->mutehdmi(security, false);

	hdmi_reschange = resolution;
	p->is_clock_on = false;

	atomic_set(&hdmi_rdma_update_event, 1);
	wake_up_interruptible(&hdmi_rdma_update_wq);
	while (1) {
		if ((list_empty(&HDMI_Buffer_List)) || (tmp > 15)) {
			if (tmp > 15)
				HDMI_LOG_ERR(" Error HDMI_Buffer_List is not empty\n");
			break;
		} else
			msleep(20);

		tmp++;
	}

	RETIF(!p->is_enabled, 0);
	RETIF(IS_HDMI_NOT_ON(), 0);

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_DBG("[HDMI] can't get semaphore in\n");
		return EAGAIN;
	}
#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	if ((resolution == HDMI_VIDEO_1920x1080p_60Hz)
	    || (resolution == HDMI_VIDEO_1920x1080p_50Hz)
	    || (resolution == HDMI_VIDEO_1280x720p3d_60Hz)
	    || (resolution == HDMI_VIDEO_1280x720p3d_50Hz)
	    || (resolution == HDMI_VIDEO_1920x1080p3d_24Hz)
	    || (resolution == HDMI_VIDEO_1920x1080p3d_23Hz)
	    ) {
		hdmi_colorspace = HDMI_YCBCR_422;
	} else {
		hdmi_colorspace = HDMI_RGB;
	}
	hdmi_drv->colordeep(hdmi_colorspace);
#endif

#if (defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
	if (hdmi_drv->tmdsonoff) {
		hdmi_mdelay(50);
		hdmi_drv->tmdsonoff(0);
	}
#endif

	hdmi_rdma_address_config(false, temp);
	hdmi_config_pll(resolution);
	dpi_setting_res((u8) resolution);
	hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888,  HDMI_VOUT_FORMAT_RGB888);

	if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution) ||
			(HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution))
		RDMASetTargetLine(1, p->hdmi_height *  2 / 5);
	else
		RDMASetTargetLine(1, p->hdmi_height * 4 / 5);

	DPI_CHECK_RET(HDMI_DPI(_DisableClk) ());
#if (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) && !defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
	DPI_CHECK_RET(HDMI_DPI(_ConfigHDMI) ());
#endif
	hdmi_dpi_config_update();
	up(&hdmi_update_mutex);
	p->is_clock_on = true;

	if ((p->is_security_output != security) && hdmi_drv->mutehdmi)
		hdmi_drv->mutehdmi(p->is_security_output, false);

	switchres = (p->hdmi_width << 16 | p->hdmi_height);
	switch_set_state(&hdmires_switch_data, switchres);

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	/* temporary solution for hdmi svp p1, always mute hdmi for svp */
	if ((p->is_svp_output != svp) && hdmi_drv->svpmutehdmi)
		hdmi_drv->svpmutehdmi(p->is_svp_output, false);
#endif

	return 0;
}

/* HDMI Driver state callback function */
void hdmi_state_callback(HDMI_STATE state)
{
	#ifdef CONFIG_SLIMPORT_ANX3618
	HDMI_EDID_INFO_T pv_get_info;
	#endif

	HDMI_LOG_DBG("%s, state = %d\n", __func__, state);

	RET_VOID_IF((p->is_force_disable == true));
	RET_VOID_IF(IS_HDMI_FAKE_PLUG_IN());

	switch (state) {
	case HDMI_STATE_NO_DEVICE:
		{
			hdmi_suspend();
			switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
			switch_set_state(&hdmires_switch_data, 0);
			break;
		}
	case HDMI_STATE_ACTIVE:
		{
			hdmi_resume();
				msleep(1000);
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			switch_set_state(&hdmi_audio_switch_data, 1);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		#ifdef CONFIG_SLIMPORT_ANX3618
			hdmi_drv->getedid(&pv_get_info);
			hdmi_drv->colordeep(0, 1);
			hdmi_set_video_config_ex(hdmi_get_resulotion(pv_get_info));
		#endif
			break;
		}
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	case HDMI_STATE_PLUGIN_ONLY:
		{
			switch_set_state(&hdmi_switch_data, HDMI_STATE_PLUGIN_ONLY);

			break;
		}
	case HDMI_STATE_EDID_UPDATE:
		{
			switch_set_state(&hdmi_switch_data, HDMI_STATE_EDID_UPDATE);

			break;
		}
	case HDMI_STATE_CEC_UPDATE:
		{
			switch_set_state(&hdmi_switch_data, HDMI_STATE_CEC_UPDATE);

			break;
		}
#endif
	default:
		{
			HDMI_LOG_ERR("%s, state not support\n", __func__);
			break;
		}
	}

	return;
}

/*static*/ void hdmi_power_on(void)
{
	HDMI_FUNC();

	RET_VOID_IF(IS_HDMI_NOT_OFF());

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}
	/* Why we set power state before calling hdmi_drv->power_on()? */
	/* Because when power on, the hpd irq will come immediately, */
	/* that means hdmi_resume will be called before hdmi_drv->power_on() retuen here. */
	/* So we have to ensure the power state is STANDBY before hdmi_resume() be called. */
	SET_HDMI_STANDBY();

	if (!p->is_rdma_clock_on) {
		disp_module_clock_on(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_on(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_on(DISP_MODULE_WDMA1, "HDMI");
		enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		p->is_rdma_clock_on = true;
	}

	hdmi_drv->power_on();

	/* When camera is open, the state will only be changed when camera exits. */
	/* So we bypass state_reset here, if camera is open. */
	/* The related scenario is: suspend in camera with hdmi enabled. */
	/* Why need state_reset() here? */
	/* When we suspend the phone, and then plug out hdmi cable, the hdmi chip status will change immediately */
	/* But when we resume the phone and check hdmi status, the irq will never come again */
	/* So we have to reset hdmi state manually, to ensure the status is the same between the host and hdmi chip. */
#if (!defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
	if (p->is_force_disable == false) {
		if (IS_HDMI_FAKE_PLUG_IN()) {
			/* FixMe, deadlock may happened here, due to recursive use mutex */
			hdmi_resume();
			msleep(1000);
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			switch_set_state(&hdmi_audio_switch_data, 1);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		} else {
			hdmi_state_reset();
			/* this is just a ugly workaround for some tv sets... */
			/* if(hdmi_drv->get_state() == HDMI_STATE_ACTIVE) */
			/* hdmi_resume(); */
		}
	}
#endif
	up(&hdmi_update_mutex);

	return;
}

/*static*/ void hdmi_resume(void)
{
	HDMI_FUNC();

	RET_VOID_IF(IS_HDMI_NOT_STANDBY());
	SET_HDMI_ON();

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}

	if (!p->is_rdma_clock_on) {
		disp_module_clock_on(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_on(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_on(DISP_MODULE_WDMA1, "HDMI");
		enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		p->is_rdma_clock_on = true;
	}


	hdmi_dpi_power_switch(true);
	hdmi_drv->resume();
	up(&hdmi_update_mutex);
}

/*static*/ void hdmi_suspend(void)
{
	hdmi_video_buffer_info temp;
	HDMI_FUNC();
	RET_VOID_IF(IS_HDMI_NOT_ON());

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}
	/* /hdmi_drv->suspend(); */
	hdmi_dpi_power_switch(false);
	hdmi_rdma_address_config(false, temp);
	hdmi_drv->suspend();
	SET_HDMI_STANDBY();

	if (p->is_rdma_clock_on) {
		disp_module_clock_off(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_off(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_off(DISP_MODULE_WDMA1, "HDMI");
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		p->is_rdma_clock_on = false;
	}

	up(&hdmi_update_mutex);
}

/*static*/ void hdmi_power_off(void)
{
	HDMI_FUNC();
	RET_VOID_IF(IS_HDMI_OFF());

	hdmi_suspend();

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}

	hdmi_drv->power_off();
	SET_HDMI_OFF();

	if (p->is_rdma_clock_on) {
		disp_module_clock_off(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_off(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_off(DISP_MODULE_WDMA1, "HDMI");
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		p->is_rdma_clock_on = false;
	}


	up(&hdmi_update_mutex);

	return;
}

/* Set HDMI orientation, will be called in mtkfb_ioctl(SET_ORIENTATION) */
/*static*/ void hdmi_setorientation(int orientation)
{
	HDMI_FUNC();
	/* /RET_VOID_IF(!p->is_enabled); */

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s\n", __func__);
		return;
	}

	p->orientation = orientation;
	p->is_reconfig_needed = true;

/* done: */
	up(&hdmi_update_mutex);
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
	return 0;
}

static BOOL hdmi_drv_init_context(void);

static void dpi_setting_res(u8 arg)
{
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	fg3DFrame = FALSE;
	fgInterlace = FALSE;
#endif

	switch (arg) {
	case HDMI_VIDEO_720x480p_60Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 62;
			hsync_back_porch = 60;
			hsync_front_porch = 16;

			vsync_pulse_width = 6;
			vsync_back_porch = 30;
			vsync_front_porch = 9;

			p->hdmi_width = 720;
			p->hdmi_height = 480;
			p->output_video_resolution = HDMI_VIDEO_720x480p_60Hz;
			break;
		}
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_720x576p_50Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 64;
			hsync_back_porch = 68;
			hsync_front_porch = 12;

			vsync_pulse_width = 5;
			vsync_back_porch = 39;
			vsync_front_porch = 5;

			p->hdmi_width = 720;
			p->hdmi_height = 576;
			p->output_video_resolution = HDMI_VIDEO_720x576p_50Hz;
			break;
		}
#endif
	case HDMI_VIDEO_1280x720p_60Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(HDMI_TDA19989) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
#endif
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 110;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->hdmi_width = 1280;
			p->hdmi_height = 720;
			p->output_video_resolution = HDMI_VIDEO_1280x720p_60Hz;
			break;
		}
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_1280x720p_50Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 440;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->hdmi_width = 1280;
			p->hdmi_height = 720;
			p->output_video_resolution = HDMI_VIDEO_1280x720p_50Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_24Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_24Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_25Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_25Hz;
			break;
		}
#endif
	case HDMI_VIDEO_1920x1080p_30Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_30Hz;
			break;
		}
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_1920x1080p_29Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_29Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_23Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;
#else
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;
#endif

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_23Hz;
			break;
		}
#endif

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case HDMI_VIDEO_1920x1080p_60Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_60Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p_50Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p_50Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080i_60Hz:
		{
			fgInterlace = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 15;
			vsync_front_porch = 2;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080i_60Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080i_50Hz:
		{
			fgInterlace = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 15;
			vsync_front_porch = 2;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080i_50Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080i3d_60Hz:
		{
			fgInterlace = TRUE;
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 15;
			vsync_front_porch = 2;

			p->hdmi_width = 1920;
			p->hdmi_height = 540;
			p->output_video_resolution = HDMI_VIDEO_1920x1080i3d_60Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080i3d_50Hz:
		{
			fgInterlace = TRUE;
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 168;
			hsync_back_porch = 184;
			hsync_front_porch = 32;

			vsync_pulse_width = 5;
			vsync_back_porch = 57;
			vsync_front_porch = 23;

			p->hdmi_width = 1920;
			p->hdmi_height = 540;
			p->output_video_resolution = HDMI_VIDEO_1920x1080i3d_50Hz;
			break;
		}
	case HDMI_VIDEO_1280x720p3d_60Hz:
		{
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;

			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 110;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->hdmi_width = 1280;
			p->hdmi_height = 720;
			p->output_video_resolution = HDMI_VIDEO_1280x720p3d_60Hz;
			break;
		}
	case HDMI_VIDEO_1280x720p3d_50Hz:
		{
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 440;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->hdmi_width = 1280;
			p->hdmi_height = 720;
			p->output_video_resolution = HDMI_VIDEO_1280x720p3d_50Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p3d_24Hz:
		{
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p3d_24Hz;
			break;
		}
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		{
			fg3DFrame = TRUE;
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			dpi_clk_div = 2;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->hdmi_width = 1920;
			p->hdmi_height = 1080;
			p->output_video_resolution = HDMI_VIDEO_1920x1080p3d_23Hz;
			break;
		}
#endif

	default:
		break;
	}
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	hdmi_res = p->output_video_resolution;
#endif
}

void MTK_HDMI_Set_Security_Output(int enable)
{
	if (p) {
		if ((p->is_security_output != enable)
		    && IS_HDMI_ON()) {
			if (hdmi_drv->mutehdmi == NULL) {
				HDMI_LOG_DBG("mutehdmi is null\n");
				return;
			}

			HDMI_LOG_DBG("change from %d to %d\n", p->is_security_output, enable);
			hdmi_drv->mutehdmi(enable, false);
		}
		p->is_security_output = enable;

	} else {
		HDMI_LOG_ERR("hdmi not init yet\n");
	}

}

/* temporary solution for hdmi svp p1, always mute hdmi for svp */
void MTK_HDMI_Set_Security_Output_SVP_P1(int enable)
{
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	if (p) {
		if ((p->is_svp_output != enable)
		    && IS_HDMI_ON()) {
			HDMI_LOG_DBG("start to switch hdmi output for svp p1 (%d, %d, %d )\n",
				 p->is_security_output, p->is_svp_output, enable);

			if (hdmi_drv->svpmutehdmi == NULL) {
				HDMI_LOG_DBG("svpmutehdmi is null\n");
				return;
			}

			HDMI_LOG_DBG("change from %d to %d\n", p->is_security_output, enable);
			hdmi_drv->svpmutehdmi(enable, false);
			HDMI_LOG_DBG("finish to switch hdmi output for svp p1 (%d, %d, %d )\n",
				 p->is_security_output, p->is_svp_output, enable);
		}
		p->is_svp_output = enable;
		p->is_security_output = enable;
	} else {
		HDMI_LOG_ERR("hdmi not init yet\n");
	}
#else
	HDMI_LOG_ERR("Error: not support external HDMI and MHL for svp p1\n");
#endif
}

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	int r = 0;
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	hdmi_device_write w_info;
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT) && defined(CONFIG_MTK_DRM_KEY_MNG_SUPPORT))
	hdmi_hdcp_drmkey key;
#else
	hdmi_hdcp_key key;
#endif
	send_slt_data send_sltdata;
	CEC_SLT_DATA get_sltdata;
	hdmi_para_setting data_info;
	HDMI_EDID_INFO_T pv_get_info;
	CEC_FRAME_DESCRIPTION cec_frame;
	CEC_ADDRESS cecaddr;
	CEC_DRV_ADDR_CFG_T cecsetAddr;
	CEC_SEND_MSG_T cecsendframe;
	READ_REG_VALUE regval;
#endif
#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
	RW_VALUE stSpi;
	unsigned int addr, u4data;
	unsigned int tmp;
	stMhlCmd_st stMhlCmd;
	HDMI_EDID_INFO_T pv_get_info;
	unsigned char pdata[16];
	MHL_3D_INFO_T pv_3d_info;
	hdmi_para_setting data_info;
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT) && defined(CONFIG_MTK_DRM_KEY_MNG_SUPPORT))
	hdmi_hdcp_drmkey key;
#else
	hdmi_hdcp_key key;
#endif
#endif

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	HDMIDRV_AUDIO_PARA audio_para;
#endif
	HDMI_LOG_DBG("ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);

	switch (cmd) {
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case MTK_HDMI_AUDIO_SETTING:
		{
			if (copy_from_user(&audio_para, (void __user *)arg, sizeof(audio_para))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				if (down_interruptible(&hdmi_update_mutex)) {
					HDMI_LOG_ERR("can't get semaphore in\n");
					return EAGAIN;
				}
				hdmi_drv->audiosetting(&audio_para);
				up(&hdmi_update_mutex);
			}
			break;
		}
#endif
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	case MTK_HDMI_WRITE_DEV:
		{
			if (copy_from_user(&w_info, (void __user *)arg, sizeof(w_info))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->write(w_info.u4Addr, w_info.u4Data);
			}
			break;
		}

	case MTK_HDMI_INFOFRAME_SETTING:
		{
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->InfoframeSetting(data_info.u4Data1 & 0xFF,
							   data_info.u4Data2 & 0xFF);
			}
			break;
		}

	case MTK_HDMI_HDCP_KEY:
		{
			if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->hdcpkey((UINT8 *) &key);
			}
			break;
		}

	case MTK_HDMI_SETLA:
		{
			if (copy_from_user(&cecsetAddr, (void __user *)arg, sizeof(cecsetAddr))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->setcecla(&cecsetAddr);
			}
			break;
		}

	case MTK_HDMI_SENDSLTDATA:
		{
			if (copy_from_user(&send_sltdata, (void __user *)arg, sizeof(send_sltdata))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->sendsltdata((UINT8 *) &send_sltdata);
			}
			break;
		}

	case MTK_HDMI_SET_CECCMD:
		{
			if (copy_from_user(&cecsendframe, (void __user *)arg, sizeof(cecsendframe))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->setceccmd(&cecsendframe);
			}
			break;
		}

	case MTK_HDMI_CEC_ENABLE:
		{
			hdmi_drv->cecenable(arg & 0xFF);
			break;
		}


	case MTK_HDMI_GET_EDID:
		{
			hdmi_drv->getedid(&pv_get_info);
			if (copy_to_user((void __user *)arg, &pv_get_info, sizeof(pv_get_info))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_CECCMD:
		{
			hdmi_drv->getceccmd(&cec_frame);
			if (copy_to_user((void __user *)arg, &cec_frame, sizeof(cec_frame))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_SLTDATA:
		{
			hdmi_drv->getsltdata(&get_sltdata);
			if (copy_to_user((void __user *)arg, &get_sltdata, sizeof(get_sltdata))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_CECADDR:
		{
			hdmi_drv->getcecaddr(&cecaddr);
			if (copy_to_user((void __user *)arg, &cecaddr, sizeof(cecaddr))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_COLOR_DEEP:
		{
			#ifndef CONFIG_SLIMPORT_ANX3618
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->colordeep(data_info.u4Data1 & 0xFF,
						    data_info.u4Data2 & 0xFF);
			}
#if (defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			hdmi_colorspace = (unsigned char)data_info.u4Data1;
			DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace) (hdmi_colorspace, hdmi_res));
#endif
			#endif
			break;
		}

	case MTK_HDMI_READ_DEV:
		{
			if (copy_from_user(&regval, (void __user *)arg, sizeof(regval))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->read(regval.u1adress, &regval.pu1Data);
			}

			if (copy_to_user((void __user *)arg, &regval, sizeof(regval))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_ENABLE_LOG:
		{
			hdmi_drv->log_enable(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_ENABLE_HDCP:
		{
			hdmi_drv->enablehdcp(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_CECRX_MODE:
		{
			hdmi_drv->setcecrxmode(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_STATUS:
		{
			hdmi_drv->hdmistatus();
			break;
		}

	case MTK_HDMI_CHECK_EDID:
		{
			hdmi_drv->checkedid(arg & 0xFF);
			break;
		}

#elif defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case MTK_HDMI_READ:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->read(stSpi.u4Addr, &(stSpi.u4Data));

			if (copy_to_user((void __user *)arg, &stSpi, sizeof(RW_VALUE))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}
	case MTK_HDMI_WRITE:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->write(stSpi.u4Addr, stSpi.u4Data);

			break;
		}
	case MTK_HDMI_DUMP:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			break;
		}
	case MTK_HDMI_STATUS:
		{
			hdmi_drv->hdmistatus();
			break;
		}
	case MTK_HDMI_DUMP6397:
		{
			hdmi_drv->dump6397();
			break;
		}
	case MTK_HDMI_DUMP6397_W:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->write6397(stSpi.u4Addr, stSpi.u4Data);

			break;
		}
	case MTK_HDMI_DUMP6397_R:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->read6397(stSpi.u4Addr, &(stSpi.u4Data));

			if (copy_to_user((void __user *)arg, &stSpi, sizeof(RW_VALUE))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}

			break;
		}
	case MTK_HDMI_CBUS_STATUS:
		{
			hdmi_drv->cbusstatus();
			break;
		}
	case MTK_HDMI_CMD:
		{
			HDMI_LOG_DBG("MTK_HDMI_CMD\n");
			if (copy_from_user(&stMhlCmd, (void __user *)arg, sizeof(stMhlCmd_st))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			}
			HDMI_LOG_DBG("[MHL]cmd=%x%x%x%x\n", stMhlCmd.u4Cmd, stMhlCmd.u4Para,
				 stMhlCmd.u4Para1, stMhlCmd.u4Para2);
			hdmi_drv->mhl_cmd(stMhlCmd.u4Cmd, stMhlCmd.u4Para, stMhlCmd.u4Para1,
					  stMhlCmd.u4Para2);
			break;
		}
	case MTK_HDMI_HDCP:
		{
			if (arg)
				hdmi_drv->enablehdcp(3);
			else
				hdmi_drv->enablehdcp(0);

			break;
		}
	case MTK_HDMI_HDCP_KEY:
		{
			if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_drv->hdcpkey((UINT8 *) &key);
			}
			break;
		}
	case MTK_HDMI_CONNECT_STATUS:
		{
			tmp = hdmi_drv->get_state();
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}
	case MTK_HDMI_GET_EDID:
		{
			hdmi_drv->getedid(&pv_get_info);
			if (copy_to_user((void __user *)arg, &pv_get_info, sizeof(pv_get_info))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}
	case MTK_MHL_GET_DCAP:
		{
			hdmi_drv->getdcapdata(pdata);
			if (copy_to_user((void __user *)arg, pdata, sizeof(pdata))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}
	case MTK_MHL_GET_3DINFO:
		{
			hdmi_drv->get3dinfo(&pv_3d_info);
			if (copy_to_user((void __user *)arg, &pv_3d_info, sizeof(pv_3d_info))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			break;
		}
	case MTK_HDMI_COLOR_DEEP:
		{
			#ifndef CONFIG_SLIMPORT_ANX3618
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
			} else {
				hdmi_colorspace = (unsigned char)(data_info.u4Data1 & 0xFF);
				if ((hdmi_res == HDMI_VIDEO_1920x1080p_60Hz)
				    || (hdmi_res == HDMI_VIDEO_1920x1080p_50Hz)
				    || (hdmi_res == HDMI_VIDEO_1280x720p3d_60Hz)
				    || (hdmi_res == HDMI_VIDEO_1280x720p3d_50Hz)
				    || (hdmi_res == HDMI_VIDEO_1920x1080p3d_24Hz)
				    || (hdmi_res == HDMI_VIDEO_1920x1080p3d_23Hz)
				    ) {
					hdmi_colorspace = HDMI_YCBCR_422;
				}
				hdmi_drv->colordeep(hdmi_colorspace);
			}
			DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace) (hdmi_colorspace, hdmi_res));
			#endif
			break;
		}
#endif
	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
		{
			#ifndef CONFIG_SLIMPORT_ANX3618
			if (arg) {
				if (p->is_enabled)
					return 0;

				HDMI_CHECK_RET(hdmi_drv_init());
				if (hdmi_drv->enter)
					hdmi_drv->enter();
				hdmi_power_on();
				p->is_enabled = true;
			} else {
				hdmi_video_buffer_info temp;
				if (!p->is_enabled)
					return 0;

				/* when disable hdmi, HPD is disabled */
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, 0);

				/* wait hdmi finish update */
				if (down_interruptible(&hdmi_update_mutex)) {
					HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
					return -EFAULT;
				}

				hdmi_rdma_address_config(false, temp);
				up(&hdmi_update_mutex);
				hdmi_power_off();

				/* wait hdmi finish update */
				if (down_interruptible(&hdmi_update_mutex)) {
					HDMI_LOG_ERR("can't get semaphore in %s()\n",
						 __func__);
					return -EFAULT;
				}
				HDMI_CHECK_RET(hdmi_drv_deinit());

				p->is_enabled = false;
				up(&hdmi_update_mutex);
				if (hdmi_drv->exit)
					hdmi_drv->exit();

			}
			#endif
			break;
		}
	case MTK_HDMI_FORCE_FULLSCREEN_ON:
	case MTK_HDMI_FORCE_CLOSE:
		{
#if (defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			/* Do nothing, keep HDMI/MHL Alive */
			/* For Camera and HDMI/MHL working at the same time */
#else
			RETIF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS, 0);
			RETIF(!p->is_enabled, 0);
			RETIF(IS_HDMI_OFF(), 0);

			if (p->is_force_disable == true)
				break;

			if (IS_HDMI_FAKE_PLUG_IN()) {
				hdmi_suspend();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, 0);
				switch_set_state(&hdmires_switch_data, 0);
			} else {
				if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
					hdmi_suspend();
					switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
					switch_set_state(&hdmi_audio_switch_data, 0);
					switch_set_state(&hdmires_switch_data, 0);
				}
			}

			p->is_force_disable = true;
#endif
			break;
		}

	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
	case MTK_HDMI_FORCE_OPEN:
		{
#if (defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			/* Do nothing, keep HDMI/MHL Alive */
			/* For Camera and HDMI/MHL working at the same time */
#else
			RETIF(p->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS, 0);
			RETIF(!p->is_enabled, 0);
			RETIF(IS_HDMI_OFF(), 0);

			if (p->is_force_disable == false)
				break;

			if (IS_HDMI_FAKE_PLUG_IN()) {
				hdmi_resume();
				msleep(1000);
				switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
				switch_set_state(&hdmi_audio_switch_data, 1);
				hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			} else {
				if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
					hdmi_resume();
					msleep(1000);
					switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
					switch_set_state(&hdmi_audio_switch_data, 1);
					hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
				}
			}
			p->is_force_disable = false;
#endif
			break;
		}
		/* Shutdown thread(No matter IPO), system suspend/resume will go this way... */
	case MTK_HDMI_POWER_ENABLE:
		{
			HDMI_LOG_DBG("%s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);
			RETIF(!p->is_enabled, 0);

			if (arg) {
				hdmi_power_on();
			} else {
				hdmi_power_off();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, 0);
				switch_set_state(&hdmires_switch_data, 0);
			}
			break;
		}
	case MTK_HDMI_AUDIO_ENABLE:
		{
			RETIF(!p->is_enabled, 0);

			if (arg)
				HDMI_CHECK_RET(hdmi_audio_enable(true));
			else
				HDMI_CHECK_RET(hdmi_audio_enable(false));

			break;
		}
	case MTK_HDMI_VIDEO_ENABLE:
		{
			RETIF(!p->is_enabled, 0);
			break;
		}
	case MTK_HDMI_VIDEO_CONFIG:
		{
			int switchres;
			int tmp = 0;
			int security = p->is_security_output;
			hdmi_video_buffer_info temp;

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			/* temporary solution for hdmi svp p1, always mute hdmi for svp */
			int svp = p->is_svp_output;
#endif
			HDMI_LOG_DBG("video resolution configuration, arg=%ld\n", arg);

#if (!defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)) && (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
			if (arg > 1) {
				arg = 1;
				HDMI_LOG_DBG("video resolution configuration, reset arg=%ld\n", arg);
			}
#endif

			RETIF(!p->is_enabled, 0);
			RETIF(IS_HDMI_NOT_ON(), 0);

			if (hdmi_reschange == arg) {
				HDMI_LOG_DBG("hdmi_reschange=%ld\n", hdmi_reschange);
				break;
			}

			if (hdmi_drv->mutehdmi)
				hdmi_drv->mutehdmi(security, false);

			hdmi_reschange = arg;
			p->is_clock_on = false;

			atomic_set(&hdmi_rdma_update_event, 1);
			wake_up_interruptible(&hdmi_rdma_update_wq);
			while (1) {
				if ((list_empty(&HDMI_Buffer_List)) || (tmp > 15)) {
					if (tmp > 15)
						HDMI_LOG_ERR(" Error HDMI_Buffer_List is not empty\n");
					break;
				} else
					msleep(20);

				tmp++;
			}

			RETIF(!p->is_enabled, 0);
			RETIF(IS_HDMI_NOT_ON(), 0);

			if (down_interruptible(&hdmi_update_mutex)) {
				HDMI_LOG_ERR("can't get semaphore in\n");
				return EAGAIN;
			}
#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			if ((arg == HDMI_VIDEO_1920x1080p_60Hz)
			    || (arg == HDMI_VIDEO_1920x1080p_50Hz)
			    || (arg == HDMI_VIDEO_1280x720p3d_60Hz)
			    || (arg == HDMI_VIDEO_1280x720p3d_50Hz)
			    || (arg == HDMI_VIDEO_1920x1080p3d_24Hz)
			    || (arg == HDMI_VIDEO_1920x1080p3d_23Hz)
			    ) {
				hdmi_colorspace = HDMI_YCBCR_422;
			} else {
				hdmi_colorspace = HDMI_RGB;
			}
			hdmi_drv->colordeep(hdmi_colorspace);
#endif

#if (defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT))
			if (hdmi_drv->tmdsonoff) {
				hdmi_mdelay(50);
				hdmi_drv->tmdsonoff(0);
			}
#endif

			hdmi_rdma_address_config(false, temp);
			hdmi_config_pll(arg);
			dpi_setting_res((u8) arg);
			hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888,  HDMI_VOUT_FORMAT_RGB888);

			if ((HDMI_VIDEO_1920x1080i_60Hz == p->output_video_resolution) ||
					(HDMI_VIDEO_1920x1080i_50Hz == p->output_video_resolution))
				RDMASetTargetLine(1, p->hdmi_height *  2 / 5);
			else
				RDMASetTargetLine(1, p->hdmi_height * 4 / 5);

			DPI_CHECK_RET(HDMI_DPI(_DisableClk) ());
#if (!defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) && !defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			DPI_CHECK_RET(HDMI_DPI(_ConfigHDMI) ());
#endif
			hdmi_dpi_config_update();
			up(&hdmi_update_mutex);
			p->is_clock_on = true;

			if ((p->is_security_output != security) && hdmi_drv->mutehdmi)
				hdmi_drv->mutehdmi(p->is_security_output, false);

			switchres = (p->hdmi_width << 16 | p->hdmi_height);
			switch_set_state(&hdmires_switch_data, switchres);

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
			/* temporary solution for hdmi svp p1, always mute hdmi for svp */
			if ((p->is_svp_output != svp) && hdmi_drv->svpmutehdmi)
				hdmi_drv->svpmutehdmi(p->is_svp_output, false);
#endif
			break;
		}
	case MTK_HDMI_AUDIO_CONFIG:
		{
			RETIF(!p->is_enabled, 0);

			break;
		}

	case MTK_HDMI_IS_FORCE_AWAKE:
		{
			if (!hdmi_drv_init_context()) {
				HDMI_LOG_ERR("hdmi_drv_init_context fail\n");
				return HDMI_STATUS_NOT_IMPLEMENTED;
			}
			r = copy_to_user(argp, &hdmi_params->is_force_awake,
					 sizeof(hdmi_params->is_force_awake)) ? -EFAULT : 0;
			break;
		}

	case MTK_HDMI_ENTER_VIDEO_MODE:
		{
			RETIF(!p->is_enabled, 0);
			RETIF(HDMI_OUTPUT_MODE_VIDEO_MODE != p->output_mode, 0);
			/* FIXME */
			/* hdmi_dst_display_path_config(true, NULL); */
			break;
		}

	case MTK_HDMI_REGISTER_VIDEO_BUFFER:
		{
			break;
		}

	case MTK_HDMI_POST_VIDEO_BUFFER:
		{
			hdmi_video_buffer_list *pBuffList;
			hdmi_video_buffer_info video_buffer_info;
			video_buffer_info.src_fmt = MTK_FB_FORMAT_RGB888;

			/* struct hdmi_video_buffer_list *buffer_list; */
			if ((p->is_enabled == false) || (p->is_clock_on == false) || IS_HDMI_NOT_ON()) {
				RETIF(!p->is_enabled, 0);
				RETIF(!p->is_clock_on, -1);
				RETIF(IS_HDMI_NOT_ON(), 0);
			}

			if (copy_from_user(&video_buffer_info, (void __user *)argp, sizeof(video_buffer_info))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
				break;
			}

			DBG_OnTriggerHDMI();

			if (p->is_clock_on) {
				pBuffList = kmalloc(sizeof(hdmi_video_buffer_list), GFP_KERNEL);
				spin_lock_bh(&hdmi_lock);
				pBuffList->buf_state = insert_new;
				pBuffList->fence = NULL;
				memcpy(&pBuffList->buffer_info, &video_buffer_info, sizeof(video_buffer_info));
				if (pBuffList->buffer_info.fenceFd >= 0)
					pBuffList->fence = sync_fence_fdget(pBuffList->buffer_info.fenceFd);

				INIT_LIST_HEAD(&pBuffList->list);
				list_add_tail(&pBuffList->list, &HDMI_Buffer_List);
				spin_unlock_bh(&hdmi_lock);

				if (dp_mutex_dst < 0) {
					atomic_set(&hdmi_rdma_config_event, 1);
					wake_up_interruptible(&hdmi_rdma_config_wq);
				}
			}
			break;
		}
	case MTK_HDMI_LEAVE_VIDEO_MODE:
		{
			RETIF(!p->is_enabled, 0);
			break;
		}

	case MTK_HDMI_FACTORY_MODE_ENABLE:
		{
			if (hdmi_drv->power_on()) {
				r = -EAGAIN;
				HDMI_LOG_ERR("Error factory mode test fail\n");
			} else {
				HDMI_LOG_DBG("before power off\n");
				hdmi_drv->power_off();
				HDMI_LOG_DBG("after power off\n");
			}
			break;
		}

	case MTK_HDMI_GET_DRM_ENABLE:
		{
			int drm_enable = 0;
			if (!hdmi_drv_init_context()) {
				HDMI_LOG_ERR("hdmi_drv_init_context fail\n");
				return HDMI_STATUS_NOT_IMPLEMENTED;
			}

			if ((p->is_clock_on == false) || (hdmi_drv->mutehdmi == NULL)) {
				HDMI_LOG_DBG("mutehdmi is null or disable %d\n", p->is_clock_on);
				drm_enable = 0;
			}
			if (hdmi_params->NeedSwHDCP == true)
				drm_enable = 1;

			r = copy_to_user(argp, &drm_enable, sizeof(drm_enable)) ? -EFAULT : 0;
			break;
		}

	case MTK_HDMI_GET_DEV_INFO:
		{
			int displayid = 0;
			mtk_dispif_info_t hdmi_info;
			HDMI_LOG_DBG("video resolution configuration get +\n");
			if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				return -EAGAIN;
			}
			if (displayid != MTKFB_DISPIF_HDMI) {
				HDMI_LOG_ERR("invalid display id:%d\n", displayid);
				return -EAGAIN;
			}
			hdmi_drv->getedid(&pv_get_info);
			memset(&hdmi_info, 0, sizeof(hdmi_info));
			hdmi_info.displayFormat = DISPIF_FORMAT_RGB888;
			hdmi_info.displayHeight = p->hdmi_height;
			hdmi_info.displayWidth = p->hdmi_width;
			hdmi_info.display_id = displayid;
			hdmi_info.isConnected = 1;
			hdmi_info.displayMode = DISPIF_MODE_COMMAND;
			/*hdmi_info.displayType = DISPIF_TYPE_DPI;*/
			/* Return connection type for HWC */
			if (1 == pv_get_info.ui1_Video_Input_Definitation)
				hdmi_info.displayType = DISPIF_TYPE_DPI;
			else
				hdmi_info.displayType = DISPIF_TYPE_DPI_VGA;
			hdmi_info.isHwVsyncAvailable = 1;
			hdmi_info.vsyncFPS = 60;
			if (copy_to_user((void __user *)arg, &hdmi_info, sizeof(hdmi_info))) {
				HDMI_LOG_ERR("copy_to_user failed!\n");
				r = -EFAULT;
			}
			HDMI_LOG_DBG("video resolution configuration get displayType-%d\n",
				 hdmi_info.displayType);
			break;
		}

	case MTK_HDMI_GET_FENCE_ID:
		{
			/* Not implement */
			break;
		}

	case MTK_HDMI_PREPARE_BUFFER:
		{
#ifdef ENABLE_HDMI_MTK_FENCE
			hdmi_buffer_info hdmi_buffer;
			unsigned int value = 0;
			int fenceFd = MTK_HDMI_NO_FENCE_FD;

			if (copy_from_user(&hdmi_buffer, (void __user *)arg, sizeof(hdmi_buffer))) {
				HDMI_LOG_ERR("copy_from_user failed!\n");
				r = -EFAULT;
				break;
			}

			if (down_interruptible(&hdmi_update_mutex)) {
				HDMI_LOG_ERR("can't get semaphore in\n");
				r = -EFAULT;
				break;
			}

			if (p->is_clock_on && IS_HDMI_ON())
				hdmi_create_fence(&fenceFd, &value);
			else
				HDMI_LOG_ERR("Error in hdmi_create_fence when is_clock_on is off\n");

			hdmi_buffer.fence_fd = fenceFd;
			hdmi_buffer.index = value;
			up(&hdmi_update_mutex);

			if (copy_to_user((void __user *)arg, &hdmi_buffer, sizeof(hdmi_buffer))) {
				HDMI_LOG_ERR("copy_to_user error!\n");
				r = -EFAULT;
			}
#endif
			break;
		}

	default:
		{
			HDMI_LOG_ERR("arguments error\n");
			break;
		}
	}

	return r;
}

#ifdef CONFIG_SLIMPORT_ANX3618
static int notifier_dongle_hdmi_status(struct notifier_block *nb, unsigned long status,
			     void *unused)
{
	HDMI_LOG_DBG("[hdmi][HDMI]notifier_dongle_hdmi_status,status=%ld\n", status);
	if (status == DONGLE_HDMI_POWER_ON) {
		if (p->is_enabled)
			return 0;

		HDMI_CHECK_RET(hdmi_drv_init());
		if (hdmi_drv->enter)
			hdmi_drv->enter();
		hdmi_power_on();
		p->is_enabled = true;
	} else if (status == DONGLE_HDMI_POWER_OFF) {
		hdmi_video_buffer_info temp;
		if (!p->is_enabled)
			return 0;

		/* when disable hdmi, HPD is disabled */
		switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
		switch_set_state(&hdmi_audio_switch_data, 0);
		switch_set_state(&hdmires_switch_data, 0);
		/* wait hdmi finish update */
		if (down_interruptible(&hdmi_update_mutex)) {
			HDMI_LOG_ERR("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
			return -EFAULT;
		}

		hdmi_rdma_address_config(false, temp);
		hdmi_dpi_power_switch(false);
		up(&hdmi_update_mutex);
		hdmi_power_off();

		/* wait hdmi finish update */
		if (down_interruptible(&hdmi_update_mutex)) {
			HDMI_LOG_ERR("[hdmi][HDMI] can't get semaphore in %s()\n",
				 __func__);
			return -EFAULT;
		}
		HDMI_CHECK_RET(hdmi_drv_deinit());

		p->is_enabled = false;
		up(&hdmi_update_mutex);
		if (hdmi_drv->exit)
			hdmi_drv->exit();

	}
	return -1;
}

static struct notifier_block notifier_dongle_block = {
	.notifier_call = notifier_dongle_hdmi_status,
};
#endif /*CONFIG_SLIMPORT_ANX3618*/
static int hdmi_remove(struct platform_device *pdev)
{
#ifdef CONFIG_SLIMPORT_ANX3618
	/* This call position may be needed modified */
	hdmi_driver_notifier_unregister(&notifier_dongle_block);
#endif /* CONFIG_SLIMPORT_ANX3618 */
	return 0;
}

static BOOL hdmi_drv_init_context(void)
{
	static const HDMI_UTIL_FUNCS hdmi_utils = {
		.udelay = hdmi_udelay,
		.mdelay = hdmi_mdelay,
		.state_callback = hdmi_state_callback,
	};

	if (hdmi_drv != NULL)
		return TRUE;

	hdmi_drv = (HDMI_DRIVER *) HDMI_GetDriver();

	if (NULL == hdmi_drv)
		return FALSE;

	hdmi_drv->set_util_funcs(&hdmi_utils);
	hdmi_drv->get_params(hdmi_params);

	return TRUE;
}

#ifdef CONFIG_SLIMPORT_ANX3618
#ifdef CONFIG_HAS_EARLYSUSPEND
int iMutexId = -1;
static void hdmi_early_suspend(struct early_suspend *h)
{
	hdmi_video_buffer_info temp;
	int iMutexEn = 1;
	iMutexId = dp_mutex_dst;

	HDMI_FUNC();
	RET_VOID_IF(IS_HDMI_NOT_ON());

	SET_HDMI_STANDBY();

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}

	if (DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN) == 0x0) {
		iMutexEn = 0;
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN, DDP_MUTEX_INTR_ENABLE_BIT);
	}

	hdmi_rdma_address_config(false, temp);

	if (iMutexEn == 0)
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN, 0x0);

	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(iMutexId), 0x0);

	if (p->is_rdma_clock_on) {
		disp_module_clock_off(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_off(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_off(DISP_MODULE_WDMA1, "HDMI");
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		p->is_rdma_clock_on = false;
	}

	up(&hdmi_update_mutex);
	return;
}

static void hdmi_late_resume(struct early_suspend *h)
{
	int tmp = 0;

	HDMI_FUNC();
	RET_VOID_IF(IS_HDMI_NOT_STANDBY());

	/* fresh buffer */
	atomic_set(&hdmi_rdma_update_event, 1);
	wake_up_interruptible(&hdmi_rdma_update_wq);
	while (1) {
		if ((list_empty(&HDMI_Buffer_List)) || (tmp > 15)) {
			if (tmp > 15)
				HDMI_LOG_ERR(" Error HDMI_Buffer_List is not empty\n");
			break;
		} else
			msleep(20);

		tmp++;
	}

	SET_HDMI_ON();

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG_ERR("can't get semaphore in %s()\n", __func__);
		return;
	}
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(iMutexId), 0x0);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(iMutexId), 1);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(iMutexId), 0);
	if (!p->is_rdma_clock_on) {
		enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
		disp_module_clock_on(DISP_MODULE_RDMA1, "HDMI");
		disp_module_clock_on(DISP_MODULE_GAMMA, "HDMI");
		disp_module_clock_on(DISP_MODULE_WDMA1, "HDMI");
		p->is_rdma_clock_on = true;
	}

	up(&hdmi_update_mutex);
	return;
}

static struct early_suspend hdmi_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING,
	.suspend = hdmi_early_suspend,
	.resume = hdmi_late_resume,
};
#endif
#endif

static void __exit hdmi_exit(void)
{
	hdmi_sync_destroy();
	device_destroy(hdmi_class, hdmi_devno);
	class_destroy(hdmi_class);
	cdev_del(hdmi_cdev);
	unregister_chrdev_region(hdmi_devno, 1);
#ifdef CONFIG_SLIMPORT_ANX3618
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hdmi_early_suspend_handler);
#endif
#endif
}

struct file_operations hdmi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdmi_ioctl,
	.open = hdmi_open,
	.release = hdmi_release,
};

static int hdmi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct class_device *class_dev = NULL;

	HDMI_FUNC();

	/* Allocate device number for hdmi driver */
	ret = alloc_chrdev_region(&hdmi_devno, 0, 1, HDMI_DEVNAME);
	if (ret) {
		HDMI_LOG_ERR("alloc_chrdev_region fail\n");
		return -1;
	}

	/* For character driver register to system, device number binded to file operations */
	hdmi_cdev = cdev_alloc();
	hdmi_cdev->owner = THIS_MODULE;
	hdmi_cdev->ops = &hdmi_fops;
	ret = cdev_add(hdmi_cdev, hdmi_devno, 1);

	/* For device number binded to device name(hdmitx), one class is corresponeded to one node */
	hdmi_class = class_create(THIS_MODULE, HDMI_DEVNAME);
	/* mknod /dev/hdmitx */
	class_dev =
	    (struct class_device *)device_create(hdmi_class, NULL, hdmi_devno, NULL, HDMI_DEVNAME);

	HDMI_LOG_DBG("current=0x%08x\n", (unsigned int)current);

	if (!hdmi_drv_init_context()) {
		HDMI_LOG_ERR("hdmi_drv_init_context fail\n");
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}

	init_waitqueue_head(&hdmi_rdma_config_wq);
	init_waitqueue_head(&hdmi_rdma_update_wq);
#ifdef CONFIG_SLIMPORT_ANX3618
	/*This call position may be needed modified*/
	hdmi_driver_notifier_register(&notifier_dongle_block);
#endif /*CONFIG_SLIMPORT_ANX3618*/

	return 0;
}

static struct platform_driver hdmi_driver = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.driver = {.name = HDMI_DEVNAME}
};

static int __init hdmi_init(void)
{
	int ret = 0;
	HDMI_FUNC();

#ifdef HDMI_BUFFER_KEEP_USE
	hdmi_va = 0;
	hdmi_mva_r = 0;
	temp_va = 0;
	temp_mva_w = 0;
#endif

	if (platform_driver_register(&hdmi_driver)) {
		HDMI_LOG_ERR("failed to register mtkfb driver\n");
		return -1;
	}

	memset((void *)&hdmi_context, 0, sizeof(_t_hdmi_context));
	SET_HDMI_OFF();

	if (!hdmi_drv_init_context()) {
		HDMI_LOG_ERR("hdmi_drv_init_context fail\n");
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}

	p->output_mode = hdmi_params->output_mode;
	p->is_security_output = 0;

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	/* temporary solution for hdmi svp p1, always mute hdmi for svp */
	p->is_svp_output = 0;
#endif

	p->orientation = 0;
	hdmi_drv->init();
	HDMI_LOG_DBG("Output mode is %s\n",
		 (hdmi_params->output_mode ==
		  HDMI_OUTPUT_MODE_DPI_BYPASS) ? "HDMI_OUTPUT_MODE_DPI_BYPASS" :
		 "HDMI_OUTPUT_MODE_LCD_MIRROR");

	if (hdmi_params->output_mode == HDMI_OUTPUT_MODE_DPI_BYPASS)
		p->output_video_resolution = HDMI_VIDEO_RESOLUTION_NUM;

	HDMI_DBG_Init();

	hdmi_switch_data.name = "hdmi";
	hdmi_switch_data.index = 0;
	hdmi_switch_data.state = NO_DEVICE;
	/* for support hdmi hotplug, inform AP the event */
	ret = switch_dev_register(&hdmi_switch_data);
#ifdef CONFIG_SLIMPORT_ANX3618
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hdmi_early_suspend_handler);
#endif
#endif
	hdmires_switch_data.name = "res_hdmi";
	hdmires_switch_data.index = 0;
	hdmires_switch_data.state = 0;
	/* for support hdmi hotplug, inform AP the event */
	ret = switch_dev_register(&hdmires_switch_data);

	if (ret) {
		HDMI_LOG_DBG("switch_dev_register returned:%d!\n", ret);
		return 1;
	}

	hdmi_sync_init();
	INIT_LIST_HEAD(&HDMI_Buffer_List);

	return 0;
}
module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_AUTHOR("Xuecheng, Zhang <xuecheng.zhang@mediatek.com>");
MODULE_DESCRIPTION("HDMI Driver");
MODULE_LICENSE("GPL");

#endif
