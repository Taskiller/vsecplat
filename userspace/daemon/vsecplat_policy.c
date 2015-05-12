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
	int slen=strlen(str), len=0;

	if(pos=strchr(str, '/')){ // XX.XX.XX.XX/N
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
	}else if(pos=strchr(str, '-')){ // XX.XX.XX.XX-YY.YY.YY.YY
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
	}else if(pos=strchr(str, '|')){ // XX.XX.XX.XX |YY.YY.YY.YY
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
	int rule_num=0;
	int idx, size=0;

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
	forward_rules->rule_num = rule_num;
	forward_rules->rule_entry = (void *)forward_rules+sizeof(struct forward_rules);	
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

int vsecplat_parse_policy(const char *buf)
{
	struct rte_json *json=NULL, *item=NULL;
	int action=0;
	
	struct forward_rules *forward_rules=NULL;
	
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
