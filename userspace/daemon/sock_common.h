#ifndef __SOCK_COMMON_H__
#define __SOCK_COMMON_H__
#include "thread.h"

#define XML_BUF_LEN 4096

int serv_un_stream(char *path);
int xml_sock_listen(struct thread *thread);

#define _UTM_GUI_PRODUCT_TYPE_WEB	1
#define _UTM_GUI_PRODUCT_TYPE_MGR	2
#define _UTM_GUI_KEEPALIVE	"keep_alive"

#define _UTM_GUI_PRODUCT_VERSION		3
#define GUISH_READ_BUFSIZ	4096
typedef struct guish_conn {
	int	fd;		//
	int ssl_sock;
	int	type;	// client type : 1, WEB; 2, MGR
	long	last_active_time;	// last update data

	int	plen;	// length of packet 
	int	dlen;	// length of data
	int	blen;	// length of buf
	int     login;  // 1: login; nothing else
	int     multi_oper;
	char    *send_buf_p;
	void	*rbuf;	// buf of data
	char *sbuf;
	int s_len;
	int sended_len;
	char flash;
	char pad1;
	char pad2;
	char pad3;
	char session_id[32];
	struct thread *r_th;	// thread of read data
	struct thread *w_th;	// thread of read data
	struct thread *rt_th;	// thread of read data
	struct thread *t_th; /*thread for telnet timeout*/
}GUICON;

#endif
