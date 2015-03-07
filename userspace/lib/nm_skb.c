#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nm_skb.h>

struct nm_skb *nm_recv(void)
{
	return NULL;
}

int nm_send(struct nm_skb *skb)
{
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

	return skb->data;
}

unsigned char *nm_skb_pull(struct nm_skb *skb, unsigned int len)
{
	if(len>skb->size){
		return NULL;
	}
	skb->len -= len;
	skb->data += len;

	return skb->data;
}
