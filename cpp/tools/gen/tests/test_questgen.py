"""questgen (tools/gen/questgen, the phase-6 quest transliterator prototype): the Java statement parser, the C++ declaration reader, the
C++ overload check, and the transliteration of real quest handlers of different shapes, including refused ones. Nothing is compiled.

Run from cpp/tools/gen: python -m unittest tests.test_questgen
"""
from __future__ import annotations

import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.dont_write_bytecode = True

import javasrc  # noqa: E402
from questgen import api, cli, cppdecl, emit, jast, mirror, paths  # noqa: E402

QUEST = paths.JAVA_QUEST_DIR
HAVE_JAVA = QUEST.is_dir()


def parse_body(src):
    """statements of the body of the only method in `class X { void m() { src } }`"""
    cu = javasrc.parse_source('class X { void m() { ' + src + ' } }')
    m = cu.types[0].methods[0]
    return jast.Parser(cu).block_stmts(m.body.start)


class JavaStatementParser(unittest.TestCase):
    def test_precedence_and_casts(self):
        (s,) = parse_body('int x = a + b * c == d && !(e instanceof Npc) ? (int) f : (g);')
        self.assertIsInstance(s, jast.Local)
        init = s.decls[0][2]
        self.assertIsInstance(init, jast.Cond)
        self.assertEqual(init.cond.op, '&&')
        eq = init.cond.left
        self.assertEqual(eq.op, '==')
        self.assertEqual(eq.left.op, '+')
        self.assertEqual(eq.left.right.op, '*')
        self.assertIsInstance(init.cond.right, jast.Unary)
        self.assertIsInstance(init.cond.right.expr.expr, jast.InstanceOf)
        self.assertIsInstance(init.a, jast.Cast)
        self.assertEqual(init.a.type.name, 'int')
        self.assertIsInstance(init.b, jast.Paren)       # (g) is a parenthesized name, not a cast

    def test_class_cast_then_call(self):
        (s,) = parse_body('targetId = ((Npc) env.getVisibleObject()).getNpcId();')
        call = s.expr.value
        self.assertIsInstance(call, jast.Call)
        self.assertEqual(call.name, 'getNpcId')
        self.assertIsInstance(call.target.expr, jast.Cast)
        self.assertEqual(call.target.expr.type.name, 'Npc')

    def test_switch_groups_keep_fall_through(self):
        (s,) = parse_body('switch (v) { case 1: case 2: a(); case 3: b(); break; default: c(); }')
        self.assertIsInstance(s, jast.Switch)
        self.assertEqual([len(g[0]) for g in s.groups], [2, 1, 1])
        self.assertEqual(len(s.groups[0][1]), 1)       # a(); then falls into case 3
        self.assertIsNone(s.groups[2][0][0])            # default

    def test_shift_is_two_tokens(self):
        (s,) = parse_body('int x = a >> 2 > b;')
        init = s.decls[0][2]
        self.assertEqual(init.op, '>')
        self.assertEqual(init.left.op, '>>')

    def test_refused_constructs(self):
        for src, cat in (('run(() -> x());', 'lambda'), ('list.forEach(Foo::bar);', 'method-reference'),
                         ('schedule(new Runnable() { public void run() {} });', 'anonymous-class'), ('try { a(); } catch (Exception e) {}', 'try-catch'),
                         ('int y = switch (x) { case 1 -> 2; default -> 3; };', 'switch-expression'), ('throw new X();', 'throw'),
                         ('outer: for (;;) { break outer; }', 'labeled-statement')):
            with self.subTest(src=src):
                with self.assertRaises(jast.Unsupported) as cm:
                    parse_body(src)
                self.assertEqual(cm.exception.category, cat)


