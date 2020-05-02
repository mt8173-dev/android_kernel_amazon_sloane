/*
 ***************************************************************************
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ***************************************************************************

	Module Name:
	pbf.h

	Abstract:
	Ralink Wireless Chip MAC related definition & structures

	Revision History:
	Who			When		  What
	--------	----------	  ----------------------------------------------
*/

#ifndef __PBF_H__
#define __PBF_H__

#ifdef RLT_MAC
#include "mac_ral/nmac/ral_nmac_pbf.h"
#endif /* RLT_MAC */

#ifdef RTMP_MAC
#include "mac_ral/omac/ral_omac_pbf.h"
#endif /* RTMP_MAC */

/* ================================================================================= */
/* Register format  for PBF                                                                                                                                                     */
/* ================================================================================= */

#define WPDMA_GLO_CFG	0x208
#ifdef RT_BIG_ENDIAN
typedef union _WPDMA_GLO_CFG_STRUC {
	struct {
		UINT32 rx_2b_offset:1;
		UINT32 clk_gate_dis:1;
		UINT32 rsv:14;
#ifdef DESC_32B_SUPPORT
		UINT32 RXHdrScater:7;
		UINT32 Desc32BEn:1;
#else
		UINT32 HDR_SEG_LEN:8;
#endif				/* DESC_32B_SUPPORT */
		UINT32 BigEndian:1;
		UINT32 EnTXWriteBackDDONE:1;
		UINT32 WPDMABurstSIZE:2;
		UINT32 RxDMABusy:1;
		UINT32 EnableRxDMA:1;
		UINT32 TxDMABusy:1;
		UINT32 EnableTxDMA:1;
	} field;
	UINT32 word;
} WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#else
typedef union _WPDMA_GLO_CFG_STRUC {
	struct {
		UINT32 EnableTxDMA:1;
		UINT32 TxDMABusy:1;
		UINT32 EnableRxDMA:1;
		UINT32 RxDMABusy:1;
		UINT32 WPDMABurstSIZE:2;
		UINT32 EnTXWriteBackDDONE:1;
		UINT32 BigEndian:1;
#ifdef DESC_32B_SUPPORT
		UINT32 Desc32BEn:1;
		UINT32 RXHdrScater:7;
#else
		UINT32 HDR_SEG_LEN:8;
#endif				/* DESC_32B_SUPPORT */
		UINT32 rsv:14;
		UINT32 clk_gate_dis:1;
		UINT32 rx_2b_offset:1;
	} field;
	UINT32 word;
} WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#endif /* endif */

#define PBF_SYS_CTRL	 0x0400
/* #define PBF_CFG         0x0404 */

#define PBF_CTRL			0x0410
#define MCU_INT_STA		0x0414
#define MCU_INT_ENA	0x0418
#define TXRXQ_PCNT		0x0438
#define PBF_DBG			0x043c

#endif /* __PBF_H__ */
