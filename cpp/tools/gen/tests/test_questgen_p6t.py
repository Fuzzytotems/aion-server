"""questgen's P6-T rules (phase6-questgen-prototype.md §5.2 items 3-4 and §8.4; emit.P6T_RULES) and the parity of the generator's output
with its Java (tools/parity): each rule on the real handlers it exists for, the same handlers still refused without it (the prototype's
rev-2 behaviour, which test_questgen.py pins), each rule's limits on synthetic handlers, the driver, every transliterated file at parity,
the rules adding files without changing any other, and the golden oracle's first slice being tier A. Nothing is compiled.

Run from cpp/tools/gen: python -m unittest tests.test_questgen_p6t
"""
from __future__ import annotations

import ast
import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'parity'))
sys.dont_write_bytecode = True

import javasrc  # noqa: E402
import parity  # noqa: E402  (tools/parity)
from questgen import cli, emit, jast, paths  # noqa: E402

QUEST = paths.JAVA_QUEST_DIR
HAVE_JAVA = QUEST.is_dir()
RULES = emit.P6T_RULES
# the files each rule unblocks: the prototype refused them for this reason only (phase6-questgen-prototype.md §4, §5.1)
UNBLOCKED = {
    'varargs-inline': ('altgard/_24016AStrangeNewThread.java', 'beluslan/_24054CrisisinBeluslan.java', 'poeta/_1005BarringtheGate.java',
                       'verteron/_14016AGateAgape.java'),
    'work-items': ('beluslan/_2663AcquiringanAntidote.java', 'beluslan/_2664AnAntidotetotheLepharists.java',
                   'eltnen/_1483HarumonerksRequest.java', 'eltnen/_1484ChiyorinrinerksRequest.java', 'theobomos/_3088InCiderTrading.java'),
    'switch-expression': ('sanctum/_1917ALingeringMystery.java', 'the_eternal_bastion/_18035ShebasSurveillance.java',
                          'the_eternal_bastion/_28035TrustInNoneButVerify.java', 'beshmundir/_30211GroupTheRodandtheOrb.java'),
    'nested-array': ('abyss_entry/_1044TestingFlightSkills.java', 'abyss_entry/_2042TheLastCheckpoint.java', 'daevanion/_1993AnotherBeginning.java',
                     'daevanion/_1994ANewChoice.java', 'daevanion/_2993AnotherBeginning.java', 'daevanion/_2994ANewChoice.java'),
}
PROTOTYPE_REASON = {'varargs-inline': 'varargs-array', 'work-items': 'api-missing', 'switch-expression': 'switch-expression',
                    'nested-array': 'type'}

_TR = {}


def transliterator(rules):
    """one Transliterator per rule set, all over one Api (the header index and the census are read once)"""
    key = frozenset(rules)
    if key not in _TR:
        shared = next(iter(_TR.values())).api if _TR else None
        _TR[key] = emit.Transliterator(api=shared, rules=key)
    return _TR[key]


class Synthetic:
    """a Java handler written to a temporary quest directory and transliterated with the given rules"""

    def __init__(self, rel, source, rules=RULES):
        self.tmp = Path(tempfile.mkdtemp(prefix='qg'))
        f = self.tmp / rel
        f.parent.mkdir(parents=True)
        f.write_text(source, encoding='utf-8')
        tr = transliterator(rules)
        saved = tr.quest_dir
        tr.quest_dir = self.tmp
        try:
            self.r = tr.transliterate(f)
        finally:
            tr.quest_dir = saved
            shutil.rmtree(self.tmp, ignore_errors=True)


HEAD = '''package quest.test;

import static com.aionemu.gameserver.model.DialogAction.*;

import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
import com.aionemu.gameserver.questEngine.model.QuestEnv;
import com.aionemu.gameserver.questEngine.model.QuestState;
import com.aionemu.gameserver.questEngine.model.QuestStatus;

public class {cls} extends AbstractQuestHandler {{
{fields}
	public {cls}() {{
		super({qid});
	}}

	@Override
	public void register() {{
		qe.registerQuestNpc(123).addOnTalkEvent(questId);
	}}
{body}}}
'''


def source(cls, qid, body, fields=''):
    return HEAD.format(cls=cls, qid=qid, body=body, fields=fields)


