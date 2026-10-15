"""javasrc: tokenizer, declaration-level parser, body helpers and type resolver for the Java sources of this repository.

Shared front end of every generator under cpp/tools/gen (skeleton.py, fieldmap.py, the JAXB generator, ...). Python 3.12 stdlib only.
It covers the Java 25 constructs used in this repository: records, enums with constant bodies, interfaces with default methods,
sealed/non-sealed/permits, switch expressions and rules, pattern matching (instanceof, case, record patterns, `when` guards), text blocks,
`var`, `_`, generics with wildcards and bounds, annotations with arguments, nested/local/anonymous classes, lambdas, method references,
static and instance initializers. Declarations are parsed fully; method bodies, initializers and constant arguments are kept as
bracket-balanced token spans that the body helpers analyse on demand. Anything unexpected raises JavaSyntaxError with path:line:col.

Entry points
------------
    tokenize(text, path='<string>') -> Tokens
    parse_source(text, path='<string>') -> CompilationUnit
    parse_file(path) -> CompilationUnit                     (UTF-8, optional BOM)
    ProjectIndex.from_roots(roots) -> ProjectIndex          (every *.java under the roots, sorted)
    main(argv): python javasrc.py [--ir OUT.json] [--resolve] [--bodies] ROOT...   (prints file/token counts and throughput)

Tokens (parallel lists, index = token number; the last token is EOF)
    kind[i]    one of IDENT, KEYWORD, INT, FLOAT, CHAR, STRING, TEXTBLOCK, OP, EOF (module constants; compare with `is` or ==)
    text[i]    source text of the token ('>' is always a single token, so '>>' shifts are two adjacent '>' tokens)
    start[i], end[i]  character offsets;  match[i]  index of the matching bracket for ( ) [ ] { }, else -1
    parent[i]  index of the innermost enclosing open bracket, or -1;  docs  {token index: javadoc text preceding that token}
    loc(i) -> (line, col) 1-based;  pos_loc(offset);  source_text(start_tok, end_tok) (half-open token range);  token(i) -> Token

Declaration model (all nodes have .line/.col of their name token; lists keep source order)
    CompilationUnit: path, source, tokens, package (str|None), package_annotations, imports [Import], types [TypeDecl],
        root/relpath (set by ProjectIndex.from_roots); all_types() (member types recursively), to_json(index=None, bodies=False)
    Import: name, static, wildcard ('import a.b.*' -> name 'a.b', wildcard True)
    TypeDecl: kind 'class'|'interface'|'enum'|'record'|'annotation'|'anonymous', name ('' for anonymous), modifiers [str],
        annotations [Annotation], type_params [TypeParam], extends [TypeRef] (superclass or super-interfaces; for an anonymous class
        the instantiated type), implements, permits, record_components [Parameter], enum_constants [EnumConstant], fields [FieldDecl],
        methods [MethodDecl] (constructors included), initializers [Initializer], types [TypeDecl] (member types), members (all of
        these in source order), outer (enclosing TypeDecl; for local/anonymous classes the type owning the body), local, anonymous,
        cu, body (Span of '{...}'), doc, start (first token incl. modifiers), index (name token); fqn ('pkg.Outer.Inner', None for
        local/anonymous), binary_name ('pkg.Outer$Inner'), annotation(name) -> Annotation|None (simple or qualified name)
    FieldDecl: modifiers, annotations, type (TypeRef incl. C-style dims), name, initializer (Span|None), declarator_index, owner, doc
    MethodDecl: kind 'method'|'constructor'|'compact_constructor', name, modifiers, annotations, type_params, return_type (None for
        constructors), params [Parameter], throws [TypeRef], body (Span|None), default_value (ElementValue|None), owner, doc
    Parameter: modifiers, annotations, type, name ('this' for a receiver parameter), varargs (type excludes the '...')
    Initializer: static, body (Span), owner;   EnumConstant: name, annotations, args [Span]|None, body (TypeDecl|None), owner, doc
    TypeRef: name (dotted as written, no type args), segments [(name, args|None)], args (last segment: None = no '<>', [] = diamond),
        dims, annotations, wildcard (None | '?' | 'extends' | 'super') with bound, index (first name token); str() renders Java syntax
    Field/Method/Parameter/EnumConstant .index = token index of the name (parameters and local variables alike)
    TypeParam: name, bounds [TypeRef], annotations
    Annotation: name, args [(name, ElementValue)] ('value' for the single-element form, single=True), get(name) -> ElementValue|None
    ElementValue: kind 'literal'|'class'|'name'|'annotation'|'array'|'expr', value (decoded literal), type (for X.class), name,
        annotation, items, text, span

Span (half-open token range [start, end) in cu, with .owner = declaration it belongs to) and its body helpers
    text, line/col, end_line/end_col, texts() (token texts)
    lambdas() -> [Lambda]: params [LambdaParam(name, type|None, index)], span, body (Span), block (bool)
    new_expressions() -> [NewExpr]: type, args [Span]|None, array, dim_exprs [Span], initializer (Span|None), anonymous (TypeDecl|None),
        qualifier (Span|None for 'outer.new X()'), span
    anonymous_classes() -> [NewExpr] with anonymous set (the TypeDecl has fields/methods with their own body spans)
    local_types() -> [TypeDecl] local classes/records/enums/interfaces declared in the span (local=True)
    local_variables() -> [LocalVar] sorted by index: kind 'local'|'for'|'foreach'|'resource'|'catch'|'pattern'|'lambda_param'|'param',
        name, type (None for implicit lambda params), union_types (catch), modifiers, initializer (Span|None), index (name token),
        scope_end (exclusive token index; blocks and flow-scoped patterns are approximated by the enclosing block). 'param' entries are
        the parameters of the owning method (when the span is its body), of methods of anonymous/local classes in the span, and the
        record components inside a compact constructor.
    identifier_refs() -> [IdentRef]: name, index, role 'expr'|'member'|'call'|'type'|'declaration'|'method_ref'|'label',
        qualifier ('this', 'super', other qualifier text, or None). Contextual keywords (yield, when, record, ...) are skipped.
        'expr' names are unclassified simple names (locals, fields, or type/package names before '.').
    assignments() -> [Assignment]: name, qualifier ('this'|text|None), op ('=', '+=', ..., '++', '--'), element (array element
        write 'x[i] = ...'), rhs (Span|None), index (name token), local (simple name shadowed by a visible local variable)
    field_assignments() -> assignments() that are not to visible locals ('this.x = ...' and 'x = ...', element writes included)
    method_calls() -> [MethodCall]: name ('this'/'super' for constructor calls), receiver (Span|None), args [Span], index, span
    method_refs() -> [MethodRef]: receiver (Span), name ('new' for constructor refs), index
    locals_visible_at(index) -> {name: LocalVar}
    enclosing_class(index) -> innermost anonymous/local TypeDecl of the span containing index, or None
    Results are memoized per Span object. Anonymous and local class declarations are parsed once per CompilationUnit.
exercise_bodies(cu) runs all helpers over every body of cu (used by the full-tree test).
ProjectIndex (type resolver over several source roots)
    from_roots(roots) / add_unit(cu); units, types {fqn: TypeDecl}, packages {pkg: {simple: fqn}}, duplicates [(fqn, path, path)]
    lookup(fqn) -> TypeDecl|None
    resolve(name, context) -> fqn str or None. name may be simple or dotted; context is a CompilationUnit, TypeDecl, MethodDecl,
        FieldDecl, Initializer, EnumConstant or Span. Order: type variables in scope, member types of the enclosing types (incl.
        inherited ones), single-type imports (and static imports of member types), the unit's own types, the same package,
        on-demand imports (project packages/types, known JDK/JAXB packages, packages learnt from explicit imports) and java.lang;
        a Span or MethodDecl context also sees the local classes declared in that body. Package-qualified names are matched against
        the index (lower-case first segments outside the project count as external).
    resolve_kind(name, context) -> (kind, value): kind 'project' (fqn), 'external' (fqn), 'external_guess' (fqn built from the only
        wildcard import of an unknown third-party package, when no enclosing type has a non-project supertype), 'primitive',
        'typevar' (name), 'local' (the local TypeDecl), 'ambiguous' ([candidate fqns]) or 'unresolved' (None).
        resolve() returns the fqn for project, external, external_guess and primitive, else None.
    resolve_type(typeref, context) -> dict tree {'name','fqn','kind','args','dims'} or {'wildcard','bound'}
    supertypes(td) -> [TypeDecl] (project types only);  member_type(td, name) -> TypeDecl|None (inherited included)
    to_json(resolve=False, bodies=False) -> {'version': IR_VERSION, 'roots', 'units': [unit IR sorted by root, relpath], 'duplicates'}

JSON IR (CompilationUnit.to_json): keys mirror the attribute names above. Spans are {'tokens': [start, end], 'start': [line, col],
'end': [line, col]} (end = last character); initializers and enum constant arguments also carry 'text' (bodies only with
bodies=True). With an index, every type reference gets 'fqn' and 'resolution' (the resolve_kind kind).
"""
from __future__ import annotations

import bisect
import json
import os
import re
import sys
import time

__all__ = [
    'IDENT', 'KEYWORD', 'INT', 'FLOAT', 'CHAR', 'STRING', 'TEXTBLOCK', 'OP', 'EOF', 'JavaSyntaxError', 'Tokens', 'Token', 'tokenize',
    'literal_value', 'parse_source', 'parse_file', 'CompilationUnit', 'Import', 'TypeDecl', 'FieldDecl', 'MethodDecl', 'Parameter',
    'Initializer', 'EnumConstant', 'TypeRef', 'TypeParam', 'Annotation', 'ElementValue', 'Span', 'Lambda', 'LambdaParam', 'NewExpr',
    'LocalVar', 'IdentRef', 'Assignment', 'MethodCall', 'MethodRef', 'ProjectIndex', 'find_java_files', 'exercise_bodies', 'main',
    'IR_VERSION', 'KNOWN_EXTERNAL_PACKAGES', 'JAVA_LANG',
]

IR_VERSION = 1

# ----------------------------------------------------------------------------------------------------------------------------------
# Tokenizer
# ----------------------------------------------------------------------------------------------------------------------------------

IDENT = 'ident'
KEYWORD = 'keyword'
INT = 'int'
FLOAT = 'float'
CHAR = 'char'
STRING = 'string'
TEXTBLOCK = 'textblock'
OP = 'op'
EOF = 'eof'

KEYWORDS = frozenset(
    'abstract assert boolean break byte case catch char class const continue default do double else enum extends final finally float '
    'for goto if implements import instanceof int interface long native new package private protected public return short static '
    'strictfp super switch synchronized this throw throws transient try void volatile while true false null'.split())
PRIMITIVES = frozenset('boolean byte char short int long float double void'.split())
MODIFIER_KEYWORDS = frozenset('public protected private static abstract final native synchronized transient volatile strictfp default'.split())
LITERAL_KINDS = frozenset((INT, FLOAT, CHAR, STRING, TEXTBLOCK))
OPEN_BRACKETS = frozenset('([{')
CLOSE_BRACKETS = frozenset(')]}')
_PAIR = {')': '(', ']': '[', '}': '{'}
ASSIGN_OPS = frozenset(('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=', '>>>='))

_OPS = ['>>>=', '<<=', '>>=', '...', '->', '::', '++', '--', '&&', '||', '==', '!=', '<=', '>=', '+=', '-=', '*=', '/=', '&=', '|=',
        '^=', '%=', '<<']
_TOKEN_PARTS = [
    ('doc', r'/\*\*(?!/)[^*]*\*+(?:[^/*][^*]*\*+)*/'),
    ('comment', r'//[^\r\n]*|/\*[^*]*\*+(?:[^/*][^*]*\*+)*/'),
    ('badcomment', r'/\*'),
    ('textblock', r'"""[ \t\f]*\r?\n(?:[^"\\]|\\[\s\S]|"(?!""))*"""'),
    ('badtextblock', r'"""'),
    ('string', r'"(?:[^"\\\r\n]|\\.)*"'),
    ('char', r"'(?:[^'\\\r\n]|\\u+[0-9a-fA-F]{4}|\\[0-7]{1,3}|\\.)'"),
    ('badquote', r'["\']'),
    ('number', r'(?:0[xX][0-9a-fA-F_]*(?:\.[0-9a-fA-F_]*)?(?:[pP][+-]?[0-9_]+)?[lLfFdD]?'
               r'|0[bB][01_]+[lL]?'
               r'|(?:[0-9][0-9_]*(?:\.[0-9_]*)?|\.[0-9][0-9_]*)(?:[eE][+-]?[0-9_]+)?[fFdDlL]?)(?![\w$])'),
    ('ident', r'(?:[^\W\d]|\$)[\w$]*'),
    ('op', '|'.join(re.escape(o) for o in _OPS) + r'|[(){}\[\];,.@=><!~?:+\-*/&|^%]'),
    ('eof', r'\Z'),
    ('err', r'[\s\S]'),
]
_TOKEN_RE = re.compile(r'[ \t\f\r\n]*(?:' + '|'.join(f'(?P<{n}>{p})' for n, p in _TOKEN_PARTS) + ')')


class JavaSyntaxError(Exception):
    """A tokenizer or parser error at path:line:col."""

    def __init__(self, path, line, col, message):
        super().__init__(f'{path}:{line}:{col}: {message}')
        self.path = path
        self.line = line
        self.col = col
        self.message = message


class Token:
    __slots__ = ('index', 'kind', 'text', 'line', 'col')

    def __init__(self, index, kind, text, line, col):
        self.index, self.kind, self.text, self.line, self.col = index, kind, text, line, col

    def __repr__(self):
        return f'Token({self.index}, {self.kind}, {self.text!r}, {self.line}:{self.col})'


