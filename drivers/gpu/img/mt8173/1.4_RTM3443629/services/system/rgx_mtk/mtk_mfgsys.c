/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/module.h>
#include <linux/sched.h>
#include "mtk_mfgsys.h"
#include <dt-bindings/clock/mt8173-clk.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpufreq.h>
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/bug.h>

#include <trace/events/mtk_events.h>
#include <linux/mtk_gpu_utility.h>

/* #define MTK_CAL_POWER_INDEX */

#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50//16//100
#define MTK_SYS_BOOST_DURATION_MS       50
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000
#define MTK_GPIO_REG_OFFSET             0x30
#define MTK_GPU_FREQ_ID_INVALID         0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID    -1

static IMG_HANDLE g_hDVFSTimer = NULL;
static POS_LOCK ghDVFSLock = NULL;

static IMG_PVOID g_pvRegsKM = NULL;
#ifdef MTK_CAL_POWER_INDEX
static IMG_PVOID g_pvRegsBaseKM = NULL;
#endif

static IMG_BOOL g_bExit = IMG_TRUE;
static IMG_INT32 g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 g_cust_boost_freq_id;
static IMG_UINT32 gpu_cust_boost_freq;
static IMG_UINT32 g_cust_upbound_freq_id;
static IMG_UINT32 gpu_cust_upbound_freq;

static IMG_UINT32 gpu_power = 0;
static IMG_UINT32 gpu_current = 0;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 boost_gpu_enable;
static IMG_UINT32 gpu_debug_enable;
static IMG_UINT32 gpu_dvfs_force_idle = 0;
static IMG_UINT32 gpu_dvfs_cb_force_idle = 0;

static IMG_UINT32 gpu_pre_loading = 0;
static IMG_UINT32 gpu_loading = 0;
static IMG_UINT32 gpu_block = 0;
static IMG_UINT32 gpu_idle = 0;
static IMG_UINT32 gpu_freq = 0;

static struct clk *g_mfgclk_power;
static struct clk *g_mfgclk_axi;
static struct clk *g_mfgclk_mem;
static struct clk *g_mfgclk_g3d;
static struct clk *g_mfgclk_26m;



static PVRSRV_DEVICE_NODE* MTKGetRGXDevNode(IMG_VOID)
{
    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    IMG_UINT32 i;
    for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
    {
        PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
        if (psDeviceNode && psDeviceNode->psDevConfig &&
            psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
        {
            return psDeviceNode;
        }
    }
    return NULL;
}

static IMG_UINT32 MTKGetRGXDevIdx(IMG_VOID)
{
    static IMG_UINT32 ms_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
    if (MTK_RGX_DEVICE_INDEX_INVALID == ms_ui32RGXDevIdx)
    {
        PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
        IMG_UINT32 i;
        for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
        {
            PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
            if (psDeviceNode && psDeviceNode->psDevConfig &&
                psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
            {
                ms_ui32RGXDevIdx = i;
                break;
            }
        }
    }
    return ms_ui32RGXDevIdx;
}

static IMG_VOID MTKWriteBackFreqToRGX(IMG_UINT32 ui32DeviceIndex, IMG_UINT32 ui32NewFreq)
{
    PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
    PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[ui32DeviceIndex];
    RGX_DATA* psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
    psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000; /* kHz to Hz write to RGX as the same unit */
}

static void MTKEnableMfgClock(void)
{
    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKEnableMfgClock"));
    }

    mt_gpufreq_voltage_enable_set(1);

    clk_prepare(g_mfgclk_power);
    clk_enable(g_mfgclk_power);

    clk_prepare(g_mfgclk_axi);
    clk_enable(g_mfgclk_axi);

    clk_prepare(g_mfgclk_mem);
    clk_enable(g_mfgclk_mem);

    clk_prepare(g_mfgclk_g3d);
    clk_enable(g_mfgclk_g3d);

    clk_prepare(g_mfgclk_26m);
    clk_enable(g_mfgclk_26m);
}


static void MTKDisableMfgClock(void)
{
    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKDisableMfgClock"));
    }

    clk_disable(g_mfgclk_26m);
    clk_unprepare(g_mfgclk_26m);

    clk_disable(g_mfgclk_g3d);
    clk_unprepare(g_mfgclk_g3d);

    clk_disable(g_mfgclk_mem);

    clk_unprepare(g_mfgclk_mem);

    clk_disable(g_mfgclk_axi);
    clk_unprepare(g_mfgclk_axi);

    clk_disable(g_mfgclk_power);
    clk_unprepare(g_mfgclk_power);

    mt_gpufreq_voltage_enable_set(0);
}


