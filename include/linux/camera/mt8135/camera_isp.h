#ifndef _MT_ISP_H
#define _MT_ISP_H

#include <linux/ioctl.h>

/*******************************************************************************
*
********************************************************************************/
#define ISP_DEV_MAJOR_NUMBER    251
#define ISP_MAGIC               'k'
/*******************************************************************************
*
********************************************************************************/
/* CAM_CTL_INT_STATUS */
#define ISP_IRQ_INT_STATUS_VS1_ST               ((unsigned int)1 << 0)
#define ISP_IRQ_INT_STATUS_TG1_ST1              ((unsigned int)1 << 1)
#define ISP_IRQ_INT_STATUS_TG1_ST2              ((unsigned int)1 << 2)
#define ISP_IRQ_INT_STATUS_EXPDON1_ST           ((unsigned int)1 << 3)
#define ISP_IRQ_INT_STATUS_TG1_ERR_ST           ((unsigned int)1 << 4)
#define ISP_IRQ_INT_STATUS_VS2_ST               ((unsigned int)1 << 5)
#define ISP_IRQ_INT_STATUS_TG2_ST1              ((unsigned int)1 << 6)
#define ISP_IRQ_INT_STATUS_TG2_ST2              ((unsigned int)1 << 7)
#define ISP_IRQ_INT_STATUS_EXPDON2_ST           ((unsigned int)1 << 8)
#define ISP_IRQ_INT_STATUS_TG2_ERR_ST           ((unsigned int)1 << 9)
#define ISP_IRQ_INT_STATUS_PASS1_TG1_DON_ST     ((unsigned int)1 << 10)
#define ISP_IRQ_INT_STATUS_PASS1_TG2_DON_ST     ((unsigned int)1 << 11)
#define ISP_IRQ_INT_STATUS_SOF1_INT_ST          ((unsigned int)1 << 12)
#define ISP_IRQ_INT_STATUS_CQ_ERR_ST            ((unsigned int)1 << 13)
#define ISP_IRQ_INT_STATUS_PASS2_DON_ST         ((unsigned int)1 << 14)
#define ISP_IRQ_INT_STATUS_TPIPE_DON_ST         ((unsigned int)1 << 15)
#define ISP_IRQ_INT_STATUS_AF_DON_ST            ((unsigned int)1 << 16)
#define ISP_IRQ_INT_STATUS_FLK_DON_ST           ((unsigned int)1 << 17)
#define ISP_IRQ_INT_STATUS_FMT_DON_ST           ((unsigned int)1 << 18)
#define ISP_IRQ_INT_STATUS_CQ_DON_ST            ((unsigned int)1 << 19)
#define ISP_IRQ_INT_STATUS_IMGO_ERR_ST          ((unsigned int)1 << 20)
#define ISP_IRQ_INT_STATUS_AAO_ERR_ST           ((unsigned int)1 << 21)
#define ISP_IRQ_INT_STATUS_LCSO_ERR_ST          ((unsigned int)1 << 22)
#define ISP_IRQ_INT_STATUS_IMG2O_ERR_ST         ((unsigned int)1 << 23)
#define ISP_IRQ_INT_STATUS_ESFKO_ERR_ST         ((unsigned int)1 << 24)
#define ISP_IRQ_INT_STATUS_FLK_ERR_ST           ((unsigned int)1 << 25)
#define ISP_IRQ_INT_STATUS_LSC_ERR_ST           ((unsigned int)1 << 26)
#define ISP_IRQ_INT_STATUS_LSC2_ERR_ST          ((unsigned int)1 << 27)
#define ISP_IRQ_INT_STATUS_BPC_ERR_ST           ((unsigned int)1 << 28)
#define ISP_IRQ_INT_STATUS_LCE_ERR_ST           ((unsigned int)1 << 29)
#define ISP_IRQ_INT_STATUS_DMA_ERR_ST           ((unsigned int)1 << 30)
/* CAM_CTL_DMA_INT */
#define ISP_IRQ_DMA_INT_IMGO_DONE_ST            ((unsigned int)1 << 0)
#define ISP_IRQ_DMA_INT_IMG2O_DONE_ST           ((unsigned int)1 << 1)
#define ISP_IRQ_DMA_INT_AAO_DONE_ST             ((unsigned int)1 << 2)
#define ISP_IRQ_DMA_INT_LCSO_DONE_ST            ((unsigned int)1 << 3)
#define ISP_IRQ_DMA_INT_ESFKO_DONE_ST           ((unsigned int)1 << 4)
#define ISP_IRQ_DMA_INT_DISPO_DONE_ST           ((unsigned int)1 << 5)
#define ISP_IRQ_DMA_INT_VIDO_DONE_ST            ((unsigned int)1 << 6)
#define ISP_IRQ_DMA_INT_VRZO_DONE_ST            ((unsigned int)1 << 7)
#define ISP_IRQ_DMA_INT_CQ0_ERR_ST              ((unsigned int)1 << 8)
#define ISP_IRQ_DMA_INT_CQ0_DONE_ST             ((unsigned int)1 << 9)
#define ISP_IRQ_DMA_INT_SOF2_INT_ST             ((unsigned int)1 << 10)
#define ISP_IRQ_DMA_INT_BUF_OVL_ST              ((unsigned int)1 << 11)
#define ISP_IRQ_DMA_INT_TG1_GBERR_ST            ((unsigned int)1 << 12)
#define ISP_IRQ_DMA_INT_TG2_GBERR_ST            ((unsigned int)1 << 13)
/* CAM_CTL_INTB_STATUS */
#define ISP_IRQ_INTB_STATUS_CQ_ERR_ST           ((unsigned int)1 << 13)
#define ISP_IRQ_INTB_STATUS_PASS2_DON_ST        ((unsigned int)1 << 14)
#define ISP_IRQ_INTB_STATUS_TPIPE_DON_ST        ((unsigned int)1 << 15)
#define ISP_IRQ_INTB_STATUS_CQ_DON_ST           ((unsigned int)1 << 19)
#define ISP_IRQ_INTB_STATUS_IMGO_ERR_ST         ((unsigned int)1 << 20)
#define ISP_IRQ_INTB_STATUS_LCSO_ERR_ST         ((unsigned int)1 << 22)
#define ISP_IRQ_INTB_STATUS_IMG2O_ERR_ST        ((unsigned int)1 << 23)
#define ISP_IRQ_INTB_STATUS_LSC_ERR_ST          ((unsigned int)1 << 26)
#define ISP_IRQ_INTB_STATUS_LCE_ERR_ST          ((unsigned int)1 << 29)
#define ISP_IRQ_INTB_STATUS_DMA_ERR_ST          ((unsigned int)1 << 30)
/* CAM_CTL_DMAB_INT */
#define ISP_IRQ_DMAB_INT_IMGO_DONE_ST           ((unsigned int)1 << 0)
#define ISP_IRQ_DMAB_INT_IMG2O_DONE_ST          ((unsigned int)1 << 1)
#define ISP_IRQ_DMAB_INT_AAO_DONE_ST            ((unsigned int)1 << 2)
#define ISP_IRQ_DMAB_INT_LCSO_DONE_ST           ((unsigned int)1 << 3)
#define ISP_IRQ_DMAB_INT_ESFKO_DONE_ST          ((unsigned int)1 << 4)
#define ISP_IRQ_DMAB_INT_DISPO_DONE_ST          ((unsigned int)1 << 5)
#define ISP_IRQ_DMAB_INT_VIDO_DONE_ST           ((unsigned int)1 << 6)
#define ISP_IRQ_DMAB_INT_VRZO_DONE_ST           ((unsigned int)1 << 7)
#define ISP_IRQ_DMAB_INT_NR3O_DONE_ST           ((unsigned int)1 << 8)
#define ISP_IRQ_DMAB_INT_NR3O_ERR_ST            ((unsigned int)1 << 9)
/* CAM_CTL_INTC_STATUS */
#define ISP_IRQ_INTC_STATUS_CQ_ERR_ST           ((unsigned int)1 << 13)
#define ISP_IRQ_INTC_STATUS_PASS2_DON_ST        ((unsigned int)1 << 14)
#define ISP_IRQ_INTC_STATUS_TPIPE_DON_ST        ((unsigned int)1 << 15)
#define ISP_IRQ_INTC_STATUS_CQ_DON_ST           ((unsigned int)1 << 19)
#define ISP_IRQ_INTC_STATUS_IMGO_ERR_ST         ((unsigned int)1 << 20)
#define ISP_IRQ_INTC_STATUS_LCSO_ERR_ST         ((unsigned int)1 << 22)
#define ISP_IRQ_INTC_STATUS_IMG2O_ERR_ST        ((unsigned int)1 << 23)
#define ISP_IRQ_INTC_STATUS_LSC_ERR_ST          ((unsigned int)1 << 26)
#define ISP_IRQ_INTC_STATUS_BPC_ERR_ST          ((unsigned int)1 << 28)
#define ISP_IRQ_INTC_STATUS_LCE_ERR_ST          ((unsigned int)1 << 29)
#define ISP_IRQ_INTC_STATUS_DMA_ERR_ST          ((unsigned int)1 << 30)
/* CAM_CTL_DMAC_INT */
#define ISP_IRQ_DMAC_INT_IMGO_DONE_ST           ((unsigned int)1 << 0)
#define ISP_IRQ_DMAC_INT_IMG2O_DONE_ST          ((unsigned int)1 << 1)
#define ISP_IRQ_DMAC_INT_AAO_DONE_ST            ((unsigned int)1 << 2)
#define ISP_IRQ_DMAC_INT_LCSO_DONE_ST           ((unsigned int)1 << 3)
#define ISP_IRQ_DMAC_INT_ESFKO_DONE_ST          ((unsigned int)1 << 4)
#define ISP_IRQ_DMAC_INT_DISPO_DONE_ST          ((unsigned int)1 << 5)
#define ISP_IRQ_DMAC_INT_VIDO_DONE_ST           ((unsigned int)1 << 6)
#define ISP_IRQ_DMAC_INT_VRZO_DONE_ST           ((unsigned int)1 << 7)
#define ISP_IRQ_DMAC_INT_NR3O_DONE_ST           ((unsigned int)1 << 8)
#define ISP_IRQ_DMAC_INT_NR3O_ERR_ST            ((unsigned int)1 << 9)
/* CAM_CTL_INT_STATUSX */
#define ISP_IRQ_INTX_STATUS_VS1_ST              ((unsigned int)1 << 0)
#define ISP_IRQ_INTX_STATUS_TG1_ST1             ((unsigned int)1 << 1)
#define ISP_IRQ_INTX_STATUS_TG1_ST2             ((unsigned int)1 << 2)
#define ISP_IRQ_INTX_STATUS_EXPDON1_ST          ((unsigned int)1 << 3)
#define ISP_IRQ_INTX_STATUS_TG1_ERR_ST          ((unsigned int)1 << 4)
#define ISP_IRQ_INTX_STATUS_VS2_ST              ((unsigned int)1 << 5)
#define ISP_IRQ_INTX_STATUS_TG2_ST1             ((unsigned int)1 << 6)
#define ISP_IRQ_INTX_STATUS_TG2_ST2             ((unsigned int)1 << 7)
#define ISP_IRQ_INTX_STATUS_EXPDON2_ST          ((unsigned int)1 << 8)
#define ISP_IRQ_INTX_STATUS_TG2_ERR_ST          ((unsigned int)1 << 9)
#define ISP_IRQ_INTX_STATUS_PASS1_TG1_DON_ST    ((unsigned int)1 << 10)
#define ISP_IRQ_INTX_STATUS_PASS1_TG2_DON_ST    ((unsigned int)1 << 11)
#define ISP_IRQ_INTX_STATUS_VEC_DON_ST          ((unsigned int)1 << 12)
#define ISP_IRQ_INTX_STATUS_CQ_ERR_ST           ((unsigned int)1 << 13)
#define ISP_IRQ_INTX_STATUS_PASS2_DON_ST        ((unsigned int)1 << 14)
#define ISP_IRQ_INTX_STATUS_TPIPE_DON_ST        ((unsigned int)1 << 15)
#define ISP_IRQ_INTX_STATUS_AF_DON_ST           ((unsigned int)1 << 16)
#define ISP_IRQ_INTX_STATUS_FLK_DON_ST          ((unsigned int)1 << 17)
#define ISP_IRQ_INTX_STATUS_FMT_DON_ST          ((unsigned int)1 << 18)
#define ISP_IRQ_INTX_STATUS_CQ_DON_ST           ((unsigned int)1 << 19)
#define ISP_IRQ_INTX_STATUS_IMGO_ERR_ST         ((unsigned int)1 << 20)
#define ISP_IRQ_INTX_STATUS_AAO_ERR_ST          ((unsigned int)1 << 21)
#define ISP_IRQ_INTX_STATUS_LCSO_ERR_ST         ((unsigned int)1 << 22)
#define ISP_IRQ_INTX_STATUS_IMG2O_ERR_ST        ((unsigned int)1 << 23)
#define ISP_IRQ_INTX_STATUS_ESFKO_ERR_ST        ((unsigned int)1 << 24)
#define ISP_IRQ_INTX_STATUS_FLK_ERR_ST          ((unsigned int)1 << 25)
#define ISP_IRQ_INTX_STATUS_LSC_ERR_ST          ((unsigned int)1 << 26)
#define ISP_IRQ_INTX_STATUS_LSC2_ERR_ST         ((unsigned int)1 << 27)
#define ISP_IRQ_INTX_STATUS_BPC_ERR_ST          ((unsigned int)1 << 28)
#define ISP_IRQ_INTX_STATUS_LCE_ERR_ST          ((unsigned int)1 << 29)
#define ISP_IRQ_INTX_STATUS_DMA_ERR_ST          ((unsigned int)1 << 30)
/* CAM_CTL_DMA_INTX */
#define ISP_IRQ_DMAX_INT_IMGO_DONE_ST           ((unsigned int)1 << 0)
#define ISP_IRQ_DMAX_INT_IMG2O_DONE_ST          ((unsigned int)1 << 1)
#define ISP_IRQ_DMAX_INT_AAO_DONE_ST            ((unsigned int)1 << 2)
#define ISP_IRQ_DMAX_INT_LCSO_DONE_ST           ((unsigned int)1 << 3)
#define ISP_IRQ_DMAX_INT_ESFKO_DONE_ST          ((unsigned int)1 << 4)
#define ISP_IRQ_DMAX_INT_DISPO_DONE_ST          ((unsigned int)1 << 5)
#define ISP_IRQ_DMAX_INT_VIDO_DONE_ST           ((unsigned int)1 << 6)
#define ISP_IRQ_DMAX_INT_VRZO_DONE_ST           ((unsigned int)1 << 7)
#define ISP_IRQ_DMAX_INT_NR3O_DONE_ST           ((unsigned int)1 << 8)
#define ISP_IRQ_DMAX_INT_NR3O_ERR_ST            ((unsigned int)1 << 9)
#define ISP_IRQ_DMAX_INT_CQ_ERR_ST              ((unsigned int)1 << 10)
#define ISP_IRQ_DMAX_INT_BUF_OVL_ST             ((unsigned int)1 << 11)
#define ISP_IRQ_DMAX_INT_TG1_GBERR_ST           ((unsigned int)1 << 12)
#define ISP_IRQ_DMAX_INT_TG2_GBERR_ST           ((unsigned int)1 << 13)

