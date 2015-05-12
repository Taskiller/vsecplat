#ifndef __VSECPLAT_VM_H__
#define __VSECPLAT_VM_H__

/*
 * type:
 * 	1: 10.0.0.1
 * 	2: 10.0.0.1/24
 * 	3: 10.0.0.1-10.0.0.25
 * 	4: 10.0.0.1|10.0.0.11|10.0.0.23
 **/
enum{
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

#if 0
struct vm_config{
	char vm_name[NM_NAME_LEN];	
	char inport[NM_ADDR_STR_LEN];
};

struct vm_list{
	int vm_num;
	struct vm_config *vm_config;
};

#define ADD_VM 1
#define DEL_VM 2
#endif 

#define ADD_RULE 1
#define DEL_RULE 2

int vsecplat_parse_policy(const char *buf);
int test_policy_parse(void);
#endif
