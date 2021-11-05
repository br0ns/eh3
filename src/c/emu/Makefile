CFLAGS = -Wall -Wno-format -no-pie -m32 -O3

OBJS = $(patsubst %.c,%.o,$(wildcard *.c))

ifdef TRACE
CFLAGS += -DTRACE
else
OBJS := $(filter-out trace.o,$(OBJS))
endif

ifdef DEBUG
CFLAGS += -DDEBUG -lpython2.7 -I/usr/include/python2.7
else
OBJS := $(filter-out debug.o,$(OBJS))
endif

HDRS = $(wildcard *.h)

.PHONY: all
all: emu

emu: $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

.PHONY: clean
clean:
	-rm -f *.o emu
