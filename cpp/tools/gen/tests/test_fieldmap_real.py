"""fieldmap.py over the real Java tree: committed outputs are up to date, and design reference points hold."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import fieldmap  # noqa: E402

REPO = fieldmap.REPO_ROOT
OUT = fieldmap.DEFAULT_OUT


@unittest.skipUnless(all(os.path.isdir(r) for r in fieldmap.DEFAULT_ROOTS + fieldmap.DEFAULT_SUPPORT_ROOTS), 'Java sources not available')
class RealTreeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = os.path.join(OUT, 'fieldmap.toml')
        cls.cycles = os.path.join(OUT, 'cycles.toml')
        cls.fm = fieldmap.build(fieldmap.DEFAULT_ROOTS, fieldmap.DEFAULT_SUPPORT_ROOTS, cls.config, cls.cycles, fieldmap.DEFAULT_STATICDATA)

    def field(self, cid, name):
        return next(f for f in self.fm.classes[cid].fields if f.name == name)

    def test_committed_outputs_are_current(self):
        texts = fieldmap.outputs(self.fm, self.config, self.cycles, fieldmap.DEFAULT_ROOTS)
        stale = []
        for name, text in texts.items():
            path = os.path.join(OUT, name)
            old = None
            if os.path.exists(path):
                with open(path, encoding='utf-8') as f:
                    old = f.read()
            if old != text:
                stale.append(name)
        self.assertEqual(stale, [], 'run python cpp/tools/gen/fieldmap.py to regenerate generated/concurrency')

    def test_design_reference_points(self):
        c = 'com.aionemu.gameserver.'
        self.assertEqual(self.fm.classes[c + 'model.stats.calc.Stat2'].kind, 'K5')  # §3.1
        self.assertEqual(self.fm.classes[c + 'questEngine.model.QuestEnv'].kind, 'K4')  # §3.1
        self.assertEqual(self.field(c + 'model.gameobjects.player.Player', 'kisk').cpp, 'Field<Ref<Kisk>>')
        self.assertEqual(self.field(c + 'model.gameobjects.VisibleObject', 'target').cpp, 'Field<Ref<VisibleObject>>')
        self.assertEqual(self.fm.parts[(c + 'model.gameobjects.VisibleObject', 'controller')].patterns, {3})  # §3.2.1 pattern 3
        self.assertIn(2, self.fm.parts[(c + 'model.gameobjects.Creature', 'gameStats')].patterns)  # Npc.java:70-71
        self.assertEqual(self.field(c + 'model.items.storage.PlayerStorage', 'actor').cpp, 'SelfOrRef<Player>')  # §3.2 PlayerStorage.actor
        self.assertEqual(self.field(c + 'model.account.Account', 'players').cpp, 'PartMap<int32_t, PlayerAccountData>')
        idian = self.fm.classes[c + 'model.items.IdianStone$1']  # §14.2i
        self.assertEqual({cap.name: cap.cpp for cap in idian.captures}, {'this': 'OwnerRef<IdianStone>', 'player': 'const Ref<Player>'})
        self.assertEqual(self.field(c + 'skillengine.model.Effect', 'periodicTasks').cpp, 'Field<Ref<Array<FutureRef>>>')  # §14.2c
        self.assertTrue(self.fm.class_json(self.fm.classes[c + 'model.gameobjects.Npc'])['hasEquals'])  # AionObject equals
        self.assertIn(c + 'model.gameobjects.player.Player.kisk', self.fm.cycle_edges)  # RT-5
        self.assertIn(c + 'model.gameobjects.VisibleObject.target', self.fm.cycle_edges)  # RT-4
        self.assertEqual(self.fm.stale_resolutions, [])

    def test_review_findings(self):
        c = 'com.aionemu.gameserver.'
        # element writes do not reassign an array (QuestVars.java:34,57)
        self.assertEqual(self.field(c + 'questEngine.model.QuestVars', 'questVars').cpp, 'const Ref<Array<int32_t>>')
        self.assertEqual(self.field(c + 'spawnengine.WalkerGroup', 'memberSteps').cpp, 'const Ref<Array<int32_t>>')
        # a protected owner field assigned only in the constructor (CreatureMoveController.java:29)
        self.assertEqual(self.field(c + 'controllers.movement.CreatureMoveController', 'owner').cpp, 'OwnerRef<T>')
        # self-typed static constants of multi-instance classes are not singletons
        for cid in ('geoEngine.collision.IgnoreProperties', 'model.house.PlayerScript', 'model.stats.calc.StatCapUtil.StatCapRule',
                    'network.aion.serverpackets.SM_FRIEND_RESPONSE'):
            self.assertFalse(self.fm.classes[c + cid].singleton, cid)
            self.assertNotEqual(self.fm.base_of(self.fm.classes[c + cid]), 'Immortal', cid)
        self.assertEqual(self.field(c + 'geoEngine.collision.IgnoreProperties', 'ELYOS').cpp, 'static inline const Ref<IgnoreProperties>')
        self.assertEqual(self.field(c + 'model.gameobjects.player.PlayerScripts', 'scripts').cpp.count('Ref<PlayerScript>'), 1)
        self.assertTrue(self.fm.classes[c + 'services.LegionService'].singleton)  # SingletonHolder
        self.assertFalse(self.fm.classes[c + 'services.panesterra.ahserion.AhserionRaid'].singleton)  # fieldmap.toml per_run_services
        # interned classes ([immortal])
        self.assertEqual(self.fm.base_of(self.fm.classes[c + 'world.zone.ZoneName']), 'Immortal')
        self.assertEqual(self.field(c + 'services.event.EventBuffHandler', 'effectForceType').cpp, 'const Effect::ForceType*')
        # Effect's observers are held by the effected creature's ObserveController: the captured Effect is retained
        for anon in ('skillengine.model.Effect$1', 'skillengine.model.Effect$2'):
            self.assertEqual({cap.name: cap.cpp for cap in self.fm.classes[c + anon].captures}['this'], 'const Ref<Effect>')
            self.assertIn(c + anon + '#this', self.fm.cycle_edges)


if __name__ == '__main__':
    unittest.main()
