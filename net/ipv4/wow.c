#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/if_packet.h>
#include <net/ip.h>
#include <net/inet_hashtables.h>
#include <net/tcp.h>
#include <linux/errno.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/in.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/device.h>

#include <net/wow.h>

static struct wow_sniffer *priv;
static LIST_HEAD(wow_pkt_list);

static struct dentry *wow_pkt_stats_dentry;

#ifdef DUMP_WOW_PACKET
static void dump_skb(struct sk_buff *skb)
{
	pr_warn("skb_info---->\n");
	pr_warn("skb = 0x%08x\n", (unsigned)skb);
	pr_warn("skb->head =0x%p, skb->data = 0x%p, skb->tail = 0x%p, skb->end = 0x%p\n",
		skb->head, skb->data, skb->tail, skb->end);
	pr_warn("skb->truesize = %u skb->len = %u, skb->data_len = %u, skb->mac_len = %u, skb->hdr_len = %u\n",
		skb->truesize, skb->len,
		skb->data_len, skb->mac_len, skb->hdr_len);
	pr_warn("skb->mac_hdr = 0x%p, skb->net_hdr = 0x%p, skb->trans_hdr = 0x%p\n",
		skb->mac_header,
		skb->network_header, skb->transport_header);
	pr_warn("sizeof(iphdr) = %d, sizoof(tcphdr) = %d, actual sizeof(iphdr) = %u\n",
		sizeof(struct iphdr),
		sizeof(struct tcphdr), ip_hdrlen(skb));
	print_hex_dump(KERN_INFO, "dump_skb ", DUMP_PREFIX_OFFSET, 16, 2,
		       skb->data, skb->len, true);

	pr_info("-----------------------------\n");
	print_hex_dump(KERN_INFO, "dump_skb ", DUMP_PREFIX_OFFSET, 16, 4,
		       skb->data, skb->len, true);

}
#else
#define dump_skb(s)	do {} while (0)
#endif

static inline void ip_dot(char *str, __u32 addr)
{
	unsigned char *ip = (unsigned char *)&addr;
	sprintf(str, "%u.%u.%u.%u",
			(unsigned)ip[3], (unsigned)ip[2],
			(unsigned)ip[1], (unsigned)ip[0]);
}

static bool match_wow_pkt(struct wow_pkt_info *l,
				struct wow_pkt_info *r)
{
	if (l->l3_proto == r->l3_proto &&
		l->saddr == r->saddr &&
		l->l4_proto == r->l4_proto &&
		l->sport == r->sport)
		return true;

	return false;
}

static void add_wow_pkt(struct wow_pkt_info *info)
{
	struct wow_pkt_info *w;

	list_for_each_entry_rcu(w, &wow_pkt_list, link) {
		if (match_wow_pkt(w, info)) {
			w->count++;
			kfree(info);
			info = NULL;
			break;
		}
	}

	if (info) {
		info->count++;
		list_add_rcu(&info->link, &wow_pkt_list);
	}
}

static void unwrap_tcp(struct sk_buff *skb, struct iphdr *iph,
			struct wow_pkt_info *info)
{
	struct tcphdr *th;
	__u16 sport, dport;
	size_t hdr_size;
	struct sock *sk;
	struct task_struct *tsk;
	struct fown_struct *io_owner;
	struct file *f;

	hdr_size = sizeof(struct tcphdr);
	if (skb->len < hdr_size) {
		pr_warn("wow: no tcp hdr\n");
		return;
	}

	th = tcp_hdr(skb);
	dump_skb(skb);
	if (th->doff < (hdr_size/4)) {
		pr_warn("wow: bad tcp hdr len\n");
		return;
	}

	if (skb->len < (th->doff * 4)) {
		pr_warn("wow: partial tcp hdr\n");
		return;
	}

	sport = ntohs(th->source);
	dport = ntohs(th->dest);
	info->sport = sport;
	info->dport = dport;

	sk = inet_lookup(dev_net(skb->dev), &tcp_hashinfo, iph->saddr,
			th->source, iph->daddr, th->dest, skb->dev->ifindex);
	if (!sk) {
		pr_warn("wow: tcp no socket for port (%u)\n",
				(unsigned)dport);
		return;
	}

