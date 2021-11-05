format elf

#include <stdlib.h>

global _start

section .text
    mov s0, -500
alignb
_start:
    mov s0, -500
.loop:
    mov a1, STDOUT_FILENO
    mov a2, hello_s
    mov a3, sizeof(hello_s)
    mov a0, SYS_write
    syscall
    div signed s0, s0, -3
    jnz s0, .loop
    ;; jz s0, .break
    ;; jmp .loop
.break:
    brk
    halt

section .data
hello_s:
    db "Hello, world!\n"
