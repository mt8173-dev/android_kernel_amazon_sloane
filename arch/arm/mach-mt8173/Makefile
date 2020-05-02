
# Makefile for the linux kernel.
#

include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/

# Fix-me: marked others for early porting
ifeq ($(CONFIG_MTK_INTERNAL), y)
ifeq ($(CONFIG_MTK_USE_RESERVED_EXT_MEM), y)
KBUILD_CFLAGS += -DMTK_USE_RESERVED_EXT_MEM
endif
endif

obj-y := core.o
obj-y += irq.o
obj-y += timer.o
obj-y += mt_devs.o
obj-y += mt_gpt.o
obj-y += mt_boot.o
obj-y += board.o
obj-y += ca7_timer.o
obj-y += mt_cpuxgpt.o
obj-y += mt_dma.o
#obj-y += mt_reg_dump.o
obj-y += mt_device_apc.o
obj-y += mt_pm_init.o
ifeq ($(CONFIG_SMP),y)
obj-y += mt-smp.o
obj-y += hotplug.o
obj-y += mt-headsmp.o
obj-y += mt_hotplug.o
obj-y += mt_hotplug_mechanism.o
endif
# TO-FIX add # for do early porting in JB migration
obj-y += eint.o
obj-$(CONFIG_MT65XX_TRACER) += pmu_v7.o mon_interface.o mt_mon.o mt_emi_bm.o
obj-y += emi_bwl.o
obj-y += emi_mpu.o
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/cmdq
obj-y += innercache.o
#obj-y += mt_l2c.o
obj-$(CONFIG_MTK_CIRQ) += mt_cirq.o
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/power \
             -I$(MTK_PATH_PLATFORM)/drivers/gpu/mali/mali/include \
             -I$(MTK_PATH_SOURCE)/drivers/wdk
obj-y += mt_spm.o mt_spm_internal.o mt_spm_fs.o mt_spm_sleep.o
obj-y += mt_spm_dpidle.o mt_spm_sodi.o mt_spm_mcdi.o mt_spm_talking.o
obj-y += mt_spm_mtcmos.o
obj-y += mt_sleep.o
#obj-y += mt_gpufreq.o mt_cpu_ss.o
obj-y += mt_cpufreq.o mt_cpu_ss.o
#obj-y += mt_emifreq.o
ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mt_gpufreq.o
endif

obj-y += board-common-audio.o

MT_CPUIDLE:=y
ifneq ($(MT_CPUIDLE),y)
obj-y += mt_dbg.o
obj-y += mt_dormant.o
else
obj-y += mt_cpuidle.o
KBUILD_CFLAGS += -DMT_CPUIDLE
KBUILD_AFLAGS += -DMT_CPUIDLE
endif

obj-y += cpu_dormant.o
obj-y += mt_idle.o
obj-y += mt_dcm.o
obj-y += mt_clkmgr_common.o
obj-y += mt_clkmgr.o
obj-y += sbchk_base.o
#obj-y += mtk_ccci_helper.o

ifneq ($(CONFIG_MTK_CCCI_DEVICES),y)
ccflags-y  += -D__USING_DUMMY_CCCI_API__
endif

#obj-y += mt_static_power.o

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mt_ptp.o
endif
obj-y += mt_cci400.o
obj-y += mssv.o

obj-y += dram_overclock.o

# Hibernation
AFLAGS_swsusp.o	:= -DTEXT_OFFSET=$(TEXT_OFFSET)
obj-$(CONFIG_HIBERNATION)	+= cpu_hibernate.o swsusp.o

KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=neon)

#KBUILD_CFLAGS += -DMT_DORMANT_UT
#KBUILD_CFLAGS += -DMT_SMP_VIRTUAL_BOOT_ADDR

# Module project related part
obj-y += $(ARCH_MTK_PROJECT)/
