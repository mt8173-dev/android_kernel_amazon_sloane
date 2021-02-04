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
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include "mt_afe_digital_type.h"
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/wait.h>

enum {
	HDMI_LOOPBACK_NONE = 0,
	HDMI_LOOPBACK_SDATA0_TO_DL1,
	HDMI_LOOPBACK_SDATA1_TO_DL1,
	HDMI_LOOPBACK_SDATA2_TO_DL1,
	HDMI_LOOPBACK_SDATA3_TO_DL1,
};

struct mt_pcm_hdmi_priv {
	bool hw_params_is_done;
	bool prepared;
	bool clk_is_on;
	bool format_changed;
	bool sample_rate_changed;
	unsigned int hdmi_loop_type;
	unsigned int hdmi_sinegen_switch;
	unsigned int cached_sample_rate;
	struct snd_dma_buffer *hdmi_dma_buf;
};

static const unsigned int table_sgen_golden_values[64] = {
	0x0FE50FE5, 0x285E1C44, 0x3F4A285E, 0x53C73414,
	0x650C3F4A, 0x726F49E3, 0x7B6C53C7, 0x7FAB5CDC,
	0x7F02650C, 0x79776C43, 0x6F42726F, 0x60C67781,
	0x4E917B6C, 0x39587E27, 0x21EB7FAB, 0x09307FF4,
	0xF01A7F02, 0xD7A17CD6, 0xC0B67977, 0xAC3874ED,
	0x9AF36F42, 0x8D906884, 0x849360C6, 0x80545818,
	0x80FD4E91, 0x86884449, 0x90BD3958, 0x9F3A2DDA,
	0xB16E21EB, 0xC6A715A8, 0xDE140930, 0xF6CFFCA1,
	0x0FE5F01A, 0x285EE3BB, 0x3F4AD7A1, 0x53C7CBEB,
	0x650CC0B6, 0x726FB61C, 0x7B6CAC38, 0x7FABA323,
	0x7F029AF3, 0x797793BC, 0x6F428D90, 0x60C6887E,
	0x4E918493, 0x395881D8, 0x21EB8054, 0x0930800B,
	0xF01A80FD, 0xD7A18329, 0xC0B68688, 0xAC388B12,
	0x9AF390BD, 0x8D90977B, 0x84939F3A, 0x8054A7E7,
	0x80FDB16E, 0x8688BBB6, 0x90BDC6A7, 0x9F3AD225,
	0xB16EDE14, 0xC6A7EA57, 0xDE14F6CF, 0xF6CF035E
};


static void mt_pcm_hdmi_set_interconnection(unsigned int connection_state, unsigned int channels);


static struct snd_pcm_hardware mt_pcm_hdmi_hardware = {
	.info =
	    (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
	     SNDRV_PCM_INFO_MMAP_VALID),
	.formats = HDMI_FORMATS,
	.rates = HDMI_RATES,
	.rate_min = HDMI_RATE_MIN,
	.rate_max = HDMI_RATE_MAX,
	.channels_min = HDMI_CHANNELS_MIN,
	.channels_max = HDMI_CHANNELS_MAX,
	.buffer_bytes_max = HDMI_MAX_BUFFER_SIZE,
	.period_bytes_min = SOC_NORMAL_USE_PERIOD_SIZE_MIN,
	.period_bytes_max = (HDMI_MAX_BUFFER_SIZE / SOC_NORMAL_USE_PERIODS_MIN),
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static void mt_pcm_hdmi_set_interconnection(unsigned int connection_state, unsigned int channels)
{
	/* O30~O37: L/R/LS/RS/C/LFE/CH7/CH8 */
	switch (channels) {
	case 8:
		mt_afe_set_connection(connection_state, INTER_CONN_I36, INTER_CONN_O36);
		mt_afe_set_connection(connection_state, INTER_CONN_I37, INTER_CONN_O37);
	case 6:
		mt_afe_set_connection(connection_state, INTER_CONN_I34, INTER_CONN_O34);
		mt_afe_set_connection(connection_state, INTER_CONN_I35, INTER_CONN_O35);
	case 4:
		mt_afe_set_connection(connection_state, INTER_CONN_I32, INTER_CONN_O33);
		mt_afe_set_connection(connection_state, INTER_CONN_I33, INTER_CONN_O32);
	case 2:
		mt_afe_set_connection(connection_state, INTER_CONN_I30, INTER_CONN_O30);
		mt_afe_set_connection(connection_state, INTER_CONN_I31, INTER_CONN_O31);
		break;
	case 1:
		mt_afe_set_connection(connection_state, INTER_CONN_I30, INTER_CONN_O30);
		break;
	default:
		pr_warn("%s unsupported channels %u\n", __func__, channels);
		break;
	}
}

static int mt_pcm_hdmi_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &mt_pcm_hdmi_hardware);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_err("%s snd_pcm_hw_constraint_integer fail %d\n", __func__, ret);

	pr_info("%s substream->pcm->device = %d\n", __func__, substream->pcm->device);

	if (!priv->clk_is_on) {
		mt_afe_main_clk_on();
		mt_afe_emi_clk_on();
	}

	return ret;
}

