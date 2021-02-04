/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt_afe_def.h"
#include <linux/types.h>
#include "mt_afe_control.h"
#include "mt_afe_debug.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <mach/mt_pwm.h>
#include <linux/input.h>
#include <sound/jack.h>
#include <sound/max97236.h>


static bool ext_speaker_amp_switch_state;
static int ext_speaker_amp_gpio_no = -1;

#define MAX97236_MCLK_RATE 2000000
static struct snd_soc_jack blofeld_jack;
static int blofeld_init_max97236(struct snd_soc_pcm_runtime *rtd);


/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link blofeld_dais[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .codec_dai_name = MT_SOC_CODEC_TXDAI_NAME,
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .codec_dai_name = MT_SOC_CODEC_RXDAI_NAME,
	 },
	{
	 .name = "HDMI_PCM_OUTPUT",
	 .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "BTSCO",
	 .stream_name = MT_SOC_BTSCO_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 .platform_name = MT_SOC_BTSCO_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "DL1AWB_CAPTURE",
	 .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "MultiMedia2_Capture",
	 .stream_name = MT_SOC_UL2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL2_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "HDMI_RAW_OUTPUT",
	 .stream_name = MT_SOC_HDMI_RAW_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_RAW_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_RAW_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "SPDIF_OUTPUT",
	 .stream_name = MT_SOC_SPDIF_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_SPDIF_CPU_DAI_NAME,
	 .platform_name = MT_SOC_SPDIF_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	 {
	 .name = "I2S0_AWB_CAPTURE",
	 .stream_name = MT_SOC_I2S0_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_I2S0_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "PLATOFRM_CONTROL",
	 .stream_name = MT_SOC_ROUTING_STREAM_NAME,
	 .cpu_dai_name = "snd-soc-dummy-dai",
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .no_pcm = 1,
	 },
#ifdef	CONFIG_SND_SOC_MAX97236
	{
	.name = "max97236-hpamp",
	.stream_name = "max97236",
	.cpu_dai_name = "snd-soc-dummy-dai",
	.platform_name = MT_SOC_ROUTING_PCM,
	.codec_dai_name = "max97236-hifi",
	.codec_name = "max97236.4-0040",
	.init = &blofeld_init_max97236,
	.no_pcm = 1,
	},
#endif
#ifdef CONFIG_SND_SOC_TFA98xx
             {
             .name = "TFA98xx_SPK_AMP", /* use tfa98xx naming */
             .stream_name = "TFA98xx_Speaker_Amp", /* use tfa98xx naming */
             .cpu_dai_name = "snd-soc-dummy-dai",
             .platform_name = MT_SOC_ROUTING_PCM,
             .codec_dai_name = "tfa98xx_codec",   /* it depends on tfa98xx driver?s implementation */
             .codec_name = "tfa98xx.4-0034",  /* it depends on tfa98xx driver?s implementation */
             .no_pcm = 1,
             },
#endif //CONFIG_SND_SOC_TFA98xx
};

/* Configure MT6391 to provide 2 mhz clk to MAX97236  */
static void blofeld_enable_max97236_clk(void)
{
	struct pwm_spec_config pwm_setting1;

	pwm_setting1.pwm_no = PWM7;
	pwm_setting1.mode = PWM_MODE_FIFO;
	pwm_setting1.clk_div = CLK_DIV1;
	pwm_setting1.clk_src = PWM_CLK_NEW_MODE_BLOCK;
	pwm_setting1.pmic_pad = true;
	pwm_setting1.intr = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 0x0b;
	pwm_setting1.PWM_MODE_FIFO_REGS.HDURATION = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.LDURATION = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting1.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xc0fc0fc0;
	pwm_setting1.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0x0fc0fc0f;
	pwm_setting1.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	pwm_set_spec_config(&pwm_setting1);
	mt_set_gpio_mode(GPIOEXT30, 1);
}

