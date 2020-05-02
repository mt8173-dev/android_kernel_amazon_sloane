#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/aee.h>
#include <linux/i2c.h>

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_cpuidle.h>
#include <mach/wd_api.h>
#include <mach/eint.h>
#include <mach/mtk_ccci_helper.h>
#include <mach/mt_cpufreq.h>
#include <mt_i2c.h>

#include "mt_spm_internal.h"

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_PCMWDT_EN           0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define CA7_BUS_CONFIG          (CA7MCUCFG_BASE + 0x1C)	/* 0x1020011c */

#define AP_PLL_CON7 (APMIXED_BASE+0x001C)

#define I2C_CHANNEL 1

int spm_dormant_sta = MT_CPU_DORMANT_RESET;
int spm_ap_mdsrc_req_cnt = 0;

/**********************************************************
 * PCM code for suspend
 **********************************************************/
#if SPM_CTRL_BIG_CPU
static const u32 suspend_binary[] = {
	0x81f58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81419801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006404, 0xc0c03340, 0x12807c1f,
	0x81409801, 0xd8000325, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c02c60,
	0x1200041f, 0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f,
	0x100062c8, 0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f,
	0x2000001a, 0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fff, 0x81419801,
	0xd8000705, 0x17c07c1f, 0xc0c034a0, 0x17c07c1f, 0x18c0001f, 0x65930001,
	0xc0c02a40, 0x17c07c1f, 0xc0c03740, 0x17c07c1f, 0x18c0001f, 0x10006404,
	0xc0c03340, 0x1280041f, 0xe8208000, 0x10006834, 0x00000010, 0x81f00407,
	0xa1dd0407, 0x81fd0407, 0xc2803980, 0x1290041f, 0x8880000c, 0x2f7be75f,
	0xd8200902, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff, 0xd0000940, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x10006834,
	0x00000000, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd8200b4c,
	0x17c07c1f, 0xe8208000, 0x10006834, 0x00000010, 0xd0001300, 0x17c07c1f,
	0x1880001f, 0x10006320, 0xc0c03120, 0xe080000f, 0xd8200c83, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0001300, 0x17c07c1f, 0xe080001f, 0x81419801,
	0xd8000e45, 0x17c07c1f, 0xc0c034a0, 0x17c07c1f, 0x18c0001f, 0x65930002,
	0xc0c02a40, 0x17c07c1f, 0x1b80001f, 0x2000049c, 0xc0c03740, 0x17c07c1f,
	0xe8208000, 0x10006354, 0xffffffff, 0x18c0001f, 0x100062c8, 0xe0e000f0,
	0xe0e00070, 0xe0e00030, 0xe0e00010, 0xe0e00000, 0x81409801, 0xd8001065,
	0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c02e40, 0x17c07c1f, 0xc2803980,
	0x1290841f, 0x81419801, 0xd8001185, 0x17c07c1f, 0x18c0001f, 0x10006404,
	0xc0c03240, 0x17c07c1f, 0xa1d20407, 0x81f28407, 0xa1d68407, 0xa0128400,
	0xa0118400, 0xa0100400, 0xa01a8400, 0xa01a0400, 0x19c0001f, 0x001c239f,
	0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f, 0x81411801, 0xd8001465,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e,
	0xe0e0000f, 0xc2803a80, 0x17c07c1f, 0x80368400, 0x1b80001f, 0x20000208,
	0x80370400, 0x18c0001f, 0x1000f688, 0x1910001f, 0x1000f688, 0xa1000404,
	0xe0c00004, 0x1b80001f, 0x20000208, 0x80360400, 0x803e0400, 0x1b80001f,
	0x20000208, 0x80380400, 0x1b80001f, 0x20000208, 0x803b0400, 0x1b80001f,
	0x20000208, 0x803d0400, 0x1b80001f, 0x20000208, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0xa1000404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0xa1000404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0x80340400, 0x17c07c1f, 0x17c07c1f, 0x80310400, 0xe8208000, 0x10000044,
	0x00000100, 0x1b80001f, 0x20000068, 0x1b80001f, 0x2000000a, 0x18c0001f,
	0x10006240, 0xe0e0000d, 0xd8001b85, 0x17c07c1f, 0x1b80001f, 0x20000100,
	0x81fa0407, 0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xa1d80407, 0xa1dc0407, 0xc2803980, 0x1291041f, 0x8880000c, 0x2f7be75f,
	0xd8201e02, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0xd0001e40, 0x17c07c1f,
	0x1b00001f, 0xbfffe7ff, 0xf0000000, 0x17c07c1f, 0x1b80001f, 0x20000fdf,
	0x1890001f, 0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002,
	0x8080080d, 0xd8202122, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f,
	0x20000004, 0xd8002a0c, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0002a00,
	0x17c07c1f, 0x81f80407, 0x81fc0407, 0x81429801, 0xd8002245, 0x17c07c1f,
	0x18c0001f, 0x65930007, 0xc0c02a40, 0x17c07c1f, 0x1880001f, 0x10006320,
	0xc0c02f00, 0xe080000f, 0xd80020a3, 0x17c07c1f, 0xe080001f, 0x81429801,
	0xd8002405, 0x17c07c1f, 0x18c0001f, 0x65930005, 0xc0c02a40, 0x17c07c1f,
	0xa1da0407, 0xe8208000, 0x10000048, 0x00000100, 0x1b80001f, 0x20000068,
	0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8, 0x1910001f, 0x1000f5c8,
	0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f, 0x100125c8,
	0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8, 0xa01d0400, 0xa01b0400,
	0xa0180400, 0xa01e0400, 0xa0160400, 0x18c0001f, 0x1000f688, 0x1910001f,
	0x1000f688, 0x81200404, 0xe0c00004, 0xa0170400, 0xa0168400, 0x1b80001f,
	0x20000104, 0x81411801, 0xd8002985, 0x17c07c1f, 0x18c0001f, 0x10006240,
	0xc0c02e40, 0x17c07c1f, 0xc2803980, 0x1291841f, 0x1b00001f, 0x7ffff7ff,
	0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003, 0xe8208000,
	0x10006834, 0x00000000, 0xe8208000, 0x10006834, 0x00000001, 0x18d0001f,
	0x10006830, 0x68e00003, 0x0000beef, 0xd8202b63, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xe0f0700d, 0xf0000000, 0x17c07c1f, 0xe0f07f0d, 0xe0f07f0f,
	0xe0f07f1e, 0xe0f07f12, 0xf0000000, 0x17c07c1f, 0xa1d08407, 0xa1d18407,
	0x1b80001f, 0x20000080, 0x812ab401, 0x80ebb401, 0xa0c00c04, 0x1a00001f,
	0x10006814, 0xe2000003, 0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f,
	0x20000020, 0xf0000000, 0x17c07c1f, 0xa1d00407, 0x1b80001f, 0x20000100,
	0x80ea3401, 0x1a00001f, 0x10006814, 0xe2000003, 0xf0000000, 0x17c07c1f,
	0xe0e03001, 0xe0e03101, 0xe0e03301, 0xe0e03703, 0xe0e03703, 0xe0e03707,
	0xf0000000, 0x17c07c1f, 0xd800342a, 0x17c07c1f, 0xe0e03703, 0xe0e03703,
	0xe0e03701, 0xd0003460, 0x17c07c1f, 0xe0e03301, 0xe0e03101, 0xf0000000,
	0x17c07c1f, 0x80390400, 0x1b80001f, 0x20000034, 0x803c0400, 0x1b80001f,
	0x20000300, 0x80350400, 0x18d0001f, 0x10000040, 0x1900001f, 0x10000040,
	0xb8c08003, 0xfffffff8, 0x00000001, 0xe1000003, 0x1b80001f, 0x20000104,
	0x81f90407, 0x81f40407, 0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f,
	0xa1d90407, 0x18d0001f, 0x10000040, 0x1900001f, 0x10000040, 0xb8c08003,
	0xfffffff8, 0x00000006, 0xe1000003, 0x1b80001f, 0x20000104, 0xa0150400,
	0xa01c0400, 0xa0190400, 0xf0000000, 0x17c07c1f, 0x18c0001f, 0x10006b6c,
	0x1910001f, 0x10006b6c, 0xa1002804, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x18c0001f, 0x100110e4, 0x1910001f, 0x100110e4, 0xa1158404, 0xe0c00004,
	0x81358404, 0xe0c00004, 0x18c0001f, 0x100040e4, 0x1910001f, 0x100040e4,
	0xa1158404, 0xe0c00004, 0x81358404, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0xd00f0000, 0x8880000c,
	0x2f7be75f, 0xd80064c2, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xc0c03080, 0x81401801, 0xd8004885, 0x17c07c1f, 0x81f60407, 0x18c0001f,
	0x100062a0, 0xc0c06580, 0x12807c1f, 0x18c0001f, 0x100062b4, 0x1910001f,
	0x100062b4, 0xa9000004, 0x00000001, 0xe0c00004, 0xa9000004, 0x00000011,
	0xe0c00004, 0x18c0001f, 0x100062a0, 0xc0c06580, 0x1280041f, 0x18c0001f,
	0x100062b0, 0xc0c06580, 0x12807c1f, 0xe8208000, 0x100062b8, 0x00000011,
	0x1b80001f, 0x20000080, 0xe8208000, 0x100062b8, 0x00000015, 0xc0c06580,
	0x1280041f, 0x18c0001f, 0x10006290, 0xc0c06580, 0x1280041f, 0xe8208000,
	0x10006404, 0x00003101, 0xc2803980, 0x1292041f, 0x81421801, 0xd8004a05,
	0x17c07c1f, 0x18c0001f, 0x65930006, 0xc0c02a40, 0x17c07c1f, 0xe8208000,
	0x10006834, 0x00000010, 0xc2803980, 0x1292841f, 0xc0c06be0, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0x81449801,
	0xd8004c25, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200001, 0xc0c06d60,
	0x12807c1f, 0xc0c06c80, 0x17c07c1f, 0xa1d38407, 0xa1d98407, 0x1800001f,
	0x00000012, 0x1800001f, 0x00000e12, 0x1800001f, 0x03800e12, 0x1800001f,
	0x038e0e12, 0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0x81200404,
	0xe0c00004, 0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c, 0xa1108404,
	0xe0c00004, 0x81200404, 0xe0c00004, 0xe8208000, 0x10006310, 0x0b1600f8,
	0x1b00001f, 0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400, 0xd8205103,
	0xa1d58407, 0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004fc0, 0x17c07c1f,
	0x1890001f, 0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd8005323, 0x17c07c1f,
	0x80c10001, 0xd80052a3, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd00052e0,
	0x17c07c1f, 0x1b00001f, 0x7ffff7ff, 0xd0004fc0, 0x17c07c1f, 0x80c10001,
	0xd8205423, 0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004fc0,
	0x17c07c1f, 0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8,
	0xe0e00ff0, 0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801,
	0xd80056e5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200000, 0xc0c06d60,
	0x1280041f, 0xc0c06c80, 0x17c07c1f, 0x1b80001f, 0x200016a8, 0x1800001f,
	0x03800e12, 0x18c0001f, 0x1020920c, 0x1910001f, 0x1020920c, 0xa1000404,
	0xe0c00004, 0x1b80001f, 0x20000300, 0x1800001f, 0x00000e12, 0x81308404,
	0xe0c00004, 0x18c0001f, 0x10209200, 0x1910001f, 0x10209200, 0xa1000404,
	0xe0c00004, 0x1b80001f, 0x20000300, 0x1800001f, 0x00000012, 0x1b80001f,
	0x20000104, 0x10007c1f, 0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407,
	0x81429801, 0xd8005e45, 0x17c07c1f, 0x18c0001f, 0x65930003, 0xc0c02a40,
	0x17c07c1f, 0x1b80001f, 0x200016a8, 0x81f80407, 0xe8208000, 0x10006354,
	0xffffffff, 0x1b80001f, 0x20000104, 0x18c0001f, 0x65930004, 0xc0c02a40,
	0x17c07c1f, 0xa1d80407, 0xe8208000, 0x10006354, 0xfffe7b47, 0xe8208000,
	0x10006834, 0x00000010, 0xc2803980, 0x1293041f, 0x81401801, 0xd80064c5,
	0x17c07c1f, 0xe8208000, 0x10006404, 0x00001101, 0x18c0001f, 0x10006290,
	0x1212841f, 0xc0c066e0, 0x12807c1f, 0xc0c066e0, 0x1280041f, 0x18c0001f,
	0x100062b0, 0x1212841f, 0xc0c066e0, 0x12807c1f, 0xe8208000, 0x100062b8,
	0x00000011, 0xe8208000, 0x100062b8, 0x00000010, 0x1b80001f, 0x20000080,
	0xc0c066e0, 0x1280041f, 0xe8208000, 0x10200268, 0x000ffffe, 0x18c0001f,
	0x100062a0, 0x1212841f, 0xc0c066e0, 0x12807c1f, 0x18c0001f, 0x100062b4,
	0x1910001f, 0x100062b4, 0x89000004, 0xffffffef, 0xe0c00004, 0x89000004,
	0xffffffee, 0xe0c00004, 0x1b80001f, 0x20000a50, 0x18c0001f, 0x100062a0,
	0xc0c066e0, 0x1280041f, 0x19c0001f, 0x01411820, 0x1ac0001f, 0x55aa55aa,
	0x10007c1f, 0xf0000000, 0xd800660a, 0x17c07c1f, 0xe2e0006d, 0xe2e0002d,
	0xd82066aa, 0x17c07c1f, 0xe2e0002f, 0xe2e0003e, 0xe2e00032, 0xf0000000,
	0x17c07c1f, 0xd80067ea, 0x17c07c1f, 0xe2e00036, 0x1380201f, 0xe2e0003e,
	0x1380201f, 0xe2e0002e, 0x1380201f, 0xd82068ea, 0x17c07c1f, 0xe2e0006e,
	0xe2e0004e, 0xe2e0004c, 0x1b80001f, 0x20000020, 0xe2e0004d, 0xf0000000,
	0x17c07c1f, 0xd8206a29, 0x17c07c1f, 0xe2e0000d, 0xe2e0000c, 0xe2e0001c,
	0xe2e0001e, 0xe2e00016, 0xe2e00012, 0xf0000000, 0x17c07c1f, 0xd8206ba9,
	0x17c07c1f, 0xe2e00016, 0x1380201f, 0xe2e0001e, 0x1380201f, 0xe2e0001c,
	0x1380201f, 0xe2e0000c, 0xe2e0000d, 0xf0000000, 0x17c07c1f, 0xa1d40407,
	0x1391841f, 0xa1d90407, 0xf0000000, 0x17c07c1f, 0x18d0001f, 0x10006604,
	0x10cf8c1f, 0xd8206c83, 0x17c07c1f, 0xf0000000, 0x17c07c1f, 0xe8208000,
	0x11008014, 0x00000002, 0xe8208000, 0x11008020, 0x00000101, 0xe8208000,
	0x11008004, 0x000000d0, 0x1a00001f, 0x11008000, 0xd8006f4a, 0xe220005d,
	0xd8206f6a, 0xe2200000, 0xe2200001, 0xe8208000, 0x11008024, 0x00000001,
	0x1b80001f, 0x20000424, 0xf0000000, 0x17c07c1f
};

