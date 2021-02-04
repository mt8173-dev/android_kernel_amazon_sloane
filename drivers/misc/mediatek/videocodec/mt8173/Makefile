include $(srctree)/drivers/misc/mediatek/Makefile.custom

vcodec_kernel_driver-objs :=        \
    videocodec_kernel_driver.o      \
    common/val/src/val.o			\
    common/drv/src/drv_base.o

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
endif

obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_driver.o


ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/videocodec/$(CONFIG_MTK_PLATFORM)/common/drv/inc		\
    -I$(srctree)/include/linux/vcodec                   \
    -I$(srctree)/include/linux/vcodec/kernel
