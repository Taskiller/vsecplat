#ifndef __NM_DEV_H__
#define __NM_DEV_H__
#include "nm_type.h"

struct nm_dev{
	char name[NM_NAME_LEN];
	int fd;
	struct netmap_if *nifp;
	int first_tx_ring, last_tx_ring, cur_tx_ring;
	int first_rx_ring, last_rx_ring, cur_rx_ring;
};

struct nm_dev *nm_open_dev(char *name);
int nm_registe_dev(struct nm_dev *dev);

#endif
