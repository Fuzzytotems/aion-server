"""Parses every .java file of the Java server trees (game-server src and handlers, commons, login-server, chat-server), runs every body
helper over every body and resolves every declared type reference. Reports throughput on stderr."""
import os
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import javasrc  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
ROOTS = ['game-server/src', 'game-server/data/handlers', 'commons/src', 'login-server/src', 'chat-server/src']


def _type_refs(ty):
    if ty is None:
        return
    if ty.wildcard:
        yield from _type_refs(ty.bound)
        return
    yield ty
    for _, args in ty.segments:
        for a in args or ():
            yield from _type_refs(a)


class RealTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        roots = [os.path.join(REPO, r) for r in ROOTS]
        for r in roots:
            if not os.path.isdir(r):
                raise AssertionError(f'Java source root missing: {r}')
        cls.errors = []
        t0 = time.perf_counter()
        cls.index = javasrc.ProjectIndex()
        cls.index.roots = [r.replace('\\', '/') for r in roots]
        cls.files = 0
        for root in roots:
            for path in javasrc.find_java_files(root):
                cls.files += 1
                try:
                    cu = javasrc.parse_file(path)
                except javasrc.JavaSyntaxError as e:
                    cls.errors.append(str(e))
                    continue
                cu.root = root.replace('\\', '/')
                cu.relpath = os.path.relpath(path, root).replace('\\', '/')
                cls.index.add_unit(cu)
        cls.parse_seconds = time.perf_counter() - t0
        cls.bytes = sum(len(u.source) for u in cls.index.units)
        cls.tokens = sum(len(u.tokens) for u in cls.index.units)

    def test_all_files_parse(self):
        self.assertEqual(self.errors, [])
        self.assertGreater(self.files, 4000)
        self.assertEqual(len(self.index.units), self.files)
        mb = self.bytes / 1e6
        print(f'\n[javasrc] parsed {self.files} files, {len(self.index.types)} types, {self.tokens} tokens, {mb:.1f} MB in '
              f'{self.parse_seconds:.1f}s ({mb / self.parse_seconds:.2f} MB/s, {self.files / self.parse_seconds:.0f} files/s)',
              file=sys.stderr)

    def test_no_duplicate_types(self):
        self.assertEqual(self.index.duplicates, [])

    def test_body_helpers_over_all_bodies(self):
        t0 = time.perf_counter()
        errors = []
        spans = 0
        for cu in self.index.units:
            try:
                spans += javasrc.exercise_bodies(cu)
            except javasrc.JavaSyntaxError as e:
                errors.append(str(e))
        seconds = time.perf_counter() - t0
        self.assertEqual(errors, [])
        self.assertGreater(spans, 30000)
        print(f'\n[javasrc] body helpers over {spans} spans in {seconds:.1f}s', file=sys.stderr)

    def test_declared_types_resolve(self):
        idx = self.index
        counts = {}
        unresolved = []
        for cu in idx.units:
            for td in cu.all_types():
                items = [(t, td) for t in td.extends + td.implements + td.permits]
                items += [(f.type, f) for f in td.fields]
                for m in td.methods:
                    items += [(m.return_type, m)] + [(p.type, m) for p in m.params] + [(t, m) for t in m.throws]
                for ty, ctx in items:
                    for ref in _type_refs(ty):
                        kind, _ = idx.resolve_kind(ref.name, ctx)
                        counts[kind] = counts.get(kind, 0) + 1
                        if kind in ('unresolved', 'ambiguous'):
                            unresolved.append(f'{cu.relpath}:{ref.line}: {ref.name}')
        total = sum(counts.values())
        print(f'\n[javasrc] resolved {total} declared type references: {dict(sorted(counts.items()))}', file=sys.stderr)
        self.assertEqual(counts.get('ambiguous', 0), 0, unresolved[:20])
        # the few unresolved names come from third-party wildcard imports (java.awt.geom.*, org.quartz.*, javax.tools members)
        self.assertLess(counts.get('unresolved', 0), total / 500, unresolved[:50])
        self.assertGreater(counts.get('project', 0), 20000)


if __name__ == '__main__':
    unittest.main()
