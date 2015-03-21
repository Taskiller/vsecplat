#include <nm_desc.h>
struct nm_desc *global_nm_desc=NULL;
int nm_desc_init(void)
{
	// here should create a pthread key and store globale variable
	global_nm_desc = (struct nm_desc *)malloc(sizeof(struct nm_desc));
	if(NULL==global_nm_desc){
		return -1;
	}
	bzero(global_nm_desc, sizeof(struct nm_desc));

	global_nm_desc->need_poll = 1;	

	INIT_LIST_HEAD(&(global_nm_desc->tx_pool.list));		

	return 0;
}


