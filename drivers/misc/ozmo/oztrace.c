/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#include "oztrace.h"
#define CREATE_TRACE_POINTS
#include "ozeventtrace.h"

#define OZ_TRACE_DUMP_SKB_LEN_MAX	(32 * 1)
#define OZ_TRACE_DUMP_SKB_ISOC_LEN_MAX	(16)
#define OZ_TRACE_DUMP_URB_LEN_MAX	 (16 * 1)
#define OZ_TRACE_DUMP_URB_ISOC_LEN_MAX	(16)


/*"OZ S 00000000 00 00 " "XX.. URB_LEN_MAX ...XX"*/
/*"OZ C 00000000 00 00 " "XX.. URB_LEN_MAX ...XX"*/
#define OZ_TRACE_BUF_SZ_URB \
	(21 + (OZ_TRACE_DUMP_URB_LEN_MAX + OZ_TRACE_DUMP_URB_LEN_MAX / 4) * 2)

/*"OZ T 0000 " "XX.. SKB_LEN_MAX ...XX"*/
#define OZ_TRACE_BUF_SZ_SKB \
	(12 + (OZ_TRACE_DUMP_SKB_LEN_MAX + OZ_TRACE_DUMP_SKB_LEN_MAX / 4) * 2)

#define WANT_TRACE_DATA_FLOW

u32 g_debug =
#ifdef WANT_TRACE_DATA_FLOW
	TRC_FLG(M)|TRC_FLG(D);
#else
	0;
#endif

void (*func[]) (char *fmt, va_list arg) = {
	trace_hcd_msg_evt,
	trace_isoc_msg_evt,
	trace_info_msg_evt
};

#define OZ_DBG_SLOT_SIZE	256
#define OZ_DBG_NUM_SLOT		8	/* Must be power of 2 */
#define OZ_DBG_SLOT_INDEX_MASK	(OZ_DBG_NUM_SLOT-1)

#define OZ_DBG_CHECK_SLOT_BUSY

#if (OZ_TRACE_BUF_SZ_URB > OZ_DBG_SLOT_SIZE) || \
	(OZ_TRACE_BUF_SZ_SKB > OZ_DBG_SLOT_SIZE)
#error "!! Invalid DBG_SLOT_SIZE !!"
#endif


static atomic_t g_dbg_count      = ATOMIC_INIT(0);
static atomic_t g_dbg_slot_usage = ATOMIC_INIT(0);
static char oz_dbg_buf[OZ_DBG_NUM_SLOT * OZ_DBG_SLOT_SIZE];


static inline char *oz_dbg_slot_alloc(int len)
{
	unsigned index = (unsigned)(
		atomic_inc_return(&g_dbg_count)) & OZ_DBG_SLOT_INDEX_MASK;

#ifdef OZ_DBG_CHECK_SLOT_BUSY
	if (atomic_read(&g_dbg_slot_usage) & (1<<index)) {
		printk("OZ M slot is busy\n");
		return NULL;
	}
	atomic_add(1 << index, &g_dbg_slot_usage);
#endif
	return &oz_dbg_buf[index * OZ_DBG_SLOT_SIZE];
}

#ifdef OZ_DBG_CHECK_SLOT_BUSY
static inline void oz_dbg_slot_free(char *slot)
{
	if (slot) {
		unsigned index = (unsigned)(
			(((uintptr_t)slot) - (uintptr_t)oz_dbg_buf)
				/ OZ_DBG_SLOT_SIZE) & OZ_DBG_SLOT_INDEX_MASK;

		atomic_sub(1 << index, &g_dbg_slot_usage);
	}
}
#else
#define oz_dbg_slot_free(slot)
#endif

static void oz_dump_data(char *buf, unsigned char *data, int len, int lmt)
{
	int i = 0;
	if (len > lmt)
		len = lmt;
	while (len--) {
		*buf = (*data>>4) + '0';
		if (*data > (0xA0-1))
			*buf += 'A' - '9' - 1;
		*++buf = (*data++&0xF) + '0';
		if (*buf > '9')
			*buf += 'A' - '9' - 1;
		if (buf++ && !(++i%4))
			*buf++ = ' ';
	}
	*buf++ = '\n';
	*buf   = 0;
}

