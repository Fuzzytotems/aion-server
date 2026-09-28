# M5h work plan (legion and housing)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 revised after its adversarial review (the closing section, "Review, 2026-09-23", lists what
> changed). A **read-only** analysis. Rev 1 read HEAD `c1edb0afb`; rev 2 re-checked every cited C++ site at HEAD **`760e8ab5c`** ("M5b-2 stage 1
> part 3: the effect classes"), whose working tree modifies only `AggroList.cpp`, `DuelService.cpp`, `CMakeLists.txt` and `tools/oracle` —
> **no file this plan names as M5h work** (checked with `git status`; part 3 committed the `CheckOutput.cpp` change G-07 builds on). **Nothing was
> compiled, built or run.** C++ statements come from reading both trees, from `game-server/chunks.cmake` and
> `tools/porting/chunks.py owner|files|java`, and from counting `AION_UNPORTED(` / `AION_PARTIAL(` sites. Undeclared bodies come from a throw-away
> script that parses the Java sources with `tools/gen/javasrc.py` and looks each method name up in the C++ headers (src, generated, `*Info.h`
> companions); every hit was then read by hand, and four false positives (`delete` → `delete_`, an inner class defined in a `.cpp`) were
> removed. Data statements come from ElementTree parses of `data/static_data` (XML comments are skipped, as the server skips them). §12 separates
> what was **measured** from what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md). Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 8,
> `docs/deviations/P5-11.md` (the wave-5a economy-legion lane), `docs/deviations/P5-10.md`, [m5b3-plan.md](m5b3-plan.md) (T-08, D6, h01/h02),
> [m5c-plan.md](m5c-plan.md) (D5, D-02..D-04, M-01, M-02, W-13), [m5d-plan.md](m5d-plan.md) (D5, A-02, E-09, §8.1), [m5e-plan.md](m5e-plan.md)
> (W-04), [m5f-plan.md](m5f-plan.md) (N-01..N-04, N-06, T-02, P-02, W-06, O-05), [m5g-plan.md](m5g-plan.md) (D1, D14, K-04, O-02),
> [m5i-plan.md](m5i-plan.md) (the cron row at `:94`, Z-01), `generated/concurrency/{cycles,fieldmap}.toml`, `tests/scenario/m5a_partial_allowlist.txt`.
> Every one of those plans is a draft under revision; this plan cites their **item ids**, which move less than their line numbers.
>
> **M5h is the eighth milestone.** Seven come before it. M5b-2 is mid-implementation; M5b-3, M5c, M5d, M5e, M5f and M5g have plans (rev 1 said
> M5e, M5f and M5g had none — they now exist and settle five §0 rows, §13's first three questions and the Wednesday-cron owner). §0 states what
> this plan assumes each delivers; every work item, case and checklist step that stands on an assumption carries its id (`A-01` … `A-25`), so the
> plan can be re-verified in one pass when M5h branches.

---

## 0. What this plan assumes the earlier milestones deliver

| Id | Assumed delivered | By | Where it is today | M5h uses it in | If it is missing at branch time |
|---|---|---|---|---|---|
| **A-01** | `ItemPacketService` (all 9 bodies): every kinah change and every item add/delete sends its packet | M5b-3 (m5c-plan.md A-01 makes the same assumption) | `services/item/ItemPacketService.cpp`, 9 `AION_UNPORTED` | create (−10,000 kinah), emblem (−price twice), warehouse kinah, studio fee (−4,000,000), `CM_HOUSE_EDIT` add (`delete(item, REGISTER)`) | **stage 1 cannot start** |
| **A-02** | `ItemService::addItem` | M5b-3 | `services/item/ItemService.cpp`, 11 `AION_UNPORTED` | the reward of a used house object (UseableItemObject.java:184) | Y16's reward half fails; the rest stands |
| **A-03** | `CM_MOVE_ITEM`, `CM_SPLIT_ITEM`, `CM_REPLACE_ITEM`, `ItemMoveService`, `ItemSplitService`, `ItemRestrictionService` (its legion arms call `LegionMember::hasRights`, ItemRestrictionService.java:23-27, 52-62) | M5b-3 (T-03) | no C++ files / 3 + 4 + 3 `AION_UNPORTED` | legion-warehouse item deposit and withdrawal | Y9's item half is dropped; kinah half stands |
| **A-04** | `LegionService::addWHItemHistory` ported under a P5-11 lease | M5b-3 **T-08** | `LegionService.cpp:420-422`, `AION_UNPORTED` | the warehouse history rows | M5h ports it (1 body, LegionService.java:1082-1092) |
| **A-05** | The item-action API: `canAct`/`act` declared on `AbstractItemAction` with `AION_UNPORTED` stubs in all 32 bound classes (incl. `SummonHouseObjectAction`, `DecorateAction`), and `ItemActions::getHouseObjectAction` / `getDecorateAction` | M5b-3 **h01/h02**, D6 | `AbstractItemAction.h` declares neither; `SummonHouseObjectAction.h` is a shell | `HouseObjectFactory::createNew(House&, ItemTemplate*)` (HouseObjectFactory.java:68-82), `CM_HOUSE_EDIT` action 3 (CM_HOUSE_EDIT.java:80-97) | M5h files the two lookups as its own additive header request |
| **A-06** | `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG` and `DialogService` whole — including the arms `CREATE_LEGION` (`sendDialogWindow`), `DISPERSE_LEGION`, `RECREATE_LEGION`, `OPEN_LEGION_WAREHOUSE`, `HOUSING_RECREATE_PERSONAL_INS`, `onCloseDialog`'s legion-warehouse release, and **`isInteractionAllowed`**, which `ActionItemNpcAI.handleDialogStart` calls before every use bar (ActionItemNpcAI.java:36-39) | M5c **D-02, D-03** (m5c W-13 already lists the close arm as "no legions until M5h") | no C++ files / `DialogService.cpp`, 7 `AION_UNPORTED` (`isInteractionAllowed` at `:27-28`) | every npc-driven case: warehouse, disband, studio fee path, studio portals | **the warehouse, disband and studio cases cannot run**; legion create/invite still can (they need no npc) |
| **A-07** | `CM_QUESTION_RESPONSE` and `ResponseRequester::respond` | M5c **D-04** (or M5b-3) | no C++ file; `ResponseRequester.cpp` ported (0 `AION_UNPORTED`) | invite, brigade-general transfer, disband, recreate (LegionService.java:189-207, 242-274, 282-336, 496-516) | M5h ports it (P5-16, 46 Java lines) |
| **A-08** | The gate seed convention: positions, absolute kinah `item_count`, item rows with `item_unique_id` above `gameserver.idfactory.wrap_at` (2²⁷), `player_quests` rows, all written with `ScenarioDatabase::execute` while the character is offline | M5c **D5** | the helper exists (`M5bScenarioTest.cpp:1786`) | every seed of §10.1 | M5h writes the helpers itself (G-02) |
| **A-09** | A price model in the oracle for `PricesService::getPriceForService` under the gate profile (siege disabled) | M5c G-01 (`m5c-economy`) | `PricesService.cpp` ported (0 `AION_UNPORTED`) | the emblem price (LegionService.java:473, 577) | G-01 reimplements it from `PricesService.java` |
| **A-10** | The `AbstractQuestHandler` base (`sendQuestDialog`, `sendQuestStartDialog`, `sendQuestEndDialog`, `changeQuestStep`, `playQuestMovie`) with `QuestService.finishQuest`, and the XML engine running the `report_to` quest 18801/28801 (`quest_script_data/oriel.xml:95`, `pernon.xml:94`). The registry itself exists: `AION_QUEST_HANDLER` (`HandlerRegistry.h:127, 319-325`, 0 entries) | M5d **H-01..H-07**, E-01, I-05 | `questEngine/**`, P5-06 | the pulled-forward studio quests `_18832`, `_18802`, `_28832`, `_28802` (H-03) | the studio is acquired only through the 4,000,000-kinah dialog (Y13); Y12 moves to M5h-2 |
| **A-11** | `ActionItemNpcAI` (`useitem`) registered — **7 bodies**: the constructor, `handleDialogStart`, `handleUseItemStart`, the anonymous `ItemUseObserver.abort`, `handleUseItemFinish`, `getTalkDelayInMs`, `handleDied` (ActionItemNpcAI.java:31-97) | M5d **D5** / A-02 (which also depends on A-06's `isInteractionAllowed`) | no C++ file (P5-05 root handler) | the base class of `StudioPortalAI` (StudioPortalAI.java:24); the 2-s use bar of every studio portal (§2.7 row 4) | M5h ports it (99 Java lines, 7 bodies) |
| **A-12** | `AbyssPointsService::addAp` ported, **with** its legion arm left calling `Legion::addContributionPoints` | M5d **E-09** (m5d §8.1 "left unported on purpose: `Legion::addContributionPoints` … M5h") | `AbyssPointsService.java:49-51` | nothing in the gate; L-01 closes the callee | no effect |
| **A-13** | A quest REWARD → finish path that pays exp, items and a title (quest 18802: exp 12,951, title 206, items 169500952 and 182213159, `quest_data.xml:46841-46845`) | M5d | – | Y12 | as A-10 |
| **A-14** | `TeleportService::teleportTo(Player&, WorldMapInstance&, x, y, z, h, animation)` and the client side of an animated map change: **`CM_TELEPORT_ANIMATION_DONE`** (a `FADE_OUT_BEAM` teleport arrives only after it — m5f-plan.md §2.1 row 7 and its packet table: "without it an animated teleport never arrives"), then `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN` and the new `CM_LEVEL_READY` | **M5f T-02** ("the five delegating overloads") and **P-02** | the instance overloads are `AION_UNPORTED` (`TeleportService.cpp:260-272`) | entering and leaving a studio (StudioPortalAI.java:36-61) | **the studio cases cannot run**; stage 1's studio lane still lands its bodies and unit tests |
| **A-15** | Personal instances: `getNextAvailableInstance` (which calls the already-ported `SpawnEngine::spawnInstance` → `HousingService::spawnHouses(instance, ownerId)`, `SpawnEngine.cpp:223` = SpawnEngine.java:188) and the partials `InstanceService.cpp:133` (create) and `:153` (`moveToExitPoint`) closed; **the `EmptyInstanceCheckerTask`** — a fixed-rate task, first run 60 s after creation, then every 60 s (InstanceService.java:58), which destroys a *personal* instance at the first run that finds it empty (`:177-180`) — and `destroyInstance` (`:82-107`), which deletes the studio `House` and so runs `HouseController::onDespawn`; `onPlayerLogin`'s personal arm (`:148-155`) | **M5f N-01, N-02, N-03** | `InstanceService.cpp:133` and `:153` are `AION_PARTIAL` (m5a allow-list rows `:39-40`); the checker and `destroyInstance` unported | studio instances; destroy, re-entry, persistence, re-login inside a studio (Y14, Y18) | as A-14 |
| **A-16** | ~~`InstanceService::getOrCreateHouseInstance` from M5f~~ — **settled: M5f does not port it** (N-03 leaves it W, "studios are M5h"; O-05 hands studios to M5h) | – | `InstanceService.cpp:116-118`, `AION_UNPORTED` | studio entry | **H-06 is required** (P5-13 lease, 9 Java lines) |
| **A-17** | ~~the instance spawn hook~~ — **measured: already ported** (`SpawnEngine.cpp:223`); reached once A-15's `getNextAvailableInstance` lands | – | – | – | folded into A-15 |
| **A-18** | **M5g's D1 splits P5-10 into six parts sharing `aion_gs_team`** and hands **P5-10f** — `model/team/legion/**`, `model/challenge/**`, `services/ChallengeTaskService` — to M5h (m5g-plan.md D1, O-02), with the test directory `tests/team/P5-10f` (the shared-target rule, `chunks.cmake:15-16`) | **M5g I-01** | measured today: legion 66 sites, challenge 20, of P5-10's 294 | the legion-model lane | the integrator makes the **same** split on day 0 (I-01) — six parts, P5-10f as named there, never a different letter |
| **A-19** | `CM_CHAT_MESSAGE_PUBLIC` whole, `PlayerRestrictions::canChat` and `PlayerChatService::logMessage` | **M5g D14 / K-04** (ported in its stage 1 if absent; m5i-plan.md Z-01 claims it too if still absent) | no C++ file; `PlayerRestrictions.cpp:196-198` and `PlayerChatService.cpp:20-26` `AION_UNPORTED` | legion chat (CM_CHAT_MESSAGE_PUBLIC.java:77-80, 152) | M5h ports them (P-03, ~12 bodies); **legion chat does not use the chat server** — `ChatType.LEGION` (10) goes through this game-server packet |
| **A-20** | `gs.scenario.m5a` … `m5g` (+ geo variants) green at branch time | all | – | G-05 | re-green first |
| **A-21** | `ExpireTimerTask` registering house objects (PlayerEnterWorldService.java:361-366) | M5b-3 (ExpireTimerTask is ported; the item half is M5b-3's) | `taskmanager/tasks/ExpireTimerTask.cpp` ported | every enter world of a studio owner; the cake's 30-day expiry (§2.12) | nothing extra |
| **A-22** | Nothing from M5e (training and progression). M5e's W-04 notes that a class change of a legion member reaches `LegionService::updateMemberInfo`; M5h ports that body (S-05) | – | – | §2.10 | – |
| **A-23** | **`PlayerService::getOrLoadPlayerCommonData` ×2** (PlayerService.java:235-247) — behind the *ported* `LegionService::getLegionMember(std::string_view)` (`LegionService.cpp:199-201`) and the no-`PlayerCommonData` arm of `getLegionMember(int)` (`:218-222`) | **M5c M-02** (P5-00) | `PlayerService.cpp:323-329`, 2 `AION_UNPORTED` | every name lookup: `appointRank`, `changeNickname`, `kickMember` (LegionService.java:357, 418, 744); `Legion::getMembers`/`streamMembers` for an uncached member (Legion.java:71-77) — after a restart, `updateLegionMemberList` and `getBrigadeGeneral`; `AbstractHouseInfoPacket` for an uncached owner (AbstractHouseInfoPacket.java:25) | M5h ports both under a **P5-00 lease** (S-09, 13 Java lines); **without them C6 and C21 throw** |
| **A-24** | Leaving an instance on a map change: `InstanceService::onLeaveInstance` → **`GeneralInstanceHandler::onLeaveInstance`** → `removeInstanceItems` → `isRestrictedToInstance` (the studios use `GeneralInstanceHandler`: no `@InstanceID` handler exists for 720010000/730010000), and `AutoGroupService::onLeaveInstance` under the Java default `autogroup.enable = true` | **M5f N-03, N-04, N-06** (m5f W-01, W-08) | `InstanceService.cpp:170-172`, `GeneralInstanceHandler.cpp:35, 121, 125` `AION_UNPORTED` | every studio entry and exit (both are cross-map teleports, TeleportService.java:514-519) | as A-14 |
| **A-25** | `SystemMailService::sendMail` | **M5c M-01** | `SystemMailService.cpp:10`, `AION_UNPORTED` | only S-08's reward arm (LegionDominionService.java:139-146), **unreachable until M5i ports `join`** — no legion can have a participant row before then | the arm stays reachable-in-principle and throws only with a hand-made `legion_dominion_participants` row; no gate or checklist step makes one |

**No assumption about M5i or later.** Siege, rifts and the conqueror/protector system are asserted *off* by the gate profile (`gameserver.siege.enable = false`,
`gameserver.cp.enable = false`, as m5c §10.1 sets them) and the two bodies of theirs M5h reaches are ported under leases (S-06). The Legion Dominion
**weekly calculation**, which the Wednesday 09:00 cron has reached since M5a, is M5h's own P5-11 work (S-08, D13) — m5i-plan.md:94 lists it as
"M5h's", and rev 1 had sent it to M5i, so no plan owned it.

---

## 1. Summary

**The roadmap's "P5-11 legion and housing 86" is a third of the work and describes the wrong third.** P5-11 holds 86 `AION_UNPORTED` sites in
its `.cpp` files (87 by grep: the 87th is a dead template body, `HouseRegistry.h:80`'s generic `discard`, which `HouseRegistry.cpp:63-64, 140-141`
writes out per map) and 3 `AION_PARTIAL` over 3,739 Java lines, but the milestone's path runs through **thirteen chunks with bodies** (seven owned,
six under file leases) plus stand-in leases on three more, and the two halves of it are in opposite states:

| | Legions | Housing |
|---|---|---|
| Model | **unported and in P5-10 (P5-10f after M5g's D1), not P5-11**: `Legion` 27, `LegionWarehouse` 28, `LegionEmblem` 6, `LegionMember` 5 sites, and 5 enum-companion bodies no header declares | **ported and only half executed**: `House`, `HouseRegistry`, `HouseBids`, 14 house-object classes, `HouseController`, 5 DAOs — 0 `AION_UNPORTED` apart from 2 placement-limit bodies (one of them dead). Startup runs the unowned-house half (§2.10); the owner-and-object half has never run |
| Service | `LegionService` **63 of 79 bodies unported** (`LegionService.cpp:43-437`); the 16 ported are the M5a load subset (P5-11.md) | `HousingService` **0 of 25 unported**; `TownService` 0; `HousingBidService` 7 (auctions) |
| Client packets | 9 `CM_LEGION*` packets (one is M5i's), **no C++ file** | 12 `CM_HOUSE*`/bid packets + `CM_USE_HOUSE_OBJECT` + `CM_RELEASE_OBJECT`, **no C++ file** |
| Server packets | all 15 `SM_LEGION_*` exist, 0 `AION_UNPORTED` | all 15 house packets exist; `SM_HOUSE_BIDS` has 1 stand-in |
| Handlers | – | the house npcs' AIs (`butler`, `housesign`, `studioportal`, `friendportal`, `housegate`) have **no C++ file**; the studio is acquired through **a chain of quests whose first and last are phase-6 Java handlers** (18832 → 18801 → 18802) |

**Six findings shape the plan.**

1. **A legion row in the database crashes the character list today — before enter world.** The first reader of a member's legion is
   `AbstractPlayerInfoPacket::writePlayerInfo`, which runs for every character of `SM_CHARACTER_LIST` (and `SM_CREATE_CHARACTER`):
   `detail::getLegionMember(*pcd)` (`AbstractPlayerInfoPacket.cpp:33` → `PacketLookups.cpp:58-61`) → `LegionService::getLegionMember` (ported,
   `LegionService.cpp:208-230`) → `LegionMemberDAO::loadLegionMember` → `LegionService::getLegion` → `Legion::Legion` (**`AION_UNPORTED`**,
   `Legion.cpp:40-46`) and `LegionMember::setPlayerData` (`AION_UNPORTED`, `LegionMember.cpp:25-31`). The packet writes the legion id and name
   (AbstractPlayerInfoPacket.java:111-113). `CM_DELETE_CHARACTER` is a second reader (`CM_DELETE_CHARACTER.cpp:34`), and `PlayerService::getPlayer`
   at enter world a third (`PlayerService.cpp:188-190`). Nothing has noticed because no gate has a legion. The load path is exercised in normal
   play **only after a server restart** — so the gate seeds a pre-existing legion (§5 D8) to reach it at all, and its first observable is C's
   character-list entry (Y11).
2. **The first cut is legions plus studios; land houses and towns are a second milestone, M5h-2, run straight after.** Studios are the housing a
   player meets first (the quest chain 18832 → 18801 → 18802, or 28832 → 28801 → 28802, at level 21, free — D4 pulls its Java handlers forward)
   and they exercise all of the house machinery — registry, objects, decorations, scripts, butler, settings, persistence. Land houses add only
   acquisition by **weekly auction**, **weekly rent** and visiting, which are cron-driven, mail-driven and money-driven and need a differently
   shaped gate; towns level only through challenge-task quests. §3 has the whole argument; the Legion Dominion ("Stonespear Reach") goes to M5i,
   **except its weekly calculation**, which the Wednesday cron has reached since M5a and which M5h ports (S-08, D13).
3. **Wiring legions in wakes eight kinds of entry point outside the two chunks** (§2.10): the character list and character deletion (finding 1); every
   experience gain of a legion member (`Rates.calcXpRate` → `Legion.hasBonus`, Rates.java:175-181, `RatesInfo.cpp:49`); every level-up
   (`PlayerController.java:609-610`), class change (m5e W-04) and **cross-map teleport** of a member (`updateMemberInfo`, TeleportService.java:246,
   535 = `TeleportService.cpp:146`) — the studio entrance and exit are cross-map teleports; every leave or kick
   (`ConquerorAndProtectorService.onLeaveLegion`, P5-12b, `AION_UNPORTED`); every disband (`SiegeService.cleanLegionId`, P5-12a,
   `AION_UNPORTED`); every `SM_LEGION_*` broadcast (`PacketSendUtility.broadcastToLegion` → `Legion::getOnlinePlayers`, unported); every lookup
   of a member **by name** or of an **uncached** member (`PlayerService::getOrLoadPlayerCommonData` ×2, P5-00, unported — A-23); and every enter
   and leave world of a member.
4. **Wiring studios in wakes the owner-and-object half of the housing code, ported and never run.** Startup already runs the unowned half at
   every M5a start (1,030 houses: `HouseController.onAfterSpawn`, `updateSpawns`, the empty script and registry loads), and every login and logout
   runs `HouseObjectCooldownsDAO` with empty data. Never run: the 14 house-object classes (941 Java lines — no house has ever held an object),
   `HouseObjectFactory`, `HouseRegistry`'s put/move/discard/save arms, the studio spawn arm and `HouseController.onDespawn`, `House.save` →
   `HousesDAO.storeHouse`, `PlayerRegisteredItemsDAO.store` and a `loadRegistry` with rows, `PlayerScripts.set/remove` and `HouseScriptsDAO`'s
   writes — roughly 1,300-1,600 Java lines (inferred from which methods the startup and login paths call) — and all ~725 lines of legion
   persistence (LegionDAO, LegionMemberDAO, LegionStorageProxy). M5b-1 predicted two hidden prerequisites and found eight; this is the milestone
   where that ratio is most likely to repeat, and it is the reason the gate is built around the studio's whole lifecycle.
5. **Lesson 1 finds 83 bodies no site count shows** (§2.9): 5 enum companions, 7 anonymous `RequestResponseHandler` bodies in `LegionService`,
   **`DecorateAction::getTemplateId`** (the generated member block holds `partId` but no accessor), 46 bodies in 15 client packets with no C++
   file, 21 in the seven handlers the path needs (two root handlers, five phase-6 ones), and 3 in `LegionDominionIntruderUpdateTask` (no C++ file;
   M5i) — **80** of them in the first cut, plus the 4 item-action stubs M5b-3 is to declare, and 7 more in `ActionItemNpcAI` if M5d does not
   deliver it (A-11). Four false positives were removed by hand.
6. **The first cut is ~224 bodies over ~3,770 Java lines**, 1.5 times M5b-1's stage 1 (~150 sites over six lanes) — and unevenly spread: the
   legion-service lane alone holds ~76 bodies in one file of one chunk (`LegionService.cpp` cannot be split between lanes), the legion-model lane
   71. **Stage 1 therefore runs in two parts with a commit between them** (M5b-2 stage 1's precedent), and the legion-service lane is **the critical path
   at 9-12 agent-days** by its effort letters — above M5b-2's K-lanes' 7-9 (§7, D14). M5h-2 is ~72 bodies over ~1,950 Java lines; the Legion Dominion
   share left to M5i is ~20 bodies.

| Scope | Sites | Bodies no count shows | **Bodies** | Java lines |
|---|---|---|---|---|
| **M5h (first cut)** | 140 (+4 stubs A-05 creates) | 80 | **~224** (+ ~12 if A-19, 7 if A-11, 3 if A-07, 2 if A-23, 1 if A-04 is missing) | ~3,770 |
| **M5h-2** (land houses, auctions, rent, visiting, towns) | 35 + 3 partials | 31 | ~72 (69 + the 3 partial closures) | ~1,950 |
| **to M5i** (Legion Dominion's instance, rifts and ranking; legion abyss ranking) | 8 | 12 | ~20 | ~540 |
| ported, first executed by M5h | 0 | – | – | ~1,300-1,600 housing + ~725 legion persistence |

---

## 2. The paths, end to end

"ported" means the body exists and contains no `AION_UNPORTED`; "U" means an `AION_UNPORTED` site; "no file" means no `.h`/`.cpp` under `src/`
or `handlers/`.

### 2.1 Creating and joining a legion

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | The legion manager (203806 *Losadis*, Sanctum; 204115 *Sorante*, Pandaemonium) offers `CREATE_LEGION` (5); `DialogService` only answers `SM_DIALOG_WINDOW` with the client's name-entry page | DialogService.java:100, 293-296 | A-06 | P5-08 |
| 2 | Client sends `CM_LEGION(0x00, readD, name)`; **a non-member reaches only arm 0x00**, a member every other arm | CM_LEGION.java:39-107, 116-165 | **no file** (opcode 0x00F0, `ClientPacketInfo.gen.inc:56`) | P5-16 |
| 3 | `LegionRestrictions.canCreateLegion`: `NameRestrictionService.isValidLegionName` / `isForbidden`, `LegionDAO.isNameUsed`, already a member, kinah `< creationrequiredkinah`. **No npc-distance check** — "STR_GUILD_CREATE_TOO_FAR_FROM_CREATOR_NPC TODO" (LegionService.java:809), so a client can create a legion anywhere | LegionService.java:804-821 | U (`LegionService.cpp:43`); the name checks and DAO are ported | P5-11 |
| 4 | `new Legion(IDFactory.nextId(), name)` → `addLegionMember(playerId)` → `decreaseKinah` → `storeLegion(legion, true)` → `addLegionMember(legion, player, BRIGADE_GENERAL)` → `addHistory(CREATE)`, `addHistory(JOIN)` → `STR_GUILD_CREATED` | LegionService.java:209-223 | U (`:252`); `Legion` ctor U (`Legion.cpp:40-46`) | P5-11, P5-10 |
| 5 | `addLegionMember(legion, player, rank)`: new `LegionMember`, `setPlayerData`, `setRank`, `LegionMemberDAO.saveNewLegionMember`, cache; `SM_LEGION_INFO` to the player; `updateLegionMemberList(player, false, player)` (the joiner excluded, so a new legion sends **one empty `SM_LEGION_MEMBERLIST`**: `FixedElementCountSplitList(…, true, 80)` splits empty data once); `broadcastToLegion(SM_LEGION_ADD_MEMBER(player, false, 1300260, name))`; `broadcastPacket(SM_LEGION_UPDATE_EMBLEM, toSelf)`; `broadcastToLegion(SM_LEGION_EDIT(0x08))`; `broadcastPacket(SM_LEGION_UPDATE_TITLE, toSelf)`; `legion.addBonus()` | LegionService.java:682-708, 1103-1117 | U (`:389-395`, `:428-430`); all packets ported | P5-11 |
| 6 | `addHistory`: `LegionDAO.insertHistory` → `legion.addHistory` (prepend, and for REWARD/WAREHOUSE drop entries older than 365 days) → `LegionDAO.deleteHistory` → `broadcastToLegion(SM_LEGION_HISTORY(history(type), type))` | LegionService.java:668-676; Legion.java:320-333 | U (`:385`, `Legion.cpp:140`); DAO ported | P5-11, P5-10 |
| 7 | Invite: `CM_LEGION(0x01, name)` → `World.getPlayer(name)` → `canInvitePlayer` (null, denied status, dead, self, already a member of this/another legion, `hasRights(INVITE)`, race) → an anonymous `RequestResponseHandler` → `SM_QUESTION_WINDOW(STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION, 0, 0, legionName, level, inviter)` to the target, `STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM` to the inviter | LegionService.java:242-274, 823-852 | U (`:261`, `:47`); the handler struct does not exist (§2.9) | P5-11 |
| 8 | The target answers `CM_QUESTION_RESPONSE` → `acceptRequest` → `addToLegion` → `legion.addLegionMember` (`canAddMember`: level-1 cap 30) → step 5 with rank VOLUNTEER → `displayLegionAnnouncement` → `addHistory(JOIN)`; `denyRequest` → `STR_GUILD_INVITE_HE_REJECTED_INVITATION` | LegionService.java:225-240, 249-257; Legion.java:87-93, 224-243 | A-07; U | P5-16, P5-11 |

### 2.2 Running a legion: ranks, permissions, announcement, chat, history, level

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | Rank: `CM_LEGION(0x06, rankId, name)` → `appointRank` → **`getLegionMember(name)`** → `PlayerService.getOrLoadPlayerCommonData(name)` (**A-23**; online or not) → `canAppointRank` (brigade general only) → `setRank`, store if offline, `broadcastToLegion(SM_LEGION_UPDATE_MEMBER(member, msgId, name))` with msgId DEPUTY 1400902, CENTURION 1300267, LEGIONARY 1300268, VOLUNTEER 1400903. The nickname (`0x0F`) and the kick (`0x04`) look their member up the same way (LegionService.java:418, 744) | LegionService.java:140-143, 356-372, 893-912 | U (`:283`, `:59`); the name lookup is ported (`LegionService.cpp:199-201`) and reaches `PlayerService.cpp:327-329` (U) |
| 2 | Brigade general: `CM_LEGION(0x05, name)` → `startBrigadeGeneralChangeProcess` (question 904979 to the leader) → `appointBrigadeGeneral(leader, target)` (a second question, to the target) → `appointBrigadeGeneral(member)`: old leader → CENTURION, two `SM_LEGION_UPDATE_MEMBER`, `SM_LEGION_EDIT(0x08)`, history APPOINTED | LegionService.java:282-352 | U ×3; three anonymous handlers |
| 3 | Permissions: `CM_LEGION(0x0D, deputy, centurion, legionary, volunteer)` → brigade general only → `SM_LEGION_EDIT(0x02, …)` broadcast. `LegionMember.hasRights(mask)`: brigade general always; otherwise `mask.can(rankPermission)` with defaults **deputy 0x1E0C, centurion 0x1C08, legionary 0x1800, volunteer 0x800** (Legion.java:29-32; `sql/aion_gs.sql:505-517` has the same defaults) and masks EDIT 0x200, INVITE 0x8, KICK 0x10, WH_WITHDRAWAL 0x4, WH_DEPOSIT 0x1000 (LegionPermissionsMask.java:8-14) | LegionService.java:383-395; LegionMember.java:116-124 | U; `hasRights` U (`LegionMember.cpp:34`); `can` has **no companion** (§2.9) |
| 4 | Self intro `0x0A`, nickname `0x0F` (brigade general, pattern `.{1,10}`), level up `0x0E` (`canChangeLevel`: brigade general, not max, **challenge tasks at level ≥ 5**, kinah ≥ `getKinahPrice`, `hasRequiredMembers`, contribution ≥ `getContributionPrice`) | LegionService.java:374-426, 918-948 | U |
| 5 | Announcement: `CM_LEGION(0x09, text)` → `changeAnnouncement` → `hasRights(EDIT)` → truncate to 256 → `LegionDAO.saveAnnouncement` → `STR_GUILD_WRITE_NOTICE_DONE` + `broadcastToLegion(SM_LEGION_EDIT(announcement))` (type 0x05), or an empty text clears it (`STR_MSG_CLEAR_GUILD_NOTICE` + `SM_LEGION_INFO` to the others). `CM_LEGION(0x07)` (the `/gnotice` command) → `STR_GUILD_NOTICE(text, epochSeconds)` or `STR_MSG_NOSET_GUILD_NOTICE`. `SM_LEGION_INFO` writes one announcement then the empty stop string (SM_LEGION_INFO.java:45-53) | LegionService.java:629-652; CM_LEGION.java:134-141 | U (`:373`) |
| 6 | Legion chat: `CM_CHAT_MESSAGE_PUBLIC(10, text)` → `ChatProcessor.handleChatCommand` → `PlayerRestrictions.canChat` → `PlayerChatService.logMessage` → `NameRestrictionService.filterMessage` → `broadcastToLegionMembers` → `SM_MESSAGE` to every online member | CM_CHAT_MESSAGE_PUBLIC.java:45-80, 152 | **no file**; A-19 |
| 7 | History: `CM_LEGION_HISTORY(page, type)`; REWARD only for the brigade general → `SM_LEGION_HISTORY(history(type), page, type)` | CM_LEGION_HISTORY.java | **no file** |

### 2.3 The legion warehouse

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | 84 npcs offer `OPEN_LEGION_WAREHOUSE` (53) — 203751 *Pauton* in Sanctum, 204074 *Gullinkambi* in Pandaemonium (measured over `npc_templates.xml` `func_dialogs`) → `openLegionWarehouse` → `canOpenWarehouse`: member, `LegionConfig.LEGION_WAREHOUSE`, npc supports 53, not disbanding, `hasRights(WH_DEPOSIT) \|\| hasRights(WH_WITHDRAWAL)`, **`legWh.setInUse(playerId)` — a compare-and-set that admits one member at a time** | LegionService.java:480-494, 1024-1045 | U (`:327`, `:91`); `LegionWarehouse::setInUse` U (`LegionWarehouse.cpp:113`) |
| 2 | `LegionWhUpdate(player)` (a save), `SM_LEGION_EDIT(0x04, kinah)`, `SM_WAREHOUSE_INFO` parts of 10 items (storage id 3) from `FixedElementCountSplitList(items, false, 10)` — **an empty warehouse yields no part** (`oneTimeSplitOnEmptyData = false`, SplitList.java:30-34) — and always a closing `SM_WAREHOUSE_INFO(null, 3, whLvl, first = items.isEmpty())`, `SM_DIALOG_WINDOW(page 25)`. So an empty warehouse sends **one** `SM_WAREHOUSE_INFO` (first = 1, no items), a warehouse of 1-10 items two (the part with first = 1, then the closing one with first = 0) | LegionService.java:482-493 | `LegionWhUpdate` ported |
| 3 | Items: `CM_MOVE_ITEM` / `CM_SPLIT_ITEM` with storage 3 → `player.getStorage(3)` = a `LegionStorageProxy` over `LegionWarehouse` (`Player.cpp:414-416`) → `ItemRestrictionService` legion arms → `addWHItemHistory` → `SM_LEGION_HISTORY(WAREHOUSE)` | ItemMoveService.java:56-58; ItemSplitService.java:80, 94 | A-03, A-04; `LegionStorageProxy` ported (P4-13) |
| 4 | Kinah: `CM_LEGION_WH_KINAH(amount, 0 = withdraw \| 1 = deposit)` → `hasRights(WH_WITHDRAWAL \| WH_DEPOSIT)` → proxy `tryDecreaseKinah` / `increaseKinah` → `addHistory(KINAH_*)`. **It checks neither an open warehouse nor the in-use holder nor npc distance** — any member with the right can move kinah from anywhere (D7) | CM_LEGION_WH_KINAH.java:34-63 | **no file** |
| 5 | Close: `CM_CLOSE_DIALOG` → `DialogService.onCloseDialog` → `unsetInUse(playerId)` | DialogService.java:60-61 | A-06; `unsetInUse` U (`LegionWarehouse.cpp:109`) |
| 6 | Save: on logout (`LegionWhUpdate`, PlayerLeaveWorldService.java:111) and every `PeriodicSaveConfig.LEGION_ITEMS` seconds (`PeriodicSaveService.LegionWarehouseSaveTask`) | PeriodicSaveService.java:45-71 | ported (`PeriodicSaveService.cpp:51-`) |

**`LegionWarehouse`'s 28 sites are small**: 22 are one-line `throw new UnsupportedOperationException("LWH should be used behind proxy")`
(LegionWarehouse.java:42-159); the real ones are the constructor, `increaseKinah(long)` (for siege rewards, through a new proxy), the three
in-use compare-and-set bodies, `setLimit` and `updateLimit((3 + expansions) × 8)`.

### 2.4 The emblem

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | Stock emblem at 203807 *Inofe* / 204116 *Garun* (`LEGION_CHANGE_EMBLEM` 52): `CM_LEGION_MODIFY_EMBLEM(legionId, emblemId, type, a, r, g, b)` → `storeLegionEmblem` → `canStoreLegionEmblem`: id 0..49, brigade general, **legion level ≥ 2 (refused silently: no packet)**, kinah ≥ `PricesService.getPriceForService(800,000, race)` → history EMBLEM_MODIFIED, kinah, `setEmblem`, `updateMembersEmblem` (an `SM_LEGION_UPDATE_EMBLEM` broadcast around every online member), `STR_GUILD_CHANGE_EMBLEM` | CM_LEGION_MODIFY_EMBLEM.java; LegionService.java:469-478, 1047-1063 | **no file**; U |
| 2 | Custom emblem, **legion level ≥ 3**: `CM_LEGION_UPLOAD_INFO(totalSize, a, r, g, b)` → `uploadEmblemInfo` (reset, `setUploadSize`, `setUploading`); then `CM_LEGION_UPLOAD_EMBLEM(size, bytes)` repeatedly → `uploadEmblemData` accumulates; when `uploadedSize >= uploadSize`: corrupt if 0 or larger, else kinah, `setCustomEmblemData`, `LegionDAO.storeLegionEmblem`, history EMBLEM_REGISTER, `updateMembersEmblem` (which also calls `sendEmblemData` for every online member when CUSTOM), `STR_GUILD_WARN_SUCCESS_UPLOAD_EMBLEM` | LegionService.java:553-590, 1008-1022 | **no file** ×2; U |
| 3 | Any client asks for another legion's emblem: `CM_LEGION_SEND_EMBLEM(legionId)` → `sendEmblemData` → `SM_LEGION_SEND_EMBLEM(legionId, emblem, dataLength, name)` then the data in **`SM_LEGION_SEND_EMBLEM_DATA` chunks of at most 7,993 bytes**; `CM_LEGION_SEND_EMBLEM_INFO(legionId)` → the header packet alone with length 0 | LegionService.java:592-624; CM_LEGION_SEND_EMBLEM*.java | **no file** ×2; U |

### 2.5 Login, logout, level-up and experience of a legion member

| # | Step | Java | C++ today |
|---|---|---|---|
| 0 | **Before any of this, at the character list**: `SM_CHARACTER_LIST` / `SM_CREATE_CHARACTER` → `writePlayerInfo` → `getLegionMember(pcd)`, which loads and caches the member and its legion, and writes the legion id, name and a 1/0 flag; `CM_DELETE_CHARACTER` asks the same (a member cannot be deleted) | AbstractPlayerInfoPacket.java:34, 111-113; CM_DELETE_CHARACTER.java:51 | ported (`AbstractPlayerInfoPacket.cpp:33` → `PacketLookups.cpp:58-61`; `CM_DELETE_CHARACTER.cpp:34`) → the same unported bodies as row 1, **inside a packet's `writeImpl`** (§9 risk 4) |
| 1 | `PlayerService.getPlayer` → `getLegionMember(pcd)` → `player.setLegionMember` | PlayerService.java:110-112 | ported (`PlayerService.cpp:188-190`) → **reaches the unported `Legion` ctor, `setMemberIds`, `setLegionEmblem`, `setHistory`, `LegionWarehouse` ctor and `LegionMember::setPlayerData`** (finding 1) — unless row 0 already cached the member |
| 2 | `PlayerEnterWorldService` → `LegionService.onLogin`: `updateMemberInfo` (broadcast `SM_LEGION_UPDATE_MEMBER`), `STR_MSG_NOTIFY_LOGIN_GUILD` to the others, `SM_LEGION_ADD_MEMBER(player, true, 0, "")` to all, `SM_LEGION_INFO`, member list, the announcement, `SM_LEGION_EDIT(0x06)` if disbanding, the bonus icon | PlayerEnterWorldService.java:273-274; LegionService.java:755-782 | call site ported (`PlayerEnterWorldService.cpp:504`), body U (`LegionService.cpp:413`) |
| 3 | Leave world: `LegionWhUpdate`; `onLogout`: `unsetInUse`, `updateMemberInfo`, `storeLegion`, `storeLegionMember`, `removeBonus` | PlayerLeaveWorldService.java:111, 133-134; LegionService.java:784-792 | call sites ported (`PlayerLeaveWorldService.cpp:134, 158`), `onLogout` U (`:417`) |
| 4 | Level change → `LegionService.updateMemberInfo` (`setPlayerData` + an `SM_LEGION_UPDATE_MEMBER` broadcast); the same body runs on a class change (m5e W-04) and on **every teleport that changes the map** (`getLegionMember().getWorldId() != worldId`) — the studio entrance and exit included | PlayerController.java:609-610; LegionService.java:537-541; TeleportService.java:245-246, 534-535 | call sites ported (`PlayerController.cpp:712-713`, `TeleportService.cpp:146`), body U (`:353`) |
| 5 | **Every experience gain** (kill, group, quest, gathering, crafting): `Rates.calcXpRate` multiplies by 1.1 when `player.getLegion().hasBonus()`, i.e. while **10 or more members are online** (`addBonus`/`removeBonus`, Legion.java:342-362, which also send `SM_ICON_INFO(1, …)`) | Rates.java:175-181 | ported (`RatesInfo.cpp:49`), `Legion::hasBonus` U (`Legion.cpp:156`) |
| 6 | AP gain → `Legion.addContributionPoints` + `SM_LEGION_EDIT(0x03)` | AbyssPointsService.java:49-51 | A-12; `addContributionPoints` U (`Legion.cpp:92`) |

### 2.6 Leaving, kicking, disbanding

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | `CM_LEGION(0x02)` → `leaveLegion` → `canLeave` (not the brigade general, not the warehouse user) → `removeLegionMember(member, null)`; `CM_LEGION(0x04, name)` → `kickMember` → **`getLegionMember(name)` (A-23)** → `canKickPlayer` (member of this legion, not self, not the brigade general, **strictly lower rank**, `hasRights(KICK)`) → `removeLegionMember(member, kicker)` | LegionService.java:743-753, 854-876, 986-995 | U; the name lookup reaches `PlayerService.cpp:327-329` (U) |
| 2 | `removeLegionMember`: **`deleteLegionMemberFromDB` first** — cache, `LegionMemberDAO.delete`, `legion.removeMember`, **`addHistory(KICK, name)` for a leave and a kick alike** (so the remaining members get an `SM_LEGION_HISTORY(LEGION)` before anything else); then `unsetInUse`, `SM_LEGION_LEAVE_MEMBER` to the rest (1300247 kicked / 1300240 left), to the leaver (1300246 / 1300241), `SM_LEGION_UPDATE_TITLE(id, 0, "", rank)` around him, `SM_ICON_INFO(1, false)` if the bonus was on, `resetLegionMember`, **`ConquerorAndProtectorService.onLeaveLegion`**, `removeBonus` | LegionService.java:95-101, 714-741 | U (`:397-403`); `onLeaveLegion` **U in P5-12b** (`ConquerorAndProtectorService.cpp:123-125`) — it is a two-line body behind `CustomConfig.CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED` (ConquerorAndProtectorService.java:124-137), so it throws even with the system disabled |
| 3 | Disband at the legion manager (`DISPERSE_LEGION` 6): `requestDisbandLegion` → `canDisbandLegion` (brigade general, not already, warehouse not in use, **warehouse empty and 0 kinah**) → question → `setDisbandTime(now + gameserver.legion.disbandtime)` → `SM_LEGION_UPDATE_MEMBER(…, 1300303, time)` + `SM_LEGION_EDIT(0x06, time)` to every online member; `RECREATE_LEGION` (7) → question `STR_GUILD_DISPERSE_STAYMODE_CANCEL` → `setDisbandTime(0)`, **`broadcastToLegion(SM_LEGION_EDIT(0x07))`, then per online member `SM_LEGION_UPDATE_MEMBER(member, 1300307, "")` and `SM_LEGION_EDIT(0x07)` again** | LegionService.java:189-207, 452-467, 496-516, 965-1006 | U; two anonymous handlers |
| 4 | **The disband itself is lazy**: the next `getLegion`/`getLegionMember` after the time runs `checkDisband` → `disbandLegion`: drop caches, **`SiegeService.cleanLegionId`**, `deleteLegionFromDB`, `updateAfterDisbandLegion` (title reset, `SM_LEGION_LEAVE_MEMBER(1300302)`, `resetLegionMember`, `onLeaveLegion`) | LegionService.java:168-187, 430-438 | `checkDisband` ported (`LegionService.cpp:232-240`), `disbandLegion` U (`:243`), `cleanLegionId` **U in P5-12a** (`SiegeService.cpp:208-210`, SiegeService.java:449-456) |

### 2.7 A studio: acquire, enter, decorate, use, configure, leave

The data: 1,032 house addresses — 500 in Oriel (700010000), 500 in Pernon (710010000), 9 in Heiron, 9 in Beluslan, 6 in Inggison, 6 in Gelkmaros
and **one studio address per race**: 2001 in `720010000` (Elyos, land 329001, building 355000 `PERSONAL_INS`/`STUDIO`, `parts_match="CP_D"`, sale
level 21, **gold price 4,000,000**, exit to Oriel (2573, 1961, 185)) and 3001 in `730010000` (Asmodians, exit to Pernon (1197, 2773, 236))
(`housing/houses.xml`, `housing/house_buildings.xml:99-105`).

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | **Free studio**: quest 18802 (Elyos, Oriel, `minlevel_permitted="21"`) / 28802 (Asmodians) — a **Java handler**: started at 830005 *Celaeno*, in REWARD state at npc 830069 *Parrine*, `SELECTED_QUEST_NOREWARD` (23) → `HousingService.registerPlayerStudio(player)` → `sendQuestEndDialog`. **It is the third quest of a chain**: 18802 needs 18801 finished, 18801 (an XML `report_to` quest, 830001 → 830005) needs **18832** finished (`quest_data.xml:46837-46848`), and 18832 is another **Java handler** (`_18832ImaginingAQuietLife`, 830365 → 830001, Q05); the Asmodian chain is 28832 (Q09) → 28801 → 28802 (`quest_data.xml:61820-61832`). Without 18832 no real player can start 18802 (D4) | `_18802AndAHomeforEveryDaeva.java:20-65`; `_18832ImaginingAQuietLife.java:20-63`; `_28802BeItEverSoHumble.java`; `_28832TakingtheTour.java` | **no file** (18802/18832: chunk **Q05**; 28802/28832: chunk **Q09**; phase 6); A-10 | Q05, Q09 |
| 2 | **Paid studio**: 830069 / 830153 *Sarrik* offer `HOUSING_RECREATE_PERSONAL_INS` (96) → `recreatePlayerStudio`. The server checks nothing but ownership and kinah, **but the client offers the button only when `SM_HOUSE_OWNER_INFO` carries `BIDDING_ALLOWED`**, i.e. `canOwnHouse` = quest 18802/28802 COMPLETE (SM_HOUSE_OWNER_INFO.java:31-35, 44; HousingService.java:263-272) — so in real play the paid path is a *re*-creation after the quest | DialogService.java:270-271 | A-06 | P5-08 |
| 3 | `createStudio(player, chargeFee)`: refuse if `player.getHouses()` is not empty (`STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE`); `getStudioAddress(race)`; `tryDecreaseKinah(goldPrice)` when charged; `new House(address, 0)`, NEW, `changeOwner` → `resetRegistry`, scripts, owner, door, sign notice, acquire time, `save()` → `notifyAboutOwnerChange`: `resetHouses`, `SM_HOUSE_OWNER_INFO`, `SM_HOUSE_ACQUIRE(playerId, 2001, 1)`; `STR_MSG_HOUSING_INS_OWN_SUCCESS` | HousingService.java:92-147, 236-261 | **ported** (`HousingService.cpp`, 0 `AION_UNPORTED`) | P5-11 |
| 4 | **Enter**: 730517 *studio entrance* (Oriel, 20.2 m from Parrine; ai `studioportal`; **`talk_info distance="5" delay="2"`**, bound radius 0.35, `npc_templates.xml:454686`) → `CM_SHOW_DIALOG` → `NpcController.onDialogRequest`: **`isInTalkRange` = talk distance + 1 + both bound radii** (PositionUtil.java:306-309, 243-250; a player's radius is 0.25, PlayerAccountData.java:99) = **6.6 m**, else `STR_DIALOG_TOO_FAR_TO_TALK` (NpcController.java:250-263) → `ActionItemNpcAI.handleDialogStart` → `DialogService.isInteractionAllowed` (A-06) → `handleUseItemStart`: **the timed path** — an anonymous `ItemUseObserver` (aborted by any move, attack, cast, sit, equip), `SM_USE_OBJECT(player, npc, 2000, 1)`, `SM_EMOTION(START_QUESTLOOT)` (to self and around), a 2,000-ms `ACTION_ITEM_NPC` task, then `SM_EMOTION(END_QUESTLOOT)`, `SM_USE_OBJECT(player, npc, 2000, 2)`, the observer removed, and only then `handleUseItemFinish` (ActionItemNpcAI.java:41-77) → `StudioPortalAI.handleUseItemFinish` → `getPlayerStudio` (else `STR_HOUSING_ENTER_NEED_HOUSE`) → `InstanceService.getOrCreateHouseInstance` → `getOrCreatePersonalInstance(720010000, ownerId)` → the instance spawn → `HousingService.spawnHouses(instance, ownerId)` → `spawnStudio` → `SpawnEngine.bringIntoWorld(studio)` → `TeleportService.teleportTo(player, instance, …, FADE_OUT_BEAM)` — an animated cross-map teleport: `onLeaveInstance` (A-24), `updateMemberInfo` for a legion member, and arrival only after `CM_TELEPORT_ANIMATION_DONE` (A-14). The personal instance is not *registered* to the player, so neither entering nor leaving sends an instance message (InstanceService.java:210-219; TeleportService.java:531-532 sends `STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF` only for a non-personal instance) | StudioPortalAI.java:36-61; InstanceService.java:127-146; HousingService.java:149-185 | `StudioPortalAI` **no file** (chunk **A1**); `ActionItemNpcAI` A-11; `getOrCreateHouseInstance` U (`InstanceService.cpp:116-118`, H-06); personal instance creation `AION_PARTIAL` (`:133`); instance teleport U — A-14, A-15, A-24 | A1, P5-13, M5f |
| 5 | The studio spawns: `HouseController.onAfterSpawn` → `getPlayerScripts`, `getRegistry` (→ `PlayerRegisteredItemsDAO.loadRegistry` → `HouseObjectFactory.createNew(registry, id, templateId)` for each stored object), `updateSpawns` (butler 810021 `studio butler`, relationship crystal 810003; the studio address has no SIGN spawn: `house_npcs.xml` has 1,032 MANAGER, 1,032 TELEPORT and **1,030** SIGN spawns), `GeoService.setHouseDoorState`; the owner sees `SM_HOUSE_RENDER` (PlayerController.java:116) and, at `CM_LEVEL_READY`, `SM_HOUSE_OBJECTS` | HouseController.java:52-89; PlayerRegisteredItemsDAO.java:86-92 | `HouseController` ported (P4-11b); **`HouseObjectFactory::createNew` ×2 U** (`HouseObjectFactory.cpp:7-13`, P5-07); `butler` **no file** (P5-05) | P4-11b, P5-07, P5-05 |
| 6 | **Decorate**: `CM_HOUSE_EDIT(1)` enter mode → `SM_HOUSE_EDIT(1)`, `SM_HOUSE_REGISTRY(1)`, `SM_HOUSE_REGISTRY(2)`; `(3, itemObjId)` → `inventory.delete(item, REGISTER)`; a `housedeco` item becomes a `HouseDecoration`, anything else `HouseObjectFactory.createNew(house, template)` → `SM_HOUSE_EDIT(3, 2 \| 1, objId)`; `(5, objId, x, y, z, rot)` spawn → `SM_HOUSE_EDIT(5, …)`, `obj.spawn()` (the owner sees `SM_HOUSE_OBJECT`), `SM_HOUSE_EDIT(4, 1, objId)`, `QuestEngine.onHouseItemUseEvent`; `(6)` move (despawn, `SM_HOUSE_EDIT(7)`, `SM_HOUSE_EDIT(5)`, respawn); `(7)` back to the registry; `(4)` delete; `(2)` leave mode. `CM_HOUSE_DECORATE(objId, templateId, lineNo)` → `PartType.getForLineNr` → `registry.setUsed(decor, room)` → `SM_HOUSE_EDIT(4, 2, objId)` **twice**, `updateAppearance` (`SM_HOUSE_UPDATE`) | CM_HOUSE_EDIT.java:40-151; CM_HOUSE_DECORATE.java | **no file** ×2; `HouseRegistry`, `HouseDecoration`, `HouseObject.spawn` ported; `QuestEngine::onHouseItemUseEvent` ported (`QuestEngine.cpp:375-381`) | P5-15 |
| 7 | **Use**: `CM_USE_HOUSE_OBJECT(objId)` → `PlaceableObjectController.onDialogRequest` — **`isInTalkRange(player, houseObject)` = the object's `talking_distance` + 1 + the player's 0.25** (a house object's template has radius 0), else `STR_MSG_HOUSING_OBJECT_TOO_FAR_TO_USE` (PlaceableObjectController.java:23-29; PositionUtil.java:311-314); for the cake 2.0 → **3.25 m** → e.g. `UseableItemObject.onUse`: the owner check when the template says `owner="true"` (the cake says `false`), cooldown, use count, **COOKING placement limit (the player may not already hold the reward)**, required item, full cube, occupant → `STR_MSG_HOUSING_OBJECT_USE`, `SM_USE_OBJECT(player, obj, delay, 8)`, a task `HOUSE_OBJECT_USE` after `delay` → `SM_USE_OBJECT(…, 0, 9)`, `ItemService.addItem(reward)`, `SM_OBJECT_USE_UPDATE`, cooldown; `CM_RELEASE_OBJECT(objId)` cancels (`SM_USE_OBJECT(…, 0, 9)`, `STR_MSG_HOUSING_OBJECT_CANCEL_USE`) | UseableItemObject.java:57-190; CM_USE_HOUSE_OBJECT.java; CM_RELEASE_OBJECT.java | **no file** ×2; `UseableItemObject::onUse` ported except `placementLimitOf` (U, `UseableItemObject.cpp:49-53`, called at `:135`); A-02 | P5-16, P4-11a |
| 8 | **Configure**: butler (810021: `func_dialogs` 88 config, 86 kick, 98 script) → `ButlerAI.onDialogSelect` → `SM_DIALOG_WINDOW(page)`; `CM_HOUSE_SETTINGS(door, showOwner, notice)` → `SM_HOUSE_ACQUIRE`, `updateAppearance`, door messages and `kickVisitors`; `CM_HOUSE_SCRIPT(address, id, size, compressed, bytes)` → audit guard (`getActiveHouse()` null or another address: `AuditLogger` and return) → `PlayerScripts.set` (decompress and validate, then **`HouseScriptsDAO.storeScript` at once**) / `remove` → **`PacketSendUtility.broadcastPacket(player, SM_HOUSE_SCRIPTS)` — the two-argument overload, which sends only to the players who know the sender, never to the sender** (CM_HOUSE_SCRIPT.java:52-62; PacketSendUtility.java:98-100; PlayerScripts.java:44-57); `CM_HOUSE_KICK(1 \| 2)` → `kickVisitors`. The owner sees his scripts only through **`ButlerAI.handleCreatureSee` → `House.sendScripts` → `PlayerScripts.sendToPlayer`**: all 8 slots (`SCRIPT_LIMIT`), empty ones as a bare id, to every player the butler sees — on every studio entry (ButlerAI.java:34-38; House.java:403-407; PlayerScripts.java:102-108) | ButlerAI.java:23-38; CM_HOUSE_SETTINGS.java; CM_HOUSE_SCRIPT.java; CM_HOUSE_KICK.java | **no file** ×4 | P5-05, P5-15 |
| 9 | **Leave**: 830228 / 830229 *studio exit* inside the instance (ai `studioportal`, `spawns/Instances/720010000_Studio.xml`; **`talk_info distance="5" delay="2"`, bound radius front 5** → talk range **11.25 m**, `npc_templates.xml:533622-533630`). **The studio's arrival point (address 2001, `houses.xml:619`) is 12.2 m from 830229 and 14.4 m from 830228 — out of range: the owner must step toward an exit first.** The same 2-s use bar as row 4 → the leaving arm → the main Oriel instance at the exit point (2573, 1961, 185), 10.7 m from 730517 (so re-entering needs another step). The empty instance is destroyed by the `EmptyInstanceCheckerTask` at its next 60-s run (A-15) → `destroyInstance` → `HouseController.onDespawn`: a reusable studio is `save()`d, `clearSpawns()`, `setPosition(null)` — **the java-hook `cycles.toml:167` names for `House.spawns`**. **`HouseObject.onDespawn` is empty** (HouseObject.java:298-300): each registry object keeps its `WorldPosition` in the destroyed `WorldMapInstance` until the owner's next entry replaces it (`spawn()`, :270-275) (§9 risk 3) | StudioPortalAI.java:40-48; HouseController.java:91-100; InstanceService.java:82-107, 177-196 | A1 / A-15 | A1, M5f |
| 10 | Persistence: `PlayerService.storePlayer` → `house.save()` for each house (registry → `PlayerRegisteredItemsDAO.store`), `HouseObjectCooldownsDAO.storeHouseObjectCooldowns`; enter world registers house objects with `ExpireTimerTask`, and Energy of Repose +5 % inside a studio. Nothing else writes a studio's registry between an edit and the next `onDespawn` or logout (no periodic player save: PeriodicSaveService.java:33 holds only the legion-warehouse and run-time tasks) | PlayerService.java:89-95; PlayerEnterWorldService.java:361-366, 396-402 | ported; A-21 | P5-00 |
| 11 | **Log in inside a studio**: `InstanceService.onPlayerLogin` with `world_owner` = the owner → `getOrCreatePersonalInstance(720010000, owner)` — the still-live instance, or a new one whose `spawnInstance` runs `spawnHouses` → the player is placed in it (instance id ≠ 1); `moveToExitPoint` only when no instance comes back or it is full (InstanceService.java:148-155; `Player.java:1600` records `world_owner` on every position change) | as left | `onPlayerLogin` ported (`InstanceService.cpp:137-148`), its callee the partial `:133` — A-15 | M5f |

### 2.8 Status by area (measured at HEAD)

| Area | Chunk | `AION_UNPORTED` | `AION_PARTIAL` | Undeclared / no file | Java lines |
|---|---|---|---|---|---|
| `LegionService` (+ inner `LegionRestrictions` 18) | P5-11 | **63** | 0 | 7 anonymous-class bodies | 1,164 |
| `HousingBidService` | P5-11 | 7 | 0 | 0 | 405 |
| `LegionDominionService`, `LegionDominionLocation` | P5-11 | 6 + 6 (**5 of them are the weekly calculation M5h takes, S-08**: `startWeeklyCalculation`, `updateLegionOccupation`, `getLegionRanking`, `getRewards`, `reset` — `LegionDominionService.cpp:60-66`, `LegionDominionLocation.cpp:61-67, 85-86`) | 0 | `LegionDominionIntruderUpdateTask`: **no C++ file**, 3 bodies | 194 + 132 + 75 |
| `Town` | P5-11 | 2 (`increaseLevel` needs header request economy-legion-1, P5-11.md) | 0 | 0 | 169 |
| housing cron tasks | P5-11 | 2 (`autoFillAuction`, `putHouseToAuction`) | **3** (`executeTask` ×3) | 0 | 290 |
| `HousingService`, `House`, `HouseRegistry`, `HouseBids`, `PlayerScript`, `TownService`, `LegionDominionParticipantInfo` | P5-11 | **0** in `.cpp` (+1 dead: the generic `HouseRegistry::discard` template at `HouseRegistry.h:78-81`, which no caller instantiates — `HouseRegistry.cpp:63-64, 140-141` write it out per map) | 0 | 0 | 1,281 (1,310 with `HouseDoorState`) |
| **P5-11 total** | | **86** (87 by grep, with the dead template) | **3** | 3 (+ 7 anonymous) | 3,739 |
| `Legion` 27, `LegionWarehouse` 28, `LegionEmblem` 6, `LegionMember` 5 | **P5-10** (P5-10f, A-18) | **66** | 0 | **5** enum-companion bodies (m5g-plan.md:236 counts 7 for the same classes; §2.9) | 974 (959 without the 15-line `LegionHistoryEntry` record) |
| `ChallengeTaskService` 8, `ChallengeTask` 7, `ChallengeQuest` 5 | P5-10 | 20 | 0 | 0 | 418 |
| house objects (`HouseObject`, `UseableItemObject`, 10 more), `HouseDecoration`, `SummonedHouseNpc` (14 classes) | P4-11a | **2** (`HouseObject.cpp:166-171` — `getPlacementLimit(bool)`, **which has no Java caller**: dead; `UseableItemObject.cpp:49-53`, reached from `onUse` at `:135`) | 0 | 0 | 941 |
| `HouseController`, `PlaceableObjectController` | P4-11b | 0 | 0 | 0 | 222 |
| `PlayerScripts`, `HouseOwnerState` | P4-12 | 0 | 0 | 0 | 130 |
| `LegionStorageProxy` | P4-13 | 0 | 0 | 0 | 218 |
| `HousesDAO`, `HouseBidsDAO`, `HouseScriptsDAO`, `HouseObjectCooldownsDAO`, `PlayerRegisteredItemsDAO`, `TownDAO`, `LegionDAO`, `LegionMemberDAO`, `LegionDominionDAO` | P4-14 | 0 | 0 | 0 | 1,454 |
| housing templates and data holders (35 classes) | P4-07b, P4-09 | 0 | 0 | 0 | 1,662 |
| `HouseObjectFactory` 2, `WarehouseService` 5; `SummonHouseObjectAction`/`DecorateAction` shells | P5-07 | 7 | 0 | 4 (A-05) | 309 |
| the 30 `SM_LEGION_*` / house server packets | P4-16, P4-17 | 1 (`SM_HOUSE_BIDS.cpp:24-27`, a stand-in whose target `AuctionEndTask::getRemainingAuctionSeconds` is ported at `AuctionEndTask.cpp:84`) | 0 | 0 | 1,306 |
| the 23 legion and housing client packets | P5-15, P5-16 | – | – | **23 packets with no file** | 1,418 |
| house-npc AIs `butler`, `housesign` / `studioportal`, `friendportal`, `housegate` | P5-05 / **A1** | – | – | **no file** ×5 | 69 / 216 |
| studio quests 18802, 18832 / 28802, 28832 | **Q05** / **Q09** (`chunks.py owner`) | – | – | **no file** ×4 | 138 + 130 |

**Where M5a left the housing half** (docs/deviations/P5-11.md, "Wave 5a"): the economy-legion lane ported all of `HousingService`, `House`,
`HouseRegistry`, `HouseBids` and `TownService` because enter world, `CM_LEVEL_READY` and startup reach them (`HousingService` is constructed before
the spawns, GameServer.java:118, and spawns the 1,030 non-studio houses of six maps at every start). It left `LegionService` at its load subset
and the three housing cron tasks at `AION_PARTIAL`.

### 2.9 The bodies no site count shows (lesson 1)

Java methods (with overrides, nested and inner classes) compared with the C++ declarations, class by class, for every class on the path:

| Class | Bodies | Why no site | Owner |
|---|---|---|---|
| `LegionPermissionsMask.can`, `LegionRank.getRankId`, `LegionHistoryAction.getId`/`getType`, `LegionEmblemType.getValue` | **5** | generated enums whose constructor data and methods belong in a hand-written companion (`generated/…/team/legion/LegionRank.h`: "Constructor data and methods are hand-written free functions in a companion header"), **and none of the four companions exists**. Three stand-ins live in `network/aion/serverpackets/detail/PacketSupport.h:44-58` (P4-17) and one in `dao/LegionDAO.cpp:57-75` (P4-14, whose comment asks P5-10 for the companion) | P5-10 (+ P4-14, P4-17 leases to delete the copies) |
| `LegionService$1`…`$7`: the anonymous `RequestResponseHandler`s of `requestDisbandLegion` (accept), `invitePlayerToLegion` (accept, deny), `startBrigadeGeneralChangeProcess` (accept), `appointBrigadeGeneral` (accept, deny), `recreateLegion` (accept) | **7** in 6 classes | hub-headers.md §9.3: anonymous classes are callback structs **defined in the `.cpp`**, never declared; `cycles.toml:259` already resolves `LegionService$2#legion` ("ResponseRequester.respond / denyAll drop the pending request") | P5-11 |
| **`DecorateAction.getTemplateId`** (`partId == null ? 0 : partId`, DecorateAction.java:28-32) | **1** | **missed by rev 1.** `DecorateAction.h` has an empty body; its generated member block (`DecorateAction.xml.inc`) holds a private `std::optional<int32_t> partId` and no accessor — unlike `SummonHouseObjectAction`, whose generated block has `getTemplateId()` (`SummonHouseObjectAction.xml.inc:13`). M5b-3's h01/h02 add `canAct`/`act` and the `ItemActions` lookups, not this. `CM_HOUSE_EDIT` action 3 calls it for every decoration (CM_HOUSE_EDIT.java:88-91), so the wallpaper step does not compile without it | P5-07 (additive header request, §8) |
| the 15 first-cut client packets | **46** | no file (3 bodies each; `CM_HOUSE_EDIT` 4) | P5-15, P5-16 |
| `ButlerAI` 4, `HouseSignAI` 2, `StudioPortalAI` 3, `_18802` 3, `_18832` 3, `_28802` 3, `_28832` 3 | **21** | no file | P5-05, A1, Q05, Q09 |
| `ActionItemNpcAI` (ctor, `handleDialogStart`, `handleUseItemStart`, **the anonymous `ItemUseObserver.abort`**, `handleUseItemFinish`, `getTalkDelayInMs`, `handleDied`) | 7 | no file; **counted only if A-11 is missing** (M5d's) | P5-05 |
| `SummonHouseObjectAction`, `DecorateAction` `canAct`/`act` | 4 | declared by M5b-3's h01 as stubs (A-05); **their Java bodies are `return false` and empty** (SummonHouseObjectAction.java, DecorateAction.java:16-26) | P5-07 |
| `LegionDominionIntruderUpdateTask` (ctor, `run`, `getInstance`) | 3 | no C++ file (`taskmanager/tasks/fwd.h:8` declares the name only) | P5-11 → M5i |
| **False positives removed by hand** | 0 | `LegionWarehouse.delete`, `LegionStorageProxy.delete`, `LegionDominionDAO.delete` are `delete_` in C++; `AuctionEndTask.ProlongedAuction.prolong` is defined in `AuctionEndTask.cpp:18-60` | – |
| **All other classes of §2.8** | 0 | every Java method has a C++ declaration | – |

**80 bodies in the first cut have no site today** (5 + 7 + 1 + 46 + 21), plus the 4 A-05 stubs — the §1 table's second column — and 7 more
if A-11 is missing. **The enum count differs from M5g's**: m5g-plan.md:236 counts 7 undeclared bodies for the legion model; this plan counts
the 5 *methods* of the four enums (`can`, `getRankId`, `getId`, `getType`, `getValue`) — the enums' constructors only carry data, which the
companions hold as tables, and the generated enums already exist (`generated/…/team/legion/LegionRank.h` etc.). Neither count changes L-05's
effort (S); the lane re-counts at branch time.

### 2.10 What wiring M5h wakes (lesson 2)

Every entry point the milestone turns on, traced to the first unported or partial body:

| Entry point | Reaches | First unported/partial body | Closed by |
|---|---|---|---|
| **the character list** (`SM_CHARACTER_LIST`, `SM_CREATE_CHARACTER`) of an account with a legion member — **the first entry point**, before enter world | §2.5 row 0: `AbstractPlayerInfoPacket.cpp:33` → `PacketLookups.cpp:58-61` → `LegionService::getLegionMember(pcd)` | `Legion::Legion` (`Legion.cpp:40-46`), `LegionMember::setPlayerData` (`LegionMember.cpp:25-31`) — **inside the packet's `writeImpl`**, with the database load under the `legionMemberById` stripe monitor (§9 risk 4) | L-01, L-02 |
| **`CM_DELETE_CHARACTER`** of a legion member | `CM_DELETE_CHARACTER.cpp:34` → `getLegionMember(pcd)` | as the row above | L-01, L-02 |
| enter world of a legion member | §2.5 row 1-2 | as above if the character list did not cache it; then `LegionService::onLogin` | L-01, L-02, S-05 |
| leave world of a legion member | §2.5 row 3 | `LegionService::onLogout`, `LegionWarehouse::unsetInUse` | S-05, L-04 |
| any experience gain of a legion member | `RatesInfo.cpp:49` | `Legion::hasBonus` | L-01 |
| any level change of a legion member | `PlayerController.cpp:712-713` | `LegionService::updateMemberInfo` | S-05 |
| **any cross-map teleport of a legion member** — the studio entrance and exit, every M5f teleporter, bind revive to another map | `TeleportService.cpp:146` (TeleportService.java:245-246, 534-535; m5f W-06) | `LegionService::updateMemberInfo` | S-05 |
| a class change of a legion member (after M5e) | `upgradePlayer` (m5e W-04) | `LegionService::updateMemberInfo` | S-05 |
| **a member looked up by name**: `CM_LEGION` 0x06 (rank), 0x0F (nickname), 0x04 (kick) | `LegionService.cpp:199-201` (ported) | **`PlayerService::getOrLoadPlayerCommonData(std::string_view)`** (`PlayerService.cpp:327-329`, **P5-00**) — even for an online member | A-23 (M5c M-02) or S-09 |
| **an uncached member** — `Legion::getMembers`/`streamMembers`/`getBrigadeGeneral` after a restart (`updateLegionMemberList` on the first member's login), `AbstractHouseInfoPacket` for an uncached legion-member owner | `LegionService.cpp:218-222` (ported) | `PlayerService::getOrLoadPlayerCommonData(int32_t)` (`PlayerService.cpp:323-325`) | A-23 or S-09 |
| any AP gain of a legion member (after M5d) | AbyssPointsService.java:49-51 | `Legion::addContributionPoints` | L-01 |
| any `SM_LEGION_*` broadcast | `PacketSendUtility.cpp:139-149` | `Legion::getOnlinePlayers` | L-01 |
| leave, kick, disband | LegionService.java:436, 737 | `ConquerorAndProtectorService::onLeaveLegion` (**P5-12b**) | S-06 |
| a lazy disband | LegionService.java:184 | `SiegeService::cleanLegionId` (**P5-12a**) | S-06 |
| legion-warehouse moves (after M5b-3) | `ItemRestrictionService` legion arms | `LegionMember::hasRights` | L-02 |
| `CM_CLOSE_DIALOG` at a warehouse npc (after M5c) | DialogService.java:60-61 | `LegionWarehouse::unsetInUse` | L-04 |
| periodic save | `PeriodicSaveService` legion task | none (the task iterates the cache; ported) | – |
| **startup** | 1,030 houses × (butler + crystal + sign) = 3,090 house npcs; `AIEngine` gives unregistered names a `DummyNpcAI` under `gameserver.dev.missing_ai_handlers = warn` (`AIEngine.cpp:155-171`) | registering `butler` and `housesign` (H-01) switches the **1,030 butlers** (and every later owned-house sign) to live AIs world-wide; `ButlerAI.handleCreatureSee` then runs `House.sendScripts` for every player a butler sees (ported). The 1,030 unowned-house signs are `useitem` (`ActionItemNpcAI`), switched on by **M5d** D5, not by M5h | H-01 (D6) |
| studio entrance and exit (use bar) | §2.7 rows 4, 9: `ActionItemNpcAI` → `DialogService::isInteractionAllowed` | no file / `DialogService.cpp:27-28` | A-11, A-06 |
| studio instance creation | §2.7 row 4-5 | `InstanceService::getOrCreateHouseInstance`; `HouseObjectFactory::createNew` for a stored object | H-06 (required: A-16 settled), H-04 |
| the studio teleports themselves | TeleportService.java:514-519 → `InstanceService::onLeaveInstance` → `GeneralInstanceHandler::onLeaveInstance` (+ `AutoGroupService::onLeaveInstance` under the Java default) | `InstanceService.cpp:170-172`, `GeneralInstanceHandler.cpp:35` | A-24 (M5f N-03, N-04, N-06) |
| studio instance destroy | the `EmptyInstanceCheckerTask` → `destroyInstance` → `HouseController.onDespawn` (ported, never run) | the checker and `destroyInstance` (M5f); then none — the risk is behavioural (§9 item 3) | A-15; gate Y18 |
| re-login inside a studio | `InstanceService.onPlayerLogin` → `getOrCreatePersonalInstance` (§2.7 row 11) / `moveToExitPoint` | the two M5a partials (`InstanceService.cpp:133, 153`) | A-15; gate Y18 |
| cron: Sunday 12:00, Monday 00:00 | `AuctionEndTask`, `AuctionAutoFillTask`, `MaintenanceTask` | the three P5-11 `AION_PARTIAL`s (unchanged by M5h) | M5h-2 |
| cron: Wednesday 09:00 | `CronJobService.cpp:185-187` → `LegionDominionService::startWeeklyCalculation` (`LegionDominionService.cpp:60-61`) → `getLegionRanking`, `updateLegionOccupation`, `getRewards`, `reset`; `SystemMailService::sendMail` only for a ranked participant | **`AION_UNPORTED` today, already scheduled since M5a** — any server that runs across a Wednesday 09:00 throws there, M5h or not. m5i-plan.md:94 marks it "M5h's … not M5i's" | **S-08** (D13); the mail arm A-25 |

**Ported and not yet executed on the paths M5h adds.** Startup already runs the unowned-house half of the housing code at every M5a start:
`HousingService` spawns the 1,030 non-studio houses (§2.8), each through `HouseController.onAfterSpawn` → `getPlayerScripts` (`HouseScriptsDAO`),
`getRegistry` (`PlayerRegisteredItemsDAO.loadRegistry`, empty) and `updateSpawns` (HouseController.java:51-89); every login and logout already runs
`HouseObjectCooldownsDAO` with empty data (PlayerService.java:95, 164). **First executed by M5h**: the 14 house-object classes (941 lines — no
house has ever held an object), `HouseObjectFactory`, `HouseRegistry`'s put/move/discard/save arms, the studio spawn arm and
`HouseController.onDespawn`, `House.save` → `HousesDAO.storeHouse`, `PlayerRegisteredItemsDAO.store` and a `loadRegistry` with rows,
`PlayerScripts.set/remove` and `HouseScriptsDAO`'s writes — roughly 1,300-1,600 Java lines (inferred, §12) — and all ~725 lines of legion
persistence (LegionDAO 370, LegionMemberDAO 137, LegionStorageProxy 218), and 27 of the 30 legion and house server packets.

### 2.11 The client packets (lesson 4)

190 Java client-packet files, 43 C++ files: 147 have no C++ file. Two of the Java files are abstract bases (`AbstractCharacterEditPacket`, which has
a C++ file, and `AbstractGmCommandPacket`), so **146 of the 188 real client packets have no C++ file** — the roadmap's "148 of 188" less
`CM_CASTSPELL` and `CM_REMOVE_ALTERED_STATE`, which M5b-2 added. Of them, M5h needs the rows below; the studio also needs M5f's
`CM_TELEPORT_ANIMATION_DONE` (A-14) and M5c's `CM_SHOW_DIALOG` / `CM_DIALOG_SELECT` / `CM_CLOSE_DIALOG` (A-06):

| Packet | Opcode | Lines | Chunk | Scope |
|---|---|---|---|---|
| `CM_LEGION` (14 arms) | 0x00F0 | 165 | P5-16 | **M5h** |
| `CM_LEGION_HISTORY` | 0x011A | 38 | P5-16 | **M5h** |
| `CM_LEGION_MODIFY_EMBLEM` | 0x011E | 45 | P5-16 | **M5h** |
| `CM_LEGION_UPLOAD_INFO` / `CM_LEGION_UPLOAD_EMBLEM` | 0x0163 / 0x0144 | 41 / 43 | P5-16 | **M5h** |
| `CM_LEGION_SEND_EMBLEM` / `CM_LEGION_SEND_EMBLEM_INFO` | 0x00F2 / 0x00D3 | 32 / 38 | P5-16 | **M5h** — a real client sends them unprompted whenever it sees a member of a custom-emblem legion |
| `CM_LEGION_WH_KINAH` | 0x02EF | 65 | P5-16 | **M5h** |
| `CM_HOUSE_EDIT` | 0x0135 | 159 | P5-15 | **M5h** |
| `CM_HOUSE_DECORATE` | 0x02EE | 60 | P5-15 | **M5h** |
| `CM_HOUSE_SETTINGS` / `CM_HOUSE_SCRIPT` / `CM_HOUSE_KICK` | 0x02EC / 0x00E1 / 0x02EB | 71 / 64 / 44 | P5-15 | **M5h** — the studio butler offers all three (88, 98, 86) |
| `CM_USE_HOUSE_OBJECT` / `CM_RELEASE_OBJECT` | 0x01A3 / 0x0184 | 42 / 46 | P5-16 | **M5h** |
| `CM_CHAT_MESSAGE_PUBLIC` | 0x00FE | 155 | P5-15 | **M5h if A-19 is missing** |
| `CM_QUESTION_RESPONSE` | 0x0115 | 46 | P5-16 | M5h only if A-07 is missing |
| `CM_GET_HOUSE_BIDS`, `CM_PLACE_BID`, `CM_REGISTER_HOUSE`, `CM_HOUSE_PAY_RENT` | 0x01BD, 0x01A0, 0x01BE, 0x01A2 | 39, 37, 70, 63 | P5-15, P5-16 | M5h-2 (auctions, rent) |
| `CM_HOUSE_OPEN_DOOR`, `CM_HOUSE_TELEPORT`, `CM_HOUSE_TELEPORT_BACK` | 0x0185, 0x01A1, 0x0122 | 63, 119, 37 | P5-15 | M5h-2 (visiting a land house, the relationship crystal, the house gate). `CM_HOUSE_OPEN_DOOR` looks up `getHouseByAddress`, which holds **no studio**, so studios never need it |
| `CM_CHALLENGE_LIST` | 0x018B | 50 | P5-15 | M5h-2 (towns and legion challenge tasks) |
| `CM_LEGION_DOMINION_REQUEST_RANKING`, `CM_ABYSS_RANKING_LEGIONS` | 0x00E0, 0x0159 | 37, 62 | P5-15, P5-16 | M5i |
| `CM_APPEARANCE` (legion rename ticket → `LegionService.tryRename`) | 0x0168 | – | P5-15 | M5j |

**Server packets: none to write.** What the gate needs is independent decoders (§6 G-02).

### 2.12 The shipped data the gate stands on (lesson 3)

| Fact | Value | Source | Consequence |
|---|---|---|---|
| creation price | 10,000 | `config/main/legions.properties` (`gameserver.legion.creationrequiredkinah`) | A's exact seed (Y1, Y2) |
| members for level 2 | **6, not LegionConfig.java's default 10** | `legions.properties` sets `level2requiredmembers … level8requiredmembers = 6` | the oracle must read the properties files the server loads, not the Java defaults |
| emblem base price | 800,000, through `PricesService.getPriceForService` | `legions.properties`, PricesService.java | A-09 |
| default permissions | 0x1E0C, 0x1C08, 0x1800, 0x800 | Legion.java:29-32 = `sql/aion_gs.sql` defaults 7692/7176/6144/2048 | Y1, Y5 |
| legion npcs | 203806 *Losadis* (1917.46, 1388.3, 590.365), 203807 *Inofe* 9.5 m away, 203751 *Pauton* (1319.2, 1405.43, 575.27) — **598.7 m** from Losadis | `spawns/Npcs/110010000_Sanctum.xml` | two gate spots, reached by seeding (D8) |
| studio npcs | 830069 *Parrine* (2563.03, 1965.39, 182.75), 730517 *studio entrance* (2583.113, 1963.681, 182.875) **20.2 m** away, nearest house **231 m** away (no `SM_HOUSE_RENDER` flood at the gate spot) | `spawns/Npcs/700010000_Oriel.xml:2029-2031, 3134-3136`, `housing/houses.xml` | two gate spots: the Parrine spot and the entrance spot, **no spot serves both** (row below) |
| talk ranges (distance + 1 + both bound radii; a player's radius 0.25) | Parrine: `distance 5`, radius 0.35 → **6.6 m**; 730517: `distance 5 delay 2`, radius 0.35 → **6.6 m**, a **2-s use bar**; 830228/830229: `distance 5 delay 2`, radius 5 → **11.25 m**, 2-s bar; the cake: `talking_distance 2.0`, radius 0 → **3.25 m** | `npc_templates.xml:454686, 531501-531512, 533622-533630`; PositionUtil.java:243-250, 306-314; PlayerAccountData.java:99 | the oracle emits a spot inside each range; the fake client **moves** between Parrine's and the entrance's spot, and from the studio's arrival point to an exit spot (C15, C19) |
| studio exits | 830228 (357.50, 284.33, 222.29), 830229 (354.44, 298.92, 222.29); the studio arrival point (366.24, 295.78, 222.35) is **12.2 m** and **14.4 m** from them; the Oriel exit point (2573, 1961, 185) is **10.7 m** from 730517 | `spawns/Instances/720010000_Studio.xml:5-11`; `houses.xml:619` | Y18: both portal steps need a move first |
| furniture | 1,520 items with `<houseobject>`: 840 passive, 309 `use_item`, 239 chair, 86 storage, 28 npc, 4 emblem, 3 postbox, 2 picture, 2 jukebox, **7 whose object id has no template** (e.g. 170195145 → 3195143) — Java's `createNew` throws `NullPointerException` on them | `items/item_templates.xml`, `housing/housing_objects.xml` | the oracle picks gate items from the other 1,513; the 7 are a unit case (H-07) |
| a studio decoration | 261 `<housedeco>` items; **103** whose part carries `CP_D` (74 `INWALL_ANY`, 29 `INFLOOR_ANY`), e.g. 171110000 *Superb Plain Wallpaper* → part 3554000; **one bare `<housedeco/>` with no id**: 170000023 *Octagonal Board Roof* (`item_templates.xml:861912`), for which `DecorateAction.getTemplateId` returns 0 | `housing/house_parts.xml` (`building_tags` is space-separated) | Y15; the bare item is an H-07 unit case |
| a deterministic useable object | 170190034 *[Event] Solorius Cake* → 3190034: `use_count` 20, `delay` 3,000 ms, `cd` 10 s, `limit` COOKING (one per studio), reward 160010196, final reward 188051654, no required item, **`owner="false"` (anyone may use it), `use_days="30"` (so `HouseObjectFactory` sets an expiry and `SM_HOUSE_EDIT`/`SM_HOUSE_OBJECT` carry `secondsUntilExpiration` ≈ 2,592,000 − elapsed, not 0), `talking_distance="2.0"`** | `housing_objects.xml:899-901` | Y15, Y16; the oracle emits the expiry window |
| a plain chair | 170120000 *Ruko Fiber Bed* → 3120000, no `use_days` (never expires) | same | Y15 |
| a legion-warehouse item | must pass `isStorableInLegWarehouse`: item mask `STORABLE_IN_LWH` and not soul-bound (ItemRestrictionService.java:52; Item.java:635-637) | `item_templates.xml` `mask` | the oracle picks the C9 and D8 warehouse items by mask (G-01) |

---

## 3. Scope: the first cut, M5h-2, and what goes elsewhere

| Feature | Where | Why |
|---|---|---|
| Legion create, invite, leave, kick, ranks, brigade general, permissions, nickname, self intro, level-up checks, announcement and `/gnotice`, chat, history, warehouse (items and kinah), stock and custom emblem, online bonus, disband and recreate, member login/logout/level updates | **M5h** | the whole of `LegionService` but four bodies; every step is reachable by one player plus a second client, and none needs time control |
| Studios: acquire (the quest chain 18832 → 18801 → 18802, or the fee), enter, leave, decorate (objects and parts), use objects, settings, scripts, kick, persistence, re-login inside | **M5h** | the first housing a player meets, free at level 21 once the chain is reachable (D4); it runs every piece of house machinery M5a ported and nobody ran |
| **Land houses**: auction registration, bidding, auction end (Sunday 12:00 cron, 5-to-30-minute prolongation), automatic auction fill (Monday 00:00), rent and impoundment (Monday 00:00), house mails | **M5h-2** | cron- and mail-driven; a gate needs cron expressions moved into the run window and money flows across four accounts; `canOwnHouse` needs quest 18802 COMPLETE; 7 + 5 + 2 bodies plus 4 packets |
| Visiting: relationship crystal (`friendportal`, `CM_HOUSE_TELEPORT`), house gate (`housegate`, `SummonHouseGateEffect`, `CM_HOUSE_TELEPORT_BACK`), knocking (`CM_HOUSE_OPEN_DOOR`) | **M5h-2** | needs a second house owner and land houses for two of three paths |
| Towns: challenge tasks, town points and levels, town npc re-spawns (`Town.increaseLevel` + header request economy-legion-1, `ChallengeTaskService`, `CM_CHALLENGE_LIST`); legion challenge tasks at level ≥ 5 | **M5h-2** | a town levels only through `CHALLENGE_TASK` quests, of which m5d-plan.md §2.4 counts **0 reachable** after M5d; the legion half is reachable only at legion level 5 |
| **The Legion Dominion weekly calculation** (`startWeeklyCalculation`, `updateLegionOccupation`, `LegionDominionLocation::getLegionRanking`, `getRewards`, `reset`) | **M5h** (S-08, D13) | the Wednesday 09:00 cron has called it since M5a (`CronJobService.cpp:185-187`); it is P5-11, the legion-service lane's chunk; it touches only the legion model M5h ports and ported DAOs/packets; with no participants (none can exist before M5i ports `join`) it resets and stores each location — and m5i-plan.md:94 assigns it to M5h |
| Legion Dominion (Stonespear Reach), the rest: `join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`, `LegionDominionLocation::join`/`store`/`updateRanking`, the intruder task, `LegionDominionPortalAI`, `joinLegionDominion`, the ranking packet | **M5i** | a weekly instance competition with invasion rifts (`openInvasionRift`) and the conqueror/protector system — world-event machinery, and its instance handler is phase 6 |
| Legion abyss ranking (`CM_ABYSS_RANKING_LEGIONS`, `AbyssRankingCache`) | **M5i** | PvP ranking |
| Legion rename (`tryRename` via `CM_APPEARANCE`) | body in **M5h** (W), packet in **M5j** | the packet is a character-service packet |

**M5h-2 runs straight after M5h**, before M5i: it depends on nothing M5i brings (mail is M5c's, crons are ported, the quest engine is M5d's), so
the roadmap row "Legions, houses, towns" is still delivered in full at row 8 — as two milestones, the way M5b became three. §6 lists M5h-2's
work items so it can branch without a new analysis; it still gets its own plan review.

---

## 4. Where this plan disagrees with the roadmap

| phase5-roadmap.md | This plan | Why |
|---|---|---|
| "P5-11 legion and housing **86**" | 86 is exact (87 by grep, one dead template body), but the first cut is **~224 bodies**, of which P5-11 is 61 + 7 + 5 (S-08) | the legion model is P5-10 (66 + 5); 15 client packets and 7 handlers have no file; 19 bodies sit in six other chunks under file leases — A1 3, Q05 6, Q09 6, P5-12b 2, P5-12a 1, P5-13 1 (§2.9, §6) |
| "M5h … chunks, mainly P5-11" | **thirteen chunks with bodies**: owned P5-10f (A-18), P5-11, P5-15, P5-16, P5-05, P5-07, P4-11a; leased A1, Q05, Q09, P5-12a, P5-12b, P5-13 — plus stand-in leases on P4-14, P4-16, P4-17, a test lease on P4-11b, and P5-00 if A-23 is missing | §2 |
| "Legions, houses, towns" in one milestone | **M5h (legions + studios) and M5h-2 (land houses, visiting, towns)**, back to back | §3 |
| (implicit) Legion Dominion is P5-11, so M5h | **M5i**, except the weekly calculation the cron already reaches (M5h, S-08) | §3 |
| "Client packets with no C++ file: 148 of 188" | **146 of 188** real packets today (147 of 190 files counting the two abstract bases; M5b-2 added `CM_CASTSPELL` and `CM_REMOVE_ALTERED_STATE`) | measured |

---

## 5. Decisions

| # | Decision | Why |
|---|---|---|
| **D1** | **The first cut is §3's M5h rows; land houses, visiting and towns are M5h-2, run immediately after.** Taken by the integrator under the standing instruction and named in the next progress update, because the roadmap row names houses and towns: if the user wants them in one milestone, M5h-2's items (§6) become M5h stage 3 without re-planning. | §3. The first cut has a green point with no time control; M5h-2's gate needs cron manipulation and four money flows. |
| **D2** | **Legion Dominion — all of it but the weekly calculation (D13) — and legion abyss ranking move to M5i; legion rename's packet to M5j.** Their bodies stay `AION_UNPORTED` and fail loudly (`LegionService::joinLegionDominion` at `LegionService.cpp:437`, `LegionDominionService`'s `join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`, `LegionDominionLocation`'s `join`, `store`, `updateRanking`, the intruder task). Taken by the integrator; named in the progress update. | §3. |
| **D3** | **The legion model is M5g's P5-10f** (m5g-plan.md D1: `model/team/legion/**`, `model/challenge/**`, `services/ChallengeTaskService`, test directory `tests/team/P5-10f` by the shared-target rule of `chunks.cmake:15-16`). **Only if M5g did not split P5-10** does M5h's integrator make exactly M5g's six-part split on day 0 (I-01) — never a two-part split and never another letter: rev 1's "P5-10b = legion, `tests/legion`" collided with M5g's P5-10b = parties. | One chunk name, one set of files. P5-10 is 294 sites; M5g works in 208 of them and M5h in 86. The pattern is m5b2-plan.md D1's P5-02a/P5-02b. |
| **D4** | **M5h pulls six phase-6 handlers forward through file leases**: `StudioPortalAI` (A1), the studio quests `_18802`, `_28802` **and their prerequisites `_18832`, `_28832`** (Q05: `_18802`, `_18832`; Q09: `_28802`, `_28832` — `chunks.py owner`). The middle quests 18801/28801 are XML `report_to` quests M5d's engine runs (A-10). The other housing tutorial quests (`_18821`, `_18828`, `_18830`, `_28821`, `_28828`, `_28830`) stay phase 6. | Without `studioportal` nobody enters a studio. **Without 18832/28832 nobody can start 18801/28801, hence not 18802/28802** (§2.7 row 1) — rev 1 pulled only 18802/28802 and claimed a real player got the free studio at level 21, which does not hold; and the paid button needs 18802 COMPLETE in the client (§2.7 row 2), so without the chain a real player gets no studio at all. The two extra handlers are 65 lines each (`sendQuestDialog`, `playQuestMovie(801)`, `changeQuestStep`). Precedent: m5d-plan.md D5 (an A1 lease). 331 Java lines. |
| **D5** | **Both acquisition paths are ported and gated**: the quest path (Y12, for A) and the paid path (Y13, for C). | They share `createStudio` but differ in the fee and in the ownership refusal, and a fake client can exercise both deterministically. |
| **D6** | **Registering `butler` and `housesign` switches the 1,030 butlers spawned at every startup (and all owned-house signs) from `DummyNpcAI` to live AIs.** Taken by the integrator; named in the progress update. `friendportal` stays unregistered until M5h-2, so relationship crystals keep a `DummyNpcAI` (the real-client checklist says so). | §2.10 startup row. `ButlerAI` extends `GeneralNpcAI`; a butler is a real npc AI after this, in six maps. |
| **D7** | **Faithful to Java where Java is odd**: no npc-distance check on `CM_LEGION(0x00)`; **`CM_LEGION_WH_KINAH` needs no open warehouse, no in-use slot and no npc** (CM_LEGION_WH_KINAH.java:34-63); the level-too-low emblem refusal sends nothing; the disband is lazy; a leave is recorded as a `KICK` history entry (LegionService.java:100); `SM_LEGION_EDIT(0x05)` carries the truncated announcement; `sendEmblemData` splits at 7,993; `SM_HOUSE_EDIT` reads the registry when it is written, not when it is built (SM_HOUSE_EDIT.java); `CM_HOUSE_SCRIPT`'s `SM_HOUSE_SCRIPTS` goes to the others, not the sender (CM_HOUSE_SCRIPT.java:62). | m5b2-plan.md D9's rule. Each is a gate or unit assertion, so a "fix" shows. |
| **D8** | **The gate seeds what M5h does not own and one thing it does**: positions, exact kinah, a quest state, furniture items (A-08's convention) — **and a pre-existing level-3 legion** (`legions`, `legion_members`, `legion_announcement_list`, `legion_history`, one legion-storable `inventory` row owned by the legion; ids above `gameserver.idfactory.wrap_at`), written after C is created and before C's account asks for the character list again. | The load path (§1 finding 1) runs in normal play only after a restart. A seeded legion is loaded the first time anything asks for its member — **the character list** (`SM_CHARACTER_LIST` → `writePlayerInfo`), before enter world — because nothing has cached it: `SM_CREATE_CHARACTER`'s lookup found no row, and a null `computeIfAbsent` result is not stored (Java's rule; the port's `ConcurrentHashMap.h` "null/empty removes"). It is also the only way to reach the level-2 and level-3 emblem gates without six members. |
| **D9** | **Everything outside the first cut stays `AION_UNPORTED` and throws** — no blanket `AION_PARTIAL`. `ChallengeTaskService::canRaiseLegionLevel` is reached only at legion level ≥ 5 with `gameserver.legion.task.requirement.enable` (LegionService.java:929-934), so it stays unported until M5h-2. | m5b2-plan.md D6's rule: a silently skipped legion or housing action is a wrong game. |
| **D10** | **The gate profile adds `gameserver.legion.disbandtime = 5`** to the latest gate profile at branch time (which already disables siege and the conqueror/protector system, m5c §10.1). | Y21 watches the lazy disband complete; 86,400 s cannot be watched. |
| **D11** | **The three housing cron partials stay**, listed in the M5h allow-list's §C (timing rows: hit only if a run crosses Sunday 12:00 or Monday 00:00). | M5h-2 closes them. |
| **D12** | **`CM_CHAT_MESSAGE_PUBLIC` is M5g's** (m5g D14/K-04: group chat needs it); M5h ports it only if A-19 is missing (P-03). | One owner for one packet. |
| **D13** | **M5h owns the Legion Dominion weekly calculation (S-08)**: `LegionDominionService::startWeeklyCalculation`, `updateLegionOccupation`, `LegionDominionLocation::getLegionRanking`, `getRewards`, `reset` — 5 bodies, ~100 Java lines, all P5-11. The reward arm calls `SystemMailService::sendMail` (A-25) and is unreachable until M5i ports `join`. Taken by the integrator; recorded here and consistent with m5i-plan.md:94 ("M5h's"). Until M5h lands, any run crossing Wednesday 09:00 still throws there. | The cron has been armed since M5a (`CronJobService.cpp:185-187`); rev 1 sent the body to M5i while M5i's plan sent it to M5h, so nobody owned it. It is in M5h's chunk and touches the legion model M5h ports. An `AION_PARTIAL` (rev 1's §13 item 6) would hide a small body instead of porting it. |
| **D14** | **Stage 1 runs in two parts with a commit between them**; the legion-service lane is the critical path at 9-12 agent-days (§7). | Rev 1 called the first cut "M5b-1's stage 1 size, one wave"; at ~224 bodies over five lanes it is 1.5× that, and the two legion lanes hold ~2.8× M5b-1's per-lane load (M5b-1: ~150 sites over six lanes, ~25 per lane, m5b2-plan.md:614-615). `LegionService.cpp` is one file of one chunk and cannot be shared. The commit point is M5b-2 stage 1's precedent (three parts). |

---

## 6. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional. The last
column names the §0 assumptions an item stands on.

### Integrator (stage 0, day 0)

| Id | What | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|
| **I-01** | **Only if M5g did not make it** (A-18): M5g's D1 manifest split of P5-10 into P5-10a..f exactly as m5g-plan.md D1 names them, with `tests/team/P5-10x` directories; M5h then owns P5-10f | – | R | S | A-18 |
| **I-02** | File leases: **A1** (`handlers/ai/portals/StudioPortalAI.*`); **Q05** (`handlers/quest/oriel/_18802*`, `_18832*`); **Q09** (`handlers/quest/pernon/_28802*`, `_28832*`); **P5-12a** (`SiegeService.cpp` `cleanLegionId`); **P5-12b** (`ConquerorAndProtectorService.cpp` `onLeaveLegion`, `resetLegionDominionRank`); **P5-13** (`InstanceService.cpp` `getOrCreateHouseInstance` — **required**, A-16 settled); **P4-14** (`LegionDAO.cpp:57-75, 338`); **P4-17** (`PacketSupport.h:44-58`, the 11 P4-17 packet `.cpp`s of L-05, `tests/sm_lz/OpcodesAndSupportTest.cpp:156-172`); **P4-16** (`SM_GM_SHOW_LEGION_MEMBERLIST.cpp:24` — or that one site keeps a local stand-in, L-05); **P5-00** (`PlayerService.cpp:323-329`, only if A-23 is missing, S-09); P5-08/P5-13 for P-03 if needed (P5-13 then has two lessees in stage 1 — `InstanceService.cpp` and `PlayerRestrictions.cpp`, different files). **Within P5-11** the stage-1 file split of §7: the studio lane holds `model/house/**` and `services/HousingService.*`, the legion-service lane the rest | – | R | S | A-19, A-23 |
| **I-03** | **Branch-time check of §0**: grep each A-row's site; for each missing one, activate the named fallback item and record it in the wave report | – | R | S | all |
| **I-04** | `game-server/config/m5h.properties.example` in the Java tree (D10) | – | R | S | – |

### Stage 1 part 1, legion model (P5-10f)

| Id | What | Java refs | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|---|
| **L-01** | `Legion` — 27 sites (`Legion.cpp:40-160`): the constructor's `setHistory(emptyMap)`, `setMemberIds`, `getMembers`/`streamMembers` (through `LegionService.getLegionMember`), `getOnlinePlayers` (`World.getPlayer`), `getBrigadeGeneral` (`orElseThrow`), `addLegionMember`/`removeMember`/`canAddMember`, `setLegionPermissions`, `setLegionLevel` (→ warehouse `updateLimit`), `addContributionPoints`, `hasRequiredMembers`/`getKinahPrice`/`getContributionPrice` over the 7-level config tables, `setAnnouncement`, `isDisbanding`, `isMember`, `setLegionEmblem`, `getWarehouseExpansions`, `getHistory`/`addHistory`/`setHistory` (per-type lists under their monitors, the 365-day trim for REWARD and WAREHOUSE), `addBonus`/`removeBonus`/`hasBonus` (≥ 10 online, compare-and-set, `SM_ICON_INFO`), `toString` | Legion.java:43-398 | – | R | M | A-12 |
| **L-02** | `LegionMember` — 5 (`isBrigadeGeneral`, `increaseChallengeScore`, `setPlayerData` ×2 at `LegionMember.cpp:25-31`, `hasRights`) | LegionMember.java | L-05 | R | S | – |
| **L-03** | `LegionEmblem` — 6 (`setCustomEmblemData`, `setEmblem`, the upload accumulator trio, `setPersistentState`) | LegionEmblem.java | – | R | S | – |
| **L-04** | `LegionWarehouse` — 28 (22 are the "behind proxy" throws; ctor, `increaseKinah` through a new `LegionStorageProxy`, `setInUse`/`unsetInUse`/`getCurrentUser` as compare-and-set, `setLimit` throw, `updateLimit`) | LegionWarehouse.java:20-168 | – | R | S | – |
| **L-05** | The four enum companions (5 bodies): `LegionPermissionsMaskInfo.h` (`can`), `LegionRankInfo.h` (`getRankId`), `LegionHistoryActionInfo.h` (`getId`, `getType`), `LegionEmblemTypeInfo.h` (`getValue`) — new files; then delete the stand-ins in `LegionDAO.cpp:57-75` and `PacketSupport.h:44-58` (leases, I-02) and point their call sites at the companions: **12 packet sites in 12 packets** — `legionEmblemTypeValue` at `AbstractHouseInfoPacket.cpp:62`, `SM_LEGION_DOMINION_LOC_INFO.cpp:28`, `SM_LEGION_SEND_EMBLEM.cpp:13`, `SM_LEGION_UPDATE_EMBLEM.cpp:16`, `SM_PLAYER_INFO.cpp:86`, `SM_SIEGE_LOCATION_INFO.cpp:55`; `legionRankId` at `SM_GM_SHOW_LEGION_MEMBERLIST.cpp:24` (**P4-16**), `SM_LEGION_ADD_MEMBER.cpp:24`, `SM_LEGION_MEMBERLIST.cpp:39`, `SM_LEGION_UPDATE_MEMBER.cpp:28`, `SM_LEGION_UPDATE_TITLE.cpp:18`; `legionHistoryActionId` at `SM_LEGION_HISTORY.cpp:33` — **plus `LegionDAO.cpp:338`** (`getType`) and **the stand-ins' own test**, `tests/sm_lz/OpcodesAndSupportTest.cpp:156-172`, which moves to test the companions. If the P4-16 lease is refused, `SM_GM_SHOW_LEGION_MEMBERLIST.cpp` keeps a local copy of `legionRankId` and the row stays in header-requests.md | the four enums | – | R | S | – |
| **L-06** | Tests in `tests/team/P5-10f`: `hasRights` over 5 ranks × 7 masks with the default permissions; `canAddMember` and `hasRequiredMembers` at the **shipped** config (6) and at the defaults (10); `getKinahPrice`/`getContributionPrice` per level; history prepend order and the 365-day trim only for REWARD/WAREHOUSE; `addBonus`/`removeBonus` at 9 → 10 → 9 online with the two `SM_ICON_INFO`; warehouse limit (3 + level − 1) × 8; the in-use compare-and-set from two threads; each companion against its enum's constructor arguments. **Mutation-proven**: name the test that goes red for swapping two masks, `>=` → `>` in `addBonus`, the trim applied to LEGION history, `updateLimit` with `DEFAULT_ROWS` 2 | – | L-01..L-05 | R | M | – |

### Stage 1, legion service (P5-11 less `model/house/**` and `HousingService.*`, + P5-12a/P5-12b leases) — part 1: S-01..S-03; part 2: S-04..S-09

| Id | What | Java refs | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|---|
| **S-01** | `LegionRestrictions` — the 18 bodies (`LegionService.cpp:43-111`), every refusal message of LegionService.java:804-1080 | LegionService.java:799-1080 | L-02 | R | M | A-09 |
| **S-02** | Create and join: `createLegion`, `addToLegion`, `invitePlayerToLegion` (+ its accept/deny struct), `addLegionMember` ×2, `displayLegionAnnouncement`, `updateLegionMemberList` ×2 (split at 80, the excluded joiner) | :209-280, :678-708, :1099-1117 | S-01, L-01 | R | M | A-01, A-07 |
| **S-03** | Running: `appointRank`, `startBrigadeGeneralChangeProcess` (+ struct), `appointBrigadeGeneral` ×2 (+ accept/deny struct), `changePermissions`, `changeSelfIntro`, `changeNickname`, `requestChangeLevel`, `changeLevel`, `setContributionPoints`, `changeAnnouncement`, `addHistory` ×2, `addRewardHistory`. `appointRank` and `changeNickname` look the member up **by name** (A-23) | :282-426, :546-551, :629-676 | S-01 | R | M | A-07, **A-23** |
| **S-04** | Emblem (part 2): `storeLegionEmblem`, `uploadEmblemInfo`, `uploadEmblemData`, `sendEmblemData` (7,993-byte chunks), `updateMembersEmblem` | :440-450, :469-478, :553-624 | S-01, L-03 | R | M | A-01, A-09 |
| **S-05** | Lifecycle (part 2): `onLogin`, `onLogout`, `updateMemberInfo` (also reached by every cross-map teleport and class change of a member, §2.10), `removeLegionMember` ×2, `kickMember` (by name, **A-23**), `leaveLegion`, `deleteLegionMemberFromDB`, `openLegionWarehouse`, `requestDisbandLegion` (+ struct), `recreateLegion` (+ struct), `disbandLegion`, `updateAfterDisbandLegion`, `updateMembersOfDisbandLegion`, `updateMembersOfRecreateLegion`, `tryRename` (W: only `CM_APPEARANCE`, M5j, calls it); **and `addWHItemHistory` if A-04 is missing** | :95-101, :181-207, :430-467, :480-541, :710-792, :1082-1092, :1119-1146 | S-01, L-01, L-04 | R | M | A-04, A-06, A-07, A-23 |
| **S-06** | Leases (part 2): `ConquerorAndProtectorService::onLeaveLegion` and `resetLegionDominionRank` (P5-12b; `updateBuffAndNotifyNearbyPlayers` stays unported — it needs a `CPInfo`, which only `onKill` creates); `SiegeService::cleanLegionId` (P5-12a, 8 lines) | ConquerorAndProtectorService.java:124-137; SiegeService.java:449-456 | – | R | S | – |
| **S-07** | Tests in `tests/legionhouse` (files `Legion*Test.cpp`; the studio lane's P5-11 tests are `House*Test.cpp`), written in both parts: the restriction decision table (each message, each order — e.g. kinah is checked after name and membership); create → invite → accept/deny → kick → leave over a fake world with three players and a recording `PacketSendUtility`, **the leave and the kick each broadcasting a `KICK` history first**; announcement truncation at 256 and the clear arm; `sendEmblemData` at 0, 7,993, 7,994 and 15,987 bytes; the corrupt-upload arm (`uploadedSize > uploadSize`); the lazy disband on a `ManualClock`; the recreate's `EDIT(0x07)`, `UPDATE_MEMBER(1300307)`, `EDIT(0x07)` sequence; **a pending invite dropped when the invitee logs out** (the `cycles.toml:259` hook) with the struct's `Legion` released; **S-08**: `startWeeklyCalculation` with no participants (each location reset and stored, one `SM_LEGION_DOMINION_LOC_INFO` world broadcast), with a previous occupier (its `SM_LEGION_DOMINION_RANK` + `SM_LEGION_INFO`, `occupied` cleared), with a ranked winner (occupation set; the reward mail through a recording `SystemMailService` seam or W if A-25 is missing), a disbanding winner skipped with the warning. Mutation-proven | – | S-01..S-06, S-08 | R | L | – |
| **S-08** | **The Legion Dominion weekly calculation** (part 2, D13): `LegionDominionService::startWeeklyCalculation`, `updateLegionOccupation` (`LegionDominionService.cpp:60-66`); `LegionDominionLocation::getLegionRanking` (points above `STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY`, sorted by points desc then date), `getRewards` (grouped by rank), `reset` (`LegionDominionLocation.cpp:61-67, 85-86`) | LegionDominionService.java:84-164; LegionDominionLocation.java:83-96, 129-131 | L-01 | R | S | A-25 |
| **S-09** | **Only if A-23 is missing**: `PlayerService::getOrLoadPlayerCommonData` ×2 (P5-00 lease; `World.getPlayer` else `PlayerDAO.loadPlayerCommonData(ById/ByName)`) + a unit test with an online and an offline player | PlayerService.java:235-247 | – | R | S | A-23 |

### Stage 1, client packets (P5-15, P5-16)

| Id | What | Java refs | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|---|
| **P-01** | The 8 legion packets with `AION_CLIENT_PACKET` markers; byte-vector tests in `tests/cm_lz` for each `readImpl` arm (CM_LEGION's 14 arms and the unknown-arm warning) | the 8 `CM_LEGION*.java` | – | R | M | – |
| **P-02** | The 7 studio packets: `CM_HOUSE_EDIT` (+ `removeRenovationCoupon`), `CM_HOUSE_DECORATE`, `CM_HOUSE_SETTINGS`, `CM_HOUSE_SCRIPT`, `CM_HOUSE_KICK` (P5-15, tests in `tests/cm_ak`); `CM_USE_HOUSE_OBJECT`, `CM_RELEASE_OBJECT` (P5-16) | the 7 `.java` | – | R | M | A-05 |
| **P-03** | **Only if A-19 is missing**: `CM_CHAT_MESSAGE_PUBLIC` (all arms; the group arms reach M5g's team model), `PlayerRestrictions::canChat` (P5-13 lease), `PlayerChatService::logMessage` ×2 and `isFlooding` (P5-08 lease) | CM_CHAT_MESSAGE_PUBLIC.java | – | R | M | A-19 |
| **P-04** | **Only if A-07 is missing**: `CM_QUESTION_RESPONSE` | CM_QUESTION_RESPONSE.java | – | R | S | A-07 |
| **P-05** | **Part 2.** In-process run tests over `InWorldPacketRunSupport.h`: `CM_LEGION(0x00)` by a member does nothing, `0x01` by a non-member does nothing (CM_LEGION.java:118, 157-163); `CM_LEGION_HISTORY` REWARD by a non-leader sends nothing; `CM_LEGION_WH_KINAH` without an open warehouse **moves the kinah** (D7); `CM_HOUSE_SCRIPT` with a foreign address sends nothing and stores nothing; a valid `CM_HOUSE_SCRIPT` sends nothing to the sender and `SM_HOUSE_SCRIPTS` to a second player who knows him; `CM_HOUSE_EDIT(3)` with an unknown item does nothing | – | P-01, P-02, S-*, L-* | R | M | – |

### Stage 1, studio (P5-05, P5-07, P4-11a, P5-11's `model/house/**` and `services/HousingService.*`, + A1/Q05/Q09/P5-13 leases)

| Id | What | Java refs | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|---|
| **H-01** | `ButlerAI` (`butler`), `HouseSignAI` (`housesign`) — new root handlers with `AION_AI` registration (D6) | ButlerAI.java, HouseSignAI.java | – | R | S | – |
| **H-02** | `StudioPortalAI` (`studioportal`, A1 lease) over `ActionItemNpcAI`; **and `ActionItemNpcAI` itself (7 bodies incl. the anonymous observer) only if A-11 is missing** | StudioPortalAI.java:24-61; ActionItemNpcAI.java:31-97 | – | R | S (M with `ActionItemNpcAI`) | A-06, A-11, A-14, A-15, A-24 |
| **H-03** | `_18832ImaginingAQuietLife`, `_18802AndAHomeforEveryDaeva` (Q05 lease), `_28832TakingtheTour`, `_28802BeItEverSoHumble` (**Q09** lease) — D4; with in-process tests on M5d's quest-handler fixture (the chunks' test directories): 18832 start at 830365 → REWARD at 830001 → COMPLETE; 18802 offered by 830005 only after 18801 COMPLETE; 18802's `SELECTED_QUEST_NOREWARD` at 830069 calling `registerPlayerStudio` | the four handlers (65 + 69 + 65 + 69 lines) | – | R | S | A-10, A-13 |
| **H-04** | `HouseObjectFactory::createNew` ×2 (P5-07; the 10-way template dispatch, `useDays` expiry); `SummonHouseObjectAction`/`DecorateAction` `canAct`/`act` (Java: `return false` / empty); **`DecorateAction::getTemplateId` (null → 0) — the additive header request of §8** | HouseObjectFactory.java:42-82; DecorateAction.java:16-32 | – | R | S | A-05 |
| **H-05** | `UseableItemObject`'s `placementLimitOf` (P4-11a, reached from `onUse`) — blocked on accessors that now exist (`PlaceableHouseObject.h:25`, `LimitTypeInfo.h:52-57`); `HouseObject::getPlacementLimit(bool)` **O**: it has no Java caller (dead), port it only to close the site | UseableItemObject.java:99; HouseObject.java:190-195 | – | R / O | S | – |
| **H-06** | `InstanceService::getOrCreateHouseInstance` (P5-13 lease) — **required**: M5f leaves it W (A-16) | InstanceService.java:127-135 | – | R | S | A-15 |
| **H-07** | Tests: `tests/itemsvc` (P5-07) — `createNew` for one template of each of the 10 kinds and for one of the **7 items with a missing template** (Java's `NullPointerException`); **the cake's expiry = now + 30 days**; **`CM_HOUSE_EDIT(3)` of the bare `<housedeco/>` item 170000023 → a `HouseDecoration` with template id 0**; the `PlayerRegisteredItemsDAO` store → load round trip through `createNew` (the restart path the gate cannot run; `tests/dao/LegionHouseItemDaoTest.cpp` is the database fixture to copy). `tests/objects` (P4-11a) — `HouseRegistry` add → spawn → move → despawn → delete with persistent states; `UseableItemObject.onUse` on a `ManualClock` (cooldown, COOKING limit, cancel through `releaseOccupant`); **a `ChairObject` built from a chair template and a `PassiveObject` from a passive one — the only check of the chair arm (the gate cannot tell them apart, §10.4)**. `tests/controllers` (P4-11b test lease) — `HouseController.onDespawn` of a reusable studio clearing its spawns and position, and **after a fabricated studio instance is destroyed, what holds the destroyed `WorldMapInstance`**: the lane either adds a documented C++ breaker (reset each registry object's position when it despawns, so no instance survives through a `HouseObject`) or records the Java retention as `accepted` in `cycles.toml` (one destroyed instance per studio owner, until the owner's next entry replaces the positions) — and the test asserts the chosen one (§9 risk 3). `tests/handlers_ai_core` (P5-05) — the butler's and sign's `onDialogSelect` pages, `handleCreatureSee` sending all 8 script slots, `studioportal` entering and leaving over fabricated instances with the 2-s bar on a `ManualClock` (a move at 1 s aborts: `SM_USE_OBJECT(…, 0, 2)`, no teleport; the observer gone after completion). `tests/legionhouse` `House*Test.cpp` (P5-11 studio files) — whatever the studio lane fixes there. Mutation-proven | – | H-01..H-06 | R | M | A-15 |

### Stage 1, gate harness (P5-SC, `tools/oracle`)

| Id | What | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|
| **G-01** | `oracle.py m5h-legion`: the config the server loads (**properties files, not Java defaults**), default permissions, creation/level/emblem prices through the price model (A-09), the Sanctum spots within talk range of 203806, 203807 and 203751 (the m5c "talk band" method), the history action ids and types, the chunk sizes of an emblem of N bytes. `oracle.py m5h-housing`: studio address, land, building, price, arrival point and exit per race; **spots inside each talk range of §2.12** — the Parrine spot (≤ 6.6 m of 830069), the entrance spot (≤ 6.6 m of 730517, reached from the Parrine spot and from the Oriel exit point by a straight walk), the exit spot inside the studio (≤ 11.25 m of 830229, ~2 m from the arrival point toward it), each with the walking time at run speed; the 830069 dialog ids (23, 96) and quest 18802's rewards; `house_npcs.xml` spawns of address 2001/3001; for a given item id its object template, kind, `use_days` (and **the expiry window a placement at time t yields**), `owner`, `talking_distance`, delay, cd, limit and rewards; for a `housedeco` item its part, type, line numbers and `CP_*` compatibility (and 0 for a bare `<housedeco/>`); **a legion-storable item** (mask `STORABLE_IN_LWH`, not soul-bound) for the C9 and D8 warehouse rows; **`--checklist-sql` printing §11's seeds**, including the quest-state shortcuts. Plus `tools/oracle` tests | – | R | M | A-09 |
| **G-02** | `GameSession` builders for the 15 (+2) packets and `CM_MOVE` walks to an oracle spot (the m5c "talk band" helper, if it exists at branch time); seed helpers (legion rows, `player_quests`, furniture items, `players.world_owner`; A-08); independent decoders `tests/scenario/decoders/{LegionDecoders,HousingDecoders}.{h,cpp}` written from the Java `writeImpl` — `SM_LEGION_INFO`, `_MEMBERLIST`, `_ADD_MEMBER`, `_EDIT` (9 types), `_UPDATE_MEMBER`, `_LEAVE_MEMBER`, `_UPDATE_TITLE`, `_UPDATE_EMBLEM`, `_SEND_EMBLEM`, `_SEND_EMBLEM_DATA`, `_HISTORY`, `_UPDATE_SELF_INTRO`, `_UPDATE_NICKNAME`, `SM_ICON_INFO`, `SM_MESSAGE` (if no earlier decoder), `SM_QUESTION_WINDOW` (if none), `SM_WAREHOUSE_INFO` (if none), the legion fields of `SM_CHARACTER_LIST`'s player entry (AbstractPlayerInfoPacket.java:111-113, extending the M5a decoder), `SM_HOUSE_OWNER_INFO`, `_ACQUIRE`, `_RENDER` (AbstractHouseInfoPacket), `_UPDATE`, `_EDIT` (4 shapes, **with the `UseableItemObject` usage block and `secondsUntilExpiration`**), `_REGISTRY`, `_OBJECT` (the type byte's `use_item` and npc arms), `_OBJECTS`, `_SCRIPTS` (8 slots, the 8 padding bytes), `SM_DELETE_HOUSE_OBJECT`, `SM_OBJECT_USE_UPDATE`; **reusing M5f's G-02 decoders** for `SM_USE_OBJECT`, `SM_EMOTION`, `SM_TELEPORT_LOC`, `SM_CHANNEL_INFO` and its `teleportAndArrive()` helper (A-14) — no `serverpackets/` include, each body consumed exactly; `*DecodersTest.cpp` | – | R | L | A-08, A-14 |

### Stage 2, the gate

| Id | What | Deps | Need | Eff | Assumes |
|---|---|---|---|---|---|
| **G-03** | `TEST(M5hScenario, Run)`, `gs.scenario.m5h` — §10; its schema pair, output directory, allow-list and the shared `RESOURCE_LOCK`. Its skeleton (C0-C1, the seeds, the decoders wired) can be written in stage 1 part 2 (§7) | stage 1 | R | L | A-01..A-25 |
| **G-04** | `gs.scenario.m5h_geo` — §10.5 | G-03 | R | M | A-15 |
| **G-05** | **Re-green** every earlier gate and `gs.smoke.startup(_geo)` (A-20): D6 changes startup (1,030 butlers get a live AI; the "No AIs could be found …" warning, `AIEngine.cpp:191-208`, loses `butler`, `housesign` and `studioportal`); no earlier gate has a legion or a house, so no case should move — **record the startup time and the live-npc count before and after** | G-03 | R | M | A-20 |
| **G-06** | **Offered to the user** (capacity tests are designed with the user, phase5-roadmap.md:65-66): a nightly where 20 clients in one legion chat, deposit and withdraw under warehouse contention, and relog | G-03 | O | M | – |
| **G-07** | `CheckOutput` (P5-14, whose M5b-2 part-3 change is committed at `760e8ab5c`): `LegionMember`, `Legion`, `LegionHistoryEntry`, `HouseObject`, `HouseDecoration`, `SummonedHouseNpc` and the `LegionService` handler structs in `zeroLiveClasses()` and the summary rows; the `WorldMapInstance` live count after the gate's studio destroy and re-entry, checked against H-07's chosen treatment of `HouseObject` positions (§9 risk 3) | – | R | S | – |
| **F-01** | Fixups the gate names, in the owning chunk | G-03 | R | – | – |

### M5h-2 (land houses, visiting, towns) — listed so it can branch without re-analysis

| Id | What | Chunk | Bodies |
|---|---|---|---|
| X-01 | `HousingBidService` 7 (`auction`, `bid`, `isAllowedToBid`, `endAuctions`, `endAuction`, `impoundAndAuctionOldPlayerHouses`, `cancelAuction`) | P5-11 | 7 |
| X-02 | `AuctionAutoFillTask`, `MaintenanceTask`, `AuctionEndTask`: 2 bodies + the 3 partials | P5-11 | 5 |
| X-03 | `MailFormatter::sendHouseMaintenanceMail`, `sendHouseAuctionMail` (lease) | P5-09 | 2 |
| X-04 | `SM_HOUSE_BIDS` stand-in → `AuctionEndTask::getRemainingAuctionSeconds` (lease) | P4-16 | 1 |
| X-05 | `CM_GET_HOUSE_BIDS`, `CM_PLACE_BID`, `CM_REGISTER_HOUSE`, `CM_HOUSE_PAY_RENT`, `CM_HOUSE_OPEN_DOOR`, `CM_HOUSE_TELEPORT`, `CM_HOUSE_TELEPORT_BACK`, `CM_CHALLENGE_LIST` | P5-15, P5-16 | 26 |
| X-06 | `FriendPortalAI`, `HouseGateAI` (A1 leases); `SummonGroupGateEffect` (1 site, P5-04) | A1, P5-04 | 6 |
| X-07 | `Town::increaseLevel`, `broadcastUpdate` + header request economy-legion-1 (`levelUpDate` → `Field<std::optional<Timestamp>>`, P5-11.md) | P5-11 | 2 |
| X-08 | `ChallengeTaskService` 8, `ChallengeTask` 7, `ChallengeQuest` 5 | P5-10f | 20 |
| X-09 | Gate `gs.scenario.m5h2`: cron expressions moved into the run window, register days `1, 7`, four accounts, two land houses | P5-SC | – |

### Deferred elsewhere

| Id | What | Milestone |
|---|---|---|
| O-01 | Legion Dominion less S-08: `LegionDominionService` 4 (`join`, `onFinishInstance`, `isInCalculationTime`, `openInvasionRift`), `LegionDominionLocation` 3 (`join`, `store`, `updateRanking`), `LegionDominionIntruderUpdateTask` (3, no file), `LegionDominionPortalAI` (A1), `CM_LEGION_DOMINION_REQUEST_RANKING`, `LegionService::joinLegionDominion` | M5i |
| O-02 | `CM_ABYSS_RANKING_LEGIONS` | M5i |
| O-03 | `CM_APPEARANCE` (legion and character rename tickets) | M5j |
| O-04 | The other six housing tutorial quests (`_18821`, `_18828`, `_18830`, `_28821`, `_28828`, `_28830`) | phase 6 (Q05, Q09) |
| O-05 | `WarehouseService` (5, personal warehouse expansion) | M5c or its M5c-2 (m5d §8.1) |

---

## 7. Lanes and stages

At most six lanes per stage; chunks disjoint within a stage (within P5-11, files disjoint: the studio lane holds `model/house/**` and
`services/HousingService.*`, the legion-service lane the rest — I-02). **Stage 1 runs in two parts with an integrator commit between them (D14).**

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 0 | integrator | manifest (only if A-18 is missing), leases | I-01..I-04 | – |
| **1 part 1** | **legion-model** | **P5-10f** (+ P4-14, P4-16, P4-17 leases) | L-01..L-06 | `tests/team/P5-10f` (+ the `tests/sm_lz` lease) |
| **1 part 1** | **legion-service** | **P5-11** less the studio lane's files | S-01, S-02, S-03 and their S-07 tests | `tests/legionhouse` (`Legion*Test.cpp`) |
| **1 part 1** | **client-packets** | **P5-15, P5-16** (+ P5-08/P5-13 leases if P-03) | P-01..P-04 | `tests/cm_ak`, `tests/cm_lz` |
| **1 part 1** | **studio** | **P5-05, P5-07, P4-11a**, P5-11's `model/house/**` + `services/HousingService.*` (+ A1, Q05, Q09, P5-13 leases) | H-01..H-07 | `tests/itemsvc`, `tests/objects`, `tests/handlers_ai_core`, `tests/legionhouse` (`House*Test.cpp`) (+ a `tests/controllers` lease) |
| **1 part 1** | **gate-harness** | P5-SC, `tools/oracle` | G-01, G-02 | `tools.oracle`, decoder self-tests |
| **1 part 2** | **legion-service** | as part 1 (+ P5-12a, P5-12b leases; P5-00 if S-09) | S-04, S-05, S-06, S-08, S-09 (if A-23), the rest of S-07 | `tests/legionhouse` |
| **1 part 2** | **client-packets** | P5-15, P5-16 | P-05 (the run tests need S-*) | `tests/cm_ak`, `tests/cm_lz` |
| **1 part 2** | **studio** (optional) | as part 1 | fixes in its P5-11 housing files that H-07 or P-05 surface | as part 1 |
| **1 part 2** | **gate-harness** | P5-SC | G-03's skeleton: C0-C1, the seeds, the decoders wired | – |
| **2** | gate | P5-SC | G-03, G-04 | `gs.scenario.m5h`, `gs.scenario.m5h_geo` |
| **2** | regate | P5-SC (second lease) or serialized in the gate lane | G-05 | every earlier gate |
| **2** | census | P5-14 | G-07 | `tests/misc` |
| **2** | fixups | whichever chunk the gate names | F-01 | owning tests + gate re-run |

**Per-lane load** (bodies from §2.8-§2.9; effort from §6's letters; M5b-1 closed ~150 sites over six lanes, ~25 per lane, m5b2-plan.md:614-615):

| Lane | Bodies | Java lines | Items and letters | Agent-days |
|---|---|---|---|---|
| legion-service | **~76**: 61 `LegionService` sites, 7 anonymous structs, 3 leased (P5-12a/b), 5 S-08 (+2 S-09) — one file of one chunk | ~1,280 | S-01..S-05 M×5, S-06 S, S-08 S, S-07 L | **9-12 — the critical path** (part 1 ~4-5: S-01..S-03 and their tests; part 2 ~5-7), above M5b-2's K-lanes' 7-9 |
| legion-model | 71: 66 sites (22 of them one-line "behind proxy" throws) + 5 companions; 12 call sites moved | 974 | L-01 M, L-02..L-05 S×4, L-06 M | 5-6 |
| client-packets | 46 (+12 P-03, +3 P-04) | 953 | P-01 M, P-02 M, P-05 M | 4-5 (+1-2 if P-03) |
| studio | ~31 (+7 if A-11): 3 AIs 9, four quest handlers 12, `createNew` 2, `canAct`/`act` 4, `placementLimitOf` 1 (+1 dead), `getTemplateId` 1, `getOrCreateHouseInstance` 1 | ~600 (+99 with `ActionItemNpcAI`) | H-01..H-06 S×6, H-07 M | 4-5 (+ part-2 fixes) |
| gate-harness | – (oracle, decoders, builders) | – | G-01 M, G-02 L | 4-5 (+ G-03 skeleton) |

**Notes.**

- **Stage 1 has five lanes, not six**, because the P5-10 split (A-18) and the leases leave nothing else. At ~224 bodies it is 1.5× M5b-1's
  stage 1 and unevenly spread: the two legion lanes carry ~2.8× M5b-1's per-lane load, and `LegionService.cpp` cannot be split between lanes.
  **Hence two parts (D14)**: part 1 lands the model, the restrictions, create/join and running, both packet sets, the studio and the harness;
  the integrator commits; part 2 lands the emblem, the lifecycle, the leases, the weekly calculation and the packet run tests. The commit
  between them gives the part-2 reviewer a smaller diff and every lane a green base.
- **Critical path: legion-service, 9-12 agent-days.** It needs `LegionMember::hasRights` (L-02) and the companions (L-05) to compile real
  restriction bodies: **merge order** L-05 → L-02 → L-01, L-03, L-04 → S-01 → S-02, S-03 (**part-1 commit**) → S-04, S-05, S-06, S-08 → P-05's
  run tests. `studio` and `gate-harness` have no body dependency on the legion lanes and start on day 1.
- **The studio lane stands on M5f** (A-14, A-15, A-24). If M5f has not delivered personal instances, the instance teleport and the leave hooks at
  branch time, the lane still lands its bodies and unit tests, and the gate's studio cases (C13-C20, Y12-Y19) move to a stage-2b run after
  M5f — the legion cases do not wait.
- **Dormant-code fixes**: bugs the studio lifecycle surfaces in `House`, `HouseRegistry` or `HousingService` (§9 risk 1) are the studio lane's —
  it holds those P5-11 files in both parts — not the busy legion-service lane's; after stage 1 they are F-01's.
- **Green points**: after part 2, the gate's legion cases (C2-C12, C21-C22; Y1-Y11, Y20-Y21) can run alone (C1 needs `onLogin`, S-05); the
  studio cases add after the studio lane and M5f. Splitting at the *chunk* instead (model first, service second) leaves a wave with nothing a
  client can observe.

---

## 8. Header requests expected

Bodies never need a request (hub-headers.md §14).

| Request | Kind | For |
|---|---|---|
| `LegionPermissionsMaskInfo.h`, `LegionRankInfo.h`, `LegionHistoryActionInfo.h`, `LegionEmblemTypeInfo.h` in `model/team/legion/` | new files (no request) | L-05 |
| `network/aion/serverpackets/detail/PacketSupport.h` (P4-17): **delete** `legionEmblemTypeValue`, `legionRankId`, `legionHistoryActionId` once the companions exist; **12 packet sites in 12 packets move** (11 in P4-17, `SM_GM_SHOW_LEGION_MEMBERLIST.cpp:24` in **P4-16**), and the stand-ins' test `tests/sm_lz/OpcodesAndSupportTest.cpp:156-172` moves to the companions (L-05) | **signature** (removal) | L-05 |
| `dao/LegionDAO.cpp` (P4-14): delete the local `getType` stand-in (`:57-75`) and point `:338` at the companion | none (a `.cpp`) | L-05 |
| **`model/templates/item/actions/DecorateAction.h` (P5-07): `int32_t getTemplateId() const` — `partId` absent → 0** (DecorateAction.java:28-32); the generated block keeps `partId` private, so the accessor goes in the hand-written class body | **additive** (a new member function) | H-04 |
| 15 new client packet headers (+ `CM_CHAT_MESSAGE_PUBLIC`, `CM_QUESTION_RESPONSE` conditionally) | new files | P-01..P-04 |
| `handlers/ai/ButlerAI.h`, `HouseSignAI.h` (P5-05), `handlers/ai/portals/StudioPortalAI.h` (A1), the four quest handler headers (Q05, Q09); `handlers/ai/ActionItemNpcAI.h` (P5-05) only if A-11 is missing | new files | H-01..H-03 |
| `ItemActions.h`: `getHouseObjectAction`, `getDecorateAction` — **only if M5b-3's h02 did not add them** | additive | H-04 (A-05) |
| `LegionService.h`, `Legion.h`, `LegionMember.h`, `LegionWarehouse.h`, `LegionEmblem.h`, `InstanceService.h`, `SiegeService.h`, `ConquerorAndProtectorService.h` | **none expected** — every declaration exists; the handler structs live in `LegionService.cpp` (hub-headers.md §9.3) | – |
| P5-14 `CheckOutput`: the G-07 classes in `zeroLiveClasses()` | additive | G-07 |
| **Manifest (D3)**: M5g's D1 six-part split (P5-10a..f sharing `aion_gs_team`, `tests/team/P5-10x`) — only if M5g did not land it (A-18) | build | I-01 |
| `game-server/config/m5h.properties.example` in the Java tree | none | I-04 |
| M5h-2 only: `Town.h` `levelUpDate` (economy-legion-1) | signature | X-07 |

---

## 9. Risks

Ordered by what is most likely to go wrong, with the evidence.

1. **The dormant housing code is the milestone's real unknown.** ~1,300-1,600 Java lines of housing and ~725 of legion persistence (§2.10) were
   ported in phase 4 and M5a against headers and unit tests and have never run in a server: `HouseController.onAfterSpawn` for a *personal*
   instance and `onDespawn`, `HouseRegistry` persistent states,
   `PlayerRegisteredItemsDAO.store`, `HouseObject.spawn`/`removeFromHouse`, `UseableItemObject`'s scheduled task, `ExpireTimerTask` for house
   objects, `SM_HOUSE_EDIT`'s write-time registry read. **Expect the gate to find several** — M5b-1 predicted two hidden prerequisites and found
   eight. The gate (§10) walks the studio's entire lifecycle including leave, destroy, re-enter, logout and a re-login inside for exactly this
   reason, and every studio row says which dormant body it is the first to run. The fixes land in the studio lane's P5-11 files (§7), not the
   legion-service lane's.
2. **Seven earlier milestones must deliver.** The studio half rests on M5f (A-14, A-15, A-24: personal instances, the instance teleport and its
   `CM_TELEPORT_ANIMATION_DONE`, the destroy timer, the leave hooks) and M5c/M5d (A-06, A-11: the dialog service and the use bar); the legion
   model's chunk and legion chat on M5g (A-18, A-19); every name lookup on M5c's M-02 (A-23). §0's fallbacks cover the small ones (A-04, A-07,
   A-11, A-19, A-23); **A-14/A-15/A-24 have no fallback inside M5h** — if M5f does not deliver them, the studio cases wait (§7). I-03 is the
   item that finds out on day 0 instead of day 4. All seven plans are drafts; their item ids are what I-03 re-checks.
3. **Lifetimes that Java's collector hides.** Legions are never evicted (`legionsById` holds every legion loaded since startup, LegionService.java:48;
   `cycles.toml:207` "a Legion holds no Player"), `legionMemberById` caches offline members too, the invite handler holds a `Legion`
   (`cycles.toml:259`), a studio's butler and crystal are held by `House.spawns` until the instance destroy runs `clearSpawns`
   (`cycles.toml:167`), and a reusable studio must drop its position (HouseController.java:91-100) or it pins a destroyed `WorldMapInstance`.
   **Its objects do not drop theirs**: `HouseObject.onDespawn` is empty (HouseObject.java:298-300), so after a destroy every registry object's
   `WorldPosition` still leads to the destroyed instance (`WorldPosition.mapRegion` retains it — the reason for the `startPos` breaker,
   `cycles.toml:357`) until the owner's next entry replaces it (`spawn()`, :270-275) — one pinned instance per studio owner who left and has not
   returned, for the server's lifetime if he never does. H-07 chooses a documented breaker or an `accepted` row; G-07 checks the choice.
   `UseableItemObject`'s use task captures the player and the object for `delay` ms (UseableItemObject.java:145). **G-07's census rows and H-07's
   `onDespawn` cases are the only checks**; the gate cannot see a leak of a server-lifetime object.
4. **Concurrency and blocking the Java code leaves loose.** `LegionEmblem`'s upload accumulator is mutated by consecutive packets without a monitor;
   `Legion.hasBonus` is a compare-and-set whose broadcast races with a concurrent logout; the warehouse's single user is a compare-and-set that a
   crashed client holds until logout (`onLogout` → `unsetInUse`); `SM_HOUSE_EDIT` reads the active house's registry on the writer thread. Port
   exactly and mark `// java-race` (m5b2-plan.md D9). `legionMemberById.computeIfAbsent` already loads from the database under a stripe monitor
   (`LegionService.cpp:212-229`, a documented conformance site) — a login storm of one legion serializes there. **And the first load of a
   legion runs inside a packet's `writeImpl`**: `SM_CHARACTER_LIST` → `writePlayerInfo` → `getLegionMember` (`AbstractPlayerInfoPacket.cpp:33`)
   does the member, legion, announcement, emblem, warehouse and history queries (`LegionService.cpp:190-197`) while the packet is being
   serialized, under that stripe monitor — Java does the same, but a slow database stalls whichever thread writes the character list. Port as
   Java; the gate's C1 is where a hang or lockdep report would show.
5. **Data traps that a grep would not see.** The shipped `legions.properties` overrides the member thresholds (6, not 10); 7 furniture items point
   at no template; `building_tags` is space-separated (a comma split finds **0** studio decorations, the parse finds 103); the level-too-low emblem
   refusal is silent. Each is in G-01 or an H-07/L-06 case.
6. **Phase-6 pull-forwards.** `_18832`/`_18802`/`_28832`/`_28802` need M5d's `AbstractQuestHandler` base and XML engine (A-10; the
   `AION_QUEST_HANDLER` registry already exists); if they are missing, D5's quest path drops to M5h-2 and a real player has no way to a studio
   at all (the paid button needs 18802 COMPLETE, §2.7 row 2) — the checklist then seeds the quest state. `StudioPortalAI` needs M5d's
   `ActionItemNpcAI` (A-11) and M5c's `isInteractionAllowed` (A-06).
7. **The real client sees more than the gate.** Oriel and Pernon hold 500 houses each, with 1,500 npcs per map; the gate spot is 231 m from the
   nearest house, but a real player walking to Parrine passes through them (`SM_HOUSE_RENDER` for every house within
   `gameserver.housing.visibility.distance` = 200 m). Clicking a butler's auction, rent or crystal options sends packets M5h does not port; they
   must be logged unknown packets, not crashes.
8. **`CM_CHAT_MESSAGE_PUBLIC` is everyone's and nobody's.** Normal, shout, group and legion chat all go through it; if neither M5g nor M5h ports it
   the real-client session cannot chat at all. D12 and P-03.
9. **The Wednesday 09:00 cron throws until M5h lands** (`CronJobService.cpp:185-187` → the unported `startWeeklyCalculation`). M5h ports it
   (S-08, D13), so after M5h the risk is gone; before that, any run of any gate or real-client session that crosses Wednesday 09:00 logs an ERROR
   and fails the "no ERROR, empty `unported_trace`" bar (Y22 of this gate, the equivalent row of every earlier gate) — a known false failure,
   not a regression.
10. **One more serialized gate run.** Three accounts, ~12 relogs, two instance round trips with their 2-s use bars and walks, and **one wait of up
    to ~60 s for the studio's destroy** (A-15's checker): budget **240-360 s** under the shared `RESOURCE_LOCK`.
11. **The legion-service lane is a long pole** (D14): ~76 bodies in one file, 9-12 agent-days, while the other four lanes finish in 4-6. Two parts
    with a commit between them keep the reviewer's diff small; if part 2 slips, the gate's skeleton (G-03) and the studio fixes still move.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5h`)

### 10.1 Processes, databases and profile

| Piece | M5h |
|---|---|
| Schemas | `aion_ls_test_m5h_<hash>` / `aion_gs_test_m5h_<hash>`, the same `SchemaLease` and sweep |
| Output directory | `<bin>/scenario/m5h` |
| `RESOURCE_LOCK` | the shared `"aion_game_server_log;aion_login_server_log"` |
| Profile | the latest gate profile at branch time (siege, conqueror/protector, rifts, vortex, world raids off; `missing_ai_handlers = warn`) **plus `gameserver.legion.disbandtime = 5`** (D10). Written out as `game-server/config/m5h.properties.example` (I-04) |
| Allow-list | `tests/scenario/m5h_partial_allowlist.txt`: §A copied at branch time from the latest gate's §A; §B nothing new; §C the three housing cron rows (D11) and the earlier timing rows |
| Characters | **A** Elyos Warrior (account A), **B** Elyos Mage (account B), **C** Elyos Priest (account C) — all Elyos (`gameserver.legion.inviteotherfaction = false`) |
| Seeds (D8, A-08) | **after the characters are created and before the first enter world**: A and B at the oracle's Losadis spot, **A's kinah exactly 10,000**, **B's kinah 9,999**; C's seeded legion (level 3, C as brigade general, contribution 12,345, one announcement, one history row, **one legion-storable item** (G-01) and 5,000 legion kinah — `inventory` rows owned by the legion id, location 3) and C at the oracle's **Pauton** spot with kinah = 2 × the oracle's emblem price (emblem changes need no npc: LegionService.java:469-478, 553-590). **Later offline windows**: C8 — B to the oracle's kill spot on Poeta with exp one kill short of level 2; C9 — A and B to the Pauton spot, A's kinah 101,000 and one legion-storable item; C13 — A to the **Parrine spot** with `player_quests(18802, REWARD)` and the three furniture items (bed, cake, wallpaper; ids above `wrap_at`); C14 — C to the Parrine spot with kinah exactly 4,000,000 and one cake; C22 — A to the Pauton spot **with `players.world_owner = 0`** (A last left the game inside its studio, C19), then to the Losadis spot |

### 10.2 Cases

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `oracle.py m5h-legion` and `m5h-housing` return every constant below |
| **C1** | setup | login, create A, B, C; seed (C's legion rows exist only now, so `SM_CREATE_CHARACTER`'s lookup found nothing and cached nothing); **each account asks for the character list again** (`CM_CHARACTER_LIST`) — C's list is where the seeded legion is first loaded (§2.5 row 0); enter world; level ready (the M5a cases) |
| **C2** | create | A `CM_LEGION(0x00, name)` (the oracle's letters-only name) |
| **C3** | refusals | B: same name; `"A1!"`; a fresh valid name with 9,999 kinah |
| **C4** | invite | A `CM_LEGION(0x01, B)`; B `CM_QUESTION_RESPONSE(yes)`; A invites C (another legion's member) and B again |
| **C5** | announcement | B `0x09` (refused), A `0x09` with 300 characters, B `0x07` |
| **C6** | ranks and permissions | A `0x06(CENTURION, B)`; A `0x0D(0x1E0C, 0x1E08, 0x1800, 0x800)`; B `0x09` again; B `0x0D`; B `0x0A(intro)`; A `0x0F(B, "nick")`. **A-23**: the rank and nickname steps look B up by name |
| **C7** | level-up refusal, money | A (0 kinah) `0x0E` |
| **C8** | relog, kill, level up | B `CM_QUIT`; seed; B re-enters on Poeta and kills the oracle's monster |
| **C9** | warehouse | A, B quit; seed Pauton; re-enter; A `0x0E` (101,000 kinah); A opens (`CM_SHOW_DIALOG(203751)`, `CM_DIALOG_SELECT(203751, 53)`); B tries to open; A deposits 1,000 kinah (`CM_LEGION_WH_KINAH(1000, 1)`) and the item (`CM_MOVE_ITEM(item, 0 → 3, −1)`); A `CM_CLOSE_DIALOG`; B opens, tries to withdraw 500, closes |
| **C10** | chat | B `CM_CHAT_MESSAGE_PUBLIC(10, text)`; C (another legion) stands beside them at Pauton |
| **C11** | history | A `CM_LEGION_HISTORY(0, LEGION)`, `(0, WAREHOUSE)`; B `(0, REWARD)` |
| **C12** | seeded legion and emblem | C's enter-world burst of C1 is read; C `CM_LEGION_HISTORY(0, LEGION)`; C opens the legion warehouse at Pauton and closes it; `CM_LEGION_MODIFY_EMBLEM(id, 5, DEFAULT, a, r, g, b)`; `CM_LEGION_UPLOAD_INFO(N = 9,000)` + `CM_LEGION_UPLOAD_EMBLEM` chunks of 4,500 + 4,500; B `CM_LEGION_SEND_EMBLEM(C's legion)` and `CM_LEGION_SEND_EMBLEM_INFO(C's legion)` |
| **C13** | studio by quest | A quits; seed Parrine + quest; A enters; `CM_SHOW_DIALOG(830069)`, `CM_DIALOG_SELECT(830069, 23, 18802)` |
| **C14** | studio by fee | C quits; seed; C enters; `CM_DIALOG_SELECT(830069, 96)`; A `CM_DIALOG_SELECT(830069, 96)` |
| **C15** | enter | A: **walks (`CM_MOVE` steps at run speed) from the Parrine spot to the oracle's entrance spot** (~20 m; the Parrine spot is 20.2 m from 730517, out of its 6.6-m range), stops; `CM_SHOW_DIALOG(730517)`; **holds still ≥ 2.5 s** (the 2-s use bar; any move aborts it); `CM_TELEPORT_ANIMATION_DONE` after `SM_TELEPORT_LOC` (A-14); `CM_LEVEL_READY`. Then C the same |
| **C16** | decorate | A, standing at its arrival point: `CM_HOUSE_EDIT(1)`; `(3, bed)`; `(5, bed, x, y, z, rot)`; `(3, cake)`; `(5, cake, …)` **with x, y, z taken from A's own position (≤ 1 m away, inside the cake's 3.25-m range)**; `(3, wallpaper)`; `CM_HOUSE_DECORATE(decor, 0, inwall line)`; `CM_HOUSE_EDIT(6, bed, x', y', z', rot')`; `(2)` |
| **C17** | use | A, not moving: `CM_USE_HOUSE_OBJECT(cake)`, wait; again at once; again after 10 s. C places its cake at its own position the same way, uses it and sends `CM_RELEASE_OBJECT` after 1 s |
| **C18** | configure | A `CM_HOUSE_SETTINGS(CLOSED, 0, notice)`; `CM_HOUSE_SCRIPT(2001, 0, S1)` (a valid compressed script whose `uncompressedSize` matches, PlayerScripts.java:44-57); `CM_HOUSE_SCRIPT(3001, 1, S2)` (foreign address, **another script id and another content**); the gate reads `house_scripts` at once; `CM_HOUSE_KICK(1)` |
| **C19** | leave, destroy, re-enter, persist, re-login | A **walks from the arrival point to the oracle's exit spot** (≤ 11.25 m of 830229; the arrival point is 12.2 m away), stops; `CM_SHOW_DIALOG(830229)`; holds still ≥ 2.5 s; the handshake; arrives in Oriel. **Waits for the instance destroy**: the `Destroying …` gs_log line (InstanceService.java:94) with a **75-s deadline** (the checker runs every 60 s, A-15), then **polls `player_registered_items` for ≤ 5 s**. Walks from the Oriel exit point to the entrance spot (10.7 m); `CM_SHOW_DIALOG(730517)`, still ≥ 2.5 s, handshake, level ready (a new instance). A `CM_QUIT` **inside the studio**; read the database; A logs in again (A-15: it lands in the studio) |
| **C20** | the member list sees the studio | B relogs (Pauton) |
| **C21** | leave and kick | B `0x02`; A re-invites B, B accepts; A `0x04(B)` (**A-23**: the kick looks B up by name) |
| **C22** | disband | A quits; seed Pauton (+ `world_owner = 0`); A enters, opens the warehouse, `CM_LEGION_WH_KINAH(1000, 0)`, `CM_MOVE_ITEM(item, 3 → 0, −1)`, closes; quits; seed Losadis; enters; `CM_DIALOG_SELECT(203806, 6)` + yes; `CM_DIALOG_SELECT(203806, 7)` + yes; `6` + yes again; wait 6 s; A quits and re-enters |
| **C23** | reports and shutdown | the M5a report bar plus the M5h rows; the stop file with characters online |

### 10.3 Assertions

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **Y1** | C2 | A receives, in this order: the kinah update to **0** (A-01); `SM_LEGION_INFO` (name, level 1, permissions 0x1E0C/0x1C08/0x1800/0x800, contribution 0, disband 0, the empty announcement stop string); **one `SM_LEGION_MEMBERLIST` with first = 1 and count 0**; `SM_LEGION_ADD_MEMBER(A, rank 0, isNew 0, msg 1300260, A's name)`; `SM_LEGION_UPDATE_EMBLEM`; `SM_LEGION_EDIT(0x08)`; `SM_LEGION_UPDATE_TITLE(A, id, name, 0)`; two `SM_LEGION_HISTORY` (each carries page 0 of the list: first total 1, [CREATE id 0]; then total 2, [JOIN id 1 with A's name, CREATE]; type LEGION); `STR_GUILD_CREATED(name)`. Database: `legions` (level 1, the four permissions), `legion_members` (A, BRIGADE_GENERAL), two `legion_history` rows | **Proves:** `canCreateLegion` at the exact price (A has exactly 10,000), the `Legion` constructor, `addLegionMember`, the empty member-list split, history. **Cannot prove:** that the client draws the legion window; the ranking position (the ranking cache is empty) | rank VOLUNTEER for the creator (byte 4); a kinah check `<=` (refused at exactly 10,000); history appended instead of prepended (the second packet lists CREATE first) |
| **Y2** | C3 | `STR_GUILD_CREATE_SAME_GUILD_EXIST`, `STR_GUILD_CREATE_INVALID_GUILD_NAME`, `STR_GUILD_CREATE_NOT_ENOUGH_MONEY`, each alone; B's kinah and the `legions` table unchanged | **Proves:** the refusal order and `isFreeName`. **Cannot prove:** `isForbidden` (the gate knows no forbidden name — L-06/S-07) | `isFreeName` inverted; a kinah check `>` |
| **Y3** | C4 | B: `SM_QUESTION_WINDOW(STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION, params [name, "1", A])`; A: `STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM(B)`. After yes: B gets `SM_LEGION_INFO`, **`SM_LEGION_MEMBERLIST` with exactly A** (count −1: first and last), `SM_LEGION_ADD_MEMBER(B, rank 4, 1300260)`, `SM_LEGION_EDIT(0x08)`, `SM_LEGION_HISTORY(JOIN B)`; A gets the same `ADD_MEMBER`, `EDIT` and `HISTORY`, and `SM_LEGION_UPDATE_TITLE(B, id, name, 4)` (they stand together). The C invite: `STR_GUILD_INVITE_HE_IS_OTHER_GUILD_MEMBER(C)`; B again: `…_HE_IS_MY_GUILD_MEMBER(B)` | **Proves:** the anonymous handler, `addToLegion`, the broadcast to the right set, the joiner excluded from its own list. **Cannot prove:** the deny arm (C is a member, so no question is asked — S-07) | the joiner not excluded (count −2); `broadcastToLegion` sending to one member only |
| **Y4** | C5 | B: `STR_GUILD_WRITE_NOTICE_DONT_HAVE_RIGHT` and nothing else; A: `STR_GUILD_WRITE_NOTICE_DONE`; **both** `SM_LEGION_EDIT(0x05, text[0:256], t)`; `legion_announcement_list` holds the 256-character text; B's `0x07`: `STR_GUILD_NOTICE(text[0:256], t)` | **Proves:** `hasRights(EDIT)` for a volunteer (0x800 has no 0x200), the 256 truncation, the DAO. **Cannot prove:** the clear arm (S-07) | truncation at 255 or none; `hasRights` true for everyone |
| **Y5** | C6 | both: `SM_LEGION_UPDATE_MEMBER(B, rank 2, msg 1300267, B)`; both: `SM_LEGION_EDIT(0x02, 0x1E0C, 0x1E08, 0x1800, 0x800)`; **B's second `0x09` now succeeds**; B's `0x0D`: `STR_GUILD_CHANGE_RIGHT_DONT_HAVE_RIGHT`; both: `SM_LEGION_UPDATE_SELF_INTRO(B, intro)`, B: `STR_GUILD_WRITE_INTRO_DONE`; both: `SM_LEGION_UPDATE_NICKNAME(B, "nick")` | **Proves:** the rank → permission column mapping of `hasRights`, live permission edits, **the name lookup through `getOrLoadPlayerCommonData` (A-23)**. **Cannot prove:** the deputy and legionary columns (L-06's table); the offline-name arm (S-09's or M5c's test) | `hasRights` reading the legionary mask for a centurion (B's second `0x09` refused); the name lookup left unported (C6 throws: ERROR + `unported_trace`) |
| **Y6** | C7, C9 | C7 (A holds 0): `STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MONEY`; C9 (A holds 101,000): `STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MEMBER` (2 < the shipped 6); kinah and level unchanged both times | **Proves:** the check order of LegionService.java:918-948 (money before members), `getKinahPrice(1)` = 100,000 and `hasRequiredMembers` at the shipped config. **Cannot prove:** a successful level-up (six members; L-06) | the member check before the money check (C7 answers `…NOT_ENOUGH_MEMBER`); the member check skipped (level 2 needs 0 contribution points, so the legion would reach level 2 and 100,000 kinah would go) |
| **Y7** | C10 | A receives `SM_MESSAGE(B, type 10, text)`; **C receives nothing** though C stands beside them | **Proves:** the legion arm routes by membership, not by distance. **Cannot prove:** the block list or the filter (normal chat's) | a broadcast to the known list |
| **Y8** | C8 | A: `SM_LEGION_UPDATE_MEMBER(B, online 0, lastOnline > 0)` on B's quit; on B's return A gets `STR_MSG_NOTIFY_LOGIN_GUILD(B)` and `SM_LEGION_ADD_MEMBER(B, isNew 1, msg 0, "")`, B gets `SM_LEGION_INFO` with the announcement, a member list of A and B, `STR_GUILD_NOTICE`. B's kill: **the experience delta equals the oracle's kill experience exactly** (2 online, no bonus), and **A receives `SM_LEGION_UPDATE_MEMBER(B, level 2, Poeta's map id)`** | **Proves:** `onLogin`/`onLogout`, **`Legion::hasBonus` answering false on the experience path**, `updateMemberInfo` from a level change across maps. **Cannot prove:** the bonus itself (10 online; L-06) | `hasBonus` true (× 1.1); `updateMemberInfo` sent only to the member |
| **Y9** | C9 | A: `SM_LEGION_EDIT(0x04, 0)`, **exactly one `SM_WAREHOUSE_INFO` (storage 3, first = 1, no items)** — the empty warehouse yields no part, only the closing packet (§2.3 row 2) — and `SM_DIALOG_WINDOW(page 25)`. B: `STR_GUILD_WAREHOUSE_IN_USE`. A's deposit: kinah 101,000 → 100,000, legion kinah +1,000, `SM_LEGION_HISTORY(WAREHOUSE, KINAH_DEPOSIT, A, "1000")`; the item: its delete, `SM_WAREHOUSE_ADD_ITEM(storage 3)`, `SM_LEGION_HISTORY(ITEM_DEPOSIT, "itemId:count")`. After A closes, B opens and sees `SM_LEGION_EDIT(0x04, 1,000)` and **two** `SM_WAREHOUSE_INFO` — the part (first = 1, the item) and the closing one (first = 0); B's withdrawal: `STR_GUILD_WAREHOUSE_NO_RIGHT` (a centurion has no 0x4). After both quit: `inventory` rows owned by the legion id with location 3 | **Proves:** the in-use compare-and-set and its release on close (A-06), the proxy, the permission split, history, `LegionWhUpdate`. **Cannot prove:** the periodic save (not in the window) | `setInUse` always true (B gets in); close not releasing (B refused after A closed) |
| **Y10** | C11 | `SM_LEGION_HISTORY(LEGION)`: total and entries **equal the list the gate recorded from its own broadcasts**, newest first, 32-character fields; `(WAREHOUSE)`: the two deposits; B's REWARD request: no packet | **Proves:** prepend order, per-type lists, the REWARD guard. **Cannot prove:** the 365-day trim (L-06) | append order (reversed) |
| **Y11** | C1, C12 | **First, C's second character list (C1): C's entry carries the seeded legion's id and name and the member flag 1** (AbstractPlayerInfoPacket.java:111-113) — the load path's first observable, inside `SM_CHARACTER_LIST`'s `writeImpl`; A's and B's entries carry 0. C's enter world (C1): `SM_LEGION_INFO(level 3, contribution 12,345, the seeded announcement)`, a member list of C, `STR_GUILD_NOTICE`; C's history request lists **the seeded history row**; C's warehouse: `SM_LEGION_EDIT(0x04, 5,000)` and **two** `SM_WAREHOUSE_INFO` — the part with **the seeded item** (first = 1; the database load of `LegionWarehouse` through `Storage.onLoadHandler`) and the closing one (first = 0). Stock emblem, **in Java's order** (LegionService.java:472-476): `SM_LEGION_HISTORY(EMBLEM_MODIFIED)`, then kinah −price, `SM_LEGION_UPDATE_EMBLEM(id, 5, 0x00, colors)`, `STR_GUILD_CHANGE_EMBLEM`. Upload: after the second chunk only, **in Java's order** (:576-584) — kinah −price, `SM_LEGION_HISTORY(EMBLEM_REGISTER)`, `SM_LEGION_UPDATE_EMBLEM(…, 0x80)`, then to C `SM_LEGION_SEND_EMBLEM(id, dataSize 9,000)`, **`SM_LEGION_SEND_EMBLEM_DATA(7,993)` and `(1,007)` whose bytes equal the upload**, and **last** `STR_GUILD_WARN_SUCCESS_UPLOAD_EMBLEM`. B's request: the same three packets with the same bytes; `…_INFO`: `SM_LEGION_SEND_EMBLEM` with size 0 alone. `legion_emblems` holds the 9,000 bytes | **Proves:** the whole load path (constructor, members, announcement, emblem, warehouse, history) **from the character list**, both emblem paths, the chunking, cross-legion reads. **Cannot prove:** that the client renders the image | chunks of 7,992; the price taken per chunk; the load path skipping `loadLegionInfo`; a `computeIfAbsent` that cached `SM_CREATE_CHARACTER`'s null (C's entry shows no legion, and no `SM_LEGION_INFO` follows) |
| **Y12** | C13 | before: `SM_HOUSE_OWNER_INFO(0, 0, state 2)`; after the select: `STR_MSG_HOUSING_INS_OWN_SUCCESS`, `SM_HOUSE_OWNER_INFO(2001, 355000, state 5)`, `SM_HOUSE_ACQUIRE(A, 2001, 1)`, then the quest completion (A-13); A's kinah unchanged; `houses` row (2001, A, 355000). A is a legion member: the quest exp levels A up → **B receives `SM_LEGION_UPDATE_MEMBER(A, new level)`** | **Proves:** the Q05 handler's reward arm, `registerPlayerStudio` (free), `changeOwner` for a new studio. **Cannot prove:** the chain's starts and steps (18832 → 18801 → 18802; H-03's tests and checklist step 10) | `recreatePlayerStudio` instead (`STR_NOT_ENOUGH_MONEY`); `notifyAboutOwnerChange` skipped (no `SM_HOUSE_ACQUIRE`) |
| **Y13** | C14 | C: kinah **0**, `STR_MSG_HOUSING_INS_OWN_SUCCESS`, `SM_HOUSE_ACQUIRE(C, 2001, 1)`; A: `STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE`, kinah unchanged; two `houses` rows | **Proves:** the fee at exactly the land price and the refusal before the charge. **Cannot prove:** the Asmodian studio (3001; unit test) | charging before the ownership check (A loses kinah) |
| **Y14** | C15 | each owner, following m5f-plan.md T13's pattern: after `CM_SHOW_DIALOG(730517)` **`SM_USE_OBJECT(owner, 730517's object, 2000, 1)` + `SM_EMOTION(START_QUESTLOOT)`; 2 ± 0.5 s later `SM_EMOTION(END_QUESTLOOT)` + `SM_USE_OBJECT(…, 2000, 2)`**; then `SM_TELEPORT_LOC(1 = FADE_OUT_BEAM, 720010000, …)` to the address point of 2001 (`houses.xml:619`) and **nothing further until `CM_TELEPORT_ANIMATION_DONE`**; then `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN(720010000)` with an instance id **≠ 1 and ≠ the other owner's**, and **no** `STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF` (a personal map, TeleportService.java:531-532); **A and B receive `SM_LEGION_UPDATE_MEMBER(A)` with map 720010000** (the map-change `updateMemberInfo`, §2.5 row 4), C its own; after `CM_LEVEL_READY` exactly **one** `SM_HOUSE_RENDER` (address 2001, its owner), `SM_NPC_INFO` for 810021 and 810003 at the `house_npcs.xml` spots of address 2001, **and the butler's `SM_HOUSE_SCRIPTS(2001)` with 8 empty slots** (`ButlerAI.handleCreatureSee` → `PlayerScripts.sendToPlayer`, H-01's observable); no npc of the other studio. `gs_log`: "Created new instance: 720010000 [id] owner:<owner id>" | **Proves:** `studioportal` + `ActionItemNpcAI`'s timed path (A-11), `getOrCreateHouseInstance`, one personal instance per owner, `spawnStudio`, `HouseController.onAfterSpawn` for a studio (first run), `onLeaveInstance` on the way in (A-24), the live `butler` AI. **Cannot prove:** the door geometry (§10.5) | a personal instance keyed on the map only (A and C share it); the use bar skipped (the teleport at once, no `SM_USE_OBJECT`); the butler left a `DummyNpcAI` (no `SM_HOUSE_SCRIPTS`) |
| **Y15** | C16 | `SM_HOUSE_EDIT(1)`, `SM_HOUSE_REGISTRY(1)`, `(2)` with 0 entries; per item: its delete (REGISTER), `SM_HOUSE_EDIT(3, 1 \| 2, objId, template)` — **for the cake with A's id and the `UseableItemObject` usage block** (SM_HOUSE_EDIT.java:74-78) and **`secondsUntilExpiration` inside the oracle's 30-day window**, for the bed 0 and no block; per spawn: `SM_HOUSE_EDIT(5, 2001, A, objId, template, x, y, z, rot)` **with the sent floats bit-exact** (the cake's again with its usage block, :99-102), `SM_HOUSE_OBJECT(objId)` (type byte from the template; the cake's `use_item` arm with its usage data, SM_HOUSE_OBJECT.java:50-56), `SM_HOUSE_EDIT(4, 1, objId)`; the decoration: **two** `SM_HOUSE_EDIT(4, 2, decor)` and `SM_HOUSE_UPDATE` whose inner-wall part is 3554000; the move: `SM_HOUSE_EDIT(7, 0, bed)`, `SM_DELETE_HOUSE_OBJECT(bed)`, `SM_HOUSE_EDIT(5, … x', y', z', rot')`, `SM_HOUSE_OBJECT(bed)` | **Proves:** `HouseObjectFactory` (H-04) for a `use_item` template, the expiry from `use_days`, the registry and decorations (`DecorateAction::getTemplateId`), `obj.spawn` visibility (first run). **Cannot prove:** the client's placement rules (the server does not check them); **the chair arm of `createNew`** — a bed built as a `PassiveObject` is byte-identical in every packet here (the type byte comes from the template, SM_HOUSE_OBJECT.java:50-51, and `ChairObject.onUse` is empty), so only H-07's factory test catches it | `createNew` building a `PassiveObject` for every template: **the cake's `SM_HOUSE_EDIT(3)`/`(5)` lose the usage block** (`instanceof UseableItemObject` fails) and its `SM_HOUSE_OBJECT` cannot write the `use_item` arm; the expiry not set (0 for the cake); a decoration applied without `setUsed` |
| **Y16** | C17 | (A stands ≤ 3.25 m from its cake, so no `STR_MSG_HOUSING_OBJECT_TOO_FAR_TO_USE`.) A: `STR_MSG_HOUSING_OBJECT_USE`, `SM_USE_OBJECT(A, cake, 3000, 8)`; **not before 2,900 ms**: `SM_USE_OBJECT(A, cake, 0, 9)`, the reward 160010196 added, `STR_MSG_HOUSING_OBJECT_REWARD_ITEM`, `SM_OBJECT_USE_UPDATE(A, A, 1, cake)`. At once again: `STR_MSG_HOUSING_CANNOT_USE_FLOWERPOT_COOLTIME`. After 10 s: `STR_MSG_CANNOT_USE_ALREADY_HAVE_REWARD_ITEM` (COOKING). C's cancel at 1 s: `SM_USE_OBJECT(C, cake, 0, 9)`, `STR_MSG_HOUSING_OBJECT_CANCEL_USE`, **no reward in the next 3 s** | **Proves:** `UseableItemObject.onUse` and its task (first run), the cooldown table, the COOKING limit (H-05), `releaseOccupant`. **Cannot prove:** the final reward at use 21 (unit test) | the COOKING check skipped (a second reward); cancel not cancelling the task (C gets the reward) |
| **Y17** | C18 | settings, **in Java's order** (CM_HOUSE_SETTINGS.java:57-67; HouseController.java:106-122): `SM_HOUSE_ACQUIRE(A, 2001, 1)`, `SM_HOUSE_UPDATE`, **`STR_MSG_HOUSING_ORDER_OUT_ALL`** (from `kickVisitors(player, true, false)`), then `STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL`. The valid script: **no packet to A** — the `SM_HOUSE_SCRIPTS` broadcast goes only to players who know A, and nobody else is in A's studio (CM_HOUSE_SCRIPT.java:62; PacketSendUtility.java:98-100); **`house_scripts`, read at once, holds exactly script 0 for A's studio with S1's decompressed XML** (stored immediately, PlayerScripts.java:50-55). The foreign script: no packet, **no row with script id 1 and none with S2's content**. Kick: `STR_MSG_HOUSING_ORDER_OUT_WITHOUT_FRIENDS` | **Proves:** the settings arm, the audit guard, the immediate script store. **Cannot prove:** that a script reaches another player (nobody else inside — P-05's run test); the owner's own copy (Y18, through the butler); kicking a visitor (M5h-2's visiting) | the audit guard missing (a row with id 1 and S2's content); `SM_HOUSE_SCRIPTS` sent to the sender (an extra packet at A); the `OUT_ALL` message dropped or after `CLOSE_DOOR_ALL` |
| **Y18** | C19 | **The walk to the exit spot: no `SM_USE_OBJECT` and no `SM_EMOTION(END_QUESTLOOT)`** — C15's use-bar observer was removed when its task ran (ActionItemNpcAI.java:68). The exit: the 2-s bar at 830229 as in Y14; `SM_TELEPORT_LOC(FADE_OUT_BEAM, 700010000, …, 2573, 1961, 185)`; after the handshake `SM_PLAYER_SPAWN(700010000)` in instance 1; **no `STR_MSG_LEAVE_INSTANCE`** (the personal instance has no registration, InstanceService.java:213); A and B: `SM_LEGION_UPDATE_MEMBER(A)` with map 700010000. **The destroy**: the `Destroying …` line within 75 s, and within 5 s after it — **before any re-entry or logout** — `player_registered_items` holds the bed at (x', y', z'), the cake and the wallpaper (DECOR, room 0) (nothing wrote the registry before, §2.7 row 10). **Re-entry**: a new instance id (≠ 1, ≠ the destroyed one); `SM_HOUSE_OBJECT` for the bed **at x', y', z'** and the cake, the wallpaper in `SM_HOUSE_RENDER`; **the butler's `SM_HOUSE_SCRIPTS(2001)`: slot 0 carries S1's compressed bytes byte-equal plus the 8 padding bytes, slot 1 is empty** (SM_HOUSE_SCRIPTS.java writeImpl). **After `CM_QUIT`**: `houses` has the settings and the notice, `house_scripts` one row (id 0), `house_object_cooldowns` the cake. **Re-login (A-15)**: A's enter world places A in 720010000 with an instance id ≠ 1 (the still-live one, or a new one whose spawn runs `spawnHouses`), and the studio re-renders (`SM_HOUSE_RENDER`, `SM_HOUSE_OBJECT` bed and cake, `SM_HOUSE_SCRIPTS`) | **Proves:** the use-bar observer's lifecycle, `onLeaveInstance` on the way out (A-24), `HouseController.onDespawn` saving a reusable studio (first run), re-spawn from the in-memory registry, the stored script reaching its owner, the logout store, `onPlayerLogin`'s personal arm. **Cannot prove:** the database → registry load through `createNew(registry, …)` (a restart; H-07 and checklist step 12) | the observer not removed (a spurious `SM_USE_OBJECT(…, 0, 2)` + `END_QUESTLOOT` on the walk); `onDespawn` not saving (0 rows after the destroy); `resetRegistry` on re-entry (empty studio); the login moved to the exit point (A in Oriel) |
| **Y19** | C20 | B's `SM_LEGION_MEMBERLIST` entry for A carries **address 2001 and door state 3 (CLOSED)** | **Proves:** `SM_LEGION_MEMBERLIST.java:46-48`'s housing arm | – |
| **Y20** | C21 | the leave, **in Java's order** (LegionService.java:95-101, 714-741): A first gets **`SM_LEGION_HISTORY(LEGION)` headed by a `KICK` entry with B's name** (a leave is recorded as `KICK`), then `SM_LEGION_LEAVE_MEMBER(1300240, B, B's name, legion name)`; B (no history — already removed from the member ids): `SM_LEGION_LEAVE_MEMBER(1300241, 0, legion name)`, `SM_LEGION_UPDATE_TITLE(B, 0, "", 2)`. The re-invite as Y3. The kick (**A-23**: by name): A gets `SM_LEGION_HISTORY(LEGION)` headed by a second `KICK` B, then `SM_LEGION_LEAVE_MEMBER(1300247, B, A's name, B's name)`; B: `SM_LEGION_LEAVE_MEMBER(1300246, 0, legion name)`, `SM_LEGION_UPDATE_TITLE(B, 0, "", 4)`; `legion_members` without B; `legion_history` with the two `KICK` rows | **Proves:** both arms of `removeLegionMember` incl. `deleteLegionMemberFromDB`'s history, the name lookup for a kick, and the P5-12b lease (it runs with the system disabled). **Cannot prove:** the bonus-icon arm (10 online) | the kicker name ignored (1300240/1300241 instead); the history skipped for a leave, or broadcast before the member is removed (B gets it too) |
| **Y21** | C22 | the withdrawals: `SM_LEGION_HISTORY(WAREHOUSE, KINAH_WITHDRAW, A, "1000")` and `(ITEM_WITHDRAW)`, A's kinah +1,000; then the question `STR_GUILD_DISPERSE_STAYMODE`; after yes: `SM_LEGION_UPDATE_MEMBER(A, 1300303, t)` and `SM_LEGION_EDIT(0x06, t)` with **t = now + 5 ± 1**; recreate: the question `STR_GUILD_DISPERSE_STAYMODE_CANCEL`, after yes **`SM_LEGION_EDIT(0x07)`, `SM_LEGION_UPDATE_MEMBER(A, 1300307, "")`, `SM_LEGION_EDIT(0x07)`** (the broadcast, then the per-member pair, LegionService.java:462-467, 504-505); disband again, 6 s, relog: **no `SM_LEGION_INFO` in A's enter-world burst**, `legions` and `legion_members` rows gone | **Proves:** the whole disband lifecycle including the lazy `checkDisband` and the P5-12a lease. **Cannot prove:** the online members' `updateAfterDisbandLegion` (nobody else is online in the legion; S-07) | `deleteLegionFromDB` skipped; a disband time from the Java default |
| **Y22** | C23 | the M5a report bar: `unported_trace.txt` empty, census empty, lockdep empty, no watchdog dump, **no ERROR in either log**; `partial_trace.txt` ⊆ the allow-list with §A hit ≥ 1 and §B hit exactly 0. (A run crossing Wednesday 09:00 exercises S-08's ported calculation instead of throwing; before M5h lands, every earlier gate crossing it fails this bar falsely — §9 risk 9.) | **Proves:** nothing on the scripted path fell outside the first cut. **Cannot prove:** paths off the script (D9 makes them fail loudly) | any unported body on the path |
| **Y23** | C23 | `live_counts.txt`: every G-07 class live 0 after shutdown with `created > 0` | **Proves:** the teardown releases legions, members, house objects and house npcs. **Cannot prove:** a mid-run leak of a destroyed studio's npcs (H-07's `onDespawn` case) | a `House.spawns` never cleared |

### 10.4 Mutation proof (the minimum set)

| Mutation | Must fail | Must stay green |
|---|---|---|
| `LegionRestrictions::canCreateLegion`: `<` → `<=` on the kinah | **Y1** (A refused at exactly 10,000) | Y2 |
| `LegionService::updateLegionMemberList`: ignore `excludedPlayerId` | **Y1**, **Y3** (count −1 / −2) | Y4-Y21 |
| `LegionMember::hasRights`: use the legionary column for a centurion | **Y5** | Y1-Y4 |
| `LegionService::changeAnnouncement`: no truncation | **Y4** | everything else |
| `Legion::hasBonus`: return true | **Y8** (exp × 1.1) | Y1-Y7 |
| `LegionWarehouse::unsetInUse`: return false without resetting | **Y9** (B refused after A closed) | Y1-Y8 |
| `Legion::addHistory`: append | **Y1** (the second history packet lists CREATE first), **Y10** | Y2-Y9 |
| `LegionService::sendEmblemData`: chunk at 7,992 | **Y11** | Y1-Y10 |
| `HouseObjectFactory::createNew`: always `PassiveObject` | **Y15** (**the cake's** `SM_HOUSE_EDIT(3)`/`(5)` without the usage block, its `SM_HOUSE_OBJECT` `use_item` arm), **Y16** (the cake does nothing on use) | Y12-Y14 |
| `HouseObjectFactory::createNew`: `PassiveObject` for a chair template only | **nothing in the gate** — the bed's packets are byte-identical (the type byte is the template's; `ChairObject.onUse` is empty). **H-07's factory test must catch it** | all |
| `UseableItemObject` `placementLimitOf`: always NONE | **Y16** (a second reward) | Y15 |
| `HouseController::onDespawn`: skip `save()` | **Y18** (`player_registered_items` still empty after the destroy, read before the re-entry and the logout) — *not* by the re-entry or the logout, which see the in-memory registry and the logout store | Y14-Y17 |
| `ActionItemNpcAI` timed path: leave the observer registered after the task | **Y18** (C19's walk to the exit spot sends `SM_EMOTION(END_QUESTLOOT)` + `SM_USE_OBJECT(…, 0, 2)` for the entrance) | Y14 |
| `ActionItemNpcAI::handleUseItemStart`: skip the bar (call `handleUseItemFinish` at once) | **Y14** (no `SM_USE_OBJECT(…, 2000, 1)`, the teleport before 1.5 s) | Y12-Y13 |
| `CM_HOUSE_SCRIPT`: drop the audit guard | **Y17** (`house_scripts` gains a row with id 1 and S2's content) | Y14-Y16 |
| `CM_HOUSE_SCRIPT`: broadcast with `toSelf` | **Y17** (an `SM_HOUSE_SCRIPTS` at A) | Y14-Y16 |
| `InstanceService::getOrCreatePersonalInstance`: ignore the owner | **Y14** | Y12-Y13 |
| `InstanceService::onPlayerLogin`: always `moveToExitPoint` for a personal map | **Y18** (the re-login lands in Oriel) | Y1-Y17 |
| `LegionService::removeLegionMember`: history after the leave broadcasts | **Y20** (the `KICK` history after `SM_LEGION_LEAVE_MEMBER`) | Y1-Y19 |
| `LegionService::disbandLegion`: skip `deleteLegionFromDB` | **Y21** | Y1-Y20 |
| `LegionDominionService::startWeeklyCalculation`: skip `reset` | **nothing in the gate** (no run crosses Wednesday 09:00 by design). **S-07's S-08 cases must catch it** | all |
| `Legion::addBonus`: threshold 9 instead of 10 | **nothing in the gate** — at most 3 are online. **L-06 must catch it** | all |
| `Legion::addHistory`: trim LEGION entries too | **nothing in the gate** — every entry is seconds old. **L-06 must catch it** | all |
| `PlayerRegisteredItemsDAO::loadRegistry`: drop decorations | **nothing in the gate** — no restart. **H-07's round trip must catch it** | all |

### 10.5 The geo gate

`gs.scenario.m5h_geo` runs the same script with `gameserver.geodata.enable = true`, `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`, the same lock.
**G-04 first establishes what geo can see.** Candidates: `GeoService::setHouseDoorState` → `GeoMap::setHouseDoorState(instanceId, address, state)`
(`GeoMap.cpp:314-318`) sets the studio door's per-instance activity at spawn and on `CM_HOUSE_SETTINGS` — geo-off it is a no-op, geo-on it
runs against 720010000's geometry **for an instance id created at run time**; nothing a fake client reads depends on it. m5b-plan.md §6.4 is the
precedent for saying so and keeping the geo gate as a re-run whose extra assertion is "no ERROR, no exception" with real geometry, rather than
inventing a row that cannot fail.

---

## 11. Real-client checklist (user, after stage 2)

Prerequisites as m5b-plan.md §10 steps 1-6 with `mygs.properties` from `m5h.properties.example`. Positions, levels and kinah come from
`oracle.py m5h-housing --checklist-sql` (G-01); run the SQL with the server stopped or the characters offline.

1. With two characters on two accounts, both Elyos and at the Losadis spot in Sanctum: talk to *Losadis*, choose **Create Legion**, name it.
   The legion window opens, 10,000 kinah is gone, the name shows over your head.
2. Invite the second character (legion window or `/invite`-style command): the second client gets a question; accept. Both lists show both
   members, online.
3. Write an announcement; the other client sees it at once. Type `/gnotice`. Relog the second character: the announcement greets it.
4. Promote the second to Centurion, give Centurions the "edit announcement" right, set a nickname and your self introduction. *(The promotion
   and the nickname look the member up by name: they need A-23 or S-09.)*
5. Legion chat: both see each other's messages; a third, non-member character standing next to you does not. *(Needs A-19 or P-03.)*
6. Walk to *Pauton*: open the legion warehouse, deposit kinah and an item; the second character is refused while you have it open and gets in
   after you close it. The history tab shows the entries.
7. Emblem: with the SQL raising the legion to level 2 (server stopped), change to a stock emblem at *Inofe*; at level 3 upload a custom emblem.
   The second client sees it on your character without asking — its client fetches it on its own (`CM_LEGION_SEND_EMBLEM`).
8. Hand the brigade general role to the second character (two questions), kick and re-invite, leave.
9. Disband at *Losadis*, then recreate (cancel) it.
10. **Studio.** With the SQL making a character level 21 in Oriel, **the whole chain** (D4): *[Housing] Imagining A Quiet Life* (18832) from
    830365 → report to 830001 (a quest movie plays) → *[Housing] Heart of Rock* (18801, XML) from 830001 → report to *Celaeno* (830005) →
    *And A Home for Every Daeva* (18802) from *Celaeno* → *Parrine* (830069) → choose the studio (no reward item): the studio is yours, free.
    **Shortcuts by SQL** (`--checklist-sql`), if a step of the chain misbehaves: `player_quests(18801, COMPLETE)` so *Celaeno* offers 18802
    at once, or `(18802, START)` to go straight to *Parrine*. **The paid option**: *Parrine* shows the buy button **only when 18802 is
    COMPLETE** (the client reads `BIDDING_ALLOWED` in `SM_HOUSE_OWNER_INFO`, §2.7 row 2) — to try it, seed `(18802, COMPLETE)` with no studio
    and 4,000,000 kinah. Then **walk up to the *studio entrance*** (~20 m from *Parrine*; within ~6 m to talk to it) and **stand still for the
    2-s bar** — moving cancels it.
11. Inside: the butler greets you. Place furniture from your cube (house edit mode), change the wallpaper, sit in a chair, use a cooking object
    (stand within ~3 m of it), open the butler's settings and scripts.
12. Leave through the *studio exit* (step toward it first: the arrival point is ~12 m away, the exit talks at ~11 m; stand still for the bar),
    come back: everything is where you left it. **Log out inside the studio and back in: you are still inside** (§2.7 row 11). **Restart the
    server and come back**: the same — this is the step no test runs (Y18's "cannot prove"); after the restart, the first member of your legion
    to log in reads the other, uncached member through `getOrLoadPlayerCommonData` (A-23/S-09).
13. **Things this milestone does not do, expected to be refused or ignored with a warning, not to crash**: the relationship crystal (it has no
    AI yet), the butler's auction and rent options, any land house in Oriel or Pernon (they render; their signs and butlers talk but buying
    one is M5h-2), the town bulletin board, the Stonespear Reach npcs. Note any id in `unported_trace.txt` — they are M5h-2's and M5i's entry
    list.
14. If the session crosses **Wednesday 09:00**, the Legion Dominion weekly calculation now runs (S-08): no ERROR is expected — with no
    participants (no legion can join before M5i) it only resets and stores each location and broadcasts `SM_LEGION_DOMINION_LOC_INFO`.
15. Send `game-server/log/`, the summary, `live_counts.txt`, `partial_trace.txt` and `unported_trace.txt`.

---

## 12. What was measured and what was inferred

**Measured** (grep, `chunks.py`, `javasrc.py` and ElementTree over the two trees — rev 1 at HEAD `c1edb0afb` + working tree, rev 2's new and
changed rows at HEAD `760e8ab5c` + working tree; re-runnable):

- P5-11: **86** `AION_UNPORTED` and **3** `AION_PARTIAL` over 15 `.cpp` files (per file: `LegionService` 63, `HousingBidService` 7,
  `LegionDominionLocation` 6, `LegionDominionService` 6, `Town` 2, `AuctionAutoFillTask` 1 + 1, `MaintenanceTask` 1 + 1, `AuctionEndTask` 0 + 1),
  **plus 1 dead template body in `HouseRegistry.h:78-81`** (87 by grep); 3,739 Java lines over 17 files; `LegionDominionIntruderUpdateTask` has
  no C++ file. The zero-site group of §2.8 is 1,281 Java lines (1,310 with `HouseDoorState`).
- P5-10: 294 sites, of which legion 66 (`LegionWarehouse` 28, `Legion` 27, `LegionEmblem` 6, `LegionMember` 5; 974 Java lines with
  `LegionHistoryEntry`, 959 without) and challenge 20.
- Every "0 unported" claim of §2.8 (housing model, services, controllers, DAOs, proxy, templates, 37 of 38 server packets); that
  `HouseObject::getPlacementLimit(bool)` has no Java caller.
- The 5 undeclared enum-companion bodies, the stand-ins (`PacketSupport.h:44-58` with **12 packet call sites in 12 packets**, one of them in
  P4-16, and their test `tests/sm_lz/OpcodesAndSupportTest.cpp:156-172`; `LegionDAO.cpp:57-75`, used at `:338`), the 7 anonymous-class bodies,
  **`DecorateAction::getTemplateId` (no accessor in `DecorateAction.h` or its generated block)**, `ActionItemNpcAI`'s 7 bodies, the 4 false positives.
- 190 Java client-packet files (2 abstract bases), 43 C++ (1 abstract base): **146 of 188** real packets without a file; the 23 legion/housing
  packets without a file; their opcodes (`ClientPacketInfo.gen.inc`).
- The C++ call sites of §2.5 and §2.10 (`AbstractPlayerInfoPacket.cpp:33`, `PacketLookups.cpp:58-61`, `CM_DELETE_CHARACTER.cpp:34`,
  `PlayerService.cpp:188-190, 323-329`, `LegionService.cpp:199-230`, `PlayerEnterWorldService.cpp:504, 547, 557`, `PlayerLeaveWorldService.cpp:134,
  158`, `PlayerController.cpp:712-713`, `TeleportService.cpp:146, 260-272`, `RatesInfo.cpp:49`, `PacketSendUtility.cpp:139-149`,
  `CronJobService.cpp:185-187`, `LegionDominionService.cpp:53-74`, `LegionDominionLocation.cpp:61-86`, `SpawnEngine.cpp:223`,
  `InstanceService.cpp:116-118, 133, 137-148, 153, 170-172`, `GeneralInstanceHandler.cpp:35, 121, 125`, `DialogService.cpp:27-28`,
  `SystemMailService.cpp:10-13`).
- The Java behaviour the gate now leans on: `ActionItemNpcAI`'s timed path (ActionItemNpcAI.java:41-77) and the observer's abort on a move
  (ItemUseObserver.java); `isInTalkRange` (PositionUtil.java:243-250, 306-314) with a player radius of 0.25 (PlayerAccountData.java:99);
  `CM_HOUSE_SCRIPT`'s two-argument broadcast (PacketSendUtility.java:98-100); `ButlerAI.handleCreatureSee` → `PlayerScripts.sendToPlayer` with 8
  slots; the `EmptyInstanceCheckerTask`'s 60-s fixed rate and its personal-instance rule (InstanceService.java:58, 177-180); the unregistered
  personal instance sending no leave message (:210-219); `onPlayerLogin`'s personal arm (:148-155); `HouseObject.onDespawn` empty and
  `spawn()` recreating the position; no periodic player save (PeriodicSaveService.java:33); `SplitList`'s empty-list rule; the order of
  `removeLegionMember`, `recreateLegion`, `storeLegionEmblem`, `uploadEmblemData` and `CM_HOUSE_SETTINGS`; `CM_LEGION_WH_KINAH`'s missing
  checks; `SM_HOUSE_OWNER_INFO`'s `BIDDING_ALLOWED` rule; `SM_HOUSE_OBJECT`/`SM_HOUSE_EDIT`'s type byte and `instanceof UseableItemObject`
  arms; `TeleportService`'s `updateMemberInfo` on a map change (:245-246, 534-535).
- The data of §2.7 and §2.12: 1,032 addresses by map, the two studio addresses and their land, building, price, arrival point and exits;
  3,094 house npc spawns; the npc ids, AI names, `func_dialogs`, **talk distances, delays and bound radii** of every npc named; 84 warehouse
  npcs; spot coordinates and the distances 598.7 m, 20.2 m, 231 m, **12.2 m and 14.4 m (arrival point → exits), 10.7 m (Oriel exit point →
  entrance)**; 1,520 furniture items by kind and the 7 without a template; 261 decoration items, the 103 for `CP_D`, **the one bare
  `<housedeco/>` (170000023)**; the cake's (**incl. `owner`, `use_days`, `talking_distance`**) and bed's attributes; quest 18802's level and
  rewards and **the chain 18832 → 18801 → 18802 (28832 → 28801 → 28802)**, with 18801/28801 XML `report_to` quests; the shipped
  `legions.properties` and `housing.properties`.
- The chunk owners: `_18802`/`_18832` Q05, `_28802`/`_28832` **Q09**, `StudioPortalAI` A1, `ButlerAI`/`HouseSignAI`/`ActionItemNpcAI` P5-05,
  `SM_GM_SHOW_LEGION_MEMBERLIST.cpp` P4-16, the other 11 stand-in packets and `PacketSupport.h` P4-17, `LegionDAO.cpp` P4-14.
- **The earlier plans' items this plan stands on**, read in their current drafts: m5c D-02..D-04, M-01, M-02, W-13; m5d D5/A-02, E-09, H-01,
  the `AION_QUEST_HANDLER` registry (`HandlerRegistry.h:127, 319-325`); m5e W-04; m5f N-01..N-04, N-06, T-02, P-02, W-06, O-05 (and N-03's "W,
  studios are M5h"); m5g D1 (P5-10f), D14/K-04, O-02; m5i-plan.md:94 (the cron "M5h's") and Z-01.
- That the working-tree changes at `760e8ab5c` touch no M5h file.

**Inferred — a lane should confirm before relying on it:**

- **That each earlier milestone delivers its §0 items**: every one of those plans is a draft under revision; I-03 re-checks the ids.
- **That a legion seeded after character creation is loaded by the next character list** (D8): reasoned from `writePlayerInfo` →
  `getLegionMember` → `LegionMemberDAO` → `getLegion` with an empty cache and a null `computeIfAbsent` result not stored; not run. Ids above
  `wrap_at` follow m5c D5's reasoning.
- **The size of the never-run housing code** (~1,300-1,600 Java lines, §2.10): from which methods the startup and login paths call, not from a
  trace.
- **That the fake client's walks (C15, C19) are accepted** at run speed without geo — the M5b/M5c gates' movement helpers are the model.
- **That B's kill in C8 is the experience the oracle computes** at the seeded exp (the M5b gate's monster and `m5b-monster` are the model).
- **The effort letters and agent-days**, from comparing body counts and Java lines with M5b-1's and M5b-2's lanes.
- **That the geo gate has nothing of its own to assert** (§10.5).

**Claims of the roadmap this analysis checked**: "P5-11 legion and housing 86" — **confirmed** as a count, **wrong** as a size (§4); "148 of 188"
— **146 of 188** today.

---

## 13. Open questions this analysis could not settle without building

**Settled since rev 1** (moved to "measured", §12): rev 1's item 1 — **M5f does not port `getOrCreateHouseInstance`** (m5f N-03 leaves it W,
O-05 hands studios to M5h), so H-06 is required; item 2 — **the destroy timer is the `EmptyInstanceCheckerTask`**, first run 60 s after the
instance is created and every 60 s after, destroying a personal instance at the first run that finds it empty (InstanceService.java:58,
177-180), so C19 waits ≤ ~60 s (deadline 75 s); item 3 — **the fake client must send `CM_TELEPORT_ANIMATION_DONE`**: an animated teleport arrives
only after it (m5f §2.1 row 7, T10, P-02); item 6 — **the weekly calculation is ported by M5h** (S-08, D13), not hidden behind an
`AION_PARTIAL`.

1. **Does a real client send `CM_LEGION(0x08)` (refresh) or `CM_LEGION_HISTORY` on its own at login?** If so they are on every legion member's
   enter-world path and belong in Y8.
2. **Is `SM_HOUSE_RENDER` for a studio sent once or on every `CM_LEVEL_READY`?** Y14 asserts once; a run decides.
3. **Does the stock-emblem refusal at legion level 1 really send nothing on the wire?** Java returns false without a message
   (LegionService.java:1053-1055); the client may show its own. The checklist should note what the client displays.
4. **How many house-npc AIs does M5h switch on at startup, exactly?** 1,030 butlers from the data; the owned-house signs are 0 in a fresh
   database. G-05 records the measured number.
5. **How does H-07 treat the destroyed instance a studio's objects still reference** — a documented C++ breaker or an `accepted` retention
   (§9 risk 3)? The studio lane decides with the census owner; both are faithful to what a player sees.

---

## Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 5 high, 11 medium and 4 low findings, and verified most of rev 1's counts,
citations and gate expectations (Y1-Y6, Y8, Y10, Y12-Y13, Y16, Y19-Y20's main packets). Every finding was re-checked against the Java source,
the C++ tree at `760e8ab5c` and the data before it was applied. Rev 2 applies all of them; two points are applied in a different form than the review proposed — row 19, and M5g's legion-model count
inside row 20 — and row 5's "record the owner in both plans" is met by this plan agreeing with m5i-plan.md:94, which already names M5h (this
revision may edit only this file).

| # | Sev. | Finding | Re-check | What changed |
|---|---|---|---|---|
| 1 | high | The studio portals have a 2-s use bar and a talk range; C15/C19 fail as written | **Confirmed.** 730517 `distance 5 delay 2` (`npc_templates.xml:454686`), 830228/830229 the same with radius 5 (`:533625, 533630`); `ActionItemNpcAI.handleUseItemStart`'s timed path (:41-77); `isInTalkRange` = distance + 1 + both radii with a player radius of 0.25 (PlayerAccountData.java:99) → **6.6 m** for the entrance (the review said ~6.7), **11.25 m** for the exits (~11.5); the Parrine spot is 20.2 m from the entrance, the arrival point 12.2/14.4 m from the exits; the cake's range 3.25 m | §2.7 rows 4, 7, 9 rewritten; §2.12 talk-range and exit rows; A-11 (7 bodies), A-06 (`isInteractionAllowed`); C15, C16, C17, C19 walk and hold still, the cake placed at A's position; Y14 and Y18 carry the use-bar packets (m5f T13's pattern) and the `CM_TELEPORT_ANIMATION_DONE` handshake; §10.4 adds "observer not removed" and "bar skipped"; G-01 emits the spots; H-07's bar test |
| 2 | high | Y17 expects `SM_HOUSE_SCRIPTS` at the sender; Java broadcasts only to others | **Confirmed** (CM_HOUSE_SCRIPT.java:62 — the review said :61; PacketSendUtility.java:98-100). The owner's copy comes from `ButlerAI.handleCreatureSee` → `PlayerScripts.sendToPlayer`, 8 slots, on every entry | §2.7 row 8; D7; Y17 drops the packet and asserts the `house_scripts` row content (script 0 = S1; no id 1, no S2); C18 sends the foreign script with another id and content; Y14 adds the first-entry `SM_HOUSE_SCRIPTS` (8 empty slots, H-01's observable); Y18 asserts slot 0 byte-equal on re-entry; §10.4 adds the audit-guard and `toSelf` mutations; P-05 adds the second-player case |
| 3 | high | Missed prerequisite: `PlayerService::getOrLoadPlayerCommonData` ×2 behind ported lookups | **Confirmed** (`PlayerService.cpp:323-329` U; `LegionService.cpp:199-201, 218-222`; LegionService.java:357, 418, 744; m5c M-02) | new **A-23** and fallback **S-09** (P5-00 lease); §1 finding 3, §2.2 row 1, §2.6 row 1, §2.10 two rows; S-03, S-05, C6, C21, Y5, Y20, checklist steps 4 and 12 tagged |
| 4 | high | "P5-10b" collides with M5g's D1 (P5-10b = parties; legion is P5-10f) | **Confirmed** (m5g-plan.md D1, O-02, the table at :236; `chunks.cmake:15-16`) | D3, A-18, I-01, §7, §8, X-08: **P5-10f**, `tests/team/P5-10f`; I-01 makes M5g's exact six-part split only if M5g did not |
| 5 | high | No milestone owns the Legion Dominion weekly cron body | **Confirmed** (m5i-plan.md:94 says "M5h's … not M5i's"; rev 1 said M5i; `CronJobService.cpp:185-187` armed since M5a) | **M5h takes it**: new D13 and **S-08** (5 bodies, ~100 lines, P5-11), A-25 for its mail arm, S-07 cases, §3 row, D2, O-01, §2.8, §2.10, risk 9, Y22, checklist step 14; §13 item 6 settled. This plan cannot edit m5i-plan.md; it now agrees with it, so both name M5h |
| 6 | med | Undeclared body missed: `DecorateAction::getTemplateId` | **Confirmed** (`DecorateAction.h` empty; the generated block has a private `partId`, no accessor; CM_HOUSE_EDIT.java:88-91). One bare `<housedeco/>` exists: 170000023 (`item_templates.xml:861912`) | §2.9 row (the 74th, now one of 80 with the four quest handlers); §8 additive header request; H-04; H-07 unit case; §2.12 |
| 7 | med | The first legion-loading entry point is the character list; `CM_DELETE_CHARACTER` a second | **Confirmed** (`AbstractPlayerInfoPacket.cpp:33` → `PacketLookups.cpp:58-61`; `CM_DELETE_CHARACTER.cpp:34`; AbstractPlayerInfoPacket.java:111-113) | §1 finding 1; §2.5 row 0; §2.10 two rows; D8; C1 re-requests the character list after the seed; Y11 asserts C's entry first; risk 4 (database load inside `writeImpl`) |
| 8 | med | Y9 expects two `SM_WAREHOUSE_INFO` for an empty warehouse | **Confirmed** (LegionService.java:488-491; SplitList.java:30-34) | §2.3 row 2; Y9 (one packet for A; two for B after the deposit); Y11's seeded warehouse keeps two |
| 9 | med | Two §10.4 mutation claims not killed as stated | **Confirmed** (HouseObject.java:122-128, 270-275, 298-300; no periodic player save; SM_HOUSE_OBJECT.java:50-56; SM_HOUSE_EDIT.java:74-78, 99-102). Also found: SM_HOUSE_OBJECT's `use_item` arm casts, so a passive cake cannot even be written | C19 reads `player_registered_items` right after the destroy; Y18; Y15's reasoning points at the cake's usage block; a separate "chair only" mutation row names H-07 as its only check; H-07 adds the chair/passive case |
| 10 | med | A's re-login in C19 lands in the studio, not Oriel | **Confirmed** (InstanceService.java:148-155; `Player.java:1600` records `world_owner`) | §2.7 row 11; C19 and Y18 assert the studio re-login (A-15); §10.4 row; C22's seed zeroes `world_owner`; checklist step 12 |
| 11 | med | The free-studio chain is unreachable in real play; the paid button is client-gated | **Confirmed** (quest_data.xml:46837-46848, 61820-61832; `_18832`/`_28832` Java handlers in Q05/Q09; 18801/28801 XML `report_to`; SM_HOUSE_OWNER_INFO.java:31-35; HousingService.java:263-272) | **D4 pulls `_18832` and `_28832` forward too** (the review's second option — 130 lines, 6 bodies) so the chain is playable; §2.7 rows 1-2; H-03; checklist step 10 gives the chain, the SQL shortcuts (18801 COMPLETE, 18802 START) and the paid path (18802 COMPLETE, no studio); risk 6 |
| 12 | med | `_28802` is in Q09, not Q05 | **Confirmed** (`chunks.py owner`) | Q09 lease in I-02 (with `_28832`); §4 "thirteen chunks with bodies"; §2.8, §7 |
| 13 | med | L-05 undercounts the stand-in sites and misses a P4-16 lease | **Confirmed**: 12 sites in 12 packets (11 P4-17, 1 P4-16), `LegionDAO.cpp:338`, `tests/sm_lz/OpcodesAndSupportTest.cpp:156-172` | L-05 lists every site; I-02 adds the P4-16 lease (or a local copy); §8 |
| 14 | med | The two legion lanes are ~3× M5b-1's per-lane load | **Confirmed** (m5b2-plan.md:614-615) | **D14**: stage 1 in two parts with a commit; §7 per-lane table and critical path (legion-service 9-12 agent-days by its letters, the review's own estimate — more than the 7-9 of M5b-2's K-lanes the review suggested as the budget); §1 finding 6; risk 11 |
| 15 | med | Dormant-code fixes in P5-11 housing would route through the busiest lane | **Confirmed** (`chunks.py files P5-11`) | the studio lane holds `model/house/**` and `services/HousingService.*` in both parts (I-02, §7), with its own `House*Test.cpp` in `tests/legionhouse`; risk 1 |
| 16 | med | §0 is stale: M5e/M5f/M5g plans exist and settle rows | **Confirmed** (m5f N-01..N-04, N-06, T-02, P-02, O-05; m5g D1, D14/K-04; m5e W-04). Also found: `SpawnEngine::spawnInstance` → `spawnHouses` is already ported (`SpawnEngine.cpp:223`) | §0 re-derived: A-14 (T-02, P-02), A-15 (checker facts), **A-16 settled → H-06 required**, A-17 folded, A-18, A-19 settled, A-22; new **A-24** (onLeaveInstance / `GeneralInstanceHandler`); §13 items 1-3 moved to §12; header and §12 inputs updated |
| 17 | low | Y11, Y17, Y20, Y21 omit or misorder packets | **Confirmed** (LegionService.java:95-101, 462-467, 472-476, 504-505, 576-584; CM_HOUSE_SETTINGS.java:57-67; HouseController.java:106-122) | Y11 emblem orders; Y17 `OUT_ALL` before `CLOSE_DOOR_ALL`; Y20 the `KICK` history first; Y21 the recreate triple; §2.6 rows 2-3; S-07 |
| 18 | low | Fixtures omit attributes that change bytes | **Confirmed** (`housing_objects.xml:899`: `owner="false"`, `use_days="30"`; ItemRestrictionService.java:52) | §2.12 cake and warehouse-item rows; G-01 emits the expiry window and a legion-storable item; Y15 asserts the expiry; H-07 |
| 19 | low | House objects keep the destroyed instance's position | **Confirmed**, **applied in another form**: Java *does* retain the destroyed instance through those positions (until the owner re-enters; `WorldPosition.mapRegion` retains it, `cycles.toml:357`), so a census asserting "none survives" would fail a faithful port unless the port adds a breaker | §2.7 row 9; risk 3; H-07 and G-07 assert whichever treatment the studio lane chooses — a documented C++ breaker or an `accepted` retention row; §13 item 5 |
| 20 | low | Count and citation errors | **Confirmed**, each: 87 by grep (dead `HouseRegistry.h:78-81`); 1,281/1,310; 974 with `LegionHistoryEntry`; **146 of 188**; `ActionItemNpcAI` 7 bodies + `isInteractionAllowed`; `LegionMember.cpp:25-31`; LegionService.java:809; the "never executed" lines restated (startup runs the unowned half); `getPlacementLimit(bool)` dead; `CM_LEGION_WH_KINAH` needs no open warehouse (D7); `CheckOutput.cpp` now committed; P5-13's two lessees noted | §1, §2.1, §2.3, §2.8, §2.9, §2.10, §2.11, §4, D7, H-05 (the dead body is **O**), I-02, G-07, §12. **M5g's 7 vs this plan's 5 undeclared legion-model bodies is kept at 5** with the reason in §2.9 (methods, not the data-only constructors); the lane re-counts |

**Found while revising** (not in the review): every cross-map teleport of a legion member runs `updateMemberInfo` (TeleportService.java:245-246,
534-535 = `TeleportService.cpp:146`; m5f W-06), so the studio entrance and exit broadcast `SM_LEGION_UPDATE_MEMBER` — a new §2.10 row, §2.5 row 4,
and assertions in Y14/Y18; a class change does the same (m5e W-04, A-22); the personal instance is never registered to its owner, so neither
the entry nor the exit sends an instance message (InstanceService.java:213; TeleportService.java:531-532) — asserted in Y14/Y18; C22's position
seed must zero `players.world_owner`, since A last left the game inside its studio; the gate budget grows to 240-360 s for the walks, the bars
and the ≤ 60-s destroy wait (risk 10).

**The milestone after rev 2**: ~224 bodies over ~3,770 Java lines in thirteen chunks with bodies; stage 1 in two parts over five lanes
(legion-model, legion-service, client-packets, studio, gate-harness) with the legion-service lane as a 9-12 agent-day critical path; stage 2 the
gate `gs.scenario.m5h` (C0-C23, Y1-Y23) with a geo re-run, the re-greens and the census; M5h-2 (land houses, visiting, towns, ~72 bodies) straight
after. The studio half still has no fallback inside M5h for M5f's personal instances, instance teleport and leave hooks (A-14, A-15, A-24).
