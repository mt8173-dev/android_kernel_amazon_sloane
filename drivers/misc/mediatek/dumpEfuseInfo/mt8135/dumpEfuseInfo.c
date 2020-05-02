#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <mach/devs.h>

#define SEC_CTRL_INDEX       6
#define SBC_FLAG_MASK        0x02 /*bit[1]*/
#define DAA_FLAG_MASK        0x04 /*bit[2]*/
#define SW_JTAG_MASK         0x40 /*bit[6]*/
#define DIS_SEC_CAP_INDEX    25
#define DIS_SEC_CAP_MASK     0x010000 /*bit[16]*/
#define SBC_KEY_HASH_0_INDEX 26
#define SBC_KEY_HASH_1_INDEX 27
#define SBC_KEY_HASH_2_INDEX 28
#define SBC_KEY_HASH_3_INDEX 29
#define SBC_KEY_HASH_4_INDEX 30
#define SBC_KEY_HASH_5_INDEX 31
#define SBC_KEY_HASH_6_INDEX 32
#define SBC_KEY_HASH_7_INDEX 33
#define M_SEC_LOCK_INDEX     34
#define SEC_LOCK_INDEX       35
#define AC_KEY0_INDEX        36
#define AC_KEY1_INDEX        37
#define AC_KEY2_INDEX        38
#define AC_KEY3_INDEX        39
#define HDMIK0_INDEX         40
#define HDMIK1_INDEX         41
#define HDMIK2_INDEX         42
#define HDMIK3_INDEX         43

static int __init dumpEfuseInfo_init(void)
{
    /*printk(KERN_ALERT"dumpEfuseInfo init\n");*/

    printk(KERN_ALERT" ====================== Start to dump Efuse info ======================\n");

    /*printk(KERN_ALERT"SBC_FLAG %08X\n", get_devinfo_with_index(SEC_CTRL_INDEX) & SBC_FLAG_MASK);*/
    /*printk(KERN_ALERT"DAA_FLAG %08X\n", get_devinfo_with_index(SEC_CTRL_INDEX) & DAA_FLAG_MASK);*/
    /*printk(KERN_ALERT"SW_JTAG_FLAG %08X\n", get_devinfo_with_index(SEC_CTRL_INDEX) & SW_JTAG_MASK);*/
    printk(KERN_ALERT" SEC_CTRL       %08X\n", get_devinfo_with_index(SEC_CTRL_INDEX));
    printk(KERN_ALERT" DIS_SEC_CAP    %8X\n",  get_devinfo_with_index(DIS_SEC_CAP_INDEX) & DIS_SEC_CAP_MASK);
    printk(KERN_ALERT" SBC_KEY_HASH_0 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_0_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_1 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_1_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_2 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_2_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_3 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_3_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_4 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_4_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_5 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_5_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_6 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_6_INDEX));
    printk(KERN_ALERT" SBC_KEY_HASH_7 %08X\n", get_devinfo_with_index(SBC_KEY_HASH_7_INDEX));
    printk(KERN_ALERT" M_SEC_LOCK     %08X\n", get_devinfo_with_index(M_SEC_LOCK_INDEX));
    printk(KERN_ALERT" SEC_LOCK       %08X\n", get_devinfo_with_index(SEC_LOCK_INDEX));
    printk(KERN_ALERT" AC_KEY0        %08X\n", get_devinfo_with_index(AC_KEY0_INDEX));
    printk(KERN_ALERT" AC_KEY1        %08X\n", get_devinfo_with_index(AC_KEY1_INDEX));
    printk(KERN_ALERT" AC_KEY2        %08X\n", get_devinfo_with_index(AC_KEY2_INDEX));
    printk(KERN_ALERT" AC_KEY3        %08X\n", get_devinfo_with_index(AC_KEY3_INDEX));
    printk(KERN_ALERT" HDMIK0         %08X\n", get_devinfo_with_index(HDMIK0_INDEX));
    printk(KERN_ALERT" HDMIK1         %08X\n", get_devinfo_with_index(HDMIK1_INDEX));
    printk(KERN_ALERT" HDMIK2         %08X\n", get_devinfo_with_index(HDMIK2_INDEX));
    printk(KERN_ALERT" HDMIK3         %08X\n", get_devinfo_with_index(HDMIK3_INDEX));

    printk(KERN_ALERT" ===================== Finish dumping Efuse info ======================\n");

	return 0;
}

static void __exit dumpEfuseInfo_exit(void)
{
    printk(KERN_ALERT" dumpEfuseInfo exits\n");
}

module_init(dumpEfuseInfo_init);
module_exit(dumpEfuseInfo_exit);
MODULE_LICENSE("GPL");
