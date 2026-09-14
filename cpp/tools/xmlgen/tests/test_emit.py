"""C++ mapping (cppmodel.py), emitted code (emit.py), outputs (irjson.py), drift check and scaffold on small Java fixtures."""
import json
import os
import tempfile
import unittest

from tests.support import generate, xmlgen
from tests.test_jaxb_rules import PropertyKindsTest

NS = '::aion::gameserver::model'


def fixture(extra=None, **tables):
    sources = dict(PropertyKindsTest.SOURCES)
    sources.update(extra or {})
    tables.setdefault('adapters', PropertyKindsTest.ADAPTERS)
    tables.setdefault('initializers', {'model.Thing.weird': {'cpp': '2147483647', 'reason': 'Integer.MAX_VALUE'}})
    return generate(sources, ['dataholders.ThingData'], **tables)


class EmitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = fixture({
            'model/Mutable.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Mutable {
                	@XmlAttribute(name = "z", required = true)
                	private float z;
                	@XmlTransient
                	private boolean active = true;
                	protected Mutable() {}
                	public float getZ() { return z; }
                	public void setZ(float z) { this.z = z; }
                	public boolean isActive() { return active; }
                }''',
            'model/Owner.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Owner {
                	@XmlElement(name = "m")
                	private List<Mutable> list;
                	@XmlAttribute
                	private int value;
                	@XmlAttribute
                	private boolean isDefault;
                	@XmlAttribute(name = "counter")
                	private Race counter = null;
                	@XmlElement(name = "kind")
                	private Race kind;
                	public int getValue() { return value; }
                	public void setValue(int v) { value = v; }
                	public boolean isDefault() { return isDefault; }
                }''',
            'dataholders/ThingData.java': '''
                @XmlRootElement(name = "things")
                @XmlAccessorType(XmlAccessType.NONE)
                public class ThingData {
                	@XmlElement(name = "thing")
                	private List<Thing> things;
                	@XmlElement(name = "owner")
                	private Owner owner;
                	void afterUnmarshal(Unmarshaller u, Object parent) {}
                	public int size() { return things.size(); }
                }''',
        }, runtime_mutable={'model.Mutable.z': 'test', 'model.Mutable.active': 'test'},
            lenient_enums={'model.Owner.counter': 'comma lists in the data', 'model.Owner.kind': 'test'})

    def file(self, rel):
        self.assertIn(rel, self.files, sorted(self.files))
        return self.files[rel]

    def test_enum_header(self):
        text = self.file('aion/gameserver/model/Race.h')
        self.assertIn('enum class Race : uint8_t {\n\tELYOS,\n\tASMODIANS,\n\tPC_ALL,\n};', text)
        self.assertIn('static constexpr std::array<std::string_view, 3> names{', text)
        self.assertLess(text.index('{"PC_ALL"'), text.index('{"asmo"'), 'xmlSorted is in byte order and uses @XmlEnumValue')
        self.assertIn(f'static_assert(verifyEnumTraits<{NS}::Race>());', text)

    def test_data_struct(self):
        text = self.file('aion/gameserver/model/Thing.h')
        self.assertIn('struct Thing : public ::aion::gameserver::runtime::StaticTemplate {', text)     # K1 marker of a hierarchy root
        self.assertIn('#include "aion/gameserver/runtime/lifetime/RefCounted.h"', text)
        expected = [
            f'std::vector<std::unique_ptr<{NS}::Action>> actions;',
            f'std::optional<std::vector<{NS}::Stat>> stats;',
            f'std::optional<std::vector<{NS}::Race>> races;',
            f'std::optional<std::unordered_set<{NS}::Race>> zones;',
            f'const {NS}::Thing* friend_ = nullptr;',
            'std::optional<LocalDateTime> start;',
            f'{NS}::Race race = {NS}::Race::PC_ALL;',
            'std::optional<int32_t> robot;',
            'std::vector<int32_t> ids;',
            'int32_t weird = 2147483647;',
        ]
        for line in expected:
            self.assertIn(line, text)

    def test_behaviour_member_blocks(self):
        inc = self.file('aion/gameserver/model/Action.xml.inc')
        self.assertIn(f'friend struct ::aion::gameserver::xml::XmlBinding<{NS}::Action>;', inc)
        self.assertIn('virtual ~Action() = default;', inc)
        self.assertIn('virtual std::string_view javaClassName() const = 0;', inc)
        self.assertIn('protected:\n\tint32_t delay = 0;', inc)
        skill = self.file('aion/gameserver/model/SkillAction.xml.inc')
        self.assertIn('std::string_view javaClassName() const override { return "SkillAction"; }', skill)
        self.assertIn('#include "aion/gameserver/model/Action.h"', self.file('aion/gameserver/model/SkillAction.xml.h'))
        holder = self.file('aion/gameserver/dataholders/ThingData.xml.inc')
        self.assertIn('void afterUnmarshal(::aion::gameserver::xml::LoadContext& ctx, const ::aion::gameserver::xml::XmlParent& parent);', holder)

    def test_runtime_mutable_accessors_and_renames(self):
        text = self.file('aion/gameserver/model/Mutable.xml.inc')
        self.assertIn('mutable ::aion::gameserver::runtime::Field<float> z{0.0f};', text)
        self.assertIn('mutable ::aion::gameserver::runtime::Field<bool> active{true};', text)
        self.assertIn('float getZ() const { return z.get(); }', text)
        self.assertIn('void setZ(float value) const { z.set(value); }', text, 'the parameter never hides a member')
        self.assertIn('bool isActive() const { return active.get(); }', text)
        owner = self.file('aion/gameserver/model/Owner.h')
        self.assertIn(f'std::vector<std::unique_ptr<{NS}::Mutable>> list;', owner, 'Field members are not movable: stored by pointer')
        self.assertIn('bool isDefault_ = false;', owner, 'a member named like a method is renamed')
        self.assertIn('bool isDefault() const { return isDefault_; }', owner)
        self.assertIn('void setValue(int32_t v) { value = v; }', owner)

    def test_binders(self):
        thing = self.file('aion/gameserver/model/Thing.bind.ipp')
        self.assertIn('choice<::aion::gameserver::model::Action, ::aion::gameserver::model::SkillAction>("skill"),', thing)
        self.assertNotIn('"abstract"', thing, 'abstract choices cannot be instantiated')
        self.assertIn('if (c.bindChoice(o.actions, Thing_actions_FACTORY, e, name))', thing)
        self.assertIn('c.bindWrapper(o.stats, e, "stat");', thing)
        self.assertIn('c.bindTextList(o.races, e);', thing)
        self.assertIn('c.assignList(o.zones, value);', thing)
        self.assertIn('c.idRef(o.friend_, value);', thing)
        self.assertIn('o.start = parse(c, value);', thing)
        self.assertIn('c.bindList(o.ids, e);', thing)
        self.assertIn('case "race"_xh:', thing)
        self.assertIn('c.checkRequiredElements({"stats"});', thing)
        self.assertIn('#include "aion/gameserver/model/SkillAction.h"', thing)
        stat = self.file('aion/gameserver/model/Stat.bind.ipp')
        self.assertIn('c.checkRequiredAttributes({"name"});', stat)
        mutable = self.file('aion/gameserver/model/Mutable.bind.ipp')
        self.assertIn('o.z.set(c.value<float>(value));', mutable)
        self.assertIn('return std::unique_ptr<::aion::gameserver::model::Mutable>(new ::aion::gameserver::model::Mutable());', mutable)
        holder = self.file('aion/gameserver/dataholders/ThingData.bind.ipp')
        self.assertIn('o.things.reserve(counts["thing"]);', holder)
        self.assertIn('c.bindSingle(o.owner, e);', holder)
        self.assertIn('XmlBinding<::aion::gameserver::dataholders::ThingData>::afterUnmarshal(o, c.load(), parent);', holder)
        self.assertIn('#include "aion/gameserver/model/Thing.bind.h"', holder)
        owner = self.file('aion/gameserver/model/Owner.bind.ipp')
        flat = '\n'.join(line.strip() for line in owner.splitlines())
        self.assertIn(f'if (std::optional<{NS}::Race> constant = enumFromXml<{NS}::Race>(trimXml(value))) {{\n'
                      'o.counter = *constant;\n} else {\no.counter.reset();\n'
                      'c.warnOnce("Owner@counter", "unknown Race constant \'" + std::string(value) + "\' is null like JAXB '
                      '(xmlgen.toml [lenient_enums]: comma lists in the data)");\n}\nreturn true;', flat)
        self.assertIn('std::string text = c.elementText(e);', owner)
        self.assertIn(f'enumFromXml<{NS}::Race>(trimXml(text))', owner)
        self.assertIn('c.warnOnce("Owner<kind>", ', owner)
        self.assertIn('#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"', owner)
        self.assertNotIn('c.assign(o.counter', owner)
        ir = json.loads(self.file('xmlmodel.json'))
        counter = next(p for c in ir['classes'] if c['fqn'].endswith('.Owner') for p in c['properties'] if p['javaName'] == 'counter')
        self.assertTrue(counter['lenientEnum'])
        self.assertEqual(counter['cpp']['binding'], 'assignLenientEnum')
        self.assertIn('- model.Owner.counter: comma lists in the data', self.file('xmlgen-report.md'))
        unity = self.file('aion/gameserver/model/model.bind.cpp')
        self.assertIn('#include "aion/gameserver/model/Thing.bind.ipp"', unity)

    def test_ir_and_classes_json(self):
        ir = json.loads(self.file('xmlmodel.json'))
        self.assertEqual((ir['format'], ir['version']), ('aion-xmlmodel', 1))
        thing = next(c for c in ir['classes'] if c['fqn'] == 'com.aionemu.gameserver.model.Thing')
        props = {p['javaName']: p for p in thing['properties']}
        self.assertEqual(props['actions']['node'], 'element')
        self.assertIsNone(props['actions']['xmlName'])
        self.assertEqual(props['actions']['choices'][0], {'xmlName': 'skill', 'typeFqn': 'com.aionemu.gameserver.model.SkillAction'})
        self.assertEqual((props['stats']['wrapperName'], props['stats']['xmlName'], props['stats']['typeFqn']),
                         ('stats', 'stat', 'com.aionemu.gameserver.model.Stat'))
        self.assertNotIn('typeFqn', props['races'])
        classes = json.loads(self.file('staticdata-classes.json'))
        mutable = next(c for c in classes['classes'] if c['fqn'] == 'com.aionemu.gameserver.model.Mutable')
        self.assertEqual([m['field'] for m in mutable['runtimeMutable']], ['z', 'active'])
        self.assertIn('## Behaviour classes and port checklist', self.file('xmlgen-report.md'))

    def test_scaffold(self):
        from tests.support import jaxb  # noqa: F401
        import scaffold
        out = dict(scaffold.scaffold(self.cm, ['dataholders.ThingData']))
        header = out['aion/gameserver/dataholders/ThingData.h']
        self.assertIn('#include "aion/gameserver/dataholders/ThingData.xml.h"', header)
        self.assertIn('class ThingData : public ::aion::gameserver::runtime::StaticTemplate {\n'
                      '#include "aion/gameserver/dataholders/ThingData.xml.inc"\npublic:', header)
        self.assertIn('#include "aion/gameserver/runtime/lifetime/RefCounted.h"', self.file('aion/gameserver/dataholders/ThingData.xml.h'),
                      'the prelude provides the marker base of the shell')
        self.assertIn('// TODO port: public int size()', header)
        source = out['aion/gameserver/dataholders/ThingData.cpp']
        self.assertIn('void ThingData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {\n\tAION_UNPORTED();', source)
        self.assertIn('return things.size();', source)


