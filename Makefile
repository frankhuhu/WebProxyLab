
CC = gcc
#CFLAGS = -Wall -O0 -g 
CFLAGS = -O0 -g -DDEBUG
LDFLAGS = -lpthread

OBJS = proxy.o util.o cache.o

all: proxy

proxy: $(OBJS)

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

cache.o: cache.c
	$(CC) $(CFLAGS) -c cache.c

clean:
	rm -f *~ *.o proxy

