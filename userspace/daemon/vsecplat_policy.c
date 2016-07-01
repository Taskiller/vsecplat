#include <ctype.h>
#include "rte_json.h"
#include "nm_type.h"
#include "nm_skb.h"
#include "nm_log.h"
#include  "vsecplat_config.h"
#include  "vsecplat_policy.h"

static inline int str_to_mac(const char *bufp, unsigned char *ptr)
{
	int i, j;
	unsigned char val;
	unsigned char c;

	i = 0;
	do {
		j = val = 0;

		/* We might get a semicolon here - not required. */
		if (i && (*bufp == ':')) {
			bufp++;
		}

		do {
			c = *bufp;
			if (((unsigned char)(c - '0')) <= 9) {
				c -= '0';
			} else if (((unsigned char)((c|0x20) - 'a')) <= 5) {
				c = (c|0x20) - ('a'-10);
			} else if (j && (c == ':' || c == 0)) {
				break;
			} else {
				return -1;
			}
			++bufp;
			val <<= 4;
			val += c;
		} while (++j < 2);
		*ptr++ = val;
	} while (++i < NM_MAC_LEN);

	return *bufp; /* Error if we don't end at end of string. */
}

static inline int bad_ip_address(char *str)
{
	char *sp;
	char tmp;
	int dots=0, nums=0;

	if(str==NULL){
    	return -2;
	}
	for(;;){
		sp = str;
		while(*str!='\0'){
			if(*str=='.'){
				if(dots>=3){
					return -1;
				}
				if(*(str+1)=='.'){
					return -1;
				}
				if(*(str+1)=='\0'){
					return -2;
				}
				dots++;
				break;
			}
			if(!isdigit((int)*str)){
				return -1;
			}
			str++;
		}
		if(str-sp>3){
			return -1;
		}
		tmp = *str;
		*str = '\0';
		if(atoi(sp)>255){
			return -1;
		}
		*str=tmp;
		nums++;
		if(*str=='\0'){
			break;
		}
		str++;
	}

	if(nums<4){
		return -1;
	}

	return 0;
}

static unsigned char maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,  0xf8, 0xfc, 0xfe, 0xff};

static inline void masklen2ip(int masklen, struct in_addr *netmask)
{
	u8 *pnt;
	int bit;
	int offset; 

	memset (netmask, 0, sizeof (struct in_addr));
	pnt = (unsigned char *) netmask; 
	
	offset = masklen / 8;
	bit = masklen % 8; 
	
	while (offset--){
		*pnt++ = 0xff;
	} 

	if (bit){
    	*pnt = maskbit[bit];
	}

  return;
}

static int get_num_obj_from_str(struct num_obj_entry *obj, char *str)
{
	char *pos=NULL, *end=NULL, *orig=NULL;
	int val=0;
//	int idx=0;
	int ret=0;

	pos = orig = str;
	end = str+strlen(str)+1;
	while(pos<end){
		if(*pos=='-'){ // range
			obj->type = NUM_RANGE;
			*pos='\0';
			obj->u.range.min = atoi(str);
			pos++;
			obj->u.range.max = atoi(pos);
			break;
	#if 0
		}else if(*pos=='|'){
			obj->type = NUM_GROUP;
			*pos='\0';
			val = atoi(orig);
			if(val==0){
				obj->num_mask = 0xffffffff;
			}
			obj->u.group.num[idx] = val;
			obj->num_mask |= val;
			idx++;
			orig = pos + 1;
	#endif
		}else if(*pos=='\0'){
			if(!obj->type){
				obj->type = NUM_SINGLE;
			}
			val = atoi(orig);
			if(val==0){
				obj->num_mask = 0xffffffff;
			}
			// obj->u.group.num[idx] = val; // when type is NUM_SIGNLE, it's also ok
			obj->u.num = val;
			obj->num_mask |= val;
		}
		pos++;
	}

	return ret;
}

/*
 * return:
 * -1 for error
 *  0 for normal format
 *  1 for not format !(
 * */
static int parse_num_objs(struct list_head *head, char *str)
{
	int format=0, ret=0;
	char *pos=NULL, *end=NULL;
	struct num_obj_entry *obj=NULL;
	
	end = str + strlen(str) + 1;
	if(str[0]=='!'){ // It's !(xxx) format
		format = 1;
		str = str + 2; // skip !(
	}

	pos = str;
	while(pos<end){
		if((*pos=='|')||(*pos=='\0')){
			*pos = '\0';

			obj = (struct num_obj_entry *)malloc(sizeof(struct num_obj_entry));
			if(NULL==obj){
				return -1;
			}
			memset(obj, 0, sizeof(struct num_obj_entry));
			INIT_LIST_HEAD(&obj->list);

			ret = get_num_obj_from_str(obj, str);
			if(ret<0){
				//TODO
				printf("something wrong, str=%s.\n", str);
				return -1;
			}
			list_add_tail(&obj->list, head);	
			str = pos + 1;
		}
		pos = pos + 1;
	}

	return format;
}

