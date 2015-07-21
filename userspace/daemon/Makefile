TARGETS := vsecplat test_deamon

target_OBJECTS = objs/daemon.o \
				 objs/thread.o \
				 objs/rte_json.o \
				 objs/vsecplat_config.o \
				 objs/vsecplat_interface.o \
				 objs/vsecplat_policy.o \
				 objs/vsecplat_record.o \
				 objs/msg_comm.o \
				 objs/stream.o 	\
				 objs/packet.o  \
				 objs/tea_crypt.o \
				 objs/nm_log.o

test_deamon_OBJECTS = objs/test_deamon.o \
				 objs/thread.o \
				 objs/tea_crypt.o

CFLAGS = -g -Wall
CC = gcc
NM = nm
INCLUDES= -I../include/ -I$(root_dir)/include/
LIBS_INCLUDE = -L$(root_dir)/userspace/lib/
LIBS = -ldl -lcrypt -lm -lnetmap -lpthread
COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LINK = $(COMPILE) -o $@

all: $(TARGETS)

vsecplat: $(target_OBJECTS) 
	$(LINK) $(target_OBJECTS) $(LIBS) $(LIBS_INCLUDE)

test_deamon: $(test_deamon_OBJECTS)
	$(LINK) $(test_deamon_OBJECTS) $(LIBS) $(LIBS_INCLUDE)

objs/test_deamon.o:test_deamon.c
	$(COMPILE) -c -o $@ $<

objs/daemon.o:daemon.c
	$(COMPILE) -c -o $@ $<

objs/thread.o:thread.c
	$(COMPILE) -c -o $@ $<

objs/rte_json.o:rte_json.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_config.o:vsecplat_config.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_interface.o:vsecplat_interface.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_policy.o:vsecplat_policy.c
	$(COMPILE) -c -o $@ $<

objs/vsecplat_record.o:vsecplat_record.c
	$(COMPILE) -c -o $@ $<

objs/msg_comm.o:msg_comm.c
	$(COMPILE) -c -o $@ $<

objs/stream.o:stream.c
	$(COMPILE) -c -o $@ $<

objs/packet.o:packet.c
	$(COMPILE) -c -o $@ $<

objs/tea_crypt.o:tea_crypt.c
	$(COMPILE) -c -o $@ $<

objs/nm_log.o:nm_log.c
	$(COMPILE) -c -o $@ $<

clean:
	- rm -f objs/*.o .tmp*
	- rm -f $(TARGETS)

