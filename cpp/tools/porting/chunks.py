"""chunks.py: reads the game server chunk manifest (cpp/game-server/chunks.cmake) without CMake (handlers-and-porting-plan.md §2.2).

Python 3.12 stdlib only. The manifest syntax, the glob language and the ownership rules are documented in
cpp/game-server/cmake/AionChunks.cmake; this module implements the same rules, and CTest gs.chunks.consistency compares its result with the
<build>/game-server/chunks.json that the configure step writes.

Usage (paths may be absolute, relative to the current directory, to the repository, to cpp/ or to cpp/game-server)
    python chunks.py owner PATH...                   owning chunk part of a C++ file (src/, handlers/, generated/) or of a file below tests/
                                                     (its test directory), or claiming part of a Java file (game-server/src,
                                                     game-server/data/handlers); leases are listed too. Files need not exist.
    python chunks.py files CHUNK                     C++ files of a chunk on disk (all parts; a LEASE part lists the files it leases)
    python chunks.py java CHUNK                      Java files claimed by a chunk
    python chunks.py list                            chunk parts: name, target, phase, roots, file and Java counts, test directory
    python chunks.py check                           the configure-time checks: every C++ source file has exactly one owner, no other C++
                                                     extension or extension case, every C++ file below tests/ lies in a chunk's test directory,
                                                     every Java file has exactly one claim
    python chunks.py check-ownership CHUNK RANGE     files changed in the git range (e.g. main..port/P4-05) that the chunk may not change:
                                                     allowed are its own files and leases, its test directories below cpp/game-server/tests
                                                     (TESTS, TEST_SUPPORT, leased TEST_SUPPORT) and cpp/docs/deviations/<chunk>.md
    python chunks.py verify-json CHUNKS_JSON         compares the ownership with the chunks.json written by CMake
Options: --manifest FILE (default cpp/game-server/chunks.cmake), --game-server DIR (C++ tree), --java-dir DIR (Java game-server tree).
Exit status: 0 ok, 1 problems found (check, check-ownership, verify-json) or unknown chunk/path, 2 usage or manifest errors.

Glob language (relative to the part's roots for C++, to the Java game-server directory for JAVA): `**/` any number of directories (also none),
a trailing `/**` everything below, `*` any characters except `/`, `?` one character except `/`, `[A-K]` a character class, `{a,b}` alternatives
(not nested). Other characters are literal; allowed are letters, digits and `_-./`.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
CPP_ROOT = HERE.parent.parent
REPO_ROOT = CPP_ROOT.parent
GAME_SERVER = CPP_ROOT / 'game-server'
DEFAULT_MANIFEST = GAME_SERVER / 'chunks.cmake'
JAVA_GAME_SERVER = REPO_ROOT / 'game-server'

CHUNK_KEYWORDS = {
    'options': ('LEASE', 'XMLGEN_SHELLS'),
    'single': ('TARGET', 'PHASE', 'TESTS', 'MAIN', 'COMPILE_WHEN_EXISTS'),
    'multi': ('ROOT', 'GLOBS', 'EXCLUDE', 'JAVA', 'JAVA_EXCLUDE', 'DEPENDS', 'PCH', 'TEST_INCLUDES', 'TEST_SUPPORT'),
}
SETTINGS_KEYWORDS = {'options': (), 'single': (), 'multi': ('EXTENSIONS', 'ROOTS', 'JAVA_ROOTS')}
GLOB_CHARS = re.compile(r'^[A-Za-z0-9_./*?{},\[\]-]+$')
TEST_DIR = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)*$')
# C++ source extensions that must not appear in another case, or at all unless they are manifest EXTENSIONS (the same list as AionChunks.cmake)
SOURCE_LIKE_EXTENSIONS = ('c', 'cc', 'cxx', 'cpp', 'h', 'hh', 'hpp', 'hxx', 'inc', 'inl', 'ipp', 'ixx', 'tcc', 'tpp')
TESTS = 'tests'


class ManifestError(Exception):
    pass


# -- CMake command parser ----------------------------------------------------------------------------------------------------------------------
def parse_cmake_commands(text, path='chunks.cmake'):
    """[(command name, [arguments], line)] of a CMake file made of plain command invocations (quoted/unquoted arguments, # comments)."""
    commands = []
    i, n, line = 0, len(text), 1

    def error(msg):
        raise ManifestError(f'{path}:{line}: {msg}')

    while i < n:
        c = text[i]
        if c == '\n':
            line += 1
            i += 1
        elif c in ' \t\r':
            i += 1
        elif c == '#':
            if text.startswith('#[[', i) or text.startswith('#[=', i):
                error('bracket comments are not supported')
            while i < n and text[i] != '\n':
                i += 1
        elif c.isalpha() or c == '_':
            start, start_line = i, line
            while i < n and (text[i].isalnum() or text[i] == '_'):
                i += 1
            name = text[start:i]
            while i < n and text[i] in ' \t':
                i += 1
            if i >= n or text[i] != '(':
                error(f'expected ( after {name}')
            i += 1
            args = []
            while True:
                if i >= n:
                    error(f'unterminated {name}(')
                c = text[i]
                if c == '\n':
                    line += 1
                    i += 1
                elif c in ' \t\r':
                    i += 1
                elif c == '#':
                    while i < n and text[i] != '\n':
                        i += 1
                elif c == ')':
                    i += 1
                    break
                elif c == '"':
                    i += 1
                    buf = []
                    while True:
                        if i >= n:
                            error('unterminated quoted argument')
                        c = text[i]
                        if c == '"':
                            i += 1
                            break
                        if c == '\\' and i + 1 < n:
                            esc = text[i + 1]
                            buf.append({'n': '\n', 't': '\t', 'r': '\r'}.get(esc, esc))
                            i += 2
                            continue
                        if c == '$' and text.startswith('${', i):
                            error('variable references are not supported in the manifest')
                        if c == '\n':
                            line += 1
                        buf.append(c)
                        i += 1
                    args.append(''.join(buf))
                elif c == '(':
                    error('nested parentheses are not supported')
                else:
                    start = i
                    while i < n and text[i] not in ' \t\r\n()#"':
                        i += 1
                    arg = text[start:i]
                    if '${' in arg:
                        error('variable references are not supported in the manifest')
                    args.append(arg)
            commands.append((name, args, start_line))
        else:
            error(f'unexpected character {c!r}')
    return commands


def parse_keywords(args, spec, where):
    """cmake_parse_arguments: {keyword: True | str | [str]} (unparsed leading arguments are an error)."""
    keywords = set(spec['options']) | set(spec['single']) | set(spec['multi'])
    result = {}
    current = None
    for a in args:
        if a in keywords:
            if a in result:
                raise ManifestError(f'{where}: {a} given twice')
            if a in spec['options']:
                result[a] = True
                current = None
            elif a in spec['single']:
                result[a] = None
                current = a
            else:
                result[a] = []
                current = a
        elif current is None:
            raise ManifestError(f'{where}: unexpected argument {a!r}')
        elif current in spec['single']:
            if result[current] is not None:
                raise ManifestError(f'{where}: unexpected argument {a!r} ({current} takes one value)')
            result[current] = a
        else:
            result[current].append(a)
    for k in spec['single']:
        if k in result and result[k] is None:
            raise ManifestError(f'{where}: {k} needs a value')
    return result


# -- globs ------------------------------------------------------------------------------------------------------------------------------------
def glob_to_regex(glob):
    """Regex (without anchors) of a manifest glob. The same replacement sequence as aion_gs_glob_to_regex in AionChunks.cmake."""
    if not GLOB_CHARS.match(glob):
        raise ManifestError(f'glob {glob!r}: only letters, digits and _-./*?[]{{}}, are allowed')
    depth = 0
    for ch in glob:
        if ch == '{':
            depth += 1
            if depth > 1:
                raise ManifestError(f'glob {glob!r}: nested braces')
        elif ch == '}':
            depth -= 1
            if depth < 0:
                raise ManifestError(f'glob {glob!r}: unbalanced braces')
        elif ch == ',' and depth == 0:
            raise ManifestError(f'glob {glob!r}: comma outside braces')
    if depth != 0:
        raise ManifestError(f'glob {glob!r}: unbalanced braces')
    if glob.count('[') != glob.count(']'):
        raise ManifestError(f'glob {glob!r}: unbalanced character class')
    r = glob.replace('.', '\\.')
    r = r.replace('**/', '@D@').replace('/**', '@E@').replace('**', '@F@')
    r = r.replace('*', '[^/]*').replace('?', '[^/]')
    r = r.replace('{', '(').replace('}', ')').replace(',', '|')
    r = r.replace('@D@', '(.*/)?').replace('@E@', '/.*').replace('@F@', '.*')
    return r


def anchored(prefixes, globs):
    """Compiled ^(prefix|...)/(glob|...)$ or None for no globs."""
    if not globs:
        return None
    pre = '|'.join(re.escape(p) for p in prefixes) if prefixes else ''
    body = '|'.join(glob_to_regex(g) for g in globs)
    return re.compile(f'^({pre})/({body})$' if prefixes else f'^({body})$')


# -- manifest ---------------------------------------------------------------------------------------------------------------------------------
@dataclass
class Part:
    index: int
    name: str
    line: int
    target: str | None = None
    phase: str | None = None
    lease: bool = False
    xmlgen_shells: bool = False
    roots: list = field(default_factory=list)
    globs: list = field(default_factory=list)
    exclude: list = field(default_factory=list)
    java: list = field(default_factory=list)
    java_exclude: list = field(default_factory=list)
    depends: list = field(default_factory=list)
    pch: list = field(default_factory=list)
    tests: str | None = None
    test_includes: list = field(default_factory=list)
    test_support: list = field(default_factory=list)
    main: str | None = None
    compile_when_exists: str | None = None
    tests_dir: str = ''  # resolved by Manifest: TESTS, or the target without aion_gs_ (+ /<chunk> for a shared target); '' for a lease

    def __post_init__(self):
        self._include = self._exclude = self._java = self._java_exclude = None

    def compile(self):
        self._include = anchored(self.roots, self.globs)
        self._exclude = anchored(self.roots, self.exclude)
        self._java = anchored([], self.java)
        self._java_exclude = anchored([], self.java_exclude)
        self._main = f'{self.roots[0]}/{self.main}' if self.main else None

    def matches(self, path):
        """path: '<root>/<relative path>' (glob match only, without the XMLGEN_SHELLS restriction)"""
        if self._main is not None and path == self._main:
            return True
        if self._include is None or not self._include.match(path):
            return False
        return self._exclude is None or not self._exclude.match(path)

    def claims_java(self, path):
        """path relative to the Java game-server directory"""
        if self._java is None or not self._java.match(path):
            return False
        return self._java_exclude is None or not self._java_exclude.match(path)

    def describe(self):
        return f'{self.name} ({self.target})' if self.target else f'{self.name} (lease)'


@dataclass
class Manifest:
    path: str
    extensions: list
    roots: list
    java_roots: list
    parts: list

    @classmethod
    def load(cls, path=DEFAULT_MANIFEST):
        path = str(path)
        with open(path, encoding='utf-8') as f:
            text = f.read()
        settings = None
        parts = []
        for name, args, line in parse_cmake_commands(text, path):
            where = f'{path}:{line}'
            if name == 'aion_gs_chunks_settings':
                if settings is not None:
                    raise ManifestError(f'{where}: aion_gs_chunks_settings given twice')
                settings = parse_keywords(args, SETTINGS_KEYWORDS, where)
            elif name == 'aion_gs_chunk':
                if not args:
                    raise ManifestError(f'{where}: aion_gs_chunk without a name')
                kw = parse_keywords(args[1:], CHUNK_KEYWORDS, where)
                p = Part(len(parts), args[0], line, target=kw.get('TARGET'), phase=kw.get('PHASE'), lease=bool(kw.get('LEASE')),
                         xmlgen_shells=bool(kw.get('XMLGEN_SHELLS')), roots=kw.get('ROOT') or ['src'], globs=kw.get('GLOBS', []),
                         exclude=kw.get('EXCLUDE', []), java=kw.get('JAVA', []), java_exclude=kw.get('JAVA_EXCLUDE', []),
                         depends=kw.get('DEPENDS', []), pch=kw.get('PCH', []), tests=kw.get('TESTS'),
                         test_includes=kw.get('TEST_INCLUDES', []), test_support=kw.get('TEST_SUPPORT', []), main=kw.get('MAIN'),
                         compile_when_exists=kw.get('COMPILE_WHEN_EXISTS'))
                parts.append(p)
            else:
                raise ManifestError(f'{where}: only aion_gs_chunks_settings() and aion_gs_chunk() are allowed, found {name}()')
        if settings is None:
            raise ManifestError(f'{path}: aion_gs_chunks_settings() is missing')
        m = cls(path, settings.get('EXTENSIONS', []), settings.get('ROOTS', []), settings.get('JAVA_ROOTS', []), parts)
        m.validate()
        for p in parts:
            p.compile()
        return m

    def validate(self):
        if not self.extensions or not self.roots or not self.java_roots:
            raise ManifestError(f'{self.path}: aion_gs_chunks_settings needs EXTENSIONS, ROOTS and JAVA_ROOTS')
        for ext in self.extensions:
            if not re.match(r'^[a-z0-9]+$', ext):
                raise ManifestError(f"{self.path}: aion_gs_chunks_settings: extension '{ext}': lower-case letters and digits only")
        for p in self.parts:
            where = f'{self.path}:{p.line}: {p.name}'
            if not re.match(r'^[A-Za-z0-9][A-Za-z0-9_-]*$', p.name):
                raise ManifestError(f'{where}: invalid chunk name')
            if p.lease:
                if p.target or p.java or p.java_exclude or p.depends or p.pch or p.main or p.tests or p.test_includes or p.compile_when_exists:
                    raise ManifestError(f'{where}: a LEASE has only PHASE, ROOT, GLOBS, EXCLUDE, TEST_SUPPORT and XMLGEN_SHELLS')
            elif not p.target:
                raise ManifestError(f'{where}: TARGET is required (or LEASE)')
            if not p.phase:
                raise ManifestError(f'{where}: PHASE is required')
            if not p.globs and not p.main and not (p.lease and p.test_support):
                raise ManifestError(f'{where}: GLOBS is required')
            for d in ([p.tests] if p.tests else []) + p.test_includes + p.test_support:
                if not TEST_DIR.match(d):
                    raise ManifestError(f"{where}: test directory '{d}': a relative path below game-server/tests (letters, digits and _-./)")
            for r in p.roots:
                if r not in self.roots:
                    raise ManifestError(f'{where}: unknown ROOT {r}')
            if p.xmlgen_shells and p.roots != ['src']:
                raise ManifestError(f'{where}: XMLGEN_SHELLS needs ROOT src')
            if p.main and len(p.roots) != 1:
                raise ManifestError(f'{where}: MAIN needs exactly one ROOT')
            for g in p.globs + p.exclude + p.java + p.java_exclude:
                glob_to_regex(g)
        self._validate_targets_and_test_dirs()

    def _validate_targets_and_test_dirs(self):
        """the rules across parts, in the order of aion_gs_check_chunks: leaf targets, then the resolved test directories must not overlap"""
        chunks_of, parts_of, leaves_of = {}, {}, {}
        for p in self.parts:
            if p.target:
                if p.name not in chunks_of.setdefault(p.target, []):
                    chunks_of[p.target].append(p.name)
                parts_of[p.target] = parts_of.get(p.target, 0) + 1
                leaves_of[p.target] = leaves_of.get(p.target, 0) + (1 if p.depends else 0)
        for target in chunks_of:
            if 0 < leaves_of[target] != parts_of[target]:
                raise ManifestError(f'chunks.cmake: target {target}: either all parts have DEPENDS (leaf) or none')
        self.test_dirs = []  # [(dir, owning part)] without equal entries (same chunk, target and kind)
        keys = set()
        for p in self.parts:
            if p.lease:
                p.tests_dir = ''
                continue
            if p.tests:
                p.tests_dir = p.tests
            else:
                p.tests_dir = p.target[len('aion_gs_'):] if p.target.startswith('aion_gs_') else p.target
                if len(chunks_of[p.target]) > 1:
                    p.tests_dir += '/' + p.name
            for key in [(p.tests_dir, p.name, p.target, 'TESTS')] + [(d, p.name, '', 'TEST_SUPPORT') for d in p.test_support]:
                if key not in keys:
                    keys.add(key)
                    self.test_dirs.append((key[0], p))
        overlaps = []
        for a in range(len(self.test_dirs)):
            for b in range(a + 1, len(self.test_dirs)):
                (dir_a, part_a), (dir_b, part_b) = self.test_dirs[a], self.test_dirs[b]
                if (dir_a + '/').startswith(dir_b + '/') or (dir_b + '/').startswith(dir_a + '/'):
                    overlaps.append(f'test directory tests/{dir_a} of {part_a.describe()} overlaps tests/{dir_b} of {part_b.describe()}')
        if overlaps:
            raise ManifestError('chunks.cmake: the test directories of different chunks or targets must not overlap:\n  ' + '\n  '.join(overlaps))

    def chunk_parts(self, chunk):
        return [p for p in self.parts if p.name == chunk]

    def test_dir_parts(self, rel):
        """(owner parts, lease parts) of 'tests/<rel>' by its test directory (leases: TEST_SUPPORT of LEASE parts)"""
        inside = rel.split('/', 1)[1] if rel.startswith(TESTS + '/') else None
        if inside is None:
            return [], []

        def contains(d):
            return inside.startswith(d + '/')
        owners = [p for d, p in self.test_dirs if contains(d)]
        leases = [p for p in self.parts if p.lease and any(contains(d) for d in p.test_support)]
        return owners, leases


# -- trees ------------------------------------------------------------------------------------------------------------------------------------
@dataclass
class Scan:
    cpp_files: list        # '<root>/<rel>' with a manifest EXTENSION (exact case)
    test_files: list       # 'tests/<rel>' with a manifest EXTENSION (exact case)
    extension_problems: list


def scan_tree(manifest, game_server=GAME_SERVER):
    """the C++ files below the roots and tests/, and the files with another C++ extension or an extension in another case (like
    aion_gs_check_chunks, independent of the file system's case sensitivity)"""
    exact = set(manifest.extensions)
    source_like = exact | set(SOURCE_LIKE_EXTENSIONS)
    allowed = ' '.join('.' + e for e in manifest.extensions)
    cpp, tests, problems = [], [], []
    for top in list(manifest.roots) + [TESTS]:
        base = Path(game_server) / top
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames.sort()
            rel_dir = os.path.relpath(dirpath, game_server).replace('\\', '/')
            for fn in filenames:
                _, dot, ext = fn.rpartition('.')
                if not dot or ext.lower() not in source_like:
                    continue
                path = f'{rel_dir}/{fn}'
                if ext not in exact:
                    problems.append(f'unexpected C++ file extension: {path} (allowed: {allowed})')
                else:
                    (tests if top == TESTS else cpp).append(path)
    return Scan(sorted(cpp), sorted(tests), sorted(problems))


def scan_cpp(manifest, game_server=GAME_SERVER):
    """sorted '<root>/<relative path>' of the source files (manifest EXTENSIONS) below the roots"""
    return scan_tree(manifest, game_server).cpp_files


def scan_java(manifest, java_dir=JAVA_GAME_SERVER):
    """sorted Java paths relative to the Java game-server directory matching JAVA_ROOTS"""
    pattern = anchored([], manifest.java_roots)
    files = []
    for top in sorted({g.split('/', 1)[0] for g in manifest.java_roots}):
        base = Path(java_dir) / top
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames.sort()
            rel_dir = os.path.relpath(dirpath, java_dir).replace('\\', '/')
            for fn in filenames:
                rel = f'{rel_dir}/{fn}'
                if pattern.match(rel):
                    files.append(rel)
    return sorted(files)


def shell_counterpart(path):
    """'src/aion/x/Y.h' -> 'generated/aion/x/Y.xml.inc' (XMLGEN_SHELLS)"""
    if not path.startswith('src/'):
        return None
    stem, dot, _ = path[len('src/'):].rpartition('.')
    return f'generated/{stem}.xml.inc' if dot else None


class Ownership:
    """Owners of the C++ files and claims of the Java files, computed like aion_gs_finalize_chunks()."""

    def __init__(self, manifest, cpp_files, java_files, test_files=(), extension_problems=()):
        self.manifest = manifest
        self.cpp_files = cpp_files
        self.java_files = java_files
        self.test_files = list(test_files)
        self.extension_problems = list(extension_problems)
        existing = set(cpp_files)
        self.part_files = {p.index: [] for p in manifest.parts}
        self.part_java = {p.index: [] for p in manifest.parts}
        self.owners = {}
        self.leases = {}
        for path in cpp_files:
            for p in manifest.parts:
                if not p.matches(path):
                    continue
                if p.xmlgen_shells and shell_counterpart(path) not in existing:
                    continue
                (self.leases if p.lease else self.owners).setdefault(path, []).append(p)
                self.part_files[p.index].append(path)
        self.claims = {}
        for path in java_files:
            for p in manifest.parts:
                if p.claims_java(path):
                    self.claims.setdefault(path, []).append(p)
                    self.part_java[p.index].append(path)

    @classmethod
    def of_tree(cls, manifest, game_server=GAME_SERVER, java_dir=JAVA_GAME_SERVER):
        scan = scan_tree(manifest, game_server)
        return cls(manifest, scan.cpp_files, scan_java(manifest, java_dir), scan.test_files, scan.extension_problems)

    def problems(self):
        out = list(self.extension_problems)
        for path in self.cpp_files:
            owners = self.owners.get(path, [])
            if not owners:
                out.append(f'no chunk owns {path}')
            elif len(owners) > 1:
                out.append(f'several chunks own {path}: ' + ', '.join(p.describe() for p in owners))
        for path in self.test_files:
            if not self.manifest.test_dir_parts(path)[0]:
                out.append(f'no chunk owns test file {path}')
        for path in self.java_files:
            claims = self.claims.get(path, [])
            if not claims:
                out.append(f'no chunk claims Java file {path}')
            elif len(claims) > 1:
                out.append(f'several chunks claim Java file {path}: ' + ', '.join(p.describe() for p in claims))
        return out


def owners_of(manifest, path, game_server=GAME_SERVER):
    """(owner parts, lease parts) of '<root>/<rel>' by glob, independent of the file's existence (leases with XMLGEN_SHELLS check the disk)"""
    owners, leases = [], []
    for p in manifest.parts:
        if not p.matches(path):
            continue
        if p.xmlgen_shells:
            counterpart = shell_counterpart(path)
            if counterpart is None or not (Path(game_server) / counterpart).is_file():
                continue
        (leases if p.lease else owners).append(p)
    return owners, leases


def normalize(path_arg, manifest, game_server=GAME_SERVER, java_dir=JAVA_GAME_SERVER):
    """('cpp', '<root>/<rel>'), ('test', 'tests/<rel>'), ('java', '<rel to Java game-server>') or (None, reason) for a path in any supported form"""
    raw = Path(path_arg)
    game_server, java_dir = Path(os.path.normpath(game_server)), Path(os.path.normpath(java_dir))
    candidates = []
    if raw.is_absolute():
        candidates.append(raw)
    else:
        candidates += [Path.cwd() / raw, REPO_ROOT / raw, CPP_ROOT / raw, game_server / raw]
    for c in candidates:
        c = Path(os.path.normpath(c))
        for base, kind in ((game_server, 'cpp'), (java_dir, 'java')):
            try:
                rel = c.relative_to(base).as_posix()
            except ValueError:
                continue
            top = rel.split('/', 1)[0]
            if kind == 'cpp' and top in manifest.roots:
                return kind, rel
            if kind == 'cpp' and top == TESTS and '/' in rel:
                return 'test', rel
            if kind == 'java' and rel.endswith('.java'):
                return kind, rel
    return None, f'{path_arg}: not below cpp/game-server/{{{",".join(manifest.roots + [TESTS])}}} nor a Java file below game-server'


# -- commands ---------------------------------------------------------------------------------------------------------------------------------
def cmd_owner(args, manifest):
    status = 0
    for a in args.paths:
        kind, rel = normalize(a, manifest, args.game_server, args.java_dir)
        if kind is None:
            print(rel)
            status = 1
        elif kind in ('cpp', 'test'):
            owners, leases = owners_of(manifest, rel, args.game_server) if kind == 'cpp' else manifest.test_dir_parts(rel)
            text = ', '.join(p.describe() for p in owners) or 'no owner'
            if leases:
                text += '; leased to ' + ', '.join(p.name for p in leases)
            print(f'{rel}: {text}')
            if len(owners) != 1:
                status = 1
        else:
            claims = [p for p in manifest.parts if p.claims_java(rel)]
            print(f'{rel}: ' + (', '.join(p.describe() for p in claims) or 'not claimed'))
            if len(claims) != 1:
                status = 1
    return status


def require_chunk(manifest, chunk):
    parts = manifest.chunk_parts(chunk)
    if not parts:
        known = sorted({p.name for p in manifest.parts})
        print(f'unknown chunk {chunk}; known: {" ".join(known)}', file=sys.stderr)
    return parts


def cmd_files(args, manifest):
    parts = require_chunk(manifest, args.chunk)
    if not parts:
        return 1
    own = Ownership(manifest, scan_cpp(manifest, args.game_server), [])
    for path in sorted({f for p in parts for f in own.part_files[p.index]}):
        print(path)
    return 0


def cmd_java(args, manifest):
    parts = require_chunk(manifest, args.chunk)
    if not parts:
        return 1
    own = Ownership(manifest, [], scan_java(manifest, args.java_dir))
    for path in sorted({f for p in parts for f in own.part_java[p.index]}):
        print(path)
    return 0


def cmd_list(args, manifest):
    own = Ownership(manifest, scan_cpp(manifest, args.game_server), scan_java(manifest, args.java_dir))
    for p in manifest.parts:
        tests = ' '.join(([f'tests/{p.tests_dir}'] if p.tests_dir else []) + [f'tests/{d}' for d in p.test_support])
        print(f'{p.name}\t{p.target or "(lease)"}\tphase {p.phase}\t{",".join(p.roots)}\t{len(own.part_files[p.index])} files\t'
              f'{len(own.part_java[p.index])} java\t{tests}')
    return 0


def cmd_check(args, manifest):
    own = Ownership.of_tree(manifest, args.game_server, args.java_dir)
    problems = own.problems()
    for line in problems:
        print(line)
    names = {p.name for p in manifest.parts}
    targets = {p.target for p in manifest.parts if p.target}
    print(f'chunks: {len(names)} chunks, {len(manifest.parts)} parts, {len(targets)} targets; {len(own.cpp_files)} C++ files, '
          f'{len(own.test_files)} test files, {len(own.java_files)} Java files, {len(problems)} problems')
    return 1 if problems else 0


def git_changed_files(rng):
    out = subprocess.run(['git', '-C', str(REPO_ROOT), 'diff', '--name-only', '--no-renames', '-z', rng], capture_output=True, check=False)
    if out.returncode != 0:
        raise ManifestError(f'git diff {rng} failed: {out.stderr.decode(errors="replace").strip()}')
    return sorted(p for p in out.stdout.decode('utf-8').split('\0') if p)


def ownership_violations(manifest, chunk, changed, game_server=GAME_SERVER):
    """changed: repository-relative paths; returns [(path, reason)] of the files the chunk may not change"""
    gs_prefix = GAME_SERVER.relative_to(REPO_ROOT).as_posix() + '/'
    cpp_prefix = CPP_ROOT.relative_to(REPO_ROOT).as_posix() + '/'
    violations = []
    for path in changed:
        if path.startswith(gs_prefix):
            rel = path[len(gs_prefix):]
            top = rel.split('/', 1)[0]
            if top in manifest.roots or top == TESTS:
                owners, leases = owners_of(manifest, rel, game_server) if top != TESTS else manifest.test_dir_parts(rel)
                if any(p.name == chunk for p in owners + leases):
                    continue
                violations.append((path, 'owned by ' + (', '.join(p.describe() for p in owners) or 'no chunk')))
                continue
        if path == f'{cpp_prefix}docs/deviations/{chunk}.md':
            continue
        violations.append((path, 'outside the chunk'))
    return violations


def cmd_check_ownership(args, manifest):
    if not require_chunk(manifest, args.chunk):
        return 1
    violations = ownership_violations(manifest, args.chunk, git_changed_files(args.range), args.game_server)
    for path, reason in violations:
        print(f'{path}: {reason}')
    print(f'check-ownership {args.chunk} {args.range}: {len(violations)} files outside the chunk')
    return 1 if violations else 0


def cmd_verify_json(args, manifest):
    with open(args.json, encoding='utf-8') as f:
        doc = json.load(f)
    own = Ownership.of_tree(manifest, args.game_server, args.java_dir)
    problems = own.problems()
    if doc.get('testFileCount') != len(own.test_files):
        problems.append(f'chunks.json has {doc.get("testFileCount")} test files, chunks.py {len(own.test_files)}')
    chunks = doc.get('parts', [])
    if len(chunks) != len(manifest.parts):
        problems.append(f'chunks.json has {len(chunks)} parts, the manifest {len(manifest.parts)}')
    for entry, p in zip(chunks, manifest.parts):
        label = f'part {p.index} {p.describe()}'
        if entry.get('name') != p.name or (entry.get('target') or None) != p.target:
            problems.append(f'{label}: chunks.json has {entry.get("name")} ({entry.get("target")})')
            continue
        if entry.get('tests') != p.tests_dir or entry.get('testSupport') != p.test_support:
            problems.append(f'{label}: test directories differ (CMake: {entry.get("tests")!r} {entry.get("testSupport")}, '
                            f'chunks.py: {p.tests_dir!r} {p.test_support})')
        for key, mine in (('files', own.part_files[p.index]), ('javaFiles', own.part_java[p.index])):
            theirs = entry.get(key, [])
            if sorted(theirs) != sorted(mine):
                only_cmake = sorted(set(theirs) - set(mine))[:5]
                only_python = sorted(set(mine) - set(theirs))[:5]
                problems.append(f'{label}: {key} differ (CMake only: {only_cmake}, chunks.py only: {only_python})')
    for line in problems:
        print(line)
    print(f'verify-json: {len(manifest.parts)} parts, {len(own.cpp_files)} C++ files, {len(own.test_files)} test files, {len(own.java_files)} Java files, '
          f'{len(problems)} problems')
    return 1 if problems else 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog='chunks.py', description=__doc__.split('\n', 1)[0])
    ap.add_argument('--manifest', default=str(DEFAULT_MANIFEST))
    ap.add_argument('--game-server', default=str(GAME_SERVER), help='C++ game-server directory (src, handlers, generated)')
    ap.add_argument('--java-dir', default=str(JAVA_GAME_SERVER), help='Java game-server directory (src, data/handlers)')
    sub = ap.add_subparsers(dest='command', required=True)
    s = sub.add_parser('owner')
    s.add_argument('paths', nargs='+')
    s = sub.add_parser('files')
    s.add_argument('chunk')
    s = sub.add_parser('java')
    s.add_argument('chunk')
    sub.add_parser('list')
    sub.add_parser('check')
    s = sub.add_parser('check-ownership')
    s.add_argument('chunk')
    s.add_argument('range')
    s = sub.add_parser('verify-json')
    s.add_argument('json')
    args = ap.parse_args(argv)
    try:
        manifest = Manifest.load(args.manifest)
        handler = {'owner': cmd_owner, 'files': cmd_files, 'java': cmd_java, 'list': cmd_list, 'check': cmd_check,
                   'check-ownership': cmd_check_ownership, 'verify-json': cmd_verify_json}[args.command]
        return handler(args, manifest)
    except ManifestError as e:
        print(f'chunks.py: {e}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
