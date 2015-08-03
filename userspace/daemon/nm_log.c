#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#define NM_LOG_FILE "/tmp/nm_status.log"
static int nm_log_fd = 0;
int nm_log_init(void)
{
	nm_log_fd = open(NM_LOG_FILE, O_RDWR|O_CREAT, 0644);
	if(nm_log_fd<0){
		return -1;
	}

	return 0;
}

int nm_log(const char *fmt, ...)
{
	int n, size=1024;
	char *p=NULL, *np=NULL;
	va_list ap;

	if(nm_log_fd<=0){
		nm_log_fd = open(NM_LOG_FILE, O_RDWR|O_CREAT, 0644);
		if(nm_log_fd<0){
			return -1;
		}
	}
	p = malloc(size);
	if(NULL==p){
		return -1;
	}
	while(1){
		va_start(ap, fmt);
		n = vsnprintf(p,size, fmt, ap);
		va_end(ap);
		if((n>-1)&(n<size)){
			break;
		}

		if(n>-1){
			size = (n>size)?(n+1):size*2;
		}else{
			size *= 2;
		}
		if(size>16*1024){
			free(p);
			return -1;
		}
		if((np=realloc(p, size))==NULL){
			free(p);
			return -1;
		}else{
			p=np;
		}
	}

//	printf("%s\n", p); // For Test

	write(nm_log_fd, p, n);

	return 0;
}
