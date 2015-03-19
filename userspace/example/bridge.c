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

	ret = nm_desc_init();		
	if(ret<0){
		return -1;
	}

	dev1 = nm_open_dev("eth1");
 	dev2 = nm_open_dev("eth2");
	
	nm_registe_dev(dev1, IN_DEV|OUT_DEV);	
	nm_registe_dev(dev2, IN_DEV|OUT_DEV);

	for(;;){
		skb = nm_recv();
		if(skb==NULL){
			continue;
		}
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
