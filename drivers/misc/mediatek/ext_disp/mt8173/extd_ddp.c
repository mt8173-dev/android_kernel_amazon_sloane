#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/hdmitx.h>
#include <linux/ktime.h>

#include "mtkfb_info.h"
#include "mtkfb.h"
#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_info.h"
#include <mach/m4u.h>
#include <mach/m4u_port.h>
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_manager.h"
#include "ddp_mmp.h"
#include "ddp_dpi.h"
#include "extd_platform.h"
/*#include "extd_drv.h"*/
#include "extd_drv_log.h"
#include "extd_lcm.h"
#include "extd_utils.h"
#include "extd_ddp.h"
#include "extd_kernel_drv.h"
#include "disp_session.h"
#include "display_recorder.h"

#include <linux/ion_drv.h>
#include <linux/slab.h>
#include <linux/mtk_ion.h>
#include "disp_drv_platform.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#include "ddp_debug.h"
#endif

#include "mtkfb_fence.h"
#include "mtk_sync.h"

#include "ddp_rdma.h"
/*for irq handle register*/
#include "ddp_irq.h"
#include "ddp_reg.h"


#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
#define DISP_INTERNAL_BUFFER_COUNT 7
#define DISP_SPLIT_COUNT 1
#define HDMI_MAX_WIDTH 1920
#define HDMI_MAX_HEIGHT 1080
#define DISP_SPLIT_BUFFER_COUNT 7
#else
#define DISP_INTERNAL_BUFFER_COUNT 3
/* split 4k buffer into non-4k buffer(according to 1080p res), max count is 4, support 2/3/4 now */
#define DISP_SPLIT_COUNT 4
#define HDMI_MAX_WIDTH 4096
#define HDMI_MAX_HEIGHT 2160
#define DISP_SPLIT_BUFFER_COUNT 7/*(DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT)*/
#endif

static ktime_t vsync_time;
static unsigned int wdma_frame_done_buf_idx;
unsigned int g_ext_PresentFenceIndex = 0;

static struct mutex vsync_mtx;

unsigned long framebuffer_mva;
unsigned long framebuffer_va;

BOOL ext_is_early_suspend = false;
static int first_build_path_decouple = 1;
static int first_build_path_lk_kernel = 1;
static unsigned long dc_vAddr[DISP_INTERNAL_BUFFER_COUNT];
static unsigned long split_dc_vAddr[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
static struct disp_internal_buffer_info *decouple_buffer_info[DISP_INTERNAL_BUFFER_COUNT];

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
DISP_BUFFER_TYPE g_wdma_security = DISP_NORMAL_BUFFER;
static DISP_BUFFER_TYPE g_rdma_security = DISP_NORMAL_BUFFER;
unsigned int gDebugSvpHdmiAlwaysSec = 0;
#endif

struct disp_internal_buffer_info {
	struct list_head list;
	struct ion_handle *handle;
	struct sync_fence *pfence;
	uint32_t fence_id;
	uint32_t mva;
	void *va;
	uint32_t size;
	uint32_t output_fence_id;
	uint32_t interface_fence_id;
	unsigned long long timestamp;
};

extern int dprec_mmp_dump_ovl_layer(OVL_CONFIG_STRUCT * ovl_layer, unsigned int l,
				    unsigned int session /*1:primary, 2:external, 3:memory */);
int ext_disp_use_cmdq = CMDQ_ENABLE;
int ext_disp_use_m4u = 1;
#if HDMI_SUB_PATH
EXT_DISP_PATH_MODE ext_disp_mode = EXTD_DECOUPLE_MODE;
#else
EXT_DISP_PATH_MODE ext_disp_mode = EXTD_DECOUPLE_MODE;
#endif
static int ext_disp_use_frc; /* disable FRC by default */

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
EXT_DISP_PATH_MODE hdmi_ext_disp_mode = EXTD_DIRECT_LINK_MODE; /* use DL mode as default */
#endif

#define ALIGN_TO(x, n)  \
    (((x) + ((n) - 1)) & ~((n) - 1))

enum EXTD_POWER_STATE {
	EXTD_DEINIT = 0,
	EXTD_INIT,
	EXTD_RESUME,
	EXTD_SUSPEND
};

typedef struct {
	enum EXTD_POWER_STATE state;
	int init;
	unsigned int session;
	int need_trigger_overlay;
	EXT_DISP_PATH_MODE mode;
	unsigned int last_vsync_tick;
	struct mutex lock;
	extd_drv_handle *plcm;
	cmdqRecHandle cmdq_handle_config;
	cmdqRecHandle cmdq_rdma_handle_config;
	cmdqRecHandle cmdq_handle_trigger;
	disp_path_handle dpmgr_handle;
	disp_path_handle ovl2mem_path_handle;
	unsigned int dc_buf_id;
	unsigned int dc_rdma_buf_id;
	unsigned int dc_buf[DISP_INTERNAL_BUFFER_COUNT];
	unsigned int dc_split_buf[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	unsigned int cur_wdma_sec_buf_id; /* get wdma secure buffer offset */
	unsigned int cur_rdma_sec_buf_id; /* get rdma secure buffer offset */
	unsigned int dc_sec_buf[DISP_INTERNAL_BUFFER_COUNT];
	unsigned int dc_split_sec_buf[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
#endif
	unsigned long long buf_pts[DISP_INTERNAL_BUFFER_COUNT];
	unsigned long long split_buf_pts[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
	unsigned long long pts;
	cmdqBackupSlotHandle cur_config_fence;
	cmdqBackupSlotHandle subtractor_when_free;
} ext_disp_path_context;

static long long ovl_frame_pts;
unsigned long long g_ovl_drop_frame_count;
unsigned long long g_ovl_repeated_frame_count;
unsigned long long g_rdma_drop_frame_count;
unsigned long long g_rdma_repeated_frame_count;

#define pgc	_get_context()

static int is_context_inited;

static int ovl_wdma_trigger_count;
static int rdma_trigger_count;
int is_hdmi_rdma_stop = 1;
extern void config_hdmitx_timing(void);

static void clear_ext_disp_path_context(ext_disp_path_context *pcontext)
{
	/* clear all members except wdma/rdma mva */
	pcontext->state = 0;
	pcontext->init = 0;
	pcontext->session = 0;
	pcontext->need_trigger_overlay = 0;
	pcontext->mode = 0;
	pcontext->last_vsync_tick = 0;
	memset((void *)&pcontext->lock, 0, sizeof(pcontext->lock));
	/*pcontext->plcm = NULL;*/
	pcontext->cmdq_handle_config = NULL;
	pcontext->cmdq_rdma_handle_config = NULL;
	pcontext->cmdq_handle_trigger = NULL;
	pcontext->dpmgr_handle = NULL;
	pcontext->ovl2mem_path_handle = NULL;
	pcontext->dc_buf_id = 0;
	pcontext->dc_rdma_buf_id = 0;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	pcontext->cur_wdma_sec_buf_id = 0;
	pcontext->cur_rdma_sec_buf_id = 0;
#endif
	pcontext->cur_config_fence = 0;
	pcontext->subtractor_when_free = 0;
}

static ext_disp_path_context *_get_context(void)
{
	static ext_disp_path_context g_context;

	if (!is_context_inited) {
		clear_ext_disp_path_context(&g_context);
		is_context_inited = 1;
		DISPMSG("_get_context set is_context_inited\n");
	}

	return &g_context;
}

EXT_DISP_PATH_MODE get_ext_disp_path_mode(void)
{
	return ext_disp_mode;
}

static void _ext_disp_path_lock(void)
{
	extd_sw_mutex_lock(NULL);	/* /(&(pgc->lock)); */
}

static void _ext_disp_path_unlock(void)
{
	extd_sw_mutex_unlock(NULL);	/* (&(pgc->lock)); */
}

static DISP_MODULE_ENUM _get_dst_module_by_lcm(extd_drv_handle *plcm)
{
	if (plcm == NULL) {
		DISPERR("plcm is null\n");
		return DISP_MODULE_UNKNOWN;
	}

	if (plcm->params->type == LCM_TYPE_DSI) {
		if (plcm->lcm_if_id == LCM_INTERFACE_DSI0) {
			return DISP_MODULE_DSI0;
		} else if (plcm->lcm_if_id == LCM_INTERFACE_DSI1) {
			return DISP_MODULE_DSI1;
		} else if (plcm->lcm_if_id == LCM_INTERFACE_DSI_DUAL) {
			return DISP_MODULE_DSIDUAL;
		} else {
			return DISP_MODULE_DSI0;
		}
	} else if (plcm->params->type == LCM_TYPE_DPI) {
		return DISP_MODULE_DPI1;
	} else {
		DISPERR("can't find ext display path dst module\n");
		return DISP_MODULE_UNKNOWN;
	}
}

/***************************************************************
***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 1.wait idle:           N         N       Y        Y
*** 2.lcm update:          N         Y       N        Y
*** 3.path start:	idle->Y      Y    idle->Y     Y
*** 4.path trigger:     idle->Y      Y    idle->Y     Y
*** 5.mutex enable:        N         N    idle->Y     Y
*** 6.set cmdq dirty:      N         Y       N        N
*** 7.flush cmdq:          Y         Y       N        N
****************************************************************/

static int _should_wait_path_idle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	*** 1.wait idle:	          N         N        Y        Y					*/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 0;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		} else {
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		}
	}
}

static int _should_update_lcm(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 2.lcm update:          N         Y       N        Y        **/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			/* TODO: lcm_update can't use cmdq now */
			return 0;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 1;
		}
	}
}

static int _should_start_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 3.path start:	idle->Y      Y    idle->Y     Y        ***/

#if HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	return 1;
#else
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		} else {
			return 1;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		} else {
			return 1;
		}
	}
#endif
}

static int _should_trigger_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 4.path trigger:     idle->Y      Y    idle->Y     Y
*** 5.mutex enable:        N         N    idle->Y     Y        ***/

	/* this is not a perfect design, we can't decide path trigger(ovl/rdma/dsi..) seperately with mutex enable */
	/* but it's lucky because path trigger and mutex enable is the same w/o cmdq, and it's correct w/ CMDQ(Y+N). */

#if HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	return 1;
#else
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		} else {
			return 0;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		} else {
			return 1;
		}
	}
#endif
}

static int _should_set_cmdq_dirty(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 6.set cmdq dirty:	    N         Y       N        N     ***/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 1;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 0;
		}
	}
}

static int _should_flush_cmdq_config_handle(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N        ***/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 1;
		} else {
			return 1;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 0;
		}
	}
}

static int _should_reset_cmdq_config_handle(void)
{
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 1;
		} else {
			return 1;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 0;
		}
	}
}

static int _should_insert_wait_frame_done_token(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N      */
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode()) {
			return 1;
		} else {
			return 1;
		}
	} else {
		if (ext_disp_is_video_mode()) {
			return 0;
		} else {
			return 0;
		}
	}
}

static int _should_trigger_interface(void)
{
	if (pgc->mode == EXTD_DECOUPLE_MODE) {
		return 0;
	} else {
		return 1;
	}
}

static int _should_config_ovl_input(void)
{
	/* should extend this when display path dynamic switch is ready */
	if (pgc->mode == EXTD_SINGLE_LAYER_MODE || pgc->mode == EXTD_DEBUG_RDMA_DPI_MODE)
		return 0;
	else
		return 1;

}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
#if 0
static enum hrtimer_restart _DISP_CmdModeTimer_handler(struct hrtimer *timer)
{
	DISPMSG("fake timer, wake up\n");
	dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
#if 0
	if ((get_current_time_us() - pgc->last_vsync_tick) > 16666) {
		dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
		pgc->last_vsync_tick = get_current_time_us();
	}
#endif
	return HRTIMER_RESTART;
}