int MTKMFGGetClocks(struct platform_device *pdev)
{
    int ret = 0;

    g_mfgclk_power = devm_clk_get(&pdev->dev, "MT_CG_MFG_POWER");
    g_mfgclk_axi = devm_clk_get(&pdev->dev, "MT_CG_MFG_AXI");
    g_mfgclk_mem = devm_clk_get(&pdev->dev, "MT_CG_MFG_MEM");
    g_mfgclk_g3d = devm_clk_get(&pdev->dev, "MT_CG_MFG_G3D");
    g_mfgclk_26m = devm_clk_get(&pdev->dev, "MT_CG_MFG_26M");

    if (IS_ERR(g_mfgclk_power)) {
        ret = PTR_ERR(g_mfgclk_power);
        dev_err(&pdev->dev, "Failed to request g_mfgclk_power: %d\n", ret);
        return ret;
    }

    if (IS_ERR(g_mfgclk_axi)) {
        ret = PTR_ERR(g_mfgclk_axi);
        dev_err(&pdev->dev, "Failed to request g_mfgclk_axi: %d\n", ret);
        return ret;
    }

    if (IS_ERR(g_mfgclk_mem)) {
        ret = PTR_ERR(g_mfgclk_mem);
        dev_err(&pdev->dev, "Failed to request g_mfgclk_mem: %d\n", ret);
        return ret;
    }

    if (IS_ERR(g_mfgclk_g3d)) {
        ret = PTR_ERR(g_mfgclk_g3d);
        dev_err(&pdev->dev, "Failed to request g_mfgclk_g3d: %d\n", ret);
        return ret;
    }

    if (IS_ERR(g_mfgclk_26m)) {
        ret = PTR_ERR(g_mfgclk_26m);
        dev_err(&pdev->dev, "Failed to request g_mfgclk_26m: %d\n", ret);
        return ret;
    }
    return 0;
}


#if  defined(MTK_USE_HW_APM)
static int MTKInitHWAPM(void)
{
#if 0 //disable HW APM
    if (!g_pvRegsKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            IMG_CPU_PHYADDR sRegsPBase;
            PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
            PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
            if (psDevInfo)
            {
                PVR_DPF((PVR_DBG_ERROR, "psDevInfo->pvRegsBaseKM: %p", psDevInfo->pvRegsBaseKM));
            }
            if (psDevConfig)
            {
                sRegsPBase = psDevConfig->sRegsCpuPBase;
                sRegsPBase.uiAddr += 0xfff000;
                PVR_DPF((PVR_DBG_ERROR, "sRegsCpuPBase.uiAddr: 0x%08lx", (unsigned long)psDevConfig->sRegsCpuPBase.uiAddr));
                PVR_DPF((PVR_DBG_ERROR, "sRegsPBase.uiAddr: 0x%08lx", (unsigned long)sRegsPBase.uiAddr));
                g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
            }
        }
    }
