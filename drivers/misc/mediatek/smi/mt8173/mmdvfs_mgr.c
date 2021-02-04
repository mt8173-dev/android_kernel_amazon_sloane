#include <linux/uaccess.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <mach/mt_smi.h>
#include <mach/mt_vcore_dvfs.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>

#include <linux/mtk_gpu_utility.h>
#include "primary_display.h"
#include "mmdvfs_mgr.h"

#undef pr_fmt
#define pr_fmt(fmt) "[" MMDVFS_LOG_TAG "]" fmt

/*MMDVFS SWITCH.
6595 : NO MMDVFS
6795 : YES MMDVFS
8173 : NO temp. HW support MMDVFS*/
#define MMDVFS_ENABLE	0

/* WQHD MMDVFS SWITCH */
#define MMDVFS_ENABLE_WQHD	0

#define MMDVFS_GPU_LOADING_NUM	30
#define MMDVFS_GPU_LOADING_START_INDEX	10
#define MMDVFS_GPU_LOADING_SAMPLE_DURATION_IN_MS	100
#define MMDVFS_GPU_LOADING_THRESHOLD	18

#if (MMDVFS_GPU_LOADING_START_INDEX >= MMDVFS_GPU_LOADING_NUM)
#error "start index too large"
#endif

/* mmdvfs MM sizes */
#define MMDVFS_PIXEL_NUM_720P	(1280 * 720)
#define MMDVFS_PIXEL_NUM_2160P	(3840 * 2160)
#define MMDVFS_PIXEL_NUM_1080P	(2100 * 1300)
#define MMDVFS_PIXEL_NUM_2M		(2100 * 1300)
/* 13M sensor */
#define MMDVFS_PIXEL_NUM_SENSOR_FULL (13000000)

/* mmdvfs display sizes */
#define MMDVFS_DISPLAY_SIZE_FHD	(1920 * 1216)

static mmdvfs_voltage_enum g_mmdvfs_scenario_voltage[SMI_BWC_SCEN_CNT] = { MMDVFS_VOLTAGE_DEFAULT };

static mmdvfs_voltage_enum g_mmdvfs_current_step;
static MTK_SMI_BWC_MM_INFO *g_mmdvfs_info;
static MTK_MMDVFS_CMD g_mmdvfs_cmd;

/* mmdvfs timer for monitor gpu loading */
typedef struct {
	/* linux timer */
	struct timer_list timer;

	/* work q */
	struct workqueue_struct *work_queue;
	struct work_struct work;

	/* data payload */
	unsigned int gpu_loadings[MMDVFS_GPU_LOADING_NUM];
	int gpu_loading_index;
} mmdvfs_gpu_monitor_struct;

typedef struct {
	spinlock_t scen_lock;
	int is_mhl_enable;
	mmdvfs_gpu_monitor_struct gpu_monitor;

} mmdvfs_context_struct;

/* mmdvfs_query() return value, remember to sync with user space */
typedef enum {
	MMDVFS_STEP_LOW = 0,
	MMDVFS_STEP_HIGH,

	MMDVFS_STEP_LOW2LOW,	/* LOW */
	MMDVFS_STEP_HIGH2LOW,	/* LOW */
	MMDVFS_STEP_LOW2HIGH,	/* HIGH */
	MMDVFS_STEP_HIGH2HIGH,	/* HIGH */
} mmdvfs_step_enum;

/* lcd size */
typedef enum {
	MMDVFS_LCD_SIZE_FHD,
	MMDVFS_LCD_SIZE_WQHD,
	MMDVFS_LCD_SIZE_END_OF_ENUM
} mmdvfs_lcd_size_enum;

static mmdvfs_context_struct g_mmdvfs_mgr_cntx;
static mmdvfs_context_struct *const g_mmdvfs_mgr = &g_mmdvfs_mgr_cntx;

static mmdvfs_lcd_size_enum mmdvfs_get_lcd_resolution(void)
{
	/* screen size */
	if (DISP_GetScreenWidth() * DISP_GetScreenHeight() <= MMDVFS_DISPLAY_SIZE_FHD)
		return MMDVFS_LCD_SIZE_FHD;

	return MMDVFS_LCD_SIZE_WQHD;
}

