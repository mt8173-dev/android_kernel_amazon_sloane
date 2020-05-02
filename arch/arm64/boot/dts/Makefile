dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
# For 8173 projects
dtb-$(CONFIG_ARCH_MT8173) += mt8173_blofeld_tb.dtb
dtb-$(CONFIG_ARCH_MT8173) += tb8173evb.dtb
dtb-$(CONFIG_ARCH_MT8173) += tb8173p1.dtb
dtb-$(CONFIG_ARCH_MT8173) += tb8173p1_therm.dtb
dtb-$(CONFIG_ARCH_MT8173) += tb8173fpga.dtb
dtb-$(CONFIG_ARCH_MT8173) += bx8173p1.dtb
dtb-$(CONFIG_ARCH_MT8173) += tb8173p1_gpu.dtb
dtb-$(CONFIG_ARCH_MT8173) += magna.dtb
dtb-$(CONFIG_ARCH_MT8173) += sloane.dtb

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
