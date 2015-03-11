#ifndef __NM_DEV_H__
#define __NM_DEV_H__
#include "nm_type.h"

struct nm_dev{
	char name[NM_NAME_LEN];
	int ifindex;
};


int nm_init(void);
struct nm_dev *nm_registe_dev(char *name);

#endif
