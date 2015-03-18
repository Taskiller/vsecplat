#include <nm_skb.h>

struct nm_skb *nm_recv(void)
{
	struct nm_skb *skb = NULL;
#if 0
	char *p=NULL;
	int rx_if_idx;
	int rx_ring_idx; 
	
	if(nm_desc->rx_if_idx==-1){ // Need poll again
       ret = poll(...);
       if(ret<=0){
      		// no packets
       }
 		// Find the avail netmap_if
       for(i=0;i<nm_desc->fds_num;i++){
     		if(nm_desc->fds[i].revents & POLLIN){
      			break;
      		}
       }
       if(i<nm_desc->fds_num){
       	nm_desc->rx_if_idx = i;
       }

	   rx_if_idx = nm_desc->rx_if_idx;
 
 		// Find the avail ring
    	for(i=first_rx_ring;i<last_rx_ring;i++){
    		ring = NETMAP_RXRING(nifp, i);
      		if(nm_ring_space(ring)) {
 				nm_desc->rx_ring_idx = i;
      			break;
      		}
    	} 
	}
	
	rx_ring_idx = nm_desc->rx_ring_idx;
	ring = NETMAP_RXRING(rx_if_idx, rx_ring_idx);
	cur = ring->cur;
	slot = &ring->slot[cur];
	p = NETMAP_BUF(ring, slot->buf_idx);
    skb = (struct nm_skb *)p;
	skb->rx_if_idx = rx_if_idx;
	skb->rx_ring_idx = rx_ring_idx;
	skb->rx_slot_idx = cur;

	// skb->i_dev = ;
    skb->head = (unsigned char *)p + NM_HEAD_OFFSET;
    skb->data = (unsigned char *)p + (NM_HEAD_OFFSET + NM_DATA_OFFSET);
    skb->tail = (unsigned char *)p + slot->len;
    skb->end = (unsigned char *)p + NM_BUF_SIZE - NM_END_RESERVED;
    skb->len = slot->len;
 
    cur = nm_ring_next(ring, cur);
    ring->head = ring->cur = cur;

#endif

	return skb;
}

int nm_send(struct nm_skb *skb)
{
#if 0
	char *p = (char *)skb;

	struct netmap_if *rx_nifp, *tx_nifp;
	struct netmap_ring *rx_ring, *tx_ring;
	struct netmap_slot *rx_slot, *tx_slot;
	
	rx_ring = NETMAP_RXRING(rx_nifp, skb->rx_ring_idx);
	rx_slot = &rx_ring->slot[skb->slot_idx];

#endif
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
	skb->head = (unsigned char *)skb + NM_SKB_SPACE;
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
	return skb->data;
}

unsigned char *nm_skb_pull(struct nm_skb *skb, unsigned int len)
{
	if(len>skb->size){
		return NULL;
	}
	skb->len -= len;
	skb->data += len;
	//TODO: need do some check 
	return skb->data;
}

void nm_queue_add_tail(struct nm_skb_queue *queue, struct nm_skb *skb)
{
	// If mutithread, need a lock
	list_add_tail(&skb->node, &queue->list);
	queue->qlen++;
}

struct nm_skb *nm_skb_dequeue(struct nm_skb_queue *queue)
{
	struct nm_skb *skb=NULL;

	// If multithread, need a lock
	
	if(!list_empty(&queue->list)){
		skb = list_first_entry(&queue->list, struct nm_skb, node);
		list_del(&skb->node);
		queue->qlen--;
	}
	return skb;	
}
