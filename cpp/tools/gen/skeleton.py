"""skeleton: forward headers, header drafts and stub sources for the C++ game server port (T1, handlers-and-porting-plan.md §2.3, §2.5).

Python 3.12 stdlib only. Reads the Java sources through javasrc.py; never runs Java. Output is deterministic (sorted, '\\n' line ends, UTF-8).

Modes
-----
    python skeleton.py --fwd --out ROOT [--check]
        One `fwd.h` per Java package with top-level types, under the include root ROOT: ROOT/aion/gameserver/<package dirs>/fwd.h, in the
        namespace mirroring the package (keyword segments get a trailing underscore: `template_`; directories keep the Java spelling):
            class X;                        Java class/interface/record
            template <class T> class X;     Java generic class (parameter names from Java)
            enum class X : std::uint8_t;    Java enum: std::uint8_t for <= 256 constants, else std::uint16_t. Enum generators must use the same
                                            rule (a differing underlying type is a compile error)
        Existing C++ declarations win: definitions under --cpp-src and --generated-root (xmlgen's data structs and enums), else forward
        declarations there (HandlerRegistry.h `class AbstractAI;`; skeleton's own fwd.h files are not read). Their class key
        (`struct GSConfig;`), template head, enum base (`enum class X;` when the C++ enum has none) are copied, and a Java class ported as
        a C++ namespace (model::DialogAction, ServerPacketsOpcodes) or as a namespace-scope alias or using-declaration (`using X = ...;`,
        `using runtime::X;`) gets a comment instead of a declaration.
        The forward headers are committed under cpp/game-server/src (S0a, decision 5): `--fwd --out cpp/game-server/src` regenerates them,
        `--fwd --check --out cpp/game-server/src` is the drift check (CTest tools.gen, test_skeleton_tree). Rerun after adding a Java class,
        after xmlgen regenerates (enum bases and struct keys come from the generated tree) and after a hand-written header changes the key
        of a declared class. Java generics that C++ erases (docs/design/hub-headers.md §8.1: every type parameter has a project bound,
        NON_TEMPLATE_CLASSES, ERASURE_BOUNDS, or a non-template C++ declaration; TEMPLATE_GENERICS stay templates) are declared, drafted
        and referenced without template parameters; their type variables are spelled as the first bound (ERASURE_BOUNDS for unbounded ones),
        and type arguments after them in fieldmap member types are dropped. Nested Java types cannot be forward declared outside their outer class; they stay nested (Outer::Inner, as CONVENTIONS
        keeps SecurityConfig::MultiClientingRestrictionMode) and are listed in a comment, as are annotation types.

    python skeleton.py --draft --out ROOT [--fieldmap FILE | --no-fieldmap] [--with-dependencies] [--classes FILE] [SELECTOR ...] [--check]
        A header draft ROOT/aion/gameserver/<pkg>/<File>.h and a stub ROOT/.../<File>.cpp per selected Java file (all top-level types of
        the file, bases first):
        - classes: Java name, javadoc (with @author), `final`, project bases (`public Base<Args>`; interfaces inherited twice and external
          supertypes become comments), fieldmap base (runtime::RefCounted/OwnedPart/Immortal/StaticTemplate) unless an ancestor has it,
          nested types nested (forward declared first; a nested class deriving from its outer class is defined after the outer class),
          Java enums as `enum class` with a TODO for constructor data/methods, records with a canonical constructor and accessors;
        - methods under Java names (keyword rule: register_(), delete_()), Java access (package-private -> public; private members of
          nested types -> public, since Java lets the whole top-level class use them), `static`, `virtual` only if abstract, declared in an
          interface or overridden by a project subtype (core, data/handlers, anonymous, local and enum-constant classes), `override` when it
          overrides a project supertype method with the same erased parameter types (a supertype with a hand-written C++ definition under
          --cpp-src, e.g. an xmlgen behaviour shell, counts only if its header or member blocks declare the method; likewise the runtime
          base of such a class is the one its C++ base clause names, not the fieldmap base), `const` for trivial getters that are not virtual and
          for equals/hashCode/toString/compareTo; declarations keep the Java parameter names, definitions rename a parameter that would hide
          a data member (MSVC C4458) to `value`;
        - special signatures: equals(Object) -> `bool equals(const X&) const`, hashCode -> `int32_t hashCode() const`, toString ->
          `std::string toString() const`, compareTo(X) -> `int32_t compareTo(const X&) const`, JAXB afterUnmarshal(Unmarshaller, Object)
          -> `void afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent)`, getInstance() of a singleton -> `static X&`
          returning a function-local static (SingletonHolder is dropped);
        - members from fieldmap.json (below), spelled from the class scope (runtime::, project and commons types qualified, includes added).
          Members that cannot be used as they are become TODO lines with the reason: no entry (`TODO(fieldmap)` block), K1 members generated
          by xmlgen, instance loggers, unresolved or raw generic names, Ref<X> of a class without a RefCounted/OwnedPart base, Field<> of or
          by-value non-enum C++ classes, `static constexpr` without a literal Java initializer, function declarations, Java-syntax
          initializers. Default initializers: `{}`; `{*this}` for PartSlot/PartMap/PartList/SelfOrRef; Semaphore permits from the Java
          `new Semaphore(n)`; none for references (OwnerRef), which constructor stubs bind. Monitor, StampedLock, Semaphore, collection
          shim and Atomic* members start with their static lock class `AION_LOCK_CLASS(JavaClass::field)` (`#stripe` for
          ConcurrentHashMap/ConcurrentKeySet; RR-16, hub-headers.md §4). A member named like a method of the class (records)
          or of the runtime base (release, monitor) gets a trailing underscore. Generated callback structs are pasted as comments;
        - without fieldmap data, `static final` primitive/String fields with literal (or literal-only arithmetic) initializers become
          `static constexpr` / `static inline const std::string`; `LoggerFactory.getLogger(X.class)` becomes `static const auto log` in the
          .cpp (CONVENTIONS logging);
        - trivial accessors (`return f;`, `this.f = p;`) are inline when the member exists and the conversion is mechanical (scalars,
          strings, Field<Ref<X>> getters returning Ptr<X>), otherwise declared and stubbed;
        - every other body is `AION_UNPORTED();` in the .cpp (#include --unported-header); classes that are templates (Java generics) or
          nested in one get inline stub bodies. Constructor stubs call the base constructor with value-initialized arguments, or with the
          class-private `unportedArgument<T>()` helper (containers, reference members; no temporaries, C4702 suppressed around them).
          RefCounted classes get AION_MAKE_REF_FRIEND, protected constructors and destructor and an out-of-line `static Ref<X> create(...)`
          (skipped with a TODO when an override of an abstract method is itself a TODO signature, since the C++ class stays abstract);
        - Java signatures without a mechanical C++ mapping (Object, Class, Runnable, raw generics, wildcards other than `? extends`, generic
          methods, types nested in enums, covariant smart-pointer returns, colliding overloads, ...) stay `// TODO(signature): <reason>:
          <Java signature>` comments, so the draft compiles;
        - includes: <std> headers by use, runtime headers first, fwd.h of
          the packages used, full headers where a complete type is needed (bases, nested types of other files, by-value members, template
          base arguments, static members; by-value returns and member types in the .cpp).
        Type mapping (docs/design/hub-headers.md §5-§8 is the reference): primitives per CONVENTIONS; String -> std::string_view parameter /
        std::string otherwise; boxed -> std::optional<T> (plain T in containers); static data templates (fieldmap K1, or JAXB-annotated) and
        interned immortals (fieldmap base Immortal without getInstance) -> `const X*`; singletons -> `X&`; packets (K2) -> `X&` parameters and
        by-value returns; confined K5 -> `X&` parameters, by-value returns (`std::unique_ptr<X>` for abstract ones); AionConnection ->
        `AionConnection*` parameter / `std::shared_ptr` otherwise; enums by value; every other class (and type variables of templates):
        parameters `X&`, or `runtime::Ptr<X>` when nullable (Project.nullable_parameter: a direct null argument - a literal, a cast or a
        conditional branch, not a null nested in an inner call - at a call site of that name and arity, a null comparison in a body of the
        method family, a one-statement setter storing it; never for an owner stored as OwnerRef/Final<O*>),
        `std::unique_ptr<X>` when the constructor or setter stores it into a part (Project.part_parameter); returns `runtime::Ptr<X>`, `X&` for
        getters of parts and owners (and cast-only overrides of them), a reference to the shim for getters of collection fields;
        `runtime::Ref<X>` in containers of members; List/Collection/Queue/Deque -> std::vector, Set -> std::unordered_set, TreeSet ->
        std::set, Map -> std::unordered_map, TreeMap -> std::map (container parameters by const reference, object elements of parameters and
        returns `runtime::Ptr<X>`); arrays -> std::span<const T> parameters / std::vector<T>; varargs -> std::initializer_list<T>, declared
        with `= {}` unless an overload with one parameter less exists (`Object...` -> std::initializer_list<std::any>, packets ->
        std::initializer_list<std::reference_wrapper<P>>); Object -> const std::any& / std::any;
        byte[] -> std::span<const uint8_t> / std::vector<uint8_t>; Optional<X> -> Ptr<X> or std::optional<T> returns; Future ->
        runtime::FutureRef; Predicate/Consumer/Function/Supplier (+Bi) parameters -> const std::function<...>& with `X&` object arguments;
        `? extends X` / `? super X` -> X; Duration -> std::chrono::milliseconds; File -> std::filesystem::path;
        ByteBuffer/Connection/PreparedStatement/ResultSet -> commons references; Timestamp -> commons::database::Timestamp.
        Hub header rules applied to declarations: a Java override whose body is only `return (X) super.m(...)` does not make the base method
        virtual and becomes a non-virtual narrowing redeclaration with a ported cast (or nothing when the C++ types agree); overrides of methods
        with a type-variable parameter of an erased generic take the erased parameter type; constructors of VisibleObject and its subclasses
        take `CreateKey key` first and get no `create` (VisibleObject::create<T>; a VisibleObject draft itself declares CreateKey, create<T> and
        postConstruct); `toString()` is non-const on RefCounted/OwnedPart/Immortal classes; interfaces held by `Ref<I>`
        (Project.retainable_interfaces) declare pure virtual retain()/release(), forwarded by their first implementor with a runtime base.
        --with-dependencies also drafts, transitively, every Java file whose full header a draft needs, except files already ported under
        --cpp-src or --generated-root and generated replacements, so the set compiles on its own (an xmlgen behaviour class that is not
        scaffolded yet is drafted with a TODO(xmlgen) note).

    python skeleton.py --list SELECTOR ...
        Prints the Java FQNs a selector expands to (one per line).

    python skeleton.py --definitions
        Lists the member functions the hub and spine headers declare without a body that no `Class::name(` definition in the header or a
        .cpp under --cpp-src defines (S0B-120: every declared hub function must link). Class templates are skipped and overloads are not
        told apart. Exit 1 if there is one.

    python skeleton.py --guards [--freeze]
        Lists every `#if`/`#elif __has_include(...)` guard in cpp/game-server/{src,tests,handlers} (outside runtime/) with the headers it
        still waits for (docs/design/hub-headers.md §3.3). Exit 1 on an open guard (every header exists) unless it is an S0b transition guard
        naming only hub and spine headers (HUBS, SPINE_HEADERS); with --freeze (or SPINE_FROZEN) every guard is an error. MSBuild tracks only
        the headers a compile read, so a guarded .cpp is not rebuilt when its header appears: remove the guard in the same change.

Selectors: a Java FQN (a nested FQN selects its file), a unique simple class name, `pkg.*` (top-level types of one package), `pkg.**`
(recursive), or a group: @hubs (the S0b hub classes that are not ported yet), @services (services.** singletons and static-only classes),
@daos (dao.*), @serverpackets (network.aion.serverpackets.**), @engines (singletons named *Engine outside services), @all. Group and
package selectors (pkg.*, pkg.**) skip Java files whose header already exists under --cpp-src or --generated-root and files another
generator owns (reported on stderr): the static data classes of generated/staticdata-classes.json (xmlgen; behaviour classes are
scaffolded with xmlgen.py scaffold), the enums of generated/xmlmodel.json and Java files with a `<File>.gen.h` under --cpp-src. Explicit
names draft already ported files, but refuse xmlgen classes and enums and fully generated replacements (opcodes.py's ServerPacketsOpcodes).
A file with a generated member block (sysmsg.py's SM_SYSTEM_MESSAGE.gen.h, MEMBER_BLOCKS) is drafted with `#include "<File>.gen.h"` in a
public section and without the methods the block declares. Enums xmlgen generates (xmlmodel.json) are never defined by a draft: a
secondary top-level enum of the drafted file becomes a comment and an include of its generated header, a nested one
`using Inner = ::ns::Outer_Inner;` (xmlgen generates nested enums at namespace scope), and other files spell a nested one Outer_Inner
with its generated header.

Common options: --java-root (default game-server/src), --handlers-root (default game-server/data/handlers, override analysis only;
--no-handlers), --cpp-src (default cpp/game-server/src: existing definitions, namespaces and runtime symbols), --generated-root (default
cpp/game-server/generated: xmlgen headers and staticdata-classes.json; --no-generated-root), --commons-src (default cpp/commons/src),
--unported-header (default aion/gameserver/runtime/base/Unported.h, where the runtime defines AION_UNPORTED), --fieldmap (default
cpp/game-server/generated/concurrency/fieldmap.json when it exists; a missing default only warns).

--check compares with --out instead of writing and exits 1 listing `missing:`, `different:` and (--fwd) `stale:` generated files (exit 2
on errors). Writing only touches files whose content changed.

fieldmap.json (tools/gen/fieldmap.py) keys read
------------------------------------------------
    {"classes": {"<Java FQN; '$' or '.' for nested>": {             (or a list of objects with "fqn")
        "kind" | "kindName": "K1".."K5" | STATIC_DATA | PACKET | IMMUTABLE_VALUE | SHARED | CONFINED
        "base": "RefCounted" | "OwnedPart" | "Immortal" | "StaticTemplate" | "packet" | null     ("packet" adds no C++ base)
        "members" | "fields": [{                                   (or an object keyed by the Java field name)
            "javaName" | "name" | "field": "level",
            "cppType" | "type" | "cpp": "Field<int32_t>",          unqualified spellings are qualified from the class scope; null with a "rule"
                                                                   means "not a hand-written member" (xmlgen member blocks)
            "declaration": "...;",                                 alternative to the type: full declaration text
            "cppName", "initializer" (C++ text, emitted as {..}), "static" or "modifiers": ["static", ...], "access", "comment", "rule"
        }],
        "extraDeclarations" | "callbackStructs": ["struct ... };"],  generated callback structs (pasted as comments)
        "callbacks": ["<callback key>", ...],                       listed in a TODO when there are no extraDeclarations
        "includes": ["aion/gameserver/..."]
    }}}
Other keys are ignored. Malformed values (unknown kind or base, members without name or type, non-string lists) are errors.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

import javasrc  # noqa: E402

CPP_ROOT = _HERE.parents[1]
REPO_ROOT = _HERE.parents[2]
DEFAULT_JAVA_ROOT = REPO_ROOT / 'game-server' / 'src'
DEFAULT_HANDLERS_ROOT = REPO_ROOT / 'game-server' / 'data' / 'handlers'
DEFAULT_CPP_SRC = CPP_ROOT / 'game-server' / 'src'
DEFAULT_COMMONS_SRC = CPP_ROOT / 'commons' / 'src'
DEFAULT_GENERATED_ROOT = CPP_ROOT / 'game-server' / 'generated'
DEFAULT_FIELDMAP = DEFAULT_GENERATED_ROOT / 'concurrency' / 'fieldmap.json'
# AION_UNPORTED (P4-02b) moved from the handler registry library to the runtime base library aion_gs_runtime_base in S0a.
DEFAULT_UNPORTED_HEADER = 'aion/gameserver/runtime/base/Unported.h'
LOGGER_HEADER = 'aion/commons/logging/LoggerFactory.h'

JAVA_PREFIX = 'com.aionemu.gameserver'
CORE_NAMESPACE = ('aion', 'gameserver')
HANDLERS_NAMESPACE = ('aion', 'gameserver', 'handlers')
RUNTIME_NAMESPACE = ('aion', 'gameserver', 'runtime')

FWD_MARK = '// GENERATED by cpp/tools/gen/skeleton.py --fwd'
DRAFT_MARK = '// DRAFT generated by cpp/tools/gen/skeleton.py --draft'


class SkeletonError(Exception):
    """Unexpected input: the generator stops instead of guessing."""


# ----------------------------------------------------------------------------------------------------------------------------------
# Names
# ----------------------------------------------------------------------------------------------------------------------------------

CPP_KEYWORDS = frozenset('''alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class
compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double
dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq
nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert
static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile
wchar_t while xor xor_eq'''.split())
# Macros that WindowsMacroGuard.h cannot remove (Windows headers rely on them) and C library macros: renamed like keywords.
MACRO_NAMES = frozenset('''TRUE FALSE NULL CONST VOID STRICT CALLBACK WINAPI PASCAL EOF assert errno stdin stdout stderr offsetof setjmp
NAN INFINITY HUGE_VAL FLT_EPSILON DBL_EPSILON FLT_MAX DBL_MAX FLT_MIN DBL_MIN CHAR_BIT CHAR_MAX CHAR_MIN INT_MAX INT_MIN LONG_MAX LONG_MIN
SHRT_MAX SHRT_MIN UINT_MAX RAND_MAX EXIT_SUCCESS EXIT_FAILURE SIZE_MAX M_PI M_E'''.split())
_IDENT_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*\Z')


def cpp_ident(name):
    """Java identifier -> C++ identifier (keyword rule: trailing underscore)."""
    if not _IDENT_RE.match(name):
        raise SkeletonError(f'Java identifier {name!r} cannot be spelled in C++')
    if name in CPP_KEYWORDS or name in MACRO_NAMES:
        return name + '_'
    # '_X...' and 'a__b' names (SM_SYSTEM_MESSAGE._STR_..., STR_..._MODE__WHILE_...) are reserved in C++: the same rule as
    # dialogaction.cpp_identifier (used by sysmsg.py and dialogaction.py), so drafts and generated member blocks spell them alike
    if re.match(r'_[A-Z]', name) or '__' in name:
        return re.sub(r'_+', '_', name.lstrip('_')) + '_'
    return name


def package_namespace(package, root_kind):
    """Java package -> C++ namespace segments."""
    pkg = package or ''
    if root_kind == 'core':
        if pkg != JAVA_PREFIX and not pkg.startswith(JAVA_PREFIX + '.'):
            raise SkeletonError(f'package {pkg!r} is outside {JAVA_PREFIX}')
        rest = pkg[len(JAVA_PREFIX) + 1:].split('.') if pkg != JAVA_PREFIX else []
        return CORE_NAMESPACE + tuple(cpp_ident(s) for s in rest)
    if not pkg:
        raise SkeletonError('handler files must declare a package')
    return HANDLERS_NAMESPACE + tuple(cpp_ident(s) for s in pkg.split('.'))


def package_dir(package, root_kind):
    """Java package -> include directory (directories keep the Java segment spelling)."""
    pkg = package or ''
    if root_kind == 'core':
        rest = pkg[len(JAVA_PREFIX) + 1:].split('.') if pkg != JAVA_PREFIX else []
        return '/'.join(list(CORE_NAMESPACE) + rest)
    return '/'.join(list(HANDLERS_NAMESPACE) + pkg.split('.'))


def enum_underlying(td):
    return 'std::uint8_t' if len(td.enum_constants) <= 256 else 'std::uint16_t'


# ----------------------------------------------------------------------------------------------------------------------------------
# Existing C++ definitions
# ----------------------------------------------------------------------------------------------------------------------------------

@dataclass
class CppDecl:
    key: str                  # 'class' | 'struct' | 'enum'
    name: str
    path: str                 # include path relative to the scanned root
    template: str | None = None   # 'template <...>' text of a class template
    underlying: str | None = None  # enum underlying type ('' = none written)
    bases: str = ''           # base clause of a class definition head (`public runtime::RefCounted`), '' without bases or unknown


class CppTree:
    """Top-level definitions and forward declarations of existing C++ headers (column-0 declarations inside namespace blocks, project
    clang-format style), over one or more include roots (--cpp-src and the xmlgen tree cpp/game-server/generated). Definitions win over
    forward declarations; skeleton's own fwd.h files are not read (they would feed earlier output back)."""

    _NS_OPEN = re.compile(r'^namespace\s+([\w:]+)\s*\{')
    _NS_CLOSE = re.compile(r'^\}\s*//\s*namespace\b')
    _CLASS = re.compile(r'^(?:(template\s*<.*>)\s*)?(class|struct)\s+(\w+)(?:\s+final)?\s*(?::(?!:).*|\{.*)?$')
    _CLASS_FWD = re.compile(r'^(?:(template\s*<.*>)\s*)?(class|struct)\s+(\w+)\s*;$')
    _ENUM_FWD = re.compile(r'^enum\s+class\s+(\w+)\s*(?::\s*([\w:]+))?\s*;$')
    _ENUM = re.compile(r'^enum\s+class\s+(\w+)\s*(?::\s*([\w:]+))?\s*(?:\{.*)?$')
    _TEMPLATE = re.compile(r'^template\s*<.*>\s*$')
    _USING = re.compile(r'^using\s+(\w+)\s*=')
    _USING_DECL = re.compile(r'^using\s+(?!namespace\b)(?:typename\s+)?[\w:]*::(\w+)\s*;$')
    _DEFINE = re.compile(r'^#define\s+(\w+)')

    def __init__(self, root=None, extra_roots=()):
        self.root = Path(root) if root else None
        self.roots = [r for r in [self.root] + [Path(x) for x in extra_roots if x] if r is not None]
        self.decls = {}      # (ns tuple, name) -> CppDecl (definitions)
        self.forwards = {}   # (ns tuple, name) -> CppDecl (forward declarations)
        self.aliases = {}    # (ns tuple, name) -> include path of a namespace-scope alias or using-declaration (`using X = ...;`, `using a::X;`)
        self.symbols = {}    # runtime symbol -> include path
        self.namespaces = set()
        self.headers = set()  # include paths of every scanned header (all roots)
        for r in self.roots:
            if r.is_dir():
                for path in sorted(r.rglob('*.h'), key=lambda p: p.as_posix()):
                    rel = path.relative_to(r).as_posix()
                    if rel.startswith('concurrency/'):
                        continue
                    self.headers.add(rel)
                    self._scan(path, r)

    def has_header(self, include):
        """True if an include path exists under one of the roots."""
        return include in self.headers

    _STRIP = re.compile(r'"(?:[^"\\]|\\.)*"' + r"|'(?:[^'\\]|\\.)*'|//.*$")

    def _scan(self, path, root):
        rel = path.relative_to(root).as_posix()
        in_runtime = rel.startswith('aion/gameserver/runtime/')
        stack = []           # (namespace segments, brace depth inside the namespace block)
        depth = 0
        in_comment = False
        pending_template = None
        text = path.read_text(encoding='utf-8-sig')
        if text.startswith(FWD_MARK):
            return           # skeleton's own output (committed fwd.h files under --cpp-src): reading it back would keep outdated declarations
        for line in text.splitlines():
            code = line
            if in_comment:
                end = code.find('*/')
                if end < 0:
                    continue
                code = ' ' * (end + 2) + code[end + 2:]
                in_comment = False
            code = self._STRIP.sub(lambda m: ' ' * len(m.group(0)) if not m.group(0).startswith('//') else '', code)
            while '/*' in code:
                a = code.index('/*')
                b = code.find('*/', a + 2)
                if b < 0:
                    code = code[:a]
                    in_comment = True
                    break
                code = code[:a] + ' ' * (b + 2 - a) + code[b + 2:]
            ns = tuple(seg for segs, _ in stack for seg in segs)
            at_ns_level = bool(stack) and depth == stack[-1][1]
            m = self._NS_OPEN.match(code)
            if m and (not stack or at_ns_level):
                segs = tuple(m.group(1).split('::'))
                full = ns + segs
                for i in range(1, len(full) + 1):
                    self.namespaces.add(full[:i])
                depth += code.count('{') - code.count('}')
                stack.append((segs, depth))
                pending_template = None
                continue
            if in_runtime:
                d = self._DEFINE.match(line)
                if d:
                    self.symbols.setdefault(d.group(1), rel)
            if at_ns_level:
                stripped = code.rstrip()
                if self._TEMPLATE.match(code):
                    pending_template = stripped
                else:
                    cm = self._CLASS.match(stripped)
                    em = self._ENUM.match(stripped)
                    um = self._USING.match(stripped)
                    forward = stripped.endswith(';') and '{' not in stripped
                    if cm and not forward:
                        tail = stripped[cm.end(3):].lstrip()
                        tail = tail[len('final'):].lstrip() if re.match(r'final\b', tail) else tail
                        bases = tail[1:].split('{', 1)[0].strip() if tail.startswith(':') else ''
                        self._add(ns, CppDecl(cm.group(2), cm.group(3), rel, template=cm.group(1) or pending_template, bases=bases),
                                  in_runtime)
                    elif em and not forward:
                        self._add(ns, CppDecl('enum', em.group(1), rel, underlying=em.group(2) or ''), in_runtime)
                    elif um or self._USING_DECL.match(stripped):
                        name = um.group(1) if um else self._USING_DECL.match(stripped).group(1)
                        self.aliases.setdefault((ns, name), rel)
                        if um and in_runtime and ns == RUNTIME_NAMESPACE:
                            self.symbols.setdefault(um.group(1), rel)
                    elif forward:
                        fm = self._CLASS_FWD.match(stripped)
                        fe = self._ENUM_FWD.match(stripped)
                        if fm:
                            self.forwards.setdefault((ns, fm.group(3)), CppDecl(fm.group(2), fm.group(3), rel,
                                                                                template=fm.group(1) or pending_template))
                        elif fe:
                            self.forwards.setdefault((ns, fe.group(1)), CppDecl('enum', fe.group(1), rel, underlying=fe.group(2) or ''))
                    if stripped and not stripped.startswith('requires'):
                        pending_template = None
            depth += code.count('{') - code.count('}')
            while stack and depth < stack[-1][1]:
                stack.pop()

    def _add(self, ns, decl, in_runtime):
        self.decls.setdefault((ns, decl.name), decl)
        if in_runtime and ns == RUNTIME_NAMESPACE:
            self.symbols.setdefault(decl.name, decl.path)

    def lookup(self, ns, name):
        return self.decls.get((tuple(ns), name))

    def declaration(self, ns, name):
        """The definition, else the first forward declaration (path order) of a name."""
        key = (tuple(ns), name)
        return self.decls.get(key) or self.forwards.get(key)


