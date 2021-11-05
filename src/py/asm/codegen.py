import sys
import operator

from collections import defaultdict

from err import show_error
from consts import *
from .data import *
from .yacc import parse
import obj

SZ_BIT = 1
SZ_BYTE = 8

FIXPOINT_LOOPS = 1000

def error(span, msg):
    show_error(input, span, msg)
    sys.exit(1)

def warn(span, msg):
    show_error(input, span, msg)

def eval_expr(expr, env, unit):
    # `unit` gives the conversion between symbol values and the result; if the
    # result is bits then `unit` is 1, if it is bytes then it is 8
    def loop(e):
        # -> (addend, vars to add, vars to subtract)
        # convert `sizeof(x)` to `$x - x`
        if isinstance(e, SizeOf):
            beg = Label(e)
            end = EndLabel(e)
            e = BinOp('-', Var(end.name), Var(beg.name))

        if isinstance(e, Number):
            return (e.value, [], [])

        elif isinstance(e, Var):
            if e.name not in env and e.name not in extern_syms:
                error(e.lexspan, 'Symbol "%s" is undefined' % e.name)
            return (0, [e], [])

        elif isinstance(e, UnOp):
            op = {'-': operator.neg,
                  '~': operator.inv,
            }[e.op]
            n, a, s = loop(e.args[0])
            return -n, s, a

        elif isinstance(e, BinOp):
            op = {'+': operator.add,
                  '-': operator.sub,
                  '*': operator.mul,
                  '/': operator.div,
                  '%': operator.mod,
                  '**': operator.pow,
                  '&': operator.and_,
                  '|': operator.or_,
                  '^': operator.xor,
            }[e.op]

            n1, a1, s1 = loop(e.args[0])
            n2, a2, s2 = loop(e.args[1])
            vs1 = set(map(str, a1 + s1))
            vs2 = set(map(str, a2 + s2))
            vs = vs1.union(vs2)
            if vs:
                if e.value == '+':
                    a = a1 + a2
                    s = s1 + s2

                elif e.value == '-':
                    a = a1 + s2
                    s = s1 + a2

                # Only one side has variables, so we just duplicate them
                elif e.value == '*' and not (vs1 and vs2):
                    if not vs1:
                        a = a2 * n1
                        s = s2 * n1
                    else:
                        a = a1 * n2
                        s = s1 * n2

                else:
                    msg  = 'Identifier' + ('' if len(vs) == 1 else 's') + ' '
                    msg += ', '.join('"%s"' % v for v in vs)
                    msg += ' used in complex expression'
                    error(e.lexpos, msg)

            else:
                a = []
                s = []

            n = op(n1, n2)

            # Need both vars to add and subtract to cancel out
            if not (a and s):
                return (n, a, s)

            # NOTE: We can only remove local variables in pairs because the load
            #       address is not yet known.  Further, the difference between
            #       pairs must be on a `unit` boundary, and they must belong to
            #       the same section.

            # Go through all vars to add
            ai = 0
            while ai < len(a):
                avar = a[ai]
                if avar.name not in env:
                    ai += 1
                    continue
                asec, aval = env[avar.name]

                # Go through all vars to sub
                si = 0
                while si < len(s):
                    svar = s[si]
                    if svar.name not in env:
                        si += 1
                        continue
                    ssec, sval = env[svar.name]

                    # We have a match
                    if asec == ssec and (aval - sval) % unit == 0:
                        n += (aval - sval) // unit
                        a.pop(ai)
                        s.pop(si)
                        break

                    si += 1

                # No match
                else:
                    ai += 1

            return (n, a, s)

    n, a, s = loop(expr)
    if s or len(a) > 1:
        msg = 'Expression must evaluate to a constant or a constant plus an ' \
              'identifier, but '
        vs = set(v.name for v in a + s)
        msg  = 'identifier' + ('' if len(vs) == 1 else 's') + ' '
        msg += ', '.join('"%s"' % v for v in vs)
        msg += ' could not be eliminated'
        error(expr.lexspan, msg)
    addent = n
    symbol = a[0].name if a else None
    return addent, symbol

