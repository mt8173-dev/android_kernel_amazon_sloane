include $(srctree)/drivers/misc/mediatek/Makefile.custom

include $(srctree)/drivers/misc/mediatek/Makefile.custom
VCOREFS_PLATFORM_DRIVER_DIR :=  $(srctree)/drivers/misc/mediatek/vcorefs/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_ARCH_MT8173) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-$(CONFIG_ARCH_MT8173) += -I$(VCOREFS_PLATFORM_DRIVER_DIR)
