#include "rte_json.h"
#include "config.h"

#define VSECPLATFORM_CFG_FILE "./config.json"

struct config *parse_vsecplat_config(void)
{
	int fd=0;
	int len=0;
	struct stat stat_buf;
	char *file_buf=NULL;
	struct config *cfg=NULL;	
	struct rte_json *json=NULL, *item=NULL, *entry=NULL;
	struct rte_json *tmp=NULL;
	int idx=0;

	memset(&stat_buf, 0, sizeof(struct stat));
	stat(VSECPLATFORM_CFG_FILE, &stat_buf);
	len = stat_buf.st_size;
	
	file_buf = malloc(len);
	if(NULL==file_buf){
		return NULL;
	}
	memset(file_buf, 0, len);
	fd = open(VSECPLATFORM_CFG_FILE, O_RDONLY);
	if(fd<0){
		free(file_buf);
		return NULL;
	}
	read(fd, file_buf, len);
	close(fd);

	cfg = malloc(sizeof(struct config));		
	if(NULL==cfg){
		free(file_buf);
		return NULL;
	}
	memset(cfg, 0, sizeof(struct config));
	json = rte_parse_json(file_buf);
	if(NULL==json){
		free(file_buf);
		free(cfg);
		return NULL;
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
		cfg->mgt_cfg = mgt_cfg;
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
		cfg->serv_cfg = serv_cfg;
	}

	item = rte_object_get_item(json, "vm_list");
	if(NULL!=item){
		cfg->vm_count = rte_array_get_size(item);
		cfg->vm_list = malloc(cfg->vm_count * sizeof(struct vm_list));
		if(NULL==cfg->vm_list){
			//TODO
		}
		memset(cfg->vm_list, 0, sizeof(struct vm_list)*cfg->vm_count);
		for(idx=0;idx<cfg->vm_count;idx++){
			entry = rte_array_get_item(item, idx);
			if(NULL==entry){
				// TODO
			}
			tmp = rte_object_get_item(entry, "name");
			if(NULL==tmp){
				// TODO
			}
			strncpy(cfg->vm_list[idx].name, tmp->u.val_str, NM_NAME_LEN);
			tmp = rte_object_get_item(entry, "inport");
			if(NULL==tmp){
				// TODO
			}
			strncpy(cfg->vm_list[idx].inport, tmp->u.val_str, NM_ADDR_STR_LEN);
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
		cfg->outport_cfg = outport_cfg;
	}

	return  cfg;
}
