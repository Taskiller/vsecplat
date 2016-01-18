#ifndef __VSECPLAT_CONFIG_H__
#define __VSECPLAT_CONFIG_H__
#include "nm_type.h"
#include "nm_dev.h"

struct mgt_cfg{
	char name[NM_NAME_LEN];
	char ipaddr[NM_ADDR_STR_LEN];
	char gateway[NM_ADDR_STR_LEN];
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
};

struct vsecplat_config{
	struct mgt_cfg *mgt_cfg;
	struct serv_cfg *serv_cfg;
	int inport_num;
	struct inport_desc *inport_desc_array;
	int outport_num;
	struct outport_desc *outport_desc_array;
	int time_interval;
	int isencrypted;
	int mirror_state;
	int guide_state;
};

#define VSECPLAT_REPORT_INTERVAL 60 // 每隔60秒报告一次统计信息

extern struct vsecplat_config *global_vsecplat_config;
int parse_vsecplat_config(void);
#endif
