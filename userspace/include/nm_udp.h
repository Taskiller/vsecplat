#ifndef __NM_UDP_H__
#define __NM_UDP_H__

struct udphdr{
	u16 source;
	u16 dest;
	u16 len;
	u16 check;	
};

#endif
