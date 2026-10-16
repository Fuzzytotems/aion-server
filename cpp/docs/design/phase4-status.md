# Phase 4 status

> Phase 4 ports the bodies behind the frozen spine (tag `spine-v1`) towards milestone M4, "all static data loads"
> ([handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.6, §2.7, amendments §9). Waves: 3a-1 (commit `a99ec5fcb`), 3a-2 (commit
> `fee633404`), 3b-1 (P4-09 holders, P4-10 world, P4-11b controllers, P4-14 DAOs and the M4 gate; commit `58b6527d7`, tag `milestone-m4`),
> 3b-2 (P4-16/P4-17 server packets, runtime hardening and M4 performance; working tree of 2026-09-15). **M4 passed** (section "Milestone M4").
> **Phase 4 is done**; what phase 5 inherits is in section "Phase 4 close-out". Header request decisions:
> [../porting/header-requests.md](../porting/header-requests.md). The per-chunk deviation fragments of phase 4 are merged into
> [../DEVIATIONS.md](../DEVIATIONS.md) (each file in [../deviations/](../deviations/) now points to its section). Earlier steps:
> [wave1-status.md](wave1-status.md), [spine-status.md](spine-status.md).

## Chunks

`AION_UNPORTED` counts `AION_UNPORTED(` occurrences in the files of `chunks.py files <chunk>` (sources and headers, tests excluded), given as
occurrences/files, at `spine-v1` → `a99ec5fcb` (3a-1) → `fee633404` (3a-2) → `58b6527d7` (3b-1) → final (the wave 3b-2 working tree,
measured at the close-out with `chunks.py files <chunk>` and a count of `AION_UNPORTED(`). Numbers at commits use today's file lists, so files
added later count as 0 there; P4-08 counts every file of its lease (shells of P5-02..06). Tests are the `TEST*` macros in the chunk's test
directory. Declared but undefined functions (no `AION_UNPORTED`, they would not link) are not counted: the StaticObject, StaticDoor, FlyRing,
Road and Gatherable constructors and the GatherableController constructor (section "Phase 4 close-out").

| Chunk | Target | Wave | Status | `AION_UNPORTED` spine-v1 → 3a-1 → 3a-2 → 3b-1 → final | Tests | Still needs (from chunk) |
|---|---|---|---|---|---|---|
| P4-01 | `aion_gs_configs` | 1 | done | 0 | `tests/configs` 52 | optional `gameserver.watchdog.*` keys for the minidump settings |
| P4-02a/b | `aion_gs_runtime_*`, `aion_gs_handler_registry` | kernel, 1; 3b-2 hardening | done | 3/1 (the macro and its doc comment in `Unported.h`, no stubs) | `tests/runtime` 454; `tests/handler_registry` 5 | kernel open items ([runtime-kernel-status.md](runtime-kernel-status.md)) |
| P4-03 | `aion_gs_geomath` | 1 | done | 0 | `tests/geomath` 77 | – |
| P4-15a | `aion_gs_network_crypt` | 1 | done | 0 | `tests/network_crypt` 27 | – |
| T2 / T2-gen | `aion_gs_xml` / `aion_gs_staticdata` | 1 | done (generated) | 0 | `tests/xml` 60 | – |
| P4-04 | `aion_gs_geo` | 3a-2 | done | 87/9 → 87/9 → 1/1 → 0 → 0 | `tests/geo` 40 | `SiegeService.getSiegeLocation` behind the SHIELD default callback (P5-12a; no loaded node is a SHIELD) |
| P4-05 | `aion_gs_base` | 3a-1 | done | 56/5 → 7/6 → 5/5 → 5/5 → 5/5 | `tests/base` 59 | `PlayerStatCalculator` (P5-01), `ChatProcessor.h` (P5-14), CAPTCHA rasterizer decision; `SellLimitInfo` and `GameTime::onHourChange` are unblocked (`RatesInfo.h`, `TemporarySpawnEngine.h` exist) |
| P4-06 | `aion_gs_sysmsg` | 3a-1 | done | 4/1 → 0 → 0 → 0 → 0 | `tests/sysmsg` 13 | stand-in l10n helpers → `ChatUtil`/`RaceInfo`, `AbyssRankEnum` companion (P5-01) |
| P4-07a | `aion_gs_templates` | 3a-1 | done | 39/12 → 6/5 → 2/1 → 0 → 0 | `tests/templates/P4-07a` 51 | item actions (P5-07) |
| P4-07b | `aion_gs_templates` | 3a-2 | done | 26/20 → 26/20 → 0 → 0 → 0 | `tests/templates/P4-07b` 27 | – |
| P4-08 | lease over P5-02..06 shells | 3a-2 | data side done | 257/182 at 3a-2, 3b-1 and final (behaviour methods for P5-02..06) | `tests/skills` 18, `tests/quest` 8 | override declarations of the `EffectTemplate` virtuals in the other effect shells (decision open since 3a-2) |
| P4-09 | `aion_gs_dataholders` | 3b-1 | done | 83/79 → 98/81 → 88/71 → 8/7 → 8/7 | `tests/dataholders` 48 | //reload setters (D3), write-back, `MotionData::calculateAnimationTimesAfterLastHit` (`ChargeSkill.h`, P5-02) |
| P4-10 | `aion_gs_world` | 3b-1 | done | 277/22 → 278/22 → 278/22 → 6/6 → 6/6 | `tests/world` 25 | `detachInstanceHandler` (P5-13), `WalkManager.h` (P5-05), the StaticDoor/StaticObject constructors (P4-11a) and GatheringTask (P5-02) for the spawners, `saveMaterialZones` (write-back) |
| P4-11a | `aion_gs_objects` | 3a-1 | done | 366/23 → 24/11 → 22/10 → 22/10 → 22/10 | `tests/objects` 36 | stat containers (P5-01), `NpcSkillTemplateEntry` (P5-02); 14 stubs and 4 undefined constructors are unblocked by phase 4 providers |
| P4-11b | `aion_gs_controllers` | 3b-1 | done | 262/25 → 263/25 → 263/25 → 30/2 → 30/2 | `tests/controllers` 31 | 26 stand-ins in `ControllerStandIns.cpp` for P5-01/02/05/10/12b/13/14 providers without headers; `GatheringTask` (P5-02) for 4 `GatherableController` bodies |
| P4-12 | `aion_gs_player` | 3a-1 | done | 378/37 → 9/5 → 9/5 → 9/5 → 9/5 | `tests/player` 27 | `ExpireTimerTask` (P5-14), `TitleChangeListener`/`ItemEquipmentListener`/`PlayerGameStats` (P5-01); the soul-binding response is unblocked (`ItemUseObserver.h` exists) |
| P4-13 | `aion_gs_items` | 3a-2 | done | 176/22 → 176/22 → 0 → 0 → 0 | `tests/items` 32 | – |
| P4-14 | `aion_gs_dao` | 3b-1 | done | 313/56 → 313/56 → 313/56 → 2/1 → 2/1 | `tests/dao` 61 (MariaDB, skipped without `AION_TEST_GS_DATABASE_URL`) | `PlayerModelEntry.h` (P5-13) for `CustomInstancePlayerModelEntryDAO` |
| P4-15 | `aion_gs_network` | 3a-1 | done | 56/5 → 24/8 → 24/8 → 24/8 → 24/8 | `tests/network` 42 | score writers (P5-13), `StatEnum` companion (P5-01), `GameServer.h` (P5-14); `SM_KEY` and the LS packets landed in 3b-2 |
| P4-16 | `aion_gs_sm_ak` | 3b-2 | done | 174/111 → 174/111 → 174/111 → 174/111 → 6/6 | `tests/sm_ak` 47 (4 skip on other chunks' bodies) | `Influence.h` (P5-12a), `PvPArenaScore.h` (P5-13), `PlayerAllianceMember.h`/`LeagueMember.h` (P5-10), `AuctionEndTask.h` (P5-11); enum companions for the local tables |
| P4-17 | `aion_gs_sm_lz` | 3b-2 | done | 208/127 → 208/127 → 208/127 → 208/127 → 0 | `tests/sm_lz` 58 | P5 model and service bodies behind `detail/PacketLookups.h` and for golden tests; `GameServer.h` (P5-14), `CM_CHARACTER_EDIT` (P5-15) |
| P5-14 (`main.cpp` only) | `aion_game_server` | M4 gate, 3b-2 | M4 startup path | 0 in `main.cpp` (the rest of P5-14: 105/22) | `gs.smoke.startup`, `gs.m4.check_static_data` | the rest of `GameServer.main` (P5) |

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

## Wave 3b-1 (2026-09-15, commit `58b6527d7`, tag `milestone-m4`)

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
| 2 DB, IDFactory | `DatabaseFactory.init`, `PlayerDAO.setAllPlayersOffline`, IDFactory from the eight `getUsedIDs` queries | The fresh schema has 61 tables (the 61 `CREATE TABLE` statements of `aion_gs.sql`; `update.sql` adds nothing it lacks; the gate's own notes said 62). Empty schema (`--check-id-factory`): "IDFactory: 1 IDs used." (id 0). Fixture schema (2 online players and 9 rows with object ids in inventory x3, player_registered_items, legions, mail, guides, houses, player_pets): "IDFactory: 12 IDs used." (id 0 and the 11 fixture ids); afterwards no player is online |
| 3 static data | DataManager strict load of all imports with hooks and post-processing | "Loaded 92 static data import(s) from 664 file(s)", 0 errors; warnings only from the reviewed exceptions (`[lenient_enums]` `SkillTemplate.counterSkill` comma lists) and the binder's empty-string notes (`MailTemplate@name`, `ShoutGroup@client_ai`), plus Java's own post-processing warnings (missing trade lists, trade-in lists, motion times) |
| 4 counts | the 90 "Loaded N ..." lines (92 numbers) in `static_data_counts.txt`, written by a file sink on the `StaticData` logger | `oracle.py compare-counts`: counts equal (102,009 items, 63,287 npcs, 13,570 skills, 8,043 quests, 3,978 zones, 6,449 walkers, 12,494 recipes, 161 maps, ...); XML quests (not logged by Java) 4,184 distinct ids = oracle extras. The anchor "22,022 spawn groups / 131,896 spots" (research/static-data-jaxb.md) is no logged number and not in the committed oracle: Java logs "Loaded 140 spawn maps entries" (equal); the spawns import of today's data has 229 `spawn_map` (140 distinct maps), 21,793 `spawn` and 131,292 `spot` elements |
| 5 geo | `GeoService.init` on the real path; `python -m geo m4-compare` | 18,583 mesh entries, 25,437 meshes, 20,028 mesh names, 151 `.geo` files, 419,707 placements, 0 missing meshes, 484,111 placement geometries, 420,626 attached nodes, 485,030 geometries, 7,961 material geometries, 89 terrain maps = `geo_expected.json`; 7,961 material zone names (7,961 distinct, digest `cde5c768fd7000bf`, 50 of 50 samples); 200 `getZ` probes bit-equal (180 surface hits) |
| 6 world, zones | `World` and `ZoneService.getZoneInstancesByWorldId` for every map instance | "World: 161 world maps created."; 161 map instances (twin counts with the configured maxima), 12,096 zone instances; the zone names of every instance equal the prediction from the zone XML (3,978 zones with an area), the whole-map zones (161) and the material zones with a material template or a registered shield (7,957: the 7,961 geometries minus the 4 `IGNORED_SHIELDS_BY_MAP_ID`) |
| 7 unported | unported trace of the run | 0 `AION_UNPORTED` hits (`unported_trace.txt` lists no site); exit code 0, no ERROR line, "Runtime shut down: 0 tasks left, ..., reclaimer backlog 0, 0 objects still tracked". The LeakCensus part ("0 objects still tracked") is trivially satisfied on the M4 path: LeakCensus tracks only objects removed from the world, and M4 never spawns or removes one; the check stays as a guard for M5a. The orderly shutdown (tasks, cleaner ids, reclaimer backlog) is a real check |

Load time and peak working set (informational): Debug build, 32 hardware threads: startup path 260.8 s alone (279.6 s inside the parallel
ctest run) (static data 9.5 s, terrains about
91 s, meshes 5 s, placements about 126 s, collision trees and World about 29 s), peak working set 5,237 MB (5,212 MB in the ctest run). The Reclaimer reports a backlog
high-water mark while the geo load and World creation run in their startup scopes (1.35M and 4.6M retired objects, logged as a WARN dump);
it is empty at shutdown. RelWithDebInfo measurements and the wave 3b-2 performance work: section "Wave 3b-2"; all timings side by side:
section "Phase 4 close-out".

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
| ~~Watchdog: an in-process `MiniDumpWriteDump` hung the server after about 45 STALL dumps in one check~~ **Resolved in wave 3b-2** (one STALL dump per check, snapshot minidumps from a helper process with a timeout and a rate limit; section "Wave 3b-2") | P4-02 kernel |
| ~~Startup memory: millions of retired objects waiting for the startup scopes to end~~ **Resolved in wave 3b-2** (cause found and removed or reclaimed during the load; backlog high-water mark 11,984,161 → 231,615 objects in RelWithDebInfo) | P4-02 (runtime lane, geo/world performance exception) |
| ~~Load time and peak working set in RelWithDebInfo~~ **Measured in wave 3b-2** (section "Phase 4 close-out") | – |
| ~~`RectangleArea.getDistance3D` creates a `Ref<Point3D>` per 3D region/zone test (11.1 million in World creation)~~ **Resolved in wave 3b-2** (the header integrator computes the clamped point as values in `RectangleArea`/`PolyArea`; World 2.7-3.5 s → 804 ms in RelWithDebInfo) | P4-05 |
| The M4 path in RelWithDebInfo emits 61 C4702 (unreachable code after `AION_UNPORTED`) in stub files of other chunks (`BaseService.cpp`, `PeriodicInstanceManager.cpp`, SM packets); Debug builds are warning-free | the owning chunks (phase 5) |
| Handler engines (QuestEngine, AIEngine, InstanceEngine, ChatProcessor) are not initialized at startup; `DatabaseCleaningService.deletePlayersOnInactiveAccounts` is unported (cleaning is disabled by default) | P5-05, P5-06, P5-13, P5-14 |
| ~~The deviation fragments of phase 4 are not merged into DEVIATIONS.md yet~~ **Merged at the phase 4 close-out** | – |

## Wave 3b-2 (2026-09-15, working tree)

Workflow `build/workflows/game-server-phase4-wave-3b2.js`: 3 lanes (sm-a P4-16, sm-b P4-17, runtime), 3 adversarial reviewers, 1 header request
integrator (also reviewing the M4 gate accessors m4-1..m4-3), 2 fixers (packets, runtime) and this close-out documentation stage.

### What was ported

| Lane | `AION_UNPORTED` | Ported |
|---|---|---|
| sm-a (P4-16) | 174/111 → 6/6 | the 112 packets SM_A* .. SM_K*: every `writeImpl`, every constructor that computes data, the private write helpers (`SM_BROKER_SERVICE`, `SM_FIND_GROUP`, `SM_INVENTORY_*`, `SM_CUSTOM_PACKET` element writers with Java's `Integer.decode`/`Long.decode`/`Float.valueOf`) and the size calculators of `SM_HOUSE_SCRIPTS` and `SM_BROKER_SERVICE`; 4 companion headers for the chunk's nested enums; the 17 PER_RECIPIENT packets override `recipients()` (lint L10), `SM_KEY` stays SHARED. Left: 6 sites waiting for headers of P5-10, P5-11, P5-12a, P5-13 |
| sm-b (P4-17) | 208/127 → 0 | the 125 packets SM_L* .. SM_Z* (without `SM_SYSTEM_MESSAGE`), `AbstractPlayerInfoPacket` and `AbstractHouseInfoPacket`; service, DAO and stat calls behind `serverpackets/detail/PacketLookups.h` (tests install replacements, the server runs the Java expressions); one additive `SM_MAIL_SERVICE::writeLetterDelete(span)` overload; bug found by the golden tests: `SM_TRANSFORM_IN_SUMMON(Player&, Creature&)` never set `player` |
| runtime (P4-02 and the geo/world performance exception) | 0 in touched files | watchdog STALL dumps and snapshot minidumps (`MinidumpWriter`), Reclaimer backlog causes removed, `BIHTree`/`PngImage`/terrain/mesh loading rewrites (digest-verified), World creation in `PER_ELEMENT` jobs, M4 check-mode step timings (`m4_summary.txt`, `startup_timeline.txt`), the 3b-1 verifier findings (below) |

Golden-byte tests are written by hand from the Java `writeImpl`s: `tests/sm_ak` 47 (43 pass, 4 skip on unported bodies of other chunks:
`PlayerSkillEntry::isNormalSkill`, `ConquerorAndProtectorService::getCPInfoForCurrentMap`, `CreatureLifeStats::getHpPercentage`,
`PlayerScript::hasData`), `tests/sm_lz` 58, including opcode tables checked against `ServerPacketsOpcodes.java` for all fixed-opcode packets.

### Review, header requests, fixes

- Review: packets 11 findings (sm-a: no byte-level divergence found; test and deviation-text findings), runtime 6 findings.
- Header requests ([header-requests.md](../porting/header-requests.md) "Wave 3b-2"): no frozen declaration changed. sm-b-1
  (`SM_SIEGE_LOCATION_INFO.locations` as a vector) and sm-b-2 (ordered members for four hash-ordered packets) rejected; m4-1..m4-3 kept, each
  now with a unit test. Change requests applied: `AionConnection::initialized` sends the ported `SM_KEY`; the stale
  `CreatureControllerTest.OnDespawnCancelsTheDecayTaskAndStopsMovement` expectation (the npc gets an `NpcKnownList` as in `SpawnEngine`);
  value-based `RectangleArea`/`PolyArea::getDistance3D` (P4-05); runtime-architecture.md §4.3 and the "Deadlock detection" row.
- Packets fixer: 8 findings fixed (`SM_BROKER_SERVICE` with an empty item list throws `NullPointerException` like Java's null field, literal
  expected bytes, `BoundRadius.DEFAULT`, comments, the `SM_ITEM_COOLDOWN` deviation reason), the duplicated enum tables of both lanes merged into
  `serverpackets/detail/PacketSupport.h` with `PacketSupportTest`, the P4-16/P4-17 deviation texts corrected; golden tests for the effect,
  attack and house packets deferred to P5. **Its verification did not finish** (first Debug build still running; see "Final verification").
- Runtime fixer: 6 of 6 fixed with tests: DEADLOCK minidumps skip the interval and the STALL budget (own budget of `maxMinidumps`); the text
  dump is logged before the minidump is written, write failures become a log line, paths are UTF-8; `QuiescentOptIn(WORLD_CREATION)` so that
  only World creation takes quiescent points below `main.cpp`'s scope; unique M4 schema per build directory
  (`aion_gs_test_m4_<md5 prefix>`, dropped on failure); `Watchdog.h` LF; helper crash exit codes and the usage message.

### Final verification

- Header integrator stage (after the lanes, before the fixers): fresh configure, two Debug builds, lint 3,394 files clean, full ctest (`-j 6`,
  database variables) **1,937 of 1,937 passed** in 721 s, `gs.m4.check_static_data` 148.9 s, `gs.smoke.startup` 21.2 s.
- Runtime fixer: RelWithDebInfo M4 run after the P4-05 change (section "Phase 4 close-out") and the Debug `gs.m4.check_static_data` inside a
  full ctest (151.3 s); no total was reported.
- Packets fixer: fresh configure, lint and `chunks.py check` clean on its tree before its last edit (`SM_GM_SHOW_LEGION_MEMBERLIST.cpp`);
  build 1 unfinished, no header check, lane tests or full ctest. **The final tree of wave 3b-2 still needs the full verification** (fresh
  configure, two Debug builds, `aion_gs_header_check`, lint, drift checks, full ctest with the database variables) before it is committed.

## Wave 3b-2: runtime lane details

Runtime hardening and M4 performance: the kernel (P4-02) plus, for performance only, `geoEngine/**` and `world/**` (results bit-identical,
checked by the oracles), `main.cpp` check-mode reporting and the M4/smoke test registration. Deviations: [../DEVIATIONS.md](../DEVIATIONS.md)
"game-server / runtime kernel" (watchdog rows), "geo engine", "world" and "startup".

### Watchdog dumps (D5: dump and keep running)

- One dump per check for stalls: every task that stalls in the same check is named in one STALL dump ("N stalled tasks:").
- Minidumps come from a process snapshot (`PssCaptureSnapshot`, virtual address clone), so no live thread stays suspended while DbgHelp works:
  written by a helper process (`aion_game_server --write-minidump <pid> <file>`, `runtime/sync/MinidumpWriter.h`), which is terminated
  after 30 s; an executable without the helper mode uses an in-process writer thread, which is abandoned after the timeout.
- Rate limit: at most one minidump per check (later dumps of the check name it), none within 60 s of the previous attempt, at most 20 per
  run. The text dump of all threads is always logged.
- Tests (`tests/runtime/sync`): a burst of 40 simultaneous stalls gives one STALL dump and one minidump per check, a probe's second dump in
  the same check names the first, the interval suppresses the next check's minidump, and the process keeps running; the helper dumps a real
  child process of the test executable (a minidump with its threads, and the child then exits normally, so it was not left suspended); a
  hanging helper is terminated after its timeout while the target keeps running; the in-process snapshot writer; argument parsing; the
  per-run limit. The server executable's helper mode was checked by hand on a running process (`MDMP` file in 0.5 s, Debug).

### Reclaimer backlog during the startup

Cause, measured with a temporary retire histogram by type and task site (RelWithDebInfo, M4 path; the Debug watchdog had seen 1.1M / 4.0M
objects at sampling time):

| Step | Retired objects | Source |
|---|---|---|
| geo (collision trees) | 3,123,492 `BoundingBox` | `BIHTree.createNode`: temporary RefCounted boxes (`createBox` for the planes, copies for the child boxes) |
| world | 11,109,573 `Point3D` | `SphereArea.intersectsRectangle` → `RectangleArea.getDistance3D` → `AbstractArea.getClosestPoint(x, y, z)` returns a `Ref<Point3D>`, once per 3D region/zone test outside the zone's z range (Reshanta's 3D regions) |
| world | 765,007 `Array<MapRegion*>` | `MapRegion.addNeighbourRegion` copies the neighbour array per neighbour (Java's `Arrays.copyOf`) |

All of it stayed in the backlog until the step's scope ended, because the main thread's STARTUP step scope was published for the whole step
(a JOIN fork-join caller stays published until its outermost scope ends) and the JOIN helpers adopted its scope. At the end of World creation
the backlog held 11,984,161 objects / 867 MB, and the shutdown spent about 3 s destroying them.

Fixes (the epoch rules unchanged: borrows stay valid until their scope ends or a quiescent point in a valid `QuiescentScope`):
- `BIHTree.construct` builds on a copy of the triangles with value boxes: no temporary boxes (tree digest `0f3c1f762e77dce4` equal before and after).
- `World`: the maps are created in a `PER_ELEMENT` job (each in its own task scope with a `QuiescentScope`); `main.cpp` opens a
  `QuiescentScope` around `World.getInstance()`, and the constructor unpublishes the caller before it waits. Reshanta is created first on the
  calling thread, and its regions in a `PER_ELEMENT` job whose elements wait (`Reclaimer::awaitBacklogBelow`) while the backlog is above half
  the watchdog's dump threshold; quiescent points between instances and neighbour rows.
- The `Point3D` churn itself was P4-05 code: removed by the header integrator's value-based `getDistance3D` (change request), after the
  measurements of this section (the later numbers are in "Phase 4 close-out").
- Runtime fixer: the quiescent points below `main.cpp`'s scope are taken only under `QuiescentOptIn(WORLD_CREATION)`, opened by the World step
  and by each `PER_ELEMENT` element of `World::World`.

### M4 in RelWithDebInfo: before and after

`gs.m4.check_static_data` on an idle machine (32 hardware threads), three runs each; "before" is the tree before the lane's geo/world changes
(`git show HEAD` versions of the 10 files, same kernel and `main.cpp` measurements), "after" is this wave. Step times from `m4_summary.txt`,
geo sub-steps from `startup_timeline.txt` (GeoWorldLoader log lines, milliseconds). Both pass the whole gate (counts, 200 bit-equal `getZ`
probes, material zone names, world zones).

| | Before (3 runs) | After (3 runs) |
|---|---|---|
| Startup path (Config .. World) | 10,037 / 10,420 / 11,013 ms | 5,807 / 6,474 / 7,685 ms |
| Static data (DataManager) | 1,516-1,532 ms | 1,542-1,954 ms (unchanged code) |
| Geo total | 4,445-4,672 ms | 1,502-2,224 ms |
| – terrains (PNG decode and `Terrain` arrays) | 1,110-1,130 ms | 520-640 ms |
| – meshes (`models.mesh`) | 790-930 ms | 330-410 ms |
| – placements (`.geo` files) | 330-340 ms | 270-520 ms |
| – collision trees | 2,200-2,290 ms | 400-670 ms |
| World | 3,987-4,790 ms | 2,718-3,459 ms |
| Peak working set | 3,570 MB | 1,668-1,678 MB |
| Reclaimer backlog high-water mark | 11,984,161 objects / 867 MB | 496,396-503,593 objects / 36 MB |
| Shutdown after the startup (runtime shutdown drain) | about 3.0-3.7 s | about 70 ms |

Profile (a scratch sampling profiler over the RelWithDebInfo run): before the changes the collision trees (`Mesh.getTriangle` through checked
`Field` loads, `BoundingBox` allocations) and Reshanta's serial region creation dominated; the terrain step was mostly per-element `Ref`
dereference checks in `Terrain.setHeightmap`, not PNG decoding; the mesh step read the 70 MB file byte by byte. After: World is Reshanta's
region/zone tests with their `Point3D` (limited by the Reclaimer's destruction rate) and static data is the largest remaining step.

Debug (the checked gate build), `gs.m4.check_static_data` inside the final full ctest of this wave (`-j 6`, a loaded machine): startup path
193.9 s (wave 3b-1: 279.6 s in ctest, 260.8 s standalone), peak working set 2,618 MB (5,212 / 5,237 MB); static data 51.1 s (under the
parallel test load; 11.0 s in the 3b-1 ctest run), terrains 15.3 s (about 91 s), meshes 1.9 s (5 s), placements 92.2 s (about 126 s),
collision trees 3.9 s, World 29.1 s (trees and World about 29 s together); backlog high-water mark 508,313 objects (1.1M geo / 4.0M World
seen by the 3b-1 watchdog); test time 216 s (300 s).

### Runtime lane verification

Fresh configure (`--fresh`) and two full Debug builds in `build/w3b2-runtime`: 0 warnings, 0 errors (`aion_gs_header_check` built);
`lint_concurrency.py --werror --cycles=core game-server/src`: 3,394 files, 0 errors, 0 warnings. Full ctest (`-j 6`, the three database
variables): 1,910 of 1,913 passed, `gs.m4.check_static_data` and `gs.smoke.startup` passed. The three failures: `gs.lint.concurrency` (an
`std::atomic` of this lane, replaced by `runtime::AtomicBoolean`) and `gs.chunks.consistency` (a test file another lane added after the
configure) pass after the fix and a reconfigure (third incremental build 0 warnings); `CreatureControllerTest.OnDespawnCancelsTheDecayTaskAndStopsMovement`
fails without a change of this lane in its path (`abortMove` now reaches an empty `KnownList` part slot of the test npc before the unported
`AggroList.clear`; a stale expectation, fixed by the header integrator).

### M4 verifier findings (wave 3b-1, low)

| Finding | Resolution |
|---|---|
| LeakCensus "0 objects still tracked" is vacuous on the M4 path | Stated in item 7 of the M4 table and in `RunM4Check.cmake`; the check stays as a guard for M5a |
| §2.7 spawn anchor 22,022 / 131,896 | handlers-and-porting-plan.md §2.7 now names the logged number (140 spawn maps) and today's data (21,793 spawns / 131,292 spots, informational) |
| The ERROR-line check missed an ERROR on the first log line | `RunM4Check.cmake` scans the log with a newline in front (checked with a scratch log whose first line is an ERROR) |
| `gs.m4.check_static_data` silently missing without Python | Always registered; without Python CMake passes `-DPYTHON=NOTFOUND` and the test reports "gs.m4.check_static_data: skipped (no Python 3 interpreter ...)" (SKIP_REGULAR_EXPRESSION) |
| 62 vs 61 tables | 61 (item 2 of the M4 table) |

## Phase 4 close-out (2026-09-15)

### M4 result

M4 ("all static data loads", handlers-and-porting-plan.md §2.7) passed in wave 3b-1 (tag `milestone-m4`) and keeps passing on the wave 3b-2
trees: every `gs.m4.check_static_data` run of wave 3b-2 in Debug (the checked gate build, inside full ctest runs) and RelWithDebInfo gave
counts, geo statistics, 200 bit-equal `getZ` probes, material zone names and world zones equal to the oracles, with 0 `AION_UNPORTED` hits,
no ERROR line and an orderly shutdown.

| Build and tree | Startup path | Static data | Geo | World | Peak working set | Reclaimer backlog high-water mark | `gs.m4` test time |
|---|---|---|---|---|---|---|---|
| Debug, 3b-1, standalone | 260.8 s | 9.5 s | about 222 s without the trees (terrains 91 s, meshes 5 s, placements 126 s) | trees and World about 29 s | 5,237 MB | 1.35M (geo) / 4.6M (World), watchdog samples | – |
| Debug, 3b-1, full ctest `-j 6` | 279.6 s | 11.0 s | – | – | 5,212 MB | – | 300 s |
| Debug, 3b-2 runtime lane, full ctest `-j 6` | 193.9 s | 51.1 s (loaded machine) | 113.3 s (terrains 15.3, meshes 1.9, placements 92.2, trees 3.9) | 29.1 s | 2,618 MB | 508,313 objects | 216 s |
| Debug, 3b-2 header integrator and runtime fixer trees, full ctest `-j 6` | – | – | – | – | – | – | 148.9 s / 151.3 s |
| RelWithDebInfo, before 3b-2 (3 runs) | 10,037-11,013 ms | 1,516-1,532 ms | 4,445-4,672 ms | 3,987-4,790 ms | 3,570 MB | 11,984,161 objects / 867 MB | – |
| RelWithDebInfo, 3b-2 runtime lane (3 runs) | 5,807-7,685 ms | 1,542-1,954 ms | 1,502-2,224 ms | 2,718-3,459 ms | 1,668-1,678 MB | 496,396-503,593 objects / 36 MB | – |
| RelWithDebInfo, 3b-2 with value-based `getDistance3D` and the runtime fixes (1 run) | 3,856 ms | 1,481 ms | 1,537 ms | 804 ms | 1,503 MB | 231,615 objects / 17.5 MB (World step) | – |

All on the same idle 32-thread machine except the ctest rows. Before/after details and profiles: section "Wave 3b-2: runtime lane details".

### Remaining unported bodies, by the chunk that unblocks them

Counts are `AION_UNPORTED` sites in phase 4 chunk files (table "Chunks"); the P5 chunks' own 105/22 (P5-14) and similar are not repeated.

| Needed from | Remaining bodies in phase 4 chunks |
|---|---|
| **Nothing (unblocked in phase 4, port first)** | P4-05 `SellLimitInfo::getSellLimit` (`RatesInfo.h`), `GameTime::onHourChange` (`TemporarySpawnEngine.h`); P4-11a 14 stubs: the `NpcData::getNpcTemplate` lookups of `SummonedObject`, `Trap`, `Kisk`, `SummonedHouseNpc`, `new NpcKnownList` in `Trap`/`Homing`/`Servant`, `HouseDecoration::getTemplate` (`HousePartsData::getPartById`), the `HouseObject` constructor lookup and `getPlacementLimit`, `UseableItemObject`'s placement limit (`PlaceableHouseObject::getPlacementLimit`, `LimitTypeInfo.h`), `Npc::canSell`/`canTradeIn`/`canPurchase` (`TradeListData`); the undefined `StaticObject`, `StaticDoor`, `FlyRing`, `Road` constructors (controller headers exist since 3b-1), then P4-10 `StaticDoorSpawnManager`/`StaticObjectSpawnManager::spawnTemplate`; P4-12 the `Equipment` soul-binding response (`ItemUseObserver.h`) |
| P5-01 stats | P4-05 `PlayerClassInfo::createStatsTemplate` (`PlayerStatCalculator`); P4-11a stat containers of `SummonedObject`, `Trap`, `Homing`, `Servant` (5); P4-11b stand-ins for `StatFunctions` and `AttackUtil`; P4-12 `ItemEquipmentListener` (2) and `TitleChangeListener` (2); `AggroList.clear` (reached by `CreatureController::onDespawn`); stat getters behind P4-17's `PacketLookups` and the life stats the 4 skipped sm_ak tests need; `StatEnum`/`AttackStatus`/`AbyssRankEnum` companions |
| P5-02 skills | P4-08 skill behaviour methods (share of 257/182); P4-09 `MotionData::calculateAnimationTimesAfterLastHit` (`ChargeSkill.h`); P4-11a `Npc::queueSkill` (3, `NpcSkillTemplateEntry`); P4-11b `GatherableController` (4) and the charge skill stand-in (`GatheringTask.h`, `ChargeSkill.h`), then the undefined `Gatherable`/`GatherableController` constructors and P4-10 `VisibleObjectSpawner::spawnGatherable`; `PlayerSkillEntry::isNormalSkill` (sm_ak test) |
| P5-03/P5-04 effects | P4-08 `EffectTemplate` behaviour (25 methods), the `applyEffect` overrides of the 110 effect shells, `WeaponDualEffect::hasDualWieldEffect` |
| P5-05 AI | P4-10 `WalkerGroup::targetReached` (`WalkManager.h`); P4-11b stand-ins for `AILogger`, `TargetEventHandler`, `WalkManager`, `ShoutEventHandler`, `FollowStartService` |
| P5-06 quests | P4-08 quest behaviour (`XMLQuest.register`, `QuestEvent.operate`, `QuestOperation.doOperate`, ...); `QuestVars::getQuestVars`, `QuestState::canRepeat` for P4-17's quest packets |
| P5-07 item services | P4-07a item actions (stripped while loading); companions `ItemDeleteType`/`ItemAddType`/`ItemUpdateType` (tables in `PacketSupport.h`) |
| P5-10 teams, legions | P4-16 `SM_ALLIANCE_MEMBER_INFO` constructor (`PlayerAllianceMember.h`) and `SM_ALLIANCE_INFO` league branch (`LeagueMember.h`); P4-11b stand-ins for `PlayerGroupService`, `PlayerAllianceService`, `PlayerTeamDistributionService`, `TeamMoveUpdater`, `TeamStatUpdater`; P4-09 `AutoGroupData` portal npc index (header request holders-1); `Legion`/`LegionMember` bodies for P4-17's legion packets; companions `TeamType`, `LootRuleType`, `GroupEvent`, `PlayerAllianceEvent`, `AutoGroupType`, `LegionRank`, `LegionHistoryAction` |
| P5-11 legion/house | P4-16 `SM_HOUSE_BIDS` remaining auction seconds (`AuctionEndTask.h`); `House` getters (`AbstractHouseInfoPacket`), `PlayerScript::hasData`, the `Town` constructor (needs a header request for `levelUpDate`); `PlayerScript.LUA_SANDBOX_FIX` |
| P5-12a siege | P4-16 `SM_INFLUENCE_RATIO`, `SM_FORTRESS_STATUS` (`Influence.h`); P4-04 SHIELD default callback (`SiegeService` constructor); `SiegeLocation::getLocationId` |
| P5-12b world events | P4-11b `RiftEnum` companion stand-in; `VortexLocation::getInvasionWorldId` (`VortexData::getVortexLocation` throws) |
| P5-13 instances | P4-15 the 8 instance score writers (24/8) and `DredgionScoreWriter` (`DredgionRoom.h`); P4-16 `SM_INSTANCE_SCORE(ArenaScoreWriter)` constructor (`PvPArenaScore.h`); P4-14 `CustomInstancePlayerModelEntryDAO` (2, `PlayerModelEntry.h`); P4-10 `WorldMapInstance::detachInstanceHandler`; P4-11b stand-ins for `PlayerRestrictions`, `PvpMapService` |
| P5-14 misc and app | P4-05 `GMService::onPlayerLogin` login commands (`ChatProcessor.h`); P4-12 `ExpireTimerTask` registrations (4: pets, emotions, motions, titles); P4-11b `GameServer.updateRatio`; P4-17 `SM_VERSION_CHECK` stand-ins (`GameServer.h`); the rest of `GameServer.main` after `World` |
| P5-15 client packets | P4-17 `SM_PLASTIC_SURGERY` ticket check copy (`CM_CHARACTER_EDIT`); `AbstractGmCommandPacket.h` for `ChatUtil`'s stand-ins |
| No owner, deferred by design | P4-09 //reload setters (`EventData`, `NpcSkillData`, `XMLQuests`; D3) and write-back (`SpawnsData.saveSpawn`, `WalkerData` 2, `ZoneData.saveData`); P4-10 `ZoneService.saveMaterialZones`; P4-05 `CAPTCHAUtil::createImage` (text rasterizer decision) |

### Open issues carried into phase 5

Verification and build:
- **Wave 3b-2 is not verified as a whole**: the last complete run (1,937 of 1,937) predates both fixers, and the packets fixer's build never
  finished. Run the full verification on the final tree before committing (section "Wave 3b-2", "Final verification").
- RelWithDebInfo: 61 C4702 warnings in stub files of other chunks (Debug is clean); they go away as the stubs are ported.
- `XmlUtilTest.ListFilesDoesNotFollowLinks` skips without the symlink privilege.

Runtime (details in [runtime-kernel-status.md](runtime-kernel-status.md)):
- The watchdog's check thread waits while the helper writes a minidump (up to `minidumpTimeout` + 5 s); text dumps have no stacks of other
  threads; the minidump settings have no config keys (P4-01, optional); no minidump on other platforms; the DEADLOCK minidump test assumes no
  earlier DEADLOCK minidump in its process (true under CTest).
- The single Reclaimer thread limits bulk producers of short-lived RefCounted objects (`awaitBacklogBelow` throttles them).
- Broadcasts serialize a SHARED packet per recipient, not once (runtime-architecture.md §8.3; performance only; P4-05 helper plus a P4-15
  enqueue path). S0B-039 send queue (deque with ordered insertion): measure first.

Packets and wire order:
- Hash-ordered packets are approximations: `SM_ITEM_COOLDOWN` ascending ids, `SM_NEARBY_QUESTS`/`SM_RECIPE_*`/`SM_TOWNS_LIST` through
  `JavaHashMapOrder` over ascending keys (sm-b-2); the client is not known to care. `SiegeService.locations` should become
  `runtime::LinkedHashMap` (P5-12a header request) to keep XML order for `SM_SIEGE_LOCATION_INFO` type 0.
- Golden-byte tests are missing where model objects cannot be built yet: `SM_ATTACK`, `SM_CASTSPELL_RESULT`, `SM_ABNORMAL_*` with effects,
  team, house, broker item, legion, quest list, skill list, pet, trade list, stats and summon packets; 4 sm_ak tests skip (see above). The
  expectations run as soon as the bodies exist.
- Local enum constructor tables (`serverpackets/detail/PacketSupport.h`, packet `.cpp` files, `network/detail/EnumIds.h`, `ItemData.h`,
  `SystemMessageL10n.h`, `ControllerSupport.h`) are replaced when the companion headers land.
- Client packet `readS` still turns lone surrogates into U+FFFD (server-to-client is lossless since 3a-2).

Model, world and data:
- `CompressUtil::decompress` rejects the output of `CompressUtil::compress` ("incorrect data check"), so house scripts loaded by
  `HouseScriptsDAO` are dropped; `LegionHouseItemDaoTest.HousesBidsAndScripts` skips (P4-05, reported in 3b-1). Open decision: keep the zlib port
  or link zlib.
- Test gaps: `MapRegion` zone priority, `FlagKnownList.update`, the awareness filters, `TemporarySpawnEngine`, `WeatherService` (no Creature
  builders in the world tests); shield attachment and zone handler order in `WorldRealDataTest`; `teleportNearHouseDoor`,
  `trySetValidGeoPoint`, `AbstractCollisionObserver::moved` (geo/house fixtures); `XMLStartCondition::check`, MotionTime with equipped weapons,
  NpcFactions quest methods, GMService/AuditLogger/AutoBan/PlayerActions (need a Player); the bind parity test misses two swapped values of the
  same type; `ConnectionAliveChecker` timing; GodStone/NpcEquippedGear race tests need `AION_PCT`.
- `AbstractPeriodicTaskManager` schedules from the base constructor (accepted risk); `ExpireTimerTask`, `LegionDominionIntruderUpdateTask`,
  `TemporaryTradeTimeTask`, `TeamMoveUpdater`, `TeamStatUpdater` must use the two-argument constructor; `AbstractFIFOPeriodicTaskManager::run`
  could move out of the template header (world-5).
- Handler engines are not initialized at startup; `DatabaseCleaningService.deletePlayersOnInactiveAccounts` is unported; staff logins throw
  while `LOGIN_EXECUTE_COMMANDS` is non-empty (P5-14 `ChatProcessor`).
- JAXBUtil callers outside static data (spawn write-back, rift/siege/world raid schedules, `InGameShopProperty`, `DatabaseCleaningService`)
  need binder entry points or a decision to drop XSD validation.
- P4-08: whether override declarations of the existing `EffectTemplate` virtuals in the effect shells are additive or header requests.
- Lifetime notes for P5 porters: `QuestDrop::setQuestId` is not called by `QuestsData` (P5-06 decides where); `OnKillEvent.operate` must not
  return early for an empty monster list; `HandlerSideDrop`/`FlyRingTemplate` are allocated once and never freed; `DropItem` keeps a
  `const Drop*` (the creator keeps the Drop alive); the `Effect` constructor used by `addSavedEffect` must copy `magicalCriticalPositions`.
- `IdianStone`'s checked accessor cannot detect a stone replaced without `onUnEquip`; `Creature::getTransformModel` stays `// java-race`
  (PartSlot has no CAS).
- Stale stand-ins to replace with the providers that exist now: `Kisk.cpp`/`SummonedHouseNpc.cpp`/`HouseObject.cpp`/`HouseDecoration.cpp`
  (above), `network/detail/EnumIds.h` and `SystemMessageL10n.h` (`ChatUtil`, P4-05 companions), the `GeoMap` WorldMapType/RegionUtil tables,
  the `WorldMapTemplate` zone attribute ids, local `simpleClassNameOf` helpers.
- Lint L19 accepts `[fields]` but not `[cpp_members]` spellings (`ZoneService.worldZoneTemplates` keeps a `// confined:` waiver).
