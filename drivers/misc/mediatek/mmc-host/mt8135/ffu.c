#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/reboot.h>

#include <mach/dma.h>
#include <mach/board.h> /* FIXME */
#include <mach/mt_reg_base.h>

#include "mt_sd.h"
#include <linux/mmc/sd_misc.h>
#include "board-custom.h"
#include "drivers/mmc/card/queue.h"
#include "drivers/mmc/core/mmc_ops.h"

#ifndef FPGA_PLATFORM
#include <mach/mt_clkmgr.h>
#endif

#include <asm/uaccess.h>

#define MTK_EMMC_FFU_SUPPORT
#ifdef MTK_EMMC_FFU_SUPPORT
/* Actually, these defination should be at linux/mmc.h */
#define EXT_CSD_FFU_STATUS	26
#define EXT_CSD_MODE_OPERATION_CODES 29
#define EXT_CSD_MODE_CONFIG	30
#define EXT_CSD_FW_CONFIG	169
#define EXT_CSD_NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED_00 302
#define EXT_CSD_NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED_01 303
#define EXT_CSD_NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED_02 304
#define EXT_CSD_NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED_03 305
#define EXT_CSD_FW_FFU_ARG_00 487
#define EXT_CSD_FW_FFU_ARG_01 488
#define EXT_CSD_FW_FFU_ARG_02 489
#define EXT_CSD_FW_FFU_ARG_03 490

#define EXT_CSD_FFU_FEATURES 492
#define EXT_CSD_SUPPORTED_MODES	493

#define EXT_CSD_FFU_STATUS_SUCCESS	0
#define EXT_CSD_FFU_STATUS_GENERAL_ERROR 0x10
#define EXT_CSD_FFU_STATUS_INSTALL_ERROR	0x11
#define EXT_CSD_FFU_STATUS_DOWNLOAD_ERROR	0x12

#define EXT_CSD_MODE_OPERATION_CODES_FFU_INSATALL	0x01
#define EXT_CSD_MODE_OPERATION_CODES_FFU_ABORT		0x02

#define EXT_CSD_MODE_NORMAL	0
#define EXT_CSD_MODE_FFU	1
#define EXT_CSD_MODE_VENDOR_SPECIFIC	0x10

#define EXT_CSD_FW_CONFIG_UPDATE_DISABLE	(0x01 << 0)

/* supports MODE_OPERATION_CODES field ? */
#define EXT_CSD_FFU_FEATURES_SUPPORTED_MODE_OPERATION_CODES	(0x01 << 0)

#define EXT_CSD_SUPPORTED_MODES_FFU		(0x01 << 0)
#define EXT_CSD_SUPPORTED_MODES_VSM		(0x01 << 1)

struct mtk_emmc_ffu_ext_csd {
	u8 ffu_status;
	u8 mode_operation_codes;
	u8 mode_config;
	u8 fw_config;
	u8 ffu_features;
	u8 supported_modes;
	u32 ffu_arg;
	u8 firmware_version[8]; /* EXT_CSD[261-254] device firmware version */
};
#endif

#define EMMC_FIRMWARE_BIN	"emmc_ffu.bin"
#define KB	(1024)
const struct firmware *fw;
static u32 *sg_msdc_multi_buffer;

