#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <elf.h>

#include "trace.h"
#include "consts.h"
#include "mem.h"
#include "syscalls.h"
#include "debug.h"
#include "types.h"
#include "util.h"

/* Trace tags. */
#define T_MEM      T_TAG(0)
#define T_DECODE   T_TAG(1)
#define T_LOAD     T_TAG(2)
#define T_INST     T_TAG(3)
#define T_CALL    (T_TAG(4) | T_INST)
#define T_ALU     (T_TAG(5) | T_INST)
#define T_PP      (T_TAG(6) | T_INST)
#define T_JMP     (T_TAG(7) | T_INST)
#define T_LDST    (T_TAG(8) | T_INST)
#define T_ALT     (T_TAG(9) | T_INST)
#define T_SYSCALL (T_TAG(10) | T_ALT)
#define T_CTX      T_TAG(11)
#define T_REGS    (T_TAG(12) | T_CTX)
#define T_STACK   (T_TAG(13) | T_CTX)

/* Preprocessor nastiness. */
#define UINT(w) _UINT(uint,w,_t)
#define _UINT(a,b,c) a##b##c
typedef UINT(ICACHE_SIZE) icache_t;

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

/* Path of program being emulated. */
char *exe_path;
/* Path of debug script. */
char *debug_cmds_path;

/* Number of instructions executed. */
long long steps;

/* Instruction cache line. */
icache_t *iline;
/* Byte address of cache line. */
word iaddr;
/* Bit offset into cache line. */
byte ioff;

/* Register file. */
word regs[REG_N] = {0};

/* Root path. */
char *root_path = NULL;

/* Program counter. */
#if defined(TRACE) || defined(DEBUG)
bitaddr_t pc;
#endif

#ifdef DEBUG
/* Is debugging enabled? */
bool debug = false;
/* Call debugger hook on next instruction. */
bool trap = false;
#else
#define debug false
#endif

/* MEMORY */

/* Current mapping for instructions. */
mmap_t *imem = NULL;

static inline
mmap_t *mem_get(word addr, word numb) {
  mmap_t *m;

  m = mem_find(addr, numb);
  if (!m) {
    fatal("segmentation fault\n");
  }

  return m;
}

#define update_imem()                                   \
  do {                                                  \
    imem = mem_get(iaddr, sizeof(icache_t));            \
    iline = (icache_t*)&imem->data[iaddr - imem->addr]; \
  } while (0)

static inline
void mem_read(void *buf, word addr, word numb) {
  mmap_t *m;
  int i;

  m = mem_get(addr, numb);
  memcpy(buf, &m->data[addr - m->addr], numb);
  if (t_info) {
    info(T_MEM, "read(0x%08x, %d)", addr, numb);
    for (i = 0; i < numb; i++) {
      info(T_MEM, i & 0xf ? " " : "\n  ");
      info(T_MEM, "%02x", ((unsigned char*)buf)[i]);
    }
    info(T_MEM, "\n");
    indent(T_MEM, -2);
  }
}

static inline
void mem_write(word addr, void *buf, word numb) {
  mmap_t *m;
  int i;

  m = mem_get(addr, numb);
  memcpy(&m->data[addr - m->addr], buf, numb);
  if (t_info) {
    info(T_MEM, "write(0x%08x, %d)", addr, numb);
    for (i = 0; i < numb; i++) {
      info(T_MEM, i & 0xf ? " " : "\n  ");
      info(T_MEM, "%02x", ((unsigned char*)buf)[i]);
    }
    info(T_MEM, "\n");
    indent(T_MEM, -2);
  }
}

/* CPU FUNCTIONS */

static inline
void do_push(word val) {
  regs[REG_SP] -= sizeof(word);
  mem_write(regs[REG_SP], &val, sizeof(word));
}

static inline
word do_pop() {
  word val;
  mem_read(&val, regs[REG_SP], sizeof(word));
  regs[REG_SP] += sizeof(word);
  return val;
}

static inline
void do_goto(bitaddr_t bitaddr) {
  iaddr = (bitaddr & ~ICACHE_MASK) / 8;
  ioff = bitaddr & ICACHE_MASK;
  update_imem();
}

void do_syscall(void) {
  long nr, args[6];
  word ret;
  mmap_t *mem;
  int i;

  nr = regs[REG_A0];
  args[0] = regs[REG_A1];
  args[1] = regs[REG_A2];
  args[2] = regs[REG_A3];
  args[3] = regs[REG_T0];
  args[4] = regs[REG_T1];
  args[5] = regs[REG_T2];

  info(T_SYSCALL, ";; syscall(%s", SYS_NAMES[nr]);

  switch (nr) {
  case SYS_chroot:
    if (root_path) {
      info(T_SYSCALL, ", %s) = -EPERM", args[0]);
      ret = -EPERM;
    } else {
      goto PASSTHRU;
    }
    break;

  case SYS_mmap:
    info(T_SYSCALL, ", 0x%08x, %d, %#x) = ", args[0], args[1], args[2]);
    ret = mem_map(args[0], args[1], args[2], NULL);
    if (!ret) {
      ret = -EINVAL;
    }
    update_imem();
    info(T_SYSCALL, (ret ? "0x%08x" : "NULL"), ret);
    break;

  case SYS_munmap:
    info(T_SYSCALL, ", 0x%08x, %d)", args[0], args[1]);
    mem_unmap(args[0], args[1]);
    update_imem();
    ret = 0;
    break;

  case SYS_mprotect:
    /* TODO: */
    ret = -1;
    break;

  case SYS_exit:
    info(T_NONE, "exit(%d)'ed after %lld steps\n", args[0], steps);
    exit(args[0]);

  default:
  PASSTHRU:
    /* Heuristically convert pointers. */
    for (i = 0; i < sizeof(args) / sizeof(*args); i++) {
      info(T_SYSCALL, ", ");
      mem = mem_find(args[i], 1);
      if (mem) {
        info(T_SYSCALL, "0x%08x", args[i]);
        args[i] += (long)mem->data - mem->addr;
        guru(T_SYSCALL, "(%#lx)", args[i]);
      } else {
        info(T_SYSCALL, "%d", args[i]);
      }
    }

    info(T_SYSCALL, ") = ");
    ret = syscall(nr, args[0], args[1], args[2], args[3], args[4], args[5]);
    info(T_SYSCALL, "%d", ret);
  }

  info(T_SYSCALL, "\n");

  regs[REG_A0] = ret;
}

