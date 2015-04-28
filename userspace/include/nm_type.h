#ifndef __NM_TYPE_H__
#define __NM_TYPE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <net/if.h>
#include <poll.h>

#include <netmap.h>
#include "netmap_user.h"

#define NM_NAME_LEN 16
#define NM_ADDR_STR_LEN 18
#define NM_IP_LEN 18
#define NM_MAC_LEN 6
#define NM_MAX_NIC 8

typedef signed char 	s8;
typedef unsigned char 	u8;
typedef signed short 	s16;
typedef unsigned short 	u16;
typedef signed int 		s32;
typedef unsigned int 	u32;
typedef signed long 	s64;
typedef unsigned long 	u64;

#endif
