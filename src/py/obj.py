import struct

from consts import EH3_HDR, EH3_OBJ
from serialize import Serialize

RELOC_ABS, \
RELOC_REL, \
= range(2)

RELOC_NAMES = {
    RELOC_ABS: 'ABS',
    RELOC_REL: 'REL',
}

BIND_GLOBAL, \
BIND_WEAK,   \
BIND_LOCAL,  \
= range(3)

BIND_NAMES = {
    BIND_GLOBAL: 'GLOBAL',
    BIND_WEAK:   'WEAK',
    BIND_LOCAL:  'LOCAL',
}

### Relocations
## Location
# The location of a relocation is given as the index of its section and the bit
# offset within that section.
#
## Addend
# The addend is implicit and stored in its location.  The result of relocation
# is given below, where `S` is the final address of the symbol, `A` is the
# addend and `P` is the final address of the relocation.
#
## Type
# REL: S + A - P (bits)
# ABS: S + A     (bytes)
#
## Example
# The instruction `call word foo` would result in a relocation for the address
# of `foo` with type `REL` and addend -32.

class Relocation(Serialize):
    __fields__ = [
        ('section',  int),
        ('offset',   int), # Bits
        ('size',     int), # Bits
        ('type',     int),
        ('symbol',   str),
    ]
    def __str__(self):
        return 'RELOC(section=%d, offset=%#x, size=%d, type=%s, symbol=%s)' % \
            (self.section,
             self.offset,
             self.size,
             RELOC_NAMES[self.type],
             self.symbol,
            )

class Symbol(Serialize):
    __fields__ = [
        ('name',     str),
        ('section',  int),
        ('offset',   int), # Bytes
        ('bind',     int),
        ('size',     int), # Bits
    ]
    size = 0
    def __str__(self):
        return 'SYMBOL(name=%s, section=%s, offset=%#x, bind=%s, size=%d)' % \
            (self.name,
             'ABS' if self.section == None else str(self.section),
             self.offset,
             BIND_NAMES[self.bind],
             self.size,
            )

class Section(Serialize):
    __fields__ = [
        ('name',     str),
        ('size',     int), # Bytes
        ('prot',     int),
        ('data',     bytearray),
    ]
    data = bytearray()
    def __str__(self):
        return 'SECTION(name=%s, size=%#x, len(data)=%#x))' % \
            (self.name,
             self.size,
             len(self.data),
            )

class Obj(Serialize):
    __fields__ = [
        ('hdr',         EH3_HDR),
        ('type',        chr(EH3_OBJ)),
        ('sections',    [Section]),
        ('symbols',     [Symbol]),
        ('relocations', [Relocation]),
    ]
    hdr = None
    type = None

    def __init__(self, fd=None):
        self.sections    = []
        self.symbols     = []
        self.relocations = []
        super(Obj, self).__init__(fd)

    def __str__(self):
        s = 'OBJECT(\n'
        s += '  RELOCS=[\n'
        for reloc in self.relocations:
            s += '    ' + str(reloc) + ',\n'
        s += '  ],\n'
        s += '  SYMBOLS=[\n'
        for symbol in self.symbols:
            s += '    ' + str(symbol) + ',\n'
        s += '  ],\n'
        s += '  SECTIONS=[\n'
        for section in self.sections:
            s += '    ' + str(section) + ',\n'
        s += '  ],\n'
        s += ')'
        return s
