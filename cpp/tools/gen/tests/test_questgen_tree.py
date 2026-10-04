"""The generated quest handlers committed to the handler tree (P6-Q ascension route, 2026-09-29, lane route-gen).

Every .cpp below cpp/game-server/handlers/aion/gameserver/handlers/quest that carries questgen's banner (emit.banner) is regenerated from the
Java file its banner names, with the driver's rules (emit.ALL_RULES), and must equal the committed file byte for byte: a hand edit, a stale
file after a generator change or a file emitted with other rules shows up here. The route's files are pinned by name, so a file that loses
its banner or goes missing fails too. Nothing is compiled; the C++ side is game-server/tests/quest_handlers_golden.
"""
from __future__ import annotations

import os
import re
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from questgen import emit, paths  # noqa: E402

HANDLER_QUEST_DIR = paths.CPP_GAME_SERVER / 'handlers' / 'aion' / 'gameserver' / 'handlers' / 'quest'
HAVE_JAVA = paths.JAVA_QUEST_DIR.is_dir()

# the ascension route's generated files in the tree (docs/deviations/Q05.md, Q09.md, Q06.md "Generated dispatches"): poeta 11, ishalgen 13,
# ascension 12
ROUTE = {
    'poeta': ('_1000Prologue', '_1001TheKerubThreat', '_1003IllegalLogging', '_1004NeutralizingOdium', '_1005BarringtheGate',
              '_1100KaliosCall', '_1107TheLostAxe', '_1111InsomniaMedicine', '_1122DeliveringPernossRobe', '_1123WheresTutty',
              '_1205ANewSkill'),
    'ishalgen': ('_2000Prologue', '_2001ThinkingAhead', '_2003TreasureOfTheDeceased', '_2005TeachingaLesson', '_2006HitThemWhereitHurts',
                 '_2100OrderoftheCaptain', '_2106VanarsFlattery', '_2114TheInsectProblem', '_2122AshesToAshes',
                 '_2123TheImprisonedGourmet', '_2125TheRobberyPlot', '_2132ANewSkill', '_2135ForLoveofNegi'),
    'ascension': ('_1913DispatchtoVerteron', '_1914DispatchtoVerteron', '_1915DispatchtoVerteron', '_1916DispatchtoVerteron',
                  '_19070ADispatchtoVerteron', '_19071ADispatchtoVerteron', '_2901DispatchtoAltgard', '_2902DispatchtoAltgard',
                  '_2903DispatchtoAltgard', '_2904DispatchtoAltgard', '_29070ADispatchtoAltgard', '_29071ADispatchtoAltgard'),
}
# P6-Q slice 2, chunk Q10 (docs/deviations/Q10.md): the altgard and pandaemonium files questgen transliterates that are in the tree (18 + 35;
# questgen transliterates 28 + 41 of the 75, and the other six are hand ports, or held back, and carry no banner)
Q10 = {
    'altgard': (
        '_2216MuMuGrassKnot', '_2222ManirsMessage', '_2228AThornInItsSide', '_2247TheGergersDisguise', '_2263ShugoPotion',
        '_2266ATrustworthyMessenger', '_2271AurtrisLetter', '_2278ASecretProposal', '_2279SolidProof', '_2284EscapingAsmodae',
        '_2289RampagingMosbears', '_2290GrokensEscape', '_24011FunnyFloatingFungus', '_24012AnOminousCrop', '_24014StompOutThePlot',
        '_24015TotemPlowed', '_24016AStrangeNewThread', '_24112NoLaissezFaireForLepharists'),
    'pandaemonium': (
        '_2912FollowtheRibbon', '_2913AChainofDebt', '_2914ATokenofLostLove', '_2916ManInTheLongBlackRobe', '_2918DeepMaternalLove',
        '_2919BookOfOblivion', '_2920ElementaryMyDearDaeva', '_2921LoveAtFirstSight', '_2922FascinatingGift', '_2925AHeartfeltConfession',
        '_2928PowerofLove', '_2937UnexpectedReward', '_2938SecretLibraryAccess', '_2948HuronsLetter', '_2952WinningVindachinerksFavor',
        '_2954DeliveringOdellaJuice', '_2957FlowersForTheBanquet', '_2958LastMinuteWorries', '_2962JafnharWhereabouts', '_2963OnBehalfOfAFriend',
        '_2965AncientWeapons', '_2985AnExpertsReward', '_4210MissingHaorunerk', '_4905InterviewingTheVeterans', '_4906TalesOfHeroes',
        '_4920MakingTheActivatedSurkana', '_4966GrowthNinissFirstCharm', '_4967GrowthNinissSecondCharm', '_4968GrowthNinissThirdCharm',
        '_4969GrowthNinissFourthCharm', '_4970TheFashionistas', '_4971ProjectRunway', '_4972JudgeNot', '_4974TheSecretOfHisSuccess',
        '_4976ASettlerAmbition'),
}
# held back at the integration of slice 2 (docs/deviations/Q10.md, "Held back"): transliterated like the others, but kept out of the
# tree because gs.scenario.travel's (and gs.scenario.ascension's) Asmodian would see them: 24010's onEnterWorldEvent starts it at the
# Altgard arrival, and the others' start npcs put them in his SM_NEARBY_QUESTS in Pandaemonium or Altgard
Q10_HELD_BACK = (
    'altgard/_2207ConversingWithaSkurv.java', 'altgard/_2209TheScribbler.java', 'altgard/_2213PoisonRootPotentFruit.java',
    'altgard/_2221ManirsUncle.java', 'altgard/_2223AMythicalMonster.java', 'altgard/_2231SiblingRivalry.java', 'altgard/_2232TheBrokenHoneyJar.java',
    'altgard/_2239MalodorAntidote.java', 'altgard/_2288MoneyWhereYourMouthIs.java', 'altgard/_24010SuthransOrders.java',
    'pandaemonium/_2911SongOfBlessing.java', 'pandaemonium/_2917ArekedilsHeritage.java', 'pandaemonium/_2953DeliveringSupplyRequest.java',
    'pandaemonium/_29004VeldinaCall.java', 'pandaemonium/_29048SeriphimTeachings.java', 'pandaemonium/_4973MarraWorry.java',
)
# the four that start their quest at a character's first enter world (Java behaviour): held back for their gate impact until the owner's answer
# of 2026-09-29 ("A": land them and let gs.scenario.m5a, m5b and m5b2 expect the prologue traffic; docs/design/owner-decisions.md)
ENTER_WORLD = ('poeta/_1000Prologue.java', 'poeta/_1100KaliosCall.java', 'ishalgen/_2000Prologue.java', 'ishalgen/_2100OrderoftheCaptain.java')


