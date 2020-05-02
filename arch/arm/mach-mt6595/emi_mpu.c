#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <linux/aee.h>
#endif
#include <linux/timer.h>
#include <linux/workqueue.h>

#include <mach/mt_reg_base.h>
#include <mach/emi_mpu.h>
#include <mach/mt_device_apc.h>
#include <mach/sync_write.h>
#include <mach/irqs.h>
#include <mach/dma.h>
/* #include <mach/mtk_ccci_helper.h> */

#define ENABLE_EMI_CHKER

#define NR_REGION_ABORT 8
#define MAX_EMI_MPU_STORE_CMD_LEN 128
/* #define ABORT_EMI_BUS_INTERFACE 0x00200000 //DEVAPC0_D0_VIO_STA_0, idx:21 */
/* #define ABORT_EMI               0x00000001 //DEVAPC0_D0_VIO_STA_3, idx:0 */
#define TIMEOUT 100
#define AXI_VIO_MONITOR_TIME    (1 * HZ)

static struct work_struct emi_mpu_work;
static struct workqueue_struct *emi_mpu_workqueue;

static unsigned int vio_addr;

struct mst_tbl_entry {
	u32 master;
	u32 port;
	u32 id_mask;
	u32 id_val;
	char *name;
};

struct emi_mpu_notifier_block {
	struct list_head list;
	emi_mpu_notifier notifier;
};

