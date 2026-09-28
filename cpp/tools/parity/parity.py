"""parity: structural parity between a Java handler and the C++ generated or hand-ported from it (phase6-inventory.md §7.6 items 1 and 4,
phase6-questgen-prototype.md §8.1 and §8.4).

Per file pair it reads the class body only (Java: from `class X` to the end of the file; C++: from `class X` to the AION_*_HANDLER marker)
and compares, after the documented renames below and nothing else:

- literals: integer, floating, string, char, boolean and null literals, as a multiset and as the ordered sequence. The ordered check is the
  one G2's twin tool needs (phase6-inventory.md §7.2): a multiset cannot see two literals swapped between positions;
- calls: the ordered sequence of called (and declared) names, and their multiset; the enum companion calls whose position moves
  (`WorldMapType.X.getId()` -> `getId(WorldMapType::X)`) are compared as a multiset only;
- operators: the ordered sequence of comparison, logical, arithmetic, bitwise, increment and conditional (`?`) operators;
- constants: the ordered sequence of ALL_CAPS names (DialogAction and enum constants, STR_* messages, SM_* packets);
- spawn ids: the multiset of the int literal that starts the argument list of every spawn* call, over the code (comments dropped);
- spawn analyzer: the set of npc ids QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers (QuestSpawnAnalyzer.java:101-127) finds in the file,
  its own regular expression over the whole raw text of each side, comments and ternaries included (the npcIdsSpawnedByHandlers parity of
  handlers-and-porting-plan.md:291-297: a dropped `// spawn(216239, ...)` changes it).

The seed is the prototype review's three scratch checks (literal, operator and call sequences over the 910 emitted files); this module
tokenizes instead of matching regular expressions (the spawn analyzer check aside, which must be the analyzer's own), and adds the
multisets, the constants and the spawn ids.

Not compared: plain identifiers (locals, receivers, operands) and plain `=`. `return var > targetId;` against `return targetId > var;` or
a changed receiver is at parity; the golden trace (tools/oracle/questtrace) is the behaviour check. There is no waiver yet: the per-line
`// parity: <reason>` of handlers-and-porting-plan.md §3.1 item 2 is not implemented, so a hand port with an owner-approved deviation
cannot pass (comments are dropped).

Documented renames (each undoes one emitter idiom of phase6-questgen-prototype.md §2 or one C++ spelling; see IDIOMS):

- comments, preprocessor lines, template argument lists (`static_cast<float>`, `runtime::Ptr<Npc>`, `std::array<int32_t, 3>`, Java
  generics) and Java annotations are dropped, so a C++ array size or cast type is not a literal or a comparison;
- a C++ identifier escaped for a keyword or macro (`register_`, `delete_`, `NULL_`) loses its trailing underscore;
- `nullptr` and `std::nullopt` are the null literal; C++ char prefixes (`u'a'`) are dropped; `901.0f` equals Java `901f`;
- Java `x instanceof T` is the operator `!=` followed by a null literal (C++ `runtime::as<T>(x) != nullptr`);
- Java `a.equals(b)` is the operator `==` (C++ `a == b`); compound assignments count as their operator (Java narrows `x += y`
  implicitly, the C++ spells out `x = static_cast<int32_t>(x + y)`); plain `=` is not compared (a C++ array initialiser has none);
- C++ unary `*` and `&` (dereference, address, lambda capture) are not operators; `T* name` / `T& name` declarators are skipped on both
  sides by the same rule;
- calls: C++ `cast`/`as`/`static_cast` (the Java casts), `value` (unboxing, Java `intValue`), `QuestEnv::create` -> `QuestEnv` (Java
  `new QuestEnv`), `front`/`back`/`empty` -> `getFirst`/`getLast`/`isEmpty` (java.util.List on std::vector), Java `super(...)` ->
  `AbstractQuestHandler`, Java `arr.length` -> `size` (C++ `arr.size()`), and `get`, `at` and a Java index that is not an int literal are
  one name (`a[i]` -> `a.at(i)`, `list.get(i)` -> `list.at(i)`);
- the class qualifier of a Java static import (`import static ...SM_SYSTEM_MESSAGE.STR_X;`) is dropped from the C++ `SM_SYSTEM_MESSAGE::STR_X`;
- Java `workItems.getFirst()` / `workItems.getLast()` are compared as `workItems.get(0)` / `workItems.get(workItems.size() - 1)`, the C++
  spelling of the P6-T emitter rule (`Rc<ArrayList>` has get and size, not getFirst and getLast).

    python parity.py pair JAVA CPP [--json]
    python parity.py tree --java-dir DIR --cpp-dir DIR [--only REL ...] [--json OUT] [--require-all]

Exit 0 when every compared pair is at parity, 1 when a pair is not, 2 on a usage or input error. Standard library only.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

# --- tokens ------------------------------------------------------------------------------------------------------------------------

_TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<comment>//[^\n]*|/\*[\s\S]*?\*/)
  | (?P<string>(?:u8|u|U|L)?"(?:\\.|[^"\\\n])*")
  | (?P<char>(?:u8|u|U|L)?'(?:\\.|[^'\\\n])+')
  | (?P<number>0[xX][0-9a-fA-F_]+[lLuU]*|0[bB][01_]+[lLuU]*|(?:\d[\d_]*(?:\.[\d_]*)?|\.\d[\d_]*)(?:[eE][+-]?\d+)?[fFdDlLuU]*)
  | (?P<ident>[A-Za-z_$][\w$]*)
  | (?P<op>>>>=|<<=|>>=|>>>|\.\.\.|->|::|\+\+|--|&&|\|\||==|!=|<=|>=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|[-+*/%=<>!~&|^?:;,.(){}\[\]@\#])
''', re.X)


@dataclass(frozen=True)
class Tok:
    kind: str       # string char number ident op
    text: str


class ParityError(Exception):
    """input the tool does not understand (an unterminated literal, no class)"""


def tokenize(text):
    out = []
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if m is None:
            raise ParityError(f'cannot tokenize at offset {pos}: {text[pos:pos + 20]!r}')
        kind = m.lastgroup
        if kind not in ('ws', 'comment'):
            out.append(Tok(kind, m.group(0)))
        pos = m.end()
    return out


# --- the class body ----------------------------------------------------------------------------------------------------------------

def java_region(text):
    """the tokens of the top-level class (from `class`), and the static imports: {(Class, member)}"""
    imports = set(re.findall(r'^\s*import\s+static\s+(?:[\w.]+\.)?(\w+)\.(\w+)\s*;', text, re.M))
    toks = tokenize(text)
    for i, t in enumerate(toks):
        if t.kind == 'ident' and t.text == 'class':
            return toks[i:], imports
    raise ParityError('no class in the Java file')


_MARKER = re.compile(r'\bAION_\w+_HANDLER$')


def cpp_region(text):
    """the tokens from the handler's `class` to its AION_*_HANDLER marker (preprocessor lines dropped)"""
    text = re.sub(r'^[ \t]*#[^\n]*', '', text, flags=re.M)
    toks = tokenize(text)
    start = next((i for i, t in enumerate(toks) if t.kind == 'ident' and t.text == 'class'), None)
    if start is None:
        raise ParityError('no class in the C++ file')
    end = next((i for i in range(start, len(toks)) if toks[i].kind == 'ident' and _MARKER.search(toks[i].text)), len(toks))
    return toks[start:end]


