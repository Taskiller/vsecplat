#ifndef __VSECPLAT_CONFIG_H__
#define __VSECPLAT_CONFIG_H__
#include "nm_type.h"
#include "nm_dev.h"

struct mgt_cfg{
	char name[NM_NAME_LEN];
	char ipaddr[NM_ADDR_STR_LEN];
	int tcpport;
};

struct serv_cfg{
	char ipaddr[NM_ADDR_STR_LEN];
	int udpport;
};

struct inport_desc{
	char name[NM_NAME_LEN];
	struct nm_dev *dev;
};

struct outport_desc{
	char name[NM_NAME_LEN];
	struct nm_dev *dev;
#if 0
	int change_dst_mac;
	unsigned char dst_mac[NM_MAC_LEN];
#endif
};

struct vsecplat_config{
	struct mgt_cfg *mgt_cfg;
	struct serv_cfg *serv_cfg;
	int inport_num;
	struct inport_desc *inport_desc_array;
	int outport_num;
	struct outport_desc *outport_desc_array;
	int time_interval;
};

#define VSECPLAT_REPORT_INTERVAL 6 // 每隔60秒报告一次统计信息

extern struct vsecplat_config *global_vsecplat_config;
int parse_vsecplat_config(void);
#endif
