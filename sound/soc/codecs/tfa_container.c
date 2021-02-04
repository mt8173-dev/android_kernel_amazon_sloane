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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/crc32.h>

#include "tfa98xx-core.h"
#include "tfa98xx-regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"



#ifndef LOAD_TFA_FW_FROM_USER
/*  static char tfa98xx_cnt_file[] = { file};       */
#include "tfa98xx_cnt.h"
#endif
int tfa_get_profile_count(struct nxpTfaDevice *dev)
{
	int i, hit = 0;

	if (!dev)
		return 0;

	for (i = 0, hit = 0; i < dev->length; i++)
		if (dev->list[i].type == dscProfile)
			hit++;

	return hit;
}


struct nxpTfaVolumeStep2File *tfa_get_vsteps(struct tfa98xx *tfa98xx, struct nxpTfaProfile *prof)
{
	u8 *base = tfa98xx->fw.base;
	int i;

//	pr_debug("\n");

	for(i = 0; i < prof->length; i++) {
		if ( prof->list[i].type == dscFile ) {
			struct nxpTfaFileDsc *file = (struct nxpTfaFileDsc *)(prof->list[i].offset + base);
			struct nxpTfaHeader *hdr = (struct nxpTfaHeader *)file->data;
			if (hdr->id == volstepHdr) {
				struct nxpTfaVolumeStep2File *vp = (struct nxpTfaVolumeStep2File *)hdr;
				pr_debug("vsteps: %d\n", vp->vsteps);
				return vp;
			}
		}
	}

	return NULL;
}


int tfa_init_profile(struct tfa98xx *tfa98xx, struct nxpTfaProfile *prof, int hit)
{
	struct nxpTfaVolumeStep2File *vp;
	u8 *base = tfa98xx->fw.base;

	vp = tfa_get_vsteps(tfa98xx, prof);
	if (vp) {
		tfa98xx->profiles[hit].vp = vp;
		tfa98xx->profiles[hit].vsteps = vp->vsteps;
	}

	tfa98xx->profiles[hit].profile = prof;
	tfa98xx->profiles[hit].name = base + prof->name.offset;
	tfa98xx->profiles[hit].index = hit;

	pr_debug("Init profile %d: %s\n", hit, tfa98xx->profiles[hit].name);

	return 0;
}

static char *filename = "TFA9887.cnt";

module_param(filename, charp, 0);

