#include <string.h>
#include <unistd.h>
#include "packet.h"
#include "nm_vlan.h"
#include "nm_ether.h"
#include "vsecplat_config.h"
#include "vsecplat_policy.h"
#include "vsecplat_interface.h"
#include "vsecplat_record.h"

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

static int nm_ipv4_recv(struct nm_skb *skb)
{
	int ret=NM_PKT_FORWARD;

	nm_skb_pull(skb, skb->nh.iph->ihl*4);
	skb->h.raw = skb->data;

	/*
	 * mirror_state==0 或 guide_state==0 时，都不需要转发，所以不必进行策略匹配。
	 * 之所以到这里才返回，是为了识别数据包的地址和端口，以便进行统计
	 * */
	if((0==global_vsecplat_config->mirror_state)|| (0==global_vsecplat_config->guide_state)){
		return NM_PKT_DROP;
	}

	ret = get_forward_policy(skb);

	return ret;	
}

static int nm_arp_recv(struct nm_skb *skb)
{
	return NM_PKT_DROP;
}

static int nm_vlan_recv(struct nm_skb *skb)
{
	int ret=0;
	unsigned short vlan_TCI, vid;
	struct vlan_ethhdr *veth = (struct vlan_ethhdr *)nm_skb_push(skb, ETH_HLEN);

	skb->protocol = veth->h_vlan_encapsulated_proto;
	vlan_TCI = ntohs(veth->h_vlan_TCI);	
	vid = (vlan_TCI & VLAN_VID_MASK);

	skb->vlanid = vid;
	
	nm_skb_pull(skb, VLAN_ETH_HLEN);
	if(ntohs(skb->protocol)==ETH_P_IP){
		ret = nm_ipv4_recv(skb);
	}else if(ntohs(skb->protocol)==ETH_P_ARP){
		ret = nm_arp_recv(skb);	
	}

	return ret;
}

static int packet_intercept(struct nm_skb *skb)
{
	int ret=NM_PKT_DROP;

	skb->mac.raw = skb->data;

	eth_type_trans(skb);
	switch(ntohs(skb->protocol)){
		case ETH_P_IP:
			ret = nm_ipv4_recv(skb);
			break;
		case ETH_P_8021Q:
			ret = nm_vlan_recv(skb);
			break;
		case ETH_P_ARP:
			ret = nm_arp_recv(skb);
			break;
		default:
			break;
	}

	return ret;
}

static int packet_send(struct nm_skb *skb)
{
	if(NULL==skb->o_dev){
		skb->o_dev = nm_get_output_dev(); 
	}

	memcpy(skb->mac.raw+NM_MAC_LEN, skb->o_dev->mac, NM_MAC_LEN);
	nm_send(skb);
	return 0;
}

void *packet_handle_thread(void *unused)
{
	int ret=0;
	int orig_len=0;
	struct nm_skb *skb=NULL;

	do{
		skb = nm_recv();
		if(NULL==skb){
			continue;
		}

		/* mirror_state==0, 既不转发，也不统计 */
		if(0==global_vsecplat_config->mirror_state){
			continue;
		}

		orig_len = skb->len;

		ret = packet_intercept(skb);

		/* guide_state==0, 停止转发，但是要统计 */
		if(0==global_vsecplat_config->guide_state){
			ret = NM_PKT_DROP;
		}

		if(ret==NM_PKT_FORWARD){
			packet_send(skb);
		}

		skb->len = orig_len;
		vsecplat_record_pkt(skb);
	}while(1);

	return unused;
}
