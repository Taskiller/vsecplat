#ifndef __VSECPLAT_VM_H__
#define __VSECPLAT_VM_H__
#include "nm_list.h"
#include "nm_mutex.h"

/*
 * type:
 * 	1: 10.0.0.1
 * 	2: 10.0.0.1/24
 * 	3: 10.0.0.1-10.0.0.25
 * 	4: 10.0.0.1|10.0.0.11|10.0.0.23
 **/
enum{
	IP_NULL,
	IP_HOST,
	IP_NET,
	IP_RANGE,
	IP_GROUP
};
struct addr_obj{
	int type;
	u32 host_ip;
	struct{
		u32 min;
		u32 max;
	}range;
	struct {
		u32 mask;
		u32 length;
	}net;
	struct{
		u32 ip_addrs[16];
	}group;
};

struct rule_entry{
	struct list_head list;
	int id;
	int forward;
	struct addr_obj sip;
	struct addr_obj dip;
	int sport;
	int dport;
	int proto;
	int vlanid;
};

struct forward_rules{
	int rule_num;
	struct rule_entry *rule_entry;
};

struct forward_rules_head{
	struct list_head list;
	// int count;
	struct nm_mutex mutex;
};

#define ADD_RULE 1
#define DEL_RULE 2

int init_policy_list(void);
int vsecplat_parse_policy(const char *buf);
int get_forward_policy(struct nm_skb *skb);

int test_policy_parse(void);
#endif
