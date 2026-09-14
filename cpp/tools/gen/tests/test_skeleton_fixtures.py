"""skeleton.py on the small Java fixture tree: golden C++ output, unit tests of the mapping rules, --check, and a compile check."""
from __future__ import annotations

import io
import json
import os
import shutil
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from tests import test_skeleton_support as ss

import skeleton

P = 'com.aionemu.gameserver.'


def fixture_project(fieldmap=True, handlers=True):
    fm = skeleton.Fieldmap.load(ss.FIXTURE_FIELDMAP) if fieldmap else skeleton.Fieldmap()
    return skeleton.Project(ss.FIXTURE_JAVA, ss.FIXTURE_HANDLERS if handlers else None, ss.FIXTURE_CPP, None, fm)


def compare_golden(test, subdir, files):
    """Compares files with tests/fixtures/skeleton/expected/<subdir> (AION_SKELETON_UPDATE=1 rewrites it)."""
    root = ss.EXPECTED / subdir
    if ss.UPDATE:
        if root.exists():
            shutil.rmtree(root)
        ss.write_tree(root, files)
    on_disk = sorted(p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file())
    test.assertEqual(on_disk, sorted(files), f'golden file set differs in {root} (AION_SKELETON_UPDATE=1 regenerates it)')
    for rel, text in sorted(files.items()):
        expected = (root / rel).read_text(encoding='utf-8')
        test.assertEqual(expected, text, f'{subdir}/{rel} differs from the golden file (AION_SKELETON_UPDATE=1 regenerates it)')


class GoldenTest(unittest.TestCase):
    """Fixture Java -> expected C++ (the expected files are reviewed like code)."""

    @classmethod
    def setUpClass(cls):
        cls.project = fixture_project()

    def test_forward_headers(self):
        compare_golden(self, 'fwd', skeleton.generate_fwd(self.project))

    def test_drafts_with_fieldmap(self):
        files = skeleton.generate_drafts(self.project, self.project.select(['@all']))
        compare_golden(self, 'draft', files)

    def test_drafts_without_fieldmap(self):
        project = fixture_project(fieldmap=False)
        files = skeleton.generate_drafts(project, project.select([P + 'model.gameobjects.Creature']))
        compare_golden(self, 'draft-nofieldmap', files)


class ForwardHeaderTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.files = skeleton.generate_fwd(fixture_project(fieldmap=False, handlers=False))

    def test_one_header_per_package_with_types(self):
        self.assertIn('aion/gameserver/model/gameobjects/fwd.h', self.files)
        self.assertIn('aion/gameserver/questEngine/handlers/template/fwd.h', self.files)
        self.assertNotIn('aion/gameserver/questEngine/fwd.h', self.files)   # package without types

    def test_declarations(self):
        model = self.files['aion/gameserver/model/fwd.h']
        self.assertIn('enum class Race : std::uint8_t;', model)
        self.assertIn('enum class Gender;', model)                          # existing C++ enum without an underlying type
        self.assertIn('//   Race::Group  (Java type nested in an enum)', model)
        self.assertIn('#include <cstdint>', model)
        self.assertIn('struct GSConfig;', self.files['aion/gameserver/configs/main/fwd.h'])
        self.assertIn('class CreatureController;', self.files['aion/gameserver/controllers/fwd.h'])      # erased generic (bounded T)
        self.assertNotIn('template', self.files['aion/gameserver/controllers/fwd.h'])
        self.assertIn('namespace aion::gameserver::questEngine::handlers::template_ {',
                      self.files['aion/gameserver/questEngine/handlers/template/fwd.h'])
        stats = self.files['aion/gameserver/model/stats/fwd.h']
        self.assertIn('class BaseFunction;', stats)                         # secondary top-level class of Functions.java
        self.assertIn('class Functions;', stats)


class NamesAndConstantsTest(unittest.TestCase):

    def test_keyword_rule(self):
        self.assertEqual(skeleton.cpp_ident('register'), 'register_')
        self.assertEqual(skeleton.cpp_ident('delete'), 'delete_')
        self.assertEqual(skeleton.cpp_ident('template'), 'template_')
        self.assertEqual(skeleton.cpp_ident('NULL'), 'NULL_')
        self.assertEqual(skeleton.cpp_ident('FLT_EPSILON'), 'FLT_EPSILON_')
        self.assertEqual(skeleton.cpp_ident('level'), 'level')
        # reserved C++ names: the dialogaction.cpp_identifier rule that sysmsg.py uses
        self.assertEqual(skeleton.cpp_ident('_STR_MSG_Heal_TO_ME'), 'STR_MSG_Heal_TO_ME_')
        self.assertEqual(skeleton.cpp_ident('STR_RESURRECT_DIALOG__SKILL'), 'STR_RESURRECT_DIALOG_SKILL_')
        import dialogaction
        for name in ('_STR_MSG_Heal_TO_ME', 'STR_RESURRECT_DIALOG__SKILL', 'register', 'level'):
            self.assertEqual(skeleton.cpp_ident(name), dialogaction.cpp_identifier(name))
        with self.assertRaises(skeleton.SkeletonError):
            skeleton.cpp_ident('a$b')

    def test_namespaces_and_dirs(self):
        self.assertEqual(skeleton.package_namespace(P + 'questEngine.handlers.template', 'core'),
                         ('aion', 'gameserver', 'questEngine', 'handlers', 'template_'))
        self.assertEqual(skeleton.package_dir(P + 'questEngine.handlers.template', 'core'), 'aion/gameserver/questEngine/handlers/template')
        self.assertEqual(skeleton.package_namespace('ai.instance', 'handlers'), ('aion', 'gameserver', 'handlers', 'ai', 'instance'))
        with self.assertRaises(skeleton.SkeletonError):
            skeleton.package_namespace('org.other', 'core')

    def _constant(self, decl):
        import javasrc
        cu = javasrc.parse_source(f'class C {{ {decl} }}')
        return skeleton.constant_initializer(cu.types[0].fields[0])

    def test_constants(self):
        self.assertEqual(self._constant('static final int A = 1_000;'), '1000')
        self.assertEqual(self._constant('static final long A = 60 * 1000L;'), '60 * 1000LL')
        self.assertEqual(self._constant('static final int A = 0xFFFFFFFF;'), 'static_cast<int32_t>(0xFFFFFFFFU)')
        self.assertEqual(self._constant('static final int A = 0x7F;'), '0x7F')
        self.assertEqual(self._constant('static final int A = -(1 << 4);'), '-(1 << 4)')
        self.assertEqual(self._constant('static final int A = 8 >> 1;'), '8 >> 1')
        self.assertEqual(self._constant('static final float A = 1f;'), '1.0f')
        self.assertEqual(self._constant('static final float A = 2;'), '2.0f')
        self.assertEqual(self._constant('static final double A = 1e3;'), '1e3')
        self.assertEqual(self._constant('static final double A = 2d;'), '2.0')
        self.assertEqual(self._constant("static final char A = 'x';"), "u'x'")
        self.assertEqual(self._constant("static final char A = '\\n';"), '0x000A')
        self.assertEqual(self._constant('static final String A = "a\\"b\\\\c\\t";'), '"a\\"b\\\\c\\t"')
        self.assertEqual(self._constant('static final boolean A = false;'), 'false')
        for bad in ('static final int A = B + 1;', 'static final int A = 1 >>> 2;', 'static final String A = "a" + "b";',
                    'static final int[] A = {1};', 'static final int A = (int) 1L;'):
            with self.assertRaises(skeleton.Todo, msg=bad):
                self._constant(bad)


