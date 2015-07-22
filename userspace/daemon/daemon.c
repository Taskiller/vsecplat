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
#include "nm_log.h"
#include "msg_comm.h"
#include "packet.h"
#include "vsecplat_config.h"
#include "vsecplat_interface.h"
#include "vsecplat_policy.h"
#include "vsecplat_record.h"

struct thread_master *master=NULL;
int main(void)
{
	int ret=0;
	struct thread thread;
	int sock;

	pthread_t packet_thread_id;

	ret = nm_log_init();
	if(ret<0){
		printf("failed to init log.\n");
		return -1;
	}

	// parse configfile and init global descriptor: global_vsecplat_config
	ret = parse_vsecplat_config();	
	if(ret<0){
		printf("Failed to parse vsecplat config.\n");
		return -1;
	}

	// Init forward policy desc: fw_policy_list
	ret = init_policy_list();
	if(ret<0){
		printf("Failed to init policy descriptor.\n");
		return -1;
	}

	// init global interface list: vsecplat_interface_list
	ret = init_vsecplat_interface_list();
	if(ret<0){
		printf("Failed to get interface.\n");
		return -1;
	}

#if 0 // Disable to test function on host
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
#endif

	// init connection descriptor: conn_desc
	ret = init_conn_desc();
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

	vsecplat_test_record();
#if 0 // For test
	add_test_policy();
	packet_handle_thread(NULL);
#endif

#if 0
	/* 创建处理数据包的线程 */
	ret = pthread_create(&packet_thread_id, NULL, &packet_handle_thread, NULL);
	if(ret<0){
		nm_log("failed to create packet thread.\n");
		return -1;
	}
#endif

	// manage thread 
	master = thread_master_create();
	if(NULL==master){
		nm_log("failed to alloc master.\n");
		return -1;
	}

#if 1
	/* 报告统计数据的线程 */
	nm_log("Add report stats thread.\n");
	thread_add_timer(master, vsecplat_report_stats, NULL, VSECPLAT_REPORT_INTERVAL);
#endif

#if 1
	sock = create_listen_socket();
	if(sock<0){
		nm_log("failed to create socket.\n");
		return -1;
	}
	thread_add_read(master, vsecplat_listen_func, NULL, sock);
#endif

	memset(&thread, 0, sizeof(struct thread));
	while(thread_fetch(master, &thread)){
		thread_call(&thread);
	}
	return 0;
}
