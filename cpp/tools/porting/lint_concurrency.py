"""lint_concurrency.py: concurrency lint for the C++ game server (design runtime-architecture.md §12.2 L1-L19, §21 L20).

Python 3.12 stdlib only. A small C++ scanner (comments, strings, raw strings, preprocessor lines, brackets, namespaces, classes, members,
functions, lambdas) feeds rules that compare the ported code with cpp/game-server/generated/concurrency/fieldmap.json.

Usage
    python lint_concurrency.py [--fieldmap FILE] [--no-fieldmap] [--rules L1,L5,...] [--cycles] [--json] [--werror] PATH...
PATH is a file or a directory (scanned recursively for .h .hpp .cpp .cc .cxx .inl .ipp). Output lines are
    path:line:col: error|warning: Lnn: message
sorted by path and line. Exit status: 0 clean, 1 errors (or warnings with --werror), 2 usage or input errors.

Areas: kernel code is every file under runtime/ plus the P4-02b services built on leaf mutexes and their own threads (services/cron/,
utils/idfactory/, utils/cron/, utils/ThreadPoolManager.*). The kernel implements the wrappers and re-architects its Java classes, so L1, L2,
L3, L6, L7, L9, L11, L14 and the placement part of L18 do not apply there; network/ may hold RankedMutex too.
L2 compares member types for K3/K4 classes only (K5 members are plain by definition; config fields follow CONVENTIONS).

Shared (K4-like) classes: C++ classes mapped to a K3/K4 Java class of fieldmap.json, and classes deriving (directly or through another
scanned class) from RefCounted, OwnedPart or Immortal. Mapping C++ -> Java: namespace aion::gameserver::handlers::<pkg> -> <pkg>,
aion::gameserver::<pkg> -> com.aionemu.gameserver.<pkg>, keyword segments with a trailing '_' lose it, nested classes Outer::Inner, generated
callback structs by their cppName; a '// fieldmap-class: <FQN>' comment on or above the class line overrides the mapping.
Packet classes: fieldmap K2, or classes deriving AionServerPacket/AionClientPacket/Ls*/Cs* packet bases/BaseServerPacket/BaseClientPacket.

Rules
    L1   members of shared classes are const, Final<>, Field<>, collection/Atomic shims, Array, parts (const unique_ptr, PartSlot, PartMap,
         PartList), OwnerRef, SelfOrRef, Monitor/StampedLock/Semaphore, PinnedCallback, std::atomic (L14 warns), or waived
    L2   member types of mapped classes equal fieldmap.json (declared fields and generated capture members; missing instance members
         are reported); '// fieldmap: <reason>' waives
    L3   no std::string_view, std::span, Ptr<, reference, raw pointer to a RefCounted class or const std::string& members in shared classes
    L4   namespace-scope and static data members hold thread-safe types (const/constexpr, std::atomic, Field, shims, Atomic*, Monitor,
         mutexes, ConfigValue, std::once_flag, loggers)
    L5   lambdas passed to schedule*/execute*/submit*/deferred, PinnedCallback and the observer/request/cron/event APIs: no [&]/[=];
         unpinned lambdas are captureless; 'this'/'&x' captures are in the pin list; by-copy captures are not Ptr/raw pointers/references.
         (b) QuiescentScope opened at the top level of the function body; no Ptr/T& locals declared before it; range-for variables inside
         it are Ref; functions with T& parameters need '// quiescent-safe: <why>'; quiescentPoint() outside a QuiescentScope warns
    L6   no std::thread/std::jthread/std::async/.detach() outside runtime/
    L7   SYNCHRONIZED and lock() counts per method equal the Java synchronized/lock() counts of fieldmap.json
    L8   banned C functions (strtok, localtime, gmtime, asctime, ctime, rand, srand, setlocale, getenv) and mutable function-local statics
         (getInstance()-style singletons of class type and const/atomic statics are allowed)
    L9   destructors of shared classes: no dereference (->), container or get() calls, SYNCHRONIZED, packets, getInstance()
    L10  packet members are not Ptr/references/raw pointers to RefCounted; no sendPacket in writeImpl; recipients() overridden exactly by the
         per-recipient packets (design 8.3); packets with write-time side effects (§8.5) are not cached in statics
    L11  no stored iterators (members of iterator type, or 'x.begin()' kept in a variable when x is not a local std container);
         'auto it = x.iterator()' is allowed
    L12  warning: DAO calls and Future get() inside SYNCHRONIZED blocks or compute callbacks ('// lockdep: <reason>' waives)
    L13  Immortal only on singletons (getInstance), static data, quest handlers and commands; per-run services are never Immortal
    L14  warning: std::atomic< in game code (outside runtime/ and configs/)
    L15  no thread_local holding Ref/Ptr/pointers/references/views
    L16  (--cycles) every cycle edge of fieldmap.json has a cycles.toml resolution; part classes hold no Ref to their owner
    L17  KnownList add( only inside KnownList::addPair
    L18  RankedMutex/LeafMutex only in runtime/ and network/; while such a mutex is held (RAII guard scope): no callable parameter invoked,
         no blocking call (Future get, join, acquire, sleep, BlockingRegion, DAO) and no SYNCHRONIZED
    L19  K5 (confined) classes are not stored in shared or packet members, nor captured by stored lambdas
    L20  compute/computeIfAbsent/computeIfPresent/merge callbacks do not write the same map (put/remove/compute*/merge/replace/clear)

Waivers (comment on the finding's line or the line above, reason mandatory; a waiver without a reason is reported as W0):
    // confined: <reason>        L1 L3 L19
    // fieldmap: <reason>        L1 L2 L3
    // lockdep: <reason>         L12
    // quiescent-safe: <reason>  L5
    // lint: L5,L12 <reason>     the listed rules
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field

HERE = os.path.dirname(os.path.abspath(__file__))
CPP_ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
DEFAULT_FIELDMAP = os.path.join(CPP_ROOT, 'game-server', 'generated', 'concurrency', 'fieldmap.json')
EXTS = ('.h', '.hpp', '.cpp', '.cc', '.cxx', '.inl', '.ipp')
ALL_RULES = tuple(f'L{i}' for i in range(1, 21))
# kernel code (runtime kernel and the P4-02b services built on leaf mutexes and their own threads): 'runtime' area
KERNEL_PATHS = ('/runtime/', '/services/cron/', '/utils/idfactory/', '/utils/cron/', '/utils/ThreadPoolManager.')


class LintError(Exception):
    pass


# ----------------------------------------------------------------------------------------------------------------------------------
# Lexer
# ----------------------------------------------------------------------------------------------------------------------------------

IDENT, NUMBER, STRING, CHAR, OP = 'ident', 'number', 'string', 'char', 'op'
_TOKEN_RE = re.compile(r'''
    (?P<ws>[ \t\r\f\v]+)
  | (?P<nl>\n)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<raw>(?:u8|u|U|L)?R"(?P<delim>[^()\\ \n]{0,16})\(.*?\)(?P=delim)")
  | (?P<str>(?:u8|u|U|L)?"(?:[^"\\\n]|\\.)*")
  | (?P<chr>(?:u8|u|U|L)?'(?:[^'\\\n]|\\.)+')
  | (?P<num>\.?\d(?:[\w.']|[eEpP][+-])*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op>::|->\*|->|\.\.\.|<=>|<<=|<=|>=|==|!=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|[{}()\[\];,.<>=!~?:+\-*/%&|^#@\\$])
''', re.X | re.S)


class Source:
    """Token stream of one C++ file with comments by line and bracket matches."""

    def __init__(self, path, text, display=None):
        self.path = path
        self.display = display or path
        self.text = text
        self.kind, self.tok, self.line, self.col = [], [], [], []
        self.comments = {}
        self.own_line_comments = set()
        self._lex()
        self.match = [-1] * len(self.tok)
        stack = []
        pairs = {')': '(', ']': '[', '}': '{'}
        for i, x in enumerate(self.tok):
            if self.kind[i] != OP:
                continue
            if x in '([{':
                stack.append(i)
            elif x in ')]}':
                while stack and self.tok[stack[-1]] != pairs[x]:
                    stack.pop()  # tolerate unbalanced macro soup
                if stack:
                    o = stack.pop()
                    self.match[o] = i
                    self.match[i] = o
        self.lines = text.split('\n')

    def _lex(self):
        text = self.text
        pos = 0
        line = 1
        line_start = 0
        at_line_start = True
        n = len(text)
        while pos < n:
            if at_line_start:
                j = pos
                while j < n and text[j] in ' \t':
                    j += 1
                if j < n and text[j] == '#':
                    # preprocessor directive with continuations; comments inside are kept
                    k = j
                    while k < n:
                        e = text.find('\n', k)
                        if e < 0:
                            e = n
                        seg = text[k:e]
                        c = seg.find('//')
                        if c >= 0:
                            self.comments.setdefault(line, []).append(seg[c + 2:].strip())
                        if seg.rstrip('\r').endswith('\\') and e < n:
                            k = e + 1
                            line += 1
                            continue
                        k = e
                        break
                    pos = k
                    at_line_start = False
                    continue
            m = _TOKEN_RE.match(text, pos)
            if m is None:
                raise LintError(f'{self.display}:{line}:{pos - line_start + 1}: cannot tokenize {text[pos]!r}')
            g = m.lastgroup
            s = m.group(0)
            if g == 'nl':
                line += 1
                line_start = m.end()
                at_line_start = True
            elif g == 'ws':
                pass
            elif g in ('lc', 'bc'):
                body = s[2:] if g == 'lc' else s[2:-2]
                self.comments.setdefault(line, []).append(body.strip())
                if not self.line or self.line[-1] != line:
                    self.own_line_comments.add(line)
                nls = s.count('\n')
                if nls:
                    line += nls
                    line_start = pos + s.rfind('\n') + 1
                at_line_start = False if g == 'lc' else at_line_start and nls == 0
            else:
                kind = {'raw': STRING, 'str': STRING, 'chr': CHAR, 'num': NUMBER, 'id': IDENT, 'op': OP}.get(g, OP)
                if g == 'delim':
                    kind = STRING
                self.kind.append(kind)
                self.tok.append(s)
                self.line.append(line)
                self.col.append(pos - line_start + 1)
                nls = s.count('\n')
                if nls:
                    line += nls
                    line_start = pos + s.rfind('\n') + 1
                at_line_start = False
            pos = m.end()

    def text_of(self, s, e):
        """Tokens [s, e) joined with C++-ish spacing."""
        out = []
        prev = None
        for i in range(s, e):
            x = self.tok[i]
            if prev is not None:
                if (self.kind[i] == IDENT or self.kind[i] == NUMBER) and (self.kind[prev] in (IDENT, NUMBER) or self.tok[prev] in (',', '>')):
                    out.append(' ')
                elif self.tok[prev] == ',':
                    out.append(' ')
            out.append(x)
            prev = i
        return ''.join(out)

    def comment_near(self, line):
        """Comments on the line itself, plus comments standing alone on the line above."""
        above = self.comments.get(line - 1, []) if (line - 1) in self.own_line_comments else []
        return self.comments.get(line, []) + above


# ----------------------------------------------------------------------------------------------------------------------------------
# Structure
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass
class Member:
    name: str
    type: str
    line: int
    col: int
    specifiers: set
    cls: 'CppClass'
    tok: int
    array: bool = False


@dataclass
class Function:
    name: str
    qual: list
    cls: 'CppClass'
    src: Source
    params: tuple
    body: tuple
    line: int
    col: int
    dtor: bool = False
    static_member: bool = False

    @property
    def display(self):
        q = '::'.join(self.qual + [self.name]) if self.qual else (f'{self.cls.qualname}::{self.name}' if self.cls else self.name)
        return q


@dataclass
class CppClass:
    name: str
    qualname: str
    namespace: list
    bases: list
    src: Source
    line: int
    col: int
    body: tuple
    parent: 'CppClass' = None
    members: list = field(default_factory=list)
    functions: list = field(default_factory=list)
    key: str = 'class'
    declared: set = field(default_factory=set)
    static_declared: set = field(default_factory=set)

    @property
    def display(self):
        return '::'.join(self.namespace + [self.qualname])


@dataclass
class GlobalVar:
    name: str
    type: str
    line: int
    col: int
    specifiers: set
    src: Source
    namespace: list


@dataclass
class Lambda:
    cap: tuple
    params: tuple
    body: tuple
    line: int
    col: int


_ACCESS = frozenset(('public', 'private', 'protected'))
_SPECIFIERS = frozenset(('static', 'inline', 'constexpr', 'constinit', 'mutable', 'thread_local', 'extern', 'virtual', 'explicit', 'friend',
                         'typename', 'volatile', 'register'))
_KEYWORDS = frozenset(('return', 'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'default', 'break', 'continue', 'goto', 'throw',
                       'try', 'catch', 'new', 'delete', 'sizeof', 'alignof', 'decltype', 'typeid', 'static_cast', 'dynamic_cast',
                       'const_cast', 'reinterpret_cast', 'co_return', 'co_await', 'co_yield', 'using', 'namespace', 'operator',
                       'template', 'typedef', 'this', 'true', 'false', 'nullptr', 'public', 'private', 'protected', 'noexcept',
                       'requires', 'concept', 'static_assert', 'class', 'struct', 'enum', 'union'))


class Parser:
    def __init__(self, src):
        self.src = src
        self.classes = []
        self.functions = []
        self.globals = []

    def parse(self):
        self._scope(0, len(self.src.tok), [], None)
        return self

    def _is_macro_line(self, i, end):
        t, k, ln = self.src.tok, self.src.kind, self.src.line
        if k[i] != IDENT or not re.fullmatch(r'[A-Z][A-Z0-9_]*', t[i]) or t[i] in ('SYNCHRONIZED',):
            return None
        j = i + 1
        if j < end and t[j] == '(' and self.src.match[j] > 0:
            j = self.src.match[j] + 1
        if j >= end or ln[j] > ln[j - 1] or t[j] in ('}', 'public', 'private', 'protected'):
            if j < end and t[j] == ';':
                return None
            return j
        return None

    def _scope(self, i, end, ns, cls):
        src = self.src
        t, k, m = src.tok, src.kind, src.match
        while i < end:
            x = t[i]
            if x == ';':
                i += 1
                continue
            if x in _ACCESS and i + 1 < end and t[i + 1] == ':':
                i += 2
                continue
            if x == 'namespace':
                j = i + 1
                names = []
                while j < end and t[j] not in ('{', ';', '='):
                    if k[j] == IDENT and t[j] != 'inline':
                        names.append(t[j])
                    j += 1
                if j < end and t[j] == '{' and m[j] > 0:
                    self._scope(j + 1, m[j], ns + names, None)
                    i = m[j] + 1
                    continue
                while j < end and t[j] != ';':
                    j += 1
                i = j + 1
                continue
            if x == 'extern' and i + 2 < end and k[i + 1] == STRING and t[i + 2] == '{' and m[i + 2] > 0:
                self._scope(i + 3, m[i + 2], ns, cls)
                i = m[i + 2] + 1
                continue
            mac = self._is_macro_line(i, end)
            if mac is not None:
                i = mac
                continue
            i = self._statement(i, end, ns, cls)

    def _skip_angle(self, j, end):
        """j at '<': index after the matching '>'."""
        t, m = self.src.tok, self.src.match
        depth = 0
        while j < end:
            x = t[j]
            if x == '<':
                depth += 1
            elif x == '>':
                depth -= 1
                if depth == 0:
                    return j + 1
            elif x in ('(', '[', '{') and m[j] > 0:
                j = m[j]
            elif x in (';', '}'):
                return j
            j += 1
        return j

    def _statement(self, i, end, ns, cls):
        src = self.src
        t, k, m = src.tok, src.kind, src.match
        s = i
        templated = False
        while i < end and t[i] == 'template' and i + 1 < end and t[i + 1] == '<':
            i = self._skip_angle(i + 1, end)
            templated = True
        if i < end and t[i] in ('using', 'typedef', 'friend', 'static_assert', 'concept'):
            j = i
            while j < end and t[j] != ';':
                if t[j] in ('(', '[', '{') and m[j] > 0:
                    j = m[j]
                j += 1
            return j + 1
        j = i
        paren = None
        eq = False
        class_kw = None
        while j < end:
            x = t[j]
            if x == '<' and j > i and k[j - 1] == IDENT and not eq:
                j = self._skip_angle(j, end)
                continue
            if x in ('(', '['):
                if x == '[' and j + 1 < end and t[j + 1] == '[':
                    j = m[j] + 1 if m[j] > 0 else j + 1
                    continue
                if x == '(' and paren is None and not eq and class_kw is None:
                    paren = j
                j = (m[j] + 1) if m[j] > 0 else j + 1
                continue
            if x == '=' and not eq:
                eq = True
            if x in ('class', 'struct', 'union', 'enum') and class_kw is None and paren is None and not eq:
                class_kw = j
            if x == ';':
                if paren is None:
                    self._declaration(s, i, j, ns, cls, templated)
                else:
                    self._maybe_function_decl(s, i, j, paren, ns, cls)
                return j + 1
            if x == '{':
                if class_kw is not None and paren is None and not eq:
                    return self._class(class_kw, j, end, ns, cls)
                if paren is not None and not eq:
                    body = self._function_body(paren, end)
                    if body is not None:
                        self._function(i, paren, body, ns, cls)
                        return m[body] + 1
                if m[j] > 0:
                    j = m[j] + 1
                    continue
            if x == '}':
                return j + 1
            j += 1
        return end

    def _function_body(self, paren, end):
        src = self.src
        t, k, m = src.tok, src.kind, src.match
        if m[paren] < 0:
            return None
        j = m[paren] + 1
        init = False
        while j < end:
            x = t[j]
            if x == '{':
                if init and j > 0 and (k[j - 1] == IDENT or t[j - 1] == '>'):
                    j = m[j] + 1 if m[j] > 0 else j + 1
                    continue
                return j
            if x in (';', '='):
                return None if not (x == '=' and init) else None
            if x == ':' and not init:
                init = True
            if x in ('(', '[') and m[j] > 0:
                j = m[j] + 1
                continue
            if x == '<' and k[j - 1] == IDENT:
                j = self._skip_angle(j, end)
                continue
            j += 1
        return None

    def _name_before(self, paren):
        t, k = self.src.tok, self.src.kind
        j = paren - 1
        if j < 0:
            return None, [], False
        if t[j] == '>':
            depth = 0
            while j >= 0:
                if t[j] == '>':
                    depth += 1
                elif t[j] == '<':
                    depth -= 1
                    if depth == 0:
                        j -= 1
                        break
                j -= 1
        name = None
        dtor = False
        if k[j] == IDENT:
            name = t[j]
            if j > 0 and t[j - 1] == '~':
                dtor = True
                j -= 1
            if j > 1 and t[j - 1] == 'operator':
                name = 'operator' + name
                j -= 1
        elif t[j] in ('()', ')', ']', '=', '==', '<', '+', '-', '*', '/', '!=', '<<', '[', '(', '->', '!', '~', '&', '|', '^', '%', '<=', '>=', '<=>', '+=', '-='):
            q = j
            while q >= 0 and t[q] != 'operator' and paren - q < 4:
                q -= 1
            if q >= 0 and t[q] == 'operator':
                name = 'operator' + ''.join(t[q + 1:paren])
                j = q
        if name is None:
            return None, [], False
        qual = []
        q = j - 1
        while q >= 1 and t[q] == '::' and k[q - 1] == IDENT:
            qual.insert(0, t[q - 1])
            q -= 2
        return name, qual, dtor

    def _function(self, start, paren, body, ns, cls):
        src = self.src
        name, qual, dtor = self._name_before(paren)
        if name is None:
            return
        static = 'static' in src.tok[start:paren]
        fn = Function(name, qual, cls, src, (paren + 1, src.match[paren]), (body, src.match[body]), src.line[paren - 1], src.col[paren - 1],
                      dtor, static)
        fn.namespace = ns
        self.functions.append(fn)
        if cls is not None and not qual:
            cls.functions.append(fn)

    def _maybe_function_decl(self, s, i, j, paren, ns, cls):
        """Records the names of member function declarations without a body (L13 getInstance)."""
        if cls is None:
            return
        name, qual, dtor = self._name_before(paren)
        if name is not None and not qual:
            cls.declared.add(name)
            if 'static' in self.src.tok[s:paren]:
                cls.static_declared.add(name)

    def _class(self, kw, brace, end, ns, parent):
        src = self.src
        t, k, m = src.tok, src.kind, src.match
        key = t[kw]
        if key == 'enum':
            close = m[brace] if m[brace] > 0 else end
            j = close + 1
            while j < end and t[j] != ';':
                j += 1
            return j + 1
        j = kw + 1
        name = None
        while j < brace:
            if t[j] == '[' and j + 1 < brace and t[j + 1] == '[' and m[j] > 0:
                j = m[j] + 1
                continue
            if k[j] == IDENT and t[j] not in ('final', 'alignas'):
                name = t[j]
                j += 1
                if j < brace and t[j] == '<':
                    j = self._skip_angle(j, brace)
                while j < brace and t[j] == '::' and k[j + 1] == IDENT:
                    name = t[j + 1]
                    j += 2
                break
            j += 1
        bases = []
        while j < brace and t[j] != ':':
            j += 1
        if j < brace:
            cur = []
            depth = 0
            for q in range(j + 1, brace):
                x = t[q]
                if x == '<':
                    depth += 1
                elif x == '>':
                    depth -= 1
                if x == ',' and depth == 0:
                    bases.append(cur)
                    cur = []
                    continue
                cur.append(q)
            if cur:
                bases.append(cur)
            bases = [src.text_of(b[0], b[-1] + 1) for b in bases if b]
            bases = [re.sub(r'^(?:(?:public|private|protected|virtual)\s+)+', '', b) for b in bases]
        close = m[brace] if m[brace] > 0 else end
        if name is None:
            name = f'<anonymous@{src.line[kw]}>'
        qual = f'{parent.qualname}::{name}' if parent is not None else name
        cls = CppClass(name, qual, list(ns), bases, src, src.line[kw], src.col[kw], (brace, close), parent, key=key)
        self.classes.append(cls)
        self._scope(brace + 1, close, ns, cls)
        j = close + 1
        while j < end and t[j] != ';':
            if t[j] in ('{', '}'):
                return j
            j += 1
        return j + 1

    def _declaration(self, s, i, j, ns, cls, templated):
        """Member or namespace-scope variable declaration in tokens [i, j) (';' at j)."""
        src = self.src
        t, k, m = src.tok, src.kind, src.match
        toks = list(range(i, j))
        if not toks:
            return
        if t[i] in ('class', 'struct', 'union', 'enum') and all(
                k[q] == IDENT or t[q] == '::' or (t[i] == 'enum' and t[q] == ':') for q in toks[1:]):
            return  # forward declaration (an opaque enum declaration may name its underlying type)
        specs = set()
        q = i
        while q < j and t[q] in _SPECIFIERS | {'const'} and not (t[q] == 'const' and False):
            if t[q] != 'const':
                specs.add(t[q])
                q += 1
            else:
                break
        # split declarators at top-level commas
        parts = []
        cur = []
        depth = 0
        p = q
        while p < j:
            x = t[p]
            if x in ('(', '[', '{') and m[p] > 0:
                cur.extend(range(p, m[p] + 1))
                p = m[p] + 1
                continue
            if x == '<' and p > q and k[p - 1] == IDENT:
                e = self._skip_angle(p, j)
                cur.extend(range(p, e))
                p = e
                continue
            if x == ',' and depth == 0:
                parts.append(cur)
                cur = []
                p += 1
                continue
            cur.append(p)
            p += 1
        if cur:
            parts.append(cur)
        if not parts:
            return
        first = parts[0]
        decl = self._split_declarator(first)
        if decl is None:
            return
        type_toks, name_i, array = decl
        type_text = src.text_of(type_toks[0], type_toks[-1] + 1) if type_toks else ''
        if not type_text or type_text in ('return', 'delete', 'goto'):
            return
        if 'operator' in (t[x] for x in first):
            return
        entries = [(name_i, array)]
        for part in parts[1:]:
            d2 = [x for x in part if k[x] == IDENT]
            if d2:
                entries.append((d2[0], False))
        for ni, arr in entries:
            if cls is not None:
                cls.members.append(Member(t[ni], type_text, src.line[ni], src.col[ni], set(specs), cls, ni, arr))
            else:
                self.globals.append(GlobalVar(t[ni], type_text, src.line[ni], src.col[ni], set(specs), src, list(ns)))

    def _split_declarator(self, toks):
        """(type token indexes, name index, is_array) of 'Type name [= init | {init} | [N] | : bits]'."""
        t, k = self.src.tok, self.src.kind
        cut = len(toks)
        for n, q in enumerate(toks):
            if t[q] in ('=', '{', '[', ':') and n > 0:
                cut = n
                break
        head = toks[:cut]
        if len(head) < 2:
            return None
        name_i = head[-1]
        if k[name_i] != IDENT or t[name_i] in _KEYWORDS:
            return None
        array = cut < len(toks) and t[toks[cut]] == '['
        return head[:-1], name_i, array


def find_lambdas(src, s, e):
    """Lambdas in tokens [s, e)."""
    t, k, m = src.tok, src.kind, src.match
    out = []
    i = s
    while i < e:
        if t[i] == '[' and m[i] > 0:
            prev = t[i - 1] if i > 0 else ''
            if i + 1 < e and t[i + 1] == '[':  # attribute
                i = m[i] + 1
                continue
            if not (k[i - 1] == IDENT and prev not in ('return', 'co_return', 'throw', 'case')) and prev not in (')', ']') and not (k[i - 1] in (STRING, NUMBER, CHAR)) and prev != 'operator':
                close = m[i]
                j = close + 1
                params = None
                if j < e and t[j] == '(' and m[j] > 0:
                    params = (j + 1, m[j])
                    j = m[j] + 1
                while j < e and t[j] in ('mutable', 'constexpr', 'noexcept', 'static'):
                    j += 1
                if j < e and t[j] == '->':
                    while j < e and t[j] != '{':
                        if t[j] in ('(', '[') and m[j] > 0:
                            j = m[j]
                        j += 1
                if j < e and t[j] == '{' and m[j] > 0 and (params is not None or j == close + 1 or t[j - 1] in ('mutable', 'noexcept')
                                                         or t[close + 1] == '->'):
                    out.append(Lambda((i + 1, close), params, (j, m[j]), src.line[i], src.col[i]))
        i += 1
    return out


def split_top(src, s, e, sep=','):
    """Token ranges of [s, e) split at top-level separators (brackets respected)."""
    t, m = src.tok, src.match
    out = []
    cur = s
    i = s
    while i < e:
        x = t[i]
        if x in ('(', '[', '{') and m[i] > 0:
            i = m[i] + 1
            continue
        if x == sep:
            out.append((cur, i))
            cur = i + 1
        i += 1
    if cur < e or out:
        out.append((cur, e))
    return [(a, b) for a, b in out if a < b]


def local_declarations(src, s, e):
    """[(name, type text, token index)] of simple declarations 'Type name' in [s, e) (parameters, locals, range-for variables)."""
    t, k = src.tok, src.kind
    out = []
    starts = {s}
    for i in range(s, e):
        if t[i] in ('(', '{', ';', ',', '}', ')') or (t[i] == ':' and i > 0 and t[i - 1] != ':' and (i + 1 >= len(t) or t[i + 1] != ':')):
            starts.add(i + 1)
    for st in sorted(starts):
        i = st
        while i < e and t[i] in ('const', 'static', 'constexpr', 'volatile', 'thread_local', 'inline'):
            i += 1
        if i >= e or k[i] != IDENT or t[i] in _KEYWORDS:
            continue
        j = i
        ok = True
        while True:  # qualified name with template arguments
            if j >= e or k[j] != IDENT or t[j] in _KEYWORDS:
                ok = False
                break
            j += 1
            if j < e and t[j] == '<':
                depth = 0
                while j < e:
                    if t[j] == '<':
                        depth += 1
                    elif t[j] == '>':
                        depth -= 1
                        if depth == 0:
                            j += 1
                            break
                    elif t[j] in (';', '{', '}', '='):
                        ok = False
                        break
                    j += 1
            if ok and j < e and t[j] == '::':
                j += 1
                continue
            break
        if not ok:
            continue
        while j < e and t[j] in ('*', '&', '&&', 'const'):
            j += 1
        if j >= e or k[j] != IDENT or t[j] in _KEYWORDS:
            continue
        if j + 1 < e and t[j + 1] not in ('=', ';', '{', '(', ':', ',', ')', '['):
            continue
        type_text = src.text_of(st, j)
        out.append((t[j], type_text, j))
    return out


# ----------------------------------------------------------------------------------------------------------------------------------
# Fieldmap
# ----------------------------------------------------------------------------------------------------------------------------------

def load_fieldmap(path):
    with open(path, encoding='utf-8') as f:
        data = json.load(f)
    if data.get('format') != 'aion-fieldmap':
        raise LintError(f'{path}: not a fieldmap.json')
    return data


_NS_STRIP = re.compile(r'(?<![\w:])(?:(?!std::)[a-z_][A-Za-z0-9_]*::)+')


def norm_type(s):
    s = s or ''
    s = s.replace('typename ', '')
    s = _NS_STRIP.sub('', s)
    s = s.replace('::aion', 'aion')
    s = re.sub(r'\s+', ' ', s).strip()
    s = re.sub(r'\s*([<>,*&()])\s*', r'\1', s)
    s = s.replace('std::int32_t', 'int32_t').replace('std::int64_t', 'int64_t').replace('std::int16_t', 'int16_t').replace('std::int8_t', 'int8_t')
    m = re.fullmatch(r'(.*?)\bconst$', s)
    if m and not m.group(1).rstrip().endswith('*'):
        s = 'const ' + m.group(1).strip()
    elif m:
        s = m.group(1).rstrip() + 'const'
    s = re.sub(r'\b(?:constexpr|constinit)\s+', 'const ', s)
    s = re.sub(r'\b(?:static|inline|mutable|thread_local)\s+', '', s)
    s = re.sub(r'\bconst\s+const\s+', 'const ', s)
    s = re.sub(r'(?<![\w:])(?:signed\s+)?int(?![\w:])', 'int32_t', s)
    s = re.sub(r'(?<![\w:])long\s+long(?![\w:])', 'int64_t', s)
    return s.strip()


# ----------------------------------------------------------------------------------------------------------------------------------
# Rules
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass(order=True, frozen=True)
class Finding:
    path: str
    line: int
    col: int
    severity: str
    rule: str
    message: str

    def format(self):
        return f'{self.path}:{self.line}:{self.col}: {self.severity}: {self.rule}: {self.message}'


SHIMS = frozenset(('ArrayList', 'LinkedList', 'ArrayDeque', 'HashMap', 'LinkedHashMap', 'TreeMap', 'HashSet', 'LinkedHashSet', 'TreeSet',
                   'EnumMap', 'PriorityQueue', 'ConcurrentHashMap', 'ConcurrentKeySet', 'CopyOnWriteArrayList', 'CopyOnWriteArraySet',
                   'ConcurrentLinkedQueue', 'ConcurrentLinkedDeque', 'AtomicBoolean', 'AtomicInteger', 'AtomicLong', 'AtomicReference',
                   'AtomicLongArray', 'AtomicIntegerArray', 'AtomicReferenceArray', 'Chm'))
L1_HEADS = frozenset(('Final', 'Field', 'PartSlot', 'PartMap', 'PartList', 'OwnerRef', 'SelfOrRef', 'Monitor', 'StampedLock', 'Semaphore',
                      'PinnedCallback', 'Array')) | SHIMS
SAFE_STATIC_HEADS = frozenset(('Field', 'Final', 'Monitor', 'StampedLock', 'Semaphore', 'ConfigValue', 'Logger', 'LeafMutex', 'RankedMutex',
                               'std::atomic', 'std::mutex', 'std::shared_mutex', 'std::recursive_mutex', 'std::once_flag', 'std::atomic_flag',
                               'std::condition_variable', 'std::condition_variable_any')) | SHIMS
TASK_APIS = frozenset(('schedule', 'scheduleAtFixedRate', 'scheduleWithFixedDelay', 'execute', 'executeLongRunning', 'submit',
                       'submitLongRunning', 'deferred', 'PinnedCallback', 'addObserver', 'attach', 'addAttackCalcObserver', 'putRequest',
                       'addOnEventEndTask'))
COMPUTE_CALLS = frozenset(('compute', 'computeIfAbsent', 'computeIfPresent', 'merge'))
MAP_WRITES = frozenset(('put', 'putIfAbsent', 'remove', 'compute', 'computeIfAbsent', 'computeIfPresent', 'merge', 'replace', 'replaceAll',
                        'clear', 'removeIf'))
BANNED_CALLS = frozenset(('strtok', 'localtime', 'gmtime', 'asctime', 'ctime', 'rand', 'srand', 'setlocale', 'getenv'))
PACKET_BASES = frozenset(('AionServerPacket', 'AionClientPacket', 'LsServerPacket', 'LsClientPacket', 'CsServerPacket', 'CsClientPacket',
                          'BaseServerPacket', 'BaseClientPacket'))
SHARED_BASES = frozenset(('RefCounted', 'OwnedPart', 'Immortal'))
PER_RECIPIENT = frozenset(('SM_ACCOUNT_PROPERTIES', 'SM_ALLIANCE_INFO', 'SM_BLOCK_LIST', 'SM_CHALLENGE_LIST', 'SM_CHARACTER_LIST',
                           'SM_DIALOG_WINDOW', 'SM_FRIEND_LIST', 'SM_FRIEND_UPDATE', 'SM_GROUP_INFO', 'SM_HOUSE_BIDS', 'SM_HOUSE_EDIT',
                           'SM_HOUSE_OBJECT', 'SM_HOUSE_REGISTRY', 'SM_INSTANCE_INFO', 'SM_LOOT_ITEMLIST', 'SM_MAIL_SERVICE',
                           'SM_MARK_FRIENDLIST', 'SM_MESSAGE', 'SM_PLAYER_INFO', 'SM_PLAYER_SEARCH', 'SM_PLAY_MOVIE', 'SM_PRICES',
                           'SM_SIEGE_LOCATION_INFO', 'SM_UNK_3_5_1', 'SM_GROUP_MEMBER_INFO', 'SM_ALLIANCE_MEMBER_INFO'))
NOT_CACHEABLE = frozenset(('SM_ATTACK', 'SM_CASTSPELL_RESULT', 'SM_PET', 'SM_PLAY_MOVIE', 'SM_GROUP_MEMBER_INFO', 'SM_ALLIANCE_MEMBER_INFO',
                           'SM_KEY'))
IMMORTAL_BASES = frozenset(('QuestHandler', 'AbstractQuestHandler', 'AdminCommand', 'PlayerCommand', 'ConsoleCommand', 'ChatCommand',
                            'StaticTemplate'))
PER_RUN_SERVICES = frozenset(('Siege', 'Base', 'WorldRaid', 'Event', 'Assault', 'AhserionRaid', 'Invasion', 'AgentFight'))
WAIVER_RULES = {'confined': {'L1', 'L3', 'L19'}, 'fieldmap': {'L1', 'L2', 'L3'}, 'lockdep': {'L12'}, 'quiescent-safe': {'L5'}}
_WAIVER_RE = re.compile(r'^(confined|fieldmap|lockdep|quiescent-safe|lint)\s*:\s*(.*)$')


class Linter:
    def __init__(self, fieldmap=None, rules=None, cycles=False):
        self.fm = fieldmap
        self.rules = set(rules or ALL_RULES)
        self.cycles = cycles
        self.findings = []
        self.sources = []
        self.classes = []
        self.functions = []
        self.globals = []
        self._fm_classes = (fieldmap or {}).get('classes', {})
        self._by_pkg_cpp = {}
        self._k5_names = set()
        self._k1_names = set()
        self._shared_names = set()
        part_names = {c.get('cppName', '').split('::')[-1] for c in self._fm_classes.values() if c.get('partOf')}
        for cid, c in self._fm_classes.items():
            pkg = self._java_package(cid)
            self._by_pkg_cpp.setdefault((pkg, c.get('cppName')), []).append(cid)
        simple_kinds = {}
        for cid, c in self._fm_classes.items():
            simple_kinds.setdefault(c.get('cppName', '').split('::')[-1], set()).add(c.get('kind'))
        for name, kinds in simple_kinds.items():
            if kinds == {'K5'}:
                self._k5_names.add(name)
            if kinds == {'K1'}:
                self._k1_names.add(name)
            if kinds <= {'K3', 'K4'} and name not in part_names:
                self._shared_names.add(name)

    def _java_package(self, cid):
        parts = cid.split('.')
        out = []
        for p in parts:
            if p[:1].isupper() or '$' in p:
                break
            out.append(p)
        return '.'.join(out)

    # -- input
    def add_file(self, path, display=None):
        with open(path, encoding='utf-8-sig', errors='strict') as f:
            text = f.read()
        self.add_source(text, display or path.replace('\\', '/'), path)

    def add_source(self, text, display, path=None):
        src = Source(path or display, text, display)
        p = Parser(src).parse()
        self.sources.append(src)
        self.classes.extend(p.classes)
        self.functions.extend(p.functions)
        self.globals.extend(p.globals)

    # -- helpers
    def report(self, src, line, col, rule, message, severity='error'):
        if rule not in self.rules:
            return
        waived, bad = self._waived(src, line, rule)
        if bad:
            self.findings.append(Finding(src.display, line, col, 'error', 'W0', f'waiver without a reason: // {bad}'))
        if waived:
            return
        self.findings.append(Finding(src.display, line, col, severity, rule, message))

    def _waived(self, src, line, rule):
        bad = None
        for c in src.comment_near(line):
            m = _WAIVER_RE.match(c)
            if not m:
                continue
            key, rest = m.group(1), m.group(2).strip()
            if key == 'lint':
                mm = re.match(r'^((?:L\d+\s*,\s*)*L\d+)\s*(.*)$', rest)
                if not mm:
                    continue
                rules = {r.strip() for r in mm.group(1).split(',')}
                if rule in rules:
                    if not mm.group(2).strip():
                        return False, c
                    return True, None
                continue
            if rule in WAIVER_RULES[key]:
                if not rest:
                    return False, c
                return True, None
        return False, bad

    @staticmethod
    def area(src):
        p = '/' + src.display.replace('\\', '/')
        if any(k in p for k in KERNEL_PATHS):
            return 'runtime'
        if '/network/' in p:
            return 'network'
        if '/configs/' in p:
            return 'configs'
        return 'game'

    def java_cid(self, cls):
        for c in cls.src.comment_near(cls.line):
            m = re.match(r'^fieldmap-class:\s*(\S+)', c)
            if m:
                return m.group(1) if m.group(1) in self._fm_classes else None
        ns = [x[:-1] if x.endswith('_') else x for x in cls.namespace]
        if ns[:2] != ['aion', 'gameserver']:
            return None
        if len(ns) >= 3 and ns[2] == 'handlers':
            pkg = '.'.join(ns[3:])
        else:
            pkg = '.'.join(['com', 'aionemu', 'gameserver'] + ns[2:])
        cand = f'{pkg}.{cls.qualname.replace("::", ".")}' if pkg else cls.qualname.replace('::', '.')
        if cand in self._fm_classes:
            return cand
        hits = self._by_pkg_cpp.get((pkg, cls.qualname))
        if hits and len(hits) == 1:
            return hits[0]
        return None

    def class_by_name(self, name):
        return [c for c in self.classes if c.name == name]

    def _base_names(self, cls):
        out = []
        for b in cls.bases:
            b = norm_type(b)
            out.append(re.split(r'[<\s]', b)[0].split('::')[-1])
        return out

    def is_shared(self, cls, _seen=None):
        cid = self.java_cid(cls)
        if cid is not None:
            return self._fm_classes[cid].get('kind') in ('K3', 'K4')
        seen = _seen or set()
        if id(cls) in seen:
            return False
        seen.add(id(cls))
        for b in self._base_names(cls):
            if b in SHARED_BASES or b.startswith('Rc'):
                return True
            for other in self.class_by_name(b):
                if other is not cls and self.is_shared(other, seen):
                    return True
        return False

    def is_packet(self, cls, _seen=None):
        cid = self.java_cid(cls)
        if cid is not None and self._fm_classes[cid].get('kind') == 'K2':
            return True
        seen = _seen or set()
        if id(cls) in seen:
            return False
        seen.add(id(cls))
        for b in self._base_names(cls):
            if b in PACKET_BASES:
                return True
            for other in self.class_by_name(b):
                if other is not cls and self.is_packet(other, seen):
                    return True
        return False

    def is_confined(self, cls):
        cid = self.java_cid(cls)
        if cid is not None:
            return self._fm_classes[cid].get('kind') == 'K5'
        return any(c.startswith('confined:') for c in cls.src.comment_near(cls.line))

    def refcounted_names(self):
        """Names of RefCounted classes (raw pointers to them are borrows). Parts (OwnedPart, fieldmap partOf) are excluded: sibling part
        pointers are the design's non-retaining form."""
        names = set(self._shared_names)
        for c in self.classes:
            if self.is_shared(c) and not self._is_part_class(c):
                names.add(c.name)
            elif c.name in names and self._is_part_class(c):
                names.discard(c.name)
        return names

    def _is_part_class(self, cls, _seen=None):
        cid = self.java_cid(cls)
        if cid is not None and self._fm_classes[cid].get('partOf'):
            return True
        seen = _seen or set()
        if id(cls) in seen:
            return False
        seen.add(id(cls))
        for b in self._base_names(cls):
            if b == 'OwnedPart':
                return True
            for other in self.class_by_name(b):
                if other is not cls and self._is_part_class(other, seen):
                    return True
        return False

    # -- run
    def run(self):
        self._refcounted = self.refcounted_names()
        for cls in self.classes:
            self._class_rules(cls)
        for fn in self.functions:
            self._function_rules(fn)
        for g in self.globals:
            self._global_rules(g)
        for src in self.sources:
            self._token_rules(src)
        if self.cycles and self.fm is not None:
            self._cycle_rules()
        self._l7()
        self.findings = sorted(set(self.findings))
        return self.findings

    # -- class-level rules: L1 L2 L3 L4 L10 L11 L13 L15 L16 L19
    def _class_rules(self, cls):
        src = cls.src
        shared = self.is_shared(cls)
        packet = self.is_packet(cls)
        cid = self.java_cid(cls)
        entry = self._fm_classes.get(cid) if cid else None
        for mb in cls.members:
            ty = norm_type(mb.type)
            raw = mb.type
            static = 'static' in mb.specifiers
            if 'thread_local' in mb.specifiers:
                self._l15(src, mb.line, mb.col, raw, mb.name)
            if static:
                if not self._static_safe(mb.type, mb.specifiers):
                    self.report(src, mb.line, mb.col, 'L4', f'static data member {cls.qualname}::{mb.name} of type `{raw}` is mutable shared state; '
                                                            'use const, Field<>, a shim, std::atomic or a Monitor-guarded holder')
                continue
            if shared and self.area(src) != 'runtime':
                self._l1(cls, mb, ty)
                self._l3(cls, mb, ty)
            if packet:
                if 'Ptr<' in ty or ty.endswith('&') or self._raw_refcounted_ptr(ty):
                    self.report(src, mb.line, mb.col, 'L10', f'packet member {cls.qualname}::{mb.name} `{raw}` must be Ref<> or a value (packets outlive borrows)')
            if (shared or packet) and re.search(r'(?:iterator|Iterator)\b', ty) and not ty.startswith('JavaIterator'):
                self.report(src, mb.line, mb.col, 'L11', f'stored iterator member {cls.qualname}::{mb.name} `{raw}`')
            if shared or packet:
                for name in self._k5_names:
                    if re.search(rf'(?<![\w:]){re.escape(name)}\b', ty):
                        self.report(src, mb.line, mb.col, 'L19', f'confined (K5) class {name} stored in {"packet" if packet else "shared"} member '
                                                                 f'{cls.qualname}::{mb.name}')
                        break
        if entry is not None and entry.get('kind') in ('K3', 'K4') and self.area(src) != 'runtime':
            self._l2(cls, cid, entry)
        if self.cycles:
            self._l16_owner(cls, entry)
        self._l13(cls, entry)
        if packet:
            self._l10_class(cls)

    def _static_safe(self, type_text, specs):
        ty = norm_type(type_text)
        if 'constexpr' in specs or ty.startswith('const ') or re.search(r'\bconst$', type_text.strip()) or 'thread_local' in specs:
            return True
        if re.match(r'^(?:const\s+)?auto\s*\*\s*const', type_text):
            return True
        head = re.split(r'[<\s*&]', ty)[0]
        full_head = re.match(r'^(std::\w+)', ty)
        if head in SAFE_STATIC_HEADS or (full_head and full_head.group(1) in SAFE_STATIC_HEADS):
            return True
        if head.startswith('Atomic') or head.startswith('Rc'):
            return True
        arr = re.match(r'^std::array<(.*),[^,]*>$', ty)
        if arr:
            return self._static_safe(arr.group(1), set())
        return False

    def _l1(self, cls, mb, ty):
        body = ty
        if body.startswith('const ') and not body.startswith('const std::unique_ptr'):
            return
        if re.search(r'\*const$', body):
            return
        if body.startswith('const std::unique_ptr<') or body.startswith('const std::array<std::unique_ptr<') or body.startswith('const std::shared_ptr<'):
            return
        head = re.split(r'[<\s]', body)[0]
        if head in L1_HEADS or head.startswith('Atomic') or head.startswith('Rc') or head == 'std::atomic':
            if head == 'std::atomic' and self.area(cls.src) not in ('runtime', 'configs'):
                self.report(cls.src, mb.line, mb.col, 'L14', f'std::atomic member {cls.qualname}::{mb.name}: use Field<> or an Atomic* shim', 'warning')
            return
        if head.endswith('Mutex') or head in ('LeafMutex',):
            return
        if body.endswith('&') or body.startswith('Ptr<') or 'string_view' in body or 'span<' in body:
            return  # L3 reports these
        if head == 'Ref' or head.startswith('std::'):
            kind = 'non-const Ref' if head == 'Ref' else 'plain std type'
        else:
            kind = 'unwrapped'
        self.report(cls.src, mb.line, mb.col, 'L1', f'{kind} member {cls.qualname}::{mb.name} `{mb.type}` in a shared class: use const, Final<>, '
                                                    'Field<>, a shim, a part or a Monitor')

    def _raw_refcounted_ptr(self, ty):
        for m in re.finditer(r'(?<![\w:])(?:const\s+)?([A-Z]\w*)\s*\*', ty):
            name = m.group(1)
            inside_field = re.search(rf'(?:Field|Final|Array)<(?:const\s+)?{re.escape(name)}\s*\*', ty)
            if name in self._refcounted and not inside_field:
                return name
        return None

    def _l3(self, cls, mb, ty):
        bad = None
        if 'string_view' in ty:
            bad = 'std::string_view'
        elif 'span<' in ty:
            bad = 'std::span'
        elif re.search(r'(?<!\w)Ptr<', ty) and not re.search(r'Field<Ptr<', ty):
            bad = 'Ptr<>'
        elif ty.endswith('&') and not ty.startswith('OwnerRef'):
            bad = 'reference'
        else:
            name = self._raw_refcounted_ptr(ty)
            if name is not None:
                bad = f'raw pointer to RefCounted {name}'
        if bad:
            self.report(cls.src, mb.line, mb.col, 'L3', f'{bad} member {cls.qualname}::{mb.name} `{mb.type}` in a shared class (borrows end with the task)')

    def _l2(self, cls, cid, entry):
        fields = {f['name']: f for f in entry.get('fields', [])}
        caps = {}
        for c in entry.get('captures', []):
            caps[self._capture_member_name(c)] = c
        seen = set()
        for mb in cls.members:
            name = mb.name
            f = fields.get(name) or fields.get(name.rstrip('_')) or caps.get(name) or caps.get(name.rstrip('_'))
            if f is None:
                continue
            seen.add(f['name'] if 'rule' in f and f in fields.values() else f.get('name'))
            expected = f.get('cpp')
            if expected is None or str(f.get('rule', '')).startswith('config field'):
                continue
            exp = norm_type(re.sub(r'\s*getInstance\(\)$', '', expected))
            if expected.endswith('getInstance()') or expected.startswith('static const Logger'):
                continue
            got = norm_type(('const ' if mb.specifiers & {'constexpr', 'constinit'} else '') + mb.type)
            if got != exp:
                self.report(cls.src, mb.line, mb.col, 'L2', f'{cls.qualname}::{mb.name} is `{mb.type}`, fieldmap.json expects `{expected}` ({f.get("rule")}; '
                                                            f'Java {f.get("java")} line {f.get("line")})')
        for f in entry.get('fields', []):
            mods = f.get('modifiers', [])
            if 'static' in mods or f.get('cpp') is None or f['name'] in seen or str(f.get('rule', '')).startswith('config field'):
                continue
            present = any(mb.name in (f['name'], f['name'] + '_') for mb in cls.members)
            if not present:
                self.report(cls.src, cls.line, cls.col, 'L2', f'{cls.qualname} lacks member {f["name"]} `{f["cpp"]}` (Java {f.get("java")} line {f.get("line")})')

    @staticmethod
    def _capture_member_name(cap):
        if cap.get('kind') == 'this':
            t = (cap.get('type') or '').rsplit('.', 1)[-1]
            return t[:1].lower() + t[1:]
        return cap.get('name')

    def _l13(self, cls, entry):
        bases = self._base_names(cls)
        if 'Immortal' not in bases:
            return
        if cls.name in PER_RUN_SERVICES or any(b in PER_RUN_SERVICES for b in bases):
            self.report(cls.src, cls.line, cls.col, 'L13', f'per-run service object {cls.qualname} must be RefCounted and pinned, not Immortal')
            return
        if entry is not None and (entry.get('singleton') or entry.get('immortal') or entry.get('kind') == 'K1'):
            return
        if any(b in IMMORTAL_BASES for b in bases):
            return
        if any(f.name in ('getInstance', 'instance') and f.static_member for f in cls.functions) or cls.static_declared & {'getInstance', 'instance'} or \
                any(fn.name in ('getInstance', 'instance') and fn.qual and fn.qual[-1] == cls.name for fn in self.functions):
            return
        self.report(cls.src, cls.line, cls.col, 'L13', f'Immortal is only for singletons, static data, quest handlers and commands: {cls.qualname}')

    def _l10_class(self, cls):
        fnames = {f.name for f in cls.functions}
        if cls.name.startswith('SM_'):
            has = 'recipients' in fnames or any(fn.name == 'recipients' and fn.qual and fn.qual[-1] == cls.name for fn in self.functions)
            if cls.name in PER_RECIPIENT and not has:
                self.report(cls.src, cls.line, cls.col, 'L10', f'{cls.name} reads the connection in writeImpl: override recipients() -> PER_RECIPIENT')
            elif cls.name not in PER_RECIPIENT and has:
                self.report(cls.src, cls.line, cls.col, 'L10', f'{cls.name} overrides recipients() but is not a per-recipient packet (design 8.3)')

    def _l15(self, src, line, col, type_text, name):
        ty = norm_type(type_text)
        if re.search(r'(?<!\w)(?:Ref|Ptr)<', ty) or ty.endswith('&') or 'string_view' in ty or 'span<' in ty or self._raw_refcounted_ptr(ty):
            self.report(src, line, col, 'L15', f'thread_local {name} `{type_text}` holds a Ref/Ptr/borrow of a RefCounted object')

    def _l16_owner(self, cls, entry):
        if entry is None:
            return
        owners = set()
        for p in entry.get('partOf', []):
            owner_cid = p.rsplit('.', 1)[0]
            oc = self._fm_classes.get(owner_cid)
            if oc is not None:
                owners.add(oc.get('cppName', '').split('::')[-1])
        for mb in cls.members:
            ty = norm_type(mb.type)
            for o in owners:
                if re.search(rf'(?<!\w)Ref<{re.escape(o)}>', ty):
                    self.report(cls.src, mb.line, mb.col, 'L16', f'part {cls.qualname} holds `{mb.type}` to its owner {o}: use OwnerRef<{o}> or SelfOrRef<{o}>')

    def _cycle_rules(self):
        edges = self.fm.get('cycleEdges', {})
        path = os.path.join('game-server', 'generated', 'concurrency', 'cycles_report.md')
        for key in sorted(edges):
            e = edges[key]
            if e.get('resolution') is None:
                self.findings.append(Finding(path, 0, 0, 'error', 'L16', f'unresolved cycle edge {key} (component {e.get("scc")}); add a cycles.toml resolution'))
        for key in self.fm.get('staleResolutions', []):
            self.findings.append(Finding(path, 0, 0, 'warning', 'L16', f'stale cycles.toml resolution {key}'))

    # -- global variables: L4 L15
    def _global_rules(self, g):
        if 'thread_local' in g.specifiers:
            self._l15(g.src, g.line, g.col, g.type, g.name)
            return
        if 'extern' in g.specifiers:
            return
        if not self._static_safe(g.type, g.specifiers):
            self.report(g.src, g.line, g.col, 'L4', f'namespace-scope variable {g.name} of type `{g.type}` is mutable shared state; use const, Field<>, a shim, '
                                                  'std::atomic or a Monitor')

    # -- function rules: L5 L8 L9 L10 L11 L12 L17 L18 L20
    def _function_rules(self, fn):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        bs, be = fn.body
        cls = fn.cls or self._owner_class(fn)
        decls = local_declarations(src, fn.params[0], fn.params[1]) + local_declarations(src, bs + 1, be)
        lambdas = find_lambdas(src, bs + 1, be)
        self._l5(fn, decls, lambdas)
        self._l5b(fn, decls)
        self._l8_statics(fn)
        if fn.dtor and cls is not None and self.is_shared(cls) and self.area(src) != 'runtime':
            self._l9(fn, cls)
        if fn.name == 'writeImpl':
            for i in range(bs, be):
                if t[i] in ('sendPacket', 'broadcastPacket') and i + 1 < be and t[i + 1] == '(':
                    self.report(src, src.line[i], src.col[i], 'L10', 'writeImpl must not send packets')
        self._l11(fn, decls)
        self._l12(fn, lambdas)
        self._l17(fn)
        self._l18_callbacks(fn, decls)
        self._l20(fn, lambdas)

    def _owner_class(self, fn):
        if not fn.qual:
            return None
        cands = [c for c in self.classes if c.name == fn.qual[-1]]
        return cands[0] if len(cands) == 1 else None

    def _call_args(self, src, open_i):
        return split_top(src, open_i + 1, src.match[open_i])

    def _l5(self, fn, decls, lambdas):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        by_start = {lam.cap[0] - 1: lam for lam in lambdas}
        bs, be = fn.body
        for i in range(bs, be):
            if k[i] != IDENT or t[i] not in TASK_APIS:
                continue
            j = i + 1
            if j < be and t[j] == '<':
                depth = 0
                while j < be:
                    if t[j] == '<':
                        depth += 1
                    elif t[j] == '>':
                        depth -= 1
                        if depth == 0:
                            j += 1
                            break
                    j += 1
            if j >= be or t[j] not in ('(', '{') or m[j] < 0:
                continue
            args = split_top(src, j + 1, m[j])
            lam_arg = None
            for n, (a, b) in enumerate(args):
                if a in by_start and by_start[a].body[1] == b - 1:
                    lam_arg = (n, by_start[a])
                    break
            if lam_arg is None:
                continue
            n, lam = lam_arg
            pins = set()
            if n > 0:
                pa, pb = args[0]
                for x in range(pa, pb):
                    if t[x] == 'this':
                        pins.add('this')
                    elif k[x] == IDENT and (x == pa or t[x - 1] in ('&', '{', ',', '(')):
                        pins.add(t[x])
            caps = split_top(src, lam.cap[0], lam.cap[1])
            where = f'lambda passed to {t[i]}()'
            for a, b in caps:
                ct = [t[x] for x in range(a, b)]
                text = src.text_of(a, b)
                line, col = src.line[a], src.col[a]
                if ct in (['&'], ['=']):
                    self.report(src, line, col, 'L5', f'{where}: default capture [{ct[0]}] is forbidden in stored lambdas; list the captures and pins')
                    continue
                if n == 0:
                    self.report(src, line, col, 'L5', f'{where}: unpinned stored lambda must be captureless (capture `{text}`; use a pin, a TaskStruct or bindTask)')
                    continue
                if ct == ['this']:
                    if 'this' not in pins:
                        self.report(src, line, col, 'L5', f'{where}: `this` is captured but not pinned')
                    continue
                if ct == ['*', 'this']:
                    self.report(src, line, col, 'L5', f'{where}: `*this` copies the object into the task')
                    continue
                if len(ct) == 2 and ct[0] == '&':
                    if ct[1] not in pins:
                        self.report(src, line, col, 'L5', f'{where}: `&{ct[1]}` is captured by reference but not pinned')
                    continue
                if len(ct) == 1 and k[a] == IDENT:
                    ty = self._decl_type(decls, ct[0], lam.cap[0])
                    if ty is not None:
                        nty = norm_type(ty)
                        if re.search(r'(?<!\w)Ptr<', nty) or nty.endswith('*') or 'string_view' in nty or 'span<' in nty:
                            self.report(src, line, col, 'L5', f'{where}: `{ct[0]}` of type `{ty}` is captured by copy; capture a Ref<> or a value')
                        for name in self._k5_names:
                            if re.search(rf'(?<![\w:]){re.escape(name)}\b', nty):
                                self.report(src, line, col, 'L19', f'{where}: confined (K5) class {name} captured by `{ct[0]}`')
                                break
                    continue
                if '=' in ct:
                    rhs = ct[ct.index('=') + 1:]
                    if rhs[:1] == ['&']:
                        self.report(src, line, col, 'L5', f'{where}: init capture `{text}` stores an address')

    @staticmethod
    def _decl_type(decls, name, before):
        best = None
        for n, ty, idx in decls:
            if n == name and idx < before and (best is None or idx > best[1]):
                best = (ty, idx)
        return best[0] if best else None

    def _l5b(self, fn, decls):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        bs, be = fn.body
        scopes = [i for i in range(bs, be) if t[i] == 'QuiescentScope' and k[i] == IDENT and i + 1 < be and k[i + 1] == IDENT]
        points = [i for i in range(bs, be) if t[i] == 'quiescentPoint' and i + 1 < be and t[i + 1] == '(']
        if not scopes:
            for i in points:
                if fn.name != 'quiescentPoint':
                    self.report(src, src.line[i], src.col[i], 'L5', 'quiescentPoint() outside a function with a QuiescentScope is a no-op', 'warning')
            return
        for sc in scopes:
            depth = 0
            j = sc
            while j > bs:
                j -= 1
                if t[j] == '}':
                    j = m[j] if m[j] > 0 else j
                elif t[j] == '{':
                    depth += 1
            if depth != 1:
                self.report(src, src.line[sc], src.col[sc], 'L5', 'QuiescentScope must be opened at the top level of the task body')
            safe = any(c.startswith('quiescent-safe:') and c[len('quiescent-safe:'):].strip() for ln in range(src.line[bs] - 3, src.line[be] + 1)
                       for c in src.comments.get(ln, []))
            for name, ty, idx in decls:
                nty = norm_type(ty)
                if idx < sc and (re.search(r'(?<!\w)Ptr<', nty) or (nty.endswith('&') and not nty.startswith('const std::') and 'Ref<' not in nty)):
                    if idx < fn.params[1] and idx >= fn.params[0]:
                        if not safe:
                            self.report(src, src.line[idx], src.col[idx], 'L5', f'parameter {name} `{ty}` lives across quiescentPoint(): '
                                                                                 'add // quiescent-safe: <which Ref keeps it alive>')
                    elif not safe:
                        self.report(src, src.line[idx], src.col[idx], 'L5', f'borrow {name} `{ty}` declared before the QuiescentScope stays live across quiescentPoint()')
            for i in range(sc, be):
                if t[i] == 'for' and i + 1 < be and t[i + 1] == '(' and m[i + 1] > 0:
                    inner = local_declarations(src, i + 2, m[i + 1])
                    for name, ty, idx in inner:
                        if idx + 1 < be and t[idx + 1] == ':' and not norm_type(ty).startswith(('Ref<', 'const Ref<', 'int', 'auto', 'const auto')):
                            if re.search(r'(?<!\w)Ptr<', norm_type(ty)) or norm_type(ty).endswith('&'):
                                self.report(src, src.line[idx], src.col[idx], 'L5', f'loop variable {name} `{ty}` inside a QuiescentScope must be a Ref<> snapshot element')

    def _l8_statics(self, fn):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        bs, be = fn.body
        for i in range(bs + 1, be):
            if t[i] != 'static' or t[i - 1] not in (';', '{', '}', ')'):
                continue
            j = i + 1
            specs = []
            while j < be and t[j] in ('const', 'constexpr', 'constinit', 'thread_local', 'inline'):
                specs.append(t[j])
                j += 1
            e = j
            while e < be and t[e] not in (';', '=', '{', '('):
                if t[e] == '<':
                    depth = 0
                    while e < be:
                        if t[e] == '<':
                            depth += 1
                        elif t[e] == '>':
                            depth -= 1
                            if depth == 0:
                                break
                        e += 1
                e += 1
            if e >= be:
                continue
            type_text = src.text_of(j, e - 1) if e - 1 > j else ''
            if 'const' in specs or 'constexpr' in specs or 'thread_local' in specs:
                if 'thread_local' in specs:
                    self._l15(src, src.line[i], src.col[i], type_text, t[e - 1])
                continue
            nty = norm_type(type_text)
            if nty.startswith(('std::atomic', 'Monitor', 'std::once_flag', 'std::mutex')) or re.match(r'^auto\s*\*\s*const', type_text):
                continue
            if t[e] == '=' and e + 1 < be and t[e + 1] == 'new':
                continue  # leaked immortal: static auto* state = new State (never reassigned, never destroyed)
            if fn.name in ('getInstance', 'instance', 'log', 'logger') and re.match(r'^[A-Z]\w*(?:<.*>)?[*&]?$', nty.split('::')[-1]):
                continue
            self.report(src, src.line[i], src.col[i], 'L8', f'mutable function-local static `{type_text}` in {fn.display}')

    def _l9(self, fn, cls):
        src = fn.src
        t = src.tok
        bs, be = fn.body
        for i in range(bs + 1, be):
            x = t[i]
            msg = None
            if x == '->' and t[i - 1] != 'this':
                msg = 'dereference'
            elif x == 'SYNCHRONIZED':
                msg = 'SYNCHRONIZED'
            elif x in ('sendPacket', 'broadcastPacket', 'broadcastToWorld'):
                msg = 'packet send'
            elif x == 'getInstance' and i + 1 < be and t[i + 1] == '(':
                msg = 'service call'
            elif x in ('add', 'put', 'remove', 'clear', 'get', 'contains', 'forEach', 'iterator', 'snapshot', 'compute', 'computeIfAbsent') \
                    and t[i - 1] in ('.', '->') and i + 1 < be and t[i + 1] == '(':
                recv = t[i - 2] if t[i - 1] == '.' else None
                scalar = recv is not None and any(mb.name == recv and re.match(r'^Field<(?!Ref<|std::string|std::shared_ptr)', norm_type(mb.type))
                                                  for mb in cls.members)
                if not (x == 'get' and scalar):
                    msg = f'container call {x}()'
            if msg:
                self.report(src, src.line[i], src.col[i], 'L9', f'destructor {fn.display}: {msg} (destructors are release-only, design 2.7)')

    def _l11(self, fn, decls):
        src = fn.src
        t, k = src.tok, src.kind
        bs, be = fn.body
        if self.area(src) == 'runtime':
            return  # the kernel implements the shims over std containers
        locals_std = {n for n, ty, _ in decls if norm_type(ty).startswith(('std::', 'const std::'))}
        cls = fn.cls or self._owner_class(fn)
        if cls is not None:
            locals_std |= {mb.name for mb in cls.members if norm_type(mb.type).startswith(('std::', 'const std::')) and not self.is_shared(cls)}
        for i in range(bs + 1, be - 2):
            if t[i] == '=' and k[i + 1] == IDENT:
                j = i + 1
                recv = []
                depth = 0
                while j < be and (depth > 0 or t[j] not in (';', ',', ')')):
                    if t[j] in ('(', '[', '{'):
                        depth += 1
                    elif t[j] in (')', ']', '}'):
                        depth -= 1
                    recv.append(t[j])
                    j += 1
                if len(recv) >= 4 and recv[-3:] == ['begin', '(', ')'] and recv[-4] in ('.', '->'):
                    base = recv[0]
                    if len(recv) == 5 and base in locals_std:
                        continue
                    if base in ('std',):
                        continue
                    self.report(src, src.line[i], src.col[i], 'L11', f'stored iterator `{" ".join(recv)}`: iterate with range-for, snapshot() or auto it = x.iterator()')

    def _blocking_ranges(self, fn, lambdas):
        src = fn.src
        t, m = src.tok, src.match
        bs, be = fn.body
        ranges = []
        for i in range(bs, be):
            if t[i] == 'SYNCHRONIZED' and i + 1 < be and t[i + 1] == '(' and m[i + 1] > 0:
                j = m[i + 1] + 1
                if j < be and t[j] == '{' and m[j] > 0:
                    ranges.append((j, m[j], 'SYNCHRONIZED'))
            if t[i] in COMPUTE_CALLS and i + 1 < be and t[i + 1] == '(' and t[i - 1] in ('.', '->'):
                for lam in lambdas:
                    if i + 1 < lam.cap[0] < m[i + 1]:
                        ranges.append((lam.body[0], lam.body[1], f'{t[i]} callback'))
        return ranges

    def _l12(self, fn, lambdas):
        src = fn.src
        t, k = src.tok, src.kind
        for s, e, what in self._blocking_ranges(fn, lambdas):
            for i in range(s, e):
                if k[i] == IDENT and t[i].endswith('DAO') and i + 1 < e and t[i + 1] in ('::', '.', '('):
                    self.report(src, src.line[i], src.col[i], 'L12', f'DAO call inside {what} (blocking under a Monitor)', 'warning')
                elif t[i] == 'get' and i + 1 < e and t[i + 1] == '(' and t[i - 1] in ('.', '->') and k[i - 2] == IDENT and \
                        re.search(r'(?i)future|task', t[i - 2]):
                    self.report(src, src.line[i], src.col[i], 'L12', f'Future::get() inside {what} (blocking under a Monitor)', 'warning')

    def _l17(self, fn):
        if fn.name == 'addPair':
            return
        src = fn.src
        t, k = src.tok, src.kind
        bs, be = fn.body
        for i in range(bs + 2, be):
            if t[i] == 'add' and i + 1 < be and t[i + 1] == '(' and t[i - 1] in ('.', '->'):
                j = i - 2
                recv = t[j]
                if recv == ')' and src.match[j] > 0:
                    recv = t[src.match[j] - 1]
                if re.search(r'(?i)knownlist', recv):
                    self.report(src, src.line[i], src.col[i], 'L17', 'KnownList add() outside KnownList::addPair (design 5.3 RR-14)')

    def _l18_callbacks(self, fn, decls):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        bs, be = fn.body
        mutex_names = set()
        cls = fn.cls or self._owner_class(fn)
        if cls is not None:
            for mb in cls.members:
                if re.search(r'(?:RankedMutex|LeafMutex)\b', mb.type):
                    mutex_names.add(mb.name)
        for n, ty, _ in decls:
            if re.search(r'(?:RankedMutex|LeafMutex)\b', ty):
                mutex_names.add(n)
        if not mutex_names:
            return
        callables = set()
        for n, ty, idx in decls:
            if fn.params[0] <= idx < fn.params[1]:
                nty = norm_type(ty)
                if re.search(r'std::function<|PinnedCallback|^auto&&$|^(?:F|Fn|Func|Callable|Callback|Predicate|Comparator|Consumer|Visitor)&*$|std::invocable', nty):
                    callables.add(n)
        for i in range(bs, be):
            if t[i] in ('scoped_lock', 'lock_guard', 'unique_lock') and i + 1 < be:
                j = i + 1
                if t[j] == '<':
                    while j < be and t[j] != '>':
                        j += 1
                    j += 1
                if j < be and k[j] == IDENT:
                    j += 1
                if j < be and t[j] in ('(', '{') and m[j] > 0:
                    held = [t[x] for x in range(j + 1, m[j]) if k[x] == IDENT]
                    if not any(h in mutex_names for h in held):
                        continue
                    # scope of the guard: until the end of the enclosing block
                    depth = 0
                    q = m[j] + 1
                    while q < be:
                        if t[q] == '{':
                            depth += 1
                        elif t[q] == '}':
                            if depth == 0:
                                break
                            depth -= 1
                        elif k[q] == IDENT and t[q] in callables and q + 1 < be and t[q + 1] == '(':
                            self.report(src, src.line[q], src.col[q], 'L18', f'callable parameter {t[q]} invoked while a leaf mutex is held in {fn.display}')
                        elif k[q] == IDENT and self._blocking_token(src, q, be):
                            self.report(src, src.line[q], src.col[q], 'L18', f'{self._blocking_token(src, q, be)} while a leaf mutex is held in '
                                                                             f'{fn.display} (leaf mutexes never block or take Monitors, design 4.1)')
                        q += 1

    @staticmethod
    def _blocking_token(src, q, be):
        """Name of a blocking operation or Monitor acquisition starting at token q, or None."""
        t, k = src.tok, src.kind
        x = t[q]
        call = q + 1 < be and t[q + 1] == '('
        if x == 'SYNCHRONIZED' or (x == 'BlockingRegion' and q + 1 < be and k[q + 1] == IDENT):
            return x
        if x in ('sleep_for', 'sleep_until') and call:
            return f'{x}()'
        if call and t[q - 1] in ('.', '->'):
            if x in ('join', 'acquire', 'tryAcquire'):
                return f'{x}()'
            if x == 'get' and k[q - 2] == IDENT and re.search(r'(?i)future|task', t[q - 2]):
                return 'Future::get()'
        if x.endswith('DAO') and q + 1 < be and t[q + 1] == '::':
            return 'DAO call'
        return None

    def _l20(self, fn, lambdas):
        src = fn.src
        t, k, m = src.tok, src.kind, src.match
        bs, be = fn.body
        for i in range(bs + 1, be):
            if t[i] not in COMPUTE_CALLS or t[i - 1] not in ('.', '->') or i + 1 >= be or t[i + 1] != '(' or m[i + 1] < 0:
                continue
            recv = self._receiver_text(src, i - 2)
            if recv is None:
                continue
            for lam in lambdas:
                if not (i + 1 < lam.cap[0] < m[i + 1]):
                    continue
                for q in range(lam.body[0], lam.body[1]):
                    if t[q] in MAP_WRITES and t[q - 1] in ('.', '->') and q + 1 < lam.body[1] and t[q + 1] == '(':
                        inner = self._receiver_text(src, q - 2)
                        if inner is not None and inner == recv:
                            self.report(src, src.line[q], src.col[q], 'L20', f'{t[q]}() on `{recv}` inside its own {t[i]} callback: a second stripe Monitor '
                                                                             'can deadlock (design 21); move the write out of the callback or guard the map with a Monitor')

    def _receiver_text(self, src, j):
        """Normalized text of the receiver expression ending at token j (identifiers, this->, member access, calls)."""
        t, k, m = src.tok, src.kind, src.match
        parts = []
        while j >= 0:
            x = t[j]
            if x == ')' and m[j] > 0:
                o = m[j]
                parts.insert(0, src.text_of(o, j + 1))
                j = o - 1
                if j >= 0 and k[j] == IDENT:
                    parts.insert(0, t[j])
                    j -= 1
            elif k[j] == IDENT or x == 'this':
                parts.insert(0, x)
                j -= 1
            else:
                return None
            if j >= 0 and t[j] in ('.', '->'):
                parts.insert(0, '.')
                j -= 1
                continue
            if j >= 0 and t[j] == '*' and j > 0 and t[j - 1] == '(':
                return None
            break
        text = ''.join(parts)
        text = re.sub(r'^this\.', '', text)
        return text or None

    # -- token rules: L6 L8 L14 L18
    def _token_rules(self, src):
        t, k = src.tok, src.kind
        area = self.area(src)
        for i, x in enumerate(t):
            if k[i] != IDENT:
                continue
            if area != 'runtime':
                if x in ('thread', 'jthread') and i >= 2 and t[i - 1] == '::' and t[i - 2] == 'std' and (i + 1 >= len(t) or t[i + 1] != '::'):
                    self.report(src, src.line[i], src.col[i], 'L6', f'std::{x} outside runtime/: use the Java-shaped pools')
                elif x == 'async' and i >= 2 and t[i - 1] == '::' and t[i - 2] == 'std':
                    self.report(src, src.line[i], src.col[i], 'L6', 'std::async outside runtime/')
                elif x == 'detach' and i >= 1 and t[i - 1] in ('.', '->') and i + 2 < len(t) and t[i + 1] == '(' and t[i + 2] == ')':
                    self.report(src, src.line[i], src.col[i], 'L6', 'detach() outside runtime/')
            if x in BANNED_CALLS and i + 1 < len(t) and t[i + 1] == '(' and (i == 0 or t[i - 1] not in ('.', '->') and
                                                                            not (t[i - 1] == '::' and i >= 2 and t[i - 2] != 'std')):
                if not (i >= 1 and k[i - 1] == IDENT and t[i - 1] not in ('return',)):
                    self.report(src, src.line[i], src.col[i], 'L8', f'banned C function {x}() (not thread-safe or not deterministic)')
            if area not in ('runtime', 'configs') and x == 'atomic' and i >= 2 and t[i - 1] == '::' and t[i - 2] == 'std' and i + 1 < len(t) and t[i + 1] == '<':
                if not self._member_line(src, src.line[i]):
                    self.report(src, src.line[i], src.col[i], 'L14', 'direct std::atomic<> in game code: prefer Field<> or an Atomic* shim', 'warning')
            if area not in ('runtime', 'network') and x in ('RankedMutex', 'LeafMutex'):
                self.report(src, src.line[i], src.col[i], 'L18', f'{x} outside runtime/ and network/: use a Monitor')

    def _member_line(self, src, line):
        return any(mb.line == line and mb.cls.src is src for c in self.classes if c.src is src for mb in c.members)

    # -- L7
    def _l7(self):
        if self.fm is None:
            return
        per_class = {}
        for fn in self.functions:
            cls = fn.cls or self._owner_class(fn)
            if cls is None:
                continue
            cid = self.java_cid(cls)
            if cid is None or self.area(fn.src) == 'runtime':
                continue
            src = fn.src
            t = src.tok
            bs, be = fn.body
            n_sync = sum(1 for i in range(bs, be) if t[i] == 'SYNCHRONIZED')
            n_lock = sum(1 for i in range(bs + 2, be) if t[i] in ('lock', 'tryLock', 'readLock', 'writeLock') and t[i - 1] in ('.', '->')
                         and i + 2 < be and t[i + 1] == '(' and t[i + 2] == ')')
            d = per_class.setdefault((cid, id(cls)), {}).setdefault(fn.name, [0, 0, fn])
            d[0] += n_sync
            d[1] += n_lock
        for (cid, _), methods in sorted(per_class.items(), key=lambda x: x[0][0]):
            java = self._fm_classes[cid].get('sync', {})
            for name, (n_sync, n_lock, fn) in sorted(methods.items()):
                j = java.get(name, {'synchronized': 0, 'lock': 0})
                if n_sync != j['synchronized'] or n_lock != j['lock']:
                    self.report(fn.src, fn.line, fn.col, 'L7', f'{fn.display}: SYNCHRONIZED/lock() {n_sync}/{n_lock}, Java synchronized/lock() '
                                                              f'{j["synchronized"]}/{j["lock"]}')


