#include <ctype.h>
#include "rte_json.h"
#include "nm_type.h"
#include "nm_skb.h"
#include  "vsecplat_policy.h"

static inline int bad_ip_address(char *str)
{
	char *sp;
  	int dots = 0, nums = 0;
  	char buf[4];

  	if (str == NULL)
    	return -2;

  	for (;;)
    {  
    	memset (buf, 0, sizeof (buf));
      	sp = str;
      	while (*str != '\0'){
	  		if (*str == '.'){
	      		if (dots >= 3)
					return -1;

	    	  	if (*(str + 1) == '.')
					return -1;

	      		if (*(str + 1) == '\0')
					return -2;

	      		dots++;
	    		break;
	    	}
	  		if (!isdigit ((int) *str))
	    		return -1;

			str++;
		}

      	if (str - sp > 3)
			return -1;

      	strncpy (buf, sp, str - sp);
      	if (atoi (buf) > 255)
			return -1;

      	nums++;
      	if (*str == '\0')
			break;

      str++;
    }

  	if (nums < 4)
    	return -1;

  	return 1;
}

static int get_addr_obj_from_str(struct addr_obj *obj, const char *str)
{
	char *pos=NULL;
	char *cp=NULL;
	int len=0;

	if(NULL!=(pos=strchr(str, '/'))){ // XX.XX.XX.XX/N
		obj->type = IP_NET;
		len = pos-str;
		cp = malloc(len+1);
		if(NULL==cp){
			//TODO
			return -1;
		}
		strncpy(cp, str, len);
		*(cp+len)='\0';
		inet_aton(cp, &obj->net.mask);
		free(cp);
		obj->net.len = atoi(++pos);
	}else if(NULL!=(pos=strchr(str, '-'))){ // XX.XX.XX.XX-YY.YY.YY.YY
		obj->type = IP_RANGE;
		len = pos-str;	
		cp = malloc(len+1);
		if(NULL==cp){
			//TODO
			return -1;
		}
		strncpy(cp, str, len);
		*(cp+len)='\0';
		inet_aton(cp, &obj->range.min);
		free(cp);
		pos++;
		inet_aton(pos, &obj->range.max);
	}else if(NULL!=(pos=strchr(str, '|'))){ // XX.XX.XX.XX |YY.YY.YY.YY
		obj->type = IP_GROUP;
		const char *tmp=str;
		int idx=0;
		do{
			len = pos-tmp;
			cp = malloc(len+1);
			if(NULL==cp){
				//TODO
				return -1;
			}
			strncpy(cp, tmp, len);
			*(cp+len)='\0';
			inet_aton(cp, &obj->group.ip_addrs[idx]);
			free(cp);
			idx++;
			tmp=pos+1;
			pos=strchr(tmp,'|');
			if(NULL==pos){
				pos=strchr(tmp, '\0');
			}
			if(idx>=16){
				//TODO
				return -1;
			}
		}while(NULL!=pos);
	}else{ //single ip addr, XX.XX.XX.XX
		obj->type = IP_HOST;
		inet_aton(str, &obj->host_ip);
	}
	return 0;
}

static struct forward_rules *get_forward_rules(struct rte_json *json)
{
	struct forward_rules *forward_rules=NULL;
	struct rule_entry *rule_entry=NULL;
	struct rte_json *item=NULL, *entry=NULL, *tmp=NULL;
	int rule_num=0, idx=0;

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
	forward_rules = (struct forward_rules *)malloc(sizeof(struct forward_rules));
	if(NULL==forward_rules){
		// TODO
		goto out;
	}
	memset(forward_rules, 0, sizeof(struct forward_rules));	
	
