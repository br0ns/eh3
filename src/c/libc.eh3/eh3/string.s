format elf

;; At least for `ncc` unrolling `memcpy` does not improve speed
;; #define MEMCPY_UNROLL
#define MEMSET_UNROLL

section .text

;; Copy right-to-left
global memcpy
alignb memcpy:
#ifdef MEMCPY_UNROLL
.loop1:
    jb a2, 0x100, .break1
    load word t0, [a1 : 0x00]
    store word [a0 : 0x00], t0
    load word t0, [a1 : 0x04]
    store word [a0 : 0x04], t0
    load word t0, [a1 : 0x08]
    store word [a0 : 0x08], t0
    load word t0, [a1 : 0x0c]
    store word [a0 : 0x0c], t0
    load word t0, [a1 : 0x10]
    store word [a0 : 0x10], t0
    load word t0, [a1 : 0x14]
    store word [a0 : 0x14], t0
    load word t0, [a1 : 0x18]
    store word [a0 : 0x18], t0
    load word t0, [a1 : 0x1c]
    store word [a0 : 0x1c], t0
    load word t0, [a1 : 0x20]
    store word [a0 : 0x20], t0
    load word t0, [a1 : 0x24]
    store word [a0 : 0x24], t0
    load word t0, [a1 : 0x28]
    store word [a0 : 0x28], t0
    load word t0, [a1 : 0x2c]
    store word [a0 : 0x2c], t0
    load word t0, [a1 : 0x30]
    store word [a0 : 0x30], t0
    load word t0, [a1 : 0x34]
    store word [a0 : 0x34], t0
    load word t0, [a1 : 0x38]
    store word [a0 : 0x38], t0
    load word t0, [a1 : 0x3c]
    store word [a0 : 0x3c], t0
    load word t0, [a1 : 0x40]
    store word [a0 : 0x40], t0
    load word t0, [a1 : 0x44]
    store word [a0 : 0x44], t0
    load word t0, [a1 : 0x48]
    store word [a0 : 0x48], t0
    load word t0, [a1 : 0x4c]
    store word [a0 : 0x4c], t0
    load word t0, [a1 : 0x50]
    store word [a0 : 0x50], t0
    load word t0, [a1 : 0x54]
    store word [a0 : 0x54], t0
    load word t0, [a1 : 0x58]
    store word [a0 : 0x58], t0
    load word t0, [a1 : 0x5c]
    store word [a0 : 0x5c], t0
    load word t0, [a1 : 0x60]
    store word [a0 : 0x60], t0
    load word t0, [a1 : 0x64]
    store word [a0 : 0x64], t0
    load word t0, [a1 : 0x68]
    store word [a0 : 0x68], t0
    load word t0, [a1 : 0x6c]
    store word [a0 : 0x6c], t0
    load word t0, [a1 : 0x70]
    store word [a0 : 0x70], t0
    load word t0, [a1 : 0x74]
    store word [a0 : 0x74], t0
    load word t0, [a1 : 0x78]
    store word [a0 : 0x78], t0
    load word t0, [a1 : 0x7c]
    store word [a0 : 0x7c], t0
    load word t0, [a1 : 0x80]
    store word [a0 : 0x80], t0
    load word t0, [a1 : 0x84]
    store word [a0 : 0x84], t0
    load word t0, [a1 : 0x88]
    store word [a0 : 0x88], t0
    load word t0, [a1 : 0x8c]
    store word [a0 : 0x8c], t0
    load word t0, [a1 : 0x90]
    store word [a0 : 0x90], t0
    load word t0, [a1 : 0x94]
    store word [a0 : 0x94], t0
    load word t0, [a1 : 0x98]
    store word [a0 : 0x98], t0
    load word t0, [a1 : 0x9c]
    store word [a0 : 0x9c], t0
    load word t0, [a1 : 0xa0]
    store word [a0 : 0xa0], t0
    load word t0, [a1 : 0xa4]
    store word [a0 : 0xa4], t0
    load word t0, [a1 : 0xa8]
    store word [a0 : 0xa8], t0
    load word t0, [a1 : 0xac]
    store word [a0 : 0xac], t0
    load word t0, [a1 : 0xb0]
    store word [a0 : 0xb0], t0
    load word t0, [a1 : 0xb4]
    store word [a0 : 0xb4], t0
    load word t0, [a1 : 0xb8]
    store word [a0 : 0xb8], t0
    load word t0, [a1 : 0xbc]
    store word [a0 : 0xbc], t0
    load word t0, [a1 : 0xc0]
    store word [a0 : 0xc0], t0
    load word t0, [a1 : 0xc4]
    store word [a0 : 0xc4], t0
    load word t0, [a1 : 0xc8]
    store word [a0 : 0xc8], t0
    load word t0, [a1 : 0xcc]
    store word [a0 : 0xcc], t0
    load word t0, [a1 : 0xd0]
    store word [a0 : 0xd0], t0
    load word t0, [a1 : 0xd4]
    store word [a0 : 0xd4], t0
    load word t0, [a1 : 0xd8]
    store word [a0 : 0xd8], t0
    load word t0, [a1 : 0xdc]
    store word [a0 : 0xdc], t0
    load word t0, [a1 : 0xe0]
    store word [a0 : 0xe0], t0
    load word t0, [a1 : 0xe4]
    store word [a0 : 0xe4], t0
    load word t0, [a1 : 0xe8]
    store word [a0 : 0xe8], t0
    load word t0, [a1 : 0xec]
    store word [a0 : 0xec], t0
    load word t0, [a1 : 0xf0]
    store word [a0 : 0xf0], t0
    load word t0, [a1 : 0xf4]
    store word [a0 : 0xf4], t0
    load word t0, [a1 : 0xf8]
    store word [a0 : 0xf8], t0
    load word t0, [a1 : 0xfc]
    store word [a0 : 0xfc], t0
    add a0, a0, 0x100
    add a1, a1, 0x100
    sub a2, a2, 0x100
    jmp .loop1