static struct pcm_desc suspend_pcm = {
	.version = "pcm_suspend_v32.2_20140205_CA15_DPD",
	.base = suspend_binary,
	.size = 898,
	.sess = 2,
	.replace = 0,
	.vec0 = EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1 = EVENT_VEC(12, 1, 0, 76),	/* FUNC_26M_SLEEP */
	.vec2 = EVENT_VEC(30, 1, 0, 154),	/* FUNC_APSRC_WAKEUP */
	.vec3 = EVENT_VEC(31, 1, 0, 244),	/* FUNC_APSRC_SLEEP */
};
#else
static const u32 suspend_binary[] = {
	0x81f58407, 0x81f68407, 0x803a0400, 0x803a8400, 0x1b80001f, 0x20000000,
	0x80300400, 0x80318400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81419801,
	0xd8000245, 0x17c07c1f, 0x18c0001f, 0x10006404, 0xc0c03340, 0x12807c1f,
	0x81409801, 0xd8000325, 0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c02c60,
	0x1200041f, 0x80310400, 0x1b80001f, 0x2000000a, 0xa0110400, 0x18c0001f,
	0x100062c8, 0xe0e00010, 0xe0e00030, 0xe0e00070, 0xe0e000f0, 0x1b80001f,
	0x2000001a, 0xe0e00ff0, 0xe8208000, 0x10006354, 0xfffe7fff, 0x81419801,
	0xd8000705, 0x17c07c1f, 0xc0c034a0, 0x17c07c1f, 0x18c0001f, 0x65930001,
	0xc0c02a40, 0x17c07c1f, 0xc0c03740, 0x17c07c1f, 0x18c0001f, 0x10006404,
	0xc0c03340, 0x1280041f, 0xe8208000, 0x10006834, 0x00000010, 0x81f00407,
	0xa1dd0407, 0x81fd0407, 0xc2803980, 0x1290041f, 0x8880000c, 0x2f7be75f,
	0xd8200902, 0x17c07c1f, 0x1b00001f, 0x7fffe7ff, 0xd0000940, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x10006834,
	0x00000000, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004, 0xd8200b4c,
	0x17c07c1f, 0xe8208000, 0x10006834, 0x00000010, 0xd0001300, 0x17c07c1f,
	0x1880001f, 0x10006320, 0xc0c03120, 0xe080000f, 0xd8200c83, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0001300, 0x17c07c1f, 0xe080001f, 0x81419801,
	0xd8000e45, 0x17c07c1f, 0xc0c034a0, 0x17c07c1f, 0x18c0001f, 0x65930002,
	0xc0c02a40, 0x17c07c1f, 0x1b80001f, 0x2000049c, 0xc0c03740, 0x17c07c1f,
	0xe8208000, 0x10006354, 0xffffffff, 0x18c0001f, 0x100062c8, 0xe0e000f0,
	0xe0e00070, 0xe0e00030, 0xe0e00010, 0xe0e00000, 0x81409801, 0xd8001065,
	0x17c07c1f, 0x18c0001f, 0x10006234, 0xc0c02e40, 0x17c07c1f, 0xc2803980,
	0x1290841f, 0x81419801, 0xd8001185, 0x17c07c1f, 0x18c0001f, 0x10006404,
	0xc0c03240, 0x17c07c1f, 0xa1d20407, 0x81f28407, 0xa1d68407, 0xa0128400,
	0xa0118400, 0xa0100400, 0xa01a8400, 0xa01a0400, 0x19c0001f, 0x001c239f,
	0x1b00001f, 0x3fffefff, 0xf0000000, 0x17c07c1f, 0x81411801, 0xd8001465,
	0x17c07c1f, 0x18c0001f, 0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e,
	0xe0e0000f, 0xc2803a80, 0x17c07c1f, 0x80368400, 0x1b80001f, 0x20000208,
	0x80370400, 0x18c0001f, 0x1000f688, 0x1910001f, 0x1000f688, 0xa1000404,
	0xe0c00004, 0x1b80001f, 0x20000208, 0x80360400, 0x803e0400, 0x1b80001f,
	0x20000208, 0x80380400, 0x1b80001f, 0x20000208, 0x803b0400, 0x1b80001f,
	0x20000208, 0x803d0400, 0x1b80001f, 0x20000208, 0x18c0001f, 0x1000f5c8,
	0x1910001f, 0x1000f5c8, 0xa1000404, 0xe0c00004, 0x18c0001f, 0x100125c8,
	0x1910001f, 0x100125c8, 0xa1000404, 0xe0c00004, 0x1910001f, 0x100125c8,
	0x80340400, 0x17c07c1f, 0x17c07c1f, 0x80310400, 0xe8208000, 0x10000044,
	0x00000100, 0x1b80001f, 0x20000068, 0x1b80001f, 0x2000000a, 0x18c0001f,
	0x10006240, 0xe0e0000d, 0xd8001b85, 0x17c07c1f, 0x1b80001f, 0x20000100,
	0x81fa0407, 0x81f18407, 0x81f08407, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xa1d80407, 0xa1dc0407, 0xc2803980, 0x1291041f, 0x8880000c, 0x2f7be75f,
	0xd8201e02, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0xd0001e40, 0x17c07c1f,
	0x1b00001f, 0xbfffe7ff, 0xf0000000, 0x17c07c1f, 0x1b80001f, 0x20000fdf,
	0x1890001f, 0x10006608, 0x80c98801, 0x810a8801, 0x10918c1f, 0xa0939002,
	0x8080080d, 0xd8202122, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f,
	0x20000004, 0xd8002a0c, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0002a00,
	0x17c07c1f, 0x81f80407, 0x81fc0407, 0x81429801, 0xd8002245, 0x17c07c1f,
	0x18c0001f, 0x65930007, 0xc0c02a40, 0x17c07c1f, 0x1880001f, 0x10006320,
	0xc0c02f00, 0xe080000f, 0xd80020a3, 0x17c07c1f, 0xe080001f, 0x81429801,
	0xd8002405, 0x17c07c1f, 0x18c0001f, 0x65930005, 0xc0c02a40, 0x17c07c1f,
	0xa1da0407, 0xe8208000, 0x10000048, 0x00000100, 0x1b80001f, 0x20000068,
	0xa0110400, 0xa0140400, 0x18c0001f, 0x1000f5c8, 0x1910001f, 0x1000f5c8,
	0x81200404, 0xe0c00004, 0x18c0001f, 0x100125c8, 0x1910001f, 0x100125c8,
	0x81200404, 0xe0c00004, 0x1910001f, 0x100125c8, 0xa01d0400, 0xa01b0400,
	0xa0180400, 0xa01e0400, 0xa0160400, 0x18c0001f, 0x1000f688, 0x1910001f,
	0x1000f688, 0x81200404, 0xe0c00004, 0xa0170400, 0xa0168400, 0x1b80001f,
	0x20000104, 0x81411801, 0xd8002985, 0x17c07c1f, 0x18c0001f, 0x10006240,
	0xc0c02e40, 0x17c07c1f, 0xc2803980, 0x1291841f, 0x1b00001f, 0x7ffff7ff,
	0xf0000000, 0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003, 0xe8208000,
	0x10006834, 0x00000000, 0xe8208000, 0x10006834, 0x00000001, 0x18d0001f,
	0x10006830, 0x68e00003, 0x0000beef, 0xd8202b63, 0x17c07c1f, 0xf0000000,
	0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e, 0x1380201f, 0xe0f07f0e,
	0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d, 0xe0f07e0d, 0xe0f07c0d,
	0xe0f0780d, 0xe0f0700d, 0xf0000000, 0x17c07c1f, 0xe0f07f0d, 0xe0f07f0f,
	0xe0f07f1e, 0xe0f07f12, 0xf0000000, 0x17c07c1f, 0xa1d08407, 0xa1d18407,
	0x1b80001f, 0x20000080, 0x812ab401, 0x80ebb401, 0xa0c00c04, 0x1a00001f,
	0x10006814, 0xe2000003, 0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f,
	0x20000020, 0xf0000000, 0x17c07c1f, 0xa1d00407, 0x1b80001f, 0x20000100,
	0x80ea3401, 0x1a00001f, 0x10006814, 0xe2000003, 0xf0000000, 0x17c07c1f,
	0xe0e03001, 0xe0e03101, 0xe0e03301, 0xe0e03703, 0xe0e03703, 0xe0e03707,
	0xf0000000, 0x17c07c1f, 0xd800342a, 0x17c07c1f, 0xe0e03703, 0xe0e03703,
	0xe0e03701, 0xd0003460, 0x17c07c1f, 0xe0e03301, 0xe0e03101, 0xf0000000,
	0x17c07c1f, 0x80390400, 0x1b80001f, 0x20000034, 0x803c0400, 0x1b80001f,
	0x20000300, 0x80350400, 0x18d0001f, 0x10000040, 0x1900001f, 0x10000040,
	0xb8c08003, 0xfffffff8, 0x00000001, 0xe1000003, 0x1b80001f, 0x20000104,
	0x81f90407, 0x81f40407, 0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f,
	0xa1d90407, 0x18d0001f, 0x10000040, 0x1900001f, 0x10000040, 0xb8c08003,
	0xfffffff8, 0x00000006, 0xe1000003, 0x1b80001f, 0x20000104, 0xa0150400,
	0xa01c0400, 0xa0190400, 0xf0000000, 0x17c07c1f, 0x18c0001f, 0x10006b6c,
	0x1910001f, 0x10006b6c, 0xa1002804, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x18c0001f, 0x100110e4, 0x1910001f, 0x100110e4, 0xa1158404, 0xe0c00004,
	0x81358404, 0xe0c00004, 0x18c0001f, 0x100040e4, 0x1910001f, 0x100040e4,
	0xa1158404, 0xe0c00004, 0x81358404, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0x1a50001f, 0x10006610, 0x8246a401, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2f7be75f, 0x1b80001f, 0xd00f0000, 0x8880000c,
	0x2f7be75f, 0xd8005fe2, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffe7b47,
	0xc0c03080, 0x81401801, 0xd80047c5, 0x17c07c1f, 0x81f60407, 0x18c0001f,
	0x10006200, 0xc0c060a0, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c060a0, 0x1280041f, 0x18c0001f, 0x10006204,
	0xc0c06440, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c060a0, 0x12807c1f,
	0xe8208000, 0x10006244, 0x00000001, 0x1b80001f, 0x20000080, 0xc0c060a0,
	0x1280041f, 0x18c0001f, 0x10006290, 0xc0c060a0, 0x1280041f, 0xe8208000,
	0x10006404, 0x00003101, 0xc2803980, 0x1292041f, 0x81421801, 0xd8004945,
	0x17c07c1f, 0x18c0001f, 0x65930006, 0xc0c02a40, 0x17c07c1f, 0xe8208000,
	0x10006834, 0x00000010, 0xc2803980, 0x1292841f, 0xc0c06700, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0x81449801,
	0xd8004b85, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200003, 0xc0c067a0,
	0x17c07c1f, 0xe2200005, 0xc0c067a0, 0x17c07c1f, 0xa1d38407, 0xa1d98407,
	0x1800001f, 0x00000012, 0x1800001f, 0x00000e12, 0x1800001f, 0x03800e12,
	0x1800001f, 0x038e0e12, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f,
	0xbfffe7ff, 0x1b80001f, 0x90100000, 0x80c00400, 0xd8204ea3, 0xa1d58407,
	0xa1dd8407, 0x1b00001f, 0x3fffefff, 0xd0004d60, 0x17c07c1f, 0x1890001f,
	0x100063e8, 0x88c0000c, 0x2f7be75f, 0xd80050c3, 0x17c07c1f, 0x80c10001,
	0xd8005043, 0x17c07c1f, 0x1b00001f, 0xbfffe7ff, 0xd0005080, 0x17c07c1f,
	0x1b00001f, 0x7ffff7ff, 0xd0004d60, 0x17c07c1f, 0x80c10001, 0xd82051c3,
	0x17c07c1f, 0xa1de0407, 0x1b00001f, 0x7fffe7ff, 0xd0004d60, 0x17c07c1f,
	0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc, 0xe0e007f8, 0xe0e00ff0,
	0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00, 0x81449801, 0xd80054a5,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200002, 0xc0c067a0, 0x17c07c1f,
	0xe2200004, 0xc0c067a0, 0x17c07c1f, 0x1b80001f, 0x200016a8, 0x1800001f,
	0x03800e12, 0x1b80001f, 0x20000300, 0x1800001f, 0x00000e12, 0x1b80001f,
	0x20000300, 0x1800001f, 0x00000012, 0x1b80001f, 0x20000104, 0x10007c1f,
	0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407, 0x81429801, 0xd8005a45,
	0x17c07c1f, 0x18c0001f, 0x65930003, 0xc0c02a40, 0x17c07c1f, 0x1b80001f,
	0x200016a8, 0x81f80407, 0xe8208000, 0x10006354, 0xffffffff, 0x1b80001f,
	0x20000104, 0x18c0001f, 0x65930004, 0xc0c02a40, 0x17c07c1f, 0xa1d80407,
	0xe8208000, 0x10006354, 0xfffe7b47, 0xe8208000, 0x10006834, 0x00000010,
	0xc2803980, 0x1293041f, 0x81401801, 0xd8005fe5, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00002101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c06200,
	0x12807c1f, 0xc0c06200, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c06200, 0x12807c1f, 0xe8208000, 0x10006244, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c06200, 0x1280041f, 0xe8208000, 0x10200268, 0x000ffffe,
	0x18c0001f, 0x10006204, 0x1212841f, 0xc0c06580, 0x1280041f, 0x18c0001f,
	0x10006200, 0x1212841f, 0xc0c06200, 0x12807c1f, 0xe8208000, 0x1000625c,
	0x00000000, 0x1b80001f, 0x20000080, 0xc0c06200, 0x1280041f, 0x19c0001f,
	0x01411820, 0x1ac0001f, 0x55aa55aa, 0x10007c1f, 0xf0000000, 0xd800612a,
	0x17c07c1f, 0xe2e0006d, 0xe2e0002d, 0xd82061ca, 0x17c07c1f, 0xe2e0002f,
	0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0xd800630a, 0x17c07c1f,
	0xe2e00036, 0x1380201f, 0xe2e0003e, 0x1380201f, 0xe2e0002e, 0x1380201f,
	0xd820640a, 0x17c07c1f, 0xe2e0006e, 0xe2e0004e, 0xe2e0004c, 0x1b80001f,
	0x20000020, 0xe2e0004d, 0xf0000000, 0x17c07c1f, 0xd8206549, 0x17c07c1f,
	0xe2e0000d, 0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016, 0xe2e00012,
	0xf0000000, 0x17c07c1f, 0xd82066c9, 0x17c07c1f, 0xe2e00016, 0x1380201f,
	0xe2e0001e, 0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c, 0xe2e0000d,
	0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0xf0000000,
	0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd82067a3, 0x17c07c1f,
	0xf0000000, 0x17c07c1f, 0xe8208000, 0x11008014, 0x00000002, 0xe8208000,
	0x11008020, 0x00000101, 0xe8208000, 0x11008004, 0x000000d0, 0x1a00001f,
	0x11008000, 0xd8006a6a, 0xe220005d, 0xd8206a8a, 0xe2200000, 0xe2200001,
	0xe8208000, 0x11008024, 0x00000001, 0x1b80001f, 0x20000424, 0xf0000000,
	0x17c07c1f
};

