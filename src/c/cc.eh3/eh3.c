/* architecture-dependent code generation for x86 */
#include <stdlib.h>
#include <stdio.h>
#include "ncc.h"

int tmpregs[] = {
  R_T0, R_T1, R_T2, R_T3,
  R_S0, R_S1, R_S2, R_S3,
  R_A3, R_A2, R_A1, R_A0,
};
int argregs[] = {
  R_A0, R_A1, R_A2, R_A3,
};

static int save_lr;

/* generated code, one code bit per buffer byte. */
static struct mem cs;

static long o_pos(void) {
  return mem_len(&cs);
}

static void o_i(long i, int n) {
  char bit;
  while (n--) {
    bit = i & 1;
    i >>= 1;
    mem_put(&cs, &bit, 1);
  }
}

static void o_iat(long pos, long i, int n) {
  char bit;
  while (n--) {
    bit = i & 1;
    i >>= 1;
    mem_cpy(&cs, pos++, &bit, 1);
  }
}

static long o_align(void) {
  long pad = -o_pos() % 8ul;
  o_i(0, pad);
  return pad;
}

static void o_bit(int bit) {
  o_i(bit, 1);
}

static void o_op(int op) {
  o_i(op, OP_SZ);
}

static void o_alt(int alt) {
  o_i(alt, ALT_SZ);
}

static void o_jcc(int jcc) {
  o_i(jcc, JCC_SZ);
}

static void o_sz(int sz) {
  o_i(sz / 8 - 1, SZ_SZ);
}

static void o_ext(int ext) {
  o_i(ext, EXT_SZ);
}

static void o_sign(int sign) {
  o_i(sign, SIGN_SZ);
}

static void o_reg(int r) {
  o_i(r, R_SZ);
}

/* XXX: All kinds of Bad Things happen if `sizeof(long) != LONGSZ`.  Hence
 *      `-m32` in the Makefile. */
static int imm_size(long i) {
  int sz, oldi=i;
  for (sz = SIZE_B; sz <= SIZE_W; sz += SIZE_B, i >>= SIZE_B) {
    /* Get around buggy integer promotion in `ncc`. */
    if (i >= (int)-128 && i <= 127) {
      return sz;
    }
  }
  die("Integer does not fit in 32b: %ld\n", oldi);
}

static void o_imm(long i, int sz) {
  if (!sz) {
    sz = imm_size(i);
  }
  o_sz(sz);
  o_i(i, sz);
}

/* Const to make the code self documenting. */
#define O_REG 0

static void o_oper(long o, int rs) {
  if (rs & O_NUM) {
    o_bit(1);
    o_imm(o, rs & ~O_NUM);
  } else {
    o_bit(0);
    o_reg(o);
  }
}
#define o_jmp o_oper

static void o_addr(int baser, int indexri, int rs) {
  o_reg(baser);
  o_oper(indexri, rs);
}

static long *rel_sym;       /* relocation symbols */
static long *rel_flg;       /* relocation flags */
static long *rel_off;       /* relocation offsets */
static long rel_n, rel_sz;  /* relocation count */

#define LAB_RET 0           /* return label */
static long lab_sz;         /* label count */
static long *lab_loc;       /* label offsets in cs */
static long jmp_n, jmp_sz;  /* jump count */
static long *jmp_off;       /* jump offsets */
static long *jmp_dst;       /* jump destinations */
static long jmp_ret;        /* the position of the last return jmp */

static long *pad_off;     /* offsets to insert byte boundary padding at */
static long pad_n, pad_sz;

static void lab_add(long id) {
  while (id >= lab_sz) {
    int lab_n = lab_sz;
    lab_sz = MAX(128, lab_sz * 2);
    lab_loc = mextend(lab_loc, lab_n, lab_sz, sizeof(*lab_loc));
  }
  lab_loc[id] = o_pos();
}

void i_label(long id) {
  lab_add(id + 1);
}

static void pad_add() {
  if (pad_n == pad_sz) {
    pad_sz = MAX(128, pad_sz * 2);
    pad_off = mextend(pad_off, pad_n, pad_sz, sizeof(*pad_off));
  }
  pad_off[pad_n] = o_pos();
  pad_n++;
}

