"""dialogaction: generates the C++ DialogAction constants from DialogAction.java.

Input:  game-server/src/com/aionemu/gameserver/model/DialogAction.java (a final class with 6,205 `public static final int` constants, a private
        nameById map filled by a static initializer through reflection, and nameOf(int))
Output (under cpp/game-server/src/aion/gameserver/, committed; docs/design/handlers-and-porting-plan.md §2.3):
    model/DialogAction.h        namespace aion::gameserver::model::DialogAction: `inline constexpr int32_t NAME = value;` per constant in
                                source order (trailing Java comments kept), nameOf(int32_t) and entries()
    model/DialogAction.gen.cpp  the {id, Java name} table sorted by id (static_assert: strictly increasing, i.e. no duplicate id), nameOf, entries

Java semantics reproduced:
    The static initializer puts every public field whose value is an Integer into nameById and throws
    IllegalArgumentException("Duplicate id <id>: <name>, <previous name>") for a repeated id. The generator fails with the same message
    (declaration order stands in for Class.getFields() order). nameOf returns null for an unknown id; C++ returns std::nullopt.
    A Java name that is a C++ keyword or a macro that cannot be undefined (NULL, EOF, ...) gets a trailing underscore in C++ (NULL -> NULL_);
    nameOf still returns the Java name.

Anything else in the class (other public fields, non-literal initializers, methods other than nameOf) is rejected.

Usage: python dialogaction.py [--java-dir GAME_SERVER_DIR] [--out-dir CPP_GAME_SERVER_SRC] [--check]
"""
from __future__ import annotations

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import javasrc  # noqa: E402

GEN_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(GEN_DIR, '..', '..', '..'))
DEFAULT_JAVA_DIR = os.path.join(REPO, 'game-server')
DEFAULT_OUT_DIR = os.path.join(REPO, 'cpp', 'game-server', 'src', 'aion', 'gameserver')

DIALOG_ACTION_JAVA = 'src/com/aionemu/gameserver/model/DialogAction.java'
HEADER_OUT = 'model/DialogAction.h'
SOURCE_OUT = 'model/DialogAction.gen.cpp'

EXPECTED_INITIALIZER = (
    '{ Map < Integer , String > nameById = DialogAction . nameById ; try { for ( Field publicField : DialogAction . class . getFields ( ) ) { '
    'if ( publicField . get ( null ) instanceof Integer dialogActionId ) { String previousName = nameById . put ( dialogActionId , '
    'publicField . getName ( ) ) ; if ( previousName != null ) throw new IllegalArgumentException ( "Duplicate id " + dialogActionId + ": " + '
    'publicField . getName ( ) + ", " + previousName ) ; } } } catch ( IllegalArgumentException | IllegalAccessException e ) { throw new '
    'ExceptionInInitializerError ( e ) ; } }')
EXPECTED_NAME_OF = '{ return nameById . get ( dialogActionId ) ; }'

# C++ keywords and alternative tokens (a Java identifier can be any of these except the ones Java reserves itself)
CPP_KEYWORDS = frozenset('''
alignas alignof and and_eq auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const constexpr constinit
const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for
friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register
reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try
typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq
'''.split())