static int _init_vsync_fake_monitor(int fps)
{
	static struct hrtimer cmd_mode_update_timer;
	static ktime_t cmd_mode_update_timer_period;

	if (fps == 0)
		fps = 60;

	cmd_mode_update_timer_period = ktime_set(0, 1000 / fps * 1000);
	DISPMSG("[MTKFB] vsync timer_period=%d\n", 1000 / fps);
	hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;

	return 0;
}
#endif

static int config_display_m4u_port(M4U_PORT_ID id, DISP_MODULE_ENUM module)
{
	int ret;
	M4U_PORT_STRUCT sPort;

	sPort.ePortID = id;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret != 0) {
		DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",
			  ddp_get_module_name(module),
			  ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

	return 0;
}

int ext_disp_config_m4u_port(void)
{
	DISPFUNC();
	/* config m4u port used by hdmi just once */
	config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1);
	config_display_m4u_port(M4U_PORT_DISP_RDMA1, DISP_MODULE_RDMA1);
	config_display_m4u_port(M4U_PORT_DISP_WDMA1, DISP_MODULE_WDMA1);

	return 0;
}

int ext_disp_common_init(void)
{
	dprec_init();

	disp_sync_init();

	dpmgr_init();

	return 0;
}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;
	disp_ddp_path_config *data_config = NULL;

	DISPFUNC();
	pgc->mode = EXTD_DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	/* mark for svp, config in primary_display just once */
	/* config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1); */

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, ext_disp_is_video_mode());
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	data_config->dst_w = hdmi_get_width();
	data_config->dst_h = hdmi_get_height();
	if (hdmi_is_interlace)
		data_config->dst_h /= 2;
	data_config->dst_dirty = 1;

	/* config rdma from decouple to direct link */
	data_config->rdma_config.address = 0;
	data_config->rdma_config.pitch = 0;
	data_config->rdma_dirty = 1;
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_config(pgc->dpmgr_handle, data_config, CMDQ_DISABLE);

	/* debug ovl1 */
	ovl_dump_reg(DISP_MODULE_OVL1);

	return ret;
}

static unsigned int extd_display_get_width(void)
{
	return hdmi_get_width();
}

static unsigned int extd_display_get_height(void)
{
	return hdmi_get_height();
}

static unsigned int extd_display_get_bpp(void)
{
	return 24;
}

int _is_hdmi_decouple_mode(EXT_DISP_PATH_MODE mode)
{
	if (mode == EXTD_DECOUPLE_MODE)
		return 1;
	else
		return 0;
}

int ext_disp_is_decouple_mode(void)
{
	return _is_hdmi_decouple_mode(pgc->mode);
}

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
int ext_disp_get_mutex_id(void)
{
	return dpmgr_path_get_mutex(pgc->dpmgr_handle);
}
#endif

static struct disp_internal_buffer_info *allocate_decouple_buffer(unsigned int size)
{
	void *buffer_va = NULL;
	unsigned int buffer_mva = 0;
	unsigned int mva_size = 0;
	struct ion_mm_data mm_data;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	struct disp_internal_buffer_info *buf_info = NULL;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	if (gDebugSvpHdmiAlwaysSec == 1) {
		DISPMSG("decouple NORMAL buffer : don't allocate, ALWAYS use secure!\n");
		return NULL;
	}
#endif

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	client = ion_client_create(g_ion_device, "disp_decouple");
	buf_info = kzalloc(sizeof(struct disp_internal_buffer_info), GFP_KERNEL);
	if (buf_info) {
		handle = ion_alloc(client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
		if (IS_ERR(handle)) {
			DISPERR("Fatal Error, ion_alloc for size %d failed\n", size);
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		buffer_va = ion_map_kernel(client, handle);
		if (buffer_va == NULL) {
			DISPERR("ion_map_kernrl failed\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}
		mm_data.config_buffer_param.kernel_handle = handle;
		mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
		if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0) {
			DISPERR("ion_test_drv: Config buffer failed.\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		ion_phys(client, handle, (ion_phys_addr_t *) &buffer_mva, (size_t *) &mva_size);
		if (buffer_mva == 0) {
			DISPERR("Fatal Error, get mva failed\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}
		buf_info->handle = handle;
		buf_info->mva = buffer_mva;
		buf_info->size = mva_size;
		buf_info->va = buffer_va;
	} else {
		DISPERR("Fatal error, kzalloc internal buffer info failed!!\n");
		kfree(buf_info);
		return NULL;
	}

	return buf_info;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
KREE_SESSION_HANDLE extd_disp_secure_memory_session_handle(void)
{
	static KREE_SESSION_HANDLE disp_secure_memory_session;

	/* TODO: the race condition here is not taken into consideration. */
	if (!disp_secure_memory_session) {
		TZ_RESULT ret;
		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &disp_secure_memory_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISPERR("KREE_CreateSession fail, ret=%d\n", ret);
			return 0;
		}
	}
	DISPDBG("disp_secure_memory_session_handle() session = %x\n",
			    (unsigned int)disp_secure_memory_session);

	return disp_secure_memory_session;
}

static KREE_SECUREMEM_HANDLE allocate_decouple_sec_buffer(unsigned int buffer_size)
{
	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle;

	/* allocate secure buffer by tz api */
	ret = KREE_AllocSecurechunkmem(extd_disp_secure_memory_session_handle(),
			&mem_handle, 0, buffer_size);
	if (ret != TZ_RESULT_SUCCESS) {
		DISPERR("KREE_AllocSecurechunkmem fail, ret = 0x%x\n", ret);
		return -1;
	}
	DISPDBG("KREE_AllocSecurchunkemem handle = 0x%x\n", mem_handle);

	return mem_handle;
}
#endif

int ext_disp_set_frame_buffer_address(unsigned long va, unsigned long mva)
{

	DISPMSG("extd disp framebuffer va 0x%lx, mva 0x%lx\n", va, mva);
	framebuffer_va = va;
	framebuffer_mva = mva;

	return 0;
}

unsigned long ext_disp_get_frame_buffer_mva_address(void)
{
	return framebuffer_mva;
}

unsigned long ext_disp_get_frame_buffer_va_address(void)
{
	return framebuffer_va;
}

static void init_decouple_buffers(void)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	/* allocate normal buffer */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		decouple_buffer_info[i] = allocate_decouple_buffer(buffer_size);
		if (decouple_buffer_info[i] != NULL) {
			pgc->dc_buf[i] = decouple_buffer_info[i]->mva;
			dc_vAddr[i] = (unsigned long)decouple_buffer_info[i]->va;
			DISPMSG("decouple NORMAL buffer : pgc->dc_buf[%d] = 0x%x dc_vAddr[%d] = 0x%lx\n", i,
				pgc->dc_buf[i], i, dc_vAddr[i]);
		}
	}

	/* split 4k normal buffer in non-4k resolution */
	/* eg. if 4k buffer is 0/1, splited into 2 non-4k buffer is 0/1/2/3 */
	/* eg. if 4k buffer is 0/1, splited into 4 non-4k buffer is 0/1/2/3/4/5/6/7 */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		if (decouple_buffer_info[i] != NULL) {
			for (j = 0; j < DISP_SPLIT_COUNT; j++) {
				pgc->dc_split_buf[DISP_SPLIT_COUNT*i+j] =
					pgc->dc_buf[i] + buffer_size/DISP_SPLIT_COUNT*j;
				DISPMSG("decouple SPLIT NORMAL buffer : pgc->dc_split_buf[%d] = 0x%x\n",
					DISP_SPLIT_COUNT*i+j, pgc->dc_split_buf[DISP_SPLIT_COUNT*i+j]);
			}
		}
	}

	/* split normal buffer va for dump */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		if (decouple_buffer_info[i] != NULL) {
			for (j = 0; j < DISP_SPLIT_COUNT; j++) {
				split_dc_vAddr[DISP_SPLIT_COUNT*i+j] =
					dc_vAddr[i] + buffer_size/DISP_SPLIT_COUNT*j;
				DISPMSG("decouple SPLIT NORMAL buffer(va) : split_dc_vAddr[%d] = 0x%lx\n",
					DISP_SPLIT_COUNT*i+j, split_dc_vAddr[DISP_SPLIT_COUNT*i+j]);
			}
		}
	}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* allocate secure buffer */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		pgc->dc_sec_buf[i] = allocate_decouple_sec_buffer(buffer_size);
		DISPMSG("decouple SECURE buffer : pgc->dc_sec_buf[%d] = 0x%x\n", i, pgc->dc_sec_buf[i]);
	}

	/* split 4k secure buffer in non-4k resolution */
	/* eg. if 4k buffer is 0/1, splited 2 non-4k buffer is 0/0/1/1 */
	/* eg. if 4k buffer is 0/1, splited 4 non-4k buffer is 0/0/0/0/1/1/1/1 */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT; i++) {
		pgc->dc_split_sec_buf[i] = pgc->dc_sec_buf[i/DISP_SPLIT_COUNT];
		DISPMSG("decouple SPLIT SECURE buffer : pgc->dc_split_sec_buf[%d] = 0x%x\n", i,
			pgc->dc_split_sec_buf[i]);
	}
#endif
}

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
#else
static void copy_fbmem_to_dc_buffer(void)
{
	unsigned int ext_w = extd_display_get_width();
	unsigned int ext_h = extd_display_get_height();
	unsigned int primary_w = primary_display_get_width();
	unsigned int primary_h = primary_display_get_height();
	void *fb_va = (void *)ext_disp_get_frame_buffer_va_address();
	unsigned int size = ext_w * ext_h * 4;
	if (ext_w > primary_w || ext_h > primary_h)
		size = primary_w * primary_h * 4;
	DISPMSG("extd w %u h %u size %u fb_va 0x%p dc_vAddr[0] 0x%lx\n", ext_w, ext_h, size, fb_va, dc_vAddr[0]);
	memcpy((void *)dc_vAddr[0], fb_va, size);
}
#endif

static void clear_decouple_internal_buffer(void)
{
	int i;

	DISPFUNC();
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++)
		memset((void *)dc_vAddr[i], 0x0, HDMI_MAX_WIDTH * HDMI_MAX_HEIGHT * 3);
}

static int _build_path_decouple(void)
{
	int ret = 0;
	disp_ddp_path_config *pconfig = NULL;
	DISP_MODULE_ENUM dst_module = 0;
	uint32_t writing_mva = 0;
	unsigned rdma_bpp = 3;
	DpColorFormat rdma_fmt = eRGB888;

	DISPFUNC();
	pgc->mode = EXTD_DECOUPLE_MODE;

	pgc->dpmgr_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP, pgc->cmdq_rdma_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create interface path SUCCESS(0x%lx)\n",
			  (unsigned long)pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
#ifdef CONFIG_USE_CMDQ
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_ENABLE);
#else
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);
#endif

	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	pgc->ovl2mem_path_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_OVL_MEMOUT, pgc->cmdq_handle_config);
	if (pgc->ovl2mem_path_handle) {
		DISPCHECK("dpmgr create ovl memout path SUCCESS(0x%lx)\n",
			  (unsigned long)pgc->ovl2mem_path_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
#ifdef CONFIG_USE_CMDQ
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
#else
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
#endif

	/* config used module m4u port */
	/* mark for svp, config in primary_display just once */
#if 0
	config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1);
	config_display_m4u_port(M4U_PORT_DISP_RDMA1, DISP_MODULE_RDMA1);
	config_display_m4u_port(M4U_PORT_DISP_WDMA1, DISP_MODULE_WDMA1);
#endif

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	mutex_lock(&vsync_mtx);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	mutex_unlock(&vsync_mtx);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);
	dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START);

	if (first_build_path_decouple) {
		DISPMSG("first_build_path_decouple just come here once!!!!\n");
		init_decouple_buffers();
	}

	clear_decouple_internal_buffer();

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
#else
	if (first_build_path_decouple) {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (!gDebugSvpHdmiAlwaysSec)
			copy_fbmem_to_dc_buffer();
#else
		copy_fbmem_to_dc_buffer();
#endif
	}
