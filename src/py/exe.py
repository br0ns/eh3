import struct

from consts import EH3_HDR, EH3_EXE
from serialize import Serialize

class Segment(Serialize):
    __fields__ = [
        ('addr', int), # Byte
        ('size', int), # Byte
        ('prot', int),
        ('data', bytearray),
    ]
    def __str__(self):
        return 'SEGMENT(addr=%#x, size=%#x, len(data)=%#x))' % \
            (self.addr,
             self.size,
             len(self.data),
            )

class Exe(Serialize):
    __fields__ = [
        ('hdr',         EH3_HDR),
        ('type',        chr(EH3_EXE)),
        ('entry',       int), # Byte
        ('segments',    [Segment]),
    ]
    hdr = None
    def __init__(self, fd=None):
        self.entry = 0
        self.segments = []
        super(Exe, self).__init__(fd)

    def __str__(self):
        s = 'EXE(\n'
        s += '  SEGMENTS=[\n'
        for segment in self.segments:
            s += '    ' + str(segment) + ',\n'
        s += '  ],\n'
        s += '  ENTRY=%#x,\n' % self.entry
        s += ')'
        return s
