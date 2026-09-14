"""jaxb: front end of the static data generator (docs/design/static-data.md §1).

Reads the Java sources through tools/gen/javasrc.py, starts at the unmarshal roots of xmlgen.toml and follows the JAXB type graph
(bound property types, @XmlElements choices, IDREF targets, adapter value types, superclasses, enums). For every reachable type it
applies the JAXB RI binding rules the sources rely on and builds the model that cppmodel.py maps to C++:

- accessor type: @XmlAccessorType is @Inherited (the nearest annotated class of the superclass chain), default PUBLIC_MEMBER;
  FIELD binds every non-static, non-transient field without @XmlTransient, PUBLIC_MEMBER every public field and every public
  getter/setter pair, NONE only annotated members; annotated getters/setters are bound with every access type (JAXB RI
  ClassInfoImpl.findFieldProperties/findGetterSetterProperties); PROPERTY is rejected (unused);
- property kinds: @XmlAttribute, @XmlElement (type=), @XmlElements, @XmlElementWrapper, @XmlList, @XmlID, @XmlIDREF,
  @XmlJavaTypeAdapter (field and class level) and unannotated FIELD/PUBLIC_MEMBER properties (elements);
- XML names: explicit names, otherwise NameConverter.standard.toVariableName of the property name (field name, or the decapitalized
  getter/setter name); a default name that differs from the Java name must be confirmed in xmlgen.toml [xml_names];
- lifecycle: the nearest declared afterUnmarshal of the class chain (JAXB calls exactly one); beforeUnmarshal is rejected except on
  the configured StaticData root;
- initializers: literals, enum constants, null, empty collections and `new X()`; anything else needs an xmlgen.toml [initializers] entry.

Anything the rules do not cover (unknown annotations, unsupported types, conflicting names, ...) raises XmlGenError with file:line.
"""
from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass, field

_TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)

from gen import javasrc  # noqa: E402

JAXB_PACKAGE = 'javax.xml.bind.annotation'
PROJECT_PREFIX = 'com.aionemu.gameserver.'

CLASS_ANNOTATIONS = {'XmlRootElement', 'XmlType', 'XmlAccessorType', 'XmlTransient', 'XmlSeeAlso', 'XmlJavaTypeAdapter', 'XmlEnum'}
MEMBER_ANNOTATIONS = {'XmlAttribute', 'XmlElement', 'XmlElements', 'XmlElementWrapper', 'XmlList', 'XmlID', 'XmlIDREF',
                      'XmlJavaTypeAdapter', 'XmlTransient'}
ENUM_CONSTANT_ANNOTATIONS = {'XmlEnumValue'}

PRIMITIVES = {'boolean', 'byte', 'short', 'int', 'long', 'float', 'double'}
BOXED = {'java.lang.Boolean': 'boolean', 'java.lang.Byte': 'byte', 'java.lang.Short': 'short', 'java.lang.Integer': 'int',
         'java.lang.Long': 'long', 'java.lang.Float': 'float', 'java.lang.Double': 'double'}
LISTS = {'java.util.List', 'java.util.ArrayList', 'java.util.LinkedList', 'java.util.Collection'}
SETS = {'java.util.Set', 'java.util.HashSet', 'java.util.LinkedHashSet'}
MAPS = {'java.util.Map', 'java.util.HashMap', 'java.util.LinkedHashMap', 'java.util.TreeMap', 'java.util.EnumMap'}
EMPTY_COLLECTION_NEW = {'ArrayList', 'LinkedList', 'HashSet', 'LinkedHashSet', 'java.util.ArrayList', 'java.util.HashSet'}

JAVA_KEYWORDS = javasrc.KEYWORDS


class XmlGenError(Exception):
    """A generator error; the message starts with file:line when a source location is known."""


def loc(node, cu=None):
    """'path:line' of a javasrc declaration node."""
    if cu is None:
        owner = node
        while owner is not None and not isinstance(owner, javasrc.TypeDecl):
            owner = getattr(owner, 'owner', None)
        cu = owner.cu if owner is not None else None
    path = cu.relpath or cu.path if cu is not None else '?'
    return f'{path}:{getattr(node, "line", 0)}'


def short_fqn(fqn):
    """com.aionemu.gameserver.a.B -> a.B (the key form of xmlgen.toml)"""
    return fqn[len(PROJECT_PREFIX):] if fqn.startswith(PROJECT_PREFIX) else fqn


def long_fqn(key):
    """a.B -> com.aionemu.gameserver.a.B (keys may also be written in full)"""
    return key if key.startswith('com.') or key.startswith('java.') else PROJECT_PREFIX + key


# ----------------------------------------------------------------------------------------------------------------------------------
# JAXB NameConverter.standard.toVariableName and Introspector.decapitalize
# ----------------------------------------------------------------------------------------------------------------------------------

def _char_class(c):
    if c.isupper():
        return 'U'
    if c.islower():
        return 'L'
    if c.isdigit():
        return 'D'
    if c.isalpha():
        return 'O'
    return 'P'


def jaxb_word_list(s):
    """NameUtil.toWordList: split at punctuation, lower->upper, letter<->digit and before the last capital of a capital run that is followed
    by a lower-case letter (XMLData -> XML, Data)."""
    words = []
    i, n = 0, len(s)
    while i < n:
        while i < n and _char_class(s[i]) == 'P':
            i += 1
        if i >= n:
            break
        j = i + 1
        while j < n:
            a, b = _char_class(s[j - 1]), _char_class(s[j])
            if b == 'P':
                break
            if (a == 'D') != (b == 'D'):
                break
            if a == 'L' and b != 'L':
                break
            if a == 'U' and b == 'U' and j + 1 < n and _char_class(s[j + 1]) == 'L':
                break
            j += 1
        w = s[i:j]
        words.append(w[:1].upper() + w[1:])
        i = j
    return words


