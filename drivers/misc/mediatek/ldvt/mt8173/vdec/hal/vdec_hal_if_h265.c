/********************************************************************************************
 *     LEGAL DISCLAIMER 
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES 
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED 
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS 
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED, 
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR 
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY 
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, 
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK 
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION 
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *     
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH 
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, 
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE 
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE. 
 *     
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS 
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.  
 ************************************************************************************************/
#include "vdec_hw_common.h"
#include "vdec_hal_if_h265.h"
#include "vdec_hw_h265.h"
#include "vdec_hal_errcode.h"
#include "../include/vdec_info_common.h"

//#include "x_hal_ic.h"
//#include "x_hal_1176.h"
//#include "x_debug.h"

//extern VDEC_INFO_VERIFY_FILE_INFO_T _tRecFileInfo;
//extern VDEC_INFO_H265_FBUF_INFO_T _ptH265FBufInfo[17];
//extern char _bFileStr1[9][300];
#include "../include/drv_common.h"

#if CONFIG_DRV_VERIFY_SUPPORT
#include "../verify/vdec_verify_general.h"
#include "../verify/vdec_verify_mpv_prov.h"
#include "../verify/vdec_info_verify.h"

#include <linux/string.h>
#include <linux/jiffies.h> 
#include <linux/timer.h>


#if (!CONFIG_DRV_LINUX)
#include <string.h>
#include <stdio.h>
#include "x_printf.h"
#endif

extern BOOL fgWrMsg2PC(void* pvAddr, UINT32 u4Size, UINT32 u4Mode, VDEC_INFO_VERIFY_FILE_INFO_T *pFILE_INFO);
#endif

const CHAR DIAG_SCAN[16]  =
{  0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

const CHAR DIAG_SCAN8[64] =
{  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

#ifdef MPV_DUMP_H265_DEC_REG
void VDec_DumpH265Reg(UCHAR ucMpvId);
#endif


// **************************************************************************
// Function : INT32 i4VDEC_HAL_H265_InitVDecHW(UINT32 u4VDecID)
// Description :Initialize video decoder hardware only for H265
// Parameter : u4VDecID : video decoder hardware ID
//                  prH265VDecInitPrm : pointer to VFIFO info struct
// Return      : =0: success.
//                  <0: fail.
// **************************************************************************
INT32 i4VDEC_HAL_H265_InitVDecHW(UINT32 u4VDecID)
{
#ifdef VDEC_SIM_DUMP
    printk("i4VDEC_HAL_H265_InitVDecHW() start!!\n");
#endif

#if (CONFIG_CHIP_VER_CURR < CONFIG_CHIP_VER_MT8555)
    vVDecResetHW(u4VDecID);
#elif (CONFIG_CHIP_VER_CURR >= CONFIG_CHIP_VER_MT8560)
    vVDecResetHW(u4VDecID, VDEC_H265);
#else
    vVDecResetHW(u4VDecID, VDEC_UNKNOWN);
#endif

#ifdef VDEC_SIM_DUMP
    printk("i4VDEC_HAL_H265_InitVDecHW() Done!!\n");
#endif
   
    return  HAL_HANDLE_OK;
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_ShiftGetBitStream(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits);
// Description :Read barrel shifter after shifting
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 u4ShiftBits : shift bits number
// Return      : Value of barrel shifter input window after shifting
// **************************************************************************
UINT32 u4VDEC_HAL_H265_ShiftGetBitStream(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits)
{
    UINT32 u4RegVal;
  
    u4RegVal = u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, u4ShiftBits);
  
    return (u4RegVal);
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_GetBitStreamShift(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits);
// Description :Read Barrel Shifter before shifting
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 u4ShiftBits : shift bits number
// Return      : Value of barrel shifter input window before shifting
// **************************************************************************
UINT32 u4VDEC_HAL_H265_GetBitStreamShift(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits)
{
    UINT32 u4RegVal0;
  
    u4RegVal0 = u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, 0);
    u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, u4ShiftBits);
    
    return (u4RegVal0);
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_GetRealBitStream(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits);
// Description :Read Barrel Shifter before shifting
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 u4ShiftBits : shift bits number
// Return      : Most significant (32 - u4ShiftBits) bits of barrel shifter input window before shifting
// **************************************************************************
UINT32 u4VDEC_HAL_H265_GetRealBitStream(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBits)
{
    UINT32 u4RegVal0;
  
    u4RegVal0 = u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, 0);
    u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, u4ShiftBits);
    
    return (u4RegVal0 >> (32-u4ShiftBits));
}


// **************************************************************************
// Function : UINT32 bVDEC_HAL_H265_GetBitStreamFlg(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Read Barrel Shifter before shifting
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : MSB of barrel shifter input window before shifting
// **************************************************************************
BOOL bVDEC_HAL_H265_GetBitStreamFlg(UINT32 u4BSID, UINT32 u4VDecID)
{
    UINT32 u4RegVal;
  
    u4RegVal = u4VDEC_HAL_H265_GetBitStreamShift(u4BSID, u4VDecID, 1);
    return ((u4RegVal >> 31));
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_UeCodeNum(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Do UE variable length decoding
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : Input window after UE variable length decoding
// **************************************************************************
UINT32 u4VDEC_HAL_H265_UeCodeNum(UINT32 u4BSID, UINT32 u4VDecID)
{
        return (u4VDecReadHEVCVLD(u4VDecID,RO_HEVLD_UE));
}


// **************************************************************************
// Function : INT32 i4VDEC_HAL_H265_SeCodeNum(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Do SE variable length decoding
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : Input window after SE variable length decoding
// **************************************************************************
INT32 i4VDEC_HAL_H265_SeCodeNum(UINT32 u4BSID, UINT32 u4VDecID)
{

        return ((INT32)u4VDecReadHEVCVLD(u4VDecID, RO_HEVLD_SE));
}

// *********************************************************************
// Function    : UINT32 u4VDEC_HAL_H265_GetStartCode_PicStart(UINT32 u4BSID, UINT32 u4VDecID)
// Description : Get next start code
// Parameter   : u4BSID : Barrel shifter ID
//                   u4VDecID : VLD ID
// Return      : None
// *********************************************************************
UINT32 u4VDEC_HAL_H265_GetStartCode_PicStart (UINT32 u4BSID, UINT32 u4VDecID)
{	
    UINT32 u4Temp = 0;
    UINT32 u4Temp2 =0;
    UINT32 u4NalType = 0;
    UINT32 u4ShiftBits = 0;
    UINT32 u4RetryNum = 0x100000;

#ifdef VDEC_SIM_DUMP
    printk ("[INFO] u4VDEC_HAL_H265_GetStartCode_PicStart() start!!\n");
#endif

    UINT32 i;
    
    do{
        u4Temp = u4VDEC_HAL_H265_ShiftGetBitStream(u4BSID, u4VDecID, 0);
        printk("%s %d u4Temp[0x%x] \n", __FUNCTION__, __LINE__, u4Temp);
        if ((u4Temp >> 8 & 0x00ffffff) != START_CODE){
           vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_CTRL, u4VDecReadHEVCVLD(u4BSID,RW_HEVLD_CTRL) | (HEVC_FIND_SC_CFG) );
           u4VDecReadHEVCVLD (u4VDecID, RW_HEVLD_CTRL );
           vVDecWriteHEVCVLD(u4VDecID,  HEVC_FC_TRG_REG,  HEVC_SC_START );
#ifdef VDEC_SIM_DUMP
                      printk("wait(`HEVC_SC_START == 0);\n");
#endif
           for (i = 0; i < u4RetryNum; i++){
                if ((u4VDecReadHEVCVLD(u4VDecID,  HEVC_FC_TRG_REG) & HEVC_SC_START) == 0)
                    break;
           }
        }  
        u4Temp = u4VDEC_HAL_H265_ShiftGetBitStream(u4BSID, u4VDecID, 8);
        u4Temp = u4VDEC_HAL_H265_GetBitStreamShift(u4BSID, u4VDecID, 32);
        u4NalType = (((u4Temp & 0xffff)>>9) & 0x3f);
        u4Temp2 =  u4VDEC_HAL_H265_ShiftGetBitStream(u4BSID, u4VDecID, 0);
        printk("%s %d u4Temp2[0x%x] u4NalType[0x%x]\n", __FUNCTION__, __LINE__, u4Temp2, u4NalType);
    } while ( (((u4Temp2>>31) & 0x01) == 0) && (u4NalType <= 21)  );   // until get "first_slice_segment_in_pic_flag" == 1

#ifdef VDEC_SIM_DUMP
    printk("[INFO] Intput Window GetStartCode  = 0x%08x\n",u4Temp );
    printk("[INFO] u4VDEC_HAL_H265_GetStartCode_PicStart() Done!!\n");
#endif


    return u4Temp;
}


// *********************************************************************
// Function    : UINT32 u4VDEC_HAL_H265_GetStartCode(UINT32 u4BSID, UINT32 u4VDecID)
// Description : Get next start code
// Parameter   : u4BSID : Barrel shifter ID
//                   u4VDecID : VLD ID
// Return      : None
// *********************************************************************
UINT32 u4VDEC_HAL_H265_GetStartCode_8530 (UINT32 u4BSID, UINT32 u4VDecID)
{	
    UINT32 u4Temp = 0;
    UINT32 u4ShiftBits = 0;
    UINT32 u4RetryNum = 0x100000;

#ifdef VDEC_SIM_DUMP
    printk ("[INFO] u4VDEC_HAL_H265_GetStartCode_8530() start!!\n");
#endif
   
#if (CONFIG_CHIP_VER_CURR >= CONFIG_CHIP_VER_MT8530)
    UINT32 i;

       u4Temp = u4VDEC_HAL_H265_ShiftGetBitStream(u4BSID, u4VDecID, 0);

        if ((u4Temp >> 8& 0x00ffffff) != START_CODE){
           vVDecWriteHEVCVLD(u4VDecID,  RW_HEVLD_CTRL, u4VDecReadHEVCVLD(u4BSID,RW_HEVLD_CTRL) | (HEVC_FIND_SC_CFG) );
           u4VDecReadHEVCVLD (u4VDecID, RW_HEVLD_CTRL );

           vVDecWriteHEVCVLD(u4VDecID,  HEVC_FC_TRG_REG,  HEVC_SC_START );
           for (i = 0; i < u4RetryNum; i++ ){
                if ((u4VDecReadHEVCVLD(u4VDecID,  HEVC_FC_TRG_REG) & HEVC_SC_START) == 0)
                    break;
           }
        }  

#endif    
    u4Temp = u4VDEC_HAL_H265_ShiftGetBitStream(u4BSID, u4VDecID, 8);
    u4Temp = u4VDEC_HAL_H265_GetBitStreamShift(u4BSID, u4VDecID, 32);   


#ifdef VDEC_SIM_DUMP
    printk("[INFO] Intput Window GetStartCode  = 0x%08x\n",u4Temp );
    printk("[INFO] u4VDEC_HAL_H265_GetStartCode_8530() Done!!\n");
#endif


    return u4Temp;
}


// **************************************************************************
// Function : INT32 i4VDEC_HAL_H265_InitBarrelShifter(UINT32 u4BSID, UINT32 u4VDecID, VDEC_INFO_H265_BS_INIT_PRM_T *prH265BSInitPrm);
// Description :Initialize barrel shifter with byte alignment
// Parameter :u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 prH265BSInitPrm : pointer to h265 initialize barrel shifter information struct
// Return      : =0: success.
//                  <0: fail.
// **************************************************************************
INT32 i4VDEC_HAL_H265_InitBarrelShifter(UINT32 u4BSID, UINT32 u4VDecID, VDEC_INFO_H265_BS_INIT_PRM_T *prH265BSInitPrm)
{
    BOOL fgInitBSResult;

    #ifdef VDEC_SIM_DUMP
        printk ("[INFO] i4VDEC_HAL_H265_InitBarrelShifter() start!!\n");
    #endif

    fgInitBSResult = fgInitH265BarrelShift(u4VDecID, prH265BSInitPrm);

    if (fgInitBSResult)
    {
#ifdef VDEC_SIM_DUMP
        printk("[INFO] i4VDEC_HAL_H265_InitBarrelShifter() Done!!\n");
#endif
        return HAL_HANDLE_OK;
    }
    else
    {
        printk("\n[ERROR] i4VDEC_HAL_H265_InitBarrelShifter() Fail!!!!!!!!!!!!!!\n\n");
        return INIT_BARRELSHIFTER_FAIL;
    }
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_ReadRdPtr(UINT32 u4BSID, UINT32 u4VDecID, UINT32 *pu4Bits);
// Description :Read current read pointer
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 pu4Bits : read pointer value with remained bits
// Return      : Read pointer value with byte alignment
// **************************************************************************
UINT32 u4VDEC_HAL_H265_ReadRdPtr(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4VFIFOSa, UINT32* pu4Bits)
{
    UINT32 retval;
    #ifdef VDEC_SIM_DUMP
        printk ("[INFO] u4VDEC_HAL_H265_ReadRdPtr() start!!\n");
    #endif
    retval = u4VDecReadH265VldRPtr(u4BSID, u4VDecID, pu4Bits, PHYSICAL(u4VFIFOSa));
    #ifdef VDEC_SIM_DUMP
        printk ("[INFO] u4VDEC_HAL_H265_ReadRdPtr() done!!\n");
    #endif

    return retval;
}


// **************************************************************************
// Function : void v4VDEC_HAL_H265_AlignRdPtr(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4AlignType);
// Description :Align read pointer to byte,word or double word
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
//                 u4AlignType : read pointer align type
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_AlignRdPtr(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4AlignType)
{
    return;
}


// **************************************************************************
// Function : UINT32 u4VDEC_HAL_H265_GetBitcount(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Read barrel shifter bitcount after initializing 
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : Current bit count
// **************************************************************************
UINT32 u4VDEC_HAL_H265_GetBitcount(UINT32 u4BSID, UINT32 u4VDecID)
{
    return HAL_HANDLE_OK;
}


// **************************************************************************
// Function : void vVDEC_HAL_H265_Modification(UINT32 u4VDecID);
// Description :Reference list reordering
// Parameter : u4VDecID : video decoder hardware ID
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_RPL_Modification(UINT32 u4VDecID)
{
    UINT32 u4Cnt;
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] vVDEC_HAL_H265_RPL_Modification()\n");
#endif
    
    vVDecWriteHEVCVLD(u4VDecID, HEVC_FC_TRG_REG, HEVC_RPL_MOD);
    u4Cnt = 0;
    while(1)
    {
        if (u4Cnt == 100)
        {
            if (0==(u4VDecReadHEVCVLD(u4VDecID, HEVC_FC_TRG_REG) & HEVC_RPL_MOD))
            {
                break;
            }
            else
            {
                u4Cnt = 0;        
            }
        }
        else
        {
            u4Cnt ++;
        }
    }
}

// **************************************************************************
// Function : void vVDEC_HAL_H265_PredWeightTable(UINT32 u4VDecID);
// Description :Decode prediction weighting table
// Parameter : u4VDecID : video decoder hardware ID
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_PredWeightTable(UINT32 u4VDecID)
{
  
    UINT32 u4Cnt;
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] vVDEC_HAL_H265_PredWeightTable()\n");
#endif
  
    vVDecWriteHEVCVLD(u4VDecID, HEVC_FC_TRG_REG, HEVC_WEIGHT_PRED_TBL);
    u4Cnt = 0;
    while(1)
    {
        if (u4Cnt == 100)
        {
            if (0==(u4VDecReadHEVCVLD(u4VDecID, HEVC_FC_TRG_REG) & HEVC_WEIGHT_PRED_TBL))
            {
                break;
            }
            else
            {
                u4Cnt = 0;        
            }
        }
        else
        {
            u4Cnt ++;
        }
    }
  
}


