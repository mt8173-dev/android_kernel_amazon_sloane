/****************************************************************************
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
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All related CFG80211 function body.

	History:

***************************************************************************/
#ifdef RT_CFG80211_SUPPORT
#define RTMP_MODULE_OS
#define RTMP_MODULE_OS_UTIL

#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_config.h"
#include "chlist.h"

/* all available channels */
UCHAR Cfg80211_Chan[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,

	/* 802.11 UNI / HyperLan 2 */
	36, 40, 44, 48, 52, 56, 60, 64,

	/* 802.11 HyperLan 2 */
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136,

	/* 802.11 UNII */
	140, 149, 153, 157, 161, 165, 169, 173,

	/* Japan */
	184, 188, 192, 196, 208, 212, 216,
};
const int Num_Cfg80211_Chan = (sizeof(Cfg80211_Chan)/sizeof(Cfg80211_Chan[0]));
#ifndef FORCE_CUSTOMIZED_COUNTRY_REGION
UCHAR Cfg80211_RadarChan[] = {
	52, 54, 56, 60, 62, 64, 100, 104, 108, 112, 116, 118, 120, 124, 126, 128, 132, 134, 136, 140,
};
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
/*
	Array of bitrates the hardware can operate with
	in this band. Must be sorted to give a valid "supported
	rates" IE, i.e. CCK rates first, then OFDM.

	For HT, assign MCS in another structure, ieee80211_sta_ht_cap.
*/
const struct ieee80211_rate Cfg80211_SupRate[12] = {
	{
	 .flags = IEEE80211_RATE_SHORT_PREAMBLE,
	 .bitrate = 10,
	 .hw_value = 0,
	 .hw_value_short = 0,
	 },
	{
	 .flags = IEEE80211_RATE_SHORT_PREAMBLE,
	 .bitrate = 20,
	 .hw_value = 1,
	 .hw_value_short = 1,
	 },
	{
	 .flags = IEEE80211_RATE_SHORT_PREAMBLE,
	 .bitrate = 55,
	 .hw_value = 2,
	 .hw_value_short = 2,
	 },
	{
	 .flags = IEEE80211_RATE_SHORT_PREAMBLE,
	 .bitrate = 110,
	 .hw_value = 3,
	 .hw_value_short = 3,
	 },
	{
	 .flags = 0,
	 .bitrate = 60,
	 .hw_value = 4,
	 .hw_value_short = 4,
	 },
	{
	 .flags = 0,
	 .bitrate = 90,
	 .hw_value = 5,
	 .hw_value_short = 5,
	 },
	{
	 .flags = 0,
	 .bitrate = 120,
	 .hw_value = 6,
	 .hw_value_short = 6,
	 },
	{
	 .flags = 0,
	 .bitrate = 180,
	 .hw_value = 7,
	 .hw_value_short = 7,
	 },
	{
	 .flags = 0,
	 .bitrate = 240,
	 .hw_value = 8,
	 .hw_value_short = 8,
	 },
	{
	 .flags = 0,
	 .bitrate = 360,
	 .hw_value = 9,
	 .hw_value_short = 9,
	 },
	{
	 .flags = 0,
	 .bitrate = 480,
	 .hw_value = 10,
	 .hw_value_short = 10,
	 },
	{
	 .flags = 0,
	 .bitrate = 540,
	 .hw_value = 11,
	 .hw_value_short = 11,
	 },
};

static const UINT32 CipherSuites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
#ifdef DOT11W_PMF_SUPPORT
	WLAN_CIPHER_SUITE_AES_CMAC,
#endif /* DOT11W_PMF_SUPPORT */
};

#ifndef FORCE_CUSTOMIZED_COUNTRY_REGION
static BOOLEAN IsRadarChannel(UCHAR ch)
{
	UINT idx = 0;
	for (idx = 0; idx < sizeof(Cfg80211_RadarChan); idx++) {
		if (Cfg80211_RadarChan[idx] == ch)
			return TRUE;
	}

	return FALSE;
}
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */

VOID CFG80211OS_Free_ch_bcn_rate(VOID *pCB)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;

	if (pCfg80211_CB->pCfg80211_Channels != NULL)
		kfree(pCfg80211_CB->pCfg80211_Channels);

	if (pCfg80211_CB->ch_flags_by_beacon != NULL)
		kfree(pCfg80211_CB->ch_flags_by_beacon);

	if (pCfg80211_CB->pCfg80211_Rates != NULL)
		kfree(pCfg80211_CB->pCfg80211_Rates);
	return;
}
/*
========================================================================
Routine Description:
	UnRegister MAC80211 Module.

Arguments:
	pCB				- CFG80211 control block pointer
	pNetDev			- Network device

Return Value:
	NONE

Note:
========================================================================
*/
VOID CFG80211OS_UnRegister(VOID *pCB, VOID *pNetDevOrg)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct net_device *pNetDev = (struct net_device *)pNetDevOrg;

	/* unregister */
	if (pCfg80211_CB->pCfg80211_Wdev != NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("80211> unregister/free wireless device\n"));

		/*
		   Must unregister, or you will suffer problem when you change
		   regulatory domain by using iw.
		 */

#ifdef RFKILL_HW_SUPPORT
		wiphy_rfkill_stop_polling(pCfg80211_CB->pCfg80211_Wdev->wiphy);
#endif /* RFKILL_HW_SUPPORT */
		wiphy_unregister(pCfg80211_CB->pCfg80211_Wdev->wiphy);
		wiphy_free(pCfg80211_CB->pCfg80211_Wdev->wiphy);
		kfree(pCfg80211_CB->pCfg80211_Wdev);
		pCfg80211_CB->pCfg80211_Wdev = NULL;

		CFG80211OS_Free_ch_bcn_rate(pCfg80211_CB);
		pCfg80211_CB->pCfg80211_Channels = NULL;
		pCfg80211_CB->ch_flags_by_beacon = NULL;
		pCfg80211_CB->pCfg80211_Rates = NULL;

		/* must reset to NULL; or kernel will panic in unregister_netdev */
		pNetDev->ieee80211_ptr = NULL;
		SET_NETDEV_DEV(pNetDev, NULL);
	}

	os_free_mem(NULL, pCfg80211_CB);
}