def bits(x, n):
    bs = []
    for _ in xrange(n):
        bs.append(x & 1)
        x >>= 1
    return bs

def codegen(arg_input):
    global input       # used by `error`
    global extern_syms # used by `eval_expr`
    input = arg_input
    extern_syms = {}

    global_syms = {}
    format = None
    object_syms = {}

    # (name, load addr, stmts)
    sections = []
    section = None
    stmts = []
    # name -> (section idx, offset)
    env = {}

    # Collect globals/externs/sections
    cur_sym = None

    # Inject an end marker for the current symbol, if any
    def end_sym():
        if cur_sym:
            end = EndLabel(cur_sym)
            stmts.append(end)
            # Will fixed point iterate later, so just initialize to 0
            env[end.name] = [len(sections), 0]

    for stmt in parse(input):
        if isinstance(stmt, Section):
            end_sym()
            if section:
                sections.append((section, None, stmts))
            section = stmt.name
            stmts = []
            cur_sym = None

        elif isinstance(stmt, Format):
            if format != None:
                error(stmt.lexspan, 'Format already declared')
            format = stmt.name
            if format not in ('eh3', 'elf'):
                error(stmt.lexspan, 'Invalid format')

        elif isinstance(stmt, Global):
            global_syms[stmt.name] = stmt

        elif isinstance(stmt, Extern):
            extern_syms[stmt.name] = stmt

        else:
            if not section:
                error(stmt.lexspan, 'No section defined yet')
            if isinstance(stmt, Label):
                object_syms[stmt.name] = stmt
                if stmt.name in env:
                    error(stmt.lexspan,
                          'Symbol "%s" was already defined' % stmt.name)
                # Will fixed point iterate later, so just initialize to 0
                env[stmt.name] = [len(sections), 0]

                is_end = isinstance(stmt, EndLabel)
                # Current symbol ends here
                if is_end:
                    cur_sym = None

                # New symbol starts here
                elif not stmt.is_local():
                    end_sym()
                    cur_sym = stmt.name

            stmts.append(stmt)

    end_sym()
    if not section:
        error(None, 'No sections defined')
    sections.append((section, None, stmts))

    # Check that all global symbols are actually defined
    for sym in global_syms:
        if sym not in object_syms:
            error(global_syms[sym].lexspan,
                  'Global variable "%s" is not defined' % sym)

    # And that all extern symbols are not
    for sym in extern_syms:
        if sym in object_syms:
            error(object_syms[sym].lexspan,
                  'Extern variable "%s" is defined' % sym)

    # Generate code
    exports = set(global_syms.keys())
    for _ in xrange(FIXPOINT_LOOPS):
        o = obj.Obj()
        fixed = True

        for secidx, (name, _base, stmts) in enumerate(sections):
            section = obj.Section()
            section.name = name
            section.prot = PROT_R
            if name == '.text':
                section.prot |= PROT_X
            elif name in ('.data', '.bss'):
                section.prot |= PROT_W
            # `size` holds the size (in bits) of uninitialized data
            section.size = 0

            code = []

            def sym_here(sym):
                return sym in env and env[sym][0] == secidx

            def align(unit=8):
                code.extend([0] * (-len(code) % unit))

            def align_bss(unit):
                section.size += -section.size % (unit)

            def fits(x, sz, sign=SIGN_U):
                # Imms are sign extended to word size, so for encoding sizes
                # less than the word size must be treated as signed numbers.
                if sign == SIGN_S or sz < SIZE_W:
                    max = 1 << (sz - 1) - 1
                else:
                    max = MASK_W
                # Negative numbers may be silently cast to unsigned, i.e. `ADD
                # a0, -1` is allowed and equivalent to `ADD a0, 0xffffffff`.
                min = -1 << (sz - 1)
                return x >= min and x <= max

            def emit_bits(val, sz):
                code.extend(bits(val, sz))

            def emit_sz(size):
                emit_bits(size / 8 - 1, 2)

            def emit_imm(lexspan, val, size):
                if not fits(val, size):
                    error(lexspan,
                          'Value %#x does not fit in %d bits' % (val, size))
                emit_bits(val, size)

            def gen_reloc(type, size, symbol):
                reloc = obj.Relocation()
                reloc.section = secidx
                reloc.offset = len(code)
                reloc.size = size
                reloc.type = type
                reloc.symbol = symbol

                if symbol not in extern_syms:
                    exports.add(symbol)
                o.relocations.append(reloc)

            def gen_jcc(op):
                emit_bits(JCC_NAMES.index(op.lower()), JCC_SZ)

            def gen_ext(ext):
                emit_bits(EXT_NAMES.index(ext.lower()), EXT_SZ)

            def gen_sz(sz):
                emit_bits(SZ_NAMES.index(sz.lower()), SZ_SZ)

            def gen_sign(sign):
                emit_bits(SIGN_NAMES.index(sign.lower()), SIGN_SZ)

            def gen_reg(reg):
                emit_bits(REG_NAMES.index(reg.name), 4)

            def gen_imm(expr, sign=SIGN_U):
                addent, symbol = eval_expr(expr, env, SZ_BYTE)
                if symbol:
                    size = 32
                    emit_sz(size)
                    gen_reloc(obj.RELOC_ABS, size, symbol)
                else:
                    for size in (SIZE_B, SIZE_H, SIZE_T, SIZE_W):
                        if fits(addent, size, sign):
                            break
                    emit_sz(size)
                emit_imm(expr.lexspan, addent, size)

            def gen_oper(x, sign=SIGN_U):
                # Optimization: 0 takes 10b, ZZ takes only 4
                if isinstance(x, Number) and x.value == 0:
                    x = Reg('zz')
                if isinstance(x, Reg):
                    code.append(0)
                    gen_reg(x)
                else: # Expr
                    code.append(1)
                    gen_imm(x, sign)

            def gen_jmp(target):
                if isinstance(target, Reg):
                    code.append(0)
                    gen_reg(target)

                else: # Expr
                    code.append(1)
                    addent, symbol = eval_expr(target, env, SZ_BIT)
                    if symbol and not sym_here(symbol):
                        size = 32
                        emit_sz(size)
                        gen_reloc(obj.RELOC_REL, size, symbol)
                    else: # symbol defined in this section
                        addent -= len(code) + 2
                        if symbol:
                            addent += env[symbol][1]
                        for size in (8, 16, 32):
                            if fits(addent - size, size, sign=SIGN_S):
                                break
                        emit_sz(size)
                    addent -= size
                    emit_imm(target.lexspan, addent, size)

            def gen_addr((base, index)):
                gen_reg(base or Reg('zz'))
                gen_oper(index)

            def followed_by_non_alloc(i):
                for j in xrange(i + 1, len(stmts)):
                    if not isinstance(stmts[j], (Alloc, Label)):
                        return True
                return False

            for i, stmt in enumerate(stmts):
                env['$'] = [secidx, len(code)]

                if isinstance(stmt, Label):
                    # Align to unit boundary if followed by `Data` or `Alloc`
                    for j in xrange(i + 1, len(stmts)):
                        if isinstance(stmts[j], Label):
                            continue
                        if isinstance(stmts[j], (Data, Alloc)):
                            unit = stmts[j].size
                            if followed_by_non_alloc(i):
                                align(unit)
                            else:
                                align_bss(unit)
                            break
                    name = stmt.name
                    # Jump/call targets does not need to be aligned if they are
                    # not exported, so record bit offsets
                    val = len(code) + section.size
                    # If a symbol was previously not defined or has changed, a
                    # fixed point has not been reached
                    if env[name][1] != val:
                        fixed = False
                        env[name][1] = val

                elif isinstance(stmt, Data):
                    size = stmt.size / 8
                    align(stmt.size)
                    for arg in stmt.args:
                        if isinstance(arg, String):
                            s = arg.value
                            # Align according to item size
                            s += '\0' * (-len(s) % size)
                            for c in s:
                                code += bits(ord(c), 8)
                        else: # expr
                            value, symbol = eval_expr(arg, env, SZ_BYTE)
                            if symbol:
                                gen_reloc(obj.RELOC_ABS, stmt.size, symbol)
                            emit_imm(arg.lexspan, value, stmt.size)

                elif isinstance(stmt, Alloc):
                    expr = stmt.args[0]
                    value, symbol = eval_expr(expr, env, SZ_BYTE)
                    if symbol:
                        error(expr.lexspan,
                              'Unresolved symbol in allocation size')
                    if followed_by_non_alloc(i):
                        warn(expr.lexspan,
                             'Uninitialized data is not a end of section; must '
                             'emit zeros.')
                        align(stmt.size)
                        emit_bits(0, value * 8)
                    else:
                        align_bss(stmt.size)
                        section.size += value * stmt.size

                elif isinstance(stmt, Align):
                    unit = stmt.size
                    if followed_by_non_alloc(i):
                        align(unit)
                    else:
                        align_bss(unit)

                elif isinstance(stmt, Inst):
                    op = stmt.op
                    args = stmt.args[::] # Modified below

                    # Convert pseudo instructions
                    if op == 'mov':
                        args.insert(1, Reg('zz'))
                    elif op == 'neg':
                        args.insert(1, Reg('zz'))
                    elif op == 'not':
                        args.insert(2, Number(~0))
                    elif op == 'nop':
                        pass
                    # Signed arithmetics
                    # elif op == 'mul':
                    #     args.insert(0, 'unsigned')
                    elif op == 'divmod':
                        args.insert(0, 'unsigned')
                    elif op == 'div':
                        args.insert(0, 'unsigned')
                        args.insert(2, Reg('zz'))
                    elif op == 'mod':
                        args.insert(0, 'unsigned')
                        args.insert(1, Reg('zz'))
                    # elif op == 'imul':
                    #     args.insert(0, 'signed')
                    elif op == 'idivmod':
                        args.insert(0, 'signed')
                    elif op == 'idiv':
                        args.insert(0, 'signed')
                        args.insert(2, Reg('zz'))
                    elif op == 'imod':
                        args.insert(0, 'signed')
                        args.insert(1, Reg('zz'))
                    # Jumps
                    elif op == 'jnz':
                        args.insert(1, Reg('zz'))
                    elif op == 'jl':
                        args.insert(0, 'signed')
                    elif op == 'jle':
                        args.insert(0, 'signed')
                    elif op == 'jg':
                        args.insert(0, 'signed')
                    elif op == 'jge':
                        args.insert(0, 'signed')
                    elif op == 'jb':
                        args.insert(0, 'unsigned')
                    elif op == 'jbe':
                        args.insert(0, 'unsigned')
                    elif op == 'ja':
                        args.insert(0, 'unsigned')
                    elif op == 'jae':
                        args.insert(0, 'unsigned')
                    elif op == 'ret':
                        args = [Reg('lr')]
                    # Subtract immediate
                    elif op == 'sub' and isinstance(args[2], Number):
                        op = 'add'
                        n = args[2]
                        # Must make copy because of multiple parses
                        args[2] = Number(-n.value)
                        args[2].lexspan = n.lexspan
                    op = OP_PSEUDO.get(op, op)

                    # Output opcode
                    if op in ALT_NAMES:
                        alt = ALT_NAMES.index(op)
                        code += bits(OP_ALT, OP_SZ)
                        code += bits(alt, ALT_SZ)
                    elif op in JCC_NAMES:
                        jcc = JCC_NAMES.index(op)
                        code += bits(OP_JCC, OP_SZ)
                        code += bits(jcc, JCC_SZ)
                    else:
                        opcode = OP_NAMES.index(op)
                        code += bits(opcode, OP_SZ)

                    if op == 'load':
                        ext, sz, reg, addr = args
                        gen_ext(ext)
                        gen_sz(sz)
                        gen_reg(reg)
                        gen_addr(addr)

                    elif op == 'store':
                        sz, addr, oper = args
                        gen_sz(sz)
                        gen_addr(addr)
                        gen_oper(oper)

                    elif op == 'push':
                        oper = args[0]
                        gen_oper(oper)

                    elif op == 'pop':
                        reg = args[0]
                        gen_reg(reg)

                    # elif op in ('add', 'xor', 'and', 'or', 'shl'):
                    elif op in ('add', 'xor', 'and', 'or', 'shl', 'mul'):
                        reg1, reg2, oper = args
                        gen_reg(reg1)
                        gen_reg(reg2)
                        gen_oper(oper)

                    elif op == 'sub':
                        reg1, reg2, reg3 = args
                        gen_reg(reg1)
                        gen_reg(reg2)
                        gen_reg(reg3)

                    # elif op in ('mul', 'shr'):
                    elif op == 'shr':
                        sign, reg1, reg2, oper = args
                        gen_sign(sign)
                        gen_reg(reg1)
                        gen_reg(reg2)
                        gen_oper(oper, sign)

                    elif op == 'divmod':
                        sign, reg1, reg2, reg3, oper = args
                        gen_sign(sign)
                        gen_reg(reg1)
                        gen_reg(reg2)
                        gen_reg(reg3)
                        gen_oper(oper, sign)

                    elif op == 'call':
                        gen_jmp(args[0])
                        align()

                    # JCC
                    elif op == 'jmp':
                        gen_jmp(args[0])

                    elif op == 'jz':
                        reg, jmp = args
                        gen_reg(reg)
                        gen_jmp(jmp)

                    elif op in ('je', 'jne'):
                        reg1, oper, jmp = args
                        gen_reg(reg1)
                        gen_oper(oper)
                        gen_jmp(jmp)

                    elif op in ('jl', 'jle', 'jg', 'jge'):
                        sign, reg1, oper, jmp = args
                        gen_sign(sign)
                        gen_reg(reg1)
                        gen_oper(oper)
                        gen_jmp(jmp)

                    # ALT
                    elif op in ('syscall', 'halt', 'brk'):
                        pass

                    elif op in ('align'):
                        align()

                    # Never reached (insh'Allah)
                    else:
                        raise NotImplemented(op)

            # Convert section size to bytes
            section.size = (section.size + len(code) + 7) / 8
            # Convert code to bytearray
            code += [0] * (-len(code) % 8)
            codesz = len(code) / 8
            section.data = bytearray(codesz)
            for i in xrange(codesz):
                x = 0
                for j in xrange(8):
                    x |= code[i * 8 + j] << j
                section.data[i] = x

            o.sections.append(section)

        if fixed:
            break

    else:
        raise Exception, 'fixpoint'

    # Check that all symbols in the symbol table are addressable
    for sym, (_, offset) in env.items():
        if offset % 8 == 0:
            continue
        if sym not in exports and sym not in global_syms:
            continue
        msg = 'Symbol "%s" is not placed on a byte boundary, but must be ' \
              'addressable because it is '
        if sym in global_syms:
            msg += 'declared global'
        else:
            msg += 'used in a relocation'
        error(object_syms[sym].lexspan, msg % sym)

    # Export symbols
    for sym in exports:
        symbol = obj.Symbol()
        symbol.name = sym
        symbol.section, symbol.offset = env[sym]
        symbol.offset /= 8
        symbol.bind = obj.BIND_GLOBAL if sym in global_syms else obj.BIND_LOCAL
        symend = EndLabel(sym)
        symbol.size = env[symend.name][1] - symbol.offset
        o.symbols.append(symbol)

    return format, o