.break1:
#endif
.loop2:
    jz a2, .break2
    sub a2, a2, 1
    load byte t0, [a1 : a2]
    store byte [a0 : a2], t0
    jmp .loop2
.break2:
    ret

global memmove
alignb memmove:
    ;; dst begins at a higher address
    ja a0, a1, memcpy
    ;; dst begins at a lower address
    mov t1, 0 ; counter
.loop:
    je t1, a2, .break
    load byte t0, [a1 : t1]
    store byte [a0 : t1], t0
    add t1, t1, 1
    jmp .loop
.break:
    ret

global memset
alignb memset:
#ifdef MEMSET_UNROLL
    ; Copy lowest byte x 4
    and a1, a1, 0xff
    shl t0, a1, 8
    or a1, t0, a1
    shl t0, a1, 16
    or a1, t0, a1
.loop1:
    jb a2, 0x100, .break1
    store word [a0 : 0x00], a1
    store word [a0 : 0x04], a1
    store word [a0 : 0x08], a1
    store word [a0 : 0x0c], a1
    store word [a0 : 0x10], a1
    store word [a0 : 0x14], a1
    store word [a0 : 0x18], a1
    store word [a0 : 0x1c], a1
    store word [a0 : 0x20], a1
    store word [a0 : 0x24], a1
    store word [a0 : 0x28], a1
    store word [a0 : 0x2c], a1
    store word [a0 : 0x30], a1
    store word [a0 : 0x34], a1
    store word [a0 : 0x38], a1
    store word [a0 : 0x3c], a1
    store word [a0 : 0x40], a1
    store word [a0 : 0x44], a1
    store word [a0 : 0x48], a1
    store word [a0 : 0x4c], a1
    store word [a0 : 0x50], a1
    store word [a0 : 0x54], a1
    store word [a0 : 0x58], a1
    store word [a0 : 0x5c], a1
    store word [a0 : 0x60], a1
    store word [a0 : 0x64], a1
    store word [a0 : 0x68], a1
    store word [a0 : 0x6c], a1
    store word [a0 : 0x70], a1
    store word [a0 : 0x74], a1
    store word [a0 : 0x78], a1
    store word [a0 : 0x7c], a1
    store word [a0 : 0x80], a1
    store word [a0 : 0x84], a1
    store word [a0 : 0x88], a1
    store word [a0 : 0x8c], a1
    store word [a0 : 0x90], a1
    store word [a0 : 0x94], a1
    store word [a0 : 0x98], a1
    store word [a0 : 0x9c], a1
    store word [a0 : 0xa0], a1
    store word [a0 : 0xa4], a1
    store word [a0 : 0xa8], a1
    store word [a0 : 0xac], a1
    store word [a0 : 0xb0], a1
    store word [a0 : 0xb4], a1
    store word [a0 : 0xb8], a1
    store word [a0 : 0xbc], a1
    store word [a0 : 0xc0], a1
    store word [a0 : 0xc4], a1
    store word [a0 : 0xc8], a1
    store word [a0 : 0xcc], a1
    store word [a0 : 0xd0], a1
    store word [a0 : 0xd4], a1
    store word [a0 : 0xd8], a1
    store word [a0 : 0xdc], a1
    store word [a0 : 0xe0], a1
    store word [a0 : 0xe4], a1
    store word [a0 : 0xe8], a1
    store word [a0 : 0xec], a1
    store word [a0 : 0xf0], a1
    store word [a0 : 0xf4], a1
    store word [a0 : 0xf8], a1
    store word [a0 : 0xfc], a1
    add a0, a0, 0x100
    sub a2, a2, 0x100
    jmp .loop1