static int get_addr_obj_from_str(struct addr_obj_entry *obj, char *str)
{
	char *pos=NULL, *end=NULL, *orig=NULL;
	int val=0;
//	int idx=0;
	struct in_addr addr;
	int star_num=0;
	int ret=0;

	pos = orig = str;
	end = str+strlen(str)+1;
	while(pos<end){
		if(*pos=='-'){ // range
			obj->type = IP_RANGE;
			*pos = '\0';
			if(bad_ip_address(str)){
				nm_log("Bad ip addr %s.\n", str);
			}
			inet_aton(str, &addr);
			obj->u.range.min = addr.s_addr;
			pos++;
			inet_aton(pos, &addr);
			obj->u.range.max = addr.s_addr;
			break;
		}else if(*pos=='/'){ // net mask
			obj->type = IP_NET_MASK;
			*pos = '\0';
			if(bad_ip_address(str)){
				nm_log("Bad ip addr %s.\n", str);
			}
			inet_aton(str, &addr);
			obj->u.net.mask = addr.s_addr;
			if(addr.s_addr==0){
				obj->addr_mask = 0xffffffff;
				break;
			}
			pos++;
			val = atoi(pos);
			if(val==0){
				obj->addr_mask = 0xffffffff;
				break;
			}
			masklen2ip(val, &addr);
			obj->u.net.length = addr.s_addr;
			obj->addr_mask = obj->u.net.mask|(~obj->u.net.length);
			break;
	#if 0
		}else if(*pos=='|'){ // group
			obj->type = IP_GROUP;
			*pos = '\0';
			if(bad_ip_address(orig)){
				nm_log("Bad ip addr %s.\n", orig);
			}
			inet_aton(orig, &addr);
			if(addr.s_addr==0){
				obj->addr_mask = 0xffffffff;
			}
			obj->u.group.ip_addrs[idx] = addr.s_addr;
			obj->addr_mask |= addr.s_addr;
			idx++;
			orig = pos + 1;
	#endif
		}else if(*pos=='*'){ // same as net mask
			obj->type = IP_NET_MASK;
			*pos='0';
			star_num++;
		}else if(*pos=='\0'){
			if(!obj->type){
				obj->type = IP_HOST;
			}
			if(bad_ip_address(orig)){
				nm_log("Bad ip addr %s.\n", orig);
			}
			inet_aton(orig, &addr);
			if(addr.s_addr==0){
				obj->addr_mask = 0xffffffff;
			}
			obj->u.host_ip = addr.s_addr;
			// obj->u.group.ip_addrs[idx] = addr.s_addr;
			obj->addr_mask |= addr.s_addr;
			if(star_num){
				val = (4-star_num)*8;
				if(val==0){
					obj->addr_mask=0xffffffff;
				}
				masklen2ip(val, &addr);
				obj->u.net.length = addr.s_addr;
			}
		}
		pos++;
	}

	return ret;
}

/*
 * return:
 * -1 for error
 *  0 for normal format
 *  1 for not format
 * */
static int parse_addr_objs(struct list_head *head, char *str)
{
	int format=0, ret=0;
	struct addr_obj_entry *obj=NULL;
	char *pos=NULL, *end=NULL;

	end = str + strlen(str) + 1;

	if(str[0]=='!'){ // It's !(xxx) format
		ret = 1;
		str = str + 2; // skip !(
	}
	
	pos = str;
	while(pos<end){
		if((*pos=='|')||(*pos=='\0')){
			*pos = '\0';
			obj = (struct addr_obj_entry *)malloc(sizeof(struct addr_obj_entry));
			if(NULL==obj){
				//TODO
				return -1;
			}
			memset(obj, 0, sizeof(struct addr_obj_entry));	
			ret = get_addr_obj_from_str(obj, str);
			if(ret<0){
				//TODO
				printf("something wrong, str=%s.\n", str);
				return -1;
			}
			INIT_LIST_HEAD(&obj->list);
			list_add_tail(&obj->list, head);	
			str = pos + 1;
		}
		pos = pos + 1;
	}

	return format;
}

static char *persist_json_entry(struct rte_json *json)
{
	char *txt_buf=NULL;
	int len=0;

	txt_buf = (char *)malloc(4096*sizeof(char));
	if(NULL==txt_buf){
		printf("Failed to alloc text buffer.\n");
		return NULL;
	}

	len = rte_persist_json(txt_buf, json, JSON_WITHOUT_FORMAT);
	if(len<0){
		free(txt_buf);
		return NULL;
	}

	return txt_buf;
}

static struct duplicate_rule *global_duplicate_rule=NULL;
static void vsecplat_store_duplicate_rule(char *json_txt)
{
    FILE *file=NULL;
	file = fopen(VSECPLAT_DUPLICATE_RULE_FILE, "w");
	if(NULL==file){
		printf("Failed to open %s\n", VSECPLAT_DUPLICATE_RULE_FILE);
		return;
	}
    if(NULL==json_txt){
	    fprintf(file, "%s", " ");
    }else{
	    fprintf(file, "%s", json_txt);
    }
    fclose(file);
    return;
}

