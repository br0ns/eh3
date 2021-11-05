format elf
global syscall

section .text
syscall:
    load zx word t0, [sp : 0]
    load zx word t1, [sp : 4]
    load zx word t2, [sp : 8]
    syscall
    ret
