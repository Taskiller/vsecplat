#include "rte_json.h"
#include "nm_log.h"
#include "vsecplat_config.h"

#define VSECPLATFORM_CFG_FILE "/mnt/config.json"
// #define VSECPLATFORM_CFG_FILE "./config.json"
struct vsecplat_config *global_vsecplat_config;

int parse_vsecplat_config(void)
{
	int fd=0;
	int len=0;
	struct stat stat_buf;
	char *file_buf=NULL;
	struct rte_json *json=NULL, *item=NULL, *entry=NULL;
	struct rte_json *tmp=NULL;
	int idx=0;

	struct mgt_cfg *mgt_cfg=NULL;
	struct serv_cfg *serv_cfg =NULL;

	fd = open(VSECPLATFORM_CFG_FILE, O_RDONLY);
	if(fd<0){
		nm_log("Failed to open vsecplat config file.\n");
		return -1;
	}

	memset(&stat_buf, 0, sizeof(struct stat));
	if(fstat(fd, &stat_buf)<0){
		nm_log("Failed to get file length.\n");
		close(fd);
		return -1;
	}
	len = stat_buf.st_size;
	file_buf = malloc(len);
	if(NULL==file_buf){
		close(fd);
		return -1;
	}
	memset(file_buf, 0, len);
	
	read(fd, file_buf, len);
	close(fd);

	global_vsecplat_config = (struct vsecplat_config *)malloc(sizeof(struct vsecplat_config));		
	if(NULL==global_vsecplat_config){
		free(file_buf);
		return -1;
	}
	memset(global_vsecplat_config, 0, sizeof(struct vsecplat_config));
	global_vsecplat_config->time_interval = VSECPLAT_REPORT_INTERVAL;
	global_vsecplat_config->mirror_state = 1;
	global_vsecplat_config->guide_state = 1;

	json = rte_parse_json(file_buf);
	if(NULL==json){
		free(file_buf);
		free(global_vsecplat_config);
		return -1;
	}

	item = rte_object_get_item(json, "mgt_cfg");	
	if(NULL!=item){
		mgt_cfg = (struct mgt_cfg *)malloc(sizeof(struct mgt_cfg));
		if(NULL==mgt_cfg){
			nm_log("Failed to alloc struct mgt_cfg.\n");
			goto out;
		}
		memset(mgt_cfg, 0, sizeof(struct mgt_cfg));

		tmp = rte_object_get_item(item, "name");
		if(NULL==tmp){
			nm_log("Failed to get manage interface name.\n");
			goto out;
		}
		strncpy(mgt_cfg->name, tmp->u.val_str, NM_NAME_LEN);

		tmp = rte_object_get_item(item, "ipaddr");	
		if(NULL!=tmp){
			strncpy(mgt_cfg->ipaddr, tmp->u.val_str, NM_ADDR_STR_LEN);
		}

		tmp = rte_object_get_item(item, "gateway");	
		if(NULL!=tmp){
			strncpy(mgt_cfg->gateway, tmp->u.val_str, NM_ADDR_STR_LEN);
		}

		tmp = rte_object_get_item(item, "tcpport");
		if(NULL==tmp){
			nm_log("Failed to parse tcpport.\n");
			goto out;
		}
		mgt_cfg->tcpport = tmp->u.val_int;

		global_vsecplat_config->mgt_cfg = mgt_cfg;
	}

	item = rte_object_get_item(json, "serv_cfg");
	if(NULL!=item){
		serv_cfg = (struct serv_cfg *)malloc(sizeof(struct serv_cfg));
		if(NULL==serv_cfg){
			nm_log("Failed to alloc struct serv_cfg.\n");
			goto out;
		}
		memset(serv_cfg, 0, sizeof(struct serv_cfg));
		tmp = rte_object_get_item(item, "ipaddr");
		if(NULL==tmp){
			nm_log("Failed to get server ipaddr.\n");
			goto out;
		}
		strncpy(serv_cfg->ipaddr, tmp->u.val_str, NM_ADDR_STR_LEN);

		tmp = rte_object_get_item(item, "udpport");
		if(NULL==tmp){
			nm_log("Failed to parse udpport.\n");
			goto out;
		}
		serv_cfg->udpport = tmp->u.val_int;
		global_vsecplat_config->serv_cfg = serv_cfg;
	}

	item = rte_object_get_item(json, "inport_list");
	if(NULL!=item){
		if(item->type != JSON_ARRAY){
			nm_log("Failed to get inport interface list.\n");
			goto out;
		}
		global_vsecplat_config->inport_num = rte_array_get_size(item);
		global_vsecplat_config->inport_desc_array = (struct inport_desc *)malloc(global_vsecplat_config->inport_num * sizeof(struct inport_desc));
		if(NULL==global_vsecplat_config->inport_desc_array){
			nm_log("Failed to alloc inport desc array.\n");
			goto out;
		}
		memset(global_vsecplat_config->inport_desc_array, 0, sizeof(struct inport_desc)*global_vsecplat_config->inport_num);
		for(idx=0;idx<global_vsecplat_config->inport_num;idx++){
			entry = rte_array_get_item(item, idx);
			if(NULL==entry){
				nm_log("Failed to get inport item %d.\n", idx);
				goto out;
			}
			tmp = rte_object_get_item(entry, "name");
			if(NULL==tmp){
				nm_log("There's no name field in item %d.\n", idx);
				goto out;
			}
			if(tmp->type!=JSON_STRING){
				nm_log("name field is valid in item %d.\n", idx);
				goto out;
			}
			strncpy(global_vsecplat_config->inport_desc_array[idx].name, tmp->u.val_str, NM_NAME_LEN); 
		}
	}

	item = rte_object_get_item(json, "outport_list");
	if(NULL!=item){
		if(item->type != JSON_ARRAY){
			nm_log("Failed to get outport interface list.\n");
			goto out;
		}

		global_vsecplat_config->outport_num = rte_array_get_size(item);
		global_vsecplat_config->outport_desc_array = (struct outport_desc *)malloc(global_vsecplat_config->outport_num * sizeof(struct outport_desc));		
		if(NULL==global_vsecplat_config->outport_desc_array){
			nm_log("Failed to alloc outport desc array.\n");
			goto out;
		}
		memset(global_vsecplat_config->outport_desc_array, 0, sizeof(struct outport_desc)*global_vsecplat_config->outport_num);
		for(idx=0;idx<global_vsecplat_config->outport_num;idx++){
			entry = rte_array_get_item(item, idx);
			if(NULL==entry){
				nm_log("Failed to get outport item %d.\n", idx);
				goto out;
			}		
			tmp = rte_object_get_item(entry, "name");
			if(NULL==tmp){
				nm_log("There's no name field in item %d.\n", idx);
				goto out;
			}
			if(tmp->type != JSON_STRING){
				nm_log("name field is valid in item %d.\n", idx);
				goto out;
			}
			strncpy(global_vsecplat_config->outport_desc_array[idx].name, tmp->u.val_str, NM_NAME_LEN); 
		}
	}

	item = rte_object_get_item(json, "time_interval");
	if(NULL!=item){
		global_vsecplat_config->time_interval = item->u.val_int;	
	}

	item = rte_object_get_item(json, "isencrypted");
	if(NULL!=item){
		global_vsecplat_config->isencrypted = item->u.val_int;
	}

	rte_destroy_json(json);
	return  0;

out:
	if(mgt_cfg){
		free(mgt_cfg);
	}
	if(serv_cfg){
		free(serv_cfg);
	}
	if(global_vsecplat_config->inport_desc_array){
		free(global_vsecplat_config->inport_desc_array);
	}

	if(global_vsecplat_config->outport_desc_array){
		free(global_vsecplat_config->outport_desc_array);
	}
	free(file_buf);
	free(global_vsecplat_config);
	rte_destroy_json(json);
	return -1;
}
