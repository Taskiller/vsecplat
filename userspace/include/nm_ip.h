#ifndef __NM_IP_H__
#define __NM_IP_H__

#include "nm_type.h"

struct iphdr{
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8 ihl:4,
	   version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u8 version:4,
	   ihl:4;
#endif
	u8 	tos;
	u16 tot_len;
	u16 id;
	u16 frag_off;
	u8 	ttl;
	u8 	protocol;
	u16 check;
	u32 saddr;
	u32 daddr;
};

#endif