static mmdvfs_voltage_enum mmdvfs_get_default_step(void)
{
	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_FHD)
		return MMDVFS_VOLTAGE_LOW;

	return MMDVFS_VOLTAGE_HIGH;
}

static mmdvfs_voltage_enum mmdvfs_get_current_step(void)
{
	return g_mmdvfs_current_step;
}

static mmdvfs_voltage_enum mmdvfs_query(MTK_SMI_BWC_SCEN scenario, MTK_MMDVFS_CMD *cmd)
{
	mmdvfs_voltage_enum step = mmdvfs_get_default_step();
	unsigned int venc_size;
	MTK_MMDVFS_CMD cmd_default;

	venc_size = g_mmdvfs_info->video_record_size[0] * g_mmdvfs_info->video_record_size[1];

	/* use default info */
	if (cmd == NULL) {
		memset(&cmd_default, 0, sizeof(MTK_MMDVFS_CMD));
		cmd_default.camera_mode = MMDVFS_CAMERA_MODE_FLAG_DEFAULT;
		cmd = &cmd_default;
	}

	/* collect the final information */
	if (cmd->sensor_size == 0)
		cmd->sensor_size = g_mmdvfs_cmd.sensor_size;

	if (cmd->sensor_fps == 0)
		cmd->sensor_fps = g_mmdvfs_cmd.sensor_fps;

	if (cmd->camera_mode == MMDVFS_CAMERA_MODE_FLAG_DEFAULT)
		cmd->camera_mode = g_mmdvfs_cmd.camera_mode;

	/* HIGH level scenarios */
	switch (scenario) {
	case SMI_BWC_SCEN_VR:
		/* VR4K high */
		if (cmd->sensor_size >= MMDVFS_PIXEL_NUM_2160P) {
			step = MMDVFS_VOLTAGE_HIGH;
		} else if ((cmd->sensor_size > MMDVFS_PIXEL_NUM_SENSOR_FULL) &&	/* binning off = sensor full size */
			   ((cmd->camera_mode & MMDVFS_CAMERA_MODE_FLAG_PIP) ||
			    (cmd->camera_mode & (MMDVFS_CAMERA_MODE_FLAG_VFB |
						 MMDVFS_CAMERA_MODE_FLAG_EIS_2_0)))) {
			/* binning off, 1080P && (PIP == 1 or (VFB && EIS == 1)) */
			step = MMDVFS_VOLTAGE_HIGH;
		}
		break;

	case SMI_BWC_SCEN_VR_SLOW:
		/* >= 120 fps SLOW MOTION high */
		if (cmd->sensor_fps >= 120)
			step = MMDVFS_VOLTAGE_HIGH;
		break;

	default:
		break;
	}

	return step;
}

static void mmdvfs_update_cmd(MTK_MMDVFS_CMD *cmd)
{
	if (cmd == NULL)
		return;

	if (cmd->sensor_size)
		g_mmdvfs_cmd.sensor_size = cmd->sensor_size;

	if (cmd->sensor_fps)
		g_mmdvfs_cmd.sensor_fps = cmd->sensor_fps;

	if (cmd->camera_mode != MMDVFS_CAMERA_MODE_FLAG_DEFAULT)
		g_mmdvfs_cmd.camera_mode = cmd->camera_mode;
}

static void mmdvfs_dump_info(void)
{
	MMDVFSMSG("CMD %d %d %d\n", g_mmdvfs_cmd.sensor_size, g_mmdvfs_cmd.sensor_fps,
		  g_mmdvfs_cmd.camera_mode);
	MMDVFSMSG("INFO VR %d %d\n", g_mmdvfs_info->video_record_size[0],
		  g_mmdvfs_info->video_record_size[1]);
}

