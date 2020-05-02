#include <linux/delay.h>
#include "disp_ovl_engine_hw.h"
#include "ddp_reg.h"
#include "ddp_debug.h"

#ifdef DISP_OVL_ENGINE_HW_SUPPORT
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <mach/m4u.h>
#include "disp_ovl_engine_core.h"
#include "ddp_hal.h"


#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include <mach/m4u_port.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif


/* Parameter */
static DISP_OVL_ENGINE_INSTANCE disp_ovl_engine_params;


/* Irq callback */
void (*disp_ovl_engine_hw_irq_callback) (unsigned int param) = NULL;
void disp_ovl_engine_hw_ovl_wdma_irq_handler(unsigned int param);
void disp_ovl_engine_hw_ovl_rdma_irq_handler(unsigned int param);

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
/* these 2 APIs are used for accessing ddp_session / ddp_mem_session with TEE */
extern KREE_SESSION_HANDLE ddp_session_handle(void);
extern KREE_SESSION_HANDLE ddp_mem_session_handle(void);

void *disp_ovl_engine_hw_allocate_secure_memory(int size);
void disp_ovl_engine_hw_free_secure_memory(void *mem_handle);
#endif

void disp_ovl_engine_hw_init(void)
{
	memset(&disp_ovl_engine_params, 0, sizeof(DISP_OVL_ENGINE_INSTANCE));

	disp_path_register_ovl_wdma_callback(disp_ovl_engine_hw_ovl_wdma_irq_handler, 0);
	disp_path_register_ovl_rdma_callback(disp_ovl_engine_hw_ovl_rdma_irq_handler, 0);
}


void disp_ovl_engine_hw_set_params(DISP_OVL_ENGINE_INSTANCE *params)
{
	memcpy(&disp_ovl_engine_params, params, sizeof(DISP_OVL_ENGINE_INSTANCE));
	atomic_set(&params->OverlaySettingDirtyFlag, 0);
	atomic_set(&params->OverlaySettingApplied, 1);
}


int g_ovl_wdma_irq_ignore = 0;
void disp_ovl_engine_hw_ovl_wdma_irq_handler(unsigned int param)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_hw_ovl_wdma_irq_handler\n");

	if (g_ovl_wdma_irq_ignore) {
		g_ovl_wdma_irq_ignore = 0;
		return;
	}

	if (disp_ovl_engine_hw_irq_callback != NULL)
		disp_ovl_engine_hw_irq_callback(param);
}

void disp_ovl_engine_hw_ovl_rdma_irq_handler(unsigned int param)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_hw_ovl_rdma_irq_handler\n");

	if (disp_ovl_engine_hw_irq_callback != NULL)
		disp_ovl_engine_hw_irq_callback(param);
}


static void _rdma0_irq_handler(unsigned int param)
{
/* unsigned int rdma_buffer_addr; */
	int lcm_width, lcm_height, lcm_bpp;	/* , tmp; */
/* struct disp_path_config_struct rConfig = {0}; */

	lcm_width = DISP_GetScreenWidth();
	lcm_height = DISP_GetScreenHeight();
	lcm_bpp = 3;		/* (DISP_GetScreenBpp() + 7) >> 3; */

	/* DISP_OVL_ENGINE_DBG("rdma0 irq interrupt, param: %d\n", param); */

	if (param & 0x4) {	/* rdma0 frame end */
#if 0
		tmp = disp_ovl_engine.RdmaRdIdx + 1;
		tmp %= OVL_ENGINE_OVL_BUFFER_NUMBER;
		if (tmp == disp_ovl_engine.OvlWrIdx) {
			DISP_OVL_ENGINE_DBG("OVL BuffCtl WDMA1 hang (%d), Show same buffer\n",
					    disp_ovl_engine.OvlWrIdx);
		} else {
			/* disp_path_get_mutex(); */
			rdma_buffer_addr =
			    disp_ovl_engine.Ovlmva +
			    lcm_width * lcm_height * lcm_bpp * disp_ovl_engine.RdmaRdIdx;
			DISP_REG_SET(DISP_REG_RDMA_MEM_START_ADDR, rdma_buffer_addr);
			DISP_OVL_ENGINE_DBG("OVL BuffCtl RdmaRdIdx: 0x%x Addr: 0x%x\n",
					    disp_ovl_engine.RdmaRdIdx, rdma_buffer_addr);
			disp_ovl_engine.RdmaRdIdx++;
			disp_ovl_engine.RdmaRdIdx %= OVL_ENGINE_OVL_BUFFER_NUMBER;
			/* disp_path_release_mutex(); */
		}
#else
		disp_ovl_engine.RdmaRdIdx = disp_ovl_engine.OvlWrIdx;
#endif
	}
}

extern void disp_register_intr(unsigned int irq, unsigned int secure);
unsigned int gOvlWdmaMutexID = 4;
static int OvlSecure;		/* Todo, this suggest that only one HW overlay. */
static int RdmaSecure;
void disp_ovl_engine_trigger_hw_overlay_decouple(void)
{
	int layer_id;
	int OvlSecureNew = 0;

	DISP_OVL_ENGINE_DBG("disp_ovl_engine_trigger_hw_overlay\n");

	OVLReset();
	WDMAReset(1);

	disp_path_config_OVL_WDMA_path(gOvlWdmaMutexID, TRUE, FALSE);

	disp_path_get_mutex_(gOvlWdmaMutexID);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
		    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
		     LAYER_SECURE_BUFFER))
			OvlSecureNew = 1;
	}

	if (OvlSecure != OvlSecureNew) {
		if (OvlSecureNew) {
			OvlSecure = OvlSecureNew;

			disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
		}
	}

	disp_path_config_OVL_WDMA(&(disp_ovl_engine_params.MemOutConfig), OvlSecure);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		disp_ovl_engine_params.cached_layer_config[layer_id].layer = layer_id;

		disp_path_config_layer_ovl_engine(&
						  (disp_ovl_engine_params.cached_layer_config
						   [layer_id]), OvlSecure);
	}

	/* disp_dump_reg(DISP_MODULE_WDMA1); */
	/* disp_dump_reg(DISP_MODULE_OVL); */

	disp_path_release_mutex_(gOvlWdmaMutexID);

	if (OvlSecure != OvlSecureNew) {
		if (!OvlSecureNew) {
			OvlSecure = OvlSecureNew;

			disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
		}
	}
}

