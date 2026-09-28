# M5f work plan (travel and instances)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 revised after its adversarial review (§14 lists what the review found and what changed).
> Rev 1 was a **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2: the cast engine and the effect core") plus the working
> tree of M5b-2 part 3; rev 2 re-read the tree at HEAD **`760e8ab5c`** (part 3 committed, 22:58), where every count rev 2 relies on is
> unchanged (`TeleportService.cpp` 21, `PortalService.cpp` 13, `BindPointTeleportService.cpp` 4, `InstanceService.cpp` 12 + 2 P,
> `GeneralInstanceHandler.cpp` 11, `PlayerReviveService.cpp` 9, `CondSkillLauncherEffect.cpp` 2; `ReturnEffect.cpp` now 0). No file this plan
> asks a lane to change is modified in the working tree (`git status`). **Nothing was compiled, built or run.**
> C++ statements come from reading both trees, `game-server/chunks.cmake`, `tools/porting/chunks.py owner`, and counting `AION_UNPORTED(` /
> `AION_PARTIAL(` sites; the Java-against-C++ method comparison of lesson 1 used `tools/gen/javasrc.py`; the data statements come from parsing
> `data/static_data` with throw-away ElementTree scripts (comments skipped as the server skips them). §12 separates what was **measured** from
> what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md), [m5c-plan.md](m5c-plan.md) and [m5d-plan.md](m5d-plan.md). Inputs:
> [phase5-roadmap.md](phase5-roadmap.md) row 6 (`:31`), m5a-plan.md §5.6 (the one-map region move) and its geo row (`:227`), m5b3-plan.md O-05
> (`:494`: `PortalDialogAI`, `ResurrectAI` → M5f), m5c-plan.md W-08/W-16/D4/D5 and §3a (`:293, :301, :424, :425, :401`), m5d-plan.md D5 and A-02,
> `docs/porting/header-requests.md` m5b2-p2-9 (`:410`), `generated/concurrency/{cycles,fieldmap}.toml`, `tests/scenario/m5{a,b}_partial_allowlist.txt`.
>
> **M5f starts after M5e, and M5b-3, M5c, M5d and M5e are all unbuilt today.** §0 states exactly what this plan assumes each delivers; every work
> item, case and checklist step that stands on one of those assumptions carries its id (`A-01` … `A-11`), so the plan can be re-verified in
> one pass at branch time.

---

## 0. What this plan assumes the earlier milestones deliver