#endif

	/* ovl need dst_dirty to set background color */
	pconfig = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	pconfig->dst_w = extd_display_get_width();
	pconfig->dst_h = extd_display_get_height();
	pconfig->dst_dirty = 1;
#ifdef CONFIG_USE_CMDQ
	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, pgc->cmdq_handle_config);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
#else
	dpmgr_path_reset(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, NULL);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
#endif

	/* config rdma1 to load logo from lk */
	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	/* need get dpi params, otherwise will don't wait mutex1 eof */
	memcpy(&(pconfig->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));

	if (first_build_path_decouple) {
		writing_mva = pgc->dc_buf[0];
		rdma_fmt = eBGRA8888;
		rdma_bpp = 4;
		pgc->dc_rdma_buf_id = 1;
		pgc->dc_buf_id = 1;

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
		writing_mva = ext_disp_get_frame_buffer_mva_address();
#else
		#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (gDebugSvpHdmiAlwaysSec)
			writing_mva = ext_disp_get_frame_buffer_mva_address();
		#endif
#endif

		pconfig->rdma_config.address = (unsigned int)writing_mva;
		first_build_path_decouple = 0;
	}
	pconfig->rdma_config.width = extd_display_get_width();
	pconfig->rdma_config.height = extd_display_get_height();
	pconfig->rdma_config.inputFormat = rdma_fmt;
	pconfig->rdma_config.pitch = extd_display_get_width() * rdma_bpp;
	pconfig->rdma_dirty = 1;
#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	/* config color1/gamma */
	pconfig->dst_dirty = 1;
#endif
#ifdef CONFIG_USE_CMDQ
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, pgc->cmdq_handle_config);
#else
	ret = dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, CMDQ_DISABLE);
	/* need start and trigger */
	ret = dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	ret = dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	/* just test show boot logo on tablet */
	/* msleep(3000); */
#endif

	DISPMSG("build_path_decouple dst(%d %d) rdma(%d %d) hdmi(%d %d)\n",
		pconfig->dst_w,
		pconfig->dst_h,
		pconfig->rdma_config.width,
		pconfig->rdma_config.height, extd_display_get_width(), extd_display_get_height());

	DISPCHECK("build decouple path finished\n");
	return ret;
}

static int _build_path_single_layer(void)
{
	return 0;
}

static int _build_path_debug_rdma_dpi(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = EXTD_DEBUG_RDMA_DPI_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA2_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	config_display_m4u_port(M4U_PORT_DISP_RDMA2, DISP_MODULE_RDMA2);

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;
	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
	DISPMSG("ext_disp path trigger thread cmd handle=0x%p\n", pgc->cmdq_handle_trigger);
	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (ext_disp_is_video_mode()) {
		/* wait and clear stream_done, HW will assert mutex enable automatically in frame done reset. */
		/* todo: should let dpmanager to decide wait which mutex's eof. */
		ret =
		    cmdqRecWait(pgc->cmdq_handle_trigger,
				dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				CMDQ_EVENT_MUTEX0_STREAM_EOF);
		/* /dpmgr_path_get_mutex(pgc->dpmgr_handle) */

		/* for some module(like COLOR) to read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF);
	} else {
		/* DSI command mode doesn't have mutex_stream_eof, need use CMDQ token instead */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		/* ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MDP_DSI0_TE_SOF); */
		/* for operations before frame transfer, such as waiting for DSI TE */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_BEFORE_STREAM_SOF);

		/* cleat frame done token, now the config thread will not allowed to config registers. */
		/* remember that config thread's priority is higher than trigger thread, so all the config queued before will be applied then STREAM_EOF token be cleared */
		/* this is what CMDQ did as "Merge" */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* enable mutex, only cmd mode need this */
		/* this is what CMDQ did as "Trigger" */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger, CMDQ_ENABLE);
		/* ret = cmdqRecWrite(pgc->cmdq_handle_trigger, (unsigned int)(DISP_REG_CONFIG_MUTEX_EN(0))&0x1fffffff, 1, ~0); */

		/* waiting for frame done, because we can't use mutex stream eof here, so need to let dpmanager help to decide which event to wait */
		/* most time we wait rdmax frame done event. */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA1_EOF);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_STREAM_EOF_EVENT);

		/* dsi is not idle rightly after rdma frame done, so we need to polling about 1us for dsi returns to idle */
		/* do not polling dsi idle directly which will decrease CMDQ performance */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_CHECK_IDLE_AFTER_STREAM_EOF);

		/* for some module(like COLOR) to read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF);

		/* polling DSI idle */
		/* ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x1401b00c, 0, 0x80000000); */
		/* polling wdma frame done */
		/* ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x140060A0, 1, 0x1); */

		/* now frame done, config thread is allowed to config register now */
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* RUN forever!!!! */
		BUG_ON(ret < 0);
	}

	/* dump trigger loop instructions to check whether dpmgr_path_build_cmdq works correctly */
	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	DISPCHECK("ext display BUILD cmdq trigger loop finished\n");

	return;
}

static void _cmdq_start_trigger_loop(void)
{
	int ret = 0;

	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!ext_disp_is_video_mode()) {
		/* need to set STREAM_EOF for the first time, otherwise we will stuck in dead loop */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		/* /dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW); */
	}

	DISPCHECK("START cmdq trigger loop finished\n");
}

static void _cmdq_stop_trigger_loop(void)
{
	int ret = 0;

	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);

	DISPCHECK("ext display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if (!ext_disp_is_video_mode()) {
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		/* /dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY); */
	}
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	/* /dprec_event_op(DPREC_EVENT_CMDQ_RESET); */
}

static void _cmdq_flush_config_handle(int blocking, void *callback, unsigned int userdata)
{
	if (blocking)
		cmdqRecFlush(pgc->cmdq_handle_config);	/* it will be blocked until mutex done */
	else {
		if (callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config, callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}
	/* dprec_event_op(DPREC_EVENT_CMDQ_FLUSH); */
}

static void _cmdq_insert_wait_frame_done_token(void)
{
	if (ext_disp_is_video_mode()) {
		cmdqRecWaitNoClear(pgc->cmdq_handle_config,
				   dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				   CMDQ_EVENT_MUTEX0_STREAM_EOF);
		/* /CMDQ_EVENT_MUTEX1_STREAM_EOF  dpmgr_path_get_mutex() */
	} else {
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}

	/* /dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF); */
}

static void _cmdq_insert_wait_frame_done_token_mira(void *handle)
{
	/* pgc->dpmgr_handle use DDP_SCENARIO_SUB_RDMA1_DISP scenario */
	if (ext_disp_is_video_mode())
		cmdqRecWaitNoClear(handle,
			dpmgr_path_get_mutex(pgc->dpmgr_handle) + CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
}

static int _convert_disp_input_to_rdma(RDMA_CONFIG_STRUCT *dst, ext_disp_input_config *src)
{
	if (src && dst) {
		dst->inputFormat = src->fmt;
		dst->address = src->addr;
		dst->width = src->src_w;
		dst->height = src->src_h;
		dst->pitch = src->src_pitch;

		return 0;
	} else {
		DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
		return -1;
	}
}

static int _convert_disp_input_to_ovl(OVL_CONFIG_STRUCT *dst, ext_disp_input_config *src)
{
	if (src && dst) {
		dst->layer = src->layer;
		dst->layer_en = src->layer_en;
		dst->fmt = src->fmt;
		dst->addr = src->addr;
		dst->vaddr = src->vaddr;
		dst->src_x = src->src_x;
		dst->src_y = src->src_y;
		dst->src_w = src->src_w;
		dst->src_h = src->src_h;
		dst->src_pitch = src->src_pitch;
		dst->dst_x = src->dst_x;
		dst->dst_y = src->dst_y;
		dst->dst_w = src->dst_w;
		dst->dst_h = src->dst_h;
		dst->keyEn = src->keyEn;
		dst->key = src->key;
		dst->aen = src->aen;
		dst->alpha = src->alpha;

		dst->sur_aen = src->sur_aen;
		dst->src_alpha = src->src_alpha;
		dst->dst_alpha = src->dst_alpha;

		dst->isDirty = src->isDirty;

		dst->buff_idx = src->buff_idx;
		dst->identity = src->identity;
		dst->connected_type = src->connected_type;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		dst->security = src->security;
		/* just for test svp */
		/*if (dst->layer == 0 && gDebugSvp == 2)*/
		/*dst->security = DISP_SECURE_BUFFER;*/
#endif

		dst->yuv_range = src->yuv_range;

		if (hdmi_is_interlace && !_is_hdmi_decouple_mode(pgc->mode)) {
			dst->dst_h /= 2;
			dst->src_pitch *= 2;
		}

		return 0;
	} else {
		DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
		return -1;
	}
}


static int _trigger_display_interface(int blocking, void *callback, unsigned int userdata)
{
	/* /DISPFUNC(); */
	/* int i = 0; */

	bool reg_flush = false;
	if (_should_wait_path_idle()) {
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);
	}

	if (_should_update_lcm()) {
		extd_drv_update(pgc->plcm, 0, 0, pgc->plcm->params->width,
				pgc->plcm->params->height, 0);
	}

	if (_should_start_path()) {
		reg_flush = true;
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagPulse, Trigger, 1);
	}

	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop, which should always be NULL for config thread */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, ext_disp_cmdq_enabled());
	}

	if (_should_set_cmdq_dirty()) {
		_cmdq_set_config_handle_dirty();
	}
	/* /if(reg_flush == false) */
	{
#if 0
		if (reg_flush == false) {
			if (_should_insert_wait_frame_done_token())
				_cmdq_insert_wait_frame_done_token();
		}

		if (_should_flush_cmdq_config_handle())
			_cmdq_flush_config_handle(reg_flush);

		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		if (reg_flush == true) {
			if (_should_insert_wait_frame_done_token())
				_cmdq_insert_wait_frame_done_token();
		}
		/* /cmdqRecDumpCommand(cmdqRecHandle handle) */
#else

		if (_should_flush_cmdq_config_handle()) {
			if (reg_flush) {
				MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagPulse,
					       Trigger, 2);
			}
			_cmdq_flush_config_handle(blocking, callback, userdata);

			/* /if(reg_flush) */
		}

		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		if (_should_insert_wait_frame_done_token() && (!_is_hdmi_decouple_mode(pgc->mode)))
			_cmdq_insert_wait_frame_done_token();
#endif
	}

	return 0;
}

static void _trigger_ovl_to_memory(disp_path_handle disp_handle,
				  cmdqRecHandle cmdq_handle,
				  void *callback, unsigned int data)
{
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, cmdq_handle, CMDQ_ENABLE);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA1_EOF);
	cmdqRecFlushAsyncCallback(cmdq_handle, (CmdqAsyncFlushCB)callback, data);
	cmdqRecReset(cmdq_handle);
}