static void jmp_add(long long off, long dst) {
  if (jmp_n == jmp_sz) {
    jmp_sz = MAX(128, jmp_sz * 2);
    jmp_off = mextend(jmp_off, jmp_n, jmp_sz, sizeof(*jmp_off));
    jmp_dst = mextend(jmp_dst, jmp_n, jmp_sz, sizeof(*jmp_dst));
  }
  jmp_off[jmp_n] = off;
  jmp_dst[jmp_n] = dst;
  jmp_n++;
}

static void i_rel(long sym, long flg, long off) {
  if (rel_n == rel_sz) {
    rel_sz = MAX(128, rel_sz * 2);
    rel_sym = mextend(rel_sym, rel_n, rel_sz, sizeof(*rel_sym));
    rel_flg = mextend(rel_flg, rel_n, rel_sz, sizeof(*rel_flg));
    rel_off = mextend(rel_off, rel_n, rel_sz, sizeof(*rel_off));
  }
  rel_sym[rel_n] = sym;
  rel_flg[rel_n] = flg;
  rel_off[rel_n] = off;
  rel_n++;
}

static void i_mov(int rd, int ri, int rs) {
  /* Moving a register to itself. */
  if (rd == ri && !(rs & O_NUM)) {
    return;
  }

  /* Setting a register to zero.  NB: must respect imm size, as it may be used
   * in a relocation. */
  else if ((O_NUM|0) == rs && 0 == rs) {
    o_op(OP_SUB);
    o_reg(rd);
    o_reg(R_ZZ);
    o_reg(R_ZZ);
  }

  /* Default case */
  else {
    o_op(OP_ADD);
    o_reg(rd);
    o_reg(R_ZZ);
    o_oper(ri, rs);
  }
}

static void i_sub(int rd, int r1, int ri, int rs) {
  if (rs & O_NUM) {
    o_op(OP_ADD);
    o_reg(rd);
    o_reg(r1);
    o_oper(-ri, rs);
  } else {
    o_op(OP_SUB);
    o_reg(rd);
    o_reg(r1);
    o_reg(ri);
  }
}

static void i_push(int ri, int rs) {
  o_op(OP_PUSH);
  o_oper(ri, rs);
}

static void i_pop(int rd) {
  o_op(OP_POP);
  o_reg(rd);
}

static void i_ext(int rd, int ri, int sz, int sign) {
  if (LONGSZ == sz) {
    return;
  }
  int s = (LONGSZ - sz) * 8;
  o_op(OP_SHL);
  o_reg(rd);
  o_reg(ri);
  o_oper(s, O_NUM);

  o_op(OP_SHR);
  o_sign(sign);
  o_reg(rd);
  o_reg(rd);
  o_oper(s, O_NUM);
}

static void i_sym(int rd, int sym, int off) {
  i_mov(rd, off, O_NUM | 32);
  /* Relocations have bit offsets. */
  i_rel(sym, OUT_CS, o_pos() - 32);
}

static void i_ld(int bt, int rd, int r1, int ri, int rs) {
  printf("ld off = %d\n", ri);
  o_op(OP_LOAD);
  o_ext(T_MSIGN & bt ? EXT_SX : EXT_ZX);
  o_sz(T_SZ(bt) * 8);
  o_reg(rd);
  o_addr(r1, ri, rs);
}

static void i_st(int bt, int r1, int r2, int ri, int rs) {
  printf("st off = %d\n", ri);
  o_op(OP_STORE);
  o_sz(T_SZ(bt) * 8);
  o_addr(r2, ri, rs);
  o_oper(r1, O_REG);
}

