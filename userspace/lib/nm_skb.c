#include <nm_dev.h>
#include <nm_desc.h>
#include <nm_skb.h>

extern struct nm_desc *global_nm_desc;

int nm_send(struct nm_skb *skb);

void nm_tx_skb_push(struct nm_skb *skb)
{
	list_add_tail(&skb->node, &(global_nm_desc->tx_pool.list));
	global_nm_desc->tx_pool.count++;
}

void nm_tx_skb_pop(void)
{
	struct list_head send_list;
	struct nm_skb *skb = NULL;

	list_replace_init(&(global_nm_desc->tx_pool.list), &send_list);
	global_nm_desc->tx_pool.count=0;

	while(!list_empty(&send_list)){
		skb = list_first_entry(&send_list, struct nm_skb, node);
		list_del(&skb->node);
		nm_send(skb);	
	}
}

static inline int get_next_if_idx(int start)
{
	while(start<global_nm_desc->fds_num){
		if(global_nm_desc->fds[start].revents & POLLIN){
			break;
		}
		start++;
	}

	if(start==global_nm_desc->fds_num){
		return -1;
	}

	return start;
}

static inline int get_next_ring_idx(int rx_if_idx, int start)
{
	int rx_ring_idx = start;

	struct nm_dev *dev = global_nm_desc->nm_dev[rx_if_idx];
	struct netmap_if *nifp = dev->nifp;
	struct netmap_ring *ring=NULL;

	if(start==0){
		rx_ring_idx = dev->first_rx_ring;
	}else{
		rx_ring_idx = start;
	}

	while(rx_ring_idx<=dev->last_rx_ring){
		ring = NETMAP_RXRING(nifp, rx_ring_idx);
		if(nm_ring_space(ring)) {
			break;
		}
		rx_ring_idx++;
	}

	if(rx_ring_idx>dev->last_rx_ring){
		return -1;
	}

	return rx_ring_idx;
}

extern int vsecplat_show_packet __attribute__((weak, section("data")));

struct nm_skb *nm_recv(void)
{
	struct nm_skb *skb=NULL;
	struct nm_dev *dev=NULL;
	char *p=NULL;
	struct netmap_if *nifp=NULL;
	struct netmap_ring *ring=NULL;
	struct netmap_slot *slot=NULL;
	int rx_if_idx, rx_ring_idx, cur; 
	int ret=0;

	if(global_nm_desc->need_poll){ // Need poll again
		ret = poll(global_nm_desc->fds, global_nm_desc->fds_num, 100);
		if(ret==0){
			// printf("poll timeout\n");
			goto no_packet;
		}
		if(ret<0){
			printf("poll error\n");
			goto no_packet;
		}
	}

 	// Find the avail netmap_if
get_next_if:
	rx_if_idx = get_next_if_idx(global_nm_desc->rx_if_idx);
	if(rx_if_idx!=-1){
		global_nm_desc->rx_if_idx = rx_if_idx;
		global_nm_desc->need_poll = 0;
	}else{
		global_nm_desc->rx_if_idx = 0;
		global_nm_desc->rx_ring_idx = 0;
		global_nm_desc->need_poll = 1;
		goto no_packet;
	}

 	// Find the avail ring
	rx_ring_idx = get_next_ring_idx(rx_if_idx, global_nm_desc->rx_ring_idx);
	if(rx_ring_idx!=-1){
		global_nm_desc->rx_ring_idx = rx_ring_idx;
	}else{
		//now this netmap_if has no packet
		global_nm_desc->fds[rx_if_idx].revents=0; 
		goto get_next_if;
	}

	dev = global_nm_desc->nm_dev[rx_if_idx];
	nifp = dev->nifp;
	ring = NETMAP_RXRING(nifp, rx_ring_idx);
	cur = ring->cur;
	slot = &ring->slot[cur];
	p = NETMAP_BUF(ring, slot->buf_idx);
    skb = (struct nm_skb *)p;
	skb->rx_if_idx = rx_if_idx;
	skb->rx_ring_idx = rx_ring_idx;
	skb->rx_slot_idx = cur;

	skb->i_dev = dev;
    skb->head = (unsigned char *)p + NM_HEAD_OFFSET + 12;
    // skb->head = (unsigned char *)p + NM_HEAD_OFFSET;
    skb->data = skb->head;
    skb->tail = (unsigned char *)p + slot->len;
    skb->end = (unsigned char *)p + NM_BUF_SIZE - NM_END_RESERVED;
    skb->len = slot->len - 12;
    // skb->len = slot->len;

#if 1
	if(vsecplat_show_packet){
		printf("Recv %s, cur=%d, len=%d, skb->data = %x %x %x %x %x %x: %x %x %x %x %x %x, %x %x %x %x, %x %x %x %x\n",
			dev->name, cur, skb->len,
			skb->data[0], skb->data[1], skb->data[2],
			skb->data[3], skb->data[4], skb->data[5],
			skb->data[6], skb->data[7], skb->data[8],
			skb->data[9], skb->data[10], skb->data[11],
			skb->data[12], skb->data[13], skb->data[14], skb->data[15],
			skb->data[16], skb->data[17], skb->data[18], skb->data[19]);
	}
#endif
    cur = nm_ring_next(ring, cur);
    ring->head = ring->cur = cur;
	
no_packet:
	return skb;
}

