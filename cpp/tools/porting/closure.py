"""closure.py: the phase-5 closure census of m5j-plan.md §10.6 (item I-04, decision D11).

Fails (exit 1) when
  - an AION_UNPORTED or AION_PARTIAL site in a phase-5 chunk (P5-*) or in C1/C2 has no row in the allow-list, or the row's count is off,
  - an allow-list row names no owner, or no site matches it any more (a stale row),
  - an opcode registered in ClientPacketInfo.gen.inc has no C++ class (clientpackets/<Class>.cpp) and no `opcode` row with an owner.

The allow-list (docs/porting/phase5-allowlist.txt) has one row per line, `#` comments and blank lines ignored:
  site|<path below cpp/game-server>|<enclosing function>|<count>|<owner>
  opcode|<CM_ class>|<owner>
The owner names the milestone, chunk, decision or config key that takes the work (a phase-6 chunk, a user-deferred feature, a config-off
feature with its key, a pull request that carries the body).

Sites are found the way census.py counts them: a `AION_UNPORTED(` or `AION_PARTIAL(` call outside comments, in the .cpp/.h/.inc files below
game-server/src and game-server/handlers; the macro definitions in runtime/base/Unported.h are not sites. The enclosing function is the last
line above the site that starts at column 0 with a qualified name followed by `(`.

Usage: python tools/porting/closure.py [--check] [--list] [--allowlist PATH]
"""
import argparse
import collections
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CPP = HERE.parent.parent
GAME_SERVER = CPP / 'game-server'
DEFAULT_ALLOWLIST = CPP / 'docs' / 'porting' / 'phase5-allowlist.txt'
OPCODES = GAME_SERVER / 'src' / 'aion' / 'gameserver' / 'network' / 'aion' / 'ClientPacketInfo.gen.inc'
CLIENTPACKETS = GAME_SERVER / 'src' / 'aion' / 'gameserver' / 'network' / 'aion' / 'clientpackets'
MACRO_FILE = 'src/aion/gameserver/runtime/base/Unported.h'

sys.path.insert(0, str(HERE))
import chunks  # noqa: E402

SITE = re.compile(r'\bAION_(UNPORTED|PARTIAL)\s*\(')
DEFINITION = re.compile(r'^[A-Za-z_][^;{}]*?\b((?:[A-Za-z_]\w*::)+~?[A-Za-z_]\w*)\s*\(')
OPCODE_ROW = re.compile(r'^AION_CLIENT_PACKET_INFO\(\d+,\s*0x[0-9A-Fa-f]+,\s*(CM_\w+)')


def in_scope(chunk):
    return chunk.startswith('P5-') or chunk in ('C1', 'C2')


def code_of(line, in_block):
    """the code of a line outside comments; (code, still in a block comment)"""
    out = []
    i = 0
    while i < len(line):
        if in_block:
            end = line.find('*/', i)
            if end < 0:
                return ''.join(out), True
            i = end + 2
            in_block = False
            continue
        if line.startswith('//', i):
            break
        if line.startswith('/*', i):
            in_block = True
            i += 2
            continue
        if line[i] == '"':  # a string literal: skipped whole (a site message may contain //)
            j = i + 1
            while j < len(line) and line[j] != '"':
                j += 2 if line[j] == '\\' else 1
            out.append(' ')
            i = j + 1
            continue
        out.append(line[i])
        i += 1
    return ''.join(out), in_block


def find_sites(text):
    """[(line number, kind, enclosing function)] of a file's text"""
    sites = []
    lines = text.split('\n')
    in_block = False
    function = '?'
    for number, line in enumerate(lines, 1):
        code, in_block_after = code_of(line, in_block)
        if not in_block:
            d = DEFINITION.match(line)
            if d:
                function = d.group(1)
        in_block = in_block_after
        if '#define' in code:
            continue
        for m in SITE.finditer(code):
            sites.append((number, m.group(1), function))
    return sites