static const struct mst_tbl_entry mst_tbl[] = {
	/* apmcu */
	{.master = MST_ID_APMCU_0, .port = 0x0, .id_mask = 0xE, .id_val = 0x8, .name = "APMCU: CA7"},
	{.master = MST_ID_APMCU_1, .port = 0x0, .id_mask = 0xE, .id_val = 0x6, .name = "APMCU: CA15"},

	{.master = MST_ID_APMCU_2, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x4, .name = "MSDC0"},
	{.master = MST_ID_APMCU_3, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x84, .name = "NFI"},
	{.master = MST_ID_APMCU_4, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x284, .name = "Audio"},
	{.master = MST_ID_APMCU_5, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x484, .name = "MSDC3"},
	{.master = MST_ID_APMCU_6, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x684, .name = "USB20"},
	{.master = MST_ID_APMCU_7, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x104, .name = "MD"},
	{.master = MST_ID_APMCU_8, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x304, .name = "SPM"},
	{.master = MST_ID_APMCU_9, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x504, .name = "MD32"},
	{.master = MST_ID_APMCU_10, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x704, .name = "THERM"},
	{.master = MST_ID_APMCU_11, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x184, .name = "DMA"},
	{.master = MST_ID_APMCU_12, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x14, .name = "PWM"},
	{.master = MST_ID_APMCU_13, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x114, .name = "MSDC1"},
	{.master = MST_ID_APMCU_14, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x214, .name = "MSCD2"},
	{.master = MST_ID_APMCU_15, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x314, .name = "SPIO"},
	{.master = MST_ID_APMCU_16, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x94, .name = "USB30"},
	{.master = MST_ID_APMCU_17, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x24, .name = "GCPU"},
	{.master = MST_ID_APMCU_18, .port = 0x0, .id_mask = 0x1F7E, .id_val = 0x34, .name = "CQ_DMA"},
	{.master = MST_ID_APMCU_19, .port = 0x0, .id_mask = 0x1F7E, .id_val = 0x44, .name = "DebugTop"},
	{.master = MST_ID_APMCU_20, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x7C4, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_APMCU_21, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x7D4, .name =
	 "Perisys IOMMU"},

	{.master = MST_ID_APMCU_22, .port = 0x0, .id_mask = 0x1F0E, .id_val = 0x2, .name = "GPU"},

	/* MM */
	{.master = MST_ID_MM_0, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x0, .name = "Larb0"},
	{.master = MST_ID_MM_1, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x80, .name = "Larb1"},
	{.master = MST_ID_MM_2, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x100, .name = "Larb2"},
	{.master = MST_ID_MM_3, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x180, .name = "Larb3"},
	{.master = MST_ID_MM_4, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x200, .name = "Larb4"},
	{.master = MST_ID_MM_5, .port = 0x2, .id_mask = 0x1FFE, .id_val = 0x3FC, .name = "M4U"},

	/* Modem */
	{.master = MST_ID_MDMCU_0, .port = 0x3, .id_mask = 0x0, .id_val = 0x0, .name = "MDMCU"},

	/* Modem HW (2G/3G) */
	{.master = MST_ID_MDHW_0, .port = 0x4, .id_mask = 0x0, .id_val = 0x0, .name = "MDHW"},

	/* Periperal */
	{.master = MST_ID_PERI_0, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x1, .name = "MSDC0"},
	{.master = MST_ID_PERI_1, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x11, .name = "NFI"},
	{.master = MST_ID_PERI_2, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x51, .name = "Audio"},
	{.master = MST_ID_PERI_3, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x91, .name = "MSDC3"},
	{.master = MST_ID_PERI_4, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xD1, .name = "USB20"},
	{.master = MST_ID_PERI_5, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x21, .name = "MD"},
	{.master = MST_ID_PERI_6, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x61, .name = "SPM"},
	{.master = MST_ID_PERI_7, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xA1, .name = "MD32"},
	{.master = MST_ID_PERI_8, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xE1, .name = "THERM"},
	{.master = MST_ID_PERI_9, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x31, .name = "DMA"},
	{.master = MST_ID_PERI_10, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x3, .name = "PWM"},
	{.master = MST_ID_PERI_11, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x23, .name = "MSDC1"},
	{.master = MST_ID_PERI_12, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x43, .name = "MSCD2"},
	{.master = MST_ID_PERI_13, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x63, .name = "SPIO"},
	{.master = MST_ID_PERI_14, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x13, .name = "USB30"},
	{.master = MST_ID_PERI_15, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x5, .name = "GCPU"},
	{.master = MST_ID_PERI_16, .port = 0x6, .id_mask = 0x1FEF, .id_val = 0x7, .name = "CQ_DMA"},
	{.master = MST_ID_PERI_17, .port = 0x6, .id_mask = 0x1FEF, .id_val = 0x9, .name = "DebugTop"},
	{.master = MST_ID_PERI_18, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xF9, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_PERI_19, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xFB, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_PERI_20, .port = 0x6, .id_mask = 0x1F01, .id_val = 0x0, .name =
	 "emi_m6 : MFG"},

	/* GPU */
	{.master = MST_ID_GPU_0, .port = 0x7, .id_mask = 0x1F80, .id_val = 0x0, .name = "emi_m7 : MFG"},
	/* apmcu */
	{.master = MST_ID_APMCU_0, .port = 0x0, .id_mask = 0xE, .id_val = 0x8, .name = "APMCU: CA7"},
	{.master = MST_ID_APMCU_1, .port = 0x0, .id_mask = 0xE, .id_val = 0x6, .name = "APMCU: CA15"},

	{.master = MST_ID_APMCU_2, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x4, .name = "MSDC0"},
	{.master = MST_ID_APMCU_3, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x84, .name = "NFI"},
	{.master = MST_ID_APMCU_4, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x284, .name = "Audio"},
	{.master = MST_ID_APMCU_5, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x484, .name = "MSDC3"},
	{.master = MST_ID_APMCU_6, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x684, .name = "USB20"},
	{.master = MST_ID_APMCU_7, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x104, .name = "MD"},
	{.master = MST_ID_APMCU_8, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x304, .name = "SPM"},
	{.master = MST_ID_APMCU_9, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x504, .name = "MD32"},
	{.master = MST_ID_APMCU_10, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x704, .name = "THERM"},
	{.master = MST_ID_APMCU_11, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x184, .name = "DMA"},
	{.master = MST_ID_APMCU_12, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x14, .name = "PWM"},
	{.master = MST_ID_APMCU_13, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x114, .name = "MSDC1"},
	{.master = MST_ID_APMCU_14, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x214, .name = "MSCD2"},
	{.master = MST_ID_APMCU_15, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x314, .name = "SPIO"},
	{.master = MST_ID_APMCU_16, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x94, .name = "USB30"},
	{.master = MST_ID_APMCU_17, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x24, .name = "GCPU"},
	{.master = MST_ID_APMCU_18, .port = 0x0, .id_mask = 0x1F7E, .id_val = 0x34, .name = "CQ_DMA"},
	{.master = MST_ID_APMCU_19, .port = 0x0, .id_mask = 0x1F7E, .id_val = 0x44, .name = "DebugTop"},
	{.master = MST_ID_APMCU_20, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x7C4, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_APMCU_21, .port = 0x0, .id_mask = 0x1FFE, .id_val = 0x7D4, .name =
	 "Perisys IOMMU"},

	{.master = MST_ID_APMCU_22, .port = 0x0, .id_mask = 0x1F0E, .id_val = 0x2, .name = "GPU"},

	/* MM */
	{.master = MST_ID_MM_0, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x0, .name = "Larb0"},
	{.master = MST_ID_MM_1, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x80, .name = "Larb1"},
	{.master = MST_ID_MM_2, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x100, .name = "Larb2"},
	{.master = MST_ID_MM_3, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x180, .name = "Larb3"},
	{.master = MST_ID_MM_4, .port = 0x2, .id_mask = 0x1F80, .id_val = 0x200, .name = "Larb4"},
	{.master = MST_ID_MM_5, .port = 0x2, .id_mask = 0x1FFE, .id_val = 0x3FC, .name = "M4U"},

	/* Modem */
	{.master = MST_ID_MDMCU_0, .port = 0x3, .id_mask = 0x0, .id_val = 0x0, .name = "MDMCU"},

	/* Modem HW (2G/3G) */
	{.master = MST_ID_MDHW_0, .port = 0x4, .id_mask = 0x0, .id_val = 0x0, .name = "MDHW"},

	/* Periperal */
	{.master = MST_ID_PERI_0, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x1, .name = "MSDC0"},
	{.master = MST_ID_PERI_1, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x11, .name = "NFI"},
	{.master = MST_ID_PERI_2, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x51, .name = "Audio"},
	{.master = MST_ID_PERI_3, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x91, .name = "MSDC3"},
	{.master = MST_ID_PERI_4, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xD1, .name = "USB20"},
	{.master = MST_ID_PERI_5, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x21, .name = "MD"},
	{.master = MST_ID_PERI_6, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x61, .name = "SPM"},
	{.master = MST_ID_PERI_7, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xA1, .name = "MD32"},
	{.master = MST_ID_PERI_8, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xE1, .name = "THERM"},
	{.master = MST_ID_PERI_9, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x31, .name = "DMA"},
	{.master = MST_ID_PERI_10, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x3, .name = "PWM"},
	{.master = MST_ID_PERI_11, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x23, .name = "MSDC1"},
	{.master = MST_ID_PERI_12, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x43, .name = "MSCD2"},
	{.master = MST_ID_PERI_13, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x63, .name = "SPIO"},
	{.master = MST_ID_PERI_14, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x13, .name = "USB30"},
	{.master = MST_ID_PERI_15, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0x5, .name = "GCPU"},
	{.master = MST_ID_PERI_16, .port = 0x6, .id_mask = 0x1FEF, .id_val = 0x7, .name = "CQ_DMA"},
	{.master = MST_ID_PERI_17, .port = 0x6, .id_mask = 0x1FEF, .id_val = 0x9, .name = "DebugTop"},
	{.master = MST_ID_PERI_18, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xF9, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_PERI_19, .port = 0x6, .id_mask = 0x1FFF, .id_val = 0xFB, .name =
	 "Perisys IOMMU"},
	{.master = MST_ID_PERI_20, .port = 0x6, .id_mask = 0x1F01, .id_val = 0x0, .name =
	 "emi_m6 : MFG"},

	/* GPU */
	{.master = MST_ID_GPU_0, .port = 0x7, .id_mask = 0x1F80, .id_val = 0x0, .name = "emi_m7 : MFG"},

	/* GPU */
	{.master = MST_ID_GPU_0, .port = 0x7, .id_mask = 0x1F80, .id_val = 0x0, .name = "emi_m7 : MFG"},

};

