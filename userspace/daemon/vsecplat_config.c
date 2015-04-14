#include "rte_json.h"
#include "vsecplat_config.h"

#define VSECPLATFORM_CFG_FILE "./config.json"
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

	memset(&stat_buf, 0, sizeof(struct stat));
	stat(VSECPLATFORM_CFG_FILE, &stat_buf);
	len = stat_buf.st_size;
	
	file_buf = malloc(len);
	if(NULL==file_buf){
		return -1;
	}
	memset(file_buf, 0, len);
	fd = open(VSECPLATFORM_CFG_FILE, O_RDONLY);
	if(fd<0){
		free(file_buf);
		return -1;
	}
	read(fd, file_buf, len);
	close(fd);

	global_vsecplat_config = malloc(sizeof(struct vsecplat_config));		
	if(NULL==global_vsecplat_config){
		free(file_buf);
		return -1;
	}
	memset(global_vsecplat_config, 0, sizeof(struct vsecplat_config));
	json = rte_parse_json(file_buf);
	if(NULL==json){
		free(file_buf);
		free(global_vsecplat_config);
		return -1;
	}

	item = rte_object_get_item(json, "mgt_cfg");	
	if(NULL!=item){
		struct mgt_cfg *mgt_cfg = malloc(sizeof(mgt_cfg));			
		if(NULL==mgt_cfg){
			// TODO
		}
		memset(mgt_cfg, 0, sizeof(struct mgt_cfg));
		tmp = rte_object_get_item(item, "name");
		if(NULL==tmp){
			// TODO
		}
		strncpy(mgt_cfg->name, tmp->u.val_str, NM_NAME_LEN);
		tmp = rte_object_get_item(item, "ipaddr");	
		if(NULL==tmp){
			// TODO
		}
		strncpy(mgt_cfg->ipaddr, tmp->u.val_str, NM_ADDR_STR_LEN);
		tmp = rte_object_get_item(item, "mac");
		if(NULL==tmp){
			// TODO
		}
		strncpy(mgt_cfg->mac, tmp->u.val_str, NM_ADDR_STR_LEN);
		global_vsecplat_config->mgt_cfg = mgt_cfg;
	}

	item = rte_object_get_item(json, "serv_cfg");
	if(NULL!=item){
		struct serv_cfg *serv_cfg=malloc(sizeof(struct serv_cfg));	
		if(NULL==serv_cfg){
			// TODO
		}
		memset(serv_cfg, 0, sizeof(struct serv_cfg));
		tmp = rte_object_get_item(item, "ipaddr");
		if(NULL==tmp){
			// TODO
		}
		strncpy(serv_cfg->ipaddr, tmp->u.val_str, NM_ADDR_STR_LEN);
		tmp = rte_object_get_item(item, "port");
		if(NULL==tmp){
			// TODO
		}
		serv_cfg->port = tmp->u.val_int;
		global_vsecplat_config->serv_cfg = serv_cfg;
	}

	item = rte_object_get_item(json, "vm_list");
	if(NULL!=item){
		global_vsecplat_config->vm_count = rte_array_get_size(item);
		global_vsecplat_config->vm_list = malloc(global_vsecplat_config->vm_count * sizeof(struct vm_list));
		if(NULL==global_vsecplat_config->vm_list){
			//TODO
		}
		memset(global_vsecplat_config->vm_list, 0, sizeof(struct vm_list)*global_vsecplat_config->vm_count);
		for(idx=0;idx<global_vsecplat_config->vm_count;idx++){
			entry = rte_array_get_item(item, idx);
			if(NULL==entry){
				// TODO
			}
			tmp = rte_object_get_item(entry, "name");
			if(NULL==tmp){
				// TODO
			}
			strncpy(global_vsecplat_config->vm_list[idx].name, tmp->u.val_str, NM_NAME_LEN);
			tmp = rte_object_get_item(entry, "inport");
			if(NULL==tmp){
				// TODO
			}
			strncpy(global_vsecplat_config->vm_list[idx].inport, tmp->u.val_str, NM_ADDR_STR_LEN);
		}
	}

	item = rte_object_get_item(json, "outport_cfg");
	if(NULL!=item){
		struct outport_cfg *outport_cfg=malloc(sizeof(struct outport_cfg));		
		if(NULL==outport_cfg){
			// TODO
		}
		tmp = rte_object_get_item(item, "mac");		
		if(NULL==tmp){
			// TODO
		}		
		strncpy(outport_cfg->mac, tmp->u.val_str, NM_ADDR_STR_LEN);
		global_vsecplat_config->outport_cfg = outport_cfg;
	}

	return  0;
}