class Tokens:
    """Token stream of one source file as parallel lists (see module docstring)."""

    __slots__ = ('path', 'source', 'kind', 'text', 'start', 'end', 'match', 'parent', 'docs', '_line_starts')

    def __init__(self, path, source):
        self.path = path
        self.source = source
        self.kind = []
        self.text = []
        self.start = []
        self.end = []
        self.match = []
        self.parent = []
        self.docs = {}
        self._line_starts = None

    def __len__(self):
        return len(self.kind)

    def pos_loc(self, pos):
        if self._line_starts is None:
            starts = [0]
            starts.extend(m.end() for m in re.finditer('\n', self.source))
            self._line_starts = starts
        line = bisect.bisect_right(self._line_starts, pos)
        return line, pos - self._line_starts[line - 1] + 1

    def loc(self, i):
        return self.pos_loc(self.start[min(i, len(self.start) - 1)])

    def token(self, i):
        line, col = self.loc(i)
        return Token(i, self.kind[i], self.text[i], line, col)

    def source_text(self, start, end):
        if end <= start:
            return ''
        return self.source[self.start[start]:self.end[end - 1]]

    def error(self, i, message):
        line, col = self.loc(i)
        return JavaSyntaxError(self.path, line, col, message)


def tokenize(text, path='<string>'):
    """Tokenize Java source text; raises JavaSyntaxError on malformed input or unbalanced brackets."""
    if text.startswith('﻿'):
        text = text[1:]
    tk = Tokens(path, text)
    kinds, texts, starts, ends, match, parent = tk.kind, tk.text, tk.start, tk.end, tk.match, tk.parent
    stack = []
    pending_doc = None
    for m in _TOKEN_RE.finditer(text):
        g = m.lastgroup
        if g == 'comment':
            continue
        if g == 'doc':
            pending_doc = m.group(g)
            continue
        s = m.start(g)
        e = m.end(g)
        if g == 'ident':
            tok = m.group(g)
            kind = KEYWORD if tok in KEYWORDS else IDENT
        elif g == 'op':
            tok = m.group(g)
            kind = OP
        elif g == 'number':
            tok = m.group(g)
            low = tok.lower()
            if low.startswith('0x'):
                kind = FLOAT if ('.' in low or 'p' in low) else INT
            elif low.startswith('0b'):
                kind = INT
            else:
                kind = FLOAT if ('.' in low or 'e' in low or low[-1] in 'fd') else INT
        elif g == 'string':
            tok = m.group(g)
            kind = STRING
        elif g == 'char':
            tok = m.group(g)
            kind = CHAR
        elif g == 'textblock':
            tok = m.group(g)
            kind = TEXTBLOCK
        elif g == 'eof':
            break
        else:
            line, col = tk.pos_loc(s)
            what = {'badcomment': 'unterminated comment', 'badtextblock': 'unterminated text block',
                    'badquote': 'unterminated or malformed string/char literal'}.get(g, f'unexpected character {m.group(g)!r}')
            raise JavaSyntaxError(path, line, col, what)
        idx = len(kinds)
        if pending_doc is not None:
            tk.docs[idx] = pending_doc
            pending_doc = None
        kinds.append(kind)
        texts.append(tok)
        starts.append(s)
        ends.append(e)
        match.append(-1)
        if kind is OP and len(tok) == 1 and (tok in OPEN_BRACKETS or tok in CLOSE_BRACKETS):
            if tok in OPEN_BRACKETS:
                parent.append(stack[-1] if stack else -1)
                stack.append(idx)
            else:
                if not stack or texts[stack[-1]] != _PAIR[tok]:
                    raise tk.error(idx, f'unbalanced {tok!r}')
                o = stack.pop()
                match[o] = idx
                match[idx] = o
                parent.append(stack[-1] if stack else -1)
        else:
            parent.append(stack[-1] if stack else -1)
    if stack:
        raise tk.error(stack[-1], f'unclosed {texts[stack[-1]]!r}')
    n = len(text)
    kinds.append(EOF)
    texts.append('')
    starts.append(n)
    ends.append(n)
    match.append(-1)
    parent.append(-1)
    return tk


_ESCAPES = {'b': '\b', 't': '\t', 'n': '\n', 'f': '\f', 'r': '\r', 's': ' ', '"': '"', "'": "'", '\\': '\\'}


def _unescape(body, path='<string>'):
    if '\\' not in body:
        return body
    out = []
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c != '\\':
            out.append(c)
            i += 1
            continue
        if i + 1 >= n:
            raise JavaSyntaxError(path, 0, 0, f'dangling escape in literal {body!r}')
        d = body[i + 1]
        if d in _ESCAPES:
            out.append(_ESCAPES[d])
            i += 2
        elif d == 'u':
            j = i + 1
            while j < n and body[j] == 'u':
                j += 1
            hexd = body[j:j + 4]
            if len(hexd) != 4 or any(h not in '0123456789abcdefABCDEF' for h in hexd):
                raise JavaSyntaxError(path, 0, 0, f'bad unicode escape in literal {body!r}')
            out.append(chr(int(hexd, 16)))
            i = j + 4
        elif d in '01234567':
            j = i + 1
            limit = 4 if d in '0123' else 3
            while j < n and j < i + limit and body[j] in '01234567':
                j += 1
            out.append(chr(int(body[i + 1:j], 8)))
            i = j
        elif d == '\n':  # text block line continuation
            i += 2
        elif d == '\r':
            i += 3 if body[i + 2:i + 3] == '\n' else 2
        else:
            raise JavaSyntaxError(path, 0, 0, f'bad escape \\{d} in literal {body!r}')
    return ''.join(out)


def _text_block_value(tok):
    content = tok[3:-3]
    content = content[content.index('\n') + 1:].replace('\r\n', '\n').replace('\r', '\n')
    lines = content.split('\n')
    # The last line is the closing delimiter line; it counts for indentation (JLS 3.10.6) even when blank.
    significant = [ln for ln in lines[:-1] if ln.strip(' \t\f')]
    last = lines[-1]
    if not last.strip(' \t\f'):
        significant.append(last)
    else:
        significant.append(last)
    indent = min((len(ln) - len(ln.lstrip(' \t\f')) for ln in significant), default=0)
    stripped = []
    for idx, ln in enumerate(lines):
        if idx == len(lines) - 1 and not ln.strip(' \t\f'):
            stripped.append('')
            continue
        stripped.append(ln[indent:].rstrip(' \t\f') if ln.strip(' \t\f') else '')
    return _unescape('\n'.join(stripped))


def literal_value(kind, text):
    """Decode a literal token: INT -> int, FLOAT -> float, CHAR/STRING/TEXTBLOCK -> str, true/false -> bool, null -> None."""
    if kind is INT or kind == INT:
        t = text.replace('_', '').rstrip('lL')
        low = t.lower()
        if low.startswith('0x'):
            v = int(low[2:], 16)
        elif low.startswith('0b'):
            v = int(low[2:], 2)
        elif len(t) > 1 and t.startswith('0'):
            v = int(t, 8)
        else:
            v = int(t)
        return v
    if kind is FLOAT or kind == FLOAT:
        t = text.replace('_', '')
        if t.lower().startswith('0x'):
            return float.fromhex(t.rstrip('fFdD'))
        return float(t.rstrip('fFdD'))
    if kind is STRING or kind == STRING:
        return _unescape(text[1:-1])
    if kind is CHAR or kind == CHAR:
        return _unescape(text[1:-1])
    if kind is TEXTBLOCK or kind == TEXTBLOCK:
        return _text_block_value(text)
    if text == 'true':
        return True
    if text == 'false':
        return False
    if text == 'null':
        return None
    raise ValueError(f'not a literal: {text!r}')


# ----------------------------------------------------------------------------------------------------------------------------------
# Model
# ----------------------------------------------------------------------------------------------------------------------------------

def _loc_json(node):
    return {'line': node.line, 'col': node.col}


class TypeRef:
    """A type as written: segments with type arguments, array dims, type annotations, or a wildcard."""

    __slots__ = ('segments', 'dims', 'annotations', 'wildcard', 'bound', 'line', 'col', 'index')

    def __init__(self, segments, dims=0, annotations=None, wildcard=None, bound=None, line=0, col=0, index=-1):
        self.segments = segments  # [(name, args|None)]
        self.dims = dims
        self.annotations = annotations or []
        self.wildcard = wildcard  # None, '?', 'extends', 'super'
        self.bound = bound
        self.line = line
        self.col = col
        self.index = index  # token index of the first name segment (or '?')

    @property
    def name(self):
        if self.wildcard:
            return '?'
        return '.'.join(s[0] for s in self.segments)

    @property
    def args(self):
        return self.segments[-1][1] if self.segments else None

    @property
    def is_primitive(self):
        return not self.wildcard and len(self.segments) == 1 and self.segments[0][0] in PRIMITIVES

    def with_dims(self, extra):
        return TypeRef(self.segments, self.dims + extra, self.annotations, self.wildcard, self.bound, self.line, self.col, self.index)

    def __str__(self):
        if self.wildcard:
            if self.wildcard == '?':
                return '?'
            return f'? {self.wildcard} {self.bound}'
        parts = []
        for n, a in self.segments:
            parts.append(n if a is None else f"{n}<{', '.join(str(x) for x in a)}>")
        return '.'.join(parts) + '[]' * self.dims

    def __repr__(self):
        return f'TypeRef({self})'

    def to_json(self, index=None, context=None):
        if self.wildcard:
            d = {'wildcard': self.wildcard}
            if self.bound is not None:
                d['bound'] = self.bound.to_json(index, context)
            return d
        d = {'name': self.name}
        if index is not None:
            kind, value = index.resolve_kind(self.name, context)
            d['fqn'] = value if kind in _RESOLVED_KINDS else None
            d['resolution'] = kind
        args = self.args
        if args is not None:
            d['args'] = [a.to_json(index, context) for a in args]
        if any(a is not None for _, a in self.segments[:-1]):
            d['segments'] = [{'name': n, 'args': None if a is None else [x.to_json(index, context) for x in a]} for n, a in self.segments]
        if self.dims:
            d['dims'] = self.dims
        if self.annotations:
            d['annotations'] = [a.to_json() for a in self.annotations]
        return d


class TypeParam:
    __slots__ = ('name', 'bounds', 'annotations', 'line', 'col')

    def __init__(self, name, bounds, annotations, line, col):
        self.name, self.bounds, self.annotations, self.line, self.col = name, bounds, annotations, line, col

    def to_json(self, index=None, context=None):
        return {'name': self.name, 'bounds': [b.to_json(index, context) for b in self.bounds],
                'annotations': [a.to_json() for a in self.annotations], **_loc_json(self)}


class Annotation:
    __slots__ = ('name', 'args', 'single', 'has_parens', 'line', 'col')

    def __init__(self, name, line, col):
        self.name = name
        self.args = []
        self.single = False
        self.has_parens = False
        self.line = line
        self.col = col

    @property
    def simple_name(self):
        return self.name.rpartition('.')[2]

    def get(self, name):
        for n, v in self.args:
            if n == name:
                return v
        return None

    def __repr__(self):
        return f'@{self.name}({", ".join(f"{n}={v!r}" for n, v in self.args)})'

    def to_json(self):
        return {'name': self.name, 'args': [{'name': n, 'value': v.to_json()} for n, v in self.args], 'single': self.single, **_loc_json(self)}


class ElementValue:
    """Annotation element value: literal, X.class, a (qualified) name, nested annotation, array, or any other expression."""

    __slots__ = ('kind', 'value', 'type', 'name', 'annotation', 'items', 'span')

    def __init__(self, kind, span, value=None, type=None, name=None, annotation=None, items=None):
        self.kind, self.span, self.value, self.type, self.name, self.annotation, self.items = kind, span, value, type, name, annotation, items

    @property
    def text(self):
        return self.span.text

    def __repr__(self):
        return f'ElementValue({self.kind}, {self.text!r})'

    def to_json(self):
        d = {'kind': self.kind, 'text': self.text}
        if self.kind == 'literal':
            d['value'] = self.value
        elif self.kind == 'class':
            d['type'] = self.type.to_json()
        elif self.kind == 'name':
            d['name'] = self.name
        elif self.kind == 'annotation':
            d['annotation'] = self.annotation.to_json()
        elif self.kind == 'array':
            d['items'] = [x.to_json() for x in self.items]
        return d


class Import:
    __slots__ = ('name', 'static', 'wildcard', 'line', 'col')

    def __init__(self, name, static, wildcard, line, col):
        self.name, self.static, self.wildcard, self.line, self.col = name, static, wildcard, line, col

    def __repr__(self):
        return f"Import({'static ' if self.static else ''}{self.name}{'.*' if self.wildcard else ''})"

    def to_json(self):
        return {'name': self.name, 'static': self.static, 'wildcard': self.wildcard, **_loc_json(self)}


class Parameter:
    __slots__ = ('modifiers', 'annotations', 'type', 'name', 'varargs', 'line', 'col', 'index')

    def __init__(self, modifiers, annotations, type, name, varargs, line, col, index):
        self.modifiers, self.annotations, self.type, self.name, self.varargs = modifiers, annotations, type, name, varargs
        self.line, self.col, self.index = line, col, index

    def __repr__(self):
        return f'Parameter({self.type} {self.name})'

    def to_json(self, index=None, context=None):
        return {'name': self.name, 'type': self.type.to_json(index, context), 'varargs': self.varargs, 'modifiers': self.modifiers,
                'annotations': [a.to_json() for a in self.annotations], **_loc_json(self)}


