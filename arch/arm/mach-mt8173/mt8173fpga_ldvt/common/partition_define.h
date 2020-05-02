#ifndef __PARTITION_DEFINE_H__
#define __PARTITION_DEFINE_H__

#define KB  (1024)
#define MB  (1024 * KB)
#define GB  (1024 * MB)

#define PART_PRELOADER "PRELOADER"
#define PART_PGPT "PGPT"
#define PART_PRO_INFO "PRO_INFO"
#define PART_NVRAM "NVRAM"
#define PART_PROTECT_F "PROTECT_F"
#define PART_PROTECT_S "PROTECT_S"
#define PART_SECCFG "SECCFG"
#define PART_SECURE "SECURE"
#define PART_UBOOT "UBOOT"
#define PART_BOOTIMG "BOOTIMG"
#define PART_RECOVERY "RECOVERY"
#define PART_SEC_RO "SEC_RO"
#define PART_SECSTATIC "SECSTATIC"
#define PART_MISC "MISC"
#define PART_LOGO "LOGO"
#define PART_EXPDB "EXPDB"
#define PART_APANIC "APANIC"
#define PART_ANDROID "ANDROID"
#define PART_ANDSYSIMG "ANDSYSIMG"
#define PART_CACHE "CACHE"
#define PART_USRDATA "USRDATA"
#define PART_USER "USER"
#define PART_FAT "FAT"
#define PART_SGPT "SGPT"

#define PART_FLAG_NONE			0
#define PART_FLAG_LEFT			0x1
#define PART_FLAG_END			0x2
#define PART_MAGIC			0x58881688
#define PART_SIZE_PRELOADER			(256*KB)
#define PART_SIZE_PGPT			(512*KB)
#define PART_SIZE_PRO_INFO			(3072*KB)
#define PART_SIZE_NVRAM			(5120*KB)
#define PART_SIZE_PROTECT_F			(10240*KB)
#define PART_SIZE_PROTECT_S			(10240*KB)
#define PART_SIZE_SECCFG			(256*KB)
#define PART_OFFSET_SECCFG			(0x1c80000)
#define PART_SIZE_UBOOT			(384*KB)
#define PART_SIZE_BOOTIMG			(15360*KB)
#define PART_SIZE_RECOVERY			(10240*KB)
#define PART_SIZE_SEC_RO			(6144*KB)
#define PART_OFFSET_SEC_RO			(0x3620000)
#define PART_SIZE_MISC			(512*KB)
#define PART_SIZE_LOGO			(8192*KB)
#define PART_SIZE_EXPDB			(11648*KB)
#define PART_SIZE_ANDROID			(819200*KB)
#define PART_SIZE_CACHE			(131072*KB)
#define PART_SIZE_USRDATA			(1212416*KB)
#define PART_SIZE_FAT			(0*KB)
#define PART_SIZE_SGPT			(512*KB)


#define PART_NUM			19



#define PART_MAX_COUNT			 40

#define WRITE_SIZE_Byte		512
typedef enum {
	EMMC = 1,
	NAND = 2,
} dev_type;

#ifdef CONFIG_MTK_EMMC_SUPPORT
typedef enum {
	EMMC_PART_UNKNOWN =
	    0, EMMC_PART_BOOT1, EMMC_PART_BOOT2, EMMC_PART_RPMB, EMMC_PART_GP1, EMMC_PART_GP2,
	EMMC_PART_GP3, EMMC_PART_GP4, EMMC_PART_USER, EMMC_PART_END
} Region;
#else
typedef enum {
	NAND_PART_USER
} Region;
#endif
struct excel_info {
	char *name;
	unsigned long long size;
	unsigned long long start_address;
	dev_type type;
	unsigned int partition_idx;
	Region region;
};
extern struct excel_info *PartInfo;


#endif
