#ifndef __VSECPLAT_CONFIG_H__
#define __VSECPLAT_CONFIG_H__
#include "nm_type.h"

struct mgt_cfg{
	char name[NM_NAME_LEN];
	char ipaddr[NM_ADDR_STR_LEN];
//	char mac[NM_MAC_LEN];
};

struct serv_cfg{
	char ipaddr[NM_ADDR_STR_LEN];
	int port;	
};

struct inport_list{
//	char mac[NM_MAC_LEN];
	char name[NM_NAME_LEN];
};

struct outport_list{
	char name[NM_NAME_LEN];
	// char mac[NM_MAC_LEN];
};

struct vsecplat_config{
	struct mgt_cfg *mgt_cfg;
	struct serv_cfg *serv_cfg;
	int inport_num;
	struct inport_list *inport_list;
	int outport_num;
	struct outport_list *outport_list;
};

extern struct vsecplat_config *global_vsecplat_config;
int parse_vsecplat_config(void);
#endif
