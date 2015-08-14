#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <execinfo.h>
#include <signal.h>
#include <time.h>

#define VSECPLAT_EXCEPTION_FILE "/mnt/exception.txt"

static void rte_record_backtrace(int sig)
{
	int fd;
	char buff[2048];

	void *bt[32];
	int bt_size;
	char **bt_sym;
	int i;

	time_t now;
	struct tm tm;
	struct timeval tv;
	struct timezone tz;

	char *buf_ops = buff;

	time(&now);
	gettimeofday(&tv, &tz);
	gmtime_r(&now, &tm);

	buf_ops += sprintf(buf_ops, "[%d-%d-%d %d:%d:%d] exception for signal %d\n",
				tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, sig);
	buf_ops += sprintf(buf_ops, "Backtrace:\n");
	bt_size = backtrace(bt, 32);
	bt_sym = backtrace_symbols(bt, bt_size);
	for(i=0;i<bt_size;i++){
		buf_ops += sprintf(buf_ops, "%p: %s\n", bt[i], bt_sym[i]);
	}

	free(bt_sym);
	fd = open(VSECPLAT_EXCEPTION_FILE, O_WRONLY|O_APPEND|O_CREAT, 0644);
	if(fd<0){
		return;
	}
	write(fd, buff, buf_ops-buff);
	close(fd);

	return;
}

static void rte_sigsem_int(int sig)
{
	struct sigaction sa;

	rte_record_backtrace(sig);
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(sig, &sa, NULL);
	raise(sig);
	
	return;
}

static void rte_sigsem_exec(int sig)
{
	struct sigaction action;

	rte_record_backtrace(sig);

	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(sig, &action, NULL);
	raise(sig);

	return;
}

static int rte_signal_set(int sig,void (*func)(int))
{
	int ret;
	struct sigaction action;

	action.sa_handler= func;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	ret = sigaction(sig, &action, NULL);
	if(ret<0){
		return -1;
	}

	return 0;
}

void rte_backtrace_init(void)
{
	rte_signal_set(SIGINT, rte_sigsem_int);
	rte_signal_set(SIGSEGV, rte_sigsem_exec);
	rte_signal_set(SIGPIPE, rte_sigsem_exec);
	rte_signal_set(SIGBUS, rte_sigsem_exec);
	rte_signal_set(SIGILL, rte_sigsem_exec);
	rte_signal_set(SIGFPE, rte_sigsem_exec);
	rte_signal_set(SIGABRT, rte_sigsem_exec);

	return;
}
