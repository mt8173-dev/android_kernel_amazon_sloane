include $(srctree)/drivers/misc/mediatek/Makefile.custom

# drivers/cmdq

ccflags-y += -I$(srctree)/drivers/misc/mediatek/dispsys/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/cmdq
#            -I$(MTK_PATH_SOURCE)/../hardware/dpframework/inc \

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

obj-y += cmdq_record.o
obj-y += cmdq_core.o
obj-y += cmdq_mutex.o
obj-y += cmdq_mdp.o
obj-y += cmdq_driver.o

obj-y += cmdq_test.o

# new add from mp11
obj-y += cmdq_device.o
obj-y += cmdq_mdp_common.o
obj-y += cmdq_mmp.o
obj-y += cmdq_platform.o