static int blofeld_init_max97236(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	/* Disable folowing pins at init time for power save
	 * They will be enabled if is requared later */
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPL");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPR");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_MIC");

	snd_soc_dapm_sync(&codec->dapm);

	max97236_set_key_div(codec, MAX97236_MCLK_RATE);

	ret = snd_soc_jack_new(codec, "h2w",
			       SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2
			       | SND_JACK_BTN_3 | SND_JACK_BTN_4 |
			       SND_JACK_BTN_5 | SND_JACK_HEADSET |
			       SND_JACK_LINEOUT, &blofeld_jack);
	if (ret) {
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(blofeld_jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	max97236_set_jack(codec, &blofeld_jack);

	max97236_detect_jack(codec);

	return 0;
}

static int blofeld_channel_cap_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int blofeld_channel_cap_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mt_afe_get_board_channel_type();
	return 0;
}

static const char *const blofeld_channel_cap[] = {
	"Stereo", "MonoLeft", "MonoRight"
};

static const char *const ext_speaker_amp_switch_function[] = {
	"Off", "On"
};

static const struct soc_enum blofeld_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(blofeld_channel_cap), blofeld_channel_cap),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ext_speaker_amp_switch_function),
			    ext_speaker_amp_switch_function),
};

static int blofeld_ext_speaker_amp_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0] && !ext_speaker_amp_switch_state) {
		if (gpio_is_valid(ext_speaker_amp_gpio_no))
			gpio_direction_output(ext_speaker_amp_gpio_no, 1);

		usleep_range(6000, 7000);	/* start-up time */
		ext_speaker_amp_switch_state = true;
	} else if (!ucontrol->value.integer.value[0] && ext_speaker_amp_switch_state) {
		if (gpio_is_valid(ext_speaker_amp_gpio_no))
			gpio_set_value(ext_speaker_amp_gpio_no, 0);

		ext_speaker_amp_switch_state = false;
	}
	return 0;
}

static int blofeld_ext_speaker_amp_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ext_speaker_amp_switch_state ? 1 : 0;
	return 0;
}

static const struct snd_kcontrol_new blofeld_controls[] = {
	SOC_ENUM_EXT("Board Channel Config", blofeld_control_enum[0],
		     blofeld_channel_cap_get, blofeld_channel_cap_set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", blofeld_control_enum[1],
		     blofeld_ext_speaker_amp_get, blofeld_ext_speaker_amp_set),
};

static struct snd_soc_card blofeld_card = {
	.name = "mt-snd-card",
	.dai_link = blofeld_dais,
	.num_links = ARRAY_SIZE(blofeld_dais),
	.controls = blofeld_controls,
	.num_controls = ARRAY_SIZE(blofeld_controls),
};

static int blofeld_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &blofeld_card;
	struct device *dev = &pdev->dev;
	int ret;

	pr_notice("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_MACHINE_NAME);
		pr_notice("%s set dev name %s\n", __func__, dev_name(dev));
	}

	ext_speaker_amp_gpio_no = of_get_named_gpio(dev->of_node, "ext-speaker-amp-gpio", 0);
	if (gpio_is_valid(ext_speaker_amp_gpio_no)) {
		ret = devm_gpio_request(dev, ext_speaker_amp_gpio_no,
					"blofeld-ext-speaker-amp-gpio");
		if (ret) {
			pr_err("%s devm_gpio_request fail %d\n", __func__, ret);
			return ret;
		}
	}

	/*Enable HS AMP CLK*/
	blofeld_enable_max97236_clk();

	card->dev = dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		return ret;
	}

	ret = mt_afe_platform_init(dev);
	if (ret) {
		pr_err("%s mt_afe_platform_init fail %d\n", __func__, ret);
		snd_soc_unregister_card(card);
		return ret;
	}

	mt_afe_debug_init();

	return 0;
}

static int blofeld_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	mt_afe_platform_deinit(&pdev->dev);

	mt_afe_debug_deinit();

	return 0;
}

static const struct of_device_id blofeld_machine_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_MACHINE_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, blofeld_machine_dt_match);

static struct platform_driver blofeld_machine_driver = {
	.driver = {
		   .name = MT_SOC_MACHINE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = blofeld_machine_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = blofeld_dev_probe,
	.remove = blofeld_dev_remove,
};

module_platform_driver(blofeld_machine_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC driver for Blofeld");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