/* lt, ge, eq, ne, le, gt */
int jccmap[] = {JCC_L, JCC_GE, JCC_E, JCC_NE, JCC_LE, JCC_G};
static long i_jmp(long op, int r1, int ri, long dst) {
  o_op(OP_JCC);
  if (op & O_JZ) {
    /* O_JNZ */
    if (op & 1) {
      o_jcc(JCC_NE);
      o_reg(r1);
      o_oper(R_ZZ, O_REG);
    }
    /* O_JZ */
    else {
      o_jcc(JCC_Z);
      o_reg(r1);
    }
  }

  else if (op & O_JCC) {
    int jcc = jccmap[op & 0xf];
    o_jcc(jcc);
    if (jcc != JCC_E && jcc != JCC_NE) {
      o_sign(O_T(op) & T_MSIGN ? SIGN_S : SIGN_U);
    }
    o_reg(r1);
    o_oper(ri, op & O_NUM);
  }

  else if (op == O_JMP) {
    o_jcc(JCC_JMP);
  }

  else {
    die("i_jmp: Not a jump operation: %lx\n", op);
  }

  /* Offset written (and possibly resized) later. */
  o_jmp(0, O_NUM | SIZE_W);
  jmp_add(o_pos() - SIZE_W, dst);
}

static void set_tail(int rd);

static void i_cmp(int op, int rd, int r1, int ri) {
  /*   jcc r1, ri, set1
       mov rd, 0 ; sub rd, zz, reg zz
       jmp done
     set1:
       mov rd, 1 ; add rd, zz, imm byte 1
     done:
  */
  int jcc = jccmap[op & 0xf];

  /* jcc r1, ri ... */
  o_op(OP_JCC);
  o_jcc(jcc);
  if (jcc != JCC_E && jcc != JCC_NE) {
    o_sign(O_T(op) & T_MSIGN ? SIGN_S : SIGN_U);
  }
  o_reg(r1);
  o_oper(ri, op & O_NUM);

  set_tail(rd);
}

static void i_lnot(int op, int rd, int r1) {
  /*   jz r1, set1
       mov rd, 0 ; sub rd, zz, reg zz
       jmp done
     set1:
       mov rd, 1 ; add rd, zz, imm byte 1
     done:
  */

  /* jz r1 ... */
  o_op(OP_JCC);
  o_jcc(JCC_Z);
  o_reg(r1);

  set_tail(rd);
}

static void set_tail(int rd) {
  int mov0sz = OP_SZ + 3 * R_SZ;
  int jmpsz = OP_SZ + JCC_SZ + OPER_B_SZ;
  int mov1sz = OP_SZ + 2 * R_SZ + OPER_B_SZ;

  /* ..., set1 */
  o_jmp(mov0sz + jmpsz, SIZE_B | O_NUM);

  /* mov rd, 0 */
  o_op(OP_SUB);
  o_reg(rd);
  o_reg(R_ZZ);
  o_reg(R_ZZ);

  /* jmp done */
  o_op(OP_JCC);
  o_jcc(JCC_JMP);
  o_jmp(mov1sz, SIZE_B | O_NUM);

  /* mov rd, 1 */
  o_op(OP_ADD);
  o_reg(rd);
  o_reg(R_ZZ);
  o_oper(1, SIZE_B | O_NUM);
}

static void i_mem(int op) {
  /* mset(t1, t2, t3) */
  /* loop: */
  /*   jz t3, break */
  /*   sub t3, t3, 1 */
  /*   store byte [t1 + t3], t2 */
  /*   jmp loop */
  /* break: */

  /* mcpy(t1, t2, t3) */
  /* loop: */
  /*   jz t3, break */
  /*   sub t3, t3, 1 */
  /*   load unsigned byte t0, [t2 + t3] */
  /*   store byte [t1 + t3], t0 */
  /*   jmp loop */
  /* break: */

  int jzsz = OP_SZ + JCC_SZ + R_SZ + OPER_B_SZ;
  int subsz = OP_SZ + 2 * R_SZ + OPER_B_SZ;
  int addrsz = R_SZ + OPER_R_SZ;
  int ldsz = OP_SZ + EXT_SZ + SZ_SZ + R_SZ + addrsz;
  int stsz = OP_SZ + SZ_SZ + addrsz + OPER_R_SZ;
  int jmpsz = OP_SZ + JCC_SZ + OPER_B_SZ;
  int iscpy = op & 1;

  int fw = subsz + stsz + jmpsz;
  if (iscpy) {
    fw += ldsz;
  }
  int bk = -(fw + jzsz);

  /* The register holding the byte to be written. */
  int rb = iscpy ? R_T0 : R_T2;

  /* jz t3, break */
  o_op(OP_JCC);
  o_jcc(JCC_Z);
  o_reg(R_T3);
  o_jmp(fw, SIZE_B | O_NUM);

  /* sub t3, t3, 1 */
  o_op(OP_ADD);
  o_reg(R_T3);
  o_reg(R_T3);
  o_oper(-1, SIZE_B | O_NUM);

  if (iscpy) {
    i_ld(UCHR, rb, R_T2, R_T3, O_REG);
  }

  i_st(UCHR, rb, R_T1, R_T3, O_REG);

  /* jmp loop */
  o_op(OP_JCC);
  o_jcc(JCC_JMP);
  o_jmp(bk, SIZE_B | O_NUM);
}