void disp_ovl_engine_config_overlay(void)
{
	unsigned int i = 0;
	int dirty;
	int layer_id;
	int OvlSecureNew = 0;

	disp_path_config_layer_ovl_engine_control(true);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
		    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
		     LAYER_SECURE_BUFFER))
			OvlSecureNew = 1;
	}

	if (OvlSecure != OvlSecureNew) {
		OvlSecure = OvlSecureNew;

		disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
	}


	disp_path_get_mutex();
	for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
		if (disp_ovl_engine_params.cached_layer_config[i].isDirty) {
			dirty |= 1 << i;
			disp_path_config_layer_ovl_engine
			    (&disp_ovl_engine_params.cached_layer_config[i], OvlSecure);
			disp_ovl_engine_params.cached_layer_config[i].isDirty = false;
		}
	}
	disp_path_release_mutex();
}

void disp_ovl_engine_direct_link_overlay(void)
{
/* unsigned int i = 0; */
/* unsigned int dirty = 0; */
	int lcm_width = DISP_GetScreenWidth();
	int buffer_bpp = 3;
	static int first_boot = 1;

	/* Remove OVL and WDMA1 from ovl_wdma_mutex, because these two will get into mutex 0 */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gOvlWdmaMutexID), 0);

	/* setup the direct link of overlay - rdma */
	DISP_OVL_ENGINE_INFO("direct link overlay, addr=0x%x\n",
			     disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx]);

	DISP_OVL_ENGINE_DBG
	    ("ovl addr: 0x%x srcModule: %d dstModule: %d inFormat: %d outFormat: %d\n",
	     disp_ovl_engine_params.path_info.ovl_config.addr,
	     disp_ovl_engine_params.path_info.srcModule, disp_ovl_engine_params.path_info.dstModule,
	     disp_ovl_engine_params.path_info.inFormat, disp_ovl_engine_params.path_info.outFormat);


	/* config m4u */
	/* if(lcm_params->dsi.mode != CMD_MODE) */
	{
		/* disp_path_get_mutex(); */
	}
#if 1
	if (1 == first_boot) {
		M4U_PORT_STRUCT portStruct;

		DISP_OVL_ENGINE_DBG("config m4u start\n\n");

		portStruct.ePortID = M4U_PORT_OVL_CH0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
		first_boot = 0;
	}
#endif

#if 0
	if (disp_ovl_engine_params.fgNeedConfigM4U) {
		M4U_PORT_STRUCT portStruct;

		DISP_OVL_ENGINE_DBG("config m4u start\n\n");

		portStruct.ePortID = M4U_PORT_OVL_CH2;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);


		portStruct.ePortID = M4U_PORT_OVL_CH3;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
		disp_ovl_engine_params.fgNeedConfigM4U = FALSE;
		disp_ovl_engine.Instance[disp_ovl_engine_params.index].fgNeedConfigM4U = FALSE;

		portStruct.ePortID = M4U_PORT_RDMA0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 0;
		portStruct.Security = 0;
		portStruct.domain = 1;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);

		DISP_OVL_ENGINE_DBG("config m4u done\n");
	}
#endif

	disp_path_register_ovl_rdma_callback(disp_ovl_engine_hw_ovl_rdma_irq_handler, 0);
	if (disp_is_cb_registered(DISP_MODULE_RDMA0, _rdma0_irq_handler))
		disp_unregister_irq(DISP_MODULE_RDMA0, _rdma0_irq_handler);

	disp_path_get_mutex();

	if (disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx] != 0) {
		disp_ovl_engine_params.path_info.ovl_config.fmt = eRGB888;
		disp_ovl_engine_params.path_info.ovl_config.src_pitch = lcm_width * buffer_bpp;
		disp_ovl_engine_params.path_info.ovl_config.addr =
		    disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];
		disp_ovl_engine_params.path_info.addr =
					disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];
	}
	disp_path_config(&(disp_ovl_engine_params.path_info));
	disp_path_release_mutex();

	/* if(lcm_params->dsi.mode != CMD_MODE) */
	{
		/* disp_path_release_mutex(); */
	}
	if (disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx] != 0) {
		disp_module_clock_off(DISP_MODULE_GAMMA, "OVL");
		disp_module_clock_off(DISP_MODULE_WDMA1, "OVL");
	}
    /*************************************************/
	/* Ultra config */

	DISP_REG_SET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING, 0x40402020);

	/* disp_wdma1 ultra */
	DISP_REG_SET(DISP_REG_WDMA_BUF_CON1 + 0x1000, 0x800800ff);
    /*************************************************/

	/* pr_info("DUMP register =============================================\n"); */
	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */
	/* pr_info("DUMP register end =============================================\n"); */

}

/* static int g_disp_mutex = 0; */
static int g_ovl_wdma_mutex = 4;

