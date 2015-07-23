#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <linux/un.h>
#include <string.h>
#include <arpa/inet.h>

#include "thread.h"
#include "msg_comm.h"

static struct thread_master *master=NULL;
static struct msg_head *policy_msg=NULL;
static int init_policy_buf()
{
	int fd;
	int len;

	policy_msg = malloc(2048);
	if(NULL==policy_msg){
		return -1;
	}
	memset(policy_msg, 0, 2048);
	fd = open("./add_rule.json", O_RDONLY);
	if(fd<0){
		free(policy_msg);
		policy_msg=NULL;
		return -1;
	}
	len = read(fd, policy_msg->data, 2048);
	close(fd);
	
	policy_msg->len = len + sizeof(struct msg_head);
	policy_msg->msg_type = NM_MSG_RULES;

	return 0;
}

int parse_report(struct msg_head *report_msg)
{
	// printf("report json=%s\n", report_msg->data);
	return 0;
}

#define BUF_LEN 1024*64
static char readbuf[BUF_LEN];
int deal_stats(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	int len=0;
	struct msg_head *report_msg=NULL;

	printf("In deal_stats\n");

	len = recvfrom(sock, (void *)(readbuf), BUF_LEN, 0, NULL, NULL);
	if(len<0){
		printf("Failed to recv from socket.\n");
		return 0;
	}

	report_msg = (struct msg_head *)readbuf;
	printf("get msg len %d, recv_len=%d, msg_type %d\n", report_msg->len, len, report_msg->msg_type);

	// if send complete
	switch(report_msg->msg_type){
		case NM_MSG_REPORTS:
			parse_report(report_msg);
			break;
		default:
			break;
	}
	
	memset(readbuf, 0, BUF_LEN);

	thread_add_read(thread->master, deal_stats, NULL, sock);
	return 0;
}

enum{
	CONNECTING_SERV,
	CONNECT_OK
};

static int conn_status=CONNECTING_SERV;
static int conn_sock=0;
int msg_timer_func(struct thread *thread)
{
	int sock;
	int ret;
	struct sockaddr_in serv;

	printf("In msg_timer_func, conn_status=%d\n", conn_status);
	switch(conn_status){
		case CONNECTING_SERV:
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
				printf("Failed to connect server.\n");
				goto out;
			}
			conn_status = CONNECT_OK;
			conn_sock = sock;
			break;

		case CONNECT_OK:
			ret = write(conn_sock, policy_msg, policy_msg->len);
			if(ret<0){
				perror("socket write error: ");	
			}
			printf("send policy, msg len=%d, writelen=%d, policy=%s\n", policy_msg->len, ret, policy_msg->data);
	
			break;

		default:
			break;
	}
out:
	thread_add_timer(thread->master, msg_timer_func, NULL, 5);
	return 0;
}

int main(void)
{
	int sock=0;
	struct sockaddr_in serv;
	struct thread thread;

	init_policy_buf();

	master = thread_master_create();
	if(NULL==master){
		//TODO
		return -1;
	}

	memset(readbuf, 0, 4096);

	// create recv thread
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock<0){
		printf("Failed to create recv socket.\n");
		return -1;
	}
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(8001);

	if(bind(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in))){
		printf("Failed to bind.\n");
		return -1;
	}

	thread_add_read(master, deal_stats, NULL, sock);	

	thread_add_timer(master, msg_timer_func, NULL, 5);

	memset(&thread, 0, sizeof(struct thread));
	while(thread_fetch(master, &thread)){
		thread_call(&thread);
	}

	return 0;
}