# Object-like macros of the C/C++ standard library and <windows.h> that commons' WindowsMacroGuard.h does not (or cannot) remove
CPP_MACROS = frozenset('''
NULL EOF TRUE FALSE CONST VOID STRICT CALLBACK WINAPI APIENTRY PASCAL interface small hyper errno assert stdin stdout stderr offsetof
INFINITY NAN HUGE_VAL HUGE_VALF HUGE_VALL FP_NAN FP_INFINITE FP_ZERO FP_SUBNORMAL FP_NORMAL MATH_ERRNO MATH_ERREXCEPT math_errhandling
BUFSIZ FILENAME_MAX FOPEN_MAX TMP_MAX L_tmpnam SEEK_SET SEEK_CUR SEEK_END EXIT_SUCCESS EXIT_FAILURE RAND_MAX MB_CUR_MAX MB_LEN_MAX
CHAR_BIT SCHAR_MIN SCHAR_MAX UCHAR_MAX CHAR_MIN CHAR_MAX SHRT_MIN SHRT_MAX USHRT_MAX INT_MIN INT_MAX UINT_MAX LONG_MIN LONG_MAX ULONG_MAX
LLONG_MIN LLONG_MAX ULLONG_MAX SIZE_MAX PTRDIFF_MIN PTRDIFF_MAX WCHAR_MIN WCHAR_MAX WINT_MIN WINT_MAX CLOCKS_PER_SEC TIME_UTC
SIGABRT SIGFPE SIGILL SIGINT SIGSEGV SIGTERM SIG_DFL SIG_ERR SIG_IGN LC_ALL LC_COLLATE LC_CTYPE LC_MONETARY LC_NUMERIC LC_TIME
FLT_MAX FLT_MIN FLT_EPSILON DBL_MAX DBL_MIN DBL_EPSILON LDBL_MAX LDBL_MIN LDBL_EPSILON FLT_DIG DBL_DIG FLT_RADIX DECIMAL_DIG
EDOM ERANGE EILSEQ EPERM ENOENT ESRCH EINTR EIO ENXIO E2BIG ENOEXEC EBADF ECHILD EAGAIN ENOMEM EACCES EFAULT EBUSY EEXIST EXDEV ENODEV
ENOTDIR EISDIR ENFILE EMFILE ENOTTY EFBIG ENOSPC ESPIPE EROFS EMLINK EPIPE EDEADLK EDEADLOCK ENAMETOOLONG ENOLCK ENOSYS ENOTEMPTY EINVAL
STRUNCATE EADDRINUSE EADDRNOTAVAIL EAFNOSUPPORT EALREADY EBADMSG ECANCELED ECONNABORTED ECONNREFUSED ECONNRESET EDESTADDRREQ EHOSTUNREACH
EIDRM EINPROGRESS EISCONN ELOOP EMSGSIZE ENETDOWN ENETRESET ENETUNREACH ENOBUFS ENODATA ENOLINK ENOMSG ENOPROTOOPT ENOSR ENOSTR ENOTCONN
ENOTRECOVERABLE ENOTSOCK ENOTSUP EOPNOTSUPP EOTHER EOVERFLOW EOWNERDEAD EPROTO EPROTONOSUPPORT EPROTOTYPE ETIME ETIMEDOUT ETXTBSY EWOULDBLOCK
'''.split())


class GenError(Exception):
    """Unexpected input: the message starts with path:line:col: when a source position is known."""


def _fail(cu, index, message):
    line, col = cu.tokens.loc(index)
    return GenError(f'{cu.path}:{line}:{col}: {message}')


def cpp_identifier(java_name):
    """The C++ spelling of a Java identifier.

    A C++ keyword or unremovable macro gets a trailing underscore (CONVENTIONS keyword rule). A name that is reserved in C++ (a leading underscore
    followed by an upper-case letter, or a double underscore anywhere) loses its leading underscores, has each run of underscores reduced to one
    and gets a trailing underscore instead (_STR_X -> STR_X_). Callers check that the results stay distinct.
    """
    if java_name in CPP_KEYWORDS or java_name in CPP_MACROS:
        return java_name + '_'
    if re.match(r'_[A-Z]', java_name) or '__' in java_name:
        return re.sub(r'_+', '_', java_name.lstrip('_')) + '_'
    return java_name


def trailing_comment(cu, token_index):
    """The text of a '//' comment that follows the token on the same source line, or ''."""
    source = cu.tokens.source
    pos = cu.tokens.end[token_index]
    eol = source.find('\n', pos)
    stripped = source[pos:eol if eol >= 0 else len(source)].strip()
    if stripped.startswith('//'):
        return stripped[2:].strip()
    if stripped and not stripped.startswith('/*'):
        raise _fail(cu, token_index, f'unexpected text after the declaration: {stripped!r}')
    return ''


def ascii_comment(cu, index, text):
    text = text.rstrip('\\ \t')
    if any(not (0x20 <= ord(c) < 0x7F) for c in text):
        raise _fail(cu, index, f'non-ASCII or control character in comment {text!r}')
    return text


def javadoc_lines(cu, index, doc):
    """The lines of a javadoc comment without the comment markers, trailing empty lines removed."""
    if doc is None:
        return []
    body = doc.strip()
    if not (body.startswith('/**') and body.endswith('*/')):
        raise _fail(cu, index, 'unexpected javadoc format')
    lines = []
    for raw in body[3:-2].replace('\r\n', '\n').split('\n'):
        line = raw.strip()
        if line.startswith('*'):
            line = line[1:]
            if line.startswith(' '):
                line = line[1:]
        lines.append(ascii_comment(cu, index, line.rstrip()))
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return lines


