#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <linux/un.h>
#include <string.h>

#include "thread.h"
#include "msg_comm.h"

static struct thread_master *master=NULL;

int create_sock(void)
{
	int ret;
	int sock;
	struct sockaddr_in serv;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0){
		//TODO
		printf("Failed to create server socket.\n");
		return -1;
	}

	/* Make server socket. */
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(8000);

	ret = bind(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));
	if(ret<0){
		//TODO
		printf("Failed to bind, err=%d\n", errno);
		close(sock);
		return -1;
	}

	ret = listen(sock, 5);
	if(ret<0){
		//TODO
		printf("Failed to listen\n");
		close(sock);
		return -1;
	}

	return sock;
}

int send_policy(int sock)
{
	int fd=0;
	struct stat stat_buf;
	int len;
	struct msg_head *msg=NULL;

	memset(&stat_buf, 0, sizeof(struct stat));
	stat("./add_rule.json", &stat_buf);
	len = stat_buf.st_size;
	
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
	read(fd, msg->data, len);
	close(fd);
	
	msg->len = len + sizeof(struct msg_head);
	write(sock, msg, len);
	free(msg);
	printf("In send_policy, policy=%s\n", msg->data);
	return 0;
}

int parse_report(struct msg_head *msg)
{
	printf("report=%s\n", msg->data);
	return 0;
}

int msg_deal(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	char *readbuf=NULL;
	int readlen;
	struct msg_head *msg=NULL;

	printf("In msg_deal\n");	
	readbuf = malloc(4096);
	memset(readbuf, 0, 4096);

	readlen = read(sock, (void *)readbuf, 4096);
	if(readlen<=0){
		// 
		return 0;
	}
	msg = (struct msg_head *)readbuf;

	printf("readlen %d, get msg len %d, msg_type %d\n", readlen, msg->len, msg->msg_type);
	if(msg->len!=readlen){
		//TODO	
	}
	// if send complete
	switch(msg->msg_type){
		case NM_GET_RULES:
			send_policy(sock);
			break;
		case NM_REPORT_COUNT:
			parse_report(msg);
			break;
		default:
			break;
	}
	free(readbuf);

	thread_add_read(thread->master, msg_deal, NULL, sock);
	return 0;
}

int msg_sock_listen(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	int accept_sock;
	int client_len;
	struct sockaddr_in client;

	printf("In msg_sock_listen\n");

	memset(&client, 0, sizeof(struct sockaddr_in));
	client_len = sizeof(struct sockaddr_in);
	accept_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&client_len);

	if(accept_sock<0){
		//TODO
		goto listen_end;
	}
	thread_add_read(thread->master, msg_deal, NULL, accept_sock);

listen_end:
	thread_add_read(thread->master, msg_sock_listen, NULL, sock);
	return 0;
}

int main(void)
{
	int sock;

	struct thread thread;
	master = thread_master_create();
	if(NULL==master){
		//TODO
		return -1;
	}

	sock = create_sock();	
	if(sock<0){
		//TODO
		printf("Fail to create socket\n");
		return -1;
	}

	thread_add_read(master, msg_sock_listen, NULL, sock);	
	memset(&thread, 0, sizeof(struct thread));
	while(thread_fetch(master, &thread)){
		thread_call(&thread);
	}

	return 0;
}
