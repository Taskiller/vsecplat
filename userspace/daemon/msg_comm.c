#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nm_log.h"
#include "vsecplat_config.h"
#include "vsecplat_policy.h"
#include "vsecplat_record.h"
#include "msg_comm.h"

#define NM_RECV_BUF_LEN 512*1024
#define NM_SEND_BUF_LEN 64*1024
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
		printf("Failed to alloc receive buffer.\n");
		goto out;
	}
	memset(conn_desc->recv_buf, 0, NM_RECV_BUF_LEN*sizeof(char));

	conn_desc->send_buf = (char *)malloc(NM_SEND_BUF_LEN);
	if(NULL==conn_desc->send_buf){
		printf("Failed to alloc send buffer.\n");
		goto out;
	}
	memset(conn_desc->send_buf, 0, NM_SEND_BUF_LEN*sizeof(char));

	conn_desc->status = VSECPLAT_WAIT_CONNECTING;
	conn_desc->timeout = 5;

	conn_desc->udpsock = socket(AF_INET, SOCK_DGRAM, 0);
	if(conn_desc->udpsock<0){
		printf("Failed to create udp socket.\n");
		goto out;
	}

	memset(&conn_desc->udpaddr, 0, sizeof(struct sockaddr_in));
	conn_desc->udpaddr.sin_family = AF_INET;
	conn_desc->udpaddr.sin_addr.s_addr = inet_addr(global_vsecplat_config->serv_cfg->ipaddr);
	conn_desc->udpaddr.sin_port = htons(global_vsecplat_config->serv_cfg->udpport);

	return 0;
out:
	free(conn_desc);
	return -1;
}

int create_listen_socket(void)
{
	int sock;
	struct sockaddr_in serv;
	int ret;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0){
		nm_log("Failed to create server socket.\n");
		return -1;
	}

	/* Make server socket. */
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(global_vsecplat_config->mgt_cfg->tcpport);

	ret = bind(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));
	if(ret<0){
		nm_log("Failed to bind: ");
		close(sock);
		return -1;
	}

	ret = listen(sock, 5);
	if(ret<0){
		nm_log("Failed to listen: ");
		close(sock);
		return -1;
	}

	return sock;
}

void clean_conn_desc(void)
{
	char *recv_buf=NULL;

	if(NULL==conn_desc){
		return;
	}

	if(conn_desc->recv_buf){
		recv_buf = conn_desc->recv_buf;
		memset(recv_buf, 0, NM_RECV_BUF_LEN*sizeof(char));
	}

	if(conn_desc->send_buf){
		free(conn_desc->send_buf);
	}

	memset(conn_desc, 0, sizeof(struct conn_desc));
	conn_desc->recv_buf = recv_buf;
	conn_desc->status = VSECPLAT_WAIT_CONNECTING;

	return;
}

extern struct thread_master *master;
int vsecplat_report_stats(struct thread *thread)
{
#if 0
	char *str;
	int len=0, w_len=0;
	struct msg_head *msg=(struct msg_head *)conn_desc->send_buf;


	memset(conn_desc->send_buf, 0, NM_SEND_BUF_LEN);
	while((str=persist_record())!=NULL){
		len = strlen(str);
		conn_desc->send_len = len+sizeof(struct msg_head);
		msg->len = len + sizeof(struct msg_head);
		msg->msg_type = NM_MSG_REPORTS;
		memcpy(msg->data, str, len);
		free(str);
		w_len = sendto(conn_desc->udpsock, (void *)conn_desc->send_buf, conn_desc->send_len,
						0, (struct sockaddr *)&conn_desc->udpaddr, sizeof(conn_desc->udpaddr));
		if(w_len<=0){
			perror("socket write error:");
			goto out;
		}

		memset(conn_desc->send_buf, 0, NM_SEND_BUF_LEN);
	}
out:
#endif
	printf("In vsecplat_report_stats\n");
	thread_add_timer(master, vsecplat_report_stats, NULL, VSECPLAT_REPORT_INTERVAL);
	return 0;
}

int vsecplat_deal_policy(struct thread *thread)
{
	int accept_sock = THREAD_FD(thread);
	int readlen=0;	
	struct msg_head *msg=NULL;

	printf("In vsecplat_deal_policy, sock=%d.\n", accept_sock);

#if 1
	readlen = read(accept_sock, conn_desc->recv_buf+conn_desc->recv_ofs, 2048);
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
	close(accept_sock);

	return 0;
out:
#endif

	thread_add_read(master, vsecplat_deal_policy, NULL, accept_sock);
	return 0;
}

int vsecplat_listen_func(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	int accept_sock;
	int client_len;
	struct sockaddr_in client;

	printf("In vsecplat_listen_func.\n");
	memset(&client, 0, sizeof(struct sockaddr_in));
	client_len = sizeof(struct sockaddr_in);
	accept_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&client_len);

	if(accept_sock<0){
		nm_log("accept error.\n");
		goto listen_end;
	}

	thread_add_read(thread->master, vsecplat_deal_policy, NULL, accept_sock);

listen_end:
	thread_add_read(thread->master, vsecplat_listen_func, NULL, sock);

	return 0;
}

