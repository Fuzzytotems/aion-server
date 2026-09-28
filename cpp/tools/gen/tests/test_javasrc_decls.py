"""Declaration-level parser tests for javasrc (one test per construct)."""
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from javasrc import JavaSyntaxError, parse_source  # noqa: E402


def parse(src):
    return parse_source(src, 'T.java')


def only_type(src):
    cu = parse(src)
    assert len(cu.types) == 1
    return cu.types[0]


class UnitHeader(unittest.TestCase):
    def test_package_and_imports(self):
        cu = parse('package com.x.y;\nimport java.util.List;\nimport java.util.*;\nimport static a.B.C;\nimport static a.B.*;;\nclass A {}')
        self.assertEqual(cu.package, 'com.x.y')
        self.assertEqual([(i.name, i.static, i.wildcard) for i in cu.imports],
                         [('java.util.List', False, False), ('java.util', False, True), ('a.B.C', True, False), ('a.B', True, True)])
        self.assertEqual((cu.imports[1].line, cu.imports[1].col), (3, 1))
        self.assertEqual(cu.types[0].fqn, 'com.x.y.A')

    def test_default_package_and_multiple_top_level_types(self):
        cu = parse('class A {} ; interface B {} enum C {} record D() {} @interface E {}')
        self.assertIsNone(cu.package)
        self.assertEqual([(t.kind, t.fqn) for t in cu.types], [('class', 'A'), ('interface', 'B'), ('enum', 'C'), ('record', 'D'),
                                                                ('annotation', 'E')])

    def test_package_annotations(self):
        cu = parse('@Deprecated @XmlSchema(namespace = "x")\npackage p;')
        self.assertEqual([a.name for a in cu.package_annotations], ['Deprecated', 'XmlSchema'])
        self.assertEqual(cu.types, [])

    def test_annotation_on_first_type_without_package(self):
        cu = parse('@Deprecated class A {}')
        self.assertEqual(cu.types[0].annotations[0].name, 'Deprecated')


class ClassHeaders(unittest.TestCase):
    def test_generics_bounds_wildcards(self):
        td = only_type('public abstract class A<K extends Comparable<? super K> & java.io.Serializable, V> '
                       'extends Base<Map<K, List<? extends V>>, ?> implements I<K>, J {}')
        self.assertEqual(td.modifiers, ['public', 'abstract'])
        self.assertEqual([p.name for p in td.type_params], ['K', 'V'])
        self.assertEqual([str(b) for b in td.type_params[0].bounds], ['Comparable<? super K>', 'java.io.Serializable'])
        sup = td.extends[0]
        self.assertEqual(sup.name, 'Base')
        self.assertEqual(str(sup), 'Base<Map<K, List<? extends V>>, ?>')
        inner = sup.args[0].args[1].args[0]
        self.assertEqual((inner.wildcard, inner.bound.name), ('extends', 'V'))
        self.assertEqual(sup.args[1].wildcard, '?')
        self.assertEqual([str(t) for t in td.implements], ['I<K>', 'J'])

    def test_interface_extends_many(self):
        td = only_type('interface I<T> extends A<T>, B {}')
        self.assertEqual([str(t) for t in td.extends], ['A<T>', 'B'])
        self.assertEqual(td.implements, [])

    def test_sealed_non_sealed_permits(self):
        cu = parse('public sealed interface S permits A, B.C {}\nfinal class A implements S {}\nnon-sealed class B implements S {}\n'
                   'sealed abstract class Z permits Y {}')
        s, a, b, z = cu.types
        self.assertEqual(s.modifiers, ['public', 'sealed'])
        self.assertEqual([t.name for t in s.permits], ['A', 'B.C'])
        self.assertEqual(b.modifiers, ['non-sealed'])
        self.assertEqual(z.modifiers, ['sealed', 'abstract'])

    def test_qualified_generic_segments(self):
        td = only_type('class A { Outer<String>.Inner<Integer> x; java.util.Map.Entry<K, V>[] e; }')
        x, e = td.fields
        self.assertEqual(x.type.segments[0], ('Outer', x.type.segments[0][1]))
        self.assertEqual(str(x.type), 'Outer<String>.Inner<Integer>')
        self.assertEqual(x.type.name, 'Outer.Inner')
        self.assertIn('segments', x.type.to_json())
        self.assertEqual((e.type.name, e.type.dims), ('java.util.Map.Entry', 1))

    def test_locations(self):
        cu = parse('package p;\n\n  public class Foo {\n\tint bar;\n}')
        td = cu.types[0]
        self.assertEqual((td.line, td.col), (3, 16))
        self.assertEqual((td.fields[0].line, td.fields[0].col), (4, 6))
        self.assertEqual((td.body.line, td.body.col, td.body.end_line, td.body.end_col), (3, 20, 5, 1))