// **************************************************************************
// Function : void vVDEC_HAL_H265_TrailingBits(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Remove traling bits to byte align
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_TrailingBits(UINT32 u4BSID, UINT32 u4VDecID)
{
    UINT32 u4Temp;
  
    u4Temp = 8 - (u4VDecHEVCVLDShiftBits(u4BSID, u4VDecID) % 8);
    // at list trailing bit

    if (u4Temp == 8 ){
        u4Temp = u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, 8);
    } else {
        u4Temp = u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, u4Temp);
    }
    
}


// **************************************************************************
// Function : BOOL bVDEC_HAL_H265_IsMoreRbspData(UINT32 u4BSID, UINT32 u4VDecID);
// Description :Check whether there is more rbsp data
// Parameter : u4BSID  : barrelshifter ID
//                 u4VDecID : video decoder hardware ID
// Return      : Is morw Rbsp data or not
// **************************************************************************
BOOL bVDEC_HAL_H265_IsMoreRbspData(UINT32 u4BSID, UINT32 u4VDecID)
{
    UINT32 u4RemainedBits;
    UINT32 u4Temp;
    INT32 i;
    
    u4RemainedBits = (u4VDecHEVCVLDShiftBits(u4BSID, u4VDecID) % 8); //0~7
    //u4RemainedBits = (8 - (((u4VDecReadHEVCVLD(RW_HEVLD_CTRL) >> 16) & 0x3F) % 8));  
    u4Temp = 0xffffffff;
    for (i=0; i<=u4RemainedBits; i++)
    {
        u4Temp &= (~(1<<i));
    }
    
    if ((u4VDecHEVCVLDGetBitS(u4BSID, u4VDecID, 0) & u4Temp) == (0x80000000))
    {
        // no more
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


// **************************************************************************
// Function : void vVDEC_HAL_H265_InitPicInfo(UINT32 u4VDecID);
// Description :Set HW registers to initialize picture info
// Parameter : u4VDecID : video decoder hardware ID
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_InitPicInfo(UINT32 u4VDecID)
{
    UINT32 dram_pitch_width, log2_max_cu_size, max_cu_size;
    UINT32 pic_width, pic_height;
    H265_SPS_Data* prSPS;
    H265_PPS_Data* prPPS;

    prSPS = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS;
    prPPS = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prPPS;
    pic_width = prSPS->u4PicWidthInLumaSamples;
    pic_height = prSPS->u4PicHeightInLumaSamples;

    log2_max_cu_size =  prSPS->u4Log2DiffMaxMinCodingBlockSize + prSPS->u4Log2MinCodingBlockSizeMinus3+3;
    max_cu_size = 1<<log2_max_cu_size;

#if 1 //HUB MT8137 HEVC
    dram_pitch_width = (((pic_width + (max_cu_size - 1)) >> log2_max_cu_size) << log2_max_cu_size) / 16;
#else
    dram_pitch_width = (pic_width%64 > 0)? (((pic_width/64) +1)*64 )/16 : pic_width/16;
#endif

    // MC part
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] MC settings\n");
#endif
    vVDecWriteMC(u4VDecID, HEVC_PIC_WIDTH, pic_width);
    vVDecWriteMC(u4VDecID, HEVC_PIC_HEIGHT, pic_height);
    vVDecWriteMC(u4VDecID, HEVC_DRAM_PITCH, dram_pitch_width );
    
#if 0 //HUB MT8137 HEVC
    vVDecWriteMC(u4VDecID, HEVC_CBCR_DPB_OFFSET, _ptH265CurrFBufInfo[u4VDecID]->u4CAddrOffset );
#endif

    // 10 bit setting
    vVDecWriteMC(u4VDecID, HEVC_MC_CMPTMODE, (_u4Main10Mode == HEVC_COMPACT_MODE) ? 0x1 : 0x0);

    int pic_mb_x = ((pic_width  + max_cu_size - 1)/max_cu_size)*max_cu_size/16;
    int pic_mb_y = ((pic_height + max_cu_size - 1)/max_cu_size)*max_cu_size/16;

    //PP part
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] Current Buffer index: %d \n",_tVerMpvDecPrm[u4VDecID].ucDecFBufIdx);
    printk ("[INFO] PP settings\n");
#endif

    vVDecWriteMC(u4VDecID, HEVC_DBK_ON, 0x1);
    vVDecWriteMC(u4VDecID, HEVC_Y_OUTPUT, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr)>>9) & 0x7fffff );
#if 1 //HUB MT8137 HEVC
    vVDecWriteMC(u4VDecID, HEVC_C_OUTPUT, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4CStartAddr)>>8) & 0xffffff );
#else
    vVDecWriteMC(u4VDecID, HEVC_C_OUTPUT, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr+_ptH265CurrFBufInfo[u4VDecID]->u4DramPicSize)>>8) & 0xffffff );
#endif
    vVDecWriteMC(u4VDecID, HEVC_OUTPUT_WIDTH, pic_mb_x);
    
    vVDecWriteMC(u4VDecID, HEVC_DBK_ON2, ((prSPS->u4ChromaFormatIdc==0)? 2:3));
    vVDecWriteMC(u4VDecID, HEVC_ENABLE_WR_REC, 0x1);
    vVDecWriteMC(u4VDecID, HEVC_PIC_WITDTH_MB, pic_mb_x-1);
    vVDecWriteMC(u4VDecID, HEVC_PIC_HEIGHT_MB, pic_mb_y-1);
    
#if 1 //HUB MT8137 HEVC    
    if(_u4Main10Mode == HEVC_16BIT_MODE)
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_EXT_BIT_DRAM_MODE, 0x0);
    }
    else if(_u4Main10Mode == HEVC_REORDER_MODE)
    {        
        vVDecWriteHEVCPP(u4VDecID, HEVC_EXT_BIT_DRAM_MODE, 0x2);
    }
    else if(_u4Main10Mode == HEVC_COMPACT_MODE)
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_EXT_BIT_DRAM_MODE, 0x1);
    }

    //rearragen 2 bit for 10bit mode
    if ((_u4Main10Mode == HEVC_16BIT_MODE) || (_u4Main10Mode == HEVC_REORDER_MODE) || (_u4Main10Mode == HEVC_COMPACT_MODE))
    {
        printk("[MT8137] ReArrange 2 bit!\n");
        vVDecWriteHEVCPP(u4VDecID, 740*4, 0x1);
    }

    vVDecWriteHEVCMISC(u4VDecID, 1*4, 0x100);
#endif
}

// **************************************************************************
// Function : void vVDEC_HAL_H265_SetRefPicListReg(UINT32 u4VDecID);
// Description :Set HW registers related with P reference list
// Parameter : u4VDecID : video decoder hardware ID
//                 prPRefPicListInfo : pointer to information of p reference list
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetRefPicListReg(UINT32 u4VDecID)
{
#if 1 //HUB MT8137 HEVC
    UINT32 addr;
    UINT32 i,pic_width,pic_height;
    unsigned int value;
    BOOL bIsUFO;  
    H265_SPS_Data* prSPS;

    bIsUFO = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode;
    prSPS = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS;

    pic_width = prSPS->u4PicWidthInLumaSamples;
    pic_height = prSPS->u4PicHeightInLumaSamples;

#ifdef VDEC_SIM_DUMP
    printk ("// [INFO] UFO settings\n");
#endif
    // UFO mode settings
    if (bIsUFO)
    { 
        // 6593 UFO mode
        #if 0 
        pic_width = ((pic_width + 63)>>6)<<6;
        pic_height = ((pic_height + 31)>>5)<<5;   
        vVDecWriteMC(u4VDecID, 700*4, ((pic_width/16-1)<<16) |(pic_height/16-1));      
        #endif
        vVDecWriteMC(u4VDecID, 698*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YLenStartAddr));
        vVDecWriteMC(u4VDecID, 699*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4CLenStartAddr));
        // leon
        vVDecWriteMC(u4VDecID, 825*4, _ptH265CurrFBufInfo[u4VDecID]->u4DramYSize);        
        vVDecWriteMC(u4VDecID, 826*4, _ptH265CurrFBufInfo[u4VDecID]->u4DramCSize);
        // enable UFO mode
        #if 1
        vVDecWriteMC(u4VDecID, 664*4, 0x11);
        #endif

		#if 0
        // temp for test.
        vVDecWriteMC(u4VDecID, 664*4, 0x0);
        #endif
        
        // 6593 UFO mode
        #if 0
        vVDecWriteMC(u4VDecID, 663*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeBS);
        vVDecWriteMC(u4VDecID, 701*4, _ptH265CurrFBufInfo[u4VDecID]->u4UFOLenYsize);
        vVDecWriteMC(u4VDecID, 343*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeYBS);
        vVDecWriteHEVCPP(u4VDecID, 803*4, 0x1 );    // UFO error handling no hang
        #endif
    } 
    else 
    {
        vVDecWriteMC(u4VDecID, 664*4, 0x0);
    }

#ifdef VDEC_SIM_DUMP
    printk ("// [INFO] EC settings\n");
#endif
    // Error Concealment settings
    UCHAR ucPreFBIndex;
    ucPreFBIndex = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.ucPreFBIndex;

    // yc seperate setting on
    vVDecWriteMC(u4VDecID, HEVC_MC_Y_C_SEP_SET,  0x1 );
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_P_LIST0_Y,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST0_Y,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST1_Y,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));

    vVDecWriteMC(u4VDecID, HEVC_MC_REF_P_LIST0_C,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST0_C,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST1_C,  PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));

    
    vVDecWriteHEVCMV(u4VDecID, 0, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4MvStartAddr)>>4 );
    vVDecWriteHEVCMV(u4VDecID, 16*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4MvStartAddr)>>4 );

    vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_ERROR_TYPE_ENABLE, 0xff7efffb);
    //vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_PICTURE_BYTES, total_bytes_in_curr_pic);
    vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_ERROR_HANDLING, 0x04011101);   // 06172013, turn on slice_reconceal_sel
#if 1 //HUB MT8137 HEVC
    if (_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prPPS->bTilesEnabledFlag 
        && (0 == _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode))
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x80004011 );
    }
    else
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01007011 );
    }
#else
    if (_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prPPS->bTilesEnabledFlag){  //tiles_enabled_flag    
#if HEVC_CRC_COMPARE
    // dual-core with tile setting while CRC on
#if 0//HUB MT8137 HEVC
    if(_u4DualCoreEnable[u4VDecID] != 0|| u4VDecID != 0)
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01007011 ); // error concealment not as good as 0x01004011, for crc check.
    }
    else // single-core with tile setting while CRC on
#endif
    {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01004011 );
    }
#else
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01004011 );
#endif
    } else {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01007011 );
    }
#endif
#ifdef VDEC_SIM_DUMP
    printk ("// [INFO] MV settings\n");
