#ifndef __NM_DESC_H__
#define __NM_DESC_H__
#include "nm_type.h"

struct nm_desc{
	int memsize;
	void *mem;
	int need_poll;
	int rx_if_idx;
	int rx_ring_idx;

	int fds_num;
	struct pollfd fds[NM_MAX_NIC];
	struct nm_dev *nm_dev[NM_MAX_NIC];
};

int nm_desc_init(void);

#endif
