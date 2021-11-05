#!/usr/bin/env python2
import sys
import os
import argparse

# TODO: Port these to support pypy
from pwnlib.util.fiddling import hexdump
from elftools.elf.elffile import *
from elftools.elf.constants import *

ELF_HDR = '\x7fELF'

from consts import *
from exe import *
from obj import *

MAX_BITS_PER_LINE = 52 # Enough for any `mov reg, imm`
SHOW_WAT = False
START_ADDR = None

def die(msg):
    print >>sys.stderr, msg
    sys.exit(1)

def tobits(xs):
    bs = []
    for x in xs:
        for _ in xrange(8):
            bs.append(x & 1)
            x >>= 1
    return bs

def frombits(bs):
    x = 0
    for b in bs[::-1]:
        x <<= 1
        x |= b
    return x

def show_data(data, symbols, vaddr=0):
    out = []
    i = 0
    for sym in sorted(symbols, key=lambda s: s.offset):
        j = sym.offset
        if sym.section == None:
            j -= vaddr
        if j < i:
            continue
        if j > len(data):
            break
        if i != j:
            out.append(hexdump(data[i:j], begin=vaddr + i))
            i = j
        out.append('%s:' % sym.name)
    out.append(hexdump(data[i:], begin=vaddr + i))
    return out

