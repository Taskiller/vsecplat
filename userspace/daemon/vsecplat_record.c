#include "nm_ether.h"
#include "nm_jhash.h"
#include "nm_log.h"
#include "rte_json.h"
#include "vsecplat_record.h"

static struct record_bucket *record_bucket_hash=NULL;

enum{
	JSON_WITHOUT_FORMAT,
	JSON_WITH_FORMAT,
};

#define VSECPLAT_RECORD_HASH_SIZE (1<<10)

int vsecplat_init_record_bucket(void)
{
	int idx=0;
	struct record_bucket *bucket=NULL;
	record_bucket_hash = (struct record_bucket *)malloc(VSECPLAT_RECORD_HASH_SIZE * sizeof(struct record_bucket));

	if(NULL==record_bucket_hash){
		nm_log("Failed to alloc record_bucket_hash.\n");
		return -1;
	}
	memset(record_bucket_hash, 0, VSECPLAT_RECORD_HASH_SIZE * sizeof(struct record_bucket));
	for(idx=0;idx<VSECPLAT_RECORD_HASH_SIZE;idx++){
		bucket = record_bucket_hash + idx;
		INIT_LIST_HEAD(&bucket->list);
		nm_mutex_init(&bucket->mutex);
	}
	return 0;
}

static u32 record_hash(struct record_entry *tmp)
{
	return (jhash_3words(tmp->sip, tmp->dip, tmp->sport|(tmp->dport<<16), 0) % VSECPLAT_RECORD_HASH_SIZE);
}

static inline int test_record_match(const struct record_entry *entry, const struct record_entry *tmp)
{
	return ((entry->sip==tmp->sip)&&(entry->dip==tmp->dip)&&
	   (entry->sport==tmp->sport)&&(entry->dport==tmp->dport)&&
	   (entry->vlanid==tmp->vlanid));
}

int vsecplat_record_pkt(struct nm_skb *skb)
{
	u32 hash=0;
	struct record_bucket *bucket=NULL;
	struct record_entry *tmp=NULL;	
	struct record_entry entry;

	if(ntohs(skb->protocol)!=ETH_P_IP){
		return 0;
	}

	memset(&entry, 0, sizeof(struct record_entry));
	entry.sip = skb->nh.iph->saddr;
	entry.dip = skb->nh.iph->daddr;
	entry.proto = skb->nh.iph->protocol;
	if(entry.proto==6){
		entry.sport = skb->h.uh->source;
		entry.dport = skb->h.uh->dest;
	}else if(entry.proto==17){
		entry.sport = skb->h.th->source;
		entry.dport = skb->h.th->dest;
	}
	entry.vlanid = skb->vlanid;

	hash = record_hash(&entry);
	bucket = record_bucket_hash + hash;
	nm_mutex_lock(&bucket->mutex);
	tmp = LIST_FIND(&bucket->list, test_record_match, struct record_entry *, &entry);
	if(NULL!=tmp){
		tmp->count++;
		tmp->packetsize += skb->len;
	}else{
		tmp = (struct record_entry *)malloc(sizeof(struct record_entry));
		if(NULL==tmp){
			//TODO
			nm_mutex_unlock(&bucket->mutex);
			return -1;
		}
		memcpy(tmp, &entry, sizeof(struct record_entry));
		INIT_LIST_HEAD(&tmp->list);
		tmp->count++;
		tmp->packetsize += skb->len;
		list_add_tail(&bucket->list, &tmp->list);
	}
	nm_mutex_unlock(&bucket->mutex);
	return 0;
}

static struct rte_json *record_entry_to_json(struct record_entry *entry)
{
	struct rte_json *obj=NULL, *item=NULL;
	char *buf=NULL;