static struct pcm_desc suspend_pcm = {
	.version = "pcm_suspend_v32.2_20140205_CA7_DPD",
	.base = suspend_binary,
	.size = 859,
	.sess = 2,
	.replace = 0,
	.vec0 = EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1 = EVENT_VEC(12, 1, 0, 76),	/* FUNC_26M_SLEEP */
	.vec2 = EVENT_VEC(30, 1, 0, 154),	/* FUNC_APSRC_WAKEUP */
	.vec3 = EVENT_VEC(31, 1, 0, 244),	/* FUNC_APSRC_SLEEP */
};
#endif

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       99	/* 3ms */

#define WAIT_UART_ACK_TIMES     10	/* 10 * 10us */

#define SPM_WAKE_PERIOD         600	/* sec */

#define WAKE_SRC_FOR_SUSPEND                                          \
    (WAKE_SRC_KP | WAKE_SRC_EINT | WAKE_SRC_CCIF_MD | WAKE_SRC_MD32 | \
     WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_THERM |            \
     WAKE_SRC_SYSPWREQ | WAKE_SRC_MD_WDT | WAKE_SRC_CLDMA_MD |        \
     WAKE_SRC_SEJ | WAKE_SRC_ALL_MD32)

#define WAKE_SRC_FOR_MD32  0                                          \
				/* (WAKE_SRC_AUD_MD32) */

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

