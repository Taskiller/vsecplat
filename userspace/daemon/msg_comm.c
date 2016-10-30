#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nm_log.h"
#include "rte_json.h"
#include "vsecplat_config.h"
#include "vsecplat_policy.h"
#include "vsecplat_record.h"
#include "msg_comm.h"
#include "tea_crypt.h"

#define NM_RECV_BUF_LEN 512*1024
#define NM_SEND_BUF_LEN 64*1024
static struct policy_conn_desc *policy_conn_desc=NULL;
static struct report_conn_desc *report_conn_desc=NULL;

int init_conn_desc(void)
{
	policy_conn_desc = (struct policy_conn_desc *)malloc(sizeof(struct policy_conn_desc));
	if(NULL==policy_conn_desc){
		return -1;
	}
	memset(policy_conn_desc, 0, sizeof(struct policy_conn_desc));

	policy_conn_desc->policy_buf = (char *)malloc(NM_RECV_BUF_LEN);
	if(NULL==policy_conn_desc->policy_buf){
		printf("Failed to alloc receive buffer.\n");
		goto out;
	}
	memset(policy_conn_desc->policy_buf, 0, NM_RECV_BUF_LEN*sizeof(char));

	report_conn_desc = (struct report_conn_desc *)malloc(sizeof(struct report_conn_desc));
	if(NULL==report_conn_desc){
		free(policy_conn_desc->policy_buf);
		free(policy_conn_desc);
		return -1;
	}
	memset(report_conn_desc, 0, sizeof(struct report_conn_desc));

	report_conn_desc->report_buf = (char *)malloc(NM_SEND_BUF_LEN);
	if(NULL==report_conn_desc->report_buf){
		printf("Failed to alloc send buffer.\n");
		goto out;
	}
	memset(report_conn_desc->report_buf, 0, NM_SEND_BUF_LEN*sizeof(char));

	report_conn_desc->report_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(report_conn_desc->report_sock<0){
		nm_log("Failed to create udp socket.\n");
		free(report_conn_desc);
		goto out;
	}

	memset(&report_conn_desc->serv_addr, 0, sizeof(struct sockaddr_in));
	report_conn_desc->serv_addr.sin_family = AF_INET;
	report_conn_desc->serv_addr.sin_addr.s_addr = inet_addr(global_vsecplat_config->serv_cfg->ipaddr);
	report_conn_desc->serv_addr.sin_port = htons(global_vsecplat_config->serv_cfg->udpport);

	return 0;

out:
	free(policy_conn_desc->policy_buf);
	free(policy_conn_desc);

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

void clean_policy_conn_desc(void)
{
	char *policy_buf=NULL;

	if(NULL==policy_conn_desc){
		return;
	}

	if(policy_conn_desc->policy_buf){
		policy_buf = policy_conn_desc->policy_buf;
		memset(policy_buf, 0, NM_RECV_BUF_LEN*sizeof(char));
	}

	memset(policy_conn_desc, 0, sizeof(struct policy_conn_desc));
	policy_conn_desc->policy_buf = policy_buf;

	return;
}

extern struct thread_master *master;
extern int vsecplat_show_record;

int vsecplat_report_stats(struct thread *thread)
{
	int ret=0;
	int len=0, w_len=0;
	struct msg_head *msg=(struct msg_head *)report_conn_desc->report_buf;
	struct list_head *pos=NULL, *tmp=NULL;
	struct record_json_item *record_json_item=NULL;

	if(!global_vsecplat_config->isstatistics){
		return 0;
	}

	if(report_conn_desc->report_sock==0){
		report_conn_desc->report_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(report_conn_desc->report_sock<0){
			nm_log("Failed to create udp socket.\n");
			goto out;
		}
	}

	memset(report_conn_desc->report_buf, 0, NM_SEND_BUF_LEN);
	ret= vsecplat_persist_record();
	if(ret<0){
		nm_log("vsecplat_persist_record return %d.\n", ret);
		return -1;
	}

	list_for_each_safe(pos, tmp, &global_record_json_list){
		record_json_item = list_entry(pos, struct record_json_item, list);
		len = rte_persist_json(msg->data, record_json_item->root, JSON_WITHOUT_FORMAT);
		if(len<=0){
			nm_log("Failed to serialize record item.\n");
			goto out;
		}

		if(vsecplat_show_record){
			printf("send record: len=%d, msg->data=%s\n", len, msg->data);
		}

		if(global_vsecplat_config->isencrypted){
			len = nm_encrypt((unsigned int *)msg->data, len);
		}

		report_conn_desc->send_len = len+sizeof(struct msg_head);
		msg->len = report_conn_desc->send_len;
		msg->msg_type = NM_MSG_REPORTS;
		w_len = sendto(report_conn_desc->report_sock, (void *)report_conn_desc->report_buf, report_conn_desc->send_len, 0,
						(struct sockaddr *)&report_conn_desc->serv_addr, sizeof(report_conn_desc->serv_addr));
		if(w_len<0){
			nm_log("socket write error, errno=%d\n", errno);
			goto out;
		}

		memset(report_conn_desc->report_buf, 0, NM_SEND_BUF_LEN);
	}

out:
	close(report_conn_desc->report_sock);
	report_conn_desc->report_sock = 0;

	clear_global_record_json_list();
	thread_add_timer(master, vsecplat_report_stats, NULL, global_vsecplat_config->time_interval);

	return 0;
}

int vsecplat_deal_policy(struct thread *thread)
{
	int accept_sock = THREAD_FD(thread);
	int readlen=0;	
	struct msg_head *msg=NULL;
	int result=0, ret=0;
	int resp_len=0;

	printf("In vsecplat_deal_policy, sock=%d.\n", accept_sock);

	readlen = read(accept_sock, (policy_conn_desc->policy_buf + policy_conn_desc->recv_ofs), 4096);
	if(readlen<=0){ // sock is close or error
		close(accept_sock);
		clean_policy_conn_desc();
		return -1;
	}
	
	msg = (struct msg_head *)policy_conn_desc->policy_buf;
	policy_conn_desc->recv_len = msg->len;
	policy_conn_desc->recv_ofs += readlen;

	if(policy_conn_desc->recv_ofs<policy_conn_desc->recv_len){ // recv not complete
		// printf("recv not complete: msg len=%d, readlen=%d\n", msg->len, readlen);
		goto out;
	}

	if(msg->msg_type!=NM_MSG_RULES){
		nm_log("Received msg_type is wrong : %d\n", msg->msg_type);
		goto out;
	}

	if(global_vsecplat_config->isencrypted){
		nm_decrypt((unsigned int *)msg->data, msg->len-sizeof(struct msg_head));
	}

	// printf("vsecplat_deal_policy readlen=%d, msg_len=%d type=%d contents:\n%s\n", readlen, msg->len, msg->msg_type, msg->data);
	result = vsecplat_parse_policy(msg->data);

	memset(msg->data, 0, policy_conn_desc->recv_len);
	resp_len = create_policy_response(msg->data, result, 0);
	if(resp_len<0){
		nm_log("Failed to create response.\n");
		goto out;
	}
	// printf("vsecplat_parse_policy response len=%ld, contents:\n%s\n", resp_len+sizeof(struct msg_head), msg->data);
	if(global_vsecplat_config->isencrypted){
		resp_len = nm_encrypt((unsigned int *)msg->data, resp_len);
	}
	msg->len = resp_len + sizeof(struct msg_head);
	ret = write(accept_sock, msg, msg->len);
	if(ret<0){
		nm_log("Failed to send response.\n");
	}

out:
	memset(policy_conn_desc->policy_buf, 0, policy_conn_desc->recv_len);
	policy_conn_desc->recv_len = 0;
	policy_conn_desc->recv_ofs = 0;

#if 0  // Need to close?
	close(accept_sock);
	return 0;
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

	// printf("In vsecplat_listen_func.\n");
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

