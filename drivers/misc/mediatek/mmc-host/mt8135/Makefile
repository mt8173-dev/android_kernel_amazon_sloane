include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)
obj-$(CONFIG_MTK_MMC) += dbg.o
obj-$(CONFIG_MTK_MMC) += sd.o mt_sd_misc.o ffu.o


# kdump/kpainc interface move to lk
ifeq ($(CONFIG_MTK_AEE_IPANIC),y)
obj-$(CONFIG_MTK_MMC) += mt_dump.o
endif
ifeq ($(CONFIG_MTK_EMMC_SUPPORT_OTP),y)
obj-$(CONFIG_MTK_MMC) += emmc_otp.o
endif