def parse(java_dir):
    """[(java name, value, comment)] in source order, plus the class javadoc lines."""
    path = os.path.join(java_dir, DIALOG_ACTION_JAVA)
    if not os.path.isfile(path):
        raise GenError(f'missing Java source: {path}')
    cu = javasrc.parse_file(path)
    if len(cu.types) != 1 or cu.types[0].name != 'DialogAction' or cu.types[0].kind != 'class':
        raise GenError(f'{path}: expected a single class DialogAction')
    td = cu.types[0]
    if td.types or td.enum_constants or td.extends or td.implements:
        raise _fail(cu, td.index, 'unexpected member types or supertypes')

    constants = []
    for f in td.fields:
        mods = set(f.modifiers)
        if 'public' not in mods:
            if f.name == 'nameById' and mods == {'private', 'static', 'final'}:
                continue
            raise _fail(cu, f.index, f'unexpected non-public field {f.name}')
        if mods != {'public', 'static', 'final'} or str(f.type) != 'int' or f.annotations:
            raise _fail(cu, f.index, f'public field {f.name} is not a plain public static final int')
        if f.initializer is None:
            raise _fail(cu, f.index, f'{f.name} has no initializer')
        texts = f.initializer.texts()
        kinds = cu.tokens.kind[f.initializer.start:f.initializer.end]
        if len(texts) == 1 and kinds[0] == javasrc.INT:
            value = javasrc.literal_value(javasrc.INT, texts[0])
        elif len(texts) == 2 and texts[0] == '-' and kinds[1] == javasrc.INT:
            value = -javasrc.literal_value(javasrc.INT, texts[1])
        else:
            raise _fail(cu, f.index, f'{f.name} must be initialized with an int literal, found {f.initializer.text!r}')
        if texts[-1][-1:] in 'lL' or not -2**31 <= value < 2**31:
            raise _fail(cu, f.index, f'{f.name} = {f.initializer.text} is not an int')
        # the statement ends with the ';' after the initializer
        end = f.initializer.end
        if cu.tokens.text[end] != ';':
            raise _fail(cu, f.index, 'expected one declarator per statement')
        constants.append((f.name, value, trailing_comment(cu, end), f.index))
    if not constants:
        raise _fail(cu, td.index, 'no constants found')

    inits = td.initializers
    if len(inits) != 1 or not inits[0].static or ' '.join(inits[0].body.texts()) != EXPECTED_INITIALIZER:
        raise _fail(cu, td.index, 'the static initializer changed; review the generator (duplicate check / name map) before accepting it')
    methods = {m.name: m for m in td.methods}
    ctor = [m for m in td.methods if m.kind == 'constructor']
    name_of = methods.get('nameOf')
    if (len(td.methods) != 2 or len(ctor) != 1 or 'private' not in ctor[0].modifiers or name_of is None
            or ' '.join(name_of.body.texts()) != EXPECTED_NAME_OF or [str(p.type) for p in name_of.params] != ['int']):
        raise _fail(cu, td.index, 'expected exactly a private constructor and nameOf(int) returning nameById.get(id)')

    # Java's static initializer: duplicate ids throw
    by_id = {}
    for name, value, _, index in constants:
        if value in by_id:
            raise _fail(cu, index, f'Duplicate id {value}: {name}, {by_id[value]}')
        by_id[value] = name
    # distinct C++ spellings (a renamed NULL_ must not collide with a Java NULL_)
    spellings = {}
    for name, _, _, index in constants:
        cpp = cpp_identifier(name)
        if cpp in spellings:
            raise _fail(cu, index, f'{name} and {spellings[cpp]} have the same C++ name {cpp}')
        spellings[cpp] = name
    return cu, constants, javadoc_lines(cu, td.index, td.doc)


HEADER_NOTE = '// Generated by cpp/tools/gen/dialogaction.py - do not edit. Regenerate with: python cpp/tools/gen/dialogaction.py'


def render_header(constants, class_doc):
    authors = [line for line in class_doc if line.startswith('@author')]
    description = [line for line in class_doc if not line.startswith('@')]
    while description and not description[-1]:
        description.pop()
    renamed = [(name, cpp_identifier(name)) for name, _, _, _ in constants if cpp_identifier(name) != name]
    out = [HEADER_NOTE,
           f'// Source: game-server/{DIALOG_ACTION_JAVA} ({len(constants)} constants)',
           '#pragma once',
           '',
           '#include <cstdint>',
           '#include <optional>',
           '#include <span>',
           '#include <string_view>',
           '',
           '/**']
    out += [f' * {line}'.rstrip() for line in description]
    out += [' * <p>',
            ' * C++: a namespace of constants instead of a final class, so that both DialogAction::QUEST_SELECT and using-directives work.',
            ' * The table behind nameOf is sorted by id and checked for duplicate ids at compile time (Java: the static initializer).']
    if renamed:
        out.append(' * Renamed because the Java name is a C++ keyword or macro: ' + ', '.join(f'{j} -> {c}' for j, c in renamed) + '.')
    out += [' * <p>',
            ' * Java: com.aionemu.gameserver.model.DialogAction',
            ' *']
    out += [f' * {line}' for line in authors]
    out += [' */',
            'namespace aion::gameserver::model::DialogAction {',
            '']
    for name, value, comment, _ in constants:
        line = f'inline constexpr int32_t {cpp_identifier(name)} = {value};'
        notes = []
        if cpp_identifier(name) != name:
            notes.append(f'Java: {name}')
        if comment:
            notes.append(comment)
        if notes:
            line += ' // ' + '; '.join(notes)
        out.append(line)
    out += ['',
            '/** C++ addition: one constant, with its Java field name */',
            'struct Entry {',
            '\tint32_t id;',
            '\tstd::string_view name;',
            '};',
            '',
            '/** Java: nameOf(dialogActionId) - the Java name of the constant with this id, or std::nullopt (Java: null) */',
            'std::optional<std::string_view> nameOf(int32_t dialogActionId) noexcept;',
            '',
            '/** C++ addition: all constants sorted by id */',
            'std::span<const Entry> entries() noexcept;',
            '',
            '} // namespace aion::gameserver::model::DialogAction',
            '']
    return '\n'.join(out)


