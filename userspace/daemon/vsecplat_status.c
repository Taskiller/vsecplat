#include "vsecplat_status.h"

struct vsecplat_status *global_vsecplat_status=NULL;

int init_vsecplat_status(void)
{
	global_vsecplat_status = malloc(sizeof(struct vsecplat_status));
	if(NULL==global_vsecplat_status){
		return -1;
	}
	memset(global_vsecplat_status, 0, sizeof(struct vsecplat_status));
	
	global_vsecplat_status->status = VSECPLAT_CONNECTING_SERV;
	return 0;
}
