/*
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "bsd_glue.h"
#include <linux/file.h>   /* fget(int fd) */

#include <netmap.h>
#include <netmap_kern.h>
#include "netmap_mem2.h"
#include <linux/rtnetlink.h>
#include <linux/nsproxy.h>

#include "netmap_linux_config.h"

#ifdef NETMAP_LINUX_HAVE_IOMMU
#include <linux/iommu.h>

/* #################### IOMMU ################## */
/*
 * Returns the IOMMU domain id that the device belongs to.
 */
int nm_iommu_group_id(struct device *dev)
{
	struct iommu_group *grp;
	int id;

	if (!dev)
		return 0;

	grp = iommu_group_get(dev);
	if (!grp)
		return 0;

	id = iommu_group_id(grp);
	return id;
}
#else /* ! HAVE_IOMMU */
int nm_iommu_group_id(struct device *dev)
{
	return 0;
}
#endif /* HAVE_IOMMU */

/* #################### VALE OFFLOADINGS SUPPORT ################## */

/* Compute and return a raw checksum over (data, len), using 'cur_sum'
 * as initial value. Both 'cur_sum' and the return value are in host
 * byte order.
 */
rawsum_t nm_csum_raw(uint8_t *data, size_t len, rawsum_t cur_sum)
{
	return csum_partial(data, len, cur_sum);
}

/* Compute an IPv4 header checksum, where 'data' points to the IPv4 header,
 * and 'len' is the IPv4 header length. Return value is in network byte
 * order.
 */
uint16_t nm_csum_ipv4(struct nm_iphdr *iph)
{
	return ip_compute_csum((void*)iph, sizeof(struct nm_iphdr));
}

/* Compute and insert a TCP/UDP checksum over IPv4: 'iph' points to the IPv4
 * header, 'data' points to the TCP/UDP header, 'datalen' is the lenght of
 * TCP/UDP header + payload.
 */
void nm_csum_tcpudp_ipv4(struct nm_iphdr *iph, void *data,
		      size_t datalen, uint16_t *check)
{
	*check = csum_tcpudp_magic(iph->saddr, iph->daddr,
				datalen, iph->protocol,
				csum_partial(data, datalen, 0));
}

/* Compute and insert a TCP/UDP checksum over IPv6: 'ip6h' points to the IPv6
 * header, 'data' points to the TCP/UDP header, 'datalen' is the lenght of
 * TCP/UDP header + payload.
 */
void nm_csum_tcpudp_ipv6(struct nm_ipv6hdr *ip6h, void *data,
		      size_t datalen, uint16_t *check)
{
	*check = csum_ipv6_magic((void *)&ip6h->saddr, (void*)&ip6h->daddr,
				datalen, ip6h->nexthdr,
				csum_partial(data, datalen, 0));
}

uint16_t nm_csum_fold(rawsum_t cur_sum)
{
	return csum_fold(cur_sum);
}


#ifdef WITH_GENERIC
/* ####################### MITIGATION SUPPORT ###################### */

/*
 * The generic driver calls netmap once per received packet.
 * This is inefficient so we implement a mitigation mechanism,
 * as follows:
 * - the first packet on an idle receiver triggers a notification
 *   and starts a timer;
 * - subsequent incoming packets do not cause a notification
 *   until the timer expires;
 * - when the timer expires and there are pending packets,
 *   a notification is sent up and the timer is restarted.
 */
NETMAP_LINUX_TIMER_RTYPE
generic_timer_handler(struct hrtimer *t)
{
    struct nm_generic_mit *mit =
	container_of(t, struct nm_generic_mit, mit_timer);
    u_int work_done;

    if (!mit->mit_pending) {
        return HRTIMER_NORESTART;
    }

    /* Some work arrived while the timer was counting down:
     * Reset the pending work flag, restart the timer and send
     * a notification.
     */
    mit->mit_pending = 0;
    /* below is a variation of netmap_generic_irq  XXX revise */
    if (nm_netmap_on(mit->mit_na)) {
        netmap_common_irq(mit->mit_na->ifp, mit->mit_ring_idx, &work_done);
        generic_rate(0, 0, 0, 0, 0, 1);
    }
    netmap_mitigation_restart(mit);

    return HRTIMER_RESTART;
}