class NestedClassesTest(unittest.TestCase):
    def test_nested_enums_data_and_behaviour_classes_and_containers(self):
        import scaffold
        cm, files = generate({
            'dataholders/ThingData.java': '''
                @XmlRootElement(name = "things")
                @XmlAccessorType(XmlAccessType.FIELD)
                public class ThingData {
                	@XmlElement(name = "entry")
                	private List<Entry> entries;
                	@XmlElement(name = "special")
                	private List<Special> specials;
                	@XmlElement(name = "fluid")
                	private List<com.aionemu.gameserver.model.Groups.FluidGroup> fluids;
                	@XmlAttribute
                	private Mode mode = Mode.FAST;
                	public int size() { return entries.size(); }
                	@XmlType(name = "Mode")
                	@XmlEnum
                	public enum Mode { SLOW, FAST }
                	@XmlAccessorType(XmlAccessType.FIELD)
                	private static class Entry {
                		@XmlAttribute
                		private int id;
                	}
                	@XmlAccessorType(XmlAccessType.FIELD)
                	public static abstract class Base {
                		@XmlAttribute
                		protected int value;
                		void afterUnmarshal(Unmarshaller u, Object parent) {}
                	}
                	public static class Special extends Base {
                	}
                }''',
            'model/Groups.java': '''
                public final class Groups {
                	@XmlAccessorType(XmlAccessType.FIELD)
                	@XmlType(name = "FluidGroup")
                	public static class FluidGroup {
                		@XmlAttribute
                		private int count;
                	}
                }''',
        }, ['dataholders.ThingData'])
        enum = files['aion/gameserver/dataholders/ThingData_Mode.h']
        self.assertIn('enum class ThingData_Mode : uint8_t {', enum)
        self.assertIn('static constexpr std::string_view javaName = "Mode";', enum)
        self.assertIn('struct ThingData_Entry : public ::aion::gameserver::runtime::StaticTemplate {', files['aion/gameserver/dataholders/ThingData_Entry.h'])
        inc = files['aion/gameserver/dataholders/ThingData.xml.inc']
        self.assertIn('class Base; // hand-written nested class, defined after ThingData', inc)
        self.assertIn('class Special; // hand-written nested class, defined after ThingData', inc)
        self.assertIn('using Entry = ::aion::gameserver::dataholders::ThingData_Entry;', inc)
        self.assertIn('using Mode = ::aion::gameserver::dataholders::ThingData_Mode;', inc)
        self.assertIn('std::vector<::aion::gameserver::dataholders::ThingData::Special> specials;', inc)
        container = files['aion/gameserver/model/Groups.h']
        self.assertIn('struct Groups {\n\tusing FluidGroup = ::aion::gameserver::model::Groups_FluidGroup;', container)
        binder = files['aion/gameserver/dataholders/ThingData.bind.ipp']
        self.assertIn('XmlBinding<::aion::gameserver::dataholders::ThingData::Base>::afterUnmarshal(o, c.load(), parent);', binder,
                      'a subclass without its own hook calls the nearest declared hook')
        header = dict(scaffold.scaffold(cm, ['dataholders.ThingData']))['aion/gameserver/dataholders/ThingData.h']
        marker = ' : public ::aion::gameserver::runtime::StaticTemplate {'
        self.assertLess(header.index('class ThingData' + marker), header.index('class ThingData::Base' + marker))
        self.assertLess(header.index('class ThingData::Base' + marker),
                        header.index('class ThingData::Special : public ::aion::gameserver::dataholders::ThingData::Base {'))


