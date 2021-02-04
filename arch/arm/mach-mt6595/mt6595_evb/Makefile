
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifneq ($(CONFIG_ARCH_MTK_PROJECT),)
ARCH_MTK_PROJECT := $(shell echo $(CONFIG_ARCH_MTK_PROJECT))
obj-y   += core/
obj-y   += dct/dct/
endif

obj-y   +=  lcm/
obj-y   +=  camera/camera/
#obj-y   +=  imgsensor/imx135_mipi_raw/
#obj-y   +=  imgsensor/ov5648_mipi_raw/
obj-y   +=  flashlight/constant_flashlight/
obj-y   +=  vibrator/
obj-y   +=  leds/
obj-y   +=  keypad/
obj-y   +=  sound/
obj-y   +=  accdet/
