#ifndef __NM_SKB_H__
#define __NM_SKB_H__
struct nm_skb{
	struct nm_skb *next;
	struct nm_skb *prev;
	unsigned int size;
	unsigned int len;
	unsigned int type;
	unsigned int buf_type;
	int i_ifidx;
	int o_ifidx;
	unsigned char *head;
	unsigned char *data;
	unsigned char *tail;
	unsigned char *end;
	
	/* TCP/UDP head */
	union{
		
	}h;

	/* IP head */
	union{

	}nh;

	union{
		unsigned char *raw;
	}mac;
};

enum{
	NETMAP_BUF,
	MEMORY_BUF
};

#define NM_BUF_SIZE 2048
#define NM_SKB_SPACE 128
#define NM_BUF_RESERVED 64

struct nm_skb *nm_recv(void);
int nm_send(struct nm_skb *skb);
struct nm_skb *nm_alloc_skb(void);
void nm_free_skb(struct nm_skb *skb);

#endif
