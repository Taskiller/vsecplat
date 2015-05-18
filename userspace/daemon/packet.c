#include "packet.h"

int packet_handle_loop(void)
{
	struct nm_skb *skb=NULL;

	do{
		skb = nm_recv();
		if(NULL==skb){
			return -1;
		}

	}while(1);
	return 0;
}