class TypeDecl:
    __slots__ = ('kind', 'name', 'modifiers', 'annotations', 'type_params', 'extends', 'implements', 'permits', 'record_components',
                 'enum_constants', 'fields', 'methods', 'initializers', 'types', 'members', 'outer', 'local', 'anonymous', 'cu', 'body',
                 'doc', 'line', 'col', 'start', 'index')

    def __init__(self, kind, name, cu, outer, line, col, start, index=-1):
        self.kind = kind
        self.name = name
        self.modifiers = []
        self.annotations = []
        self.type_params = []
        self.extends = []
        self.implements = []
        self.permits = []
        self.record_components = []
        self.enum_constants = []
        self.fields = []
        self.methods = []
        self.initializers = []
        self.types = []
        self.members = []
        self.outer = outer
        self.local = False
        self.anonymous = kind == 'anonymous'
        self.cu = cu
        self.body = None
        self.doc = None
        self.line = line
        self.col = col
        self.start = start  # first token of the declaration (modifiers included)
        self.index = index  # name token (for anonymous classes: the 'new' token or the enum constant name)

    def __repr__(self):
        return f'TypeDecl({self.kind} {self.fqn or self.name or "<anonymous>"})'

    @property
    def fqn(self):
        if self.local or self.anonymous:
            return None
        if self.outer is not None:
            o = self.outer.fqn
            return None if o is None else f'{o}.{self.name}'
        return f'{self.cu.package}.{self.name}' if self.cu.package else self.name

    @property
    def binary_name(self):
        if self.local or self.anonymous:
            return None
        if self.outer is not None:
            o = self.outer.binary_name
            return None if o is None else f'{o}${self.name}'
        return self.fqn

    def has_modifier(self, m):
        return m in self.modifiers

    def annotation(self, simple_or_qualified):
        for a in self.annotations:
            if a.name == simple_or_qualified or a.simple_name == simple_or_qualified:
                return a
        return None

    def to_json(self, index=None, bodies=False):
        ctx = self
        d = {'kind': self.kind, 'name': self.name, 'fqn': self.fqn, **_loc_json(self), 'modifiers': self.modifiers,
             'annotations': [a.to_json() for a in self.annotations], 'type_params': [p.to_json(index, ctx) for p in self.type_params],
             'extends': [t.to_json(index, ctx) for t in self.extends], 'implements': [t.to_json(index, ctx) for t in self.implements],
             'permits': [t.to_json(index, ctx) for t in self.permits]}
        if self.kind == 'record':
            d['record_components'] = [p.to_json(index, ctx) for p in self.record_components]
        if self.kind == 'enum':
            d['enum_constants'] = [c.to_json(index, bodies) for c in self.enum_constants]
        d['fields'] = [f.to_json(index, bodies) for f in self.fields]
        d['methods'] = [m.to_json(index, bodies) for m in self.methods]
        d['initializers'] = [i.to_json(bodies) for i in self.initializers]
        d['types'] = [t.to_json(index, bodies) for t in self.types]
        d['body'] = self.body.to_json(bodies) if self.body is not None else None
        d['doc'] = self.doc
        return d


class FieldDecl:
    __slots__ = ('modifiers', 'annotations', 'type', 'name', 'initializer', 'declarator_index', 'owner', 'doc', 'line', 'col', 'index')

    def __init__(self, modifiers, annotations, type, name, owner, doc, line, col, index):
        self.modifiers, self.annotations, self.type, self.name, self.owner, self.doc = modifiers, annotations, type, name, owner, doc
        self.initializer = None
        self.declarator_index = 0
        self.line, self.col, self.index = line, col, index

    def __repr__(self):
        return f'FieldDecl({self.type} {self.name})'

    def to_json(self, index=None, bodies=False):
        return {'name': self.name, **_loc_json(self), 'modifiers': self.modifiers, 'annotations': [a.to_json() for a in self.annotations],
                'type': self.type.to_json(index, self.owner), 'declarator_index': self.declarator_index,
                'initializer': self.initializer.to_json(True) if self.initializer is not None else None, 'doc': self.doc}


class MethodDecl:
    __slots__ = ('kind', 'name', 'modifiers', 'annotations', 'type_params', 'return_type', 'params', 'throws', 'body', 'default_value',
                 'owner', 'doc', 'line', 'col', 'index')

    def __init__(self, kind, name, modifiers, annotations, type_params, owner, doc, line, col, index):
        self.kind, self.name, self.modifiers, self.annotations, self.type_params = kind, name, modifiers, annotations, type_params
        self.return_type = None
        self.params = []
        self.throws = []
        self.body = None
        self.default_value = None
        self.owner, self.doc, self.line, self.col, self.index = owner, doc, line, col, index

    def __repr__(self):
        return f'MethodDecl({self.kind} {self.name}({", ".join(str(p.type) for p in self.params)}))'

    def to_json(self, index=None, bodies=False):
        ctx = self
        return {'kind': self.kind, 'name': self.name, **_loc_json(self), 'modifiers': self.modifiers,
                'annotations': [a.to_json() for a in self.annotations], 'type_params': [p.to_json(index, ctx) for p in self.type_params],
                'return_type': self.return_type.to_json(index, ctx) if self.return_type is not None else None,
                'params': [p.to_json(index, ctx) for p in self.params], 'throws': [t.to_json(index, ctx) for t in self.throws],
                'default_value': self.default_value.to_json() if self.default_value is not None else None,
                'body': self.body.to_json(bodies) if self.body is not None else None, 'doc': self.doc}


class Initializer:
    __slots__ = ('static', 'body', 'owner', 'line', 'col')

    def __init__(self, static, body, owner, line, col):
        self.static, self.body, self.owner, self.line, self.col = static, body, owner, line, col

    def to_json(self, bodies=False):
        return {'static': self.static, **_loc_json(self), 'body': self.body.to_json(bodies)}


class EnumConstant:
    __slots__ = ('name', 'annotations', 'args', 'body', 'owner', 'doc', 'line', 'col', 'index')

    def __init__(self, name, annotations, owner, doc, line, col, index):
        self.name, self.annotations, self.owner, self.doc, self.line, self.col, self.index = name, annotations, owner, doc, line, col, index
        self.args = None
        self.body = None

    def __repr__(self):
        return f'EnumConstant({self.name})'

    def to_json(self, index=None, bodies=False):
        return {'name': self.name, **_loc_json(self), 'annotations': [a.to_json() for a in self.annotations],
                'args': None if self.args is None else [a.to_json(True) for a in self.args],
                'body': self.body.to_json(index, bodies) if self.body is not None else None, 'doc': self.doc}


class CompilationUnit:
    __slots__ = ('path', 'tokens', 'package', 'package_annotations', 'imports', 'types', 'root', 'relpath', '_anon', '_local')

    def __init__(self, path, tokens):
        self.path = path
        self.tokens = tokens
        self.package = None
        self.package_annotations = []
        self.imports = []
        self.types = []
        self.root = None
        self.relpath = None
        self._anon = {}
        self._local = {}

    @property
    def source(self):
        return self.tokens.source

    def __repr__(self):
        return f'CompilationUnit({self.path})'

    def all_types(self):
        out = []

        def walk(ts):
            for t in ts:
                out.append(t)
                walk(t.types)
        walk(self.types)
        return out

    def to_json(self, index=None, bodies=False):
        return {'path': self.relpath or self.path, 'root': self.root, 'package': self.package,
                'package_annotations': [a.to_json() for a in self.package_annotations], 'imports': [i.to_json() for i in self.imports],
                'types': [t.to_json(index, bodies) for t in self.types]}


# ----------------------------------------------------------------------------------------------------------------------------------
# Parser
# ----------------------------------------------------------------------------------------------------------------------------------