SWITCHES = source('_99003Switches', 99003, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);
		int page = 0;
		page = switch (env.getDialogActionId()) {
			case QUEST_SELECT -> 1011;
			case SETPRO1, SETPRO2 -> 1012; // two labels
			default -> 0;
		};
		switch (env.getDialogActionId()) {
			case SETPRO1 -> qs.setQuestVarById(0, 1);
			case SETPRO2 -> {
				qs.setQuestVarById(0, 2);
				return true;
			}
			default -> {
			}
		}
		return sendQuestDialog(env, page);
	}

	@Override
	public boolean onKillEvent(QuestEnv env) {
		return switch (env.getTargetId()) {
			case 123 -> true;
			default -> false;
		};
	}
''')

SWITCHES_REFUSED = source('_99004Switches', 99004, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		int x = 1 + switch (env.getDialogActionId()) {
			case QUEST_SELECT -> 1;
			default -> 2;
		};
		return x > 1;
	}

	@Override
	public boolean onKillEvent(QuestEnv env) {
		QuestState qs = env.getPlayer().getQuestStateList().getQuestState(questId);
		return switch (qs.getStatus()) {
			case START -> true;
			case REWARD, COMPLETE, LOCKED -> false;
		};
	}

	@Override
	public boolean onAttackEvent(QuestEnv env) {
		return switch (env.getTargetId()) {
			case 123 -> {
				yield true;
			}
			default -> false;
		};
	}
''')

STRING_SWITCH = source('_99010Switches', 99010, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		String name = "a";
		return switch (name) {
			case "a" -> true;
			default -> false;
		};
	}
''')

VARARGS_USED_AFTER = source('_99011Varargs', 99011, '''
	@Override
	public void onLevelChangedEvent(Player player) {
		int[] quests = { 99001, 99002 };
		defaultOnLevelChangedEvent(player, quests);
		player.getQuestStateList().getQuestState(quests[0]);
	}
''')

VARARGS_KEPT = source('_99005Varargs', 99005, '''
	@Override
	public void onQuestCompletedEvent(QuestEnv env) {
		int[] quests = { 99001, 99002 };
		for (int q : quests)
			env.setQuestId(q);
		defaultOnQuestCompletedEvent(env, quests);
	}

	@Override
	public void onLevelChangedEvent(Player player) {
		int[] quests = { questId, 99002 };
		defaultOnLevelChangedEvent(player, quests);
	}
''')

WORK_ITEMS = source('_99006WorkItems', 99006, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		int last = workItems.size() - 1;
		if (workItems.isEmpty() || last > 2)
			return false;
		return giveQuestItem(env, workItems.get(last).getItemId(), workItems.getFirst().getCount());
	}
''')

WORK_ITEMS_REFUSED = source('_99007WorkItems', 99007, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		return workItems.remove(0) != null;
	}
''')

ARRAYS = source('_99008Arrays', 99008, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		String[] names = { "a", "b" };
		int[][] local = { { 1, 2 }, { 3, 4 } };
		return names[1].equals("b") && local[1][0] == 3 && ROWS[env.getDialogActionId()][1] == 2;
	}
''', fields='\n\tprivate static final int[][] ROWS = { { 1, 2 }, { 3, 4 }, { 5, 6 } };\n')

JAGGED = source('_99009Arrays', 99009, '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		return ROWS[0][0] == 1;
	}
