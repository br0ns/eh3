# output architecture: x64, x86, arm, eh3
# OUT = x64
OUT = eh3

CC = cc
# CFLAGS = -Wall -O2 -DNEATCC_`echo $(OUT) | tr xarm XARM`
CFLAGS = -m32 -Wno-format -O2 -DNEATCC_`echo $(OUT) | tr [a-z] [A-Z]` \
	-DDIE_OP_NOT_SUPPORTED
LDFLAGS = -m32

OBJS = ncc.o tok.o out.o cpp.o gen.o int.o reg.o mem.o $(OUT).o

all: ncc
%.o: %.c ncc.h $(OUT).h
	$(CC) -c $(CFLAGS) $<
ncc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
clean:
	rm -f *.o ncc
