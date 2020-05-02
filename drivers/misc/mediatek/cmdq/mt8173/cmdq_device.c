#include "cmdq_device.h"
#include "cmdq_core.h"
/* #include <mach/mt_irq.h> */

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
typedef struct CmdqModuleBaseVA {
	long MMSYS_CONFIG;
	long MDP_RDMA0;
	long MDP_RDMA1;
	long MDP_RSZ0;
	long MDP_RSZ1;
	long MDP_RSZ2;
	long MDP_WDMA;
	long MDP_WROT0;
	long MDP_WROT1;
	long MDP_TDSHP0;
	long MDP_TDSHP1;
	long MM_MUTEX;
	long VENC;
	long MSDC0;
	long AUDIO;
	long DISP_PWM0;
} CmdqModuleBaseVA;

typedef struct CmdqDeviceStruct {
	struct device *pDev;
	long regBaseVA;		/* considering 64 bit kernel, use long */
	long regBasePA;
	uint32_t irqId;
	uint32_t irqSecId;
} CmdqDeviceStruct;

static CmdqModuleBaseVA gCmdqModuleBaseVA;
static CmdqDeviceStruct gCmdqDev;

struct device *cmdq_dev_get(void)
{
	return gCmdqDev.pDev;
}

const uint32_t cmdq_dev_get_irq_id(void)
{
	return gCmdqDev.irqId;
}

const uint32_t cmdq_dev_get_irq_secure_id(void)
{
	return gCmdqDev.irqSecId;
}

const long cmdq_dev_get_module_base_VA_GCE(void)
{
	return gCmdqDev.regBaseVA;
}

const long cmdq_dev_get_module_base_PA_GCE(void)
{
	return gCmdqDev.regBasePA;
}

const long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void)
{
	return gCmdqModuleBaseVA.MMSYS_CONFIG;
}

const long cmdq_dev_get_module_base_VA_MDP_RDMA0(void)
{
	return gCmdqModuleBaseVA.MDP_RDMA0;
}

const long cmdq_dev_get_module_base_VA_MDP_RDMA1(void)
{
	return gCmdqModuleBaseVA.MDP_RDMA1;
}

const long cmdq_dev_get_module_base_VA_MDP_RSZ0(void)
{
	return gCmdqModuleBaseVA.MDP_RSZ0;
}

const long cmdq_dev_get_module_base_VA_MDP_RSZ1(void)
{
	return gCmdqModuleBaseVA.MDP_RSZ1;
}

const long cmdq_dev_get_module_base_VA_MDP_RSZ2(void)
{
	return gCmdqModuleBaseVA.MDP_RSZ2;
}

const long cmdq_dev_get_module_base_VA_MDP_WDMA(void)
{
	return gCmdqModuleBaseVA.MDP_WDMA;
}

const long cmdq_dev_get_module_base_VA_MDP_WROT0(void)
{
	return gCmdqModuleBaseVA.MDP_WROT0;
}

const long cmdq_dev_get_module_base_VA_MDP_WROT1(void)
{
	return gCmdqModuleBaseVA.MDP_WROT1;
}

const long cmdq_dev_get_module_base_VA_MDP_TDSHP0(void)
{
	return gCmdqModuleBaseVA.MDP_TDSHP0;
}

const long cmdq_dev_get_module_base_VA_MDP_TDSHP1(void)
{
	return gCmdqModuleBaseVA.MDP_TDSHP1;
}

const long cmdq_dev_get_module_base_VA_MM_MUTEX(void)
{
	return gCmdqModuleBaseVA.MM_MUTEX;
}

const long cmdq_dev_get_module_base_VA_MSDC0(void)
{
	return gCmdqModuleBaseVA.MSDC0;
}

const long cmdq_dev_get_module_base_VA_AUDIO(void)
{
	return gCmdqModuleBaseVA.AUDIO;
}

const long cmdq_dev_get_module_base_VA_DISP_PWM0(void)
{
	return gCmdqModuleBaseVA.DISP_PWM0;
}

const long cmdq_dev_get_module_base_VA_VENC(void)
{
	return gCmdqModuleBaseVA.VENC;
}

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, name);
	VA = (unsigned long)of_iomap(node, 0);
	CMDQ_LOG("DEV: VA(%s): 0x%lx\n", name, VA);

	return VA;
}

void cmdq_dev_free_module_base_VA(const long VA)
{
	iounmap((void *)VA);
}