#endif

    value = _rH265PicInfo[u4VDecID].bLowDelayFlag;
    vVDecWriteHEVCMV(u4VDecID, HEVC_MV_CTRL,value);

    value   = 0;
    if (_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {
        for (i = 0; i < 16; i++){
            int idx;
            idx = (i % _rH265PicInfo[u4VDecID].i4RefListNum);
            value = value | (_rH265PicInfo[u4VDecID].i4LongTermList0[idx] << i) | (_rH265PicInfo[u4VDecID].i4LongTermList1[idx] << (i + 16));
        }
        vVDecWriteHEVCMV(u4VDecID, HEVC_LONG_TERM, value);
    }

    value = 0;
    for (i = 0; i < _rH265PicInfo[u4VDecID].i4DpbLTBuffCnt; i++)
    {
        if ((i % 4) == 0) {
            value = 0;
        }
        
        value |= ((_rH265PicInfo[u4VDecID].i4DpbLTBuffId[i] + 1) & 0x1f) << (i*8);
        if ((i % 4) == 3){
            vVDecWriteHEVCMV(u4VDecID, HEVC_DPB_LT_BUF_ID_0_3+ i ,value);
        }
    }

    if ((i % 4) != 0) {
        vVDecWriteHEVCMV(u4VDecID, HEVC_DPB_LT_BUF_ID_0_3+ i ,value);
    }

    if (_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {
        for (i = 0; i < 16; i = i+1)
        { 
            int idx;
            idx = (i % _rH265PicInfo[u4VDecID].i4RefListNum);
            addr = HEVC_L0_INFO_0 + i*4;
            value = ((_rH265PicInfo[u4VDecID].i4PocDiffList0[idx] & 0xff) << 0) 
                | (((_rH265PicInfo[u4VDecID].i4BuffIdList0[idx] + 1) & 0x1f) << 8);
            vVDecWriteHEVCMV(u4VDecID, addr ,value);
        }

        for (i = 0; i < 16; i = i+1)
        {
            int idx;
            idx = (i % _rH265PicInfo[u4VDecID].i4RefListNum);
            addr = HEVC_L1_INFO_0 + i*4;
            value = ((_rH265PicInfo[u4VDecID].i4PocDiffList1[idx] & 0xff) << 0) 
                | (((_rH265PicInfo[u4VDecID].i4BuffIdList1[idx] + 1) & 0x1f) << 8);
            vVDecWriteHEVCMV(u4VDecID, addr ,value);
        }
    }

#ifdef VDEC_SIM_DUMP
    printk ("// [INFO] [%d]Current Buffer index: %d \n", u4VDecID, _tVerMpvDecPrm[u4VDecID].ucDecFBufIdx);
#endif
    vVDecWriteHEVCMV(u4VDecID, HEVC_MV_WR_SA , (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4MvStartAddr)>>4) & 0xfffffff );

    if (_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {
        for (i = 0; i < 16; i++)
        {
            int idx;
            idx = (i % _rH265PicInfo[u4VDecID].i4RefListNum);
            vVDecWriteHEVCMV(u4VDecID, i*4, (PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4MvStartAddr) >> 4) & 0xfffffff );
            vVDecWriteMC(u4VDecID, (i + 279)*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4YStartAddr));            
            vVDecWriteMC(u4VDecID, (i + 760)*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4CStartAddr));
#if 0
            if (bIsUFO){
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded &= (~( 0x1<<i ));
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded |= (_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].bIsUFOEncoded << i );
            }
#endif
        }
        for(i=0;i<16;i++)
        {
            int idx;
            idx = (i % _rH265PicInfo[u4VDecID].i4RefListNum);
            vVDecWriteHEVCMV(u4VDecID, (i + 16)*4, (PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4MvStartAddr) >> 4) & 0xfffffff);
            vVDecWriteMC(u4VDecID, (i + 311)*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4YStartAddr));            
            vVDecWriteMC(u4VDecID, (i + 792)*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4CStartAddr));
#if 0
            if (bIsUFO){
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded &= (~( 0x1<<(i+16) ));
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded |= (_ptH265FBufInfo[u4VDecID][ _rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].bIsUFOEncoded << (i+16) );
            }
#endif
        }
   }
#else
    UINT32 addr;
    UINT32 i,pic_width,pic_height;
    unsigned int value;
    BOOL bIsUFO;  
    H265_SPS_Data* prSPS;

    bIsUFO = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode;
    prSPS = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS;

    pic_width = prSPS->u4PicWidthInLumaSamples;
    pic_height = prSPS->u4PicHeightInLumaSamples;

#ifdef VDEC_SIM_DUMP
    printk ("[INFO] UFO settings\n");
#endif
    // UFO mode settings
    if (bIsUFO)
    {
#if 0 //HUB MT8137 HEVC
        pic_width = ((pic_width + 63)>>6)<<6;
        pic_height = ((pic_height + 31)>>5)<<5;   
        vVDecWriteMC(u4VDecID, 700*4, ((pic_width/16-1)<<16) |(pic_height/16-1)); 
#endif

        vVDecWriteMC(u4VDecID, 698*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YLenStartAddr));
        vVDecWriteMC(u4VDecID, 699*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4CLenStartAddr));

#if 1 //HUB MT8137 HEVC
        vVDecWriteMC(u4VDecID, 825*4, _ptH265CurrFBufInfo[u4VDecID]->u4DramYSize);        
        vVDecWriteMC(u4VDecID, 826*4, _ptH265CurrFBufInfo[u4VDecID]->u4DramCSize);

        vVDecWriteMC(u4VDecID, HEVC_MC_UFO_ONOFF, 0x11);
#endif

#if 0 //HUB MT8137 HEVC
        vVDecWriteMC(u4VDecID, 663*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeBS);
        vVDecWriteMC(u4VDecID, 701*4, _ptH265CurrFBufInfo[u4VDecID]->u4UFOLenYsize);
        vVDecWriteMC(u4VDecID, 343*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeYBS);

        vVDecWriteHEVCPP(u4VDecID, 803*4, 0x1 );    // UFO error handling no hang
#endif
    } else {
        vVDecWriteMC(u4VDecID, 664*4, 0x0);
    }

#ifdef VDEC_SIM_DUMP
        printk ("[INFO] EC settings\n");
#endif
    // Error Concealment settings
    UCHAR ucPreFBIndex;
    ucPreFBIndex = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.ucPreFBIndex;

#if 1 //HUB MT8137 HEVC
    // yc seperate setting on
    vVDecWriteMC(u4VDecID, HEVC_MC_Y_C_SEP_SET, 0x1);
#endif

    vVDecWriteMC(u4VDecID, HEVC_MC_REF_P_LIST0_Y, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST0_Y, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST1_Y, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4YStartAddr));
    
#if 1 //HUB MT8137 HEVC
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_P_LIST0_C, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST0_C, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));
    vVDecWriteMC(u4VDecID, HEVC_MC_REF_B_LIST1_C, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4CStartAddr));
#endif

    vVDecWriteHEVCMV(u4VDecID, 0, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4MvStartAddr)>>4 );
    vVDecWriteHEVCMV(u4VDecID, 16*4, PHYSICAL(_ptH265FBufInfo[u4VDecID][ucPreFBIndex].u4MvStartAddr)>>4 );

    vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_ERROR_TYPE_ENABLE, 0xff7efffb);
    //vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_PICTURE_BYTES, total_bytes_in_curr_pic);
    vVDecWriteHEVCVLD(u4VDecID, HEVC_VLD_ERROR_HANDLING, 0x04011101);   // 06172013, turn on slice_reconceal_sel

    if ( _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prPPS->bTilesEnabledFlag ){  //tiles_enabled_flag
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01004011 );
    } else {
        vVDecWriteHEVCPP(u4VDecID, HEVC_PP_ERROR_HANDLE_MODE, 0x01007011 );
    }

#ifdef VDEC_SIM_DUMP
        printk ("[INFO] MV settings\n");
#endif

    value   =  _rH265PicInfo[u4VDecID].bLowDelayFlag;
    vVDecWriteHEVCMV(u4VDecID, HEVC_MV_CTRL,value);

    value   = 0;
    if(_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {
        for(i=0; i<16;i++) {
            int idx;
            idx = (i%_rH265PicInfo[u4VDecID].i4RefListNum);
            value = value | (_rH265PicInfo[u4VDecID].i4LongTermList0[idx] <<i) | (_rH265PicInfo[u4VDecID].i4LongTermList1[idx] <<(i+16));
        }
        vVDecWriteHEVCMV(u4VDecID, HEVC_LONG_TERM,value);
    }

    value = 0;
    for(i=0; i<_rH265PicInfo[u4VDecID].i4DpbLTBuffCnt;i++)
    {
        if(i%4 == 0) {
            value = 0;
        }
        value |= ((_rH265PicInfo[u4VDecID].i4DpbLTBuffId[i] + 1) & 0x1f) << (i*8);
        if(i%4 == 3){
            vVDecWriteHEVCMV(u4VDecID, HEVC_DPB_LT_BUF_ID_0_3+ i ,value);
        }
    }

    if(i%4 != 0) {
        vVDecWriteHEVCMV(u4VDecID, HEVC_DPB_LT_BUF_ID_0_3+ i ,value);
    }

    if(_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {

        for(i=0; i<16;i=i+1)
        { 
            int idx;
            idx = (i%_rH265PicInfo[u4VDecID].i4RefListNum);
            addr = HEVC_L0_INFO_0 + i*4;
            value = ((_rH265PicInfo[u4VDecID].i4PocDiffList0[idx] & 0xff) << 0) |
              (((_rH265PicInfo[u4VDecID].i4BuffIdList0[idx] + 1) & 0x1f) << 8) ;
            vVDecWriteHEVCMV(u4VDecID, addr ,value);
        }

        for(i=0; i<16;i=i+1)
        {
            int idx;
            idx = (i%_rH265PicInfo[u4VDecID].i4RefListNum);
            addr      = HEVC_L1_INFO_0 + i*4;
            value = ((_rH265PicInfo[u4VDecID].i4PocDiffList1[idx] & 0xff) << 0) |
              (((_rH265PicInfo[u4VDecID].i4BuffIdList1[idx] + 1) & 0x1f) << 8) ;
            vVDecWriteHEVCMV(u4VDecID, addr ,value);
        }
    }

#ifdef VDEC_SIM_DUMP
    printk ("[INFO] Current Buffer index: %d \n",_tVerMpvDecPrm[u4VDecID].ucDecFBufIdx);
#endif
    vVDecWriteHEVCMV(u4VDecID, HEVC_MV_WR_SA ,(PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4MvStartAddr)>>4) & 0xfffffff );

    if(_rH265PicInfo[u4VDecID].i4RefListNum > 0)
    {
        for(i=0;i<16;i++)
        {
            int idx;
            idx = (i%_rH265PicInfo[u4VDecID].i4RefListNum);
            vVDecWriteHEVCMV(u4VDecID, i*4 , (PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4MvStartAddr) >>4) & 0xfffffff );
            vVDecWriteMC(u4VDecID, (i+279)*4 ,PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4YStartAddr));
            vVDecWriteMC(u4VDecID, (i+760)*4 ,PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].u4CStartAddr));
#if 0
            if (bIsUFO){
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded &= (~( 0x1<<i ));
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded |= (_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList0[idx]].bIsUFOEncoded << i );
            }
#endif
        }
        for(i=0;i<16;i++)
        {
            int idx;
            idx = (i%_rH265PicInfo[u4VDecID].i4RefListNum);
            vVDecWriteHEVCMV(u4VDecID, (i+16)*4 ,(PHYSICAL(_ptH265FBufInfo[u4VDecID][ _rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4MvStartAddr) >>4) & 0xfffffff );
            vVDecWriteMC(u4VDecID, (i+311)*4 ,PHYSICAL(_ptH265FBufInfo[u4VDecID][ _rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4YStartAddr) );
            vVDecWriteMC(u4VDecID, (i+792)*4 ,PHYSICAL(_ptH265FBufInfo[u4VDecID][_rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].u4CStartAddr));
#if 0
            if (bIsUFO){
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded &= (~( 0x1<<(i+16) ));
                _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded |= (_ptH265FBufInfo[u4VDecID][ _rH265PicInfo[u4VDecID].i4BuffIdList1[idx]].bIsUFOEncoded << (i+16) );
            }
#endif
        }
   }
#endif    
}