/*
========================================================================
Routine Description:
	Initialize wireless channel in 2.4GHZ and 5GHZ.

Arguments:
	pAd				- WLAN control block pointer
	pWiphy			- WLAN PHY interface
	pChannels		- Current channel info
	pRates			- Current rate info

Return Value:
	TRUE			- init successfully
	FALSE			- init fail

Note:
	TX Power related:

	1. Suppose we can send power to 15dBm in the board.
	2. A value 0x0 ~ 0x1F for a channel. We will adjust it based on 15dBm/
		54Mbps. So if value == 0x07, the TX power of 54Mbps is 15dBm and
		the value is 0x07 in the EEPROM.
	3. Based on TX power value of 54Mbps/channel, adjust another value
		0x0 ~ 0xF for other data rate. (-6dBm ~ +6dBm)

	Other related factors:
	1. TX power percentage from UI/users;
	2. Maximum TX power limitation in the regulatory domain.
========================================================================
*/
BOOLEAN CFG80211_SupBandInit(IN VOID *pCB,
			     IN CFG80211_BAND * pDriverBandInfo,
			     IN VOID *pWiphyOrg, IN VOID *pChannelsOrg, IN VOID *pRatesOrg)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;
	struct ieee80211_channel *pChannels = (struct ieee80211_channel *)pChannelsOrg;
	struct ieee80211_rate *pRates = (struct ieee80211_rate *)pRatesOrg;
	struct ieee80211_supported_band *pBand;
	UINT32 NumOfChan = 0, NumOfRate = 0;
	UINT32 IdLoop = 0;
	UINT32 CurTxPower;
	struct _CH_FLAGS_BEACON *ch_flags_from_beaon = NULL;

	/* sanity check */
	if (pDriverBandInfo->RFICType == 0)
		pDriverBandInfo->RFICType = RFIC_24GHZ | RFIC_5GHZ;

	/* 1. Calcute the Channel Number */
	if (pDriverBandInfo->RFICType & RFIC_5GHZ)
		NumOfChan = CFG80211_NUM_OF_CHAN_2GHZ + CFG80211_NUM_OF_CHAN_5GHZ;
	else
		NumOfChan = CFG80211_NUM_OF_CHAN_2GHZ;

	/* 2. Calcute the Rate Number */
	if (pDriverBandInfo->FlgIsBMode == TRUE)
		NumOfRate = 4;
	else
		NumOfRate = 4 + 8;

	CFG80211DBG(RT_DEBUG_ERROR,
		    ("80211> RFICType= %d, NumOfChan= %d\n", pDriverBandInfo->RFICType, NumOfChan));
	CFG80211DBG(RT_DEBUG_ERROR, ("80211> Number of rate = %d\n", NumOfRate));

	/* 3. Allocate the Channel instance */
	if (pChannels == NULL) {
		pChannels = kzalloc(sizeof(*pChannels) * NumOfChan, GFP_KERNEL);
		if (!pChannels) {
			DBGPRINT(RT_DEBUG_ERROR, ("80211> ieee80211_channel allocation fail!\n"));
			return FALSE;
		}
	}

	if (!ch_flags_from_beaon) {
		ch_flags_from_beaon = kzalloc(sizeof(struct _CH_FLAGS_BEACON) * NumOfChan, GFP_KERNEL);
		if (!ch_flags_from_beaon) {
			kfree(pChannels);
			DBGPRINT(RT_DEBUG_ERROR,
				("80211> chlist flags for beacon update allocat fail\n"));
			return FALSE;
		}
	}

	/* 4. Allocate the Rate instance */
	if (pRates == NULL) {
		pRates = kzalloc(sizeof(*pRates) * NumOfRate, GFP_KERNEL);
		if (!pRates) {
			kfree(pChannels);
			kfree(ch_flags_from_beaon);
			DBGPRINT(RT_DEBUG_ERROR, ("80211> ieee80211_rate allocation fail!\n"));
			return FALSE;
		}
	}

	/* get TX power */
#ifdef SINGLE_SKU
	CurTxPower = pDriverBandInfo->DefineMaxTxPwr;	/* dBm */
#else
	CurTxPower = 20;	/* unknown */
#endif /* SINGLE_SKU */

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> CurTxPower = %d dBm\n", CurTxPower));

	/* 5. init channel */
	for (IdLoop = 0; IdLoop < NumOfChan; IdLoop++) {

		if (IdLoop >= 14) {
			pChannels[IdLoop].band = IEEE80211_BAND_5GHZ;
			pChannels[IdLoop].center_freq =
			    ieee80211_channel_to_frequency(Cfg80211_Chan[IdLoop],
							   IEEE80211_BAND_5GHZ);
		} else {
			pChannels[IdLoop].band = IEEE80211_BAND_2GHZ;
			pChannels[IdLoop].center_freq =
			    ieee80211_channel_to_frequency(Cfg80211_Chan[IdLoop],
							   IEEE80211_BAND_2GHZ);
		}

		ch_flags_from_beaon[IdLoop].ch = Cfg80211_Chan[IdLoop];
		ch_flags_from_beaon[IdLoop].flags = 0xffffffff;
		pChannels[IdLoop].hw_value = IdLoop;

		if (IdLoop < CFG80211_NUM_OF_CHAN_2GHZ)
			pChannels[IdLoop].max_power = CurTxPower;
		else
			pChannels[IdLoop].max_power = CurTxPower;

		pChannels[IdLoop].max_antenna_gain = 0xff;

#ifndef FORCE_CUSTOMIZED_COUNTRY_REGION
		/* if (RadarChannelCheck(pAd, Cfg80211_Chan[IdLoop])) */
		if (IsRadarChannel(Cfg80211_Chan[IdLoop])) {
			pChannels[IdLoop].flags = 0;
			CFG80211DBG(RT_DEBUG_TRACE,
				    ("====> Rader Channel %d\n", Cfg80211_Chan[IdLoop]));
			pChannels[IdLoop].flags |=
			    (IEEE80211_CHAN_RADAR | IEEE80211_CHAN_PASSIVE_SCAN);
		}
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */

/*		CFG_TODO:
		pChannels[IdLoop].flags
		enum ieee80211_channel_flags {
			IEEE80211_CHAN_DISABLED		= 1<<0,
			IEEE80211_CHAN_PASSIVE_SCAN	= 1<<1,
			IEEE80211_CHAN_NO_IBSS		= 1<<2,
			IEEE80211_CHAN_RADAR		= 1<<3,
			IEEE80211_CHAN_NO_HT40PLUS	= 1<<4,
			IEEE80211_CHAN_NO_HT40MINUS	= 1<<5,
		};
 */
	}

	/* 6. init rate */
	for (IdLoop = 0; IdLoop < NumOfRate; IdLoop++)
		memcpy(&pRates[IdLoop], &Cfg80211_SupRate[IdLoop], sizeof(*pRates));

