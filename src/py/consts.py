EH3_HDR = '\xffEH3'
EH3_OBJ, \
EH3_EXE, \
= range(2)

REG_ZZ = 0
REG_A0 = 1
REG_A1 = 2
REG_A2 = 3
REG_A3 = 4
REG_S0 = 5
REG_S1 = 6
REG_S2 = 7
REG_S3 = 8
REG_T0 = 9
REG_T1 = 10
REG_T2 = 11
REG_T3 = 12
REG_FP = 13
REG_SP = 14
REG_LR = 15
REG_N  = 16
REG_SZ =  4

REG_NAMES = (
    'zz',
    'a0',
    'a1',
    'a2',
    'a3',
    's0',
    's1',
    's2',
    's3',
    't0',
    't1',
    't2',
    't3',
    'fp',
    'sp',
    'lr',
)

OP_LOAD   =  0
OP_STORE  =  1
OP_PUSH   =  2
OP_POP    =  3
OP_ADD    =  4
OP_SUB    =  5
OP_MUL    =  6
OP_DIVMOD =  7
OP_XOR    =  8
OP_AND    =  9
OP_OR     = 10
OP_SHL    = 11
OP_SHR    = 12
OP_CALL   = 13
OP_JCC    = 14
OP_ALT    = 15
OP_N      = 16
OP_SZ     = 4

OP_NAMES = (
    'load',
    'store',
    'push',
    'pop',
    'add',
    'sub',
    'mul',
    'divmod',
    'xor',
    'and',
    'or',
    'shl',
    'shr',
    'call',
    # jcc,
    # alt
)

# Map pseudo opcodes to real opcodes
OP_PSEUDO = {
    'mov'    : 'add',
    'neg'    : 'sub',
    'not'    : 'xor',
    'div'    : 'divmod',
    'mod'    : 'divmod',
    'idivmod': 'divmod',
    'idiv'   : 'divmod',
    'imod'   : 'divmod',
    # 'imul'   : 'mul',
    'nop'    : 'align',
    'ret'    : 'jmp',
    'jnz'    : 'jne',
    'jb'     : 'jl',
    'jbe'    : 'jle',
    'ja'     : 'jg',
    'jae'    : 'jge',
}

ALT_SYSCALL =  0
ALT_HALT    =  1
ALT_ALIGN   =  2
ALT_BRK     =  3
ALT_N       =  4
ALT_SZ      =  2

ALT_NAMES = (
    'syscall',
    'halt',
    'align',
    'brk',
)

JCC_JMP    = 0
JCC_Z      = 1
JCC_E      = 2
JCC_NE     = 3
JCC_L      = 4
JCC_LE     = 5
JCC_G      = 6
JCC_GE     = 7
JCC_N      = 8
JCC_SZ     = 3

JCC_NAMES = (
    'jmp',
    'jz',
    'je',
    'jne',
    'jl',
    'jle',
    'jg',
    'jge',
)

SZ_B  = 0
SZ_H  = 1
SZ_T  = 2
SZ_W  = 3
SZ_N  = 4
SZ_SZ = 2

SZ_NAMES = (
    'byte',
    'half',
    'trib',
    'word',
)

EXT_ZX = 0
EXT_SX = 1
EXT_N  = 2
EXT_SZ = 1

EXT_NAMES = (
    'zx',
    'sx',
)

SIGN_U  = 0
SIGN_S  = 1
SIGN_N  = 2
SIGN_SZ = 1

SIGN_NAMES = (
    'unsigned',
    'signed',
)

SIZE_B =  8
SIZE_H = 16
SIZE_T = 24
SIZE_W = 32

MASK_B = 0xff
MASK_H = 0xffff
MASK_T = 0xffffff
MASK_W = 0xffffffff

SIGN_B = 0x80
SIGN_H = 0x8000
SIGN_T = 0x800000
SIGN_W = 0x80000000

MEM_SIZE = 2**32
MEM_MASK = MASK_W

REG_MASK = MEM_MASK

PAGE_SIZE = 0x1000
PAGE_MASK = 0xfff

STACK_SIZE = 0x100000
STACK_TOP = MEM_SIZE

ICACHE_SIZE = 64
ICACHE_FMT = '<Q'
ICACHE_MASK = ICACHE_SIZE - 1

LOG_SILENT   = 0
LOG_CRITICAL = 1
LOG_WARNING  = 2
LOG_INFO     = 3
LOG_DEBUG    = 4
LOG_GURU     = 5

DBG_SET_LOG_LEVEL  = 0
DBG_PUSH_LOG_LEVEL = 1
DBG_POP_LOG_LEVEL  = 2
DBG_REGS           = 3
DBG_REG            = 4
DBG_MEM            = 5
DBG_MEM_R          = 6
DBG_VAL            = 7

# Bitmask
PROT_R = 1
PROT_W = 2
PROT_X = 4
def prot2str(p):
    out = ''
    if p & PROT_R:
        out += 'R'
    if p & PROT_W:
        out += 'W'
    if p & PROT_X:
        out += 'X'
    return out
