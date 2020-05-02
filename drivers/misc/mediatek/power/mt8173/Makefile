include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_PMIC)          += pmic_mt6397.o da9212.o pmic_chr_type_det.o

ifeq ($(CONFIG_MTK_BATTERY_THROTTLING),y)
    obj-$(CONFIG_MTK_BATTERY_THROTTLING) += mt_battery_throttle.o
endif