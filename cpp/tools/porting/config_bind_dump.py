"""config_bind_dump.py: the bound config keys of the C++ config classes, read from their bind functions' source.

Every AION_BIND / AION_BIND_PATTERN of the game server's config classes (game-server/src/aion/gameserver/configs/**.cpp) and every
processor.bind / processor.bindNamed of commons' (commons/src/aion/commons/configs/*.cpp), in the bind functions' order, one row per call:

    <class>\t<bind|pattern>\t<field>\t<key or key pattern>\t<default, or "-" for none>

The default is the C++ string literal's value (adjacent literals joined, escapes decoded); classes are sorted by name. The golden file
game-server/tests/configs/config_binds.golden.txt was written from the tree before ConfigurableProcessor::describe existed
(C2 cp4, 2026-10-18); ConfigDescribeTest dumps the same rows from describe() and compares them with it.

usage: python tools/porting/config_bind_dump.py [--write]   (prints the rows; --write replaces the golden file)
"""
import re
import sys
from pathlib import Path

CPP = Path(__file__).resolve().parents[2]
GOLDEN = CPP / 'game-server/tests/configs/config_binds.golden.txt'
CALL = re.compile(r'\b(AION_BIND_PATTERN|AION_BIND|processor\.bindNamed|processor\.bindPatternNamed|processor\.bind|p\.bind)\s*\(')
ESCAPES = {'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'", 'r': '\r', '0': '\0'}


def split_args(text, start):
    """the comma separated arguments of the call whose '(' is at text[start - 1]; returns (args, end)"""
    args, cur, depth, i = [], '', 0, start
    while i < len(text):
        c = text[i]
        if c == '"':
            j = i + 1
            while text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            cur += text[i:j + 1]
            i = j + 1
            continue
        if c == '/' and text[i:i + 2] == '//':
            i = text.index('\n', i)
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                args.append(cur.strip())
                return args, i + 1
            depth -= 1
        elif c == ',' and depth == 0:
            args.append(cur.strip())
            cur = ''
            i += 1
            continue
        cur += c
        i += 1
    raise ValueError('unterminated call')


def literal(arg):
    """the value of a C++ string literal argument (adjacent literals joined)"""
    out, i = '', 0
    arg = arg.strip()
    while i < len(arg):
        if arg[i].isspace():
            i += 1
            continue
        if arg[i] != '"':
            raise ValueError('not a string literal: ' + arg)
        i += 1
        while arg[i] != '"':
            if arg[i] == '\\' and arg[i + 1] in 'uU':
                width = 4 if arg[i + 1] == 'u' else 8
                out += chr(int(arg[i + 2:i + 2 + width], 16))
                i += 2 + width
            elif arg[i] == '\\':
                out += ESCAPES[arg[i + 1]]
                i += 2
            else:
                out += arg[i]
                i += 1
        i += 1
    return out


def rows():
    files = sorted((CPP / 'game-server/src/aion/gameserver/configs').rglob('*.cpp')) + sorted((CPP / 'commons/src/aion/commons/configs').glob('*.cpp'))
    by_class = {}
    for f in files:
        text = f.read_text(encoding='utf-8')
        found = []
        for m in CALL.finditer(text):
            name = m.group(1)
            if name.startswith('#'):
                continue
            line_start = text.rfind('\n', 0, m.start()) + 1
            if text[line_start:m.start()].lstrip().startswith(('//', '*', '#define')):
                continue
            args, _ = split_args(text, m.end())
            if name in ('AION_BIND', 'AION_BIND_PATTERN'):
                args = args[1:]  # the processor
                field, key = args[1], literal(args[0])
                rest = args[2:]
            elif name in ('processor.bindNamed', 'processor.bindPatternNamed'):
                field, key = literal(args[0]), literal(args[1])
                rest = args[3:]
            else:
                field, key = args[1], literal(args[0])
                rest = args[2:]
            kind = 'pattern' if 'PATTERN' in name or 'Pattern' in name else 'bind'
            default = literal(rest[0]) if rest else None
            found.append((kind, field, key, default))
        if found:
            by_class[f.stem] = found
    out = []
    for cls in sorted(by_class):
        for kind, field, key, default in by_class[cls]:
            esc = lambda s: s.replace('\\', '\\\\').replace('\t', '\\t').replace('\n', '\\n')
            out.append(f'{cls}\t{kind}\t{field}\t{esc(key)}\t{"-" if default is None else esc(default)}')
    return out


if __name__ == '__main__':
    result = rows()
    if '--write' in sys.argv:
        GOLDEN.write_text('\n'.join(result) + '\n', encoding='utf-8', newline='\n')
        print(f'{len(result)} rows written to {GOLDEN.relative_to(CPP)}')
    else:
        print('\n'.join(result))
