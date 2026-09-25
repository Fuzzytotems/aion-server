"""emit: the Java-to-C++ quest handler transliterator (G1).

One Java file in, one C++ file out, or a refusal. The C++ file has the shape of the handler tree (regscan rules,
cpp/game-server/tools/regscan/README.md): the quest prelude first, the class in the namespace of its directory, the hooks as overrides of
AbstractQuestHandler with the C++ signatures, register() as register_(), the constants as static constexpr members, then the
AION_QUEST_HANDLER marker with the literal quest id. Statements are emitted in Java order and shape: if/else chains, switch groups with
their fall-through (never normalised; a `[[fallthrough]];` marks it), returns, loops. Java comments travel with their statements.

Expressions are typed against the C++ headers (cppdecl via api.Api): a Java local of a class type becomes `runtime::Ptr<T>` (Ref<T> for a
freshly created QuestEnv), member access is `->` on pointers and `.` on references, a pointer passed to a `T&` parameter is dereferenced,
Java `==` on objects is identity, `x instanceof T` is `runtime::as<T>(x) != nullptr`, `(T) x` is `runtime::cast<T>(x)`, a primitive
cast is static_cast (so `(float) 262.9` keeps Java's double-then-float rounding), `new int[] {...}` is a std::array, the varargs of
defaultOnLevelChangedEvent are a braced list, enum methods are their companion functions (`getId(WorldMapType::X)`), `Integer` is
std::optional<int32_t> and is unboxed with `.value()`.

Comments: a statement's own-line comments go before it and its trailing comment after it; so do the comments of case labels, of if/else
and loop heads without braces, and the comments before a method (Javadoc or not). Comments inside an expression are dropped. A Java
logic bug the inventory lists (KNOWN_JAVA_BUGS, phase6-inventory.md §11) is kept, as decision U3 says, and marked `// java-bug kept`.

Refusals (Unsupported categories) are collected per statement, so a file usually lists more than its first reason. They are a lower
bound: a method whose body does not parse (a lambda, an anonymous class, a switch expression, a throw) is refused whole, and a local of a
refused type is not checked further, so what such code calls is never looked at.
"""
from __future__ import annotations

import re
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from . import api as apimod
from . import jast, paths
from .jast import Unsupported

import javasrc  # noqa: E402
import skeleton  # noqa: E402

# --- C++ types of expressions ------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class CT:
    kind: str                # prim string enum obj array ilist optional vector null void any class unknown
    name: str = ''           # prim: C++ name; enum/obj/class: simple name; array: element C++ name
    ref: str = ''            # obj: ptr owning lref clref raw value
    elem: 'CT | None' = None
    size: int = -1           # array: static length, -1 unknown

    def __str__(self):
        if self.kind == 'obj':
            return f'{self.ref}:{self.name}'
        if self.kind in ('optional', 'vector'):
            return f'{self.kind}<{self.elem}>'
        return f'{self.kind}:{self.name}' if self.name else self.kind


INT = CT('prim', 'int32_t')
LONG = CT('prim', 'int64_t')
BOOL = CT('prim', 'bool')
FLOAT = CT('prim', 'float')
DOUBLE = CT('prim', 'double')
BYTE = CT('prim', 'int8_t')
STRING = CT('string', 'std::string_view')
NULL = CT('null')
VOID = CT('void')
UNKNOWN = CT('unknown')

JAVA_PRIM = {'int': 'int32_t', 'long': 'int64_t', 'boolean': 'bool', 'float': 'float', 'double': 'double', 'byte': 'int8_t',
             'short': 'int16_t', 'char': 'char16_t'}
RANK = {'int8_t': 1, 'int16_t': 2, 'char16_t': 2, 'uint8_t': 1, 'uint16_t': 2, 'int32_t': 3, 'uint32_t': 3, 'int64_t': 4, 'float': 5,
        'double': 6}
CPP_PRIMS = set(RANK) | {'bool', 'void', 'size_t'}
# precedence of emitted C++ expressions: 0 postfix/primary, 1 unary, then the binary levels, 13 conditional and assignment
BIN_PREC = {'*': 2, '/': 2, '%': 2, '+': 3, '-': 3, '<<': 4, '>>': 4, '<': 5, '>': 5, '<=': 5, '>=': 5, '==': 6, '!=': 6, '&': 7, '^': 8,
            '|': 9, '&&': 10, '||': 11}
CLOSURES = ('lambda', 'anonymous-class', 'method-reference', 'local-class', 'inner-class')
# the order in which a file's primary refusal reason is chosen
PRIORITY = ['lambda', 'anonymous-class', 'method-reference', 'local-class', 'inner-class', 'initializer', 'switch-expression', 'throw',
            'try-catch', 'synchronized', 'labeled-statement', 'constructor-body', 'mutable-field', 'field-initializer', 'hook-not-virtual',
            'header-signature', 'api-missing', 'api-undeclared', 'type', 'generic-type']
# categories whose detail (the member or type) is part of the report key: 'api-missing: Class.member'
DETAILED = ('api-missing', 'api-undeclared', 'type', 'hook-not-virtual', 'header-signature')
# java.util.List methods that change the list
LIST_MUTATORS = frozenset('add addAll addFirst addLast remove removeAll removeIf retainAll replaceAll set sort clear'.split())

# Java logic bugs a faithful transliteration keeps (phase6-inventory.md §11; decision U3: a faithful port plus a note): file -> {Java line
# of the statement: note}. The note goes before that statement as a `// java-bug kept` comment and into the report.
KNOWN_JAVA_BUGS = {
    'beshmundir/_30348ImprovedAethercannon.java': {
        41: 'requires 100100716 (the item of _30343ImprovedMace.java:41); quest_data.xml collects 101900655'},
    'sanctum/_3963GrowthFlorasThirdCharm.java': {
        70: 'tryDecreaseKinah on the line above already took the kinah (Storage.java:82-88); this takes it a second time'},
    'sanctum/_3964GrowthFlorasFourthCharm.java': {
        70: 'tryDecreaseKinah on the line above already took the kinah (Storage.java:82-88); this takes it a second time'},
    'the_circle/_47106TurningUpTheAmplifiers.java': {
        31: 'registers kills of 217173 but counts (:36) and spawns (:62) 217175, so the quest cannot complete'},
}
JAVA_BUG_MARK = '// java-bug kept (U3, phase6-inventory.md §11): '


class Cascade(Exception):
    """a follow-on failure of an earlier refusal (an unknown variable): not recorded as a reason"""


@dataclass
class E:
    text: str
    ct: CT
    prec: int = 0
    lvalue: bool = False


@dataclass
class Var:
    name: str
    cpp: str
    ct: CT
    kind: str = 'local'      # local param field const member
    used: bool = False
    mark: str = ''           # placeholder before the declaration: '[[maybe_unused]] ' when the local is never read
    origin: str = ''         # a hook parameter: 'AbstractQuestHandler.hook(name) is <C++ type as the header writes it>'


@dataclass
class MethodSig:
    name: str
    cpp: str
    params: list             # [CT]
    ret: CT
    static: bool = False


@dataclass
class FileResult:
    rel: str                 # path below data/handlers/quest, '/' separated
    klass: str = ''
    quest_id: int | None = None
    package: str = ''
    status: str = 'refused'  # ok | refused
    tier: str = ''           # A | B (ok files)
    reasons: list = field(default_factory=list)      # [(category, detail)]
    apis: Counter = field(default_factory=Counter)   # 'Class.member' -> call count
    api_rows: set = field(default_factory=set)       # row ids used
    undeclared: set = field(default_factory=set)     # 'Class.member' used through PLANNED
    idioms: Counter = field(default_factory=Counter)
    hazards: list = field(default_factory=list)      # parity hazards found in a transliterated file (evaluation order)
    java_bugs: list = field(default_factory=list)    # KNOWN_JAVA_BUGS notes placed in the output: 'line N: note'
    api_status: dict = field(default_factory=dict)   # 'Class.member' -> {body status of each C++ overload called}
    api_tier: dict = field(default_factory=dict)     # 'Class.member' -> 'core' or the API_TABLE row id
    cpp: str = ''
    lines: int = 0
    out_path: str = ''

    @property
    def primary(self):
        if not self.reasons:
            return None
        cats = [c for c, _ in self.reasons]
        for p in PRIORITY:
            if p in cats:
                return p
        return cats[0]

    def reason_keys(self):
        """the distinct reasons as report keys: 'api-missing: X.y' for API gaps, the category otherwise"""
        out = []
        for c, d in self.reasons:
            k = f'{c}: {d.split(" @ ")[0]}' if c in DETAILED else c
            if k not in out:
                out.append(k)
        return out


def cpp_ident(name):
    return skeleton.cpp_ident(name)


