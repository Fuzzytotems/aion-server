"""cppdecl: a small declaration reader for the C++ headers the quest handlers call into.

It reads what the transliterator needs to type an expression and to spell a call: for each class, its bases, its member functions with
their return and parameter types (in written form), and whether they are static or virtual; for each namespace, its free functions (the
enum companions such as `getId(WorldMapType)` in WorldMapTypeInfo.h) and the enums it defines. Comments, strings and preprocessor lines
are blanked with skeleton._strip_cpp first; `#include` lines are read separately (the include closure of the quest prelude), and a
member block a class body includes (`SM_SYSTEM_MESSAGE.gen.h`) is spliced into that class.

It is not a C++ parser: it walks bracket-balanced token groups and splits declarations at `;` and at function bodies. Templates are
recorded with `template=True` and their parameter types left as written. It never raises on odd input; a declaration it cannot split
is skipped.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

from . import paths

import skeleton  # noqa: E402  (tools/gen on sys.path, see paths.py)

_TOKEN = re.compile(r'\s+|(?P<id>[A-Za-z_]\w*)|(?P<num>\d[\w.\']*)|'
                    r'(?P<op>::|->|\.\.\.|&&|\|\||<=>|[<>=!]=|<<=|>>=|[{}()\[\];,<>&*~=:.+\-/%!?|^@#])')
_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
SPECIFIERS = frozenset('static virtual inline constexpr consteval explicit friend extern mutable thread_local'.split())
QUALIFIERS = frozenset('const noexcept override final volatile &'.split())


@dataclass
class Param:
    type: str
    name: str
    default: str | None = None


@dataclass
class Func:
    name: str
    ret: str                 # '' for constructors and destructors
    params: list
    static: bool = False
    virtual: bool = False
    const: bool = False
    template: bool = False
    pure: bool = False
    defined: bool = False    # an inline body in the header
    access: str = 'public'
    owner: str = ''          # class simple name, '' for a free function
    header: str = ''
    line: int = 0
    ns: tuple = ()           # the enclosing namespace (free functions) or class path (members)

    @property
    def min_arity(self):
        return sum(1 for p in self.params if p.default is None and not p.type.endswith('...'))

    @property
    def max_arity(self):
        return 10 ** 6 if any(p.type.endswith('...') for p in self.params) else len(self.params)

    def signature(self):
        ps = ', '.join(p.type + (' ' + p.name if p.name else '') + (' = ' + p.default if p.default else '') for p in self.params)
        return f'{self.ret + " " if self.ret else ""}{self.owner + "::" if self.owner else ""}{self.name}({ps})'


@dataclass
class ClassDecl:
    name: str
    qual: tuple              # namespace path + outer classes + name
    header: str              # include path (aion/gameserver/...)
    bases: list = field(default_factory=list)       # base class names as written (template args dropped)
    methods: dict = field(default_factory=dict)     # name -> [Func]
    kind: str = 'class'
    line: int = 0
    usings: set = field(default_factory=set)        # `using Base::name;`: the base overloads stay visible


@dataclass
class EnumDecl:
    name: str
    qual: tuple
    header: str
    constants: list


class HeaderIndex:
    """Classes, enums and free functions of a set of headers, by simple name (the last declaration wins for classes of the same name in
    different namespaces; qual keeps the full path)."""

    def __init__(self, roots=None):
        self.roots = [Path(r) for r in (roots or paths.CPP_INCLUDE_ROOTS)]
        self.classes = {}        # simple name -> ClassDecl
        self.classes_by_qual = {}
        self.enums = {}          # simple name -> EnumDecl
        self.free = {}           # name -> [Func] (namespace scope)
        self.headers = {}        # include path -> text (raw)
        self.scanned = set()

    # -- files ------------------------------------------------------------------------------------------------------------------
    def find(self, include):
        for r in self.roots:
            p = r / include
            if p.is_file():
                return p
        return None

    def read(self, include):
        if include not in self.headers:
            p = self.find(include)
            self.headers[include] = p.read_text(encoding='utf-8-sig', errors='replace') if p else None
        return self.headers[include]

    def includes_of(self, include):
        text = self.read(include)
        return _INCLUDE.findall(text) if text else []

    def include_closure(self, include, limit=4000):
        """every header reachable from `include` through #include "..." (the file itself included)"""
        seen, todo = set(), [include]
        while todo and len(seen) < limit:
            h = todo.pop()
            if h in seen:
                continue
            seen.add(h)
            if self.read(h) is None:
                continue
            todo.extend(i for i in self.includes_of(h) if i not in seen)
        return seen

    # -- scanning ---------------------------------------------------------------------------------------------------------------
    def scan(self, include):
        if include in self.scanned:
            return
        self.scanned.add(include)
        text = self.read(include)
        if text is None:
            return
        _Scanner(self, include, text).run()

    def lookup(self, cls, name, _seen=None):
        """[Func] named `name` declared in class `cls` or, failing that, its nearest base that declares it (C++ name hiding)"""
        c = self.classes.get(cls)
        if c is None:
            return []
        _seen = _seen or set()
        _seen.add(cls)
        if name in c.methods:
            own = list(c.methods[name])
            if name in c.usings:
                for b in c.bases:
                    bn = b.rpartition('::')[2]
                    if bn not in _seen:
                        have = {tuple(p.type for p in g.params) for g in own}
                        own += [f for f in self.lookup(bn, name, _seen) if tuple(p.type for p in f.params) not in have]
            return own
        for b in c.bases:
            bn = b.rpartition('::')[2]
            if bn in _seen:
                continue
            r = self.lookup(bn, name, _seen)
            if r:
                return r
        return []

    def owner_of(self, cls, name, _seen=None):
        """the class (simple name) whose declaration of `name` a call on `cls` finds, or None"""
        c = self.classes.get(cls)
        if c is None:
            return None
        if name in c.methods:
            return cls
        _seen = _seen or set()
        _seen.add(cls)
        for b in c.bases:
            bn = b.rpartition('::')[2]
            if bn not in _seen:
                r = self.owner_of(bn, name, _seen)
                if r:
                    return r
        return None

    def is_subclass(self, cls, base, _seen=None):
        if cls == base:
            return True
        c = self.classes.get(cls)
        if c is None:
            return False
        _seen = _seen or set()
        _seen.add(cls)
        return any(self.is_subclass(b.rpartition('::')[2], base, _seen) for b in c.bases if b.rpartition('::')[2] not in _seen)


