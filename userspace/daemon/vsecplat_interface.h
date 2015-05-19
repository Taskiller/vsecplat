#ifndef __VSECPLAT_INTERFACE_H__
#define __VSECPLAT_INTERFACE_H__

#include "nm_type.h"
#include "nm_list.h"

struct vsecplat_interface{
	struct list_head list;
	char name[NM_NAME_LEN];
	char mac[NM_MAC_LEN];
};

int init_vsecplat_interface_list(void);
struct vsecplat_interface *vsecplat_get_interface_by_mac(char *mac);
struct vsecplat_interface *vsecplat_get_interface_by_name(char *name);

int setup_mgt_interface(void);
int setup_dp_interfaces(void);

#endif
