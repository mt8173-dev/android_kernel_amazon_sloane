# drivers/video/mtk/Makefile
#
# Makefile for MT6516 video driver.
#

include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/staging/android \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM)


include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)
endif
ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),"nxp_tda19989")
	ccflags-y +=	-DHDMI_TDA19989
endif
ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),"sil9024")
	obj-y	+= sil9024/
endif
ifeq ($(CONFIG_MTK_PLATFORM),"mt8135")
obj-y	:= hdmitx.o debug.o  hdmitx_test.o
endif


ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
obj-y  += hdmitx/$(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
obj-y   += mhltx/$(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifneq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),)
ccflags-y += -I$(srctree)/include/trustzone/
endif