// **************************************************************************
// Function : void vVDEC_HAL_H265_SetSPSHEVLD(UINT32 u4VDecID, H265_SPS_Data *prSPS);
// Description :Set Slice data to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 prPPS
//                 prSPS : pointer to sequence parameter set struct
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetSPSHEVLD(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
{
    int max_cu_width, max_cu_heigtht;
    int pic_width, pic_height;
    int lcu_pic_width, lcu_pic_height;
    int log2_lt_ref_pic_sps = 1;
    int log2_short_ref_pic_idx = 1;
    int log2_max_cu_size;
    int log2_max_tu_size;

    UINT32 u4SPSInfo;
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] vVDEC_HAL_H265_SetSPSHEVLD() start!!\n");
#endif
    
    log2_max_cu_size =  prSPS->u4Log2DiffMaxMinCodingBlockSize + prSPS->u4Log2MinCodingBlockSizeMinus3+3;
    log2_max_tu_size = prSPS->u4Log2DiffMaxMinTtransformBlockSize + prSPS->u4Log2MinTransformBlockSizeMinus2+2;

    max_cu_width = 1 << log2_max_cu_size;
    max_cu_heigtht = 1 << log2_max_cu_size;

    pic_width = prSPS->u4PicWidthInLumaSamples;
    pic_height = prSPS->u4PicHeightInLumaSamples;
    pic_width = ((pic_width + max_cu_width -1) >> log2_max_cu_size ) << log2_max_cu_size;
    pic_height = ((pic_height + max_cu_width -1) >> log2_max_cu_size ) << log2_max_cu_size;

    lcu_pic_width    = ( pic_width%max_cu_width ) ? pic_width /max_cu_width  + 1 : pic_width /max_cu_width;
    lcu_pic_height   = ( pic_height%max_cu_heigtht ) ? pic_height/max_cu_heigtht + 1 : pic_height/max_cu_heigtht;

    _rH265PicInfo[u4VDecID].u4PicWidthInCU = lcu_pic_width;
    _rH265PicInfo[u4VDecID].u4PicHeightInCU = lcu_pic_height;

    vVDecWriteVLDTOP(u4VDecID, RW_VLD_PIC_MB_SIZE_M1, (((lcu_pic_width  - 1) & 0xfff) << 0 ) |(((lcu_pic_height - 1) & 0xfff) << 16 ));  
    vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_PIC_PIX_SIZE, (((prSPS->u4PicWidthInLumaSamples) & 0xffff) << 0 ) | (((prSPS->u4PicHeightInLumaSamples) & 0xffff) << 16 ));  
    vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_PIC_BLK_SIZE, (((lcu_pic_width  - 0) & 0xfff) << 0 ) |(((lcu_pic_height - 0) & 0xfff) << 16 ));  

    // for 10 bit or 12 bit
#if 0 //HUB MT8137 HEVC
    if ((_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.u4ProfileIdc == 2)
        || (_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.bProfileCompatibilityFlag[2] == 1))
    {
        vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_BIT_DEPTH_SETTING, 0x22);  
    }
#else
    if ((prSPS->u4BitDepthLumaMinus8 != 0) && (prSPS->u4BitDepthChromaMinus8 != 0))
    {
        if ((prSPS->u4BitDepthLumaMinus8 == 0x2) && (prSPS->u4BitDepthChromaMinus8 == 0x2)) 
        {
            vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_BIT_DEPTH_SETTING, 0x22);  
        }
        else if ((prSPS->u4BitDepthLumaMinus8 == 0x4) && (prSPS->u4BitDepthChromaMinus8 == 0x4)) 
        {
            // leon, need confirm
            vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_BIT_DEPTH_SETTING, 0x22);  
        }
        else
        {
            // error
        }
    }     
#endif

    u4SPSInfo = 0;
    u4SPSInfo = (prSPS->u4ChromaFormatIdc & 0x3);
    u4SPSInfo |= ((prSPS->bUsePCM& 0x1)<< 2);
    u4SPSInfo |= ((prPPS->bListsModificationPresentFlag& 0x1) << 3); 
    u4SPSInfo |= ((prSPS->bUseAMP & 0x1) << 4);
    u4SPSInfo |= ((prSPS->bUseSAO & 0x1) << 5);
    u4SPSInfo |= ((prSPS->bPCMFilterDisableFlag & 0x1) << 6);
    u4SPSInfo |= ((prSPS->bTMVPFlagsPresent & 0x1) << 7);
    u4SPSInfo |= ((prSPS->u4PCMBitDepthLumaMinus1 & 0xf) << 8);
    u4SPSInfo |= ((prSPS->u4PCMBitDepthChromaMinus1& 0xf) << 12);
    u4SPSInfo |= ((prSPS->u4NumLongTermRefPicSPS & 0x3f) << 16);
    u4SPSInfo |= ((prSPS->bLongTermRefsPresent & 0x1) << 22);
    u4SPSInfo |= ((prSPS->u4NumShortTermRefPicSets & 0x7f) << 24);
    
    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_SPS_0, u4SPSInfo);

    //printk("1-------prSPS->u4NumLongTermRefPicSPS %d\n",prSPS->u4NumLongTermRefPicSPS);
    while ( (prSPS->u4NumLongTermRefPicSPS & 0x3f) > (1 << log2_lt_ref_pic_sps)){
        //printk("looping-------log2_lt_ref_pic_sps %d\n",log2_lt_ref_pic_sps);
        log2_lt_ref_pic_sps++;
    }
    //printk("2-------prSPS->u4NumShortTermRefPicSets %d\n",prSPS->u4NumShortTermRefPicSets);
    while ( (prSPS->u4NumShortTermRefPicSets & 0x7f) > (1 << log2_short_ref_pic_idx)){
        //printk("looping-------log2_short_ref_pic_idx %d\n",log2_short_ref_pic_idx);
        log2_short_ref_pic_idx++;
    }
    //printk("3-------log2_lt_ref_pic_sps %d; log2_short_ref_pic_idx %d\n", log2_lt_ref_pic_sps, log2_short_ref_pic_idx);
    
    u4SPSInfo = 0;
    u4SPSInfo = ((prSPS->u4Log2MaxPicOrderCntLsbMinus4+4) & 0x1f);
    u4SPSInfo |= ((log2_short_ref_pic_idx & 0x7)<< 8);
    u4SPSInfo |= ((log2_lt_ref_pic_sps & 0x7)<< 12);
    u4SPSInfo |= ((prSPS->bUseStrongIntraSmoothing & 0x1)<< 16);
    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_SPS_1, u4SPSInfo);

    
    u4SPSInfo = 0;
    u4SPSInfo = ((prSPS->u4Log2MinCodingBlockSizeMinus3+3) & 0x7);
    u4SPSInfo |= ((log2_max_cu_size & 0x7)<< 4);
    u4SPSInfo |= (((prSPS->u4Log2MinTransformBlockSizeMinus2+2)& 0x7)<< 8);
    u4SPSInfo |= ((log2_max_tu_size & 0x7)<< 12);
    u4SPSInfo |= (((prSPS->u4PCMLog2LumaMinSizeMinus3+3) & 0x7)<< 16);
    u4SPSInfo |= (((prSPS->u4PCMLog2LumaMaxSize)& 0x7)<< 20);
    u4SPSInfo |= (((prSPS->u4QuadtreeTUMaxDepthInter -1)& 0x7)<< 24);
    u4SPSInfo |= (((prSPS->u4QuadtreeTUMaxDepthIntra -1)& 0x7)<< 28);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_SPS_SIZE, u4SPSInfo);
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] vVDEC_HAL_H265_SetSPSHEVLD() done!!\n");
#endif

#if 0 //HUB MT8137 HEVC
    if ((prSPS->u4BitDepthChromaMinus8 == 2) && (prSPS->u4BitDepthLumaMinus8 == 2))
    {
        vVDecWriteHEVCPP(u4VDecID, 740*4, 1);
    }
#endif
}


// **************************************************************************
// Function : void vVDEC_HAL_H265_SetPPSHEVLD(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
// Description :Set PPS data to HW
// Parameter : u4VDecID : video decoder hardware ID
//                   prSPS
//                   prPPS : pointer to picture parameter set struct
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetPPSHEVLD(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
{
    UINT32 u4PPSInfo;
    INT32 i;
    int log2_min_cu_qp_delta_size;
    int log2_max_cu_size;
    BOOL bIsUFO;  
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] vVDEC_HAL_H265_SetPPSHEVLD() start!!\n");
#endif
   
    bIsUFO = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode;
    log2_max_cu_size =  prSPS->u4Log2DiffMaxMinCodingBlockSize + prSPS->u4Log2MinCodingBlockSizeMinus3+3;

    u4PPSInfo = 0;
    u4PPSInfo = (prPPS->bSignHideFlag & 0x1);
    u4PPSInfo |= ((prPPS->bCabacInitPresentFlag & 0x1)<< 1);
    u4PPSInfo |= ((prPPS->bConstrainedIntraPredFlag & 0x1)<< 2);
    u4PPSInfo |= ((prPPS->bTransformSkipEnabledFlag & 0x1)<< 3);
    u4PPSInfo |= ((prPPS->bWPPredFlag& 0x1)<< 4);
    u4PPSInfo |= ((prPPS->bWPBiPredFlag& 0x1)<< 5);
    u4PPSInfo |= ((prPPS->bOutputFlagPresentFlag & 0x1)<< 6);
    u4PPSInfo |= ((prPPS->bTransquantBypassEnableFlag & 0x1)<< 7);
    u4PPSInfo |= ((prPPS->bDependentSliceSegmentsEnabledFlag & 0x1)<< 8);
    u4PPSInfo |= ((prPPS->bEntropyCodingSyncEnabledFlag & 0x1)<< 9);
    u4PPSInfo |= ((prPPS->bSliceHeaderExtensionPresentFlag & 0x1)<< 11);
    u4PPSInfo |= ((prPPS->u4NumRefIdxL0DefaultActiveMinus1& 0xf)<< 16);
    u4PPSInfo |= ((prPPS->u4NumRefIdxL1DefaultActiveMinus1& 0xf)<< 20);
    u4PPSInfo |= (((prPPS->u4Log2ParallelMergeLevelMinus2+2)& 0x7)<< 24);
    u4PPSInfo |= ((prPPS->u4NumExtraSliceHeaderBits & 0x7)<< 28);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_PPS, u4PPSInfo);

    log2_min_cu_qp_delta_size = log2_max_cu_size - (prPPS->u4DiffCuQPDeltaDepth);

    u4PPSInfo = 0;
    u4PPSInfo = (prPPS->bCuQPDeltaEnabledFlag & 0x1);
    u4PPSInfo |= ((prPPS->bPPSSliceChromaQpFlag & 0x1)<< 1);
    u4PPSInfo |= ((log2_min_cu_qp_delta_size & 0x7)<< 4);

#if 0 //HUB MT8137 HEVC
    if ((_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.u4ProfileIdc == 2)
        || (_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.bProfileCompatibilityFlag[2] == 1))
    {
        u4PPSInfo |= (((prPPS->i4PicInitQPMinus26+26) & 0x7f)<< 8);
    }
    else
    {
        u4PPSInfo |= (((prPPS->i4PicInitQPMinus26+26) & 0x3f)<< 8);
    }
#else
    if ((prSPS->u4BitDepthLumaMinus8 == 0) && (prSPS->u4BitDepthChromaMinus8 == 0))
    {
        u4PPSInfo |= (((prPPS->i4PicInitQPMinus26+26) & 0x3f)<< 8);
    }
    else // 10 bit
    {
        u4PPSInfo |= (((prPPS->i4PicInitQPMinus26+26) & 0x7f)<< 8);
    }
#endif

    u4PPSInfo |= ((prPPS->i4PPSCbQPOffset & 0x1f)<< 16);
    u4PPSInfo |= ((prPPS->i4PPSCrQPOffset & 0x1f)<< 24);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_PPS_QP, u4PPSInfo);

    u4PPSInfo = 0;
    u4PPSInfo = (prPPS->bTilesEnabledFlag & 0x1);
    u4PPSInfo |= ((prPPS->u4NumColumnsMinus1 & 0x1f)<< 8);
    u4PPSInfo |= ((prPPS->u4NumRowsMinus1 & 0x1f)<< 16);
    u4PPSInfo |= ((prPPS->bLoopFilterAcrossTilesEnabledFlag & 0x1)<< 24);
    
    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_PPS_TILE , u4PPSInfo);

    u4PPSInfo = 0;
    u4PPSInfo = (prPPS->bLoopFilterAcrossSlicesEnabledFlag & 0x1);
    u4PPSInfo |= ((prPPS->bDeblockingFilterControlPresentFlag & 0x1)<< 1);
    u4PPSInfo |= ((prPPS->bDeblockingFilterOverrideEnabledFlag & 0x1)<< 2);
    u4PPSInfo |= ((prPPS->bPicDisableDeblockingFilterFlag & 0x1)<< 3);
    u4PPSInfo |= ((prPPS->i4DeblockingFilterBetaOffsetDiv2 & 0x1f)<< 4);     //[notice] initial value need to be check
    u4PPSInfo |= ((prPPS->i4DeblockingFilterTcOffsetDiv2 & 0x1f)<< 9);      //

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVLD_PPS_DBK , u4PPSInfo);

    if (bIsUFO){
        BOOL bUFOEnc;
        BOOL bUFODec;
        UINT32 u4RefUFOEncoded;
        int i;

#if 1 //HUB MT8137 HEVC 
        _ptH265CurrFBufInfo[u4VDecID]->bIsUFOEncoded = 1;
        bUFOEnc = 1;
#else
        if ( prPPS->bTilesEnabledFlag ){
            _ptH265CurrFBufInfo[u4VDecID]->bIsUFOEncoded = 0;
            bUFOEnc = 0;
        } else {
            _ptH265CurrFBufInfo[u4VDecID]->bIsUFOEncoded = 1;
            bUFOEnc = 1;
        }
#endif
#if 0 //HUB MT8137 HEVC
        u4RefUFOEncoded = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.u4RefUFOEncoded;
        if (u4RefUFOEncoded==0){
            bUFODec = 0;
            vVDecWriteMC(u4VDecID, 664*4, (bUFODec<<4)|bUFOEnc);
        } else {
            bUFODec = 1;
            vVDecWriteMC(u4VDecID, 664*4, (bUFODec<<4)|bUFOEnc);
            vVDecWriteMC(u4VDecID, 722*4, 0x1);
            vVDecWriteMC(u4VDecID, 718*4, u4RefUFOEncoded);
            vVDecWriteMC(u4VDecID, 719*4, u4RefUFOEncoded);
            vVDecWriteMC(u4VDecID, 720*4, u4RefUFOEncoded);
        }       
        
        vVDecWriteHEVCPP(u4VDecID, 706*4, 0x1 );   // UFO garbage remove

        #ifdef VDEC_SIM_DUMP
            printk ("[INFO] UFO Mode !! Encoder: %d Decoder: %d\n", bUFOEnc, bUFODec);
        #endif
#endif
    }
    
