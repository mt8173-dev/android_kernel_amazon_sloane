include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/devapc/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/include/trustzone/
endif

obj-y += emi_bwl.o
obj-y += emi_mpu.o
