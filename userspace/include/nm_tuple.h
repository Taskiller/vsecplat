#ifndef __NM_TUPLE_H__
#define __NM_TUPLE_H__

struct nm_tuple{
	uint32_t sip_addr;
	uint32_t dip_addr;
	uint16_t sport;
	uint16_t dport;
	uint8_t protocol;
};

#endif