class CppTreeTest(unittest.TestCase):

    def test_scan(self):
        root = ss.short_temp_dir('skscan')
        try:
            ss.write_tree(root, {'aion/gameserver/x/A.h': '''#pragma once
namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::gameserver::x {

/** comment with class Fake { */
struct A final : public B {
	class Nested {};
	void f() { if (true) { } }
};

template <class T>
class Tmpl {
};

enum class E : uint8_t { X };
enum class F { Y };
struct Tag {};
class Fwd;

namespace detail {
class Hidden {};
} // namespace detail

inline void g() {
}

class AfterFunction {
};

} // namespace aion::gameserver::x
''', 'aion/gameserver/runtime/lifetime/R.h': '''#pragma once
#define AION_SOMETHING 1
namespace aion::gameserver::runtime {
using Alias = int;
class Thing {};
}
'''})
            tree = skeleton.CppTree(root)
            ns = ('aion', 'gameserver', 'x')
            self.assertEqual(tree.lookup(ns, 'A').key, 'struct')
            self.assertEqual(tree.lookup(ns, 'Tmpl').template, 'template <class T>')
            self.assertEqual(tree.lookup(ns, 'E').underlying, 'uint8_t')
            self.assertEqual(tree.lookup(ns, 'F').underlying, '')
            self.assertEqual(tree.lookup(ns, 'Tag').key, 'struct')
            self.assertIsNotNone(tree.lookup(ns, 'AfterFunction'))
            self.assertIsNone(tree.lookup(ns, 'Nested'))
            self.assertIsNone(tree.lookup(ns, 'Fake'))
            self.assertIsNone(tree.lookup(ns, 'Fwd'))
            self.assertIsNone(tree.lookup(('aion', 'commons', 'configuration'), 'ConfigurableProcessor'))
            self.assertIsNotNone(tree.lookup(ns + ('detail',), 'Hidden'))
            self.assertEqual(tree.symbols['Thing'], 'aion/gameserver/runtime/lifetime/R.h')
            self.assertEqual(tree.symbols['Alias'], 'aion/gameserver/runtime/lifetime/R.h')
            self.assertEqual(tree.symbols['AION_SOMETHING'], 'aion/gameserver/runtime/lifetime/R.h')
        finally:
            shutil.rmtree(root, ignore_errors=True)


class FieldmapTest(unittest.TestCase):

    def test_fixture(self):
        fm = skeleton.Fieldmap.load(ss.FIXTURE_FIELDMAP)
        creature = fm.lookup(P + 'model.gameobjects.Creature')
        self.assertEqual(creature.kind, 'K4')
        self.assertEqual(list(creature.members), ['level', 'name', 'spawnObserver'])
        self.assertEqual(fm.lookup(P + 'model.gameobjects.AionObject').base, 'RefCounted')
        self.assertEqual(fm.lookup(P + 'model.templates.item.ItemTemplate').kind, 'K1')

    def test_list_form_and_binary_names(self):
        fm = skeleton.Fieldmap.from_json({'classes': [{'fqn': 'a.B$C', 'members': [{'name': 'x', 'type': 'int32_t'}]}]})
        self.assertEqual(fm.lookup('a.B.C').members['x'].cpp_type, 'int32_t')

    def test_errors(self):
        for data in ({}, {'classes': 1}, {'classes': {'a.B': {'kind': 'K9'}}}, {'classes': {'a.B': {'base': 'Shared'}}},
                     {'classes': {'a.B': {'members': [{'javaName': 'x'}]}}}, {'classes': {'a.B': {'members': [{'cppType': 'int'}]}}},
                     {'classes': {'a.B': {'members': [{'name': 'x', 'type': 'int', 'access': 'package'}]}}},
                     {'classes': {'a.B': {'members': [{'name': 'x', 'type': 'int'}, {'name': 'x', 'type': 'int'}]}}},
                     {'classes': {'a.B': {'extraDeclarations': 'struct X {};'}}}):
            with self.assertRaises(skeleton.SkeletonError, msg=json.dumps(data)):
                skeleton.Fieldmap.from_json(data)