static void mmdvfs_timer_callback(unsigned long data)
{
	mmdvfs_gpu_monitor_struct *gpu_monitor = (mmdvfs_gpu_monitor_struct *) data;

	unsigned int gpu_loading = 0;

	if (mtk_get_gpu_loading(&gpu_loading)) {
		/* MMDVFSMSG("gpuload %d %ld\n", gpu_loading, jiffies_to_msecs(jiffies)); */
	}

	/* store gpu loading into the array */
	gpu_monitor->gpu_loadings[gpu_monitor->gpu_loading_index++] = gpu_loading;

	/* fire another timer until the end */
	if (gpu_monitor->gpu_loading_index < MMDVFS_GPU_LOADING_NUM - 1) {
		mod_timer(&gpu_monitor->timer,
			  jiffies + msecs_to_jiffies(MMDVFS_GPU_LOADING_SAMPLE_DURATION_IN_MS));
	} else {
		/* the final timer */
		int i;
		int avg_loading;
		unsigned int sum = 0;

		for (i = MMDVFS_GPU_LOADING_START_INDEX; i < MMDVFS_GPU_LOADING_NUM; i++)
			sum += gpu_monitor->gpu_loadings[i];

		avg_loading = sum / MMDVFS_GPU_LOADING_NUM;

		MMDVFSMSG("gpuload %d AVG %d\n", jiffies_to_msecs(jiffies), avg_loading);

		/* drops to low step if the gpu loading is low */
		if (avg_loading <= MMDVFS_GPU_LOADING_THRESHOLD)
			queue_work(gpu_monitor->work_queue, &gpu_monitor->work);
	}

}

static void mmdvfs_gpu_monitor_work(struct work_struct *work)
{
	MMDVFSMSG("WQ %d\n", jiffies_to_msecs(jiffies));
}

static void mmdvfs_init_gpu_monitor(mmdvfs_gpu_monitor_struct *gm)
{
	struct timer_list *gpu_timer = &gm->timer;

	/* setup gpu monitor timer */
	setup_timer(gpu_timer, mmdvfs_timer_callback, (unsigned long)gm);

	gm->work_queue = create_singlethread_workqueue("mmdvfs_gpumon");
	INIT_WORK(&gm->work, mmdvfs_gpu_monitor_work);
}

#if MMDVFS_ENABLE_WQHD

static void mmdvfs_start_gpu_monitor(mmdvfs_gpu_monitor_struct *gm)
{
	struct timer_list *gpu_timer = &gm->timer;

	gm->gpu_loading_index = 0;
	memset(gm->gpu_loadings, 0, sizeof(unsigned int) * MMDVFS_GPU_LOADING_NUM);

	mod_timer(gpu_timer, jiffies + msecs_to_jiffies(MMDVFS_GPU_LOADING_SAMPLE_DURATION_IN_MS));
}

static void mmdvfs_stop_gpu_monitor(mmdvfs_gpu_monitor_struct *gm)
{
	struct timer_list *gpu_timer = &gm->timer;

	/* flush workqueue */
	flush_workqueue(gm->work_queue);
	/* delete timer */
	del_timer(gpu_timer);
}

#endif				/* MMDVFS_ENABLE_WQHD */

int mmdvfs_set_step(MTK_SMI_BWC_SCEN scenario, mmdvfs_voltage_enum step)
{
	int i, scen_index;
	mmdvfs_voltage_enum final_step = mmdvfs_get_default_step();

#if !MMDVFS_ENABLE
	return 0;
#endif

#if !MMDVFS_ENABLE_WQHD
	/* do nothing if disable MMDVFS in WQHD */
	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD)
		return 0;
#endif	/* MMDVFS_ENABLE_WQHD */

	MMDVFSMSG("MMDVFS set voltage scen %d step %d\n", scenario, step);

	if (scenario >= SMI_BWC_SCEN_CNT || (scenario < SMI_BWC_SCEN_NORMAL)) {
		MMDVFSERR("invalid scenario\n");
		return -1;
	}

	/* dump information */
	mmdvfs_dump_info();

	/* go through all scenarios to decide the final step */
	scen_index = (int)scenario;

	spin_lock(&g_mmdvfs_mgr->scen_lock);

	g_mmdvfs_scenario_voltage[scen_index] = step;

	/* one high = final high */
	for (i = 0; i < SMI_BWC_SCEN_CNT; i++) {
		if (g_mmdvfs_scenario_voltage[i] == MMDVFS_VOLTAGE_HIGH) {
			final_step = MMDVFS_VOLTAGE_HIGH;
			break;
		}
	}

	g_mmdvfs_current_step = final_step;

	spin_unlock(&g_mmdvfs_mgr->scen_lock);

	MMDVFSMSG("MMDVFS set voltage scen %d step %d final %d\n", scenario, step, final_step);