/* struct list_head emi_mpu_notifier_list[NR_MST]; */
static const char *UNKNOWN_MASTER = "unknown";
static spinlock_t emi_mpu_lock;

#ifdef ENABLE_EMI_CHKER
struct timer_list emi_axi_vio_timer;
#endif

char *smi_larb0_port[14] =
    { "disp_ovl_0", "disp_rdma_0", "disp_rdma_1", "disp_wdma_0", "disp_ovl_1", "disp_rdma_2",
	"disp_wdma_1", "disp_od_r", "disp_od_w", "mdp_rdma_0", "mdp_rdma_1", "mdp_wdma",
	    "mdp_wrot_0", "mdp_wrot_1"
};

char *smi_larb1_port[9] =
    { "hw_vdec_mc_ext", "hw_vdec_pp_ext", "hw_vdec_ufo_ext", "hw_vdec_vid_ext",
	"hw_vdec_vid2_ext", "hw_vdec_avc_ext", "hw_vdec_pred_rd_ext", "hw_vdec_pred_wr_ext",
	    "hw_vdec_ppwarp_ext"
};

char *smi_larb2_port[21] =
    { "cam_imgo", "cam_rrzo", "cam_aao", "cam_icso", "cam_esfko", "cam_imgo_d", "cam_isci",
	"cam_isci_d", "cam_bpci", "cam_bpci_d", "cam_ufdi", "cam_imgi", "cam_img2o", "cam_img3o",
	    "cam_vipi",
	"cam_vip2i", "cam_vip3i", "cam_icei", "cam_rb", "cam_rp", "cam_wr"
};

char *smi_larb3_port[19] =
    { "venc_rcpu", "venc_rec", "venc_bsdma", "venc_sv_comv", "vend_rd_comv", "jpgenc_bsdma",
	"remdc_sdma", "remdc_bsdma", "jpgenc_rdma", "jpgenc_sdma", "jpgenc_wdma", "jpgdec_bsdma",
	    "venc_cur_luma",
	"venc_cur_chroma", "venc_ref_luma", "vend_ref_chroma", "redmc_wdma", "venc_nbm_rdma",
	    "venc_nbm_wdma"
};

char *smi_larb4_port[4] = { "mjc_mv_rd", "mjc_mv_wr", "mjc_dma_rd", "mjc_dma_wr" };

