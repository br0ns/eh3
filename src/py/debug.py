from consts import *

def mem_find(addr, numb):
    for m in maps:
        if m['addr'] <= addr and m['addr'] + len(m['data']) >= addr + numb:
            return m

def mem_read(addr, numb):
    m = mem_find(addr, numb)
    if m:
        i = addr - m['addr']
        return m['data'][i : i + numb]

def mem_write(addr, data):
    numb = len(data)
    m = mem_find(addr, numb)
    if m:
        i = addr - m['addr']
        m['data'][i : i + numb] = bytearray(data)
        return True
    return False

def bstoi(bs):
    n = 0
    for b in bs[::-1]:
        n <<= 8
        n |= b
    return n

def itobs(n, nb):
    bs = []
    for _ in xrange(nb):
        bs.append(n & 0xff)
        n >>= 8
    return bytearray(bs)

def mem_read_bits(bitaddr, nbits):
    addr = bitaddr / 8
    offset = bitaddr % 8
    numb = (offset + nbits + 7) / 8
    data = mem_read(addr, numb)
    if data is None:
        return
    data = bstoi(data)
    data >>= offset
    data &= (1 << nbits) - 1
    return data

def mem_write_bits(bitaddr, n, nbits):
    addr = bitaddr / 8
    offset = bitaddr % 8
    numb = (offset + nbits + 7) / 8
    data = mem_read(addr, numb)
    if data is None:
        return
    data = bstoi(data)
    mask = (1 << nbits) - 1
    data &= ~(mask << offset)
    data |= (n & mask) << offset
    data = itobs(data, numb)
    return mem_write(addr, data)

breakpoints = []

BRK = OP_ALT | (ALT_BRK << OP_SZ)
def brkp(addr):
    breakpoints.append((addr, mem_read_bits(addr, 6)))
    mem_write_bits(addr, BRKP, 6)

def setpc(byteaddr, bitoffset=0):
    global pc
    pc = byteaddr * 8 + bitoffset

def debug_init():
    pass

def debug_hook():
    # brkp(0x008000d0 * 8)
    # mem_write_bits(0x800001 * 8 + 4, 2, 2)
    # print mem_read(0x800001, 3)
    regs[REG_A1] = 42
    setpc(0x0080013a)
    # return True
    pass

def debug_finit():
    pass