/*******************************************************************************
*
********************************************************************************/
typedef enum {
	ISP_IRQ_CLEAR_NONE,
	ISP_IRQ_CLEAR_WAIT,
	ISP_IRQ_CLEAR_ALL
} ISP_IRQ_CLEAR_ENUM;

typedef enum {
	ISP_IRQ_TYPE_INT,
	ISP_IRQ_TYPE_DMA,
	ISP_IRQ_TYPE_INTB,
	ISP_IRQ_TYPE_DMAB,
	ISP_IRQ_TYPE_INTC,
	ISP_IRQ_TYPE_DMAC,
	ISP_IRQ_TYPE_INTX,
	ISP_IRQ_TYPE_DMAX,
	ISP_IRQ_TYPE_AMOUNT
} ISP_IRQ_TYPE_ENUM;


typedef enum {
	ISP_IRQ_USER_ISPDRV = 0,
	ISP_IRQ_USER_MW = 1,
	ISP_IRQ_USER_3A = 2,
	ISP_IRQ_USER_HWSYNC = 3,
	ISP_IRQ_USER_ACDK = 4,
	ISP_IRQ_USER_EIS = 5,
	ISP_IRQ_USER_MAX
} ISP_IRQ_USER_ENUM;

typedef struct {
	ISP_IRQ_CLEAR_ENUM Clear;
	ISP_IRQ_TYPE_ENUM Type;
	unsigned int Status;
	int UserNumber;
	unsigned int Timeout;
} ISP_WAIT_IRQ_STRUCT;

