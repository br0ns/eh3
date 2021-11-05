# coding: utf-8

from functools import partial

def readn(fd, n):
    s = []
    while n:
        r = fd.read(n)
        if not r:
            raise EOFError
        s.append(r)
        n -= len(r)
    return ''.join(s)

def read1(fd):
    c = fd.read(1)
    if not c:
        raise EOFError
    return c

# Signed LEB128
def loadint(fd):
    n = 0
    x = 0
    while True:
        b = ord(read1(fd))
        x |= (b & 0x7f) << n
        n += 7
        if not (b & 0x80):
            break
    # Sign extend
    if b & 0x40:
        x |= ~0 << n
    return x

def dumpint(fd, x):
    while True:
        b = x & 0x7f
        x >>= 7
        s = b & 0x40 # sign bit written
        # Only sign bits remain, and at least one sign bit has been written
        if (x == 0 and not s) or (x == -1 and s):
            fd.write(chr(b))
            break
        fd.write(chr(b | 0x80))

# Null-terminated UTF-8 strings
def loadstr(fd):
    s = ''
    while True:
        c = read1(fd)
        if c == '\0':
            break
        else:
            s += c
    return s

def dumpstr(fd, s):
    if isinstance(s, unicode):
        s = s.encode('utf8')
    fd.write(s + '\0')

# Byte arrays
def loadbytearray(fd):
    n = loadint(fd)
    return bytearray(fd.read(n))

def dumpbytearray(fd, bs):
    dumpint(fd, len(bs))
    fd.write(bs)

# Create loader/dumper functions
def mkobject(cls):
    def load(fd):
        obj = cls()
        obj.load(fd)
        return obj
    def dump(fd, obj):
        obj.dump(fd)
    return load, dump

def mklist((l, d)):
    def load(fd):
        return [l(fd) for _ in xrange(loadint(fd))]
    def dump(fd, xs):
        dumpint(fd, len(xs))
        for x in xs:
            d(fd, x)
    return load, dump

def mktuple(lds):
    def load(fd):
        return tuple(l(fd) for l, _ in lds)
    def dump(fd, xs):
        for (_, d), x in zip(lds, xs):
            d(fd, x)
    return load, dump

def mkconst(c):
    def load(fd):
        if not readn(fd, len(c)) == c:
            raise ValueError('expected %r' % c)
        return c
    def dump(fd, _):
        fd.write(c)
    return load, dump

def mk(desc):
    if isinstance(desc, list):
        if len(desc) != 1:
            raise TypeError('expected singleton list: %r' % desc)
        return mklist(mk(desc[0]))
    elif isinstance(desc, tuple):
        return mktuple(map(mk, desc))
    elif isinstance(desc, str):
        return mkconst(desc)
    elif isinstance(desc, type):
        if issubclass(desc, (int, long)):
            return loadint, dumpint
        elif issubclass(desc, (str, unicode)):
            return loadstr, dumpstr
        elif issubclass(desc, bytearray):
            return loadbytearray, dumpbytearray
        elif issubclass(desc, Serialize):
            return mkobject(desc)

    raise TypeError('cannot serialize type: %r' % desc)

class Serialize(object):
    def __new__(cls, *args, **kwargs):
        # Build loaders/dumpers
        lds = []
        for name, desc in cls.__fields__:
            if not isinstance(desc, (list, tuple, type)):
                setattr(cls, name, desc)
            lds.append((name, mk(desc)))
        cls.__fields__ = lds
        # Don't call this method on object creation from now on
        cls.__new__ = object.__new__
        # Create and return new object
        return object.__new__(cls, *args, **kwargs)
    def __init__(self, fd=None):
        if fd:
            self.load(fd)
    def load(self, fd):
        for name, (l, _) in self.__fields__:
            setattr(self, name, l(fd))
    def dump(self, fd):
        for name, (_, d) in self.__fields__:
            d(fd, getattr(self, name))

if __name__ == '__main__':
    import sys
    from StringIO import StringIO
    from pwnlib.util.fiddling import hexdump

    def test(desc, val):
        fd = StringIO()

        load, dump = mk(desc)

        print 'DESC = %s' % (desc,)
        print 'VAL  = %r' % (val,)

        dump(fd, val)

        fd.seek(0)
        s = fd.read()
        print 'Serialized:'
        print hexdump(s)

        fd.seek(0)
        try:
            val_ = load(fd)
        except:
            val_ = None

        if val != val_:
            print 'ERROR: %r' % (val_,)
        else:
            print 'OK'

        print

    tests = (
        (int, 0x42),
        (str, 'Hello, world!'),
        (str, ''),
        (str, 'æøå'),
        (bytearray, bytearray()),
        (bytearray, bytearray('hello')),
        (bytearray, bytearray('æøå')),
        ('const', None),
        ([int],  []),
        ([int],  [42]),
        ([int],  [42, 0, 0, 0, 0, -43]),
        ((int, int, str),
                   (1, 2, 'hello')),
        ([(int, int)],
                   [(1, 2), (3, 4)]),
        (([int], str),
                   ([1, 2, 3], 'hello')),
    )

    for desc, val in tests:
        test(desc, val)

    class Foo(Serialize):
        __fields__ = {
            'bar': int,
            'baz': ([int], str),
        }

    class Bar(Serialize):
        __fields__ = {
            'foos': [Foo],
        }

    foo1 = Foo()
    foo1.bar = 42
    foo1.baz = ([1,2,3], '123')
    foo2 = Foo()
    foo2.bar = 0
    foo2.baz = ([-1, -9999], '')
    bar = Bar()
    bar.foos = [foo1, foo2]

    fd = StringIO()
    bar.dump(fd)
    fd.seek(0)
    bar_ = Bar(fd)
    print len(bar_.foos)
    print bar_.foos[0].bar
    print bar_.foos[0].baz
    print bar_.foos[1].bar
    print bar_.foos[1].baz
