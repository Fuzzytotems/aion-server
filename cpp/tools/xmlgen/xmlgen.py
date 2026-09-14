"""xmlgen: static data generator (JAXB replacement, docs/design/static-data.md §1-2 and amendments). Python 3.12, stdlib only.

Commands (run from anywhere; paths default to the repository layout):
    python cpp/tools/xmlgen/xmlgen.py generate [--out DIR]      writes the generated tree (default cpp/game-server/generated) and prunes
                                                                 stale generated files (never touches generated/concurrency)
    python cpp/tools/xmlgen/xmlgen.py check [--out DIR]         regenerates in memory and compares with DIR; exit 1 on drift
    python cpp/tools/xmlgen/xmlgen.py scaffold FQN... [--src DIR] [--force-dir]
                                                                 creates hand-written X.h/X.cpp stubs of behaviour classes if missing
Options: --config FILE (default xmlgen.toml next to this script), --java-src DIR (default from the config).

Pipeline: jaxb.py (Java model, JAXB rules) -> cppmodel.py (C++ mapping) -> emit.py (headers, member blocks, binders) + irjson.py
(xmlmodel.json, staticdata-classes.json, xmlgen-report.md). Exit code 2 on XmlGenError (message with file:line).
"""
from __future__ import annotations

import argparse
import os
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import cppmodel  # noqa: E402
import emit  # noqa: E402
import irjson  # noqa: E402
import jaxb  # noqa: E402
import scaffold  # noqa: E402
from jaxb import XmlGenError  # noqa: E402

CPP_ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
DEFAULT_OUT = os.path.join(CPP_ROOT, 'game-server', 'generated')
DEFAULT_SRC = os.path.join(CPP_ROOT, 'game-server', 'src')
DEFAULT_CONFIG = os.path.join(HERE, 'xmlgen.toml')
OWNED_ROOT_FILES = ('xmlmodel.json', 'staticdata-classes.json', 'xmlgen-report.md', 'xmlgen-include-root.inc')
OWNED_DIRS = ('aion',)


def load_config(path):
    with open(path, 'rb') as f:
        doc = tomllib.load(f)
    java_src = doc.get('java_src')
    if not java_src:
        raise XmlGenError(f'{path}: java_src is missing')
    return doc, os.path.normpath(os.path.join(os.path.dirname(path), java_src))


def build(config=DEFAULT_CONFIG, java_src=None, index=None):
    """(CppModel, {relative path: text}) for a configuration"""
    doc, default_src = load_config(config)
    if index is None:
        index = jaxb.load_index(java_src or default_src)
    return build_from(index, doc)


def build_from(index, doc):
    """(CppModel, {relative path: text}) for a parsed Java index and an xmlgen.toml document (dict)"""
    policy = jaxb.Policy.from_toml({k: v for k, v in doc.items() if k != 'java_src'})
    model = jaxb.Model(index, policy).build()
    cm = cppmodel.CppModel(model).build()
    files = emit.Emitter(cm).run()
    files['xmlmodel.json'] = irjson.dumps(irjson.xmlmodel(cm))
    files['staticdata-classes.json'] = irjson.dumps(irjson.staticdata_classes(cm))
    files['xmlgen-report.md'] = irjson.report(cm, files)
    unused = policy.unused_entries()
    if unused:
        raise XmlGenError('unused xmlgen.toml entries (remove them or fix their keys):\n' + '\n'.join(unused))
    return cm, files


def existing_files(out):
    found = {}
    for name in OWNED_ROOT_FILES:
        p = os.path.join(out, name)
        if os.path.isfile(p):
            found[name] = p
    for d in OWNED_DIRS:
        base = os.path.join(out, d)
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames.sort()
            for fn in sorted(filenames):
                p = os.path.join(dirpath, fn)
                found[os.path.relpath(p, out).replace('\\', '/')] = p
    return found