def generated_files():
    """(tree file, Java file relative to data/handlers/quest) for every tree file with questgen's banner"""
    out = []
    for f in sorted(HANDLER_QUEST_DIR.rglob('*.cpp')):
        for line in f.read_text(encoding='utf-8').split('\n'):
            if line.startswith(emit.BANNER_FIRST_LINE):
                java = line[len(emit.BANNER_FIRST_LINE):].rstrip('.')
                out.append((f, java.removeprefix('game-server/data/handlers/quest/')))
                break
    return out


class Banner(unittest.TestCase):
    def test_the_banner_says_generated_and_compiled(self):
        lines = emit.banner('game-server/data/handlers/quest/poeta/_1001TheKerubThreat.java')
        self.assertEqual(lines[0], '// Generated by cpp/tools/gen/questgen from game-server/data/handlers/quest/poeta/_1001TheKerubThreat.java.')
        self.assertIn("Compiled into its Q chunk's handler library", lines[1])
        self.assertTrue(all(len(ln) <= 150 for ln in lines))          # .clang-format ColumnLimit
        self.assertNotIn('Not compiled', '\n'.join(lines))
        self.assertNotIn('(prototype)', '\n'.join(lines))


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class CommittedTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = generated_files()
        cls.tr = emit.Transliterator(rules=emit.ALL_RULES)

    def test_the_route_files_are_committed_with_the_banner(self):
        found = {(f.parent.name, f.stem) for f, _ in self.files}
        for directory, classes in ROUTE.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual(sum(len(v) for v in ROUTE.values()), 36)
        for directory, classes in Q10.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual((len(Q10['altgard']), len(Q10['pandaemonium'])), (18, 35))
        self.assertEqual(len(Q10_HELD_BACK), 16)

    def test_the_enter_world_files_are_in_the_tree(self):
        for rel in ENTER_WORLD:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertIn('qe.registerOnEnterWorld(questId);', r.cpp)
                self.assertEqual((HANDLER_QUEST_DIR / rel).with_suffix('.cpp').read_bytes(), r.cpp.encode('utf-8'))

    def test_the_q10_held_back_files_transliterate_and_stay_out_of_the_tree(self):
        # the integration of slice 2: each is questgen's output (the regenerate command lands it), none is in the tree or the table
        found = {(f.parent.name, f.stem) for f, _ in self.files}
        for rel in Q10_HELD_BACK:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertFalse((HANDLER_QUEST_DIR / rel).with_suffix('.cpp').exists())
                directory, klass = rel.removesuffix('.java').split('/')
                self.assertNotIn((directory, klass), found)
                self.assertNotIn(klass, Q10[directory])
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_24010SuthransOrders.java')
        self.assertIn('qe.registerOnEnterWorld(questId);', r.cpp)

    def test_the_route_java_bugs_are_kept_and_marked(self):
        # phase6-inventory.md §11, the rows P6-Q added (the route-gen review): the marker sits right before the statement of the Java line
        # KNOWN_JAVA_BUGS names, and the code is kept as Java wrote it
        cases = (('poeta/_1004NeutralizingOdium.java', 69, 'var 4',
                  'else if (targetId == 700030 && var == 1 || var == 4) { // The Cauldron\n\t\t\t\t',
                  '\t\t\t\tswitch (dialogActionId) {\n'),
                 ('poeta/_1111InsomniaMedicine.java', 59, 'null-checked',
                  'else if (targetId == 203061) {\n\t\t\t',
                  '\t\t\tif (env.getDialogActionId() == QUEST_SELECT) {\n\t\t\t\tif (qs->getQuestVarById(0) == 0)'))
        for rel, line, words, before, after in cases:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertEqual(len(r.java_bugs), 1)
                self.assertTrue(r.java_bugs[0].startswith(f'line {line}: ') and words in r.java_bugs[0], r.java_bugs)
                self.assertRegex(r.cpp, re.escape(before + emit.JAVA_BUG_MARK) + '[^\n]*\n' + re.escape(after))
                self.assertTrue(all(len(ln.expandtabs(2)) <= 150 for ln in r.cpp.split('\n') if emit.JAVA_BUG_MARK in ln))

    def test_the_slice_two_include_rules(self):
        # P6-Q slice 2 (Q10, docs/deviations/Q10.md "Generator changes"): the compile fixes of the altgard files. A Ref result the Java discards
        # is still destroyed by the caller, which needs the complete type (api.OWNING_RETURN_HEADERS); a pointer dereferenced for a T& parameter
        # needs its class's header (emit.convert)
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2213PoisonRootPotentFruit.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tSkillEngine::getInstance().applyEffectDirectly(255, *player, *player);', r.cpp)
        self.assertIn('#include "aion/gameserver/skillengine/model/Effect.h"\n', r.cpp)
        self.assertTrue((paths.CPP_GAME_SERVER / 'src' / 'aion/gameserver/skillengine/model/Effect.h').is_file())
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2223AMythicalMonster.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('spawnForFiveMinutes(211621, *env.getPlayer()->getWorldMapInstance(), ', r.cpp)
        self.assertIn('#include "aion/gameserver/world/WorldMapInstance.h"\n', r.cpp)
        # neither rule adds an include a file does not need: 2207 dereferences nothing and calls nothing that returns a Ref
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2207ConversingWithaSkurv.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertNotIn('Effect.h', r.cpp)
        self.assertNotIn('WorldMapInstance.h', r.cpp)

    def test_every_generated_file_regenerates_byte_for_byte(self):
        self.assertTrue(self.files)
        for f, rel in self.files:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertEqual(f.read_bytes(), r.cpp.encode('utf-8'), f'{f} differs from questgen output: regenerate it')
                self.assertTrue(r.cpp.rstrip('\n').endswith(f'}} // namespace aion::gameserver::handlers::quest::{f.parent.name}'))
                self.assertIn(f'AION_QUEST_HANDLER({f.stem}, ', r.cpp)


if __name__ == '__main__':
    unittest.main()
