
CC = gcc
#CFLAGS = -Wall -O0 -g 
CFLAGS = -O0 -g -DDEBUG
LDFLAGS = -lpthread

OBJS = proxy.o util.o

all: proxy

proxy: $(OBJS)

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

clean:
	rm -f *~ *.o proxy