int disp_ovl_engine_indirect_link_overlay(void)
{
	/* Steps for reconfig display path
	   1. allocate internal buffer
	   2. set overlay output to buffer
	   3. config rdma read from memory and change mutex setting
	   4. config overlay to wdma, reconfig new mutex
	 */
	int lcm_width, lcm_height;
	int buffer_bpp;
	int layer_id;
	int tmpBufferSize;
	int temp_va = 0;
	static int internal_buffer_init;
	struct disp_path_config_mem_out_struct rMemOutConfig = { 0 };
/* struct disp_path_config_struct rConfig = {0}; */
	struct disp_path_config_struct config = { 0 };
	int i = 0;

	/* pr_info("DUMP register =============================================\n"); */
	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */
	/* pr_info("DUMP register end =============================================\n"); */

	DISP_OVL_ENGINE_INFO("indirect link overlay\n");

	DISP_OVL_ENGINE_DBG
	    ("ovl addr: 0x%x srcModule: %d dstModule: %d inFormat: %d outFormat: %d\n",
	     disp_ovl_engine_params.path_info.ovl_config.addr,
	     disp_ovl_engine_params.path_info.srcModule, disp_ovl_engine_params.path_info.dstModule,
	     disp_ovl_engine_params.path_info.inFormat, disp_ovl_engine_params.path_info.outFormat);

	/* step 1, alloc resource */
	lcm_width = DISP_GetScreenWidth();
	lcm_height = DISP_GetScreenHeight();
	buffer_bpp = 3;		/* (DISP_GetScreenBpp() + 7) >> 3; */
	tmpBufferSize = lcm_width * lcm_height * buffer_bpp * OVL_ENGINE_OVL_BUFFER_NUMBER;

	DISP_OVL_ENGINE_DBG("lcm_width: %d, lcm_height: %d, buffer_bpp: %d\n",
			    lcm_width, lcm_height, buffer_bpp);

	if (0 == internal_buffer_init) {
		internal_buffer_init = 1;

		DISP_OVL_ENGINE_DBG("indirect link alloc internal buffer\n");

		temp_va = (unsigned int)vmalloc(tmpBufferSize);

		if (((void *)temp_va) == NULL) {
			DISP_OVL_ENGINE_DBG("vmalloc %dbytes fail\n", tmpBufferSize);
			return OVL_ERROR;
		}

		if (m4u_alloc_mva(M4U_CLNTMOD_WDMA,
				  temp_va, tmpBufferSize, 0, 0, &disp_ovl_engine.Ovlmva)) {
			DISP_OVL_ENGINE_DBG("m4u_alloc_mva for disp_ovl_engine.Ovlmva fail\n");
			return OVL_ERROR;
		}

		m4u_dma_cache_maint(M4U_CLNTMOD_WDMA,
				    (void const *)temp_va, tmpBufferSize, DMA_BIDIRECTIONAL);

		{
			int i;

			for (i = 0; i < OVL_ENGINE_OVL_BUFFER_NUMBER; i++) {
				disp_ovl_engine.OvlBufAddr[i] =
				    disp_ovl_engine.Ovlmva +
				    lcm_width * lcm_height * buffer_bpp * i;

				disp_ovl_engine.OvlBufSecurity[i] = FALSE;
			}
		}

		DISP_OVL_ENGINE_DBG("M4U alloc mva: 0x%x va: 0x%x size: 0x%x\n",
				    disp_ovl_engine.Ovlmva, temp_va, tmpBufferSize);
	}

	disp_module_clock_on(DISP_MODULE_GAMMA, "OVL");
	disp_module_clock_on(DISP_MODULE_WDMA1, "OVL");
	/* disp_module_clock_on(DISP_MODULE_RDMA1, "OVL"); */

	disp_ovl_engine.OvlWrIdx = 0;
	disp_ovl_engine.RdmaRdIdx = 0;
	/* temp_va = disp_ovl_engine.Ovlmva + lcm_width * lcm_height * buffer_bpp * disp_ovl_engine.OvlWrIdx; */

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	/* Allocate or free secure buffer */
	if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] != OvlSecure) {
		if (OvlSecure) {
			/* Allocate secure buffer */
			disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
			    (unsigned int)disp_ovl_engine_hw_allocate_secure_memory(lcm_width *
										    lcm_height *
										    buffer_bpp);
			disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = TRUE;
		} else {
			/* Free secure buffer */
			disp_ovl_engine_hw_free_secure_memory((void
							       *)(disp_ovl_engine.OvlBufAddr
								  [disp_ovl_engine.OvlWrIdx]));
			disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
			    disp_ovl_engine.Ovlmva +
			    lcm_width * lcm_height * buffer_bpp * disp_ovl_engine.OvlWrIdx;
			disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = FALSE;
		}
	}