int tfa98xx_cnt_loadfile(struct tfa98xx *tfa98xx, int index)
{
	u16 addr = tfa98xx->i2c->addr;
	struct firmware *container = NULL;
	struct firmware lcontainer;
	struct nxpTfaContainer *data;
	int i, j, hit, ret;
	u32 crc32;
	u8 *base;

	pr_debug("I2C device: 0x%x\n", addr);

	/* Load DSP config and firmware files */
 #ifdef LOAD_TFA_FW_FROM_USER
	ret = request_firmware(&container, filename, tfa98xx->codec->dev);
	if (ret) {
		pr_err("Failed to read %s", filename);
		return ret;
	}
#else
	container = &lcontainer;
	container->data = (u8 *)tfa98xx_cnt_file;
	container->size = sizeof(tfa98xx_cnt_file);
	ret = 0;
#endif

	pr_debug("loaded %s - size: %ld\n", filename, container ? container->size : 0);

	data = (struct nxpTfaContainer *)container->data;
	pr_debug("%.2s%.2s\n", data->version, data->subversion);
	pr_debug("%.8s\n", data->customer);
	pr_debug("%.8s\n", data->application);
	pr_debug("%.8s\n", data->type);
	pr_debug("%d ndev\n", data->ndev);
	pr_debug("%d nprof\n", data->nprof);

	if (*(u16*)data->id != params) {
		pr_err("Wrong container type %.2s", (char *)&data->id);
		return -EINVAL;
	}

	crc32 = ~crc32_le(~0, ((u8*)container->data)+14, container->size-14);

	if (crc32 != data->CRC) {
		pr_err("fw checksum test failed %x\n", crc32);
		return -ENOEXEC;
	}

	base = (u8 *) data;
	tfa98xx->fw.base = data;
	for (i = 0; i < data->ndev; i++) {
		struct nxpTfaDevice *dev;
		struct nxpTfaProfile *prof;
		struct nxpTfaFileDsc *dsc;

		base = (u8*)data + data->index[i].offset;
		dev = (struct nxpTfaDevice *)base;
/*  ++++++ Temp: the cnt file used is for I@C address 0x36/0x37, but 0x34/0x35 on Magna +++ */
		if(dev->dev==0x36){
			dev->dev = 0x34;
			pr_debug("CHANGE DEVICE ADDR FROM 0X36 TO 0X34");
			}

		if(dev->dev==0x37) {
			dev->dev = 0x35;
			pr_debug("CHANGE DEVICE ADDR FROM 0X37 TO 0X35");
		}
/* ------------------------------------------------------------------------*/
		/* Check if this is the proper device addr */
		if (dev->dev != addr)
			continue;

		pr_debug("dev[0x%02x]: %s\n", dev->dev, (u8 *)data + dev->name.offset);

		tfa98xx->fw.dev = (struct nxpTfaDevice *)base;
		tfa98xx->fw.name = (u8 *)data + dev->name.offset;
		tfa98xx->profile_count = tfa_get_profile_count(tfa98xx->fw.dev);

		tfa98xx->profiles = devm_kzalloc(tfa98xx->codec->dev, tfa98xx->profile_count * sizeof(struct tfaprofile), GFP_KERNEL);
		if (!tfa98xx->profiles)
			return -ENOMEM;

		for (j = 0, hit = 0; j < dev->length; j++) {
			switch (dev->list[j].type) {
			case  dscProfile:
				prof = (struct nxpTfaProfile *)((u8 *)data + dev->list[j].offset);
				pr_debug("[%d] profile: %s\n", j, (u8 *)data + prof->name.offset);
				tfa_init_profile(tfa98xx, prof, hit++);
				break;
			case  dscFile:
			case  dscPatch:
				dsc = (struct nxpTfaFileDsc *)((u8 *)data + dev->list[j].offset);
				pr_debug("[%d] file: %s\n", j, (u8 *)data + dsc->name.offset);
				break;
			default:
				//pr_debug("[%d] %d\n", j, dev->list[j].type);
				break;
			}
		}
	}

	if (!tfa98xx->profiles) {
		pr_err("[0x%02x] No profile data for the device\n", addr);
	}

	return 0;
}

int tfa_get_vstep_count(struct tfa98xx *tfa98xx, struct nxpTfaProfile *prof)
{
	u8 *base = tfa98xx->fw.base;
	int i;

	pr_debug("\n");

	for(i = 0; i < prof->length; i++) {
		if ( prof->list[i].type == dscFile ) {
			struct nxpTfaFileDsc *file = (struct nxpTfaFileDsc *)(prof->list[i].offset + base);
			struct nxpTfaHeader *hdr = (struct nxpTfaHeader *)file->data;
			if (hdr->id == volstepHdr) {
				struct nxpTfaVolumeStep2File *vp = (struct nxpTfaVolumeStep2File *)hdr;
				pr_debug("vsteps: %d\n", vp->vsteps);
				return vp->vsteps;
			}
		}
	}

	return 0;
}


struct nxpTfaProfile *tfaContProfile(struct tfa98xx *tfa98xx, int index)
{
	struct nxpTfaDevice 	*dev = tfa98xx->fw.dev;
	struct nxpTfaProfile	*prof;
	char			*base = tfa98xx->fw.base;
	int			j, hit;

	pr_debug("%d\n", index);

	for (j = 0, hit = 0; j < dev->length; j++) {
		if (dev->list[j].type == dscProfile) {
			prof = (struct nxpTfaProfile *)(base + dev->list[j].offset);
			if (hit++ == index) {
				pr_debug("select profile[%d]='%s'\n", index, base + prof->name.offset);
				return prof;
			}
		}
	}

	return NULL;
}



/*
 * return the bitfield
 */
struct nxpTfaBitfield tfaContDsc2Bf(struct nxpTfaDescPtr dsc)
{
	u32 *ptr = (u32 *) (&dsc);
	union {
		struct nxpTfaBitfield bf;
		u32 num;
	} num_bf;

	num_bf.num = *ptr & TFA_BITFIELDDSCMSK;
	return num_bf.bf;
}

TFA_NAMETABLE

