#include "vsecplat_interface.h"

static struct list_head vsecplat_interface_list;

static char *get_interface_name(char *name, char *p)
{
	int namestart=0, nameend=0;
	while (isspace(p[namestart]))
		namestart++;
	nameend=namestart;
	while (p[nameend] && p[nameend]!=':' && !isspace(p[nameend]))
		nameend++;
	if (p[nameend]==':') {
		if ((nameend-namestart)<IFNAMSIZ) {
			memcpy(name,&p[namestart],nameend-namestart);
			name[nameend-namestart]='\0';
			p=&p[nameend];
		} else {
			/* Interface name too large */
			name[0]='\0';
		}
	} else {
		/* trailing ':' not found - return empty */
		name[0]='\0';
	}
	return p + 1;
}

int init_vsecplat_interface_list(void)
{
	int sock=0;
	struct vsecplat_interface *ifp=NULL;	
	FILE *fp=NULL;
	char buf[1024+1];

	INIT_LIST_HEAD(&vsecplat_interface_list);
	if((sock = socket(AF_INET, SOCK_DGRAM, 0))<0) {
		//TODO
		return -1;
	}

	fp = fopen("/proc/net/dev", "r");
	if(NULL==fp){
		close(sock);
		return -1;
	}

	/* Drop header lines */
	fgets(buf, 1024, fp);
	fgets(buf, 1024, fp);
	
	while(fgets(buf, 1024, fp)!=NULL){
		char name[NM_NAME_LEN];
		struct ifreq ifr;
		buf[1024] = '\0';
		get_interface_name(name, buf);
		memset(&ifr, 0, sizeof(struct ifreq));
		
		strcpy(ifr.ifr_name, name);
		ioctl(sock, SIOCGIFHWADDR, &ifr);
		ifp = (struct vsecplat_interface *)malloc(sizeof(struct vsecplat_interface));
		if(NULL==ifp){
			printf("Failed to alloc interface.\n");
			goto out;
		}
		memset(ifp, 0, sizeof(struct vsecplat_interface));
		INIT_LIST_HEAD(&ifp->list);
#if 0
		printf("interface name: %s, ifr_hwaddr.sa_data=%x:%x:%x:%x:%x:%x\n", 
					name,
					ifr.ifr_hwaddr.sa_data[0] & 0xff,
					ifr.ifr_hwaddr.sa_data[1] & 0xff,
					ifr.ifr_hwaddr.sa_data[2] & 0xff,
					ifr.ifr_hwaddr.sa_data[3] & 0xff ,
					ifr.ifr_hwaddr.sa_data[4] & 0xff,
					ifr.ifr_hwaddr.sa_data[5] & 0xff);
#endif
		strncpy(ifp->name, name, NM_NAME_LEN);
		memcpy(ifp->mac, ifr.ifr_hwaddr.sa_data, NM_MAC_LEN);
		list_add_tail(&ifp->list, &vsecplat_interface_list);
	}

	close(sock);
	fclose(fp);
	return 0;
out:
	close(sock);
	fclose(fp);
	return -1;
}

struct vsecplat_interface *vsecplat_get_interface_by_mac(char *mac)
{
	struct vsecplat_interface *ifp=NULL;		
	list_for_each_entry(ifp, &vsecplat_interface_list, list){
		if(0==memcmp(ifp->mac, mac, NM_MAC_LEN)){
			return ifp;
		}
	}
	return NULL;
}

