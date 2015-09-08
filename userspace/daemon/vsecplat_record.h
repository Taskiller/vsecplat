#ifndef __VSECPLAT_RECORD_H__
#define __VSECPLAT_RECORD_H__
#include <sys/time.h>
#include "nm_type.h"
#include "nm_list.h"
#include "nm_skb.h"
#include "nm_mutex.h"

struct record_entry{
	struct list_head list;
	u32 sip;
	u32 dip;
	u16 sport;
	u16 dport;
	u8 proto;
	u16 vlanid;
	u64 count;
	u64 packetsize;
};

struct record_bucket{
	struct list_head list;
	// u32 count;
	struct nm_mutex mutex;
};

struct record_json_item{
	struct list_head list;
	struct rte_json *root;
	struct rte_json *array;
	char *json_str;
};

int vsecplat_init_record_bucket(void);
int vsecplat_record_pkt(struct nm_skb *skb);
int vsecplat_test_record(void);
int vsecplat_persist_record(void);
int clear_global_record_json_list(void);

extern struct list_head global_record_json_list;

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

#endif
