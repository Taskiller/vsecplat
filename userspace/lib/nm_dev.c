#include <nm_dev.h>

int nm_init(void)
{
	// here should create a pthread key and store globale variable

	return 0;
}

struct nm_dev *nm_registe_dev(char *name)
{
	struct nm_dev *dev = NULL;
	int fd = 0;
	struct nmreq req;
	void *mem = NULL;

	int namelen = strlen(name);
	if(namelen<=0 || namelen>=NM_NAME_LEN){
		return NULL;
	}	
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
	req.nr_flags = NR_REG_ALL_NIC;
	strncpy(req.nr_name, name, namelen);
	req.nr_name[namelen] = '\0';
	
	if(ioctl(fd, NIOCREGIF, &req)){
		printf("Dev %s NIOCREGIF failed\n", name);
		return NULL;
	}

	printf("dev=%s, nr_version=%d, nr_offset=0x%x, nr_mmesize=0x%x\n",
		req.nr_name, req.nr_version, req.nr_offset, req.nr_memsize);
	printf("nr_tx_slots=%d nr_rx_slots=%d, nr_tx_rings=%d, nr_rx_rings=%d, nr_ringid=%d\n",
		req.nr_tx_slots, req.nr_rx_slots, req.nr_tx_rings, req.nr_rx_rings, req.nr_ringid);


	mem = mmap(NULL, req.nr_memsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	printf("mem=%p\n", mem);

	return dev;
}