static int parse_duplicate_rule(struct duplicate_rule *duplicate_rule, struct rte_json *json)
{
    struct rte_json *item=NULL, *ips_entry=NULL, *tmp=NULL;
    int entry_num=0, idx=0;
	int ret=0;

    if(NULL==duplicate_rule){
        return -1;
    }

    duplicate_rule->json_txt = persist_json_entry(json);
    if(duplicate_rule->json_txt){
        printf("duplicate: %s\n", duplicate_rule->json_txt);
    }

    item = rte_object_get_item(json, "duplicate_rules"); 
    if(NULL==item){
        goto out;
    }
    if(item->type!=JSON_OBJECT){
        goto out;
    }

    ips_entry = rte_object_get_item(item, "src_ips"); 
    if(NULL!=ips_entry){
        if(ips_entry->type != JSON_ARRAY) {
            goto out;
        }
    }
    entry_num = rte_array_get_size(ips_entry);
    if(entry_num!=0){
        for(idx=0; idx<entry_num; idx++){
            tmp = rte_array_get_item(ips_entry, idx);
            if(NULL==tmp){
                goto out;
            }
            ret = get_addr_obj_from_str(duplicate_rule->src_ip+idx, tmp->u.val_str);
			if(ret<0){
				//TODO
			}
        }
    }

    ips_entry = rte_object_get_item(item, "dst_ips");
    if(NULL!=ips_entry){
        if(ips_entry->type != JSON_ARRAY) {
            goto out; 
        }
    }
    entry_num = rte_array_get_size(ips_entry);
    if(entry_num!=0){
        for(idx=0; idx<entry_num; idx++){
            tmp = rte_array_get_item(ips_entry, idx);
            if(NULL==tmp){
                goto out;
            }
            ret = get_addr_obj_from_str(duplicate_rule->dst_ip+idx, tmp->u.val_str);
			if(ret<0){
				//TODO
			}
        }
    }

    return 0;

out:
    nm_log("Failed to parse duplicate rules.\n");
    return -1;
}

static int add_duplicate_rule(struct duplicate_rule *duplicate_rule)
{
    if(NULL==duplicate_rule){
        return -1;
    }
	nm_mutex_lock(&global_duplicate_rule->mutex);	
    memcpy(global_duplicate_rule->src_ip, duplicate_rule->src_ip, DUPLICATE_IP_MAX_NUM * sizeof(struct addr_obj_entry));
    memcpy(global_duplicate_rule->dst_ip, duplicate_rule->dst_ip, DUPLICATE_IP_MAX_NUM * sizeof(struct addr_obj_entry));
    if(duplicate_rule->json_txt){
        vsecplat_store_duplicate_rule(duplicate_rule->json_txt);
        global_duplicate_rule->json_txt = duplicate_rule->json_txt;
    }
	nm_mutex_unlock(&global_duplicate_rule->mutex);	
    return 0;
}

static int del_duplicate_rule(void)
{
	nm_mutex_lock(&global_duplicate_rule->mutex);	
    memset(global_duplicate_rule->src_ip, 0, DUPLICATE_IP_MAX_NUM * sizeof(struct addr_obj_entry));
    memset(global_duplicate_rule->dst_ip, 0, DUPLICATE_IP_MAX_NUM * sizeof(struct addr_obj_entry));
    free(global_duplicate_rule->json_txt);
    unlink(VSECPLAT_DUPLICATE_RULE_FILE);
	nm_mutex_unlock(&global_duplicate_rule->mutex);	
    return 0;
}

static inline void init_rule_entry(struct rule_entry *entry)
{
	memset(entry, 0, sizeof(struct rule_entry));
	INIT_LIST_HEAD(&entry->sip);	
	INIT_LIST_HEAD(&entry->dip);	
	INIT_LIST_HEAD(&entry->sport);	
	INIT_LIST_HEAD(&entry->dport);	
	INIT_LIST_HEAD(&entry->proto);	
	INIT_LIST_HEAD(&entry->vlanid);	
	return;
}

static struct forward_rules *parse_forward_rules(struct rte_json *json)
{
	struct forward_rules *forward_rules=NULL;
	struct rule_entry *rule_entry=NULL;
	struct rte_json *item=NULL, *entry=NULL, *tmp=NULL;
	int rule_num=0, idx=0;
	int not_flag=0;

	item = rte_object_get_item(json, "forward_rules");
	if(NULL==item){
		return NULL;
	}

	if(item->type!=JSON_ARRAY){
		return NULL;
	}
	rule_num = rte_array_get_size(item);	
	if(rule_num==0){
		return NULL;
	}
	forward_rules = (struct forward_rules *)malloc(sizeof(struct forward_rules));
	if(NULL==forward_rules){
		nm_log("Failed to alloc for forward_rules.\n");
		return NULL;
	}
	memset(forward_rules, 0, sizeof(struct forward_rules));	
	
	forward_rules->rule_num = rule_num;
	forward_rules->rule_entry = (void **)malloc(rule_num*sizeof(void *));
	if(NULL==forward_rules->rule_entry){
		nm_log("Failed to alloc forward_rules->rule_entry.\n");
		free(forward_rules);
		return NULL;
	}

	memset((void *)forward_rules->rule_entry, 0, rule_num*sizeof(void *));

