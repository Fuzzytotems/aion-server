# M5j work plan (the rest: chat, commands, social, the long tail)

> **Status:** plan **rev 1**, 2026-09-23, revised after its adversarial review (§16 lists what the review found and what changed). A
> **read-only** analysis over HEAD `760e8ab5c` ("M5b-2 stage 1 part 3: the effect classes; passives and post-spawn skills go live") plus the
> working tree (an untracked `cpp/chat-server`, `tools/porting/census.py`, sibling plans and oracle work — no file this plan names as M5j work
> is modified there). **Nothing was compiled, built or run.** C++ statements come from reading both trees, from `game-server/chunks.cmake` and
> `tools/porting/chunks.py files|java|owner`, and from counting `AION_UNPORTED(` / `AION_PARTIAL(` call sites in `.cpp` files with comment
> lines skipped; Java method counts come from `tools/gen/javasrc.py` (named classes; anonymous classes and lambdas are counted separately where
> a row says so); data counts come from throw-away ElementTree parses of `data/static_data` (XML comments skipped). The scripts live in the
> session scratchpad (`plans/m5j/`, rev 1 additions end in `_r1.py` plus `m5j_deep40.py`); §14 says which numbers are **measured** and which
> are **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md). Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 10
> (`phase5-roadmap.md:35`); the sibling drafts as they stood at 23:20 on 2026-09-23 — [m5b3-plan.md](m5b3-plan.md) rev 2,
> [m5c-plan.md](m5c-plan.md) rev 2, [m5d-plan.md](m5d-plan.md) rev 2, [m5e-plan.md](m5e-plan.md) rev 2, [m5f-plan.md](m5f-plan.md) rev 2,
> [m5g-plan.md](m5g-plan.md) rev 1, [m5h-plan.md](m5h-plan.md) rev 2, [m5i-plan.md](m5i-plan.md) rev 1 (m5e, m5f and m5h were revised while
> this revision was written, so this plan cites them by item id, not by line); [phase5-census.md](phase5-census.md);
> [chat-server-port.md](chat-server-port.md); [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.10 and the acceptance rows at
> :646-652; [m5a-client-session.md](m5a-client-session.md) F-2 and [m5b-client-session.md](m5b-client-session.md).
>
> **Three findings change the shape of the roadmap, not only of this milestone.**
>
> 1. **M5j is not one milestone, and it is where every sibling's "later" lands.** Measured against the eight sibling drafts (M5b-3..M5i), M5j owns
>    ~1,400-1,550 bodies after the earlier milestones take their share — **about three times M5b-2** — in fourteen unrelated groups (§1.1).
>    Twelve pieces of work are deferred by a sibling to a milestone that does not take them (§2.6): the kisks and windstreams (m5f O-02), recall,
>    the static doors and the periodic-instance residue (m5f O-03 → "M5g / M5i"), instance matchmaking with the startup step that stops a
>    server running Java's default config (m5g O-01, m5f O-07), Legion Dominion (m5h O-01 → M5i, which calls it M5h's), the PvP half of the
>    kill reward with `CM_SHOW_MAP` (m5g O-05, m5i O-04 → "a PvP milestone" the roadmap does not have), `ApExtractAction` (m5c §3a → M5i),
>    thirteen more root AIs on 1,702 spawn spots, and the summon residue M5e does not take. **Rev 0's "orphaned item enhancement" is
>    withdrawn**: m5c-plan.md §3a now gives every enchant, manastone, tuning and stigma body a home (M5c, M5e), and M5j receives only its "M5j"
>    row (pets, mounts, cosmetics, the arcade; 56 bodies) plus group K if the user keeps the broker in M5c (m5c D2). This plan runs M5j as
>    **five stages** and recommends running stage 0 early (D1).
> 2. **A GM account cannot enter the world today.** `PlayerEnterWorldService::enterWorld` calls `GMService::onPlayerLogin`
>    (`PlayerEnterWorldService.cpp:562`), whose staff arm is `AION_UNPORTED` whenever `gameserver.administration.login.execute_commands` is
>    non-empty (`GMService.cpp:59-69`, the site at :65) — and its default is `//invis, //invul, //enemy none, //see` (`AdminConfig.cpp:28`, Java
>    `AdminConfig.java:77`, `admin.properties:101`). Every account with `access_level > 0` (`Player.java:848-850`) throws out of `enterWorld`;
>    the catch at `PlayerEnterWorldService.cpp:344-351` (Java `PlayerEnterWorldService.java:167-174`) deletes the player, marks it offline and
>    sends `SM_ENTER_WORLD_CHECK(CONNECTION_ERROR)`. **The GM never enters the world.** Both real-client sessions so far used an access-level-0
>    account (`m5a-client-session.md`, Setup). The fix is one line, because `ChatProcessor` is already ported (§3.3, D4) — and it is a
>    prerequisite of M5i's gate, which drives every world event from an access-level-9 account (m5i-plan.md D2, :549).
> 3. **GM commands should move forward, and three plans are about to port the same chat and command framework** (§5, D1). M5g ports
>    `CM_CHAT_MESSAGE_PUBLIC` with `canChat` (m5g-plan.md K-04, W-01, W-03); M5i ports it again with `ChatCommand` and `AdminCommand` for its gate
>    (m5i-plan.md D2, Z-01, Z-02). A 40-command stage 0 (121 bodies, 2,724 Java lines) run right after M5b-2 gives every later real-client session
>    a GM toolkit and chat, and removes those items from M5g and M5i.

---

## 0. What this plan assumes the earlier milestones deliver

Every work item, case and checklist step below that stands on one of these rows carries its id, so the plan can be re-verified in one pass
when M5j branches (I-05). "If not" says what M5j inherits; **"triggered"** means the sibling draft already says it will not deliver, and the
inherited work is a named item in §7.

