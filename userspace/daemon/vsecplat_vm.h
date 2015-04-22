#ifndef __VSECPLAT_VM_H__
#define __VSECPLAT_VM_H__

struct rule_list{
	int id;
	int action;
	char sip[NM_ADDR_STR_LEN];	
	char dip[NM_ADDR_STR_LEN];	
	int sport;
	int dport;
	int proto;
	int vlanid;
};

struct vm_rules{
	char vm_name[NM_NAME_LEN];
	int rule_num;
	struct rule_list *rule_list
};

struct vm_config{
	char vm_name[NM_NAME_LEN];	
	char inport[NM_ADDR_STR_LEN];
};

struct vm_list{
	int vm_num;
	struct vm_config *vm_config;
};

#endif