def check_src_conflicts(files, src):
    if src is None or not os.path.isdir(src):
        return
    conflicts = [rel for rel in sorted(files) if rel.startswith('aion/') and not rel.endswith(('.xml.h', '.xml.inc', '.bind.h', '.bind.cpp'))
                 and os.path.exists(os.path.join(src, rel))]
    if conflicts:
        raise XmlGenError('both src/ and generated/ would provide these headers (make the class a behaviour class via '
                          'xmlgen.toml [force_behaviour] or delete the hand-written file):\n' + '\n'.join(conflicts))


def read_text(path):
    with open(path, 'r', encoding='utf-8', newline='') as f:
        return f.read()


def write_text(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def check_case_collisions(files):
    """the generated tree is committed and checked out on case-insensitive file systems too"""
    seen = {}
    for rel in sorted(files):
        other = seen.setdefault(rel.lower(), rel)
        if other != rel:
            raise XmlGenError(f'generated files differ only in case: {other}, {rel}')


def generate(out, files, src=DEFAULT_SRC):
    check_case_collisions(files)
    check_src_conflicts(files, src)
    old = existing_files(out)
    lower = {rel.lower() for rel in files}
    removed = 0
    for rel, path in sorted(old.items()):  # first, so that a file renamed only in case is not deleted after being written
        if rel not in files:
            os.remove(path)
            removed += 1
    old = existing_files(out)
    written = 0
    for rel, text in sorted(files.items()):
        path = os.path.join(out, rel)
        if rel in old and read_text(old[rel]) == text:
            continue
        write_text(path, text)
        written += 1
    assert all(rel.lower() in lower for rel in existing_files(out))
    for dirpath, dirnames, filenames in sorted(os.walk(os.path.join(out, 'aion'), topdown=False)):
        if not dirnames and not filenames and not os.listdir(dirpath):
            os.rmdir(dirpath)
    return written, removed


def drift(out, files):
    """[(status, path)] differences between the generated tree on disk and `files`"""
    old = existing_files(out)
    diffs = []
    for rel in sorted(set(old) | set(files)):
        if rel not in old:
            diffs.append(('missing', rel))
        elif rel not in files:
            diffs.append(('stale', rel))
        elif read_text(old[rel]) != files[rel]:
            diffs.append(('changed', rel))
    return diffs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--config', default=DEFAULT_CONFIG)
    parser.add_argument('--java-src')
    sub = parser.add_subparsers(dest='command', required=True)
    g = sub.add_parser('generate')
    g.add_argument('--out', default=DEFAULT_OUT)
    g.add_argument('--src', default=DEFAULT_SRC)
    c = sub.add_parser('check')
    c.add_argument('--out', default=DEFAULT_OUT)
    s = sub.add_parser('scaffold')
    s.add_argument('classes', nargs='+')
    s.add_argument('--src', default=DEFAULT_SRC)
    args = parser.parse_args(argv)
    try:
        cm, files = build(args.config, args.java_src)
        if args.command == 'generate':
            written, removed = generate(args.out, files, args.src)
            print(f'xmlgen: {len(files)} files ({written} written, {removed} removed) in {args.out}')
            return 0
        if args.command == 'check':
            diffs = drift(args.out, files)
            for status, rel in diffs:
                print(f'{status}: {rel}')
            if diffs:
                print(f'xmlgen check: {len(diffs)} generated files differ from {args.out}; run xmlgen.py generate', file=sys.stderr)
                return 1
            print(f'xmlgen check: {len(files)} files up to date')
            return 0
        if args.command == 'scaffold':
            for rel, text in scaffold.scaffold(cm, args.classes):
                path = os.path.join(args.src, rel)
                if os.path.exists(path):
                    print(f'exists, kept: {path}')
                    continue
                write_text(path, text)
                print(f'created: {path}')
            return 0
    except XmlGenError as e:
        print(f'xmlgen: error: {e}', file=sys.stderr)
        return 2
    return 2


if __name__ == '__main__':
    sys.exit(main())