# ----------------------------------------------------------------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------------------------------------------------------------

def collect(paths):
    out = []
    for p in paths:
        if os.path.isdir(p):
            for dirpath, dirnames, filenames in os.walk(p):
                dirnames.sort()
                for f in sorted(filenames):
                    if f.endswith(EXTS):
                        out.append(os.path.join(dirpath, f))
        elif os.path.isfile(p):
            out.append(p)
        else:
            raise LintError(f'no such file or directory: {p}')
    return sorted(set(out), key=lambda x: x.replace('\\', '/'))


def display_path(path):
    try:
        rel = os.path.relpath(path, CPP_ROOT)
        if not rel.startswith('..'):
            return rel.replace('\\', '/')
    except ValueError:
        pass
    return path.replace('\\', '/')


def main(argv=None):
    ap = argparse.ArgumentParser(description='Concurrency lint (design runtime-architecture.md §12.2).')
    ap.add_argument('paths', nargs='+')
    ap.add_argument('--fieldmap', default=None, help=f'fieldmap.json (default: {display_path(DEFAULT_FIELDMAP)} when present)')
    ap.add_argument('--no-fieldmap', action='store_true')
    ap.add_argument('--rules', default=None, help='comma-separated rule ids (default: all)')
    ap.add_argument('--cycles', action='store_true', help='L16: report unresolved cycle edges of fieldmap.json')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--werror', action='store_true')
    args = ap.parse_args(argv)
    try:
        fm = None
        if not args.no_fieldmap:
            path = args.fieldmap or (DEFAULT_FIELDMAP if os.path.exists(DEFAULT_FIELDMAP) else None)
            if path:
                fm = load_fieldmap(path)
        rules = None
        if args.rules:
            rules = [r.strip() for r in args.rules.split(',') if r.strip()]
            bad = [r for r in rules if r not in ALL_RULES]
            if bad:
                raise LintError(f'unknown rules {bad}')
        lint = Linter(fm, rules, args.cycles)
        for f in collect(args.paths):
            lint.add_file(f, display_path(f))
        findings = lint.run()
    except LintError as e:
        print(f'lint_concurrency: error: {e}', file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps([f.__dict__ for f in findings], indent=1))
    else:
        for f in findings:
            print(f.format())
        errors = sum(1 for f in findings if f.severity == 'error')
        warnings = len(findings) - errors
        print(f'lint_concurrency: {len(lint.sources)} files, {errors} errors, {warnings} warnings', file=sys.stderr)
    errors = sum(1 for f in findings if f.severity == 'error')
    if errors or (args.werror and findings):
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
