#ifndef __DDP_GAMMA_H__
#define __DDP_GAMMA_H__

typedef enum {
	DISP_GAMMA0 = 0,
	DISP_GAMMA1,
	DISP_GAMMA_TOTAL
} disp_gamma_id_t;


typedef unsigned int gamma_entry;
#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

#define DISP_GAMMA_LUT_SIZE 512

typedef struct {
	disp_gamma_id_t hw_id;
	gamma_entry lut[DISP_GAMMA_LUT_SIZE];
} DISP_GAMMA_LUT_T;

typedef struct {
	disp_gamma_id_t hw_id;
	unsigned int coef[3][3];
} DISP_GAMMA_CCORR_T;


void disp_gamma_init(disp_gamma_id_t id, unsigned int width, unsigned int height, void *cmdq);

int disp_gamma_set_lut(const DISP_GAMMA_LUT_T __user *gamma_lut);

int disp_gamma_set_color_corr(const DISP_GAMMA_CCORR_T __user *color_corr);


#endif