#endif

	temp_va = disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx];

	disp_path_unregister_ovl_rdma_callback(disp_ovl_engine_hw_ovl_rdma_irq_handler, 0);

	/* step 2, config WDMA1 */
	{
		M4U_PORT_STRUCT portStruct;
		portStruct.ePortID = M4U_PORT_WDMA1;	/* hardware port ID, defined in M4U_PORT_ID_ENUM //M4U_PORT_WDMA1; */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 1;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
	}
	rMemOutConfig.dirty = TRUE;
	rMemOutConfig.dstAddr = temp_va;
	rMemOutConfig.enable = TRUE;
	rMemOutConfig.outFormat = eRGB888;
	rMemOutConfig.srcROI.x = 0;
	rMemOutConfig.srcROI.y = 0;
	rMemOutConfig.srcROI.width = lcm_width;
	rMemOutConfig.srcROI.height = lcm_height;
	rMemOutConfig.security = OvlSecure;

	disp_path_config_OVL_WDMA_path(0, FALSE, TRUE);

	disp_path_get_mutex();
	/* disp_path_config_mem_out(&rMemOutConfig); */
	disp_path_config_OVL_WDMA(&rMemOutConfig, OvlSecure);

	/*disp_path_config_OVL_WDMA_path(g_ovl_wdma_mutex); */
	/*disp_path_get_mutex_(g_ovl_wdma_mutex); */
	/*disp_path_config_OVL_WDMA(&rMemOutConfig, OvlSecure); */
	/*should use couple instance's overlay info to config */
	for (i = 0; i < OVL_ENGINE_INSTANCE_MAX_INDEX; i++) {
		if ((COUPLE_MODE == disp_ovl_engine.Instance[i].mode)
		    && disp_ovl_engine.Instance[i].bUsed)
			break;
	}

	if (i < OVL_ENGINE_INSTANCE_MAX_INDEX) {
		/* get valid couple instace */
		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			disp_ovl_engine.Instance[i].cached_layer_config[layer_id].layer = layer_id;
			disp_path_config_layer_ovl_engine(&
							  (disp_ovl_engine.Instance[i].
							   cached_layer_config[layer_id]),
							  OvlSecure);
		}
	} else {
		/* can't get valid couple instance */
		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			disp_ovl_engine_params.cached_layer_config[layer_id].layer = layer_id;
			disp_path_config_layer_ovl_engine(&
							  (disp_ovl_engine_params.
							   cached_layer_config[layer_id]), 0);
		}
	}
	/* disp_path_release_mutex_(g_ovl_wdma_mutex); */
	disp_path_release_mutex();
	/* disp_path_wait_mem_out_done(); */
	disp_path_wait_ovl_wdma_done();

	disp_path_config_OVL_WDMA_path(0, FALSE, FALSE);

	disp_path_get_mutex();

	rMemOutConfig.enable = 0;
	/* disp_path_config_mem_out(&rMemOutConfig); */
	disp_path_config_OVL_WDMA(&rMemOutConfig, OvlSecure);

	disp_path_release_mutex();
	disp_path_wait_reg_update();
	if (0) {
		int cnt = 0;

		g_ovl_wdma_irq_ignore = 1;

		while (1) {
			if (g_ovl_wdma_irq_ignore == 0)
				break;

			msleep(1);

			cnt++;
			if (cnt > 1000) {
				DISP_OVL_ENGINE_ERR("Wait WDMA interrupt timeout\n");
				break;
			}
		}
	}


	disp_ovl_engine.OvlWrIdx++;

	/* step 3, config RDMA0 */
	#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx] != RdmaSecure) {
		if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx]) {
			RdmaSecure = disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];
			DISP_OVL_ENGINE_ERR("RdmaSecure = %d\n", RdmaSecure);
			disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);
		}
	}
	#endif
	if (RdmaSecure == 0) {
		M4U_PORT_STRUCT portStruct;
		portStruct.ePortID = M4U_PORT_RDMA0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 1;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
	}

	/*RDMAReset(0);*/
	disp_path_get_mutex();

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	#if 0
	if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx] != RdmaSecure) {
		if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx]) {
			RdmaSecure = disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];

			DISP_OVL_ENGINE_ERR("RdmaSecure = %d\n", RdmaSecure);

			disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);
		}
	}
	#endif

	if (RdmaSecure) {
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = (uint32_t) temp_va;
		param[1].value.a = disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];
		paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
		DISP_OVL_ENGINE_DBG("Rdma config handle=0x%x\n", param[0].value.a);

		ret =
		    KREE_TeeServiceCall(ddp_session_handle(),
					TZCMD_DDP_RDMA_ADDR_CONFIG, paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_OVL_ENGINE_ERR("TZCMD_DDP_RDMA_ADDR_CONFIG fail, ret=%d\n", ret);
		}
	} else
#endif
	{
		DISP_REG_SET(DISP_REG_RDMA_MEM_START_ADDR, temp_va);
	}

	disp_path_release_mutex();

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx] != RdmaSecure) {
		if (!disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx]) {
			RdmaSecure = disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];

			DISP_OVL_ENGINE_ERR("RdmaSecure = %d\n", RdmaSecure);

			disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);
		}
	}
#endif


	disp_path_wait_reg_update();
	disp_path_get_mutex();
	DISP_REG_SET(DISP_REG_BLS_RST, 0x1);
	DISP_REG_SET(DISP_REG_BLS_RST, 0x0);
	DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);
	config.srcModule = DISP_MODULE_RDMA0;
	config.inFormat = RDMA_INPUT_FORMAT_RGB888;
	config.addr = 0xFFFFFFFF;/*temp_va;	*/ /* *(volatile unsigned int *)(0xF40030A0); */
	config.pitch = lcm_width * buffer_bpp;
	config.srcHeight = lcm_height;
	config.srcWidth = lcm_width;
	config.dstModule = disp_ovl_engine.Instance[0].path_info.dstModule;
	config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
	disp_register_irq(DISP_MODULE_RDMA0, _rdma0_irq_handler);
	disp_path_config(&config);
	disp_path_release_mutex();
	disp_path_wait_reg_update();
	OVLReset();
	WDMAReset(1);

	/* step 4, because disp_path_config reconfig the path */
	/* disp_path_config_OVL_WDMA_path(g_ovl_wdma_mutex); */

    /*************************************************/
	/* Ultra config */
	DISP_REG_SET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, 0x00000000);
	DISP_REG_SET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING, 0x00000000);
	DISP_REG_SET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING, 0x00000000);
	DISP_REG_SET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING, 0x00000000);


	/* disp_wdma1 ultra */
	DISP_REG_SET(DISP_REG_WDMA_BUF_CON1 + 0x1000, 0x000800ff);
    /*************************************************/

	/* disp_dump_reg(DISP_MODULE_MUTEX); */
	return 0;
}