void netmap_mitigation_init(struct nm_generic_mit *mit, int idx,
                                struct netmap_adapter *na)
{
    hrtimer_init(&mit->mit_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    mit->mit_timer.function = &generic_timer_handler;
    mit->mit_pending = 0;
    mit->mit_ring_idx = idx;
    mit->mit_na = na;
}


void netmap_mitigation_start(struct nm_generic_mit *mit)
{
    hrtimer_start(&mit->mit_timer, ktime_set(0, netmap_generic_mit), HRTIMER_MODE_REL);
}

void netmap_mitigation_restart(struct nm_generic_mit *mit)
{
    hrtimer_forward_now(&mit->mit_timer, ktime_set(0, netmap_generic_mit));
}

int netmap_mitigation_active(struct nm_generic_mit *mit)
{
    return hrtimer_active(&mit->mit_timer);
}

void netmap_mitigation_cleanup(struct nm_generic_mit *mit)
{
    hrtimer_cancel(&mit->mit_timer);
}



/* #################### GENERIC ADAPTER SUPPORT ################### */

/*
 * This handler is registered within the attached net_device
 * in the Linux RX subsystem, so that every mbuf passed up by
 * the driver can be stolen to the network stack.
 * Stolen packets are put in a queue where the
 * generic_netmap_rxsync() callback can extract them.
 * Packets that comes from netmap_txsync_to_host() are not
 * stolen.
 */
#ifdef NETMAP_LINUX_HAVE_RX_REGISTER
#ifdef NETMAP_LINUX_HAVE_RX_HANDLER_RESULT
static rx_handler_result_t linux_generic_rx_handler(struct mbuf **pm)
{
    /* If we were called by NM_SEND_UP(), we want to pass the mbuf
       to network stack. We detect this situation looking at the
       priority field. */
    if ((*pm)->priority == NM_MAGIC_PRIORITY_RX)
            return RX_HANDLER_PASS;

    /* When we intercept a sk_buff coming from the driver, it happens that
       skb->data points to the IP header, e.g. the ethernet header has
       already been pulled. Since we want the netmap rings to contain the
       full ethernet header, we push it back, so that the RX ring reader
       can see it. */
    skb_push(*pm, 14);

    /* Steal the mbuf and notify the pollers for a new RX packet. */
    generic_rx_handler((*pm)->dev, *pm);

    return RX_HANDLER_CONSUMED;
}
#else /* ! HAVE_RX_HANDLER_RESULT */
static struct sk_buff *linux_generic_rx_handler(struct mbuf *m)
{
	generic_rx_handler(m->dev, m);
	return NULL;
}
#endif /* HAVE_RX_HANDLER_RESULT */
#endif /* HAVE_RX_REGISTER */

/* Ask the Linux RX subsystem to intercept (or stop intercepting)
 * the packets incoming from the interface attached to 'na'.
 */
int
netmap_catch_rx(struct netmap_adapter *na, int intercept)
{
#ifndef NETMAP_LINUX_HAVE_RX_REGISTER
    return 0;
#else /* HAVE_RX_REGISTER */
    struct ifnet *ifp = na->ifp;

    if (intercept) {
        return -netdev_rx_handler_register(na->ifp,
                &linux_generic_rx_handler, na);
    } else {
        netdev_rx_handler_unregister(ifp);
        return 0;
    }
#endif /* HAVE_RX_REGISTER */
}

#ifdef NETMAP_LINUX_SELECT_QUEUE
u16 generic_ndo_select_queue(struct ifnet *ifp, struct mbuf *m
#if NETMAP_LINUX_SELECT_QUEUE >= 3
                                , void *accel_priv
#if NETMAP_LINUX_SELECT_QUEUE >= 4
				, select_queue_fallback_t fallback
#endif /* >= 4 */
#endif /* >= 3 */
		)
{
    return skb_get_queue_mapping(m); // actually 0 on 2.6.23 and before
}
#endif /* SELECT_QUEUE */

/* Replacement for the driver ndo_start_xmit() method.
 * When this function is invoked because of the dev_queue_xmit() call
 * in generic_xmit_frame() (e.g. because of a txsync on the NIC), we have
 * to call the original ndo_start_xmit() method.
 * In all the other cases (e.g. when the TX request comes from the network
 * stack) we intercept the packet and put it into the RX ring associated
 * to the host stack.
 */
static netdev_tx_t
generic_ndo_start_xmit(struct mbuf *m, struct ifnet *ifp)
{
    struct netmap_generic_adapter *gna =
                        (struct netmap_generic_adapter *)NA(ifp);

    if (likely(m->priority == NM_MAGIC_PRIORITY_TX))
        return gna->save_start_xmit(m, ifp); /* To the driver. */

    /* To a netmap RX ring. */
    return linux_netmap_start_xmit(m, ifp);
}

/* Must be called under rtnl. */
void netmap_catch_tx(struct netmap_generic_adapter *gna, int enable)
{
    struct netmap_adapter *na = &gna->up.up;
    struct ifnet *ifp = na->ifp;

    if (enable) {
        /*
         * Save the old pointer to the netdev_ops,
         * create an updated netdev ops replacing the
         * ndo_select_queue() and ndo_start_xmit() methods
         * with our custom ones, and make the driver use it.
         */
        na->if_transmit = (void *)ifp->netdev_ops;
        /* Save a redundant copy of ndo_start_xmit(). */
        gna->save_start_xmit = ifp->netdev_ops->ndo_start_xmit;

        gna->generic_ndo = *ifp->netdev_ops;  /* Copy all */
        gna->generic_ndo.ndo_start_xmit = &generic_ndo_start_xmit;
#ifndef NETMAP_LINUX_SELECT_QUEUE
	printk("%s: no packet steering support\n", __FUNCTION__);
#else
        gna->generic_ndo.ndo_select_queue = &generic_ndo_select_queue;
#endif

        ifp->netdev_ops = &gna->generic_ndo;
    } else {
	/* Restore the original netdev_ops. */
        ifp->netdev_ops = (void *)na->if_transmit;
    }
}

/* Transmit routine used by generic_netmap_txsync(). Returns 0 on success
   and -1 on error (which may be packet drops or other errors). */
int generic_xmit_frame(struct ifnet *ifp, struct mbuf *m,
	void *addr, u_int len, u_int ring_nr)
{
    netdev_tx_t ret;

    /* Empty the sk_buff. */
    if (unlikely(skb_headroom(m)))
	skb_push(m, skb_headroom(m));
    skb_trim(m, 0);

    /* TODO Support the slot flags (NS_MOREFRAG, NS_INDIRECT). */
    skb_copy_to_linear_data(m, addr, len); // skb_store_bits(m, 0, addr, len);
    skb_put(m, len);
    NM_ATOMIC_INC(&m->users);
    m->dev = ifp;
    /* Tell generic_ndo_start_xmit() to pass this mbuf to the driver. */
    m->priority = NM_MAGIC_PRIORITY_TX;
    skb_set_queue_mapping(m, ring_nr);

    ret = dev_queue_xmit(m);

    if (likely(ret == NET_XMIT_SUCCESS)) {
        return 0;
    }
    if (unlikely(ret != NET_XMIT_DROP)) {
        /* If something goes wrong in the TX path, there is nothing
           intelligent we can do (for now) apart from error reporting. */
        RD(5, "dev_queue_xmit failed: HARD ERROR %d", ret);
    }
    return -1;
}
#endif /* WITH_GENERIC */

/* Use ethtool to find the current NIC rings lengths, so that the netmap
   rings can have the same lengths. */
int
generic_find_num_desc(struct ifnet *ifp, unsigned int *tx, unsigned int *rx)
{
    int error = EOPNOTSUPP;
#ifdef NETMAP_LINUX_HAVE_GET_RINGPARAM
    struct ethtool_ringparam rp;

    if (ifp->ethtool_ops && ifp->ethtool_ops->get_ringparam) {
        ifp->ethtool_ops->get_ringparam(ifp, &rp);
        *tx = rp.tx_pending;
        *rx = rp.rx_pending;
	error = 0;
    }
#endif /* HAVE_GET_RINGPARAM */
    return error;
}

/* Fills in the output arguments with the number of hardware TX/RX queues. */
void
generic_find_num_queues(struct ifnet *ifp, u_int *txq, u_int *rxq)
{
#ifdef NETMAP_LINUX_HAVE_NUM_QUEUES
    *txq = ifp->real_num_tx_queues;
    *rxq = ifp->real_num_rx_queues;
#else
    *txq = 1;
    *rxq = 1; /* TODO ifp->real_num_rx_queues */
#endif /* HAVE_NUM_QUEUES */
}

int
netmap_linux_config(struct netmap_adapter *na,
		u_int *txr, u_int *txd, u_int *rxr, u_int *rxd)
{
	struct ifnet *ifp = na->ifp;
	int error = 0;

	rtnl_lock();

	if (ifp == NULL) {
		D("zombie adapter");
		error = ENXIO;
		goto out;
	}
	error = generic_find_num_desc(ifp, txd, rxd);
	if (error)
		goto out;
	generic_find_num_queues(ifp, txr, rxr);

out:
	rtnl_unlock();

	return error;
}


/* ######################## FILE OPERATIONS ####################### */

struct net_device *
ifunit_ref(const char *name)
{
#ifndef NETMAP_LINUX_HAVE_INIT_NET
	return dev_get_by_name(name);
#else
	void *ns = &init_net;
#ifdef CONFIG_NET_NS
	ns = current->nsproxy->net_ns;
#endif
	return dev_get_by_name(ns, name);
#endif
}

void if_rele(struct net_device *ifp)
{
	dev_put(ifp);
}



/*
 * Remap linux arguments into the FreeBSD call.
 * - pwait is the poll table, passed as 'dev';
 *   If pwait == NULL someone else already woke up before. We can report
 *   events but they are filtered upstream.
 *   If pwait != NULL, then pwait->key contains the list of events.
 * - events is computed from pwait as above.
 * - file is passed as 'td';
 */
static u_int
linux_netmap_poll(struct file * file, struct poll_table_struct *pwait)
{
#ifdef NETMAP_LINUX_PWAIT_KEY
	int events = pwait ? pwait->NETMAP_LINUX_PWAIT_KEY : \
		     POLLIN | POLLOUT | POLLERR;
#else
	int events = POLLIN | POLLOUT; /* XXX maybe... */
#endif /* PWAIT_KEY */
	return netmap_poll((void *)pwait, events, (void *)file);
}

int
linux_netmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct netmap_priv_d *priv = vma->vm_private_data;
	struct netmap_adapter *na = priv->np_na;
	struct page *page;
	unsigned long off = (vma->vm_pgoff + vmf->pgoff) << PAGE_SHIFT;
	unsigned long pa, pfn;

