"""questgen's G1-lane rules (docs/design/phase6-transliterator.md §2; emit.G1_RULES and API rows B31-B38): the closure parser of jast (opt in;
tools/oracle's extractor opts in too since lane C, phase6-transliterator.md §7), the scheduled-closure rule on the real handlers it exists
for and on synthetic ones (the captures lint L5 accepts, the refusals), the rows B32-B38 and the configuration flag rule on their real
files, the driver's default rule set, the whole corpus (the rules only add files, every transliterated file at parity), compilecheck's
diagnostics parser, and, where MSVC and the vcpkg headers are found, compilecheck itself on two emitted files (cl with a Q chunk's flags and
the prelude PCH, about 10 s; AION_QUESTGEN_COMPILE_CHECK=0 skips it).

Run from cpp/tools/gen: python -m unittest tests.test_questgen_g1
"""
from __future__ import annotations

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'parity'))
sys.dont_write_bytecode = True

import parity  # noqa: E402  (tools/parity)
from questgen import cli, compilecheck, emit, jast, paths  # noqa: E402
from tests.test_questgen_p6t import Synthetic, parse_body, source, transliterator  # noqa: E402

QUEST = paths.JAVA_QUEST_DIR
HAVE_JAVA = QUEST.is_dir()
ALL = emit.ALL_RULES
P6T = emit.P6T_RULES

# the files rule scheduled-closure unblocks (refused as lambda / anonymous-class before; the measurement of phase6-transliterator.md §3)
CLOSURE_FILES = (
    'altgard/_2208MauInTenMinutesADay.java', 'altgard/_24013PoisonInTheWaters.java', 'beluslan/_24051InvesetigatetheDisappearance.java',
    'beluslan/_24052AFrozenCity.java', 'beluslan/_2620SummoningPhagrasul.java', 'beshmundir/_30208GroupTheTruthHurts.java',
    'beshmundir/_30308GroupSummonRespondentUtra.java', 'brusthonin/_4082GatheringtheHerbPouches.java', 'cygnea/_10501ResearchtheRuins.java',
    'cygnea/_10503GuardDownSecretsOut.java', 'cygnea/_10505SneezeAttack.java', 'eltnen/_1361FindingDrinkingWater.java',
    'eltnen/_1466RespectForDeltras.java', 'enshar/_20506MuscleOverMind.java', 'enshar/_25023SproutingDevelopments.java',
    'gelkmaros/_20033DranaSolution.java', 'heiron/_1643TheStarOfHeiron.java', 'inggison/_11031CanIEatIt.java',
    'inggison/_11032EverythingsBetterWithTentacles.java', 'inggison/_11033YouMakeMeSick.java', 'ishalgen/_2002WheresRae.java',
    'ishalgen/_2136TheLostAxe.java', 'morheim/_2393TheLoveOfAFather.java', 'silentera_canyon/_30056DirvisiasSorrow.java',
    'silentera_canyon/_30156NepsLove.java', 'silentera_canyon/_30157VilisMind.java', 'talocs_hollow/_11466AHardSeedtoCrack.java',
)
# the files rows B32-B38 unblock (refused as api-missing for exactly these members before; rows are vocabulary, not rules, so --p6t-rules
# and --prototype-rules gain them too, as rows B26-B30 did)
ROW_FILES = {
    'B32': ('ishalgen/_2007WheresRaeThisTime.java',),
    'B33': ('beluslan/_4200ASuspiciousCall.java', 'heiron/_3200PriceOfGoodwill.java'),
    'B34': ('crafting/_29000ExpertEssencetappersTest.java', 'morheim/_24021GhostsintheDesert.java'),
    'B35': ('inggison/_10032HelpintheHollow.java',),
    'B36': ('ascension/_1007ACeremonyinSanctum.java', 'ascension/_2009ACeremonyinPandaemonium.java'),
    'B37': ('clash_of_destiny/_24030ShowdownWithDestiny.java',),
    'B38': ('morheim/_24022SneakBehindtheIceClaw.java',),
}