# --- normalisation -----------------------------------------------------------------------------------------------------------------

CPP_KEYWORDS = frozenset('''alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl
concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast
else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator
or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct
switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor
xor_eq'''.split())
# statement keywords and other names that are followed by '(' without being a call
NOT_CALLS = frozenset('''if for while switch return catch synchronized sizeof alignof decltype typeid static_cast dynamic_cast const_cast
reinterpret_cast cast as value intValue string_view noexcept requires'''.split())
# names after which a '<' opens a template argument list in C++ (plus any capitalised type name, on both sides)
TEMPLATES = frozenset('''static_cast dynamic_cast const_cast reinterpret_cast cast as Ptr Ref array optional vector initializer_list span
unique_ptr shared_ptr function pair tuple set map unordered_map unordered_set Field Borrowed'''.split())
TYPE_ARG_TOKENS = frozenset(', . ? [ ] < > >> >>> :: * & extends super const'.split())
OPERAND_END_WORDS = frozenset(('this', 'true', 'false', 'null', 'nullptr'))
NOT_OPERAND_WORDS = frozenset('''return case new throw else do instanceof delete sizeof co_return co_yield goto'''.split())
# calls whose argument order moves in C++ (an enum method becomes a companion free function taking the enum first)
COMPANIONS = frozenset(('getId', 'id', 'getRewardPageByIndex', 'getStartingClass', 'isStartingClass'))
CALL_RENAMES = {'front': 'getFirst', 'back': 'getLast', 'empty': 'isEmpty', 'at': 'get', 'super': 'AbstractQuestHandler'}
BINARY_OPS = frozenset('== != < > <= >= && || + - * / % & | ^ << >> >>> ?'.split())
COMPOUND = {'+=': '+', '-=': '-', '*=': '*', '/=': '/', '%=': '%', '&=': '&', '|=': '|', '^=': '^', '<<=': '<<', '>>=': '>>', '>>>=': '>>>'}
DECLARATOR_FOLLOW = frozenset(') , = ; { : ['.split())
CONSTANT = re.compile(r'^[A-Z][A-Z0-9_]*[A-Z0-9]_?$')

