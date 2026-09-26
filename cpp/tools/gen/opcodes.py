"""opcodes: generates the game server's opcode tables from the Java sources.

Inputs (under the Java game-server directory, default: the repository's game-server/):
    src/com/aionemu/gameserver/network/aion/ServerPacketsOpcodes.java      server packet class -> opcode (237 entries)
    src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java   client opcode -> packet class + allowed states (PacketInfo[250])
    src/com/aionemu/gameserver/network/aion/AionConnection.java            the State enum (allowed states are printed in ordinal order)
    src/com/aionemu/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.java   INTERNAL_VERSION (opcode obfuscation base)
    src/com/aionemu/gameserver/network/Crypt.java                          checked: the obfuscation formulas this generator reproduces
    src/com/aionemu/gameserver/network/aion/{serverpackets,clientpackets}/*.java   checked: every registered class exists, is concrete, derives
                                                                          from AionServerPacket/AionClientPacket and (client packets) has the
                                                                          public (int, Set) constructor that PacketInfo looks up reflectively

Outputs (under cpp/game-server/src/aion/gameserver/, committed; see docs/design/handlers-and-porting-plan.md §2.3):
    network/aion/ServerPacketsOpcodes.gen.h   forward declarations of the registered SM classes, `opcodeOf<SM_X>` constants (a class without an
                                              opcode does not compile), and ServerPacketsOpcodes::ENTRIES {opcode, wireOpcode, name, clientName}
                                              sorted by opcode, with INTERNAL_VERSION
    network/aion/ClientPacketInfo.gen.inc     X-macro list AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, "clientName", STATE...) sorted by
                                              opcode, plus AION_CLIENT_PACKET_TABLE_SIZE(250) when that macro is defined

wireOpcode is the 16-bit value on the wire: for server packets (opcode + INTERNAL_VERSION) ^ 0xDF (Crypt.encodeServerPacketOpcode), for client
packets the unique 16-bit value w with Crypt.decodeClientPacketOpcode(w) == opcode, i.e. (((opcode + INTERNAL_VERSION) ^ 0xEF) + 0x0C) ^ 0xEF.
clientName is the bracketed client-side name from the trailing Java comment ("S_KEY", "C_VERSION (VersionPacket)"), or empty.

Java semantics reproduced as checks (the generator fails instead of guessing):
    ServerPacketsOpcodes.addPacketOpcode: negative opcodes are skipped, a duplicate opcode throws IllegalArgumentException; a class registered
    twice would silently keep only its last opcode in Java and is rejected here.
    AionClientPacketFactory: an index outside the array throws ArrayIndexOutOfBoundsException; assigning an index twice would silently keep the
    last PacketInfo in Java and is rejected here, as are duplicate states (EnumSet.of would ignore them).

Usage: python opcodes.py [--java-dir GAME_SERVER_DIR] [--out-dir CPP_GAME_SERVER_SRC] [--check]
    --check   compare instead of writing; exit 1 if a committed output differs (drift test)
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

PKG_DIR = 'src/com/aionemu/gameserver/network'
SERVER_OPCODES_JAVA = PKG_DIR + '/aion/ServerPacketsOpcodes.java'
CLIENT_FACTORY_JAVA = PKG_DIR + '/aion/AionClientPacketFactory.java'
CONNECTION_JAVA = PKG_DIR + '/aion/AionConnection.java'
VERSION_CHECK_JAVA = PKG_DIR + '/aion/serverpackets/SM_VERSION_CHECK.java'
CRYPT_JAVA = PKG_DIR + '/Crypt.java'
SERVER_PACKETS_DIR = PKG_DIR + '/aion/serverpackets'
CLIENT_PACKETS_DIR = PKG_DIR + '/aion/clientpackets'

SERVER_OPCODES_OUT = 'network/aion/ServerPacketsOpcodes.gen.h'
CLIENT_INFO_OUT = 'network/aion/ClientPacketInfo.gen.inc'

# Token texts of the Java method bodies whose semantics this generator reproduces; any change there must be reviewed here first.
EXPECTED_BODIES = {
    (CRYPT_JAVA, 'encodeServerPacketOpcode'): '{ return ( opcode + SM_VERSION_CHECK . INTERNAL_VERSION ) ^ 0xDF ; }',
    (CRYPT_JAVA, 'decodeClientPacketOpcode'): '{ return ( ( opcode ^ 0xEF ) - 0xC ^ 0xEF ) - SM_VERSION_CHECK . INTERNAL_VERSION ; }',
    (SERVER_OPCODES_JAVA, 'addPacketOpcode'): '{ if ( opcode < 0 ) return ; if ( opcodes . values ( ) . contains ( opcode ) ) throw new '
                                              'IllegalArgumentException ( String . format ( "There already exists another packet with id 0x%02X" '
                                              ', opcode ) ) ; opcodes . put ( packetClass , opcode ) ; }',
    (SERVER_OPCODES_JAVA, 'getOpcode'): '{ Integer opcode = opcodes . get ( packetClass ) ; if ( opcode == null ) throw new '
                                        'IllegalArgumentException ( "There is no opcode for " + packetClass + " defined." ) ; return opcode ; }',
}
EXPECTED_TRY_CREATE_PREFIX = ('State state = client . getState ( ) ; int opcode = Crypt . decodeClientPacketOpcode ( data . getShort ( ) & 0xffff ) '
                              '; data . position ( data . position ( ) + 3 ) ; PacketInfo < ? extends AionClientPacket > packetInfo = opcode < 0 || '
                              'opcode >= packets . length ? null : packets [ opcode ] ;')


class GenError(Exception):
    """Unexpected input: the message starts with path:line: when a source position is known."""


def _fail(cu, index, message):
    line, col = cu.tokens.loc(index)
    return GenError(f'{cu.path}:{line}:{col}: {message}')


def trailing_comment(cu, token_index):
    """The text of a '//' comment that follows the token on the same source line, or ''."""
    tokens = cu.tokens
    source = tokens.source
    pos = tokens.end[token_index]
    eol = source.find('\n', pos)
    rest = source[pos:eol if eol >= 0 else len(source)]
    stripped = rest.strip()
    if stripped.startswith('//'):
        return stripped[2:].strip()
    if stripped and not stripped.startswith('/*'):
        raise _fail(cu, token_index, f'unexpected text after the statement: {stripped!r}')
    return ''


def bracketed_name(comment):
    m = re.match(r'\[([^\]]*)\]', comment)
    return m.group(1).strip() if m else ''


def cpp_string(text):
    """A C++ string literal for printable ASCII text."""
    if any(not (0x20 <= ord(c) < 0x7F) for c in text):
        raise GenError(f'non-printable or non-ASCII character in {text!r}')
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def cpp_comment(text):
    """Text safe for a '//' comment (no line continuation, ASCII)."""
    text = text.rstrip('\\ ')
    if any(not (0x20 <= ord(c) < 0x7F) and c != '\t' for c in text):
        raise GenError(f'non-ASCII character in comment {text!r}')
    return text


def split_statements(cu, start, end):
    """Token index ranges [s, e) of the ';'-terminated statements between start and end (exclusive), at bracket depth 0."""
    tokens = cu.tokens
    result = []
    i = start
    s = start
    while i < end:
        m = tokens.match[i]
        if tokens.kind[i] == javasrc.OP and tokens.text[i] in ('(', '[', '{') and m >= 0:
            i = m + 1
            continue
        if tokens.text[i] == ';':
            result.append((s, i + 1))
            s = i + 1
        i += 1
    if s != end:
        raise _fail(cu, s, 'statement without terminating ;')
    return result


def match_tokens(cu, start, end, pattern):
    """Matches token texts against pattern items: a literal text, INT (captures int), IDENT (captures name). Returns captures or None."""
    tokens = cu.tokens
    if end - start != len(pattern):
        return None
    captures = []
    for i, p in zip(range(start, end), pattern):
        kind, text = tokens.kind[i], tokens.text[i]
        if p == 'INT':
            if kind != javasrc.INT:
                return None
            captures.append(javasrc.literal_value(kind, text))
        elif p == 'IDENT':
            if kind != javasrc.IDENT:
                return None
            captures.append(text)
        elif text != p:
            return None
    return captures


def body_text(span):
    return ' '.join(span.texts())


def find_method(cu, td, name):
    found = [m for m in td.methods if m.name == name]
    if len(found) != 1:
        raise _fail(cu, td.index, f'expected exactly one method {name} in {td.name}, found {len(found)}')
    return found[0]


def check_body(java_dir, units, key):
    path, method = key
    cu = units[path]
    m = find_method(cu, cu.types[0], method)
    actual = body_text(m.body)
    if actual != EXPECTED_BODIES[key]:
        raise _fail(cu, m.index, f'body of {method} changed; review the generator before accepting it:\n  expected {EXPECTED_BODIES[key]}\n'
                                 f'  actual   {actual}')


def parse(java_dir, relpath, units):
    if relpath not in units:
        path = os.path.join(java_dir, relpath)
        if not os.path.isfile(path):
            raise GenError(f'missing Java source: {path}')
        units[relpath] = javasrc.parse_file(path)
    return units[relpath]


def static_int_constant(cu, td, name):
    fields = [f for f in td.fields if f.name == name]
    if len(fields) != 1:
        raise _fail(cu, td.index, f'expected one field {name}')
    f = fields[0]
    if not {'public', 'static', 'final'} <= set(f.modifiers) or str(f.type) != 'int' or f.initializer is None:
        raise _fail(cu, f.index, f'{name} must be a public static final int with an initializer')
    t = f.initializer
    if t.end - t.start != 1 or cu.tokens.kind[t.start] != javasrc.INT:
        raise _fail(cu, f.index, f'{name} must be initialized with an int literal')
    return javasrc.literal_value(javasrc.INT, cu.tokens.text[t.start])


def the_static_initializer(cu, td):
    inits = [i for i in td.initializers if i.static]
    if len(inits) != 1 or any(not i.static for i in td.initializers):
        raise _fail(cu, td.index, f'expected exactly one static initializer in {td.name}')
    return inits[0].body


# ----------------------------------------------------------------------------------------------------------------------------------------------
# Class checks
# ----------------------------------------------------------------------------------------------------------------------------------------------

def check_packet_class(java_dir, units, directory, name, base, cu_ref, ref_index, need_constructor):
    relpath = f'{directory}/{name}.java'
    if not os.path.isfile(os.path.join(java_dir, relpath)):
        raise _fail(cu_ref, ref_index, f'{name} is registered but {relpath} does not exist')
    seen = []
    current = name
    first = True
    while True:
        cu = parse(java_dir, f'{directory}/{current}.java', units)
        tds = [t for t in cu.types if t.name == current]
        if len(tds) != 1 or tds[0].kind != 'class':
            raise _fail(cu_ref, ref_index, f'{relpath} does not declare class {current}')
        td = tds[0]
        if first:
            if 'abstract' in td.modifiers:
                raise _fail(cu_ref, ref_index, f'{name} is abstract and cannot be instantiated')
            if need_constructor:
                ctors = [m for m in td.methods if m.kind == 'constructor' and 'public' in m.modifiers and len(m.params) == 2
                         and str(m.params[0].type) == 'int' and str(m.params[1].type).split('<')[0] in ('Set', 'java.util.Set')]
                if len(ctors) != 1:
                    raise _fail(cu_ref, ref_index, f'{name} has no public constructor (int, Set<State>) (PacketInfo uses getConstructor)')
            first = False
        if not td.extends:
            raise _fail(cu_ref, ref_index, f'{current} does not derive from {base}')
        parent = td.extends[0].name.split('.')[-1]
        if parent == base:
            return
        if parent in seen or not os.path.isfile(os.path.join(java_dir, f'{directory}/{parent}.java')):
            raise _fail(cu_ref, ref_index, f'{current} derives from {parent}, which is not {base} or a packet class of {directory}')
        seen.append(parent)
        current = parent


# ----------------------------------------------------------------------------------------------------------------------------------------------
# Parsing
# ----------------------------------------------------------------------------------------------------------------------------------------------

def parse_internal_version(java_dir, units):
    cu = parse(java_dir, VERSION_CHECK_JAVA, units)
    return static_int_constant(cu, cu.types[0], 'INTERNAL_VERSION')


def parse_states(java_dir, units):
    cu = parse(java_dir, CONNECTION_JAVA, units)
    td = cu.types[0]
    enums = [t for t in td.types if t.name == 'State' and t.kind == 'enum']
    if len(enums) != 1:
        raise _fail(cu, td.index, 'AionConnection.State enum not found')
    constants = [c.name for c in enums[0].enum_constants]
    if not constants or any(c.args or c.body for c in enums[0].enum_constants):
        raise _fail(cu, enums[0].index, 'unexpected State enum shape')
    return constants


def parse_server_opcodes(java_dir, units):
    """[(opcode, class name, comment, line)] in source order, with Java's duplicate checks."""
    cu = parse(java_dir, SERVER_OPCODES_JAVA, units)
    td = cu.types[0]
    for key in EXPECTED_BODIES:
        if key[0] == SERVER_OPCODES_JAVA:
            check_body(java_dir, units, key)
    body = the_static_initializer(cu, td)
    entries = []
    by_opcode = {}
    by_class = {}
    for s, e in split_statements(cu, body.start + 1, body.end - 1):
        negative = match_tokens(cu, s, e, ['addPacketOpcode', '(', '-', 'INT', ',', 'IDENT', '.', 'class', ')', ';'])
        captures = negative or match_tokens(cu, s, e, ['addPacketOpcode', '(', 'INT', ',', 'IDENT', '.', 'class', ')', ';'])
        if captures is None:
            raise _fail(cu, s, f'unexpected statement in the static initializer: {cu.tokens.source_text(s, e)}')
        opcode, name = captures
        if negative:
            opcode = -opcode
        if opcode > 0x7FFFFFFF:
            raise _fail(cu, s, 'opcode out of int range')
        comment = trailing_comment(cu, e - 1)
        line = cu.tokens.loc(s)[0]
        if opcode < 0:  # Java: addPacketOpcode ignores negative opcodes
            continue
        if opcode in by_opcode:
            raise _fail(cu, s, f'There already exists another packet with id 0x{opcode:02X} ({by_opcode[opcode]}, {name})')
        if name in by_class:
            raise _fail(cu, s, f'{name} is registered twice (opcodes {by_class[name]} and {opcode}); Java would keep only the last one')
        if not name.startswith('SM_'):
            raise _fail(cu, s, f'server packet class name {name} does not start with SM_')
        by_opcode[opcode] = name
        by_class[name] = opcode
        check_packet_class(java_dir, units, SERVER_PACKETS_DIR, name, 'AionServerPacket', cu, s, False)
        entries.append((opcode, name, comment, line))
    if not entries:
        raise _fail(cu, body.start, 'no server packet opcodes found')
    return entries


