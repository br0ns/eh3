CC = ncc
CFLAGS = -I../libc
LD = nld
LDFLAGS =

SRCS = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRCS))
OBJS = $(patsubst %.c,%.o,$(SRCS))

.PHONY: all
all: $(PROGS)

.PHONY: clean
clean:
	rm -f *.o $(PROGS)

# Remove implicit rule
%: %.c

%: %.o
	$(LD) $(LDFLAGS) $< ../libc/start.o ../libc/libc.a -o $@

%.o: %.c
	$(CC) $(CFLAGS) $<
