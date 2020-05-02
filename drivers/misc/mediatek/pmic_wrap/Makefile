
include $(srctree)/drivers/misc/mediatek/Makefile.custom
obj-$(CONFIG_ARCH_MT8135) +=  mt8135_pmic_wrap.o
obj-$(CONFIG_ARCH_MT6595) +=  mt_pmic_wrap.o mt6595/

include $(srctree)/drivers/misc/mediatek/Makefile.custom
WRAP_PLATFORM_DRIVER_DIR :=  $(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_ARCH_MT8173) += mt_pmic_wrap.o
obj-$(CONFIG_ARCH_MT8173) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-$(CONFIG_ARCH_MT8173) += -I$(WRAP_PLATFORM_DRIVER_DIR)