void disp_ovl_engine_set_overlay_to_buffer(void)
{
	if (disp_ovl_engine.bCouple) {
		disp_path_config_mem_out(&disp_ovl_engine_params.MemOutConfig);
	} else {
		/* output to overlay buffer */
	}
}

int disp_ovl_engine_trigger_hw_overlay_couple(void)
{
	struct disp_path_config_mem_out_struct rMemOutConfig = { 0 };
	unsigned int temp_va = 0;
	unsigned int width, height, bpp;
	unsigned int size = 0, layer_id;
	int tmp;
	int OvlSecureNew = 0;

	/* overlay output to internal buffer */
	if (atomic_read(&disp_ovl_engine_params.OverlaySettingDirtyFlag)) {
		/* output to buffer */
		tmp = disp_ovl_engine.OvlWrIdx + 1;
		tmp %= OVL_ENGINE_OVL_BUFFER_NUMBER;
		if (tmp == disp_ovl_engine.RdmaRdIdx) {
			DISP_OVL_ENGINE_ERR
			    ("OVL BuffCtl RDMA hang (%d), stop write (ovlWrIdx: %d)\n",
			     disp_ovl_engine.RdmaRdIdx, disp_ovl_engine.OvlWrIdx);
			disp_ovl_engine.OvlWrIdx = (disp_ovl_engine.OvlWrIdx + OVL_ENGINE_OVL_BUFFER_NUMBER - 1)
				% OVL_ENGINE_OVL_BUFFER_NUMBER;
			DISP_OVL_ENGINE_ERR("OVL BuffCtl new WrIdx: %d\n", disp_ovl_engine.OvlWrIdx);
			/*return OVL_ERROR;*/
		}

		width = DISP_GetScreenWidth();
		height = DISP_GetScreenHeight();
		bpp = 3;	/* (DISP_GetScreenBpp() + 7) >> 3; */

		DISP_OVL_ENGINE_DBG("OVL BuffCtl disp_ovl_engine.OvlWrIdx: %d\n",
				    disp_ovl_engine.OvlWrIdx);
		/* temp_va = disp_ovl_engine.Ovlmva + (width * height * bpp) * disp_ovl_engine.OvlWrIdx; */

		OVLReset();
		WDMAReset(1);

		disp_path_config_OVL_WDMA_path(gOvlWdmaMutexID, TRUE, FALSE);
		disp_path_get_mutex_(g_ovl_wdma_mutex);

		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
			    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
			     LAYER_SECURE_BUFFER))
				OvlSecureNew = 1;
		}

		if (OvlSecure != OvlSecureNew) {
			if (OvlSecureNew) {
				OvlSecure = OvlSecureNew;

				DISP_OVL_ENGINE_ERR
				    ("disp_ovl_engine_trigger_hw_overlay_couple, OvlSecure=0x%x\n",
				     OvlSecure);

				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			}
		}
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
		/* Allocate or free secure buffer */
		if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] != OvlSecureNew) {
			if (OvlSecureNew) {
				/* Allocate secure buffer */
				disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
				    (unsigned int)disp_ovl_engine_hw_allocate_secure_memory(width *
											    height *
											    bpp);
				disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = TRUE;
			} else {
				/* Free secure buffer */
				disp_ovl_engine_hw_free_secure_memory((void
								       *)(disp_ovl_engine.OvlBufAddr
									  [disp_ovl_engine.
									   OvlWrIdx]));
				disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
				    disp_ovl_engine.Ovlmva +
				    (width * height * bpp) * disp_ovl_engine.OvlWrIdx;
				disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = FALSE;
			}

			DISP_OVL_ENGINE_ERR("OvlBufSecurity[%d] = %d\n", disp_ovl_engine.OvlWrIdx,
					    OvlSecureNew);
		}
#endif

		temp_va = disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx];
		rMemOutConfig.dirty = TRUE;
		rMemOutConfig.dstAddr = temp_va;
		rMemOutConfig.enable = TRUE;
		rMemOutConfig.outFormat = eRGB888;
		rMemOutConfig.srcROI.x = 0;
		rMemOutConfig.srcROI.y = 0;
		rMemOutConfig.srcROI.width = width;
		rMemOutConfig.srcROI.height = height;
		rMemOutConfig.security = OvlSecureNew;
		disp_path_config_OVL_WDMA(&rMemOutConfig, OvlSecure);

		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			disp_ovl_engine_params.cached_layer_config[layer_id].layer = layer_id;
			disp_path_config_layer_ovl_engine(&
							  (disp_ovl_engine_params.
							   cached_layer_config[layer_id]),
							  OvlSecure);
		}
		disp_path_release_mutex_(g_ovl_wdma_mutex);

		if (OvlSecure != OvlSecureNew) {
			if (!OvlSecureNew) {
				OvlSecure = OvlSecureNew;

				DISP_OVL_ENGINE_ERR
				    ("disp_ovl_engine_trigger_hw_overlay_couple, OvlSecure=0x%x\n",
				     OvlSecure);

				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			}
		}

		/* Update RDMA */
		{
			unsigned int rdma_buffer_addr;
			int lcm_width, lcm_height, lcm_bpp;	/* , tmp; */

			lcm_width = DISP_GetScreenWidth();
			lcm_height = DISP_GetScreenHeight();
			lcm_bpp = 3;	/* (DISP_GetScreenBpp() + 7) >> 3; */

			disp_path_get_mutex();
			/* rdma_buffer_addr = disp_ovl_engine.Ovlmva + lcm_width * lcm_height * lcm_bpp * disp_ovl_engine.RdmaRdIdx; */
			rdma_buffer_addr = disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
			if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx] != RdmaSecure) {
				if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx]) {
					RdmaSecure =
					    disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.
									   RdmaRdIdx];

					DISP_OVL_ENGINE_ERR("RdmaSecure = %d\n", RdmaSecure);

					disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);
				}
			}

			if (RdmaSecure) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;

				param[0].value.a = (uint32_t) rdma_buffer_addr;
				param[1].value.a =
				    disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];
				paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
				DISP_OVL_ENGINE_DBG("Rdma config handle=0x%x\n", param[0].value.a);

				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_RDMA_ADDR_CONFIG, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					DISP_OVL_ENGINE_ERR
					    ("TZCMD_DDP_RDMA_ADDR_CONFIG fail, ret=%d\n", ret);
				}
			} else
