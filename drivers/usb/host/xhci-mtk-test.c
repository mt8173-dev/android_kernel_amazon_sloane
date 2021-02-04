
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include "../core/usb.h"

#include "xhci.h"
#include <linux/kobject.h>
static struct usb_hcd *test_hcd;
static struct kobject *mu3h_kobj;

static int t_test_j(int argc, char **argv);
static int t_test_k(int argc, char **argv);
static int t_test_se0(int argc, char **argv);
static int t_test_packet(int argc, char **argv);
static int t_test_suspend(int argc, char **argv);
static int t_test_resume(int argc, char **argv);
static int t_test_get_device_descriptor(int argc, char **argv);
static int t_test_enumerate_bus(int argc, char **argv);

#define PORT_PLS_VALUE(p) ((p>>5) & 0xf)

#define mu3h_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define MAX_NAME_SIZE 32
#define MAX_ARG_SIZE 4

#define mu3h_dbg(fmt, args...)  xhci_err(hcd_to_xhci(test_hcd), fmt, ## args)

struct hqa_test_cmd {
	char name[MAX_NAME_SIZE];
	int (*cb_func)(int argc, char **argv);
	char *discription;
};


struct hqa_test_cmd xhci_mtk_hqa_cmds[] = {
	{"test.j", &t_test_j, "Test_J"},
	{"test.k", &t_test_k, "Test_K"},
	{"test.se0", &t_test_se0, "Test_SE0_NAK"},
	{"test.packet", &t_test_packet, "Test_PACKET"},
	{"test.suspend", &t_test_suspend, "Port Suspend"},
	{"test.resume", &t_test_resume, "Port Resume"},
	{"test.enumbus", &t_test_enumerate_bus, "Enumerate Bus"},
	{"test.getdesc", &t_test_get_device_descriptor, "Single Step Get Device Discriptor"},
	{"", NULL, ""},
};


int call_function_mu3h(char *buf)
{
	int i;
	int argc;
	char *argv[MAX_ARG_SIZE];

	argc = 0;
	do {
		argv[argc] = strsep(&buf, " ");
		mu3h_dbg("[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);

	for (i = 0; i < sizeof(xhci_mtk_hqa_cmds) / sizeof(struct hqa_test_cmd); i++) {
		if ((!strcmp(xhci_mtk_hqa_cmds[i].name, argv[0])) && (xhci_mtk_hqa_cmds[i].cb_func != NULL))
			return xhci_mtk_hqa_cmds[i].cb_func(argc, argv);
	}

	return -1;
}


int mu3h_test_mode = 0;

int test_mode_enter(u32 port_id, u32 test_value)
{
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	u32 temp;

	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 0) {
		xhci_stop(test_hcd);
		xhci_halt(xhci);
	}

	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);

	mu3h_test_mode = 1;

	return 0;
}

int test_mode_exit(void)
{
	struct xhci_hcd *xhci;

	/*struct usb_hcd *secondary_hcd;*/

	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 1) {
#if 0
		xhci_reset(xhci);
		/*reinitIP(&pdev->dev);*/

		if (!usb_hcd_is_primary_hcd(test_hcd))
			secondary_hcd = test_hcd;
		else
			secondary_hcd = xhci->shared_hcd;

		retval = xhci_init(test_hcd->primary_hcd);
		if (retval)
			return retval;

		retval = xhci_run(test_hcd->primary_hcd);
		if (!retval)
			retval = xhci_run(secondary_hcd);

		/*enableXhciAllPortPower(xhci);*/
#endif
		mu3h_test_mode = 0;
	}
	return 0;
}

static int t_test_j(int argc, char **argv)
{
	long port_id;
	u32 test_value;

	port_id = 2;
	test_value = 1;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	test_mode_enter(port_id, test_value);

	return 0;
}

static int t_test_k(int argc, char **argv)
{
	long port_id;
	u32 test_value;

	port_id = 2;
	test_value = 2;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	test_mode_enter(port_id, test_value);

	return 0;
}

static int t_test_se0(int argc, char **argv)
{
	long port_id;
	u32 test_value;

	port_id = 2;
	test_value = 3;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%ld\n", __func__, port_id);
	test_mode_enter(port_id, test_value);

	return 0;
}

static int t_test_packet(int argc, char **argv)
{
	long port_id;
	u32 test_value;
	port_id = 2;
	test_value = 4;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%ld\n", __func__, port_id);
	test_mode_enter(port_id, test_value);

	return 0;
}

