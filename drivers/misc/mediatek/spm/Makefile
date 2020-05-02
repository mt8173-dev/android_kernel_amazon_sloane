include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/i2c

obj-y	+= mt_spm.o
obj-y	+= mt_spm_internal.o
obj-$(CONFIG_PM)	+= mt_spm_fs.o
obj-y	+= mt_spm_sleep.o
obj-y	+= mt_spm_dpidle.o
obj-y	+= mt_spm_sodi.o
obj-y	+= mt_spm_mcdi.o
obj-y	+= mt_spm_ddrdfs.o
#obj-y	+= mt_spm_ddrdfs.o
