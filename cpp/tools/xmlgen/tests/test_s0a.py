"""Spine step S0a (handlers-and-porting-plan.md §2.5): core enums (decision 3), behaviour class shells (decision 4), the [ignore_attributes]
statements and the class adapter template on small Java fixtures."""
import contextlib
import io
import os
import tempfile
import unittest

from tests.support import generate, jaxb, xmlgen

import scaffold

NS = '::aion::gameserver::model'

SOURCES = {
    'dataholders/ThingData.java': '''
        @XmlRootElement(name = "things")
        @XmlAccessorType(XmlAccessType.NONE)
        public class ThingData {
        	@XmlElement(name = "thing")
        	private List<Thing> things;
        	@XmlElement(name = "step")
        	private List<Step> steps;
        	@XmlElement(name = "calc")
        	private Calc calc;
        	void afterUnmarshal(Unmarshaller u, Object parent) {}
        }''',
    'model/Thing.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class Thing {
        	@XmlAttribute
        	private Race race;
        	@XmlElement(name = "equipment")
        	private Gear equipment;
        	@XmlAttribute(name = "id", required = true)
        	private int id;
        }''',
    'model/Race.java': 'public enum Race { ELYOS, ASMODIANS }',
    'model/Calc.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class Calc {
        	@XmlAttribute
        	private int x;
        	public int twice() { return x * 2; }
        }''',
    'model/Step.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class Step {
        	@XmlAttribute
        	private float x;
        	protected Step() {}
        	@XmlAttribute(name = "uid")
        	public void setXmlUid(String uid) { x = Float.parseFloat(uid); }
        	public float size() { return x * 2; }
        	public enum Kind { FIRST, LAST }
        }''',
    'model/Gear.java': '''
        /** @author Luno */
        @XmlJavaTypeAdapter(GearAdapter.class)
        public class Gear {
        	private GearList v;
        	public Gear(GearList v) { this.v = v; }
        	public void init() { synchronized (this) { v = null; } }
        }''',
    'model/GearAdapter.java': '''
        import javax.xml.bind.annotation.adapters.XmlAdapter;
        public class GearAdapter extends XmlAdapter<GearList, Gear> {
        	public Gear unmarshal(GearList v) { return new Gear(v); }
        	public GearList marshal(Gear v) { return null; }
        }''',
    'model/GearList.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class GearList {
        	@XmlElement(name = "item")
        	private List<Integer> items;
        }''',
    'model/Unbound.java': '''
        public class Unbound {
        	/** a core enum nested in a class that JAXB never binds */
        	public enum State { CONNECTED, AUTHED, DELETE }
        	private enum Hidden { A }
        }''',
    'model/Special.java': '''
        public enum Special {
        	ONE { public int value() { return 1; } },
        	NULL(2) { public int value() { return 2; } };
        	Special() {}
        	Special(int x) {}
        	public abstract int value();
        }''',
    'model/Holder.java': '''
        public class Holder {
        	public Mood mood() { return null; }
        }
        enum Mood { HAPPY, SAD }''',
    'configs/Handwritten.java': '''
        public class Handwritten {
        	public enum Mode { NONE, FULL }
        }''',
}
ADAPTERS = {'model.GearAdapter': {
    'value_type': 'model.GearList', 'cpp': f'std::unique_ptr<{NS}::Gear>', 'header': 'aion/gameserver/model/Gear.h',
    'unmarshal': f'c.replaceSingle(o.{{member}}, std::make_unique<{NS}::Gear>(std::move(value)), e);\no.{{member}}->init(c.load());',
    'reason': 'test'}}
TABLES = dict(adapters=ADAPTERS, ignore_attributes={'model.Thing.cName': 'client name'}, core_enums=True,
              hand_written_enums={'configs.Handwritten.Mode': 'hand-written'})


def fixture(**overrides):
    tables = dict(TABLES)
    tables.update(overrides)
    return generate(SOURCES, ['dataholders.ThingData'], **tables)


class CoreEnumsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = fixture()

    def test_every_enum_of_the_tree_is_emitted(self):
        enums = {e.fqn.rpartition('gameserver.')[2]: e for e in self.cm.enums.values()}
        self.assertEqual(sorted(enums), ['model.Mood', 'model.Race', 'model.Special', 'model.Step.Kind', 'model.Unbound.Hidden',
                                         'model.Unbound.State'])
        self.assertFalse(enums['model.Race'].model.core, 'reached by JAXB')
        self.assertTrue(enums['model.Unbound.State'].model.core)
        state = self.files['aion/gameserver/model/Unbound_State.h']
        self.assertIn('enum class Unbound_State : uint8_t {\n\tCONNECTED,\n\tAUTHED,\n\tDELETE,\n};', state)
        self.assertIn('(not bound by JAXB)', state)
        self.assertIn('static constexpr std::string_view javaName = "State";', state)
        self.assertIn(f'static_assert(verifyEnumTraits<{NS}::Unbound_State>());', state)

    def test_constant_bodies_are_emitted_as_constants_and_listed(self):
        special = self.files['aion/gameserver/model/Special.h']
        self.assertIn('enum class Special : uint8_t {\n\tONE,\n\tNULL_, // Java NULL\n};', special)
        self.assertIn('the constant-specific bodies are hand-written', special)
        report = self.files['xmlgen-report.md']
        self.assertIn('- model.Special (methods, constructor arguments, constant bodies, core)', report)
        self.assertIn('| Core enums (not bound by JAXB, policy core_enums) | 5 |', report)

    def test_nested_core_enum_of_a_bound_class_is_aliased(self):
        inc = self.files['aion/gameserver/model/Step.xml.inc']
        self.assertIn(f'using Kind = {NS}::Step_Kind;', inc)
        self.assertIn('#include "aion/gameserver/model/Step_Kind.h"', self.files['aion/gameserver/model/Step.xml.h'])
        report = self.files['xmlgen-report.md']
        section = report[report.index('## Nested enums without a generated alias'):report.index('## Core enums with a hand-written')]
        self.assertIn(f'- model.Unbound.State -> `{NS}::Unbound_State`', section)
        self.assertNotIn('model.Step.Kind', section)

    def test_hand_written_enums_are_skipped_and_checked(self):
        self.assertNotIn('aion/gameserver/configs/Handwritten_Mode.h', self.files)
        self.assertIn('- configs.Handwritten.Mode: hand-written', self.files['xmlgen-report.md'])
        with self.assertRaisesRegex(jaxb.XmlGenError, r'\[hand_written_enums\] model.Race is bound by JAXB'):
            fixture(hand_written_enums={'configs.Handwritten.Mode': 'x', 'model.Race': 'x'})
        with self.assertRaisesRegex(jaxb.XmlGenError, 'unused xmlgen.toml entries'):
            fixture(core_enums=False)

    def test_core_enums_are_not_static_data_classes(self):
        import json
        classes = json.loads(self.files['staticdata-classes.json'])['classes']
        fqns = {c['fqn'] for c in classes}
        self.assertIn('com.aionemu.gameserver.model.Race', fqns)
        self.assertNotIn('com.aionemu.gameserver.model.Unbound.State', fqns)
        ir = json.loads(self.files['xmlmodel.json'])
        self.assertEqual({e['fqn']: e['core'] for e in ir['enums']}['com.aionemu.gameserver.model.Unbound.State'], True)

    def test_without_core_enums_only_reachable_enums(self):
        cm, files = generate(SOURCES, ['dataholders.ThingData'], adapters=ADAPTERS, ignore_attributes={'model.Thing.cName': 'client name'})
        self.assertEqual(sorted(e.java_name for e in cm.enums.values()), ['Race'])
        self.assertNotIn('using Kind', files['aion/gameserver/model/Step.xml.inc'])

    def test_generate_refuses_a_hand_written_nested_definition(self):
        with tempfile.TemporaryDirectory() as src:
            os.makedirs(os.path.join(src, 'aion', 'gameserver', 'model'))
            with open(os.path.join(src, 'aion', 'gameserver', 'model', 'Unbound.h'), 'w') as f:
                f.write('class Unbound {\n\tenum class State { CONNECTED };\n};\n')
            with self.assertRaisesRegex(jaxb.XmlGenError, r'model.Unbound.State \(aion.gameserver.model.Unbound.h\)'):
                xmlgen.check_src_conflicts(self.files, src, self.cm)

    def test_generate_refuses_a_hand_written_secondary_enum_definition(self):
        """a secondary top-level enum (Mood in Holder.java) is generated as Mood.h: the hand-written Holder.h must not define it; a comment
        naming it or an alias is fine"""
        with tempfile.TemporaryDirectory() as src:
            holder = os.path.join(src, 'aion', 'gameserver', 'model', 'Holder.h')
            os.makedirs(os.path.dirname(holder))
            with open(holder, 'w') as f:
                f.write('#include "aion/gameserver/model/Mood.h"\n// enum class Mood: generated by xmlgen\nclass Holder {\n'
                        '\tusing State = ::aion::gameserver::model::Unbound_State;\n};\n')
            xmlgen.check_src_conflicts(self.files, src, self.cm)
            with open(holder, 'a') as f:
                f.write('struct X {}; enum class Mood : uint8_t { HAPPY, SAD };\n')
            with self.assertRaisesRegex(jaxb.XmlGenError, r'model.Mood \(aion.gameserver.model.Holder.h\)'):
                xmlgen.check_src_conflicts(self.files, src, self.cm)


class BindingStatementsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = fixture()

    def test_ignored_attributes_call_ignore_attribute(self):
        ipp = '\n'.join(line.strip() for line in self.files['aion/gameserver/model/Thing.bind.ipp'].splitlines())
        self.assertIn('if (name == "cName") {\nstatic_cast<void>(value); // not bound by Java, ignored like JAXB (xmlgen.toml '
                      '[ignore_attributes]: client name)\nc.ignoreAttribute(); // BindStats counts it as ignored\nreturn true;\n}', ipp)

    def test_class_adapter_replaces_the_single_object(self):
        ipp = '\n'.join(line.strip() for line in self.files['aion/gameserver/model/Thing.bind.ipp'].splitlines())
        self.assertIn(f'std::unique_ptr<{NS}::GearList> value = constructBound<{NS}::GearList>();\nc.bindObject(*value, e, c.currentObject());\n'
                      f'c.replaceSingle(o.equipment, std::make_unique<{NS}::Gear>(std::move(value)), e);\no.equipment->init(c.load());', ipp)
        self.assertNotIn('o.equipment = ', ipp)

    def test_the_real_policy_uses_replace_single(self):
        doc, _ = xmlgen.load_config(xmlgen.DEFAULT_CONFIG)
        template = doc['adapters']['dataholders.loadingutils.adapters.NpcEquippedGearAdapter']['unmarshal']
        self.assertTrue(template.startswith('c.replaceSingle(o.{member}, '), template)
        self.assertTrue(doc['core_enums'])


class ShellsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = fixture()
        cls.shells = dict(scaffold.scaffold_all(cls.cm))

    def test_all_top_level_behaviour_classes_and_adapter_targets(self):
        self.assertEqual(sorted(self.shells), ['aion/gameserver/dataholders/ThingData.cpp', 'aion/gameserver/dataholders/ThingData.h',
                                               'aion/gameserver/model/Calc.h', 'aion/gameserver/model/Gear.cpp', 'aion/gameserver/model/Gear.h',
                                               'aion/gameserver/model/Step.cpp', 'aion/gameserver/model/Step.h'],
                         'no Calc.cpp: a source without definitions is not written')

    def test_shell_contains_only_what_the_generated_code_needs(self):
        step = self.shells['aion/gameserver/model/Step.h']
        self.assertEqual(step, '#pragma once\n\n#include "aion/gameserver/model/Step.xml.h"\n\nnamespace aion::gameserver::model {\n\n'
                               '/** Java com.aionemu.gameserver.model.Step. */\n'
                               'class Step : public ::aion::gameserver::runtime::StaticTemplate {\n#include "aion/gameserver/model/Step.xml.inc"\n'
                               'protected:\n\tStep() = default; // Java: protected Step()\n};\n\n} // namespace aion::gameserver::model\n')
        source = self.shells['aion/gameserver/model/Step.cpp']
        self.assertIn(f'#include "{scaffold.UNPORTED_HEADER}"', source)
        self.assertIn('void Step::setXmlUid(std::string_view /*uid*/) {\n\tAION_UNPORTED();\n}', source)
        self.assertNotIn('Java:', source)
        self.assertNotIn('TODO', step)
        holder = self.shells['aion/gameserver/dataholders/ThingData.cpp']
        self.assertIn('void ThingData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {\n\tAION_UNPORTED();\n}',
                      holder)
        self.assertIn('class ThingData : public ::aion::gameserver::runtime::StaticTemplate {\n'
                      '#include "aion/gameserver/dataholders/ThingData.xml.inc"\npublic:\n};',
                      self.shells['aion/gameserver/dataholders/ThingData.h'])
        self.assertEqual(scaffold.UNPORTED_HEADER, 'aion/gameserver/runtime/base/Unported.h')

    def test_class_adapter_target_shell(self):
        header = self.shells['aion/gameserver/model/Gear.h']
        self.assertIn('#include "aion/gameserver/model/GearList.h"', header)
        self.assertIn('/** Java com.aionemu.gameserver.model.Gear (shell of the class-level adapter model.GearAdapter: the contract of the '
                      'generated binders). @author Luno */', header)
        self.assertIn(f'\texplicit Gear(std::unique_ptr<{NS}::GearList> v);\n\tvoid init(::aion::gameserver::xml::LoadContext& ctx);\n', header)
        self.assertIn('private:\n\t// fieldmap: owns the bound adapter value', header, 'lint waiver: the shell does not follow fieldmap.json yet')
        self.assertIn(f'\tstd::unique_ptr<{NS}::GearList> v;', header)
        self.assertIn('// fieldmap: shell; the other Java members arrive with the port of the class', header)
        source = self.shells['aion/gameserver/model/Gear.cpp']
        self.assertIn(f'Gear::Gear(std::unique_ptr<{NS}::GearList> value) : v(std::move(value)) {{\n}}', source)
        self.assertIn('// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED or documents the load-time confinement\n'
                      'void Gear::init(::aion::gameserver::xml::LoadContext& /*ctx*/) {\n\tAION_UNPORTED();\n}', source)

    def test_scaffold_with_comments_keeps_the_port_checklist(self):
        out = dict(scaffold.scaffold(self.cm, ['model.Step']))
        self.assertIn('protected:\n\tStep() = default; // Java: protected Step()\npublic:\n\t// TODO port: public float size()',
                      out['aion/gameserver/model/Step.h'])
        self.assertIn('/* Java:', out['aion/gameserver/model/Step.cpp'])

    def test_cli_scaffold_all_never_overwrites(self):
        with tempfile.TemporaryDirectory() as src:
            existing = os.path.join(src, 'aion', 'gameserver', 'model', 'Step.h')
            os.makedirs(os.path.dirname(existing))
            with open(existing, 'w') as f:
                f.write('// hand-written\n')
            created, kept = xmlgen.write_scaffold(src, scaffold.scaffold_all(self.cm))
            self.assertEqual([os.path.normpath(p) for p in kept], [existing])
            self.assertEqual(len(created), 6)
            with open(existing) as f:
                self.assertEqual(f.read(), '// hand-written\n')
            again, kept = xmlgen.write_scaffold(src, scaffold.scaffold_all(self.cm))
            self.assertEqual((again, len(kept)), ([], 7))

    def test_cli_needs_classes_or_all(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            xmlgen.main(['--java-src', os.devnull, 'scaffold'])


if __name__ == '__main__':
    unittest.main()