class _Parser:
    __slots__ = ('cu', 'tk', 'k', 't', 'm', 'i')

    def __init__(self, cu, i=0):
        self.cu = cu
        self.tk = cu.tokens
        self.k = self.tk.kind
        self.t = self.tk.text
        self.m = self.tk.match
        self.i = i

    # -- primitives
    def error(self, message, i=None):
        return self.tk.error(self.i if i is None else i, message)

    def expect(self, text):
        if self.t[self.i] != text:
            raise self.error(f'expected {text!r}, found {self.t[self.i]!r}')
        self.i += 1

    def ident(self):
        i = self.i
        if self.k[i] is not IDENT:
            raise self.error(f'expected identifier, found {self.t[i]!r}')
        self.i = i + 1
        return self.t[i]

    def qualified_name(self):
        parts = [self.ident()]
        while self.t[self.i] == '.' and self.k[self.i + 1] is IDENT:
            self.i += 1
            parts.append(self.ident())
        return '.'.join(parts)

    def loc(self, i):
        return self.tk.loc(i)

    def span(self, s, e, owner=None):
        return Span(self.cu, s, e, owner)

    # -- compilation unit
    def parse_compilation_unit(self):
        cu = self.cu
        t, k = self.t, self.k
        start = self.i
        anns = []
        while t[self.i] == '@' and t[self.i + 1] != 'interface':
            anns.append(self.parse_annotation())
        if t[self.i] == 'package' and k[self.i] is KEYWORD:
            self.i += 1
            cu.package = self.qualified_name()
            cu.package_annotations = anns
            self.expect(';')
        else:
            self.i = start
        while True:
            if t[self.i] == ';':
                self.i += 1
            elif t[self.i] == 'import' and k[self.i] is KEYWORD:
                line, col = self.loc(self.i)
                self.i += 1
                static = False
                if t[self.i] == 'static':
                    static = True
                    self.i += 1
                parts = [self.ident()]
                wildcard = False
                while t[self.i] == '.':
                    self.i += 1
                    if t[self.i] == '*':
                        self.i += 1
                        wildcard = True
                        break
                    parts.append(self.ident())
                self.expect(';')
                cu.imports.append(Import('.'.join(parts), static, wildcard, line, col))
            else:
                break
        while k[self.i] is not EOF:
            if t[self.i] == ';':
                self.i += 1
                continue
            doc = self.tk.docs.get(self.i)
            s = self.i
            mods, anns = self.parse_modifiers()
            if not self.at_type_decl():
                raise self.error(f'expected a type declaration, found {t[self.i]!r}')
            cu.types.append(self.parse_type_decl(mods, anns, None, doc, s))
        return cu

    # -- modifiers and annotations
    def parse_modifiers(self):
        t, k = self.t, self.k
        mods = []
        anns = []
        while True:
            i = self.i
            x = t[i]
            kind = k[i]
            if x == '@' and t[i + 1] != 'interface':
                anns.append(self.parse_annotation())
            elif kind is KEYWORD and x in MODIFIER_KEYWORDS:
                if x == 'default' and t[i + 1] in (':', '->'):
                    break
                mods.append(x)
                self.i = i + 1
            elif kind is IDENT and x == 'sealed' and (k[i + 1] is KEYWORD or t[i + 1] in ('@', 'sealed', 'non', 'record')):
                mods.append('sealed')
                self.i = i + 1
            elif (kind is IDENT and x == 'non' and t[i + 1] == '-' and t[i + 2] == 'sealed'
                  and self.tk.end[i] == self.tk.start[i + 1] and self.tk.end[i + 1] == self.tk.start[i + 2]):
                mods.append('non-sealed')
                self.i = i + 3
            else:
                break
        return mods, anns

    def parse_annotation(self):
        self.expect('@')
        line, col = self.loc(self.i)
        ann = Annotation(self.qualified_name(), line, col)
        t = self.t
        if t[self.i] == '(':
            ann.has_parens = True
            close = self.m[self.i]
            self.i += 1
            if self.i != close:
                if self.k[self.i] is IDENT and t[self.i + 1] == '=':
                    while True:
                        n = self.ident()
                        self.expect('=')
                        ann.args.append((n, self.parse_element_value()))
                        if t[self.i] == ',' and self.i < close:
                            self.i += 1
                            continue
                        break
                else:
                    ann.args.append(('value', self.parse_element_value()))
                    ann.single = True
            if self.i != close:
                raise self.error(f'unexpected {t[self.i]!r} in annotation @{ann.name}')
            self.i = close + 1
        return ann

    def parse_element_value(self):
        t = self.t
        s = self.i
        if t[s] == '@':
            a = self.parse_annotation()
            return ElementValue('annotation', self.span(s, self.i), annotation=a)
        if t[s] == '{':
            close = self.m[s]
            self.i = s + 1
            items = []
            while self.i < close:
                items.append(self.parse_element_value())
                if t[self.i] == ',':
                    self.i += 1
                elif self.i != close:
                    raise self.error(f'unexpected {t[self.i]!r} in annotation array')
            self.i = close + 1
            return ElementValue('array', self.span(s, self.i), items=items)
        e = self.expr_end(s)
        if e == s:
            raise self.error('expected annotation element value')
        self.i = e
        return self.classify_value(s, e)

    def classify_value(self, s, e):
        t, k = self.t, self.k
        span = self.span(s, e)
        n = e - s
        if n == 1 and (k[s] in LITERAL_KINDS or (k[s] is KEYWORD and t[s] in ('true', 'false', 'null'))):
            return ElementValue('literal', span, value=literal_value(k[s], t[s]))
        if n == 2 and t[s] in ('-', '+') and k[s + 1] in (INT, FLOAT):
            v = literal_value(k[s + 1], t[s + 1])
            return ElementValue('literal', span, value=-v if t[s] == '-' else v)
        if n >= 3 and t[e - 1] == 'class' and t[e - 2] == '.':
            sub = _Parser(self.cu, s)
            try:
                ty = sub.parse_type()
            except JavaSyntaxError:
                ty = None
            if ty is not None and sub.i == e - 2:
                return ElementValue('class', span, type=ty)
        if k[s] is IDENT and n % 2 == 1:
            ok = True
            for j in range(s + 1, e, 2):
                if t[j] != '.' or k[j + 1] is not IDENT:
                    ok = False
                    break
            if ok:
                return ElementValue('name', span, name=''.join(t[s:e]))
        return ElementValue('expr', span)

    # -- expressions (extent only)
    def expr_end(self, i):
        """Index of the first token at bracket depth 0 that ends an expression starting at i: ',' ';' ')' ']' '}'."""
        t, k, m = self.t, self.k, self.m
        while True:
            kind = k[i]
            if kind is OP:
                x = t[i]
                if x in OPEN_BRACKETS:
                    i = m[i] + 1
                    continue
                if x == ',' or x == ';' or x in CLOSE_BRACKETS:
                    return i
                if x == '.' and t[i + 1] == '<':
                    i = self.skip_type_args(i + 1)
                    continue
                i += 1
            elif kind is KEYWORD:
                x = t[i]
                if x == 'new' and t[i - 1] != '::':
                    i = self.skip_new_type(i)
                elif x == 'instanceof':
                    i += 1
                    while t[i] == 'final':
                        i += 1
                    sub = _Parser(self.cu, i)
                    sub.parse_type()
                    i = sub.i
                else:
                    i += 1
            elif kind is EOF:
                raise self.error('unexpected end of file in expression', i)
            else:
                i += 1

    def skip_type_args(self, i):
        sub = _Parser(self.cu, i)
        sub.parse_type_args()
        return sub.i

    def skip_new_type(self, i):
        """i at 'new'; returns the index after the instantiated type (before '(' '[' or '{')."""
        sub = _Parser(self.cu, i + 1)
        if self.t[sub.i] == '<':
            sub.parse_type_args()
        sub.parse_type(allow_dims=False)
        return sub.i

    # -- types
    def parse_type(self, allow_dims=True):
        t, k = self.t, self.k
        anns = []
        while t[self.i] == '@':
            anns.append(self.parse_annotation())
        i = self.i
        line, col = self.loc(i)
        if k[i] is KEYWORD and t[i] in PRIMITIVES:
            segs = [(t[i], None)]
            self.i = i + 1
        elif k[i] is IDENT:
            segs = []
            while True:
                n = t[self.i]
                self.i += 1
                args = self.parse_type_args() if t[self.i] == '<' else None
                segs.append((n, args))
                if t[self.i] == '.':
                    j = self.i + 1
                    while t[j] == '@':  # a.@A B
                        sub = _Parser(self.cu, j)
                        sub.parse_annotation()
                        j = sub.i
                    if k[j] is IDENT:
                        self.i = j
                        continue
                break
        else:
            raise self.error(f'expected type, found {t[i]!r}')
        dims = self.parse_dims() if allow_dims else 0
        return TypeRef(segs, dims, anns, line=line, col=col, index=i)

    def parse_dims(self):
        t = self.t
        dims = 0
        while True:
            i = self.i
            if t[i] == '@':
                j = i
                while t[j] == '@':
                    sub = _Parser(self.cu, j)
                    sub.parse_annotation()
                    j = sub.i
                if t[j] == '[' and t[j + 1] == ']':
                    self.i = j
                    continue
                return dims
            if t[i] == '[' and t[i + 1] == ']':
                dims += 1
                self.i = i + 2
            else:
                return dims

    def parse_type_args(self):
        t = self.t
        self.expect('<')
        if t[self.i] == '>':
            self.i += 1
            return []
        args = []
        while True:
            anns = []
            while t[self.i] == '@':
                anns.append(self.parse_annotation())
            if t[self.i] == '?':
                q = self.i
                line, col = self.loc(q)
                self.i += 1
                if t[self.i] in ('extends', 'super'):
                    w = t[self.i]
                    self.i += 1
                    args.append(TypeRef([], 0, anns, w, self.parse_type(), line, col, q))
                else:
                    args.append(TypeRef([], 0, anns, '?', None, line, col, q))
            else:
                ty = self.parse_type()
                ty.annotations = anns + ty.annotations
                args.append(ty)
            if t[self.i] == ',':
                self.i += 1
                continue
            self.expect('>')
            return args

    def parse_type_params(self):
        t = self.t
        self.expect('<')
        out = []
        while True:
            anns = []
            while t[self.i] == '@':
                anns.append(self.parse_annotation())
            line, col = self.loc(self.i)
            name = self.ident()
            bounds = []
            if t[self.i] == 'extends':
                self.i += 1
                bounds.append(self.parse_type())
                while t[self.i] == '&':
                    self.i += 1
                    bounds.append(self.parse_type())
            out.append(TypeParam(name, bounds, anns, line, col))
            if t[self.i] == ',':
                self.i += 1
                continue
            self.expect('>')
            return out

    def parse_type_list(self):
        out = [self.parse_type()]
        while self.t[self.i] == ',':
            self.i += 1
            out.append(self.parse_type())
        return out

    # -- declarations
    def at_type_decl(self):
        i = self.i
        t, k = self.t, self.k
        x = t[i]
        if k[i] is KEYWORD:
            return x in ('class', 'interface', 'enum')
        if x == '@':
            return t[i + 1] == 'interface'
        return x == 'record' and k[i] is IDENT and k[i + 1] is IDENT and t[i + 2] in ('(', '<')

    def parse_type_decl(self, mods, anns, outer, doc, start):
        t = self.t
        x = t[self.i]
        if x == '@':
            kind = 'annotation'
            self.i += 2
        else:
            kind = x
            self.i += 1
        name_i = self.i
        line, col = self.loc(name_i)
        name = self.ident()
        td = TypeDecl(kind, name, self.cu, outer, line, col, start, name_i)
        td.modifiers = mods
        td.annotations = anns
        td.doc = doc
        if t[self.i] == '<':
            if kind in ('enum', 'annotation'):
                raise self.error(f'type parameters on {kind}')
            td.type_params = self.parse_type_params()
        if kind == 'record':
            if t[self.i] != '(':
                raise self.error("expected '(' after record name")
            td.record_components = self.parse_params()
        while True:
            x = t[self.i]
            if x == 'extends':
                self.i += 1
                if kind == 'class':
                    td.extends = [self.parse_type()]
                elif kind == 'interface':
                    td.extends = self.parse_type_list()
                else:
                    raise self.error(f'extends on {kind}', self.i - 1)
            elif x == 'implements':
                self.i += 1
                if kind == 'interface' or kind == 'annotation':
                    raise self.error(f'implements on {kind}', self.i - 1)
                td.implements = self.parse_type_list()
            elif x == 'permits' and self.k[self.i] is IDENT:
                self.i += 1
                td.permits = self.parse_type_list()
            else:
                break
        if t[self.i] != '{':
            raise self.error(f"expected '{{' to open the body of {name}, found {t[self.i]!r}")
        self.parse_body(td)
        return td

    def parse_body(self, td):
        """self.i at '{' of a class/interface/enum/record/annotation/anonymous body; parses members and moves past '}'."""
        open_i = self.i
        close = self.m[open_i]
        td.body = self.span(open_i, close + 1, td)
        self.i = open_i + 1
        if td.kind == 'enum':
            self.parse_enum_constants(td, close)
        while self.i < close:
            self.parse_member(td)
        if self.i != close:
            raise self.error('declaration runs past the end of the body')
        self.i = close + 1

    def parse_enum_constants(self, td, close):
        t = self.t
        while self.i < close and t[self.i] != ';':
            doc = self.tk.docs.get(self.i)
            anns = []
            while t[self.i] == '@':
                anns.append(self.parse_annotation())
            idx = self.i
            line, col = self.loc(idx)
            name = self.ident()
            c = EnumConstant(name, anns, td, doc, line, col, idx)
            if t[self.i] == '(':
                c.args = self.split_args(self.i, c)
                self.i = self.m[self.i] + 1
            if t[self.i] == '{':
                body = TypeDecl('anonymous', '', self.cu, td, line, col, idx, idx)
                self.parse_body(body)
                c.body = body
            td.enum_constants.append(c)
            td.members.append(c)
            if t[self.i] == ',':
                self.i += 1
                continue
            break
        if t[self.i] == ';':
            self.i += 1
        elif self.i != close:
            raise self.error(f'unexpected {t[self.i]!r} after enum constants')

    def split_args(self, open_i, owner=None):
        """Argument spans of a '(' ... ')' list."""
        t = self.t
        close = self.m[open_i]
        out = []
        i = open_i + 1
        if i == close:
            return out
        while True:
            e = self.expr_end(i)
            if e == i:
                raise self.error('empty argument', i)
            out.append(Span(self.cu, i, e, owner))
            if e == close:
                return out
            if t[e] != ',':
                raise self.error(f'unexpected {t[e]!r} in argument list', e)
            i = e + 1

    def parse_member(self, td):
        t, k = self.t, self.k
        s = self.i
        x = t[s]
        if x == ';':
            self.i += 1
            return
        doc = self.tk.docs.get(s)
        if x == '{' or (x == 'static' and t[s + 1] == '{'):
            static = x == 'static'
            b = s + 1 if static else s
            line, col = self.loc(s)
            init = Initializer(static, None, td, line, col)
            init.body = self.span(b, self.m[b] + 1, init)
            self.i = self.m[b] + 1
            td.initializers.append(init)
            td.members.append(init)
            return
        mods, anns = self.parse_modifiers()
        if self.at_type_decl():
            nested = self.parse_type_decl(mods, anns, td, doc, s)
            td.types.append(nested)
            td.members.append(nested)
            return
        tparams = self.parse_type_params() if t[self.i] == '<' else []
        i = self.i
        if k[i] is IDENT and t[i + 1] == '(':
            if t[i] != td.name or td.kind in ('interface', 'annotation', 'anonymous'):
                raise self.error(f'method {t[i]!r} without a return type (not a constructor of {td.name or "an anonymous class"})')
            line, col = self.loc(i)
            md = MethodDecl('constructor', t[i], mods, anns, tparams, td, doc, line, col, i)
            self.i = i + 1
            md.params = self.parse_params()
            self.parse_method_tail(md)
            td.methods.append(md)
            td.members.append(md)
            return
        if td.kind == 'record' and k[i] is IDENT and t[i] == td.name and t[i + 1] == '{':
            line, col = self.loc(i)
            md = MethodDecl('compact_constructor', t[i], mods, anns, tparams, td, doc, line, col, i)
            self.i = i + 1
            self.parse_method_tail(md)
            td.methods.append(md)
            td.members.append(md)
            return
        rtype = self.parse_type()
        i = self.i
        line, col = self.loc(i)
        name = self.ident()
        if t[self.i] == '(':
            md = MethodDecl('method', name, mods, anns, tparams, td, doc, line, col, i)
            md.return_type = rtype
            md.params = self.parse_params()
            extra = self.parse_dims()
            if extra:
                md.return_type = rtype.with_dims(extra)
            self.parse_method_tail(md)
            td.methods.append(md)
            td.members.append(md)
            return
        if tparams:
            raise self.error('type parameters on a field', i)
        n = 0
        while True:
            dims = self.parse_dims()
            f = FieldDecl(mods, anns, rtype.with_dims(dims) if dims else rtype, name, td, doc, line, col, i)
            f.declarator_index = n
            if t[self.i] == '=':
                self.i += 1
                b = self.i
                e = self.expr_end(b)
                if e == b:
                    raise self.error('missing field initializer')
                f.initializer = self.span(b, e, f)
                self.i = e
            td.fields.append(f)
            td.members.append(f)
            if t[self.i] == ',':
                self.i += 1
                n += 1
                i = self.i
                line, col = self.loc(i)
                name = self.ident()
                continue
            self.expect(';')
            return

    def parse_method_tail(self, md):
        t = self.t
        if t[self.i] == 'throws':
            self.i += 1
            md.throws = self.parse_type_list()
        if t[self.i] == 'default' and md.owner.kind == 'annotation':
            self.i += 1
            md.default_value = self.parse_element_value()
            self.expect(';')
            return
        if t[self.i] == '{':
            b = self.i
            md.body = self.span(b, self.m[b] + 1, md)
            self.i = self.m[b] + 1
        elif t[self.i] == ';':
            self.i += 1
        else:
            raise self.error(f"expected method body or ';', found {t[self.i]!r}")

    def parse_params(self, allow_implicit=False):
        """self.i at '('; returns [Parameter] and moves past ')'. With allow_implicit, bare names (lambda params) get type None."""
        t, k = self.t, self.k
        close = self.m[self.i]
        self.i += 1
        params = []
        while self.i < close:
            s = self.i
            if allow_implicit and k[s] is IDENT and (t[s + 1] == ',' or s + 1 == close):
                line, col = self.loc(s)
                params.append(Parameter([], [], None, t[s], False, line, col, s))
                self.i = s + 1
            else:
                mods, anns = self.parse_modifiers()
                ptype = self.parse_type()
                varargs = False
                while t[self.i] == '@':
                    ptype.annotations.append(self.parse_annotation())
                if t[self.i] == '...':
                    varargs = True
                    self.i += 1
                i = self.i
                line, col = self.loc(i)
                if t[i] == 'this':
                    name = 'this'
                    self.i += 1
                elif k[i] is IDENT and t[i + 1] == '.' and t[i + 2] == 'this':
                    name = t[i] + '.this'
                    self.i += 3
                else:
                    name = self.ident()
                dims = self.parse_dims()
                params.append(Parameter(mods, anns, ptype.with_dims(dims) if dims else ptype, name, varargs, line, col, i))
            if t[self.i] == ',':
                self.i += 1
                if self.i == close:
                    raise self.error('trailing comma in parameter list')
            elif self.i != close:
                raise self.error(f'unexpected {t[self.i]!r} in parameter list')
        self.i = close + 1
        return params


