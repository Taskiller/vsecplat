#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vsecplat_config.h"
#include "vsecplat_policy.h"
#include "vsecplat_record.h"
#include "msg_comm.h"

#define NM_RECV_BUF_LEN 512*1024
static struct conn_desc *conn_desc=NULL;
int init_conn_desc(void)
{
	conn_desc = (struct conn_desc *)malloc(sizeof(struct conn_desc));
	if(NULL==conn_desc){
		return -1;
	}
	memset(conn_desc, 0, sizeof(struct conn_desc));

	conn_desc->recv_buf = (char *)malloc(NM_RECV_BUF_LEN);
	if(NULL==conn_desc->recv_buf){
		//TODO
	}
	memset(conn_desc->recv_buf, 0, NM_RECV_BUF_LEN*sizeof(char));
	conn_desc->status = VSECPLAT_CONNECTING_SERV;
	conn_desc->timeout = 5;

	return 0;
}

void clean_conn_desc(void)
{
	char *recv_buf=NULL;

	if(NULL==conn_desc){
		return;
	}

	close(conn_desc->sock);

	if(conn_desc->recv_buf){
		recv_buf = conn_desc->recv_buf;
		memset(recv_buf, 0, NM_RECV_BUF_LEN*sizeof(char));
	}

	if(conn_desc->send_buf){
		free(conn_desc->send_buf);
	}

	memset(conn_desc, 0, sizeof(struct conn_desc));
	conn_desc->recv_buf = recv_buf;
	conn_desc->status = VSECPLAT_CONNECTING_SERV;
	conn_desc->timeout = 5;

	return;
}

extern struct thread_master *master;
#define VSECPLAT_REPORT_INTERVAL 60 // report every one minute
int vsecplat_report_stats(struct thread *thread)
{
	char *str;
	int len, w_len;
	struct msg_head msg;

	// printf("In vsecplat_report_stats.\n");

	if(conn_desc->send_ofs==0){
		str = persist_record();
		if(NULL==str){
			//TODO
			goto no_count_report;
		}
		len = strlen(str);
		conn_desc->send_len = len;
		conn_desc->send_buf = str;
		memset(&msg, 0, sizeof(struct msg_head));
		msg.len = len + sizeof(struct msg_head);
		msg.msg_type = NM_MSG_REPORTS;
	
		w_len = write(conn_desc->sock, (void *)&msg, sizeof(struct msg_head));
		if(w_len<=0){ 
			perror("socket write error:");
			goto out;
		}
		if(w_len<sizeof(struct msg_head)){
			//TODO,It should not be happen.
		}
	
		w_len = write(conn_desc->sock, (void *)conn_desc->send_buf, conn_desc->send_len);
		if(w_len<=0){
			//TODO
			perror("socket write error:");
			goto out;
		}
		conn_desc->send_ofs += w_len;
	}else{
		w_len = write(conn_desc->sock, (void *)(conn_desc->send_buf+conn_desc->send_ofs), conn_desc->send_len - conn_desc->send_ofs);
		if(w_len<=0){
			//TODO
			perror("socket write error:");
			goto out;
		}
		conn_desc->send_ofs += w_len;
	}

	if(conn_desc->send_ofs<conn_desc->send_len){ // write not complete
		thread_add_write(master, vsecplat_report_stats, NULL, conn_desc->sock);	
		return 0;
	}
	
	free(conn_desc->send_buf);
	conn_desc->send_ofs = 0;
	conn_desc->send_len = 0;
	conn_desc->send_buf = NULL;

no_count_report:
	thread_add_timer(master, vsecplat_timer_func, NULL, conn_desc->timeout);
	return 0;

out:
	clean_conn_desc();
	thread_add_timer(master, vsecplat_timer_func, NULL, conn_desc->timeout);
	return 0;
}

int vsecplat_deal_policy(struct thread *thread)
{
	int serv_sock = THREAD_FD(thread);
	int readlen=0;	
	struct msg_head *msg=NULL;

	printf("In vsecplat_deal_policy.\n");

	readlen = read(serv_sock, conn_desc->recv_buf+conn_desc->recv_ofs, 2048); 
	if(readlen<=0){ // sock is close or error
		clean_conn_desc();
		return -1;
	}
	
	msg = (struct msg_head *)conn_desc->recv_buf;
	conn_desc->recv_len = msg->len;
	conn_desc->recv_ofs += readlen;

	if(conn_desc->recv_ofs<conn_desc->recv_len){ // recv not complete
		printf("msg len=%d, readlen=%d\n", msg->len, readlen);
		goto out;
	}
	
	printf("vsecplat_deal_policy readlen=%d, msg len=%d type=%d policy:%s\n", readlen, msg->len, msg->msg_type, msg->data);

	switch(msg->msg_type){
		case NM_MSG_RULES:
			vsecplat_parse_policy(msg->data);		
			break;
		case NM_MSG_REPORTS:
			break;
		default:
			break;
	}

	memset(conn_desc->recv_buf, 0, conn_desc->recv_len);
	conn_desc->recv_len = 0;
	conn_desc->recv_ofs = 0;

out:
	thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
	return 0;
}

int vsecplat_timer_func(struct thread *thread)
{
	int sock;
	int ret;
	struct sockaddr_in serv;

	// printf("In vsecplat_timer_func\n");

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
			conn_desc->status = VSECPLAT_RUNNING;
			thread_add_read(master, vsecplat_deal_policy, NULL, conn_desc->sock);	
			conn_desc->timeout = VSECPLAT_REPORT_INTERVAL;
			break;

		case VSECPLAT_RUNNING:
			thread_add_write(master, vsecplat_report_stats, NULL, conn_desc->sock);
			return 0;

		default:
			break;
	}
out:
	thread_add_timer(master, vsecplat_timer_func, NULL, conn_desc->timeout);
	return 0;
}

