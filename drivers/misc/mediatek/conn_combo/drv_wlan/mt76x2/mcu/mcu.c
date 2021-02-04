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
	rtmp_mcu.c

	Abstract:

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#include	"rt_config.h"

INT mcu_sys_init(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS Status;

	/* Load MCU firmware */
	MCU_CTRL_INIT(pAd);

	Status = load_patch(pAd);

	if (Status != NDIS_STATUS_SUCCESS) {
		DBGPRINT_ERR(("load patch failed, Status[=0x%08x]\n", Status));
		return FALSE;
	}

	Status = NICLoadFirmware(pAd);
	if (Status != NDIS_STATUS_SUCCESS) {
		DBGPRINT_ERR(("NICLoadFirmware failed, Status[=0x%08x]\n", Status));
		return FALSE;
	}

	return TRUE;
}

VOID ChipOpsMCUHook(PRTMP_ADAPTER pAd, enum MCU_TYPE MCUType)
{
	RTMP_CHIP_OP *pChipOps = &pAd->chipOps;



#ifdef CONFIG_ANDES_SUPPORT
	if (MCUType == ANDES) {
		RTMP_CHIP_CAP *pChipCap = &pAd->chipCap;


#ifdef RTMP_USB_SUPPORT
		if (pChipCap->need_load_rom_patch) {
			pChipOps->load_rom_patch = andes_usb_load_rom_patch;
			pChipOps->erase_rom_patch = andes_usb_erase_rom_patch;
		}

		if (pChipCap->need_load_fw) {
			pChipOps->loadFirmware = andes_usb_loadfw;
			pChipOps->eraseFirmware = andes_usb_erasefw;
		}

		pChipOps->fw_init = andes_usb_fw_init;
#endif /* RTMP_USB_SUPPORT */

		if (pChipCap->need_load_fw) {
			/* pChipOps->sendCommandToMcu = andes_send_cmd_msg; */
			pChipOps->MCUCtrlInit = andes_ctrl_init;
			pChipOps->MCUCtrlExit = andes_ctrl_exit;
			pChipOps->Calibration = andes_calibration;
			pChipOps->BurstWrite = andes_burst_write;
			pChipOps->BurstRead = andes_burst_read;
			pChipOps->RandomRead = andes_random_read;
			pChipOps->RFRandomRead = andes_rf_random_read;
			pChipOps->ReadModifyWrite = andes_read_modify_write;
			pChipOps->RFReadModifyWrite = andes_rf_read_modify_write;
			pChipOps->RandomWrite = andes_random_write;
			pChipOps->RFRandomWrite = andes_rf_random_write;
#ifdef CONFIG_ANDES_BBP_RANDOM_WRITE_SUPPORT
			pChipOps->BBPRandomWrite = andes_bbp_random_write;
#endif /* CONFIG_ANDES_BBP_RANDOM_WRITE_SUPPORT */
			pChipOps->sc_random_write = andes_sc_random_write;
			pChipOps->sc_rf_random_write = andes_sc_rf_random_write;
			pChipOps->PwrSavingOP = andes_pwr_saving;
#ifdef NEW_WOW_SUPPORT
			pChipOps->AsicWOWEnable = andes_wow_enable;
			pChipOps->AsicWOWDisable = andes_wow_disable;
#endif /* endif */
		}
	}
#endif /* CONFIG_ANDES_SUPPORT */
}