RUNTIME_FALLBACK_SYMBOLS = {
    'PinnedCallback': 'aion/gameserver/runtime/sched/PinnedCallback.h', 'Pin': 'aion/gameserver/runtime/sched/Pin.h',
    'ArrayList': 'aion/gameserver/runtime/collections/ArrayList.h', 'LinkedList': 'aion/gameserver/runtime/collections/ArrayList.h',
    'HashMap': 'aion/gameserver/runtime/collections/HashMap.h', 'HashSet': 'aion/gameserver/runtime/collections/HashSet.h',
    'ConcurrentHashMap': 'aion/gameserver/runtime/collections/ConcurrentHashMap.h',
    'CopyOnWriteArrayList': 'aion/gameserver/runtime/collections/CopyOnWriteArrayList.h',
    'ArrayDeque': 'aion/gameserver/runtime/collections/ArrayDeque.h', 'PriorityQueue': 'aion/gameserver/runtime/collections/ArrayDeque.h',
    'ConcurrentLinkedQueue': 'aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h',
    'AtomicBoolean': 'aion/gameserver/runtime/fields/Atomic.h', 'AtomicInteger': 'aion/gameserver/runtime/fields/Atomic.h',
    'AtomicLong': 'aion/gameserver/runtime/fields/Atomic.h', 'AtomicReference': 'aion/gameserver/runtime/fields/Atomic.h',
    'Ref': 'aion/gameserver/runtime/lifetime/Ref.h', 'Ptr': 'aion/gameserver/runtime/lifetime/Ref.h',
    'makeRef': 'aion/gameserver/runtime/lifetime/Ref.h', 'RefCounted': 'aion/gameserver/runtime/lifetime/RefCounted.h',
    'Immortal': 'aion/gameserver/runtime/lifetime/RefCounted.h', 'StaticTemplate': 'aion/gameserver/runtime/lifetime/RefCounted.h',
    'AION_MAKE_REF_FRIEND': 'aion/gameserver/runtime/lifetime/RefCounted.h', 'OwnedPart': 'aion/gameserver/runtime/lifetime/Parts.h',
    'PartSlot': 'aion/gameserver/runtime/lifetime/Parts.h', 'PartMap': 'aion/gameserver/runtime/lifetime/Parts.h',
    'PartList': 'aion/gameserver/runtime/lifetime/Parts.h', 'OwnerRef': 'aion/gameserver/runtime/lifetime/Parts.h',
    'SelfOrRef': 'aion/gameserver/runtime/lifetime/Parts.h', 'Field': 'aion/gameserver/runtime/fields/Field.h',
    'Final': 'aion/gameserver/runtime/fields/Final.h', 'Array': 'aion/gameserver/runtime/fields/Array.h',
    'FutureRef': 'aion/gameserver/runtime/sched/Future.h', 'Future': 'aion/gameserver/runtime/sched/Future.h',
    'Monitor': 'aion/gameserver/runtime/sync/Monitor.h',
}


# ----------------------------------------------------------------------------------------------------------------------------------
# fieldmap.json
# ----------------------------------------------------------------------------------------------------------------------------------

KIND_ALIASES = {'K1': 'K1', 'STATIC_DATA': 'K1', 'K2': 'K2', 'PACKET': 'K2', 'K3': 'K3', 'IMMUTABLE_VALUE': 'K3', 'K4': 'K4',
                'SHARED': 'K4', 'K5': 'K5', 'CONFINED': 'K5'}
BASES = ('RefCounted', 'OwnedPart', 'Immortal', 'StaticTemplate', 'packet')
ACCESS = ('public', 'protected', 'private')


@dataclass
class MemberLayout:
    java_name: str
    cpp_name: str
    cpp_type: str | None             # type text as fieldmap.py prints it (may be unqualified: Field<Ref<MapRegion>>)
    declaration: str | None          # full declaration text (alternative to cpp_type)
    initializer: str | None
    static: bool
    access: str | None
    comment: str | None
    rule: str | None = None          # fieldmap.py rule name ("non-final scalar", "K1: member block generated by xmlgen", ...)


@dataclass
class ClassLayout:
    fqn: str
    kind: str | None
    base: str | None                 # C++ base to add (RefCounted/OwnedPart/Immortal/StaticTemplate); None for packets
    members: dict                    # java name -> MemberLayout (insertion order = fieldmap order)
    extra: list
    includes: list
    callbacks: list = field(default_factory=list)   # keys of generated callback structs (anonymous classes, lambdas)


def _first(d, keys, what, required=True):
    for k in keys:
        if k in d and d[k] is not None:
            return d[k]
    if required:
        raise SkeletonError(f'fieldmap: {what} has none of the keys {list(keys)}')
    return None


class Fieldmap:
    """Reader of generated/concurrency/fieldmap.json (interface in the module docstring)."""

    def __init__(self, classes=None, path=None):
        self.classes = classes or {}
        self.path = path

    @classmethod
    def load(cls, path):
        try:
            data = json.loads(Path(path).read_text(encoding='utf-8'))
        except (OSError, ValueError) as e:
            raise SkeletonError(f'fieldmap: cannot read {path}: {e}') from e
        return cls.from_json(data, str(path))

    @classmethod
    def from_json(cls, data, path='<fieldmap>'):
        if not isinstance(data, dict) or 'classes' not in data:
            raise SkeletonError(f'fieldmap: {path} has no top-level "classes"')
        raw = data['classes']
        if isinstance(raw, dict):
            items = list(raw.items())
        elif isinstance(raw, list):
            items = []
            for entry in raw:
                if not isinstance(entry, dict):
                    raise SkeletonError(f'fieldmap: {path}: class entries must be objects')
                items.append((_first(entry, ('fqn', 'class', 'javaClass'), 'class entry'), entry))
        else:
            raise SkeletonError(f'fieldmap: {path}: "classes" must be an object or a list')
        classes = {}
        for fqn, entry in items:
            if not isinstance(entry, dict):
                raise SkeletonError(f'fieldmap: {path}: entry {fqn} is not an object')
            fqn = fqn.replace('$', '.')
            kind = entry.get('kind', entry.get('kindName'))
            if kind is not None:
                if kind not in KIND_ALIASES:
                    raise SkeletonError(f'fieldmap: {fqn}: unknown kind {kind!r}')
                kind = KIND_ALIASES[kind]
            base = entry.get('base')
            if base is not None and base not in BASES:
                raise SkeletonError(f'fieldmap: {fqn}: unknown base {base!r}')
            if base == 'packet':
                base = None          # packets derive from their Java superclass only
            raw_members = entry.get('members', entry.get('fields', []))
            if isinstance(raw_members, dict):
                raw_members = [dict(v, javaName=k) if isinstance(v, dict) else v for k, v in raw_members.items()]
            if not isinstance(raw_members, list):
                raise SkeletonError(f'fieldmap: {fqn}: "members" must be a list or an object')
            members = {}
            for m in raw_members:
                if not isinstance(m, dict):
                    raise SkeletonError(f'fieldmap: {fqn}: member entries must be objects')
                name = _first(m, ('javaName', 'name', 'field'), f'member of {fqn}')
                decl = m.get('declaration')
                ctype = _first(m, ('cppType', 'type', 'cpp'), f'member {fqn}.{name}', required=False)
                rule = m.get('rule')
                if decl is None and ctype is None and rule is None:
                    raise SkeletonError(f'fieldmap: {fqn}.{name}: needs "cppType", "declaration" or a "rule" without a C++ type')
                for key, value in (('cppType', ctype), ('declaration', decl), ('rule', rule)):
                    if value is not None and not isinstance(value, str):
                        raise SkeletonError(f'fieldmap: {fqn}.{name}: "{key}" must be a string')
                access = m.get('access')
                if access is not None and access not in ACCESS:
                    raise SkeletonError(f'fieldmap: {fqn}.{name}: unknown access {access!r}')
                if name in members:
                    raise SkeletonError(f'fieldmap: {fqn}: member {name} listed twice')
                static = bool(m.get('static', False)) or 'static' in (m.get('modifiers') or [])
                members[name] = MemberLayout(name, m.get('cppName') or cpp_ident(name), ctype, decl, m.get('initializer'),
                                             static, access, m.get('comment'), rule)
            extra = entry.get('extraDeclarations', entry.get('callbackStructs', []))
            includes = entry.get('includes', [])
            if not isinstance(extra, list) or not all(isinstance(x, str) for x in extra):
                raise SkeletonError(f'fieldmap: {fqn}: "extraDeclarations" must be a list of strings')
            if not isinstance(includes, list) or not all(isinstance(x, str) for x in includes):
                raise SkeletonError(f'fieldmap: {fqn}: "includes" must be a list of strings')
            callbacks = entry.get('callbacks', [])
            if not isinstance(callbacks, list) or not all(isinstance(x, str) for x in callbacks):
                raise SkeletonError(f'fieldmap: {fqn}: "callbacks" must be a list of strings')
            classes[fqn] = ClassLayout(fqn, kind, base, members, extra, includes, callbacks)
        return cls(classes, path)

    def lookup(self, fqn):
        return self.classes.get(fqn)


# ----------------------------------------------------------------------------------------------------------------------------------
# Project model
# ----------------------------------------------------------------------------------------------------------------------------------

JAXB_ANNOTATIONS = frozenset(('XmlRootElement', 'XmlType', 'XmlAccessorType', 'XmlEnum', 'XmlSeeAlso'))
_PACKET_BASE_RE = re.compile(r'(?:Aion|Ls|Cs|Base)(?:Server|Client)Packet\Z')

# Java generic classes that the design maps to non-template C++ classes: forward declarations and drafts drop the type parameters,
# references drop the type arguments and the type variables are spelled as their first bound. An existing non-template C++ declaration
# (definition or forward declaration under --cpp-src) has the same effect.
#
# Erasure rule (docs/design/hub-headers.md "Generics"): a project generic class whose type parameters ALL have a bound naming a project
# type (`VisibleObjectController<T extends VisibleObject>`, `GeneralTeam<M extends AionObject, TM extends TeamMember<M>>`) is erased like
# javac erases it: one non-template C++ class, so wildcard and raw uses (`VisibleObjectController<? extends VisibleObject>`, `Siege<?>`)
# and the subclasses that bind the parameter (`NpcController extends CreatureController<Npc>`) share one C++ base. Subclasses narrow the
# accessors that return a type variable by redeclaring them. Unbounded generics (`SplitList<Type>`) and TEMPLATE_GENERICS stay templates.
NON_TEMPLATE_CLASSES = {
    JAVA_PREFIX + '.ai.AbstractAI': 'handlers-and-porting-plan.md amendments: AbstractAI derives runtime::OwnedPart, Creature holds '
                                    'PartSlot<AbstractAI>, AIFactory returns std::unique_ptr<ai::AbstractAI> (HandlerRegistry.h)',
}
# erased generics with an unbounded type variable: the C++ spelling of each variable (a Java FQN)
ERASURE_BOUNDS = {
    JAVA_PREFIX + '.model.team.TeamMember': {'M': JAVA_PREFIX + '.model.gameobjects.AionObject'},   # GeneralTeam<M extends AionObject, TM extends TeamMember<M>>
}
# bounded generics that stay C++ templates
TEMPLATE_GENERICS = {
    JAVA_PREFIX + '.ai.AITemplate': 'handlers-and-porting-plan.md amendments: the typed AI base AITemplate<T> declares using OwnerType = T',
    JAVA_PREFIX + '.ai.AIEngine.DummyAI': 'derives the template AITemplate<T>',
}
ERASURE_NOTE = 'erasure rule: every type parameter has a project bound'
# Java interfaces held by Ref<I> besides the ones fieldmap.json names (HandlerRegistry.h factories, erased GeneralTeam members)
RETAINABLE_INTERFACES = frozenset((
    JAVA_PREFIX + '.instance.handlers.InstanceHandler', JAVA_PREFIX + '.world.zone.handler.ZoneHandler', JAVA_PREFIX + '.model.team.TeamMember',
))

# Java files whose C++ side other generators write under --cpp-src: '<File>.gen.h' next to the header. A member block is #included by the
# hand-written class (sysmsg.py); any other generated header replaces the Java class completely (opcodes.py: namespace ServerPacketsOpcodes).
MEMBER_BLOCK_MARK = 'Member block of class '


def _sysmsg_generated(cu, m):
    """True if sysmsg.py generates the static factory m of SM_SYSTEM_MESSAGE (its classify(); hand-ported factories stay in the draft)."""
    import sysmsg
    if m.kind != 'method' or 'static' not in m.modifiers or m.body is None:
        return False
    f = sysmsg.Factory(m, [])
    try:
        sysmsg.classify(cu, f)
    except sysmsg.GenError:
        return False
    return f.kind in ('plain', 'generated')


# class name -> (predicate(cu, MethodDecl): the member block declares the method, std headers the block expects before it, contract note)
MEMBER_BLOCKS = {
    'SM_SYSTEM_MESSAGE': (_sysmsg_generated, ('cstdint', 'string', 'string_view', 'vector'),
                          'hand-written contract of sysmsg.py: SM_SYSTEM_MESSAGE(int32_t msgId, std::vector<std::string> params) and '
                          'static std::string toJavaString(int32_t / int64_t / int8_t / float)'),
}

HUBS = [
    'model.gameobjects.AionObject', 'model.gameobjects.VisibleObject', 'model.gameobjects.Creature', 'model.gameobjects.Npc',
    'model.gameobjects.Summon', 'model.gameobjects.player.Player', 'model.gameobjects.player.PlayerCommonData', 'model.gameobjects.Item',
    'model.gameobjects.Persistable', 'model.items.storage.Storage', 'model.items.storage.IStorage', 'model.team.TemporaryPlayerTeam',
    'model.templates.spawns.SpawnTemplate', 'model.templates.spawns.SpawnGroup', 'model.house.House',
    'world.World', 'world.WorldPosition', 'world.WorldMapInstance', 'world.MapRegion', 'world.knownlist.KnownList',
    'world.zone.ZoneInstance', 'world.zone.ZoneName',
    'controllers.VisibleObjectController', 'controllers.CreatureController', 'controllers.NpcController', 'controllers.PlayerController',
    'controllers.ObserveController', 'controllers.observer.ActionObserver', 'controllers.observer.ObserverType',
    'controllers.effect.EffectController', 'controllers.attack.AggroList', 'controllers.movement.CreatureMoveController',
    'model.stats.container.CreatureGameStats', 'model.stats.container.CreatureLifeStats', 'model.stats.container.StatEnum',
    'model.stats.calc.Stat2', 'skillengine.model.Skill', 'skillengine.model.Effect', 'skillengine.effect.EffectTemplate',
    'skillengine.model.SkillTemplate', 'skillengine.effect.AbnormalState', 'skillengine.SkillEngine',
    'questEngine.QuestEngine', 'questEngine.handlers.AbstractQuestHandler', 'questEngine.model.QuestEnv', 'questEngine.model.QuestState',
    'questEngine.model.QuestStatus', 'questEngine.handlers.HandlerResult', 'ai.AbstractAI', 'ai.AI', 'ai.AITemplate', 'ai.NpcAI',
    'ai.event.AIEventType', 'ai.AIState', 'ai.AISubState', 'instance.handlers.InstanceHandler', 'instance.handlers.GeneralInstanceHandler',
    'world.zone.handler.ZoneHandler', 'world.zone.handler.QuestZoneHandler', 'utils.chathandlers.ChatCommand',
    'utils.chathandlers.AdminCommand', 'utils.chathandlers.PlayerCommand', 'utils.chathandlers.ConsoleCommand',
    'network.aion.AionConnection', 'network.aion.AionServerPacket', 'network.aion.AionClientPacket', 'utils.PacketSendUtility',
    'dataholders.DataManager', 'spawnengine.SpawnEngine', 'world.geo.GeoService',
    # utils.ThreadPoolManager and utils.idfactory.IDFactory are hub classes too, but the runtime kernel already ports them.
]


# C++-only (or out-of-assignment) spine headers the hubs and HandlerRegistry.h depend on: frozen with the spine and compiled alone by
# aion_gs_header_check like the hubs (game-server/CMakeLists.txt gs_spine_headers, kept equal by test_skeleton_tree).
SPINE_HEADERS = [
    'aion/gameserver/model/Expirable.h', 'aion/gameserver/model/GameEngine.h', 'aion/gameserver/model/gameobjects/player/LogoutBreakers.h',
    'aion/gameserver/model/stats/calc/StatOwner.h', 'aion/gameserver/model/team/GeneralTeam.h', 'aion/gameserver/model/team/TeamMember.h',
    'aion/gameserver/model/templates/L10n.h', 'aion/gameserver/network/aion/SerializedBody.h', 'aion/gameserver/network/aion/StateSet.h',
    'aion/gameserver/world/zone/handler/GeneralZoneHandler.h',
]

# `__has_include` guards (docs/design/hub-headers.md §3.3). MSBuild records only the headers a compile read, so a guarded .cpp is not
# rebuilt when a header its guard waits for appears: a guard must be removed in the change that adds its last header (an open guard is an
# error of --guards), and no guard may be left at the spine freeze (--guards --freeze). The runtime kernel's feature checks are exempt.
GUARD_EXEMPT_PREFIXES = ('aion/gameserver/runtime/',)
_GUARD_DIRECTIVE_RE = re.compile(r'^[ \t]*#[ \t]*(?:if|elif)\b((?:[^\n]*\\\r?\n)*[^\n]*)', re.M)
_HAS_INCLUDE_RE = re.compile(r'__has_include\s*\(\s*[<"]([^">]+)[">]\s*\)')


@dataclass
class Guard:
    path: str                        # relative to its scan root, '/' separators
    line: int
    headers: list                    # every header the directive names, in order
    missing: list                    # the ones no include root has


def spine_guards(scan_roots, include_roots):
    """Every `#if`/`#elif` directive naming `__has_include` in the .h/.cpp files under scan_roots (outside GUARD_EXEMPT_PREFIXES), with
    the headers it names that none of include_roots has."""
    out = []
    includes = [Path(r) for r in include_roots]
    for root in scan_roots:
        root = Path(root)
        if not root.is_dir():
            continue
        for path in sorted(root.rglob('*')):
            if path.suffix not in ('.h', '.cpp') or not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            if rel.startswith(GUARD_EXEMPT_PREFIXES):
                continue
            text = path.read_text(encoding='utf-8-sig', errors='replace')
            if '__has_include' not in text:
                continue
            for m in _GUARD_DIRECTIVE_RE.finditer(text):
                headers = _HAS_INCLUDE_RE.findall(m.group(1))
                if not headers:
                    continue
                missing = [h for h in headers if not any((r / h).is_file() for r in includes)]
                out.append(Guard(f'{root.name}/{rel}', text.count('\n', 0, m.start()) + 1, headers, missing))
    return out


# Set by the integrator at the spine freeze (handlers-and-porting-plan.md §2.5, S0c): from then on no `__has_include` guard may remain.
SPINE_FROZEN = True


def spine_header_set():
    """Include paths of the hub headers (HUBS as top-level files, the kernel ports ThreadPoolManager and IDFactory) and SPINE_HEADERS."""
    out = {'aion/gameserver/' + rel.replace('.', '/') + '.h' for rel in HUBS}
    out |= {'aion/gameserver/utils/ThreadPoolManager.h', 'aion/gameserver/utils/idfactory/IDFactory.h'}
    return out | set(SPINE_HEADERS)


def guard_problems(guards, frozen=SPINE_FROZEN):
    """hub-headers.md §3.3 rules for `__has_include` guards: an open guard (every header exists) is an error, except an `S0b transition`
    guard that names only hub and spine headers before the freeze (the integrator removes those at the freeze); at the freeze every guard
    is an error."""
    spine = spine_header_set()
    problems = []
    for g in guards:
        where = f'{g.path}:{g.line}'
        if frozen:
            problems.append(f'{where}: `__has_include` guard left at the spine freeze (waits for: {", ".join(g.missing) or "nothing"})')
        elif not g.missing and not all(h in spine for h in g.headers):
            problems.append(f'{where}: open guard (every header it names exists): remove it in the change that adds the header, MSBuild '
                            f'does not rebuild the file when a header appears ({", ".join(g.headers)})')
    return problems


def _norm(p):
    return str(p).replace('\\', '/')


# ----------------------------------------------------------------------------------------------------------------------------------
# Declared member functions without a definition (S0B-120: every hub constructor, destructor and stub must link)
# ----------------------------------------------------------------------------------------------------------------------------------

_NOT_FUNCTION_NAMES = frozenset(('if', 'for', 'while', 'switch', 'return', 'sizeof', 'alignof', 'decltype', 'static_assert', 'noexcept',
                                 'requires', 'catch', 'throw', 'typeid'))


def _strip_cpp(text):
    """Comments, string and character literals and preprocessor lines blanked out (offsets and newlines kept)."""
    out = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
            continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
            continue
        if text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = re.match(r'R"([^(\s]*)\(', text[i:])
            if m:
                end = text.find(')' + m.group(1) + '"', i)
                j = n if end < 0 else end + len(m.group(1)) + 2
                out.append(re.sub(r'[^\n]', ' ', text[i:j]))
                i = j
                continue
        if c == '"' or (c == "'" and not (i > 0 and (text[i - 1].isalnum() or text[i - 1] == '_'))):
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(' ' * (j - i))
            i = j
            continue
        if c == '#' and line_start:
            j = i
            while True:
                e = text.find('\n', j)
                e = n if e < 0 else e
                if e > i and text[e - 1] == '\\' and e < n:
                    j = e + 1
                    continue
                break
            out.append(re.sub(r'[^\n]', ' ', text[i:e]))
            i = e
            continue
        if c == '\n':
            line_start = True
        elif not c.isspace():
            line_start = False
        out.append(c)
        i += 1
    return ''.join(out)


def _matching_brace(text, i):
    depth, j, n = 1, i + 1, len(text)
    while j < n and depth:
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
        j += 1
    return j