/*		CFG_TODO:
		enum ieee80211_rate_flags {
			IEEE80211_RATE_SHORT_PREAMBLE	= 1<<0,
			IEEE80211_RATE_MANDATORY_A	= 1<<1,
			IEEE80211_RATE_MANDATORY_B	= 1<<2,
			IEEE80211_RATE_MANDATORY_G	= 1<<3,
			IEEE80211_RATE_ERP_G		= 1<<4,
		};
 */

	/* 7. Fill the Band 2.4GHz */
	pBand = &pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_2GHZ];
	if (pDriverBandInfo->RFICType & RFIC_24GHZ) {
		pBand->n_channels = CFG80211_NUM_OF_CHAN_2GHZ;
		pBand->n_bitrates = NumOfRate;
		if (pBand->channels != pChannels)	{
			pBand->channels = pChannels;
		};
		if (pBand->bitrates != pRates)	{
			pBand->bitrates = pRates;
		};

		/* for HT, assign pBand->ht_cap */
		pBand->ht_cap.ht_supported = true;
		pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
		    IEEE80211_HT_CAP_SM_PS |
		    IEEE80211_HT_CAP_SGI_40 | IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_DSSSCCK40;
		pBand->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;	/* 2 ^ 16 */
		pBand->ht_cap.ampdu_density = pDriverBandInfo->MpduDensity;	/* YF_TODO */

		memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> TxStream = %d\n", pDriverBandInfo->TxStream));

		switch (pDriverBandInfo->TxStream) {
		case 1:
		default:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(150);
			break;

		case 2:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_mask[1] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(300);
			break;

		case 3:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_mask[1] = 0xff;
			pBand->ht_cap.mcs.rx_mask[2] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(450);
			break;
		}

		pBand->ht_cap.mcs.rx_mask[4] = 0x01;	/* 40MHz */
		pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;

		pWiphy->bands[IEEE80211_BAND_2GHZ] = pBand;
	} else {
		pWiphy->bands[IEEE80211_BAND_2GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
	}

	/* 8. Fill the Band 5GHz */
	pBand = &pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_5GHZ];
	if (pDriverBandInfo->RFICType & RFIC_5GHZ) {
		pBand->n_channels = CFG80211_NUM_OF_CHAN_5GHZ;
		pBand->n_bitrates = NumOfRate - 4;	/*Disable 11B rate */

		if (pBand->channels != &pChannels[CFG80211_NUM_OF_CHAN_2GHZ])	{
			pBand->channels = &pChannels[CFG80211_NUM_OF_CHAN_2GHZ];
		};
		if (pBand->bitrates != &pRates[4])	{
			pBand->bitrates = &pRates[4];
		};

		/* for HT, assign pBand->ht_cap */
		pBand->ht_cap.ht_supported = true;
		pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
		    IEEE80211_HT_CAP_SM_PS |
		    IEEE80211_HT_CAP_SGI_40 | IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_DSSSCCK40;
		pBand->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;	/* 2 ^ 16 */
		pBand->ht_cap.ampdu_density = pDriverBandInfo->MpduDensity;	/* YF_TODO */

		memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
		switch (pDriverBandInfo->RxStream) {
		case 1:
		default:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(150);
			break;

		case 2:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_mask[1] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(300);
			break;

		case 3:
			pBand->ht_cap.mcs.rx_mask[0] = 0xff;
			pBand->ht_cap.mcs.rx_mask[1] = 0xff;
			pBand->ht_cap.mcs.rx_mask[2] = 0xff;
			pBand->ht_cap.mcs.rx_highest = cpu_to_le16(450);
			break;
		}

		pBand->ht_cap.mcs.rx_mask[4] = 0x01;	/* 40MHz */
		pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;

		pWiphy->bands[IEEE80211_BAND_5GHZ] = pBand;
	} else {
		pWiphy->bands[IEEE80211_BAND_5GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
	}

	/* 9. re-assign to mainDevice info */
	/* CFG80211OS_SupBandReInit assign same pointers, so have to check it*/
	if (pCfg80211_CB->pCfg80211_Channels != pChannels)	{
		if (pCfg80211_CB->pCfg80211_Channels)
			kfree(pCfg80211_CB->pCfg80211_Channels);

		pCfg80211_CB->pCfg80211_Channels = pChannels;
	};

	if (pCfg80211_CB->pCfg80211_Rates != pRates)	{
		if (pCfg80211_CB->pCfg80211_Rates)
			kfree(pCfg80211_CB->pCfg80211_Rates);

		pCfg80211_CB->pCfg80211_Rates = pRates;
	};

	if (pCfg80211_CB->ch_flags_by_beacon)
		kfree(pCfg80211_CB->ch_flags_by_beacon);

	pCfg80211_CB->ch_flags_by_beacon = ch_flags_from_beaon;

	return TRUE;
}

/*
========================================================================
Routine Description:
	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.

Arguments:
	pCB				- CFG80211 control block pointer
	pBandInfo		- Band information

Return Value:
	TRUE			- re-init successfully
	FALSE			- re-init fail

Note:
	CFG80211_SupBandInit() is called in xx_probe().
	But we do not have complete chip information in xx_probe() so we
	need to re-init bands in xx_open().
========================================================================
*/
BOOLEAN CFG80211OS_SupBandReInit(IN VOID *pCB, IN CFG80211_BAND * pBandInfo)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy;

	if ((pCfg80211_CB == NULL) || (pCfg80211_CB->pCfg80211_Wdev == NULL))
		return FALSE;

	pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;

	if (pWiphy != NULL) {
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> re-init bands...\n"));

		/* re-init bands */
		CFG80211_SupBandInit(pCfg80211_CB, pBandInfo, pWiphy,
				     pCfg80211_CB->pCfg80211_Channels,
				     pCfg80211_CB->pCfg80211_Rates);

		/* re-init PHY */
		pWiphy->rts_threshold = pBandInfo->RtsThreshold;
		pWiphy->frag_threshold = pBandInfo->FragmentThreshold;
		pWiphy->retry_short = pBandInfo->RetryMaxCnt & 0xff;
		pWiphy->retry_long = (pBandInfo->RetryMaxCnt & 0xff00) >> 8;

		return TRUE;
	}

	return FALSE;
}

