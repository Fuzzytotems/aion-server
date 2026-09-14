"""fieldmap.py: mechanical concurrency member mapping of the Java game server (design runtime-architecture.md §3, §5.3, §12.2).

Python 3.12 stdlib only; uses javasrc.py as the Java front end. Deterministic output; anything unexpected raises FieldmapError.

What it computes
----------------
1. Class kinds (§3.1), one per class, anonymous and local classes included:
   K1 STATIC_DATA      the JAXB class set. Taken from generated/staticdata-classes.json when that file exists (xmlgen output), otherwise
                       the documented heuristic: every class carrying a javax.xml.bind annotation on the type or a member, closed over
                       project superclasses, over the project types of bound fields (annotated, or unannotated non-static non-transient
                       fields of @XmlAccessorType(FIELD) classes) and over @XmlElement(type=)/@XmlElements/@XmlSeeAlso class values.
   K2 PACKET           classes whose project superclass chain reaches a packet base ([settings] packet_bases, BasePacket).
   K3 IMMUTABLE_VALUE  escaping classes whose instance fields are all final (or effectively final private) with immutable types
                       (primitive, boxed, String, enum, external immutable values, K1, K3) and whose superclass is K3 or external Object.
   K4 SHARED           escaping classes that are not K3.
   K5 CONFINED         classes that never escape. Escape analysis (§3.1, RR-12) is a fixpoint over class trees (all classes below the
                       topmost project superclass share one decision, because a RefCounted base makes every subclass RefCounted):
                       a type escapes when it appears in a static field, a K1 non-bound member, a K2 packet member, a captured variable
                       of a stored callback or task, an argument of a task/stored-callback API, or a member (or captured variable) of an
                       escaping class; an interface that escapes makes every implementor escape; type arguments of project supertypes of an
                       escaping class escape. fieldmap.toml [kinds] overrides with a reason.
   Enums are K1 when JAXB-reached, otherwise K3 (all instance fields final) or K4.
2. The field table (§3.2) for every field: cpp member spelling and the rule that produced it (final/volatile/non-final x
   scalar/reference/string/collection/array/atomic/future/lock/connection/callback, statics, config @Property fields, parts, owner and
   sibling references). Effectively final = private field assigned only in constructors/initializers of its class outside lambdas and
   inner classes (element writes `a[i] = v` do not assign the field). Singletons (Immortal, `static X& getInstance()`): exactly one static
   field of the class type in the class or its SingletonHolder, and every `new X(..)` of the project is its initializer or an assignment to
   it; other static fields of the own type are named constants. fieldmap.toml [immortal] lists interned classes (ZoneName): Immortal,
   referenced as `const X*`.
3. Parts (§3.2.1): pattern 1 (final X f = new X(this..) / f = new X(this..) in a constructor, array element writes included),
   pattern 2 (setF(new X(this..)) in a constructor, or r.setF(new X(r..)) on a local r = new T(..) before r is stored/spawned), pattern 3
   (late-bound controllers: P.setOwner(this) in a constructor, P traced through constructor parameters, super(..)/this(..) arguments
   and getters to the field), pattern 4 (fieldmap.toml [fields] part = true). Part classes derive OwnedPart; their field holding the owner
   is OwnerRef<O> (final, or assigned only in constructors of the part class and its subclasses: writes are searched over the whole tree
   for non-private fields), SelfOrRef<O> (also assigned elsewhere) or Final<O*> (late-bound, set by setOwner);
   fields typed as another part class of the same owner are sibling pointers Field<Sib*>.
4. Callbacks: every lambda, method reference and anonymous class with its syntactic context and storage: a [stored_callback_apis] call,
   a [future_apis] task (owner = the class whose field or storing call receives the Future), a project method or constructor inferred to
   store the parameter into a field (assignment or add/put/offer/push on a field), a field assignment, or 'sync' (not stored). Captured
   variables (javac val$x / this$0) of lambdas, anonymous, local and non-static inner classes, with their C++ members. A captured `this`
   is OwnerRef<X> only when every storage owner of the callback is X (or a subtype); otherwise it retains (const Ref<X>) and is a cycle edge.
5. Java equals/hashCode/compareTo overrides (hasEquals, §3.3 RR-13) and synchronized / lock() counts per method (lint L7).
6. Capture-aware retaining graph and cycles (§5.3): nodes are K3/K4 classes and stored lambda/method-reference sites; edges are retaining
   fields (targets expanded to all subtypes), part edges, captured variables, 'extends' (a subclass contains its superclass part) and
   'stored' (storage owner -> callback). Every field or capture edge inside a strongly connected component needs a cycles.toml resolution.

Outputs (cpp/game-server/generated/concurrency, override with --out)
    fieldmap.json      {format: aion-fieldmap, version, inputs, summary, classes{cid: {...}}, callbacks{id: {...}}, cycleEdges{key: {...}}}
                       one JSON object per line inside the maps (diff friendly). Class ids: FQN for named types, Outer$N for anonymous
                       and Outer$NName for local classes (numbered per named owner in source order), Enum$CONSTANT for constant bodies.
                       Class entry keys: kind, kindName, reason, file, line, javaKind, cppName, extends, implements, externalSuper, base
                       (RefCounted | OwnedPart | Immortal | StaticTemplate), hasEquals, equalsFrom, hashCodeFrom, compareToFrom, partOf,
                       singleton, typeParams, fields [{name, java, line, cpp, rule, modifiers, effectivelyFinal, flags, retains, part,
                       overrideReason, xmlBound}], captures, storage, storageOwners, sync {method: {synchronized, lock}}, callbacks, and
                       the skeleton.py interface: members [{javaName, cppType | declaration, static, initializer, cppName, comment}] and
                       extraDeclarations (generated callback structs of the anonymous/local classes declared inside).
    parts.json         {format: aion-parts, version, parts: [{owner, field, partType, patterns, cpp, ownerField, evidence[...]}]}
    cycles_report.md   strongly connected components with every edge that needs a resolution, an example cycle per edge and its
                       cycles.toml status; stale cycles.toml keys.
    escape_report.md   per class kind with the first escape reason (the escape graph), K5 classes, flagged fields.
Hand-owned inputs next to the outputs: fieldmap.toml ([settings] packet_bases/connection_bases/per_run_services/value_types (classes ported
as C++ value types: scalar-like, never referenced), [stored_callback_apis], [future_apis], [kinds], [immortal], [fields]; format in its
header) and
cycles.toml ([resolutions] "Class.field" / "Class$1#capture" / "Outer@L123#capture" = "part" | "java-hook: m" | "cpp-breaker: m" |
"zombie-safe: edge" | "accepted: why").

CLI
    python fieldmap.py [--roots R...] [--support-roots R...] [--out DIR] [--config fieldmap.toml] [--cycles cycles.toml]
                       [--staticdata-classes FILE] [--class FQN ...] [--no-write] [--check]
    --class prints the member block and generated callback structs for porters (several --class allowed); --check compares the outputs
    with the files on disk and exits 1 when they differ.
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
import re
import sys
import tomllib
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import javasrc  # noqa: E402

CPP_ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
REPO_ROOT = os.path.normpath(os.path.join(CPP_ROOT, '..'))
DEFAULT_ROOTS = [os.path.join(REPO_ROOT, 'game-server', 'src'), os.path.join(REPO_ROOT, 'game-server', 'data', 'handlers')]
DEFAULT_SUPPORT_ROOTS = [os.path.join(REPO_ROOT, 'commons', 'src')]
DEFAULT_OUT = os.path.join(CPP_ROOT, 'game-server', 'generated', 'concurrency')
DEFAULT_STATICDATA = os.path.join(CPP_ROOT, 'game-server', 'generated', 'staticdata-classes.json')

FORMAT_VERSION = 1
K1, K2, K3, K4, K5 = 'K1', 'K2', 'K3', 'K4', 'K5'
KIND_NAMES = {K1: 'STATIC_DATA', K2: 'PACKET', K3: 'IMMUTABLE_VALUE', K4: 'SHARED', K5: 'CONFINED'}
RESOLUTION_RE = re.compile(r'^(part|(java-hook|cpp-breaker|zombie-safe|accepted): \S.*)$')


class FieldmapError(Exception):
    pass


# ----------------------------------------------------------------------------------------------------------------------------------
# Java type tables
# ----------------------------------------------------------------------------------------------------------------------------------

PRIMS = {'int': 'int32_t', 'long': 'int64_t', 'short': 'int16_t', 'byte': 'int8_t', 'char': 'char16_t', 'boolean': 'bool',
         'float': 'float', 'double': 'double'}
BOXED = {'java.lang.Integer': 'int32_t', 'java.lang.Long': 'int64_t', 'java.lang.Short': 'int16_t', 'java.lang.Byte': 'int8_t',
         'java.lang.Character': 'char16_t', 'java.lang.Boolean': 'bool', 'java.lang.Float': 'float', 'java.lang.Double': 'double',
         'java.lang.Number': 'double'}
STRINGS = frozenset(('java.lang.String', 'java.lang.CharSequence'))
# concrete Java collection -> (shim, RefCounted alias or None, number of type arguments)
COLLECTION_IMPLS = {
    'java.util.ArrayList': ('ArrayList', 'RcArrayList', 1),
    'java.util.LinkedList': ('LinkedList', 'RcLinkedList', 1),
    'java.util.ArrayDeque': ('ArrayDeque', 'RcArrayDeque', 1),
    'java.util.HashMap': ('HashMap', 'RcHashMap', 2),
    'java.util.LinkedHashMap': ('LinkedHashMap', 'RcLinkedHashMap', 2),
    'java.util.TreeMap': ('TreeMap', 'RcTreeMap', 2),
    'java.util.HashSet': ('HashSet', 'RcHashSet', 1),
    'java.util.LinkedHashSet': ('LinkedHashSet', 'RcLinkedHashSet', 1),
    'java.util.TreeSet': ('TreeSet', 'RcTreeSet', 1),
    'java.util.EnumMap': ('EnumMap', None, 2),
    'java.util.PriorityQueue': ('PriorityQueue', None, 1),
    'java.util.concurrent.ConcurrentHashMap': ('ConcurrentHashMap', 'RcConcurrentHashMap', 2),
    'java.util.concurrent.CopyOnWriteArrayList': ('CopyOnWriteArrayList', 'RcCopyOnWriteArrayList', 1),
    'java.util.concurrent.CopyOnWriteArraySet': ('CopyOnWriteArraySet', None, 1),
    'java.util.concurrent.ConcurrentLinkedQueue': ('ConcurrentLinkedQueue', 'RcConcurrentLinkedQueue', 1),
    'java.util.concurrent.ConcurrentLinkedDeque': ('ConcurrentLinkedDeque', None, 1),
    '@ConcurrentKeySet': ('ConcurrentKeySet', 'RcConcurrentKeySet', 1),
}
COLLECTION_IFACES = {
    'java.util.List': 'java.util.ArrayList', 'java.util.Collection': 'java.util.ArrayList', 'java.lang.Iterable': 'java.util.ArrayList',
    'java.util.SequencedCollection': 'java.util.ArrayList', 'java.util.Map': 'java.util.HashMap', 'java.util.Set': 'java.util.HashSet',
    'java.util.Queue': 'java.util.ArrayDeque', 'java.util.Deque': 'java.util.ArrayDeque', 'java.util.SortedMap': 'java.util.TreeMap',
    'java.util.NavigableMap': 'java.util.TreeMap', 'java.util.SequencedMap': 'java.util.LinkedHashMap',
    'java.util.SortedSet': 'java.util.TreeSet', 'java.util.NavigableSet': 'java.util.TreeSet', 'java.util.SequencedSet': 'java.util.LinkedHashSet',
    'java.util.concurrent.ConcurrentMap': 'java.util.concurrent.ConcurrentHashMap',
}
# collections without a runtime shim (flagged 'noShim'; the member spelling keeps the Java name)
COLLECTION_OTHER = frozenset(('java.util.Vector', 'java.util.Stack', 'java.util.Hashtable', 'java.util.WeakHashMap', 'java.util.IdentityHashMap',
                              'java.util.EnumSet', 'java.util.BitSet', 'java.util.concurrent.ConcurrentSkipListMap',
                              'java.util.concurrent.ConcurrentSkipListSet', 'java.util.concurrent.LinkedBlockingQueue',
                              'java.util.concurrent.ArrayBlockingQueue', 'java.util.concurrent.DelayQueue', 'java.util.concurrent.BlockingQueue',
                              'java.util.concurrent.PriorityBlockingQueue', 'java.util.concurrent.LinkedBlockingDeque',
                              'java.util.concurrent.ConcurrentNavigableMap'))
ATOMICS = {'java.util.concurrent.atomic.AtomicBoolean': 'AtomicBoolean', 'java.util.concurrent.atomic.AtomicInteger': 'AtomicInteger',
           'java.util.concurrent.atomic.AtomicLong': 'AtomicLong', 'java.util.concurrent.atomic.AtomicReference': 'AtomicReference',
           'java.util.concurrent.atomic.AtomicLongArray': 'AtomicLongArray', 'java.util.concurrent.atomic.AtomicIntegerArray': 'AtomicIntegerArray',
           'java.util.concurrent.atomic.AtomicReferenceArray': 'AtomicReferenceArray'}
FUTURES = frozenset(('java.util.concurrent.Future', 'java.util.concurrent.ScheduledFuture', 'java.util.concurrent.RunnableScheduledFuture',
                     'java.util.concurrent.FutureTask', 'java.util.concurrent.CompletableFuture'))
LOCKS = {'java.util.concurrent.locks.ReentrantLock': 'Monitor', 'java.util.concurrent.locks.Lock': 'Monitor',
         'java.util.concurrent.locks.StampedLock': 'StampedLock', 'java.util.concurrent.Semaphore': 'Semaphore',
         'java.util.concurrent.locks.ReentrantReadWriteLock': 'Monitor', 'java.util.concurrent.locks.ReadWriteLock': 'Monitor'}
FUNCTIONAL = {  # fqn -> (return index or None for void / 'bool', parameter type-argument indexes)
    'java.lang.Runnable': ('void', ()), 'java.util.concurrent.Callable': (0, ()), 'java.util.function.Supplier': (0, ()),
    'java.util.function.Consumer': ('void', (0,)), 'java.util.function.BiConsumer': ('void', (0, 1)), 'java.util.function.Function': (1, (0,)),
    'java.util.function.BiFunction': (2, (0, 1)), 'java.util.function.Predicate': ('bool', (0,)), 'java.util.function.BiPredicate': ('bool', (0, 1)),
    'java.util.function.UnaryOperator': (0, (0,)), 'java.util.function.BinaryOperator': (0, (0, 0)),
    'java.util.function.IntConsumer': ('void', ('int32_t',)), 'java.util.function.IntSupplier': ('int32_t', ()),
    'java.util.function.BooleanSupplier': ('bool', ()), 'java.util.function.IntPredicate': ('bool', ('int32_t',)),
    'java.util.function.ToIntFunction': ('int32_t', (0,)), 'java.util.function.IntFunction': (0, ('int32_t',)),
    'java.util.Comparator': ('int32_t', (0, 0)),
}
LOGGERS = frozenset(('org.slf4j.Logger',))
THREADLOCALS = frozenset(('java.lang.ThreadLocal', 'java.lang.InheritableThreadLocal'))
OBJECT = 'java.lang.Object'
IMMUTABLE_EXTERNAL = frozenset(STRINGS | set(BOXED) | {
    'java.time.LocalDateTime', 'java.time.LocalDate', 'java.time.LocalTime', 'java.time.ZonedDateTime', 'java.time.Instant', 'java.time.Duration',
    'java.time.ZoneId', 'java.time.DayOfWeek', 'java.time.Month', 'java.math.BigInteger', 'java.math.BigDecimal', 'java.util.UUID',
    'java.util.regex.Pattern', 'java.util.Locale', 'java.lang.Class', 'java.util.concurrent.TimeUnit', 'java.net.InetAddress',
    'java.nio.file.Path', 'java.io.File', 'java.time.format.DateTimeFormatter', 'java.time.temporal.ChronoUnit', 'java.lang.Enum',
    'java.lang.Record', 'java.util.Optional', 'java.util.OptionalInt', 'javax.xml.bind.JAXBContext'})
# external types spelled differently in C++
EXTERNAL_SPELLING = {'java.util.concurrent.TimeUnit': 'TimeUnit', 'java.util.regex.Pattern': 'std::wregex', 'java.io.File': 'std::filesystem::path',
                     'java.nio.file.Path': 'std::filesystem::path'}
STORE_CALLS = frozenset(('add', 'put', 'putIfAbsent', 'offer', 'push', 'addFirst', 'addLast', 'set', 'addIfAbsent', 'offerFirst',
                         'offerLast', 'compareAndSet', 'getAndSet', 'lazySet'))
SPAWN_CALLS = frozenset(('storeObject', 'spawn', 'spawnObject', 'bringIntoWorld', 'addObject'))
JAXB_BOUND_FIELD_ANNOTATIONS = frozenset(('XmlElement', 'XmlElements', 'XmlAttribute', 'XmlElementWrapper', 'XmlList', 'XmlIDREF', 'XmlID',
                                          'XmlValue', 'XmlJavaTypeAdapter', 'XmlElementRef', 'XmlElementRefs', 'XmlAnyElement', 'XmlMixed'))
JAXB_ANNOTATIONS = frozenset(set(javasrc.KNOWN_EXTERNAL_PACKAGES['javax.xml.bind.annotation']) | {'XmlJavaTypeAdapter', 'XmlJavaTypeAdapters'})
CONFIG_ANNOTATIONS = frozenset(('Property', 'Properties'))
# field flags listed in escape_report.md (review items; 'guessedImpl', 'constantArray' and 'singleton' are informational)
FLAGS_REPORTED = frozenset(('confinedInShared', 'untyped', 'unresolved', 'noShim', 'externalType', 'nonFinalAtomic', 'nonFinalLock',
                            'callbackField', 'mutableInImmutable', 'instanceThreadLocal'))

DEFAULT_CONFIG = {
    'settings': {
        'packet_bases': ['com.aionemu.commons.network.packet.BasePacket'],
        'connection_bases': ['com.aionemu.commons.network.AConnection'],
        'per_run_services': [],
        'value_types': [],
    },
    'stored_callback_apis': {},
    'future_apis': {},
    'kinds': {},
    'immortal': {},
    'fields': {},
}


def _rel(path):
    try:
        return os.path.relpath(path, REPO_ROOT).replace('\\', '/')
    except ValueError:  # another drive
        return os.path.abspath(path).replace('\\', '/')


def _lower_camel(name):
    return name[:1].lower() + name[1:] if name else name


# ----------------------------------------------------------------------------------------------------------------------------------
# Model records
# ----------------------------------------------------------------------------------------------------------------------------------

class ClassInfo:
    __slots__ = ('cid', 'td', 'cu', 'origin', 'lex_parent', 'root_span', 'new_expr', 'const', 'supers', 'superclass', 'subs', 'kind',
                 'kind_reason', 'fields', 'captures', 'cpp_name', 'static_context', 'part_of', 'escape', 'k1_reason', 'group', 'callbacks',
                 'immutable', 'storage', 'singleton', 'runtime_mutable', 'anon_seq', 'external_super', 'value_type', 'immortal')

    def __init__(self, cid, td, cu, origin):
        self.cid, self.td, self.cu, self.origin = cid, td, cu, origin
        self.lex_parent = None
        self.root_span = None
        self.new_expr = None
        self.const = None
        self.supers = []
        self.superclass = None
        self.subs = []
        self.kind = None
        self.kind_reason = ''
        self.fields = []
        self.captures = []
        self.cpp_name = ''
        self.static_context = False
        self.part_of = []
        self.escape = None
        self.k1_reason = None
        self.group = None
        self.callbacks = []
        self.immutable = False
        self.storage = None
        self.singleton = False
        self.runtime_mutable = set()
        self.anon_seq = 0
        self.external_super = False
        self.value_type = False
        self.immortal = False  # fieldmap.toml [immortal]: interned instances, never reclaimed

    @property
    def is_interface(self):
        return self.td.kind in ('interface', 'annotation')

    @property
    def is_enum(self):
        return self.td.kind == 'enum'

    @property
    def scalar_like(self):
        """Enums and configured value types: copied, never referenced (no Ref, no escape, no retention)."""
        return self.td.kind == 'enum' or self.value_type

    @property
    def simple(self):
        return self.td.name or self.cpp_name

    @property
    def file(self):
        return _rel(self.cu.path)

    def __repr__(self):
        return f'ClassInfo({self.cid})'


class RootSpan:
    __slots__ = ('span', 'ci', 'member', 'static', '_lambda_starts', '_ident_idx', '_args', '_rhs', '_local_init')

    def __init__(self, span, ci, member, static):
        self.span, self.ci, self.member, self.static = span, ci, member, static
        self._lambda_starts = None
        self._ident_idx = None
        self._args = None
        self._rhs = None
        self._local_init = None


class FieldInfo:
    __slots__ = ('ci', 'decl', 'name', 'static', 'final', 'volatile', 'eff_final', 'jt', 'cpp', 'rule', 'flags', 'retains', 'part',
                 'config', 'line', 'record_component', 'bound', 'java', 'override_reason', 'transient')

    def __init__(self, ci, decl, name, line, java):
        self.ci, self.decl, self.name, self.line, self.java = ci, decl, name, line, java
        self.static = self.final = self.volatile = self.eff_final = False
        self.jt = None
        self.cpp = None
        self.rule = ''
        self.flags = []
        self.retains = []
        self.part = None
        self.config = False
        self.record_component = False
        self.bound = False
        self.override_reason = None
        self.transient = False


class JType:
    """Classified Java type: cat in prim, boxed, string, enum, class, coll, array, atomic, future, lock, threadlocal, functional, logger,
    object, ext, unknown. For 'class' ci is the project ClassInfo; tvar names a type variable (bound in ci when resolvable)."""
    __slots__ = ('cat', 'fqn', 'ci', 'args', 'name', 'tvar', 'wildcard_args')

    def __init__(self, cat, fqn=None, ci=None, args=(), name=None, tvar=None, wildcard_args=False):
        self.cat, self.fqn, self.ci, self.args, self.name, self.tvar, self.wildcard_args = cat, fqn, ci, list(args), name, tvar, wildcard_args

    def __repr__(self):
        return f'JType({self.cat} {self.fqn or self.name} {self.args})'


class Capture:
    __slots__ = ('name', 'kind', 'java', 'jt', 'ci', 'line', 'cpp', 'rule')

    def __init__(self, name, kind, java, jt, ci, line):
        self.name, self.kind, self.java, self.jt, self.ci, self.line = name, kind, java, jt, ci, line
        self.cpp = None
        self.rule = ''


class Callback:
    __slots__ = ('id', 'kind', 'rs', 'start', 'end', 'line', 'ci', 'anon', 'context', 'api', 'storage', 'owners', 'captures', 'future_holder',
                 'escape_args')

    def __init__(self, cid, kind, rs, start, end, line, ci):
        self.id, self.kind, self.rs, self.start, self.end, self.line, self.ci = cid, kind, rs, start, end, line, ci
        self.anon = None
        self.context = ''
        self.api = ''
        self.storage = 'sync'
        self.owners = []
        self.captures = []
        self.future_holder = ''
        self.escape_args = []


class PartInfo:
    __slots__ = ('owner', 'field', 'patterns', 'evidence', 'part_types', 'element', 'owner_classes', 'dims')

    def __init__(self, owner, field):
        self.owner, self.field = owner, field
        self.patterns = set()
        self.evidence = []
        self.part_types = set()
        self.element = False
        self.owner_classes = set()
        self.dims = None


# ----------------------------------------------------------------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------------------------------------------------------------

def load_config(path):
    cfg = json.loads(json.dumps(DEFAULT_CONFIG))
    if path is None or not os.path.exists(path):
        return cfg
    with open(path, 'rb') as f:
        data = tomllib.load(f)
    unknown = set(data) - set(cfg)
    if unknown:
        raise FieldmapError(f'{path}: unknown sections {sorted(unknown)}')
    for k, v in data.get('settings', {}).items():
        if k not in cfg['settings']:
            raise FieldmapError(f'{path}: unknown setting {k!r}')
        cfg['settings'][k] = v
    for sec in ('stored_callback_apis', 'future_apis'):
        for name, entry in data.get(sec, {}).items():
            if not isinstance(entry, dict):
                raise FieldmapError(f'{path}: [{sec}] {name} must be a table')
            extra = set(entry) - {'owner', 'receiver', 'reason'}
            if extra:
                raise FieldmapError(f'{path}: [{sec}] {name}: unknown keys {sorted(extra)}')
            if sec == 'stored_callback_apis' and 'owner' not in entry:
                raise FieldmapError(f'{path}: [{sec}] {name}: owner is required')
            cfg[sec][name] = entry
    for key, entry in data.get('kinds', {}).items():
        if entry.get('kind') not in (K1, K2, K3, K4, K5) or not entry.get('reason'):
            raise FieldmapError(f'{path}: [kinds] {key!r} needs kind = K1..K5 and a reason')
        extra = set(entry) - {'kind', 'reason'}
        if extra:
            raise FieldmapError(f'{path}: [kinds] {key!r}: unknown keys {sorted(extra)}')
        cfg['kinds'][key] = entry
    for key, entry in data.get('immortal', {}).items():
        if not isinstance(entry, dict) or not entry.get('reason') or set(entry) - {'reason'}:
            raise FieldmapError(f'{path}: [immortal] {key!r} needs exactly a reason')
        cfg['immortal'][key] = entry
    for key, entry in data.get('fields', {}).items():
        if not entry.get('reason'):
            raise FieldmapError(f'{path}: [fields] {key!r} needs a reason')
        extra = set(entry) - {'cpp', 'part', 'retire', 'reason', 'part_type'}
        if extra:
            raise FieldmapError(f'{path}: [fields] {key!r}: unknown keys {sorted(extra)}')
        if entry.get('retire') not in (None, 'OWNER', 'RECLAIMER'):
            raise FieldmapError(f'{path}: [fields] {key!r}: retire must be OWNER or RECLAIMER')
        cfg['fields'][key] = entry
    return cfg


def load_cycles(path):
    if path is None or not os.path.exists(path):
        return {}
    with open(path, 'rb') as f:
        data = tomllib.load(f)
    extra = set(data) - {'resolutions'}
    if extra:
        raise FieldmapError(f'{path}: unknown sections {sorted(extra)}')
    out = {}
    for key, value in data.get('resolutions', {}).items():
        if not isinstance(value, str) or not RESOLUTION_RE.match(value):
            raise FieldmapError(f'{path}: resolution of {key!r} must be part | java-hook: m | cpp-breaker: m | zombie-safe: edge | accepted: why')
        out[key] = value
    return out


def load_staticdata_classes(path):
    """Reads xmlgen's staticdata-classes.json. Accepted shapes: [fqn...], {classes: [fqn | {fqn, runtimeMutable|runtime_mutable: [...]}]},
    {classes: {fqn: {runtimeMutable: [...]}}}. Returns {fqn: set(runtime-mutable field names)}."""
    with open(path, encoding='utf-8') as f:
        data = json.load(f)
    classes = data.get('classes') if isinstance(data, dict) else data
    out = {}
    if isinstance(classes, dict):
        items = [(k, v) for k, v in classes.items()]
    elif isinstance(classes, list):
        items = []
        for c in classes:
            if isinstance(c, str):
                items.append((c, {}))
            elif isinstance(c, dict) and 'fqn' in c:
                items.append((c['fqn'], c))
            else:
                raise FieldmapError(f'{path}: unexpected class entry {c!r}')
    else:
        raise FieldmapError(f'{path}: expected a class list')
    for fqn, info in items:
        rm = (info or {}).get('runtimeMutable', (info or {}).get('runtime_mutable', []))
        names = set()
        for r in rm:
            names.add(r if isinstance(r, str) else r.get('name'))
        out[fqn] = names
    return out


# ----------------------------------------------------------------------------------------------------------------------------------
# The analysis
# ----------------------------------------------------------------------------------------------------------------------------------

class FieldMap:
    def __init__(self, idx, output_roots, config=None, cycles=None, staticdata=None, staticdata_source='heuristic'):
        self.idx = idx
        self.output_roots = [os.path.normcase(os.path.abspath(r)) for r in output_roots]
        self.cfg = config or load_config(None)
        self.resolutions = cycles or {}
        self.staticdata = staticdata
        self.staticdata_source = staticdata_source
        self.classes = {}
        self.ci_of = {}
        self.root_spans = []
        self.spans_by_cu = {}
        self.callbacks = {}
        self.parts = {}
        self.owner_field_rules = {}
        self.sync_counts = {}
        self.cycle_edges = {}
        self.cycle_sccs = []
        self.stale_resolutions = []
        self.warnings = []
        self._field_cache = {}
        self._method_cache = {}
        self._stored_param_cache = {}
        self._storing_by_name = None
        self.rs_of_method = {}
        self._all_subtypes_cache = {}

    # -- driver
    def run(self):
        self._collect()
        self._link_supertypes()
        self._collect_fields()
        self._effectively_final()
        self._detect_parts()
        self._callbacks()
        self._inner_class_captures()
        self._kinds()
        self._map_all()
        self._sync()
        self._cycles()
        return self

    # -- collection
    def _origin(self, cu):
        p = os.path.normcase(os.path.abspath(cu.path))
        for r in self.output_roots:
            if p.startswith(r + os.sep):
                return 'output'
        return 'support'

    def _add(self, cid, td, cu, origin):
        if cid in self.classes:
            raise FieldmapError(f'duplicate class id {cid} ({cu.path})')
        ci = ClassInfo(cid, td, cu, origin)
        self.classes[cid] = ci
        self.ci_of[id(td)] = ci
        return ci

    def _collect(self):
        units = sorted(self.idx.units, key=lambda u: u.path.replace('\\', '/'))
        for cu in units:
            origin = self._origin(cu)
            spans = []
            stack = [(td, None) for td in reversed(cu.types)]
            while stack:
                td, parent = stack.pop()
                if parent is None:
                    cid = td.fqn
                elif td.anonymous:  # enum constant body
                    cid = f'{parent.cid}${self._const_name(td)}'
                else:
                    cid = f'{parent.cid}.{td.name}'
                if td.fqn is not None and cid != td.fqn:
                    cid = td.fqn
                ci = self._add(cid, td, cu, origin)
                ci.lex_parent = parent
                if parent is None:
                    ci.cpp_name = td.name
                elif td.anonymous:
                    ci.cpp_name = f'{parent.cpp_name.replace("::", "_")}_{self._const_name(td)}'
                    ci.const = self._const_name(td)
                else:
                    ci.cpp_name = f'{parent.cpp_name}::{td.name}'
                for m in td.methods:
                    if m.body is not None:
                        spans.append(RootSpan(m.body, ci, m, 'static' in m.modifiers))
                for f in td.fields:
                    if f.initializer is not None:
                        spans.append(RootSpan(f.initializer, ci, f, 'static' in f.modifiers or td.kind == 'interface'))
                for ini in td.initializers:
                    spans.append(RootSpan(ini.body, ci, ini, ini.static))
                for c in td.enum_constants:
                    for a in c.args or []:
                        spans.append(RootSpan(a, ci, c, True))
                for n in reversed(td.types):
                    stack.append((n, ci))
                for c in reversed(td.enum_constants):
                    if c.body is not None:
                        stack.append((c.body, ci))
            spans.sort(key=lambda r: r.span.start)
            self.spans_by_cu[id(cu)] = spans
            for r in spans:
                if isinstance(r.member, javasrc.MethodDecl):
                    self.rs_of_method[id(r.member)] = r
            self.root_spans.extend(spans)
            # anonymous and local classes found in the root spans
            counters = {}
            for rs in spans:
                sp = rs.span
                found = [(ne.index, 'anon', ne) for ne in sp.anonymous_classes()] + [(lt.start, 'local', lt) for lt in sp.local_types()]
                found.sort(key=lambda x: x[0])
                for pos, what, obj in found:
                    td = obj.anonymous if what == 'anon' else obj
                    if id(td) in self.ci_of:
                        continue
                    named = rs.ci
                    while named.td.anonymous or named.td.local:
                        named = named.lex_parent
                    n = counters.get(named.cid, 0) + 1
                    counters[named.cid] = n
                    base = named.cid
                    cid = f'{base}${n}' if what == 'anon' else f'{base}${n}{td.name}'
                    ci = self._add(cid, td, cu, origin)
                    ci.root_span = rs
                    ci.static_context = rs.static
                    ci.anon_seq = n
                    inner = sp.enclosing_class(pos)
                    ci.lex_parent = self.ci_of[id(inner)] if inner is not None else rs.ci
                    outer_simple = named.cpp_name.replace('::', '_')
                    if what == 'anon':
                        ci.new_expr = obj
                        base_name = obj.type.segments[-1][0]
                        ci.cpp_name = f'{outer_simple}_{base_name}'
                    else:
                        ci.cpp_name = f'{outer_simple}_{td.name}'
                    # member types of anonymous/local classes
                    st = [(x, ci) for x in td.types]
                    while st:
                        x, par = st.pop()
                        xi = self._add(f'{par.cid}.{x.name}', x, cu, origin)
                        xi.lex_parent = par
                        xi.cpp_name = f'{par.cpp_name}_{x.name}'
                        st.extend((y, xi) for y in x.types)
        # disambiguate generated struct names
        by_name = {}
        for ci in self.classes.values():
            if ci.td.anonymous and ci.const is None:
                by_name.setdefault(ci.cpp_name, []).append(ci)
        for name, cis in by_name.items():
            if len(cis) > 1:
                cis.sort(key=lambda c: (c.cu.path, c.td.start))
                for i, c in enumerate(cis[1:], 2):
                    c.cpp_name = f'{name}_{i}'

    @staticmethod
    def _const_name(td):
        return td.cu.tokens.text[td.index]

    def _link_supertypes(self):
        for fqn in self.cfg['settings']['value_types']:
            if fqn not in self.classes:
                raise FieldmapError(f'fieldmap.toml: settings.value_types: unknown class {fqn}')
            self.classes[fqn].value_type = True
        for ci in self.classes.values():
            td = ci.td
            sups = []
            for s in self.idx.supertypes(td):
                si = self.ci_of.get(id(s))
                if si is not None:
                    sups.append(si)
            if td.anonymous and ci.const is not None and not sups:
                sups.append(ci.lex_parent)
            ci.supers = sups
            if td.kind in ('class', 'anonymous', 'enum', 'record'):
                for s in sups:
                    if s.td.kind == 'class' or (s.td.kind == 'enum' and ci.const is not None) or s.td.kind == 'anonymous':
                        ci.superclass = s
                        break
            refs = list(td.extends) + (list(td.implements) if td.kind != 'anonymous' else [])
            if td.kind in ('class', 'anonymous') and td.extends:
                kind, _ = self.idx.resolve_kind(td.extends[0].name, ci.lex_parent.td if td.anonymous and ci.lex_parent else td)
                if kind not in ('project', 'local') and td.extends[0].name not in ('Object', 'java.lang.Object'):
                    if not (td.anonymous and ci.superclass is None and self._is_external_interface(td.extends[0], ci)):
                        ci.external_super = True
            del refs
        for ci in self.classes.values():
            for s in ci.supers:
                s.subs.append(ci)
        for ci in self.classes.values():
            ci.subs.sort(key=lambda c: c.cid)

    def _is_external_interface(self, ref, ci):
        fqn = self.idx.resolve(ref.name, ci.lex_parent.td if ci.lex_parent else ci.td)
        return fqn in FUNCTIONAL or fqn in ('java.lang.Runnable', 'java.util.Comparator', 'java.lang.Comparable', 'java.lang.Iterable',
                                            'java.util.Iterator') or (fqn is not None and fqn.startswith('java.util.function.'))

    def all_subtypes(self, ci):
        r = self._all_subtypes_cache.get(ci.cid)
        if r is None:
            out = []
            seen = {ci.cid}
            stack = [ci]
            while stack:
                c = stack.pop()
                out.append(c)
                for s in c.subs:
                    if s.cid not in seen:
                        seen.add(s.cid)
                        stack.append(s)
            r = out
            self._all_subtypes_cache[ci.cid] = r
        return r

    def superclass_chain(self, ci):
        out = []
        c = ci
        seen = set()
        while c is not None and c.cid not in seen:
            seen.add(c.cid)
            out.append(c)
            c = c.superclass
        return out

    # -- types
    def classify(self, ref, ctx, ci_ctx=None):
        if ref is None:
            return JType('unknown', name='?')
        if ref.wildcard:
            if ref.wildcard == 'extends' and ref.bound is not None:
                return self.classify(ref.bound, ctx, ci_ctx)
            return JType('object', fqn=OBJECT, name='Object')
        if ref.dims:
            jt = self.classify(javasrc.TypeRef(ref.segments, 0, ref.annotations, None, None, ref.line, ref.col, ref.index), ctx, ci_ctx)
            for _ in range(ref.dims):
                jt = JType('array', args=[jt], name=f'{jt.name}[]')
            return jt
        if ref.is_primitive:
            return JType('prim', name=ref.name)
        if ref.name == 'var':
            return JType('unknown', name='var')
        kind, value = self.idx.resolve_kind(ref.name, ctx)
        args = ref.args
        wildcard_args = bool(args) and any(a.wildcard for a in args)
        jargs = [self.classify(a, ctx, ci_ctx) for a in (args or [])]
        if kind == 'typevar':
            bound = self._tvar_bound(value, ctx, ci_ctx)
            if bound is not None:
                b = self.classify(bound[0], bound[1], ci_ctx)
                b.tvar = value
                return b
            return JType('object', fqn=OBJECT, name=value, tvar=value)
        if kind == 'project' or kind == 'local':
            td = self.idx.types[value] if kind == 'project' else value
            ci = self.ci_of.get(id(td))
            if ci is None:
                return JType('unknown', name=ref.name)
            if td.kind == 'enum' or ci.value_type:
                return JType('enum', fqn=value if kind == 'project' else ci.cid, ci=ci, name=ref.name)
            return JType('class', fqn=ci.cid, ci=ci, args=jargs, name=ref.name, wildcard_args=wildcard_args)
        if kind in ('external', 'external_guess'):
            fqn = value
            if fqn in STRINGS:
                return JType('string', fqn=fqn, name='String')
            if fqn in BOXED:
                return JType('boxed', fqn=fqn, name=ref.name)
            if fqn in COLLECTION_IMPLS or fqn in COLLECTION_IFACES or fqn in COLLECTION_OTHER:
                return JType('coll', fqn=fqn, args=jargs, name=ref.name)
            if fqn in ATOMICS:
                return JType('atomic', fqn=fqn, args=jargs, name=ref.name)
            if fqn in FUTURES:
                return JType('future', fqn=fqn, args=jargs, name=ref.name)
            if fqn in LOCKS:
                return JType('lock', fqn=fqn, name=ref.name)
            if fqn in THREADLOCALS:
                return JType('threadlocal', fqn=fqn, args=jargs, name=ref.name)
            if fqn in FUNCTIONAL or fqn.startswith('java.util.function.'):
                return JType('functional', fqn=fqn, args=jargs, name=ref.name)
            if fqn in LOGGERS:
                return JType('logger', fqn=fqn, name=ref.name)
            if fqn == OBJECT:
                return JType('object', fqn=fqn, name='Object')
            return JType('ext', fqn=fqn, args=jargs, name=ref.name)
        return JType('unknown', name=ref.name)

    def _tvar_bound(self, name, ctx, ci_ctx):
        """(bound TypeRef, resolution context) of type variable name visible from ctx, or None."""
        md = None
        o = ctx
        if isinstance(o, javasrc.Span):
            o = o.owner
        if isinstance(o, javasrc.MethodDecl):
            md = o
            for p in md.type_params:
                if p.name == name:
                    return (p.bounds[0], md) if p.bounds else None
            o = md.owner
        elif isinstance(o, (javasrc.FieldDecl, javasrc.Initializer, javasrc.EnumConstant)):
            o = o.owner
        td = o if isinstance(o, javasrc.TypeDecl) else (ci_ctx.td if ci_ctx is not None else None)
        ci = self.ci_of.get(id(td)) if td is not None else None
        while ci is not None:
            for p in ci.td.type_params:
                if p.name == name:
                    return (p.bounds[0], ci.td) if p.bounds else None
            ci = ci.lex_parent
        return None

    def cpp_class_name(self, ci, jt=None):
        name = ci.cpp_name
        if jt is not None and jt.args and not jt.wildcard_args and ci.td.type_params:
            name += '<' + ', '.join(self.spell_plain(a) for a in jt.args) + '>'
        return name

    # -- fields
    def _collect_fields(self):
        for ci in self.classes.values():
            td = ci.td
            iface = td.kind in ('interface', 'annotation')
            for fd in td.fields:
                fi = FieldInfo(ci, fd, fd.name, fd.line, str(fd.type))
                mods = fd.modifiers
                fi.static = 'static' in mods or iface
                fi.final = 'final' in mods or iface
                fi.volatile = 'volatile' in mods
                fi.transient = 'transient' in mods
                fi.config = any(a.simple_name in CONFIG_ANNOTATIONS for a in fd.annotations)
                fi.jt = self.classify(fd.type, fd, ci)
                ci.fields.append(fi)
            for p in td.record_components:
                fi = FieldInfo(ci, p, p.name, p.line, str(p.type))
                fi.final = True
                fi.record_component = True
                fi.jt = self.classify(p.type, td, ci)
                ci.fields.append(fi)

    def find_field(self, ci, name, _seen=None):
        key = (ci.cid, name)
        if key in self._field_cache:
            return self._field_cache[key]
        res = None
        for fi in ci.fields:
            if fi.name == name:
                res = fi
                break
        if res is None:
            for c in ci.td.enum_constants:
                if c.name == name:
                    res = 'enum-constant'
                    break
        if res is None:
            seen = _seen or set()
            seen.add(ci.cid)
            for s in ci.supers:
                if s.cid in seen:
                    continue
                r = self.find_field(s, name, seen)
                if r is not None:
                    res = r
                    break
        self._field_cache[key] = res
        return res

    def find_methods(self, ci, name, _seen=None):
        key = (ci.cid, name)
        if key in self._method_cache:
            return self._method_cache[key]
        res = [(ci, m) for m in ci.td.methods if m.name == name and m.kind == 'method']
        seen = _seen or set()
        seen.add(ci.cid)
        for s in ci.supers:
            if s.cid not in seen:
                res.extend(x for x in self.find_methods(s, name, seen) if x not in res)
        self._method_cache[key] = res
        return res

    def has_external_super(self, ci):
        return any(c.external_super for c in self.superclass_chain(ci))

    # -- span helpers
    def spans_of_cu(self, cu):
        return self.spans_by_cu[id(cu)]

    @staticmethod
    def in_lambda(rs, i):
        for lam in rs.span.lambdas():
            if lam.body.start <= i < lam.body.end:
                return True
        return False

    def top_level(self, rs, i):
        return rs.span.enclosing_class(i) is None and not self.in_lambda(rs, i)

    @staticmethod
    def new_exact(rs, s, e):
        for ne in rs.span.new_expressions():
            if ne.span.start == s and ne.span.end == e:
                return ne
        return None

    @staticmethod
    def has_arg_text(ne, text):
        return any(a.texts() == [text] for a in (ne.args or []))

    def class_at(self, rs, i):
        td = rs.span.enclosing_class(i)
        return self.ci_of[id(td)] if td is not None else rs.ci

    # -- effectively final
    def _effectively_final(self):
        writes = {}  # (cid, name) -> True when written outside constructors/initializers of the declaring class
        names_by_cu = {}
        for ci in self.classes.values():
            for fi in ci.fields:
                if not fi.final and not fi.record_component and 'private' in getattr(fi.decl, 'modifiers', ()):
                    names_by_cu.setdefault(id(ci.cu), set()).add(fi.name)
        for rs in self.root_spans:
            names = names_by_cu.get(id(rs.span.cu))
            if not names:
                continue
            for a in rs.span.field_assignments():
                if a.name not in names or a.element:
                    continue  # `a[i] = v` / `a[i]++` writes an element, not the field
                inner = rs.span.enclosing_class(a.index)
                if a.qualifier not in (None, 'this'):
                    for fi in self._private_fields_named(rs.span.cu, a.name):
                        writes[(fi.ci.cid, fi.name)] = True
                    continue
                cls = self.ci_of[id(inner)] if inner is not None else rs.ci
                found = None
                c = cls
                while c is not None:
                    r = self.find_field(c, a.name)
                    if isinstance(r, FieldInfo):
                        found = r
                        break
                    c = c.lex_parent
                if found is None:
                    continue
                ok = False
                if inner is None and not self.in_lambda(rs, a.index) and found.ci is rs.ci:
                    m = rs.member
                    if isinstance(m, javasrc.MethodDecl) and m.kind in ('constructor', 'compact_constructor') and not found.static:
                        ok = True
                    elif isinstance(m, javasrc.Initializer) and m.static == found.static:
                        ok = True
                    elif isinstance(m, javasrc.FieldDecl) and ('static' in m.modifiers) == found.static:
                        ok = True
                elif inner is not None and found.ci is cls and not self._in_lambda_within(rs, a.index, inner):
                    # anonymous/local class writing its own field inside its initializer block or field initializer
                    for ini in inner.initializers:
                        if ini.body.start <= a.index < ini.body.end and ini.static == found.static:
                            ok = True
                    for fdd in inner.fields:
                        if fdd.initializer is not None and fdd.initializer.start <= a.index < fdd.initializer.end:
                            ok = True
                if not ok:
                    writes[(found.ci.cid, found.name)] = True
        for ci in self.classes.values():
            for fi in ci.fields:
                if fi.final or fi.record_component:
                    continue
                if 'private' in fi.decl.modifiers and not fi.volatile and (ci.cid, fi.name) not in writes:
                    fi.eff_final = True

    def _private_fields_named(self, cu, name):
        out = []
        for ci in self.classes.values():
            if ci.cu is cu:
                for fi in ci.fields:
                    if fi.name == name and not fi.record_component and 'private' in fi.decl.modifiers:
                        out.append(fi)
        return out

    def _in_lambda_within(self, rs, i, td):
        for lam in rs.span.lambdas():
            if lam.body.start <= i < lam.body.end and td.body.start < lam.span.start:
                return True
        return False

    def is_final(self, fi):
        return fi.final or fi.eff_final

    # -- parts
    def _part(self, fi, pattern, part_ci, evidence, owner_ci, element=False):
        key = (fi.ci.cid, fi.name)
        p = self.parts.get(key)
        if p is None:
            p = PartInfo(fi.ci, fi)
            self.parts[key] = p
        p.patterns.add(pattern)
        if part_ci is not None:
            p.part_types.add(part_ci.cid)
        if fi.jt is not None:
            base = fi.jt.args[0] if fi.jt.cat == 'array' and fi.jt.args else fi.jt
            if base.cat == 'class':
                p.part_types.add(base.ci.cid)
        if len(p.evidence) < 8 and evidence not in p.evidence:
            p.evidence.append(evidence)
        p.owner_classes.add(owner_ci.cid)
        if element:
            p.element = True

    def _evidence(self, rs, i, pattern):
        tk = rs.span.cu.tokens
        line = tk.loc(i)[0]
        text = tk.source.splitlines()[line - 1].strip() if tk.source else ''
        return {'file': _rel(rs.span.cu.path), 'line': line, 'pattern': pattern, 'text': text[:160]}

    def _new_class(self, ne, rs):
        kind, value = self.idx.resolve_kind(ne.type.name, rs.span)
        if kind == 'project':
            return self.ci_of.get(id(self.idx.types[value]))
        if kind == 'local':
            return self.ci_of.get(id(value))
        return None

    def _field_in_chain(self, ci, name):
        r = self.find_field(ci, name)
        return r if isinstance(r, FieldInfo) else None

    def _detect_parts(self):
        for key, ov in sorted(self.cfg['fields'].items()):
            cid, _, fname = key.rpartition('.')
            ci = self.classes.get(cid)
            fi = next((f for f in ci.fields if f.name == fname), None) if ci is not None else None
            if fi is None:
                raise FieldmapError(f'fieldmap.toml: [fields] entry for unknown field {key}')
            if ov.get('part'):
                pci = None
                if ov.get('part_type'):
                    pci = self.classes.get(ov['part_type'])
                    if pci is None:
                        raise FieldmapError(f'fieldmap.toml: {key}: unknown part_type {ov["part_type"]}')
                self._part(fi, 4, pci, {'file': 'fieldmap.toml', 'line': 0, 'pattern': 4, 'text': ov['reason']}, ci)
        rs_of_method = {id(rs.member): rs for rs in self.root_spans if isinstance(rs.member, javasrc.MethodDecl)}
        for rs in self.root_spans:
            if rs.ci.origin != 'output':
                continue
            sp = rs.span
            m = rs.member
            is_ctor = isinstance(m, javasrc.MethodDecl) and m.kind in ('constructor', 'compact_constructor')
            if isinstance(m, javasrc.FieldDecl) and not rs.static:
                ne = self.new_exact(rs, sp.start, sp.end)
                if ne is not None and ne.anonymous is None and self.has_arg_text(ne, 'this'):
                    fi = self._field_in_chain(rs.ci, m.name)
                    if fi is not None:
                        self._part(fi, 1, self._new_class(ne, rs), self._evidence(rs, ne.index, 1), rs.ci)
                elif ne is not None and ne.array:
                    pass
            if is_ctor:
                self._ctor_part_scan(rs, rs.ci, rs_of_method, 0, set())
            # pattern 2 on a freshly created local: r = new T(..); r.setF(new X(r..)) before r is stored
            self._fresh_local_parts(rs)
        self._owner_fields()
        self._owner_field_writes()

    def _owner_field_writes(self):
        """Owner fields (rule 'owner') that are not final and assigned outside the constructors (and instance initializers) of the part
        class or its subclasses: those hold foreign objects too (SelfOrRef). Non-private fields are searched in every root span; a qualified
        write (`x.owner = ..`) of a non-private owner field counts wherever its name matches (conservative)."""
        self.owner_written_outside = set()
        targets = {}
        for (cid, name), rule in self.owner_field_rules.items():
            if rule[0] != 'owner':
                continue
            fi = self._field_in_chain(self.classes[cid], name)
            if fi is None or fi.final or fi.eff_final or fi.static:
                continue
            targets.setdefault(name, []).append(fi)
        if not targets:
            return
        for rs in self.root_spans:
            for a in rs.span.field_assignments():
                cands = targets.get(a.name)
                if not cands or a.element:
                    continue
                if a.qualifier not in (None, 'this', 'super'):
                    for fi in cands:
                        if 'private' not in fi.decl.modifiers or fi.ci.cu is rs.span.cu:
                            self.owner_written_outside.add((fi.ci.cid, fi.name))
                    continue
                inner = rs.span.enclosing_class(a.index)
                cls = self.ci_of[id(inner)] if inner is not None else rs.ci
                found = None
                c = cls
                while c is not None:
                    r = self.find_field(c, a.name)
                    if isinstance(r, FieldInfo):
                        found = r
                        break
                    c = c.lex_parent
                if found is None or not any(found is fi for fi in cands):
                    continue
                m = rs.member
                in_ctor = inner is None and not self.in_lambda(rs, a.index) and rs.ci in self.all_subtypes(found.ci) and (
                    (isinstance(m, javasrc.MethodDecl) and m.kind in ('constructor', 'compact_constructor'))
                    or (isinstance(m, javasrc.Initializer) and not m.static) or (isinstance(m, javasrc.FieldDecl) and 'static' not in m.modifiers))
                if not in_ctor:
                    self.owner_written_outside.add((found.ci.cid, found.name))

    def _ctor_part_scan(self, rs, owner, rs_of_method, depth, seen):
        """Patterns 1-3 in a constructor of owner, and in the owner's own methods it calls (setupStatContainers(), depth 2)."""
        sp = rs.span
        for a in sp.field_assignments():
            if a.op != '=' or a.rhs is None or a.qualifier not in (None, 'this') or not self.top_level(rs, a.index):
                continue
            ne = self.new_exact(rs, a.rhs.start, a.rhs.end)
            if ne is None or ne.anonymous is not None or not self.has_arg_text(ne, 'this'):
                continue
            fi = self._field_in_chain(owner, a.name)
            if fi is None or fi.static:
                continue
            self._part(fi, 1, self._new_class(ne, rs), self._evidence(rs, a.index, 1), owner, element=a.element)
        for c in sp.method_calls():
            if not self.top_level(rs, c.index):
                continue
            if c.receiver is not None and c.receiver.texts() != ['this']:
                rtexts = c.receiver.texts()
            else:
                rtexts = None
            if rtexts is None and c.name.startswith('set') and len(c.name) > 3 and len(c.args) == 1:
                ne = self.new_exact(rs, c.args[0].start, c.args[0].end)
                if ne is not None and ne.anonymous is None and self.has_arg_text(ne, 'this'):
                    fi = self._setter_field(owner, c.name)
                    if fi is not None and not fi.static:
                        self._part(fi, 2, self._new_class(ne, rs), self._evidence(rs, c.index, 2), owner)
            if c.name == 'setOwner' and len(c.args) == 1 and c.args[0].texts() == ['this'] and c.receiver is not None:
                fi = self._receiver_field(rs, c.receiver)
                if fi is not None and not fi.static:
                    self._part(fi, 3, None, self._evidence(rs, c.index, 3), owner)
            if depth < 2 and rtexts is None and c.name not in ('super', 'this'):
                for mci, md in self.find_methods(owner, c.name):
                    hr = rs_of_method.get(id(md))
                    if hr is None or 'static' in md.modifiers or len(md.params) != len(c.args) or id(md) in seen:
                        continue
                    seen.add(id(md))
                    self._ctor_part_scan(hr, owner, rs_of_method, depth + 1, seen)

    def _fresh_local_parts(self, rs):
        sp = rs.span
        locals_ = [v for v in sp.local_variables() if v.kind == 'local' and v.initializer is not None]
        if not locals_:
            return
        calls = sp.method_calls()
        for v in locals_:
            ne0 = self.new_exact(rs, v.initializer.start, v.initializer.end)
            if ne0 is None or ne0.anonymous is not None:
                continue
            tci = self._new_class(ne0, rs)
            if tci is None:
                continue
            stored_at = None
            for c in calls:
                if c.index <= v.index or c.index >= v.scope_end:
                    continue
                if c.name in SPAWN_CALLS and any(a.texts() == [v.name] for a in c.args):
                    stored_at = c.index if stored_at is None else min(stored_at, c.index)
            for c in calls:
                if c.index <= v.index or c.index >= v.scope_end or (stored_at is not None and c.index > stored_at):
                    continue
                if c.receiver is None or c.receiver.texts() != [v.name] or not c.name.startswith('set') or len(c.args) != 1:
                    continue
                ne = self.new_exact(rs, c.args[0].start, c.args[0].end)
                if ne is None or ne.anonymous is not None or not self.has_arg_text(ne, v.name):
                    continue
                fi = self._setter_field(tci, c.name)
                if fi is not None and not fi.static:
                    self._part(fi, 2, self._new_class(ne, rs), self._evidence(rs, c.index, 2), tci)

    def _owner_fields(self):
        """Owner and sibling fields inside part classes."""
        part_class_owner = {}
        for (cid, fname), p in sorted(self.parts.items()):
            for pt in p.part_types:
                part_class_owner.setdefault(pt, set()).update(self._owner_family(p))
        for (cid, fname), p in sorted(self.parts.items()):
            for pt in sorted(p.part_types):
                pci = self.classes[pt]
                if 3 in p.patterns:
                    for sub in self.all_subtypes(pci):
                        for mci, md in self.find_methods(sub, 'setOwner'):
                            if len(md.params) == 1 and md.body is not None:
                                for a in md.body.field_assignments():
                                    if a.rhs is not None and a.rhs.texts() == [md.params[0].name]:
                                        ofi = self._field_in_chain(mci, a.name)
                                        if ofi is not None:
                                            self.owner_field_rules[(ofi.ci.cid, ofi.name)] = ('late-bound owner', p)
                else:
                    self._owner_field_from_ctor(pci, p)
        for ci in sorted(self.classes.values(), key=lambda c: c.cid):
            owners = part_class_owner.get(ci.cid)
            if not owners:
                continue
            for c in self.superclass_chain(ci):
                for fi in c.fields:
                    if fi.static or (c.cid, fi.name) in self.owner_field_rules:
                        continue
                    base = fi.jt
                    if base is not None and base.cat == 'class' and base.ci.cid in part_class_owner and base.ci.cid != ci.cid:
                        if part_class_owner[base.ci.cid] & owners:
                            self.owner_field_rules.setdefault((c.cid, fi.name), ('sibling part', None))

    def _owner_family(self, p):
        out = set()
        for o in p.owner_classes | {p.owner.cid}:
            for s in self.all_subtypes(self.classes[o]):
                out.add(s.cid)
        return out

    def _owner_field_from_ctor(self, pci, p):
        """The field of part class pci assigned from the constructor parameter that receives 'this'."""
        fam = self._owner_family(p)
        for c in self.superclass_chain(pci):
            for md in c.td.methods:
                if md.kind != 'constructor' or md.body is None:
                    continue
                for i, prm in enumerate(md.params):
                    jt = self.classify(prm.type, md, c)
                    if jt.cat != 'class' or not (fam & {s.cid for s in self.all_subtypes(jt.ci)}):
                        continue
                    ofi = self._trace_param(c, md, i, 0)
                    if ofi is not None and (ofi.ci.cid, ofi.name) not in self.owner_field_rules:
                        self.owner_field_rules[(ofi.ci.cid, ofi.name)] = ('owner', p)

    def _setter_field(self, ci, setter):
        for mci, md in self.find_methods(ci, setter):
            if len(md.params) != 1 or md.body is None:
                continue
            for a in md.body.field_assignments():
                if a.op == '=' and a.rhs is not None and a.rhs.texts() == [md.params[0].name] and a.qualifier in (None, 'this'):
                    fi = self._field_in_chain(mci, a.name)
                    if fi is not None:
                        return fi
        return None

    def _trace_param(self, ci, md, pidx, depth):
        if depth > 4 or md.body is None or pidx >= len(md.params):
            return None
        pname = md.params[pidx].name
        for a in md.body.field_assignments():
            if a.op == '=' and a.rhs is not None and a.rhs.texts() == [pname] and a.qualifier in (None, 'this'):
                fi = self._field_in_chain(ci, a.name)
                if fi is not None:
                    return fi
        for c in md.body.method_calls():
            if c.name not in ('super', 'this'):
                continue
            for j, arg in enumerate(c.args):
                if arg.texts() != [pname]:
                    continue
                target = ci.superclass if c.name == 'super' else ci
                if target is None:
                    continue
                for cmd in target.td.methods:
                    if cmd.kind == 'constructor' and (len(cmd.params) == len(c.args) or (cmd.params and cmd.params[-1].varargs)):
                        r = self._trace_param(target, cmd, j, depth + 1)
                        if r is not None:
                            return r
        return None

    def _receiver_field(self, rs, receiver):
        texts = receiver.texts()
        ci = rs.ci
        m = rs.member
        if len(texts) == 1:
            name = texts[0]
            if isinstance(m, javasrc.MethodDecl):
                for i, prm in enumerate(m.params):
                    if prm.name == name:
                        return self._trace_param(ci, m, i, 0)
            return self._field_in_chain(ci, name)
        if len(texts) == 3 and texts[0] == 'this' and texts[1] == '.':
            return self._field_in_chain(ci, texts[2])
        if len(texts) == 3 and texts[1] == '(' and texts[2] == ')':
            return self._getter_field(ci, texts[0], 0)
        return None

    def _getter_field(self, ci, name, depth):
        if depth > 4:
            return None
        for mci, md in self.find_methods(ci, name):
            if md.params or md.body is None:
                continue
            t = md.body.texts()
            if 'return' not in t:
                continue
            i = t.index('return') + 1
            while i < len(t) and t[i] == '(':  # cast: (Type<..>) expr
                depth_p = 0
                j = i
                while j < len(t):
                    if t[j] == '(':
                        depth_p += 1
                    elif t[j] == ')':
                        depth_p -= 1
                        if depth_p == 0:
                            break
                    j += 1
                i = j + 1
            rest = t[i:]
            if len(rest) >= 2 and rest[1] == ';':
                return self._field_in_chain(mci, rest[0])
            if len(rest) >= 4 and rest[:2] == ['this', '.'] and rest[3] == ';':
                return self._field_in_chain(mci, rest[2])
            if len(rest) >= 6 and rest[0] == 'super' and rest[1] == '.' and rest[3] == '(' and rest[4] == ')' and mci.superclass is not None:
                return self._getter_field(mci.superclass, rest[2], depth + 1)
        return None

    # -- callbacks and captures
    def _ident_range(self, rs, s, e):
        refs = rs.span.identifier_refs()
        if rs._ident_idx is None:
            rs._ident_idx = [r.index for r in refs]
        lo = bisect.bisect_left(rs._ident_idx, s)
        hi = bisect.bisect_left(rs._ident_idx, e)
        return refs[lo:hi]

    def _lexical_chain(self, rs, i):
        out = []
        c = self.class_at(rs, i)
        while c is not None:
            out.append(c)
            c = c.lex_parent
        return out

    def _decl_index(self, ci):
        return ci.td.start if not ci.td.anonymous else (ci.new_expr.index if ci.new_expr is not None else ci.td.start)

    def captures(self, rs, s, e, inside):
        """Captured variables of the code in tokens [s, e) of rs. inside(ci) tells whether a class is declared within the range."""
        sp = rs.span
        tk = sp.cu.tokens
        t = tk.text
        out = {}

        def add_this(c, line):
            for cap in out.values():
                if cap.kind == 'this' and cap.ci is c:
                    return
            name = 'this' if not any(x.kind == 'this' for x in out.values()) else f'this${c.simple}'
            out[name] = Capture(name, 'this', c.simple, JType('class', fqn=c.cid, ci=c, name=c.simple), c, line)

        def owner_of_member(chain, name, method):
            for c in chain:
                if method:
                    ms = self.find_methods(c, name)
                    if ms:
                        return c, all('static' in md.modifiers for _, md in ms)
                else:
                    r = self.find_field(c, name)
                    if r is not None:
                        if r == 'enum-constant':
                            return c, True
                        return c, r.static
            return None, None

        for r in self._ident_range(rs, s, e):
            if r.qualifier in ('this', 'super'):
                if r.role in ('member', 'call'):
                    c = self.class_at(rs, r.index)
                    if not inside(c) and not rs.static:
                        add_this(c, r.line)
                continue
            if r.qualifier is not None:
                continue
            if r.role == 'expr':
                vis = sp.locals_visible_at(r.index)
                v = vis.get(r.name)
                if v is not None:
                    if v.index < s and r.name not in out:
                        jt = self._local_type(rs, v)
                        out[r.name] = Capture(r.name, 'local' if v.kind != 'param' else 'param',
                                              str(v.type) if v.type is not None else 'var', jt, jt.ci if jt.cat == 'class' else None, r.line)
                    continue
                chain = self._lexical_chain(rs, r.index)
                c, static = owner_of_member(chain, r.name, False)
                if c is not None:
                    if not static and not inside(c) and not rs.static:
                        add_this(self._instance_holder(chain, c), r.line)
                    continue
            elif r.role == 'call':
                chain = self._lexical_chain(rs, r.index)
                c, static = owner_of_member(chain, r.name, True)
                if c is not None:
                    if not static and not inside(c) and not rs.static:
                        add_this(self._instance_holder(chain, c), r.line)
                    continue
                if any(imp.static and (imp.wildcard or imp.name.rpartition('.')[2] == r.name) for imp in sp.cu.imports):
                    continue
                inner = chain[0] if chain else None
                if inner is not None and not inside(inner):
                    if self.has_external_super(inner) and not rs.static:
                        add_this(inner, r.line)
        for i in range(s, e):
            if t[i] == 'this' and tk.kind[i] is javasrc.KEYWORD:
                if t[i - 1] == '.':
                    q = t[i - 2]
                    for c in self._lexical_chain(rs, i):
                        if c.td.name == q and not inside(c):
                            add_this(c, tk.loc(i)[0])
                            break
                    continue
                c = self.class_at(rs, i)
                if not inside(c) and not rs.static:
                    add_this(c, tk.loc(i)[0])
        return [out[k] for k in sorted(out, key=lambda n: (out[n].kind != 'this', out[n].line, n))]

    def _instance_holder(self, chain, c):
        """The lexical class whose instance is captured when member c (found by chain lookup) is used: the chain entry that has c in its
        supertype closure (c itself or a subclass of c)."""
        for x in chain:
            if x is c or c in self._supers_closure(x):
                return x
        return c

    def _supers_closure(self, ci):
        out = []
        seen = set()
        stack = list(ci.supers)
        while stack:
            s = stack.pop()
            if s.cid in seen:
                continue
            seen.add(s.cid)
            out.append(s)
            stack.extend(s.supers)
        return out

    def _local_type(self, rs, v):
        if v.type is not None and v.type.name != 'var':
            return self.classify(v.type, rs.span, rs.ci)
        if v.initializer is not None:
            ne = self.new_exact(rs, v.initializer.start, v.initializer.end)
            if ne is not None:
                return self.classify(ne.type, rs.span, rs.ci)
        return JType('unknown', name='var')

    def _contexts(self, rs):
        if rs._args is None:
            args = {}
            for c in rs.span.method_calls():
                for i, a in enumerate(c.args):
                    args.setdefault((a.start, a.end), ('call', c, i))
            for ne in rs.span.new_expressions():
                for i, a in enumerate(ne.args or []):
                    args.setdefault((a.start, a.end), ('new', ne, i))
            rhs = {}
            for a in rs.span.assignments():
                if a.rhs is not None:
                    rhs[(a.rhs.start, a.rhs.end)] = a
            loc = {}
            for v in rs.span.local_variables():
                if v.initializer is not None:
                    loc[(v.initializer.start, v.initializer.end)] = v
            rs._args, rs._rhs, rs._local_init = args, rhs, loc
        return rs._args, rs._rhs, rs._local_init

    def _callbacks(self):
        for rs in self.root_spans:
            if rs.ci.origin != 'output':
                continue
            sp = rs.span
            tk = sp.cu.tokens
            named = rs.ci
            while named.td.anonymous or named.td.local:
                named = named.lex_parent
            items = []
            for lam in sp.lambdas():
                items.append(('lambda', lam.span.start, lam.span.end, lam))
            for mr in sp.method_refs():
                items.append(('method_ref', mr.receiver.start, mr.index + 1, mr))
            for ne in sp.anonymous_classes():
                items.append(('anonymous', ne.span.start, ne.span.end, ne))
            for kind, s, e, obj in items:
                line, col = tk.loc(s)
                ci_here = self.class_at(rs, s)
                if kind == 'anonymous':
                    aci = self.ci_of[id(obj.anonymous)]
                    cbid = aci.cid
                else:
                    cbid = f'{named.cid}@L{line}:{col}'
                cb = Callback(cbid, kind, rs, s, e, line, ci_here)
                if kind == 'anonymous':
                    cb.anon = aci
                    body = obj.anonymous.body
                    cb.captures = self.captures(rs, body.start, body.end, lambda c, a=aci: self._declared_within(c, a))
                    aci.captures = cb.captures
                elif kind == 'lambda':
                    ls, le = s, e

                    def inside_lambda(c, ls=ls, le=le):
                        return (c.td.anonymous or c.td.local) and c.root_span is rs and ls <= self._decl_index(c) < le
                    cb.captures = self.captures(rs, obj.body.start, obj.body.end, inside_lambda)
                else:
                    rt = obj.receiver.texts()
                    if rt == ['this'] or rt == ['super']:
                        c = ci_here
                        cb.captures = [] if rs.static else [Capture('this', 'this', c.simple, JType('class', fqn=c.cid, ci=c, name=c.simple), c, line)]
                    elif len(rt) == 1 and rt[0][:1].islower():
                        cb.captures = self.captures(rs, obj.receiver.start, obj.receiver.end, lambda c: False)
                    elif len(rt) >= 3 and rt[-2:] == ['.', 'this']:
                        cb.captures = self.captures(rs, obj.receiver.start, obj.receiver.end, lambda c: False)
                    else:
                        cb.captures = []
                self._classify_storage(cb)
                if kind == 'anonymous':
                    aci.storage = cb
                if cb.storage == 'sync' and kind != 'anonymous':
                    continue
                if cbid in self.callbacks:
                    raise FieldmapError(f'duplicate callback id {cbid}')
                self.callbacks[cbid] = cb
                ci_here.callbacks.append(cbid)

    def _declared_within(self, c, a):
        x = c
        while x is not None:
            if x is a:
                return True
            x = x.lex_parent
        return False

    def _classify_storage(self, cb):
        rs = cb.rs
        ctx = self._expr_context(rs, cb.start, cb.end)
        kind = ctx[0]
        if kind == 'call':
            c, pos = ctx[1], ctx[2]
            api = self._api_for_call(rs, c, pos)
            cb.context = f'argument {pos + 1} of {c.name}()'
            if api is None:
                cb.storage = 'sync'
                return
            if api[0] == 'future':
                cb.api = c.name
                self._future_storage(cb, rs, c)
                self._task_args(cb, rs, c)
                return
            cb.api = c.name
            if api[0] == 'static':
                cb.storage = 'static'
                return
            cb.storage = 'stored'
            cb.owners = api[1]
            self._task_args(cb, rs, c)
            return
        if kind == 'new':
            ne, pos = ctx[1], ctx[2]
            nci = self._new_class(ne, rs)
            cb.context = f'argument {pos + 1} of new {ne.type.name}()'
            if nci is not None and self._ctor_stores(nci, len(ne.args or []), pos):
                cb.storage = 'stored'
                cb.api = f'new {ne.type.name}'
                cb.owners = [nci]
            else:
                cb.storage = 'sync'
            return
        if kind == 'assign':
            a = ctx[1]
            cb.context = f'assigned to {a.qualifier + "." if a.qualifier else ""}{a.name}'
            if a.local:
                self._local_flow(cb, rs, a.name, a.index)
                return
            fi = self._field_for_assignment(rs, a)
            if fi is not None:
                cb.storage = 'stored'
                cb.api = f'field {fi.ci.simple}.{fi.name}'
                cb.owners = [fi.ci] if not fi.static else []
                if fi.static:
                    cb.storage = 'static'
            else:
                cb.storage = 'unknown'
            return
        if kind == 'local':
            v = ctx[1]
            cb.context = f'local {v.name}'
            self._local_flow(cb, rs, v.name, v.index)
            return
        if kind == 'field':
            fi = self._field_in_chain(rs.ci, rs.member.name)
            cb.context = f'initializer of field {rs.member.name}'
            cb.api = f'field {rs.ci.simple}.{rs.member.name}'
            if fi is not None and fi.static:
                cb.storage = 'static'
            else:
                cb.storage = 'stored'
                cb.owners = [rs.ci]
            return
        if kind == 'return':
            cb.context = 'returned'
            cb.storage = 'returned'
            return
        cb.context = 'expression'
        cb.storage = 'sync'

    def _expr_context(self, rs, s, e):
        args, rhs, loc = self._contexts(rs)
        if (s, e) in args:
            k, obj, pos = args[(s, e)]
            return (k, obj, pos)
        if (s, e) in rhs:
            return ('assign', rhs[(s, e)])
        if (s, e) in loc:
            return ('local', loc[(s, e)])
        sp = rs.span
        if isinstance(rs.member, javasrc.FieldDecl) and sp.start == s and sp.end == e:
            return ('field',)
        t = sp.cu.tokens.text
        if t[s - 1] == 'return':
            return ('return',)
        # (Cast) expr or ( expr )
        if t[s - 1] == ')' and sp.cu.tokens.match[s - 1] >= sp.start:
            o = sp.cu.tokens.match[s - 1]
            return self._expr_context(rs, o, e)
        return ('other',)

    def _field_for_assignment(self, rs, a):
        if a.qualifier not in (None, 'this'):
            return None
        chain = self._lexical_chain(rs, a.index)
        for c in chain:
            fi = self._field_in_chain(c, a.name)
            if fi is not None:
                return fi
        return None

    def _local_flow(self, cb, rs, name, decl_index):
        """A callback held in a local: follow the local into a storing call or a field assignment."""
        sp = rs.span
        for c in sp.method_calls():
            if c.index <= decl_index:
                continue
            for pos, a in enumerate(c.args):
                if a.texts() == [name]:
                    api = self._api_for_call(rs, c, pos)
                    if api is None:
                        continue
                    cb.api = c.name
                    if api[0] == 'future':
                        self._future_storage(cb, rs, c)
                    elif api[0] == 'static':
                        cb.storage = 'static'
                    else:
                        cb.storage = 'stored'
                        cb.owners = api[1]
                    return
        for a in sp.field_assignments():
            if a.index > decl_index and a.rhs is not None and a.rhs.texts() == [name]:
                fi = self._field_for_assignment(rs, a)
                if fi is not None:
                    cb.api = f'field {fi.ci.simple}.{fi.name}'
                    cb.storage = 'static' if fi.static else 'stored'
                    cb.owners = [] if fi.static else [fi.ci]
                    return
        cb.storage = 'local'

    def _future_storage(self, cb, rs, call):
        cb.storage = 'task'
        ctx = self._expr_context(rs, call.span.start, call.span.end)
        holder = None
        if ctx[0] == 'call':
            api = self._api_for_call(rs, ctx[1], ctx[2])
            if api is not None and api[0] == 'owner':
                cb.owners = api[1]
                holder = f'{ctx[1].name}()'
        elif ctx[0] == 'assign':
            a = ctx[1]
            if a.local:
                sp = rs.span
                for c in sp.method_calls():
                    if c.index > a.index:
                        for pos, arg in enumerate(c.args):
                            if arg.texts() == [a.name]:
                                api = self._api_for_call(rs, c, pos)
                                if api is not None and api[0] == 'owner':
                                    cb.owners = api[1]
                                    holder = f'{c.name}()'
            else:
                fi = self._field_for_assignment(rs, a)
                if fi is not None and not fi.static:
                    cb.owners = [fi.ci]
                    holder = f'field {fi.ci.simple}.{fi.name}'
                elif fi is not None:
                    holder = f'static field {fi.ci.simple}.{fi.name}'
        elif ctx[0] == 'local':
            v = ctx[1]
            sp = rs.span
            for c in sp.method_calls():
                if c.index > v.index:
                    for pos, arg in enumerate(c.args):
                        if arg.texts() == [v.name]:
                            api = self._api_for_call(rs, c, pos)
                            if api is not None and api[0] == 'owner':
                                cb.owners = api[1]
                                holder = f'{c.name}()'
            for a in sp.field_assignments():
                if a.index > v.index and a.rhs is not None and a.rhs.texts() == [v.name]:
                    fi = self._field_for_assignment(rs, a)
                    if fi is not None and not fi.static:
                        cb.owners = [fi.ci]
                        holder = f'field {fi.ci.simple}.{fi.name}'
        cb.future_holder = holder or ''

    def _task_args(self, cb, rs, call):
        for a in call.args:
            if a.start == cb.start and a.end == cb.end:
                continue
            ne = self.new_exact(rs, a.start, a.end)
            if ne is not None:
                nci = self._new_class(ne, rs)
                if nci is not None:
                    cb.escape_args.append(nci)

    @staticmethod
    def _config_api(table, name, recv):
        """Entry of [future_apis]/[stored_callback_apis] for a call: keys are 'method' or 'Receiver.method' (receiver text must contain
        Receiver); an explicit receiver = regex overrides."""
        for key in sorted(table, key=lambda k: (-len(k), k)):
            entry = table[key]
            prefix, _, method = key.rpartition('.')
            if method != name:
                continue
            pattern = entry.get('receiver', re.escape(prefix) if prefix else '')
            if re.search(pattern, recv):
                return entry
        return None

    @staticmethod
    def _count_args(t, m, open_i):
        close = m[open_i]
        if close == open_i + 1:
            return 0
        n = 1
        depth = 0
        for j in range(open_i + 1, close):
            x = t[j]
            if x in ('(', '[', '{'):
                depth += 1
            elif x in (')', ']', '}'):
                depth -= 1
            elif x == ',' and depth == 0:
                n += 1
        return n

    def _method_return(self, ci, name, nargs):
        for mci, md in self.find_methods(ci, name):
            if len(md.params) == nargs or (md.params and md.params[-1].varargs and nargs >= len(md.params) - 1):
                if md.return_type is None or md.return_type.name == 'void':
                    return None
                return self.classify(md.return_type, md, mci)
        return None

    def expr_type(self, rs, s, e):
        """(JType, FieldInfo of the last field access or None, static type reference) of a simple receiver expression
        (names, this, field accesses, method calls, X.getInstance() chains); (None, None, False) when unknown."""
        sp = rs.span
        tk = sp.cu.tokens
        t, k, m = tk.text, tk.kind, tk.match
        i = s
        jt = None
        field = None
        static_ref = False
        if i >= e:
            return None, None, False
        if t[i] == 'this' and k[i] is javasrc.KEYWORD:
            c = self.class_at(rs, i)
            jt = JType('class', fqn=c.cid, ci=c, name=c.simple)
            i += 1
        elif t[i] == 'super' and k[i] is javasrc.KEYWORD:
            c = self.class_at(rs, i).superclass
            if c is None:
                return None, None, False
            jt = JType('class', fqn=c.cid, ci=c, name=c.simple)
            i += 1
        elif k[i] is javasrc.IDENT:
            name = t[i]
            if i + 1 < e and t[i + 1] == '(':
                for c in self._lexical_chain(rs, i):
                    jt = self._method_return(c, name, self._count_args(t, m, i + 1))
                    if jt is not None or self.find_methods(c, name):
                        break
                i = m[i + 1] + 1
            else:
                v = sp.locals_visible_at(i).get(name)
                if v is not None and v.index < i:
                    jt = self._local_type(rs, v)
                else:
                    for c in self._lexical_chain(rs, i):
                        fi = self._field_in_chain(c, name)
                        if fi is not None:
                            jt, field = fi.jt, fi
                            break
                    if jt is None:
                        kind, value = self.idx.resolve_kind(name, sp)
                        if kind in ('project', 'local'):
                            td = self.idx.types[value] if kind == 'project' else value
                            c = self.ci_of.get(id(td))
                            if c is not None:
                                jt = JType('class', fqn=c.cid, ci=c, name=c.simple)
                                static_ref = True
                i += 1
        else:
            return None, None, False
        while i < e and jt is not None:
            if t[i] != '.' or i + 1 >= e or k[i + 1] is not javasrc.IDENT:
                return None, None, False
            name = t[i + 1]
            if jt.cat not in ('class', 'enum') or jt.ci is None:
                return (None, None, False) if i + 2 < e else (None, None, False)
            if i + 2 < e and t[i + 2] == '(':
                jt = self._method_return(jt.ci, name, self._count_args(t, m, i + 2))
                field = None
                i = m[i + 2] + 1
            else:
                fi = self._field_in_chain(jt.ci, name)
                jt, field = (fi.jt, fi) if fi is not None else (None, None)
                i += 2
            static_ref = False
        return jt, field, static_ref

    def _api_for_call(self, rs, call, pos):
        name = call.name
        recv = call.receiver.text if call.receiver is not None else ''
        fut = self._config_api(self.cfg['future_apis'], name, recv)
        if fut is not None:
            return ('future',)
        st = self._config_api(self.cfg['stored_callback_apis'], name, recv)
        if st is not None:
            owners = []
            for o in ([st['owner']] if isinstance(st['owner'], str) else st['owner']):
                oci = self.classes.get(o)
                if oci is None:
                    raise FieldmapError(f'fieldmap.toml: stored_callback_apis.{name}: unknown owner class {o}')
                owners.append(oci)
            return ('owner', owners)
        if name in ('super', 'this'):
            target = rs.ci.superclass if name == 'super' else rs.ci
            if target is not None and self._ctor_stores(target, len(call.args), pos):
                return ('owner', [target])
            return None
        rt = call.receiver.texts() if call.receiver is not None else None
        rjt = None
        if rt is not None and rt not in (['this'], ['super']):
            rjt, rfield, _ = self.expr_type(rs, call.receiver.start, call.receiver.end)
            if rjt is not None and rjt.cat in ('coll', 'atomic'):
                if name in STORE_CALLS and rfield is not None:
                    return ('static',) if rfield.static else ('owner', [rfield.ci])
                return None
            if rjt is None and name in STORE_CALLS:
                return None
            if rjt is not None and rjt.cat != 'class':
                return None
        cands = []
        for ci, md in self._methods_named(name):
            varargs = bool(md.params) and md.params[-1].varargs
            if not (len(md.params) == len(call.args) or (varargs and len(call.args) >= len(md.params) - 1)):
                continue
            stored = self._stored_params(ci, md)
            p = len(md.params) - 1 if varargs and pos >= len(md.params) - 1 else pos
            if p in stored:
                cands.append((ci, md, stored[p]))
        if not cands:
            return None
        if rt is None or rt == ['this'] or rt == ['super']:
            chain = self._lexical_chain(rs, call.index)
            keep = [x for x in cands if any(x[0] is c or x[0] in self._supers_closure(c) for c in chain)]
            if not keep:
                return None
            cands = keep
        elif rjt is not None:
            rci = rjt.ci
            fam = {rci.cid} | {s.cid for s in self._supers_closure(rci)} | {s.cid for s in self.all_subtypes(rci)}
            cands = [x for x in cands if x[0].cid in fam]
        owners = []
        task = False
        for ci, md, stored in cands:
            for o in stored:
                if o == 'task':
                    task = True
                elif o not in owners:
                    owners.append(o)
        if owners:
            return ('owner', sorted(owners, key=lambda c: c.cid))
        return ('future',) if task else None

    def _methods_named(self, name):
        if self._storing_by_name is None:
            out = {}
            for ci in sorted(self.classes.values(), key=lambda c: c.cid):
                if ci.origin != 'output':
                    continue
                for md in ci.td.methods:
                    if md.kind == 'method' and md.body is not None and md.params:
                        out.setdefault(md.name, []).append((ci, md))
            self._storing_by_name = out
        return self._storing_by_name.get(name, ())

    def _stored_params(self, ci, md):
        """{parameter index: [owner ClassInfo | 'task']} for a method or constructor that stores the parameter: assigns it to a field,
        adds it to a field collection, or passes it on to a storing call (inferred recursively, guarded against cycles)."""
        key = (ci.cid, id(md))
        if key in self._stored_param_cache:
            return self._stored_param_cache[key]
        self._stored_param_cache[key] = {}
        rs = self.rs_of_method.get(id(md))
        stored = {}
        if rs is None or md.body is None:
            return stored
        body = md.body
        names = {p.name: i for i, p in enumerate(md.params)}

        def add(pos, owners):
            lst = stored.setdefault(pos, [])
            for o in owners:
                if o not in lst:
                    lst.append(o)
        for a in body.field_assignments():
            if a.rhs is not None and len(a.rhs.texts()) == 1 and a.rhs.texts()[0] in names:
                fi = self._field_for_assignment(rs, a)
                if fi is not None and not fi.static:
                    add(names[a.rhs.texts()[0]], [fi.ci])
        for c in body.method_calls():
            for j, arg in enumerate(c.args):
                texts = arg.texts()
                if len(texts) != 1 or texts[0] not in names:
                    continue
                if body.locals_visible_at(c.index).get(texts[0]) is not None and \
                        body.locals_visible_at(c.index)[texts[0]].kind != 'param':
                    continue
                pos = names[texts[0]]
                if c.name in ('super', 'this'):
                    target = ci.superclass if c.name == 'super' else ci
                    if target is None:
                        continue
                    for cmd in target.td.methods:
                        if cmd.kind == 'constructor' and cmd is not md and len(cmd.params) == len(c.args) and cmd.body is not None:
                            sub = self._stored_params(target, cmd)
                            if j in sub:
                                add(pos, sub[j])
                    continue
                api = self._api_for_call(rs, c, j)
                if api is None:
                    continue
                if api[0] == 'owner':
                    add(pos, api[1])
                elif api[0] == 'future':
                    add(pos, ['task'])
        # a parameter captured by a stored anonymous class or lambda is stored with it (AIActions.addRequest)
        inner = [(ne.span.start, ne.span.end, 'anonymous') for ne in body.anonymous_classes()]
        inner += [(lam.span.start, lam.span.end, 'lambda') for lam in body.lambdas()]
        for s0, e0, kind in sorted(inner):
            used = sorted({r.name for r in self._ident_range(rs, s0, e0) if r.qualifier is None and r.role == 'expr' and r.name in names})
            if not used:
                continue
            tmp = Callback('', kind, rs, s0, e0, 0, ci)
            self._classify_storage(tmp)
            if tmp.storage in ('stored', 'task'):
                for n in used:
                    add(names[n], tmp.owners or (['task'] if tmp.storage == 'task' else []))
        self._stored_param_cache[key] = stored
        return stored

    def _ctor_stores(self, ci, nargs, pos):
        for md in ci.td.methods:
            if md.kind == 'constructor' and (len(md.params) == nargs or (md.params and md.params[-1].varargs)) and md.body is not None:
                st = self._stored_params(ci, md)
                if pos in st or (md.params and md.params[-1].varargs and pos >= len(md.params) - 1 and len(md.params) - 1 in st):
                    return True
        return False

    def _inner_class_captures(self):
        """this$0 of non-static named inner classes and captures of local classes."""
        for ci in self.classes.values():
            td = ci.td
            if ci.origin != 'output' or td.anonymous:
                continue
            if td.local:
                rs = ci.root_span
                ci.captures = self.captures(rs, td.body.start, td.body.end, lambda c, a=ci: self._declared_within(c, a))
                continue
            if ci.lex_parent is None or td.kind != 'class' or 'static' in td.modifiers or ci.lex_parent.td.kind in ('interface', 'annotation'):
                continue
            caps = {}
            for rs in self.spans_of_cu(ci.cu):
                if not self._declared_within(rs.ci, ci):
                    continue
                for cap in self.captures(rs, rs.span.start, rs.span.end, lambda c, a=ci: self._declared_within(c, a)):
                    if cap.kind == 'this':
                        caps.setdefault(cap.ci.cid, cap)
            ci.captures = [caps[k] for k in sorted(caps)]
            for i, cap in enumerate(ci.captures):
                cap.name = 'this$0' if i == 0 else f'this${cap.ci.simple}'

    # -- kinds
    def _kinds(self):
        # K1
        if self.staticdata is not None:
            for fqn, rm in self.staticdata.items():
                ci = self.classes.get(fqn)
                if ci is None:
                    self.warnings.append(f'staticdata-classes.json: unknown class {fqn}')
                    continue
                ci.kind = K1
                ci.kind_reason = 'staticdata-classes.json'
                ci.runtime_mutable = set(rm)
        else:
            self._k1_heuristic()
        # K2
        bases = set(self.cfg['settings']['packet_bases'])
        for ci in self.classes.values():
            if ci.kind is None and any(c.cid in bases for c in self.superclass_chain(ci)[1:]):
                ci.kind = K2
                ci.kind_reason = 'packet: extends ' + next(c.cid for c in self.superclass_chain(ci)[1:] if c.cid in bases)
        # groups (class trees below the topmost project superclass)
        for ci in self.classes.values():
            chain = self.superclass_chain(ci)
            ci.group = chain[-1].cid
        members = {}
        for ci in self.classes.values():
            members.setdefault(ci.group, []).append(ci)
        overrides = self.cfg['kinds']
        for key in overrides:
            if key not in self.classes:
                raise FieldmapError(f'fieldmap.toml: [kinds] unknown class {key}')
        escaped = {}
        queue = deque()

        def mark(ci, reason):
            if ci is None or ci.kind in (K1, K2):
                return
            ov = overrides.get(ci.cid)
            if ov is not None and ov['kind'] != K4 and ov['kind'] != K3:
                return
            if ci.scalar_like:
                return
            if ci.is_interface:
                if ('iface', ci.cid) in escaped:
                    return
                escaped[('iface', ci.cid)] = reason
                for sub in self.all_subtypes(ci)[1:]:
                    mark(sub, f'implements {ci.cid} ({reason})')
                return
            g = ci.group
            if g in escaped:
                return
            escaped[g] = reason
            for m in members[g]:
                if m.escape is None:
                    m.escape = reason if m is ci else f'same class tree as {ci.cid}: {reason}'
            queue.append(g)

        def mark_type(jt, reason):
            for c in self._type_classes(jt):
                mark(c, reason)

        for key, ov in sorted(overrides.items()):
            if ov['kind'] in (K3, K4):
                mark(self.classes[key], f'fieldmap.toml: {ov["reason"]}')
        for ci in sorted(self.classes.values(), key=lambda c: c.cid):
            for fi in ci.fields:
                if fi.static:
                    mark_type(fi.jt, f'static field {ci.cid}.{fi.name}')
                elif ci.kind == K2:
                    mark_type(fi.jt, f'packet member {ci.cid}.{fi.name}')
                elif ci.kind == K1 and not self._jaxb_bound(ci, fi):
                    mark_type(fi.jt, f'static data member {ci.cid}.{fi.name}')
                elif ci.is_enum or ci.const is not None:
                    mark_type(fi.jt, f'enum member {ci.cid}.{fi.name}')
        for cb in sorted(self.callbacks.values(), key=lambda c: c.id):
            if cb.storage in ('sync', 'local'):
                continue
            where = f'{cb.rs.span.cu.path and _rel(cb.rs.span.cu.path)}:{cb.line}'
            if cb.anon is not None:
                mark(cb.anon, f'{cb.storage} anonymous class at {where}')
            for cap in cb.captures:
                mark_type(cap.jt, f'captured by {cb.kind} at {where}')
            for a in cb.escape_args:
                mark(a, f'task argument at {where}')
        while queue:
            g = queue.popleft()
            for m in sorted(members[g], key=lambda c: c.cid):
                for fi in m.fields:
                    if not fi.static:
                        mark_type(fi.jt, f'member of {m.cid}.{fi.name}')
                for cap in m.captures:
                    mark_type(cap.jt, f'captured by {m.cid}')
                for ref in list(m.td.extends) + list(m.td.implements):
                    for a in ref.args or []:
                        mark_type(self.classify(a, m.lex_parent.td if m.td.anonymous and m.lex_parent else m.td, m), f'type argument of {m.cid}')
        # immutability (greatest fixpoint)
        cand = {ci.cid for ci in self.classes.values() if self._immutable_local(ci)}
        changed = True
        while changed:
            changed = False
            for cid in sorted(cand):
                ci = self.classes[cid]
                ok = True
                if ci.superclass is not None and ci.superclass.cid not in cand and ci.superclass.kind != K1:
                    ok = False
                if ok:
                    for fi in ci.fields:
                        if fi.static:
                            continue
                        for c in self._type_classes(fi.jt):
                            if c.kind != K1 and not c.scalar_like and c.cid not in cand:
                                ok = False
                                break
                        if not ok:
                            break
                if not ok:
                    cand.discard(cid)
                    changed = True
        for ci in self.classes.values():
            ci.immutable = ci.cid in cand
            if ci.kind in (K1, K2):
                continue
            if ci.value_type:
                ci.kind = K5
                ci.kind_reason = 'value type (fieldmap.toml settings.value_types): copied by value, members plain'
                continue
            if ci.is_enum or (ci.td.anonymous and ci.const is not None):
                ci.kind = K3 if ci.immutable else K4
                ci.kind_reason = 'enum: all instance fields final' if ci.immutable else 'enum with mutable instance fields (global state)'
                continue
            esc = escaped.get(ci.group) if not ci.is_interface else escaped.get(('iface', ci.cid))
            if esc is not None:
                ci.kind = K3 if ci.immutable else K4
                ci.kind_reason = ci.escape or esc
            else:
                ci.kind = K5
                ci.kind_reason = 'confined: inferred (never stored in shared state)'
        for key, ov in overrides.items():
            ci = self.classes[key]
            ci.kind = ov['kind']
            ci.kind_reason = f'fieldmap.toml: {ov["reason"]}'
        self._singletons()
        for key in sorted(self.cfg['immortal']):
            ci = self.classes.get(key)
            if ci is None:
                raise FieldmapError(f'fieldmap.toml: [immortal] {key!r} is not a class of the tree')
            ci.immortal = True

    def _singleton_fields(self, ci):
        """Static fields of ci's own type declared in ci or in its nested SingletonHolder."""
        holders = [ci] + [c for c in self.classes.values() if c.lex_parent is ci and c.td.name == 'SingletonHolder']
        return [fi for c in holders for fi in c.fields if fi.static and fi.jt is not None and fi.jt.cat == 'class' and fi.jt.ci is ci]

    def _singletons(self):
        """ci.singleton: exactly one static field of the own type (in the class or its SingletonHolder), and every creation of the class
        (`new X(..)`, also of subclasses and anonymous subclasses) is that field's initializer or the right side of an assignment to it."""
        candidates = {}
        per_run = set(self.cfg['settings']['per_run_services'])
        for ci in self.classes.values():
            if ci.is_interface or ci.is_enum or ci.cid in per_run:
                continue
            fields = self._singleton_fields(ci)
            if len(fields) == 1:
                candidates[ci.cid] = fields[0]
        if not candidates:
            return
        bad = set()
        for rs in self.root_spans:
            for mr in rs.span.method_refs():
                if mr.name != 'new':
                    continue
                kind, value = self.idx.resolve_kind(' '.join(mr.receiver.texts()).replace(' ', ''), rs.span)
                created = self.ci_of.get(id(self.idx.types[value])) if kind == 'project' else (self.ci_of.get(id(value)) if kind == 'local' else None)
                if created is not None:
                    bad.update(c.cid for c in self.superclass_chain(created) if c.cid in candidates)  # X::new creates instances anywhere
            assigns = None
            for ne in rs.span.new_expressions():
                if ne.array:
                    continue
                created = self._new_class(ne, rs)
                if created is None:
                    continue
                for c in self.superclass_chain(created):
                    fi = candidates.get(c.cid)
                    if fi is None or c.cid in bad:
                        continue
                    ok = False
                    init = fi.decl.initializer if isinstance(fi.decl, javasrc.FieldDecl) else None
                    if init is not None and init.cu is rs.span.cu and init.start <= ne.index < init.end:
                        ok = True
                    else:
                        if assigns is None:
                            assigns = rs.span.field_assignments()
                        for a in assigns:
                            if a.name == fi.name and not a.element and a.rhs is not None and a.rhs.start <= ne.index < a.rhs.end \
                                    and rs.span.cu is fi.ci.cu:
                                ok = True
                                break
                    if not ok:
                        bad.add(c.cid)
        for cid in candidates:
            if cid not in bad:
                self.classes[cid].singleton = True

    def _immutable_local(self, ci):
        if ci.td.kind == 'annotation':
            return True
        if ci.external_super:
            return False
        for fi in ci.fields:
            if fi.static:
                continue
            if not self.is_final(fi):
                return False
            jt = fi.jt
            if jt.cat in ('prim', 'boxed', 'string', 'enum', 'class'):
                continue
            if jt.cat == 'ext' and jt.fqn in IMMUTABLE_EXTERNAL:
                continue
            if jt.cat == 'object' and jt.tvar is not None:
                continue
            return False
        return True

    def _type_classes(self, jt, generic_args=True):
        """Project classes in a type tree. generic_args=False descends only into containers (collections, arrays, atomics): the type
        arguments of a project generic class or a functional interface do not retain their objects."""
        out = []
        stack = [jt]
        while stack:
            j = stack.pop()
            if j is None:
                continue
            if j.cat in ('class', 'enum') and j.ci is not None:
                out.append(j.ci)
            if generic_args or j.cat in ('coll', 'array', 'atomic'):
                stack.extend(j.args)
        return out

    def _k1_heuristic(self):
        seeds = []
        for ci in sorted(self.classes.values(), key=lambda c: c.cid):
            if not any(i.name.startswith('javax.xml.bind') for i in ci.cu.imports):
                continue
            anns = [a.simple_name for a in ci.td.annotations]
            for fd in ci.td.fields:
                anns.extend(a.simple_name for a in fd.annotations)
            for md in ci.td.methods:
                anns.extend(a.simple_name for a in md.annotations)
            hit = sorted(set(anns) & JAXB_ANNOTATIONS)
            if hit:
                ci.kind = K1
                ci.kind_reason = f'JAXB annotation @{hit[0]}'
                seeds.append(ci)
        queue = deque(seeds)
        while queue:
            ci = queue.popleft()
            nxt = []
            if ci.superclass is not None:
                nxt.append((ci.superclass, f'superclass of static data class {ci.cid}'))
            for fi in ci.fields:
                if fi.static or not self._jaxb_bound(ci, fi):
                    continue
                for c in self._type_classes(fi.jt):
                    nxt.append((c, f'bound field {ci.cid}.{fi.name}'))
                for a in fi.decl.annotations:
                    for c in self._annotation_classes(a, ci):
                        nxt.append((c, f'@{a.simple_name} of {ci.cid}.{fi.name}'))
            for a in ci.td.annotations:
                if a.simple_name == 'XmlSeeAlso':
                    for c in self._annotation_classes(a, ci):
                        nxt.append((c, f'@XmlSeeAlso of {ci.cid}'))
            for c, why in nxt:
                if c.kind is None and c.origin == 'output':
                    c.kind = K1
                    c.kind_reason = why
                    queue.append(c)

    def _annotation_classes(self, ann, ci):
        out = []
        stack = [v for _, v in ann.args]
        while stack:
            v = stack.pop()
            if v is None:
                continue
            if v.kind == 'class' and v.type is not None:
                jt = self.classify(v.type, ci.td, ci)
                if jt.cat in ('class', 'enum'):
                    out.append(jt.ci)
            elif v.kind == 'array':
                stack.extend(v.items)
            elif v.kind == 'annotation':
                stack.extend(x for _, x in v.annotation.args)
        return out

    def _jaxb_bound(self, ci, fi):
        if fi.record_component or fi.transient or fi.static:
            return False
        anns = {a.simple_name for a in fi.decl.annotations}
        if 'XmlTransient' in anns:
            return False
        if anns & JAXB_BOUND_FIELD_ANNOTATIONS:
            return True
        for c in self.superclass_chain(ci):
            a = c.td.annotation('XmlAccessorType')
            if a is not None:
                v = a.get('value')
                return v is not None and v.text.endswith('FIELD')
        return False

    # -- field mapping
    def spell_value(self, jt):
        c = jt.cat
        if c == 'prim':
            return PRIMS[jt.name]
        if c == 'boxed':
            return BOXED[jt.fqn]
        if c == 'string':
            return 'std::string'
        if c in ('enum', 'class'):
            return self.cpp_class_name(jt.ci, jt)
        if c == 'ext':
            return EXTERNAL_SPELLING.get(jt.fqn, jt.fqn.rpartition('.')[2])
        if c == 'object':
            return jt.tvar or 'Object'
        return jt.name or '?'

    def spell_plain(self, jt):
        if jt.tvar:
            return jt.tvar
        if jt.cat in ('boxed', 'prim'):
            return BOXED.get(jt.fqn) or PRIMS.get(jt.name, jt.name)
        if jt.cat in ('coll', 'array', 'atomic', 'future', 'functional'):
            return self.spell_elem(jt, 'shared')
        return self.spell_value(jt)

    def ref_target(self, jt):
        """C++ spelling of a reference to a class type for shared members: ('ref'|'template'|'shared_ptr', name)."""
        ci = jt.ci
        name = jt.tvar or self.cpp_class_name(ci, jt)
        if ci.kind == K1:
            return 'template', name
        if not jt.tvar and (ci.immortal or ci.singleton) and ci.kind in (K3, K4):
            return 'immortal', name
        if self._is_connection(ci) or ci.kind == K2:
            return 'shared_ptr', name
        return 'ref', name

    def _is_connection(self, ci):
        bases = set(self.cfg['settings']['connection_bases'])
        return any(c.cid in bases for c in self.superclass_chain(ci))

    def shim_for(self, jt, fi=None):
        """(shim, rc alias, flags) of a collection type, using the concrete class of the initializer or constructor assignment."""
        flags = []
        impl = jt.fqn
        if fi is not None:
            found = self._initializer_impl(fi)
            if found is not None:
                impl = found
        if impl in COLLECTION_IFACES:
            impl = COLLECTION_IFACES[impl]
            flags.append('guessedImpl')
        if impl in COLLECTION_IMPLS:
            shim, rc, _ = COLLECTION_IMPLS[impl]
            return shim, rc, flags
        flags.append('noShim')
        return impl.rpartition('.')[2], None, flags

    def _initializer_impl(self, fi):
        spans = []
        if isinstance(fi.decl, javasrc.FieldDecl) and fi.decl.initializer is not None:
            spans.append(fi.decl.initializer)
        for rs in self.spans_of_cu(fi.ci.cu):
            if rs.ci is fi.ci and isinstance(rs.member, javasrc.MethodDecl) and rs.member.kind == 'constructor':
                for a in rs.span.field_assignments():
                    if a.name == fi.name and a.rhs is not None and a.qualifier in (None, 'this'):
                        spans.append(a.rhs)
        for sp in spans:
            texts = sp.texts()
            if 'newKeySet' in texts or 'newSetFromMap' in texts:
                return '@ConcurrentKeySet'
            for ne in sp.new_expressions():
                fqn = self.idx.resolve(ne.type.name, sp)
                if fqn in COLLECTION_IMPLS or fqn in COLLECTION_OTHER:
                    return fqn
        return None

    def spell_elem(self, jt, mode):
        """Element spelling inside shims ('shared'), packets ('packet') or confined classes ('confined')."""
        c = jt.cat
        if c in ('prim', 'boxed', 'string', 'enum', 'ext'):
            if c == 'boxed':
                return BOXED[jt.fqn]
            return self.spell_value(jt)
        if c == 'class':
            kind, name = self.ref_target(jt)
            if kind in ('template', 'immortal'):
                return f'const {name}*'
            if kind == 'shared_ptr':
                return f'std::shared_ptr<{name}>'
            if mode == 'confined':
                return f'{name}*' if jt.ci.kind == K5 else f'Ptr<{name}>'
            return f'Ref<{name}>'
        if c == 'coll':
            args = [self.spell_elem(a, mode) for a in jt.args]
            if mode in ('packet', 'confined'):
                return self.std_collection(jt, args)
            shim, rc, _ = self.shim_for(jt)
            inner = f'{shim}<{", ".join(args)}>' if args else shim
            return f'Ref<{rc}<{", ".join(args)}>>' if rc and args else f'Ref<Rc<{inner}>>'
        if c == 'array':
            e = self.spell_elem(jt.args[0], mode)
            return f'std::vector<{e}>' if mode in ('packet', 'confined') else f'Ref<Array<{e}>>'
        if c == 'future':
            return 'FutureRef'
        if c == 'atomic':
            return f'Ref<Rc<{self.atomic_spelling(jt)}>>'
        if c == 'functional':
            return f'PinnedCallback<{self.functional_sig(jt)}>'
        if c == 'object':
            return jt.tvar and f'Ref<{jt.tvar}>' or 'Ref<RefCounted>'
        return jt.name or '?'

    def std_collection(self, jt, args):
        impl = COLLECTION_IFACES.get(jt.fqn, jt.fqn)
        simple = impl.rpartition('.')[2]
        table = {'ArrayList': 'std::vector', 'LinkedList': 'std::vector', 'ArrayDeque': 'std::deque', 'HashMap': 'std::unordered_map',
                 'TreeMap': 'std::map', 'HashSet': 'std::unordered_set', 'TreeSet': 'std::set', 'LinkedHashMap': 'std::vector<std::pair',
                 'LinkedHashSet': 'std::vector', 'EnumMap': 'std::map', 'PriorityQueue': 'std::priority_queue', 'ConcurrentHashMap': 'std::unordered_map',
                 'CopyOnWriteArrayList': 'std::vector'}
        std = table.get(simple, 'std::vector')
        if std == 'std::vector<std::pair':
            return f'std::vector<std::pair<{", ".join(args)}>>'
        return f'{std}<{", ".join(args)}>'

    def atomic_spelling(self, jt):
        shim = ATOMICS[jt.fqn]
        if shim == 'AtomicReference':
            inner = self.spell_elem(jt.args[0], 'shared') if jt.args else 'Ref<RefCounted>'
            return f'AtomicReference<{inner}>'
        return shim

    def functional_sig(self, jt):
        ret, params = FUNCTIONAL.get(jt.fqn, ('void', ()))

        def arg(i):
            if isinstance(i, str):
                return i
            if i >= len(jt.args):
                return '?'
            a = jt.args[i]
            if a.cat == 'class' and self.ref_target(a)[0] == 'ref':
                return f'{self.ref_target(a)[1]}&'
            return self.spell_plain(a)
        r = arg(ret) if not isinstance(ret, str) else ret
        if isinstance(ret, int) and ret < len(jt.args) and jt.args[ret].cat == 'class' and self.ref_target(jt.args[ret])[0] == 'ref':
            r = f'Ref<{self.ref_target(jt.args[ret])[1]}>'
        return f'{r}({", ".join(arg(p) for p in params)})'

    def shared_member(self, jt, final, fi=None):
        """(cpp, rule, flags) of a K3/K4 instance member or a captured variable."""
        c = jt.cat
        fin = 'final' if final else 'non-final'
        if c in ('prim', 'enum'):
            t = self.spell_value(jt)
            return (f'const {t}', f'{fin} scalar', []) if final else (f'Field<{t}>', f'{fin} scalar', [])
        if c == 'boxed':
            t = f'std::optional<{BOXED[jt.fqn]}>'
            return (f'const {t}', f'{fin} nullable scalar', []) if final else (f'Field<{t}>', f'{fin} nullable scalar', [])
        if c == 'string':
            return ('const std::string', 'final string', []) if final else ('Field<std::string>', 'non-final string', [])
        if c == 'class':
            kind, name = self.ref_target(jt)
            flags = []
            if jt.ci.kind == K5 and jt.ci.kind_reason.startswith('fieldmap.toml'):
                return (name, 'member of a class confined by fieldmap.toml', [])
            if jt.ci.kind == K5:
                flags.append('confinedInShared')
            if kind == 'template':
                return (f'const {name}*', 'final template reference', flags) if final else (f'Field<const {name}*>', 'non-final template reference', flags)
            if kind == 'immortal':
                return (f'const {name}*', 'final immortal reference', flags) if final else (f'Field<const {name}*>', 'non-final immortal reference', flags)
            if kind == 'shared_ptr':
                rule = 'connection' if self._is_connection(jt.ci) else 'packet reference'
                return (f'const std::shared_ptr<{name}>', rule, flags) if final else (f'Field<std::shared_ptr<{name}>>', rule, flags)
            return (f'const Ref<{name}>', 'final object reference', flags) if final else (f'Field<Ref<{name}>>', 'non-final object reference', flags)
        if c == 'coll':
            shim, rc, flags = self.shim_for(jt, fi)
            args = [self.spell_elem(a, 'shared') for a in jt.args]
            if final:
                return (f'{shim}<{", ".join(args)}>' if args else shim, 'final collection', flags)
            inner = f'{rc}<{", ".join(args)}>' if rc and args else f'Rc<{shim}<{", ".join(args)}>>'
            return (f'Field<Ref<{inner}>>', 'non-final collection', flags)
        if c == 'array':
            e = self.spell_elem(jt.args[0], 'shared')
            return (f'const Ref<Array<{e}>>', 'final array', []) if final else (f'Field<Ref<Array<{e}>>>', 'non-final array', [])
        if c == 'atomic':
            a = self.atomic_spelling(jt)
            return (a, 'final atomic', []) if final else (f'Field<Ref<Rc<{a}>>>', 'non-final atomic', ['nonFinalAtomic'])
        if c == 'future':
            return ('const FutureRef', 'final future', []) if final else ('Field<FutureRef>', 'non-final future', [])
        if c == 'lock':
            return (LOCKS[jt.fqn], 'lock', [] if final else ['nonFinalLock'])
        if c == 'threadlocal':
            inner = self.spell_elem(jt.args[0], 'confined') if jt.args else '?'
            return (f'static thread_local {inner}', 'thread-local', ['instanceThreadLocal'])
        if c == 'functional':
            sig = self.functional_sig(jt)
            return (f'const PinnedCallback<{sig}>', 'final callback', []) if final else (
                f'Field<std::shared_ptr<const PinnedCallback<{sig}>>>', 'non-final callback', ['callbackField'])
        if c == 'logger':
            return ('const Logger', 'logger', [])
        if c == 'object':
            if fi is not None and isinstance(fi.decl, javasrc.FieldDecl) and fi.decl.initializer is not None and \
                    fi.decl.initializer.texts()[:4] == ['new', 'Object', '(', ')']:
                return ('Monitor', 'lock object', [])
            if jt.tvar:
                return (f'const Ref<{jt.tvar}>', 'final object reference', []) if final else (f'Field<Ref<{jt.tvar}>>', 'non-final object reference', [])
            return ('const Ref<RefCounted>', 'final untyped reference', ['untyped']) if final else (
                'Field<Ref<RefCounted>>', 'non-final untyped reference', ['untyped'])
        if c == 'ext':
            t = self.spell_value(jt)
            flags = [] if jt.fqn in IMMUTABLE_EXTERNAL or jt.fqn in EXTERNAL_SPELLING else ['externalType']
            return (f'const {t}', f'{fin} external value', flags) if final else (f'Field<{t}>', f'{fin} external value', flags)
        return (f'/* {jt.name} */', 'unresolved type', ['unresolved'])

    def plain_member(self, jt, mode):
        c = jt.cat
        if c in ('prim', 'enum', 'string', 'ext'):
            return self.spell_value(jt)
        if c == 'boxed':
            return f'std::optional<{BOXED[jt.fqn]}>'
        if c == 'class':
            return self.spell_elem(jt, mode)
        if c in ('coll', 'array'):
            return self.spell_elem(jt, mode)
        if c == 'future':
            return 'FutureRef'
        if c == 'atomic':
            return self.atomic_spelling(jt)
        if c == 'lock':
            return LOCKS[jt.fqn]
        if c == 'functional':
            return f'PinnedCallback<{self.functional_sig(jt)}>'
        if c == 'logger':
            return 'const Logger'
        if c == 'object':
            return f'Ptr<{jt.tvar}>' if jt.tvar and mode == 'confined' else ('Ref<RefCounted>' if mode == 'packet' else 'Ptr<RefCounted>')
        return f'/* {jt.name} */'

    def _map_all(self):
        overrides = self.cfg['fields']
        seen_override = set()
        for ci in sorted(self.classes.values(), key=lambda c: c.cid):
            for fi in ci.fields:
                key = f'{ci.cid}.{fi.name}'
                self._map_field(ci, fi)
                ov = overrides.get(key)
                if ov is not None:
                    seen_override.add(key)
                    if 'cpp' in ov:
                        fi.cpp = ov['cpp']
                    fi.rule = 'fieldmap.toml override'
                    fi.override_reason = ov['reason']
                self._retains(fi)
            for cap in ci.captures:
                self._map_capture(ci, cap, ci.storage)
        for cb in self.callbacks.values():
            if cb.anon is None:
                for cap in cb.captures:
                    self._map_capture(None, cap, cb)
        unknown = set(overrides) - seen_override
        if unknown:
            raise FieldmapError(f'fieldmap.toml: [fields] entries for unknown fields: {sorted(unknown)}')
        for p in self.parts.values():
            for pt in p.part_types:
                self.classes[pt].part_of.append(f'{p.owner.cid}.{p.field.name}')
        for ci in self.classes.values():
            ci.part_of = sorted(set(ci.part_of))

    def _map_field(self, ci, fi):
        jt = fi.jt
        final = self.is_final(fi)
        if fi.config:
            scalar = jt.cat in ('prim', 'enum', 'boxed')
            fi.cpp = f'static inline std::atomic<{self.spell_value(jt) if jt.cat != "boxed" else BOXED[jt.fqn]}>' if scalar else \
                f'static inline ConfigValue<{self.plain_member(jt, "packet")}>'
            fi.rule = 'config field (CONVENTIONS)'
            return
        if fi.static:
            self._map_static(ci, fi, final)
            return
        if ci.kind == K1:
            if fi.name in ci.runtime_mutable:
                cpp, _, fl = self.shared_member(jt, False, fi)
                fi.cpp = f'mutable {cpp}'
                fi.rule = 'K1 runtime_mutable'
                fi.flags = fl
            else:
                fi.cpp = None
                fi.rule = 'K1: member block generated by xmlgen' if self._jaxb_bound(ci, fi) else 'K1 non-bound member (hand-written, const after load)'
            fi.bound = self._jaxb_bound(ci, fi)
            return
        key = (ci.cid, fi.name)
        p = self.parts.get(key)
        if p is not None and ci.kind in (K3, K4):
            fi.part = p
            X = self.spell_value(jt.args[0]) if jt.cat == 'array' and jt.args else (self.cpp_class_name(jt.ci) if jt.cat == 'class' else self.spell_value(jt))
            if p.element or jt.cat == 'array':
                dims = self._array_dims(fi)
                fi.cpp = f'const std::array<std::unique_ptr<{X}>, {dims}>' if dims else f'PartList<{X}>'
                fi.rule = 'part array (pattern 1)'
            elif (3 in p.patterns or 1 in p.patterns) and final and 2 not in p.patterns:
                fi.cpp = f'const std::unique_ptr<{X}>'
                fi.rule = f'part (pattern {min(p.patterns)})'
            else:
                fi.cpp = f'PartSlot<{X}>'
                fi.rule = f'replaceable part (pattern {min(p.patterns)})'
            return
        orule = self.owner_field_rules.get(key)
        if orule is not None and ci.kind in (K3, K4) and jt.cat in ('class', 'object'):
            name = jt.tvar or (self.cpp_class_name(jt.ci) if jt.cat == 'class' else 'RefCounted')
            if orule[0] == 'late-bound owner':
                fi.cpp = f'Final<{name}*>'
                fi.rule = 'late-bound part owner (OwnerRef via bindOwner, pattern 3)'
            elif orule[0] == 'owner':
                if final or key not in self.owner_written_outside:
                    fi.cpp = f'OwnerRef<{name}>'
                    fi.rule = 'part owner' if final else 'part owner (assigned only in constructors)'
                else:
                    fi.cpp = f'SelfOrRef<{name}>'
                    fi.rule = 'part owner or foreign object'
            else:
                fi.cpp = f'Field<{name}*>'
                fi.rule = 'sibling part'
            return
        if ci.kind == K2:
            fi.cpp = self.plain_member(jt, 'packet')
            fi.rule = 'packet member'
            return
        if ci.kind == K5:
            fi.cpp = self.plain_member(jt, 'confined')
            fi.rule = 'confined member'
            return
        cpp, rule, flags = self.shared_member(jt, final, fi)
        if fi.eff_final and not fi.final and final and rule.startswith('final '):
            rule = 'effectively final ' + rule.removeprefix('final ')
        if fi.volatile:
            rule = rule.replace('non-final', 'volatile')
        fi.cpp, fi.rule, fi.flags = cpp, rule, flags
        if ci.kind == K3 and not cpp.startswith('const') and cpp not in ('Monitor',):
            fi.flags = fi.flags + ['mutableInImmutable']

    def _array_dims(self, fi):
        if isinstance(fi.decl, javasrc.FieldDecl) and fi.decl.initializer is not None:
            for ne in fi.decl.initializer.new_expressions():
                if ne.array and ne.dim_exprs:
                    return ne.dim_exprs[0].text.replace('.', '::')
        return None

    def _map_static(self, ci, fi, final):
        jt = fi.jt
        c = jt.cat
        init = fi.decl.initializer if isinstance(fi.decl, javasrc.FieldDecl) else None
        if c == 'logger':
            fi.cpp, fi.rule = 'static const Logger log', 'logger (CONVENTIONS: static const auto log)'
            return
        if c == 'threadlocal':
            inner = self.plain_member(jt.args[0], 'confined') if jt.args else '?'
            fi.cpp, fi.rule = f'static thread_local {inner}', 'thread-local'
            return
        if final and c in ('prim', 'enum', 'string', 'boxed'):
            t = 'std::string_view' if c == 'string' else (BOXED[jt.fqn] if c == 'boxed' else self.spell_value(jt))
            const_init = init is not None and all(k in (javasrc.INT, javasrc.FLOAT, javasrc.CHAR, javasrc.STRING, javasrc.OP, javasrc.IDENT, javasrc.KEYWORD)
                                                  for k in init.cu.tokens.kind[init.start:init.end]) and '(' not in init.texts() and 'new' not in init.texts()
            if const_init:
                fi.cpp, fi.rule = f'static constexpr {t}', 'static constant'
            else:
                t = 'std::string' if c == 'string' else t
                fi.cpp, fi.rule = f'static inline const {t}', 'static final value'
            return
        if final and c == 'coll':
            shim, rc, flags = self.shim_for(jt, fi)
            args = [self.spell_elem(a, 'shared') for a in jt.args]
            fi.cpp, fi.rule, fi.flags = f'static inline {shim}<{", ".join(args)}>' if args else f'static inline {shim}', 'static shim', flags
            return
        if final and c == 'class':
            kind, name = self.ref_target(jt)
            if jt.ci.singleton and (jt.ci is ci or (ci.lex_parent is not None and jt.ci is ci.lex_parent and ci.td.name == 'SingletonHolder')):
                fi.cpp, fi.rule, fi.flags = f'static {name}& getInstance()', 'singleton instance (CONVENTIONS: function-local static)', ['singleton']
                return
            if kind == 'immortal':
                fi.cpp, fi.rule = f'static inline const {name}*', 'static immortal reference'
            elif kind == 'template':
                fi.cpp, fi.rule = f'static inline const {name}*', 'static template reference'
            elif kind == 'shared_ptr':
                fi.cpp, fi.rule = f'static inline const std::shared_ptr<{name}>', 'static reference'
            else:
                fi.cpp, fi.rule = f'static inline const Ref<{name}>', 'static reference'
            if jt.ci is ci:
                fi.rule = 'named constant of the own type'  # not a singleton (several constants, or instances created elsewhere)
            return
        if final and c == 'functional':
            fi.cpp, fi.rule = f'static inline const PinnedCallback<{self.functional_sig(jt)}>', 'static callback'
            return
        if final and c == 'array':
            e = self.spell_elem(jt.args[0], 'shared')
            flags = ['constantArray'] if init is not None and init.texts()[:1] == ['{'] else []
            fi.cpp, fi.rule, fi.flags = f'static inline const Ref<Array<{e}>>', 'static array', flags
            return
        cpp, rule, flags = self.shared_member(jt, final, fi)
        if cpp.startswith('static '):
            fi.cpp = cpp
        else:
            fi.cpp = f'static inline {cpp}'
        fi.rule = 'static ' + rule
        fi.flags = flags

    def _map_capture(self, ci, cap, storage_cb):
        jt = cap.jt
        owners = storage_cb.owners if storage_cb is not None and storage_cb.storage == 'stored' else []
        # OwnerRef only if the captured object owns every list holding the callback (runtime-architecture.md §3.2); a callback also stored
        # by another object (Effect's observers live in the effected creature's ObserveController) retains the captured object
        if cap.kind == 'this' and jt.ci is not None and owners and all(o is jt.ci or o in self.all_subtypes(jt.ci) for o in owners):
            cap.cpp = f'OwnerRef<{self.cpp_class_name(jt.ci)}>'
            cap.rule = 'captured owner of the storing object'
            return
        if jt.cat == 'unknown':
            cap.cpp = '/* unknown type */'
            cap.rule = 'captured variable of unknown type'
            return
        cpp, rule, _ = self.shared_member(jt, True)
        if jt.cat == 'coll':
            shim, rc, _ = self.shim_for(jt)
            args = [self.spell_elem(a, 'shared') for a in jt.args]
            cpp = f'const Ref<{rc}<{", ".join(args)}>>' if rc and args else f'const Ref<Rc<{shim}<{", ".join(args)}>>>'
            rule = 'captured collection'
        cap.cpp = cpp
        cap.rule = 'captured ' + ('this' if cap.kind == 'this' else 'variable') + ': ' + rule

    def _retains(self, fi):
        fi.retains = []
        if fi.static or fi.ci.kind not in (K3, K4):
            return
        orule = self.owner_field_rules.get((fi.ci.cid, fi.name))
        if orule is not None and orule[0] in ('late-bound owner', 'sibling part'):
            return
        if orule is not None and orule[0] == 'owner' and fi.cpp and fi.cpp.startswith('OwnerRef'):
            return
        if fi.cpp is None:
            return
        out = []
        for c in self._type_classes(fi.jt, generic_args=False):
            if c.kind in (K3, K4) and not c.scalar_like and not (c.immortal or c.singleton):
                out.append(c.cid)
        fi.retains = sorted(set(out))

    # -- equals and synchronization
    def overrides_info(self, ci):
        res = {}
        if ci.td.kind == 'record':
            res['equals'] = res['hashCode'] = ci.cid
        for c in self.superclass_chain(ci):
            for md in c.td.methods:
                if md.kind != 'method' or 'static' in md.modifiers:
                    continue
                if md.name == 'equals' and len(md.params) == 1 and md.params[0].type.name in ('Object', 'java.lang.Object'):
                    res.setdefault('equals', c.cid)
                elif md.name == 'hashCode' and not md.params:
                    res.setdefault('hashCode', c.cid)
                elif md.name == 'compareTo' and len(md.params) == 1:
                    res.setdefault('compareTo', c.cid)
        return res

    def _sync(self):
        for rs in self.root_spans:
            if rs.ci.origin != 'output' or not isinstance(rs.member, javasrc.MethodDecl):
                continue
            sp = rs.span
            tk = sp.cu.tokens
            t = tk.text
            excluded = [(ne.anonymous.body.start, ne.anonymous.body.end) for ne in sp.anonymous_classes()]
            excluded += [(lt.body.start, lt.body.end) for lt in sp.local_types()]
            n_sync = 1 if 'synchronized' in rs.member.modifiers else 0
            for i in range(sp.start, sp.end):
                if t[i] == 'synchronized' and tk.kind[i] is javasrc.KEYWORD and not any(a <= i < b for a, b in excluded):
                    n_sync += 1
            n_lock = sum(1 for c in sp.method_calls() if c.name in ('lock', 'tryLock', 'lockInterruptibly', 'readLock', 'writeLock')
                         and not c.args and c.receiver is not None and not any(a <= c.index < b for a, b in excluded))
            if n_sync or n_lock:
                d = self.sync_counts.setdefault(rs.ci.cid, {}).setdefault(rs.member.name, {'synchronized': 0, 'lock': 0})
                d['synchronized'] += n_sync
                d['lock'] += n_lock
        # anonymous class methods
        for ci in self.classes.values():
            if ci.origin != 'output' or not (ci.td.anonymous or ci.td.local) or ci.root_span is None:
                continue
            for md in ci.td.methods:
                if md.body is None:
                    continue
                t = md.body.texts()
                n_sync = t.count('synchronized') + (1 if 'synchronized' in md.modifiers else 0)
                if n_sync:
                    d = self.sync_counts.setdefault(ci.cid, {}).setdefault(md.name, {'synchronized': 0, 'lock': 0})
                    d['synchronized'] += n_sync

    # -- cycles
    def _cycles(self):
        nodes = {}
        edges = {}  # u -> list of (v, key, kind)

        def node(ci):
            return ci.cid

        def add(u, v, key, kind, detail=''):
            edges.setdefault(u, []).append((v, key, kind, detail))
            nodes.setdefault(u, None)
            nodes.setdefault(v, None)

        def expand(ci):
            return [s for s in self.all_subtypes(ci) if s.kind in (K3, K4)]

        shared = [ci for ci in sorted(self.classes.values(), key=lambda c: c.cid) if ci.kind in (K3, K4) and ci.origin == 'output']
        for ci in shared:
            if ci.superclass is not None and ci.superclass.kind in (K3, K4):
                add(ci.cid, ci.superclass.cid, 'extends', 'extends')
            for fi in ci.fields:
                if fi.static or not (fi.retains or fi.part):
                    continue
                kind = 'part' if fi.part is not None else 'field'
                targets = set(fi.retains)
                if fi.part is not None:
                    targets |= set(fi.part.part_types)
                for t in sorted(targets):
                    for s in expand(self.classes[t]):
                        add(ci.cid, s.cid, f'{ci.cid}.{fi.name}', kind)
            for cap in ci.captures:
                if cap.cpp and cap.cpp.startswith('OwnerRef'):
                    continue
                for c in self._type_classes(cap.jt, generic_args=False):
                    if c.kind in (K3, K4) and not c.scalar_like:
                        for s in expand(c):
                            add(ci.cid, s.cid, f'{ci.cid}#{cap.name}', 'capture')
        for cb in sorted(self.callbacks.values(), key=lambda c: c.id):
            if cb.storage in ('sync', 'local') or not cb.owners:
                continue
            target = cb.anon.cid if cb.anon is not None else f'cb:{cb.id}'
            if cb.anon is not None and cb.anon.kind not in (K3, K4):
                continue
            for o in cb.owners:
                if o.kind in (K3, K4):
                    for s in expand(o):
                        add(s.cid, target, 'stored', 'stored', cb.api)
            if cb.anon is None:
                for cap in cb.captures:
                    if cap.cpp and cap.cpp.startswith('OwnerRef'):
                        continue
                    for c in self._type_classes(cap.jt, generic_args=False):
                        if c.kind in (K3, K4) and not c.scalar_like:
                            for s in expand(c):
                                add(target, s.cid, f'{cb.id}#{cap.name}', 'capture')
        for u in edges:
            edges[u] = sorted(set(edges[u]))
        order = sorted(nodes)
        scc = self._tarjan(order, edges)
        comp_of = {}
        for i, comp in enumerate(scc):
            for n in comp:
                comp_of[n] = i
        cyclic = []
        for i, comp in enumerate(scc):
            if len(comp) > 1 or any(v == comp[0] for v, _, _, _ in edges.get(comp[0], ())):
                cyclic.append(i)
        cyclic.sort(key=lambda i: (-len(scc[i]), sorted(scc[i])[0]))
        self.cycle_sccs = []
        self.cycle_edges = {}
        for rank, i in enumerate(cyclic, 1):
            comp = set(scc[i])
            by_key = {}
            preds = {}
            for u in comp:
                for v, key, kind, detail in edges.get(u, ()):
                    if v in comp:
                        preds.setdefault(v, []).append((u, key, kind))
                        if kind in ('field', 'capture'):
                            by_key.setdefault(key, []).append((u, v, kind))
            dist_cache = {}
            entries = []
            for key in sorted(by_key):
                pairs = sorted(set(by_key[key]))
                best = None
                for u, v, kind in pairs:
                    if u not in dist_cache:
                        dist_cache[u] = self._reverse_bfs(u, comp, preds)
                    dist, nxt = dist_cache[u]
                    if v in dist and (best is None or dist[v] < best[0]):
                        best = (dist[v], u, v, kind)
                if best is None:
                    continue
                _, u, v, kind = best
                path = [(u, key, v)]
                cur = v
                dist, nxt = dist_cache[u]
                guard = 0
                while cur != u and guard < 200:
                    step = nxt[cur]
                    path.append((cur, step[1], step[0]))
                    cur = step[0]
                    guard += 1
                targets = sorted({v for _, v, _ in pairs})
                res = self.resolutions.get(key)
                entry = {'scc': rank, 'kind': kind, 'from': u, 'targets': targets, 'example': [[a, k, b] for a, k, b in path],
                         'resolution': res}
                sugg = self._suggest(key, kind)
                if sugg:
                    entry['suggestion'] = sugg
                self.cycle_edges[key] = entry
                entries.append(key)
            self.cycle_sccs.append({'rank': rank, 'nodes': sorted(comp), 'edges': entries})
        self.stale_resolutions = sorted(k for k in self.resolutions if k not in self.cycle_edges)

    OBSERVER_OWNERS = frozenset(('com.aionemu.gameserver.controllers.ObserveController',))

    def _suggest(self, key, kind):
        """A review hint for cycles.toml (never applied automatically)."""
        if kind != 'capture':
            return None
        site = key.rpartition('#')[0]
        cb = self.callbacks.get(site)
        if cb is None:
            return None
        if cb.storage == 'task':
            if cb.api == 'scheduleAtFixedRate':
                return f'java-hook: cancel of the periodic task{" held by " + cb.future_holder if cb.future_holder else ""}'
            return 'accepted: one-shot task releases its captures when it runs or is cancelled'
        if any(o.cid in self.OBSERVER_OWNERS for o in cb.owners):
            return 'cpp-breaker: LogoutBreakers::run / onDelete clear observers without notification (design §5.1)'
        return None

    @staticmethod
    def _reverse_bfs(target, comp, preds):
        dist = {target: 0}
        nxt = {}
        q = deque([target])
        while q:
            x = q.popleft()
            for u, key, kind in sorted(preds.get(x, ())):
                if u not in dist:
                    dist[u] = dist[x] + 1
                    nxt[u] = (x, key, kind)
                    q.append(u)
        return dist, nxt

    @staticmethod
    def _tarjan(order, edges):
        index = {}
        low = {}
        on = set()
        stack = []
        out = []
        counter = 0
        for root in order:
            if root in index:
                continue
            work = [(root, 0)]
            index[root] = low[root] = counter
            counter += 1
            stack.append(root)
            on.add(root)
            while work:
                v, i = work[-1]
                es = edges.get(v, ())
                if i < len(es):
                    work[-1] = (v, i + 1)
                    w = es[i][0]
                    if w not in index:
                        index[w] = low[w] = counter
                        counter += 1
                        stack.append(w)
                        on.add(w)
                        work.append((w, 0))
                    elif w in on:
                        low[v] = min(low[v], index[w])
                else:
                    work.pop()
                    if work:
                        p = work[-1][0]
                        low[p] = min(low[p], low[v])
                    if low[v] == index[v]:
                        comp = []
                        while True:
                            w = stack.pop()
                            on.discard(w)
                            comp.append(w)
                            if w == v:
                                break
                        out.append(sorted(comp))
        return out

    # ------------------------------------------------------------------------------------------------------------------------------
    # Outputs
    # ------------------------------------------------------------------------------------------------------------------------------
    def class_json(self, ci):
        d = {'kind': ci.kind, 'kindName': KIND_NAMES.get(ci.kind), 'reason': ci.kind_reason, 'file': ci.file, 'line': ci.td.line,
             'javaKind': ci.td.kind, 'cppName': ci.cpp_name}
        if ci.superclass is not None:
            d['extends'] = ci.superclass.cid
        ifaces = [s.cid for s in ci.supers if s is not ci.superclass]
        if ifaces:
            d['implements'] = ifaces
        if ci.external_super:
            d['externalSuper'] = True
        base = self.base_of(ci)
        if base:
            d['base'] = base
        ov = self.overrides_info(ci)
        d['hasEquals'] = 'equals' in ov
        if ov.get('equals'):
            d['equalsFrom'] = ov['equals']
        if ov.get('hashCode'):
            d['hashCodeFrom'] = ov['hashCode']
        if ov.get('compareTo'):
            d['compareToFrom'] = ov['compareTo']
        if ci.part_of:
            d['partOf'] = ci.part_of
        if ci.singleton:
            d['singleton'] = True
        if ci.immortal:
            d['immortal'] = True
        if ci.td.type_params:
            d['typeParams'] = [p.name for p in ci.td.type_params]
        fields = []
        for fi in ci.fields:
            f = {'name': fi.name, 'java': fi.java, 'line': fi.line, 'cpp': fi.cpp, 'rule': fi.rule}
            mods = [m for m in (fi.decl.modifiers if hasattr(fi.decl, 'modifiers') and not fi.record_component else [])]
            if fi.record_component:
                mods = ['private', 'final']
            if mods:
                f['modifiers'] = mods
            if fi.eff_final:
                f['effectivelyFinal'] = True
            if fi.flags:
                f['flags'] = sorted(set(fi.flags))
            if fi.retains:
                f['retains'] = fi.retains
            if fi.part is not None:
                f['part'] = sorted(fi.part.patterns)
            if fi.override_reason:
                f['overrideReason'] = fi.override_reason
            if fi.bound:
                f['xmlBound'] = True
            fields.append(f)
        d['fields'] = fields
        if ci.captures:
            d['captures'] = [self.capture_json(c) for c in ci.captures]
        if ci.td.anonymous and ci.new_expr is not None and ci.storage is not None:
            d['storage'] = ci.storage.storage
            if ci.storage.owners:
                d['storageOwners'] = [o.cid for o in ci.storage.owners]
        if ci.cid in self.sync_counts:
            d['sync'] = self.sync_counts[ci.cid]
        if ci.callbacks:
            d['callbacks'] = sorted(ci.callbacks)
        members = self.skeleton_members(ci)
        if members:
            d['members'] = members
        structs = self._structs_by_class().get(ci.cid)
        if structs:
            d['extraDeclarations'] = structs
        return d

    def _structs_by_class(self):
        """Generated callback structs of anonymous and local classes, attached to their innermost named enclosing class."""
        if getattr(self, '_structs_cache', None) is None:
            out = {}
            for other in sorted(self.classes.values(), key=lambda c: c.cid):
                if other.origin != 'output' or not ((other.td.anonymous and other.const is None) or other.td.local):
                    continue
                named = other.lex_parent
                while named is not None and (named.td.anonymous or named.td.local):
                    named = named.lex_parent
                if named is not None:
                    out.setdefault(named.cid, []).append(self.callback_struct(other))
            self._structs_cache = out
        return self._structs_cache

    def skeleton_members(self, ci):
        """Member layout in the interface read by tools/gen/skeleton.py: javaName, cppType or declaration, static, initializer,
        comment. K1 members generated by xmlgen and members without a C++ spelling are left out."""
        out = []
        for fi in ci.fields:
            if fi.cpp is None:
                continue
            m = {'javaName': fi.name}
            cpp = fi.cpp
            if cpp.endswith('getInstance()'):
                m['declaration'] = cpp + ';'
            elif fi.rule.startswith('logger'):
                m['declaration'] = f'// logger: static const auto log = LoggerFactory::getLogger("{ci.cid}"); at namespace scope (CONVENTIONS)'
            elif cpp.startswith('static '):
                m['static'] = True
                m['cppType'] = cpp[len('static '):]
                init = self._constant_initializer(fi)
                if init is not None and 'constexpr' in cpp:
                    m['initializer'] = init
            else:
                m['cppType'] = cpp
            if fi.override_reason:
                m['comment'] = f'fieldmap.toml: {fi.override_reason}'
            out.append(m)
        for cap in ci.captures:
            if cap.cpp:
                out.append({'javaName': cap.name, 'cppName': self._cap_member(cap), 'cppType': cap.cpp, 'comment': f'captured {cap.kind} (line {cap.line})'})
        return out

    @staticmethod
    def _constant_initializer(fi):
        init = fi.decl.initializer if isinstance(fi.decl, javasrc.FieldDecl) else None
        if init is None or init.end - init.start > 2:
            return None
        tk = init.cu.tokens
        texts = tk.text[init.start:init.end]
        kinds = tk.kind[init.start:init.end]
        if len(texts) == 2 and texts[0] != '-':
            return None
        lit = texts[-1]
        kind = kinds[-1]
        if kind == javasrc.INT:
            lit = lit.replace('_', '').rstrip('lL')
        elif kind == javasrc.FLOAT:
            lit = lit.replace('_', '').rstrip('dD')
        elif kind == javasrc.KEYWORD and lit in ('true', 'false'):
            pass
        elif kind != javasrc.STRING:
            return None
        return ('-' if len(texts) == 2 else '') + lit

    def base_of(self, ci):
        if ci.kind == K1:
            return 'StaticTemplate'
        if ci.kind == K2:
            return None
        if ci.kind == K5 or ci.is_interface:
            return None
        if ci.part_of:
            return 'OwnedPart'
        if ci.immortal:
            return 'Immortal'
        if ci.superclass is not None and ci.superclass.kind in (K3, K4):
            return None  # the ancestor's base (a singleton subclass of a shared class keeps it: one lifetime base per class tree)
        if ci.singleton:
            return 'Immortal'
        return 'RefCounted'

    @staticmethod
    def capture_json(c):
        return {'name': c.name, 'kind': c.kind, 'java': c.java, 'line': c.line, 'cpp': c.cpp, 'rule': c.rule,
                'type': c.jt.ci.cid if c.jt is not None and c.jt.ci is not None else (c.jt.fqn if c.jt is not None else None)}

    def callback_json(self, cb):
        d = {'kind': cb.kind, 'file': _rel(cb.rs.span.cu.path), 'line': cb.line, 'enclosing': cb.ci.cid, 'context': cb.context,
             'storage': cb.storage}
        if cb.api:
            d['api'] = cb.api
        if cb.owners:
            d['owners'] = [o.cid for o in cb.owners]
        if cb.future_holder:
            d['futureHolder'] = cb.future_holder
        if cb.anon is not None:
            d['class'] = cb.anon.cid
            d['struct'] = cb.anon.cpp_name
        else:
            d['captures'] = [self.capture_json(c) for c in cb.captures]
            d['pins'] = self.pin_list(cb)
        return d

    def pin_list(self, cb):
        pins = []
        for c in cb.captures:
            if c.kind == 'this':
                pins.append('this')
            elif c.jt is not None and c.jt.cat == 'class' and c.jt.ci.kind in (K3, K4):
                pins.append(f'&{c.name}')
        return pins

    def summary(self):
        kinds = {}
        for ci in self.classes.values():
            if ci.origin == 'output':
                kinds[ci.kind] = kinds.get(ci.kind, 0) + 1
        fields = {}
        flagged = {}
        n_fields = 0
        for ci in self.classes.values():
            if ci.origin != 'output':
                continue
            for fi in ci.fields:
                n_fields += 1
                fields[fi.rule] = fields.get(fi.rule, 0) + 1
                for fl in fi.flags:
                    flagged[fl] = flagged.get(fl, 0) + 1
        storage = {}
        for cb in self.callbacks.values():
            storage[f'{cb.kind}:{cb.storage}'] = storage.get(f'{cb.kind}:{cb.storage}', 0) + 1
        unresolved = sum(1 for e in self.cycle_edges.values() if e['resolution'] is None)
        return {'classes': sum(kinds.values()), 'kinds': dict(sorted(kinds.items())), 'fields': n_fields,
                'rules': dict(sorted(fields.items())), 'flags': dict(sorted(flagged.items())), 'callbacks': dict(sorted(storage.items())),
                'parts': len(self.parts), 'cyclicComponents': len(self.cycle_sccs), 'cycleEdges': len(self.cycle_edges),
                'unresolvedCycleEdges': unresolved, 'staleResolutions': len(self.stale_resolutions), 'warnings': len(self.warnings)}

    def fieldmap_json_text(self, inputs):
        lines = ['{', f'"format": "aion-fieldmap",', f'"version": {FORMAT_VERSION},',
                 f'"generator": "cpp/tools/gen/fieldmap.py",', f'"inputs": {json.dumps(inputs, sort_keys=True)},',
                 f'"summary": {json.dumps(self.summary(), sort_keys=True)},']

        def block(name, items, last=False):
            lines.append(f'"{name}": {{')
            for i, (k, v) in enumerate(items):
                sep = ',' if i < len(items) - 1 else ''
                lines.append(f'{json.dumps(k)}: {json.dumps(v, sort_keys=True, ensure_ascii=False, separators=(",", ":"))}{sep}')
            lines.append('}' + ('' if last else ','))
        classes = [(ci.cid, self.class_json(ci)) for ci in sorted(self.classes.values(), key=lambda c: c.cid) if ci.origin == 'output']
        block('classes', classes)
        block('callbacks', [(k, self.callback_json(self.callbacks[k])) for k in sorted(self.callbacks)])
        block('cycleEdges', [(k, self.cycle_edges[k]) for k in sorted(self.cycle_edges)])
        lines.append(f'"staleResolutions": {json.dumps(self.stale_resolutions)},')
        lines.append(f'"warnings": {json.dumps(self.warnings)}')
        lines.append('}')
        return '\n'.join(lines) + '\n'

    def parts_json_text(self):
        parts = []
        for (cid, fname), p in sorted(self.parts.items()):
            fi = p.field
            parts.append({'owner': cid, 'field': fname, 'java': fi.java, 'line': fi.line, 'patterns': sorted(p.patterns),
                          'partTypes': sorted(p.part_types), 'ownerClasses': sorted(p.owner_classes), 'cpp': fi.cpp, 'rule': fi.rule,
                          'evidence': p.evidence})
        owner_fields = []
        for (cid, fname), (rule, p) in sorted(self.owner_field_rules.items()):
            ci = self.classes[cid]
            fi = self._field_in_chain(ci, fname)
            owner_fields.append({'class': cid, 'field': fname, 'rule': rule, 'cpp': fi.cpp if fi else None,
                                 'part': f'{p.owner.cid}.{p.field.name}' if p is not None else None})
        text = json.dumps({'format': 'aion-parts', 'version': FORMAT_VERSION, 'parts': parts, 'ownerFields': owner_fields}, indent=1,
                          sort_keys=True, ensure_ascii=False)
        return text + '\n'

    def cycles_report_text(self):
        out = ['# Capture-aware reference cycles (generated by cpp/tools/gen/fieldmap.py)', '',
               'Type-level retaining graph of K3/K4 classes (design runtime-architecture.md §5.3). Edge kinds: `field` (retaining member, targets',
               'expanded to every K3/K4 subtype), `capture` (captured variable of a stored lambda, method reference or anonymous/inner class),',
               '`part` (owner -> part), `extends` (a subclass contains its superclass part) and `stored` (storage owner -> callback). Every',
               '`field` and `capture` edge inside a strongly connected component needs a `cycles.toml` resolution:',
               '`part` | `java-hook: <method>` | `cpp-breaker: <method>` | `zombie-safe: <edge>` | `accepted: <why>`.', '']
        s = self.summary()
        out.append(f'Components with cycles: {s["cyclicComponents"]}. Edges needing a resolution: {s["cycleEdges"]}, unresolved: '
                   f'{s["unresolvedCycleEdges"]}. Stale cycles.toml keys: {s["staleResolutions"]}.')
        out.append('')
        out.append('Resolution skeleton for cycles.toml (copy the unresolved keys and fill in the value):')
        out.append('')
        out.append('```toml')
        out.append('[resolutions]')
        for key in sorted(self.cycle_edges):
            if self.cycle_edges[key]['resolution'] is None:
                sugg = self.cycle_edges[key].get('suggestion', '')
                out.append(f'# "{key}" = "{sugg}"')
        out.append('```')
        out.append('')
        for comp in self.cycle_sccs:
            nodes = comp['nodes']
            out.append(f'## Component {comp["rank"]}: {len(nodes)} nodes, {len(comp["edges"])} edges')
            out.append('')
            shown = ', '.join(f'`{self._short(n)}`' for n in nodes[:40])
            out.append(f'Nodes: {shown}{" ..." if len(nodes) > 40 else ""}')
            out.append('')
            out.append('| Edge | Kind | Example cycle | Resolution |')
            out.append('|---|---|---|---|')
            for key in comp['edges']:
                e = self.cycle_edges[key]
                cyc = ' → '.join([self._short(e['example'][0][0])] + [f'`{self._short_key(k)}` {self._short(b)}' for a, k, b in e['example']])
                res = e['resolution'] or ('**UNRESOLVED**' + (f' (suggested: {e["suggestion"]})' if e.get('suggestion') else ''))
                out.append(f'| `{key}` | {e["kind"]} | {cyc} | {res} |')
            out.append('')
        if self.stale_resolutions:
            out.append('## Stale cycles.toml keys')
            out.append('')
            for k in self.stale_resolutions:
                out.append(f'- `{k}`')
            out.append('')
        return '\n'.join(out) + '\n'

    @staticmethod
    def _short(cid):
        if cid.startswith('cb:'):
            return 'λ' + FieldMap._short(cid[3:])
        head, sep, tail = cid.partition('@')
        return head.rpartition('.')[2] + sep + tail if not head.endswith(')') else cid

    @staticmethod
    def _short_key(key):
        if key in ('extends', 'stored'):
            return key
        if '#' in key:
            left, _, cap = key.rpartition('#')
            return f'{FieldMap._short(left)}#{cap}'
        cls, _, f = key.rpartition('.')
        return f'{cls.rpartition(".")[2]}.{f}'

    def escape_report_text(self):
        out = ['# Escape analysis and class kinds (generated by cpp/tools/gen/fieldmap.py)', '',
               'K5 CONFINED classes never appear in shared state (design runtime-architecture.md §3.1); every other kind lists the first',
               f'reason found by the fixpoint. K1 source: {self.staticdata_source}.', '']
        s = self.summary()
        out.append('| Kind | Classes |')
        out.append('|---|---|')
        for k, n in s['kinds'].items():
            out.append(f'| {k} {KIND_NAMES.get(k, "")} | {n} |')
        out.append('')
        for kind in (K5, K4, K3, K2, K1):
            cis = [ci for ci in sorted(self.classes.values(), key=lambda c: c.cid) if ci.origin == 'output' and ci.kind == kind]
            out.append(f'## {kind} {KIND_NAMES[kind]} ({len(cis)})')
            out.append('')
            for ci in cis:
                if kind in (K1, K2):
                    out.append(f'- `{ci.cid}`: {ci.kind_reason}')
                else:
                    out.append(f'- `{ci.cid}` ({ci.file}:{ci.td.line}): {ci.kind_reason}')
            out.append('')
        flagged = []
        for ci in sorted(self.classes.values(), key=lambda c: c.cid):
            if ci.origin != 'output':
                continue
            for fi in ci.fields:
                if set(fi.flags) & FLAGS_REPORTED:
                    flagged.append(f'- `{ci.cid}.{fi.name}` `{fi.java}` → `{fi.cpp}`: {", ".join(sorted(fi.flags))}')
        out.append(f'## Flagged fields ({len(flagged)})')
        out.append('')
        out.extend(flagged)
        out.append('')
        if self.warnings:
            out.append('## Warnings')
            out.append('')
            out.extend(f'- {w}' for w in self.warnings)
            out.append('')
        return '\n'.join(out) + '\n'

    # -- --class
    def member_block(self, cid):
        ci = self.classes.get(cid)
        if ci is None:
            raise FieldmapError(f'unknown class {cid}')
        out = [f'// fieldmap.py --class {cid}', f'// {ci.file}:{ci.td.line}',
               f'// kind: {ci.kind} {KIND_NAMES.get(ci.kind, "")} ({ci.kind_reason})']
        base = self.base_of(ci)
        ov = self.overrides_info(ci)
        extra = []
        if base:
            extra.append(f'base: {base}')
        if ci.superclass is not None:
            extra.append(f'extends {ci.superclass.cpp_name}')
        if ci.part_of:
            extra.append(f'part of {", ".join(ci.part_of)}')
        extra.append(f'hasEquals: {"from " + ov["equals"] if "equals" in ov else "no (identity)"}')
        out.append('// ' + '; '.join(extra))
        if ci.kind == K5:
            out.append('// confined: inferred')
        if ci.kind == K1:
            out.append('// K1: the bound member block is generated by xmlgen; non-bound members below')
        width = max([len(fi.cpp or '') + len(fi.name) for fi in ci.fields] + [0]) + 3
        for fi in ci.fields:
            if fi.cpp is None:
                continue
            decl = self._decl(fi.cpp, fi.name)
            mods = ' '.join(fi.decl.modifiers) + ' ' if hasattr(fi.decl, 'modifiers') and fi.decl.modifiers else ''
            note = f'// {mods}{fi.java} {fi.name} ({os.path.basename(ci.cu.path)}:{fi.line}) [{fi.rule}]'
            if fi.flags:
                note += f' flags: {", ".join(sorted(set(fi.flags)))}'
            out.append(f'\t{decl.ljust(width)}{note}')
        if ci.captures:
            out.append('\t// captured variables:')
            for cap in ci.captures:
                out.append(f'\t{self._decl(cap.cpp, self._cap_member(cap)).ljust(width)}// {cap.kind} {cap.java} {cap.name} (line {cap.line}) [{cap.rule}]')
        if ci.cid in self.sync_counts:
            out.append('// synchronized/lock per method: ' + ', '.join(f'{m}: {v["synchronized"]}/{v["lock"]}' for m, v in sorted(self.sync_counts[ci.cid].items())))
        structs = []
        lambdas = []
        for other in sorted(self.classes.values(), key=lambda c: c.cid):
            if (other.td.anonymous and other.const is None or other.td.local) and self._declared_within(other.lex_parent, ci):
                structs.append(self.callback_struct(other))
        for cbid in sorted(self._callbacks_within(ci)):
            cb = self.callbacks[cbid]
            if cb.anon is None:
                caps = ', '.join(f'{c.name} ({c.cpp})' for c in cb.captures) or 'none'
                where = f'{cb.storage}' + (f' in {", ".join(o.simple for o in cb.owners)}' if cb.owners else '') + \
                    (f' via {cb.future_holder}' if cb.future_holder else '')
                pins = self.pin_list(cb)
                lambdas.append(f'// {cb.kind} at line {cb.line}: {cb.context}; {where}; pin {{{", ".join(pins)}}}; captures: {caps}')
        if structs:
            out.append('')
            out.append('// generated callback structs')
            out.extend(structs)
        if lambdas:
            out.append('')
            out.append('// stored lambdas and method references')
            out.extend(lambdas)
        return '\n'.join(out) + '\n'

    def _callbacks_within(self, ci):
        return [k for k, cb in self.callbacks.items() if self._declared_within(cb.ci, ci)]

    @staticmethod
    def _cap_member(cap):
        if cap.kind == 'this':
            return _lower_camel(cap.ci.simple) if cap.name in ('this', 'this$0') else _lower_camel(cap.name.replace('this$', ''))
        return cap.name

    @staticmethod
    def _decl(cpp, name):
        if cpp.startswith('static ') and cpp.endswith('getInstance()'):
            return cpp + ';'
        if cpp == 'static const Logger log':
            return cpp + ';'
        return f'{cpp} {name};'

    def callback_struct(self, ci):
        cb = ci.storage
        base_ref = ci.new_expr.type if ci.new_expr is not None else (ci.td.extends[0] if ci.td.extends else None)
        base = 'TaskStruct'
        if ci.superclass is not None:
            base = ci.superclass.cpp_name
        elif ci.supers:
            base = ci.supers[0].cpp_name
        elif base_ref is not None:
            fqn = self.idx.resolve(base_ref.name, ci.lex_parent.td if ci.lex_parent else ci.td)
            if fqn in FUNCTIONAL or fqn == 'java.lang.Runnable':
                base = 'TaskStruct'
            else:
                base = base_ref.name
        where = f'{os.path.basename(ci.cu.path)}:{ci.td.line}'
        head = f'// {"anonymous " + (base_ref.name if base_ref else "") if ci.td.anonymous else "local class " + ci.td.name} at {where} ({ci.cid})'
        if cb is not None:
            head += f'; {cb.context}; storage: {cb.storage}' + (f' in {", ".join(o.simple for o in cb.owners)}' if cb.owners else '')
        lines = [head, f'struct {ci.cpp_name} final : {base} {{']
        for cap in ci.captures:
            lines.append(f'\t{self._decl(cap.cpp, self._cap_member(cap))} // captured {cap.kind} {cap.java} {cap.name} (line {cap.line})')
        for fi in ci.fields:
            if fi.cpp:
                lines.append(f'\t{self._decl(fi.cpp, fi.name)} // {fi.java} {fi.name} (line {fi.line}) [{fi.rule}]')
        if base != 'TaskStruct':
            params = ', '.join(f'{self._param(cap)} {self._cap_member(cap)}' for cap in ci.captures)
            lines.append(f'\tstatic Ref<{ci.cpp_name}> create({params});')
        lines.append('};')
        return '\n'.join(lines)

    def _param(self, cap):
        jt = cap.jt
        if jt is not None and jt.cat == 'class' and jt.ci is not None and jt.ci.kind in (K3, K4):
            return f'{self.cpp_class_name(jt.ci)}&'
        return (cap.cpp or 'auto').removeprefix('const ')