# the idioms the renames undo, for the report (phase6-questgen-prototype.md §2 "The mappings")
IDIOMS = (
    'template argument lists dropped (casts, Ptr/Ref, std::array sizes, Java generics)',
    'keyword/macro escapes (register_, delete_, NULL_) lose the trailing underscore',
    'nullptr / std::nullopt are Java null; u\'c\' is Java \'c\'; 901.0f is Java 901f',
    'x instanceof T is != and null (runtime::as<T>(x) != nullptr); a.equals(b) is ==; x op= y is op; plain = is not compared',
    'unary * and & (dereference, address, capture) and T*/T& declarators are not operators',
    'casts and unboxing are not calls; QuestEnv::create is new QuestEnv; front/back/empty are getFirst/getLast/isEmpty; super(...) is the '
    'AbstractQuestHandler constructor; arr.length is size(); get, at and a non-literal index are one name',
    'the class of a Java static import is dropped from the C++ qualified name',
    'workItems.getFirst()/getLast() are workItems.get(0)/get(workItems.size() - 1)',
)


def _skip_type_args(toks, i, allow_numbers):
    """toks[i] is '<': the index after its matching '>' when what lies between is a type argument list, else None"""
    depth = 0
    j = i
    while j < len(toks):
        t = toks[j]
        if t.kind == 'op' and t.text == '<':
            depth += 1
        elif t.kind == 'op' and t.text in ('>', '>>', '>>>'):
            depth -= len(t.text)
            if depth <= 0:
                return j + 1
        elif t.kind == 'ident' or (t.kind == 'op' and t.text in TYPE_ARG_TOKENS) or (allow_numbers and t.kind == 'number'):
            pass
        else:
            return None
        j += 1
    return None


def _drop_templates(toks, cpp):
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        out.append(t)
        if t.kind == 'ident' and i + 1 < len(toks) and toks[i + 1].text == '<' and (t.text in TEMPLATES or t.text[:1].isupper()):
            k = _skip_type_args(toks, i + 1, allow_numbers=cpp)
            if k is not None:
                i = k
                continue
        i += 1
    return out


def _drop_annotations(toks):
    """Java `@Name` and `@Name(...)` (their literals are not code)"""
    out = []
    i = 0
    while i < len(toks):
        if toks[i].text == '@' and i + 1 < len(toks) and toks[i + 1].kind == 'ident' and toks[i + 1].text != 'interface':
            i += 2
            while i + 1 < len(toks) and toks[i].text == '.' and toks[i + 1].kind == 'ident':
                i += 2
            if i < len(toks) and toks[i].text == '(':
                i = _match(toks, i) + 1
            continue
        out.append(toks[i])
        i += 1
    return out


def _match(toks, i):
    """index of the bracket closing toks[i]"""
    pairs = {'(': ')', '[': ']', '{': '}'}
    o, c = toks[i].text, pairs[toks[i].text]
    depth = 0
    for j in range(i, len(toks)):
        if toks[j].text == o:
            depth += 1
        elif toks[j].text == c:
            depth -= 1
            if depth == 0:
                return j
    raise ParityError(f'unbalanced {o!r}')


