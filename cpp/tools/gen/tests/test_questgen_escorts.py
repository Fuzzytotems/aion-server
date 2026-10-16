"""questgen's API rows B26-B30 (2026-09-30): the six escort handlers M5d stage 3 found refused only for want of rows (m5d-plan.md §21.1,
"The other six"), which call QuestTasks themselves instead of defaultStartFollowEvent: beluslan/_24053 and _2634, morheim/_2333 and _2394,
pandaemonium/_4212, sanctum/_3212. B26 npc AI events (Creature.getAi, AbstractAI.onCreatureEvent), B27 creature tasks
(CreatureController.addTask), B28 escort checks (QuestTasks.newFollowingToTargetCheckTask), B29 walking (WalkManager.startWalking,
VisibleObject.getSpawn, SpawnTemplate.setWalkerId), B30 the npc info packet (new SM_NPC_INFO), and the overload fit setWalkerId needs: a
Java String for C++'s nullable String, std::optional<std::string_view>. Each row on a synthetic handler that calls it beside the core
vocabulary, then the six real files: transliterated with exactly their rows, at parity with their Java, and not in the handler tree (their
follower's AI "following", FollowingNpcAI, has no C++ file: m5d-plan.md §21.1). Nothing is compiled here; the six were compile-checked
against this tree's headers when the rows were added (phase6-questgen-prototype.md, the note of 2026-09-30).

Run from cpp/tools/gen: python -m unittest tests.test_questgen_escorts
"""
from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'parity'))

import parity  # noqa: E402  (tools/parity)
from questgen import api, cli, emit, paths  # noqa: E402

from tests.test_questgen_p6t import RULES, Synthetic, source, transliterator  # noqa: E402

QUEST = paths.JAVA_QUEST_DIR
HAVE_JAVA = QUEST.is_dir()
HANDLER_QUEST_DIR = paths.CPP_GAME_SERVER / 'handlers' / 'aion' / 'gameserver' / 'handlers' / 'quest'

# the six and the API-table rows each one uses (B02 spawnInFrontOf, B03 defaultFollowEndEvent, B13 getController and delete, B16 and B18
# the CHANGE_SPEED emotion broadcast)
SIX = {
    'beluslan/_24053TheMaulingoftheMau.java': {'B02', 'B03', 'B13', 'B26', 'B27', 'B28'},
    'beluslan/_2634TheDraupnirRedemption.java': {'B02', 'B03', 'B13', 'B26', 'B27', 'B28', 'B30'},
    'morheim/_2333ARibbitOutOfWater.java': {'B02', 'B03', 'B13', 'B16', 'B18', 'B26', 'B27', 'B28', 'B29'},
    'morheim/_2394ADyingWish.java': {'B02', 'B03', 'B13', 'B16', 'B18', 'B26', 'B27', 'B28', 'B29'},
    'pandaemonium/_4212MissingSidrunerk.java': {'B03', 'B13', 'B16', 'B18', 'B26', 'B27', 'B28', 'B29'},
    'sanctum/_3212TheMissingCubeCraftsman.java': {'B03', 'B13', 'B16', 'B18', 'B26', 'B27', 'B28', 'B29'},
}

BODY = '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		Player player = env.getPlayer();
		Npc npc = (Npc) env.getVisibleObject();
%s
		return false;
	}
