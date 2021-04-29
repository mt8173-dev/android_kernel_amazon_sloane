/*
 ***************************************************************************
 * MediaTec Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2006, MediaTek Technology, Inc.
 *
 * All rights reserved. MediaTek's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    debugfs.c

Abstract:
    debugfs related subroutines

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Conard Chou   03-10-2015    created
*/

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "rt_config.h"

struct dentry *mt_dfs_dir = NULL;

#define DFS_FILE_OPS(name)						\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.read = mt_##name##_read,					\
	.write = mt_##name##_write,					\
	.open = mt_open_generic,					\
};

#define DFS_FILE_READ_OPS(name)						\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.read = mt_##name##_read,					\
	.open = mt_open_generic,					\
};

#define DFS_FILE_WRITE_OPS(name)					\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.write = mt_##name##_write,					\
	.open = mt_open_generic,					\
};



static int mt_open_generic(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static ssize_t mt_info_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf, *p;
	size_t size = 1024;
	PRTMP_ADAPTER pad = (PRTMP_ADAPTER) file->private_data;
	unsigned int fw_ver = pad->FirmwareVersion;

	buf = kzalloc(size, GFP_KERNEL);
	p = buf;
	if (!p)
		return -ENOMEM;

	p += sprintf(p, "CHIP Ver:Rev=0x%08x\n", pad->MACVersion);
	p += sprintf(p, "DRI VER-%s  FW VER-%X.%X.%X\n", STA_DRIVER_VERSION,
		(fw_ver & 0xff000000u) >> 24, (fw_ver & 0x00ff0000u) >> 16,
		(fw_ver & 0x0000ffffu));
	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);


	kfree(buf);
	return retval;
}


static ssize_t mt_debug_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf;
	size_t size = 1024;
	size_t len = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = sprintf(buf, "debug read\n");
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	return retval;
}

static ssize_t mt_statlog_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf, *p;
	unsigned long size = 2048;
	PRTMP_ADAPTER pad = (PRTMP_ADAPTER) file->private_data;

	buf = kzalloc(size, GFP_KERNEL);

	p = buf;
	if (!p)
		return -ENOMEM;

	memset(p, 0x00, size);
	RtmpIoctl_rt_private_get_statistics(pad, p, size);
	p = (char *)((size_t) p + strlen(p) + 1);   /* 1: size of '\0' */

	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);

	kfree(buf);
	return retval;
}

static ssize_t mt_fwpath_write(struct file *file, const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	char buf[16] = {0};
	if (0 == copy_from_user(buf, user_buf, min(count, sizeof(buf))))
		DBGPRINT(RT_DEBUG_TRACE, ("%s:%s\n", __func__, buf));
	return count;
}
#ifdef WCX_SUPPORT
#define NVRAM_QUERY_PERIOD 100
#define NVRAM_QUERY_TIMES 5
static ssize_t mt_nvram_write(struct file *file, const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) file->private_data;
	char buf[16] = {0};
	UINT32 wait_cnt = 0;

	if (count <= 0
)
		DBGPRINT(RT_DEBUG_ERROR, ("%s:invalid param\n", __func__));

	if (0 == copy_from_user(buf, user_buf, min(count, sizeof(buf)))) {
		DBGPRINT(RT_DEBUG_TRACE, ("mt_nvram_write:%s\n", buf));
		while (TRUE != pAd->chipOps.eeinit(pAd) && wait_cnt < NVRAM_QUERY_TIMES) {
			msleep(NVRAM_QUERY_PERIOD);
			wait_cnt++;
			DBGPRINT(RT_DEBUG_TRACE, ("Get NVRAM times(%d)\n", wait_cnt));
		}
		if (wait_cnt >= NVRAM_QUERY_TIMES) {
			DBGPRINT(RT_DEBUG_ERROR, ("Get NVRAM timeout\n"));
			pAd->nvramValid = FALSE;
		} else
			pAd->nvramValid = TRUE;
	} else
		return -EFAULT;

	return count;
}
#endif /* WCX_SUPPORT */

/* Add debugfs file operation */
DFS_FILE_READ_OPS(info);
DFS_FILE_READ_OPS(debug);
DFS_FILE_READ_OPS(statlog);
DFS_FILE_WRITE_OPS(fwpath);
#ifdef WCX_SUPPORT
DFS_FILE_WRITE_OPS(nvram);
#endif /* WCX_SUPPORT */

/*
 * create debugfs root dir
 */
void mt_debugfs_init(void)
{
	char *dfs_dir_name = "mtwifi";

	if (!mt_dfs_dir)
		mt_dfs_dir = debugfs_create_dir(dfs_dir_name, NULL);


	if (!mt_dfs_dir) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create %s dir fail!\n", __func__, dfs_dir_name));
		return;
	}
}

/*
 * create debugfs directory & file for each device
 */
void mt_dev_debugfs_init(PRTMP_ADAPTER pad)
{
	POS_COOKIE pobj = (POS_COOKIE) pad->OS_Cookie;

	DBGPRINT(RT_DEBUG_INFO, ("device debugfs init start!\n"));

	/* create device dir */
	pobj->debugfs_dev = debugfs_create_dir(pad->net_dev->name, mt_dfs_dir);

	if (!pobj->debugfs_dev) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create %s dir fail!\n",
					__func__, pad->net_dev->name));
		return;
	}
	/* Add debugfs file */
	if (!debugfs_create_file("info", 0644, pobj->debugfs_dev, pad, &mt_dfs_info_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create info file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("debug", 0644, pobj->debugfs_dev, pad, &mt_dfs_debug_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create debug file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("statlog", 0644, pobj->debugfs_dev, pad, &mt_dfs_statlog_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("fwpath", 0644, pobj->debugfs_dev, pad, &mt_dfs_fwpath_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}
#ifdef WCX_SUPPORT
	if (!debugfs_create_file("nvram", 0644, pobj->debugfs_dev, pad, &mt_dfs_nvram_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}
#endif /* WCX_SUPPORT */
	DBGPRINT(RT_DEBUG_INFO, ("debugfs init finish!\n"));
	return;

LABEL_CREATE_FAIL:
	debugfs_remove_recursive(pobj->debugfs_dev);
	pobj->debugfs_dev = NULL;
}

/*
 * remove debugfs directory and files for each device
 */
void mt_dev_debugfs_remove(PRTMP_ADAPTER pad)
{
	POS_COOKIE pobj = (POS_COOKIE) pad->OS_Cookie;

	if (pobj->debugfs_dev != NULL) {
		debugfs_remove_recursive(pobj->debugfs_dev);
		pobj->debugfs_dev = NULL;
		DBGPRINT(RT_DEBUG_INFO, ("debugfs remove finsih!\n"));
	}
}

/*
 * remove debugfs root dir
 */
void mt_debugfs_remove(void)
{
	if (mt_dfs_dir != NULL) {
		debugfs_remove(mt_dfs_dir);
		mt_dfs_dir = NULL;
	}
}
