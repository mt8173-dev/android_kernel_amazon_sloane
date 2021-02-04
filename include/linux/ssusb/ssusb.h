/*
 * This is used to for host and peripheral modes of the driver for
 * super speed Dual-Role Controllers (MediaTek).
 */

#ifndef _LINUX_USB_SSUSB_H_
#define _LINUX_USB_SSUSB_H_

/*
* copy from musb/musb.h temp. modify later
*/

/* The USB role is defined by the connector used on the board, so long as
 * standards are being followed.  (Developer boards sometimes won't.)
 */
enum musb_mode {
	MUSB_UNDEFINED = 0,
	MUSB_HOST,		/* A or Mini-A connector */
	MUSB_PERIPHERAL,	/* B or Mini-B connector */
	MUSB_OTG		/* Mini-AB connector */
};

/* struct clk; */

enum musb_fifo_style {
	FIFO_RXTX,		/* add MUSB_ prefix to avoid confilicts with musbfsh.h, gang */
	FIFO_TX,
	FIFO_RX
};


enum musb_ep_mode {
	EP_CONT,
	EP_INT,
	EP_BULK,
	EP_ISO
};

struct musb_fifo_cfg {
	u8 hw_ep_num;
	enum musb_fifo_style style;
	u16 maxpacket;
	enum musb_ep_mode ep_mode;
};

#define MUSB_EP_FIFO(ep, st, m, pkt)		\
{						\
	.hw_ep_num	= ep,			\
	.style		= st,			\
	.mode		= m,			\
	.maxpacket	= pkt,			\
}


struct musb_hdrc_config {
	struct musb_fifo_cfg *fifo_cfg;	/* board fifo configuration */
	u32 fifo_cfg_size;	/* size of the fifo configuration */

	/* MUSB configuration-specific details */
	//unsigned multipoint:1;	/* multipoint device */
	//unsigned dyn_fifo:1 __deprecated;	/* supports dynamic fifo sizing */
	//unsigned soft_con:1 __deprecated;	/* soft connect required */
	//unsigned utm_16:1 __deprecated;	/* utm data witdh is 16 bits */
	//unsigned big_endian:1;	/* true if CPU uses big-endian */
	//unsigned mult_bulk_tx:1;	/* Tx ep required for multbulk pkts */
	//unsigned mult_bulk_rx:1;	/* Rx ep required for multbulk pkts */
	//unsigned high_iso_tx:1;	/* Tx ep required for HB iso */
	//unsigned high_iso_rx:1;	/* Rx ep required for HD iso */
	//unsigned dma:1 __deprecated;	/* supports DMA */
	//unsigned vendor_req:1 __deprecated;	/* vendor registers required */

	u32 num_eps;		/* number of endpoints _with_ ep0 */
	//u8 dma_channels __deprecated;	/* number of dma channels */
	u32 dyn_fifo_size;	/* dynamic size in bytes */
	//u8 vendor_ctrl __deprecated;	/* vendor control reg width */
	//u8 vendor_stat __deprecated;	/* vendor status reg witdh */
	//u8 dma_req_chan __deprecated;	/* bitmask for required dma channels */
	u32 ram_bits;		/* ram address size */

};

struct musb_hdrc_platform_data {
	int port_num;
	/* MUSB_HOST, MUSB_PERIPHERAL, or MUSB_OTG */
	int drv_mode;
	int otg_mode;
	int str_mode;
	int otg_init_as_host;  /* init as device or host? */
	int is_u3_otg;    /* is usb3 or usb2? */
	int eint_num;
	int p0_vbus_mode;
	int p0_gpio_num;
	int p0_gpio_active_low;
	int p1_vbus_mode;
	int p1_gpio_num;
	int p1_gpio_active_low;
	int wakeup_src;
#if 0
	/* for clk_get() */
	const char *clock;

	/* (HOST or OTG) switch VBUS on/off */
	int (*set_vbus) (struct device *dev, int is_on);

	/* (HOST or OTG) mA/2 power supplied on (default = 8mA) */
	u8 power;

	/* (PERIPHERAL) mA/2 max power consumed (default = 100mA) */
	u8 min_power;

	/* (HOST or OTG) msec/2 after VBUS on till power good */
	u8 potpgt;

	/* (HOST or OTG) program PHY for external Vbus */
	unsigned extvbus:1;

	/* Power the device on or off */
	int (*set_power) (int state);
#endif
	/* MUSB configuration-specific details */
	struct musb_hdrc_config *config;

	/* Architecture specific board data     */
	void *board_data;

	/* Platform specific struct musb_ops pointer */
	const void *platform_ops;
};


struct ssusb_xhci_pdata {
	int	need_str;
};

extern void mtk_xhci_set(void *xhci);

#endif				/* __LINUX_USB_MUSB_H */