char *tfaContBfName(u16 num)
{
	int n = 0;
	do {
		if (TfaBfNames[n].bfEnum == num)
			return TfaBfNames[n].bfName;
	}
	while (TfaBfNames[n++].bfEnum != 0xffff);

	return TfaBfNames[n-1].bfName;
}

/*
 * write reg and bitfield items in the profilelist the target
 */
int tfaContWriteRegsProf(struct tfa98xx *tfa98xx, int profile)
{
	struct nxpTfaProfile *prof = tfaContProfile(tfa98xx, profile);
	u8 *base = tfa98xx->fw.base;
	int i;
	int err = 0;

	pr_debug("profile: %d\n", profile);

	if (!prof) {
		return -EINVAL;
	}

	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < prof->length; i++) {
		if ( prof->list[i].type == dscPatch ||
		     prof->list[i].type ==dscFile ||
		     prof->list[i].type ==dscProfile )
			break;

		if (prof->list[i].type & dscBitfieldBase) {
			err = tfaRunWriteBitfield(tfa98xx, tfaContDsc2Bf(prof->list[i]));
		}

		if (!prof->list[i].type == dscRegister) {
			err = tfaRunWriteRegister(tfa98xx, (struct nxpTfaRegpatch *)(base + prof->list[i].offset));
		}

		if (err)
			break;
	}

	return err;
}

/*
 * write  reg  and bitfield items in the devicelist to the target
 */
int tfaContWriteRegsDev(struct tfa98xx *tfa98xx)
{
	struct nxpTfaDevice *dev = tfa98xx->fw.dev;
	u8 *base = tfa98xx->fw.base;
	int i;
	int err = 0;

	pr_debug("\n");

	if (!dev)
		return -EINVAL;

	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch ||
		    dev->list[i].type ==dscFile ||
		    dev->list[i].type ==dscProfile)
		    break;

		if  (dev->list[i].type & dscBitfieldBase) {
			err = tfaRunWriteBitfield(tfa98xx, tfaContDsc2Bf(dev->list[i]));
		}

		if  (dev->list[i].type == dscRegister) {
			pr_debug("reg %x\n", dev->list[i].type);
			err = tfaRunWriteRegister(tfa98xx, (struct nxpTfaRegpatch *)(dev->list[i].offset + base));
		}

		if (err)
			break;
	}

	return err;
}

/*
 * show the contents of the header
 */
void tfaContShowHeader(struct tfa98xx *tfa98xx, struct nxpTfaHeader *hdr)
{
	char id[2];

	pr_debug("File header\n");

	id[1] = hdr->id >> 8;
	id[0] = hdr->id & 0xff;
	pr_debug("  id:%.2s version:%.2s subversion:%.2s\n", id, hdr->version, hdr->subversion);
	pr_debug("  size:%d CRC:0x%08x \n", hdr->size, hdr->CRC);
	pr_debug("  customer:%.8s application:%.8s type:%.8s\n", hdr->customer, hdr->application, hdr->type);
}

/*
 * write  patchfile in the devicelist to the target
 */
int tfaContWritePatch(struct tfa98xx *tfa98xx)
{
	struct nxpTfaDevice *dev = tfa98xx->fw.dev;
	u8 *base = tfa98xx->fw.base;
	struct nxpTfaFileDsc *file;
	struct nxpTfaPatch *patchfile;
	int size;
	int i;

	pr_debug("\n");

	/* process the list until a patch  is encountered */
	for(i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch) {
			file = (struct nxpTfaFileDsc *)(dev->list[i].offset + base);
			patchfile =(struct nxpTfaPatch *)&file->data;
			tfaContShowHeader(tfa98xx, &patchfile->hdr);

			/* size is total length including header */
			size = patchfile->hdr.size - sizeof(struct nxpTfaPatch);
			return tfa98xx_dsp_patch(tfa98xx, size, (const u8*) patchfile->data);
		}
	}

	return -EINVAL;
}