static struct mtk_emmc_ffu_ext_csd *get_ffu_ext_csd(struct mtk_emmc_ffu_ext_csd *ext_csd)
{
	char l_buf[512];
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	struct mmc_host *mmc = NULL;
	int err = -ENOMEM;
	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!ext_csd);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	card = host->mmc->card;
	memset(l_buf, 0, sizeof(l_buf));

	mmc_claim_host(mmc);

	err = mmc_send_ext_csd(card, l_buf);
	mmc_release_host(mmc);
	if (err) {
		pr_err("EMMC FFU: Failed to send ext_csd %d\n", err);
		return NULL;
	}
	ext_csd->ffu_status = l_buf[EXT_CSD_FFU_STATUS];
	ext_csd->mode_config = l_buf[EXT_CSD_MODE_CONFIG];
	ext_csd->fw_config = l_buf[EXT_CSD_FW_CONFIG];
	ext_csd->supported_modes = l_buf[EXT_CSD_SUPPORTED_MODES];
	ext_csd->ffu_arg = ((l_buf[EXT_CSD_FW_FFU_ARG_03] << 24) | (l_buf[EXT_CSD_FW_FFU_ARG_02] << 16)
			| (l_buf[EXT_CSD_FW_FFU_ARG_01] << 8) | l_buf[EXT_CSD_FW_FFU_ARG_00]);
	memcpy(ext_csd->firmware_version, &l_buf[254], 8);
	pr_err("EMMC FFU: ext_csd->ffu_status: %02x\n", ext_csd->ffu_status);
	pr_err("EMMC FFU: ext_csd->mode_config: %02x\n", ext_csd->mode_config);
	pr_err("EMMC FFU: ext_csd->fw_config: %02x\n", ext_csd->fw_config);
	pr_err("EMMC FFU: ext_csd->supported_modes: %02x\n", ext_csd->supported_modes);
	pr_err("EMMC FFU: ext_csd->ffu_arg: %08x\n", ext_csd->ffu_arg);
	pr_err("EMMC FFU: ext_csd->firmware_version: %02x\n", ext_csd->firmware_version[0]);

	return ext_csd;
}

static int is_ffu_supported(void)
{
	struct mtk_emmc_ffu_ext_csd ffu_ext_csd;

	if (get_ffu_ext_csd(&ffu_ext_csd)) {
		return (ffu_ext_csd.supported_modes & EXT_CSD_SUPPORTED_MODES_FFU) &&
				!(ffu_ext_csd.fw_config & EXT_CSD_FW_CONFIG_UPDATE_DISABLE);
	}

	return 0;
}

static int is_mode_operation_codes_supported(void)
{
	struct mtk_emmc_ffu_ext_csd ffu_ext_csd;

	if (get_ffu_ext_csd(&ffu_ext_csd)) {
		return ffu_ext_csd.ffu_features & EXT_CSD_FFU_FEATURES_SUPPORTED_MODE_OPERATION_CODES;
	}

	return 0;
}

/* By the Spec, while MODE_OPERATION_CODES supported, Host sets MODE_OPERATION_CODES to FFU_INSATALL
 * which automacitally sets MODE_CONFIG to NORMAL
 */
static int install_firmware(void)
{
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	struct mmc_host *mmc = NULL;
	int err = -ENOMEM;
	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	card = host->mmc->card;

	mmc_claim_host(mmc);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_OPERATION_CODES, EXT_CSD_MODE_OPERATION_CODES_FFU_INSATALL, 1000);
	mmc_release_host(mmc);
	if (err) {
		pr_err("EMMC_FFU: Failed when install firmware!\n");
		return err;
	}

	return 0;
}

