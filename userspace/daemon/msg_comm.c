#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "msg_comm.h"

int init_sock(char *ipaddr, int port)
{
	int sock;
	struct sockaddr_in addr;
	struct in_addr in;
	int ret;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0){
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	inet_aton(ipaddr, &in);
	addr.sin_addr.s_addr = in.s_addr;
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
	int sock;
	int ret;
	struct sockaddr_in serv;
	char buf[128];

	memset(buf, 0, 128);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0){
		goto out;
	}
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(8000);
	inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

	ret = connect(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));	
	if(ret<0){
		close(sock);
		goto out;
	}
	sprintf(buf, "%s", "just for fun");
	send(sock, buf, strlen(buf), 0);

	return ;
out:
	thread_add_timer(master, timer_func, NULL, 5);	
	return;
}