	pa = netmap_mem_ofstophys(na->nm_mem, off);
	ND("fault off %lx -> phys addr %lx", off, pa);
	if (pa == 0)
		return VM_FAULT_SIGBUS;
	pfn = pa >> PAGE_SHIFT;
	if (!pfn_valid(pfn))
		return VM_FAULT_SIGBUS;
	page = pfn_to_page(pfn);
	get_page(page);
	vmf->page = page;
	return 0;
}

static struct vm_operations_struct linux_netmap_mmap_ops = {
	.fault = linux_netmap_fault,
};

static int
linux_netmap_mmap(struct file *f, struct vm_area_struct *vma)
{
	int error = 0;
	unsigned long off;
	u_int memsize, memflags;

	struct netmap_priv_d *priv = f->private_data;
	struct netmap_adapter *na = priv->np_na;
	/*
	 * vma->vm_start: start of mapping user address space
	 * vma->vm_end: end of the mapping user address space
	 * vma->vm_pfoff: offset of first page in the device
	 */

	if (priv->np_nifp == NULL) {
		return -EINVAL;
	}
	mb();

	/* check that [off, off + vsize) is within our memory */
	error = netmap_mem_get_info(na->nm_mem, &memsize, &memflags, NULL);
	ND("get_info returned %d", error);
	if (error)
		return -error;
	off = vma->vm_pgoff << PAGE_SHIFT;
	ND("off %lx size %lx memsize %x", off,
			(vma->vm_end - vma->vm_start), memsize);
	if (off + (vma->vm_end - vma->vm_start) > memsize)
		return -EINVAL;
	if (memflags & NETMAP_MEM_IO) {
		vm_ooffset_t pa;

		/* the underlying memory is contiguous */
		pa = netmap_mem_ofstophys(na->nm_mem, 0);
		if (pa == 0)
			return -EINVAL;
		return remap_pfn_range(vma, vma->vm_start, 
				pa >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot);
	} else {
		/* non contiguous memory, we serve 
		 * page faults as they come
		 */
		vma->vm_private_data = priv;
		vma->vm_ops = &linux_netmap_mmap_ops;
	}
	return 0;
}


/*
 * This one is probably already protected by the netif lock XXX
 */
netdev_tx_t
linux_netmap_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	netmap_transmit(dev, skb);
	return (NETDEV_TX_OK);
}

/* while in netmap mode, we cannot tolerate any change in the
 * number of rx/tx rings and descriptors
 */
int
linux_netmap_set_ringparam(struct net_device *dev,
	struct ethtool_ringparam *e)
{
	return -EBUSY;
}

#ifdef NETMAP_LINUX_HAVE_SET_CHANNELS
int
linux_netmap_set_channels(struct net_device *dev,
	struct ethtool_channels *e)
{
	return -EBUSY;
}
#endif


#ifndef NETMAP_LINUX_HAVE_UNLOCKED_IOCTL
#define LIN_IOCTL_NAME	.ioctl
int
linux_netmap_ioctl(struct inode *inode, struct file *file, u_int cmd, u_long data /* arg */)
#else
#define LIN_IOCTL_NAME	.unlocked_ioctl
long
linux_netmap_ioctl(struct file *file, u_int cmd, u_long data /* arg */)
#endif
{
	int ret = 0;
	union {
		struct nm_ifreq ifr;
		struct nmreq nmr;
	} arg;
	size_t argsize = 0;

	switch (cmd) {
	case NIOCTXSYNC:
	case NIOCRXSYNC:
		break;
	case NIOCCONFIG:
		argsize = sizeof(arg.ifr);
		break;
	default:
		argsize = sizeof(arg.nmr);
		break;
	}
	if (argsize) {
		if (!data)
			return -EINVAL;
		bzero(&arg, argsize);
		if (copy_from_user(&arg, (void *)data, argsize) != 0)
			return -EFAULT;
	}
	ret = netmap_ioctl(NULL, cmd, (caddr_t)&arg, 0, (void *)file);
	if (data && copy_to_user((void*)data, &arg, argsize) != 0)
		return -EFAULT;
	return -ret;
}