def declared_member_functions(header_text):
    """[(enclosing class names, name, line)] of the member functions a header declares without a body (`name(...) ...;` at class scope), outside
    class templates and without `= 0`, `= default` or `= delete`."""
    text = _strip_cpp(header_text)
    decls = []
    stack = []                  # ('class', name, templated) or ('scope', None, False)
    stmt_start = 0
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '{':
            head = ' '.join(text[stmt_start:i].split())
            head = re.sub(r'^(?:(?:public|protected|private)\s*:\s*)+', '', head)
            m = re.match(r'^(template\s*<.*>\s*)?(?:class|struct|union)\s+(?:\[\[[^\]]*\]\]\s*)?(\w+)(?:\s+final)?\s*(?::[^;{}]*)?$', head)
            if m:
                templated = bool(m.group(1)) or any(s[2] for s in stack)
                stack.append(('class', m.group(2), templated))
                i += 1
                stmt_start = i
                continue
            if re.match(r'^(?:inline\s+)?namespace\b[^;{}()]*$', head) or re.match(r'^extern\s+"', head):
                stack.append(('scope', None, False))
                i += 1
                stmt_start = i
                continue
            # a function body, an initializer (`x{...}`), an enum body or a lambda: skip it
            j = _matching_brace(text, i)
            body_of_function = bool(re.search(r'\)\s*(?:const\s*)?(?:&{1,2}\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?(?:override\s*|final\s*)*'
                                              r'(?:->[^{]*)?$', head)) or head.endswith(')')
            is_type = bool(re.match(r'^(?:enum|(?:typedef\s+)?(?:class|struct|union)\b)', head))
            i = j
            if body_of_function or is_type:
                # the declaration ends with its body (an enum/struct definition still needs its `;`, which then ends an empty statement)
                stmt_start = j
            continue
        if c == '}':
            if stack:
                stack.pop()
            i += 1
            stmt_start = i
            continue
        if c == ';':
            stmt = ' '.join(text[stmt_start:i].split())
            stmt_start = i + 1
            i += 1
            if not stack or stack[-1][0] != 'class' or stack[-1][2]:
                continue
            s = re.sub(r'^(?:(?:public|protected|private)\s*:\s*)+', '', stmt)
            s = re.sub(r'^(?:AION_MAKE_REF_FRIEND\s*)+', '', s)
            if not s or s.startswith(('using ', 'friend ', 'typedef ', 'template')) or 'static_assert' in s:
                continue
            if re.search(r'=\s*(?:0|default|delete)$', s):
                continue
            m = re.search(r'(~?\b\w+|\boperator\s*(?:\(\)|[^\s(]+))\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?:const\s*)?(?:&{1,2}\s*)?'
                          r'(?:noexcept(?:\s*\([^)]*\))?\s*)?(?:override\s*|final\s*)*$', s)
            if not m:
                continue
            name = m.group(1).replace(' ', '')
            prefix = s[:m.start()]
            if name in _NOT_FUNCTION_NAMES or '=' in prefix or '(' in prefix or re.fullmatch(r'[A-Z][A-Z0-9]*_[A-Z0-9_]*', name):  # macro calls
                continue
            decls.append(([s_[1] for s_ in stack if s_[0] == 'class'], name, header_text.count('\n', 0, i) + 1))
            continue
        i += 1
    return decls


def undefined_member_functions(headers, include_root, source_roots):
    """Member functions the given headers (include paths below include_root) declare without a body and that no `Class::name(` definition in
    the header itself or a .cpp under source_roots defines: [(header, 'Outer::Inner::name', line)]. Overloads are not told apart."""
    corpus = '\n'.join(_strip_cpp(path.read_text(encoding='utf-8-sig', errors='replace'))
                       for root in source_roots for path in sorted(Path(root).rglob('*.cpp')))
    missing = []
    for header in sorted(headers):
        path = Path(include_root) / header
        if not path.is_file():
            continue
        text = path.read_text(encoding='utf-8-sig', errors='replace')
        own = _strip_cpp(text)
        for classes, name, line in declared_member_functions(text):
            name_re = re.escape(name).replace(r'operator', r'operator\s*')
            pattern = re.compile(r'\b' + re.escape(classes[-1]) + r'\s*::\s*' + name_re + r'\s*\(')
            if not pattern.search(corpus) and not pattern.search(own):
                missing.append((header, '::'.join(classes + [name]), line))
    return missing


def is_singleton(td):
    return any(m.kind == 'method' and m.name == 'getInstance' and 'static' in m.modifiers and not m.params for m in td.methods)


def is_static_only(td):
    methods = [m for m in td.methods if m.kind == 'method']
    return bool(methods) and all('static' in m.modifiers for m in methods) and all('static' in f.modifiers for f in td.fields)


class Project:
    """The parsed Java trees plus the derived facts the emitters need."""

    def __init__(self, java_root=DEFAULT_JAVA_ROOT, handlers_root=DEFAULT_HANDLERS_ROOT, cpp_src=DEFAULT_CPP_SRC,
                 commons_src=DEFAULT_COMMONS_SRC, fieldmap=None, generated_root=None):
        self.java_root = _norm(java_root)
        self.handlers_root = _norm(handlers_root) if handlers_root else None
        if not Path(self.java_root).is_dir():
            raise SkeletonError(f'Java root {self.java_root} does not exist')
        roots = [self.java_root]
        if self.handlers_root:
            if not Path(self.handlers_root).is_dir():
                raise SkeletonError(f'handlers root {self.handlers_root} does not exist')
            roots.append(self.handlers_root)
        self.index = javasrc.ProjectIndex.from_roots(roots)
        if self.index.duplicates:
            fqn, a, b = self.index.duplicates[0]
            raise SkeletonError(f'duplicate Java type {fqn}: {a} and {b}')
        self.generated_root = Path(generated_root) if generated_root else None
        self.cpp = CppTree(cpp_src, [self.generated_root] if self.generated_root else [])
        self.xmlgen_classes = self._load_xmlgen_classes()
        self.xmlgen_enums = self._load_xmlgen_enums()
        self.commons_src = Path(commons_src) if commons_src else None
        self.fieldmap = fieldmap or Fieldmap()
        self.unit_kind = {}
        for cu in self.index.units:
            self.unit_kind[id(cu)] = 'core' if cu.root == self.java_root else 'handlers'
        self.core_units = [cu for cu in self.index.units if self.unit_kind[id(cu)] == 'core']
        self._ns_cache = {}
        self._erasure_cache = {}
        self._null_literals = None
        self._nullable_cache = {}
        self._part_param_cache = {}
        self._retainable = None
        self.known_paths = set()
        for cu in self.index.units:
            ns = self.unit_ns(cu)
            for i in range(1, len(ns) + 1):
                self.known_paths.add(ns[:i])
            for td in cu.all_types():
                self.known_paths.add(ns + self.type_path(td))
        for path, _ in self.xmlgen_enums.values():
            self.known_paths.add(path)
        for i in range(1, len(RUNTIME_NAMESPACE) + 1):
            self.known_paths.add(RUNTIME_NAMESPACE[:i])
        self.known_paths.add(CORE_NAMESPACE + ('xml',))
        self.known_paths.add(('aion', 'commons'))
        self.known_paths.update(self.cpp.namespaces)
        if self.commons_src is not None and self.commons_src.is_dir():
            for d in sorted(self.commons_src.rglob('*')):
                if d.is_dir():
                    self.known_paths.add(tuple(d.relative_to(self.commons_src).parts))
        self._subtype_methods = None
        self._subtypes = None
        self.skipped_existing = set()
        self._root_labels = {}
        self._cpp_names = None
        self._member_headers = {}
        self._todo_signatures = {}
        self._ported_declares = {}

    def _load_xmlgen_classes(self):
        """{fqn: kind} of generated/staticdata-classes.json (the K1 classes xmlgen generates or scaffolds), {} without the file."""
        if self.generated_root is None:
            return {}
        path = self.generated_root / 'staticdata-classes.json'
        if not path.is_file():
            return {}
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
            if data.get('format') != 'aion-staticdata-classes':
                raise ValueError('unexpected format')
            return {c['fqn']: c['kind'] for c in data['classes']}
        except (ValueError, KeyError, TypeError) as e:
            raise SkeletonError(f'{path}: {e}') from e

    def _load_xmlgen_enums(self):
        """{fqn: (C++ path tuple, header)} of the project enums xmlgen generates (generated/xmlmodel.json: JAXB and core enums, top-level,
        secondary top-level and nested ones, the latter at namespace scope as Outer_Inner), {} without the file."""
        if self.generated_root is None:
            return {}
        path = self.generated_root / 'xmlmodel.json'
        if not path.is_file():
            return {}
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
            if data.get('format') != 'aion-xmlmodel':
                raise ValueError('unexpected format')
            return {e['fqn']: (tuple(e['cpp']['qualifiedName'].lstrip(':').split('::')), e['cpp']['header'])
                    for e in data['enums'] if not e.get('external')}
        except (ValueError, KeyError, TypeError, AttributeError) as e:
            raise SkeletonError(f'{path}: {e}') from e

    def generated_enum(self, td):
        """(C++ path tuple, header) of a Java enum whose C++ enum xmlgen generates, else None."""
        if td.kind != 'enum' or not td.fqn:
            return None
        return self.xmlgen_enums.get(td.fqn)

    # -- generics
    def erased_generic(self, td):
        """True for a Java generic class ported as a non-template C++ class: NON_TEMPLATE_CLASSES, ERASURE_BOUNDS, an existing non-template
        C++ declaration, or the erasure rule (every type parameter has a project bound; TEMPLATE_GENERICS and existing C++ templates
        excepted)."""
        if not td.type_params:
            return False
        if td.fqn in NON_TEMPLATE_CLASSES or td.fqn in ERASURE_BOUNDS:
            return True
        if td.fqn in TEMPLATE_GENERICS:
            return False
        if td.outer is None:
            d = self.cpp.declaration(self.unit_ns(td.cu), cpp_ident(td.name))
            if d is not None and d.key in ('class', 'struct'):
                return not d.template
        cached = self._erasure_cache.get(id(td))
        if cached is None:
            cached = all(p.bounds and self.index.resolve_kind(p.bounds[0].name, td)[0] == 'project' for p in td.type_params)
            self._erasure_cache[id(td)] = cached
        return cached

    def erasure_note(self, td):
        return NON_TEMPLATE_CLASSES.get(td.fqn) or (ERASURE_NOTE if td.fqn in ERASURE_BOUNDS or td.fqn not in TEMPLATE_GENERICS and not (
            td.outer is None and self.cpp.declaration(self.unit_ns(td.cu), cpp_ident(td.name)) is not None) else 'existing C++ declaration')

    def cpp_type_params(self, td):
        """The type parameters of the C++ class (none for erased generics)."""
        return [] if self.erased_generic(td) else td.type_params

    # -- generator-owned files
    def generator_owner(self, td):
        """None, or (generator, how) for a top-level type whose C++ side another generator owns: ('xmlgen', 'enum') for the enums of
        xmlmodel.json (also secondary top-level types of another Java file), ('xmlgen', kind) for the static data classes of
        staticdata-classes.json, ('member block', gen.h path) for a sysmsg-style member block, ('generated', gen.h path) for a generated
        header replacing the class (opcodes.py). Nested generated enums do not make their outer class generator-owned (generated_enum)."""
        top = self.top_level(td)
        if self.generated_enum(top) is not None:
            return 'xmlgen', 'enum'
        kind = self.xmlgen_classes.get(top.fqn)
        if kind is not None:
            return 'xmlgen', kind
        gen = self.unit_header(top.cu)[:-2] + '.gen.h'
        root = self.cpp.root
        if root is not None and (root / gen).is_file():
            head = (root / gen).read_text(encoding='utf-8-sig')[:2000]
            if MEMBER_BLOCK_MARK + top.name + ':' in head:
                return 'member block', gen
            return 'generated', gen
        return None

    # -- names and paths
    def unit_ns(self, cu):
        key = id(cu)
        ns = self._ns_cache.get(key)
        if ns is None:
            ns = package_namespace(cu.package, self.unit_kind[key])
            self._ns_cache[key] = ns
        return ns

    def unit_dir(self, cu):
        return package_dir(cu.package, self.unit_kind[id(cu)])

    def unit_header(self, cu):
        stem = cu.relpath.rsplit('/', 1)[-1][:-len('.java')]
        return f'{self.unit_dir(cu)}/{stem}.h'

    @staticmethod
    def type_path(td):
        names = []
        t = td
        while t is not None:
            names.append(cpp_ident(t.name))
            t = t.outer
        return tuple(reversed(names))

    @staticmethod
    def top_level(td):
        while td.outer is not None:
            td = td.outer
        return td

    def source_label(self, cu):
        base = self._root_labels.get(cu.root)
        if base is None:
            root = Path(cu.root)
            try:
                base = root.resolve().relative_to(REPO_ROOT.resolve()).as_posix()
            except ValueError:
                base = root.name
            self._root_labels[cu.root] = base
        return f'{base}/{cu.relpath}'

    # -- classification
    def layout(self, td):
        return self.fieldmap.lookup(td.fqn) if td.fqn else None

    def all_supertypes(self, td):
        out, seen, todo = [], {id(td)}, list(self.index.supertypes(td))
        while todo:
            s = todo.pop(0)
            if id(s) in seen:
                continue
            seen.add(id(s))
            out.append(s)
            todo.extend(self.index.supertypes(s))
        return out

    def superclass(self, td):
        """The project superclass TypeDecl of a class (None for interfaces, external or no superclass)."""
        if td.kind != 'class' or not td.extends:
            return None
        kind, value = self.index.resolve_kind(td.extends[0].name, td)
        if kind == 'project':
            sup = self.index.types[value]
            return sup if sup.kind == 'class' else None
        return None

    def kind(self, td):
        """K1..K5 from fieldmap.json, else K1 for JAXB-annotated classes, K2 for packets, None (treated as K4)."""
        lay = self.layout(td)
        if lay is not None and lay.kind:
            return lay.kind
        chain = [td] + self.all_supertypes(td)
        if any(a.simple_name in JAXB_ANNOTATIONS for t in chain for a in t.annotations):
            return 'K1'
        if any(_PACKET_BASE_RE.search(t.name or '') for t in chain):
            return 'K2'
        return None

    def base_kind(self, td):
        """'RefCounted' etc. when the class or a project ancestor has a fieldmap base. An ancestor that already has a hand-written C++
        definition (a header under --cpp-src, e.g. an xmlgen behaviour shell) counts with the runtime base its definition names: none if
        it has no base clause, the fieldmap base of its Java superclasses if it only names other bases. The superclass chain decides first:
        an implemented interface without a base clause (Persistable) says nothing about the class's base (House -> VisibleObject ->
        AionObject); an interface that names a runtime base counts only when the chain has none."""
        chain = [td]
        while True:
            sup = self.superclass(chain[-1])
            if sup is None or any(sup is c for c in chain):
                break
            chain.append(sup)
        for t in chain:
            ported = self.ported_definition(t)
            if ported is not None:
                named = [b for b in BASES if b != 'packet' and re.search(r'\b' + b + r'\b', ported.bases)]
                if named:
                    return named[0]
                if not ported.bases:
                    return None
                continue
            lay = self.layout(t)
            if lay is not None and lay.base:
                return lay.base
        for t in self.all_supertypes(td):
            if any(t is c for c in chain):
                continue
            ported = self.ported_definition(t)
            if ported is not None:
                named = [b for b in BASES if b != 'packet' and re.search(r'\b' + b + r'\b', ported.bases)]
                if named:
                    return named[0]
                continue
            lay = self.layout(t)
            if lay is not None and lay.base:
                return lay.base
        return None

    def ported_definition(self, td):
        """The C++ class definition of a top-level Java type whose header exists under --cpp-src (hand-written), else None."""
        if td.outer is not None or self.cpp.root is None:
            return None
        d = self.cpp.lookup(self.unit_ns(td.cu), cpp_ident(td.name))
        if d is None or d.key not in ('class', 'struct') or d.path != self.unit_header(td.cu) or not (self.cpp.root / d.path).is_file():
            return None
        return d

    def ported_declares(self, td, name):
        """False if td has a hand-written C++ definition (ported_definition) whose header, and the member blocks it includes, do not
        mention a function `name(`: the Java method is not declared in C++ (yet), so drafts must not mark an `override` of it."""
        d = self.ported_definition(td)
        if d is None:
            return True
        key = (d.path, name)
        if key not in self._ported_declares:
            texts = []
            for rel in [d.path] + _INCLUDE_RE.findall((self.cpp.root / d.path).read_text(encoding='utf-8-sig')):
                if rel == d.path or rel.endswith(('.inc', '.gen.h')) or rel == d.path[:-2] + '.xml.h':
                    path = next((r / rel for r in self.cpp.roots if (r / rel).is_file()), None)
                    if path is not None:
                        texts.append(path.read_text(encoding='utf-8-sig'))
            self._ported_declares[key] = re.search(r'\b' + re.escape(cpp_ident(name)) + r'\s*\(', '\n'.join(texts)) is not None
        return self._ported_declares[key]

    # -- override analysis
    def _build_subtypes(self):
        direct = {}
        declared = {}

        def add(td):
            key = id(td)
            # cast-only overrides (hub-headers.md §8.2) are narrowing redeclarations in C++: they do not make the base method virtual
            declared[key] = {(m.name, len(m.params)) for m in td.methods if m.kind == 'method' and 'static' not in m.modifiers
                             and not self.is_cast_override(m)}
            for sup in self.index.supertypes(td):
                direct.setdefault(id(sup), []).append(td)

        for cu in self.index.units:
            for td in cu.all_types():
                add(td)
            for top in cu.types:
                if top.body is None:
                    continue
                for ne in top.body.anonymous_classes():
                    add(ne.anonymous)
                for lt in top.body.local_types():
                    add(lt)
            for t in cu.all_types():
                for c in t.enum_constants:
                    if c.body is not None:
                        add(c.body)
        self._subtypes = direct
        self._declared = declared
        self._subtype_methods = {}

    def subtype_methods(self, td):
        if self._subtypes is None:
            self._build_subtypes()
        key = id(td)
        cached = self._subtype_methods.get(key)
        if cached is not None:
            return cached
        out, seen, todo = set(), {key}, list(self._subtypes.get(key, []))
        while todo:
            s = todo.pop()
            if id(s) in seen:
                continue
            seen.add(id(s))
            out |= self._declared.get(id(s), set())
            todo.extend(self._subtypes.get(id(s), []))
        self._subtype_methods[key] = out
        return out

    def overrides(self, td, m):
        return self.overridden_method(td, m) is not None

    def overridden_method(self, td, m, hand_written=True):
        """The nearest project supertype method (same name and arity) that m overrides, or None (hand_written=False: in Java, ignoring
        hand-written C++ definitions that do not declare the method)."""
        sig = (m.name, len(m.params))
        mine = self._param_sig(m)
        # breadth-first like all_supertypes, but a supertype with a hand-written C++ definition that does not declare the method hides it
        # and everything above it (its C++ bases need not be the Java ones, e.g. an xmlgen shell without the Java interfaces)
        seen, todo, chain = {id(td)}, list(self.index.supertypes(td)), []
        while todo:
            s = todo.pop(0)
            if id(s) in seen:
                continue
            seen.add(id(s))
            if not hand_written or self.ported_declares(s, m.name):
                chain.append(s)
                todo.extend(self.index.supertypes(s))
        for sup in chain:
            for sm in sup.methods:
                if sm.kind == 'method' and 'static' not in sm.modifiers and 'private' not in sm.modifiers and (sm.name, len(sm.params)) == sig:
                    theirs = self._param_sig(sm)
                    if all(a[1] == b[1] and (a[0] == '*' or b[0] == '*' or a[0] == b[0]) for a, b in zip(mine, theirs)):
                        return sm
        return None

    # -- cast-only overrides and parameter nullability (docs/design/hub-headers.md §8.2, §5.1)
    @staticmethod
    def is_cast_override(m):
        """True for a Java override whose body is only `return (X) super.m(params...);` (a covariant narrowing: the C++ subclass redeclares
        the accessor non-virtually, and it does not make the base method virtual)."""
        if m.kind != 'method' or m.body is None or 'static' in m.modifiers:
            return False
        t = m.body.texts()[1:-1]
        if len(t) < 9 or t[0] != 'return' or t[1] != '(' or t[-1] != ';':
            return False
        try:
            close = t.index(')', 2)
        except ValueError:
            return False
        rest = t[close + 1:]
        expected = ['super', '.', m.name, '(']
        for i, p in enumerate(m.params):
            if i:
                expected.append(',')
            expected.append(p.name)
        expected += [')', ';']
        return close > 2 and rest == expected

    @staticmethod
    def _null_argument(texts):
        """hub-headers.md §5.1 rule 1: the argument itself is a null literal, a cast of one (`(X) null`), a parenthesized one, or a
        conditional expression with such a branch (nested conditionals included). A null inside a nested call, instantiation or lambda
        of the argument (`add(new Entry(a ? b : null))`) is an argument of that inner expression, not of this call."""
        t = list(texts)
        while len(t) >= 2 and t[0] == '(' and Project._closing_paren(t, 0) == len(t) - 1:
            t = t[1:-1]
        if t == ['null']:
            return True
        if len(t) >= 4 and t[0] == '(':
            close = Project._closing_paren(t, 0)
            if close is not None and t[close + 1:] == ['null']:
                return True
        depth, question = 0, None
        for i, tok in enumerate(t):
            if tok in ('(', '[', '{'):
                depth += 1
            elif tok in (')', ']', '}'):
                depth -= 1
            elif depth == 0 and tok == '->':
                return False                    # a lambda: its null result is not an argument
            elif depth == 0 and tok == '?' and question is None and i > 0 and t[i - 1] != '<':
                question = i
        if question is None:
            return False
        depth, nested = 0, 0
        for i in range(question + 1, len(t)):
            tok = t[i]
            if tok in ('(', '[', '{'):
                depth += 1
            elif tok in (')', ']', '}'):
                depth -= 1
            elif depth == 0 and tok == '?':
                nested += 1
            elif depth == 0 and tok == ':':
                if nested:
                    nested -= 1
                    continue
                return Project._null_argument(t[question + 1:i]) or Project._null_argument(t[i + 1:])
        return False

    @staticmethod
    def _closing_paren(t, start):
        depth = 0
        for i in range(start, len(t)):
            if t[i] == '(':
                depth += 1
            elif t[i] == ')':
                depth -= 1
                if depth == 0:
                    return i
        return None

    def _build_null_literals(self):
        """{(callee simple name, arity, argument index)} of every call or instantiation passing a null literal (constructors by class name;
        super(...)/this(...) by the called constructor's class)."""
        literal = set()
        for cu in self.index.units:
            for top in cu.types:
                if top.body is None:
                    continue
                for call in top.body.method_calls():
                    if call.name in ('super', 'this'):
                        continue
                    for i, a in enumerate(call.args):
                        if self._null_argument(a.texts()):
                            literal.add((call.name, len(call.args), i))
                for ne in top.body.new_expressions():
                    if ne.args is None or ne.array:
                        continue
                    simple = ne.type.name.rsplit('.', 1)[-1]
                    for i, a in enumerate(ne.args):
                        if self._null_argument(a.texts()):
                            literal.add((simple, len(ne.args), i))
            for td in cu.all_types():
                for m in td.methods:
                    if m.kind != 'constructor' or m.body is None:
                        continue
                    for call in m.body.method_calls():
                        if call.name not in ('super', 'this'):
                            continue
                        target = td if call.name == 'this' else self.superclass(td)
                        if target is None:
                            continue
                        for i, a in enumerate(call.args):
                            if self._null_argument(a.texts()):
                                literal.add((target.name, len(call.args), i))
        self._null_literals = literal

    def _descendants(self, td):
        if self._subtypes is None:
            self._build_subtypes()
        out, seen, todo = [], {id(td)}, list(self._subtypes.get(id(td), []))
        while todo:
            s = todo.pop()
            if id(s) in seen:
                continue
            seen.add(id(s))
            out.append(s)
            todo.extend(self._subtypes.get(id(s), []))
        return out

    def method_family(self, td, m):
        """Every method that must share m's C++ signature: the same name and arity in the topmost supertypes declaring it and in all their
        subtypes (core, handlers, anonymous and local classes). Constructors: the constructors of td with the same arity."""
        if m.kind == 'constructor':
            return [c for c in td.methods if c.kind == 'constructor' and len(c.params) == len(m.params)]
        sig = (m.name, len(m.params))

        def declared(t):
            return [x for x in t.methods if x.kind == 'method' and 'static' not in x.modifiers and (x.name, len(x.params)) == sig]

        tops = [t for t in [td] + self.all_supertypes(td) if declared(t)] or [td]
        family, seen = [], set()
        for top in tops:
            for t in [top] + self._descendants(top):
                if id(t) in seen:
                    continue
                seen.add(id(t))
                family += declared(t)
        return family or [m]

    def nullable_parameter(self, td, m, index):
        """hub-headers.md §5.1: True if an object parameter must be a nullable Ptr<X>: a call site passes a null literal (by method name and
        arity), a body of the method family compares the parameter with null, or a one-statement setter of the family stores it into a field
        that is not an owner reference. False means X&."""
        key = (id(td), id(m), index)
        cached = self._nullable_cache.get(key)
        if cached is not None:
            return cached
        if self._null_literals is None:
            self._build_null_literals()
        name = td.name if m.kind == 'constructor' else m.name
        for fm in self.method_family(td, m):
            # the owner of a part (OwnerRef, late-bound Final<O*>) is never null, whatever other methods of that name receive
            lay = self.layout(fm.owner) if fm.owner is not None and fm.body is not None and index < len(fm.params) else None
            if lay is None:
                continue
            for asg in fm.body.field_assignments():
                member = lay.members.get(asg.name) if asg.op == '=' and asg.rhs is not None and asg.rhs.texts() == [fm.params[index].name] else None
                if member is not None and re.search(r'\bOwnerRef<|\bFinal<[^<>]*\*>', member.cpp_type or member.declaration or ''):
                    self._nullable_cache[key] = False
                    return False
        result = (name, len(m.params), index) in self._null_literals
        if not result:
            for fm in self.method_family(td, m):
                if fm.body is None or index >= len(fm.params):
                    continue
                p = fm.params[index].name
                t = fm.body.texts()
                for i in range(len(t) - 2):
                    a, op, b = t[i], t[i + 1], t[i + 2]
                    if op in ('==', '!=') and ((a == p and b == 'null' and (i == 0 or t[i - 1] != '.')) or (a == 'null' and b == p)):
                        result = True
                        break
                    if a in ('isNull', 'nonNull') and op == '(' and b == p:
                        result = True
                        break
                if result:
                    break
                owner = fm.owner
                lay = self.layout(owner) if owner is not None else None
                setter = fm.kind == 'method' and len(fm.params) == 1 and fm.body.texts()[-2:] == [';', '}'] and fm.body.texts()[1:-1].count(';') == 1
                for asg in (fm.body.field_assignments() if setter else []):
                    if asg.op != '=' or asg.element or asg.rhs is None or asg.rhs.texts() != [p]:
                        continue
                    member = lay.members.get(asg.name) if lay is not None else None
                    mtype = (member.cpp_type or member.declaration or '') if member is not None else ''
                    if re.search(r'\bOwnerRef<|\bFinal<[^<>]*\*>', mtype):
                        continue
                    result = True
                    break
                if result:
                    break
        self._nullable_cache[key] = result
        return result

    def part_parameter(self, td, m, index):
        """True if a constructor or setter parameter becomes a part of td (stored into a const std::unique_ptr / PartSlot member, directly or
        through super(...)/this(...)): the parameter is std::unique_ptr<X> (hub-headers.md §10.2)."""
        key = (id(td), id(m), index)
        cached = self._part_param_cache.get(key)
        if cached is not None:
            return cached
        self._part_param_cache[key] = False
        result = False
        if m.body is not None and index < len(m.params):
            p = m.params[index].name
            lay = self.layout(td)
            for asg in m.body.field_assignments():
                if asg.op == '=' and not asg.element and asg.rhs is not None and asg.rhs.texts() == [p] and lay is not None:
                    member = lay.members.get(asg.name)
                    if member is not None and re.search(r'\b(unique_ptr|PartSlot)<', member.cpp_type or member.declaration or ''):
                        result = True
                        break
            if not result and m.kind == 'constructor':
                for call in m.body.method_calls():
                    if call.name not in ('super', 'this'):
                        continue
                    target = td if call.name == 'this' else self.superclass(td)
                    if target is None:
                        continue
                    for j, a in enumerate(call.args):
                        if a.texts() == [p] and any(c.kind == 'constructor' and len(c.params) == len(call.args) and self.part_parameter(target, c, j)
                                                    for c in target.methods):
                            result = True
        self._part_param_cache[key] = result
        return result

    def retainable_interfaces(self):
        """FQNs of Java interfaces held by Ref<I> (a fieldmap member type names `Ref<I>`, HandlerRegistry.h's Ref<InstanceHandler> and
        Ref<ZoneHandler>, erased team members): they declare pure virtual retain()/release(), which implementors forward to their runtime base
        (hub-headers.md §9.2)."""
        if self._retainable is None:
            names = set()
            for lay in self.fieldmap.classes.values():
                for ml in lay.members.values():
                    names.update(re.findall(r'\bRef<(\w+)>', ml.cpp_type or ml.declaration or ''))
            out = set(RETAINABLE_INTERFACES)
            for fqn, td in self.index.types.items():
                if td.kind == 'interface' and td.name in names and self.unit_kind[id(td.cu)] == 'core':
                    out.add(fqn)
            self._retainable = out
        return self._retainable

    def retainable_implemented(self, td):
        """The retainable interfaces td implements itself that no superclass implements already (td forwards retain/release)."""
        if td.kind == 'interface' or self.base_kind(td) not in ('RefCounted', 'OwnedPart'):
            return []
        mine = {t.fqn for t in self.all_supertypes(td) if t.kind == 'interface' and t.fqn in self.retainable_interfaces()}
        sup = self.superclass(td)
        if sup is not None and self.base_kind(sup) in ('RefCounted', 'OwnedPart'):
            mine -= {t.fqn for t in self.all_supertypes(sup) if t.kind == 'interface'}
        return sorted(mine)

    def is_visible_object(self, td):
        """True for VisibleObject and its subclasses (constructed by VisibleObject::create<T> with the CreateKey passkey, hub-headers.md §10.1)."""
        return any(t.fqn == VISIBLE_OBJECT_FQN for t in [td] + self.all_supertypes(td))

    def method_is_virtual(self, owner, m):
        """The C++ virtual-ness drafts give m in its own class: abstract, declared by an interface, or overridden by a non-cast override."""
        if 'static' in m.modifiers or 'private' in m.modifiers or m.kind != 'method':
            return False
        if 'abstract' in m.modifiers or owner.kind == 'interface':
            return True
        if self.overridden_method(owner, m, hand_written=False) is not None:
            return self.method_is_virtual(self.overridden_method(owner, m, hand_written=False).owner,
                                          self.overridden_method(owner, m, hand_written=False))
        return self.overridden_in_subtypes(owner, m) and 'final' not in m.modifiers

    def overridden_in_subtypes(self, td, m):
        """True if a project subtype of td (core, handlers, anonymous, local and enum-constant classes) overrides m: a non-static, non-private
        method with the same name, arity and compatible erased parameter types that is not a cast-only override (hub-headers.md §8.2). A
        private same-named helper with other parameter types (NightmareCircus.sendMsg(int, int) against
        GeneralInstanceHandler.sendMsg(SM_SYSTEM_MESSAGE, int)) is not an override."""
        sig = (m.name, len(m.params))
        if sig not in self.subtype_methods(td):
            return False
        key = ('overridden', id(td), id(m))
        cached = self._nullable_cache.get(key)
        if cached is not None:
            return cached
        mine = self._param_sig(m)
        result = False
        for s in self._descendants(td):
            for sm in s.methods:
                if (sm.kind != 'method' or 'static' in sm.modifiers or 'private' in sm.modifiers or (sm.name, len(sm.params)) != sig
                        or self.is_cast_override(sm)):
                    continue
                theirs = self._param_sig(sm)
                if all(a[1] == b[1] and (a[0] == '*' or b[0] == '*' or a[0] == b[0]) for a, b in zip(mine, theirs)):
                    result = True
                    break
            if result:
                break
        self._nullable_cache[key] = result
        return result

    def _param_sig(self, m):
        """Java erasure-like parameter signature: [(resolved type or '*' for type variables, dims)]."""
        out = []
        for prm in m.params:
            kind, value = self.index.resolve_kind(prm.type.name, m)
            dims = prm.type.dims + (1 if prm.varargs else 0)
            out.append(('*' if kind == 'typevar' else (value if isinstance(value, str) else prm.type.name), dims))
        return out

    # -- selection
    def core_type(self, fqn):
        td = self.index.types.get(fqn)
        if td is None or self.unit_kind[id(td.cu)] != 'core':
            return None
        return td

    def select(self, selectors):
        """Selectors -> sorted list of top-level core TypeDecls (see module docstring)."""
        out = {}
        tops = [td for cu in self.core_units for td in cu.types]
        for sel in selectors:
            sel = sel.strip()
            if not sel:
                continue
            found = []
            if sel.startswith('@'):
                found = self._group(sel, tops)
            elif sel.endswith('.**'):
                pkg = sel[:-3]
                found = [td for td in tops if td.cu.package == pkg or (td.cu.package or '').startswith(pkg + '.')]
            elif sel.endswith('.*'):
                pkg = sel[:-2]
                found = [td for td in tops if td.cu.package == pkg]
            elif '.' in sel:
                td = self.core_type(sel)
                if td is None:
                    raise SkeletonError(f'selector {sel}: no such Java type under {self.java_root}')
                found = [self.top_level(td)]
                self._check_draftable(found[0], sel)
            else:
                found = [td for td in tops if td.name == sel]
                if len(found) > 1:
                    raise SkeletonError(f'selector {sel} is ambiguous: {sorted(t.fqn for t in found)}')
                for td in found:
                    self._check_draftable(td, sel)
            if not found:
                raise SkeletonError(f'selector {sel} matches no Java type')
            if sel.startswith('@') or sel.endswith('.*') or sel.endswith('.**'):
                kept = [td for td in found if not self.already_ported(td)]
                self.skipped_existing.update(td.fqn for td in found if self.already_ported(td))
                found = kept
            for td in found:
                out[td.fqn] = td
        return [out[k] for k in sorted(out)]

    def _check_draftable(self, td, sel):
        owner = self.generator_owner(td)
        if owner is None or owner[0] == 'member block':
            return
        if owner[0] == 'xmlgen' and owner[1] == 'enum':
            header = (self.generated_enum(td) or ((), 'staticdata-classes.json'))[1]
            raise SkeletonError(f'selector {sel}: {td.fqn} is an enum xmlgen generates ({header}); draft the Java file of the classes '
                                'that use it instead')
        if owner[0] == 'xmlgen':
            how =('write the class with `python cpp/tools/xmlgen/xmlgen.py scaffold`' if owner[1] == 'behaviour'
                   else 'xmlgen generates its header')
            raise SkeletonError(f'selector {sel}: {td.fqn} is a static data class ({owner[1]}) of staticdata-classes.json: {how}')
        raise SkeletonError(f'selector {sel}: {td.fqn} is replaced by the generated {owner[1]}')

    def member_headers(self, td):
        """Headers of the project types that td's fieldmap members (and those of its project superclasses) need complete where td is
        constructed or destroyed (Ref<X>/shim destructors), excluding td's own file."""
        key = id(td)
        cached = self._member_headers.get(key)
        if cached is not None:
            return cached
        self._member_headers[key] = set()        # cycle guard
        result = set()
        lay = self.layout(td)
        if lay is not None and lay.members:
            emitter = DraftEmitter(self, td.cu)
            emitter.ctx.capture = []
            for ml in lay.members.values():
                text = ml.declaration or ml.cpp_type
                if text and '/*' not in text:
                    emitter.spell_cpp(' '.join(text.split()), td)
            for t in emitter.ctx.capture:
                top = self.top_level(t)
                if top.cu is not td.cu:
                    result.add(self.unit_header(top.cu))
        sup = self.superclass(td)
        if sup is not None:
            result |= self.member_headers(sup)
            if sup.cu is not td.cu:
                result.add(self.unit_header(sup.cu))
        self._member_headers[key] = result
        return result

    def todo_signatures(self, td):
        """(name, arity) of td's methods whose draft declaration is a TODO(signature) comment."""
        key = id(td)
        cached = self._todo_signatures.get(key)
        if cached is None:
            self._todo_signatures[key] = set()     # cycle guard
            emitter = DraftEmitter(self, td.cu)
            plans = emitter.plan_methods(td, bool(self.cpp_type_params(td)), False, self.superclass(td), None, check_create=False)
            cached = {(pl.m.name, len(pl.m.params)) for pl in plans if pl.todo is not None and pl.m is not None}
            self._todo_signatures[key] = cached
        return cached

    def cpp_by_name(self, name):
        """(namespace, CppDecl) of the only existing game server C++ class/struct/enum with this simple name, else None."""
        if self._cpp_names is None:
            by_name = {}
            for (ns, n), d in self.cpp.decls.items():
                by_name.setdefault(n, []).append((ns, d))
            self._cpp_names = {n: v[0] for n, v in by_name.items() if len(v) == 1}
        return self._cpp_names.get(name)

    def already_ported(self, td):
        """True if --cpp-src or the generated tree already has the C++ header of td's Java file, or another generator owns it (group and
        package selectors skip those)."""
        return self.cpp.has_header(self.unit_header(td.cu)) or self.generator_owner(td) is not None

    def _group(self, name, tops):
        p = JAVA_PREFIX + '.'
        if name == '@all':
            return list(tops)
        if name == '@hubs':
            res = []
            for rel in HUBS:
                td = self.core_type(p + rel)
                if td is None:
                    raise SkeletonError(f'@hubs: {p + rel} does not exist')
                res.append(td)
            return res
        if name == '@services':
            return [td for td in tops if td.kind == 'class' and (td.cu.package or '').startswith(p + 'services')
                    and (is_singleton(td) or is_static_only(td))]
        if name == '@daos':
            return [td for td in tops if td.cu.package == p + 'dao']
        if name == '@serverpackets':
            return [td for td in tops if (td.cu.package or '').startswith(p + 'network.aion.serverpackets')]
        if name == '@engines':
            return [td for td in tops if td.kind == 'class' and td.name.endswith('Engine') and is_singleton(td)
                    and not (td.cu.package or '').startswith(p + 'services')]
        raise SkeletonError(f'unknown group {name}')