	/* lock all callbacks, so the struct sock * remains unchanged and then
	 * check that we have socket and file struct
	 */
	read_lock_bh(&sk->sk_callback_lock);
	if (sk->sk_socket && sk->sk_socket->file) {
		f = sk->sk_socket->file;
		get_file(f);
		io_owner = &f->f_owner;
		if (io_owner) {
			tsk = pid_task(io_owner->pid, io_owner->pid_type);
			if (tsk) {
				rcu_read_lock();
				task_lock(tsk);
				pr_info("wow: tcp name:tgid:pidtype:tid :port = %s:%d:%d:%d:%u\n",
					tsk->comm,
					tsk->tgid, io_owner->pid_type, tsk->pid,
					(unsigned)dport);
				strcpy(info->procname, tsk->comm);
				task_unlock(tsk);
				rcu_read_unlock();
			}
		}
		fput(f);
	}
	read_unlock_bh(&sk->sk_callback_lock);

	sock_put(sk);
}

static void unwrap_ip(struct sk_buff *skb, struct wow_pkt_info *info)
{
	struct iphdr *iph;
	int len;

	if (unlikely(sizeof(struct iphdr) > skb->len))
		goto out;

	iph = ip_hdr(skb);
	if (unlikely((iph->ihl*4) > skb->len))
		goto out;

	len = ntohs(iph->tot_len);
	if ((skb->len < len) ||
			(len < iph->ihl*4))
		goto out;

	info->saddr = ntohl(iph->saddr);
	info->daddr = ntohl(iph->daddr);

	if (unlikely(ipv4_is_multicast(iph->daddr)))
		goto out;

	if (unlikely(ipv4_is_local_multicast(iph->daddr)))
		goto out;

	if (unlikely(ipv4_is_lbcast(iph->daddr)))
		goto out;

	/* if there are more fragments to come or we are at an offset
	 * just log the source and destination ip and get out
	 */
	if (ip_is_fragment(iph)) {
		info->frag_off = (iph->frag_off &
				htons(IP_MF | IP_OFFSET));
			goto out;
	}

	info->l4_proto = iph->protocol;
	/* move pointers to next proto header */
	skb_pull(skb, ip_hdrlen(skb));
	skb_reset_transport_header(skb);

	switch (iph->protocol) {
		/* tcp */
	case IPPROTO_IP:
	case IPPROTO_TCP:
		unwrap_tcp(skb, iph, info);
		break;
	case IPPROTO_UDP:
		/* placeholder for udp unwrap */
		break;
	};

out:
	return;
}

static void wow_detect_work(struct work_struct *work)
{
	struct wow_sniffer *p =
		container_of(work, struct wow_sniffer, wow_work);
	struct sk_buff *skb;
	struct wow_pkt_info *info;

	/* we come here only when we know we have an ipv4 packet */
	while (skb_queue_len(&p->q)) {

		skb = skb_dequeue(&p->q);
		BUG_ON(!skb);

		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			pr_warn("%s: failed to allocate packet info, skipping packet\n",
				__func__);
			kfree_skb(skb);
			continue;
		}

		info->pkt_type = skb->pkt_type;
		info->l3_proto = ntohs(skb->protocol);

		if (info->l3_proto == ETH_P_IP)
			unwrap_ip(skb, info);

		add_wow_pkt(info);

		/* release skb */
		kfree_skb(skb);
	}

}

/**
 * tag_wow_skb - tag socket buffer with a patter so the sniffer can detect it
 *		 when it gets pushed up the network stack
 *
 * @skb - socket buffer to tag
 *
 * The function uses first 4 bytes of skb->cb[48] and writes them with a
 * pattern. This is hardly unique and can/should be changed in future.
 *
 * Fix Me: @ssp
 */
void tag_wow_skb(struct sk_buff *skb)
{
	struct wow_skb_parm *parm = WOWCB(skb);

	memset(&parm->magic[0], 0xa5, 4);
}
EXPORT_SYMBOL(tag_wow_skb);

static inline bool is_wow_packet(struct sk_buff *skb)
{
	struct wow_skb_parm *parm = WOWCB(skb);
	if (unlikely(parm->magic[0] == 0xa5 &&
		parm->magic[1] == 0xa5 &&
		parm->magic[2] == 0xa5 &&
		parm->magic[3] == 0xa5))
		return true;
	return false;
}