static int __match_id(u32 axi_id, int tbl_idx, u32 port_ID)
{
	u32 mm_larb;
	u32 smi_port;

	if (((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val)
	    && (port_ID == mst_tbl[tbl_idx].port)) {
		switch (port_ID) {
		case 0:	/* ARM */
		case 3:	/* MD */
		case 4:	/* MD HW (2G/3G) */
		case 6:	/* Peripheral */
		case 7:	/* GPU */
			pr_crit(KERN_CRIT "Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			break;
		case 2:	/* MM */
			mm_larb = axi_id >> 7;
			smi_port = (axi_id & 0x7F) >> 2;
			if (mm_larb == 0x0) {
				if (smi_port >= ARRAY_SIZE(smi_larb0_port)) {
					pr_crit(KERN_CRIT
						"[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit(KERN_CRIT "Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb0_port[smi_port]);
			} else if (mm_larb == 0x1) {
				if (smi_port >= ARRAY_SIZE(smi_larb1_port)) {
					pr_crit(KERN_CRIT
						"[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit(KERN_CRIT "Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb1_port[smi_port]);
			} else if (mm_larb == 0x2) {
				if (smi_port >= ARRAY_SIZE(smi_larb2_port)) {
					pr_crit(KERN_CRIT
						"[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit(KERN_CRIT "Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb2_port[smi_port]);
			} else if (mm_larb == 0x3) {
				if (smi_port >= ARRAY_SIZE(smi_larb3_port)) {
					pr_crit(KERN_CRIT
						"[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit(KERN_CRIT "Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb3_port[smi_port]);
			} else if (mm_larb == 0x4) {
				if (smi_port >= ARRAY_SIZE(smi_larb4_port)) {
					pr_crit(KERN_CRIT
						"[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_crit(KERN_CRIT "Violation master name is %s (%s).\n",
					mst_tbl[tbl_idx].name, smi_larb4_port[smi_port]);
			} else {	/*M4U */

				pr_crit(KERN_CRIT "Violation master name is %s.\n",
					mst_tbl[tbl_idx].name);
			}
			break;
		default:
			pr_crit(KERN_CRIT
				"[EMI MPU ERROR] Invalidate port ID! lookup bus ID table failed!\n");
			break;
		}
		return 1;
	} else {
		return 0;
	}
}

static u32 __id2mst(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x000001FFF;
	port_ID = id & 0x00000007;

	pr_crit("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID);

	/* Both M0 & M1 are APMCU */
	if (port_ID == 1)
		port_ID = 0;

	/* Both M2 & M5 are MM */
	if (port_ID == 5)
		port_ID = 2;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].master;
	}
	return MST_INVALID;
}

static char *__id2name(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x00001FFF;
	port_ID = id & 0x00000007;

	pr_crit("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID);

	/* Both M0 & M1 are APMCU */
	if (port_ID == 1)
		port_ID = 0;

	/* Both M2 & M5 are MM */
	if (port_ID == 5)
		port_ID = 2;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}

static void __clear_emi_mpu_vio(void)
{
	u32 dbg_s, dbg_t;

	/* clear violation status */
	mt65xx_reg_sync_writel(0x000003FF, EMI_MPUP);
	mt65xx_reg_sync_writel(0x000003FF, EMI_MPUQ);
	mt65xx_reg_sync_writel(0x000003FF, EMI_MPUR);
	mt65xx_reg_sync_writel(0x000003FF, EMI_MPUY);

	/* clear debug info */
	mt65xx_reg_sync_writel(0x80000000, EMI_MPUS);
	dbg_s = readl(IOMEM(EMI_MPUS));
	dbg_t = readl(IOMEM(EMI_MPUT));

	/* MT6582 EMI hw bug that EMI_MPUS[10:0] and EMI_MPUT can't be cleared */
	dbg_s &= 0xFFFF0000;
	if (dbg_s) {
		pr_crit(KERN_CRIT "Fail to clear EMI MPU violation\n");
		pr_crit(KERN_CRIT "EMI_MPUS = %x, EMI_MPUT = %x", dbg_s, dbg_t);
	}
}

/*EMI MPU violation handler*/
static irqreturn_t mpu_violation_irq(int irq, void *dev_id)
{
	u32 dbg_s, dbg_t, dbg_pqry;
	u32 master_ID, domain_ID, wr_vio;
	s32 region;
	int i, res;
	char *master_name;

	/* Need DEVAPC owner porting */
	res = mt_devapc_check_emi_violation();
	if (res)
		return IRQ_NONE;

	pr_crit(KERN_INFO "It's a MPU violation.\n");

	dbg_s = readl(IOMEM(EMI_MPUS));
	dbg_t = readl(IOMEM(EMI_MPUT));

	pr_crit(KERN_ALERT "Clear status.\n");

	master_ID = (dbg_s & 0x00003FFF) | ((dbg_s & 0x0C000000) >> 12);
	domain_ID = (dbg_s >> 14) & 0x00000003;
	wr_vio = (dbg_s >> 28) & 0x00000003;
	region = (dbg_s >> 16) & 0xFF;

	for (i = 0; i < NR_REGION_ABORT; i++) {
		if ((region >> i) & 1)
			break;
	}
	region = (i >= NR_REGION_ABORT) ? -1 : i;

	switch (domain_ID) {
	case 0:
		dbg_pqry = readl(IOMEM(EMI_MPUP));
		break;
	case 1:
		dbg_pqry = readl(IOMEM(EMI_MPUQ));
		break;
	case 2:
		dbg_pqry = readl(IOMEM(EMI_MPUR));
		break;
	case 3:
		dbg_pqry = readl(IOMEM(EMI_MPUY));
		break;
	default:
		dbg_pqry = 0;
		break;
	}

	/*TBD: print the abort region */

	pr_crit(KERN_CRIT "EMI MPU violation.\n");
	pr_crit(KERN_CRIT "[EMI MPU] Debug info start ----------------------------------------\n");

	pr_crit(KERN_CRIT "EMI_MPUS = %x, EMI_MPUT = %x.\n", dbg_s, dbg_t);
	pr_crit(KERN_CRIT "Current process is \"%s \" (pid: %i).\n", current->comm, current->pid);
	pr_crit(KERN_CRIT "Violation address is 0x%x.\n", dbg_t + EMI_PHY_OFFSET);
	pr_crit(KERN_CRIT "Violation master ID is 0x%x.\n", master_ID);
	/*print out the murderer name */
	master_name = __id2name(master_ID);
	pr_crit(KERN_CRIT "Violation domain ID is 0x%x.\n", domain_ID);
	pr_crit(KERN_CRIT "%s violation.\n", (wr_vio == 1) ? "Write" : "Read");
	pr_crit(KERN_CRIT "Corrupted region is %d\n\r", region);
	if (dbg_pqry & OOR_VIO)
		pr_crit(KERN_CRIT "Out of range violation.\n");

	pr_crit(KERN_CRIT "[EMI MPU] Debug info end------------------------------------------\n");

#if 0
	/* For MDHW debug usage, 0x6C -> MDHW, Master is 3G */
	if (dbg_s & 0x6C)
		exec_ccci_kern_func_by_md_id(0, ID_FORCE_MD_ASSERT, NULL, 0);
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
	/*FIXME: skip ca17-read violation to trigger root-cause KE. Remove it before E2 QC */
	if ((0 != dbg_s) && (master_ID != 0x831)) {
		aee_kernel_exception("EMI MPU",
				     "EMI MPU violation.\nEMI_MPUS = 0x%x, EMI_MPUT = 0x%x, module is %s.\n",
				     dbg_s, dbg_t + EMI_PHY_OFFSET, master_name);
	}
#endif

	__clear_emi_mpu_vio();

	mt_devapc_clear_emi_violation();

	pr_crit("[EMI MPU] _id2mst = %d\n", __id2mst(master_ID));

#if 0
	/* Marcos(MT6582): Each hw module has an unique ID.
	   There is no need to use notifier function to distinguish different hw module which has the same bus ID. */
	list_for_each(p, &(emi_mpu_notifier_list[__id2mst(master_ID)])) {
		block = list_entry(p, struct emi_mpu_notifier_block, list);
		block->notifier(dbg_t + EMI_PHY_OFFSET, wr_vio);
	}
#endif

	vio_addr = dbg_t + EMI_PHY_OFFSET;

	return IRQ_HANDLED;
}


/* Acquire DRAM Setting for PASR/DPD */
void acquire_dram_setting(struct basic_dram_setting *pasrdpd)
{
	int ch_nr = MAX_CHANNELS;
	unsigned int emi_cona, col_bit, row_bit;

	pasrdpd->channel_nr = ch_nr;

	emi_cona = readl(IOMEM(EMI_CONA));

	/* channel 0 */
	{
		/* rank 0 */
		col_bit = ((emi_cona >> 4) & 0x03) + 9;
		row_bit = ((emi_cona >> 12) & 0x03) + 13;
		pasrdpd->channel[0].rank[0].valid_rank = true;
		/* 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[0].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		pasrdpd->channel[0].rank[0].segment_nr = 8;
		if (0 != (emi_cona & (1 << 17))) {	/* rank 1 exist */
			col_bit = ((emi_cona >> 6) & 0x03) + 9;
			row_bit = ((emi_cona >> 14) & 0x03) + 13;
			pasrdpd->channel[0].rank[1].valid_rank = true;
			/* 32 bits * 8 banks, unit Gb */
			pasrdpd->channel[0].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			pasrdpd->channel[0].rank[1].segment_nr = 8;
		} else {
			pasrdpd->channel[0].rank[1].valid_rank = false;
			pasrdpd->channel[0].rank[1].rank_size = 0;
			pasrdpd->channel[0].rank[1].segment_nr = 0;
		}
	}

	if (0 != (emi_cona & 0x01)) {	/* channel 1 exist */
		/* rank0 setting */
		col_bit = ((emi_cona >> 20) & 0x03) + 9;
		row_bit = ((emi_cona >> 28) & 0x03) + 13;
		pasrdpd->channel[1].rank[0].valid_rank = true;
		/* 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[1].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		pasrdpd->channel[1].rank[0].segment_nr = 8;

		if (0 != (emi_cona & (1 << 16))) {	/* rank 1 exist */
			col_bit = ((emi_cona >> 22) & 0x03) + 9;
			row_bit = ((emi_cona >> 30) & 0x03) + 13;
			pasrdpd->channel[1].rank[1].valid_rank = true;
			/* 32 bits * 8 banks, unit Gb */
			pasrdpd->channel[1].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			pasrdpd->channel[1].rank[1].segment_nr = 8;
		} else {
			pasrdpd->channel[1].rank[1].valid_rank = false;
			pasrdpd->channel[1].rank[1].rank_size = 0;
			pasrdpd->channel[1].rank[1].segment_nr = 0;
		}
	} else {		/* channel 2 does not exist */
		pasrdpd->channel[1].rank[0].valid_rank = false;
		pasrdpd->channel[1].rank[0].rank_size = 0;
		pasrdpd->channel[1].rank[0].segment_nr = 0;

		pasrdpd->channel[1].rank[1].valid_rank = false;
		pasrdpd->channel[1].rank[1].rank_size = 0;
		pasrdpd->channel[1].rank[1].segment_nr = 0;
	}

	return;
}

/*
 * emi_mpu_set_region_protection: protect a region.
 * @start: start address of the region
 * @end: end address of the region
 * @region: EMI MPU region id
 * @access_permission: EMI MPU access permission
 * Return 0 for success, otherwise negative status code.
 */
int emi_mpu_set_region_protection(unsigned int start, unsigned int end, int region,
				  unsigned int access_permission)
{
	int ret = 0;
	unsigned int tmp;
	unsigned long flags;

	if ((end != 0) || (start != 0)) {
		/*Address 64KB alignment */
		start -= EMI_PHY_OFFSET;
		end -= EMI_PHY_OFFSET;
		start = start >> 16;
		end = end >> 16;

		if (end <= start)
			return -EINVAL;
	}

	spin_lock_irqsave(&emi_mpu_lock, flags);

	switch (region) {
	case 0:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUI)) & 0xFFFF0000;
		mt65xx_reg_sync_writel(0, EMI_MPUI);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUA);
		mt65xx_reg_sync_writel(tmp | access_permission, EMI_MPUI);
		break;

	case 1:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUI)) & 0x0000FFFF;
		mt65xx_reg_sync_writel(0, EMI_MPUI);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUB);
		mt65xx_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUI);
		break;

	case 2:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUJ)) & 0xFFFF0000;
		mt65xx_reg_sync_writel(0, EMI_MPUJ);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUC);
		mt65xx_reg_sync_writel(tmp | access_permission, EMI_MPUJ);
		break;

	case 3:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUJ)) & 0x0000FFFF;
		mt65xx_reg_sync_writel(0, EMI_MPUJ);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUD);
		mt65xx_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUJ);
		break;

	case 4:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUK)) & 0xFFFF0000;
		mt65xx_reg_sync_writel(0, EMI_MPUK);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUE);
		mt65xx_reg_sync_writel(tmp | access_permission, EMI_MPUK);
		break;

	case 5:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUK)) & 0x0000FFFF;
		mt65xx_reg_sync_writel(0, EMI_MPUK);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUF);
		mt65xx_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUK);
		break;

	case 6:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUL)) & 0xFFFF0000;
		mt65xx_reg_sync_writel(0, EMI_MPUL);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUG);
		mt65xx_reg_sync_writel(tmp | access_permission, EMI_MPUL);
		break;

	case 7:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUL)) & 0x0000FFFF;
		mt65xx_reg_sync_writel(0, EMI_MPUL);
		mt65xx_reg_sync_writel((start << 16) | end, EMI_MPUH);
		mt65xx_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUL);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&emi_mpu_lock, flags);

	return ret;
}
EXPORT_SYMBOL(emi_mpu_set_region_protection);

