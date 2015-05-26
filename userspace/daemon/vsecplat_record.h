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
	u64 timestamp;
};

struct record_bucket{
	struct list_head list;
	u32 count;
	struct nm_mutex mutex;
};

int vsecplat_init_record_bucket(void);
int vsecplat_record_pkt(struct nm_skb *skb);

#define LIST_FIND(head, cmpfn, type, args...) 	\
		({ 										\
		 const struct list_head *__i, *__j=NULL;\
		 list_for_each(__i, (head))				\
		 if(cmpfn((const type)__i, ##args)){	\
			 __j = __i;							\
			 break;								\
		 }										\
		 (type)__j;								\
		 })

char *persist_record(void);

#endif
