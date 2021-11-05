#ifndef __CONSTS_H
#define __CONSTS_H

#include <stdint.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wunused-variable"

static const char EH3_HDR[] = {0xff, 'E', 'H', '3'};
#define EH3_OBJ 0
#define EH3_EXE 1
static const char ELF_HDR[] = {0x7f, 'E', 'L', 'F'};

#define REG_ZZ  0
#define REG_A0  1
#define REG_A1  2
#define REG_A2  3
#define REG_A3  4
#define REG_S0  5
#define REG_S1  6
#define REG_S2  7
#define REG_S3  8
#define REG_T0  9
#define REG_T1 10
#define REG_T2 11
#define REG_T3 12
#define REG_FP 13
#define REG_SP 14
#define REG_LR 15
#define REG_N  16
#define REG_SZ  4

static const char *REG_NAMES[] = {
  "zz", "a0", "a1", "a2", "a3", "s0", "s1", "s2",
  "s3", "t0", "t1", "t2", "t3", "fp", "sp", "lr",
};

#define OP_LOAD   0
#define OP_STORE  1
#define OP_PUSH   2
#define OP_POP    3
#define OP_ADD    4
#define OP_SUB    5
#define OP_MUL    6
#define OP_DIVMOD 7
#define OP_XOR    8
#define OP_AND    9
#define OP_OR    10
#define OP_SHL   11
#define OP_SHR   12
#define OP_CALL  13
#define OP_JCC   14
#define OP_ALT   15
#define OP_N     16
#define OP_SZ     4

static const char *OP_NAMES[] = {
  "load",
  "store",
  "push",
  "pop",
  "add",
  "sub",
  "mul",
  "divmod",
  "xor",
  "and",
  "or",
  "shl",
  "shr",
  "call",
  NULL, /* JCC */
  NULL, /* ALT */
};

#define ALT_SYSCALL 0
#define ALT_HALT    1
#define ALT_ALIGN   2
#define ALT_BRK     3
#define ALT_N       4
#define ALT_SZ      2

static const char *ALT_NAMES[] = {
  "syscall",
  "halt",
  "align",
  "brk",
};

#define JCC_JMP 0
#define JCC_Z   1
#define JCC_E   2
#define JCC_NE  3
#define JCC_L   4
#define JCC_LE  5
#define JCC_G   6
#define JCC_GE  7
#define JCC_N   8
#define JCC_SZ  3

static const char *JCC_NAMES[] = {
  "jmp",
  "jz",
  "je",
  "jne",
  "jl",
  "jle",
  "jg",
  "jge",
};

#define SZ_B  0
#define SZ_H  1
#define SZ_T  2
#define SZ_W  3
#define SZ_N  4
#define SZ_SZ 2

static const char *SZ_NAMES[] = {
  "byte",
  "half",
  "trib",
  "word",
};

#define EXT_ZX 0
#define EXT_SX 1
#define EXT_N  2
#define EXT_SZ 1

static const char *EXT_NAMES[] = {
  "zx",
  "sx",
};

#define SIGN_U 0
#define SIGN_S 1
#define SIGN_N 2
#define SIGN_SZ 1

static const char *SIGN_NAMES[] = {
  "unsigned",
  "signed",
};

#define SIZE_B  8
#define SIZE_H 16
#define SIZE_T 24
#define SIZE_W 32

#define MASK_W 0xffffffff
#define MASK_T   0xffffff
#define MASK_H     0xffff
#define MASK_B       0xff

#define SIGN_W 0x80000000
#define SIGN_T   0x800000
#define SIGN_H     0x8000
#define SIGN_B       0x80

#define MEM_SIZE 0x100000000L
#define MEM_MASK MASK_W

#define REG_MASK MEM_MASK

#define PAGE_SIZE 0x1000
#define PAGE_MASK  0xfff

#define MMAP_MIN_ADDR (PAGE_SIZE * 0x800)

#define STACK_SIZE 0x100000
#define STACK_TOP  MEM_SIZE

#define ICACHE_SIZE 64
#define ICACHE_MASK (ICACHE_SIZE - 1)

#define PROT_R 1
#define PROT_W 2
#define PROT_X 4

#pragma GCC diagnostic pop

#endif /* __CONSTS_H */
