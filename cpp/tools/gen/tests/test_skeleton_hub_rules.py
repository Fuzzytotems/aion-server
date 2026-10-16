"""skeleton.py rules of the S0b hub header guide (docs/design/hub-headers.md) on a small throwaway Java tree: erasure of bounded generics,
nullable or reference parameters, parts and owners, cast-only overrides, VisibleObject construction, interned immortals, interfaces held by
Ref, collection-field getters, varargs and Object parameters; plus a /W4 /WX compile of the result."""
from __future__ import annotations

import os
import shutil
import unittest

from tests import test_skeleton_support as ss

import skeleton

P = 'com.aionemu.gameserver.'
JAVA = {
    'com/aionemu/gameserver/model/gameobjects/AionObject.java': '''package com.aionemu.gameserver.model.gameobjects;
public abstract class AionObject {
	private final int objectId;
	public AionObject(int objId) { this.objectId = objId; }
	public int getObjectId() { return objectId; }
	public abstract String getName();
}
''',
    'com/aionemu/gameserver/model/gameobjects/VisibleObject.java': '''package com.aionemu.gameserver.model.gameobjects;
import com.aionemu.gameserver.controllers.VisibleObjectController;
import com.aionemu.gameserver.world.zone.ZoneName;
public abstract class VisibleObject extends AionObject {
	private final VisibleObjectController<? extends VisibleObject> controller;
	private VisibleObject target;
	public VisibleObject(int objId, VisibleObjectController<? extends VisibleObject> controller, VisibleObject creator) {
		super(objId);
		this.controller = controller;
	}
	public VisibleObjectController<? extends VisibleObject> getController() { return controller; }
	public VisibleObject getTarget() { return target; }
	public void setTarget(VisibleObject target) { this.target = target; }
	public boolean canSee(VisibleObject object) { return object != null; }
	public void see(VisibleObject object) { object.getName(); }
	public boolean isInside(ZoneName zone) { return false; }
	public void onEvent(int id, Object... args) { }
	public void delete(int... ids) { }
	public void spawn(int id) { }
	public void spawn(int id, int... ids) { }
	public void notice(VisibleObject object) { }
	public VisibleObject nearest(VisibleObject from) { return null; }
	public void say(String text, int id) { }
}
''',
    'com/aionemu/gameserver/model/gameobjects/Npc.java': '''package com.aionemu.gameserver.model.gameobjects;
import java.util.List;
import java.util.ArrayList;
import com.aionemu.gameserver.controllers.NpcController;
public class Npc extends VisibleObject {
	private final List<Npc> minions = new ArrayList<>();
	public Npc(NpcController controller) {
		super(1, controller, null);
		controller.setOwner(this);
	}
	@Override
	public NpcController getController() { return (NpcController) super.getController(); }
	@Override
	public String getName() { return "npc"; }
	@Override
	public void see(VisibleObject object) { if (object == null) return; }
	public List<Npc> getMinions() { return minions; }
	public void clear() { setTarget(null); notice(nearest(canSee(this) ? this : null)); }
	private void say(int text, int id) { }
}
''',
    'com/aionemu/gameserver/controllers/VisibleObjectController.java': '''package com.aionemu.gameserver.controllers;
import com.aionemu.gameserver.model.gameobjects.VisibleObject;
public abstract class VisibleObjectController<T extends VisibleObject> {
	private T owner;
	public void setOwner(T owner) { this.owner = owner; }
	public T getOwner() { return owner; }
	public void onTarget(T target) { }
}
''',
    'com/aionemu/gameserver/controllers/NpcController.java': '''package com.aionemu.gameserver.controllers;
import com.aionemu.gameserver.model.gameobjects.Npc;
public class NpcController extends VisibleObjectController<Npc> {
	@Override
	public void onTarget(Npc target) { }
}
''',
    'com/aionemu/gameserver/world/zone/ZoneName.java': '''package com.aionemu.gameserver.world.zone;
public final class ZoneName {
	private final String name;
	private ZoneName(String name) { this.name = name; }
	public static ZoneName get(String name) { return null; }
}
''',
    'com/aionemu/gameserver/instance/handlers/InstanceHandler.java': '''package com.aionemu.gameserver.instance.handlers;
import com.aionemu.gameserver.model.gameobjects.Npc;
public interface InstanceHandler {
	void onDie(Npc npc);
}
''',
    'com/aionemu/gameserver/instance/handlers/GeneralInstanceHandler.java': '''package com.aionemu.gameserver.instance.handlers;
import com.aionemu.gameserver.model.gameobjects.Npc;
public class GeneralInstanceHandler implements InstanceHandler {
	@Override
	public void onDie(Npc npc) { }
}
''',
    'com/aionemu/gameserver/utils/SplitList.java': '''package com.aionemu.gameserver.utils;
public class SplitList<Type> {
	public void add(Type element) { }
}
''',
}
FIELDMAP = {'classes': {
    P + 'model.gameobjects.AionObject': {'kind': 'K4', 'base': 'RefCounted', 'members': [{'javaName': 'objectId', 'cppType': 'const int32_t'}]},
    P + 'model.gameobjects.VisibleObject': {'kind': 'K4', 'members': [
        {'javaName': 'controller', 'cppType': 'const std::unique_ptr<VisibleObjectController>'},
        {'javaName': 'target', 'cppType': 'Field<Ref<VisibleObject>>'}]},
    P + 'model.gameobjects.Npc': {'kind': 'K4', 'members': [{'javaName': 'minions', 'cppType': 'ArrayList<Ref<Npc>>'}]},
    P + 'controllers.VisibleObjectController': {'kind': 'K4', 'base': 'OwnedPart', 'members': [{'javaName': 'owner', 'cppType': 'Final<T*>'}]},
    P + 'controllers.NpcController': {'kind': 'K4'},
    P + 'world.zone.ZoneName': {'kind': 'K3', 'base': 'Immortal', 'members': [{'javaName': 'name', 'cppType': 'const std::string'}]},
    P + 'instance.handlers.GeneralInstanceHandler': {'kind': 'K4', 'base': 'RefCounted'},
    P + 'utils.SplitList': {'kind': 'K5'},
}}