class Transliterator:
    """transliterate(path) -> FileResult; one instance serves every file (the Api is loaded once)"""

    def __init__(self, api=None, quest_dir=None):
        self.api = api or apimod.Api()
        self.ix = self.api.index
        self.quest_dir = Path(quest_dir or paths.JAVA_QUEST_DIR)
        self.aqh = self.ix.classes['AbstractQuestHandler']
        self.hooks = {n: fs for n, fs in self.aqh.methods.items() if any(f.virtual for f in fs) and n not in ('~AbstractQuestHandler',)}
        self.nonvirtual_hooks = {'rideAction', 'onProtectEndEvent', 'onProtectFailEvent'}

    # =================================================================================================================================
    # file level
    # =================================================================================================================================
    def transliterate(self, path):
        path = Path(path)
        try:
            rel = path.resolve().relative_to(self.quest_dir.resolve()).as_posix()
        except ValueError:
            rel = path.name
        self.r = FileResult(rel)
        self.includes = set()
        self.std_includes = set()
        self.ctor_const = None
        self.consts = []
        self.java_bugs = KNOWN_JAVA_BUGS.get(rel, {})
        self.java_bugs_placed = set()
        try:
            cu = javasrc.parse_file(str(path))
        except javasrc.JavaSyntaxError as e:
            self.r.reasons.append(('java-syntax', str(e)))
            return self.r
        self.cu = cu
        self.p = jast.Parser(cu)
        self.src = cu.tokens.source
        self.r.lines = self.src.count('\n') + (0 if self.src.endswith('\n') else 1)
        self.r.package = cu.package or ''
        try:
            text = self.file(cu)
        except Unsupported as e:
            self.refuse(e)
            text = ''
        except Cascade:
            text = ''
        if self.r.reasons:
            self.r.status = 'refused'
        else:
            self.r.status = 'ok'
            self.r.tier = 'B' if self.r.api_rows else 'A'
            self.r.cpp = text
            for line in sorted(set(self.java_bugs) - self.java_bugs_placed):
                self.r.java_bugs.append(f'line {line}: NOT PLACED (no statement starts on that line): {self.java_bugs[line]}')
        return self.r

    def java_bug_note(self, s, depth):
        """the `// java-bug kept` line before statement s when KNOWN_JAVA_BUGS names the Java line it starts on"""
        if not self.java_bugs:
            return []
        line = self.cu.tokens.loc(s.tok)[0]
        note = self.java_bugs.get(line)
        if note is None or line in self.java_bugs_placed:
            return []
        self.java_bugs_placed.add(line)
        self.r.java_bugs.append(f'line {line}: {note}')
        return ['\t' * depth + JAVA_BUG_MARK + note]

    def refuse(self, e):
        where = ''
        if e.tok is not None and e.tok >= 0:
            line, _ = self.cu.tokens.loc(e.tok)
            where = f' @ line {line}'
        detail = e.detail
        if e.category in DETAILED:
            detail = detail + where
        self.r.reasons.append((e.category, detail))

    def fail(self, category, detail, tok=-1):
        raise Unsupported(category, detail, tok)

    def file(self, cu):
        if len(cu.types) != 1:
            self.fail('file-shape', f'{len(cu.types)} top-level types')
        td = cu.types[0]
        self.td = td
        self.r.klass = td.name
        if td.kind != 'class' or not td.extends or td.extends[0].name != 'AbstractQuestHandler':
            self.fail('base-class', f'{td.kind} {td.name} extends {td.extends[0] if td.extends else "-"}', td.index)
        for t in td.types:
            self.refuse(Unsupported('inner-class', f'{t.kind} {t.name}', t.index))
        for ini in td.initializers:
            self.refuse(Unsupported('initializer', 'static' if ini.static else 'instance', ini.body.start))
        # single static imports other than DialogAction (SM_SYSTEM_MESSAGE.STR_MSG_DailyQuest_Ask_Mentee): member -> class
        self.static_imports = {}
        for imp in cu.imports:
            if imp.static and not imp.wildcard and not imp.name.endswith('DialogAction.' + imp.name.rpartition('.')[2]):
                cls, _, member = imp.name.rpartition('.')
                self.static_imports[member] = cls.rpartition('.')[2]
        # class members visible in bodies
        self.members = {'questId': Var('questId', 'questId', INT, 'member'),
                        'qe': Var('qe', 'qe', CT('obj', 'QuestEngine', 'lref'), 'member')}
        self.consts = []
        assigned = self.assigned_names(td)
        for f in td.fields:
            try:
                self.field(f, assigned)
            except Unsupported as e:
                self.refuse(e)
                self.members[f.name] = Var(f.name, cpp_ident(f.name), UNKNOWN, 'const')   # later uses cascade silently
        ctor_id = self.constructor(td)
        self.r.quest_id = ctor_id
        # own methods (helpers and hooks), for calls between them
        self.own = {}
        methods = [m for m in td.methods if m.kind == 'method']
        sigs = {}
        for m in methods:
            try:
                sig = self.method_sig(m)
                sigs[id(m)] = sig
                self.own.setdefault(m.name, []).append(sig)
            except Unsupported as e:
                self.refuse(e)
        out_methods = []
        for m in methods:
            if id(m) not in sigs:
                continue
            try:
                out_methods.append((m, self.method(m, sigs[id(m)])))
            except Unsupported as e:
                self.refuse(e)
            except Cascade:
                pass
        return self.render(td, ctor_id, out_methods)

    def assigned_names(self, td):
        """field names some method assigns (x = ..., this.x = ..., x++, x[i] = ...)"""
        out = set()
        for m in td.methods:
            if m.body is None:
                continue
            for a in m.body.assignments():
                if not a.local:
                    out.add(a.name)
        return out

    def constructor(self, td):
        ctors = [m for m in td.methods if m.kind == 'constructor']
        if len(ctors) != 1:
            self.fail('constructor-body', f'{len(ctors)} constructors', td.index)
        c = ctors[0]
        if c.params:
            self.fail('constructor-body', 'constructor with parameters', c.index)
        texts = c.body.texts()
        # { super ( X ) ; }
        if len(texts) != 7 or texts[1:3] != ['super', '('] or texts[4:6] != [')', ';']:
            self.fail('constructor-body', ' '.join(texts[1:-1])[:80], c.body.start)
        arg = texts[3]
        if re.fullmatch(r'\d+', arg):
            return int(arg)
        for v in self.consts:
            if v[0] == arg and re.fullmatch(r'\d+', v[3]):
                self.ctor_const = arg
                return int(v[3])
        self.fail('constructor-body', f'super({arg}): not an int constant', c.body.start)

    # -- fields -----------------------------------------------------------------------------------------------------------------------
    def field(self, f, assigned):
        mods = set(f.modifiers)
        name = f.name
        if f.initializer is None:
            self.fail('mutable-field' if 'final' not in mods else 'field-initializer', f'{f.type} {name} (no initializer)', f.index)
        if 'final' not in mods and name in assigned:
            self.fail('mutable-field', f'{f.type} {name} (written by a method: per-player state in the singleton handler)', f.index)
        jt = jast.JType(f.type.name, f.type.dims, None if f.type.args is None else 'x', f.index)
        if f.type.args is not None:
            self.fail('generic-type', f'field {f.type} {name}', f.index)
        s = f.initializer.start
        if self.cu.tokens.text[s] == '{':
            init, _ = self.p.array_init(s)
        else:
            init = self.p.expr_span(s, f.initializer.end)
        ct = self.java_type(jt, f.index)
        cpp = cpp_ident(name)
        self.local_scope = [{}]
        self.marks = []
        self.method_stmts = []
        self.method_ret = VOID
        if ct.kind == 'array':
            items = init.items if isinstance(init, jast.ArrayInit) else (init.init.items if isinstance(init, jast.NewArray) else None)
            if items is None:
                self.fail('field-initializer', f'{name}: array field without an initializer list', f.index)
            texts = [self.convert(self.expr(it), CT('prim', ct.name)) for it in items]
            decl = f'static constexpr std::array<{ct.name}, {len(texts)}> {cpp}{{{", ".join(texts)}}};'
            self.std_includes.add('array')
            ct = CT('array', ct.name, size=len(texts))
        elif ct.kind in ('prim', 'string', 'enum'):
            e = self.expr(init)
            if not self.is_constant(init):
                self.fail('field-initializer', f'{name} = {f.initializer.text[:60]}', f.index)
            decl = f'static constexpr {self.decl_type(ct)} {cpp} = {self.convert(e, ct)};'
        else:
            self.fail('field-initializer', f'{f.type} {name} = {f.initializer.text[:60]}', f.index)
        comment = self.trailing(f.initializer.end)
        self.members[name] = Var(name, cpp, ct, 'const')
        self.consts.append((name, decl + comment, f.index, f.initializer.text.strip()))

    def is_constant(self, e):
        if isinstance(e, jast.Lit):
            return e.kind != 'null'
        if isinstance(e, jast.Name):
            v = self.members.get(e.name)
            return (v is not None and v.kind == 'const') or e.name in self.api.dialog_actions
        if isinstance(e, jast.FieldAccess):
            return isinstance(e.target, jast.Name) and (e.target.name in apimod.ENUMS or e.target.name == 'DialogAction')
        if isinstance(e, jast.Unary):
            return self.is_constant(e.expr)
        if isinstance(e, jast.Binary):
            return self.is_constant(e.left) and self.is_constant(e.right)
        if isinstance(e, (jast.Paren, jast.Cast)):
            return self.is_constant(e.expr)
        return False

    # -- types ------------------------------------------------------------------------------------------------------------------------
    def java_type(self, jt, tok=-1):
        """CT of a declared Java type (locals, parameters, fields)"""
        if jt.args is not None:
            self.fail('generic-type', str(jt), tok)
        name = apimod.NESTED.get(jt.name, jt.name)
        if jt.dims == 1 and jt.name in JAVA_PRIM:
            return CT('array', JAVA_PRIM[jt.name])
        if jt.dims:
            self.fail('type', f'{jt}', tok)
        if name in JAVA_PRIM:
            return CT('prim', JAVA_PRIM[name])
        if name == 'String':
            return STRING
        if name == 'Integer':
            return CT('optional', elem=INT)
        if name == 'var':
            return CT('var')
        if '.' in name:
            self.fail('type', jt.name, tok)
        if self.api.is_enum(name):
            return CT('enum', name)
        if name in self.ix.classes:
            return CT('obj', name, 'ptr')
        self.fail('type', jt.name, tok)

    def cpp_type(self, text):
        """CT of a C++ type as written in a header"""
        t = text.strip()
        if t.endswith('&&'):
            return CT('any')
        const = t.startswith('const ')
        if const:
            t = t[6:].strip()
        lref = t.endswith('&')
        if lref:
            t = t[:-1].strip()
        if t.endswith(' const'):
            t = t[:-6].strip()
        raw = t.endswith('*')
        if raw:
            t = t[:-1].strip()
            if t.startswith('const '):
                t = t[6:]
        m = re.fullmatch(r'(?:[\w:]*::)?(Ptr|Ref)<(.+)>', t)
        if m and not raw:
            inner = self.simple(m.group(2))
            return CT('obj', inner, 'ptr' if m.group(1) == 'Ptr' else 'owning')
        m = re.fullmatch(r'std::optional<(.+)>', t)
        if m:
            return CT('optional', elem=self.cpp_type(m.group(1)))
        m = re.fullmatch(r'std::span<(?:const )?(.+)>', t)
        if m:
            return CT('array', self.cpp_type(m.group(1)).name)
        m = re.fullmatch(r'std::array<(.+?),\s*(\d+)>', t)
        if m:
            return CT('array', self.cpp_type(m.group(1)).name, size=int(m.group(2)))
        m = re.fullmatch(r'std::initializer_list<(.+)>', t)
        if m:
            return CT('ilist', self.cpp_type(m.group(1)).name)
        m = re.fullmatch(r'std::vector<(.+)>', t)
        if m:
            return CT('vector', elem=self.cpp_type(m.group(1)))
        if t in ('std::string_view', 'std::string'):
            return STRING
        if t in CPP_PRIMS:
            return CT('void') if t == 'void' else CT('prim', 'int64_t' if t == 'size_t' else t)
        if re.fullmatch(r'[A-Z]', t):
            return CT('any')      # template parameter
        name = self.simple(t)
        if self.api.is_enum(name):
            return CT('enum', name)
        return CT('obj', name, ('clref' if const else 'lref') if lref else ('raw' if raw else 'value'))

    @staticmethod
    def simple(t):
        t = re.sub(r'<.*>', '', t.strip())
        return t.rpartition('::')[2].strip()

    def decl_type(self, ct):
        """C++ spelling of a CT in a declaration"""
        if ct.kind == 'prim':
            return ct.name
        if ct.kind == 'string':
            return 'std::string_view'
        if ct.kind == 'enum':
            self.need(ct.name)
            return self.api.cpp_name(ct.name)
        if ct.kind == 'obj':
            n = self.api.cpp_name(ct.name)
            self.need(ct.name)
            if ct.ref == 'ptr':
                return f'runtime::Ptr<{n}>'
            if ct.ref == 'owning':
                return f'runtime::Ref<{n}>'
            if ct.ref == 'raw':
                return f'const {n}*'
            if ct.ref in ('lref', 'clref'):
                return f'{n}&'
            return n
        if ct.kind == 'optional':
            self.std_includes.add('optional')
            return f'std::optional<{self.decl_type(ct.elem)}>'
        if ct.kind == 'array':
            self.std_includes.add('array')
            return f'std::array<{ct.name}, {ct.size}>'
        self.fail('type', str(ct))

    def need(self, name):
        """include the header of a class or enum the file names or dereferences, unless the prelude brings it"""
        h = self.api.header_of(name)
        if self.api.needs_include(h):
            self.includes.add(h)

    def spell_param_type(self, text):
        """a header's parameter type rewritten for the handler namespace (prelude names bare, others fully qualified)"""
        def repl(m):
            full = m.group(0)
            if full.startswith(('std::', 'runtime::')):
                return full
            name = full.rpartition('::')[2]
            self.need(name)
            return self.api.cpp_name(name)
        return re.sub(r'(?<![\w:])(?:\w+::)*[A-Z]\w*', repl, text)

    # =================================================================================================================================
    # methods
    # =================================================================================================================================
    def method_sig(self, m):
        if m.type_params:
            self.fail('generic-type', f'generic method {m.name}', m.index)
        if m.name == 'register':
            return MethodSig('register', 'register_', [], VOID)
        overrides = any(a.name == 'Override' for a in m.annotations)
        hook = self.hooks.get(m.name)
        if m.name in self.nonvirtual_hooks and (overrides or len(m.params) >= 1):
            self.fail('hook-not-virtual', f'AbstractQuestHandler.{m.name}', m.index)
        if hook and (overrides or any(len(f.params) == len(m.params) for f in hook)):
            fs = [f for f in hook if len(f.params) == len(m.params)]
            if not fs:
                self.fail('hook-signature', f'{m.name}/{len(m.params)}', m.index)
            f = fs[0]
            return MethodSig(m.name, cpp_ident(m.name), [self.cpp_type(p.type) for p in f.params], self.cpp_type(f.ret or 'void'))
        if overrides:
            self.fail('hook-signature', f'@Override {m.name} matches no C++ hook', m.index)
        params = []
        for p in m.params:
            if p.varargs:
                self.fail('type', f'varargs parameter {p.type}...', p.index)
            ct = self.java_type(jast.JType(p.type.name, p.type.dims, None if p.type.args is None else 'x', p.index), p.index)
            if ct.kind == 'obj' and ct.name in ('QuestEnv', 'Item'):
                ct = CT('obj', ct.name, 'lref')
            params.append(ct)
        rt = m.return_type
        ret = VOID if str(rt) == 'void' else self.java_type(jast.JType(rt.name, rt.dims, None if rt.args is None else 'x', m.index), m.index)
        return MethodSig(m.name, cpp_ident(m.name), params, ret, 'static' in m.modifiers)

    def method(self, m, sig):
        """(C++ lines of the member function, access)"""
        hook = sig.cpp == 'register_' or m.name in self.hooks
        self.local_scope = [{}]
        self.marks = []
        self.method_ret = sig.ret
        params_cpp = []
        if m.name in self.hooks and sig.cpp != 'register_':
            f = [f for f in self.hooks[m.name] if len(f.params) == len(m.params)][0]
            for jp, cp in zip(m.params, f.params):
                ct = self.cpp_type(cp.type)
                pn = cpp_ident(jp.name)
                written = re.sub(r'(?<![\w:])(?!std::)(?:\w+::)+', '', cp.type)
                self.local_scope[-1][jp.name] = Var(jp.name, pn, ct, 'param',
                                                    origin=f'AbstractQuestHandler.{m.name}({jp.name}) is {written}')
                params_cpp.append(f'{self.spell_param_type(cp.type)} {pn}')
            head = f'{self.spell_param_type(f.ret)} {sig.cpp}({", ".join(params_cpp)}) override'
        else:
            for jp, ct in zip(m.params, sig.params):
                pn = cpp_ident(jp.name)
                self.local_scope[-1][jp.name] = Var(jp.name, pn, ct, 'param')
                params_cpp.append(f'{self.decl_type(ct)} {pn}')
            ret = 'void' if sig.ret.kind == 'void' else self.decl_type(sig.ret)
            head = f'{"static " if sig.static else ""}{ret} {sig.cpp}({", ".join(params_cpp)})'
            if sig.cpp == 'register_':
                head = 'void register_() override'
        body_open = m.body.start
        stmts = self.parse_block(m.body.start)
        self.method_stmts = stmts
        lines = [head + ' {' + self.trailing(body_open)]
        lines += self.stmts(stmts, 1)
        lines += self.leading(self.cu.tokens.match[body_open], 1)
        lines.append('}')
        lines = self.finish_marks(lines)
        access = 'public' if (hook or 'public' in m.modifiers or 'protected' in m.modifiers) else 'private'
        pre = self.leading(self.first_token(m), 0)      # the Javadoc and any other comment lines before the method
        while pre and not pre[0]:
            pre.pop(0)                                   # render() puts one blank line between members
        return lines, access, pre

    def first_token(self, m):
        """index of the first token of a member declaration (annotations and modifiers included)"""
        i = m.index
        T = self.cu.tokens
        # walk back over the return type, modifiers and annotations to the previous ';' / '}' / '{'
        while i > 0 and T.text[i - 1] not in (';', '}', '{'):
            i -= 1
        return i

    def parse_block(self, open_i):
        try:
            return self.p.block_stmts(open_i)
        except Unsupported as e:
            self.refuse(e)
            raise Cascade() from None

    # =================================================================================================================================
    # statements
    # =================================================================================================================================
    def stmts(self, stmts, depth):
        out = []
        for s in stmts:
            out += self.leading(s.tok, depth)
            out += self.java_bug_note(s, depth)
            try:
                out += self.stmt(s, depth, top=True)
            except Cascade:
                out.append('\t' * depth + '/* refused */')
            except Unsupported as e:
                self.refuse(e)
                out.append('\t' * depth + '/* refused */')
                if isinstance(s, jast.Local):
                    for name, _d, _i, _t in s.decls:
                        self.local_scope[-1][name] = Var(name, cpp_ident(name), UNKNOWN)
        return out

    def body(self, s, depth):
        """the lines of a statement used as an if/loop body: a block's contents, or one indented statement"""
        if isinstance(s, jast.Block):
            self.local_scope.append({})
            lines = self.stmts(s.stmts, depth + 1) + self.leading(s.last, depth + 1)
            self.local_scope.pop()
            return lines, True
        self.local_scope.append({})
        lines = self.leading(s.tok, depth + 1) + self.java_bug_note(s, depth + 1) + self.stmt(s, depth + 1)
        self.local_scope.pop()
        return lines, False

    def stmt(self, s, depth, top=False):
        ind = '\t' * depth
        tr = self.trailing(s.last)
        if isinstance(s, jast.If):
            return self.if_(s, depth, ind, hoist=top)
        if isinstance(s, jast.Local):
            return [ind + self.local(s) + tr]
        if isinstance(s, jast.ExprStmt):
            e = self.expr(s.expr, stmt=True)
            return [ind + e.text + ';' + tr]
        if isinstance(s, jast.Return):
            if s.expr is None:
                return [ind + 'return;' + tr]
            e = self.expr(s.expr)
            return [ind + 'return ' + self.convert(e, self.method_ret) + ';' + tr]
        if isinstance(s, jast.Switch):
            return self.switch(s, depth)
        if isinstance(s, jast.Block):
            self.local_scope.append({})
            lines = [ind + '{' + self.trailing(s.tok)] + self.stmts(s.stmts, depth + 1) + self.leading(s.last, depth + 1) + [ind + '}' + tr]
            self.local_scope.pop()
            return lines
        if isinstance(s, jast.Break):
            return [ind + 'break;' + tr]
        if isinstance(s, jast.Continue):
            return [ind + 'continue;' + tr]
        if isinstance(s, jast.Empty):
            return [ind + ';' + tr]
        if isinstance(s, jast.ForEach):
            it = self.expr(s.iterable)
            if it.ct.kind == 'vector':
                ct = it.ct.elem
                jt = self.java_type(s.type, s.tok)
                if ct.kind != jt.kind or ct.name != jt.name:
                    self.fail('type', f'for-each {s.type} over {it.ct}', s.tok)
                self.local_scope.append({s.name: Var(s.name, cpp_ident(s.name), ct)})
                self.r.idioms['for-each over a std::vector'] += 1
                b, braces = self.body(s.body, depth)
                self.local_scope.pop()
                head = f'{ind}for ({self.decl_type(ct)} {cpp_ident(s.name)} : {it.text})'
                return self.wrap(head, b, braces, ind, s.body)
            if it.ct.kind != 'array':
                self.fail('type', f'for-each over {it.ct}', s.tok)
            ct = self.java_type(s.type, s.tok)
            self.local_scope.append({s.name: Var(s.name, cpp_ident(s.name), ct)})
            self.r.idioms['for-each over an int array'] += 1
            b, braces = self.body(s.body, depth)
            self.local_scope.pop()
            head = f'{ind}for ({self.decl_type(ct)} {cpp_ident(s.name)} : {it.text})'
            return self.wrap(head, b, braces, ind, s.body)
        if isinstance(s, jast.For):
            self.local_scope.append({})
            init = ''
            if s.init:
                if isinstance(s.init[0], jast.Local):
                    init = self.local(s.init[0])[:-1]
                else:
                    init = ', '.join(self.expr(x.expr, stmt=True).text for x in s.init)
            cond = self.convert(self.expr(s.cond), BOOL) if s.cond is not None else ''
            upd = ', '.join(self.expr(u, stmt=True).text for u in s.update)
            b, braces = self.body(s.body, depth)
            self.local_scope.pop()
            return self.wrap(f'{ind}for ({init}; {cond}; {upd})', b, braces, ind, s.body)
        if isinstance(s, jast.While):
            cond = self.convert(self.expr(s.cond), BOOL)
            b, braces = self.body(s.body, depth)
            return self.wrap(f'{ind}while ({cond})', b, braces, ind, s.body)
        if isinstance(s, jast.DoWhile):
            b, braces = self.body(s.body, depth)
            cond = self.convert(self.expr(s.cond), BOOL)
            return [ind + 'do {'] + b + [f'{ind}}} while ({cond});' + tr]
        self.fail('statement', type(s).__name__, s.tok)

    def wrap(self, head, body, braces, ind, stmt):
        if braces:
            return [head + ' {' + self.brace_comments(stmt.tok)] + body + [ind + '}' + self.trailing(stmt.last)]
        return [head + self.trailing(stmt.tok - 1)] + body       # a comment after the loop head's ')'

    def brace_comments(self, brace):
        """the trailing comments of the token before a '{' (a ')' or `else`, when Java puts the brace on the next line) and of the '{'"""
        return self.trailing(brace - 1) + self.trailing(brace)

    def if_(self, s, depth, ind, prefix='if', hoist=False):
        pre = self.instanceof_binding(s, ind) if self.has_binding(s) else []
        if pre and not hoist:
            self.fail('instanceof-pattern', 'a pattern binding in an if that is not a block statement', s.tok)
        cond = self.convert(self.expr(s.cond), BOOL)
        then, tb = self.body(s.then, depth)
        lines = list(pre)
        if tb:
            lines.append(f'{ind}{prefix} ({cond}) {{' + self.brace_comments(s.then.tok))
            lines += then
            closing = ind + '}'
        else:
            lines.append(f'{ind}{prefix} ({cond})' + self.trailing(s.then.tok - 1))      # a comment after the condition's ')'
            lines += then
            closing = None
        e = s.else_
        if e is None:
            if closing:
                lines.append(closing + self.trailing(s.then.last))
            return lines
        if isinstance(e, jast.If) and not self.has_binding(e):
            sub = self.if_(e, depth, ind, prefix='else if')
            if closing:
                sub[0] = closing + ' ' + sub[0].lstrip('\t')
            else:
                sub[0] = sub[0]
            return lines + sub
        eb, ebr = self.body(e, depth)
        if ebr:
            head = (closing + ' else {') if closing else (ind + 'else {')
            lines.append(head + self.brace_comments(e.tok))
            lines += eb
            lines.append(ind + '}' + self.trailing(e.last))
        else:
            tr = self.trailing(e.tok - 1)                   # a comment after `else`
            if closing:
                lines.append(closing + ' else' + tr)
            else:
                lines.append(ind + 'else' + tr)
            lines += eb
        return lines

    def has_binding(self, s):
        return any(isinstance(x, jast.InstanceOf) and x.binding for x in jast.walk_exprs(s.cond))

    def instanceof_binding(self, s, ind):
        """`if (!(x instanceof T n))` / `if (x instanceof T n)`: declare `runtime::Ptr<T> n = runtime::as<T>(x);` before the if and
        test `n != nullptr` (only when the pattern is the whole condition: hoisting it out of a longer condition would evaluate x where
        Java's short-circuit does not)"""
        found = [x for x in jast.walk_exprs(s.cond) if isinstance(x, jast.InstanceOf) and x.binding]
        c = s.cond
        while isinstance(c, jast.Paren) or (isinstance(c, jast.Unary) and c.op == '!'):
            c = c.expr
        if len(found) != 1 or c is not found[0]:
            self.fail('instanceof-pattern', 'a pattern binding inside a larger condition', s.tok)
        x = found[0]
        if any(x.binding in sc for sc in self.local_scope):
            self.fail('instanceof-pattern', f'binding {x.binding} redeclared', s.tok)
        ct = self.java_type(x.type, s.tok)
        e = self.expr(x.expr)
        self.need(ct.name)
        self.local_scope[-1][x.binding] = Var(x.binding, cpp_ident(x.binding), CT('obj', ct.name, 'ptr'))
        x.hoisted = True
        self.r.idioms['instanceof pattern hoisted'] += 1
        return [f'{ind}runtime::Ptr<{self.api.cpp_name(ct.name)}> {cpp_ident(x.binding)} = '
                f'runtime::as<{self.api.cpp_name(ct.name)}>({e.text});']

    def local(self, s):
        """one Java local declaration statement -> one C++ declaration (declarators of mixed C++ types are split)"""
        parts = []
        base = self.java_type(s.type, s.tok)
        for name, extra, init, name_tok in s.decls:
            if extra:
                self.fail('type', f'{s.type} {name}[]', name_tok)
            ct = base
            e = None
            if init is not None:
                if isinstance(init, jast.ArrayInit):
                    if ct.kind != 'array':
                        self.fail('type', f'array initializer for {s.type}', name_tok)
                    texts = [self.convert(self.expr(it), CT('prim', ct.name)) for it in init.items]
                    ct = CT('array', ct.name, size=len(texts))
                    v = self.new_local(name, ct)
                    parts.append(f'{v.mark}{self.decl_type(ct)} {cpp_ident(name)}{{{", ".join(texts)}}};')
                    self.r.idioms['int[] local as std::array'] += 1
                    continue
                e = self.expr(init)
                if ct.kind == 'var':
                    ct = e.ct
                    if ct.kind == 'obj' and ct.ref in ('lref', 'clref', 'value'):
                        ct = CT('obj', ct.name, 'ptr')
                if ct.kind == 'obj':
                    if e.ct.kind == 'obj' and e.ct.ref in ('lref', 'clref') and self.bindable_to_reference(name) \
                            and self.ix.is_subclass(e.ct.name, ct.name):
                        # a part, owner or singleton accessor (X&) kept in a local Java never reassigns or null-checks: X& (conventions)
                        ct = CT('obj', ct.name, 'lref')
                        self.r.idioms['reference accessor kept in a T& local'] += 1
                    elif e.ct.kind == 'obj' and e.ct.ref == 'owning':
                        ct = CT('obj', ct.name, 'owning')
                    elif e.ct.kind == 'obj' and e.ct.ref == 'raw':
                        ct = CT('obj', ct.name, 'raw')
                    elif e.ct.kind == 'obj' and e.ct.ref == 'value':
                        self.fail('type', f'{ct.name} {name} = a temporary', name_tok)
                    elif e.ct.kind == 'obj' and not self.ix.is_subclass(e.ct.name, ct.name) and not self.ix.is_subclass(ct.name, e.ct.name):
                        self.fail('type', f'{ct.name} {name} = {e.ct.name}', name_tok)
                if ct.kind == 'array' and e.ct.kind == 'array':
                    ct = e.ct
            elif ct.kind == 'var':
                self.fail('type', f'var {name} without initializer', name_tok)
            if ct.kind == 'array' and ct.size < 0:
                self.fail('type', f'{s.type} {name} without a fixed length', name_tok)
            text = f'{self.decl_type(ct)} {cpp_ident(name)}'
            if e is not None:
                text += ' = ' + self.convert(e, ct)
            v = self.new_local(name, ct)
            parts.append(v.mark + text + ';')
        return ' '.join(parts)

    def bindable_to_reference(self, name):
        """the current method never assigns `name` again and never compares it with null"""
        for x in jast.walk_exprs(self.method_stmts):
            if isinstance(x, jast.Assign) and isinstance(x.target, jast.Name) and x.target.name == name:
                return False
            if isinstance(x, jast.Binary) and x.op in ('==', '!=') and any(
                    isinstance(s, jast.Name) and s.name == name for s in (x.left, x.right)) and any(
                    isinstance(s, jast.Lit) and s.kind == 'null' for s in (x.left, x.right)):
                return False
        return True

    def new_local(self, name, ct):
        """declare a local in the current scope; its declaration starts with a mark that finish_marks() turns into [[maybe_unused]]
        when nothing reads it (Java handlers often fetch a value they never use; MSVC /W4 C4189)"""
        v = Var(name, cpp_ident(name), ct, 'local', False, f'\x00MU{len(self.marks)}\x00')
        self.marks.append(v)
        self.local_scope[-1][name] = v
        return v

    def finish_marks(self, lines):
        text = '\n'.join(lines)
        for v in self.marks:
            if v.mark in text:
                text = text.replace(v.mark, '' if v.used else '[[maybe_unused]] ')
                if not v.used:
                    self.r.idioms['[[maybe_unused]] on a local Java never reads'] += 1
        return text.split('\n')

    def switch(self, s, depth):
        ind = '\t' * depth
        subj = self.expr(s.expr)
        if subj.ct.kind == 'optional':
            subj = E(self.postfix(subj) + '.value()', subj.ct.elem)
            self.r.idioms['Integer unboxed with value()'] += 1
        if subj.ct.kind == 'string':
            self.fail('string-switch', 'switch on a String', s.tok)
        if subj.ct.kind not in ('prim', 'enum'):
            self.fail('type', f'switch on {subj.ct}', s.tok)
        # locals declared in one case group and used in a later one cannot be braced
        declared = []
        for labels, body, _ in s.groups:
            names = {n for st in body if isinstance(st, jast.Local) for n, *_ in st.decls}
            declared.append(names)
        for k, (labels, body, _) in enumerate(s.groups):
            used = {x.name for x in jast.walk_exprs(body) if isinstance(x, jast.Name)}
            for j in range(k):
                if declared[j] & used:
                    self.fail('switch-scope-local', f'{sorted(declared[j] & used)} declared in an earlier case group', s.tok)
        lines = [f'{ind}switch ({subj.text}) {{' + self.trailing(self.cu.tokens.match[s.tok + 1] + 1)]
        for k, (labels, body, lab_toks) in enumerate(s.groups):
            braces = bool(declared[k])
            rest = list(labels)
            for n, lt in enumerate(lab_toks):
                # one Java `case a, b:` / `default:` label: its own-line comments before it, its trailing comment after its last C++ label
                colon = self.label_colon(lt)
                count = self.label_count(lt, colon)
                mine, rest = rest[:count], rest[count:]
                lines += self.leading(lt, depth + 1)
                for li, lab in enumerate(mine):
                    text = f'{ind}\tdefault:' if lab is None else f'{ind}\tcase {self.case_label(lab, subj.ct)}:'
                    if li == len(mine) - 1:
                        if braces and n == len(lab_toks) - 1:
                            text += ' {'
                        text += self.trailing(colon)
                    lines.append(text)
            if rest:
                self.fail('syntax', f'{len(rest)} case labels not matched to their tokens', s.tok)
            if braces:
                self.r.idioms['case body braced (declarations)'] += 1
            self.local_scope.append({})
            inner = depth + 2
            lines += self.stmts(body, inner)
            self.local_scope.pop()
            last = body[-1] if body else None
            if body and k + 1 < len(s.groups) and not self.ends_flow(last):
                lines.append('\t' * inner + '[[fallthrough]]; // Java: no break')
                self.r.idioms['case fall-through kept'] += 1
            if braces:
                lines.append(f'{ind}\t}}')
        lines += self.leading(s.last, depth + 1)
        lines.append(ind + '}' + self.trailing(s.last))
        return lines

    def label_colon(self, lab_tok):
        """the ':' that ends the `case ...:` / `default:` label whose keyword is token lab_tok"""
        T = self.cu.tokens
        j, pending = lab_tok + 1, 0
        while j < len(T.text):
            t = T.text[j]
            if t == '(':
                j = T.match[j]
            elif t == '?':
                pending += 1
            elif t == ':':
                if pending == 0:
                    return j
                pending -= 1
            j += 1
        self.fail('syntax', 'a case label without its colon', lab_tok)

    def label_count(self, lab_tok, colon):
        """how many labels the Java label at lab_tok holds: 1, or 1 + its top-level commas (`case 1, 2:`)"""
        T = self.cu.tokens
        if T.text[lab_tok] == 'default':
            return 1
        n, j = 1, lab_tok + 1
        while j < colon:
            if T.text[j] == '(':
                j = T.match[j]
            elif T.text[j] == ',':
                n += 1
            j += 1
        return n

    def ends_flow(self, st):
        """Java's `cannot complete normally`, for the statements a case group ends with"""
        if isinstance(st, (jast.Return, jast.Break, jast.Continue)):
            return True
        if isinstance(st, jast.Block):
            return bool(st.stmts) and self.ends_flow(st.stmts[-1])
        if isinstance(st, jast.If):
            return st.else_ is not None and self.ends_flow(st.then) and self.ends_flow(st.else_)
        if isinstance(st, jast.Switch):
            if not st.groups or not any(None in labels for labels, _b, _t in st.groups) or not st.groups[-1][1]:
                return False
            if any(self.has_break(s) for _l, body, _t in st.groups for s in body):
                return False
            return self.ends_flow(st.groups[-1][1][-1])
        return False

    def has_break(self, st):
        """a break that leaves the enclosing switch (not one of a nested loop or switch)"""
        if isinstance(st, jast.Break):
            return True
        if isinstance(st, jast.Block):
            return any(self.has_break(s) for s in st.stmts)
        if isinstance(st, jast.If):
            return self.has_break(st.then) or (st.else_ is not None and self.has_break(st.else_))
        return False

    def case_label(self, lab, ct):
        if ct.kind == 'enum':
            if isinstance(lab, jast.Name):
                self.need(ct.name)
                return f'{self.api.cpp_name(ct.name)}::{lab.name}'
            e = self.expr(lab)
            return e.text
        e = self.expr(lab)
        if e.ct.kind not in ('prim', 'enum'):
            self.fail('type', f'case label {e.ct}', lab.tok)
        return e.text

    # =================================================================================================================================
    # expressions
    # =================================================================================================================================
    def lookup_var(self, name):
        for sc in reversed(self.local_scope):
            if name in sc:
                return sc[name]
        return self.members.get(name)

    def expr(self, x, stmt=False):
        e = self._expr(x)
        if e.ct.kind == 'unknown':
            raise Cascade()
        return e

    def paren(self, e, maxprec):
        return e.text if e.prec <= maxprec else f'({e.text})'

    def postfix(self, e):
        return self.paren(e, 0)

    def _expr(self, x):
        if isinstance(x, jast.Lit):
            return self.literal(x)
        if isinstance(x, jast.Name):
            return self.name(x)
        if isinstance(x, jast.Paren):
            inner = self._expr(x.expr)
            return E(f'({inner.text})', inner.ct, 0)
        if isinstance(x, jast.FieldAccess):
            return self.field_access(x)
        if isinstance(x, jast.Call):
            return self.call(x)
        if isinstance(x, jast.New):
            return self.new(x)
        if isinstance(x, jast.NewArray):
            return self.new_array(x)
        if isinstance(x, jast.ArrayInit):
            self.fail('type', 'bare array initializer', x.tok)
        if isinstance(x, jast.Index):
            a = self.expr(x.target)
            if a.ct.kind != 'array':
                self.fail('type', f'index on {a.ct}', x.tok)
            i = self.expr(x.index)
            if isinstance(x.index, jast.Lit) and x.index.kind == 'int' and x.index.text.isdigit() and 0 <= int(x.index.text) < a.ct.size:
                return E(f'{self.postfix(a)}[{i.text}]', CT('prim', a.ct.name), 0, True)
            # Java throws ArrayIndexOutOfBoundsException where std::array::operator[] is undefined: at() throws too
            self.r.idioms['array index not provably in range: at()'] += 1
            return E(f'{self.postfix(a)}.at({self.unbox(i)})', CT('prim', a.ct.name), 0, True)
        if isinstance(x, jast.Cast):
            return self.cast(x)
        if isinstance(x, jast.Unary):
            return self.unary(x)
        if isinstance(x, jast.Binary):
            return self.binary(x)
        if isinstance(x, jast.Cond):
            ce = self.expr(x.cond)
            a = self.expr(x.a)
            b = self.expr(x.b)
            ct = a.ct if a.ct.kind != 'null' else b.ct
            if a.ct.kind == 'obj' and b.ct.kind == 'obj' and a.ct.ref != b.ct.ref:
                self.fail('type', f'conditional of {a.ct} and {b.ct}', x.tok)
            if a.ct.kind == 'prim' and b.ct.kind == 'prim' and a.ct.name != b.ct.name:
                ct = self.promote(a.ct, b.ct)
            self.r.idioms['conditional expression'] += 1
            return E(f'{self.paren(ce, 11)} ? {self.paren(E(self.convert(a, ct), ct, a.prec), 12)} : '
                     f'{self.paren(E(self.convert(b, ct), ct, b.prec), 13)}', ct, 13)
        if isinstance(x, jast.Assign):
            return self.assign(x)
        if isinstance(x, jast.InstanceOf):
            if x.binding:
                if getattr(x, 'hoisted', False):
                    return E(f'{cpp_ident(x.binding)} != nullptr', BOOL, 6)
                self.fail('instanceof-pattern', 'pattern binding', x.tok)
            e = self.expr(x.expr)
            ct = self.java_type(x.type, x.tok)
            if ct.kind != 'obj' or e.ct.kind != 'obj':
                self.fail('type', f'instanceof {x.type} on {e.ct}', x.tok)
            self.need(ct.name)
            self.r.idioms['instanceof as runtime::as'] += 1
            return E(f'runtime::as<{self.api.cpp_name(ct.name)}>({e.text}) != nullptr', BOOL, 6)
        self.fail('expression', type(x).__name__, x.tok)

    def literal(self, x):
        k = x.kind
        t = x.text.replace('_', '') if k in ('int', 'long', 'float', 'double') else x.text
        if k == 'int':
            return E(t, INT)       # decimal, hex, octal and binary literals are spelled alike in C++
        if k == 'long':
            return E(t[:-1] + 'LL', LONG)
        if k == 'float':
            t = t[:-1] if t[-1] in 'fF' else t
            if not t.lower().startswith('0x') and '.' not in t and 'e' not in t.lower():
                t += '.0'           # Java 901f is C++ 901.0f (901f does not lex)
            return E(t + 'f', FLOAT)
        if k == 'double':
            t = t[:-1] if t[-1] in 'dD' else t
            if '.' not in t and 'e' not in t.lower():
                t += '.0'
            return E(t, DOUBLE)
        if k == 'bool':
            return E(t, BOOL)
        if k == 'null':
            return E('nullptr', NULL)
        if k == 'string':
            if not x.text.isascii():
                self.fail('string-literal', 'non-ASCII string literal', x.tok)
            return E(x.text, STRING)
        if k == 'char':
            return E('u' + x.text, CT('prim', 'char16_t'))
        self.fail('expression', f'literal {k}', x.tok)

    def name(self, x):
        v = self.lookup_var(x.name)
        if v is not None:
            if v.ct.kind == 'unknown':
                raise Cascade()
            v.used = True
            return E(v.cpp, v.ct, 0, v.kind in ('local', 'param'))
        if x.name == 'this':
            return E('*this', CT('obj', self.td.name, 'lref'), 1)
        if x.name in self.api.dialog_actions:
            self.record_api('DialogAction', '*', 'core')
            return E(self.api.dialog_actions[x.name], INT)
        if x.name in ('workItems', 'actionItems'):
            self.fail('api-missing', f'AbstractQuestHandler.{x.name} (a protected field)', x.tok)
        if x.name in apimod.ENUMS or self.api.is_enum(x.name) or x.name in apimod.STATIC_CLASSES or x.name in self.ix.classes \
                or x.name in self.api.prelude_names or x.name[:1].isupper():
            return E(x.name, CT('class', x.name))
        self.fail('unknown-name', x.name, x.tok)

    def field_access(self, x):
        t = self.expr(x.target)
        if t.ct.kind == 'class':
            cls = apimod.NESTED.get(t.ct.name, t.ct.name)
            if cls == 'DialogAction':
                if x.name in self.api.dialog_actions:
                    self.record_api('DialogAction', '*', 'core')
                    return E(self.api.dialog_actions[x.name], INT)
            if self.api.is_enum(cls):
                enum = self.ix.enums[cls]
                if x.name not in enum.constants:
                    self.fail('api-missing', f'{cls}.{x.name}', x.tok)
                self.need(cls)
                return E(f'{self.api.cpp_name(cls)}::{x.name}', CT('enum', cls))
            if (cls + '.' + x.name) in apimod.NESTED:
                return E(apimod.NESTED[cls + '.' + x.name], CT('class', apimod.NESTED[cls + '.' + x.name]))
            self.fail('api-missing', f'{cls}.{x.name}', x.tok)
        if t.ct.kind == 'array' and x.name == 'length':
            self.r.idioms['array length'] += 1
            return E(f'static_cast<int32_t>({self.postfix(t)}.size())', INT)
        self.fail('field-access', f'{t.ct}.{x.name}', x.tok)

    # -- calls ------------------------------------------------------------------------------------------------------------------------
    def call(self, x):
        name = x.name
        cname = cpp_ident(name)
        tgt = x.target
        if tgt is None or (isinstance(tgt, jast.Name) and tgt.name == 'this'):
            if name in self.own and not any(len(s.params) == len(x.args) for s in self.own[name]) \
                    and self.ix.owner_of('AbstractQuestHandler', cname):
                # Java merges overloads across the hierarchy; a C++ member of the same name hides the base's, so qualify
                self.r.idioms['base overload qualified (hidden by a handler method)'] += 1
                return self.member_call('AbstractQuestHandler', None, x, cname, implicit=True, qualifier='AbstractQuestHandler::')
            if name in self.own:
                sig = self.pick_own(self.own[name], x)
                args = [self.convert(self.expr(a), p) for a, p in zip(x.args, sig.params)]
                prefix = '' if tgt is None else 'this->'
                return E(f'{prefix}{sig.cpp}({", ".join(args)})', sig.ret)
            if name in self.static_imports and self.ix.owner_of('AbstractQuestHandler', cname) is None:
                self.r.idioms['static import spelled Class::member'] += 1
                return self.static_call(self.static_imports[name], x, cname)
            return self.member_call('AbstractQuestHandler', None, x, cname, implicit=True)
        if isinstance(tgt, jast.Name) and tgt.name == 'super':
            return self.member_call('AbstractQuestHandler', None, x, cname, implicit=True, qualifier='AbstractQuestHandler::')
        t = self.expr(tgt)
        if t.ct.kind == 'class':
            return self.static_call(t.ct.name, x, cname)
        if t.ct.kind == 'enum':
            return self.enum_call(t, x, cname)
        if t.ct.kind == 'obj':
            if name == 'equals' and len(x.args) == 1:
                return self.equals(t, x)
            return self.member_call(t.ct.name, t, x, cname)
        if t.ct.kind == 'optional' and name == 'intValue':
            return E(self.postfix(t) + '.value()', t.ct.elem)
        if t.ct.kind == 'vector':
            self.r.idioms['java.util.List methods on std::vector'] += 1
            if name == 'isEmpty' and not x.args:
                return E(self.postfix(t) + '.empty()', BOOL)
            if name == 'size' and not x.args:
                return E(f'static_cast<int32_t>({self.postfix(t)}.size())', INT)
            if name == 'get' and len(x.args) == 1:
                return E(f'{self.postfix(t)}.at({self.unbox(self.expr(x.args[0]))})', t.ct.elem)
            if name in ('getFirst', 'getLast') and not x.args:
                return E(f'{self.postfix(t)}.{"front" if name == "getFirst" else "back"}()', t.ct.elem)
            v = self.lookup_var(tgt.name) if isinstance(tgt, jast.Name) else None
            if name in LIST_MUTATORS and v is not None and ' is const ' in v.origin:
                # the C++ hook hands the list over read-only (onBonusApplyEvent: AbstractQuestHandler.h:144-145) where Java lets the
                # handler add to it: no API row can close that, only a changed hook signature (a header request)
                self.fail('header-signature', f'{v.origin}; Java List.{name} mutates it', x.tok)
        if t.ct.kind == 'string' and name == 'equals' and len(x.args) == 1:
            a = self.expr(x.args[0])
            return E(f'{self.paren(t, 5)} == {self.paren(a, 5)}', BOOL, 6)
        self.fail('api-missing', f'{t.ct}.{name}', x.tok)

    def pick_own(self, sigs, x):
        fits = [s for s in sigs if len(s.params) == len(x.args)]
        if len(fits) != 1:
            self.fail('overload', f'own method {x.name}/{len(x.args)}: {len(fits)} candidates', x.tok)
        return fits[0]

    def equals(self, t, x):
        a = self.expr(x.args[0])
        if t.ct.name == 'ZoneName' and a.ct.kind == 'obj' and a.ct.name == 'ZoneName':
            self.record_api('ZoneName', 'equals', None)
            self.r.idioms['ZoneName.equals as identity'] += 1
            return E(f'{self.paren(t, 5)} == {self.paren(a, 5)}', BOOL, 6)
        self.fail('api-missing', f'{t.ct.name}.equals', x.tok)

    def record_api(self, owner, name, row):
        key = f'{owner}.{name}'
        self.r.apis[key] += 1
        if isinstance(row, apimod.Row):
            self.r.api_rows.add(row.id)
            self.r.api_tier[key] = row.id
        else:
            self.r.api_tier[key] = 'core'

    def member_call(self, cls, recv, x, cname, implicit=False, qualifier=''):
        owner = self.ix.owner_of(cls, cname)
        if owner is None:
            planned = apimod.PLANNED.get((cls, x.name))
            tier = self.api.tier(cls, x.name)
            if planned and tier:
                return self.planned_call(cls, x, planned, recv)
            self.fail('api-missing' if tier is None else 'api-undeclared', f'{cls}.{x.name}', x.tok)
        tier = self.api.tier(owner, cname) or self.api.tier(cls, cname)
        if tier is None:
            self.fail('api-missing', f'{owner}.{x.name}', x.tok)
        funcs = self.ix.lookup(cls, cname)
        f, args = self.resolve(funcs, x, f'{owner}.{x.name}')
        self.record_api(owner, x.name, tier)
        self.track_status(owner, x.name, f, len(x.args))
        self.need(owner)
        if f.static and recv is not None:
            text = f'{self.api.cpp_name(owner)}::{cname}({", ".join(args)})'
        elif recv is None:
            text = f'{qualifier}{cname}({", ".join(args)})'
        else:
            self.need(cls)
            arrow = '->' if recv.ct.ref in ('ptr', 'owning', 'raw') else '.'
            text = f'{self.postfix(recv)}{arrow}{cname}({", ".join(args)})'
        return E(text, self.ret_type(f))

    def ret_type(self, f):
        ct = self.cpp_type(f.ret or 'void')
        return ct

    def static_call(self, cls, x, cname):
        cls = apimod.NESTED.get(cls, cls)
        planned = apimod.PLANNED.get((cls, x.name))
        if planned and self.api.tier(cls, x.name):
            return self.planned_call(cls, x, planned, None)
        if cls in apimod.STATIC_NAMESPACES:
            return self.namespace_call(cls, x, cname)
        c = self.ix.classes.get(cls)
        if c is None:
            if self.api.is_enum(cls):
                return self.enum_static(cls, x, cname)
            self.fail('api-missing', f'{cls}.{x.name}', x.tok)
        funcs = c.methods.get(cname, [])
        if not funcs:
            planned = apimod.PLANNED.get((cls, x.name))
            if planned and self.api.tier(cls, x.name):
                return self.planned_call(cls, x, planned, None)
            if self.api.is_enum(cls):
                return self.enum_static(cls, x, cname)
            self.fail('api-missing' if self.api.tier(cls, x.name) is None else 'api-undeclared', f'{cls}.{x.name}', x.tok)
        tier = self.api.tier(cls, cname)
        if tier is None:
            self.fail('api-missing', f'{cls}.{x.name}', x.tok)
        if not any(f.static for f in funcs):
            self.fail('api-missing', f'{cls}.{x.name} (not static in C++)', x.tok)
        f, args = self.resolve([f for f in funcs if f.static], x, f'{cls}.{x.name}')
        label = x.name if cls != 'SM_SYSTEM_MESSAGE' else 'STR_*'
        self.record_api(cls, label, tier)
        self.track_status(cls, label, f, len(x.args))
        self.need(cls)
        return E(f'{self.api.cpp_name(cls)}::{cname}({", ".join(args)})', self.ret_type(f))

    def namespace_call(self, cls, x, cname):
        """a Java static method of a class C++ ports as a namespace of free functions (Rnd)"""
        tier = self.api.tier(cls, x.name)
        if tier is None:
            self.fail('api-missing', f'{cls}.{x.name}', x.tok)
        ns = apimod.STATIC_NAMESPACES[cls]
        cands = [f for f in self.ix.free.get(cname, []) if tuple(f.ns) == ns]
        if not cands:
            self.fail('api-undeclared', f'{cls}.{x.name}', x.tok)
        f, args = self.resolve(cands, x, f'{cls}.{x.name}')
        self.record_api(cls, x.name, tier)
        self.track_status(cls, x.name, f, len(x.args))
        self.includes_for_header(f.header)
        return E(f'::{"::".join(ns)}::{cname}({", ".join(args)})', self.ret_type(f))

    def enum_call(self, t, x, cname):
        """Java enum instance method -> its companion free function, first argument the enum value"""
        enum = t.ct.name
        tier = self.api.tier(enum, x.name)
        if tier is None:
            self.fail('api-missing', f'{enum}.{x.name}', x.tok)
        cands = [f for f in self.ix.free.get(cname, []) if f.params and self.simple(f.params[0].type) == enum]
        if not cands:
            self.fail('api-undeclared', f'{enum}.{x.name}', x.tok)
        f = cands[0]
        fake = jast.Call(x.tok, None, x.name, x.args)
        _, args = self.resolve([cppdecl_strip_first(f)], fake, f'{enum}.{x.name}')
        self.record_api(enum, x.name, tier)
        self.track_status(enum, x.name, f, len(x.args) + 1)
        self.includes_for_header(f.header)
        return E(f'::{"::".join(f.ns)}::{cname}({", ".join([t.text] + args)})', self.ret_type(f))

    def enum_static(self, enum, x, cname):
        tier = self.api.tier(enum, x.name)
        if tier is None:
            self.fail('api-missing', f'{enum}.{x.name}', x.tok)
        cands = [f for f in self.ix.free.get(cname, []) if not (f.params and self.simple(f.params[0].type) == enum)]
        cands = [f for f in cands if self.ix.enums.get(enum) and tuple(f.ns) == tuple(self.ix.enums[enum].qual[:-1])] or cands
        if not cands:
            self.fail('api-undeclared', f'{enum}.{x.name}', x.tok)
        f, args = self.resolve(cands, x, f'{enum}.{x.name}')
        self.record_api(enum, x.name, tier)
        self.track_status(enum, x.name, f, len(x.args))
        self.includes_for_header(f.header)
        return E(f'::{"::".join(f.ns)}::{cname}({", ".join(args)})', self.ret_type(f))

    def includes_for_header(self, h):
        if self.api.needs_include(h):
            self.includes.add(h)

    def planned_call(self, cls, x, planned, recv):
        args = [self.expr(a) for a in x.args]
        self.record_api(cls, x.name, self.api.tier(cls, x.name))
        self.r.undeclared.add(f'{cls}.{x.name}')
        self.includes.add(planned.header)
        text = planned.cpp.replace('{args}', ', '.join(self.unbox(a) for a in args))
        return E(text, self.cpp_type(planned.ret))

    def track_status(self, owner, name, f, arity):
        key = f'{owner}.{name}'
        self.r.api_status.setdefault(key, set()).add(self.api.body_status(f, arity))

    # -- overloads --------------------------------------------------------------------------------------------------------------------
    PURE = re.compile(r'(get|is|has|can|contains|size|equals|id|valueOf|isEmpty|chance|nextBoolean)([A-Z_]|$)')

    def side_effecting(self, x):
        """a Java expression that calls something other than a getter, or assigns"""
        for n in jast.walk_exprs(x):
            if isinstance(n, jast.Call) and not self.PURE.match(n.name):
                return True
            if isinstance(n, jast.Assign) or (isinstance(n, jast.Unary) and n.op in ('++', '--')):
                return True
        return False

    def check_order(self, exprs, x, what):
        """Java evaluates arguments and operands left to right; C++ leaves the order of function arguments (and of most operands)
        unspecified. Two side-effecting ones are a parity hazard: recorded, not refused (none of the 1,035 files is known to rely on it)."""
        if sum(1 for e in exprs if self.side_effecting(e)) >= 2:
            line, _ = self.cu.tokens.loc(x.tok)
            self.r.hazards.append(f'{what} with two side-effecting operands @ line {line}')

    def resolve(self, funcs, x, key):
        """pick the overload a Java call would bind to; returns (Func, converted argument texts)"""
        self.check_order(x.args, x, f'arguments of {key}')
        args = [self.expr(a) for a in x.args]
        best = []
        for f in funcs:
            if f.template and not (f.owner == 'PacketSendUtility'):
                continue
            r = self.applicable(f, args, x)
            if r is None:
                continue
            best.append((r[0], f, r[1]))
        if not best:
            sig = ', '.join(str(a.ct) for a in args)
            if args and args[-1].ct.kind == 'array' and any(f.params and self.cpp_type(f.params[-1].type).kind == 'ilist' for f in funcs):
                self.fail('varargs-array', f'{key}: an int[] passed to Java varargs; the C++ parameter is std::initializer_list', x.tok)
            self.fail('overload', f'{key}({sig}): no C++ overload fits', x.tok)
        # a higher score wins; on a tie a non-template overload beats a template (C++ rules)
        best.sort(key=lambda b: (-b[0], b[1].template))
        if len(best) > 1 and best[0][0] == best[1][0] and best[0][1].template == best[1][1].template \
                and best[0][1].signature() != best[1][1].signature():
            self.fail('overload', f'{key}: ambiguous between {best[0][1].signature()} and {best[1][1].signature()}', x.tok)
        f, plan = best[0][1], best[0][2]
        texts = []
        emitted = []
        for item in plan:
            if item[0] == 'ilist':
                texts.append('{' + ', '.join(self.convert(a, item[2]) for a in item[1]) + '}')
                emitted.append(('ilist', None))
                self.r.idioms['varargs as a braced list'] += 1
            else:
                t = self.convert(item[1], item[2])
                texts.append(t)
                emitted.append(self.emitted_type(item[1], item[2], t))
        texts = self.pin_cpp_overload(f, funcs, emitted, texts, x, key)
        return f, texts

    # -- the C++ side of overload resolution ------------------------------------------------------------------------------------------
    def emitted_type(self, e, p, text):
        """the C++ type category of an argument as emitted: ('lvalue', class), ('ptr', class), ('prim', name), ..."""
        k = e.ct.kind
        if k == 'null':
            return ('nullopt', '') if text == 'std::nullopt' else ('nullptr', '')
        if k == 'optional' and text.endswith('.value()'):
            return ('prim', e.ct.elem.name)
        if k == 'obj':
            if text.startswith('*'):
                return ('lvalue', e.ct.name)
            if e.ct.ref in ('lref', 'clref'):
                return ('lvalue', e.ct.name)
            return (e.ct.ref, e.ct.name)          # ptr owning raw value
        if k == 'string':
            return ('strlit', '') if text.startswith('"') else ('string', '')
        if k in ('prim', 'enum', 'array', 'optional', 'vector'):
            return (k, e.ct.name if k != 'optional' else str(e.ct))
        return (k, e.ct.name)

    PROMOTIONS = {('int8_t', 'int32_t'), ('int16_t', 'int32_t'), ('char16_t', 'int32_t'), ('bool', 'int32_t'), ('uint8_t', 'int32_t'),
                  ('uint16_t', 'int32_t'), ('float', 'double')}

    def cpp_rank(self, a, p):
        """C++ implicit conversion rank of an emitted argument to a parameter: 0 exact, 1 promotion, 2 conversion, 3 user-defined, None
        when not viable"""
        kind, name = a
        if kind == 'ilist':
            return 0 if p is not None and p.kind == 'ilist' else None
        if p is None or p.kind == 'any':
            return 0
        pk = p.kind
        if kind == 'nullptr':
            if pk == 'obj' and p.ref in ('ptr', 'owning'):
                return 3
            if pk == 'obj' and p.ref == 'raw':
                return 2
            if pk == 'prim' and p.name == 'bool':
                return 2
            return None
        if kind == 'nullopt':
            return 3 if pk == 'optional' else None
        if kind == 'prim':
            if pk == 'prim':
                if name == p.name:
                    return 0
                if (name, p.name) in self.PROMOTIONS:
                    return 1
                return 2
            if pk == 'optional' and p.elem is not None and p.elem.kind == 'prim':
                return 3
            return None
        if kind == 'enum':
            return 0 if pk == 'enum' and p.name == name else None
        if kind == 'strlit':
            if pk == 'string':
                return 3
            if pk == 'prim' and p.name == 'bool':
                return 2            # const char* -> bool beats const char* -> std::string_view
            return None
        if kind == 'string':
            return 0 if pk == 'string' else None
        if kind in ('lvalue', 'value'):
            if pk == 'obj' and p.ref in ('lref', 'clref'):
                if kind == 'value' and p.ref == 'lref':
                    return None
                return 0 if name == p.name else (2 if self.ix.is_subclass(name, p.name) else None)
            if pk == 'obj' and p.ref == 'ptr' and kind == 'lvalue':
                return 3 if self.ix.is_subclass(name, p.name) else None
            if pk == 'obj' and p.ref == 'value':
                return 3 if self.ix.is_subclass(name, p.name) else None
            return None
        if kind in ('ptr', 'owning'):
            if pk == 'obj' and p.ref == 'ptr':
                return (0 if kind == 'ptr' and name == p.name else 3) if self.ix.is_subclass(name, p.name) else None
            return None
        if kind == 'raw':
            if pk == 'obj' and p.ref == 'raw':
                return 0 if name == p.name else (2 if self.ix.is_subclass(name, p.name) else None)
            if pk == 'prim' and p.name == 'bool':
                return 2
            return None
        if kind == 'array':
            return 3 if pk == 'array' and p.name == name else None
        if kind == 'optional':
            return 0 if pk == 'optional' else None
        return None

    def pin_cpp_overload(self, f, funcs, emitted, texts, x, key):
        """C++ ranks the overloads its own way (an int converts to bool, a string literal to bool before string_view). If another
        overload would beat or tie the one Java binds, cast the primitive arguments to the exact parameter types; refuse if that does
        not settle it."""
        n = len(emitted)
        if any(k == 'ilist' for k, _ in emitted) or any(self.cpp_type(p.type).kind == 'ilist' for p in f.params):
            return texts            # Java varargs: the initializer_list overload is the only one of its name

        def ranks(g, em):
            ps = [self.cpp_type(p.type) for p in g.params]
            if any(p.kind == 'ilist' for p in ps):
                return None
            pack = bool(g.params) and g.params[-1].type.endswith('...')
            if pack:
                ps = ps[:-1]
            lo = sum(1 for p in g.params if p.default is None and not p.type.endswith('...'))
            if n < lo or (n > len(ps) and not pack):
                return None
            out = []
            for i, a in enumerate(em):
                r = self.cpp_rank(a, ps[i] if i < len(ps) else None)
                if r is None:
                    return None
                out.append(r)
            return out

        def conflicts(em):
            rf = ranks(f, em)
            if rf is None:
                return ['the chosen overload']
            bad = []
            for g in funcs:
                if g is f or (g.template and f.template):
                    continue
                rg = ranks(g, em)
                if rg is None:
                    continue
                f_better = all(a <= b for a, b in zip(rf, rg)) and any(a < b for a, b in zip(rf, rg))
                if g.template and not f.template and all(a <= b for a, b in zip(rf, rg)):
                    f_better = True     # a non-template wins a tie against a template
                if not f_better:
                    bad.append(g.signature())
            return bad

        bad = conflicts(emitted)
        if not bad:
            return texts
        ps = [self.cpp_type(p.type) for p in f.params]
        new_texts = list(texts)
        new_em = list(emitted)
        for i, a in enumerate(emitted):
            if i < len(ps) and a[0] == 'prim' and ps[i].kind == 'prim' and a[1] != ps[i].name:
                new_texts[i] = f'static_cast<{ps[i].name}>({texts[i]})'
                new_em[i] = ('prim', ps[i].name)
            elif i < len(ps) and a[0] == 'strlit' and ps[i].kind == 'string':
                new_texts[i] = f'std::string_view({texts[i]})'
                new_em[i] = ('string', '')
        still = conflicts(new_em)
        if still:
            self.fail('cpp-overload', f'{key}: C++ would not bind {f.signature()} (also viable: {still[0]})', x.tok)
        self.r.idioms['argument cast to pin the Java overload in C++'] += 1
        return new_texts

    def applicable(self, f, args, x):
        """(score, plan) when the call fits f; plan items are ('arg', E, param CT) or ('ilist', [E], element CT)"""
        ps = [self.cpp_type(p.type) for p in f.params]
        n = len(args)
        plan = []
        score = 0
        if ps and ps[-1].kind == 'ilist':
            fixed = ps[:-1]
            if n < len(fixed):
                return None
            elem = CT('prim', ps[-1].name)
            for a, p in zip(args, fixed):
                s = self.fit(a, p)
                if s is None:
                    return None
                score += s
                plan.append(('arg', a, p))
            rest = args[len(fixed):]
            if len(rest) == 1 and rest[0].ct.kind == 'array':
                return None
            for a in rest:
                s = self.fit(a, elem)
                if s is None:
                    return None
                score += s
            if rest or f.params[-1].default is None:
                plan.append(('ilist', rest, elem))
            return score, plan
        pack = bool(f.params) and f.params[-1].type.endswith('...')
        if pack:
            ps = ps[:-1]
        lo = sum(1 for p in f.params if p.default is None and not p.type.endswith('...'))
        if not (lo <= n and (pack or n <= len(ps))):
            return None
        for a, p in zip(args, ps):
            s = self.fit(a, p)
            if s is None:
                return None
            score += s
            plan.append(('arg', a, p))
        for a in args[len(ps):]:
            if a.ct.kind not in ('prim', 'string', 'enum'):
                return None
            score += 1
            plan.append(('arg', a, None))
        return score, plan

    def fit(self, a, p):
        """how well an argument of type a fits parameter type p (Java method invocation conversions): 3 exact, 2 widening or
        subclass, 1 unboxing; None if it does not"""
        ak, pk = a.ct.kind, p.kind
        if pk == 'any':
            return 1 if ak == 'obj' else None
        if ak == 'null':
            if pk == 'obj' and p.ref in ('ptr', 'owning', 'raw'):
                return 2
            if pk in ('optional', 'string'):
                return 2 if pk == 'optional' else None
            return None
        if pk == 'prim':
            if ak == 'optional' and a.ct.elem.kind == 'prim':
                return 1 if self.fit(E('', a.ct.elem), p) else None
            if ak != 'prim':
                return None
            if a.ct.name == p.name:
                return 3
            if 'bool' in (a.ct.name, p.name):
                return None
            ra, rp = RANK.get(a.ct.name), RANK.get(p.name)
            if ra is None or rp is None:
                return None
            if ra < rp:
                return 2
            return None
        if pk == 'optional':
            if ak == 'optional':
                return 3
            if ak == 'prim' and a.ct.name == p.elem.name:
                return 2
            return None
        if pk == 'enum':
            return 3 if ak == 'enum' and a.ct.name == p.name else None
        if pk == 'string':
            return 3 if ak == 'string' else None
        if pk == 'array':
            return 3 if ak == 'array' and a.ct.name == p.name else None
        if pk == 'obj':
            if ak != 'obj':
                return None
            if not self.ix.is_subclass(a.ct.name, p.name):
                return None
            if p.ref == 'lref' and a.ct.ref == 'value':
                return None                 # a temporary does not bind to T&
            if p.ref == 'raw' and a.ct.ref != 'raw':
                return None
            if a.ct.ref == 'raw' and p.ref != 'raw':
                return None
            return 3 if a.ct.name == p.name else 2
        return None

    def convert(self, e, p):
        """e's text for a slot of type p (a parameter, a return value, an initializer)"""
        ek = e.ct.kind
        if p is None:
            return e.text
        if ek == 'null':
            if p.kind == 'optional':
                return 'std::nullopt'
            return 'nullptr'
        if p.kind == 'obj' and ek == 'obj':
            if p.ref in ('lref', 'clref') and e.ct.ref in ('ptr', 'owning', 'raw'):
                self.r.idioms['pointer dereferenced for a T& parameter'] += 1
                return '*' + self.paren(e, 1)
            return e.text
        if p.kind == 'prim' and ek == 'optional':
            self.r.idioms['Integer unboxed with value()'] += 1
            return self.postfix(e) + '.value()'
        if p.kind == 'prim' and p.name == 'bool' and ek == 'obj':
            self.fail('type', f'{e.ct} used as boolean')
        return e.text

    def unbox(self, e):
        if e.ct.kind == 'optional':
            self.r.idioms['Integer unboxed with value()'] += 1
            return self.postfix(e) + '.value()'
        return e.text

    # -- new, casts, operators --------------------------------------------------------------------------------------------------------
    def new(self, x):
        cls = x.type.name
        if cls == 'QuestEnv':
            funcs = self.ix.classes['QuestEnv'].methods.get('create', [])
            f, args = self.resolve(funcs, x, 'QuestEnv.create')
            self.record_api('QuestEnv', '<init>', 'core')
            self.track_status('QuestEnv', '<init>', f, len(x.args))
            self.r.idioms['new QuestEnv as QuestEnv::create'] += 1
            return E(f'QuestEnv::create({", ".join(args)})', CT('obj', 'QuestEnv', 'owning'))
        c = self.ix.classes.get(cls)
        tier = self.api.tier(cls, '<init>')
        if c is None or tier is None:
            self.fail('api-missing', f'new {cls}', x.tok)
        funcs = [f for f in c.methods.get(cls, []) if not f.static]
        f, args = self.resolve(funcs, x, f'new {cls}')
        self.record_api(cls, '<init>', tier)
        self.track_status(cls, '<init>', f, len(x.args))
        self.need(cls)
        return E(f'{self.api.cpp_name(cls)}({", ".join(args)})', CT('obj', cls, 'value'))

    def new_array(self, x):
        if x.type.dims or x.type.name not in JAVA_PRIM:
            self.fail('type', f'new {x.type}[]', x.tok)
        el = JAVA_PRIM[x.type.name]
        items = [self.convert(self.expr(i), CT('prim', el)) for i in x.init.items]
        self.std_includes.add('array')
        self.r.idioms['new int[] as std::array'] += 1
        return E(f'std::array<{el}, {len(items)}>{{{", ".join(items)}}}', CT('array', el, size=len(items)))

    def cast(self, x):
        e = self.expr(x.expr)
        ty = x.type
        if ty.dims == 0 and ty.name in JAVA_PRIM:
            target = CT('prim', JAVA_PRIM[ty.name])
            if e.ct.kind == 'optional':
                e = E(self.postfix(e) + '.value()', e.ct.elem)
            if e.ct.kind != 'prim':
                self.fail('type', f'({ty}) of {e.ct}', x.tok)
            self.r.idioms['primitive cast as static_cast'] += 1
            return E(f'static_cast<{target.name}>({e.text})', target)
        ct = self.java_type(ty, x.tok)
        if ct.kind == 'obj' and e.ct.kind == 'obj':
            self.need(ct.name)
            self.r.idioms['class cast as runtime::cast'] += 1
            return E(f'runtime::cast<{self.api.cpp_name(ct.name)}>({e.text})', CT('obj', ct.name, 'ptr'))
        self.fail('type', f'({ty}) of {e.ct}', x.tok)

    def unary(self, x):
        e = self.expr(x.expr)
        op = x.op
        if op in ('++', '--'):
            if not e.lvalue:
                self.fail('assignment-target', f'{op} on {x.expr.__class__.__name__}', x.tok)
            return E(f'{e.text}{op}' if x.postfix else f'{op}{e.text}', e.ct, 0 if x.postfix else 1)
        if op == '!':
            return E('!' + self.paren(E(self.convert(e, BOOL), e.ct, e.prec), 1), BOOL, 1)
        if e.ct.kind == 'optional':
            e = E(self.postfix(e) + '.value()', e.ct.elem)
        if e.ct.kind != 'prim':
            self.fail('type', f'{op} on {e.ct}', x.tok)
        return E(op + self.paren(e, 1), e.ct if RANK.get(e.ct.name, 3) >= 3 else INT, 1)

    def binary(self, x):
        op = x.op
        if op not in ('&&', '||'):
            self.check_order([x.left, x.right], x, f'operator {op}')
        a = self.expr(x.left)
        b = self.expr(x.right)
        lvl = BIN_PREC[op]
        if op in ('==', '!='):
            return self.equality(x, a, b, op, lvl)
        if op in ('&&', '||'):
            return E(f'{self.paren(E(self.convert(a, BOOL), BOOL, a.prec), lvl)} {op} {self.paren(E(self.convert(b, BOOL), BOOL, b.prec), lvl - 1)}',
                     BOOL, lvl)
        if op == '+' and (a.ct.kind == 'string' or b.ct.kind == 'string'):
            self.fail('string-concat', 'String concatenation', x.tok)
        a = self.unboxed(a)
        b = self.unboxed(b)
        if a.ct.kind != 'prim' or b.ct.kind != 'prim':
            self.fail('type', f'{a.ct} {op} {b.ct}', x.tok)
        if op in ('<', '>', '<=', '>='):
            ct = BOOL
        elif op in ('&', '|', '^') and a.ct.name == 'bool':
            ct = BOOL
        else:
            ct = self.promote(a.ct, b.ct)
            if op in ('+', '-', '*', '/', '%'):
                self.r.idioms['arithmetic'] += 1
        return E(f'{self.paren(a, lvl)} {op} {self.paren(b, lvl - 1)}', ct, lvl)

    def unboxed(self, e):
        if e.ct.kind == 'optional':
            self.r.idioms['Integer unboxed with value()'] += 1
            return E(self.postfix(e) + '.value()', e.ct.elem)
        return e

    @staticmethod
    def promote(a, b):
        ra, rb = RANK.get(a.name, 3), RANK.get(b.name, 3)
        top = max(ra, rb, 3)
        for n, r in RANK.items():
            if r == top and n in ('int32_t', 'int64_t', 'float', 'double'):
                return CT('prim', n)
        return INT

    def equality(self, x, a, b, op, lvl):
        ak, bk = a.ct.kind, b.ct.kind
        if 'null' in (ak, bk):
            o = b if ak == 'null' else a
            if o.ct.kind == 'obj' and o.ct.ref in ('ptr', 'owning', 'raw'):
                return E(f'{self.paren(o, lvl)} {op} nullptr', BOOL, lvl)
            if o.ct.kind == 'optional':
                return E(f'{self.paren(o, lvl)} {op} std::nullopt', BOOL, lvl)
            self.fail('type', f'{o.ct} compared with null', x.tok)
        if ak == 'obj' and bk == 'obj':
            if a.ct.ref in ('lref', 'clref', 'value') or b.ct.ref in ('lref', 'clref', 'value'):
                self.fail('type', f'identity of {a.ct} and {b.ct}', x.tok)
            self.r.idioms['object == as identity'] += 1
            return E(f'{self.paren(a, lvl)} {op} {self.paren(b, lvl - 1)}', BOOL, lvl)
        if ak == 'enum' and bk == 'enum':
            return E(f'{self.paren(a, lvl)} {op} {self.paren(b, lvl - 1)}', BOOL, lvl)
        a = self.unboxed(a)
        b = self.unboxed(b)
        if a.ct.kind == 'prim' and b.ct.kind == 'prim':
            return E(f'{self.paren(a, lvl)} {op} {self.paren(b, lvl - 1)}', BOOL, lvl)
        self.fail('type', f'{a.ct} {op} {b.ct}', x.tok)

    def assign(self, x):
        t = self.expr(x.target)
        if not t.lvalue:
            self.fail('assignment-target', f'assignment to {type(x.target).__name__}', x.tok)
        v = self.expr(x.value)
        if t.ct.kind == 'obj' and t.ct.ref in ('lref', 'clref'):
            self.fail('assignment-target', f'assignment to the reference {t.text} (a Java parameter reassigned)', x.tok)
        if x.op == '=':
            if t.ct.kind == 'obj' and v.ct.kind == 'obj' and t.ct.ref == 'ptr' and v.ct.ref == 'owning':
                self.fail('type', 'a new object stored in a Ptr local', x.tok)
            return E(f'{t.text} = {self.convert(v, t.ct)}', t.ct, 13)
        v = self.unboxed(v)
        if t.ct.kind == 'prim' and v.ct.kind == 'prim' and RANK.get(v.ct.name, 0) > RANK.get(t.ct.name, 9):
            # Java's compound assignment narrows implicitly: x op= v is x = (T) (x op v)
            self.r.idioms['narrowing compound assignment made explicit'] += 1
            op = x.op[:-1]
            return E(f'{t.text} = static_cast<{t.ct.name}>({t.text} {op} {self.paren(v, BIN_PREC.get(op, 12) - 1)})', t.ct, 13)
        return E(f'{t.text} {x.op} {self.paren(v, 12)}', t.ct, 13)

    # =================================================================================================================================
    # comments
    # =================================================================================================================================
    def comments_in(self, text):
        return re.findall(r'//[^\n]*|/\*[\s\S]*?\*/', text)

    def leading(self, tok, depth):
        """the comments on the lines between token tok-1 and token tok (the part after the first newline of the gap)"""
        g = self.p.gap(tok)
        nl = g.find('\n')
        if nl < 0:
            return []
        out = []
        rest = g[nl:]
        first = re.search(r'//|/\*', rest)
        head = rest if first is None else rest[:first.start()]
        if re.search(r'\n[ \t\r]*\n', head) and tok > 0 and self.cu.tokens.text[tok - 1] not in ('{', ':') \
                and self.cu.tokens.text[tok] != '}':
            out.append('')          # keep the Java blank line between statements
        for c in self.comments_in(rest):
            if c.startswith('/**') and tok in self.cu.tokens.docs:
                pass
            for ln in c.split('\n'):
                out.append('\t' * depth + ln.strip() if not ln.strip().startswith('*') else '\t' * depth + ' ' + ln.strip())
        return out

    def trailing(self, tok):
        """a comment on the same line after token tok"""
        T = self.cu.tokens
        if tok + 1 >= len(T.text):
            return ''
        g = self.p.gap(tok + 1)
        nl = g.find('\n')
        head = g if nl < 0 else g[:nl]
        cs = self.comments_in(head)
        return (' ' + ' '.join(c.strip() for c in cs)) if cs else ''

    # =================================================================================================================================
    # the file
    # =================================================================================================================================
    def render(self, td, quest_id, out_methods):
        ns = skeleton.package_namespace(self.cu.package, 'handlers')
        rel_java = f'game-server/data/handlers/quest/{self.r.rel}'
        head = ['#include "aion/gameserver/handlers/quest/QuestPrelude.h"', '']
        std = sorted(self.std_includes)
        if std:
            head += [f'#include <{s}>' for s in std] + ['']
        inc = sorted(self.includes)
        if inc:
            head += [f'#include "{h}"' for h in inc] + ['']
        body = [f'namespace {"::".join(ns)} {{', '',
                f'// Generated by cpp/tools/gen/questgen (prototype) from {rel_java}. Not compiled.', '']
        doc = self.cu.tokens.docs.get(self.class_first_token(td))
        if doc:
            body += [ln.rstrip().replace('\t', '') if not ln.lstrip().startswith('*') else ' ' + ln.strip() for ln in doc.split('\n')]
        body.append(f'class {td.name} final : public AbstractQuestHandler {{')
        if self.consts:
            body.append('private:')
            for name, decl, tok, _ in self.consts:
                body += self.leading(self.first_token_of_field(tok), 1)
                body.append('\t' + decl)
            body.append('')
        body.append('public:')
        body.append(f'\t{td.name}() : AbstractQuestHandler({self.ctor_const or quest_id}) {{}}')
        cur = 'public'
        for m, (lines, access, pre) in out_methods:
            body.append('')
            if access != cur:
                body.append(access + ':')
                cur = access
            body += ['\t' + ln if ln else '' for ln in pre + lines]
        body.append('};')
        body.append(f'AION_QUEST_HANDLER({td.name}, {quest_id});')
        body += ['', f'}} // namespace {"::".join(ns)}', '']
        return '\n'.join(head + body)

    def class_first_token(self, td):
        i = td.index
        T = self.cu.tokens
        while i > 0 and T.text[i - 1] not in (';', '}', '{'):
            i -= 1
        return i

    def first_token_of_field(self, tok):
        T = self.cu.tokens
        i = tok
        while i > 0 and T.text[i - 1] not in (';', '}', '{'):
            i -= 1
        return i


def cppdecl_strip_first(f):
    """a copy of a companion Func without its first (enum) parameter"""
    from dataclasses import replace
    return replace(f, params=f.params[1:])