#endif
    if (g_pvRegsKM)
    {
	DRV_WriteReg32(g_pvRegsKM + 0x24, 0x004a3d4d);
	DRV_WriteReg32(g_pvRegsKM + 0x28, 0x4d45520b);
	DRV_WriteReg32(g_pvRegsKM + 0xe0, 0x7a710184);
	DRV_WriteReg32(g_pvRegsKM + 0xe4, 0x835f6856);
	DRV_WriteReg32(g_pvRegsKM + 0xe8, 0x00470248);
	DRV_WriteReg32(g_pvRegsKM + 0xec, 0x80000000);
	DRV_WriteReg32(g_pvRegsKM + 0xa0, 0x08000000);
    }
    else
    {
        PVR_DPF((PVR_DBG_ERROR, "%s g_pvRegsKM is NULL", __FUNCTION__));
    }
	return PVRSRV_OK;
}

static int MTKDeInitHWAPM(void)
{
#if 0
    if (g_pvRegsKM)
    {
	    DRV_WriteReg32(g_pvRegsKM + 0x24, 0x0);
    	DRV_WriteReg32(g_pvRegsKM + 0x28, 0x0);
    }
#endif
	return PVRSRV_OK;
}
#endif

static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
    PVRSRV_ERROR eResult;
    IMG_UINT32 ui32RGXDevIdx;

    if (mt_gpufreq_dvfs_ready() == false)
    {
        return IMG_FALSE;
    }

    // bottom bound
    if (ui32NewFreqID > g_bottom_freq_id)
    {
        ui32NewFreqID = g_bottom_freq_id;
    }
    if (ui32NewFreqID > g_cust_boost_freq_id)
    {
        ui32NewFreqID = g_cust_boost_freq_id;
    }

    // up bound
    if (ui32NewFreqID < g_cust_upbound_freq_id)
    {
        ui32NewFreqID = g_cust_upbound_freq_id;
    }

    // thermal power limit
    if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
    {
        ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();
    }

    // no change
    if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
    {
        return IMG_FALSE;
    }

    ui32RGXDevIdx = MTKGetRGXDevIdx();
    if (MTK_RGX_DEVICE_INDEX_INVALID == ui32RGXDevIdx)
    {
        return IMG_FALSE;
    }

    eResult = PVRSRVDevicePreClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID*)NULL);
    if ((PVRSRV_OK == eResult) || (PVRSRV_ERROR_RETRY == eResult))
    {
        unsigned int ui32GPUFreq;
        unsigned int ui32CurFreqID;
        PVRSRV_DEV_POWER_STATE ePowerState;

        PVRSRVGetDevicePowerState(ui32RGXDevIdx, &ePowerState);
        if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
        {
            MTKEnableMfgClock();
        }

        mt_gpufreq_target(ui32NewFreqID);
        ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
        ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
        gpu_freq = ui32GPUFreq;
        MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);

        if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
        {
            MTKDisableMfgClock();
        }

        if (PVRSRV_OK == eResult)
        {
            PVRSRVDevicePostClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID*)NULL);
        }

        return IMG_TRUE;
    }

    return IMG_FALSE;
}

static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
    if (0 < g_iSkipCount)
    {
        return;
    }

    if (boost_gpu_enable == 0)
    {
        return;
    }

    OSLockAcquire(ghDVFSLock);

    if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index())
    {
        if (MTKDoGpuDVFS(ui32BoostFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
        {
            g_sys_dvfs_time_ms = OSClockms();
        }
    }

    OSLockRelease(ghDVFSLock);

}

static void MTKFreqPowerLimitCB(unsigned int ui32LimitFreqID)
{
    if (0 < g_iSkipCount)
    {
        return;
    }

    OSLockAcquire(ghDVFSLock);

    if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index())
    {
        if (MTKDoGpuDVFS(ui32LimitFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
        {
            g_sys_dvfs_time_ms = OSClockms();
        }
    }

    OSLockRelease(ghDVFSLock);
}

#ifdef MTK_CAL_POWER_INDEX
static IMG_VOID MTKStartPowerIndex(IMG_VOID)
{
    if (!g_pvRegsBaseKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            PVRSRV_RGXDEV_INFO* psDevInfo = psDevNode->pvDevice;
            if (psDevInfo)
            {
                g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
            }
        }
    }
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
    }
}

