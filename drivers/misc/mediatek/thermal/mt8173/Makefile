include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/thermal

obj-$(CONFIG_THERMAL)   += mtk_thermal_platform.o
obj-$(CONFIG_THERMAL)	+= mtk_ts_pa_thput.o

ifeq ($(CONFIG_SLOANE), y)

obj-$(CONFIG_CPU_THERMAL)	+= mtk_cpufreq_cooling.o
obj-$(CONFIG_THERMAL)   += mtk_gpufreq_cooling.o

ifneq ($(CONFIG_MTK_COMBO_COMM),)
    obj-$(CONFIG_THERMAL)	+= mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL)   += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL)	+= mtk_ts_abb.o
obj-$(CONFIG_THERMAL)	+= mtk_ts_all_ts.o
obj-$(CONFIG_THERMAL)	+= mtk_ts_pmic.o

endif
