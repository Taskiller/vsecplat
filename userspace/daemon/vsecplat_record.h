#ifndef __VSECPLAT_RECORD_H__
#define __VSECPLAT_RECORD_H__
#include "nm_type.h"

struct record_entry{
	u32 sip;
	u32 dip;
	u32 sport;
	u32 dport;
	u32 proto;
	u64 count;
};

#endif
