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
	struct in_addr host_ip;
	struct{
		struct in_addr min;
		struct in_addr max;
	}range;
	struct {
		struct in_addr mask;
		int len;
	}net;
	struct{
		struct in_addr ip_addrs[16];
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

int vsecplat_parse_policy(const char *buf);
int test_policy_parse(void);
#endif