#ifdef VDEC_SIM_DUMP
    printk ("[INFO] vVDEC_HAL_H265_SetPPSHEVLD() done!!\n");
#endif

}


// **************************************************************************
// Function :void vVDEC_HAL_H265_SetSHDRHEVLD(UINT32 u4VDecID, H265_Slice_Hdr_Data *prSliceHdr , BOOL bUseSAO, H265_PPS_Data *prPPS)
// Description :Set part of slice header data to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 prSliceHdr : pointer to slice parameter set struct
//                 bUseSAO, prPPS
// Return      : None
// **************************************************************************
#if 0
void vVDEC_HAL_H265_SetSHDRHEVLD(UINT32 u4VDecID, H265_Slice_Hdr_Data *prSliceHdr, BOOL bUseSAO, H265_PPS_Data *prPPS)
#else
void vVDEC_HAL_H265_SetSHDRHEVLD(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_Slice_Hdr_Data *prSliceHdr, BOOL bUseSAO, H265_PPS_Data *prPPS)
#endif
{
    UINT32 u4SHDRInfo;
    int i4Indexer;
    int i4NumRpsCurrTempList = 0;
    int numRpsCurrTempList0 = 0;
    int length = 0;
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] vVDEC_HAL_H265_SetSHDRHEVLD() start!!\n");
#endif

    u4SHDRInfo = 0;
    u4SHDRInfo = ((bUseSAO & prSliceHdr->bSaoEnabledFlag & 0x1) << 5);
    u4SHDRInfo |= ((bUseSAO & prSliceHdr->bSaoEnabledFlagChroma & 0x1) << 6);
    u4SHDRInfo |= ((prSliceHdr->bCabacInitFlag & 0x1) << 7);

#if 0 //HUB MT8137 HEVC
    if ((_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.u4ProfileIdc == 2)
        || (_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS->rSPS_PTL.generalPTL.bProfileCompatibilityFlag[2] == 1))
    {
        u4SHDRInfo |= ((prSliceHdr->i4SliceQp & 0x7f) << 8);
    }
    else
    {
        u4SHDRInfo |= ((prSliceHdr->i4SliceQp & 0x3f) << 8);
    }
#else
    // 8 bit
    if (prSPS->u4BitDepthLumaMinus8 == 0 && prSPS->u4BitDepthChromaMinus8 == 0)
    {
        u4SHDRInfo |= ((prSliceHdr->i4SliceQp & 0x3f) << 8);
    }
    else // 10 bit    
    {
        u4SHDRInfo |= ((prSliceHdr->i4SliceQp & 0x7f) << 8);
    }
#endif

    u4SHDRInfo |= (((prPPS->i4PPSCbQPOffset + prSliceHdr->i4SliceQpDeltaCb) & 0x1f) << 16);
    u4SHDRInfo |= (((prPPS->i4PPSCrQPOffset + prSliceHdr->i4SliceQpDeltaCr) & 0x1f) << 24);
    
    vVDecWriteHEVCVLD(u4VDecID, RW_HEVC_SLICE_1 , u4SHDRInfo);

    u4SHDRInfo = 0;
    u4SHDRInfo = (((prSliceHdr->i4NumRefIdx[0]-1) & 0xf) << 0) ;
    u4SHDRInfo |= (((prSliceHdr->i4NumRefIdx[1]-1) & 0xf) << 4);
    u4SHDRInfo |= ((prSliceHdr->u4ColRefIdx & 0xf) << 8) ;
    u4SHDRInfo |= (((5-prSliceHdr->u4FiveMinusMaxNumMergeCand) & 0x7) << 12);
    u4SHDRInfo |= ((prSliceHdr->i4DeblockingFilterBetaOffsetDiv2 & 0x1f) << 16);        //[notice] initial value need to be check
    u4SHDRInfo |= ((prSliceHdr->i4DeblockingFilterTcOffsetDiv2 & 0x1f) << 21);          //
    u4SHDRInfo |= ((prSliceHdr->bTMVPFlagsPresent & 0x1) << 26);
    u4SHDRInfo |= ((prSliceHdr->bColFromL0Flag & 0x1) << 27);
    u4SHDRInfo |= ((prSliceHdr->bMvdL1ZeroFlag & 0x1) << 28);
    u4SHDRInfo |= ((prSliceHdr->bLoopFilterAcrossSlicesEnabledFlag & 0x1) << 29);
    u4SHDRInfo |= ((prSliceHdr->bDeblockingFilterDisableFlag & 0x1) << 30);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVC_SLICE_2, u4SHDRInfo);

    u4SHDRInfo = 0;
    u4SHDRInfo = (( _rH265PicInfo[u4VDecID].i4StrNumDeltaPocs & 0x1f) << 0);
    u4SHDRInfo |= ((( _rH265PicInfo[u4VDecID].i4StrNumNegPosPics) & 0x3f) << 8);
    u4SHDRInfo |= (( _rH265PicInfo[u4VDecID].i4NumLongTerm & 0x1f) << 16);
    u4SHDRInfo |= (( _rH265PicInfo[u4VDecID].i4NumLongTermSps & 0x1f) << 24);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVC_SLICE_STR_LT, u4SHDRInfo);

    // calculate i4NumRpsCurrTempList
    for( i4Indexer=0; i4Indexer < prSliceHdr->pShortTermRefPicSets->u4NumberOfNegativePictures 
            +prSliceHdr->pShortTermRefPicSets->u4NumberOfPositivePictures
            +prSliceHdr->pShortTermRefPicSets->u4NumberOfLongtermPictures; i4Indexer++) {
        if(prSliceHdr->pShortTermRefPicSets->bUsed[i4Indexer]) {
            i4NumRpsCurrTempList++;
        }
    }

    numRpsCurrTempList0 = i4NumRpsCurrTempList;
    if ( numRpsCurrTempList0 > 1 ){
            length = 1;
            numRpsCurrTempList0 --;
    }
    while ( numRpsCurrTempList0 >>= 1) {
        length ++;
    }

    u4SHDRInfo = 0;
    u4SHDRInfo = ((i4NumRpsCurrTempList & 0xf) << 0) | ((length & 0x7) << 4);

    vVDecWriteHEVCVLD(u4VDecID, RW_HEVC_REF_PIC_LIST_MOD, u4SHDRInfo);

    u4SHDRInfo = 0;
    u4SHDRInfo  = (0xffff << 16) | (prSliceHdr->u4NalType<<8) |(21);
    
    vVDecWriteHEVCVLD(u4VDecID, RW_HEVC_ERR_DET_CTRL, u4SHDRInfo);
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] vVDEC_HAL_H265_SetSHDRHEVLD() done!!\n");
#endif

}

// **************************************************************************
// Function :void vVDEC_HAL_H265_SetSLPP(UINT32 u4VDecID, pH265_SL_Data ScallingList)
// Description :Set scaling list data to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 ScallingList : scaling list data
/// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetSLPP(UINT32 u4VDecID, pH265_SL_Data ScallingList)
{
    UINT32  u4SLInfo;
    
    _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt=0;
    
    if(_rH265PicInfo[u4VDecID].u4SliceCnt==0)
    {
        u4SLInfo = 0;
        u4SLInfo = ((ScallingList->i4ScalingListDC[2][0] & 0xff )<< 0);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[2][1] & 0xff )<< 8);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[2][2] & 0xff )<<16);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[2][3] & 0xff )<<24);
        
        vVDecWriteHEVCPP(u4VDecID, HEVC_IQ_SACLING_FACTOR_DC_0, u4SLInfo );

        u4SLInfo = 0;
        u4SLInfo = ((ScallingList->i4ScalingListDC[2][4] & 0xff )<< 0);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[2][5] & 0xff )<< 8);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[3][0] & 0xff )<<16);
        u4SLInfo |= ((ScallingList->i4ScalingListDC[3][1] & 0xff )<<24);

        vVDecWriteHEVCPP(u4VDecID, HEVC_IQ_SACLING_FACTOR_DC_1, u4SLInfo );
    }

}


// **************************************************************************
// Function :void vVDEC_HAL_H265_SetSLVLD(UINT32 u4VDecID, INT32 *coeff, INT32 width, INT32 height, INT32 invQuantScales)
// Description :Set scaling list data to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 coeff : scaling list coefficient
//                 width, height, invQuantScales
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetSLVLD(UINT32 u4VDecID, INT32 *coeff, INT32 width, INT32 height, INT32 invQuantScales)
{
    int index;
    UINT32  u4SLInfo;

    if((invQuantScales==40) && (width==height))
    { 
        if(width==4) //4x4
        {
            for( index=0; index<4; index++)
            {           
                  if(_rH265PicInfo[u4VDecID].u4SliceCnt==0)
                  {
                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_ADDR, _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt );
                      u4SLInfo = 0;
                      u4SLInfo =  (coeff[0*4+index] & 0xff )<< 0;
                      u4SLInfo |= (coeff[1*4+index] & 0xff )<< 8;
                      u4SLInfo |= (coeff[2*4+index] & 0xff )<< 16;
                      u4SLInfo |= (coeff[3*4+index] & 0xff )<< 24;
                     
                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_DATA, u4SLInfo );
                  }
                  _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt++;
            }
    
      }
      else //8x8, 16x16, 32x32
      {
            for( index=0; index<8; index++)
            {
                  if(_rH265PicInfo[u4VDecID].u4SliceCnt==0)
                  {
                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_ADDR, _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt );
                      u4SLInfo = 0;
                      u4SLInfo =  (coeff[0*8+index] & 0xff )<< 0;
                      u4SLInfo |= (coeff[1*8+index] & 0xff )<< 8;
                      u4SLInfo |= (coeff[2*8+index] & 0xff )<< 16;
                      u4SLInfo |= (coeff[3*8+index] & 0xff )<< 24;
                     
                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_DATA, u4SLInfo );
                  }
                  _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt++;
    
                  if(_rH265PicInfo[u4VDecID].u4SliceCnt==0)
                  {
                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_ADDR, _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt );
                      u4SLInfo = 0;
                      u4SLInfo =  (coeff[4*8+index] & 0xff )<< 0;
                      u4SLInfo |= (coeff[5*8+index] & 0xff )<< 8;
                      u4SLInfo |= (coeff[6*8+index] & 0xff )<< 16;
                      u4SLInfo |= (coeff[7*8+index] & 0xff )<< 24;

                      vVDecWriteVLD(u4VDecID, HEVC_IQ_SRAM_DATA, u4SLInfo );
                  }
                  _rH265PicInfo[u4VDecID].u4IqSramAddrAccCnt++;
            }
        }   
    }


}



