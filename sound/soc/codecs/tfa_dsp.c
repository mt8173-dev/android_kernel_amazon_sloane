/*
 * Copyright (C) NXP Semiconductors (PLMA)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define pr_fmt(fmt) "%s(%s): " fmt, __func__, tfa98xx->fw.name
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/crc32.h>

#include "tfa98xx-core.h"
#include "tfa98xx-regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"



/* size of the data buffer used for I2C transfer */
#define TFA98XX_MAX_I2C_SIZE	252


#define TFA98XX_XMEM_CALIBRATION_DONE 	231 // 0xe7
#define TFA98XX_XMEM_IMPEDANCE		232
#define TFA98XX_XMEM_COUNT_BOOT		161 // 0xa1

/*
 * Maximum number of retries for DSP result
 * Keep this value low!
 * If certain calls require longer wait conditions, the
 * application should poll, not the API
 * The total wait time depends on device settings. Those
 * are application specific.
 */
#define TFA98XX_WAITRESULT_NTRIES	50
#define TFA98XX_WAITRESULT_NTRIES_LONG	2000


/* DSP module IDs */
#define MODULE_FRAMEWORK        0
#define MODULE_SPEAKERBOOST     1
#define MODULE_BIQUADFILTERBANK 2
#define MODULE_SETRE 		9


/* RPC commands IDs */
/* Load a full model into SpeakerBoost. */
#define SB_PARAM_SET_LSMODEL	0x06
#define SB_PARAM_SET_EQ		0x0A /* 2 Equaliser Filters */
#define SB_PARAM_SET_PRESET	0x0D /* Load a preset */
#define SB_PARAM_SET_CONFIG	0x0E /* Load a config */
#define SB_PARAM_SET_DRC	0x0F
#define SB_PARAM_SET_AGCINS	0x10


/* gets the speaker calibration impedance (@25 degrees celsius) */
#define SB_PARAM_GET_RE0		0x85
#define SB_PARAM_GET_LSMODEL		0x86 /* Gets LoudSpeaker Model */
#define SB_PARAM_GET_CONFIG_PRESET	0x80
#define SB_PARAM_GET_STATE		0xC0
#define SB_PARAM_GET_XMODEL		0xC1 /* Gets Excursion Model */

#define SPKRBST_TEMPERATURE_EXP		9

/* Framwork params */
#define FW_PARAM_GET_STATE		0x84
#define FW_PARAM_GET_FEATURE_BITS	0x85


/*
 * write a bit field
 */
int tfaRunWriteBitfield(struct tfa98xx *tfa98xx,  struct nxpTfaBitfield bf)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value, oldvalue, msk, tmp;
	union {
		u16 field;
		struct nxpTfaBfEnum Enum;
	} bfUni;

	value = bf.value;
	bfUni.field = bf.field;
	//bfUni.field  &= 0x7fff; //mask of high bit, done before

 	pr_debug("bitfield: %s=%d (0x%x[%d..%d]=0x%x)\n", tfaContBfName(bfUni.field), value, bfUni.Enum.address,
							  bfUni.Enum.pos, bfUni.Enum.pos+bfUni.Enum.len, value);
	if (((struct nxpTfaBfEnum*)&bf.field)->address & 0x80) {
		pr_err("WARNING:not a persistant write of MTP\n");
	}

	oldvalue = (u16)snd_soc_read(codec, bfUni.Enum.address);
	tmp = oldvalue;

	msk = ((1 << (bfUni.Enum.len + 1)) - 1) << bfUni.Enum.pos;
	oldvalue &= ~msk;
	oldvalue |= value << bfUni.Enum.pos;
	pr_debug("bitfield: %s=%d (0x%x -> 0x%x)\n", tfaContBfName(bfUni.field), value, tmp, oldvalue);
	snd_soc_write(codec, bfUni.Enum.address, oldvalue);

	return 0;
}

/*
 * write the register based on the input address, value and mask
 * only the part that is masked will be updated
 */
int tfaRunWriteRegister(struct tfa98xx *tfa98xx, struct nxpTfaRegpatch *reg)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value, newvalue;

	pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n", reg->address, reg->value, reg->mask);

	value = (u16)snd_soc_read(codec, reg->address);
	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;
	value |= newvalue;
	snd_soc_write(codec, reg->address, value);

	return 0;
}

/*
 * tfa98xx_dsp_system_stable will compensate for the wrong behavior of CLKS
 * to determine if the DSP subsystem is ready for patch and config loading.
 *
 * A MTP calibration register is checked for non-zero.
 *
 * Note: This only works after i2c reset as this will clear the MTP contents.
 * When we are configured then the DSP communication will synchronize access.
 */
int tfa98xx_dsp_system_stable(struct tfa98xx *tfa98xx, int *ready)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 status, mtp0, sysctrl;
	int tries;

	*ready = 0;

	/* check the contents of the STATUS register */
	status = (u16)snd_soc_read(codec, TFA98XX_STATUSREG);
	sysctrl = (u16)snd_soc_read(codec, TFA98XX_SYS_CTRL);

	pr_debug("statusreg = 0x%04x, sysctrl=0x%04x\n", status, sysctrl);

	/*
	 * if AMPS is set then we were already configured and running
	 * no need to check further
	 */
	*ready = (status & TFA98XX_STATUSREG_AMPS_MSK) == (TFA98XX_STATUSREG_AMPS_MSK);

	pr_debug("AMPS %d\n", *ready);
	if (*ready)
		return 0;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = (status & (TFA98XX_STATUSREG_AREFS_MSK | TFA98XX_STATUSREG_CLKS_MSK))
		  == (TFA98XX_STATUSREG_AREFS_MSK | TFA98XX_STATUSREG_CLKS_MSK);
	pr_debug("AREFS | CLKS %d\n", *ready);
	if (!*ready)		/* if not ready go back */
		return 0;

	if (tfa98xx->rev != REV_TFA9890) {
		*ready = 1;
		return 0;
	}

	/*
	 * check MTPB
	 *   mtpbusy will be active when the subsys copies MTP to I2C
	 *   2 times retry avoids catching this short mtpbusy active period
	 */
	for (tries = 2; tries > 0; tries--) {
		status = (u16)snd_soc_read(codec, TFA98XX_STATUSREG);
		/* check the contents of the STATUS register */
		*ready = (status & TFA98XX_STATUSREG_MTPB_MSK) == 0;
		if (*ready)	/* if ready go on */
			break;
	}
	pr_debug("MTPB %d\n", *ready);
	if (tries == 0) {		/* ready will be 0 if retries exausted */
		pr_debug("Not ready %d\n", !*ready);
		return 0;
	}

	/*
	 * check the contents of  MTP register for non-zero,
	 * this indicates that the subsys is ready
	 */
	mtp0 = (u16)snd_soc_read(codec, 0x84);

	*ready = (mtp0 != 0);	/* The MTP register written? */
	pr_debug("MTP0 %d\n", *ready);

	return ret;
}

