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
#include "packet.h"
#include "vsecplat_config.h"
#include "vsecplat_interface.h"
#include "vsecplat_status.h"
#include "vsecplat_record.h"

struct thread_master *master=NULL;
int main(void)
{
	// test_policy_parse();
	int ret=0;
#if 0
	int sock=0;
	struct thread thread;
#endif

	// parse configfile and init global descriptor: global_vsecplat_config
	ret = parse_vsecplat_config();	
	if(ret<0){
		printf("Failed to parse vsecplat config.\n");
		return -1;
	}

	// init global interface list: vsecplat_interface_list
	ret = init_vsecplat_interface_list();
	if(ret<0){
		printf("Failed to get interface.\n");
		return -1;
	}

	// setup mgt interface ip address and set it to up
	ret = setup_mgt_interface();
	if(ret<0){
		printf("Failed to setup mgt interface.\n");
		return -1;
	}

	// set dataplane interface to netmap mode
	ret = setup_dp_interfaces();
	if(ret<0){
		printf("Failed to setup dataplane interface.\n");
		return -1;
	}

	// init app status desc: global_vsecplat_status
	ret = init_vsecplat_status();
	if(ret<0){
		printf("Failed to init vsecplat status.\n");
		return -1;
	}

	// init record bucket: record_bucket_hash
	ret = vsecplat_init_record_bucket();
	if(ret<0){
		printf("Failed to init record bucket.\n");
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
		memset(&thread, 0, sizeof(struct thread));

		while(thread_fetch(master, &thread)){
			thread_call(&thread);
		}
	} else
#endif
	{
		// In child process, will deal with the packet
		packet_handle_loop();
	}

	return 0;
}