extern int get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level);

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

extern int request_uart_to_sleep(void);
extern void mtk_uart_restore(void);
extern void dump_uart_reg(void);

static struct pwr_ctrl suspend_ctrl = {
	.wake_src = WAKE_SRC_FOR_SUSPEND,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.infra_dcm_lock = 1,
	.wfi_op = WFI_OP_AND,
#if 0
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,
	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.md2_req_mask = 1,
	.disp_req_mask = 1,
	.mfg_req_mask = 1,
#else
	.ca7top_idle_mask = 0,
	.ca15top_idle_mask = 0,
	.mcusys_idle_mask = 0,
	.disp_req_mask = 0,
	.mfg_req_mask = 0,
	.md1_req_mask = 0,
	.md2_req_mask = 0,
	.md32_req_mask = 1,
	.md_apsrc_sel = 0,

	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,
#endif


#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
};

struct spm_lp_scen __spm_suspend = {
	.pcmdesc = &suspend_pcm,
	.pwrctrl = &suspend_ctrl,
};

static void spm_i2c_control(u32 channel, bool onoff)
{
	static int pdn;
	static bool i2c_onoff;
	u32 base, i2c_clk;

	switch (channel) {
	case 0:
		base = I2C0_BASE;
		i2c_clk = MT_CG_PERI_I2C0;
		break;
	case 1:
		base = I2C1_BASE;
		i2c_clk = MT_CG_PERI_I2C1;
		break;
	case 2:
		base = I2C2_BASE;
		i2c_clk = MT_CG_PERI_I2C2;
		break;
	case 3:
		base = I2C3_BASE;
		i2c_clk = MT_CG_PERI_I2C3;
		break;
	case 4:
		base = I2C4_BASE;
		i2c_clk = MT_CG_PERI_I2C4;
		break;
	default:
		break;
	}

	if ((1 == onoff) && (0 == i2c_onoff)) {
		i2c_onoff = 1;
#if 1
		pdn = spm_read(PERI_PDN0_STA) & (1U << i2c_clk);
		spm_write(PERI_PDN0_CLR, pdn);	/* power on I2C */
#else
		pdn = clock_is_on(i2c_clk);
		if (!pdn)
			enable_clock(i2c_clk, "spm_i2c");
#endif
		spm_write(base + OFFSET_CONTROL, 0x0);	/* init I2C_CONTROL */
		spm_write(base + OFFSET_TRANSAC_LEN, 0x1);	/* init I2C_TRANSAC_LEN */
		spm_write(base + OFFSET_EXT_CONF, 0x1800);	/* init I2C_EXT_CONF */
		spm_write(base + OFFSET_IO_CONFIG, 0x3);	/* init I2C_IO_CONFIG */
		spm_write(base + OFFSET_HS, 0x102);	/* init I2C_HS */
	} else if ((0 == onoff) && (1 == i2c_onoff)) {
		i2c_onoff = 0;
#if 1
		spm_write(PERI_PDN0_SET, pdn);	/* restore I2C power */
#else
		if (!pdn)
			disable_clock(i2c_clk, "spm_i2c");
#endif
	} else
		ASSERT(1);
}