/* Download firmware, 32K each time */
static int download_firmware(unsigned int arg, const u8 *data, unsigned int len)
{
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	struct mmc_host *mmc = NULL;
	int err = -ENOMEM;
	struct scatterlist msdc_sg;
	struct mmc_data  msdc_data;
	struct mmc_command msdc_cmd;
	struct mmc_command msdc_stop;
	struct mmc_request  msdc_mrq;

	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	card = host->mmc->card;

	sg_msdc_multi_buffer = (u32 *)kzalloc(32*KB, GFP_KERNEL);
	if (sg_msdc_multi_buffer == NULL) {
		pr_err("EMMC_FFU: allock 64KB memory failed\n");
		return err;
	}

	pr_err("EMMC FFU: %s, %d, len: %d\n", __func__, __LINE__, len);
	memcpy(sg_msdc_multi_buffer, data, len);

	pr_err("EMMC FFU: %s, %d, len: %d\n", __func__, __LINE__, len);
	msdc_polling_idle(host);
	/* Now send CMD25 + FFU_ARG */
	memset(&msdc_data, 0, sizeof(struct mmc_data));
	memset(&msdc_mrq, 0, sizeof(struct mmc_request));
	memset(&msdc_cmd, 0, sizeof(struct mmc_command));
	memset(&msdc_stop, 0, sizeof(struct mmc_command));

	msdc_mrq.cmd = &msdc_cmd;
	msdc_mrq.data = &msdc_data;

	msdc_data.flags = MMC_DATA_WRITE;
	msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	msdc_data.blocks = len / 512 ;
	msdc_cmd.arg = arg;
	msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	msdc_stop.opcode = MMC_STOP_TRANSMISSION;
	msdc_stop.arg = 0;
	msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	msdc_data.stop = &msdc_stop;

	msdc_data.blksz = 512;
	msdc_data.sg = &msdc_sg;
	msdc_data.sg_len = 1;

	sg_init_one(&msdc_sg, sg_msdc_multi_buffer, len);
	mmc_set_data_timeout(&msdc_data, card);
	mmc_wait_for_req(mmc, &msdc_mrq);
	kfree(sg_msdc_multi_buffer);
	msdc_polling_idle(host);

	if (msdc_cmd.error) {
		pr_err("EMMC_FFU: SEND CMD25 error %d\n", msdc_cmd.error);
		return msdc_cmd.error;
	}
	if (msdc_data.error) {
		pr_err("EMMC_FFU: Write eMMC data error %d\n", msdc_data.error);
		return msdc_data.error;
	}
	return 0;

}
static int do_mtk_emmc_ffu(void)
{
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	struct mmc_host *mmc = NULL;
	int err = -ENOMEM;
	struct mtk_emmc_ffu_ext_csd ffu_ext_csd;
	unsigned int left = 0;
	const u8 *data = fw->data;
	unsigned int download_arg = 0;

	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	card = host->mmc->card;

	mmc_claim_host(mmc);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG, EXT_CSD_MODE_FFU, 1000);
	mmc_release_host(mmc);
	if (err) {
		pr_err("EMMC_FFU: Failed to swtich to FFU mode!\n");
		return err;
	}
	/* need re-get ext_csd here, some emmc card(KSI) need set ffu mode first, then the ffu_arg is correct */
	if (get_ffu_ext_csd(&ffu_ext_csd)) {
		download_arg = ffu_ext_csd.ffu_arg; /* just get the ffu_arg */
	} else {
		goto out;
	}

	left = fw->size;

	mmc_claim_host(mmc);
	while (left) {
		download_arg = ffu_ext_csd.ffu_arg;

		if (left >= 32*KB) {
			err = download_firmware(download_arg, &data[fw->size - left], 32*KB);
			left -= 32*KB;
			if (err) {
				mmc_release_host(mmc);
				return err;
			}
		} else {
			err = download_firmware(download_arg, &data[fw->size - left], left);
			left = 0;
			if (err) {
				mmc_release_host(mmc);
				return err;
			}
		}
	}
	mmc_release_host(mmc);
	return 0;

out:
	return -1;
}

/* ffu_failed is the return value from do_mtk_emmc_ffu */
static int post_mtk_emmc_ffu(int ffu_failed)
{
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	struct mmc_host *mmc = NULL;
	int err = -ENOMEM;


	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	card = host->mmc->card;


	if (ffu_failed) {
		mmc_claim_host(mmc);
		/* back to normal */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG, EXT_CSD_MODE_NORMAL, 1000);
		mmc_release_host(mmc);
		if (err) {
			pr_err("EMMC FFU: Failed to swtich to NORMAL mode!\n");
			return err;
		}
	} else if (is_mode_operation_codes_supported()) {
		err = install_firmware();
		if (err) {
			return err;
		}
	} else {
		/* Host sets MODE_CONFIG to Normal and performs CMD0/HW Rset/Power cycle */
		mmc_claim_host(mmc);
		/* back to normal */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG, EXT_CSD_MODE_NORMAL, 1000);
		mmc_release_host(mmc);
		if (err) {
			pr_err("EMMC FFU: Failed to swtich to NORMAL mode!\n");
			return err;
		} else {
			/* do Power cycle, not return */
			kernel_restart(NULL);
		}
	}

	return 0;
}

