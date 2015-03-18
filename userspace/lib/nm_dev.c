#include <nm_desc.h>
#include <nm_dev.h>
extern struct nm_desc *global_nm_desc;

struct nm_desc *global_nm_desc=NULL;
int nm_dev_init(void)
{
	// here should create a pthread key and store globale variable
	global_nm_desc = (struct nm_desc *)malloc(sizeof(struct nm_desc));
	if(NULL==global_nm_desc){
		return -1;
	}
	bzero(global_nm_desc, sizeof(struct nm_desc));

	return 0;
}

struct nm_dev *nm_open_dev(char *name)
{
	struct nm_dev *dev = NULL;
	int fd = 0;
	struct nmreq req;

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

	if(global_nm_desc->memsize==0){
		global_nm_desc->memsize = req.nr_memsize;
		global_nm_desc->mem = mmap(NULL, req.nr_memsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	}
	printf("mem=%p\n", global_nm_desc->mem);

	strcpy(dev->name, req.nr_name);
	dev->fd = fd;
	dev->nifp = NETMAP_IF(global_nm_desc->mem, req.nr_offset);
	dev->first_tx_ring = 0;
	dev->first_rx_ring = 0;
	dev->last_tx_ring = req.nr_tx_rings;
	dev->last_rx_ring = req.nr_rx_rings;
	printf("%s, fd=%d, first_tx_ring=%d, first_rx_ring=%d, last_tx_ring=%d, last_rx_ring=%d\n", 
		dev->name, dev->fd, dev->first_tx_ring, dev->first_rx_ring, dev->last_tx_ring, dev->last_rx_ring);

	return dev;
}
