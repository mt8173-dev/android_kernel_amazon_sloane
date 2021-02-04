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
/*----------------------------------------------------------------------------*
 * $RCSfile: drv_fs_linux.c,v $
 * $Revision: #1 $
 * $Date: 2012/09/29 $
 * $Author: dtvbm11 $
 * $CCRevision: /main/DTV_X_HQ_int/DTV_X_ATSC/1 $
 * $SWAuthor: Yan Wang $
 * $MD5HEX: 34076dbf72c1e756a741024f89fac47f $
 *
 * Description: 
 *         This file contains File Manager exported API's in Linux kernel mode.
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
                    include files
-----------------------------------------------------------------------------*/
#include "x_os.h"

#include "x_assert.h"

#if 1
#include "UDVT_IF.h"
#endif

#include <linux/fs.h>
#include <asm/uaccess.h>
#include "linux/syscalls.h"
#include "linux/module.h"
#include "x_drv_map.h"
#define UTIL_Printf printk

/*-----------------------------------------------------------------------------
                    macros, defines, typedefs, enums
 ----------------------------------------------------------------------------*/
#define PSEEK_SET   0   /* offset from begining of file*/
#define PSEEK_CUR   1   /* offset from current file pointer*/
#define PSEEK_END   2   /* offset from end of file*/

/*-----------------------------------------------------------------------------
                    External functions implementations
 ----------------------------------------------------------------------------*/
INT32 DrvFSMount(UINT32 dwDriveNo, UINT32 *pu4DrvFSTag)
{
    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


INT32 DrvFSUnMount()
{
    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


#if 1
INT32 DrvFSOpenFile(char* pcDirFileName, UINT32 dwFlags, INT32* piFd)
{
    struct file *filp;

    if ((NULL == pcDirFileName) || (NULL == piFd))
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }
    
    UTIL_Printf("[Drv_FS] %s: #%d: open file: %s\n", __FUNCTION__, __LINE__, pcDirFileName);

    if(dwFlags == DRV_FS_W_C)
    {
    //    filp = filp_open(pcDirFileName, O_CREAT|O_WRONLY, dwFlags);
    			filp = (struct file *)UDVT_IF_OpenFile(pcDirFileName,"ab");
    }
    else if(dwFlags == DRV_FS_RW_C)
    {
    //    filp = filp_open(pcDirFileName, O_CREAT|O_RDWR, dwFlags);
    			filp = (struct file *)UDVT_IF_OpenFile(pcDirFileName,"rwb");
    }
    else
    {
    //    filp = filp_open(pcDirFileName, O_RDONLY, dwFlags);
    			filp = (struct file *)UDVT_IF_OpenFile(pcDirFileName,"rb");
    }

    if(filp == 0)
    //if (IS_ERR(filp))
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_FAIL);
        return (DRV_FSR_FAIL);
    }

    *piFd = (INT32)filp;

    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


INT32 DrvFSGetFileSize(INT32 iFd, UINT32 *pu4FileSize)
{
    struct file *filp = (struct file *)iFd;
 //   INT32 iRet;
 //   INT32 iCur;
       
    if (NULL == pu4FileSize)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

		#if 0
    if (NULL == filp->f_op)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }
    
    /* get current offset */
    iCur = filp->f_op->llseek(filp, 0, SEEK_CUR);
    
    /* move to end to get end offset, that is the file size */
    *pu4FileSize = filp->f_op->llseek(filp, 0, SEEK_END);

    /* move back to current offset */
    iRet = filp->f_op->llseek(filp, iCur, SEEK_SET);
		#endif

		*pu4FileSize = UDVT_IF_GetFileLength((UINT32)iFd);
    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