GENERATOR_JAVA = {
    'model/gameobjects/Creature.java': 'public class Creature {\n\tpublic String getName() {\n\t\treturn null;\n\t}\n}\n',
    'model/gameobjects/player/Player.java': 'import com.aionemu.gameserver.model.gameobjects.Creature;\npublic class Player extends Creature {\n}\n',
    'ai/AbstractAI.java': """import com.aionemu.gameserver.model.gameobjects.Creature;
public abstract class AbstractAI<T extends Creature> {
	protected T owner;
	protected AbstractAI(T owner) {
		this.owner = owner;
	}
	public T getOwner() {
		return owner;
	}
}
""",
    'ai/AITemplate.java': """import com.aionemu.gameserver.model.gameobjects.Creature;
public abstract class AITemplate<T extends Creature> extends AbstractAI<T> {
	protected AITemplate(T owner) {
		super(owner);
	}
	public abstract void think(T target);
}
""",
    'model/templates/walker/RouteStep.java': 'public class RouteStep {\n\tprivate float x;\n}\n',
    'model/templates/walker/WalkerTemplate.java': 'public class WalkerTemplate {\n\tprivate String id;\n}\n',
    'model/templates/walker/WalkerSupport.java': 'public class WalkerSupport {\n\tpublic void walk(RouteStep step) {\n\t}\n}\n',
    'network/aion/AionServerPacket.java': 'public abstract class AionServerPacket {\n}\n',
    'utils/Future.java': 'public interface Future {\n}\n',
    # hand-written C++ definitions (xmlgen behaviour shells) that lack the Java interfaces, methods and runtime bases
    'model/items/L10n.java': 'public interface L10n {\n\tint getL10nId();\n}\n',
    'model/items/ItemBase.java': 'public abstract class ItemBase implements L10n {\n\tpublic int getL10nId() {\n\t\treturn 0;\n\t}\n'
                                 '\tpublic String getLabel() {\n\t\treturn null;\n\t}\n}\n',
    'model/items/ItemData.java': 'public class ItemData extends ItemBase {\n\t@Override\n\tpublic int getL10nId() {\n\t\treturn 1;\n\t}\n'
                                 '\t@Override\n\tpublic String getLabel() {\n\t\treturn "x";\n\t}\n}\n',
    'model/items/Gear.java': 'public class Gear {\n}\n',
    'model/items/CountedGear.java': 'public class CountedGear {\n}\n',
    'utils/TimeUnit.java': 'public enum TimeUnit {\n\tSECONDS\n}\n',
    'network/aion/ServerPacketsOpcodes.java': 'public class ServerPacketsOpcodes {\n\tpublic static int getOpcode() {\n\t\treturn 1;\n\t}\n}\n',
    # enums xmlgen generates (xmlmodel.json): top-level, secondary top-level and nested (generated as Outer_Inner at namespace scope)
    'ai/AIState.java': 'public enum AIState {\n\tIDLE, DIED\n}\n',
    'services/RecallService.java': """public class RecallService {
	public enum CancelReason {
		MOVED, DIED
	}
	private static class Pending {
	}
	public void cancel(CancelReason reason) {
	}
}
""",
    'services/PunishmentService.java': """public class PunishmentService {
	public enum PunishmentType {
		PRISON, GATHER
	}
	public PunishmentType type() {
		return null;
	}
}
""",
    'services/abyss/AbyssSkillService.java': """import com.aionemu.gameserver.services.RecallService;
public class AbyssSkillService {
	public static AbyssSkills get(int id) {
		return null;
	}
	public void recall(RecallService.CancelReason reason) {
	}
}
enum AbyssSkills {
	FIRST, SECOND
}
""",
    'network/aion/serverpackets/SM_SYSTEM_MESSAGE.java': """import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.network.aion.AionServerPacket;
public final class SM_SYSTEM_MESSAGE extends AionServerPacket {
	private final int msgId;
	public SM_SYSTEM_MESSAGE(int msgId, Object... params) {
		this.msgId = msgId;
	}
	public static SM_SYSTEM_MESSAGE STR_PLAIN(String name) {
		return new SM_SYSTEM_MESSAGE(1300001, name);
	}
	public static SM_SYSTEM_MESSAGE _STR_RESERVED() {
		return new SM_SYSTEM_MESSAGE(1300002);
	}
	public static SM_SYSTEM_MESSAGE STR_HAND(Player player) {
		return new SM_SYSTEM_MESSAGE(1300003, player.getName());
	}
	public int getMsgId() {
		return msgId;
	}
}
""",
}
GENERATOR_CPP = {
    'aion/gameserver/handlers/HandlerRegistry.h': """#pragma once

namespace aion::gameserver::ai {
class AbstractAI;
}
""",
    # skeleton's own output is never read back (it would keep an outdated class key)
    'aion/gameserver/model/templates/walker/fwd.h': skeleton.FWD_MARK + """ from the Java package x - do not edit.
#pragma once

namespace aion::gameserver::model::templates::walker {

class RouteStep;

} // namespace aion::gameserver::model::templates::walker
""",
    'aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen.h': """// Generated by cpp/tools/gen/sysmsg.py - do not edit.
//
// Member block of class SM_SYSTEM_MESSAGE: #include it in a public section of the class body.
static SM_SYSTEM_MESSAGE STR_PLAIN(std::string_view name);
static SM_SYSTEM_MESSAGE STR_RESERVED_();
""",
    # Java classes re-exported by an alias or a using-declaration (runtime types in utils) cannot be forward declared again
    'aion/gameserver/utils/ThreadPoolManager.h': """#pragma once

namespace aion::gameserver::utils {

using runtime::Future;
using TimeUnit = runtime::TimeUnit;

} // namespace aion::gameserver::utils
""",
    'aion/gameserver/model/items/ItemBase.h': """#pragma once

#include <string>

namespace aion::gameserver::model::items {

class ItemBase {
public:
	std::string getLabel() const;
};

} // namespace aion::gameserver::model::items
""",
    'aion/gameserver/model/items/Gear.h': """#pragma once

namespace aion::gameserver::model::items {

class Gear {
};

} // namespace aion::gameserver::model::items
""",
    'aion/gameserver/model/items/CountedGear.h': """#pragma once

namespace aion::gameserver::model::items {

class CountedGear final : public runtime::RefCounted {
};

} // namespace aion::gameserver::model::items
""",
    'aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h': """// Generated by cpp/tools/gen/opcodes.py - do not edit.
#pragma once

namespace aion::gameserver::network::aion {

namespace ServerPacketsOpcodes {
int32_t getOpcode();
} // namespace ServerPacketsOpcodes

} // namespace aion::gameserver::network::aion
""",
}
GENERATOR_GENERATED = {
    'aion/gameserver/model/templates/walker/RouteStep.h': """// GENERATED by tools/xmlgen - do not edit
#pragma once

namespace aion::gameserver::model::templates::walker {

struct RouteStep {
	float x = 0.0f;
};

} // namespace aion::gameserver::model::templates::walker
""",
    'staticdata-classes.json': json.dumps({'format': 'aion-staticdata-classes', 'version': 1, 'classes': [
        {'fqn': P + 'model.templates.walker.RouteStep', 'kind': 'data'},
        {'fqn': P + 'model.templates.walker.WalkerTemplate', 'kind': 'behaviour'}]}),
    'xmlmodel.json': json.dumps({'format': 'aion-xmlmodel', 'version': 1, 'classes': [], 'enums': [
        {'fqn': P + fqn, 'external': False, 'core': True, 'cpp': {'qualifiedName': '::aion::gameserver::' + cpp, 'header': header,
                                                                  'underlying': 'uint8_t'}}
        for fqn, cpp, header in (
            ('ai.AIState', 'ai::AIState', 'aion/gameserver/ai/AIState.h'),
            ('services.RecallService.CancelReason', 'services::RecallService_CancelReason', 'aion/gameserver/services/RecallService_CancelReason.h'),
            ('services.PunishmentService.PunishmentType', 'services::PunishmentService_PunishmentType',
             'aion/gameserver/services/PunishmentService_PunishmentType.h'),
            ('services.abyss.AbyssSkills', 'services::abyss::AbyssSkills', 'aion/gameserver/services/abyss/AbyssSkills.h'))] + [
        {'fqn': 'java.time.DayOfWeek', 'external': True, 'core': True, 'cpp': {'qualifiedName': '::std::chrono::weekday', 'header': '<chrono>'}}]}),
    'aion/gameserver/ai/AIState.h': '#pragma once\n#include <cstdint>\nnamespace aion::gameserver::ai {\nenum class AIState : uint8_t { IDLE, DIED };\n}\n',
    'aion/gameserver/services/RecallService_CancelReason.h':
        '#pragma once\n#include <cstdint>\nnamespace aion::gameserver::services {\nenum class RecallService_CancelReason : uint8_t { MOVED, DIED };\n}\n',
    'aion/gameserver/services/PunishmentService_PunishmentType.h':
        '#pragma once\n#include <cstdint>\nnamespace aion::gameserver::services {\nenum class PunishmentService_PunishmentType : uint8_t { PRISON, GATHER };\n}\n',
    'aion/gameserver/services/abyss/AbyssSkills.h':
        '#pragma once\n#include <cstdint>\nnamespace aion::gameserver::services::abyss {\nenum class AbyssSkills : uint8_t { FIRST, SECOND };\n}\n',
}