# ----------------------------------------------------------------------------------------------------------------------------------
# Forward headers
# ----------------------------------------------------------------------------------------------------------------------------------

def generate_fwd(project):
    """{relative path: content} of every fwd.h."""
    by_dir = {}
    for cu in project.core_units:
        if not cu.types:
            continue
        by_dir.setdefault(project.unit_dir(cu), []).append(cu)
    files = {}
    for d in sorted(by_dir):
        units = by_dir[d]
        package = units[0].package
        ns = project.unit_ns(units[0])
        decls, nested, annotations = [], [], []
        need_cstdint = False
        for cu in units:
            for td in cu.types:
                line, uses_std = _fwd_decl(project, ns, td)
                if line is None:
                    annotations.append(td.name)
                else:
                    decls.append((td.name, line))
                    need_cstdint |= uses_std
                for inner in cu.all_types():
                    if inner.outer is not None and project.top_level(inner) is td:
                        in_enum = any(o.kind == 'enum' for o in _outers(inner))
                        nested.append('::'.join(project.type_path(inner)) + ('  (Java type nested in an enum)' if in_enum else ''))
        names = [n for n, _ in decls]
        dup = {n for n in names if names.count(n) > 1}
        if dup:
            raise SkeletonError(f'package {package}: duplicate type names {sorted(dup)}')
        lines = [f'{FWD_MARK} from the Java package {package} - do not edit.',
                 '// Regenerate after adding a Java class or regenerating xmlgen: python cpp/tools/gen/skeleton.py --fwd --out cpp/game-server/src',
                 '#pragma once', '']
        if need_cstdint:
            lines += ['#include <cstdint>', '']
        lines.append(f'namespace {"::".join(ns)} {{')
        lines.append('')
        lines += [line for _, line in sorted(decls)]
        if nested:
            lines.append('')
            lines.append('// Nested types (declared inside their outer class, include its header):')
            lines += [f'//   {n}' for n in sorted(nested)]
        if annotations:
            lines.append('')
            lines.append('// Java annotation types (no C++ type; markers and registries replace them):')
            lines += [f'//   @{n}' for n in sorted(annotations)]
        lines.append('')
        lines.append(f'}} // namespace {"::".join(ns)}')
        files[f'{d}/fwd.h'] = '\n'.join(lines) + '\n'
    return files


def _std_int(type_name):
    """`uint8_t` -> `std::uint8_t` (the same type; xmlgen spells enum bases without std::)"""
    return 'std::' + type_name if re.fullmatch(r'u?int(?:8|16|32|64)_t', type_name) else type_name


def _outers(td):
    t = td.outer
    while t is not None:
        yield t
        t = t.outer


def _fwd_decl(project, ns, td):
    """(declaration line or None for annotation types, uses <cstdint>)."""
    if td.kind == 'annotation':
        return None, False
    name = cpp_ident(td.name)
    if tuple(ns) + (name,) in project.cpp.namespaces:
        return f'// {name}: a C++ namespace in the existing port (no forward declaration)', False
    existing = project.cpp.declaration(ns, name)
    if existing is not None:
        if existing.key == 'enum':
            if existing.underlying:
                underlying = _std_int(existing.underlying)
                return f'enum class {name} : {underlying};', underlying.startswith('std::')
            return f'enum class {name};', False
        if existing.template:
            if '=' in existing.template:
                return f'// {existing.template} {existing.key} {name}: see {existing.path} (default template arguments)', False
            return f'{existing.template} {existing.key} {name};', False
        return f'{existing.key} {name};', False
    alias = project.cpp.aliases.get((tuple(ns), name))
    if alias is not None:
        return f'// {name}: a C++ alias or using-declaration in {alias} (no forward declaration)', False
    if td.kind == 'enum':
        return f'enum class {name} : {enum_underlying(td)};', True
    if project.cpp_type_params(td):
        params = ', '.join(f'class {cpp_ident(p.name)}' for p in td.type_params)
        return f'template <{params}> class {name};', False
    return f'class {name};', False


# ----------------------------------------------------------------------------------------------------------------------------------
# Type mapping
# ----------------------------------------------------------------------------------------------------------------------------------

PRIMITIVE_TYPES = {'int': 'int32_t', 'long': 'int64_t', 'short': 'int16_t', 'byte': 'int8_t', 'char': 'char16_t', 'boolean': 'bool',
                   'float': 'float', 'double': 'double', 'void': 'void'}
BOXED = {'java.lang.Integer': 'int32_t', 'java.lang.Long': 'int64_t', 'java.lang.Short': 'int16_t', 'java.lang.Byte': 'int8_t',
         'java.lang.Character': 'char16_t', 'java.lang.Boolean': 'bool', 'java.lang.Float': 'float', 'java.lang.Double': 'double'}
SEQUENCES = frozenset(('java.util.List', 'java.util.ArrayList', 'java.util.LinkedList', 'java.util.Collection', 'java.lang.Iterable',
                       'java.util.Queue', 'java.util.Deque', 'java.util.ArrayDeque', 'java.util.concurrent.CopyOnWriteArrayList',
                       'java.util.concurrent.ConcurrentLinkedQueue', 'java.util.concurrent.ConcurrentLinkedDeque',
                       'java.util.concurrent.BlockingQueue', 'java.util.concurrent.LinkedBlockingQueue'))
HASH_SETS = frozenset(('java.util.Set', 'java.util.HashSet', 'java.util.concurrent.CopyOnWriteArraySet'))
TREE_SETS = frozenset(('java.util.TreeSet', 'java.util.SortedSet', 'java.util.NavigableSet', 'java.util.concurrent.ConcurrentSkipListSet'))
HASH_MAPS = frozenset(('java.util.Map', 'java.util.HashMap', 'java.util.concurrent.ConcurrentHashMap', 'java.util.concurrent.ConcurrentMap'))
TREE_MAPS = frozenset(('java.util.TreeMap', 'java.util.SortedMap', 'java.util.NavigableMap', 'java.util.concurrent.ConcurrentSkipListMap'))
FUNCTIONS = {'java.util.function.Predicate': (1, 'bool'), 'java.util.function.BiPredicate': (2, 'bool'),
             'java.util.function.Consumer': (1, 'void'), 'java.util.function.BiConsumer': (2, 'void'),
             'java.util.function.Function': (1, None), 'java.util.function.BiFunction': (2, None),
             'java.util.function.Supplier': (0, None)}
CONNECTION_FQN = JAVA_PREFIX + '.network.aion.AionConnection'
VISIBLE_OBJECT_FQN = JAVA_PREFIX + '.model.gameobjects.VisibleObject'
# Java library type -> (commons header, C++ path, 'ref' (parameters only, by reference) | 'value' (std::optional unless in containers))
COMMONS_EQUIVALENTS = {
    'java.nio.ByteBuffer': ('aion/commons/utils/ByteBuffer.h', ('aion', 'commons', 'utils', 'ByteBuffer'), 'ref'),
    'java.sql.Connection': ('aion/commons/database/Connection.h', ('aion', 'commons', 'database', 'Connection'), 'ref'),
    'java.sql.PreparedStatement': ('aion/commons/database/PreparedStatement.h', ('aion', 'commons', 'database', 'PreparedStatement'), 'ref'),
    'java.sql.ResultSet': ('aion/commons/database/ResultSet.h', ('aion', 'commons', 'database', 'ResultSet'), 'ref'),
    'java.sql.Timestamp': ('aion/commons/database/SqlTypes.h', ('aion', 'commons', 'database', 'Timestamp'), 'value'),
}
XML_HOOK_HEADER = 'aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h'
# Simple C++ names fieldmap.py uses for commons types: (header, C++ path)
COMMONS_TYPES = {
    'Timestamp': ('aion/commons/database/SqlTypes.h', ('aion', 'commons', 'database', 'Timestamp')),
    'Logger': ('aion/commons/logging/Logger.h', ('aion', 'commons', 'logging', 'Logger')),
    'InetSocketAddress': ('aion/commons/utils/InetSocketAddress.h', ('aion', 'commons', 'utils', 'InetSocketAddress')),
    'ByteBuffer': ('aion/commons/utils/ByteBuffer.h', ('aion', 'commons', 'utils', 'ByteBuffer')),
}
STD_INCLUDES = [('std::filesystem', 'filesystem'), ('std::regex', 'regex'), ('std::wregex', 'regex'), ('std::array', 'array'),
                ('std::atomic', 'atomic'), ('std::deque', 'deque'), ('std::list', 'list'), ('std::pair', 'utility'), ('std::variant', 'variant'),
                ('std::mutex', 'mutex'), ('std::bitset', 'bitset'), ('std::tuple', 'tuple'), ('std::vector', 'vector'), ('std::optional', 'optional'), ('std::string_view', 'string_view'), ('std::span', 'span'),
                ('std::unordered_map', 'unordered_map'), ('std::unordered_set', 'unordered_set'), ('std::map', 'map'), ('std::set', 'set'),
                ('std::function', 'functional'), ('std::reference_wrapper', 'functional'), ('std::initializer_list', 'initializer_list'),
                ('std::any', 'any'), ('std::chrono', 'chrono'), ('std::shared_ptr', 'memory'), ('std::unique_ptr', 'memory')]


class Todo(Exception):
    """A Java construct without a mechanical C++ spelling (the declaration becomes a TODO comment)."""