static void spm_suspend_pre_process(struct pwr_ctrl *pwrctrl)
{
	/* set LTE pd mode to avoid LTE power on after dual-vcore resume */
	spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) | 0xF);	/* set before dual-vcore suspend */

	/* set PMIC WRAP table for suspend power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_SUSPEND);

	/* FIXME: wait for dual-vcore suspend test finish. */
#if 0
	if (is_dualvcore_pdn(pwrctrl->pcm_flags))
		mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_HW_MODE);	/* if dual-vcore suspend enable, set VCORE_PND_EN to HW mode */
	else
#endif
		mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_SW_MODE);	/* if dual-vcore suspend disable, set VCORE_PND_EN to SW mode */
#if 0
	spm_write(0xF0000204, (spm_read(0xF0000204) | 0x15));

	spm_write(0xF020900C, (spm_read(0xF020900C) & ~0x8880));
	spm_write(0xF0209010, (spm_read(0xF020900C) & ~0x808));
#endif
#if 0
	spm_write(0xF0000048, (spm_read(0xF0000048) | 0x10000));

	spm_write(0xF000f5cc, spm_read(0xF000f5cc) & ~(1 << 0));	/* need */
	spm_write(0xF000f5cc, spm_read(0xF000f5cc) & ~(1 << 4));	/* need */
	spm_write(0xF000f5cc, spm_read(0xF000f5cc) & ~(1 << 8));	/* need */

	spm_write(0xF00125cc, spm_read(0xF00125cc) & ~(1 << 0));	/* need */
	spm_write(0xF00125cc, spm_read(0xF00125cc) & ~(1 << 8));	/* need */

	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 24));	/* need */
	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 16));	/* need */
	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 11));	/* need */
	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 10));	/* need */
	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 9));	/* need */
	spm_write(0xF000f5c8, spm_read(0xF000f5c8) & ~(1 << 0));	/* need */

	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 24));	/* need */
	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 16));	/* need */
	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 11));	/* meed */
	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 10));	/* need */
	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 9));	/* need */
	spm_write(0xF00125c8, spm_read(0xF00125c8) & ~(1 << 0));	/* need */