// **************************************************************************
// Function : void vVDEC_HAL_H265_SetTilesInfo(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
//// Description :Set Tiles Info to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 prSPS
//                 prPPS : pointer to picture parameter set struct
//              
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetTilesInfo(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
{
    UINT32  uiColumnIdx = 0;
    UINT32  uiRowIdx = 0;
    UINT32  uiRightEdgePosInCU;
    UINT32  uiBottomEdgePosInCU;
    
    UINT32  last_lcu_x_in_tile_0_reg = 0;
    UINT32  last_lcu_x_in_tile_1_reg = 0;
    UINT32  last_lcu_x_in_tile_2_reg = 0;
    UINT32  last_lcu_x_in_tile_3_reg = 0;
    UINT32  last_lcu_x_in_tile_4_reg = 0;
    UINT32  last_lcu_x_in_tile_5_reg = 0;
    UINT32  last_lcu_x_in_tile_6_reg = 0;
    UINT32  last_lcu_x_in_tile_7_reg = 0;
    UINT32  last_lcu_x_in_tile_8_reg = 0;
    UINT32  last_lcu_x_in_tile_9_reg = 0;
    UINT32  last_lcu_x_in_tile_10_reg = 0;
    UINT32  last_lcu_x_in_tile_11_reg = 0;
    UINT32  last_lcu_x_in_tile_12_reg = 0;
    UINT32  last_lcu_x_in_tile_13_reg = 0;
    UINT32  last_lcu_x_in_tile_14_reg = 0;
    UINT32  last_lcu_x_in_tile_15_reg = 0;
    UINT32  last_lcu_x_in_tile_16_reg = 0;
    
    UINT32  last_lcu_y_in_tile_0_reg = 0;
    UINT32  last_lcu_y_in_tile_1_reg = 0;
    UINT32  last_lcu_y_in_tile_2_reg = 0;
    UINT32  last_lcu_y_in_tile_3_reg = 0;
    UINT32  last_lcu_y_in_tile_4_reg = 0;
    UINT32  last_lcu_y_in_tile_5_reg = 0;
    UINT32  last_lcu_y_in_tile_6_reg = 0;
    UINT32  last_lcu_y_in_tile_7_reg = 0;
    UINT32  last_lcu_y_in_tile_8_reg = 0;
    UINT32  last_lcu_y_in_tile_9_reg = 0;
    UINT32  last_lcu_y_in_tile_10_reg = 0;
    UINT32  last_lcu_y_in_tile_11_reg = 0;
    UINT32  last_lcu_y_in_tile_12_reg = 0;
    UINT32  last_lcu_y_in_tile_13_reg = 0;
    UINT32  last_lcu_y_in_tile_14_reg = 0;
    UINT32  last_lcu_y_in_tile_15_reg = 0;   
    UINT32  last_lcu_y_in_tile_16_reg = 0;
    
    for (uiColumnIdx = 0; uiColumnIdx < prPPS->u4NumColumnsMinus1 + 1; uiColumnIdx++ )
    {
#if 1 //HUB MT8137 HEVC
        uiRightEdgePosInCU = _rH265PicInfo[u4VDecID].rTileInfo[uiColumnIdx].u4RightEdgePosInCU;

        if (uiRightEdgePosInCU >= 512)
            last_lcu_x_in_tile_16_reg = last_lcu_x_in_tile_16_reg | (1<<(uiRightEdgePosInCU-512));
        else if (uiRightEdgePosInCU >= 480)
            last_lcu_x_in_tile_15_reg = last_lcu_x_in_tile_15_reg | (1<<(uiRightEdgePosInCU-480));
        else if (uiRightEdgePosInCU >= 448)
            last_lcu_x_in_tile_14_reg = last_lcu_x_in_tile_14_reg | (1<<(uiRightEdgePosInCU-448));
        else if (uiRightEdgePosInCU >= 416)
            last_lcu_x_in_tile_13_reg = last_lcu_x_in_tile_13_reg | (1<<(uiRightEdgePosInCU-416));
        else if (uiRightEdgePosInCU >= 384)
            last_lcu_x_in_tile_12_reg = last_lcu_x_in_tile_12_reg | (1<<(uiRightEdgePosInCU-384));
        else if (uiRightEdgePosInCU >= 352)
            last_lcu_x_in_tile_11_reg = last_lcu_x_in_tile_11_reg | (1<<(uiRightEdgePosInCU-352));
        else if (uiRightEdgePosInCU >= 320)
            last_lcu_x_in_tile_10_reg = last_lcu_x_in_tile_10_reg | (1<<(uiRightEdgePosInCU-320));
        else if (uiRightEdgePosInCU >= 288)
            last_lcu_x_in_tile_9_reg = last_lcu_x_in_tile_9_reg | (1<<(uiRightEdgePosInCU-288));
        else if (uiRightEdgePosInCU >= 256)
            last_lcu_x_in_tile_8_reg = last_lcu_x_in_tile_8_reg | (1<<(uiRightEdgePosInCU-256));
        else if (uiRightEdgePosInCU >= 224)
            last_lcu_x_in_tile_7_reg = last_lcu_x_in_tile_7_reg | (1<<(uiRightEdgePosInCU-224));
        else if (uiRightEdgePosInCU >= 192)
            last_lcu_x_in_tile_6_reg = last_lcu_x_in_tile_6_reg | (1<<(uiRightEdgePosInCU-192));
        else if (uiRightEdgePosInCU >= 160)
            last_lcu_x_in_tile_5_reg = last_lcu_x_in_tile_5_reg | (1<<(uiRightEdgePosInCU-160));
        else if (uiRightEdgePosInCU >= 128)
            last_lcu_x_in_tile_4_reg = last_lcu_x_in_tile_4_reg | (1<<(uiRightEdgePosInCU-128));
        else if (uiRightEdgePosInCU >= 96)
            last_lcu_x_in_tile_3_reg = last_lcu_x_in_tile_3_reg | (1<<(uiRightEdgePosInCU-96));
        else if (uiRightEdgePosInCU >= 64)
            last_lcu_x_in_tile_2_reg = last_lcu_x_in_tile_2_reg | (1<<(uiRightEdgePosInCU-64));
        else if (uiRightEdgePosInCU >= 32)
            last_lcu_x_in_tile_1_reg = last_lcu_x_in_tile_1_reg | (1<<(uiRightEdgePosInCU-32));
        else
            last_lcu_x_in_tile_0_reg = last_lcu_x_in_tile_0_reg | (1<<(uiRightEdgePosInCU-0));
#else
        uiRightEdgePosInCU = _rH265PicInfo[u4VDecID].rTileInfo[uiColumnIdx].u4RightEdgePosInCU;
        if (uiRightEdgePosInCU >= 224)
            last_lcu_x_in_tile_7_reg = last_lcu_x_in_tile_7_reg | (1<<(uiRightEdgePosInCU-224));
        else if (uiRightEdgePosInCU >= 192)
            last_lcu_x_in_tile_6_reg = last_lcu_x_in_tile_6_reg | (1<<(uiRightEdgePosInCU-192));
        else if (uiRightEdgePosInCU >= 160)
            last_lcu_x_in_tile_5_reg = last_lcu_x_in_tile_5_reg | (1<<(uiRightEdgePosInCU-160));
        else if (uiRightEdgePosInCU >= 128)
            last_lcu_x_in_tile_4_reg = last_lcu_x_in_tile_4_reg | (1<<(uiRightEdgePosInCU-128));
        else if (uiRightEdgePosInCU >= 96)
            last_lcu_x_in_tile_3_reg = last_lcu_x_in_tile_3_reg | (1<<(uiRightEdgePosInCU-96));
        else if (uiRightEdgePosInCU >= 64)
            last_lcu_x_in_tile_2_reg = last_lcu_x_in_tile_2_reg | (1<<(uiRightEdgePosInCU-64));
        else if (uiRightEdgePosInCU >= 32)
            last_lcu_x_in_tile_1_reg = last_lcu_x_in_tile_1_reg | (1<<(uiRightEdgePosInCU-32));
        else
            last_lcu_x_in_tile_0_reg = last_lcu_x_in_tile_0_reg | (1<<(uiRightEdgePosInCU-0));
#endif
    }
    
    for( uiRowIdx=0; uiRowIdx < prPPS->u4NumRowsMinus1+1; uiRowIdx++ )
    {
#if 1 //HUB MT8137 HEVC
        uiBottomEdgePosInCU = _rH265PicInfo[u4VDecID].rTileInfo[uiRowIdx*(prPPS->u4NumColumnsMinus1+1)].u4BottomEdgePosInCU;

        if (uiBottomEdgePosInCU >= 512)
            last_lcu_y_in_tile_16_reg = last_lcu_y_in_tile_16_reg | (1<<(uiBottomEdgePosInCU-512));
        else if (uiBottomEdgePosInCU >= 480)
            last_lcu_y_in_tile_15_reg = last_lcu_y_in_tile_15_reg | (1<<(uiBottomEdgePosInCU-480));
        else if (uiBottomEdgePosInCU >= 448)
            last_lcu_y_in_tile_14_reg = last_lcu_y_in_tile_14_reg | (1<<(uiBottomEdgePosInCU-448));
        else if (uiBottomEdgePosInCU >= 416)
            last_lcu_y_in_tile_13_reg = last_lcu_y_in_tile_13_reg | (1<<(uiBottomEdgePosInCU-416));
        else if (uiBottomEdgePosInCU >= 384)
            last_lcu_y_in_tile_12_reg = last_lcu_y_in_tile_12_reg | (1<<(uiBottomEdgePosInCU-384));
        else if (uiBottomEdgePosInCU >= 352)
            last_lcu_y_in_tile_11_reg = last_lcu_y_in_tile_11_reg | (1<<(uiBottomEdgePosInCU-352));
        else if (uiBottomEdgePosInCU >= 320)
            last_lcu_y_in_tile_10_reg = last_lcu_y_in_tile_10_reg | (1<<(uiBottomEdgePosInCU-320));
        else if (uiBottomEdgePosInCU >= 288)
            last_lcu_y_in_tile_9_reg = last_lcu_y_in_tile_9_reg | (1<<(uiBottomEdgePosInCU-288));
        else if (uiBottomEdgePosInCU >= 256)
            last_lcu_y_in_tile_8_reg = last_lcu_y_in_tile_8_reg | (1<<(uiBottomEdgePosInCU-256));
        else if (uiBottomEdgePosInCU >= 224)
            last_lcu_y_in_tile_7_reg = last_lcu_y_in_tile_7_reg | (1<<(uiBottomEdgePosInCU-224));        
        else if (uiBottomEdgePosInCU >= 192)
            last_lcu_y_in_tile_6_reg = last_lcu_y_in_tile_6_reg | (1<<(uiBottomEdgePosInCU-192));        
        else if (uiBottomEdgePosInCU >= 160)
            last_lcu_y_in_tile_5_reg = last_lcu_y_in_tile_5_reg | (1<<(uiBottomEdgePosInCU-160));
        else if (uiBottomEdgePosInCU >= 128)
            last_lcu_y_in_tile_4_reg = last_lcu_y_in_tile_4_reg | (1<<(uiBottomEdgePosInCU-128));
        else if (uiBottomEdgePosInCU >= 96)
            last_lcu_y_in_tile_3_reg = last_lcu_y_in_tile_3_reg | (1<<(uiBottomEdgePosInCU-96));
        else if (uiBottomEdgePosInCU >= 64)
            last_lcu_y_in_tile_2_reg = last_lcu_y_in_tile_2_reg | (1<<(uiBottomEdgePosInCU-64));
        else if (uiBottomEdgePosInCU >= 32)
            last_lcu_y_in_tile_1_reg = last_lcu_y_in_tile_1_reg | (1<<(uiBottomEdgePosInCU-32));
        else
            last_lcu_y_in_tile_0_reg = last_lcu_y_in_tile_0_reg | (1<<(uiBottomEdgePosInCU-0));
#else
        uiBottomEdgePosInCU = _rH265PicInfo[u4VDecID].rTileInfo[uiRowIdx*(prPPS->u4NumColumnsMinus1+1)].u4BottomEdgePosInCU;
        if (uiBottomEdgePosInCU >= 96)
            last_lcu_y_in_tile_3_reg = last_lcu_y_in_tile_3_reg | (1<<(uiBottomEdgePosInCU-96));
        else if (uiBottomEdgePosInCU >= 64)
            last_lcu_y_in_tile_2_reg = last_lcu_y_in_tile_2_reg | (1<<(uiBottomEdgePosInCU-64));
        else if (uiBottomEdgePosInCU >= 32)
            last_lcu_y_in_tile_1_reg = last_lcu_y_in_tile_1_reg | (1<<(uiBottomEdgePosInCU-32));
        else
            last_lcu_y_in_tile_0_reg = last_lcu_y_in_tile_0_reg | (1<<(uiBottomEdgePosInCU-0));
#endif
    }
    
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_0,last_lcu_x_in_tile_0_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_1,last_lcu_x_in_tile_1_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_2,last_lcu_x_in_tile_2_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_3,last_lcu_x_in_tile_3_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_4,last_lcu_x_in_tile_4_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_5,last_lcu_x_in_tile_5_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_6,last_lcu_x_in_tile_6_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_7,last_lcu_x_in_tile_7_reg);
#if 1 //HUB MT8137 HEVC
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_8,last_lcu_x_in_tile_8_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_9,last_lcu_x_in_tile_9_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_10,last_lcu_x_in_tile_10_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_11,last_lcu_x_in_tile_11_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_12,last_lcu_x_in_tile_12_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_13,last_lcu_x_in_tile_13_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_14,last_lcu_x_in_tile_14_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_15,last_lcu_x_in_tile_15_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_X_16,last_lcu_x_in_tile_16_reg);
#endif

    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_0,last_lcu_y_in_tile_0_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_1,last_lcu_y_in_tile_1_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_2,last_lcu_y_in_tile_2_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_3,last_lcu_y_in_tile_3_reg);
#if 1 //HUB MT8137 HEVC
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_4,last_lcu_y_in_tile_4_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_5,last_lcu_y_in_tile_5_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_6,last_lcu_y_in_tile_6_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_7,last_lcu_y_in_tile_7_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_8,last_lcu_y_in_tile_8_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_9,last_lcu_y_in_tile_9_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_10,last_lcu_y_in_tile_10_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_11,last_lcu_y_in_tile_11_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_12,last_lcu_y_in_tile_12_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_13,last_lcu_y_in_tile_13_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_14,last_lcu_y_in_tile_14_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_15,last_lcu_y_in_tile_15_reg);
    vVDecWriteHEVCVLD(u4VDecID,RW_HEVC_TILE_Y_16,last_lcu_y_in_tile_16_reg);
#endif
}
    
