/*
 * idme.c
 *
 * Copyright 2013 Amazon Technologies, Inc.  or its Affiliates  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define IDME_OF_BOARD_ID	"/idme/board_id"

#define PRODUCT_FEATURES_DIR "product_features"
#define PRODUCT_FEATURE_NAME_GPS "gps"
#define PRODUCT_FEATURE_NAME_WAN "wan"
#define MAC_SEC_KEY "mac_sec"
#define MAC_SEC_OWNER 1000

#define PRODUCT_FEATURE_STRING_GPS " "
#define PRODUCT_FEATURE_STRING_WAN " "

static int idme_proc_show(struct seq_file *seq, void *v)
{
	struct property *pp = (struct property *)seq->private;

	BUG_ON(!pp);

	seq_write(seq, pp->value, pp->length);

	return 0;
}

static int idme_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, idme_proc_show, PDE_DATA(inode));
}

static const struct file_operations idme_fops = {
	.owner = THIS_MODULE,
	.open = idme_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

bool board_has_wan(void)
{
	struct device_node *ap;
	int len;

	ap = of_find_node_by_path(IDME_OF_BOARD_ID);
	if (ap) {
		const char *boardid = of_get_property(ap, "value", &len);
		if (len >= 2) {
			if (boardid[0] == '0' && boardid[5] == '1')
				return true;
		}
	}

	return false;
}

EXPORT_SYMBOL(board_has_wan);

unsigned int idme_get_board_type(void)
{
	struct device_node *ap = NULL;
	char board_type[5] = { 0 };
	const char *board_id = NULL;
	unsigned int rs = 0;

	ap = of_find_node_by_path(IDME_OF_BOARD_ID);
	if (ap)
		board_id = (const char *)of_get_property(ap, "value", NULL);
	else
		pr_err("of_find_node_by_path failed\n");

	strlcpy(board_type, board_id, sizeof(board_type));
	if (unlikely(kstrtouint(board_type, 16, &rs)))
		pr_err("idme_get_board_type kstrtouint failed!\v");

	return rs;
}

EXPORT_SYMBOL(idme_get_board_type);

unsigned int idme_get_board_rev(void)
{
	struct device_node *ap = NULL;
	char board_rev[3] = { 0 };
	const char *board_id = NULL;
	unsigned int rs = 0;

	ap = of_find_node_by_path(IDME_OF_BOARD_ID);
	if (ap)
		board_id = (const char *)of_get_property(ap, "value", NULL);
	else
		pr_err("of_find_node_by_path failed\n");

	strlcpy(board_rev, (board_id + 7), sizeof(board_rev));

	if (unlikely(kstrtouint(board_rev, 16, &rs)))
		pr_err("idme_get_board_rev kstrtouint failed!\v");

	return rs;
}

EXPORT_SYMBOL(idme_get_board_rev);

static int __init idme_init(void)
{
	struct proc_dir_entry *proc_idme = NULL;
	struct device_node *root = NULL, *child = NULL;
	struct property *pp_value = NULL;
	int perm = 0;
	/* static struct proc_dir_entry *proc_product_features_dir; */
	struct proc_dir_entry *child_pde = NULL;
	bool access_restrict = false;

	root = of_find_node_by_path("/idme");
	if (!root)
		return -EINVAL;

	/* Create the root IDME procfs node */
	proc_idme = proc_mkdir("idme", NULL);
	if (!proc_idme) {
		of_node_put(root);
		return -ENOMEM;
	}

	/* Populate each IDME field */
	for (child = NULL; (child = of_get_next_child(root, child));) {
		pp_value = of_find_property(child, "value", NULL);

		if (strcmp(child->name, MAC_SEC_KEY) == 0)
			access_restrict = true;
		else
			access_restrict = false;

		if (!pp_value)
			continue;

		if (of_property_read_u32(child, "permission", &perm))
			continue;

		/* These values aren't writable anyways */
		perm &= ~(S_IWUGO);

		if (access_restrict)
			perm = 0400;

		child_pde = proc_create_data(child->name, perm, proc_idme,
			&idme_fops, pp_value);

		if (child_pde && access_restrict)
			proc_set_user(child_pde, MAC_SEC_OWNER, 0);
	}

	of_node_put(child);
	of_node_put(root);

	return 0;
}

fs_initcall(idme_init);