void oz_trace_f_urb_in(struct urb *urb)
{
	int  i = 0;
	char *buf = oz_dbg_slot_alloc(OZ_TRACE_BUF_SZ_URB);
	int endpoint = usb_pipeendpoint(urb->pipe);

	if (buf == NULL)
		return;

	if (usb_pipein(urb->pipe))
		endpoint |= 0x80;


	if (endpoint == 0x00 || endpoint == 0x80) {
		i += sprintf(&buf[i], "OZ S %08X %02X %02X ",
			 (unsigned int)((uintptr_t)urb), endpoint,
			urb->transfer_buffer_length);

		/* Setup packet 8bytes */
		oz_dump_data(&buf[i], urb->setup_packet, 8, 8);

		/* Write data if the direction is out */
		if (endpoint == 0x00) {
			buf[i+17] = ' ';
			oz_dump_data(
				&buf[i+18],
				(u8 *)(urb->transfer_buffer),
				urb->transfer_buffer_length,
				OZ_TRACE_DUMP_URB_LEN_MAX);
		}
	} else {
		int length = urb->transfer_buffer_length;

		if (usb_pipeisoc(urb->pipe)) {
			int n;
			length = 0;
			for (n = 0; n < urb->number_of_packets; n++)
				length += urb->iso_frame_desc[n].length;
		}

		i += sprintf(&buf[i], "OZ S %08X %02X %02X ",
			(unsigned int)((uintptr_t)urb), endpoint,
			length);
		if (!(endpoint & 0x80)) {
			oz_dump_data(
				&buf[i],
				(u8 *)(urb->transfer_buffer),
				length,
#if OZ_TRACE_DUMP_URB_ISOC_LEN_MAX < OZ_TRACE_DUMP_URB_LEN_MAX
				usb_pipeisoc(urb->pipe) ?
					OZ_TRACE_DUMP_URB_ISOC_LEN_MAX :
#endif
					OZ_TRACE_DUMP_URB_LEN_MAX);

		} else {
			oz_dump_data(&buf[i], NULL, 0, 0);
		}
	}
	printk(buf);
	oz_dbg_slot_free(buf);
}

void oz_trace_f_urb_out(struct urb *urb, int status)
{
	int  i = 0;
	char *buf = oz_dbg_slot_alloc(OZ_TRACE_BUF_SZ_URB);
	int endpoint = usb_pipeendpoint(urb->pipe);
	int length = urb->actual_length;

	if (buf == NULL)
		return;

	if (usb_pipein(urb->pipe))
		endpoint |= 0x80;

	if (usb_pipeisoc(urb->pipe)) {
		length = urb->transfer_buffer_length;
		if (endpoint & 0x80)
			length = urb->iso_frame_desc[0].actual_length;
	}
	if (status != 0) {
		printk("OZ E %08X %08X\n",
			(unsigned int)((uintptr_t)urb), status);
	} else {
		i += sprintf(&buf[i], "OZ C %08X %02X %02X ",
			(unsigned int)((uintptr_t)urb),
			endpoint,
			length);

		if (endpoint & 0x80) {
			oz_dump_data(&buf[i],
				(u8 *)(urb->transfer_buffer),
				length,
#if OZ_TRACE_DUMP_URB_ISOC_LEN_MAX < OZ_TRACE_DUMP_URB_LEN_MAX
				usb_pipeisoc(urb->pipe) ?
					OZ_TRACE_DUMP_URB_ISOC_LEN_MAX :
#endif
					OZ_TRACE_DUMP_URB_LEN_MAX);
		} else {
			oz_dump_data(&buf[i], NULL, 0, 0);
		}
		printk(buf);
	}
	oz_dbg_slot_free(buf);
}

void oz_trace_f_skb(struct sk_buff *skb, char dir)
{
	int  i = 0;
	char *buf = oz_dbg_slot_alloc(OZ_TRACE_BUF_SZ_SKB);
	int len = skb->len;

	if (buf == NULL)
		return;

	if (dir == 'T')
		len -= 14;

	i += sprintf(&buf[i], "OZ %c %04X ", dir, len);
	oz_dump_data(
		&buf[i],
		(u8 *)skb_network_header(skb),
		len,
#if OZ_TRACE_DUMP_SKB_ISOC_LEN_MAX < OZ_TRACE_DUMP_SKB_LEN_MAX
		(*(u8 *)skb_network_header(skb)) & 0x20 ?
			OZ_TRACE_DUMP_SKB_ISOC_LEN_MAX :
#endif
			OZ_TRACE_DUMP_SKB_LEN_MAX);
	printk(buf);
	oz_dbg_slot_free(buf);
}

void oz_trace_f_dbg(void)
{
}

void trace_dbg_msg(int c, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	func[c](fmt, arg);
	va_end(arg);
}

void trace_debug_log(char log_type, ...)
{
	va_list arg;
	char *fmt;

	va_start(arg, log_type);
	fmt = va_arg(arg, char *);
	switch (log_type) {
	case 'H':
		trace_hcd_msg_evt(fmt, arg);
		break;
	case 'I':
		trace_isoc_msg_evt(fmt, arg);
		break;
	 default:
		trace_info_msg_evt(fmt, arg);
		break;
	}
	va_end(arg);
}
