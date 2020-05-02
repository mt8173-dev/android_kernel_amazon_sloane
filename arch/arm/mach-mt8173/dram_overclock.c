#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/system.h>
#include <asm/delay.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_freqhopping.h>
#include <mach/emi_bwl.h>
#include <mach/mt_typedefs.h>
#include <mach/memory.h>
#include <mach/mt_sleep.h>

#if 0
#define DRAMC_WRITE_REG(val, offset)     do { \
				      (*(volatile unsigned int *)(DRAMC0_BASE + (offset))) = (unsigned int)(val); \
				      (*(volatile unsigned int *)(DDRPHY_BASE + (offset))) = (unsigned int)(val); \
				      (*(volatile unsigned int *)(DRAMC_NAO_BASE + (offset))) = (unsigned int)(val); \
				      } while (0)

static int dram_clk;
static DEFINE_SPINLOCK(lock);

__attribute__ ((__section__(".sram.func")))
int sram_set_dram(int clk)
{
	/* set ac timing */
	if (clk == 293) {
		DRAMC_WRITE_REG(0x778844D5, 0x0);
		DRAMC_WRITE_REG(0xC0064301, 0x7C);
		DRAMC_WRITE_REG(0x9F0C8CA0, 0x44);
		DRAMC_WRITE_REG(0x03406348, 0x8);
		DRAMC_WRITE_REG(0x11662742, 0x1DC);
		DRAMC_WRITE_REG(0x01001010, 0x1E8);
		DRAMC_WRITE_REG(0x17000000, 0xFC);
		udelay(10);
	}
	return 0;
}

static void enable_gpu(void)
{
	enable_clock(MT_CG_MFG_HYD, "MFG");
	enable_clock(MT_CG_MFG_G3D, "MFG");
	enable_clock(MT_CG_MFG_MEM, "MFG");
	enable_clock(MT_CG_MFG_AXI, "MFG");
}

static void disable_gpu(void)
{
	disable_clock(MT_CG_MFG_AXI, "MFG");
	disable_clock(MT_CG_MFG_MEM, "MFG");
	disable_clock(MT_CG_MFG_G3D, "MFG");
	disable_clock(MT_CG_MFG_HYD, "MFG");
}

static ssize_t dram_overclock_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d\n", get_ddr_type(), mt_fh_get_dramc());
}

static ssize_t dram_overclock_store(struct device_driver *driver, const char *buf, size_t count)
{
	int clk, ret = 0;

	clk = simple_strtol(buf, 0, 10);
	dram_clk = mt_fh_get_dramc();
	if (clk == dram_clk) {
		printk(KERN_ERR "dram_clk:%d, is equal to user inpu clk:%d\n", dram_clk, clk);
		return count;
	}
	spin_lock(&lock);
	ret = sram_set_dram(clk);
	if (ret < 0)
		printk(KERN_ERR "dram overclock in sram failed:%d, clk:%d\n", ret, clk);
	spin_unlock(&lock);
	ret = mt_fh_dram_overclock(clk);
	if (ret < 0)
		printk(KERN_ERR "dram overclock failed:%d, clk:%d\n", ret, clk);
	printk(KERN_INFO "In %s pass, dram_clk:%d, clk:%d\n", __func__, dram_clk, clk);
	return count;
}

extern unsigned int RESERVED_MEM_SIZE_FOR_TEST_3D;
extern unsigned int FB_SIZE_EXTERN;
extern unsigned int get_max_DRAM_size(void);
static ssize_t ftm_dram_3d_show(struct device_driver *driver, char *buf)
{
	unsigned int pa_3d_base =
	    PHYS_OFFSET + get_max_DRAM_size() - RESERVED_MEM_SIZE_FOR_TEST_3D - FB_SIZE_EXTERN;
	return snprintf(buf, PAGE_SIZE, "%u\n", pa_3d_base);
}

static ssize_t ftm_dram_3d_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

static ssize_t ftm_dram_mtcmos_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t ftm_dram_mtcmos_store(struct device_driver *driver, const char *buf, size_t count)
{
	int enable;
	enable = simple_strtol(buf, 0, 10);
	if (enable == 1) {
		enable_gpu();
		printk(KERN_INFO "enable in %s, enable:%d\n", __func__, enable);
	} else if (enable == 0) {
		disable_gpu();
		printk(KERN_INFO "enable in %s, disable:%d\n", __func__, enable);
	} else
		printk(KERN_ERR "dram overclock failed:%s, enable:%d\n", __func__, enable);
	return count;
}
#endif

void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1)
{
	if (segment_rank1 == 0xFF) {	/* all segments of rank1 are not reserved -> rank1 enter DPD */
		slp_dpd_en(1);
	}

	if (segment_rank1 != 0xFF)
		slp_pasr_en(1, segment_rank0 | (segment_rank1 << 16));
}

