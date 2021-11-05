/* #define TRACE */

/* architecture-dependent header for x86 */
#define LONGSZ      4   /* word size */
#define I_ARCH      "__eh3__"
#define EM_EH3 12
#define R_EH3_ABS 1
#define R_EH3_REL 2

/* Registers */
#define R_ZZ  0
#define R_A0  1
#define R_RET R_A0
#define R_A1  2
#define R_A2  3
#define R_A3  4
#define R_S0  5
#define R_S1  6
#define R_S2  7
#define R_S3  8
#define R_T0  9
#define R_T1 10
#define R_T2 11
#define R_T3 12
#define R_FP 13
#define R_SP 14
#define R_LR 15
#define R_N  16
#define R_SZ  4

/* Opcodes. */
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

/* Alt ops */
#define ALT_SYSCALL 0
#define ALT_HALT    1
#define ALT_ALIGN   2
#define ALT_BRK     3
#define ALT_N       4
#define ALT_SZ      2

/* Jumps. */
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

/* Size, sign extension, and sign. */
#define SZ_B  0
#define SZ_H  1
#define SZ_T  2
#define SZ_W  3
#define SZ_N  4
#define SZ_SZ 2

#define EXT_ZX 0
#define EXT_SX 1
#define EXT_N  2
#define EXT_SZ 1

#define SIGN_U 0
#define SIGN_S 1
#define SIGN_N 2
#define SIGN_SZ 1

/* Util consts */
#define SIZE_B  8
#define SIZE_H 16
#define SIZE_T 24
#define SIZE_W 32

#define OPER_B_SZ (1 + SZ_SZ + SIZE_B)
#define OPER_H_SZ (1 + SZ_SZ + SIZE_H)
#define OPER_T_SZ (1 + SZ_SZ + SIZE_T)
#define OPER_W_SZ (1 + SZ_SZ + SIZE_W)
#define OPER_R_SZ (1 + R_SZ)

#define MASK_W 0xffffffff
#define MASK_T   0xffffff
#define MASK_H     0xffff
#define MASK_B       0xff

#define SIGN_W 0x80000000
#define SIGN_T   0x800000
#define SIGN_H     0x8000
#define SIGN_B       0x80

/* For register allocator. */
#define N_REGS      16   /* number of registers */
#define N_TMPS      12   /* number of tmp registers */
#define N_ARGS      4    /* number of arg registers */

/* Empty register mask. */
#define R_NONE 0

/* mask of tmp registers */
#define R_TMPS (0xffff & ~                      \
  (1 << R_ZZ |                                  \
   1 << R_FP |                                  \
   1 << R_LR |                                  \
   1 << R_SP ))

/* mask of arg registers */
#define R_ARGS                                  \
  (1 << R_A0 |                                  \
   1 << R_A1 |                                  \
   1 << R_A2 |                                  \
   1 << R_A3 )

/* mask of callee-saved registers */
#define R_PERM (0xffff & ~R_ARGS & ~              \
  (1 << R_T0 |                                    \
   1 << R_T1 |                                    \
   1 << R_T2 |                                    \
   1 << R_T3 ))

#define REG_FP      R_FP  /* frame pointer register */
#define REG_SP      R_SP  /* stack pointer register */

/* Saved in prologue: FP, LR */
#define I_ARG0      (-2 * LONGSZ) /* offset of the first argument from FP */
#define I_LOC0      0   /* offset of the first local from FP */