/*
 * emi_mpu_notifier_register: register a notifier.
 * master: MST_ID_xxx
 * notifier: the callback function
 * Return 0 for success, otherwise negative error code.
 */
#if 0
int emi_mpu_notifier_register(int master, emi_mpu_notifier notifier)
{
	struct emi_mpu_notifier_block *block;
	static int emi_mpu_notifier_init;
	int i;

	if (master >= MST_INVALID)
		return -EINVAL;

	block = kmalloc(sizeof(struct emi_mpu_notifier_block), GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	if (!emi_mpu_notifier_init) {
		for (i = 0; i < NR_MST; i++)
			INIT_LIST_HEAD(&(emi_mpu_notifier_list[i]));

		emi_mpu_notifier_init = 1;
	}

	block->notifier = notifier;
	list_add(&(block->list), &(emi_mpu_notifier_list[master]));

	return 0;
}
#endif

static ssize_t emi_mpu_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	unsigned int start, end;
	unsigned int reg_value;
	unsigned int d0, d1, d2, d3;
	static const char *permission[6] = {
		"No protect",
		"Only R/W for secure access",
		"Only R/W for secure access, and non-secure read access",
		"Only R/W for secure access, and non-secure write access",
		"Only R for secure/non-secure",
		"Both R/W are forbidden" "Only secure W is forbidden"
	};

	reg_value = readl(IOMEM(EMI_MPUA));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 0 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUB));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 1 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUC));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 2 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUD));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 3 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUE));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 4 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUF));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 5 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUG));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 6 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUH));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 7 --> 0x%x to 0x%x\n", start, end);

	ptr += sprintf(ptr, "\n");

	reg_value = readl(IOMEM(EMI_MPUI));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 0 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 1 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUJ));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 2 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 3 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUK));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 4 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 5 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUL));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 6 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 7 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	return strlen(buf);
}

