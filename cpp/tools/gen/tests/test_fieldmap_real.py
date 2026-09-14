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
        self.assertEqual(self.field(c + 'model.gameobjects.VisibleObject', 'target').cpp, 'SelfOrRef<VisibleObject>')  # fieldmap.toml, RT-4
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
        # a protected owner field assigned only in the constructor (CreatureMoveController.java:29), no fieldmap.toml spelling: its type
        # variable T extends VisibleObject is spelled as the bound (erasure rule, hub-headers.md §8.1)
        owner = self.field(c + 'controllers.movement.CreatureMoveController', 'owner')
        self.assertEqual((owner.cpp, owner.rule), ('OwnerRef<VisibleObject>', 'part owner (assigned only in constructors)'))
        # erasure without overrides: bounds for type variables, no type arguments for erased generics
        self.assertEqual((self.field(c + 'ai.AbstractAI', 'owner').cpp, self.field(c + 'ai.AbstractAI', 'owner').rule), ('OwnerRef<Creature>', 'part owner'))
        self.assertEqual(self.field(c + 'model.team.GeneralTeam', 'leader').cpp, 'Field<Ref<TeamMember>>')
        self.assertEqual(self.field(c + 'model.vortex.VortexLocation', 'activeVortex').cpp, 'Field<Ref<DimensionalVortex>>')
        self.assertEqual(self.field('instance.pvp.BasicPvpInstance', 'instanceScore').cpp, 'Field<Ref<PvpInstanceScore>>')
        self.assertEqual(self.field(c + 'network.aion.AionConnection', 'packetProcessor').cpp,
                         'static inline const Ref<PacketProcessor<AionConnection>>')  # commons generics stay templates
        # hub-headers.md §6 external spellings, dropped fields and capture overrides (fieldmap.toml)
        self.assertEqual((self.field(c + 'questEngine.QuestEngine', 'messageTask').cpp, self.field(c + 'questEngine.QuestEngine', 'messageTask').rule),
                         ('Field<Ref<JobDetail>>', 'non-final external handle'))
        self.assertEqual(self.field(c + 'taskmanager.AbstractCronTask', 'nextRun').cpp, 'Field<Timestamp>')
        self.assertEqual(self.field(c + 'model.geometry.Polygon2D', 'bounds').cpp, 'Field<Rectangle2D>')
        self.assertIsNone(self.field(c + 'questEngine.QuestEngine', 'scriptManager').cpp)
        checker = self.fm.classes[c + 'network.aion.AionConnection.ConnectionAliveChecker']
        self.assertEqual([cap.cpp for cap in checker.captures], ['const std::weak_ptr<AionConnection>'])
        self.assertNotIn(c + 'network.aion.AionConnection.ConnectionAliveChecker#this$0', self.fm.cycle_edges)
        self.assertEqual(self.field(c + 'world.zone.ZoneName', 'NONE').cpp, 'static const ZoneName* const')
        self.assertEqual(self.field(c + 'model.gameobjects.Persistable', 'NEW').cpp, 'static const PinnedCallback<bool(Persistable&)>')
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
        self.assertEqual(self.field(c + 'services.event.EventBuffHandler', 'effectForceType').cpp, 'const Effect_ForceType*')  # fieldmap.toml
        self.assertEqual(self.field(c + 'skillengine.model.Effect', 'forceType').cpp, 'Field<const Effect::ForceType*>')  # inferred
        # Effect's observers are held by the effected creature's ObserveController: the captured Effect is retained
        for anon in ('skillengine.model.Effect$1', 'skillengine.model.Effect$2'):
            self.assertEqual({cap.name: cap.cpp for cap in self.fm.classes[c + anon].captures}['this'], 'const Ref<Effect>')
            self.assertIn(c + anon + '#this', self.fm.cycle_edges)

    def test_s0c_freeze_decisions(self):
        c = 'com.aionemu.gameserver.'
        cls = self.fm.classes
        # RefCounted class trees are no parts (ChargeInfo extends ActionObserver, PlayerAllianceGroup extends TemporaryPlayerTeam)
        for part_type, owner in (('model.items.ChargeInfo', 'model.gameobjects.Item.conditioningInfo'),
                                 ('model.team.alliance.PlayerAllianceGroup', 'model.team.alliance.PlayerAlliance.groups')):
            self.assertEqual(cls[c + part_type].part_of, [], part_type)
            self.assertNotIn(tuple((c + owner).rsplit('.', 1)), self.fm.parts)
        self.assertEqual(self.field(c + 'model.gameobjects.Item', 'conditioningInfo').cpp, 'Field<Ref<ChargeInfo>>')
        self.assertEqual(self.field(c + 'model.team.alliance.PlayerAlliance', 'groups').cpp, 'HashMap<int32_t, Ref<PlayerAllianceGroup>>')
        self.assertEqual(self.field(c + 'model.team.alliance.PlayerAllianceGroup', 'alliance').cpp, 'const Ref<PlayerAlliance>')
        self.assertEqual(self.field(c + 'model.gameobjects.Npc', 'overriddenEquipment').cpp, 'Field<Ref<NpcEquippedGear>>')
        # ItemStone has no base of its own ([bases]): ManaStone is RefCounted, IdianStone a part of Item.idianStone
        self.assertEqual([self.fm.base_of(cls[c + 'model.items.' + n]) for n in ('ItemStone', 'ManaStone', 'IdianStone')], [None, 'RefCounted', 'OwnedPart'])
        # singletons through a nested *Holder class
        for svc in ('services.RiftService', 'services.VortexService', 'services.AutoGroupService'):
            self.assertEqual(self.fm.base_of(cls[c + svc]), 'Immortal', svc)
        # handler roots are Immortal; confined elements are values
        self.assertEqual(self.fm.base_of(cls[c + 'questEngine.handlers.AbstractQuestHandler']), 'Immortal')
        self.assertEqual(self.field(c + 'geoEngine.collision.CollisionResults', 'results').cpp, 'std::vector<CollisionResult>')

    def _edge_file(self, key):
        site = key.split('#')[0] if '#' in key else key.rsplit('.', 1)[0]
        cb = self.fm.callbacks.get(site)
        if cb is not None:
            return fieldmap._rel(cb.rs.span.cu.path)
        return fieldmap._rel(self.fm.classes[site].cu.path)

    def test_cycle_edges_outside_the_handlers_are_resolved(self):
        """S0b cycle review: every cycle edge of game-server/src has a cycles.toml resolution; the handler scripts resolve theirs in phase 6."""
        unresolved = sorted(k for k, e in self.fm.cycle_edges.items() if e['resolution'] is None and '/data/handlers/' not in self._edge_file(k))
        self.assertEqual(unresolved, [])

    def test_cycle_review_reference_points(self):
        c = 'com.aionemu.gameserver.'
        edges = self.fm.cycle_edges
        # a Ref to a part retains the part's owner: PlayerAccountData is a part of Account (fieldmap.toml PartMap)
        self.assertIn(c + 'model.account.Account', edges[c + 'model.gameobjects.player.Player.playerAccountData']['targets'])
        # captured singletons are pinned pointers, not retaining edges
        self.assertNotIn(c + 'services.LegionService$1#this', edges)
        # non-retaining fieldmap.toml spelling
        self.assertEqual(self.field(c + 'custom.instance.neuralnetwork.Link', 'input').retains, [])
        self.assertEqual(self.fm.classes[c + 'model.team.group.events.PlayerGroupLeavedEvent'].kind, 'K5')  # fieldmap.toml [kinds]

    def test_logout_breakers_match_cycles_toml(self):
        """model/gameobjects/player/LogoutBreakers.h lists exactly the zombie-safe edges of cycles.toml, and its steps cut resolved edges."""
        import re
        header = os.path.join(fieldmap.CPP_ROOT, 'game-server', 'src', 'aion', 'gameserver', 'model', 'gameobjects', 'player', 'LogoutBreakers.h')
        with open(header, encoding='utf-8') as f:
            text = f.read()

        def table(name):
            m = re.search(rf'{name}\{{\{{(.*?)\}}\}};', text, re.S)
            self.assertIsNotNone(m, name)
            return re.findall(r'"(com\.aionemu\.[^"]+)"', m.group(1))

        resolutions = fieldmap.load_cycles(self.cycles)
        zombie = {k for k, v in resolutions.items() if v.startswith('zombie-safe:')}
        listed = table('ZOMBIE_SAFE_EDGES')
        self.assertEqual(len(listed), len(set(listed)))
        self.assertEqual(set(listed), zombie)
        for name in ('LOGOUT_STEPS', 'DELETE_STEPS'):
            for key in table(name):
                self.assertIn(key, self.fm.cycle_edges, f'{name}: {key}')
                self.assertTrue(resolutions[key].startswith(('zombie-safe:', 'cpp-breaker:')), f'{name}: {key} = {resolutions[key]}')


if __name__ == '__main__':
    unittest.main()