ITEM_USE = source('_99101Closures', 99101, '''
	@Override
	public HandlerResult onItemUseEvent(final QuestEnv env, Item item) {
		final Player player = env.getPlayer();
		final int id = item.getItemTemplate().getTemplateId();
		final QuestState qs = player.getQuestStateList().getQuestState(questId);
		ThreadPoolManager.getInstance().schedule(new Runnable() {

			@Override
			public void run() {
				// the use bar ended
				qs.setQuestVarById(0, id);
				updateQuestStatus(env);
				if (qs.getStatus() == QuestStatus.START)
					return;
				player.getInventory().decreaseByItemId(id, 1);
			}
		}, 3000);
		return HandlerResult.SUCCESS;
	}

	@Override
	public boolean onKillEvent(QuestEnv env) {
		int var = 1;
		ThreadPoolManager.getInstance().schedule(() -> env.getPlayer().getQuestStateList().getQuestState(var), 1000);
		ThreadPoolManager.getInstance().schedule(() -> {
			int x = var + 1;
		}, 1000L);
		return true;
	}
''')

REFUSED = {
    'string': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		String zone = "a";
		ThreadPoolManager.getInstance().schedule(() -> {
			zone.equals("b");
		}, 1000);
		return true;
	}
''', 'closure-capture'),
    'params': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		ThreadPoolManager.getInstance().schedule(x -> {
		}, 1000);
		return true;
	}
''', 'lambda'),
    'not-runnable': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		ThreadPoolManager.getInstance().schedule(new Callable() {
			public void call() {
			}
		}, 1000);
		return true;
	}
''', 'anonymous-class'),
    'this-in-anonymous': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		ThreadPoolManager.getInstance().schedule(new Runnable() {
			public void run() {
				this.run();
			}
		}, 1000);
		return true;
	}
''', 'anonymous-class'),
    'two-members': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		ThreadPoolManager.getInstance().schedule(new Runnable() {
			int n = 1;
			public void run() {
			}
		}, 1000);
		return true;
	}
''', 'anonymous-class'),
    'elsewhere': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		Runnable r = () -> {
		};
		return true;
	}
''', 'lambda'),
    'behind-a-refused-statement': ('''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		if (env.getPlayer().isUnknownMember())
			return env.getPlayer().getKnownList().stream().anyMatch(o -> o == null);
		return true;
	}
''', 'lambda'),
}

CONFIG = source('_99102Config', 99102, '''
	@Override
	public void register() {
		if (CustomConfig.ENABLE_SIMPLE_2NDCLASS)
			return;
		qe.registerQuestNpc(123).addOnTalkEvent(questId);
	}

	@Override
	public boolean onKillEvent(QuestEnv env) {
		return GroupConfig.GROUP_MAX_DISTANCE > 10;
	}
''')
CONFIG_UNKNOWN = source('_99103Config', 99103, '''
	@Override
	public boolean onKillEvent(QuestEnv env) {
		return CustomConfig.NO_SUCH_FLAG;
	}
''')


