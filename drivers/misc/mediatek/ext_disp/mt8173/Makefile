
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/misc/mediatek/ext_disp/  \
             -I$(srctree)/drivers/misc/mediatek/video/     \
             -I$(srctree)/drivers/misc/mediatek/dispsys/ \
             -I$(srctree)/drivers/misc/mediatek/sync/ \
             -I$(srctree)/drivers/misc/mediatek/mhl/driver/mhl_tx_drv/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/ \
             -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)/inc/\
             -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)/\
             -I$(srctree)/drivers/staging/android/ion/ \
             -I$(srctree)/drivers/misc/mediatek/videox/$(MTK_PLATFORM) \

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),nxp_tda19989)
	ccflags-y +=	-DHDMI_TDA19989
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),Sii8338)
	ccflags-y +=	-DMHL_SII8338
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),Sii8348)
	ccflags-y +=	-DMHL_SII8348
endif

ifeq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

obj-y	:= extd_ddp.o extd_debug.o  extd_drv.o extd_utils.o extd_lcm.o Kara_1280x720.o extd_fence.o