static int mt_pcm_hdmi_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	if (priv->hw_params_is_done)
		priv->hw_params_is_done = false;
	if (priv->prepared) {
		if (!priv->clk_is_on) {
			mt_afe_disable_apll_tuner(runtime->rate);
			mt_afe_disable_apll(runtime->rate);
		}
		mt_afe_remove_ctx_substream(MT_AFE_MEM_CTX_HDMI);
		priv->prepared = false;
	}

	if (!priv->clk_is_on) {
		mt_afe_main_clk_off();
		mt_afe_emi_clk_off();
	}
	return 0;
}

static int mt_pcm_hdmi_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;
	unsigned int rate = params_rate(hw_params);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (ret < 0)
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);
	else if (ret >= 0)
		mt_afe_init_dma_buffer(MT_AFE_MEM_CTX_HDMI, runtime);

	pr_info("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%llx\n",
		__func__, runtime->dma_bytes, runtime->dma_area,
		(unsigned long long)runtime->dma_addr);

	pr_info("%s rate = %u cached_sample_rate = %u\n",
		__func__, rate, priv->cached_sample_rate);

	if (priv->cached_sample_rate != rate)
		priv->sample_rate_changed = true;
	else
		priv->sample_rate_changed = false;

	if (!priv->hw_params_is_done) {
		if (!priv->clk_is_on) {
			mt_afe_enable_apll(rate);
			mt_afe_enable_apll_tuner(rate);
		} else if (priv->sample_rate_changed || priv->format_changed) {
			if (priv->sample_rate_changed)
				pr_info("%s sample rate has changed\n", __func__);
			if (priv->format_changed)
				pr_info("%s format has changed\n", __func__);
			mt_afe_disable_hdmi_out();
			mt_afe_disable_hdmi_tdm();
			mt_afe_hdmi_clk_off();
			mt_afe_disable_apll_div_power(MT_AFE_I2S3_BCK, priv->cached_sample_rate);
			mt_afe_disable_apll_div_power(MT_AFE_I2S3, priv->cached_sample_rate);
			mt_afe_disable_apll_tuner(priv->cached_sample_rate);
			mt_afe_disable_apll(priv->cached_sample_rate);
			mt_afe_emi_clk_off();
			mt_afe_main_clk_off();
			mt_afe_main_clk_on();
			mt_afe_emi_clk_on();
			mt_afe_enable_apll(rate);
			mt_afe_enable_apll_tuner(rate);
		}
	}
	priv->cached_sample_rate = rate;
	priv->hw_params_is_done = true;
	return ret;
}

static int mt_pcm_hdmi_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int mt_pcm_hdmi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t mclk_div;

	pr_info("%s rate = %u channels = %u period_size = %lu\n",
		__func__, runtime->rate, runtime->channels, runtime->period_size);

	if (!priv->prepared) {
		if (!priv->clk_is_on || priv->sample_rate_changed || priv->format_changed) {
			pr_info("%s msleep +\n", __func__);
			msleep(200);
			pr_info("%s msleep -\n", __func__);
			mclk_div = mt_afe_set_mclk(MT_AFE_I2S3, runtime->rate);
			mt_afe_set_i2s3_bclk(mclk_div, runtime->rate, 2, 32);

			mt_afe_enable_apll_div_power(MT_AFE_I2S3, runtime->rate);
			mt_afe_enable_apll_div_power(MT_AFE_I2S3_BCK, runtime->rate);

			mt_afe_hdmi_clk_on();

			mt_afe_set_hdmi_tdm1_config(runtime->channels, MT_AFE_I2S_WLEN_32BITS);
			mt_afe_set_hdmi_tdm2_config(runtime->channels);
			mt_afe_set_hdmi_out_channel(runtime->channels);

			/* interconnection */
			mt_pcm_hdmi_set_interconnection(INTER_CONNECT, runtime->channels);

			mt_afe_enable_hdmi_out();

			mt_afe_enable_hdmi_tdm();

			priv->format_changed = false;
		}
	}

	priv->prepared = true;
	priv->clk_is_on = true;

	return 0;
}