def disasm(code, relocs, symbols, vaddr=0):
    insts = {}
    code = tobits(code)
    seen = set()
    pc = [0]
    bits = []

    # avoid search through all relocs for every instruction
    next_reloc = [0]
    relocs = sorted(relocs, key=lambda r: r.offset)

    def symval(s):
        addr = s.offset
        if s.section != None: # SHN_ABS
            addr += vaddr
        return addr

    lbls = {symval(s) * 8: s.name for s in symbols}
    work = [addr - vaddr * 8 for addr in lbls]
    if START_ADDR != None:
        work.append(START_ADDR - vaddr * 8)

    def get(n):
        if pc[0] < 0 or pc[0] + n > len(code):
            raise EOFError
        bs = []
        for _ in xrange(n):
            if pc[0] in work:
                raise EOFError
            if pc[0] in seen:
                raise EOFError
            seen.add(pc[0])
            bs.append(code[pc[0]])
            pc[0] += 1
        bits.append(bs)
        return frombits(bs)

    def align():
        get(-pc[0] % 8)

    def get_ext():
        return EXT_NAMES[get(EXT_SZ)]

    def get_sz():
        return SZ_NAMES[get(SZ_SZ)]

    def get_sign():
        return SIGN_NAMES[get(SIGN_SZ)]

    def get_reg():
        return REG_NAMES[get(REG_SZ)]

    def get_imm(sign=SIGN_U):
        sz = get(SZ_SZ)
        sz = (sz + 1) * 8
        # Sign extend
        s = 1 << (sz - 1)
        n = get(sz)
        n = (s ^ n) - s
        return n

    def i2a(n):
        if n >= 0 and n <= 9:
            return str(n)
        else:
            return '%d/%#x' % (n, n & MASK_W)

    def get_oper(sign=SIGN_U):
        isimm = get(1)
        if isimm:
            return i2a(get_imm())
        else:
            return get_reg()

    def get_addr():
        out = '['
        base = get_reg()
        index = get_oper()
        if base != 'zz':
            out += base + ' : '
        out += index + ']'
        return out

    def get_jmp():
        isimm = get(1)
        if isimm:
            sz = get(SZ_SZ)
            size = (sz + 1) * 8
            sbit = 1 << (size - 1)
            isreloc = False
            for reloc in relocs:
                if reloc.offset == pc[0]:
                    isreloc = True
                    break
            imm = get(size)
            if imm & sbit:
                imm |= ~0 << size
            addr = vaddr * 8 + pc[0] + imm
            if not isreloc:
                lbl = lbls.setdefault(addr, 'loc_%#x.%d' % (addr / 8, addr % 8))
                work.append(pc[0] + imm)
                return '%s ; %s' % (i2a(imm), lbl)
            else:
                return '%s' % reloc.symbol
        else:
            return get_reg()

    def disasm1():
        # Reset bits; added to by `get`
        bits[::] = []
        cont = True
        args = None
        op = get(OP_SZ)

        if op == OP_ALT:
            op = ALT_NAMES[get(ALT_SZ)]
            if op == 'align':
                align()
            elif op == 'halt':
                cont = False

        elif op == OP_JCC:
            op = JCC_NAMES[get(JCC_SZ)]
            if op == 'jmp':
                cont = False
                jmp = get_jmp()
                # Pseudo
                if jmp == 'lr':
                    op = 'ret'
                else:
                    args = jmp

            elif op == 'jz':
                reg = get_reg()
                jmp = get_jmp()
                args = '%s, %s' % (reg, jmp)

            elif op in ('je', 'jne'):
                reg1 = get_reg()
                oper = get_oper()
                jmp = get_jmp()
                args = '%s, %s, %s' % (reg1, oper, jmp)
                # Pseudo
                if op == 'jne' and oper == 'zz':
                    op = 'jnz'
                    args = '%s, %s' % (reg1, jmp)

            elif op in ('jl', 'jle', 'jg', 'jge'):
                sign = get_sign()
                reg1 = get_reg()
                oper = get_oper()
                jmp = get_jmp()
                args = '%s, %s, %s' % (reg1, oper, jmp)
                # Pseudos
                if sign == 'unsigned':
                    op = op.replace('l', 'b').replace('g', 'a')

        else:
            op = OP_NAMES[op]

            if op == 'load':
                ext = get_ext()
                sz = get_sz()
                reg = get_reg()
                addr = get_addr()
                args = '%s %s %s, %s' % (ext, sz, reg, addr)

            elif op == 'store':
                sz = get_sz()
                addr = get_addr()
                oper = get_oper()
                args = '%s %s, %s' % (sz, addr, oper)

            elif op == 'push':
                oper = get_oper()
                args = oper

            elif op == 'pop':
                reg = get_reg()
                args = reg

            elif op in ('add', 'xor', 'and', 'or', 'shl', 'mul'):
                reg1 = get_reg()
                reg2 = get_reg()
                oper = get_oper()
                args = '%s, %s, %s' % (reg1, reg2, oper)
                # Pseudos
                if op == 'add':
                    if reg2 == 'zz':
                        op = 'mov'
                        args = '%s, %s' % (reg1, oper)
                    elif oper[0] == '-':
                        oper = i2a(int(oper[1:].split('/')[0]))
                        op = 'sub'
                        args = '%s, %s, %s' % (reg1, reg2, oper)
                elif op == 'xor' and oper == '0xffffffff':
                    op = 'not'
                    args = '%s, %s' % (reg1, reg2)

            elif op == 'sub':
                reg1 = get_reg()
                reg2 = get_reg()
                reg3 = get_reg()
                args = '%s, %s, %s' % (reg1, reg2, reg3)
                # Pseudo
                if reg2 == 'zz':
                    if reg3 == 'zz':
                        op = 'mov'
                        args = '%s, 0' % reg1
                    else:
                        op = 'neg'
                        args = '%s, %s' % (reg1, reg3)

            # elif op in ('mul', 'shr'):
            elif op == 'shr':
                sign = get_sign()
                reg1 = get_reg()
                reg2 = get_reg()
                oper = get_oper()
                args = '%s, %s, %s' % (reg1, reg2, oper)
                # Pseudo
                if sign == 'signed':
                    op = 'sar'

            elif op == 'divmod':
                sign = get_sign()
                reg1 = get_reg()
                reg2 = get_reg()
                reg3 = get_reg()
                oper = get_oper()
                args = '%s, %s, %s, %s' % (reg1, reg2, reg3, oper)
                # Pseudos
                if reg1 == 'zz':
                    op = 'mod'
                    args = '%s, %s, %s' % (reg2, reg3, oper)
                elif reg2 == 'zz':
                    op = 'div'
                    args = '%s, %s, %s' % (reg1, reg3, oper)
                if sign == 'signed':
                    op = 'i' + op

            elif op == 'call':
                jmp = get_jmp()
                args = jmp
                if pc[0] % 8:
                    align()
                    bits[-1] = ['|'] + bits[-1]

            else:
                pass
                # raise NotImplementedError(op)

        inst = op
        if args:
            inst += ' ' + args
        return cont, inst

    while work:
        pc[0] = work.pop(0)
        cont = True
        while cont:
            try:
                oldpc = pc[0]
                cont, inst = disasm1()
                insts[oldpc] = (pc[0], bits[::], inst)
            except EOFError:
                break

    # Format instructions
    def format1(beg, end, bits, inst):
        # Split raw bits over several lines if necessary
        bls = []
        # Convert bits to strings
        bits = map(lambda bs: ''.join(map(str, bs)), bits)
        while bits:
            bl = bits.pop(0)
            while bits:
                if len(bl) + len(bits[0]) + 1 <= MAX_BITS_PER_LINE:
                    bl += ' ' + bits.pop(0)
                    continue
                break
            bls.append(bl)

        # Annotate bits with reloc locations
        while next_reloc[0] < len(relocs):
            reloc = relocs[next_reloc[0]]
            if reloc.offset >= end:
                break
            next_reloc[0] += 1
            if reloc.offset < beg:
                continue

            addent = frombits(code[reloc.offset : reloc.offset + reloc.size])
            sbit = 1 << (reloc.size - 1)
            if addent & sbit:
                addent |= ~0 << reloc.size

            n = reloc.offset - beg
            for i, bl in enumerate(bls):
                for j, c in enumerate(bl):
                    if c != ' ':
                        if n == 0:
                            break
                        n -= 1
                else:
                    j = 0
                    i += 1
                if n == 0:
                    line = '%s^ %s%d %s' % (
                        ' ' * j,
                        RELOC_NAMES[reloc.type],
                        reloc.size,
                        reloc.symbol)
                    if addent:
                        if addent > 0:
                            line += ' + '
                        else:
                            line += ' - '
                            addent = -addent
                        line += '%d' % addent
                    bls.insert(i + 1, line)
                    break

        # Add lines to output
        lines.append((beg, bls[0], inst))
        for bl in bls[1:]:
            lines.append((None, bl, None))

    # Format instructions
    def watbits(beg, end):
        bits = []
        step = MAX_BITS_PER_LINE
        for i in xrange(beg, end, step):
            j = min(i + step, end)
            bits.append(code[i:j])
        if SHOW_WAT:
            format1(beg, end, bits, '<???>')
        else:
            format1(beg, end, ['(%d bits)' % (end - beg)], '<???>')
    lines = []
    prevend = end = 0
    for beg, (end, bits, inst) in sorted(insts.items()):
        if beg > prevend:
            watbits(prevend, beg)
        format1(beg, end, bits, inst)
        prevend = end
    if end < len(code):
        watbits(end, len(code))

    # End of section
    lines.append((pc[0], None, None))

    # Format output
    out = []
    for (addr, bits, inst) in lines:
        if addr != None and addr >= 0 and addr <= len(code):
            lbl = lbls.get(vaddr * 8 + addr)
            if lbl:
                out.append('%s:' % lbl)
            addr = '0x%08x.%d:' % (vaddr + addr / 8, addr % 8)

        else:
            addr = ' ' * 13

        bits = (bits or '').ljust(MAX_BITS_PER_LINE)
        inst = inst or ''
        out.append(' '.join((addr, bits, inst)))

    return out

