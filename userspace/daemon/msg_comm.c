#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vsecplat_config.h"
#include "vsecplat_record.h"
#include "msg_comm.h"

static struct conn_desc *conn_desc=NULL;
int init_conn_desc(void)
{
	conn_desc = (struct conn_desc *)malloc(sizeof(struct conn_desc));
	if(NULL==conn_desc){
		return -1;
	}
	memset(conn_desc, 0, sizeof(struct conn_desc));
	conn_desc->status = VSECPLAT_CONNECTING_SERV;

	return 0;
}

extern struct thread_master *master;
int vsecplat_deal_policy(struct thread *thread)
{
	int serv_sock = THREAD_FD(thread);
	int readlen=0;	
	char *readbuf = NULL;
	struct msg_head *msg=NULL;

	readbuf = malloc(2048);
	if(NULL==readbuf){
		return -1;
	}

	memset(readbuf, 0, 2048);
	readlen = read(serv_sock, readbuf, 2048);
	if(readlen<=0){
		return -1;
	}
	msg = (struct msg_head *)readbuf;
	if(msg->len != readlen){
		// TODO
	}

	printf("vsecplat_deal_policy readlen=%d, msg len=%d type=%d policy:%s\n", readlen, msg->len, msg->msg_type, msg->data);

	switch(msg->msg_type){
		case NM_ADD_RULES:
			break;
		case NM_DEL_RULES:
			break;
		default:
			break;
	}

	thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
	free(readbuf);
	return 0;
}

int vsecplat_timer_func(struct thread *thread)
{
	int sock;
	int ret;
	int timeout=5;
	struct sockaddr_in serv;
	struct msg_head *msg=NULL;
	char *str=NULL;
	int len=0;
	printf("In vsecplat_timer_func\n");

	switch(conn_desc->status){
		case VSECPLAT_CONNECTING_SERV:
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
			conn_desc->status = VSECPLAT_CONNECT_OK;
			conn_desc->sock = sock;

			break;
		case VSECPLAT_CONNECT_OK:
			msg = (struct msg_head *)malloc(sizeof(struct msg_head));
			if(NULL==msg){
				//TODO
			}
			memset(msg, 0, sizeof(struct msg_head));
			msg->len = sizeof(struct msg_head);
			msg->msg_type = NM_GET_RULES;
			printf("send msg len %d, type %d.\n", msg->len, msg->msg_type);
			send(conn_desc->sock, msg, msg->len, 0);
			free(msg);
			conn_desc->status = VSECPLAT_RUNNING;
			thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
			timeout = 20;  // report every one minute

			break;
		case VSECPLAT_RUNNING:
			// report count
			str = persist_record();
			if(NULL==str){
				//TODO
				break;
			}
			len = strlen(str) + sizeof(struct msg_head);
			msg = (struct msg_head *)malloc(len);
			if(NULL==msg){
				//TODO
			}
			memset(msg, 0, len);
			msg->len = len;
			msg->msg_type = NM_REPORT_COUNT;
			strcpy(msg->data, str);
			printf("will report count, msg len=%d.\n", msg->len);
			send(conn_desc->sock, msg, msg->len, 0);

			break;
		default:
			break;
	}

out:
	thread_add_timer(master, vsecplat_timer_func, NULL, timeout);
	return 0;
}

