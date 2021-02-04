#include "cmdq_fs.h"
#include "cmdq_core.h"
/*internal use*/
static void fs_create(struct fs_struct *file, const char *fileName);
static void fs_write(struct fs_struct *file, char *buffer);
static void fs_close(struct fs_struct *file);

void init_fs_struct(struct fs_struct *file)
{
	memset(file->fileName, 0, MAX_SIZE);
	file->fs = 0;
	file->fp = NULL;
	file->fs_create = fs_create;
	file->fs_write = fs_write;
	file->fs_close = fs_close;
}


void fs_create(struct fs_struct *file, const char *fileName)
{
	memset(file->fileName, 0, MAX_SIZE);
	if (NULL != fileName && *fileName != '\0')
		sprintf(file->fileName, "/sdcard/%s", fileName);
	file->fs = get_fs();
	set_fs(KERNEL_DS);
	file->fp = filp_open("/sdcard/1.txt", O_RDWR | O_CREAT | O_TRUNC, 0x644);
	if (IS_ERR(file->fp))
		CMDQ_ERR("create file[%s] error, fp[%p]\n", file->fileName, file->fp);
	else
		CMDQ_MSG("create file[%s] sucess, fp[%p]\n", file->fileName, file->fp);
}

void fs_write(struct fs_struct *file, char *buffer)
{
	file->fp->f_op->write(file->fp, buffer, strlen(buffer), &file->fp->f_pos);
}

void fs_close(struct fs_struct *file)
{
	if (file->fp)
		filp_close(file->fp, NULL);
	set_fs(file->fs);
}