class HeaderContext:
    """State of one generated header: namespace, class-scope names, includes."""

    def __init__(self, project, cu):
        self.project = project
        self.cu = cu
        self.ns = project.unit_ns(cu)
        self.scope_names = set()
        self.includes = set()       # project/runtime/commons include paths
        self.value_headers = set()  # full headers of project types returned by value (needed where stubs are defined)
        self.capture = None         # list collecting the project types spelled while it is set
        self.unit_types = {id(t) for t in cu.all_types()}

    # -- spelling
    def qualify(self, full):
        """Shortest spelling of the C++ path `full` (namespace + names) that unqualified lookup from this header's namespace (and from
        the class scopes of this file) resolves to exactly that path."""
        full = tuple(full)
        known = self.project.known_paths
        for start in range(len(full) - 1, -1, -1):
            first = full[start]
            if first in self.scope_names:
                continue
            hit = None
            for i in range(len(self.ns), -1, -1):
                cand = self.ns[:i] + (first,)
                if cand in known:
                    hit = cand
                    break
            if hit == full[:start + 1]:
                return '::'.join(full[start:])
        return '::' + '::'.join(full)

    def runtime(self, symbol):
        path = self.project.cpp.symbols.get(symbol) or RUNTIME_FALLBACK_SYMBOLS.get(symbol)
        if path is None:
            raise SkeletonError(f'runtime symbol {symbol} not found under --cpp-src')
        self.includes.add(path)
        return self.qualify(RUNTIME_NAMESPACE + (symbol,))

    def project_type(self, td, as_base=False):
        """Spelling of a project type; records the include (fwd.h, or the outer's/base's full header; the generated header of a nested
        enum xmlgen generates, spelled Outer_Inner)."""
        cu = td.cu
        generated = self.project.generated_enum(td)
        if generated is not None and td.outer is not None and id(td) not in self.unit_types:
            self.includes.add(generated[1])
            return self.qualify(generated[0])
        if self.capture is not None and generated is None:
            # a generated enum is complete with its opaque declaration (fwd.h) or its alias: no full header where it is held by value
            self.capture.append(td)
        path = self.project.unit_ns(cu) + self.project.type_path(td)
        if id(td) not in self.unit_types:
            if as_base or td.outer is not None:
                self.includes.add(self.project.unit_header(cu))
            elif self.project.unit_kind[id(cu)] == 'core':
                self.includes.add(f'{self.project.unit_dir(cu)}/fwd.h')
            else:
                raise Todo(f'handler type {td.fqn}')
        return self.qualify(path)

    def _value_type(self, td):
        top = self.project.top_level(td)
        if id(top) not in self.unit_types:
            self.value_headers.add(self.project.unit_header(top.cu))

    def commons_type(self, fqn):
        parts = fqn.split('.')
        rel = parts[2:]        # com.aionemu.commons.<pkg>.<Class> -> commons/<pkg>/<Class>.h
        header = 'aion/' + '/'.join(rel[:-1]) + '/' + rel[-1] + '.h'
        src = self.project.commons_src
        if src is None or not (src / header).is_file():
            raise Todo(f'{fqn} has no C++ header {header}')
        self.includes.add(header)
        return self.qualify(('aion',) + tuple(rel))

    # -- mapping
    def map_type(self, ref, ctx, pos, varargs=False):
        """Java TypeRef -> C++ text. pos: param, return, element, key, typearg. Raises Todo."""
        if varargs:
            if ref.dims == 0 and not ref.wildcard and ref.name not in PRIMITIVE_TYPES:
                kind, value = self.project.index.resolve_kind(ref.name, ctx)
                if kind == 'external' and value == 'java.lang.Object':
                    return 'std::initializer_list<std::any>'                           # hub-headers.md §7.4
                if kind == 'project' and self.project.kind(self.project.index.types[value]) == 'K2':
                    return f'std::initializer_list<std::reference_wrapper<{self._project(self.project.index.types[value], ref, ctx, "typearg")}>>'
            if ref.dims == 0 and pos == 'param':
                return f'std::initializer_list<{_ref_to_ptr(self.map_type(ref, ctx, "element"))}>'
            ref = ref.with_dims(1)
        if ref.wildcard is not None:
            if ref.wildcard in ('extends', 'super') and ref.bound is not None:
                return self.map_type(ref.bound, ctx, pos)    # hub-headers.md §8.3
            raise Todo(f'wildcard {ref}')
        if ref.dims:
            if ref.dims > 1:
                raise Todo(f'multi-dimensional array {ref}')
            elem = javasrc.TypeRef(ref.segments, 0, ref.annotations, line=ref.line, col=ref.col, index=ref.index)
            if elem.name == 'byte':
                return 'std::span<const uint8_t>' if pos == 'param' else 'std::vector<uint8_t>'
            e = self.map_type(elem, ctx, 'element')
            if pos in ('param', 'return'):
                e = _ref_to_ptr(e)
            if pos == 'param':
                return f'std::span<{e} const>' if e.startswith('const ') else f'std::span<const {e}>'
            return f'std::vector<{e}>'
        name = ref.name
        if name in PRIMITIVE_TYPES:
            if name == 'void' and pos != 'return':
                raise Todo('void outside a return type')
            return PRIMITIVE_TYPES[name]
        kind, value = self.project.index.resolve_kind(name, ctx)
        if kind == 'typevar':
            return self._typevar(ref, ctx, pos)
        if kind == 'project':
            return self._project(self.project.index.types[value], ref, ctx, pos)
        if kind == 'external':
            return self._external(value, ref, ctx, pos)
        raise Todo(f'type {ref} resolves as {kind}')

    def _args(self, ref, ctx, count):
        args = ref.args
        if args is None or len(args) != count:
            raise Todo(f'raw or unexpected generic use {ref}')
        return args

    def _typevar(self, ref, ctx, pos):
        owner = ctx if isinstance(ctx, javasrc.TypeDecl) else getattr(ctx, 'owner', None)
        if isinstance(ctx, javasrc.MethodDecl) and any(p.name == ref.name for p in ctx.type_params):
            raise Todo(f'type variable {ref.name} of a generic method')
        param = None
        t = owner
        declaring = None
        while t is not None and param is None:
            param = next((p for p in t.type_params if p.name == ref.name), None)
            declaring = t
            t = t.outer
        if param is None:
            raise Todo(f'type variable {ref.name}')
        if self.project.erased_generic(declaring):
            spelled = ERASURE_BOUNDS.get(declaring.fqn, {}).get(ref.name)
            if spelled is not None:
                return self.map_type(javasrc.TypeRef([(part, None) for part in spelled.split('.')]), declaring, pos)
            if not param.bounds:
                raise Todo(f'unbounded type variable {ref.name} of the non-template {declaring.fqn}')
            return self.map_type(param.bounds[0], declaring, pos)
        name = cpp_ident(ref.name)
        if pos == 'typearg':
            return name
        # Java type arguments are reference types: object semantics, like a shared (K4) class
        return self.runtime('Ref' if pos in ('element', 'key') else 'Ptr') + f'<{name}>'

    def _project(self, td, ref, ctx, pos):
        if td.kind == 'annotation':
            raise Todo(f'annotation type {td.fqn} used as a type')
        chain = td.outer
        while chain is not None:
            if self.project.cpp_type_params(chain):
                raise Todo(f'member type of generic class {chain.fqn}')
            if chain.kind in ('enum', 'annotation'):
                raise Todo(f'type nested in the enum {chain.fqn} (C++ enums have no member types)')
            chain = chain.outer
        spelled = self.project_type(td)
        if self.project.cpp_type_params(td):
            args = self._args(ref, ctx, len(td.type_params))
            spelled += '<' + ', '.join(self.map_type(a, ctx, 'typearg') for a in args) + '>'
        elif ref.args and not td.type_params:
            raise Todo(f'type arguments on non-generic {td.fqn}')
        if pos == 'typearg':
            return spelled
        if td.kind == 'enum':
            return spelled
        if td.fqn == CONNECTION_FQN:
            return f'{spelled}*' if pos == 'param' else f'std::shared_ptr<{spelled}>'
        kind = self.project.kind(td)
        if kind == 'K1':
            return f'const {spelled}*'
        if kind == 'K2':
            if pos == 'param':
                return f'{spelled}&'
            if pos == 'return':
                self._value_type(td)
                return spelled          # packets are stack temporaries (runtime-architecture.md §8.2): factories return by value
            raise Todo(f'packet {td.fqn} stored in a container')
        if kind == 'K5':
            if pos == 'param':
                return f'{spelled}&'
            if td.kind == 'interface' or 'abstract' in td.modifiers:
                return f'std::unique_ptr<{spelled}>'   # a confined value of an abstract type: owned, never sliced
            self._value_type(td)
            return spelled
        if self.project.base_kind(td) == 'Immortal':
            if not is_singleton(td):
                return f'const {spelled}*'      # interned immortals (fieldmap.toml [immortal]): like templates (hub-headers.md §5)
            return f'{spelled}&' if pos in ('param', 'return') else f'{spelled}*'
        if pos in ('element', 'key'):
            return self.runtime('Ref') + f'<{spelled}>'
        return self.runtime('Ptr') + f'<{spelled}>'

    def _external(self, fqn, ref, ctx, pos):
        if fqn == 'java.lang.String':
            if ref.args:
                raise Todo(f'{ref}')
            return 'std::string_view' if pos == 'param' else 'std::string'
        if fqn in BOXED:
            prim = BOXED[fqn]
            return prim if pos in ('element', 'key', 'typearg') else f'std::optional<{prim}>'
        borrowed = _ref_to_ptr if pos in ('param', 'return') else (lambda text: text)
        if fqn in SEQUENCES:
            (a,) = self._args(ref, ctx, 1)
            return self._container(f'std::vector<{borrowed(self.map_type(a, ctx, "element"))}>', pos)
        if fqn in HASH_SETS or fqn in TREE_SETS:
            (a,) = self._args(ref, ctx, 1)
            e = self.map_type(a, ctx, 'key')
            if fqn in TREE_SETS and ('Ref<' in e or e.endswith('*')):
                raise Todo(f'ordered set of objects {ref}')
            return self._container(f'std::{"unordered_set" if fqn in HASH_SETS else "set"}<{borrowed(e)}>', pos)
        if fqn in HASH_MAPS or fqn in TREE_MAPS:
            k, v = self._args(ref, ctx, 2)
            kt, vt = self.map_type(k, ctx, 'key'), self.map_type(v, ctx, 'element')
            if fqn in TREE_MAPS and ('Ref<' in kt or kt.endswith('*')):
                raise Todo(f'ordered map with object keys {ref}')
            return self._container(f'std::{"unordered_map" if fqn in HASH_MAPS else "map"}<{borrowed(kt)}, {borrowed(vt)}>', pos)
        if fqn == 'java.util.Optional':
            (a,) = self._args(ref, ctx, 1)
            inner = self.map_type(a, ctx, 'return')
            if pos != 'return':
                raise Todo(f'Optional outside a return type {ref}')
            return inner if ('Ptr<' in inner or inner.endswith('*')) else f'std::optional<{inner}>'
        if fqn in ('java.util.concurrent.Future', 'java.util.concurrent.ScheduledFuture'):
            if pos in ('param', 'return', 'element'):
                return self.runtime('FutureRef')
            raise Todo(f'{ref}')
        if fqn in FUNCTIONS:
            arity, ret = FUNCTIONS[fqn]
            if pos != 'param':
                raise Todo(f'stored functional interface {ref} (PinnedCallback rules)')
            args = self._args(ref, ctx, arity + (0 if ret else 1))
            params = [_ptr_to_reference(self.map_type(a, ctx, 'param')) for a in args[:arity]]   # hub-headers.md §7.3
            r = ret if ret else self.map_type(args[-1], ctx, 'return')
            return f'const std::function<{r}({", ".join(params)})>&'
        if fqn == 'java.time.Duration':
            return 'std::chrono::milliseconds'
        if fqn == 'java.io.File':
            return 'const std::filesystem::path&' if pos == 'param' else 'std::filesystem::path'
        if fqn in COMMONS_EQUIVALENTS:
            header, path, how = COMMONS_EQUIVALENTS[fqn]
            self.includes.add(header)
            spelled = self.qualify(path)
            if how == 'ref':
                if pos == 'param':
                    return f'{spelled}&'
                raise Todo(f'{fqn} outside a parameter')
            return spelled if pos in ('element', 'key', 'typearg') else f'std::optional<{spelled}>'
        if fqn == 'java.lang.Object' and pos in ('param', 'return'):
            return 'const std::any&' if pos == 'param' else 'std::any'     # hub-headers.md §6, §7.4
        if fqn.startswith('com.aionemu.commons.'):
            spelled = self.commons_type(fqn)
            if ref.args:
                raise Todo(f'generic commons type {ref}')
            if pos == 'param':
                return f'{spelled}&'
            raise Todo(f'commons type {fqn} outside a parameter')
        raise Todo(f'no C++ mapping for {fqn}')

    @staticmethod
    def _container(text, pos):
        if pos == 'param':
            return f'const {text}&'
        return text


# ----------------------------------------------------------------------------------------------------------------------------------
# Drafts
# ----------------------------------------------------------------------------------------------------------------------------------

def javadoc_lines(raw):
    if not raw:
        return []
    body = raw.strip()
    if body.startswith('/**'):
        body = body[3:]
    if body.endswith('*/'):
        body = body[:-2]
    out = []
    for line in body.splitlines():
        s = line.strip()
        if s.startswith('*'):
            s = s[1:]
            if s.startswith(' '):
                s = s[1:]
        out.append(s.rstrip().replace('*/', '* /'))
    while out and not out[0]:
        out.pop(0)
    while out and not out[-1]:
        out.pop()
    collapsed = []
    for s in out:
        if s or (collapsed and collapsed[-1]):
            collapsed.append(s)
    return collapsed


def doc_block(raw, indent, summary_only=False):
    lines = javadoc_lines(raw)
    if summary_only:
        summary = []
        for s in lines:
            if not s or s.startswith('@'):
                break
            summary.append(s)
        lines = summary
    if not lines:
        return []
    if summary_only and len(lines) == 1 and len(lines[0]) < 110:
        return [f'{indent}/** {lines[0]} */']
    return [f'{indent}/**'] + [f'{indent} * {s}'.rstrip() for s in lines] + [f'{indent} */']


def one_line(text, limit=100):
    s = ' '.join(text.split())
    if len(s) > limit:
        s = s[:limit - 3] + '...'
    return s.rstrip('\\')


# data members of runtime::RefCounted / OwnedPart / OwnedPartBase / Immortal (a parameter with such a name would hide them: C4458)
RUNTIME_BASE_MEMBERS = frozenset(('count', 'queued', 'retireEpoch', 'monitor_', 'cookie', 'partRefs_', 'partStamp_', 'owner_'))
# member functions of those bases: a data member with such a name would hide them from Ref<X>/SYNCHRONIZED (renamed with '_')
RUNTIME_BASE_METHODS = frozenset(('retain', 'release', 'monitor', 'refCount', 'isManaged', 'isRegistered', 'partOwner', 'partRefCount',
                                  'bindOwner', 'isOwnerBound'))

_POINTER_LIKE = frozenset(('Ref', 'Ptr', 'OwnerRef', 'SelfOrRef', 'unique_ptr', 'shared_ptr', 'weak_ptr', 'PartSlot', 'PartMap', 'PartList',
                           'PinnedCallback', 'function'))


_PTR_TYPE_RE = re.compile(r'^((?:::)?(?:\w+::)*)Ptr<(.+)>$')
_REF_TYPE_RE = re.compile(r'^((?:::)?(?:\w+::)*)Ref<(.+)>$')


def _ptr_to_reference(text):
    """`runtime::Ptr<X>` -> `X&` (non-null object parameters and callback arguments, hub-headers.md §5.1, §7.3); other types unchanged."""
    m = _PTR_TYPE_RE.match(text)
    return f'{m.group(2)}&' if m else text


def _ref_to_ptr(text):
    """`runtime::Ref<X>` -> `runtime::Ptr<X>` (borrowed elements of collections in parameters and returns, hub-headers.md §7.1)."""
    m = _REF_TYPE_RE.match(text)
    return f'{m.group(1)}Ptr<{m.group(2)}>' if m else text


def _same_type(member_type, param_type):
    """Textual type equality modulo the qualification of fieldmap spellings (Race vs model::Race)."""
    strip = lambda t: re.sub(r'\b(?:[A-Za-z_]\w*::)+(?=[A-Za-z_])', '', t.replace(' ', ''))
    return strip(member_type) == strip(param_type)


def _template_argument_of(text, start):
    """Name of the innermost template whose argument list contains text[start], or None."""
    depth = 0
    for i in range(start - 1, -1, -1):
        ch = text[i]
        if ch == '>':
            depth += 1
        elif ch == '<':
            if depth == 0:
                name = re.search(r'(\w+)\s*$', text[:i])
                return name.group(1) if name else None
            depth -= 1
    return None


def _held_by_value(text, start, end):
    """True if the type name at text[start:end] is held by value: not a pointer/reference and not an argument of a pointer-like template
    (Ref<X>, unique_ptr<X>, PartSlot<X>, ...)."""
    after = text[end:].lstrip()
    if after.startswith(('*', '&')):
        return False
    depth = 0
    i = start - 1
    while i >= 0:
        ch = text[i]
        if ch == '>':
            depth += 1
        elif ch == '<':
            if depth == 0:
                name = re.search(r'(\w+)\s*$', text[:i])
                return not (name and name.group(1) in _POINTER_LIKE)
            depth -= 1
        elif ch == '(' and depth == 0:
            return False     # function type of a callback
        i -= 1
    return True


def java_access(mods, in_interface=False, nested=False):
    """C++ access of a Java member. Package-private -> public. Private members of nested types -> public: Java lets the whole top-level
    class (outer and sibling nested types) use them, C++ does not."""
    if in_interface:
        return 'private' if 'private' in mods and not nested else 'public'
    for a in ('public', 'protected', 'private'):
        if a in mods:
            return 'public' if a == 'private' and nested else a
    return 'public'


def java_field_text(f):
    init = ''
    if f.initializer is not None:
        init = ' = ' + one_line(f.initializer.text, 60)
    mods = ' '.join(f.modifiers)
    return one_line(f'{mods} {f.type} {f.name}{init};'.strip(), 140)


def java_method_text(m):
    mods = ' '.join(x for x in m.modifiers)
    tparams = ('<' + ', '.join(p.name for p in m.type_params) + '> ') if m.type_params else ''
    params = ', '.join(f'{p.type}{"..." if p.varargs else ""} {p.name}' for p in m.params)
    ret = '' if m.kind != 'method' else f'{m.return_type} '
    return one_line(f'{mods} {tparams}{ret}{m.name}({params})'.strip(), 160)


# -- constants --------------------------------------------------------------------------------------------------------------------

_CONST_OPS = frozenset(('+', '-', '*', '/', '%', '(', ')', '<<', '&', '|', '^', '~'))


def _int_literal(text, java_type):
    t = text.replace('_', '')
    is_long = t[-1] in 'lL'
    t = t.rstrip('lL')
    value = javasrc.literal_value(javasrc.INT, text)
    bits = 64 if is_long else 32
    if value >= 1 << bits:
        raise Todo(f'integer literal {text} out of range')
    signed = value - (1 << bits) if value >= 1 << (bits - 1) else value
    if signed != value:
        return f'static_cast<{"int64_t" if is_long else "int32_t"}>({t}{"ULL" if is_long else "U"})'
    if t.lower().startswith('0b'):
        return t + ('LL' if is_long else '')
    return t.replace("'", '') + ('LL' if is_long else '')


def _float_literal(text, java_type):
    t = text.replace('_', '')
    suffix = t[-1] if t[-1] in 'fFdD' else ''
    body = t[:-1] if suffix else t
    if not body.lower().startswith('0x') and not any(c in body for c in '.eE'):
        body += '.0'
    is_float = suffix in ('f', 'F') or java_type == 'float'
    return body + ('f' if is_float else '')


def _cpp_string_literal(value):
    try:
        value.encode('utf-8')
    except UnicodeEncodeError as e:
        raise Todo('string with unpaired surrogates') from e
    out = []
    for ch in value:
        o = ord(ch)
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif o < 0x20 or o == 0x7f:
            out.append(f'\\{o:03o}')
        elif ch == '?':
            out.append('?')
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'


def constant_initializer(f):
    """C++ initializer text for a static final primitive/String field with a literal-only initializer; raises Todo otherwise."""
    if f.initializer is None or f.type.dims or f.type.args:
        raise Todo('not a literal constant')
    cu = f.owner.cu
    tk = cu.tokens
    s, e = f.initializer.start, f.initializer.end
    jt = f.type.name
    if jt == 'String' or jt == 'java.lang.String':
        if e - s == 1 and tk.kind[s] == javasrc.STRING:
            return _cpp_string_literal(javasrc.literal_value(tk.kind[s], tk.text[s]))
        raise Todo('String constant is not a single literal')
    if jt == 'boolean':
        if e - s == 1 and tk.text[s] in ('true', 'false'):
            return tk.text[s]
        raise Todo('boolean constant is not a literal')
    if jt == 'char':
        if e - s == 1 and tk.kind[s] == javasrc.CHAR:
            ch = javasrc.literal_value(tk.kind[s], tk.text[s])
            o = ord(ch)
            if 0x20 <= o < 0x7f and ch not in "'\\":
                return f"u'{ch}'"
            return f'0x{o:04X}'
        raise Todo('char constant is not a literal')
    if jt not in PRIMITIVE_TYPES or jt == 'void':
        raise Todo('not a primitive constant')
    parts = []
    i = s
    while i < e:
        k, t = tk.kind[i], tk.text[i]
        if k == javasrc.INT:
            parts.append(_int_literal(t, jt))
        elif k == javasrc.FLOAT:
            if jt not in ('float', 'double'):
                raise Todo('floating literal in an integer constant')
            parts.append(_float_literal(t, jt))
        elif t == '>' and i + 1 < e and tk.text[i + 1] == '>':
            if i + 2 < e and tk.text[i + 2] == '>':
                raise Todo('>>> in a constant')
            parts.append('>>')
            i += 1
        elif k == javasrc.OP and t in _CONST_OPS:
            parts.append(t)
        else:
            raise Todo('constant initializer is not literal arithmetic')
        i += 1
    if jt in ('float', 'double') and len(parts) == 1 and tk.kind[s] == javasrc.INT:
        return _float_literal(tk.text[s], jt)
    out = []
    prev = None          # previous part
    prev_unary = False   # previous part is a unary operator
    for part in parts:
        unary = part in ('-', '+', '~') and (prev is None or (prev in _CONST_OPS and prev != ')') or prev == '>>')
        if out and not (prev == '(' or prev_unary or part == ')'):
            out.append(' ')
        out.append(part)
        prev, prev_unary = part, unary
    return ''.join(out)


def logger_name(f, index):
    """Logger name for `static final Logger log = LoggerFactory.getLogger(X.class | "name")`, else None."""
    if f.type.name not in ('Logger', 'org.slf4j.Logger') or 'static' not in f.modifiers or f.initializer is None:
        return None
    texts = f.initializer.texts()
    if texts[:4] != ['LoggerFactory', '.', 'getLogger', '('] or texts[-1] != ')':
        return None
    inner = texts[4:-1]
    tk = f.owner.cu.tokens
    if len(inner) == 1 and tk.kind[f.initializer.start + 4] == javasrc.STRING:
        return javasrc.literal_value(javasrc.STRING, inner[0])
    if len(inner) >= 3 and inner[-2:] == ['.', 'class'] and all(t == '.' or re.match(r'\w+\Z', t) for t in inner[:-2]):
        kind, value = index.resolve_kind(''.join(inner[:-2]), f.owner)
        if kind == 'project':
            return index.types[value].binary_name
    return None


# -- plans ------------------------------------------------------------------------------------------------------------------------

@dataclass
class MethodPlan:
    m: object
    access: str
    decl: str | None = None          # declaration line without indentation/semicolon (None -> todo)
    todo: str | None = None
    stub_head: str | None = None     # out-of-class definition head (without body)
    inline_body: str | None = None   # body for inline accessors
    stub_body: list = field(default_factory=lambda: ['AION_UNPORTED();'])
    pure: bool = False
    key: tuple = ()
    comment: str | None = None
    kind: str = 'method'
    params: list = field(default_factory=list)   # [(C++ type, C++ name)] as declared (Java names)
    def_params: list = field(default_factory=list)  # names used in definitions: a parameter never hides a data member (C4458)
    def_decl: str | None = None      # declaration text with def_params (inline definitions)
    mem_init: str = ''                            # ' : Base(...)' of constructor stubs
    note: str | None = None                       # emitted as a plain comment instead of a declaration (cast-only override, inherited)


