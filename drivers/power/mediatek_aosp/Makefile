include $(srctree)/drivers/misc/mediatek/Makefile.custom
obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_meter.o mt_battery_common.o mt_switch_charging.o mt_bm_data.o

ifeq ($(CONFIG_MTK_BQ24297_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq24297.o mt_otg_helper_func.o
endif

ifeq ($(CONFIG_MTK_BQ25890_SUPPORT),y)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq25890.o mt_otg_helper_func.o
endif

obj-$(CONFIG_MTK_PMIC) += mt_battery_meter_hal_6397.o
