#include <linux/io.h>
#include <mu3phy/mtk-phy.h>
#ifdef CONFIG_U3D_HAL_SUPPORT
/* #include <mu3d/hal/mu3d_hal_osal.h> */
#endif

#if 0 /*def CONFIG_U3_PHY_AHB_SUPPORT */

PHY_INT32 U3PhyWriteReg32(PHY_UINT32 addr, PHY_UINT32 data)
{
	writel(data, (void __iomem *)addr);
	return 0;
}

PHY_INT32 U3PhyReadReg32(PHY_UINT32 addr)
{
	return readl((void __iomem *)addr);
}

PHY_INT32 U3PhyWriteReg8(PHY_UINT32 addr, PHY_UINT8 data)
{
	void __iomem *addr_tmp = (void __iomem *)(addr & ~0x3);
	unsigned int tmp = readl(addr_tmp);
	unsigned int offset = (addr & 0x3) * 8;
	unsigned int msk = 0xff << offset;
	writel(((tmp & ~(msk)) | ((data << offset) & (msk))), addr_tmp);

	return 0;
}

PHY_INT8 U3PhyReadReg8(PHY_UINT32 addr)
{
	return ((readl((void __iomem *)addr) >> ((addr % 4) * 8)) & 0xff);
}

#endif
