import string
import sys

import ply.yacc as yacc

from err import show_error
from .lex import tokens
from .data import *

debug = False
track = False

class State: pass
state = State()
state.section = None
state.symbol = None
state.input = None

def set_parent(ident):
    if ident.is_local():
        if state.symbol:
            ident.parent = state.symbol.value
        else:
            error(ident.lexspan,
                  'Local label "%s" has no parent label' % ident.value)
    elif isinstance(ident, Label):
        state.symbol = ident

def error(span, msg):
    show_error(state.input, span, msg)
    sys.exit(1)

start = 'stmts'

# TODO: get these (or at least opcodes) from lexer
def p_ident(p):
    '''ident : GLOBAL
             | EXTERN
             | SECTION
             | END
             | SIZEOF
             | FORMAT

             | SZ
             | EXT
             | SIGN
             | DATA
             | ALLOC
             | ID

             | OP_LOAD
             | OP_STORE
             | OP_PUSH
             | OP_POP
             | OP_ADD
             | OP_SUB
             | OP_MUL
             | OP_DIVMOD
             | OP_XOR
             | OP_AND
             | OP_OR
             | OP_SHL
             | OP_SHR
             | OP_CALL

             | OP_JMP
             | OP_JZ
             | OP_JE
             | OP_JNE
             | OP_JL
             | OP_JLE
             | OP_JG
             | OP_JGE

             | OP_SYSCALL
             | OP_HALT
             | OP_ALIGN
             | OP_BRK

             | OP_MOV
             | OP_DIV
             | OP_MOD
             | OP_NEG
             | OP_NOT
             | OP_NOP
             | OP_RET'''
    p[0] = Ident(p[1])
    p[0].lexspan = p.lexspan(1)

def p_reg(p):
    'reg : REG'
    p[0] = Reg(p[1].lower())
    p[0].lexspan = p.lexspan(1)

precedence = (
    ('left', 'OR'),
    ('left', 'XOR'),
    ('left', 'AND'),
    ('left', 'LSHIFT', 'RSHIFT'),
    ('left', 'PLUS', 'MINUS'),
    ('left', 'TIMES', 'DIVIDE', 'MODULUS'),
    ('right', 'POWER'),
    ('right', 'UMINUS', 'NOT'),
)

def p_expr_binop(p):
    '''expr : expr PLUS expr
            | expr MINUS expr
            | expr TIMES expr
            | expr DIVIDE expr
            | expr MODULUS expr
            | expr POWER expr
            | expr AND expr
            | expr OR expr
            | expr XOR expr
            | expr LSHIFT expr
            | expr RSHIFT expr'''
    p[0] = BinOp(p[2], p[1], p[3])
    p[0].lexspan = p.lexspan(2)

def p_expr_unop(p):
    '''expr : MINUS expr %prec UMINUS
            | NOT expr'''
    p[0] = UnOp(p[1], p[2])
    p[0].lexspan = p.lexspan(1)

def p_expr_sizeof(p):
    'expr : SIZEOF LPAREN ident RPAREN'
    p[0] = SizeOf(p[3])
    p[0].lexspan = p.lexspan(1)

def p_expr_var(p):
    'expr : ident'
    p[0] = Var(p[1])
    set_parent(p[0])

def p_expr_number(p):
    'expr : NUMBER'
    p[0] = Number(p[1])
    p[0].lexspan = p.lexspan(1)

def p_expr_parens(p):
    'expr : LPAREN expr RPAREN'
    p[0] = p[2]
    p[0].lexspan = (p.lexspan(1)[0], p.lexspan(3)[1])

def p_addr(p):
    'addr : LBRACK addr_inner RBRACK'
    p[0] = p[2]

def p_addr_inner(p):
    '''addr_inner : reg COLON oper
                  | oper'''
    if len(p) == 4:
        base = p[1]
        index = p[3]
    else:
        base = None
        index = p[1]
    p[0] = (base, index)

def p_jmp(p):
    '''jmp : reg
           | expr'''
    p[0] = p[1]

def p_oper(p):
    '''oper : reg
            | expr'''
    p[0] = p[1]

def p_extsz(p):
    '''extsz : SZ
             | EXT SZ'''
    if len(p) == 2:
        ext = 'zx'
        sz = p[1]
        # if sz != 'word':
        #     error(p.lexpan(1),
        #         'Expected zx/sx before size specifier')
    else:
        ext, sz = p[1:]
    p[0] = (ext, sz)

