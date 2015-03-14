#ifndef __NM_TCP_H__
#define __NM_TCP_H__

struct tcphdr{
	u16 source;
	u16 dest;
	u32 seq;
	u32 ack_seq;

#if defined(__LITTLE_ENDIAN_BITFIELD)
	u16	res1:4,
		doff:4,
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u16	doff:4,
		res1:4,
		cwr:1,
		ece:1,
		urg:1,
		ack:1,
		psh:1,
		rst:1,
		syn:1,
		fin:1;
#endif
	u16 window;
	u16 check;
	u16 urg_ptr;
};

#endif