typedef struct {
	ISP_IRQ_TYPE_ENUM Type;
	ISP_IRQ_USER_ENUM UserNumber;
	unsigned int Status;
} ISP_READ_IRQ_STRUCT;

typedef struct {
	ISP_IRQ_TYPE_ENUM Type;
	ISP_IRQ_USER_ENUM UserNumber;
	unsigned int Status;
} ISP_CLEAR_IRQ_STRUCT;

typedef enum {
	ISP_HOLD_TIME_VD,
	ISP_HOLD_TIME_EXPDONE
} ISP_HOLD_TIME_ENUM;

typedef struct {
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
} ISP_REG_STRUCT;

typedef struct {
	unsigned int Data;	/* pointer to ISP_REG_STRUCT */
	unsigned int Count;	/* count */
} ISP_REG_IO_STRUCT;

typedef void (*pIspCallback) (void);

typedef enum {
	/* Work queue. It is interruptible, so there can be "Sleep" in work queue function. */
	ISP_CALLBACK_WORKQUEUE_VD,
	ISP_CALLBACK_WORKQUEUE_EXPDONE,
	/* Tasklet. It is uninterrupted, so there can NOT be "Sleep" in tasklet function. */
	ISP_CALLBACK_TASKLET_VD,
	ISP_CALLBACK_TASKLET_EXPDONE,
	ISP_CALLBACK_AMOUNT
} ISP_CALLBACK_ENUM;

