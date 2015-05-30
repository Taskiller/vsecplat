#ifndef __MSG_COMM_H__
#define __MSG_COMM_H__

#include "thread.h"

enum{
	VSECPLAT_CONNECTING_SERV=0,
	VSECPLAT_CONNECT_OK,
	VSECPLAT_RUNNING,
	VSECPLAT_STATUS_MAX
};

struct conn_desc{
	int sock;
	int timeout;
	int status;	
	
	int recv_len;
	int send_len;
	int recv_ofs;
	int send_ofs;
	
	char *recv_buf;
	char *send_buf;
};

enum{
	NM_ADD_RULES=1,   /* manage center add the policy rules */
	NM_DEL_RULES,	/* manage center del the policy rules */
	NM_REPORT_COUNT,		/* vm report the packet records */
};

struct msg_head{
	int len;
	unsigned char msg_type;
	char data[0];
};

int init_conn_desc(void);
int vsecplat_timer_func(struct thread *thread);

#endif
