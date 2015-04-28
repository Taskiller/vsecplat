#ifndef __VSECPLAT_INTERFACE_H__
#define __VSECPLAT_INTERFACE_H__

struct vsecplat_interface{
	struct list_head list;
	char name[NM_NAME_LEN];
	char mac[NM_MAC_LEN];
};

int vsecplat_get_all_interface(void);
struct vsecplat_interface *vsecplat_get_interface_by_mac(char *mac);

#endif
