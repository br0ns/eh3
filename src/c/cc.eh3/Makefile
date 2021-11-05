# output architecture: x64, x86, arm, eh3
OUT = eh3

CC = ../cc/ncc
LD = ../ld/nld
LIBC = ../libc
CFLAGS = -O2 -DNEATCC_`echo $(OUT) | tr [a-z] [A-Z]` \
	-I $(LIBC)
LDFLAGS =

OBJS = ncc.o tok.o out.o cpp.o gen.o int.o reg.o mem.o $(OUT).o
LIBS = $(LIBC)/start.o $(LIBC)/libc.a

all: ncc
%.o: %.c ncc.h $(OUT).h
	$(CC) -c $(CFLAGS) $<
ncc: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)
clean:
	rm -f *.o ncc