typedef struct {
	ISP_CALLBACK_ENUM Type;
	pIspCallback Func;
} ISP_CALLBACK_STRUCT;

/*  */
/* length of the two memory areas */
#define RT_BUF_TBL_NPAGES 16
#define ISP_RT_BUF_SIZE 16
#define ISP_RT_CQ0C_BUF_SIZE (ISP_RT_BUF_SIZE)	/* (ISP_RT_BUF_SIZE>>1) */
/* pass1 setting sync index */
#define ISP_REG_P1_CFG_IDX 0x4090
/*  */
typedef enum {
	_cam_tg_ = 0,
	_cam_tg2_,		/* 1 */
	_camsv_tg_,		/* 2 */
	_camsv2_tg_,		/* 3 */
	_cam_tg_max_
} _isp_tg_enum_;

/*  */
typedef enum {
	_imgi_ = 0,
	_imgci_,		/* 1 */
	_vipi_,			/* 2 */
	_vip2i_,		/* 3 */
	_imgo_,			/* 4 */
	_ufdi_,			/* 5 */
	_lcei_,			/* 6 */
	_ufeo_,			/* 7 */
	_rrzo_,			/* 8 */
	_imgo_d_,		/* 9 */
	_rrzo_d_,		/* 10 */
	_img2o_,		/* 11 */
	_img3o_,		/* 12 */
	_img3bo_,		/* 13 */
	_img3co_,		/* 14 */
	_camsv_imgo_,		/* 15 */
	_camsv2_imgo_,		/* 16 */
	_mfbo_,			/* 17 */
	_feo_,			/* 18 */
	_wrot_,			/* 19 */
	_wdma_,			/* 20 */
	_jpeg_,			/* 21 */
	_venc_stream_,		/* 21 */
	_rt_dma_max_
} _isp_dma_enum_;
/*  */
typedef struct {
	unsigned int w;
	unsigned int h;
	unsigned int xsize;
	unsigned int stride;
	unsigned int fmt;
	unsigned int pxl_id;
	unsigned int wbn;
	unsigned int ob;
	unsigned int lsc;
	unsigned int rpg;
	unsigned int m_num_0;
	unsigned int frm_cnt;
} ISP_RT_IMAGE_INFO_STRUCT;