class DraftEmitter:
    def __init__(self, project, cu, unported_header=DEFAULT_UNPORTED_HEADER):
        self.project = project
        self.cu = cu
        self.ctx = HeaderContext(project, cu)
        self.unported_header = unported_header
        self.stubs = []                 # (template context, lines)
        self.cpp_statics = []           # namespace-scope lines in the .cpp
        self.cpp_includes = set()
        self.logger_names_used = set()
        self.inline_stubs = False
        self.emitted = {}               # id(TypeDecl) -> {java name: MemberLayout} declared from fieldmap.json
        self.reference_members = {}     # id(TypeDecl) -> [(member name, referenced type)] bound by constructor stubs
        self.needs_helper = set()       # id(TypeDecl) of classes whose constructor stubs use unportedArgument<T>()
        self._member_name_cache = {}
        self.extra_std = set()          # std headers a generated member block expects before it
        self._collect_scope_names()

    def _collect_scope_names(self):
        names = self.ctx.scope_names
        for td in self.cu.all_types():
            names.add(cpp_ident(td.name))
            for f in td.fields:
                names.add(cpp_ident(f.name))
            for m in td.methods:
                names.add(cpp_ident(m.name))
            for c in td.enum_constants:
                names.add(cpp_ident(c.name))
            for p in td.type_params:
                names.add(cpp_ident(p.name))
            for rc in td.record_components:
                names.add(cpp_ident(rc.name))
            lay = self.project.layout(td)
            if lay:
                names.update(ml.cpp_name for ml in lay.members.values())
        # nested type names are always spelled Outer::Inner, but top-level names of this file must stay visible
        for td in self.cu.types:
            names.discard(cpp_ident(td.name))

    # -- entry
    def emit(self):
        ns = self.ctx.ns
        body = []
        tops = self._ordered_tops()
        for td in tops:
            body += self.emit_type(td, '', template_ctx=False)
            body.append('')
        header_lines = [f'{DRAFT_MARK} from {self.project.source_label(self.cu)}.',
                        '// Hand-owned after review (handlers-and-porting-plan.md §2.5); regenerating overwrites hand edits.']
        for td in self.cu.types:
            owner = self.project.generator_owner(td)
            if owner is not None and owner[0] == 'xmlgen' and owner[1] != 'enum':
                header_lines.append(f'// TODO(xmlgen): {td.name} is a static data {owner[1]} class (staticdata-classes.json): this dependency '
                                    f'draft stands in until `python cpp/tools/xmlgen/xmlgen.py scaffold {td.fqn}` writes the real header.')
        header_lines += ['#pragma once', '']
        std = sorted(set(self._std_includes('\n'.join(body))) | self.extra_std)
        if std:
            header_lines += [f'#include <{h}>' for h in std] + ['']
        own_fwd = f'{self.project.unit_dir(self.cu)}/fwd.h'
        includes = set(self.ctx.includes)
        if self.inline_stubs:
            includes |= self.ctx.value_headers     # inline stub bodies (templates) return those types by value
        includes.add(own_fwd)
        includes.discard(self.project.unit_header(self.cu))
        runtime_first = sorted(includes, key=lambda h: (not h.startswith('aion/gameserver/runtime/'), h))
        header_lines += [f'#include "{h}"' for h in runtime_first] + ['']
        header_lines.append(f'namespace {"::".join(ns)} {{')
        header_lines.append('')
        header_lines += body
        header_lines.append(f'}} // namespace {"::".join(ns)}')
        header = '\n'.join(header_lines) + '\n'

        cpp_lines = [f'{DRAFT_MARK} from {self.project.source_label(self.cu)}.', '']
        cpp_lines.append(f'#include "{self.project.unit_header(self.cu)}"')
        stub_bodies = [lines for is_tmpl, lines in self.stubs if not is_tmpl]
        inc = set(self.cpp_includes) | (self.ctx.value_headers - self.ctx.includes)
        if any('AION_UNPORTED' in line for lines in stub_bodies for line in lines):
            inc.add(self.unported_header)
        if inc:
            cpp_lines.append('')
            cpp_lines += [f'#include "{h}"' for h in sorted(inc, key=lambda h: (not h.startswith('aion/gameserver/runtime/'), h))]
        if self.cpp_statics or stub_bodies:
            cpp_lines.append('')
            cpp_lines.append(f'namespace {"::".join(ns)} {{')
            cpp_lines.append('')
            if self.cpp_statics:
                cpp_lines += self.cpp_statics
                cpp_lines.append('')
            for lines in stub_bodies:
                cpp_lines += lines
                cpp_lines.append('')
            cpp_lines.append(f'}} // namespace {"::".join(ns)}')
        cpp = '\n'.join(cpp_lines) + '\n'
        return header, cpp

    @staticmethod
    def _std_includes(text):
        out = set()
        for token, header in STD_INCLUDES:
            if token in text:
                out.add(header)
        if re.search(r'std::string\b(?!_view)', text):
            out.add('string')
        if re.search(r'\b(?:u?int(?:8|16|32|64)_t)\b', text):
            out.add('cstdint')
        return sorted(out)

    def _ordered_tops(self):
        return self._ordered(list(self.cu.types))

    def _ordered(self, types):
        """Source order, except that a type is emitted after the sibling types it extends or uses as base type arguments."""
        ids = {id(t) for t in types}
        ordered, placed = [], set()
        index = self.project.index

        def deps(td):
            out = [s for s in index.supertypes(td) if id(s) in ids]
            for ref in list(td.extends) + list(td.implements):
                for arg in ref.args or []:
                    if arg.wildcard is None:
                        kind, value = index.resolve_kind(arg.name, td)
                        if kind == 'project' and id(index.types[value]) in ids:
                            out.append(index.types[value])
            return out

        def place(td, stack=()):
            if id(td) in placed:
                return
            if id(td) in stack:
                raise SkeletonError(f'{self.cu.path}: cyclic base dependency among {td.fqn}')
            for dep in deps(td):
                if dep is not td:
                    place(dep, stack + (id(td),))
            placed.add(id(td))
            ordered.append(td)

        for td in types:
            place(td)
        return ordered

    # -- types
    def emit_type(self, td, indent, template_ctx):
        if td.kind == 'annotation':
            return [f'{indent}// Java annotation type @{td.name}: no C++ type (markers and registries replace it)']
        if td.kind == 'enum':
            generated = self.project.generated_enum(td)
            if generated is not None:
                return self.emit_generated_enum(td, indent, generated)
            return self.emit_enum(td, indent)
        return self.emit_class(td, indent, template_ctx)

    def emit_generated_enum(self, td, indent, generated):
        """An enum xmlgen generates: its header is included; a nested one becomes an alias of the namespace-scope Outer_Inner enum (a
        second definition would be a distinct type, and `xmlgen.py generate` refuses it)."""
        path, header = generated
        self.ctx.includes.add(header)
        name = cpp_ident(td.name)
        if td.outer is None:
            return [f'{indent}// {name}: an enum generated by xmlgen ({header})']
        return [f'{indent}using {name} = ::{"::".join(path)}; // generated by xmlgen']

    def emit_enum(self, td, indent):
        lines = doc_block(td.doc, indent)
        name = cpp_ident(td.name)
        existing = self.project.cpp.lookup(self.ctx.ns, name) if td.outer is None else None
        underlying = _std_int(existing.underlying) if existing is not None and existing.key == 'enum' else enum_underlying(td)
        base = f' : {underlying}' if underlying else ''
        lines.append(f'{indent}enum class {name}{base} {{')
        extras = []
        if any(c.args for c in td.enum_constants):
            extras.append('constructor arguments')
        if any(c.body is not None for c in td.enum_constants):
            extras.append('constant bodies')
        if td.fields:
            extras.append('fields')
        if td.methods:
            extras.append('methods ' + ', '.join(sorted({m.name for m in td.methods if m.kind == 'method'})))
        if td.types:
            extras.append('nested types ' + ', '.join(t.name for t in td.types))
        for c in td.enum_constants:
            lines.append(f'{indent}\t{cpp_ident(c.name)},')
        lines.append(f'{indent}}};')
        if extras:
            lines.append(f'{indent}// TODO(enum): {name} has {"; ".join(extras)}: companion free functions (static-data.md §2.5)')
        return lines

    def emit_class(self, td, indent, template_ctx, qualified=False):
        p = self.project
        lines = doc_block(td.doc, indent)
        name = cpp_ident(td.name)
        is_interface = td.kind == 'interface'
        in_template = template_ctx or bool(p.cpp_type_params(td))
        lay = p.layout(td)
        base_kind = p.base_kind(td)
        refcounted = base_kind == 'RefCounted'
        cls_path = p.type_path(td)
        qual = '::'.join(cls_path)

        # declaration head
        head = ''
        if p.cpp_type_params(td):
            head = 'template <' + ', '.join(f'class {cpp_ident(tp.name)}' for tp in td.type_params) + '>'
            lines.append(f'{indent}{head}')
        elif td.type_params:
            lines.append(f'{indent}// Java generic {td.name}<{", ".join(tp.name for tp in td.type_params)}>: a non-template C++ class '
                         f'({p.erasure_note(td)}); type variables are spelled as their bounds')
        key = 'class'
        if td.outer is None:
            existing = p.cpp.declaration(self.ctx.ns, name)
            if existing is not None and existing.key in ('class', 'struct'):
                key = existing.key
        bases, base_notes = [], []
        superclass = None
        sup = p.superclass(td)
        if lay is not None and lay.base and td.kind in ('class', 'record') and (sup is None or p.base_kind(sup) != lay.base):
            bases.append('public ' + self.ctx.runtime(lay.base))
        refs = [('extends', r) for r in td.extends] + [('implements', r) for r in td.implements]
        project_supers = []
        for how, r in refs:
            kind, value = p.index.resolve_kind(r.name, td)
            if kind == 'project':
                project_supers.append(p.index.types[value])
        inherited = {id(s) for sup in project_supers for s in p.all_supertypes(sup)}
        for how, r in refs:
            kind, value = p.index.resolve_kind(r.name, td)
            if kind == 'project':
                sup = p.index.types[value]
                if td.kind == 'enum':
                    base_notes.append(f'{how} {r}')
                    continue
                if id(sup) in inherited:
                    base_notes.append(f'{how} {r} (already inherited through another base)')
                    continue
                try:
                    spelled = self.ctx.project_type(sup, as_base=True)
                    if p.cpp_type_params(sup):
                        args = self.ctx._args(r, td, len(sup.type_params))
                        self.ctx.capture = []
                        try:
                            spelled += '<' + ', '.join(self.ctx.map_type(a, td, 'typearg') for a in args) + '>'
                        finally:
                            arg_types, self.ctx.capture = self.ctx.capture, None
                        for t in arg_types:
                            # the base template is instantiated here, and MSVC defines its implicit virtual destructor (Ref<T>, SelfOrRef<T>
                            # members) with it: the type arguments must be complete in this header
                            top = p.top_level(t)
                            if id(top) not in self.ctx.unit_types:
                                self.ctx.includes.add(p.unit_header(top.cu))
                    elif r.args and not sup.type_params:
                        raise Todo(f'type arguments on {sup.fqn}')
                    bases.append(f'public {spelled}')
                    if sup.kind == 'class':
                        superclass = sup
                except Todo as e:
                    base_notes.append(f'{how} {r} (TODO: {e})')
            else:
                base_notes.append(f'{how} {r}')
        if superclass is not None:
            own = p.unit_header(self.cu)
            needed = {h for h in p.member_headers(superclass) if h != own}
            (self.ctx.includes if in_template else self.ctx.value_headers).update(needed)
        final = ' final' if 'final' in td.modifiers and td.kind == 'class' else ''
        base_text = (' : ' + ', '.join(bases)) if bases else ''
        for note in base_notes:
            lines.append(f'{indent}// Java {note}')
        declared = '::'.join(p.type_path(td)[-2:]) if qualified else name
        lines.append(f'{indent}{key} {declared}{final}{base_text} {{')
        inner = indent + '\t'
        section = []
        access_state = [None]

        def label(access):
            if access_state[0] != access:
                section.append(f'{indent}{access}:')
                access_state[0] = access

        if refcounted and td.kind in ('class', 'record'):
            section.append(f'{inner}AION_MAKE_REF_FRIEND')
            self.ctx.includes.add(p.cpp.symbols.get('AION_MAKE_REF_FRIEND') or RUNTIME_FALLBACK_SYMBOLS['AION_MAKE_REF_FRIEND'])

        singleton = is_singleton(td)
        # nested types: forward declarations (same access as the definition), then definitions with bases first
        nested = [nt for nt in td.types if not (singleton and nt.name == 'SingletonHolder') and nt.kind != 'annotation']
        aliased = set()     # generated nested enums: the alias replaces the forward declaration and the definition
        if len(nested) > 1:
            for nt in nested:
                label(java_access(nt.modifiers, is_interface))
                if p.generated_enum(nt) is not None:
                    section += self.emit_type(nt, inner, in_template)
                    aliased.add(id(nt))
                elif nt.kind == 'enum':
                    section.append(f'{inner}enum class {cpp_ident(nt.name)} : {enum_underlying(nt)};')
                elif p.cpp_type_params(nt):
                    tps = ', '.join(f'class {cpp_ident(tp.name)}' for tp in nt.type_params)
                    section.append(f'{inner}template <{tps}> class {cpp_ident(nt.name)};')
                else:
                    section.append(f'{inner}class {cpp_ident(nt.name)};')
        deferred = []
        for nt in self._ordered(list(td.types)):
            if (singleton and nt.name == 'SingletonHolder') or id(nt) in aliased:
                continue
            if nt.kind == 'class' and not in_template and self._extends_enclosing(nt):
                # a nested class deriving from its outer class can only be defined after the outer class is complete
                if len(nested) <= 1:
                    label(java_access(nt.modifiers, is_interface))
                    section.append(f'{inner}class {cpp_ident(nt.name)};')
                deferred.append(nt)
                continue
            label(java_access(nt.modifiers, is_interface))
            section += self.emit_type(nt, inner, in_template)

        # VisibleObject itself: the two-phase construction of every visible object (hub-headers.md §10.1)
        if td.fqn == VISIBLE_OBJECT_FQN:
            make_ref, ref = self.ctx.runtime('makeRef'), self.ctx.runtime('Ref')
            self.extra_std.update(('concepts', 'utility'))
            label('protected')
            section += [f'{inner}/** Passkey of every VisibleObject constructor: only create<T> can make one, so postConstruct() can never be skipped. */',
                        f'{inner}struct CreateKey {{', f'{inner}private:', f'{inner}\tCreateKey() = default;', f'{inner}\tfriend class {name};',
                        f'{inner}}};',
                        f'{inner}/** C++ only: the Java constructor-body work that needs the dynamic type; overrides call the base version first. */',
                        f'{inner}virtual void postConstruct() {{}}']
            label('public')
            section += [f'{inner}/** Java `new T(args...)` of every visible object: constructs T and runs postConstruct(). */',
                        f'{inner}template <std::derived_from<{name}> T, class... Args>',
                        f'{inner}[[nodiscard]] static {ref}<T> create(Args&&... args) {{',
                        f'{inner}\t{ref}<T> object = {make_ref}<T>(CreateKey(), std::forward<Args>(args)...);',
                        f'{inner}\tstatic_cast<{name}&>(*object).postConstruct();', f'{inner}\treturn object;', f'{inner}}}']

        # members
        self.ctx.capture = []
        try:
            self.emit_members(td, lay, inner, label, is_interface, section)
        finally:
            member_types, self.ctx.capture = self.ctx.capture, None
        for t in member_types:
            # constructors, destructors (and create()) instantiate the member destructors (Ref<X>, shims): X must be complete there
            top = self.project.top_level(t)
            if id(top) not in self.ctx.unit_types:
                if in_template:
                    self.ctx.includes.add(self.project.unit_header(top.cu))
                else:
                    self.ctx.value_headers.add(self.project.unit_header(top.cu))

        # generated member block (sysmsg.py): included in a public section, its methods are not drafted
        block = self._member_block(td)
        if block is not None:
            gen, (_, std, contract) = block
            label('public')
            section.append(f'{inner}// Generated member block: {contract}.')
            section.append(f'#include "{gen}"')
            self.extra_std.update(std)

        # methods
        plans = self.plan_methods(td, in_template, refcounted, superclass, lay)
        for plan in plans:
            label(plan.access)
            doc = doc_block(plan.m.doc, inner, summary_only=True) if plan.m is not None else []
            section += doc
            if plan.todo is not None:
                section.append(f'{inner}// TODO(signature): {plan.todo}: {java_method_text(plan.m)}')
                continue
            if plan.note is not None:
                section.append(f'{inner}// {plan.note}')
                continue
            comment = f' // {plan.comment}' if plan.comment else ''
            if plan.pure:
                section.append(f'{inner}{plan.decl} = 0;{comment}')
            elif plan.inline_body is not None:
                section.append(f'{inner}{plan.def_decl or plan.decl} {{ {plan.inline_body} }}{comment}')
            elif in_template and plan.stub_head is not None:
                self.inline_stubs = True
                if 'unportedArgument<' in plan.mem_init:
                    section += ['#ifdef _MSC_VER', '#pragma warning(push)', '#pragma warning(disable : 4702) // the initializer never returns',
                                '#endif', f'{inner}{plan.def_decl or plan.decl}{plan.mem_init} {{}}{comment}', '#ifdef _MSC_VER',
                                '#pragma warning(pop)', '#endif']
                elif plan.stub_body == ['AION_UNPORTED();']:
                    self.ctx.includes.add(self.unported_header)
                    section.append(f'{inner}{plan.def_decl or plan.decl}{plan.mem_init} {{ AION_UNPORTED(); }}{comment}')
                else:
                    section.append(f'{inner}{plan.def_decl or plan.decl}{plan.mem_init} {{{comment}')
                    section += [f'{inner}\t{b}' for b in plan.stub_body]
                    section.append(f'{inner}}}')
            else:
                section.append(f'{inner}{plan.decl};{comment}')
                if plan.stub_head is not None:
                    pre, post = [], []
                    if 'unportedArgument<' in plan.mem_init:
                        pre = ['#ifdef _MSC_VER', '#pragma warning(push)', '#pragma warning(disable : 4702) // the base initializer never returns',
                               '#endif']
                        post = ['#ifdef _MSC_VER', '#pragma warning(pop)', '#endif']
                    self.stubs.append((False, pre + [f'{plan.stub_head} {{'] + [f'\t{b}' for b in plan.stub_body] + ['}'] + post))

        if id(td) in self.needs_helper:
            label('private')
            self.ctx.includes.add(self.unported_header)
            section.append(f'{inner}/** Constructor stubs bind arguments and reference members to this: AION_UNPORTED() throws first. */')
            section.append(f'{inner}template <class U>')
            section.append(f'{inner}[[noreturn]] static U& unportedArgument() {{ AION_UNPORTED(); }}')

        # Ref<I> of an interface: pure virtual retain/release, forwarded by the first implementor with a runtime base (hub-headers.md §9.2)
        if is_interface and td.fqn in p.retainable_interfaces():
            label('public')
            section.append(f'{inner}/** C++ only: Ref<{name}> retains the implementing object (hub-headers.md §9.2). */')
            section.append(f'{inner}virtual void retain() const noexcept = 0;')
            section.append(f'{inner}virtual void release() const noexcept = 0;')
        elif p.retainable_implemented(td):
            base = self.ctx.runtime(p.base_kind(td))
            label('public')
            section.append(f'{inner}/** C++ only: {", ".join(f.rsplit(".", 1)[-1] for f in p.retainable_implemented(td))} retain the object itself. */')
            section.append(f'{inner}void retain() const noexcept override {{ {base}::retain(); }}')
            section.append(f'{inner}void release() const noexcept override {{ {base}::release(); }}')

        # destructor
        dtor = self._destructor(td, plans, superclass, refcounted, lay)
        if dtor:
            access, decl, body = dtor
            label(access)
            if in_template or body is not None:
                section.append(f'{inner}{decl}{body if body is not None else " = default;"}')
            else:
                section.append(f'{inner}{decl};')
                self.stubs.append((False, [f'{qual}::~{name}() = default;']))
        lines += section
        lines.append(f'{indent}}};')
        for nt in deferred:
            lines.append('')
            lines += self.emit_class(nt, indent, template_ctx, qualified=True)
        return lines

    def _member_block(self, td):
        """(gen.h include path, MEMBER_BLOCKS entry) of a top-level class whose methods are partly generated as a member block, else None."""
        if td.outer is not None:
            return None
        owner = self.project.generator_owner(td)
        if owner is None or owner[0] != 'member block':
            return None
        entry = MEMBER_BLOCKS.get(td.name)
        if entry is None:
            raise SkeletonError(f'{td.fqn}: generated member block {owner[1]} without a MEMBER_BLOCKS entry in skeleton.py')
        return owner[1], entry

    def _extends_enclosing(self, nt):
        enclosing = {id(o) for o in _outers(nt)}
        return any(id(s) in enclosing for s in self.project.all_supertypes(nt))

    def _destructor(self, td, plans, superclass, refcounted, lay):
        name = cpp_ident(td.name)
        if td.kind == 'interface':
            return 'public', f'virtual ~{name}()', ' = default;'
        if refcounted:
            return 'protected', f'~{name}() override', None
        has_virtual = any(pl.decl and ('virtual ' in pl.decl or pl.pure) for pl in plans)
        has_project_base = superclass is not None or any(
            self.project.index.resolve_kind(r.name, td)[0] == 'project' for r in td.implements)
        if has_virtual and not has_project_base:
            return 'public', f'virtual ~{name}()', None
        return None

    # -- members
    def emit_members(self, td, lay, inner, label, is_interface, out):
        """Appends the member block to out (the class section that label() also appends to).

        Records the members actually declared (self.emitted[id(td)], used for inline accessors) and reference members that constructor
        stubs must bind (self.reference_members[id(td)])."""
        todo = []            # (line, Java text, reason or None)
        emitted = self.emitted.setdefault(id(td), {})
        file = self.cu.relpath.rsplit('/', 1)[-1]
        if lay is not None:
            self.ctx.includes.update(lay.includes)
            if lay.extra:
                # generated callback structs belong to the body port (chunk agents copy them, runtime-architecture.md §3.5): kept as a
                # comment so the draft compiles before their bases and captured types exist
                label('private')
                out.append(f'{inner}// TODO(callbacks): generated declarations from fieldmap.json, enable them with the bodies that use them:')
                for block in lay.extra:
                    out += [f'{inner}// {ln}'.rstrip() for ln in block.splitlines()]
        java_names = {f.name for f in td.fields} | {rc.name for rc in td.record_components}
        for rc in td.record_components:
            member = lay.members.get(rc.name) if lay is not None else None
            text = f'record component {one_line(str(rc.type))} {rc.name}'
            line, reason = self._member_line(member, td, None) if member is not None else (None, None)
            if line is None:
                todo.append((rc.line, text, reason))
            else:
                label(member.access or 'private')
                out.append(inner + line)
                emitted[rc.name] = member
        for f in td.fields:
            if f.name == 'serialVersionUID':
                continue
            access = 'public' if is_interface else java_access(f.modifiers, nested=td.outer is not None)
            lname = logger_name(f, self.project.index)
            if lname is not None:
                self._logger(f, lname)
                continue
            member = lay.members.get(f.name) if lay is not None else None
            if member is not None and member.rule and member.rule.startswith('singleton instance'):
                continue
            if member is not None:
                line, reason = self._member_line(member, td, f)
                if line is not None:
                    label(member.access or access)
                    out.append(inner + line)
                    emitted[f.name] = member
                    continue
                todo.append((f.line, java_field_text(f), reason))
                continue
            if ('static' in f.modifiers and 'final' in f.modifiers) or is_interface:
                try:
                    init = constant_initializer(f)
                except Todo:
                    init = None
                if init is not None:
                    label(access)
                    if f.type.name in ('String', 'java.lang.String'):
                        out.append(f'{inner}static inline const std::string {cpp_ident(f.name)} = {init};')
                    else:
                        out.append(f'{inner}static constexpr {PRIMITIVE_TYPES[f.type.name]} {cpp_ident(f.name)} = {init};')
                    continue
            todo.append((f.line, java_field_text(f), None if lay is None else 'no fieldmap.json member'))
        if lay is not None:
            for name, ml in lay.members.items():
                if name not in java_names:
                    line, reason = self._member_line(ml, td, None)
                    if line is None:
                        todo.append((0, f'fieldmap member {name}', reason))
                    else:
                        label(ml.access or 'private')
                        out.append(inner + line)
                        emitted[name] = ml
        if todo or td.initializers:
            label('private')
            if lay is None:
                out.append(f'{inner}// TODO(fieldmap): no fieldmap.json entry for {td.name}; declare these Java fields with the member types printed by')
                out.append(f'{inner}// python cpp/tools/gen/fieldmap.py --class {td.fqn}   (runtime-architecture.md §3.2)')
            else:
                out.append(f'{inner}// TODO(fieldmap): Java fields of {td.name} without a usable fieldmap.json member:')
            for line, text, reason in todo:
                out.append(f'{inner}//   {file}:{line}  {text}' + (f'  [{reason}]' if reason else ''))
            for ini in td.initializers:
                out.append(f'{inner}//   {file}:{ini.line}  {"static " if ini.static else ""}initializer block')
        if lay is not None and lay.callbacks and not lay.extra:
            label('private')
            out.append(f'{inner}// TODO(callbacks): generated callback structs of {td.name} (python cpp/tools/gen/fieldmap.py --class <key>):')
            out += [f'{inner}//   {key}' for key in lay.callbacks]

    _CPP_NAME = re.compile(r'(?<![\w:])((?:::)?[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)')
    _CPP_PLAIN = CPP_KEYWORDS | frozenset('int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t size_t'.split())

    def _member_line(self, ml, td, f):
        """(C++ member declaration, None) or (None, reason) for a fieldmap member."""
        if ml.declaration:
            text = ml.declaration.strip()
            if not text.endswith((';', '}')):
                text += ';'
            if re.search(r'\w\s*\([^)]*\)\s*(?:const\s*)?;$', text) and '=' not in text:
                return None, f'fieldmap declares a function: {text}'
            spelled, unresolved = self.spell_cpp(text, td)
            if unresolved:
                return None, f'fieldmap declaration names {", ".join(unresolved)}'
            return spelled + (f' // {ml.comment}' if ml.comment else ''), None
        if ml.cpp_type is None:
            return None, f'fieldmap: {ml.rule}'
        t = ' '.join(ml.cpp_type.split())
        if (ml.rule and ml.rule.startswith('logger')) or re.search(r'(?:^|\s)Logger$', t):
            return None, f'fieldmap: {ml.rule or "logger"} (CONVENTIONS logging: a static logger in the .cpp)'
        if '/*' in t or re.search(r'[?@$]', t):
            return None, f'fieldmap type {t}'
        init = None
        if ml.initializer is not None:
            init = ml.initializer.strip()
            init = init if init.startswith('{') else '{' + init + '}'
        words = t.split(' ')
        static = ml.static
        while words and words[0] in ('static', 'inline'):
            static |= words.pop(0) == 'static'
        if static:
            # in-class definitions of static data members: constexpr (implicitly inline) or inline
            t = ' '.join(['static'] + (words if words[:1] == ['constexpr'] else ['inline'] + words))
        else:
            t = ' '.join(words)
        if t.startswith('static constexpr'):
            # the constant value comes from the Java literal (fieldmap.py may pass the Java spelling, e.g. "2f")
            try:
                init = ' = ' + constant_initializer(f) if f is not None else None
            except Todo:
                init = None
            if init is None:
                return None, f'fieldmap: {t}; no literal Java initializer to translate'
        elif init is not None and re.fullmatch(r'\{-?(?:\d[\d_]*\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFdDlL]\}', init):
            return None, f'fieldmap initializer {ml.initializer} is Java syntax'
        outer_capture, self.ctx.capture = self.ctx.capture, []
        try:
            spelled, unresolved = self.spell_cpp(t, td)
        finally:
            used, self.ctx.capture = self.ctx.capture, outer_capture
        if unresolved:
            return None, f'fieldmap type {t} names {", ".join(unresolved)}'
        if t.startswith('static'):
            # static inline members are defined (and destroyed) in every TU that includes the header: complete types there
            for u in used:
                top = self.project.top_level(u)
                if id(top) not in self.ctx.unit_types:
                    self.ctx.includes.add(self.project.unit_header(top.cu))
        elif outer_capture is not None:
            outer_capture.extend(used)
        is_reference = spelled.endswith('&') or 'OwnerRef<' in spelled
        if init is None:
            if is_reference:
                init = ''
            elif re.match(r'(?:const\s+)?(?:runtime::)?(?:PartSlot|PartMap|PartList|SelfOrRef)<', spelled):
                # parts know their owner (Parts.h constructors); SelfOrRef has RefCounted and OwnedPart overloads
                base = self.project.base_kind(td)
                if 'SelfOrRef<' in spelled and base in ('RefCounted', 'OwnedPart'):
                    init = f'{{static_cast<const {self.ctx.runtime(base)}&>(*this)}}'
                else:
                    init = '{*this}'
            elif re.search(r'(?:^|[\s:])Semaphore$', spelled):
                # no default constructor: the permits come from the Java `new Semaphore(n[, fair])`
                java = ' '.join(f.initializer.texts()) if f is not None and f.initializer is not None else ''
                sm = re.fullmatch(r'new Semaphore \( (\d+) (?:, (true|false) )?\)', java)
                if sm is None:
                    return None, f'fieldmap type {t}: no literal Java `new Semaphore(n)` initializer'
                init = '{' + sm.group(1) + (', ' + sm.group(2) if sm.group(2) else '') + '}'
            else:
                init = '{}'
        init = self._lock_class_initializer(td, ml, spelled, init)
        if is_reference:
            m = re.search(r'OwnerRef<(.*)>\s*$', spelled)
            target = m.group(1) if m else spelled.rstrip('&').strip()
            self.reference_members.setdefault(id(td), []).append((self.member_name(td, ml), target))
        text = f'{spelled} {self.member_name(td, ml)}{init};'
        notes = []
        if ml.comment:
            notes.append(ml.comment)
        if f is not None and f.initializer is not None and ml.initializer is None and not t.startswith('static constexpr'):
            notes.append('Java: = ' + one_line(f.initializer.text, 60))
        return text + (f' // {"; ".join(notes)}' if notes else ''), None

    _LOCKABLE_RE = re.compile(r'^(?:(?:static|inline|mutable|const)\s+)*(?:runtime::)?(Monitor|StampedLock|Semaphore|AtomicBoolean|AtomicInteger|'
                              r'AtomicLong|AtomicNumber|AtomicReference|AtomicLongArray|ArrayList|LinkedList|ArrayDeque|PriorityQueue|HashMap|'
                              r'LinkedHashMap|TreeMap|EnumMap|HashSet|LinkedHashSet|TreeSet|ConcurrentHashMap|ConcurrentKeySet|'
                              r'CopyOnWriteArrayList|CopyOnWriteArraySet|ConcurrentLinkedQueue|ConcurrentLinkedDeque)(?:<|$)')
    _VALUE_AFTER_LOCK_CLASS = frozenset(('Semaphore', 'AtomicBoolean', 'AtomicInteger', 'AtomicLong', 'AtomicNumber', 'AtomicReference',
                                         'AtomicLongArray'))

    @staticmethod
    def _lock_class_initializer(td, ml, spelled, init):
        """RR-16 (runtime-architecture.md §3.4, hub-headers.md §4): a member Monitor, StampedLock, Semaphore, collection shim or Atomic* is
        initialized with its static lock class `AION_LOCK_CLASS(DeclaringClass::field)` (Java class and field names; `#stripe` for the
        stripe monitors of ConcurrentHashMap/ConcurrentKeySet), followed by the Java literal arguments."""
        m = DraftEmitter._LOCKABLE_RE.match(spelled)
        if m is None or init is None:
            return init
        names, t = [], td
        while t is not None:
            names.append(t.name)
            t = t.outer
        stripe = '#stripe' if m.group(1) in ('ConcurrentHashMap', 'ConcurrentKeySet') else ''
        tag = f'AION_LOCK_CLASS({"::".join(reversed(names))}::{ml.java_name}{stripe})'
        if init == '{}':
            return '{' + tag + '}'
        if init.startswith('{') and init.endswith('}') and m.group(1) in DraftEmitter._VALUE_AFTER_LOCK_CLASS:
            return '{' + tag + ', ' + init[1:]
        return init

    def member_name(self, td, ml):
        """C++ name of a fieldmap member: Java allows a field and a method with the same name (records always have them), C++ does
        not, so such members get a trailing underscore; so do members named like a member function of the runtime base (release)."""
        name = ml.cpp_name
        methods = {cpp_ident(m.name) for m in td.methods if m.kind == 'method'}
        if td.kind == 'record':
            methods |= {cpp_ident(rc.name) for rc in td.record_components}
        if self.project.base_kind(td):
            methods |= RUNTIME_BASE_METHODS
        return name + '_' if name in methods else name

    def spell_cpp(self, text, td):
        """Qualifies the names in a fieldmap.py C++ type or declaration from td's scope: runtime symbols -> runtime::X, Java types -> their
        C++ path (records includes), commons and existing C++ types by unique simple name. Returns (text, unresolved names)."""
        unresolved = []
        p = self.project
        type_params = {tp.name for t in [td] + list(_outers(td)) for tp in p.cpp_type_params(t)}

        def repl(m):
            chain = m.group(1)
            if chain.startswith('::'):
                return chain
            parts = chain.split('::')
            first = parts[0]
            if first in self._CPP_PLAIN or first == 'std' or first in type_params:
                return chain
            if first == 'runtime' and len(parts) > 1:
                path = p.cpp.symbols.get(parts[1]) or RUNTIME_FALLBACK_SYMBOLS.get(parts[1])
                if path is not None:
                    self.ctx.includes.add(path)
                return chain
            if first in ('commons', 'aion') or (CORE_NAMESPACE + (first,)) in p.known_paths:
                return chain
            path = p.cpp.symbols.get(first) or RUNTIME_FALLBACK_SYMBOLS.get(first)
            if path is not None and path.startswith('aion/gameserver/runtime/'):
                self.ctx.includes.add(path)
                return self.ctx.qualify(RUNTIME_NAMESPACE + tuple(parts))
            kind, value = p.index.resolve_kind(first, td)
            if kind == 'typevar' and len(parts) == 1:
                # a type variable of an erased generic (hub-headers.md §8.1): its bound (ERASURE_BOUNDS for unbounded ones)
                for t in [td] + list(_outers(td)):
                    tp = next((x for x in t.type_params if x.name == first), None)
                    if tp is None:
                        continue
                    if p.erased_generic(t):
                        fqn = ERASURE_BOUNDS.get(t.fqn, {}).get(first)
                        if fqn is None and tp.bounds:
                            bound_kind, bound = p.index.resolve_kind(tp.bounds[0].name, t)
                            fqn = bound if bound_kind == 'project' else None
                        if fqn is not None and fqn in p.index.types:
                            return self.ctx.project_type(p.index.types[fqn])
                    break
            if kind == 'project':
                t = p.index.types[value]
                rest = []
                for seg in parts[1:]:
                    nested = p.index.member_type(t, seg) if not rest else None
                    if nested is not None:
                        if t.kind in ('enum', 'annotation'):
                            unresolved.append(f'{chain} (type nested in an enum)')
                            return chain
                        t = nested
                    elif t.kind == 'enum' and not rest and any(c.name == seg for c in t.enum_constants):
                        rest.append(cpp_ident(seg))
                    else:
                        unresolved.append(chain)
                        return chain
                if p.cpp_type_params(t) and not rest and not text[m.end():].lstrip().startswith('<'):
                    unresolved.append(chain + ' (raw generic type)')
                    return chain
                try:
                    spelled = '::'.join([self.ctx.project_type(t)] + rest)
                except Todo:
                    unresolved.append(chain)
                    return chain
                if (not rest and _template_argument_of(text, m.start()) == 'Ref' and p.base_kind(t) not in ('RefCounted', 'OwnedPart')
                        and t.fqn not in p.retainable_interfaces()):
                    unresolved.append(f'{chain} (Ref<> of a class without a RefCounted/OwnedPart base in fieldmap.json)')
                    return chain
                if not rest and t.kind != 'enum' and _held_by_value(text, m.start(), m.end()):
                    top = p.top_level(t)
                    if id(top) not in self.ctx.unit_types:
                        self.ctx.includes.add(p.unit_header(top.cu))   # a by-value member needs the complete type in the header
                return spelled
            known = COMMONS_TYPES.get(first)
            if known is not None and len(parts) == 1:
                self.ctx.includes.add(known[0])
                return self.ctx.qualify(known[1])
            decl = p.cpp_by_name(first)
            if decl is not None and len(parts) == 1:
                ns, d = decl
                if d.key != 'enum' and _template_argument_of(text, m.start()) == 'Field':
                    unresolved.append(f'{chain} (Field<> of the C++ class {d.path}: not trivially copyable)')
                    return chain
                if d.key != 'enum' and _held_by_value(text, m.start(), m.end()):
                    unresolved.append(f'{chain} (the C++ class {d.path} held by value; the Java field is a nullable reference)')
                    return chain
                self.ctx.includes.add(d.path)
                return self.ctx.qualify(ns + (first,))
            unresolved.append(chain)
            return chain

        text = self._strip_erased_arguments(text, td)
        out = self._CPP_NAME.sub(repl, text)
        return out, unresolved

    def _strip_erased_arguments(self, text, td):
        """Drops the template argument list after a Java generic that C++ erases (`DamageInfo<Creature>*` -> `DamageInfo*`, hub-headers.md §8.1)."""
        p = self.project
        out, pos = [], 0
        for m in self._CPP_NAME.finditer(text):
            if m.start() < pos:
                continue
            chain = m.group(1)
            rest = text[m.end():]
            if not rest.startswith('<') or chain.startswith('::') or '::' in chain:
                continue
            kind, value = p.index.resolve_kind(chain, td)
            if kind != 'project' or not p.erased_generic(p.index.types[value]):
                continue
            depth, end = 0, m.end()
            while end < len(text):
                if text[end] == '<':
                    depth += 1
                elif text[end] == '>':
                    depth -= 1
                    if depth == 0:
                        break
                end += 1
            out.append(text[pos:m.end()])
            pos = end + 1
        out.append(text[pos:])
        return ''.join(out)

    def _logger(self, f, lname):
        self.cpp_includes.add(LOGGER_HEADER)
        var = cpp_ident(f.name)
        line = f'static const auto {var} = {self.ctx.qualify(("aion", "commons", "logging", "LoggerFactory"))}::getLogger({_cpp_string_literal(lname)});'
        self.cpp_statics.append(line if var not in self.logger_names_used else f'// TODO(logger): second logger {var} for {lname}')
        self.logger_names_used.add(var)

    # -- methods
    def plan_methods(self, td, in_template, refcounted, superclass, lay, check_create=True):
        p = self.project
        name = cpp_ident(td.name)
        qual = '::'.join(p.type_path(td))
        is_interface = td.kind == 'interface'
        plans = []
        seen = {}
        sub_methods = p.subtype_methods(td)
        members = self.emitted.get(id(td), {})
        field_names = {f.name: f for f in td.fields}
        block = self._member_block(td)
        for m in td.methods:
            if block is not None and block[1][0](td.cu, m):
                continue
            access = java_access(m.modifiers, is_interface, nested=td.outer is not None)
            plan = MethodPlan(m, access, kind=m.kind)
            try:
                if m.kind == 'compact_constructor':
                    raise Todo('compact record constructor')
                if m.type_params:
                    raise Todo('generic method')
                self._plan_signature(td, m, plan, name, qual, is_interface, sub_methods, refcounted, superclass, members, field_names,
                                     in_template)
            except Todo as e:
                plan.todo = str(e)
                plan.decl = None
            if plan.todo is None and plan.note is None:
                if plan.key in seen:
                    plan.todo = f'C++ signature collides with the declaration at line {seen[plan.key].line}'
                else:
                    seen[plan.key] = m
            plans.append(plan)
            if (plan.todo is None and m.kind == 'constructor' and refcounted and 'abstract' not in td.modifiers and check_create
                    and not p.is_visible_object(td)):      # visible objects: VisibleObject::create<T> (hub-headers.md §10.1)
                missing = self._cpp_pure_left(td)
                if missing:
                    plans.append(MethodPlan(m, 'public', todo=f'create(): {missing} stays pure virtual in C++ (its override is a TODO signature)'))
                else:
                    plans.append(self._create_plan(td, m, plan, name, qual, in_template))
        if td.kind == 'record':
            arity = len(td.record_components)
            if not any(mm.kind == 'constructor' and len(mm.params) == arity for mm in td.methods):
                plan = MethodPlan(None, 'public', kind='constructor')
                try:
                    plan.params = [(self.ctx.map_type(rc.type, td, 'param'), cpp_ident(rc.name)) for rc in td.record_components]
                    plan.def_params = self._definition_params(td, plan.params)
                    ptext = ', '.join(f'{t} {n}' for t, n in plan.params)
                    dtext = ', '.join(f'{t} {n}' for t, n in plan.def_params)
                    plan.decl = f'{"explicit " if arity == 1 else ""}{name}({ptext})'
                    plan.def_decl = f'{"explicit " if arity == 1 else ""}{name}({dtext})'
                    plan.key = (name, tuple(t for t, _ in plan.params), False)
                    plan.stub_head = f'{qual}::{name}({dtext})'
                    plan.comment = 'canonical record constructor'
                    if plan.key not in seen:
                        seen[plan.key] = td
                        plans.insert(0, plan)
                except Todo:
                    pass
            for rc in td.record_components:
                if any(mm.name == rc.name and not mm.params for mm in td.methods):
                    continue
                plan = MethodPlan(None, 'public', kind='method')
                try:
                    rt = self.ctx.map_type(rc.type, td, 'return')
                    plan.decl = f'{rt} {cpp_ident(rc.name)}() const'
                    plan.key = (cpp_ident(rc.name), (), True)
                    member = members.get(rc.name)
                    body = self._accessor_body('get', member, [], rt, self.member_name(td, member)) if member is not None else None
                    if body is not None:
                        plan.inline_body = body
                    else:
                        plan.stub_head = f'{rt} {qual}::{cpp_ident(rc.name)}() const'
                        plan.comment = 'record accessor'
                except Todo as e:
                    plan.todo = str(e)
                    plan.m = None
                if plan.todo is None:
                    plans.append(plan)
        return plans

    def _create_plan(self, td, m, ctor_plan, name, qual, in_template):
        params = ctor_plan.def_params
        plan = MethodPlan(None, 'public', kind='create')
        rt = self.ctx.runtime('Ref')
        mk = self.ctx.runtime('makeRef')
        ptext = ', '.join(f'{t} {n}' for t, n in params)
        plan.decl = f'static {rt}<{qual}> create({ptext})'
        body = f'return {mk}<{qual}>({", ".join(n for _, n in params)});'
        plan.key = ('create', tuple(t for t, _ in params), False)
        if in_template:
            plan.inline_body = body
        else:   # out of line: makeRef instantiates the member destructors, which need complete member types (included by the .cpp)
            plan.stub_head = f'{rt}<{qual}> {qual}::create({ptext})'
            plan.stub_body = [body]
        return plan

    def _plan_signature(self, td, m, plan, name, qual, is_interface, sub_methods, refcounted, superclass, members, field_names, in_template):
        ctx = self.ctx
        params = []
        self_ref = f'const {qual}&'
        if (m.kind == 'method' and m.name == 'afterUnmarshal' and len(m.params) == 2 and m.params[0].type.name.endswith('Unmarshaller')
                and 'static' not in m.modifiers):
            ctx.includes.add(XML_HOOK_HEADER)
            xml = ctx.qualify(CORE_NAMESPACE + ('xml',))
            plan.params = [(f'{xml}::LoadContext&', 'ctx'), (f'const {xml}::XmlParent&', cpp_ident(m.params[1].name))]
            plan.def_params = self._definition_params(td, plan.params)
            plan.decl = f'void afterUnmarshal({", ".join(f"{t} {n}" for t, n in plan.params)})'
            plan.def_decl = f'void afterUnmarshal({", ".join(f"{t} {n}" for t, n in plan.def_params)})'
            plan.key = ('afterUnmarshal', tuple(t for t, _ in plan.params), False)
            plan.stub_head = f'void {qual}::{plan.def_decl[len("void "):]}'
            plan.comment = 'JAXB hook (XmlBinding.h)'
            return
        for index, prm in enumerate(m.params):
            pname = cpp_ident(prm.name)
            if len(m.params) == 1 and m.name == 'equals' and prm.type.name in ('Object', 'java.lang.Object') and m.kind == 'method':
                params.append((self_ref, pname))
                continue
            if m.name == 'compareTo' and len(m.params) == 1 and m.kind == 'method':
                kind, value = self.project.index.resolve_kind(prm.type.name, m)
                if kind == 'project' and self.project.index.types[value] is td:
                    params.append((self_ref, pname))
                    continue
            params.append((self.param_type(td, m, index, prm), pname))
        visible_ctor = m.kind == 'constructor' and self.project.is_visible_object(td)
        if visible_ctor:
            params.insert(0, ('CreateKey', 'key'))        # hub-headers.md §10.1
        plan.params = params
        plan.def_params = self._definition_params(td, params)
        param_text = self._declaration_params(td, m, params)
        def_text = ', '.join(f'{t} {n}' for t, n in plan.def_params)
        static = 'static' in m.modifiers
        const = False
        if m.kind == 'constructor':
            if td.kind == 'enum' or is_interface:
                raise Todo('constructor of an enum/interface')
            explicit = 'explicit ' if len(params) == 1 else ''
            if refcounted:
                plan.access = 'protected'
            plan.decl = f'{explicit}{cpp_ident(td.name)}({param_text})'
            plan.def_decl = f'{explicit}{cpp_ident(td.name)}({def_text})'
            plan.key = (name, tuple(t for t, _ in params), False)
            plan.mem_init = self._base_init(td, superclass, in_template,
                                            key=visible_ctor and superclass is not None and self.project.is_visible_object(superclass))
            refs = self.reference_members.get(id(td), [])
            if refs:
                binds = ', '.join(f'{member}(unportedArgument<{target}>())' for member, target in refs)
                plan.mem_init = f'{plan.mem_init}, {binds}' if plan.mem_init else f' : {binds}'
                self.needs_helper.add(id(td))
            if 'unportedArgument<' in plan.mem_init:
                plan.stub_body = ['// unportedArgument() in the base initializer reports AION_UNPORTED (a body statement would be unreachable)']
            plan.stub_head = f'{qual}::{name}({def_text}){plan.mem_init}'
            if 'synchronized' in m.modifiers:
                plan.comment = 'synchronized'
            return
        mname = cpp_ident(m.name)
        special = (m.name, len(m.params))
        if not static and (special == ('hashCode', 0) or (
                m.name in ('equals', 'compareTo') and len(params) == 1 and params[0][0] == self_ref)):
            const = True
        if not static and special == ('toString', 0) and self.project.base_kind(td) not in ('RefCounted', 'OwnedPart', 'Immortal'):
            const = True        # toString() of runtime-based classes calls virtual getters: non-const (hub-headers.md §9.1)
        if special == ('hashCode', 0) and not static:
            ret = 'int32_t'
        elif special == ('toString', 0) and not static:
            ret = 'std::string'
        elif special == ('clone', 0) or special == ('finalize', 0):
            raise Todo(f'Java {m.name}()')
        elif static and m.name == 'getInstance' and not m.params and self._returns_self(td, m):
            ret = f'{qual}&'
        else:
            ret = self.return_type(td, m)
        accessor = self._trivial_accessor(td, m, field_names)
        if accessor is not None and accessor[0] == 'get' and not static:
            const = True
            shim = self._shim_member_type(td, accessor[1], members)
            if shim is not None:
                ret = f'{shim}&'      # the live Java collection (hub-headers.md §7.1)
                plan.inline_body = f'return this->{self.member_name(td, members[accessor[1]])};'
                const = False
        virtual = override = pure = False
        if not static and 'private' not in m.modifiers:
            overrides = self.project.overrides(td, m)
            abstract = 'abstract' in m.modifiers or (is_interface and m.body is None and 'default' not in m.modifiers)
            overridden = (m.name, len(m.params)) in sub_methods and self.project.overridden_in_subtypes(td, m)
            if overrides:
                override = True
                base_m = self.project.overridden_method(td, m)
                try:
                    base_ret = self._substitute(self.return_type(base_m.owner, base_m), self._type_arg_map(td, base_m.owner))
                except Todo:
                    base_ret = None
                bridged = self._bridge_parameters(td, m, base_m, params)
                if bridged != params:
                    # javac bridge methods: an override of a method with a type-variable parameter of an erased generic takes the erased
                    # parameter type (hub-headers.md §8.2)
                    params = bridged
                    plan.params = params
                    plan.def_params = self._definition_params(td, params)
                    param_text = self._declaration_params(td, m, params)
                    def_text = ', '.join(f'{t} {n}' for t, n in plan.def_params)
                cast_only = self.project.is_cast_override(m) and not self.project.method_is_virtual(base_m.owner, base_m)
                if cast_only and base_ret is not None and base_ret == ret:
                    plan.note = f'{java_method_text(m)}: cast-only override, the C++ type of {base_m.owner.name}::{mname} already fits (inherited)'
                    return
                if base_ret is not None and base_ret != ret and not base_m.type_params and not m.return_type.name == 'void':
                    if not cast_only:
                        raise Todo(f'covariant return type ({ret} overrides {base_ret} of {base_m.owner.name}; C++ smart pointers are not covariant)')
                    # hub-headers.md §8.2: a non-virtual redeclaration with the narrower type, defined as a cast of the base accessor
                    override = False
                    base_qual = '::'.join(ctx.project.type_path(base_m.owner))
                    base_spelled = ctx.qualify(ctx.project.unit_ns(base_m.owner.cu) + ctx.project.type_path(base_m.owner))
                    call = f'{base_spelled}::{mname}({", ".join(n for _, n in self._definition_params(td, params))})'
                    if ret.endswith('&'):
                        cast = f'static_cast<{ret}>({call})'
                    elif ret.endswith('*'):
                        cast = f'static_cast<{ret}>({call})'
                    else:
                        mt = _PTR_TYPE_RE.match(ret)
                        cast = f'{mt.group(1)}cast<{mt.group(2)}>({call})' if mt else None
                    if cast is not None:
                        plan.stub_body = [f'return {cast};']
                        kind, value = ctx.project.index.resolve_kind(m.return_type.name, m)
                        if kind == 'project':
                            ctx._value_type(ctx.project.index.types[value])     # the cast needs the complete narrower type
                    plan.comment = f'narrows {base_qual}::{mname} (Java cast-only override)'
                    root = base_m
                    while self.project.is_cast_override(root) and self.project.overridden_method(root.owner, root, hand_written=False):
                        root = self.project.overridden_method(root.owner, root, hand_written=False)
                    const = self._trivial_accessor(root.owner, root, {f.name: f for f in root.owner.fields}) is not None
            elif abstract or is_interface or (overridden and 'final' not in m.modifiers):
                virtual = True
            pure = abstract
            if not overrides and special not in (('equals', 1), ('hashCode', 0), ('toString', 0), ('compareTo', 1)) and any(
                    a.simple_name == 'Override' for a in m.annotations):
                plan.comment = ('@Override: the hand-written C++ base does not declare it (yet)'
                                if self.project.overridden_method(td, m, hand_written=False) else '@Override of a Java library type')
        if const and (virtual or override) and special not in (('equals', 1), ('hashCode', 0), ('toString', 0), ('compareTo', 1)):
            const = False    # const must agree across the hierarchy: only the Object/Comparable methods are const everywhere
        if 'synchronized' in m.modifiers:
            plan.comment = ((plan.comment + '; ') if plan.comment else '') + 'synchronized'
        specifiers = (' override' if override else '') + (' final' if 'final' in m.modifiers and override else '')
        head = f'{"static " if static else ""}{"virtual " if virtual else ""}{ret} {mname}'
        plan.decl = f'{head}({param_text}){" const" if const else ""}{specifiers}'
        plan.def_decl = f'{head}({def_text}){" const" if const else ""}{specifiers}'
        plan.pure = pure
        plan.key = (mname, tuple(t for t, _ in params), const)
        if pure:
            return
        plan.stub_head = f'{ret} {qual}::{mname}({def_text}){" const" if const else ""}'
        if static and m.name == 'getInstance' and ret == f'{qual}&':
            if self._default_constructible(td):
                plan.stub_body = [f'static {name} instance; // Java SingletonHolder', 'return instance;']
            plan.comment = 'Java singleton'
        part = self.part_member_of_accessor(td, m) if accessor is not None else None
        if part is not None and members.get(part[1]) is not None:
            how, fname, mtype = part
            member_name = self.member_name(td, members[fname])
            if how == 'get' and ret.endswith('&'):
                if re.search(r'\bOwnerRef<', mtype):
                    plan.inline_body, plan.stub_head = f'return this->{member_name};', None
                elif re.search(r'\bFinal<', mtype):
                    plan.inline_body, plan.stub_head = f'return *this->{member_name}.get();', None
                else:   # PartSlot::operator* names typeid(X), unique_ptr needs nothing, but both stay out of line (hub-headers.md §3.3)
                    plan.stub_body = [f'return *this->{member_name};']
                    plan.comment = ((plan.comment + '; ') if plan.comment else '') + 'part accessor (hub-headers.md §3.3)'
                return
            if how == 'set' and plan.params and plan.params[0][0].endswith('&') and re.search(r'\bFinal<[^<>]*\*>', mtype):
                # late-bound controller owner (pattern 3): store and bind, before the owner is published (hub-headers.md §10.3)
                value = plan.def_params[0][1]
                plan.stub_body = [f'this->{member_name}.set(&{value});', f'bindOwner({value});']
                plan.comment = ((plan.comment + '; ') if plan.comment else '') + 'owner binding (hub-headers.md §10.3)'
                return
            if how == 'set' and plan.params and plan.params[0][0].startswith('std::unique_ptr<') and re.search(r'\bPartSlot<', mtype):
                plan.stub_body = [f'this->{member_name}.set(std::move({plan.def_params[0][1]}));']
                plan.comment = ((plan.comment + '; ') if plan.comment else '') + 'part accessor (hub-headers.md §3.3)'
                return
        if plan.inline_body is not None:
            plan.stub_head = None
            return
        if accessor is not None:
            how, fname = accessor
            member = members.get(fname)
            body = self._accessor_body(how, member, plan.def_params, ret, self.member_name(td, member)) if member is not None else None
            if body is not None:
                plan.inline_body = body
                plan.stub_head = None
            elif member is not None and self._template_pointer_getter(how, member, ret):
                plan.inline_body, plan.stub_head = f'return this->{self.member_name(td, member)};', None
            elif member is not None and how == 'set' and not static and plan.params and re.match(
                    r'(?:runtime::)?Field<(?:runtime::)?(?:Ref<|std::shared_ptr<)', member.cpp_type or '') and plan.params[0][0].startswith(
                    ('runtime::Ptr<', 'Ptr<', 'std::shared_ptr<')):
                # releases the previous value: out of line, ported (hub-headers.md §3.3)
                plan.stub_body = [f'this->{self.member_name(td, member)}.set({plan.def_params[0][1]});']
                plan.comment = ((plan.comment + '; ') if plan.comment else '') + 'trivial setter (out of line: it releases the previous value)'
            else:
                plan.comment = ((plan.comment + '; ') if plan.comment else '') + f'trivial accessor of {fname}: inline once the member exists'

    @staticmethod
    def _template_pointer_getter(how, member, ret):
        """A getter of a `const X*` member (templates, interned immortals) returning the same pointer type."""
        core = re.sub(r'^(?:static\s+|inline\s+)+', '', (member.cpp_type or '').strip())
        mt = re.fullmatch(r'const\s+([\w:]+)\s*\*', core)
        rt = re.fullmatch(r'const\s+([\w:]+)\s*\*', ret.strip())
        return how == 'get' and mt is not None and rt is not None and mt.group(1).rsplit('::', 1)[-1] == rt.group(1).rsplit('::', 1)[-1]

    def _bridge_parameters(self, td, m, base_m, params):
        """params with the positions whose Java type in base_m names a type variable of an erased generic replaced by base_m's C++ type."""
        variables = set()
        for t in [base_m.owner] + list(_outers(base_m.owner)):
            if self.project.erased_generic(t):
                variables.update(tp.name for tp in t.type_params)
        if not variables:
            return params
        out = list(params)
        for i, prm in enumerate(base_m.params):
            if i >= len(out) or not variables.intersection(re.findall(r'\w+', str(prm.type))):
                continue
            try:
                out[i] = (self.param_type(base_m.owner, base_m, i, prm), out[i][1])
            except Todo:
                pass
        return out

    @staticmethod
    def _declaration_params(td, m, params):
        """Declaration parameter list: a trailing varargs parameter gets `= {}` (Java calls it without varargs, hub-headers.md §7.4) unless
        an overload of the class with one parameter less would make that call ambiguous (Java picks the fixed-arity overload)."""
        texts = [f'{t} {n}' for t, n in params]
        if m.params and m.params[-1].varargs and texts:
            arity = len(m.params) - 1
            if not any(o is not m and o.kind == m.kind and o.name == m.name and len(o.params) == arity for o in td.methods):
                texts[-1] += ' = {}'
        return ', '.join(texts)

    def param_type(self, td, m, index, prm):
        """C++ parameter type: the mapped type, with object parameters as X& unless nullable (Ptr<X>) or a part (std::unique_ptr<X>)
        (hub-headers.md §5.1, §10.2)."""
        text = self.ctx.map_type(prm.type, m, 'param', varargs=prm.varargs)
        if prm.varargs or prm.type.dims:
            return text
        mt = _PTR_TYPE_RE.match(text)
        if mt is None:
            return text
        if self.project.part_parameter(td, m, index):
            kind, value = self.project.index.resolve_kind(prm.type.name, m)
            if kind == 'project':
                self.ctx._value_type(self.project.index.types[value])     # a by-value unique_ptr parameter is destroyed by the definition
            return f'std::unique_ptr<{mt.group(2)}>'
        if self.project.nullable_parameter(td, m, index):
            return text
        return f'{mt.group(2)}&'

    _SHIM_RE = re.compile(r'(?:runtime::)?(?:ArrayList|LinkedList|HashMap|HashSet|ConcurrentHashMap|CopyOnWriteArrayList|ArrayDeque|'
                          r'PriorityQueue|ConcurrentLinkedQueue|ConcurrentLinkedDeque|TreeMap|TreeSet|LinkedHashMap|LinkedHashSet|'
                          r'ConcurrentKeySet)<')

    def _shim_member_type(self, td, fname, members):
        """The qualified C++ type of a non-static collection shim member (a getter returns a reference to it), else None."""
        member = members.get(fname)
        if member is None or member.static or member.declaration or not member.cpp_type:
            return None
        core = re.sub(r'^(?:mutable\s+|const\s+)+', '', member.cpp_type.strip())
        if not self._SHIM_RE.match(core):
            return None
        spelled, unresolved = self.spell_cpp(core, td)
        return None if unresolved else spelled

    def part_member_of_accessor(self, owner, m):
        """(how, member name, member type text) if m is a trivial accessor of a part or owner member of owner, else None."""
        acc = self._trivial_accessor(owner, m, {f.name: f for f in owner.fields})
        if acc is None:
            return None
        lay = self.project.layout(owner)
        member = lay.members.get(acc[1]) if lay is not None else None
        if member is None:
            return None
        t = member.cpp_type or member.declaration or ''
        if re.search(r'\b(unique_ptr|PartSlot|OwnerRef)<|\bFinal<[^<>]*\*>', t):
            return acc[0], acc[1], t
        return None

    def return_type(self, owner, m):
        """C++ return type: parts, owners and late-bound controller owners are references (hub-headers.md §5, §10.2); a cast-only override
        of such an accessor too."""
        ret = self.ctx.map_type(m.return_type, m, 'return')
        mt = _PTR_TYPE_RE.match(ret)
        if mt is None or m.params:
            return ret
        target_owner, target = owner, m
        if self.project.is_cast_override(m):
            base = self.project.overridden_method(owner, m, hand_written=False)
            while base is not None and self.project.is_cast_override(base):
                base = self.project.overridden_method(base.owner, base, hand_written=False)
            if base is None:
                return ret
            target_owner, target = base.owner, base
        part = self.part_member_of_accessor(target_owner, target)
        if part is not None and part[0] == 'get':
            return f'{mt.group(2)}&'
        return ret

    def _member_names(self, td):
        key = id(td)
        names = self._member_name_cache.get(key)
        if names is None:
            names = set(RUNTIME_BASE_MEMBERS) if self.project.base_kind(td) else set()
            for t in [td] + self.project.all_supertypes(td):
                names.update(cpp_ident(f.name) for f in t.fields)
                names.update(cpp_ident(rc.name) for rc in t.record_components)
                lay = self.project.layout(t)
                if lay is not None:
                    names.update(ml.cpp_name for ml in lay.members.values())
            self._member_name_cache[key] = names
        return names

    def _definition_params(self, td, params):
        """Parameter names for definitions: a name that equals a data member becomes 'value' (or '<name>Value'), like the ported
        login server does, so definitions never hide members (MSVC C4458 at /W4). Declarations keep the Java names."""
        members = self._member_names(td)
        taken = {n for _, n in params}
        out = []
        for t, n in params:
            if n in members:
                for candidate in ('value', f'{n}Value', f'{n}Param'):
                    if candidate not in members and candidate not in taken:
                        taken.add(candidate)
                        n = candidate
                        break
            out.append((t, n))
        return out

    def _cpp_pure_left(self, td):
        """Name of an abstract supertype method whose Java implementation (td or a superclass) has a TODO signature in its draft, so the
        C++ class stays abstract and cannot be created; None if there is none."""
        p = self.project
        chain = [td]
        while chain[-1] is not None:
            chain.append(p.superclass(chain[-1]))
        chain = chain[:-1]
        for sup in p.all_supertypes(td):
            for am in sup.methods:
                is_abstract = am.kind == 'method' and 'static' not in am.modifiers and (
                    'abstract' in am.modifiers or (sup.kind == 'interface' and am.body is None and 'default' not in am.modifiers))
                if not is_abstract or (am.name, len(am.params)) in p.todo_signatures(sup):
                    continue
                for c in chain:
                    impl = next((m for m in c.methods if m.kind == 'method' and m.name == am.name and len(m.params) == len(am.params)
                                 and 'abstract' not in m.modifiers), None)
                    if impl is not None:
                        if (am.name, len(am.params)) in p.todo_signatures(c):
                            return f'{sup.name}.{am.name}'
                        break
        return None

    def _returns_self(self, td, m):
        kind, value = self.project.index.resolve_kind(m.return_type.name, m)
        return kind == 'project' and self.project.index.types[value] is td

    def _default_constructible(self, td):
        if 'abstract' in td.modifiers or self.project.base_kind(td) == 'RefCounted':
            return False
        ctors = [m for m in td.methods if m.kind == 'constructor']
        return not ctors or any(not c.params for c in ctors)

    def _base_init(self, td, superclass, in_template, key=False):
        if superclass is None:
            return ''
        ctors = [m for m in superclass.methods if m.kind == 'constructor']
        if not ctors or any(not c.params for c in ctors) and not key:
            return ''
        candidates = []
        for c in ctors:
            self.ctx.capture = []
            try:
                subst = self._type_arg_map(td, superclass)
                types = [self._substitute(self.param_type(superclass, c, i, prm), subst) for i, prm in enumerate(c.params)]
            except Todo:
                continue
            finally:
                used, self.ctx.capture = self.ctx.capture, None
            containers = sum(1 for t in types if t.startswith(('const std::vector', 'const std::unordered', 'const std::map', 'const std::set')))
            candidates.append((containers, len(candidates), c, types, used))
        for containers, _, c, types, used in sorted(candidates, key=lambda x: (x[0], x[1])):
            base = self.ctx.project_type(superclass, as_base=True)
            if self.project.cpp_type_params(superclass):
                ref = td.extends[0]
                try:
                    base += '<' + ', '.join(self.ctx.map_type(a, td, 'typearg') for a in self.ctx._args(ref, td, len(superclass.type_params))) + '>'
                except Todo:
                    return ''
            args = ['key'] if key else []
            for t in types:
                if t.endswith('*'):
                    args.append(f'static_cast<{t}>(nullptr)')
                elif t.startswith('std::unique_ptr<'):
                    args.append('nullptr')
                elif t.endswith('&'):
                    # no temporary: a container of Ref<X> would need X complete (and RefCounted) for its destructor; X& has no value
                    args.append(f'unportedArgument<{t[len("const "):-1] if t.startswith("const ") else t[:-1]}>()')
                    self.needs_helper.add(id(td))
                else:
                    args.append(t + '{}')
            return f' : {base}({", ".join(args)})'
        return ''

    def _type_arg_map(self, td, sup):
        """{type variable of sup: C++ type argument} for a direct supertype reference of td (raises Todo if sup is not direct)."""
        if not self.project.cpp_type_params(sup):
            return {}
        for ref in list(td.extends) + list(td.implements):
            kind, value = self.project.index.resolve_kind(ref.name, td)
            if kind == 'project' and self.project.index.types[value] is sup:
                args = self.ctx._args(ref, td, len(sup.type_params))
                return {cpp_ident(tp.name): self.ctx.map_type(a, td, 'typearg') for tp, a in zip(sup.type_params, args)}
        raise Todo(f'{sup.fqn} is not a direct generic supertype')

    @staticmethod
    def _substitute(text, subst):
        for name, arg in subst.items():
            text = re.sub(rf'(?<![\w:]){re.escape(name)}(?!\w)', arg, text)
        return text

    @staticmethod
    def _trivial_accessor(td, m, field_names):
        if m.body is None or m.kind != 'method':
            return None
        t = m.body.texts()[1:-1]
        if not m.params and len(t) in (3, 5) and t[0] == 'return' and t[-1] == ';':
            name = t[1] if len(t) == 3 else (t[3] if t[1:3] == ['this', '.'] else None)
            if name in field_names and ('static' in field_names[name].modifiers) == ('static' in m.modifiers):
                return 'get', name
        if len(m.params) == 1 and m.return_type is not None and m.return_type.name == 'void':
            pn = m.params[0].name
            if len(t) == 6 and t[:2] == ['this', '.'] and t[3] == '=' and t[4] == pn and t[5] == ';':
                name = t[2]
            elif len(t) == 4 and t[1] == '=' and t[2] == pn and t[3] == ';' and t[0] != pn:
                name = t[0]
            else:
                return None
            if name in field_names and 'static' not in field_names[name].modifiers and 'static' not in m.modifiers:
                return 'set', name
        return None

    @staticmethod
    def _accessor_body(how, member, params, ret, name):
        if member.declaration:
            return None
        t = member.cpp_type.strip()
        core = re.sub(r'^(?:static\s+|inline\s+|mutable\s+|const\s+)+', '', t)
        shim = re.search(r'\b(ArrayList|LinkedList|HashMap|HashSet|ConcurrentHashMap|CopyOnWriteArrayList|ArrayDeque|PriorityQueue|'
                         r'ConcurrentLinkedQueue|Array|PartSlot|PartMap|PartList|unique_ptr|Atomic\w*)\b', core)
        if shim or ret.startswith(('std::vector', 'std::unordered', 'std::map', 'std::set', 'std::span')):
            return None
        is_field = re.match(r'(?:runtime::)?Field<', core) is not None
        is_final = re.match(r'(?:runtime::)?Final<', core) is not None
        pointerish = re.search(r'Ref<|Ptr<|FutureRef|SelfOrRef|OwnerRef|\*|shared_ptr', core) is not None
        if how == 'get':
            if pointerish and not (re.search(r'(?<!Future)Ref<', core) and ret.startswith('runtime::Ptr<')
                                   and not re.search(r'FutureRef|SelfOrRef|OwnerRef|\*|shared_ptr', core)):
                return None      # the conversion to the mapped return type is not mechanical (Final<T*>, FutureRef, connections)
            return f'return this->{name}.get();' if (is_field or is_final) else f'return this->{name};'
        # setters of reference members instantiate retain() (complete types): stubs
        if t.startswith('const ') or is_final or t.endswith('&') or pointerish or params[0][0].endswith('*'):
            return None
        pname, ptype = params[0][1], params[0][0]
        inner = core[core.index('<') + 1:core.rindex('>')].strip() if is_field else core
        if is_field:
            if inner == 'std::string' and ptype == 'std::string_view':
                return f'this->{name}.set(std::string({pname}));'
            return f'this->{name}.set({pname});' if _same_type(inner, ptype) else None
        if (core == 'std::string' and ptype == 'std::string_view') or _same_type(core, ptype):
            return f'this->{name} = {pname};'
        return None