#if	MMDVFS_ENABLE
	/* call vcore dvfs API */
	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {	/* WQHD */
		/* TODO: use final_step and retry vcore dvfs */
		MMDVFSMSG("HIGH to %d\n", step);
		if (step == MMDVFS_VOLTAGE_LOW)
			vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_LOW_PWR);
		else
			vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_UNREQ);

	} else {		/* FHD */
		MMDVFSMSG("FHD %d\n", final_step);
		if (final_step == MMDVFS_VOLTAGE_HIGH)
			vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_PERF);
		else
			vcorefs_request_dvfs_opp(KR_MM_SCEN, OPPI_UNREQ);
	}
#endif

	return 0;
}

void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd)
{
#if !MMDVFS_ENABLE
	return;
#endif

	MMDVFSMSG("MMDVFS cmd %u %d\n", cmd->type, cmd->scen);

	switch (cmd->type) {
	case MTK_MMDVFS_CMD_TYPE_SET:
		/* save cmd */
		mmdvfs_update_cmd(cmd);
		cmd->ret = mmdvfs_set_step(cmd->scen, mmdvfs_query(cmd->scen, cmd));
		break;

	case MTK_MMDVFS_CMD_TYPE_QUERY:
		{		/* query with some parameters */
			if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {
				/* QUERY ALWAYS HIGH for WQHD */
				cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2HIGH;
			} else {	/* FHD */
				mmdvfs_voltage_enum query_voltage = mmdvfs_query(cmd->scen, cmd);
				mmdvfs_voltage_enum current_voltage = mmdvfs_get_current_step();

				if (current_voltage < query_voltage) {
					cmd->ret = (unsigned int)MMDVFS_STEP_LOW2HIGH;
				} else if (current_voltage > query_voltage) {
					cmd->ret = (unsigned int)MMDVFS_STEP_HIGH2LOW;
				} else {
					cmd->ret =
					    (unsigned int)(query_voltage ==
							   MMDVFS_VOLTAGE_HIGH ?
							   MMDVFS_STEP_HIGH2HIGH :
							   MMDVFS_STEP_LOW2LOW);
				}
			}
			/* cmd->ret = (unsigned int)query_voltage; */
			break;
		}

	default:
		MMDVFSMSG("invalid mmdvfs cmd\n");
		BUG();
		break;
	}
}

void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen)
{
#if !MMDVFS_ENABLE
	return;
#endif

	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {
#if MMDVFS_ENABLE_WQHD
		if (scen == SMI_BWC_SCEN_VP)
			mmdvfs_stop_gpu_monitor(&g_mmdvfs_mgr->gpu_monitor);
#endif				/* MMDVFS_ENABLE_WQHD */
	}

	/* reset scenario voltage to default when it exits */
	mmdvfs_set_step(scen, mmdvfs_get_default_step());
}

void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen)
{
#if !MMDVFS_ENABLE
	return;
#endif

	if (mmdvfs_get_lcd_resolution() == MMDVFS_LCD_SIZE_WQHD) {
#if MMDVFS_ENABLE_WQHD
		if (scen == SMI_BWC_SCEN_VP)
			mmdvfs_start_gpu_monitor(&g_mmdvfs_mgr->gpu_monitor);
#endif				/* MMDVFS_ENABLE_WQHD */
	} else {		/* FHD */
		switch (scen) {
		case SMI_BWC_SCEN_ICFP:
			mmdvfs_set_step(scen, MMDVFS_VOLTAGE_HIGH);
			break;

		case SMI_BWC_SCEN_VR:
		case SMI_BWC_SCEN_VR_SLOW:
			mmdvfs_set_step(scen, mmdvfs_query(scen, NULL));
			break;

		default:
			break;
		}
	}
}

void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info)
{
#if !MMDVFS_ENABLE
	return;
#endif

	spin_lock_init(&g_mmdvfs_mgr->scen_lock);
	/* set current step as the default step */
	g_mmdvfs_current_step = mmdvfs_get_default_step();

	g_mmdvfs_info = info;

	mmdvfs_init_gpu_monitor(&g_mmdvfs_mgr->gpu_monitor);
}

void mmdvfs_mhl_enable(int enable)
{
	g_mmdvfs_mgr->is_mhl_enable = enable;
}
