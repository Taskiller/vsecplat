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

/*
 * return:
 * -1 for error
 *  0 for normal format
 *  1 for not format
 * */
static int get_num_obj_from_str(struct num_obj *obj, char *str)
{
	char *pos=NULL, *end=NULL, *orig=NULL;
	int val=0, idx=0;
	int ret=0;
	if(str[0]=='!'){
		ret=1;
		str = str + 1;
	}
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
		}else if(*pos=='\0'){
			if(!obj->type){
				obj->type = NUM_SINGLE;
			}
			val = atoi(orig);
			if(val==0){
				obj->num_mask = 0xffffffff;
			}
			obj->u.group.num[idx] = val; // when type is NUM_SIGNLE, it's also ok
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
 *  1 for not format
 * */
static int get_addr_obj_from_str(struct addr_obj *obj, char *str)
{
	char *pos=NULL, *end=NULL, *orig=NULL;
	int val=0, idx=0;
	struct in_addr addr;
	int ret=0;
	int star_num=0;

	if(str[0]=='!'){ // not format
		ret = 1;
		str = str + 1;
	}
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
			obj->u.group.ip_addrs[idx] = addr.s_addr;
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

static char *persist_policy_entry(struct rte_json *json)
{
	char *txt_buf=NULL;
	int len=0;

	txt_buf = (char *)malloc(512*sizeof(char));
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
		memset(rule_entry, 0, sizeof(struct rule_entry));	
		*(forward_rules->rule_entry+idx)=rule_entry;
	}

	for(idx=0;idx<rule_num;idx++){
		rule_entry = *(forward_rules->rule_entry+idx);
		entry = rte_array_get_item(item, idx);
		if(NULL==entry){
			goto out;
		}


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
			not_flag=get_addr_obj_from_str(&(rule_entry->sip), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_SIP;
			}
		}

		tmp = rte_object_get_item(entry, "dip");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = get_addr_obj_from_str(&(rule_entry->dip), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_DIP;
			}
		}

		tmp = rte_object_get_item(entry, "sport");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = get_num_obj_from_str(&(rule_entry->sport), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_SPORT;
			}
		}

		tmp = rte_object_get_item(entry, "dport");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = get_num_obj_from_str(&(rule_entry->dport), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_DPORT;
			}
		}

		tmp = rte_object_get_item(entry, "proto");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = get_num_obj_from_str(&(rule_entry->proto), tmp->u.val_str);
			if(not_flag>0){
				rule_entry->rule_not_flag |= RULE_NOT_PROTO;
			}
		}

		tmp = rte_object_get_item(entry, "vlanid");
		if(NULL!=tmp){
			if(tmp->type != JSON_STRING){
				goto out;
			}
			not_flag = get_num_obj_from_str(&(rule_entry->vlanid), tmp->u.val_str);
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

		rule_entry->json_txt = persist_policy_entry(entry);
	#if 0
		if(NULL!=rule_entry->json_txt){
			printf("rule_entry->json_txt = %s\n", rule_entry->json_txt);
		}
	#endif
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

static struct forward_rules_head *fw_policy_list=NULL;
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
				free(pos_entry);
			}
		}
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
		rule_entry = *(forward_rules->rule_entry+rule_idx);	
		list_for_each_safe(pos, n, &fw_policy_list->list){
			pos_entry = list_entry(pos, struct rule_entry, list);
			if(rule_entry->id == pos_entry->id){
				list_del(pos);
				if(NULL!=pos_entry->json_txt){
					free(pos_entry->json_txt);
				}
				free(pos_entry);
			}
		}
	}
	nm_mutex_unlock(&fw_policy_list->mutex);
	for(rule_idx=0;rule_idx<forward_rules->rule_num;rule_idx++){
		free(*(forward_rules->rule_entry+rule_idx));
	}
	return 0;
}

static int vsecplat_add_policy(struct forward_rules *forward_rules)
{
	int rule_idx=0;
	struct rule_entry *rule_entry=NULL;

	// First, delete the rule with the same id
	vsecplat_clear_policy(forward_rules);

	nm_mutex_lock(&fw_policy_list->mutex);	
	for(rule_idx=0; rule_idx<forward_rules->rule_num; rule_idx++){
		rule_entry = *(forward_rules->rule_entry+rule_idx);
		INIT_LIST_HEAD(&rule_entry->list);
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
		case NM_CHECK_RULES:
			rte_destroy_json(json);
			return 0;
		case NM_DISABLE_MIRROR:
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
		default:
			break;
	}
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
static inline int test_match_addr_obj(struct addr_obj *addr_obj, const u32 ip)
{
	int i=0;
	// printf("addr_mask=0x%x, type=%d, ip=0x%x\n", addr_obj->addr_mask, addr_obj->type, ip);
	if(addr_obj->addr_mask==0xffffffff){
		return 1;
	}
	if(addr_obj->addr_mask && ((addr_obj->addr_mask&ip)!=ip)){
		return 0;
	}
	switch(addr_obj->type){
		case IP_NULL:
			return 1;
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
			if((addr_obj->u.range.min<=ip)&&(addr_obj->u.range.max>=ip)){
				return 1;
			}
			break;
		case IP_GROUP:
			for(i=0;i<GROUP_ITEM_MAX_NUM;i++){
				if(ip==addr_obj->u.group.ip_addrs[i]){
					return 1;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

static inline int test_match_num_obj(struct num_obj *num_obj, u32 num)
{
	int i=0;

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
		case NUM_GROUP:
			for(i=0;i<GROUP_ITEM_MAX_NUM;i++){
				if(num==num_obj->u.group.num[i]){
					return 1;
				}
			}
			break;
		default:
			break;
	}

	return 0;
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
		if(!(((rule_entry->rule_not_flag&RULE_NOT_SIP)==RULE_NOT_SIP)^test_match_addr_obj(&rule_entry->sip, saddr))){
			// printf("SIP NOT match.\n");
			continue;
		}
		if(!(((rule_entry->rule_not_flag&RULE_NOT_DIP)==RULE_NOT_DIP)^test_match_addr_obj(&rule_entry->dip, daddr))){
			// printf("DIP NOT match.\n");
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_SPORT)==RULE_NOT_SPORT)^test_match_num_obj(&rule_entry->sport, sport))){
			// printf("SPORT NOT match. rule_entry->sport: mask=%d, num=%d, sport=%d\n", rule_entry->sport.num_mask, rule_entry->sport.u.num, sport);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_DPORT)==RULE_NOT_DPORT)^test_match_num_obj(&rule_entry->dport, dport))){
			// printf("DPORT NOT match. rule_entry->dport: mask=%d, num=%d, dport=%d\n", rule_entry->dport.num_mask, rule_entry->dport.u.num, dport);
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_PROTO)==RULE_NOT_PROTO)^test_match_num_obj(&rule_entry->proto, proto))){
			// printf("PROTO NOT match.\n");
			continue;
		}

		if(!(((rule_entry->rule_not_flag&RULE_NOT_VLANID)==RULE_NOT_VLANID)^test_match_num_obj(&rule_entry->vlanid, vlanid))){
			// printf("VLANID NOT match.\n");
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