def _java_rewrites(toks):
    """Java spellings rewritten into the C++ shape of an emitter rule (the workItems accessor rule)"""
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t.text == 'workItems' and i + 4 < len(toks) and toks[i + 1].text == '.' and toks[i + 2].text in ('getFirst', 'getLast') \
                and toks[i + 3].text == '(' and toks[i + 4].text == ')':
            out += [t, Tok('op', '.'), Tok('ident', 'get'), Tok('op', '(')]
            if toks[i + 2].text == 'getFirst':
                out.append(Tok('number', '0'))
            else:
                out += [Tok('ident', 'workItems'), Tok('op', '.'), Tok('ident', 'size'), Tok('op', '('), Tok('op', ')'), Tok('op', '-'),
                        Tok('number', '1')]
            out.append(Tok('op', ')'))
            i += 5
            continue
        out.append(t)
        i += 1
    return out


def _cpp_rewrites(toks, static_imports):
    """C++ spellings mapped back: `QuestEnv::create(` is `QuestEnv(`, a static import's class qualifier is dropped"""
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t.text == 'QuestEnv' and i + 3 < len(toks) and toks[i + 1].text == '::' and toks[i + 2].text == 'create' and toks[i + 3].text == '(':
            out.append(t)
            i += 3
            continue
        if t.kind == 'ident' and i + 2 < len(toks) and toks[i + 1].text == '::' and (t.text, toks[i + 2].text) in static_imports:
            i += 2
            continue
        out.append(t)
        i += 1
    return out


# QuestSpawnAnalyzer.java:101, with Java's ASCII \d
SPAWN_ANALYZER = re.compile(r'\bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))?', re.ASCII)


def spawn_analyzer_ids(text):
    """the npc ids QuestSpawnAnalyzer.parseSpawnNpcIds adds for this raw source text"""
    return {int(g) for m in SPAWN_ANALYZER.finditer(text) for g in m.groups() if g is not None}


# --- facts -------------------------------------------------------------------------------------------------------------------------

@dataclass
class Facts:
    literals: list = field(default_factory=list)      # [(kind, value)]
    calls: list = field(default_factory=list)         # ordered names, companions excluded
    companions: Counter = field(default_factory=Counter)
    operators: list = field(default_factory=list)
    constants: list = field(default_factory=list)
    spawn_ids: Counter = field(default_factory=Counter)


def _number(text, cpp):
    t = text.replace('_', '') if not cpp else text.replace("'", '')
    low = t.lower()
    if low.startswith('0x'):
        return ('int', int(low.rstrip('lu'), 16))
    if low.startswith('0b'):
        return ('int', int(low[2:].rstrip('lu'), 2))
    is_float = '.' in t or 'e' in low or low.endswith(('f', 'd'))
    if not is_float:
        digits = low.rstrip('lu')
        if len(digits) > 1 and digits.startswith('0'):
            return ('int', int(digits, 8))
        return ('int', int(digits))
    kind = 'float' if low.endswith('f') else 'double'
    return (kind, float(low.rstrip('fd')))


def _char(text):
    return ('char', text[text.index("'"):])


def _unescape_ident(name):
    """C++ escapes of a Java name that is a C++ keyword (register_) or a macro (NULL_)"""
    if name.endswith('_') and not name.endswith('__'):
        stem = name[:-1]
        if stem in CPP_KEYWORDS or CONSTANT.match(stem):
            return stem
    return name


def _operand_end(t):
    if t is None:
        return False
    if t.kind in ('number', 'string', 'char'):
        return True
    if t.kind == 'ident':
        return t.text not in NOT_OPERAND_WORDS
    return t.text in (')', ']', '++', '--')


