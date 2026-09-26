"""jast: statement and expression trees of Java method bodies, over the tokens of tools/gen/javasrc.py.

javasrc parses declarations and keeps bodies as token spans; this module parses the statements and expressions inside them, for the
subset of Java the quest handlers use. Anything outside the subset raises Unsupported with a category (the refusal reasons of the
transliterator): closures (lambda, method reference, anonymous class, local class), exception handling (try, throw), synchronized,
labeled statements, switch expressions and switch rules, generic method calls, array creation without an initializer, class literals.

Every node keeps `tok`, the index of its first token; statements also keep `last`, the index of their last token, which the emitter
uses to carry the Java comments along (comments(), trailing_comment()).
"""
from __future__ import annotations

from dataclasses import dataclass

from . import paths  # noqa: F401  (puts tools/gen on sys.path for javasrc)

import javasrc  # noqa: E402

IDENT, KEYWORD, OP, INT, FLOAT, CHAR, STRING, TEXTBLOCK = (javasrc.IDENT, javasrc.KEYWORD, javasrc.OP, javasrc.INT, javasrc.FLOAT,
                                                          javasrc.CHAR, javasrc.STRING, javasrc.TEXTBLOCK)
PRIMITIVES = frozenset('boolean byte char short int long float double'.split())


class Unsupported(Exception):
    """A construct the transliterator refuses: category (a refusal reason key) and detail (where and what)."""

    def __init__(self, category, detail='', tok=-1):
        super().__init__(f'{category}: {detail}')
        self.category = category
        self.detail = detail
        self.tok = tok


# --- expressions -------------------------------------------------------------------------------------------------------------------
@dataclass
class Expr:
    tok: int


@dataclass
class Lit(Expr):
    kind: str          # int long float double char string bool null
    text: str


@dataclass
class Name(Expr):
    name: str


@dataclass
class FieldAccess(Expr):
    target: Expr
    name: str


@dataclass
class Call(Expr):
    target: Expr | None      # None: unqualified; Name('super') / Name('this') for super.x() / this.x()
    name: str
    args: list


@dataclass
class New(Expr):
    type: 'JType'
    args: list


@dataclass
class NewArray(Expr):
    type: 'JType'            # element type
    init: 'ArrayInit'


@dataclass
class ArrayInit(Expr):
    items: list


@dataclass
class Index(Expr):
    target: Expr
    index: Expr


@dataclass
class Cast(Expr):
    type: 'JType'
    expr: Expr


@dataclass
class Unary(Expr):
    op: str
    expr: Expr
    postfix: bool = False


@dataclass
class Binary(Expr):
    op: str
    left: Expr
    right: Expr


@dataclass
class Cond(Expr):
    cond: Expr
    a: Expr
    b: Expr


@dataclass
class Assign(Expr):
    op: str
    target: Expr
    value: Expr


@dataclass
class InstanceOf(Expr):
    expr: Expr
    type: 'JType'
    binding: str | None = None


@dataclass
class Paren(Expr):
    expr: Expr


# --- types -------------------------------------------------------------------------------------------------------------------------
@dataclass
class JType:
    name: str                # 'int', 'Player', 'ItemPacketService.ItemAddType'
    dims: int = 0
    args: str | None = None  # generic arguments as written, None if none
    tok: int = -1

    def __str__(self):
        return self.name + ('<' + self.args + '>' if self.args is not None else '') + '[]' * self.dims

    @property
    def primitive(self):
        return self.name in PRIMITIVES and self.dims == 0


# --- statements --------------------------------------------------------------------------------------------------------------------
@dataclass
class Stmt:
    tok: int
    last: int


@dataclass
class Block(Stmt):
    stmts: list


@dataclass
class Local(Stmt):
    type: JType
    decls: list          # [(name, extra dims, init Expr | None, name token)]
    final: bool = False


@dataclass
class ExprStmt(Stmt):
    expr: Expr


@dataclass
class If(Stmt):
    cond: Expr
    then: Stmt
    else_: Stmt | None


