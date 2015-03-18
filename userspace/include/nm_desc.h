#ifndef __NM_DESC_H__
#define __NM_DESC_H__
#include <nm_type.h>

struct nm_desc{
	int memsize;
	void *mem;
	int rx_if_idx;
	int rx_ring_id;

	struct pollfd fds[NM_MAX_NIC];
	int fds_num;
	struct nm_dev *nm_dev[NM_MAX_NIC];
};

#endif
