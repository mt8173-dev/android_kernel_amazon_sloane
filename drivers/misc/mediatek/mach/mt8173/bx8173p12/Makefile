
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_ALSPS_MTK_APDS9930),y)
obj-y	+=  alsps/APDS9930/
endif

ifeq ($(CONFIG_ACCELEROMETER_MTK_MPU6050),y)
obj-y   +=  accelerometer/mpu6050g/
endif

ifeq ($(CONFIG_GYROSCOPE_MTK_MPU6050),y)
obj-y   +=  gyroscope/mpu6050gy/
endif

#obj-y   +=  vibrator/
#obj-$(CONFIG_MTK_LEDS)   +=  leds/
#obj-y   +=  accdet/
