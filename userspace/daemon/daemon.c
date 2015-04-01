#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct thread_master *master=NULL;

struct config_desc *init_global_desc(void)
{
	
}

int main(void)
{
	int ret=0;
	int sock=0;
	struct thread thread;

	// parse configfile and init global descriptor

	ret = fork();	

	if(ret>0){ // parent
		master = thread_master_create();
		if(NULL==master){
			//TODO
			return -1;
		}
	
		sock = serv_un_stream(GUI_XML_PATH);	
		if(sock<0){
			//TODO
			return -1;
		}

		thread_add_read(master, xml_sock_listen, NULL, sock);	
		memset(&thread, 0, sizeof(struct thread));

		while(thread_fetch(master, &thread)){
			thread_call(&thread);
		}
	}

	// In child process, will deal with the packet
	packet_handle_loop();

	return 0;


}