| Id | Assumed delivered by | What | Where it is today | Why M5f needs it | If it is not delivered |
|---|---|---|---|---|---|
| **A-01** | M5b-2 | `CM_CASTSPELL`, the cast engine, **skill 243 *Return*** (`ReturnEffect`, in m5b2-plan.md §2.4's 34 classes), `EffectController::hasAbnormalEffect(pred)` + `Effect::isHiPass` (`Effect.cpp:976`, ported), the skill decoders (m5b2 G-02), `gs.scenario.m5b2` green | `ReturnEffect.cpp` 0 `AION_UNPORTED` since `760e8ab5c`; `CM_CASTSPELL.cpp` exists | Return is a travel path (§2.4); `checkKinahForTransportation` asks `hasAbnormalEffect(Effect::isHiPass)` (TeleportService.java:164) | cases C9, C14 drop; the rest stands |
| **A-02** | M5b-3 | **`ItemPacketService::sendItemPacket`** (every kinah change: `Storage::decreaseItemCount` → `Storage.cpp:160`) and the inventory decoders (m5b3 G-02, `SM_INVENTORY_UPDATE_ITEM`) | `ItemPacketService.cpp:7-39`, 9 `AION_UNPORTED` | **every paid travel**: teleporter, flight, hotspot (`tryDecreaseKinah(…, DEC_KINAH_FLY)`, TeleportService.java:172, BindPointTeleportService.java:57), obelisk bind (ResurrectAI.java:97) | **half the gate is paid** — only Return, portals and instances remain (§9 fallback) |
| **A-03** | M5b-3 | `CM_USE_ITEM`, `ItemActionService`, the `canAct`/`act` virtuals of `AbstractItemAction` (m5b3 "h01" layout batch) | no `CM_USE_ITEM`; `MultiReturnAction.h`, `InstanceTimeClear.h` are shells with no `.cpp` | the two travel item actions m5c-plan.md §3a (`:401`) hands to M5f (X-01, X-02) | X-01/X-02 stay deferred (they are **O**) |
| **A-04** | M5c | `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, **`CM_QUESTION_RESPONSE`**; `DialogService` whole (m5c D4), incl. `isInteractionAllowed` and the `AIRLINE_SERVICE` arm (DialogService.java:187-197) that calls `TeleportService.showMap`; `PrivateStoreService::closePrivateStore` (`abortPlayerActions` calls it when `hasStore()`, TeleportService.java:196-197); the decoders `SM_DIALOG_WINDOW`, `SM_QUESTION_WINDOW` and their builders (m5c G-02) | no packet files; `DialogService.cpp` 7 `AION_UNPORTED`; `PrivateStoreService.cpp:24` U | every npc-driven travel starts with a dialog; the obelisk bind is a question window | **M5f cannot start** its npc half; hotspot, Return and flight-by-packet remain |
| **A-05** | M5c | the **Daeva seed** recipe proven by m5c C19 (m5c D5: `players.player_class`, a `player_quests (1006, COMPLETE)` row, `players.exp`) and `ScenarioDatabase::execute` seeding of positions and the kinah row | – | the gate's Daeva characters (D4) | the seed is re-proven in G-03 (a few seeding statements and one enter-world case more) |
| **A-06** | M5d | **`ActionItemNpcAI`** (m5d A-02; `PortalAI` extends it, PortalAI.java:22); the XML quests registered (so `QuestEngine.onEnterWorld` and zone triggers run on **every map a teleport lands on**, CM_LEVEL_READY.java:93); `QuestEngine::getQuestNpc(...).getOnTalkEvent()` and `QuestService::checkStartConditions` for `PortalDialogAI.checkDialog` (PortalDialogAI.java:101-118); m5d's A1 lease **released** | no `ActionItemNpcAI` file; `QuestEngine.cpp:111` partial | Haramel's entrance and exit are `ai="portal"` (§2.9); the teleport statues are `portal_dialog` | V-04 ports `ActionItemNpcAI` here (7 bodies, 99 Java lines) |
| **A-07** | M5e | `ClassChangeService` (`setClass`, `changeClassToSelection`, `showClassChangeDialog`, `completeAscensionQuest`; 7 `AION_UNPORTED` today) — **the real client's only quest-free way to become a Daeva** is `gameserver.simple.secondclass.enable = true` (CustomConfig.java:68; PlayerEnterWorldService.java:320-321; CM_DIALOG_SELECT.java:105-106; ClassChangeService.java:79-83) | `ClassChangeService.cpp` 7 U | the real-client checklist's Daeva steps (§11 steps 9-16). **Not** the gate, which seeds (D4) | the checklist uses the SQL of §11 instead |
| **A-08** | M5e | header request **m5b2-p2-9** (`RecallService::validateCast` takes a `Ptr`) applied by "the next lane that owns P5-08 (M5e/M5f at the latest)" (`header-requests.md:410`) | not applied | none on M5f's path | T-07 applies it |
| **A-09** | all | `gs.scenario.m5a`, `m5b`, `m5b2`, `m5b3`, `m5c`, `m5d` (and M5e's) green at branch time | – | G-05 re-greens them | the regate lane grows |
| **A-10** | M5e (+ M5b-3) | **M5e's stage-1 effect set** (m5e-plan.md §2.4, E-01/E-02, `:256-262`: 32 classes after A-02c — among them `DeformEffect`, `BindEffect`, `DelayedSpellAttackInstantEffect`, `CarveSignetEffect`, `SignetBurstEffect`, `SleepEffect`), which **includes the monsters of Verteron and Altgard**: m5e D4 (`:438`) and W-21 (`:383`), the union of 11 classes (m5e `:320-321`: `AbstractDispelEffect`, `AlwaysBlockEffect`, `BlindEffect`, `CurseEffect`, `DispelDebuffEffect`, `DispelEffect`, `FallEffect`, `FpAttackInstantEffect`, `PoisonEffect`, `SilenceEffect`, `SleepEffect`), with `BlindEffect`/`PoisonEffect`/`SilenceEffect`/`ParalyzeEffect` through M5b-3 (m5e A-02c, `:51`). **Not** delivered by M5e: the rest of Eltnen's and Morheim's monster classes, which m5e O-06 (`:510`) hands to "M5f and after" (D15) | all still `AION_UNPORTED` at `760e8ab5c` (e.g. `PoisonEffect.cpp` 5, `SleepEffect.cpp` 4) | the real-client checklist puts a Daeva among those monsters (§11 steps 12-13, 16); rev 2's own measure agrees with m5e's list for both maps (§2.8 W-22). The gate does not fight | the checklist does not fight in Verteron or Altgard either (§11) |
| **A-11** | M5c | the m5c oracle's price helpers (m5c-plan.md G-01, `oracle.py m5c-trade`): `tools/oracle/m5c/trade.py` `race_prices` (`:955-970`) and `times_div_100d` (`:485`), `tools/oracle/m5c/trade_config.py` `load_config` (`:288`) | **present in the working tree today as untracked files that m5c's lane is still writing** (rev 1 named a `trade_config.py` helper that did not exist yet) | G-01's teleport, flight and portal prices go through `PricesService.getPriceForService` (PricesService.java:86-90), whose three factors are m5c's `race_prices` | G-01 writes `getPriceForService` itself from PricesService.java:21-53, 86-90 (~30 lines of Python, the same float and truncation rules) |

**Re-verification at branch time:** for A-02/A-03/A-04/A-06/A-07/A-10 grep the named files for `AION_UNPORTED(` and check the packet files
exist; for A-05 read m5c's gate for case C19; for A-06 run `chunks.py owner` on `handlers/ai/portals/*` and check no A1 lease is active; for
A-10 re-run the npc-skill census of §2.8 W-22 (`npc_skills.xml` × `skill_templates.xml` × `Effects.java` × the C++ tree); for A-11 check that
`oracle.py m5c-trade` exists. A-02 and A-04 are **blocking** for the npc half; A-01, A-03, A-07, A-08, A-10, A-11 change single items or
checklist steps.

---

## 1. Summary

**The teleport core is already ported; its entry points and the instance engine are not.** M5a and M5b-1 needed the *mechanics* of moving a
player (region moves, bind revive on one map), so the part of `TeleportService` that actually moves a player exists — and the part a player
triggers does not:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `TeleportService::SpawnTask` (the whole cross-map/same-map arrival), `sendLoc`, `abortPlayerActions`, `spawnOnSameMap`, `teleportTo` (8 args) and its three world-id overloads, `moveToBindLocation`, `sendObeliskBindPoint`, `sendKiskBindPoint` | `services/teleport/TeleportService.cpp:103-147, 170-224, 236-249, 274-282, 296-341` (Java TeleportService.java:179-219, 249-289, 338-383, 500-536) |
| `CM_TELEPORT_ANIMATION_DONE`'s mechanism: `Future::deferred` + `runNowIfPending` | `runtime/sched/Future.h:104-127`; `TeleportService.cpp:185-186` |
| `CM_LEVEL_READY` (the second half of every cross-map teleport), `CM_EMOTION`'s `LAND_FLYTELEPORT` arm, `PlayerController::onFlyTeleportEnd`, `FlyController` | `CM_LEVEL_READY.cpp` (0 U), `CM_EMOTION.cpp:173-174`, `PlayerController.cpp:748`, `FlyController.cpp` (0 U) |
| `InstanceService`: `onPlayerLogin`, `getRegisteredInstance`, `instanceExists`, `onLogout`, `onEnterInstance`, `onEnterZone`/`onLeaveZone`, `getInstanceRate`, `getDestroyDelaySeconds`; `InstanceEngine::init`/`getNewInstanceHandler`; `InstanceScaler` (all) | `services/instance/InstanceService.cpp:108-114, 137-148, 156-193`; `instance/InstanceEngine.cpp:21-47`; `InstanceScaler.cpp` 0 U |
| The world under an instance: `WorldMapInstanceFactory`, `WorldMap::{addInstance,removeWorldMapInstance,getNextInstanceId}`, `WorldMapInstance::{register_,isRegistered,registerTeam,setStartPos,getPlayersInside,isFull}`, `SpawnEngine::spawnInstance`, `TemporarySpawnEngine::onInstanceDestroy`, `WalkerFormator::onInstanceDestroy`, `StaticDoorSpawnManager::spawnTemplate` | `world/*.cpp` (1 U in the whole P4-10 set on this path, §2.6), `spawnengine/*.cpp` |
| **Every server packet M5f sends** (20): `SM_TELEPORT_MAP`, `SM_TELEPORT_LOC`, `SM_EMOTION`, `SM_BIND_POINT_TELEPORT`, `SM_BIND_POINT_INFO`, `SM_INSTANCE_INFO`, `SM_INSTANCE_COUNT_INFO`, `SM_PLAYER_SPAWN`, `SM_CHANNEL_INFO`, `SM_QUESTION_WINDOW`, `SM_ACTION_ANIMATION`, `SM_USE_OBJECT`, `SM_DIALOG_WINDOW`, `SM_PLAYER_INFO`, `SM_DELETE`, `SM_INSTANCE_STAGE_INFO`, `SM_SHIELD_EFFECT`, `SM_ABYSS_ARTIFACT_INFO3`, `SM_WINDSTREAM`, `SM_WINDSTREAM_ANNOUNCE` | 0 `AION_UNPORTED` each (measured); their constructors read only ported holders (checked for `SM_INSTANCE_INFO`, `SM_TELEPORT_LOC`: M5c's "a 0-unported packet that throws" trap does not apply here) |
| The data: `TeleporterData`, `TeleLocationData`, `FlyPathData`, `HotspotData`, `BindPointData`, `Portal2Data`, `PortalLocData`, `InstanceCooltimeData`, `InstanceExitData`, `WindstreamData`, `RiftData`, `StaticDoorData` | `dataholders/*.cpp`, 0 U each |
| The persistence: `PlayerBindPointDAO`, `PortalCooldownsDAO`, `PortalCooldownList` | 0 U each — **but `player_bind_point` and `portal_cooldowns` have never been written by any gate** (D12) |
| `Player::setPosition` resets `lastPositionFromClient` on every teleport (the geo collision observers' guard) | `Player.cpp:1115-1124` (Player.java:1592-1600) |

**What is empty:**

| # | Hole | Where | Size (measured) |
|---|---|---|---|
| 1 | **The player's way in**: the teleporter npc path (`teleport`, `validateTeleporterAndGetTemplate`, `checkKinahForTransportation`, `showMap`, `teleportToFirstTeleportLocation`), the rest of `TeleportService`, all of `PortalService`, `BindPointTeleportService` (the 4.x map "hotspot" teleport) | **P5-08** | **38 sites + 3 anonymous bodies** (`TeleportService$1.acceptRequest`, `BindPointTeleportService$1.run`, `BindPointTeleportService$2.run`), 1,055 Java lines of files |
| 2 | **The instance engine**: `getNextAvailableInstance` ×4, the `EmptyInstanceCheckerTask` ×4, `destroyInstance` (+ the C++ cycle breakers `cycles.toml:33-34` names), `onLeaveInstance`, `getOrRegisterInstance`, the two M5a partials; `GeneralInstanceHandler`'s 11 bodies — **the base class all 73 phase-6 `@InstanceID` handlers call** (`spawn`, `getNpc`, `deleteAliveNpcs`, `sendMsg`, `onLeaveInstance`) | **P5-13** (+ P4-10) | **23 sites + 2 partials + 1** (`WorldMapInstance::detachInstanceHandler`, `WorldMapInstance.cpp:165-168`) **+ the no-op `InstanceHandler` that body installs, which does not exist anywhere in the tree** (43 pure virtuals of `InstanceHandler.h`, N-07, a lesson-1 hole no site count shows); the 24th site, `InstanceEngine::addInstanceHandlerClass`, stays (D10); 533 Java lines of files |
| 3 | **Five client packets with no file**: `CM_TELEPORT_SELECT`, `CM_TELEPORT_ANIMATION_DONE`, `CM_BIND_POINT_TELEPORT`, `CM_INSTANCE_LEAVE`, `CM_MOVE_IN_AIR` — opcodes already registered (`ClientPacketInfo.gen.inc:31, 57, 60, 131, 201`) | **P5-15/16** | 15 bodies, 259 Java lines |
| 4 | **Three npc AIs with no file**: `ResurrectAI` (the obelisk, `ai="resurrect"`), `PortalAI` (`ai="portal"`: Haramel's entrance and exit, the abyss gates), `PortalDialogAI` (`ai="portal_dialog"`: the teleport statues, the capitals' inner portals) — today `DummyNpcAI` answers a click with nothing (m5c W-15) | **P5-05** / **A1** (phase 6, lease) | 14 bodies (`ResurrectAI` 4 incl. the anonymous `AIRequest.acceptRequest`, `PortalAI` 5, `PortalDialogAI` 5), 339 Java lines |
| 5 | **Two siege bodies on every path**: `SiegeService::getSiegeIdByLocId` is the **first call of every npc teleport** (TeleportService.java:81); `onEnterSiegeWorld` runs in `CM_LEVEL_READY` for Inggison, Gelkmaros and Reshanta (CM_LEVEL_READY.java:70-72, Player.java:1268-1273) — with **no siege-config guard** | **P5-12a** (lease) | 2 bodies (`SiegeService.cpp:280, 288`) |
| 6 | **The logout of a dead player inside an instance**: `PlayerLeaveWorldService.cpp:118-120` (PlayerLeaveWorldService.java:95-97) calls `PlayerReviveService::instanceRevive`, `AION_UNPORTED` (`PlayerReviveService.cpp:102, 106`) — M5f is the first milestone in which a player can die inside an instance (W-20) | **P5-08** | 2 bodies |
| 7 | **The one monster skill of Haramel outside every earlier milestone's effect subset**: Drudgelord Kakiti (216897, `aggressive`, level 18) casts 19214 *Borrowed Life*, a `spellatkdraininstant` effect → `SpellAtkDrainInstantEffect` (`SpellAtkDrainInstantEffect.cpp:8`, U; optional in m5e T-02) (W-21) | **P5-04** | 1 site, 40 Java lines |

**Total: ~101 required bodies over ~2,300 Java lines of files (~1,500 lines of bodies to write — an estimate, §12), plus the no-op
`InstanceHandler`'s 43 one-line overrides (N-07), plus ~80 optional** (§5; X-05b alone is 27 sites) — rows 1-7 above (41 + 26 + 15 + 14 + 2 +
2 + 1). Rev 1 also counted N-06's four autogroup bodies; they are deferred (W-08: with `autogroup.enable` at its Java default the server
cannot even start, so no map change reaches them). That is the size of M5c (~132) and a fifth of M5b-2 (~521). **The size is not the risk;
the first instance lifecycle is** (§8 risk 1): M5f is the first milestone that *creates and destroys* a `WorldMapInstance` at runtime — 117
npc spots, their AIs, walkers and an instance handler per entry into Haramel (§2.9) — and the C++-only cycle breakers that destruction relies
on (`cycles.toml:353-357`) have never run; one of them needs a class nobody has written (N-07).

**Six findings shape the plan.**

1. **The first cross-map teleport throws today, and it is two bodies deep.** `SpawnTask::run` is ported, but for any map change it calls
   `InstanceService::onLeaveInstance` (`InstanceService.cpp:170-172`, U), which calls `GeneralInstanceHandler::onLeaveInstance` (`:35`, U) —
   and **every** map, not only instances, has a `GeneralInstanceHandler` (the default instances of an instance map, WorldMap.java:33; every
   field map through `InstanceEngine::getNewInstanceHandler`, WorldMapInstanceFactory.java:14 and InstanceEngine.java:49, since no field map has an
   `@InstanceID` handler). So M5b-2's *Return* (A-01) works in Poeta and throws the day a character binds or dies on another map. Items
   N-03/N-04 close it first. (Death itself is safe: M5b-1's bind revive stays on one map as long as no bind point exists elsewhere.)
2. **A level-1 character can travel on the start maps, but cannot leave them.** The Poeta and Ishalgen teleporters refuse a non-Daeva with
   `NO_RIGHT` (DialogService.java:188-195) and becoming a Daeva needs the phase-6 quest `_1006Ascension` / `_2008Ascension` or
   `gameserver.simple.secondclass.enable` (A-07). What a level-1 player *can* do is measured in §2.9: two flight masters per map, two obelisks,
   the map's three hotspots, a teleport statue and *Return*. The gate uses both kinds of character (D4).
3. **The only solo instance a player reaches without a quest is Haramel (300200000)**: level ≥ 16, `DAILY` 16 entries, entrance `730318` in
   Verteron / `730319` in Altgard, `ai="portal"` with a 3-second use bar (§2.9). Karamatis/Ataxiar (the ascension instances) are opened by
   quest scripts (_1006Ascension.java:98-99); Sliver of Darkness and Space of Destiny have `portal_use` rows on `ai="general"` npcs (203164
   morai, 203546 skuld), which no portal AI reaches, so a quest script opens them too (inferred; `_1929ASliverofDarkness.java:80` teleports
   to 203164); Aerdina/Bregirun need quest 1020/2022; every other instance needs a group (M5g).
   **No phase-6 instance handler is required**: a map without one gets `GeneralInstanceHandler` (InstanceEngine.java:49). `HaramelInstance`
   (63 lines, only the boss's death) is an optional pull-forward (H-01).
4. **A level-16 Daeva seed must be a Templar, not a Gladiator** (lesson 2, measured): a Gladiator autolearns the passive 563 *Determination* at
   level 15, whose `condskilllauncher` effect is `CondSkillLauncherEffect` (2 `AION_UNPORTED`), and `activatePassiveSkillEffects` does not
   catch (m5b2-plan.md D11) — so **no Gladiator of level ≥ 15 can enter the world** until that class is ported. A Templar's 11 passives ≤ 16
   are all ported (§2.8 W-14). This is also a real-player trap that belongs to M5e.
5. **Rifts do not belong here** (D1): no rift location is on Poeta, Ishalgen, Verteron or Altgard (`rift_locations.xml` lists only Eltnen,
   Heiron, Inggison, Cygnea, Morheim, Beluslan, Gelkmaros, Enshar), rifts are P5-12b's `RiftManager`/`RVController` and the roadmap's M5i row;
   M5f only gives them `TeleportService` (RVController.java:112, 139).
6. **Travel carries a real player past the effect boundary every earlier milestone drew** (lesson 2, measured, §2.8 W-21/W-22). m5b2-plan.md
   D6 (`:348`) keeps every effect class outside its subset `AION_UNPORTED`; m5e-plan.md D4 (`:438`) closes the scope at level 20 with the
   monsters of Verteron and Altgard (A-10) and hands Eltnen and Morheim to "M5f and after" (O-06, `:510`). Measured at `760e8ab5c`: Poeta,
   Ishalgen, Sanctum and Pandaemonium have **no** monster skill outside the ported set; **Haramel has exactly one** (Kakiti's 19214, hole 7);
   **Eltnen and Morheim keep 11 and 10 classes that no milestone takes** (12 distinct, 28 sites, 614 Java lines) once M5b-3 and M5e have
   delivered theirs. D15 takes the Haramel class as required and the Eltnen/Morheim classes as an optional item, and bounds the real-client
   checklist accordingly.

---

## 2. The paths, end to end

"ported" = the C++ body exists with no `AION_UNPORTED`; **U** = `AION_UNPORTED`; **P** = `AION_PARTIAL`; **no file** = no C++ class.

### 2.1 A teleporter npc: map to map

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `CM_SHOW_DIALOG` → `NpcController.onDialogRequest` → `GeneralNpcAI` → `SM_DIALOG_WINDOW` (page of a `func_dialogs="44"` npc) | NpcController.java:250-262 | M5c (A-04) | P5-16, P4-11b |
| 2 | `CM_DIALOG_SELECT(AIRLINE_SERVICE = 44)` → `DialogService.onDialogSelect`: **203679 / 203194 refuse a non-Daeva with `NO_RIGHT`**, else `TeleportService.showMap` | DialogService.java:187-197 | M5c (A-04); `showMap` **U** (`TeleportService.cpp:284`, m5c W-08) | P5-08 |
| 3 | `showMap` → `validateTeleporterAndGetTemplate` (teleporter data by npc id, `getType(player)` FRIEND/SUPPORT, `isInTalkRange` = talk distance + 1, not flying) → `SM_TELEPORT_MAP(npcObjId, teleportId)` | TeleportService.java:134-156, 291-295 | **U** ×2 (`:159, :284`); packet ported | P5-08 |
| 4 | `CM_TELEPORT_SELECT(targetObjId, locId, H)`: dead → return; the npc must be in the **known list**, else audit; `validateTeleporterAndGetTemplate` again; `getTeleportLocation(locId)` else `STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE`; `teleport(player, loc, npc.hasStatic() ? JUMP_IN_STATUE : JUMP_IN)` | CM_TELEPORT_SELECT.java:39-69 | **no file** | P5-16 |
| 5 | `teleport`: teleloc template; **`SiegeService.getSiegeIdByLocId`** (a 40-label switch, SiegeService.java:612-674) and, for a fortress loc, `isCanTeleport`; `required_quest` → `isCompleteQuest`; **`checkKinahForTransportation`**: `HiPass` → 1 kinah else `PricesService.getPriceForService(price, race)`, `tryDecreaseKinah(DEC_KINAH_FLY)` else `STR_MSG_NOT_ENOUGH_KINA`; REGULAR → `sendLoc(mapId, instanceId 1 or the current one, x, y, z, h, animation)` | TeleportService.java:72-132, 158-177 | **U** (`:154, :164`); `getSiegeIdByLocId` **U** (`SiegeService.cpp:288`); `getPriceForService` ported (`PricesService.cpp:84`); the kinah packet **U** until A-02 | P5-08, P5-12a |
| 6 | `sendLoc`: `abortPlayerActions` (close store, cancel recall, cancel cast, drop target, unride, end a flight path), `World.despawn(player, animation's delete animation)` (TeleportService.java:182). `World.despawn` marks the player **not spawned** (World.java:318) **before** it clears the known list (`:325`); `KnownList.clear` (KnownList.java:51-56) then calls the player's own `notSee` for every known object, which **returns at once for a player that is not spawned** ("player is teleporting, no need to send deletion packets", PlayerController.java:134-135; the port's guard: `PlayerController.cpp:228-229`, `World.cpp:326, 333`). **So the teleporting player receives no `SM_DELETE`**; every player that knew it receives `SM_DELETE(player, the delete animation)` — `JUMP_IN`/`JUMP_IN_STATUE`/`JUMP_IN_GATE` → 11, `FADE_OUT_BEAM` → 2, else `FADE_OUT` → 1 (TeleportAnimation.java:58-67, ObjectDeleteAnimation.java:38) — then `SM_TELEPORT_LOC(animation, mapId, mapId-or-instanceId, x, y, z, h)` and a deferred `SpawnTask` as the `TELEPORT` task | TeleportService.java:179-206 | ported (`TeleportService.cpp:170-202`) | P5-08 |
| 7 | `CM_TELEPORT_ANIMATION_DONE`: `getAndRemoveTask(TELEPORT)`, run it now if not done, `get()`; on an exception log and spawn in place | CM_TELEPORT_ANIMATION_DONE.java:30-50 | **no file** | P5-16 |
| 8 | `SpawnTask.run`: (animated) dead or instance gone → spawn in place; `abortPlayerActions`; map or instance changes → `ConquerorAndProtectorService.onLeaveMap` + **`InstanceService.onLeaveInstance`**; `setPosition` (player + pet); same map → `spawnOnSameMap` (`SM_CHANNEL_INFO`, `SM_PLAYER_INFO`, `SM_STATS_INFO`, `SM_MOTION`, spawn, protection, effect icons, `updateZone`), else `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN` (+ `STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF` for a non-personal instance); legion member → `updateMemberInfo` | TeleportService.java:500-536 | ported (`TeleportService.cpp:103-147`), **but `onLeaveInstance` U** (`InstanceService.cpp:170`) → `GeneralInstanceHandler::onLeaveInstance` U (`:35`) → `removeInstanceItems` U (`:125`) → `isRestrictedToInstance` U (`:121`); `updateMemberInfo` U (guarded: legion member) | P5-13 |
| 9 | `CM_LEVEL_READY` on the new map: `SM_INSTANCE_COUNT_INFO` when in an instance, `SM_PLAYER_INFO`, `SM_MOTION`, windstream announces, **spawn**, siege world → **`SiegeService.onEnterSiegeWorld`**, `ConquerorAndProtectorService.onEnterMap`, `RiftInformer.sendRiftsInfo`, `updateNearbyQuests`, weather, **`QuestEngine.onEnterWorld`**, `PlayerController.onEnterWorld`, `InstanceService.onEnterInstance`, effect icons, `SM_CUBE_UPDATE`, town, events | CM_LEVEL_READY.java:44-111 | ported; `onEnterSiegeWorld` **U** (`SiegeService.cpp:280`, reached from `CM_LEVEL_READY.cpp:97-98`) | P5-16 (the 5a lease), P5-12a |

### 2.2 A flight master: the flight transport

There is no `SM_FLIGHT…` packet in 4.8: a flight transport is an **emotion pair around a client-driven path**.

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | Steps 1-5 of §2.1 with a `type="FLIGHT"` location (the flight masters' only kind: 142 of the 544 telelocations, each with a `teleportid`, e.g. Kustanon 203070 → loc 13, `teleportid="5001"`, npc_teleporter.xml:145-149) | TeleportService.java:94-122 | as §2.1 |
| 2 | Only if `gameserver.security.validation.flypath` (default **false**, SecurityConfig.java:69-70, security.properties:100): the fly-path template **looked up by `location.getLocId()`**, start within 7 m, start world. **Measured quirk:** fly-path ids and loc ids are different number spaces (loc 13's "fly path 13" starts in 220030000), so enabling the validator refuses every flight — faithful, kept, warned (D7) | :95-117 | U (part of `teleport`) |
| 3 | `abortPlayerActions`, `setState(FLYING)`, `unsetState(ACTIVE)`, `setFlightTeleportId(teleportid)` (a `FlightPath(FLIGHT_TRANSPORTER, id, 0)`, Player.java:809-811), broadcast **`SM_EMOTION(START_FLYTELEPORT = 6, teleportid)`** | :118-122 | U (part of `teleport`); `SM_EMOTION` ported |
| 4 | The client flies (38 s from Akarios to Melponeh, `flypath_template.xml` id 5) and sends **`CM_MOVE_IN_AIR(worldId, x, y, z, heading, distance)`**: only while spawned and `FLYING`; `flightPath.setDistance`, stop protection, `updatePosition`, `onMoveFromClient`, `onMove`. **It broadcasts nothing**; `World.updatePosition(…)` → `updatePosition(…, true)` → `object.updateKnownlist()` (World.java:167-169, 234-235) → `KnownList.update` (forget out-of-range objects, find new ones within max(95, 95) m: VisibleObject.java:247-249, KnownList.java:155-196, 208-216) — so the known list follows the flight (read, not inferred) | CM_MOVE_IN_AIR.java:34-60 | **no file** |
| 5 | Others see the flyer through `SM_PLAYER_INFO`, sent **once**, when the flyer first enters their known list (`findVisibleObjects` adds both ways, KnownList.java:192-193 → `PlayerController.see` → `sendPlayerInfoPackets`, PlayerController.java:90-103, 122-129); it writes the flight id and the distance of **the `CM_MOVE_IN_AIR` that brought the flyer into range** (the packet sets the distance before `updatePosition`, CM_MOVE_IN_AIR.java:52, 57) while `isUsingFlightTransporterOrWindstream()` | SM_PLAYER_INFO.java:198-200; Player.java:821-823 | ported |
| 6 | Landing: **`CM_EMOTION(LAND_FLYTELEPORT = 7)`** → `onFlyTeleportEnd`: unset `FLYING`, (validator) audit, `ACTIVE`, `updateZone`, `setFlightPath(null)` | CM_EMOTION.java:166-167; PlayerController.java:652-686 | ported (`CM_EMOTION.cpp:173-174`, `PlayerController.cpp:748`) |

### 2.3 The hotspot teleport (`CM_BIND_POINT_TELEPORT`)

The 4.x world-map teleport: the player clicks a hotspot on the map, a 10-second cast runs, then a same-map teleport. **No npc, no level, no
Daeva check** — a level-1 character can use it (§2.9).

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | `CM_BIND_POINT_TELEPORT(action: 1 cast \| 2 cancel \| 3 done, [locId D, kinah Q])`; dead → return | CM_BIND_POINT_TELEPORT.java:24-46 | **no file** |
| 2 | `teleport`: hotspot by id else audit; **price = max(1, base + (long)(base × distance / 1000))** with the distance in **float** (PositionUtil.java:223-230), then `max(price, clientPrice)` with a WARN when they differ by more than 1; `checkRequirements` (same world, race, kinah, **600-second cooldown** held in a static in-memory map) | BindPointTeleportService.java:39-49, 80-120 | **U** ×3 (`BindPointTeleportService.cpp:38, 46, 50`) |
| 3 | broadcast `SM_BIND_POINT_TELEPORT(1, playerId, locId)`; `TaskId.SKILL_USE` = a 10-s task: `tryDecreaseKinah(DEC_KINAH_FLY)`, `addCooldown`, broadcast `SM_BIND_POINT_TELEPORT(3, playerId, locId, 600)`, then a 1-s task: not dying → `teleportTo(worldId, x, y, z)` (NONE → same map → §2.1 step 8's `spawnOnSameMap`) | :51-71 | **U** (the two anonymous `Runnable`s, not declared — lesson 1); `addCooldown`, `getCooldown`, `onLogin` ported |
| 4 | cancel: if a `SKILL_USE` task exists → cancel it, broadcast `SM_BIND_POINT_TELEPORT(2, …)` | :74-79 | **U** (`:42`) |

### 2.4 Bind points, *Return*, bind revive

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Click an obelisk (`ai="resurrect"`, 129 bind-point templates): `ResurrectAI.handleDialogStart` — already bound within 20 m → `STR_ALREADY_REGISTER_THIS_RESURRECT_POINT`; race/tribe checks; prison → refuse; **`AIActions.addRequest(STR_ASK_REGISTER_RESURRECT_POINT, price)`** → `SM_QUESTION_WINDOW` | data/handlers/ai/ResurrectAI.java:43-73 | **no file** (`DummyNpcAI`, m5c W-15); `AIActions::addRequest` ported (`AIActions.cpp:119-123`) | P5-05 |
| 2 | `CM_QUESTION_RESPONSE(yes)` → the anonymous `AIRequest.acceptRequest`: same world, kinah ≥ price, **within 5 m**; a `BindPointPosition` at the **player's** position (NEW or UPDATE_REQUIRED); `PlayerBindPointDAO.store`; `decreaseKinah(price)` (**raw price, no `PricesService`**); `sendObeliskBindPoint` (`SM_BIND_POINT_INFO(0, 1, map, x, y, z, 0)`); `SM_ACTION_ANIMATION(BIND_KISK)`; `STR_DEATH_REGISTER_RESURRECT_POINT` | ResurrectAI.java:75-107 | no file; the DAO and `sendObeliskBindPoint` ported (`PlayerBindPointDAO.cpp:91`, `TeleportService.cpp:296-312`); `CM_QUESTION_RESPONSE` A-04 | P5-05 |
| 3 | *Return* (skill 243: 6,000-ms cast, `<selfflying restriction="GROUND"/>`, `skill_templates.xml:3046-3058`) → `ReturnEffect.applyEffect` → **`moveToBindLocation`**: the bind point, or the race's initial spawn (`player_initial_data.xml:3-4`) → `teleportTo(world, x, y, z, h)` → NONE animation | ReturnEffect.java; TeleportService.java:362-383 | A-01; `moveToBindLocation` ported. **Cross-map → §2.1 step 8's U** | P5-04, P5-08 |
| 4 | `CM_REVIVE(BIND_REVIVE)` → `PlayerReviveService.bindRevive` → `moveToBindLocation` (M5b-1's path, same map today) | PlayerReviveService.java:100-133 | ported; cross-map → step 8's U | P5-08 |
| 5 | `instanceRevive` (the handler's `onReviveEvent`, else revive at 25 % with soul sickness and `teleportTo(startPos's map, x, y, z)` — the 5-argument world-id overload, ported at `TeleportService.cpp:236` — else `bindRevive` when there is no `startPos`). Reached two ways: `CM_REVIVE(INSTANCE_REVIVE)`, offered only when `allowInstanceRevive()` — **false for a plain `GeneralInstanceHandler`** (GeneralInstanceHandler.java:274-276); and **the logout or disconnect of a dead player inside any instance (or on 400030000), whatever its handler** (PlayerLeaveWorldService.java:95-97; `PlayerLeaveWorldService.cpp:118-120`) — W-20 | PlayerReviveService.java:157-187 | **U** (`PlayerReviveService.cpp:102, 106`) → **R** (T-06) | P5-08 |
| 6 | Enter world: `SM_BIND_POINT_INFO` from the loaded bind point | PlayerEnterWorldService.java:264 | ported (M5a) | – |

### 2.5 Portals and the instance engine

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Click a portal (`ai="portal"`): `PortalAI.handleDialogStart` → `QuestEngine.onDialog(USE_OBJECT)` → `ActionItemNpcAI.handleDialogStart` → `isInteractionAllowed` → **use bar**: `SM_USE_OBJECT(player, npc, talkDelay ms, 1)` + `SM_EMOTION(START_QUESTLOOT)`; an `ItemUseObserver` aborts it; after `talk_info delay` (3 s for Haramel) `SM_EMOTION(END_QUESTLOOT)` + `SM_USE_OBJECT(…, 2)` → `handleUseItemFinish` | PortalAI.java:41-57; ActionItemNpcAI.java:36-80 | **no file** ×2 (A1, P5-05; A-06) | A1, P5-05 |
| 2 | `handleUseItemFinish`: `Portal2Data.getPortalUsePath(npcId, player)` → **`PortalService.port`**; else a teleporter template → `teleportToFirstTeleportLocation(FADE_OUT_BEAM)` | PortalAI.java:47-57 | `Portal2Data` ported; the rest **U** | A1 |
| 2b | `portal_dialog` npcs: `checkDialog` → page `getTeleportDialogId` (or a quest page) → `CM_DIALOG_SELECT(dialog id)` → `onDialogSelect` → `getPortalDialogPath(npcId, dialogActionId)` → `PortalService.port` | PortalDialogAI.java:49-170 | **no file** | A1 |
| 3 | `port`: portal loc; group requirement unless staff/membership; `maxPlayers` from `instance_cooltimes.xml` by race; `checkMentor`, `checkRace` (+ fortress race), `checkRank`, `checkTitle`, `checkQuests`, `checkPlayerSize` (group/alliance/league); the registered instance by player/group/alliance/league id; **not registered and `isPortalUseDisabled` → `STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME`**, registered elsewhere → `reenter`; `checkEnterLevel` (portal min level or the cooltime's; error → `SM_DIALOG_WINDOW(err_level)` or `STR_MSG_CANT_INSTANCE_ENTER_LEVEL`), `checkAndRemoveRequiredItems`; same map → plain `teleportTo`; then by `maxPlayers`: 0/1 → `transfer` or `port(private)`, 3/6 → group, else alliance | PortalService.java:45-191, 193-343 | **U** ×13 (`PortalService.cpp:12-70`) | P5-08 |
| 4 | `port(private)`: instance map → **`InstanceService.getNextAvailableInstance(world, personal ? owner : 0, 0, maxPlayers, true)`**, `register`, `transfer`; else `teleportTo(FADE_OUT_BEAM)` | :345-355 | U | P5-08 |
| 5 | `getNextAvailableInstance`: `WorldMapInstanceFactory.createWorldMapInstance(map, owner, InstanceEngine::getNewInstanceHandler, maxPlayers)`, **`SpawnEngine.spawnInstance(instance, difficulty, owner)`**, `onInstanceCreate`, and with `autoDestroy` an **`EmptyInstanceCheckerTask` at a fixed rate of 60 s after 60 s**; `log.info("Created new instance: …")` | InstanceService.java:39-77 | **U** ×4 (`InstanceService.cpp:81-95`); everything it calls ported | P5-13 |
| 6 | `transfer`: `startPos` once, `register`, `teleportTo(…, instanceId, …, FADE_OUT_BEAM)` (§2.1 steps 6-9), then unless `reenter` **`PortalCooldownList.addPortalCooldown(world, calculateInstanceEntranceCooltime)`** → `PortalCooldownsDAO.storePortalCooldowns` + **`SM_INSTANCE_INFO(2, player, world)`** | PortalService.java:357-367; PortalCooldownList.java:66-77; InstanceCooltimeData.java:70-100 | U / ported | P5-08 |
| 7 | Inside: `CM_INSTANCE_LEAVE` → `handler.leaveInstance` — **a no-op in `GeneralInstanceHandler`** (GeneralInstanceHandler.java:61); only 13 phase-6 handlers override it. Leaving is by an exit portal (Haramel `730320`), *Return*, death + bind revive, or logout | CM_INSTANCE_LEAVE.java:24-29 | **no file** | P5-15 |
| 8 | Leaving: `onLeaveInstance` → handler's `onLeaveInstance` (remove instance effects, remove items restricted to the world) + the leave message: solo `STR_MSG_LEAVE_INSTANCE(destroyDelay / 60)`, emptied party, last player | InstanceService.java:210-224; GeneralInstanceHandler.java:65-68, 278-292 | **U** (`:170`; `GeneralInstanceHandler.cpp:35, 121, 125`) | P5-13 |
| 9 | Destruction: the checker (no player inside and personal, or disbanded team, or `now > max(taskStart, lastPlayerLeave) + delay − 1 s`) → `destroyInstance`: cancel the checker, `removeWorldMapInstance`, `TemporarySpawnEngine.onInstanceDestroy`, **each player: `STR_MSG_LEAVE_INSTANCE_FORCE` + `moveToExitPoint`; every other object `controller.delete()`**, `onInstanceDestroy`, `WalkerFormator.onInstanceDestroy`; **C++ additions**: `detachInstanceHandler`, `setStartPos(nullptr)`, `releaseRegisteredTeam` (`cycles.toml:33-34, 353-357`) and erasing `InstanceScaler::scalings` (the TODO at `InstanceService.cpp:98-100`). `detachInstanceHandler` must install "the no-op InstanceHandler" (`WorldMapInstance.cpp:165-168`; `WorldMapInstance.h:169` promises `getInstanceHandler()` is never null; the design is single-thread-synthesis.md:56, 272 "a shared `NoopInstanceHandler`") — **no such class exists** (grep: the only `InstanceHandler` implementors are `GeneralInstanceHandler`, `PvpMapHandler`, `AutoInstanceHandler` and test doubles), and it cannot be a `GeneralInstanceHandler`, which holds `const Ref<WorldMapInstance>` (`GeneralInstanceHandler.h:44`) and would rebuild the cycle | InstanceService.java:82-107, 167-198 | **U** (`:65-79, :97-102`); `detachInstanceHandler` **U** (P4-10); the no-op handler **no file** (N-07) | P5-13, P4-10 |
| 10 | Login inside an instance: `onPlayerLogin` → registered → set the instance id; **not registered (destroyed) → `moveToExitPoint` → `moveToInstanceExit`** (`instance_exit.xml` by world and race, else the bind location) — **during enter world, before the spawn**: `teleportTo` → `sendLoc` → `SpawnTask.run` sends its own `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN`, then the enter world sends them again (PlayerEnterWorldService.java:223, 261, 270) | InstanceService.java:148-161; TeleportService.java:394-403 | `onPlayerLogin` ported; **`moveToExitPoint` P** (`InstanceService.cpp:153`); `moveToInstanceExit` **U** (`TeleportService.cpp:348`) | P5-13, P5-08 |
| 11 | Instance "timers": the checker (step 9), the **entrance cooltime** (`DAILY`/`WEEKLY` at `ent_cool_time` HHMM server time, `RELATIVE` in minutes; `maxcount` entries), `SM_INSTANCE_INFO` at enter world (M5a's position 24, ported) and after each entry; handler timers are phase 6 | InstanceCooltimeData.java:70-100 | ported except the entry path | – |

### 2.6 Status by area (measured at HEAD + working tree)

| Area | Chunk | `AION_UNPORTED` | `AION_PARTIAL` | Undeclared / no file | M5f takes |
|---|---|---|---|---|---|
| `services/teleport/**` (`TeleportService` 21, `PortalService` 13, `BindPointTeleportService` 4) | **P5-08** | 38 | 0 | 3 anonymous bodies | **all 38 + 3** |
| `PlayerReviveService` `instanceRevive` ×2, `kiskRevive` ×2 | P5-08 | 4 (of 9) | 0 | – | **`instanceRevive` ×2 R** (W-20), `kiskRevive` ×2 W (T-06) |
| rest of P5-08: pets 30, summons 17, duel 11, PvP 10, abyss 10, chat 9, `DialogService` 7 → M5c, `ClassChangeService` 7 → M5e, social 6, punishment 5, `PlayerReviveService`'s other 5, `RecallService` 4 → M5g, `KiskService` 2 → M5j, `PlayerLimitService` 1 | P5-08 | 124 | 2 | – | none (T-07's signature only) |
| **P5-08 total** | | **166** (roadmap: 166 ✓) | 2 | | |
| `InstanceService` 12 + `GeneralInstanceHandler` 11 + `InstanceEngine` 1 | **P5-13** | 24 | 2 (`:133`, `:153`) | 0 named methods | **23 + 2** (`addInstanceHandlerClass` stays, D10) |
| `PvpMapHandler` 39 + `PvpMapService` P, `CustomInstanceService` 13 (custom features) | P5-13 | 52 | 1 | – | none → M5j (D6) |
| `InstanceScore` 10, `InstancePlayerReward` 3, `PeriodicInstanceManager` 9, `PvPArenaService` 6 (scored/registered group instances) | P5-13 | 28 | 0 | – | none → M5g/M5i; rev 1's N-06 (3 `PeriodicInstanceManager` bodies) is deferred with `scheduleRegistration` (W-08, O-07) |
| `AutoGroupService::onLeaveInstance` (of the file's 21) | P5-10 | 1 | 0 | – | none (O-07; no lease) |
| `PlayerRestrictions` (`canTrade`, `canChat`, `canUseItem`, `canChangeEquip` → M5b-3/M5c; `canInviteTo*` ×3 → M5g), `PlayerTransfer*` 7 → M5j | P5-13 | 14 | 0 | – | none |
| **P5-13 total** | | **118** (roadmap: 118 ✓) | 3 | | |
| `WorldMapInstance::detachInstanceHandler` + the no-op `InstanceHandler` it installs | P4-10 | 1 | 0 | **1 class, 43 overrides** (41 game methods + `retain`/`release`, `InstanceHandler.h:31-119`) | **1 + the class** (lease, N-02/N-07) |
| `SiegeService::getSiegeIdByLocId`, `onEnterSiegeWorld` | P5-12a | 2 (of the chunk's 107) | 0 | – | **2** (lease) |
| `CM_TELEPORT_SELECT`, `CM_TELEPORT_ANIMATION_DONE`, `CM_BIND_POINT_TELEPORT`, `CM_INSTANCE_LEAVE`, `CM_MOVE_IN_AIR` | P5-15/16 | – | – | 5 files, 15 bodies | **5** |
| `CM_POSITION_SELF` ("C_BLINK", empty `runImpl`), `CM_CHANGE_CHANNEL`, `CM_PLAY_MOVIE_END` (if M5d did not) | P5-15/16 | – | – | 3 files, 9 bodies | **O** |
| `ResurrectAI` (P5-05), `PortalAI`, `PortalDialogAI` (A1) | P5-05, A1 | – | – | 3 files, 14 bodies | **3** |
| `ActionItemNpcAI` (P5-05) | P5-05 | – | – | 1 file, 7 bodies | only if A-06 fails |
| `MultiReturnAction`, `InstanceTimeClear` (P5-07), `ReturnPointEffect` (P5-04, 3 U) | P5-07, P5-04 | 3 | 0 | 8 bodies (m5c §3a) | **O** (A-03) |
| `SpellAtkDrainInstantEffect` (P5-04) — Haramel's Kakiti (W-21) | P5-04 | 1 | 0 | – | **R** (X-05a, D15) unless M5e's optional T-02 took it |
| The Eltnen/Morheim monster classes past A-10: P5-03 `DiseaseEffect` 4, `ConfuseEffect` 4, `FpAttackEffect` 2, `FearEffect` 4 (+1 inner), `DispelDebuffMentalEffect` 1, `CloseAerialEffect` 2, `DelayedFpAtkInstantEffect` 3 (+1 inner), `DispelBuffEffect` 1; P5-04 `MpAttackInstantEffect` 2, `ProtectEffect` 2 (+ its observer), `MagicCounterAtkEffect` 2 (+1 inner) (W-22) | P5-03, P5-04 | 27 | 0 | ~4-6 inner bodies | **O** (X-05b, D15) |
| `HaramelInstance` (I1), `_1006Ascension` / `_2008Ascension` (Q06) | I1, Q06 | – | – | 2 + 19 bodies, 63 + 592 lines | **O** (D2, D5) |

### 2.7 Lesson 1: the bodies no site count sees

Measured with `javasrc.parse_file` over the 31 Java files of the path (every method with a body in every member, local and anonymous type)
against the names declared in the C++ `.h`/`.cpp`/generated headers:

- **`TeleportService`, `PortalService`, `BindPointTeleportService`, `InstanceService`, `InstanceEngine`, `GeneralInstanceHandler`,
  `PortalCooldownList`, `BindPointPosition`, `PlayerBindPointDAO`, `KiskService`, `FlyController`, `FlightPath`, `CM_INSTANCE_INFO`: 0 named
  methods undeclared.** The shells written at the freeze are complete here — unlike M5b-2's 208 undeclared overrides — because none of these
  classes overrides a hub virtual. (`WorldMapInstance.register` reports as missing only because C++ spells it `register_`.)
- **3 anonymous bodies** that must be defined in the `.cpp` (hub-headers.md §7.3): `TeleportService$1.acceptRequest` (TeleportService.java:467-473),
  and the two `Runnable`s of `BindPointTeleportService.teleport`, whose fieldmap keys the C++ source already names:
  **`BindPointTeleportService$1`** (BindPointTeleportService.java:53-71, the outer 10-s task) and **`BindPointTeleportService$2`** (`:63-69`,
  the inner 1-s task) (`BindPointTeleportService.cpp:35-36`). They are **two different kinds**: only `$1` is stored — as the player's
  `SKILL_USE` task (`:53`, `addTask`) — and captures the player, i.e. player → controller tasks → Future → task → player, the K-06 kind of
  cycle that needs a `cycles.toml` row (cut by `cancelTask`/the task's own completion); `$2` is an **unstored one-shot `schedule`** (`:63`)
  and gets the `accepted: one-shot task: releases its captures when it runs or is cancelled` row its peers have (e.g. the `AuctionEndTask`
  row of `cycles.toml`). **Neither has a row today**; T-08 adds both after `fieldmap.py --class` (the existing rows for `SpawnTask` and
  `EmptyInstanceCheckerTask` are `fieldmap.toml:68, 77`).
- **The no-op `InstanceHandler`** (§2.5 step 9): a whole C++-only class the design names and no file declares — 43 pure virtuals of
  `InstanceHandler.h` (`:31-119`, including `retain`/`release`). No site count shows it; N-07 writes it.
- **Whole classes with no C++ file on the path: 15, 66 bodies** — the 8 client packets of §2.10's R and O rows (24; 5 required), 4 AIs (21:
  `ResurrectAI` 4, `PortalAI` 5, `PortalDialogAI` 5 required, `ActionItemNpcAI` 7 by A-06), `HaramelInstance` (2), the two ascension quests
  (19). Plus `MultiReturnAction`/`InstanceTimeClear`, shells with no `.cpp` (8, m5c §3a): **17 classes, 74 bodies** (rev 1 printed "13, 81";
  measured again with `javasrc.parse_file` for rev 2).

### 2.8 Lesson 2: what M5f wakes

Traced from every entry point M5f turns on — the five client packets, the three AIs, *Return* and bind revive on another map, the instance
checker task, enter world inside an instance, **leave world inside an instance** (rev 2, W-20), **`CM_LEVEL_READY` on every map a teleport can
reach**, and **the skills of the monsters on those maps and in Haramel** (rev 2, W-21/W-22) — to the first unported or partial body. **W** = reached by M5f's own paths (closed or deliberately loud); **D** = dormant (a state the start maps and the gate do not produce).

| # | Reached from | First unported / partial body | Kind | Resolution |
|---|---|---|---|---|
| W-01 | any map change (teleport, *Return*, bind revive, portal) | `InstanceService::onLeaveInstance` → `GeneralInstanceHandler::onLeaveInstance` → `removeInstanceItems` → `isRestrictedToInstance` (`InstanceService.cpp:170`, `GeneralInstanceHandler.cpp:35, 121, 125`) | **W** | N-03, N-04 |
| W-02 | every npc teleport | `SiegeService::getSiegeIdByLocId` (`SiegeService.cpp:288`) | **W** | T-05 |
| W-03 | `CM_LEVEL_READY` on Inggison, Gelkmaros, Reshanta (Sanctum's aerolink 730218, Pandaemonium's 730219, the abyss gates 730059/730062 — §2.9) | `SiegeService::onEnterSiegeWorld` (`:280`); **no config guard** (CM_LEVEL_READY.java:70-72). With `siege.enable = false` `SiegeService` holds no location (m5c §2.10), so the body sends two empty packets | **W** | T-05 |
| W-04 | every paid travel | `ItemPacketService::sendItemPacket` (`Storage.cpp:160`, in `decreaseItemCount`) | W | **A-02** |
| W-05 | `abortPlayerActions` with a private store open | `PrivateStoreService::closePrivateStore` (`:24`) | W (guarded by `hasStore()`) | A-04 |
| W-06 | `SpawnTask.run` for a legion member; `teleportDeadTo` | `LegionService::updateMemberInfo` (`LegionService.cpp:352`) | D (no legions before M5h) | – |
| W-07 | `teleportTo` for a duelist | `DuelService::loseDuel` (`DuelService.cpp:49`) | D (no duels on the path) | – |
| W-08 | every map change with `gameserver.autogroup.enable` — `true` by default (AutoGroupConfig.java:12, autogroup.properties:7; the C++ binding keeps the default, `AutoGroupConfig.cpp:9`) — through `InstanceService.onLeaveInstance` (InstanceService.java:222-223) | `AutoGroupService::onLeaveInstance` (`AutoGroupService.cpp:152`, P5-10) → `PeriodicInstanceManager::checkAndSendOpenRegistrations` (`PeriodicInstanceManager.cpp:74`) → `isInLvlRange` (`:82`). **But with autogroup enabled the server never gets that far**: startup step `PeriodicInstanceManager.getInstance()` (`GameServer.cpp:243`; Java GameServer.java:165) runs the constructor (`PeriodicInstanceManager.cpp:25-50`), which calls `scheduleRegistration` (`AION_UNPORTED`, `:52-55`) seven times, and `runStep` does not catch (`GameServer.cpp:139-141, 311-315`) — **the start fails** | **D** (unreachable: every profile and the user's `mygs.properties:9` set it `false`; the Java default cannot start) | **none in M5f** — rev 1's N-06 is deferred with `scheduleRegistration` to whichever milestone ports instance matchmaking (m5g-plan.md O-01 defers it past M5g) (O-07) |
| W-09 | enter world after a relog in a destroyed instance | `moveToExitPoint` P → `moveToInstanceExit` U — **the first teleport ever run during enter world, before the spawn** (§2.5 step 10) | **W** | N-03, T-02 |
| W-10 | `EmptyInstanceCheckerTask` (a pool thread, 60 s) | the checker ×4 → `destroyInstance` → `detachInstanceHandler` → the no-op handler (no file) | **W** | N-01, N-02, **N-07** |
| W-11 | `SpawnEngine::spawnInstance(Haramel)`: 117 spots of 42 npc ids | ported; **their AIs**: `aggressive` 76, `general` 3, `noaction` 7 ported; `quest_use_item` 24, `useitem` 3 (M5d, A-06); `portal` 1, `portal_dialog` 1 (M5f); **`summoner` 1** (the boss 216922, `SummonerAI`, root, no file), **`chest` 1** (`ChestAI`, root, no file; no plan takes it — m5b3-plan.md:777 found no chest on the start maps) → `DummyNpcAI`, WARN at spawn | **W**, silent | the boss stays inert until phase 6 (checklist) |
| W-12 | `CM_LEVEL_READY` on the arrival maps (90 m around each arrival, measured): Verteron 69 spots — `simple_abyssguard` 14, `postbox`, `resurrect`, **`speaker`**; Sanctum 12 — `simple_abyssguard` 4, `portal_dialog`; Altgard 46; Morheim 59 — **`following`**, **`book`**; Ishalgen 41 | `SpeakerAI`, `FollowingNpcAI`, `BookAI` (root, no file; `following` is m5c D2's) → `DummyNpcAI` (the arrival npcs' *skills* are W-22's) | D, silent | M5j / capital economy |
| W-13 | `CM_LEVEL_READY` → `QuestEngine.onEnterWorld` on **each** new map | M5d's registry (A-06) — the XML quests with enter-world / zone triggers run for the first time on Verteron, Altgard, Sanctum, Pandaemonium, Morheim | **W** (unknown until run) | the gate lane measures it first (§13 item 1) |
| W-14 | enter world of a **Gladiator ≥ 15** (a real player, or a seed) | passive 563 → `CondSkillLauncherEffect` (2 U) in `activatePassiveSkillEffects`, which does not catch (m5b2-plan.md D11) — **the character cannot enter the world** | **W** | the gate seeds a Templar (D4); the class goes to **M5e** (a real Gladiator reaches 15 by play); M5f takes it as O (X-04) if M5e did not |
| W-15 | a Daeva seed at level 16 → `PlayerController.onLevelChange(1, 16)` at enter world (PlayerEnterWorldService.java:204) → `SkillLearnService.learnNewSkills` | ported (`SkillLearnService.cpp:80`); the Templar's 38 autolearn skills ≤ 16 are added; their **actives** use unported classes (`MPHealInstantEffect`, `HostileUpEffect`, `TargetChangeEffect`, …) but are only reached when cast | D | the gate casts only 243 |
| W-16 | first automated entry into **Sanctum** / **Pandaemonium** | unknown (m5c W-16 measures Sanctum first) | unknown | inherited from M5c's gate |
| W-17 | `PvpMapService::init` at startup once `getNextAvailableInstance` exists | the partial `PvpMapService.cpp:32` — closing it would create the 301220000 instance with `PvpMapHandler` (39 U, 858 Java lines) **at every startup, enabled or not** (PvpMapService.java:28) | **W** | **kept partial** (D6) |
| W-18 | teleport scrolls (`ReturnPointEffect`, `MultiReturnAction`) | `TeleportService::useTeleportScroll` (U) | D (item use: A-03) | X-01..X-03 (O) |
| W-19 | `DialogService` `MATCH_MAKER`, `PortalDialogAI` `INSTANCE_PARTY_MATCH`/`OPEN_INSTANCE_RECRUIT` | `PeriodicInstanceManager`, `AutoGroupType`, `FindGroupService` | D (dredgion/recruit npcs are not on the path) | M5g |
| W-20 | **the logout or disconnect of a player who is dead inside an instance** (or on 400030000) — `PlayerLeaveWorldService.cpp:118-120` (Java :95-97). M5f is the first milestone with a live instance a player can die in (Haramel: 76 `aggressive` spots, W-11) | `PlayerReviveService::instanceRevive` (`PlayerReviveService.cpp:102/106`, U). The throw leaves `leaveWorld` at `:120`, so everything after it — storing effects, cooldowns and life stats, the team logouts, **`getController().delete_()` at `:154`**, the position save — is skipped (only the `LogoutBreakers` finally guard at `:85` runs): the character stays in the world as a ghost, the leaked-Player kind of defect the user's sessions have found before | **W** | **T-06, required** (its callees are ported: `revive`, the 5-argument `teleportTo` at `TeleportService.cpp:236`, `bindRevive`, `getStartPos`; `teleportToEvent` is T-02's); unit cases in T-08, a checklist line (§11 step 13) |
| W-21 | a real player fighting in Haramel | Drudgelord Kakiti (216897, `aggressive`, level 18, 1 spot, **61.9 m** from the entry) casts 19214 *Borrowed Life* → `SpellAtkDrainInstantEffect` (`SpellAtkDrainInstantEffect.cpp:8`, U). Every other Haramel monster skill is in the ported set at `760e8ab5c` (measured over `npc_skills.xml`, `skill_templates.xml`, `Effects.java`'s bindings and the C++ tree, data-only generated classes such as `SpellAttackInstantEffect` counted as ported) | **W** (the gate stays within 14 m of the entry and does not reach it) | **X-05a, required** (D15) |
| W-22 | a real player fighting on the maps a teleporter now reaches past Verteron/Altgard | measured, npc ids with an unported class (spots): **Eltnen** — beyond A-10: `SpellAtkDrainInstantEffect` 20 (162), `MpAttackInstantEffect` 15 (122), `DiseaseEffect` 11 (113), `ConfuseEffect` 9 (65), `ProtectEffect` 8 (38), `FpAttackEffect` 9 (35), `FearEffect` 4 (16), `MagicCounterAtkEffect` 3 (8), `DispelDebuffMentalEffect` 3 (4), `CloseAerialEffect` 2 (2), `DelayedFpAtkInstantEffect` 1 (1); **Morheim** — `SpellAtkDrainInstantEffect` 16 (303), `FpAttackEffect` 27 (152), `ProtectEffect` 7 (106), `ConfuseEffect` 13 (101), `DiseaseEffect` 5 (92), `DelayedFpAtkInstantEffect` 8 (25), `MpAttackInstantEffect` 8 (24), `DispelBuffEffect` 4 (17), `FearEffect` 3 (15), `CloseAerialEffect` 1 (1). (Also still U today and delivered by A-10/M5e: Poison, Blind, Dispel, Paralyze, Deform, Sleep, Silence, Bind, Fall, …; Verteron's and Altgard's own lists equal m5e's 11 classes.) Near the arrivals every holder is one of the arrival town's own-race guards: all 15 spots within 200 m of Morheim loc 10 (207593 at 9 m, 204391, 204314, 207587, 204315, 204434, 207592, 204711, 207585 — `race="ASMODIANS" tribe="GUARD_DARK"`) and all 4 within 200 m of Eltnen loc 5 (203938 at 11 m, 207557, 203904 — `race="ELYOS" tribe="GUARD"`), so arriving and standing in town is safe | **W** for a real player; D for the gate (it arrives in Morheim and fights nothing) | **X-05b, optional** (D15); if it does not land, the checklist does not fight on Eltnen or Morheim |

### 2.9 The travel data of the four start maps and the capitals (lesson 3)

Parsed with ElementTree from `spawns/Npcs/*.xml`, `npc_teleporter.xml`, `teleport_location.xml`, `flypath_template.xml`, `hotspot_template.xml`,
`bind_points/bind_points.xml`, `portals/portal_template2.xml`, `portals/portal_loc.xml`, `instance_cooltimes.xml`, `instance_exit.xml`; prices
through `PricesService.getPriceForService` with the siege-off influence (global 125 %, modifier 100 %, taxes 113 %, m5c §2.10). **G-01
re-derives every number; the gate asserts the oracle's output, never these constants.**

| Map | What a **level-1** character can use | What needs a **Daeva** |
|---|---|---|
| **Poeta** 210010000 (1,029 spots) | flight masters **203070 Kustanon** (Akarios, 803.9/1242.2/119.0) → loc 13 Melponeh's Campsite, `teleportid 5001`, 160 → **226** kinah; **203083 Aero** (Melponeh, 425.4/1739.6/119.9) → loc 12 Akarios, `6001`, 226. Obelisks **700013** (Akarios, 853.1/1206.8/118.6, 47 kinah) and **700014** (Melponeh, 423.1/1742.2/120.7, 143). Hotspots **13** Akarios (807/1242/119), **14** Daminu Forest (560/1382/119), **15** Melponeh (427/1741/120), base 44. Statue **730531** (558.3/1382.1/119.1, `portal_dialog`) → 2100100 Akarios / 2100101 Melponeh | **203194 Daines** (804.9/1244.6/119.0): loc 2 Sanctum (100 → 141, `required_quest` 1006), loc 4 Verteron (800 → **1130**) |
| **Ishalgen** 220010000 (1,386) | **203513 Sheofin** → loc 18 Anturoon Crossing (`11001`); **203545 Garhara** → loc 17 Aldelle (`12001`); obelisks 700063 (43), 700064 (134); hotspots 16-18; statue 730532 | **203679 Osmar**: loc 7 Pandaemonium (rq 2008), loc 9 Altgard (1130) |
| **Verteron** 210030000 (2,328) | – | **203091 Urakron** (1639.1/1498.8/120.0): Sanctum 500 → 706, Poeta, Eltnen, Heiron, Theobomos; flight masters 203120, 203159, 203173, 205248, 205249; obelisks 700015-700017, 700019, 700020; hotspots 19-23 (base 387); **Haramel entrance 730318** (2539.3/834.9/104.0, `ai="portal"`, delay 3 s, talk 5); abyss gate 700088 → Aerdina (quest 1020/14016), 730059 → Reshanta (quest 1044) |
| **Altgard** 220030000 (2,464) | – | **203581 Ukin** (1754.1/1805.1/255.9): Pandaemonium, Ishalgen, **Morheim 1700 → 2401**, Beluslan, Brusthonin; flight masters 203561, 203678, 203683, 205258, 205259; obelisks; **Haramel 730319**; 700089 → Bregirun (quest 2022/24016) |
| Sanctum / Pandaemonium | – | 203726 Polyidus / 204191 Doman (to every Elyos/Asmodian field map; Inggison/Gelkmaros 9300 → 13,136); inner statues; 730218/730219 aerolinks (siege worlds, W-03) |

**Haramel (300200000)**: `instance_cooltime` id 46, `DAILY`, `ent_cool_time 900` (09:00 server time), `maxcount 16`, max members 1/1,
**min level 16**; entry `portal_loc 3002000` (172, 20, 144.22548, h 60); exit portal **730320** at (185.738, 20.1436, 144.224) — **13.7 m** from
the entry, talk distance 5, delay 3 s — to `2100301` (2533.8564, 835.055, 103.967476, h 59) / `2200301`; `instance_exit` gives the same points
(`instance_exit.xml:16-17`). Nearest aggressive npc to the entry: 31.5 m (216899, `srange 8`); Drudgelord Kakiti (216897, the one unported
monster skill, W-21) stands 61.9 m from it. **32 of the 110 instance cooltime entries are
solo**; every other instance a player reaches needs a group (M5g) or a quest script (phase 6).

**The gate's two Daeva seeds** (D4; `player_experience_table.xml`: level 16 starts at **844,378** exp, `getStartExpForLevel(16) = exp[15]`,
PlayerExperienceTable.java:29-34): Elyos and Asmodian **TEMPLAR** (W-14). **The hotspot price from the Elyos spawn** (1212.9423, 1044.8516,
140.75568) **to hotspot 13**: float distance 451.807 → 44 + 19 = **63** kinah.

### 2.10 The client packets (lesson 4)

Measured against `network/aion/clientpackets`: **42** C++ `CM_*` classes of Java's **188**, i.e. **146** with no C++ file (phase5-roadmap.md's 148 and
the M5f brief's 147 predate `CM_CASTSPELL` and `CM_REMOVE_ALTERED_STATE`; the M5b-2 part-3 working tree adds none).

| Packet | Java lines | Chunk | Need | Why |
|---|---|---|---|---|
| `CM_TELEPORT_SELECT` | 70 | P5-16 | **R** | the teleporter map's choice |
| `CM_TELEPORT_ANIMATION_DONE` | 51 | P5-16 | **R** | **without it an animated teleport never arrives**: the player stays despawned with a pending `TELEPORT` task |
| `CM_BIND_POINT_TELEPORT` | 47 | P5-15 | **R** | the hotspot teleport |
| `CM_INSTANCE_LEAVE` | 30 | P5-15 | **R** | the client's "leave instance" button (a no-op for `GeneralInstanceHandler`, D7) |
| `CM_MOVE_IN_AIR` | 61 | P5-16 | **R** | the flight path; without it the server's position freezes for the whole flight |
| `CM_POSITION_SELF` | 25 | P5-16 | O | "C_BLINK" (`ClientPacketInfo.gen.inc:33`); empty `runImpl`; only silences an unknown-packet line if the 4.8 client sends it |
| `CM_CHANGE_CHANNEL` | 40 | P5-15 | O | channels: default `max.twincount.usual = 1` (world.properties:13) → one channel |
| `CM_PLAY_MOVIE_END` | 57 | P5-16 | O | M5d's W; M5f needs it only for `HaramelInstance`'s movie 457 (H-01) |
| `CM_WINDSTREAM` | 93 | P5-16 | **D** | no windstream on any map before level 45 (`windstreams.xml`: 900030000, 210050000, 220070000, 300020000, 300250000) → M5j |
| `CM_RECALLED_BY_OTHER_ANSWER` | 38 | P5-16 | D | summon group member → M5g |
| `CM_OPEN_STATICDOOR` | 35 | P5-16 | D | instance doors: no static door in Haramel (`staticdoor_templates.xml`, 0 rows for 300200000); group instances → M5g / phase 6 |
| `CM_HOUSE_TELEPORT`, `CM_HOUSE_TELEPORT_BACK` | – | P5-15 | D | M5h |
| `CM_SHOW_MAP` | 44 | P5-16 | D | action 0 = conqueror/protector intruder scan → M5i |
| from earlier milestones | | | entry criteria | `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_QUESTION_RESPONSE` (A-04), `CM_USE_ITEM` (A-03), `CM_CASTSPELL` (A-01) |

(Owners from `chunks.py owner` on the would-be paths: P5-15 = `CM_[A-K]` — `CM_BIND_POINT_TELEPORT`, `CM_CHANGE_CHANNEL`, `CM_INSTANCE_LEAVE`,
`CM_HOUSE_TELEPORT*`; P5-16 = the rest — `CM_MOVE_IN_AIR`, `CM_TELEPORT_SELECT`, `CM_TELEPORT_ANIMATION_DONE`, `CM_POSITION_SELF`,
`CM_PLAY_MOVIE_END`, `CM_WINDSTREAM`, `CM_RECALLED_BY_OTHER_ANSWER`; chunks.cmake:421-438.)

### 2.11 Where rifts, windstreams, kisks, recall and scrolls belong

| Feature | Home | Evidence |
|---|---|---|
| Rifts (`RiftService`, `RiftManager`, `RVController`, `RiftProtectorAI`) | **M5i** (D1) | no rift on the four maps (§1 finding 5); P5-12b; `gameserver.rift.enable = true` by default (custom.properties:77), so the user sees rifts only on Eltnen/Morheim and beyond; M5f makes `RVController`'s `teleportTo` calls (RVController.java:112, 139) work |
| Windstreams | **M5j** | §2.10 |
| Kisks (`KiskAI` 93 lines, `InvisiblekiskAI` 34, `KiskService` 2 U, `kiskRevive` 2 U, `ToyPetSpawnAction`) | **M5j** | the kisk is an item-summoned npc; m5c §3a sends `ToyPetSpawnAction` to M5j |
| Recall / summon group member (`RecallService` 4 U, `CM_RECALLED_BY_OTHER_ANSWER`) | **M5g** | needs a group member target; A-08's signature change is applied here |
| Teleport scrolls (`ReturnPointEffect`, `MultiReturnAction`), `InstanceTimeClear` | **M5f, optional** | m5c §3a (`:401`); need A-03 |
| House teleports | M5h | – |

---

## 3. Where this plan disagrees with the roadmap

| phase5-roadmap.md (`:31`) | This plan | Why |
|---|---|---|
| M5f = "P5-08 (teleport), P5-13" | **P5-08 43 (38 sites + 3 anonymous + `instanceRevive` ×2), P5-13 23 + 2 partials, P5-05 1 AI, A1 2 AIs (lease), P5-15/16 5 packets, P5-12a 2 (lease), P4-10 1 + the no-op handler class (lease), P5-04 1 (X-05a), P5-02a 1 line (lease, T-07)**; optional P5-07, P5-04, P5-03 (X-05b: 27 sites), I1, Q06 | §2.6 |
| "P5-13 instances and restrictions … 118" | **M5f takes 23 of 118 (+ 2 of its 3 partials)**; 1 stays (D10), 52 are custom features (PvP map, Roah) → M5j, 28 scored/registered group instances (incl. all 9 of `PeriodicInstanceManager`) → M5g/M5i and later, 14 restrictions and transfers elsewhere | §2.6 |
| (implicitly) the travel milestone opens the maps it reaches | it opens them for **travel**; fighting on Eltnen and Morheim needs 11 effect classes nobody has planned (W-22) — optional here (X-05b), required only for Haramel's one class (X-05a) | D15 |
| "the instance engine" | the engine **and the base every phase-6 instance handler calls** (`GeneralInstanceHandler`), but **no instance handler**; group entry is ported blind and verified by M5g | D2, D3 |
| (implicitly) rifts with travel | **M5i** | D1 |
| (implicitly) a player can use the teleporters | only a **Daeva** can leave the start maps; the real client needs M5e's `simple.secondclass` or SQL (A-07) | §1 finding 2 |

---

## 4. Decisions

Decisions the integrator takes under the standing instruction (phase5-roadmap.md:58-63) unless marked **user**.

| # | Decision | Why |
|---|---|---|
| **D1** | **Rifts are not M5f; they stay with M5i.** | §2.11: no rift on the start maps, and rifts are P5-12b's world-event machinery (spawn timers, `RVController`, entry limits), not travel |
| **D2** | **No phase-6 instance handler is required. `GeneralInstanceHandler` is ported whole (11 bodies)** because it is the base of all 73 `@InstanceID` handlers (48 extend it directly). **`HaramelInstance` is optional (H-01, I1 lease)**: its only hook is the boss's `onDie` (chest per class, movie 457, the dimensional gate 700852, HaramelInstance.java:23-61), which the gate does not reach | a map without a handler gets `GeneralInstanceHandler` (InstanceEngine.java:49); the boss's AI (`summoner`) is unported anyway (W-11) |
| **D3** | **The gate enters only solo (`maxPlayers` 1) and open (`maxPlayers` 0) destinations.** `PortalService`'s group, alliance and league arms are ported faithfully and unit-tested on the arms that need no team; **M5g's gate re-verifies them** (a `PlayerGroup` is P5-10, 294 U). The staff bypass (`AdminConfig.INSTANCE_ENTER_ALL`) is not used | groups come after M5f in the roadmap |
| **D4** | **The gate seeds rather than plays what M5f does not own** (m5c D5 precedent, A-05): two Daeva **Templars** at level 16 (`player_class = TEMPLAR`, `player_quests (1006 \| 2008, COMPLETE)`, `exp = 844,378`), their kinah row and positions (one relog before the Haramel cases). **The level-1 character is not seeded at all**: the hotspot takes it from the spawn to Akarios (§2.9) | the Gladiator trap (W-14); walking 900 m of Verteron is not M5f's feature; ascension is a phase-6 quest |
| **D5** | **user** — **The ascension quests `_1006Ascension` / `_2008Ascension` (592 Java lines, Q06) stay phase 6 by default; an optional stage-3 lane can pull them forward** (H-02). They need M5d's handler base, M5e's `ClassChangeService.setClass`, and M5f's instance creation (`getNextAvailableInstance(KARAMATIS_B, player)`, _1006Ascension.java:98-99), flight emotion (:157-161) and same-map beam teleports. The recommendation is **no**: the real client reaches Daeva through `gameserver.simple.secondclass.enable` (A-07) and the gate through the seed | this changes what the user plays at level 9, so it is the user's call |
| **D6** | **`PvpMapService.cpp:32` stays `AION_PARTIAL`** (its row stays in every allow-list); `PvpMapHandler` (39 U, 858 lines) and `CustomInstanceService` (13) go to M5j | closing it creates the PvP-map instance at every startup even when `pvpmap.enable = false` (PvpMapService.java:26-29, custom.properties:217) — a lesson-2 wake for a disabled custom feature |
| **D7** | **Java's travel quirks are kept and named, not fixed**: the fly-path validator keyed by `locId` (§2.2 step 2); `CM_TELEPORT_SELECT` does not repeat the dialog's Daeva check (a crafted packet reaches Verteron from Poeta if it can pay — CM_TELEPORT_SELECT.java vs DialogService.java:188-195); `CM_INSTANCE_LEAVE` is a no-op for `GeneralInstanceHandler`; the hotspot takes `max(server, client)` price; the hotspot cooldown lives in memory (lost on restart); the double `SM_PLAYER_SPAWN` at a login into a destroyed instance (§2.5 step 10); `SM_CHANNEL_INFO` is always (1, 1) during a teleport because the player is not spawned yet (SM_CHANNEL_INFO.java constructor) | faithfulness beats a nicer engine (m5b2-plan.md D9); each quirk is a gate row or a checklist warning so it is visible |
| **D8** | **The two siege bodies on the path are ported under a P5-12a lease** (T-05), not left loud | W-02 is on *every* npc teleport; W-03 on every siege-world arrival |
| **D9** | **`TeleportService` is ported whole (21 + 1)**, including the arms M5f's gate cannot reach (`teleportToPrison`, `teleportToEvent`, `setEventPos`, `sendTeleportRequest`, `useTeleportScroll`, `moveToTargetWithDistance`, `changeChannel`) | 124 phase-6 handler files and 31 other source files call it (272 `teleportTo` sites) — the phase-6 prerequisite, like M5d's handler base; each is ≤ 30 Java lines |
| **D10** | **`InstanceEngine::addInstanceHandlerClass` stays `AION_UNPORTED`** with a deviation row: in C++ handlers register through `AION_INSTANCE_HANDLER` markers at compile time (`InstanceEngine.cpp:21-26`), so the Java class-loader callback has no caller | an unreachable body; porting it would invent a runtime registration path |
| **D11** | **The gate profile sets `gameserver.instance.solo.destroy_delay_seconds = 1`** (default 600, instance.properties:22) so the first checker run (60 s after creation, hard-coded, InstanceService.java:58) destroys an empty Haramel; `STR_MSG_LEAVE_INSTANCE`'s parameter becomes `1 / 60 = 0`, asserted as such. **The real-client session sets it back to 600** (§11 prerequisites): the checklist's 10-minute steps are written for the default | a 600-s delay makes a destroy case cost 11 minutes |
| **D12** | **The first writes of `player_bind_point` (insert and update) and `portal_cooldowns` are gate assertions, read back from the database** (m5c D11's "first use" rule) | the DAOs are ported (`PlayerBindPointDAO.cpp:91`) and never ran against MySQL/Postgres |
| **D13** | **`destroyInstance` erases the `InstanceScaler::scalings` entry although scaling is off by default** (`instance.scaling.enable = false`, instance.properties:34; `canScale` also requires `maxPlayers > 1`, InstanceScaler.java:40-42) | the TODO at `InstanceService.cpp:98-100`: Java's `WeakHashMap` forgets the instance by itself, the port's strong map would not |
| **D14** | **The Haramel boss stays inert** (`SummonerAI`, W-11) and the checklist says so | a root AI handler of the boss's kind is phase 6 / M5j |
| **D15** | **The effect boundary, re-drawn for travel** (rev 2): (a) **`SpellAtkDrainInstantEffect` is required (X-05a)** — the only monster skill of Haramel outside the ported set (W-21), 1 site, 40 Java lines — unless M5e's optional T-02 already took it; (b) **the 11 further classes of Eltnen's and Morheim's monsters are optional (X-05b**, 27 sites + ~5 inner bodies, 574 Java lines, W-22); (c) m5b2-plan.md D6's rule stays: every class outside the ported set stays `AION_UNPORTED` and throws, no blanket `AION_PARTIAL`; (d) **if X-05b does not land, the real-client checklist does not fight on Eltnen or Morheim** (arrive, look around the arrival town, leave) and says why; if A-10 is short, the same holds for Verteron and Altgard | Haramel is the one instance M5f opens to a real player, and one 40-line class makes it whole; Eltnen and Morheim are level-20+ maps (m5e D4's reasoning, m5e-plan.md:438) whose classes m5e O-06 handed forward without an owner — M5f is the first milestone that can carry a player there, so it names them and offers them, but does not make travel wait for combat |

---

## 5. Work items

Effort is **size, not time** (m5c-plan.md §5, `:440-444`; rev 1's agent-day letters were not calibrated and its day budget contradicted
them): **S** ≤ 10 bodies or ≤ 250 Java lines, **M** ≤ 30 / ≤ 700, **L** ≤ 60 / ≤ 1,500, **XL** beyond; a test item is sized by the bodies it
covers. The measured pace to set it against (git log, `git diff` counts of removed `AION_UNPORTED(` lines): M5b-1 went from its plan commit
`5f65cb14f` (2026-09-22 02:29) through stage 1 `340c05c5e` (16:40, 121 sites) to its gate `23c4e6485` (2026-09-23 02:32), about 24 h; M5b-2
part 2 `c1edb0afb` removed 295 sites in about 4 h after part 1 `29009d778` (14:36 → 18:49), and part 3 `760e8ab5c` 105 more in about 4 h
(→ 22:58) — roughly 50 bodies per lane per part. Need: **R** required, **W** stub-with-warning allowed, **O** optional. The last column names
the §0 assumption an item stands on.

### Integrator (stage 0)

| Id | What | Deps | Need | Eff | A |
|---|---|---|---|---|---|
| I-01 | **Leases** (one active per chunk, released at merge): A1 → `handlers/ai/portals/{PortalAI,PortalDialogAI}.*` (+ its tests: `tests/handlers_ai_core` under the lease, or the derived `tests/handlers_ai_world` — decide here); P4-10 → `world/WorldMapInstance.cpp` (one body **and the file-local no-op handler class**, N-02/N-07); P5-12a → `services/SiegeService.cpp` (two bodies); **P5-02a → `skillengine/model/Skill.cpp:858-860` (one caller line, T-07; dropped if A-08 held)**. Rev 1's P5-10 lease on `AutoGroupService.cpp` is gone (W-08). Optional: I1 → `handlers/instance/HaramelInstance.*`; Q06 → `handlers/quest/ascension/_1006*`, `_2008*` | – | R | S | A-06 (m5d's A1 lease released), A-08 |
| I-02 | Header requests of §7 (the m5b2-p2-9 signature if M5e did not apply it) | – | R | S | A-08 |
| I-03 | `game-server/config/m5f.properties.example` in the Java tree (m5b-plan.md I-01's location) | – | R | S | – |
| I-04 | Allow-list row deletions coordinated with G-05 (`InstanceService.cpp:133, 153` in every gate's list, in the commit that closes them) | N-03 | R | S | A-09 |

### Stage 1, teleport (P5-08, + the P5-12a lease and the one-line P5-02a lease)

| Id | What | Java refs | Deps | Need | Eff | A |
|---|---|---|---|---|---|---|
| **T-01** | The npc path: `teleportToFirstTeleportLocation`, **`teleport`** (both arms), `validateTeleporterAndGetTemplate`, **`checkKinahForTransportation`**, **`showMap`** (5 bodies, ~120 Java lines) | TeleportService.java:65-177, 291-295 | T-05 | R | S | A-02, A-04 |
| **T-02** | The rest of `TeleportService` (D9, 16 sites + the anonymous `acceptRequest`): `teleportTo(WorldPosition&)`, `teleportDeadTo`, the five delegating overloads (**incl. the `int instanceId` one at `TeleportService.cpp:255` that the same-map portal arm uses**, PortalService.java:118-120), **`moveToInstanceExit`**, **`teleportToNpc`** (the only geo-z teleport, :304-333), `teleportToPrison`, `moveToTargetWithDistance`, `useTeleportScroll`, `changeChannel`, `setEventPos`, `teleportToEvent`, `sendTeleportRequest` + the anonymous `acceptRequest` | :221-289, 297-333, 385-479 | **N-03** (`getOrRegisterInstance`, called by `teleportToNpc` for an instance map, TeleportService.java:318-319) | R | M | – |
| **T-03** | **`PortalService`** — 13 bodies: `port` ×2, the **nine** checks (`checkMentor`, `checkEnterLevel`, `checkRace`, `checkSiegeId`, `checkRank`, `checkPlayerSize`, `checkTitle`, `checkQuests`, `checkAndRemoveRequiredItems`), `port(private)`, `transfer` | PortalService.java:45-367 | N-01 | R | M | A-02 (`checkAndRemoveRequiredItems`) |
| **T-04** | **`BindPointTeleportService`**: `teleport` (+ the two anonymous `Runnable`s `$1`/`$2`, defined in the `.cpp`, §2.7), `cancelTeleport`, `calculateTeleportationPrice` (float distance), `checkRequirements` | BindPointTeleportService.java:39-120 | – | R | S | A-02 |
| **T-05** | Lease P5-12a: `SiegeService::getSiegeIdByLocId` (the switch, SiegeService.java:612-…), `onEnterSiegeWorld` (:590-605) | SiegeService.java | I-01 | R | S | – |
| **T-06** | **`PlayerReviveService::instanceRevive` ×2 — required** (W-20: every logout of a dead player inside an instance reaches it, `PlayerLeaveWorldService.cpp:118-120`; and `CM_REVIVE(INSTANCE_REVIVE)` once a handler allows it, e.g. H-01); `kiskRevive` ×2 stay **W** (kisks are M5j) | PlayerReviveService.java:135-187 | N-04 (the handler's `onReviveEvent` is already inline, `GeneralInstanceHandler.h:76`); `teleportToEvent` (T-02) for the event-mode arm | R (`instanceRevive`), W (`kiskRevive`) | S | – |
| T-07 | Apply header request m5b2-p2-9 (`RecallService::validateCast(Player&, Ptr<VisibleObject>)`) **and, in the same commit, its caller `Skill.cpp:858-860`** under I-01's one-line P5-02a lease (a signature change without its caller does not build) | header-requests.md:410 | I-02 | R | S | A-08 |
| **T-08** | Tests (`tests/playersvc`): the price table (`getPriceForService` × the teleloc prices of §2.9, `HiPass` → 1); `validateTeleporterAndGetTemplate`'s four refusals; `teleport` FLIGHT vs REGULAR (state, emotion, no `SM_TELEPORT_LOC`); required quest; `getSiegeIdByLocId` rows; **the hotspot price with float distance** against the oracle's vector; hotspot cooldown and cancel on a `DeterministicExecutor`/`ManualClock` (charge at 10 s, move at 11 s); `PortalService`'s decision table for maxPlayers 0/1 (cooldown refusal, level error page vs system message, reenter keeps the cooldown count, **the same-map arm moves with `NONE`**); `teleportToNpc`'s z fallback (`spot.z + 0.5` when geo is off); **`instanceRevive`**: the `startPos` arm (revive 25 %, soul sickness, the move to `startPos` on the same instance), **the bind arm when `startPos` is null**, the `onReviveEvent`-true arm with a scripted test handler, and **a `leaveWorld` of a dead player inside an instance that completes** (the player leaves the world, `delete_()` ran, the position saved is `startPos`); **`cycles.toml` rows after `fieldmap.py --class`**: `BindPointTeleportService$1` (stored as `SKILL_USE`: a K-06-style cycle row, cut by the task's completion or `cancelTask`), `BindPointTeleportService$2` (unstored one-shot: an `accepted: one-shot task` row) and `TeleportService$1` (the anonymous request handler). Mutation-proven | – | T-01..T-06 | R | L | – |

### Stage 1, the instance engine (P5-13, + the P4-10 lease)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **N-01** | `getNextAvailableInstance` ×4 and the **`EmptyInstanceCheckerTask`** ×4 (`canDestroyInstance`, `isRegisteredTeamDisbanded`, `calculateDestroyTime`, `run`) | InstanceService.java:39-77, 167-198 | – | R | S |
| **N-02** | **`destroyInstance`** + the C++ breakers: `WorldMapInstance::detachInstanceHandler` (P4-10 lease; installs N-07's no-op handler, `cycles.toml:354`), `setStartPos(nullptr)`, `releaseRegisteredTeam`, the `InstanceScaler::scalings` erase (D13); the forced-leave arm for players inside (`STR_MSG_LEAVE_INSTANCE_FORCE` + `moveToExitPoint`) | :82-107 | N-01, N-07 | R | S |
| **N-03** | **`onLeaveInstance`** (the message arms; the `AUTO_GROUP_ENABLE` arm is ported faithfully and stays unreachable, W-08), `getOrRegisterInstance`, **close the two M5a partials**: `getOrCreatePersonalInstance` (`:133`, one call) and `moveToExitPoint` (`:153` → `TeleportService::moveToInstanceExit`, T-02); `getOrCreateHouseInstance` **W** (studios are M5h) | :109-161, 210-224 | N-01, T-02 (the partial closure only) | R | S |
| **N-04** | **`GeneralInstanceHandler`** — 11: `onLeaveInstance`, `removeInstanceItems`, `isRestrictedToInstance`, `spawn` ×2, `spawnAndSetRespawn`, `getNpc`, `deleteAliveNpcs`, `sendMsg` ×2, and `portToStartPosition` = Java's `throw new UnsupportedOperationException()` (:239-241) | GeneralInstanceHandler.java:61-292 | – | R | M |
| **N-07** | **The no-op `InstanceHandler`** that `detachInstanceHandler` installs (§2.5 step 9; single-thread-synthesis.md:56, 272): **a file-local class in `WorldMapInstance.cpp`** (P4-10, the lease I-01 already takes — no header, so no request), one static instance, **not reference-counted** (`retain`/`release` are no-ops, `InstanceHandler.h:116-119`), holding no `WorldMapInstance`. 43 overrides (`InstanceHandler.h:31-119`): every `void` body empty; every value answers what `GeneralInstanceHandler` answers **for an instance map** without touching an instance (`onReviveEvent` false, `onDie(Player&, Creature&)` false, `getStage` `DEFAULT`, `getInstanceScore` null, `onPassFlyingRing` false, `canEnter` true, `getExpMultiplier` 1.5f, `getApMultiplier` 1f, `allowSelfReviveBySkill`/`ByItem` true, `allowKiskRevive` false, `allowInstanceRevive` false — GeneralInstanceHandler.java:87-276); `portToStartPosition` throws as the base does. **A deviation** (a destroyed instance answers with the no-op handler, where Java keeps calling the real one until it is collected): a row in `docs/deviations/P4-10.md`. The alternative — a header in `instance/handlers` (P5-13) — needs a header request and buys nothing | single-thread-synthesis.md:56; `WorldMapInstance.cpp:165-168`, `.h:169-176` | – | R | L (43 one-line bodies, ~90 C++ lines) |
| **N-05** | Tests (`tests/instance`): **the lifecycle on a `DeterministicExecutor` + `ManualClock`**: create (spawns counted), register, a player enters and leaves, the checker's three destroy conditions (empty + delay, personal, disbanded team — the last with a fabricated `GeneralTeam` if one can be built, else W), `destroyInstance` with a player inside (forced leave), **and then: every `Npc` of the instance, its AIs, the handler, the `WorldMapInstance` and the checker are reclaimed** (`liveCounts()` back to the pre-create values, `Reclaimer::reclaimNow`, `LeakCensus::getLeaks()` empty); **after `destroyInstance`, `getInstanceHandler()` is non-null, is not the destroyed `GeneralInstanceHandler`, and a call on it (e.g. `onDie(npc)`, `getExpMultiplier()`) neither throws nor retains anything** (N-07); `onLeaveInstance`'s three messages; `removeInstanceItems` with an item restricted to the world; `moveToExitPoint` with and without an `instance_exit` row; a relog inside a live and inside a destroyed instance; **a logout while dead inside a live instance ends at `startPos`, and at the bind point when `startPos` is null** (with T-06). Mutation-proven | – | N-01..N-04, N-07, T-06 | R | L |

### Stage 1, client packets (P5-15, P5-16)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| P-01 | `CM_TELEPORT_SELECT` (`readD` target, `readD` locId, `readH`) + `runImpl` | CM_TELEPORT_SELECT.java:39-69 | T-01 | R | S |
| P-02 | `CM_TELEPORT_ANIMATION_DONE` (no body; `getAndRemoveTask(TELEPORT)`, `runNowIfPending`, `get()`, the catch → `SM_PLAYER_INFO` + spawn) | CM_TELEPORT_ANIMATION_DONE.java:30-50 | – | R | S |
| P-03 | `CM_BIND_POINT_TELEPORT` (`readC` action; action 1: `readD` locId, `readQ` kinah) | CM_BIND_POINT_TELEPORT.java:24-46 | T-04 | R | S |
| P-04 | `CM_INSTANCE_LEAVE` | CM_INSTANCE_LEAVE.java:19-29 | – | R | S |
| P-05 | `CM_MOVE_IN_AIR` (`readD` world, `readF` ×3, `readC` heading, `readD` distance) | CM_MOVE_IN_AIR.java:34-60 | – | R | S |
| P-06 | O: `CM_POSITION_SELF`, `CM_CHANGE_CHANNEL` (+ WORLD_EMULATE_FASTTRACK arm), `CM_PLAY_MOVIE_END` if M5d did not | – | T-02 | O | S |
| P-07 | Tests: `tests/cm_ak`, `tests/cm_lz` byte vectors per packet (every arm of `CM_BIND_POINT_TELEPORT`), the `AION_CLIENT_PACKET` markers, in-process run tests over `InWorldPacketRunSupport.h` (`CM_TELEPORT_ANIMATION_DONE` with no task, a done task and a throwing task; `CM_MOVE_IN_AIR` ignored when not `FLYING`) | – | P-01..P-05 | R | M |

### Stage 1, travel npcs (P5-05, + the A1 lease)

| Id | What | Java refs | Deps | Need | Eff | A |
|---|---|---|---|---|---|---|
| V-01 | `ResurrectAI` (`ai="resurrect"`) + its anonymous `AIRequest` (4 bodies) | ResurrectAI.java:34-107 | – | R | S | A-04 |
| V-02 | `PortalAI` (`ai="portal"`, 5 bodies) | PortalAI.java:21-57 | T-03 | R | S | A-06 |
| V-03 | `PortalDialogAI` (`ai="portal_dialog"`, 5 bodies) incl. its ten hard-coded npc ids (six page arms) | PortalDialogAI.java:29-170 | T-03 | R | S | A-04, A-06 |
| V-04 | `ActionItemNpcAI` (7 bodies incl. the observer's `abort`) — **only if A-06 fails** | ActionItemNpcAI.java | A-04 | W | S | – |
| V-05 | Tests (`tests/handlers_ai_core` + the lease's): the obelisk refusals (already bound within 20 m, race/tribe, prison, far > 5 m on accept, not enough kinah), the bind stored at the **player's** position; `PortalAI` use bar abort by the observer; `PortalDialogAI.checkDialog`'s page choice | – | V-01..V-03 | R | M | – |

### Stage 1, gate harness (P5-SC, `tools/oracle`)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-01** | **`oracle.py m5f-travel`**: `--npc ID` → teleporter template, its locations with type, `teleportid`, price → `getPriceForService` (PricesService.java:86-90, three truncating multiplications; its factors from m5c's `race_prices`/`times_div_100d` in `tools/oracle/m5c/trade.py` and `load_config` in `m5c/trade_config.py` — **A-11**; if they are absent at branch time, G-01 writes the ~30 lines itself), required quest, destination xyz/h, the Daeva gate of DialogService.java:188-195; `--hotspot ID --from x,y,z` → the price with Java float distance (`m5a/javafloat.py`); `--obelisk ID` → price; `--portal NPC --race` → path, loc, cooltime row (id, type, maxcount, min level, members), the next reuse time for `DAILY`/`WEEKLY` at a given clock, the exit; `--instance-spawns WORLD --difficulty 0 --near x,y,z --radius` → the `spawnInstance` npc set (reuse `m5a-spawns`' rules; temporary spawns by game time); `--exp-for-level L`; `--census` → re-derives §2.9 (the level-1 and Daeva tables), W-14's per-class passive check and **W-21/W-22's monster-skill classes per map** (`npc_skills.xml` × `skill_templates.xml` × `Effects.java`'s bindings × the C++ tree, data-only generated classes counted as ported) against the C++ tree. `--geo-check` → the data-z sanity list of §10.5 G3 through `tools/oracle/geo/probes.py`. Plus `tools/oracle` tests | A-11 | R | M |
| **G-02** | `decoders/TravelDecoders.{h,cpp}` from the Java `writeImpl`: `SM_TELEPORT_MAP` (D, H), `SM_TELEPORT_LOC` (C, D, D, F ×3, C), `SM_CHANNEL_INFO` (D, D), `SM_BIND_POINT_TELEPORT` (C action, D player; action 1: D loc; action 3: D loc, D cooldown; SM_BIND_POINT_TELEPORT.java writeImpl), `SM_BIND_POINT_INFO` (C, C, D, F ×3, D), `SM_INSTANCE_INFO` (C, D, C, H, per player D, H, per instance D, D, D, D, D, C, then S), `SM_INSTANCE_COUNT_INFO` (D, D, D), `SM_USE_OBJECT` (D, D, D, C), `SM_ACTION_ANIMATION`, `SM_EMOTION`'s `START_FLYTELEPORT` arm, and the flight fields of `SM_PLAYER_INFO` (:198-200) in the existing decoder; builders `buildCM_TELEPORT_SELECT`, `buildCM_TELEPORT_ANIMATION_DONE`, `buildCM_BIND_POINT_TELEPORT`, `buildCM_INSTANCE_LEAVE`, `buildCM_MOVE_IN_AIR`, `buildCM_EMOTION(type)`; a `teleportAndArrive()` helper (animation done → `SM_PLAYER_SPAWN` → `CM_LEVEL_READY`); `TravelDecodersTest.cpp` (body consumed exactly, no `serverpackets/` include) | A-04 (dialog decoders), A-02 (inventory decoders) | R | M |

### Stage 1, effects and items (P5-04, P5-03, P5-07)

| Id | What | Need | Eff | A |
|---|---|---|---|---|
| **X-05a** | **`SpellAtkDrainInstantEffect`** (P5-04, 1 site, 40 Java lines) — Haramel's Kakiti (W-21, D15); dropped if M5e's optional T-02 ported it. Test in `tests/effects_mz` on M5b-2's `EffectClassTestSupport.h` (the drain arithmetic, mutation-proven) | **R** | S | A-10 |
| X-05b | **The 11 Eltnen/Morheim classes past A-10** (W-22, D15): P5-03 `DiseaseEffect`, `ConfuseEffect`, `FpAttackEffect`, `FearEffect`, `DispelDebuffMentalEffect`, `CloseAerialEffect`, `DelayedFpAtkInstantEffect`, `DispelBuffEffect`; P5-04 `MpAttackInstantEffect`, `ProtectEffect`, `MagicCounterAtkEffect` — 27 sites + ~5 inner bodies, 574 Java lines; minus whatever M5e's optional T-02 took (`DispelBuffEffect`, `MagicCounterAtkEffect`). Tests in `tests/effects_al`/`effects_mz` | O | L | A-10 |
| X-01 | `MultiReturnAction` (canAct, act, the observer's abort, the task) | O | S | A-03 |
| X-02 | `InstanceTimeClear` (canAct, act, abort, run) | O | S | A-03 |
| X-03 | `ReturnPointEffect` (3 U) + `useTeleportScroll` (T-02) | O | S | A-01 |
| X-04 | `CondSkillLauncherEffect` (2 U) — **only if M5e did not port it** (W-14) | O | S | – |

### Stage 2

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-03** | `TEST(M5fScenario, Run)`: §10's cases, `<bin>/scenario/m5f`, schema pair `aion_{ls,gs}_test_m5f_<hash>`, the shared `RESOURCE_LOCK`, `tests/scenario/m5f_partial_allowlist.txt`, `gs.scenario.m5f` in `ScenarioTests.cmake` | stage 1, G-01, G-02 | R | L |
| **G-04** | `gs.scenario.m5f_geo` (§10.5) and the `tests/geo` rows G2a-G2c | G-03 | R | M |
| **G-05** | **Re-green** `gs.scenario.m5a` (+ `_geo`), `m5b` (+ `_geo`), `m5b2` (+ `_geo`), `m5b3`, `m5c`, `m5d`, M5e's: delete the `InstanceService.cpp:133, 153` rows (§C in m5a/m5b, `m5a_partial_allowlist.txt:38-40`, `m5b_partial_allowlist.txt:68-70`, and whichever later lists copied them); **flip m5c's W-08 expectation** (the flight master now answers `SM_TELEPORT_MAP`) wherever m5c's gate or checklist asserts it; record before/after counts in the wave report | G-03 | R | L |
| G-06 | Fixups the gate names (owning chunks) | G-03 | R | – |
| H-01 | `HaramelInstance` (I1 lease) + `CM_PLAY_MOVIE_END` (P-06); it makes `allowInstanceRevive()` true, so `CM_REVIVE(INSTANCE_REVIVE)` reaches T-06's (now required) `instanceRevive` | stage 1 | O | S |
| H-02 | **user (D5)**: `_1006Ascension`, `_2008Ascension` (Q06 lease) | M5d handler base, A-07 | O | L |
| G-07 | A proposal for the capacity session (not a lane): instance churn — N clients entering and leaving Haramel for 30 minutes under ASan, asserting 0 live `WorldMapInstance` beyond baseline and an empty census | G-03 | O | – |

### Deferred

| Id | What | Milestone |
|---|---|---|
| O-01 | Rifts (P5-12b) | M5i |
| O-02 | Windstreams (`CM_WINDSTREAM`), kisks (`KiskAI`, `InvisiblekiskAI`, `KiskService`, `kiskRevive`) | M5j |
| O-03 | Recall (`RecallService` 4, `CM_RECALLED_BY_OTHER_ANSWER`), group/alliance instance entry verification, `PeriodicInstanceManager`, `InstanceScore`, `PvPArenaService`, `CM_OPEN_STATICDOOR` + `StaticDoorService` (P5-14, 4 U) | M5g / M5i |
| O-04 | `PvpMapHandler`, `PvpMapService` partial, `CustomInstanceService`, `PlayerTransfer*` | M5j |
| O-05 | House teleports, studios (`getOrCreateHouseInstance`) | M5h |
| O-06 | The 73 phase-6 instance handlers (17,155 Java lines) and their 308 AI files (23,466 lines) | phase 6 |
| O-07 | Rev 1's **N-06**: `AutoGroupService::onLeaveInstance` (P5-10), `PeriodicInstanceManager::checkAndSendOpenRegistrations` ×2 and `isInLvlRange` — **together with `scheduleRegistration`**, without which an autogroup-enabled server cannot start (W-08) | whichever milestone ports instance matchmaking (m5g-plan.md O-01 defers it past M5g) |

---

## 6. Lanes

At most six per stage; chunks disjoint within a stage (leases count as the leasing lane's).

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 0 | integrator | manifest | I-01..I-04 | – |
| 1 | **teleport** | **P5-08**, P5-12a (lease), P5-02a (one-line lease, T-07) | T-01..T-08 | `tests/playersvc`, `tests/siege` |
| 1 | **instance-engine** | **P5-13**, P4-10 (lease) | N-01..N-05, N-07 | `tests/instance`, `tests/world` |
| 1 | **packets** | P5-15, P5-16 | P-01..P-07 | `tests/cm_ak`, `tests/cm_lz` |
| 1 | **travel-npcs** | P5-05, A1 (lease) | V-01..V-05 | `tests/handlers_ai_core` |
| 1 | **gate-harness** | P5-SC, `tools/oracle` | G-01, G-02 | `tools.oracle`, decoder self-tests |
| 1 | **effects-items** (R for X-05a only) | P5-04, P5-03, P5-07 | **X-05a**; O: X-05b, X-01..X-04 | `tests/effects_mz`, `tests/effects_al`, `tests/itemsvc` |
| 2 | **gate** | P5-SC | G-03, G-04 (+ `tests/geo` rows under a P4-04 test lease or in the geo owner's lane) | `gs.scenario.m5f`, `_geo` |
| 2 | **regate** | P5-SC (second lease) *or* serialized in the gate lane | G-05 | the earlier gates |
| 2 | fixups | whichever chunks the gate names | G-06 | owning tests |
| 2/3 | *pull-forwards* (O) | I1, Q06 (leases), P5-16 | H-01, H-02 | owning tests |

**Merge order in stage 1.** I-01..I-03 → **N-04 + N-03's `onLeaveInstance` and `getOrRegisterInstance`** (W-01: they unblock every
cross-map teleport, including M5b-2's *Return* off-map) → T-05 → T-01/T-02 (T-02 after N-03's `getOrRegisterInstance`) and **T-06** (W-20)
→ N-07 → N-01/N-02 → T-03/T-04 → P-* (P-02 and P-04/P-05 any time; P-01 after T-01, P-03 after T-04) → V-* (V-02/V-03 after T-03) → N-03's
partial closures **together with I-04's allow-list deletions** (the M5b-2 D7 rule: a closed partial and its row go in one commit). T-07 goes
in one commit with its caller line. X-05a has no dependency. G-01/G-02 have no body dependency and start with the wave.

**Critical path.** `instance-engine` (N-01..N-04 and N-07: ~70 bodies of which 43 are one-line overrides, plus N-05's lifecycle test, the
long pole) and `teleport` (T-01..T-06: ~45 bodies, plus T-08). By the measured pace (§5: ~50 bodies per lane per wave part, ~4 h per part;
M5b-1 ~24 h from plan commit to gate) each is about one wave part plus its tests, and the milestone is of the order of M5b-1 — an
extrapolation, not a budget. `packets`, `travel-npcs` and `effects-items` are S-M and should finish first and help with T-08/N-05. Stage 2's
`gate` is L (three characters on three accounts, ~24 cases), and it needs a P4-04 lease on `tests/geo` for G2a-G2c (P4-04 owns `tests/geo`,
`chunks.py owner`).

---

## 7. Header requests expected

Bodies never need a request (hub-headers.md §14); these are the declaration changes the analysis predicts.

| Request | Kind | For |
|---|---|---|
| **None** for `TeleportService.h`, `PortalService.h`, `BindPointTeleportService.h`, `InstanceService.h`, `GeneralInstanceHandler.h`, `InstanceEngine.h`, `WorldMapInstance.h`, `PlayerReviveService.h`, `SpellAtkDrainInstantEffect.h` — every Java method is declared (measured, §2.7); the 3 anonymous bodies and the `Runnable`s are defined in the `.cpp`; **N-07's no-op handler is file-local to `WorldMapInstance.cpp`** (a header in `instance/handlers` would need a request and gains nothing) | – | T-*, N-*, X-05a |
| `services/RecallService.h`: m5b2-p2-9 (`validateCast` takes `Ptr<VisibleObject>`), **if M5e did not apply it** — with its caller `Skill.cpp:858-860` (P5-02a, I-01's one-line lease) in the same commit | signature (approved already) | T-07 |
| New files: 5 client packets (+ 3 optional), `ResurrectAI` (P5-05), `PortalAI`, `PortalDialogAI` (A1), optionally `HaramelInstance` (I1), the two ascension quests (Q06), `MultiReturnAction.cpp`, `InstanceTimeClear.cpp` | new files (no request); `fwd.h` regeneration where a directory gains a class | P-*, V-*, H-*, X-* |
| `MultiReturnAction`/`InstanceTimeClear` `canAct`/`act` overrides — **covered by M5b-3's h01 batch** if it declared them on every action shell | additive (A-03) | X-01, X-02 |
| **Manifest**: the leases of I-01 (A1, P4-10, P5-12a, the one-line P5-02a; no P5-10 since rev 2); `tests/handlers_ai_world` if I-01 chooses it | build | I-01 |
| `game-server/config/m5f.properties.example` (Java tree) | none | I-03 |

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence.

1. **The first runtime instance lifecycle leaks** (**high**). Nothing has ever created or destroyed a `WorldMapInstance` after startup. One
   Haramel entry creates 117 npc spots with their controllers, AIs, known lists, walker groups and a handler that points back at the instance
   (`cycles.toml:121`); destruction relies on four C++-only breakers (`:33-34, 353-357`) and one TODO (D13) that have never run, and one of
   the breakers installs a class nobody had written (N-07). A missed breaker is **a whole instance per entry**, invisible to every assertion
   except the census and `live_counts`; a no-op handler that retained anything would be the same leak in another shape. Mitigation: N-05's
   deterministic lifecycle test with live-count equality, X13-style rows in the gate (T20), and G-07's churn proposal. **Also**: the checker
   runs on a pool thread and can destroy the instance while a player re-enters (Java races the same way — `// java-race`, not a fix).
2. **Cross-map arrival wakes code per map** (**high**). `CM_LEVEL_READY` runs for every map a player lands on, with M5d's quests (W-13), the
   siege-world arm (W-03), the arrival npcs' AIs (W-12) and `onEnterWorld`'s per-map effect removal; the Gladiator trap (W-14) shows how a
   seed or a level can make enter world itself fail; leaving the world dead inside an instance (W-20) and the monsters' skills on the maps a
   player can now reach (W-21, W-22) were found only by the review of rev 1. (Rev 1's W-08 — autogroup on every map change — is moot: with
   autogroup at its Java default the server cannot start at all.) M5b-1 predicted two prerequisites and found eight. Mitigation: the gate
   lands on five maps plus an instance, and G-01's `--census` re-checks W-14 and W-21/W-22 at branch time.
3. **M5f stands on four milestones nobody has built** (**high**). Kinah (A-02, M5b-3), dialogs and the question window (A-04, M5c),
   `ActionItemNpcAI` and the quest registry (A-06, M5d), and the real client's Daeva path (A-07, M5e). If A-02 or A-04 slips, the paid and
   npc-driven half of the gate cannot run. **Fallback** (§9): without A-02 every paid case goes (teleporters, flights, hotspot, obelisk) but
   *Return* across maps (a seeded bind point), the teleport statue and Haramel (both free) remain; without A-04 every npc case goes, and what
   remains is the hotspot (A-02 only), *Return* with seeded bind points and a login into Haramel's map with no registered instance (a seeded
   `world_id = 300200000`, which reaches `moveToExitPoint` without any dialog) — each fallback is about a third of §10.
4. **The login inside a destroyed instance teleports during enter world** (medium). `moveToExitPoint` runs before the spawn (§2.5 step 10):
   `abortPlayerActions` and `World.despawn` on a player that was never spawned, and a `SpawnTask` whose `SM_PLAYER_SPAWN` precedes the enter
   world's own. Java does exactly this; C++ may trip an assertion Java never had (a `MapRegion` that is null, a known list not initialised).
   §13 item 2; case C21 is where it shows.
5. **The animation handshake** (medium). An animated teleport arrives only on `CM_TELEPORT_ANIMATION_DONE`; a client that disconnects between
   `SM_TELEPORT_LOC` and that packet leaves a despawned player whose `TELEPORT` task pins it (`Future::deferred`'s `Pin`, `TeleportService.cpp:185-186`)
   until `cancelAllTasks` at logout. The logout of a despawned player has never run. A unit case in P-07 and a gate case (quit during the
   animation) are cheap; the gate includes it as C12b.
6. **Time-dependent assertions** (medium). Haramel's reuse time is the next 09:00 in *server* time (InstanceCooltimeData.java:79-87); a run that
   crosses 09:00 changes it; the hotspot cooldown (600 s) is per process and per character. The oracle computes the expected value from the
   gate's clock with a crossing check, and each character uses the hotspot once.
7. **Float exactness of prices and positions** (medium). The hotspot price uses a float distance (§2.3), telelocs are floats written as
   decimals (`1640.76` → the nearest float), and the gate asserts arrival positions to 1 cm and prices exactly. The oracle uses
   `javafloat.py`; the decoders compare floats bit for bit only where Java copies them unchanged.
8. **Group-instance arms are ported blind** (medium, D3). `PortalService`'s team arms cannot run before M5g; a wrong arm shows up only there.
9. **Geo arrival z** (low for the port, medium for the user). 4.8 corrects no arrival z except `teleportToNpc`'s (TeleportService.java:327-329);
   a teleloc whose z lies under the geo terrain drops the real client through the world. G-01's `--geo-check` lists them; the port cannot fix
   data.
10. **The flypath validator quirk** (low, D7). If the user enables `gameserver.security.validation.flypath`, every flight is refused with an
    audit line. The checklist says so.
11. **Combat past the effect boundary** (medium for the user, D15). Without X-05b a monster of Eltnen or Morheim that casts one of W-22's
    classes throws at the first unported body of the cast (m5b2-plan.md D6's intent); what the real client then sees is inferred — a skill
    that does nothing, a monster that stops acting — and `unported_trace.txt` fills. The gate
    cannot see it (it does not fight there); the checklist keeps the user out of those fights unless X-05b landed, and says so.

---

## 9. The split: two stages (and an optional third)

| Stage | Content | Why here |
|---|---|---|
| 0 | leases, header requests, the profile | – |
| **1** | the six lanes (five R, `effects-items` R for X-05a only): ~101 bodies + N-07's 43 one-line overrides | chunks are disjoint and the dependency chain is short (W-01 first) |
| **2** | the gate, the geo gate, the regate, fixups; H-01 if taken | the gate needs every stage-1 lane; the regate needs the gate's findings |
| 3 (O) | H-02 (ascension, **user**, D5) | only on the user's answer |

**Fallback if A-02 or A-04 is late** (risk 3): stage 1 lands unchanged (its unit tests need neither kinah packets nor dialogs — kinah changes
are asserted on `Storage` directly, dialogs are driven through `NpcController::onDialogSelect` in-process), and G-03 is written in two parts.
**A-02 late**: part 1 = C0-C1, C9 and C14 with a *seeded* `player_bind_point` row on another map (instead of C7), a teleport-statue case
(730531, free: its paths lead to Poeta itself, so `PortalService.port` takes the same-map arm and moves the player at once with `NONE`,
no animation — PortalService.java:118-120 — through T-02's `int instanceId` overload, `TeleportService.cpp:255`) and C15-C21 (Haramel is free); part 2 = C2-C8, C10-C13, C22. **A-04 late**: part 1 = C0-C3 (the hotspot needs no dialog),
C9/C14 with seeded binds, and a login with a seeded `world_id = 300200000` (no registered instance → `moveToExitPoint` → the Verteron exit,
T18's second half); part 2 = everything with an npc.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5f`)

### 10.1 Processes, databases and profile

As m5c-plan.md §10.1, except:

| Piece | M5f |
|---|---|
| Schemas / output | `aion_{ls,gs}_test_m5f_<hash>`, `<bin>/scenario/m5f`, the shared `RESOURCE_LOCK` |
| Profile | M5c's set (siege, autogroup, rift, vortex, worldraid, cp disabled; limits off; `missing_ai_handlers = warn`; events and shouts off) **plus** `gameserver.instance.solo.destroy_delay_seconds = 1` (D11), `gameserver.security.validation.flypath = false` (explicit), `gameserver.simple.secondclass.enable = false`, `gameserver.geodata.enable = false`. Written out as `game-server/config/m5f.properties.example` (I-03) |
| Allow-list | `tests/scenario/m5f_partial_allowlist.txt`, derived at branch time from the latest earlier gate's list (m5c's rule): §A the startup rows — **`QuestEngine.cpp:111`** ("XML quests are not registered") **unless M5d closed it (A-06)** and `BaseService.cpp:18` (`m5b_partial_allowlist.txt:32, 34`) — and whatever M5b-3..M5e leave; §C `QuestEngine.cpp:115` (quest analysis, off), **`PvpMapService.cpp:32`** (D6), `PlayerService.cpp:268` (`m5b_partial_allowlist.txt:65-72`) as still present; **no `InstanceService` row** (closed) |
| Characters | **E1**: fresh Elyos Warrior, account A, level 1, **unseeded**. **E2**: Elyos Templar Daeva, account B, level 16, kinah 20,000, position seeded twice (Melponeh landing point; later next to Haramel's entrance). **S1**: Asmodian Templar Daeva, account C, level 16, kinah 20,000, position seeded next to Osmar. All seeds as m5c D5 (A-05) |
| Targets | every npc, loc, price and point **from G-01** |

### 10.2 Cases

**Run order** (rev 2): C0-C8, **C11, C12**, C9, C10, C12b, C13-C23 — so that E1, back at Akarios after C8 and standing next to Daines,
is the observer of E2's departure in C12 (T10). Case numbers are kept so §9's fallback and §10.3 read unchanged.

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `m5f-travel --npc 203070/203083/203194/203091/203679/203581`, `--hotspot 13 --from <E1 spawn>`, `--obelisk 700014`, `--portal 730318 --race ELYOS`, `--instance-spawns 300200000 …`, `--exp-for-level 16`, `--census` |
| C1 | setup | create E1, E2, S1 (M5a cases replayed); seed E2, S1 offline; E2 enters world at Melponeh (the flight observer); E1 enters at the Elyos spawn |
| **C2** | hotspot | E1: `CM_BIND_POINT_TELEPORT(1, 13, oraclePrice)`, wait 12 s |
| C3 | hotspot cooldown | E1: `CM_BIND_POINT_TELEPORT(1, 15, …)` |
| C4 | the Daeva gate of the dialog | E1: `CM_SHOW_DIALOG(Daines)`, `CM_DIALOG_SELECT(44)` |
| C5 | a crafted select E1 cannot pay | E1: `CM_TELEPORT_SELECT(Daines, loc 4)` with 937 kinah (D7) |
| **C6** | flight | E1: Kustanon → `CM_DIALOG_SELECT(44)` → `CM_TELEPORT_SELECT(13)`; `CM_MOVE_IN_AIR` × 20 along the straight line to flypath 5's end with rising `distance`; `CM_EMOTION(LAND_FLYTELEPORT)`; **one more `CM_MOVE_IN_AIR` to a point beyond the 95-m visibility range of both E1's landing point and E2** (flypath 5's Akarios start, 806.63/1242.11/119, ~630 m away, `flypath_template.xml:7`); wait 2 s; then a `CM_MOVE` of 2 m at the landing point |
| **C7** | bind | E1: `CM_SHOW_DIALOG(700014)` → `CM_QUESTION_RESPONSE(yes)`; then the same again |
| C8 | flight back | E1: Aero → loc 12 (as C6, without the observer rows) |
| **C9** | *Return* on the map | E1 at Akarios: `CM_CASTSPELL(243, …, self)`, wait 8 s |
| C10 | bind round trip | E1: `CM_QUIT(0)`, read `player_bind_point`, relog |
| C11 | E2 to Daines | E2: Aero → Akarios (a second flight, `6001`); runs right after C8, while E1 stands at Akarios |
| **C12** | **map to map** | E2: Daines → `CM_TELEPORT_SELECT(4)` → (no packet) → `CM_TELEPORT_ANIMATION_DONE` → `CM_LEVEL_READY`; **E1 stands within 95 m of Daines** (the observer) |
| C12b | quit during the animation | E2: repeat to Sanctum from Verteron (C13's npc) but `CM_QUIT(0)` right after `SM_TELEPORT_LOC`; relog → where Java puts it (the old map; the kinah is already paid) |
| **C13** | a capital | E2: Urakron → loc 2 Sanctum (Urakron's loc 2 has no `required_quest`; Daines's and Polyidus's have 1006 — T-08 covers that check) |
| **C14** | *Return* across maps | E2 in Sanctum, no bind point: `CM_CASTSPELL(243)` |
| C15 | reposition | E2: quit; seed the position next to 730318; relog |
| **C16** | **enter Haramel** | E2: `CM_SHOW_DIALOG(730318)`, wait for the use bar, `CM_TELEPORT_ANIMATION_DONE`, `CM_LEVEL_READY` |
| C17 | leave button | E2: `CM_INSTANCE_LEAVE` |
| **C18** | exit portal | E2: move 9 m, `CM_SHOW_DIALOG(730320)`, … |
| **C19** | re-enter | E2: `CM_SHOW_DIALOG(730318)` again |
| C20 | relog inside, instance alive | E2: `CM_QUIT(0)`, relog at once. **C16-C20 must finish within ~50 s of the instance's creation**: the checker's first run is 60 s after it (InstanceService.java:58) and, with D11's 1-s delay, destroys the instance if E2 is outside at that moment; the gate reads the creation time from the `Created new instance` log line and fails loudly (not flakily) if it overruns |
| **C21** | relog after destruction | E2: `CM_QUIT(0)`; wait for `Destroying` in `gs_log` (≤ 75 s); relog |
| **C22** | two consecutive map changes | S1: Osmar → loc 9 Altgard; Ukin → loc 10 Morheim |
| C23 | reports and shutdown | the Q8 bar + M5f rows; the stop file with characters online |

### 10.3 Assertions

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **T1** | C2 | `SM_BIND_POINT_TELEPORT(1, E1, 13)` at once; **10 ± 1 s later** `SM_BIND_POINT_TELEPORT(3, E1, 13, 600)` and kinah **−63** (oracle); **1 ± 0.5 s later** `SM_CHANNEL_INFO(1, 1)`, `SM_PLAYER_INFO` at (807, 1242, 119) to 1 cm, `SM_STATS_INFO`, `SM_MOTION` — **no `SM_PLAYER_SPAWN`**; no WARN "prices don't match" | **Proves** the two tasks and their order, the float-distance price, the same-map arm of `SpawnTask`. **Cannot** prove the client cast bar | charging at t = 0 (kinah falls early); a move without the 1-s task; `SM_PLAYER_SPAWN` on a same-map move (the cross-map arm) |
| T2 | C3 | `STR_FLYING_TIME_NOT_READY`, no `SM_BIND_POINT_TELEPORT`, kinah unchanged | the cooldown check. **Cannot** prove its 600-s length (T-08's unit test) | a cooldown not stored |
| T3 | C4 | `SM_DIALOG_WINDOW(Daines, NO_RIGHT)`, **no** `SM_TELEPORT_MAP` | M5c's `isDaeva` arm, still live after `showMap` is ported | `showMap` called before the Daeva check |
| T4 | C5 | `STR_MSG_NOT_ENOUGH_KINA(1130)`, no `SM_TELEPORT_LOC`, kinah and position unchanged | `checkKinahForTransportation` (price 800 → 1130), that `teleport` stops before `sendLoc`, and D7's quirk (no Daeva check in the packet path) is faithful | a price without taxes (1000); a teleport that despawns before charging |
| **T5** | C6 | `SM_TELEPORT_MAP(Kustanon, 103)`; after the select: broadcast **`SM_EMOTION(E1, 6, 5001)`** and kinah **−226**, **no `SM_TELEPORT_LOC`**; **E2 receives `SM_PLAYER_INFO(E1)` exactly once during the flight, with flight id 5001 and the `distance` of the `CM_MOVE_IN_AIR` that first put E1 within 95 m of E2** (the gate computes which step that is from the positions it sent; §2.2 step 5); E1's known list follows the flight — Melponeh npcs announced, Akarios npcs deleted (read: World.java:167-169, 234-235, §2.2 step 4); **after `LAND_FLYTELEPORT`, the extra `CM_MOVE_IN_AIR` to Akarios changes nothing: within 2 s E1 receives no `SM_DELETE` and no npc announce, and E2 receives no `SM_DELETE(E1)`**; the `CM_MOVE` then reaches E2 as `SM_MOVE(E1)` at the landing point (CM_MOVE.java:149; E1 is still in E2's known list) | **Proves** the FLIGHT arm, `setFlightTeleportId`, `CM_MOVE_IN_AIR` and its `FLYING` gate (a mutant that moves E1 ~630 m makes both lists churn), `onFlyTeleportEnd`. **Cannot** prove the path shape or the 38-s duration (client side; validator off) | the template's `teleportId` (103) instead of the location's `teleportid`; `CM_MOVE_IN_AIR` without the `FLYING` check (E2 gets `SM_DELETE(E1)`, E1 gets Melponeh's deletions); `onFlyTeleportEnd` not unsetting `FLYING` (same rows) |
| **T6** | C7 | `SM_QUESTION_WINDOW(STR_ASK_REGISTER_RESURRECT_POINT, 143)`; on yes: `SM_BIND_POINT_INFO(0, 1, 210010000, E1's x, y, z, 0)`, kinah **−143**, `SM_ACTION_ANIMATION(E1, BIND_KISK)`, `STR_DEATH_REGISTER_RESURRECT_POINT`; **`player_bind_point` holds E1's position** (D12); the second attempt: `STR_ALREADY_REGISTER_THIS_RESURRECT_POINT`, no question, no charge | `ResurrectAI`, the `AIRequest`, `PlayerBindPointDAO.store` (insert). **Cannot** prove the race/tribe refusals (V-05) | the obelisk's position stored instead of the player's; the 20-m check missing |
| T7 | C8, C11 | `SM_EMOTION(…, 6, 6001)`, kinah −226 | the second flight master and a second character | – |
| **T8** | C9 | `SM_CASTSPELL(243)` with a 6,000 ± 10 % ms bar; after it `SM_CHANNEL_INFO` + `SM_PLAYER_INFO` at **the bind point of T6** to 1 cm, no `SM_PLAYER_SPAWN` | `ReturnEffect` → `moveToBindLocation` → the bind branch. **Cannot** prove the no-bind branch (T12) | the race spawn used although a bind exists |
| T9 | C10 | the enter-world `SM_BIND_POINT_INFO` equals T6's; one `player_bind_point` row; **and, since C10 relogs within 600 s of C2's charge, the enter world also sends `SM_BIND_POINT_TELEPORT(3, E1, 13, t)` with `t` = 600 − the whole seconds since the charge, ± 2 s** (`BindPointTeleportService.onLogin`, BindPointTeleportService.java:32-37 via PlayerEnterWorldService.java:283; ported at `BindPointTeleportService.cpp:28-33`, `PlayerEnterWorldService.cpp:513`, never run by a gate). The gate measures the elapsed time; if it exceeds 600 s it asserts the packet's absence instead and says so | the DAO round trip; the in-memory hotspot cooldown surviving a relog (D7: not a restart) | a bind not persisted (`NEW` state lost); a cooldown not reported at login |
| **T10** | C12 | `SM_TELEPORT_MAP(Daines, 2)`; after the select: kinah **−1130**; **E2 receives no `SM_DELETE`** from the select to its `SM_PLAYER_SPAWN` (a teleporting player gets no deletion packets: World.java:318 before `:325`, PlayerController.java:134-135; §2.1 step 6); **E1, the observer at Akarios, receives `SM_DELETE(E2, 11)`** (`JUMP_IN` → `ObjectDeleteAnimation.JUMP_IN`, TeleportAnimation.java:58-67, ObjectDeleteAnimation.java:38; `SM_DELETE` = D objectId, C animation, SM_DELETE.java:43-46); `SM_TELEPORT_LOC(3 = JUMP_IN, 210030000, 210030000, 1640.76, 1500.32, 119.71, 0)`; **nothing else until E2 sends `CM_TELEPORT_ANIMATION_DONE`**; then `SM_CHANNEL_INFO(1, 1)` + `SM_PLAYER_SPAWN(210030000, …)` at the teleloc to 1 cm; after `CM_LEVEL_READY` the npc set within 90 m equals the oracle's (m5a V1-V4 rules); after a quit `players.world_id = 210030000` | **Proves** the whole cross-map path incl. **W-01** (`onLeaveInstance`, formerly U), the despawn seen from outside, and the deferred `SpawnTask`. **Cannot** prove the loading screen | a `SpawnTask` run at once (the spawn precedes the animation-done packet); **no despawn** — E1 gets no `SM_DELETE(E2)`, and `SpawnTask.run` returns at once for a player that is still spawned (TeleportService.java:502), so no `SM_PLAYER_SPAWN` either; **the not-spawned guard dropped** (E2 receives a deletion burst Java never sends); the wrong delete animation; `onLeaveInstance` left U (ERROR + `unported_trace`) |
| T10b | C12b | after the relog E2 is where Java's `SpawnTask` never ran (the pre-teleport map and position), kinah −706 kept; `live_counts` shows no `SpawnTask` left | the pinned task is released by `cancelAllTasks` at logout (risk 5). **Cannot** prove the reconnect of a real client | a leaked `SpawnTask` pinning a `Player` |
| T11 | C13 | as T10 for 110010000 (kinah −706) | the first automated capital arrival by teleport (W-16) | a teleloc read from the wrong map |
| **T12** | C14 | after the cast: **no `SM_TELEPORT_LOC`**; `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN(210010000)` at the Elyos spawn (`player_initial_data.xml:4`) | the no-bind branch and a cross-map teleport **without** animation (the `NONE` arm of `sendLoc`) | an animation sent for `NONE` |
| **T13** | C16 | `SM_USE_OBJECT(E2, 730318, 3000, 1)` + `SM_EMOTION(START_QUESTLOOT)`; **3 ± 0.5 s later** `SM_EMOTION(END_QUESTLOOT)` + `SM_USE_OBJECT(…, 3000, 2)`; `SM_TELEPORT_LOC(1 = FADE_OUT_BEAM, 300200000, id ≥ 2, 172, 20, 144.22548, 60)`; **`SM_INSTANCE_INFO(2, …)` with cooltime id 46, max 16, offset −1, and the reuse field = the seconds remaining until the oracle's reuse time, ± 2 s** (the packet writes `(int) (reuseTime − now) / 1000`, a remaining time, not an absolute one: SM_INSTANCE_INFO.java:47); a `portal_cooldowns` row; `gs_log` "Created new instance: 300200000 [id]"; after the handshake `SM_PLAYER_SPAWN(300200000)`, `STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF`, and after `CM_LEVEL_READY` `SM_INSTANCE_COUNT_INFO(300200000, id, 1)` and the instance's npc set within 90 m = the oracle's `spawnInstance` set | **Proves** `PortalAI` + the use bar, `PortalService.port` (solo arm), `getNextAvailableInstance` + `spawnInstance`, `transfer`, the cooldown and its first DB write. **Cannot** prove the group arms (D3) | an instance id of 1 (the inaccessible default); no spawns; the cooldown on re-entry (T16) |
| T14 | C17 | nothing within 2 s | the no-op of D7. **Cannot** prove the 13 overriding handlers | an invented exit on `CM_INSTANCE_LEAVE` |
| **T15** | C18 | the use bar; `SM_TELEPORT_LOC(1, 210030000, 210030000, 2533.8564, 835.055, 103.967476, 59)`; after the handshake **`STR_MSG_LEAVE_INSTANCE(0)`** (solo arm, D11) + `SM_PLAYER_SPAWN(210030000)` | `onLeaveInstance`'s solo message and `GeneralInstanceHandler.onLeaveInstance` on the way out. **Cannot** prove `removeInstanceItems` removes anything (no restricted item; N-05) | a party message for a solo instance; the delay in seconds instead of minutes |
| **T16** | C19 | `SM_TELEPORT_LOC` with **the same instance id** as T13; **no `SM_INSTANCE_INFO`**; `portal_cooldowns` entry count still 1 | the `reenter` branch (PortalService.java:101-110, 363-366) | a second instance per entry; a cooldown charged on re-entry |
| T17 | C20 | the enter world lands E2 **in the same instance id**, `SM_INSTANCE_COUNT_INFO` with it | `onPlayerLogin`'s registered arm with a real registration | the default instance 1 |
| **T18** | C21 | `Destroying … 300200000` in `gs_log` 60-75 s after the instance's creation; the relog's burst holds **two** `SM_PLAYER_SPAWN`, the first at the Haramel exit (`instance_exit.xml:16`), and E2 ends in Verteron there; the enter-world `SM_INSTANCE_INFO` still shows Haramel's entry at −1 | the checker, `destroyInstance`, **`moveToExitPoint` → `moveToInstanceExit` (the closed partial `:153`)** and D7's double spawn. **Cannot** prove the forced leave of a player inside at destruction (N-05) | a checker that never fires (no log line); a login that leaves E2 in the dead default instance |
| T19 | C22 | two cross-map arrivals (Altgard, Morheim) to 1 cm, kinah −1130 and −2401 | consecutive map changes; the Asmodian tables | state left from the first change (a stale `MapRegion`) |
| **T20** | C23 | the Q8 bar: `unported_trace.txt` **empty**, census empty, lockdep empty, no ERROR in either log, `partial_trace.txt` ⊆ the allow-list with §B hit 0; **`live_counts.txt`: `WorldMapInstance` and `GeneralInstanceHandler` live == the baseline, `EmptyInstanceCheckerTask` and `TeleportService::SpawnTask` live 0 with `created > 0`** | the instance and its handler were reclaimed (risk 1). **Cannot** prove the 117 npcs were reclaimed by count alone (temporary spawns move `Npc` counts) — the census is what covers them | a missed breaker; a checker task not cancelled by `destroyInstance` |

### 10.4 Mutation proof (the minimum set)

Every assertion is watched failing with the quoted outputs; the rows that record what the gate cannot catch are required too.

| Mutation | Must fail | Must stay green |
|---|---|---|
| `TeleportService::sendLoc`: run the `SpawnTask` at once for animated teleports | **T10** | T1, T12 |
| `TeleportService::sendLoc`: skip `World.despawn` | **T10** twice: E1 gets no `SM_DELETE(E2, 11)`, and `SpawnTask::run` returns at once for a spawned player (TeleportService.java:502), so no `SM_PLAYER_SPAWN` | – |
| `PlayerController::notSee`: drop the not-spawned guard (`PlayerController.cpp:228-229`) | **T10** (E2 receives `SM_DELETE`s) | T5 |
| `TeleportAnimation`'s delete animation: `FADE_OUT` for `JUMP_IN` | **T10** (E1's `SM_DELETE` animation 1, not 11) | T1 |
| `SpawnTask::run`: skip `InstanceService::onLeaveInstance` | **T15** (no leave message) | T1, T8 |
| `BindPointTeleportService::teleport`: charge before the 10-s task | **T1** | T2 |
| `TeleportService::teleport`: `SM_EMOTION` with the template's `teleportId` | **T5** | T10 |
| `CM_MOVE_IN_AIR`: drop the `FLYING` check | **T5** (the post-landing move goes ~630 m: E2 gets `SM_DELETE(E1)`, E1 gets Melponeh's deletions) | T10 |
| `CM_MOVE_IN_AIR`: skip the known-list update (`updatePosition(…, false)`) | **T5** (E2 never receives `SM_PLAYER_INFO(E1)` during the flight) | T10 |
| `ResurrectAI` accept: store the obelisk's position | **T6**, T8 | T1 |
| `moveToBindLocation`: ignore the bind point | **T8** | T12 |
| `PortalService::transfer`: add the cooldown on re-entry | **T16** | T13 |
| `getNextAvailableInstance`: skip `spawnInstance` | **T13** | T15 |
| `destroyInstance`: skip `detachInstanceHandler` | **T20** (live `GeneralInstanceHandler`) and N-05 | T18 |
| N-07's no-op handler: retain the instance, or return null from `getInstanceHandler()` after destroy | **nothing in the gate** beyond T20's live counts — **N-05** (non-null, retains nothing) | all |
| `PlayerReviveService::instanceRevive`: skip the `startPos` move | **nothing in the gate** (no death inside Haramel) — **T-08/N-05** | all |
| `InstanceService::moveToExitPoint`: leave the partial | **T18** | T13-T17 |
| `PricesService` taxes dropped (a P5-09 file, mutated only for the proof) | **T4**, T5, T10 | T1 (hotspots use no `PricesService`) |
| `BindPointTeleportService`: distance in double instead of float | **nothing in the gate** (63 either way) — **T-08's unit vector must catch it** | all |
| `BindPointTeleportService::onLogin`: not called at enter world | **T9** (no `SM_BIND_POINT_TELEPORT(3, …)` at the relog) | T1, T2 |
| `GeneralInstanceHandler::removeInstanceItems`: remove nothing | **nothing in the gate** (no restricted item) — **N-05** | all |
| `EmptyInstanceCheckerTask`: ignore `isRegisteredTeamDisbanded` | **nothing in the gate** (no teams, D3) — **N-05** (or M5g) | all |
| `Player::setPosition`: drop `resetLastPositionFromClient` | **nothing in either gate** — **G2c** in `tests/geo` | all |

### 10.5 The geo gate (`gs.scenario.m5f_geo`)

`gameserver.geodata.enable = true`, `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`, the same `RESOURCE_LOCK`. **4.8 corrects no arrival z
except `teleportToNpc`'s** (TeleportService.java:327-329), so — as m5a-plan.md's geo row (`:227`) did for spawns — the geo gate's value is the
arrival path in a geo-built world plus the rows a geo-off run cannot make:

| # | Assertion | Proves / cannot prove |
|---|---|---|
| **G1** | every arrival of T1, T5 (the landing is client-given), T8, T10-T13, T15, T18, T19 equals the data xyz **to 1 cm** in the geo run | kills "a `GeoService.getZ` snap added to `sendLoc`/`SpawnTask`", which the geo-off gate passes (`getZ` answers NaN without geo) |
| **G2** | S1 stands on **Morheim (220020000), the only field map with a terrain-material image** (`data/geo/220020000_materials.png`; m5a-plan.md:227 names it as the missing player-side material path): the Q8 bar stays clean and `created(TerrainZoneCollisionMaterialActor)` − baseline ≥ 1 | the player-side material path runs under a teleport arrival. **Cannot** prove *which* creature created the actor (Morheim's 777 temporary-spawn rows add actors at game-hour changes) — hence the `tests/geo` rows: |
| G2a | `tests/geo` (real data): a `Player` spawned at loc 10's point on 220020000 carries one `TerrainZoneCollisionMaterialActor` | the attach, deterministically |
| G2b | `tests/geo`: `teleportToNpc`'s z equals the geo oracle's `probes.getZ` bit for bit at three npcs (and `spot.z + 0.5` with geo off) | the one geo-corrected teleport |
| G2c | `tests/geo`: after `Player::setPosition` to a far point, a new `AbstractCollisionObserver` takes `oldPos` = the new position (AbstractCollisionObserver.java:33-37) | the reset that keeps shields and material zones from firing on a teleport |
| **G3** | `tools/oracle` test: every destination of §2.9 has `|data z − geo z| ≤ 1.0` per the geo oracle, and the list of those that do not is printed | a data check for the real client (risk 9); **proves nothing about the port** |

### 10.6 Re-greening the earlier gates

G-05. What moves: (a) the `InstanceService.cpp:133, 153` rows disappear from every allow-list in the commit that closes them; (b) m5c's
flight-master expectation (m5c W-08) flips from "loud" to `SM_TELEPORT_MAP`; (c) M5b-1's bind revive and M5b-2's *Return* reach
`onLeaveInstance` only across maps (`currentWorldId != worldId`, TeleportService.java:514-519) and their gates stay on one map, so those
packet sequences should not change — **record** that they did not; (d) M5c's C19 (Sanctum crafting) and M5d's Verteron/Altgard quests keep
their seeded positions — they are not rewritten to travel here.

---

## 11. Real-client checklist (user, after stage 2)

Prerequisites as m5b-plan.md §10 steps 1-6, with `mygs.properties` from `m5f.properties.example` **but the user's usual geo setting (on)**,
**`gameserver.simple.secondclass.enable = true`** (A-07) and **`gameserver.instance.solo.destroy_delay_seconds = 600`** — the default
(instance.properties:22); the example's value of 1 is for the gate only (D11), and steps 14-15 are written for 600. Each step names the npc
and what the screen should show. **Where to fight** (D15): Poeta, Ishalgen, Sanctum, Pandaemonium and Haramel are safe (every monster skill
ported once X-05a lands); Verteron and Altgard only if M5e delivered its monster classes (A-10); **Eltnen and Morheim only if X-05b landed** —
otherwise arrive, look around the arrival town, and leave.

**A new Elyos character (level 1):**

1. Open the map and click the **Akarios Village** hotspot: a 10-second cast bar, then you stand in Akarios; about 63 kinah are gone. Try a
   second hotspot at once: "not ready yet".
2. Talk to **Daines** (the teleporter): the dialog says you have no right — correct for a non-Daeva.
3. Talk to **Kustanon** (flight master, next to Daines) → flight → *Melponeh's Campsite*: the take-off animation, the 38-second flight, the
   landing; ~226 kinah. Npcs of Melponeh appear on the way. **Do not** enable `gameserver.security.validation.flypath` (D7).
4. Click the **obelisk** at Melponeh: a question with the price (143); accept: the bind animation, the map shows the new bind point. Click it
   again: "already registered".
5. Take **Aero** back to Akarios. Cast ***Return***: 6 seconds, then you are at Melponeh.
6. Die to a monster and revive at the bind point: Melponeh (M5b-1's revive, now at your own bind).
7. Walk to the **teleport statue** in Daminu Forest (or use the Daminu hotspot after 10 minutes): its dialog offers Akarios and Melponeh;
   pick one: **an instant move, no animation** — both destinations are on Poeta itself, so the portal takes its same-map arm and teleports
   with `NONE` (PortalService.java:118-120).
8. Log out and in: the bind point is still shown on the map.

**Becoming a Daeva** (M5e): level to 9, relog, pick a class in the dialog that opens (or, offline, run the SQL of m5c §11 with a **Templar**
and level ≥ 16 — **not a Gladiator at 15+**, W-14).

9. Talk to **Daines** → map → **Verteron**: the jump animation, a loading screen, Verteron Citadel; ~1,130 kinah.
10. **Urakron** → **Sanctum** (706); walk around (the first real Sanctum visit); **Polyidus** → back to **Poeta** (141).
11. From Verteron with no bind point, cast ***Return***: you land at the Poeta start point (a loading screen).
12. Bind at the **Verteron obelisk** (480), fly with **Mirdiena** to Pilgrims Respite, cast *Return*.
13. At level ≥ 16 walk to the **Haramel secret entrance** (south-east Verteron, near 2539, 835): a 3-second bar, a beam, "the instance has
    been opened for you"; the instance list shows 1 of 16 entries used. Fight inside: **Drudgelord Kakiti** (about 60 m in) drains your HP
    with *Borrowed Life* (X-05a). **The boss Hamerun does nothing** (D14); do not expect a chest. **Then let a monster kill you inside and
    log out while dead, without reviving** (W-20); log in again at once: you are alive at Haramel's entry point (the instance's start
    position) — not dead, frozen or duplicated, and the server log shows no error for the logout.
14. Walk back to the **Haramel exit** right behind you: Verteron, "the instance will be destroyed in 10 minutes". Enter again within
    10 minutes: the same instance, the entry count unchanged.
15. Log out inside Haramel, **wait 12 minutes** (the instance dies at the first once-a-minute check that comes 10 minutes after you left,
    InstanceService.java:58, 177-191), log in: you are at the Haramel exit in Verteron.
16. Asmodian: **Osmar** → Altgard; **Ukin** → **Morheim** (the first field map with terrain materials): look around the arrival town; **do
    not leave it to fight unless X-05b landed** (D15, W-22 — Morheim's monsters cast 10 classes no earlier milestone ports).

**Expect and note, not a regression:** the abyss gates (quests 1020/1044), the Inggison/Gelkmaros aerolinks (siege worlds — they should load,
W-03), rifts (none before Eltnen/Morheim; M5i), windstreams (none before Inggison), kisks (M5j), group instances ("party only"), the
ascension quest itself (phase 6, D5), guards and mailboxes whose AI is not ported (a click does nothing), and — outside the fighting
boundary above — a monster skill that does nothing and a line in `unported_trace.txt` (note the npc and the skill). Send `game-server/log/`,
`live_counts.txt`, `partial_trace.txt`, `unported_trace.txt` and the client's unknown-packet lines (lesson 4: which client packets the session
sent that have no C++ file).

---

## 12. What was measured and what was inferred

**Measured** (a re-runnable grep, parse or script over the two trees; throw-away scripts in the session scratchpad):

- Every `AION_UNPORTED`/`AION_PARTIAL` count of §2.6 and its per-file split; P5-08 = 166 and P5-13 = 118 agree with the roadmap; the P5-08
  teleport package 38 (21 + 13 + 4), `InstanceService` 12 + 2 P, `GeneralInstanceHandler` 11, `InstanceEngine` 1, `WorldMapInstance` 1,
  `PlayerReviveService` 9 (4 on the path), `SiegeService` 2 on the path.
- The port status of every callee named in §2.1-§2.5 (a definition index of all C++ `.cpp` files with each body's status).
- Lesson 1: 0 undeclared named methods over the 31 files, the 3 anonymous bodies; rev 2: 15 classes with no file on the path (66 bodies,
  `javasrc.parse_file` per class, anonymous members counted, lambdas not) + the 2 shells (8); the no-op `InstanceHandler` absent from the
  whole tree (grep for implementors of `InstanceHandler`) against `InstanceHandler.h`'s 43 pure virtuals.
- 188 Java `CM_*` against 42 C++ → **146** without a file; the opcodes of the five R packets in `ClientPacketInfo.gen.inc`.
- The 20 server packets and 12 data holders at 0 `AION_UNPORTED`; `PlayerBindPointDAO`, `PortalCooldownsDAO`, `PortalCooldownList` at 0.
- All data of §2.9: npc templates, spots, teleporter templates (249), telelocations (284), flypaths (208), hotspots (73), bind points (129),
  portal templates (255 use / 182 dialog / 29 scroll — rev 1's 185/31 counted commented-out entries; rev 2 re-counted on parsed XML, where
  comments are not elements), portal locs (405), instance cooltimes (110; 32 solo), exits (137), world maps (110
  instances); the Haramel spawn file (117 spots, 42 ids, the AI kinds of W-11) and the distances in it; the AI kinds within 90 m of each
  arrival (W-12); the rift and windstream maps; Morheim's material image and its 777 temporary-spawn rows.
- The price table (siege-off influence as m5c §2.10) and the hotspot-13 price (63) with Java float arithmetic; the level-16 exp (844,378).
- **W-14**: per class, the autolearn passives ≤ 16 and their effect classes against the C++ tree (ported `.cpp`, or a generated data-only
  header): only `GLADIATOR` fails (skill 563, `CondSkillLauncherEffect`, 2 U), for both races; the Templar's 11 passives pass.
- The flypath quirk (loc ids and flypath ids are different spaces) and the validator's default `false`.
- W-08: `gameserver.autogroup.enable` defaults to `true` (AutoGroupConfig.java:12, autogroup.properties:7) and `InstanceService.onLeaveInstance`
  calls `AutoGroupService.onLeaveInstance` under it (InstanceService.java:222-223), which calls `PeriodicInstanceManager.checkAndSendOpenRegistrations`
  (AutoGroupService.java:264); all three C++ bodies are `AION_UNPORTED`. **And** (rev 2, read) with it enabled the startup step at
  `GameServer.cpp:243` throws from the `PeriodicInstanceManager` constructor (`PeriodicInstanceManager.cpp:25-55`), uncaught by `runStep`
  (`GameServer.cpp:139-141, 311-315`) — so the map-change path is unreachable today.
- W-20: `PlayerLeaveWorldService.cpp:118-120` → `PlayerReviveService.cpp:102/106` (U); the steps it skips (`:121-154`); its callees' status.
- W-21/W-22 (rev 2): per map, the npc ids of `spawns/Npcs/*.xml` and `spawns/Instances/300200000_Haramel.xml` × `npc_skills/npc_skills.xml` ×
  the effect elements of `skills/skill_templates.xml` × `Effects.java`'s `@XmlElement` bindings × the C++ `skillengine/effect/*.cpp`
  (`AION_UNPORTED(` count; generated data-only classes counted as ported): Poeta, Ishalgen, Sanctum, Pandaemonium 0; Haramel 1 (19214);
  Verteron and Altgard = m5e's 11 classes; Eltnen 23 and Morheim 24 unported leaf classes today, 11 and 10 of them beyond A-10; the
  holders within 200 m of the Morheim (loc 10) and Eltnen (loc 5) arrivals; Kakiti's distance from the Haramel entry (61.9 m); the Java
  line counts and chunks (`chunks.py owner`) of the 12 classes.
- `cycles.toml` rows for the instance destroy breakers and the two K4 tasks; no row for `BindPointTeleportService`'s anonymous tasks
  (`BindPointTeleportService$1`, `$2`, `BindPointTeleportService.cpp:35-36`).
- The pace of §5: git log timestamps and the `AION_UNPORTED(` lines removed per commit (`c1edb0afb`: 295; `760e8ab5c`: 105).
- 124 handler files and 31 other source files call `TeleportService` (272 `teleportTo` sites); 36 handler files call `InstanceService`; 73 `@InstanceID`
  handlers (48 extend `GeneralInstanceHandler` directly), 78 files / 17,155 lines, 308 instance AI files / 23,466 lines.

**Inferred, a lane should confirm before relying on it:**

- **That the C++ `World::despawn`, `abortPlayerActions` and `SpawnTask::run` behave like Java on a player that was never spawned** (risk 4,
  C21). Only a run tells.
- **That a same-map teleport does not change M5b-1's and M5b-2's packet sequences** (§10.6 c). Read from TeleportService.java:514-527; not run.
- **That `CM_LEVEL_READY` on Verteron, Sanctum, Altgard, Morheim and Haramel reaches nothing unported beyond W-01..W-22** — the path is
  ported, but M5d's quests and the arrival npcs are data-driven (W-13).
- **That the 4.8 client sends `CM_MOVE_IN_AIR` during a flight transport and `CM_TELEPORT_ANIMATION_DONE` after every animated teleport** —
  from the Java handlers and the opcode names, not from a capture.
- *(Rev 1 listed "`CM_MOVE_IN_AIR` updates the known list" and "E2 receives `SM_PLAYER_INFO` of the flying E1" here; rev 2 closed both by
  reading — World.java:167-169, 234-235; KnownList.java:155-196; PlayerController.java:90-129 — and moved them to §2.2 steps 4-5.)*
- **"~1,500 lines of bodies"** (§1) — the Java file lines of §2.6 minus the bodies already ported, estimated per file, not counted body by
  body. The effort letters of §5 are sizes (measured); any time is an extrapolation from the git-log pace.
- **What the no-op handler's value answers should be** (N-07): rev 2 prescribes `GeneralInstanceHandler`'s answers for an instance map; no
  caller after destruction was traced to prove any of them matters.
- **That `SpawnEngine::spawnInstance(Haramel)` reaches no unported body** (walkers, static objects, temporary spawns inside an instance).
- **That M5c's gate proved the Daeva seed** (A-05) and that M5e's `simple.secondclass` path works (A-07).
- **That the checker's 60-s schedule is observable in 75 s** on the user's machine under load (T18's window).

**Claims of other plans this analysis checked:** m5c W-08 (`TeleportService::showMap` at `TeleportService.cpp:284`) — **confirmed**;
m5b3-plan.md O-05 (`PortalDialogAI`, `ResurrectAI` → M5f) — **confirmed and extended** by `PortalAI`; m5c §3a (`MultiReturnAction`,
`InstanceTimeClear` → M5f, 8 bodies) — **confirmed** as shells without `.cpp`; m5c D5's Gladiator seed — **fine at level 10, fails from 15**
(W-14); m5d's "windstreams (M5f)" (`:359`) — **corrected**: no windstream is reachable before level 45, M5j.

---

## 13. Open questions this analysis could not settle without building

1. **What `QuestEngine.onEnterWorld` and the zone triggers do on each new map** once M5d's 4,184 XML quests are registered (W-13). The gate
   lane runs C12, C13, C22 first and reads `unported_trace.txt` before writing assertions.
2. **Whether `moveToExitPoint` during enter world works in C++** (risk 4): a not-yet-spawned player through `sendLoc`.
3. **Whether the existing `SM_PLAYER_INFO` decoder can read the flight fields** without knowing the flight state from outside (its branch
   flags, `PacketDecoders.h:192`), or needs the gate to tell it.
4. **Which `ServerTime` zone the gate's server uses** for Haramel's 09:00 reset, and whether the oracle can read it from the profile.
5. **Whether the real 4.8 client sends `CM_POSITION_SELF`** ("C_BLINK") after teleports — decides P-06's `CM_POSITION_SELF` from O to R.
6. ~~Whether the gate should also run one map change with `autogroup.enable = true`~~ — **answered by reading (rev 2)**: an
   autogroup-enabled server does not start: `GameServer.cpp:243` → the `PeriodicInstanceManager` constructor (`PeriodicInstanceManager.cpp:25-50`)
   → `scheduleRegistration`, `AION_UNPORTED` (`:52-55`), uncaught by `runStep` (`GameServer.cpp:139-141, 311-315`). No map change happens,
   so there is nothing to gate; N-06 is deferred (O-07).
7. **Whether the instance's `Npc`s are reclaimed within the run** or only at shutdown — decides whether T20 can pin `Npc` counts or must rely
   on the census.
8. **Whether M5c's or M5d's gate asserts W-08's loud flight master** (G-05 b) — read those gates at branch time.

---

## 14. Review, 2026-09-23

The adversarial review of rev 1 returned **needs-revision** with 13 findings (3 high, 5 medium, 5 low) and a list of verified claims (the
unported counts, the chunk totals, the Java and C++ citations except one, the packet census, lesson 1's result, the prices, the data counts
of Haramel and the arrivals, W-14, finding 1, the D7 quirks, the kinah budget, stage 1's disjoint chunks). Rev 2 re-checked every finding
against the two trees before applying it.

| # | Sev. | Finding | Verdict | What changed |
|---|---|---|---|---|
| 1 | high | T10 expected a burst of `SM_DELETE` to the teleporting player that Java never sends | **confirmed** (World.java:318 before `:325`; KnownList.java:51-56; PlayerController.java:134-135; the port's `PlayerController.cpp:228-229`, `World.cpp:326, 333`) | §2.1 step 6 rewritten; T10 asserts **no** `SM_DELETE` to E2 and `SM_DELETE(E2, 11)` to the observer E1; §10.2 runs C11/C12 right after C8 so E1 stands at Akarios (rev 1's order had E1 back at Melponeh — the review's fix needed this reorder); §10.4 gains the skip-despawn row (killed twice, TeleportService.java:502), the dropped-guard row and the wrong-animation row |
| 2 | high | A dead player logging out inside an instance reaches the unported `instanceRevive` | **confirmed** (`PlayerLeaveWorldService.cpp:118-120` → `PlayerReviveService.cpp:102/106`; the throw skips everything to `delete_()` at `:154`) | W-20 added; T-06's `instanceRevive` ×2 **required** (`kiskRevive` stays W); §1 hole 6; §2.4 step 5; T-08 and N-05 cases (startPos arm, bind arm, a `leaveWorld` that completes); §10.4 row; checklist step 13 (die, log out dead, log in) |
| 3 | high | Monster skills on the maps M5f opens use unported effect classes; M5e's hand-off (O-06) was dropped | **confirmed and re-measured**: Poeta, Ishalgen, Sanctum, Pandaemonium 0 classes; **Haramel 1** (Kakiti's 19214); Verteron/Altgard = m5e's 11 (A-10); beyond A-10, **Eltnen 11 and Morheim 10** classes (12 distinct, 28 sites, 614 Java lines). Several classes the review listed for Morheim (Sleep, Blind, Paralyze, Poison, Dispel, Bind) are M5e's or M5b-3's, so they sit under A-10, not in M5f | A-10 added; W-21, W-22; §1 finding 6 and hole 7; **D15**: X-05a (`SpellAtkDrainInstantEffect`) **required**, X-05b (the 11 classes) optional, the checklist does not fight on Eltnen/Morheim without X-05b (nor on Verteron/Altgard if A-10 is short); risk 11; §3 row; the sixth lane becomes `effects-items` (R for X-05a only) |
| 4 | medium | W-08/N-06 misjudged: with autogroup at its default the server cannot start | **confirmed** (`GameServer.cpp:243` → `PeriodicInstanceManager.cpp:25-55`, `runStep` does not catch, `GameServer.cpp:139-141, 311-315`) | W-08 → **D**; N-06 deferred as O-07 with `scheduleRegistration`; the P5-10 lease is gone from I-01, §6, §3 and the merge order; N-05's autogroup case removed; §13 Q6 answered; risk 2 and §12 corrected |
| 5 | medium | The no-op `InstanceHandler` that `detachInstanceHandler` installs does not exist | **confirmed** (grep: no implementor; 43 pure virtuals; `GeneralInstanceHandler.h:44` holds a `Ref<WorldMapInstance>`) | **N-07**: a file-local, static, non-counted class in `WorldMapInstance.cpp` under the P4-10 lease, its answers specified, a deviation row in `docs/deviations/P4-10.md`; N-02 depends on it; N-05 asserts a non-null, non-retaining handler after destroy; §1 hole 2, §2.5 step 9, §2.6, §2.7, §7, risk 1 |
| 6 | medium | T5's post-landing and distance rows could not kill the "drop the `FLYING` check" mutant | **confirmed** (CM_MOVE_IN_AIR.java:44-60 broadcasts nothing; `SM_PLAYER_INFO` is sent once, on entering the known list) | C6's extra `CM_MOVE_IN_AIR` goes ~630 m, beyond the 95-m range; T5 asserts no known-list churn on either side and then `SM_MOVE(E1)` to E2; the distance row is "the packet that first put E1 in range"; the known-list inference closed by reading (§2.2 steps 4-5, §12); a second mutation row (`updatePosition(…, false)`) |
| 7 | medium | The checklist contradicted the profile it installs | **confirmed** (`m5f.properties.example` carries D11's 1 s) | §11 prerequisites set `destroy_delay_seconds = 600`; step 15 waits 12 minutes, with the reason; D11 says the session uses the default |
| 8 | medium | Effort letters uncalibrated, and the day budget contradicted them | **confirmed** | §5 adopts m5c's size letters and quotes the git-log pace (now including part 3: 105 sites in ~4 h); every item re-lettered by size; the "3-4 days" budget deleted; §6's critical path is stated in bodies and derived from the pace as an extrapolation |
| 9 | low | The teleport statue moves with `NONE`, no beam | **confirmed** (PortalService.java:118-120; `TeleportService.cpp:255`) | checklist step 7; §9's fallback names T-02's overload; T-02 and T-08 mention the same-map arm |
| 10 | low | Several numbers and one citation wrong | **confirmed, one part rejected** (below) | hotspots 73; portal_dialog 182, portal_scroll 29 (re-counted on parsed XML); P5-12a 107; 31 other source files; `Storage.cpp:160`; T-03's **nine** checks; §1 and §12 now both say ~1,500 lines; §10.1 names `QuestEngine.cpp:111` (unless A-06 closed it); §2.7's class/body count re-measured: **15 classes, 66 bodies (+ 2 shells, 8 → 17, 74)** |
| 11 | low | `BindPointTeleportService`'s anonymous tasks mis-described | **confirmed** (`BindPointTeleportService.cpp:35-36`; Java `:53` stored, `:63` unstored) | §2.7 and T-08 use the keys `$1`/`$2` and classify them apart (a K-06-style cycle row vs an `accepted: one-shot task` row) |
| 12 | low | Gaps in the lane and dependency table | **confirmed** | I-01 and the teleport lane take a one-line P5-02a lease for T-07, whose header change and caller go in one commit; T-02 depends on N-03 (`getOrRegisterInstance`, TeleportService.java:318-319); **A-11** names m5c's oracle helpers (which have since appeared in the working tree as untracked files, `tools/oracle/m5c/trade.py`, `trade_config.py`) with G-01's own fallback |
| 13 | low | T13's reuse time and a missed login row in T9 | **confirmed** (SM_INSTANCE_INFO.java:47 writes the remaining seconds; `BindPointTeleportService.onLogin` at PlayerEnterWorldService.java:283) | T13 asserts the remaining seconds ± 2 s; T9 asserts `SM_BIND_POINT_TELEPORT(3, E1, 13, t)` at the relog (or its absence past 600 s); a §10.4 row for it |

**Rejected in part — finding 10's "AIs are 15 + 7 = 22, not 21".** Measured with `javasrc.parse_file` plus a grep for anonymous classes:
`ResurrectAI` has 4 bodies (constructor, `handleDialogStart`, `bindHere`, the anonymous `AIRequest.acceptRequest`), `PortalAI` 5,
`PortalDialogAI` 5 — **14**, not rev 1's 15 — and `ActionItemNpcAI` 7 (6 + the `ItemUseObserver.abort`; the scheduled lambda is part of
`handleUseItemStart`). So §2.7's 21 was right and §1's/§2.6's "15" was the error; rev 2 prints 14 there. Likewise the review's
"27 + 21 + 2 + 19 = 69" used rev 1's 27 bodies for "9 client packets", but only 8 packets are on the path (§2.10's R and O rows, 24 bodies).

**Found by this revision, beyond the review:** HEAD moved to `760e8ab5c` (part 3 committed) and no count rev 2 relies on changed;
`ReturnEffect.cpp` is now fully ported (A-01); rev 1's C-order put E1 at Melponeh during C12, so finding 1's observer needed the §10.2
reorder; the Eltnen/Morheim holders nearest the arrivals are own-race guards, so arriving is safe even without X-05b; the m5c oracle files
exist now (A-11 records them as work in progress rather than absent).

**The milestone after rev 2:** ~101 required bodies (+ N-07's 43 one-line overrides) over ~2,300 Java lines of files; six stage-1 lanes
(teleport, instance-engine, packets, travel-npcs, gate-harness, effects-items) on disjoint chunks; stage 2 the gate (`gs.scenario.m5f`, ~24
cases, three characters), the geo gate (`gs.scenario.m5f_geo`) and the regate; an optional stage 3 only on the user's answer to D5.