#if 0
static int check_ffu_status(void)
{
	struct mtk_emmc_ffu_ext_csd ffu_ext_csd;

	if (get_ffu_ext_csd(&ffu_ext_csd)) {
		if (ffu_ext_csd.ffu_status == EXT_CSD_FFU_STATUS_SUCCESS) {
			pr_info("EMMC FFU: Field firmware update successed!\n");
			return 1;
		} else if (ffu_ext_csd.ffu_status == EXT_CSD_FFU_STATUS_GENERAL_ERROR) {
			pr_err("EMMC FFU: Field firmware update failed! program fail!\n");
			return 0;
		} else if (ffu_ext_csd.ffu_status == EXT_CSD_FFU_STATUS_INSTALL_ERROR) {
			pr_err("EMMC FFU: Field firmware update failed! host does not finish FFU flow and power cycle occurs!\n");
			return 0;
		} else if (ffu_ext_csd.ffu_status == EXT_CSD_FFU_STATUS_DOWNLOAD_ERROR) {
			pr_err("EMMC FFU: Field firmware update failed! header information is incorrect!\n");
			return 0;
		} else {
			pr_err("EMMC FFU: Field firmware update failed! unknow ffu_status: %d\n", ffu_ext_csd.ffu_status);
		}
	}

	return 0;
}
#endif

static int request_emmc_firmware(void)
{
	struct msdc_host *host = NULL;
	struct mmc_card *card = NULL;
	int err = -ENOMEM;
	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	card = host->mmc->card;

	err = request_firmware(&fw, EMMC_FIRMWARE_BIN, &card->dev);
	if (err < 0) {
		pr_err("EMMC FFU: %s request firmware failed %d\n", EMMC_FIRMWARE_BIN, err);
		return err;
	}
	if (fw->size % 512) {
		pr_err("EMMC FFU: Size of %s is not multiple of 512!\n", EMMC_FIRMWARE_BIN);
		return -EINVAL;
	}

	if (fw->size > (512*KB)) {
		pr_err("EMMC FFU: Firmware size is too big: %d\n", fw->size);
		return -EINVAL;
	}
	pr_info("EMMC FFU: %s request firmware successed, fw size is %d\n", EMMC_FIRMWARE_BIN, fw->size);

	return 0;
}
static void release_emmc_firmware(void)
{
	release_firmware(fw);
}

/* By this method, user can check whether FFU is supported */
static ssize_t emmc_ffu_read(struct file *filp, char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	char buf[2] = {0};
	if (is_ffu_supported()) {
		buf[0] = '1';
	} else {
		buf[0] = 0;
	}
	return simple_read_from_buffer(ubuf, cnt, ppos,
			buf, 2);
}

static int emmc_ffu_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int emmc_ffu_write(struct file *file, const char __user *buffer,
							size_t size, loff_t *ppos)
{
	char tmp[4];
	int err = 0;
	memset(tmp, 0, sizeof(tmp));
	if (size > 2) {
		pr_err("EMMC FFU: Invalied argument, should be '0' or '1'\n");
		return -EINVAL;
	}
	if (copy_from_user(tmp, buffer, size)) {
		return  -EFAULT;
	}

	if (tmp[0] == '1') {
		pr_info("EMMC FFU: start to do ffu!\n");
		if (0 == request_emmc_firmware()) {
			if (is_ffu_supported()) {
				err = do_mtk_emmc_ffu();
				err = post_mtk_emmc_ffu(err);
			} else {
				pr_err("EMMC FFU: Do not support FFU!\n");
			}
		}
		release_emmc_firmware();
	} else {
		pr_err("EMMC FFU: Invalied argument, should be '0' or '1'\n");
	}

	return size;
}

static int emmc_ffu_release(struct inode *inode, struct file *file)
{
	return 0;
}
const struct file_operations mmc_dbg_ffu_fops = {
	.owner = THIS_MODULE,
	.open = emmc_ffu_open,
	.read = emmc_ffu_read,
	.llseek = default_llseek,
	.write = emmc_ffu_write,
	.release = emmc_ffu_release,
};
EXPORT_SYMBOL_GPL(mmc_dbg_ffu_fops);

MODULE_LICENSE("GPL");

