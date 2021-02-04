#ifndef _CAM_M4U_EXTERN_H
#define _CAM_M4U_EXTERN_H

#include <mach/m4u.h>

m4u_callback_ret_t ISP_M4U_TranslationFault_callback(int port, unsigned int mva, void *data);
void mt_irq_set_sens(unsigned int irq, unsigned int sens);
void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

#endif