static int
linux_netmap_release(struct inode *inode, struct file *file)
{
	(void)inode;	/* UNUSED */
	if (file->private_data)
		netmap_dtor(file->private_data);
	return (0);
}


static int
linux_netmap_open(struct inode *inode, struct file *file)
{
	struct netmap_priv_d *priv;
	(void)inode;	/* UNUSED */

	priv = malloc(sizeof(struct netmap_priv_d), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return -ENOMEM;

	file->private_data = priv;

	return (0);
}


static struct file_operations netmap_fops = {
    .owner = THIS_MODULE,
    .open = linux_netmap_open,
    .mmap = linux_netmap_mmap,
    LIN_IOCTL_NAME = linux_netmap_ioctl,
    .poll = linux_netmap_poll,
    .release = linux_netmap_release,
};



#ifdef WITH_V1000
/* ##################### V1000 BACKEND SUPPORT ##################### */

/* Private info stored into the memory area pointed by
   netmap_adapter.na_private field. */
struct netmap_backend {
    /* The netmap adapter connected to the v1000 backend. */
    struct netmap_adapter *na;
    /* The file struct attached to the unique priv_structure
       attached to *na. */
    struct file *file;
    /* Pointer to the task which owns this v1000 backend, and
       so the adapter. */
    struct task_struct *owner;
    /* Pointers to callbacks (in *na) that are overridden by
       the v1000 backend. */
    void (*saved_nm_dtor)(struct netmap_adapter *);
    int (*saved_nm_notify)(struct netmap_adapter *, u_int ring,
                           enum txrx, int flags);
};

/* Callback that overrides na->nm_dtor. */
static void netmap_backend_nm_dtor(struct netmap_adapter *na)
{
        struct netmap_backend *be = na->na_private;

        if (be) {
                /* Restore the netmap adapter callbacks
                   overridden by the backend. */
                na->nm_dtor = be->saved_nm_dtor;
                na->nm_notify = be->saved_nm_notify;
                /* Free the backend memory. */
                kfree(be);
                na->na_private = NULL;
                D("v1000 backend support removed for %p", na);

        }

        /* Call the original destructor, if any. */
        if (na->nm_dtor)
            na->nm_dtor(na);
}

/* Callback that overrides na->nm_notify. */
static int netmap_backend_nm_notify(struct netmap_adapter *na,
				u_int n_ring, enum txrx tx, int flags)
{
	struct netmap_kring *kring;

	ND("called");
	if (tx == NR_TX) {
		kring = na->tx_rings + n_ring;
		wake_up_interruptible_poll(&kring->si, POLLIN |
					POLLRDNORM | POLLRDBAND);
		if (na->tx_si_users > 0)
			wake_up_interruptible_poll(&na->tx_si, POLLIN |
					POLLRDNORM | POLLRDBAND);
	} else {
		kring = na->rx_rings + n_ring;
		wake_up_interruptible_poll(&kring->si, POLLIN |
					POLLRDNORM | POLLRDBAND);
		if (na->rx_si_users > 0)
			wake_up_interruptible_poll(&na->rx_si, POLLIN |
					POLLRDNORM | POLLRDBAND);
	}

	return 0;
}

/* Called by an external module (the v1000 frontend) which wants to
   attach to the netmap file descriptor fd. Setup the backend (if
   necessary) and return a pointer to the backend private structure,
   which can be passed back to the backend exposed interface.
   If successful, the caller holds a reference to the file struct
   associated to 'fd'.
*/
void *netmap_get_backend(int fd)
{
	struct file *filp = fget(fd); /* fd --> file */
	struct netmap_priv_d *priv;
	struct netmap_adapter *na;
        struct netmap_backend *be;
        int error = 0;

	if (!filp)
            return ERR_PTR(-EBADF);

	if (filp->f_op != &netmap_fops) {
            error = -EINVAL;
            goto err;
        }

        /* file --> netmap priv */
	priv = (struct netmap_priv_d *)filp->private_data;
	if (!priv) {
            error = -EBADF;
            goto err;
        }

	NMG_LOCK();
	na = priv->np_na; /* netmap priv --> netmap adapter */
	if (na == NULL) {
            error = -EBADF;
            goto lock_err;
	}

        be = (struct netmap_backend *)(na->na_private);

        /* Allow request if the netmap adapter is not already used by
           the kernel or the request comes from the owner. */
        if (NETMAP_OWNED_BY_KERN(na) && (!be || be->owner != current)) {
                error = -EBUSY;
                goto lock_err;
        }

        if (!be) {
                /* Setup the backend. */
                be = na->na_private = malloc(sizeof(struct netmap_backend),
                                            M_DEVBUF, M_MOWAIT | M_ZERO);
                if (!be) {
                        error = -ENOMEM;
                        goto lock_err;
                }
                be->na = na;
                be->file = filp;
                be->owner = current;  /* set the owner */

                /* Override some callbacks. */
                be->saved_nm_dtor = na->nm_dtor;
                be->saved_nm_notify = na->nm_notify;
                na->nm_dtor = &netmap_backend_nm_dtor;
                na->nm_notify = &netmap_backend_nm_notify;

                D("v1000 backend support created for %p", na);
        }
        NMG_UNLOCK();

        return be;

lock_err:
	NMG_UNLOCK();
err:
        fput(filp);
        return ERR_PTR(error);
}
EXPORT_SYMBOL(netmap_get_backend);

struct file* netmap_backend_get_file(void *opaque)
{
    struct netmap_backend *be = opaque;

    return be->file;
}
EXPORT_SYMBOL(netmap_backend_get_file);

static int netmap_common_sendmsg(struct netmap_adapter *na, struct msghdr *m,
                          size_t len, unsigned flags)
{
    struct netmap_ring *ring;
    struct netmap_kring *kring;
    unsigned i, last;
    unsigned avail;
    unsigned j;
    unsigned nm_buf_size;
    struct iovec *iov = m->msg_iov;
    size_t iovcnt = m->msg_iovlen;

    ND("message_len %d, %p", (int)len, na_sock);

    if (unlikely(na == NULL)) {
        RD(5, "Null netmap adapter");
        return len;
    }

    /* Grab the netmap ring normally used from userspace. */
    kring = &na->tx_rings[0];
    ring = kring->ring;
    nm_buf_size = ring->nr_buf_size;

    i = last = ring->cur;
    avail = ring->tail + ring->num_slots - ring->cur;
    if (avail >= ring->num_slots)
	avail -= ring->num_slots;

    ND("A) cur=%d avail=%d, hwcur=%d, hwtail=%d\n",
	i, avail, na->tx_rings[0].nr_hwcur, na->tx_rings[0].nr_hwtail);
    if (avail < iovcnt) {
        /* Not enough netmap slots. */
        return 0;
    }

    for (j=0; j<iovcnt; j++) {
        uint8_t *iov_frag = iov[j].iov_base;
        unsigned iov_frag_size = iov[j].iov_len;
        unsigned offset = 0;
#if 0
        unsigned k = 0;
        uint8_t ch;

        printk("len=%d: ", iov_frag_size);
        for (k=0; k<iov_frag_size && k<36; k++) {
            if(copy_from_user(&ch, iov_frag + k, 1)) {
                D("failed");
            }
            printk("%02x:", ch);
        }printk("\n");
#endif
        while (iov_frag_size) {
            unsigned nm_frag_size = min(iov_frag_size, nm_buf_size);
            uint8_t *dst;

            if (unlikely(avail == 0)) {
                return 0;
            }

            dst = NMB(na, &ring->slot[i]);

            ring->slot[i].len = nm_frag_size;
            ring->slot[i].flags = NS_MOREFRAG;
            if (copy_from_user(dst, iov_frag + offset, nm_frag_size)) {
                D("copy_from_user() error");
            }

            last = i;
            if (unlikely(++i == ring->num_slots))
                i = 0;
            avail--;

            offset += nm_frag_size;
            iov_frag_size -= nm_frag_size;
        }
    }

    ring->slot[last].flags &= ~NS_MOREFRAG;

    ring->cur = i;

    if (!(flags & MSG_MORE))
        kring->nm_sync(kring, 0);
    ND("B) cur=%d avail=%d, hwcur=%d, hwtail=%d\n",
	i, avail, na->tx_rings[0].nr_hwcur, na->tx_rings[0].nr_hwtail);

    return len;
}

int netmap_backend_sendmsg(void *opaque, struct msghdr *m, size_t len, unsigned flags)
{
    struct netmap_backend *be = opaque;

    return netmap_common_sendmsg(be->na, m, len, flags);
}
EXPORT_SYMBOL(netmap_backend_sendmsg);

static inline int netmap_common_peek_head_len(struct netmap_adapter *na)
{
        /* Here we assume to have a virtual port. */
        struct netmap_vp_adapter *vpna = (struct netmap_vp_adapter *)na;
	struct netmap_kring *kring = &na->rx_rings[0];
        struct netmap_ring *ring = kring->ring;
	u_int i;
	int ret = 0;

        /* Do the rxsync here. The recvmsg() callback must be
           called after the peek_head_len() callback. */
        if (nm_ring_empty(ring))
            kring->nm_sync(kring, NAF_FORCE_READ);

        i = ring->cur;
	if (!nm_ring_empty(ring)) {
		for(;;) {
			ret += ring->slot[i].len;
			if (!(ring->slot[i].flags & NS_MOREFRAG))
				break;
			if (unlikely(++i == ring->num_slots))
				i = 0;
		}
	}
        ND("peek %d, %d iovecs cur=%d tail=%d, hwcur=%d, hwtail=%d\n",
            ret, i + 1 - ring->cur,
            ring->cur, ring->tail, be->na->rx_rings[0].nr_hwcur,
            be->na->rx_rings[0].nr_hwtail);

        /* The v1000 frontend assumes that the peek_head_len() callback
           doesn't count the bytes of the virtio-net-header. */
        if (likely(ret >= vpna->virt_hdr_len)) {
            ret -= vpna->virt_hdr_len;
        }

	return ret;
}

int netmap_backend_peek_head_len(void *opaque)
{
        struct netmap_backend *be = opaque;

        return netmap_common_peek_head_len(be->na);
}
EXPORT_SYMBOL(netmap_backend_peek_head_len);

static int netmap_common_recvmsg(struct netmap_adapter *na,
                                 struct msghdr *m, size_t len)
{
	struct netmap_ring *ring;
	/* netmap variables */
	unsigned i, avail;
	bool morefrag;
	unsigned nm_frag_size;
	unsigned nm_frag_ofs;
	uint8_t *src;
	/* iovec variables */
	unsigned j;
	struct iovec *iov = m->msg_iov;
	size_t iovcnt = m->msg_iovlen;
	uint8_t *dst;
	unsigned iov_frag_size;
	unsigned iov_frag_ofs;
	/* counters */
	unsigned copy_size;
	unsigned copied;

	/* The caller asks for 'len' bytes. */
	ND("recvmsg %d, %p", (int)len, na);

	if (unlikely(na == NULL)) {
		RD(5, "Null netmap adapter");
		return len;
	}

	/* Total bytes actually copied. */
	copied = 0;

	/* Grab the netmap RX ring normally used from userspace. */
	ring = na->rx_rings[0].ring;
	i = ring->cur;

	avail = ring->tail + ring->num_slots - ring->cur;
	if (avail >= ring->num_slots)
	    avail -= ring->num_slots;

        ND("A) cur=%d avail=%d, hwcur=%d, hwtail=%d\n",
	    i, avail, na->rx_rings[0].nr_hwcur, na->rx_rings[0].nr_hwtail);

	/* Index into the input iovec[]. */
	j = 0;

	/* Spurious call: Do nothing. */
	if (unlikely(avail == 0))
		return 0;

	/* init netmap variables */
	morefrag = (ring->slot[i].flags & NS_MOREFRAG);
	nm_frag_ofs = 0;
	nm_frag_size = ring->slot[i].len;
	src = NMB(na, &ring->slot[i]);
        if (unlikely(++i == ring->num_slots))
            i = 0;
        avail--;

	/* init iovec variables */
	iov_frag_ofs = 0;
	iov_frag_size = iov[j].iov_len;
	dst = iov[j].iov_base;
        j++;

	/* Copy from the netmap scatter-gather to the caller
	 * scatter-gather.
	 */
	while (copied < len) {
		copy_size = min(nm_frag_size, iov_frag_size);
		if (unlikely(copy_to_user(dst + iov_frag_ofs,
				src + nm_frag_ofs, copy_size))) {
			RD(5, "copy_to_user() failed");
		}
		nm_frag_ofs += copy_size;
		nm_frag_size -= copy_size;
		iov_frag_ofs += copy_size;
		iov_frag_size -= copy_size;
		copied += copy_size;
		if (nm_frag_size == 0) {
			/* Netmap slot exhausted. If this was the
			 * last slot, or no more slots ar available,
			 * we've done.
			 */
			if (!morefrag || !avail)
				break;
			morefrag = (ring->slot[i].flags & NS_MOREFRAG);
			nm_frag_ofs = 0;
			nm_frag_size = ring->slot[i].len;
			src = NMB(na, &ring->slot[i]);
			/* Take the next slot. */
                        if (unlikely(++i == ring->num_slots))
                            i = 0;
			avail--;
		}
		if (iov_frag_size == 0) {
			/* The current iovec fragment is exhausted.
			 * Since we enter here, there must be more
			 * to read from the netmap slots (otherwise
			 * we would have exited the loop in the
			 * above branch).
			 * If this was the last fragment, it means
			 * that there is not enough space in the input
			 * iovec[].
			 */
			if (unlikely(j >= iovcnt)) {
				break;
			}
			/* Take the next iovec fragment. */
			iov_frag_ofs = 0;
			iov_frag_size = iov[j].iov_len;
			dst = iov[j].iov_base;
			j++;
		}
	}

	if (unlikely(!avail && morefrag)) {
		RD(5, "Error: ran out of slots, with a pending"
				"incomplete packet\n");
	}

	ring->head = ring->cur = i;

	ND("read %d bytes using %d iovecs", copied, j);
        ND("B) cur=%d avail=%d, hwcur=%d, hwtail=%d\n",
	    i, avail, na->rx_rings[0].nr_hwcur, na->rx_rings[0].nr_hwtail);

	return copied;
}

int netmap_backend_recvmsg(void *opaque, struct msghdr *m, size_t len)
{
    struct netmap_backend *be = opaque;

    return netmap_common_recvmsg(be->na, m, len);
}
EXPORT_SYMBOL(netmap_backend_recvmsg);



/* ######################## SOCKET SUPPORT ######################### */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
struct netmap_sock {
	struct sock sk;
	struct socket sock;
	struct socket_wq wq;
	void (*saved_nm_dtor)(struct netmap_adapter *);
	int (*saved_nm_notify)(struct netmap_adapter *, u_int ring,
				enum txrx, int flags);
	void *owner;
	struct sk_buff *fake_skb;
	struct netmap_adapter *na;
};

static struct proto netmap_socket_proto = {
        .name = "netmap",
        .owner = THIS_MODULE,
        .obj_size = sizeof(struct netmap_sock),
};

static int netmap_socket_sendmsg(struct kiocb *iocb, struct socket *sock,
                                 struct msghdr *m, size_t total_len);
static int netmap_socket_recvmsg(struct kiocb *iocb, struct socket *sock,
		struct msghdr *m, size_t total_len, int flags);

static struct proto_ops netmap_socket_ops = {
        .sendmsg = netmap_socket_sendmsg,
        .recvmsg = netmap_socket_recvmsg,
};

static void netmap_sock_write_space(struct sock *sk)
{
    wait_queue_head_t *wqueue;

    if (!sock_writeable(sk) ||
        !test_and_clear_bit(SOCK_ASYNC_NOSPACE, &sk->sk_socket->flags)) {
            return;
    }

    wqueue = sk_sleep(sk);
    if (wqueue && waitqueue_active(wqueue)) {
        wake_up_interruptible_poll(wqueue, POLLOUT | POLLWRNORM | POLLWRBAND);
    }
}

static void netmap_sock_teardown(struct netmap_adapter *na)
{
	struct netmap_sock *nm_sock = na->na_private;

    if (nm_sock) {
	/* Restore the saved destructor. */
	na->nm_dtor = nm_sock->saved_nm_dtor;
	na->nm_notify = nm_sock->saved_nm_notify;

	kfree_skb(nm_sock->fake_skb);

        sock_put(&nm_sock->sk);
        /* XXX What?
           kfree(nm_sock);
           sk_release_kernel(&nm_sock->sk);
           */
        sk_free(&nm_sock->sk);
        na->na_private = NULL;
        D("socket support freed for (%p)", na);
    }
}

static void netmap_socket_nm_dtor(struct netmap_adapter *na)
{
	netmap_sock_teardown(na);
	/* Call the saved destructor, if any. */
	if (na->nm_dtor)
		na->nm_dtor(na);
}

static int netmap_socket_nm_notify(struct netmap_adapter *na,
				u_int n_ring, enum txrx tx, int flags)
{
	struct netmap_kring *kring;
	struct netmap_sock *nm_sock;

	D("called");
	nm_sock = (struct netmap_sock *)(na->na_private);
	if (likely(nm_sock)) {
		struct sk_buff_head* q = &nm_sock->sk.sk_receive_queue;
		unsigned long f;

		spin_lock_irqsave(&q->lock, f);
		if (!skb_queue_len(q)) {
			nm_sock->fake_skb->len = netmap_common_peek_head_len(na);
			D("peek %d", nm_sock->fake_skb->len);
			if (nm_sock->fake_skb->len)
				__skb_queue_tail(q, nm_sock->fake_skb);
		}
		spin_unlock_irqrestore(&q->lock, f);
	}

	if (tx == NR_TX) {
		kring = na->tx_rings + n_ring;
		wake_up_interruptible_poll(&kring->si, POLLIN |
					POLLRDNORM | POLLRDBAND);
		if (na->tx_si_users > 0)
			wake_up_interruptible_poll(&na->tx_si, POLLIN |
					POLLRDNORM | POLLRDBAND);
	} else {
		kring = na->rx_rings + n_ring;
		wake_up_interruptible_poll(&kring->si, POLLIN |
					POLLRDNORM | POLLRDBAND);
		if (na->rx_si_users > 0)
			wake_up_interruptible_poll(&na->rx_si, POLLIN |
					POLLRDNORM | POLLRDBAND);
	}

	return 0;
}

static struct netmap_sock *netmap_sock_setup(struct netmap_adapter *na, struct file *filp)
{
        struct netmap_sock *nm_sock;

        na->na_private = nm_sock = (struct netmap_sock *)sk_alloc(&init_net, AF_UNSPEC,
                                                        GFP_KERNEL, &netmap_socket_proto);
        if (!nm_sock) {
            return NULL;
        }

	nm_sock->sock.wq = &nm_sock->wq;   /* XXX rcu? */
        init_waitqueue_head(&nm_sock->wq.wait);
        nm_sock->sock.file = filp;
        nm_sock->sock.ops = &netmap_socket_ops;
        sock_init_data(&nm_sock->sock, &nm_sock->sk);
        nm_sock->sk.sk_write_space = &netmap_sock_write_space;

	/* Create a fake skb. */
	nm_sock->fake_skb = alloc_skb(1800, GFP_ATOMIC);
	if (!nm_sock->fake_skb) {
		D("fake skbuff allocation failed");
		sk_free(&nm_sock->sk);
		na->na_private = NULL;

		return NULL;
	}

        sock_hold(&nm_sock->sk);

        /* Set the backpointer to the netmap_adapter parent structure. */
        nm_sock->na = na;

	nm_sock->owner = current;

	nm_sock->saved_nm_dtor = na->nm_dtor;
	nm_sock->saved_nm_notify = na->nm_notify;
	na->nm_dtor = &netmap_socket_nm_dtor;
	na->nm_notify = &netmap_socket_nm_notify;

        D("socket support OK for (%p)", na);

        return nm_sock;
}

struct socket *get_netmap_socket(int fd)
{
	struct file *filp = fget(fd);
	struct netmap_priv_d *priv;
	struct netmap_adapter *na;
	struct netmap_sock *nm_sock;

	if (!filp)
		return ERR_PTR(-EBADF);

	if (filp->f_op != &netmap_fops)
		return ERR_PTR(-EINVAL);

	priv = (struct netmap_priv_d *)filp->private_data;
	if (!priv)
		return ERR_PTR(-EBADF);

	NMG_LOCK();
	na = priv->np_na;
	if (na == NULL) {
		NMG_UNLOCK();
		return ERR_PTR(-EBADF);
	}

	nm_sock = (struct netmap_sock *)(na->na_private);

	if (NETMAP_OWNED_BY_KERN(na) && (!nm_sock || nm_sock->owner != current)) {
		NMG_UNLOCK();
		return ERR_PTR(-EBUSY);
	}

	if (!nm_sock)
		nm_sock = netmap_sock_setup(na, filp);
	NMG_UNLOCK();

	ND("na_private %p, nm_sock %p", na->na_private, nm_sock);

	/* netmap_sock_setup() may fail because of OOM */
	if (!nm_sock)
		return ERR_PTR(-ENOMEM);

	return &nm_sock->sock;
}
EXPORT_SYMBOL(get_netmap_socket);

static int netmap_socket_sendmsg(struct kiocb *iocb, struct socket *sock,
                                 struct msghdr *m, size_t total_len)
{
    struct netmap_sock *nm_sock = container_of(sock, struct netmap_sock, sock);

    return netmap_common_sendmsg(nm_sock->na, m, total_len, 0);
}

static int netmap_socket_recvmsg(struct kiocb *iocb, struct socket *sock,
		struct msghdr *m, size_t total_len, int flags)
{
	struct netmap_sock *nm_sock = container_of(sock, struct netmap_sock, sock);
	struct netmap_adapter *na = nm_sock->na;
        int ret = netmap_common_recvmsg(na, m, total_len);
        int peek_len;

	/* Update the fake skbuff. */
	peek_len = netmap_common_peek_head_len(na);
	if (peek_len)
		nm_sock->fake_skb->len = peek_len;
	else {
		skb_dequeue(&nm_sock->sk.sk_receive_queue);
		D("dequeue");
	}

	return ret;
}
#endif  /* >= 2.6.35 */
#endif /* WITH_V1000 */


#ifdef WITH_VALE
#ifdef CONFIG_NET_NS
#include <net/netns/generic.h>

int netmap_bns_id;

struct netmap_bns {
	struct net *net;
	struct nm_bridge *bridges;
	u_int num_bridges;
};

#ifdef NETMAP_LINUX_HAVE_PERNET_OPS_ID
int
nm_bns_create(struct net *net, struct netmap_bns **ns)
{
	*ns = net_generic(net, netmap_bns_id);
	return 0;
}
#define nm_bns_destroy(_1, _2)
#else
int
nm_bns_create(struct net *net, struct netmap_bns **ns)
{
	int error = 0;

	*ns = kmalloc(sizeof(*ns), GFP_KERNEL);
	if (!*ns)
		return -ENOMEM;

	error = net_assign_generic(net, netmap_bns_id, *ns);
	if (error) {
		kfree(*ns);
		*ns = NULL;
	}
	return error;
}

void
nm_bns_destroy(struct net *net, struct netmap_bns *ns)
{
	kfree(ns);
	net_assign_generic(net, netmap_bns_id, NULL);
}
#endif

struct net*
netmap_bns_get(void)
{
	return get_net(current->nsproxy->net_ns);
}

void
netmap_bns_put(struct net *net_ns)
{
	put_net(net_ns);
}

void
netmap_bns_getbridges(struct nm_bridge **b, u_int *n)
{
	struct net *net_ns = current->nsproxy->net_ns;
	struct netmap_bns *ns = net_generic(net_ns, netmap_bns_id);

	*b = ns->bridges;
	*n = ns->num_bridges;
}

static int __net_init
netmap_pernet_init(struct net *net)
{
	struct netmap_bns *ns;
	int error = 0;

	error = nm_bns_create(net, &ns);
	if (error)
		return error;

	ns->net = net;
	ns->num_bridges = 8;
	ns->bridges = netmap_init_bridges2(ns->num_bridges);
	if (ns->bridges == NULL) {
		nm_bns_destroy(net, ns);
		return -ENOMEM;
	}

	return 0;
}

static void __net_init
netmap_pernet_exit(struct net *net)
{
	struct netmap_bns *ns = net_generic(net, netmap_bns_id);

	netmap_uninit_bridges2(ns->bridges, ns->num_bridges);
	ns->bridges = NULL;

	nm_bns_destroy(net, ns);
}

static struct pernet_operations netmap_pernet_ops = {
	.init = netmap_pernet_init,
	.exit = netmap_pernet_exit,
#ifdef NETMAP_LINUX_HAVE_PERNET_OPS_ID
	.id = &netmap_bns_id,
	.size = sizeof(struct netmap_bns),
#endif
};

int
netmap_bns_register(void)
{
#ifdef NETMAP_LINUX_HAVE_PERNET_OPS_ID
	return -register_pernet_subsys(&netmap_pernet_ops);
#else
	return -register_pernet_gen_subsys(&netmap_bns_id,
			&netmap_pernet_ops);
#endif
}

void
netmap_bns_unregister(void)
{
#ifdef NETMAP_LINUX_HAVE_PERNET_OPS_ID
	unregister_pernet_subsys(&netmap_pernet_ops);
#else
	unregister_pernet_gen_subsys(netmap_bns_id,
			&netmap_pernet_ops);
#endif
}
#endif /* CONFIG_NET_NS */
#endif /* WITH_VALE */


/* ########################## MODULE INIT ######################### */

struct miscdevice netmap_cdevsw = { /* same name as FreeBSD */
	MISC_DYNAMIC_MINOR,
	"netmap",
	&netmap_fops,
};


static int linux_netmap_init(void)
{
        /* Errors have negative values on linux. */
	return -netmap_init();
}


static void linux_netmap_fini(void)
{
        netmap_fini();
}

#ifndef NETMAP_LINUX_HAVE_LIVE_ADDR_CHANGE
#define IFF_LIVE_ADDR_CHANGE 0
#endif

#ifndef NETMAP_LINUX_HAVE_TX_SKB_SHARING
#define IFF_TX_SKB_SHARING 0
#endif

static struct device_driver linux_dummy_drv = {.owner = THIS_MODULE};

static int linux_nm_vi_open(struct net_device *netdev)
{
	netif_start_queue(netdev);
	return 0;
}

static int linux_nm_vi_stop(struct net_device *netdev)
{
	netif_stop_queue(netdev);
	return 0;
}
static int linux_nm_vi_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	if (skb != NULL)
		kfree_skb(skb);
	return 0;
}