INT32 DrvFSSeekFile(INT32 iFd, INT64 iOffset, INT32 iOrigin)
{
    struct file *filp = (struct file *)iFd;
    INT32 iPos;
    
    if ((iOrigin < 0) || (iOrigin > 2)) /*PSEEK_SET: 0, PSEEK_CUR: 1, PSEEK_END 2*/
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_PARAMETER_ERR);
        return (DRV_FSR_PARAMETER_ERR);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    //UTIL_Printf("[Drv_FS] %s: #%d: old Offset=%d\n", __FUNCTION__, __LINE__, (INT32)filp->f_pos);

    //UTIL_Printf("[Drv_FS] %s: #%d: old Offset=%d\n", __FUNCTION__, __LINE__, (INT32)filp->f_pos);

    //iPos = filp->f_pos;

    if (iOrigin == PSEEK_SET)
    {
    		iPos = (INT32)UDVT_IF_SeekFile((UINT32)filp,(INT32)iOffset,0);
        //iPos = iOffset;
        //filp->f_pos = iPos;
    }
    else if (iOrigin == PSEEK_CUR)
    {
    		iPos = (INT32)UDVT_IF_SeekFile((UINT32)filp,(INT32)iOffset,1);
        //iPos += iOffset;
        //filp->f_pos = iPos;
    }
    else if (iOrigin == PSEEK_END)
    {
        //if (NULL == filp->f_op)
        //{
        //    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        //    return DRV_FSR_NULL_POINT;
        //}
        //iPos = filp->f_op->llseek(filp, iOffset, SEEK_END);
        iPos = (INT32)UDVT_IF_SeekFile((UINT32)filp,(INT32)iOffset,2);
    }    

    //UTIL_Printf("[Drv_FS] %s: #%d: new Offset=%d\n", __FUNCTION__, __LINE__, (INT32)filp->f_pos);
   
    
    
    return (iPos);
}


INT32 DrvFSReadFile(INT32 iFd, void* pbBuf, UINT32 u4Count)
{
    struct file *filp = (struct file *)iFd;
  //  mm_segment_t oldfs;
    INT32 iRet;

    if (NULL == pbBuf)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    //if (NULL == filp->f_op)
    //{
    //    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
    //    return DRV_FSR_NULL_POINT;
    //}

    //oldfs = get_fs();
    //set_fs(KERNEL_DS);

    //iRet = filp->f_op->read(filp, pbBuf, u4Count, &filp->f_pos);
		iRet = (INT32)UDVT_IF_ReadFile(pbBuf,1,u4Count,(UINT32)filp);
    //set_fs(oldfs);

    //UTIL_Printf("[Drv_FS] %s: #%d: read %d bytes Offset=%d\n", __FUNCTION__, __LINE__, iRet, (unsigned)filp->f_pos);
    return (iRet);
}


INT32 DrvFSWriteFile(INT32 iFd, const void *pbBuf, DWRD dwSize)
{
    struct file *filp = (struct file *)iFd;
    //mm_segment_t oldfs;
    INT32 iRet;

    if (NULL == pbBuf)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (0 == dwSize)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_PARAMETER_ERR);
        return (DRV_FSR_PARAMETER_ERR);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

   /* if (NULL == filp->f_op)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }
*/
    //oldfs = get_fs();
    //set_fs(KERNEL_DS);
    iRet = (INT32)UDVT_IF_WriteFile((void *)pbBuf,1,dwSize,(UINT32)filp);
    //iRet = filp->f_op->write(filp, pbBuf, dwSize, &filp->f_pos);

    //set_fs(oldfs);

  //  UTIL_Printf("[Drv_FS] %s: #%d: write (%d, %d) bytes Offset=%d from 0x%8x\n", __FUNCTION__, __LINE__, iRet, dwSize, (unsigned)filp->f_pos, (unsigned)pbBuf);
    return (iRet);
}


INT32 DrvFSCloseFile(INT32 iFd)
{
    struct file *filp = (struct file *)iFd;

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    //filp_close(filp, NULL);
		UDVT_IF_CloseFile((UINT32)filp);
    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}

//#else

INT32 DrvFSUSBOpenFile(char* pcDirFileName, UINT32 dwFlags, INT32* piFd)
{
    struct file *filp;

    if ((NULL == pcDirFileName) || (NULL == piFd))
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }
    
//   UTIL_Printf("[Drv_FS] %s: #%d: open file: %s\n", __FUNCTION__, __LINE__, pcDirFileName);

    if(dwFlags == DRV_FS_W_C)
    {
        filp = filp_open(pcDirFileName, O_CREAT|O_WRONLY, dwFlags);
    }
    else if(dwFlags == DRV_FS_RW_C)
    {
        filp = filp_open(pcDirFileName, O_CREAT|O_RDWR, dwFlags);
    }
    else
    {
        filp = filp_open(pcDirFileName, O_RDONLY, dwFlags);
    }

    if (IS_ERR(filp))
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_FAIL);
        return (DRV_FSR_FAIL);
    }

    *piFd = (INT32)filp;

    //UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


