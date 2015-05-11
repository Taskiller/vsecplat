#ifndef __NM_MUTEX_H__
#define __NM_MUTEX_H__
#include <pthread.h>

struct nm_mutex{
	pthread_mutex_t lock;
};

int nm_mutex_init(struct nm_mutex *mutex);
int nm_mutex_lock(struct nm_mutex *mutex);
int nm_mutex_unlock(struct nm_mutex *mutex);

#endif
