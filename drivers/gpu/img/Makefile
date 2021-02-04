ccflags-y += -Werror
$(warning ----baiyang CONFIG_ARCH_MT8173 = $(CONFIG_ARCH_MT8173))
ifeq ($(CONFIG_ARCH_MT8173), y)
  obj-y += mt8173/
endif

ifeq ($(CONFIG_ARCH_MT6595), y)
  #obj-$(CONFIG_PVR_RGX_MT6595) += mt6595/m1.3_ED2673322/
  obj-y += mt6595/
endif

ifeq ($(CONFIG_ARCH_MT8135), y)
  obj-$(CONFIG_PVR_RGX_MT8135) += mt8135/
endif
