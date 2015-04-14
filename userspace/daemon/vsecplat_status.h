#ifndef __STATUS_H__
#define __STATUS_H__

#include "nm_type.h"

enum{
	VSECPLAT_CONNECTING_SERV,
	VSECPLAT_CONNECT_OK,
	VSECPLAT_STATUS_MAX
};

struct vsecplat_status{
	int status;
	int serv_sock;
};


extern struct vsecplat_status *global_vsecplat_status;
int init_vsecplat_status(void);

#endif
