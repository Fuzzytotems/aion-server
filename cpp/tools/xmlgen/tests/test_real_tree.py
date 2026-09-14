"""The generator over the real Java tree: drift check of the committed cpp/game-server/generated tree (`xmlgen.py check`), model sizes
against the research counts (docs/design/research/static-data-jaxb.md) and V1, the independent XSD cross-check of the IR
(tools/oracle, with the reviewed v1_allowlist.json)."""
import collections
import json
import os
import sys
import unittest

from tests.support import GENERATED, JAVA_SRC, ORACLE, REPO, XMLGEN, jaxb, xmlgen

STATIC_DATA = os.path.join(REPO, 'game-server', 'data', 'static_data')


@unittest.skipUnless(os.path.isdir(JAVA_SRC), 'Java sources not found')
class RealTreeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = xmlgen.build()
        cls.ir = json.loads(cls.files['xmlmodel.json'])

    def test_generated_tree_is_up_to_date(self):
        diffs = xmlgen.drift(GENERATED, self.files)
        self.assertEqual(diffs[:20], [], f'{len(diffs)} generated files differ: run python cpp/tools/xmlgen/xmlgen.py generate')

    def test_output_is_deterministic(self):
        _, again = xmlgen.build(index=self.cm.model.index)
        self.assertEqual(again.keys(), self.files.keys())
        self.assertTrue(all(again[k] == self.files[k] for k in self.files))

    def test_model_matches_the_research_counts(self):
        classes = self.ir['classes']
        kinds = collections.Counter(p['kind'] for c in classes for p in c['properties'])
        self.assertEqual(len(classes), 744)
        self.assertEqual(len(self.ir['enums']), 115, '87 annotated enums plus the unannotated enums of bound properties')
        self.assertEqual(kinds['choiceList'], 12, '12 @XmlElements lists')
        self.assertEqual(sum(len(p['choices']) for c in classes for p in c['properties'] if p.get('choices')), 287)
        self.assertEqual(kinds['wrapper'], 8)
        self.assertEqual(kinds['xmlListAttribute'] + kinds['xmlListElement'], 28)
        self.assertEqual(kinds['adapter'], 7)
        self.assertEqual(kinds['methodSetter'], 3)
        self.assertEqual(kinds['idref'], 2)
        self.assertEqual(sum(1 for c in classes for p in c['properties'] if p['implicit']), 31 - 3, '31 implicit fields, 3 denied')
        self.assertEqual(sum(1 for c in classes if c['afterUnmarshal']), 111)
        holders = next(c for c in classes if c['xmlRootElement'] == 'static_data')['properties']
        self.assertEqual(len(holders), 92)
        self.assertEqual(self.cm.policy.unused_entries(), [])

    @unittest.skipUnless(os.path.isdir(STATIC_DATA), 'static data not found')
    def test_v1_ir_matches_the_xsds(self):
        if ORACLE not in sys.path:
            sys.path.insert(0, ORACLE)
        from staticdata_oracle import xsdcheck
        with open(os.path.join(XMLGEN, 'v1_allowlist.json'), encoding='utf-8') as f:
            allowlist = json.load(f)
        report = xsdcheck.check(self.ir, xsdcheck.load_schemas(STATIC_DATA), allowlist)
        self.assertEqual(report['differences'], [])
        self.assertEqual(report['staleAllowlist'], [])
        self.assertTrue(report['ok'])
        self.assertGreater(report['summary']['matched'], 700)

    @unittest.skipUnless(os.path.isdir(STATIC_DATA), 'static data not found')
    def test_every_enum_value_of_the_data_is_a_constant_or_lenient(self):
        """Streams the merged static data (oracle V2 import rules) through the IR and checks every value of an enum-typed attribute or
        text element against the enum tables: an unknown constant fails the generated binder (parseEnum) unless the property is in
        [lenient_enums]. Also checks that the data uses no attribute or element the IR does not bind (except [ignore_attributes])."""
        if ORACLE not in sys.path:
            sys.path.insert(0, ORACLE)
        from pathlib import Path
        from staticdata_oracle import merged
        walker = EnumValueWalker(self.ir)
        merged.stream_static_data(Path(STATIC_DATA), [walker])
        self.assertGreater(walker.checked, 1_000_000)
        lenient = {f'{c["fqn"]}.{p["javaName"]}' for c in self.ir['classes'] for p in c['properties'] if p['lenientEnum']}
        self.assertEqual({k for k in walker.unknown_values if k not in lenient}, set(),
                         {k: dict(v) for k, v in walker.unknown_values.items() if k not in lenient})
        self.assertEqual(lenient - set(walker.unknown_values), set(), 'stale [lenient_enums] entries (the data has only known constants)')
        self.assertEqual(dict(walker.unknown_values['com.aionemu.gameserver.skillengine.model.SkillTemplate.counterSkill']),
                         {'BLOCK,RESIST': 22, 'RESIST,PARRY': 6, 'RESIST,DODGE': 2})
        self.assertEqual(set(walker.unbound), set(jaxb.long_fqn(k) for k in self.cm.policy.ignore_attributes))
        # the generated finish() enforces requiredAttributes/requiredElements in strict and lenient loading; JAXB never does, so every
        # violation in the data needs xmlgen.toml [unenforced_required]
        self.assertEqual(dict(walker.missing_required), {})
        self.assertGreater(walker.required_checked, 100_000)

    def test_static_data_holder_table(self):
        table = self.files['aion/gameserver/dataholders/StaticDataHolders.xml.h']
        self.assertIn('X("world_maps", ::aion::gameserver::dataholders::WorldMapsData, worldMapsData)', table)
        self.assertEqual(table.count('\tX('), 92)