/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from driver.

Arguments:
	pAd				- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
VOID CFG80211OS_RegHint(IN VOID *pCB, IN UCHAR *pCountryIe, IN ULONG CountryIeLen)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;

	CFG80211DBG(RT_DEBUG_ERROR,
		    ("crda> regulatory domain hint: %c%c\n", pCountryIe[0], pCountryIe[1]));

	if ((pCfg80211_CB->pCfg80211_Wdev == NULL) || (pCountryIe == NULL)) {
		CFG80211DBG(RT_DEBUG_ERROR, ("crda> regulatory domain hint not support!\n"));
		return;
	}

	/* hints a country IE as a regulatory domain "without" channel/power info. */
	regulatory_hint(pCfg80211_CB->pCfg80211_Wdev->wiphy, (const char *)pCountryIe);
}

/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from country element.

Arguments:
	pAdCB			- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
VOID CFG80211OS_RegHint11D(IN VOID *pCB, IN UCHAR *pCountryIe, IN ULONG CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */
}

BOOLEAN CFG80211OS_BandInfoGet(IN VOID *pCB,
			       IN VOID *pWiphyOrg, OUT VOID **ppBand24, OUT VOID **ppBand5)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return FALSE;

	*ppBand24 = pWiphy->bands[IEEE80211_BAND_2GHZ];
	*ppBand5 = pWiphy->bands[IEEE80211_BAND_5GHZ];
	return TRUE;
}

UINT32 CFG80211OS_ChanNumGet(IN VOID *pCB, IN VOID *pWiphyOrg, IN UINT32 IdBand)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return 0;

	if (pWiphy->bands[IdBand] != NULL)
		return pWiphy->bands[IdBand]->n_channels;

	return 0;
}

NDIS_STATUS CFG80211_UpdateChFlagsByBeacon(IN VOID *pAdCB, UCHAR channel)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdCB;
	struct wiphy *pWiphy = NULL;
	int freq = 0;
	struct ieee80211_channel *ch = NULL;
	int idx = 0;
	bool updated = false;
	struct _CH_FLAGS_BEACON *flags_updated;
	CFG80211_CB *cfg80211_cb = NULL;

	RTMP_DRIVER_80211_CB_GET(pAd, &cfg80211_cb);
	flags_updated = cfg80211_cb->ch_flags_by_beacon;
	pWiphy = pAd->net_dev->ieee80211_ptr->wiphy;

	if (pWiphy == NULL)
		goto fail1;

	if (channel > 14)
		freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_5GHZ);
	else
		freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);

	if (!freq)
		goto fail3;

	ch = ieee80211_get_channel(pWiphy, freq);

	if (!ch)
		goto fail5;

	if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN) {
		ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
		updated = true;
	}

	if  (ch->flags & IEEE80211_CHAN_RADAR) {
		ch->flags &= ~IEEE80211_CHAN_RADAR;
		updated = true;
	}

	for (idx = 0; idx < pAd->ChannelListNum; idx++) {
		if (pAd->ChannelList[idx].Channel == channel)
			break;
	}

	if (idx != pAd->ChannelListNum) {
		if (!(ch->flags & IEEE80211_CHAN_RADAR) &&
			pAd->ChannelList[idx].DfsReq)
			pAd->ChannelList[idx].DfsReq = FALSE;
		pAd->ChannelList[idx].Flags = (ch->flags & 0x3f);
		if (!(ch->flags & IEEE80211_CHAN_NO_HT40MINUS)
			|| !(ch->flags & IEEE80211_CHAN_NO_HT40PLUS))
				pAd->ChannelList[idx].Flags |= CHANNEL_40M_CAP;
		if (!(ch->flags & IEEE80211_CHAN_NO_80MHZ))
				pAd->ChannelList[idx].Flags |= CHANNEL_80M_CAP;
		pAd->ChannelList[idx].regFlags = ch->flags;
	}

	for (idx = 0; idx < sizeof(Cfg80211_Chan); idx++) {
		if (flags_updated[idx].ch == channel)
			break;
	}

	if (idx != pAd->ChannelListNum) {
		struct _CH_FLAGS_BEACON *info = &flags_updated[idx];
		if (info->flags & IEEE80211_CHAN_PASSIVE_SCAN) {
			CFG80211DBG(RT_DEBUG_TRACE, ("%s update PASSIVE_SCAN flags by beacon\n", __func__));
			info->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
		}
		if  (info->flags & IEEE80211_CHAN_RADAR) {
			CFG80211DBG(RT_DEBUG_TRACE, ("%s update CHAN_RADAR flags by beacon\n", __func__));
			info->flags &= ~IEEE80211_CHAN_RADAR;
		}
	} else
		CFG80211DBG(RT_DEBUG_TRACE, ("%s not match channel for ch %u\n", __func__, channel));

	if (!updated)
		return NDIS_STATUS_SUCCESS;
	pAd->flagsUpdated = TRUE;
	return NDIS_STATUS_SUCCESS;
fail1:
	CFG80211DBG(RT_DEBUG_ERROR, ("%s-fail1: wiphy NULL\n", __func__));
fail3:
	CFG80211DBG(RT_DEBUG_ERROR, ("%s-fail3: 2.4g channel list NULL\n"
		, __func__));
fail5:
	CFG80211DBG(RT_DEBUG_ERROR, ("%s-fail5: channelist NULL\n"
		, __func__));
	return NDIS_STATUS_FAILURE;
}

BOOLEAN CFG80211OS_ChanInfoGet(IN VOID *pCB,
			       IN VOID *pWiphyOrg,
			       IN UINT32 IdBand,
			       IN UINT32 IdChan,
			       OUT UINT32 *pChanId, OUT UINT32 *pPower, OUT BOOLEAN *pFlgIsRadar, UINT32 *chFlags)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;
	struct ieee80211_supported_band *pSband;
	struct ieee80211_channel *pChan;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return FALSE;

	pSband = pWiphy->bands[IdBand];
	pChan = &pSband->channels[IdChan];

	*pChanId = ieee80211_frequency_to_channel(pChan->center_freq);

	if (pChan->flags & IEEE80211_CHAN_DISABLED) {
		CFG80211DBG(RT_DEBUG_ERROR, ("Chan %03d (frq %d):\t not allowed!\n",
					     (*pChanId), pChan->center_freq));
		return FALSE;
	} else {
		CFG80211DBG(RT_DEBUG_TRACE, ("Chan %03d (frq %d):\t allowed, flags:%x\n",
					     (*pChanId), pChan->center_freq, pChan->flags));
	}

	*chFlags = pChan->flags;
	*pPower = pChan->max_power;

	if (pChan->flags & IEEE80211_CHAN_RADAR)
		*pFlgIsRadar = TRUE;
	else
		*pFlgIsRadar = FALSE;

	return TRUE;
}