/*
 * Disable clock gating
 */
static int tfa98xx_clockgating(struct tfa98xx *tfa98xx, int on)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("%d\n", on);

	/* The clockgating hack is used only for the tfa9890 */
	if (tfa98xx->rev != REV_TFA9890)
		return 0;

	/* for TFA9890 temporarily disable clock gating when dsp reset is used */
	value = snd_soc_read(codec, TFA98XX_CURRENTSENSE4);

	if (on)	/* clock gating on - clear the bit */
		value &= ~TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;
	else	/* clock gating off - set the bit */
		value |= TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;

	return snd_soc_write(codec, TFA98XX_CURRENTSENSE4, value);
}

/*
 * tfa98xx_dsp_reset will deal with clock gating control in order
 * to reset the DSP for warm state restart
 */
static int tfa98xx_dsp_reset(struct tfa98xx *tfa98xx, int state)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* for TFA9890 temporarily disable clock gating when dsp reset is used */
	tfa98xx_clockgating(tfa98xx, 0);

	value = snd_soc_read(codec,TFA98XX_CF_CONTROLS);

	/* set requested the DSP reset signal state */
	value = state ? (value | TFA98XX_CF_CONTROLS_RST_MSK) :
			(value & ~TFA98XX_CF_CONTROLS_RST_MSK);

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, value);

	/* clock gating restore */
	return tfa98xx_clockgating(tfa98xx, 1);
}

int tfa98xx_powerdown(struct tfa98xx *tfa98xx, int powerdown)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("%d\n", powerdown);

	/* read the SystemControl register, modify the bit and write again */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);

	switch (powerdown) {
	case 1:
		value |= TFA98XX_SYS_CTRL_PWDN_MSK;
		break;
	case 0:
		value &= ~(TFA98XX_SYS_CTRL_PWDN_MSK);
		break;
	default:
		return -EINVAL;
	}

	return snd_soc_write(codec, TFA98XX_SYS_CTRL, value);
}


static int tfa98xx_read_data(struct tfa98xx *tfa98xx, u8 address, int len, u8 *data)
{
//	pr_debug("@%02x, #%d\n", address, len);

	if (tfa98xx_i2c_read(tfa98xx->i2c, address, data, len)) {
		pr_err("Error during I2C read\n");
		return -EIO;
	}

	return 0;
}

unsigned int tfa98xx_dspmem_read(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	u8 buf[3];
	int len = 3;
	int val = -EIO;

	pr_debug("\n");

	if (tfa98xx_i2c_read(tfa98xx->i2c, TFA98XX_CF_MEM, buf, len) != 0)
		return val;

	if (len != 3)
		return val;

	val = buf[0] << 16 | buf[1] << 8 | buf[2];

	return val;
}


void tfa98xx_convert_data2bytes(int num_data, const int *data, u8 *bytes)
{
	int i, k, d;
	/*
	 * note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2&23
	 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		bytes[k] = (d >> 16) & 0xFF;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xFF;
		bytes[k + 2] = (d) & 0xFF;	/* LSB */
	}
}


/*
 * convert DSP memory bytes to signed 24 bit integers
 * data contains "len/3" elements
 * bytes contains "len" elements
 */
void tfa98xx_convert_bytes2data(int len, const u8 *bytes, int *data)
{
	int i, k, d;
	int num_data = len / 3;

	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}


int tfa98xx_dsp_read_mem(struct tfa98xx *tfa98xx, u16 start_offset, int num_words, int *values)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_ctrl;	/* to sent to the CF_CONTROLS register */
	u8 bytes[MAX_PARAM_SIZE];
	int burst_size;		/* number of words per burst size */
	int bytes_per_word = 3;
	int len;
	int *p;

//	pr_debug("@0x%04x, #%d\n", start_offset, num_words);

	/* first set DMEM and AIF, leaving other bits intact */
	cf_ctrl = snd_soc_read(codec, TFA98XX_CF_CONTROLS);
	cf_ctrl &= ~0x000E;	/* clear AIF & DMEM */
	/* set DMEM, leave AIF cleared for autoincrement */
	cf_ctrl |= (Tfa98xx_DMEM_XMEM << 1);

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);

	snd_soc_write(codec, TFA98XX_CF_MAD, start_offset);

	len = num_words * bytes_per_word;
	p = values;
	for (; len > 0;) {
		burst_size = ROUND_DOWN(16, bytes_per_word);
		if (len < burst_size)
			burst_size = len;

		ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, burst_size, bytes);
		if (ret)
			return ret;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);
//		pr_debug("0x%06x\n", *p);
		len -= burst_size;
		p += burst_size / bytes_per_word;
	}

	return 0;
}

/*
 * Write all the bytes specified by len and data
 */
static int tfa98xx_write_data(struct tfa98xx *tfa98xx, u8 subaddress, int len,
		       const u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u8 write_data[MAX_PARAM_SIZE];
	/* subaddress followed by data */
	int count = len + 1;

//	pr_debug("%d\n", len);

	if (count > MAX_PARAM_SIZE) {
		pr_err("Error param size too big %d\n", len);
		return -EINVAL;
	}

	write_data[0] = subaddress;
	memcpy(write_data + 1, data, len);

	return tfa98xx_bulk_write_raw(codec, write_data, count);
}