def parse_source(text, path='<string>'):
    """Parse one Java compilation unit from text."""
    tk = tokenize(text, path)
    cu = CompilationUnit(path, tk)
    _Parser(cu).parse_compilation_unit()
    return cu


def parse_file(path):
    with open(path, 'rb') as f:
        data = f.read()
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError as e:
        raise JavaSyntaxError(str(path), 0, 0, f'not valid UTF-8: {e}') from None
    return parse_source(text, str(path).replace('\\', '/'))


# ----------------------------------------------------------------------------------------------------------------------------------
# Spans and body helpers
# ----------------------------------------------------------------------------------------------------------------------------------

class LambdaParam:
    __slots__ = ('name', 'type', 'index')

    def __init__(self, name, type, index):
        self.name, self.type, self.index = name, type, index

    def __repr__(self):
        return f'LambdaParam({self.type} {self.name})' if self.type else f'LambdaParam({self.name})'


class Lambda:
    __slots__ = ('span', 'params', 'body', 'block', 'arrow', 'line', 'col')

    def __init__(self, span, params, body, block, arrow):
        self.span, self.params, self.body, self.block, self.arrow = span, params, body, block, arrow
        self.line, self.col = span.line, span.col

    def __repr__(self):
        return f'Lambda({self.span.text!r})'


class NewExpr:
    __slots__ = ('index', 'type', 'args', 'args_span', 'array', 'dim_exprs', 'initializer', 'anonymous', 'qualifier', 'span', 'line', 'col')

    def __init__(self, index, type):
        self.index = index
        self.type = type
        self.args = None
        self.args_span = None
        self.array = False
        self.dim_exprs = []
        self.initializer = None
        self.anonymous = None
        self.qualifier = None
        self.span = None
        self.line = type.line
        self.col = type.col

    def __repr__(self):
        return f'NewExpr({self.span.text!r})'


class LocalVar:
    __slots__ = ('kind', 'name', 'type', 'union_types', 'modifiers', 'initializer', 'index', 'scope_end', 'line', 'col')

    def __init__(self, kind, name, type, index, scope_end, line, col):
        self.kind, self.name, self.type, self.index, self.scope_end, self.line, self.col = kind, name, type, index, scope_end, line, col
        self.union_types = None
        self.modifiers = []
        self.initializer = None

    def __repr__(self):
        return f'LocalVar({self.kind} {self.type} {self.name})'


class IdentRef:
    __slots__ = ('name', 'index', 'role', 'qualifier', 'line', 'col')

    def __init__(self, name, index, role, qualifier, line, col):
        self.name, self.index, self.role, self.qualifier, self.line, self.col = name, index, role, qualifier, line, col

    def __repr__(self):
        return f'IdentRef({self.name} {self.role})'


class Assignment:
    __slots__ = ('name', 'qualifier', 'op', 'element', 'rhs', 'index', 'local', 'line', 'col')

    def __init__(self, name, qualifier, op, element, rhs, index, local, line, col):
        self.name, self.qualifier, self.op, self.element, self.rhs = name, qualifier, op, element, rhs
        self.index, self.local, self.line, self.col = index, local, line, col

    def __repr__(self):
        q = f'{self.qualifier}.' if self.qualifier else ''
        return f"Assignment({q}{self.name}{'[]' if self.element else ''} {self.op})"


class MethodCall:
    __slots__ = ('name', 'receiver', 'args', 'index', 'span', 'line', 'col')

    def __init__(self, name, receiver, args, index, span, line, col):
        self.name, self.receiver, self.args, self.index, self.span, self.line, self.col = name, receiver, args, index, span, line, col

    def __repr__(self):
        return f'MethodCall({self.span.text!r})'


class MethodRef:
    __slots__ = ('receiver', 'name', 'index', 'line', 'col')

    def __init__(self, receiver, name, index, line, col):
        self.receiver, self.name, self.index, self.line, self.col = receiver, name, index, line, col

    def __repr__(self):
        return f'MethodRef({self.receiver.text}::{self.name})'


_STATEMENT_KEYWORDS_BEFORE_PAREN = ('for', 'try', 'catch')
_LOCAL_RECORD_PREDECESSORS = frozenset(('{', '}', ';', ':', '->', 'final', 'static', 'abstract', 'strictfp'))
_CONTEXTUAL_KEYWORDS = frozenset(('yield', 'when', 'record', 'sealed', 'permits', 'non'))
_NOT_YIELD_FOLLOWERS = frozenset(('=', '.', '(', '[', '++', '--', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=', '>>>=',
                                  ';', ',', ')', ':', '->', '::'))