static IMG_VOID MTKReStartPowerIndex(IMG_VOID)
{
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
    }
}

static IMG_VOID MTKStopPowerIndex(IMG_VOID)
{
    if (g_pvRegsBaseKM)
    {
        DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
    }
}

static IMG_UINT32 MTKCalPowerIndex(IMG_VOID)
{
    IMG_UINT32 ui32State, ui32Result;
    PVRSRV_DEV_POWER_STATE  ePowerState;
    IMG_BOOL bTimeout;
    IMG_UINT32 u32Deadline;
    IMG_PVOID pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
    IMG_PVOID pvPOWER_ESTIMATE_RESULT;

    if (!g_pvRegsBaseKM && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
    {
        MTKStartPowerIndex();
    }

    pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)0x6328;


    if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
    {
        return 0;
    }

    if (PVRSRVPowerLock() != PVRSRV_OK)
    {
        return 0;
    }

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
    if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
	{
		PVRSRVPowerUnlock();
		return 0;
	}

    //writes 1 to GPIO_INPUT_REQ, bit[0]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

    //wait for 1 in GPIO_OUTPUT_REQ, bit[16]
    bTimeout = IMG_TRUE;
    u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
    while(OSClockus() < u32Deadline)
    {
        if (0x10000 & DRV_Reg32(pvGPIO_REG))
        {
            bTimeout = IMG_FALSE;
            break;
        }
    }

    //writes 0 to GPIO_INPUT_REQ, bit[0]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
    if (bTimeout)
    {
        PVRSRVPowerUnlock();
        return 0;
    }

    //read GPIO_OUTPUT_DATA, bit[24]
    ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

    //read POWER_ESTIMATE_RESULT
    ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

    //writes 1 to GPIO_OUTPUT_ACK, bit[17]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

    //wait for 0 in GPIO_OUTPUT_REG, bit[16]
    bTimeout = IMG_TRUE;
    u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
    while(OSClockus() < u32Deadline)
    {
        if (!(0x10000 & DRV_Reg32(pvGPIO_REG)))
        {
            bTimeout = IMG_FALSE;
            break;
        }
    }

    //writes 0 to GPIO_OUTPUT_ACK, bit[17]
    DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
    if (bTimeout)
    {
        PVRSRVPowerUnlock();
        return 0;
    }

    MTKReStartPowerIndex();

    PVRSRVPowerUnlock();

    return (1 == ui32State) ? ui32Result : 0;
}
#endif

static IMG_VOID MTKCalGpuLoading(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle)
{
    PVRSRV_DEVICE_NODE* psDevNode;
    PVRSRV_RGXDEV_INFO* psDevInfo;

    psDevNode = MTKGetRGXDevNode();
    if (!psDevNode)
    {
        return;
    }
    psDevInfo = psDevNode->pvDevice;
    if (psDevInfo && psDevInfo->pfnGetGpuUtilStats)
    {
        RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};
        sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode);
        if (sGpuUtilStats.bValid)
        {
#if 0
            PVR_DPF((PVR_DBG_ERROR,"Loading: A(%d), I(%d), B(%d)",
                sGpuUtilStats.ui32GpuStatActive, sGpuUtilStats.ui32GpuStatIdle, sGpuUtilStats.ui32GpuStatBlocked));
#endif

            *pui32Loading = sGpuUtilStats.ui32GpuStatActiveHigh/100;
            *pui32Block = sGpuUtilStats.ui32GpuStatBlocked/100;
            *pui32Idle = sGpuUtilStats.ui32GpuStatIdle/100;
        }
    }
}

