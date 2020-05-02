include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y	+= -I$(srctree)

obj-y	+= mtk_thermal_monitor.o

ifeq ($(CONFIG_ARCH_MT8173), y)
obj-y	+= mt8173/
endif

obj-$(CONFIG_THERMAL)  += $(subst ",,$(CONFIG_MTK_PLATFORM))/