class GeneratorAwareTest(unittest.TestCase):
    """Other generators' outputs (xmlgen tree, sysmsg member block, opcodes namespace) and forward declarations of the existing port."""

    @classmethod
    def setUpClass(cls):
        cls.root = ss.short_temp_dir('skgen')
        ss.write_tree(cls.root / 'java', {'com/aionemu/gameserver/' + rel: f'package {P}{rel.rsplit("/", 1)[0].replace("/", ".")};\n' + text
                                          for rel, text in GENERATOR_JAVA.items()})
        ss.write_tree(cls.root / 'cpp', GENERATOR_CPP)
        ss.write_tree(cls.root / 'generated', GENERATOR_GENERATED)
        fieldmap = skeleton.Fieldmap.from_json({'classes': {P + 'model.items.Gear': {'base': 'RefCounted'},
                                                           P + 'model.items.CountedGear': {'base': 'Immortal'}}})
        cls.project = skeleton.Project(cls.root / 'java', None, cls.root / 'cpp', None, fieldmap, cls.root / 'generated')
        cls.fwd = skeleton.generate_fwd(cls.project)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, ignore_errors=True)

    def test_forward_headers_use_generated_and_forward_declared_types(self):
        walker = self.fwd['aion/gameserver/model/templates/walker/fwd.h']
        self.assertIn('struct RouteStep;', walker)                          # xmlgen struct (the stale skeleton fwd.h is ignored)
        self.assertIn('class WalkerTemplate;', walker)
        ai = self.fwd['aion/gameserver/ai/fwd.h']
        self.assertIn('\nclass AbstractAI;', ai)                            # HandlerRegistry.h: non-template class
        self.assertIn('template <class T> class AITemplate;', ai)
        self.assertIn('// ServerPacketsOpcodes: a C++ namespace', self.fwd['aion/gameserver/network/aion/fwd.h'])
        utils = self.fwd['aion/gameserver/utils/fwd.h']
        self.assertIn('// Future: a C++ alias or using-declaration in aion/gameserver/utils/ThreadPoolManager.h', utils)
        self.assertIn('// TimeUnit: a C++ alias or using-declaration in aion/gameserver/utils/ThreadPoolManager.h', utils)
        self.assertNotIn('class Future;', utils)
        self.assertNotIn('#include <cstdint>', utils)

    def test_group_selectors_skip_generator_owned_files(self):
        selected = {td.fqn for td in self.project.select(['@all'])}
        for owned in ('model.templates.walker.RouteStep', 'model.templates.walker.WalkerTemplate', 'network.aion.ServerPacketsOpcodes',
                      'network.aion.serverpackets.SM_SYSTEM_MESSAGE'):
            self.assertNotIn(P + owned, selected)
            self.assertIn(P + owned, self.project.skipped_existing)
        self.assertIn(P + 'model.templates.walker.WalkerSupport', selected)
        self.assertEqual([td.fqn for td in self.project.select([P + 'model.templates.**'])], [P + 'model.templates.walker.WalkerSupport'])
        for sel, message in (('RouteStep', 'static data class'), (P + 'model.templates.walker.WalkerTemplate', 'xmlgen.py scaffold'),
                             ('ServerPacketsOpcodes', 'replaced by the generated')):
            with self.assertRaisesRegex(skeleton.SkeletonError, message):
                self.project.select([sel])

    def test_member_block_draft(self):
        files = skeleton.generate_drafts(self.project, self.project.select(['SM_SYSTEM_MESSAGE']))
        header = files['aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h']
        self.assertIn('#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen.h"', header)
        self.assertNotIn('STR_PLAIN', header.replace('SM_SYSTEM_MESSAGE.gen.h', ''))
        self.assertNotIn('STR_RESERVED', header)
        self.assertIn('STR_HAND', header)                                    # hand-ported factory (Player parameter) stays
        self.assertIn('getMsgId()', header)
        for std in ('<cstdint>', '<string>', '<string_view>', '<vector>'):
            self.assertIn(f'#include {std}', header)
        self.assertNotIn('STR_PLAIN', files['aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.cpp'])

    def test_non_template_generic_draft(self):
        files = skeleton.generate_drafts(self.project, self.project.select([P + 'ai.*']))
        abstract_ai = files['aion/gameserver/ai/AbstractAI.h']
        self.assertIn('class AbstractAI {', abstract_ai)
        self.assertNotIn('template <class T>', abstract_ai)
        self.assertIn('runtime::Ptr<model::gameobjects::Creature> getOwner()', abstract_ai)
        template = files['aion/gameserver/ai/AITemplate.h']
        self.assertIn('template <class T>\nclass AITemplate : public AbstractAI {', template)
        self.assertIn('virtual void think(T& target) = 0;', template)          # TEMPLATE_GENERICS stays a template; non-null T&

    def test_hand_written_definitions_win(self):
        """Runtime bases and overrides follow an existing hand-written C++ definition, not the Java class or fieldmap.json."""
        t = self.project.index.types
        self.assertIsNone(self.project.base_kind(t[P + 'model.items.Gear']))                    # shell without the fieldmap base
        self.assertEqual(self.project.base_kind(t[P + 'model.items.CountedGear']), 'RefCounted')  # the C++ base clause wins
        self.assertEqual(self.project.cpp.lookup(('aion', 'gameserver', 'model', 'items'), 'CountedGear').bases, 'public runtime::RefCounted')
        data = t[P + 'model.items.ItemData']
        methods = {m.name: m for m in data.methods}
        self.assertTrue(self.project.overrides(data, methods['getLabel']))                     # declared by the shell
        self.assertFalse(self.project.overrides(data, methods['getL10nId']))                   # L10n is not a base of the shell
        header = skeleton.generate_drafts(self.project, self.project.select([P + 'model.items.ItemData']))['aion/gameserver/model/items/ItemData.h']
        self.assertIn('std::string getLabel() override;', header)
        self.assertIn('int32_t getL10nId(); // @Override: the hand-written C++ base does not declare it (yet)', header)

    def test_generated_enums_are_generator_owned(self):
        """Every enum of xmlmodel.json (not only the staticdata-classes.json JAXB enums) is xmlgen's: group selectors skip it, explicit
        selectors refuse it, even as a secondary top-level type of another Java file."""
        self.assertEqual(self.project.generator_owner(self.project.index.types[P + 'ai.AIState']), ('xmlgen', 'enum'))
        self.assertEqual(self.project.generator_owner(self.project.index.types[P + 'services.abyss.AbyssSkills']), ('xmlgen', 'enum'))
        self.assertIsNone(self.project.generator_owner(self.project.index.types[P + 'services.RecallService']), 'a nested enum only')
        selected = {td.fqn for td in self.project.select(['@all'])}
        self.assertNotIn(P + 'ai.AIState', selected)
        self.assertNotIn(P + 'services.abyss.AbyssSkills', selected)
        self.assertIn(P + 'services.abyss.AbyssSkillService', selected)
        for sel in (P + 'ai.AIState', 'AIState', 'AbyssSkills', P + 'services.abyss.AbyssSkills'):
            with self.assertRaisesRegex(skeleton.SkeletonError, 'is an enum xmlgen generates'):
                self.project.select([sel])
        self.assertEqual([td.fqn for td in self.project.select([P + 'services.RecallService.CancelReason'])], [P + 'services.RecallService'])

    def test_nested_generated_enums_are_aliased(self):
        files = skeleton.generate_drafts(self.project, self.project.select([P + 'services.*']))
        recall = files['aion/gameserver/services/RecallService.h']
        self.assertIn('#include "aion/gameserver/services/RecallService_CancelReason.h"', recall)
        self.assertIn('public:\n\tusing CancelReason = ::aion::gameserver::services::RecallService_CancelReason; // generated by xmlgen\n'
                      'private:\n\tclass Pending;\n', recall)                      # the alias replaces the forward declaration
        self.assertNotRegex(recall, r'enum class CancelReason\b')
        self.assertEqual(recall.count('using CancelReason'), 1)
        self.assertIn('void cancel(RecallService::CancelReason reason);', recall)
        punishment = files['aion/gameserver/services/PunishmentService.h']   # a single nested type: no forward declaration pass
        self.assertIn('\tusing PunishmentType = ::aion::gameserver::services::PunishmentService_PunishmentType; // generated by xmlgen\n',
                      punishment)
        self.assertNotRegex(punishment, r'enum class PunishmentType\b')

    def test_secondary_generated_enum_is_included_not_defined(self):
        files = skeleton.generate_drafts(self.project, self.project.select(['AbyssSkillService']), with_dependencies=True)
        self.assertEqual(sorted(files), ['aion/gameserver/services/abyss/AbyssSkillService.cpp',
                                         'aion/gameserver/services/abyss/AbyssSkillService.h'],
                         'the outer class of a nested generated enum is not a dependency')
        header = files['aion/gameserver/services/abyss/AbyssSkillService.h']
        self.assertNotRegex(header, r'(?m)^\s*enum class AbyssSkills\b')
        self.assertIn('// AbyssSkills: an enum generated by xmlgen (aion/gameserver/services/abyss/AbyssSkills.h)', header)
        self.assertIn('#include "aion/gameserver/services/abyss/AbyssSkills.h"', header)
        self.assertIn('static AbyssSkills get(int32_t id);', header)
        # a nested generated enum of another file: its namespace-scope name and generated header, not the outer class header
        self.assertIn('void recall(RecallService_CancelReason reason);', header)
        self.assertIn('#include "aion/gameserver/services/RecallService_CancelReason.h"', header)
        self.assertNotIn('RecallService.h"', header)

    def test_dependencies_stop_at_generated_headers(self):
        files = skeleton.generate_drafts(self.project, self.project.select(['WalkerSupport']), with_dependencies=True)
        self.assertEqual(sorted(files), ['aion/gameserver/model/templates/walker/WalkerSupport.cpp',
                                         'aion/gameserver/model/templates/walker/WalkerSupport.h'])


class ProjectTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.project = fixture_project()

    def type(self, fqn):
        return self.project.index.types[P + fqn]

    def test_selection(self):
        sel = lambda *s: [t.fqn for t in self.project.select(list(s))]
        self.assertEqual(sel('@daos'), [P + 'dao.BarDAO'])
        self.assertEqual(sel('@services'), [P + 'services.FooService'])
        self.assertEqual(sel(P + 'network.aion.serverpackets.*'),
                         [P + 'network.aion.serverpackets.SM_FOO', P + 'network.aion.serverpackets.SM_FOO_LIST',
                          P + 'network.aion.serverpackets.SM_ITEMS'])
        self.assertEqual(sel(P + 'model.gameobjects.Creature.State'), [P + 'model.gameobjects.Creature'])
        self.assertEqual(sel('Player'), [P + 'model.gameobjects.player.Player'])
        self.assertNotIn(P + 'configs.main.GSConfig', sel('@all'))                 # already has a C++ header
        self.assertEqual(sel(P + 'configs.main.GSConfig'), [P + 'configs.main.GSConfig'])   # explicit selection still drafts
        for bad in ('NoSuchClass', P + 'nope.X', '@unknown', P + 'nope.*'):
            with self.assertRaises(skeleton.SkeletonError, msg=bad):
                self.project.select([bad])

    def test_kinds(self):
        self.assertEqual(self.project.kind(self.type('model.templates.item.ItemTemplate')), 'K1')
        self.assertEqual(self.project.kind(self.type('network.aion.serverpackets.SM_FOO')), 'K2')
        self.assertEqual(self.project.kind(self.type('model.gameobjects.Creature')), 'K4')
        self.assertIsNone(self.project.kind(self.type('dao.BarDAO')))
        self.assertEqual(self.project.base_kind(self.type('model.gameobjects.player.Player')), 'RefCounted')

    def test_override_analysis(self):
        creature = self.type('model.gameobjects.Creature')
        subs = self.project.subtype_methods(creature)
        self.assertIn(('onSpawn', 0), subs)            # overridden by a handler class
        self.assertIn(('onDie', 1), subs)              # overridden by Player
        observer = self.project.subtype_methods(self.type('controllers.observer.ActionObserver'))
        self.assertIn(('moved', 0), observer)          # overridden by an anonymous class
        self.assertNotIn(('attacked', 1), observer)
        player = self.type('model.gameobjects.player.Player')
        on_die = next(m for m in player.methods if m.name == 'onDie')
        self.assertIs(self.project.overridden_method(player, on_die).owner, creature)
        register = next(m for m in player.methods if m.name == 'register' and not m.params)
        self.assertIsNone(self.project.overridden_method(player, register))

    def test_qualification(self):
        emitter = skeleton.DraftEmitter(self.project, self.type('dao.BarDAO').cu)
        q = emitter.ctx.qualify
        self.assertEqual(q(('aion', 'gameserver', 'dao', 'BarDAO')), 'BarDAO')
        self.assertEqual(q(('aion', 'gameserver', 'model', 'gameobjects', 'player', 'Player')), 'model::gameobjects::player::Player')
        self.assertEqual(q(('aion', 'gameserver', 'runtime', 'Ptr')), 'runtime::Ptr')
        self.assertEqual(q(('aion', 'commons', 'utils', 'ByteBuffer')), 'commons::utils::ByteBuffer')
        packet = skeleton.DraftEmitter(self.project, self.type('network.aion.serverpackets.SM_FOO').cu)
        # 'aion' names aion::gameserver::network::aion inside that namespace
        self.assertEqual(packet.ctx.qualify(('aion', 'gameserver', 'model', 'Race')), 'model::Race')
        self.assertEqual(packet.ctx.qualify(('aion', 'commons', 'utils', 'ByteBuffer')), 'commons::utils::ByteBuffer')
        self.assertEqual(packet.ctx.qualify(('aion', 'gameserver', 'network', 'aion', 'AionConnection')), 'AionConnection')
        # a member name hides a namespace of the same name
        emitter.ctx.scope_names.add('model')
        self.assertEqual(q(('aion', 'gameserver', 'model', 'Race')), 'gameserver::model::Race')