def jaxb_variable_name(s):
    words = jaxb_word_list(s)
    if not words:
        return s
    name = words[0].lower() + ''.join(words[1:])
    return name + '_' if name in JAVA_KEYWORDS else name


def decapitalize(s):
    if len(s) > 1 and s[0].isupper() and s[1].isupper():
        return s
    return s[:1].lower() + s[1:]


# ----------------------------------------------------------------------------------------------------------------------------------
# Model
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass
class JType:
    """A resolved Java type of a bound member."""
    kind: str  # primitive | boxed | string | enum | class | interface | list | set | array | map | external
    fqn: str  # primitive name, java.lang.Integer, project fqn, java.util.List, ...
    elem: 'JType | None' = None

    def __str__(self):
        if self.kind in ('list', 'set'):
            return f'{self.fqn.rpartition(".")[2]}<{self.elem}>'
        if self.kind == 'array':
            return f'{self.elem}[]'
        return self.fqn.rpartition('.')[2] if self.kind != 'primitive' else self.fqn

    def to_json(self):
        d = {'kind': self.kind, 'fqn': self.fqn}
        if self.elem is not None:
            d['elem'] = self.elem.to_json()
        return d

    @property
    def is_collection(self):
        return self.kind in ('list', 'set', 'array')

    @property
    def scalar(self):
        """primitive, boxed, String or enum"""
        return self.kind in ('primitive', 'boxed', 'string', 'enum')


@dataclass
class Init:
    """A parsed field initializer."""
    kind: str  # none | null | literal | enum | empty_collection | new_object | config | expr
    text: str = ''
    value: object = None  # literal value (int/float/bool/str) or enum constant name
    token_kind: str = ''  # javasrc token kind of the literal
    enum_fqn: str = ''
    cpp: str = ''  # [initializers] mapping
    reason: str = ''

    def to_json(self):
        d = {'kind': self.kind}
        if self.text:
            d['java'] = self.text
        if self.kind == 'literal':
            d['value'] = self.value
        elif self.kind == 'enum':
            d['enum'] = self.enum_fqn
            d['constant'] = self.value
        elif self.kind == 'config':
            d['cpp'] = self.cpp
        return d


@dataclass
class Property:
    owner: 'ClassModel'
    java_name: str
    source: str  # field | method
    node: str  # attribute | element | choice
    xml_name: str | None
    java_type: JType  # declared type (field type, getter return type or setter parameter type)
    value_type: JType  # bound type: collection element type, @XmlElement(type=) or the adapter value type
    required: bool = False
    implicit: bool = False
    collection: str | None = None  # list | set | array (the declared type is a collection)
    wrapper: str | None = None
    wrapper_required: bool = False
    choices: list = field(default_factory=list)  # [(xml name, fqn)] in declaration order
    xml_list: bool = False
    xml_id: bool = False
    idref: bool = False
    adapter: str | None = None  # field-level adapter fqn
    class_adapter: str | None = None  # class-level adapter fqn of the bound type
    getter: str | None = None
    setter: str | None = None
    access: str = 'private'  # private | protected | public | package
    initializer: Init = field(default_factory=lambda: Init('none'))
    decl: object = None  # FieldDecl or MethodDecl
    # policy (xmlgen.toml)
    runtime_mutable: bool = False
    storage_by_pointer: bool = False
    optional_string: bool = False
    enforce_required: bool = True  # false for [unenforced_required]: JAXB ignores required, and the data violates it
    lenient_enum: bool = False  # [lenient_enums]: an unknown constant gives null (JAXB's enum leaf) instead of an error

    @property
    def key(self):
        return f'{self.owner.fqn}.{self.java_name}'

    @property
    def location(self):
        return loc(self.decl)

    @property
    def kind(self):
        """IR property kind (design §1.3)"""
        if self.source == 'method':
            return 'methodSetter'
        if self.idref:
            return 'idref'
        if self.adapter or self.class_adapter:
            return 'adapter'
        if self.node == 'choice':
            return 'choiceList' if self.collection else 'choice'
        if self.wrapper:
            return 'wrapper'
        if self.node == 'attribute':
            if self.xml_list:
                return 'xmlListAttribute'
            return 'attributeCollection' if self.collection else 'attribute'
        if self.xml_list:
            return 'xmlListElement'
        return 'elementList' if self.collection else 'element'


@dataclass
class EnumModel:
    fqn: str
    td: object
    constants: list  # [(java name, xml lexical form)]
    has_methods: bool
    has_constructor_args: bool
    has_constant_bodies: bool
    has_fields: bool
    xml_type_name: str | None
    referenced_by: set = field(default_factory=set)

    @property
    def location(self):
        return loc(self.td) if self.td is not None else 'xmlgen.toml [external_enums]'

    @property
    def external(self):
        return self.td is None


@dataclass
class ClassModel:
    fqn: str
    td: object
    accessor_type: str
    abstract: bool
    superclass: str | None  # project fqn of the Java superclass (bound as a JAXB type), None for Object
    xml_root_element: str | None
    xml_type_name: str | None  # effective @XmlType name ('' anonymous)
    xml_transient: bool
    class_adapter: str | None
    properties: list = field(default_factory=list)
    hook: object = None  # MethodDecl of afterUnmarshal declared in this class
    before_unmarshal: object = None
    subclasses: list = field(default_factory=list)
    choice_base: bool = False  # declared element type of an @XmlElements property
    referenced_by: set = field(default_factory=set)
    denied: list = field(default_factory=list)  # [(field name, reason)]
    no_arg_constructor: str = 'implicit'  # implicit | public | protected | private | package | none
    mutable_fields: list = field(default_factory=list)  # unbound FieldDecls listed in [runtime_mutable] (generated as mutable Field<T>)
    ignored_attributes: list = field(default_factory=list)  # [(XML name, reason)] consumed without a member ([ignore_attributes])

    @property
    def location(self):
        return loc(self.td)

    @property
    def simple_name(self):
        return self.td.name