#if 0
	spm_write(0xF0006218, 0x32);
	spm_write(0xF000621C, 0x32);
	spm_write(0xF0006220, 0x32);
#endif
#if 0
	spm_write(0xF00062A0, 0x32);
	spm_write(0xF00062A4, 0x32);
	spm_write(0xF00062A8, 0x32);
	spm_write(0xF00062AC, 0x32);
	spm_write(0xF00062B0, 0x32);
#endif
#endif
#if 0
	/* 0x1000F5C8[0] = 1, 0x100125C8[0] = 1             //ALLCK_EN SW mode */
	spm_write(0xF000F5C8, spm_read(0xF000F5C8) | 0x1);
	spm_write(0xF00125C8, spm_read(0xF00125C8) | 0x1);
#if 0
	/* 0x1000F640[4] = 1, 0x10012640[4] = 1              //ALLCK_EN = 1 */
	spm_crit2("dramc 0x1000F640 = 0x%x, 0x10012640 = 0x%x\n", spm_read(0xF000F640),
		  spm_read(0xF0012640));
	/* 0x1000F5C8[4] = 1, 0x100125C8[4] = 1             //mempllout_off for ALLCK_EN bypass delay chain */
	spm_crit2("dramc 0x1000F5C8 = 0x%x, 0x100125C8 = 0x%x\n", spm_read(0xF000F5C8),
		  spm_read(0xF00125C8));
	/* 0x1000F5CC[8] = 0, 0x100125CC[8] = 0            //Phase sync HW mode (by mempllout_off) */
	/* 0x1000F5CC[12] = 1, 0x100125CC[12] = 1        //mempllout_off for phase sync bypass delay chain */
	spm_crit2("dramc 0x1000F5CC = 0x%x, 0x100125CC = 0x%x\n", spm_read(0xF000F5CC),
		  spm_read(0xF00125CC));
	/* 0x1000F640[2] = 1, 0x1000F690[2] = 1              //CHA/05PHY phase sync HW mode (by mempllout_off) */
	spm_crit2("dramc 0x1000F640 = 0x%x, 0x1000F690 = 0x%x\n", spm_read(0xF000F640),
		  spm_read(0xF000F690));