static int mt_pcm_hdmi_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_info("%s period_size = %lu\n", __func__, runtime->period_size);

	mt_afe_add_ctx_substream(MT_AFE_MEM_CTX_HDMI, substream);

	mt_afe_enable_hdmi_out();

	mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_HDMI, MT_AFE_MEMIF_16_BIT);

	mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_HDMI);

	/* here to set interrupt */
	mt_afe_set_irq_counter(MT_AFE_IRQ_MCU_MODE_IRQ5, runtime->period_size);
	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ5, true);

	mt_afe_enable_afe(true);

	return 0;
}

static int mt_pcm_hdmi_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_info("%s\n", __func__);

	mt_afe_disable_hdmi_out();

	if (!priv->clk_is_on) {
		mt_pcm_hdmi_set_interconnection(INTER_DISCONNECT, runtime->channels);
		mt_afe_disable_hdmi_tdm();
	}

	mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_HDMI);

	mt_afe_set_irq_state(MT_AFE_IRQ_MCU_MODE_IRQ5, false);

	mt_afe_enable_afe(false);

	/* clean audio hardware buffer */
	mt_afe_reset_dma_buffer(MT_AFE_MEM_CTX_HDMI);

	if (!priv->clk_is_on) {
		mt_afe_hdmi_clk_off();
		mt_afe_disable_apll_div_power(MT_AFE_I2S3_BCK, runtime->rate);
		mt_afe_disable_apll_div_power(MT_AFE_I2S3, runtime->rate);
	}

	return 0;
}

static int mt_pcm_hdmi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mt_pcm_hdmi_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mt_pcm_hdmi_stop(substream);
	default:
		pr_warn("%s command %d not handled\n", __func__, cmd);
		break;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t mt_pcm_hdmi_pointer(struct snd_pcm_substream *substream)
{
	return mt_afe_update_hw_ptr(MT_AFE_MEM_CTX_HDMI);
}

static int hdmi_loopback_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);
	ucontrol->value.integer.value[0] = priv->hdmi_loop_type;
	return 0;
}

static int hdmi_loopback_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);

	if (priv->hdmi_loop_type == ucontrol->value.integer.value[0]) {
		pr_notice("%s dummy operation for %u\n", __func__, priv->hdmi_loop_type);
		return 0;
	}

	if (priv->hdmi_loop_type != HDMI_LOOPBACK_NONE) {
		mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I00, INTER_CONN_O03);
		mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I01, INTER_CONN_O04);
		mt_afe_disable_2nd_i2s_in();
		mt_afe_disable_hdmi_tdm_i2s_loopback();
		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC))
			mt_afe_disable_i2s_dac();

		mt_afe_dac_clk_off();
	}

	switch (ucontrol->value.integer.value[0]) {
	case HDMI_LOOPBACK_SDATA0_TO_DL1:
	case HDMI_LOOPBACK_SDATA1_TO_DL1:
	case HDMI_LOOPBACK_SDATA2_TO_DL1:
	case HDMI_LOOPBACK_SDATA3_TO_DL1:
		mt_afe_dac_clk_on();
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I00, INTER_CONN_O03);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I01, INTER_CONN_O04);
		mt_afe_set_i2s_asrc_config(priv->cached_sample_rate);
		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_I2S, priv->cached_sample_rate);
		mt_afe_set_2nd_i2s_in(MT_AFE_I2S_WLEN_32BITS,
				MT_AFE_I2S_SRC_SLAVE_MODE,
				MT_AFE_BCK_INV_INVESE_BCK,
				MT_AFE_NORMAL_CLOCK);
		mt_afe_enable_2nd_i2s_in();
		mt_afe_set_hdmi_tdm_i2s_loopback_data(ucontrol->value.integer.value[0] -
					       HDMI_LOOPBACK_SDATA0_TO_DL1);
		mt_afe_enable_hdmi_tdm_i2s_loopback();
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(priv->cached_sample_rate);
			mt_afe_enable_i2s_dac();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		}
		break;
	default:
		break;
	}

	priv->hdmi_loop_type = ucontrol->value.integer.value[0];
	return 0;
}

