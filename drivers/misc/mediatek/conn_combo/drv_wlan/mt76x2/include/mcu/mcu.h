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
	mcu.h

	Abstract:
	MCU related information

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifndef __MCU_H__
#define __MCU_H__

enum MCU_TYPE {
	SWMCU,
	M8051,
	ANDES,
};

/*
 * Power opration
 */
enum PWR_OP {
	RADIO_OFF = 0x30,
	RADIO_ON,
	RADIO_OFF_AUTO_WAKEUP,
	RADIO_OFF_ADVANCE,
	RADIO_ON_ADVANCE,
};

/*
 *  Load code method
 */
enum LOAD_CODE_METHOD {
	HEADER_METHOD,
	BIN_FILE_METHOD,
};

struct _RTMP_ADAPTER;

VOID ChipOpsMCUHook(struct _RTMP_ADAPTER *pAd, enum MCU_TYPE MCUType);
VOID MCUCtrlInit(struct _RTMP_ADAPTER *pAd);
VOID MCUCtrlExit(struct _RTMP_ADAPTER *pAd);

INT mcu_sys_init(struct _RTMP_ADAPTER *pAd);

#endif /* endif */