# ----------------------------------------------------------------------------------------------------------------------------------
# Policy (xmlgen.toml)
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass
class Policy:
    roots: list
    xml_names: dict = field(default_factory=dict)  # property key -> confirmed default xml name
    deny_implicit: dict = field(default_factory=dict)  # property key -> reason
    initializers: dict = field(default_factory=dict)  # property key -> {cpp, reason}
    adapters: dict = field(default_factory=dict)  # adapter fqn -> table
    runtime_mutable: dict = field(default_factory=dict)  # property key -> reason
    force_behaviour: dict = field(default_factory=dict)  # class fqn -> reason
    storage_by_pointer: dict = field(default_factory=dict)  # property key -> reason
    optional_strings: dict = field(default_factory=dict)  # property key -> reason
    ignore_public_members: dict = field(default_factory=dict)  # class fqn + '.' + property -> reason
    before_unmarshal_allowed: dict = field(default_factory=dict)  # class fqn -> reason
    external_enums: dict = field(default_factory=dict)  # JDK enum fqn -> {namespace, header, constants, reason}
    ignore_attributes: dict = field(default_factory=dict)  # class fqn + '.' + XML attribute name -> reason
    unenforced_required: dict = field(default_factory=dict)  # property key -> reason (required = true violated by the data)
    lenient_enums: dict = field(default_factory=dict)  # property key -> reason (unknown constants in the data become null like JAXB)
    used: set = field(default_factory=set)

    TABLES = ('xml_names', 'deny_implicit', 'initializers', 'adapters', 'runtime_mutable', 'force_behaviour', 'storage_by_pointer',
              'optional_strings', 'ignore_public_members', 'before_unmarshal_allowed', 'external_enums', 'ignore_attributes',
              'unenforced_required', 'lenient_enums')

    @staticmethod
    def _keyed(table, name):
        out = {}
        for key, value in (table or {}).items():
            if isinstance(value, str):
                value = {'reason': value}
            if not isinstance(value, dict) or not value.get('reason'):
                raise XmlGenError(f'xmlgen.toml [{name}] {key}: every entry needs a reason')
            out[long_fqn(key)] = value
        return out

    @classmethod
    def from_toml(cls, doc):
        roots = [long_fqn(r) for r in doc.get('roots', [])]
        if not roots:
            raise XmlGenError('xmlgen.toml: roots is empty')
        unknown = set(doc) - {'roots', 'java_src'} - set(cls.TABLES)
        if unknown:
            raise XmlGenError(f'xmlgen.toml: unknown keys {sorted(unknown)}')
        p = cls(roots)
        for name in cls.TABLES:
            setattr(p, name, cls._keyed(doc.get(name), name))
        for key, value in p.xml_names.items():
            if 'name' not in value:
                raise XmlGenError(f'xmlgen.toml [xml_names] {key}: needs name')
        for key, value in p.initializers.items():
            if 'cpp' not in value:
                raise XmlGenError(f'xmlgen.toml [initializers] {key}: needs cpp (use "" for the default C++ initializer)')
        for key, value in p.external_enums.items():
            if not value.get('namespace') or not value.get('constants') or not isinstance(value['constants'], list):
                raise XmlGenError(f'xmlgen.toml [external_enums] {key}: needs namespace and constants')
        return p

    def lookup(self, table_name, key):
        table = getattr(self, table_name)
        value = table.get(key)
        if value is not None:
            self.used.add((table_name, key))
        return value

    def unused_entries(self):
        out = []
        for name in self.TABLES:
            for key in sorted(getattr(self, name)):
                if (name, key) not in self.used:
                    out.append(f'[{name}] {short_fqn(key)}')
        return out


# ----------------------------------------------------------------------------------------------------------------------------------
# Front end
# ----------------------------------------------------------------------------------------------------------------------------------