_INCLUDE_RE = re.compile(r'^#include "([^"]+)"', re.M)


def generate_drafts(project, selected, unported_header=DEFAULT_UNPORTED_HEADER, with_dependencies=False):
    """{relative path: content} of the drafts (header + stub .cpp) for the files of the selected top-level types.

    with_dependencies also drafts every Java file whose full header a draft includes (base classes, outers of nested types used in
    signatures, types held by value or needed complete by stubs), transitively, except files already ported under --cpp-src, so the result
    compiles on its own together with the forward headers and --cpp-src."""
    files = {}
    units = {id(td.cu): td.cu for td in selected}
    by_header = {project.unit_header(cu): cu for cu in project.core_units}
    done = set()
    while True:
        pending = sorted((cu for key, cu in units.items() if key not in done), key=lambda u: u.relpath)
        if not pending:
            break
        for cu in pending:
            done.add(id(cu))
            header, cpp = DraftEmitter(project, cu, unported_header).emit()
            h = project.unit_header(cu)
            files[h] = header
            files[h[:-2] + '.cpp'] = cpp
            if with_dependencies:
                for inc in _INCLUDE_RE.findall(header + cpp):
                    dep = by_header.get(inc)
                    if dep is not None and not project.cpp.has_header(inc) and not any(
                            (project.generator_owner(t) or ('',))[0] == 'generated' for t in dep.types):
                        # ported headers (--cpp-src, generated tree) and generated replacements stay as they are; a member-block class is
                        # drafted around its block, an xmlgen behaviour class that is not scaffolded yet with a TODO(xmlgen) note
                        units.setdefault(id(dep), dep)
    return dict(sorted(files.items()))