def scan(game_server=GAME_SERVER, manifest=None):
    """{(path, function): [(line, kind)]} of the in-scope sites, and {chunk: count}"""
    manifest = manifest or chunks.Manifest.load(str(game_server / 'chunks.cmake'))
    found = collections.defaultdict(list)
    per_chunk = collections.Counter()
    for root in ('src', 'handlers'):
        for f in sorted(p for p in (game_server / root).rglob('*') if p.suffix in ('.cpp', '.h', '.inc')):
            rel = f.relative_to(game_server).as_posix()
            if rel == MACRO_FILE:
                continue
            text = f.read_text(encoding='utf-8', errors='replace')
            if 'AION_UNPORTED' not in text and 'AION_PARTIAL' not in text:
                continue
            owners, _ = chunks.owners_of(manifest, rel, str(game_server))
            chunk = owners[0].name if owners else 'no owner'
            if not in_scope(chunk):
                continue
            for line, kind, function in find_sites(text):
                found[(rel, function)].append((line, kind))
                per_chunk[chunk] += 1
    return found, per_chunk


def missing_opcodes(opcodes=OPCODES, clientpackets=CLIENTPACKETS):
    classes = []
    for line in opcodes.read_text(encoding='utf-8').split('\n'):
        m = OPCODE_ROW.match(line.strip())
        if m:
            classes.append(m.group(1))
    return [c for c in classes if not (clientpackets / f'{c}.cpp').is_file()]


def read_allowlist(path):
    """(sites {(path, function): (count, owner)}, opcodes {class: owner}, problems)"""
    sites, opcodes, problems = {}, {}, []
    for number, raw in enumerate(Path(path).read_text(encoding='utf-8').split('\n'), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        fields = [f.strip() for f in line.split('|')]
        where = f'{path}:{number}'
        if fields[0] == 'site' and len(fields) == 5:
            _, rel, function, count, owner = fields
            if not count.isdigit():
                problems.append(f'{where}: the count "{count}" is no number')
                continue
            if not owner:
                problems.append(f'{where}: the row names no owner')
            if (rel, function) in sites:
                problems.append(f'{where}: a second row of {rel} {function}')
            sites[(rel, function)] = (int(count), owner)
        elif fields[0] == 'opcode' and len(fields) == 3:
            _, cls, owner = fields
            if not owner:
                problems.append(f'{where}: the row names no owner')
            opcodes[cls] = owner
        else:
            problems.append(f'{where}: not a `site|path|function|count|owner` or `opcode|class|owner` row')
    return sites, opcodes, problems


def check(found, missing, allowlist_path):
    sites, opcodes, problems = read_allowlist(allowlist_path)
    for key, occurrences in sorted(found.items()):
        row = sites.get(key)
        if row is None:
            lines = ', '.join(str(n) for n, _ in occurrences)
            problems.append(f'{key[0]}:{lines}: {len(occurrences)} site(s) in {key[1]} without an allow-list row')
        elif row[0] != len(occurrences):
            problems.append(f'{key[0]} {key[1]}: {len(occurrences)} site(s), the allow-list row says {row[0]}: set the row count to {len(occurrences)}')
    for key in sorted(set(sites) - set(found)):
        problems.append(f'{key[0]} {key[1]}: the allow-list row matches no site any more (stale): remove the row `site|{key[0]}|{key[1]}|...`')
    for cls in missing:
        if cls not in opcodes:
            problems.append(f'{cls}: a registered opcode without a C++ class and without an allow-list row')
    for cls in sorted(set(opcodes) - set(missing)):
        problems.append(f'{cls}: the allow-list row matches no missing class any more (stale): remove the row `opcode|{cls}|...`')
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(prog='closure.py', description=__doc__.split('\n', 1)[0])
    ap.add_argument('--allowlist', default=str(DEFAULT_ALLOWLIST))
    ap.add_argument('--list', action='store_true', help='print every in-scope site and missing opcode (allow-list rows, owner empty)')
    ap.add_argument('--check', action='store_true', help='compare with the allow-list (the default)')
    args = ap.parse_args(argv)
    found, per_chunk = scan()
    missing = missing_opcodes()
    if args.list:
        for (rel, function), occurrences in sorted(found.items()):
            print(f'site|{rel}|{function}|{len(occurrences)}|')
        for cls in missing:
            print(f'opcode|{cls}|')
        return 0
    problems = check(found, missing, args.allowlist)
    total = sum(len(v) for v in found.values())
    summary = ', '.join(f'{c} {n}' for c, n in sorted(per_chunk.items()))
    print(f'closure: {total} sites in {len(found)} functions ({summary}); {len(missing)} registered opcodes without a C++ class')
    for p in problems:
        print(f'closure: {p}')
    print('closure: OK' if not problems else f'closure: {len(problems)} problem(s)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