def parse_client_packets(java_dir, units, states):
    """(table size, [(opcode, class name, [states in ordinal order], comment, line)] in source order)."""
    cu = parse(java_dir, CLIENT_FACTORY_JAVA, units)
    td = cu.types[0]
    fields = [f for f in td.fields if f.name == 'packets']
    if len(fields) != 1 or fields[0].initializer is None:
        raise _fail(cu, td.index, 'field packets not found')
    size_captures = match_tokens(cu, fields[0].initializer.start, fields[0].initializer.end,
                                 ['new', 'PacketInfo', '<', '?', '>', '[', 'INT', ']'])
    if size_captures is None:
        raise _fail(cu, fields[0].index, 'packets must be initialized with new PacketInfo<?>[N]')
    table_size = size_captures[0]
    try_create = find_method(cu, td, 'tryCreatePacket')
    if not body_text(try_create.body).startswith('{ ' + EXPECTED_TRY_CREATE_PREFIX):
        raise _fail(cu, try_create.index, 'tryCreatePacket changed (opcode decoding or table lookup); review the generator')

    body = the_static_initializer(cu, td)
    t = cu.tokens
    # static { try { ... } catch (NoSuchMethodException e) { ... } }
    inner = body.start + 1
    if t.text[inner] != 'try' or t.text[inner + 1] != '{':
        raise _fail(cu, inner, 'expected static { try { ... } catch ... }')
    try_open = inner + 1
    try_close = t.match[try_open]
    after = t.text[try_close + 1:body.end - 1]
    if not after or after[0] != 'catch' or t.match[t.match[try_close + 2] + 1] != body.end - 2:
        raise _fail(cu, try_close, 'expected a single catch block after the try block')

    entries = []
    by_opcode = {}
    by_class = {}
    for s, e in split_statements(cu, try_open + 1, try_close):
        head = match_tokens(cu, s, s + 13, ['packets', '[', 'INT', ']', '=', 'new', 'PacketInfo', '<', '>', '(', 'IDENT', '.', 'class'])
        if head is None or t.text[e - 2] != ')' or t.text[e - 1] != ';' or t.match[s + 9] != e - 2:
            raise _fail(cu, s, f'unexpected statement in the static initializer: {t.source_text(s, e)}')
        opcode, name = head
        state_names = []
        i = s + 13
        while i < e - 2:
            captures = match_tokens(cu, i, i + 4, [',', 'State', '.', 'IDENT'])
            if captures is None:
                raise _fail(cu, i, f'expected ", State.X" in {t.source_text(s, e)}')
            state_names.append(captures[0])
            i += 4
        if not state_names:
            raise _fail(cu, s, f'{name} has no allowed state')
        for st in state_names:
            if st not in states:
                raise _fail(cu, s, f'unknown state {st}')
        if len(set(state_names)) != len(state_names):
            raise _fail(cu, s, f'duplicate state for {name}')
        if not 0 <= opcode < table_size:
            raise _fail(cu, s, f'opcode {opcode} outside the packets array (length {table_size})')
        if opcode in by_opcode:
            raise _fail(cu, s, f'packets[{opcode}] is assigned twice ({by_opcode[opcode]}, {name}); Java would keep only the last one')
        if name in by_class:
            raise _fail(cu, s, f'{name} is registered twice (opcodes {by_class[name]} and {opcode})')
        if not name.startswith('CM_'):
            raise _fail(cu, s, f'client packet class name {name} does not start with CM_')
        by_opcode[opcode] = name
        by_class[name] = opcode
        check_packet_class(java_dir, units, CLIENT_PACKETS_DIR, name, 'AionClientPacket', cu, s, True)
        ordered = [st for st in states if st in state_names]  # EnumSet iterates in ordinal order
        entries.append((opcode, name, ordered, trailing_comment(cu, e - 1), t.loc(s)[0]))
    if not entries:
        raise _fail(cu, body.start, 'no client packets found')
    return table_size, entries