class ClosureParser(unittest.TestCase):
    def test_closures_are_opt_in(self):
        with self.assertRaises(jast.Unsupported) as e:
            parse_body('f(() -> { g(); });')
        self.assertEqual(e.exception.category, 'lambda')
        with self.assertRaises(jast.Unsupported) as e:
            parse_body('f(new Runnable() { public void run() { g(); } });')
        self.assertEqual(e.exception.category, 'anonymous-class')

    def test_lambda_and_anonymous_runnable_nodes(self):
        st = parse_body('f(() -> { g(); }, x -> x + 1, (a, b) -> a, new Runnable() { @Override public void run() { h(); } });', closures=True)
        args = st[0].expr.args
        self.assertEqual([type(a).__name__ for a in args], ['Closure'] * 4)
        self.assertEqual((args[0].kind, args[0].params, len(args[0].body)), ('lambda', [], 1))
        self.assertEqual((args[1].params, args[1].body, type(args[1].expr).__name__), (['x'], None, 'Binary'))
        self.assertEqual(args[2].params, ['a', 'b'])
        self.assertEqual((args[3].kind, args[3].iface, args[3].method, len(args[3].body)), ('anonymous-class', 'Runnable', 'run', 1))

    def test_what_the_closure_parser_still_refuses(self):
        for src, cat in (('f((int a) -> a);', 'lambda'), ('f(new Runnable() { int n; public void run() {} });', 'anonymous-class'),
                         ('f(new Runnable() { public void run(int a) {} });', 'anonymous-class'),
                         ('f(new Thread(r) { public void run() {} });', 'anonymous-class')):
            with self.subTest(src=src), self.assertRaises(jast.Unsupported) as e:
                parse_body(src, closures=True)
            self.assertEqual(e.exception.category, cat)

    def test_a_switch_rule_label_is_still_not_a_lambda(self):
        st = parse_body('switch (a) { case X -> f(); default -> g(); }', switch_expressions=True, closures=True)
        self.assertTrue(st[0].rules)


class ScheduledClosure(unittest.TestCase):
    def test_captures_and_pins(self):
        r = Synthetic('test/_99101Closures.java', ITEM_USE, rules=ALL).r
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('B31', r.api_rows)
        self.assertIn('#include "aion/gameserver/utils/ThreadPoolManager.h"', r.cpp)
        # the hook's QuestEnv& is captured by reference and pinned, the Ptr locals become Ref init-captures, the int is copied, `this` is
        # captured because the body calls updateQuestStatus, and pinned always (an Immortal)
        self.assertIn('\t\tThreadPoolManager::getInstance().schedule({this, &env}, [this, qs = runtime::Ref<QuestState>(qs), id, &env, '
                      'player = runtime::Ref<Player>(player)] {\n', r.cpp)
        self.assertIn('\t\t\t// the use bar ended\n\t\t\tqs->setQuestVarById(0, id);\n', r.cpp)
        self.assertIn('\t\t\tif (qs->getStatus() == QuestStatus::START)\n\t\t\t\treturn;\n', r.cpp)    # run()'s return is the lambda's
        self.assertIn('\t\t\tplayer->getInventory().decreaseByItemId(id, 1);\n\t\t}, 3000);\n', r.cpp)
        # an expression lambda is a statement; a lambda that names no member captures no `this`, but still pins it (lint L5: a pinned
        # lambda may capture copies only)
        self.assertIn('schedule({this, &env}, [&env, var] {\n\t\t\tenv.getPlayer()->getQuestStateList()->getQuestState(var);\n\t\t}, 1000);',
                      r.cpp)
        self.assertIn('schedule({this}, [var] {\n\t\t\t[[maybe_unused]] int32_t x = var + 1;\n\t\t}, 1000LL);', r.cpp)
        self.assertEqual(r.idioms['scheduled anonymous Runnable as a pinned C++ lambda'], 1)
        self.assertEqual(r.idioms['scheduled lambda as a pinned C++ lambda'], 2)
        self.assertEqual(parity.compare(ITEM_USE, r.cpp), [])

    def test_without_the_rule_the_file_is_refused_as_before(self):
        r = Synthetic('test/_99101Closures.java', ITEM_USE, rules=P6T).r
        self.assertEqual(r.status, 'refused')
        self.assertEqual(r.primary, 'lambda')            # onKillEvent's lambdas; onItemUseEvent's Runnable is refused as well
        self.assertIn('anonymous-class', [c for c, _d in r.reasons])

    def test_refusals(self):
        for name, (body, cat) in REFUSED.items():
            with self.subTest(case=name):
                r = Synthetic('test/_99104Refused.java', source('_99104Refused', 99104, body), rules=ALL).r
                self.assertEqual(r.status, 'refused')
                self.assertIn(cat, [c for c, _d in r.reasons], r.reasons)


