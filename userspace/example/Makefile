PROGS=bridge

CLEANFILES = $(PROGS) *.o

CFLAGS = -g
CFLAGS += -Wall -Wunused-function -Wextra
INCLUDES= -I../include/ -I$(root_dir)/include/
LIBS_INCLUDE = -L$(root_dir)/userspace/lib/
LIBS = -lnetmap
all: $(PROGS)
	cp -f $(PROGS) $(root_dir)/rootfs/usr/bin/

bridge: bridge.o
	$(CC) $(LIBS) $(LIBS_INCLUDE) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

clean:
	-@rm -rf $(CLEANFILES)
	