#endif
			{
				DISP_REG_SET(DISP_REG_RDMA_MEM_START_ADDR, rdma_buffer_addr);
			}

			disp_path_release_mutex();

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
			if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx] != RdmaSecure) {
				if (!disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx]) {
					RdmaSecure =
					    disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.
									   RdmaRdIdx];

					DISP_OVL_ENGINE_ERR("RdmaSecure = %d\n", RdmaSecure);

					disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);
				}
			}
#endif

			DISP_OVL_ENGINE_DBG("OVL BuffCtl RdmaRdIdx: 0x%x Addr: 0x%x\n",
					    disp_ovl_engine.RdmaRdIdx, rdma_buffer_addr);
		}

		/* disp_path_wait_mem_out_done(); */
		disp_ovl_engine.OvlWrIdx++;
		disp_ovl_engine.OvlWrIdx %= OVL_ENGINE_OVL_BUFFER_NUMBER;
	}
	if (disp_ovl_engine_params.MemOutConfig.dirty) {

		if (atomic_read(&disp_ovl_engine_params.OverlaySettingDirtyFlag)) {
			/* copy data from overlay buffer to capture buffer */
			/* wait wdma done */
			disp_path_wait_ovl_wdma_done();
			/* sw copy ,maybe need change to wdma write... */
			size =
			    disp_ovl_engine_params.MemOutConfig.srcROI.width *
			    disp_ovl_engine_params.MemOutConfig.srcROI.width * 3;
			memcpy(&disp_ovl_engine_params.MemOutConfig.dstAddr, &temp_va, size);
		} else {
			disp_path_get_mutex_(g_ovl_wdma_mutex);
			disp_path_config_OVL_WDMA(&disp_ovl_engine_params.MemOutConfig, OvlSecure);
			disp_path_release_mutex_(g_ovl_wdma_mutex);
		}
	}

	return 0;
}

int dump_all_info(void)
{
	int i;

	DISP_OVL_ENGINE_INFO
	    ("dump_all_info ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	DISP_OVL_ENGINE_INFO
	    ("disp_ovl_engine:\nbInit %d bCouple %d bModeSwitch %d Ovlmva 0x%x OvlWrIdx %d RdmaRdIdx %d\n",
	     disp_ovl_engine.bInit, disp_ovl_engine.bCouple, disp_ovl_engine.bModeSwitch,
	     disp_ovl_engine.Ovlmva, disp_ovl_engine.OvlWrIdx, disp_ovl_engine.RdmaRdIdx);

	for (i = 0; i < 2; i++) {
		DISP_OVL_ENGINE_INFO
		    ("disp_ovl_engine.Instance[%d]:\nindex %d bUsed %d mode %d status %d "
		     "OverlaySettingDirtyFlag %d OverlaySettingApplied %d fgNeedConfigM4U %d\n", i,
		     disp_ovl_engine.Instance[i].index, disp_ovl_engine.Instance[i].bUsed,
		     disp_ovl_engine.Instance[i].mode, disp_ovl_engine.Instance[i].status,
		     *(unsigned int *)(&disp_ovl_engine.Instance[i].OverlaySettingDirtyFlag),
		     *(unsigned int *)(&disp_ovl_engine.Instance[i].OverlaySettingApplied),
		     (unsigned int)disp_ovl_engine.Instance[i].fgNeedConfigM4U);

		DISP_OVL_ENGINE_INFO("disp_ovl_engine.Instance[%d].cached_layer_config:\n "
				     "layer %d layer_en %d source %d addr 0x%x vaddr 0x%x fmt %d src_x %d"
				     "src_y %d src_w %d src_h %d src_pitch %d dst_x %d dst_y %d dst_w %d "
				     "dst_h %d isDirty %d security %d\n", i,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].layer,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].layer_en,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].source,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].addr,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].vaddr,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].fmt,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_x,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_y,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_w,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_h,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_pitch,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_x,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_y,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_w,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_h,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].isDirty,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].security);
	}



	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_MUTEX); */
	/* disp_dump_reg(DISP_MODULE_WDMA1); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */

	DISP_OVL_ENGINE_INFO
	    ("dump_all_info end --------------------------------------------------------\n");

	return 0;
}