/*
========================================================================
Routine Description:
	Initialize a channel information used in scan inform.

Arguments:

Return Value:
	TRUE		- Successful
	FALSE		- Fail

Note:
========================================================================
*/
BOOLEAN CFG80211OS_ChanInfoInit(IN VOID *pCB,
				IN UINT32 InfoIndex,
				IN UCHAR ChanId,
				IN UCHAR MaxTxPwr, IN BOOLEAN FlgIsNMode, IN BOOLEAN FlgIsBW20M)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct ieee80211_channel *pChan;

	if (InfoIndex >= MAX_NUM_OF_CHANNELS)
		return FALSE;

	pChan = (struct ieee80211_channel *)&(pCfg80211_CB->ChanInfo[InfoIndex]);
	memset(pChan, 0, sizeof(*pChan));

	if (ChanId > 14)
		pChan->band = IEEE80211_BAND_5GHZ;
	else
		pChan->band = IEEE80211_BAND_2GHZ;

	pChan->center_freq = ieee80211_channel_to_frequency(ChanId, pChan->band);

/* no use currently in 2.6.30 */
/*	if (ieee80211_is_beacon(((struct ieee80211_mgmt *)pFrame)->frame_control)) */
/*		pChan->beacon_found = 1; */
	return TRUE;
}

/*
========================================================================
Routine Description:
	Inform us that a scan is got.

Arguments:
	pAdCB				- WLAN control block pointer

Return Value:
	NONE

Note:
	Call RT_CFG80211_SCANNING_INFORM, not CFG80211_Scaning
========================================================================
*/
VOID CFG80211OS_Scaning(IN VOID *pCB,
			IN UINT32 ChanId,
			IN UCHAR *pFrame,
			IN UINT32 FrameLen, IN INT32 RSSI, IN BOOLEAN FlgIsNMode, IN UINT8 BW)
{
#ifdef CONFIG_STA_SUPPORT
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct ieee80211_supported_band *pBand;
	UINT32 IdChan;
	UINT32 CenFreq;
	UINT CurBand;
	struct wiphy *pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	struct cfg80211_bss *bss = NULL;
	struct ieee80211_mgmt *mgmt;
	mgmt = (struct ieee80211_mgmt *)pFrame;

	if (ChanId == 0)
		ChanId = 1;

	/* get channel information */
	if (ChanId > 14)
		CenFreq = ieee80211_channel_to_frequency(ChanId, IEEE80211_BAND_5GHZ);
	else
		CenFreq = ieee80211_channel_to_frequency(ChanId, IEEE80211_BAND_2GHZ);

	if (ChanId > 14)
		CurBand = IEEE80211_BAND_5GHZ;
	else
		CurBand = IEEE80211_BAND_2GHZ;

	pBand = &pCfg80211_CB->Cfg80211_bands[CurBand];

	for (IdChan = 0; IdChan < pBand->n_channels; IdChan++) {
		if (pBand->channels[IdChan].center_freq == CenFreq)
			break;
	}

	if (IdChan >= pBand->n_channels) {
		DBGPRINT(RT_DEBUG_ERROR, ("80211> Can not find any chan info! ==> %d[%d],[%d]\n",
					  ChanId, CenFreq, pBand->n_channels));
		return;
	}

	if (pWiphy->signal_type == CFG80211_SIGNAL_TYPE_MBM) {
		/* CFG80211_SIGNAL_TYPE_MBM: signal strength in mBm (100*dBm) */
		RSSI = RSSI * 100;
	}

	if (!mgmt->u.probe_resp.timestamp) {
		struct timeval tv;
		do_gettimeofday(&tv);
		mgmt->u.probe_resp.timestamp = ((UINT64) tv.tv_sec * 1000000) + tv.tv_usec;
	}

	/* inform 80211 a scan is got */
	/* we can use cfg80211_inform_bss in 2.6.31, it is easy more than the one */
	/* in cfg80211_inform_bss_frame(), it will memcpy pFrame but pChan */
	bss = cfg80211_inform_bss_frame(pWiphy, &pBand->channels[IdChan],
					mgmt, FrameLen, RSSI, GFP_ATOMIC);

	if (unlikely(!bss)) {
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> bss inform fail ==> %d\n", IdChan));
		return;
	}
	cfg80211_put_bss(pWiphy, bss);

#endif /* CONFIG_STA_SUPPORT */
}

/*
========================================================================
Routine Description:
	Inform us that scan ends.

Arguments:
	pAdCB			- WLAN control block pointer
	FlgIsAborted	- 1: scan is aborted

Return Value:
	NONE

Note:
========================================================================
*/
/* get RALINK pAd control block in 80211 Ops */
#define MAC80211_PAD_GET(__pAd, __pWiphy)                      \
	{                                                          \
		ULONG *__pPriv;                                        \
		__pPriv = (ULONG *)(wiphy_priv(__pWiphy));             \
		__pAd = (VOID *)(*__pPriv);                            \
		if (__pAd == NULL)                                     \
			DBGPRINT(RT_DEBUG_ERROR,                           \
					("80211> %s but pAd = NULL!", __func__));  \
	}

VOID CFG80211OS_ScanEnd(IN VOID *pCB, IN BOOLEAN FlgIsAborted)
{
#ifdef CONFIG_STA_SUPPORT
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	PRTMP_ADAPTER pAd;

	NdisAcquireSpinLock(&pCfg80211_CB->scan_notify_lock);

	MAC80211_PAD_GET(pAd, pCfg80211_CB->pCfg80211_Wdev->wiphy);

	if (pCfg80211_CB->pCfg80211_ScanReq) {
		CFG80211DBG(RT_DEBUG_TRACE, ("80211> cfg80211_scan_done\n"));
		cfg80211_scan_done(pCfg80211_CB->pCfg80211_ScanReq, FlgIsAborted);
		pCfg80211_CB->pCfg80211_ScanReq = NULL;
	} else {
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> cfg80211_scan_done ==> NULL\n"));
	}

	pAd->cfg80211_ctrl.FlgCfg80211Scanning = FALSE;

	NdisReleaseSpinLock(&pCfg80211_CB->scan_notify_lock);
#endif /* CONFIG_STA_SUPPORT */
}

