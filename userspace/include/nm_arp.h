#ifndef __NM_ARP_H__
#define __NM_ARP_H__

#include "nm_type.h"

struct arphdr{
	__be16 			ar_hrd;
	__be16 			ar_pro;
	unsigned char 	ar_hln;
	unsigned char 	ar_pln;
	__be16 			ar_op;
};
#endif
