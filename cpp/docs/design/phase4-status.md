# Phase 4 status

> Phase 4 ports the bodies behind the frozen spine (tag `spine-v1`) towards milestone M4, "all static data loads"
> ([handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.6, §2.7, amendments §9). Waves: 3a-1 (commit `a99ec5fcb`), 3a-2 (commit
> `fee633404`), 3b-1 (P4-09 holders, P4-10 world, P4-11b controllers, P4-14 DAOs and the M4 gate; working tree of 2026-09-15). **M4 passed**
> (section "Milestone M4"). Header request decisions: [../porting/header-requests.md](../porting/header-requests.md). Deviations are collected
> per chunk in [../deviations/](../deviations/) until the integrator merges them into [../DEVIATIONS.md](../DEVIATIONS.md). Earlier steps:
> [wave1-status.md](wave1-status.md), [spine-status.md](spine-status.md).

## Chunks

`AION_UNPORTED` counts `AION_UNPORTED(` occurrences in the files of `chunks.py files <chunk>` (sources and headers, tests excluded), given as
occurrences/files, at `spine-v1` → `a99ec5fcb` (3a-1) → `fee633404` (3a-2) → now (the wave 3b-1 working tree after the M4 gate). The
`fee633404` and "now" numbers use today's file lists (`git grep` at the commit), so files added later count as 0 there; P4-08 counts every
file of its lease (shells of P5-02..06), which earlier versions of this table counted differently. Tests are the `TEST*` macros in the chunk's
test directory; the full ctest result is in "Milestone M4".

| Chunk | Target | Wave | Status | `AION_UNPORTED` spine-v1 → 3a-1 → 3a-2 → now | Tests | Still needs (from chunk) |
|---|---|---|---|---|---|---|
| P4-01 | `aion_gs_configs` | 1 | done | 0 | `tests/configs` 52 | – |
| P4-02a/b | `aion_gs_runtime_*`, `aion_gs_handler_registry` | kernel, 1 | done | 3/1 (the macro and its doc comment in `Unported.h`, no stubs) | `tests/runtime` 440; `tests/handler_registry` 5 | kernel open items ([runtime-kernel-status.md](runtime-kernel-status.md)); the watchdog minidump hang below |
| P4-03 | `aion_gs_geomath` | 1 | done | 0 | `tests/geomath` 77 | – |
| P4-15a | `aion_gs_network_crypt` | 1 | done | 0 | `tests/network_crypt` 27 | – |
| T2 / T2-gen | `aion_gs_xml` / `aion_gs_staticdata` | 1 | done (generated) | 0 | `tests/xml` 60 | – |
| P4-04 | `aion_gs_geo` | 3a-2 | ported | 87/9 → 87/9 → 1/1 → 0 | `tests/geo` 40 | `SiegeService.getSiegeLocation` behind the SHIELD default callback (P5-12a; no loaded node is a SHIELD) |
| P4-05 | `aion_gs_base` | 3a-1 | ported | 56/5 → 7/6 → 5/5 → 5/5 | `tests/base` 59 | `TemporarySpawnEngine` hook of `GameTime::onHourChange` (now available, P4-10); `ChatProcessor.h` (P5-14); `PlayerStatCalculator` (P5-01); CAPTCHA rasterizer decision |
| P4-06 | `aion_gs_sysmsg` | 3a-1 | ported | 4/1 → 0 → 0 → 0 | `tests/sysmsg` 13 | stand-in l10n helpers → `ChatUtil`/`RaceInfo`, `AbyssRankEnum` companion (P5-01) |
| P4-07a | `aion_gs_templates` | 3a-1 | ported | 39/12 → 6/5 → 2/1 → 0 | `tests/templates/P4-07a` 51 | item actions (P5-07) |
| P4-07b | `aion_gs_templates` | 3a-2 | ported | 26/20 → 26/20 → 0 → 0 | `tests/templates/P4-07b` 27 | – |
| P4-08 | lease over P5-02..06 shells | 3a-2 | data side ported | 257/182 at 3a-2 and now (behaviour methods for P5-02..06) | `tests/skills` 18, `tests/quest` 8 | override declarations of the `EffectTemplate` virtuals in the other effect shells (open, 3a-2) |
| P4-09 | `aion_gs_dataholders` | 3b-1 | ported | 83/79 → 98/81 → 88/71 → 8/7 | `tests/dataholders` 48 | //reload setters (D3), write-back (`SpawnsData.saveSpawn`, `WalkerData`, `ZoneData.saveData`), `MotionData::calculateAnimationTimesAfterLastHit` (`ChargeSkill.h`, P5-02) |
| P4-10 | `aion_gs_world` | 3b-1 | ported | 277/22 → 278/22 → 278/22 → 6/6 | `tests/world` 24 | no-op `InstanceHandler` for `detachInstanceHandler` (P5-13), `WalkManager.h` (P5-05), Gatherable/StaticDoor/StaticObject constructors and controllers for the spawners (P4-11a/b), `ZoneService.saveMaterialZones` (write-back) |
| P4-11a | `aion_gs_objects` | 3a-1 | ported | 366/23 → 24/11 → 22/10 → 22/10 | `tests/objects` 36 | summon/trap/homing/servant stats (P5-01), `NpcSkillTemplateEntry` users (P5-02), the constructors the P4-10 spawners need |
| P4-11b | `aion_gs_controllers` | 3b-1 | ported | 262/25 → 263/25 → 263/25 → 30/2 | `tests/controllers` 31 | 26 stand-ins in `ControllerStandIns.cpp` for P5-01/02/05/10/12b/13/14 providers without headers; `GatheringTask` (P5-02) for 4 `GatherableController` bodies |
| P4-12 | `aion_gs_player` | 3a-1 | ported | 378/37 → 9/5 → 9/5 → 9/5 | `tests/player` 27 | `ExpireTimerTask` (P5-14), `TitleChangeListener`/`ItemEquipmentListener`/`PlayerGameStats` (P5-01) |
| P4-13 | `aion_gs_items` | 3a-2 | ported | 176/22 → 176/22 → 0 → 0 | `tests/items` 32 | – |
| P4-14 | `aion_gs_dao` | 3b-1 | ported | 313/56 → 313/56 → 313/56 → 2/1 | `tests/dao` 61 (MariaDB, skipped without `AION_TEST_GS_DATABASE_URL`) | `PlayerModelEntry.h` (P5-13) for `CustomInstancePlayerModelEntryDAO`; `LegionHistoryAction` companion (P5-10) |
| P4-15 | `aion_gs_network` | 3a-1 | ported | 56/5 → 24/8 → 24/8 → 24/8 | `tests/network` 42 | score writers (P5-13), `SM_KEY::writeImpl` (P4-16), three LS packets (P4-17), `StatEnum` companion (P5-01), `GameServer.h` (P5-14) |
| P4-16 | `aion_gs_sm_ak` | 3b | not started | 174/111 | none | model bodies for tests |
| P4-17 | `aion_gs_sm_lz` | 3b | not started | 208/127 | none | model bodies for tests |
| P5-14 (`main.cpp` only) | `aion_game_server` | M4 gate | M4 startup path | 0 in `main.cpp` (the rest of P5-14: 105/22) | `gs.smoke.startup`, `gs.m4.check_static_data` | the rest of `GameServer.main` (P5) |

## Wave 3a-1 (2026-09-14, commit `a99ec5fcb`)

Workflow `build/workflows/game-server-phase4-wave-3a1.js`: 5 port lanes (P4-06 went with the network lane), 5 adversarial reviewers, 1
header request integrator, 3 grouped fixers; 14 agents, 7.7M tokens, 2,573 tool calls. Results: `build/workflows/w3a1-result.json`,
`build/workflows/w3a1-followups.json`.

### What was ported

| Lane | `AION_UNPORTED` (lane report) | Ported |
|---|---|---|
| base (P4-05) | 56 → 11 after port, 7 after fixes and header requests | GameTime, Point3D, Announcement, Expirable, PacketSendUtility (all overloads, pinned `scheduleOrRun`); 21 enum companions; Chance, PositionUtil (bit-exact float oracle), TimeUtil, Util, ChatUtil; geometry (AbstractArea, Cylinder/Rectangle/Poly/Sphere/SemisphereArea, Plane3D, Polygon2D with Java 25 Path2D rules); collections (SplitList family, Predicates, CollectionUtil); ServerTime; AuditLogger, AutoBan, GMService; PlayerActions; HTMLCache (own cache format); CompressUtil (zlib level-6 deflate port and inflater, byte-exact against zlib 1.3.1); DDSConverter; XmlUtil::listFiles; JavaColor, simpleClassName, enumValueOf |
| templates (P4-07a) | 39 → 6 → 2 (pre-stage) | all 8 afterUnmarshal hooks and the `setXmlUid`/`setXmlName` setters of item, npc, spawns, world, zone, stats, pet; 38 behaviour classes, SpawnGroup (pool under the map monitor), PetDopingBag; 11 new headers (spawn template family, WorldZoneTemplate, MaterialZoneTemplate, SpawnSearchResult, ...); 12 enum companions. Real data binds strictly: 102,009 items and 2,483 decomposables (actions stripped), 63,287 npcs (equipment stripped), 140 spawn maps, 161 world maps, 24 weather, 218 pets, 33 pet feed; totals match the oracle |
| objects (P4-11a) | 366 → 25 → 24 | VisibleObject, Creature, Npc, Summon, SummonedObject, SiegeNpc, Trap, Kisk, Pet, TransformModel, DropNpc, Letter, BrokerItem, Item, HouseObject family, GroupApplication/Recruitment, ServerWideGroup, AssembledNpc(Part); 20 classes without a header got header and body (house objects, Homing, Servant, SummonedHouseNpc, StaticObject, StaticDoor, Gatherable, FlyRing, Road); 8 enum companions |
| player (P4-12) | 378 → 35 → 20 (header requests) → 9 (fixes) | `model.gameobjects.player` and `model.account` (46 Java files); `create<Player>` works in tests (pet loader seam, stat doubles); 8 enum companions incl. `RatesInfo`; `LogoutBreakers` run/onDelete and the zombie breaker |
| network (P4-15, P4-06) | P4-15 56 → 29 → 24; P4-06 4 → 0 | AionConnection (eager serialization, SM_KEY in `initialized()`, 3-strike decrypt, PFF filter, alive checker), packet bases, PacketWriteHelper, client packet factory table, GameConnectionFactoryImpl, FloodManager, NetFlusher, BannedMac*; LS link (25 packets, per-link SerialExecutor), CS link (6 packets); 20 iteminfo classes, SkillEntryWriter, 8 of 9 instance score writers (partly); `SM_SYSTEM_MESSAGE::writeImpl`, the 3 hand factories, `toJavaString` overloads |

### Review and fix outcomes

| Lane | Review findings | Fix group | Outcome |
|---|---|---|---|
| base | 8 (3 medium: fixed-Huffman `compress`, GMService throws on every login, l10n lone surrogates) | base-templates | 8 fixed with tests; 3 deferred: GMService (resolved by pre-1), l10n surrogates (resolved by pre-4), broadcast serialize-once (performance only, open). The fixer's final fresh build and full ctest had not finished when it returned |
| templates | 3 (1 medium: `SpawnSearchResult` non-owning spot) | base-templates | all fixed (spot resolved by pre-3) |
| objects | 5 (all low) | objects-player | 13 of 13 fixed across both lanes; full ctest 1,526/1,526 |
| player | 8 (1 high: `setPlayerMode` with Java null; 3 medium: account warehouse actor, pet loading order, quest methods left unported) | objects-player | all fixed (3 as a deviation or `// java-race` marker) |
| network | 9 (2 medium: LS/CS connection read twice, `BonusInfoBlobEntry` unported) | network-sysmsg | 8 fixed; rejected: `DredgionScoreWriter` header (`Ref<DredgionRoom>` needs a complete type, P5-13); full ctest 1,522 passed, 0 failed |

### Header requests

18 requests: 11 approved, 3 approved (modified), 4 rejected ([header-requests.md](../porting/header-requests.md) "Wave 3a-1").

| Id | Change | Decision |
|---|---|---|
| base-1, base-2 | `Area::getClosestPoint` returns `std::optional<Point2D>` / `Ref<Point3D>` | approved |
| base-3 | `Point2D` constructors | rejected (P4-07a had added them) |
| templates-1 | 9 `StatsTemplate` getters and the destructor virtual (PlayerStatsTemplate overrides) | approved |
| objects-1 | `Trap::getMasterName` override | approved |
| player-1 | `KnownList::clearWithoutNotify` (P4-10 stub) | approved |
| player-2 | `ObserveController::hasObservers` (P4-11b stub) | approved (modified: non-const) |
| player-3 | `PlayerStorage::getActor` | approved |
| player-4, player-5 | `PlayerExperienceTable` and 7 other holder lookups (P4-09 stubs) | approved |
| player-6 | `NpcFactionTemplate::isMentor`/`getMinLevel`, `QuestTemplate::isMentor` | approved |
| player-7 | `WeaponDualEffect::hasDualWieldEffect` (P5-04), `EmotionLearnAction::isLearnable` (P5-07) | approved |
| network-1, network-2 | new `GameServer.h` (P5-14), `CM_PING.h` (P5-00) | rejected (new files of those chunks; exact local stand-ins) |
| network-3 | `ItemRestrictionCleanupData` lookups (P4-09 stubs) | approved |
| network-4 | `TEST_SUPPORT support` for P4-15, `FakeGameClient.h` to `tests/support` | approved (modified: `TEST_INCLUDES support`) |
| network-5 | link the login server library into network tests | rejected (one process-wide DB pool; P5-00 harness) |
| network-6 | network `// fieldmap:` waivers into `fieldmap.toml` | approved (modified; lint L1 accepts `[cpp_members]`) |

### Deviations and Java races per chunk

| Chunk | Deviations ([../deviations/](../deviations/)) | Java races fixed (D6, with test) | Kept, marked `// java-race` |
|---|---|---|---|
| P4-05 | 18: `sendPacket` single connection load, sin/cos intrinsics, CompressUtil errors, HTMLCache format and encoding, CAPTCHA, awt-only Polygon2D/RectangleArea members, JAXB/XSD, Unicode lowercase, l10n as WTF-8, staff snapshot, ServerTime precision, `getClosestPoint` types, `Chance` template, `AutoBan` friend, `setPlayerMode` `std::any`, enum companions | `PacketSendUtility.sendPacket` (no test: needs a Player) | Polygon2D `addPoint`/`reset`; HTMLCache reload (safe through `Field<Ref>`) |
| P4-06 | 2: `Object...` formatting, l10n stand-ins | – | – |
| P4-07a | 4: `getPetFunctions` without mutation, per-template empty stats, hook exceptions, `ResultedItem` XmlID lookup | `PetTemplate.getPetFunctions` | `PetDopingBag.switchItems` |
| P4-11a | 14: lazy-creation CAS (2), Item `NOACTION`, `toString` identity and creator, `canUseSkillInMove`, null enum accessors (Npc, GroupRecruitment), epoch timestamps, shared default `KiskStatsTemplate`, StaticDoor states, master-name overrides, Servant object type, AssembledNpc parts copy | `Creature.setSkillCoolDown` map; `Item` stones and modifiers lazy creation (PCT-widened tests) | `getTransformModel` (PartSlot has no CAS), `Summon.registerRelease`, `setPersistentState` of Item, BrokerItem, HouseObject; S0B-052 `setTarget` bug kept |
| P4-12 | 9: AbyssRank initial state, MotionList null key, `getAttackType`, null timestamps, `LogoutBreakers` step isolation, zombie breaker edges, PetList test seam, `setActive(0)` broadcast, Account iteration order | none needed | `getCharacterPasskey`, `getChainSkills`, `getHouses`, `addRideObserver`, `ResponseRequester.denyAll`, `PortalCooldown.increaseEnterCount`, `AbyssRank.addAp/addGp`, `PlayerCommonData.addExp/setExp/addReposeEnergy` |
| P4-15 | 22: eager serialization, oversize warning, name echo, SM_KEY, alive checker creation, PFF map, `toString` scope, client packet table, logs, ordered LS/CS execution, link connect, NetFlusher, transfer log, `clonePacket`, flood NPE, factory, MAC ban log, enum stand-ins, `GameServer` shutdown flags, LS packets serialized on the sender, negative array size | LS/CS link send after a link drop (`upConnection()`), `kickOnlineCharacters` | `BannedMacManager.banAddress`, `ChatServer.setPublicAddress`, `CM_CS_AUTH_RESPONSE` state order, `LoginServer.reconnect` |

### Deferred items

| Item | Owner |
|---|---|
| M4/M5a login blocker: `GMService::getInstance()` threw until `SkillData::getSkillTemplates` existed | resolved in the 3a-2 pre-stage (pre-1); staff logins still throw while `LOGIN_EXECUTE_COMMANDS` is non-empty (default) until `ChatProcessor` (P5-14) |
| l10n ids with `id % 32768` in 27648..28671 sent as U+FFFD | resolved (pre-4, WTF-8); client `readS` still replaces lone surrogates (open) |
| Broadcasts serialize a SHARED packet per recipient, not once (runtime-architecture.md §8.3; performance only) | P4-05 `PacketSendUtility` helper + P4-15 enqueue of a serialized body |
| JAXBUtil callers outside static data (SpawnsData write-back with XSD, Rift/Siege/WorldRaid schedules, InGameShopProperty, DatabaseCleaningService) | their owners (P4-09, P5-12a/b, P5-14): binder entry points or a decision to drop XSD validation |
| CompressUtil ports zlib instead of linking it | lead decision (zlib is only an indirect vcpkg dependency) |
| S0B-039 send queue (deque with ordered insertion) | P4-15, measure first |
| `DredgionScoreWriter` and 8 score writers' remaining bodies | P5-13 headers, then P4-15 |
| Stub bodies added by header requests (P4-09 lookups, `KnownList::clearWithoutNotify`, `ObserveController::hasObservers`, `WeaponDualEffect::hasDualWieldEffect`, `EmotionLearnAction::isLearnable`) | P4-09, P4-10, P4-11b, P5-04, P5-07 |
| Untested for lack of a Player or published data: PacketSendUtility D6 fix, GMService, AuditLogger, AutoBan, PlayerActions, `Predicates::Players`; NpcFactions quest methods; `Trap::getMasterName`; Equipment two-handed dedupe with real weapons (`PlayerSkillList.isSkillPresent`, P5-02) | P4-05, P4-12, P4-11a, P5-00 harness |
| ConnectionAliveChecker timing test (needs a DeterministicExecutor executable or clock injection); sysmsg hand factories through a real Player | P4-15, P4-06 |
| `Creature::getTransformModel` stays `// java-race` while other lazy creations use CAS | integrator decision (PartSlot CAS) |
| Remaining `// fieldmap:` waivers: `HTMLCache.h`, `StaticDoor.h` (`TreeSet<StaticDoorState>`) | integrator (`fieldmap.toml`) |
| Stand-ins to replace now that providers exist: `network/detail/EnumIds.h` and `SystemMessageL10n.h` (P4-05 companions, `ChatUtil`), local `simpleClassNameOf` in `AIEngine.cpp` (P5-05), `ChatCommand` color helper; still waiting: `ItemSlot`/`ItemMask` (P4-13), `AbyssRankEnum`/`StatEnum`/`AttackStatus`/`XPLossEnum` (P5-01), `ZoneAttributes` (P4-10), `AbstractGmCommandPacket.h` (P5-15) | the using chunks |
| `XmlUtilTest.ListFilesDoesNotFollowLinks` skips without the symlink privilege; `tools/gen` drafted-units threshold relaxed from > 500 to > 0 | – (noted) |

## Wave 3a-2 (2026-09-15, commit `fee633404`)

Workflow `build/workflows/game-server-phase4-wave-3a2.js`: a pre-stage for the four requests wave 3a-1 left open, 4 port lanes, 4 adversarial
reviewers, 1 header request integrator, 2 grouped fixers and a documentation agent; 13 agents, 6.2M tokens, 1,986 tool calls. Results:
`build/workflows/w3a2-result.json`, `build/workflows/w3a2-followups.json`.

### What was ported

| Lane | `AION_UNPORTED` (lane report) | Ported |
|---|---|---|
| pre-stage | – | pre-1 `SkillData` lookups (GMService no longer throws at login), pre-2 `NpcData`/`ItemSetData`/`ItemGroupsData`/`WorldMapsData` lookups, pre-3 `SpawnSearchResult.spot` by value, pre-4 WTF-8 strings for l10n ids with lone surrogates |
| geo (P4-04) | 87 → 1 (the default material zone sink, ported by the M4 gate) | all 23 classes: GeoWorldLoader (terrain PNGs, `models.mesh`, `<mapId>.geo`), BIH tree (equal to brute force on 10k seeded rays), collision, bounding, scene, DespawnableNode, GeoMap, Terrain; the independent Python geo oracle `tools/oracle/geo` (counts, 200 `getZ` probes, material zone names), matched by `GeoRealDataTest` |
| templates-b (P4-07b) | 26 → 4 → 0 | every hook, binding setter and logic method of the remaining template packages, 20 enum companions, headers for 9 classes; about 40 imports bind with hooks and match the oracle counts |
| shells (P4-08 lease) | data side only | the four skill/quest `afterUnmarshal` hooks (effect types and conflicts, motions, times), skill template queries, XML quest npc lookups; behaviour methods stay for P5-02..06 |
| items (P4-13) | 176 → 7 → 0 | storage (ItemStorage, Storage, PlayerStorage, LegionStorageProxy), stones, charge info, trade, broker, drop, enchant/tempering, in-game shop models |

### Review, header requests, fixes

- Review: 17 findings (geo 3, templates-b 4, shells 4, items 6; 3 medium). Fixes: geo-items 9 of 9 fixed; templates-shells 7 fixed and 1
  rejected, plus 13 data-only bodies ported. Both fixers ran the full verification (fresh configure, two Debug builds, lint, full ctest with
  1,673 tests passed).
- Header requests: 18 decisions (13 approved, 4 approved modified, 1 rejected; [header-requests.md](../porting/header-requests.md) "Wave 3a-2"):
  holder lookups ported into P4-09 together with their index hooks (MaterialData, ItemData, HouseBuildingData, WalkerVersionsData, SkillData,
  ItemRandomBonusData, TemperingData, RecipeData), the effect, condition, action and XML quest behaviour virtuals as layout changes of the P4-08
  lease, and `fieldmap.toml` decisions for `IgnoreProperties`, the `TempVars`/BIH stack and the IdianStone observer capture.
- Deviations: [P4-04.md](../deviations/P4-04.md), [P4-07b.md](../deviations/P4-07b.md), [P4-08.md](../deviations/P4-08.md),
  [P4-13.md](../deviations/P4-13.md).
- Left open (follow-ups): the liveness of a replaced IdianStone, race tests that need `AION_PCT`, MotionTime with equipped weapons,
  `XMLStartCondition::check` without a test, override declarations of the `EffectTemplate` virtuals in the other effect shells.

## Wave 3b-1 (2026-09-15)

Workflow `build/workflows/game-server-phase4-wave-3b1-m4.js`: 4 port lanes (holders P4-09, world P4-10, controllers P4-11b, dao P4-14), 4
adversarial reviewers, 1 header request integrator, 2 grouped fixers, the M4 gate integrator and an independent M4 verifier.

### What was ported

| Lane | `AION_UNPORTED` 3a-2 → now | Ported |
|---|---|---|
| holders (P4-09) | 88/71 → 8/7 | the 97 holders with their `afterUnmarshal` indexes; `StaticData` (the 90 "Loaded N ..." lines in Java wording and order); `DataManager` init with the strict load, the retirement rule and the post-processing (`ItemData.cleanup`, `GlobalDropData.processRules`, `validateBuyLists`, `validateMotions`, `DecomposeAction` ids, `NpcData.init`); Java `HashMap` iteration order where callers see it (`JavaHashMapOrder`, including `computeIfAbsent` head insertion and a treeification report); the ingame shop and schedule config roots. Left: the //reload setters (D3), write-back, `MotionData::calculateAnimationTimesAfterLastHit` (P5-02) |
| world (P4-10) | 278/22 → 6/6 | World, WorldMap, WorldMapInstance (2D/3D, factory), MapRegion, zones and ZoneService (zone instances, material zones, zone handlers from the registry), ZoneName, KnownList with the striped pair lock (`addPair` and the two-sided removals), the spawn engine bodies that need no game loop, the GeoService facade, respawn/weather/game time services, movement and FIFO task managers, `SiegeLocation` getters (M4 path exception). Left: `detachInstanceHandler` (P5-13), `WalkerGroup::targetReached` (P5-05), three spawners waiting for P4-11a/b constructors, `saveMaterialZones` |
| controllers (P4-11b) | 263/25 → 30/2 | VisibleObject, Creature, Npc, Player, Summon, House and Fly controllers, the movement controllers, ObserveController and the observer family, AbstractMaterialSkillActor, GatherableController (except the GatheringTask part); Java exception types instead of unsafe dereferences (`ControllerSupport.h`). Left: 26 stand-ins for providers without headers, 4 GatheringTask bodies |
| dao (P4-14) | 313/56 → 2/1 | the 56 DAOs with verbatim SQL, batches, transactions, generated keys, blobs and NULL handling, the scrollable `getUsedIDs` queries, Java `String.hashCode` record hashes; round trips against a fresh `aion_gs.sql` schema; `DaoSqlParityTest` compares setter indexes and getter columns with the Java sources. Left: `CustomInstancePlayerModelEntryDAO` (P5-13) |

### Review, header requests, fixes

- Header requests: 12 lane requests plus prev-1 (12 approved, 1 rejected; [header-requests.md](../porting/header-requests.md) "Wave 3b-1"), the
  integrator-owned change requests (realdata labels, smoke test, `fieldmap.toml`, stale test expectations), and the M4 gate's own additive
  changes m4-1..m4-4 ("Wave 3b-1, M4 gate").
- holders-world fixer: 16 findings fixed or answered with evidence, among them the KnownList pair lock (a forced interleaving fails on the old
  handshake), the Java `HashMap` order of four map getters and of `computeIfAbsent`, strict loading at startup (kept: Java validates changed
  data against the XSDs and does not start on an error), the walker formations of `SpawnEngine.spawnInstance` and coordinate logs through
  `JavaFloat::toString`. Its full verification passed (1,821 of 1,821 tests).
- controllers-dao fixer: 13 findings (11 fixed, 2 partly): client-chosen quest ids, Java exception types instead of crashes, 10 stand-ins
  replaced by the real providers, the `PlayerQuestListDAO` D6 fix, the bind parity test. Its final verification was cut short (build 1 still
  running); the M4 gate's final verification below covers the resulting tree.
- Deviations: [P4-09.md](../deviations/P4-09.md), [P4-10.md](../deviations/P4-10.md), [P4-11b.md](../deviations/P4-11b.md),
  [P4-14.md](../deviations/P4-14.md), [P5-14.md](../deviations/P5-14.md) (the startup path of `main.cpp`).
- Open from the fixers: MapRegion zone priority, `FlagKnownList.update`, the awareness filters, TemporarySpawnEngine and WeatherService have no
  tests (no Creature/Player builders in the world tests); `WorldRealDataTest` does not check shield attachment or handler order;
  `AbstractPeriodicTaskManager` schedules from the base constructor (accepted risk); no geo or house fixture tests for `teleportNearHouseDoor`,
  `trySetValidGeoPoint` and `AbstractCollisionObserver::moved`; the bind parity test does not catch two swapped values of the same type.

### M4 gate changes

- `src/main.cpp`: Java `GameServer.main` order through `World` (Logging, `Config.load`, `DatabaseFactory.init`, `PlayerDAO.setAllPlayersOffline`,
  cleaning if enabled, ThreadPoolManager/CronService/IDFactory with the eight `getUsedIDs` sources, DataManager, `ZoneService.init`,
  `GeoService.init`, World), one STARTUP task scope per step, the load time and peak working set logged, and the check options
  `--check-static-data`, `--check-id-factory`, `--check-output`, `--check-geo-probes` (deviations in [P5-14.md](../deviations/P5-14.md)).
- `GeoCallbacks::createMaterialZone` default ported (P4-04 has no `AION_UNPORTED` left); the additive accessors m4-1..m4-3 for the report files.
- `ForkJoinPool::runIndexed` (P4-02b): the elements of a STARTUP or SHUTDOWN caller keep the caller's task kind. Without it the first full run
  reported every terrain decoding element as a watchdog STALL (a Debug build decodes a PNG for more than 60 s), wrote a minidump per thread and
  hung inside the 45th in-process `MiniDumpWriteDump` (idle process, no progress for minutes; open item).
- CTest `gs.m4.check_static_data` (`cmake/RunM4Check.cmake`, labels `m4;oracle;realdata`, database helper `aion_gs_m4_database` in `tests/m4`), and
  `gs.smoke.startup` on its own `aion_gs_test_smoke` schema with the geo data switched off; both hold the `aion_game_server_log` resource lock.
- `tools/oracle/geo/m4.py` (`python -m geo m4-probes|m4-compare`) with unit tests; `loader.py` records the material id of each material zone.

## Milestone M4: all static data loads (2026-09-15)

**Passed**, measured on the wave 3b-1 working tree in `build/w3b1-m4` (Debug, checked build) by CTest `gs.m4.check_static_data`
(`cmake/RunM4Check.cmake`), which runs `aion_game_server` in `game-server/` twice against a fresh `aion_gs_test_m4` schema created from
`game-server/sql/aion_gs.sql`, then the independent oracles. Item by item (handlers-and-porting-plan.md §2.7):

| Item | Check | Result |
|---|---|---|
| 1 Config | `Config.load` binds the 35 classes (33 GS + CommonsConfig + DatabaseConfig) | No "is unknown and therefore ignored" warning. No config oracle exists, so the set is recorded: empty. A scratch prediction from the Java `@Property`/`@Properties` annotations of the 35 classes, the keys of `config/{administration,main,network}/*.properties` and the `logback.xml` variables (`build/w3b1-m4-scratch/config_oracle.py`: 427 keys, 5 patterns, 601 file keys) also gives the empty set |
| 2 DB, IDFactory | `DatabaseFactory.init`, `PlayerDAO.setAllPlayersOffline`, IDFactory from the eight `getUsedIDs` queries | Empty schema (`--check-id-factory`): "IDFactory: 1 IDs used." (id 0). Fixture schema (2 online players and 9 rows with object ids in inventory x3, player_registered_items, legions, mail, guides, houses, player_pets): "IDFactory: 12 IDs used." (id 0 and the 11 fixture ids); afterwards no player is online |
| 3 static data | DataManager strict load of all imports with hooks and post-processing | "Loaded 92 static data import(s) from 664 file(s)", 0 errors; warnings only from the reviewed exceptions (`[lenient_enums]` `SkillTemplate.counterSkill` comma lists) and the binder's empty-string notes (`MailTemplate@name`, `ShoutGroup@client_ai`), plus Java's own post-processing warnings (missing trade lists, trade-in lists, motion times) |
| 4 counts | the 90 "Loaded N ..." lines (92 numbers) in `static_data_counts.txt`, written by a file sink on the `StaticData` logger | `oracle.py compare-counts`: counts equal (102,009 items, 63,287 npcs, 13,570 skills, 8,043 quests, 3,978 zones, 6,449 walkers, 12,494 recipes, 161 maps, ...); XML quests (not logged by Java) 4,184 distinct ids = oracle extras. The anchor "22,022 spawn groups / 131,896 spots" (research/static-data-jaxb.md) is no logged number and not in the committed oracle: Java logs "Loaded 140 spawn maps entries" (equal); the spawns import of today's data has 229 `spawn_map` (140 distinct maps), 21,793 `spawn` and 131,292 `spot` elements |
| 5 geo | `GeoService.init` on the real path; `python -m geo m4-compare` | 18,583 mesh entries, 25,437 meshes, 20,028 mesh names, 151 `.geo` files, 419,707 placements, 0 missing meshes, 484,111 placement geometries, 420,626 attached nodes, 485,030 geometries, 7,961 material geometries, 89 terrain maps = `geo_expected.json`; 7,961 material zone names (7,961 distinct, digest `cde5c768fd7000bf`, 50 of 50 samples); 200 `getZ` probes bit-equal (180 surface hits) |
| 6 world, zones | `World` and `ZoneService.getZoneInstancesByWorldId` for every map instance | "World: 161 world maps created."; 161 map instances (twin counts with the configured maxima), 12,096 zone instances; the zone names of every instance equal the prediction from the zone XML (3,978 zones with an area), the whole-map zones (161) and the material zones with a material template or a registered shield (7,957: the 7,961 geometries minus the 4 `IGNORED_SHIELDS_BY_MAP_ID`) |
| 7 unported | unported trace of the run | 0 `AION_UNPORTED` hits (`unported_trace.txt` lists no site); exit code 0, no ERROR line, "Runtime shut down: 0 tasks left, ..., reclaimer backlog 0, 0 objects still tracked" |

Load time and peak working set (informational): Debug build, 32 hardware threads: startup path 260.8 s alone (279.6 s inside the parallel
ctest run) (static data 9.5 s, terrains about
91 s, meshes 5 s, placements about 126 s, collision trees and World about 29 s), peak working set 5,237 MB (5,212 MB in the ctest run). The Reclaimer reports a backlog
high-water mark while the geo load and World creation run in their startup scopes (1.35M and 4.6M retired objects, logged as a WARN dump);
it is empty at shutdown. A Release/RelWithDebInfo measurement is still to be taken.

Final verification of the gate: fresh configure (`--fresh`) and two full Debug builds with 0 warnings and 0 errors (`aion_gs_header_check` is
part of ALL); `lint_concurrency.py --werror --cycles=core game-server/src` 3,385 files, 0 errors, 0 warnings; drift checks clean
(`skeleton.py --fwd --check`, `fieldmap.py --check`, `chunks.py check` and `verify-json`, `xmlgen.py check`, `oracle.py check`,
`python -m geo check`); full ctest (`-j 6`) with the three database variables: 1,823 of 1,823 tests passed in 747 s (5 disabled benchmark
and stress tests not run; the login server database tests skip without `AION_TEST_LS_DATABASE_URL`), `gs.m4.check_static_data` 300 s,
`gs.smoke.startup` 23 s. Mutations of the report files (one count line, one zone name, one probe, the XML quest count) make both comparisons
fail.

### Open items after M4

| Item | Owner |
|---|---|
| Watchdog: an in-process `MiniDumpWriteDump` hung the server after about 45 STALL dumps in one check (all threads of a parallel load reported at once). The stall exemption now covers startup fork-join elements, but a burst of dumps can still happen elsewhere: dump once per check, or write the minidump from a helper process | P4-02 kernel |
| Startup memory: 5.2 GB peak working set in Debug, with millions of retired objects waiting for the startup scopes to end; a quiescent point per map in `GeoWorldLoader.loadWorld` and in `World` creation would let the Reclaimer run during the load | P4-04, P4-10 |
| Load time and peak working set in RelWithDebInfo (the design's PORTING_STATUS record) | next integrator |
| Handler engines (QuestEngine, AIEngine, InstanceEngine, ChatProcessor) are not initialized at startup; `DatabaseCleaningService.deletePlayersOnInactiveAccounts` is unported (cleaning is disabled by default) | P5-05, P5-06, P5-13, P5-14 |
| The deviation fragments of phase 4 (docs/deviations/*.md) are not merged into DEVIATIONS.md yet | documentation stage |
