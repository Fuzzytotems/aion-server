"""xmlgen: static data generator (JAXB replacement, docs/design/static-data.md §1-2 and amendments). Python 3.12, stdlib only.

Commands (run from anywhere; paths default to the repository layout):
    python cpp/tools/xmlgen/xmlgen.py generate [--out DIR]      writes the generated tree (default cpp/game-server/generated) and prunes
                                                                 stale generated files (never touches generated/concurrency)
    python cpp/tools/xmlgen/xmlgen.py check [--out DIR] [--src DIR]  regenerates in memory and compares with DIR; exit 1 on drift, 2 on
                                                                 a conflict with a hand-written header (as generate)
    python cpp/tools/xmlgen/xmlgen.py scaffold FQN... [--src DIR]  creates hand-written X.h/X.cpp stubs of behaviour classes if missing
    python cpp/tools/xmlgen/xmlgen.py scaffold --all [--src DIR]  creates the S0a shells of all behaviour classes and class adapter targets
                                                                 (no port comments) if missing, and lists the files that exist already
Options: --config FILE (default xmlgen.toml next to this script), --java-src DIR (default from the config).

Pipeline: jaxb.py (Java model, JAXB rules) -> cppmodel.py (C++ mapping) -> emit.py (headers, member blocks, binders) + irjson.py
(xmlmodel.json, staticdata-classes.json, xmlgen-report.md). Exit code 2 on XmlGenError (message with file:line).
"""
from __future__ import annotations

import argparse
import os
import re
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
OWNED_ROOT_FILES = ('xmlmodel.json', 'staticdata-classes.json', 'xmlgen-report.md')
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


def check_src_conflicts(files, src, cm=None):
    if src is None or not os.path.isdir(src):
        return
    conflicts = [rel for rel in sorted(files) if rel.startswith('aion/') and not rel.endswith(('.xml.h', '.xml.inc', '.bind.h', '.bind.cpp'))
                 and os.path.exists(os.path.join(src, rel))]
    if conflicts:
        raise XmlGenError('both src/ and generated/ would provide these headers (make the class a behaviour class via '
                          'xmlgen.toml [force_behaviour] or delete the hand-written file):\n' + '\n'.join(conflicts))
    if cm is None:
        return
    # a nested enum generated as Outer_Inner.h, or a secondary top-level enum of another Java file, while the hand-written header of its
    # Java file (skeleton draft) defines the enum itself; a top-level enum of its own file is a path conflict above
    nested = []
    for e in sorted(cm.enums.values(), key=lambda e: e.fqn):
        td = e.model.td
        if td is None:  # [external_enums]
            continue
        stem = td.cu.relpath.replace('\\', '/').rsplit('/', 1)[-1][:-len('.java')]
        if td.outer is None and td.name == stem:
            continue
        path = os.path.join(src, cppmodel.package_dir(td.cu.package), stem + '.h')
        definition = re.compile(r'(?m)^[ \t]*(?:[^/\n]*[;{}][ \t]*)?enum\s+class\s+' + re.escape(e.java_name) + r'\b')
        if os.path.isfile(path) and definition.search(read_text(path)):
            nested.append(f'{jaxb.short_fqn(e.fqn)} ({os.path.relpath(path, src)})')
    if nested:
        raise XmlGenError('these enums have a hand-written definition and a generated one (add them to xmlgen.toml [hand_written_enums] or '
                          'alias the generated enum in the hand-written class):\n' + '\n'.join(nested))


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


def generate(out, files, src=DEFAULT_SRC, cm=None):
    check_case_collisions(files)
    check_src_conflicts(files, src, cm)
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


def write_scaffold(src, outputs):
    """writes the scaffold outputs that do not exist yet; returns ([created paths], [existing paths, kept])"""
    created, kept = [], []
    for rel, text in outputs:
        path = os.path.join(src, rel)
        if os.path.exists(path):
            kept.append(path)
            continue
        write_text(path, text)
        created.append(path)
    return created, kept


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
    c.add_argument('--src', default=DEFAULT_SRC)
    s = sub.add_parser('scaffold')
    s.add_argument('classes', nargs='*')
    s.add_argument('--all', action='store_true', help='all behaviour classes and class adapter targets, without port comments')
    s.add_argument('--src', default=DEFAULT_SRC)
    args = parser.parse_args(argv)
    if args.command == 'scaffold' and args.all == bool(args.classes):
        parser.error('scaffold needs either class names or --all')
    try:
        cm, files = build(args.config, args.java_src)
        if args.command == 'generate':
            written, removed = generate(args.out, files, args.src, cm)
            print(f'xmlgen: {len(files)} files ({written} written, {removed} removed) in {args.out}')
            return 0
        if args.command == 'check':
            check_case_collisions(files)
            check_src_conflicts(files, args.src, cm)
            diffs = drift(args.out, files)
            for status, rel in diffs:
                print(f'{status}: {rel}')
            if diffs:
                print(f'xmlgen check: {len(diffs)} generated files differ from {args.out}; run xmlgen.py generate', file=sys.stderr)
                return 1
            print(f'xmlgen check: {len(files)} files up to date')
            return 0
        if args.command == 'scaffold':
            outputs = scaffold.scaffold_all(cm) if args.all else scaffold.scaffold(cm, args.classes)
            created, kept = write_scaffold(args.src, outputs)
            for path in kept:
                print(f'exists, kept: {path}')
            if not args.all:
                for path in created:
                    print(f'created: {path}')
            print(f'xmlgen scaffold: {len(created)} files created, {len(kept)} existing files kept in {args.src}')
            return 0
    except XmlGenError as e:
        print(f'xmlgen: error: {e}', file=sys.stderr)
        return 2
    return 2


if __name__ == '__main__':
    sys.exit(main())
