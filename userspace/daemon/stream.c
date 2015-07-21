#include <unistd.h>

int readn(int fd, char *ptr, int nbytes)
{
	int nleft;
	int nread;

	nleft = nbytes;
	while(nleft>0){
		nread = read(fd, ptr, nleft);
		if(nread<0){
			return nread;
		}else if(nread==0){
			break;
		}
		nleft -= nread;
		ptr += nread;
	}
	return (nbytes-nleft);
}


int writen(int fd, char *ptr, int nbytes)
{
	int nleft;
	int nwritten;

	nleft = nbytes;
	while(nleft>0){
		nwritten = write(fd, ptr, nleft);
		if(nwritten<=0){
			return nwritten;
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (nbytes-nleft);
}