void cmdq_dev_init_module_base_VA(void)
{
	memset(&gCmdqModuleBaseVA, 0, sizeof(CmdqModuleBaseVA));

#ifdef CMDQ_OF_SUPPORT
	gCmdqModuleBaseVA.MMSYS_CONFIG =
	    cmdq_dev_alloc_module_base_VA_by_name("mediatek,MMSYS_CONFIG");
	gCmdqModuleBaseVA.MDP_RDMA0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_RDMA0");
	gCmdqModuleBaseVA.MDP_RDMA1 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_RDMA1");
	gCmdqModuleBaseVA.MDP_RSZ0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_RSZ0");
	gCmdqModuleBaseVA.MDP_RSZ1 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_RSZ1");
	gCmdqModuleBaseVA.MDP_RSZ2 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_RSZ2");
	gCmdqModuleBaseVA.MDP_WDMA = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_WDMA");
	gCmdqModuleBaseVA.MDP_WROT0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_WROT0");
	gCmdqModuleBaseVA.MDP_WROT1 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_WROT1");
	gCmdqModuleBaseVA.MDP_TDSHP0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_TDSHP0");
	gCmdqModuleBaseVA.MDP_TDSHP1 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MDP_TDSHP1");
	gCmdqModuleBaseVA.MM_MUTEX = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MM_MUTEX");
	gCmdqModuleBaseVA.VENC = cmdq_dev_alloc_module_base_VA_by_name("mediatek,VENC");
	gCmdqModuleBaseVA.MSDC0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MSDC0");
	gCmdqModuleBaseVA.AUDIO = cmdq_dev_alloc_module_base_VA_by_name("mediatek,AUDIO");
	gCmdqModuleBaseVA.DISP_PWM0 = cmdq_dev_alloc_module_base_VA_by_name("mediatek,DISP_PWM");
#else
	/* use ioremap to allocate */
	gCmdqModuleBaseVA.MMSYS_CONFIG = (long)ioremap(MMSYS_CONFIG_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_RDMA0 = (long)ioremap(MDP_RDMA0_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_RDMA1 = (long)ioremap(MDP_RDMA1_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_RSZ0 = (long)ioremap(MDP_RSZ0_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_RSZ1 = (long)ioremap(MDP_RSZ1_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_RSZ2 = (long)ioremap(MDP_RSZ2_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_WDMA = (long)ioremap(MDP_WDMA_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_WROT0 = (long)ioremap(MDP_WROT0_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_WROT1 = (long)ioremap(MDP_WROT1_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_TDSHP0 = (long)ioremap(MDP_TDSHP0_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MDP_TDSHP1 = (long)ioremap(MDP_TDSHP1_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MM_MUTEX = (long)ioremap(MM_MUTEX_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.VENC = (long)ioremap(VENC_BASE_PA, 0x1000);
	gCmdqModuleBaseVA.MSDC0 = (long)ioremap(MSDC0_BASE_PA, 0x10000);
	gCmdqModuleBaseVA.AUDIO = (long)ioremap(AUDIO_BASE_PA, 0x10000);
	gCmdqModuleBaseVA.DISP_PWM0 = (long)ioremap(DISP_PWM0_PA, 0x10000);

#endif
}

void cmdq_dev_deinit_module_base_VA(void)
{
#ifdef CMDQ_OF_SUPPORT
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MMSYS_CONFIG());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_RDMA0());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_RDMA1());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_RSZ0());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_RSZ1());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_RSZ2());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_WDMA());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_WROT0());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_WROT1());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_TDSHP0());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MDP_TDSHP1());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MM_MUTEX());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_VENC());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_MSDC0());
	cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_AUDIO());

	memset(&gCmdqModuleBaseVA, 0, sizeof(CmdqModuleBaseVA));
#endif
}

const long of_getPA(struct device_node *dev, int index)
{
	struct resource res;
	if (of_address_to_resource(dev, index, &res))
		return 0;
	return res.start;

}

void cmdq_dev_init(struct platform_device *pDevice)
{
#ifdef CMDQ_OF_SUPPORT
	struct device_node *node = pDevice->dev.of_node;
#endif
	/* init cmdq device dependent data */
	do {
		memset(&gCmdqDev, 0x0, sizeof(struct CmdqDeviceStruct));

		gCmdqDev.pDev = &pDevice->dev;
#ifdef CMDQ_OF_SUPPORT
		gCmdqDev.regBaseVA = (unsigned long)of_iomap(node, 0);
		/* gCmdqDev.regBasePA = GCE_BASE_PA; */
		gCmdqDev.regBasePA = of_getPA(node, 0);
		if (0 == gCmdqDev.regBasePA)
			CMDQ_ERR("ERROR!!! get GCE PA from device tree error. PA:%ld",
				 gCmdqDev.regBasePA);

		gCmdqDev.irqId = irq_of_parse_and_map(node, 0);
		gCmdqDev.irqSecId = irq_of_parse_and_map(node, 1);
#else
		gCmdqDev.regBaseVA = (long)ioremap(GCE_BASE_PA, 0x1000);
		gCmdqDev.regBasePA = GCE_BASE_PA;
		gCmdqDev.irqId = CQ_DMA_IRQ_BIT_ID;
		gCmdqDev.irqSecId = CQ_DMA_SEC_IRQ_BIT_ID;
#endif

		CMDQ_LOG
		    ("[CMDQ] platform_dev: dev: %p, PA: %lx, VA: %lx, irqId: %d,  irqSecId:%d\n",
		     gCmdqDev.pDev, gCmdqDev.regBasePA, gCmdqDev.regBaseVA, gCmdqDev.irqId,
		     gCmdqDev.irqSecId);
	} while (0);

	/* init module VA */
	cmdq_dev_init_module_base_VA();
}

void cmdq_dev_deinit(void)
{
	cmdq_dev_deinit_module_base_VA();

	/* deinit cmdq device dependent data */
	do {
#ifdef CMDQ_OF_SUPPORT
		cmdq_dev_free_module_base_VA(cmdq_dev_get_module_base_VA_GCE());
		gCmdqDev.regBaseVA = 0;
#else
		/* do nothing */
#endif
	} while (0);
}
