include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += flashlight.o

subdir-ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_FLASHLIGHT_DUMMYFLASHLIGHT_SUPPORT),y)
subdir-ccflags-y += -DDUMMY_FLASHLIGHT
flashlight-y += dummy_flashlight/dummy_flashlight.o
endif

ifeq ($(CONFIG_MTK_FLASHLIGHT_CONSTANTFLASHLIGHT_SUPPORT),y)
subdir-ccflags-y += -DCONSTANT_FLASHLIGHT
flashlight-y += constant_flashlight/leds_strobe.o
endif

obj-y += src/
