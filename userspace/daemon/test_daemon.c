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
#include "tea_crypt.h"

enum{
	SOCKET_WANT_CONNECT,
	SOCKET_WANT_WRITE,
	SOCKET_WANT_READ,
};

static struct thread_master *master=NULL;
static struct msg_head *policy_msg=NULL;
static char response_buf[2048];
static int conn_status=SOCKET_WANT_CONNECT;
static int conn_sock=0;

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

	//len = nm_encrypt((unsigned int *)policy_msg->data, len);

	policy_msg->len = len + sizeof(struct msg_head);
	policy_msg->msg_type = NM_MSG_RULES;

	return 0;
}

int parse_report(struct msg_head *report_msg)
{
	int len=0;
	//len = nm_decrypt((unsigned int *)report_msg->data, report_msg->len-sizeof(struct msg_head));
	printf("Recv stat len=%d, contents:\n%s\n", len, report_msg->data);
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

int msg_timer_func(struct thread *thread);
static char readbuf[BUF_LEN];
int deal_response(struct thread *thread)
{
	int sock = THREAD_FD(thread);	
	int len = 0;
	struct msg_head *msg=(struct msg_head *)response_buf;
	printf("In deal_response.\n");
	memset(response_buf, 0, 2048);

	len = read(sock, (void *)response_buf, 2048);
	if(len<=0){
		close(sock);
		conn_status = SOCKET_WANT_CONNECT;
		goto out;
	}

	// nm_decrypt((unsigned int *)msg->data, len);
	printf("response: type=%d, len=%d, data=%s\n", msg->msg_type, msg->len, msg->data);

	conn_status = SOCKET_WANT_WRITE;
out:
	thread_add_timer(thread->master, msg_timer_func, NULL, 5);
	return 0;
}

int msg_timer_func(struct thread *thread)
{
	int sock;
	int ret;
	struct sockaddr_in serv;

	printf("In msg_timer_func, conn_status=%d\n", conn_status);
	switch(conn_status){
		case SOCKET_WANT_CONNECT:
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock<0){
				goto out;
			}
			memset(&serv, 0, sizeof(struct sockaddr_in));
			serv.sin_family = AF_INET;
			serv.sin_port = htons(8000);
			inet_pton(AF_INET, "192.168.1.146", &serv.sin_addr);
			// inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
			ret = connect(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));
			if(ret<0){
				close(sock);
				printf("Failed to connect server.\n");
				goto out;
			}
			conn_status = SOCKET_WANT_WRITE;
			conn_sock = sock;
			break;

		case SOCKET_WANT_WRITE:
		#if 1
			ret = write(conn_sock, policy_msg, policy_msg->len);
			if(ret<0){
				perror("socket write error: ");	
				close(conn_sock);
				conn_status = SOCKET_WANT_CONNECT;
				goto out;
			}
			printf("send policy, msg len=%d, writelen=%d\n", policy_msg->len, ret);
			conn_status = SOCKET_WANT_READ;	
			thread_add_read(thread->master, deal_response, NULL, conn_sock);
		#endif
			return 0;
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