class ConfigFlags(unittest.TestCase):
    def test_flags_are_the_static_atomics_read_implicitly(self):
        r = Synthetic('test/_99102Config.java', CONFIG, rules=ALL).r
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tif (CustomConfig::ENABLE_SIMPLE_2NDCLASS)\n\t\t\treturn;\n', r.cpp)
        self.assertIn('return GroupConfig::GROUP_MAX_DISTANCE > 10;', r.cpp)
        self.assertIn('#include "aion/gameserver/configs/main/CustomConfig.h"', r.cpp)
        self.assertIn('#include "aion/gameserver/configs/main/GroupConfig.h"', r.cpp)
        self.assertEqual(r.api_rows, {'B36'})

    def test_an_unknown_flag_is_an_api_gap(self):
        r = Synthetic('test/_99103Config.java', CONFIG_UNKNOWN, rules=ALL).r
        self.assertEqual(r.reason_keys(), ['api-missing: CustomConfig.NO_SUCH_FLAG'])


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class RealFiles(unittest.TestCase):
    def test_the_closure_files_need_the_rule(self):
        for rel in CLOSURE_FILES:
            with self.subTest(file=rel):
                on = transliterator(ALL).transliterate(QUEST / rel)
                off = transliterator(P6T).transliterate(QUEST / rel)
                self.assertEqual(on.status, 'ok', on.reasons)
                self.assertIn('B31', on.api_rows)
                self.assertEqual(off.status, 'refused')
                self.assertIn(off.primary, ('lambda', 'anonymous-class'))

    def test_each_row_unblocks_its_files(self):
        for rid, rels in ROW_FILES.items():
            for rel in rels:
                with self.subTest(row=rid, file=rel):
                    on = transliterator(ALL).transliterate(QUEST / rel)
                    self.assertEqual(on.status, 'ok', on.reasons)
                    self.assertIn(rid, on.api_rows)

    def test_the_driver_runs_every_rule(self):
        with contextlib.redirect_stdout(io.StringIO()) as out:
            cli.main(['--dry-run', '--no-pairs', '--only', 'eltnen/_1361FindingDrinkingWater.java'])
        self.assertIn('1 of 1 transliterated', out.getvalue())
        self.assertIn('scheduled-closure', out.getvalue())
        with contextlib.redirect_stdout(io.StringIO()) as out:
            cli.main(['--dry-run', '--no-pairs', '--p6t-rules', '--only', 'eltnen/_1361FindingDrinkingWater.java'])
        self.assertIn('0 of 1 transliterated', out.getvalue())


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class Corpus(unittest.TestCase):
    """all 1,035 handlers with the driver's rules and with the P6-T rules only (about 25 s)"""

    @classmethod
    def setUpClass(cls):
        files = cli.find_files()
        cls.all = {r.rel: r for r in (transliterator(ALL).transliterate(f) for f in files)}
        cls.p6t = {r.rel: r for r in (transliterator(P6T).transliterate(f) for f in files)}

    def test_the_g1_rules_only_add_files(self):
        added = sorted(rel for rel, r in self.all.items() if r.status == 'ok' and self.p6t[rel].status != 'ok')
        # the closure files, and the two files of row B33 whose use-bar Runnable needs the rule as well
        self.assertEqual(added, sorted(CLOSURE_FILES + ROW_FILES['B33']))
        for rel, r in self.p6t.items():
            if r.status == 'ok':
                with self.subTest(rel=rel):
                    self.assertEqual(self.all[rel].cpp, r.cpp)
        # the P6-T output (935 files on 2026-09-30) and eight of the ten files of rows B32-B38; with rule scheduled-closure the 27 closure
        # files and the other two (B33)
        self.assertEqual(sum(1 for r in self.p6t.values() if r.status == 'ok'), 943)
        self.assertEqual(sum(1 for r in self.all.values() if r.status == 'ok'), 972)
        for rid, rels in ROW_FILES.items():
            for rel in rels:
                self.assertIn(rid, self.all[rel].api_rows)

    def test_every_transliterated_file_is_at_parity_with_its_java(self):
        bad = {}
        for r in self.all.values():
            if r.status == 'ok':
                ms = parity.compare((QUEST / r.rel).read_text(encoding='utf-8-sig'), r.cpp)
                if ms:
                    bad[r.rel] = [str(m) for m in ms][:2]
        self.assertEqual(bad, {})

    def test_the_closures_left_are_the_misplaced_ones(self):
        # with the rule, a closure refuses a file only where it is not the task of a schedule call (the mentor dailies' anyMatch, forEachNpc,
        # findObject, Arrays.stream): 22 files; the scheduled closures of the other refused files are transliterated and checked, so those
        # files now list the API gaps the parser's refusal used to hide (phase6-questgen-prototype.md §4.1, "reason lists are lower bounds")
        lam = {rel for rel, r in self.all.items() if r.status != 'ok' and {'lambda', 'anonymous-class'} & {c for c, _d in r.reasons}}
        self.assertEqual(len(lam), 22)
        for rel in lam:
            for c, d in self.all[rel].reasons:
                if c in ('lambda', 'anonymous-class'):
                    self.assertIn('a closure outside ThreadPoolManager.getInstance().schedule', d, rel)


