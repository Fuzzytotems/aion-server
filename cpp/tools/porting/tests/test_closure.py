"""closure.py (m5j-plan.md §10.6): the site finder and the allow-list comparison, on synthetic input"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import closure  # noqa: E402


class FindSitesTest(unittest.TestCase):
    def test_sites_outside_comments_with_their_function(self):
        text = '\n'.join([
            'void Foo::bar(int x) {',
            '\tAION_UNPORTED();',
            '}',
            '// AION_UNPORTED() in a line comment',
            '/* AION_PARTIAL("a") in a',
            ' * block comment AION_UNPORTED() */',
            'int Foo::baz() {',
            '\tconst char* s = "AION_UNPORTED() in a string";',
            '\tAION_PARTIAL("the reason // with slashes");',
            '\treturn AION_UNPORTED(), 0; // AION_UNPORTED() again',
            '}',
            '#define AION_UNPORTED() throw 1',
        ])
        self.assertEqual(closure.find_sites(text), [(2, 'UNPORTED', 'Foo::bar'), (9, 'PARTIAL', 'Foo::baz'), (10, 'UNPORTED', 'Foo::baz')])

    def test_a_site_before_any_definition_has_no_function(self):
        self.assertEqual(closure.find_sites('inline int x = (AION_UNPORTED(), 1);'), [(1, 'UNPORTED', '?')])


class CheckTest(unittest.TestCase):
    def allowlist(self, text):
        f = tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False, encoding='utf-8')
        f.write(text)
        f.close()
        self.addCleanup(lambda: Path(f.name).unlink())
        return f.name

    def test_every_site_and_opcode_covered_is_ok(self):
        path = self.allowlist('# comment\n\nsite|src/a.cpp|A::f|2|M5h\nopcode|CM_X|M5i\n')
        found = {('src/a.cpp', 'A::f'): [(3, 'UNPORTED'), (9, 'PARTIAL')]}
        self.assertEqual(closure.check(found, ['CM_X'], path), [])

    def test_the_problems(self):
        path = self.allowlist('site|src/a.cpp|A::f|1|M5h\nsite|src/b.cpp|B::g|1|\nsite|src/c.cpp|C::h|1|M5i\nopcode|CM_Y|M5i\nnonsense\n')
        found = {('src/a.cpp', 'A::f'): [(3, 'UNPORTED'), (4, 'UNPORTED')], ('src/b.cpp', 'B::g'): [(1, 'UNPORTED')],
                 ('src/d.cpp', 'D::k'): [(7, 'PARTIAL')]}
        problems = closure.check(found, ['CM_X'], path)
        self.assertEqual(len(problems), 7, problems)
        text = '\n'.join(problems)
        self.assertIn('names no owner', text)                      # B::g
        self.assertIn('not a `site|path|function|count|owner`', text)  # nonsense
        self.assertIn('2 site(s), the allow-list row says 1', text)  # A::f
        self.assertIn('src/d.cpp:7: 1 site(s) in D::k without an allow-list row', text)
        self.assertIn('C::h: the allow-list row matches no site any more (stale)', text)
        self.assertIn('CM_X: a registered opcode without a C++ class and without an allow-list row', text)
        self.assertIn('CM_Y: the allow-list row matches no missing class any more (stale)', text)


if __name__ == '__main__':
    unittest.main()