void exit_pasr_dpd_config(void)
{
	slp_dpd_en(0);
	slp_pasr_en(0, 0);
}

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	ptr = vmalloc(PAGE_SIZE * 2);
	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	printk("Test DRAM start address 0x%x\n", (unsigned int)ptr);
	printk("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++) {
		MEM32_BASE[i] = 0;
	}

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			vfree(ptr);
			return -1;
		} else {
			MEM32_BASE[i] = 0xffffffff;
		}
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -2;
		} else
			MEM32_BASE[i] = 0x00;
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		MEM8_BASE[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (MEM8_BASE[i] != pattern8++) {
			vfree(ptr);
			return -3;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			vfree(ptr);
			return -4;
		}
		pattern8 += 2;
	}

	/* === Verify pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		MEM16_BASE[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (MEM16_BASE[i] != pattern16++) {
			vfree(ptr);
			return -5;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			vfree(ptr);
			return -6;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			vfree(ptr);
			return -7;
		} else {
			MEM32_BASE[i] = 0xa5a5a5a5;
		}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			vfree(ptr);
			return -8;
		} else {
			MEM8_BASE[i * 4] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			vfree(ptr);
			return -9;
		} else {
			MEM8_BASE[i * 4 + 2] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			vfree(ptr);
			return -10;
		} else {
			MEM8_BASE[i * 4 + 1] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			vfree(ptr);
			return -11;
		} else {
			MEM8_BASE[i * 4 + 3] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with ffff Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			vfree(ptr);
			return -12;
		} else {
			MEM16_BASE[i * 2 + 1] = 0xffff;
		}
	}


	/* === Read Check then Fill Memory with ffff Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			vfree(ptr);
			return -13;
		} else {
			MEM16_BASE[i * 2] = 0xffff;
		}
	}
    /*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -14;
		}
	}


    /************************************************
    * Additional verification
    ************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++) {
		MEM_BASE[i] = PATTERN1;
	}


	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];

		if (value != PATTERN1) {
			vfree(ptr);
			return -15;
		}
		MEM_BASE[i] = PATTERN2;
	}


	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -16;
		}
		MEM_BASE[i] = PATTERN1;
	}


	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -17;
		}
		MEM_BASE[i] = PATTERN2;
	}


	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -18;
		}
		MEM_BASE[i] = PATTERN1;
	}


	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -19;
		}
	}


	/* === 1/2/4-byte combination test === */
	i = (unsigned int)MEM_BASE;
	while (i < (unsigned int)MEM_BASE + (size << 2)) {
		*((unsigned char *)i) = 0x78;
		i += 1;
		*((unsigned char *)i) = 0x56;
		i += 1;
		*((unsigned short *)i) = 0x1234;
		i += 2;
		*((unsigned int *)i) = 0x12345678;
		i += 4;
		*((unsigned short *)i) = 0x5678;
		i += 2;
		*((unsigned char *)i) = 0x34;
		i += 1;
		*((unsigned char *)i) = 0x12;
		i += 1;
		*((unsigned int *)i) = 0x12345678;
		i += 4;
		*((unsigned char *)i) = 0x78;
		i += 1;
		*((unsigned char *)i) = 0x56;
		i += 1;
		*((unsigned short *)i) = 0x1234;
		i += 2;
		*((unsigned int *)i) = 0x12345678;
		i += 4;
		*((unsigned short *)i) = 0x5678;
		i += 2;
		*((unsigned char *)i) = 0x34;
		i += 1;
		*((unsigned char *)i) = 0x12;
		i += 1;
		*((unsigned int *)i) = 0x12345678;
		i += 4;
	}
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			vfree(ptr);
			return -20;
		}
	}


	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	MEM8_BASE[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;
		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			MEM8_BASE[waddr8] = pattern8 + 1;
		if (MEM8_BASE[raddr8] != pattern8) {
			vfree(ptr);
			return -21;
		}
		pattern8++;
	}


	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	MEM16_BASE[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			MEM16_BASE[i + 1] = pattern16 + 1;
		if (MEM16_BASE[i] != pattern16) {
			vfree(ptr);
			return -22;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	MEM32_BASE[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			MEM32_BASE[i + 1] = pattern32 + 1;
		if (MEM32_BASE[i] != pattern32) {
			vfree(ptr);
			return -23;
		}
		pattern32++;
	}
	printk("complex R/W mem test pass\n");
	vfree(ptr);
	return 1;
}

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;
	ret = Binning_DRAM_complex_mem_test();
	if (ret > 0) {
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
	}
}

static ssize_t complex_mem_test_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

#if 0
DRIVER_ATTR(emi_clk_test, 0664, dram_overclock_show, dram_overclock_store);
DRIVER_ATTR(emi_clk_3d_test, 0664, ftm_dram_3d_show, ftm_dram_3d_store);
DRIVER_ATTR(emi_clk_mtcmos_test, 0664, ftm_dram_mtcmos_show, ftm_dram_mtcmos_store);
#else
DRIVER_ATTR(emi_clk_mem_test, 0664, complex_mem_test_show, complex_mem_test_store);
#endif

static struct device_driver dram_test_drv = {
	.name = "emi_clk_test",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
};

extern char __ssram_text, _sram_start, __esram_text;
int __init dram_test_init(void)
{
	int ret;
#if 0
	unsigned char *dst = &__ssram_text;
	unsigned char *src = &_sram_start;
#endif

	ret = driver_register(&dram_test_drv);
	if (ret) {
		printk(KERN_ERR "fail to create the dram_test driver\n");
		return ret;
	}
#if 0
	ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_test);
	if (ret) {
		printk(KERN_ERR "fail to create the dram_test sysfs files\n");
		return ret;
	}
	ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_3d_test);
	if (ret) {
		printk(KERN_ERR "fail to create the ftm_dram_3d_drv sysfs files\n");
		return ret;
	}
	ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_mtcmos_test);
	if (ret) {
		printk(KERN_ERR "fail to create the ftm_dram_mtcmos_drv sysfs files\n");
		return ret;
	}

	for (dst = &__ssram_text; dst < (unsigned char *)&__esram_text; dst++, src++) {
		*dst = *src;
	}
#else
	ret = driver_create_file(&dram_test_drv, &driver_attr_emi_clk_mem_test);
	if (ret) {
		printk(KERN_ERR "fail to create the ftm_dram_mtcmos_drv sysfs files\n");
		return ret;
	}
#endif

	return 0;
}
arch_initcall(dram_test_init);
