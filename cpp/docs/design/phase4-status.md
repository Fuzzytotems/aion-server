# Phase 4 status

> Phase 4 ports the bodies behind the frozen spine (tag `spine-v1`) towards milestone M4, "all static data loads"
> ([handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.6, §2.7, amendments §9). Waves: 3a-1 (committed as `a99ec5fcb`),
> 3a-2 (in progress), 3b. Header request decisions: [../porting/header-requests.md](../porting/header-requests.md). Deviations are collected
> per chunk in [../deviations/](../deviations/) until the integrator merges them into [../DEVIATIONS.md](../DEVIATIONS.md). Earlier steps:
> [wave1-status.md](wave1-status.md), [spine-status.md](spine-status.md).

## Chunks

`AION_UNPORTED` counts `AION_UNPORTED(` occurrences in the files of `chunks.py files <chunk>` (sources and headers, tests excluded), given as
occurrences/files. Columns: `spine-v1`; `HEAD` = `a99ec5fcb` (wave 3a-1); "now" = the working tree on 2026-09-14 with the wave 3a-2
pre-stage applied and the 3a-2 lanes in progress, so the P4-04, P4-07b, P4-08 and P4-13 numbers are still moving. Tests are the `TEST*` macros
in the chunk's test directory now, plus the last reported run.

| Chunk | Target | Wave | Status | `AION_UNPORTED` spine-v1 → HEAD → now | Tests | Still needs (from chunk) |
|---|---|---|---|---|---|---|
| P4-01 | `aion_gs_configs` | 1 | done | 0 | `tests/configs` 52 | – |
| P4-02a/b | `aion_gs_runtime_*`, `aion_gs_handler_registry` | kernel, 1 | done | 3/1 → 3/1 → 3/1 (the macro and its doc comment in `Unported.h`, no stubs) | `tests/runtime` 439; `tests/handler_registry` 5 | kernel open items ([runtime-kernel-status.md](runtime-kernel-status.md)) |
| P4-03 | `aion_gs_geomath` | 1 | done | 0 | `tests/geomath` 77 | – |
| P4-15a | `aion_gs_network_crypt` | 1 | done | 0 | `tests/network_crypt` 27 | – |
| T2 / T2-gen | `aion_gs_xml` / `aion_gs_staticdata` | 1 | done (generated) | 0 | `tests/xml` 60 | – |
| P4-04 | `aion_gs_geo` | 3a-2 | in progress | 87/9 → 87/9 → 87/9 | none yet | `CollisionIntention` companion; callbacks for DataManager, ZoneService, SiegeService uses |
| P4-05 | `aion_gs_base` | 3a-1 | ported | 56/5 → 7/6 → 5/5 | `tests/base` 59 (55 pass, 1 skip reported) | `GameTime::onHourChange`: `TemporarySpawnEngine.h` (P4-10); `GMService::onPlayerLogin` commands: `ChatProcessor.h` (P5-14); `createStatsTemplate`: `PlayerStatCalculator` (P5-01) and a template lifetime decision; `CAPTCHAUtil::createImage`: a text rasterizer decision. `SellLimitInfo::getSellLimit` is unblocked (P4-12's `RatesInfo.h` exists) |
| P4-06 | `aion_gs_sysmsg` | 3a-1 | ported | 4/1 → 0 → 0 | `tests/sysmsg` 13 (13/13) | stand-in l10n helpers in `network/detail/SystemMessageL10n.h` → `ChatUtil`/`RaceInfo` (P4-05, now available), `AbyssRankEnum` companion (P5-01) |
| P4-07a | `aion_gs_templates` | 3a-1 | ported | 39/12 → 6/5 → 2/1 | `tests/templates/P4-07a` 51 (44 pass, 3 skip reported) | `MaterialZoneTemplate`: `BoundingBox` (P4-04); real zones load: `ZoneName::createOrGet` (P4-10); npc equipment: `NpcEquippedGear::init` (P4-13); item actions (P5-07); `ItemData::getItemTemplate` for `ResultedItem` (P4-09) |
| P4-07b | `aion_gs_templates` | 3a-2 | in progress | 26/20 → 26/20 → 17/17 | none yet | `PlaceableHouseObject.getPlacementLimit`, `Building.getSize`, `LimitType`/`StaticDoorState` companions (asked by P4-11a) |
| P4-08 | lease over P5-02..06 shells | 3a-2 | in progress | 158/115 → 159/116 → 155/112 | – | – |
| P4-09 | `aion_gs_dataholders` | 3b (pre-stage in 3a-2) | partly: `SkillData`, `ItemSetData`, `WorldMapsData` lookups and hooks, `NpcData::getNpcTemplate`, `ItemGroupsData::isFood` | 83/79 → 98/81 → 95/78 (3a-1 added lookup stubs) | `tests/dataholders` 8 | holder bodies stubbed by 3a-1 header requests: `PlayerExperienceTable` x3, `TitleData`, `NpcFactionsData` x2, `InstanceCooltimeData` x2, `PetData`, `PetFeedData`, `AtreianPassportData`, `TribeRelationsData`, `ItemRestrictionCleanupData` x3; `NpcData.init` needs `NpcStatCalculation` (P5-01) |
| P4-10 | `aion_gs_world` | 3b | not started | 277/22 → 278/22 → 278/22 | none | S0B-042/118; `KnownList::clearWithoutNotify` stub (player-1) |
| P4-11a | `aion_gs_objects` | 3a-1 | ported | 366/23 → 24/11 → 24/11 | `tests/objects` 36 (36/36) | `ItemData`, `HousingObjectData`, `HousePartsData`, `TradeListData`, `GatherableData` lookups (P4-09); `NpcKnownList.h` (P4-10); controller headers `StaticObjectController`, `GatherableController`, `FlyRingController`, `RoadController` (P4-11b); summon/trap/homing/servant stats containers (P5-01); `NpcSkillTemplateEntry`/`QueuedNpcSkillTemplate` (P5-02/P4-07b). The `NPC_DATA` base initializers (SummonedObject, Trap, Kisk, SummonedHouseNpc) are unblocked by pre-2 |
| P4-11b | `aion_gs_controllers` | 3b | not started | 262/25 → 263/25 → 263/25 | none | `ObserveController::hasObservers` stub (player-2, same monitor as the clear) |
| P4-12 | `aion_gs_player` | 3a-1 | ported | 378/37 → 9/5 → 9/5 | `tests/player` 27 (27/27) | `ExpireTimerTask` (P5-14) x4; `TitleChangeListener` x2, `ItemEquipmentListener` x2, `PlayerGameStats` constructor (P5-01); `ItemUseObserver` (P4-11b); `PlayerPetsDAO` (P4-14); P4-09 holder bodies above |
| P4-13 | `aion_gs_items` | 3a-2 | in progress | 176/22 → 176/22 → 176/22 | `tests/items` 2 (spine) | `ItemSlot` companion, `ItemMask`/`ItemId` headers (asked by P4-07a, P4-11a, P4-12, P4-15) |
| P4-14 | `aion_gs_dao` | 3b | not started | 313/56 | none | `String.hashCode` helper for record hashes; `usedIds` sources in `main.cpp` |
| P4-15 | `aion_gs_network` | 3a-1 | ported | 56/5 → 24/8 → 24/8 | `tests/network` 42 (41/41) | score and reward classes, `DredgionRoom.h` (P5-13) for 8 instance score writers and `DredgionScoreWriter`; `SM_KEY::writeImpl` (P4-16); `SM_L2AUTH_LOGIN_CHECK`, `SM_RECONNECT_KEY`, `SM_MESSAGE` (P4-17); `ItemRestrictionCleanupData` bodies (P4-09, blocks every item info packet at run time); `StatEnum` companion (P5-01); `GameServer.h` (P5-14); S0B-039 |
| P4-16 | `aion_gs_sm_ak` | 3b | not started | 174/111 | none | model bodies for tests |
| P4-17 | `aion_gs_sm_lz` | 3b | not started | 208/127 | none | model bodies for tests |

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

## Wave 3a-2

> Placeholder: filled in when the wave finishes. Lanes: P4-04 geo (with a Python geo oracle), P4-07b templates, P4-08 skill and quest data
> shells, P4-13 items. The pre-stage decided the four requests wave 3a-1 left open (pre-1 `SkillData` lookups, pre-2 `NpcData`/`ItemSetData`/
> `ItemGroupsData`/`WorldMapsData` lookups, pre-3 `SpawnSearchResult.spot` by value, pre-4 WTF-8 l10n strings;
> [header-requests.md](../porting/header-requests.md) "Wave 3a-2, pre-stage").