	obj = new_json_item();
	if(NULL==obj){
		return NULL;
	}
	obj->type = JSON_OBJECT;

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_STRING;
	buf = (char *)malloc(NM_IP_LEN);
	if(NULL==buf){
		rte_destroy_json(obj);
		return NULL;
	}
	memset(buf, 0, NM_IP_LEN);
	sprintf(buf, "%s", inet_ntoa(*(struct in_addr *)&entry->sip));
	item->u.val_str = buf;
	rte_object_add_item(obj, "sip", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_STRING;
	buf = (char *)malloc(NM_IP_LEN);
	if(NULL==buf){
		rte_destroy_json(obj);
		return NULL;
	}
	memset(buf, 0, NM_IP_LEN);
	sprintf(buf, "%s", inet_ntoa(*(struct in_addr *)&entry->dip));
	item->u.val_str = buf;
	rte_object_add_item(obj, "dip", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->sport;
	rte_object_add_item(obj, "sport", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->dport;
	rte_object_add_item(obj, "dport", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->proto;
	rte_object_add_item(obj, "proto", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->vlanid;
	rte_object_add_item(obj, "vlanid", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->count;
	rte_object_add_item(obj, "count", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = entry->packetsize;
	rte_object_add_item(obj, "packetsize", item);

	return obj;
}

#define MAX_REPORT_ITEM 64
char *persist_record(void)
{
	char *str;
	int idx=0;
	int item_count=0;
	struct rte_json *root=NULL, *array=NULL, *item=NULL;
	struct list_head *pos=NULL, *tmp=NULL;
	struct record_bucket *bucket=NULL;
	struct record_entry *entry=NULL;

	root = new_json_item();
	if(NULL==root){
		return NULL;
	}
	root->type = JSON_OBJECT;

	array = new_json_item();
	if(NULL==array){
		goto out;
	}
	array->type = JSON_ARRAY;

	for(idx=0;idx<VSECPLAT_RECORD_HASH_SIZE;idx++){
		bucket = record_bucket_hash + idx;	
		nm_mutex_lock(&bucket->mutex);
		list_for_each_safe(pos, tmp, &bucket->list){
			entry = list_entry(pos, struct record_entry, list);
			if(entry->count==0){
				list_del(&entry->list);
				free(entry);
			}else{
				// persist the record to json
				item = record_entry_to_json(entry);
				if(NULL==item){
					nm_log("Failed to create json item.\n");
					nm_mutex_unlock(&bucket->mutex);
					goto out;
				}
				rte_array_add_item(array, item);
				item_count++;
				if(item_count>=MAX_REPORT_ITEM){
					nm_mutex_unlock(&bucket->mutex);
					goto count_break;
				}
			}
		}
		nm_mutex_unlock(&bucket->mutex);
	}

count_break:
	if(0==rte_array_get_size(array)){
		goto out;
	}

	rte_object_add_item(root, "record_list", array);
	str = rte_serialize_json(root, JSON_WITHOUT_FORMAT);

	// printf("persist_record=%s\n", str);
out:
	rte_destroy_json(root);
	return str;
}

int vsecplat_test_record(void)
{
	struct record_entry entry;
	struct record_entry *tmp=NULL;
	struct record_bucket *bucket=NULL;
	u32 hash=0;

	// char *str=NULL;

	memset(&entry, 0, sizeof(struct record_entry));
	entry.sip = 0x10000a;
	entry.dip = 0x20000b;
	entry.proto = 17;
	entry.sport = 2025;
	entry.dport = 123;
	entry.vlanid = 0;

	hash = record_hash(&entry);
	bucket = record_bucket_hash + hash;
	nm_mutex_lock(&bucket->mutex);
	tmp = LIST_FIND(&bucket->list, test_record_match, struct record_entry *, &entry);
	if(NULL!=tmp){
		tmp->count++;
	}else{
		tmp = (struct record_entry *)malloc(sizeof(struct record_entry));
		if(NULL==tmp){
			//TODO
			nm_mutex_unlock(&bucket->mutex);
			return -1;
		}
		memcpy(tmp, &entry, sizeof(struct record_entry));
		INIT_LIST_HEAD(&tmp->list);
		tmp->count++;
		list_add_tail(&bucket->list, &tmp->list);
	}
	nm_mutex_unlock(&bucket->mutex);

	hash = record_hash(&entry);
	bucket = record_bucket_hash + hash;
	nm_mutex_lock(&bucket->mutex);
	tmp = LIST_FIND(&bucket->list, test_record_match, struct record_entry *, &entry);
	if(NULL!=tmp){
		tmp->count++;
	}else{
		tmp = (struct record_entry *)malloc(sizeof(struct record_entry));
		if(NULL==tmp){
			//TODO
			nm_mutex_unlock(&bucket->mutex);
			return -1;
		}
		memcpy(tmp, &entry, sizeof(struct record_entry));
		INIT_LIST_HEAD(&tmp->list);
		tmp->count++;
		list_add_tail(&bucket->list, &tmp->list);
	}
	nm_mutex_unlock(&bucket->mutex);

#if 0
	str = persist_record();

	printf("%s", str);
#endif
	return 0;
}