	for(idx=0;idx<rule_num;idx++){
		rule_entry = (struct rule_entry *)malloc(sizeof(struct rule_entry));
		if(NULL==rule_entry){
			nm_log("Failed to alloc rule_entry %d.\n", idx);
			goto out;
		}
		init_rule_entry(rule_entry);
		// memset(rule_entry, 0, sizeof(struct rule_entry));	
		*(forward_rules->rule_entry+idx)=rule_entry;
	}

	for(idx=0;idx<rule_num;idx++){
		rule_entry = *(forward_rules->rule_entry+idx);

		entry = rte_array_get_item(item, idx);
		if(NULL==entry){
			goto out;
		}

		rule_entry->json_txt = persist_json_entry(entry);
	#if 0
		if(NULL!=rule_entry->json_txt){
			printf("rule_entry->json_txt = %s\n", rule_entry->json_txt);
		}
	#endif

		tmp = rte_object_get_item(entry, "id");
		if(NULL!=tmp){
			if(tmp->type != JSON_INTEGER){
				goto out;
			}
			rule_entry->id = tmp->u.val_int;
		}
		
		tmp = rte_object_get_item(entry, "sip");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag=parse_addr_objs(&(rule_entry->sip), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_SIP;
			}
		}

		tmp = rte_object_get_item(entry, "dip");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = parse_addr_objs(&(rule_entry->dip), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_DIP;
			}
		}

		tmp = rte_object_get_item(entry, "sport");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = parse_num_objs(&(rule_entry->sport), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_SPORT;
			}
		}

		tmp = rte_object_get_item(entry, "dport");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = parse_num_objs(&(rule_entry->dport), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_DPORT;
			}
		}

		tmp = rte_object_get_item(entry, "proto");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = parse_num_objs(&(rule_entry->proto), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_PROTO;
			}
		}

		tmp = rte_object_get_item(entry, "vlanid");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = parse_num_objs(&(rule_entry->vlanid), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_VLANID;
			}
		}

		tmp = rte_object_get_item(entry, "conversion");
		if(NULL!=tmp){
			if(tmp->type != JSON_INTEGER){
				goto out;
			}
			rule_entry->conversion = tmp->u.val_int;
		}

		if(rule_entry->conversion){
			tmp = rte_object_get_item(entry, "dst_mac");
			if(NULL!=tmp){
				if(tmp->type != JSON_STRING){
					goto out;
				}
				str_to_mac(tmp->u.val_str, rule_entry->dst_mac);
			}
		}

		tmp = rte_object_get_item(entry, "forward");
		if(NULL!=tmp){
			if(tmp->type != JSON_INTEGER){
				goto out;
			}
			if(0==tmp->u.val_int){ /* 0: drop, 1: forward */
				rule_entry->forward = NM_PKT_DROP;
			}else{
				rule_entry->forward = NM_PKT_FORWARD;
			}
		}

	}
	return forward_rules;

out:
	for(idx=0;idx<forward_rules->rule_num;idx++){
		rule_entry = *(forward_rules->rule_entry + idx);
		if(NULL!=rule_entry){
			free(rule_entry);
		}
	}
	free(forward_rules);
	return NULL;
}

/*
 * return:
 * >0  : success, len of json string
 * -1 : failure 
 * */
