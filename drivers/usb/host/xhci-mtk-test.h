/*
 * Copyright (C) 2014 Mediatek
 *
 * chunfeng yun <chunfeng.yun@mediatek.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __XHCI_MTK_TEST_H
#define __XHCI_MTK_TEST_H
#if IS_ENABLED(CONFIG_MTK_XHCI)
int xhci_mtk_test_creat_sysfs(struct usb_hcd *hcd);
int xhci_mtk_test_destroy_sysfs(void);

#else
static inline int xhci_mtk_test_creat_sysfs(struct usb_hcd *hcd)
{
	return 0;
}
static inline int xhci_mtk_test_destroy_sysfs(void)
{
	return 0;
}
#endif
#endif /* __XHCI_MTK_TEST_H */