static int tfa98xx_dsp_write(struct tfa98xx *tfa98xx, unsigned int address, int value)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_ctrl; /* the value to sent to the CF_CONTROLS register */
	u8 bytes[3];
	int dmem = (address >> 16) & 0xf;

	pr_debug("@0x%04x=%d\n", address, value);

	/* first set DMEM and AIF, leaving other bits intact */
	cf_ctrl = snd_soc_read(codec, TFA98XX_CF_CONTROLS);
	cf_ctrl &= ~0x000E;     /* clear AIF & DMEM */
	cf_ctrl |= (dmem << 1); /* set DMEM, leave AIF cleared for autoincrement */

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);
	snd_soc_write(codec, TFA98XX_CF_MAD, address & 0xffff);

	tfa98xx_convert_data2bytes(1, &value, bytes);
	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_MEM, 3, bytes);
	if (ret)
		return ret;

	return 0;
}


int tfaRunResetCount(struct tfa98xx *tfa98xx)
{
	int count;

	tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_COUNT_BOOT, 1, &count);

	return count;
}

/*
 * wait for calibrate done
 */
int tfa98xxRunWaitCalibration(struct tfa98xx *tfa98xx, int *done)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	int tries = 0;
	u16 mtp;

	*done = 0;

	mtp = snd_soc_read(codec, TFA98XX_MTP);

	pr_debug("TFA98XX_MTP 0x%04x\n", mtp);

	/* in case of calibrate once wait for MTPEX */
	if (mtp & TFA98XX_MTP_MTPOTC) {
		pr_debug("calibrate once wait for MTPEX\n");
		while ((*done == 0) && (tries < TFA98XX_WAITRESULT_NTRIES))
		{
			msleep_interruptible(5);
			mtp = snd_soc_read(codec, TFA98XX_MTP);
			*done = (mtp & TFA98XX_MTP_MTPEX);	/* check MTP bit1 (MTPEX) */
			tries++;
		}
	} else { /* poll xmem for calibrate always */
		pr_debug("poll xmem for calibrate always\n");
		while ((*done == 0) && (tries < TFA98XX_WAITRESULT_NTRIES))
		{
			msleep_interruptible(5);
			ret = tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_CALIBRATION_DONE, 1, done);
			tries++;
		}
	}

	if (tries == TFA98XX_WAITRESULT_NTRIES) {
		pr_err("Calibrate Done timedout\n");
		return -ETIMEDOUT;
	}

	return ret;
}

static int tfa9887_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	/* DSP must be in control of the amplifier to avoid plops */
	value |= TFA98XX_SYS_CTRL_AMPE_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	/* some other registers must be set for optimal amplifier behaviour */
	snd_soc_write(codec, TFA98XX_BAT_PROT, 0x13AB);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, 0x001F);
	/* peak voltage protection is always on, but may be written */
	snd_soc_write(codec, 0x08, 0x3C4E);
	/* TFA98XX_SYSCTRL_DCA = 0 */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, 0x024D);
	snd_soc_write(codec, 0x0A, 0x3EC3);
	snd_soc_write(codec, 0x41, 0x0308);
	snd_soc_write(codec, 0x49, 0x0E82);

	return 0;
}

static int tfa9895_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* all i2C registers are already set to default */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	/* DSP must be in control of the amplifier to avoid plops */
	value |= TFA98XX_SYS_CTRL_AMPE_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	/* some other registers must be set for optimal amplifier behaviour */
	snd_soc_write(codec, TFA98XX_BAT_PROT, 0x13AB);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, 0x001F);
	/* peak voltage protection is always on, but may be written */
	snd_soc_write(codec, 0x08, 0x3C4E);
	/* TFA98XX_SYSCTRL_DCA = 0 */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, 0x024D);
	snd_soc_write(codec, 0x41, 0x0308);
	snd_soc_write(codec, 0x49, 0x0E82);

	return 0;
}

static int tfa9897_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;

	pr_debug("\n");

	/* all i2C registers must already set to default POR value */
	snd_soc_write(codec, 0x07, 0x1856); // adaptive mode off, peak to average=1, current=1.750 A
	snd_soc_write(codec, 0x49, 0xadb0); // 8 ohm bit 5
	snd_soc_write(codec, 0x48, 0x0308); // $48:[3] - 0 ==> 1; iddqtestbst - default value changed
	snd_soc_write(codec, 0x4b, 0x081e); // invert current
	snd_soc_write(codec, 0x0c, 0x0007); // PWM from analog clipper
	snd_soc_write(codec, 0x10, 0x0228); // TDM enable
	snd_soc_write(codec, 0x10, 0x0228); // TDM enable, TDMPRF = 0, TDMFSLN = 1 slot
	snd_soc_write(codec, 0x14, 0x0000); // BCK/FS = 32 to be compatible with default 9890

	return 0;
}

static int tfa9890_specific(struct tfa98xx *tfa98xx)
{
	pr_debug("\n");
	return 0;
}

/*
 * clockless way to determine if this is the tfa9887 or tfa9895
 * by testing if the PVP bit is writable
 */
static int tfa98xx_is87(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 save_value, check_value;

	pr_debug("\n");

	save_value = snd_soc_read(codec, 0x08);

	/* if clear it's 87 */
	if ((save_value & 0x0400) == 0)
		return 1;

	/* try to clear pvp bit */
	snd_soc_write(codec, 0x08, (save_value & ~0x0400));
	check_value = snd_soc_read(codec, 0x08);

	/* restore */
	snd_soc_write(codec, 0x08, save_value);
	/* could we write the bit */

	/* if changed it's the 87 */
	return (check_value != save_value) ? 1 : 0;
}

/*
 * I2C register init should be done at probe/recover time (TBC)
 */
static int tfa98xx_init(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 reg;
	int ret;

	pr_debug("Reset all i2c registers\n");

	/* reset all i2c registers to default */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, TFA98XX_SYS_CTRL_I2CR_MSK);

	switch (tfa98xx->rev) {
	case 0x12:
		if (tfa98xx_is87(tfa98xx))
			ret = tfa9887_specific(tfa98xx);
		else
			ret = tfa9895_specific(tfa98xx);
		break;
	case 0x80:
		ret = tfa9890_specific(tfa98xx);
		/*
		 * Some PLL registers must be set optimal for
		 * amplifier behaviour. This is implemented in a file specific
		 * for the type number.
		 */
		snd_soc_write(codec, 0x40, 0x5a6b);
		reg = snd_soc_read(codec, 0x59);
		reg |= 0x3;
		reg = snd_soc_write(codec, 0x59, reg);
		snd_soc_write(codec, 0x40, 0x0000);
		break;
	case 0x97:
		ret = tfa9897_specific(tfa98xx);
		break;
	case 0x81:
		/* for the RAM version disable clock-gating */
		ret = tfa9890_specific(tfa98xx);
		tfa98xx_clockgating(tfa98xx, 0);
		break;
	default:
		return 0;
	}

	return ret;
}