#ifdef  HEVC_STILL_IMAGE
// **************************************************************************
// Function : void vVDEC_HAL_H265_SetStillImageInfo(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
//// Description :Set Still image Info to HW
// Parameter : u4VDecID : video decoder hardware ID
//                 prSPS
//                 prPPS : pointer to picture parameter set struct
//              
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_SetStillImageInfo(UINT32 u4VDecID, H265_SPS_Data *prSPS, H265_PPS_Data *prPPS)
{
    vVDecWriteVLDTOP(u4VDecID, 11*4, 0xc0000000);  
    vVDecWriteVLDTOP(u4VDecID, 16*4, 0x3);
    vVDecWriteVLDTOP(u4VDecID, 10*4, PHYSICAL(_pucVLDPredBuf[u4VDecID]));
    vVDecWriteVLDTOP(u4VDecID, 38*4, PHYSICAL(_pucVLDIPBuf[u4VDecID]));
    printk("[INFO] VLDTOP_38 [0x%x] pucVLDIPBuf [0x%x]\n", u4VDecReadVLDTOP(u4VDecID, 38*4), PHYSICAL(_pucVLDIPBuf[u4VDecID]));
    vVDecWriteVLDTOP(u4VDecID, 46*4, PHYSICAL(_pucVLDVBuf[u4VDecID]));
    printk("[INFO] VLDTOP_46 [0x%x] pucVLDVBuf [0x%x] \n", u4VDecReadVLDTOP(u4VDecID, 46*4), PHYSICAL(_pucVLDVBuf[u4VDecID]));

    vVDecWriteHEVCPP(u4VDecID, 771*4, 0x1);
    vVDecWriteHEVCPP(u4VDecID, 769*4, PHYSICAL(_pucPPSAOBuf[u4VDecID]));
    printk("[INFO] PP_769 [0x%x] _pucPPSAOBuf [0x%x]\n", u4VDecReadHEVCPP(u4VDecID, 769*4), PHYSICAL(_pucPPSAOBuf[u4VDecID]));
    vVDecWriteHEVCPP(u4VDecID, 770*4, PHYSICAL(_pucPPDBKBuf[u4VDecID]));
    printk("[INFO] PP_770 [0x%x] _pucPPDBKBuf [0x%x]\n", u4VDecReadVLDTOP(u4VDecID, 770*4), PHYSICAL(_pucPPDBKBuf[u4VDecID]));
    vVDecWriteHEVCPP(u4VDecID, 45*4, 0);
    vVDecWriteHEVCPP(u4VDecID, 46*4, u4VDecReadHEVCPP(u4VDecID, 45*4) + 4224);
    vVDecWriteHEVCPP(u4VDecID, 47*4, u4VDecReadHEVCPP(u4VDecID, 46*4) + 4224);
    vVDecWriteHEVCPP(u4VDecID, 48*4, u4VDecReadHEVCPP(u4VDecID, 47*4) + 4224);
    vVDecWriteHEVCPP(u4VDecID, 49*4, u4VDecReadHEVCPP(u4VDecID, 48*4) + 2112);
    vVDecWriteHEVCPP(u4VDecID, 50*4, u4VDecReadHEVCPP(u4VDecID, 49*4) + 1056);
    vVDecWriteHEVCPP(u4VDecID, 51*4, u4VDecReadHEVCPP(u4VDecID, 50*4) + 1056);
    vVDecWriteHEVCPP(u4VDecID, 52*4, 8448);

    vVDecWriteHEVCPP(u4VDecID, 705*4, 528);
    vVDecWriteHEVCPP(u4VDecID, 706*4, 1584);
    vVDecWriteHEVCPP(u4VDecID, 707*4, 2112);
}
#endif 

// **************************************************************************
// Function : INT32 i4VDEC_HAL_H265_DecStart(UINT32 u4VDecID, VDEC_INFO_DEC_PRM_T *prDecPrm);
// Description :Set video decoder hardware registers to decode for H265
// Parameter : ptHalDecH265Info : pointer to H265 decode info struct
// Return      : =0: success.
//                  <0: fail.
// **************************************************************************
INT32 i4VDEC_HAL_H265_DecStart(UINT32 u4VDecID, VDEC_INFO_DEC_PRM_T *prDecPrm)
{
    printk("// input_window=0x%x \n", u4VDecReadHEVCVLD(u4VDecID, 0x0));
    vVDecWriteHEVCVLD( u4VDecID, HEVLD_PIC_TRG_REG, 0x1);

    return HAL_HANDLE_OK;
}


// **************************************************************************
// Function : void v4VDEC_HAL_H265_GetMbxMby(UINT32 u4VDecID, UINT32 *pu4Mbx, UINT32 *pu4Mby);
// Description :Read current decoded mbx and mby
// Parameter : u4VDecID : video decoder hardware ID
//                 u4Mbx : macroblock x value
//                 u4Mby : macroblock y value
// Return      : None
// **************************************************************************
void vVDEC_HAL_H265_GetMbxMby(UINT32 u4VDecID, UINT32 *pu4Mbx, UINT32 *pu4Mby)
{
    *pu4Mbx = u4VDecReadMC(u4VDecID, RO_MC_MBX);
    *pu4Mby = u4VDecReadMC(u4VDecID, RO_MC_MBY);
}


// **************************************************************************
// Function : BOOL fgVDEC_HAL_H265_DecPicComplete(UINT32 u4VDecID);
// Description :Check if all video decoder modules are complete
// Parameter : u4VDecID : video decoder hardware ID
// Return      : TRUE: Decode complete, FALSE: Not yet
// **************************************************************************
BOOL fgVDEC_HAL_H265_DecPicComplete(UINT32 u4VDecID)
{
    if(u4VDecReadHEVCVLD(u4VDecID, RO_HEVLD_STATE_INFO) & HEVLD_PIC_FINISH)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

// **************************************************************************
// Function : void u4VDEC_HAL_H265_GetErrMsg(UINT32 u4VDecID);
// Description :Read h265 error message after decoding end
// Parameter : u4VDecID : video decoder hardware ID
// Return      : H265 decode error message
// **************************************************************************
UINT32 u4VDEC_HAL_H265_GetErrMsg(UINT32 u4VDecID)
{
    return u4VDecReadHEVCVLD(u4VDecID, RO_HEVLD_ERR_ACCUMULATOR);
}


// **************************************************************************
// Function : void u4VDEC_HAL_H265_GetErrMsg(UINT32 u4VDecID);
// Description :Read h265 error message after decoding end
// Parameter : u4VDecID : video decoder hardware ID
// Return      : H265 decode error message
// **************************************************************************
BOOL fgVDEC_HAL_H265_ChkErrInfo(UINT32 ucBsId, UINT32 u4VDecID, UINT32 u4DecErrInfo, UINT32 u4ECLevel)
{
    UINT32 u4Data; 
    BOOL fgIsVDecErr;
    
    fgIsVDecErr = TRUE;

    switch(u4ECLevel)
    {
        case 2:
            // Ignore the real non-NextStartCode condition
            if( (u4DecErrInfo == (CABAC_ZERO_WORD_ERR|NO_NEXT_START_CODE))
            // Add For CQ: 31166, 31113 Customer_B_B_K: HEVCHD Disc            
            || (u4DecErrInfo == (CABAC_ZERO_WORD_ERR))
            )            	
            {
                fgIsVDecErr = FALSE;
            }
        case 0:
        case 1:
        default:
            if (u4DecErrInfo == CABAC_ZERO_WORD_ERR)
            {
                vVDEC_HAL_H265_TrailingBits(ucBsId, u4VDecID);
                u4Data = u4VDEC_HAL_H265_ShiftGetBitStream(ucBsId, u4VDecID, 0);
                if(((u4Data >> 8) == START_CODE) || (u4Data == 0x00000000) || (u4Data == START_CODE))
                {
                    fgIsVDecErr = FALSE;
                }
            }
            else if (u4DecErrInfo == NO_NEXT_START_CODE) // don't care "No next start code"
            {
                fgIsVDecErr = FALSE;
            }
            else if ((u4DecErrInfo == CABAC_ALIGN_BIT_ERR) && (!(u4VDecReadHEVCVLD(u4VDecID, RW_HEVLD_ERR_MASK) & CABAC_ALIGN_BIT_ERR))) // don't care "No next start code"
            {
                fgIsVDecErr = FALSE;
            } 
            break;
    }
    
    return fgIsVDecErr;
}



void vVDEC_HAL_H265_VDec_PowerDown(UCHAR u4VDecID)
{
#if (CONFIG_CHIP_VER_CURR >= CONFIG_CHIP_VER_MT8555)
     vVDecPowerDownHW(u4VDecID);
#endif
}

#if CONFIG_DRV_VERIFY_SUPPORT
UINT32 u4VDEC_HAL_H265_VDec_ReadFinishFlag(UINT32 u4VDecID)
{
  return ((u4VDecReadHEVCMISC(u4VDecID, RW_HEVC_DEC_COMPLETE)>>16) & 0x1);
}

UINT32 u4VDEC_HAL_H265_VDec_ClearInt(UINT32 u4VDecID){
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] u4VDEC_HAL_H265_VDec_ClearInt() start!!\n");
#endif
     UINT32 u4temp;
     vVDecWriteHEVCMISC (u4VDecID, RW_HEVC_DEC_COMPLETE,  u4VDecReadHEVCMISC(u4VDecID, RW_HEVC_DEC_COMPLETE)|0x1);
     u4temp = u4VDecReadHEVCMISC(u4VDecID, RW_HEVC_DEC_COMPLETE);
     vVDecWriteHEVCMISC (u4VDecID, RW_HEVC_DEC_COMPLETE,  u4temp |(0x1<< 4));
     vVDecWriteHEVCMISC (u4VDecID, RW_HEVC_DEC_COMPLETE,  u4temp & 0xffffffef);
     u4temp = u4VDecReadHEVCMISC(u4VDecID, RW_HEVC_DEC_COMPLETE);
     vVDecWriteHEVCMISC (u4VDecID, RW_HEVC_DEC_COMPLETE,  u4temp |(0x1<< 12));  // clear for VP mode

#ifdef VDEC_SIM_DUMP
        printk ("[INFO] u4VDEC_HAL_H265_VDec_ClearInt() done!!\n");
#endif

}


extern int  Wait_decode_finished( unsigned long  start_time );
UINT32 vVDEC_HAL_H265_VDec_VPmode(UINT32 u4VDecID)
{
    UINT32 risc_val1, pic_width, pic_height;
    BOOL bIsUFO;  
    H265_SPS_Data* prSPS;

    bIsUFO = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode;
    prSPS = _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.prSPS;

    //VP mode for end of bitstream error
    risc_val1= u4VDecReadHEVCVLD(u4VDecID, 57*4);
    //if (_u4PicCnt[u4VDecID] >= 1){  risc_val1 = 1; }      // force frame num turn on VP mode for debug
    if ( risc_val1 & 0x1 ){
    
        UINT32 SliceStartLCURow, u4LCUsize, u4RealWidth, u4W_Dram;
        UINT32 pic_real_wifth, pic_real_height,i, minDPOC;
        UINT32 MC_130, MC_131, MC_608, VLD_TOP_26, VLD_TOP_28;
        VDEC_INFO_H265_INIT_PRM_T rH265VDecInitPrm;
        UINT8 ucRefFBIndex;

        //search min delta POC pic in DPB
        minDPOC = 9999999;
        for (i=0; i<_tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++ ){
            if (i ==_tVerMpvDecPrm[u4VDecID].ucDecFBufIdx){
                continue;
            }
            if (abs(_ptH265CurrFBufInfo[u4VDecID]->i4POC -_ptH265FBufInfo[u4VDecID][i].i4POC) < minDPOC){
                minDPOC = abs(_ptH265CurrFBufInfo[u4VDecID]->i4POC -_ptH265FBufInfo[u4VDecID][i].i4POC);
                ucRefFBIndex = i;
            } 
        }
        if (minDPOC==9999999){
            ucRefFBIndex= _tVerMpvDecPrm[u4VDecID].SpecDecPrm.rVDecH265DecPrm.ucPreFBIndex;
        }
               
        rH265VDecInitPrm.u4FGDatabase = (UINT32)_pucFGDatabase[u4VDecID];
        rH265VDecInitPrm.u4FGSeedbase = (UINT32)_pucFGSeedbase[u4VDecID];


        risc_val1= u4VDecReadHEVCVLD(u4VDecID, RW_HEVLD_SPS_SIZE);
        u4LCUsize = 1<<((risc_val1>>4) & 0x7);
        risc_val1= u4VDecReadHEVCVLD(u4VDecID, RO_VLD_VWPTR);
        SliceStartLCURow = (risc_val1>>16)  & 0x3ff;

        risc_val1= u4VDecReadVLDTOP(u4VDecID, HEVC_VLD_TOP_PIC_PIX_SIZE);  
        u4RealWidth = risc_val1 & 0xFFFF; 
        u4W_Dram = ((u4RealWidth + 63)/64)*64;
    
        if ((SliceStartLCURow%2)==1 && u4LCUsize==16 ){
            SliceStartLCURow--;
        }

        if (bIsUFO){    //UFO HW constrain
            while(SliceStartLCURow*u4LCUsize*u4W_Dram % (8*4096) !=0 ||((SliceStartLCURow%2)==1 && u4LCUsize==16 )){
                SliceStartLCURow--;
            }
        }       

        //SliceStartLCURow=0; //full frame copy test
        
        printk("[INFO] VP mode!!  SliceStartLCURow %d; u4LCUsisze %d; refBufferIndex %d(pic #%d)\n", SliceStartLCURow, u4LCUsize, ucRefFBIndex,_ptH265FBufInfo[u4VDecID][ucRefFBIndex].u4PicCnt);

        pic_real_wifth = u4VDecReadMC(u4VDecID, HEVC_PIC_WIDTH);
        MC_130 =  ((pic_real_wifth+15)>>4)<<4;
        pic_real_height = u4VDecReadMC(u4VDecID, HEVC_PIC_HEIGHT);
        pic_real_height -= SliceStartLCURow*u4LCUsize;
        MC_131 = ((pic_real_height+15)>>4)<<4;
        MC_608 = u4VDecReadMC(u4VDecID, HEVC_DRAM_PITCH);
        VLD_TOP_26 = ((((pic_real_height+15)/16-1)& 0x7ff)<<16) |(((pic_real_wifth+15)/16-1)& 0x7ff);
        VLD_TOP_28 =  (((pic_real_height+15)>>4)<<20) | (((pic_real_wifth+15)>>4)<<4);
    
        i4VDEC_HAL_H265_InitVDecHW(u4VDecID);
    
#ifdef VDEC_SIM_DUMP
        printk ("[INFO] VP UFO settings\n");
#endif
        // UFO mode settings
        if (bIsUFO){
            pic_width = ((pic_real_wifth + 63)>>6)<<6;
            pic_height = ((pic_real_height + 31)>>5)<<5;   
            vVDecWriteMC(u4VDecID, 700*4, ((pic_width/16-1)<<16) |(pic_height/16-1));      
            vVDecWriteMC(u4VDecID, 664*4, 0x11);
            vVDecWriteMC(u4VDecID, 698*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YLenStartAddr+(SliceStartLCURow*u4LCUsize*u4W_Dram/256)));
            vVDecWriteMC(u4VDecID, 699*4, PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4CLenStartAddr+(SliceStartLCURow*u4LCUsize*u4W_Dram/512)));   
            vVDecWriteMC(u4VDecID, 663*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeBS+(SliceStartLCURow*u4LCUsize*u4W_Dram/256)-SliceStartLCURow*u4LCUsize*u4W_Dram);
            vVDecWriteMC(u4VDecID, 701*4, _ptH265CurrFBufInfo[u4VDecID]->u4UFOLenYsize-(SliceStartLCURow*u4LCUsize*u4W_Dram/256)+(SliceStartLCURow*u4LCUsize*u4W_Dram/512));
            vVDecWriteMC(u4VDecID, 343*4, _ptH265CurrFBufInfo[u4VDecID]->u4PicSizeYBS-(SliceStartLCURow*u4LCUsize*u4W_Dram)/2);
            vVDecWriteHEVCPP(u4VDecID, 706*4, 0x1 );   // UFO garbage remove

            // bypass PP_out setting
            vVDecWriteMC(u4VDecID,139*4, ((pic_real_wifth+15)>>4));
            vVDecWriteMC(u4VDecID,152*4, ((pic_real_wifth+15)>>4)-1);
            vVDecWriteMC(u4VDecID,153*4, ((pic_real_height+15)>>4)-1);
            
            vVDecWriteMC(u4VDecID, 136*4, 0x1);
            risc_val1 = u4VDecReadMC(u4VDecID, 142*4);
            vVDecWriteMC(u4VDecID, 142*4, risc_val1 & (~0x3));
            vVDecWriteMC(u4VDecID, 148*4, 0x1);
            risc_val1 = u4VDecReadMC(u4VDecID, 525*4);
            vVDecWriteMC(u4VDecID, 525*4, risc_val1 & (~0x1));
            vVDecWriteMC(u4VDecID, 137*4, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr)+ u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
            vVDecWriteMC(u4VDecID, 138*4, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr + _ptH265CurrFBufInfo[u4VDecID]->u4CAddrOffset)+ u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);

        }

