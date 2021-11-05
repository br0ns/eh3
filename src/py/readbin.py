#!/usr/bin/env python2
import os
import sys
import argparse

from exe import *
from obj import *

if __name__ == '__main__':
    p = argparse.ArgumentParser(
        description = \
        'Display information about executable and object files.',
    )

    p.add_argument(
        'file',
        metavar='<file>',
        type=argparse.FileType('r+b'),
    )

    args = p.parse_args()
    fd = args.file
    hdr = fd.read(4)
    if hdr == EH3_HDR:
        t = ord(fd.read(1))
        fd.seek(0, os.SEEK_SET)
        if t == EH3_EXE:
            print Exe(fd)
        elif t == EH3_OBJ:
            print Obj(fd)
        else:
            print >>sys.stderr, 'Bad EH3 file type'
            exit(1)
    else:
        print >>sys.stderr, 'Unknown file type'
        exit(1)
