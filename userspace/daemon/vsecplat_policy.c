#include "nm_type.h"
#include "rte_json.h"
#include  "vsecplat_policy.h"

static int vsecplat_add_vm(struct rte_json *json)
{
	return 0;
}

static int vsecplat_del_vm(struct rte_json *json)
{
	return 0;
}

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
	struct rte_json *tmp=NULL;	

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
	switch(item->u.val_int){
		case ADD_VM:
			break;
		case DEL_VM:
			break;
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