int create_policy_response(char *buf, int result, int report_state)
{
	int len;
	struct rte_json *root=NULL;
	struct rte_json *item=NULL;

	// printf("In create_policy_response.\n");

	root = new_json_item();
	if(NULL==root){
		return -1;
	}
	root->type = JSON_OBJECT;

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(root);
		return -1;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = result;
	rte_object_add_item(root, "result", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(root);
		return -1;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = report_state;
	rte_object_add_item(root, "report_state", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(root);
		return -1;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = 0;
	rte_object_add_item(root, "mirror_state", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(root);
		return -1;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = 0;
	rte_object_add_item(root, "guide_state", item);

	len = rte_persist_json(buf, root, JSON_WITHOUT_FORMAT);
	if(len<=0){
		rte_destroy_json(root);
		return -1;
	}

	return len;
}

static int free_rule_entry(struct rule_entry *entry)
{
	struct list_head *pos=NULL, *n=NULL;
	struct addr_obj_entry *addr_obj=NULL;	
	struct num_obj_entry *num_obj=NULL;	

	list_for_each_safe(pos, n, &entry->sip){
		list_del(pos);
		addr_obj=list_entry(pos, struct addr_obj_entry, list);
		free(addr_obj);
	}

	list_for_each_safe(pos, n, &entry->dip){
		list_del(pos);
		addr_obj=list_entry(pos, struct addr_obj_entry, list);
		free(addr_obj);
	}

	list_for_each_safe(pos, n, &entry->sport){
		list_del(pos);
		num_obj=list_entry(pos, struct num_obj_entry, list);
		free(num_obj);
	}

	list_for_each_safe(pos, n, &entry->dport){
		list_del(pos);
		num_obj=list_entry(pos, struct num_obj_entry, list);
		free(num_obj);
	}

	list_for_each_safe(pos, n, &entry->proto){
		list_del(pos);
		num_obj=list_entry(pos, struct num_obj_entry, list);
		free(num_obj);
	}

	list_for_each_safe(pos, n, &entry->vlanid){
		list_del(pos);
		num_obj=list_entry(pos, struct num_obj_entry, list);
		free(num_obj);
	}

	free(entry);

	return 0;
}

static struct recurs_dstmac_head *recurs_dstmac_head=NULL;
static struct forward_rules_head *fw_policy_list=NULL;
#if 0
static int vsecplat_clear_policy(struct forward_rules *forward_rules)
{
	struct list_head *pos=NULL, *n=NULL;
	struct rule_entry *rule_entry=NULL, *pos_entry=NULL;
	int rule_idx=0;

	nm_mutex_lock(&fw_policy_list->mutex);
	for(rule_idx=0;rule_idx<forward_rules->rule_num;rule_idx++){
		rule_entry = *(forward_rules->rule_entry+rule_idx);
		list_for_each_safe(pos, n, &fw_policy_list->list){
			pos_entry = list_entry(pos, struct rule_entry, list);
			if(rule_entry->id == pos_entry->id){
				list_del(pos);
				if(NULL!=pos_entry->json_txt){
					free(pos_entry->json_txt);
				}
				free_rule_entry(pos_entry);
			}
		}
	}
	nm_mutex_unlock(&fw_policy_list->mutex);
	return 0;
}
#endif

static int del_recurs_dst_mac(struct rule_entry *rule_entry)
{
    struct list_head *pos=NULL;
    struct recurs_dstmac *dstmac=NULL;
    if(0==rule_entry->conversion){
        return 0;
    }

	nm_mutex_lock(&recurs_dstmac_head->mutex);	
    dstmac = rule_entry->recurs_dstmac; 
    pos = &dstmac->list;
    list_del(pos);
    free(dstmac);
	nm_mutex_unlock(&recurs_dstmac_head->mutex);	

	return 0;
}

static int vsecplat_del_policy(struct forward_rules *forward_rules)
{
	struct list_head *pos=NULL, *n=NULL;
	struct rule_entry *rule_entry=NULL, *pos_entry=NULL;
	int rule_idx=0;	

	nm_mutex_lock(&fw_policy_list->mutex);
	for(rule_idx=0;rule_idx<forward_rules->rule_num;rule_idx++){
		rule_entry = *(forward_rules->rule_entry+rule_idx);	
		list_for_each_safe(pos, n, &fw_policy_list->list){
			pos_entry = list_entry(pos, struct rule_entry, list);
			if(rule_entry->id == pos_entry->id){
				list_del(pos);
				if(NULL!=pos_entry->json_txt){
					free(pos_entry->json_txt);
				}
				del_recurs_dst_mac(pos_entry);
				free_rule_entry(pos_entry);
			}
		}
	}
	nm_mutex_unlock(&fw_policy_list->mutex);
	for(rule_idx=0;rule_idx<forward_rules->rule_num;rule_idx++){
		free_rule_entry(*(forward_rules->rule_entry+rule_idx));
	}
	return 0;
}

static int add_recurs_dst_mac(struct rule_entry *rule_entry)
{
    struct recurs_dstmac *dstmac=NULL;
    if(0==rule_entry->conversion){
        return 0;
    }

	nm_mutex_lock(&recurs_dstmac_head->mutex);	
    dstmac = (struct recurs_dstmac *)malloc(sizeof(struct recurs_dstmac));	
    if(NULL==dstmac){
	    nm_mutex_unlock(&recurs_dstmac_head->mutex);	
        return -1;
    }
    memset(dstmac, 0, sizeof(struct recurs_dstmac));
    INIT_LIST_HEAD(&dstmac->list);
    memcpy(dstmac->dst_mac, rule_entry->dst_mac, NM_MAC_LEN);
    list_add_tail(&dstmac->list, &recurs_dstmac_head->list);
    rule_entry->recurs_dstmac = dstmac;

	nm_mutex_unlock(&recurs_dstmac_head->mutex);	

    return 0;
}

static int vsecplat_add_policy(struct forward_rules *forward_rules)
{
	int rule_idx=0;
	struct rule_entry *rule_entry=NULL;
#if 0
	// First, delete the rule with the same id
	vsecplat_clear_policy(forward_rules);
#endif

	nm_mutex_lock(&fw_policy_list->mutex);	
	for(rule_idx=0; rule_idx<forward_rules->rule_num; rule_idx++){
		rule_entry = *(forward_rules->rule_entry+rule_idx);
		INIT_LIST_HEAD(&rule_entry->list);
		add_recurs_dst_mac(rule_entry);
		list_add_tail(&rule_entry->list, &fw_policy_list->list);
	}	
	nm_mutex_unlock(&fw_policy_list->mutex);	
	return 0;
}

static void vsecplat_store_policy(void)
{
	struct list_head *head=NULL, *pos=NULL;
	struct rule_entry *rule_entry=NULL;

	FILE *file=NULL;
	file = fopen(VSECPLAT_POLICY_FILE, "w");
	if(NULL==file){
		printf("Failed to open %s\n", VSECPLAT_POLICY_FILE);
		return;
	}
	fprintf(file, "{\"action\":1,\"forward_rules\":[");
	nm_mutex_lock(&fw_policy_list->mutex);
	head = &fw_policy_list->list;
	list_for_each(pos, head){
		rule_entry = list_entry(pos, struct rule_entry, list);
		fprintf(file, "%s", rule_entry->json_txt);
		if(pos->next!=head){
			fprintf(file, ",");
		}
	}
	nm_mutex_unlock(&fw_policy_list->mutex);
	fprintf(file, "]}");

	fclose(file);
	return;
}

int vsecplat_parse_policy(const char *buf)
{
	struct forward_rules *forward_rules=NULL;
    struct duplicate_rule *duplicate_rule=NULL;
	struct rte_json *json=NULL, *item=NULL;
	int action=0;
	int ret=0;	

	json = rte_parse_json(buf);
	if(NULL==json){
		nm_log("Failed to parse policy json.\n");
		return -1;
	}

	if(json->type != JSON_OBJECT){
		nm_log("Policy json format is wrong\n");
		return -1;
	}

	item = rte_object_get_item(json, "action");
	if(NULL==item){
		nm_log("Failed to get action item.\n");
		goto out;
	}
	
	if(item->type != JSON_INTEGER){
		goto out;
	}
	action = item->u.val_int;
	switch(action){
		case NM_CHECK_RULES:  // Maybe no use
			rte_destroy_json(json);
			return 0;
		case NM_DISABLE_MIRROR: //
			rte_destroy_json(json);
			global_vsecplat_config->mirror_state=0;
			return 0;
		case NM_ENABLE_MIRROR:
			rte_destroy_json(json);
			global_vsecplat_config->mirror_state=1;
			return 0;
		case NM_DISABLE_GUIDE:
			rte_destroy_json(json);
			global_vsecplat_config->guide_state=0;
			return 0;
		case NM_ENABLE_GUIDE:
			rte_destroy_json(json);
			global_vsecplat_config->guide_state=1;
			return 0;
        case NM_ADD_DUPLICATE_RULE:
            duplicate_rule = (struct duplicate_rule *)malloc(sizeof(struct duplicate_rule));
            if(NULL==duplicate_rule){
                break;
            }
            memset(duplicate_rule, 0, sizeof(struct duplicate_rule));
            ret=parse_duplicate_rule(duplicate_rule, json);
            if(ret<0){
                //TODO
                free(duplicate_rule);
                rte_destroy_json(json);
                return -1;
            }
            ret=add_duplicate_rule(duplicate_rule); 
            if(ret<0){
                //TODO
                free(duplicate_rule);
                rte_destroy_json(json);
                return -1;
            }
            free(duplicate_rule);
            rte_destroy_json(json);
            return 0;
        case NM_DEL_DUPLICATE_RULE:
            ret=del_duplicate_rule();
            rte_destroy_json(json);
            return 0;
		default:
			break;
	};
#if 0
	if(action==NM_CHECK_RULES){
		rte_destroy_json(json);
		return 0;
	}
#endif
	forward_rules = parse_forward_rules(json);
	if(NULL==forward_rules){
		rte_destroy_json(json);
		return -1;
	}
	switch(action){
		case NM_ADD_RULES:
			ret = vsecplat_add_policy(forward_rules);		
			break;
		case NM_DEL_RULES:
			ret = vsecplat_del_policy(forward_rules);		
			break;
		default:
			break;
	}

	free(forward_rules);
	rte_destroy_json(json);
	if(0==ret){
		vsecplat_store_policy();
	}

	return ret;
out:
	rte_destroy_json(json);
	return -1;
}

/*
 * if match return 1, otherwise return 0 
 **/
static inline int test_match_addr_obj(struct addr_obj_entry *addr_obj, const u32 ip)
{
	// printf("addr_mask=0x%x, type=%d, ip=0x%x\n", addr_obj->addr_mask, addr_obj->type, ip);
	if(addr_obj->addr_mask==0xffffffff){
		return 1;
	}
	if(addr_obj->addr_mask && ((addr_obj->addr_mask&ip)!=ip)){
		return 0;
	}
	switch(addr_obj->type){
		case IP_NULL:
			return 0; // the add_obj is empty
		case IP_HOST:
			// printf("IP_HOST: host_ip=0x%x\n", addr_obj->u.host_ip);
			if((addr_obj->u.host_ip==0)||(addr_obj->u.host_ip==ip)){
				return 1;
			}
			break;
		case IP_NET_MASK:
			// printf("IP_NET_MASK: mask=0x%x, len=0x%x\n", addr_obj->u.net.mask, addr_obj->u.net.length);
			if((addr_obj->u.net.mask & addr_obj->u.net.length)==(ip&addr_obj->u.net.length)){
				return 1;
			}
			break;
		case IP_RANGE:
			// printf("IP_RANGE: min=0x%x, max=0x%x\n", addr_obj->u.range.min, addr_obj->u.range.max);
			if((ntohl(addr_obj->u.range.min)<=ntohl(ip))&&(ntohl(addr_obj->u.range.max)>=ntohl(ip))){
				return 1;
			}
			break;
	#if 0
		case IP_GROUP:
			for(i=0;i<GROUP_ITEM_MAX_NUM;i++){
				if(ip==addr_obj->u.group.ip_addrs[i]){
					return 1;
				}
			}
			break;
	#endif
		default:
			break;
	}
	return 0;
}

static inline int check_addr_obj_list(struct list_head *head, const u32 ip)
{
	struct list_head *pos=NULL;	
	struct addr_obj_entry *entry=NULL;

	list_for_each(pos, head){
		entry = list_entry(pos, struct addr_obj_entry, list);
		if(NULL==entry){
			//  TODO
		}
		if(test_match_addr_obj(entry, ip)){ // match
			return 1;	
		}
	}
	return 0;
}

static inline int test_match_num_obj(struct num_obj_entry *num_obj, u32 num)
{
	if(num_obj->num_mask==0xffffffff){
		return 1;
	}
	if(num_obj->num_mask && ((num_obj->num_mask&num)!=num)){
		return 0;
	}

	switch(num_obj->type){
		case NUM_NULL:
			return 1;
		case NUM_SINGLE:
			if((num_obj->u.num==0)||(num_obj->u.num==num)){
				return 1;
			}
			break;
		case NUM_RANGE:
			if((num_obj->u.range.min<=num)&&(num_obj->u.range.max>=num)){
				return 1;
			}
			break;
	#if 0
		case NUM_GROUP:
			for(i=0;i<GROUP_ITEM_MAX_NUM;i++){
				if(num==num_obj->u.group.num[i]){
					return 1;
				}
			}
			break;
	#endif
		default:
			break;
	}
	return 0;
}

static inline int check_num_obj_list(struct list_head *head, u32 num)
{
	struct list_head *pos=NULL;
	struct num_obj_entry *entry = NULL;

	list_for_each(pos, head){
		entry = list_entry(pos, struct num_obj_entry, list);
		if(NULL==entry){
			// TODO
		}
		if(test_match_num_obj(entry, num)){ // match
			return 1;
		}
	}
	return 0;
}

int check_duplicate_rule(struct nm_skb *skb)
{
    u32 saddr=0, daddr=0;
    int idx=0;
    int src_in_srcips=0, dst_in_srcips=0, dst_in_dstips=0;
    struct addr_obj_entry *ip_obj=NULL;
	saddr = skb->nh.iph->saddr;
	daddr = skb->nh.iph->daddr;
    nm_mutex_lock(&global_duplicate_rule->mutex);
    ip_obj = global_duplicate_rule->src_ip;
    for(idx=0; idx<DUPLICATE_IP_MAX_NUM; idx++){
        if((ip_obj+idx)->type==IP_NULL){ // the ip_obj is empty
            break;
        }
        src_in_srcips = test_match_addr_obj(ip_obj+idx, saddr); 
        if(src_in_srcips==1){ // match
            // printf("match src_in_srcips, idx=%d ", idx);
            break;
        }
    }

    ip_obj = global_duplicate_rule->src_ip;
    for(idx=0; idx<DUPLICATE_IP_MAX_NUM; idx++){
        if((ip_obj+idx)->type==IP_NULL){
            break;
        }
        dst_in_srcips = test_match_addr_obj(ip_obj+idx, daddr); 
        if(dst_in_srcips==1){
            // printf("match dst_in_srcips, idx=%d ", idx);
            break;
        }
    }

    ip_obj = global_duplicate_rule->dst_ip;
    for(idx=0; idx<DUPLICATE_IP_MAX_NUM; idx++){
        if((ip_obj+idx)->type==IP_NULL){
            break;
        }
        dst_in_dstips = test_match_addr_obj(ip_obj+idx, daddr); 
        if(dst_in_dstips==1){
            // printf("match dst_in_dstips, idx=%d ", idx);
            break;
        }
    }
    nm_mutex_unlock(&global_duplicate_rule->mutex);
    if((src_in_srcips==1)&&(dst_in_srcips==0)&&(dst_in_dstips==1)){
        // printf("DISCARD: saddr=0x%x, daddr=0x%x\n", saddr, daddr);
        return NM_PKT_DISCARD;
    }

    return NM_PKT_FORWARD;
}

int check_recursive_packet(struct nm_skb *skb)
{
	int ret = NM_PKT_FORWARD;	
    struct list_head *pos=NULL;
    struct recurs_dstmac *dstmac=NULL;

	nm_mutex_lock(&recurs_dstmac_head->mutex);	
	list_for_each(pos, &recurs_dstmac_head->list){
		dstmac = list_entry(pos, struct recurs_dstmac, list);
        if(!memcmp(dstmac->dst_mac, skb->mac.raw, NM_MAC_LEN)){
	        nm_mutex_unlock(&recurs_dstmac_head->mutex);	
            return NM_PKT_DISCARD;
        }
    }
	nm_mutex_unlock(&recurs_dstmac_head->mutex);	
	return ret;
}

int get_forward_policy(struct nm_skb *skb)
{
	struct list_head *pos;
	struct rule_entry *rule_entry=NULL;
	u32 saddr=0,daddr=0;
	u16 sport=0,dport=0,proto=0,vlanid=0;	

	saddr = skb->nh.iph->saddr;
	daddr = skb->nh.iph->daddr;
	proto = skb->nh.iph->protocol;
	vlanid = ntohs(skb->vlanid);

	if(proto==IPPROTO_UDP){ // UDP
		sport = ntohs(skb->h.uh->source);
		dport = ntohs(skb->h.uh->dest);
	}else if(proto==IPPROTO_TCP){ // TCP
		sport = ntohs(skb->h.th->source);
		dport = ntohs(skb->h.th->dest);
	}

	nm_mutex_lock(&fw_policy_list->mutex);	
	list_for_each(pos, &fw_policy_list->list){
		rule_entry = list_entry(pos, struct rule_entry, list);
		if(!(((rule_entry->rule_not_flag&RULE_NOT_SIP)==RULE_NOT_SIP)^check_addr_obj_list(&rule_entry->sip, saddr))){
			// printf("SIP NOT match.saddr=0x%x\n", saddr);
			continue;
		}
		if(!(((rule_entry->rule_not_flag&RULE_NOT_DIP)==RULE_NOT_DIP)^check_addr_obj_list(&rule_entry->dip, daddr))){
			// printf("DIP NOT match. daddr=0x%x\n", daddr);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_SPORT)==RULE_NOT_SPORT)^check_num_obj_list(&rule_entry->sport, sport))){
			// printf("SPORT NOT match. sport=%d\n", sport);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_DPORT)==RULE_NOT_DPORT)^check_num_obj_list(&rule_entry->dport, dport))){
			// printf("DPORT NOT match. dport=%d\n", dport);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_PROTO)==RULE_NOT_PROTO)^check_num_obj_list(&rule_entry->proto, proto))){
			// printf("PROTO NOT match. proto=0x%x\n", proto);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_VLANID)==RULE_NOT_VLANID)^check_num_obj_list(&rule_entry->vlanid, vlanid))){
			// printf("VLANID NOT match. vlanid=0x%x\n", vlanid);
			continue;
		}

		// reach here, the packet match the policy
		nm_mutex_unlock(&fw_policy_list->mutex);

		// Need to conversion the DMAC
		if((rule_entry->forward==NM_PKT_FORWARD)&&(rule_entry->conversion)){
			memcpy(skb->mac.raw, rule_entry->dst_mac, NM_MAC_LEN);
		}

		return rule_entry->forward;
	}

	nm_mutex_unlock(&fw_policy_list->mutex);	
	return NM_PKT_DROP;
}

int init_recurs_dst_mac_list(void)
{
	recurs_dstmac_head = (struct recurs_dstmac_head *)malloc(sizeof(struct recurs_dstmac_head));
	if(NULL==recurs_dstmac_head){
		return -1;
	}
	memset(recurs_dstmac_head, 0, sizeof(struct recurs_dstmac_head));	
	INIT_LIST_HEAD(&recurs_dstmac_head->list);
	nm_mutex_init(&recurs_dstmac_head->mutex);

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

	global_duplicate_rule = (struct duplicate_rule*)malloc(sizeof(struct duplicate_rule));	
	if(NULL==global_duplicate_rule){
		return -1;
	}
	memset(global_duplicate_rule, 0, sizeof(struct duplicate_rule));
	nm_mutex_init(&global_duplicate_rule->mutex);

	return 0;
}

int vsecplat_load_duplicate_rule(void)
{
    int fd=0; 
    int len=0;
    struct stat stat_buf;
    char *file_buf=NULL;

    if(access(VSECPLAT_DUPLICATE_RULE_FILE, F_OK)){
        return 0; // File not exist
    }

    memset(&stat_buf, 0, sizeof(struct stat));
	stat(VSECPLAT_DUPLICATE_RULE_FILE, &stat_buf);
	len = stat_buf.st_size;

    file_buf = malloc(len);
    if(NULL==file_buf){
        nm_log("Failed to malloc buffer for loading duplicate rules.\n");
        return -1;
    }
    memset(file_buf, 0, len);
    fd = open(VSECPLAT_DUPLICATE_RULE_FILE, O_RDONLY);
    if(fd<0){
        free(file_buf);
        return -1;
    }
    read(fd, file_buf, len);
    close(fd);

    vsecplat_parse_policy(file_buf);
    free(file_buf);
    return 0;
}

int vsecplat_load_policy(void)
{
	int fd=0;
	int len=0;
	struct stat stat_buf;
	char *file_buf=NULL;

	if(access(VSECPLAT_POLICY_FILE, F_OK)){
		return 0; // file not exist
	}

	memset(&stat_buf, 0, sizeof(struct stat));
	stat(VSECPLAT_POLICY_FILE, &stat_buf);
	len = stat_buf.st_size;

	file_buf = malloc(len);
	if(NULL==file_buf){
		printf("Failed to malloc buffer for loading policy.\n");
		return -1;
	}
	memset(file_buf, 0, len);
	fd = open(VSECPLAT_POLICY_FILE, O_RDONLY);
	if(fd<0){
		free(file_buf);
		return -1;
	}
	read(fd, file_buf, len);
	close(fd);

	vsecplat_parse_policy(file_buf);
	free(file_buf);
	return 0;
}

#if 0
int add_test_policy(void)
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
	free(file_buf);
	return 0;
}
#endif
