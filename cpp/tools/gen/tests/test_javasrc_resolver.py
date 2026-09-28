"""Type resolver (ProjectIndex) and CLI tests for javasrc."""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import javasrc  # noqa: E402
from javasrc import ProjectIndex, parse_source  # noqa: E402

FILES = {
    'root1/com/x/model/Base.java': '''package com.x.model;
public abstract class Base<T> {
    public static class Template {}
    public interface Listener { enum Kind { A } }
}''',
    'root1/com/x/model/Item.java': '''package com.x.model;
import java.util.*;
import javax.xml.bind.annotation.*;
import com.x.util.Helper;
@XmlAccessorType(XmlAccessType.FIELD)
public class Item extends Base<Item> implements Base.Listener, Comparable<Item> {
    public static class Inner { class Deeper {} }
    Template template;          // inherited member type
    Kind kind;                  // member of an inherited interface
    Inner inner;
    Inner.Deeper deeper;
    Helper helper;              // single-type import beats the same package
    Other other;                // same package
    List<String> names;         // java.util.* (known JDK package)
    String s; Runnable r;       // java.lang
    Timer timer;                // project package via wildcard would collide; java.util.Timer here
    <E extends Number> E pick(Map.Entry<E, ? super Item> e) { return null; }
    com.x.util.Helper qualified;
    java.util.concurrent.ConcurrentHashMap<String, Item> chm;
    Missing missing;
    void body() {
        class LocalType {}
        LocalType lt = null;
        Runnable anon = new Runnable() { Template t; public void run() {} };
    }
}''',
    'root1/com/x/model/Other.java': 'package com.x.model;\nclass Other {}\nclass Helper {}\n',
    'root2/com/x/util/Helper.java': '''package com.x.util;
public final class Helper { public enum Mode { ON, OFF } }''',
    'root2/com/x/util/Timer.java': 'package com.x.util;\npublic class Timer {}\n',
    'root2/com/x/svc/Service.java': '''package com.x.svc;
import static com.x.util.Helper.Mode;
import static com.x.util.Helper.*;
import com.x.model.*;
import com.x.util.*;
import org.quartz.*;
public class Service {
    Mode mode;        // static import of a member type
    Item item;        // wildcard project package
    Timer timer;      // project wildcard beats java.lang/JDK tables (java.util is not imported here)
    Trigger trigger;  // only opaque wildcard package: guessed
    Other hidden;     // package-private in com.x.model: still found through the wildcard
}''',
    'root2/com/x/svc/Clash.java': '''package com.x.svc;
import com.x.model.*;
import com.x.other.*;
public class Clash extends Thread { Dup dup; Kind k; }''',
    'root2/com/x/other/Dup.java': 'package com.x.other;\npublic class Dup {}\nclass Kind {}\n',
    'root2/com/x/model/Dup.java': 'package com.x.model;\npublic class Dup {}\n',
    'root2/com/x/svc/Ext.java': '''package com.x.svc;
import org.quartz.*;
public class Ext extends org.quartz.Job { Trigger trigger; }''',
    'root2/com/x/svc/Enums.java': '''package com.x.svc;
public enum Enums { A { Inner i; }; static class Inner {} }''',
}


class ResolverFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        for rel, text in FILES.items():
            path = os.path.join(cls.tmp.name, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(text)
        cls.roots = [os.path.join(cls.tmp.name, 'root1'), os.path.join(cls.tmp.name, 'root2')]
        cls.idx = ProjectIndex.from_roots(cls.roots)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def field_kind(self, fqn, field):
        td = self.idx.lookup(fqn)
        f = [x for x in td.fields if x.name == field][0]
        return self.idx.resolve_kind(f.type.name, f)

    def test_index_contents(self):
        self.assertEqual(len(self.idx.units), len(FILES))
        self.assertEqual([u.relpath for u in self.idx.units][:3], ['com/x/model/Base.java', 'com/x/model/Item.java', 'com/x/model/Other.java'])
        self.assertIn('com.x.model.Item.Inner.Deeper', self.idx.types)
        self.assertEqual(self.idx.packages['com.x.model']['Other'], 'com.x.model.Other')
        self.assertEqual(self.idx.duplicates, [])

    def test_duplicates_are_reported(self):
        idx = ProjectIndex()
        idx.add_unit(parse_source('package a; class X {}', 'one.java'))
        idx.add_unit(parse_source('package a; class X {}', 'two.java'))
        self.assertEqual(idx.duplicates, [('a.X', 'one.java', 'two.java')])

    def test_item_fields(self):
        item = 'com.x.model.Item'
        cases = {
            'template': ('project', 'com.x.model.Base.Template'),
            'kind': ('project', 'com.x.model.Base.Listener.Kind'),
            'inner': ('project', 'com.x.model.Item.Inner'),
            'deeper': ('project', 'com.x.model.Item.Inner.Deeper'),
            'helper': ('project', 'com.x.util.Helper'),
            'other': ('project', 'com.x.model.Other'),
            'names': ('external', 'java.util.List'),
            's': ('external', 'java.lang.String'),
            'r': ('external', 'java.lang.Runnable'),
            'timer': ('external', 'java.util.Timer'),
            'qualified': ('project', 'com.x.util.Helper'),
            'chm': ('external', 'java.util.concurrent.ConcurrentHashMap'),
            'missing': ('unresolved', None),
        }
        for field, expected in cases.items():
            with self.subTest(field=field):
                self.assertEqual(self.field_kind(item, field), expected)

    def test_method_type_vars_and_nested_external(self):
        item = self.idx.lookup('com.x.model.Item')
        pick = [m for m in item.methods if m.name == 'pick'][0]
        self.assertEqual(self.idx.resolve_kind('E', pick), ('typevar', 'E'))
        self.assertEqual(self.idx.resolve(pick.params[0].type.name, pick), 'java.util.Map.Entry')
        tree = self.idx.resolve_type(pick.params[0].type, pick)
        self.assertEqual(tree['args'][0], {'name': 'E', 'fqn': None, 'kind': 'typevar', 'args': None, 'dims': 0})
        self.assertEqual(tree['args'][1]['bound']['fqn'], 'com.x.model.Item')
        self.assertEqual(self.idx.resolve_kind('T', self.idx.lookup('com.x.model.Base')), ('typevar', 'T'))
        self.assertEqual(self.idx.resolve('int', item), 'int')

    def test_annotations(self):
        item = self.idx.lookup('com.x.model.Item')
        self.assertEqual(self.idx.resolve(item.annotations[0].name, item), 'javax.xml.bind.annotation.XmlAccessorType')
        self.assertEqual(self.idx.resolve('XmlAccessType.FIELD', item), 'javax.xml.bind.annotation.XmlAccessType.FIELD')

    def test_local_and_anonymous_contexts(self):
        item = self.idx.lookup('com.x.model.Item')
        body = [m for m in item.methods if m.name == 'body'][0].body
        kind, value = self.idx.resolve_kind('LocalType', body)
        self.assertEqual((kind, value.name), ('local', 'LocalType'))
        self.assertIsNone(self.idx.resolve('LocalType', body))
        anon = body.anonymous_classes()[0].anonymous
        self.assertEqual(self.idx.resolve(anon.fields[0].type.name, anon.fields[0]), 'com.x.model.Base.Template')
        self.assertEqual(self.idx.resolve('Runnable', anon), 'java.lang.Runnable')

    def test_service_imports(self):
        svc = 'com.x.svc.Service'
        self.assertEqual(self.field_kind(svc, 'mode'), ('project', 'com.x.util.Helper.Mode'))
        self.assertEqual(self.field_kind(svc, 'item'), ('project', 'com.x.model.Item'))
        self.assertEqual(self.field_kind(svc, 'timer'), ('project', 'com.x.util.Timer'))
        self.assertEqual(self.field_kind(svc, 'trigger'), ('external_guess', 'org.quartz.Trigger'))
        self.assertEqual(self.idx.resolve('Trigger', self.idx.lookup(svc)), 'org.quartz.Trigger')
        self.assertEqual(self.field_kind(svc, 'hidden'), ('project', 'com.x.model.Other'))

    def test_ambiguity_and_external_supertypes(self):
        self.assertEqual(self.field_kind('com.x.svc.Clash', 'dup'), ('ambiguous', ['com.x.model.Dup', 'com.x.other.Dup']))
        self.assertEqual(self.field_kind('com.x.svc.Ext', 'trigger'), ('unresolved', None))  # may be a member of the external Job
        self.assertEqual(self.idx.supertypes(self.idx.lookup('com.x.svc.Ext')), [])
        self.assertEqual([t.fqn for t in self.idx.supertypes(self.idx.lookup('com.x.model.Item'))],
                         ['com.x.model.Base', 'com.x.model.Base.Listener'])

    def test_enum_constant_body_context(self):
        enums = self.idx.lookup('com.x.svc.Enums')
        body = enums.enum_constants[0].body
        self.assertEqual(self.idx.resolve(body.fields[0].type.name, body.fields[0]), 'com.x.svc.Enums.Inner')
        self.assertEqual(self.idx.supertypes(body), [enums])

    def test_member_type_lookup(self):
        item = self.idx.lookup('com.x.model.Item')
        self.assertEqual(self.idx.member_type(item, 'Template').fqn, 'com.x.model.Base.Template')
        self.assertIsNone(self.idx.member_type(item, 'Nope'))

    def test_learned_packages_from_explicit_imports(self):
        idx = ProjectIndex()
        idx.add_unit(parse_source('package a; import org.lib.Widget; class A {}', 'A.java'))
        cu = parse_source('package b; import org.lib.*; class B { Widget w; }', 'B.java')
        idx.add_unit(cu)
        b = cu.types[0]
        self.assertEqual(idx.resolve_kind('Widget', b.fields[0]), ('external', 'org.lib.Widget'))

    def test_json_ir(self):
        ir = self.idx.to_json(resolve=True)
        text = json.dumps(ir)
        self.assertEqual(text, json.dumps(self.idx.to_json(resolve=True)))
        unit = [u for u in ir['units'] if u['path'] == 'com/x/model/Item.java'][0]
        fields = {f['name']: f for f in unit['types'][0]['fields']}
        self.assertEqual((fields['template']['type']['fqn'], fields['template']['type']['resolution']), ('com.x.model.Base.Template', 'project'))
        self.assertEqual(fields['names']['type']['args'][0]['fqn'], 'java.lang.String')
        self.assertEqual(ir['version'], javasrc.IR_VERSION)

    def test_cli(self):
        out_path = os.path.join(self.tmp.name, 'ir.json')
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = javasrc.main(['--ir', out_path, '--resolve', '--bodies'] + self.roots)
        self.assertEqual(rc, 0)
        self.assertIn(f'parsed {len(FILES)} files', buf.getvalue())
        self.assertIn('analysed', buf.getvalue())
        with open(out_path, encoding='utf-8') as f:
            ir = json.load(f)
        self.assertEqual(len(ir['units']), len(FILES))

    def test_bad_context(self):
        with self.assertRaises(TypeError):
            self.idx.resolve('X', object())


if __name__ == '__main__':
    unittest.main()
