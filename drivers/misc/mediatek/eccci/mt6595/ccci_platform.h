#ifndef _CCCCI_PLATFORM_H_
#define _CCCCI_PLATFORM_H_
#ifdef MT6290
#define INFRACFG_AO_BASE 0
#else
#include <mach/mt_reg_base.h>
#endif

#define CCCI_PLATFORM "MT6595E1"
#define CCCI_DRIVER_VER 0x20110118
#define CURR_SEC_CCCI_SYNC_VER (1)	/* Note: must sync with sec lib, if ccci and sec has dependency change */
#define CCCI_MEM_ALIGN SZ_32M
#define CCCI_SMEM_ALIGN 0x100000	/* 1M */

#ifndef MT6290
#define FEATURE_GET_MD_GPIO_NUM
#define FEATURE_GET_MD_GPIO_VAL
#define FEATURE_GET_MD_ADC_NUM
#define FEATURE_GET_MD_ADC_VAL
#define FEATURE_GET_MD_EINT_ATTR
#define FEATURE_GET_MD_BAT_VOL
#define FEATURE_PM_IPO_H
#define FEATURE_DFO_EN
#if 0				/* DEPRECATED */
#define FEATURE_GET_TD_EINT_NUM
#define FEATURE_GET_DRAM_TYPE_CLK
#endif

#define ENABLE_EMI_PROTECTION
#define ENABLE_DRAM_API
#define ENABLE_MEM_REMAP_HW
/* #define ENABLE_CHIP_VER_CHECK */
/* #define ENABLE_2G_3G_CHECK */
#define ENABLE_MEM_SIZE_CHECK
/* #define ENABLE_MD_WDT_DBG */
#endif

#define INVALID_ADDR (0xF0000000)	/* the last EMI bank, properly not used */
#define KERN_EMI_BASE (0x40000000)	/* Bank4 */

/* - AP side, using mcu config base */
/* -- AP Bank4 */
#define AP_BANK4_MAP0 (0)	/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x200)) */
#define AP_BANK4_MAP1 (0)	/* ((volatile unsigned int*)(MCUSYS_CFGREG_BASE+0x204)) */
/* - MD side, using infra config base */
/* -- MD1 Bank 0 */
#define MD1_BANK0_MAP0 ((unsigned int *)(INFRACFG_AO_BASE+0x300))
#define MD1_BANK0_MAP1 ((unsigned int *)(INFRACFG_AO_BASE+0x304))
/* -- MD1 Bank 4 */
#define MD1_BANK4_MAP0 ((unsigned int *)(INFRACFG_AO_BASE+0x308))
#define MD1_BANK4_MAP1 ((unsigned int *)(INFRACFG_AO_BASE+0x30C))

#define DBG_FLAG_DEBUG		(1<<0)
#define DBG_FLAG_JTAG		(1<<1)
#ifndef MT6290
#define MD_DEBUG_MODE		(DBGAPB_BASE+0x1A010)
#endif
#define MD_DBG_JTAG_BIT		(1<<0)

void ccci_clear_md_region_protection(struct ccci_modem *md);
void ccci_set_mem_access_protection(struct ccci_modem *md);
void ccci_set_ap_region_protection(struct ccci_modem *md);
void ccci_set_mem_remap(struct ccci_modem *md, unsigned long smem_offset);
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);
void ccci_get_platform_version(char *ver);

#endif				/* _CCCCI_PLATFORM_H_ */