.break1:
#endif
.loop2:
    jz a2, .break2
    sub a2, a2, 1
    store byte [a0 : a2], a1
    jmp .loop2
.break2:
    ret

global memchr
alignb memchr:
    add a2, a0, a2 ; end
.loop:
    je a0, a2, .break
    load byte t0, [a0]
    je t0, a1, .found
    add a0, a0, 1
    jmp .loop
.break:
    mov a0, 0
.found:
    ret

global memcmp
alignb memcmp:
    mov t0, 0
    ;; must return 0 if `n` (a2) is 0
    mov t1, 0
    mov t2, 0
.loop:
    je t0, a2, .break
    load byte t1, [a0 : t0]
    load byte t2, [a1 : t0]
    jne t1, t2, .break
    add t0, t0, 1
    jmp .loop
.break:
    sub a0, t1, t2
    ret

global strlen
alignb strlen:
    mov t0, 0
.loop:
    load byte t1, [a0 : t0]
    jz t1, .break
    add t0, t0, 1
    jmp .loop
.break:
    mov a0, t0
    ret

global memrchr
alignb memrchr:
    add a0, a0, a2 ; end
    sub a0, a0, a2 ; start
.loop:
    je a0, a2, .break
    sub a0, a0, 1
    load byte t0, [a0]
    je t0, a1, .found
    jmp .loop
.break:
    mov a0, 0
.found:
    ret

global strchr
alignb strchr:
.loop:
    load byte t0, [a0]
    je t0, a1, .found
    add a0, a0, 1
    jnz t0, .loop
.break:
    mov a0, 0
.found:
    ret

global strcmp
alignb strcmp:
    mov t0, 0
.loop:
    load byte t1, [a0 : t0]
    load byte t2, [a1 : t0]
    jne t1, t2, .break
    jz t1, .break
    add t0, t0, 1
    jmp .loop
.break:
    sub a0, t1, t2
    ret

global strcpy
alignb strcpy:
    mov t0, 0
.loop:
    load byte t1, [a1 : t0]
    store byte [a0 : t0], t1
    jz t1, .break
    add t0, t0, 1
    jmp .loop
.break:
    ; dst already in a0
    ret

global strrchr
alignb strrchr:
    mov t1, 0 ; last found
    sub a0, a0, 1 ; so we can increment at start of loop
.loop:
    add a0, a0, 1
    load byte t0, [a0]
    jz t0, .break
    jne t0, a1, .loop
    mov t1, a0
    jmp .loop
.break:
    mov a0, t1
    ret

global strncmp
alignb strncmp:
    mov t0, 0
    ;; return 0 if `n` (a2) is 0
    mov t1, 0
    mov t2, 0
.loop:
    je t0, a2, .break
    load byte t1, [a0 : t0]
    load byte t2, [a1 : t0]
    jne t1, t2, .break
    jz t1, .break
    add t0, t0, 1
    jmp .loop
.break:
    sub a0, t1, t2
    ret
