#######################################
# Sepcify source files
#######################################
include $(srctree)/drivers/misc/mediatek/Makefile.custom

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MASP_CORE_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/masp

asf_link := asf
lib_obj := $(MTK_PLATFORM)/module/sec_mod.o

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)

include $(MASP_CORE_DRIVER_DIR)/asf/Makefile.core
include $(MASP_CORE_DRIVER_DIR)/$(MTK_PLATFORM)/mach/Makefile.mach

ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT),y)
CCCI_CUSTOM_DRIVER_DIR := $(call my-dir)$(call to-root,$(obj))mediatek/custom/out/$(call lc,$(MTK_PROJECT))/kernel/ccci
lib_obj += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
include $(MASP_CORE_DRIVER_DIR)/asf/Makefile.crypto
lib_obj += $(crypto_obj)
lib_obj += $(asf_link)/auth/sec_wrapper.o
endif

else
ccflags-y += -DMTK_SECURITY_MODULE_LITE
core_obj += $(asf_link)/core/sec_ops.o
core_obj += $(asf_link)/core/alg_sha1.o
ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
mach_obj += $(MTK_PLATFORM)/mach/mtee/sec_uid.o
else
mach_obj += $(MTK_PLATFORM)/mach/sec_uid.o
endif
ccflags-y += -I$(src)/asf/asf_inc
endif

# BUILD-IN
obj-y += sec.o


# HEADER FILES
ccflags-y += -I$(src)/module

# INCLUDE ASP LIBRARY
sec-y += $(lib_obj) $(core_obj) $(mach_obj)
