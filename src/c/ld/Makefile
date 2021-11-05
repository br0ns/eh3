CC = cc
CFLAGS = -Wall -O2 -m32
LDFLAGS =-m32

all: nld
.c.o:
	$(CC) -c $(CFLAGS) $<
nld: nld.o
	$(CC) $(LDFLAGS) -o $@ $^
clean:
	rm -f nld *.o