def disasm_eh3_exe(exe):
    out = []
    for i, segment in enumerate(exe.segments):
        if out:
            out.append('')
        prot = prot2str(segment.prot)
        out.append('Segment %s 0x%08x--0x%08x' %
                   (prot, segment.addr, segment.addr + segment.size))

        relocs = []
        symbols = []

        if segment.prot & PROT_X:
            out += disasm(segment.data, relocs, symbols, vaddr=segment.addr)
        else:
            out += show_data(segment.data, symbols, vaddr=segment.addr)

    print '\n'.join(out)

def disasm_eh3_obj(obj):
    out = []
    for i, section in enumerate(obj.sections):
        if out:
            out.append('')
        prot = prot2str(section.prot)
        out.append('Section %s %s:' % (prot, section.name))

        def filt(xs):
            return sorted([x for x in xs if x.section in (i, None)],
                          key=lambda x: x.offset)
        relocs = filt(obj.relocations)
        symbols = filt(obj.symbols)

        if section.name == '.text':
            out += disasm(section.data, relocs, symbols)
        else:
            out += show_data(section.data, symbols)

    print '\n'.join(out)

def disasm_eh3(fd):
    fd.seek(len(EH3_HDR), os.SEEK_SET)
    t = ord(fd.read(1))
    fd.seek(0, os.SEEK_SET)
    if t == EH3_OBJ:
        disasm_eh3_obj(Obj(fd))
    elif t == EH3_EXE:
        disasm_eh3_exe(Exe(fd))
    else:
        die('Unknown EH3 type')

