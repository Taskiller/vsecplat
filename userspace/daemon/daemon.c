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

#include "thread.h"
#include "msg_comm.h"
#include "vsecplat_config.h"
#include "vsecplat_interface.h"

struct thread_master *master=NULL;
int main(void)
{
	int ret=0;
	int sock=0;
	struct thread thread;
	struct vsecplat_interface *ifp=NULL;
	// parse configfile and init global descriptor
	ret = parse_vsecplat_config();	
	if(ret<0){
		printf("Failed to parse vsecplat config.\n");
		return -1;
	}

	ret = init_vsecplat_interface_list();	
	if(ret<0){
		printf("Failed to get interface.\n");
		return -1;
	}

	ifp = vsecplat_get_interface_by_mac(global_vsecplat_config->mgt_cfg->mac);	
	if(NULL!=ifp){
		printf("find mgt interface : %s\n", ifp->name);
	}

	ret = init_vsecplat_status();
	if(ret<0){
		printf("Failed to init vsecplat status.\n");
		return -1;
	}
#if 0
//	ret = fork();

//	if(ret>0)
	{ // parent
		master = thread_master_create();
		if(NULL==master){
			//TODO
			return -1;
		}

		thread_add_timer(master, vsecplat_timer_func, NULL, 5);	
		// thread_add_read(master, xml_sock_listen, NULL, sock);	
		memset(&thread, 0, sizeof(struct thread));

		while(thread_fetch(master, &thread)){
			thread_call(&thread);
		}
	}
#if 0
	else{
		// In child process, will deal with the packet
		packet_handle_loop();
	}
#endif
#endif

	return 0;


}
