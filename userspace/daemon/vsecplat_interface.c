#include <sys/ioctl.h>
#include "nm_dev.h"

int vsecplat_get_all_interface(void)
{
	int fd=0;	
	int if_num=0;
	struct ifconf ioctl_req;
	struct ifreq if_vector[32];
	struct ifreq *ifendp=NULL, *ifpi=NULL;

	if((fd = socket(AF_INET, SOCK_DGRAM, 0))<0) {
		//TODO
		return -1;
	}

	ioctl_req.ifc_len = sizeof(if_vector);
	ioctl_req.ifc_buf = (void *)if_vector;
	if(ioctl(fd, SIOCGIFCONF, &ioctl_req)<0){
		//TODO
		close(fd);
		return -1;
	}

	ifendp = (struct ifreq *)((char *)if_vector + ioctl_req.ifc_len);
	for(ifpi=if_vector; ifpi<ifendp; ifpi++){
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		if(ifpi->ifr_addr.sa_family != AF_INET){
			continue;
		}
		strcpy(ifr.ifr_name, ifpi->ifr_name);
		ioctl(fd, SIOCGIFHWADDR, &ifr);

		printf("interface name: %s, ifr_hwaddr.sa_data=%x:%x:%x:%x:%x:%x\n", 
					ifpi->ifr_name,
					ifr.ifr_hwaddr.sa_data[0] & 0xff,
					ifr.ifr_hwaddr.sa_data[1] & 0xff,
					ifr.ifr_hwaddr.sa_data[2] & 0xff,
					ifr.ifr_hwaddr.sa_data[3] & 0xff,
					ifr.ifr_hwaddr.sa_data[4] & 0xff,
					ifr.ifr_hwaddr.sa_data[5] & 0xff);
	}

	close(fd);
	return 0;
}

struct nm_dev *vsecplat_get_dev_by_mac(char *mac)
{
	struct nm_dev *dev = NULL;
}

