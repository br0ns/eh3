from consts import *

class Node(object):
    lexpos = None
    lexend = None
    def __init__(self, value, *args):
        # Make it possible to easily change the type of a node
        if isinstance(value, Node):
            node = value
            self.value = node.value

            # Can only get args from one place
            assert not args or not node.args
            self.args = args or node.args

            self.lexpos = node.lexpos
            self.lexend = node.lexend

        else:
            self.value = value
            self.args = args
    def __repr__(self):
        s = '%s:%r' % (self.__class__.__name__, self.value)
        if self.args:
            s += '(%s)' % ', '.join(map(repr, self.args))
        return s
    @property
    def lexspan(self):
        return (self.lexpos, self.lexend)
    @lexspan.setter
    def lexspan(self, (beg, end)):
        self.lexpos = beg
        self.lexend = end

# Mixins
class WithName:
    @property
    def name(self):
        return self.value
class WithScope:
    parent = None
    def is_local(self):
        return self.value[0] == '.'
    @property
    def name(self):
        if self.is_local():
            return '%s%s' % (self.parent, self.value)
        else:
            return self.value
class WithOp:
    @property
    def op(self):
        return self.value

class Opcode(Node): pass
class Inst(Node): pass
class Reg(Node, WithName): pass
class Ident(Node, WithName): pass
class Label(WithScope, Ident): pass
class EndLabel(Label):
    # Little hack here: '$' can't be part of an identifier, so we use it here to
    # give end markers unique names.  Further, local labels can't have end
    # markers, so no need to take care of those here
    @property
    def name(self):
        return '$' + self.value

class Format(Ident): pass
class Global(Ident): pass
class Extern(Ident): pass
class Section(Ident): pass

class Align(Node): pass
class Data(Node): pass
class Alloc(Node): pass
class String(Node): pass

class Ext(Node): pass
class Sz(Node): pass
class Sign(Node): pass

# Abstract class
class Expr(Node): pass

class UnOp(Expr, WithOp): pass
class BinOp(Expr, WithOp): pass
class Number(Expr): pass
class Var(Expr, WithScope, Ident): pass
class SizeOf(Expr, Ident): pass
