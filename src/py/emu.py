#!/usr/bin/env python2.7
import os
import sys
import time
import argparse
import string
import struct
import random
import errno
import ctypes
import subprocess

from exe import *
from consts import *
from syscalls import *

libc = ctypes.CDLL('libc.so.6')
_syscall = libc.syscall
_syscall.restype = ctypes.c_long
def syscall(nr, *args):
    res = _syscall(nr, *args)
    if res == -1:
        return -ctypes.get_errno()
    else:
        return res

# greatly speed up execution speed if pypy is installed
if os.readlink('/proc/self/exe') != os.path.realpath('/usr/bin/pypy'):
  try:
    os.execvp('pypy', ['pypy'] + sys.argv)
  except:
    pass

def isprint(c):
    return c in string.ascii_letters + string.digits + string.punctuation + ' '

log_level = [LOG_WARNING]
def set_log_level(lvl):
    log_level[-1] = lvl

def push_log_level(lvl):
    log_level.append(lvl)

def pop_log_level():
    if len(log_level) >= 2:
        log_level.pop()

def log(msg, lvl):
    if lvl <= log_level[-1]:
        print >>sys.stderr, msg

def guru(msg):
    log('Guru meditation: %s' % msg, LOG_GURU)

def debug(msg):
    log(msg, LOG_DEBUG)

def info(msg):
    log(msg, LOG_INFO)

def warn(msg):
    log('WW: %s' % msg, LOG_WARNING)

def critical(msg):
    log('EE: %s' % msg, LOG_CRITICAL)

def error(msg):
    log('EE: %s' % msg, LOG_CRITICAL)
    sys.exit(-1)

class ICache(object):
    def __init__(self, vm):
        self.vm = vm
        # Current mapping, initialized to clean up `goto`
        self.mem = (-1, '')
        # Offset into current memory mapping
        self.off = 0
        # Number of bits in the cache
        self.len = 0
        # Instruction cache
        self.buf = 0

    def fill(self):
        # Fetch a cache line from memory
        beg = self.off & ~ICACHE_MASK
        end = beg + ICACHE_SIZE
        beg //= 8
        end //= 8
        assert end <= len(self.mem[1])
        self.len = ICACHE_SIZE
        self.buf, = struct.unpack(ICACHE_FMT, self.mem[1][beg:end])

    def __call__(self, n):
        ret = 0
        nb = 0 # next bit
        while n > self.len:
            ret |= self.buf << nb
            n -= self.len
            nb += self.len
            self.off += self.len

            self.fill()

        ret |= (self.buf & ((1 << n) - 1)) << nb
        self.buf >>= n
        self.off += n
        self.len -= n

        return ret

    def goto(self, addr):
        # Address within another mapping
        base, data = self.mem
        byteaddr = addr // 8
        if byteaddr < base or byteaddr >= base + len(data):
            self.mem = self.vm.mem_search(byteaddr)
            assert self.mem

        self.off = addr - self.mem[0] * 8

        # XXX: Optimization: always keep full cache line an detect jumps within
        #      it, avoiding a fill
        self.fill()

        # Offset into cache line
        n = self.off & ICACHE_MASK
        self.len -= n
        self.buf >>= n

    def align(self):
        # Byte align
        n = 8 - self.off & 7
        self.off += n
        self.len -= n
        self.buf >>= n