static ssize_t emi_mpu_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int region;
	unsigned int access_permission;
	char *command;
	char *ptr;
	char *token[5];

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_crit(KERN_CRIT "emi_mpu_store command overflow.");
		return count;
	}
	pr_crit(KERN_CRIT "emi_mpu_store: %s\n", buf);

	command = kmalloc((size_t) MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;

	strcpy(command, buf);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_MPU_STR, strlen(EN_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_crit(KERN_DEBUG "token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++)
			pr_crit(KERN_DEBUG "token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], &token[1], 16);
		end_addr = kstrtoul(token[2], &token[2], 16);
		region = kstrtoul(token[3], &token[3], 16);
		access_permission = kstrtoul(token[4], &token[4], 16);
		emi_mpu_set_region_protection(start_addr, end_addr, region, access_permission);
		pr_crit(KERN_CRIT
			"Set EMI_MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x.\n",
			start_addr, end_addr, region, access_permission);
	} else if (!strncmp(buf, DIS_MPU_STR, strlen(DIS_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_crit(KERN_DEBUG "token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++)
			pr_crit(KERN_DEBUG "token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], &token[1], 16);
		end_addr = kstrtoul(token[2], &token[2], 16);
		region = kstrtoul(token[3], &token[3], 16);
		emi_mpu_set_region_protection(0x0, 0x0, region,
					      SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION,
								   NO_PROTECTION, NO_PROTECTION));
		pr_crit("set EMI MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x\n", 0, 0,
			region, SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
						     NO_PROTECTION));
	} else {
		pr_crit(KERN_CRIT "Unknown emi_mpu command.\n");
	}

	kfree(command);

	return count;
}

DRIVER_ATTR(mpu_config, 0644, emi_mpu_show, emi_mpu_store);

void mtk_search_full_pgtab(void)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long addr;
	unsigned long addr_2nd, addr_2nd_end;
	unsigned int v_addr = vio_addr;

	/*FIXME: testing */
	/* vio_addr = 0x9DE0D000; */

	for (addr = 0xC0000000; addr < 0xFFF00000; addr += 0x100000) {
		pgd = pgd_offset(&init_mm, addr);
		if (pgd_none(*pgd) || !pgd_present(*pgd))
			continue;

		pud = pud_offset(pgd, addr);
		if (pud_none(*pud) || !pud_present(*pud))
			continue;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd) || !pmd_present(*pmd))
			continue;

		/* pr_crit("[EMI MPU] ============= addr = %x\n", addr); */

#ifndef CONFIG_ARM_LPAE
		if ((pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_TABLE) {
			/* Page table entry */
			/* pr_crit("[EMI MPU] 2nd Entry pmd: %lx, *pmd = %lx\n",
			   (unsigned long)(pmd), (unsigned long)pmd_val(*(pmd))); */
			addr_2nd = addr;
			addr_2nd_end = addr_2nd + 0x100000;
			for (; addr_2nd < (addr_2nd_end); addr_2nd += 0x1000) {
				pte = pte_offset_map(pmd, addr_2nd);
				/* pr_crit("[EMI MPU] pmd: %x, pte: %x, *pte = %x, addr_2nd = 0x%x,
				   addr_2nd_end = 0x%x\n", (unsigned long)(pmd), (unsigned long)(pte),
				   (unsigned long)pte_val(*(pte)), addr_2nd, addr_2nd_end); */
				if (((unsigned long)v_addr & PAGE_MASK) ==
				    ((unsigned long)pte_val(*(pte)) & PAGE_MASK)) {
					pr_crit
					    ("[EMI MPU] Find page entry section at pte: %lx. violation address = 0x%x\n",
					     (unsigned long)(pte), v_addr);
					return;
				}
			}
		} else {
			/* pr_crit("[EMI MPU] Section pmd: %x, addr = 0x%x\n", (unsigned long)(pmd), addr); */
			/* Section */
			/* if(v_addr>>20 == (unsigned long)pmd_val(*(pmd))>>20) */
			if (((unsigned long)pmd_val(*(pmd)) & SECTION_MASK) ==
			    ((unsigned long)v_addr & SECTION_MASK)) {
				pr_crit
				    ("[EMI MPU] Find page entry section at pmd: %lx. violation address = 0x%x\n",
				     (unsigned long)(pmd), v_addr);
				return;
			}
		}
#else
		/* TBD */
#endif
	}
	pr_crit("[EMI MPU] ****** Can not find page table entry! violation address = 0x%x ******\n",
		v_addr);

	return;
}

void emi_mpu_work_callback(struct work_struct *work)
{
	pr_crit("[EMI MPU] Enter EMI MPU workqueue!\n");
	mtk_search_full_pgtab();
	pr_crit("[EMI MPU] Exit EMI MPU workqueue!\n");
}

static ssize_t pgt_scan_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t pgt_scan_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	unsigned int ret;

	if (unlikely(sscanf(buf, "%u", &value) != 1))
		return -EINVAL;

	if (value == 1) {
		ret = queue_work(emi_mpu_workqueue, &emi_mpu_work);
		if (!ret)
			pr_crit(KERN_DEBUG "[EMI MPU] submit workqueue failed, ret = %d\n", ret);
	}

	return count;
}

DRIVER_ATTR(pgt_scan, 0644, pgt_scan_show, pgt_scan_store);

#ifdef ENABLE_EMI_CHKER
static void emi_axi_set_chker(const unsigned int setting)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));
	value &= ~(0x7 << 16);
	value |= (setting);

	mt65xx_reg_sync_writel(value, EMI_CHKER);
}

