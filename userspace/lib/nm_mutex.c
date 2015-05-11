#include "nm_type.h"
#include "nm_mutex.h"

int nm_mutex_init(struct nm_mutex *mutex)
{
	return pthread_mutex_init(&mutex->lock, NULL);		
}

int nm_mutex_lock(struct nm_mutex *mutex)
{
	return pthread_mutex_lock(&mutex->lock);
}

int nm_mutex_unlock(struct nm_mutex *mutex)
{
	return pthread_mutex_unlock(&mutex->lock);
}

