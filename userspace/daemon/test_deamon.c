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

#include "thread.h"
#include "msg_comm.h"

static struct thread_master *master=NULL;

int send_policy(int sock)
{
	int ret=0;
	int fd=0;
	int len;
	struct msg_head *msg=NULL;

	msg = malloc(2048);
	if(NULL==msg){
		return -1;
	}
	memset(msg, 0, 2048);
	fd = open("./add_rule.json", O_RDONLY);
	if(fd<0){
		free(msg);
		return -1;
	}
	len = read(fd, msg->data, 2048);
	close(fd);
	
	msg->len = len + sizeof(struct msg_head);
	msg->msg_type = NM_MSG_RULES;

	ret = write(sock, msg, msg->len);
	if(ret<0){
		perror("socket write error: ");	
	}
	
	free(msg);
	printf("In send_policy, msg len=%d, writelen=%d, policy=%s\n", msg->len, ret, msg->data);
	return 0;
}

int parse_report(struct msg_head *msg)
{
	printf("report json=%s\n", msg->data);
	return 0;
}

#define BUF_LEN 1024*64
static char readbuf[BUF_LEN];
static int readlen=0;
static int readofs=0;
static int send_sock=0;

int msg_deal(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	int len=0;
	struct msg_head *msg=NULL;

	printf("In msg_deal, readofs=%d\n", readofs);

	len = read(sock, (void *)(readbuf+readofs), 4096);
	if(len<=0){
		perror("read error:");
		return 0;
	}

	readofs += len;
	msg = (struct msg_head *)readbuf;
	readlen = msg->len;

	printf("readofs=%d, readlen=%d\n", readofs, readlen);

	if(readofs<readlen){
		goto out;
	}

	printf("readofs %d, get msg len %d, msg_type %d\n", readofs, msg->len, msg->msg_type);

	// if send complete
	switch(msg->msg_type){
		case NM_MSG_REPORTS:
			parse_report(msg);
			break;
		default:
			break;
	}
	
	memset(readbuf, 0, BUF_LEN);
	readlen = 0;
	readofs = 0;
out:
	thread_add_read(thread->master, msg_deal, NULL, sock);
	return 0;
}

int send_msg(struct thread *thread)

{
	send_policy(send_sock);

	thread_add_timer(thread->master, send_msg, NULL, 5);

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
			send_policy(conn_sock);
			close(conn_sock);
			conn_sock=0;
			conn_status = CONNECTING_SERV;
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
	struct thread thread;
	master = thread_master_create();
	if(NULL==master){
		//TODO
		return -1;
	}

	memset(readbuf, 0, 4096);

	thread_add_timer(master, msg_timer_func, NULL, 5);

	memset(&thread, 0, sizeof(struct thread));
	while(thread_fetch(master, &thread)){
		thread_call(&thread);
	}

	return 0;
}
