import sys
import string
import re
import ply.lex as lex

from err import show_error
from consts import *

OP    = map(string.upper, OP_NAMES)
OP   += map(string.upper, JCC_NAMES)
OP   += map(string.upper, ALT_NAMES)
OP   += map(string.upper, OP_PSEUDO)
SZ    = map(string.upper, SZ_NAMES)
EXT   = map(string.upper, EXT_NAMES)
SIGN  = map(string.upper, SIGN_NAMES)
REG   = map(string.upper, REG_NAMES)

SIZE_SUFFIXES = ('BIT', 'B', 'H', 'W')

keywords = [
    'GLOBAL',
    'EXTERN',
    'SECTION',
    'SIZEOF',
    'END',
    'FORMAT',
]

tokens = [
    'ID',
    'NUMBER',
    'STRING',
    'COLON',
    'COMMA',
    'LBRACK',
    'RBRACK',

    # Handled in `t_ID`
    'SZ',
    'EXT',
    'SIGN',
    'REG',
    'DATA',
    'ALLOC',
    'ALIGN',

    # Expressions
    'PLUS',
    'MINUS',
    'TIMES',
    'DIVIDE',
    'MODULUS',
    'POWER',
    'AND',
    'OR',
    'XOR',
    'NOT',
    'LSHIFT',
    'RSHIFT',
    'LPAREN',
    'RPAREN',
] \
+ keywords \
+ ['OP_%s' % op for op in set(OP)] # "jmp" both in `OP_NAMES` and `JCC_NAMES`.


t_ignore  = ' \t\n\r'

ID = r'\$|\.*[a-z_][._a-z0-9]*'
STRING = r'"([^\\]|(\\.))*?"'
NUMBER_bin = r'0b[01]+'
NUMBER_oct = r'0[0-7]+'
NUMBER_dec = r'[0-9]+'
NUMBER_hex = r'0x[0-9a-f]+'
NUMBER_lit = r"'([^\\]|(\\.))*?'"
NUMBER = '|'.join('(%s)' % r for r in \
                  (NUMBER_bin,
                   NUMBER_oct,
                   NUMBER_hex,
                   NUMBER_dec,
                   NUMBER_lit,
                  ))
t_COLON = r':'
t_COMMA = r','
t_LBRACK = r'\['
t_RBRACK = r'\]'

t_PLUS = r'\+'
t_MINUS = r'-'
t_TIMES = r'\*'
t_DIVIDE = r'/'
t_MODULUS = r'%'
t_POWER = r'\*\*'
t_AND = r'&'
t_OR = r'\|'
t_XOR = r'\^'
t_NOT = r'~'
t_LSHIFT = r'<<'
t_RSHIFT = r'>>'
t_LPAREN = r'\('
t_RPAREN = r'\)'

def error(t, msg, eend=None):
    show_error(t.lexer.lexdata, (t.lexpos, getattr(t, 'endlexpos', None)), msg)
    t.lexer.skip(1)
    # sys.exit(1)

def evalstr(t):
    try:
        t.endlexpos = t.lexpos + len(t.value)
        t.value = t.value.decode('string_escape')[1:-1]
    except ValueError as e:
        msg = e.message
        msg = msg[0].upper() + msg[1:]
        error(t, msg, t.lexpos + len(t.value))


@lex.TOKEN(ID)
def t_ID(t):
    t.endlexpos = t.lexpos + len(t.value)
    v = t.value.upper()

    def with_size(s):
        return v.startswith(s) and v[len(s):] in SIZE_SUFFIXES

    if v in keywords:
        t.type = v

    elif v in OP:
        t.type = 'OP_%s' % v

    elif v in SZ:
        t.type = 'SZ'

    elif v in EXT:
        t.type = 'EXT'

    elif v in SIGN:
        t.type = 'SIGN'

    elif v in REG:
        t.type = 'REG'

    elif with_size('D'):
        t.type = 'DATA'

    elif with_size('RES'):
        t.type = 'ALLOC'

    elif with_size('ALIGN'):
        t.type = 'ALIGN'

    return t

@lex.TOKEN(STRING)
def t_STRING(t):
    evalstr(t)
    return t

@lex.TOKEN(NUMBER)
def t_NUMBER(t):
    t.endlexpos = t.lexpos + len(t.value)
    if t.value[0] == "'":
        evalstr(t)
        v = 0
        for c in t.value:
            v <<= 8
            v |= ord(c)
        t.value = v
    else:
        base = 10
        if len(t.value) >= 2 and t.value[0] == '0':
            if t.value[1] in 'xX':
                base = 16
            elif t.value[1] in 'bB':
                base = 2
            elif all(d in '01234567' for d in t.value):
                base = 8
        t.value = int(t.value, base)
    return t

def t_COMMENT(t):
    r'[;#].*'
    # No return value. Token discarded
    pass

def t_error(t):
    error(t, 'Illegal character')

lexer = lex.lex(reflags=re.IGNORECASE | re.VERBOSE)
