#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "msg_comm.h"

int init_sock(char *ipaddr, int port)
{
	int sock;
	struct sockaddr_in addr;
	int ret;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0){
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	// addr.sin_addr.s_addr = ip_addr(ipaddr);
	addr.sin_port = htons(port);
	
	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if(ret<0){
		close(sock);
		return -1;
	}else if(ret==0){

	}

	return sock;
}

static struct conn_desc *conn_desc=NULL;
struct conn_desc *init_conn_desc(void)
{
	struct conn_desc *desc = NULL;	
	desc = (struct conn_desc *)malloc(sizeof(struct conn_desc));
	if(NULL==desc){
		return NULL;
	}
	memset(desc, 0, sizeof(struct conn_desc));

	return desc;
}

extern struct thread_master *master;
int timer_func(struct thread *thread)
{
	printf("In timer_func\n");

	thread_add_timer(master, timer_func, NULL, 5);	
	return;
}