def facts(toks, cpp):
    """the parity facts of a normalised token list"""
    toks = _drop_templates(toks, cpp)
    f = Facts()
    n = len(toks)
    for i, t in enumerate(toks):
        prev = toks[i - 1] if i > 0 else None
        nxt = toks[i + 1] if i + 1 < n else None
        k, x = t.kind, t.text
        if k == 'number':
            f.literals.append(_number(x, cpp))
        elif k == 'string':
            f.literals.append(('string', x[x.index('"'):]))
        elif k == 'char':
            f.literals.append(_char(x))
        elif k == 'ident':
            name = _unescape_ident(x) if cpp else x
            if name in ('true', 'false'):
                f.literals.append(('bool', name))
                continue
            if name in ('null', 'nullptr', 'nullopt'):
                f.literals.append(('null', ''))
                continue
            if name == 'instanceof' and not cpp:
                f.operators.append('!=')
                f.literals.append(('null', ''))
                continue
            if CONSTANT.match(name) and len(name) > 1:
                f.constants.append(name)
            if not cpp and name == 'length' and prev is not None and prev.text == '.' and (nxt is None or nxt.text != '('):
                f.calls.append('size')
                continue
            if nxt is not None and nxt.text == '(':
                if not cpp and name == 'equals' and prev is not None and prev.text == '.':
                    f.operators.append('==')
                    continue
                if name in NOT_CALLS:
                    continue
                if not cpp and name == 'new':
                    continue
                name = CALL_RENAMES.get(name, name)
                if name in COMPANIONS:
                    f.companions[name] += 1
                else:
                    f.calls.append(name)
                if name.startswith('spawn') and i + 3 < n and toks[i + 2].kind == 'number' and toks[i + 3].text in (',', ')'):
                    f.spawn_ids[_number(toks[i + 2].text, cpp)[1]] += 1
        elif k == 'op':
            if x == '[' and _operand_end(prev) and nxt is not None and nxt.text != ']' and not cpp:
                close = _match(toks, i)
                if not (close == i + 2 and nxt.kind == 'number'):
                    f.calls.append('get')           # a[i] -> a.at(i): the emitter keeps [] only for an int literal in range
                continue
            if x in COMPOUND:
                f.operators.append(COMPOUND[x])
            elif x in ('++', '--', '!', '~'):
                f.operators.append(x)
            elif x in ('+', '-'):
                f.operators.append(x if _operand_end(prev) else 'u' + x)
            elif x in ('*', '&'):
                if not _operand_end(prev):
                    continue                        # C++ dereference, address-of, lambda capture
                if prev.kind == 'ident' and nxt is not None and nxt.kind == 'ident' and i + 2 < n and toks[i + 2].text in DECLARATOR_FOLLOW:
                    continue                        # `T* name` / `T& name` (the same rule on both sides)
                f.operators.append(x)
            elif x in BINARY_OPS:
                f.operators.append(x)
    return f


def java_facts(text):
    toks, imports = java_region(text)
    return facts(_java_rewrites(_drop_annotations(toks)), cpp=False), imports


def cpp_facts(text, static_imports=frozenset()):
    return facts(_cpp_rewrites(cpp_region(text), static_imports), cpp=True)


# --- comparison --------------------------------------------------------------------------------------------------------------------

CHECKS = ('literals-multiset', 'literals-order', 'calls-multiset', 'calls-order', 'companions', 'operators-order', 'constants-order',
          'spawn-ids', 'spawn-analyzer')


@dataclass
class Mismatch:
    check: str
    detail: str

    def __str__(self):
        return f'{self.check}: {self.detail}'


def _first_difference(a, b):
    k = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
    return f'at {k} of {len(a)} (Java) / {len(b)} (C++): Java {a[max(0, k - 2):k + 3]} C++ {b[max(0, k - 2):k + 3]}'


def _multiset_difference(a, b):
    ca, cb = Counter(a), Counter(b)
    only_java = sorted((ca - cb).elements(), key=repr)
    only_cpp = sorted((cb - ca).elements(), key=repr)
    return f'only in Java {only_java[:6]}, only in C++ {only_cpp[:6]}'


def compare_facts(j, c):
    out = []
    if Counter(j.literals) != Counter(c.literals):
        out.append(Mismatch('literals-multiset', _multiset_difference(j.literals, c.literals)))
    if j.literals != c.literals:
        out.append(Mismatch('literals-order', _first_difference(j.literals, c.literals)))
    if Counter(j.calls) != Counter(c.calls):
        out.append(Mismatch('calls-multiset', _multiset_difference(j.calls, c.calls)))
    if j.calls != c.calls:
        out.append(Mismatch('calls-order', _first_difference(j.calls, c.calls)))
    if j.companions != c.companions:
        out.append(Mismatch('companions', f'Java {dict(j.companions)} C++ {dict(c.companions)}'))
    if j.operators != c.operators:
        out.append(Mismatch('operators-order', _first_difference(j.operators, c.operators)))
    if j.constants != c.constants:
        out.append(Mismatch('constants-order', _first_difference(j.constants, c.constants)))
    if j.spawn_ids != c.spawn_ids:
        out.append(Mismatch('spawn-ids', f'Java {dict(j.spawn_ids)} C++ {dict(c.spawn_ids)}'))
    return out