#if 0
static int _trigger_overlay_engine(void)
{
	/* maybe we need a simple merge mechanism for CPU config. */
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL, ext_disp_use_cmdq);
	return 0;
}
#endif

#if HDMI_SUB_PATH
static unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
	return 0;
}

static unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
	return 0;
}

static unsigned int cmdqDdpDumpInfo(uint64_t engineFlag, char *pOutBuf, unsigned int bufSize)
{
	DISPERR("extd cmdq timeout:%llu\n", engineFlag);
	ext_disp_diagnose();
	return 0;
}

static unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
	return 0;
}
#endif


#if HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
static struct task_struct *ext_disp_present_fence_release_worker_task;
wait_queue_head_t ext_disp_irq_wq;
atomic_t ext_disp_irq_event = ATOMIC_INIT(0);

static void ext_disp_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	/* RET_VOID_IF_NOLOG(!is_hdmi_active()); */

	if (module == DISP_MODULE_RDMA1) {
		if (param & 0x4) {	/* frame done */
			/* /MMProfileLogEx(ddp_mmp_get_events()->Extd_IrqStatus, MMProfileFlagPulse, module, param); */
			vsync_time = ktime_get();
			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* In TEE, we have to protect WDMA registers, so we can't enable WDMA interrupt */
	/* here we use ovl frame done interrupt instead */
	if ((module == DISP_MODULE_OVL1) && ext_disp_is_decouple_mode()) {
		/* OVL1 frame done */
		if (param & 0x2) {
			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#else
	if ((module == DISP_MODULE_WDMA1) && ext_disp_is_decouple_mode()) {
		/* wdma1 frame done */
		if (param & 0x1) {
			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#endif

}

void ext_disp_update_present_fence(unsigned int fence_idx)
{
	g_ext_PresentFenceIndex = fence_idx;
}

static int ext_disp_present_fence_release_worker_kthread(void *data)
{
	/* int ret = 0; */
	struct sched_param param = {.sched_priority = RTPM_PRIO_FB_THREAD };
	sched_setscheduler(current, SCHED_RR, &param);

#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
#else
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
#endif

	while (1) {
#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
		wait_event_interruptible_timeout(ext_disp_irq_wq, atomic_read(&ext_disp_irq_event), HZ / 10);
		atomic_set(&ext_disp_irq_event, 0);
#else
		/* ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC); */
		ret =
		    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ / 25);
#endif

		/*  release present fence in vsync callback */
		{
			int fence_increment = 0;
			disp_sync_info *layer_info =
			    _get_sync_info(MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, 1),
					   disp_sync_get_present_timeline_id());
			if (layer_info == NULL) {
				DISPERR("_get_sync_info fail in present_fence_release thread\n");
				continue;
			}

			_ext_disp_path_lock();
			fence_increment = g_ext_PresentFenceIndex - layer_info->timeline->value;
			if (fence_increment > 0) {
				timeline_inc(layer_info->timeline, fence_increment);
				MMProfileLogEx(ddp_mmp_get_events()->Extd_release_present_fence,
					       MMProfileFlagPulse, g_ext_PresentFenceIndex,
					       fence_increment);

				DISP_PRINTF(DDP_FENCE1_LOG,
					    " release_present_fence idx %d timeline->value %d\n",
					    g_ext_PresentFenceIndex, layer_info->timeline->value);
			}


			_ext_disp_path_unlock();
		}
	}
	return 0;
}

#endif

struct task_struct *hdmi_config_rdma_task = NULL;
wait_queue_head_t hdmi_config_rdma_wq;
atomic_t hdmi_config_rdma_event = ATOMIC_INIT(0);

static void _hdmi_config_rdma_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (!is_hdmi_active()) {
		DISPMSG("hdmi is not plugin, exit _hdmi_config_rdma_irq_handler\n");
		return;
	}

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	if (!_is_hdmi_decouple_mode(pgc->mode)) {
		DISPDBG("hdmi is not decouple, so don't need config rdma\n");
		return;
	}
#endif

	if (module == DISP_MODULE_RDMA1) {
		if (param & 0x2) { /* frame start */
			atomic_set(&hdmi_config_rdma_event, 1);
			wake_up_interruptible(&hdmi_config_rdma_wq);
		}
	}
}

static int hdmi_config_rdma_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE};
	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_event_interruptible(hdmi_config_rdma_wq, atomic_read(&hdmi_config_rdma_event));
		atomic_set(&hdmi_config_rdma_event, 0);

		/* hdmi config rdma indepenent, instead of config wdma and rdma in the same cmdq task */
		hdmi_config_rdma();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);
	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);

	return 0;
}

int ext_disp_init(struct platform_device *dev, char *lcm_name, unsigned int session)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;
	/* DISP_MODULE_ENUM dst_module = 0; */

	LCM_PARAMS *lcm_param = NULL;
	/* LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED; */
#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
#else
	disp_ddp_path_config *data_config = NULL;
#endif

	DISPFUNC();
	dpmgr_init();

	init_cmdq_slots(&(pgc->cur_config_fence), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->subtractor_when_free), DISP_SESSION_TIMELINE_COUNT, 0);

	mutex_init(&vsync_mtx);
	extd_mutex_init(&(pgc->lock));
	_ext_disp_path_lock();

	if (pgc->plcm == NULL) {
		pgc->plcm = extd_drv_probe(lcm_name, LCM_INTERFACE_NOTDEFINED);
		if (pgc->plcm == NULL) {
			DISPCHECK("disp_lcm_probe returns null\n");
			ret = EXT_DISP_STATUS_ERROR;
			goto done;
		} else {
			DISPCHECK("disp_lcm_probe SUCCESS\n");
		}
	}

	lcm_param = extd_drv_get_params(pgc->plcm);

	if (lcm_param == NULL) {
		DISPERR("get lcm params FAILED\n");
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}

#if HDMI_SUB_PATH
	ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,
		(CmdqClockOnCB)cmdqDdpClockOn, (CmdqDumpInfoCB)cmdqDdpDumpInfo,
		(CmdqResetEngCB)cmdqDdpResetEng, (CmdqClockOffCB)cmdqDdpClockOff);
#endif

	if (ret) {
		DISPERR("cmdqCoreRegisterCB failed, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_handle=0x%p\n", pgc->cmdq_handle_config);
	}

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_rdma_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_rdma_handle=0x%p\n", pgc->cmdq_rdma_handle_config);

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	if (hdmi_ext_disp_mode == EXTD_DIRECT_LINK_MODE)
		ext_disp_mode = EXTD_DIRECT_LINK_MODE;
	else if (hdmi_ext_disp_mode == EXTD_DECOUPLE_MODE)
		ext_disp_mode = EXTD_DECOUPLE_MODE;
#endif

	if (ext_disp_mode == EXTD_DIRECT_LINK_MODE) {
		_build_path_direct_link();

		DISPCHECK("ext_disp display is DIRECT LINK MODE\n");
	} else if (ext_disp_mode == EXTD_DECOUPLE_MODE) {
		_build_path_decouple();

		DISPCHECK("ext_disp display is DECOUPLE MODE\n");
	} else if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE) {
		_build_path_single_layer();

		DISPCHECK("ext_disp display is SINGLE LAYER MODE\n");
	} else if (ext_disp_mode == EXTD_DEBUG_RDMA_DPI_MODE) {
		_build_path_debug_rdma_dpi();

		DISPCHECK("ext_disp display is DEBUG RDMA to dpi MODE\n");
	} else {
		DISPCHECK("ext_disp display mode is WRONG\n");
	}

	is_hdmi_rdma_stop = 0;

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		_cmdq_build_trigger_loop();

		DISPCHECK("ext_disp display BUILD cmdq trigger loop finished\n");

		_cmdq_start_trigger_loop();
	}

	pgc->session = session;

	DISPCHECK("ext_disp display START cmdq trigger loop finished\n");

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
#else
	dpmgr_path_set_video_mode(pgc->dpmgr_handle, ext_disp_is_video_mode());

	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);


	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));

	data_config->dst_w = lcm_param->width;
	data_config->dst_h = lcm_param->height;
	if (hdmi_is_interlace && !_is_hdmi_decouple_mode(pgc->mode))
		data_config->dst_h /= 2;
	data_config->dst_dirty = 1;

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, CMDQ_DISABLE);
#endif

	if (!extd_drv_is_inited(pgc->plcm)) {
		ret = extd_drv_init(dev, pgc->plcm);
	}
	/* this will be set to always enable cmdq later */
	if (ext_disp_is_video_mode()) {
		/* /ext_disp_use_cmdq = CMDQ_ENABLE; */
		if (ext_disp_mode == EXTD_DEBUG_RDMA_DPI_MODE)
			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_RDMA2_DONE);
		else
			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_RDMA1_DONE);
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE && (!_is_hdmi_decouple_mode(pgc->mode))) {
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}

	pgc->state = EXTD_INIT;

 done:

	/* /dpmgr_check_status(pgc->dpmgr_handle); */

	_ext_disp_path_unlock();

	ext_disp_resume();

#if HDMI_SUB_PATH_BOOT || HDMI_SUB_PATH_PROB_V2
#else
	/* dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE); */
#endif


#if HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	if ((ext_disp_present_fence_release_worker_task == NULL)
	    && !boot_up_with_facotry_mode()) {

#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
		init_waitqueue_head(&ext_disp_irq_wq);

		disp_register_module_irq_callback(DISP_MODULE_RDMA1, ext_disp_irq_handler);
#endif

		ext_disp_present_fence_release_worker_task =
		    kthread_create(ext_disp_present_fence_release_worker_kthread,
				   NULL, "ext_disp_present_fence_worker");
		wake_up_process(ext_disp_present_fence_release_worker_task);

		DISPMSG("wake_up ext_disp_present_fence_worker\n");

	}
#endif

	if (!hdmi_config_rdma_task && !boot_up_with_facotry_mode() && ext_disp_use_frc) {
		init_waitqueue_head(&hdmi_config_rdma_wq);

		disp_register_module_irq_callback(DISP_MODULE_RDMA1, _hdmi_config_rdma_irq_handler);

		hdmi_config_rdma_task =
		    kthread_create(hdmi_config_rdma_kthread, NULL, "hdmi_config_rdma_kthread");
		wake_up_process(hdmi_config_rdma_task);
	}

	DISPMSG("call config_hdmitx_timing\n");
	config_hdmitx_timing();

	DISPMSG("ext_disp_init done\n");
	return ret;
}

static void wait_util_ovl1_is_idle(void)
{
	int sleep_cnt = 0;

	DISPFUNC();
	/* check ovl1 if idle to avoid GCE can not wait WDMA1 EOF */
	do {
		sleep_cnt++;
		if (sleep_cnt > 200) {
			DISPMSG("check ovl1 idle timeout(200ms), can stop display hw!\n");
			break;
		}
		udelay(1000);
	} while ((DISP_REG_GET(DISP_OVL_INDEX_OFFSET + DISP_REG_OVL_STA) & 0x1));

	if (sleep_cnt > 1)
		DISPMSG("check ovl1 idle, sleep_cnt = %d\n", sleep_cnt);
	DISPMSG("func|wait_util_ovl1_is_idle finish\n");
}

static void wait_rdma1_frame_done(void)
{
	DISPFUNC();
	if (pgc->state == EXTD_SUSPEND)
		DISPMSG("ext_disp path has been early suspend\n");
	else
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ / 10);
	DISPMSG("func|wait_rdma1_frame_done finish\n");
}

