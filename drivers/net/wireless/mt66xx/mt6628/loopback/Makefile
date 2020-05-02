# only support 6628 currently, do not support on-binary
ifneq ($(MTK_COMBO_CHIP_MT6628),)
# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
	ccflags-y += -DLINUX -DMT6628 -DDBG=1
	obj-$(CONFIG_MTK_COMBO_WIFI) += mtk_wifi_loopback.o
# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
	mtk_wifi_loopback-objs  := fw_download.o mt6628-core.o
endif
