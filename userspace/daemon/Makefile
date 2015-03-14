TARGETS := vsecplat 

target_OBJECTS = objs/daemon.o \
				 objs/thread.o \
				 objs/sock_common.o \
				 objs/rte_json.o

CFLAGS = -g 
LIBS = -ldl -lcrypt
CC = gcc
NM = nm
COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LINK = $(COMPILE) -o $@

all: $(TARGETS)

vsecplat: $(target_OBJECTS) 
	$(LINK) $(target_OBJECTS) $(LIBS) 

# .c.o:
#	$(COMPILE) -c $<

objs/daemon.o:daemon.c
	$(COMPILE) -c -o $@ $<

objs/thread.o:thread.c
	$(COMPILE) -c -o $@ $<

objs/sock_common.o:sock_common.c
	$(COMPILE) -c -o $@ $<

objs/rte_json.o:rte_json.c
	$(COMPILE) -c -o $@ $<

clean:
	- rm -f objs/*.o .tmp*
	- rm -f $(TARGETS)
