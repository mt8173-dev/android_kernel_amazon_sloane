#ifndef __DISP_OVL_ENGINE_HW_H__
#define __DISP_OVL_ENGINE_HW_H__

#include "disp_ovl_engine_core.h"

extern bool is_early_suspended;
/* Ovl_Engine SW */
#define DISP_OVL_ENGINE_HW_SUPPORT

#ifdef DISP_OVL_ENGINE_HW_SUPPORT
void disp_ovl_engine_hw_init(void);
void disp_ovl_engine_hw_set_params(DISP_OVL_ENGINE_INSTANCE *params);
void disp_ovl_engine_trigger_hw_overlay(void);
void disp_ovl_engine_hw_register_irq(void (*irq_callback) (unsigned int param));
int disp_ovl_engine_hw_mva_map(struct disp_mva_map *mva_map_struct);
int disp_ovl_engine_hw_mva_unmap(struct disp_mva_map *mva_map_struct);
int disp_ovl_engine_hw_reset(void);
#endif

#endif
