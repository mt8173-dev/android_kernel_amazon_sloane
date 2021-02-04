#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *dir;
static int _is_4k_video;

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

 out:
	free_page((unsigned long)buf);

	return NULL;
}

/* is_4k_video */
static int is_4k_video_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", _is_4k_video);

	return 0;
}

static ssize_t is_4k_video_proc_write(struct file *file,
						  const char __user *buffer, size_t count,
						  loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d", &_is_4k_video) != 1)
		pr_crit
			("echo [is_4k_video:0/1] > /proc/video/is_4k_video\n");

	free_page((unsigned long)buf);

	return count;
}

static int is_4k_video_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, is_4k_video_proc_show, PDE_DATA(inode));
}

static const struct file_operations is_4k_video_proc_fops = {
	.owner			= THIS_MODULE,
	.open			= is_4k_video_proc_open,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
	.write			= is_4k_video_proc_write,
};

static int __init video_proc_init(void)
{
	int ret = 0;
	struct proc_dir_entry *file = NULL;
	dir = proc_mkdir("video", NULL);
	if (!dir) {
		pr_crit("fail to create /proc/video @ %s()\n", __func__);
		return -ENOMEM;
	}
	proc_set_user(dir, 0, 1003);

	file = proc_create("is_4k_video", S_IRUGO | S_IWUSR | S_IWGRP, dir,
						&is_4k_video_proc_fops);
	if (!file) {
		pr_crit("%s(), create /proc/video/is_4k_video failed\n", __func__);
		return -ENOMEM;
	}
	proc_set_user(file, 0, 1003);

	return ret;
}

static void __exit video_proc_exit(void)
{
	if (dir)
		remove_proc_entry("is_4k_video", dir);
}
module_init(video_proc_init);
module_exit(video_proc_exit);
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