class DraftRulesTest(unittest.TestCase):
    """Spot checks of individual rules on the generated fixture drafts (the golden files cover everything)."""

    @classmethod
    def setUpClass(cls):
        cls.project = fixture_project()
        cls.files = skeleton.generate_drafts(cls.project, cls.project.select(['@all']))

    def h(self, rel):
        return self.files[f'aion/gameserver/{rel}.h']

    def cpp(self, rel):
        return self.files[f'aion/gameserver/{rel}.cpp']

    def test_refcounted_classes(self):
        creature = self.h('model/gameobjects/Creature')
        self.assertIn('AION_MAKE_REF_FRIEND', creature)
        self.assertIn('static runtime::Ref<Creature> create(', creature)
        self.assertIn('protected:\n\t~Creature() override;', creature)
        self.assertIn('class AionObject : public runtime::RefCounted {', self.h('model/gameobjects/AionObject'))
        self.assertNotIn('create(', self.h('model/gameobjects/AionObject'))            # abstract
        self.assertIn('Creature::Creature(int32_t value, const std::vector<runtime::Ptr<controllers::observer::ActionObserver>>& '
                      'observersValue) : AionObject(int32_t{}) {', self.cpp('model/gameobjects/Creature'))
        self.assertIn('Creature(int32_t objectId, const std::vector<runtime::Ptr<controllers::observer::ActionObserver>>& observers);',
                      creature)                                                   # declarations keep the Java names; borrowed elements

    def test_members_and_accessors(self):
        creature = self.h('model/gameobjects/Creature')
        self.assertIn('\truntime::Field<int32_t> level{};', creature)
        self.assertIn('int32_t getLevel() const { return this->level.get(); }', creature)
        self.assertIn('void setName(std::string_view value) { this->name.set(std::string(value)); }', creature)   # no C4458
        self.assertIn('// TODO(fieldmap): Java fields of Creature without a usable fieldmap.json member:', creature)
        self.assertIn('//   Creature.java:20  private Race race;  [no fieldmap.json member]', creature)
        self.assertIn('static constexpr int32_t MAX_LEVEL = 65;', creature)
        self.assertIn('struct Creature_Observer1 {', creature)
        self.assertIn('protected:\n\truntime::Field<int64_t> spawnObserver{}; // capture member', creature)
        player = self.h('model/gameobjects/player/Player')
        # fieldmap.py's own format: unqualified C++ spellings, rules, modifiers
        self.assertIn('runtime::Field<Persistable::PersistentState> state{}; // Java: = PersistentState.NEW', player)
        self.assertIn('Persistable::PersistentState getPersistentState() override { return this->state.get(); }', player)
        self.assertIn('void setClientConnection(network::aion::AionConnection* connection);', player)   # raw pointer -> shared_ptr: stub
        self.assertIn('#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"', player)
        self.assertIn('runtime::Field<std::shared_ptr<network::aion::AionConnection>> connection{};', player)
        self.assertIn('static inline const runtime::Ref<Player> NOBODY{}; // Java: = null', player)
        self.assertIn('runtime::PartSlot<Creature::Stats> stats{*this};', player)
        self.assertIn('runtime::Field<bool> release_{};', player)                          # would hide RefCounted::release()
        self.assertIn('bool isRelease() const { return this->release_.get(); }', player)
        self.assertIn('void setCount(int32_t value) { this->count.set(value); }', player)   # 'count' would hide RefCounted::count
        self.assertIn('[fieldmap: logger (CONVENTIONS logging: a static logger in the .cpp)]', player)
        self.assertIn('template <class U>\n\t[[noreturn]] static U& unportedArgument() { AION_UNPORTED(); }', player)
        outer = self.h('model/gameobjects/Outer')
        self.assertIn('names Outer::Inner (Ref<> of a class without a RefCounted/OwnedPart base in fieldmap.json)]', outer)
        item = self.h('model/templates/item/ItemTemplate')
        self.assertIn('class ItemTemplate : public runtime::StaticTemplate {', item)
        self.assertIn('private int templateId;  [fieldmap: K1: member block generated by xmlgen]', item)
        self.assertIn('// TODO(callbacks): generated declarations from fieldmap.json, enable them with the bodies that use them:\n'
                      '\t// struct Creature_Observer1 {\n\t// \tconst Ref<Creature> owner;\n\t// };', creature)

    def test_signatures(self):
        player = self.h('model/gameobjects/player/Player')
        self.assertIn('void register_();', player)
        self.assertIn('void delete_(std::initializer_list<int32_t> ids = {});', player)                    # varargs (§7.4)
        self.assertIn('// TODO(signature): C++ signature collides with the declaration at line 68: public void send(Collection<ItemTemplate> '
                      'items)', player)
        self.assertIn('void register_(const std::any& listener);', player)                                 # Object parameter
        self.assertIn('// TODO(signature): generic method:', player)
        self.assertIn('void send(const std::vector<const templates::item::ItemTemplate*>& items);', player)
        self.assertIn('std::unordered_set<int32_t> friendIds(std::span<const uint8_t> data, runtime::FutureRef task);', player)
        self.assertIn('runtime::Ptr<Player> findFriend(std::string_view name);', player)
        self.assertIn('bool removeFriends(const std::function<bool(Player&)>& filter);', player)             # callback arguments
        self.assertIn('void onDie(Creature& lastAttacker) override;', player)                               # never null: X&
        self.assertIn('runtime::ConcurrentHashMap<int32_t, runtime::Ref<Player>>& getFriends() { return this->friends; }', player)
        creature = self.h('model/gameobjects/Creature')
        self.assertIn('virtual void onSpawn();', creature)
        self.assertIn('Creature::State getState();', creature)
        observer = self.h('controllers/observer/ActionObserver')
        self.assertIn('virtual void moved();', observer)
        self.assertIn('\tvoid attacked(model::gameobjects::Creature& creature);', observer)
        self.assertIn('virtual ~ActionObserver();', observer)
        aion_object = self.h('model/gameobjects/AionObject')
        self.assertIn('bool equals(const AionObject& obj) const;', aion_object)
        self.assertIn('virtual std::string getName() = 0;', aion_object)
        dao = self.h('dao/BarDAO')
        self.assertIn('static void storePlayer(model::gameobjects::player::Player& player, '
                      'std::optional<commons::database::Timestamp> lastOnline);', dao)
        self.assertIn('static std::unordered_set<model::Race> races();', dao)
        self.assertIn('void afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent); // JAXB hook (XmlBinding.h)',
                      self.h('model/templates/item/ItemTemplate'))
        packet = self.h('network/aion/AionServerPacket')
        self.assertIn('virtual void writeImpl(AionConnection* con) = 0;', packet)
        self.assertIn('void writeBuffer(commons::utils::ByteBuffer& buf);', packet)

    def test_singletons_loggers_constants(self):
        header, source = self.h('services/FooService'), self.cpp('services/FooService')
        self.assertIn('static FooService& getInstance(); // Java singleton', header)
        self.assertIn('\tstatic FooService instance; // Java SingletonHolder\n\treturn instance;', source)
        self.assertNotIn('SingletonHolder {', header)
        self.assertIn('static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.FooService");',
                      source)
        self.assertIn('network::aion::serverpackets::SM_FOO packet(int32_t value);', header)
        self.assertIn('#include "aion/gameserver/network/aion/serverpackets/SM_FOO.h"', source)     # by-value return type
        self.assertIn('static constexpr int64_t LONG_VALUE = 5 * 60 * 1000LL;', header)
        self.assertIn('static inline const std::string NAME = "foo \\"bar\\"\\n\u00e9";', header)
        self.assertIn('//   FooService.java:30  private static final String NOT_LITERAL = String.valueOf(INT_VALUE);', header)
        self.assertIn('// TODO(signature): no C++ mapping for java.lang.Runnable: public void schedule(Runnable r)', header)

    def test_nesting_templates_records_interfaces(self):
        outer = self.h('model/gameobjects/Outer')
        self.assertIn('class Outer::Inner : public Outer {', outer)
        self.assertLess(outer.index('class Base {'), outer.index('class Derived : public Outer::Base {'))
        self.assertIn('Outer::Derived::Derived() : Outer::Base(int32_t{}) {', self.cpp('model/gameobjects/Outer'))
        self.assertIn('public:\n\t\texplicit Base(int32_t value);', outer)     # private constructor of a nested class
        controller = self.h('controllers/CreatureController')      # erased generic: T is spelled as its bound Creature
        self.assertIn('// Java generic CreatureController<T>: a non-template C++ class (erasure rule', controller)
        self.assertIn('\nclass CreatureController : public runtime::OwnedPart {', controller)
        self.assertIn('\truntime::OwnerRef<model::gameobjects::Creature> owner;', controller)
        self.assertIn('\texplicit CreatureController(model::gameobjects::Creature& owner);', controller)   # an owner is never null
        self.assertIn('\tmodel::gameobjects::Creature& getOwner() const { return this->owner; }', controller)
        self.assertIn('#pragma warning(disable : 4702) // the base initializer never returns\n#endif\n'
                      'CreatureController::CreatureController(model::gameobjects::Creature& value) : '
                      'owner(unportedArgument<model::gameobjects::Creature>()) {', self.cpp('controllers/CreatureController'))
        self.assertIn('class PlayerController : public CreatureController {', self.h('controllers/PlayerController'))
        point = self.h('model/geometry/Point')
        self.assertIn('Point(float x, float y); // canonical record constructor', point)
        self.assertIn('int32_t compareTo(const Point& o) const;', point)
        self.assertIn('float x() const; // record accessor', point)
        functions = self.h('model/stats/Functions')
        self.assertLess(functions.index('class BaseFunction {'), functions.index('class Functions : public BaseFunction {'))
        handler = self.h('questEngine/handlers/template/QuestTemplateHandler')
        self.assertIn('namespace aion::gameserver::questEngine::handlers::template_ {', handler)
        self.assertIn('virtual void register_() = 0;', handler)
        self.assertIn('virtual ~QuestTemplateHandler() = default;', handler)
        race = self.h('model/Race')
        self.assertIn('enum class Race : std::uint8_t {', race)
        self.assertIn('// TODO(enum): Race has methods isPlayerRace; nested types Group', race)
        items = self.cpp('network/aion/serverpackets/SM_ITEMS')
        self.assertIn(': SM_ITEMS(unportedArgument<std::vector<runtime::Ptr<model::gameobjects::player::Player>>>())', items)

    def test_todo_mode(self):
        project = fixture_project(fieldmap=False)
        creature = skeleton.generate_drafts(project, project.select([P + 'model.gameobjects.Creature']))[
            'aion/gameserver/model/gameobjects/Creature.h']
        self.assertIn('// TODO(fieldmap): no fieldmap.json entry for Creature; declare these Java fields with the member types printed by',
                      creature)
        self.assertIn('// python cpp/tools/gen/fieldmap.py --class com.aionemu.gameserver.model.gameobjects.Creature', creature)
        self.assertIn('int32_t getLevel() const; // trivial accessor of level: inline once the member exists', creature)
        self.assertIn('void setName(std::string_view name); // trivial accessor of name: inline once the member exists', creature)
        self.assertNotIn('AION_MAKE_REF_FRIEND', creature)

    def test_dependencies(self):
        files = skeleton.generate_drafts(self.project, self.project.select([P + 'network.aion.serverpackets.SM_FOO_LIST']),
                                         with_dependencies=True)
        self.assertEqual(sorted(f for f in files if f.endswith('.h')),
                         ['aion/gameserver/network/aion/AionServerPacket.h', 'aion/gameserver/network/aion/serverpackets/SM_FOO.h',
                          'aion/gameserver/network/aion/serverpackets/SM_FOO_LIST.h'])


