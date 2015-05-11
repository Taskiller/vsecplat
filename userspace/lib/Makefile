CFLAGS = -g
CFLAGS += -fPIC -Wall -Wunused-function -Wextra
INCLUDES= -I../include/ -I$(root_dir)/include/
LIBS=-lpthread

LIB=libnetmap.so

all: $(LIB)

LIBOBJS=nm_skb.o nm_dev.o nm_desc.o nm_eth.o nm_mutex.o nm_jhash.o

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(LIB):$(LIBOBJS)
	$(CC) $(LIBS) -shared -o $@ $^

clean:
	-@rm -rf $(LIB) *.o