static struct netmap_ring *get_tx_ring(struct nm_dev *dev)
{
	struct netmap_ring *ring=NULL;
	int start = dev->first_tx_ring;
	while(start<=dev->last_tx_ring){
		ring = NETMAP_TXRING(dev->nifp, start);
		if(nm_ring_space(ring)){
			break;
		}
		// printf("tx ring, start=%d, head=%d, cur=%d, tail=%d\n", start, ring->head, ring->cur, ring->tail);
		start++;
	}

	if(start>dev->last_tx_ring){
		// when no avial space in ring
		return NULL;
	}

	return ring;
}

int nm_send(struct nm_skb *skb)
{
	struct netmap_if *rx_nifp;
	struct netmap_ring *rx_ring, *tx_ring;
	struct netmap_slot *rx_slot, *tx_slot;
	int cur;

	struct nm_dev *tx_dev=skb->o_dev;	

	if(tx_dev==NULL){
		// printf("tx_dev is NULL");
		return -1;
	}
#if 1
	if(vsecplat_show_packet){
		printf("Send %s, len=%d, skb->head = %x %x %x %x %x %x: %x %x %x %x %x %x, %x %x %x %x, %x %x %x %x\n",
			tx_dev->name, skb->len,
			skb->head[0], skb->head[1], skb->head[2],
			skb->head[3], skb->head[4], skb->head[5],
			skb->head[6], skb->head[7], skb->head[8],
			skb->head[9], skb->head[10], skb->head[11],
			skb->head[12], skb->head[13], skb->head[14], skb->head[15],
			skb->head[13], skb->head[14], skb->head[15], skb->head[16]);
	}
#endif

	if(skb->buf_type==MEMORY_BUF){ // packet buf is system memory

	}else{ // packet buf is netmap buf
		rx_nifp = skb->i_dev->nifp;
		rx_ring = NETMAP_RXRING(rx_nifp, skb->rx_ring_idx);
		rx_slot = &rx_ring->slot[skb->rx_slot_idx];
		tx_ring = get_tx_ring(tx_dev);
		if(tx_ring==NULL){
			printf("No tx space, Need do TXSYNC.\n");
		 	ioctl(tx_dev->fd, NIOCTXSYNC, NULL); // No tx space, Need do TXSYNC
			nm_tx_skb_push(skb);
			return 0;
		}

		cur = tx_ring->cur;
		tx_slot = &tx_ring->slot[tx_ring->cur];	
		tx_slot->len = rx_slot->len;
		uint32_t pkt = tx_slot->buf_idx;	
		tx_slot->buf_idx = rx_slot->buf_idx;
		rx_slot->buf_idx = pkt;
		tx_slot->flags |= NS_BUF_CHANGED;
		rx_slot->flags |= NS_BUF_CHANGED;

		cur = nm_ring_next(tx_ring, cur);
		tx_ring->head = tx_ring->cur = cur;
	}

	ioctl(tx_dev->fd, NIOCTXSYNC, NULL);

	return 0;
}

struct nm_skb *nm_alloc_skb(void)
{
	struct nm_skb *skb = NULL;

	skb = (struct nm_skb *)malloc(NM_BUF_SIZE);	
	if(NULL==skb){
		return NULL;
	}
	memset(skb, 0, sizeof(struct nm_skb));
	skb->buf_type = MEMORY_BUF;	
	skb->head = (unsigned char *)skb + NM_HEAD_OFFSET;
	skb->data = skb->tail = skb->head;
	skb->end = (unsigned char *)skb + NM_BUF_SIZE;

	return skb;
}

void nm_free_skb(struct nm_skb *skb)
{
	if(skb->buf_type==MEMORY_BUF){
		free(skb);
	}else if(skb->buf_type==NETMAP_BUF){
		//TODO: will free netmap buf
	}
	return;
}

unsigned char *nm_skb_push(struct nm_skb *skb, unsigned int len)
{
	skb->data -= len;	
	skb->len += len;
	//TODO: need do some check
	if(skb->data<skb->head){
		printf("Error in nm_skb_push\n");
	}
	return skb->data;
}

unsigned char *nm_skb_pull(struct nm_skb *skb, unsigned int len)
{
	if(len>skb->len){
		return NULL;
	}
	skb->len -= len;
	skb->data += len;
	//TODO: need do some check 
	return skb->data;
}