/*
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 */
static int tfa98xx_startup(struct tfa98xx *tfa98xx)
{
	int tries, status, ret;

	pr_debug("\n");

	/* load the optimal TFA98XX in HW settings */
	ret = tfa98xx_init(tfa98xx);

	/*
	 * I2S settings to define the audio input properties
	 * these must be set before the subsys is up
	 * this will run the list until a non-register item is encountered
	 */
	ret = tfaContWriteRegsDev(tfa98xx); // write device register settings

	/*
	 * also write register the settings from the default profile
	 * NOTE we may still have ACS=1 so we can switch sample rate here
	 * ret = tfaContWriteRegsProf(tfa98xx, tfa98xx->profile);
	 * power on the sub system
	 */
	ret = tfa98xx_powerdown(tfa98xx, 0);

	/*  powered on
	 *    - now it is allowed to access DSP specifics
	 */

	/*
	 * wait until the DSP subsystem hardware is ready
	 *    note that the DSP CPU is not running (RST=1)
	 */
	pr_debug("Waiting for DSP system stable...()\n");
	for (tries = 1; tries < CFSTABLE_TRIES; tries++) {
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (status)
			break;
	}

	if (tries == CFSTABLE_TRIES) {
		pr_err("Time out\n");
		return -ETIMEDOUT;
	}  else {
		pr_debug("OK (tries=%d)\n", tries);
	}

	/* the CF subsystem is enabled */
	pr_debug("reset count:0x%x\n", tfaRunResetCount(tfa98xx));

	return 0;
}

int tfaRunIsCold(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check status ACS bit to set */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("ACS %d\n", (status & TFA98XX_STATUSREG_ACS) != 0);

	return (status & TFA98XX_STATUSREG_ACS) != 0;
}
/*
 * report if we are in powerdown state
 * use AREFS from the status register iso the actual PWDN bit
 * return true if powered down
 */
int tfaRunIsPwdn(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check if PWDN bit is clear by looking at AREFS */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("AREFS %d\n", (status & TFA98XX_STATUSREG_AREFS) != 0);

	return (status & TFA98XX_STATUSREG_AREFS) == 0;
}

int tfaRunIsAmpRunning(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check status SWS bit to set */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("SWS %d\n", (status & TFA98XX_STATUSREG_SWS_MSK) != 0);

	return (status & TFA98XX_STATUSREG_SWS_MSK) != 0;
}

#define CF_CONTROL 0x8100

int tfaRunColdboot(struct tfa98xx *tfa98xx, int state)
{
	int ret = 0;
	int tries = 10;

	pr_debug("\n");

//	tfa98xx_dsp_reset(tfa98xx, 1);

	/* repeat set ACS bit until set as requested */
	while (state == !tfaRunIsCold(tfa98xx)) {
		/* set coldstarted in CF_CONTROL to force ACS */
		ret = tfa98xx_dsp_write(tfa98xx, (Tfa98xx_DMEM_IOMEM << 16) | CF_CONTROL, 1);

		if (tries-- == 0) {
			pr_debug("coldboot (ACS) did not %s\n", state ? "set":"clear");
			return -EINVAL;
		}
	}

	return ret;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
int tfaRunCfPowerup(struct tfa98xx *tfa98xx)
{
	int ret = 0;
	int tries, status;

	pr_debug("\n");

	/* power on the sub system */
	ret = tfa98xx_powerdown(tfa98xx, 0);

	pr_debug("Waiting for DSP system stable...\n");

	// wait until everything is stable, in case clock has been off
	for (tries = CFSTABLE_TRIES; tries > 0; tries--) {
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (status)
			break;
	}

	if (tries==0) {
		// timedout
		pr_err("DSP subsystem start timed out\n");
		return -ETIMEDOUT;
	}

	return ret;
}

/*
 * the patch contains a header with the following
 * IC revision register: 1 byte, 0xFF means don't care
 * XMEM address to check: 2 bytes, big endian, 0xFFFF means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
int tfa98xx_check_ic_rom_version(struct tfa98xx *tfa98xx, const u8 patchheader[])
{
	int ret = 0;
	u16 checkrev;
	u16 checkaddress;
	int checkvalue;
	int value = 0;
	int status;

	pr_debug("FW rev: %x, IC rev %x\n", patchheader[0], tfa98xx->rev);

	checkrev = patchheader[0];
	if ((checkrev != 0xff) && (checkrev != tfa98xx->rev))
		return -EINVAL;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue = (patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];

	if (checkaddress != 0xffff) {
		pr_debug("checkvalue: 0x%04x, checkvalue 0x%08x\n", checkvalue, checkvalue);
		/* before reading XMEM, check if we can access the DSP */
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (!ret) {
			if (!status) {
				/* DSP subsys not running */
				ret = -EBUSY;
			}
		}

		/* read register to check the correct ROM version */
		if (!ret) {
			ret =	tfa98xx_dsp_read_mem(tfa98xx, checkaddress, 1, &value);
			pr_debug("checkvalue: 0x%08x, DSP 0x%08x\n", checkvalue, value);
		}

		if (!ret) {
			if (value != checkvalue)
				ret = -EINVAL;
		}
	}

	return ret;
}