# ----------------------------------------------------------------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------------------------------------------------------------

def write_files(out_root, files):
    """Writes files whose content changed. Returns (written, unchanged)."""
    out = Path(out_root)
    written = unchanged = 0
    for rel in sorted(files):
        path = out / rel
        data = files[rel].encode('utf-8')
        if path.is_file() and path.read_bytes() == data:
            unchanged += 1
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        written += 1
    return written, unchanged


def check_files(out_root, files, stale_mark=None):
    """Differences between files and disk: list of 'missing: x', 'different: x', 'stale: x' (sorted)."""
    out = Path(out_root)
    problems = []
    for rel in sorted(files):
        path = out / rel
        if not path.is_file():
            problems.append(f'missing: {rel}')
        elif path.read_bytes() != files[rel].encode('utf-8'):
            problems.append(f'different: {rel}')
    if stale_mark is not None and out.is_dir():
        for path in sorted(out.rglob('fwd.h'), key=lambda x: x.as_posix()):
            rel = path.relative_to(out).as_posix()
            if rel not in files and path.read_bytes().startswith(stale_mark.encode('utf-8')):
                problems.append(f'stale: {rel}')
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description='Forward headers, header drafts and stubs for the C++ game server (see module docstring).')
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--fwd', action='store_true', help='generate fwd.h for every Java package')
    mode.add_argument('--draft', action='store_true', help='generate header drafts and stub .cpp files for the selected classes')
    mode.add_argument('--list', action='store_true', help='print the Java FQNs the selectors expand to')
    mode.add_argument('--guards', action='store_true',
                      help='list the __has_include guards under --cpp-src and the tests (hub-headers.md §3.3); exit 1 on an open guard')
    mode.add_argument('--definitions', action='store_true',
                      help='list the member functions the hub and spine headers declare without a definition; exit 1 if any')
    ap.add_argument('--freeze', action='store_true', help='--guards: every guard is an error (the spine freeze gate)')
    ap.add_argument('selectors', nargs='*', help='FQN, simple name, pkg.*, pkg.**, @hubs, @services, @daos, @serverpackets, @engines, @all')
    ap.add_argument('--classes', help='file with one selector per line (# comments)')
    ap.add_argument('--out', help='output include root (required for --fwd/--draft)')
    ap.add_argument('--check', action='store_true', help='compare with --out instead of writing; exit 1 on differences')
    ap.add_argument('--with-dependencies', action='store_true',
                    help='--draft: also draft the files whose headers the drafts include (bases, outers of nested types), transitively')
    ap.add_argument('--java-root', default=str(DEFAULT_JAVA_ROOT))
    ap.add_argument('--handlers-root', default=str(DEFAULT_HANDLERS_ROOT))
    ap.add_argument('--no-handlers', action='store_true', help='do not read data/handlers (override analysis sees core subclasses only)')
    ap.add_argument('--cpp-src', default=str(DEFAULT_CPP_SRC))
    ap.add_argument('--commons-src', default=str(DEFAULT_COMMONS_SRC))
    gen = ap.add_mutually_exclusive_group()
    gen.add_argument('--generated-root', default=str(DEFAULT_GENERATED_ROOT),
                     help='xmlgen output tree: its headers count as existing C++ declarations, staticdata-classes.json as generator-owned')
    gen.add_argument('--no-generated-root', action='store_true')
    fm = ap.add_mutually_exclusive_group()
    fm.add_argument('--fieldmap', help=f'fieldmap.json (default {DEFAULT_FIELDMAP} if it exists)')
    fm.add_argument('--no-fieldmap', action='store_true')
    ap.add_argument('--unported-header', default=DEFAULT_UNPORTED_HEADER)
    args = ap.parse_args(argv)

    if args.definitions:
        cpp_src = Path(args.cpp_src)
        headers = spine_header_set()
        missing = undefined_member_functions(headers, cpp_src, [cpp_src])
        for header, qualified, line in missing:
            print(f'{header}:{line}: {qualified} is declared but has no definition')
        print(f'skeleton: {len(headers)} hub and spine headers, {len(missing)} declarations without a definition', file=sys.stderr)
        return 1 if missing else 0

    if args.guards:
        cpp_src = Path(args.cpp_src)
        scan = [cpp_src, cpp_src.parent / 'tests', cpp_src.parent / 'handlers']
        roots = [cpp_src, cpp_src.parent / 'handlers', Path(args.commons_src)]
        if not args.no_generated_root:
            roots.append(Path(args.generated_root))
        guards = spine_guards(scan, roots)
        for g in guards:
            print(f'{g.path}:{g.line}: ' + (f'waits for {", ".join(g.missing)}' if g.missing else f'open ({", ".join(g.headers)})'))
        problems = guard_problems(guards, frozen=args.freeze or SPINE_FROZEN)
        for p in problems:
            print(f'error: {p}', file=sys.stderr)
        missing = sorted({h for g in guards for h in g.missing})
        print(f'skeleton: {len(guards)} guards, {sum(1 for g in guards if not g.missing)} open, {len(missing)} missing headers, '
              f'{len(problems)} problems', file=sys.stderr)
        return 1 if problems else 0

    try:
        selectors = list(args.selectors)
        if args.classes:
            for line in Path(args.classes).read_text(encoding='utf-8').splitlines():
                line = line.split('#', 1)[0].strip()
                if line:
                    selectors.append(line)
        if (args.fwd or args.draft) and not args.out:
            raise SkeletonError('--out is required')
        if args.fwd and selectors:
            raise SkeletonError('--fwd takes no selectors')
        if (args.draft or args.list) and not selectors:
            raise SkeletonError('no classes selected')
        fieldmap = Fieldmap()
        if args.draft and not args.no_fieldmap:
            if args.fieldmap:
                fieldmap = Fieldmap.load(args.fieldmap)
            elif DEFAULT_FIELDMAP.is_file():
                fieldmap = Fieldmap.load(DEFAULT_FIELDMAP)
            else:
                print(f'skeleton: warning: {DEFAULT_FIELDMAP} not found, every class gets a TODO(fieldmap) member block', file=sys.stderr)
        project = Project(args.java_root, None if args.no_handlers or args.fwd or args.list else args.handlers_root,
                          args.cpp_src, args.commons_src, fieldmap, None if args.no_generated_root else args.generated_root)
        if args.list:
            for td in project.select(selectors):
                print(td.fqn)
            if project.skipped_existing:
                print(f'skeleton: skipped {len(project.skipped_existing)} classes that already have a header (or a generator owns them)',
                      file=sys.stderr)
            return 0
        if args.fwd:
            files = generate_fwd(project)
            stale_mark = FWD_MARK
        else:
            files = generate_drafts(project, project.select(selectors), args.unported_header, args.with_dependencies)
            if project.skipped_existing:
                print(f'skeleton: skipped {len(project.skipped_existing)} classes that already have a header (or a generator owns them): '
                      + ', '.join(sorted(project.skipped_existing)), file=sys.stderr)
            stale_mark = None
        if args.check:
            problems = check_files(args.out, files, stale_mark)
            for p in problems:
                print(p)
            print(f'skeleton: {len(files)} files checked, {len(problems)} problems', file=sys.stderr)
            return 1 if problems else 0
        written, unchanged = write_files(args.out, files)
        print(f'skeleton: {len(files)} files ({written} written, {unchanged} unchanged) under {args.out}', file=sys.stderr)
        return 0
    except (SkeletonError, javasrc.JavaSyntaxError) as e:
        print(f'skeleton: error: {e}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