typedef struct {
	unsigned int srcX;
	unsigned int srcY;
	unsigned int srcW;
	unsigned int srcH;
	unsigned int dstW;
	unsigned int dstH;
} ISP_RT_RRZ_INFO_STRUCT;

typedef struct {
	unsigned int memID;
	unsigned int size;
	unsigned int base_vAddr;
	unsigned int base_pAddr;
	unsigned int timeStampS;
	unsigned int timeStampUs;
	unsigned int bFilled;
	ISP_RT_IMAGE_INFO_STRUCT image;
	ISP_RT_RRZ_INFO_STRUCT rrzInfo;
	unsigned int bDequeued;
	signed int bufIdx;	/* used for replace buffer */
} ISP_RT_BUF_INFO_STRUCT;
/*  */
typedef struct {
	unsigned int count;
	ISP_RT_BUF_INFO_STRUCT data[ISP_RT_BUF_SIZE];
} ISP_DEQUE_BUF_INFO_STRUCT;
/*  */
typedef struct {
	unsigned int start;	/* current DMA accessing buffer */
	unsigned int total_count;	/* total buffer number.Include Filled and empty */
	unsigned int empty_count;	/* total empty buffer number include current DMA accessing buffer */
	unsigned int pre_empty_count;	/* previous total empty buffer number include current DMA accessing buffer */
	unsigned int active;
	unsigned int read_idx;
	unsigned int img_cnt;	/* cnt for mapping to which sof */
	ISP_RT_BUF_INFO_STRUCT data[ISP_RT_BUF_SIZE];
} ISP_RT_RING_BUF_INFO_STRUCT;
/*  */
typedef enum {
	ISP_RT_BUF_CTRL_ENQUE,	/* 0 */
	ISP_RT_BUF_CTRL_EXCHANGE_ENQUE,	/* 1 */
	ISP_RT_BUF_CTRL_DEQUE,	/* 2 */
	ISP_RT_BUF_CTRL_IS_RDY,	/* 3 */
	ISP_RT_BUF_CTRL_GET_SIZE,	/* 4 */
	ISP_RT_BUF_CTRL_CLEAR,	/* 5 */
	ISP_RT_BUF_CTRL_MAX
} ISP_RT_BUF_CTRL_ENUM;
/*  */
typedef enum {
	ISP_RTBC_STATE_INIT,	/* 0 */
	ISP_RTBC_STATE_SOF,
	ISP_RTBC_STATE_DONE,
	ISP_RTBC_STATE_MAX
} ISP_RTBC_STATE_ENUM;
/*  */
typedef enum {
	ISP_RTBC_BUF_EMPTY,	/* 0 */
	ISP_RTBC_BUF_FILLED,	/* 1 */
	ISP_RTBC_BUF_LOCKED,	/* 2 */
} ISP_RTBC_BUF_STATE_ENUM;
/*  */
typedef struct {
	ISP_RTBC_STATE_ENUM state;
	unsigned long dropCnt;
	ISP_RT_RING_BUF_INFO_STRUCT ring_buf[_rt_dma_max_];
} ISP_RT_BUF_STRUCT;
/*  */
typedef struct {
	ISP_RT_BUF_CTRL_ENUM ctrl;
	_isp_dma_enum_ buf_id;
	unsigned int data_ptr;
	unsigned int ex_data_ptr;	/* exchanged buffer */
} ISP_BUFFER_CTRL_STRUCT;
/*  */
/* reference count */
#define _use_kernel_ref_cnt_
/*  */
typedef enum {
	ISP_REF_CNT_GET,	/* 0 */
	ISP_REF_CNT_INC,	/* 1 */
	ISP_REF_CNT_DEC,	/* 2 */
	ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE,	/* 3 */
	ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE,	/* 4 */
	ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE,	/* 5 */
	ISP_REF_CNT_MAX
} ISP_REF_CNT_CTRL_ENUM;
/*  */
typedef enum {
	ISP_REF_CNT_ID_IMEM,	/* 0 */
	ISP_REF_CNT_ID_ISP_FUNC,	/* 1 */
	ISP_REF_CNT_ID_GLOBAL_PIPE,	/* 2 */
	ISP_REF_CNT_ID_P1_PIPE,	/* 3 */
	ISP_REF_CNT_ID_P2_PIPE,	/* 4 */
	ISP_REF_CNT_ID_MAX,
} ISP_REF_CNT_ID_ENUM;
/*  */
typedef struct {
	ISP_REF_CNT_CTRL_ENUM ctrl;
	ISP_REF_CNT_ID_ENUM id;
	unsigned long data_ptr;
} ISP_REF_CNT_CTRL_STRUCT;

