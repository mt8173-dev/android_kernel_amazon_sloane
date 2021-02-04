
# Makefile for the linux kernel.
#
# Only aarch32 need to build core/ (need machine descriptor)

include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_ARM64), y)
## Fix-me: marked others for early porting
#ifeq ($(CONFIG_MTK_INTERNAL), yes)
#ifeq ($(MTK_USE_RESERVED_EXT_MEM), yes)
#KBUILD_CFLAGS += -DMTK_USE_RESERVED_EXT_MEM
#endif
#endif
#
obj-y += mt_cpuxgpt.o
obj-y += mt_gpt.o
obj-y += mt_pm_ldo.o
obj-y += mt_sleep.o
obj-y += mt_cpufreq.o
obj-y += mt_cpufreq_tlp.o
obj-y += mt_static_power.o
#obj-y += mt_cpu_ss.o
obj-y += mt_freqhopping.o
obj-y += mt_pm_init.o
obj-y += mt_ptp2.o
obj-y += mt_spm_mtcmos.o
# obj-y += mt_clkmgr_common.o
# obj-y += mt_clkmgr.o
obj-y += mt_idle.o
obj-y += mt_dcm.o

obj-y += mt_dramc.o

obj-y += innercache.o
# Mediatek psci wrapper
obj-$(CONFIG_MTK_PSCI)	+= mt_psci.o

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mt_ptp.o
#obj-y += mt_ptp2.o
#obj-y += mt_ptp2_little.o
obj-y += mt_gpufreq.o
endif

# Only build project if they exists, this is a workaround for projects
# that still have their custom folder.
ifneq (,$(wildcard $(kbuild-dir)/$(ARCH_MTK_PROJECT)/Makefile))
obj-y += $(ARCH_MTK_PROJECT)/
endif

MT_CPUIDLE:=y
ifneq ($(MT_CPUIDLE),y)
obj-y += mt_dbg.o
obj-y += mt_dormant.o
else
obj-y += mt_cpuidle64.o
KBUILD_CFLAGS += -DMT_CPUIDLE
KBUILD_AFLAGS += -DMT_CPUIDLE
##KBUILD_CFLAGS += -DMT_DORMANT_UT
endif
#
#obj-y += cpu_dormant.o

obj-y += mt_hotplug_strategy_main.o mt_hotplug_strategy_core.o mt_hotplug_strategy_procfs.o mt_hotplug_strategy_cpu.o mt_hotplug_strategy_api.o mt_hotplug_strategy_algo.o

obj-$(CONFIG_COMMON_CLK_DEBUG) += mt_clkdbg.o
obj-$(CONFIG_TB8173_EVB) += board-tb8173evb.o
obj-$(CONFIG_TB8173_P1) += board-tb8173p1.o
obj-$(CONFIG_BX8173_P1) += board-bx8173p1.o
obj-$(CONFIG_TB8173_BLOFELD) += board-tb8173_blofeld.o
obj-$(CONFIG_MAGNA) += board-magna.o
obj-$(CONFIG_SLOANE) += board-sloane.o
obj-$(CONFIG_BX8173_P12) += board-bx8173p12.o
obj-$(CONFIG_MTK_SENSOR_SUPPORT) += mt_cam.o

endif
