#include "nm_jhash.h"
#include "vsecplat_record.h"

static struct record_bucket *record_bucket_hash=NULL;

#define VSECPLAT_RECORD_HASH_SIZE (1<<10)

int vsecplat_init_record_bucket(void)
{
	int idx=0;
	struct record_bucket *bucket=NULL;
	record_bucket_hash = (struct record_bucket *)malloc(VSECPLAT_RECORD_HASH_SIZE * sizeof(struct record_bucket));

	if(NULL==record_bucket_hash){
		// TODO
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
	struct record_entry *entry=NULL;
	struct record_entry *tmp=NULL;	

	tmp = (struct record_entry *)malloc(sizeof(struct record_entry));
	if(NULL==tmp){
		//TODO
		return -1;
	}
	memset(&tmp, 0, sizeof(struct record_entry));
	INIT_LIST_HEAD(&tmp->list);
	tmp->sip = skb->nh.iph->saddr;
	tmp->dip = skb->nh.iph->daddr;
	tmp->proto = skb->nh.iph->protocol;
	if(tmp->proto==6){
		tmp->sport = skb->h.uh->source;
		tmp->dport = skb->h.uh->dest;
	}else if(tmp->proto==17){
		tmp->sport = skb->h.th->source;
		tmp->dport = skb->h.th->dest;
	}
	tmp->vlanid = skb->vlanid;

	hash = record_hash(tmp);
	bucket = record_bucket_hash + hash;
	nm_mutex_lock(&bucket->mutex);
	entry = LIST_FIND(&record_bucket_hash->list, test_record_match, struct record_entry *, tmp);
	if(NULL!=entry){
		entry->count++;
	}else{
		tmp->count++;
		list_add_tail(&bucket->list, &tmp->list);
	}
	nm_mutex_unlock(&bucket->mutex);
	return 0;
}