class EnumValueWalker:
    """merged.stream_static_data visitor: resolves elements to IR classes (declared type, @XmlElements choice, wrapper item, superclass
    properties) and collects unknown enum values per property, unbound attributes/elements and missing required attributes/elements (the
    flattened requiredAttributes/requiredElements of the IR, checked like BindContext: per element, per holder over all its files)."""
    WS = ' \t\r\n'

    def __init__(self, ir):
        self.classes = {c['fqn']: c for c in ir['classes']}
        self.enums = {e['fqn']: frozenset(k['xml'] for k in e['constants']) for e in ir['enums']}
        self.tables = {}
        self.stack = []
        self.checked = 0
        self.unknown_values = collections.defaultdict(collections.Counter)  # 'class fqn.javaName' -> value -> count
        self.unbound = collections.Counter()  # 'class fqn.xml name' (attributes) / 'class fqn<element>'
        self.present = []  # per stack entry: (attribute names, child element names)
        self.missing_required = collections.Counter()  # 'path (class fqn)@attribute' / 'path (class fqn)<element>'
        self.required_checked = 0
        self.holder_tag = None

    def table(self, fqn):
        t = self.tables.get(fqn)
        if t is None:
            chain = []
            c = self.classes[fqn]
            while c is not None:
                chain.append(c)
                c = self.classes[c['superclass']] if c['superclass'] else None
            attrs, elems = {}, {}
            for c in reversed(chain):
                for p in c['properties']:
                    key = (c['fqn'], p)
                    if p['node'] == 'attribute':
                        attrs[p['xmlName']] = key
                    elif p.get('choices'):
                        for choice in p['choices']:
                            elems[choice['xmlName']] = ('class', choice['typeFqn'])
                    elif p.get('wrapperName'):
                        elems[p['wrapperName']] = ('wrapper', key)
                    elif p.get('typeFqn'):
                        elems[p['xmlName']] = ('class', p['typeFqn'])
                    else:
                        elems[p['xmlName']] = ('scalar', key)
            t = self.tables[fqn] = (attrs, elems)
        return t

    def check(self, key, value):
        owner, p = key
        vt = p['valueType']
        if vt['kind'] != 'enum':
            return
        values = value.split() if (p['collection'] or p['xmlList']) else [value.strip(self.WS)]
        constants = self.enums[vt['fqn']]
        for v in values:
            self.checked += 1
            if v not in constants:
                self.unknown_values[f'{owner}.{p["javaName"]}'][v] += 1

    def attributes(self, frame, attrib):
        if frame[0] != 'class':
            return
        attrs = self.table(frame[1])[0]
        for name, value in attrib.items():
            if name.startswith('{'):
                continue
            key = attrs.get(name)
            if key is None:
                self.unbound[f'{frame[1]}.{name}'] += 1
            else:
                self.check(key, value)

    def child_frame(self, parent, tag):
        if parent[0] == 'class':
            e = self.table(parent[1])[1].get(tag)
            if e is not None:
                return e
            self.unbound[f'{parent[1]}<{tag}>'] += 1
        elif parent[0] == 'wrapper':
            owner, p = parent[1]
            if tag == p['xmlName']:
                return ('class', p['typeFqn']) if p.get('typeFqn') else ('scalar', parent[1])
            self.unbound[f'{owner}.{p["wrapperName"]}<{tag}>'] += 1
        elif parent[0] == 'scalar':
            self.unbound[f'{parent[1][0]}.{parent[1][1]["javaName"]}<{tag}>'] += 1
        return ('unbound', tag)

    def check_required(self, frame, present, where):
        if frame[0] != 'class':
            return
        c = self.classes[frame[1]]
        self.required_checked += 1
        for name in c['requiredAttributes']:
            if name not in present[0]:
                self.missing_required[f'{where} ({frame[1]})@{name}'] += 1
        for name in c['requiredElements']:
            if name not in present[1]:
                self.missing_required[f'{where} ({frame[1]})<{name}>'] += 1

    def begin_holder(self, imp, file, tag, attrib):
        frame = self.child_frame(('class', 'com.aionemu.gameserver.dataholders.StaticData'), tag)
        self.stack = [frame]
        self.present = [({n for n in attrib if not n.startswith('{')}, set())]
        self.holder_tag = tag
        self.attributes(frame, attrib)

    def start(self, path, tag, attrib):
        self.present[-1][1].add(tag)
        frame = self.child_frame(self.stack[-1], tag)
        self.stack.append(frame)
        self.present.append(({n for n in attrib if not n.startswith('{')}, set()))
        self.attributes(frame, attrib)

    def end(self, path, element):
        frame = self.stack.pop()
        self.check_required(frame, self.present.pop(), f'{self.holder_tag}/' + '/'.join(path))
        if frame[0] == 'scalar':
            self.check(frame[1], element.text or '')

    def end_holder(self, imp):
        self.check_required(self.stack[0], self.present[0], self.holder_tag)


if __name__ == '__main__':
    unittest.main()