def render_source(constants):
    out = [HEADER_NOTE,
           f'// Source: game-server/{DIALOG_ACTION_JAVA}',
           '#include "aion/gameserver/model/DialogAction.h"',
           '',
           '#include <algorithm>',
           '#include <array>',
           '',
           'namespace aion::gameserver::model::DialogAction {',
           '',
           'namespace {',
           '',
           '// Java: nameById, as a table sorted by id',
           f'constexpr std::array<Entry, {len(constants)}> ENTRIES = {{{{']
    for name, value, _, _ in sorted(constants, key=lambda c: c[1]):
        out.append(f'\t{{{cpp_identifier(name)}, "{name}"}},')
    out += ['}};',
            '',
            'constexpr bool strictlyIncreasingIds() noexcept {',
            '\tfor (size_t i = 1; i < ENTRIES.size(); i++) {',
            '\t\tif (ENTRIES[i - 1].id >= ENTRIES[i].id)',
            '\t\t\treturn false;',
            '\t}',
            '\treturn true;',
            '}',
            '',
            '// Java: the static initializer throws IllegalArgumentException("Duplicate id ...")',
            'static_assert(strictlyIncreasingIds(), "DialogAction: duplicate id");',
            '',
            '} // namespace',
            '',
            'std::optional<std::string_view> nameOf(int32_t dialogActionId) noexcept {',
            '\tconst auto it = std::ranges::lower_bound(ENTRIES, dialogActionId, {}, &Entry::id);',
            '\tif (it == ENTRIES.end() || it->id != dialogActionId)',
            '\t\treturn std::nullopt;',
            '\treturn it->name;',
            '}',
            '',
            'std::span<const Entry> entries() noexcept {',
            '\treturn ENTRIES;',
            '}',
            '',
            '} // namespace aion::gameserver::model::DialogAction',
            '']
    return '\n'.join(out)


def generate(java_dir):
    """Returns ({output relpath: content}, summary dict)."""
    _, constants, class_doc = parse(os.path.abspath(java_dir))
    outputs = {HEADER_OUT: render_header(constants, class_doc), SOURCE_OUT: render_source(constants)}
    summary = {
        'constants': len(constants),
        'min_id': min(v for _, v, _, _ in constants),
        'max_id': max(v for _, v, _, _ in constants),
        'renamed': [f'{n} -> {cpp_identifier(n)}' for n, _, _, _ in constants if cpp_identifier(n) != n],
    }
    return outputs, summary


def write_outputs(outputs, out_dir, check):
    """Writes (or with check compares) the outputs; returns the relpaths that differ."""
    differ = []
    for rel, content in sorted(outputs.items()):
        path = os.path.join(out_dir, rel)
        old = None
        if os.path.isfile(path):
            with open(path, encoding='utf-8', newline='') as f:
                old = f.read()
        if old == content:
            continue
        differ.append(rel)
        if not check:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(content)
    return differ


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--java-dir', default=DEFAULT_JAVA_DIR, help='the Java game-server directory')
    ap.add_argument('--out-dir', default=DEFAULT_OUT_DIR, help='cpp/game-server/src/aion/gameserver')
    ap.add_argument('--check', action='store_true', help='exit 1 if a committed output is out of date')
    args = ap.parse_args(argv)
    try:
        outputs, summary = generate(args.java_dir)
    except (GenError, javasrc.JavaSyntaxError) as e:
        print(f'error: {e}', file=sys.stderr)
        return 2
    differ = write_outputs(outputs, args.out_dir, args.check)
    for k, v in summary.items():
        print(f'{k}: {v}')
    for rel in differ:
        print(f'{"out of date" if args.check else "written"}: {rel}')
    return 1 if args.check and differ else 0


if __name__ == '__main__':
    sys.exit(main())
