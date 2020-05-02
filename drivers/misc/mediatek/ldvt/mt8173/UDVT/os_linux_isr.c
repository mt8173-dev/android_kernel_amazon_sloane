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


#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/string.h>
#include "x_os.h"
#include "x_pdwnc.h"

#define MAX_VECTOR_ID         160                  // Max vector ID
#define MAX_SHARED_VECTOR_ID  20
#define SHARED_IRQ_DEV        "SHARED"

// ISR control block of OS driver
typedef struct
{
    x_os_isr_fct        pf_isr;
    char                devname[8];
} OS_DRV_ISR_T;

static OS_DRV_ISR_T     s_isr_list[MAX_VECTOR_ID];
static OS_DRV_ISR_T     s_shared_isr_list[MAX_SHARED_VECTOR_ID];


static DEFINE_SPINLOCK(x_irq_lock);

irqreturn_t IsrProc(int irq, void *dev_id)
{
    if (s_isr_list[irq].pf_isr != NULL)
    {
        s_isr_list[irq].pf_isr((UINT16)irq);
        return IRQ_HANDLED;
    }
    return IRQ_NONE;
}

irqreturn_t IsrShareProc(int irq, void *dev_id)
{
    int i;
    if (irq == VECTOR_PWDNC) {
      for (i=0;i<MAX_SHARED_VECTOR_ID;i++) {
        if ((PDWNC_READ32(REG_RW_INTSTA) & (1 << i)) && s_shared_isr_list[i].pf_isr != NULL)
        {
          s_shared_isr_list[i].pf_isr((UINT16)irq);
          return IRQ_HANDLED;
        }
      }
    }
    return IRQ_NONE;
}

INT32
x_reg_isr_ex(UINT16         ui2_vec_id,
             x_os_isr_fct   pf_isr,
             x_os_isr_fct   *ppf_old_isr,
             ISR_FLAG_T     e_flags)
{
    x_os_isr_fct pf_old_isr;
    int ret;
    unsigned long flags;

    spin_lock_irqsave(&x_irq_lock, flags);
    pf_old_isr = s_isr_list[ui2_vec_id].pf_isr;
    if (pf_old_isr != NULL)
    {
        free_irq(ui2_vec_id, NULL);
    }
    s_isr_list[ui2_vec_id].pf_isr = pf_isr;
    if (pf_isr != NULL)
    {
        ret = request_irq(ui2_vec_id, IsrProc, e_flags, s_isr_list[ui2_vec_id].devname, NULL);
        if (ret != 0)
        {
            spin_unlock_irqrestore(&x_irq_lock, flags);
            return OSR_FAIL;
        }
    }
    *ppf_old_isr = pf_old_isr;
    spin_unlock_irqrestore(&x_irq_lock, flags);
    return OSR_OK;
}

EXPORT_SYMBOL(x_reg_isr_ex);


INT32
x_reg_isr(UINT16         ui2_vec_id,
          x_os_isr_fct   pf_isr,
          x_os_isr_fct   *ppf_old_isr)
{
    return x_reg_isr_ex(ui2_vec_id, pf_isr, ppf_old_isr, 0);
}

EXPORT_SYMBOL(x_reg_isr);

INT32
x_reg_isr_shared(UINT16         ui2_vec_id,
             x_os_isr_fct   pf_isr,
             x_os_isr_fct   *ppf_old_isr, 
             void *dev_id)
{
    x_os_isr_fct pf_old_isr;
    unsigned long flags;

    if (ui2_vec_id >= MAX_SHARED_VECTOR_ID)
      return OSR_FAIL;

    spin_lock_irqsave(&x_irq_lock, flags);
    pf_old_isr = s_shared_isr_list[ui2_vec_id].pf_isr;
    s_shared_isr_list[ui2_vec_id].pf_isr = pf_isr;
    if (pf_isr != NULL)
    {
      strncpy(s_isr_list[ui2_vec_id].devname, dev_id, 8);
    }
    else
    {
      snprintf(s_isr_list[ui2_vec_id].devname, 8, "SISR_%01d", ui2_vec_id);
    }
    *ppf_old_isr = pf_old_isr;
    spin_unlock_irqrestore(&x_irq_lock, flags);
    return OSR_OK;
}

EXPORT_SYMBOL(x_reg_isr_shared);

INT32
isr_init(VOID)
{
    int i;

    memset(s_isr_list, 0, sizeof(s_isr_list));
    for (i = 0; i < MAX_VECTOR_ID; i++)
    {
        snprintf(s_isr_list[i].devname, 8, "ISR_%02d", i);
    }
    for (i = 0; i < MAX_SHARED_VECTOR_ID; i++)
    {
        snprintf(s_shared_isr_list[i].devname, 8, "SISR_%01d", i);
}

    i = request_irq(VECTOR_PWDNC, IsrShareProc, IRQF_SHARED, SHARED_IRQ_DEV, SHARED_IRQ_DEV);

    if (i == 0)
    return OSR_OK;
    return OSR_FAIL;
}