int tfaContWriteFilterbank(struct tfa98xx *tfa98xx, struct nxpTfaFilter *filter)
{
	unsigned char biquad_index;
	int ret = 0;

	for (biquad_index = 0; biquad_index < 10; biquad_index++) {
		if (filter[biquad_index].enabled) {
			ret = tfa98xx_dsp_biquad_set_coeff(tfa98xx, biquad_index + 1, // start @1
						    sizeof(filter[biquad_index].biquad.bytes),
						    filter[biquad_index].biquad.bytes);
		} else {
			ret = tfa98xx_dsp_diquad_disable(tfa98xx, biquad_index+1);
		}

		if (ret) {
			pr_err("Error %d\n", ret);
			return ret;
		}
	}

	return 0;
}

int tfaContWriteEq(struct tfa98xx *tfa98xx, struct nxpTfaEqualizerFile *eqf)
{
	return tfaContWriteFilterbank(tfa98xx, eqf->filter);
}

/*
 * write a parameter file to de device
 */
int tfaContWriteVstep(struct tfa98xx *tfa98xx, struct nxpTfaVolumeStep2File *vp)
{
	int vstep;
	int err = 0;

	vstep = tfa98xx->vstep;

	pr_debug("vstep[%d/%d]\n", vstep, vp->vsteps - 1);

	if (tfa98xx->dsp_init != TFA98XX_DSP_INIT_DONE)
		return -EINVAL;

	if (vstep < vp->vsteps) {
		err = tfa98xx_dsp_write_preset(tfa98xx, sizeof(vp->vstep[0].preset), vp->vstep[vstep].preset);
		if (err)
			return err;

		err = tfaContWriteFilterbank(tfa98xx, vp->vstep[vstep].filter);
		if (err)
			return err;

		tfa98xx_set_volume(tfa98xx, vp->vstep[vstep].attenuation);
	} else {
		pr_err("vstep[%d] > %d\n", tfa98xx->vstep , vp->vsteps - 1);
		return -EINVAL;
	}

	return err;
}

/*
 * write a parameter file to the device
 */
int tfaContWriteFile(struct tfa98xx *tfa98xx, struct nxpTfaFileDsc *file)
{
	int size;
	struct nxpTfaHeader *hdr = (struct nxpTfaHeader *)file->data;
	enum tfa_cnt_header_type type;
	int err = 0;

	type = hdr->id;

	switch (type) {
	case volstepHdr:
		pr_debug("volstep\n");
		err = tfaContWriteVstep(tfa98xx, (struct nxpTfaVolumeStep2File *)hdr);
		break;
	case speakerHdr:
		pr_debug("speaker\n");
		size = hdr->size - sizeof(struct nxpTfaSpeakerFile);
		err = tfa98xx_dsp_write_speaker_parameters(tfa98xx, size, (const u8 *)((struct nxpTfaSpeakerFile *)hdr)->data);
		break;
	case presetHdr:
		pr_debug("preset\n");
		size = hdr->size - sizeof(struct nxpTfaPresetFile);
		err = tfa98xx_dsp_write_preset(tfa98xx, size, (const u8 *)((struct nxpTfaPresetFile *)hdr)->data);
		break;
	case configHdr:
		pr_debug("config\n");
		size = hdr->size - sizeof(struct nxpTfaConfigFile);
		err = tfa98xx_dsp_write_config(tfa98xx, size, (const u8 *)((struct nxpTfaConfigFile *)hdr)->data);
		break;
	case equalizerHdr:
		pr_debug("equalizer\n");
		tfaContWriteEq(tfa98xx, (struct nxpTfaEqualizerFile *) hdr);
		break;
	case patchHdr:
		pr_debug("patch\n");
		size = hdr->size - sizeof(struct nxpTfaPatch );
		err = tfa98xx_dsp_patch(tfa98xx,  size, (const u8 *)((struct nxpTfaPatch *)hdr)->data);
		break;
	case drcHdr:
		/*
		 * The DRC file is split as:
		 * 36 bytes for generic header (customer, application, and type)
		 * 127x3 (381) bytes first block contains the device and sample rate
		 * 				independent settings
		 * 127x3 (381) bytes block the device and sample rate specific values.
		 * The second block can always be recalculated from the first block,
		 * if vlsCal and the sample rate are known.
		 */
		size = sizeof(struct drcParamBlock);
		/* fixed size for first block */
		err = tfa98xx_dsp_write_drc(tfa98xx, size, (const u8 *)((struct nxpTfaDrcFile *)hdr)->data);
		break;
	default:
		pr_err("Header is of unknown type: 0x%x\n", type);
		return -EINVAL;
	}

	return err;
}


