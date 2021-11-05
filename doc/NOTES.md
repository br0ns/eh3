# Elektronhjerne 3 (EH3)

## Registers

`r0` ... `r15`

`r0` := `0`
`LR` := `r14`
`SP` := `r15`

`FLGS`: `ZF`, `OF`, `SF`, `CF`

## Memory

2^32 bytes flat.

Memory units must observe natural alignment.

## Instructions

```
TYPES:
opcode:    4b
reg:       4b
imm(n):    nb # Always sign-extend to 32b
imm(byte): imm(8)
imm(half): imm(16)
imm(trib): imm(24)
imm(word): imm(32)
sz:        {byte, half, trib, word}
oper:      {reg, sz + imm(sz)}
ext:       {zx, sx}
sign:      {unsigned, signed}
jcc:       {-, z, e, ne, l, le, g, ge}
       OR? {z, nz, e, ne, l, le, g, ge}
scale:     imm(5)
addr:      reg + oper
jmp:       oper # reg: abs byte addr, imm: rel bit offset
altop:     {syscall, halt, align, brk}
```

### Encoding

```
type1: imm(1)
type2: imm(2)
type3: {x, imm(3)}
type: {type1, type2, type3}

                  LSB --> MSB
(type)(type1)0 => 00_0
(type)(type2)3 => 10_11
(type)(type3)x => 01_0
(type)(type3)6 => 01_1_011

11_... is an invalid encoding
```

```
INSTRUCTION:                     COMMENT:
LOAD ext sz reg, [addr]
STORE sz [addr], oper
PUSH oper                        SP is incremented after
POP reg
ADD reg, reg, oper
SUB reg, reg, reg
MUL sign reg, reg, oper          ?? Does this need to be signed ??
                                 ?? Have two result registers   ??
DIVMOD sign reg, reg, reg, oper
XOR reg, reg, oper
AND reg, reg, oper
OR reg, reg, oper
SHL reg, reg, oper
SHR sign reg, reg, oper
CALL jmp
JCC - jmp
    z reg, jmp
    e/ne reg, oper, jmp
    cc sign reg, oper, jmp
ALT altop
```

### Semantics

`call` and `alt align` are padded, such that they end on a byte boundary.

### Pseudo

```
MOV reg, oper                 ADD reg, ZZ, oper
NEG reg, reg                  SUB reg, ZZ, reg
NOT reg, reg                  XOR reg, reg, ~0
DIV sign ext reg, reg, oper   DIVMOD sign ext reg, ZZ, reg, oper
MOD sign ext reg, reg, oper   DIVMOD sign ext ZZ, reg, reg, oper
NOP                           ALT align
RET                           JMP lr
JNZ oper, jmp                 JNE ZZ, oper, jmp
```

## Instructions (legacy)

```
opcode:    4b
reg:       4b
imm(n):    nb # Always sign-extend to 32b
imm(byte): imm(8)
imm(half): imm(16)
imm(word): imm(32)
sz:        {byte, half, -, word}
oper:      {reg, sz + imm(sz)}
ext:       {zx, sx}
cc:        {always, e, b, be, a, ae, l, le, g, ge, o, s, ne, no, ns, -}
scale:     imm(5)
addr:      oper + {reg << scale, -}
jmp:       oper
```

Conditional operations:

```
INSTRUCTION:                  COMMENT:
LOAD.cc ext sz reg, [addr]
STORE.cc sz [addr], oper
PUSH.cc oper                  # SP is incremented after
POP.cc reg
ADD.cc reg, reg, oper         # SF, ZF, OF, CF
SUB.cc reg, reg, oper         # SF, ZF, OF, CF
MUL.cc reg, reg, oper         # OF
DIV.cc reg, reg, oper         # ZF
MOD.cc reg, reg, oper         # ZF
XOR.cc reg, reg, oper         # SF, ZF
AND.cc reg, reg, oper         # SF, ZF
OR.cc  reg, reg, oper         # SF, ZF
SHL.cc reg, reg, oper         # SF, ZF, OF
SHR.cc reg, reg, oper         # SF, ZF, OF
JMP.cc jmp                    # reg: abs byte addr, imm: rel bit offset
CALL.cc jmp                   # always align, then push PC
```

Flags only affected if `cc` is `always`.

A second set of (unconditional) operations is given by the `alt` prefix:

```
SYSCALL
HALT
ALIGN
BRK
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
RESERVED
NOP
```

### Pseudo

```
NEG reg, reg             SUB reg, zz, reg
NOT reg, reg             XOR reg, reg, ~0
MOV reg, oper            ADD reg, zz, oper
TST reg, oper            AND zz, reg, oper
CMP reg, oper            SUB zz, reg, oper
RET                      JMP lr
```