class VM(object):
    def __init__(self, exe, stack_top=STACK_TOP,
                 stack_size=STACK_SIZE):
        self.mem = []
        self.regs = [0] * REG_N
        self.flgs = [0] * FLG_N

        self.stack_top = stack_top
        self.stack_size = stack_size
        assert self.mmap(stack_top - stack_size, stack_size)
        self.regs[REG_SP] = stack_top

        for segment in exe.segments:
            assert self.mmap(segment.addr, segment.size), \
                'MMAP(%#x, %#x)' % (segment.addr, segment.size)
            if segment.data:
                self.write(segment.addr, segment.data)

        self.code = ICache(self)
        self.code.goto(exe.entry * 8)

    @property
    def pc(self):
        # Bit address
        base, _ = self.code.mem
        return (base << 3) + self.code.off

    def dump_regs(self):
        info('>>> REGISTER DUMP')
        for r in xrange(0, REG_N, 4):
            info('  %-3s: 0x%08x  %-3s: 0x%08x'
                 '  %-3s: 0x%08x  %-3s: 0x%08x' % \
                 (REG_NAMES[r    ], self.regs[r    ],
                  REG_NAMES[r + 1], self.regs[r + 1],
                  REG_NAMES[r + 2], self.regs[r + 2],
                  REG_NAMES[r + 3], self.regs[r + 3],
                 ))

    def dump_reg(self, r):
        info('>>> %-3s: 0x%08x' % (REG_NAMES[r], self.regs[r]))

    def dump_mem(self, addr):
        beg = addr
        end = beg + 0x100
        info('>>> MEMORY DUMP ( 0x%08x -> 0x%08x )' % (beg, end))
        for addr in xrange(beg, end, 16):
            if addr >= MEM_SIZE:
                break
            line = self.mem[addr : addr + 16]
            text = ''.join(c if isprint(c) else '.' for c in map(chr, line))
            info('%05x'
                      '  %02x %02x %02x %02x'
                      '  %02x %02x %02x %02x'
                      '  %02x %02x %02x %02x'
                      '  %02x %02x %02x %02x'
                      '  |%s|'
                      % ((addr,) + tuple(line) + (text,)))

    def mem_bisect(self, addr):
        i = 0
        j = len(self.mem)

        # Find highest mapping starting <= `addr`
        while i < j:
            k = (i + j) // 2
            if addr < self.mem[k][0]:
                j = k
            else:
                i = k + 1

        return i - 1

    def mem_search(self, addr, size=1):
        i = self.mem_bisect(addr)
        if i == len(self.mem):
            return None

        base, data = self.mem[i]

        # Check that `addr` is within mapping
        if addr < base:
            return None
        if addr + size > base + len(data):
            return None

        return base, data

    def mmap(self, addr, numb):
        # Round down/up
        numb = (numb + (addr & PAGE_MASK) + PAGE_MASK) & ~PAGE_MASK
        addr &= ~PAGE_MASK
        assert addr + numb <= MEM_SIZE

        if addr == 0:
            free = []
            # Page 0 cannot be mapped
            prev = PAGE_SIZE
            for base, mem in self.mem + [(MEM_SIZE, '')]:
                if numb < base - prev:
                    free.append((prev, base))
                prev = base + len(mem)
            if not free:
                return 0
            beg, end = random.choice(free)
            addr = random.randint(beg, end - numb) & ~PAGE_MASK

        guru('MMAP(0x%08x, %d)' % (addr, numb))

        data = bytearray(numb)
        # Where to insert the new mapping
        i = self.mem_bisect(addr)
        # How many mappings to be removed
        n = 0

        # Merge backwards?
        if i >= 0:
            base, mem = self.mem[i]
            if base + len(mem) == addr:
                guru('  Merge backwards: 0x%08x' % base)
                data = mem + data
                addr = base
                numb += len(mem)
                n += 1
                i -= 1
            elif base + len(mem) > addr:
                return 0

        i += 1

        # Merge forwards?
        if i + 1 < len(self.mem):
            base, mem = self.mem[i + 1]
            if base == addr + numb:
                guru('  Merge forwards: 0x%08x' % base)
                data += mem
                n += 1
            # Overlaps
            elif base < addr + numb:
                return 0

        # Insert new
        if n == 0:
            self.mem.insert(i, (addr, data))
        # Overwrite
        else:
            self.mem[i] = (addr, data)

        # Delete mappings
        if n > 1:
            self.mem[i + 1 : i + n] = []

        return addr

    def munmap(self, addr, numb):
        return False

    def read(self, addr, numb):
        base, mem = self.mem_search(addr, numb)
        beg = addr - base
        end = beg + numb
        return mem[beg:end]

    def write(self, addr, data):
        base, mem = self.mem_search(addr, len(data))
        beg = addr - base
        end = beg + len(data)
        mem[beg:end] = bytearray(data)

    def readb(self, addr):
        return struct.unpack('B', self.read(addr, 1))[0]

    def readh(self, addr):
        return struct.unpack('<H', self.read(addr, 2))[0]

    def readw(self, addr):
        return struct.unpack('<I', self.read(addr, 4))[0]

    def writeb(self, addr, val):
        self.write(addr, struct.pack('B', val))

    def writeh(self, addr, val):
        self.write(addr, struct.pack('<H', val))

    def writew(self, addr, val):
        self.write(addr, struct.pack('<I', val))

    def push(self, val):
        self.regs[REG_SP] -= 4
        self.writew(self.regs[REG_SP], val)

    def pop(self):
        val = self.readw(self.regs[REG_SP])
        self.regs[REG_SP] += 4
        return val

    def syscall(self):
        nr = self.regs[REG_A0]
        args = [
            self.regs[REG_A1],
            self.regs[REG_A2],
            self.regs[REG_A3],
            self.regs[REG_T0],
            self.regs[REG_T1],
            self.regs[REG_T2],
        ]

        if nr == SYS_mmap:
            addr = self.mmap(args[0], args[1])
            return addr or -errno.EINVAL

        elif nr == SYS_munmap:
            if self.munmap(args[0], args[1]):
                return 0
            else:
                return -errno.EINVAL

        args_str = []
        for i, arg in enumerate(args):
            mem = self.mem_search(arg)
            if mem:
                base, data = mem
                offset = arg - base
                numb = len(data) - offset
                data = data[offset:]
                args[i] = (ctypes.c_ubyte * numb).from_buffer(data)

                arg = '0x%08x' % arg
                guru('Arg #1 cast to pointer: %s' % arg)
            else:
                arg = str(arg)
            args_str.append(arg)

        debug('  ;; %s(%s)' % (
            SYS_NAMES.get(nr, '<invalid>'),
            ', '.join(args_str)))

        return syscall(nr, *args)

    def spin(self):
        pcaddr = '%#x.%05d' % (self.code.mem[0], self.code.off)
        guru('PC = %s' % pcaddr)
        cc = self.code(4)
        op = self.code(4)
        self.regs[REG_ZZ] = 0

        def get_reg():
            reg = self.code(4)
            dbg = '%s' % REG_NAMES[reg]
            return reg, dbg

        def get_regval():
            reg, dbg = get_reg()
            val = self.regs[reg]
            if reg != REG_ZZ:
                dbg += ' = %#x' % val
            return val, dbg

        def get_imm():
            sz, _ = get_sz()
            imm = self.code(sz)
            dbg = '%#x' % imm
            return imm, dbg

        def get_op():
            if self.code(1) == 0:
                val, dbg = get_regval()
            else:
                val, dbg = get_imm()
            return val, dbg

        def get_sz():
            sz = self.code(2)
            dbg = '%s' % SZ_NAMES[sz]
            return (sz + 1) * 8, dbg

        def get_addr():
            dbg = '['
            addr, dbg1 = get_op()
            dbg += dbg1
            noscale = self.code(1)
            if not noscale:
                reg, dbg2 = get_regval()
                imm = self.code(5)
                addr += reg << imm
                dbg += ' : %s << %d' % (dbg2, imm)
            dbg += ']'
            return addr, dbg

        def get_jmp(iscall=False):
            isreg = self.code(1) == 0
            if isreg:
                val, dbg = get_regval()
            else:
                sz, dbg = get_sz()
                sbit = 1 << (sz - 1)
                val = self.code(sz)
                if val & sbit:
                    val |= ~0 << sz
                dbg += ' %d' % val
            return isreg, val, dbg

        def get_binop():
            reg, dbg1 = get_reg()
            op1, dbg2 = get_regval()
            op2, dbg3 = get_op()
            return reg, op1, op2, ', '.join((dbg1, dbg2, dbg3))

        if cc == CC_ALT:
            guru('OP = %d(%s)' % (op, OP_ALT_NAMES[op]))
            debug('%s: %s' % (pcaddr, OP_ALT_NAMES[op]))

            if op == OP_ALT_HALT:
                return False

            elif op == OP_ALT_ALIGN:
                self.code.align()

            elif op == OP_ALT_NOP:
                pass

            elif op == OP_ALT_BRK:
                return False

            elif op == OP_ALT_SYSCALL:
                self.regs[REG_A0] = self.syscall()

            else:
                error('Undefined ALT OP: %d' % op)

        else:
            guru('CC = %d(%s)' % (cc, CC_NAMES[cc]))
            guru('OP = %d(%s)' % (op, OP_NAMES[op]))

            sf, zf, of, cf = self.flgs
            if not any([
                    cc == CC_ALWAYS,
                    cc == CC_E  and zf,
                    cc == CC_B  and cf,
                    cc == CC_BE and (cf or zf),
                    cc == CC_A  and not (cf or zf),
                    cc == CC_AE and not cf,
                    cc == CC_L  and sf != of,
                    cc == CC_LE and (sf != of or zf),
                    cc == CC_G  and (sf == of and not zf),
                    cc == CC_GE and sf == of,
                    cc == CC_O  and of,
                    cc == CC_S  and sf,
                    cc == CC_NE and not zf,
                    cc == CC_NO and not of,
                    cc == CC_NS and not sf,
            ]):
                guru('(skipped)')
                return True

            if op == OP_LOAD:
                ext, dbg1 = get_ext()
                sz, dbg2 = get_sz()
                reg, dbg3 = get_reg()
                addr, dbg4 = get_addr()
                dbg = '%s %s %s, %s' % (dbg1, dbg2, dbg3, dbg4)
                if sz == 8:
                    val = self.readb(addr)
                    sbit = SIGN_B
                elif sz == 16:
                    val = self.readh(addr)
                    sbit = SIGN_H
                elif sz == 32:
                    val = self.readw(addr)
                    sbit = SIGN_W
                if ext == EXT_SX and val & sbit:
                    val |= ~0 << sz
                self.regs[reg] = val & MASK_W

            elif op == OP_STORE:
                sz, dbg1 = get_sz()
                addr, dbg2 = get_addr()
                oper, dbg3 = get_op()
                dbg = '%s %s, %s' % (dbg1, dbg2, dbg3)
                if sz == 8:
                    self.writeb(addr, oper & MASK_B)
                elif sz == 16:
                    self.writeh(addr, oper & MASK_H)
                elif sz == 32:
                    self.writew(addr, oper & MASK_W)

            elif op == OP_PUSH:
                oper, dbg = get_op()
                self.push(oper)

            elif op == OP_POP:
                reg, dbg = get_reg()
                self.regs[reg] = self.pop()

            elif op in (OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_XOR, OP_AND,
                        OP_OR, OP_SHL, OP_SHR):
                reg, op1, op2, dbg = get_binop()

                if op == OP_ADD:
                    res = (op1 + op2) & MASK_W
                    sf = bool(res & SIGN_W)
                    zf = res == 0
                    of = \
                        (op1 & SIGN_W) == (op2 & SIGN_W) and \
                        (op1 & SIGN_W) != (res & SIGN_W)
                    cf = res < op1 or res < op2

                elif op == OP_SUB:
                    res = (op1 - op2) & MASK_W
                    sf = bool(res & SIGN_W)
                    zf = res == 0
                    cf = res < op1 or res < op2
                    of = op1 and op2 and cf

                elif op == OP_MUL:
                    res = (op1 * op2) & MASK_W
                    of = op1 and op2 and (res < op1 or res < op2)

                elif op == OP_DIV:
                    res = (op1 // op2) & MASK_W
                    zf = res == 0

                elif op == OP_MOD:
                    res = (op1 % op2) & MASK_W
                    zf = res == 0

                elif op == OP_XOR:
                    res = (op1 ^ op2) & MASK_W
                    sf = bool(res & SIGN_W)
                    zf = res == 0

                elif op == OP_AND:
                    res = (op1 & op2) & MASK_W
                    sf = bool(res & SIGN_W)
                    zf = res == 0

                elif op == OP_OR:
                    res = (op1 | op2) & MASK_W
                    sf = bool(res & SIGN_W)
                    zf = res == 0

                elif op == OP_SHL:
                    res = (op1 << op2) & MASK_W
                    of = (res >> op2) != op1
                    sf = bool(res & SIGN_W)
                    zf = res == 0

                elif op == OP_SHR:
                    res = (op1 >> op2) & MASK_W
                    of = (res << op2) != op1
                    zf = res == 0

                self.regs[reg] = res

            elif op in (OP_JMP, OP_CALL):
                isreg, op1, dbg = get_jmp(iscall=op==OP_CALL)
                if op == OP_CALL:
                    self.regs[REG_LR] = (self.pc // 8) & REG_MASK
                if isreg:
                    self.code.goto(op1 * 8)
                else:
                    self.code.goto(op1 + self.pc)

            else:
                error('Undefined OP: %d' % op)

            if cc == CC_ALWAYS:
                self.flgs = [sf, zf, of, cf]

            debug('%s: %s %s' % (pcaddr, OP_NAMES[op], dbg))

        return True

    def run(self):
        try:
            while self.spin():
                pass
        except AssertionError as e:
            raise
            error(e.message)

if __name__ == '__main__':
    # ft = subprocess.check_output(['file', '-bL', sys.executable])
    # if not ft.startswith('ELF 32-bit'):
    #     error('Only 32bit supported')

    p = argparse.ArgumentParser(
        description = \
        'Reference emulator implementation.',
    )

    p.add_argument(
        'exe',
        metavar='<exe>',
        type=argparse.FileType('rb'),
        default=sys.stdin,
        nargs='?',
    )

    g = p.add_mutually_exclusive_group()

    g.add_argument(
        '--verbose', '-v',
        action='count',
        default=0,
        help='Increase verbosity.',
    )

    g.add_argument(
        '--quiet', '-q',
        action='count',
        default=0,
        help='Decrease verbosity.',
    )

    args = p.parse_args()
    set_log_level(args.verbose - args.quiet + LOG_WARNING)
    VM(Exe(args.exe)).run()