# NB: Add imul back in?
def p_inst(p):
    '''inst : OP_LOAD extsz reg COMMA addr
            | OP_STORE SZ addr COMMA oper
            | OP_PUSH oper
            | OP_POP reg
            | OP_ADD reg COMMA reg COMMA oper
            | OP_SUB reg COMMA reg COMMA oper
            | OP_MUL reg COMMA reg COMMA oper
            | OP_DIVMOD reg COMMA reg COMMA reg COMMA oper
            | OP_XOR reg COMMA reg COMMA oper
            | OP_AND reg COMMA reg COMMA oper
            | OP_OR  reg COMMA reg COMMA oper
            | OP_SHL reg COMMA reg COMMA oper
            | OP_SHR SIGN reg COMMA reg COMMA oper
            | OP_CALL jmp

            | OP_JMP jmp
            | OP_JZ reg COMMA jmp
            | OP_JNZ reg COMMA jmp
            | OP_JE reg COMMA oper COMMA jmp
            | OP_JNE reg COMMA oper COMMA jmp
            | OP_JL reg COMMA oper COMMA jmp
            | OP_JLE reg COMMA oper COMMA jmp
            | OP_JG reg COMMA oper COMMA jmp
            | OP_JGE reg COMMA oper COMMA jmp
            | OP_JB reg COMMA oper COMMA jmp
            | OP_JBE reg COMMA oper COMMA jmp
            | OP_JA reg COMMA oper COMMA jmp
            | OP_JAE reg COMMA oper COMMA jmp

            | OP_SYSCALL
            | OP_HALT
            | OP_ALIGN
            | OP_BRK

            | OP_MOV reg COMMA oper
            | OP_DIV reg COMMA reg COMMA oper
            | OP_MOD reg COMMA reg COMMA oper
            | OP_IDIVMOD reg COMMA reg COMMA reg COMMA oper
            | OP_IDIV reg COMMA reg COMMA oper
            | OP_IMOD reg COMMA reg COMMA oper
            | OP_NEG reg COMMA reg
            | OP_NOT reg COMMA reg
            | OP_NOP
            | OP_RET'''
    p[0] = Inst(p[1])
    op = p[1].lower()
    args = filter(lambda x: x != ',', p[2:])
    if op == 'load':
        ext, sz = args[0]
        args = [ext, sz] + args[1:]

    p[0].op = op
    p[0].args = args
    p[0].lexspan = p.lexspan(1)

def p_label(p):
    'label : ident COLON'
    p[0] = Label(p[1])
    p[0].lexend = p.lexpos(2) + len(p[2])
    set_parent(p[0])

def p_format(p):
    'decl : FORMAT ID'
    fmt = p[2].lower()
    p[0] = Format(p[2])
    p[0].lexspan = (p.lexspan(1)[0], p.lexspan(2)[1])

def p_section(p):
    'decl : SECTION ident'
    p[0] = Section(p[2])
    p[0].lexpos = p.lexpos(1)

def p_global(p):
    'decl : GLOBAL ident'
    p[0] = Global(p[2])
    p[0].lexpos = p.lexpos(1)
    if '.' in p[0].name:
        error(p[0].lexspan, 'Only top-level symbols can be exported')

def p_extern(p):
    'decl : EXTERN ident'
    p[0] = Extern(p[2])
    p[0].lexpos = p.lexpos(1)

def set_size(p):
    tok = p.value.lower()
    if tok.endswith('bit'):
        p.size = 1
    elif tok.endswith('b'):
        p.size = 8
    elif tok.endswith('h'):
        p.size = 16
    elif tok.endswith('w'):
        p.size = 32
    else:
        error(p.lexspan, 'Invalid size suffix')

def p_data_decl(p):
    'decl : DATA datas'
    p[0] = Data(p[1], *p[2])
    p[0].lexspan = p.lexspan(1)
    set_size(p[0])

def p_align(p):
    'decl : ALIGN'
    p[0] = Align(p[1])
    p[0].lexspan = p.lexspan(1)
    set_size(p[0])

def p_alloc(p):
    'decl : ALLOC expr'
    p[0] = Alloc(p[1], p[2])
    p[0].lexspan = p.lexspan(1)
    set_size(p[0])

def p_data(p):
    '''data : expr
            | STRING'''
    if isinstance(p[1], Expr):
        p[0] = p[1]
    else:
        p[0] = String(p[1])
        p[0].lexspan = p.lexspan(1)

def p_end(p):
    'decl : END'
    if not state.symbol:
        error(p.lexspan(1), 'No symbol to end here')
    p[0] = EndLabel(state.symbol.value)
    p[0].lexspan = p.lexspan(1)
    state.symbol = None

def p_datas(p):
    '''datas : data COMMA datas
             | data'''
    p[0] = [p[1]]
    if len(p) > 2:
        p[0] += p[3]

def p_stmt(p):
    '''stmt : decl
            | inst
            | label'''
    p[0] = p[1]

def p_stmts(p):
    '''stmts : stmt stmts
             |'''
    if len(p) > 1:
        p[0] = [p[1]] + p[2]
    else:
        p[0] = []

def p_error(t):
    # if not t:
    #     tok = parser.symstack[-1].value
    #     beg = tok.lexpos
    #     end = beg + len(tok.value)
    #     beg, end = tok.lexspan
    #     msg = 'Unexpected end of file'
    # else:
    #     if parser.symstack and parser.symstack[-1].type == 'ident':
    #         ident = parser.symstack[-1].value
    #         beg = ident.lexpos
    #         end = beg + len(ident.value)
    #         msg = 'Expected opcode, label or declaration'
    #     else:
    #         beg = t.lexpos
    #         end = getattr(t, 'endlexpos', None)
    #         msg = 'Syntax error'
    if not t:
        t = parser.symstack[-1].value
        msg = 'Unexpected end of file'
    else:
        msg = 'Syntax error'
    try:
        beg, end = t.lexspan
    except:
        beg = t.lexpos
        end = getattr(t, 'endlexpos', None)
    error((beg, end), msg)

# Build the parser
parser = yacc.yacc()
def parse(input):
    state.input = input
    return parser.parse(input, debug=debug, tracking=track)
