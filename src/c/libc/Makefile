# output architecture: x64, x86, arm, eh3
# OUT = x64
OUT = eh3

# default assemblers
ASx64 = fasm
ASx86 = fasm
ASarm = neatas
ASeh3 = easm

CC = ncc
AS = $(AS$(OUT))
CFLAGS = -O2 -I.

all: start.o libc.a

%.o: %.s
	$(AS) $^ >/dev/null
%.o: %.c
	$(CC) $(CFLAGS) $^

OBJS1 = $(patsubst %.c,%.o,$(wildcard *.c))
OBJS2 = $(patsubst %.s,%.o,$(wildcard $(OUT)/*.s))

start.o: $(OUT)/start.o
	cp $(OUT)/start.o .
libc.a: $(OBJS1) $(OBJS2)
	$(AR) rcs $@ $(OBJS1) $(OBJS2)

clean:
	rm -f *.o *.a x86/*.o arm/*.o x64/*.o eh3/*.o