def compare(java_text, cpp_text):
    """[Mismatch] between a Java handler's text and its C++ text; empty when they are at parity"""
    jf, imports = java_facts(java_text)
    out = compare_facts(jf, cpp_facts(cpp_text, imports))
    js, cs = spawn_analyzer_ids(java_text), spawn_analyzer_ids(cpp_text)
    if js != cs:
        out.append(Mismatch('spawn-analyzer', f'only in Java {sorted(js - cs)}, only in C++ {sorted(cs - js)}'))
    return out


def compare_files(java_path, cpp_path):
    j = Path(java_path).read_text(encoding='utf-8-sig')
    c = Path(cpp_path).read_text(encoding='utf-8-sig')
    return compare(j, c)


def compare_tree(java_dir, cpp_dir, only=None):
    """{rel: [Mismatch] or None when the C++ file is missing} for every Java file below java_dir (rel is the Java path below it)"""
    java_dir, cpp_dir = Path(java_dir), Path(cpp_dir)
    rels = sorted(p.relative_to(java_dir).as_posix() for p in java_dir.rglob('*.java'))
    if only:
        wanted = set(only)
        rels = [r for r in rels if r in wanted or Path(r).stem in wanted]
    out = {}
    for rel in rels:
        cpp = cpp_dir / (rel[:-len('.java')] + '.cpp')
        out[rel] = compare_files(java_dir / rel, cpp) if cpp.is_file() else None
    return out


def tree_report(results):
    compared = {r: m for r, m in results.items() if m is not None}
    bad = {r: [str(x) for x in m] for r, m in compared.items() if m}
    by_check = Counter(x.check for m in compared.values() if m for x in {y.check: y for y in m}.values())
    return {'format': 'aion-parity', 'version': 1, 'javaFiles': len(results), 'compared': len(compared),
            'missingCpp': len(results) - len(compared), 'mismatchedFiles': len(bad),
            'mismatchesByCheck': {k: by_check.get(k, 0) for k in CHECKS}, 'checks': list(CHECKS), 'renames': list(IDIOMS),
            'files': bad}


def main(argv=None):
    ap = argparse.ArgumentParser(prog='parity.py', description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('pair', help='one Java file against one C++ file')
    p.add_argument('java')
    p.add_argument('cpp')
    p.add_argument('--json', action='store_true', help='print the mismatches as JSON')
    p = sub.add_parser('tree', help='every Java file below --java-dir against the .cpp at the same relative path below --cpp-dir')
    p.add_argument('--java-dir', required=True)
    p.add_argument('--cpp-dir', required=True)
    p.add_argument('--only', nargs='+', metavar='REL')
    p.add_argument('--json', metavar='OUT', help='write the report as JSON')
    p.add_argument('--require-all', action='store_true', help='a Java file without its C++ file is a failure')
    args = ap.parse_args(argv)
    try:
        if args.cmd == 'pair':
            ms = compare_files(args.java, args.cpp)
            if args.json:
                print(json.dumps([{'check': m.check, 'detail': m.detail} for m in ms], indent=1))
            else:
                for m in ms:
                    print(m)
                print('parity: OK' if not ms else f'parity: {len(ms)} mismatches')
            return 1 if ms else 0
        results = compare_tree(args.java_dir, args.cpp_dir, args.only)
    except (ParityError, OSError) as e:
        print(f'parity: {e}', file=sys.stderr)
        return 2
    rep = tree_report(results)
    if args.json:
        Path(args.json).write_text(json.dumps(rep, indent=1), encoding='utf-8')
    print(f"parity: {rep['compared']} pairs compared, {rep['mismatchedFiles']} with mismatches, {rep['missingCpp']} Java files without C++")
    for check, n in rep['mismatchesByCheck'].items():
        if n:
            print(f'  {n:4}  {check}')
    for rel, ms in list(rep['files'].items())[:20]:
        print(f'  {rel}: {ms[0]}')
    failed = rep['mismatchedFiles'] or (args.require_all and rep['missingCpp'])
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
