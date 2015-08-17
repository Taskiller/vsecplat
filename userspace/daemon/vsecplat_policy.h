#ifndef __VSECPLAT_VM_H__
#define __VSECPLAT_VM_H__
#include "nm_list.h"
#include "nm_skb.h"
#include "nm_mutex.h"

#define GROUP_ITEM_MAX_NUM 16
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
	IP_NET_MASK,
	IP_RANGE,
	IP_GROUP
};
struct addr_obj{
	int type;
	u32 addr_mask;
	union{
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
			u32 ip_addrs[GROUP_ITEM_MAX_NUM];
		}group;
	}u;
};

enum{
	NUM_NULL,
	NUM_SINGLE,
	NUM_RANGE,
	NUM_GROUP
};
struct num_obj{
	int type;
	u32 num_mask;
	union{
		u32 num; // single number
		struct{   
			u32 min;
			u32 max;
		}range;
		struct{
			u32 num[GROUP_ITEM_MAX_NUM];
		}group;
	}u;
};

struct rule_entry{
	struct list_head list;
	int id;
	int forward;
	int conversion;
	unsigned char dst_mac[NM_MAC_LEN];
#define RULE_NOT_SIP  (1<<0)
#define RULE_NOT_DIP  (1<<1)
#define RULE_NOT_SPORT  (1<<2)
#define RULE_NOT_DPORT  (1<<3)
#define RULE_NOT_PROTO  (1<<4)
#define RULE_NOT_VLANID  (1<<5)
	int rule_not_flag;
	struct addr_obj sip;
	struct addr_obj dip;
	struct num_obj sport;
	struct num_obj dport;
	struct num_obj proto;
	struct num_obj vlanid;
};

struct forward_rules{
	int rule_num;
	void **rule_entry;
};

enum{
	NM_ADD_RULES=1,
	NM_DEL_RULES,
	NM_CHECK_RULES,  // Maybe no use
	NM_DISABLE_MIRROR, // Stop report and forward
	NM_ENABLE_MIRROR,
	NM_DISABLE_GUIDE, // Stop forward
	NM_ENABLE_GUIDE
};
struct forward_rules_head{
	struct list_head list;
	// int count;
	struct nm_mutex mutex;
};

int init_policy_list(void);
int vsecplat_parse_policy(const char *buf);
int get_forward_policy(struct nm_skb *skb);
int create_policy_response(char *buf, int result, int report_state);

int add_test_policy(void);
#endif
