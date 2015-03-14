#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "linklist.h"
#include "sock_common.h"
#include "xml_common.h"

static struct list *guish_conn_list=NULL;
extern  int separte_fw_hole_packet(void);

int serv_un_stream(char *path)
{
	int ret;
	int sock, len;
	struct sockaddr_un serv;
	mode_t old_mask;

	/* First of all, unlink existing socket */
	unlink(path);
	old_mask = umask(0077);

	/* Make UNIX domain socket. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sock<0){
		//TODO
		return -1;
	}

	/* Make server socket. */
	memset(&serv, 0, sizeof(struct sockaddr_un));
	serv.sun_family = AF_UNIX;
	strncpy(serv.sun_path, path, strlen(path));
	len = sizeof(serv.sun_family) + strlen(serv.sun_path);

	ret = bind(sock, (struct sockaddr *)&serv, len);
	if(ret<0){
		//TODO
		close(sock);
		return -1;
	}

	ret = listen(sock, 5);
	if(ret<0){
		//TODO
		close(sock);
		return -1;
	}
	umask(old_mask);

	return sock;
}

static int guish_conn_clear(GUICON *guicon)
{
	if(guicon->rbuf){
		free(guicon->rbuf);
		guicon->rbuf = NULL;
	}
	if(guicon->send_buf_p){
		free(guicon->send_buf_p);
		guicon->send_buf_p = NULL;
	}
	guicon->plen = 0;
	guicon->dlen = 0;
	guicon->blen = 0;
	return 0;
}

static int guish_conn_del(GUICON *guicon)
{
	struct listnode *listnode=NULL;
	GUICON *tmp=NULL;
	THREAD_OFF(guicon->r_th);
	THREAD_OFF(guicon->w_th);
	THREAD_OFF(guicon->rt_th);
	THREAD_OFF(guicon->rt_th);

	if(guicon->fd){
		close(guicon->fd);
	}

	if(guicon->rbuf){
		free(guicon->rbuf);
		guicon->rbuf = NULL;
	}

	if(guicon->send_buf_p){
		free(guicon->send_buf_p);
		guicon->send_buf_p = NULL;
	}
	LIST_LOOP(guish_conn_list, tmp, listnode){
		if(guicon->fd == tmp->fd){
			listnode_delete(guish_conn_list, tmp);
			break;
		}
	}
	free(guicon);
	return 0;
}

/* 用于处理accept socket 上的事件 */
int xml_read(struct thread *thread)
{
	int readlen=0;
	int sock = THREAD_FD(thread);
	GUICON *guicon = THREAD_ARG(thread);
	void *rebuf=NULL;
	utm_cmd_head *head_info=NULL;	
	utm_cmd_body *cmd_body=NULL;
	char session_id[33];
	int product=0, version=0, type=0, total_len=0;
	int ret=0;
	int str_len=0;
	
	memset(session_id, 0, 33);
	guicon->r_th = NULL;
	if(guicon->send_buf_p){
		free(guicon->send_buf_p);
		guicon->send_buf_p = NULL;
	}
	if(NULL==guicon->rbuf){
		guicon->rbuf = malloc(GUISH_READ_BUFSIZ);
		if(NULL==guicon->rbuf){
			//TODO
			guish_conn_del(guicon);
			return 0;
		}
		guicon->blen = GUISH_READ_BUFSIZ;
	}else if(guicon->blen < guicon->plen){
		rebuf = realloc(guicon->rbuf, guicon->plen+1);
		if(NULL==rebuf){
			//TODO
			guish_conn_del(guicon);
			return 0;
		}
		guicon->rbuf = rebuf;
		guicon->blen = guicon->plen;
	}
	readlen = read(sock, guicon->rbuf+guicon->dlen, guicon->blen-guicon->dlen);
	if(readlen<=0){
		//TODO
		guish_conn_del(guicon);
		return 0;
	}
	guicon->dlen += readlen;
	memset(guicon->rbuf+guicon->dlen, 0, 1);
	if (guicon->dlen >= sizeof(utm_cmd_head)) {
		head_info = (utm_cmd_head *)guicon->rbuf;
		product  = ntohl(head_info->product);
		version  = ntohl(head_info->version);
		type      = ntohl(head_info->type);
		total_len = ntohl(head_info->length);
		strncpy(session_id, head_info->session_id, 32);

		// printf("product = %d, version = %d, type = %d\n", product, version, type);

		if((_UTM_GUI_PRODUCT_TYPE_WEB != product)||(_UTM_GUI_PRODUCT_VERSION != version)){
			//TODO
			guish_conn_del(guicon);
			return 0;
		}
		guicon->plen = total_len;
		if(guicon->dlen == total_len){
			cmd_body = (utm_cmd_body *)(guicon->rbuf+sizeof(utm_cmd_head));
			str_len = strlen(cmd_body->data);
			// printf("Get XML file :\n %s\n session_id:%s\n", cmd_body->data, session_id);
			if(UCT_CMD!=type){
				if (type == UCT_UPDATE)
				{
						int pid = fork();
						if (pid == 0)
						{
							char upload_file[256];
							char replace_cmd[512];
							memset(upload_file, 0, sizeof(upload_file));
							memset(replace_cmd, 0, sizeof(replace_cmd));
							strncpy(upload_file, cmd_body->data, sizeof(upload_file)-1);
							snprintf(replace_cmd, 512, "mv %s %s", upload_file, "/tmp/utmvsos.pkg");
							system(replace_cmd);
							ret = separte_fw_hole_packet();
							xml_return_code(ret, guicon, head_info );

							exit(0);
						}
				}
				guish_conn_clear(guicon);
				goto add_thread;
			}
			ret = deal_xml(cmd_body->data, str_len, guicon, head_info);
			if(ret<0){
				//TODO
				guish_conn_del(guicon);
				return 0;
			}else{
				//TODO
				guish_conn_clear(guicon);
			}
		}
	}
add_thread:
	guicon->r_th = thread_add_read(thread->master, xml_read, guicon, sock);
	return 0;
}

/* 用于处理listen socket上的事件 */
int xml_sock_listen(struct thread *thread)
{
	int sock = THREAD_FD(thread);
	int accept_sock;
	int client_len;
	struct sockaddr_in client;
	struct listnode *listnode;	
	GUICON *guicon=NULL;
	int found=0;

	// printf("In xml_sock_listen.\n");
	
	memset(&client, 0, sizeof(struct sockaddr_in));
	client_len = sizeof(struct sockaddr_in);
	accept_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&client_len);

	if(accept_sock<0){
		//TODO
		goto listen_end;
	}

	if(NULL==guish_conn_list){
		guish_conn_list = list_new();
	}
	LIST_LOOP(guish_conn_list, guicon, listnode){
		if(guicon->fd == accept_sock){
			found = 1;
			break;
		}
	}
	if(0==found){
		guicon = (GUICON *)malloc(sizeof(GUICON));
		if(NULL==guicon){
			//TODO
			goto listen_end;
		}
		memset(guicon, 0, sizeof(GUICON));	
		guicon->fd = accept_sock;
		guicon->type = _UTM_GUI_PRODUCT_TYPE_WEB;
		guicon->r_th = thread_add_read(thread->master, xml_read, guicon, accept_sock);
	}else{
		guicon->type = _UTM_GUI_PRODUCT_TYPE_WEB;
		if(NULL==guicon->r_th){
			guicon->r_th = thread_add_read(thread->master, xml_read, guicon, accept_sock);
		}	
	}

listen_end:
	thread_add_read(thread->master, xml_sock_listen, NULL, sock);
	return 0;
}
