#include "nm_ether.h"
#include "nm_jhash.h"
#include "nm_log.h"
#include "rte_json.h"
#include "vsecplat_record.h"

static struct record_bucket *record_bucket_hash=NULL;
static time_t last_report_time;

#define VSECPLAT_RECORD_HASH_SIZE (1<<10)

struct list_head global_record_json_list;

int clear_global_record_json_list(void)
{
	struct list_head *pos=NULL, *tmp=NULL;
	struct record_json_item *item=NULL;
	list_for_each_safe(pos, tmp, &global_record_json_list){
		item = list_entry(pos, struct record_json_item, list);	
		list_del(&item->list);
		rte_destroy_json(item->root);
		if(item->json_str){
			free(item->json_str);
		}
		free(item);
	}
	return 0;
}

int vsecplat_init_record_bucket(void)
{
	int idx=0;
	struct timeval report_time;
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

	gettimeofday(&report_time, NULL);
	last_report_time = report_time.tv_sec;

	INIT_LIST_HEAD(&global_record_json_list);

	return 0;
}

static u32 record_hash(struct record_entry *tmp)
{
	return (jhash_3words(tmp->sip, tmp->dip, tmp->sport|(tmp->dport<<16), 0) % VSECPLAT_RECORD_HASH_SIZE);
}

static inline int test_record_match(const struct record_entry *entry, const struct record_entry *tmp)
{
#if 0
	return ((entry->sip==tmp->sip)&&(entry->dip==tmp->dip)&&
	   (entry->sport==tmp->sport)&&(entry->dport==tmp->dport)&&
	   (entry->vlanid==tmp->vlanid));
#endif
	return ((entry->sip==tmp->sip)&&(entry->dip==tmp->dip)&&
	   (entry->sport==tmp->sport)&&(entry->dport==tmp->dport));
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
	if(entry.proto==IPPROTO_UDP){
		entry.sport = skb->h.uh->source;
		entry.dport = skb->h.uh->dest;
	}else if(entry.proto==IPPROTO_TCP){
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
		memset(tmp, 0, sizeof(struct record_entry));
		memcpy(tmp, &entry, sizeof(struct record_entry));
		INIT_LIST_HEAD(&tmp->list);
		tmp->count++;
		tmp->packetsize = skb->len;
		list_add_tail(&tmp->list, &bucket->list);
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
	item->u.val_int = ntohs(entry->sport);
	rte_object_add_item(obj, "sport", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(obj);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = ntohs(entry->dport);
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

// #define MAX_REPORT_ITEM 64
#define MAX_REPORT_ITEM 10
struct record_json_item *new_record_json_item(void)
{
	struct record_json_item *record_json_item=NULL;
	struct rte_json *item=NULL;
	struct timeval cur_time;

	gettimeofday(&cur_time, NULL);

	record_json_item = (struct record_json_item *)malloc(sizeof(struct record_json_item));
	if(NULL==record_json_item){
		return NULL;
	}
	memset(record_json_item, 0, sizeof(struct record_json_item));
	INIT_LIST_HEAD(&record_json_item->list);	

	record_json_item->root = new_json_item();
	if(NULL==record_json_item->root){
		rte_destroy_json(record_json_item->root);
		return NULL;
	}
	record_json_item->root->type = JSON_OBJECT;

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(record_json_item->root);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = last_report_time;
	rte_object_add_item(record_json_item->root, "starttime", item);

	item = new_json_item();
	if(NULL==item){
		rte_destroy_json(record_json_item->root);
		return NULL;
	}
	item->type = JSON_INTEGER;
	item->u.val_int = cur_time.tv_sec;
	rte_object_add_item(record_json_item->root, "endtime", item);

	record_json_item->array = new_json_item();
	if(NULL==record_json_item->array){
		rte_destroy_json(record_json_item->root);
		rte_destroy_json(record_json_item->array);
		return NULL;
	}
	record_json_item->array->type = JSON_ARRAY;

	return record_json_item;
}

int vsecplat_persist_record(void)
{
	int idx=0;
	int item_count=0;
	struct rte_json *item=NULL;
	struct list_head *pos=NULL, *tmp=NULL;
	struct record_bucket *bucket=NULL;
	struct record_entry *entry=NULL;
	struct record_json_item *record_json_item=NULL;

	struct timeval report_time;

	record_json_item = new_record_json_item();
	if(NULL==record_json_item){
		nm_log("Failed to create record_json_item.\n");
		return -1;
	}

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
					rte_destroy_json(record_json_item->array);
					free(record_json_item);
					goto out;
				}
				rte_array_add_item(record_json_item->array, item);
				entry->count=0;
				entry->packetsize=0;
				item_count++;
				if(item_count>=MAX_REPORT_ITEM){
					item_count=0;
					rte_object_add_item(record_json_item->root, "record_list", record_json_item->array);
					list_add_tail(&record_json_item->list, &global_record_json_list);
					record_json_item = new_record_json_item();
					if(NULL==record_json_item){
						nm_log("Failed to alloc record_json_item.\n");
						nm_mutex_unlock(&bucket->mutex);
						goto out;
					}
				}
			}
		}
		nm_mutex_unlock(&bucket->mutex);
	}

	if(item_count>0){
		rte_object_add_item(record_json_item->root, "record_list", record_json_item->array);
		list_add_tail(&record_json_item->list, &global_record_json_list);
	}

    if(list_empty(&global_record_json_list)){
		rte_object_add_item(record_json_item->root, "record_list", record_json_item->array);
    	list_add_tail(&record_json_item->list, &global_record_json_list);
    }

	gettimeofday(&report_time, NULL);
	last_report_time = report_time.tv_sec;

	return 0;
out:
	clear_global_record_json_list();
	return -1;
}

int vsecplat_test_record(void)
{
	struct record_entry entry;
	struct record_entry *tmp=NULL;
	struct record_bucket *bucket=NULL;
	u32 hash=0;
	int i=0;

	memset(&entry, 0, sizeof(struct record_entry));
	entry.sip = 0x10000a;
	entry.dip = 0x20000b;
	entry.proto = 17;
	entry.sport = 2025;
	entry.dport = 123;
	entry.vlanid = 0;
	entry.packetsize = 1251;

	for(i=0;i<128;i++){
		entry.sip++;
		entry.vlanid = i;
		hash = record_hash(&entry);
		bucket = record_bucket_hash + hash;
		nm_mutex_lock(&bucket->mutex);
		tmp = LIST_FIND(&bucket->list, test_record_match, struct record_entry *, &entry);
		if(NULL!=tmp){
			tmp->count++;
			tmp->packetsize += entry.packetsize;
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
			tmp->packetsize = entry.packetsize;
			list_add_tail(&tmp->list, &bucket->list);
		}
		nm_mutex_unlock(&bucket->mutex);
	}
	return 0;
}

