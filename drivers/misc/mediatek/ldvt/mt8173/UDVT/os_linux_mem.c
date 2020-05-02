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


#include <linux/module.h>
#include <linux/slab.h>
#include "x_os.h"
#include "x_assert.h"

#define CONFIG_SYS_MEM_WASTE_WARNING 0
#define GFP_MEM     (GFP_KERNEL | __GFP_REPEAT  | __GFP_NOFAIL)



VOID* x_mem_alloc (SIZE_T  z_size)
{

#if CONFIG_SYS_MEM_WASTE_WARNING
    if (z_size >= PAGE_SIZE)
    {
        size_t size = z_size;
        void *caller    = __builtin_return_address(0);
            
        size_t z_waste  = ((1 << get_order(size)) << PAGE_SHIFT) - size;
        if((size + (512*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 512K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else if((size + (256*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 256K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (128*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 128K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (64*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 64K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (32*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 32K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (16*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 16K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (8*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 8K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        else  if((size + (4*1024)) < ((1 << get_order(size)) << PAGE_SHIFT))
            printk("[%s] size = %d, waste(%d) > 4K, LR = %p\n", __FUNCTION__, size, z_waste, caller);
        
    }
#endif

    return kmalloc(z_size, GFP_MEM);
}

VOID* x_mem_calloc (UINT32  ui4_num_element,
                    SIZE_T  z_size_element)
{
    return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID* x_mem_realloc (VOID*  pv_mem_block,
                     SIZE_T z_new_size)
{
    return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}


VOID x_mem_free (VOID*  pv_mem_block)
{
    kfree(pv_mem_block);
}


VOID* x_mem_ch2_alloc (SIZE_T  z_size)
{
    return kmalloc(z_size, GFP_MEM);
}


VOID* x_mem_ch2_calloc (UINT32  ui4_num_element, SIZE_T  z_size_element)
{
    return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID* x_mem_ch2_realloc (VOID*  pv_mem_block, SIZE_T z_new_size)
{
    return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}


INT32 x_mem_part_create (HANDLE_T*    ph_part_hdl,
                         const CHAR*  ps_name,
                         VOID*        pv_addr,
                         SIZE_T       z_size,
                         SIZE_T       z_alloc_size)
{
    return OSR_OK;
}


INT32 x_mem_part_delete (HANDLE_T h_part_hdl)
{
    return OSR_OK;
}


INT32 x_mem_part_attach (HANDLE_T*    ph_part_hdl,
                         const CHAR*  ps_name)
{
    return OSR_OK;
}


VOID* x_mem_part_alloc (HANDLE_T  h_part_hdl,
                        SIZE_T    z_size)
{
    return kmalloc(z_size, GFP_MEM);
}


VOID* x_mem_part_calloc (HANDLE_T  h_part_hdl,
                         UINT32    ui4_num_element,
                         SIZE_T    z_size_element)
{
    return kcalloc(ui4_num_element, z_size_element, GFP_MEM);
}


VOID* x_mem_part_realloc (HANDLE_T     h_part_hdl,
                          VOID*        pv_mem_block,
                          SIZE_T       z_new_size)
{
    return krealloc(pv_mem_block, z_new_size, GFP_MEM);
}

EXPORT_SYMBOL(x_mem_alloc);
EXPORT_SYMBOL(x_mem_calloc);
EXPORT_SYMBOL(x_mem_realloc);
EXPORT_SYMBOL(x_mem_free);
EXPORT_SYMBOL(x_mem_ch2_alloc);
EXPORT_SYMBOL(x_mem_ch2_calloc);
EXPORT_SYMBOL(x_mem_ch2_realloc);
EXPORT_SYMBOL(x_mem_part_create);
EXPORT_SYMBOL(x_mem_part_delete);
EXPORT_SYMBOL(x_mem_part_attach);
EXPORT_SYMBOL(x_mem_part_alloc);
EXPORT_SYMBOL(x_mem_part_calloc);
EXPORT_SYMBOL(x_mem_part_realloc);