static int bopmap(long op) {
  static int add[] = {OP_ADD, OP_SUB, OP_AND, OP_OR, OP_XOR};
  static int shl[] = {OP_SHL, OP_SHR};
  static int mul[] = {OP_MUL, OP_DIVMOD, OP_DIVMOD};
  if (op & O_ADD) {
    return add[op & 0xf];
  }
  else if (op & O_SHL) {
    return shl[op & 0xf];
  }
  else if (op & O_MUL) {
    return mul[op & 0xf];
  } else {
    die("bopmap: Not a binary operation\n");
  }
}

/* XXX: All kinds of Bad Things happen if `sizeof(long) != LONGSZ`.  Hence
 *      `-m32` in the Makefile. */
int i_imm(long lim, long n) {
  long max = (1 << (lim - 1)) - 1;
  long min = -(max + 1);
  return n <= max && n >= min;
}

long i_reg(long op, long *rd, long *r1, long *r2, long *r3, long *tmp) {
  long oc = O_C(op);
  long bt = O_T(op);
  *rd = 0;
  *r1 = 0;
  *r2 = 0;
  *r3 = 0;
  *tmp = 0;

  /* It seems that for `O_NUM` the returned value is the maximal size of an
   * immediate in the given intruction.  But I can't figure out why also 32 is
   * returned for `O_MOV | O_SYM`.  As far as I can tell the value is never used
   * in that case.  Also, I think I found a bug in `io_imm`: for `O_JCC` the 1st
   * operand is used in `imm_ok` but I think it should have been the second. */

  if (oc & O_MOV) {
    *rd = R_TMPS;
    /* if (oc & (O_NUM | O_SYM)) { */
    if (oc & O_NUM) {
      *r1 = SIZE_W;
    } else {
      *r1 = R_TMPS;
    }
    return 0;
  }

  if (oc & O_BOP) {
    *rd = R_TMPS;
    *r1 = R_TMPS;
    *r2 = op & O_NUM ? SIZE_W : R_TMPS;
    return 0;
  }

  if (oc & O_UOP) {
    *rd = R_TMPS;
    *r1 = op & O_NUM ? SIZE_W : R_TMPS;
    return 0;
  }

  if (oc & O_MEM) {
    *r1 = 1 << R_T1;
    *r2 = 1 << R_T2;
    *r3 = 1 << R_T3;
    *tmp = *r1 | *r3;
    if (oc == O_MCPY) {
      *tmp |= *r2;
      /* Load byte into t0 when copying. */
      *tmp |= 1 << R_T0;
    }
    return 0;
  }

  if (oc == O_RET) {
    *r1 = 1 << R_RET;
    return 0;
  }

  if (oc & O_CALL) {
    *rd = 1 << R_RET;
    *r1 = oc & O_SYM ? R_NONE : R_TMPS;
    *tmp = R_TMPS & ~R_PERM;
    return 0;
  }

  if (oc & O_LD) {
    *rd = R_TMPS;
    *r1 = R_TMPS;
    *r2 = oc & O_NUM ? SIZE_W : R_TMPS;
    return 0;
  }

  if (oc & O_ST) {
    *r1 = R_TMPS;
    *r2 = R_TMPS;
    *r3 = oc & O_NUM ? SIZE_W : R_TMPS;
    return 0;
  }

  if (oc & O_JZ) {
    *r1 = R_TMPS;
    return 0;
  }

  if (oc & O_JCC) {
    *r1 = R_TMPS;
    *r2 = oc & O_NUM ? SIZE_W : R_TMPS;
    return 0;
  }

  if (oc == O_JMP) {
    return 0;
  }

  return 1;
}