int tfa98xx_process_patch_file(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 size;
	int index;
	int ret = 0;
	u8 chunk_buf[TFA98XX_MAX_I2C_SIZE + 1];

	pr_debug("len %d\n", len);

	/*
	 * expect following format in patchBytes:
	 * 2 bytes len of I2C transaction in little endian, then the bytes,
	 * excluding the slave address which is added from the handle
	 * This repeats for the whole file
	 */
	index = 0;
	while (index < len) {
		/* extract little endian length */
		size = data[index] + data[index + 1] * 256;
		if (size > TFA98XX_MAX_I2C_SIZE) {
			pr_err("Patch chunk size %d > %d\n", size, TFA98XX_MAX_I2C_SIZE);
		}

		index += 2;

		if ((index + size) > len) {
			/* outside the buffer, error in the input data */
			return -EINVAL;
		}

		/*
		 * Need to copy data from the fw into local memory to avoid
		 * trouble with some i2c controller
		 */
		memcpy(chunk_buf, data + index, size);

		ret = tfa98xx_bulk_write_raw(codec, chunk_buf, size);
		if (ret) {
			pr_err("writing dsp patch failed %d\n", ret);
			break;
		}

		index += size;
	}

	return ret;
}

#define PATCH_HEADER_LENGTH 6
int tfa98xx_dsp_patch(struct tfa98xx *tfa98xx, int patchLength, const u8 *patchBytes)
{
	int ret = 0;

	pr_debug("\n");

	if (patchLength < PATCH_HEADER_LENGTH)
		return -EINVAL;

	ret = tfa98xx_check_ic_rom_version(tfa98xx, patchBytes);
	if (ret) {
		pr_err("ERROR: %d\n", ret);
		return ret;

	}

	ret = tfa98xx_process_patch_file(tfa98xx, patchLength - PATCH_HEADER_LENGTH, patchBytes + PATCH_HEADER_LENGTH);
	return ret;
}


int tfa98xx_set_configured(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* read the SystemControl register, modify the bit and write again */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	value |= TFA98XX_SYS_CTRL_SBSL_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	return 0;
}

#define TO_LONG_LONG(x)	((s64)(x)<<32)
#define TO_INT(x)	((x)>>32)
#define TO_FIXED(e) 	e

int float_to_int(u32 x)
{
    unsigned e = (0x7F + 31) - ((* (unsigned*) &x & 0x7F800000) >> 23);
    unsigned m = 0x80000000 | (* (unsigned*) &x << 8);
    return (int)((m >> e) & -(e < 32));
}

int tfa98xx_set_volume(struct tfa98xx *tfa98xx, u32 voldB)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;
	int volume_value;

	value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);

	/*
	 * 0x00 ->  0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xFE -> -127dB
	 * 0xFF -> muted
	 */
	volume_value = 2 * float_to_int(voldB);
	if (volume_value > 255)
		volume_value = 255;

	pr_debug("%d, attenuation -%d dB\n", volume_value, float_to_int(voldB));

	/* volume value is in the top 8 bits of the register */
	value = (value & 0x00FF) | (u16)(volume_value << 8);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, value);

	return 0;
}


int tfa98xx_get_volume(struct tfa98xx *tfa98xx, s64 *pVoldB)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 value;

	pr_debug("\n");

	value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);
	value >>= 8;
	*pVoldB = TO_FIXED(value) / -2;

	return ret;
}


int tfa98xx_set_mute(struct tfa98xx *tfa98xx, enum Tfa98xx_Mute mute)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 audioctrl_value;
	u16 sysctrl_value;

	pr_debug("\n");

	audioctrl_value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);
	sysctrl_value = snd_soc_read(codec, TFA98XX_SYS_CTRL);

	switch (mute) {
	case Tfa98xx_Mute_Off:
		/*
		 * previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		audioctrl_value &= ~(TFA98XX_AUDIO_CTR_CFSM_MSK);
		sysctrl_value |= TFA98XX_SYS_CTRL_AMPE_MSK;
		break;
	case Tfa98xx_Mute_Digital:
		/* expect the amplifier to run */
		/* set the cf_mute bit */
		audioctrl_value |= TFA98XX_AUDIO_CTR_CFSM_MSK;
		/* set the enbl_amplifier bit */
		sysctrl_value |= (TFA98XX_SYS_CTRL_AMPE_MSK);
		break;
	case Tfa98xx_Mute_Amplifier:
		/* clear the cf_mute bit */
		audioctrl_value &= ~TFA98XX_AUDIO_CTR_CFSM_MSK;
		/* clear the enbl_amplifier bit and active mode */
		sysctrl_value &= ~TFA98XX_SYS_CTRL_AMPE_MSK;
		break;
	default:
		return -EINVAL;
	}

	ret = snd_soc_write(codec, TFA98XX_AUDIO_CTR, audioctrl_value);
	if (ret)
		return ret;

	ret = snd_soc_write(codec, TFA98XX_SYS_CTRL, sysctrl_value);
	return ret;
}


int tfaRunMute(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 status;
	int tries = 0;

	pr_debug("\n");

	/* signal the TFA98XX to mute plop free and turn off the amplifier */
	ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Amplifier);
	if (ret)
		return ret;

	/* now wait for the amplifier to turn off */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);
	while (((status & TFA98XX_STATUSREG_SWS) == TFA98XX_STATUSREG_SWS) && (tries < TFA98XX_WAITRESULT_NTRIES))
	{
		usleep_range(10000, 10000);
		status = snd_soc_read(codec, TFA98XX_STATUSREG);
		tries++;
	}

	/* The amplifier is always switching */
	if (tries == TFA98XX_WAITRESULT_NTRIES)
		return -ETIMEDOUT;

	pr_debug("-------------------- muted --------------------\n");

	return 0;
}


int tfaRunUnmute(struct tfa98xx *tfa98xx)
{
	int ret = 0;

	/* signal the TFA98XX to mute  */
	ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Off);

	pr_debug("-------------------unmuted ------------------\n");

	return ret;
}


/* check that num_byte matches the memory type selected */
int tfa98xx_check_size(enum Tfa98xx_DMEM which_mem, int len)
{
	int ret = 0;
	int modulo_size = 1;

	switch (which_mem) {
	case Tfa98xx_DMEM_PMEM:
		/* 32 bit PMEM */
		modulo_size = 4;
		break;
	case Tfa98xx_DMEM_XMEM:
	case Tfa98xx_DMEM_YMEM:
	case Tfa98xx_DMEM_IOMEM:
		/* 24 bit MEM */
		modulo_size = 3;
		break;
	default:
		return -EINVAL;
	}

	if ((len % modulo_size) != 0)
		return -EINVAL;

	return ret;
}


