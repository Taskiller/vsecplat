#ifndef __MSG_COMM_H__
#define __MSG_COMM_H__

#include "thread.h"

struct conn_desc{
	int sock;
	char *recv_buf;
	char *send_buf;
	int recv_len;
	int send_len;
};

int init_sock(char *ipaddr, int port);
int vsecplat_timer_func(struct thread *thread);

#endif
