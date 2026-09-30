"""P6-Q slice 2, chunk Q03 (2026-09-29): the generated verteron and heiron handlers in the tree, and the three generator fixes their compile
needed (docs/deviations/Q03.md): an empty Java switch becomes its selector evaluated (MSVC C4060/C4065 under /W4), a pointer dereferenced for
a T& parameter includes T's header (Ptr<T>::operator* needs the complete type), and a returned Ref<T> includes T's header (~Ref needs the
complete type; api.OWNING_RETURN_HEADERS for a T whose header is not scanned). The byte-for-byte drift of every bannered tree file is
test_questgen_tree.py's; this file pins the chunk's list, its two hand ports and the fixes. Nothing is compiled; the C++ side is
game-server/tests/quest_handlers_golden (the golden traces of the 72 generated files in the tree; 4 more are held back) and game-server/tests/quest_handlers_q03 (the hand ports).

Run from cpp/tools/gen: python -m unittest tests.test_questgen_q03
"""
from __future__ import annotations

import os
import sys
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from questgen import api, emit, paths  # noqa: E402

from tests.test_questgen_p6t import Synthetic, source  # noqa: E402

HANDLER_QUEST_DIR = paths.CPP_GAME_SERVER / 'handlers' / 'aion' / 'gameserver' / 'handlers' / 'quest'
HAVE_JAVA = paths.JAVA_QUEST_DIR.is_dir()

# the chunk's hand ports: questgen refuses them (their anonymous Runnables), docs/deviations/Q03.md "Hand ports"
HAND_PORTS = ('heiron/_1643TheStarOfHeiron', 'heiron/_3200PriceOfGoodwill')
# transliterated like the others but held out of the tree (the review of 2026-09-29, docs/deviations/Q03.md "Held back"): gs.scenario.travel's
# Elyos Daeva arrives in Verteron at level 10, where 14010's onEnterWorldEvent starts its quest (CM_LEVEL_READY -> QuestEngine.onEnterWorld)
# and the start npcs of 1131, 1146 and 1152 put their quests (min level 11-12) into the arrival's SM_NEARBY_QUESTS; U1/U7 hold them back
HELD_BACK = ('verteron/_1131UndeliveredArmor', 'verteron/_1146DelicateMandrake', 'verteron/_1152OdellaRecipe',
             'verteron/_14010TerrainOfTheVerteronFortress')


def java_files(directory):
    return sorted(f'{directory}/{f.stem}' for f in (paths.JAVA_QUEST_DIR / directory).glob('*.java'))