#ifdef NETMAP_LINUX_HAVE_GET_STATS64
static struct rtnl_link_stats64 *linux_nm_vi_get_stats(
		struct net_device *netdev,
		struct rtnl_link_stats64 *stats)
{
	return stats;
}
#endif

static int linux_nm_vi_change_mtu(struct net_device *netdev, int new_mtu)
{
	return 0;
}
static void linux_nm_vi_destructor(struct net_device *netdev)
{
//	netmap_detach(netdev);
	free_netdev(netdev);
}
static const struct net_device_ops nm_vi_ops = {
	.ndo_open = linux_nm_vi_open,
	.ndo_stop = linux_nm_vi_stop,
	.ndo_start_xmit = linux_nm_vi_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_change_mtu = linux_nm_vi_change_mtu,
#ifdef NETMAP_LINUX_HAVE_GET_STATS64
	.ndo_get_stats64 = linux_nm_vi_get_stats,
#endif
};
/* dev->name is not initialized yet */
static void
linux_nm_vi_setup(struct ifnet *dev)
{
	ether_setup(dev);
	dev->netdev_ops = &nm_vi_ops;
	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->priv_flags |= IFF_LIVE_ADDR_CHANGE;
	dev->destructor = linux_nm_vi_destructor;
	dev->tx_queue_len = 0;
	/* XXX */
	dev->features = NETIF_F_LLTX | NETIF_F_SG | NETIF_F_FRAGLIST |
		NETIF_F_HIGHDMA | NETIF_F_HW_CSUM | NETIF_F_TSO;
#ifdef NETMAP_LINUX_HAVE_HW_FEATURES
	dev->hw_features = dev->features & ~NETIF_F_LLTX;
#endif
#ifdef NETMA_LINUX_HAVE_ADDR_RANDOM
	eth_hw_addr_random(dev);
#endif
}

