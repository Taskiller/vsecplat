#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vsecplat_status.h"
#include "vsecplat_config.h"
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
int vsecplat_deal_policy(struct thread *thread)
{
	int serv_sock = THREAD_FD(thread);
	int readlen=0;	
	char *readbuf = NULL;

	readbuf = malloc(4096);
	if(NULL==readbuf){
		return -1;
	}

	memset(readbuf, 0, 4096);
	readlen = read(serv_sock, readbuf, 4096);
	if(readlen<=0){
		return -1;
	}

	printf("readlen=%d, policy:%s\n", readlen, readbuf);

	thread_add_read(master, vsecplat_deal_policy, NULL, global_vsecplat_status->serv_sock);	
	free(readbuf);
	return 0;
}

int vsecplat_timer_func(struct thread *thread)
{
	printf("In vsecplat_timer_func\n");
	int sock;
	int ret;
	struct sockaddr_in serv;
	char buf[128];

	switch(global_vsecplat_status->status){
		case VSECPLAT_CONNECTING_SERV:
			memset(buf, 0, 128);
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock<0){
				goto out;
			}
			memset(&serv, 0, sizeof(struct sockaddr_in));
			serv.sin_family = AF_INET;
			serv.sin_port = htons(global_vsecplat_config->serv_cfg->port);
			inet_pton(AF_INET, global_vsecplat_config->serv_cfg->ipaddr, &serv.sin_addr);

			ret = connect(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));	
			if(ret<0){
				close(sock);
				printf("failed to connect server\n");
				goto out;
			}
			global_vsecplat_status->status = VSECPLAT_CONNECT_OK;
			global_vsecplat_status->serv_sock = sock;

			break;

		case VSECPLAT_CONNECT_OK:
			sprintf(buf, "%s", "get policy");
			send(global_vsecplat_status->serv_sock, buf, strlen(buf), 0);

			thread_add_read(master, vsecplat_deal_policy, NULL, global_vsecplat_status->serv_sock);	
			break;

		default:
			break;
	}

out:
	thread_add_timer(master, vsecplat_timer_func, NULL, 5);	
	return 0;
}