/*
========================================================================
Routine Description:
	Inform CFG80211 about association status.

Arguments:
	pAdCB			- WLAN control block pointer
	pBSSID			- the BSSID of the AP
	pReqIe			- the element list in the association request frame
	ReqIeLen		- the request element length
	pRspIe			- the element list in the association response frame
	RspIeLen		- the response element length
	FlgIsSuccess	- 1: success; otherwise: fail

Return Value:
	None

Note:
========================================================================
*/
void CFG80211OS_ConnectResultInform(IN VOID *pCB,
				    IN UCHAR *pBSSID,
				    IN UCHAR *pReqIe,
				    IN UINT32 ReqIeLen,
				    IN UCHAR *pRspIe, IN UINT32 RspIeLen, IN UCHAR FlgIsSuccess)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;

	if ((pCfg80211_CB->pCfg80211_Wdev->netdev == NULL) || (pBSSID == NULL))
		return;

	if (FlgIsSuccess) {
		cfg80211_connect_result(pCfg80211_CB->pCfg80211_Wdev->netdev,
					pBSSID,
					pReqIe,
					ReqIeLen,
					pRspIe, RspIeLen, WLAN_STATUS_SUCCESS, GFP_KERNEL);
	} else {
		cfg80211_connect_result(pCfg80211_CB->pCfg80211_Wdev->netdev,
					pBSSID,
					NULL, 0, NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE, GFP_KERNEL);
	}
}

/* CFG_TODO: should be merge totoger */
void CFG80211OS_P2pClientConnectResultInform(IN PNET_DEV pNetDev,
					     IN UCHAR *pBSSID,
					     IN UCHAR *pReqIe,
					     IN UINT32 ReqIeLen,
					     IN UCHAR *pRspIe,
					     IN UINT32 RspIeLen, IN UCHAR FlgIsSuccess)
{
	if ((pNetDev == NULL) || (pBSSID == NULL)
		|| pNetDev->ieee80211_ptr->sme_state != CFG80211_SME_CONNECTING)
		DBGPRINT(RT_DEBUG_WARN, ("%s, not indicate! It will cause kernel warning\n", __func__));
	else {
		DBGPRINT(RT_DEBUG_OFF, ("%s, Connect Result = %d\n", __func__, FlgIsSuccess));
		if (FlgIsSuccess) {
			DBGPRINT(RT_DEBUG_TRACE, ("APCLI: ReqIeLen %d, RspIeLen, %d\n", ReqIeLen, RspIeLen));
			hex_dump("APCLI Req:", pReqIe, ReqIeLen);
			hex_dump("APCLI Rsp:", pRspIe, RspIeLen);
			cfg80211_connect_result(pNetDev,
						pBSSID,
						pReqIe,
						ReqIeLen,
						pRspIe, RspIeLen, WLAN_STATUS_SUCCESS, GFP_KERNEL);
		} else {
			cfg80211_connect_result(pNetDev,
						pBSSID,
						NULL, 0, NULL, 0,
						WLAN_STATUS_UNSPECIFIED_FAILURE, GFP_KERNEL);
		}
	}

}

BOOLEAN CFG80211OS_RxMgmt(IN PNET_DEV pNetDev, IN INT32 freq, IN PUCHAR frame, IN UINT32 len)
{
	return cfg80211_rx_mgmt(
				       pNetDev->ieee80211_ptr,
				       freq, 0,	/* CFG_TODO return 0 in dbm */
				       frame, len,
				       GFP_ATOMIC);

}

VOID CFG80211OS_TxStatus(IN PNET_DEV pNetDev, IN INT32 cookie, IN PUCHAR frame, IN UINT32 len,
			 IN BOOLEAN ack)
{
	DBGPRINT(RT_DEBUG_TRACE, ("%s, Tx Status = %d, cookie = 0x%x\n", __func__, ack, cookie));
	return cfg80211_mgmt_tx_status(
					      pNetDev->ieee80211_ptr,
					      cookie, frame, len, ack, GFP_ATOMIC);

}

VOID CFG80211OS_NewSta(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr, IN const PUCHAR assoc_frame,
		       IN UINT32 assoc_len)
{
	struct station_info sinfo;

	struct ieee80211_mgmt *mgmt;

	NdisZeroMemory(&sinfo, sizeof(sinfo));

/*KERNEL_VERSION 3,2,0*/
	sinfo.filled = STATION_INFO_ASSOC_REQ_IES;

	mgmt = (struct ieee80211_mgmt *)assoc_frame;
	sinfo.assoc_req_ies_len = assoc_len - 24 - 4;
	sinfo.assoc_req_ies = mgmt->u.assoc_req.variable;
	DBGPRINT(RT_DEBUG_OFF,
					 ("New Sta Associated %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(mac_addr)));
	return cfg80211_new_sta(pNetDev, mac_addr, &sinfo, GFP_ATOMIC);

}

VOID CFG80211OS_DelSta(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr)
{
	DBGPRINT(RT_DEBUG_OFF,
					 ("Sta Disassociated %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(mac_addr)));
	return cfg80211_del_sta(pNetDev, mac_addr, GFP_ATOMIC);
}

VOID CFG80211OS_MICFailReport(PNET_DEV pNetDev, const PUCHAR src_addr, BOOLEAN unicast, INT key_id,
			      const PUCHAR tsc)
{
	cfg80211_michael_mic_failure(pNetDev, src_addr,
				     (unicast ? NL80211_KEYTYPE_PAIRWISE : NL80211_KEYTYPE_GROUP),
				     key_id, tsc, GFP_ATOMIC);
}

VOID CFG80211OS_Roamed(PNET_DEV pNetDev, IN UCHAR *pBSSID,
		       IN UCHAR *pReqIe, IN UINT32 ReqIeLen, IN UCHAR *pRspIe, IN UINT32 RspIeLen)
{
	cfg80211_roamed(pNetDev,
			NULL,
			pBSSID, pReqIe, ReqIeLen, pRspIe, RspIeLen, GFP_KERNEL);
}

VOID CFG80211OS_RecvObssBeacon(VOID *pCB, const PUCHAR pFrame, INT frameLen, INT freq)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy;
	pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;	/* To avoid compile warning: unused variable @20140529 */

	cfg80211_report_obss_beacon(pWiphy, pFrame, frameLen, freq, 0 /*dbm */);
}

VOID CFG80211OS_ForceUpdateChanFlags(IN VOID *pCB,
				     IN UINT32 freq_start_mhz,
				     IN UINT32 freq_end_mhz, IN UINT32 flags)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct ieee80211_supported_band *pBand;
	struct ieee80211_channel *pChannels = NULL;
	INT32 idx = 0;

	if (!pCB || !freq_start_mhz || !freq_end_mhz)
		return;
	if (freq_start_mhz < 4000) {
		pBand = &pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_2GHZ];
	} else
		pBand = &pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_5GHZ];

	if (!pBand) {
		return;
	}
	pChannels = pBand->channels;

	if (!pChannels) {
		return;
	}
	/* Note ToDo: We only disable (NL80211_RRF_PASSIVE_SCAN &  NL80211_RRF_NO_IBSS) bits now */
	for (idx = 0; idx < pBand->n_channels; idx++) {
		if (!pChannels)
			break;
		if ((pChannels->center_freq >= (UINT16) freq_start_mhz) &&
		    (pChannels->center_freq <= (UINT16) freq_end_mhz)) {

			if (!(flags & NL80211_RRF_PASSIVE_SCAN))
				pChannels->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
			if (!(flags & NL80211_RRF_NO_IBSS))
				pChannels->flags &= ~IEEE80211_CHAN_NO_IBSS;
		}
		pChannels++;
	}

}