/* Debugging */
#define ARG1 (1 << 0)
#define ARG2 (1 << 1)
#define ARG3 (1 << 2)
static struct {
  long inst;
  char *name;
  long args;
} opnames[] = {
  {O_JNZ  , "JNZ" , ARG3},
  {O_JZ   , "JZ"  , ARG3},
  {O_MCPY , "MCPY", ARG1|ARG2|ARG3},
  {O_MSET , "MSET", ARG1|ARG2|ARG3},
  {O_LNOT , "LNOT", ARG1},
  {O_NOT  , "NOT" , ARG1},
  {O_NEG  , "NEG" , ARG1},
  {O_GT   , "GT"  , ARG1|ARG2},
  {O_LE   , "LE"  , ARG1|ARG2},
  {O_NE   , "NE"  , ARG1|ARG2},
  {O_EQ   , "EQ"  , ARG1|ARG2},
  {O_GE   , "GE"  , ARG1|ARG2},
  {O_LT   , "LT"  , ARG1|ARG2},
  {O_MOD  , "MOD" , ARG1|ARG2},
  {O_DIV  , "DIV" , ARG1|ARG2},
  {O_MUL  , "MUL" , ARG1|ARG2},
  {O_SHR  , "SHR" , ARG1|ARG2},
  {O_SHL  , "SHL" , ARG1|ARG2},
  {O_XOR  , "XOR" , ARG1|ARG2},
  {O_OR   , "OR"  , ARG1|ARG2},
  {O_AND  , "AND" , ARG1|ARG2},
  {O_SUB  , "SUB" , ARG1|ARG2},
  {O_ADD  , "ADD" , ARG1|ARG2},
  {O_ST   , "ST"  , ARG1|ARG2|ARG3},
  {O_LD   , "LD"  , ARG1|ARG2},
  {O_RET  , "RET" , ARG1},
  {O_JCC|5, "JGT"  , ARG1|ARG2|ARG3},
  {O_JCC|4, "JLE"  , ARG1|ARG2|ARG3},
  {O_JCC|3, "JNE"  , ARG1|ARG2|ARG3},
  {O_JCC|2, "JEQ"  , ARG1|ARG2|ARG3},
  {O_JCC|1, "JGE"  , ARG1|ARG2|ARG3},
  {O_JCC|0, "JLT"  , ARG1|ARG2|ARG3},
  /* {O_JCC , "JCC" , ARG1|ARG2|ARG3}, */
  {O_JMP  , "JMP" , ARG3},
  {O_MOV  , "MOV" , ARG1|ARG2},
  {O_CALL , "CALL", ARG1|ARG2|ARG3},
  {0, NULL, 0},
};