class CommandLineTest(unittest.TestCase):

    def run_main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = skeleton.main([str(a) for a in args])
        return code, out.getvalue(), err.getvalue()

    def test_write_and_check(self):
        root = ss.short_temp_dir('skcli')
        try:
            common = ['--java-root', ss.FIXTURE_JAVA, '--cpp-src', ss.FIXTURE_CPP, '--commons-src', ss.FIXTURE_CPP]
            self.assertEqual(self.run_main('--fwd', '--out', root, *common)[0], 0)
            self.assertEqual(self.run_main('--fwd', '--out', root, '--check', *common)[0], 0)
            draft = ['--draft', '--out', root, '--handlers-root', ss.FIXTURE_HANDLERS, '--fieldmap', ss.FIXTURE_FIELDMAP, *common, '@daos']
            self.assertEqual(self.run_main(*draft)[0], 0)
            self.assertEqual(self.run_main(*draft, '--check')[0], 0)
            # a changed file, a stale generated fwd.h and a hand-written fwd.h
            (root / 'aion/gameserver/dao/BarDAO.h').write_text('// edited\n', encoding='utf-8')
            stale = root / 'aion/gameserver/removed/fwd.h'
            stale.parent.mkdir(parents=True)
            stale.write_text(skeleton.FWD_MARK + ' from the Java package x - do not edit.\n', encoding='utf-8')
            (root / 'aion/gameserver/hand').mkdir(parents=True)
            (root / 'aion/gameserver/hand/fwd.h').write_text('// hand-written\n', encoding='utf-8')
            code, out, _ = self.run_main(*draft, '--check')
            self.assertEqual((code, out), (1, 'different: aion/gameserver/dao/BarDAO.h\n'))
            code, out, _ = self.run_main('--fwd', '--out', root, '--check', *common)
            self.assertEqual((code, out), (1, 'stale: aion/gameserver/removed/fwd.h\n'))
            code, out, _ = self.run_main('--list', '--java-root', ss.FIXTURE_JAVA, '--cpp-src', ss.FIXTURE_CPP, '@daos', '@services')
            self.assertEqual((code, out), (0, P + 'dao.BarDAO\n' + P + 'services.FooService\n'))
            classes = root / 'classes.txt'
            classes.write_text('# hubs\nPlayer\n\n' + P + 'dao.*  # daos\n', encoding='utf-8')
            code, out, _ = self.run_main('--list', '--java-root', ss.FIXTURE_JAVA, '--cpp-src', ss.FIXTURE_CPP, '--classes', classes)
            self.assertEqual(out, P + 'dao.BarDAO\n' + P + 'model.gameobjects.player.Player\n')
            code, _, err = self.run_main('--draft', '--out', root, '--java-root', ss.FIXTURE_JAVA, '--no-fieldmap', 'NoSuchClass')
            self.assertEqual(code, 2)
            self.assertIn('selector NoSuchClass matches no Java type', err)
            self.assertEqual(self.run_main('--draft', '--java-root', ss.FIXTURE_JAVA, '--no-fieldmap', '@daos')[0], 2)   # --out missing
        finally:
            shutil.rmtree(root, ignore_errors=True)