static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int* pui32NewFreqID)
{
    int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
    int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
    int i32NewFreqID = i32CurFreqID;

    if (ui32GPULoading >= 99)
    {
        i32NewFreqID = 0;
    }
    else if (ui32GPULoading <= 1)
    {
        i32NewFreqID = i32MaxLevel;
    }
    else if (ui32GPULoading >= 85)
    {
        i32NewFreqID -= 2;
    }
    else if (ui32GPULoading <= 30)
    {
        i32NewFreqID += 2;
    }
    else if (ui32GPULoading >= 70)
    {
        i32NewFreqID -= 1;
    }
    else if (ui32GPULoading <= 50)
    {
        i32NewFreqID += 1;
    }

    if (i32NewFreqID < i32CurFreqID)
    {
        if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
        {
            i32NewFreqID -= 1;
        }
    }
    else if (i32NewFreqID > i32CurFreqID)
    {
        if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
        {
            i32NewFreqID += 1;
        }
    }

    if (i32NewFreqID > i32MaxLevel)
    {
        i32NewFreqID = i32MaxLevel;
    }
    else if (i32NewFreqID < 0)
    {
        i32NewFreqID = 0;
    }

    if (i32NewFreqID != i32CurFreqID)
    {
        
        *pui32NewFreqID = (unsigned int)i32NewFreqID;
        return IMG_TRUE;
    }
    
    return IMG_FALSE;
}

static IMG_VOID MTKDVFSTimerFuncCB(IMG_PVOID pvData)
{
    IMG_UINT32 x1, x2, x3, y1, y2, y3;

    int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
    int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();

    x1=10000;
    x2=30000;
    x3=50000;
    y1=50;
    y2=430;
    y3=750;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKDVFSTimerFuncCB"));
    }

    if (0 == gpu_dvfs_enable)
    {
        gpu_power = 0;
        gpu_current = 0;
        gpu_loading = 0;
        gpu_block= 0;
        gpu_idle = 0;
        return;
    }

    if (g_iSkipCount > 0)
    {
        gpu_power = 0;
        gpu_current = 0;
        gpu_loading = 0;
        gpu_block= 0;
        gpu_idle = 0;
        g_iSkipCount -= 1;
    }
    else if ((!g_bExit) || (i32CurFreqID < i32MaxLevel))
    {
        IMG_UINT32 ui32NewFreqID;

        // calculate power index
#ifdef MTK_CAL_POWER_INDEX
        gpu_power = MTKCalPowerIndex();
        //mapping power index to power current
        if (gpu_power < x1)
        {
            gpu_current = y1;
        }
        else if (gpu_power < x2)
        {
            gpu_current = y1+((gpu_power-x1)*(y2-y1)/(x2-x1));
        }
        else if (gpu_power < x3)
        {
            gpu_current = y2+((gpu_power-x2)*(y3-y2) / (x3-x2));
        }
        else
        {
            gpu_current =y3;
        }

#else
        gpu_power = 0;
	gpu_current = 0;
#endif

        MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);

        OSLockAcquire(ghDVFSLock);

        // check system boost duration
        if ((g_sys_dvfs_time_ms > 0) && (OSClockms() - g_sys_dvfs_time_ms < MTK_SYS_BOOST_DURATION_MS))
        {
            OSLockRelease(ghDVFSLock);
            return;
        }
        else
        {
            g_sys_dvfs_time_ms = 0;
        }

        // do gpu dvfs
        if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
        {
            MTKDoGpuDVFS(ui32NewFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
        }

        gpu_pre_loading = gpu_loading;

        OSLockRelease(ghDVFSLock);
    }
}

void MTKMFGEnableDVFSTimer(bool bEnable)
{
	/* OSEnableTimer() and OSDisableTimer() should be called sequentially, following call will lead to assertion.
	   OSEnableTimer();
	   OSEnableTimer();
	   OSDisableTimer();
	   ...
	   bPreEnable is to avoid such scenario */
	static bool bPreEnable = false;

	if (gpu_debug_enable)
	{
		PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: %s (%s)",
			bEnable ? "yes" : "no", bPreEnable ? "yes" : "no"));
	}

	if (g_hDVFSTimer)
	{
		if (bEnable == true && bPreEnable == false)
		{
			OSEnableTimer(g_hDVFSTimer);
			bPreEnable = true;
		}
		else if (bEnable == false && bPreEnable == true)
		{
			OSDisableTimer(g_hDVFSTimer);
			bPreEnable = false;
		}
		else if (gpu_debug_enable)
		{
			BUG();
		}
	}
}