long i_ins(long op, long rd, long r1, long r2, long r3) {
  long oc = O_C(op);
  long bt = O_T(op);

  int i;
  if (oc & O_OUT) {
    TRACE("%3d := ", rd);
  } else {
    TRACE("       ");
  }
  for (i = 0; opnames[i].inst; i++) {
    if ((opnames[i].inst & oc) == opnames[i].inst) {
      TRACE("%s", opnames[i].name);
      break;
    }
  }
  if (!opnames[i].inst) {
    TRACE("<unknown>\n");
  } else {
    if (op & O_NUM) {
      TRACE(".NUM");
    }
    else if (op & O_LOC) {
      TRACE(".LOC");
    }
    else if (op & O_SYM) {
      TRACE(".SYM");
    }
    else {
      TRACE(".REG");
    }
    TRACE(" %d", bt & T_MSIZE);
    TRACE(bt & T_MSIGN ? "s" : "u");
    if (opnames[i].args & ARG1) {
      TRACE(" %#x", r1);
    }
    if (opnames[i].args & ARG2) {
      TRACE(" %#x", r2);
    }
    if (opnames[i].args & ARG3) {
      TRACE(" %#x", r3);
    }
    TRACE("\n");
  }

  if (!(oc & (O_MOV | O_ST | O_LD | O_SHL)) && T_SZ(bt) && T_SZ(bt) < LONGSZ) {
    die("Operation on non-word\n");
  }

  if (oc == (O_SUB | O_NUM)) {
    o_op(OP_ADD);
    o_reg(rd);
    o_reg(r1);
    o_oper(-r2, O_NUM);
    return 0;
  }

  if (oc == O_SUB) {
    o_op(OP_SUB);
    o_reg(rd);
    o_reg(r1);
    o_reg(r2);
    return 0;
  }

  if (oc & (O_ADD | O_SHL)) {
    o_op(bopmap(oc));
    /* O_SHR */
    if (oc & O_SHL && oc & 1) {
      o_sign(bt & T_MSIGN ? SIGN_S : SIGN_U);
    }
    o_reg(rd);
    o_reg(r1);
    o_oper(r2, oc & O_NUM);
    return 0;
  }

  if (oc & O_MUL) {
    /* O_DIV */
    if (oc & 1) {
      o_op(OP_DIVMOD);
      o_sign(bt & T_MSIGN ? SIGN_S : SIGN_U);
      o_reg(rd);
      o_reg(R_ZZ);
      o_reg(r1);
      o_oper(r2, op & O_NUM);
      return 0;
    }

    /* O_MOD */
    else if (oc & 2) {
      o_op(OP_DIVMOD);
      o_sign(bt & T_MSIGN ? SIGN_S : SIGN_U);
      o_reg(R_ZZ);
      o_reg(rd);
      o_reg(r1);
      o_oper(r2, op & O_NUM);
      return 0;
    }

    /* O_MUL */
    else {
      o_op(OP_MUL);
      /* o_sign(bt & T_MSIGN ? SIGN_S : SIGN_U); */
      o_reg(rd);
      o_reg(r1);
      o_oper(r2, op & O_NUM);
      return 0;
    }
  }

  if (oc & O_CMP) {
    i_cmp(op, rd, r1, r2);
    return 0;
  }

  if (oc == O_NEG) {
    o_op(OP_SUB);
    o_reg(rd);
    o_reg(R_ZZ);
    o_reg(r1);
    return 0;
  }

  if (oc == O_NOT) {
    o_op(OP_XOR);
    o_reg(rd);
    o_reg(r1);
    o_oper(~0, O_NUM);
    return 0;
  }

  if (oc == O_LNOT) {
    i_lnot(op, rd, r1);
    return 0;
  }

  if (oc == O_CALL) {
    o_op(OP_CALL);
    o_jmp(r1, O_REG);
    pad_add();
    return 0;
  }

  if (oc == (O_CALL | O_SYM)) {
    o_op(OP_CALL);
    o_jmp((unsigned int)-SIZE_W, O_NUM | SIZE_W);
    i_rel(r1, OUT_CS | OUT_RLREL, o_pos() - SIZE_W);
    pad_add();
    return 0;
  }

  if (oc == (O_MOV | O_SYM)) {
    i_sym(rd, r1, r2);
    return 0;
  }

  if (oc == (O_MOV | O_NUM)) {
    i_mov(rd, r1, oc & O_NUM);
    return 0;
  }

  if (oc == O_MOV) {
    if (T_SZ(bt) == LONGSZ) {
      i_mov(rd, r1, O_REG);
    } else {
      i_ext(rd, r1, T_SZ(bt), bt & T_MSIGN ? SIGN_S : SIGN_U);
    }
    return 0;
  }

  if (oc & O_MEM) {
    i_mem(op);
    return 0;
  }

  if (oc == O_RET) {
    jmp_ret = o_pos();
    i_jmp(O_JMP, 0, 0, LAB_RET);
    return 0;
  }

  if (oc == (O_LD | O_NUM)) {
    i_ld(bt, rd, r1, r2, O_NUM);
    return 0;
  }

  if (oc == (O_ST | O_NUM)) {
    i_st(bt, r1, r2, r3, O_NUM);
    return 0;
  }

  if (oc & O_JXX) {
    i_jmp(op, r1, r2, r3 + 1);
    return 0;
  }

  return 1;
}

