include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o
obj-$(CONFIG_MTK_VIBRATOR) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
#ccflags-y := -Idrivers/staging/android/
ccflags-y  += -I$(LED_PLATFORM_DRIVER_DIR)/
