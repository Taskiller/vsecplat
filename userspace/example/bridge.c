#include <stdlib.h>
#include <stdio.h>

#include <nm_skb.h>
#include <nm_desc.h>
#include <nm_dev.h>

#include <netmap_user.h>

int main(int argc, char *argv[])
{
	struct nm_dev *dev1 = NULL;
	struct nm_dev *dev2 = NULL;
	nm_init();		

	dev1 = nm_registe_dev("eth1");
// 	dev2 = nm_registe_dev("eth2");
	while(1)
		sleep(10);
	return 0;
}
