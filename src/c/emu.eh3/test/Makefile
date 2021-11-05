TESTS = mem trace

CFLAGS=-g -m32

.PHONY: all
all: $(TESTS)

mem: mem.c ../mem.o ../util.o
trace: trace.c ../trace.o

.PHONY: clean
clean:
	-rm -f $(TESTS)