#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
VOID CFG80211OS_EnableChanFlagsByBand(IN struct ieee80211_channel *pChannels,
				      IN UINT32 n_channels,
				      IN UINT32 freq_start_mhz,
				      IN UINT32 freq_end_mhz, IN UINT32 flags)
{
	INT32 idx = 0;

	if (!pChannels) {
		return;
	}
	for (idx = 0; idx < n_channels; idx++) {
		if ((pChannels[idx].center_freq >= (UINT16) freq_start_mhz) &&
		    (pChannels[idx].center_freq <= (UINT16) freq_end_mhz)) {
			/* If this is not disabled channel, we clear the flag of IEEE80211_CHAN_DISABLED */
			if (!(flags & CHANNEL_DISABLED))
				pChannels[idx].flags &= ~IEEE80211_CHAN_DISABLED;
			if (flags & CHANNEL_PASSIVE_SCAN)
				pChannels[idx].flags |= IEEE80211_CHAN_PASSIVE_SCAN;
			if (flags & CHANNEL_NO_IBSS)
				pChannels[idx].flags |= IEEE80211_CHAN_NO_IBSS;
			if (flags & CHANNEL_RADAR)
				pChannels[idx].flags |= IEEE80211_CHAN_RADAR;
		}

	}

}
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */

VOID CFG80211OS_ForceUpdateChanFlagsByBand(IN struct ieee80211_supported_band *pBand,
					   IN struct ieee80211_channel *pChannelUpdate)
{
	struct ieee80211_channel *pChannels;
	INT32 idx = 0;

	if (!pBand || !pChannelUpdate)
		return;

	pChannels = pBand->channels;

	for (idx = 0; idx < pBand->n_channels; idx++) {
		pChannels[idx].flags = pChannelUpdate[idx].flags;
	}
}

INT32 CFG80211OS_UpdateRegRuleByRegionIdx(IN VOID *pCB, IN VOID *pChDesc2G, IN VOID *pChDesc5G)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = NULL;
	struct ieee80211_regdomain *reg_ptr = NULL;
	INT32 size_of_regd = 0, reg_rules_n = 0;
	UINT32 freq_start_mhz = 0, freq_end_mhz = 0;
	PCH_DESC pChDesc = NULL;
	INT32 n_channels = 0;

#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
	INT32 ii = 0;
