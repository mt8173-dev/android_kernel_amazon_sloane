# drivers/image/jpeg/Makefile
include $(srctree)/drivers/misc/mediatek/Makefile.custom

EXTRA_CFLAGS = -DSDEBUG_T

#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

jpeg_driver-objs := jpeg_cmdq.o jpeg_drv_6589_enc.o jpeg_drv_6589_dec.o jpeg_drv.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mt8173 \

#jpeg_driver-objs := jpeg_drv_6589_enc.o  jpeg_drv.o

# EOF