| Id | Milestone | Assumed delivered | Evidence | If not |
|---|---|---|---|---|
| **A-B2** | M5b-2 | its subset (34 effect classes + D13's four), `P5-02a`/`P5-02b` with 0 `AION_UNPORTED` and 0 `AION_PARTIAL`, npc abilities (stage 2), `CM_CASTSPELL`, `CM_REMOVE_ALTERED_STATE` | measured at `760e8ab5c`: P5-02a 0 U / 0 P, P5-02b 0 U / 0 P | stage 3 grows by the missing classes |
| **A-B3a** | M5b-3 | `ItemService::addItem` (7 overloads), `ItemPacketService` (9) | m5b3-plan.md T-02; m5c-plan.md A-01/A-02 | `//add`, `CM_CHARACTER_EDIT`'s ticket removal and `CM_ATREIAN_PASSPORT` cannot complete |
| **A-B3b** | M5b-3 | the item-action API declared for all 32 bound action classes (m5b3-plan.md D6) and the typed lookups of header request m5b3-h02 (`getHouseObjectAction`, `getAdoptPetAction`, `getRideAction`, …) | m5b3-plan.md D6, §9 m5b3-h02 | M5j files both requests before stage 1 (§9) |
| **A-B3c** | M5b-3 | the 8 item packets + `CM_MANASTONE` (action 4 live, the other arms loud); `ItemEquipmentListener::onItemUnequipment`, `removeStoneStats` (P-02) | m5b3-plan.md §2.7, P-02 | the packets join §2.4's M5j list |
| **A-C1** | M5c | `CM_QUESTION_RESPONSE`, `DialogService` | m5c-plan.md §2.8, D4 | friend and duel requests cannot be accepted; stage 1 ports the packet |
| **A-C2** | M5c | `PlayerService::getOrLoadPlayerCommonData` ×2 | m5c-plan.md W-05 / M-02 | `CM_BLOCK_ADD` of an offline name, `//access`, `//rename`, `//headhunting` wait |
| **A-C3** | M5c | `SystemMailService`, `CubeExpandService` (R, P-05), `PostboxAI` | m5c-plan.md §2.8, §3a | `//sysmail`, `//addcube`, `setinventorygrowth` wait |
| **A-C4** | **user** (m5c-plan.md D2, :422) | (a) **a capital-economy milestone after M5f** takes the broker, express mail (`CM_READ_EXPRESS_MAIL`, `DeliveryManAI`, `FollowingNpcAI`), trade-in (`CM_BUY_TRADE_IN_TRADE`), the AP vendors (`AbyssPointsService` 4), the warehouse and §3a's **group K** (~70 bodies, m5c-plan.md :405) — M5j inherits none of it; or (b) **the broker stays in M5c** (stage 3, B-01/B-02) and group K goes to M5j | m5c-plan.md D2, §3a (:383-414) | under (b) M5j inherits group K and — m5c D2 names no other home for them — express mail, trade-in and the AP vendors: +9 packets (459 lines), +2 AIs (111 lines), ~90 bodies (stage 2, item E-09) |
| **A-C5** | M5c | m5c §3a's start-map item work: `EnchantService`, `EnchantItemAction`, the `ItemSocketService` manastone bodies, `ExtractAction`, `CM_MANASTONE` arms 1/2/3/8, `DecomposeAction` + `CM_SELECT_DECOMPOSABLE`, `RemodelAction`, `ItemActionService` + `TuningAction` + `CM_TUNE`/`CM_TUNE_RESULT`, `CubeExpandService` + `ExpandInventoryAction` | m5c-plan.md §3a (stage 0/1 rows) | not M5j's: the census (I-04) names them against M5c |
| **A-D1** | M5d | `QuestService` (`startQuest`, `finishQuest`, `abandonQuest`), the XML quest engine; `QuestEngine::reload` **stays unported** | m5d-plan.md D8 | `//quest`, `addquest` wait; `//reload quests` is M5j's either way |
| **A-D2** | M5d | E-09: `AbyssPointsService::addAp`, `GloryPointsService::addGp`, `BonusService::getQuestBonus` (+2 helpers), `CubeExpandService::questExpand` | m5d-plan.md E-09 | stage 1 takes them (S-04, S-08) |
| **A-D3** | M5d | `AbyssGuardSimpleAI`, `ActionItemNpcAI` (and `QuestItemNpcAI` under an A1 lease) | m5d-plan.md D5 (m5i-plan.md D5 lists `AbyssGuardSimpleAI` too; the earlier milestone ports it) | stage 3 takes them |
| **A-D4** | M5d | `CM_DELETE_QUEST` (R); `CM_PLAY_MOVIE_END` (W), `CM_OBJECT_SEARCH` (O). `CM_QUEST_SHARE` is handed to M5g (m5d-plan.md §3.7) and taken there (m5g-plan.md K-04) | m5d-plan.md §3.8 | a W or O packet still without a file at M5j's branch joins stage 1's packet lanes |
| **A-E1** | M5e | `ClassChangeService` 7 (C-01), `SkillLearnAction` (C-03), `PlayerReviveService::skillRevive` (C-02) | m5e-plan.md stage 1 | `//set class`, `changeclass`, `classup`, `//res` wait |
| **A-E2** | M5e | stage 3 summons: `SummonsService` 13, `SummonRelease` 2, `TrapService` 2, `SummonGameStats` 12, `SummonLifeStats` 1; the effect classes `SummonEffect`, `SummonHomingEffect`, `SummonServantEffect`, `SummonTrapEffect`, `PetOrderUseUltraSkillEffect`, `SpellAtkDrainInstantEffect`; the `servant`/`trap`/`homing` AIs; the five `CM_SUMMON_*` (M-01..M-05, all R) | m5e-plan.md §2.5, M-01..M-05 | stage 2 takes the block as its own lanes (+~65 bodies) |
| **A-E2r** | – (**triggered**) | **not taken by M5e**: the `skillarea` AI (`SkillAreaNpcAI`); `SummonFunctionalNpcEffect`, `SummonGroupGateEffect`, `SummonTotemEffect`, `PetOrderUnSummonEffect` (1 site each, P5-04); `HomingGameStats`, `ServantGameStats`, `TrapGameStats` (no C++ file, 15 bodies, 234 Java lines) and the four stat set-up sites that construct them (`Homing.cpp:30`, `Servant.cpp:27`, `Servant.cpp:36`, `Trap.cpp:60`, chunk P4-11a) | m5e-plan.md mentions none of the three stat classes; `Homing.java:35`, `Servant.java:26, :40`, `Trap.java:25` build them | stage 3 item N-06. **Flag to the integrator:** the set-up sites sit on the spawn path of every servant, trap and homing M5e's own M-03/M-04 turn on, so M5e's gate will very likely hit them first and take them |
| **A-E3** | M5e | `CM_USE_CHARGE_SKILL`, `CM_TOGGLE_SKILL_DEACTIVATE` (C-04, R) | m5e-plan.md C-04 | +2 packets, 75 lines |
| **A-E4** | M5e | the 32 Daeva effect classes (E-01/E-02, R); stigma — `StigmaService` 10 and the 10 stigma-only classes incl. `SummonSkillAreaEffect` — is an optional stage-3 lane, **recommended taken** (m5e-plan.md D6, T-01/T-02) | m5e-plan.md D6 | stage 2 takes stigma (+~33 bodies), stage 3 the classes |
| **A-E5** | M5e (**triggered**) | only `skillRevive`. `rebirthRevive`, `itemSelfRevive`, `duelRevive` have no owner (m5e-plan.md O-05: "M5f / M5j"; M5f takes `instanceRevive`, T-06) | `PlayerReviveService.cpp:40-49, :145-146` | stage 1 item S-02 |
| **A-F1** | M5f | `TeleportService`, `PortalService`, `BindPointTeleportService`, `InstanceService`, `GeneralInstanceHandler`, `InstanceEngine` (except `addInstanceHandlerClass`), `WorldMapInstance::detachInstanceHandler`, `instanceRevive` ×2 (R, T-06); `CM_TELEPORT_SELECT`, `CM_TELEPORT_ANIMATION_DONE`, `CM_BIND_POINT_TELEPORT`, `CM_INSTANCE_LEAVE`, `CM_MOVE_IN_AIR`; `ResurrectAI`, `PortalAI`, `PortalDialogAI` (A1 lease). `CM_CHANGE_CHANNEL`, `CM_POSITION_SELF` are **O** (P-06) | m5f-plan.md §2.6, T-06, P-06 | the teleport commands and the world tour wait; the two O packets join stage 1 (S-06) |
| **A-F2** | M5f (**triggered**) | nothing: m5f-plan.md O-02 and §2.11 send `CM_WINDSTREAM` and the kisks (`KiskAI`, `InvisiblekiskAI`, `KiskService` 2, `kiskRevive` ×2 — W in M5f's T-06) to M5j | m5f-plan.md O-02, §2.11 | stage 1 (windstream, S-06) and stage 2 (kisks, E-05) |
| **A-F3** | M5f (**triggered**) | nothing for `ChestAI`, `HiddenTeleportNpcAI`, `ShifterAI` — no sibling takes them (m5f W-11: "no plan takes it" for `chest`) | grep over the eight sibling drafts | stage 3 (N-01) |
| **A-F4** | M5f (**triggered**) | nothing of the scored and registered group instances: m5f-plan.md O-03 sends recall, `PeriodicInstanceManager`, `InstanceScore`, `PvPArenaService`, `StaticDoorService` + `CM_OPEN_STATICDOOR` to "M5g / M5i"; neither plan mentions any of them; **m5f's O-07 withdraws its own rev-1 item N-06** (the enter-world and map-change autogroup arms) "together with `scheduleRegistration`, without which an autogroup-enabled server cannot start" | m5f-plan.md O-03, O-07; grep of m5g/m5i: 0 hits for each | stage 1 (static doors S-07, recall S-11), stage 4 (autogroup X-01, J13 L-03) |
| **A-G1** | M5g | P5-10 except O-01 (instance matchmaking) and O-05 (the PvP half); `TemporaryPlayerTeam::sendPacket`; `PvpService::doReward`'s team arm and `PlayerChatService::logMessage` ×2 (W-03); `canChat` (W-01); packets `CM_INVITE_TO_GROUP`, `CM_PLAYER_STATUS_INFO`, `CM_DISTRIBUTION_SETTINGS`, `CM_GROUP_DISTRIBUTION`, `CM_GROUP_LOOT`, **`CM_CLIENT_COMMAND_ROLL`**, `CM_SHOW_BRAND`, `CM_GROUP_DATA_EXCHANGE`, `CM_FIND_GROUP`, `CM_QUEST_SHARE`, and `CM_CHAT_MESSAGE_PUBLIC` if nobody ported it first (A-14, D14) | m5g-plan.md :59 (A-14), K-01..K-04 (:460, :462), W-01 (:470), W-03 (:472) | the GROUP/ALLIANCE/LEAGUE chat arms stay dormant |
| **A-G2** | M5g (**triggered**) | not taken: `RecallService` 4 + `CM_RECALLED_BY_OTHER_ANSWER` (m5f §2.11 sends recall to M5g; m5g never mentions it), `AutoGroupService` + `CM_AUTO_GROUP` (O-01, :515: "after M5f … with the phase-6 PvP instance handlers"), `PvPArenaService`, the PvP half (O-05, :519: "a PvP milestone") | grep of m5g-plan.md: 0 hits for `RecallService`, `PvPArenaService`, `PeriodicInstanceManager` | stage 1 (recall S-11, the PvP half S-12), stage 4 (autogroup X-01) |
| **A-G3** | M5g ↔ M5j stage 0 | the chat overlap. **With D1**, stage 0 has ported `CM_CHAT_MESSAGE_PUBLIC`, `canChat`, `logMessage` by M5g's branch, so m5g A-14 is false and K-04 keeps only `CM_QUEST_SHARE`, W-01 drops `canChat`, W-03 drops `logMessage`. **Without D1**, M5g ports them — and `canChat` calls `ChatBanService.isBanned`/`getBanMinutes`/`banPlayer` and `PlayerChatService.isFlooding` (PlayerRestrictions.java:263-270; `ChatBanService.cpp:7-26`, `PlayerChatService.cpp:11-13`, all `AION_UNPORTED`), which m5g-plan.md does not list (0 hits) | m5g-plan.md K-04, W-01, W-03 | without D1 and without those four bodies, every chat line in M5g throws (lesson 2) — flagged for M5g's review |
| **A-H1** | M5h | P5-11 except Legion Dominion; 23 packets (8 `CM_LEGION*` other than the dominion ranking, the 9 `CM_HOUSE*`, `CM_GET_HOUSE_BIDS`, `CM_PLACE_BID`, `CM_REGISTER_HOUSE`, `CM_USE_HOUSE_OBJECT`, `CM_RELEASE_OBJECT`, `CM_CHALLENGE_LIST`); `ButlerAI`, `HouseSignAI`. `CM_APPEARANCE` is sent to M5j (O-03), `CM_ABYSS_RANKING_LEGIONS` to M5i (O-02), which sends it on to M5j (m5i-plan.md :246) | m5h-plan.md §2, O-02, O-03 | the LEGION chat arm stays dormant |
| **A-H2** | M5h (**triggered**) | Legion Dominion: m5h O-01 sends `LegionDominionService` 6, `LegionDominionLocation` 6, `LegionDominionIntruderUpdateTask` (no file), `LegionDominionPortalAI` (A1), `CM_LEGION_DOMINION_REQUEST_RANKING`, `LegionService::joinLegionDominion` to M5i; m5i-plan.md :94 calls the same work "M5h's". `OneDmgAI` and `OneDmgNoActionAI` (912 spots, 705 spawned from `spawns/Npcs` at startup) appear in no plan | m5h-plan.md O-01; m5i-plan.md :94; grep: 0 hits for `OneDmg` | stage 1 (Legion Dominion S-13), stage 3 (the two AIs) |
| **A-I1** | M5i | P5-12a/b except O-01 (Ahserion → phase 6) and O-04; `SM_FORTRESS_STATUS`, `SM_INFLUENCE_RATIO`, `scheduleReviveAtBase`; **M5i's own stage-0 chat-command control plane** (D2: `CM_CHAT_MESSAGE_PUBLIC`, `ChatCommand` 7, `AdminCommand` 2, six C1 commands under a lease, the `gmCommand` harness; Z-01, Z-02, Z-05 at :319-323); `ArtifactAI`, `FlagNpcAI`, `RiftProtectorAI` (D5, :289) | m5i-plan.md D2, D5, §2.7 | – |
| **A-I2** | M5i (**triggered**) | nothing of the PvP half: m5i O-04 (:410) and m5g O-05 send the conqueror/protector PvP arms, `CM_SHOW_MAP` and the PvP half of `doReward` to "a PvP milestone" that the roadmap does not have (m5b2-plan.md O-04 is the first such mention). `ApExtractAction` (m5c §3a → M5i) and `CM_ABYSS_RANKING_*` (:246 → M5j) likewise | m5i-plan.md O-04, :246; grep: 0 hits for `PvpService`, `ApExtract` | stage 1 item S-12 (the PvP half + `CM_SHOW_MAP`), S-09 (`ApExtractAction`) |
| **A-I3** | M5i (**triggered**) | the four `ConquestOffering*AI` and `NoDmgNoActionAI` (329 spots: `conquest_offering_spawner` 162, `no_dmg_no_action` 162 + 5) appear in no plan | grep: 0 hits | stage 3 (N-01) |
| **A-I4** | M5j (now) → M5i | **I-02 lands before M5i's gate**: its account B has `access_level = 9` (m5i-plan.md :549) and would never enter the world (§3.3); m5i lists no `GMService` item | m5i-plan.md :549; grep: 0 hits for `GMService` in all sibling drafts | M5i's gate cannot start |
| **A-CS** | – | the chat server port (`cpp/chat-server`, **untracked today**) is committed, green under `-L chatserver`, and switched on in the build | chat-server-port.md | the chat link stays off; C-03 waits |

**Nothing in this plan assumes a sibling decision it cannot see.** Where two drafts disagree (A-D3: m5d and m5i both list
`AbyssGuardSimpleAI`; m5f §2.11 sends recall to M5g, which does not take it), the earlier milestone that actually lists the work wins, and a
deferral nobody accepts is M5j's (D15).

---

## 1. Summary

### 1.1 What phase 5 still has after M5b-2..M5i

"Sites" are `AION_UNPORTED` / `AION_PARTIAL` call sites in `.cpp` files. "Bodies" add the Java bodies that have no C++ declaration (lesson 1).

| # | Group | Chunks | Sites U / P | Bodies with no C++ file or declaration | Java lines | Stage |
|---|---|---|---|---|---|---|
| **J1** | chat and the command framework; GM login; the stage-0 prerequisites | P5-14, P5-08, P5-13, P5-00, P4-05, P5-15, P4-12, P4-09 | 29 / 1 | ~25 (8 packets) + 1 anonymous | ~1,100 | **0** |
| **J2** | the chat server link | P4-15 (ported), P5-SC, build | 0 (its prerequisites are in J1) | – | – | 0 part 2 |
| **J3** | the 152 command handlers | C1, C2 | 0 (no C++ file) | 532 | 11,823 | 0 (40), rides (41), 1, 2, 4 |
| **J4** | social and quality of life | P5-08, P5-00, P5-09, P5-07, P5-15, P5-16 | 11 / 1 | ~85 (22 packets) + 17 undeclared (6 item actions) | ~2,300 | 1 |
| **J5** | duel, abyss ranking, revives | P5-08, P5-07, P5-15 | 23 / 2 | ~15 (3 packets) + 5 (`ApExtractAction`) | ~900 | 1 |
| **J6** | pets, mounts, cosmetics (m5c §3a's M5j row) | P5-08, P5-07, P5-15, P5-16 | 30 (+12 D12) | ~40 (3 packets; `AdoptPetAction` 5, `RideAction` 4 + 4 anonymous, `ExpExtractAction` 5 + 1) | ~1,500 (+250 D12) | 2 |
| **J7** | group K, express mail, trade-in, AP vendors — **only under A-C4 (b)** | P5-07, P5-09, P5-05, P5-15, P5-16 | ~35 | ~55 (9 packets, 2 AIs) | ~2,000 | 2 (conditional) |
| **J8** | root npc AIs | P5-05 | 0 | 123 in 26 AIs (+10 in 2 under A-C4 (b)) | 1,594 (+111) | 2 (the 2 kisk AIs), 3 |
| **J9** | effect classes outside M5b-2's subset and M5e's lanes | P5-03, P5-04 | ≤ 248 | 0 (the 30 shell-only classes are data-only) | ≤ ~4,300 | 3 (N-05 decides) |
| **J10** | dormant P5-14 services | P5-14, P5-16 | 18 | 3 (1 packet) | 390 | 1 |
| **J11** | administration | P5-08, P5-13, P4-05 | 13 | 5 (`CMT_CHARACTER_INFORMATION`) | 952 | 1 |
| **J12** | custom content (config-off) | P5-13, P4-14, P5-09, P5-07 | 72 / 1 | 85 | ~2,650 | 4 (D12, **user**) |
| **J13** | phase-6 prerequisites | P5-13, P4-15, P4-16, P4-11a | 28 | 201 (19 classes) | 2,193 | 4 |
| **J14** | the orphans (§2.6): kisks, windstream, `CM_STOP_TRAINING`, recall, Legion Dominion, the PvP half, instance matchmaking, the summon residue | P5-08, P5-07, P5-05, P5-11, P5-12b, P5-01, P5-04, P5-10, P5-13, P4-11a, P5-15, P5-16 | ~120 | ~150 (9 packets, 5 files with no C++ file) + m5g's 52 undeclared autogroup bodies | ~3,600 | 1, 2, 3, 4 |
| | **Total** (upper bound; J12 included, J7 excluded) | | **~590 / 5** | **~1,340** | **~33,000** | |

Command bodies come from `javasrc` (359 admin, 59 player, 114 console). **The site column is again the smallest part of the work**: ~590 sites
beside ~1,340 bodies in Java classes with no C++ file and methods the C++ files never declare — the ratio phase5-census.md §1 measured for
all of phase 5 (3,342 open bodies against 1,682 sites). The stage totals of §12 (~1,400-1,550 bodies) are this upper bound (~1,930) minus J12
(D12), the 41 commands that ride with earlier milestones, and N-05's expected cut of J9.

### 1.2 The client packets

**147 client-packet files have no C++ file** (190 Java files in `clientpackets/`, 43 C++ headers plus `fwd.h`). §2.4 assigns every one:
**2 are never needed** (`CM_GODSTONE_SOCKET`, `CM_TIME_CHECK_QUIT` are commented out of `AionClientPacketFactory.java:119, :237`), **88 belong
to M5b-3..M5i outright** (incl. the broker's 9, whichever milestone A-C4 picks), **M5j must add 45** (2,392 Java lines: 42 no sibling takes,
plus `CM_SHOW_MAP`, `CM_CHANGE_CHANNEL` and `CM_POSITION_SELF`, which M5i/M5f list as optional), and **12 are conditional**:
`CM_CHAT_MESSAGE_PUBLIC` (stage 0 with D1, else M5g), `CM_PLAY_MOVIE_END` and `CM_OBJECT_SEARCH` (M5d W/O), and the 9 of A-C4 (b).

### 1.3 The findings that shape the plan

1. **The work is fourteen groups with nothing in common but their leftover status** (§1.1). §12 splits M5j into **stage 0** (GM toolkit and
   chat, recommended straight after M5b-2), **stage 1** (social, duel, the PvP half, recall, Legion Dominion, administration), **stage 2** (pets,
   mounts, cosmetics, kisks; group K under A-C4 (b)), **stage 3** (npc AIs, the summon residue, the effect tail) and **stage 4** (instance
   matchmaking, the phase-6 prerequisites, the system commands and the phase-5 closure census).
2. **GM accounts are broken at enter world** (header finding 2, §3.3); the fix is a one-line port independent of everything else (D4), and M5i's
   gate depends on it (A-I4).
3. **The chat link is ported and switched off; one service stands between it and a green three-server gate.** `ChatBanService` (5 bodies,
   `ChatBanService.cpp:7-26`) is reached by `CM_CS_PLAYER_AUTH_RESPONSE.cpp:26-27` on every chat login and by `PlayerRestrictions.canChat` on
   every chat line (§3.4). Stage 0 ports it anyway.
4. **Deferral is where M5j's scope grows** (§2.6). Twelve deferrals in sibling drafts point at a milestone that does not take the work, or at
   "a PvP milestone" that is not on the roadmap. D15 makes M5j their default owner; item I-04's census turns every future one into a named row
   the day it appears; item I-05 re-runs §0 at branch time.
5. **A server with Java's default config does not start today, and nobody owned the reason** (W-13): `gameserver.autogroup.enable` defaults
   to `true`, and the startup step `PeriodicInstanceManager.getInstance()` (`GameServer.cpp:243`) reaches the unported `scheduleRegistration`
   (`PeriodicInstanceManager.cpp:52-54`); an exception in a startup step leaves `GameServer::main` (`GameServer.cpp:139-141, :311-316`). Every
   M5 profile sets the key to `false` (`m5a.properties.example:12`, `m5b.properties.example:18`, `mygs.properties:9`). Stage 4 closes it.
6. **Porting the remaining npc AIs wakes code world-wide** (lesson 2): the 26 AIs M5j owns sit on **1,227 open-world spots and 753 others**
   today, all running `DummyNpcAI` (`AIEngine.cpp:162-164`, `gameserver.dev.missing_ai_handlers=warn`). Each switch is named with its spot
   count in the wave report (D10).

---

## 2. The inventory

### 2.1 Method

For every phase-5 chunk (and C1/C2): the C++ files `chunks.py files <chunk>` selects, grepped for `AION_UNPORTED(` and `AION_PARTIAL(` at the
start of a code line (`plans/m5j/m5j_sites_r1.py`; rev 0 counted every mention, headers and comments included, which inflated P5-11, P5-12a
and P5-12b); the Java files `chunks.py java <chunk>` selects, each looked up by basename; every Java method with a body compared by name with
the identifiers of its C++ files (`m5j_undeclared.py`, the method of m5c-plan.md §2.8); Java bodies and lines of named file sets with
`m5j_bodies_r1.py`. Then each area was assigned by the sibling drafts' own item tables, not by the roadmap rows (§0), with a grep of the eight
drafts for every class and packet name this plan inherits (the "0 hits" of §0).

### 2.2 By chunk: now, earlier milestones, M5j

| Chunk | U / P today | Taken by earlier milestones (assumption) | Left for M5j |
|---|---|---|---|
| P5-00 | 4 / 2 | `getOrLoadPlayerCommonData` ×2 (A-C2) | `addMacro`, `removeMacro` (`PlayerService.cpp:376-382`); the staff `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:527`, stage 0) and the `EMOTIONS_ALL` partial (`PlayerService.cpp:268`) |
| P5-01 | 28 / 0 | summon stats 13 (A-E2), `sendGroupPacketUpdate` (A-G1), `onItemUnequipment` and `removeStoneStats` (A-B3c) | `StatFunctions`' PvP half 5 (`StatFunctions.cpp:194-211`, S-12); the 7 `toString` bodies (`Stat2.cpp:41`, `StatFunction.cpp:68`, the five `Stat*Function.cpp`) — **no command calls them** (Stat.java:185-217 formats its own record, Info.java prints `getCurrent()`), so they go to stage 4's closure; the three homing/servant/trap stat classes (A-E2r) |
| P5-02a/b | 0 / 0 | all (A-B2) | – |
| P5-03 / P5-04 | 122 / 134 | the subset (A-B2), the potion and godstone procs (A-B3), M5e's 32 Daeva classes and 6 summon classes (A-E2, A-E4), the stigma classes if M5e's D6 is taken | **J9**: ≤ 112 classes, ≤ 248 sites before M5e; ~80 / ~180 after M5e's required lanes; N-05 decides. The four summon-residue classes and `RecallInstantEffect` (2 sites) are named in N-06 and S-11 |
| P5-05 | 0 / 5 | the 5 partials (A-B2 stage 2); 14 of the 40 missing root AIs (§2.5) | **26 root AIs** (J8) |
| P5-06 | 163 / 2 | all (A-D1) | – |
| P5-07 | 109 / 0 | item services (A-B3), §3a's M5c rows (A-C5), `StigmaService` (A-E4), `QuestStartAction`/`ReadAction` (M5d), `MultiReturnAction`/`InstanceTimeClear` (M5f, O), the house actions (M5h), group K under A-C4 (a) | m5c §3a's M5j row: `AdoptPetAction`, `ToyPetSpawnAction`, `RideAction`, `AnimationAddAction`, `EmotionLearnAction`, `TitleAddAction`, `CosmeticItemAction`, `FireworksUseAction`, `MegaphoneAction`, `ExpExtractAction`, `UpgradeArcadeService` 12 (D12); `ApExtractAction` (A-I2); group K under A-C4 (b) |
| P5-08 | 165 / 2 | `DialogService` (M5c), `ClassChangeService` and `skillRevive` (A-E1), teleport (A-F1), summons (A-E2), `addAp`/`addGp` (A-D2), `PlayerLimitService` (M5c), `doReward`'s team arm and `logMessage` (A-G1) | **J1** `PlayerChatService` 4, `ChatBanService` 5; **J4** `SocialService` 6; **J5** `DuelService` 11, `AbyssRankUpdateService` 4 + 2 P, `AbyssService` 1, `PvpService` headhunting/bounty 4, `duelRevive`, `rebirthRevive`, `itemSelfRevive`; **J6** toy pets 30; **J11** `PunishmentService` 5; **J14** `PvpService` PvP half 5, `RecallService` 4, `KiskService` 2, `kiskRevive` ×2 |
| P5-09 | 121 / 1 | drop (M5b-3), trade/mail/craft (M5c), `BonusService` 3 (A-D2), the broker (A-C4) | `AtreianPassportService` 1, `AdventService` 2 (J4); `WebRewardService` 6 (J12); `StarterKitService` 1 if M5c's optional R-02 is not taken; express mail and trade-in under A-C4 (b) |
| P5-10 | 294 / 0 | all but instance matchmaking (A-G1) | **J14** `AutoGroupService` (21 minus `onLeaveInstance` and `isInAutoInstance`), `AutoInstance`, `LookingForParty`, `AutoGroupUtility`, `AGPlayer`, the three auto-instance classes with no file, `AutoGroupType`'s companion — m5g-plan.md O-01: 50 sites + 52 undeclared |
| P5-11 | 86 / 3 | all but Legion Dominion (A-H1) | **J14** `LegionDominionService` 6, `LegionDominionLocation` 6, `LegionService::joinLegionDominion` 1, the new `LegionDominionIntruderUpdateTask` (3 bodies, 75 lines) |
| P5-12a | 105 / 0 | all (A-I1) | – |
| P5-12b | 197 / 1 | all but the conqueror/protector PvP arms (A-I1, A-I2) | **J14** `ConquerorAndProtectorService` 12, `CPBuff` 2 |
| P5-13 | 118 / 3 | restrictions (M5b-3, M5c, M5g), the instance engine (A-F1) | **J1** `canChat` (`PlayerRestrictions.cpp:196`, stage 0); **J11** `PlayerTransferService` 6, `PlayerTransfer` 1, `CMT_CHARACTER_INFORMATION`; **J12** custom content; **J13** the score/reward/position residue; **J14** `PeriodicInstanceManager` 9, `PvPArenaService` 6 |
| P5-14 | 31 / 0 | none | **all 31**: `utils/chathandlers` 13 (stage 0), `NpcShoutsService` 6, `StaticDoorService` 4, `DatabaseCleaningService` 8 (J10) |
| P5-15 / P5-16 | 0 / 0 (78 + 69 missing files) | 88 packets (§2.4) | **45 packets** + up to 12 conditional |
| C1 / C2 | 152 missing files | M5i's six (its D2) + 35 more riding with their milestones (§5.3, D2) | 111 commands, of which stage 0 takes 40 |
| phase 4 | 100 sites in all phase-4 chunks | the P4-08 quest shells (31, M5d), `Npc::canSell`/`canPurchase`/`canTradeIn` (M5c), team/house/siege packets (M5g/h/i) | stage 0: `GMService::onPlayerLogin` arm (P4-05), the four `registerExpirable` forwarders (P4-12: `TitleList.cpp:25-29`, `PetList.cpp:25-28`, `EmotionList.cpp:18-21`, `MotionList.cpp:24-27`), the guard prefix of `SpawnsData::saveSpawn` (P4-09, `SpawnsData.cpp:212-215`); later: `CAPTCHAUtil::createImage` (P4-05), the rest of the C2 write-back and reload setters (P4-09 7, P4-10 1), the instance score writers (P4-15, 24), `SM_INSTANCE_SCORE` (P4-16), `Npc::queueSkill` ×3, the homing/servant/trap set-up sites (P4-11a 4, A-E2r), `CustomInstancePlayerModelEntryDAO` (P4-14, 2) |

### 2.3 The groups, sized

**J1 — chat, the command framework, GM login (stage 0).** `ChatCommand` 7 (`ChatCommand.cpp:67-131`), `AdminCommand` 2 (`:20-26`),
`PlayerCommand` 2 (`:14-20`), `ConsoleCommand` 2 (`:20-26`); `PlayerChatService` 4 (`PlayerChatService.cpp:11-27`); `ChatBanService` 5
(`ChatBanService.cpp:7-26`) + the anonymous GAG runnable (`ChatBanService.java:40-46`); `PlayerRestrictions::canChat` (`PlayerRestrictions.cpp:196`);
`GMService::onPlayerLogin`'s staff arm (`GMService.cpp:65`); the staff `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:527`;
`GameServer::versionInfo` exists, `GameServer.h:49`); **two prerequisites the minimal command set reaches one call level down** (§5.2): the four
`registerExpirable` forwarders of P4-12, each a file-local helper whose body is `AION_UNPORTED` although `ExpireTimerTask::registerExpirable`
exists (`ExpireTimerTask.cpp:20`; `PlayerEnterWorldService.cpp:611, :616` already call it) — `TitleList::addTitle` calls one on every new title
(`TitleList.cpp:73`, Java `TitleList.java:60-64`); and **the guard prefix of `SpawnsData::saveSpawn`** (Java `SpawnsData.java:205-214`: no spawn
template, a special spawn class, `respawnTime <= 0`, a temporary spawn → `false` before any file I/O), which `//delete` calls after every
delete (`Delete.java:68`); 8 packets: `CM_CHAT_MESSAGE_PUBLIC` (155 lines), `CM_CHAT_MESSAGE_WHISPER` (78), `CM_CHAT_GROUP_INFO` (41),
`CM_CHAT_PLAYER_INFO` (39), `AbstractGmCommandPacket` (33), `CM_BUILDER_COMMAND` (21), `CM_BUILDER_CONTROL` (20), `CM_DEBUG_COMMAND` (27).
**`ChatProcessor` itself is ported** (`ChatProcessor.cpp:73-138`, started at `GameServer.cpp:161`), and so are `CommandsAccessService`,
`ChatUtil`, `NameRestrictionService`, `SM_MESSAGE`, `SM_CHAT_WINDOW`. The frozen headers declare every Java method of the four command classes
(`ChatCommand.h:62-143`; lesson-1 scan: 0 undeclared in P5-14).

**J2 — the chat server link (stage 0 part 2).** The game-server side is ported and off: `ChatServer`, `ChatServerConnection`, 2 client and 4 server
packets (`network/chatserver/**`, P4-15, 0 unported), started at `GameServer.cpp:270-273` only if `gameserver.chatserver.enable`
(`GSConfig.cpp:11`, default `false`), built only with `-DAION_BUILD_CHAT_SERVER=ON` (`CMakeLists.txt:46`, default OFF). Its one unported
dependency is J1's `ChatBanService`. The rest is integration: the build switch, a three-server gate, a profile, and the gag semantics (D9).

**J3 — the commands.** 152 handlers (101 admin, 16 player, 35 console), 11,823 Java lines, 532 bodies, **no C++ file for any of them**; the
registry, the markers and `ChatProcessor::init` exist (`HandlerRegistry.h:135-158`, `ChatProcessor.cpp:73-83`). §5 splits them: 40 in stage 0
(121 bodies, 2,724 lines), 41 riding with M5b-3..M5i (4,466 lines; M5i's six are already its own D2), 71 in stages 1, 2 and 4 (4,633 lines).

**J4 — social and quality of life (stage 1).** `SocialService` 6 (friends, block list, memo); 22 packets (`CM_FRIEND_ADD/DEL/SET_MEMO`,
`CM_MARK_FRIENDLIST`, `CM_BLOCK_ADD/DEL/SET_REASON`, `CM_PLAYER_SEARCH`, `CM_VIEW_PLAYER_DETAILS`, `CM_SET_NOTE`, `CM_TITLE_SET`,
`CM_BONUS_TITLE`, `CM_MACRO_CREATE/DELETE`, `CM_REPORT_PLAYER`, `CM_SHOW_RESTRICTIONS`, `CM_GF_WEBSHOP_TOKEN_REQUEST`, `CM_CHARACTER_EDIT`,
`CM_QUESTIONNAIRE`, `CM_ATREIAN_PASSPORT`, `CM_CAPTCHA`, `CM_MEGAPHONE`; 1,108 Java lines); `PlayerService` macros 2; the `EMOTIONS_ALL`
partial, which needs `EmotionLearnAction.getLearnableEmotionIds` (undeclared); `AdventService` 2, `AtreianPassportService` 1; six item actions
of m5c §3a's M5j row whose `canAct`/`act` M5b-3 declares but does not define (A-B3b): `TitleAddAction` 2, `EmotionLearnAction` 3,
`AnimationAddAction` 4 + 1 anonymous, `MegaphoneAction` 3, `CosmeticItemAction` 2, `FireworksUseAction` 2. `CM_CLIENT_COMMAND_ROLL` left
this group (m5g K-02). Every DAO it writes is ported (phase5-census.md; the DAO tree has 2 unported sites, both in
`CustomInstancePlayerModelEntryDAO`).

**J5 — duel, abyss ranking, revives (stage 1).** `DuelService` 11 + 3 anonymous request handlers (`DuelService.cpp:25-66`);
`PlayerReviveService::duelRevive`, `rebirthRevive`, `itemSelfRevive` (`PlayerReviveService.cpp:40-49, :145-146`; A-E5); `AbyssRankUpdateService`
4 + the two cron partials (`AbyssRankUpdateService.cpp:37, :58`); `AbyssService::announceAbyssSkillUsage`; the `PvpService` headhunting and
bounty bodies (`sendBountyReward`, `finalizeHeadhuntingSeason`, `getHeadhunterById`, `getHeadhunter`; `PvpService.cpp:45-58, :108-109`;
headhunting is `gameserver.event.headhunting.enable=false`, `EventsConfig.cpp:13`); `ApExtractAction` (4 + 1; A-I2); packets
`CM_DUEL_REQUEST`, `CM_ABYSS_RANKING_LEGIONS`, `CM_ABYSS_RANKING_PLAYERS`.

**J6 — pets, mounts, cosmetics (stage 2).** `PetService` 10, `PetAdoptionService` 4, `PetFeedCalculator` 4, `PetFeedProgress` 7,
`PetMoodService` 4, `PetSpawnService` 1 (P5-08); `AdoptPetAction` (5 bodies), **`RideAction`** (canAct, act, finishUse, getRideInfo + 4
anonymous observers — the item `ItemUseObserver.abort` and the `ABNORMALSETTED`, `ATTACKED`, `DOT_ATTACKED` dismount observers,
`RideAction.java:83-150`; `RideAction.h` declares nothing and there is no `.cpp`), `ExpExtractAction` (5 + 1) (P5-07); `CM_PET` (175 lines),
`CM_PET_EMOTE` (101), `CM_APPEARANCE` (119; m5h O-03). `UpgradeArcadeService` 12 + `CM_UPGRADE_ARCADE` (59) belong here too but are an event
feature under D12. m5c §3a sizes the whole M5j row (these, the J4 actions and `CM_MEGAPHONE`) at 56 bodies. `ToyPetSpawnAction` is **not** a
toy-pet action: it places a kisk (`ToyPetSpawnAction.java:51-62`: `STR_CANNOT_USE_BINDSTONE_ITEM_WHILE_FLYING`, `KiskService.haveKisk`,
`isPutKiskZone`), so it is in J14's kisk item.

**J7 — group K and the capital-economy remainder (stage 2, only under A-C4 (b)).** m5c-plan.md §3a group K (:405): `WarehouseService`
5 + 1, `ItemChargeService` 12 + 1 + `ChargeAction` + `CM_CHARGE_ITEM`, `ItemPurificationService` + `CM_ITEM_PURIFICATION`,
`ItemRemodelService` + `CM_ITEM_REMODEL`, `ArmsfusionService` + `CM_FUSION_WEAPONS` + `CM_BREAK_WEAPONS`, `TamperingAction`, `PolishAction`,
`DyeAction`, `AssemblyItemAction`, `PackAction` + `CM_UNWRAP_ITEM`, `CompositionAction` (no C++ file) + `CM_COMPOSITE_STONES` (~70 bodies);
plus express mail (`CM_READ_EXPRESS_MAIL`, `DeliveryManAI`, `FollowingNpcAI`), trade-in (`CM_BUY_TRADE_IN_TRADE`) and the AP vendors
(`AbyssPointsService` 4). Under A-C4 (a) J7 is empty.

**J8 — root npc AIs (stages 2 and 3).** §2.5.

**J9 — the effect tail (stage 3).** 117 effect classes outside M5b-2's 38 carry 256 sites at `760e8ab5c`. M5e takes six classes in its summon lane
(the five summon classes, 8 sites, and `SpellAtkDrainInstantEffect`, M-03), 32 Daeva classes (68 sites), and the 10 stigma classes if its D6 is
taken; M5b-3 takes the potion and godstone procs. That leaves **≤ 112 classes / ≤ 248 sites before M5e and ~80 / ~180 after its required
lanes**. The 13 artifact-activation classes m5i O-03 sends to "an effects milestone" (m5b2-plan.md O-01) are among them and are M5j's by D15. The 30 effect classes without a `.cpp` are data-only
(`APBoostEffect`, `StatboostEffect`, …). Item N-05 re-derives the list from the oracle before the stage-3 lanes are sized.

**J10 — dormant P5-14 services (stage 1).** `NpcShoutsService` 6 (reached only with `gameserver.npcshouts.enable=true`, `AIConfig.cpp:14`,
default false, through `NpcAI::ask(CAN_SHOUT)`, `NpcAI.cpp:146-147`), `StaticDoorService` 4 + `CM_OPEN_STATICDOOR` (m5f O-03 → "M5g / M5i";
neither takes it), `DatabaseCleaningService` 8 (startup, only with `gameserver.cleaning.enable=true`, `CleaningConfig.cpp:8`, `GameServer.cpp:303`).

**J11 — administration (stage 1).** `PunishmentService` 5 (`banChar`, `unbanChar`, `setIsInPrison`, `calculateDuration`, `setIsNotGatherable`;
the last reached by `GatherableController.cpp:125` only with `gameserver.security.captcha.enable=true`, `SecurityConfig.cpp:16`, default false);
`CAPTCHAUtil::createImage`; the player transfer (`PlayerTransferService` 6, `PlayerTransfer` 1, `CMT_CHARACTER_INFORMATION` 399 lines with no C++
file), reached from the login server's `CM_PTRANSFER_RESPONSE` only when an operator starts a transfer.

**J12 — custom content (stage 4, D12).** `PvpMapHandler` 39 + the `PvpMapService::init` partial (`gameserver.pvpmap.enable=false`,
`CustomConfig.java:258`), `CustomInstanceService` 13, `RoahCustomInstanceHandler` (431 lines) and the seven `custom/instance/neuralnetwork`
classes (622 lines), `CustomInstancePlayerModelEntryDAO` 2, `WebRewardService` 6 (`gameserver.web_rewards.enable=false`, `GSConfig.java:77`),
`UpgradeArcadeService` 12 + `CM_UPGRADE_ARCADE` (`gameserver.event.arcade.enable=false`, `EventsConfig.java:18`), and the headhunting bodies of
J5. m5i-plan.md O-05 sends the arcade, headhunting and the advent calendar here too.

**J13 — phase-6 prerequisites (stage 4).** 19 P5-13 classes with no C++ file (2,193 lines, 201 named bodies: `InstanceID`, `DredgionRoom`,
`InstanceBuff`, the five instance positions and `InstancePositionHandler`, the six instance scores, four player rewards), the 24 sites of the
instance score writers (`network/aion/instanceinfo/*ScoreWriter.cpp`, P4-15), `SM_INSTANCE_SCORE` (P4-16), `Npc::queueSkill` ×3
(`Npc.cpp:200-210`). Nothing in phase 5 reaches them except instance matchmaking (J14); the instance handlers of I1..I6 do.

**J14 — the orphans (stages 1-4).** One item per deferral nobody accepts (§2.6):

| Item | Contents | Bodies | Java lines | Stage |
|---|---|---|---|---|
| kisks | `ToyPetSpawnAction` (6 + 1 anonymous), `KiskService` 2 (`KiskService.cpp:19-24`), `kiskRevive` ×2 (`PlayerReviveService.cpp:94-99`), `KiskAI` (6), `InvisiblekiskAI` (3) | ~20 | ~360 | 2 (E-05) |
| windstream | `CM_WINDSTREAM` (states 0-8; `FlyController::switchToGliding`, `SM_WINDSTREAM`, `SM_EMOTION`) | 3 | 93 | 1 (S-06) |
| stop training | `CM_STOP_TRAINING` → `InstanceHandler.onStopTraining` (the no-op default of M5f's `GeneralInstanceHandler`) | 3 | 28 | 1 (S-06) |
| recall | `RecallService` 4 (`requestSummon`, `validateCast`, `canBeSummoned`, `canRecallAt`; `RecallService.cpp:41-95`), `RecallInstantEffect` 2, `CM_RECALLED_BY_OTHER_ANSWER` | 9 | ~240 | 1 (S-11) |
| Legion Dominion | `LegionDominionService` 6 (`LegionDominionService.cpp:52-73`), `LegionDominionLocation` 6, `LegionDominionIntruderUpdateTask` (new, 3), `LegionService::joinLegionDominion` (`LegionService.cpp:436-438`), the two dominion bodies of `ConquerorAndProtectorService` (`resetLegionDominionRank`, `isOccupiedLegionDominionZone`), `CM_LEGION_DOMINION_REQUEST_RANKING`; `LegionDominionPortalAI` (A1, 122 lines) **O** | ~21 | ~480 | 1 (S-13) |
| the PvP half | `PvpService` 5 (`doReward`'s PvP arm `:85`, `findMembersToCountKillFor`, `logKill`, `rewardPlayerTeam`, `updateKillQuests`), `StatFunctions` PvP 5, `ConquerorAndProtectorService` 10 other bodies, `CPBuff` 2, `CM_SHOW_MAP` | ~25 | ~600 | 1 (S-12) |
| instance matchmaking | m5g O-01's `AutoGroupService` residue and model (50 sites + 52 undeclared), `PeriodicInstanceManager` 9 (`PeriodicInstanceManager.cpp:52-90`), `PvPArenaService` 6, `CM_AUTO_GROUP` | ~115 | ~1,300 | 4 (X-01) |
| summon residue | A-E2r: `SkillAreaNpcAI` (in J8), 4 effect classes (4 sites), 3 stat classes (15 bodies, 234 lines) + 4 P4-11a set-up sites | ~23 | ~450 | 3 (N-06) |

### 2.4 The 147 client packets with no C++ file

Every file assigned (`plans/m5j/m5j_cmassign_r1.py`; Java lines measured):

| Milestone | Files | Lines | Packets |
|---|---|---|---|
| M5b-3, arms 1/2/3/8 of `CM_MANASTONE` live in M5c | 9 | 550 | `CM_START_LOOT`, `CM_LOOT_ITEM`, `CM_USE_ITEM`, `CM_MOVE_ITEM`, `CM_SPLIT_ITEM`, `CM_REPLACE_ITEM`, `CM_EQUIP_ITEM`, `CM_DELETE_ITEM`, `CM_MANASTONE` |
| M5c | 24 | 1,220 | `CM_SHOW_DIALOG`, `CM_QUESTION_RESPONSE`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_BUY_ITEM`, the six `CM_EXCHANGE_*`, `CM_CHECK_MAIL_LIST`, `CM_GET_MAIL_ATTACHMENT`, `CM_DELETE_MAIL`, `CM_SEND_MAIL`, `CM_READ_MAIL`, `CM_PRIVATE_STORE`, `CM_PRIVATE_STORE_NAME`, `CM_CRAFT`, `CM_RECIPE_DELETE`, `CM_GATHER`, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_SELECT_DECOMPOSABLE` |
| the broker: capital economy (A-C4 a) or M5c stage 3 (A-C4 b) | 9 | 363 | the 9 broker packets |
| capital economy (A-C4 a), else **M5j** J7 | 9 | 459 | `CM_BUY_TRADE_IN_TRADE`, `CM_READ_EXPRESS_MAIL`, `CM_CHARGE_ITEM`, `CM_ITEM_PURIFICATION`, `CM_ITEM_REMODEL`, `CM_FUSION_WEAPONS`, `CM_BREAK_WEAPONS`, `CM_UNWRAP_ITEM`, `CM_COMPOSITE_STONES` |
| M5d | 1 | 38 | `CM_DELETE_QUEST` |
| M5d W / O (M5f H-01 may take the first), else **M5j** | 2 | 104 | `CM_PLAY_MOVIE_END`, `CM_OBJECT_SEARCH` |
| M5e | 7 | 426 | `CM_USE_CHARGE_SKILL`, `CM_TOGGLE_SKILL_DEACTIVATE`, the five `CM_SUMMON_*` |
| M5f | 5 | 259 | `CM_TELEPORT_SELECT`, `CM_TELEPORT_ANIMATION_DONE`, `CM_BIND_POINT_TELEPORT`, `CM_INSTANCE_LEAVE`, `CM_MOVE_IN_AIR` |
| M5g | 10 | 725 | `CM_INVITE_TO_GROUP`, `CM_GROUP_DATA_EXCHANGE`, `CM_GROUP_DISTRIBUTION`, `CM_GROUP_LOOT`, `CM_DISTRIBUTION_SETTINGS`, `CM_PLAYER_STATUS_INFO`, `CM_FIND_GROUP`, `CM_SHOW_BRAND`, `CM_CLIENT_COMMAND_ROLL`, `CM_QUEST_SHARE` |
| M5h | 23 | 1,431 | 8 `CM_LEGION*`, 9 `CM_HOUSE*`, `CM_GET_HOUSE_BIDS`, `CM_PLACE_BID`, `CM_REGISTER_HOUSE`, `CM_USE_HOUSE_OBJECT`, `CM_RELEASE_OBJECT`, `CM_CHALLENGE_LIST` |
| never | 2 | 56 | `CM_GODSTONE_SOCKET`, `CM_TIME_CHECK_QUIT` |
| **M5j stage 0** with D1, else M5g K-04 / M5i Z-01 | 1 | 155 | `CM_CHAT_MESSAGE_PUBLIC` (opcode slot 27) |
| **M5j J1** | 7 | 259 | `CM_CHAT_MESSAGE_WHISPER` (28), `CM_CHAT_PLAYER_INFO` (39), `CM_CHAT_GROUP_INFO` (61), `CM_BUILDER_COMMAND` (41), `CM_BUILDER_CONTROL` (42), `CM_DEBUG_COMMAND` (180) — registered at `AionClientPacketFactory.java:55-208` — and `AbstractGmCommandPacket` (no opcode) |
| **M5j J4** | 22 | 1,108 | the 22 of §2.3 J4 |
| **M5j J5** | 3 | 163 | `CM_DUEL_REQUEST`, `CM_ABYSS_RANKING_LEGIONS`, `CM_ABYSS_RANKING_PLAYERS` |
| **M5j J6** | 4 | 454 | `CM_PET`, `CM_PET_EMOTE`, `CM_APPEARANCE`, `CM_UPGRADE_ARCADE` (D12) |
| **M5j J10** | 1 | 35 | `CM_OPEN_STATICDOOR` |
| **M5j J14** | 5 | 264 | `CM_WINDSTREAM`, `CM_STOP_TRAINING`, `CM_RECALLED_BY_OTHER_ANSWER`, `CM_LEGION_DOMINION_REQUEST_RANKING`, `CM_AUTO_GROUP` |
| **M5j** unless M5i / M5f take their O | 3 | 109 | `CM_SHOW_MAP` (m5i :240, O; its PvP arms are M5j's by A-I2), `CM_CHANGE_CHANNEL`, `CM_POSITION_SELF` (m5f P-06, O) |
| **Total** | **147** | | **M5j must add 45 (2,392 lines)**; +1 with D1; +2 if M5d/M5f leave theirs; +9 under A-C4 (b) |

Chunks: P5-15 is `CM_[A-K]*` and `Abstract*` (`chunks.cmake:421-429`), P5-16 the rest (`chunks.cmake:430-440`). **Server packets: none to
write.** All 238 Java `SM_*` classes have a C++ file; the six with an unported `writeImpl` (`SM_ALLIANCE_INFO`, `SM_ALLIANCE_MEMBER_INFO`,
`SM_FORTRESS_STATUS`, `SM_HOUSE_BIDS`, `SM_INFLUENCE_RATIO`, `SM_INSTANCE_SCORE`) belong to A-G1, A-I1, A-H1 and J13. Every packet M5j's groups
send was checked: `SM_MESSAGE`, `SM_CHAT_WINDOW`, `SM_CHAT_INIT`, `SM_FRIEND_LIST/RESPONSE/UPDATE`, `SM_BLOCK_LIST/RESPONSE`, `SM_DUEL`,
`SM_PET`, `SM_PET_EMOTE`, `SM_TITLE_INFO`, `SM_MACRO_LIST/RESULT`, `SM_PLAYER_SEARCH`, `SM_VIEW_PLAYER_DETAILS`, `SM_UPDATE_NOTE`,
`SM_ABYSS_RANKING_*`, `SM_CAPTCHA`, `SM_GF_WEBSHOP_TOKEN_RESPONSE`, `SM_MARK_FRIENDLIST`, `SM_QUESTIONNAIRE`, `SM_GM_BOOKMARK_ADD`,
`SM_WINDSTREAM`, `SM_LEGION_DOMINION_RANK`, `SM_EMOTION` — 0 unported each. **What M5j needs are decoders** (H-02, H-11, H-21), written from
the Java `writeImpl`.

Today a real client that sends one of the 147 gets `"<client> sent CM_X, which is not ported yet. Packet won't be instantiated."` once per
class (`AionClientPacketFactory.cpp:120-123`). **At M5j's end that line must be impossible for every registered opcode** (I-04).

### 2.5 The 40 root AI handlers with no C++ file

43 Java root AIs under `data/handlers/ai/*.java`; 3 ported (`GeneralNpcAI`, `AggressiveNpcAI`, `NoActionAI`, P5-05). Spot counts from the spawn
files by the template's `ai` attribute (`npc_templates.xml`; "Npcs" = `spawns/Npcs`, spawned at startup):

| AI name (class) | Templates | Spots (Npcs / other) | Owner |
|---|---|---|---|
| postbox | 7 | 68 / 4 | M5c (A-C3) |
| deliveryman, following | 2, 26 | 0, 20 / 0, 61 | A-C4 (a) capital economy, (b) **M5j** J7 |
| simple_abyssguard, useitem | 859, 489 | 870 / 127; 85 / 293 | M5d (A-D3) |
| servant, homing, trap | 301, 121, 372 | 0 / 1; 0 / 0; 0 / 0 | M5e (A-E2) |
| resurrect | 121 | 92 / 0 | M5f (A-F1) |
| butler, housesign | 12, 6 | 0 / 0 | M5h (A-H1) |
| artifact, flag, rift_protector | 222, 1,566, 6 | 0, 48, 0 / 225, 183, 6 | M5i (A-I1) |
| **book** | 85 | 81 / 4 | **M5j** (22 in Sanctum, 22 in Pandaemonium) |
| **summoner** | 46 | 81 / 12 | **M5j** (43 in Reshanta 400010000; Haramel's boss, m5f W-11) |
| **speaker**, **bubblegut**, **fountain** | 5, 1, 2 | 3, 6, 2 / 0 | **M5j** |
| **aggressive_no_loot** | 393 | 0 / 89 | **M5j** (instances 301390000, 301310000) |
| **aggressive_boss_summon**, **bomb**, **firecracker**, **useSkillAndDie**, **neutralguard** | 8, 7, 2, 28, 6 | 0 / 0 (spawned by handlers or skills) | **M5j** |
| **no_interaction**, **customcdreset** | 0, 0 | no template uses them | **M5j** (reached only through `//ai set`, Z12) |
| **onedmg_aggressive**, **onedmg_passive** | 23, 112 | 354, 351 / 0, 207 | **M5j** (A-H2; 614 of 912 on the housing maps 700010000/710010000) |
| **conquest_offering_{aggressive,buff_npc,portal,spawner}**, **no_dmg_no_action** | 112, 4, 2, 24, 4 | 0, 0, 0, 162, 162 / 0, 0, 0, 0, 5 | **M5j** (A-I3; the spawner maps 220070000 and 210050000) |
| **chest**, **hidden_teleporter**, **shifter** | 201, 8, 1 | 17, 8, 0 / 433, 0, 1 | **M5j** (A-F3) |
| **skillarea** | 65 | 0 / 2 | **M5j** (A-E2r) |
| **kisk**, **invisible_kisk** | 74, 4 | 0 / 0 (item-spawned) | **M5j** stage 2 (A-F2) |

**M5j owns 26 root AIs** (123 bodies, 1,594 Java lines; +2 AIs and 111 lines under A-C4 (b)): the 13 of rev 0 (63 bodies, 791 lines, 173 / 105
spots) and 13 inherited ones (60 bodies, 803 lines, **1,054 / 648 spots**). The handlers-and-porting-plan.md:646 AI smoke ("spawn an Npc with it,
fire SPAWNED → CREATURE_SEE → ATTACKED → DIED → DESPAWNED, run all timers, 0 exceptions or unported hits") becomes a test over all 43 in stage 3
(N-02). 391 other AI names (4,973 templates) belong to phase 6's A1 and I1..I6 (measured).

### 2.6 Deferrals nobody accepts (D15)

| Work | Deferred by | To | What that plan does | M5j item |
|---|---|---|---|---|
| Kisks (`KiskAI`, `InvisiblekiskAI`, `KiskService`, `kiskRevive`), `ToyPetSpawnAction` | m5f O-02, §2.11; m5c §3a | M5j | – (accepted) | E-05 |
| `CM_WINDSTREAM` | m5f O-02 | M5j | – (accepted) | S-06 |
| Recall (`RecallService` 4, `CM_RECALLED_BY_OTHER_ANSWER`) | m5f O-03, §2.11 | M5g | not mentioned | S-11 |
| `StaticDoorService` + `CM_OPEN_STATICDOOR` | m5f O-03 | M5g / M5i | not mentioned | S-07, S-06 |
| `PeriodicInstanceManager`, `PvPArenaService`, `InstanceScore` | m5f O-03, O-07 | M5g / M5i; "whichever milestone ports instance matchmaking" | not mentioned; m5g O-01 defers matchmaking past M5g | X-01, L-03 |
| Instance matchmaking (`AutoGroupService`, `CM_AUTO_GROUP`) | m5g O-01 | "after M5f … with the phase-6 PvP instance handlers" | – | X-01 |
| The PvP half of `doReward`, the CP PvP arms, `CM_SHOW_MAP` | m5g O-05, m5i O-04, m5b2 O-04 | "a PvP milestone" (not on the roadmap) | – | S-12 |
| Legion Dominion | m5h O-01 | M5i | m5i :94 calls it "M5h's" | S-13 |
| `CM_ABYSS_RANKING_LEGIONS` | m5h O-02 | M5i | m5i :246: "M5j" | S-05 |
| `ApExtractAction` | m5c §3a | M5i | not mentioned | S-09 |
| `rebirthRevive`, `itemSelfRevive`, `duelRevive` | m5e O-05 | "M5f / M5j" | M5f takes `instanceRevive` only | S-02 |
| `CM_STOP_TRAINING` | – | – | in no sibling plan | S-06 |
| `ChestAI`, `HiddenTeleportNpcAI`, `ShifterAI`, `OneDmgAI`, `OneDmgNoActionAI`, the four `ConquestOffering*AI`, `NoDmgNoActionAI` | m5f W-11 ("no plan takes it") | – | in no sibling plan | N-01 |
| `SkillAreaNpcAI`, 4 summon effects, 3 stat classes + 4 set-up sites | – | – | M5e takes the rest of summons | N-06 |
| 13 artifact-activation effect classes | m5i O-03 | "an effects milestone" (m5b2 O-01) | – | N-03/N-04 |
| Reward services outside E-09 (`AdventService`, `AtreianPassportService`, `WebRewardService`), the arcade, headhunting | m5c §2.8, m5i O-05 | M5j | – (accepted) | S-08, D12 |

**Withdrawn from rev 0:** the "orphaned item enhancement" (enchant, manastones, stigma, tuning, remodel, purification, amplification,
decomposition). m5c-plan.md §3a (:383-414) now homes every piece: M5c stage 0/1, M5e (stigma), group K (A-C4), and m5c's "M5j" row, which is
J6 plus the six J4 actions. **Item I-04 turns this table into a check**: a closure census that fails for any `AION_UNPORTED` in a phase-5 chunk
whose allow-list row does not name its owner.

---

## 3. The paths, end to end

### 3.1 A line of chat

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | `CM_CHAT_MESSAGE_PUBLIC.readImpl`: `readC` chat type, `readS` message | CM_CHAT_MESSAGE_PUBLIC.java:39-42 | **no file** (P5-15) |
| 2 | `ChatProcessor.handleChatCommand(player, message)` — a `//` or `.` prefix with a registered alias runs the command and ends the packet | CM_CHAT_MESSAGE_PUBLIC.java:48; ChatProcessor.java:58-70 | **ported** (`ChatProcessor.cpp:110-123`) |
| 3 | `PlayerRestrictions.canChat`: online, prison (`STR_INGAME_BLOCK_IN_NO_CHAT(minutes)`), `ChatBanService.isBanned`, `PlayerChatService.isFlooding` → a 2-minute gag | PlayerRestrictions.java:254-274 | `AION_UNPORTED` (`PlayerRestrictions.cpp:196`); `ChatBanService` 5 and `PlayerChatService` 4 unported |
| 4 | `PlayerChatService.logMessage` (CHAT_LOG or ADMINAUDIT_LOG) | PlayerChatService.java:32-36 | unported |
| 5 | `NameRestrictionService.filterMessage` | – | ported |
| 6 | Dispatch by type: NORMAL/SHOUT → `broadcastPacket(… SM_MESSAGE, toSelf=true, p -> !p.getBlockList().contains(sender) \|\| staff)`; GROUP/ALLIANCE/LEAGUE/LEGION → team or legion `sendPackets` (A-G1, A-H1); COMMAND → abyss commanders only; other types → staff only | CM_CHAT_MESSAGE_PUBLIC.java:57-153 | the broadcast helpers are ported; the team and legion arms are dormant until A-G1/A-H1 |
| 7 | `SM_MESSAGE(Player sender, …)`: `senderObjectId` is 0 if the sender has `AbnormalState.HIDE` set (an invisible GM), else its id; **`senderRace = sender.getRace().getRaceId() + 1`** when the sender is a player, the chat type is not a system message, `CustomConfig.SPEAKING_BETWEEN_FACTIONS` is false and the sender is not staff, else 0. `writeImpl`: `writeC(type)`, **`writeC(receiver.isStaff() ? 0 : senderRace)`**, `writeD(senderObjectId)`, `writeS(senderName)`, `writeS(message)`, and for SHOUT `writeF(x, y, z)` | SM_MESSAGE.java:79-81, :114-133 (the race at :121-122), :136-148 (the byte at :140) | ported |

Step 7's race byte depends on **both ends**: a staff receiver always reads 0; a non-staff receiver reads the sender's race id + 1 — **1 for an
Elyos sender (`Race.ELYOS` = 0, Race.java:18), 2 for an Asmodian** — as long as `gameserver.chat.factions.enable` keeps its default `false`
(`CustomConfig.java:26-27`, `custom.properties:16`). That makes it the cheapest exact assertion of the chat path (X5).

### 3.2 A GM command

| # | Step | Java | C++ today |
|---|---|---|---|
| 1 | The line arrives through §3.1 step 2; `getParamsFromString` splits outside square brackets | ChatProcessor.java:58-70 | ported (`ChatProcessor.cpp:140-`) |
| 2 | `AdminCommand.process`: `validateAccess` (`player.hasAccess(level) \|\| CommandsAccessService.hasAccess(id, alias)`; a staff member without the level is told `<You need access level N or higher to use //x>`); **a non-staff player gets `false`, so the text goes out as chat** | AdminCommand.java:38-58 (the message at :41, the return at :48) | unported (`AdminCommand.cpp:20-26`); `CommandsAccessService` ported |
| 3 | `LOG_GMAUDIT` line, then `run` | AdminCommand.java:50-56 | – |
| 4 | `ChatCommand.run`: the `help` arm (`"Command: " + alias + description + syntax`), `execute`, **`IllegalArgumentException` → `sendInfo(toErrorMessage(e))`**, **any other `Throwable` → an ERROR line and `false` → `"<Error while executing command>"`** | ChatCommand.java:60-79 | unported (`ChatCommand.cpp:67-69`) |
| 5 | `sendInfo` → `ChatUtil.split` → `PacketSendUtility.sendMessage` → `SM_MESSAGE(0, null, text, GOLDEN_YELLOW)` | ChatCommand.java:200-212; PacketSendUtility.java:27-29 | unported / ported |
| 6 | Console commands arrive on `CM_BUILDER_COMMAND`/`CM_BUILDER_CONTROL` → `AbstractGmCommandPacket.runImpl` → `handleConsoleCommand`; an unknown alias answers `"The command X is not implemented."`; `CM_DEBUG_COMMAND` only writes an ADMINAUDIT line | AbstractGmCommandPacket.java; CM_DEBUG_COMMAND.java | packets missing; `handleConsoleCommand` ported (`ChatProcessor.cpp:125-138`) |

Once the framework is ported, **an unported body inside a command logs an ERROR and answers the GM instead of killing anything** — the same
swallowing shape as `CreatureController::useSkill` (m5b2-plan.md §8 risk 6); the gates' "no ERROR line" rule keeps it honest. A command whose
C++ file does not exist is not registered at all (`HandlerRegistry.h:135-158`), so its alias reaches the chat as text.

### 3.3 A GM enters the world (the defect)

`PlayerEnterWorldService.enterWorld` calls `GMService.getInstance().onPlayerLogin(player)` near its end (PlayerEnterWorldService.java:323;
C++ `PlayerEnterWorldService.cpp:562`). Java runs every `LOGIN_EXECUTE_COMMANDS` entry through `ChatProcessor.handleChatCommand`, then registers
the staff member (GMService.java:51-57). C++ has the registration and replaced the loop by `AION_UNPORTED()` with the comment "ChatProcessor.h
(P5-14) does not exist yet" (`GMService.cpp:59-69`). It exists now. With the default config the arm is entered by every account with
`access_level > 0`. The exception leaves `enterWorld` at `:562` and is caught by `startEnterWorld` (`PlayerEnterWorldService.cpp:344-351`, as
Java does at PlayerEnterWorldService.java:167-174): the player is deleted from the world (`getController().delete_()`), marked offline in memory
and in the database, detached from the connection, and the client receives **`SM_ENTER_WORLD_CHECK(CONNECTION_ERROR)`**. **A GM cannot enter
the world at all**; the staff registration (`staffMembers.put`, after the throw) never happens either.

**The one-line port is safe before any command exists**: with no command registered, `handleChatCommand` returns `false` for all four strings
(`ChatProcessor.cpp:110-123` = ChatProcessor.java:58-70), which is exactly Java's behaviour for an unregistered alias. D4 lands it immediately.

### 3.4 The chat server link

| # | Step | Where | State |
|---|---|---|---|
| 1 | Startup: `ChatServer.connect` if `gameserver.chatserver.enable`; `SM_CS_AUTH` → `CM_CS_AUTH_RESPONSE` stores the public address | `GameServer.cpp:270-273`; `CM_CS_AUTH_RESPONSE.cpp:33` | ported, off by default |
| 2 | `SM_VERSION_CHECK` announces 1 chat server (4 IPv4 bytes, port) once the link is authenticated, and the channel-chat write level `gameserver.chatserver.min_level` (default 10) | `SM_VERSION_CHECK.cpp:115`; SM_VERSION_CHECK.java:95; `GSConfig.cpp:12` | ported |
| 3 | In the world the client sends `CM_CHAT_AUTH`; the game server sends `SM_CS_PLAYER_AUTH` | `CM_CHAT_AUTH.cpp:27` | ported |
| 4 | `CM_CS_PLAYER_AUTH_RESPONSE`: `SM_CHAT_INIT(48-byte token)` to the client, **then `ChatBanService::isBanned`** and, if gagged, `sendPlayerGagPacket(id, minutes × 60000)` | `CM_CS_PLAYER_AUTH_RESPONSE.cpp:22-27` | **reaches unported `ChatBanService` on every chat login** |
| 5 | The client talks to the chat server directly (channels) | `cpp/chat-server` | ported, untracked (A-CS) |
| 6 | Logout: `SM_CS_PLAYER_LOGOUT` | `PlayerLeaveWorldService.cpp:164` | ported |
| 7 | Link lost: `ChatServer.reconnect` schedules a connect **5 s** later if the link was authenticated, **15 s** otherwise; a failed connect retries after **10 s** on a `SocketException` and **60 s** on any other `IOException`; no reconnect once `GameServer::isShutdownScheduled()` | ChatServer.java:54-64, :76-82; `ChatServer.cpp:61-70, :86-89`; `ChatServerConnection.cpp:58` | ported |
| 8 | Gag: `ChatBanService.banPlayer` → `sendPlayerGagPacket(id, durationMillis)` — a duration where the chat server expects a point in time, and `PunishmentService` passes minutes where milliseconds are expected (`PunishmentService.cpp:47`) | chat-server-port.md "Open items" (:98-106) | Java's behaviour; D9 |

### 3.5 Social and duel, briefly

`CM_FRIEND_ADD` → an `SM_QUESTION_WINDOW` to the target → **`CM_QUESTION_RESPONSE` (A-C1)** → `SocialService.makeFriends` → the friend DAO
(ported) and `SM_FRIEND_RESPONSE`/`SM_FRIEND_LIST` to both. `CM_DUEL_REQUEST` → `DuelService.onDuelRequest` → question → `confirmDuelWith` →
`startDuel` (a 5-minute draw task, `DuelService.java:217-229`) → a fight → **`PlayerController.onDie` sees `isDueling`, calls `loseDuel`, raises
both players to at least 33 % HP and MP, and returns before the death path** (PlayerController.java:275-289; C++ `PlayerController.cpp:367-381`
ported, `DuelService::loseDuel` unported at `DuelService.cpp:50`). That early return is the duel's defining behaviour and the gate's exact
assertion (Z6).

### 3.6 A whisper

`CM_CHAT_MESSAGE_WHISPER` checks, in this order (CM_CHAT_MESSAGE_WHISPER.java runImpl): no such player → `STR_NO_SUCH_USER`; the receiver in
`NO_WHISPERS_MODE` and the sender not staff → `STR_WHISPER_REFUSE`; **`sender.getLevel() < LEVEL_TO_WHISPER` and the receiver not staff →
`STR_CANT_WHISPER_LEVEL("10")`** (`gameserver.chat.whisper.level`, default 10, `CustomConfig.java:32-33`, `custom.properties:20`); the sender on
the receiver's block list → `STR_YOU_EXCLUDED`; different races, factions not allowed, neither staff → `STR_MSG_CANT_WHISPER_OTHER_RACE`; then
`canChat`, `logWhisper` and `SM_MESSAGE(WHISPER)`. **A character that is not a Daeva is capped at level 9** (`PlayerCommonData.java:276-281`:
`maxLevel` is 10 unless Daeva, and the level shown is `maxLevel - 1`), so **no starting-class character can whisper a non-staff player** — only
staff. The gate and the checklist are built around that (Z2, §13).

---

## 4. What M5j turns on, and what it wakes (lesson 2)

Traced from every entry point to the first unported or partial body. **W** = reached by M5j's own code or by a default install, closed or
deliberately left loud; **D** = dormant (config or data the gates and start maps do not produce), named so nobody rediscovers it.

| # | Entry point | First unported / partial body today | Kind | Resolution |
|---|---|---|---|---|
| W-01 | enter world, `access_level > 0` | `GMService::onPlayerLogin` (`GMService.cpp:65`) → the catch at `PlayerEnterWorldService.cpp:344-351` | **W, reachable now** | I-02 (D4) |
| W-02 | enter world, `hasAccess(REVISION_INFO_ON_LOGIN)` | the `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:527`) | W | K-02 |
| W-03 | `CM_CHAT_MESSAGE_PUBLIC` / `_WHISPER` | `canChat` → `ChatBanService`, `PlayerChatService` | W | K-03, K-04 |
| W-04 | the chat link (A-CS, config on) | `ChatBanService::isBanned` at every chat login | W | K-04, C-03 |
| W-05 | a staff member's four login commands | `AdminCommand::process` → `ChatCommand::run` → the four `execute` bodies | W | K-01, K-07 |
| W-06 | any ported command's other arms | `//set class` → `ClassChangeService::setClass` (A-E1), `//set ap` → `addAp` (A-D2), **`//spawn <item id>`** → `ItemActions.getHouseObjectAction` (SpawnNpc.java:37-40, :71; A-B3b) and a house object (M5h), `//useskill` with an effect outside the ported set | W, **loud** | D6: stay `AION_UNPORTED` until their milestone; logged ERROR + `<Error while executing command>` |
| W-07 | **`//spawn <npc> <static id> <respawn time>` with a respawn time > 0** (SpawnNpc.java:46, :64) and **every `//delete` of an npc or gatherable** whose spawn is a plain `SpawnTemplate` (Delete.java:68) | `SpawnsData::saveSpawn` (`SpawnsData.cpp:212-215`). Java returns `false` first for a missing template, a special spawn class, `respawnTime <= 0` and a temporary spawn (SpawnsData.java:205-214); **every other case — i.e. nearly every world monster — writes `data/static_data/spawns/<dir>/New/<map>.xml`** (SpawnsData.java:216-263) into the Java tree the servers run from, which git tracks | W, **dangerous** | stage 0 ports the guard prefix (K-10), so `//delete` of a `//spawn`ed temporary npc returns quietly; the file-I/O tail stays loud until stage 4 (D7); its real-client behaviour is D14 (**user**) |
| W-08 | `CM_FRIEND_ADD`, `CM_DUEL_REQUEST` | the answer needs `CM_QUESTION_RESPONSE` | W | A-C1 |
| W-09 | a duel: every attack on the opponent | `DuelService::loseDuel`, the draw task, `endDebuffsByOpponent`, `cancelSummonedObjectAttacks` | W | S-02 |
| W-10 | `AbyssRankUpdateService` cron jobs (scheduled at `GameServer.cpp:242`) | two `AION_PARTIAL`s (`AbyssRankUpdateService.cpp:37, :58`) | W (silent partials) | S-04; the rows at `m5b_partial_allowlist.txt:76-77` go |
| W-11 | `CM_OPEN_STATICDOOR` (a click on a door) | `StaticDoorService::openStaticDoor` | W | S-07 |
| W-12 | the 26 root AIs (§2.5) | their npcs switch from `DummyNpcAI` to live AIs: **1,227 open-world spots** (book 81, summoner 81, speaker 3, bubblegut 6, fountain 2, onedmg 705, conquest-offering spawner 162, no_dmg_no_action 162, chest 17, hidden_teleporter 8) and **753 others** (instances, sieges) | W, **world-wide** | N-01, E-05; each named in the wave report (D10) |
| W-13 | **startup with `gameserver.autogroup.enable=true`** — the Java default (`AutoGroupConfig.java:12`, `autogroup.properties:7`, `AutoGroupConfig.cpp:9`) | `PeriodicInstanceManager`'s constructor → `scheduleRegistration` (`PeriodicInstanceManager.cpp:26-52` → `:52-54`) inside the startup step at `GameServer.cpp:243`; the exception leaves `GameServer::main` (`GameServer.cpp:139-141, :311-316`): **the server does not start**. With the server up, every enter world reaches `checkAndSendOpenRegistrations` (`PlayerEnterWorldService.cpp:516-518` → `AutoGroupService.cpp:86-87` → `PeriodicInstanceManager.cpp:74-75`) and every map change `AutoGroupService::onLeaveInstance` (m5f W-08) | **D** under every M5 profile and `mygs.properties:9`; **W** under the Java default | X-01 (stage 4). m5f O-07 withdrew rev 1's partial fix; the census allow-list names the key and X-01 |
| W-14 | the weekly Legion Dominion cron `0 0 9 ? * WED *`, scheduled unconditionally (`CronJobService.cpp:84-88, :185-187`) | `LegionDominionService::startWeeklyCalculation` (`LegionDominionService.cpp:60-61`): one ERROR every Wednesday 09:00 on any running server, today | W (weekly, loud) | S-13 |
| W-15 | **`//kill` of a player who sees the GM as an enemy** (Kill.java:93: the attacker is the GM unless the target is a PvP target that does not see the GM as an enemy). The default login command `//enemy none` makes the GM neutral to everyone (Enemy.java), so this needs `//enemy cancel` (then any player of the other race) or `//enemy all` (any player), or emptied login commands (D13) | the PvP half of `PvpService::doReward` (`PvpService.cpp:85`), reached through `PlayerController::doReward` (`PlayerController.cpp:469-471`) | W, **loud from stage 0 on** | S-12 (stage 1); the stage-0 checklist warns. `//damage` is a self-attack (Damage.java:80) and never reaches it |
| D-01 | `gameserver.npcshouts.enable=true` | `NpcShoutsService` (every `ShoutEventHandler` hook, `ShoutEventHandler.cpp:50-72`) | D | S-07; the real-client profile may turn it on after stage 1 |
| D-02 | `gameserver.cleaning.enable=true` | `DatabaseCleaningService` at startup (`GameServer.cpp:303`) — **deletes characters** | D | S-07; unit-tested on a scratch schema only |
| D-03 | `gameserver.security.captcha.enable=true` | `PunishmentService::setIsNotGatherable`, `CAPTCHAUtil::createImage` | D | S-03, S-08 |
| D-04 | a login-server player transfer | `PlayerTransferService` | D | S-08 |
| D-05 | `gameserver.event.headhunting.enable=true`, `gameserver.kill.reward.enable=true` | `PvpService` bounty and headhunting bodies | D | S-04 |
| D-06 | `gameserver.pvpmap.enable=true`, `//cinstance`, the custom-instance AIs | J12 | D | D12 (**user**) |
| D-07 | a player killed by a player (rifts, invasions, the open PvP maps, a duel gone wrong) | `PvpService::doReward`'s PvP half (`PvpService.cpp:85`) — an ERROR per PvP death | D until M5i's rifts; W-15 before that | S-12 |
| D-08 | a grouped player dies to a monster (A-G1 not delivered) | `PvpService.cpp:75` (`TemporaryPlayerTeam::sendPacket`) | D | A-G1 (m5g W-03) |
| D-09 | GROUP / ALLIANCE / LEAGUE / LEGION chat | team and legion `sendPackets` | D | A-G1, A-H1 |

**D-07 and W-15 are the ones to watch.** `doReward` runs on every player death; M5i's rifts bring the first invasions, and since no sibling
takes the PvP half (A-I2) the first real-client invasion produces the ERROR-per-death shape of m5b-client-session.md S-1 until stage 1.

---

## 5. Should GM commands move earlier? — yes, in three parts

### 5.1 Why

- **A GM cannot log in today** (§3.3). Real-client sessions have avoided it by using a player account; that avoids every command too.
- **Every milestone from M5b-3 on asks the user to reach content a fresh character cannot reach**: M5c's checklist relocates a character by SQL
  to reach Sanctum (m5c-plan.md D5, §11), M5d seeds levels, M5f needs flight-path destinations, M5g needs two characters in one place.
  `//moveto`, `//add`, `//set level`, `//spawn` and `//kill` replace those SQL steps and make each session shorter and repeatable.
- **In-game chat is needed for any two-player session** (exchange in M5c, groups in M5g, duels here), and it shares its whole path with commands
  (§3.1 step 2).
- **Two sibling plans already port pieces of it** (A-G1/A-G3, A-I1): doing it once, early, removes three items from them (D1).
- **It is cheap.** The framework is 13 bodies and the dispatcher is already ported.

### 5.2 The minimal testing set (stage 0, 40 commands, 121 bodies, 2,724 Java lines)

Chosen so a tester can log in as a GM, see and change a character, make and kill monsters and talk. **Reachability was checked two call levels
deep** (`plans/m5j/m5j_deep40.py`: level 1 = every method name the command calls, resolved against the C++ bodies of the classes it imports and
of the common receivers; level 2 = the C++ bodies of the ported level-1 callees, scanned for calls into `AION_UNPORTED`/`AION_PARTIAL` bodies
**including file-local helpers**, then read by hand). It found, beyond the framework's own bodies (K-01): `TitleList::addTitle` →
the `registerExpirable` helper (`TitleList.cpp:25-29, :73`) for `//addtitle`, and `SpawnsData::saveSpawn` for `//delete` and `//spawn`
(`Delete.java:68`, `SpawnNpc.java:64`). Both are now stage-0 items (K-10). Its other hits were name collisions (`Map.put` read as
`Storage::put`, `toString` of a string builder read as `Stat2::toString`) and are listed in the script output.

| Purpose | Commands (alias) | Notes |
|---|---|---|
| **T0 — log in as a GM** | `//invis`, `//invul`, `//enemy`, `//see` | the default `login.execute_commands`; W-01/W-05 |
| where am I, what is this | `//coords`, `//info`, `//zone`, `//online`, `//time`, `//weather` | |
| the character | `//addexp`, `levelup`, `leveldown` (console), `//set` (**only its `level`/`exp` arms** before A-E1/A-D2 — W-06), `//addskill`, `//delskill`, `//addtitle` (K-10), `//heal`, `//speed`, `//dispel`, `//removecd`, `//morph`, `//state`, `//stat` | a non-Daeva stops at level 9 whatever `//set level` asks (§3.6); `//addexp` runs `PlayerController.onLevelChange` (PlayerController.java:568-598), the path M5b-1's gate already runs for level 2 |
| monsters | `//spawn` (**npc arm; the item arm is W-06**), `//delete` (K-10's guard: quiet for a temporary spawn, loud for a world spawn until stage 4), `//kill`, `//damage`, `//ai`, `//npcskill`, `//useskill` | `//spawn`, `//delete`, `//ai`, `//stat`, `//removecd` are C2 files (`chunks.cmake:611-616`); `//ai set` replaces an npc's AI through `Creature::replaceAi` (`Creature.h:123`; Java uses reflection, Ai.java:84-89); `//kill` of a player can reach W-15 |
| talking | `//announce`, `//say`, `//whisper`, `//kick`, `//gag`, `//movie` | `//gag` needs `ChatBanService` (K-04) |
| player commands | `.help`, `.id`, `.gmlist` | |

**Not in the set, and why:** `//moveto`/`//goto` (need `TeleportService`, A-F1), `//add` (needs `ItemService::addItem`, A-B3a), `//res` (A-E1),
`//quest` (A-D1). They are the most-wanted commands and they ride with their milestones (§5.3). **`//stat`, `//info`, `//state` and
`//npcskill` do not need the P5-01 `toString` bodies** (Stat.java:185-217 formats its own `StatFunctionInfo`; Info.java prints `getCurrent()`
values), so rev 0's seven `toString` bodies left stage 0.

### 5.3 Commands ride with the milestone that ports their services (D2)

Measured by the milestone that ports each command's first-level static callees (`m5j_cmdtier_r1.py`, owner map updated for m5c §3a):

| Milestone | Commands | Lines |
|---|---|---|
| M5b-3 | `//add`, `//addset`, `//dropinfo`, `//dye`, `wishid`, `.buy`, `.easter`, `.symphony` | 749 |
| M5c | `//access`, `//addcube`, `//rename`, `//sysmail`, `setinventorygrowth`, **`//equip`, `set_enchantcount`, `wish`** (their `EnchantService`/`ItemSocketService` callees are M5c's by §3a) | 906 |
| M5d | `//quest`, `addquest` (`//reload` stays in stage 4: its quest arm calls `QuestEngine::reload`, which M5d leaves unported, A-D1) | 369 |
| M5e | `//res`, `changeclass`, `classup` (and `//set class`) | 127 |
| M5f | `//moveto`, `//goto`, `//movetome`, `//moveplayertoplayer`, `//movetoobj`, `//instance`, `//house`'s teleport arm, `//fixpath` (no save), `teleport`, `teleport_to_named`, `teleportto` | 1,361 |
| M5g | `//event` | 178 |
| M5h | `//legion`, `//auction`; `//spawn`'s item arm (house objects, with m5b3-h02's `getHouseObjectAction`) | 289 |
| M5i | `//siege`, `//base`, `//rift`, `//vortexraid`, `//worldraid`, `//ahserion` — **already M5i's own** (m5i-plan.md D2, under a C1 lease) | 487 |
| **Total** | **41** | **4,466** |

Each of those milestones gets a small "commands" item in its last stage, holding a C1/C2 lease for that stage (D3, m5i D2's model). Its gate
asserts at least one of its commands (e.g. M5f's gate teleports with `//moveto` as well as with a flight master), and **its real-client checklist
uses them**.

### 5.4 The remaining 71 (4,633 lines)

Stage 1 takes the administrative and social ones (`//ban*`, `//unban*`, `//sprison`, `//rprison`, `//ranking`, `//headhunting`, `//playerinfo`,
`//announcements`, `//passkeyreset`, `//securitytoken`, `//grant`, `.faction`, `.advent`, …), stage 2 the pet and item ones (`//pet`,
`.preview`, …), stage 4 the system ones (`//configure`, `//debug`, `//send`, `//spawnu`, `//reload` of static data, `//bookmark`, `//collide`,
`//cinstance`) with D7's write-back. The 99 commands the first-level scan calls "ready" were checked two levels deep only for the 40 of §5.2;
L-04's command smoke is what proves the rest.

---

## 6. Decisions

| # | Decision | Who | Why |
|---|---|---|---|
| **D1** | **Stage 0 (GM toolkit and in-game chat) runs right after M5b-2's gate, before M5b-3.** Stages 1-4 stay at roadmap row 10. **If taken:** M5g's K-04 keeps only `CM_QUEST_SHARE`, its W-01 drops `canChat` and its W-03 drops `logMessage` ×2 (A-G3); M5i's Z-01 and Z-02 drop, and Z-05 builds `gmCommand` on stage 0's H-02 builders; M5i keeps its six C1 commands under D3's lease. **If not taken:** M5g ports `CM_CHAT_MESSAGE_PUBLIC` with `canChat` — and must then also port `ChatBanService.isBanned`/`getBanMinutes`/`banPlayer` and `PlayerChatService.isFlooding` (A-G3); M5i ports `ChatCommand` 7 and `AdminCommand` 2; stage 0 (after M5i) then drops K-01's `ChatCommand`/`AdminCommand` part, K-03, K-04's ban and flood bodies and K-05's public packet, and keeps the rest | **user** (changes the roadmap order) | §5.1. Size: one wave of six lanes and a gate part (§12); every later real-client session gets GM tools and chat, and the chat framework is ported once instead of in pieces by two plans |
| **D2** | **Commands ride with services** (§5.3), each in its milestone's last stage, with one gate assertion and checklist use | integrator if D1 is taken; otherwise the user | a command is a thin client of a service; porting it with the service gives that milestone's tester the shortcut. m5i D2 already does it for its six |
| **D3** | **Manifest, aligned with m5i D2**: C1/C2 **stay `PHASE 6`** (`chunks.cmake:603, :611`); every phase-5 stage that ports commands holds a written lease on C1 and/or C2 for that stage, as the wave-5a leases (`chunks.cmake:442`) and m5i D2 do; C1/C2 gain a `TESTS` home (`tests/handlers_commands`); stage 0 also leases P4-12 (the four forwarders) and P4-09 (the `saveSpawn` guard); P5-SC gains `TEST_INCLUDES` for `cpp/chat-server/tests/support` (`FakePeers.h`); P5-07 splits into P5-07a services / P5-07b `model/templates/item/actions/**` only if M5j's stages 1 and 2 need both halves in one stage (they do not under A-C4 (a)) | integrator | a chunk is the unit of ownership; the lease model is the one the siblings already use, so no chunk moves phase while three milestones touch it |
| **D4** | **The `GMService::onPlayerLogin` arm is ported in the next wave, whatever D1 says** (item I-02, one line, P4-05). **It is a prerequisite of M5i's gate** (A-I4) and of any gate or session with a GM account | integrator, under the standing instruction | a shipped defect on the enter-world path (§3.3) |
| **D5** | *(rev 0's "item enhancement becomes M5c-3" is withdrawn: m5c §3a homes it; group K's home is m5c D2's question, already put to the user there)* | – | §2.6 |
| **D6** | **Arms of ported commands that reach unported services stay `AION_UNPORTED`**; no `AION_PARTIAL` is added to make a command look complete. The gates never type them; the checklist lists them (W-06, W-15) | integrator | m5b2-plan.md D6's argument: `ChatCommand.run` turns the throw into an ERROR line and a visible `<Error while executing command>`, which is the honest answer |
| **D7** | **XML write-back** (`SpawnsData::saveSpawn`'s file-I/O tail, `WalkerData::addTemplate`/`saveData`, `ZoneData::saveData`, `ZoneService::saveMaterialZones`) **is ported only in stage 4, tested against a temporary data root, and never exercised by a gate.** Stage 0 ports only `saveSpawn`'s guard prefix (SpawnsData.java:205-214), which returns before any file I/O. The triggers are `//spawn` with a respawn time > 0 and every `//delete` of a plain world spawn (W-07) — not a `save` argument, which does not exist | integrator | the servers run from the Java tree, so a save writes into tracked `game-server/data/static_data` |
| **D8** | **The chat link keeps Java's default `gameserver.chatserver.enable=false`**; the gate profile and the real-client profile turn it on. The integrator turns `AION_BUILD_CHAT_SERVER` ON once A-CS holds | integrator | faithfulness; the M5a..M5i gates must not start depending on a third process |
| **D9** | **The gag semantics stay Java's** (a duration sent where the chat server compares a point in time; minutes passed as milliseconds by `PunishmentService`), recorded in `docs/deviations/P5-08.md`; a fix is offered to the user as a protocol change on both servers | integrator records; **user** decides a fix | chat-server-port.md:98-106 |
| **D10** | **Every root AI is ported whole**, and the AI smoke of handlers-and-porting-plan.md:646 covers all 43. The wave report names each AI switched from `DummyNpcAI` with its spot count (§2.5) | integrator | lesson 2; the M5a F-1 precedent |
| **D11** | **A phase-5 closure census** (item I-04) is part of M5j's definition of done: 0 `AION_UNPORTED`/`AION_PARTIAL` in phase-5 chunks outside an allow-list whose every row names its owner (a phase-6 chunk, a user-deferred feature, or a config-off feature with its key), every registered client opcode has a C++ class, **and a server started with every Java default reaches "online"** (W-13) | integrator | a deferral nobody accepts is only visible to a census |
| **D12** | **Custom content** (`PvpMap`, `CustomInstance`/Roah and its neural-network player model, headhunting, the upgrade arcade, web rewards): port it (~160 bodies, ~2,650 Java lines: the arcade's service in stage 2, the rest in stage 4), or declare it out of scope with a deviation row that says its config must stay off | **user** | all of it is config-off by default (§2.3 J12) and none of it is retail 4.8 |
| **D13** | **The GM gate and the GM checklist keep Java's login commands.** The checklist says how to turn them off (`gameserver.administration.login.execute_commands =` empty), because a GM with `//enemy none` and `//invul` cannot test combat | integrator | faithfulness, and the defect of §3.3 is only exercised with the default |
| **D14** | **After stage 4, what the write-back does in a real-client session**: (a) **faithful** — `//delete` of a world npc and `//spawn` with a respawn time write `data/static_data/spawns/<dir>/New/<map>.xml` in the user's tracked Java tree (visible in `git status`, revertible); (b) a C++-only key redirects the write-back root to a scratch directory (a deviation row); (c) the file-I/O tail stays `AION_UNPORTED` for good (a deviation: those commands then end in `<Error while executing command>`). **Recommended: (a)**, with the warning in every checklist that uses the two commands | **user** (it writes into their data, and (b)/(c) are deviations) | W-07; the gates are unaffected either way (D7) |
| **D15** | **A deferral nobody accepts is M5j's** (roadmap row 10: "remaining services"): the rows of §2.6. If a sibling's revision takes one, M5j drops it at branch time (I-05) | integrator, under the standing instruction | lesson 2: a body that belongs to no milestone is a throw a real player finds |

---

## 7. Work items

Effort is **size, not time** (m5c-plan.md §5's classes): **S** ≤ 10 bodies or ≤ 250 Java lines, **M** ≤ 30 / ≤ 700, **L** ≤ 60 / ≤ 1,500,
**XL** beyond. Need: **R** required, **W** stub-with-warning allowed, **O** optional. Deps name §0 assumptions where they apply.

### Integrator

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | D3's manifest changes (C1/C2 leases and `TESTS`, the P4-12/P4-09 stage-0 leases, P5-SC `TEST_INCLUDES`, the P5-07 split only if needed). | – | R | S |
| I-02 | **D4**: `GMService::onPlayerLogin`: replace `AION_UNPORTED()` at `GMService.cpp:65` by the loop over `LOGIN_EXECUTE_COMMANDS` calling `ChatProcessor::getInstance().handleChatCommand(player, cmd)`. A `tests/base` case with a staff player, the default config and no registered command asserts that **the character entered the world** (online, spawned, no `SM_ENTER_WORLD_CHECK(CONNECTION_ERROR)`) and that **the steps after `:562` ran**: the bookmarks loop (`:563-564`), the staff registration (`GMService::getInstance().getAllStaff…` contains it), and the `PLAYER_UPDATE` and `INVENTORY_UPDATE` controller tasks (`:622-626`). **Next wave; prerequisite of M5i's gate (A-I4).** | – | R | S |
| I-03 | Chat link switch-on (D8): `AION_BUILD_CHAT_SERVER` ON; `aion_gs_scenario_tests` depends on `aion_chat_server`. | A-CS | R | S |
| I-04 | **The closure census** (D11), built on the existing read-only `tools/porting/census.py` (untracked today; phase5-census.md): an allow-list mode (`docs/porting/phase5-allowlist.txt`, every row with an owner), the opcode check, CTest `porting.phase5.census`. Run at every M5j stage end, **and offered to M5b-3..M5i as their own end check**. | – | R | M |
| I-05 | **Re-verify §0 at branch time**: for every row grep the named files for `AION_UNPORTED(` and check the C++ file exists; re-run `m5j_cmassign_r1.py`, `m5j_aispawn.py` and the §2.6 greps over the sibling plans as they stand; move any item a sibling took. | – | R | S |

### Stage 0 — GM toolkit and in-game chat (after M5b-2 by D1)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| K-01 | The command framework: `ChatCommand` 7, `AdminCommand` 2, `PlayerCommand` 2, `ConsoleCommand` 2 (P5-14). `toErrorMessage`'s enum arm uses `utils::enumValueOf`'s `EnumConstantException` (handlers-and-porting-plan.md:308). | ChatCommand.java:60-248; AdminCommand.java:38-58; PlayerCommand.java:28-44; ConsoleCommand.java:41-62 | – | R | S |
| K-02 | Close the staff `VERSION_INFO` partial (`PlayerEnterWorldService.cpp:527`, P5-00) with `GameServer::versionInfo()`. No allow-list lists `:527` today (checked: m5a, m5b, m5b2 allow-lists), so nothing to delete. | PlayerEnterWorldService.java | – | R | S |
| K-03 | `PlayerRestrictions::canChat` (P5-13). | PlayerRestrictions.java:254-274 | K-04 | R | S |
| K-04 | `PlayerChatService` 4 and `ChatBanService` 5 + the GAG runnable (P5-08). The runnable pins the `Player` for the gag's duration; it is a controller task (`TaskId.GAG`) and must be cut by `cancelAllTasks` (check `cycles.toml`: no `ChatBanService` row today). | PlayerChatService.java; ChatBanService.java:26-75 | – | R | S |
| K-05 | `CM_CHAT_MESSAGE_PUBLIC` (D1), `CM_CHAT_MESSAGE_WHISPER`, `CM_CHAT_GROUP_INFO`, `CM_CHAT_PLAYER_INFO` (P5-15) with `tests/cm_ak` byte vectors and in-process run tests. | the four Java files | K-03 | R | M |
| K-06 | `AbstractGmCommandPacket` (incl. `replaceUnsupportedCommandChars`), `CM_BUILDER_COMMAND`, `CM_BUILDER_CONTROL`, `CM_DEBUG_COMMAND` (P5-15). | the four Java files | K-01 | R | S |
| K-07 | T0: `//invis`, `//invul`, `//enemy`, `//see` (C1 lease). | admincommands/{Invis,Invul,Enemy,See}.java | K-01 | R | S |
| K-08 | T1, C1 part: the other 29 C1 commands of §5.2 (C1 lease). | §5.2 | K-01, K-04, K-10 | R | XL (75 bodies, ~1,900 lines) |
| K-08b | T1, C2 part: `//spawn`, `//delete`, `//ai`, `//stat`, `//removecd`, `levelup`, `leveldown` (C2 lease). | §5.2 | K-01, K-10 | R | L (38 bodies, 666 lines) |
| K-09 | Tests: `tests/misc` (the `help` arm, the three `run` outcomes, `ChatUtil.split` over a help text longer than `MESSAGE_SIZE_LIMIT / 2`, access levels incl. `CommandsAccessService`), `tests/playersvc` (gag, flood → 2-minute gag, unban task on a `ManualClock`, the whisper order of §3.6), `tests/handlers_commands` (each ported command's arms; the **command smoke** of handlers-and-porting-plan.md:652 over the ported set). | – | K-01..K-10 | R | L |
| **K-10** | **The two one-level-down prerequisites** (§5.2): the four `registerExpirable` forwarders (P4-12 lease: `TitleList.cpp:25-29`, `PetList.cpp:25-28`, `EmotionList.cpp:18-21`, `MotionList.cpp:24-27` → `ExpireTimerTask::getInstance().registerExpirable`, including `taskmanager/tasks/ExpireTimerTask.h` as `PlayerEnterWorldService.cpp:157` does); **the guard prefix of `SpawnsData::saveSpawn`** (P4-09 lease: the four early `return false` of SpawnsData.java:205-214, then the existing `AION_UNPORTED()` for the file-I/O tail — D7). Tests in `tests/player` (a title added → `ExpireTimerTask` holds it) and `tests/dataholders` (each guard returns `false` without touching the file system; a plain world spawn reaches the unported tail). | TitleList.java:60-64; SpawnsData.java:205-214 | – | R | S |
| H-01 | **`oracle.py m5j-commands`**: every alias with its access level (`config/administration/commands.properties`, 152 entries — e.g. `kill = 7` at :56), description and syntax parsed from the Java constructors, the rendered `help` text **after `ChatUtil.split`**, the texts of the access message (AdminCommand.java:41) and of every reply the gate asserts, the l10n strings (`ChatUtil.l10n(id)` = `"$"` + two UTF-16 units of `id << 1 \| 1`, ChatUtil.java:96-102), the `ChatType` ids, the whisper level and the non-Daeva level cap. **Mind the aliases a naive `super("…")` scan misses** (it finds 150 of 152; `Bookmark_add` uses a constant, and `cooldown`/`motion` are levels for aliases built elsewhere). | – | – | R | M |
| H-02 | `GameSession` builders `buildCM_CHAT_MESSAGE_PUBLIC(type, text)`, `buildCM_CHAT_MESSAGE_WHISPER`, `buildCM_BUILDER_COMMAND`, a `gmCommand(text)` helper (m5i Z-05's shape, so M5i reuses it); `decoders/ChatDecoders.{h,cpp}` for `SM_MESSAGE` (incl. the SHOUT floats) and `SM_CHAT_WINDOW`, written from `writeImpl`; a `ScenarioDatabase` seed `account_data.access_level` (`login-server/sql/aion_ls.sql:12`) before the first login; `ChatDecodersTest.cpp`. | – | – | R | M |
| G-01 | `gs.scenario.gm` — §10.2. | stage 0 part 1 | – | R | L |
| C-03 | `gs.scenario.chat` — §10.3; `m5j.properties.example` beside the other profiles in the Java tree. | K-04, I-03, **A-CS** | – | R (W if A-CS fails) | L |
| G-02 | Re-green `gs.scenario.m5a/_geo`, `m5b/_geo`, `m5b2/_geo` and `gs.smoke.startup(_geo)`. Nothing should move: none of those gates uses a staff account or chat, and no allow-list row names `PlayerEnterWorldService.cpp:527`. Record the before/after packet counts to prove it. | G-01 | – | R | S |

### Stage 1 — social, duel, the PvP half, recall, Legion Dominion, administration (after M5i)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| S-01 | `SocialService` 6 (P5-08). | SocialService.java | A-C1, A-C2 | R | S |
| S-02 | `DuelService` 11 + 3 anonymous handlers, `PlayerReviveService::duelRevive`, `rebirthRevive`, `itemSelfRevive` (P5-08; A-E5). The draw task pins both players for 5 minutes (`DuelService.java:217-229`); `removeDuel` must cancel it — add the `cycles.toml` row. | DuelService.java; PlayerReviveService.java | A-C1 | R | M |
| S-03 | `PunishmentService` 5 (P5-08): prison (needs `TeleportService`, A-F1), bans, the captcha ban (D-03). | PunishmentService.java | A-F1 | R | S |
| S-04 | Abyss (P5-08): `AbyssRankUpdateService` 4 + the two cron partials, `AbyssService` 1, the `PvpService` headhunting/bounty bodies (D-05), `addAp`/`addGp` if A-D2 did not. | AbyssRankUpdateService.java; PvpService.java | A-D2 | R | M |
| S-05 | Packets A-K (P5-15): the J4 packets of `CM_[A-K]*`, the 3 of J5, and `CM_CHANGE_CHANNEL` if M5f left it. | – | S-01..S-04, A-B3a | R | L |
| S-06 | Packets L-Z (P5-16): the J4 packets of `CM_[L-Z]*`, `CM_OPEN_STATICDOOR`, `CM_WINDSTREAM` (A-F2), `CM_STOP_TRAINING` (A-F4), `CM_RECALLED_BY_OTHER_ANSWER`, `CM_LEGION_DOMINION_REQUEST_RANKING`, `CM_SHOW_MAP`, `CM_POSITION_SELF` if M5f left it, `CM_PLAY_MOVIE_END`/`CM_OBJECT_SEARCH` if M5d left them. Trace `CM_WINDSTREAM`'s callees (`FlyController::switchToGliding`, `PlayerLifeStats::triggerFpRestore`, `QuestEngine.onEnterWindStream`) to their first unported body before porting. | – | S-07, S-11..S-13, A-B3b | R | L |
| S-07 | P5-14: `NpcShoutsService` 6 (the task is K4, `NpcShoutsService.cpp:11-15`), `StaticDoorService` 4, `DatabaseCleaningService` 8 (scratch-schema tests only, D-02). | the three Java files | – | R | M |
| S-08 | The small residue: `PlayerService` macros 2 and the `EMOTIONS_ALL` partial (P5-00); `CAPTCHAUtil::createImage` (P4-05 lease); `AtreianPassportService` 1, `AdventService` 2, and `BonusService` 3 if A-D2 failed (P5-09a); the player transfer 7 + `CMT_CHARACTER_INFORMATION` (P5-13; D-04). | – | A-B3a | R/W | L |
| S-09 | Item actions (P5-07): `TitleAddAction`, `EmotionLearnAction` (unblocks S-08's partial), `AnimationAddAction`, `MegaphoneAction`, `CosmeticItemAction`, `FireworksUseAction`, `ApExtractAction` (A-I2). | model/templates/item/actions/*.java | A-B3b | R | M |
| S-10 | Commands (C1/C2 lease): the administrative and social part of §5.4. | – | S-01..S-04 | R | L |
| **S-11** | **Recall** (A-G2): `RecallService` 4 (P5-08), `RecallInstantEffect` 2 (P5-04). | RecallService.java; RecallInstantEffect.java | A-G1 | R | S |
| **S-12** | **The PvP half** (A-I2): `PvpService` 5 (P5-08: `doReward`'s PvP arm at `PvpService.cpp:85`, `findMembersToCountKillFor`, `logKill`, `rewardPlayerTeam`, `updateKillQuests`); `StatFunctions` PvP 5 (P5-01, `StatFunctions.cpp:194-211`); `ConquerorAndProtectorService` 10 + `CPBuff` 2 (P5-12b, or P5-12b3 if m5i D1 split it). | PvpService.java:111-171; StatFunctions.java; ConquerorAndProtectorService.java | A-G1, A-I1 | R | M |
| **S-13** | **Legion Dominion** (A-H2, P5-11): `LegionDominionService` 6, `LegionDominionLocation` 6, `LegionDominionIntruderUpdateTask` (new file, 3), `LegionService::joinLegionDominion`, the two dominion bodies of `ConquerorAndProtectorService` (P5-12b, with S-12's lane); **`LegionDominionPortalAI`** (A1 lease) **O**: its instance handler is phase 6. | LegionDominionService.java; LegionDominionLocation.java | A-H1, A-F1 | R | M |
| S-14 | Tests in the owning chunks' directories (`tests/playersvc`, `tests/cm_ak`, `tests/cm_lz`, `tests/misc`, `tests/itemsvc`, `tests/stats`, `tests/legionhouse`, `tests/worldevents`), incl. the weekly calculation on a `ManualClock` and the PvP AP arithmetic against the oracle. | – | S-01..S-13 | R | L |
| H-11 | Decoders `SocialDecoders.{h,cpp}` (`SM_FRIEND_LIST/RESPONSE/UPDATE`, `SM_BLOCK_LIST/RESPONSE`, `SM_DUEL`, `SM_UPDATE_NOTE`, `SM_MACRO_LIST`, `SM_PLAYER_SEARCH`, `SM_ABYSS_RANKING_PLAYERS`, `SM_LEGION_DOMINION_RANK`, `SM_WINDSTREAM`, `SM_QUESTION_WINDOW` unless M5c wrote it); builders; **`oracle.py m5j-social`** (duel HP floor, draw time, title and macro limits, the PvP AP formulas of `StatFunctions` for given levels and ranks, the Daeva seed of Z2). | – | – | R | M |
| G-11 | `gs.scenario.m5j` stage-1 cases — §10.4. | S-*, H-11 | – | R | L |

### Stage 2 — pets, mounts, cosmetics, kisks (after stage 1; group K under A-C4 (b))

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| E-01 | Toy pets (P5-08): `PetService` 10, `PetAdoptionService` 4, `PetFeedCalculator` 4, `PetFeedProgress` 7, `PetMoodService` 4, `PetSpawnService` 1. | A-B3a | R | M |
| E-02 | Item actions (P5-07): `AdoptPetAction`, **`RideAction`** (4 + 4 anonymous observers; the mount path: `setPlayerMode(RIDE)`, `SM_EMOTION(CHANGE_SPEED)`, `SM_EMOTION(RIDE, npcId)`), `ExpExtractAction`. | A-B3b | R | M |
| E-03 | Packets (P5-15, P5-16): `CM_PET`, `CM_PET_EMOTE`, `CM_APPEARANCE`, and `CM_UPGRADE_ARCADE` — its file is required whatever D12 says, by I-04's opcode rule; with D12 = out of scope its service calls stay loud behind `gameserver.event.arcade.enable=false`. | E-01, E-02 | R | M |
| E-05 | **Kisks** (A-F2): `ToyPetSpawnAction` (P5-07, with E-02), `KiskService` 2 and `kiskRevive` ×2 (P5-08, with E-01), `KiskAI`, `InvisiblekiskAI` (P5-05; switch named in the wave report). | A-B3b, A-C1 | R | M |
| E-06 | `UpgradeArcadeService` 12 + `CM_UPGRADE_ARCADE` — only if D12 says port. | D12 | O | M |
| E-07 | Commands of §5.4's pet and item part (C1/C2 lease). | E-01 | R | S |
| E-08 | Tests (`tests/itemsvc`, `tests/playersvc`, `tests/cm_*`, `tests/handlers_ai_core`), incl. **pet feed tables with a seeded `Rnd`** against golden vectors computed from the Java formulas. | E-01..E-07 | R | L |
| E-09 | **Only under A-C4 (b):** group K (P5-07 services and actions, `CompositionAction` new file, 7 packets), express mail (`CM_READ_EXPRESS_MAIL`, `DeliveryManAI`, `FollowingNpcAI`), trade-in (`CM_BUY_TRADE_IN_TRADE`), the AP vendors (`AbyssPointsService` 4) — ~90 bodies, as its own lanes. | A-C4 | R (b) | XL |
| E-10 | **Only if M5e's D6 is not taken (A-E4):** `StigmaService` 10 (P5-07); its 10 classes go to stage 3's effect lanes. | A-E4 | R | M |
| H-21 | `oracle.py m5j-items` (pet feed tables from `pet_feed.xml`, ride infos, a kisk-allowed spot where `isPutKiskZone` holds); decoders for `SM_PET`, `SM_PET_EMOTE`, `SM_EMOTION` as the cases need. | – | R | M |
| G-21 | `gs.scenario.m5j` stage-2 cases — §10.4. | E-*, H-21 | R | L |

### Stage 3 — npc AIs, the summon residue, the effect tail (after stage 2)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| N-01 | The 24 root AIs of §2.5 not in E-05 (P5-05), plus any of the 14 others whose assumption failed. | A-C4, A-D3, A-F1, A-H1, A-I1 | R | L (114 bodies, 1,467 lines) |
| N-02 | The AI smoke over all 43 root AIs (`tests/handlers_ai_core`), `ManualClock`, ASan: 0 exceptions, 0 unported hits; `SkillCooltimeResetAI`'s `GeoService.canSee` (SkillCooltimeResetAI.java:88) against the `tests/geo` fixtures. | N-01 | R | M |
| N-03 | Effects A-L outside the subset and M5e's lanes (P5-03), in the order N-05 gives. | N-05, A-E4 | R | XL (upper bound) |
| N-04 | Effects M-Z (P5-04), incl. the summon residue `SummonFunctionalNpcEffect`, `SummonGroupGateEffect`, `SummonTotemEffect`, `PetOrderUnSummonEffect` and, if M5e's T-02 was not taken, `SummonSkillAreaEffect`. | N-05, A-E4 | R | XL (upper bound) |
| N-05 | **`oracle.py m5j-effects --census`**: every effect class reachable from any player skill (all classes, all levels), any npc skill, any item skill, any post-spawn skill, closed under `extends`, minus the ported set — the work list of N-03/N-04 and the "still unreachable" remainder the census allow-list records. | – | R | M |
| **N-06** | **The summon stat residue** (A-E2r), unless M5e took it: `HomingGameStats`, `ServantGameStats`, `TrapGameStats` (P5-01, new files, 15 bodies) and the four set-up sites that construct them (P4-11a lease: `Homing.cpp:30`, `Servant.cpp:27, :36`, `Trap.cpp:60`). | A-E2 | R | M |
| G-31 | `gs.scenario.m5j` stage-3 cases — §10.4. | N-* | R | M |

### Stage 4 — instance matchmaking, the long tail and the closure (after stage 3)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **X-01** | **Instance matchmaking** (A-G2, A-F4): m5g O-01's `AutoGroupService` residue, `AutoInstance`, `LookingForParty`, `AutoGroupUtility`, `AGPlayer`, the three auto-instance classes with no file and `AutoGroupType`'s companion (P5-10; 50 sites + 52 undeclared), `PeriodicInstanceManager` 9 and `PvPArenaService` 6 (P5-13), `CM_AUTO_GROUP` (P5-15). **Closes W-13**: a startup with the Java default reaches "online". Entering a registered instance lands in the instance with M5f's no-op handler until phase 6 provides the PvP instance handlers. | A-F1, A-G1 | R | XL |
| L-01 | The system commands (C1/C2 lease): `//configure` (the named config field registry, handlers-and-porting-plan.md:304), `//debug` (`NioServer::snapshotConnections`, :305), `//send`, `//spawnu`, `//reload` for static data and commands (:216, :724), `//bookmark`, `//collide`, the console remainder; **D7's write-back** (P4-09 `SpawnsData`'s file-I/O tail, `WalkerData`, `ZoneData`; P4-10 `ZoneService`) with temp-directory tests and D14's answer; the reload setters (`EventData::setEvents`, `NpcSkillData::setNpcSkillTemplates`, `XMLQuests::setData`); the 7 P5-01 `toString` bodies. | A-D1, D14 | R | XL |
| L-02 | J12 custom content — **only if D12 says port** (P5-13 `custom/**`, P4-14 DAO, P5-09 `WebRewardService`). | D12 | O | XL |
| L-03 | J13 phase-6 prerequisites (P5-13 residue, P4-15 score writers, P4-16 `SM_INSTANCE_SCORE`, P4-11a `Npc::queueSkill`), each with byte-vector tests from the Java `write*`. | X-01 | R | XL |
| L-04 | Command smoke over **all 152** (`help`, no parameters, from a GM) and the "every alias has an access level" check (handlers-and-porting-plan.md:652). | L-01 | R | M |
| G-41 | `gs.scenario.world_tour` (nightly, geo on) — §10.5. | A-F1, L-01 | R | L |
| G-42 | `porting.phase5.census` green with the final allow-list (I-04), incl. a startup with every Java default. | all | R | S |

---

## 8. Lanes

At most six lanes per stage part; chunks disjoint within a part; every lane followed by an adversarial reviewer whose first job is to
mutation-test the lane's new assertions. Leases (D3) count as the lane's chunk for the part.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| now | integrator | P4-05 | I-02 | `tests/base` |
| 0.1 | **gm-core** | P5-14, P5-00, P5-13 | K-01, K-02, K-03 | `tests/misc`, `tests/login_slice`, `tests/instance` |
| 0.1 | **chat-services** | P5-08 | K-04 | `tests/playersvc` |
| 0.1 | **chat-packets** | P5-15 | K-05, K-06 | `tests/cm_ak` |
| 0.1 | **commands-c1** | C1 (lease), P4-12 (lease) | K-07, K-08, K-10 (forwarders) | `tests/handlers_commands`, `tests/player` |
| 0.1 | **commands-c2** | C2 (lease), P4-09 (lease) | K-08b, K-10 (guard) | `tests/handlers_commands`, `tests/dataholders` |
| 0.1 | **harness** | P5-SC, `tools/oracle` | H-01, H-02 | decoder self-tests, `tools.oracle` |
| 0.2 | **gate** | P5-SC | G-01, C-03 (serial), G-02 | `gs.scenario.gm`, `gs.scenario.chat`, the earlier gates |
| 0.2 | fixups | whatever the gate names | – | – |
| 1.1 | **player-services** | P5-08 | S-01..S-04, S-11 (service) | `tests/playersvc` |
| 1.1 | **pvp-dominion** | P5-01, P5-12b, P5-11, P5-04 | S-12, S-13, S-11 (effect) | `tests/stats`, `tests/worldevents`, `tests/legionhouse`, `tests/effects_mz` |
| 1.1 | **residue** | P5-14, P5-00, P4-05 (lease), P5-09, P5-13 | S-07, S-08 | owning dirs |
| 1.1 | **actions+commands** | P5-07, C1/C2 (lease) | S-09, S-10 | `tests/itemsvc`, `tests/handlers_commands` |
| 1.1 | **harness** | P5-SC, `tools/oracle` | H-11 | – |
| 1.2 | **packets-ak** | P5-15 | S-05 | `tests/cm_ak` |
| 1.2 | **packets-lz** | P5-16 | S-06 | `tests/cm_lz` |
| 1.2 | **gate** (after both) | P5-SC | S-14's cross-lane part, G-11 | `gs.scenario.m5j` |
| 2 | **pets-kisks** | P5-08 | E-01, E-05 (service) | `tests/playersvc` |
| 2 | **actions** | P5-07 | E-02, E-05 (action), E-10 | `tests/itemsvc` |
| 2 | **packets** | P5-15, P5-16 | E-03 | `tests/cm_ak`, `tests/cm_lz` |
| 2 | **kisk-ais** | P5-05 | E-05 (AIs) | `tests/handlers_ai_core` |
| 2 | **commands+harness** | C1/C2 (lease), P5-SC, `tools/oracle` | E-07, H-21 | – |
| 2 | *group K* (A-C4 b) | P5-09, and P5-07/P5-15/P5-16 **as a second part** after the lanes above | E-09 | `tests/economy`, `tests/itemsvc` |
| 2 gate | gate + fixups | P5-SC | G-21 | `gs.scenario.m5j` |
| 3 | **root-ais** | P5-05 | N-01, N-02 | `tests/handlers_ai_core` |
| 3 | **effects-al** | P5-03 | N-03 | `tests/effects_al` |
| 3 | **effects-mz** | P5-04 | N-04 | `tests/effects_mz` |
| 3 | **summon-stats** | P5-01, P4-11a (lease) | N-06 | `tests/stats`, `tests/objects` |
| 3 | **harness** | P5-SC, `tools/oracle` | N-05 | – |
| 3 gate | gate + fixups | P5-SC | G-31 | `gs.scenario.m5j` |
| 4 | **matchmaking** | P5-10, P5-13 (`PeriodicInstanceManager`, `PvPArenaService`), P5-15 (`CM_AUTO_GROUP`) | X-01 | `tests/team`, `tests/instance`, `tests/cm_ak` |
| 4 | **system-commands** | C1/C2 (lease), P4-09, P4-10, P5-01 | L-01, L-04 | `tests/handlers_commands`, `tests/dataholders`, `tests/world`, `tests/stats` |
| 4 | **custom** (D12) | P4-14, P5-09 | L-02 (DAO, web rewards) | `tests/dao`, `tests/economy` |
| 4 | **network-residue** | P4-15, P4-16, P4-11a | L-03 (writers, packet, `queueSkill`) | `tests/network`, `tests/sm_ak`, `tests/objects` |
| 4 | **census** | `tools/porting` | I-04 finish | `porting.phase5.census` |
| 4.2 | **instance-residue** | P5-13 | L-03 (P5-13 classes), L-02 (P5-13 `custom/**`) — after `matchmaking` releases P5-13 | `tests/instance` |
| 4 gate | gate | P5-SC | G-41, G-42 | `gs.scenario.world_tour`, census |

**Critical paths.** Stage 0: `commands-c1` (83 bodies, ~2,060 Java lines, XL) — start it on day 1 against the framework's header, which is frozen and complete.
Stage 1: `player-services` (~50 bodies over seven services). Stage 2: small unless A-C4 (b), when group K is the path. Stage 3: the effect lanes,
whose size N-05 decides. Stage 4: `matchmaking` (~115 bodies) and `system-commands` (`//configure` and `//reload`, 133 and 107 Java lines, are the
two reflective bodies that need the replacements of handlers-and-porting-plan.md:304-310, and `//goto` alone is 440 lines of location tables).

---

## 9. Header requests expected

| Request | Kind | For |
|---|---|---|
| none for `ChatCommand.h`, `AdminCommand.h`, `PlayerCommand.h`, `ConsoleCommand.h`, `ChatBanService.h`, `PlayerChatService.h`, `SocialService.h`, `DuelService.h`, `RecallService.h`, `KiskService.h`, `LegionDominionService.h` | – | the lesson-1 scan found every Java method declared (P5-14: 0; P5-08: only enum accessors and anonymous classes) |
| **the item-action API** (`AbstractItemAction` virtuals, the `override` declarations on the 32 bound action classes incl. `RideAction.h`, which declares nothing today) and **m5b3-h02**'s `ItemActions` lookups (`getHouseObjectAction`, `getAdoptPetAction`, `getRideAction`, …) — assumed filed and approved in M5b-3 (A-B3b, m5b3-plan.md D6); **if not, M5j files them before stage 1** | additive | S-09, E-02, E-05, `//spawn`'s item arm (M5h's ride) |
| enum accessors the generated enums may not carry: `SummonMode`, `UnsummonType`, `PetHungryLevel` ×3, `BanAction.getId` — **verify first**: generated enums usually expose them as free functions under other names (e.g. `abyssRankId`, `PlayerEnterWorldService.cpp:566`) | additive or none | E-01, S-03 |
| new headers (no request): the packets, `CompositionAction` (A-C4 b), the 26 root AIs, the 19 J13 classes, `CMT_CHARACTER_INFORMATION`, `LegionDominionIntruderUpdateTask`, the three summon stat classes (N-06), the auto-instance classes (X-01), the neural-network classes if D12 ports them (+ `fwd.h` regeneration, `skeleton.py --fwd`) | new files | all stages |
| **Manifest** (D3): C1/C2 leases and `TESTS`, P4-12/P4-09/P4-11a/P4-05 leases, P5-SC `TEST_INCLUDES` for the chat server's `tests/support`, P5-07a/b if needed | build | I-01 |
| `CMakeLists.txt:46` `AION_BUILD_CHAT_SERVER` ON | build | I-03 |
| `game-server/config/m5j.properties.example` in the Java tree beside the other gate profiles | none | C-03 |

---

## 10. Gate specification

### 10.1 Processes, databases, profiles

Like m5b2-plan.md §10.1: own schema pairs (`aion_{ls,gs}_test_gm_<hash>`, `…_chat_…`, `…_m5j_…`), own output directories, the same
`RESOURCE_LOCK "aion_game_server_log;aion_login_server_log"`, own partial allow-lists. A GM account is an account whose
`account_data.access_level` is set by `ScenarioDatabase::execute` before its first login (the precedent of m5b-plan.md D12's `player_life_stat`
seed). The profile states `gameserver.chat.factions.enable = false` and `gameserver.chat.whisper.level = 10` explicitly (their Java defaults,
`custom.properties:16, :20`), because X5 and Z2 depend on them. **No geo variant for `gs.scenario.gm`, `gs.scenario.chat` or `gs.scenario.m5j`**:
measured, of every M5j path only `//collide`, the `teleport` console command and two root AIs call `GeoService` (`AbyssGuardSimpleAI`, which
M5d ports, and `SkillCooltimeResetAI`, which no template uses and N-02 unit-tests against the geo fixtures); m5c-plan.md D12 is the precedent for
saying so. Geo is exercised by the world tour (§10.5).

### 10.2 `gs.scenario.gm` (stage 0)

Accounts: **G** (access level 9), **P** (level 0), **L** (level 1) — Elyos characters placed by their creation spawn on Poeta — and **Q**
(level 0, an Asmodian character on Ishalgen). Every exact text, level and id comes from `oracle.py m5j-commands` (H-01), never from the port.
The allow-list's §A carries the M5b-3 drop partial (`DropRegistrationService.cpp:43`) while M5b-3 is not in, because X8 kills a monster.

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **X1** | G enters the world with the default `login.execute_commands` | the enter-world burst completes (the M5a gate's last-packet rule), **no `SM_ENTER_WORLD_CHECK(CONNECTION_ERROR)`** (the plain `SM_ENTER_WORLD_CHECK()` of the normal path, PlayerEnterWorldService.java:221, is expected) and **no ERROR line**; within it, G receives `SM_MESSAGE` type `GOLDEN_YELLOW`, sender id 0, with **exactly** the oracle's texts of `//invul` (`"$"` + l10n 293440), `//enemy none` (`"You are now neutral to everyone."`) and `//see` (l10n 288645); `STR_SKILL_EFFECT_INVISIBLE_BEGIN` and an `SM_PLAYER_STATE` for itself with the HIDE visual state (`//invis`, Invis.java execute) | **Proves:** I-02, K-01's `process`/`run`, the four T0 bodies. **Cannot prove:** that monsters now ignore G — a behaviour no Z-row fights as G | I-02 reverted (`CONNECTION_ERROR`, an ERROR); `process` that does not call `run` (no replies) |
| **X2** | G: `//<alias> help` for every command stage 0 registered | the `SM_MESSAGE` sequence equals the oracle's rendering **after `ChatUtil.split`** | **Proves:** the `help` arm, `sendInfo`, `split` over long texts, and that each constructor's alias, description and syntax match Java. **Cannot prove:** the command bodies | a `split` boundary off by one |
| **X3** | L (access level 1): `//kill` on a monster | exactly one `SM_MESSAGE` `"<You need access level 7 or higher to use //kill>"` — **the level from the oracle** (`commands.properties:56`: `kill = 7`) — and the monster keeps its HP | **Proves:** `validateAccess` and the staff arm of `process`. **Cannot prove:** `CommandsAccessService`'s per-player grants (unit test) | `validateAccess` answering true (the monster dies); a level read from the wrong key (the text differs) |
| **X4** | P (level 0): `//kill` | **G receives P's text `"//kill"` as a NORMAL `SM_MESSAGE`**, and P gets no access message | **Proves:** `process` returns `isStaff()` = false so the line goes out as chat (AdminCommand.java:47-48) — a player cannot probe for commands | returning true (the text vanishes) |
| **X5** | P: NORMAL `"hello"` (G within broadcast range); Q: NORMAL `"hi"` on Ishalgen | G receives `SM_MESSAGE(type NORMAL, race byte **0**, P's object id, P's name, "hello")`; **P's own echo carries race byte 1** (`Race.ELYOS` id 0 + 1); **Q's own echo carries race byte 2** (`Race.ASMODIANS` id 1 + 1) — values from the oracle, which reads `SM_MESSAGE.java:121-122, :140` and the factions key | **Proves:** §3.1 end to end and both ends of the race byte. **Cannot prove:** the team and legion arms (D-09), the factions-enabled arm (unit test) | a constant 0 (P's echo fails), a constant 1 (Q's echo fails), the race id without `+ 1` (P's echo reads 0), the staff check dropped (G reads 1) |
| **X6** | P → G whisper; P → a name nobody has | G receives `SM_MESSAGE` type WHISPER (G is staff, so P's level does not matter, §3.6); P receives `SM_SYSTEM_MESSAGE` `STR_NO_SUCH_USER` (id from the oracle) | **Proves:** `CM_CHAT_MESSAGE_WHISPER`'s first arm and the staff exemption of the level arm. **Cannot prove:** the block and faction arms (Z2, unit tests) | swapped sender and receiver; a level check that ignores `receiver.isStaff()` (P gets `STR_CANT_WHISPER_LEVEL`) |
| **X7** | G: `//gag P 1 test` (syntax `<player> <duration> <reason>` / `<player> remove`, Gag.java constructor); P speaks; G: `//gag P remove`; P speaks | after the gag P receives `STR_INGAME_BLOCK_ENABLE_NO_CHAT(1)` and the reason; P's line is answered by `STR_INGAME_BLOCK_IN_NO_CHAT(1)` and G receives nothing; after `remove` P receives `STR_CAN_CHAT_NOW` and G receives P's next line | **Proves:** `ChatBanService` ban/unban and `canChat`. **Cannot prove:** the unban *timer* (a `ManualClock` unit test in K-09) | `isBanned` inverted; `canChat` skipping the ban |
| **X8** | G: `//spawn 210663`, targets it, `//kill`; `//addexp <n>` (n from the oracle, landing on a level ≤ 9); `//addskill 1328 1`; `//speed 5`, then `//speed 0`; G targets P: `//damage 50`, then `//heal` | `SM_NPC_INFO` with npc id 210663 within 3 m of G; its death (the M5b decoders' die sequence); `SM_STATUPDATE_EXP` with the oracle's exp and, when the level changes, **`SM_ACTION_ANIMATION(G, LEVEL_UP, newLevel)`** (PlayerController.java:587) and the oracle's `SM_SKILL_LIST` additions (`SkillLearnService.learnNewSkills`, :594); `SM_SKILL_LIST` containing 1328 level 1; an `SM_STATS_INFO` whose speed is the fixed value (`SPEED` = 5 × 1000 through `Stat.CommandStatFunction`), then the original; **P's `SM_STATUPDATE_HP` shows `max − ⌊0.5 × max⌋`** (Damage.java: a value ≤ 100 is a percentage; a self-attack at :80), **then `max`**, and G receives the oracle's `"… has been refreshed."` | **Proves:** the most-used T1 bodies against real services; `//heal` is observable because its target is damaged first. **Cannot prove:** that the spawn survives a restart (D7) | a `//spawn` at the wrong height (z against G); a `//heal` that does nothing (P stays at half) |
| **X8b** | G: `//addtitle <id>` (an Elyos title from the oracle) on itself; G `//spawn`s 210663 again (no respawn time), targets it and `//delete`s it | `SM_TITLE_INFO` with the title, and **no ERROR**; the npc's `SM_DELETE`, no `"Spawn removed permanently"` text and no ERROR (the spawn has `respawnTime` 0, SpawnsData.java:211-212) | **Proves:** K-10's forwarder and `saveSpawn`'s guard. **Cannot prove:** the write-back (D7) | a forwarder left `AION_UNPORTED` (ERROR + `<Error while executing command>`); a guard that reaches the file tail |
| **X9** | G via `CM_BUILDER_COMMAND`: `"levelup 1"`, then `"nosuchcommand"` | the level rises by 1 (≤ 9); the second answers `"The command nosuchcommand is not implemented."` | **Proves:** `AbstractGmCommandPacket` → `handleConsoleCommand`. **Cannot prove:** `CM_BUILDER_CONTROL`'s client semantics (same code path) | – |
| **X10** | reports | the M5a Q8 bar: `unported_trace.txt` empty, census empty, no ERROR, `partial_trace.txt` ⊆ the gm allow-list; **`live_counts`: `Player` live 0 after the four logouts** | **Proves:** nothing on the path fell outside stage 0, and the GAG task released P | a GAG task that outlives the logout |

**Mutation proof (minimum set, each watched failing with both outputs quoted):** revert I-02 → X1; `process` returns true for non-staff → X4;
`validateAccess` true → X3; drop `ChatUtil.split` → X2; `SM_MESSAGE` race byte constant 1 → X5 (Q); race id without `+ 1` → X5 (P);
`canChat` without the ban check → X7; `//heal` body emptied → X8; K-10 forwarder reverted → X8b; `ChatBanService` GAG task not registered as a
controller task → X10 (`Player` live 1). **What the gate deliberately cannot catch:** the `toErrorMessage` enum formatting (unit test), the
flood gag (unit test with a `ManualClock`), ADMINAUDIT logging (a log-content unit test).

### 10.3 `gs.scenario.chat` (stage 0 part 2, A-CS)

chat-server-port.md:122-144's proposed gate, adopted as written, plus a gag step: login server, chat server (`ChildProcess`, stop file, scratch
copy of `chat-server/` with a test `mycs.properties` and schema), game server with `gameserver.chatserver.enable=true` and the test address and
password.

| # | Assertion | Proves / cannot prove |
|---|---|---|
| Y1 | "Gameserver #1 is now online" in the chat server log and the game server's connected line, before the first client | the link and `SM_CS_AUTH`; cannot prove reconnect (Y6) |
| Y2 | `SM_VERSION_CHECK` announces **one** chat server at 127.0.0.1 and the chat client port, and the channel-chat level byte equals `gameserver.chatserver.min_level` (10) | `setPublicAddress`, the announcement, the level (SM_VERSION_CHECK.java:95) |
| Y3 | `CM_CHAT_AUTH` → `SM_CHAT_INIT` with a 48-byte token; the fake chat client authenticates with it | the three-party handshake **and that `ChatBanService::isBanned` ran without throwing** (§3.4 step 4) |
| Y4 | two players join their map channel; one speaks; the other receives exactly Java's `SM_CHANNEL_MESSAGE`; the `chatlog` row exists | the chat server itself (the fake client ignores the client-side level rule of Y2); cannot prove other channel kinds |
| Y5 | G: `//gag P 1 test` → the chat server receives `CM_PLAYER_GAG` for P (its log line) | the gag packet leaves the game server; **cannot prove a gag works** — D9, Java's own gap |
| Y6 | stop the chat server (stop file) and restart it **within 5 s**: the game server logs `"Reconnecting to chat server in 5s..."` (the link was authenticated, ChatServer.java:79-81) and is authenticated again **no earlier than 5 s and no later than 5 s + slack** after the drop; a new client's `SM_VERSION_CHECK` announces it again. A second drop with the chat server kept down for 12 s: the connect at 5 s fails with a `SocketException` and the next attempt comes 10 s later (ChatServer.java:56-58), so re-authentication happens in [15 s, 15 s + slack]. On game-server shutdown no reconnect is attempted (`ChatServerConnection.cpp:58`) | `reconnect`'s two delays, the retry delay and the `isShutdownScheduled` arm; cannot prove the 60 s arm (a unit test with a non-socket `IOException`) |
| Y7 | logout → the chat server closes the player's chat connection; all three processes exit 0; no unported hits | logout path and shutdown ordering |

Labels `scenario;chatserver;realdata`; the scenario `RESOURCE_LOCK`; a missing chat server binary **fails** the gate (the M5a "fail when a
prerequisite is missing" rule), except while A-CS does not hold, when C-03 is W and the gate is not registered.

### 10.4 `gs.scenario.m5j` (stages 1-3, cumulative)

Accounts G (level 9, Elyos), players **A** and **B** (level 0, Elyos), **C** (level 0, Asmodian) and **G2** (access level 9, Asmodian). **B is
seeded as a Daeva of level ≥ 10** by m5c-plan.md D5's recipe (the advanced class in `players.player_class`, the ascension quest row, and
`players.exp` from the oracle), because a non-Daeva cannot whisper a non-staff player (§3.6). Cases are added per stage; each stage's gate
re-runs the earlier cases.

| # | Stage | Case and assertion | Proves / cannot prove | Mutation it kills |
|---|---|---|---|---|
| Z1 | 1 | A `CM_FRIEND_ADD(B)` → B gets `SM_QUESTION_WINDOW` → B accepts (A-C1) → both receive `SM_FRIEND_RESPONSE` and a list containing the other; **the DB holds two `friends` rows**; B quits → A receives `SM_FRIEND_UPDATE` offline | `makeFriends` and the online-status push; cannot prove the memo arm (unit) | a one-row `makeFriends` |
| Z2 | 1 | A blocks B (`CM_BLOCK_ADD` with a reason) → B's NORMAL line no longer reaches A; **B (Daeva, level ≥ 10) whispers A → `STR_YOU_EXCLUDED(A)`**. Then B blocks A as well, and **A (level 1) whispers B → `STR_CANT_WHISPER_LEVEL("10")`, not `STR_YOU_EXCLUDED`** — the level arm comes before the block arm (§3.6); texts and level from the oracle | the block list feeds §3.1 step 6 and §3.6's block arm; the order of the whisper arms | a block list stored but not consulted; the level check dropped or moved after the block check |
| Z3 | 1 | `CM_SET_NOTE` → `SM_UPDATE_NOTE` to A and to A's friend; relog → the note persists; `CM_MACRO_CREATE` → relog → `SM_MACRO_LIST` contains it; `CM_TITLE_SET` with an owned and an unowned title | persistence of three small DAOs; cannot prove title *effects* | a macro not stored |
| Z4 | 1 | `CM_PLAYER_SEARCH(name=B)` → `SM_PLAYER_SEARCH` lists B; `CM_VIEW_PLAYER_DETAILS(B)` → `SM_VIEW_PLAYER_DETAILS` with B's equipped-item count | the two lookups | – |
| Z5 | 1 | A `CM_DUEL_REQUEST(B)` → B accepts → `SM_DUEL` start to both | `onDuelRequest`/`confirmDuelWith`/`startDuel` | – |
| **Z6** | 1 | A fights B until B would die: **B receives no `SM_DIE`; B's HP and MP are ≥ 33 % of max** (the oracle computes the floor); both receive `SM_DUEL` with the lost/won result | **the `isDueling` early return of PlayerController.java:275-289 and `loseDuel`**; cannot prove the 5-minute draw (unit, `ManualClock`) | `loseDuel` not called (B dies) |
| Z7 | 1 | G `//sprison B 1` (A-F1) → B is moved to the prison map and B's chat gets `STR_INGAME_BLOCK_IN_NO_CHAT`; `//rprison B` → B back | `PunishmentService` + `canChat`'s prison arm | – |
| Z8 | 1 | `CM_ABYSS_RANKING_PLAYERS` → `SM_ABYSS_RANKING_PLAYERS`; G `//ranking update` → no ERROR, and the two §A allow-list rows of the cron partials are gone | S-04 closed the partials | a partial left in place |
| **Z14** | 1 | G2 (Asmodian GM) and A (Elyos), both seeded to one oracle spot where both races may fight and to levels ≥ 10; G2 types `//enemy cancel` (its login commands made it neutral to everyone, D13), then `//kill` A (Kill.java:93: A now sees G2 as an enemy, so G2 is the attacker) → A dies (the M5b die sequence); **A's and G2's AP change by the oracle's `calculatePvPApLost` / `calculatePvpApGained` values**, `SM_ABYSS_RANK` to both; no ERROR | the PvP half and `StatFunctions`' PvP arithmetic (S-12); cannot prove the team split (unit test) | `rewardPlayerTeam` not called (G2's AP unchanged); the loss formula swapped with the gain |
| **Z15** | 1 | A (legion member, M5h) `CM_LEGION_DOMINION_REQUEST_RANKING(1)` → `SM_LEGION_DOMINION_RANK` for location 1; `CM_WINDSTREAM` state 0 then 3 on a windstream the oracle picks → `SM_WINDSTREAM(0, 1)`, `SM_WINDSTREAM(3, 1)` and `SM_EMOTION(WINDSTREAM_EXIT)` | S-13's lookup and the windstream packet; the weekly calculation is a `ManualClock` unit test | a ranking for the wrong location |
| Z9 | 2 | A uses a ride item (`//add`, id from the oracle) → after the item's delay A and a watcher receive `SM_EMOTION(CHANGE_SPEED)`, `SM_EMOTION(RIDE, npcId)` (RideAction.java:152-153) and `SM_ITEM_USAGE_ANIMATION(…, 0, 1, 1)`; A uses it again → dismount (the packets `unsetPlayerMode(RIDE)` sends, from the oracle) | `RideAction`'s two arms; cannot prove the dismount observers (unit) | a ride that never unsets |
| Z10 | 2 | A, seeded at the oracle's kisk-allowed spot, uses a kisk item (`ToyPetSpawnAction`) → `SM_NPC_INFO` of the kisk; A binds (the kisk dialog, A-C1); G `//kill` A (same race: A is killed by itself, Kill.java:93) → A's revive offer includes the kisk; `CM_REVIVE(KISK)` → A revives at the kisk | kisks end to end (E-05) | `kiskRevive` left unported (ERROR) |
| Z11 | 2 | A adopts and summons a toy pet (the egg item from `//add`; `CM_PET` adopt, summon) → `SM_PET` sequences; relog → the pet persists (`player_pets`) | toy pets | – |
| Z12 | 3 | G `//spawn` one npc of each of the 24 N-01 root AIs that have a template beside A; `no_interaction` and `customcdreset` have no template, so G `//ai set <name>` on a spawned npc (Ai.java:82-89; `Creature::replaceAi`); A approaches and attacks where the AI allows | each AI's spawn and first reactions with **no ERROR** (the AI smoke covers the full event list) | a handler throwing on `handleSpawned` |
| Z13 | all | reports as X10; `live_counts`: `Player`, `Pet`, `Kisk` live 0 after logout | leaks in the new tasks (duel draw, gag, pet mood, kisk) | a draw task never cancelled |

Under A-C4 (b) stage 2 adds one group-K case (the oracle picks the cheapest reachable service, e.g. a remodel with a seeded pair of items) with
m5c-plan.md D6's randomness rule.

### 10.5 `gs.scenario.world_tour` (stage 4, nightly, geo on)

G teleports with `//moveto` (A-F1) to the oracle's safe spot of **each of the 42 maps of `spawns/Npcs/`**, waits 15 s (map regions activate,
AIs think, walkers walk, zone handlers fire), and moves on. Assertions: no ERROR line, `unported_trace.txt` empty, no watchdog dump, `Player` live
0 at the end. A second run starts the server **with every Java default** (autogroup on, W-13) and asserts it reaches "online". **Proves:** that
nothing on any open-world map throws once a player is present — the class of bug the first real-client session found (m5a-client-session.md
F-1). **Cannot prove:** anything about instances, sieges or events. About 12 minutes; `LABELS "scenario;realdata;geo;nightly"`, `TIMEOUT 2700`.

### 10.6 The closure checks (stage 4)

`porting.phase5.census` (I-04, D11): fails on any `AION_UNPORTED`/`AION_PARTIAL` in a phase-5 chunk (and C1/C2) that has no allow-list row with
an owner; fails if any opcode registered in `ClientPacketInfo.gen.inc` has no C++ class (only the two commented-out ones may be missing). The
command smoke (L-04) and the AI smoke (N-02) run under `ctest -L smoke`.

---

## 11. Risks

1. **Scope that moves under the plan.** Twelve deferrals already land here (§2.6), and every "later" in an earlier plan lands here by default
   (D15). **Mitigation:** I-05 re-derives §0 at branch time; I-04's census runs at the end of *every* milestone, so a new orphan is named the day
   it appears; D1 takes the self-contained stage 0 out of the tail.
2. **A default install does not start** (W-13) until stage 4. Every profile the project ships sets autogroup off, but a user who copies Java's
   `config/main` unchanged gets a server that stops at startup step `PeriodicInstanceManager.getInstance()`. **Mitigation:** the census row
   names the key; G-42 and the world tour's second run prove the fix.
3. **Waking dormant code at world scale, and touching the user's data.** Stages 2-3 switch **1,980 spots** from `DummyNpcAI`; stage 1 makes
   shouts, doors, cleaning and captcha reachable behind config; C2's write-back writes into the Java tree (W-07). **Mitigation:** D7 and D14,
   D10 (per-AI spot counts), the 43-AI smoke, the world tour, D-02's scratch-schema-only rule for `DatabaseCleaningService`, which deletes
   characters.
4. **Access control and the three-process chat gate.** A wrong `validateAccess`, `isStaff` or `CommandsAccessService` port gives players GM
   powers — a security defect no feature test notices unless it asks (X3, X4). The GM's own login commands hide combat bugs from a GM tester
   (D13). The chat gate adds a third process that is **untracked today** (A-CS), a third schema, run-time ports and reconnect timing (Y6).
   **Mitigation:** X3/X4 mutation-proven; the chat gate W until A-CS holds.
5. **Three plans touching one framework** (A-G3, A-I1). Without D1, M5g and M5i each port part of the chat and command path, and M5g's `canChat`
   reaches four bodies it does not list. **Mitigation:** D1, or the A-G3 note carried into M5g's review.
6. **Swallowed exceptions, again.** `ChatCommand.run` catches every `Throwable` (ChatCommand.java:73-77) and `CreatureController::useSkill` does
   too; D6 relies on the gates' "no ERROR line" rule, which must not be relaxed for command gates.
7. **Tasks that pin players.** The GAG task, the duel draw task (5 minutes, both players), the pet mood task, the kisk's lifetime, the shout
   task (K4), the Legion Dominion intruder task: each needs its `cycles.toml`/`fieldmap.toml` row checked; X10/Z13's live-0 rows are the only gate
   evidence. m5b2-plan.md §8 items 1-2 apply unchanged.
8. **Float exactness** in pet-feed arithmetic and the PvP AP formulas: golden vectors in two build types.
9. **The effect tail is unmeasurable today** (J9 is an upper bound). N-05 must run before the stage-3 lanes are sized.
10. **The oracle's command model.** Parsing 152 Java constructors for aliases and syntax texts is a small compiler task; the naive scan already
    misses 2 of 152 (H-01). A wrong oracle makes X2 a consistency check between two wrong readers.

---

## 12. Sizing and the split

Size classes as in §7 (bodies and Java lines, m5c-plan.md §5). Bodies are sites plus undeclared plus no-file bodies, rounded.

| Stage | What a player (or tester) can do at the end | Bodies | Java lines | Lanes | Class |
|---|---|---|---|---|---|
| **0** GM toolkit and chat | log in as a GM; use 40 commands; chat and whisper; channel chat through the chat server (A-CS) | ~175 (55 framework, chat, packets, prerequisites + 121 in 40 commands) | ~3,700 | 6 + gate | XL (one wave) |
| **1** social, duel, PvP, recall, dominion, administration | friends, block list, notes, titles, macros, search, view details, duel, a PvP kill that rewards, recall, Legion Dominion ranking, windstreams, prison and bans, abyss ranking, doors; shouts and cleaning behind config | ~345 | ~7,000 | 5 + 2 + gate | XL (two parts) |
| **2** pets, mounts, cosmetics, kisks | adopt and summon a pet, ride a mount, place and revive at a kisk, rename by ticket | ~150 (+~90 under A-C4 (b), +~14 by D12, +~33 if A-E4 fails) | ~2,200 | 5 (+1) + gate | XL |
| **3** npc AIs, summon residue, effect tail | every root AI live; every reachable effect class ported | ~240-390 (N-05 decides) | ~3,500-6,000 | 5 + gate | XL |
| **4** matchmaking, long tail, closure | autogroup registration; a default-config server starts; system commands, reload, write-back; phase-6 prerequisites; world tour; census green | ~500 (+~145 by D12) | ~7,000 (+~2,400) | 5 + 1 + gates | XL |
| **Total** | | **~1,400-1,550** (+ D12, + A-C4 (b), + failed assumptions) | **~23,000-26,000** | | |

**Against the measured pace, as an extrapolation only.** The git log shows M5b-1 going from its plan commit `5f65cb14f` (09-22 02:29) through
stage 1 `340c05c5e` (16:40) to its gate `23c4e6485` (09-23 02:32), about 24 h; M5b-2's stage 0 `6f6756c06` (09-23 10:41) and three stage-1
parts `29009d778` (14:36), `c1edb0afb` (18:49, 295 sites closed) and `760e8ab5c` (22:58) each took about four hours, i.e. a six-lane part closes
roughly 250-300 bodies. At that pace M5j's ~1,500 bodies are six or seven wave parts plus five gate parts: **roughly three to five days of wall
clock**, not rev 0's "7-10 weeks of lane time". The extrapolation assumes M5j's bodies are as tractable as M5b-2's; the effect tail (N-05), the
command oracle (H-01) and the three-process chat gate are where it can break. M5j is 2.7 to 3 times M5b-2 in bodies (~521 over 17,835 Java
lines, m5b2-plan.md:16-18), but shallower: mostly packets, commands and small services with no engine in the middle.

**Why this order.** Stage 0 first because it is small, self-contained and makes every later session cheaper (D1). Stage 1 next because it closes
the most client packets a real player's UI sends unprompted (friends, titles, macros, the webshop token), the two cron partials, the weekly Legion
Dominion ERROR (W-14) and the PvP half that M5i's invasions reach (D-07). Stage 2 is item-born work. Stage 3 after the milestones whose AIs and
effects it would otherwise duplicate. Stage 4 last because its census is the definition of phase 5's end, and its commands touch every holder and
config.

**If a stage has to split:** stage 1 is already two parts; stage 2 at group K (A-C4 (b)) and at kisks; stage 4 at matchmaking (X-01 first, with
W-13) and at J12/J13 (the census may allow-list them with owners).

---

## 13. Real-client checklist

Prerequisites as m5b-plan.md §10 steps 1-6, with `mygs.properties` from `m5j.properties.example`.

**After I-02 (steps 1-2) and after stage 0 (all):**

1. In `aion_ls`: `UPDATE account_data SET access_level = 9 WHERE name = '<your account>';`. Log in and enter the world. **Expected:** you
   enter normally — before I-02 the client shows a connection error and the character never appears (§3.3) — and four yellow lines appear:
   invulnerable, neutral to everyone, the see-hidden line, and you are invisible to others.
2. For a combat session as a GM, empty `gameserver.administration.login.execute_commands` in `mygs.properties` (D13).
3. `.help` lists the player commands; `//kill help` prints the syntax. Try `//coords`, `//info` on a monster, `//spawn 210663`, `//kill`,
   `//addexp 1000`, `//set level 9` (**a character that is not a Daeva stops at 9**, §3.6), `//addskill 1328 1`, `//speed 50`, `//addtitle`,
   and on a second character: `//damage 50`, then `//heal`.
4. **Expected loud refusals (D6):** `//set class …`, `//moveto …`, `//add …` answer `<Error while executing command>` or are not recognised
   (they arrive with M5e, M5f, M5b-3). **After `//enemy cancel`, `//kill` on a player of the other race — or on anyone after `//enemy all` —
   logs an ERROR per kill until stage 1** (W-15); with the default `//enemy none` the victim kills itself and nothing is logged; `//damage`
   never reaches it. **`//delete` of a world monster deletes it and then answers `<Error while executing command>`**; it respawns
   after a restart (W-07). `//delete` of something you `//spawn`ed without a respawn time is quiet. Note which ones you tried.
5. Chat: say something; whisper a name that does not exist ("no such user"). **Whispers:** a character below level 10 can whisper only a GM;
   a GM below level 10 can whisper only other staff (§3.6). With a second client: whisper the GM from the player, `//gag <name> 1 test` the
   player and watch the refusal, then `//gag <name> remove`.
6. With the chat server (chat-server-port.md "Running it next to the login and game servers"): the chat window's channels are visible and the
   chat server's log shows the player log in and out. **Writing** in a channel needs level `gameserver.chatserver.min_level` (default 10), which
   no starting-class character reaches: for the session set it to 1 in `mygs.properties`, or seed a Daeva by SQL (m5c-plan.md §11).

**After stage 1:** add a friend (the request window appears on the other client), block someone and check their chat disappears, set a note,
pick a title, create a macro and relog, search for a player, inspect a player's gear, duel (the loser stays alive at a third of their HP),
`//sprison` and `//rprison` a test character, open the Abyss ranking window, ride a windstream, and — with a GM of the other race — `//kill` a
player and watch both AP totals change.

**After stage 2:** adopt a toy pet from its egg and summon it; ride a mount and dismount; place a kisk, bind to it, die and revive there; use a
rename ticket.

**After stage 3:** walk through Sanctum and Pandaemonium (the readable books), Reshanta's summoner monsters and a housing map's training dummies;
nothing should log an ERROR.

**After stage 4:** start the server once with Java's `config/main` unchanged (autogroup on) and log in; `//moveto` through a few maps with geo
enabled; `//reload` of a static-data holder and of the commands. **`//spawn` with a respawn time and `//delete` of a world monster now write
`data/static_data/spawns/<dir>/New/<map>.xml` in the Java tree (D14 (a)) — check `git status` afterwards.**

**Always send:** `game-server/log/`, `unported_trace.txt`, `partial_trace.txt`, `live_counts.txt`, and **every "which is not ported yet" line**
(`AionClientPacketFactory.cpp:122`) — each names a packet the client really sends.

---

## 14. What was measured and what was inferred

**Measured** (read-only scripts in the session scratchpad `plans/m5j/`, re-runnable):

- The per-chunk `AION_UNPORTED`/`AION_PARTIAL` site counts of §2.2 at `760e8ab5c` plus the working tree, `.cpp` files only, comment lines skipped
  (`m5j_sites_r1.py`): 100 sites left in phase 4.
- The 147 missing client-packet files (190 Java files, 43 C++ headers plus `fwd.h`), their Java line counts and their assignment against the
  sibling item tables (`m5j_cmassign_r1.py`), and the two unregistered opcodes.
- The 40 missing root AIs, their `@AIName`s, bodies and lines (`m5j_bodies_r1.py`), and the template and spot counts of §2.5.
- The command counts: 101/16/35 files, 11,823 lines, 532 bodies; the 40 of stage 0: 121 bodies, 2,724 lines; the tiers of §5.3 (`m5j_cmdtier_r1.py`);
  the two-level reachability of the 40 (`m5j_deep40.py`, then read by hand: `TitleList::addTitle`'s helper and `SpawnsData::saveSpawn`).
- The J13 classes (19, 2,193 lines, 201 named bodies) and the J14 bodies (`m5j_bodies_r1.py`).
- The GM enter-world defect's code path and its catch (`GMService.cpp:59-69`, `PlayerEnterWorldService.cpp:344-351, :562`, Java
  `PlayerEnterWorldService.java:167-174`) — **by reading**; nobody has run it.
- The race byte (`SM_MESSAGE.java:121-122, :140`, `Race.java:18`), the whisper order and level (`CM_CHAT_MESSAGE_WHISPER.java`,
  `custom.properties:20`), the non-Daeva cap (`PlayerCommonData.java:276-281`), the channel-chat level (`GSConfig.cpp:12`,
  `SM_VERSION_CHECK.java:95`), the reconnect delays (`ChatServer.java:56-64, :76-82`; `ChatServer.cpp:61-70, :86-89`).
- The write-back triggers (`SpawnNpc.java:46, :64`, `Delete.java:68`, `SpawnsData.java:205-263`), `//kill`'s attacker rule (`Kill.java:93`),
  `//damage`'s self-attack (`Damage.java:80`), `//heal`'s target rule, the `kill = 7` access level (`commands.properties:56`).
- The autogroup startup path (`GameServer.cpp:243, :311-316`, `PeriodicInstanceManager.cpp:26-54`) and the Legion Dominion cron
  (`CronJobService.cpp:84-88, :185-187`).
- The sibling-plan greps of §0 and §2.6 (0 hits), over the drafts as they stood at 23:20.
- That no M5j path calls `GeoService` except `//collide`, `teleport`, `AbyssGuardSimpleAI` (M5d) and `SkillCooltimeResetAI`.

**Inferred, to be confirmed before relying on it:**

- **That the GM defect behaves as described at run time** (what the client shows was not observed). I-02's test settles the code path; the
  stage-0 checklist step 1 settles the client.
- **That a server with the Java default really stops at W-13** — read from `GameServer::runStep` and the observer, not run.
- **Every §0 row.** They are readings of drafts, three of which (m5e, m5f, m5h) were revised while this was written and two (m5g, m5i) are
  unreviewed. I-05 re-runs them at branch time.
- **The reachability of the other 99 "ready" commands**, checked one level deep only.
- **J9's size** — an upper bound until N-05; **J14's matchmaking size** — m5g's measurement, not re-measured here.
- **The body counts of packets** (~3 per packet from phase5-census.md's 452 bodies over 147 files) and of the §5.4 commands (~3.5 per command).
- **The wall-clock extrapolation of §12.**
- **That C1/C2 leases across five milestones do not collide** in `chunks.py check` (not run).

---

## 15. Open questions

1. **D1, D12, D14 and D9's fix are the user's.** Until answered, the integrator runs I-02 (D4) and nothing else of M5j. m5c-plan.md D2 (A-C4) is
   already before the user and decides whether J7 exists.
2. Does the 4.8 client send `CM_BUILDER_COMMAND` at all without a GM-enabled client build? If not, the console commands are reachable only
   through `ChatProcessor.handleConsoleCommand` callers inside the server (`Teleportto` → `GoTo` shows commands call each other), and X9 needs the
   fake client only. The stage-0 real-client session answers it.
3. Does M5e's gate hit the homing/servant/trap stat set-up sites (A-E2r) and take N-06? Its review should be told.
4. Should `gs.scenario.gm` merge into `gs.scenario.m5j` once stage 1 lands, to save one serialized server run (m5b2-plan.md §8 risk 12)?
   Recommended: yes, keeping `gs.scenario.chat` separate because of its third process.
5. Is the ADMINAUDIT/CHAT_LOG logger split (`PlayerChatService`, `AdminCommand`) wired in the C++ logging configuration, or do those lines go
   to the main log? The gates are unaffected; the checklist's log bundle is.
6. Which of the five handler-spawned root AIs (`aggressive_boss_summon`, `bomb`, `firecracker`, `useSkillAndDie`, `neutralguard`: 0 spawn-file
   spots) are spawned only by phase-6 handlers? If all, they are phase-6 prerequisites rather than stage-3 work.

---

## 16. Review, 2026-09-23

The adversarial review of rev 0 returned **needs-revision** with 1 critical, 4 high, 5 medium and 3 low findings, and a list of verified
claims (the 147-packet table, the GM defect path, the framework counts, the chat link, the AI spot counts, the command counts, the per-service
counts). Every finding was re-checked against the trees and the sibling drafts before it was applied.

| # | Severity | Finding | Re-check | What changed |
|---|---|---|---|---|
| 1 | critical | §0 was written when no M5e-M5i draft existed; nine rows now contradict them | **confirmed**, and three of those drafts were revised again during this revision (m5f O-07 withdrew its autogroup fix; m5e rev 2 moved `SpellAtkDrainInstantEffect` into M-03) | §0 re-derived row by row (A-C4, A-C5, A-E2/A-E2r, A-E4, A-E5, A-F1..A-F4, A-G1..A-G3, A-H1, A-H2, A-I1..A-I4); every triggered row is a named item (E-05, S-06, S-11, S-12, S-13, N-01, N-06, X-01); new group J14; §1.1, §1.2, §2.4 (45 must-add packets, `CM_CLIENT_COMMAND_ROLL` out), §2.5 (26 AIs, 1,980 spots), W-12, N-01 and §12 recomputed; D15 names M5j the default owner |
| 2 | high | the "orphaned item enhancement", J7, D5, stage 2 and Z9/Z10 are stale: m5c §3a homes that work | **confirmed** (m5c-plan.md :383-414) | orphan claim withdrawn (header, §2.6); J7 is now group K under A-C4 (b) only; D5 withdrawn; stage 2 re-scoped to m5c's M5j row (pets, `RideAction`, `ExpExtractAction`, `CM_APPEARANCE`, the arcade) and kisks; Z9/Z10 replaced by ride and kisk cases; rev 0's E-05 (`removeStoneStats`, which is M5b-3's P-02) dropped |
| 3 | high | stage 0's "every callee ported" is false for `//addtitle`, `//delete`, `//spawn` | **confirmed** (`TitleList.cpp:25-29, :73`; `SpawnsData.cpp:212-215`; `ItemActions.h:17-27`) | option (a): new K-10 ports the four `registerExpirable` forwarders (P4-12 lease) and `saveSpawn`'s guard prefix (P4-09 lease); `//spawn`'s item arm declared loud (W-06) and moved to M5h's ride with m5b3-h02; a two-level reachability check over all 40 (`m5j_deep40.py`) found nothing else; X8b added |
| 4 | high | X5 expects the wrong race byte | **confirmed** (`SM_MESSAGE.java:121-122, :140` — the review's :45-47/:64 are the field declarations; the logic is at these lines) | X5 now expects 0 for the staff receiver, 1 on P's echo, 2 on an Asmodian's echo (new account Q); §3.1 step 7 rewritten; the factions key stated in §10.1 |
| 5 | high | three plans port the chat framework; M5i's GM account needs I-02 | **confirmed** (m5g K-04/W-01/W-03, m5i D2/Z-01/Z-02/Z-05/:549); also found that M5g's `canChat` reaches four unported bodies it does not list | §0 rows A-G3, A-I1, A-I4; D1 states what M5g and M5i drop if taken and what stage 0 drops if not; D4 names I-02 a prerequisite of M5i's gate; D3 now keeps C1/C2 in phase 6 with per-stage leases (m5i's model) |
| 6 | medium | W-07/D7 describe the write-back triggers wrongly | **confirmed** (`SpawnNpc.java:46, :64`, `Delete.java:68`) | W-07 and D7 rewritten; §5.2 corrected; D14 (**user**) on real-client behaviour; checklist warnings in stage 0 and stage 4 |
| 7 | medium | `RideAction` missing; `ToyPetSpawnAction` is the kisk action | **confirmed** (`ToyPetSpawnAction.java:51-62`; `RideAction.h` declares nothing) | `RideAction` in J6/E-02 with Z9; `ToyPetSpawnAction`, `KiskService`, `kiskRevive`, `KiskAI`, `InvisiblekiskAI` merged into E-05 with Z10; the S-08/E-04 double assignment of `PetList::registerExpirable` resolved by K-10 |
| 8 | medium | the Legion Dominion cron and autogroup's Java default are missing from §4 | **confirmed**, and worse than stated for autogroup: m5f rev 2's O-07 withdrew the enter-world and map-change arms too | W-13 and W-14; items X-01 (stage 4) and S-13 (stage 1); D11 adds "a default-config server starts"; world tour second run; risk 2 |
| 9 | medium | Z2 and checklist steps 5-6 break Java's level rules | **confirmed** (`CM_CHAT_MESSAGE_WHISPER` order; `PlayerCommonData.java:276-281`; `GSConfig.cpp:12`) | §3.6 added; B seeded as a Daeva (m5c D5 recipe); Z2 asserts both the block and the level arm; checklist steps 3, 5, 6 state the limits |
| 10 | medium | X3, X8, Z12, Y6 have wrong or unobservable expectations | **confirmed** | X3 takes the level from H-01 (`kill = 7`); X8 uses `SM_ACTION_ANIMATION(LEVEL_UP)` and heals a target damaged first; Z12 reaches the two template-less AIs through `//ai set`; Y6 and §3.4 step 7 use the 5/15/10/60 s constants |
| 11 | medium | §12's sizing does not match the git-log pace | **confirmed** | §7 and §12 use m5c's size classes; the time figure is an explicit extrapolation from the commit times (three to five days) |
| 12 | low | §3.3 describes the GM defect's consequence wrongly | **confirmed** (`PlayerEnterWorldService.cpp:344-351`) | §3.3, header finding 2 and checklist step 1 corrected; I-02's test asserts entry and the steps after `:562` |
| 13 | low | D-07 is reachable from stage-0 commands | **confirmed** (`Kill.java:93`) | W-15; stage-0 checklist warning; the PvP half is M5j's own S-12 |
| 14 | low | counts and citations that do not reproduce | **confirmed** for each | P5-11 86/3, P5-12a 105, P5-12b 197/1 (site regex now `.cpp`-only); P5-02a/b 0 / 0; phase 4 100; 238 `SM_*`; J13 19 classes / 2,193 lines; 9 `CM_HOUSE*`; W-10's rows at `m5b_partial_allowlist.txt:76-77`; G-02 has no `:527` row to move; chat-server-port.md :98-106; the `toString` bodies left stage 0; `SkillCooltimeResetAI`'s `GeoService` call noted; P5-09a named; the manastone arms and `CM_QUEST_SHARE` moved to M5c and M5g |

**Findings rejected:** none. Two were narrowed rather than applied as written: (a) finding 1's claim that `CM_CHANGE_CHANNEL` "joins the
must-add list" — it does, but only unless M5f takes its optional P-06, so it is counted in the 45 and marked droppable; (b) finding 3's option
between porting the prerequisites and dropping the commands — the plan ports them (option a), because the forwarders are one line each and the
guard prefix returns before any file I/O, so `//addtitle` and `//delete` stay in the minimal set.