INT32 DrvFSUSBGetFileSize(INT32 iFd, UINT32 *pu4FileSize)
{
    struct file *filp = (struct file *)iFd;
    INT32 iRet;
    INT32 iCur;
       
    if (NULL == pu4FileSize)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    if (NULL == filp->f_op)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }
    
    /* get current offset */
    iCur = filp->f_op->llseek(filp, 0, SEEK_CUR);
    
    /* move to end to get end offset, that is the file size */
    *pu4FileSize = filp->f_op->llseek(filp, 0, SEEK_END);

    /* move back to current offset */
    iRet = filp->f_op->llseek(filp, iCur, SEEK_SET);

    //UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}


INT32 DrvFSUSBSeekFile(INT32 iFd, INT64 iOffset, INT32 iOrigin)
{
    struct file *filp = (struct file *)iFd;
    INT32 iPos;
    
    if ((iOrigin < 0) || (iOrigin > 2)) /*PSEEK_SET: 0, PSEEK_CUR: 1, PSEEK_END 2*/
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_PARAMETER_ERR);
        return (DRV_FSR_PARAMETER_ERR);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

   // UTIL_Printf("[Drv_FS] %s: #%d: old Offset=%d\n", __FUNCTION__, __LINE__, (INT32)filp->f_pos);

    iPos = filp->f_pos;

    if (iOrigin == PSEEK_SET)
    {
        iPos = iOffset;
        filp->f_pos = iPos;
    }
    else if (iOrigin == PSEEK_CUR)
    {
        iPos += iOffset;
        filp->f_pos = iPos;
    }
    else if (iOrigin == PSEEK_END)
    {
        if (NULL == filp->f_op)
        {
            UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
            return DRV_FSR_NULL_POINT;
        }
        iPos = filp->f_op->llseek(filp, iOffset, SEEK_END);
    }    

    UTIL_Printf("[Drv_FS] %s: #%d: new Offset=%d\n", __FUNCTION__, __LINE__, (INT32)filp->f_pos);
    return (iPos);
}


INT32 DrvFSUSBReadFile(INT32 iFd, void* pbBuf, UINT32 u4Count)
{
    struct file *filp = (struct file *)iFd;
    mm_segment_t oldfs;
    INT32 iRet;

    if (NULL == pbBuf)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    if (NULL == filp->f_op)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    iRet = filp->f_op->read(filp, pbBuf, u4Count, &filp->f_pos);

    set_fs(oldfs);

    //UTIL_Printf("[Drv_FS] %s: #%d: read %d bytes Offset=%d\n", __FUNCTION__, __LINE__, iRet, (unsigned)filp->f_pos);
    return (iRet);
}


INT32 DrvFSUSBWriteFile(INT32 iFd, const void *pbBuf, DWRD dwSize)
{
    struct file *filp = (struct file *)iFd;
    mm_segment_t oldfs;
    INT32 iRet;

    if (NULL == pbBuf)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return (DRV_FSR_NULL_POINT);
    }

    if (0 == dwSize)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_PARAMETER_ERR);
        return (DRV_FSR_PARAMETER_ERR);
    }

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    if (NULL == filp->f_op)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    
    iRet = filp->f_op->write(filp, pbBuf, dwSize, &filp->f_pos);

    set_fs(oldfs);

    UTIL_Printf("[Drv_FS] %s: #%d: write (%d, %d) bytes Offset=%d from 0x%8x\n", __FUNCTION__, __LINE__, iRet, dwSize, (unsigned)filp->f_pos, (unsigned)pbBuf);
    return (iRet);
}


INT32 DrvFSUSBCloseFile(INT32 iFd)
{
    struct file *filp = (struct file *)iFd;

    if (NULL == filp)
    {
        UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_NULL_POINT);
        return DRV_FSR_NULL_POINT;
    }

    filp_close(filp, NULL);

    UTIL_Printf("[Drv_FS] %s: #%d: return %d\n", __FUNCTION__, __LINE__, DRV_FSR_SUCCESS);
    return (DRV_FSR_SUCCESS);
}
EXPORT_SYMBOL(DrvFSOpenFile);
EXPORT_SYMBOL(DrvFSGetFileSize);
EXPORT_SYMBOL(DrvFSSeekFile);
EXPORT_SYMBOL(DrvFSReadFile);
EXPORT_SYMBOL(DrvFSWriteFile);
EXPORT_SYMBOL(DrvFSCloseFile);
EXPORT_SYMBOL(DrvFSUSBOpenFile);
EXPORT_SYMBOL(DrvFSUSBGetFileSize);
EXPORT_SYMBOL(DrvFSUSBSeekFile);
EXPORT_SYMBOL(DrvFSUSBReadFile);
EXPORT_SYMBOL(DrvFSUSBWriteFile);
EXPORT_SYMBOL(DrvFSUSBCloseFile);

#endif