class Members(unittest.TestCase):
    def test_fields_multi_declarators_dims_initializers(self):
        td = only_type('class A { private static final Map<String, List<Integer>> M = new HashMap<String, List<Integer>>(), '
                       'N = Map.<String, List<Integer>>of(); int a[] = {1, 2}, b, c[][]; Runnable r = () -> { int x = 1, y; }; '
                       'Object o = new Object() { int f = 1, g = 2; }; boolean z = x instanceof Map<?, ?> m && m.isEmpty(), w = a < b; }')
        f = {x.name: x for x in td.fields}
        self.assertEqual(list(f), ['M', 'N', 'a', 'b', 'c', 'r', 'o', 'z', 'w'])
        self.assertEqual(f['M'].initializer.text, 'new HashMap<String, List<Integer>>()')
        self.assertEqual(f['N'].initializer.text, 'Map.<String, List<Integer>>of()')
        self.assertEqual(f['M'].modifiers, ['private', 'static', 'final'])
        self.assertEqual(f['N'].declarator_index, 1)
        self.assertEqual((f['a'].type.name, f['a'].type.dims, f['a'].initializer.text), ('int', 1, '{1, 2}'))
        self.assertEqual((f['b'].type.dims, f['b'].initializer), (0, None))
        self.assertEqual(f['c'].type.dims, 2)
        self.assertEqual(f['r'].initializer.text, '() -> { int x = 1, y; }')
        self.assertEqual(f['o'].initializer.text, 'new Object() { int f = 1, g = 2; }')
        self.assertEqual(f['z'].initializer.text, 'x instanceof Map<?, ?> m && m.isEmpty()')
        self.assertEqual(f['w'].initializer.text, 'a < b')
        self.assertIs(f['r'].initializer.owner, f['r'])

    def test_switch_expression_and_text_block_initializers(self):
        td = only_type('class A { int v = switch (k) { case 1, 2 -> 3; default -> { yield 4; } }; String s = """\n  x\n  """; int q; }')
        self.assertEqual([x.name for x in td.fields], ['v', 's', 'q'])
        self.assertTrue(td.fields[0].initializer.text.startswith('switch'))

    def test_methods(self):
        td = only_type('abstract class A { public <T extends Number> List<T> f(final @Nonnull Map<String, T> m, int... rest) '
                       'throws IOException, E { return null; } abstract void g(); int h()[] { return null; } '
                       'void r(A this, String a[]) {} synchronized native void n(); }')
        f, g, h, r, n = td.methods
        self.assertEqual((f.kind, f.name, str(f.return_type), [p.name for p in f.type_params]), ('method', 'f', 'List<T>', ['T']))
        self.assertEqual([(str(p.type), p.name, p.varargs, p.modifiers, [a.name for a in p.annotations]) for p in f.params],
                         [('Map<String, T>', 'm', False, ['final'], ['Nonnull']), ('int', 'rest', True, [], [])])
        self.assertEqual([str(t) for t in f.throws], ['IOException', 'E'])
        self.assertEqual(f.body.text, '{ return null; }')
        self.assertIs(f.body.owner, f)
        self.assertIsNone(g.body)
        self.assertEqual(g.modifiers, ['abstract'])
        self.assertEqual(h.return_type.dims, 1)
        self.assertEqual([(p.name, p.type.dims) for p in r.params], [('this', 0), ('a', 1)])
        self.assertEqual(n.modifiers, ['synchronized', 'native'])

    def test_constructors_and_initializers(self):
        td = only_type('class A<T> { static int s; static { s = 1; } { x = 2; } int x; public <U> A(U u) throws E { this(); } '
                       'A() { super(); } }')
        self.assertEqual([(i.static, i.body.text) for i in td.initializers], [(True, '{ s = 1; }'), (False, '{ x = 2; }')])
        c1, c2 = td.methods
        self.assertEqual((c1.kind, c1.name, c1.return_type, [p.name for p in c1.type_params]), ('constructor', 'A', None, ['U']))
        self.assertEqual(c2.kind, 'constructor')
        self.assertEqual([type(m).__name__ for m in td.members], ['FieldDecl', 'Initializer', 'Initializer', 'FieldDecl', 'MethodDecl',
                                                                 'MethodDecl'])

    def test_interface_default_static_private_methods(self):
        td = only_type('interface I { int C = 1; default void a() {} static I b() { return null; } private void c() {} void d(); }')
        self.assertEqual([(m.name, m.modifiers, m.body is not None) for m in td.methods],
                         [('a', ['default'], True), ('b', ['static'], True), ('c', ['private'], True), ('d', [], False)])
        self.assertEqual(td.fields[0].name, 'C')

    def test_nested_types_fqn_binary_name(self):
        cu = parse('package p; class A { static class B { interface C {} enum D { X } } record R(int a) {} private @interface N {} }')
        a = cu.types[0]
        self.assertEqual([t.fqn for t in cu.all_types()], ['p.A', 'p.A.B', 'p.A.B.C', 'p.A.B.D', 'p.A.R', 'p.A.N'])
        c = a.types[0].types[0]
        self.assertEqual((c.binary_name, c.outer.name), ('p.A$B$C', 'B'))
        self.assertEqual(a.types[2].modifiers, ['private'])

    def test_docs_on_declarations(self):
        td = only_type('/** Type. */ @Deprecated class A { /** Field. */ int x; /** Method. */ void f() {} /** Nested. */ class B {} }')
        self.assertEqual(td.doc, '/** Type. */')
        self.assertEqual((td.fields[0].doc, td.methods[0].doc, td.types[0].doc), ('/** Field. */', '/** Method. */', '/** Nested. */'))