static int hdmi_sinegen_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);
	ucontrol->value.integer.value[0] = priv->hdmi_sinegen_switch;
	return 0;
}

static int hdmi_sinegen_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);
	uint32_t sample_rate = priv->cached_sample_rate;
	uint32_t channels = 2;

	if (priv->hdmi_sinegen_switch == ucontrol->value.integer.value[0]) {
		pr_notice("%s dummy operation for %u\n", __func__, priv->hdmi_sinegen_switch);
		return 0;
	}

	if (ucontrol->value.integer.value[0]) {
		uint32_t mclk_div;
		priv->hdmi_dma_buf = kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
		priv->hdmi_dma_buf->area = dma_alloc_coherent(platform->dev, HDMI_MAX_BUFFER_SIZE,
							      &(priv->hdmi_dma_buf->addr),
							      GFP_KERNEL);
		if (priv->hdmi_dma_buf->area) {
			size_t i;
			priv->hdmi_dma_buf->bytes = HDMI_MAX_BUFFER_SIZE;
			for (i = 0; i < priv->hdmi_dma_buf->bytes;
			     i += sizeof(table_sgen_golden_values))
				memcpy((void *)(priv->hdmi_dma_buf->area + i),
				       (void *)table_sgen_golden_values,
				       sizeof(table_sgen_golden_values));
		} else {
			pr_warn("%s dma_alloc_coherent fail\n", __func__);
			kfree(priv->hdmi_dma_buf);
			return 0;
		}

		mt_afe_main_clk_on();

		mt_afe_enable_apll(sample_rate);
		mt_afe_enable_apll_tuner(sample_rate);

		mclk_div = mt_afe_set_mclk(MT_AFE_I2S3, sample_rate);
		mt_afe_set_i2s3_bclk(mclk_div, sample_rate, 2, 32);

		mt_afe_enable_apll_div_power(MT_AFE_I2S3, sample_rate);
		mt_afe_enable_apll_div_power(MT_AFE_I2S3_BCK, sample_rate);

		mt_afe_hdmi_clk_on();

		mt_afe_set_mclk(MT_AFE_SPDIF, sample_rate);
		mt_afe_enable_apll_div_power(MT_AFE_SPDIF, sample_rate);
		mt_afe_spdif_clk_on();

		mt_afe_set_reg(AFE_HDMI_OUT_BASE, priv->hdmi_dma_buf->addr, 0xffffffff);
		mt_afe_set_reg(AFE_HDMI_OUT_END,
			    priv->hdmi_dma_buf->addr + (priv->hdmi_dma_buf->bytes - 1), 0xffffffff);

		mt_afe_set_memif_fetch_format(MT_AFE_DIGITAL_BLOCK_HDMI, MT_AFE_MEMIF_16_BIT);

		mt_afe_set_hdmi_tdm1_config(channels, MT_AFE_I2S_WLEN_32BITS);
		mt_afe_set_hdmi_tdm2_config(channels);
		mt_afe_set_hdmi_out_channel(channels);

		mt_pcm_hdmi_set_interconnection(INTER_CONNECT, channels);

		mt_afe_enable_hdmi_out();

		mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_HDMI);
		mt_afe_enable_afe(true);

		mt_afe_enable_hdmi_tdm();
	} else {
		mt_pcm_hdmi_set_interconnection(INTER_DISCONNECT, channels);

		mt_afe_disable_hdmi_tdm();

		mt_afe_disable_hdmi_out();

		mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_HDMI);

		mt_afe_enable_afe(false);

		mt_afe_spdif_clk_off();
		mt_afe_disable_apll_div_power(MT_AFE_SPDIF, sample_rate);

		mt_afe_hdmi_clk_off();
		mt_afe_disable_apll_div_power(MT_AFE_I2S3_BCK, sample_rate);
		mt_afe_disable_apll_div_power(MT_AFE_I2S3, sample_rate);

		mt_afe_disable_apll_tuner(sample_rate);
		mt_afe_disable_apll(sample_rate);

		mt_afe_main_clk_off();

		if (priv->hdmi_dma_buf) {
			if (priv->hdmi_dma_buf->area) {
				dma_free_coherent(platform->dev, HDMI_MAX_BUFFER_SIZE,
						  priv->hdmi_dma_buf->area,
						  priv->hdmi_dma_buf->addr);
			}
			kfree(priv->hdmi_dma_buf);
			priv->hdmi_dma_buf = NULL;
		}
	}

	priv->hdmi_sinegen_switch = ucontrol->value.integer.value[0];
	return 0;
}

