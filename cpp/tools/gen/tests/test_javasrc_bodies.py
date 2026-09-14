"""Body helper tests for javasrc: lambdas, anonymous/local classes, new expressions, locals, identifiers, assignments, calls."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from javasrc import JavaSyntaxError, exercise_bodies, parse_source  # noqa: E402


def body(stmts, members=''):
    cu = parse_source('package p;\nclass Outer<K> {\n  int counter;\n  int[] arr;\n' + members + '\n  void body(Object o) {\n' + stmts
                      + '\n  }\n}\n', 'T.java')
    td = cu.types[0]
    return [m for m in td.methods if m.name == 'body'][0].body


class Lambdas(unittest.TestCase):
    def test_forms(self):
        b = body('list.forEach(x -> x.run());\nf((a, b) -> { return a + b; });\ng((final String s, @A int... n) -> s);\n'
                 'h(() -> 1);\ncomputeIfAbsent(id, _ -> new ArrayList<>());\nk(a -> b -> a + b);')
        lams = b.lambdas()
        self.assertEqual([lam.span.text for lam in lams], ['x -> x.run()', '(a, b) -> { return a + b; }',
                                                            '(final String s, @A int... n) -> s', '() -> 1', '_ -> new ArrayList<>()',
                                                            'a -> b -> a + b', 'b -> a + b'])
        self.assertEqual([[(p.name, str(p.type) if p.type else None) for p in lam.params] for lam in lams],
                         [[('x', None)], [('a', None), ('b', None)], [('s', 'String'), ('n', 'int')], [], [('_', None)], [('a', None)],
                          [('b', None)]])
        self.assertEqual([lam.block for lam in lams], [False, True, False, False, False, False, False])
        self.assertEqual(lams[1].body.text, '{ return a + b; }')
        self.assertEqual((lams[0].line, lams[0].col), (7, 14))
        self.assertIs(lams[0].span.owner, b.owner)

    def test_switch_rules_are_not_lambdas(self):
        b = body('int r = switch (o) { case Integer i when i > 3 ? true : false -> 1; case String s -> { yield 2; } '
                 'case null, default -> 0; };\nswitch (k) { case A, B -> run(() -> 5); default -> {} }')
        self.assertEqual([lam.span.text for lam in b.lambdas()], ['() -> 5'])

    def test_lambda_in_field_initializer_and_enum_args(self):
        cu = parse_source('enum E { A(x -> x * 2); final java.util.function.IntUnaryOperator f = y -> y; E(Object o) {} }')
        td = cu.types[0]
        self.assertEqual([lam.span.text for lam in td.enum_constants[0].args[0].lambdas()], ['x -> x * 2'])
        self.assertEqual([lam.span.text for lam in td.fields[0].initializer.lambdas()], ['y -> y'])

    def test_bad_arrow(self):
        b = body('int x = 1 + -> 2;')
        with self.assertRaises(JavaSyntaxError):
            b.lambdas()


class NewAndAnonymous(unittest.TestCase):
    def test_new_expressions(self):
        b = body('var a = new ArrayList<Map<String, Integer>>();\nint[][] g = new int[3][];\nString[] s = new String[] {"a", "b"};\n'
                 'Inner i = outer.new Inner(1, 2);\nObject d = new <T>Generic<>(x);\nnew java.util.HashMap<>(16).clear();\n'
                 'Supplier<X> sup = X::new;')
        news = b.new_expressions()
        self.assertEqual([n.span.text for n in news], ['new ArrayList<Map<String, Integer>>()', 'new int[3][]', 'new String[] {"a", "b"}',
                                                        'outer.new Inner(1, 2)', 'new <T>Generic<>(x)', 'new java.util.HashMap<>(16)'])
        self.assertEqual([(str(n.type), n.array) for n in news], [('ArrayList<Map<String, Integer>>', False), ('int[][]', True),
                                                                  ('String[]', True), ('Inner', False), ('Generic<>', False),
                                                                  ('java.util.HashMap<>', False)])
        self.assertEqual([a.text for a in news[3].args], ['1', '2'])
        self.assertEqual(news[3].qualifier.text, 'outer')
        self.assertEqual([d.text for d in news[1].dim_exprs], ['3'])
        self.assertEqual(news[2].initializer.text, '{"a", "b"}')
        self.assertEqual(news[4].type.args, [])

    def test_anonymous_classes(self):
        b = body('Runnable r = new Runnable() {\n  int cnt = 0;\n  @Override public void run() { cnt++; counter--; new Thread() { '
                 'public void run() {} }.start(); }\n};\nexecutor.submit(new Callable<Integer>() { public Integer call() { return 1; } });')
        anon = b.anonymous_classes()
        self.assertEqual([str(a.type) for a in anon], ['Runnable', 'Thread', 'Callable<Integer>'])
        r = anon[0].anonymous
        self.assertEqual((r.kind, r.anonymous, r.fqn, str(r.extends[0])), ('anonymous', True, None, 'Runnable'))
        self.assertEqual(r.outer.name, 'Outer')
        self.assertEqual([f.name for f in r.fields], ['cnt'])
        run = r.methods[0]
        self.assertEqual((run.name, run.annotations[0].name), ('run', 'Override'))
        self.assertEqual([a.span.text for a in run.body.anonymous_classes()], ['new Thread() { public void run() {} }'])
        self.assertIs(anon[1].anonymous, run.body.anonymous_classes()[0].anonymous)  # parsed once per unit
        self.assertEqual(anon[2].anonymous.methods[0].return_type.name, 'Integer')
        self.assertEqual(b.enclosing_class(run.body.start + 2), r)
        self.assertIsNone(b.enclosing_class(b.start + 1))

    def test_bad_new(self):
        with self.assertRaises(JavaSyntaxError):
            body('Object x = new Foo;').new_expressions()


class LocalTypes(unittest.TestCase):
    def test_local_class_record_enum_interface(self):
        b = body('class Local<T> extends Base implements Runnable { int z; Local(int z) { this.z = z; } public void run() {} class Deep {} }\n'
                 'record R(int q) {}\nenum Color { RED, GREEN }\ninterface Cb { void f(); }\nfinal class Fin {}\n'
                 'Class<?> c = String.class;\nint record = 1;')
        lts = b.local_types()
        self.assertEqual([(t.kind, t.name, t.local) for t in lts], [('class', 'Local', True), ('record', 'R', True), ('enum', 'Color', True),
                                                                    ('interface', 'Cb', True), ('class', 'Fin', True)])
        self.assertEqual([m.kind for m in lts[0].methods], ['constructor', 'method'])
        self.assertEqual(lts[0].types[0].name, 'Deep')
        self.assertEqual((lts[0].fqn, lts[0].types[0].fqn, lts[4].modifiers), (None, None, ['final']))
        self.assertEqual(b.enclosing_class(lts[0].methods[0].body.start + 1), lts[0])

    def test_local_class_inside_local_class_method(self):
        b = body('class A { class Member {} void f() { class B { int deep; } int local; } }\nint after = 1;')
        self.assertEqual([t.name for t in b.local_types()], ['A', 'B'])
        self.assertEqual([v.name for v in b.local_variables()], ['o', 'local', 'after'])
        self.assertEqual(b.enclosing_class(b.local_types()[1].body.start + 2).name, 'B')


class LocalVariables(unittest.TestCase):
    def test_kinds_and_types(self):
        b = body('var list = new ArrayList<String>();\nfinal @A Map<String, List<Integer>> m = null, n;\nint a[] = {1}, c = 2;\n'
                 'for (int i = 0, j = 1; i < j; i++) {}\nfor (final String k : list) {}\n'
                 'try (var in = open(); java.io.Reader rd = x) {} catch (IllegalStateException | IllegalArgumentException e) {}\n'
                 'if (o instanceof String s && !s.isEmpty()) {}\nif (o instanceof Point(int px, var py) && px > 0) {}\n'
                 'switch (o) { case Integer v when v > 1 -> {} case Box(Point(var bx, _), String bs) -> {} default -> {} }\n'
                 'list.forEach((String t) -> {});\nlist.forEach(u -> {});\n'
                 'outer: for (;;) { break outer; }\nint yield = 1;\ncounter = 2;\nfoo.bar();\ni < n;')
        got = [(v.kind, v.name, str(v.type) if v.type else None) for v in b.local_variables()]
        self.assertEqual(got, [('param', 'o', 'Object'), ('local', 'list', 'var'), ('local', 'm', 'Map<String, List<Integer>>'), ('local', 'n', 'Map<String, List<Integer>>'),
                               ('local', 'a', 'int[]'), ('local', 'c', 'int'), ('for', 'i', 'int'), ('for', 'j', 'int'), ('foreach', 'k', 'String'),
                               ('resource', 'in', 'var'), ('resource', 'rd', 'java.io.Reader'), ('catch', 'e', 'IllegalStateException'),
                               ('pattern', 's', 'String'), ('pattern', 'px', 'int'), ('pattern', 'py', 'var'), ('pattern', 'v', 'Integer'),
                               ('pattern', 'bx', 'var'), ('pattern', 'bs', 'String'), ('lambda_param', 't', 'String'),
                               ('lambda_param', 'u', None), ('local', 'yield', 'int')])
        vs = {v.name: v for v in b.local_variables()}
        self.assertEqual([str(t) for t in vs['e'].union_types], ['IllegalStateException', 'IllegalArgumentException'])
        self.assertEqual(vs['m'].modifiers, ['final'])
        self.assertEqual(vs['list'].initializer.text, 'new ArrayList<String>()')
        self.assertIsNone(vs['n'].initializer)
        self.assertEqual((vs['list'].line, vs['list'].col), (7, 5))

    def test_scopes(self):
        b = body('int a = 1;\n{ int inner = 2; use(inner); }\nfor (int i = 0; i < 3; i++) use(i);\nfor (String s : xs) { use(s); }\n'
                 'try (var r = open()) { use(r); } catch (Exception e) { use(e); }\nf(x -> x);\nuse(a);')
        t = b.cu.tokens.text

        def index_of(text, nth=0):
            return [i for i in range(b.start, b.end) if t[i] == text][nth]
        last_use = index_of('use', 5)
        self.assertEqual(sorted(b.locals_visible_at(last_use)), ['a', 'o'])
        self.assertIn('inner', b.locals_visible_at(index_of('use', 0)))
        self.assertEqual(sorted(b.locals_visible_at(index_of('use', 1))), ['a', 'i', 'o'])
        self.assertEqual(sorted(b.locals_visible_at(index_of('use', 2))), ['a', 'o', 's'])
        self.assertEqual(sorted(b.locals_visible_at(index_of('use', 3))), ['a', 'o', 'r'])
        self.assertEqual(sorted(b.locals_visible_at(index_of('use', 4))), ['a', 'e', 'o'])
        x_body = [i for i in range(b.start, b.end) if t[i] == 'x'][1]
        self.assertEqual(sorted(b.locals_visible_at(x_body)), ['a', 'o', 'x'])

    def test_anonymous_members_are_not_locals(self):
        b = body('Runnable r = new Runnable() { int cnt = 0; public void run() { int inRun = 1; } };\nclass L { int field; }')
        self.assertEqual([v.name for v in b.local_variables()], ['o', 'r', 'inRun'])

    def test_parameters(self):
        b = body('o = null;\nnew Consumer<String>() { public void accept(String o) { o = ""; } };\n'
                 'record R(int a) { R { a = 1; } void f(int q) { q++; } }\nlist.forEach(counter -> counter = 1);')
        self.assertEqual([(v.kind, v.name) for v in b.local_variables()],
                         [('param', 'o'), ('param', 'o'), ('param', 'a'), ('param', 'q'), ('lambda_param', 'counter')])
        self.assertEqual([(a.name, a.local) for a in b.assignments()], [('o', True), ('o', True), ('a', True), ('q', True), ('counter', True)])
        self.assertEqual(b.field_assignments(), [])


class Identifiers(unittest.TestCase):
    def test_roles(self):
        b = body('String s = this.name + other.field + helper(1) + Outer.this.counter;\nlist.stream().map(String::valueOf);\n'
                 'label: for (;;) { continue label; }\nswitch (o) { case String t when t.isEmpty() -> { yield 1; } default -> {} }\n'
                 'x = java.util.List.<Integer>of(counter);\n@SuppressWarnings("x") int q = 0;')
        refs = [(r.name, r.role, r.qualifier) for r in b.identifier_refs()]
        self.assertEqual(refs, [
            ('String', 'type', None), ('s', 'declaration', None), ('name', 'member', 'this'), ('other', 'expr', None),
            ('field', 'member', 'other'), ('helper', 'call', None), ('Outer', 'expr', None), ('counter', 'member', 'Outer.this'),
            ('list', 'expr', None), ('stream', 'call', 'list'), ('map', 'call', 'list.stream()'), ('String', 'expr', None),
            ('valueOf', 'method_ref', None), ('label', 'label', None), ('label', 'label', None), ('o', 'expr', None),
            ('String', 'type', None), ('t', 'declaration', None), ('t', 'expr', None), ('isEmpty', 'call', 't'),
            ('x', 'expr', None), ('java', 'expr', None), ('util', 'member', 'java'), ('List', 'member', 'java.util'),
            ('Integer', 'type', None), ('of', 'call', 'java.util.List'), ('counter', 'expr', None), ('q', 'declaration', None)])
        self.assertEqual((b.identifier_refs()[0].line, b.identifier_refs()[0].col), (7, 1))


class Assignments(unittest.TestCase):
    def test_forms(self):
        b = body('counter = 1;\nthis.counter += 2;\ncounter++;\n--counter;\n++this.counter;\narr[1] = 3;\nthis.arr[0] |= 1;\n'
                 'int local = 0; local = 5; local++;\nother.x = 4;\nnew Foo().y = 5;\nint[][] g = null; g[1][2] = 3;\n'
                 'int decl = 1, decl2 = decl;\nif (a == b) {}\nx >>>= 2;\n@A(v = 1) int ann = 2;\n'
                 'Runnable r = () -> { int counter = 0; counter = 7; };\nOuter.this.counter = 8;')
        got = [(a.name, a.qualifier, a.op, a.element, a.local, a.rhs.text if a.rhs else None) for a in b.assignments()]
        self.assertEqual(got, [
            ('counter', None, '=', False, False, '1'), ('counter', 'this', '+=', False, False, '2'), ('counter', None, '++', False, False, None),
            ('counter', None, '--', False, False, None), ('counter', 'this', '++', False, False, None), ('arr', None, '=', True, False, '3'),
            ('arr', 'this', '|=', True, False, '1'), ('local', None, '=', False, True, '5'), ('local', None, '++', False, True, None),
            ('x', 'other', '=', False, False, '4'), ('y', 'new Foo()', '=', False, False, '5'), ('g', None, '=', True, True, '3'),
            ('x', None, '>>>=', False, False, '2'), ('counter', None, '=', False, True, '7'),
            ('counter', 'Outer.this', '=', False, False, '8')])
        fields = [(a.name, a.qualifier) for a in b.field_assignments()]
        self.assertNotIn(('local', None), fields)
        self.assertIn(('counter', 'this'), fields)
        self.assertEqual((b.assignments()[0].line, b.assignments()[0].col), (7, 1))

    def test_assignments_in_constructor_of_local_class(self):
        b = body('class L { int z; L(int z) { this.z = z; } }')
        a = b.assignments()
        self.assertEqual([(x.name, x.qualifier) for x in a], [('z', 'this')])
        self.assertEqual(b.enclosing_class(a[0].index).name, 'L')


class Calls(unittest.TestCase):
    def test_method_calls(self):
        b = body('foo(1, bar(2));\nthis.baz();\nsuper.qux(a, b);\nlist.stream().map(x -> x.y()).toList();\n'
                 'Collections.<String>emptyList();\nnew Foo(1).go();\n((Player) o).getName();\narr[0].length();\n"s".length();\n'
                 'Runnable r = new Runnable() { public void run() {} };\nclass L { L() { this(1); } L(int a) { super(); } <T> T gen() { return null; } }\n'
                 'if (o instanceof Point(int px, int py)) {}\nOuter.super.toString();\nif (a > max() && b < c || d > e.f()) {}')
        got = [(c.name, c.receiver.text if c.receiver else None, [a.text for a in c.args]) for c in b.method_calls()]
        self.assertEqual(got, [
            ('foo', None, ['1', 'bar(2)']), ('bar', None, ['2']), ('baz', 'this', []), ('qux', 'super', ['a', 'b']),
            ('stream', 'list', []), ('map', 'list.stream()', ['x -> x.y()']), ('y', 'x', []), ('toList', 'list.stream().map(x -> x.y())', []),
            ('emptyList', 'Collections', []), ('go', 'new Foo(1)', []), ('getName', '((Player) o)', []), ('length', 'arr[0]', []),
            ('length', '"s"', []), ('this', None, ['1']), ('super', None, []), ('toString', 'Outer.super', []), ('max', None, []),
            ('f', 'e', [])])
        self.assertEqual(b.method_calls()[0].span.text, 'foo(1, bar(2))')

    def test_method_refs(self):
        b = body('f(String::valueOf, this::run, super::hashCode, Foo[]::new, ArrayList::new, java.util.Objects::nonNull, '
                 'List::<String>of, a.b()::c);')
        self.assertEqual([(r.receiver.text, r.name) for r in b.method_refs()],
                         [('String', 'valueOf'), ('this', 'run'), ('super', 'hashCode'), ('Foo[]', 'new'), ('ArrayList', 'new'),
                          ('java.util.Objects', 'nonNull'), ('List', 'of'), ('a.b()', 'c')])


class Misc(unittest.TestCase):
    def test_span_basics(self):
        b = body('int x = 1;')
        self.assertEqual(b.text, '{\nint x = 1;\n  }')
        self.assertEqual((b.line, b.col, b.end_line, b.end_col), (6, 23, 8, 3))
        self.assertEqual(b.texts()[:3], ['{', 'int', 'x'])
        self.assertIn(b.start, b)
        self.assertNotIn(b.end, b)
        self.assertTrue(b.contains(b.local_variables()[1].initializer))
        self.assertEqual(b.to_json(True)['text'], b.text)

    def test_text_block_and_switch_pattern_body(self):
        b = body('String q = """\n    select * from t where a = ?\n    """;\nObject r = switch (o) { case Integer i -> i + 1; default -> o; };')
        self.assertEqual([v.name for v in b.local_variables()], ['o', 'q', 'r', 'i'])
        self.assertEqual(b.assignments(), [])

    def test_exercise_bodies_counts_nested_spans(self):
        cu = parse_source('class A { int f = 1; static {} enum E { X(1) { void m() {} }; E(int a) {} } void g() { new Object() { void h() {} }; '
                          'class L { void i() {} } } }')
        self.assertEqual(exercise_bodies(cu), 8)


if __name__ == '__main__':
    unittest.main()
