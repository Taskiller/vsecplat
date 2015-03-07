PROGS=bridge

CLEANFILES = $(PROGS) *.o

CFLAGS = -g
CFLAGS += -Wall -Wunused-function -Wextra
INCLUDES= -I../include/ -I$(root_dir)/include/
LIBS = -L$(root_dir)/userspace/lib/libnetmap

all: $(PROGS)
	cp -f $(PROGS) $(root_dir)/rootfs/usr/bin/

bridge: bridge.o
	$(CC) $(LIBS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

clean:
	-@rm -rf $(CLEANFILES)
	