# ----------------------------------------------------------------------------------------------------------------------------------------------
# Obfuscation (Crypt.java, checked by EXPECTED_BODIES)
# ----------------------------------------------------------------------------------------------------------------------------------------------

def _int32(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


def encode_server_opcode(opcode, internal_version):
    """Crypt.encodeServerPacketOpcode in int arithmetic."""
    return _int32((opcode + internal_version) ^ 0xDF)


def decode_client_opcode(opcode, internal_version):
    """Crypt.decodeClientPacketOpcode in int arithmetic (Java precedence: '-' binds tighter than '^')."""
    return _int32((((opcode ^ 0xEF) - 0xC) ^ 0xEF) - internal_version)


def client_wire_opcode(opcode, internal_version):
    """The unique 16-bit wire value that decodes to opcode, or GenError."""
    wire = (((opcode + internal_version) ^ 0xEF) + 0x0C) ^ 0xEF
    if not 0 <= wire <= 0xFFFF or decode_client_opcode(wire, internal_version) != opcode:
        raise GenError(f'client opcode {opcode} has no 16-bit wire value')
    return wire


# ----------------------------------------------------------------------------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------------------------------------------------------------------------

HEADER_NOTE = '// Generated by cpp/tools/gen/opcodes.py - do not edit. Regenerate with: python cpp/tools/gen/opcodes.py'


def render_server_opcodes(entries, internal_version):
    by_opcode = sorted(entries)
    names = sorted(name for _, name, _, _ in entries)
    out = [HEADER_NOTE,
           f'// Source: game-server/{SERVER_OPCODES_JAVA} ({len(entries)} server packets), INTERNAL_VERSION from SM_VERSION_CHECK.java.',
           '#pragma once',
           '',
           '#include <array>',
           '#include <cstdint>',
           '#include <string_view>',
           '',
           'namespace aion::gameserver::network::aion::serverpackets {',
           '']
    out += [f'class {n};' for n in names]
    out += ['',
            '} // namespace aion::gameserver::network::aion::serverpackets',
            '',
            'namespace aion::gameserver::network::aion {',
            '',
            'namespace detail {',
            '/** Deliberately undefined: opcodeOf<P> does not compile for a class without an opcode (Java: IllegalArgumentException at runtime) */',
            'template <class P>',
            'struct NoServerPacketOpcodeDefinedFor;',
            '} // namespace detail',
            '',
            '/**',
            ' * Java: ServerPacketsOpcodes.getOpcode(packetClass), as a constant. Needs only the forward declarations above.',
            ' * SM_CUSTOM_PACKET has no entry: its opcode is chosen at runtime.',
            ' */',
            'template <class P>',
            'inline constexpr int32_t opcodeOf = sizeof(detail::NoServerPacketOpcodeDefinedFor<P>);',
            '']
    for opcode, name, comment, _ in by_opcode:
        line = f'template <>\ninline constexpr int32_t opcodeOf<serverpackets::{name}> = {opcode};'
        if comment:
            line += f' // {cpp_comment(comment)}'
        out.append(line)
    out += ['',
            'namespace ServerPacketsOpcodes {',
            '',
            '/** Java: SM_VERSION_CHECK.INTERNAL_VERSION - client version number and base of the opcode obfuscation (Crypt) */',
            f'inline constexpr int32_t INTERNAL_VERSION = {internal_version};',
            '',
            '/** One registered server packet. wireOpcode = (opcode + INTERNAL_VERSION) ^ 0xDF, the first 16-bit word after the frame length. */',
            'struct Entry {',
            '\tint32_t opcode;',
            '\tuint16_t wireOpcode;',
            '\tstd::string_view name;',
            '\tstd::string_view clientName;',
            '};',
            '',
            '/** All registered server packets, sorted by opcode */',
            f'inline constexpr std::array<Entry, {len(entries)}> ENTRIES = {{{{']
    for opcode, name, comment, _ in by_opcode:
        wire = encode_server_opcode(opcode, internal_version)
        if not 0 <= wire <= 0xFFFF:
            raise GenError(f'server opcode {opcode} ({name}) does not fit into 16 bits after obfuscation')
        out.append(f'\t{{{opcode}, 0x{wire:04X}, "{name}", {cpp_string(bracketed_name(comment))}}},')
    out += ['}};',
            '',
            '/** @return the entry with the given opcode, or nullptr */',
            'constexpr const Entry* findByOpcode(int32_t opcode) noexcept {',
            '\tsize_t low = 0;',
            '\tsize_t high = ENTRIES.size();',
            '\twhile (low < high) {',
            '\t\tconst size_t mid = low + (high - low) / 2;',
            '\t\tif (ENTRIES[mid].opcode < opcode)',
            '\t\t\tlow = mid + 1;',
            '\t\telse',
            '\t\t\thigh = mid;',
            '\t}',
            '\treturn low < ENTRIES.size() && ENTRIES[low].opcode == opcode ? &ENTRIES[low] : nullptr;',
            '}',
            '',
            '} // namespace ServerPacketsOpcodes',
            '',
            '} // namespace aion::gameserver::network::aion',
            '']
    return '\n'.join(out)


def render_client_info(table_size, entries, internal_version, states):
    out = [HEADER_NOTE,
           f'// Source: game-server/{CLIENT_FACTORY_JAVA} ({len(entries)} client packets), states from AionConnection.State,',
           f'// wire opcodes from Crypt.decodeClientPacketOpcode with INTERNAL_VERSION {internal_version}.',
           '//',
           '// X-macro list, sorted by opcode. Define before including (this file has no include guard, include it once per expansion):',
           '//   AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, state...)',
           '//     opcode      index into Java\'s PacketInfo table, i.e. the decoded opcode',
           '//     wireOpcode  the 16-bit word after the frame length that Crypt::decodeClientPacketOpcode maps to opcode',
           '//     Class       simple name of the packet class in aion::gameserver::network::aion::clientpackets',
           '//     clientName  string literal: the bracketed client-side name from the Java comment, or ""',
           f'//     state...    one or more of {", ".join(states)} (AionConnection.State), in ordinal order',
           '//   AION_CLIENT_PACKET_TABLE_SIZE(size)   optional: the length of Java\'s PacketInfo table (opcodes are below it)',
           '#ifndef AION_CLIENT_PACKET_INFO',
           '#error "define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) before including ClientPacketInfo.gen.inc"',
           '#endif',
           '#ifdef AION_CLIENT_PACKET_TABLE_SIZE',
           f'AION_CLIENT_PACKET_TABLE_SIZE({table_size})',
           '#endif']
    for opcode, name, st, comment, _ in sorted(entries):
        wire = client_wire_opcode(opcode, internal_version)
        out.append(f'AION_CLIENT_PACKET_INFO({opcode}, 0x{wire:04X}, {name}, {cpp_string(bracketed_name(comment))}, {", ".join(st)})')
    out.append('')
    return '\n'.join(out)


def generate(java_dir):
    """Parses the Java sources and returns ({output relpath: content}, summary dict)."""
    java_dir = os.path.abspath(java_dir)
    units = {}
    parse(java_dir, CRYPT_JAVA, units)
    for key in EXPECTED_BODIES:
        if key[0] == CRYPT_JAVA:
            check_body(java_dir, units, key)
    internal_version = parse_internal_version(java_dir, units)
    states = parse_states(java_dir, units)
    server = parse_server_opcodes(java_dir, units)
    table_size, client = parse_client_packets(java_dir, units, states)
    outputs = {
        SERVER_OPCODES_OUT: render_server_opcodes(server, internal_version),
        CLIENT_INFO_OUT: render_client_info(table_size, client, internal_version, states),
    }
    state_counts = {}
    for _, _, st, _, _ in client:
        key = '+'.join(st)
        state_counts[key] = state_counts.get(key, 0) + 1
    summary = {
        'internal_version': internal_version,
        'server_packets': len(server),
        'max_server_opcode': max(o for o, _, _, _ in server),
        'client_packets': len(client),
        'client_table_size': table_size,
        'client_states': dict(sorted(state_counts.items())),
    }
    return outputs, summary


def write_outputs(outputs, out_dir, check):
    """Writes (or with check compares) the outputs; returns the list of relpaths that differ."""
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