class CompileCheckParser(unittest.TestCase):
    def test_diagnostics(self):
        out = ('_1361FindingDrinkingWater.cpp\n'
               r'C:\s\src\aion\x\_1.cpp(70,12): error C2039: ' + "'getStatusX': is not a member of 'QuestState'\n"
               r'C:\s\src\aion\x\_1.cpp(71): warning C4189: ' + "'x': local variable is initialized but not referenced\n"
               r'D:\h\Ref.h(10): fatal error C1083: Cannot open include file' + "\n")
        d = compilecheck.parse_diagnostics(out)
        self.assertEqual([(x['kind'], x['code'], x['line']) for x in d], [('error', 'C2039', 70), ('warning', 'C4189', 71),
                                                                           ('fatal error', 'C1083', 10)])
        self.assertEqual(compilecheck.first_error_class({'errors': d[:1]}), "C2039: '…': is not a member of '…'")
        self.assertEqual(compilecheck.first_error_class({'errors': []}), 'no diagnostic (exit code)')

    def test_the_stage_must_lie_outside_the_repository(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()) as err:
            compilecheck.main(['--stage', str(paths.CPP_ROOT / 'build' / 'qg-stage'), '--only', '_1361FindingDrinkingWater'])
        self.assertIn('inside the repository', err.getvalue())
        self.assertFalse((paths.CPP_ROOT / 'build' / 'qg-stage').exists())


def have_msvc():
    try:
        compilecheck.find_vcvars()
        compilecheck.vcpkg_include()
        return True
    except SystemExit:
        return False


@unittest.skipUnless(HAVE_JAVA and have_msvc() and os.environ.get('AION_QUESTGEN_COMPILE_CHECK') != '0',
                     'needs MSVC (vcvars64.bat) and vcpkg headers; AION_QUESTGEN_COMPILE_CHECK=0 turns it off')
class CompileCheck(unittest.TestCase):
    def test_a_closure_file_and_a_config_file_compile_clean(self):
        with tempfile.TemporaryDirectory(prefix='qgcc') as stage, contextlib.redirect_stdout(io.StringIO()) as out:
            rc = compilecheck.main(['--stage', stage, '--jobs', '2', '--only', 'eltnen/_1361FindingDrinkingWater.java',
                                    'ascension/_1007ACeremonyinSanctum.java'])
        self.assertEqual(rc, 0, out.getvalue())
        self.assertIn('clean 2, warnings 0, errors 0', out.getvalue())


if __name__ == '__main__':
    unittest.main()