class Records(unittest.TestCase):
    def test_record_components_compact_constructor(self):
        td = only_type('public record P<T>(@XmlAttribute int x, List<T> ys, String... names) implements Comparable<P<T>> { '
                       'public P { Objects.requireNonNull(ys); } P(int x) { this(x, null); } static int z; '
                       'public int compareTo(P<T> o) { return 0; } }')
        self.assertEqual(td.kind, 'record')
        self.assertEqual([(str(c.type), c.name, c.varargs) for c in td.record_components],
                         [('int', 'x', False), ('List<T>', 'ys', False), ('String', 'names', True)])
        self.assertEqual(td.record_components[0].annotations[0].name, 'XmlAttribute')
        self.assertEqual([m.kind for m in td.methods], ['compact_constructor', 'constructor', 'method'])
        self.assertEqual(td.methods[0].params, [])
        self.assertEqual(str(td.implements[0]), 'Comparable<P<T>>')

    def test_empty_record(self):
        td = only_type('record E() {}')
        self.assertEqual((td.record_components, td.members), ([], []))


class Enums(unittest.TestCase):
    def test_constants_args_bodies_members(self):
        td = only_type('enum Rank implements Runnable { @Deprecated IRON(0, 4, \'\\uE02B\'), BRONZE(1, f(2, 3), Map.<A, B>of()) { '
                       '@Override public void run() {} int extra; }, SILVER, ; private final int a; Rank() { this(0, 0, 0); } '
                       'Rank(int a, Object b, Object c) { this.a = a; } public void run() {} }')
        names = [c.name for c in td.enum_constants]
        self.assertEqual(names, ['IRON', 'BRONZE', 'SILVER'])
        iron, bronze, silver = td.enum_constants
        self.assertEqual([a.text for a in iron.args], ['0', '4', "'\\uE02B'"])
        self.assertEqual(iron.annotations[0].name, 'Deprecated')
        self.assertEqual([a.text for a in bronze.args], ['1', 'f(2, 3)', 'Map.<A, B>of()'])
        self.assertEqual((bronze.body.kind, [m.name for m in bronze.body.methods], bronze.body.fields[0].name), ('anonymous', ['run'], 'extra'))
        self.assertIs(bronze.body.outer, td)
        self.assertIsNone(bronze.body.fqn)
        self.assertEqual((silver.args, silver.body), (None, None))
        self.assertEqual([m.kind for m in td.methods], ['constructor', 'constructor', 'method'])
        self.assertEqual(td.fields[0].name, 'a')

    def test_enum_forms(self):
        self.assertEqual([c.name for c in only_type('enum E { A, B }').enum_constants], ['A', 'B'])
        self.assertEqual([c.name for c in only_type('enum E { A, B, }').enum_constants], ['A', 'B'])
        self.assertEqual(only_type('enum E { ; int x; }').enum_constants, [])
        self.assertEqual(only_type('enum E {}').enum_constants, [])
        self.assertEqual(only_type('enum E { A() }').enum_constants[0].args, [])