'''
# the last line of the prologue BODY declares (`player` before it is [[maybe_unused]] where the body does not read it)
NPC = '\t\truntime::Ptr<Npc> npc = runtime::cast<Npc>(env.getVisibleObject());\n'
# what the template's register() includes (qe.registerQuestNpc(123).addOnTalkEvent)
REGISTER_INCLUDE = 'aion/gameserver/model/templates/quest/QuestNpc.h'


def synthetic(body):
    """the FileResult of a handler whose dialog hook runs `body` after the player and the dialog's npc"""
    return Synthetic('test/_99902Escort.java', source('_99902Escort', 99902, BODY % body)).r


def includes(cpp):
    """the headers an emitted file includes after the prelude, but the one of the template's register()"""
    return [ln[len('#include "'):-1] for ln in cpp.split('\n')
            if ln.startswith('#include "') and not ln.endswith(('QuestPrelude.h"', REGISTER_INCLUDE + '"'))]


def tiers(r):
    """'Class.member' -> API-table row of every member the file calls outside the core vocabulary"""
    return {k: t for k, t in r.api_tier.items() if t != 'core'}


def row(rid):
    return {r.id: r for r in api.API_TABLE}[rid]


class EscortHeaders(unittest.TestCase):
    def test_the_rows_classes_and_enums_are_indexed_with_their_bases(self):
        ix = transliterator(RULES).api.index
        # with AbstractAI's base AI, as HEADERS lists base classes (NpcAI's base AITemplate<Npc> is a class template, which cppdecl does not
        # index)
        for name in ('AbstractAI', 'AI', 'NpcAI', 'WalkManager', 'QuestTasks', 'SM_NPC_INFO', 'SpawnTemplate'):
            with self.subTest(cls=name):
                self.assertIn(name, ix.classes)
        for name in ('AIEventType', 'TaskId'):
            with self.subTest(enum=name):
                self.assertIn(name, ix.enums)


class EscortRows(unittest.TestCase):
    def test_b26_npc_ai_events(self):
        r = synthetic('\t\tnpc.getAi().onCreatureEvent(AIEventType.FOLLOW_ME, player);')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(tiers(r), {'Creature.getAi': 'B26', 'AbstractAI.onCreatureEvent': 'B26'})
        # getAi() is AbstractAI& (Creature.h): a reference, so `.`; the follower and the event as defaultStartFollowEvent sends them
        self.assertIn(NPC + '\t\tnpc->getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *player);\n', r.cpp)
        self.assertEqual(includes(r.cpp), ['aion/gameserver/ai/AbstractAI.h'])      # the member call needs the complete AbstractAI

    def test_b27_creature_tasks(self):
        r = synthetic('\t\tplayer.getController().addTask(TaskId.QUEST_FOLLOW, '
                      'QuestTasks.newFollowingToTargetCheckTask(env, npc, 204813));')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(tiers(r), {'Player.getController': 'B13', 'CreatureController.addTask': 'B27',
                                    'QuestTasks.newFollowingToTargetCheckTask': 'B28'})
        self.assertIn(NPC + '\t\tplayer->getController().addTask(TaskId::QUEST_FOLLOW, '
                                 'QuestTasks::newFollowingToTargetCheckTask(env, *npc, 204813));\n', r.cpp)
        self.assertEqual(includes(r.cpp), ['aion/gameserver/controllers/CreatureController.h',
                                           'aion/gameserver/controllers/PlayerController.h',
                                           'aion/gameserver/questEngine/task/QuestTasks.h'])

    def test_b28_escort_checks_with_each_overload(self):
        # QuestTasks.java:28-66: the npc, npc id, point and zone overloads; C++ takes the npcs as Npc& and the zone as const ZoneName*
        r = synthetic('\t\tQuestTasks.newFollowingToTargetCheckTask(env, npc, npc);\n'
                      '\t\tQuestTasks.newFollowingToTargetCheckTask(env, npc, 204813);\n'
                      '\t\tQuestTasks.newFollowingToTargetCheckTask(env, npc, 505.69427f, 437.69382f, 885.1844f);\n'
                      '\t\tQuestTasks.newFollowingToTargetCheckTask(env, npc, ZoneName.get("HALABANA_HOT_SPRINGS_220020000"));')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(tiers(r), {'QuestTasks.newFollowingToTargetCheckTask': 'B28'})
        self.assertIn(NPC + '\t\tQuestTasks::newFollowingToTargetCheckTask(env, *npc, *npc);\n'
                                 '\t\tQuestTasks::newFollowingToTargetCheckTask(env, *npc, 204813);\n'
                                 '\t\tQuestTasks::newFollowingToTargetCheckTask(env, *npc, 505.69427f, 437.69382f, 885.1844f);\n'
                                 '\t\tQuestTasks::newFollowingToTargetCheckTask(env, *npc, '
                                 'ZoneName::get("HALABANA_HOT_SPRINGS_220020000"));\n',
                      r.cpp)
        self.assertEqual(includes(r.cpp), ['aion/gameserver/questEngine/task/QuestTasks.h', 'aion/gameserver/world/zone/ZoneName.h'])
        self.assertEqual(row('B28').gate, row('B03').gate)       # QuestTasks came with the follow helpers (M5d stage 3, E-07)

    def test_b29_walking(self):
        r = synthetic('\t\tnpc.getSpawn().setWalkerId("4212");\n\t\tWalkManager.startWalking((NpcAI) npc.getAi());')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(tiers(r), {'VisibleObject.getSpawn': 'B29', 'SpawnTemplate.setWalkerId': 'B29', 'Creature.getAi': 'B26',
                                    'WalkManager.startWalking': 'B29'})
        # Java's (NpcAI) cast throws for another AI, and so does runtime::cast (NpcMoveController.cpp's spelling; the AIEngine.h note)
        self.assertIn(NPC + '\t\tnpc->getSpawn()->setWalkerId("4212");\n'
                                 '\t\tWalkManager::startWalking(*runtime::cast<NpcAI>(npc->getAi()));\n', r.cpp)
        self.assertEqual(includes(r.cpp), ['aion/gameserver/ai/NpcAI.h', 'aion/gameserver/ai/manager/WalkManager.h',
                                           'aion/gameserver/model/templates/spawns/SpawnTemplate.h'])

    def test_b30_npc_info_packet(self):
        r = synthetic('\t\tPacketSendUtility.sendPacket(player, new SM_NPC_INFO(npc, player));')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertEqual(tiers(r), {'SM_NPC_INFO.<init>': 'B30'})
        self.assertIn(NPC + '\t\tPacketSendUtility::sendPacket(*player, SM_NPC_INFO(*npc, *player));\n', r.cpp)    # the Npc& overload
        self.assertEqual(includes(r.cpp), ['aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h'])


class NullableStringParameter(unittest.TestCase):
    """SpawnTemplate::setWalkerId(std::optional<std::string_view>): Java's setWalkerId(String) may be passed null, so C++ spells the
    String as an optional; a String argument converts to it implicitly (std::optional's converting constructor)"""

    def test_a_string_literal_and_a_string_local_are_passed_as_they_are(self):
        r = synthetic('\t\tString walker = "4212";\n\t\tnpc.getSpawn().setWalkerId(walker);\n\t\tnpc.getSpawn().setWalkerId("4212");')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn(NPC + '\t\tstd::string_view walker = "4212";\n\t\tnpc->getSpawn()->setWalkerId(walker);\n'
                                 '\t\tnpc->getSpawn()->setWalkerId("4212");\n', r.cpp)

    def test_the_java_fit_and_the_cpp_rank(self):
        tr = transliterator(RULES)
        nullable, optional_int = emit.CT('optional', elem=emit.STRING), emit.CT('optional', elem=emit.INT)
        string = emit.E('"4212"', emit.STRING)
        self.assertEqual(tr.fit(string, nullable), 2)          # below a std::string_view parameter's 3: a String overload stays best
        self.assertIsNone(tr.fit(string, optional_int))
        # C++: one user-defined conversion either way, from a literal and from a std::string_view
        self.assertEqual(tr.cpp_rank(('strlit', ''), nullable), 3)
        self.assertEqual(tr.cpp_rank(('string', ''), nullable), 3)
        self.assertIsNone(tr.cpp_rank(('strlit', ''), optional_int))
        self.assertIsNone(tr.cpp_rank(('string', ''), optional_int))


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class TheSixEscorts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tr = transliterator(RULES)
        cls.results = {rel: tr.transliterate(QUEST / rel) for rel in SIX}

    def test_they_transliterate_with_their_rows_at_parity(self):
        for rel, rows in SIX.items():
            with self.subTest(file=rel):
                r = self.results[rel]
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertEqual(r.api_rows, rows)
                self.assertEqual(parity.compare((QUEST / rel).read_text(encoding='utf-8-sig'), r.cpp), [])

    def test_their_escort_starts(self):
        # the statements the rows exist for, one per file (_24053:133-134, _2634:68-70, _2333:71-75, _2394:66-70, _4212:78-81, _3212:78-81)
        starts = {
            'beluslan/_24053TheMaulingoftheMau.java':
                '\t\t\t\t\t\tsurvivor->getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *player);\n'
                '\t\t\t\t\t\tplayer->getController().addTask(TaskId::QUEST_FOLLOW, '
                'QuestTasks::newFollowingToTargetCheckTask(env, *survivor, 204813));\n',
            'beluslan/_2634TheDraupnirRedemption.java':
                '\t\t\t\t\t\t\t\tPacketSendUtility::sendPacket(*player, SM_NPC_INFO(*survivor, *player));\n'
                '\t\t\t\t\t\t\t\tsurvivor->getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *player);\n'
                '\t\t\t\t\t\t\t\tplayer->getController().addTask(TaskId::QUEST_FOLLOW, '
                'QuestTasks::newFollowingToTargetCheckTask(env, *survivor, 204828));\n',
            'morheim/_2333ARibbitOutOfWater.java':
                '\t\t\t\t\tWalkManager::startWalking(*runtime::cast<NpcAI>(debrie->getAi()));\n'
                '\t\t\t\t\tdebrie->getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *player);\n',
            'morheim/_2394ADyingWish.java':
                '\t\t\t\t\tplayer->getController().addTask(TaskId::QUEST_FOLLOW, QuestTasks::newFollowingToTargetCheckTask(env, *orlan, '
                'ZoneName::get("HALABANA_HOT_SPRINGS_220020000")));\n',
            'pandaemonium/_4212MissingSidrunerk.java':
                '\t\t\t\t\tnpc->getSpawn()->setWalkerId("4212");\n'
                '\t\t\t\t\tWalkManager::startWalking(*runtime::cast<NpcAI>(npc->getAi()));\n',
            'sanctum/_3212TheMissingCubeCraftsman.java':
                '\t\t\t\t\tplayer->getController().addTask(TaskId::QUEST_FOLLOW, '
                'QuestTasks::newFollowingToTargetCheckTask(env, *npc, 505.69427f, 437.69382f, 885.1844f));\n',
        }
        for rel, text in starts.items():
            with self.subTest(file=rel):
                self.assertIn(text, self.results[rel].cpp)

    def test_the_report_counts_the_rows_files(self):
        _results, rep = cli.run([QUEST / rel for rel in SIX], pairs=False)
        used = {rid: v['files'] for rid, v in rep['apiRowsUsed'].items() if 'B26' <= rid <= 'B30'}
        self.assertEqual(used, {'B26': 6, 'B27': 6, 'B28': 6, 'B29': 4, 'B30': 1})

    def test_they_are_not_in_the_handler_tree(self):
        # m5d-plan.md §21.1: like B03's 14 escorts, they wait for FollowingNpcAI (without it the follower neither walks nor respawns)
        for rel in SIX:
            with self.subTest(file=rel):
                self.assertFalse((HANDLER_QUEST_DIR / rel).with_suffix('.cpp').exists())


if __name__ == '__main__':
    unittest.main()