	forward_rules->rule_num = rule_num;
	forward_rules->rule_entry = (struct rule_entry *)malloc(rule_num*sizeof(struct rule_entry));
	if(NULL==forward_rules->rule_entry){
		goto out;
	}
	memset(forward_rules->rule_entry, 0, rule_num*sizeof(struct rule_entry));	
	for(idx=0;idx<rule_num;idx++){
		rule_entry = forward_rules->rule_entry + idx;
		entry = rte_array_get_item(item, idx);
		if(NULL==entry){
			// TODO
			goto out;
		}

		tmp = rte_object_get_item(entry, "id");
		if(NULL==tmp){
			goto out;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->id = tmp->u.val_int;
		
		tmp = rte_object_get_item(entry, "sip");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_STRING){
			goto out;
		}
		get_addr_obj_from_str(&(rule_entry->sip), tmp->u.val_str);	

		tmp = rte_object_get_item(entry, "dip");
		if(NULL==tmp){
			continue;
		}
		get_addr_obj_from_str(&(rule_entry->dip), tmp->u.val_str);	

		tmp = rte_object_get_item(entry, "sport");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->sport = tmp->u.val_int;

		tmp = rte_object_get_item(entry, "dport");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->dport = tmp->u.val_int;

		tmp = rte_object_get_item(entry, "proto");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->proto = tmp->u.val_int;

		tmp = rte_object_get_item(entry, "vlanid");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->vlanid = tmp->u.val_int;

		tmp = rte_object_get_item(entry, "forward");
		if(NULL==tmp){
			continue;
		}
		if(tmp->type != JSON_INTEGER){
			goto out;
		}
		rule_entry->forward = tmp->u.val_int;
	}
	return forward_rules;
out:
	free(forward_rules);
	return NULL;
}

static struct forward_rules_head *fw_policy_list=NULL;

static int vsecplat_add_policy(struct forward_rules *forward_rules)
{
	int rule_idx=0;
	struct rule_entry *rule_entry=NULL;
	nm_mutex_lock(&fw_policy_list->mutex);	
	// Maybe need check first

	for(rule_idx=0; rule_idx<forward_rules->rule_num; rule_idx++){
		rule_entry = forward_rules->rule_entry+rule_idx;
		INIT_LIST_HEAD(&rule_entry->list);
		list_add_tail(&rule_entry->list, &fw_policy_list->list);
	}	
	nm_mutex_unlock(&fw_policy_list->mutex);	
	return 0;
}

static int vsecplat_del_policy(struct forward_rules *forward_rules)
{
	struct list_head *pos=NULL, *n=NULL;
	struct rule_entry *rule_entry=NULL, *pos_entry=NULL;
	int rule_idx=0;	
	nm_mutex_lock(&fw_policy_list->mutex);
	for(rule_idx=0;rule_idx<forward_rules->rule_num;rule_idx++){
		rule_entry = forward_rules->rule_entry+rule_idx;	
		list_for_each_safe(pos, n, &fw_policy_list->list){
			pos_entry = list_entry(pos, struct rule_entry, list);
			if(rule_entry->id == pos_entry->id){
				list_del(&pos_entry->list);
				free(pos_entry);
				break;
			}
		}
	}
	nm_mutex_unlock(&fw_policy_list->mutex);
	free(forward_rules->rule_entry);

	return 0;
}

int vsecplat_parse_policy(const char *buf)
{
	struct forward_rules *forward_rules=NULL;
	struct rte_json *json=NULL, *item=NULL;
	int action=0;
	int ret=0;	

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

	forward_rules = get_forward_rules(json);		
	if(NULL==forward_rules){
		// TODO
		return -1;
	}
	switch(action){
		case ADD_RULE:
			ret = vsecplat_add_policy(forward_rules);		
			break;
		case DEL_RULE:
			ret = vsecplat_del_policy(forward_rules);		
			break;
		default:
			break;
	}

	free(forward_rules);
	rte_destroy_json(json);
	return 0;
out:
	rte_destroy_json(json);
	return -1;
}

/*
 * return 
 * 	0: not permit forward 
 * 	1: permit
 **/
int get_forward_policy(struct nm_skb *skb)
{
	return 0;
}

int init_policy_list(void)
{
	fw_policy_list = (struct forward_rules_head *)malloc(sizeof(struct forward_rules_head));	
	if(NULL==fw_policy_list){
		return -1;
	}
	memset(fw_policy_list, 0, sizeof(struct forward_rules_head));
	INIT_LIST_HEAD(&fw_policy_list->list);
	nm_mutex_init(&fw_policy_list->mutex);

	return 0;
}

int test_policy_parse(void)
{
	int fd=0;
	int len=0;
	struct stat stat_buf;
	char *file_buf=NULL;

	memset(&stat_buf, 0, sizeof(struct stat));
	stat("./add_rule.json", &stat_buf);
	len = stat_buf.st_size;
	
	file_buf = malloc(len);
	if(NULL==file_buf){
		return -1;
	}
	memset(file_buf, 0, len);
	fd = open("./add_rule.json", O_RDONLY);
	if(fd<0){
		free(file_buf);
		return -1;
	}
	read(fd, file_buf, len);
	close(fd);

	vsecplat_parse_policy(file_buf);
	return 0;
}