@unittest.skipUnless(ss.REAL_CPP_SRC.is_dir(), 'needs the runtime headers')
class HubRulesTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = ss.short_temp_dir('skhubrules')
        ss.write_tree(cls.root / 'java', JAVA)
        (cls.root / 'cpp').mkdir()
        cls.project = skeleton.Project(cls.root / 'java', None, cls.root / 'cpp', None, skeleton.Fieldmap.from_json(FIELDMAP))
        cls.fwd = skeleton.generate_fwd(cls.project)
        cls.drafts = skeleton.generate_drafts(cls.project, cls.project.select(['@all']))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, ignore_errors=True)

    def h(self, rel):
        return self.drafts[f'aion/gameserver/{rel}.h']

    def cpp(self, rel):
        return self.drafts[f'aion/gameserver/{rel}.cpp']

    def test_erasure(self):
        self.assertTrue(self.project.erased_generic(self.project.index.types[P + 'controllers.VisibleObjectController']))
        self.assertFalse(self.project.erased_generic(self.project.index.types[P + 'utils.SplitList']))   # unbounded: template
        self.assertIn('class VisibleObjectController;', self.fwd['aion/gameserver/controllers/fwd.h'])
        self.assertIn('template <class Type> class SplitList;', self.fwd['aion/gameserver/utils/fwd.h'])
        controller = self.h('controllers/VisibleObjectController')
        self.assertIn('runtime::Final<model::gameobjects::VisibleObject*> owner{};', controller)
        self.assertIn('model::gameobjects::VisibleObject& getOwner() const { return *this->owner.get(); }', controller)
        self.assertIn('void setOwner(model::gameobjects::VisibleObject& owner); // owner binding', controller)
        self.assertIn('\tthis->owner.set(&value);\n\tbindOwner(value);', self.cpp('controllers/VisibleObjectController'))
        # the override of a method with a type-variable parameter takes the erased type (javac bridge)
        self.assertIn('void onTarget(model::gameobjects::VisibleObject& target) override;', self.h('controllers/NpcController'))
        self.assertIn('class NpcController : public VisibleObjectController {', self.h('controllers/NpcController'))

    def test_nullable_and_reference_parameters(self):
        visible = self.h('model/gameobjects/VisibleObject')
        self.assertIn('void setTarget(runtime::Ptr<VisibleObject> target);', visible)            # setTarget(null) and a setter
        self.assertIn('\tbool canSee(runtime::Ptr<VisibleObject> object);', visible)             # compared with null
        self.assertIn('virtual void see(runtime::Ptr<VisibleObject> object);', visible)          # a subclass override compares with null
        self.assertIn('void onEvent(int32_t id, std::initializer_list<std::any> args = {});', visible)
        self.assertIn('void delete_(std::initializer_list<int32_t> ids = {});', visible)
        self.assertIn('void spawn(int32_t id, std::initializer_list<int32_t> ids);', visible)   # no `= {}`: spawn(int) would be ambiguous
        # rule 1 counts only direct arguments: the null branch belongs to nearest(...), not to notice(...)
        self.assertIn('	void notice(VisibleObject& object);', visible)
        self.assertIn('runtime::Ptr<VisibleObject> nearest(runtime::Ptr<VisibleObject> from);', visible)
        # a private helper with other parameter types is not an override: say(String, int) stays non-virtual
        self.assertIn('	void say(std::string_view text, int32_t id);', visible)
        self.assertNotIn('virtual void say(', visible)
        self.assertIn('bool isInside(const world::zone::ZoneName* zone);', visible)             # interned immortal
        self.assertIn('static const ZoneName* get(std::string_view name);', self.h('world/zone/ZoneName'))
        # constructor: the part is a unique_ptr, the null literal of super(..., null) makes `creator` nullable
        self.assertIn('VisibleObject(CreateKey key, int32_t objId, std::unique_ptr<controllers::VisibleObjectController> controller, '
                      'runtime::Ptr<VisibleObject> creator);', visible)
        self.assertNotIn('static runtime::Ref<VisibleObject> create', visible)

    def test_parts_casts_and_construction(self):
        visible = self.h('model/gameobjects/VisibleObject')
        self.assertIn('controllers::VisibleObjectController& getController() const; // part accessor', visible)
        self.assertIn('\treturn *this->controller;', self.cpp('model/gameobjects/VisibleObject'))
        npc = self.h('model/gameobjects/Npc')
        self.assertIn('\tNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller);', npc)
        self.assertIn('struct CreateKey {', visible)
        self.assertIn('[[nodiscard]] static runtime::Ref<T> create(Args&&... args) {', visible)
        self.assertNotIn('static runtime::Ref<Npc> create', npc)
        self.assertIn('controllers::NpcController& getController() const; // narrows VisibleObject::getController (Java cast-only override)', npc)
        source = self.cpp('model/gameobjects/Npc')
        self.assertIn('return static_cast<controllers::NpcController&>(VisibleObject::getController());', source)
        self.assertIn(': VisibleObject(key, int32_t{}, nullptr, runtime::Ptr<VisibleObject>{}) {', source)
        self.assertIn('#include "aion/gameserver/controllers/NpcController.h"', source)
        self.assertIn('runtime::ArrayList<runtime::Ref<Npc>>& getMinions() { return this->minions; }', npc)   # live collection
        self.assertIn('runtime::ArrayList<runtime::Ref<Npc>> minions{AION_LOCK_CLASS(Npc::minions)};', npc)   # RR-16 lock class (§4)
        # cast-only overrides do not make the base virtual
        self.assertIn('\tcontrollers::VisibleObjectController& getController() const;', visible)
        self.assertNotIn('virtual controllers::VisibleObjectController& getController', visible)

    def test_retainable_interfaces(self):
        handler = self.h('instance/handlers/InstanceHandler')
        self.assertIn('virtual void retain() const noexcept = 0;', handler)
        self.assertIn('virtual void onDie(model::gameobjects::Npc& npc) = 0;', handler)
        general = self.h('instance/handlers/GeneralInstanceHandler')
        self.assertIn('class GeneralInstanceHandler : public runtime::RefCounted, public InstanceHandler {', general)
        self.assertIn('void retain() const noexcept override { runtime::RefCounted::retain(); }', general)
        self.assertIn('void release() const noexcept override { runtime::RefCounted::release(); }', general)

    def test_cast_override_detection(self):
        npc = self.project.index.types[P + 'model.gameobjects.Npc']
        get_controller = next(m for m in npc.methods if m.name == 'getController')
        self.assertTrue(skeleton.Project.is_cast_override(get_controller))
        self.assertFalse(skeleton.Project.is_cast_override(next(m for m in npc.methods if m.name == 'getName')))

    @unittest.skipUnless(os.name == 'nt' and ss.find_cmake() is not None and os.environ.get('AION_SKELETON_SKIP_COMPILE') != '1',
                         'needs CMake and MSVC (AION_SKELETON_SKIP_COMPILE=1 skips)')
    def test_compile(self):
        work = ss.short_temp_dir('skhubc')
        try:
            ss.write_tree(work / 'inc', {**self.fwd, **self.drafts})
            sources = [work / 'inc' / f for f in sorted(self.drafts) if f.endswith('.cpp')]
            ok, output = ss.compile_check(work, [work / 'inc', ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC, ss.VCPKG_INCLUDE], sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB8029' not in line and 'MSB8074' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
