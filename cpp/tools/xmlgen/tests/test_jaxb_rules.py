"""V0: JAXB rules of the front end (jaxb.py) on small Java fixtures."""
import unittest

from tests.support import jaxb, model_of

HOLDER = '''
@XmlRootElement(name = "things")
@XmlAccessorType(XmlAccessType.NONE)
public class ThingData {
	@XmlElement(name = "thing")
	private List<Thing> things;
}
'''


def prop(model, cls, name):
    c = model.classes['com.aionemu.gameserver.' + cls]
    for p in c.properties:
        if p.java_name == name:
            return p
    raise AssertionError(f'{cls}.{name} not bound: {[p.java_name for p in c.properties]}')


class NamesTest(unittest.TestCase):
    def test_variable_names(self):
        self.assertEqual(jaxb.jaxb_variable_name('routeStepList'), 'routeStepList')
        self.assertEqual(jaxb.jaxb_variable_name('CLASS_ALL'), 'classALL')
        self.assertEqual(jaxb.jaxb_variable_name('XMLData'), 'xmlData')
        self.assertEqual(jaxb.jaxb_variable_name('hp2ratio'), 'hp2Ratio')
        self.assertEqual(jaxb.jaxb_variable_name('duration2'), 'duration2')
        self.assertEqual(jaxb.jaxb_variable_name('WorldMapTemplate'), 'worldMapTemplate')
        self.assertEqual(jaxb.decapitalize('XmlUid'), 'xmlUid')
        self.assertEqual(jaxb.decapitalize('URL'), 'URL')


class AccessorTypeTest(unittest.TestCase):
    def test_field_access_binds_unannotated_fields_and_is_inherited(self):
        m = model_of({
            'dataholders/ThingData.java': HOLDER,
            'model/Base.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public abstract class Base {
                	protected int shared;
                	private static int COUNTER;
                	private transient int cache;
                	@XmlTransient
                	private int hidden;
                }''',
            'model/Thing.java': '''
                public class Thing extends Base {
                	private Modifiers modifiers;
                	@XmlAttribute(name = "id", required = true)
                	private int id;
                }''',
            'model/Modifiers.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Modifiers {
                	@XmlAttribute
                	private float value = 1.5f;
                }''',
        }, ['dataholders.ThingData'])
        thing = m.classes['com.aionemu.gameserver.model.Thing']
        self.assertEqual(thing.accessor_type, 'FIELD', '@XmlAccessorType is @Inherited')
        modifiers = prop(m, 'model.Thing', 'modifiers')
        self.assertTrue(modifiers.implicit)
        self.assertEqual((modifiers.node, modifiers.xml_name, modifiers.kind), ('element', 'modifiers', 'element'))
        self.assertEqual([p.java_name for p in m.classes['com.aionemu.gameserver.model.Base'].properties], ['shared'])
        self.assertEqual(prop(m, 'model.Modifiers', 'value').initializer.kind, 'literal')
        self.assertIn('com.aionemu.gameserver.model.Modifiers', m.classes, 'implicit element types are reachable')

    def test_public_member_default_binds_public_fields_and_rejects_public_property_pairs(self):
        sources = {
            'dataholders/ThingData.java': HOLDER,
            'model/Thing.java': '''
                @XmlType(name = "Thing")
                public class Thing {
                	public int count;
                	private int secret;
                	@XmlAttribute
                	private int level;
                	public int getLevel() { return level; }
                }''',
        }
        m = model_of(sources, ['dataholders.ThingData'])
        self.assertEqual(m.classes['com.aionemu.gameserver.model.Thing'].accessor_type, 'PUBLIC_MEMBER')
        self.assertEqual([p.java_name for p in m.classes['com.aionemu.gameserver.model.Thing'].properties], ['count', 'level'])
        sources['model/Thing.java'] = '''
            public class Thing {
            	private int level;
            	public int getLevel() { return level; }
            	public void setLevel(int level) { this.level = level; }
            }'''
        with self.assertRaisesRegex(jaxb.XmlGenError, 'public getter/setter pair Level'):
            model_of(sources, ['dataholders.ThingData'])
        m = model_of(sources, ['dataholders.ThingData'],
                     ignore_public_members={'model.Thing.level': 'never in the data'})
        self.assertEqual(m.classes['com.aionemu.gameserver.model.Thing'].properties, [])

    def test_annotated_setter_method_property(self):
        m = model_of({
            'dataholders/ThingData.java': HOLDER,
            'model/Thing.java': '''
                @XmlAccessorType(XmlAccessType.NONE)
                public class Thing {
                	private int id;
                	@XmlID
                	@XmlAttribute(name = "id", required = true)
                	private void setXmlUid(String uid) { id = Integer.parseInt(uid); }
                }''',
        }, ['dataholders.ThingData'])
        p = prop(m, 'model.Thing', 'xmlUid')
        self.assertEqual((p.source, p.setter, p.xml_id, p.required, p.kind), ('method', 'setXmlUid', True, True, 'methodSetter'))

    def test_default_xml_names_that_change_need_confirmation(self):
        sources = {
            'dataholders/ThingData.java': HOLDER,
            'model/Thing.java': '''
                @XmlAccessorType(XmlAccessType.FIELD)
                public class Thing {
                	@XmlAttribute
                	private int hp_ratio;
                }''',
        }
        with self.assertRaisesRegex(jaxb.XmlGenError, "default XML name of hp_ratio is 'hpRatio'"):
            model_of(sources, ['dataholders.ThingData'])
        m = model_of(sources, ['dataholders.ThingData'], xml_names={'model.Thing.hp_ratio': {'name': 'hpRatio', 'reason': 'test'}})
        self.assertEqual(prop(m, 'model.Thing', 'hp_ratio').xml_name, 'hpRatio')


