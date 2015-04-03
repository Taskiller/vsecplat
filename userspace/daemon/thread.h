#ifndef __THREAD_H__
#define __THREAD_H__

struct thread;
struct thread_list{
	struct thread *head;
	struct thread *tail;
	int count;
};

struct thread_master{
	struct thread_list read;
	struct thread_list write;
	struct thread_list timer;
	struct thread_list ready;
	struct thread_list unuse;
	fd_set readfd;
	fd_set writefd;
	fd_set exceptfd;
	unsigned long alloc;
};

struct thread{
	unsigned char type;
	struct thread *next;
	struct thread *prev;
	struct thread_master *master;
	int (*func)(struct thread *);
	void *arg;
	union{
		int val;
		int fd;
		struct timeval sands;
	}u;
};

#define THREAD_READ 0
#define THREAD_WRITE 1
#define THREAD_TIMER 2
#define THREAD_EVENT 3
#define THREAD_READY 4
#define THREAD_UNUSED 6

#define THREAD_ARG(x) ((x)->arg)
#define THREAD_FD(x) ((x)->u.fd)
#define THREAD_VAL(x) ((x)->u.val)

void thread_cancel(struct thread *thread);
#define THREAD_OFF(thread) \
	do{	\
		if(thread){ \
			thread_cancel(thread); \
			thread = NULL; \
		} \
	}while(0);

struct thread_master *thread_master_create(void);
struct thread *thread_add_read(struct thread_master *,
						int (*)(struct thread *), void *, int);
struct thread *thread_add_write(struct thread_master *,
						int (*)(struct thread *), void *, int);
struct thread *thread_add_timer(struct thread_master *,
						int (*)(struct thread *), void *, long);
struct thread *thread_add_event(struct thread_master *,
						int (*)(struct thread *), void *, int);
struct thread *thread_fetch(struct thread_master *, struct thread *);
void thread_call(struct thread *);
#endif
