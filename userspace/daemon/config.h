#ifndef __CONFIG_H__
#define __CONFIG_H__
#include "nm_type.h"

struct mgt_cfg{
	char name[NM_NAME_LEN];
	char ipaddr[NM_ADDR_STR_LEN];
	char mac[NM_ADDR_STR_LEN];
};

struct serv_cfg{
	char ipaddr[NM_ADDR_STR_LEN];
	int port;	
};
struct vm_list{
	char name[NM_NAME_LEN];
	char inport[NM_ADDR_STR_LEN];
};

struct outport_cfg{
	// char name[NM_NAME_LEN];
	char mac[NM_ADDR_STR_LEN];
};

struct config{
	struct mgt_cfg *mgt_cfg;
	struct serv_cfg *serv_cfg;
	struct vm_list *vm_list;
	int vm_count;
	struct outport_cfg *outport_cfg;
};

struct config *parse_vsecplat_config(void);
#endif