class Span:
    """Half-open token range [start, end) of a compilation unit, e.g. a method body '{...}' or an initializer expression."""

    __slots__ = ('cu', 'start', 'end', 'owner', '_cache')

    def __init__(self, cu, start, end, owner=None):
        self.cu = cu
        self.start = start
        self.end = end
        self.owner = owner
        self._cache = None

    def __repr__(self):
        return f'Span({self.start}, {self.end}, {self.text[:40]!r})'

    def __contains__(self, index):
        return self.start <= index < self.end

    def contains(self, other):
        return self.start <= other.start and other.end <= self.end

    @property
    def text(self):
        return self.cu.tokens.source_text(self.start, self.end)

    def texts(self):
        return self.cu.tokens.text[self.start:self.end]

    @property
    def line(self):
        return self.cu.tokens.loc(self.start)[0]

    @property
    def col(self):
        return self.cu.tokens.loc(self.start)[1]

    @property
    def end_line(self):
        return self.cu.tokens.pos_loc(self.cu.tokens.end[self.end - 1] - 1)[0] if self.end > self.start else self.line

    @property
    def end_col(self):
        return self.cu.tokens.pos_loc(self.cu.tokens.end[self.end - 1] - 1)[1] if self.end > self.start else self.col

    def to_json(self, with_text=False):
        d = {'tokens': [self.start, self.end], 'start': [self.line, self.col], 'end': [self.end_line, self.end_col]}
        if with_text:
            d['text'] = self.text
        return d

    def _memo(self, key, fn):
        if self._cache is None:
            self._cache = {}
        v = self._cache.get(key)
        if v is None:
            v = fn()
            self._cache[key] = v
        return v

    def _owner_type(self):
        o = self.owner
        while o is not None and not isinstance(o, TypeDecl):
            o = getattr(o, 'owner', None)
        return o

    # -- switch labels
    def _switch_arrows(self):
        return self._memo('arrows', self._compute_switch_arrows)

    def _compute_switch_arrows(self):
        tk = self.cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        arrows = set()
        e = self.end
        for i in range(self.start, e):
            if k[i] is not KEYWORD:
                continue
            x = t[i]
            if x == 'case':
                j = i + 1
                q = 0
                while j < e:
                    y = t[j]
                    if k[j] is OP:
                        if y in OPEN_BRACKETS:
                            j = m[j] + 1
                            continue
                        if y == '->':
                            arrows.add(j)
                            break
                        if y == '?':
                            q += 1
                        elif y == ':':
                            if q == 0:
                                break
                            q -= 1
                        elif y == ';' or y in CLOSE_BRACKETS:
                            break
                    j += 1
            elif x == 'default' and t[i + 1] == '->':
                arrows.add(i + 1)
        return arrows

    # -- lambdas
    def lambdas(self):
        return self._memo('lambdas', self._compute_lambdas)

    def _compute_lambdas(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        arrows = self._switch_arrows()
        out = []
        p = _Parser(cu)
        for i in range(self.start, self.end):
            if t[i] != '->' or i in arrows:
                continue
            j = i - 1
            if t[j] == ')':
                ps = m[j]
                p.i = ps
                params = [LambdaParam(x.name, x.type, x.index) for x in p.parse_params(allow_implicit=True)]
                if p.i != i:
                    raise tk.error(i, 'malformed lambda parameters')
            elif k[j] is IDENT:
                ps = j
                params = [LambdaParam(t[j], None, j)]
            else:
                raise tk.error(i, f"'->' after {t[j]!r} is neither a lambda nor a switch rule")
            if t[i + 1] == '{':
                be = m[i + 1] + 1
                block = True
            else:
                be = p.expr_end(i + 1)
                block = False
                if be == i + 1:
                    raise tk.error(i, 'empty lambda body')
            out.append(Lambda(Span(cu, ps, be, self.owner), params, Span(cu, i + 1, be, self.owner), block, i))
        return out

    # -- new expressions and anonymous classes
    def new_expressions(self):
        return self._memo('new', self._compute_new)

    def _compute_new(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        out = []
        p = _Parser(cu)
        for i in range(self.start, self.end):
            if t[i] != 'new' or k[i] is not KEYWORD or t[i - 1] == '::':
                continue
            p.i = i + 1
            if t[p.i] == '<':
                p.parse_type_args()
            ty = p.parse_type(allow_dims=False)
            ne = NewExpr(i, ty)
            j = p.i
            if t[j] == '(':
                ne.args = p.split_args(j, self.owner)
                ne.args_span = Span(cu, j, m[j] + 1, self.owner)
                j = m[j] + 1
                if t[j] == '{':
                    anon = cu._anon.get(j)
                    if anon is None:
                        anon = TypeDecl('anonymous', '', cu, self._owner_type(), ty.line, ty.col, i, i)
                        anon.extends = [ty]
                        sub = _Parser(cu, j)
                        sub.parse_body(anon)
                        cu._anon[j] = anon
                    ne.anonymous = anon
                    j = m[j] + 1
            elif t[j] == '[' or t[j] == '@':
                ne.array = True
                dims = 0
                while True:
                    while t[j] == '@':
                        p.i = j
                        p.parse_annotation()
                        j = p.i
                    if t[j] != '[':
                        break
                    if t[j + 1] != ']':
                        ne.dim_exprs.append(Span(cu, j + 1, m[j], self.owner))
                    dims += 1
                    j = m[j] + 1
                ne.type = ty.with_dims(dims)
                if t[j] == '{':
                    ne.initializer = Span(cu, j, m[j] + 1, self.owner)
                    j = m[j] + 1
                if not ne.dim_exprs and ne.initializer is None:
                    raise tk.error(i, 'array creation without dimensions or initializer')
            else:
                raise tk.error(j, f"expected '(' or '[' after 'new {ty}', found {t[j]!r}")
            if t[i - 1] == '.':
                q = _primary_start(tk, i - 2)
                ne.qualifier = Span(cu, q, i - 1, self.owner)
                ne.span = Span(cu, q, j, self.owner)
            else:
                ne.span = Span(cu, i, j, self.owner)
            out.append(ne)
        return out

    def anonymous_classes(self):
        return [n for n in self.new_expressions() if n.anonymous is not None]

    # -- local types
    def local_types(self):
        return self._memo('local_types', self._compute_local_types)

    def _compute_local_types(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m, parent = tk.text, tk.kind, tk.match, tk.parent
        # member types of anonymous classes and of local classes already found are members, not local types
        member_braces = {n.anonymous.body.start for n in self.anonymous_classes()}
        out = []
        i = self.start
        e = self.end
        while i < e:
            x = t[i]
            is_decl = False
            if k[i] is KEYWORD and x in ('class', 'interface', 'enum') and t[i - 1] != '.':
                is_decl = True
            elif (x == 'record' and k[i] is IDENT and k[i + 1] is IDENT and t[i + 2] in ('(', '<')
                  and t[i - 1] in _LOCAL_RECORD_PREDECESSORS):
                is_decl = True
            elif x == '@' and t[i + 1] == 'interface':
                is_decl = True
            if not is_decl or parent[i] in member_braces:
                i += 1
                continue
            s = i
            while (k[s - 1] is KEYWORD and t[s - 1] in ('final', 'abstract', 'static', 'strictfp')) or (k[s - 1] is IDENT and t[s - 1] == 'sealed'):
                s -= 1
            td = cu._local.get(s)
            if td is None:
                sub = _Parser(cu, s)
                mods, anns = sub.parse_modifiers()
                td = sub.parse_type_decl(mods, anns, self._owner_type(), tk.docs.get(s), s)
                td.local = True
                cu._local[s] = td
            out.append(td)
            stack = [td]
            while stack:
                n = stack.pop()
                member_braces.add(n.body.start)
                stack.extend(n.types)
            i = td.body.start + 1  # continue inside: method bodies of the local class may declare local classes themselves
        return out

    def _class_body_braces(self):
        return self._memo('class_braces', lambda: {n.anonymous.body.start for n in self.anonymous_classes()}
                          | {td.body.start for td in self.local_types()} | self._nested_local_type_braces())

    def _nested_local_type_braces(self):
        out = set()

        def walk(td):
            for n in td.types:
                out.add(n.body.start)
                walk(n)
            for c in td.enum_constants:
                if c.body is not None:
                    out.add(c.body.body.start)
        for td in self.local_types():
            walk(td)
        for n in self.anonymous_classes():
            walk(n.anonymous)
        return out

    # -- local variables
    def local_variables(self):
        return self._memo('locals', self._compute_locals)

    def _compute_locals(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m, parent = tk.text, tk.kind, tk.match, tk.parent
        class_braces = self._class_body_braces()
        arrows = self._switch_arrows()
        s0, e = self.start, self.end
        p = _Parser(cu)
        out = []
        self._cache['record_patterns'] = set()
        # declarations at statement starts, for/try/catch headers
        for i in range(s0, e):
            if i == s0:
                continue
            prev = t[i - 1]
            kind = None
            par = parent[i]
            if prev in ('{', '}', ';') and k[i - 1] is OP:
                if par in class_braces:
                    continue
                if par >= 0 and t[par] != '{':
                    if t[par] == '(' and t[par - 1] in ('for', 'try') and k[par - 1] is KEYWORD:
                        if prev != ';':
                            continue
                        kind = 'resource' if t[par - 1] == 'try' else 'for'
                    else:
                        continue
                else:
                    kind = 'local'
            elif prev == '(' and k[i - 2] is KEYWORD and t[i - 2] in _STATEMENT_KEYWORDS_BEFORE_PAREN:
                kind = {'for': 'for', 'try': 'resource', 'catch': 'catch'}[t[i - 2]]
            elif prev == '->' and (i - 1) in arrows:
                kind = 'local'
            elif prev == ':' and k[i - 1] is OP and (par < 0 or t[par] == '{') and par not in class_braces:
                kind = 'local'
            else:
                continue
            x = t[i]
            if not (k[i] is IDENT or (k[i] is KEYWORD and (x in PRIMITIVES or x == 'final')) or x == '@'):
                continue
            decl = self._try_local_decl(p, i, kind)
            if decl:
                out.extend(decl)
        # pattern variables
        for i in range(s0, e):
            x = t[i]
            if k[i] is KEYWORD and x == 'instanceof':
                j = i + 1
                while t[j] == 'final':
                    j += 1
                self._pattern_at(p, j, out, self._scope_end_block(i))
            elif k[i] is KEYWORD and x == 'case':
                j = i + 1
                while True:
                    j2 = self._pattern_at(p, j, out, self._scope_end_block(i))
                    if j2 is None:
                        break
                    if t[j2] == ',':
                        j = j2 + 1
                        continue
                    break
        # lambda parameters
        for lam in self.lambdas():
            for prm in lam.params:
                line, col = tk.loc(prm.index)
                out.append(LocalVar('lambda_param', prm.name, prm.type, prm.index, lam.span.end, line, col))
        # parameters of the owning method and of methods of anonymous/local classes declared in the span
        methods = []
        if isinstance(self.owner, MethodDecl) and self.owner.body is not None and self.owner.body.start == s0:
            methods.append((self.owner, e))
        for td in self._all_inner_classes():
            methods.extend((md, md.body.end) for md in td.methods if md.body is not None)
        for md, scope_end in methods:
            params = md.params
            if md.kind == 'compact_constructor':
                params = md.owner.record_components
            for prm in params:
                if prm.name == 'this' or prm.name.endswith('.this'):
                    continue
                out.append(LocalVar('param', prm.name, prm.type, prm.index, scope_end, prm.line, prm.col))
        out.sort(key=lambda v: v.index)
        return out

    def _scope_end_block(self, i):
        tk = self.cu.tokens
        par = tk.parent[i]
        while par >= 0 and tk.text[par] != '{':
            par = tk.parent[par]
        if par < 0 or par < self.start:
            return self.end
        return tk.match[par]

    def _pattern_at(self, p, j, out, scope_end):
        """Parses a type pattern or record pattern at j; appends pattern variables; returns the index after it or None."""
        tk = self.cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        if not (k[j] is IDENT or (k[j] is KEYWORD and t[j] in PRIMITIVES) or t[j] == '@'):
            return None
        p.i = j
        try:
            while t[p.i] == 'final' or t[p.i] == '@':
                if t[p.i] == 'final':
                    p.i += 1
                else:
                    p.parse_annotation()
            ty = p.parse_type()
        except JavaSyntaxError:
            return None
        n = p.i
        if k[n] is IDENT and t[n] != 'when' or (t[n] == 'when' and t[n + 1] in ('->', ':', 'when', ',', ')')):
            line, col = tk.loc(n)
            out.append(LocalVar('pattern', t[n], ty, n, scope_end, line, col))
            return n + 1
        if t[n] == '(' and not ty.is_primitive:
            self._cache['record_patterns'].add(n - 1)
            close = m[n]
            q = n + 1
            while q < close:
                if t[q] == '_' and (t[q + 1] == ',' or q + 1 == close):
                    q += 1
                else:
                    r = self._pattern_at(p, q, out, scope_end)
                    if r is None:
                        return None
                    q = r
                if t[q] == ',':
                    q += 1
                elif q != close:
                    return None
            return close + 1
        return None

    def _try_local_decl(self, p, i, kind):
        tk = self.cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        p.i = i
        mods = []
        try:
            while True:
                if t[p.i] == 'final' and k[p.i] is KEYWORD:
                    mods.append('final')
                    p.i += 1
                elif t[p.i] == '@' and t[p.i + 1] != 'interface':
                    p.parse_annotation()
                else:
                    break
            if t[p.i] == 'yield' or t[p.i] == 'record' and k[p.i + 1] is IDENT and t[p.i + 2] in ('(', '<'):
                return None
            if not (k[p.i] is IDENT or (k[p.i] is KEYWORD and t[p.i] in PRIMITIVES)) or t[p.i] == 'void':
                return None
            ty = p.parse_type()
            union = None
            if kind == 'catch':
                union = [ty]
                while t[p.i] == '|':
                    p.i += 1
                    union.append(p.parse_type())
        except JavaSyntaxError:
            return None
        n = p.i
        if k[n] is not IDENT or t[n + 1] not in ('=', ';', ',', ':', '[', ')'):
            return None
        if t[n + 1] == ':' and kind != 'for':
            return None
        if t[n + 1] == ')' and kind not in ('catch', 'resource'):
            return None
        scope_end = self._decl_scope_end(i, kind)
        out = []
        if kind == 'for' and t[n + 1] == ':':
            kind = 'foreach'
        while True:
            name_i = p.i
            if k[name_i] is not IDENT:
                raise tk.error(name_i, 'expected variable name')
            p.i += 1
            dims = p.parse_dims()
            line, col = tk.loc(name_i)
            v = LocalVar(kind, t[name_i], ty.with_dims(dims) if dims else ty, name_i, scope_end, line, col)
            v.modifiers = mods
            v.union_types = union if union and len(union) > 1 else None
            out.append(v)
            if t[p.i] == '=':
                b = p.i + 1
                e2 = p.expr_end(b)
                v.initializer = Span(self.cu, b, e2, self.owner)
                p.i = e2
            if t[p.i] == ',' and kind in ('local', 'for'):
                p.i += 1
                continue
            break
        return out

    def _decl_scope_end(self, i, kind):
        tk = self.cu.tokens
        t, m, parent = tk.text, tk.match, tk.parent
        par = parent[i]
        if kind in ('for', 'resource', 'catch', 'foreach'):
            if par < 0:
                return self.end
            j = m[par] + 1
            if kind == 'resource' or kind == 'catch':
                return m[j] + 1 if t[j] == '{' else self.end
            if t[j] == '{':
                return m[j] + 1
            sub = _Parser(self.cu)
            # single statement body: scan to its ';' (blocks inside are skipped by expr_end)
            try:
                end = sub.expr_end(j)
            except JavaSyntaxError:
                return self.end
            return min(end + 1, self.end)
        return self._scope_end_block(i) if par >= 0 else self.end

    def locals_visible_at(self, index):
        vis = {}
        for v in self.local_variables():
            if v.index < index < v.scope_end or v.index == index:
                vis[v.name] = v
        return vis

    # -- identifiers
    def identifier_refs(self):
        return self._memo('idents', self._compute_idents)

    def _compute_idents(self):
        tk = self.cu.tokens
        t, k = tk.text, tk.kind
        decl_names = {v.index for v in self.local_variables()}
        type_positions = set(self._cache['record_patterns'])
        generic_calls = {}  # identifier after '.<...>' -> index of the '.'
        for i in range(self.start, self.end):
            if t[i] == '.' and t[i + 1] == '<':
                sub = _Parser(self.cu, i + 1)
                for a in sub.parse_type_args():
                    self._mark_type_positions(a, type_positions)
                generic_calls[sub.i] = i
        for v in self.local_variables():
            if v.type is not None:
                for ty in (v.union_types or [v.type]):
                    self._mark_type_positions(ty, type_positions)
        for ne in self.new_expressions():
            self._mark_type_positions(ne.type, type_positions)
        member_decl = set()
        for td in self.local_types() + [n.anonymous for n in self.anonymous_classes()]:
            self._mark_member_decls(td, member_decl, type_positions)
        ctx_keywords = self._contextual_keyword_positions()
        out = []
        for i in range(self.start, self.end):
            if k[i] is not IDENT:
                continue
            prev = t[i - 1]
            name = t[i]
            qualifier = None
            if prev == '@' or (prev == '.' and k[i - 2] is IDENT and t[i - 3] == '@'):
                continue
            if i in ctx_keywords:
                continue
            if i in decl_names or i in member_decl:
                role = 'declaration'
            elif i in type_positions:
                role = 'type'
            elif prev == '::':
                role = 'method_ref'
            elif prev == '.' or i in generic_calls:
                role = 'call' if t[i + 1] == '(' else 'member'
                dot = generic_calls.get(i, i - 1)
                q = t[dot - 1]
                if q in ('this', 'super') and t[dot - 2] != '.':
                    qualifier = q
                else:
                    qs = _primary_start(tk, dot - 1)
                    qualifier = tk.source_text(qs, dot)
            elif t[i + 1] == '(':
                role = 'call'
            elif t[i + 1] == ':' and prev in ('{', '}', ';', ':') and t[i - 1] != '?':
                role = 'label' if not self._in_case_label(i) else 'expr'
            elif prev in ('break', 'continue'):
                role = 'label'
            else:
                role = 'expr'
            line, col = tk.loc(i)
            out.append(IdentRef(name, i, role, qualifier, line, col))
        return out

    def _contextual_keyword_positions(self):
        """Token indices where 'yield', 'when', 'record', 'sealed', 'permits' or 'non' act as keywords, not identifiers."""
        tk = self.cu.tokens
        t, k = tk.text, tk.kind
        out = set()
        for v in self.local_variables():
            if v.kind == 'pattern' and t[v.index + 1] == 'when':
                out.add(v.index + 1)
        for i in range(self.start, self.end):
            x = t[i]
            if k[i] is not IDENT or x not in _CONTEXTUAL_KEYWORDS:
                continue
            if x == 'yield' and t[i - 1] in ('{', '}', ';', '->', ':') and t[i + 1] not in _NOT_YIELD_FOLLOWERS:
                out.add(i)
            elif x == 'when' and t[i - 1] == ')' and self._in_case_label(i):
                out.add(i)
        for td in self._all_inner_classes():
            if td.local:
                for i in range(td.start, td.body.start):
                    if k[i] is IDENT and t[i] in _CONTEXTUAL_KEYWORDS and i != td.index:
                        out.add(i)
        return out

    def enclosing_class(self, index):
        """Innermost anonymous or local class declared in this span whose body contains token index, or None."""
        best = None
        for td in self._all_inner_classes():
            b = td.body
            if b.start < index < b.end - 1 and (best is None or b.start > best.body.start):
                best = td
        return best

    def _all_inner_classes(self):
        def compute():
            out = []

            def walk(td):
                out.append(td)
                for n in td.types:
                    walk(n)
                for c in td.enum_constants:
                    if c.body is not None:
                        walk(c.body)
            for td in self.local_types():
                walk(td)
            for ne in self.anonymous_classes():
                walk(ne.anonymous)
            return out
        return self._memo('inner_classes', compute)

    def _in_case_label(self, i):
        tk = self.cu.tokens
        j = i - 1
        while j > self.start and tk.text[j] not in (';', '{', '}'):
            if tk.text[j] == 'case':
                return True
            j -= 1
        return False

    def _mark_type_positions(self, ty, positions):
        tk = self.cu.tokens
        if ty is None or ty.wildcard:
            if ty is not None and ty.bound is not None:
                self._mark_type_positions(ty.bound, positions)
            return
        i = ty.index
        if i < 0:
            return
        t = tk.text
        for n, args in ty.segments:
            while t[i] == '@' or t[i] == '.':
                i += 1
                if t[i - 1] == '@':
                    sub = _Parser(self.cu, i - 1)
                    sub.parse_annotation()
                    i = sub.i
            if t[i] == n:
                positions.add(i)
            i += 1
            if args is not None:
                for a in args:
                    self._mark_type_positions(a, positions)
                if t[i] == '<':
                    sub = _Parser(self.cu, i)
                    sub.parse_type_args()
                    i = sub.i

    def _mark_member_decls(self, td, member_decl, type_positions):
        for f in td.fields:
            member_decl.add(f.index)
            self._mark_type_positions(f.type, type_positions)
        for md in td.methods:
            member_decl.add(md.index)
            if md.return_type is not None:
                self._mark_type_positions(md.return_type, type_positions)
            for prm in md.params:
                member_decl.add(prm.index)
                self._mark_type_positions(prm.type, type_positions)
        for c in td.enum_constants:
            member_decl.add(c.index)
        for n in td.types:
            self._mark_member_decls(n, member_decl, type_positions)
        if td.local:
            member_decl.add(td.index)
        for ty in td.extends + td.implements + td.permits:
            self._mark_type_positions(ty, type_positions)
        for prm in td.record_components:
            member_decl.add(prm.index)
            self._mark_type_positions(prm.type, type_positions)
        for tp in td.type_params:
            for b in tp.bounds:
                self._mark_type_positions(b, type_positions)

    # -- assignments
    def assignments(self):
        return self._memo('assign', self._compute_assign)

    def field_assignments(self):
        return [a for a in self.assignments() if not a.local]

    def _compute_assign(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        decls = {v.index for v in self.local_variables()}
        member_decl = set()
        types = set()
        for td in self.local_types() + [n.anonymous for n in self.anonymous_classes()]:
            self._mark_member_decls(td, member_decl, types)
        ann_ranges = self._annotation_arg_ranges()
        p = _Parser(cu)
        out = []
        for i in range(self.start, self.end):
            x = t[i]
            if k[i] is not OP:
                continue
            if x in ASSIGN_OPS:
                lhs_end = i
                op = x
            elif x == '++' or x == '--':
                op = x
                if k[i - 1] is IDENT or t[i - 1] == ']':
                    lhs_end = i  # postfix
                elif k[i + 1] is IDENT or t[i + 1] == 'this':
                    lhs_end = None  # prefix
                else:
                    continue
            else:
                continue
            if any(a < i < b for a, b in ann_ranges):
                continue
            if lhs_end is None:
                # prefix ++x / ++this.x / ++x[i]
                j = i + 1
                qual = None
                if t[j] == 'this' and t[j + 1] == '.' and k[j + 2] is IDENT:
                    qual = 'this'
                    j += 2
                if k[j] is not IDENT or (qual is None and t[j + 1] == '.'):
                    continue
                element = t[j + 1] == '['
                name_i = j
                rhs = None
            else:
                j = lhs_end - 1
                element = False
                while t[j] == ']':
                    j = m[j] - 1
                    element = True
                if k[j] is not IDENT:
                    continue
                name_i = j
                qual = None
                if t[j - 1] == '.':
                    if t[j - 2] == 'this' and t[j - 3] != '.':
                        qual = 'this'
                    else:
                        qs = _primary_start(tk, j - 2)
                        qual = tk.source_text(qs, j - 1)
                elif not element and (k[j - 1] is IDENT or t[j - 1] in ('>', ']') or (k[j - 1] is KEYWORD and t[j - 1] in PRIMITIVES)):
                    continue  # a declarator
                if name_i in decls or name_i in member_decl:
                    continue
                if op in ('++', '--'):
                    rhs = None
                else:
                    e2 = p.expr_end(i + 1)
                    rhs = Span(cu, i + 1, e2, self.owner)
            name = t[name_i]
            local = False
            if qual is None:
                local = name in self.locals_visible_at(name_i)
            line, col = tk.loc(name_i)
            out.append(Assignment(name, qual, op, element, rhs, name_i, local, line, col))
        return out

    def _annotation_arg_ranges(self):
        tk = self.cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        out = []
        for i in range(self.start, self.end):
            if t[i] == '@' and k[i + 1] is IDENT:
                j = i + 2
                while t[j] == '.' and k[j + 1] is IDENT:
                    j += 2
                if t[j] == '(':
                    out.append((j, m[j]))
        return out

    # -- method calls and references
    def method_calls(self):
        return self._memo('calls', self._compute_calls)

    def _compute_calls(self):
        cu = self.cu
        tk = cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        class_braces = self._class_body_braces()
        self.local_variables()
        new_type_idx = set(self._cache['record_patterns'])
        for ne in self.new_expressions():
            self._mark_type_positions(ne.type, new_type_idx)
        ann_ranges = self._annotation_arg_ranges()
        p = _Parser(cu)
        out = []
        for i in range(self.start, self.end):
            if t[i + 1] != '(':
                continue
            x = t[i]
            kind = k[i]
            if kind is KEYWORD and x in ('this', 'super'):
                name = x
            elif kind is IDENT:
                if i in new_type_idx:
                    continue
                prev = t[i - 1]
                pk = k[i - 1]
                if prev == '@' or (prev == '.' and t[i - 3] == '@'):
                    continue
                if pk is IDENT or prev == ']' or (pk is KEYWORD and (prev in PRIMITIVES or prev in MODIFIER_KEYWORDS)):
                    continue  # method or constructor declaration
                if prev == '>':
                    lt = _matching_lt(tk, i - 1)
                    if (lt is None or t[lt - 1] != '.') and tk.parent[i] in class_braces:
                        continue  # generic method declaration in a local or anonymous class body
                if tk.parent[i] in class_braces and prev in ('{', '}', ';'):
                    continue  # constructor of a local class
                name = x
            else:
                continue
            if any(a < i < b for a, b in ann_ranges):
                continue
            # receiver
            j = i - 1
            if t[j] == '>':
                lt = _matching_lt(tk, j)
                if lt is not None and t[lt - 1] == '.':
                    j = lt - 1  # explicit type arguments: recv.<T>name(...)
            receiver = None
            start = i
            if t[j] == '.':
                rs = _primary_start(tk, j - 1)
                receiver = Span(cu, rs, j, self.owner)
                start = rs
            args = p.split_args(i + 1, self.owner)
            line, col = tk.loc(i)
            out.append(MethodCall(name, receiver, args, i, Span(cu, start, m[i + 1] + 1, self.owner), line, col))
        return out

    def method_refs(self):
        tk = self.cu.tokens
        t, k = tk.text, tk.kind
        out = []
        for i in range(self.start, self.end):
            if t[i] != '::':
                continue
            j = i + 1
            if t[j] == '<':
                sub = _Parser(self.cu, j)
                sub.parse_type_args()
                j = sub.i
            if not (k[j] is IDENT or t[j] == 'new'):
                raise tk.error(j, "expected method name after '::'")
            rs = _primary_start(tk, i - 1)
            line, col = tk.loc(rs)
            out.append(MethodRef(Span(self.cu, rs, i, self.owner), t[j], j, line, col))
        return out


def _matching_lt(tk, gt):
    """For a '>' closing type arguments, the index of the matching '<' (None when unbalanced)."""
    t = tk.text
    depth = 0
    j = gt
    while j >= 0:
        x = t[j]
        if x == '>':
            depth += 1
        elif x == '<':
            depth -= 1
            if depth == 0:
                return j
        elif x in (';', '{', '}', '(', ')', '=', '&&', '||'):
            return None
        j -= 1
    return None


def _primary_start(tk, j):
    """j = last token of a receiver expression (the token before '.' or '::'); returns the first token index of that expression."""
    t, k, m = tk.text, tk.kind, tk.match
    while True:
        x = t[j]
        kind = k[j]
        if kind is OP and (x == ')' or x == ']'):
            o = m[j]
            pk = k[o - 1]
            px = t[o - 1]
            if x == ']' or pk is IDENT or (pk is KEYWORD and px in ('this', 'super')) or px in (']', ')', '>'):
                if x == ')' and not (pk is IDENT or (pk is KEYWORD and px in ('this', 'super')) or px == '>'):
                    return o  # parenthesized expression
                j = o - 1
                if t[j] == '>' and x == ')':
                    lt = _matching_lt(tk, j)
                    if lt is not None and t[lt - 1] == '.':
                        j = lt - 1  # at '.', handled below
                        j -= 1
                        continue
                continue
            return o
        if kind is OP and x == '>':
            lt = _matching_lt(tk, j)
            if lt is None:
                return j + 1
            j = lt - 1
            continue
        if kind is IDENT or kind in LITERAL_KINDS or (kind is KEYWORD and x in ('this', 'super', 'class', 'new', 'true', 'false', 'null')
                                                    or kind is KEYWORD and x in PRIMITIVES):
            if kind is IDENT and t[j - 1] == 'new' and k[j - 1] is KEYWORD:
                return j - 1
            if t[j - 1] == '.' or t[j - 1] == '::':
                j -= 2
                continue
            if kind is IDENT and t[j - 1] == '@':
                return j - 1
            return j
        return j + 1


# ----------------------------------------------------------------------------------------------------------------------------------
# Type resolution
# ----------------------------------------------------------------------------------------------------------------------------------

def _names(s):
    return frozenset(s.split())


JAVA_LANG = _names('''
AbstractMethodError Appendable ArithmeticException ArrayIndexOutOfBoundsException ArrayStoreException AssertionError AutoCloseable
Boolean BootstrapMethodError Byte CharSequence Character Class ClassCastException ClassCircularityError ClassFormatError ClassLoader
ClassNotFoundException ClassValue CloneNotSupportedException Cloneable Comparable Deprecated Double Enum EnumConstantNotPresentException
Error Exception ExceptionInInitializerError Float FunctionalInterface IllegalAccessError IllegalAccessException IllegalArgumentException
IllegalCallerException IllegalMonitorStateException IllegalStateException IllegalThreadStateException IncompatibleClassChangeError
IndexOutOfBoundsException InheritableThreadLocal InstantiationError InstantiationException Integer InternalError InterruptedException
Iterable LayerInstantiationException LinkageError Long MatchException Math Module ModuleLayer NegativeArraySizeException
NoClassDefFoundError NoSuchFieldError NoSuchFieldException NoSuchMethodError NoSuchMethodException NullPointerException Number
NumberFormatException Object OutOfMemoryError Override Package Process ProcessBuilder ProcessHandle Readable Record
ReflectiveOperationException Runnable Runtime RuntimeException SafeVarargs ScopedValue SecurityException SecurityManager Short
StackOverflowError StackTraceElement StackWalker StrictMath String StringBuffer StringBuilder StringIndexOutOfBoundsException
SuppressWarnings System Thread ThreadDeath ThreadGroup ThreadLocal Throwable TypeNotPresentException UnknownError UnsatisfiedLinkError
UnsupportedClassVersionError UnsupportedOperationException VerifyError VirtualMachineError Void WrongThreadException
''')

KNOWN_EXTERNAL_PACKAGES = {
    'java.lang': JAVA_LANG,
    'java.util': _names('''
AbstractCollection AbstractList AbstractMap AbstractQueue AbstractSequentialList AbstractSet ArrayDeque ArrayList Arrays Base64 BitSet
Calendar Collection Collections Comparator ConcurrentModificationException Currency Date Deque Dictionary DoubleSummaryStatistics
EnumMap EnumSet Enumeration EventListener EventObject Formattable Formatter GregorianCalendar HashMap HashSet Hashtable HexFormat
IdentityHashMap IllegalFormatException InputMismatchException IntSummaryStatistics Iterator LinkedHashMap LinkedHashSet LinkedList List
ListIterator ListResourceBundle Locale LongSummaryStatistics Map MissingResourceException NavigableMap NavigableSet NoSuchElementException
Objects Observable Observer Optional OptionalDouble OptionalInt OptionalLong PrimitiveIterator PriorityQueue Properties
PropertyResourceBundle Queue Random RandomAccess ResourceBundle Scanner SequencedCollection SequencedMap SequencedSet ServiceLoader Set
SimpleTimeZone SortedMap SortedSet Spliterator Spliterators SplittableRandom Stack StringJoiner StringTokenizer TimeZone Timer TimerTask
TreeMap TreeSet UUID UnknownFormatConversionException Vector WeakHashMap'''),
    'java.util.concurrent': _names('''
AbstractExecutorService ArrayBlockingQueue BlockingDeque BlockingQueue BrokenBarrierException Callable CancellationException
CompletableFuture CompletionException CompletionService CompletionStage ConcurrentHashMap ConcurrentLinkedDeque ConcurrentLinkedQueue
ConcurrentMap ConcurrentNavigableMap ConcurrentSkipListMap ConcurrentSkipListSet CopyOnWriteArrayList CopyOnWriteArraySet CountDownLatch
CyclicBarrier DelayQueue Delayed Exchanger ExecutionException Executor ExecutorCompletionService ExecutorService Executors Flow
ForkJoinPool ForkJoinTask ForkJoinWorkerThread Future FutureTask LinkedBlockingDeque LinkedBlockingQueue LinkedTransferQueue Phaser
PriorityBlockingQueue RecursiveAction RecursiveTask RejectedExecutionException RejectedExecutionHandler RunnableFuture
RunnableScheduledFuture ScheduledExecutorService ScheduledFuture ScheduledThreadPoolExecutor Semaphore StructuredTaskScope
SynchronousQueue ThreadFactory ThreadLocalRandom ThreadPoolExecutor TimeUnit TimeoutException TransferQueue'''),
    'java.util.concurrent.atomic': _names('''
AtomicBoolean AtomicInteger AtomicIntegerArray AtomicIntegerFieldUpdater AtomicLong AtomicLongArray AtomicLongFieldUpdater
AtomicMarkableReference AtomicReference AtomicReferenceArray AtomicReferenceFieldUpdater AtomicStampedReference DoubleAccumulator
DoubleAdder LongAccumulator LongAdder'''),
    'java.util.concurrent.locks': _names('''
AbstractOwnableSynchronizer AbstractQueuedLongSynchronizer AbstractQueuedSynchronizer Condition Lock LockSupport ReadWriteLock
ReentrantLock ReentrantReadWriteLock StampedLock'''),
    'java.util.function': _names('''
BiConsumer BiFunction BiPredicate BinaryOperator BooleanSupplier Consumer DoubleBinaryOperator DoubleConsumer DoubleFunction
DoublePredicate DoubleSupplier DoubleToIntFunction DoubleToLongFunction DoubleUnaryOperator Function IntBinaryOperator IntConsumer
IntFunction IntPredicate IntSupplier IntToDoubleFunction IntToLongFunction IntUnaryOperator LongBinaryOperator LongConsumer LongFunction
LongPredicate LongSupplier LongToDoubleFunction LongToIntFunction LongUnaryOperator ObjDoubleConsumer ObjIntConsumer ObjLongConsumer
Predicate Supplier ToDoubleBiFunction ToDoubleFunction ToIntBiFunction ToIntFunction ToLongBiFunction ToLongFunction UnaryOperator'''),
    'java.util.stream': _names('BaseStream Collector Collectors DoubleStream IntStream LongStream Stream StreamSupport Gatherer Gatherers'),
    'java.io': _names('''
BufferedInputStream BufferedOutputStream BufferedReader BufferedWriter ByteArrayInputStream ByteArrayOutputStream CharArrayReader
CharArrayWriter Closeable Console DataInput DataInputStream DataOutput DataOutputStream EOFException Externalizable File FileDescriptor
FileFilter FileInputStream FileNotFoundException FileOutputStream FileReader FileWriter FilenameFilter FilterInputStream
FilterOutputStream Flushable IOException InputStream InputStreamReader InterruptedIOException InvalidObjectException LineNumberReader
NotSerializableException ObjectInputStream ObjectOutputStream ObjectStreamException OutputStream OutputStreamWriter PipedInputStream
PipedOutputStream PrintStream PrintWriter PushbackInputStream RandomAccessFile Reader SequenceInputStream Serial Serializable
StringReader StringWriter UncheckedIOException UnsupportedEncodingException Writer'''),
    'java.sql': _names('''
Array BatchUpdateException Blob CallableStatement Clob Connection DataTruncation DatabaseMetaData Date Driver DriverManager JDBCType
NClob ParameterMetaData PreparedStatement Ref ResultSet ResultSetMetaData RowId SQLClientInfoException SQLDataException SQLException
SQLFeatureNotSupportedException SQLIntegrityConstraintViolationException SQLSyntaxErrorException SQLTimeoutException SQLType SQLWarning
SQLXML Savepoint Statement Struct Time Timestamp Types'''),
    'javax.xml.bind.annotation': _names('''
DomHandler W3CDomHandler XmlAccessOrder XmlAccessType XmlAccessorOrder XmlAccessorType XmlAnyAttribute XmlAnyElement XmlAttachmentRef
XmlAttribute XmlElement XmlElementDecl XmlElementRef XmlElementRefs XmlElementWrapper XmlElements XmlEnum XmlEnumValue XmlID XmlIDREF
XmlInlineBinaryData XmlList XmlMimeType XmlMixed XmlNs XmlNsForm XmlRegistry XmlRootElement XmlSchema XmlSchemaType XmlSchemaTypes
XmlSeeAlso XmlTransient XmlType XmlValue'''),
}


_RESOLVED_KINDS = frozenset(('project', 'external', 'external_guess', 'primitive'))


def _context_parts(context):
    """(cu, innermost TypeDecl or None, type-variable names, local TypeDecls in scope) of a resolution context."""
    tvars = set()
    local_types = []
    o = context
    if isinstance(o, Span):
        local_types.extend(o.local_types())
        o = o.owner if o.owner is not None else o.cu
    if isinstance(o, MethodDecl):
        tvars.update(p.name for p in o.type_params)
        if o.body is not None and not local_types:
            local_types.extend(o.body.local_types())
        o = o.owner
    elif isinstance(o, (FieldDecl, Initializer, EnumConstant)):
        o = o.owner
    if isinstance(o, TypeDecl):
        return o.cu, o, tvars, local_types
    if isinstance(o, CompilationUnit):
        return o, None, tvars, local_types
    raise TypeError(f'unsupported resolution context {context!r}')


class ProjectIndex:
    """Index of the parsed units of several source roots with a simple-name -> FQN resolver (see module docstring)."""

    def __init__(self):
        self.units = []
        self.types = {}
        self.packages = {}
        self.duplicates = []
        self.learned = {}
        self._supers = {}
        self._in_progress = set()
        self.roots = []

    @classmethod
    def from_roots(cls, roots):
        idx = cls()
        for root in roots:
            idx.roots.append(str(root).replace('\\', '/'))
            for path in find_java_files(root):
                cu = parse_file(path)
                cu.root = str(root).replace('\\', '/')
                cu.relpath = os.path.relpath(path, root).replace('\\', '/')
                idx.add_unit(cu)
        return idx

    def add_unit(self, cu):
        self.units.append(cu)
        pkg = cu.package or ''
        names = self.packages.setdefault(pkg, {})
        for td in cu.all_types():
            fqn = td.fqn
            if fqn in self.types:
                self.duplicates.append((fqn, self.types[fqn].cu.path, cu.path))
                continue
            self.types[fqn] = td
            if td.outer is None:
                names[td.name] = fqn
        for imp in cu.imports:
            if not imp.wildcard and not imp.static:
                pkg_name, _, simple = imp.name.rpartition('.')
                self.learned.setdefault(pkg_name, set()).add(simple)
        self._supers.clear()

    def lookup(self, fqn):
        return self.types.get(fqn)

    # -- supertypes and member types
    def supertypes(self, td):
        key = id(td)
        cached = self._supers.get(key)
        if cached is not None:
            return cached
        if key in self._in_progress:
            return []
        self._in_progress.add(key)
        try:
            out = []
            refs = list(td.extends) + list(td.implements)
            ctx = td.outer if td.anonymous and td.outer is not None else td
            for ref in refs:
                kind, value = self.resolve_kind(ref.name, ctx)
                if kind == 'project':
                    out.append(self.types[value])
                elif kind == 'local':
                    out.append(value)
            if td.anonymous and td.outer is not None and td.outer.kind == 'enum' and not refs:
                out.append(td.outer)
        finally:
            self._in_progress.discard(key)
        self._supers[key] = out
        return out

    def member_type(self, td, name, _seen=None):
        for n in td.types:
            if n.name == name:
                return n
        seen = _seen if _seen is not None else set()
        seen.add(id(td))
        for sup in self.supertypes(td):
            if id(sup) in seen:
                continue
            r = self.member_type(sup, name, seen)
            if r is not None:
                return r
        return None

    # -- resolution
    def resolve(self, name, context):
        kind, value = self.resolve_kind(name, context)
        return value if kind in _RESOLVED_KINDS else None

    def resolve_kind(self, name, context):
        if name in PRIMITIVES:
            return 'primitive', name
        cu, td, tvars, local_types = _context_parts(context)
        parts = name.split('.')
        kind, value = self._resolve_simple(parts[0], cu, td, tvars, local_types)
        rest = parts[1:]
        if kind in ('unresolved', 'external_guess') and rest:
            for n in range(1, len(parts)):
                cand = '.'.join(parts[:n + 1])
                if cand in self.types:
                    kind, value, rest = 'project', cand, parts[n + 1:]
                    break
            else:
                if parts[0][:1].islower():
                    return 'external', name  # package-qualified name outside the project
                if kind == 'unresolved':
                    return kind, value
        if kind == 'typevar':
            return (kind, value) if not rest else ('unresolved', None)
        if kind not in ('project', 'local', 'external', 'external_guess'):
            return kind, value
        for seg in rest:
            if kind == 'project' or kind == 'local':
                owner = value if kind == 'local' else self.types[value]
                mt = self.member_type(owner, seg)
                if mt is None:
                    return 'unresolved', None
                kind, value = self._decl_result(mt)
            else:
                value = f'{value}.{seg}'
        return kind, value

    def _resolve_simple(self, n, cu, td, tvars, local_types=()):
        if n in tvars:
            return 'typevar', n
        for lt in local_types:
            if lt.name == n:
                return 'local', lt
        # enclosing types: type parameters, member types (inherited included), the type's own name
        o = td
        while o is not None:
            if any(p.name == n for p in o.type_params):
                return 'typevar', n
            m = self.member_type(o, n)
            if m is not None:
                return self._decl_result(m)
            if o.name == n and not o.anonymous:
                return self._decl_result(o)
            o = o.outer
        # single-type imports
        for imp in cu.imports:
            if imp.wildcard:
                continue
            if imp.name.rpartition('.')[2] == n:
                if not imp.static:
                    return ('project' if imp.name in self.types else 'external'), imp.name
                if imp.name in self.types:
                    return 'project', imp.name
        # the unit's own and same-package types
        for t in cu.types:
            if t.name == n:
                return self._decl_result(t)
        fqn = self.packages.get(cu.package or '', {}).get(n)
        if fqn is not None:
            return 'project', fqn
        # on-demand imports and java.lang
        project = []
        external = []
        for imp in cu.imports:
            if not imp.wildcard:
                continue
            if imp.name in self.types:
                m = self.member_type(self.types[imp.name], n)
                if m is not None:
                    project.append(m.fqn)
                continue
            if imp.static:
                continue
            f = self.packages.get(imp.name, {}).get(n)
            if f is not None:
                project.append(f)
            elif n in KNOWN_EXTERNAL_PACKAGES.get(imp.name, ()) or n in self.learned.get(imp.name, ()):
                if imp.name not in self.packages:
                    external.append(f'{imp.name}.{n}')
        if n in JAVA_LANG:
            external.append(f'java.lang.{n}')
        project = sorted(set(project))
        external = sorted(set(external))
        if len(project) == 1:
            return 'project', project[0]
        if len(project) > 1:
            return 'ambiguous', project
        if len(external) == 1:
            return 'external', external[0]
        if len(external) > 1:
            return 'ambiguous', external
        opaque = [imp.name for imp in cu.imports if imp.wildcard and not imp.static and imp.name not in self.packages
                  and imp.name not in self.types and imp.name not in KNOWN_EXTERNAL_PACKAGES]
        if len(opaque) == 1 and n[:1].isupper() and not self._has_external_supertype(td):
            return 'external_guess', f'{opaque[0]}.{n}'
        return 'unresolved', None

    def _has_external_supertype(self, td):
        """True when td, an enclosing type or one of their project supertypes extends a type outside the project (whose member
        types the index cannot see)."""
        seen = set()
        stack = []
        o = td
        while o is not None:
            stack.append(o)
            o = o.outer
        while stack:
            o = stack.pop()
            if id(o) in seen:
                continue
            seen.add(id(o))
            key = ('ext', id(o))
            if key in self._in_progress:
                return True
            self._in_progress.add(key)
            try:
                ctx = o.outer if o.anonymous and o.outer is not None else o
                for ref in o.extends + o.implements:
                    kind, value = self.resolve_kind(ref.name, ctx)
                    if kind == 'project':
                        stack.append(self.types[value])
                    elif kind == 'local':
                        stack.append(value)
                    else:
                        return True
            finally:
                self._in_progress.discard(key)
        return False

    @staticmethod
    def _decl_result(td):
        fqn = td.fqn
        if fqn is None:
            return 'local', td
        return 'project', fqn

    def resolve_type(self, ref, context):
        if ref.wildcard:
            return {'wildcard': ref.wildcard, 'bound': self.resolve_type(ref.bound, context) if ref.bound is not None else None}
        kind, value = self.resolve_kind(ref.name, context)
        return {'name': ref.name, 'fqn': value if kind in _RESOLVED_KINDS else None, 'kind': kind,
                'args': None if ref.args is None else [self.resolve_type(a, context) for a in ref.args], 'dims': ref.dims}

    def to_json(self, resolve=False, bodies=False):
        units = sorted(self.units, key=lambda u: (u.root or '', u.relpath or u.path))
        return {'version': IR_VERSION, 'roots': self.roots,
                'units': [u.to_json(self if resolve else None, bodies) for u in units],
                'duplicates': [list(d) for d in self.duplicates]}


# ----------------------------------------------------------------------------------------------------------------------------------
# Files and CLI
# ----------------------------------------------------------------------------------------------------------------------------------

def find_java_files(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for f in filenames:
            if f.endswith('.java'):
                out.append(os.path.join(dirpath, f))
    out.sort(key=lambda p: p.replace('\\', '/'))
    return out


def exercise_bodies(cu):
    """Runs every body helper over every body, initializer and argument span of cu (including anonymous and local classes);
    returns the number of spans analysed. Used by the full-tree test and --bodies."""
    count = 0
    stack = list(cu.types)
    seen = set()
    while stack:
        td = stack.pop()
        if id(td) in seen:
            continue
        seen.add(id(td))
        spans = [m.body for m in td.methods if m.body is not None]
        spans += [f.initializer for f in td.fields if f.initializer is not None]
        spans += [i.body for i in td.initializers]
        for c in td.enum_constants:
            spans += c.args or []
            if c.body is not None:
                stack.append(c.body)
        stack.extend(td.types)
        for s in spans:
            count += 1
            s.lambdas()
            s.local_variables()
            s.identifier_refs()
            s.assignments()
            s.method_calls()
            s.method_refs()
            for ne in s.anonymous_classes():
                stack.append(ne.anonymous)
            stack.extend(s.local_types())
    return count


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description='Parse Java sources and optionally write the JSON IR.')
    ap.add_argument('roots', nargs='+')
    ap.add_argument('--ir', help='write the JSON IR to this file')
    ap.add_argument('--resolve', action='store_true', help='annotate type references with resolved FQNs')
    ap.add_argument('--bodies', action='store_true', help='also run the body helpers over every body')
    args = ap.parse_args(argv)
    t0 = time.perf_counter()
    idx = ProjectIndex.from_roots(args.roots)
    t1 = time.perf_counter()
    n_tokens = sum(len(u.tokens) for u in idx.units)
    n_bytes = sum(len(u.source) for u in idx.units)
    print(f'parsed {len(idx.units)} files, {len(idx.types)} types, {n_tokens} tokens, {n_bytes / 1e6:.1f} MB in {t1 - t0:.1f}s '
          f'({n_bytes / 1e6 / max(t1 - t0, 1e-9):.2f} MB/s)')
    if idx.duplicates:
        for d in idx.duplicates:
            print(f'duplicate type {d[0]}: {d[1]} and {d[2]}', file=sys.stderr)
    if args.bodies:
        spans = sum(exercise_bodies(u) for u in idx.units)
        t2 = time.perf_counter()
        print(f'analysed {spans} body spans in {t2 - t1:.1f}s')
    if args.ir:
        with open(args.ir, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(idx.to_json(resolve=args.resolve), f, indent=1, ensure_ascii=False)
            f.write('\n')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except JavaSyntaxError as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)