int ext_disp_deinit(char *lcm_name)
{
	DISPFUNC();

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT)
		goto deinit_exit;

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	wait_util_ovl1_is_idle();
	if (_is_hdmi_decouple_mode(pgc->mode))
		wait_rdma1_frame_done();
#else
	wait_util_ovl1_is_idle();
	wait_rdma1_frame_done();
#endif

	/* use cmdq api to ensure task finish before stop display hardware */
	cmdqRecWaitThreadIdleWithTimeout(1, 200); /* normal world use GCE hw thread 1 */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	cmdqRecWaitThreadIdleWithTimeout(13, 200); /* secure world use GCE hw thread 13 */
#endif

	mutex_lock(&vsync_mtx);
	if (ext_disp_mode == EXTD_DECOUPLE_MODE) {
		dpmgr_disable_event(pgc->dpmgr_handle,
					DISP_PATH_EVENT_IF_VSYNC);
	}
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_destroy_path(pgc->dpmgr_handle);
	/* destory path and release mutex, avoid acquire mutex from 0 again */
	if (_is_hdmi_decouple_mode(pgc->mode)) {
		dpmgr_path_stop(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_path_reset(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_path_deinit(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_destroy_path(pgc->ovl2mem_path_handle);
	}

	cmdqRecDestroy(pgc->cmdq_handle_config);
	cmdqRecDestroy(pgc->cmdq_handle_trigger);
	cmdqRecDestroy(pgc->cmdq_rdma_handle_config);

	pgc->state = EXTD_DEINIT;
	mutex_unlock(&vsync_mtx);

 deinit_exit:
	_ext_disp_path_unlock();
	is_context_inited = 0;
	DISPMSG("ext_disp_deinit done\n");
	return 0;
}

/* register rdma done event */
int ext_disp_wait_for_idle(void)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	DISPFUNC();

	_ext_disp_path_lock();

/* done: */
	_ext_disp_path_unlock();
	return ret;
}

int ext_disp_wait_for_dump(void)
{
	return 0;
}

int ext_disp_wait_for_vsync(void *config)
{
	disp_session_vsync_config *c = (disp_session_vsync_config *) config;
	int ret = 0;

	if (ext_is_early_suspend) {
		DISPDBG("ext_disp path has been early suspend\n");
		return -1;
	}

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND) {
		DISPDBG("ext_disp path destroy or suspend, should not wait vsync\n");
		return -1;
	}

	if (pgc->dpmgr_handle == NULL) {
		DISP_PRINTF(DDP_VYSNC_LOG, "vsync for ext display path not ready yet(1)\n");
		return -1;
	}

	mutex_lock(&vsync_mtx);
	if (pgc->dpmgr_handle == NULL) {
		DISP_PRINTF(DDP_VYSNC_LOG, "vsync for ext display path not ready yet(1)\n");
		mutex_unlock(&vsync_mtx);
		return -1;
	}
	ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ/10);
	/* /dprec_logger_trigger(DPREC_LOGGER_VSYNC); */
	if (ret == -2) {
		DISPCHECK("vsync for ext display path not enabled yet(2)\n");
		mutex_unlock(&vsync_mtx);
		return -1;
	}
	mutex_unlock(&vsync_mtx);
	/* DISPMSG("vsync signaled\n"); */
	c->vsync_ts = ktime_to_ns(vsync_time);
	c->vsync_cnt++;

	return ret;
}

int ext_disp_suspend(void)
{
#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	bool is_ovl_hg_close = false;
#endif
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	DISPFUNC();

	_ext_disp_path_lock();
	ext_is_early_suspend = true;

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND) {
		DISPERR("EXTD_DEINIT or EXTD_SUSPEND\n");
		goto done;
	}

	pgc->need_trigger_overlay = 0;

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_stop_trigger_loop();

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	if (!(DISP_REG_GET(DISP_REG_CONFIG_MMSYS_HW_DCM_DIS0) & 0x00001000)) {
		is_ovl_hg_close = true;
		DISP_REG_SET_FIELD(NULL, MMSYS_DISPLAY_FLD_DCM_OVL1_SET, DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0, 0x1);
		DISPMSG("ext_disp_suspend before reset, ovl_0x240: 0x%08x, cg0: 0x%08x, dcm0: 0x%08x\n",
			DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + 0x1000),
			DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
			DISP_REG_GET(DISP_REG_CONFIG_MMSYS_HW_DCM_DIS0));
	}

	wait_util_ovl1_is_idle();
	if (_is_hdmi_decouple_mode(pgc->mode))
		wait_rdma1_frame_done();

	/* use cmdq api to ensure task finish before stop display hardware */
	cmdqRecWaitThreadIdleWithTimeout(1, 200);
	#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	cmdqRecWaitThreadIdleWithTimeout(13, 200);
	#endif
#else
	wait_rdma1_frame_done();
#endif
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	/* /if(dpmgr_path_is_busy(pgc->dpmgr_handle)) */
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	if (_is_hdmi_decouple_mode(pgc->mode)) {
		dpmgr_path_stop(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_path_reset(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
	}
#endif

#if 0				/* /(dpmgr_path_is_busy2(pgc->dpmgr_handle)) */
	{
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		dpmgr_check_status(pgc->dpmgr_handle);
	}
#endif

#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	DISPMSG("ext_disp_suspend after reset, ovl_0x240: 0x%08x, cg0: 0x%08x, dcm0: 0x%08x\n",
		DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG + 0x1000),
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_HW_DCM_DIS0));
	if (is_ovl_hg_close)
		DISP_REG_SET_FIELD(NULL, MMSYS_DISPLAY_FLD_DCM_OVL1_SET, DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0, 0x1);
#endif

	extd_drv_suspend(pgc->plcm);
	/* /dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE); */

	pgc->state = EXTD_SUSPEND;


 done:
	_ext_disp_path_unlock();

	DISPMSG("ext_disp_suspend done\n");
	return ret;
}

int ext_disp_resume(void)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	_ext_disp_path_lock();

	if (pgc->state < EXTD_INIT) {
		DISPERR("EXTD_DEINIT\n");
		goto done;
	}

	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);
#if HDMI_SUB_PATH_SUPPORT_DL_DC_DYNAMIC_SWITCH
	if (_is_hdmi_decouple_mode(pgc->mode))
		dpmgr_path_power_on(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
#endif

	extd_drv_resume(pgc->plcm);

	/* /dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE); */

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_start_trigger_loop();


	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("stop display path failed, still busy\n");
		ret = -1;
		/* FIXME : bypass dpmgr_path_is_busy temp */
		/* goto done; */
	}

	pgc->state = EXTD_RESUME;
	ext_is_early_suspend = false;

 done:
	_ext_disp_path_unlock();
	DISPMSG("ext_disp_resume done\n");
	return ret;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static void init_wdma_idx_switch_between_normal_and_secure(void)
{
	static int last_wdma_security;
	static int cur_wdma_security;

	/* wdma switch between nonsec and sec */
	last_wdma_security = cur_wdma_security;
	cur_wdma_security = g_wdma_security;
	if (cur_wdma_security != last_wdma_security) {
		pgc->dc_buf_id = 0;
		DISPMSG("[SVP] : wdma switch from [%d] to [%d], index = %d\n",
			last_wdma_security, cur_wdma_security, pgc->dc_buf_id);
	}
}

static void init_rdma_idx_switch_between_normal_and_secure(void)
{
	static int last_rdma_security;
	static int cur_rdma_security;

	/* rdma switch between nonsec and sec */
	last_rdma_security = cur_rdma_security;
	cur_rdma_security = g_rdma_security;
	if (cur_rdma_security != last_rdma_security) {
		pgc->dc_rdma_buf_id = 0;
		DISPMSG("[SVP] : rdma switch from [%d] to [%d], index = %d\n",
			last_rdma_security, cur_rdma_security, pgc->dc_rdma_buf_id);
	}
}
#endif

static unsigned int get_wdma_buffer_index(void)
{
	int index = 0;

	index = pgc->dc_buf_id + 1;
	if (hdmi_use_original_buffer) {
		index %= DISP_INTERNAL_BUFFER_COUNT;
		if (index == pgc->dc_rdma_buf_id)
			HDMI_FRC_LOG("OVL buffctl overwrite pgc->dc_buf_id = %d\n", pgc->dc_buf_id);
		else
			pgc->dc_buf_id = index;
	} else {
		index %= DISP_SPLIT_BUFFER_COUNT;
		if (index == pgc->dc_rdma_buf_id) {
			/*pgc->dc_buf_id = (pgc->dc_buf_id + DISP_SPLIT_BUFFER_COUNT - 1) % DISP_SPLIT_BUFFER_COUNT;*/
			HDMI_FRC_LOG("OVL buffctl overwrite pgc->dc_buf_id = %d\n", pgc->dc_buf_id);
		} else
			pgc->dc_buf_id = index;
	}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	init_wdma_idx_switch_between_normal_and_secure();
#endif

	DISPDBG("%s : pgc->dc_buf_id = %d\n", __func__, pgc->dc_buf_id);

	return pgc->dc_buf_id;
}

static uint32_t get_wdma_normal_buffer_mva(void)
{
	uint32_t writing_mva;

	unsigned int index = get_wdma_buffer_index();

	if (hdmi_use_original_buffer) {
		writing_mva = pgc->dc_buf[index];
		pgc->buf_pts[index] = ovl_frame_pts;
	} else {
		writing_mva = pgc->dc_split_buf[index];
		pgc->split_buf_pts[index] = ovl_frame_pts;
	}

	return writing_mva;
}

static uint32_t get_wdma_secure_buffer_handle(void)
{
	uint32_t writing_mva;

	int index = get_wdma_buffer_index();

	if (hdmi_use_original_buffer) {
		writing_mva = pgc->dc_sec_buf[index];
		pgc->buf_pts[index] = ovl_frame_pts;
	} else {
		writing_mva = pgc->dc_split_sec_buf[index];
		/* for calculate wdma secure buffer offset */
		pgc->cur_wdma_sec_buf_id = index;
		pgc->split_buf_pts[index] = ovl_frame_pts;
	}

	return writing_mva;
}

static unsigned int get_wdma_secure_buffer_offset(void)
{
	unsigned int offset = 0;
	unsigned int offset_index = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	if (hdmi_use_original_buffer) {
		offset = 0;
	} else {
		offset_index = pgc->cur_wdma_sec_buf_id % DISP_SPLIT_COUNT;
		offset = (buffer_size / DISP_SPLIT_COUNT) * offset_index;
	}

	DISPDBG("%s : wdma secure buffer offset = %d\n", __func__, offset);

	return offset;
}