static void emi_axi_set_master(const unsigned int setting)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));
	value &= ~(0x0F << AXI_NON_ALIGN_CHK_MST);
	value |= (setting & 0xF) << AXI_NON_ALIGN_CHK_MST;

	mt65xx_reg_sync_writel(value, EMI_CHKER);
}

static void emi_axi_dump_info(int aee_ke_en)
{
	int value, master_ID;
	char *master_name;

	value = readl(IOMEM(EMI_CHKER));
	master_ID = (value & 0x0000FFFF);

	if (value & 0x0000FFFF) {
		pr_crit(KERN_CRIT "AXI violation.\n");
		pr_crit(KERN_CRIT
			"[EMI MPU AXI] Debug info start ----------------------------------------\n");

		pr_crit(KERN_CRIT "EMI_CHKER = %x.\n", value);
		pr_crit(KERN_CRIT "Violation address is 0x%x.\n", readl(IOMEM(EMI_CHKER_ADR)));
		pr_crit(KERN_CRIT "Violation master ID is 0x%x.\n", master_ID);
		pr_crit(KERN_CRIT
			"Violation type is: AXI_ADR_CHK_EN(%d), AXI_LOCK_CHK_EN(%d), AXI_NON_ALIGN_CHK_EN(%d).\n",
			(value & (1 << AXI_ADR_VIO)) ? 1 : 0,
			(value & (1 << AXI_LOCK_ISSUE)) ? 1 : 0,
			(value & (1 << AXI_NON_ALIGN_ISSUE)) ? 1 : 0);
		pr_crit(KERN_CRIT "%s violation.\n",
			(value & (1 << AXI_VIO_WR)) ? "Write" : "Read");

		pr_crit(KERN_CRIT
			"[EMI MPU AXI] Debug info end ----------------------------------------\n");

		master_name = __id2name(master_ID);
#ifdef CONFIG_MTK_AEE_FEATURE
		if (aee_ke_en)
			aee_kernel_exception("EMI MPU AXI",
					     "AXI violation.\EMI_CHKER = 0x%x, module is %s.\n",
					     value, master_name);
#endif
		/* clear AXI checker status */
		mt65xx_reg_sync_writel((1 << AXI_VIO_CLR) | readl(IOMEM(EMI_CHKER)), EMI_CHKER);
	}
}

static void emi_axi_vio_timer_func(unsigned long a)
{
	emi_axi_dump_info(1);

	mod_timer(&emi_axi_vio_timer, jiffies + AXI_VIO_MONITOR_TIME);
}

static ssize_t emi_axi_vio_show(struct device_driver *driver, char *buf)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));

	emi_axi_dump_info(0);

	return snprintf(buf, PAGE_SIZE,
			"AXI vio setting is: ADR_CHK_EN %s, LOCK_CHK_EN %s, NON_ALIGN_CHK_EN %s\n",
			(value & (1 << AXI_ADR_CHK_EN)) ? "ON" : "OFF",
			(value & (1 << AXI_LOCK_CHK_EN)) ? "ON" : "OFF",
			(value & (1 << AXI_NON_ALIGN_CHK_EN)) ? "ON" : "OFF");
}