static int t_test_suspend(int argc, char **argv)
{
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	u32 temp;
	long port_id;

	port_id = 2;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 1) {
		test_mode_exit();
		msleep(2000);
	}

	/* set PLS = 3 */
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);

    msleep(100);
	temp = xhci_readl(xhci, addr);
	if (PORT_PLS_VALUE(temp) != 3) {
		mu3h_dbg("port not enter suspend state\n");
		return -1;
	} else
		mu3h_dbg("port enter suspend state\n");

	return 0;
}

static int t_test_resume(int argc, char **argv)
{
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	u32 temp;
	long port_id;

	port_id = 2;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 1) {
		mu3h_dbg("please suspend port first\n");
		return -1;
	}
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
	temp = xhci_readl(xhci, addr);
	if (PORT_PLS_VALUE(temp) != 3) {
		mu3h_dbg("port not in suspend state, please suspend port first\n");
		return -1;
	} else {
		temp = xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | (15 << 5) | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
    msleep(20);
		temp = xhci_readl(xhci, addr);
		temp = xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		msleep(100);
		temp = xhci_readl(xhci, addr);
		if (PORT_PLS_VALUE(temp) != 0) {
			mu3h_dbg("port rusume fail\n");
			return -1;
		} else
		  mu3h_dbg("port resume ok\n");
	}

	return 0;
}

static int t_test_enumerate_bus(int argc, char **argv)
{
	struct xhci_hcd *xhci;
	struct usb_device *usb2_rh;
	struct usb_device *udev;
	long port_id;
	u32 retval;

	port_id = 2;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 1) {
		test_mode_exit();
		return 0;
	}

	usb2_rh = test_hcd->self.root_hub;

#if 1
	udev = usb_hub_find_child(usb2_rh, port_id-1);
#else
	udev = usb2_rh->children[port_id-2];
#endif

	if (udev != NULL) {
		retval = usb_reset_device(udev);
		if (retval) {
			mu3h_dbg("ERROR: enumerate bus fail!\n");
			return -1;
		}
	} else {
		mu3h_dbg("ERROR: Device does not exist!\n");
		return -1;
	}

	return 0;
}
static int t_test_get_device_descriptor(int argc, char **argv)
{
	struct xhci_hcd *xhci;
	struct usb_device *usb2_rh;
	struct usb_device *udev;
	long port_id;
	u32 retval = 0;

	port_id = 2;

	if (argc > 1 && kstrtol(argv[1], 10, &port_id))
		mu3h_dbg("mu3h %s get port-id failed\n", __func__);

	mu3h_dbg("mu3h %s test port%d\n", __func__, (int)port_id);
	xhci = hcd_to_xhci(test_hcd);

	if (mu3h_test_mode == 1) {
		test_mode_exit();
		msleep(2000);
	}

	usb2_rh = test_hcd->self.root_hub;

#if 1
	udev = usb_hub_find_child(usb2_rh, port_id-1);
#else
	udev = usb2_rh->children[port_id-2];
#endif

	if (udev != NULL) {
		retval = usb_get_device_descriptor(udev, USB_DT_DEVICE_SIZE);
		if (retval != sizeof(udev->descriptor)) {
			mu3h_dbg("ERROR: get device descriptor fail!\n");
			return -1;
		}
	} else {
		mu3h_dbg("ERROR: Device does not exist!\n");
		return -1;
	}

	return 0;
}

static ssize_t test_cmd_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
/*	return sprintf(buf, "%d\n", force_suspend); */
	return 1;
}

static ssize_t test_cmd_store(struct kobject *kobj, struct kobj_attribute *attr,
	       const char *buf, size_t n)
{
	int retval;

	retval = 0;

	retval = call_function_mu3h((char *)buf);
	if (retval < 0) {
		mu3h_dbg(KERN_DEBUG "mu3h cli fail\n");
		return -1;
	}

	return n;
}

mu3h_attr(test_cmd);


static struct attribute *mu3h_test_cmd[] = {
	&test_cmd_attr.attr,
	NULL,
};

static struct attribute_group mu3h_attr_group = {
	.attrs = mu3h_test_cmd,
};


int xhci_mtk_test_creat_sysfs(struct usb_hcd *hcd)
{
	int retval = 0;

	test_hcd = hcd;
	mu3h_kobj = kobject_create_and_add("mu3h", NULL);
	if (!mu3h_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(mu3h_kobj, &mu3h_attr_group);

	return retval;
}

int xhci_mtk_test_destroy_sysfs(void)
{
	sysfs_remove_group(mu3h_kobj, &mu3h_attr_group);
	kobject_put(mu3h_kobj);
	return 0;
}