int tfa98xx_execute_param(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;

	/* the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	u16 cf_ctrl = 0x0002;
	cf_ctrl |= (1 << 8) | (1 << 4);	/* set the cf_req1 and cf_int bit */
	return snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);
}

int tfa98xx_wait_result(struct tfa98xx *tfa98xx, int waitRetryCount)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;

	/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	do {
		cf_status = snd_soc_read(codec, TFA98XX_CF_STATUS);
		tries++;
	} while ((!ret) && ((cf_status & 0x0100) == 0)
			  && (tries < waitRetryCount));
//	pr_debug("tries: %d\n", tries);
	if (tries >= waitRetryCount) {
		/* something wrong with communication with DSP */
		pr_err("Error DSP not running\n");
		return -EINVAL;
	}

	return 0;
}


/* read the return code for the RPC call */
int tfa98xx_check_rpc_status(struct tfa98xx *tfa98xx, int *status)
{
	int ret = 0;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	u16 cf_mad = 0x0000;
	u8 mem[3];	/* for the status read from DSP memory */
	u8 buffer[4];

	/* minimize the number of I2C transactions by making use
	 * of the autoincrement in I2C */
	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/* write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS, sizeof(buffer), buffer);
	if (ret)
		return ret;

	/* read 1 word (24 bit) from XMEM */
	ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, 3 /* sizeof(mem) */, mem);
	if (ret)
		return ret;

	*status = mem[0] << 16 | mem[1] << 8 | mem[2];

	return 0;
}

int tfa98xx_write_parameter(struct tfa98xx *tfa98xx,
			    u8 module_id,
			    u8 param_id,
			    int len, const u8 data[])
{
	int  ret;
	/*
	 * the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0
	 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters)*/
	u16 cf_mad = 0x0001;
	u8 buffer[7];
	int offset = 0;
	int chunk_size = ROUND_DOWN(TFA98XX_MAX_I2C_SIZE, 3);  /* XMEM word size */
	int remaining_bytes = len;

//	pr_debug("%d\n", len);

	ret = tfa98xx_check_size(Tfa98xx_DMEM_XMEM, len);
	if (!ret) {
		if ((len <= 0) || (len > MAX_PARAM_SIZE)) {
			pr_err("Error in parameters size\n");
			return -EINVAL;
		}
	}

	/*
	 * minimize the number of I2C transactions by making use of
	 * the autoincrement in I2C
	 */

	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/*
	 * write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS
	 */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);
	/*
	 * write the module and RPC id into CF_MEM, which
	 * follows CF_MAD
	 */
	buffer[4] = 0;
	buffer[5] = module_id + 128;
	buffer[6] = param_id;

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS, sizeof(buffer), buffer);
	if (ret)
		return ret;

	/*
	 * Thanks to autoincrement in cf_ctrl, next write will
	 * happen at the next address
	 */
	while ((!ret) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;

		ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_MEM, chunk_size, data + offset);

		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	return ret;
}


/* Execute RPC protocol to write something to the DSP */
int tfa98xx_dsp_set_param_var_wait(struct tfa98xx *tfa98xx,
				   u8 module_id,
				   u8 param_id, int len,
				   const u8 data[], int waitRetryCount)
{
	int ret = 0;
	int status = 0;

//	pr_debug("\n");

	/* 1) write the id and data to the DSP XMEM */
	ret = tfa98xx_write_parameter(tfa98xx, module_id, param_id, len, data);
	if (ret)
		return ret;

	/* 2) wake up the DSP and let it process the data */
	ret = tfa98xx_execute_param(tfa98xx);
	if (ret)
		return ret;

	/* 3) wait for the ack */
	ret = tfa98xx_wait_result(tfa98xx, waitRetryCount);
	if (ret)
		return ret;

	/* 4) check the RPC return value */
	ret = tfa98xx_check_rpc_status(tfa98xx, &status);
	if (ret)
		return ret;

	if (status) {
		/* DSP RPC call returned an error */
		pr_err("DSP RPC error %d\n", status + ERROR_RPC_BASE);
		return -EIO;
	}

	return ret;
}

/* Execute RPC protocol to write something to the DSP */
int tfa98xx_dsp_set_param(struct tfa98xx *tfa98xx, u8 module_id,
			  u8 param_id, int len,
			  const u8 *data)
{
	/* Use small WaitResult retry count */
	return tfa98xx_dsp_set_param_var_wait(tfa98xx, module_id, param_id,
					      len, data,
					      TFA98XX_WAITRESULT_NTRIES);
}


int tfa98xx_dsp_write_config(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	int ret = 0;
	int has_drc = 0;

	pr_debug("\n");

	ret = tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_SET_CONFIG, len, data);
	if (ret)
		return ret;

	ret = tfa98xx_dsp_support_drc(tfa98xx, &has_drc);
	if (ret)
		return ret;

	if (has_drc) {
		/*
		 * Need to set AgcGainInsert back to PRE, as
		 * the SetConfig forces it to POST
		 */
		ret = tfa98xx_dsp_set_agc_gain_insert(tfa98xx, Tfa98xx_AgcGainInsert_PreDrc);
	}

	return ret;
}


/* the number of biquads supported */
#define TFA98XX_BIQUAD_NUM	10
#define BIQUAD_COEFF_SIZE	6

int tfa98xx_dsp_diquad_disable(struct tfa98xx *tfa98xx, int biquad_index)
{
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	u8 data[BIQUAD_COEFF_SIZE * 3];

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return -EINVAL;

	if (biquad_index < 1)
		return -EINVAL;

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int) - 8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;

	/*
	 * convert to fixed point and then bytes suitable for
	 * transmission over I2C
	 */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, data);
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_BIQUADFILTERBANK,
				     (u8)biquad_index,
				     (u8)(BIQUAD_COEFF_SIZE * 3),
				     data);
}

int tfa98xx_dsp_biquad_set_coeff(struct tfa98xx *tfa98xx, int biquad_index,
				 int len, u8* data)
{
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_BIQUADFILTERBANK,
				     biquad_index, len, data);
}

/*
 * The AgcGainInsert functions are static because they are not public:
 * only allowed mode is PRE.
 * The functions are nevertheless needed because the mode is forced to
 * POST by each SetLSmodel and each SetConfig => it should be reset to
 * PRE afterwards.
 */
