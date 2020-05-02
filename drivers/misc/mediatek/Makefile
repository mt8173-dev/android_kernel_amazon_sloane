
include $(srctree)/drivers/misc/mediatek/Makefile.custom

#ifneq ($(filter "MT7662",$(CONFIG_MTK_COMBO_CHIP)),)
ifeq ($(CONFIG_MTK_COMBO_CHIP_MT7662),y)
obj-y += conn_combo/
else
include $(srctree)/drivers/misc/mediatek/combo/Makefile.combo
endif

# Enable warning to error in all MTK subdir.
#subdir-ccflags-y += -Werror

MTK_FM_RX_AUDIO_FLAG := $(addprefix -D, $(shell echo $(CONFIG_MTK_FM_RX_AUDIO) | tr a-z A-Z))
subdir-ccflags-y += $(MTK_FM_RX_AUDIO_FLAG)

obj-$(CONFIG_CONN_MD)	+= conn_md/
obj-$(CONFIG_MD32_SUPPORT)   += md32/
obj-y   +=  aee/
obj-y   +=  boot/
obj-y	+=  boot_reason/
obj-$(CONFIG_MTK_BTIF)   +=  btif/
obj-$(CONFIG_ARCH_MT8173)   +=  chip/
obj-$(CONFIG_ARCH_MT6595)   +=  cmdq/
obj-$(CONFIG_ARCH_MT8173)   +=  cmdq/
obj-$(CONFIG_ARCH_MT8135)   +=  dumpEfuseInfo/
obj-$(CONFIG_MTK_DUM_CHAR)   +=  dum-char/
obj-$(CONFIG_MTK_GPT_SCHEME_SUPPORT)   += partition/
obj-y   +=  eint/
obj-y   +=  gcpu/
obj-y   +=  gdma/
obj-$(CONFIG_MTK_LEGACY_GPIO_DRIVER)    +=      gpio/
obj-y   +=  jpeg/
obj-y   +=  kernel/
obj-$(CONFIG_ARCH_MT6595)   +=  keypad/
obj-y   +=  lcm/
#obj-$(CONFIG_ARCH_MT8135)   +=  lcm/
obj-y   +=  m4u/
obj-$(CONFIG_ZRAM) +=  mlog/
obj-y   +=  mmp/
obj-$(CONFIG_MTKPASR)       +=  mtkpasr/
obj-y   +=  mtprof/
#obj-$(CONFIG_USB_MU3D_DRV)  +=  mu3d/
#obj-$(CONFIG_MU3_PHY)       +=  mu3phy/
#obj-$(CONFIG_MTK_XHCI)   +=  xhci/
obj-y   +=  ssusb/
obj-y   +=  pa/
obj-y   +=  pmic_wrap/
obj-y   +=  vcorefs/
obj-$(CONFIG_ARCH_MT6595)   +=  pmt/
obj-$(CONFIG_ARCH_MT6595)   +=  power_gs/
obj-y   +=  sched/
obj-y   +=  selinux_warning/
obj-$(CONFIG_MTK_SYSTRACKER)   +=  systracker/
obj-y   +=  touchpanel/
obj-y   +=  uart/
obj-$(CONFIG_ARCH_MT8135)   +=  video/
obj-y   +=  videox/
obj-y   +=  wdk/
obj-y   +=  xlog/
obj-$(CONFIG_ARCH_MT8173)   +=  spm/
obj-$(CONFIG_ARCH_MT8173)   +=  rtc/
obj-$(CONFIG_MTK_VIBRATOR)   +=  vibrator/
obj-$(CONFIG_MTK_LEDS)   +=  leds/
obj-y   +=  emi/

ifneq ($(MTK_BATTERY_NO_HAL),yes)
obj-$(CONFIG_ARCH_MT8135)   += power/
endif

ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y   += hdmi/
obj-y   += ext_disp/
endif

ifneq ($(CONFIG_MTK_PLATFORM),)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
obj-$(CONFIG_HAVE_CMMB_FEATURE)   += cmmbdrv/$(MTK_PLATFORM)/
obj-y   += devapc/$(MTK_PLATFORM)/
obj-y   += devinfo/$(MTK_PLATFORM)/
obj-y   += dispsys/$(MTK_PLATFORM)/
obj-$(CONFIG_MTK_LDVT) += ldvt/$(MTK_PLATFORM)/
obj-y   += masp/
obj-y   += mmc-host/$(MTK_PLATFORM)/
#obj-y   += power-loss-test/$(MTK_PLATFORM)/
obj-y   += power/$(MTK_PLATFORM)/

obj-y   += gpu/

obj-y   += spi/$(MTK_PLATFORM)/
obj-y   += wdt/$(MTK_PLATFORM)/
obj-y   += ddr/$(MTK_PLATFORM)/
obj-y   += smi/$(MTK_PLATFORM)/
endif

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y   +=  accdet/
obj-y   +=  almk/
obj-y   +=  auxadc/
# gator-driver is not custom-release and will be removed
obj-y   += $(call all-subdirs-with-makefile,gator-driver)
obj-y   +=  ccci/
obj-y   +=  ccmni/
obj-$(CONFIG_ARCH_MT8173)	+=  dbg_dump/
obj-y   +=  dual_ccci/
obj-y   +=  eccci/
obj-y   +=  emd_ctl/
obj-$(CONFIG_MTK_ETM) += etm/
obj-y   +=  fmradio/
obj-y   +=  freqhopping/
obj-y   +=  gps/
obj-$(CONFIG_MTK_LEDS)   +=  leds/
obj-y   +=  mhl/
obj-$(CONFIG_MTK_NFC)   += nfc/
obj-y   +=  pwm/
obj-y   +=  ram_console/
obj-y   +=  rtc/
obj-y	+=  thermal/

obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)  +=  trustzone/
obj-$(CONFIG_MTK_VIBRATOR)   +=  vibrator/
obj-y   +=  videocodec/

endif#!CONFIG_MTK_FPGA
obj-y   +=  hwmon/
obj-y   +=  sync/

ifeq ($(CONFIG_ARM64),y)
obj-y   += camera/
obj-y   += cam_cal/
obj-y   += imgsensor/
endif #!CONFIG_ARM64
obj-y   += eeprom/
obj-y   += flashlight/
obj-y   +=  freqhopping/
obj-y   += lens/
ifeq ($(CONFIG_ARM64),y)
obj-y   += irq/
obj-y   += mach/  #temp place for gpt
endif
obj-y   += dynamic_boost/

obj-$(CONFIG_MTK_IRRX_SUPPORT)   +=  irrx/
ifneq ($(filter y, $(CONFIG_ARM_PSCI) $(CONFIG_ARM64)),)
obj-y   +=  atf_log/
endif