#endif
#endif

	spm_i2c_control(I2C_CHANNEL, 1);
}

static void spm_suspend_post_process(void)
{
	/* restore LTE pd mode after dual-vcore resume */
	spm_write(AP_PLL_CON7, spm_read(AP_PLL_CON7) & ~0xF);	/* set after dual-vcore resume */

	/* set VCORE_PND_EN to SW mode */
	mt_cpufreq_apply_pmic_cmd(IDX_SP_VCORE_PDN_EN_HW_MODE);

	/* set PMIC WRAP table for normal power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

	spm_i2c_control(I2C_CHANNEL, 0);
}

static void spm_set_sysclk_settle(void)
{
	u32 md_settle, settle;

	/* get MD SYSCLK settle */
	spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) | CC_SYSSETTLE_SEL);
	spm_write(SPM_CLK_SETTLE, 0);
	md_settle = spm_read(SPM_CLK_SETTLE);

	/* SYSCLK settle = MD SYSCLK settle but set it again for MD PDN */
	spm_write(SPM_CLK_SETTLE, SPM_SYSCLK_SETTLE - md_settle);
	settle = spm_read(SPM_CLK_SETTLE);

	spm_crit2("md_settle = %u, settle = %u\n", md_settle, settle);
}

/* FIXME: need to ask uart owner to provide API */
#if 0
static int spm_request_uart_to_sleep(void)
{
	u32 val1;
	int i = 0;

	/* request UART to sleep */
	val1 = spm_read(SPM_POWER_ON_VAL1);
	spm_write(SPM_POWER_ON_VAL1, val1 | R7_UART_CLK_OFF_REQ);

	/* wait for UART to ACK */
	while (!(spm_read(SPM_PCM_REG13_DATA) & R13_UART_CLK_OFF_ACK)) {
		if (i++ >= WAIT_UART_ACK_TIMES) {
			spm_write(SPM_POWER_ON_VAL1, val1);
			spm_error2("CANNOT GET UART SLEEP ACK (0x%x)\n", spm_read(PERI_PDN0_STA));
			dump_uart_reg();
			return -EBUSY;
		}
		udelay(10);
	}

	return 0;
}
#endif

static void spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* enable PCM WDT (normal mode) to start count if needed */
#if SPM_PCMWDT_EN
	{
		u32 con1;
		con1 = spm_read(SPM_PCM_CON1) & ~(CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
		spm_write(SPM_PCM_CON1, CON1_CFG_KEY | con1);

		if (spm_read(SPM_PCM_TIMER_VAL) > PCM_TIMER_MAX)
			spm_write(SPM_PCM_TIMER_VAL, PCM_TIMER_MAX);
		spm_write(SPM_PCM_WDT_TIMER_VAL, spm_read(SPM_PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
		spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_PCM_WDT_EN);
	}
#endif

	/* init PCM_PASR_DPD_0 for DPD */
	spm_write(SPM_PCM_PASR_DPD_0, 0);

	/* init PCM_PASR_DPD_2 for MD32 debug flag */
	spm_write(SPM_PCM_PASR_DPD_2, 0);

	__spm_kick_pcm_to_run(pwrctrl);
}

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
		spm_dormant_sta = mt_cpu_dormant(CPU_SHUTDOWN_MODE /* | DORMANT_SKIP_WFI */);
		switch (spm_dormant_sta) {
		case MT_CPU_DORMANT_RESET:
			break;
		case MT_CPU_DORMANT_ABORT:
			break;
		case MT_CPU_DORMANT_BREAK:
			break;
		case MT_CPU_DORMANT_BYPASS:
			break;
		}
	} else {
		spm_dormant_sta = -1;
		spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) | 0x10);
		wfi_with_sync();
		spm_write(CA7_BUS_CONFIG, spm_read(CA7_BUS_CONFIG) & ~0x10);
	}

	if (is_infra_pdn(pwrctrl->pcm_flags))
		mtk_uart_restore();
}

static void spm_clean_after_wakeup(void)
{
	/* disable PCM WDT to stop count if needed */
#if SPM_PCMWDT_EN
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | (spm_read(SPM_PCM_CON1) & ~CON1_PCM_WDT_EN));
#endif

	__spm_clean_after_wakeup();
}

static wake_reason_t spm_output_wake_reason(struct wake_status *wakesta, struct pcm_desc *pcmdesc)
{
	wake_reason_t wr;

	wr = __spm_output_wake_reason(wakesta, pcmdesc, true);

	spm_crit2("big core = %d, suspend dormant state = %d\n", SPM_CTRL_BIG_CPU, spm_dormant_sta);
	spm_crit2("SPM_PCM_PASR_DPD_2 = 0x%x\n", spm_read(SPM_PCM_PASR_DPD_2));
	if (0 != spm_ap_mdsrc_req_cnt)
		spm_crit2("warning: spm_ap_mdsrc_req_cnt = %d, r7[ap_mdsrc_req] = 0x%x\n",
			  spm_ap_mdsrc_req_cnt, spm_read(SPM_POWER_ON_VAL1) & (1 << 17));

	if (wakesta->r12 & WAKE_SRC_EINT)
		mt_eint_print_status();

#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & WAKE_SRC_CCIF_MD)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
#endif

	return wr;
}