class IncludeCycleTest(unittest.TestCase):
    def test_pointer_members_of_an_include_cycle_are_forward_declared(self):
        _, files = fixture({
            'model/Stat.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Stat {
                	@XmlAttribute(required = true)
                	private String name;
                	@XmlElement(name = "back")
                	private Back back;
                	void afterUnmarshal(Unmarshaller u, Object parent) {}
                }''',
            'model/Back.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Back {
                	@XmlElement(name = "stat")
                	private Stat stat;
                }''',
        })
        back = files['aion/gameserver/model/Back.h']
        stat = files['aion/gameserver/model/Stat.xml.h']
        back_includes_stat = '#include "aion/gameserver/model/Stat.h"' in back
        stat_includes_back = '#include "aion/gameserver/model/Back.h"' in stat
        self.assertFalse(back_includes_stat and stat_includes_back, 'the include cycle is broken')
        self.assertTrue(back_includes_stat or 'class Stat;' in back)
        self.assertTrue(stat_includes_back or 'struct Back;' in stat)


class GenerateAndCheckTest(unittest.TestCase):
    def test_generate_prunes_and_check_reports_drift(self):
        _, files = fixture()
        with tempfile.TemporaryDirectory() as out:
            os.makedirs(os.path.join(out, 'concurrency'))
            keep = os.path.join(out, 'concurrency', 'fieldmap.json')
            with open(keep, 'w') as f:
                f.write('{}')
            stale = os.path.join(out, 'aion', 'gameserver', 'old', 'Old.h')
            os.makedirs(os.path.dirname(stale))
            with open(stale, 'w') as f:
                f.write('old')
            written, removed = xmlgen.generate(out, files, src=None)
            self.assertEqual((written, removed), (len(files), 1))
            self.assertTrue(os.path.exists(keep), 'generated/concurrency belongs to fieldmap.py')
            self.assertFalse(os.path.exists(os.path.dirname(stale)))
            self.assertEqual(xmlgen.drift(out, files), [])
            changed = os.path.join(out, 'aion', 'gameserver', 'model', 'Race.h')
            with open(changed, 'a') as f:
                f.write('// edited\n')
            os.remove(os.path.join(out, 'xmlmodel.json'))
            with open(os.path.join(out, 'aion', 'extra.h'), 'w') as f:
                f.write('')
            self.assertEqual(xmlgen.drift(out, files), [('stale', 'aion/extra.h'), ('changed', 'aion/gameserver/model/Race.h'),
                                                        ('missing', 'xmlmodel.json')])

    def test_case_collisions_and_src_conflicts_fail(self):
        with self.assertRaisesRegex(xmlgen.XmlGenError, 'differ only in case'):
            xmlgen.check_case_collisions({'aion/a/X.h': '', 'aion/a/x.h': ''})
        with tempfile.TemporaryDirectory() as src:
            os.makedirs(os.path.join(src, 'aion', 'a'))
            with open(os.path.join(src, 'aion', 'a', 'X.h'), 'w') as f:
                f.write('')
            with self.assertRaisesRegex(xmlgen.XmlGenError, 'both src/ and generated/'):
                xmlgen.check_src_conflicts({'aion/a/X.h': '', 'aion/a/Y.xml.inc': ''}, src)


if __name__ == '__main__':
    unittest.main()
