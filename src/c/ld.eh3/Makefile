CC = ../cc/ncc
LD = ../ld/nld
CFLAGS = -O2 -I../libc
LDFLAGS =

all: nld
.c.o:
	$(CC) -c $(CFLAGS) $<
nld: nld.o
	$(LD) $(LDFLAGS) $^ ../libc/start.o ../libc/libc.a -o $@
clean:
	rm -f nld *.o
