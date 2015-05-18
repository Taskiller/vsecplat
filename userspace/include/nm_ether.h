#ifndef __NM_ETHER_H__
#define __NM_ETHER_H__
#include "nm_type.h"

#define ETH_ALEN 6
#define ETH_HLEN 14

/* Ethernet Protocol ID */
#define ETH_P_IP 	0x0800
#define ETH_P_ARP	0x0806
#define ETH_P_8021Q	0x8100

/* Non DIX type */
#define ETH_P_802_3 0x0001
#define ETH_P_802_2 0x0004

struct ethhdr{
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
	u16 h_proto;
}__attribute__((packed));

#endif
