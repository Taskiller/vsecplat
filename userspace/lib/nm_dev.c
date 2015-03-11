#include <nm_dev.h>

int nm_init(void)
{
	int fd = 0;
	struct nmreq req;		
	unsigned int memsize;
	void *mem=NULL;

	fd = open("/dev/netmap", O_RDWR);
	if(fd<0){
		printf("Can't open netmap device!\n");
		return -1;
	}

	memset(&req, 0, sizeof(struct nmreq));	
	req.nr_version = NETMAP_API;
	
	if(ioctl(fd, NIOCGINFO, &req)){
		printf("netmap NIOCGINFO failed.\n");
		close(fd);
	}
	memsize = req.nr_memsize;		
	printf("memsize = %dMB\n", memsize>>20);

	mem = mmap(NULL, memsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	
	return 0;
}

struct nm_dev *nm_registe_dev(char *name)
{
	struct nm_dev *dev = NULL;
	int fd = 0;
	struct nmreq req;

	int namelen = strlen(name);

	// check the dev name is valid
	
	dev = (struct nm_dev *)malloc(sizeof(struct nm_dev));
	if(NULL==dev){
		return NULL;
	}
	memset(dev, 0, sizeof(struct nm_dev));

	fd = open("/dev/netmap", O_RDWR);
	if(fd<0){
		printf("Can't open netmap device!\n");
		free(dev);
		return NULL;
	}
	memset(&req, 0, sizeof(struct nmreq));	
	req.nr_version = NETMAP_API;
	req.nr_flags = NR_REG_ALL_NISC;
	strncpy(req.nr_name, name, namelen);
	req.nr_name[namelen] = '\0';
	
	if(ioctl(fd, NIOCREGIF, &req)){
		printf("Dev %s NIOCREGIF failed\n", name);
		return NULL;
	}

	return dev;
}