class PropertyKindsTest(unittest.TestCase):
    SOURCES = {
        'dataholders/ThingData.java': HOLDER,
        'dataholders/loadingutils/adapters/LocalDateTimeAdapter.java': '''
            import javax.xml.bind.annotation.adapters.XmlAdapter;
            public class LocalDateTimeAdapter extends XmlAdapter<String, LocalDateTime> {
            	public LocalDateTime unmarshal(String v) { return LocalDateTime.parse(v); }
            	public String marshal(LocalDateTime v) { return v.toString(); }
            }''',
        'model/Race.java': '''
            @XmlEnum
            public enum Race {
            	ELYOS,
            	@XmlEnumValue("asmo")
            	ASMODIANS,
            	PC_ALL;
            }''',
        'model/Action.java': '''
            @XmlAccessorType(XmlAccessType.FIELD)
            public abstract class Action {
            	@XmlAttribute
            	protected int delay;
            	public abstract void act();
            }''',
        'model/SkillAction.java': '''
            public class SkillAction extends Action {
            	@XmlAttribute(name = "skill_id")
            	private int skillId;
            	public void act() {}
            }''',
        'model/AbstractAction.java': '''
            public abstract class AbstractAction extends Action {
            }''',
        'model/Stat.java': '''
            @XmlAccessorType(XmlAccessType.FIELD)
            @XmlType(name = "Stat")
            public class Stat {
            	@XmlAttribute(required = true)
            	private String name;
            }''',
        'model/Thing.java': '''
            @XmlAccessorType(XmlAccessType.FIELD)
            public class Thing {
            	@XmlElements({ @XmlElement(name = "skill", type = SkillAction.class), @XmlElement(name = "abstract", type = AbstractAction.class) })
            	private List<Action> actions;
            	@XmlElementWrapper(name = "stats", required = true)
            	@XmlElement(name = "stat")
            	private List<Stat> stats;
            	@XmlList
            	@XmlElement(name = "races")
            	private List<Race> races;
            	@XmlAttribute(name = "zones")
            	private Set<Race> zones;
            	@XmlAttribute(name = "friend")
            	@XmlIDREF
            	private Thing friend;
            	@XmlAttribute(name = "start")
            	@XmlJavaTypeAdapter(com.aionemu.gameserver.dataholders.loadingutils.adapters.LocalDateTimeAdapter.class)
            	private LocalDateTime start;
            	@XmlAttribute
            	private Race race = Race.PC_ALL;
            	@XmlAttribute
            	private Integer robot;
            	private List<Integer> ids = new ArrayList<>();
            	@XmlAttribute
            	private int weird = Integer.MAX_VALUE;
            }''',
    }
    ADAPTERS = {'dataholders.loadingutils.adapters.LocalDateTimeAdapter': {'cpp': 'LocalDateTime', 'header': 'a.h', 'parse': 'parse',
                                                                           'reason': 'test'}}

    def setUp(self):
        self.m = model_of(self.SOURCES, ['dataholders.ThingData'], adapters=self.ADAPTERS)

    def test_kinds(self):
        m = self.m
        actions = prop(m, 'model.Thing', 'actions')
        self.assertEqual(actions.kind, 'choiceList')
        self.assertEqual(actions.choices, [('skill', 'com.aionemu.gameserver.model.SkillAction'),
                                           ('abstract', 'com.aionemu.gameserver.model.AbstractAction')])
        self.assertTrue(m.classes['com.aionemu.gameserver.model.Action'].choice_base)
        stats = prop(m, 'model.Thing', 'stats')
        self.assertEqual((stats.kind, stats.wrapper, stats.xml_name, stats.required), ('wrapper', 'stats', 'stat', True))
        self.assertEqual(prop(m, 'model.Thing', 'races').kind, 'xmlListElement')
        self.assertEqual(prop(m, 'model.Thing', 'zones').kind, 'attributeCollection')
        self.assertEqual(prop(m, 'model.Thing', 'friend').kind, 'idref')
        self.assertEqual(prop(m, 'model.Thing', 'start').kind, 'adapter')
        self.assertEqual(prop(m, 'model.Thing', 'ids').kind, 'elementList')
        self.assertEqual(prop(m, 'model.Thing', 'race').initializer.value, 'PC_ALL')
        self.assertEqual(prop(m, 'model.Thing', 'ids').initializer.kind, 'empty_collection')
        self.assertEqual(prop(m, 'model.Thing', 'weird').initializer.kind, 'expr')
        self.assertEqual(m.enums['com.aionemu.gameserver.model.Race'].constants, [('ELYOS', 'ELYOS'), ('ASMODIANS', 'asmo'),
                                                                                  ('PC_ALL', 'PC_ALL')])

    def test_unmapped_initializer_is_an_error_in_the_cpp_mapping(self):
        with self.assertRaisesRegex(jaxb.XmlGenError, r'initializer `Integer.MAX_VALUE` needs xmlgen.toml \[initializers\]'):
            from tests.support import cppmodel
            cppmodel.CppModel(self.m).build()

    def test_unconfigured_adapter_and_unsupported_annotations_fail(self):
        with self.assertRaisesRegex(jaxb.XmlGenError, 'LocalDateTimeAdapter is not configured'):
            model_of(self.SOURCES, ['dataholders.ThingData'])
        sources = dict(self.SOURCES)
        sources['model/Stat.java'] = '''
            @XmlAccessorType(XmlAccessType.FIELD)
            public class Stat {
            	@XmlValue
            	private String name;
            }'''
        with self.assertRaisesRegex(jaxb.XmlGenError, 'model/Stat.java:.*unsupported JAXB annotation @XmlValue'):
            model_of(sources, ['dataholders.ThingData'], adapters=self.ADAPTERS)

    def test_unreachable_annotated_types_are_reported(self):
        sources = dict(self.SOURCES)
        sources['model/Orphan.java'] = '@XmlRootElement(name = "orphan")\npublic class Orphan {}'
        m = model_of(sources, ['dataholders.ThingData'], adapters=self.ADAPTERS)
        self.assertEqual(m.unreachable, ['com.aionemu.gameserver.model.Orphan'])

    def test_hooks_and_policy_tables(self):
        sources = dict(self.SOURCES)
        sources['model/Stat.java'] = '''
            @XmlAccessorType(XmlAccessType.FIELD)
            public class Stat {
            	@XmlAttribute(required = true)
            	private String name;
            	private int tempHate;
            	void afterUnmarshal(Unmarshaller u, Object parent) {}
            	void beforeUnmarshal(Unmarshaller u, Object parent) {}
            }'''
        with self.assertRaisesRegex(jaxb.XmlGenError, 'beforeUnmarshal is not supported'):
            model_of(sources, ['dataholders.ThingData'], adapters=self.ADAPTERS)
        m = model_of(sources, ['dataholders.ThingData'], adapters=self.ADAPTERS,
                     before_unmarshal_allowed={'model.Stat': 'test'}, deny_implicit={'model.Stat.tempHate': 'per cast'},
                     unenforced_required={'model.Stat.name': 'data'})
        stat = m.classes['com.aionemu.gameserver.model.Stat']
        self.assertIsNotNone(stat.hook)
        self.assertEqual(stat.denied, [('tempHate', 'per cast')])
        self.assertFalse(prop(m, 'model.Stat', 'name').enforce_required)
        self.assertEqual(m.policy.unused_entries(), [])

    def test_lenient_enums_need_an_optional_scalar_enum(self):
        for key in ('model.Thing.robot', 'model.Thing.race', 'model.Thing.races', 'model.Thing.zones'):
            with self.subTest(key=key), self.assertRaisesRegex(jaxb.XmlGenError, r'\[lenient_enums\] entry is not'):
                model_of(self.SOURCES, ['dataholders.ThingData'], adapters=self.ADAPTERS, lenient_enums={key: 'x'})

if __name__ == '__main__':
    unittest.main()
