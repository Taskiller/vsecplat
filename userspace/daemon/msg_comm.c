#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

static void combinate_bufs(char *buf, char *tmp)
{
	if((NULL==buf)||(NULL==tmp)){
		return;
	}


	return;
}

static void free_bufs(char *tmp)
{
	char *next=NULL;

	if(NULL==tmp){
		return;
	}

	do{
		next = tmp + 2040;
		free(tmp);
		tmp = next;
	}while(NULL!=tmp);

	return;
}

void clean_conn_desc(void)
{
	if(NULL==conn_desc){
		return;
	}

	close(conn_desc->sock);

	if(conn_desc->recv_buf){
		free_bufs(conn_desc->recv_buf);
	}

	if(conn_desc->send_buf){
		free_bufs(conn_desc->send_buf);
	}

	memset(conn_desc, 0, sizeof(struct conn_desc));
	conn_desc->status = VSECPLAT_CONNECTING_SERV;

	return;
}


extern struct thread_master *master;
#define VSECPLAT_REPORT_INTERVAL 20
int vsecplat_report_stats(struct thread *thread)
{
	char *str;
	int len, w_len;
	struct msg_head *msg=NULL;
	int ret;
	printf("In vsecplat_report_stats.\n");

	str = persist_record();
	if(NULL==str){
		//TODO
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

	w_len = write(conn_desc->sock, msg, msg->len);
	if(ret<0){
		//TODO
		perror("socket write error :");
	}

	if(w_len<msg->len){ // not complete
		thread_add_write(master, vsecplat_report_stats, NULL, conn_desc->sock);	
		return 0;
	}

	thread_add_timer(master, vsecplat_timer_func, NULL, VSECPLAT_REPORT_INTERVAL);
	return 0;
}

int vsecplat_deal_policy(struct thread *thread)
{
	int serv_sock = THREAD_FD(thread);
	int readlen=0;	
	char *readbuf = NULL;
	struct msg_head *msg=NULL;

	printf("In vsecplat_deal_policy.\n");

	if(NULL==conn_desc->recv_buf){
		readbuf = malloc(2048);
		if(NULL==readbuf){
			goto out;
		}
		memset(readbuf, 0, 2048);
		readlen = read(serv_sock, readbuf, 2040); // last 8byte reserved for next pointer
		if(readlen<=0){ // sock is close or error
			clean_conn_desc();
			// thread_add_timer(master, vsecplat_timer_func, NULL, 5);
			return -1;
		}

		conn_desc->recv_buf = readbuf;
		msg = (struct msg_head *)readbuf;
		conn_desc->recv_ofs = readlen;
		conn_desc->recv_len = msg->len;

		if(msg->len != readlen){ // recv not complete
			printf("msg len=%d, readlen=%d\n", msg->len, readlen);
			goto out;
		}
	}else{

	}
	
	printf("vsecplat_deal_policy readlen=%d, msg len=%d type=%d policy:%s\n", readlen, msg->len, msg->msg_type, msg->data);

	msg = (struct msg_head *)conn_desc->recv_buf;
	readbuf = (char *)malloc(conn_desc->recv_len);

	if(NULL==readbuf){
		//TODO
	}

	switch(msg->msg_type){
		case NM_ADD_RULES:

			break;
		case NM_DEL_RULES:
			break;
		default:
			break;
	}

	free_bufs(conn_desc->recv_buf);
	conn_desc->recv_len = 0;
	conn_desc->recv_ofs = 0;
	conn_desc->recv_buf = NULL;

out:
	thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
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
		#if 0
			msg = (struct msg_head *)malloc(sizeof(struct msg_head));
			if(NULL==msg){
				//TODO
			}
			memset(msg, 0, sizeof(struct msg_head));
			msg->len = sizeof(struct msg_head);
			msg->msg_type = NM_GET_RULES;
			printf("send msg len %d, type %d.\n", msg->len, msg->msg_type);
			ret = write(conn_desc->sock, msg, msg->len);
			if(ret<0){
				//TODO
				perror("socket write error: ");
			}
			free(msg);
		#endif

			conn_desc->status = VSECPLAT_RUNNING;
			thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
			timeout = VSECPLAT_REPORT_INTERVAL;
			break;

		case VSECPLAT_RUNNING:
			thread_add_write(master, vsecplat_report_stats, NULL, conn_desc->sock);
			return 0;
		default:
			break;
	}
out:
	thread_add_timer(master, vsecplat_timer_func, NULL, timeout);
	return 0;
}