class Annotations(unittest.TestCase):
    def test_argument_forms(self):
        td = only_type('@XmlRootElement(name = "a")\n@XmlAccessorType(XmlAccessType.FIELD)\n@SuppressWarnings({"unchecked", "rawtypes"})\n'
                       '@XmlElements({ @XmlElement(name = "x", type = X.class), @XmlElement(name = "y", type = int[].class, '
                       'required = true) })\n@Range(min = -5, max = 0x10, f = 1.5f, c = \'c\', expr = 1 << 3, s = "a" + "b", n = null)\n'
                       '@Marker @Empty() @java.lang.Deprecated(since = "9")\nclass A {}')
        anns = {a.name: a for a in td.annotations}
        self.assertEqual(list(anns), ['XmlRootElement', 'XmlAccessorType', 'SuppressWarnings', 'XmlElements', 'Range', 'Marker', 'Empty',
                                      'java.lang.Deprecated'])
        self.assertEqual(anns['XmlRootElement'].get('name').value, 'a')
        acc = anns['XmlAccessorType']
        self.assertTrue(acc.single)
        self.assertEqual((acc.get('value').kind, acc.get('value').name), ('name', 'XmlAccessType.FIELD'))
        self.assertEqual([i.value for i in anns['SuppressWarnings'].get('value').items], ['unchecked', 'rawtypes'])
        els = anns['XmlElements'].get('value')
        self.assertEqual(els.kind, 'array')
        x, y = [i.annotation for i in els.items]
        self.assertEqual((x.get('type').kind, x.get('type').type.name), ('class', 'X'))
        self.assertEqual((y.get('type').type.name, y.get('type').type.dims, y.get('required').value), ('int', 1, True))
        r = anns['Range']
        self.assertEqual([(n, v.kind, v.value) for n, v in r.args if v.kind == 'literal'],
                         [('min', 'literal', -5), ('max', 'literal', 16), ('f', 'literal', 1.5), ('c', 'literal', 'c'), ('n', 'literal', None)])
        self.assertEqual((r.get('expr').kind, r.get('expr').text), ('expr', '1 << 3'))
        self.assertEqual(r.get('s').kind, 'expr')
        self.assertEqual((anns['Marker'].has_parens, anns['Empty'].has_parens, anns['Empty'].args), (False, True, []))
        self.assertEqual(anns['java.lang.Deprecated'].simple_name, 'Deprecated')
        self.assertIs(td.annotation('Deprecated'), anns['java.lang.Deprecated'])
        self.assertEqual((anns['XmlAccessorType'].line, anns['XmlAccessorType'].col), (2, 2))
        json.dumps([a.to_json() for a in td.annotations])

    def test_annotation_type_with_defaults(self):
        td = only_type('@Retention(RetentionPolicy.RUNTIME) public @interface Cfg { String key(); int[] ids() default {1, 2}; '
                       'Class<?> type() default Object.class; String name() default ""; int C = 3; enum Kind { A } }')
        self.assertEqual(td.kind, 'annotation')
        key, ids, typ, name = td.methods
        self.assertIsNone(key.default_value)
        self.assertEqual([i.value for i in ids.default_value.items], [1, 2])
        self.assertEqual(typ.default_value.type.name, 'Object')
        self.assertEqual(name.default_value.value, '')
        self.assertEqual((td.fields[0].name, td.types[0].kind), ('C', 'enum'))

    def test_annotations_interleaved_with_modifiers_and_type_use(self):
        td = only_type('class A { public @Nullable static final String s = null; List<@NonNull String> l; String @A [] arr; }')
        s, l, arr = td.fields
        self.assertEqual((s.modifiers, [a.name for a in s.annotations]), (['public', 'static', 'final'], ['Nullable']))
        self.assertEqual(l.type.args[0].annotations[0].name, 'NonNull')
        self.assertEqual(arr.type.dims, 1)