/* struct for enqueue/dequeue control in ihalpipe wrapper */
typedef enum {
	ISP_ED_BUFQUE_CTRL_ENQUE_FRAME = 0,	/* 0,signal that a specific buffer is enqueued */
	ISP_ED_BUFQUE_CTRL_WAIT_DEQUE,	/* 1,a dequeue thread is waiting to do dequeue */
	ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS,	/* 2,signal that a buffer is dequeued (success) */
	ISP_ED_BUFQUE_CTRL_DEQUE_FAIL,	/* 3,signal that a buffer is dequeued (fail) */
	ISP_ED_BUFQUE_CTRL_WAIT_FRAME,	/* 4,wait for a specific buffer */
	ISP_ED_BUFQUE_CTRL_WAKE_WAITFRAME,	/* 5,wake all sleeped users to check buffer is dequeued or not */
	ISP_ED_BUFQUE_CTRL_CLAER_ALL,	/* 6,free all recored dequeued buffer */
	ISP_ED_BUFQUE_CTRL_MAX
} ISP_ED_BUFQUE_CTRL_ENUM;

typedef struct {
	ISP_ED_BUFQUE_CTRL_ENUM ctrl;
	unsigned int processID;
	unsigned int callerID;
	int p2burstQIdx;
	int p2dupCQIdx;
	unsigned int timeoutUs;
} ISP_ED_BUFQUE_STRUCT;

/********************************************************************************************
 pass1 real time buffer control use cq0c
********************************************************************************************/
/*  */
#define _rtbc_use_cq0c_

#define _MAGIC_NUM_ERR_HANDLING_


