#include "nm_type.h"
#include "rte_json.h"
#include  "vsecplat_policy.h"

static int vsecplat_add_rules(struct rte_json *json)
{
	return 0;
}

static int vsecplat_del_rules(struct rte_json *json)
{
	return 0;
}

int vsecplat_parse_policy(const char *buf)
{
	struct rte_json *json=NULL, *item=NULL;		
	struct rte_json *tmp=NULL, *entry=NULL;
	int action=0;
	int rule_num=0;
	int idx, size=0;
	
	struct forward_rules *forward_rules=NULL;
	struct rule_entry *rule_entry=NULL;
	
	json = rte_parse_json(buf);
	if(NULL==json){
		printf("Failed to parse policy json.\n");
		return -1;
	}

	item = rte_object_get_item(json, "action");
	if(NULL==item){
		goto out;
	}
	
	if(item->type != JSON_INTEGER){
		goto out;
	}
	action = item->u.val_int;

	item = rte_object_get_item(json, "forward_rules");
	if(NULL==item){
		goto out;
	}
	if(item->type!=JSON_ARRAY){
		goto out;
	}

	rule_num = rte_array_get_size(item);	
	if(rule_num==0){
		goto out;
	}
	size = rule_num*sizeof(struct rule_entry)+sizeof(struct forward_rules);
	forward_rules = (struct forward_rules *)malloc(size);
	if(NULL==forward_rules){
		// TODO
		goto out;
	}
	memset(forward_rules, 0, sizeof(size));	
	forward_rules->rule_entry = (void *)forward_rules+sizeof(struct forward_rules);	
	for(idx=0;idx<rule_num;idx++){
		rule_entry = forward_rules->rule_entry + idx;
		entry = rte_array_get_item(item, idx);
		if(NULL==entry){
			// TODO
			goto out;
		}
		tmp = rte_object_get_item(entry, "sip");
		if(NULL==tmp){
			goto out;
		}
		
		tmp = rte_object_get_item(entry, "dip");
		if(NULL==tmp){
			goto out;
		}

		tmp = rte_object_get_item(entry, "sport");
		if(NULL==tmp){
			goto out;
		}

		tmp = rte_object_get_item(entry, "dport");
		if(NULL==tmp){
			goto out;
		}

		tmp = rte_object_get_item(entry, "vlanid");
		if(NULL==tmp){
			goto out;
		}

		tmp = rte_object_get_item(entry, "forward");
		if(NULL==tmp){
			goto out;
		}
		


	}

	switch(action){
		case ADD_RULE:
			break;
		case DEL_RULE:
			break;
		default:
			break;
	}

	rte_destroy_json(json);
	return 0;
out:
	rte_destroy_json(json);
	return -1;
}