PVRSRV_ERROR MTKDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState )
    {
        if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
        {
            g_bExit = IMG_TRUE;

#ifdef MTK_CAL_POWER_INDEX
            MTKStopPowerIndex();
#endif
        }

#if  defined(MTK_USE_HW_APM)
        MTKDeInitHWAPM();
#endif
        MTKDisableMfgClock();

        gpu_loading = 0;
        gpu_block = 0;
        gpu_idle = 0;
        gpu_power = 0;
    }

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced)
{
    if( PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState &&
        PVRSRV_DEV_POWER_STATE_ON == eNewPowerState)
    {
        MTKEnableMfgClock();

#if defined(MTK_USE_HW_APM)
        MTKInitHWAPM();
#endif
        if (g_hDVFSTimer && PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
        {
#ifdef MTK_CAL_POWER_INDEX
            MTKStartPowerIndex();
#endif
            g_bExit = IMG_FALSE;
        }
#if 0
        if (g_iSkipCount > 0)
        {
            // During boot up
            unsigned int ui32NewFreqID = mt_gpufreq_get_dvfs_table_num() - 1;
            unsigned int ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
            if (ui32NewFreqID != ui32CurFreqID)
            {
                IMG_UINT32 ui32RGXDevIdx = MTKGetRGXDevIdx();
                unsigned int ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32NewFreqID);
                mt_gpufreq_target(ui32NewFreqID);
                MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);
            }
        }
#endif
    }

    return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
    {
    }

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if(PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{
    }

	return PVRSRV_OK;
}

static void MTKBoostGpuFreq(void)
{
    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKBoostGpuFreq"));
    }
    MTKFreqInputBoostCB(0);
}

static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
    unsigned int ui32MaxLevel;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKSetBottomGPUFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

    if (g_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_bottom_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }
     
    OSLockRelease(ghDVFSLock);

}

static unsigned int MTKCustomGetGpuFreqLevelCount(void)
{
    return mt_gpufreq_get_dvfs_table_num();
}

static void MTKCustomBoostGpuFreq(unsigned int ui32FreqLevel)
{
    unsigned int ui32MaxLevel;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKCustomBoostGpuFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_cust_boost_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

    if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_cust_boost_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }

    OSLockRelease(ghDVFSLock);
}

static void MTKCustomUpBoundGpuFreq(unsigned int ui32FreqLevel)
{
    unsigned int ui32MaxLevel;

    if (gpu_debug_enable)
    {
        PVR_DPF((PVR_DBG_ERROR, "MTKCustomUpBoundGpuFreq: freq = %d", ui32FreqLevel));
    }

    ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    if (ui32MaxLevel < ui32FreqLevel)
    {
        ui32FreqLevel = ui32MaxLevel;
    }

    OSLockAcquire(ghDVFSLock);

    // 0 => The highest frequency
    // table_num - 1 => The lowest frequency
    g_cust_upbound_freq_id = ui32MaxLevel - ui32FreqLevel;
    gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

    if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
    {
        MTKDoGpuDVFS(g_cust_upbound_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);
    }
     
    OSLockRelease(ghDVFSLock);
}

static unsigned int MTKGetCustomBoostGpuFreq(void)
{
    unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    return ui32MaxLevel - g_cust_boost_freq_id;
}

static unsigned int MTKGetCustomUpBoundGpuFreq(void)
{
    unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
    return ui32MaxLevel - g_cust_upbound_freq_id;
}

static IMG_UINT32 MTKGetGpuLoading(IMG_VOID)
{
    return gpu_loading;
}

static IMG_UINT32 MTKGetGpuBlock(IMG_VOID)
{
    return gpu_block;
}

