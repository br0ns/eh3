format elf

#include <stdlib.h>
extern environ
extern main
extern __neatlibc_exit
global _start

section .text
;; Stack layout:
;; sp + 0x00:          : argc
;;    + 0x04           : argv
;;    + 0x04 + argc * 4: NULL
;;    + 0x08 + argc * 4: envp
;; char *env
_start:
    mov fp, 0
    load zx word a0, [sp] ; argc
    add a1, sp, 4         ; argv
    mul a2, a0, 4
    add a2, a2, a1
    add a2, a2, 4         ; envp

    store word [environ], a2
    call main
    mov a1, a0
    call __neatlibc_exit
    mov a0, SYS_exit
    syscall
    halt