int disp_ovl_disable_all_layer = true;
void disp_ovl_engine_trigger_hw_overlay(void)
{
	unsigned int i = 0;
	/*unsigned int layer_en_backup[DDP_OVL_LAYER_MUN] = { 0 }; */

	disp_path_config_layer_ovl_engine_control(true);

	if (disp_ovl_engine.bModeSwitch) {
		pr_err
		    ("disp_ovl_engine_trigger_hw_overlay will switch curr mode bCoupled = %d\n",
		     disp_ovl_engine.bCouple);
		/* before switch disp path, disable all overlay to avoind flicking, in boot up process, only once */
#if 0
		if (disp_ovl_disable_all_layer) {
			/* disp_path_get_mutex(); */
			DISP_OVL_ENGINE_DBG("disable all layer\n");
			for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
				/* back up all overlay's enable/disable status */
				layer_en_backup[i] =
				    disp_ovl_engine_params.cached_layer_config[i].layer_en;
				disp_ovl_engine_params.cached_layer_config[i].layer_en = false;
				disp_ovl_engine_params.cached_layer_config[i].isDirty = true;
			}
			disp_ovl_engine_config_overlay();
			disp_path_wait_reg_update();
			DISP_OVL_ENGINE_DBG("disable done\n");

			/* disp_path_release_mutex(); */

			/* roll back the layer_en flags */
			for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
				disp_ovl_engine_params.cached_layer_config[i].layer_en =
				    layer_en_backup[i];
			}
			disp_ovl_disable_all_layer = false;

			/* memcpy(&disp_directlink_instance, &disp_ovl_engine_params, sizeof(DISP_OVL_ENGINE_INSTANCE)); */
		}
#endif

		disp_path_get_mutex();

		/* DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(0), 0x0); */
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(2), 0x0);

		{
			unsigned int mutex3_mod = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(3));

			if (mutex3_mod & (1<<9)) {
				unsigned int timeout = DISP_REG_GET(DISP_REG_CONFIG_REG_UPD_TIMEOUT);
				unsigned int commit = DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT);

				DISP_MSG("[DDP] %s reset mutex 3 MOD 0x%x timeout 0x%x commit 0x%x\n",
				__func__ , mutex3_mod, timeout, commit);

				if ((commit & (1<<9)) == 0) {
					DISP_MSG("[DDP] %s commit 0x%x of bls bit(9) is 0\n", __func__ , commit);

					/* set mutex3 mod*/
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(3), 0);

					/* reset mutex3 */
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(3), 1);
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(3), 0);
				}
			}
		}

		disp_path_release_mutex();

		disp_path_wait_reg_update();

		/*DISP_REG_SET(DISP_REG_BLS_RST, 0x1); */
		/*DISP_REG_SET(DISP_REG_BLS_RST, 0x0); */
		/*DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000); */
		/*OVLReset(); */
		WDMAReset(1);
		/*RDMAReset(0); */

		/* dump_all_info(); */

		if (disp_ovl_engine.bCouple) {
#ifdef DISP_OVL_ENGINE_REQUEST
			{
				struct disp_ovl_engine_request_struct overlayRequest;

				overlayRequest.request = DISP_OVL_REQUEST_FORCE_DISABLE_CABC;
				overlayRequest.value = 1;

				Disp_Ovl_Engine_Set_Request(&overlayRequest, 1000);
			}
#endif

			/* disconnet the overlay -- rdma direct link */
			disp_ovl_engine_indirect_link_overlay();
			disp_ovl_engine.bCouple = FALSE;
		} else {
			static int first = 1;
			/* link the overlay -- rdma */
			/* if(first) */
			/* { */
			/* first = 0; */
			/* memcpy(&disp_ovl_engine_params, &disp_directlink_instance, sizeof(DISP_OVL_ENGINE_INSTANCE)); */
			if (first) {
				first = 0;
			} else {
			DISP_REG_SET(DISP_REG_BLS_RST, 0x1);
			DISP_REG_SET(DISP_REG_BLS_RST, 0x0);
			DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);
			OVLReset();
			RDMAReset(0);
			}

			disp_ovl_engine_direct_link_overlay();
			disp_ovl_engine.bCouple = TRUE;
			/* } */

#ifdef DISP_OVL_ENGINE_REQUEST
			{
				static int first = 1;
				struct disp_ovl_engine_request_struct overlayRequest;

				if (first) {
					/* First time, ovlfd does not exist. */
					first = 0;
				} else {

					overlayRequest.request =
					    DISP_OVL_REQUEST_FORCE_DISABLE_CABC;
					overlayRequest.value = 0;

					Disp_Ovl_Engine_Set_Request(&overlayRequest, 1000);


				}
			}
#endif
		}
		disp_ovl_engine.bModeSwitch = FALSE;
	}



	if (disp_ovl_engine.bCouple) {	/* only for couple instance */
		int dirty;
		int layer_id;
		int OvlSecureNew = 0;

		disp_path_config_layer_ovl_engine_control(true);

		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
			    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
			     LAYER_SECURE_BUFFER))
				OvlSecureNew = 1;
		}

		if (OvlSecure != OvlSecureNew) {
			if (OvlSecureNew) {
				OvlSecure = OvlSecureNew;
				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			}
		}
		/* update overlay layer config */
		DISP_OVL_ENGINE_DBG(" couple mode\n");
		if (atomic_read(&disp_ovl_engine_params.OverlaySettingDirtyFlag)) {
			DISP_OVL_ENGINE_DBG("process couple instance\n");
			disp_path_get_mutex();
			for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
				if (disp_ovl_engine_params.cached_layer_config[i].isDirty) {
					dirty |= 1 << i;
					disp_path_config_layer_ovl_engine_ex
					    (&disp_ovl_engine_params.cached_layer_config[i],
						OvlSecure,
						true);
					disp_ovl_engine_params.cached_layer_config[i].isDirty =
					    false;
				}
			}
			disp_path_release_mutex();
			atomic_set(&disp_ovl_engine_params.OverlaySettingDirtyFlag, 0);
		}

		if (OvlSecure != OvlSecureNew) {
			if (!OvlSecureNew) {
				OvlSecure = OvlSecureNew;
				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			}
		}

		/* capture screen */
		if (disp_ovl_engine_params.MemOutConfig.dirty) {
			disp_ovl_engine_set_overlay_to_buffer();
		}
		DISP_OVL_ENGINE_DBG(" couple mode finish\n\n");
