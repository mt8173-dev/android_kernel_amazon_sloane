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


#include "x_os.h"


extern INT32 mem_init(VOID *pv_addr, SIZE_T z_size, VOID *pv_ch2_addr, SIZE_T z_ch2_size);
extern INT32 os_thread_init(VOID);
extern INT32 os_timer_init(VOID);
extern INT32 os_sema_init(VOID);
extern INT32 msg_q_init(VOID);
extern INT32 ev_grp_init(VOID);
extern INT32 isr_init(VOID);


INT32
os_init(VOID *pv_addr, SIZE_T z_size, VOID *pv_ch2_addr, SIZE_T z_ch2_size)
{
    INT32 i4_i;

    i4_i = mem_init(pv_addr, z_size, pv_ch2_addr, z_ch2_size);
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    i4_i = os_thread_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    i4_i = os_timer_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    i4_i = os_sema_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    i4_i = msg_q_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    i4_i = ev_grp_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    /*
    i4_i = isr_init();
    if (i4_i != OSR_OK)
    {
        return i4_i;
    }
    */
    return OSR_OK;
}

