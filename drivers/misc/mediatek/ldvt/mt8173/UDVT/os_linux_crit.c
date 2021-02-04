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


#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include "x_assert.h"
#include "x_os.h"


#define INVALID_OWNER ((struct thread_info *)(NULL))

static struct thread_info *s_crit_owner = INVALID_OWNER;
static DEFINE_SPINLOCK(s_crit_lock);
static unsigned long s_crit_count;


CRIT_STATE_T
x_os_drv_crit_start(VOID)
{
    unsigned long flags;

    if (s_crit_owner != current_thread_info())
    {
        spin_lock_irqsave(&s_crit_lock, flags);
        s_crit_owner = current_thread_info();
        s_crit_count = 1;
        return (CRIT_STATE_T)(flags);
    }
    s_crit_count++;
    return (CRIT_STATE_T)(s_crit_count);
}


VOID
x_os_drv_crit_end(CRIT_STATE_T t_old_level)
{
    unsigned long flags = (unsigned long)(t_old_level);

    ASSERT(s_crit_owner == current_thread_info());
    s_crit_count--;
    if (s_crit_count != 0)
    {
        ASSERT(flags == s_crit_count + 1);
        return;
    }
    s_crit_owner = INVALID_OWNER;
    spin_unlock_irqrestore(&s_crit_lock, flags);
}


CRIT_STATE_T (* x_crit_start)(VOID) = x_os_drv_crit_start;
VOID (* x_crit_end)(CRIT_STATE_T  t_old_level) = x_os_drv_crit_end;


EXPORT_SYMBOL(x_crit_start);
EXPORT_SYMBOL(x_crit_end);


