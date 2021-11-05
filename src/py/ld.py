#!/usr/bin/env python2
import sys
import argparse

from obj import *
from exe import *
from consts import PAGE_SIZE, PAGE_MASK

BASE_ADDR = 0x80000000

def die(s):
    print >>sys.stderr, s
    sys.exit(1)

def ld(objs):

    def lookup(name, object=None):
        weak = None
        for symbol in obj.symbols:
            if symbol.name != name:
                continue
            if symbol.bind == BIND_LOCAL and symbol.object == object:
                return symbol
            if symbol.bind == BIND_GLOBAL:
                return symbol
            if symbol.bind == BIND_WEAK:
                weak = symbol
        return weak

    # Merge objects
    obj = Obj()
    for o in objs:
        for reloc in o.relocations:
            reloc.section += len(obj.sections)
            reloc.object = o
            obj.relocations.append(reloc)

        for symbol in o.symbols:
            symbol.section += len(obj.sections)
            symbol.object = o
            if lookup(symbol.name, o):
                die('Symbol redefined: %s' % symbol.name)
            obj.symbols.append(symbol)

        obj.sections += o.sections

    # Place sections
    next_addr = [BASE_ADDR]
    prot = [None]
    def place(collect=None):
        for i, section in enumerate(obj.sections):
            if hasattr(section, 'addr'):
                continue
            if collect and collect != section.name:
                continue
            addr = next_addr[0]
            if section.prot != prot[0]:
                addr = (addr + PAGE_SIZE - 1) & ~PAGE_MASK
                prot[0] = section.prot
            section.addr = addr
            next_addr[0] = addr + section.size

    place('.text')
    place('.data')
    place('.bss')
    place() # rest, if any

    # Resolve relocations
    for reloc in obj.relocations:
        symbol = lookup(reloc.symbol, reloc.object)
        if not symbol:
            die('Undefined symbol: %s' % reloc.symbol)
        data = obj.sections[reloc.section].data
        if reloc.type == RELOC_ABS:
            val = obj.sections[symbol.section].addr + symbol.offset
        elif reloc.type == RELOC_REL:
            val = (obj.sections[symbol.section].addr -
                   obj.sections[reloc.section].addr +
                   symbol.offset) * 8 - reloc.offset
        else:
            die('invalid relocation type: %d' % reloc.type)

        # Relocation offsets and sizes are in bits
        beg, begbit = divmod(reloc.offset, 8)
        end, endbit = divmod(reloc.offset + reloc.size, 8)

        # Shift zero's into the lower bits of the lower byte, to avoid change
        val <<= begbit
        carry = 0
        for i in xrange(beg, end):
            x = (val & 0xff) + data[i] + carry
            carry = x >> 8
            val >>= 8
            x &= 0xff
            data[i] = x

        i += 1
        if endbit:
            mask = ~(~0 << endbit)
            x = (val & mask) + (data[i] & mask) + carry
            carry = x >> endbit
            val >>= endbit
            # Copy upper bits
            x |= data[i] & ~mask
            data[i] = x

        if val or carry:
            print >>sys.stderr, \
                '!! Relocated value for symbol "%s" does not fit in %d bits ' \
                'at %s:%s:%#x' % (
                    reloc.symbol,
                    reloc.size,
                    reloc.object.file,
                    o.sections[reloc.section].name,
                    reloc.offset)

    # Create EXE
    exe = Exe()
    def key(x):
        k = x.addr
        # Put .bss at the end
        if x.size != len(x.data):
            k += MEM_SIZE
        return k
    for section in sorted(obj.sections, key=key):
        segment = Segment()
        segment.addr = section.addr
        segment.size = section.size
        segment.prot = section.prot
        segment.data = section.data
        exe.segments.append(segment)

    # Merge segments
    i = 1
    while i < len(exe.segments):
        prev, this = exe.segments[i - 1: i + 1]
        if all((prev.addr + prev.size == this.addr,
                prev.size == len(prev.data),
                prev.prot == this.prot)):
            prev.size += this.size
            prev.data += this.data
            exe.segments.pop(i)
        else:
            # Maybe align up to next page boundary
            this.addr = max(
                this.addr, (prev.addr + prev.size + PAGE_MASK) & ~PAGE_MASK)
            i += 1

    # Entry point
    start = lookup('_start')
    if not start:
        die('Undefined symbol: _start')
    exe.entry = obj.sections[start.section].addr + start.offset

    return exe

if __name__ == '__main__':
    p = argparse.ArgumentParser(
        description = \
        'Create executable from object files.',
    )

    p.add_argument(
        'files',
        metavar='<file>',
        type=argparse.FileType('r+b'),
        nargs='+',
    )

    p.add_argument(
        '--outfile','-o',
        metavar='<file>',
        type=argparse.FileType('wb'),
        default=file('a.out', 'wb'),
    )

    args = p.parse_args()
    objs = []
    for f in args.files:
        o = Obj(f)
        o.file = f.name
        objs.append(o)
    exe = ld(objs)
    exe.dump(args.outfile)
