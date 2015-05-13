#ifndef __VSECPLAT_RECORD_H__
#define __VSECPLAT_RECORD_H__
#include "nm_type.h"
#include "nm_list.h"
#include "nm_skb.h"
#include "nm_mutex.h"

struct record_entry{
	struct list_head list;
	u32 sip;
	u32 dip;
	u32 sport;
	u32 dport;
	u32 proto;
	u16 vlanid;
	u64 count;
};

struct record_bucket{
	struct list_head list;
	struct nm_mutex mutex;
};

int vsecplat_init_record_bucket(void);
int vsecplat_record_pkt(struct nm_skb *skb);

#endif
