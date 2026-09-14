"""cppmodel: maps the JAXB model (jaxb.py) to C++ (docs/design/static-data.md §2 and amendments).

Decides per class whether it is data-only (fully generated struct) or has behaviour (hand-written class that includes the generated
member block), names every C++ entity, maps member types (§2.4), finds trivial Java accessors, polymorphic hierarchies, hook owners,
required checks, @XmlElements factories and the includes each generated file needs.

C++ naming
- Namespaces mirror packages (com.aionemu.gameserver.a.b -> aion::gameserver::a::b), types keep their Java simple names.
- Nested enums and nested data-only classes are emitted at namespace scope as Outer_Inner (EnumTraits specializations must live at
  namespace scope, and data structs need no access to the outer class); the outer class gets `using Inner = Outer_Inner;`.
- Nested classes with behaviour stay nested (Outer::Inner) inside the hand-written outer class, which is therefore a behaviour class too.
- Identifiers that are C++ keywords (or macros of the platform headers, for enum constants) get a trailing underscore.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field

import jaxb
from jaxb import XmlGenError, short_fqn

CPP_KEYWORDS = set('''alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept
const constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit
export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private
protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template
this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq'''.split())
# Object-like macros of <windows.h>/<cmath>/<cerrno>/<cstdio>/<climits> that WindowsMacroGuard.h does not remove (it keeps TRUE, FALSE,
# NULL, CONST, VOID, ...): an enum constant with such a name cannot be spelled in a TU that includes those headers.
RESERVED_MACROS = set('''TRUE FALSE NULL CONST VOID STRICT CALLBACK WINAPI PASCAL APIENTRY EOF BUFSIZ DOMAIN SING OVERFLOW UNDERFLOW TLOSS
PLOSS EDOM ERANGE EILSEQ EINVAL ENOMEM NAN INFINITY HUGE_VAL CHAR_BIT CHAR_MIN CHAR_MAX INT_MIN INT_MAX SIZE_MAX errno assert stdin stdout
stderr'''.split())

INT_TYPES = {'byte': 'int8_t', 'short': 'int16_t', 'int': 'int32_t', 'long': 'int64_t'}
PRIMITIVE_CPP = {'boolean': 'bool', 'byte': 'int8_t', 'short': 'int16_t', 'int': 'int32_t', 'long': 'int64_t', 'float': 'float',
                 'double': 'double'}
RUNTIME_FIELD = '::aion::gameserver::runtime::Field'
FWD_HEADER = 'aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h'
FIELD_HEADER = 'aion/gameserver/runtime/fields/Field.h'
# Marker base of the K1 static data classes (runtime/lifetime/RefCounted.h): every hierarchy root derives it (data structs and hand-written
# behaviour shells), so `const T*` template pointers are IsTemplatePtr/Pinnable for the task capture rules (runtime-architecture.md §7.3)
STATIC_TEMPLATE = '::aion::gameserver::runtime::StaticTemplate'
STATIC_TEMPLATE_HEADER = 'aion/gameserver/runtime/lifetime/RefCounted.h'


def cpp_ident(name):
    return name + '_' if name in CPP_KEYWORDS else name


def enum_constant_ident(name):
    return name + '_' if name in CPP_KEYWORDS or name in RESERVED_MACROS else name


def namespace_of(package):
    if package == 'com.aionemu.gameserver':
        return 'aion::gameserver'
    if not package.startswith('com.aionemu.gameserver.'):
        raise XmlGenError(f'package {package} is outside com.aionemu.gameserver')
    return 'aion::gameserver::' + '::'.join(cpp_ident(p) for p in package[len('com.aionemu.gameserver.'):].split('.'))


def package_dir(package):
    return 'aion/gameserver/' + '/'.join(package[len('com.aionemu.gameserver.'):].split('.')) if package != 'com.aionemu.gameserver' \
        else 'aion/gameserver'


def fnv1a64(name):
    h = 0xcbf29ce484222325
    for b in name.encode('utf-8'):
        h ^= b
        h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def cpp_string_literal(value):
    out = ['"']
    for ch in value:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\t':
            out.append('\\t')
        elif 32 <= o < 127:
            out.append(ch)
        else:
            raise XmlGenError(f'non-ASCII character in string literal {value!r}')
    out.append('"')
    return ''.join(out)


# ----------------------------------------------------------------------------------------------------------------------------------
# Model
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass
class CEnum:
    model: object  # jaxb.EnumModel
    fqn: str
    java_name: str
    namespace: str
    cpp_name: str
    header: str  # include path
    constants: list  # [(java name, cpp name, xml lexical)]
    underlying: str
    outer_alias: tuple | None = None  # (outer fqn, alias name, access)

    @property
    def qualified(self):
        return f'::{self.namespace}::{self.cpp_name}'


@dataclass
class CMember:
    prop: object  # jaxb.Property
    name: str
    type: str
    init: str  # '' or the brace/equal initializer text (without '=')
    access: str  # public | protected | private
    bind: str  # binding category (see emit.bind_statement)
    comment: str
    reserve: bool = False  # in-place std::vector<Object>, reserved in XmlBinding<T>::reserve
    elem_qualified: str = ''  # element/object type (qualified) for lists, singles, choices, idrefs
    optional: bool = False
    scalar_cpp: str = ''  # scalar value type (for runtime_mutable Field<T> and method setters)
    factory: str = ''  # factory name of choices
    adapter: dict | None = None
    return_kind: str = ''  # value | cref | pointer | field
    java_field: str = ''  # Java field name (members that stand for a Java field)


@dataclass
class CAccessor:
    java: object  # MethodDecl
    kind: str  # getter | setter
    member: CMember
    access: str
    code: str = ''


@dataclass
class CStatic:
    name: str
    code: str  # member declaration inside the struct
    definition: str = ''  # namespace-scope definition after the struct ('' if none)


@dataclass
class CFactory:
    name: str
    base_qualified: str
    entries: list  # [(xml name, derived qualified)] sorted by byte order
    owner: object  # CClass


@dataclass
class CClass:
    model: object  # jaxb.ClassModel
    fqn: str
    java_name: str
    package: str
    namespace: str
    cpp_name: str  # flattened (Outer_Inner) for nested data classes, simple name otherwise
    qualified: str
    kind: str = 'data'  # data | behaviour
    reasons: list = field(default_factory=list)
    outer: object = None  # CClass of the Java outer class (nested classes)
    nested_in_cpp: bool = False  # true nesting (behaviour inside behaviour)
    header: str = ''  # include path of the header that defines the class (generated or hand-written)
    inc: str = ''  # member block include path (behaviour)
    prelude: str = ''  # generated declarations header of the member block (behaviour)
    members: list = field(default_factory=list)
    accessors: list = field(default_factory=list)
    statics: list = field(default_factory=list)
    aliases: list = field(default_factory=list)  # [(alias, qualified, access)]
    superclass: object = None  # CClass
    polymorphic_root: bool = False
    polymorphic: bool = False  # derives from a polymorphic root (or is one)
    hook_owner: object = None  # CClass declaring the nearest afterUnmarshal
    create: bool = False
    required_attributes: list = field(default_factory=list)
    required_elements: list = field(default_factory=list)
    factories: list = field(default_factory=list)
    non_trivial_methods: list = field(default_factory=list)  # MethodDecls without generated C++ (port checklist)
    includes: set = field(default_factory=set)  # headers the declarations need
    forward: set = field(default_factory=set)  # qualified names that only need a declaration
    has_attribute: bool = False
    has_element: bool = False
    has_reserve: bool = False
    has_finish: bool = False
    container: bool = False  # unbound outer class of bound nested classes (no binding)

    @property
    def is_data(self):
        return self.kind == 'data'


class CppModel:
    def __init__(self, model):
        self.model = model
        self.policy = model.policy
        self.classes = {}
        self.enums = {}
        self.errors = []
        self.optional_boxed_with_initializer = []  # report: boxed fields that became plain values
        self.string_literal_defaults = []
        self.renamed_members = []  # report: members renamed because a method or nested type has their name
        self.abstract_choices = []  # report: @XmlElements entries of abstract classes (not in the factories)

    def error(self, node, message):
        return XmlGenError(f'{jaxb.loc(node)}: {message}')

    # -- names ---------------------------------------------------------------------------------------------------------------------------
    def build(self):
        m = self.model
        for fqn, e in sorted(m.enums.items()):
            self.enums[fqn] = self.name_enum(e)
        for fqn, c in sorted(m.classes.items()):
            td = c.td
            package = td.cu.package
            self.classes[fqn] = CClass(c, fqn, td.name, package, namespace_of(package), td.name, '')
        for cc in list(self.classes.values()):
            if cc.model.superclass:
                cc.superclass = self.classes[cc.model.superclass]
            outer = cc.model.td.outer
            if outer is not None:
                if outer.fqn not in self.classes:
                    self.add_container(outer)
                cc.outer = self.classes[outer.fqn]
        self.classify()
        self.name_classes()
        for cc in sorted(self.classes.values(), key=lambda c: c.fqn):
            try:
                self.map_members(cc)
            except XmlGenError as e:
                self.errors.append(str(e))
        if self.errors:
            raise XmlGenError('\n'.join(sorted(set(self.errors))))
        for cc in self.ordered_classes():
            self.map_accessors(cc)
            self.finish_class(cc)
        return self

    def add_container(self, td):
        """An unbound class that only encloses bound nested classes (FeedGroups): a generated struct with aliases."""
        if td.outer is not None or td.kind != 'class' or td.extends or td.implements or td.initializers                 or any('static' not in f.modifiers for f in td.fields) or any(m.kind != 'constructor' for m in td.methods):
            raise self.error(td, f'the unbound outer class {td.fqn} of bound nested classes has members; bind it or move the nested classes')
        cm = jaxb.ClassModel(td.fqn, td, 'NONE', False, None, None, None, False, None)
        cc = CClass(cm, td.fqn, td.name, td.cu.package, namespace_of(td.cu.package), td.name, '')
        cc.container = True
        self.classes[td.fqn] = cc
        for f in td.fields:
            if self.static_code(cc, f) is None:
                raise self.error(f, f'static field {f.name} of the container class {td.fqn}')

    def name_enum(self, e):
        if e.external:
            table = self.policy.external_enums[e.fqn]
            namespace = table['namespace']
            cpp_name = e.fqn.rpartition('.')[2]
            header = table.get('header') or namespace.replace('::', '/') + f'/{cpp_name}.h'
            outer_alias = None
            java_name = cpp_name
        else:
            td = e.td
            namespace = namespace_of(td.cu.package)
            chain = []
            t = td
            while t is not None:
                chain.append(t)
                t = t.outer
            chain.reverse()
            cpp_name = '_'.join(t.name for t in chain)
            header = f'{package_dir(td.cu.package)}/{cpp_name}.h'
            outer_alias = (td.outer.fqn, td.name, _access(td.modifiers)) if td.outer is not None else None
            java_name = td.name
        constants = []
        seen = set()
        for java, xml in e.constants:
            ident = enum_constant_ident(java)
            if ident in seen:
                raise XmlGenError(f'{e.location}: enum constant {ident} collides after escaping')
            seen.add(ident)
            constants.append((java, ident, xml))
        underlying = 'uint8_t' if len(constants) <= 256 else 'uint16_t'
        return CEnum(e, e.fqn, java_name, namespace, cpp_name, header, constants, underlying, outer_alias)

    def name_classes(self):
        for cc in self.classes.values():
            chain = []
            c = cc
            while c is not None:
                chain.append(c)
                c = c.outer
            chain.reverse()
            if cc.outer is None:
                cc.cpp_name = cc.java_name
                cc.qualified = f'::{cc.namespace}::{cc.cpp_name}'
                cc.header = f'{package_dir(cc.package)}/{cc.cpp_name}.h'
            elif cc.is_data:
                cc.cpp_name = '_'.join(c.java_name for c in chain)
                cc.qualified = f'::{cc.namespace}::{cc.cpp_name}'
                cc.header = f'{package_dir(cc.package)}/{cc.cpp_name}.h'
            else:
                cc.nested_in_cpp = True
                cc.cpp_name = cc.java_name
        for cc in self.classes.values():
            if cc.nested_in_cpp:
                cc.qualified = f'::{cc.namespace}::' + '::'.join(self._chain_names(cc))
                top = cc
                while top.outer is not None:
                    top = top.outer
                cc.header = top.header
        for cc in self.classes.values():
            if not cc.is_data:
                flat = '_'.join(self._chain_names(cc))
                cc.inc = f'{package_dir(cc.package)}/{flat}.xml.inc'
                cc.prelude = f'{package_dir(cc.package)}/{flat}.xml.h'

    @staticmethod
    def _chain_names(cc):
        chain = []
        c = cc
        while c is not None:
            chain.append(c.java_name)
            c = c.outer
        return list(reversed(chain))

    # -- classification ------------------------------------------------------------------------------------------------------------------
    def classify(self):
        for cc in self.classes.values():
            cc.reasons = self.behaviour_reasons(cc)
        changed = True
        while changed:  # a behaviour class nested in a data class makes the outer class a behaviour class
            changed = False
            for cc in self.classes.values():
                if cc.reasons and cc.outer is not None and not cc.outer.reasons:
                    cc.outer.reasons.append(f'contains the nested behaviour class {cc.java_name}')
                    changed = True
                # a nested class deriving from a nested behaviour class of the same outer class must stay nested as well (its
                # header would otherwise include the outer header that includes it)
                sup = cc.superclass
                if not cc.reasons and cc.outer is not None and sup is not None and sup.reasons and sup.outer is not None \
                        and self.top_level(sup) is self.top_level(cc):
                    cc.reasons.append(f'derives from the nested behaviour class {sup.java_name}')
                    changed = True
        for cc in self.classes.values():
            cc.kind = 'behaviour' if cc.reasons else 'data'
        # polymorphism
        for cc in self.classes.values():
            if cc.model.choice_base:
                root = cc
                c = cc.superclass
                while c is not None:
                    if c.model.choice_base:
                        root = c
                    c = c.superclass
                root.polymorphic_root = True
        for cc in self.classes.values():
            c = cc
            while c is not None:
                if c.polymorphic_root:
                    cc.polymorphic = True
                    break
                c = c.superclass
        for cc in self.classes.values():
            if cc.polymorphic_root and cc.is_data:
                raise self.error(cc.model.td, 'a polymorphic root must be a behaviour class')

    @staticmethod
    def top_level(cc):
        while cc.outer is not None:
            cc = cc.outer
        return cc

    def behaviour_reasons(self, cc):
        c = cc.model
        td = c.td
        reasons = []
        forced = self.policy.lookup('force_behaviour', c.fqn)
        if forced is not None:
            reasons.append(f'xmlgen.toml force_behaviour: {forced["reason"]}')
        if c.hook is not None:
            reasons.append('afterUnmarshal hook')
        if c.before_unmarshal is not None:
            reasons.append('beforeUnmarshal hook')
        if c.abstract:
            reasons.append('abstract')
        if c.choice_base:
            reasons.append('@XmlElements base class')
        if td.implements:
            reasons.append('implements ' + ', '.join(str(t) for t in td.implements))
        if c.no_arg_constructor not in ('implicit', 'public'):
            reasons.append(f'{c.no_arg_constructor} no-argument constructor')
        if any(p.source == 'method' for p in c.properties):
            reasons.append('annotated accessor methods')
        if c.denied:
            reasons.append('fields excluded by deny_implicit')
        fields = self.generated_fields(c, True)
        for f in td.fields:
            if 'static' in f.modifiers:
                if self.static_code(cc, f) is None:
                    reasons.append(f'static field {f.name}')
                continue
            if f.name not in fields:
                reasons.append(f'unbound field {f.name}')
        if td.initializers:
            reasons.append('initializer block')
        for t in td.types:
            if t.fqn not in self.model.classes and t.fqn not in self.model.enums:
                reasons.append(f'unbound nested type {t.name}')
        for m in td.methods:
            if m.kind == 'constructor':
                if m.params or (m.body is not None and len(m.body.texts()) > 2):
                    reasons.append('constructor with parameters or a body')
                continue
            if m.name in ('afterUnmarshal', 'beforeUnmarshal'):
                continue
            if self.trivial_kind(c, m, fields) is None:
                reasons.append(f'method {m.name}')
        # dedupe, keep order
        out = []
        for r in reasons:
            if r not in out:
                out.append(r)
        return out

    def generated_fields(self, c, data):
        """{java field name: FieldDecl} of the instance fields that get generated members: bound fields, [runtime_mutable] fields and,
        for data-only classes, the mappable unbound fields"""
        out = {p.java_name: p.decl for p in c.properties if p.source == 'field'}
        for f in c.mutable_fields:
            out[f.name] = f
        if data:
            for f in c.td.fields:
                if 'static' not in f.modifiers and f.name not in out and self.transient_member_type(f) is not None:
                    out[f.name] = f
        return out

    def trivial_kind(self, c, m, fields=None):
        """('getter'|'setter', java field name) for trivial accessors of fields with generated members, else None"""
        if fields is None:
            fields = self.generated_fields(c, False)
        if m.kind != 'method' or 'static' in m.modifiers or m.type_params or m.body is None or 'abstract' in m.modifiers \
                or 'synchronized' in m.modifiers:
            return None
        if m.annotations:
            return None
        t = m.body.texts()
        if not m.params and m.return_type is not None and str(m.return_type) != 'void':
            name = None
            if len(t) == 5 and t[0] == '{' and t[1] == 'return' and t[3] == ';' and t[4] == '}':
                name = t[2]
            elif len(t) == 7 and t[:4] == ['{', 'return', 'this', '.'] and t[5:] == [';', '}']:
                name = t[4]
            f = fields.get(name)
            if f is not None and str(m.return_type) == str(f.type):
                return 'getter', name
            return None
        if len(m.params) == 1 and m.return_type is not None and str(m.return_type) == 'void':
            param = m.params[0].name
            name = None
            if len(t) == 8 and t[:3] == ['{', 'this', '.'] and t[4] == '=' and t[5] == param and t[6:] == [';', '}']:
                name = t[3]
            elif len(t) == 6 and t[0] == '{' and t[2] == '=' and t[3] == param and t[4:] == [';', '}'] and t[1] != param:
                name = t[1]
            f = fields.get(name)
            if f is not None and str(m.params[0].type) == str(f.type) and not m.params[0].varargs:
                return 'setter', name
        return None

    def transient_member_type(self, f):
        """C++ type and initializer of an unbound instance field of a data class, or None if it needs hand-written code"""
        try:
            jt = self.model.resolve(f.type, f, f)
        except XmlGenError:
            return None
        if jt.kind not in ('primitive', 'boxed', 'string', 'enum'):
            return None
        if f.initializer is not None:
            return None
        return jt

    def static_code(self, cc, f):
        """CStatic for a static final constant of a data class, or None"""
        if 'final' not in f.modifiers or f.initializer is None:
            return None
        toks = f.initializer.texts()
        kinds = f.initializer.cu.tokens.kind[f.initializer.start:f.initializer.end]
        tname = str(f.type)
        name = cpp_ident(f.name)
        if tname in PRIMITIVE_CPP and len(toks) in (1, 2):
            neg = len(toks) == 2 and toks[0] == '-'
            if (len(toks) == 1 or neg) and kinds[-1] in (jaxb.javasrc.INT, jaxb.javasrc.FLOAT) or toks in (['true'], ['false']):
                value = toks[-1] if toks[-1] in ('true', 'false') else literal_cpp(tname, kinds[-1], toks[-1], neg)
                return CStatic(name, f'static constexpr {PRIMITIVE_CPP[tname]} {name} = {value};')
            return None
        if tname == 'String' and len(toks) == 1 and kinds[0] == jaxb.javasrc.STRING:
            return CStatic(name, f'static constexpr std::string_view {name} = '
                                 f'{cpp_string_literal(jaxb.javasrc.literal_value(kinds[0], toks[0]))};')
        if toks[:1] == ['new'] and toks[-2:] == ['(', ')'] and ''.join(toks[1:-2]) == cc.java_name == tname:
            return CStatic(name, f'static const {cc.java_name} {name};', f'inline const {cc.java_name} {cc.java_name}::{name}{{}};')
        return None

    # -- members -------------------------------------------------------------------------------------------------------------------------
    def scalar(self, jt):
        if jt.kind in ('primitive', 'boxed'):
            return PRIMITIVE_CPP[jt.fqn if jt.kind == 'primitive' else jaxb.BOXED[jt.fqn]]
        if jt.kind == 'string':
            return 'std::string'
        if jt.kind == 'enum':
            return self.enums[jt.fqn].qualified
        raise XmlGenError(f'not a scalar: {jt}')

    def class_ref(self, fqn, node):
        cc = self.classes.get(fqn)
        if cc is None:
            raise self.error(node, f'{fqn} is not a bound class')
        return cc

    def map_members(self, cc):
        c = cc.model
        access_default = 'public' if cc.is_data else None
        for p in c.properties:
            if p.source == 'method':
                self.map_method_property(cc, p)
                continue
            member = self.map_property(cc, p)
            if access_default:
                member.access = access_default
            cc.members.append(member)
        for f in c.mutable_fields:
            cc.members.append(self.map_mutable_field(cc, f))
        if cc.is_data:
            bound = {p.java_name for p in c.properties} | {f.name for f in c.mutable_fields}
            for f in c.td.fields:
                if 'static' in f.modifiers:
                    cc.statics.append(self.static_code(cc, f))
                    continue
                if f.name in bound:
                    continue
                jt = self.transient_member_type(f)
                ctype = self.scalar(jt)
                init = ''
                rk = 'cref'
                if jt.kind == 'primitive':
                    init = default_value(jt.fqn)
                    rk = 'value'
                elif jt.kind in ('boxed', 'enum'):
                    ctype = f'std::optional<{ctype}>'
                mem = CMember(None, cpp_ident(f.name), ctype, init, 'public', 'none', f'unbound field {f.name}', return_kind=rk)
                mem.java_field = f.name
                cc.members.append(mem)
                self.add_type_includes(cc, jt)
        # C++ cannot have a data member and a member function (or nested type) of the same name; Java can. Java fields are accessed by
        # name inside their class only, so the member is renamed (Java methods keep their names for callers).
        taken = set()
        x = cc
        while x is not None:
            taken |= {m.name for m in x.model.td.methods}
            taken |= {t.name for t in x.model.td.types}
            x = x.superclass
        for mem in cc.members:
            if mem.name and mem.name in taken:
                self.renamed_members.append(f'{short_fqn(cc.fqn)}.{mem.name} -> {mem.name}_')
                mem.name += '_'
        names = {}
        for mem in cc.members:
            if not mem.name:
                continue
            if mem.name in names:
                raise self.error(c.td, f'duplicate member name {mem.name}')
            if mem.name == cc.cpp_name or mem.name == cc.java_name:
                raise self.error(c.td, f'member {mem.name} has the name of its class')
            names[mem.name] = mem

    def map_mutable_field(self, cc, f):
        """an unbound field listed in [runtime_mutable]: mutable Field<T> for primitives, enums and template references"""
        jt = self.model.resolve(f.type, f, f)
        init = ''
        toks = f.initializer.texts() if f.initializer is not None else []
        if jt.kind == 'primitive':
            scalar = PRIMITIVE_CPP[jt.fqn]
            if toks in (['true'], ['false']):
                init = toks[0]
            elif toks:
                kinds = f.initializer.cu.tokens.kind[f.initializer.start:f.initializer.end]
                neg = toks[0] == '-'
                if len(toks) != (2 if neg else 1) or kinds[-1] not in (jaxb.javasrc.INT, jaxb.javasrc.FLOAT):
                    raise self.error(f, f'[runtime_mutable] initializer {f.initializer.text} is not a literal')
                init = literal_cpp(jt.fqn, kinds[-1], toks[-1], neg)
            else:
                init = default_value(jt.fqn)
        elif jt.kind == 'class' and jt.fqn in self.classes:
            target = self.classes[jt.fqn]
            scalar = f'const {target.qualified}*'
            cc.forward.add(target.fqn)
            if toks not in ([], ['null']):
                raise self.error(f, f'[runtime_mutable] initializer {f.initializer.text} of a template reference')
            init = 'nullptr'
        elif jt.kind == 'enum':
            scalar = self.scalar(jt)
            self.add_type_includes(cc, jt)
            if toks:
                raise self.error(f, '[runtime_mutable] enum initializers are not supported')
            init = f'{scalar}{{}}'
        else:
            raise self.error(f, f'[runtime_mutable] field of type {jt} is not supported')
        cc.includes.add(FIELD_HEADER)
        access = {'package': 'public'}.get(jaxb._access(f.modifiers), jaxb._access(f.modifiers))
        mem = CMember(None, cpp_ident(f.name), f'mutable {RUNTIME_FIELD}<{scalar}>', init, 'public' if cc.is_data else access, 'none',
                      f'runtime_mutable (unbound field {f.name})', scalar_cpp=scalar, return_kind='field')
        mem.java_field = f.name
        return mem

    def map_method_property(self, cc, p):
        if cc.is_data:
            raise self.error(p.decl, 'annotated accessor methods need a behaviour class')
        if p.node != 'attribute' or p.collection or not p.value_type.scalar:
            raise self.error(p.decl, 'annotated accessor methods are supported for scalar attributes only')
        cc.members.append(CMember(p, '', '', '', 'private', 'methodSetter', f'@XmlAttribute {p.setter}(...)',
                                  scalar_cpp=self.scalar(p.value_type)))
        self.add_type_includes(cc, p.value_type)

    def add_type_includes(self, cc, jt):
        if jt.kind == 'enum':
            cc.includes.add(self.enums[jt.fqn].header)

    def initializer_cpp(self, p, ctype_scalar):
        init = p.initializer
        jt = p.java_type
        if init.kind == 'none' or init.kind == 'null':
            return None
        if init.kind == 'config':
            return init.cpp
        if init.kind == 'literal':
            if init.token_kind == 'boolean':
                return 'true' if init.value else 'false'
            base = jt.fqn if jt.kind == 'primitive' else jaxb.BOXED.get(jt.fqn)
            if init.token_kind == jaxb.javasrc.STRING:
                if jt.kind != 'string':
                    raise self.error(p.decl, f'string initializer for {jt}')
                return cpp_string_literal(init.value)
            if base is None:
                raise self.error(p.decl, f'numeric initializer for {jt}')
            text = init.text.strip()
            neg = text.startswith('-')
            return literal_cpp(base, init.token_kind, text.lstrip('-').strip(), neg)
        if init.kind == 'enum':
            e = self.enums[init.enum_fqn]
            ident = next(cpp for java, cpp, _ in e.constants if java == init.value)
            return f'{e.qualified}::{ident}'
        if init.kind == 'empty_collection':
            return ''
        raise self.error(p.decl, f'initializer `{init.text}` needs xmlgen.toml [initializers] {short_fqn(p.key)} = {{ cpp = "...", reason = "..." }}')

    def map_property(self, cc, p):
        jt, vt = p.java_type, p.value_type
        name = cpp_ident(p.java_name)
        access = {'package': 'public'}.get(p.access, p.access)
        comment = annotation_comment(p)
        m = CMember(p, name, '', '', access, '', comment)
        m.java_field = p.java_name
        if p.runtime_mutable and (p.collection or not vt.scalar or p.idref or p.adapter or vt.kind == 'string'):
            raise self.error(p.decl, '[runtime_mutable] supports non-collection primitive and enum properties only')
        if p.adapter:
            table = self.policy.adapters[p.adapter]
            if p.collection and p.java_type.kind != 'array':
                raise self.error(p.decl, 'adapter on a collection')
            cc.includes.add(table['header'])
            m.adapter = table
            init = self.initializer_cpp(p, None)
            if init is None:
                m.type = f'std::optional<{table["cpp"]}>'
                m.optional = True
            else:
                m.type = table['cpp']
                m.init = init
            m.bind = 'adapterAttribute' if p.node == 'attribute' else 'adapterText'
            m.return_kind = 'cref'
            return m
        if p.class_adapter:
            table = self.policy.adapters[p.class_adapter]
            if p.collection or p.node != 'element':
                raise self.error(p.decl, 'class-level adapter on a collection or attribute')
            cc.includes.add(table['header'])
            value_cc = self.class_ref(jaxb.long_fqn(table['value_type']), p.decl)
            m.adapter = table
            m.type = table['cpp']
            m.elem_qualified = value_cc.qualified
            m.bind = 'classAdapter'
            m.return_kind = 'pointer' if table['cpp'].startswith('std::unique_ptr<') else 'cref'
            cc.includes.add(value_cc.header)
            self.no_initializer(p)
            return m
        if p.idref:
            target = self.class_ref(vt.fqn, p.decl)
            m.elem_qualified = target.qualified
            cc.forward.add(target.fqn)
            if p.collection:
                m.type = f'std::vector<const {target.qualified}*>'
                m.bind = 'idRefList' if p.node == 'attribute' else 'idRefText'
                if p.node == 'attribute' and not p.xml_list:
                    m.bind = 'idRefList'
                m.return_kind = 'cref'
            else:
                m.type = f'const {target.qualified}*'
                m.init = 'nullptr'
                m.bind = 'idRef' if p.node == 'attribute' else 'idRefText'
                m.return_kind = 'value'
            self.no_initializer(p)
            return m
        if p.node == 'choice':
            base = self.class_ref(vt.fqn, p.decl) if vt.fqn in self.classes else None
            if base is None:
                raise self.error(p.decl, f'@XmlElements base {vt.fqn} is not a bound class')
            cc.includes.add(base.header)
            m.elem_qualified = base.qualified
            m.type = f'std::vector<std::unique_ptr<{base.qualified}>>' if p.collection else f'std::unique_ptr<{base.qualified}>'
            m.bind = 'choice'
            m.return_kind = 'cref' if p.collection else 'pointer'
            entries = []
            for xml, target in p.choices:
                tc = self.class_ref(target, p.decl)
                if tc.model.abstract:
                    # JAXB cannot instantiate it either (unmarshalling such an element fails), so the data never contains it
                    self.abstract_choices.append(f'{short_fqn(p.key)}: <{xml}> -> {short_fqn(target)}')
                    continue
                entries.append((xml, tc.qualified, tc))
            entries.sort(key=lambda e: e[0].encode('utf-8'))
            fname = f'{cc.cpp_name}_{p.java_name}'
            m.factory = fname
            cc.factories.append(CFactory(fname, base.qualified, entries, cc))
            self.no_initializer(p)
            return m
        if vt.scalar:
            scalar = self.scalar(vt)
            self.add_type_includes(cc, vt)
            m.scalar_cpp = scalar
            if p.collection:
                container = 'std::unordered_set' if jt.kind == 'set' else 'std::vector'
                init = self.initializer_cpp(p, None)
                if p.node == 'attribute' or p.xml_list or p.wrapper:
                    if p.wrapper and jt.kind == 'set':
                        raise self.error(p.decl, 'wrapper of a Set')
                    if init is None:
                        m.type = f'std::optional<{container}<{scalar}>>'
                        m.optional = True
                    elif init == '':
                        m.type = f'{container}<{scalar}>'
                    else:
                        m.type = f'{container}<{scalar}>'
                        m.init = init
                    m.bind = 'assignList' if p.node == 'attribute' else ('wrapper' if p.wrapper else 'bindTextList')
                else:
                    if jt.kind == 'set':
                        raise self.error(p.decl, 'element list of a Set is not supported')
                    m.type = f'std::vector<{scalar}>'
                    if init not in (None, ''):
                        m.init = init
                    m.bind = 'bindList'
                m.return_kind = 'cref'
                return m
            init = self.initializer_cpp(p, scalar)
            required_enum = vt.kind == 'enum' and p.required
            if p.runtime_mutable:
                if vt.kind not in ('primitive', 'enum') or (vt.kind == 'enum' and init is None and not p.required):
                    raise self.error(p.decl, '[runtime_mutable] needs a primitive or an initialized/required enum')
                m.type = f'mutable {RUNTIME_FIELD}<{scalar}>'
                m.init = init if init is not None else (default_value(vt.fqn) if vt.kind == 'primitive' else '')
                if init is None and vt.kind == 'enum':
                    m.init = f'{scalar}{{}}'
                cc.includes.add(FIELD_HEADER)
                m.bind = 'fieldAttribute' if p.node == 'attribute' else 'fieldText'
                m.return_kind = 'field'
                return m
            if vt.kind == 'primitive':
                m.type = scalar
                m.init = init if init is not None else default_value(vt.fqn)
                m.return_kind = 'value'
            elif vt.kind == 'boxed':
                if init is None:
                    m.type = f'std::optional<{scalar}>'
                    m.optional = True
                else:
                    m.type = scalar
                    m.init = init
                    self.optional_boxed_with_initializer.append(p.key)
                m.return_kind = 'value' if not m.optional else 'cref'
            elif vt.kind == 'string':
                if p.optional_string:
                    if init is not None:
                        raise self.error(p.decl, '[optional_strings] with an initializer')
                    m.type = 'std::optional<std::string>'
                    m.optional = True
                else:
                    m.type = 'std::string'
                    if init is not None:
                        m.init = init
                        self.string_literal_defaults.append(p.key)
                m.return_kind = 'cref'
            else:  # enum
                if init is not None:
                    m.type = scalar
                    m.init = init
                    m.return_kind = 'value'
                elif required_enum:
                    m.type = scalar
                    m.init = f'{scalar}{{}}'
                    m.return_kind = 'value'
                else:
                    m.type = f'std::optional<{scalar}>'
                    m.optional = True
                    m.return_kind = 'value'
            m.bind = 'assign' if p.node == 'attribute' else 'bindText'
            if p.xml_id:
                m.bind += 'XmlId'
            if p.lenient_enum:
                if vt.kind != 'enum' or not m.optional:
                    raise self.error(p.decl, '[lenient_enums] needs a std::optional enum member')
                m.bind += 'LenientEnum'
            return m
        # object elements
        target = self.class_ref(vt.fqn, p.decl)
        if target.model.abstract:
            raise self.error(p.decl, f'element of the abstract class {short_fqn(target.fqn)} without @XmlElements')
        m.elem_qualified = target.qualified
        cc.includes.add(target.header)
        by_pointer = p.storage_by_pointer or target.model.no_arg_constructor not in ('implicit', 'public') or self.has_runtime_mutable(target)
        if p.collection:
            if jt.kind == 'set':
                raise self.error(p.decl, 'Set of objects is not supported')
            inner = f'std::unique_ptr<{target.qualified}>' if by_pointer else target.qualified
            if p.wrapper:
                m.type = f'std::optional<std::vector<{inner}>>'
                m.optional = True
                m.bind = 'wrapper'
            else:
                m.type = f'std::vector<{inner}>'
                m.bind = 'bindList'
                m.reserve = not by_pointer
            init = self.initializer_cpp(p, None)
            if init not in (None, ''):
                raise self.error(p.decl, 'object list initializer')
            if init == '' and p.wrapper:
                m.type = f'std::vector<{inner}>'
                raise self.error(p.decl, 'initialized wrapper list is not supported')
            m.return_kind = 'cref'
            return m
        init = self.initializer_cpp(p, None)
        if init not in (None, ''):
            raise self.error(p.decl, 'object element initializer other than the default')
        m.type = f'std::unique_ptr<{target.qualified}>'
        m.bind = 'bindSingle'
        m.return_kind = 'pointer'
        return m

    def no_initializer(self, p):
        if p.initializer.kind not in ('none', 'null'):
            raise self.error(p.decl, f'initializer `{p.initializer.text}` is not supported for this property kind')

    def has_runtime_mutable(self, cc):
        c = cc
        while c is not None:
            if any(p.runtime_mutable for p in c.model.properties) or c.model.mutable_fields:
                return True
            c = c.superclass
        return False

    # -- accessors -----------------------------------------------------------------------------------------------------------------------
    def map_accessors(self, cc):
        members = {mem.java_field: mem for mem in cc.members if mem.java_field}
        fields = self.generated_fields(cc.model, cc.is_data)
        member_names = set()
        x = cc
        while x is not None:
            member_names |= {mem.name for mem in x.members if mem.name}
            member_names |= {f.name for f in x.model.td.fields}
            x = x.superclass
        for meth in cc.model.td.methods:
            if meth.kind == 'constructor' or meth.name in ('afterUnmarshal', 'beforeUnmarshal'):
                continue
            kind = self.trivial_kind(cc.model, meth, fields)
            if kind is None:
                cc.non_trivial_methods.append(meth)
                continue
            member = members[kind[1]]
            p = member.prop
            if p is not None and p.initializer.kind == 'config' and p.initializer.cpp == '':
                # the C++ default (empty/null) stands for a Java default object: the accessor substitutes it by hand
                cc.non_trivial_methods.append(meth)
                continue
            access ={'package': 'public'}.get(jaxb._access(meth.modifiers), jaxb._access(meth.modifiers))
            if cc.is_data:
                access = 'public'
            acc = CAccessor(meth, kind[0], member, access)
            acc.code = accessor_code(acc, member_names)
            if acc.code is None:
                cc.non_trivial_methods.append(meth)
                continue
            cc.accessors.append(acc)

    # -- per class -----------------------------------------------------------------------------------------------------------------------
    def finish_class(self, cc):
        c = cc.model
        # hook owner
        h = cc
        while h is not None and h.model.hook is None:
            h = h.superclass
        cc.hook_owner = h
        cc.create = c.no_arg_constructor not in ('implicit', 'public')
        # required, flattened over the class chain (base first)
        chain = []
        x = cc
        while x is not None:
            chain.append(x)
            x = x.superclass
        attrs, elems = [], []
        for x in reversed(chain):
            for p in x.model.properties:
                if not p.required or not p.enforce_required:
                    continue
                if p.node == 'attribute':
                    attrs.append(p.xml_name)
                elif p.wrapper:
                    elems.append(p.wrapper)
                elif p.node == 'element':
                    elems.append(p.xml_name)
        cc.required_attributes = attrs
        cc.required_elements = elems
        own_attr = any(mem.prop is not None and mem.prop.node == 'attribute' for mem in cc.members) or bool(cc.model.ignored_attributes)
        own_elem = any(mem.prop is not None and mem.prop.node in ('element', 'choice') for mem in cc.members)
        own_reserve = any(mem.reserve for mem in cc.members)
        base = cc.superclass
        cc.has_attribute = own_attr or (base is not None and base.has_attribute)
        cc.has_element = own_elem or (base is not None and base.has_element)
        cc.has_reserve = own_reserve or (base is not None and base.has_reserve)
        cc.has_finish = bool(attrs or elems or cc.hook_owner is not None)
        if base is not None:
            cc.includes.add(base.header)
        elif not cc.container:
            cc.includes.add(STATIC_TEMPLATE_HEADER)  # the root derives STATIC_TEMPLATE (the prelude of a behaviour class provides it)
        # aliases for nested enums and nested data classes
        for e in self.enums.values():
            if e.outer_alias and e.outer_alias[0] == cc.fqn:
                cc.aliases.append((e.java_name, e.qualified, {'package': 'public'}.get(e.outer_alias[2], e.outer_alias[2])))
                cc.includes.add(e.header)
        for n in self.classes.values():
            if n.outer is cc and not n.nested_in_cpp:
                access = jaxb._access(n.model.td.modifiers)
                cc.aliases.append((n.java_name, n.qualified, {'package': 'public'}.get(access, access)))
                cc.includes.add(n.header)
        cc.aliases.sort()
        cc.includes.discard(cc.header)

    def ordered_classes(self):
        """classes sorted by fqn, superclasses before their subclasses (has_attribute etc. need the base first)"""
        out, done = [], set()

        def visit(c):
            if c.fqn in done:
                return
            if c.superclass is not None:
                visit(c.superclass)
            done.add(c.fqn)
            out.append(c)
        for c in sorted(self.classes.values(), key=lambda c: c.fqn):
            visit(c)
        return out


def _access(modifiers):
    return jaxb._access(modifiers)


def default_value(primitive):
    """C++ spelling of the Java default value of a primitive field"""
    return {'boolean': 'false', 'float': '0.0f', 'double': '0.0'}.get(primitive, '0')


def literal_cpp(base, token_kind, text, neg):
    """C++ spelling of a Java numeric literal for a field of primitive type `base`"""
    t = text.replace('_', '')
    sign = '-' if neg else ''
    if base in ('float', 'double'):
        if token_kind == jaxb.javasrc.INT:
            t = t.rstrip('lL')
            if t.lower().startswith(('0x', '0b')) or (len(t) > 1 and t.startswith('0')):
                raise XmlGenError(f'unsupported float initializer {text}')
            t = t + '.0'
        else:
            t = t.rstrip('fFdD')
            if t.startswith('.'):
                t = '0' + t
            if t.endswith('.'):
                t = t + '0'
            if '.' not in t and 'e' not in t.lower():
                t = t + '.0'
        return f'{sign}{t}f' if base == 'float' else f'{sign}{t}'
    if base == 'boolean':
        raise XmlGenError(f'numeric initializer {text} for boolean')
    if token_kind != jaxb.javasrc.INT:
        raise XmlGenError(f'floating point initializer {text} for {base}')
    t = t.rstrip('lL')
    if len(t) > 1 and t.startswith('0') and t[1] not in 'xXbB':
        raise XmlGenError(f'octal initializer {text} is not supported')
    value = int(t, 0)
    if neg:
        value = -value
    limits = {'byte': 7, 'short': 15, 'int': 31, 'long': 63}[base]
    if not -(1 << limits) <= value < (1 << limits):
        raise XmlGenError(f'initializer {text} out of range for {base}')
    if base == 'long' and not -(1 << 31) <= value < (1 << 31):
        return f'INT64_C({value})'
    if value == -(1 << 31):
        return '(-2147483647 - 1)'
    return str(value)


def annotation_comment(p):
    parts = []
    if p.node == 'attribute':
        parts.append(f'@XmlAttribute(name = "{p.xml_name}"' + (', required = true)' if p.required else ')'))
    elif p.node == 'choice':
        parts.append(f'@XmlElements ({len(p.choices)} choices)')
    else:
        if p.wrapper:
            parts.append(f'@XmlElementWrapper(name = "{p.wrapper}")')
        if p.implicit:
            parts.append(f'implicit element <{p.xml_name}>')
        else:
            parts.append(f'@XmlElement(name = "{p.xml_name}"' + (', required = true)' if p.required else ')'))
    if p.xml_list:
        parts.append('@XmlList')
    if p.xml_id:
        parts.append('@XmlID')
    if p.idref:
        parts.append('@XmlIDREF')
    if p.adapter:
        parts.append(f'@XmlJavaTypeAdapter({p.adapter.rpartition(".")[2]})')
    if p.class_adapter:
        parts.append(f'class adapter {p.class_adapter.rpartition(".")[2]}')
    if p.runtime_mutable:
        parts.append('runtime_mutable')
    init = p.initializer
    if init.kind not in ('none',):
        parts.append(f'Java: = {init.text}')
    return ' '.join(parts)


def accessor_code(acc, member_names=()):
    """C++ code of a trivial accessor, or None if the member kind has no mechanical mapping. A setter parameter never has the name of a
    member of the class chain (MSVC C4458)."""
    m = acc.member
    name = acc.java.name
    if acc.kind == 'getter':
        rk = m.return_kind
        if rk == 'value':
            return f'{m.type} {name}() const {{ return {m.name}; }}'
        if rk == 'cref':
            return f'const {m.type}& {name}() const {{ return {m.name}; }}'
        if rk == 'pointer':
            inner = m.type[len('std::unique_ptr<'):-1]
            return f'const {inner}* {name}() const {{ return {m.name}.get(); }}'
        if rk == 'field':
            return f'{m.scalar_cpp} {name}() const {{ return {m.name}.get(); }}'
        return None
    param = cpp_ident(acc.java.params[0].name)
    for candidate in (param, 'value', 'newValue', 'newValue_'):
        if candidate not in member_names and candidate != m.name:
            param = candidate
            break
    target = m.name
    rk = m.return_kind
    if rk == 'field':
        return f'void {name}({m.scalar_cpp} {param}) const {{ {target}.set({param}); }}'
    if rk == 'value':
        return f'void {name}({m.type} {param}) {{ {target} = {param}; }}'
    if rk in ('cref', 'pointer'):
        return f'void {name}({m.type} {param}) {{ {target} = std::move({param}); }}'
    return None