static int extd_disp_config_output(void)
{
	int ret = 0;
	disp_ddp_path_config *pconfig = NULL;
	uint32_t writing_mva = 0;
	void *cmdq_handle = NULL;

	/* _ext_disp_path_lock(); */

	/* config ovl1->wdma1 */
	cmdq_handle = pgc->cmdq_handle_config;
	pconfig = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* 0 : nonsec/sec(default) 1 : always sec 2 : always normal, for svp debug */
		if (gDebugSvpHdmiAlwaysSec == 1)
			g_wdma_security = DISP_SECURE_BUFFER;
		else if (gDebugSvpHdmiAlwaysSec == 2)
			g_wdma_security = DISP_NORMAL_BUFFER;

		pconfig->wdma_config.security = g_wdma_security;
		if (pconfig->wdma_config.security == DISP_SECURE_BUFFER) {
			/* get secure buffer handle and offset */
			writing_mva = get_wdma_secure_buffer_handle();
			pconfig->wdma_config.split_buf_offset = get_wdma_secure_buffer_offset();
		} else
			writing_mva = get_wdma_normal_buffer_mva();
#else
		writing_mva = get_wdma_normal_buffer_mva();
#endif

		if (writing_mva)
			pconfig->wdma_config.dstAddress = writing_mva;
		else
			DISPERR("wdma input address is null!!\n");
		pconfig->wdma_config.srcHeight = extd_display_get_height();
		pconfig->wdma_config.srcWidth = extd_display_get_width();
		pconfig->wdma_config.clipX = 0;
		pconfig->wdma_config.clipY = 0;
		pconfig->wdma_config.clipHeight = extd_display_get_height();
		pconfig->wdma_config.clipWidth = extd_display_get_width();
		pconfig->wdma_config.outputFormat = eRGB888;
		pconfig->wdma_config.useSpecifiedAlpha = 1;
		pconfig->wdma_config.alpha = 0xFF;
		pconfig->wdma_config.dstPitch =
			extd_display_get_width() * DP_COLOR_BITS_PER_PIXEL(eRGB888) / 8;
	}
	pconfig->wdma_dirty = 1;

	if ((pconfig->wdma_config.srcWidth != pconfig->dst_w)
	    || (pconfig->wdma_config.srcHeight != pconfig->dst_h)
	    ) {
		DISPMSG("============ ovl_roi(%d %d) != wdma(%d %d) ============\n",
			pconfig->dst_w,
			pconfig->dst_h,
			pconfig->wdma_config.srcWidth, pconfig->wdma_config.srcHeight);

		pconfig->wdma_config.srcWidth = pconfig->dst_w;
		pconfig->wdma_config.srcHeight = pconfig->dst_h;

		pconfig->wdma_config.clipWidth = pconfig->dst_w;
		pconfig->wdma_config.clipHeight = pconfig->dst_h;


	}

	DISP_PRINTF(DDP_RESOLUTION_LOG, "config_wdma w %d, h %d hdmi(%d %d)\n",
		    pconfig->wdma_config.srcWidth,
		    pconfig->wdma_config.srcHeight,
		    extd_display_get_width(), extd_display_get_height());

	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, cmdq_handle);

	/* _ext_disp_path_unlock(); */

	return ret;
}

static int video_fps;

void hdmi_set_video_fps(int fps)
{
	video_fps = fps;
	if (video_fps > 0)
		HDMI_FRC_LOG("[FRC] : hdmi_set_video_fps video_fps = %d\n", video_fps);
}

static enum FRC_TYPE get_frc_type(void)
{
	enum FRC_TYPE frc_type = FRC_UNSOPPORT_MODE;
	int tv_fps = hdmi_get_tv_fps();

	if ((!video_fps) || (!tv_fps))
		frc_type = FRC_1_TO_1_MODE;
	else if ((video_fps > 2300 && video_fps < 2600 && 50000 == tv_fps)
		    || (video_fps > 2900 && video_fps < 3100 && 59940 == tv_fps))
		frc_type = FRC_1_TO_2_MODE; /* 25->50 / 30->60 */
	else if (video_fps > 2300 && video_fps < 2450 && 59940 == tv_fps)
		frc_type = FRC_2_TO_5_MODE; /* 24->60 */
	else if (video_fps == 0x1fff) /* vdec will set fps=0x1fff when miracast */
		frc_type = FRC_FOR_MIRACAST;
	else
		frc_type = FRC_1_TO_1_MODE; /* other */

	if (frc_type == FRC_1_TO_2_MODE || frc_type == FRC_2_TO_5_MODE)
		HDMI_FRC_LOG("[FRC] : frc_type = %d tv_fps = %d video_fps = %d\n",
			frc_type, tv_fps, video_fps);

	return frc_type;
}

static int get_true_rdma_buf_id(int rdma_buf_id)
{
	int cur_rdma_buf_id;

	if (hdmi_use_original_buffer)
		cur_rdma_buf_id = rdma_buf_id % DISP_INTERNAL_BUFFER_COUNT;
	else
		cur_rdma_buf_id = rdma_buf_id % DISP_SPLIT_BUFFER_COUNT;

	if (cur_rdma_buf_id != rdma_buf_id)
		DISPMSG("res change %d to %d\n", cur_rdma_buf_id, rdma_buf_id);

	return cur_rdma_buf_id;
}

static int get_newest_rdma_cur_buf_id(void)
{
	if (hdmi_use_original_buffer)
		pgc->dc_rdma_buf_id = wdma_frame_done_buf_idx % DISP_INTERNAL_BUFFER_COUNT;
	else
		pgc->dc_rdma_buf_id = wdma_frame_done_buf_idx % DISP_SPLIT_BUFFER_COUNT;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	init_rdma_idx_switch_between_normal_and_secure();
#endif

	DISPDBG("get_config_rdma_cur_buf_id : pgc->dc_rdma_buf_id = %d\n", pgc->dc_rdma_buf_id);

	return pgc->dc_rdma_buf_id;
}

static int get_rdma_cur_buf_id(void)
{
	static unsigned long long pre_video_pts;

	/* compare rdma buf id with wdma, get rdma cur buf id */
	/* if consequent frame's pts is same, will drop current frame */
	while (pgc->dc_rdma_buf_id != wdma_frame_done_buf_idx) {
		if (hdmi_use_original_buffer) {
			pgc->dc_rdma_buf_id = (pgc->dc_rdma_buf_id + 1) % DISP_INTERNAL_BUFFER_COUNT;
			if ((pre_video_pts != pgc->buf_pts[pgc->dc_rdma_buf_id])
				|| (!pre_video_pts)
				|| (!(pgc->buf_pts[pgc->dc_rdma_buf_id]))
				)
				break;
			else
				HDMI_RDMA_PTS_LOG("[PTS] : %lld pts repeat, pass this buffer id %d",
					pre_video_pts, pgc->dc_rdma_buf_id);
		} else {
			pgc->dc_rdma_buf_id = (pgc->dc_rdma_buf_id + 1) % DISP_SPLIT_BUFFER_COUNT;
			if ((pre_video_pts != pgc->split_buf_pts[pgc->dc_rdma_buf_id])
				|| (!pre_video_pts)
				|| (!(pgc->split_buf_pts[pgc->dc_rdma_buf_id]))
				)
				break;
			else
				HDMI_RDMA_PTS_LOG("[PTS] : %lld pts repeat, pass this buffer id %d",
					pre_video_pts, pgc->dc_rdma_buf_id);
		}
	}

	if (hdmi_use_original_buffer)
		pre_video_pts = pgc->buf_pts[pgc->dc_rdma_buf_id];
	else
		pre_video_pts = pgc->split_buf_pts[pgc->dc_rdma_buf_id];

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	init_rdma_idx_switch_between_normal_and_secure();
#endif

	DISPDBG("get_config_rdma_cur_buf_id : pgc->dc_rdma_buf_id = %d\n", pgc->dc_rdma_buf_id);

	return pgc->dc_rdma_buf_id;
}