#ifdef VDEC_SIM_DUMP
        printk ("[INFO] VP settings\n");
#endif
        risc_val1= u4VDecReadVLDTOP(u4VDecID, 36*4);  
        vVDecWriteVLDTOP(u4VDecID, 36*4, risc_val1 | (0x1<<1) );  //Turn on VP mode flag

        vVDecWriteMC(u4VDecID, HEVC_PIC_WIDTH, MC_130);
        vVDecWriteMC(u4VDecID, HEVC_PIC_HEIGHT, MC_131);
        vVDecWriteMC(u4VDecID, HEVC_DRAM_PITCH, MC_608 );
        
        vVDecWriteVLDTOP(u4VDecID, RW_VLD_PIC_MB_SIZE_M1, VLD_TOP_26);  
        vVDecWriteVLDTOP(u4VDecID, HEVC_VLD_TOP_PIC_PIX_SIZE, VLD_TOP_28);  

        vVDecWriteMC(u4VDecID, 9*4, 0x1);

        vVDecWriteMC(u4VDecID, 0*4, (PHYSICAL(_ptH265FBufInfo[u4VDecID][ucRefFBIndex].u4YStartAddr) + u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
        vVDecWriteMC(u4VDecID, 1*4, (PHYSICAL(_ptH265FBufInfo[u4VDecID][ucRefFBIndex].u4YStartAddr + _ptH265FBufInfo[u4VDecID][ucRefFBIndex].u4CAddrOffset)+ u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);
        vVDecWriteMC(u4VDecID, 2*4, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr)+ u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
        vVDecWriteMC(u4VDecID, 3*4, (PHYSICAL(_ptH265CurrFBufInfo[u4VDecID]->u4YStartAddr + _ptH265CurrFBufInfo[u4VDecID]->u4CAddrOffset)+ u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);

        risc_val1= u4VDecReadVLDTOP(u4VDecID, 36*4);  
        vVDecWriteVLDTOP(u4VDecID, 36*4,  risc_val1 |0x1 );  // Trigger VP mode 

  
        risc_val1 = u4VDecReadHEVCMISC(u4VDecID, RW_HEVC_DEC_COMPLETE);
        vVDecWriteHEVCMISC (u4VDecID, RW_HEVC_DEC_COMPLETE, risc_val1 & (~(0x1<<12)) ) ;
    
        return Wait_decode_finished( jiffies );
    }
    
    _u4CurrPicStartAddr[1]  = 0;
    return 0;
}

extern int Dump_reg( UINT32 base_r, UINT32 start_r, UINT32 end_r , char* pBitstream_name , UINT32 frame_number, BOOL bDecodeDone);

void vVDEC_HAL_H265_VDec_DumpReg(UINT32 u4VDecID, BOOL bDecodeDone)
{
    char pBitstream_name[200] = {0};

    memcpy (pBitstream_name , _bFileStr1[u4VDecID][1]+12 , (strlen(_bFileStr1[u4VDecID][1]) -38) );
    pBitstream_name[(strlen(_bFileStr1[u4VDecID][1]) -38)] = '\0';

    printk("[INFO] Dump register for %s #%d\n", pBitstream_name,_u4PicCnt[u4VDecID] );
    
    Dump_reg (0xF6028000, 0, 0, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);      //HEVC
    Dump_reg (0xF6028000, 33, 37, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);  //HEVC
    Dump_reg (0xF6028000, 40, 255, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);     //HEVC
    Dump_reg (0xF6021000, 33, 255, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);      //VLD
    Dump_reg (0xF6024000, 0, 255, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);        //MV
    Dump_reg (0xF6022000, 0, 1024, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);        //MC
    Dump_reg (0xF6025000, 0, 1024, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);       //PP
    Dump_reg (0xF6021800, 0, 255, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);       //VLD_TOP
    Dump_reg (0xF6020000, 0, 255, pBitstream_name, _u4PicCnt[u4VDecID], bDecodeDone);       //VLD_TOP
}

void vVDEC_HAL_H265_VDec_ReadCheckSum(UINT32 u4VDecID, UINT32 *pu4CheckSum)
{
  UINT32  u4Temp,u4Cnt;
  
  u4Temp = 0;
  *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f4);
  pu4CheckSum ++;
  u4Temp ++;
  *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f8);    
  pu4CheckSum ++;
  u4Temp ++;
  *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x608);    
  pu4CheckSum ++;
  u4Temp ++;
  *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x60c);        
  pu4CheckSum ++;
  u4Temp ++;

  //MC  378~397  
  for(u4Cnt=378; u4Cnt<=397; u4Cnt++)
  {
    *pu4CheckSum = u4VDecReadMC(u4VDecID, (u4Cnt<<2));
    pu4CheckSum ++;   
    u4Temp ++;
  }

  //HEVC VLD  165~179
  for(u4Cnt=165; u4Cnt<=179; u4Cnt++)
  {
    *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, (u4Cnt<<2));            
      pu4CheckSum ++;  
      u4Temp ++;
  }

  //MV  147~151
  for(u4Cnt=147; u4Cnt<=151; u4Cnt++)
  {
    *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
    pu4CheckSum ++;       
    u4Temp ++;
  }

  //IP  212    
  *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (212 << 2));
  pu4CheckSum ++;  
  u4Temp ++;
   
  //IQ  235~239
  for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
  {
    *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
    pu4CheckSum ++;     
    u4Temp ++;
  }    

  //IS  241~245
  for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
  {
    *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
    pu4CheckSum ++;     
    u4Temp ++;
  }

  while(u4Temp < MAX_CHKSUM_NUM)
  {
    *pu4CheckSum = 0;            
    pu4CheckSum ++;   
    u4Temp ++;
  }  
}

BOOL fgVDEC_HAL_H265_VDec_CompCheckSum(UINT32 *pu4DecCheckSum, UINT32 *pu4GoldenCheckSum)
{
  if((*pu4GoldenCheckSum) != (*pu4DecCheckSum))
  {
    return (FALSE);
  }
  pu4GoldenCheckSum ++;
  pu4DecCheckSum ++;
  if((*pu4GoldenCheckSum) != (*pu4DecCheckSum))
  {
    return (FALSE);
  }
  pu4GoldenCheckSum ++;
  pu4DecCheckSum ++;
  if((*pu4GoldenCheckSum) != (*pu4DecCheckSum))
  {
    return (FALSE);
  }
  pu4GoldenCheckSum ++;
  pu4DecCheckSum ++;
  if((*pu4GoldenCheckSum) != (*pu4DecCheckSum))
  {
    return (FALSE);
  }
  pu4GoldenCheckSum ++;
  pu4DecCheckSum ++;
  return (TRUE);
}

#endif

#ifdef MPV_DUMP_H265_CHKSUM
#define MAX_CHKSUM_NUM 80
UINT32 _u4DumpChksum[2][MAX_CHKSUM_NUM];
void vVDEC_HAL_H265_VDec_ReadCheckSum1(UINT32 u4VDecID)
{
    UINT32  u4Temp,u4Cnt;

    UINT32 *pu4CheckSum = _u4DumpChksum[0];
        
    u4Temp = 0;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f4);
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f8);    
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x608);    
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x60c);        
    pu4CheckSum ++;
    u4Temp ++;

    //MC  378~397  
    for(u4Cnt=378; u4Cnt<=397; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadMC(u4VDecID, (u4Cnt<<2));
        pu4CheckSum ++;   
        u4Temp ++;
    }

    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (44<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (45<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (46<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    //VLD  58~63
    for(u4Cnt=58; u4Cnt<=63; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }

    *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, 0x84);            
    pu4CheckSum ++;  
    u4Temp ++;
    
    //HEVC VLD  148~152
    for(u4Cnt=148; u4Cnt<=155; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }

    //HEVC VLD  165~179
    for(u4Cnt=165; u4Cnt<=179; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }

    //MV  147~151
    for(u4Cnt=147; u4Cnt<=151; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;       
        u4Temp ++;
    }

    //IP  212    
    *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (212 << 2));
    pu4CheckSum ++;  
    u4Temp ++;

    //IQ  235~239
    for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;     
        u4Temp ++;
    }    

    //IS  241~245
    for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;     
        u4Temp ++;
    }

    while(u4Temp < MAX_CHKSUM_NUM)
    {
        *pu4CheckSum = 0;            
        pu4CheckSum ++;   
        u4Temp ++;
    }  
}

void vVDEC_HAL_H265_VDec_ReadCheckSum2(UINT32 u4VDecID)
{
    UINT32  u4Temp,u4Cnt;

    UINT32 *pu4CheckSum = _u4DumpChksum[1];
        
    u4Temp = 0;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f4);
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x5f8);    
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x608);    
    pu4CheckSum ++;
    u4Temp ++;
    *pu4CheckSum = u4VDecReadMC(u4VDecID, 0x60c);        
    pu4CheckSum ++;
    u4Temp ++;

    //MC  378~397  
    for(u4Cnt=378; u4Cnt<=397; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadMC(u4VDecID, (u4Cnt<<2));
        pu4CheckSum ++;   
        u4Temp ++;
    }

    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (44<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (45<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    *pu4CheckSum = u4VDecReadVLD(u4VDecID, (46<<2));        
    pu4CheckSum ++;
    u4Temp ++;
    
    //VLD  58~63
    for(u4Cnt=58; u4Cnt<=63; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }

    *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, 0x84);            
    pu4CheckSum ++;  
    u4Temp ++;
    
    //HEVC VLD  148~152
    for(u4Cnt=148; u4Cnt<=155; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }

    //HEVC VLD  165~179
    for(u4Cnt=165; u4Cnt<=179; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCVLD(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;  
        u4Temp ++;
    }
    
    //MV  147~151
    for(u4Cnt=147; u4Cnt<=151; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;       
        u4Temp ++;
    }

    //IP  212    
    *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (212 << 2));
    pu4CheckSum ++;  
    u4Temp ++;

    //IQ  235~239
    for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;     
        u4Temp ++;
    }    

    //IS  241~245
    for(u4Cnt=241; u4Cnt<=245; u4Cnt++)
    {
        *pu4CheckSum = u4VDecReadHEVCMV(u4VDecID, (u4Cnt<<2));            
        pu4CheckSum ++;     
        u4Temp ++;
    }

    while(u4Temp < MAX_CHKSUM_NUM)
    {
        *pu4CheckSum = 0;            
        pu4CheckSum ++;   
        u4Temp ++;
    }  
}
#endif