# ----------------------------------------------------------------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------------------------------------------------------------

def build(roots=None, support_roots=None, config_path=None, cycles_path=None, staticdata_path=None):
    roots = roots or DEFAULT_ROOTS
    support_roots = DEFAULT_SUPPORT_ROOTS if support_roots is None else support_roots
    for r in roots + support_roots:
        if not os.path.isdir(r):
            raise FieldmapError(f'source root not found: {r}')
    idx = javasrc.ProjectIndex.from_roots(list(roots) + list(support_roots))
    cfg = load_config(config_path)
    cycles = load_cycles(cycles_path)
    staticdata = None
    source = 'heuristic (JAXB annotations; generated/staticdata-classes.json not present)'
    if staticdata_path and os.path.exists(staticdata_path):
        staticdata = load_staticdata_classes(staticdata_path)
        source = _rel(staticdata_path)
    fm = FieldMap(idx, roots, cfg, cycles, staticdata, source)
    return fm.run()


def build_from_sources(files, config_text=None, cycles_text=None, staticdata=None):
    """Maps in-memory Java sources ({'com/x/A.java': text}) through a temporary root (tests and experiments). config_text and
    cycles_text are TOML documents; staticdata is {fqn: set(runtime-mutable field names)} or None for the K1 heuristic."""
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        root = os.path.join(tmp, 'src')
        for rel, text in files.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(text)
        cfg_path = cyc_path = None
        if config_text is not None:
            cfg_path = os.path.join(tmp, 'fieldmap.toml')
            with open(cfg_path, 'w', encoding='utf-8') as f:
                f.write(config_text)
        if cycles_text is not None:
            cyc_path = os.path.join(tmp, 'cycles.toml')
            with open(cyc_path, 'w', encoding='utf-8') as f:
                f.write(cycles_text)
        idx = javasrc.ProjectIndex.from_roots([root])
        fm = FieldMap(idx, [root], load_config(cfg_path), load_cycles(cyc_path), staticdata,
                      'staticdata-classes.json (test)' if staticdata is not None else 'heuristic')
        return fm.run()