/* OPERAND TRACING */

#define trace_pc(tags, lvl, pc)                     \
  trace((tags), (lvl), "0x%08x.%d: ",               \
        (pc) / 8, (pc) & 7)

#define trace_val(tags, lvl, val, suf)              \
  do {                                              \
    trace((tags), (lvl), (val) < 0 || (val) > 9 ?   \
          "%d/%#x" : "%d", (val), (val));           \
    trace((tags), (lvl), (suf));                    \
  } while (0)

#define trace_dst(tags, lvl, reg, suf)                  \
  do {                                                  \
    trace((tags), (lvl), "%s", REG_NAMES[(reg)]);       \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_res(tags, lvl, res)                       \
  do {                                                  \
    if (t_trace((lvl) + 1)) {                           \
      trace((tags), (lvl), " ; ");                      \
      trace_val((tags), (lvl), res, "");                \
    }                                                   \
    trace((tags), (lvl), "\n");                         \
  } while (0)

#define trace_reg(tags, lvl, reg, suf)                  \
  do {                                                  \
    trace((tags), (lvl), "%s", REG_NAMES[(reg)]);       \
    if (t_trace((lvl) + 1) && REG_ZZ != (reg)) {        \
      trace((tags), (lvl), " (");                       \
      trace_val((tags), (lvl), regs[(reg)], ")");       \
    }                                                   \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_sz(tags, lvl, sz, suf)                    \
  do {                                                  \
    trace((tags), (lvl), "%s", SZ_NAMES[(sz) / 8 - 1]); \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_ext(tags, lvl, ext, suf)                  \
  do {                                                  \
    trace((tags), (lvl), "%s", EXT_NAMES[(ext)]);       \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_sign(tags, lvl, sign, suf)                \
  do {                                                  \
    trace((tags), (lvl), "%s", SIGN_NAMES[(sign)]);     \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_jmp(tags, lvl, ri, reg, imm, jmp, suf)    \
  do {                                                  \
    if (JMP_REG == ri) {                                \
      trace_dst((tags), (lvl), (reg), "");              \
    } else {                                            \
      trace((tags), (lvl), "%d", (imm));                \
    }                                                   \
    if (t_trace((lvl) + 1)) {                           \
      trace((tags), (lvl), " (0x%08x.%x)",              \
            (addr_t)((jmp) / 8), (byte)((jmp) & 7));    \
    }                                                   \
    trace((tags), (lvl), (suf));                        \
  } while (0)

#define trace_oper(tags, lvl, ri, reg, imm, suf)    \
  do {                                              \
    if (t_trace(lvl)) {                             \
      if (OPER_REG == ri) {                         \
        trace_reg((tags), (lvl), (reg), (suf));     \
      } else {                                      \
        trace_val((tags), (lvl), (imm), (suf));     \
      }                                             \
    }                                               \
  } while (0)

#define trace_addr(tags, lvl, regbase, ri, reg, imm, suf)       \
  do {                                                          \
    trace((tags), (lvl), "[");                                  \
    if (REG_ZZ != regbase) {                                    \
      trace_reg((tags), (lvl), regbase, " : ");                 \
    }                                                           \
    trace_oper((tags), (lvl), (ri), (reg), (imm), "]");         \
    if (t_trace((lvl) + 1) && REG_ZZ != regbase) {              \
      trace((tags), (lvl), "(0x%08x)", regs[(regbase)] + imm);  \
    }                                                           \
    trace((tags), (lvl), (suf));                                \
  } while (0)

#define info_pc(tags, pc)          trace_pc((tags), T_INFO, (pc))
#define info_val(tags, val, suf)   trace_val((tags), T_INFO, (val), (suf))
#define info_res(tags, val)        trace_res((tags), T_INFO, (val))
#define info_reg(tags, reg, suf)   trace_reg((tags), T_INFO, (reg), (suf))
#define info_dst(tags, reg, suf)   trace_dst((tags), T_INFO, (reg), (suf))
#define info_sz(tags, reg, suf)    trace_sz((tags), T_INFO, (sz), (suf))
#define info_ext(tags, reg, suf)   trace_ext((tags), T_INFO, (ext), (suf))
#define info_sign(tags, sign, suf) trace_sign((tags), T_INFO, (sign), (suf))
#define info_jmp(tags, ri, reg, imm, jmp, suf) \
  trace_jmp((tags), T_INFO, (ri), (reg), (imm), (jmp), (suf))
#define info_oper(tags, ri, reg, imm, suf)          \
  trace_oper((tags), T_INFO, (ri), (reg), (imm), (suf))
#define info_addr(tags, regbase, ri, reg, imm, suf)                \
  trace_addr((tags), T_INFO, (regbase), (ri), (reg), (imm), (suf))

/* INSTRUCTION DECODING */

static inline
word decode_fetch(byte bits) {
  word out;
  int i;

  guru(T_DECODE, "fetch(0x%08x.%d, %d): ", iaddr + ioff / 8, ioff % 8, bits);

  out = ioff == ICACHE_SIZE ? 0 : *iline >> ioff;

  if (bits + ioff >= ICACHE_SIZE) {
    iaddr += sizeof(icache_t);
    if (iaddr - imem->addr >= imem->size) {
      fatal("segmentation fault\n");
    }
    zen(T_DECODE, "Loading instruction cache line from 0x%08x\n", iaddr);
    iline++;
    out |= *iline << (ICACHE_SIZE - ioff);
    ioff = ioff + bits - ICACHE_SIZE;
  } else {
    ioff += bits;
  }

  if (bits < sizeof(word) * 8) {
    out &= (1 << bits) - 1;
  }

  if (t_guru) {
    for (i = 0; i < bits; i++) {
      guru(T_DECODE, out & (1 << i) ? "1" : "0");
    }
    guru(T_DECODE, "\n");
  }

  return out;
}

static inline
void decode_align() {
  info(T_DECODE, "align 0x%08x.%d -> ", iaddr + ioff / 8, ioff % 8);
  ioff = (ioff + 7) & ~7;
  info(T_DECODE, "0x%08x.0\n", iaddr + ioff / 8);
}

static inline
byte decode_reg(void) {
  byte reg = decode_fetch(REG_SZ);
  info(T_DECODE, "reg: %s (%d)\n", REG_NAMES[reg], reg);
  return reg;
}

static inline
byte decode_op(void) {
  byte op = decode_fetch(OP_SZ);
  info(T_DECODE, "op: %s (%d)\n", OP_NAMES[op], op);
  return op;
}

static inline
byte decode_alt(void) {
  byte alt = decode_fetch(ALT_SZ);
  info(T_DECODE, "alt: %s (%d)\n", ALT_NAMES[alt], alt);
  return alt;
}

static inline
byte decode_jcc(void) {
  byte jcc = decode_fetch(JCC_SZ);
  info(T_DECODE, "jcc: %s (%d)\n", JCC_NAMES[jcc], jcc);
  return jcc;
}

static inline
byte decode_sz(void) {
  byte sz = decode_fetch(SZ_SZ);
  info(T_DECODE, "sz: %s (%d)\n", SZ_NAMES[sz], sz);
  return (1 + sz) * 8;
}

static inline
byte decode_ext(void) {
  byte ext = decode_fetch(EXT_SZ);
  info(T_DECODE, "ext: %s (%d)\n", EXT_NAMES[ext], ext);
  return ext;
}

static inline
byte decode_sign(void) {
  byte sign = decode_fetch(SIGN_SZ);
  info(T_DECODE, "sign: %s (%d)\n", SIGN_NAMES[sign], sign);
  return sign;
}

static inline
word decode_imm(void) {
  sword imm;
  word sbit;
  byte sz;

  sz = decode_sz();
  imm = decode_fetch(sz);

  /* Sign extend. */
  sbit = 1 << (sz - 1);
  imm = (imm ^ sbit) - sbit;

  info(T_DECODE, "val: ");
  info_val(T_DECODE, imm, "\n");
  return imm;
}

#define OPER_REG 0
#define OPER_IMM 1
static inline
byte decode_oper_type(void) {
  byte ret;
  ret = decode_fetch(1);
  info(T_DECODE, "oper: ");
  info(T_DECODE, OPER_REG == ret ? "reg" : "imm");
  info(T_DECODE, "\n");
  return ret;
}

#define decode_oper(ri, reg, imm)               \
  do {                                          \
    ri = decode_oper_type();                    \
    indent(T_DECODE, 2);                        \
    if (OPER_REG == ri) {                       \
      reg = decode_reg();                       \
      imm = regs[reg];                          \
    } else {                                    \
      reg = 0; /* Silence compiler. */          \
      imm = decode_imm();                       \
    }                                           \
    indent(T_DECODE, -2);                       \
  } while(0)

#define JMP_REG 0
#define JMP_IMM 1
static inline
byte decode_jmp_type(void) {
  byte ret;
  ret = decode_fetch(1);
  info(T_DECODE, "jmp: ");
  info(T_DECODE, JMP_REG == ret ? "reg" : "imm");
  info(T_DECODE, "\n");
  return ret;
}

#define decode_jmp(ri, reg, imm, jmp)                       \
  do {                                                      \
    ri = decode_jmp_type();                                 \
    indent(T_DECODE, 2);                                    \
    if (JMP_REG == ri) {                                    \
      reg = decode_reg();                                   \
      imm = 0; /* Silence compiler. */                      \
      /* Registers hold absolute byte addresses. */         \
      jmp = 8 * (bitaddr_t)regs[reg];                       \
    } else {                                                \
      reg = 0; /* Silence compiler. */                      \
      imm = decode_imm();                                   \
      /* Immediate hold relative bit offsets. */            \
      jmp = (signed)imm;                                    \
      /* Must decode first to update `iaddr` and `ioff`. */ \
      jmp += (bitaddr_t)iaddr * 8 + ioff;                   \
    }                                                       \
    indent(T_DECODE, -2);                                   \
  } while(0)

#define decode_addr(regbase, ri, reg, imm)      \
  do {                                          \
    info(T_DECODE, "addr:\n");                 \
    indent(T_DECODE, 2);                        \
    regbase = decode_reg();                     \
    decode_oper(ri, reg, imm);                  \
    indent(T_DECODE, -2);                       \
  } while (0)

/* THE MIGHTY `step` FUNCTION */

static inline
bool step(void) {
  bool dojmp;
  byte op, sz, ext, sign, reg1, reg2, reg3, reg4, ri;
  word val1, val2, sbit;
  sword sval1, sval2;
  addr_t addr;
  bitaddr_t jmp;

#if defined(TRACE) || defined(DEBUG)
  pc = (bitaddr_t)iaddr * 8 + ioff;
#endif

#ifdef DEBUG
  /* Remember if we already called the hook, so we don't do it twice for BRK
   * instructions. */
  bool didtrap = trap;
  if (trap) {
    trap = debug_hook();
    do_goto(pc);
  }
#endif

  regs[REG_ZZ] = 0;
  /* Silence compiler. */
  sign = 0;

  info(T_REGS, "pc = %#x.%d\n", iaddr, ioff);
  info(T_REGS, "%s = 0x%08x, %s = 0x%08x, %s = 0x%08x, %s = 0x%08x\n",
        REG_NAMES[0], regs[0],
        REG_NAMES[1], regs[1],
        REG_NAMES[2], regs[2],
        REG_NAMES[3], regs[3]);
  info(T_REGS, "%s = 0x%08x, %s = 0x%08x, %s = 0x%08x, %s = 0x%08x\n",
        REG_NAMES[4], regs[4],
        REG_NAMES[5], regs[5],
        REG_NAMES[6], regs[6],
        REG_NAMES[7], regs[7]);
  info(T_REGS, "%s = 0x%08x, %s = 0x%08x, %s = 0x%08x, %s = 0x%08x\n",
        REG_NAMES[8], regs[8],
        REG_NAMES[9], regs[9],
        REG_NAMES[10], regs[10],
        REG_NAMES[11], regs[11]);
  info(T_REGS, "%s = 0x%08x, %s = 0x%08x, %s = 0x%08x, %s = 0x%08x\n",
        REG_NAMES[12], regs[12],
        REG_NAMES[13], regs[13],
        REG_NAMES[14], regs[14],
        REG_NAMES[15], regs[15]);

  op = decode_op();

  if (OP_ALT == op) {
    info_pc(T_ALT, pc);

    op = decode_alt();
    info(T_ALT, "%s\n", ALT_NAMES[op]);

    switch (op) {

    case ALT_SYSCALL:
      do_syscall();
      break;

    case ALT_HALT:
      return false;

    case ALT_ALIGN:
      decode_align();
      break;

    case ALT_BRK:
#ifdef DEBUG
      if (debug && !didtrap) {
        pc += OP_SZ + ALT_SZ;
        trap = debug_hook();
        do_goto(pc);
        return true;
      } else {
#endif
        fatal("trap\n");
#ifdef DEBUG
      }
#endif
      break;

    default:
      fatal("Undefined ALT opcode: %d\n", op);
    }

  }

  else if (OP_JCC == op) {

    op = decode_jcc();
    /* Delay tracing JMP as it may really be a RET. */
    if (t_info && JCC_JMP != op) {
      info_pc(T_JMP, pc);
      info(T_JMP, "%s ", JCC_NAMES[op]);
    }

    dojmp = false;
    switch (op) {

    case JCC_JMP:
      dojmp = true;
      break;

    case JCC_Z:
      reg1 = decode_reg();
      info_reg(T_JMP, reg1, ", ");
      dojmp = 0 == regs[reg1];
      break;

    case JCC_L:
    case JCC_LE:
    case JCC_G:
    case JCC_GE:
      sign = decode_sign();
      info(T_JMP, "%s ", SIGN_NAMES[sign]);

      /* Fall through. */
    case JCC_E:
    case JCC_NE:
      reg1 = decode_reg();
      val1 = regs[reg1];
      decode_oper(ri, reg2, val2);
      info_reg(T_JMP, reg1, ", ");
      info_oper(T_JMP, ri, reg2, val1, ", ");

      sval1 = (sword)val1;
      sval2 = (sword)val2;
      dojmp = ((JCC_E  == op && val1 == val2) ||
               (JCC_NE == op && val1 != val2) ||
               (JCC_L  == op && sign == SIGN_U && val1 < val2) ||
               (JCC_LE == op && sign == SIGN_U && val1 <= val2) ||
               (JCC_G  == op && sign == SIGN_U && val1 > val2) ||
               (JCC_GE == op && sign == SIGN_U && val1 >= val2) ||
               (JCC_L  == op && sign == SIGN_S && sval1 < sval2) ||
               (JCC_LE == op && sign == SIGN_S && sval1 <= sval2) ||
               (JCC_G  == op && sign == SIGN_S && sval1 > sval2) ||
               (JCC_GE == op && sign == SIGN_S && sval1 >= sval2));
      break;

    default:
      fatal("Undefined JCC opcode: %d\n", op);
    }

    /* Decode jmp target.  Reuse `val1`. */
    decode_jmp(ri, reg1, val1, jmp);

    /* Pseudo. */
    if (t_info && JCC_JMP == op) {
      if (REG_LR == reg1) {
        info_pc(T_CALL, pc);
        info(T_CALL, "ret");
        if (t_guru) {
          info(T_CALL, " ; jmp ");
          info_jmp(T_CALL, ri, reg1, val1, jmp, "");
        }
        info(T_CALL, "\n");
      } else {
        info_pc(T_JMP, pc);
        info(T_JMP, "jmp ");
        info_jmp(T_JMP, ri, reg1, val1, jmp, "\n");
      }
    } else {
        info_jmp(T_JMP, ri, reg1, val1, jmp, " ; ");
        info(T_JMP, dojmp ? "taken\n" : "not taken\n");
    }

    if (dojmp) {
      do_goto(jmp);
    }
  }

  else {

    switch (op) {

    case OP_LOAD:
      info_pc(T_LDST, pc);
      info(T_LDST, "load ");

      ext = decode_ext();
      sz = decode_sz();
      reg1 = decode_reg();
      decode_addr(reg2, ri, reg3, val1);
      addr = regs[reg2] + val1;

      info_ext(T_LDST, ext, " ");
      info_sz(T_LDST, sz, " ");
      info_dst(T_LDST, reg1, ", ");
      info_addr(T_LDST, reg2, ri, reg3, val1, "");

      val2 = 0;
      mem_read(&val2, addr, sz / 8);
      if (EXT_SX == ext) {
        sbit = 1 << (sz - 1);
        val2 = (val2 ^ sbit) - sbit;
      }
      regs[reg1] = val2;

      info_res(T_LDST, val2);
      break;

    case OP_STORE:
      info_pc(T_LDST, pc);
      info(T_LDST, "store ");

      sz = decode_sz();
      decode_addr(reg1, ri, reg2, val1);
      addr = regs[reg1] + val1;
      decode_oper(ri, reg3, val2);

      info_sz(T_LDST, sz, " ");
      info_addr(T_LDST, reg1, ri, reg2, val1, ", ");
      info_oper(T_LDST, ri, reg3, val2, "\n");

      mem_write(addr, &val2, sz / 8);
      break;

    case OP_PUSH:
      info_pc(T_PP, pc);
      info(T_PP, "push ");

      decode_oper(ri, reg1, val1);
      info_oper(T_PP, ri, reg1, val1, "\n");
      do_push(val1);
      break;

    case OP_POP:
      info_pc(T_PP, pc);
      info(T_PP, "pop ");

      reg1 = decode_reg();
      info_dst(T_PP, reg1, "");

      regs[reg1] = do_pop();

      info_res(T_PP, regs[reg1]);
      break;

    /* case OP_MUL: */
    case OP_SHR:
      sign = decode_sign();
      /* Fall through. */
    case OP_ADD:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
    case OP_SHL:
    case OP_MUL:
      info_pc(T_ALU, pc);

      reg1 = decode_reg();
      reg2 = decode_reg();
      val1 = regs[reg2];
      decode_oper(ri, reg3, val2);

      if (t_info) {
        if (OP_ADD == op && REG_ZZ == reg2) {
          info(T_ALU, "mov ");
          info_dst(T_ALU, reg1, ", ");
          info_oper(T_ALU, ri, reg3, val2, "\n");
        } else {
          if (OP_SHR == op && SIGN_S == sign) {
            info(T_ALU, "sar ");
          } else {
            info(T_ALU, OP_NAMES[op]);
          }
          info(T_ALU, " ");
          info_dst(T_ALU, reg1, ", ");
          info_reg(T_ALU, reg2, ", ");
          info_oper(T_ALU, ri, reg3, val2, "");
        }
      }

      switch (op) {
      case OP_ADD: regs[reg1] = val1 + val2; break;
      case OP_AND: regs[reg1] = val1 & val2; break;
      case OP_OR:  regs[reg1] = val1 | val2; break;
      case OP_XOR: regs[reg1] = val1 ^ val2; break;
      case OP_SHL: regs[reg1] = val1 << (val2 & 31); break;
      case OP_MUL:
        /* if (SIGN_U == sign) { */
          regs[reg1] = val1 * val2;
        /* } else { */
        /*   regs[reg1] = (sword)val1 * (sword)val2; */
        /* } */
        break;
      case OP_SHR:
        if (SIGN_U == sign) {
          regs[reg1] = val1 >> (val2 & 31);
        } else {
          regs[reg1] = (sword)val1 >> (val2 & 31);
        }
        break;
      }

      if (t_info && !(OP_ADD == op && REG_ZZ == reg2)) {
        info_res(T_ALU, regs[reg1]);
      }
      break;

    case OP_SUB:
      info_pc(T_ALU, pc);

      reg1 = decode_reg();
      reg2 = decode_reg();
      val1 = regs[reg2];
      reg3 = decode_reg();
      val2 = regs[reg3];

      if (REG_ZZ == reg2 && REG_ZZ == reg3) {
        info(T_ALU, "mov ");
        info_dst(T_ALU, reg1, ", 0\n");
      } else {
        info(T_ALU, "sub ");
        info_dst(T_ALU, reg1, ", ");
        info_reg(T_ALU, reg2, ", ");
        info_reg(T_ALU, reg3, "");
      }

      regs[reg1] = val1 - val2;
      if (t_info && !(REG_ZZ == reg2 && REG_ZZ == reg3)) {
        info_res(T_ALU, regs[reg1]);
      }
      break;

    case OP_DIVMOD:
      info_pc(T_ALU, pc);

      sign = decode_sign();
      reg1 = decode_reg();
      reg2 = decode_reg();
      reg3 = decode_reg();
      val1 = regs[reg3];
      decode_oper(ri, reg4, val2);

      if (t_info) {
        if (REG_ZZ == reg2) {
          info(T_ALU, "div ");
          info_dst(T_ALU, reg1, ", ");
        } else if (REG_ZZ == reg1) {
          info(T_ALU, "mod ");
          info_dst(T_ALU, reg2, ", ");
        } else {
          info(T_ALU, "divmod ");
          info_dst(T_ALU, reg1, ", ");
          info_dst(T_ALU, reg2, ", ");
        }
        info_reg(T_ALU, reg3, ", ");
        info_oper(T_ALU, ri, reg4, val2, "");
      }

      if (SIGN_U == sign) {
        regs[reg1] = val1 / val2;
        regs[reg2] = val1 % val2;
      } else {
        regs[reg1] = (sword)val1 / (sword)val2;
        regs[reg2] = (sword)val1 % (sword)val2;
      }

      if (t_guru) {
        info(T_ALU, " ; ");
        if (REG_ZZ == reg2) {
          info(T_ALU, "div ");
          info_val(T_ALU, regs[reg1], "");
        } else if (REG_ZZ == reg1) {
          info(T_ALU, "mod ");
          info_val(T_ALU, regs[reg2], "");
        } else {
          info(T_ALU, "divmod ");
          info_val(T_ALU, regs[reg1], ", ");
          info_val(T_ALU, regs[reg2], "");
        }
      }
      info(T_ALU, "\n");
      break;

    case OP_CALL:
      info_pc(T_CALL, pc);
      info(T_CALL, "call ");
      decode_jmp(ri, reg1, val1, jmp);
      decode_align();

      info_jmp(T_CALL, ri, reg1, val1, jmp, "\n");

      regs[REG_LR] = iaddr + ioff / 8;
      do_goto(jmp);
      break;

    default:
      fatal("Undefined opcode: %d\n", op);
    }
  }
  return true;
}

/* PROGRAM LOADING */

word read_leb128(int fd, word *out) {
  word n = 0;
  word x = 0;
  byte b;

  for (;;) {
    if (0 >= read(fd, &b, 1)) {
      break;
    }
    x |= (b & 0x7f) << n;
    n += 7;
    if (b & 0x80) {
      continue;
    }
    break;
  }

  /* Sign extend. */
  if (b & 0x40) {
    x |= ~0 << n;
  }

  *out = x;
  return n;
}

addr_t load_eh3(int fd) {
  byte hdr[sizeof(EH3_HDR)];
  word type, prot;
  addr_t addr, entry;
  word offset, size, numb, segments;
  mmap_t *segment;

  readn(fd, hdr, sizeof(hdr));
  if (memcmp(hdr, EH3_HDR, sizeof(hdr))) {
    fatal("Bad header\n");
  }

  if (0 == read_leb128(fd, &type)) {
    fatal("No EH3 file type\n");
  }
  if (type != EH3_EXE) {
    fatal("Not and EH3 executable\n");
  }
  info(T_LOAD, "type = %d\n", type);

  if (0 == read_leb128(fd, &entry)) {
    fatal("No entry point\n");
  }
  info(T_LOAD, "entry = 0x%08x\n", entry);

  if (0 == read_leb128(fd, &segments)) {
    fatal("No segment table\n");
  }
  info(T_LOAD, "segments =\n");

  for (;;) {
    if (0 == read_leb128(fd, &addr)) {
      break;
    }
    if (0 == read_leb128(fd, &size)) {
      fatal("Bad segment\n");
    }
    if (0 == read_leb128(fd, &prot)) {
      fatal("Bad segment\n");
    }
    if (0 == read_leb128(fd, &numb)) {
      fatal("Bad segment\n");
    }
    if (numb > size) {
      fatal("Segment size is less than data\n");
    }

    info(T_LOAD, "  0x%08x-0x%08x (size = %#x, len(data) = %#x)\n",
          addr, addr + size, size, numb);
    if (!mem_map(addr, numb, 7, &segment)) {
      fatal("Could not map segment %#x--%#x\n", addr, addr + size);
    }

    offset = addr - segment->addr;
    readn(fd, &segment->data[offset], numb);
    memset(&segment->data[offset + numb], 0, size - numb);
  }

  return entry;
}

addr_t load_elf(int fd) {
  int i;
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdr;
  mmap_t *segment;
  addr_t addr;
  word offset, size, numb;

  readn(fd, &ehdr, sizeof(ehdr));

  for (i = 0; i < ehdr.e_phnum; i++) {
    lseek(fd, ehdr.e_phoff + sizeof(phdr) * i, SEEK_SET);
    readn(fd, &phdr, sizeof(phdr));
    if (phdr.p_type != PT_LOAD) {
      continue;
    }

    addr = phdr.p_vaddr;
    size = phdr.p_memsz;
    numb = phdr.p_filesz;

    info(T_LOAD, "  0x%08x-0x%08x (size = %#x, len(data) = %#x)\n",
          addr, addr + size, size, numb);
    if (!mem_map(addr, size, 7, &segment)) {
      fatal("Could not map segment %#--%#x\n", addr, addr + size);
    }

    lseek(fd, phdr.p_offset, SEEK_SET);
    offset = addr - segment->addr;
    readn(fd, &segment->data[offset], numb);
    memset(&segment->data[offset + numb], 0, size - numb);
  }

  return ehdr.e_entry;
}

void load_and_run(char *exe, int argc, char *argv[], char *envp[]) {
  int fd, nenv, i, len;
  addr_t entry, p;
  char ehdr[MAX(sizeof(EH3_HDR), sizeof(ELF_HDR))];

  info(T_LOAD, "Loading program '%s'\n", exe);
  fd = open(exe, O_RDONLY);
  if (-1 == fd) {
    perror("open");
    exit(EXIT_FAILURE);
  }

  if (!mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE, 6, NULL)) {
    fatal("Could not allocate stack\n");
  }
  regs[REG_SP] = STACK_TOP & MASK_W;

  /* Find length of envp. */
  for (nenv = 0; envp[nenv]; nenv++);

  /* Save envp strings. */
  for (i = nenv - 1; i >= 0; i--) {
    len = strlen(envp[i]) + 1;
    regs[REG_SP] -= len;
    mem_write(regs[REG_SP], envp[i], len);
  }

  /* Save argv strings. */
  for (i = argc - 1; i >= 0; i--) {
    len = strlen(argv[i]) + 1;
    regs[REG_SP] -= len;
    mem_write(regs[REG_SP], argv[i], len);
    info(T_LOAD, "argv[%d] = %p (%s)\n", i, regs[REG_SP], argv[i]);
  }

  /* Align SP. */
  regs[REG_SP] &= -4;

  /* Invariant: points just after the next string. */
  p = STACK_TOP & MASK_W;

  /* Save envp array. */
  do_push(0);
  for (i = nenv - 1; i >= 0; i--) {
    p -= strlen(envp[i]) + 1;
    do_push(p);
  }

  /* Save argv array. */
  for (i = argc - 1; i >= 0; i--) {
    p -= strlen(argv[i]) + 1;
    do_push(p);
    info(T_LOAD, "&argv[%d] = %#x\n", i, regs[REG_SP], argv[i]);
  }

  /* Save argc. */
  do_push(argc);

  readn(fd, ehdr, sizeof(ehdr));
  lseek(fd, 0, SEEK_SET);

  if (0 == memcmp(ehdr, ELF_HDR, sizeof(ELF_HDR))) {
    entry = load_elf(fd);
  } else if (0 == memcmp(ehdr, EH3_HDR, sizeof(EH3_HDR))) {
    entry = load_eh3(fd);
  } else {
    fatal("Unknown file type\n");
  }

  close(fd);

  info(T_LOAD, "Jump to entry point: 0x%08x\n", entry);
  do_goto((bitaddr_t)entry * 8);

  /* Silence compiler. */
#ifdef DEBUG
  if (debug) {
    pc = (bitaddr_t)iaddr * 8 + ioff;
    debug_init(exe_path, debug_cmds_path);
    atexit(debug_fini);
  }
#endif

  while (step()) {
    steps++;
  }
  warn(T_NONE, "Halted after %lld steps\n", steps);
}

void usage(const char *name) {
  fprintf(stderr,
          "usage: %s [<options>] <program> [<args>]\n"
          "  -h, --help\n"
          "     Show this message.\n"
          "  -g, --debug\n"
          "     Enable debugger.\n"
          "  -x, --exec <path>\n"
          "     Debug script to run after program load.  Implies -g.\n"
          "  -t, --trace <tag>\n"
          "     Enable tracing of <tag>.  Use 'all' to enable all tags.\n"
          "     load        Program loading.\n"
          "     decode      Instruction decoding\n"
          "     mem         Memory operations.\n"
          "     ctx         Program context.\n"
          "      regs       Registers.\n"
          "      stack      Stack.\n"
          "     inst        Instructions.\n"
          "      call       CALL, RET.\n"
          "      alu        ADD, SUB, ...\n"
          "      pp         PUSH, POP.\n"
          "      jmp        JCC.\n"
          "      ldst       LOAD, Store.\n"
          "      alt        ALT.\n"
          "       syscall   SYSCALL.\n"
          "  -r --root <path>\n"
          "     Path to root directory.  Subsequent `chroot` calls will return `EPERM`.\n"
          "  -v, --verbose\n"
          "    More verbose tracing.  May be specified more than once.\n"
          "  -q, --quiet\n"
          "    Less verbose tracing.  May be specified more than once.\n"
          , name);
  exit(EXIT_FAILURE);
}


int main(int argc, char *argv[], char *envp[]) {
  int i, j;
  char *arg, *tag;

  t_tag_mark(T_MEM, "mem");
  t_tag_mark(T_DECODE, "decode");
  t_tag_mark(T_LOAD, "load");
  t_tag_mark(T_INST, "inst");
  /* t_tag_mark(T_CALL, "call"); */
  /* t_tag_mark(T_ALU, "alu"); */
  /* t_tag_mark(T_PP, "pp"); */
  /* t_tag_mark(T_JMP, "jmp"); */
  /* t_tag_mark(T_LDST, "ldst"); */
  /* t_tag_mark(T_ALT, "alt"); */
  /* t_tag_mark(T_SYSCALL, "syscall"); */
  t_tag_mark(T_CTX, "ctx");
  /* t_tag_mark(T_REGS, "regs"); */
  /* t_tag_mark(T_STACK, "stack"); */
  struct {
    const char *name;
    long tag;
  } tags[] = {
    {"mem", T_MEM},
    {"decode", T_DECODE},
    {"load", T_LOAD},
    {"inst", T_INST},
    {"call", T_CALL},
    {"alu", T_ALU},
    {"pp", T_PP},
    {"jmp", T_JMP},
    {"ldst", T_LDST},
    {"alt", T_ALT},
    {"syscall", T_SYSCALL},
    {"ctx", T_CTX},
    {"regs", T_REGS},
    {"stack", T_STACK},
    {NULL, 0},
  };

#if !defined(DEBUG) || !defined(TRACE)
bool didwarn = false;
#define recompile(what)                                                 \
  do {                                                                  \
    if (!didwarn) {                                                     \
      fprintf(stderr, "WARNING: Recompile with " what " support for "   \
              "%s to take effect.\n", arg);                             \
      didwarn = true;                                                   \
    }                                                                   \
  } while (0)
#endif

#define popvar(var)                                                     \
  do {                                                                  \
    var = argv[i];                                                      \
    memmove(&argv[i], &argv[i + 1], sizeof(*argv) * (argc - i - 1));    \
    argc--;                                                             \
  } while (0)
#define poparg() popvar(arg)

#define isopt(short, long)                                              \
  ((short && 0 == memcmp((arg), "-" short, 2)) ||                       \
   (long && 0 == memcmp((arg), "--" long, strlen(long) + 2) &&          \
    (0   == arg[strlen(long) + 2] ||                                    \
     '=' == arg[strlen(long) + 2])                                      \
    )                                                                   \
   )
#define isflg(short, long)                      \
  ((short && 0 == strcmp((arg), "-" short)) ||  \
   (long && 0 == strcmp((arg), "--" long)))

#define popopt(var)                                         \
  do {                                                      \
    if ('-' != arg[1] && arg[2]) {                          \
      var = arg + 2;                                        \
    } else if ((var = strchr(arg, '='))) {                  \
      var++;                                                \
    } else {                                                \
      if (i == argc) {                                      \
        fprintf(stderr, "%s needs an argument.\n", arg);    \
        exit(EXIT_FAILURE);                                 \
      }                                                     \
      popvar(var);                                          \
    }                                                       \
  } while (0)

#define repopt(n)                               \
  do {                                          \
    n = 1;                                      \
    if ('-' != arg[1]) {                        \
      while (arg[n + 1]) {                      \
        if (arg[++n] != arg[1]) {               \
          badarg();                             \
        }                                       \
      }                                         \
    }                                           \
  } while (0)

#define badarg()                                    \
  do {                                              \
    fprintf(stderr, "Invalid argument: %s\n", arg); \
    exit(EXIT_FAILURE);                             \
  } while (0)

#ifdef DEBUG
#define do_debug()                              \
  do {                                          \
    debug = true;                               \
    /* Trap on first instruction. */            \
    trap = true;                                \
  } while (0)
#else
#define do_debug() recompile("debugging")
#endif

#ifdef TRACE
#define do_trace()
#else
#define do_trace() recompile("tracing");
#endif

  for (i = 1; i < argc;) {
    if (argv[i][0] != '-') {
      i++;
      continue;
    }
    poparg();

    if (0 == strcmp(arg, "--")) {
      break;
    }

    if (isflg("h", "help")) {
      usage(argv[0]);
    }

    else if (isflg("g", "debug")) {
      do_debug();
    }

    else if (isflg("x", "exec")) {
      popopt(debug_cmds_path);
      do_debug();
    }

    else if (isopt("t", "trace")) {
      popopt(tag);
      if (0 == strcmp(tag, "all")) {
        t_tags_on(T_ALL);
      } else {
        for (j = 0; tags[j].name; j++) {
          if (0 == strcmp(tag, tags[j].name)) {
            t_tag_on(tags[j].tag);
            break;
          }
        }
        if (NULL == tags[j].name) {
          fprintf(stderr, "Invalid tag name: %s\n", arg);
          exit(EXIT_FAILURE);
        }
      }
      do_trace();
    }

    else if (isopt("r", "root")) {
      popopt(root_path);
      if (-1 == chroot(root_path)) {
        perror("chroot");
        exit(EXIT_FAILURE);
      }
    }

    else if (isflg("v", "verbose")) {
      repopt(j);
      t_lvl_add(j);
      do_trace();
    }

    else if (isflg("q", "quiet")) {
      repopt(j);
      t_lvl_add(-j);
      do_trace();
    }

    else {
      badarg();
    }

  }

  if (argc < 2) {
    usage(argv[0]);
  }
  exe_path = argv[1];
  load_and_run(argv[1], argc - 1, &argv[1], envp);

  return EXIT_SUCCESS;
}