class Json(unittest.TestCase):
    def test_ir_is_serializable_and_deterministic(self):
        src = ('package p; import java.util.*; /** d */ @A(x = {1, 2}) public class C<T> extends B implements I { int f = 1; '
               'enum E { X { void m() {} } } record R(int a) {} C() {} static { } <U> void g(U u) throws Exception {} }')
        a = json.dumps(parse(src).to_json(), sort_keys=False)
        b = json.dumps(parse(src).to_json(), sort_keys=False)
        self.assertEqual(a, b)
        ir = json.loads(a)
        c = ir['types'][0]
        self.assertEqual((c['fqn'], c['doc'], c['fields'][0]['initializer']['text']), ('p.C', '/** d */', '1'))
        self.assertEqual(c['types'][0]['enum_constants'][0]['body']['methods'][0]['name'], 'm')
        self.assertNotIn('text', c['methods'][1]['body'])
        with_bodies = parse(src).to_json(bodies=True)
        self.assertIn('text', with_bodies['types'][0]['methods'][1]['body'])


class SyntaxErrors(unittest.TestCase):
    def assertError(self, src, line, col, fragment):
        with self.assertRaises(JavaSyntaxError) as cm:
            parse(src)
        e = cm.exception
        self.assertEqual((e.line, e.col), (line, col), str(e))
        self.assertIn(fragment, e.message)

    def test_method_without_return_type(self):
        self.assertError('class A {\n  foo() {} }', 2, 3, 'without a return type')

    def test_garbage_in_class_body(self):
        self.assertError('class A { int x = 1 }', 1, 21, "expected ';'")
        self.assertError('class A { + }', 1, 11, 'expected type')

    def test_top_level_garbage(self):
        self.assertError('package p; int x;', 1, 12, 'expected a type declaration')
        self.assertError('import a.b', 1, 11, "expected ';'")

    def test_bad_annotation(self):
        self.assertError('@A(x = 1, 2) class A {}', 1, 11, 'expected identifier')
        self.assertError('@A(1, 2) class A {}', 1, 5, 'unexpected')

    def test_misplaced_clauses(self):
        self.assertError('interface I implements J {}', 1, 13, 'implements on interface')
        self.assertError('enum E<T> {}', 1, 7, 'type parameters on enum')
        self.assertError('class A { <T> int x; }', 1, 19, 'type parameters on a field')

    def test_bad_enum_constants(self):
        self.assertError('enum E { A B }', 1, 12, 'after enum constants')

    def test_trailing_comma_in_params(self):
        self.assertError('class A { void f(int a,) {} }', 1, 24, 'trailing comma')


if __name__ == '__main__':
    unittest.main()