#if defined(_rtbc_use_cq0c_)
/*  */
typedef volatile union _CQ_RTBC_FBC_ {
	volatile struct {
		unsigned long FBC_CNT:4;
		unsigned long DROP_INT_EN:1;
		unsigned long rsv_5:6;
		unsigned long RCNT_INC:1;
		unsigned long rsv_12:2;
		unsigned long FBC_EN:1;
		unsigned long LOCK_EN:1;
		unsigned long FB_NUM:4;
		unsigned long RCNT:4;
		unsigned long WCNT:4;
		unsigned long DROP_CNT:4;
	} Bits;
	unsigned long Reg_val;
} CQ_RTBC_FBC;

typedef struct _cq_cmd_st_ {
	unsigned long inst;
	unsigned long data_ptr_pa;
} CQ_CMD_ST;
/*
typedef struct _cq_cmd_rtbc_st_
{
    CQ_CMD_ST imgo;
    CQ_CMD_ST img2o;
    CQ_CMD_ST cq0ci;
    CQ_CMD_ST end;
}CQ_CMD_RTBC_ST;
*/
typedef struct _cq_info_rtbc_st_ {
	CQ_CMD_ST imgo;
	CQ_CMD_ST img2o;
	CQ_CMD_ST next_cq0ci;
	CQ_CMD_ST end;
	unsigned long imgo_base_pAddr;
	unsigned long rrzo_base_pAddr;
	signed int imgo_buf_idx;	/* used for replace buffer */
	signed int rrzo_buf_idx;	/* used for replace buffer */
} CQ_INFO_RTBC_ST;
typedef struct _cq_ring_cmd_st_ {
	CQ_INFO_RTBC_ST cq_rtbc;
	unsigned long next_pa;
	struct _cq_ring_cmd_st_ *pNext;
} CQ_RING_CMD_ST;
typedef struct _cq_rtbc_ring_st_ {
	CQ_RING_CMD_ST rtbc_ring[ISP_RT_CQ0C_BUF_SIZE];
	unsigned long imgo_ring_size;
	unsigned long img2o_ring_size;
} CQ_RTBC_RING_ST;
#endif
/*  */
/********************************************************************************************

********************************************************************************************/