int tfaContWriteItem(struct tfa98xx *tfa98xx, struct nxpTfaDescPtr * dsc)
{
	struct nxpTfaFileDsc *file;
	struct nxpTfaRegpatch *reg;
	u8 *base = tfa98xx->fw.base;

	pr_debug("\n");

	switch (dsc->type) {
	case dscDevice:
	case dscProfile:
		/* ignore device and profile list */
		break;
	case dscRegister:
		reg = (struct nxpTfaRegpatch *)(dsc->offset + base);
		pr_debug("$0x%2x=0x%02x,0x%02x\n", reg->address, reg->mask, reg->value);
		return tfaRunWriteRegister(tfa98xx, reg);
	case dscString:
		/* zero terminated string */
		pr_debug(";string: %s\n", dsc->offset + base);
		break;
	case dscFile:
	case dscPatch:
		/* filename + file contents */
		file = (struct nxpTfaFileDsc *)(dsc->offset + base);
		pr_debug("file: %s\n", file->name.offset + base);
		break;
	default:
		if (dsc->type & dscBitfieldBase)
			return tfaRunWriteBitfield(tfa98xx , tfaContDsc2Bf(*dsc));
	}

	return 0;
}

/*
 * process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 */
int tfaContWriteProfile(struct tfa98xx *tfa98xx, int profile, int vstep)
{
	struct nxpTfaProfile *prof = tfaContProfile(tfa98xx, profile);
	struct nxpTfaFileDsc *file;
	u8 *base = tfa98xx->fw.base;
	int i, pwdn=-1;

	pr_debug("\n");

	if (!prof) {
		return -EINVAL;
	}

	if (tfa98xx->profile_current != profile ) {
		/* profile switch so mute first */
		tfaRunMute(tfa98xx);
	}

	tfa98xx->profile_current = profile;
	tfa98xx->vstep = tfa98xx->profiles[profile].vstep;

	/*
	 * process the list and write all registers,
	 * if a file is encountered then see if we need to poweron first
	 */
	for(i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscFile) {
			if (pwdn < 0)
				pwdn = tfaRunIsPwdn(tfa98xx) ;

			if (pwdn) {
				tfaRunCfPowerup(tfa98xx);
				pwdn=0;
			}

			file = (struct nxpTfaFileDsc *)(prof->list[i].offset + base);
			if (tfaContWriteFile(tfa98xx, file) ){
				return -EINVAL;
			}
		} else {
			/* process and write all non-file items */
			if (tfaContWriteItem(tfa98xx, &prof->list[i]))
				return -EINVAL;
		}
	}

	tfaRunUnmute(tfa98xx);

	pr_debug("new vstep %d\n", tfa98xx->vstep);

	return 0;
}

/*
 * write all param files in the profilelist to the target
 * this is used during startup when may be ACS is set
 */
int tfaContWriteFilesProf(struct tfa98xx *tfa98xx, int profile, int vstep)
{
	struct nxpTfaProfile *prof = tfaContProfile(tfa98xx, profile);
	struct nxpTfaFileDsc *file;
	u8 *base = tfa98xx->fw.base;
	int i;

	pr_debug("\n");

	if (!prof) {
		return -EINVAL;
	}

	/* process the list and write all files  */
	for(i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscFile) {
			file = (struct nxpTfaFileDsc *)(prof->list[i].offset + base);
			if (tfaContWriteFile(tfa98xx, file)) {
				return -EINVAL;
			}
		}
	}

	tfa98xx->profile_current = profile;
	tfa98xx->vstep = vstep;

	return 0;
}

/*
 * write all  param files in the devicelist to the target
 */
int tfaContWriteFiles(struct tfa98xx *tfa98xx)
{
	struct nxpTfaDevice *dev = tfa98xx->fw.dev;
	u8 *base = tfa98xx->fw.base;
	struct nxpTfaFileDsc *file;
	int i;

	pr_debug("\n");

	/* process the list and write all files */
	for(i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscFile) {
			file = (struct nxpTfaFileDsc *)(dev->list[i].offset + base);
			if (tfaContWriteFile(tfa98xx, file)) {
				pr_err("error\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

