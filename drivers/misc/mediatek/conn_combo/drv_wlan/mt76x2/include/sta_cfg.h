/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2009, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    sta_cfg.h

    Abstract:

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------

*/

#ifndef __STA_CFG_H__
#define __STA_CFG_H__

INT RTMPSTAPrivIoctlSet(IN RTMP_ADAPTER * pAd, IN PSTRING SetProcName,
			IN PSTRING ProcArg);
INT RtmpIoctl_rt_private_get_statistics(IN RTMP_ADAPTER * pAd,
			IN VOID * pData, IN ULONG Data);
#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
/* set WOW enable */
INT Set_WOW_Enable(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set GPIO pin for wake-up signal */
INT Set_WOW_GPIO(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set delay time for WOW really enable */
INT Set_WOW_Delay(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set wake up hold time */
INT Set_WOW_Hold(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set wakeup signal type */
INT Set_WOW_InBand(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set GPIO high/low active */
INT Set_WOW_HighActive(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set always trigger wakeup */
INT Set_WOW_Always_Trigger(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set IPv4 TCP port for wake up  */
INT Set_WOW_TcpPort_v4(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set IPv6 TCP port for wake up  */
INT Set_WOW_TcpPort_v6(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set IPv4 UDP port for wake up  */
INT Set_WOW_UdpPort_v4(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set IPv6 UDP port for wake up  */
INT Set_WOW_UdpPort_v6(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* set IP for wake up  */
INT Set_WOW_IP(IN PRTMP_ADAPTER pAd, IN PSTRING arg);

#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

#ifdef RTMP_MAC_USB
/* Sets the FW into WOW Suspend mode */
INT Set_UsbWOWSuspend(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
/* Resume the FW to Normal mode */
INT Set_UsbWOWResume(IN PRTMP_ADAPTER pAd, IN PSTRING arg);

INT Set_Register_Dump(IN PRTMP_ADAPTER pAd, IN INT arg);

INT Set_Chip_Reset(IN PRTMP_ADAPTER pAd, IN PSTRING arg);

/* USB2.0 & USB3.0 switch */
INT Set_UsbSwitch(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
#endif /* RTMP_MAC_USB */

#endif /* __STA_CFG_H__ */
