"""Spine step S0c follow-ups (S0B-016, S0B-130): generated accessors that a Java subclass overrides are virtual, and a class adapter target
can be a RefCounted class held by runtime::Ref (NpcEquippedGear)."""
import os
import unittest

from tests.support import GENERATED, generate, xmlgen

import scaffold

NS = '::aion::gameserver::model'
REF_NS = '::aion::gameserver::runtime'

VIRTUAL_SOURCES = {
    'dataholders/EffectData.java': '''
        @XmlRootElement(name = "effects")
        @XmlAccessorType(XmlAccessType.NONE)
        public class EffectData {
        	@XmlElements({ @XmlElement(name = "dot", type = DotEffect.class), @XmlElement(name = "hit", type = HitEffect.class) })
        	private List<BaseEffect> effects;
        }''',
    'model/BaseEffect.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public abstract class BaseEffect {
        	@XmlAttribute
        	protected int duration;
        	@XmlAttribute
        	protected int value;
        	@XmlAttribute
        	protected boolean noresist;
        	public int getDuration() { return duration; }
        	public int getValue() { return value; }
        	public boolean isNoResist() { return noresist; }
        }''',
    'model/DotEffect.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class DotEffect extends BaseEffect {
        	@Override
        	public int getDuration() { return duration + 1000; }
        }''',
    'model/HitEffect.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class HitEffect extends BaseEffect {
        	@XmlAttribute
        	protected boolean cannotmiss;
        	@Override
        	public boolean isNoResist() { return cannotmiss || super.isNoResist(); }
        }''',
}


class VirtualAccessorsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = generate(VIRTUAL_SOURCES, ['dataholders.EffectData'])

    def test_overridden_getters_are_virtual(self):
        inc = self.files['aion/gameserver/model/BaseEffect.xml.inc']
        self.assertIn('\tvirtual int32_t getDuration() const { return duration; }', inc)
        self.assertIn('\tvirtual bool isNoResist() const { return noresist; }', inc)

    def test_getters_nobody_overrides_stay_non_virtual(self):
        inc = self.files['aion/gameserver/model/BaseEffect.xml.inc']
        self.assertIn('\tint32_t getValue() const { return value; }', inc)
        self.assertNotIn('virtual int32_t getValue()', inc)

    def test_the_overriding_methods_are_hand_written(self):
        self.assertNotIn('getDuration', self.files['aion/gameserver/model/DotEffect.xml.inc'])
        self.assertNotIn('isNoResist', self.files['aion/gameserver/model/HitEffect.xml.inc'])

    def test_real_effect_template_getters(self):
        """AbstractOverTimeEffect.getValue/getDuration2 and SkillAttackInstantEffect.isNoResist override them (their shells say override)"""
        with open(os.path.join(GENERATED, 'aion', 'gameserver', 'skillengine', 'effect', 'EffectTemplate.xml.inc'), encoding='utf-8') as f:
            inc = f.read()
        for getter in ('virtual int32_t getValue() const', 'virtual int32_t getDuration2() const', 'virtual bool isNoResist() const'):
            self.assertIn(getter, inc)
        self.assertEqual(inc.count('virtual '), 5, 'the destructor, javaClassName() and the three overridden getters')


REF_SOURCES = {
    'dataholders/ThingData.java': '''
        @XmlRootElement(name = "things")
        @XmlAccessorType(XmlAccessType.NONE)
        public class ThingData {
        	@XmlElement(name = "thing")
        	private List<Thing> things;
        }''',
    'model/Thing.java': '''
        @XmlAccessorType(XmlAccessType.FIELD)
        public class Thing {
        	@XmlElement(name = "equipment")
        	private Gear equipment;
        	public Gear getEquipment() { return equipment; }
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
}
REF_ADAPTERS = {'model.GearAdapter': {
    'value_type': 'model.GearList', 'cpp': f'{REF_NS}::Ref<{NS}::Gear>', 'header': 'aion/gameserver/model/Gear.h',
    'unmarshal': f'c.replaceSingle(o.{{member}}, {NS}::Gear::create(std::move(value)), e);\no.{{member}}->init(c.load());',
    'reason': 'test'}}


class RefCountedClassAdapterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cm, cls.files = generate(REF_SOURCES, ['dataholders.ThingData'], adapters=REF_ADAPTERS)
        cls.shells = dict(scaffold.scaffold_all(cls.cm))

    def test_member_and_accessor(self):
        inc = self.files['aion/gameserver/model/Thing.h']  # data-only: generated completely
        self.assertIn(f'\t{REF_NS}::Ref<{NS}::Gear> equipment; // @XmlElement(name = "equipment") class adapter GearAdapter', inc)
        self.assertIn(f'\t{REF_NS}::Ptr<{NS}::Gear> getEquipment() const {{ return equipment; }}', inc)

    def test_binder_creates_and_replaces(self):
        ipp = '\n'.join(line.strip() for line in self.files['aion/gameserver/model/Thing.bind.ipp'].splitlines())
        self.assertIn(f'c.bindObject(*value, e, c.currentObject());\nc.replaceSingle(o.equipment, {NS}::Gear::create(std::move(value)), e);\n'
                      'o.equipment->init(c.load());', ipp)

    def test_target_shell_is_ref_counted(self):
        header = self.shells['aion/gameserver/model/Gear.h']
        self.assertIn('#include "aion/gameserver/runtime/lifetime/RefCounted.h"', header)
        self.assertIn(f'class Gear : public {REF_NS}::RefCounted {{\n\tAION_MAKE_REF_FRIEND\n\npublic:\n'
                      f'\tstatic {REF_NS}::Ref<Gear> create(std::unique_ptr<{NS}::GearList> v);\n'
                      '\tvoid init(::aion::gameserver::xml::LoadContext& ctx);\n\nprotected:\n'
                      f'\texplicit Gear(std::unique_ptr<{NS}::GearList> v);\n\t~Gear() override;\n', header)
        source = self.shells['aion/gameserver/model/Gear.cpp']
        self.assertIn(f'{REF_NS}::Ref<Gear> Gear::create(std::unique_ptr<{NS}::GearList> value) {{\n'
                      f'\treturn {REF_NS}::makeRef<Gear>(std::move(value));\n}}', source)
        self.assertIn('Gear::~Gear() = default;', source)

    def test_the_real_policy_holds_npc_equipped_gear_by_ref(self):
        doc, _ = xmlgen.load_config(xmlgen.DEFAULT_CONFIG)
        table = doc['adapters']['dataholders.loadingutils.adapters.NpcEquippedGearAdapter']
        self.assertEqual(table['cpp'], f'{REF_NS}::Ref<::aion::gameserver::model::items::NpcEquippedGear>')
        self.assertIn('NpcEquippedGear::create(std::move(value))', table['unmarshal'])


if __name__ == '__main__':
    unittest.main()