static IMG_UINT32 MTKGetGpuIdle(IMG_VOID)
{
    return gpu_idle;
}

static IMG_UINT32 MTKGetPowerIndex(IMG_VOID)
{
    return gpu_current;    //gpu_power;
}


PVRSRV_ERROR MTKMFGSystemInit(void)
{
    PVRSRV_ERROR error;

	error = OSLockCreate(&ghDVFSLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK)
    {
        PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
        return error;
    }

    g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

    g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);
    if(!g_hDVFSTimer)
    {
        OSLockDestroy(ghDVFSLock);
    	PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
        return PVRSRV_ERROR_OUT_OF_MEMORY;
    }

    MTKMFGEnableDVFSTimer(true);

#ifdef MTK_GPU_DVFS
    gpu_dvfs_enable = 1;
#else
    gpu_dvfs_enable = 0;
#endif
    
    boost_gpu_enable = 1;

    g_sys_dvfs_time_ms = 0;

    g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
    gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

    g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
    gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

    g_cust_upbound_freq_id = 0;
    gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

    gpu_debug_enable = 0;

    mt_gpufreq_mfgclock_notify_registerCB(MTKEnableMfgClock, MTKDisableMfgClock);

    mt_gpufreq_input_boost_notify_registerCB(MTKFreqInputBoostCB);
    mt_gpufreq_power_limit_notify_registerCB(MTKFreqPowerLimitCB);

    mtk_boost_gpu_freq_fp = MTKBoostGpuFreq;

    mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;

    mtk_custom_get_gpu_freq_level_count_fp = MTKCustomGetGpuFreqLevelCount;

    mtk_custom_boost_gpu_freq_fp = MTKCustomBoostGpuFreq;

    mtk_custom_upbound_gpu_freq_fp = MTKCustomUpBoundGpuFreq;

    mtk_get_custom_boost_gpu_freq_fp = MTKGetCustomBoostGpuFreq;

    mtk_get_custom_upbound_gpu_freq_fp = MTKGetCustomUpBoundGpuFreq;

    /*mtk_enable_gpu_dvfs_timer_fp = MTKMFGEnableDVFSTimer; */

    mtk_get_gpu_power_loading_fp = MTKGetPowerIndex;

    mtk_get_gpu_loading_fp = MTKGetGpuLoading;
    mtk_get_gpu_block_fp = MTKGetGpuBlock;
    mtk_get_gpu_idle_fp = MTKGetGpuIdle;

    if (!g_pvRegsKM)
    {
        PVRSRV_DEVICE_NODE* psDevNode = MTKGetRGXDevNode();
        if (psDevNode)
        {
            IMG_CPU_PHYADDR sRegsPBase;
            PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
            if (psDevConfig && (!g_pvRegsKM))
            {
                sRegsPBase = psDevConfig->sRegsCpuPBase;
                sRegsPBase.uiAddr += (uintptr_t)0xfff000;
                g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
            }
        }
    }

    return PVRSRV_OK;
}

IMG_VOID MTKMFGSystemDeInit(void)
{
    g_bExit = IMG_TRUE;

	if(g_hDVFSTimer)
    {
		MTKMFGEnableDVFSTimer(false);
		OSRemoveTimer(g_hDVFSTimer);
        g_hDVFSTimer = IMG_NULL;
    }

    if (ghDVFSLock)
    {
        OSLockDestroy(ghDVFSLock);
        ghDVFSLock = NULL;
    }

#ifdef MTK_CAL_POWER_INDEX
    g_pvRegsBaseKM = NULL;
#endif

    if (g_pvRegsKM)
    {
        OSUnMapPhysToLin(g_pvRegsKM, 0xFF, 0);
        g_pvRegsKM = NULL;
    }
}

module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_power, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);
module_param(gpu_dvfs_force_idle, uint, 0644);
module_param(gpu_dvfs_cb_force_idle, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(gpu_freq, uint, 0644);
