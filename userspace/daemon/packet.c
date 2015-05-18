#include "packet.h"
#include "nm_ether.h"

static inline void eth_type_trans(struct nm_skb *skb)
{
	unsigned char *rawp;
	struct ethhdr *eth = (struct ethhdr *)(skb->mac.raw);		
	nm_skb_pull(skb, ETH_HLEN);
	if(ntohs(eth->h_proto)>=1536){
		skb->protocol = eth->h_proto;
	}else{
		rawp = skb->data;
		if(*(unsigned short *)rawp == 0xffff)
			skb->protocol = htons(ETH_P_802_3);
		else
			skb->protocol = htons(ETH_P_802_2);
	}
	skb->nh.raw = skb->data;
}

static int packet_intercept(struct nm_skb *skb)
{

	skb->mac.raw = skb->data;
		
	eth_type_trans(skb);
	switch(ntohs(skb->protocol)){
		case ETH_P_IP:
			break;
		case ETH_P_8021Q:
			break;
		case ETH_P_ARP:
			break;
		default:
			return 0;
	}
	return 0;
}

int packet_handle_loop(void)
{
	int ret=0;
	struct nm_skb *skb=NULL;

	do{
		skb = nm_recv();
		if(NULL==skb){
			return -1;
		}
		
		ret = packet_intercept(skb);

	}while(1);

	return 0;
}