static int hdmi_fmt_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);
	ucontrol->value.integer.value[0] = priv->format_changed;
	return 0;
}

static int hdmi_fmt_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(platform);

	if (priv->format_changed == ucontrol->value.integer.value[0]) {
		pr_notice("%s dummy operation for %u\n", __func__, priv->format_changed);
		return 0;
	}
	priv->format_changed = ucontrol->value.integer.value[0];
	return 0;
}

static const char *const hdmi_loopback_function[] = {
	ENUM_TO_STR(HDMI_LOOPBACK_NONE),
	ENUM_TO_STR(HDMI_LOOPBACK_SDATA0_TO_DL1),
	ENUM_TO_STR(HDMI_LOOPBACK_SDATA1_TO_DL1),
	ENUM_TO_STR(HDMI_LOOPBACK_SDATA2_TO_DL1),
	ENUM_TO_STR(HDMI_LOOPBACK_SDATA3_TO_DL1),
};

static const char *const hdmi_sinegen_function[] = { "Off", "On" };
static const char *const hdmi_fmt_chg[] = { "Off", "On" }; /* format changed */

static const struct soc_enum mt_pcm_hdmi_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hdmi_loopback_function), hdmi_loopback_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hdmi_sinegen_function), hdmi_sinegen_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hdmi_fmt_chg), hdmi_fmt_chg),
};

static const struct snd_kcontrol_new mt_pcm_hdmi_controls[] = {
	SOC_ENUM_EXT("HDMI_Loopback_Select", mt_pcm_hdmi_control_enum[0], hdmi_loopback_get,
		     hdmi_loopback_set),
	SOC_ENUM_EXT("Audio_Hdmi_SideGen_Switch", mt_pcm_hdmi_control_enum[1], hdmi_sinegen_get,
		     hdmi_sinegen_set),
	SOC_ENUM_EXT("Audio_Hdmi_Format_Switch", mt_pcm_hdmi_control_enum[2], hdmi_fmt_get,
		     hdmi_fmt_set),
};

static void mt_pcm_hdmi_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int mt_pcm_hdmi_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;
	size = mt_pcm_hdmi_hardware.buffer_bytes_max;

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
		card->dev, size, size);
	if (!ret)
		pr_err("%s preallocate successful\n", __func__);
	else
		pr_err("%s preallocate failed\n", __func__);
	return ret;
}

static int mt_pcm_hdmi_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, mt_pcm_hdmi_controls,
				      ARRAY_SIZE(mt_pcm_hdmi_controls));
	return 0;
}

static struct snd_pcm_ops mt_pcm_hdmi_ops = {
	.open = mt_pcm_hdmi_open,
	.close = mt_pcm_hdmi_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt_pcm_hdmi_hw_params,
	.hw_free = mt_pcm_hdmi_hw_free,
	.prepare = mt_pcm_hdmi_prepare,
	.trigger = mt_pcm_hdmi_trigger,
	.pointer = mt_pcm_hdmi_pointer,
};

static struct snd_soc_platform_driver mt_pcm_hdmi_platform = {
	.ops = &mt_pcm_hdmi_ops,
	.pcm_new = mt_pcm_hdmi_new,
	.pcm_free = mt_pcm_hdmi_free,
	.probe = mt_pcm_hdmi_probe,
};

static int mt_pcm_hdmi_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_hdmi_priv *priv;
	pr_notice("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_HDMI_PLATFORM_NAME);
		pr_notice("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_hdmi_priv), GFP_KERNEL);
	if (!priv) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	priv->cached_sample_rate = 44100;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_hdmi_platform);
}

static int mt_pcm_hdmi_dev_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_hdmi_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_HDMI_PLATFORM_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_hdmi_dt_match);

static struct platform_driver mt_pcm_hdmi_driver = {
	.driver = {
		   .name = MT_SOC_HDMI_PLATFORM_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_hdmi_dt_match,
		   },
	.probe = mt_pcm_hdmi_dev_probe,
	.remove = mt_pcm_hdmi_dev_remove,
};

module_platform_driver(mt_pcm_hdmi_driver);

MODULE_DESCRIPTION("AFE PCM HDMI platform driver");
MODULE_LICENSE("GPL");