#if 0
		disp_dump_reg(DISP_MODULE_OVL);
		disp_dump_reg(DISP_MODULE_RDMA1);
		disp_dump_reg(DISP_MODULE_MUTEX);
		disp_dump_reg(DISP_MODULE_CONFIG);
#endif

	} else {		/* decouple mode */

		/* set overlay output to buffer */

		DISP_OVL_ENGINE_DBG(" decouple mode\n");
		if (COUPLE_MODE == disp_ovl_engine_params.mode) {	/* couple instance */
			DISP_OVL_ENGINE_DBG(" couple instance\n");
			disp_ovl_engine_trigger_hw_overlay_couple();
		} else {	/* de-couple instance */

			DISP_OVL_ENGINE_DBG(" decouple instance\n");
			disp_ovl_engine_trigger_hw_overlay_decouple();
		}
	}

}

void disp_ovl_engine_hw_register_irq(void (*irq_callback) (unsigned int param))
{
	disp_ovl_engine_hw_irq_callback = irq_callback;
}


int disp_ovl_engine_hw_mva_map(struct disp_mva_map *mva_map_struct)
{
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;

	param[0].value.a = mva_map_struct->module;
	param[1].value.a = mva_map_struct->cache_coherent;
	param[2].value.a = mva_map_struct->addr;
	param[3].value.a = mva_map_struct->size;
	paramTypes =
	    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret =
	    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_SECURE_MVA_MAP, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_MAP) fail, ret=%d\n",
				    ret);

		return -1;
	}
#endif
	return 0;
}



int disp_ovl_engine_hw_mva_unmap(struct disp_mva_map *mva_map_struct)
{
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;

	param[0].value.a = mva_map_struct->module;
	param[1].value.a = mva_map_struct->cache_coherent;
	param[2].value.a = mva_map_struct->addr;
	param[3].value.a = mva_map_struct->size;
	paramTypes =
	    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret =
	    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_SECURE_MVA_UNMAP, paramTypes,
				param);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR
		    ("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_UNMAP) fail, ret=%d\n", ret);

		return -1;
	}
#endif
	return 0;
}

int disp_ovl_engine_hw_reset(void)
{
	if (disp_ovl_engine.bCouple) {
		OVLReset();
		RDMAReset(0);
	} else {
		OVLReset();
		WDMAReset(1);
	}
	return OVL_OK;
}

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
static KREE_SESSION_HANDLE disp_ovl_engine_secure_memory_session;
KREE_SESSION_HANDLE disp_ovl_engine_secure_memory_session_handle(void)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_secure_memory_session_handle() acquire TEE session\n");
	/* TODO: the race condition here is not taken into consideration. */
	if (NULL == disp_ovl_engine_secure_memory_session) {
		TZ_RESULT ret;
		DISP_OVL_ENGINE_DBG
		    ("disp_ovl_engine_secure_memory_session_handle() create session\n");
		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &disp_ovl_engine_secure_memory_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_OVL_ENGINE_ERR("KREE_CreateSession fail, ret=%d\n", ret);
			return NULL;
		}
	}

	DISP_OVL_ENGINE_DBG("disp_ovl_engine_secure_memory_session_handle() session=%x\n",
			    (unsigned int)disp_ovl_engine_secure_memory_session);
	return disp_ovl_engine_secure_memory_session;
}


void *disp_ovl_engine_hw_allocate_secure_memory(int size)
{
	KREE_SECUREMEM_HANDLE mem_handle;
	TZ_RESULT ret;
	struct disp_mva_map mva_map_struct;

	/* Allocate */
	ret = KREE_AllocSecurechunkmem(disp_ovl_engine_secure_memory_session_handle(),
				       &mem_handle, 0, size);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_AllocSecurechunkmem fail, ret=%d\n", ret);
		return NULL;
	}

	DISP_OVL_ENGINE_DBG("KREE_AllocSecurechunkmem handle=0x%x\n", mem_handle);

	/* Map mva */
	mva_map_struct.addr = (unsigned int)mem_handle;
	mva_map_struct.size = size;
	mva_map_struct.cache_coherent = 0;
	mva_map_struct.module = M4U_CLNTMOD_RDMA;
	disp_ovl_engine_hw_mva_map(&mva_map_struct);


	return (void *)mem_handle;
}


void disp_ovl_engine_hw_free_secure_memory(void *mem_handle)
{
	TZ_RESULT ret;
	struct disp_mva_map mva_map_struct;

	/* Unmap mva */
	mva_map_struct.addr = (unsigned int)mem_handle;
	mva_map_struct.size = 0;
	mva_map_struct.cache_coherent = 0;
	mva_map_struct.module = M4U_CLNTMOD_RDMA;
	disp_ovl_engine_hw_mva_unmap(&mva_map_struct);

	/* Free */
	ret = KREE_UnreferenceSecurechunkmem(disp_ovl_engine_secure_memory_session_handle(),
					     (KREE_SECUREMEM_HANDLE) mem_handle);

	DISP_OVL_ENGINE_DBG("KREE_UnreferenceSecurechunkmem handle=0x%0x\n",
			    (unsigned int)mem_handle);

	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_UnreferenceSecurechunkmem fail, ret=%d\n", ret);
	}

	return;
}
#endif

#endif
