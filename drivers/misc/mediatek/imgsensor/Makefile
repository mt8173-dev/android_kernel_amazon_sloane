include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_SENSOR_SUPPORT) += src/
obj-$(CONFIG_MTK_SENSOR_SUPPORT) += imgsensor.o

ccflags-y += -I$(src)/inc -I$(srctree)/include/linux/camera

ifeq ($(CONFIG_MTK_IMGSENSOR_HI253YUV_SUPPORT),y)
ccflags-y += -DHI253_YUV
imgsensor-y += hi253_yuv/hi253yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_HI704YUV_SUPPORT),y)
ccflags-y += -DHI704_YUV
imgsensor-y += hi704_yuv/hi704yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_HI704RAW_SUPPORT),y)
ccflags-y += -DHI704_RAW
imgsensor-y += hi704_raw/hi704raw_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV2722MIPIRAW_SUPPORT),y)
ccflags-y += -DOV2722_MIPI_RAW
imgsensor-y += ov2722_mipi_raw/ov2722mipiraw_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV2724MIPIRAW_SUPPORT),y)
ccflags-y += -DOV2724_MIPI_RAW
imgsensor-y += ov2724_mipi_raw/ov2724mipiraw_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV9724MIPIRAW_SUPPORT),y)
ccflags-y += -DOV9724_MIPI_RAW
imgsensor-y += ov9724_mipi_raw/ov9724mipiraw_Sensor.o
endif


ifeq ($(CONFIG_MTK_IMGSENSOR_IMX135MIPIRAW_SUPPORT),y)
ccflags-y += -DIMX135_MIPI_RAW
imgsensor-y += imx135_mipi_raw/imx135mipiraw_Sensor.o
imgsensor-y += imx135_mipi_raw/imx135_otp.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV5648MIPI_SUPPORT),y)
ccflags-y += -DOV5648_MIPI_RAW
imgsensor-y += ov5648_mipi_raw/ov5648mipi_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_S5K2P8MIPI_SUPPORT),y)
ccflags-y += -DS5K2P8_MIPI_RAW
imgsensor-y += s5k2p8_mipi_raw/s5k2p8mipi_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_S5K4ECGXMIPIYUV_SUPPORT),y)
ccflags-y += -DS5K4ECGX_MIPI_YUV
imgsensor-y += s5k4ecgx_mipi_yuv/s5k4ecgxmipi_yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_OV2659YUV_SUPPORT),y)
ccflags-y += -DOV2659_YUV
imgsensor-y += ov2659_yuv/ov2659yuv_Sensor.o
endif

ifeq ($(CONFIG_MTK_IMGSENSOR_S5K5E2YAMIPIRAW_SUPPORT),y)
ccflags-y += -DS5K5E2YA_MIPI_RAW
imgsensor-y += s5k5e2ya_mipi_raw/s5k5e2yamipiraw_Sensor.o
endif

