include $(srctree)/drivers/misc/mediatek/Makefile.custom

CONFIG_CUSTOM_KERNEL_LCM := $(shell echo $(patsubst "%",%,$(CONFIG_CUSTOM_KERNEL_LCM)) | tr a-z A-Z)

CUSTOM_KERNEL_FLAGS := $(foreach var,$(CONFIG_CUSTOM_KERNEL_LCM), -D$(var))
ccflags-y += $(CUSTOM_KERNEL_FLAGS)

obj-y   +=  mt65xx_lcm_list.o

obj-$(CONFIG_CPT_CLAP070WP03XG_SN65DSI83)   +=  cpt_clap070wp03xg_sn65dsi83.o sn65dsi83_i2c.o
obj-$(CONFIG_HX8369_RGB_6585_FPGA)   +=  hx8369_rgb_6585_fpga.o
obj-$(CONFIG_NT35521_WXGA_DSI_VDO_HH060IA)   +=  nt35521_wxga_dsi_vdo_hh060ia.o
obj-$(CONFIG_NT51012_WXGA_DSI_VDO) += nt51012_wxga_dsi_vdo.o
obj-$(CONFIG_CM_N070ICE_DSI_VDO_MT8135)	+= cm_n070ice_dsi_vdo_mt8135.o
obj-$(CONFIG_R69429_WQXGA_DSI_VDO)	+= r69429_wqxga_dsi_vdo.o
obj-$(CONFIG_R69429_WQXGA_DSI_CMD)	+= r69429_wqxga_dsi_cmd.o
obj-$(CONFIG_NT35595_FHD_DSI_CMD_TRULY_TPS65132)	+= nt35595_fhd_dsi_cmd_truly_tps65132.o


ifeq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif
