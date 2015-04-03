#include <sys/socket.h>
#include <sys/select.h>
// #include <linux/time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "thread.h"

#define TIMER_SECOND_MICRO 1000000L
static struct timeval timeval_adjust(struct timeval a)
{
	while(a.tv_usec>=TIMER_SECOND_MICRO){
		a.tv_usec -= TIMER_SECOND_MICRO;
		a.tv_sec++;
	}

	while(a.tv_usec<0){
		a.tv_usec += TIMER_SECOND_MICRO;
		a.tv_sec--;
	}

	if(a.tv_sec<0){
		a.tv_sec=0;
		a.tv_usec=10;
	}
	if(a.tv_sec>TIMER_SECOND_MICRO){
		a.tv_sec = TIMER_SECOND_MICRO;
	}
	return a;
}

static struct timeval timeval_subtract(struct timeval a, struct timeval b)
{
	struct timeval ret;
	ret.tv_usec = a.tv_usec - b.tv_usec;
	ret.tv_sec = a.tv_sec - b.tv_sec;

	return timeval_adjust(ret);
}

static int timeval_cmp(struct timeval a, struct timeval b)
{
	return ((a.tv_sec==b.tv_sec) ? (a.tv_usec-b.tv_usec):(a.tv_sec-b.tv_sec));
}

struct thread_master *thread_master_create(void)
{
	struct thread_master *master=NULL;
	master = (struct thread_master *)malloc(sizeof(struct thread_master));	
	if(NULL!=master){
		memset(master, 0, sizeof(struct thread_master));
	}

	return master;
}

static void  thread_list_add(struct thread_list *list, struct thread *thread)
{
	thread->next = NULL;
	thread->prev = list->tail;
	if(list->tail){
		list->tail->next = thread;
	}else{
		list->head = thread;
	}
	list->tail = thread;
	list->count++;
}

static void thread_list_add_before(struct thread_list *list, struct thread *point, struct thread *thread)
{
	thread->next = point;
	thread->prev = point->prev;
	if(point->prev){
		point->prev->next = thread;
	}else{
		list->head = thread;
	}
	point->prev = thread;
	list->count++;
}

static struct thread *thread_list_delete(struct thread_list *list, struct thread *thread)
{
	if(thread->next){
		thread->next->prev = thread->prev;
	}else{
		list->tail = thread->prev;
	}
	
	if(thread->prev){
		thread->prev->next = thread->next;
	}else{
		list->head = thread->next;
	}
	thread->next = thread->prev = NULL;
	list->count--;

	return thread;
}

static int thread_empty(struct thread_list *list)
{
	return (list->head==NULL)?1:0;
}

struct thread *thread_trim_head(struct thread_list *list)
{
	if(!thread_empty(list)){
		return thread_list_delete(list, list->head);
	}	
	return NULL;
}

static struct thread *thread_get(struct thread_master *m,
				unsigned char type, 
				int (*func)(struct thread *), void *arg)
{
	struct thread *thread=NULL;
	thread = thread_trim_head(&m->unuse);
	if(NULL==thread){
		thread = (struct thread *)malloc(sizeof(struct thread));

		m->alloc++;
	}

	if(NULL!=thread){
		memset(thread, 0, sizeof(struct thread));
		thread->type = type;
		thread->master = m;
		thread->func = func;
		thread->arg = arg;
	}

	return thread;
}

struct thread *thread_add_read(struct thread_master *m,
					int (*func)(struct thread *), void *arg, int fd)
{
	struct thread *thread;
#if 0
	char file_name[64];

	memset(file_name, 0, 64);
	snprintf(file_name, 63, "/proc/%d/fd/%d", getpid(), fd);
	if(access(file_name, F_OK)<0){
		//TODO
		return NULL;
	}
#endif

	if(FD_ISSET(fd, &m->readfd)){
		//TODO
		return NULL;
	}

	thread = thread_get(m, THREAD_READ, func, arg);
	FD_SET(fd, &m->readfd);
	thread->u.fd = fd;
	thread_list_add(&m->read, thread);	

	return thread;
}

struct thread *thread_add_write(struct thread_master *m,
					int (*func)(struct thread *), void *arg, int fd)
{
	struct thread *thread;
#if 0
	char file_name[64];

	memset(file_name, 0, 64);
	snprintf(file_name, 63, "/proc/%d/fd/%d", getpid(), fd);
	if(access(file_name, F_OK)<0){
		//TODO
		return NULL;
	}
#endif
	if(FD_ISSET(fd, &m->writefd)){
		//TODO
		return NULL;
	}

	thread = thread_get(m, THREAD_WRITE, func, arg);
	FD_SET(fd, &m->writefd);
	thread->u.fd = fd;
	thread_list_add(&m->write, thread);

	return thread;
}

struct thread *thread_add_timer(struct thread_master *m,
				int (*func)(struct thread *), void *arg, long timer)
{
	struct timeval timer_now;
	struct thread *thread;
	struct thread *tt;

	thread = thread_get(m, THREAD_TIMER, func, arg );
	gettimeofday(&timer_now, NULL);
	// timer_now.tv_sec = ;
	timer_now.tv_sec += timer;
	thread->u.sands = timer_now;

	for(tt=m->timer.head; tt; tt=tt->next){
		if(timeval_cmp(thread->u.sands, tt->u.sands)<=0)
			break;
	}
	if(tt){
		thread_list_add_before(&m->timer, tt, thread);
	}else{
		thread_list_add(&m->timer, thread);
	}

	return thread;
}

struct thread *thread_add_event(struct thread_master *m,
					int (*func)(struct thread *), void *arg, int val)
{
	return NULL;
}

void thread_add_unuse(struct thread_master *m, struct thread *thread)
{
	thread_list_add(&m->unuse, thread);
}


void thread_cancel(struct thread *thread)
{
	if(NULL==thread){
		return;
	}	
	switch(thread->type){
		case THREAD_READ:
			FD_CLR(thread->u.fd, &thread->master->readfd);
			thread_list_delete(&thread->master->read, thread);
			break;
		case THREAD_WRITE:
			FD_CLR(thread->u.fd, &thread->master->writefd);
			thread_list_delete(&thread->master->write, thread);
			break;
		case THREAD_TIMER:
			break;
		case THREAD_EVENT:
			break;
		case THREAD_READY:
			thread_list_delete(&thread->master->ready, thread);
			break;
		default:
			break;
	}
	thread->type = THREAD_UNUSED;
	thread_add_unuse(thread->master, thread);
	return;
}

static struct thread *thread_run(struct thread_master *m, struct thread *thread, struct thread *fetch)
{
	*fetch = *thread; //It's ok??	
	thread->type = THREAD_UNUSED;
	thread_add_unuse(m, thread);
	return fetch;
}

static int thread_process_fd(struct thread_list *list, fd_set *fdset, fd_set *mfdset)
{
	struct thread *thread=NULL;
	struct thread *next=NULL;
	
	int ready=0;

	for(thread=list->head; thread; thread=next){
		next = thread->next;
		if(FD_ISSET(THREAD_FD(thread), fdset)){ // 如果thread的sock上发生了事件 
			FD_CLR(THREAD_FD(thread), mfdset);
			thread_list_delete(list, thread);
			thread_list_add(&thread->master->ready, thread);
			thread->type = THREAD_READY;
			ready++;
		}
	}
	return 0;
}

struct thread *thread_fetch(struct thread_master *m, struct thread *fetch)
{
	int num;
	struct thread *thread;
	struct timeval timer_val;
	struct timeval timer_now;

	fd_set readfd;
	fd_set writefd;
	fd_set exceptfd;	

	while(1){
		gettimeofday(&timer_now, NULL);
		for(thread=m->timer.head; thread; thread=thread->next){
			if(timeval_cmp(timer_now, thread->u.sands)>=0){
				thread_list_delete(&m->timer, thread);
				return thread_run(m, thread, fetch);
			}
		}

		if((thread=thread_trim_head(&m->ready))!=NULL){
			return thread_run(m, thread, fetch);
		}

		readfd = m->readfd;
		writefd = m->writefd;	
		exceptfd = m->exceptfd;

		timer_val.tv_sec = 0;
		timer_val.tv_usec = 10;
		num = select(FD_SETSIZE, &readfd, &writefd, &exceptfd, &timer_val);
		if(num==0){
			continue;
		}
		if(num<0){
			if(errno==EINTR){
				continue;
			}
			//TODO
			return NULL;
		}

		/* Read thread */
		thread_process_fd(&m->read, &readfd, &m->readfd);

		/* Write thread */
		thread_process_fd(&m->write, &writefd, &m->writefd);
	}

	return NULL;
}

void thread_call(struct thread *thread)
{

	(*thread->func)(thread);		

	return;
}