static int get_frc_rdma_cur_buf_id(void)
{
	static int count_1_to_2_mode;
	static int count_2_to_5_mode;
	static int last_frc_type = FRC_UNSOPPORT_MODE;
	static int cur_frc_type = FRC_UNSOPPORT_MODE;
	static int cur_buf_id;

	if (!ext_disp_use_frc || boot_up_with_facotry_mode()) {
		cur_buf_id = pgc->dc_buf_id;
		return cur_buf_id;
	}

	last_frc_type = cur_frc_type;
	cur_frc_type = get_frc_type();
	if (cur_frc_type != last_frc_type) {
		HDMI_FRC_LOG("[FRC] : switch from [%d] to [%d]\n", last_frc_type, cur_frc_type);
		count_1_to_2_mode = 0;
		count_2_to_5_mode = 0;
	}

	if (cur_frc_type == FRC_1_TO_2_MODE) {
		count_1_to_2_mode %= 2;
		if (0 == count_1_to_2_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		else
			cur_buf_id = get_true_rdma_buf_id(cur_buf_id);
		count_1_to_2_mode++;
	} else if (cur_frc_type == FRC_2_TO_5_MODE) {
		count_2_to_5_mode %= 5;
		if (0 == count_2_to_5_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		if (2 == count_2_to_5_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		else
			cur_buf_id = get_true_rdma_buf_id(cur_buf_id);
		count_2_to_5_mode++;
	} else if (cur_frc_type == FRC_1_TO_1_MODE) {
		cur_buf_id = get_rdma_cur_buf_id();
	} else if (cur_frc_type == FRC_FOR_MIRACAST) {
		HDMI_FRC_LOG("[PTS] : FRC_FOR_MIRACAST\n");
		/* rdma will use the newest index in miracast scenario, and maybe drop frame */
		cur_buf_id = get_newest_rdma_cur_buf_id();
	} else {
		HDMI_FRC_LOG("[PTS] : FRC_UNSOPPORT_MODE\n");
		cur_buf_id = get_rdma_cur_buf_id();
	}

	DISPDBG("get_config_rdma_cur_buf_id : pgc->dc_rdma_buf_id = %d\n", pgc->dc_rdma_buf_id);

	/* check if FRC_1_TO_2_MODE/FRC_2_TO_5_MODE work properly by log
	 * FRC_1_TO_2_MODE id : 0 0 1 1 2 2 3 3 ...
	 * FRC_2_TO_5_MODE id : 0 0 1 1 1 2 2 3 3 3 ...
	 */
	if (cur_frc_type != FRC_1_TO_1_MODE && video_fps)
		HDMI_FRC_LOG("[FRC] : RDMA cur_buf_id = %d\n", cur_buf_id);

	return cur_buf_id;
}

static uint32_t get_rdma_normal_buffer_mva(void)
{
	uint32_t writing_mva;
	int cur_buf_id;

	cur_buf_id = get_frc_rdma_cur_buf_id();

	/* get rdma normal buffer mva according to cur buf id */
	if (hdmi_use_original_buffer) {
		writing_mva = pgc->dc_buf[cur_buf_id];
		pgc->pts = pgc->buf_pts[cur_buf_id];
	} else {
		writing_mva = pgc->dc_split_buf[cur_buf_id];
		pgc->pts = pgc->split_buf_pts[cur_buf_id];
	}

	if (hdmi_is_interlace && 1 == DPI0_IS_TOP_FIELD())
		writing_mva += extd_display_get_width() * 3;

	return writing_mva;
}

static uint32_t get_rdma_secure_buffer_handle(void)
{
	uint32_t writing_mva;
	int cur_buf_id;

	cur_buf_id = get_frc_rdma_cur_buf_id();
	/* for calculate rdma secure buffer offset */
	pgc->cur_rdma_sec_buf_id = cur_buf_id;

	/* get rdma secure buffer handle according to cur buf id */
	if (hdmi_use_original_buffer) {
		writing_mva = pgc->dc_sec_buf[cur_buf_id];
		pgc->pts = pgc->buf_pts[cur_buf_id];
	} else {
		writing_mva = pgc->dc_split_sec_buf[cur_buf_id];
		pgc->pts = pgc->split_buf_pts[cur_buf_id];
	}

	return writing_mva;
}

static unsigned int get_rdma_secure_buffer_offset(void)
{
	unsigned int offset = 0;
	unsigned int offset_index = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	if (hdmi_use_original_buffer) {
		offset = 0;
	} else {
		offset_index = pgc->cur_rdma_sec_buf_id % DISP_SPLIT_COUNT;
		offset = (buffer_size / DISP_SPLIT_COUNT) * offset_index;
	}

	if (hdmi_is_interlace && 1 == DPI0_IS_TOP_FIELD())
		offset += extd_display_get_width() * 3;

	DISPDBG("%s : rdma secure buffer offset = %d\n", __func__, offset);

	return offset;
}

static void dump_trigger_count(void)
{
	rdma_trigger_count++;
	if (rdma_trigger_count >= hdmi_get_tv_fps()/1000) {
		HDMI_FRC_LOG("[FRC] : rdma_trigger_count = %d ovl_wdma_trigger_count = %d\n",
			rdma_trigger_count, ovl_wdma_trigger_count);
		rdma_trigger_count = 0;
		ovl_wdma_trigger_count = 0;
	}
}

static int valid_pts_diff;

static void hdmi_cal_frame_pts(int fps)
{
#if 0
	if (fps != 0)
		valid_pts_diff = 1000/fps + 1;
#else
	valid_pts_diff = 45;
#endif
}

static void calculate_ovl_frame_pts(void)
{
	static long long last_ovl_pts;
	static long long cur_ovl_pts;
	unsigned long long twice_pts_diff = 0;

	last_ovl_pts = cur_ovl_pts;
	cur_ovl_pts = ovl_frame_pts;

	if (last_ovl_pts && cur_ovl_pts && valid_pts_diff) {
		twice_pts_diff = cur_ovl_pts - last_ovl_pts;
		HDMI_OVL_PTS_LOG("[PTS] : ovl twice_pts_diff = %lld cur_pts = %lld last_pts = %lld\n",
			twice_pts_diff, cur_ovl_pts, last_ovl_pts);

		if (twice_pts_diff > 1000) {
			HDMI_OVL_PTS_LOG("[PTS] : ovl maybe next seek, ignore temp\n");
		} else if (twice_pts_diff > valid_pts_diff) {
			g_ovl_drop_frame_count++;
			DISPMSG("[PTS] : ovl frame drop(check hwc)! count = %lld cur_pts = %lld last_pts = %lld\n",
				g_ovl_drop_frame_count, cur_ovl_pts, last_ovl_pts);
		} else if (twice_pts_diff == 0) {
			g_ovl_repeated_frame_count++;
			HDMI_OVL_PTS_LOG("[PTS] : ovl frame repeated(check hwc)! count = %lld pts = %lld\n",
				g_ovl_repeated_frame_count, cur_ovl_pts);
		} else if (twice_pts_diff < 0) {
			HDMI_OVL_PTS_LOG("[PTS] : ovl maybe prev seek, ignore temp\n");
		}
	}
}

static void calculate_rdma_frame_pts(void)
{
	static long long last_rdma_pts;
	static long long cur_rdma_pts;
	unsigned long long twice_pts_diff = 0;

	last_rdma_pts = cur_rdma_pts;
	cur_rdma_pts = pgc->pts;

	if (last_rdma_pts && cur_rdma_pts && valid_pts_diff) {
		twice_pts_diff = cur_rdma_pts - last_rdma_pts;
		HDMI_RDMA_PTS_LOG("[PTS] : twice_pts_diff = %lld  cur_pts = %lld last_pts = %lld\n",
			twice_pts_diff, cur_rdma_pts, last_rdma_pts);

		if (twice_pts_diff > 1000) {
			HDMI_RDMA_PTS_LOG("[PTS] : rdma maybe next seek, ignore temp\n");
		} else if (twice_pts_diff > valid_pts_diff) {
			g_rdma_drop_frame_count++;
			DISPMSG("[PTS] : rdma frame drop! count = %lld cur_pts = %lld last_pts = %lld\n",
				g_rdma_drop_frame_count, cur_rdma_pts, last_rdma_pts);
		} else if (twice_pts_diff == 0) {
			if (FRC_1_TO_1_MODE == get_frc_type()) {
				g_rdma_repeated_frame_count++;
				HDMI_RDMA_PTS_LOG("[PTS] : rdma frame repeated! count = %lld pts = %lld\n",
					g_rdma_repeated_frame_count, cur_rdma_pts);
			}
		} else if (twice_pts_diff < 0) {
			HDMI_RDMA_PTS_LOG("[PTS] : rdma maybe prev seek, ignore temp\n");
		}
	}
}

unsigned long long get_rdma_frame_drop_count(void)
{
	return g_rdma_drop_frame_count;
}

void hdmi_config_rdma(void)
{
	disp_ddp_path_config *pconfig = NULL;
	uint32_t writing_mva = 0;
	cmdqRecHandle cmdq_handle;

	/* DISPFUNC(); */
	if (first_build_path_lk_kernel)
		return;

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT) {
		DISPMSG("EXTD_DEINIT : exit hdmi_config_rdma!\n");
		goto done;
	}

	dump_trigger_count();

	{
		cmdq_handle = pgc->cmdq_rdma_handle_config;
		if (cmdq_handle == NULL)
			DISPERR("hdmi_config_rdma : cmdq_handle is NULL!\n");
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

		pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* 0 : nonsec/sec(default) 1 : always sec 2 : always normal, for svp debug */
		if (gDebugSvpHdmiAlwaysSec == 1)
			g_rdma_security = DISP_SECURE_BUFFER;
		else if (gDebugSvpHdmiAlwaysSec == 2)
			g_rdma_security = DISP_NORMAL_BUFFER;

		pconfig->rdma_config.security = g_rdma_security;
		if (pconfig->rdma_config.security == DISP_SECURE_BUFFER) {
			/* get secure buffer handle and offset */
			writing_mva = get_rdma_secure_buffer_handle();
			pconfig->rdma_config.split_buf_offset =
				get_rdma_secure_buffer_offset();
		} else
			writing_mva = get_rdma_normal_buffer_mva();
#else
		writing_mva = get_rdma_normal_buffer_mva();
#endif
		calculate_rdma_frame_pts();

		if (writing_mva)
			pconfig->rdma_config.address = (unsigned int)writing_mva;
		else
			DISPERR("rdma input address is null!!\n");
		pconfig->rdma_config.width = extd_display_get_width();
		pconfig->rdma_config.height = extd_display_get_height();
		pconfig->rdma_config.inputFormat = eRGB888;
		pconfig->rdma_config.pitch = extd_display_get_width() * 3;
		if (hdmi_is_interlace && _is_hdmi_decouple_mode(pgc->mode)) {
			pconfig->rdma_config.height /= 2;
			pconfig->rdma_config.pitch *= 2;
		}

		pconfig->rdma_dirty = 1;
		dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_ENABLE);
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_ENABLE);
		cmdqRecFlushAsyncCallback(cmdq_handle, NULL, 0);
		cmdqRecReset(cmdq_handle);
	}

done:
	_ext_disp_path_unlock();

	DISPDBG("hdmi_config_rdma done\n");
}

static void ovl_wdma_callback(void)
{
	if (!ext_disp_use_frc) {
		DISPMSG("ext_disp_use_frc close\n");
		hdmi_config_rdma();
	} else {
		wdma_frame_done_buf_idx = pgc->dc_buf_id;
		DISPDBG("%s wdma_frame_done_buf_idx = %d\n", __func__, wdma_frame_done_buf_idx);
	}
	first_build_path_lk_kernel = 0;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static void ovl_wdma_callback_normal(void)
{
	ovl_wdma_callback();
	/* ensure wdma frame done before rdma switch normal/secure world */
	g_rdma_security = DISP_NORMAL_BUFFER;
}

static void ovl_wdma_callback_secure(void)
{
	ovl_wdma_callback();
	/* ensure wdma frame done before rdma switch normal/secure world */
	g_rdma_security = DISP_SECURE_BUFFER;
}
#endif

int ext_disp_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;
	/* DISPFUNC(); */

	_ext_disp_path_lock();
#if HDMI_SUB_PATH
	/*
	   DISPMSG("%s hdmi_active %d state %d handle 0x%p 0x%p fac %d\n",
	   __func__,
	   is_hdmi_active(),
	   pgc->state,
	   pgc->dpmgr_handle,
	   pgc->ovl2mem_path_handle,
	   boot_up_with_facotry_mode());
	 */

	if (boot_up_with_facotry_mode()) {
		DISPDBG("%s is_hdmi_active %d state %d dpmgr_handle 0x%p\n",
			__func__, is_hdmi_active(), pgc->state, pgc->dpmgr_handle);

		DISPDBG("%s boot_up_with_facotry_mode\n", __func__);
	} else if (pgc->dpmgr_handle == NULL) {
		DISPMSG("%s is_hdmi_active %d state %d dpmgr_handle not init yet!!!!\n",
			__func__, is_hdmi_active(), pgc->state);

		_ext_disp_path_unlock();
		return -1;
	} else if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)
		   || pgc->need_trigger_overlay < 1) {
		DISPMSG("trigger ext display is already sleeped 0x%p 0x%p\n",
			pgc->dpmgr_handle, pgc->ovl2mem_path_handle);


		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Trigger,
			       0);
		_ext_disp_path_unlock();
		return -1;
	}
#else
	if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)
	    || pgc->need_trigger_overlay < 1) {
		DISPMSG("trigger ext display is already sleeped\n");
		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Trigger,
			       0);
		_ext_disp_path_unlock();
		return -1;
	}
#endif

	if (_is_hdmi_decouple_mode(pgc->mode))
		extd_disp_config_output();

	/* _ext_disp_path_lock(); */

	ovl_wdma_trigger_count++;

	if (_should_trigger_interface()) {
		_trigger_display_interface(blocking, callback, userdata);
	} else {
		/* _trigger_overlay_engine(); */
		/* _trigger_display_interface(FALSE, ovl_wdma_callback, 0); */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (g_wdma_security == DISP_NORMAL_BUFFER)
			_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
				pgc->cmdq_handle_config, ovl_wdma_callback_normal, 0);
		else if (g_wdma_security == DISP_SECURE_BUFFER)
			_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
				pgc->cmdq_handle_config, ovl_wdma_callback_secure, 0);
#else
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
			pgc->cmdq_handle_config, ovl_wdma_callback, 0);
#endif
	}

	_ext_disp_path_unlock();

	/* for pan display : factory/recovery mode */
	if (_is_hdmi_decouple_mode(pgc->mode) && boot_up_with_facotry_mode())
		hdmi_config_rdma();

	DISPDBG("ext_disp_trigger done\n");

	return ret;
}

extern unsigned int hdmi_va;
extern unsigned int hdmi_mva_r;