class Model:
    """The reachable JAXB model of the configured roots."""

    def __init__(self, index, policy):
        self.index = index
        self.policy = policy
        self.classes = {}  # fqn -> ClassModel
        self.enums = {}  # fqn -> EnumModel
        self.unreachable = []  # [fqn] JAXB-annotated types that are not reachable
        self.external_types = {}  # fqn -> [property keys] (adapter value types, LocalDateTime, ...)
        self.errors = []
        self._queue = []

    # -- helpers ---------------------------------------------------------------------------------------------------------------------
    def error(self, node, message):
        return XmlGenError(f'{loc(node)}: {message}')

    def jaxb_name(self, ann, ctx):
        """simple JAXB annotation name, or None for non-JAXB annotations"""
        kind, value = self.index.resolve_kind(ann.name, ctx)
        if kind in ('external', 'external_guess') and isinstance(value, str):
            pkg, _, simple = value.rpartition('.')
            if pkg == JAXB_PACKAGE or pkg == JAXB_PACKAGE + '.adapters':
                return simple
            if pkg.startswith('javax.xml.bind'):
                raise self.error(ctx, f'unsupported JAXB annotation @{value}')
        return None

    def jaxb_annotations(self, node, ctx, allowed, what):
        out = {}
        for ann in node.annotations:
            name = self.jaxb_name(ann, ctx)
            if name is None:
                continue
            if name not in allowed:
                raise self.error(node, f'unsupported JAXB annotation @{name} on {what}')
            if name in out:
                raise self.error(node, f'repeated @{name}')
            out[name] = ann
        return out

    @staticmethod
    def string_arg(ann, name, node, model):
        v = ann.get(name)
        if v is None:
            return None
        if v.kind != 'literal' or not isinstance(v.value, str):
            raise model.error(node, f'@{ann.name}({name}=...) must be a string literal, got {v.text}')
        return v.value

    @staticmethod
    def bool_arg(ann, name, node, model):
        v = ann.get(name)
        if v is None:
            return False
        if v.kind != 'literal' or not isinstance(v.value, bool):
            raise model.error(node, f'@{ann.name}({name}=...) must be a boolean literal, got {v.text}')
        return v.value

    def class_arg(self, ann, name, node, ctx):
        v = ann.get(name)
        if v is None:
            return None
        if v.kind != 'class':
            raise self.error(node, f'@{ann.name}({name}=...) must be a class literal, got {v.text}')
        kind, fqn = self.index.resolve_kind(v.type.name, ctx)
        if kind not in ('project', 'external'):
            raise self.error(node, f'cannot resolve {v.type.name} ({kind})')
        return fqn

    def check_no_namespace(self, ann, node):
        if ann.get('namespace') is not None and self.string_arg(ann, 'namespace', node, self) != '':
            raise self.error(node, f'@{ann.name}(namespace=...) other than "" is not supported')

    def check_args(self, ann, allowed, node):
        for n, _ in ann.args:
            if n not in allowed:
                raise self.error(node, f'unsupported @{ann.name} argument {n!r}')

    def resolve(self, typeref, ctx, node):
        """JType of a TypeRef"""
        if typeref.dims:
            if typeref.dims > 1:
                raise self.error(node, f'multi-dimensional array {typeref} is not supported')
            elem = self.resolve(typeref.with_dims(-typeref.dims), ctx, node)
            return JType('array', 'array', elem)
        if typeref.wildcard:
            raise self.error(node, f'wildcard type {typeref} is not supported')
        name = typeref.name
        if name in PRIMITIVES:
            return JType('primitive', name)
        if name in ('char', 'void'):
            raise self.error(node, f'type {name} is not supported')
        kind, fqn = self.index.resolve_kind(name, ctx)
        if kind == 'typevar':
            raise self.error(node, f'type variable {name} is not supported')
        if kind == 'project':
            td = self.index.lookup(fqn)
            if typeref.args:
                raise self.error(node, f'generic project type {typeref} is not supported')
            if td.kind == 'enum':
                return JType('enum', fqn)
            if td.kind == 'interface':
                return JType('interface', fqn)
            if td.kind == 'class':
                return JType('class', fqn)
            raise self.error(node, f'{td.kind} {fqn} cannot be bound')
        if kind not in ('external',):
            raise self.error(node, f'cannot resolve type {typeref} ({kind})')
        if fqn in BOXED:
            return JType('boxed', fqn)
        if fqn == 'java.lang.String':
            return JType('string', fqn)
        if fqn in LISTS or fqn in SETS:
            args = typeref.args
            if not args or len(args) != 1:
                raise self.error(node, f'raw or diamond collection type {typeref}')
            elem = self.resolve(args[0], ctx, node)
            if elem.is_collection:
                raise self.error(node, f'nested collection {typeref} is not supported')
            return JType('list' if fqn in LISTS else 'set', fqn, elem)
        if fqn in MAPS:
            return JType('map', fqn)
        if self.policy.lookup('external_enums', fqn) is not None:
            return JType('enum', fqn)
        return JType('external', fqn)

    # -- reachability ------------------------------------------------------------------------------------------------------------------
    def build(self):
        for root in self.policy.roots:
            if self.index.lookup(root) is None:
                raise XmlGenError(f'xmlgen.toml: root {root} not found')
            self.enqueue(root, 'root')
        while self._queue:
            fqn, why = self._queue.pop()
            if fqn in self.classes or fqn in self.enums:
                continue
            td = self.index.lookup(fqn)
            if td is None and fqn in self.policy.external_enums:
                table = self.policy.external_enums[fqn]
                self.enums[fqn] = EnumModel(fqn, None, [(c, c) for c in table['constants']], True, True, False, True, None)
                self.enums[fqn].referenced_by.add(why)
                continue
            try:
                if td.kind == 'enum':
                    self.enums[fqn] = self.build_enum(td)
                    self.enums[fqn].referenced_by.add(why)
                elif td.kind == 'class':
                    self.build_class(td, why)
                else:
                    raise self.error(td, f'{td.kind} {fqn} is reachable from {why} but cannot be bound')
            except XmlGenError as e:
                self.errors.append(str(e))
        if self.errors:
            raise XmlGenError('\n'.join(sorted(set(self.errors))))
        for c in self.classes.values():
            if c.superclass:
                self.classes[c.superclass].subclasses.append(c.fqn)
        for c in self.classes.values():
            c.subclasses.sort()
            for p in c.properties:
                if p.node == 'choice':
                    base = p.value_type.fqn
                    if base in self.classes:
                        self.classes[base].choice_base = True
                    for name, target in p.choices:
                        if not self.is_subclass(target, base):
                            raise self.error(p.decl, f'choice {name} -> {short_fqn(target)} is not a subclass of {short_fqn(base)}')
        self.find_unreachable()
        return self

    def enqueue(self, fqn, why):
        if fqn in self.classes:
            self.classes[fqn].referenced_by.add(why)
        elif fqn in self.enums:
            self.enums[fqn].referenced_by.add(why)
        else:
            self._queue.append((fqn, why))

    def is_subclass(self, fqn, base):
        while fqn is not None:
            if fqn == base:
                return True
            c = self.classes.get(fqn)
            fqn = c.superclass if c else None
        return False

    def find_unreachable(self):
        out = []
        for fqn, td in sorted(self.index.types.items()):
            if fqn in self.classes or fqn in self.enums or td.kind not in ('class', 'enum'):
                continue
            annotated = any(self.safe_jaxb_name(a, td) for a in td.annotations)
            if not annotated:
                for m in list(td.fields) + list(td.methods):
                    if any(self.safe_jaxb_name(a, m) for a in m.annotations):
                        annotated = True
                        break
            if annotated:
                out.append(fqn)
        self.unreachable = out

    def safe_jaxb_name(self, ann, ctx):
        try:
            return self.jaxb_name(ann, ctx)
        except XmlGenError:
            return 'unsupported'

    # -- enums -------------------------------------------------------------------------------------------------------------------------
    def build_enum(self, td):
        anns = self.jaxb_annotations(td, td, {'XmlEnum', 'XmlType', 'XmlSeeAlso', 'XmlAccessorType'}, f'enum {td.fqn}')
        if 'XmlEnum' in anns:
            self.check_args(anns['XmlEnum'], {'value'}, td)
            v = anns['XmlEnum'].get('value')
            if v is not None and (v.kind != 'class' or v.type.name != 'String'):
                raise self.error(td, f'@XmlEnum({v.text}) is not supported (only String lexical forms)')
        type_name = None
        if 'XmlType' in anns:
            self.check_args(anns['XmlType'], {'name', 'propOrder'}, td)
            type_name = self.string_arg(anns['XmlType'], 'name', td, self)
        constants = []
        seen = set()
        for c in td.enum_constants:
            canns = self.jaxb_annotations(c, td, ENUM_CONSTANT_ANNOTATIONS, f'enum constant {td.fqn}.{c.name}')
            xml = c.name
            if 'XmlEnumValue' in canns:
                xml = self.string_arg(canns['XmlEnumValue'], 'value', c, self)
            if xml in seen:
                raise self.error(c, f'duplicate XML enum value {xml!r}')
            seen.add(xml)
            constants.append((c.name, xml))
        if not constants:
            raise self.error(td, f'enum {td.fqn} has no constants')
        for m in list(td.fields) + list(td.methods):
            if any(self.jaxb_name(a, m) for a in m.annotations):
                raise self.error(m, 'JAXB annotations on enum members are not supported')
        methods = [m for m in td.methods if m.kind != 'constructor']
        return EnumModel(td.fqn, td, constants, bool(methods), any(c.args for c in td.enum_constants),
                         any(c.body is not None for c in td.enum_constants), any('static' not in f.modifiers for f in td.fields),
                         type_name)

    # -- classes -----------------------------------------------------------------------------------------------------------------------
    def accessor_type(self, td):
        t = td
        while t is not None:
            anns = self.jaxb_annotations(t, t, CLASS_ANNOTATIONS, f'class {t.fqn}')
            a = anns.get('XmlAccessorType')
            if a is not None:
                v = a.get('value')
                if v is None or v.kind != 'name':
                    raise self.error(t, f'@XmlAccessorType needs XmlAccessType.X, got {a}')
                value = v.name.rpartition('.')[2]
                if value not in ('FIELD', 'NONE', 'PUBLIC_MEMBER', 'PROPERTY'):
                    raise self.error(t, f'unknown XmlAccessType {v.name}')
                if value == 'PROPERTY':
                    raise self.error(t, 'XmlAccessType.PROPERTY is not supported (unused by the sources)')
                return value
            t = self.superclass_decl(t)
        return 'PUBLIC_MEMBER'

    def superclass_decl(self, td):
        if not td.extends:
            return None
        kind, fqn = self.index.resolve_kind(td.extends[0].name, td)
        if kind == 'project':
            return self.index.lookup(fqn)
        return None

    def build_class(self, td, why):
        fqn = td.fqn
        if td.type_params:
            raise self.error(td, f'generic class {fqn} cannot be bound')
        if td.outer is not None and 'static' not in td.modifiers and td.outer.kind != 'interface':
            raise self.error(td, f'inner (non-static) class {fqn} cannot be bound by JAXB')
        anns = self.jaxb_annotations(td, td, CLASS_ANNOTATIONS, f'class {fqn}')
        if 'XmlEnum' in anns:
            raise self.error(td, '@XmlEnum on a class')
        superclass = None
        if td.extends:
            kind, sfqn = self.index.resolve_kind(td.extends[0].name, td)
            if kind == 'project':
                superclass = sfqn
            elif sfqn != 'java.lang.Object':
                raise self.error(td, f'superclass {td.extends[0]} ({kind} {sfqn}) is outside the project and cannot be bound')
        root = None
        if 'XmlRootElement' in anns:
            self.check_args(anns['XmlRootElement'], {'name'}, td)
            root = self.string_arg(anns['XmlRootElement'], 'name', td, self)
            if root is None or root == '##default':
                root = jaxb_variable_name(td.name)
        type_name = jaxb_variable_name(td.name)
        if 'XmlType' in anns:
            self.check_args(anns['XmlType'], {'name', 'propOrder', 'namespace'}, td)
            self.check_no_namespace(anns['XmlType'], td)
            n = self.string_arg(anns['XmlType'], 'name', td, self)
            if n is not None and n != '##default':
                type_name = n
        transient = 'XmlTransient' in anns
        if transient:
            raise self.error(td, f'@XmlTransient class {fqn} is reachable (folding transient classes is not implemented)')
        class_adapter = None
        if 'XmlJavaTypeAdapter' in anns:
            raise self.error(td, f'class-level adapter class {fqn} is reachable itself; bind its adapter value type instead')
        cm = ClassModel(fqn, td, self.accessor_type(td), 'abstract' in td.modifiers, superclass, root, type_name, transient, class_adapter)
        cm.referenced_by.add(why)
        self.classes[fqn] = cm
        ctors = [m for m in td.methods if m.kind == 'constructor']
        if ctors:
            no_arg = [m for m in ctors if not m.params]
            if not no_arg:
                cm.no_arg_constructor = 'none'
            else:
                mods = no_arg[0].modifiers
                cm.no_arg_constructor = next((m for m in ('public', 'protected', 'private') if m in mods), 'package')
        if superclass:
            self.enqueue(superclass, f'superclass of {short_fqn(fqn)}')
        self.build_properties(cm)
        self.build_hooks(cm)
        names = {}
        for p in cm.properties:
            for n in self.property_xml_names(p):
                if n in names:
                    raise self.error(p.decl, f'XML name {n!r} is bound twice in {short_fqn(fqn)} ({names[n]} and {p.java_name})')
                names[n] = p.java_name

    @staticmethod
    def property_xml_names(p):
        if p.node == 'attribute':
            return [('@', p.xml_name)]
        if p.node == 'choice':
            return [('<', n) for n, _ in p.choices]
        if p.wrapper:
            return [('<', p.wrapper)]
        return [('<', p.xml_name)]

    def build_hooks(self, cm):
        td = cm.td
        for m in td.methods:
            if m.name in ('afterUnmarshal', 'beforeUnmarshal') and m.kind == 'method':
                if len(m.params) != 2 or 'static' in m.modifiers:
                    raise self.error(m, f'{m.name} must be an instance method (Unmarshaller, Object)')
                p0 = self.index.resolve_kind(m.params[0].type.name, m)
                p1 = self.index.resolve_kind(m.params[1].type.name, m)
                if p0 != ('external', 'javax.xml.bind.Unmarshaller') or p1 != ('external', 'java.lang.Object'):
                    raise self.error(m, f'{m.name} parameters must be (Unmarshaller, Object), got {p0}, {p1}')
                if m.name == 'afterUnmarshal':
                    cm.hook = m
                else:
                    if self.policy.lookup('before_unmarshal_allowed', cm.fqn) is None:
                        raise self.error(m, 'beforeUnmarshal is not supported (xmlgen.toml [before_unmarshal_allowed])')
                    cm.before_unmarshal = m
            elif m.name in ('beforeMarshal', 'afterMarshal'):
                pass  # write-back only (design §3.6)

    # -- properties --------------------------------------------------------------------------------------------------------------------
    def build_properties(self, cm):
        td = cm.td
        at = cm.accessor_type
        for f in td.fields:
            anns = self.jaxb_annotations(f, f, MEMBER_ANNOTATIONS, f'field {f.name}')
            if 'static' in f.modifiers or 'transient' in f.modifiers:
                if anns:
                    raise self.error(f, f'JAXB annotation on a static or transient field {f.name}')
                continue
            key = f'{cm.fqn}.{f.name}'
            if 'XmlTransient' in anns:
                if len(anns) > 1:
                    raise self.error(f, f'@XmlTransient field {f.name} has other JAXB annotations')
                if self.policy.lookup('runtime_mutable', key) is not None:
                    cm.mutable_fields.append(f)
                continue
            bound = bool(anns) or at == 'FIELD' or (at == 'PUBLIC_MEMBER' and 'public' in f.modifiers)
            if not bound:
                if self.policy.lookup('runtime_mutable', key) is not None:
                    cm.mutable_fields.append(f)
                continue
            if not anns:
                deny = self.policy.lookup('deny_implicit', key)
                if deny is not None:
                    cm.denied.append((f.name, deny['reason']))
                    continue
            try:
                jt = self.resolve(f.type, f, f)
                self.add_property(cm, f.name, 'field', anns, jt, f, implicit=not anns, access=_access(f.modifiers))
            except XmlGenError as e:
                self.errors.append(str(e))
        self.build_accessor_properties(cm)
        prefix = cm.fqn + '.'
        for key in sorted(self.policy.ignore_attributes):
            if key.startswith(prefix) and '.' not in key[len(prefix):]:
                name = key[len(prefix):]
                self.policy.lookup('ignore_attributes', key)
                if any(p.node == 'attribute' and p.xml_name == name for p in cm.properties):
                    raise self.error(td, f'[ignore_attributes] {short_fqn(key)} is a bound attribute')
                cm.ignored_attributes.append((name, self.policy.ignore_attributes[key]['reason']))

    def build_accessor_properties(self, cm):
        td = cm.td
        at = cm.accessor_type
        getters, setters, order = {}, {}, []
        for m in td.methods:
            if m.kind != 'method':
                continue
            anns = self.jaxb_annotations(m, m, MEMBER_ANNOTATIONS, f'method {m.name}')
            if 'static' in m.modifiers:
                if anns:
                    raise self.error(m, 'JAXB annotation on a static method')
                continue
            used = False
            name = m.name
            prop = None
            if name.startswith('get') and len(name) > 3 and not m.params:
                prop = name[3:]
            elif name.startswith('is') and len(name) > 2 and not m.params:
                prop = name[2:]
            if prop is not None:
                getters[prop] = (m, anns)
                order.append(prop)
                used = True
            if name.startswith('set') and len(name) > 3 and len(m.params) == 1:
                prop = name[3:]
                if prop in setters:
                    raise self.error(m, f'overloaded setter {name} is not supported')
                setters[prop] = (m, anns)
                order.append(prop)
                used = True
            if not used and anns:
                raise self.error(m, f'JAXB annotation on method {name} that is neither a getter nor a setter')
        seen = set()
        for prop in order:
            if prop in seen:
                continue
            seen.add(prop)
            g = getters.get(prop)
            s = setters.get(prop)
            annotated = (g is not None and g[1]) or (s is not None and s[1])
            if annotated:
                if (g is not None and 'XmlTransient' in g[1]) or (s is not None and 'XmlTransient' in s[1]):
                    continue
                if g is not None and g[1] and s is not None and s[1]:
                    raise self.error(g[0], f'both getter and setter of {prop} are annotated')
                anns = g[1] if g is not None and g[1] else s[1]
                node = g[0] if g is not None and g[1] else s[0]
                if s is None:
                    raise self.error(node, f'annotated property {prop} has no setter (unmarshalling needs one)')
                jt = self.resolve(s[0].params[0].type, s[0], s[0])
                if g is not None:
                    gt = self.resolve(g[0].return_type, g[0], g[0])
                    if gt != jt:
                        raise self.error(g[0], f'getter type {gt} differs from setter type {jt}')
                p = self.add_property(cm, decapitalize(prop), 'method', anns, jt, node, implicit=False, access=_access(s[0].modifiers))
                p.getter = g[0].name if g is not None else None
                p.setter = s[0].name
            elif at == 'PUBLIC_MEMBER' and g is not None and s is not None and 'public' in g[0].modifiers and 'public' in s[0].modifiers:
                key = f'{cm.fqn}.{decapitalize(prop)}'
                if self.policy.lookup('ignore_public_members', key) is None:
                    raise self.error(g[0], f'public getter/setter pair {prop} of the PUBLIC_MEMBER class {short_fqn(cm.fqn)} is an implicit JAXB '
                                           f'element property (xmlgen.toml [ignore_public_members] {short_fqn(key)})')

    def add_property(self, cm, java_name, source, anns, jt, decl, implicit, access):
        ctx = decl
        if 'XmlElements' in anns and ('XmlElement' in anns or 'XmlAttribute' in anns):
            raise self.error(decl, '@XmlElements combined with @XmlElement/@XmlAttribute')
        if 'XmlAttribute' in anns and ('XmlElement' in anns or 'XmlElementWrapper' in anns):
            raise self.error(decl, '@XmlAttribute combined with an element annotation')
        key = f'{cm.fqn}.{java_name}'
        collection = jt.kind if jt.is_collection else None
        value = jt.elem if jt.is_collection else jt
        if jt.kind == 'map':
            raise self.error(decl, f'Map property {java_name} cannot be bound (xmlgen.toml [deny_implicit] {short_fqn(key)})')
        p = Property(cm, java_name, source, 'element', None, jt, value, implicit=implicit, collection=collection, access=access, decl=decl)
        if 'XmlList' in anns:
            self.check_args(anns['XmlList'], set(), decl)
            if not collection:
                raise self.error(decl, '@XmlList on a non-collection property')
            p.xml_list = True
        if 'XmlID' in anns:
            p.xml_id = True
        if 'XmlIDREF' in anns:
            p.idref = True
        adapter_ann = anns.get('XmlJavaTypeAdapter')
        if adapter_ann is not None:
            self.check_args(adapter_ann, {'value'}, decl)
            p.adapter = self.class_arg(adapter_ann, 'value', decl, ctx)
            table = self.policy.lookup('adapters', p.adapter)
            if table is None:
                raise self.error(decl, f'adapter {p.adapter} is not configured in xmlgen.toml [adapters]')
        if 'XmlAttribute' in anns:
            a = anns['XmlAttribute']
            self.check_args(a, {'name', 'required'}, decl)
            p.node = 'attribute'
            p.xml_name = self.string_arg(a, 'name', decl, self)
            p.required = self.bool_arg(a, 'required', decl, self)
        elif 'XmlElements' in anns:
            a = anns['XmlElements']
            v = a.get('value')
            if v is None or v.kind != 'array' or not v.items:
                raise self.error(decl, '@XmlElements needs a non-empty array of @XmlElement')
            p.node = 'choice'
            names = set()
            for item in v.items:
                if item.kind != 'annotation' or item.annotation.simple_name != 'XmlElement':
                    raise self.error(decl, f'unexpected @XmlElements entry {item.text}')
                e = item.annotation
                self.check_args(e, {'name', 'type'}, decl)
                name = self.string_arg(e, 'name', decl, self)
                target = self.class_arg(e, 'type', decl, ctx)
                if not name or name == '##default' or target is None:
                    raise self.error(decl, f'@XmlElements entry needs name and type: {item.text}')
                if name in names:
                    raise self.error(decl, f'duplicate choice name {name!r}')
                names.add(name)
                p.choices.append((name, target))
                self.enqueue(target, f'choice {name} of {short_fqn(key)}')
            if 'XmlElementWrapper' in anns:
                raise self.error(decl, '@XmlElementWrapper around @XmlElements is not supported')
            if value.kind not in ('class', 'interface'):
                raise self.error(decl, f'@XmlElements on a property of type {jt}')
        else:
            if 'XmlElement' in anns:
                a = anns['XmlElement']
                self.check_args(a, {'name', 'required', 'type'}, decl)
                p.xml_name = self.string_arg(a, 'name', decl, self)
                p.required = self.bool_arg(a, 'required', decl, self)
                override = self.class_arg(a, 'type', decl, ctx)
                if override is not None:
                    td = self.index.lookup(override)
                    if td is None:
                        if override != value.fqn:
                            raise self.error(decl, f'@XmlElement(type={override}) differs from the element type {value}')
                    else:
                        p.value_type = JType('enum' if td.kind == 'enum' else 'class', override)
            if 'XmlElementWrapper' in anns:
                w = anns['XmlElementWrapper']
                self.check_args(w, {'name', 'required'}, decl)
                if not collection:
                    raise self.error(decl, '@XmlElementWrapper on a non-collection property')
                p.wrapper = self.string_arg(w, 'name', decl, self)
                if p.wrapper is None or p.wrapper == '##default':
                    p.wrapper = self.default_xml_name(p, java_name)
                p.wrapper_required = self.bool_arg(w, 'required', decl, self)
        if p.node != 'choice' and (p.xml_name is None or p.xml_name == '##default'):
            p.xml_name = self.default_xml_name(p, java_name)
        if p.wrapper and p.wrapper_required:
            p.required = True
        self.check_value_type(p)
        self.apply_policy(p)
        p.initializer = self.parse_initializer(p) if source == 'field' else Init('none')
        if p.lenient_enum and (source != 'field' or p.value_type.kind != 'enum' or p.collection or p.xml_list or p.xml_id or p.idref
                               or p.node not in ('attribute', 'element') or p.required or p.runtime_mutable or p.adapter
                               or p.class_adapter or p.initializer.kind not in ('none', 'null')):
            raise self.error(p.decl, '[lenient_enums] entry is not an optional scalar enum attribute or element field without an initializer')
        cm.properties.append(p)
        return p

    def default_xml_name(self, p, java_name):
        name = jaxb_variable_name(java_name)
        if name != java_name:
            confirmed = self.policy.lookup('xml_names', p.key)
            if confirmed is None or confirmed['name'] != name:
                raise self.error(p.decl, f'default XML name of {java_name} is {name!r}; confirm it in xmlgen.toml [xml_names] '
                                         f'{short_fqn(p.key)} = {{ name = "{name}", reason = "..." }}')
        return name

    def check_value_type(self, p):
        v = p.value_type
        key = p.key
        if p.adapter:
            table = self.policy.adapters[p.adapter]
            if table.get('value_type'):
                raise self.error(p.decl, f'adapter {p.adapter} with value_type is a class-level adapter')
            self.external_types.setdefault(v.fqn, []).append(key)
            return
        if p.idref:
            if v.kind != 'class':
                raise self.error(p.decl, f'@XmlIDREF on {p.java_type}')
            self.enqueue(v.fqn, f'IDREF {short_fqn(key)}')
            return
        if v.kind == 'enum':
            self.enqueue(v.fqn, f'property {short_fqn(key)}')
        elif v.kind == 'class':
            td = self.index.lookup(v.fqn)
            anns = self.jaxb_annotations(td, td, CLASS_ANNOTATIONS, f'class {v.fqn}')
            adapter = anns.get('XmlJavaTypeAdapter')
            if adapter is not None:
                afqn = self.class_arg(adapter, 'value', td, td)
                table = self.policy.lookup('adapters', afqn)
                if table is None or not table.get('value_type'):
                    raise self.error(p.decl, f'class-level adapter {afqn} of {v.fqn} needs xmlgen.toml [adapters] with value_type')
                p.class_adapter = afqn
                vt = long_fqn(table['value_type'])
                if self.index.lookup(vt) is None:
                    raise self.error(p.decl, f'adapter value type {vt} not found')
                self.external_types.setdefault(v.fqn, []).append(key)
                self.enqueue(vt, f'adapter value type of {short_fqn(key)}')
            else:
                self.enqueue(v.fqn, f'property {short_fqn(key)}')
        elif v.kind == 'interface':
            if p.node != 'choice':
                raise self.error(p.decl, f'property of interface type {v.fqn} needs @XmlElement(type=) or @XmlElements')
        elif v.kind in ('primitive', 'boxed', 'string'):
            pass
        else:
            raise self.error(p.decl, f'property type {p.java_type} is not supported (adapter?)')
        if p.node == 'attribute' and v.kind in ('class', 'interface'):
            raise self.error(p.decl, f'attribute of class type {v.fqn}')
        if p.xml_list and not v.scalar:
            raise self.error(p.decl, '@XmlList of non-scalar values')
        if p.xml_id and not (v.kind == 'string' and not p.collection):
            raise self.error(p.decl, '@XmlID must be a String property')

    def apply_policy(self, p):
        key = p.key
        if self.policy.lookup('runtime_mutable', key) is not None:
            p.runtime_mutable = True
        if self.policy.lookup('storage_by_pointer', key) is not None:
            p.storage_by_pointer = True
        if self.policy.lookup('optional_strings', key) is not None:
            if p.value_type.kind != 'string' or p.collection:
                raise self.error(p.decl, '[optional_strings] entry is not a String property')
            p.optional_string = True
        if self.policy.lookup('unenforced_required', key) is not None:
            if not p.required:
                raise self.error(p.decl, '[unenforced_required] entry is not required')
            p.enforce_required = False
        if self.policy.lookup('lenient_enums', key) is not None:
            p.lenient_enum = True

    # -- initializers ------------------------------------------------------------------------------------------------------------------
    def parse_initializer(self, p):
        f = p.decl
        span = f.initializer
        mapped = self.policy.lookup('initializers', p.key)
        if span is None:
            if mapped is not None:
                raise self.error(f, f'[initializers] {short_fqn(p.key)}: field has no initializer')
            return Init('none')
        text = span.text
        if mapped is not None:
            return Init('config', text, cpp=mapped['cpp'], reason=mapped['reason'])
        toks = span.texts()
        kinds = span.cu.tokens.kind[span.start:span.end]
        jt = p.java_type
        if toks == ['null']:
            return Init('null', text)
        if len(toks) == 1 and toks[0] in ('true', 'false'):
            return Init('literal', text, toks[0] == 'true', 'boolean')
        neg = len(toks) == 2 and toks[0] == '-' and kinds[1] in (javasrc.INT, javasrc.FLOAT)
        if (len(toks) == 1 and kinds[0] in (javasrc.INT, javasrc.FLOAT, javasrc.STRING)) or neg:
            k = kinds[-1]
            v = javasrc.literal_value(k, toks[-1])
            if neg:
                v = -v
            return Init('literal', text, v, k)
        if jt.kind == 'enum' or (jt.is_collection is False and jt.kind == 'enum'):
            ids = ''.join(toks)
            if re.fullmatch(r'[\w.$]+', ids):
                owner_name, _, constant = ids.rpartition('.')
                enum_fqn = jt.fqn
                if owner_name:
                    kind, efqn = self.index.resolve_kind(owner_name, f)
                    if kind != 'project' or efqn != enum_fqn:
                        raise self.error(f, f'initializer {text} is not a constant of {enum_fqn}')
                if self.index.lookup(enum_fqn) is None:
                    raise self.error(f, f'initializer {text} of the external enum {enum_fqn} needs xmlgen.toml [initializers]')
                constants = {c.name for c in self.index.lookup(enum_fqn).enum_constants}
                if constant in constants:
                    return Init('enum', text, constant, enum_fqn=enum_fqn)
        if jt.is_collection and len(toks) >= 3 and toks[0] == 'new':
            tail = toks[1:]
            name = tail[0]
            if name in EMPTY_COLLECTION_NEW and tail[-2:] == ['(', ')'] and (len(tail) == 3 or tail[1] == '<'):
                return Init('empty_collection', text)
        if jt.kind == 'class' and toks[:1] == ['new'] and toks[-2:] == ['(', ')']:
            kind, nfqn = self.index.resolve_kind(''.join(toks[1:-2]), f)
            if kind == 'project' and nfqn == jt.fqn:
                return Init('new_object', text)
        return Init('expr', text)


def _access(modifiers):
    for m in ('public', 'protected', 'private'):
        if m in modifiers:
            return m
    return 'package'


def load_index(java_src):
    return javasrc.ProjectIndex.from_roots([java_src])