int tfa98xx_dsp_set_agc_gain_insert(struct tfa98xx *tfa98xx,
				    enum Tfa98xx_AgcGainInsert
				    agcGainInsert)
{
	int ret = 0;
	unsigned char bytes[3];

	pr_debug("\n");

	tfa98xx_convert_data2bytes(1, (int *) &agcGainInsert, bytes);

	ret = tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_SET_AGCINS, 3, bytes);

	return ret;
}


int tfa98xx_dsp_write_speaker_parameters(struct tfa98xx *tfa98xx, int len,
					 const u8 *data)
{
	int ret = 0;
	int has_drc = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d\n", len);

	ret = tfa98xx_dsp_set_param_var_wait(tfa98xx, MODULE_SPEAKERBOOST,
					       SB_PARAM_SET_LSMODEL, len,
					       data,
					       TFA98XX_WAITRESULT_NTRIES_LONG);
	if (ret)
		return ret;

	ret = tfa98xx_dsp_support_drc(tfa98xx, &has_drc);
	if (ret)
		return ret;

	if (has_drc) {
		/*
		 * Need to set AgcGainInsert back to PRE, as
		 * the SetConfig forces it to POST
		 */
		ret = tfa98xx_dsp_set_agc_gain_insert(tfa98xx, Tfa98xx_AgcGainInsert_PreDrc);
	}

	return ret;
}


int tfa98xx_dsp_write_preset(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	if (!data)
		return -EINVAL;

	pr_debug("\n");

	return tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				     SB_PARAM_SET_PRESET, len, data);
}

/* load all the parameters for the DRC settings from a file */
int tfa98xx_dsp_write_drc(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	if (!data)
		return -EINVAL;

	pr_debug("\n");

	return tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				     SB_PARAM_SET_DRC, len, data);
}


/* Execute RPC protocol to read something from the DSP */
int tfa98xx_dsp_get_param(struct tfa98xx *tfa98xx, u8 module_id,
			  u8 param_id, int len, u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	/* the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters)*/
	u16 cf_mad = 0x0001;
	u16 cf_status;
	int status = 0;
	int offset = 0;
	int chunk_size = ROUND_DOWN(TFA98XX_MAX_I2C_SIZE, 3 /* XMEM word size */ );
	int remaining_bytes = len;
	int tries = 0;
	u8 buffer[7];

	pr_debug("\n");

	ret = tfa98xx_check_size(Tfa98xx_DMEM_XMEM, len);
	if (!ret) {
		if ((len <= 0) || (len > MAX_PARAM_SIZE))
			return -EINVAL;
	}

	/*
	 * minimize the number of I2C transactions by making use of
	 * the autoincrement in I2C
	 */

	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/* write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);
	/* write the module and RPC id into CF_MEM,
	 * which follows CF_MAD */
	buffer[4] = 0;
	buffer[5] = module_id + 128;
	buffer[6] = param_id;

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS,
				   sizeof(buffer), buffer);

	/* 2) wake up the DSP and let it process the data */
	/* set the cf_req1 and cf_int bit */
	cf_ctrl |= (1 << 8) | (1 << 4);
	ret = snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);

	/* 3) wait for the ack */
	do {
		cf_status = snd_soc_read(codec, TFA98XX_CF_STATUS);
		tries++;
	/* don't wait forever, DSP is pretty quick to respond (< 1ms)*/
	} while ((!ret)
		&& ((cf_status & 0x0100) == 0)
		&& (tries < TFA98XX_WAITRESULT_NTRIES));

	if (tries >= TFA98XX_WAITRESULT_NTRIES) {
		/* something wrong with communication with DSP */
		return -ETIMEDOUT;
	}

	/* 4) check the RPC return value */
	ret = tfa98xx_check_rpc_status(tfa98xx, &status);
	if (ret)
		return ret;

	if (status) {
		/* DSP RPC call returned an error */
		pr_err("DSP RPC error %d\n", status + ERROR_RPC_BASE);
		return -EIO;
	}

	/* 5) read the resulting data */
	/* memory address to be accessed (0: Status,
	 * 1: ID, 2: parameters) */
	cf_mad = 0x0002;
	snd_soc_write(codec, TFA98XX_CF_MAD, cf_mad);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((!ret) && (remaining_bytes > 0)) {
		if (remaining_bytes < TFA98XX_MAX_I2C_SIZE)
			chunk_size = remaining_bytes;

		/* else chunk_size remains at initialize value above */
		ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, chunk_size,
					  data + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	return ret;
}


int tfa98xx_dsp_get_sw_feature_bits(struct tfa98xx *tfa98xx, int features[2])
{
	int ret = 0;
	unsigned char bytes[3 * 2];

	pr_debug("\n");

	ret = tfa98xx_dsp_get_param(tfa98xx, MODULE_FRAMEWORK,
					FW_PARAM_GET_FEATURE_BITS, sizeof(bytes),
					bytes);
	/* old ROM code may respond with ERROR_RPC_PARAMID -> -EIO */
	if (ret)
		return ret;

	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, features);

	return ret;
}


int tfa98xx_dsp_support_drc(struct tfa98xx *tfa98xx, int *has_drc)
{
	int ret = 0;
	*has_drc = 0;

	pr_debug("\n");

	if (tfa98xx->has_drc) {
		*has_drc = tfa98xx->has_drc;
	} else {
		int features[2];

		ret = tfa98xx_dsp_get_sw_feature_bits(tfa98xx, features);
		if (!ret) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*has_drc = (features[0] & FEATURE1_DRC) == 0;
		} else if (ret == -EIO) {
			/* older ROM code, doesn't support it */
			*has_drc = 0;
			ret = 0;
		}
		/* else some other ret, return transparently */

		if (!ret) {
			tfa98xx->has_drc = *has_drc;
		}
	}
	return ret;
}

int tfa98xx_resolve_incident(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->rev == REV_TFA9897) {
		pr_warn("OCDS TFA9897 trigger\n");
		/* TFA9897 need to reset the DSP to take the newly set re0 */
		tfa98xx_dsp_reset(tfa98xx, 1);
		tfa98xx_dsp_reset(tfa98xx, 0);
	} else {
		/* TFA98xx need power cycle */
		tfa98xx_powerdown(tfa98xx, 1);
		tfa98xx_powerdown(tfa98xx, 0);
	}

	return 0;
}