#if SPM_PWAKE_EN
static u32 spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
		period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
		if (period <= 0) {
			spm_warning("CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		spm_crit2("pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}
#endif

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	unsigned long flags;

	if (spm_is_wakesrc_invalid(wakesrc))
		return -EINVAL;

	spin_lock_irqsave(&__spm_lock, flags);
	if (enable) {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = wakesrc;
		else
			__spm_suspend.pwrctrl->wake_src |= wakesrc;
	} else {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = 0;
		else
			__spm_suspend.pwrctrl->wake_src &= ~wakesrc;
	}
	spin_unlock_irqrestore(&__spm_lock, flags);

	return 0;
}

/*
 * wakesrc: WAKE_SRC_XXX
 */
u32 spm_get_sleep_wakesrc(void)
{
	return __spm_suspend.pwrctrl->wake_src;
}


wake_reason_t spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	u32 sec = 0;
	int wd_ret;
	struct wake_status wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;
	struct wd_api *wd_api;
	static wake_reason_t last_wr = WR_NONE;
	struct pcm_desc *pcmdesc = __spm_suspend.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_suspend.pwrctrl;
	struct spm_lp_scen *lpscen;

	lpscen = spm_check_talking_get_lpscen(&__spm_suspend, &spm_flags);
	pcmdesc = lpscen->pcmdesc;
	pwrctrl = lpscen->pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	set_pwrctrl_pcm_data(pwrctrl, spm_data);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(-1 /* FIXME */ , last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret)
		wd_api->wd_suspend_notify();

	spin_lock_irqsave(&__spm_lock, flags);
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	spm_suspend_pre_process(pwrctrl);

	spm_set_sysclk_settle();

	spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	if (request_uart_to_sleep()) {
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	spm_kick_pcm_to_run(pwrctrl);

	spm_trigger_wfi_for_sleep(pwrctrl);

	__spm_get_wakeup_status(&wakesta);

	spm_clean_after_wakeup();

	last_wr = spm_output_wake_reason(&wakesta, pcmdesc);

 RESTORE_IRQ:
	spm_suspend_post_process();

	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&__spm_lock, flags);

	if (!wd_ret)
		wd_api->wd_resume_notify();

	return last_wr;
}

bool spm_is_md_sleep(void)
{
	return !((spm_read(SPM_PCM_REG13_DATA) & R13_MD1_SRCLKENA) |
		 (spm_read(SPM_PCM_REG13_DATA) & R13_MD2_SRCLKENA));
}

#if 0				/* No connsys */
bool spm_is_conn_sleep(void)
{
	/* need to check */
}
#endif

void spm_set_wakeup_src_check(void)
{
	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xFFFFFFFF);

	/* set wakeup event */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~WAKE_SRC_FOR_SUSPEND);
}

bool spm_check_wakeup_src(void)
{
	/* check wanek event raw status */
	if (spm_read(SPM_SLEEP_ISR_RAW_STA))
		return 1;
	else
		return 0;
}

void spm_poweron_config_set(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_md32_sram_con(u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	/* enable register control */
	spm_write(SPM_MD32_SRAM_CON, value);
	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_ap_mdsrc_req(u8 set)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	if (set) {
		if (spm_ap_mdsrc_req_cnt < 0) {
			spm_crit2("warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n", set,
				  spm_ap_mdsrc_req_cnt);
			goto AP_MDSRC_REC_CNT_ERR;
		}

		spm_ap_mdsrc_req_cnt++;

		/* enable register control */
		spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

		/* Assert ap_mdsrc_req */
		spm_write(SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1) | (1 << 17));

		/* if md_apsrc_req = 1'b0, wait 26M settling time (3ms) */
		if (0 == (spm_read(SPM_PCM_REG13_DATA) & R13_AP_MD1SRC_ACK))
			mdelay(3);

		/* Check ap_mdsrc_ack = 1'b1 */
		while (0 == (spm_read(SPM_PCM_REG13_DATA) & R13_AP_MD1SRC_ACK));
	} else {
		spm_ap_mdsrc_req_cnt--;

		if (spm_ap_mdsrc_req_cnt < 0) {
			spm_crit2("warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n", set,
				  spm_ap_mdsrc_req_cnt);
			goto AP_MDSRC_REC_CNT_ERR;
		}

		if (0 == spm_ap_mdsrc_req_cnt) {
			/* enable register control */
			spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

			/* init r7 with POWER_ON_VAL1 for de-assert ap_mdsrc_req[17] */
			spm_write(SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1) & ~(1 << 17));
		}

	}

 AP_MDSRC_REC_CNT_ERR:
	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_report_wakeup_event(struct mt_wake_event *we, int code)
{
	unsigned long flags;
	struct mt_wake_event *head;
	struct mt_wake_event_map *evt;

	static char *ev_desc[] = {
		"RTC", "WIFI", "WAN", "USB",
		"PWR", "HALL", "BT", "CHARGER",
	};

	spin_lock_irqsave(&spm_lock, flags);
	head = mt_wake_event;
	mt_wake_event = we;
	we->parent = head;
	we->code = code;
	mt_wake_event = we;
	spin_unlock_irqrestore(&spm_lock, flags);
	pr_err("%s: WAKE EVT: %s#%d (parent %s#%d)\n",
	       __func__, we->domain, we->code,
	       head ? head->domain : "NONE", head ? head->code : -1);
	evt = spm_map_wakeup_event(we);
	if (evt && evt->we != WEV_NONE) {
		char *name = (evt->we >= 0
			      && evt->we < ARRAY_SIZE(ev_desc)) ? ev_desc[evt->we] : "UNKNOWN";
		pm_report_resume_irq(evt->irq);
		pr_err("%s: WAKEUP from source %d [%s]\n", __func__, evt->we, name);
	}
}
EXPORT_SYMBOL(spm_report_wakeup_event);

struct mt_wake_event *spm_get_wakeup_event(void)
{
	return mt_wake_event;
}
EXPORT_SYMBOL(spm_get_wakeup_event);


void spm_output_sleep_option(void)
{
	spm_notice("PWAKE_EN:%d, PCMWDT_EN:%d, BYPASS_SYSPWREQ:%d, I2C_CHANNEL:%d\n",
		   SPM_PWAKE_EN, SPM_PCMWDT_EN, SPM_BYPASS_SYSPWREQ, I2C_CHANNEL);
}

MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
