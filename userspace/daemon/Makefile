TARGETS := vsecplat  server

target_OBJECTS = objs/daemon.o \
				 objs/thread.o \
				 objs/rte_json.o \
				 objs/config.o \
				 objs/msg_comm.o \
				 objs/packet.o 

server_OBJECTS = objs/server.o \
				 objs/thread.o

CFLAGS = -g 
LIBS = -ldl -lcrypt -lm
CC = gcc
NM = nm
COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LINK = $(COMPILE) -o $@

all: $(TARGETS)

vsecplat: $(target_OBJECTS) 
	$(LINK) $(target_OBJECTS) $(LIBS) 

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

objs/config.o:config.c
	$(COMPILE) -c -o $@ $<

objs/msg_comm.o:msg_comm.c
	$(COMPILE) -c -o $@ $<

objs/packet.o:packet.c
	$(COMPILE) -c -o $@ $<

clean:
	- rm -f objs/*.o .tmp*
	- rm -f $(TARGETS)