def _tokens(text):
    out = []
    line = 1
    pos = 0
    for m in _TOKEN.finditer(text):
        s = m.group(0)
        if m.start() != pos:        # unknown character: skip it
            pass
        pos = m.end()
        if m.lastgroup is None:
            line += s.count('\n')
            continue
        out.append((s, line))
    return out


def render(toks):
    """C++ tokens -> type text: `runtime::Ptr<model::X>`, `const std::vector<const T*>&`"""
    out = ''
    for t in toks:
        if not out:
            out = t
        elif t in ('::', '<', '>', ',', '&', '*', '&&', '...', ')', ']', '[') or out.endswith(('::', '<', '(', '[')):
            out += t + (' ' if t == ',' else '')
        else:
            out += ' ' + t
    return out.replace(' ,', ',').strip()


class _Scanner:
    def __init__(self, index, include, text):
        self.index = index
        self.include = include
        self.raw = text
        self.toks = _tokens(skeleton._strip_cpp(text))
        self.match = {}
        st = []
        for i, (t, _) in enumerate(self.toks):
            if t in ('(', '[', '{'):
                st.append(i)
            elif t in (')', ']', '}') and st:
                j = st.pop()
                self.match[i] = j
                self.match[j] = i

    def t(self, i):
        return self.toks[i][0] if 0 <= i < len(self.toks) else ''

    def run(self):
        self.scope(0, len(self.toks), (), None)

    def skip_angle(self, i):
        """i at '<': index after the matching '>' (parentheses skipped)"""
        depth = 0
        n = len(self.toks)
        while i < n:
            t = self.t(i)
            if t in ('(', '[', '{'):
                i = self.match.get(i, i)
            elif t == '<':
                depth += 1
            elif t == '>':
                depth -= 1
                if depth == 0:
                    return i + 1
            elif t == ';':
                return i
            i += 1
        return i

    def scope(self, i, end, ns, cls):
        """declarations in [i, end): namespace scope when cls is None, else the body of ClassDecl cls"""
        access = 'private' if cls is not None and cls.kind == 'class' else 'public'
        template = False
        while i < end:
            t = self.t(i)
            if t == ';':
                i += 1
                continue
            if t in ('public', 'private', 'protected') and self.t(i + 1) == ':':
                access = t
                i += 2
                continue
            if t == 'namespace':
                j = i + 1
                names = []
                while self.t(j) not in ('{', ';', '='):
                    if self.t(j) not in ('::', 'inline'):
                        names.append(self.t(j))
                    j += 1
                if self.t(j) == '{':
                    close = self.match.get(j, end)
                    self.scope(j + 1, close, ns + tuple(names), None)
                    i = close + 1
                else:
                    i = self.to_semi(j) + 1
                continue
            if t == 'template':
                if self.t(i + 1) == '<':
                    i = self.skip_angle(i + 1)
                else:
                    i += 1
                template = True
                continue
            if t in ('using', 'static_assert', 'typedef', 'friend'):
                semi = self.to_semi(i)
                if t == 'using' and cls is not None and semi - i >= 4 and self.t(semi - 2) == '::':
                    cls.usings.add(self.t(semi - 1))
                i = semi + 1
                template = False
                continue
            if t.startswith('AION_') and t.isupper() or re.fullmatch(r'AION_[A-Z0-9_]+', t):
                i += 1
                if self.t(i) == '(':
                    i = self.match.get(i, i) + 1
                continue
            if t in ('class', 'struct', 'union') or (t == 'enum'):
                i = self.type_decl(i, end, ns, cls, template)
                template = False
                continue
            i = self.member(i, end, ns, cls, access, template)
            template = False
        return i

    def to_semi(self, i):
        n = len(self.toks)
        while i < n and self.t(i) != ';':
            if self.t(i) in ('(', '[', '{'):
                i = self.match.get(i, i)
            i += 1
        return i

    def type_decl(self, i, end, ns, cls, template):
        kind = self.t(i)
        j = i + 1
        if kind == 'enum' and self.t(j) in ('class', 'struct'):
            j += 1
        while self.t(j) == '[' or self.t(j).startswith('alignas'):
            j = self.match.get(j, j) + 1
        name = self.t(j) if re.fullmatch(r'[A-Za-z_]\w*', self.t(j)) and self.t(j) not in ('final',) else ''
        # find '{' or ';' of this declaration
        k = j
        while k < end and self.t(k) not in ('{', ';'):
            if self.t(k) == '(':
                k = self.match.get(k, k)
            k += 1
        if self.t(k) == ';' or k >= end:
            # forward declaration, or a variable of an elaborated type
            return k + 1
        close = self.match.get(k, end)
        outer = cls.qual if cls is not None else ns
        if kind == 'enum':
            consts = []
            m = k + 1
            expect = True
            while m < close:
                tt = self.t(m)
                if expect and re.fullmatch(r'[A-Za-z_]\w*', tt):
                    consts.append(tt)
                    expect = False
                elif tt == ',':
                    expect = True
                elif tt in ('(', '{', '['):
                    m = self.match.get(m, m)
                m += 1
            if name:
                e = EnumDecl(name, outer + (name,), self.include, consts)
                self.index.enums[name] = e
                if cls is not None:
                    self.index.enums.setdefault(cls.name + '::' + name, e)
        else:
            bases = []
            m = j + 1
            if self.t(m) == 'final':
                m += 1
            if self.t(m) == ':':
                cur = []
                m += 1
                while m < k:
                    tt = self.t(m)
                    if tt == '<':
                        m = self.skip_angle(m)
                        continue
                    if tt == ',':
                        if cur:
                            bases.append('::'.join(cur))
                        cur = []
                    elif tt not in ('public', 'protected', 'private', 'virtual', '::'):
                        cur.append(tt)
                    m += 1
                if cur:
                    bases.append('::'.join(cur))
            if name and not template:
                c = ClassDecl(name, outer + (name,), self.include, bases, {}, kind, self.toks[i][1])
                if cls is None or name not in self.index.classes:
                    self.index.classes[name] = c
                self.index.classes_by_qual[c.qual] = c
                self.scope(k + 1, close, ns, c)
                self.splice_member_blocks(c, k, close)
        # `} name;` / `};`
        m = close + 1
        return self.to_semi(m) + 1 if self.t(m) != ';' else m + 1

    def splice_member_blocks(self, c, open_i, close_i):
        """#include "X.gen.h" / "X.xml.inc" inside the class body (blanked by _strip_cpp): scan it as members of c"""
        lines = self.raw.split('\n')
        first, last = self.toks[open_i][1], self.toks[close_i][1]
        for ln in lines[first - 1:last]:
            m = _INCLUDE.match(ln)
            if m and (m.group(1).endswith('.gen.h') or m.group(1).endswith('.xml.inc')):
                text = self.index.read(m.group(1))
                if text:
                    sub = _Scanner(self.index, m.group(1), text)
                    sub.scope(0, len(sub.toks), c.qual[:-1], c)

    def member(self, i, end, ns, cls, access, template):
        """one declaration starting at i; returns the index after it"""
        n = len(self.toks)
        j = i
        paren = -1
        angle = 0
        while j < end:
            t = self.t(j)
            if t == ';' and angle <= 0:
                break
            if t == '<':
                # operator< and comparisons do not occur in declarations we care about
                angle += 1
            elif t == '>':
                angle -= 1
            elif t == '(':
                if paren < 0 and angle <= 0:
                    paren = j
                j = self.match.get(j, j) + 1
                if paren >= 0 and paren == self.match.get(j - 1, -2):
                    # after the parameter list: qualifiers, then '{' body, ':' initializers, '= ...;' or ';'
                    k = j
                    while self.t(k) in QUALIFIERS or self.t(k) in ('&&',) or self.t(k) == '->':
                        if self.t(k) == '->':
                            k += 1
                            while self.t(k) not in ('{', ';', '=') and k < end:
                                if self.t(k) == '<':
                                    k = self.skip_angle(k)
                                    continue
                                k += 1
                            break
                        if self.t(k) == 'noexcept' and self.t(k + 1) == '(':
                            k = self.match.get(k + 1, k + 1)
                        k += 1
                    if self.t(k) == ':' and self.t(k + 1) != ':':
                        k += 1
                        while k < end:
                            tt = self.t(k)
                            if tt in ('(', '{') and (self.t(k - 1) not in (',', ':') and re.fullmatch(r'[\w>]+', self.t(k - 1) or '')):
                                k = self.match.get(k, k) + 1
                                continue
                            if tt == '{':
                                break
                            k += 1
                    if self.t(k) == '{':
                        close = self.match.get(k, end)
                        self.record(i, paren, k, ns, cls, access, template, defined=True)
                        return close + 1
                    if self.t(k) == '=':
                        semi = self.to_semi(k)
                        pure = self.t(k + 1) == '0'
                        self.record(i, paren, k, ns, cls, access, template, pure=pure, defined=self.t(k + 1) in ('default', 'delete'))
                        return semi + 1
                    if self.t(k) == ';':
                        self.record(i, paren, k, ns, cls, access, template)
                        return k + 1
                    j = k
                continue
            elif t in ('{', '['):
                j = self.match.get(j, j) + 1
                continue
            j += 1
        return j + 1

    def record(self, start, paren, after, ns, cls, access, template, pure=False, defined=False):
        head = [self.t(k) for k in range(start, paren)]
        # drop attributes [[...]] and specifiers
        clean = []
        k = 0
        static = virtual = False
        while k < len(head):
            t = head[k]
            if t == '[' and k + 1 < len(head) and head[k + 1] == '[':
                depth = 0
                while k < len(head):
                    if head[k] == '[':
                        depth += 1
                    elif head[k] == ']':
                        depth -= 1
                        if depth == 0:
                            break
                    k += 1
                k += 1
                continue
            if t in SPECIFIERS:
                static |= t == 'static'
                virtual |= t == 'virtual'
                k += 1
                continue
            clean.append(t)
            k += 1
        if not clean:
            return
        if 'operator' in clean:
            return
        name = clean[-1]
        if not re.fullmatch(r'~?[A-Za-z_]\w*', name):
            if len(clean) >= 2 and clean[-2] == '~':
                name = '~' + name
            else:
                return
        ret_toks = clean[:-1]
        if ret_toks and ret_toks[-1] == '~':
            ret_toks = ret_toks[:-1]
            name = '~' + name
        # qualified out-of-line definitions (A::f) are not declarations of this scope
        if ret_toks and ret_toks[-1] == '::':
            return
        params = self.params(paren + 1, self.match.get(paren, paren))
        quals = [self.t(k) for k in range(self.match.get(paren, paren) + 1, after)]
        f = Func(name, render(ret_toks), params, static, virtual or 'override' in quals, 'const' in quals, template, pure, defined, access,
                 cls.name if cls is not None else '', self.include, self.toks[start][1], cls.qual if cls is not None else ns)
        if cls is not None:
            cls.methods.setdefault(name, []).append(f)
        else:
            self.index.free.setdefault(name, []).append(f)

    def params(self, i, end):
        out = []
        cur = []
        j = i
        angle = 0
        while j < end:
            t = self.t(j)
            if t in ('(', '[', '{'):
                close = self.match.get(j, j)
                cur.extend(self.t(k) for k in range(j, close + 1))
                j = close + 1
                continue
            if t == '<':
                angle += 1
            elif t == '>':
                angle -= 1
            if t == ',' and angle <= 0:
                out.append(cur)
                cur = []
            else:
                cur.append(t)
            j += 1
        if cur:
            out.append(cur)
        res = []
        for p in out:
            if p == ['void']:
                continue
            default = None
            if '=' in p:
                e = p.index('=')
                default = render(p[e + 1:])
                p = p[:e]
            name = ''
            if len(p) >= 2 and re.fullmatch(r'[A-Za-z_]\w*', p[-1]) and p[-2] not in ('::',) and p[-1] not in (
                    'int32_t', 'int64_t', 'int8_t', 'int16_t', 'bool', 'float', 'double', 'int', 'long', 'char', 'const'):
                name = p[-1]
                p = p[:-1]
            res.append(Param(render(p), name, default))
        return res
