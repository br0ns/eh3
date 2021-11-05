#!/bin/sh
set -e
set -x
. ./source.this.file.sh

(cd src/c/cc/ ; make OUT=eh3)
# (cd src/c/cc/ ; make clean all OUT=eh3)

(cd src/c/libc/ ; make OUT=eh3)
# (cd src/c/libc/ ; make clean all OUT=eh3)

# (cd src/c/cc.eh3/ ; make OUT=eh3) > /dev/null
# (cd src/c/cc.eh3/ ; make OUT=eh3)
# (cd src/c/cc.eh3/ ; make clean all OUT=eh3)

(cd src/c/ld/ ; make)
# (cd src/c/ld/ ; make clean all)

(cd src/c/emu ; make clean all TRACE=1)

easm ex/syscall.asm

for f in ex/*.c ; do
    # ncc -I/usr/include -I/usr/include/linux -Isrc/c/libc "$f"
    # eemu bin/ncc.eh3 -I/usr/include -I/usr/include/linux -Isrc/c/libc "$f"

    ncc -Isrc/c/libc "$f"
    # eemu -- bin/ncc.eh3 -Isrc/c/libc "$f"
    # eemu bin/ncc.eh3 -Isrc/c/libc.eh3 "$f"

    nld -o "${f%.c}" ex/syscall.o "${f%.c}.o" src/c/libc/start.o src/c/libc/libc.a
done


# PROG=hello
# eemu bin/ncc.eh3 -I src/c/libc ex/$PROG.c
# nld ex/$PROG.o src/c/cc.eh3/mem.o src/c/libc/start.o src/c/libc/libc.a

# eemu a.out $@ && echo 0 || echo $?

# edis ex/test.o
# edis ex/start.o
# edis -p a.out