static bool is_wow_resume(void)
{
	ktime_t ts, now;
	wakeup_event_t ev;

	ev = pm_get_resume_ev(&ts);
	if ((ev != WEV_WIFI) && (ev != WEV_WAN))
		return false;

	/* Make sure the resume happened during the last couple
	 * of seconds. This is very dangerous approach but if and when
	 * we do end up tracking additional lpm wakeups, we will
	 * be easily able to ignore them based on the count of wakeup events
	 * seen on Wifi/Wan interrupts.
	 *
	 * Fix Me: find better way to differentiate between *real wakeups* and
	 * wifi lpm wakeup packets
	 */

	now = ns_to_ktime(sched_clock());

	if ((s64)(2*NSEC_PER_SEC) < ktime_to_ns(ktime_sub(now, ts)))
		return false;

	return true;
}

static int wow_rcv(struct sk_buff *skb, struct net_device *dev,
		struct packet_type *pt, struct net_device *orig_dev)
{
	struct wow_sniffer *s = (struct wow_sniffer *)pt->af_packet_priv;

	if (skb->pkt_type == PACKET_OUTGOING)
		goto drop;

	if (!is_wow_packet(skb))
		goto drop;

#ifndef TRACK_LPM
	if (!is_wow_resume())
		goto drop;
#endif
#if 0
	print_hex_dump(KERN_INFO, "wow_rcv ", DUMP_PREFIX_OFFSET, 16, 2,
		       skb->data, skb->len, true);

	pr_info("-----------------------------\n");
	print_hex_dump(KERN_INFO, "wow_rcv ", DUMP_PREFIX_OFFSET, 16, 4,
		       skb->data, skb->len, true);
#endif
	skb_queue_tail(&s->q, skb);
	schedule_work(&s->wow_work);

	return 0;

drop:
	/* Throw the packet out. */
	kfree_skb(skb);

	return 0;
}

struct packet_type wow_packet = {
	.type = cpu_to_be16(ETH_P_ALL),
	.func = wow_rcv,
};

/**
 * print_wow_pkt_stats- Print wow packets per ip address.
 * @m: seq_file to print the statistics into.
 * @w: wow pkt info object.
 */
static int print_wow_pkt_stats(struct seq_file *m,
				     struct wow_pkt_info *w)
{
	char saddr[16] = {0,};
	const char *l3;
	const char *l4;
	int ret;

	ip_dot(saddr, w->saddr);

	switch (w->l3_proto) {
	case ETH_P_IP:
		l3 = "ip";
		break;
	case ETH_P_ARP:
		l3 = "arp";
		break;
	default:
		l3 = "n/a";
		break;
	}

	switch (w->l4_proto) {
		/* tcp */
	case IPPROTO_IP:
	case IPPROTO_TCP:
		l4 = "tcp";
		break;
	case IPPROTO_UDP:
		l4 = "udp";
		break;
	case IPPROTO_ICMP:
		l4 = "icmp";
		break;
	default:
		l4 = "n/a";
		break;
	};

	if (w->procname[0] == '\0')
		strcpy(w->procname, "n/a");

	ret = seq_printf(m, "%-16s\t%u\t%s\t%s\t%u\t%u\t%s\n",
			 saddr, w->count,
			 l3, l4, w->sport, w->dport, w->procname);

	return ret;
}

/**
 * wow_pkt_stats_show - Print wow pkt statistics information.
 * @m: seq_file to print the statistics into.
 */
static int wow_pkt_stats_show(struct seq_file *m, void *unused)
{
	struct wow_pkt_info *w;

	seq_puts(m, "saddr\t\tcount\tl3_proto\tl4_proto\tsrc port\tdst port\tproc-name\n");

	rcu_read_lock();
	list_for_each_entry_rcu(w, &wow_pkt_list, link)
		print_wow_pkt_stats(m, w);
	rcu_read_unlock();

	return 0;
}


static int wow_pkt_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, wow_pkt_stats_show, NULL);
}

static const struct file_operations wow_pkt_stats_fops = {
	.owner = THIS_MODULE,
	.open = wow_pkt_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * FIXME: @ssp
 * Finish removal of the driver
 */
static void __exit wow_exit(void)
{
	dev_remove_pack(&wow_packet);
}

static int __init wow_init(void)
{
	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pr_err("%s: not enough memory!\n", __func__);
		return -ENOMEM;
	}

	skb_queue_head_init(&priv->q);
	INIT_WORK(&priv->wow_work, wow_detect_work);
	wow_packet.af_packet_priv = priv;

	wow_pkt_stats_dentry = debugfs_create_file("wow_events",
			S_IRUGO, NULL, NULL, &wow_pkt_stats_fops);

	dev_add_pack(&wow_packet);

	pr_info("wow_sniffer registered\n");

	return 0;
}

module_init(wow_init);
module_exit(wow_exit);