@unittest.skipIf(os.environ.get('AION_SKELETON_SKIP_COMPILE') == '1', 'AION_SKELETON_SKIP_COMPILE=1')
@unittest.skipIf(ss.find_cmake() is None or os.name != 'nt', 'needs CMake and MSVC')
class FixtureCompileTest(unittest.TestCase):
    """The fixture forward headers and drafts compile with the project's MSVC flags and /WX against the real runtime headers."""

    def test_compile(self):
        project = fixture_project()
        fwd = skeleton.generate_fwd(project)
        drafts = skeleton.generate_drafts(project, project.select(['@all']))
        nofm = fixture_project(fieldmap=False)
        drafts_nofm = skeleton.generate_drafts(nofm, nofm.select(['@all']))
        work = ss.short_temp_dir('skfx')
        try:
            ss.write_tree(work / 'inc', {**fwd, **drafts})
            ss.write_tree(work / 'nofm', {**fwd, **drafts_nofm})
            sources = [work / 'inc' / f for f in sorted(drafts) if f.endswith('.cpp')]
            fwd_tu = work / 'fwd_all.cpp'
            fwd_tu.write_text(''.join(f'#include "{f}"\n' for f in sorted(fwd)) + '#include "aion/gameserver/configs/main/GSConfig.h"\n'
                              '#include "aion/gameserver/model/Gender.h"\n', encoding='utf-8')
            sources.append(fwd_tu)
            ok, output = ss.compile_check(work, [work / 'inc', ss.FIXTURE_CPP, ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC,
                                                 ss.VCPKG_INCLUDE], sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB8029' not in line and 'MSB8074' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems) or output[-4000:])
            # TODO-mode drafts (no fieldmap.json): same paths, so a second library over their own include root
            sources2 = [work / 'nofm' / f for f in sorted(drafts_nofm) if f.endswith('.cpp')]
            ok, output = ss.compile_check(work / 'second', [work / 'nofm', ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC,
                                                            ss.VCPKG_INCLUDE], sources2)
            problems = [line for line in ss.warnings_in(output) if 'MSB8029' not in line and 'MSB8074' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