static void i_subsp(long val) {
  if (!val)
    return;
  o_op(OP_ADD);
  o_reg(R_SP);
  o_reg(R_SP);
  o_oper(-val, O_NUM);
}

static int regs_count(long regs)
{
  int cnt = 0;
  int i;
  for (i = 0; i < N_REGS; i++)
    if (((1 << i) & R_TMPS) & regs)
      cnt++;
  return cnt;
}

static void regs_save(long sregs)
{
  int i;
  for (i = N_REGS - 1; i >= 0; --i) {
    if (((1 << i) & R_TMPS) & sregs) {
      i_push(i, O_REG);
    }
  }
}

static void regs_load(long sregs)
{
  int i;
  for (i = 0; i < N_REGS; i++) {
    if (((1 << i) & R_TMPS) & sregs) {
      i_pop(i);
    }
  }
}

void i_wrap(int argc, long sargs, long spsub, int initfp, long sregs,
            long sregs_pos) {
  TRACE("i_wrap: argc = %d, sargs = %d, spsub = %d, initfp = %d, sregs = %#x, "
        "sregs_pos = %#x\n",
         argc, sargs, spsub, initfp, sregs, sregs_pos);

  long body_n;
  void *body;

  /* prologue length */
  long diff;
  int i, szarg, szloc;

  /* removing the last jmp to the epilogue */
  if (jmp_ret + OP_SZ + JCC_SZ + 1 + SZ_SZ + SIZE_W == o_pos()) {
    mem_cut(&cs, jmp_ret);
    jmp_n--;
  }

  lab_add(LAB_RET);

  /* Save function body. */
  body_n = mem_len(&cs);
  body = mem_get(&cs);

  /* Create stack frame? */
  /* Stack layout: */
  /*   fp + 8 : args */
  /*   fp + 4 : lr */
  /*   fp     : fp */
  /*   fp - 4 : locals */
  /*          : callee saved regs */
  /*          : func call args */
  /*   sp     : (sp = fp - spsub) */

  /* generating function prologue */
  if (initfp) {
    regs_save(sargs);
    i_push(R_LR, O_REG);
    i_push(R_FP, O_REG);
    /* mov fp, sp */
    i_mov(R_FP, R_SP, O_REG);
  }

  /* Compute stack layout. */
  szloc = spsub;
  szarg = 0;
  if (spsub && sregs) {
    szloc = -sregs_pos - regs_count(sregs) * ULNG;
    szarg = spsub + sregs_pos;
  }
  i_subsp(szloc);
  regs_save(sregs);
  i_subsp(szarg);

  /* /\* Must align before body for its alignments to work right. *\/ */
  /* o_op(OP_ALT); */
  /* o_alt(ALT_ALIGN); */
  /* o_align(); */

  diff = o_pos();

  mem_put(&cs, body, body_n);
  free(body);

  /* generating function epilogue */

  /* Restoring calle saved registers. */
  i_subsp(-szarg);
  regs_load(sregs);

  if (initfp) {
    /* mov sp, fp */
    i_mov(R_SP, R_FP, O_REG);
    i_pop(R_FP);
    i_pop(R_LR);
    i_subsp(regs_count(sargs) * -ULNG);
  }

  /* ret == jmp lr */
  o_op(OP_JCC);
  o_jcc(JCC_JMP);
  o_jmp(R_LR, O_REG);

  /* adjusting code offsets */
  for (i = 0; i < rel_n; i++)
    rel_off[i] += diff;
  for (i = 0; i < jmp_n; i++)
    jmp_off[i] += diff;
  for (i = 0; i < lab_sz; i++)
    lab_loc[i] += diff;
  for (i = 0; i < pad_sz; i++)
    pad_off[i] += diff;
}