int
nm_vi_persist(const char *name, struct ifnet **ret)
{
	struct ifnet *ifp;

	if (!try_module_get(linux_dummy_drv.owner))
		return EFAULT;
#ifdef NETMAP_LINUX_ALLOC_NETDEV_4ARGS
	ifp = alloc_netdev(0, name, NET_NAME_UNKNOWN, linux_nm_vi_setup);
#else
	ifp = alloc_netdev(0, name, linux_nm_vi_setup);
#endif
	if (!ifp) {
		module_put(linux_dummy_drv.owner);
		return ENOMEM;
	}
	dev_net_set(ifp, &init_net);
	ifp->features |= NETIF_F_NETNS_LOCAL; /* just for safety */
	register_netdev(ifp);
	ifp->dev.driver = &linux_dummy_drv;
	netif_start_queue(ifp);
	*ret = ifp;
	return 0;
}

void
nm_vi_detach(struct ifnet *ifp)
{
	netif_stop_queue(ifp);
	unregister_netdev(ifp);
	module_put(linux_dummy_drv.owner);
}

module_init(linux_netmap_init);
module_exit(linux_netmap_fini);

/* export certain symbols to other modules */
EXPORT_SYMBOL(netmap_attach);		/* driver attach routines */
EXPORT_SYMBOL(netmap_detach);		/* driver detach routines */
EXPORT_SYMBOL(nm_txsync_prologue);	/* txsync support */
EXPORT_SYMBOL(nm_rxsync_prologue);	/* rxsync support */
EXPORT_SYMBOL(netmap_ring_reinit);	/* ring init on error */
EXPORT_SYMBOL(netmap_reset);		/* ring init routines */
EXPORT_SYMBOL(netmap_rx_irq);	        /* default irq handler */
EXPORT_SYMBOL(netmap_no_pendintr);	/* XXX mitigation - should go away */
#ifdef WITH_VALE
EXPORT_SYMBOL(netmap_bdg_ctl);		/* bridge configuration routine */
EXPORT_SYMBOL(netmap_bdg_learning);	/* the default lookup function */
EXPORT_SYMBOL(netmap_bdg_name);		/* the bridge the vp is attached to */
#endif /* WITH_VALE */
EXPORT_SYMBOL(netmap_disable_all_rings);
EXPORT_SYMBOL(netmap_enable_all_rings);
EXPORT_SYMBOL(netmap_krings_create);


MODULE_AUTHOR("http://info.iet.unipi.it/~luigi/netmap/");
MODULE_DESCRIPTION("The netmap packet I/O framework");
MODULE_LICENSE("Dual BSD/GPL"); /* the code here is all BSD. */