''', fields='\n\tprivate static final int[][] ROWS = { { 1, 2 }, { 3 } };\n')


def parse_body(src, **kw):
    cu = javasrc.parse_source('class X { void m() { ' + src + ' } }')
    return jast.Parser(cu, **kw).block_stmts(cu.types[0].methods[0].body.start)


class Parser(unittest.TestCase):
    def test_switch_expressions_and_rule_arms_are_opt_in(self):
        src = 'int y = switch (x) { case 1, 2 -> 3; default -> 4; }; switch (x) { case A -> a(); default -> { b(); } }'
        with self.assertRaises(jast.Unsupported) as cm:
            parse_body(src)
        self.assertEqual(cm.exception.category, 'switch-expression')
        local, sw = parse_body(src, switch_expressions=True)
        sx = local.decls[0][2]
        self.assertIsInstance(sx, jast.SwitchExpr)
        self.assertEqual([[None if lab is None else lab.text for lab in labels] for labels, _v, _t, _s in sx.arms], [['1', '2'], [None]])
        self.assertEqual([v.text for _l, v, _t, _s in sx.arms], ['3', '4'])
        self.assertIsInstance(sw, jast.Switch)
        self.assertTrue(sw.rules)
        self.assertEqual([type(body[0]).__name__ for _l, body, _t in sw.groups], ['ExprStmt', 'Block'])

    def test_a_rule_arm_label_is_not_a_lambda(self):
        # the prototype read `case SETPRO1, QUEST_SELECT -> {` as a lambda (beshmundir/_30211GroupTheRodandtheOrb.java:52)
        src = 'switch (x) { case A, B -> { c(); } }'
        with self.assertRaises(jast.Unsupported) as cm:
            parse_body(src)
        self.assertEqual(cm.exception.category, 'lambda')
        (sw,) = parse_body(src, switch_expressions=True)
        self.assertEqual([lab.name for lab in sw.groups[0][0]], ['A', 'B'])

    def test_a_block_arm_with_yield_stays_refused(self):
        with self.assertRaises(jast.Unsupported) as cm:
            parse_body('int y = switch (x) { case 1 -> { yield 2; } default -> 3; };', switch_expressions=True)
        self.assertEqual(cm.exception.category, 'switch-expression')

    def test_a_lambda_after_a_rule_arm_is_still_a_lambda(self):
        # the rule-arm label mode ends with the labels: a later `y -> ...` in the same method is refused as a lambda
        with self.assertRaises(jast.Unsupported) as cm:
            parse_body('switch (x) { case A -> a(); } Consumer c = y -> b(y);', switch_expressions=True)
        self.assertEqual((cm.exception.category, str(cm.exception)), ('lambda', 'lambda: line 1:65'))

    def test_a_throw_arm_is_refused(self):
        with self.assertRaises(jast.Unsupported) as cm:
            parse_body('switch (x) { case A -> throw new E(); }', switch_expressions=True)
        self.assertEqual(cm.exception.category, 'throw')


class RuleNames(unittest.TestCase):
    def test_unknown_rule_is_an_error(self):
        with self.assertRaises(ValueError):
            emit.Transliterator(rules={'varargs-inline', 'no-such-rule'})
        self.assertEqual(RULES, {'varargs-inline', 'work-items', 'switch-expression', 'nested-array'})


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class RealFiles(unittest.TestCase):
    def run_file(self, rel, rules=RULES):
        return transliterator(rules).transliterate(QUEST / rel)

    def test_each_rule_unblocks_its_files_and_only_with_the_rule(self):
        for rule, rels in UNBLOCKED.items():
            for rel in rels:
                with self.subTest(rule=rule, rel=rel):
                    self.assertEqual(self.run_file(rel).status, 'ok', self.run_file(rel).reasons)
                    without = self.run_file(rel, RULES - {rule})
                    self.assertEqual(without.status, 'refused')
                    self.assertIn(PROTOTYPE_REASON[rule] if rel != 'beshmundir/_30211GroupTheRodandtheOrb.java' else 'lambda',
                                  [c for c, _d in without.reasons])

    def test_varargs_inline(self):
        r = self.run_file('poeta/_1005BarringtheGate.java')
        self.assertIn('\tvoid onQuestCompletedEvent(QuestEnv& env) override {\n'
                      '\t\tdefaultOnQuestCompletedEvent(env, {1100, 1004, 1003, 1002, 1001});\n\t}', r.cpp)
        self.assertIn('defaultOnLevelChangedEvent(player, {1100, 1004, 1003, 1002, 1001});', r.cpp)
        self.assertNotIn('quests', r.cpp)                                   # the local is gone, its values moved into the call
        self.assertEqual(r.idioms['int[] local passed inline to a varargs helper'], 2)
        self.assertEqual(r.tier, 'A')

    def test_work_items(self):
        r = self.run_file('eltnen/_1484ChiyorinrinerksRequest.java')
        self.assertIn('giveQuestItem(env, workItems->get(0)->getItemId(), workItems->get(0)->getCount());', r.cpp)      # getFirst()
        self.assertIn('giveQuestItem(env, workItems->get(1)->getItemId(), workItems->get(1)->getCount());', r.cpp)      # get(1)
        self.assertIn('workItems->get(workItems->size() - 1)->getItemId()', r.cpp)                                     # getLast()
        self.assertEqual(r.api_tier['AbstractQuestHandler.workItems'], 'core')
        self.assertEqual(r.api_status['AbstractQuestHandler.workItems'], {'ported'})
        r = self.run_file('beluslan/_2664AnAntidotetotheLepharists.java')
        self.assertIn('return sendQuestStartDialog(env, workItems->get(0));', r.cpp)   # the const QuestItems* overload
        r = self.run_file('beluslan/_2663AcquiringanAntidote.java')
        self.assertIn('return defaultCloseDialog(env, 0, 1, true, false, workItems->get(0));', r.cpp)

    def test_switch_expressions(self):
        r = self.run_file('sanctum/_1917ALingeringMystery.java')
        self.assertIn('std::optional<int32_t> rewardGroup{}; // Java: initialized by the switch expression below\n', r.cpp)
        self.assertIn('\t\t\t\t\t\tcase SETPRO3:\n\t\t\t\t\t\t\trewardGroup = 2;\n\t\t\t\t\t\t\tbreak;\n\t\t\t\t\t\tdefault:\n'
                      '\t\t\t\t\t\t\trewardGroup = std::nullopt;\n\t\t\t\t\t\t\tbreak;\n', r.cpp)
        r = self.run_file('the_eternal_bastion/_18035ShebasSurveillance.java')
        self.assertIn('switch (dialogActionId) { // Java: a switch expression\n\t\t\t\t\t\tcase QUEST_SELECT:\n'
                      '\t\t\t\t\t\t\treturn sendQuestDialog(env, 1352);\n', r.cpp)
        self.assertIn('\t\t\t\t\t\tdefault:\n\t\t\t\t\t\t\treturn false;\n', r.cpp)
        r = self.run_file('beshmundir/_30211GroupTheRodandtheOrb.java')
        self.assertIn('\t\t\t\t\t\tcase SETPRO1:\n\t\t\t\t\t\tcase QUEST_SELECT:\n\t\t\t\t\t\t\t{\n', r.cpp)
        self.assertNotIn('[[fallthrough]]', r.cpp)
        self.assertNotIn('break; // Java: a `case ->` arm', r.cpp)          # the arm returns on every path

    def test_nested_arrays(self):
        r = self.run_file('daevanion/_1993AnotherBeginning.java')
        self.assertIn('static constexpr std::array<std::array<int32_t, 2>, 25> items{{{110600834, 110600835}, {113600800, 113600801}, ', r.cpp)
        self.assertIn('{111301470, 111301480}}};', r.cpp)
        self.assertIn('for (int32_t itemId : items.at(dialogIndex)) {', r.cpp)
        r = self.run_file('abyss_entry/_1044TestingFlightSkills.java')
        self.assertIn('static constexpr std::array<std::string_view, 6> rings{"ELTNEN_FORTRESS_210020000_1", ', r.cpp)
        self.assertIn('#include <string_view>', r.cpp)
        self.assertIn('for (std::string_view ring : rings) {', r.cpp)
        self.assertIn('if (rings[0] == flyingRing) {', r.cpp)


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class SyntheticRules(unittest.TestCase):
    def test_switch_expression_forms(self):
        r = Synthetic('test/_99003Switches.java', SWITCHES).r
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tint32_t page = 0;\n\t\tswitch (env.getDialogActionId()) { // Java: a switch expression\n\t\t\tcase QUEST_SELECT:\n'
                      '\t\t\t\tpage = 1011;\n\t\t\t\tbreak;\n\t\t\tcase SETPRO1:\n\t\t\tcase SETPRO2: // two labels\n\t\t\t\tpage = 1012;\n', r.cpp)
        # rule arms do not fall through: an expression arm and an empty block get a break, a block that returns does not
        self.assertIn('\t\t\tcase SETPRO1:\n\t\t\t\tqs->setQuestVarById(0, 1);\n'
                      '\t\t\t\tbreak; // Java: a `case ->` arm does not fall through\n', r.cpp)
        self.assertIn('\t\t\tcase SETPRO2:\n\t\t\t\t{\n\t\t\t\t\tqs->setQuestVarById(0, 2);\n\t\t\t\t\treturn true;\n'
                      '\t\t\t\t}\n\t\t\tdefault:\n', r.cpp)
        self.assertEqual(r.cpp.count('break; // Java: a `case ->` arm does not fall through'), 2)
        self.assertIn('\t\tswitch (env.getTargetId()) { // Java: a switch expression\n\t\t\tcase 123:\n\t\t\t\treturn true;\n\t\t\tdefault:\n'
                      '\t\t\t\treturn false;\n\t\t}\n', r.cpp)
        self.assertEqual(parity.compare(SWITCHES, r.cpp), [])

    def test_switch_expressions_it_refuses(self):
        r = Synthetic('test/_99004Switches.java', SWITCHES_REFUSED).r
        self.assertEqual(r.status, 'refused')
        details = [d for c, d in r.reasons if c == 'switch-expression']
        self.assertIn('a switch expression inside a larger expression', details)
        self.assertIn('a switch expression without a default arm', details)        # C++ has no MatchException for an unnamed value
        self.assertTrue(any('yield' in d for d in details), details)

    def test_a_switch_expression_on_a_string_is_refused(self):
        # a C++ switch takes an integral or enum subject only
        r = Synthetic('test/_99010Switches.java', STRING_SWITCH).r
        self.assertEqual(r.reasons, [('type', 'switch on string:std::string_view @ line 25')])

    def test_varargs_array_kept_when_it_is_not_a_single_constant_use(self):
        r = Synthetic('test/_99005Varargs.java', VARARGS_KEPT).r
        self.assertEqual(r.status, 'refused')
        self.assertEqual([c for c, _d in r.reasons], ['varargs-array', 'varargs-array'])   # a second use; a non-constant element
        # a second use after the call: the array stays a local, and the call is refused where it passes it (not by the safety net)
        r = Synthetic('test/_99011Varargs.java', VARARGS_USED_AFTER).r
        self.assertEqual(r.reasons, [('varargs-array', 'AbstractQuestHandler.defaultOnLevelChangedEvent: an int[] passed to Java varargs; '
                                                       'the C++ parameter is std::initializer_list')])

    def test_an_inlined_array_used_elsewhere_refuses_the_method(self):
        # the safety net behind inline_varargs: if it ever let a second use through, the dropped local must not reach the C++
        tr = transliterator(RULES)
        tr.inline_varargs = lambda name, init, ct: True
        try:
            r = Synthetic('test/_99005Varargs.java', VARARGS_KEPT).r
        finally:
            del tr.inline_varargs
        self.assertIn(('varargs-array', 'onQuestCompletedEvent: an int[] kept for a varargs call is used elsewhere'), r.reasons)
        self.assertNotIn(emit.INLINE_MARK, r.cpp)

    def test_work_items_accessors(self):
        r = Synthetic('test/_99006WorkItems.java', WORK_ITEMS).r
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('int32_t last = workItems->size() - 1;', r.cpp)
        self.assertIn('if (workItems->isEmpty() || last > 2)', r.cpp)
        self.assertIn('return giveQuestItem(env, workItems->get(last)->getItemId(), workItems->get(0)->getCount());', r.cpp)
        self.assertEqual(parity.compare(WORK_ITEMS, r.cpp), [])
        r = Synthetic('test/_99007WorkItems.java', WORK_ITEMS_REFUSED).r
        self.assertEqual(r.reason_keys(), ['api-missing: AbstractQuestHandler.workItems.remove'])

    def test_nested_array_locals_fields_and_limits(self):
        r = Synthetic('test/_99008Arrays.java', ARRAYS).r
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('static constexpr std::array<std::array<int32_t, 2>, 3> ROWS{{{1, 2}, {3, 4}, {5, 6}}};', r.cpp)
        self.assertIn('std::array<std::string_view, 2> names{"a", "b"};', r.cpp)
        self.assertIn('std::array<std::array<int32_t, 2>, 2> local{{{1, 2}, {3, 4}}};', r.cpp)
        self.assertIn('return names[1] == "b" && local[1][0] == 3 && ROWS.at(env.getDialogActionId())[1] == 2;', r.cpp)
        self.assertEqual(parity.compare(ARRAYS, r.cpp), [])
        r = Synthetic('test/_99009Arrays.java', JAGGED).r
        self.assertEqual(r.reason_keys(), ['type: int[][] with rows of [1, 2] elements (std::array needs one length)'])


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class Driver(unittest.TestCase):
    def test_the_driver_turns_the_rules_on_and_prototype_rules_off(self):
        rels = [rel for rels in UNBLOCKED.values() for rel in rels]
        files = [QUEST / rel for rel in rels]
        results, rep = cli.run(files, pairs=False)
        self.assertEqual(rep['rules'], sorted(emit.ALL_RULES))     # the P6-T rules and, since the G1 lane, emit.G1_RULES
        self.assertEqual(rep['coverage']['transliterated'], len(rels))
        results, rep = cli.run(files, pairs=False, rules=frozenset())
        self.assertEqual((rep['rules'], rep['coverage']['transliterated']), ([], 0))
        with contextlib.redirect_stdout(io.StringIO()) as out:
            cli.main(['--dry-run', '--no-pairs', '--only', 'poeta/_1005BarringtheGate.java'])
        self.assertIn('1 of 1 transliterated', out.getvalue())
        self.assertIn('emitter rules: nested-array, scheduled-closure, switch-expression, varargs-inline, work-items', out.getvalue())
        with contextlib.redirect_stdout(io.StringIO()) as out:
            cli.main(['--dry-run', '--no-pairs', '--prototype-rules', '--only', 'poeta/_1005BarringtheGate.java'])
        self.assertIn('0 of 1 transliterated', out.getvalue())
        self.assertIn('emitter rules: the prototype (rev 2) only', out.getvalue())


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class Corpus(unittest.TestCase):
    """all 1,035 handlers, with and without the rules (about 20 s)"""

    @classmethod
    def setUpClass(cls):
        files = cli.find_files()
        cls.p6t = {r.rel: r for r in (transliterator(RULES).transliterate(f) for f in files)}
        cls.proto = {r.rel: r for r in (transliterator(frozenset()).transliterate(f) for f in files)}

    def test_every_transliterated_file_is_at_parity_with_its_java(self):
        # phase6-inventory.md §7.6 items 1 and 4: literal multisets and the ordered literals, calls, operators and constants
        ok = [r for r in self.p6t.values() if r.status == 'ok']
        self.assertGreaterEqual(len(ok), 935)       # 929, and the six escorts of rows B26-B30 (2026-09-30, tests.test_questgen_escorts)
        bad = {}
        for r in ok:
            ms = parity.compare((QUEST / r.rel).read_text(encoding='utf-8-sig'), r.cpp)
            if ms:
                bad[r.rel] = [str(m) for m in ms][:2]
        self.assertEqual(bad, {})

    def test_the_rules_only_add_files(self):
        added = sorted(rel for rel, r in self.p6t.items() if r.status == 'ok' and self.proto[rel].status != 'ok')
        # and two files of the G1 lane's rows that need a P6-T rule as well (2026-10-04, tests.test_questgen_g1: B37 and a switch rule in
        # _24030, B32 and a varargs array in _2007)
        self.assertEqual(added, sorted([rel for rels in UNBLOCKED.values() for rel in rels]
                                       + ['clash_of_destiny/_24030ShowdownWithDestiny.java', 'ishalgen/_2007WheresRaeThisTime.java']))
        for rel, r in self.proto.items():
            if r.status == 'ok':
                with self.subTest(rel=rel):
                    self.assertEqual(self.p6t[rel].cpp, r.cpp)                   # byte for byte the prototype's output
        # the prototype's 910 and the six escorts rows B26-B30 admit (2026-09-30): API rows are not P6-T rules, so both sets gain them; and
        # six of the ten files of the G1 lane's rows B32-B38 (2026-10-04, tests.test_questgen_g1; two need a P6-T rule, two rule
        # scheduled-closure)
        # row B39 (the Q08 follow-up, lane C, 2026-10-05) adds gelkmaros/_20034
        self.assertEqual(sum(1 for r in self.proto.values() if r.status == 'ok'), 923)

    def test_the_oracle_slice_is_tier_a(self):
        # tools/oracle/questtrace/extract.py SLICE: the Poeta and Ishalgen files questgen emits in tier A (phase6-inventory.md §9.3 item 1)
        tree = ast.parse((paths.CPP_ROOT / 'tools' / 'oracle' / 'questtrace' / 'extract.py').read_text(encoding='utf-8'))
        slice_ = next(ast.literal_eval(n.value) for n in tree.body
                      if isinstance(n, ast.Assign) and getattr(n.targets[0], 'id', '') == 'SLICE_TIER_A')
        expected = sorted(rel for rel, r in self.p6t.items() if rel.startswith(('poeta/', 'ishalgen/')) and r.status == 'ok' and r.tier == 'A')
        self.assertEqual(sorted(slice_), expected)


if __name__ == '__main__':
    unittest.main()
