#ifndef __NM_TYPE_H__
#define __NM_TYPE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <net/if.h>
#include <poll.h>
#include <netmap.h>
#include <netmap_user.h>

#define NM_NAME_LEN 16
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