HEADER = '''#pragma once
#include "aion/gameserver/x/Other.h"
namespace aion::gameserver::x {
class Base {
public:
	virtual bool f(int64_t a) = 0;
	void g(int32_t a);
};
/** doc */
class Derived final : public Base {
	AION_MAKE_REF_FRIEND
	runtime::Field<int32_t> v{};
public:
	using Base::f;
	bool f(int64_t a, bool b) { return b; }
	static runtime::Ptr<model::Thing> make(const std::vector<const T*>& items, std::initializer_list<int32_t> ids = {});
	void g(bool a);
	template <class P>
	static void send(P&& packet);
};
enum class Kind : uint8_t { A, B = 3, C };
constexpr int32_t getId(Kind k) noexcept { return 0; }
}
'''


class CppDeclarations(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='qg'))
        (self.tmp / 'aion/gameserver/x').mkdir(parents=True)
        (self.tmp / 'aion/gameserver/x/H.h').write_text(HEADER, encoding='utf-8')
        self.ix = cppdecl.HeaderIndex([self.tmp])
        self.ix.scan('aion/gameserver/x/H.h')

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_classes_bases_and_members(self):
        d = self.ix.classes['Derived']
        self.assertEqual(d.qual, ('aion', 'gameserver', 'x', 'Derived'))
        self.assertEqual(d.bases, ['Base'])
        make = d.methods['make'][0]
        self.assertTrue(make.static)
        self.assertEqual(make.ret, 'runtime::Ptr<model::Thing>')
        self.assertEqual([p.type for p in make.params], ['const std::vector<const T*>&', 'std::initializer_list<int32_t>'])
        self.assertIsNotNone(make.params[1].default)
        self.assertTrue(d.methods['send'][0].template)
        self.assertTrue(self.ix.classes['Base'].methods['f'][0].virtual)

    def test_using_declaration_keeps_base_overloads_and_hiding_does_not(self):
        self.assertEqual(sorted(len(f.params) for f in self.ix.lookup('Derived', 'f')), [1, 2])
        self.assertEqual([f.params[0].type for f in self.ix.lookup('Derived', 'g')], ['bool'])   # Base::g(int32_t) is hidden
        self.assertEqual(self.ix.owner_of('Derived', 'g'), 'Derived')
        self.assertTrue(self.ix.is_subclass('Derived', 'Base'))

    def test_enums_and_companions(self):
        self.assertEqual(self.ix.enums['Kind'].constants, ['A', 'B', 'C'])
        self.assertEqual([f.ns for f in self.ix.free['getId']], [('aion', 'gameserver', 'x')])


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class RealQuests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tr = emit.Transliterator()

    def run_file(self, rel):
        return self.tr.transliterate(QUEST / rel)

    def test_talk_chain_tier_a(self):
        r = self.run_file('eltnen/_1363ThankingMabangtah.java')
        self.assertEqual((r.status, r.tier, r.quest_id), ('ok', 'A', 1363), r.reasons)
        cpp = r.cpp
        self.assertTrue(cpp.startswith('#include "aion/gameserver/handlers/quest/QuestPrelude.h"\n'))
        self.assertIn('namespace aion::gameserver::handlers::quest::eltnen {', cpp)
        self.assertIn('class _1363ThankingMabangtah final : public AbstractQuestHandler {', cpp)
        self.assertIn('_1363ThankingMabangtah() : AbstractQuestHandler(1363) {}', cpp)
        self.assertIn('\t\tqe.registerQuestNpc(203943)->addOnQuestStart(questId); // Turiel\n', cpp)
        self.assertIn('bool onDialogEvent(QuestEnv& env) override {', cpp)
        self.assertIn('runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);', cpp)
        self.assertIn('if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)', cpp)
        self.assertIn('targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();', cpp)
        self.assertIn('if (qs == nullptr || qs->isStartable()) {', cpp)
        self.assertIn('qs->setStatus(QuestStatus::REWARD);', cpp)
        # F01's close-step idiom: SM_DIALOG_WINDOW(obj, 10) sent directly (phase6-inventory.md §4.5), the Ptr dereferenced for Player&
        self.assertIn('PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));', cpp)
        self.assertIn('#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"', cpp)
        self.assertTrue(cpp.rstrip().endswith('AION_QUEST_HANDLER(_1363ThankingMabangtah, 1363);\n\n} // namespace aion::gameserver::handlers::quest::eltnen'))
        self.assertEqual(r.apis['QuestEngine.registerQuestNpc'], 3)

    def test_mission_chain_hooks_switch_and_varargs(self):
        r = self.run_file('altgard/_24011FunnyFloatingFungus.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        cpp = r.cpp
        self.assertIn('std::array<int32_t, 3> talkNpcs{203558, 203572, 203558};', cpp)
        self.assertIn('for (int32_t id : talkNpcs)\n\t\t\tqe.registerQuestNpc(id)->addOnTalkEvent(questId);', cpp)
        self.assertIn('switch (dialogActionId) {\n\t\t\t\t\tcase QUEST_SELECT:\n', cpp)
        self.assertIn('void onQuestCompletedEvent(QuestEnv& env) override {\n\t\tdefaultOnQuestCompletedEvent(env, {24010});', cpp)
        self.assertIn('void onLevelChangedEvent(Player& player) override {\n\t\tdefaultOnLevelChangedEvent(player, {24010});', cpp)
        self.assertIn('changeQuestStep(env, 6, 6, true); // reward', cpp)

    def test_item_use_calls_the_planned_fromBoolean(self):
        r = self.run_file('gelkmaros/_21137BerokinImageMarble.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(r.undeclared, {'HandlerResult.fromBoolean'})
        self.assertIn('HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {', r.cpp)
        self.assertIn('return ::aion::gameserver::questEngine::handlers::fromBoolean(sendQuestDialog(env, 4));', r.cpp)
        self.assertIn('return HandlerResult::FAILED;', r.cpp)
        self.assertIn('#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"', r.cpp)

    def test_constant_arrays_become_static_constexpr_members(self):
        r = self.run_file('daevanion/_19638TroublewithTwos.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('private:\n\tstatic constexpr std::array<int32_t, 2> npcIds{798926, 799022}; // Outremus & Lothas', r.cpp)
        self.assertIn('qe.registerQuestNpc(npcIds[0])->addOnQuestStart(questId);', r.cpp)
        self.assertIn('changeQuestStep(env, var1, var1 + 1, false, 1); // @1: 1 - 9', r.cpp)
        self.assertIn('if (0 <= var1 && var1 < 9) {', r.cpp)

    def test_tier_b_kinah_and_rewards(self):
        r = self.run_file('brusthonin/_4074GainOrLose.java')
        self.assertEqual((r.status, r.tier), ('ok', 'B'), r.reasons)
        self.assertIn('B06', r.api_rows)                  # kinah
        self.assertIn('int64_t kinahAmount = player->getInventory().getKinah();', r.cpp)
        self.assertIn('if (qs->getRewardGroup() == std::nullopt)', r.cpp)
        self.assertIn('switch (qs->getRewardGroup().value()) {', r.cpp)
        self.assertIn('ItemService::addItem(*player, 186000010, ::aion::commons::utils::Rnd::get(1, 3));', r.cpp)

    def test_case_bodies_with_declarations_are_braced(self):
        r = self.run_file('sanctum/_1901KrallicPotion.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('case SELECT2_2: {\n', r.cpp)
        self.assertIn('Storage& inventory = player->getInventory();', r.cpp)   # a part accessor (Storage&) kept as a reference
        self.assertIn('if (inventory.tryDecreaseKinah(10000)) {', r.cpp)     # IStorage::tryDecreaseKinah(int64_t) through `using`
        # the inner switch has a default and returns on every path: Java cannot fall into `case 798025`, so no [[fallthrough]]
        self.assertNotIn('[[fallthrough]]', r.cpp)

    def test_float_literals_and_teleports(self):
        r = self.run_file('aturam_sky_fortress/_18303MakingASurCantA.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('B01', r.api_rows)
        self.assertNotRegex(r.cpp, r'[^.\d]\d+f\b')       # Java 901f is C++ 901.0f
        self.assertIn('901.0f', r.cpp)

    def test_refused_closure(self):
        r = self.run_file('kaisinel_academy/_37003CamouflageKillers.java')     # phase6-inventory.md §4.1: F07 mentor dailies
        self.assertEqual(r.status, 'refused')
        self.assertEqual(r.primary, 'lambda')
        self.assertEqual(r.cpp, '')

    def test_refused_mutable_field(self):
        r = self.run_file('eltnen/_1367MabangtahsFeast.java')                   # phase6-inventory.md §11: per-player state in fields
        self.assertEqual((r.status, r.primary), ('refused', 'mutable-field'))

    def test_refused_api_gap(self):
        r = self.run_file('steel_rake/_3208ThePuzzlingBlueprint.java')
        self.assertEqual(r.status, 'refused')
        self.assertEqual(r.reason_keys(), ['api-missing: QuestService.checkStartConditions'])

    def test_refused_varargs_array(self):
        r = self.run_file('poeta/_1005BarringtheGate.java')
        self.assertEqual((r.status, r.primary), ('refused', 'varargs-array'))

    def test_refused_header_signature(self):
        # Java adds to the reward list (_80016EventSockHop.java:81; QuestService.java:197-203 hands the same list on as the reward), but
        # the C++ hook takes it as a const vector of raw const pointers (AbstractQuestHandler.h:144-145): no API row can close that
        r = self.run_file('event_quests/_80016EventSockHop.java')
        self.assertEqual((r.status, r.primary), ('refused', 'header-signature'))
        self.assertEqual(r.reason_keys(), ['header-signature: AbstractQuestHandler.onBonusApplyEvent(rewardItems) is '
                                           'const std::vector<const QuestItems*>&; Java List.add mutates it'])
        rep = cli.summarize([r], self.tr.api)
        self.assertEqual(rep['apiGaps'], [])                  # not an API gap
        self.assertEqual(list(rep['refusedByPrimaryReason']), r.reason_keys())

    def test_types_the_prelude_exports_are_indexed(self):
        # House, SpawnSearchResult and InstanceHandler have headers (QuestPrelude.h re-exports them): the gap is the API behind them
        for rel, key in (('reshanta/_24043LazyLanguageLessons.java', 'api-missing: DataManager.SPAWNS_DATA'),
                         ('inggison/_10034FoundUnderground.java', 'api-missing: WorldMapInstance.getInstanceHandler'),
                         ('oriel/_18830MovingIn.java', 'api-missing: Player.getActiveHouse')):
            with self.subTest(rel=rel):
                keys = self.run_file(rel).reason_keys()
                self.assertIn(key, keys)
                self.assertFalse([k for k in keys if k.startswith('type:')], keys)

    def test_comments_on_case_labels_methods_and_if_heads(self):
        r = self.run_file('bare_truth/_14030RetrievedMemory.java')
        self.assertIn('\tcase 203700: // Fasimedes\n', r.cpp)
        r = self.run_file('abyss_entry/_1922DeliveronYourPromises.java')
        self.assertRegex(r.cpp, r"\n\t+// Should be removed \(it's for backward compatibility\)\n"
                                r"\t+// Epeios sets REWARD status now and doesn't reach this part\n\t+case SELECT_QUEST_REWARD:\n")
        r = self.run_file('altgard/_2263ShugoPotion.java')
        self.assertIn('\n\t// On time end if not in reward status delete quest items and quest itself\n'
                      '\tbool onQuestTimerEndEvent(QuestEnv& env) override {\n', r.cpp)
        r = self.run_file('altgard/_24012AnOminousCrop.java')
        self.assertIn('if (targetId == 203605) // Loriniah\n', r.cpp)

    def test_known_java_bug_is_kept_and_marked(self):
        # phase6-inventory.md §11 and decision U3: a faithful port plus a note
        r = self.run_file('beshmundir/_30348ImprovedAethercannon.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertRegex(r.cpp, r'\n\t+// java-bug kept \(U3, phase6-inventory\.md §11\): requires 100100716 [^\n]*101900655[^\n]*\n'
                                r'\t+if \(player->getInventory\(\)\.getItemCountByItemId\(100100716\) >= 1\) \{')
        self.assertEqual(len(r.java_bugs), 1)
        r = self.run_file('sanctum/_3963GrowthFlorasThirdCharm.java')
        self.assertRegex(r.cpp, r'\n\t+// java-bug kept [^\n]*tryDecreaseKinah[^\n]*\n\t+player->getInventory\(\)\.decreaseKinah\(70000\);')


class ApiTable(unittest.TestCase):
    def test_event_quest_row_is_gated_on_m5d(self):
        # QuestService.startEventQuest is ported in M5d E-02 (m5d-plan.md:541); only the event content itself is M5i
        row = {r.id: r for r in api.API_TABLE}['B05']
        self.assertTrue(row.gate.startswith('M5d E-02'), row.gate)


SYNTHETIC = '''package quest.test;

import static com.aionemu.gameserver.model.DialogAction.*;

import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
import com.aionemu.gameserver.questEngine.model.QuestEnv;
import com.aionemu.gameserver.questEngine.model.QuestState;

public class _99001Synthetic extends AbstractQuestHandler {

	private static final int NPC = 123; // the npc

	public _99001Synthetic() {
		super(99001);
	}

	@Override
	public void register() {
		qe.registerQuestNpc(NPC).addOnTalkEvent(questId);
	}

	@Override
	public boolean onDialogEvent(QuestEnv env) {
		Player player = env.getPlayer();
		QuestState qs = player.getQuestStateList().getQuestState(questId);
		int x = 3;
		long total = 0;
		total += 2.5;
		x += total;
		switch (env.getDialogActionId()) {
			case QUEST_SELECT:
				x++;
			case SETPRO1:
				return defaultCloseDialog(env, 0, 1);
			default:
				break;
		}
		return helper(env, qs, x);
	}

	private boolean helper(QuestEnv env, QuestState qs, int x) {
		return qs != null && x > 1 && sendQuestDialog(env, 1011);
	}
}
'''


class SyntheticQuest(unittest.TestCase):
    """rules no real quest exercises today: a genuine fall-through, narrowing compound assignment, unused locals, helper methods"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix='qg'))
        (cls.tmp / 'test').mkdir()
        (cls.tmp / 'test' / '_99001Synthetic.java').write_text(SYNTHETIC, encoding='utf-8')
        cls.r = emit.Transliterator(quest_dir=cls.tmp).transliterate(cls.tmp / 'test' / '_99001Synthetic.java')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_status(self):
        self.assertEqual((self.r.status, self.r.tier, self.r.quest_id), ('ok', 'A', 99001), self.r.reasons)

    def test_constant_member(self):
        self.assertIn('private:\n\tstatic constexpr int32_t NPC = 123; // the npc\n', self.r.cpp)
        self.assertIn('qe.registerQuestNpc(NPC)->addOnTalkEvent(questId);', self.r.cpp)

    def test_fall_through_is_kept_and_marked(self):
        self.assertIn('\t\t\tcase QUEST_SELECT:\n\t\t\t\tx++;\n\t\t\t\t[[fallthrough]]; // Java: no break\n\t\t\tcase SETPRO1:\n', self.r.cpp)

    def test_narrowing_compound_assignment(self):
        self.assertIn('total = static_cast<int64_t>(total + 2.5);', self.r.cpp)
        self.assertIn('x = static_cast<int32_t>(x + total);', self.r.cpp)

    def test_helper_method(self):
        self.assertIn('return helper(env, qs, x);', self.r.cpp)
        self.assertIn('\n\nprivate:\n\tbool helper(QuestEnv& env, runtime::Ptr<QuestState> qs, int32_t x) {', self.r.cpp)
        self.assertIn('return qs != nullptr && x > 1 && sendQuestDialog(env, 1011);', self.r.cpp)


SYNTHETIC_COMMENTS = '''package quest.test;

import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
import com.aionemu.gameserver.questEngine.model.QuestEnv;

public class _99002Comments extends AbstractQuestHandler {

	public _99002Comments() {
		super(99002);
	}

	// registers the npc
	@Override
	public void register() {
		qe.registerQuestNpc(123).addOnTalkEvent(questId);
	}

	@Override
	public boolean onDialogEvent(QuestEnv env) {
		switch (env.getTargetId()) {
			case 123: // Npc A
			case 124: { // braced in Java
				return false;
			}
			// the B npc
			case 125: // Npc B
				int x = 1;
				return x > 0;
			default: // anyone else
				break;
		}
		if (env.getTargetId() == 1)// brace on the next line
		{
			return false;
		}
		if (env.getTargetId() == 0) // nobody
			return false;
		else // somebody
			return true;
	}
}
'''


class SyntheticComments(unittest.TestCase):
    """Java comments on case labels (trailing and own-line), before methods and after if/else heads without braces are kept"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix='qg'))
        (cls.tmp / 'test').mkdir()
        (cls.tmp / 'test' / '_99002Comments.java').write_text(SYNTHETIC_COMMENTS, encoding='utf-8')
        cls.r = emit.Transliterator(quest_dir=cls.tmp).transliterate(cls.tmp / 'test' / '_99002Comments.java')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_status(self):
        self.assertEqual(self.r.status, 'ok', self.r.reasons)

    def test_case_labels(self):
        self.assertIn('\t\t\tcase 123: // Npc A\n\t\t\tcase 124:\n\t\t\t\t{ // braced in Java\n', self.r.cpp)
        # an own-line comment before a label stays before it; the brace the emitter adds goes before the label's comment
        self.assertIn('\n\t\t\t\t}\n\t\t\t// the B npc\n\t\t\tcase 125: { // Npc B\n\t\t\t\tint32_t x = 1;\n', self.r.cpp)
        self.assertIn('\t\t\tdefault: // anyone else\n\t\t\t\tbreak;\n', self.r.cpp)

    def test_method_comment(self):
        self.assertIn('{}\n\n\t// registers the npc\n\tvoid register_() override {\n', self.r.cpp)

    def test_if_and_else_heads_without_braces(self):
        self.assertIn('\t\tif (env.getTargetId() == 0) // nobody\n\t\t\treturn false;\n\t\telse // somebody\n\t\t\treturn true;\n', self.r.cpp)

    def test_comment_before_a_brace_on_the_next_line(self):
        self.assertIn('\t\tif (env.getTargetId() == 1) { // brace on the next line\n\t\t\treturn false;\n\t\t}\n', self.r.cpp)

    def test_every_comment_once(self):
        for c in ('// Npc A', '// braced in Java', '// the B npc', '// Npc B', '// anyone else', '// registers the npc', '// nobody',
                  '// brace on the next line', '// somebody'):
            with self.subTest(c=c):
                self.assertEqual(self.r.cpp.count(c), 1)


class OverloadPinning(unittest.TestCase):
    """C++ ranks overloads differently from Java: an int converts to bool, a string literal to bool before std::string_view."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='qg'))
        (self.tmp / 'aion/gameserver/y').mkdir(parents=True)
        (self.tmp / 'aion/gameserver/y/S.h').write_text('''namespace aion::gameserver::y {
class S {
public:
	static bool f(int64_t a, int64_t b);
	static bool f(bool a, bool b);
	static bool g(std::string_view s);
	static bool g(bool b);
};
}
''', encoding='utf-8')
        self.tr = emit.Transliterator.__new__(emit.Transliterator)
        self.tr.ix = cppdecl.HeaderIndex([self.tmp])
        self.tr.ix.scan('aion/gameserver/y/S.h')
        self.tr.api = type('A', (), {'is_enum': staticmethod(lambda n: False)})()
        self.tr.r = emit.FileResult('t')

    def resolve(self, name, args):
        x = jast.Call(0, None, name, [jast.Lit(0, 'x', '') for _ in args])
        self.tr.expr = lambda _e, it=iter(args): next(it)
        self.tr.cu = type('CU', (), {'tokens': type('T', (), {'loc': staticmethod(lambda i: (1, 1))})()})()
        return self.tr.resolve(self.tr.ix.classes['S'].methods[name], x, f'S.{name}')

    def test_int_arguments_are_cast_to_the_java_overload(self):
        f, texts = self.resolve('f', [emit.E('1', emit.INT), emit.E('2', emit.INT)])
        self.assertEqual([p.type for p in f.params], ['int64_t', 'int64_t'])       # Java: int widens to long, never to boolean
        self.assertEqual(texts, ['static_cast<int64_t>(1)', 'static_cast<int64_t>(2)'])   # C++ would find f ambiguous

    def test_string_literal_is_pinned_to_string_view(self):
        f, texts = self.resolve('g', [emit.E('"a"', emit.STRING)])
        self.assertEqual(f.params[0].type, 'std::string_view')
        self.assertEqual(texts, ['std::string_view("a")'])                         # C++ would pick g(bool)


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class DriverAndTwins(unittest.TestCase):
    def test_emit_refuses_the_repository(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()) as err:
            cli.main(['--dry-run', '--quiet', '--no-pairs', '--only', '_1363ThankingMabangtah', '--emit', str(paths.CPP_ROOT / 'build' / 'x')])
        self.assertIn('inside the repository', err.getvalue())
        self.assertFalse((paths.CPP_ROOT / 'build' / 'x').exists())

    def test_run_summary_and_emit(self):
        out = Path(tempfile.mkdtemp(prefix='qg'))
        try:
            files = cli.find_files(['_1363ThankingMabangtah', 'kaisinel_academy/_37003CamouflageKillers.java',
                                    '_21137BerokinImageMarble'])
            results, rep = cli.run(files, out, pairs=False)
            c = rep['coverage']
            self.assertEqual((c['transliterated'], c['refused'], c['total']), (2, 1, 3))
            self.assertEqual(c['blockedOnPlannedDeclaration'], 1)
            self.assertIn('lambda', rep['refusedByPrimaryReason'])
            written = out / 'aion/gameserver/handlers/quest/eltnen/_1363ThankingMabangtah.cpp'
            self.assertTrue(written.is_file())
            self.assertNotIn(b'\r', written.read_bytes())
            used = {row['api']: row for row in rep['apiUsed']}
            self.assertEqual(used['HandlerResult.fromBoolean']['declared'], 'planned')
            self.assertEqual(used['QuestEngine.registerQuestNpc']['declared'], 'yes')
        finally:
            shutil.rmtree(out, ignore_errors=True)

    def test_literal_twins(self):
        a, la = mirror.normalized(QUEST / 'abyss_entry/_1920TestingYourMettle.java')
        b, lb = mirror.normalized(QUEST / 'abyss_entry/_2945HoningYourSkills.java')
        self.assertEqual(a, b)                  # the same structure: one port plus a literal substitution row (G2)
        self.assertNotEqual(la, lb)
        c, _ = mirror.normalized(QUEST / 'eltnen/_1363ThankingMabangtah.java')
        self.assertNotEqual(a, c)


if __name__ == '__main__':
    unittest.main()