@dataclass
class Switch(Stmt):
    expr: Expr
    groups: list         # [([label Expr | None (default)], [Stmt], label tokens)]


@dataclass
class Return(Stmt):
    expr: Expr | None


@dataclass
class Break(Stmt):
    label: str | None = None


@dataclass
class Continue(Stmt):
    label: str | None = None


@dataclass
class For(Stmt):
    init: list
    cond: Expr | None
    update: list
    body: Stmt


@dataclass
class ForEach(Stmt):
    type: JType
    name: str
    iterable: Expr
    body: Stmt
    final: bool = False


@dataclass
class While(Stmt):
    cond: Expr
    body: Stmt


@dataclass
class DoWhile(Stmt):
    body: Stmt
    cond: Expr


@dataclass
class Empty(Stmt):
    pass


BINARY_PREC = [
    ('||',), ('&&',), ('|',), ('^',), ('&',), ('==', '!='), ('<', '>', '<=', '>=', 'instanceof'), ('<<', '>>', '>>>'), ('+', '-'),
    ('*', '/', '%'),
]
ASSIGN_OPS = frozenset(('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=', '>>>='))


class Parser:
    """Parser over the tokens of one CompilationUnit."""

    def __init__(self, cu):
        self.cu = cu
        T = cu.tokens
        self.kind = T.kind
        self.text = T.text
        self.start = T.start
        self.end = T.end
        self.match = T.match
        self.src = T.source

    # -- helpers --------------------------------------------------------------------------------------------------------------------
    def t(self, i):
        return self.text[i]

    def loc(self, i):
        line, col = self.cu.tokens.loc(i)
        return f'{line}:{col}'

    def fail(self, category, i, what=''):
        raise Unsupported(category, f'line {self.loc(i)}' + (f': {what}' if what else ''), i)

    def expect(self, i, text):
        if self.t(i) != text:
            self.fail('syntax', i, f'expected {text!r}, found {self.t(i)!r}')
        return i + 1

    def adjacent(self, i):
        """token i+1 starts where token i ends (for '>' '>' shifts)"""
        return self.end[i] == self.start[i + 1]

    # -- comments -------------------------------------------------------------------------------------------------------------------
    def gap(self, i):
        """source text between token i-1 and token i"""
        s = self.end[i - 1] if i > 0 else 0
        return self.src[s:self.start[i]]

    # -- types ----------------------------------------------------------------------------------------------------------------------
    def parse_type(self, i):
        """(JType, next index) or (None, i) when no type starts at i"""
        k = self.kind[i]
        if k == KEYWORD and self.t(i) in PRIMITIVES:
            name = self.t(i)
            j = i + 1
        elif k == IDENT:
            name = self.t(i)
            j = i + 1
            while self.t(j) == '.' and self.kind[j + 1] == IDENT:
                name += '.' + self.t(j + 1)
                j += 2
        else:
            return None, i
        args = None
        if self.t(j) == '<':
            depth = 0
            s = j
            while True:
                tt = self.t(j)
                if tt == '<':
                    depth += 1
                elif tt == '>':
                    depth -= 1
                    if depth == 0:
                        break
                elif tt in (';', '{', '}', '(', ')', '=') or self.kind[j] == javasrc.EOF:
                    return None, i
                j += 1
            args = self.src[self.end[s]:self.start[j]].strip()
            j += 1
        dims = 0
        while self.t(j) == '[' and self.t(j + 1) == ']':
            dims += 1
            j += 2
        return JType(name, dims, args, i), j

    def looks_like_local(self, i):
        j = i
        final = False
        while self.t(j) in ('final',) or self.t(j) == '@':
            if self.t(j) == '@':
                j += 2
                if self.t(j) == '(':
                    j = self.match[j] + 1
                continue
            final = True
            j += 1
        ty, k = self.parse_type(j)
        if ty is None:
            return None
        if self.kind[k] == IDENT and self.t(k + 1) in ('=', ';', ',', '[', ':'):
            return j, final
        return None

    # -- statements -----------------------------------------------------------------------------------------------------------------
    def block_stmts(self, open_i):
        """statements of the block whose '{' is at open_i"""
        close = self.match[open_i]
        out = []
        i = open_i + 1
        while i < close:
            s, i = self.stmt(i)
            out.append(s)
        return out

    def stmt(self, i):
        t = self.t(i)
        k = self.kind[i]
        if t == '{':
            close = self.match[i]
            return Block(i, close, self.block_stmts(i)), close + 1
        if t == ';':
            return Empty(i, i), i + 1
        if k == KEYWORD:
            if t == 'if':
                c1 = i + 1
                c2 = self.match[c1]
                cond = self.expr_span(c1 + 1, c2)
                then, j = self.stmt(c2 + 1)
                els = None
                if self.t(j) == 'else':
                    els, j = self.stmt(j + 1)
                return If(i, j - 1, cond, then, els), j
            if t == 'return':
                if self.t(i + 1) == ';':
                    return Return(i, i + 1, None), i + 2
                e, j = self.expr(i + 1)
                j = self.expect(j, ';')
                return Return(i, j - 1, e), j
            if t in ('break', 'continue'):
                label = None
                j = i + 1
                if self.kind[j] == IDENT:
                    label = self.t(j)
                    self.fail('labeled-statement', i, f'{t} {label}')
                j = self.expect(j, ';')
                return (Break if t == 'break' else Continue)(i, j - 1, label), j
            if t == 'switch':
                return self.switch(i)
            if t == 'for':
                return self.for_(i)
            if t == 'while':
                c1 = i + 1
                c2 = self.match[c1]
                cond = self.expr_span(c1 + 1, c2)
                body, j = self.stmt(c2 + 1)
                return While(i, j - 1, cond, body), j
            if t == 'do':
                body, j = self.stmt(i + 1)
                j = self.expect(j, 'while')
                c2 = self.match[j]
                cond = self.expr_span(j + 1, c2)
                j = self.expect(c2 + 1, ';')
                return DoWhile(i, j - 1, body, cond), j
            if t == 'try':
                self.fail('try-catch', i)
            if t == 'throw':
                self.fail('throw', i)
            if t == 'synchronized':
                self.fail('synchronized', i)
            if t in ('class', 'interface', 'enum', 'abstract', 'static'):
                self.fail('local-class', i)
            if t == 'assert':
                self.fail('assert', i)
        if k == IDENT and self.t(i + 1) == ':' and self.t(i + 2) != ':':
            self.fail('labeled-statement', i, t)
        if k == IDENT and t in ('record', 'yield') and self.kind[i + 1] == IDENT:
            self.fail('local-class' if t == 'record' else 'switch-expression', i, t)
        loc = self.looks_like_local(i)
        if loc is not None:
            return self.local(i, loc[0], loc[1])
        e, j = self.expr(i)
        j = self.expect(j, ';')
        return ExprStmt(i, j - 1, e), j

    def local(self, i, type_i, final, terminator=';'):
        ty, j = self.parse_type(type_i)
        if ty.name == 'var':
            ty = JType('var', 0, None, type_i)
        if self.t(j) == 'class':
            self.fail('local-class', i)
        decls = []
        while True:
            name_i = j
            name = self.t(j)
            j += 1
            extra = 0
            while self.t(j) == '[' and self.t(j + 1) == ']':
                extra += 1
                j += 2
            init = None
            if self.t(j) == '=':
                if self.t(j + 1) == '{':
                    init, j = self.array_init(j + 1)
                else:
                    init, j = self.expr(j + 1)
            decls.append((name, extra, init, name_i))
            if self.t(j) == ',':
                j += 1
                continue
            break
        if terminator:
            j = self.expect(j, terminator)
        return Local(i, j - 1, ty, decls, final), j

    def switch(self, i):
        c1 = i + 1
        c2 = self.match[c1]
        subject = self.expr_span(c1 + 1, c2)
        b1 = c2 + 1
        if self.t(b1) != '{':
            self.fail('syntax', b1, 'switch without a block')
        b2 = self.match[b1]
        groups = []
        j = b1 + 1
        cur = None
        while j < b2:
            if self.t(j) in ('case', 'default'):
                labels = []
                lab_tok = j
                if self.t(j) == 'default':
                    j += 1
                    labels.append(None)
                else:
                    j += 1
                    while True:
                        e, j = self.expr(j, no_lambda=True)
                        labels.append(e)
                        if self.t(j) == ',':
                            j += 1
                            continue
                        break
                if self.t(j) == '->':
                    self.fail('switch-rule', j)
                j = self.expect(j, ':')
                if cur is None or cur[1]:
                    cur = (labels, [], [lab_tok])
                    groups.append(cur)
                else:
                    cur[0].extend(labels)
                    cur[2].append(lab_tok)
                continue
            if cur is None:
                self.fail('syntax', j, 'statement before the first case label')
            s, j = self.stmt(j)
            cur[1].append(s)
        return Switch(i, b2, subject, groups), b2 + 1

    def for_(self, i):
        c1 = i + 1
        c2 = self.match[c1]
        # for-each?
        j = c1 + 1
        final = False
        while self.t(j) == 'final':
            final = True
            j += 1
        ty, k = self.parse_type(j)
        if ty is not None and self.kind[k] == IDENT and self.t(k + 1) == ':':
            it = self.expr_span(k + 2, c2)
            body, n = self.stmt(c2 + 1)
            return ForEach(i, n - 1, ty, self.t(k), it, body, final), n
        # classic for
        semi1 = self.find_top(c1 + 1, c2, ';')
        semi2 = self.find_top(semi1 + 1, c2, ';')
        init = []
        if semi1 > c1 + 1:
            loc = self.looks_like_local(c1 + 1)
            if loc is not None:
                d, _ = self.local(c1 + 1, loc[0], loc[1], terminator=None)
                init.append(d)
            else:
                for a, b in self.split_top(c1 + 1, semi1):
                    init.append(ExprStmt(a, b - 1, self.expr_span(a, b)))
        cond = self.expr_span(semi1 + 1, semi2) if semi2 > semi1 + 1 else None
        update = [self.expr_span(a, b) for a, b in self.split_top(semi2 + 1, c2)] if c2 > semi2 + 1 else []
        body, n = self.stmt(c2 + 1)
        return For(i, n - 1, init, cond, update, body), n

    def find_top(self, a, b, text):
        j = a
        while j < b:
            if self.t(j) in ('(', '[', '{'):
                j = self.match[j]
            elif self.t(j) == text:
                return j
            j += 1
        return b

    def split_top(self, a, b):
        out = []
        s = a
        j = a
        while j < b:
            if self.t(j) in ('(', '[', '{'):
                j = self.match[j]
            elif self.t(j) == ',':
                out.append((s, j))
                s = j + 1
            j += 1
        if s < b:
            out.append((s, b))
        return out

    # -- expressions ----------------------------------------------------------------------------------------------------------------
    def expr_span(self, a, b):
        e, j = self.expr(a)
        if j != b:
            self.fail('syntax', j, f'unexpected {self.t(j)!r} in expression')
        return e

    def expr(self, i, no_lambda=False):
        return self.assignment(i)

    def assignment(self, i):
        self.check_lambda(i)
        left, j = self.conditional(i)
        op = self.t(j)
        if op in ASSIGN_OPS:
            if op == '>>>=':
                self.fail('unsigned-shift', j)
            right, k = self.assignment(j + 1)
            return Assign(left.tok, op, left, right), k
        return left, j

    def check_lambda(self, i):
        if self.kind[i] == IDENT and self.t(i + 1) == '->':
            self.fail('lambda', i)
        if self.t(i) == '(' and self.t(self.match[i] + 1) == '->':
            self.fail('lambda', i)

    def conditional(self, i):
        c, j = self.binary(i, 0)
        if self.t(j) == '?':
            a, k = self.assignment(j + 1)
            k = self.expect(k, ':')
            b, n = self.conditional(k)
            return Cond(c.tok, c, a, b), n
        return c, j

    def binop_at(self, j):
        t = self.t(j)
        if t == '>' and self.t(j + 1) == '>' and self.adjacent(j):
            if self.t(j + 2) == '>' and self.adjacent(j + 1):
                return '>>>', 3
            if self.t(j + 2) in ('=', '>=') and self.adjacent(j + 1):
                return None, 0
            return '>>', 2
        if t == 'instanceof':
            return t, 1
        if self.kind[j] == OP and t in ('||', '&&', '|', '^', '&', '==', '!=', '<', '>', '<=', '>=', '<<', '+', '-', '*', '/', '%'):
            return t, 1
        return None, 0

    def binary(self, i, level):
        if level == len(BINARY_PREC):
            return self.unary(i)
        left, j = self.binary(i, level + 1)
        while True:
            op, width = self.binop_at(j)
            if op is None or op not in BINARY_PREC[level]:
                return left, j
            if op == '>>>':
                self.fail('unsigned-shift', j)
            if op == 'instanceof':
                k = j + 1
                if self.t(k) == 'final':
                    k += 1
                ty, k = self.parse_type(k)
                if ty is None:
                    self.fail('syntax', j + 1, 'instanceof without a type')
                binding = None
                if self.kind[k] == IDENT:
                    binding = self.t(k)
                    k += 1
                left = InstanceOf(left.tok, left, ty, binding)
                j = k
                continue
            right, k = self.binary(j + width, level + 1)
            left = Binary(left.tok, op, left, right)
            j = k

    def is_cast(self, i):
        """i at '(': a cast `(Type) operand`?"""
        close = self.match[i]
        ty, j = self.parse_type(i + 1)
        if ty is None or j != close:
            return None
        nxt = self.t(close + 1)
        nk = self.kind[close + 1]
        if ty.name in PRIMITIVES:
            return ty
        if nk in (IDENT, INT, FLOAT, CHAR, STRING, TEXTBLOCK) or nxt in ('(', '!', '~', 'this', 'new', 'super', 'true', 'false', 'null'):
            return ty
        return None

    def unary(self, i):
        t = self.t(i)
        if self.kind[i] == OP and t in ('+', '-', '!', '~', '++', '--'):
            e, j = self.unary(i + 1)
            return Unary(i, t, e), j
        if t == '(':
            ty = self.is_cast(i)
            if ty is not None:
                self.check_lambda(self.match[i] + 1)
                e, j = self.unary(self.match[i] + 1)
                return Cast(i, ty, e), j
        return self.postfix(i)

    def postfix(self, i):
        e, j = self.primary(i)
        while True:
            t = self.t(j)
            if t == '.':
                if self.t(j + 1) == '<':
                    self.fail('generic-method-call', j)
                if self.t(j + 1) == 'new':
                    self.fail('inner-class-creation', j)
                if self.t(j + 1) == 'class':
                    self.fail('class-literal', j)
                name = self.t(j + 1)
                if self.t(j + 2) == '(':
                    args, k = self.args(j + 2)
                    e = Call(e.tok, e, name, args)
                    j = k
                else:
                    e = FieldAccess(e.tok, e, name)
                    j += 2
                continue
            if t == '::':
                self.fail('method-reference', j)
            if t == '[':
                idx = self.expr_span(j + 1, self.match[j])
                e = Index(e.tok, e, idx)
                j = self.match[j] + 1
                continue
            if t in ('++', '--') and self.kind[j] == OP:
                e = Unary(e.tok, t, e, postfix=True)
                j += 1
                continue
            return e, j

    def args(self, open_i):
        close = self.match[open_i]
        out = []
        for a, b in self.split_top(open_i + 1, close):
            self.check_lambda(a)
            out.append(self.expr_span(a, b))
        return out, close + 1

    def array_init(self, open_i):
        close = self.match[open_i]
        items = []
        for a, b in self.split_top(open_i + 1, close):
            if self.t(a) == '{':
                it, _ = self.array_init(a)
                items.append(it)
            else:
                items.append(self.expr_span(a, b))
        return ArrayInit(open_i, items), close + 1

    def primary(self, i):
        t = self.t(i)
        k = self.kind[i]
        if k == INT:
            return Lit(i, 'long' if t[-1] in 'lL' else 'int', t), i + 1
        if k == FLOAT:
            return Lit(i, 'float' if t[-1] in 'fF' else 'double', t), i + 1
        if k == CHAR:
            return Lit(i, 'char', t), i + 1
        if k == STRING:
            return Lit(i, 'string', t), i + 1
        if k == TEXTBLOCK:
            self.fail('text-block', i)
        if t in ('true', 'false'):
            return Lit(i, 'bool', t), i + 1
        if t == 'null':
            return Lit(i, 'null', t), i + 1
        if t == '(':
            self.check_lambda(i)
            e = self.expr_span(i + 1, self.match[i])
            return Paren(i, e), self.match[i] + 1
        if t == 'this':
            if self.t(i + 1) == '(':
                self.fail('constructor-call', i)
            return Name(i, 'this'), i + 1
        if t == 'super':
            if self.t(i + 1) == '(':
                self.fail('constructor-call', i)
            return Name(i, 'super'), i + 1
        if t == 'new':
            return self.new(i)
        if t == 'switch':
            self.fail('switch-expression', i)
        if k == IDENT:
            if self.t(i + 1) == '->':
                self.fail('lambda', i)
            if self.t(i + 1) == '(':
                args, j = self.args(i + 1)
                return Call(i, None, t, args), j
            return Name(i, t), i + 1
        if k == KEYWORD and t in PRIMITIVES:
            if self.t(i + 1) == '.' and self.t(i + 2) == 'class':
                self.fail('class-literal', i)
        self.fail('syntax', i, f'unexpected {t!r}')

    def new(self, i):
        ty, j = self.parse_type(i + 1)
        if ty is None:
            self.fail('syntax', i + 1, 'new without a type')
        if ty.args is not None:
            self.fail('generic-type', i, f'new {ty}')
        if self.t(j) == '[':
            if self.t(j + 1) == ']':
                dims = 0
                while self.t(j) == '[' and self.t(j + 1) == ']':
                    dims += 1
                    j += 2
                if self.t(j) != '{':
                    self.fail('syntax', j, 'array creation without an initializer')
                init, k = self.array_init(j)
                return NewArray(i, JType(ty.name, dims - 1, None, ty.tok), init), k
            self.fail('array-creation', i, f'new {ty.name}[size]')
        ty2 = JType(ty.name, 0, None, ty.tok)
        # the type parser consumed `[]` dims; a plain class creation has none
        if ty.dims:
            if self.t(j) != '{':
                self.fail('syntax', j, 'array creation without an initializer')
            init, k = self.array_init(j)
            return NewArray(i, JType(ty.name, ty.dims - 1, None, ty.tok), init), k
        if self.t(j) != '(':
            self.fail('syntax', j, 'expected ( after new T')
        args, k = self.args(j)
        if self.t(k) == '{':
            self.fail('anonymous-class', i, f'new {ty.name}() {{...}}')
        return New(i, ty2, args), k


def walk_exprs(node):
    """every Expr under a statement or expression, pre-order"""
    if isinstance(node, list):
        for n in node:
            yield from walk_exprs(n)
        return
    if node is None:
        return
    if isinstance(node, Expr):
        yield node
    for f in getattr(node, '__dataclass_fields__', {}):
        if f in ('tok', 'last'):
            continue
        v = getattr(node, f)
        if isinstance(v, (Expr, Stmt)):
            yield from walk_exprs(v)
        elif isinstance(v, list):
            for x in v:
                if isinstance(x, (Expr, Stmt)):
                    yield from walk_exprs(x)
                elif isinstance(x, tuple):
                    for y in x:
                        if isinstance(y, (Expr, Stmt)):
                            yield from walk_exprs(y)
                        elif isinstance(y, list):
                            yield from walk_exprs([z for z in y if isinstance(z, (Expr, Stmt))])