def outputs(fm, config_path, cycles_path, roots):
    inputs = {'roots': [_rel(r) for r in roots], 'staticdataClasses': fm.staticdata_source,
              'fieldmapToml': _rel(config_path) if config_path and os.path.exists(config_path) else None,
              'cyclesToml': _rel(cycles_path) if cycles_path and os.path.exists(cycles_path) else None}
    return {'fieldmap.json': fm.fieldmap_json_text(inputs), 'parts.json': fm.parts_json_text(),
            'cycles_report.md': fm.cycles_report_text(), 'escape_report.md': fm.escape_report_text()}


def main(argv=None):
    ap = argparse.ArgumentParser(description='Concurrency field map of the Java game server (design runtime-architecture.md §3).')
    ap.add_argument('--roots', nargs='+', default=None, help='Java source roots whose classes are mapped')
    ap.add_argument('--support-roots', nargs='*', default=None, help='roots used only for resolution (default: commons/src)')
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--config', default=None, help='fieldmap.toml (default: <out>/fieldmap.toml)')
    ap.add_argument('--cycles', default=None, help='cycles.toml (default: <out>/cycles.toml)')
    ap.add_argument('--staticdata-classes', default=DEFAULT_STATICDATA)
    ap.add_argument('--class', dest='classes', action='append', default=[], help='print the member block of this Java FQN')
    ap.add_argument('--no-write', action='store_true')
    ap.add_argument('--check', action='store_true', help='exit 1 when the committed outputs differ')
    args = ap.parse_args(argv)
    roots = args.roots or DEFAULT_ROOTS
    config_path = args.config or os.path.join(args.out, 'fieldmap.toml')
    cycles_path = args.cycles or os.path.join(args.out, 'cycles.toml')
    try:
        fm = build(roots, args.support_roots, config_path, cycles_path, args.staticdata_classes)
        for cid in args.classes:
            sys.stdout.write(fm.member_block(cid))
        if args.classes and not args.check:
            return 0
        texts = outputs(fm, config_path, cycles_path, roots)
    except (FieldmapError, javasrc.JavaSyntaxError) as e:
        print(f'fieldmap: error: {e}', file=sys.stderr)
        return 2
    if args.check:
        diff = []
        for name, text in texts.items():
            p = os.path.join(args.out, name)
            old = None
            if os.path.exists(p):
                with open(p, encoding='utf-8') as f:  # universal newlines: a CRLF checkout is not a difference
                    old = f.read()
            if old != text:
                diff.append(name)
        if diff:
            print(f'fieldmap: outputs out of date: {", ".join(diff)}', file=sys.stderr)
            return 1
        return 0
    if not args.no_write:
        os.makedirs(args.out, exist_ok=True)
        for name, text in texts.items():
            with open(os.path.join(args.out, name), 'w', encoding='utf-8', newline='\n') as f:
                f.write(text)
    s = fm.summary()
    print(json.dumps(s, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