def banner_of(rel):
    f = HANDLER_QUEST_DIR / f'{rel}.cpp'
    return f.is_file() and any(line.startswith(emit.BANNER_FIRST_LINE) for line in f.read_text(encoding='utf-8').split('\n'))


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class Q03Tree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tr = emit.Transliterator(rules=emit.P6T_RULES)
        cls.java = java_files('verteron') + java_files('heiron')

    def test_every_handler_of_the_chunk_is_in_the_tree_but_the_held_back_ones(self):
        self.assertEqual(len(self.java), 78)
        self.assertTrue(set(HELD_BACK) <= set(self.java))
        for rel in self.java:
            with self.subTest(file=rel):
                self.assertEqual((HANDLER_QUEST_DIR / f'{rel}.cpp').is_file(), rel not in HELD_BACK)
                if rel not in HELD_BACK:
                    self.assertEqual(banner_of(rel), rel not in HAND_PORTS)

    def test_the_held_back_files_transliterate_and_stay_out_of_the_tree(self):
        for rel in HELD_BACK:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / f'{rel}.java')
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertFalse((HANDLER_QUEST_DIR / f'{rel}.cpp').exists())
        # what holds each of them back: 14010 starts at an enter world in Verteron, the others start at an npc (a quest marker)
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'verteron/_14010TerrainOfTheVerteronFortress.java')
        self.assertIn('qe.registerOnEnterWorld(questId);', r.cpp)
        for rel in HELD_BACK[:3]:
            with self.subTest(file=rel):
                self.assertIn('->addOnQuestStart(questId);', self.tr.transliterate(paths.JAVA_QUEST_DIR / f'{rel}.java').cpp)

    def test_the_hand_ports_are_the_files_questgen_refuses(self):
        for rel in self.java:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / f'{rel}.java')
                if rel in HAND_PORTS:
                    self.assertEqual(r.status, 'refused')
                    self.assertEqual({reason for reason, _ in r.reasons}, {'anonymous-class'})
                else:
                    self.assertEqual(r.status, 'ok', r.reasons)
                    if rel not in HELD_BACK:
                        self.assertEqual((HANDLER_QUEST_DIR / f'{rel}.cpp').read_bytes(), r.cpp.encode('utf-8'))

    def test_the_hand_ports_follow_questgens_conventions(self):
        for rel in HAND_PORTS:
            with self.subTest(file=rel):
                text = (HANDLER_QUEST_DIR / f'{rel}.cpp').read_text(encoding='utf-8')
                klass = rel.split('/')[1]
                self.assertIn(f'// Hand-ported from game-server/data/handlers/quest/{rel}.java', text)
                self.assertIn(f'AION_QUEST_HANDLER({klass}, {klass[1:5]});', text)
                self.assertTrue(text.rstrip('\n').endswith('} // namespace aion::gameserver::handlers::quest::heiron'))
                self.assertIn('ThreadPoolManager::getInstance().schedule({', text)   # the Runnables as pinned tasks

    def test_the_fixes_on_the_chunks_files(self):
        def cpp(rel):
            r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
            self.assertEqual(r.status, 'ok', r.reasons)
            return r.cpp
        # _1194ReducingTursinStrength.java:52-53: `switch (targetId) { }`
        text = cpp('verteron/_1194ReducingTursinStrength.java')
        self.assertIn('\t\t\tstatic_cast<void>(targetId); // Java: an empty switch\n', text)
        self.assertNotIn('switch (targetId)', text)
        # `*player->getWorldMapInstance()` for spawnForFiveMinutes' WorldMapInstance& (_14016AGateAgape.java:105; _1636's statue)
        for rel, deref in (('verteron/_14016AGateAgape.java', '*player->getWorldMapInstance()'),
                           ('heiron/_1636AFluteForTheFixing.java', '*drakeStoneStatue->getWorldMapInstance()')):
            with self.subTest(file=rel):
                text = cpp(rel)
                self.assertIn(deref, text)
                self.assertIn('#include "aion/gameserver/world/WorldMapInstance.h"', text)
        # _18602NightmareinShiningArmor.java:137 discards applyEffectDirectly's Ref<Effect>
        text = cpp('heiron/_18602NightmareinShiningArmor.java')
        self.assertIn('SkillEngine::getInstance().applyEffectDirectly(19288, *player, *player);', text)
        self.assertIn('#include "aion/gameserver/skillengine/model/Effect.h"', text)


BODY = '''
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		Player player = env.getPlayer();
		int targetId = env.getTargetId();
%s
		return false;
	}
'''
QUEST_ENV_IMPORT = 'import com.aionemu.gameserver.questEngine.model.QuestEnv;\n'


def synthetic(body):
    src = source('_99901Test', 99901, BODY % body)
    src = src.replace(QUEST_ENV_IMPORT, QUEST_ENV_IMPORT + 'import com.aionemu.gameserver.skillengine.SkillEngine;\n')
    return Synthetic('test/_99901Test.java', src).r


class Fixes(unittest.TestCase):
    def test_an_empty_switch_is_its_selector_evaluated_with_its_comments(self):
        r = synthetic('\t\tswitch (targetId) { // none yet\n\t\t\t// later\n\t\t}')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tstatic_cast<void>(targetId); // Java: an empty switch // none yet\n\t\t// later\n', r.cpp)
        self.assertNotIn('switch (', r.cpp)
        self.assertEqual(r.idioms.get('empty switch as its selector evaluated'), 1)

    def test_a_switch_with_cases_is_unchanged(self):
        r = synthetic('\t\tswitch (targetId) {\n\t\t\tcase 1:\n\t\t\t\treturn true;\n\t\t}')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tswitch (targetId) {\n\t\t\tcase 1:\n\t\t\t\treturn true;\n\t\t}\n', r.cpp)
        self.assertNotIn('empty switch as its selector evaluated', r.idioms)

    def test_a_dereferenced_pointer_includes_its_class(self):
        r = synthetic('\t\tspawnForFiveMinutes(203098, player.getWorldMapInstance(), 1f, 2f, 3f, (byte) 0);')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('*player->getWorldMapInstance()', r.cpp)
        self.assertIn('#include "aion/gameserver/world/WorldMapInstance.h"', r.cpp)

    def test_a_returned_ref_includes_its_class(self):
        self.assertEqual(api.OWNING_RETURN_HEADERS, {'Effect': 'aion/gameserver/skillengine/model/Effect.h'})
        r = synthetic('\t\tSkillEngine.getInstance().applyEffectDirectly(19288, player, player);')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('#include "aion/gameserver/skillengine/model/Effect.h"', r.cpp)
        r = synthetic('\t\tsendQuestDialog(env, 1011);')
        self.assertNotIn('Effect.h', r.cpp)


if __name__ == '__main__':
    unittest.main()
