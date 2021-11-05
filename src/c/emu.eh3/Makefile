CC = ../cc/ncc
LD = ../ld/nld
AS = ../../py/asm.py
CFLAGS = -I../libc -I ./include

OBJS = $(patsubst %.c,%.o,$(wildcard *.c))
OBJS += syscall.o
OBJS += emu.o

ifdef TRACE
CFLAGS += -DTRACE
endif

HDRS = $(wildcard *.h)

.PHONY: all
all: emu

emu.c: mkemu.py
	python $<

# Delete built-in rule
%: %.c

emu: $(OBJS) $(HDRS)
	$(LD) $(LDFLAGS) $(OBJS) ../libc/start.o ../libc/libc.a -o $@

.PHONY: clean
clean:
	-rm -f *.o emu emu.c