/* insert alignment padding after calls */
static void i_align() {
  long prev = 0;
  long this;
  long diff = 0; /* code length difference after inserting padding */
  long pad;
  int rel = 0; /* current relocation */
  int jmp = 0; /* current jump */
  int lab = LAB_RET + 1; /* current label */
  long c_len = mem_len(&cs);
  char *c = mem_get(&cs);
  int i;

  /* Copy code, insert padding and adjust offsets. */
  for (i = 0; i < pad_n; i++) {
    this = pad_off[i];
    /* Strict comparison because padding is inserted before `this`. */
    while (rel < rel_n && rel_off[rel] < this) {
      rel_off[rel++] += diff;
    }
    while (jmp < jmp_n && jmp_off[jmp] < this) {
      jmp_off[jmp++] += diff;
    }
    while (lab < lab_sz && lab_loc[lab] < this) {
      lab_loc[lab++] += diff;
    }
    /* printf("%d %d\n", this, lab_loc[lab]); */
    mem_put(&cs, &c[prev], this - prev);
    prev = this;
    diff += o_align();
  }
  while (rel < rel_n)
    rel_off[rel++] += diff;
  while (jmp < jmp_n)
    jmp_off[jmp++] += diff;
  while (lab < lab_sz)
    lab_loc[lab++] += diff;
  lab_loc[LAB_RET] += diff;
  mem_put(&cs, &c[prev], c_len - prev);
  free(c);
}

/* introduce shorter jumps, if possible */
static void i_shortjumps() {
  long prev = 0;
  long this;
  long diff = 0; /* the difference after changing jump instructions */
  int rel = 0; /* current relocation */
  int lab = LAB_RET + 1; /* current label */
  long c_len = mem_len(&cs);
  char *c = mem_get(&cs);
  int i, *jmp_sz;

  /* Compute imm sizes.  NOTE: May not be optimal because distances become
   * (weakly) shorter later on.  Could fixed-point iterate. */
  jmp_sz = malloc(jmp_n * sizeof(*jmp_sz));
  for (i = 0; i < jmp_n; i++) {
    jmp_sz[i] = imm_size(lab_loc[jmp_dst[i]] - (jmp_off[i] + SIZE_W));
  }

  /* Copy code with shorter imms and adjust offsets. */
  for (i = 0; i < jmp_n; i++) {
    this = jmp_off[i];
    jmp_off[i] -= diff;
    while (rel < rel_n && rel_off[rel] <= this) {
      rel_off[rel++] -= diff;
    }
    while (lab < lab_sz && lab_loc[lab] <= this) {
      lab_loc[lab++] -= diff;
    }

    mem_put(&cs, &c[prev], this - prev + jmp_sz[i]);
    prev = this + SIZE_W;
    diff += SIZE_W - jmp_sz[i];
  }
  while (rel < rel_n)
    rel_off[rel++] -= diff;
  while (lab < lab_sz)
    lab_loc[lab++] -= diff;
  lab_loc[LAB_RET] -= diff;
  mem_put(&cs, &c[prev], c_len - prev);
  free(c);

  /* Write jump offsets. */
  for (i = 0; i < jmp_n; i++) {
    o_iat(jmp_off[i] - 2, jmp_sz[i] / 8 - 1, SZ_SZ);
    o_iat(jmp_off[i], lab_loc[jmp_dst[i]] - jmp_off[i] - jmp_sz[i], jmp_sz[i]);
  }
  free(jmp_sz);
}

void i_code(char **c, long *c_len, long **rsym, long **rflg, long **roff,
            long *rcnt) {
  int i, j;

  i_align();

  i_shortjumps();

  /* Convert bits to bytes; must align to a byte boundary. */
  o_align();
  *c_len = mem_len(&cs) / 8;
  *c = malloc(*c_len);
  for (i = 0; i < *c_len; i++) {
    unsigned char b = 0;
    for (j = 0; j < 8; j++) {
      b |= cs.s[i * 8 + j] << j;
    }
    (*c)[i] = b;
  }
  mem_done(&cs);

  *rsym = rel_sym;
  *rflg = rel_flg;
  *roff = rel_off;
  *rcnt = rel_n;
  rel_sym = NULL;
  rel_flg = NULL;
  rel_off = NULL;
  rel_n = 0;
  rel_sz = 0;
  jmp_n = 0;
  pad_n = 0;
}

void i_done(void) {
  free(jmp_off);
  free(jmp_dst);
  free(lab_loc);
  free(pad_off);
}