int ext_disp_config_input(ext_disp_input_config *input)
{
	int ret = 0;
	/* int i = 0; */
	/* int layer = 0; */
	/* /DISPFUNC(); */

	disp_ddp_path_config *data_config;

	/* all dirty should be cleared in dpmgr_path_get_last_config() */

	disp_path_handle *handle;
	if (_is_hdmi_decouple_mode(pgc->mode))
		handle = pgc->ovl2mem_path_handle;
	else
		handle = pgc->dpmgr_handle;

#if HDMI_SUB_PATH
	DISPDBG("%s ext disp is sleeped %d is_hdmi_active %d\n", __func__,
		ext_disp_is_sleepd(), is_hdmi_active());
#else
	if ((is_hdmi_active() == false) || ext_disp_is_sleepd()) {
		DISPMSG("ext disp is already sleeped\n");
		return 0;
	}
#endif

	_ext_disp_path_lock();
	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

	if (input->layer_en) {
		if (input->vaddr) {
			/* /_debug_pattern(0x00000000, input->vaddr, input->dst_w, input->dst_h, input->src_pitch, 0x00000000, input->layer, input->buff_idx); */
		} else {
			/* /_debug_pattern(input->addr,0x00000000,  input->dst_w, input->dst_h, input->src_pitch, 0x00000000, input->layer, input->buff_idx); */
		}
	}
#ifdef EXTD_DBG_USE_INNER_BUF
	if (input->fmt == eYUY2) {
		/* /input->layer_en = 1; */
		/* /memset(input, 0, sizeof(ext_disp_input_config)); */
		input->layer_en = 1;
		input->addr = hdmi_mva_r;
		input->vaddr = hdmi_va;
		input->fmt = eRGB888;	/* /eRGBA8888  eYUY2 */
		input->src_w = 1280;
		input->src_h = 720;
		input->src_x = 0;
		input->src_y = 0;
		input->src_pitch = 1280 * 3;
		input->dst_w = 1280;
		input->dst_h = 720;
		input->dst_x = 0;
		input->dst_y = 0;
		input->aen = 0;
		input->alpha = 0xff;
	}
#endif


	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
		data_config->ovl_dirty = 1;
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty = 1;
	}

	/* /DISPERR("ext_disp_config_input cmdq %d wi %d ovl %d vm %d\n", ext_disp_cmdq_enabled(), _should_wait_path_idle(), _should_config_ovl_input(), ext_disp_is_video_mode()); */
	if (_should_wait_path_idle()) {
		dpmgr_wait_event_timeout(handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);
	}

	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	ret =
	    dpmgr_path_config(handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;
	/* /DISPMSG("ext_disp_config_input done\n"); */

	_ext_disp_path_unlock();


	return ret;
}

int get_cur_config_fence(int idx)
{
	int fence_idx;

	if (pgc->cur_config_fence)
		cmdqBackupReadSlot(pgc->cur_config_fence, idx, &fence_idx);
	else {
		DISPMSG("get_cur_config_fence : cur_config_fence is NULL!\n");
		fence_idx = -1;
	}

	return fence_idx;
}

int get_subtractor_when_free(int idx)
{
	int fence_idx;

	if (pgc->subtractor_when_free)
		cmdqBackupReadSlot(pgc->subtractor_when_free, idx, &fence_idx);
	else {
		DISPMSG("get_cur_config_fence : subtractor_when_free is NULL!\n");
		fence_idx = -1;
	}

	return fence_idx;
}

int ext_disp_config_input_multiple(ext_disp_input_config *input,
				   disp_session_input_config *session_input)
{
	int ret = 0;
	int i = 0;
	disp_ddp_path_config *data_config;
	disp_path_handle *handle;
	int idx = session_input->config[0].next_buff_idx;
	int fps = 0;

	/* DISPFUNC(); */
	if (_is_hdmi_decouple_mode(pgc->mode))
		handle = pgc->ovl2mem_path_handle;
	else
		handle = pgc->dpmgr_handle;

#if HDMI_SUB_PATH
	if ((pgc->dpmgr_handle == NULL)
	    && !boot_up_with_facotry_mode()) {
		DISPMSG("%s is_hdmi_active %d state %d dpmgr_handle not init yet!!!!\n",
			__func__, is_hdmi_active(), pgc->state);

		DISPMSG("config ext disp is already sleeped 0x%p 0x%p\n",
			pgc->dpmgr_handle, pgc->ovl2mem_path_handle);

		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Config,
			       idx);
		return 0;
	}
#endif

	if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)) {
		DISPMSG("%s ext disp is already sleeped,is_hdmi_active %d, ext disp state %d\n",
			__func__, is_hdmi_active(), pgc->state);
		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Config,
			       idx);
		/* -2 will release fence */
		return -2;
	}

	_ext_disp_path_lock();

	/* write fence_id/enable to DRAM using cmdq
	 * it will be used when release fence (put these after config registers done) */
	for (i = 0; i < session_input->config_layer_num; i++) {
		unsigned int last_fence, cur_fence;
		disp_input_config *input_cfg = &session_input->config[i];
		int layer = input_cfg->layer_id;

		cmdqBackupReadSlot(pgc->cur_config_fence, layer, &last_fence);
		cur_fence = input_cfg->next_buff_idx;

		if (cur_fence != -1 && cur_fence > last_fence)
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->cur_config_fence,
						layer, cur_fence);

		/* for dim_layer/disable_layer/no_fence_layer, just release all fences configured */
		/* for other layers, release current_fence-1 */
		if (input_cfg->buffer_source == DISP_BUFFER_ALPHA
		    || input_cfg->layer_enable == 0 || cur_fence == -1)
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->subtractor_when_free,
						layer, 0);
		else
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->subtractor_when_free,
						layer, 1);
	}

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			/* /dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG,
			   input->layer|(input->layer_en<<16), input->addr); */

			if (input[i].dirty) {
				dprec_mmp_dump_ovl_layer(&(data_config->ovl_config[input[i].layer]),
							 input[i].layer, 2);
				ret =
				    _convert_disp_input_to_ovl(&
							       (data_config->ovl_config
								[input[i].layer]), &input[i]);
			}
			/*
			else
			{
				data_config->ovl_config[input[i].layer].layer_en = input[i].layer_en;
				data_config->ovl_config[input[i].layer].layer = input[i].layer;
			}
			 */
			data_config->ovl_dirty = 1;

			/* /dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input->src_x, input->src_y); */
		}

#if 1
		/*
		** get video fps and pts info from hwc
		** hwc set video info in layer 0 if have video layer
		** if have video and ui, and video don't update, hwc just pass ui to display
		** we should keep old video fps and pts
		*/
		if (input[0].dirty) {
			fps = input[0].fps;
			hdmi_set_video_fps(fps);

			ovl_frame_pts = input[0].timestamp;
			hdmi_cal_frame_pts(fps);
			calculate_ovl_frame_pts();
		}
#else
		/* get video fps from hwc */
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			if (input[i].dirty) {
				fps = input[i].fps;
				if (fps != 0)
					break;
			}
		}
		hdmi_set_video_fps(fps);

		hdmi_cal_frame_pts(fps);

		/* get video pts from hwc */
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
#if 1
			ovl_frame_pts = input[i].timestamp;
#else
			/* test code */
			ovl_frame_pts++;
#endif
			if (ovl_frame_pts != 0)
				break;
		}
		calculate_ovl_frame_pts();
#endif
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	ret =
	    dpmgr_path_config(handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;

	_ext_disp_path_unlock();
	DISPDBG("config_input_multiple idx %x -w %d, h %d\n", idx, data_config->ovl_config[0].src_w,
		data_config->ovl_config[0].src_h);

	DISP_PRINTF(DDP_RESOLUTION_LOG,
		    "config_input_multiple idx %x -l0(%d %d) l1(%d %d) l2(%d %d) l3(%d %d) dst(%d %d)\n",
		    idx, data_config->ovl_config[0].src_w, data_config->ovl_config[0].src_h,
		    data_config->ovl_config[1].src_w, data_config->ovl_config[1].src_h,
		    data_config->ovl_config[2].src_w, data_config->ovl_config[2].src_h,
		    data_config->ovl_config[3].src_w, data_config->ovl_config[3].src_h,
		    data_config->dst_w, data_config->dst_h);


	return ret;
}

int ext_disp_is_alive(void)
{
	unsigned int temp = 0;
	DISPFUNC();
	_ext_disp_path_lock();
	temp = pgc->state;
	_ext_disp_path_unlock();

	return temp;
}

int ext_disp_is_sleepd(void)
{
	unsigned int temp = 0;
	/* DISPFUNC(); */
	_ext_disp_path_lock();
	temp = !pgc->state;
	_ext_disp_path_unlock();

	return temp;
}



int ext_disp_get_width(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params) {
		return pgc->plcm->params->width;
	} else {
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int ext_disp_get_height(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params) {
		return pgc->plcm->params->height;
	} else {
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int ext_disp_get_bpp(void)
{
	return 32;
}

int ext_disp_get_info(void *info)
{
	return 0;
}

unsigned int ext_disp_get_sess_id(void)
{
	if (is_context_inited > 0)
		return pgc->session;
	else
		return 0;
}

int ext_disp_get_pages(void)
{
	return 3;
}

int ext_disp_is_video_mode(void)
{
	/* TODO: we should store the video/cmd mode in runtime, because ROME will support cmd/vdo dynamic switch */
	return extd_drv_is_video_mode(pgc->plcm);
}

int ext_disp_diagnose(void)
{
	int ret = 0;

	DISPCHECK("ext_disp_diagnose, is_context_inited --%d\n", is_context_inited);

	if (is_context_inited > 0) {
		DISPMSG("========================= dump hdmi begin =========================\n");
		if (_is_hdmi_decouple_mode(pgc->mode) && (NULL != pgc->ovl2mem_path_handle))
			dpmgr_check_status(pgc->ovl2mem_path_handle);

		if (NULL != pgc->dpmgr_handle)
			dpmgr_check_status(pgc->dpmgr_handle);

		ddp_dump_analysis(DISP_MODULE_CONFIG);
		ddp_dump_reg(DISP_MODULE_CONFIG);

		ddp_dump_analysis(DISP_MODULE_MUTEX);
		ddp_dump_reg(DISP_MODULE_MUTEX);

		DISPMSG("========================= dump hdmi finish ========================\n");
	}

	return ret;
}

CMDQ_SWITCH ext_disp_cmdq_enabled(void)
{
	return ext_disp_use_cmdq;
}

int ext_disp_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq)
{
	_ext_disp_path_lock();

	ext_disp_use_cmdq = use_cmdq;
	DISPCHECK("display driver use %s to config register now\n",
		  (use_cmdq == CMDQ_ENABLE) ? "CMDQ" : "CPU");

	_ext_disp_path_unlock();
	return ext_disp_use_cmdq;
}

/* for dump decouple internal buffer */
#define COPY_SIZE 512
void extd_disp_dump_decouple_buffer(void)
{
	char *file_name = "hdmidcbuf.bin";
	char fileName[20];
	mm_segment_t fs;
	struct file *fp = NULL;
	char buffer[COPY_SIZE];
	unsigned char *pBuffer;
	unsigned int bufferSize = extd_display_get_width() * extd_display_get_height() * 3;
	int i = 0;

	if (!_is_hdmi_decouple_mode(pgc->mode)) {
		DISPERR("extd_disp_dump_decouple_buffer : is not DECOUPLE mode, return\n");
		return;
	}

	_ext_disp_path_lock();

	if (hdmi_use_original_buffer)
		pBuffer = (unsigned char *)dc_vAddr[pgc->dc_buf_id];
	else
		pBuffer = (unsigned char *)split_dc_vAddr[pgc->dc_buf_id];

	memset(fileName, 0, 20);
	if (NULL != file_name && *file_name != '\0')
		sprintf(fileName, "/sdcard/%s", file_name);

	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0x644);

	/* write date */
	for (i = 0; i < bufferSize / COPY_SIZE; i++) {
		/* DISPMSG("[%4d] memcpy pBuffer(%p)\n", i+1, pBuffer); */
		memcpy(buffer, pBuffer, COPY_SIZE);
		fp->f_op->write(fp, buffer, COPY_SIZE, &fp->f_pos);
		pBuffer += COPY_SIZE;
	}

	filp_close(fp, NULL);
	set_fs(fs);

	_ext_disp_path_unlock();

	DISPMSG("extd_disp_dump_decouple_buffer end\n");
}

