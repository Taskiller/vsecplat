#ifndef __NM_VLAN_H__
#define __NM_VLAN_H__

#define VLAN_HLEN 4
#define VLAN_ETH_ALEN 6
#define VLAN_ETH_HLEN 18

#define VLAN_VID_MASK 0xfff

struct vlan_ethhdr{
	unsigned char h_dest[VLAN_ETH_ALEN];
	unsigned char h_source[VLAN_ETH_ALEN];
	__be16 h_vlan_proto;
	__be16 h_vlan_TCI;
	unsigned short h_vlan_encapsulated_proto;
};
#endif
