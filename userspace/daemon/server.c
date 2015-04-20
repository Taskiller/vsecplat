#include <stdio.h>
#include <stdlib.h>
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
	char *file_buf=NULL;

	printf("In send_policy\n");

	memset(&stat_buf, 0, sizeof(struct stat));
	stat("./policy.json", &stat_buf);
	len = stat_buf.st_size;
	
	file_buf = malloc(len);
	if(NULL==file_buf){
		return -1;
	}
	memset(file_buf, 0, len);
	fd = open("./policy.json", O_RDONLY);
	if(fd<0){
		free(file_buf);
		return -1;
	}
	read(fd, file_buf, len);
	close(fd);
	
	write(sock, file_buf, len);
	free(file_buf);
	printf("end send_policy\n");
	return 0;
}

int msg_deal(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	void *readbuf=NULL;
	int readlen;

	printf("In msg_deal\n");	
	readbuf = malloc(4096);
	memset(readbuf, 0, 4096);

	readlen = read(sock, readbuf, 4096);
	if(readlen<=0){
		// 
		return 0;
	}
	printf("get msg: %s\n", readbuf);
	// if send complete
	if(!strcmp(readbuf, "get policy")){
		send_policy(sock);
	}else{
		free(readbuf);
	}

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