/*******************************************************************************
*
********************************************************************************/
typedef enum {
	ISP_CMD_RESET,		/* Reset */
	ISP_CMD_RESET_BUF,
	ISP_CMD_READ_REG,	/* Read register from driver */
	ISP_CMD_WRITE_REG,	/* Write register to driver */
	ISP_CMD_HOLD_TIME,
	ISP_CMD_HOLD_REG,	/* Hold reg write to hw, on/off */
	ISP_CMD_WAIT_IRQ,	/* Wait IRQ */
	ISP_CMD_READ_IRQ,	/* Read IRQ */
	ISP_CMD_CLEAR_IRQ,	/* Clear IRQ */
	ISP_CMD_DUMP_REG,	/* Dump ISP registers , for debug usage */
	ISP_CMD_SET_USER_PID,	/* for signal */
	ISP_CMD_RT_BUF_CTRL,	/* for pass buffer control */
	ISP_CMD_REF_CNT,	/* get imem reference count */
	ISP_CMD_GET_CHIP_VER,	/* get chip version 0x0000:E1 , 0x1001:E2 */
	ISP_CMD_DEBUG_FLAG,	/* Dump message level */
	ISP_CMD_SENSOR_FREQ_CTRL,	/* sensor frequence control */
	ISP_CMD_REGISTER_IRQ,	/* register for a specific irq */
	ISP_CMD_UNREGISTER_IRQ,	/* unregister for a specific irq */
	ISP_CMD_ED_QUEBUF_CTRL,
	ISP_CMD_UPDATE_REGSCEN,
	ISP_CMD_QUERY_REGSCEN,
	ISP_CMD_UPDATE_BURSTQNUM,
	ISP_CMD_QUERY_BURSTQNUM,
	ISP_CMD_DUMP_ISR_LOG,	/* dump isr log */
	ISP_CMD_GET_CUR_SOF,
	ISP_CMD_GET_DMA_ERR,
	ISP_CMD_GET_INT_ERR,
#ifdef T_STAMP_2_0
	ISP_CMD_SET_FPS,
#endif
} ISP_CMD_ENUM;
/*  */
#define ISP_RESET           _IO(ISP_MAGIC, ISP_CMD_RESET)
#define ISP_RESET_BUF       _IO(ISP_MAGIC, ISP_CMD_RESET_BUF)
#define ISP_READ_REGISTER   _IOWR(ISP_MAGIC, ISP_CMD_READ_REG,      ISP_REG_IO_STRUCT)
#define ISP_WRITE_REGISTER  _IOWR(ISP_MAGIC, ISP_CMD_WRITE_REG,     ISP_REG_IO_STRUCT)
#define ISP_HOLD_REG_TIME   _IOW(ISP_MAGIC, ISP_CMD_HOLD_TIME,     ISP_HOLD_TIME_ENUM)
#define ISP_HOLD_REG        _IOW(ISP_MAGIC, ISP_CMD_HOLD_REG,      bool)
#define ISP_WAIT_IRQ        _IOW(ISP_MAGIC, ISP_CMD_WAIT_IRQ,      ISP_WAIT_IRQ_STRUCT)
#define ISP_READ_IRQ        _IOR(ISP_MAGIC, ISP_CMD_READ_IRQ,      ISP_READ_IRQ_STRUCT)
#define ISP_CLEAR_IRQ       _IOW(ISP_MAGIC, ISP_CMD_CLEAR_IRQ,     ISP_CLEAR_IRQ_STRUCT)
#define ISP_DUMP_REG        _IO(ISP_MAGIC, ISP_CMD_DUMP_REG)
#define ISP_SET_USER_PID    _IOW(ISP_MAGIC, ISP_CMD_SET_USER_PID,  unsigned long)
#define ISP_BUFFER_CTRL     _IOWR(ISP_MAGIC, ISP_CMD_RT_BUF_CTRL,   ISP_BUFFER_CTRL_STRUCT)
#define ISP_REF_CNT_CTRL    _IOWR(ISP_MAGIC, ISP_CMD_REF_CNT,       ISP_REF_CNT_CTRL_STRUCT)
#define ISP_GET_CHIP_VER    _IOR(ISP_MAGIC, ISP_CMD_GET_CHIP_VER,  int)
#define ISP_DEBUG_FLAG      _IOW(ISP_MAGIC, ISP_CMD_DEBUG_FLAG,    unsigned long)
#define ISP_SENSOR_FREQ_CTRL  _IOW(ISP_MAGIC, ISP_CMD_SENSOR_FREQ_CTRL,    unsigned long)
#define ISP_REGISTER_IRQ    _IOW(ISP_MAGIC, ISP_CMD_REGISTER_IRQ,  ISP_WAIT_IRQ_STRUCT)
#define ISP_UNREGISTER_IRQ    _IOW(ISP_MAGIC, ISP_CMD_UNREGISTER_IRQ,  ISP_WAIT_IRQ_STRUCT)
#define ISP_ED_QUEBUF_CTRL     _IOWR(ISP_MAGIC, ISP_CMD_ED_QUEBUF_CTRL, ISP_ED_BUFQUE_STRUCT)
#define ISP_UPDATE_REGSCEN     _IOWR(ISP_MAGIC, ISP_CMD_UPDATE_REGSCEN, unsigned int)
#define ISP_QUERY_REGSCEN     _IOR(ISP_MAGIC, ISP_CMD_QUERY_REGSCEN, unsigned int)
#define ISP_UPDATE_BURSTQNUM _IOW(ISP_MAGIC, ISP_CMD_UPDATE_BURSTQNUM, int)
#define ISP_QUERY_BURSTQNUM _IOR (ISP_MAGIC, ISP_CMD_QUERY_BURSTQNUM, int)
#define ISP_DUMP_ISR_LOG    _IO(ISP_MAGIC, ISP_CMD_DUMP_ISR_LOG)
#define ISP_GET_CUR_SOF     _IOR(ISP_MAGIC, ISP_CMD_GET_CUR_SOF,      unsigned long)
#define ISP_GET_DMA_ERR     _IOWR(ISP_MAGIC, ISP_CMD_GET_DMA_ERR,      unsigned int)
#define ISP_GET_INT_ERR     _IOR(ISP_MAGIC, ISP_CMD_GET_INT_ERR,      unsigned long)
#ifdef T_STAMP_2_0
	#define ISP_SET_FPS     _IOW (ISP_MAGIC, ISP_CMD_SET_FPS,    unsigned int)
#endif
/*  */
bool ISP_RegCallback(ISP_CALLBACK_STRUCT *pCallback);
bool ISP_UnregCallback(ISP_CALLBACK_ENUM Type);
int32_t ISP_MDPClockOnCallback(uint64_t engineFlag);
int32_t ISP_MDPDumpCallback(uint64_t engineFlag, int level);
int32_t ISP_MDPResetCallback(uint64_t engineFlag);

int32_t ISP_MDPClockOffCallback(uint64_t engineFlag);

int32_t ISP_BeginGCECallback(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress);
int32_t ISP_EndGCECallback(uint32_t taskID, uint32_t regCount, uint32_t *regValues);

/*  */
#endif
