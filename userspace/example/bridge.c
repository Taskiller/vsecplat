#include <stdlib.h>
#include <stdio.h>

#include <nm_skb.h>
#include <nm_desc.h>
#include <nm_dev.h>

#include <netmap_user.h>
extern struct nm_desc *global_nm_desc;
int main(int argc, char *argv[])
{
	struct nm_dev *dev1 = NULL;
	struct nm_dev *dev2 = NULL;
	int ret = 0;
	struct nm_skb *skb=NULL;

	ret = nm_dev_init();		
	if(ret<0){
		return -1;
	}

	dev1 = nm_open_dev("eth1");
 	dev2 = nm_open_dev("eth2");

	global_nm_desc->fds[0].fd = dev1->fd;
	global_nm_desc->fds[0].events |= POLLIN;
	global_nm_desc->nm_dev[0] = dev1;
	global_nm_desc->fds_num++;

	global_nm_desc->fds[1].fd = dev2->fd;
	global_nm_desc->fds[1].events |= POLLOUT;
	global_nm_desc->nm_dev[1] = dev2;
	global_nm_desc->fds_num++;

	for(;;){
		skb = nm_recv();
		if(skb->i_dev == dev1){
			skb->o_dev = dev2;
		}
		if(skb->i_dev == dev2){
			skb->o_dev = dev1;
		}
		nm_send(skb);
	}

	return 0;
}