int tfa98xx_dsp_get_calibration_impedance(struct tfa98xx *tfa98xx, u32 *re25)
{
	int ret = 0;
	u8 bytes[3];
	int data[1];
	int done, Tcal;

	pr_debug("\n");

	ret = tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_CALIBRATION_DONE, 1, &done);
	if (ret)
		return ret;

	if (!done) {
		pr_err("Calibration not done %d\n", done);
		return -EINVAL;
	}

	ret = tfa98xx_dsp_get_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_GET_RE0, 3, bytes);

	tfa98xx_convert_bytes2data(3, bytes, data);

	/* /2^23*2^(def.SPKRBST_TEMPERATURE_EXP) */
	*re25 = TO_FIXED(data[0]) / (1 << (23 - SPKRBST_TEMPERATURE_EXP));

	ret = tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_IMPEDANCE, 1, &Tcal);
	pr_debug("Tcal %d\n", Tcal);

	return ret;
}


/*
 * Run the startup/init sequence and set ACS bit
 */
int tfaRunColdStartup(struct tfa98xx *tfa98xx)
{
	int ret;

	pr_debug("\n");

	ret = tfa98xx_startup(tfa98xx);
	pr_debug("tfa98xx_startup %d\n", ret);
	if (ret)
		return ret;

	/* force cold boot */
	ret = tfaRunColdboot(tfa98xx, 1); // set ACS
	pr_debug("tfaRunColdboot %d\n", ret);
	if (ret)
		return ret;

	/* start */
	ret = tfaContWritePatch(tfa98xx);
	pr_debug("tfaContWritePatch %d\n", ret);

	return ret;
}


static int coldboot = 0;
module_param(coldboot, int, S_IRUGO | S_IWUSR);

/*
 * Start the maximus speakerboost algorithm this implies a full system
 * startup when the system was not already started.
 */
int tfaRunSpeakerBoost(struct tfa98xx *tfa98xx, int force)
{
	int ret = 0;
	u32 re25;

	pr_debug("force: %d\n", force);

	if (force) {
		ret = tfaRunColdStartup(tfa98xx);
		if (ret)
			return ret;
		/* DSP is running now */
	}

	if (force || tfaRunIsCold(tfa98xx)) {
		int done;

		pr_debug("coldstart%s\n", force ? " (forced)" : "");

		/* in case of force CF already runnning */
		if (!force) {
			ret = tfa98xx_startup(tfa98xx);
			if (ret) {
				pr_err("tfa98xx_startup %d\n", ret);
				return ret;
			}

			/* load patch and start the DSP */
			ret = tfaContWritePatch(tfa98xx);
			if (ret) {
				pr_err("tfaContWritePatch %d\n", ret);
				return ret;
			}
		}

		/*
		 * DSP is running now
		 *   NOTE that ACS may be active
		 *   no DSP reset/sample rate may be done until configured (SBSL)
		 */

		/* soft mute */
		ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Digital);
		if (ret) {
			pr_err("Tfa98xx_SetMute error: %d\n", ret);
			return ret;
		}

		/*
		 * For the first configuration the DSP expects at least
		 * the speaker, config and a preset.
		 * Therefore all files from the device list as well as the file
		 * from the default profile are loaded before SBSL is set.
		 *
		 * Note that the register settings were already done before loading the patch
		 *
		 * write all the files from the device list (typically spk and config)
		 */
		ret = tfaContWriteFiles(tfa98xx);
		if (ret) {
			pr_err("tfaContWriteFiles error: %d\n", ret);
			return ret;
		}

		/*
		 * write all the files from the profile list (typically preset)
		 * use volumestep 0
		 */
		tfaContWriteFilesProf(tfa98xx, 0, 0);

		/* tell DSP it's loaded SBSL = 1 */
		ret = tfa98xx_set_configured(tfa98xx);
		if (ret) {
			pr_err("tfa98xx_set_configured error: %d\n", ret);
			return ret;
		}

		/* await calibration, this should return ok */
		tfa98xxRunWaitCalibration(tfa98xx, &done);
		if (!done) {
			pr_err("Calibration not done!\n");
			return -ETIMEDOUT;
		} else {
			pr_info("Calibration done\n");
		}

	} else {
		/* already warm, so just pwr on */
		ret = tfaRunCfPowerup(tfa98xx);
	}

	tfa98xx_dsp_get_calibration_impedance(tfa98xx, &re25);
	pr_debug("imp: %d ohms\n", re25);

	tfaRunUnmute(tfa98xx);

	return ret;
}


int tfa98xx_start(struct tfa98xx *tfa98xx, int profile, int vstep)
{
	int forcecoldboot = coldboot;

	if (!tfa98xx->profile_count || !tfa98xx->profiles)
		return -EINVAL;


	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_RECOVER) {
		pr_warn("Restart for recovery\n");
		forcecoldboot = 1;
	}

	/*
	 * set the current profile and vsteps
	 * in case they get written during cold start
	 */
	tfa98xx->vstep = tfa98xx->profiles[profile].vstep;

	pr_debug("Starting device, vstep %d\n", tfa98xx->vstep);

	/* tfaRunSpeakerBoost implies un-mute */
	if (tfaRunSpeakerBoost(tfa98xx, forcecoldboot))
		return -EINVAL;

	/*
	 * check if the profile and steps are the one we want
	 *  ! only written when ACS==1
	 */
	if (profile != tfa98xx->profile_current)
		/* was it done already */
		tfaContWriteProfile(tfa98xx, profile, vstep);
	else
		/* only do the file = vstep */
		tfaContWriteFilesProf(tfa98xx, profile, vstep);

	return 0;
}


int tfa98xx_stop(struct tfa98xx *tfa98xx)
{
	int ret = 0;

	pr_debug("Stopping device [%s]\n", tfa98xx->fw.name);

	/* tfaRunSpeakerBoost implies unmute */
	/* mute + SWS wait */
	ret = tfaRunMute(tfa98xx);
	if (ret)
		return ret;

	/* powerdown CF */
	ret = tfa98xx_powerdown(tfa98xx, 1);
	if (ret)
		return ret;

	return ret;
}
