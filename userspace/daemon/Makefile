TARGETS := vsecplat  server

target_OBJECTS = objs/daemon.o \
				 objs/thread.o \
				 objs/rte_json.o \
				 objs/vsecplat_config.o \
				 objs/vsecplat_status.o \
				 objs/vsecplat_interface.o \
				 objs/msg_comm.o \
				 objs/packet.o 

server_OBJECTS = objs/server.o \
				 objs/thread.o

CFLAGS = -g 
LIBS = -ldl -lcrypt -lm
CC = gcc
NM = nm
INCLUDES= -I../include/ -I$(root_dir)/include/
LIBS_INCLUDE = -L$(root_dir)/userspace/lib/
COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LINK = $(COMPILE) -o $@

all: $(TARGETS)

vsecplat: $(target_OBJECTS) 
	$(LINK) $(target_OBJECTS) $(LIBS) $(LIBS_INCLUDE)

server: $(server_OBJECTS) 
	$(LINK) $(server_OBJECTS) $(LIBS) 

objs/server.o:server.c
	$(COMPILE) -c -o $@ $<

objs/daemon.o:daemon.c
	$(COMPILE) -c -o $@ $<

objs/thread.o:thread.c
	$(COMPILE) -c -o $@ $<

objs/rte_json.o:rte_json.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_config.o:vsecplat_config.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_status.o:vsecplat_status.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_interface.o:vsecplat_interface.c
	$(COMPILE) -c -o $@ $<

objs/msg_comm.o:msg_comm.c
	$(COMPILE) -c -o $@ $<

objs/packet.o:packet.c
	$(COMPILE) -c -o $@ $<

clean:
	- rm -f objs/*.o .tmp*
	- rm -f $(TARGETS)

