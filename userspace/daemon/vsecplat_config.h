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

struct inport_desc{
//	char mac[NM_MAC_LEN];
	char name[NM_NAME_LEN];
};

struct outport_desc{
	char name[NM_NAME_LEN];
	// char mac[NM_MAC_LEN];
};

struct vsecplat_config{
	struct mgt_cfg *mgt_cfg;
	struct serv_cfg *serv_cfg;
	int inport_num;
	struct inport_desc *inport_desc_array;
	int outport_num;
	struct outport_desc *outport_desc_array;
};

extern struct vsecplat_config *global_vsecplat_config;
int parse_vsecplat_config(void);
#endif
