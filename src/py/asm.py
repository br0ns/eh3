#!/usr/bin/env python2
import os
import sys
import argparse
import atexit
import subprocess

from asm.lex import lexer
from asm.yacc import parse
from asm.codegen import codegen
from asm.elf import outelf
from consts import EH3_HDR

VERBOSE = False
INCLUDE = os.path.dirname(os.path.realpath(__file__)) + '/asm/include/'

LD_EH3 = os.path.dirname(__file__) + '/eld'
LD_ELF = os.path.dirname(__file__) + '/nld'
DEFAULT_FMT = 'eh3'

def die(msg):
    print >>sys.stderr, '\x1b[1;31m%s\x1b[m' % msg
    sys.exit(1)

def log(msg):
    print >>sys.stderr, '\x1b[32m%s\x1b[m' % msg

def debug(msg):
    if VERBOSE:
        print >>sys.stderr, '\x1b[33m%s\x1b[m' % msg

def unlink(path):
    debug('+ rm -f %s' % Spath)
    try:
        os.unlink(path)
    except:
        pass

def asm(ipath, Spath, opath, include):
    debug('%s -> %s' % (ipath, opath))
    inc = ' '.join('-I "%s"' % p for p in include + [INCLUDE])
    ret = os.system('cpp -Werror -E -nostdinc %s "%s" -o "%s"' %
                    (inc, ipath, Spath))
    if ret:
        sys.exit(ret)

    input = file(Spath, 'rb').read()

    return codegen(input)

if __name__ == '__main__':
    p = argparse.ArgumentParser()

    p.add_argument(
        '--keep-S',
        action='store_true',
        help='Keep .S files',
    )

    p.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output',
    )

    p.add_argument(
        'files',
        metavar='<file.asm>',
        nargs='+',
    )

    p.add_argument(
        '-l',
        action='store_true',
        help='Link.',
    )

    p.add_argument(
        '--format', '-f',
        choices=('eh3', 'elf'),
        help='Output format.',
    )

    p.add_argument(
        '-o', '--outfile',
        help='Name of the output file.  Has not effect when -c is given.',
    )

    p.add_argument(
        '-I',
        metavar='<dir>',
        help='Add directory to search path.',
        nargs='*',
        default=[],
    )

    args = p.parse_args()
    opaths = []
    Spaths = []

    if not args.keep_S:
        atexit.register(lambda: map(unlink, Spaths))

    for ipath in args.files:
        ipath = os.path.relpath(ipath, os.getcwd())
        Spath = os.path.splitext(ipath)[0] + '.S'
        Spaths.append(Spath)
        opath = os.path.splitext(ipath)[0] + '.o'
        opaths.append(opath)
        fmt, obj = asm(ipath, Spath, opath, args.I)
        ofd = file(opath, 'wb')

        fmt = args.format or fmt or DEFAULT_FMT
        if fmt == 'eh3':
            obj.dump(ofd)
        else:
            outelf(ofd, obj)

        ofd.close()

    if args.l:
        LD = {'eh3': LD_EH3, 'elf': LD_ELF}[args.format]
        if not os.path.exists(LD):
            die('Linker is missing: %s' % LD)
        argv = [LD] + map(os.path.realpath, opaths)
        if args.outfile:
            argv += ['-o', args.outfile]
        subprocess.check_call(argv)
