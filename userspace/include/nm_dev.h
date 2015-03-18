#ifndef __NM_DEV_H__
#define __NM_DEV_H__
#include "nm_type.h"

struct nm_dev{
	char name[NM_NAME_LEN];
	int fd;
	struct netmap_if *nifp;
	u16 first_tx_ring, last_tx_ring, cur_tx_ring;
	u16 first_rx_ring, last_rx_ring, cur_rx_ring;
};


int nm_dev_init(void);
struct nm_dev *nm_open_dev(char *name);

#endif