def disasm_elf(fd, use_phdrs=False):
    elf = ELFFile(fd)
    out = []
    relocs = []
    symbols = []
    shstrtab = elf.get_section(elf.header['e_shstrndx'])
    def str(strtab, offset):
        # Undo elftool's decoding
        return strtab.get_string(offset).encode('utf8')

    # Collect relocations and symbols
    for section in elf.iter_sections():
        if isinstance(section, RelocationSection):
            relsec = section['sh_info']
            symtab = elf.get_section(section['sh_link'])
            for reloc in section.iter_relocations():
                strtab = elf.get_section(symtab['sh_link'])
                r = Relocation()
                r.section = section['sh_info']
                symbol = symtab.get_symbol(reloc['r_info_sym'])
                r.symbol = str(strtab, symbol['st_name'])
                r.offset = reloc['r_offset']
                r.type = reloc['r_info_type'] - 1
                r.size = 32
                relocs.append(r)

        elif isinstance(section, SymbolTableSection):
            strtab = elf.get_section(section['sh_link'])
            for symbol in section.iter_symbols():
                s = Symbol()
                s.name = str(strtab, symbol['st_name'])
                s.section = symbol['st_shndx']
                if s.section == 'SHN_UNDEF':
                    continue
                if s.section == 'SHN_ABS':
                    s.section = None
                s.offset = symbol['st_value']
                s.bind = {
                    'STB_GLOBAL': BIND_GLOBAL,
                    'STB_WEAK': BIND_WEAK,
                    'STB_LOCAL': BIND_LOCAL,
                }[symbol['st_info']['bind']]
                symbols.append(s)

    if use_phdrs:
        for s in symbols:
            if s.name == '_start':
                break
        else:
            s = Symbol()
            s.name = '_start'
            s.section = None
            s.offset = elf['e_entry']
            s.bind = BIND_GLOBAL
            symbols.append(s)

        for segment in elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD':
                continue

            if out:
                out.append('')

            vaddr = segment['p_vaddr']
            memsz = segment['p_memsz']
            flags = segment['p_flags']
            data = bytearray(segment.data().ljust(memsz, '\0'))

            def filt(xs):
                return sorted([x for x in xs if x.section == None],
                              key=lambda x: x.offset)
            rels = []
            syms = filt(symbols)

            out.append('Segment 0x%08x--0x%08x' % (vaddr, vaddr + memsz))

            if flags & P_FLAGS.PF_X:
                out += disasm(data, rels, syms, vaddr=vaddr)
            else:
                out += show_data(data, syms, vaddr=vaddr)


    else:
        for i, section in enumerate(elf.iter_sections()):
            if section['sh_type'] != 'SHT_PROGBITS':
                continue

            def filt(xs):
                return sorted([x for x in xs if x.section in (i, None)],
                              key=lambda x: x.offset)
            rels = filt(relocs)
            syms = filt(symbols)
            data = bytearray(section.data())

            if out:
                out.append('')
            out.append('Section %s:' % str(shstrtab, section['sh_name']))

            if section['sh_flags'] & SH_FLAGS.SHF_EXECINSTR:
                out += disasm(data, rels, syms)
            else:
                out += show_data(data, syms)

    print '\n'.join(out)

if __name__ == '__main__':
    p = argparse.ArgumentParser()

    p.add_argument(
        'files',
        metavar='<file.asm>',
        type=argparse.FileType('rb'),
        nargs='+',
    )

    p.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output.',
    )

    p.add_argument(
        '--show-wat', '-w',
        action='store_true',
        help='Also show bits that wasn\'t disassembled.',
    )

    p.add_argument(
        '--start', '-s',
        help='Start disassembling from this (virtual) address.',
    )

    p.add_argument(
        '--phdrs', '-p',
        action='store_true',
        help='Use program headers (instead of section headers).  Only affects '
        'ELF files.',
    )

    args = p.parse_args()
    SHOW_WAT = args.show_wat
    if args.start:
        try:
            START_ADDR = int(args.start)
        except:
            try:
                START_ADDR = int(args.start, 16)
            except:
                die('Invalid start address')

    for fd in args.files:
        hdr = fd.read(4)
        fd.seek(0, os.SEEK_SET)
        if len(args.files) > 1:
            print '[%s]' % fd.name
        if hdr == EH3_HDR:
            disasm_eh3(fd)
        elif hdr == ELF_HDR:
            disasm_elf(fd, use_phdrs=args.phdrs)
        else:
            die('Unknown file type')