#else
	UINT32 flags = 0;
	INT32 rule_idx = 0;
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */

	if (!pCB || (!pChDesc2G && !pChDesc5G))
		return -EINVAL;
	pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	if (!pWiphy) {
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> %s: invalid pWiphy!!\n", __func__));
	}
	/* first, calculate the number of reg_rules */
	if (pChDesc2G)
		reg_rules_n += TotalRuleNum((PCH_DESC) pChDesc2G);
	if (pChDesc5G)
		reg_rules_n += TotalRuleNum((PCH_DESC) pChDesc5G);

	CFG80211DBG(RT_DEBUG_TRACE, ("80211> %s: TotalRuleNum=%d\n", __func__, reg_rules_n));

	size_of_regd = sizeof(struct ieee80211_regdomain) +
	    ((reg_rules_n + 1) * sizeof(struct ieee80211_reg_rule));

	reg_ptr = kzalloc(size_of_regd, GFP_KERNEL);
	if (!reg_ptr)
		return -ENOMEM;
	reg_ptr->n_reg_rules = reg_rules_n;
	reg_ptr->alpha2[0] = '9';	/* just give meaningless alphabetics here */
	reg_ptr->alpha2[1] = '9';
	/* 2GHz rules */
	pChDesc = (PCH_DESC) pChDesc2G;
	n_channels = pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_2GHZ].n_channels;
	if (pChDesc && n_channels > 0) {
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
		struct ieee80211_channel pTmpCh[n_channels];
		memset(pTmpCh, 0, sizeof(pTmpCh));
		/* init all channels to be disabled */
		for (ii = 0; ii < n_channels; ii++) {
			pTmpCh[ii].flags |= IEEE80211_CHAN_DISABLED;
			pTmpCh[ii].center_freq =
			    pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_2GHZ].channels[ii].
			    center_freq;
		}
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
		while (pChDesc && pChDesc->FirstChannel) {
			freq_start_mhz =
			    ieee80211_channel_to_frequency(pChDesc->FirstChannel,
							   IEEE80211_BAND_2GHZ);
			freq_end_mhz =
			    ieee80211_channel_to_frequency(pChDesc->FirstChannel +
							   (pChDesc->NumOfCh - 1),
							   IEEE80211_BAND_2GHZ);
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
			CFG80211OS_EnableChanFlagsByBand(pTmpCh, n_channels, freq_start_mhz,
							 freq_end_mhz,
							 (UINT32) pChDesc->ChannelProp);
#else /* ! FORCE_CUSTOMIZED_COUNTRY_REGION */
			flags = 0;
			if (pChDesc->ChannelProp & CHANNEL_PASSIVE_SCAN)
				flags |= NL80211_RRF_PASSIVE_SCAN;
			if (pChDesc->ChannelProp & CHANNEL_NO_IBSS)
				flags |= NL80211_RRF_NO_IBSS;
			if (pChDesc->ChannelProp & CHANNEL_RADAR)
				flags |= NL80211_RRF_DFS;
			RT_REG_RULE(reg_ptr->reg_rules[rule_idx], freq_start_mhz - 10,	/* start freq */
				    freq_end_mhz + 10,	/* end freq */
				    20,	/* bw */
				    6,	/* antenna gain */
				    20,	/* eirp */
				    flags	/* flags */
			    );
			/*
			   CFG80211DBG(RT_DEBUG_TRACE, ("rule_idx=%d, srart_freq=%u, end_freq=%d, bw=%d flags=%d\n",
			   rule_idx, reg_ptr->reg_rules[rule_idx].freq_range.start_freq_khz,
			   reg_ptr->reg_rules[rule_idx].freq_range.end_freq_khz,
			   reg_ptr->reg_rules[rule_idx].freq_range.max_bandwidth_khz,
			   reg_ptr->reg_rules[rule_idx].flags));
			 */
			CFG80211OS_ForceUpdateChanFlags(pCB, freq_start_mhz, freq_end_mhz, flags);
			rule_idx++;
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
			pChDesc++;
		}
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
		CFG80211OS_ForceUpdateChanFlagsByBand(&pCfg80211_CB->Cfg80211_bands
						      [IEEE80211_BAND_2GHZ], pTmpCh);
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
	}
	/* 5GHz rules */
	pChDesc = (PCH_DESC) pChDesc5G;
	n_channels = pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_5GHZ].n_channels;
	if (pChDesc && n_channels > 0) {
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
		struct ieee80211_channel pTmpCh2[n_channels];
		memset(pTmpCh2, 0, sizeof(pTmpCh2));
		/* init all channels to be disabled */
		for (ii = 0; ii < n_channels; ii++) {
			pTmpCh2[ii].flags |= IEEE80211_CHAN_DISABLED;
			pTmpCh2[ii].center_freq =
			    pCfg80211_CB->Cfg80211_bands[IEEE80211_BAND_5GHZ].channels[ii].
			    center_freq;
		}
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
		while (pChDesc && pChDesc->FirstChannel) {
			freq_start_mhz =
			    ieee80211_channel_to_frequency(pChDesc->FirstChannel,
							   IEEE80211_BAND_5GHZ);
			freq_end_mhz =
			    ieee80211_channel_to_frequency(pChDesc->FirstChannel +
							   ((pChDesc->NumOfCh - 1) * 4),
							   IEEE80211_BAND_5GHZ);
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
			CFG80211OS_EnableChanFlagsByBand(pTmpCh2, n_channels, freq_start_mhz,
							 freq_end_mhz,
							 (UINT32) pChDesc->ChannelProp);
#else /* ! FORCE_CUSTOMIZED_COUNTRY_REGION */
			flags = 0;
			if (pChDesc->ChannelProp & CHANNEL_PASSIVE_SCAN)
				flags |= NL80211_RRF_PASSIVE_SCAN;
			if (pChDesc->ChannelProp & CHANNEL_NO_IBSS)
				flags |= NL80211_RRF_NO_IBSS;
			if (pChDesc->ChannelProp & CHANNEL_RADAR)
				flags |= NL80211_RRF_DFS;
			RT_REG_RULE(reg_ptr->reg_rules[rule_idx], freq_start_mhz - 10,	/* start freq */
				    freq_end_mhz + 10,	/* end freq */
				    40,	/* bw */
				    6,	/* antenna gain */
				    20,	/* eirp */
				    flags	/* flags */
			    );
			/*
			   CFG80211DBG(RT_DEBUG_TRACE, ("rule_idx=%d, srart_freq=%u, end_freq=%d, bw=%d flags=%d\n",
			   rule_idx, reg_ptr->reg_rules[rule_idx].freq_range.start_freq_khz,
			   reg_ptr->reg_rules[rule_idx].freq_range.end_freq_khz,
			   reg_ptr->reg_rules[rule_idx].freq_range.max_bandwidth_khz,
			   reg_ptr->reg_rules[rule_idx].flags));
			 */
			CFG80211OS_ForceUpdateChanFlags(pCB, freq_start_mhz, freq_end_mhz, flags);
			rule_idx++;
#endif /* CUSTOMIZED_COUNTRY_REGION_CE_1 */
			pChDesc++;
		}
#ifdef FORCE_CUSTOMIZED_COUNTRY_REGION
		CFG80211OS_ForceUpdateChanFlagsByBand(&pCfg80211_CB->Cfg80211_bands
						      [IEEE80211_BAND_5GHZ], pTmpCh2);
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */
	}
#ifndef FORCE_CUSTOMIZED_COUNTRY_REGION
	ASSERT(rule_idx == reg_rules_n);
	    /* pWiphy->flags |= (WIPHY_FLAG_STRICT_REGULATORY | WIPHY_FLAG_CUSTOM_REGULATORY); */
	wiphy_apply_custom_regulatory(pWiphy, reg_ptr);
#endif /* FORCE_CUSTOMIZED_COUNTRY_REGION */

	kfree(reg_ptr);
	return 0;
}

INT32 CFG80211OS_ReadyOnChannel(IN VOID *pAdOrg, IN VOID *pChInfo, IN UINT32 duration)
{
	CMD_RTPRIV_IOCTL_80211_CHAN *pChanInfo = pChInfo;

	cfg80211_ready_on_channel(pChanInfo->pWdev, pChanInfo->cookie, pChanInfo->chan, duration,
				  GFP_ATOMIC);

	return 0;
}

INT32 CFG80211OS_RemainOnChannelExpired(IN VOID *pAdOrg, IN VOID *pCtrl)
{
	PCFG80211_CTRL pCfg80211_ctrl = (CFG80211_CTRL *) pCtrl;

	cfg80211_remain_on_channel_expired(pCfg80211_ctrl->Cfg80211ChanInfo.pWdev,
					   pCfg80211_ctrl->Cfg80211ChanInfo.cookie,
					   pCfg80211_ctrl->Cfg80211ChanInfo.chan, GFP_ATOMIC);

	return 0;
}

#endif /* RT_CFG80211_SUPPORT */