ssize_t emi_axi_vio_store(struct device_driver *driver, const char *buf, size_t count)
{
	int value;
	int cpu = 0;		/* assign timer to CPU0 to avoid CPU plug-out and timer will be unavailable */

	value = readl(IOMEM(EMI_CHKER));

	if (!strncmp(buf, "ADR_CHK_ON", strlen("ADR_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_ADR_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "LOCK_CHK_ON", strlen("LOCK_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_LOCK_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "NON_ALIGN_CHK_ON", strlen("NON_ALIGN_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_NON_ALIGN_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "OFF", strlen("OFF"))) {
		emi_axi_set_chker(0);
		del_timer(&emi_axi_vio_timer);
	} else {
		pr_crit("invalid setting\n");
	}

	return count;
}

DRIVER_ATTR(emi_axi_vio, 0644, emi_axi_vio_show, emi_axi_vio_store);

#endif				/* #ifdef ENABLE_EMI_CHKER */

/*
static int emi_mpu_panic_cb(struct notifier_block *this, unsigned long event, void *ptr)
{
    emi_axi_dump_info(1);

    return NOTIFY_DONE;
}*/

static struct device_driver emi_mpu_ctrl = {
	.name = "emi_mpu_ctrl",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
};

/*
static struct notifier_block emi_mpu_blk = {
	.notifier_call	= emi_mpu_panic_cb,
};*/

static int __init emi_mpu_mod_init(void)
{
	int ret;
	struct basic_dram_setting DRAM_setting;

	pr_crit(KERN_INFO "Initialize EMI MPU.\n");

	spin_lock_init(&emi_mpu_lock);

	__clear_emi_mpu_vio();


	/* Set Device APC initialization for EMI-MPU. */
	mt_devapc_emi_initial();

	/*
	 * NoteXXX: Interrupts of vilation (including SPC in SMI, or EMI MPU)
	 *          are triggered by the device APC.
	 *          Need to share the interrupt with the SPC driver.
	 */

	ret =
	    request_irq(APARM_DOMAIN_IRQ_BIT_ID, (irq_handler_t) mpu_violation_irq,
			IRQF_TRIGGER_LOW | IRQF_SHARED, "mt_emi_mpu", &emi_mpu_ctrl);
	if (ret != 0) {
		pr_crit(KERN_CRIT "Fail to request EMI_MPU interrupt. Error = %d.\n", ret);
		return ret;
	}
#if 1
	acquire_dram_setting(&DRAM_setting);
	pr_crit("[EMI] EMI_CONA =  0x%x\n", readl(IOMEM(EMI_CONA)));
	pr_crit("[EMI] Support channel number %d\n", DRAM_setting.channel_nr);
	pr_crit("[EMI] Channel 0 : rank 0 : %d Gb, segment no : %d\n",
		DRAM_setting.channel[0].rank[0].rank_size,
		DRAM_setting.channel[0].rank[0].segment_nr);
	pr_crit("[EMI] Channel 0 : rank 1 : %d Gb, segment no : %d\n",
		DRAM_setting.channel[0].rank[1].rank_size,
		DRAM_setting.channel[0].rank[1].segment_nr);
	pr_crit("[EMI] Channel 1 : rank 0 : %d Gb, segment no : %d\n",
		DRAM_setting.channel[1].rank[0].rank_size,
		DRAM_setting.channel[1].rank[0].segment_nr);
	pr_crit("[EMI] Channel 1 : rank 1 : %d Gb, segment no : %d\n",
		DRAM_setting.channel[1].rank[1].rank_size,
		DRAM_setting.channel[1].rank[1].segment_nr);
#endif

#ifdef ENABLE_EMI_CHKER
	/* AXI violation monitor setting and timer function create */
	mt65xx_reg_sync_writel((1 << AXI_VIO_CLR) | readl(IOMEM(EMI_CHKER)), EMI_CHKER);
	emi_axi_set_master(MASTER_ALL);
	init_timer(&emi_axi_vio_timer);
	emi_axi_vio_timer.expires = jiffies + AXI_VIO_MONITOR_TIME;
	emi_axi_vio_timer.function = &emi_axi_vio_timer_func;
	emi_axi_vio_timer.data = ((unsigned long)0);
#endif				/* #ifdef ENABLE_EMI_CHKER */

#if !defined(USER_BUILD_KERNEL)
#ifdef ENABLE_EMI_CHKER
	/* Enable AXI 4KB boundary violation monitor timer */
	/* emi_axi_set_chker(1 << AXI_ADR_CHK_EN); */
	/* add_timer_on(&emi_axi_vio_timer, 0); */
#endif

	/* register driver and create sysfs files */
	ret = driver_register(&emi_mpu_ctrl);
	if (ret)
		pr_crit(KERN_CRIT "Fail to register EMI_MPU driver.\n");

	ret = driver_create_file(&emi_mpu_ctrl, &driver_attr_mpu_config);
	if (ret)
		pr_crit(KERN_CRIT "Fail to create MPU config sysfs file.\n");
#ifdef ENABLE_EMI_CHKER
	ret = driver_create_file(&emi_mpu_ctrl, &driver_attr_emi_axi_vio);
	if (ret)
		pr_crit(KERN_CRIT "Fail to create AXI violation monitor sysfs file.\n");
#endif
	ret = driver_create_file(&emi_mpu_ctrl, &driver_attr_pgt_scan);
	if (ret)
		pr_crit(KERN_CRIT "Fail to create pgt scan sysfs file.\n");
#endif

	/* atomic_notifier_chain_register(&panic_notifier_list, &emi_mpu_blk); */

	/* Create a workqueue to search pagetable entry */
	emi_mpu_workqueue = create_singlethread_workqueue("emi_mpu");
	INIT_WORK(&emi_mpu_work, emi_mpu_work_callback);
	/*Init for testing */
	/*emi_mpu_set_region_protection(0x9c000000,
	   0x9d7fffff, //MD_IMG_REGION_LEN
	   0,    //region
	   SET_ACCESS_PERMISSON(SEC_R_NSEC_R, SEC_R_NSEC_R, SEC_R_NSEC_R, SEC_R_NSEC_R)); */

	return 0;
}

static void __exit emi_mpu_mod_exit(void)
{
}
module_init(emi_mpu_mod_init);
module_exit(emi_mpu_mod_exit);

/* EXPORT_SYMBOL(start_mm_mau_protect); */
