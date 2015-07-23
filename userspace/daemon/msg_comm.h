#ifndef __MSG_COMM_H__
#define __MSG_COMM_H__

#include "thread.h"

enum{
	VSECPLAT_WAIT_CONNECTING=0,
	VSECPLAT_CONNECT_OK,
	VSECPLAT_RUNNING,
	VSECPLAT_STATUS_MAX
};

struct conn_desc{
	int report_sock;
	struct sockaddr_in serv_addr;
	int status;	
	
	int recv_len;
	int recv_ofs;
	int send_len;
	int send_ofs;
	
	char *policy_buf;
	char *report_buf;
};

enum{
	NM_MSG_RULES=1,
	NM_MSG_REPORTS,
};

struct msg_head{
	int len;
	int msg_type;
	char data[0];
}__attribute__((packed));


int init_conn_desc(void);
int create_listen_socket(void);
int vsecplat_timer_func(struct thread *thread);
int vsecplat_report_stats(struct thread *thread);
int vsecplat_listen_func(struct thread *thread);

#endif
