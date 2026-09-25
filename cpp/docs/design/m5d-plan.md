# M5d work plan (quest engine)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 revised after its adversarial review; §16 lists what the review found and what changed.
> A **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2: the cast engine and the effect
> core") plus the working tree, whose uncommitted M5b-2 part-3 lanes touch `skillengine/effect/*` and `AttackUtil.cpp` and **no file this plan
> names** (checked with `git status`). **Nothing was compiled, built or run for this plan.** C++ statements come from reading both trees, from
> `game-server/chunks.cmake` and `tools/porting/chunks.py owner`, and from counting `AION_UNPORTED(` / `AION_PARTIAL(` sites. Data statements
> come from parsing `data/static_data` with throw-away scripts (ElementTree, so XML comments are skipped as the server skips them) and from
> two read-only oracle runs (`oracle.py m5a-creation`, `oracle.py m5b-monster`), which read the Java tree and print. §14 separates what was
> **measured** from what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md). Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 4,
> [m5a-plan.md](m5a-plan.md) W-02, [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §1 and the P5-06 acceptance row (line 647),
> [m5a-client-session.md](m5a-client-session.md) F-2, [m5b-client-session.md](m5b-client-session.md) S-1, `tests/scenario/m5a_partial_allowlist.txt`,
> `tests/scenario/m5b_partial_allowlist.txt`, `docs/deviations/P5-06.md`, `docs/porting/header-requests.md` shells-4/shells-5.
>
> **Refreshed 2026-09-24** (read-only again: nothing was built or run except Python) against HEAD `5fbb03a08` ("M5b-3 complete"). The
> refresh was asked for at `4867fbc44`; `5fbb03a08` came after it and changed only `tests/scenario/**`, `tools/oracle/**` and docs, so every
> production number here is also `4867fbc44`'s. The working tree holds **M5c's stage 0, uncommitted and unbuilt** (the dialog lane's
> `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_QUESTION_RESPONSE`, `DialogService`, `PostboxAI`, and M5c's I-01 split of
> P5-09); where that matters, both states are given. The inputs were m5c-plan.md's refresh and its review (§15-§16 there), m5b3-plan.md
> §15-§20 and [phase6-questgen-prototype.md](phase6-questgen-prototype.md) rev 2. **§17 lists every change and its reason.** The body is
> edited only where a statement had become wrong, and each such edit says "(refresh)". The user's decisions (D13, D16) stay open.
>
> **Three corrections to the roadmap, in order of how much they change the milestone.**
>
> 1. **"About 4,184 XML-template quests come online with P5-06" is half true.** The number is exact: 4,184 `quest_script_data` elements, 4,184
>    distinct ids, **none** of which also has a Java handler. But after this milestone only **2,511** of them have a start npc that the server
>    spawns at startup and a player can reach without a phase-6 script (439 of those only because §6 D5 pulls a 75-line AI forward; 310 more
>    have their giver only in siege, instance, base, vortex or Ahserion spawns, which wait for M5f and M5i), and only **901** need nothing but
>    dialogs and kills — **795** of them if the four reward bodies of E-09 stayed unported. The rest wait for loot (505), crafting (586),
>    quest objects (135), items from outside the quest (316) or are unreachable (1,673). §2.4 has the whole decomposition. **And not one quest
>    can pay its reward before M5b-3**: every kinah reward goes through `ItemPacketService::sendItemPacket`, which is `AION_UNPORTED` (§3.5).
>    (Refresh: M5b-3 has ported it and `ItemService::addItem`; §17.2.)
> 2. **P5-06 is 268 bodies, not 163.** 163 `AION_UNPORTED` sites plus **105 bodies no site count can see**: the 17 template handler classes
>    (`ReportTo`, `MonsterHunt`, `ItemCollecting`, …) that *are* the XML quests have **no C++ file at all** (75 bodies, 2,067 Java lines), plus
>    the `task/` package, `QuestSpawnAnalyzer`, `KillOperation`, five undeclared methods in the xmlQuest shells and `HandlerResult.fromBoolean`
>    (§4.2). Adding the dialog plumbing, three npc AIs and the four reward services outside the chunk (E-09), the milestone is **~318 bodies
>    over ~9,320 Java lines**. (Refresh: M5b-3 ported four of the 163 sites, so the chunk is 159 + 105 = 264 bodies. Adding the 8 quest-item
>    action bodies that m5c-plan.md §3a hands to M5d (E-10), the milestone is ~322 bodies, and ~307 once M5c's stage 0 lands; §17.1.)
> 3. **Closing the one partial that registers the XML quests wakes code in every existing gate.** Ten `report_on_levelup` quests register for
>    *every* enter world and every level change — and **every fresh character's first enter world is a level change** (0 → 1,
>    PlayerEnterWorldService.java:204). Their hook calls `QuestService::startQuest`, which is `AION_UNPORTED` — so a registration that lands
>    before the engine would put an ERROR line into every enter world of `gs.scenario.m5a`, `m5b` and `m5b2` (refresh: and `m5b3`, and `m5c`
>    once it exists). The registration is therefore
>    the **last commit of stage 1** (D3), as M5b-2's D11 did for passive skills.

---

## 1. Summary

**M5a left the quest engine a dispatcher with an empty registry, and it is a good dispatcher.** Everything that *routes* a quest event is
ported; nothing that *handles* one is.

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `QuestEngine` — `init` (except two partials), all 33 event dispatchers, `getQuestNpc`, all `register*` methods, `addQuestHandler`, `sendCompletedQuests`, the 09:00 message cron | `questEngine/QuestEngine.cpp` (950 lines): **1** `AION_UNPORTED` (`reload`, `:121`) and **2** `AION_PARTIAL` (`:111` the XML registration, `:115` the spawn analyzer) |
| The hooks that call the engine: kill (`NpcController::doReward` → `onKill`), talk (`TalkEventHandler::onTalk` → `onDialog`), enter world, level change, zone, distance, aggro, item get/remove, skill use, logout | `controllers/NpcController.cpp:262-265`, `ai/handler/TalkEventHandler.cpp:34`, `network/aion/clientpackets/CM_LEVEL_READY.cpp:108,113`, `controllers/PlayerController.cpp:298,312,423,693`, `model/items/storage/Storage.cpp:188,235`, `skillengine/model/Skill.cpp:753` (refresh: `:751`); **refresh:** item use, `CM_USE_ITEM.cpp:109-112` (M5b-3) |
| The quest markers: `WorldMapInstance::addObject` collects the start-quest ids of every spawned npc, `PlayerController::updateNearbyQuests` filters them through `QuestService::checkStartConditions` and sends `SM_NEARBY_QUESTS` | `world/WorldMapInstance.cpp:77-90`, `controllers/PlayerController.cpp:263-270` (Java WorldMapInstance.java:109-131, PlayerController.java:170-177) |
| `QuestService::checkStartConditions` ×2, `checkCombineSkill`, `inventoryItemCheck`, the level requirement helpers, the quest-drop registry; **refresh:** the four quest-drop bodies `getQuestDrop`, `isQuestDrop`, `allowLooting`, `regQuestDropItem` (M5b-3 L-04) | `services/QuestService.cpp` — 10 of 36 bodies; **refresh: 14 of 36** (`:368-519`) |
| `NpcController::onDialogRequest` / `onDialogSelect`, `GeneralNpcAI::handleDialogStart`, `TalkEventHandler`, `getStartPageId` | `controllers/NpcController.cpp:284-306`, `handlers/ai/GeneralNpcAI.cpp:42-48` (refresh: `handlers/aion/gameserver/handlers/ai/GeneralNpcAI.cpp:46-47`), `model/DialogPageInfo.cpp:17-29` |
| `QuestStateList`, `PlayerQuestListDAO` (load, store, add, update, delete), every quest template class, `XMLQuests` in Java's `HashMap` order | 0 unported in each; `dataholders/XMLQuests.cpp:12-18` |
| **Every quest server packet**: `SM_QUEST_ACTION`, `SM_QUEST_LIST`, `SM_QUEST_COMPLETED_LIST`, `SM_NEARBY_QUESTS`, `SM_QUEST_REPEAT`, `SM_DIALOG_WINDOW`, `SM_PLAY_MOVIE`, `SM_QUESTION_WINDOW`, `SM_SHOW_NPC_ON_MAP`, `SM_USE_OBJECT`, `SM_LOOKATOBJECT` | all exist, **0 `AION_UNPORTED`** in each (measured) |

**What is empty is the handling side**, in five pieces (the refresh adds a sixth):

| # | Hole | Where | Size (measured) |
|---|---|---|---|
| 1 | **The template handlers** — the 17 classes an XML quest turns into. No `.h`, no `.cpp`; only `handlers/template/fwd.h` names them. | P5-06 | 75 bodies, 2,067 Java lines |
| 2 | **The handler base** `AbstractQuestHandler` — the dialog, step, item and spawn helpers every template and every phase-6 handler calls | P5-06 | 88 of 91 bodies unported, 1,290 Java lines |
| 3 | **The quest state and the service** — `QuestState` 13, `QuestVars` 3, `QuestEnv` 1, `QuestService` 26 (start, finish, rewards, abandon, timers, quest drops) | P5-06 | 43 bodies; **refresh: 39** (`QuestService` 22: M5b-3 ported the four quest-drop bodies) |
| 4 | **The way in**: `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_DELETE_QUEST` have no C++ file; `DialogService` is 7 of 7 `AION_UNPORTED`; the AIs of the givers of 476 XML quests (`simple_abyssguard`) and of 2,720 quest-object spots spawned at startup (`quest_use_item`) are not ported. **Refresh:** still so at HEAD; in the working tree M5c's stage 0 has written the first three packets, `CM_QUESTION_RESPONSE` and `DialogService` (one in-arm `AION_UNPORTED` left, `DialogService.cpp:314`), uncommitted (§17.2) | P5-08, P5-15, P5-16, P5-05, A1 | ~40 bodies, ~1,105 Java lines |
| 5 | **The reward bodies outside the chunk** that `finishQuest` calls: `BonusService::getQuestBonus` (P5-09), `AbyssPointsService::addAp` and `GloryPointsService::addGp` (P5-08), `CubeExpandService::questExpand` (P5-07) — all `AION_UNPORTED` (§3.5 row 16). **Refresh:** `BonusService` is P5-09a after M5c's split; `CubeExpandService` is M5c's required P-05 (stage 1) | P5-09, P5-08, P5-07 | ~10 bodies, ~150 Java lines (E-09) |
| 6 | **Refresh — the quest-item actions**: `QuestStartAction` and `ReadAction`, 2 `canAct`/`act` stubs each (`m5b3-h01`), `finishUse` each and an `ItemUseObserver` each. `CM_USE_ITEM` reaches the stubs at HEAD, and they throw. m5c-plan.md §3a and m5b3-plan.md O-03 send them to M5d | P5-07 | 8 bodies, 155 Java lines (E-10) |

**The decisive finding is §2.5:** a fresh Elyos character can run the chain **1101 "Sleeping on the Job" → 1102 "Kerubar Hunt"** with nothing
but dialogs and kills of the striped kerub M5b-2 already targets, and the quest data gives the exact rewards (120 kinah + 130 exp, then
400 kinah + 180 exp). Finishing 1101 at Mires makes the engine open 1102's start dialog by itself (the follow-up branch of
`AbstractQuestHandler.sendQuestEndDialog`, AbstractQuestHandler.java:450), and finishing 1102 opens 1103's. That is the gate (§10).

---

## 2. Measured: the quest data and how the engine loads it

### 2.1 The files

| File | What it holds | Loaded by |
|---|---|---|
| `data/static_data/quest_data/quest_data.xml` | **8,043** `<quest>` templates: race, levels, category, rewards, `<quest_kill>`, `<quest_drop>`, `<collect_items>`, `<quest_work_items>`, `<start_conditions>` | `DataManager.QUEST_DATA` (static_data.xml:122) |
| `data/static_data/quest_data/challenge_tasks.xml` | 122 `<task>` elements — town/legion challenge tasks, **not** quests | static_data.xml:121 |
| `data/static_data/quest_script_data/*.xml` | **89** files (plus the XSD), **4,184** elements in 15 kinds; 10 files are empty (`abyss_entry`, `ascension`, `atreia_happiness_guild`, `bare_truth`, `carving_out_a_fortune`, `clash_of_destiny`, `convent_of_marchutan`, `hero`, `the_hidden_truth`, `time_of_return`) | `DataManager.XML_QUESTS` (static_data.xml:123, `singleRootTag`) |
| `data/handlers/quest/**/*.java` | **1,035** handler classes, 95,539 Java lines — phase 6 (chunks Q01–Q14) | `ScriptManager` + `QuestHandlerLoader` in Java; the `AION_QUEST_HANDLER` registry in C++ (`handlers/HandlerRegistry.h:127-133`, **0 entries today**) |

**How `QuestEngine.init` combines them** (QuestEngine.java:86-110): quest drops and inventory items from `QUEST_DATA`; then
`scriptManager.load(QUEST_HANDLER_DIRECTORY)` registers the 1,035 Java handlers (:103); **then** `for (XMLQuest xmlQuest : XML_QUESTS.getAllQuests())
xmlQuest.register(this)` (:104-105), where each `register` constructs a template handler and calls `addQuestHandler`. `addQuestHandler` is
`putIfAbsent` with a "Duplicate handler" warning (QuestEngine.java:892-898), so **a Java handler would win over an XML one — but no quest id is
in both sets** (measured: 0 overlap). The C++ port keeps the order: registry handlers first, then the XML loop, which today is the
`AION_PARTIAL` at `QuestEngine.cpp:110-111`.

### 2.2 The 15 XML kinds

The JAXB mapping is `XMLQuests.java:19-27` (16 element names; `mentor_monster_hunt` is mapped and unused).

| Kind | Quests | Template class (Java lines) | What completing it needs | Milestone that supplies it |
|---|---|---|---|---|
| `item_collecting` | **1,681** | `ItemCollecting` (179) | the `<collect_items>`: quest drops from monsters (loot), from 7xxxxx quest objects, or items from elsewhere | M5b-3 loot (**done**, refresh); §6 D5 for objects; gathering is unassigned (the user's D13) |
| `monster_hunt` | **1,163** | `MonsterHunt` (289) | the `<quest_kill>` counts in `quest_data.xml` | **M5b-1 (done)** |
| `work_order` | 574 | `WorkOrders` (105) | crafting; every one has a `<bonus>` | M5c; E-09 for the bonus |
| `report_to` | 468 | `ReportTo` (108) | dialog only (plus a work item on accept for some) | **M5d** |
| `report_to_many` | 73 | `ReportToMany` (188) | dialog with several npcs | **M5d** |
| `kill_in_world` | 50 | `KillInWorld` (168) | PvP kills (`PvpService.java:267`) | a PvP milestone |
| `crafting_rewards` | 42 | `CraftingRewards` (91) | a crafting skill | M5c |
| `item_order` | 32 | `ItemOrders` (121) | using a start item (`CM_USE_ITEM`) | **M5d E-10 for 27, `CM_USE_ITEM` (M5b-3, done) for 5** (refresh, corrected after review: 27 start items also carry `<queststart>` for the same quest, and `CM_USE_ITEM.cpp:108-112` skips `onItemUseEvent` for those, so they start only through `QuestStartAction.finishUse`, QuestStartAction.java:82-87; the 5 without it, 1323, 16904, 26904, 30007 and 30107, route through `CM_USE_ITEM.cpp:109-112`; §17.3 N1) |
| `relic_rewards` | 30 | `RelicRewards` (103) | turning in relics the player owns; every one pays AP | M5b-3 (items, **done**); E-09 for AP |
| `skill_use` | 30 | `SkillUse` (127) | casting listed skills (`Skill.java:640`) | M5b-2 |
| `kill_spawned` | 13 | `KillSpawned` (138) | killing npcs a quest object spawns | M5d + objects |
| `kill_in_zone` | 12 | `KillInZone` (130) | PvP kills in a zone (`PvpService.java:266`); every one pays GP | a PvP milestone; E-09 for GP |
| `report_on_levelup` | 10 | `ReportOnLevelUp` (66) | reaching a level (the ten stigma quests, `stigma.xml:19-28`, levels 30-55) | M5d |
| `fountain_rewards` | 5 | `FountainRewards` (81) | turning in coins; every one has a `<bonus>` | M5b-3 (items, **done**); E-09 for the bonus |
| `xml_quest` | **1** (1127 "Ancient Cube", Poeta) | `XmlQuest` (104) + the xmlQuest mini-language (conditions, operations, events) | a quest object and XML-scripted steps | M5d + objects |
| **Total** | **4,184** | 16 classes + `AbstractTemplateQuestHandler` (10) = **2,067** lines | | |

### 2.3 Java handlers against XML templates

| | Quests | Note |
|---|---|---|
| In `quest_data.xml` | **8,043** | categories: QUEST 4,542, EVENT 832, IMPORTANT 672, TASK 574, MISSION 312, FACTION 364, SIGNIFICANT 215, CHALLENGE_TASK 174, PUBLIC 145, SEEN_MARKER 133, NON_COUNT 55, PRIMARY 20, LEGION 5 |
| XML template (`quest_script_data`) | **4,184** | all 4,184 ids exist in `quest_data.xml`; 0 duplicates across the 89 files |
| Java handler (`data/handlers/quest`) | **1,035** | 1,030 `super(N)` + 5 `super(_questId)` with a constant; all in `quest_data.xml`; 0 duplicates |
| **Both** | **0** | so no XML handler is shadowed by `putIfAbsent` |
| Neither | **2,824** | QUEST 1,855, EVENT 376, MISSION 199, FACTION 115, IMPORTANT 99, PUBLIC 99, SIGNIFICANT 25, NON_COUNT 23, CHALLENGE_TASK 18, SEEN_MARKER 10, LEGION 3, PRIMARY 2 — templates without any handler; they never start |

1,845 of the 4,184 XML quests carry `restricted="true"`. **That attribute changes nothing**: `QuestTemplate.isRestricted()` (QuestTemplate.java:377-381,
"in client has any bm_restrict_category") has no caller anywhere in `src` or `data/handlers`.

### 2.4 What "come online" can mean: the 4,184 decomposed

**Can a player start it?** A start npc is reachable when **the server spawns it at startup** and its AI's `handleDialogStart` runs
`TalkEventHandler.onTalk` — `general` and `aggressive` do (`AggressiveNpcAI extends GeneralNpcAI`); every AI name without a C++ handler gets a
`DummyNpcAI` whose `handleDialogStart` does nothing (`ai/AIEngine.cpp:156-170`, gameserver.dev.missing_ai_handlers=warn).

"Spawned at startup" (rev 2; rev 1 counted every file under `spawns/**`): `SpawnEngine.spawnAll` skips instance maps (SpawnEngine.java:119-124)
and spawns a map's regular spawns — the `<spawn>` elements directly under `<spawn_map>` — except those with `handler="RIFT"`, which go to the
`RiftManager` (SpawnEngine.java:155-158). Siege, base, rift, vortex, mercenary and Ahserion spawns are nested elements that `SpawnsData` keeps
in their own maps (SpawnsData.java:56-77, 101-118, 185-199) for their services to spawn — and those services are disabled or partial in C++
(`SiegeService.cpp:89` unported, `BaseService.cpp:18` partial). Measured over `spawns/**`: 12,244 direct spawns on world maps in `Npcs/`, 42 in
`Statics/` (`handler="STATIC"`, spawned), 493 in `Gather/`; not at startup: 46 direct spawns of `Npcs/` on instance maps, 3,736 in
`Instances/`, 28 `RIFT` spawns, and 3,215 siege, 1,558 base, 117 rift/vortex, 148 mercenary and 137 Ahserion spawns.

**Preconditions** (rev 2; rev 1 read only the first `<start_conditions>` group): a `<start_conditions>` group that holds a `finished` list is
*optional* — Java requires one optional group plus every other group (`QuestTemplate.getRequiredConditionCount`, QuestTemplate.java:170-187;
`XMLStartCondition.isOptional`, XMLStartCondition.java:46-48; the count at QuestService.java:365-372). A quest is **blocked by phase 6** when
fewer groups can pass than are required, counting a group as failing if it names a Java-handled quest under `finished` or `acquired`
(`unfinished` and `noacquired` on a never-started Java quest pass). 98 XML quests have more than one group.

| Start | Quests |
|---|---|
| start npc spawned at startup, AI `general`/`aggressive` (ported), not blocked by a Java-handled precondition | **2,072** |
| start npc spawned at startup, AI **`simple_abyssguard`** (not ported; `AbyssGuardSimpleAI.java`, 75 lines), not blocked | **439** |
| as either above, but blocked by a Java-handled precondition (phase 6) | 307 (270 + 37) |
| start npc spawned only by a service: sieges 185, instances 91, bases 41, vortex 3, Ahserion's Flight 2 — **M5i and M5f** | 322 |
| start npc never spawned by any spawn file (spawned by phase-6 handlers, events, or not at all) | 498 |
| no start npc (started by an item, a zone, a level, a kill or another handler) | 415 |
| `minlevel_permitted="99"` (disabled in data) | 126 |
| start npc with another unported AI | 5 |
| **Total** | **4,184** |

**Reachable after M5d = 2,072 + 439 = 2,511.** Of the 322 service-spawned, 310 would otherwise be reachable (siege 182, instance 82, base 41,
vortex 3, Ahserion 2): they wait for M5i (sieges, bases, vortex, Ahserion) and M5f (instances). What completing the 2,511 needs:

| Completion needs | Quests of the 2,511 | Kinds |
|---|---|---|
| dialogs and kills only | **901** | report_to 272, monster_hunt 609, report_to_many 19, xml_quest 1 — **106 of them** pay AP (82), a bonus item (34), GP (2) or a cube expansion (1) through E-09's bodies; **795** without them |
| quest loot from monsters | 505 | item_collecting — **M5b-3** |
| crafting | 586 | work_order 574, crafting_rewards 12 — **M5c**; **every** work order also has a `<bonus>` (E-09) |
| items obtained outside the quest's own drops (gathering, vendors, crafting, ordinary loot) | 316 | item_collecting — gathering has no milestone yet |
| quest objects (`quest_use_item` AI) and their drops | 135 | item_collecting — D5 + M5b-3 |
| turn-in of relics or coins the player owns | 30 | relic_rewards 26 (**all** pay AP), fountain_rewards 4 (**all** have a bonus) — M5b-3 items + E-09 |
| skill use | 24 | skill_use — M5b-2 |
| PvP kills | 14 | kill_in_zone 12 (**all** pay GP), kill_in_world 2 |

**The reward bodies of E-09** (§3.5 row 16), measured over all 4,184 XML quests: 760 have a `<bonus>` (all 574 work orders among them), 363 pay
AP, 57 GP, 2 extend the cube (`extend_inventory="1"`; 0 extend the warehouse); 1,115 carry at least one. Within the 2,511: 687 bonus, 169 AP,
14 GP, 1 cube — **844** quests. **No Poeta or Ishalgen quest has any of them**, so the gate (§10) does not depend on E-09. The 156 XML quests of
category `CHALLENGE_TASK` would also reach the unported `ChallengeTaskService::onChallengeQuestFinish`/`onAcceptTask` (P5-10,
QuestService.java:105-106, 438-439); none of them is reachable (106 have no start npc, 50 an unspawned one).

And **every** reward path crosses M5b-3 (§3.5): 2,676 of the 4,184 have an item reward and nearly all others a kinah reward. (Refresh:
M5b-3 has landed, so this dependency is met at `5fbb03a08`. The Java tree's `src/` and `data/` have not changed since `c1edb0afb` (only
`config/*.properties.example`), so every count in §2 still holds.)

### 2.5 Poeta and Ishalgen, quest by quest

`quest_zone="Poeta"`: **48** quests — **28 XML, 13 Java, 7 with no handler**. `quest_zone="Ishalgen"`: **52** — **28 XML, 17 Java, 7 none**.

**Java-handled (phase 6):** Poeta 1000, 1001, 1002, 1003, 1004, 1005, 1100, 1107, 1111, 1114, 1122, 1123, 1205 (`data/handlers/quest/poeta/`);
Ishalgen 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2100, 2106, 2114, 2122, 2123, 2125, 2132, 2135, 2136 (`data/handlers/quest/ishalgen/`).
These are the prologue (with its movie), all the campaign missions and "A New Skill". **No handler:** Poeta 1128 (level 99), 9612-9614 ("[Test]"),
80617, 80621, 80643 (events); Ishalgen 2111, 2130 (level 99), 2150, 2151, 80619, 80622, 80644.

**The 56 XML quests**, with what each needs beyond the M5d engine. "M5b-3 packets" means kinah (`ItemPacketService`), reward items and work
items (`ItemService`); **every row needs M5b-3** (rev 1 exempted 2109, but its three items drop from quest object 700125, so it needs M5b-3's
loot and `ItemService`). Rewards are from `quest_data.xml` `<rewards>`; no start-map quest has a `<bonus>`, AP, GP or cube reward (§2.4).
The start-map quests are unaffected by rev 2's startup-spawn scope (§2.4): every giver below is a regular spawn of `210010000_Poeta.xml` or
`220010000_Ishalgen.xml`. **Refresh:** "M5b-3 packets", "loot" and "work item" are met at HEAD (§17.2). `oracle.py m5d-quest` (commit
`83db3742e`) re-derives the rewards of 1101, 1102, 1103, 2101 and 2102 exactly as listed. It also re-derives the follow-up windows D6
relies on: 1102 after 1101, 1103 after 1102, page 10 after 2101, 2103 after 2102. Four start-map items carry `<queststart>`: Poeta's
182200214 (quest 1114) and 182200501 (1107), and Ishalgen's 182203120 (2122) and 182203130 (2136, also `<read>`). All four start
**Java** quests, so after M5d they stay silent (D9, E-10).

| Id | Name | Kind | Lvl | Precondition | Also needs | Reward |
|---|---|---|---|---|---|---|
| **1101** | Sleeping on the Job | report_to 203049 elpas → 203057 mires | 1 | – | M5b-3 packets | 120 kinah, 130 exp |
| **1102** | Kerubar Hunt | monster_hunt at mires: 3 × 210133/210134 | 1 | 1101 | M5b-3 packets | 400 kinah, 180 exp |
| 1103 | Grain Thieves | item_collecting at mires: 3 × 182200201 from object 700105 | 1 | 1102 | quest object AI (D5), drops | 290, 590 |
| 1104 | Report to Polinia | report_to mires → polinia | 1 | 1103 | – | 90, 520 |
| 1105 | The Snuffler Headache | item_collecting, drops of 210079 | 1 | – | loot | 450, 535 |
| 1106 | Helping Kales | report_to kales → uno | 1 | 1105 | work item | 300 exp |
| 1108 | Uno's Ingredients | item_collecting, drops of 210668/210205 | 2 | – | loot | 1,240, 671 |
| 1109 | Abandoned Goods | item_collecting, object 700106 | 3 | – | quest object AI, drops | 770, 825 |
| 1110 | A Book for Namus | report_to melampus (aggressive) → namus | 4 | – | work item | 170, 495 |
| 1112 | To Fish in Peace | monster_hunt: 5 brax + 5 slink | 3 | – | – | 1,810, 1,375, 30 × 169300002 |
| 1113 | Mushroom Thieves | monster_hunt: 8 bellepigs | 4 | – | – | 1,738 exp, selectable + 3 × 160003001 |
| 1115 | The Elim's Message | report_to_many namus → feira, asteros | 4 | – | – | 680, 2,673 |
| 1116 | Pernos's Robe | item_collecting, drops | 5 | – | loot | 2,080, 3,014 |
| 1117 | Light up the Night | item_collecting, drops | 4 | – | loot | 1,690, 2,475 |
| 1118 | Polinia's Ointment | report_to_many polinia → kustanon, melponeh | 6 | – | work item | 370, 2,167 |
| 1119 | A Taste of Namus's Medicine | report_to namus → lonian | 6 | – | work item | 560, 1,848 |
| 1120 | Thinning out Worgs | monster_hunt at **tula (simple_abyssguard)** | 6 | – | AI (D5) | 3,040, 4,455, selectable |
| 1121 | Oz's Prayer Beads | item_collecting at **oz (simple_abyssguard)** | 6 | – | AI (D5), loot | 2,360, 3,685 |
| 1124 | Avenging Tutty | item_collecting, drops | 7 | **1123 (Java)** | phase 6 | – |
| 1125 | Suspicious Ore | item_collecting poa → kalio, object 700107 | 7 | – | quest object AI, drops | 4,050, 6,094 |
| 1126 | Mushroom Research | item_collecting, drops | 7 | – | loot | 3,300, 5,445 |
| 1127 | Ancient Cube | **xml_quest**, object 700001 | 2 | – | quest object AI, xmlQuest language, work item 182200215 (quest_data.xml:1107-1109) | 2,400, 4,015 |
| 1129 | Scouting Timolia Mine | item_collecting, drops | 8 | – | loot | 7,205 exp, 125001836 |
| 1206 | Collecting Aria | item_collecting, 3 × 152000401 Aria | 7 | – | **gathering** | 1,441 exp, 3 × 162000002 |
| 1207 | Tula's Music Box | item_collecting at tula, 3 × 152000201 Iron Ore | 7 | – | gathering, AI | 3,245 exp, selectable |
| 1230, 1231 | Message to Madeline, A House Guest | report_to 801032 ↔ 801033 | 3 | – | **unreachable**: both npcs are only in commented-out "Fast Track" spawn blocks, `210010000_Poeta.xml:378-381` (801032) and `:812-815` (801033) | – |
| 80158 | [Event] Ahead of the Trends | item_collecting | 99 | – | **unreachable** (level 99, start npc 830533 not spawned) | – |
| **2101** | On Your Feet! | report_to 203500 asak → 203504 vandar | 1 | – | – | 80 kinah, 130 exp |
| **2102** | A Bloody Task | monster_hunt at vandar: 4 × 210363/210364 | 1 | – | – | 120, 180, **10 × 169300002** |
| 2103 | The Sprigg Report | report_to vandar → guheitun (aggressive) | 1 | 2102 | – | 410, 590 |
| 2104 | Meeting the Fruit Quota | item_collecting, object 700124 | 1 | – | quest object AI, drops | 770, 520 |
| 2105 | Sparkle and Shine | item_collecting, drops | 1 | – | loot | 530, 420 |
| 2107 | Return to Sender | item_order ulgorn → linevir | 3 | – | **unstartable, in Java too**: `ItemOrders` starts it only when the player holds start item 182203107 (ItemOrders.java:41, 68), and that item (item_templates.xml:878856-878863) has no source anywhere — it appears only as 2107's own work item (quest_data.xml:10091); no drop, drop rule, reward or handler gives it | 490, 825 |
| 2108 | The Shinier the Better | item_collecting, drops | 2 | – | loot | 1,155 exp, 162000002 |
| 2109 | Love in Bloom | item_collecting, object 700125 | 3 | – | quest object AI, drops (M5b-3 loot and items) | 517 exp |
| 2110 | Idle Hands | report_to kaindal → motgar | 3 | – | – | 2,112 exp, 160003503 |
| 2112 | Urd's Request | item_collecting, drops | 3 | – | loot | 2,970, 1,925 |
| 2113 | Token of a Lost Love | item_collecting at dabi | 4 | – | AI (D5), loot | 1,705 exp, 2 × 162000007 |
| 2115 | Sap for Leather | report_to dabi → dalor | 5 | – | AI, work item | 1,370, 1,353 |
| 2116 | A Treasure Map | item_collecting at lidun, object 700139 | 5 | 2137 | AI, quest object AI | 2,080, 2,255 |
| 2117 | Food Bandits | monster_hunt at mijou: 9 karnifs | 5 | – | AI | 1,705 exp, selectable |
| 2118 | Up to no Good | item_collecting at derot | 5 | – | AI, loot | 2,365 exp, 4 × 160003002 |
| 2119 | Black Opal | item_collecting at lidun, object 700127 | 5 | 2116 | AI, quest object AI | 2,530 exp, title 52, 121000749 |
| 2120 | A Brax Skin Rug | item_collecting, drops | 5 | – | loot | 3,760, 1,595 |
| 2121 | Checking on a Friend | report_to mijou → delle | 5 | – | AI, work item | 730, 1,078 |
| 2124 | The Captain's Secret | item_collecting at jephani | 6 | – | AI, loot | 3,140, 3,157, selectable |
| 2126 | As Much as You Can Carry | item_collecting at devalin, object 700129 | 7 | – | AI, quest object AI | 4,820, 4,499 |
| 2127 | Pearls Before Swine | item_collecting at devalin | 7 | – | AI, loot | 4,450, 5,555 |
| 2128 | A Good Tonic | item_collecting at alfrigh | 7 | – | AI, loot | 3,720, 5,302, 4 × 162000007 |
| 2129 | Suspicious Activity | monster_hunt nalto → ulgorn: 210408 + 210409 | 8 | – | – | 9,185 exp, selectable |
| 2131 | A Delivery of Leather | report_to dalor → denma | 5 | 2115 | work item | 2,680, 1,573 |
| 2133 | Azpha For Health And Well-being | item_collecting, 3 × 152000451 Azpha | 2 | – | **gathering** | 1,441 exp, 3 × 162000002 |
| 2134 | Alfrigh's Request | item_collecting at alfrigh, Iron Ore | 7 | – | gathering, AI | 3,245 exp, selectable |
| 2137 | A Cursed Opal | report_to derot → lidun | 5 | 2118 | AI, work item | 370, 473 |
| 80160 | [Event] Just One New Thing | item_collecting | 99 | – | **unreachable** | – |

**Start-map totals:** of the 56 XML quests, **46 are playable end to end after M5d** provided M5b-2 and M5b-3 have landed (Poeta 22, Ishalgen 24),
**4 wait for gathering** (1206, 1207, 2133, 2134) and **6 cannot be played** (1230, 1231, 80158, 80160 unreachable; 1124 behind Java quest 1123;
2107 unstartable in Java too). (Refresh: both milestones have landed. Measured with `oracle.py m5c-craft --skill 30001 --level 1 --map …`,
**1206 and 2133 need only `CM_GATHER`**: Young Aria 400601 (Poeta, 71 spots) and Young Azpha 400651 (Ishalgen, 62) take Collection 1,
which every new character has. **1207 and 2134 also need Collection 15**: Iron Ore 152000201 comes from Impure Iron Ore, 400201 on Poeta
(20 spots) and 400251 on Ishalgen (11). m5c-plan.md W-27 and D10 hold the same facts.)
**Without D5**, 16 start-map quests have a quest giver that does not talk: they start at one of **9 `simple_abyssguard` npcs** (203081, 203082
in Poeta; 203534, 203539, 203540, 203541, 203543, 203544, 203548 in Ishalgen — one spawn spot each, 9 of the start maps' 12
`simple_abyssguard` spots). They are 1120, 1121, 1207 (Poeta) and 2113, 2115-2119, 2121, 2124, 2126-2128, 2134, 2137 (Ishalgen); 14 of them if
the two gathering quests 1207 and 2134 are left out, which is the "14" of rev 1. And the Poeta chain stops at 1103 (whose grain sacks are
`quest_use_item` objects) — so 1104 never becomes available. That is why D5 is in scope.

---

## 3. The path a quest takes, end to end

"ported" means the body exists with no `AION_UNPORTED`. The last column marks every dependency on another milestone.

### 3.1 The offer: quest markers

| # | Step | Java | C++ today | Needs |
|---|---|---|---|---|
| 1 | Each spawned npc: `QuestEngine.getQuestNpc(templateId).getOnQuestStart()` ids go into the map instance's `questIds`; if new ones appear, `updateNearbyQuests` runs for every player 1.5 s later | WorldMapInstance.java:109-131 | ported (`WorldMapInstance.cpp:77-90`); **`questIds` stays empty until the XML registration** | M5d |
| 2 | `updateNearbyQuests`: for each map quest id, `checkStartConditions(player, id, false, 2, …)`; send `SM_NEARBY_QUESTS` with bit 17 set for quests up to 2 levels too high (the grey marker) | PlayerController.java:170-177; SM_NEARBY_QUESTS.java | ported; runs at `CM_LEVEL_READY` (`CM_LEVEL_READY.cpp:108`), on level change, on quest start/finish/abandon | M5d |

**Correction to the task statement:** the markers are `SM_NEARBY_QUESTS`, not `SM_NPC_INFO`. `SM_NPC_INFO.java` has no quest field at all (grep
for "quest" finds nothing); `SM_QUEST_LIST` is the journal (§3.6). `checkStartConditions` reaches `QuestState.canRepeat` (`AION_UNPORTED`) as
soon as a player has a COMPLETE quest (QuestService.java:311), inside a `catch` that logs an ERROR.

### 3.2 Talking to the npc

| # | Step | Java | C++ today | Needs |
|---|---|---|---|---|
| 3 | Client sends **`CM_SHOW_DIALOG(targetObjectId)`**; protection stop, trading check, `removeHideEffects` if hidden and the npc cannot talk to invisible players, `npc.getController().onDialogRequest(player)` | CM_SHOW_DIALOG.java:27-42 | **no C++ file** (refresh: none at HEAD; M5c's D-03 wrote it in the working tree, uncommitted) | M5c stage 0 (m5c-plan.md D-03), else M5d (P5-16) — D4; `removeHideEffects` is M5b-2 (done) |
| 4 | `onDialogRequest`: `canInteract`, `isInTalkRange` (talk distance + 1; 5 + 1 m for elpas and mires, their `talk_info` at `npc_templates.xml:1852,1972`), else `STR_DIALOG_TOO_FAR_TO_TALK`; `AI DIALOG_START` | NpcController.java:250-263 | ported (`NpcController.cpp:284-297`) | – |
| 5 | AI: `GeneralNpcAI.handleDialogStart` → `TalkEventHandler.onTalk`: `QuestEngine.onDialog(new QuestEnv(npc, player, 0, USE_OBJECT))`; if no handler consumes it, `SM_DIALOG_WINDOW(npc, DialogPage.getStartPageId(npc, player))` | TalkEventHandler.java:22-45 | ported; **npcs whose AI is `simple_abyssguard` (the givers of 476 reachable-or-Java-blocked XML quests; on the start maps 9 giver npcs for 16 quests) or `quest_use_item` (2,959 spawn spots, 2,720 spawned at startup, 237 on the start maps) get a `DummyNpcAI` and never answer** | D5 |
| 6 | `getStartPageId`: 10 (quest selection) when the npc has a startable quest or an active one; `isInteractionAllowed` first | DialogPage.java:113-125, 186-192 | ported (`DialogPageInfo.cpp:17-29`) — but it **calls `DialogService::isInteractionAllowed`, which is `AION_UNPORTED`** (`DialogService.cpp:27-29`); refresh: still at HEAD, ported in the working tree by M5c's D-02 (`DialogService.cpp:406-410`) | M5c, else M5d (D4) |
| 7 | `QuestEngine.onDialog` for `questId == 0`: every quest in the npc's `onTalkEvent` list, `handler.onDialogEvent(env)`; `catch (Exception)` logs "QE: exception in onDialog" | QuestEngine.java:152-182 | ported | – |
| 8 | Template `onDialogEvent` with `USE_OBJECT` and no state: falls to `AbstractQuestHandler.onDialogEvent` (ASK_QUEST_ACCEPT, the refusals, FINISH_DIALOG), which returns false for USE_OBJECT | AbstractQuestHandler.java:93-117; ReportTo.java:63-107 | **no template class**; `AbstractQuestHandler::onDialogEvent` and `QuestEnv::getTargetId` are `AION_UNPORTED` | M5d |

### 3.3 Selecting and accepting

| # | Step | Java | C++ today | Needs |
|---|---|---|---|---|
| 9 | Client sends **`CM_DIALOG_SELECT(targetObjectId, dialogActionId, extendedRewardIndex, lastPage, questId, unk)`**; unknown action ids are dropped; target 0 or self = a journal action (`can_report` auto-reward → `finishQuest`, else `onDialog`); for an npc: function-dialog and `isInteractionAllowed` audits, then `onDialogSelect` | CM_DIALOG_SELECT.java:44-124 | **no C++ file** (refresh: none at HEAD; M5c's D-03 wrote it in the working tree, uncommitted, with the journal branch at `CM_DIALOG_SELECT.cpp:82-115` and a test that pins `finishQuest`'s throw, §17.3 N5) | M5c stage 0 (m5c-plan.md D-03; its journal branch reaches `finishQuest`, m5c W-12), else M5d (P5-15) — D4 |
| 10 | `NpcController.onDialogSelect`: talk range, `ai.onDialogSelect` (false for a plain npc), `DialogService.onDialogSelect` | NpcController.java:266-272 | ported (`NpcController.cpp:299-306`) | – |
| 11 | `DialogService.onDialogSelect` → `handleQuestDialogueOrSendNextPage`: with a quest id, `QuestEngine.onDialog(env)`; if nothing consumed it, `SM_DIALOG_WINDOW(npc, dialogActionId, questId)` ("next page") | DialogService.java:69-111, 282-291 | **`AION_UNPORTED`** (all 7 `DialogService` bodies, `DialogService.cpp:10-37`, chunk P5-08); refresh: at HEAD still, ported in the working tree (`handleQuestDialogueOrSendNextPage` at `DialogService.cpp:386-397`) | M5c ports it whole (m5c-plan.md D4, D-02), else M5d — D4 |
| 12 | Template, no state, `QUEST_SELECT` (31): `sendQuestDialog(env, startDialogId ?: dataDriven ? 4762 : 1011)` → `SM_DIALOG_WINDOW(npc, 1011, questId)` | ReportTo.java:72-73; AbstractQuestHandler.java:330-352 | no template; `sendQuestDialog` unported | M5d |
| 13 | `QUEST_ACCEPT` (29) / `QUEST_ACCEPT_1` (1002) / `QUEST_ACCEPT_SIMPLE` (20000) → `sendQuestStartDialog(env, workItem)` → **`QuestService.startQuest(env)`** → `SM_QUEST_ACTION(ADD, qs)` + `updateNearbyQuests`; the work item is given (`giveQuestItem`); then page 1003 (or close for SIMPLE) | AbstractQuestHandler.java:373-397; QuestService.java:400-447 | `startQuest` ×2 unported; `QuestState` status setters unported; `giveQuestItem` needs `ItemService::addItem` (`ItemService.cpp:41-65`, 7 overloads unported; refresh: ported by M5b-3) | M5d; work items M5b-3 (done) |

### 3.4 The progress triggers

| Trigger | Java path | C++ today | Needs |
|---|---|---|---|
| **Kill** | `NpcController.doReward` → `QuestEngine.onKill(new QuestEnv(npc, player, 0))` for each rewarded player (NpcController.java:231) → `MonsterHunt.onKillEvent`: packs the kill count into 6-bit quest vars, `updateQuestStatus` → `SM_QUEST_ACTION(UPDATE)` | hook ported (`NpcController.cpp:262-265`), runs on every kill today with an empty registry; `MonsterHunt` has no file; `QuestState::getQuestVarById`/`setQuestVarById` unported | **M5b-1 (done)**; team kills go through `PlayerTeamDistributionService` (a stand-in, P5-10) |
| **Loot** | `DropRegistrationService.registerDrop` → `QuestService.getQuestDrop(dropItems, index, npc, members, player)` adds quest items for players with a START quest at the right `collecting_step` (DropRegistrationService.java:84; QuestService.java:666-733, 754-796) | `registerDrop` is the M5b-1 partial (`DropRegistrationService.cpp:43`); the four quest-drop bodies are P5-06 and unported, and the frozen `getQuestDrop` signature takes a `const` set it cannot add to (`QuestService.h:77`). **Refresh:** all done by M5b-3. The partial is closed, the four bodies are ported (`QuestService.cpp:368-519`), and m5b3-h03 is applied (`QuestService.h:81`, `runtime::RcHashSet<runtime::Ref<DropItem>>&`). `isQuestDrop` still reaches `QuestState::getQuestVarById` (unported) for a START quest whose drop has a collecting step (`QuestService.cpp:472-478`, m5b3 O-06) | **M5b-3 (done)**; E-01 closes the `getQuestVarById` arm |
| **Quest objects** | `QuestItemNpcAI` (`quest_use_item`): `onCanAct` gate, use bar, `onDialog(USE_OBJECT)`, drop registration, die | no C++ AI (chunk A1, phase 6); `ActionItemNpcAI` (`useitem`, P5-05) neither | D5 + M5b-3 drops (done: `AIActions::registerDrop`, `AIActions.cpp:111-113`; `DropService::requestDropList`, `DropService.cpp:162`) |
| **Talk** | a later `CM_DIALOG_SELECT` at an end npc | as §3.3 | M5d |
| **Item get / remove** | `Storage` → `QuestEngine.onItemGet/onItemRemoved` → `updateNearbyQuests` for inventory-condition items | ported (`Storage.cpp:188,235`) | M5b-3 (items) |
| **Item use** | `CM_USE_ITEM` → `QuestEngine.onItemUseEvent` (`ItemOrders`, `ReportToMany` start items); `QuestStartAction` | no `CM_USE_ITEM`. **Refresh:** `CM_USE_ITEM` is ported and calls `onItemUseEvent` for every used item except a `QuestStartAction` item (`CM_USE_ITEM.cpp:109-112`, CM_USE_ITEM.java:89-90). The registry is empty today. After the join, the templates register 39 ids with `registerQuestItem`: 32 `item_order` first work items (ItemOrders.java:41, 47) and 7 `report_to_many` start items (ReportToMany.java:55). **Only 6 of them route through `CM_USE_ITEM`** (`item_order` 1323, 16904, 26904, 30007, 30107 and `report_to_many` 4914). The other 33 also carry `<queststart>` for the same quest, so `CM_USE_ITEM` skips them and they reach `onItemUseEvent` only from `QuestStartAction.finishUse` (QuestStartAction.java:82-87), which is E-10 (review correction, §17.3 N1). None of the 39 drops on the start maps (measured). The `QuestStartAction` and `ReadAction` stubs throw (E-10) | M5b-3 (done) for 6; **E-10 for 33** |
| **Skill use** | `Skill.endCast` → `QuestEngine.onUseSkill` → `SkillUse` | hook ported (`Skill.cpp:753`; refresh: `:751`) | M5b-2 |
| **Enter world / level / zone** | `onEnterWorld` (`CM_LEVEL_READY.java:93`), `onLevelChanged` (PlayerController.java:590), `onEnterZone` | all ported | M5d |
| **PvP kill** | `PvpService` → `onKillInWorld` / `onKillInZone` / `onKillRanked` | PvP service not on this path | a PvP milestone |

### 3.5 Reporting and the reward

| # | Step | Java | C++ today | Needs |
|---|---|---|---|---|
| 14 | At the end npc, `QUEST_SELECT` → page 2375 (`report_to`) or 1352 (`monster_hunt`); `SELECT_QUEST_REWARD` (1009) → checks (kill totals, collect items) → `qs.setStatus(REWARD)`, `updateQuestStatus` → `SM_QUEST_ACTION(UPDATE)`, page 5 (the reward window) | ReportTo.java:82-99; MonsterHunt.java:131-151; AbstractQuestHandler.java:290-296 | unported / no file | M5d |
| 15 | `SELECTED_QUEST_REWARD1..15` (8-22) or `SELECTED_QUEST_NOREWARD` (23) → `sendQuestEndDialog` → **`QuestService.finishQuest`** | AbstractQuestHandler.java:413-470; QuestService.java:77-117 | unported | M5d |
| 16 | `finishQuest`: reward group, reward items (`ItemService.addItem`), **kinah** `inventory.increaseKinah(Rates.QUEST_KINAH…, INC_KINAH_QUEST)`, **exp** `addExp(exp, Rates.XP_QUEST, npc l10n)`, title, AP, DP, GP, cube/warehouse expansion; `setStatus(COMPLETE)`, `SM_QUEST_ACTION(UPDATE)`, `onQuestCompleted`, npc-faction completion, `updateNearbyQuests` | QuestService.java:77-117, 220-244 | exp: **ported** (`PlayerCommonData.cpp:101`, `RatesInfo.cpp:71-72` — `XP_QUEST` has no cap, unlike `XP_HUNTING`'s `expNeed × 0.2`, Rates.java:16-19 vs 29-35); title ported (`TitleList.cpp:61`); **kinah: `Storage::increaseKinah` is ported but ends in `ItemPacketService::sendItemPacket`, which is `AION_UNPORTED`** (`Storage.cpp:91-138`, `ItemPacketService.cpp:19-21`); items: `ItemService::addItem` unported (**refresh: both ported by M5b-3**, `ItemPacketService` 9 of 9, `ItemService` 10 of 10); DP: `PlayerCommonData::addDp` ported (`PlayerCommonData.cpp:224`); npc-faction completion ported (`NpcFactions.cpp:245`). **Unported and outside M5b-3** (rev 2): `getRewardItems` calls `BonusService::getQuestBonus` whenever the template has a `<bonus>` (QuestService.java:197-204; `services/reward/BonusService.cpp:7-9`, P5-09), and `giveReward` calls `AbyssPointsService::addAp` for `ap` (QuestService.java:230-235; `AbyssPointsService.cpp:15-17`, P5-08), `GloryPointsService::addGp` for `gp` (:238-239; `GloryPointsService.cpp:7-9`, P5-08) and `CubeExpandService::questExpand` for `extend_inventory="1"` (:240-241; `CubeExpandService.cpp:19-21`, P5-07; M5c lists it only as optional P-05, m5c-plan.md:387 — **refresh: M5c's rev 2 made P-05 required, stage 1**, so the cube part of E-09 drops once M5c lands). **Refresh:** `getQuestBonus` returns a `new QuestItems(...)` (BonusService.java:36), which the declared `const QuestItems*` returns cannot own — §9's reward-list request. `addAp` reaches `Legion::addContributionPoints` (`Legion.cpp:91`, unported, P5-11) only for a legion member; everything else it reaches is ported (`AbyssRank::addAp`, `AbyssSkillService::updateSkills`, `Equipment::checkRankLimitItems`, `SM_ABYSS_RANK`, `SM_ABYSS_RANK_UPDATE`). `extend_inventory="2"` (`WarehouseService::expand`, unported) occurs in no quest. | **M5b-3 (P5-07) for kinah and items; E-09 for bonus, AP, GP and cube** |
| 17 | Follow-up: after `finishQuest`, the npc's first startable quest whose `finished` precondition is the quest just completed gets its start dialog (`QUEST_SELECT`, `setDialogContinuationFromPreQuest`); otherwise the quest selection page or a closed window | AbstractQuestHandler.java:422-457 | unported | M5d |

**The reward packet is the hard dependency of this milestone.** 1101's 120 kinah cannot be paid without `ItemPacketService`; the throw would
land in `QuestEngine.onDialog`'s `catch` and log an ERROR. The roadmap already puts M5b-3 before M5d; this plan makes it an **entry criterion**
(§8.1). The four bodies M5b-3 does not cover — bonus, AP, GP, cube — reach 844 of the 2,511 reachable quests (§2.4) but no start-map quest;
they are ~10 small bodies, so M5d ports them itself (E-09, D14). Left unported they would not fail cleanly: a `<bonus>` throws in
`getRewardItems` before anything is paid, leaving the quest stuck in REWARD behind an ERROR; `ap` and `gp` throw in `giveReward` **after**
kinah and exp were paid (QuestService.java:222-227 before :230-239) and before `setStatus(COMPLETE)` (:108), so every retry pays again.
(Refresh: the entry criterion is met. M5b-3 ported `ItemPacketService` and `ItemService` at `4867fbc44`, and its gate committed at
`5fbb03a08`.)

### 3.6 Persistence

| Piece | Java | C++ today |
|---|---|---|
| Load at login | `PlayerQuestListDAO.load` → `new QuestState(...)`, `setPersistentState(UPDATED)` | DAO ported; **`QuestState::setPersistentState` is `AION_UNPORTED`** (`PlayerQuestListDAO.cpp:70`) — the first row in `player_quests` makes the DAO log "Could not restore QuestStateList" and return an empty list |
| Store at logout and in the periodic save | `PlayerQuestListDAO.store` (PlayerService.java:84) | DAO ported; calls `QuestVars::getQuestVars` (`PlayerQuestListDAO.cpp:119,144`), `AION_UNPORTED` |
| Journal at enter world | `sendCompletedQuests` (`SM_QUEST_COMPLETED_LIST`, uses `qs.canRepeat()`), `SM_QUEST_LIST(uncompleted)` (uses `getQuestVars()`) | PlayerEnterWorldService.java:237-238; C++ `PlayerEnterWorldService.cpp:460`, `SM_QUEST_COMPLETED_LIST.cpp:29`, `SM_QUEST_LIST.cpp:24` — both packets are ported and **both call unported model bodies** as soon as a quest exists |
| **Refresh: the load outside the login** | `PlayerCommonData.updateDaeva` reads the list through `PlayerQuestListDAO.load(playerObjId)` when the player object does not exist yet, as it does while the level is computed at load (PlayerCommonData.java:276, 588-610) | ported (`PlayerCommonData.cpp:190, 265-289`), so a character of an advanced class with a `player_quests` row hits `setPersistentState` twice at login: two "Could not restore QuestStateList" ERROR lines, and an ascended character (1006/2008 COMPLETE) loads **as a non-Daeva, capped at level 9**. m5c-plan.md's C19 seeds exactly that character (its D5; X21a, checklist step 11): §17.3 N4, D18 |

### 3.7 Abandon, share, timers, movies

`CM_DELETE_QUEST` → `QuestService.abandonQuest` (QuestService.java:849-885): delete or reset to COMPLETE, work items removed, `SM_QUEST_ACTION(ABANDON)`.
`CM_QUEST_SHARE` → group/alliance members (P5-10, M5g). Quest timers (`questTimerStart`, `SM_QUEST_ACTION(TIMER)`) and movies (`playQuestMovie`,
`CM_PLAY_MOVIE_END` → `onMovieEnd`) are used by Java handlers and by `ReportToMany` npc infos with a `movie`; no start-map XML quest uses either.

### 3.8 The client packets with no C++ file

Measured against `network/aion/clientpackets` (42 C++ `CM_*` classes incl. `CM_CASTSPELL` and `CM_REMOVE_ALTERED_STATE` from M5b-2; Java has 188).
**Refresh:** 51 at HEAD (M5b-3's nine, among them `CM_USE_ITEM`, `CM_START_LOOT`, `CM_LOOT_ITEM`). The working tree has 55: M5c's
`CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG` and `CM_QUESTION_RESPONSE`, uncommitted. `CM_DELETE_QUEST`, `CM_QUEST_SHARE`,
`CM_PLAY_MOVIE_END` and `CM_OBJECT_SEARCH` still have no file (census: 3 undeclared bodies each).

| Packet | Java lines | Chunk | Need in M5d | Why |
|---|---|---|---|---|
| `CM_SHOW_DIALOG` | 43 | P5-16 | **R** (M5c stage 0 ports it, m5c-plan.md:227) | the only way to open an npc dialog |
| `CM_DIALOG_SELECT` | 125 | P5-15 | **R** (M5c stage 0, m5c-plan.md:228) | every quest step goes through it |
| `CM_CLOSE_DIALOG` | 37 | P5-15 | **R** (M5c stage 0, m5c-plan.md:228) | `DialogService.onCloseDialog` → `DIALOG_FINISH`, `SM_LOOKATOBJECT`; without it the npc keeps the player targeted |
| `CM_DELETE_QUEST` | 38 | P5-15 | **R** | abandoning from the journal |
| `CM_QUEST_SHARE` | 83 | P5-16 | W | needs groups (M5g) |
| `CM_QUESTION_RESPONSE` | 46 | P5-16 | W (M5c stage 0 ports it, m5c-plan.md:227) | answers `SM_QUESTION_WINDOW` (share acceptance and many non-quest prompts) |
| `CM_PLAY_MOVIE_END` | 57 | P5-16 | W | `onMovieEnd`; no start-map XML quest plays a movie |
| `CM_OBJECT_SEARCH` | 47 | P5-16 | O | the journal's "show on map"; `SM_SHOW_NPC_ON_MAP` exists |
| `CM_USE_ITEM`, `CM_START_LOOT`, `CM_LOOT_ITEM` | 126, –, – | P5-16/P5-15 | entry criterion | M5b-3 (item use, loot) — **met** (refresh) |
| `CM_GATHER` | – | P5-15 | out of scope | gathering has no milestone (D13; m5c-plan.md D10 asks the user the same question) |
| `CM_HOUSE_DECORATE`, `CM_HOUSE_EDIT`, `CM_USE_HOUSE_OBJECT`, `CM_WINDSTREAM` | 60, 159, 42, 93 | P5-15/16 | out of scope | housing (M5h), windstreams (M5f) |

### 3.9 Server packets: none needed, decoders needed

All quest server packets exist with 0 `AION_UNPORTED` (§1). The gate needs **independent decoders** (m5a-plan.md D9) for `SM_DIALOG_WINDOW`
(D target, H page, D quest, H 0, H page-dependent — SM_DIALOG_WINDOW.java), `SM_QUEST_ACTION` (C type, D quest, then per type: ADD `C status,
C 0, D step|flags<<24, H 0, C 0`; UPDATE `C, C, D, H`; ABANDON `D 0`; TIMER `D, C`; SHARE `D, D`; UNK `H 1, H 0` — SM_QUEST_ACTION.java; an
`extra_category` quest writes **nothing**) and `SM_NEARBY_QUESTS` (C 0, H −size, D id | bit 17). `SM_QUEST_LIST` and `SM_QUEST_COMPLETED_LIST`
decoders exist (`decoders/PacketDecoders.h:494,497`; refresh: `:500, :503`); `SM_STATUPDATE_EXP` is decoded inside `M5bScenarioTest.cpp:204-219`
(refresh: `:227-240`) and should move to `decoders/`. **Refresh:** M5b-3 added `decoders/ItemDecoders.{h,cpp}`
(`SM_INVENTORY_UPDATE_ITEM`, `SM_INVENTORY_ADD_ITEM` and more) and a per-client `InventoryModel` (`M5b3ScenarioTest.cpp:483`). M5c's G-02
plans to lift the model into a shared header. M5c's G-02 also plans an `SM_DIALOG_WINDOW` decoder (its `EconomyDecoders`) and builders for
the dialog packets; none of these exists in the working tree yet. Whichever lands first, M5d uses it (G-02).

---

## 4. The chunk, measured

### 4.1 `AION_UNPORTED` and `AION_PARTIAL` in P5-06

`chunks.py files P5-06` (globs `questEngine/**`, `services/QuestService.*`, chunks.cmake:300-304), `grep -o 'AION_UNPORTED('` per file:

| Sites | File | Bodies |
|---|---|---|
| **88** | `questEngine/handlers/AbstractQuestHandler.cpp` | all but the constructor and the two loaders |
| **26** (refresh: **22**) | `services/QuestService.cpp` | `finishQuest`, `validateAndFixRewardGroup`, `getRewardItems`, `getRewardIndex`, `giveReward`, `calculateRepeatDate`, `findNextRepeatDay`, `startQuest` ×2, `addOrUpdateQuest`, `startEventQuest`, `checkQuestListSize`, `collectItemCheck`, `checkAndGetCollectItemQuestRewardCategory` ×2, ~~`getQuestDrop(Set,…)`, `allowLooting`, `regQuestDropItem`, `isQuestDrop`~~ (refresh: ported by M5b-3's L-04, `QuestService.cpp:368-519`), `questTimerStart`, `invisibleTimerStart`, `questTimerEnd`, `abandonQuest`, `getEachDropMembersGroup/Alliance`, `removeQuestWorkItems` |
| **13** | `questEngine/model/QuestState.cpp` | `setQuestVarById`, `getQuestVarById`, `setQuestVar`, `setStatus` ×2, `setCompleteCount`, `setRewardGroup`, `isStartable`, `canRepeat`, `setPersistentState`, `setFlags`, `getStepGroup`, `setStepGroup` |
| 3 | `questEngine/model/QuestVars.cpp` | `getVarById`, `setVarById`, `getQuestVars` |
| 1 | `questEngine/model/QuestEnv.cpp` | `getTargetId` |
| 1 (+2 partial) | `questEngine/QuestEngine.cpp` | `reload`; partials `:111` (XML registration) and `:115` (spawn analyzer) |
| 16 | `questEngine/handlers/models/*Data.cpp` | the `register_` of every XML kind (one line each: construct the template, `addQuestHandler`) |
| 15 | `questEngine/handlers/models/xmlQuest/**` | 8 operations `doOperate`, 5 conditions `doCheck`, `OnTalkEvent`/`OnKillEvent::operate` |
| **163** (+2) | **total** — matches the roadmap | |
| **159** (+2) | **refresh, HEAD `5fbb03a08`**: the same files, `QuestService.cpp` 22; `census.py --chunks P5-06`: 159 `AION_UNPORTED`, 2 `AION_PARTIAL` sites (1 partial body). P5-06 has no uncommitted file | |

Java lines in the chunk (`JAVA` globs minus `QuestHandlerLoader.java`, 79 files): **8,066** by `wc -l` (census: 8,067).

### 4.2 The bodies no site count reaches (lesson 1)

Compared method by method: every Java method of the 79 chunk classes against the names declared in the C++ shell (`src/…/X.h`) and its
generated header (`generated/…/X.xml.h`, `.xml.inc`, `.h`). False positives removed (`register` is `register_`; enum `value`/`fromValue` are
xmlgen companions; `QuestService$1.run` is an inner class).

| Class(es) | C++ today | Bodies | Java lines | On the M5d path? |
|---|---|---|---|---|
| **16 template handlers + `AbstractTemplateQuestHandler`** (`handlers/template/*`) | **no file** (only `fwd.h`) | **75** (incl. 17 constructors) | 2,067 | **yes — they are the XML quests** |
| `QuestDialog.operate`, `QuestNpc.operate`, `QuestVar.operate`, `QuestConditions.checkConditionOfSet`, `QuestOperations.operate` | shell headers declare only the generated getters | 5 | – | yes, for `xml_quest` 1127 (the chain `QuestEvent.operate` → `QuestVar` → `QuestNpc` → `QuestDialog` → conditions/operations) |
| `HandlerResult.fromBoolean` | generated enum, no companion | 1 | 18 | yes (`ReportToMany.onItemUseEvent`) |
| `task/` — `QuestTasks` 4, `FollowingNpcCheckTask` 5, 4 destination checkers × 2 | no file | 17 | 227 | no — only `AbstractQuestHandler.defaultStartFollowEvent`, which only Java handlers call (14 files) |
| `QuestSpawnAnalyzer` | no file | 6 | 125 | no — behind `gameserver.analysis.quest_handlers` (the `:115` partial) |
| `KillOperation` | no file | 1 | 23 | never — not in `QuestOperation`'s `@XmlSeeAlso`, so JAXB never builds one (header-requests.md shells-4) |
| **Total** | | **105** (81 on the path) | | |

**The chunk is 163 + 105 = 268 bodies (+ 2 partial closures).** The template handlers are the largest missing-file block since M5b-2's
`skillengine/properties/`, and like it they have no site to count. **Refresh: 159 + 105 = 264.** `census.py` reports 109 undeclared
bodies: these 105, plus the `value`/`fromValue` pairs of `ConditionOperation` and `ConditionUnionType`. This table left those four out
as xmlgen companions. The generated enums carry only `EnumTraits` names (`generated/…/questEngine/model/ConditionOperation.h`), and those
names serve JAXB's `value()`/`fromValue`. Census does not credit them. Census counts the chunk as **269 open** (159 + 1 partial body +
109).

### 4.3 Outside the chunk, on the user's path

| Piece | Chunk | Bodies | Java lines | Status |
|---|---|---|---|---|
| `DialogService` (7 bodies) | P5-08 | 7 | 378 | all `AION_UNPORTED` (refresh: at HEAD; in the working tree M5c's D-02 has 6 ported and the `MATCH_MAKER` autogroup-on arm as an in-arm `AION_UNPORTED`, `DialogService.cpp:314`) |
| `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_DELETE_QUEST` | P5-16, P5-15 | 8 (read + run) | 243 | no file (refresh: the first three are in the working tree, M5c's D-03) |
| `CM_QUEST_SHARE`, `CM_QUESTION_RESPONSE`, `CM_PLAY_MOVIE_END`, `CM_OBJECT_SEARCH` | P5-16 | 8 | 233 | no file (W/O) (refresh: `CM_QUESTION_RESPONSE` is in the working tree, M5c's D-04) |
| `AbyssGuardSimpleAI` (`simple_abyssguard`) | P5-05 (root handler) | ~6 | 75 | no file (census: 6 undeclared) |
| `ActionItemNpcAI` (`useitem`) | P5-05 (root handler) | ~7 (census: 6) | 99 | no file |
| `QuestItemNpcAI` (`quest_use_item`) | **A1 (phase 6)** `handlers/ai/quests/` (chunks.cmake:527-531; refresh: `:543-547` in the working tree, after M5c's P5-09 split) | 4 | 77 | no file |
| **Total** | | **~40** | **~1,105** | |
| The reward services of E-09: `BonusService` 3 (`getQuestBonus`, `getMatchingItemsOfRandomGroup`, `getBonusGroups`), `AbyssPointsService` 3 (`addAp` ×2, `onRankChanged`; not the siege variant `addAp(Player, VisibleObject, int)`), `GloryPointsService::addGp`, `CubeExpandService` 3 (`questExpand`, `expand`, `canExpand`) | P5-09, P5-08, P5-08, P5-07 | ~10 | ~150 | all `AION_UNPORTED` (§3.5 row 16); refresh: unchanged at HEAD and in the working tree. `BonusService` is P5-09a after M5c's split, and `CubeExpandService` is M5c's required P-05 |
| **Refresh — E-10:** `QuestStartAction`, `ReadAction` (`canAct`/`act` stubs from `m5b3-h01`, `finishUse`, the `ItemUseObserver` of each) | P5-07 | 8 | 155 | 4 `AION_UNPORTED` + 2 undeclared + 2 anonymous (census); m5c-plan.md §3a and m5b3-plan.md O-03 send them to M5d |

**Milestone total: ~318 bodies over ~9,320 Java lines** (8,066 + ~1,105 + ~150), of which ~24 (task, analyzer, `KillOperation`) and ~8 (the W
packets) may stay. If M5c lands its stage 0 (D4), ~15 of the outside bodies (`DialogService` 7, `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`,
`CM_CLOSE_DIALOG`, `CM_QUESTION_RESPONSE`) arrive done. **Refresh:** −4 (quest drops, M5b-3) +8 (E-10) gives **~322 bodies over ~9,340
Java lines** at HEAD. It is **~307** once M5c's stage 0 commits: the 15 bodies above are in the working tree now. It is **~304** once
M5c's P-05 also takes `CubeExpandService`'s three.

### 4.4 The P4-08 lease on the quest shells

`chunks.cmake:153-164` keeps two phase-4 leases alive: `aion_gs_chunk(P4-08 LEASE PHASE 4 XMLGEN_SHELLS GLOBS "aion/gameserver/questEngine/**")`
and `aion_gs_chunk(P4-08 LEASE PHASE 4 TEST_SUPPORT skills quest)`. Measured with `chunks.py owner`:

- **What it covers:** exactly **74 of P5-06's files** — every file under `questEngine/handlers/models/**` (the `*Data` shells, `Monster`,
  `XMLQuest.h`, the xmlQuest conditions, operations and events), because `XMLGEN_SHELLS` restricts the glob to files whose
  `generated/…/X.xml.inc` exists. And the test directory `tests/quest` ("P5-06 (aion_gs_quest); leased to P4-08").
- **What it does not cover:** `QuestEngine.*`, `AbstractQuestHandler.*`, `model/**`, `services/QuestService.*`, and the new
  `handlers/template/**` files (no `.xml.inc`; `chunks.py owner …/template/ReportTo.cpp` answers "P5-06" alone).
- **What it means for lanes:** `check-ownership` accepts a change by the owner *or* the lessee (`chunks.py:661-685`), so a P5-06 lane is not
  blocked mechanically — but two parties own the same 74 files, P4-08 is a finished phase-4 chunk, and the comment at chunks.cmake:157-158
  ("the questEngine half stays until phase 6 takes the quest shells") is **wrong about who needs them**: the shells' behaviour (`register_`,
  `doCheck`, `doOperate`, `operate`) is **P5-06's M5d work**; phase 6's quest handlers live in `handlers/quest/**`, a different tree. And if
  D1 splits P5-06, the lease glob would span all three parts. **D2 releases it** the way `a3f301e7b` released the skill half.

---

## 5. What registering the XML quests wakes (lesson 2)

Closing `QuestEngine.cpp:111` turns 4,184 registrations on. Traced from every entry point the running server already has, to the first
unported or partial body (`qs` = the player's `QuestState` for that quest):

| Entry point | Reached in | Path once registered | First unported body |
|---|---|---|---|
| **Startup** | every run | `XMLQuest::register_` ×4,184 → template constructors → `AbstractQuestHandler` ctor (ported) → `register()` → `QuestEngine::register*` (ported) | the 16 `register_` bodies; the 17 template classes (no file) |
| **Spawn** (83,885 spawns) | every run | `WorldMapInstance::addObject` → `questIds` → `updateNearbyQuestsTask` | none (ported) |
| **Enter world**, `CM_LEVEL_READY` | **every gate** | `QuestEngine::onEnterWorld` → the **10 `ReportOnLevelUp`** handlers (registered for *every* enter world, ReportOnLevelUp.java:28-35, 52-66) → `QuestStateList.hasQuest` → **`QuestService::startQuest`** | **`QuestService::startQuest`** — **one** ERROR line per enter world ("QE: exception in onEnterWorld"): the `try` encloses the whole loop (QuestEngine.java:340-350, `QuestEngine.cpp:341-350`), so the first unported `startQuest` ends it |
| Enter world, invasion worlds | a player in Theobomos/Brusthonin etc. | 16 `KillInWorld`/`MonsterHunt` with `invasion_world` → `VortexService::getLocationByWorld` (ported) → `qs.isStartable()` | `QuestState::isStartable` |
| Enter world, `updateNearbyQuests` | every gate | `checkStartConditions` for the map's start quests | none for a fresh character; `QuestState::canRepeat` once a quest is COMPLETE |
| **Level change** | **every gate**: a fresh character's first enter world calls `onLevelChange(old_level 0, 1)` (PlayerEnterWorldService.java:204, `PlayerEnterWorldService.cpp:413`), which is where the first-enter `SM_NEARBY_QUESTS` of the M5a pattern comes from (`M5aScenarioTest.cpp:1043-1044`, PlayerController.java:590-591); then every level-up (level 2 needs 400 exp) | `QuestEngine::onLevelChanged` → 5 `ReportOnLevelUp` of the race (the dispatch filters by race, QuestEngine.java:230-244) → `startQuest` | `QuestService::startQuest` — one ERROR per level change ("QE: exception in onLevelChanged", same whole-loop `try`) |
| **Kill** | M5b, M5b2 gates (210663, **210133** — 1102's own target); refresh: and M5b3's | `onKill` → `MonsterHunt(1102)::onKillEvent` → `getQuestState` → null → false | none for a player without the quest; `getQuestVarById` with it |
| **Refresh — kill of an npc with a quest drop** | every gate that kills one: 20 quest-drop items on Poeta's npcs, 25 on Ishalgen's (m5b3-plan.md §2.4), e.g. 210668 for 1108 | `registerDrop` → `QuestService::getQuestDrop` (ported by M5b-3) → `isQuestDrop`: false at `qs == null`. After the join a character can hold a START `item_collecting` quest, and for a drop with a `collecting_step` the next line reads `qs.getQuestVarById(0)` (`QuestService.cpp:472-478`) | `QuestState::getQuestVarById` / `QuestVars::getVarById` (E-01, in the join) — pinned today by `QuestDropTest.ACollectingStepReachesTheUnportedQuestVariablesOnlyForAStartedQuest` (`tests/quest/QuestDropTest.cpp:347`), which E-01 must rewrite |
| Aggro, distance | every fight | `onAddAggroList`, `onAtDistance` | none: no XML quest uses `aggro_start_npc_ids` or `start_dist_npc_id` (0 occurrences in `quest_script_data`) |
| Zone | geo gates | 6 `start_zone` quests, all `IDRAKSHA_…_300610000` (an instance) | not on any gate path |
| Skill use | M5b2 gate | `onUseSkill` → 30 `SkillUse` | `getQuestState` → null for these skills' quests |
| Talk | nothing today (no `CM_SHOW_DIALOG`); **every talk to any npc** once M5c's stage 0 lands it (refresh: written in the working tree, uncommitted) | §3.2: `TalkEventHandler.onTalk` sends `USE_OBJECT` to **every** quest in the npc's `onTalkEvent` list (TalkEventHandler.java:26; QuestEngine.java:167-175) — so every template's `onDialogEvent` without a quest state, and `XmlQuest`'s `OnTalkEvent::operate` (XmlQuest.java:73-76) | `DialogService::isInteractionAllowed`, `QuestEnv::getTargetId`, the 16 templates' `onDialogEvent`, `AbstractQuestHandler::onDialogEvent`, the xmlQuest language |
| Item use | once M5b-3 lands `CM_USE_ITEM` (**refresh: live at HEAD**, `CM_USE_ITEM.cpp:109-112`; M5b-3's gate uses potions and godstones, none of them among the 39 registered quest items) | `QuestEngine.onItemUseEvent` → `ItemOrders` / `ReportToMany` start items (no quest state yet). **Review correction:** directly from `CM_USE_ITEM` only for the 6 registered items without `<queststart>` (`item_order` 1323, 16904, 26904, 30007, 30107; `report_to_many` 4914). The other 33 (27 `item_order`, 6 `report_to_many`) carry `<queststart>` for the same quest, which `CM_USE_ITEM.cpp:108-112` skips (CM_USE_ITEM.java:89-90); they arrive through the next row | their `onItemUseEvent` |
| **Refresh — a `<queststart>` or `<read>` item** | today, on a real client: four start-map items drop (§2.5 note) | `CM_USE_ITEM` → `QuestStartAction` / `ReadAction` `canAct` | the `m5b3-h01` stubs throw now (E-10). Ported, `QuestStartAction.finishUse` runs `isStartable`, `checkStartConditions`, `onItemUseEvent` and `onDialog(ASK_QUEST_ACCEPT)` (QuestStartAction.java:68-88). For the four start-map items, all of them Java quests, those calls find no handler and do nothing (D9). **E-10 is the start path of 39 XML quests, and the only one for 38 of them** (review correction; 11216 `item_collecting` also has start npc 799017): of the 157 `<queststart>` items, 39 start an XML template (27 `item_order`, 6 `report_to_many`, 5 `report_to`, 1 `item_collecting`; 78 start a Java quest, 40 a quest with no handler; measured). For the 33 whose item is also registered with `registerQuestItem`, `finishUse`'s `onItemUseEvent` reaches `ItemOrders`/`ReportToMany.onItemUseEvent` (page 4); for the other 6 it answers `UNKNOWN` and `onDialog(ASK_QUEST_ACCEPT)` reaches the template's `onDialogEvent` (QuestStartAction.java:84-87, QuestEngine.java:152-164) |
| Quest object | once D5's AIs exist | `QuestItemNpcAI.handleDialogStart` → `QuestEngine.onCanAct` → `ItemCollecting`'s action-item registration → `AbstractQuestHandler.onCanAct` (QuestItemNpcAI.java:34-39, QuestEngine.java:587-600) | `AbstractQuestHandler::onCanAct` |
| Cron 09:00 | a run crossing 09:00 | `messageTask` → `qs.isStartable()` for completed quests (QuestEngine.java:911-935) | `QuestState::isStartable` |
| Login with quest rows | once any quest was saved (refresh: **and M5c's C19 seed**, a `player_quests` row for 1006; §3.6) | `PlayerQuestListDAO::load` (and `PlayerCommonData::updateDaeva`'s own load) | `QuestState::setPersistentState` |
| Logout / periodic save | once any quest exists | `PlayerQuestListDAO::store` | `QuestVars::getQuestVars` |

**Consequence (D3), rewritten in rev 2:** the `register_` bodies may be ported early (tests call them directly), but the `:111` partial closes
only in the commit that also lands `QuestService::startQuest` and `checkStartConditions`' callees, `QuestState`/`QuestVars`,
`AbstractQuestHandler::onDialogEvent` and `onCanAct`, and — **for every one of the 16 template classes** — the **join minimum**: the
constructor, `register()`, and every hook a registration reaches **without the player holding that quest**:

| Hook | Templates that override it (`handlers/template/*.java`) | Reached by |
|---|---|---|
| `onEnterWorldEvent` | `ReportOnLevelUp` (10), `KillInWorld` (10 with `invasion_world`: beluslan.xml:533, brusthonin.xml:227-228, eltnen.xml:477, gelkmaros.xml:494, heiron.xml:525, inggison.xml:468, morheim.xml:456, theobomos.xml:216-217), `MonsterHunt` (6 with `invasion_world`) | every enter world (QuestEngine.cpp:341-350 calls every registered handler) |
| `onLevelChangedEvent` | `ReportOnLevelUp`, `ReportToMany` (only with `mission="true"`: 0 in the data) | every first enter world and every level-up |
| `onKillEvent` | `MonsterHunt`, `KillSpawned`, `XmlQuest` (→ `OnKillEvent::operate` up to its `qs == null` return), `MentorMonsterHunt` (no data) | every kill of a registered monster — the M5b and M5b-2 gates' kerub kills run `MonsterHunt(1102)` |
| `onDialogEvent` | **all 16** | every talk (the Talk row above) |
| `onUseSkillEvent` | `SkillUse` | the M5b-2 gate's casts, if a listed skill |
| `onEnterZoneEvent` | `MonsterHunt`, `ItemCollecting` (6 `start_zone` quests, all in instance 300610000) | entering that zone (M5f) |
| `onItemUseEvent` | `ItemOrders`, `ReportToMany` | `CM_USE_ITEM` (M5b-3; refresh: ported, so this hook is reachable from the join on) for 6 start items, and `QuestStartAction.finishUse` (E-10) for the other 33 (review correction, §17.3 N1) |

Only the PvP hooks (`onKillInWorldEvent`, `onKillInZoneEvent` — `PvpService` is not on any M5d path) and `onAtDistanceEvent` /
`onAddAggroListEvent` (0 registrations in the data, §5 Aggro row) may land after the join; before they land they stay `AION_UNPORTED`, and no
gate reaches them. `XmlQuest.onDialogEvent` runs the whole xmlQuest language for every talk to 1127's npcs, so if T-03 is deferred (D10),
`XmlQuestData::register_` becomes an `AION_PARTIAL` that registers nothing (1127 is then not offered), listed in §B of the allow-lists.

The same commit deletes the `:111` rows from `m5a_partial_allowlist.txt:18`, `m5b_partial_allowlist.txt:32`, the M5b-2 allow-list
(m5b2-plan.md §10.1 puts it in §A) and — if they exist by then — the M5b-3 and M5c allow-lists (m5c-plan.md:559 puts `:111` in its §A),
because Q1 asserts §A rows are hit at least once — a row whose site no longer exists turns those gates red. It also **re-pins the `:115` rows**
(`m5a_partial_allowlist.txt:19`, `m5b_partial_allowlist.txt:71` and their M5b-2, M5b-3 and M5c counterparts): the rows are pinned by line
number, and rewriting `QuestEngine.cpp:110-111` moves the analyzer partial off line 115.

**Refresh, the rows at HEAD:** `:111` is at `m5a_partial_allowlist.txt:18`, `m5b_partial_allowlist.txt:35`, `m5b2_partial_allowlist.txt:25`
and `m5b3_partial_allowlist.txt:24`, each in §A. `:115` is at `m5a :19`, `m5b :63`, `m5b2 :47` and `m5b3 :38`, each in §C. The M5b-3 list
is committed (`5fbb03a08`). M5c's list does not exist yet (its §10.1 plans both rows). The lanes that feed the join also rewrite
three unit cases that pin throws (review: the refresh said two): E-01 two, E-02 with D-02's tests one (risk 11; §17.3 N3, N5).

---

## 6. Decisions

| # | Decision | Why |
|---|---|---|
| **D1** | **Split P5-06 into three parts sharing `aion_gs_quest`**: **P5-06a engine** — `QuestEngine.*`, `QuestSpawnAnalyzer*`, `services/QuestService.*`, `model/**`, `task/**` (44 sites + 23 invisible, 2,678 Java lines; refresh: 40 sites, M5b-3 ported four); **P5-06b handler base** — `handlers/AbstractQuestHandler.*`, `handlers/HandlerResult*`, `handlers/fwd.h` (88 + 1, 1,308 lines); **P5-06c XML templates** — `handlers/template/**`, `handlers/models/**` (31 + 81, 4,080 lines). Test directories `tests/quest` (a), new `tests/quest_handlers` (b), new `tests/quest_templates` (c). The integrator decides before stage 1. | A chunk is the unit of ownership; as one chunk P5-06 is one ~268-body lane, the critical path of the whole milestone. The seams are real: the engine routes and stores, the base provides helpers, the templates are the quests. `register_` constructs a template, so models and templates belong together. Precedent: m5b2-plan.md D1 (P5-02a/b), P4-07a/b; refresh: and M5c's P5-09a/b/c, now in the working tree's `chunks.cmake:329-352`. |
| **D2** | **Release the P4-08 lease on `questEngine/**` and on `tests/quest`** (chunks.cmake:153-164), correcting the comment of `a3f301e7b`. | §4.4: the leased files are P5-06's behaviour. |
| **D3** | **The XML registration is the last commit of stage 1** (the end of stage 1b, §12). Ported `register_` bodies stay behind the `:111` partial until §5's join minimum lands for all 16 templates; the join commit removes the `:111` rows from every allow-list, re-pins the `:115` rows and updates `QuestEngineTest.InitReachesThePartialSitesOfXmlQuestsAndTheSpawnAnalysis`. | §5: an early registration logs one ERROR per enter world and per level change in every gate, and the first enter world is a level change. Precedent: m5b2-plan.md D11. |
| **D4** | **The dialog plumbing (`CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `DialogService`) is ported whole by whichever of M5c and M5d runs first.** Reconciled with the M5c draft (rev 1, untracked, unreviewed): **M5c stage 0 ports `CM_SHOW_DIALOG`, `CM_QUESTION_RESPONSE`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG` and all 7 `DialogService` bodies** (m5c-plan.md:227-228, its D4 at :335), including `CM_DIALOG_SELECT`'s journal branch that reaches `QuestService::finishQuest` (m5c W-12 at :258). M5d's dialog lane therefore plans for the small case: `CM_DELETE_QUEST`, `CM_QUEST_SHARE`/`CM_PLAY_MOVIE_END` (W), `CM_OBJECT_SEARCH` (O), the quest-arm tests of the M5c packets, and E-09's P5-08 bodies. D-01..D-04 stay in the plan as the fallback I-03 activates if M5c did not land them. **Refresh:** M5c's rev 2 was reviewed and refreshed, and its dialog lane (D-01..D-06 there) is written in the working tree, uncommitted and unbuilt: `DialogService` with 6 of 7 bodies ported and `MATCH_MAKER`'s autogroup-on branch as an in-arm `AION_UNPORTED()` (`DialogService.cpp:314`, m5c W-31), the four packets, `PostboxAI`. So the fallback is very likely unneeded, and I-03 checks it at branch time. One M5c test pins `finishQuest`'s throw and must change with E-02 (§17.3 N5). | One port, not two half-ports. The plan must not depend on the draft; I-03 is the final check. |
| **D5** | **M5d pulls three npc AIs forward**: `simple_abyssguard` (`AbyssGuardSimpleAI`, P5-05), `useitem` (`ActionItemNpcAI`, P5-05) and `quest_use_item` (`QuestItemNpcAI`, chunk A1, through a file lease). **Taken by the integrator under the standing instruction** (phase5-roadmap.md:58-63); name it in the next progress update, because it switches **870 `simple_abyssguard` spots and 2,720 `quest_use_item` spots spawned at startup** (997 and 2,959 in all spawn files) from `DummyNpcAI` to live AIs world-wide. | The givers of 476 XML quests (439 reachable + 37 Java-blocked) are `simple_abyssguard`, among them 9 start-map npcs giving 16 quests (§2.5); 135 reachable item-collecting quests and the Poeta chain's 1103 need `quest_use_item` objects (Poeta 72 spots, Ishalgen 165). Without them 16 of the 56 start-map quests cannot start and the Elyos chain stops at 1103. 251 Java lines together. **Refresh:** m5b3-plan.md's O-05 (and its §3 row on m5b-plan.md O-08) still sends `AbyssGuardSimpleAI` to **M5j**, and m5c-plan.md W-15 says "M5d/M5j". This decision takes it into M5d. Those two rows should point here; that is the integrator's edit, not this file's. |
| **D6** | **The gate chain is the Elyos 1101 → 1102 → 1103 (accept, abandon)**, plus the Asmodian **2101 and 2102** for an item reward — **not a chain**: 2102 has no precondition (quest_data.xml:10050-10055), so it is offered from the first enter world and 2101's reward window ends on the quest-selection page; 2102's reward opens 2103 as its follow-up (2103 requires 2102, :10056-10060). The Asmodian plays on a **second account B** (D15). | §2.5: dialogs and kills only; exact rewards in the data; the follow-up dialog appears three times (1102, 1103, 2103) and its absence once (2101); 1103 exercises `ItemCollecting`'s start path and abandonment without needing its objects. 2102 is the smallest reward with an item (10 × 169300002 "Bandage"). Refresh: the new Asmodian Warrior already holds 20 Bandages (`oracle.py m5a-creation`), so the reward adds to that stack (Y13). The oracle confirms all four follow-up windows (§2.5 note). |
| **D7** | **Dialog page ids, status bytes, quest vars and reward deltas are asserted exactly; kills as counts.** | They are template constants (the page ids are literals in the templates: 1011, 1003, 2375, 1352, 5). Rewards pass through rates of 1.0 (RatesConfig.java:36-37, 78-79, membership 0) with no cap for quests. |
| **D8** | **`task/**`, `QuestSpawnAnalyzer` and `QuestEngine::reload` stay out of the gate path**: `task/` is a phase-6 prerequisite ported in stage 3 (only Java handlers reach it); the analyzer stays the `:115` partial (config-gated) until E-08 in stage 3; `reload` stays unported with `//reload` of static data (static-data design D3). **This meets the P5-06 acceptance row only in part**: handlers-and-porting-plan.md:647 also asks for "QuestSpawnAnalyzer output equals the Java-regex expectation on the ported set". Until E-08 lands, that is a **deviation from the acceptance row**, recorded in `docs/deviations/P5-06.md` by the join commit and closed by E-08. | None of them is reachable from a client in M5d. |
| **D9** | **Until phase 6 the C++ registry has no Java quest**, so the nearby-quest markers, the npcs' `onTalkEvent` lists and the follow-up choice lack the 13 Poeta / 17 Ishalgen Java quests. Recorded in `docs/deviations/P5-06.md`; the oracle models the XML-only registry. | A faithful difference in content, not in behaviour. |
| **D10** | **`xml_quest` (only 1127) is R but last**: the 15 xmlQuest sites + 5 undeclared methods + `XmlQuest` template land in stage 1b, or in stage 2 if stage 1b runs long. **If deferred past the join, `XmlQuestData::register_` becomes an `AION_PARTIAL` that registers nothing** (§5: `XmlQuest.onDialogEvent` runs the language on every talk to its npcs), so 1127 is simply not offered until T-03 lands. | 25 bodies for one quest, which also needs the quest-object AI. |
| **D11** | **`QuestService`'s quest-drop family** is called from `DropRegistrationService.registerDrop` on every kill (DropRegistrationService.java:84). Reconciled with the M5b-3 draft (rev 1, untracked, unreviewed): **m5b3-plan.md L-04 ports `getQuestDrop`, `isQuestDrop`, `allowLooting` and `regQuestDropItem` under a P5-06 file lease** (m5b3-plan.md:135, 365, 441), against its header request **m5b3-h03**, which replaces the frozen `const std::unordered_set<Ptr<DropItem>>&` of `QuestService.h:77` by a mutable set (m5b3-plan.md:469) — Java adds to that set (QuestService.java:682-728). It leaves `getEachDropMembersGroup/Alliance` (QuestService.java:903-933), which only `QuestItemNpcAI` calls, for a grouped player. **M5d verifies L-04 and ports the two group variants (E-04)**; if M5b-3 left an `AION_PARTIAL` or did not land h03, M5d closes it, carries h03 itself (§9) and edits M5b-3's allow-list. **Refresh: verified at HEAD, all met.** The four bodies are at `QuestService.cpp:368-519` with no partial, h03 is applied (`QuestService.h:81`), and the tests are `tests/quest/QuestDropTest.cpp`. Two deviations are recorded in `docs/deviations/P5-06.md` (M5b-3 section): an empty `players` vector stands for Java's null, and `asHandlerSideDrop` tests membership instead of `instanceof` (m5b3-loot-h01 deferred). The `QuestDrop` quest id is now set by `QuestsData::afterUnmarshal` (`QuestsData.cpp:21-26`, the `4867fbc44` fix), and `QuestEngine::init` only reads it. E-04 is therefore the two group variants only. | Without the quest-state bodies, `isQuestDrop` answers false at `qs == null` (QuestService.java:754-760), so the port is safe before M5d. |
| **D12** | **The gate is a new `gs.scenario.m5d` (+ `_geo`)** with its own schema pair, output directory and allow-list, under the same `RESOURCE_LOCK`; the earlier gates are kept and re-run. **Review addition, conditional:** another workflow's uncommitted `tests/scenario/ScenarioTests.cmake` replaces the single lock by two gate slots, `AION_GS_GATE_SLOT_1`/`_2` (`ScenarioTests.cmake:13-53`). If that commits, both M5d tests hold **one** slot, the one with the smaller runtime sum, together (a gate and its geo variant share a slot: one schema prefix per slot, `:38-42`), and the runtime comment there gains their two lines (§10.1). | m5b2-plan.md D10's argument. |
| **D13** | **Gathering is out of scope.** `CM_GATHER` has no C++ file and no roadmap milestone names it; 1206, 1207, 2133, 2134 and part of the 316 "items from elsewhere" wait for it. **Recommend adding it to M5c** (crafting materials come from the same system; the M5c draft already lists `CM_GATHER` as an optional stage-2 packet, m5c-plan.md:232). **Refresh: still open, and still the user's.** M5c's refresh made its own D10 the same question, and `CM_GATHER` stays O there until the user answers. The facts are sharper now: `CM_GATHER` (51 Java lines, 2 bodies) is the only missing piece, and nothing behind it is unported without CAPTCHA (m5c W-27). 1206 and 2133 need Collection 1, which every character has; 1207 and 2134 need Collection 15 (§2.5 note). | A user-visible gap on the start maps; the decision is the user's (it changes a milestone's scope). |
| **D14** | **M5d ports the four reward services `finishQuest` reaches outside M5b-3 (E-09)**: `BonusService` (P5-09, file lease), `AbyssPointsService::addAp` ×2 + `onRankChanged` and `GloryPointsService::addGp` (P5-08, the dialog lane's chunk), `CubeExpandService::questExpand`/`expand`/`canExpand` (P5-07, file lease, unless M5c's optional P-05 landed them). `Legion::addContributionPoints` (P5-11, legion members only) stays unported until M5h. **Refresh:** `BonusService` sits in **P5-09a** after M5c's split, so the lease names P5-09a. M5c's rev 2 made P-05 (`CubeExpandService` whole) **required** in its stage 1, so the cube part drops here once M5c lands, which the roadmap order makes the planned case. `AbyssPointsService` stays with M5d, because M5c's D2 sends the AP vendors to a later milestone. `getQuestBonus` needs the reward-list header change of D17. | §3.5: 844 reachable quests carry a bonus, AP, GP or cube reward, all 574 work orders among them; left unported, AP and GP quests pay kinah and exp again on every retry. ~10 bodies, ~150 Java lines. Integrator's decision under the standing instruction: it adds scope inside the milestone the user asked for. |
| **D15** | **The Asmodian gate character is created on a second account B**, not in a second slot of account A. | `gameserver.character.creation.mode` defaults to 0 (GSConfig.java:37-38, config/main/gameserver.properties:35), and in mode 0 `CM_CREATE_CHARACTER` answers `RESPONSE_OTHER_RACE` when the account already holds a character of the other race (CM_CREATE_CHARACTER.java:94-96) — `gs.scenario.m5a` asserts exactly that (`M5aScenarioTest.cpp:1368-1375`) and already uses two accounts (`:1298-1307`). A second account keeps the M5d profile equal to the M5b-2 profile; setting mode 1 in `m5d.properties.example` would also work, but it would change a server default for a reason unrelated to quests, and the real-client session would inherit it. |
| **D16** | **The quest stress run (G-06) is offered to the user, not scheduled by the integrator.** | phase5-roadmap.md:62-66: machine load beyond the resource rules and the design of the capacity tests are the user's. G-06 extends the M5b-2 leak nightly (a precedent), but stress clients that fight and quest add load. |
| **D17** | **Refresh — proposed for the integrator (the M5d owner) to confirm at I-02: the two header shapes that phase 6's generated quests depend on.** (a) **H-06 lands as `questEngine/handlers/HandlerResultInfo.h`** with `fromBoolean(std::optional<bool>)` in namespace `::aion::gameserver::questEngine::handlers`, the spelling the transliterator assumes (phase6-questgen-prototype.md §5.2 item 1; 60 generated files call it). (b) **The reward list becomes a list of values**: `std::vector<QuestItems>&` in `AbstractQuestHandler::onBonusApplyEvent` and `QuestEngine::onBonusApplyEvent` (declared `const std::vector<const QuestItems*>&`, `AbstractQuestHandler.h:144-145`, `QuestEngine.h:162-163`; `QuestEngine.cpp:617` holds the ported body); `std::vector<QuestItems>` from `QuestService::getRewardItems` (`QuestService.h:39-40`); `std::optional<QuestItems>` from `BonusService::getQuestBonus` (`BonusService.h:24`). | (a) The prototype needs a spelling to emit, and the plan named only "a new file". If H-06 lands with any other spelling, the 60 generated files must be regenerated against it. (b) Java builds the list and adds a **new** `QuestItems` to it: the bonus (BonusService.java:36, QuestService.java:197-203), and two event handlers (`event_quests/_80016`, `_80018`, `:81`). A `const` reference cannot be appended to, and a `const QuestItems*` cannot own an item made at run time. `QuestItems` is already a value type (`QuestItems.h:9-12`), and it is not `RefCounted`. **E-02 and E-09 need (b) whether or not any handler is generated**, so §9's "none expected" for these headers was wrong. It is a hub-header change, hence the integrator's. |
| **D18** | **Refresh — open for the integrator: M5c's C19 needs two `QuestState` bodies before M5d exists.** m5c-plan.md D5 seeds a Daeva with a `player_quests` row (1006, COMPLETE). Its load runs through `PlayerQuestListDAO::load` twice: once at login, once in `PlayerCommonData::updateDaeva`. Both calls hit the unported `QuestState::setPersistentState` (§3.6). Once that is ported, the enter world's `SM_QUEST_COMPLETED_LIST` hits `canRepeat` next (`SM_QUEST_COMPLETED_LIST.cpp:29`), and the logout's `store` runs `setPersistentState(UPDATED)` on every state (`PlayerQuestListDAO.cpp:103-105`). **Recommended:** M5c ports `setPersistentState` and `canRepeat` under a P5-06 file lease on `QuestState.cpp` (QuestState.java:121-154, ~35 lines) before its stage 3. E-01 then takes them as done and adds the rest. The alternative is that C19 waits for M5d's E-01. **Review addition:** porting `setPersistentState` turns a committed P4-12 case red, whoever ports it: `PlayerModelBodiesTest.QuestStateListKeepsDeletedQuestIds` (`tests/player/PlayerModelBodiesTest.cpp:441`, `a99ec5fcb`) expects `QuestStateList::deleteQuest(1006)` to throw, because `deleteQuest` calls `setPersistentState(DELETED)` (`QuestStateList.cpp:45`). So the lease also covers that test file (chunk P4-12, `chunks.py owner`), and the port rewrites `:441` to assert the transition: a NEW state becomes NOACTION, any other DELETED (QuestState.java:140-154). | Without one of the two, C19 logs two ERROR lines at login and loads the character as a level-9 non-Daeva, so X21a and Hestia's learn (X17) fail. m5c-plan.md does not mention it (checked: no `setPersistentState`, `canRepeat` or `PlayerQuestListDAO` in it). The roadmap runs M5c before M5d, so "wait" means M5c's stage 3 cannot close. |
| **D19** | **Refresh — M5d takes `QuestStartAction` and `ReadAction` (E-10)**, under a P5-07 file lease on their four files. | m5c-plan.md §3a and m5b3-plan.md O-03 (as edited by m5b3 §20) name M5d as their home. `CM_USE_ITEM` reaches their throwing stubs today, from four start-map drops. Their `finishUse` routes into the quest engine (QuestStartAction.java:68-88), so they belong with it. **Review correction: more than "stop the stubs throwing".** `QuestStartAction.finishUse` is the **start path of 39 XML quests**: 27 of the 32 `item_order` quests, 6 of the 7 `report_to_many` start items, 5 `report_to` and 1 `item_collecting` (§5, §17.3 N1). Without E-10, 38 of them cannot be started after the join; the 39th, 11216, also has a start npc (799017). |

---

## 7. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional,
**P** phase-6 prerequisite (in milestone if time allows).

### Integrator

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | D1's manifest split, D2's lease release, the new test directories `tests/quest_handlers`, `tests/quest_templates`. | – | R | S |
| I-02 | The header-request batch of §9, decided before stage 1. | – | R | S |
| I-03 | **Entry criteria check** (§8.1): confirm what M5b-3 and M5c delivered (the drafts say: M5c stage 0 the dialog plumbing, M5b-3 L-04 the quest drops + h03); re-cut the dialog lane (D4), E-04 (D11) and E-09's cube part (D14) accordingly. **Refresh:** M5b-3's half is verified at HEAD (§17.2; E-04 shrinks now). What is left for branch time is M5c: its stage 0 (in the working tree today), its P-05, its G-02 builders and `SM_DIALOG_WINDOW` decoder, whether its `InventoryModel` lift landed, and D18's answer. | – | R | S |
| I-04 | File leases: A1 `handlers/ai/quests/QuestItemNpcAI.*` to the quest-npc-ais lane (D5); `services/reward/BonusService.*` (P5-09) and, unless M5c's P-05 ported it, `services/CubeExpandService.*` (P5-07) to the dialog-and-rewards lane (E-09); P5-07 if H-03 needs one. **Refresh:** `BonusService.*` is **P5-09a** now. Add P5-07's `model/templates/item/actions/{QuestStartAction,ReadAction}.*` to the dialog-and-rewards lane (E-10, D19). If E-01 takes over M5c's D18 bodies, no lease is needed for them. **Review addition:** a P4-12 test-file lease on `tests/player/PlayerModelBodiesTest.cpp` to the quest-engine lane, because E-01's `setPersistentState` turns its `:441` case red (E-01; the same lease goes to M5c's lane if D18 is answered yes). | – | R | S |
| I-05 | **The D3 join** (end of stage 1b): close `QuestEngine.cpp:111`; delete its allow-list rows from the M5a, M5b, M5b-2 and (if present) M5b-3 and M5c lists; **re-pin the `:115` rows** in the same lists; record the acceptance-row deviation of D8 in `docs/deviations/P5-06.md`; run every earlier gate on the join commit. **Refresh:** the rows at HEAD are listed after §5's join table. The M5b-3 list exists, and M5c's will. The earlier gates are now eight scenario gates plus the two smokes, then M5c's one (no geo variant, m5c D12), about 34-36 min in all (m5b3-plan.md §19.3: 1,746 s for the eight, `m5b3` 146 s, `m5b3_geo` 314 s; m5c-plan.md risk 11). **Review addition:** that is the serial figure, for the single lock. If the two-slot scheme of D12 has committed, I-05 runs `ctest -j 2` and budgets the larger slot's sum instead: today 1,062 s and 1,051 s (`ScenarioTests.cmake:44-47`, which include the smokes and the M4 check), about 1,170-1,250 s in the larger once M5c's gate joins slot 2, roughly 23-25 min with the 16-21 % that running beside another run cost on 2026-09-24 (1,227 s and 1,286 s measured against a 1,062 s slot, `:16-18`). Risk 13 has both figures. | E-01, E-02, H-01, H-02, **all of T-01's join minimum (§5)**, T-02, T-04's smoke | R | M |

### Stage 1, the engine (P5-06a)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **E-01** | `QuestState` (13), `QuestVars` (3), `QuestEnv::getTargetId`. The 6-bit var packing (`setVarById`/`getVarById`), `isStartable`/`canRepeat` over `max_repeat_count`, repeat cycles and `nextRepeatTime`, the status setters with `completeTime`, `setPersistentState` with Java's NEW → UPDATE_REQUIRED rules. `// java-race` where Java races (no synchronization in QuestState.java). **Merges first — every other lane calls it.** **Refresh:** E-01 turns **two** existing cases red (review correction; the refresh named one). (1) `QuestDropTest.ACollectingStepReachesTheUnportedQuestVariablesOnlyForAStartedQuest` (`tests/quest/QuestDropTest.cpp:347`, the throw at `:355`) expects `getQuestVarById`'s `UnportedException`, so E-01 rewrites it to assert the collecting-step comparison (QuestService.java:760-763). It also closes the "a drop with a collecting step … throws" row of `docs/deviations/P5-06.md`. (2) `PlayerModelBodiesTest.QuestStateListKeepsDeletedQuestIds` (`tests/player/PlayerModelBodiesTest.cpp:441`, chunk **P4-12**, not P5-06) expects `QuestStateList::deleteQuest(1006)` to throw, because `deleteQuest` calls `setPersistentState(DELETED)` (`QuestStateList.cpp:45`); E-01 rewrites it, under I-04's P4-12 test-file lease, to assert that the returned state's persistent state is NOACTION (it was NEW) and that a state set to UPDATED before the delete becomes DELETED (QuestState.java:140-154). E-01 also un-skips `PlayerDaoTest.QuestStateListLoadAndStore` (`tests/dao/PlayerDaoTest.cpp:619-649`, P4-14, `SKIP_IF_UNPORTED` on `setPersistentState` and `QuestVars::getQuestVars`), which must then pass against MariaDB. Before merging, grep the test tree for `UnportedException` next to `QuestState`, `QuestVars` and `QuestService` (risk 11). `setPersistentState` and `canRepeat` may already have been landed by M5c under D18, which then carries case (2). | QuestState.java, QuestVars.java, QuestEnv.java | – | R | M |
| **E-02** | `QuestService` start/finish/reward/abandon: `startQuest` ×2, `addOrUpdateQuest`, `startEventQuest`, `checkQuestListSize`, `finishQuest`, `validateAndFixRewardGroup`, `getRewardItems`, `getRewardIndex`, `giveReward`, `calculateRepeatDate`, `findNextRepeatDay`, `removeQuestWorkItems`, `abandonQuest`. **Refresh:** `getRewardItems` needs D17(b)'s value list (it collects a new bonus `QuestItems`), and so does the phase-6 transliterator's B05 row (`startEventQuest`, 3 generated files). E-02 turns M5c's `DialogSelectRunTest.ReportingAQuestWithoutAnNpcFinishesItThroughTheUnportedQuestService` (`tests/cm_ak/DialogSelectPacketsTest.cpp:450-457`, working tree) red, because `finishQuest` stops throwing. D-02's quest-arm tests replace that case in the same integration part. | QuestService.java:77-300, 400-470, 849-885, 935-948 | E-01 | R | L |
| E-03 | `collectItemCheck`, `checkAndGetCollectItemQuestRewardCategory` ×2. | QuestService.java:557-664 | E-01 | R | S |
| E-04 | The quest-drop family (D11): verify M5b-3's L-04 (`getQuestDrop` against m5b3-h03, `isQuestDrop`, `allowLooting`, `regQuestDropItem`) or port it and carry h03; port `getEachDropMembersGroup/Alliance` (reached only by a grouped player at a quest object — M5g; a unit case on a fabricated group, else W). **Refresh:** L-04 is verified (D11), so E-04 is **the two group variants only**, 2 bodies. | QuestService.java:666-796, 903-933 | E-01 | R | S |
| E-05 | Timers: `questTimerStart`, `invisibleTimerStart`, `questTimerEnd` (Java handlers only, cheap). **Refresh:** still W for M5d's gate, but a **P** for phase 6. The transliterator's row B04 (16 generated files) calls these three. | QuestService.java:811-847 | E-01 | W (P) | S |
| E-06 | Tests (`tests/quest`): var packing golden vectors derived from the Java arithmetic; `isStartable`/`canRepeat` table incl. daily/weekly repeat dates on a `ManualClock`; `finishQuest` reward arithmetic (reward groups, `SELECTED_QUEST_REWARDn` → index n−1, `SELECTED_QUEST_NOREWARD` with `extendedRewardIndex − 8`, class rewards, extended rewards on the Xth repeat, and the **order** kinah → exp → title → AP → DP → GP → cube of QuestService.java:220-244); `finishQuest` refused outside REWARD (`:84-85`); `startQuest` ADD vs UPDATE; `abandonQuest` delete vs reset-to-COMPLETE; `PlayerQuestListDAO` round trip. Mutation-proven: name the test that fails for a wrong var shift, `XP_HUNTING` in place of `XP_QUEST`, and a dropped reward-group check. | – | E-01..E-05, E-09 | R | L |

### Stage 1, the handler base (P5-06b)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **H-01** | Dialog helpers: `onDialogEvent`, `sendQuestDialog` (with the reward-window exploit guard), `sendDialogPacket`, `sendQuestSelectionDialog`, `closeDialogWindow`, `sendQuestStartDialog` ×3, `sendQuestEndDialog` ×2 **with the follow-up continuation**, `isAcceptableQuest`, `defaultCloseDialog` ×6, `sendQuestRewardDialog`, `sendQuestNoneDialog` ×6, `sendItemCollectingStartDialog`. | AbstractQuestHandler.java:93-117, 290-520 | E-01 | R | L |
| H-02 | Step helpers: `updateQuestStatus`, `changeQuestStep` ×3, `hasAnyPreQuestFinished`, `onCanAct`, `getActionItems`. | | E-01 | R | S |
| H-03 | Item helpers: `checkQuestItems` ×2, `checkQuestItemsSimple`, `checkItemExistence` ×2, `giveQuestItem` ×3, `removeQuestItem` ×2, `useQuestObject` ×7, `useQuestItem` ×5 (use bar, `ItemUseObserver`, `SM_USE_OBJECT`). | | E-01; M5b-3 `ItemService` | R | M |
| H-04 | Event defaults: `defaultOnKillEvent` ×8, `defaultOnKillRankedEvent` ×2, `defaultOnKillInZoneEvent` ×2, `defaultOnUseSkillEvent`, `defaultOnGetItemEvent`, `defaultOnLevelChangedEvent`, `defaultOnQuestCompletedEvent`, `defaultOnEnterZoneEvent`, `sendEmotion`, `playQuestMovie` ×2; `defaultStartFollowEvent` ×2 / `defaultFollowEndEvent` ×2 wait for E-07 (P). | | E-01 | R | M |
| H-05 | Spawn helpers ×9 (`spawn`, `spawnInFrontOf`, `spawnForFiveMinutes*`, `spawnInFront`, `spawnTemporarily`) over the ported `SpawnEngine`. | | – | R | S |
| H-06 | `HandlerResult` companion `fromBoolean` (new header, §9). **Refresh:** with D17(a)'s spelling, `questEngine/handlers/HandlerResultInfo.h`, namespace `::aion::gameserver::questEngine::handlers`, `fromBoolean(std::optional<bool>)`. 60 generated phase-6 files assume it. | HandlerResult.java | – | R | S |
| H-07 | Tests (`tests/quest_handlers`): page ids per action for a fabricated handler over real quest templates; the exploit guards — **`sendQuestEndDialog` on a START quest with `SELECT_QUEST_REWARD` (1009) returns false and sends nothing** (AbstractQuestHandler.java:416-417; without the guard it sends `SM_DIALOG_WINDOW(npc, DialogPage.NULL, quest)` and returns true, DialogPage.java:85-111 — the gate cannot reach this guard, because `ReportTo` and `MonsterHunt` call `sendQuestEndDialog` only in REWARD, §10.4), and `sendQuestDialog`'s reward-page guard (:331-339); the follow-up picks the **first** startable quest in `QuestNpc.getOnQuestStart()` iteration order (a `HashSet`, hash order of the ids) whose `finished` precondition is the quest just completed, and falls back to page 10 / close; `changeQuestStep` rollback message. Mutation-proven. | | H-01..H-06 | R | M |

### Stage 1, the XML templates (P5-06c)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **T-01a** | **Stage 1a — the five kinds on the gate path, new files, whole:** `AbstractTemplateQuestHandler`, `ReportTo`, `MonsterHunt`, `ItemCollecting`, `ReportToMany`, `ReportOnLevelUp` (840 Java lines). | `handlers/template/*.java` | E-01, H-01 (link), E-02 (link) | R | L |
| **T-01b** | **Stage 1b — the other eleven classes**: `KillInWorld`, `KillInZone`, `KillSpawned`, `MentorMonsterHunt`, `SkillUse`, `ItemOrders`, `WorkOrders`, `CraftingRewards`, `RelicRewards`, `FountainRewards`, `XmlQuest` (D10) — at least the **join minimum of §5** for each (constructor, `register()`, every state-free hook incl. the whole `onDialogEvent`) before I-05; the PvP hooks may follow. | `handlers/template/*.java` (1,227 lines) | T-01a | R | L |
| T-02 | The 16 `*Data::register_` bodies — ported, held behind `:111` (D3); `XmlQuestData::register_` as D10's `AION_PARTIAL` if T-03 slips. | `handlers/models/*Data.java` | T-01a | R | S |
| T-03 | The xmlQuest language: 8 operations, 5 conditions, 2 events, and the 5 undeclared methods (§9). | `handlers/models/xmlQuest/**` | T-01b | R (D10) | M |
| T-04 | Tests (`tests/quest_templates`, **the xml-templates lane's own**: 1a for the five kinds, 1b for the rest and the smoke): **per template one real quest through its path** (the P5-06 acceptance row of handlers-and-porting-plan.md:647) on an in-process player — accept, progress (kill, item, talk, level), report, reward. The templates whose every quest carries an E-09 reward are tested **with** it (E-09 lands in 1a): `WorkOrders` (all 574 have a `<bonus>`: assert one item of the bonus group matching race — `Chance.selectElement` is random), `RelicRewards` (all 30 pay AP: `SM_ABYSS_RANK` and `STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN`), `KillInZone` (all 12 pay GP; the kill driven in-process through `onKillInZoneEvent`), `FountainRewards` (all 5 bonus); the other twelve use a quest with none of them — G-01's `--census` lists them by kind, for example 1101 `report_to`, 1102 `monster_hunt`, 1103 `item_collecting`, 1115 `report_to_many`, 1941 `crafting_rewards`, 1182 `item_order`, 36504 `kill_spawned`, 13818 `kill_in_world`, 3910 `skill_use`, 13830 `report_on_levelup`, 1127 `xml_quest`; `MentorMonsterHunt` has no quest in the data and runs on a fabricated `mentor_monster_hunt` element. **Registration order**: after registering the real data, the `onTalkEvent` list of an npc with several quests (mires 203057: `[1101, 1102, 1103, 1104]`) and `questOnEnterWorld` (26 ids) equal Java's `HashMap` iteration order of `XMLQuests` as G-01's oracle prints it (`--registration-order`), which kills an `std::unordered_map` iteration (risk 5). Plus the **quest smoke** (1b): register all 4,184, and for every registered (quest, talk npc) pair send `USE_OBJECT` and `QUEST_SELECT` for a fresh player and a player in START state, and for every registered (quest, monster) pair a kill — **0 exceptions, 0 unported hits**. Real-data test (`QuestModelsRealDataTest` is the fixture precedent). **Refresh:** write the in-process fixture (a player with a quest state list, a registered handler, a recorder of `SM_DIALOG_WINDOW` and `SM_QUEST_ACTION`) as a reusable test-support header. Phase 6's golden traces drive every generated handler through exactly such a harness after M5d (phase6-questgen-prototype.md §8.3, phase6-inventory.md §7.6 item 3). **Review addition — item-started quests go through E-10.** 1182's start item 182200549 carries `<queststart questid="1182">` (no casting delay), so `CM_USE_ITEM` skips `onItemUseEvent` for it and the quest starts only in `QuestStartAction.finishUse` (§17.3 N1). The `item_order` case therefore uses the item the way `CM_USE_ITEM` does, through `QuestStartAction::act`, not by calling `onItemUseEvent` directly, and a second `item_order` case, 1323, whose start item has no `<queststart>`, covers the direct route. The same holds for `report_to_many`'s start items: 2274 goes through E-10, 4914 directly. The fixture offers both routes. | – | T-01a/b, T-03, H-*, E-*, E-09, **E-10** (review) | R | L |

### Stage 1, dialog, packets and rewards (P5-08, P5-15, P5-16 + leases) — the M5c draft ports the dialog plumbing (D4)

D-01, D-02's port and D-04 are **fallbacks** that I-03 activates only if M5c's stage 0 did not land; the planned lane is D-02's quest-arm
tests, D-03's `CM_DELETE_QUEST`, D-05 and E-09. **Refresh:** plus E-10. M5c's stage 0 is in the working tree, so the fallbacks are expected
to stay unused.

| Id | What | Java refs | Need | Eff |
|---|---|---|---|---|
| D-01 | *(fallback)* `CM_SHOW_DIALOG` + `AION_CLIENT_PACKET` marker + byte-vector test (`tests/cm_lz`). | CM_SHOW_DIALOG.java | R | S |
| D-02 | *(port: fallback)* `CM_DIALOG_SELECT` incl. the journal branch (`can_report` auto reward, CM_DIALOG_SELECT.java:75-107) and the `DIALOG_INFO` echo (:66-68); the `ENABLE_SIMPLE_2NDCLASS` arm is default-off (CustomConfig.java:68-69) and reaches `ClassChangeService` (M5e) — W. Byte vectors in `tests/cm_ak`. **Always**: the quest-arm tests of M5c's port — journal `SELECTED_QUEST_AUTO_REWARD` (108) on a `can_report` quest in REWARD finishes it, in START does nothing (the gate's Y7b), on a quest without `can_report` falls to `QuestEngine.onDialog`. **Refresh:** these cases replace M5c's `DialogSelectRunTest.ReportingAQuestWithoutAnNpcFinishesItThroughTheUnportedQuestService`, which pins `finishQuest`'s throw. They merge with E-02 (§8.2). | CM_DIALOG_SELECT.java | R | S |
| D-03 | `CM_CLOSE_DIALOG` *(fallback)*, `CM_DELETE_QUEST` + byte vectors. | | R | S |
| D-04 | *(fallback)* `DialogService`: `onCloseDialog`, `onDialogSelect` (the non-quest arms: trade list, pets, charge, housing), `handleQuestDialogueOrSendNextPage`, `sendDialogWindow`, `isInteractionAllowed`, `isSummonOwner`, `isSubDialogRestricted`; decision-table tests in `tests/playersvc`. | DialogService.java | R | M |
| D-05 | `CM_QUEST_SHARE`, `CM_PLAY_MOVIE_END`, `CM_OBJECT_SEARCH` (and `CM_QUESTION_RESPONSE` if M5c did not). Refresh: m5e-plan.md routes the ascension's movies 14 and 151 through `CM_PLAY_MOVIE_END` (its route R). If M5e wants it before its own stage, D-05's `CM_PLAY_MOVIE_END` is the cheap place. | | W/O | S |
| **E-09** | **The reward services (D14)**: `BonusService::getQuestBonus`, `getMatchingItemsOfRandomGroup`, `getBonusGroups` (P5-09 lease); `AbyssPointsService::addAp(Player&, int)`, `addAp(Ptr<Player>, int, gainMessage)`, `onRankChanged` (P5-08; `Legion::addContributionPoints` stays unported — legion members only, M5h); `GloryPointsService::addGp` (P5-08; its offline arm `AbyssRankDAO.addGp` is not on the quest path); `CubeExpandService::questExpand`, `expand`, `canExpand` (P5-07 lease, unless M5c's P-05 ported them). Unit tests in `tests/playersvc` / `tests/economy` / `tests/itemsvc`: AP added and `SM_ABYSS_RANK` sent, a rank change broadcasting `SM_ABYSS_RANK_UPDATE`, GP message and packet, the cube limit +1 and `SM_CUBE_UPDATE`, the bonus group chosen per `BonusType` (random: assert membership, not identity). **Refresh:** the lease is P5-09a. `getQuestBonus` returns `std::optional<QuestItems>` under D17(b). The cube part drops once M5c's required P-05 lands. | BonusService.java:27-81, AbyssPointsService.java:33-63, GloryPointsService.java:18-35, CubeExpandService.java:73-94, 114-123 | R | S |
| **E-10** | **Refresh — the quest-item actions (D19)**: `QuestStartAction::canAct`/`act` (the `m5b3-h01` stubs) with `finishUse` and the use-bar `ItemUseObserver`, and `ReadAction::canAct`/`act` with `finishUse` and its observer (P5-07 file lease). Tests in `tests/itemsvc`: the cast delay and its abort (`STR_ITEM_CANCELED`, the cancel animation); **an item whose quest cannot start, in two cases** (review correction; `finishUse` always starts the cooldown and broadcasts `SM_ITEM_USAGE_ANIMATION` first, QuestStartAction.java:69-70): (a) the quest is active or cannot be repeated → the animation, then `STR_USE_ITEM` only, silently (`isStartable` is false, so `checkStartConditions` is not called, :74-75); (b) a race or level restriction → `checkStartConditions(…, warn = true, …)` sends the restriction message, then `STR_USE_ITEM` (:75-80); a startable XML quest reaches `onItemUseEvent` and then `onDialog(ASK_QUEST_ACCEPT)`; **a start item registered with `registerQuestItem` reaches the registered handler's `onItemUseEvent` through `finishUse`, not through `CM_USE_ITEM`** (a fabricated handler registered for a real `<queststart>` item, so the case does not wait for T-01; `CM_USE_ITEM` must not call it a second time, `CM_USE_ITEM.cpp:108-112`); a `<read>` item that also has `<queststart>` does nothing in `ReadAction` (ReadAction.java:27-29). **Review addition:** E-10 is the start path of 39 XML quests, the only one for 38 (D19, §17.3 N1), and case (a) needs E-01's `isStartable`, so E-10 merges after E-01. | QuestStartAction.java:35-88, ReadAction.java:21-66 | R | S |

### Stage 1, quest npc AIs (P5-05 + A1 lease)

| Id | What | Java refs | Need | Eff |
|---|---|---|---|---|
| A-01 | `AbyssGuardSimpleAI` (`simple_abyssguard`): npc-vs-npc aggro with `isEnemy`, level ≥ 2, `isMapRegionActive`, `GeoService.canSee`. | AbyssGuardSimpleAI.java | R | S |
| A-02 | `ActionItemNpcAI` (`useitem`): the use bar, `ItemUseObserver` abort, `TaskId.ACTION_ITEM_NPC`, `AIActions.handleUseItemFinish`. **Depends on D-04** (M5c's `DialogService::isInteractionAllowed`: `handleDialogStart` calls it first, ActionItemNpcAI.java:35-38). Refresh: that body is ported in the working tree (M5c's D-02). `AIActions::handleUseItemFinish` ends in `GeneralInstanceHandler::handleUseItemFinish`, an empty inline (`GeneralInstanceHandler.h:144`), so nothing unported sits behind it on a world map. Census: 6 bodies. | ActionItemNpcAI.java | R | M |
| A-03 | `QuestItemNpcAI` (`quest_use_item`, A1 lease), 4 bodies: `onCanAct` gate, `onDialog(USE_OBJECT)`, drop registration, die, `requestDropList`. **Depends on A-02** (its superclass), **H-02** (`AbstractQuestHandler::onCanAct`), **T-01a** (`ItemCollecting`'s action-item `registerCanAct` and `onDialogEvent`), E-04 (`getEachDropMembersGroup/Alliance`) and M5b-3's drops (`AIActions.registerDrop`, `DropService.requestDropList`). Stage 1b. Refresh: M5b-3's two are met (`AIActions.cpp:111-113`, `DropService.cpp:162`). | QuestItemNpcAI.java:34-71 | R | S |
| A-04 | Tests (`tests/handlers_ai_core`): the AI smoke of handlers-and-porting-plan.md for the three names; a quest object used on a `ManualClock` (bar, abort on move, finish); a guard's npc-vs-npc aggro table. | | R | M |

### Stage 1, gate harness (P5-SC, tools/oracle)

| Id | What | Need | Eff |
|---|---|---|---|
| G-01 | **`oracle.py m5d-quests`**: `--race --map --level` → the nearby-quest set with grey bits over the **XML-only** registry (D9); `--quest ID` → kind, start/end npcs and their nearest spots, the page ids the template sends at each step, kill targets and counts, rewards after rates (kinah, exp, items), the follow-up quest at the end npc **or its absence** (`--quest 2101` must answer "no follow-up; quest-selection page 10"); `--exp-path --race --quest-exp … --kill-exp …` → from the exp events a run recorded (quest rewards and kills, in order), **where the level-up falls** (which event crosses `expNeed`) — the gate asserts that place (Y11); `--registration-order [--npc ID]` → Java's `HashMap` iteration order of `XMLQuests` and the resulting `onTalkEvent` / `questOnEnterWorld` lists (T-04); `--census` → re-derives §2.3-§2.5 with rev 2's startup-spawn and precondition rules. Uses `m5a/creation.py` (spawn, starting kinah 1,000) and `m5b-monster` (kill exp 80 at level 1 for 210133, 210134, 210363 and 210364 alike — 429 or 627 capped by `expNeed 400 × 0.2`). Plus `tools/oracle` tests. **Refresh — most of G-01 exists.** Commit `83db3742e` ("Oracles ahead") added `oracle.py m5d-quest` and `m5d-quests` (`tools/oracle/m5d/`, 3,406 lines; `tests/test_m5d.py`, 38 tests, green on this refresh's run). They cover the nearby set and its grey bits over the XML-only registry, with the wire order as `HashMap` buckets; per quest, the handler, registration, prerequisites, steps with page ids and vars, kill runs, and rewards after rates in payment order; and the follow-up window at each end npc, including 2101's page 10. The registry census is there too. **`--exp-path` is not needed.** `m5d-quest --quest 1102 --exp 370 --completed 1101` reports the level before and after the reward (`levelsSinceEnterWorld [1, 2]`), and the gate sums the recorded exp events itself. **What G-01 still owes:** `--registration-order [--npc ID]` (the `onTalkEvent` and `questOnEnterWorld` lists; `m5d/quests.py:329-337` computes them internally but does not print them), and `--census` for §2.4's reachability decomposition. The effort becomes S. | R | ~~M~~ S |
| G-02 | `GameSession` builders `buildCM_SHOW_DIALOG`, `buildCM_DIALOG_SELECT`, `buildCM_CLOSE_DIALOG`, `buildCM_DELETE_QUEST`, a `talk(npcObjectId, action, questId)` helper; **`decoders/QuestDecoders.{h,cpp}`** for `SM_DIALOG_WINDOW`, `SM_QUEST_ACTION` (six types), `SM_NEARBY_QUESTS`, and `SM_STATUPDATE_EXP` moved from `M5bScenarioTest.cpp`; `QuestDecodersTest.cpp` (body consumed exactly, no `serverpackets/` include). **Refresh:** M5c's G-02 plans the three dialog builders and an `SM_DIALOG_WINDOW` decoder. If they land first, G-02 keeps `buildCM_DELETE_QUEST`, `talk()`, `SM_QUEST_ACTION`, `SM_NEARBY_QUESTS` and the `SM_STATUPDATE_EXP` move. m5e-plan.md A-04c expects `SM_DIALOG_WINDOW`, `SM_QUEST_ACTION` and `SM_STATUPDATE_EXP` decoders from here. The decoders matter to M5e, not the file they live in. Reuse M5b-3's `ItemDecoders` and its `InventoryModel` (lifted by M5c) for Y6, Y12 and Y13. The kill helpers (`FightRecording`, `waitForRespawnAt`) are file-local in `M5bScenarioTest.cpp`/`M5b2ScenarioTest.cpp:1208`, so lift them or copy them. | R | M |

### Stage 2

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| G-03 | `TEST(M5dScenario, Run)`, `gs.scenario.m5d` — §10. | stage 1, I-05 | R | L |
| G-04 | `gs.scenario.m5d_geo` — §10.5. | G-03 | R | M |
| G-05 | **Re-green** `gs.scenario.m5a`, `m5a_geo`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo`, the M5b-3 and M5c gates and `gs.smoke.startup(_geo)` — §10.6 (I-05 already ran them on the join commit; G-05 re-runs them on stage 2's head and records before/after). Refresh: `m5b3` and `m5b3_geo` are registered at HEAD (`5fbb03a08`). Review addition: under the two gate slots (D12, if committed) G-05 runs `ctest -j 2` and uses the larger slot's sum as its budget (risk 13: about 1,470-1,720 s for the whole label with M5c's and M5d's gates, 28-35 min with the slowdown of a neighbouring run); under the single lock, the serial ~38-43 min. | I-05 | R | M |
| F-01 | Fixups the gate names, in the owning chunk. | G-03 | R | – |

### Stage 3

| Id | What | Need | Eff |
|---|---|---|---|
| E-07 | `task/**` (6 classes, 17 bodies) and `AbstractQuestHandler`'s follow family; `fieldmap.toml:74` already classifies `FollowingNpcCheckTask` K4. (Refresh: the transliterator's row B03, 14 generated files, waits for it.) | P | M |
| E-08 | `QuestSpawnAnalyzer` (D8). | W | S |
| G-06 | **Offered to the user (D16)**: a stress nightly where stress clients accept, abandon and progress quests while fighting; census adds `QuestEnv` (RefCounted) live 0 and `QuestState` live 0 after logout. Without the user's yes, the census rows go into the gate's Y14 only. Review addition: if the two gate slots commit (D12), G-06 holds **both** slots, as `gs.scenario.m5a_stress` does in the working tree's `StressTests.cmake`, so no other server runs beside its clients. | O (user) | M |

---

## 8. Lanes, stages and order

### 8.1 Entry criteria (from earlier milestones)

| Needed | From | Used by | Without it |
|---|---|---|---|
| `ItemPacketService::sendItemPacket` (and the `SM_INVENTORY_UPDATE_ITEM` path) | **M5b-3** (P5-07) | every kinah reward (§3.5) | no quest with gold can be finished — **refresh: met** (9 of 9 ported) |
| `ItemService::addItem` | **M5b-3** | reward and selectable items, work items on accept | 2,676 quests with item rewards; 1106, 1110, 1118, 1119 on Poeta — **refresh: met** |
| drops and loot (`DropRegistrationService`, `CM_START_LOOT`, `CM_LOOT_ITEM`) + D11 (L-04 and header request m5b3-h03) | **M5b-3** (P5-09) | quest loot, quest objects | 505 + 135 reachable quests — **refresh: met**, incl. the `QuestDrop` quest-id fix (`QuestsData.cpp:21-26`) |
| `CM_USE_ITEM`, `QuestStartAction` | **M5b-3** | `item_order`, item-started quests | 32 + 7 + more — **refresh: `CM_USE_ITEM` met; `QuestStartAction` (and `ReadAction`) came back to M5d as throwing stubs: E-10** (review: `CM_USE_ITEM` alone starts only 6 of the 39; E-10 starts the other 33 and 6 unregistered ones, §17.3 N1) |
| dialog plumbing (`CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_QUESTION_RESPONSE`, `DialogService`) | **M5c stage 0** (m5c-plan.md:227-228), else this plan's fallback (D4) | everything | – — **refresh: in M5c's working tree, uncommitted** (I-03 checks) |
| crafting | **M5c** | 586 quests | – |
| `EffectController::removeHideEffects`, npc skill casts, `onUseSkill` | **M5b-2** | talking while hidden, the kerub fight, 24 `skill_use` quests | – — **refresh: met** (`50a9158bf`) |
| **Not an entry criterion — owned by M5d (E-09, D14):** `BonusService::getQuestBonus` (P5-09), `AbyssPointsService::addAp` and `GloryPointsService::addGp` (P5-08), `CubeExpandService::questExpand` (P5-07, M5c's optional P-05) | **M5d** (leases for P5-09 and P5-07, I-04) | bonus, AP, GP and cube reward | 844 reachable quests could not be rewarded (§2.4) — **refresh:** `BonusService` P5-09a; the cube part is M5c's required P-05 |
| **Refresh — the other way round, a prerequisite M5c needs from here:** `QuestState::setPersistentState`, `canRepeat` | **M5d E-01**, or M5c under D18 | M5c's C19 Daeva seed | C19 fails (level 9, two ERROR lines) |
| **Left unported on purpose:** `Legion::addContributionPoints` (P5-11), `ChallengeTaskService::onChallengeQuestFinish`/`onAcceptTask` (P5-10), `WarehouseService::expand` (P5-07) | M5h; M5g (the P5-10 chunk); M5c or the "M5c-2" m5c-plan.md D2 proposes | AP for legion members; `CHALLENGE_TASK` quests; `extend_inventory="2"` | none reachable in M5d (no legions; 0 reachable challenge quests; 0 quests with `extend_inventory="2"`) |

### 8.2 Lanes

At most six per stage; chunks disjoint within a stage.

**Rev 2 splits stage 1 into 1a and 1b** (review finding: the chain E-01 → T-01 → T-04 → I-05 alone is ≥ 8 agent-days, and M5b-2's single
stage 1 is in its third part). The integrator commits between them; `:111` stays partial through 1a, so part 1a changes nothing the earlier
gates see.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 1a | **quest-engine** | P5-06a | E-01..E-03, E-05, E-06 | `tests/quest` |
| 1a | **handler-base** | P5-06b | H-01..H-07 | `tests/quest_handlers` |
| 1a | **xml-templates** | P5-06c | T-01a, T-02, T-04 for the five kinds | `tests/quest_templates` |
| 1a | **dialog-and-rewards** | P5-08, P5-15, P5-16 + file leases `BonusService.*` (P5-09; refresh: P5-09a), `CubeExpandService.*` (P5-07; refresh: only if M5c's P-05 did not land), refresh: `QuestStartAction.*`, `ReadAction.*` (P5-07) | D-02 tests, D-03, D-05, **E-09**, refresh: **E-10**; D-01/D-02 port/D-04 only as I-03's fallback | `tests/cm_ak`, `tests/cm_lz`, `tests/playersvc`, `tests/economy` (refresh: `tests/economy/P5-09a`), `tests/itemsvc` |
| 1a | **quest-npc-ais** | P5-05 | A-01, A-02, A-04 for them | `tests/handlers_ai_core` |
| 1a | **gate-harness** | P5-SC, tools/oracle | G-01, G-02 | `tools.oracle`, decoder self-tests |
| 1b | **xml-templates** | P5-06c | T-01b, T-03 (D10), the rest of T-04 and the smoke | `tests/quest_templates` |
| 1b | **quest-npc-ais** | P5-05 + A1 lease | A-03, A-04 for it | `tests/handlers_ai_core` |
| 1b | **quest-engine** | P5-06a | E-04 (after M5b-3's L-04; refresh: L-04 is in, so E-04 can move to 1a), fixups from 1a | `tests/quest` |
| 1a→1b | integrator | manifest, leases | I-01..I-04; **I-05, the join, closes 1b** | full verification + every earlier gate |
| 2 | **gate** | P5-SC | G-03, G-04 | `gs.scenario.m5d`, `_geo` |
| 2 | **regate** | P5-SC (second lease) | G-05 | the earlier gates |
| 2 | **xml-quest + fixups** | P5-06c, owners | T-03 if still deferred (D10), F-01 | owning tests |
| 3 | **phase-6 prerequisites** (+ stress if the user says yes, D16) | P5-06a, P5-14, P5-SC | E-07, E-08, (G-06) | `tests/quest`, (`gs.scenario.m5a_stress`) |

T-04 stays in the xml-templates lane in both parts: `tests/quest_templates` is P5-06c's, and rev 1's "quest-engine takes T-04's smoke" would
have had the P5-06a lane write another lane's test directory inside one stage.

**Merge order in 1a.** I-01/I-02 (day 0) → **E-01** (everyone reads quest state) → H-01/H-02 and T-01a → E-02, E-09 → T-02, H-03..H-06 →
the rest. G-01/G-02 have no body dependency and start on day 1. Cross-lane dependencies that are **not** "any time": **A-02 after D-04**
(M5c's `DialogService::isInteractionAllowed`, or the fallback's); E-06's reward-order case after E-09; T-04 for the five kinds after H-01,
E-02 and E-09. **Refresh, three more:** the D17 header batch (I-02) comes before E-02 and E-09, because both need the value-typed reward
list. **E-02 merges in the same integration part as D-02's quest-arm tests**, which replace M5c's case pinning `finishQuest`'s throw.
E-10 needs nothing but its lease; its `onDialog` case asserts only "no exception" until T-01a. (Review correction: E-10 also merges **after E-01**, because `finishUse` calls `qs.isStartable()` whenever the player holds the quest, QuestStartAction.java:74-75, and that is `AION_UNPORTED` until E-01, `QuestState.cpp:65`. Only a version whose tests use no quest state could merge earlier.)

**Merge order in 1b.** T-01b (join minimum first, §5) → T-03 → the T-04 smoke → **A-03 after A-02, H-02, T-01a and M5b-3's drops** → E-04
after M5b-3's L-04 → **I-05, the join (D3)**, which runs every earlier gate before it merges. (Refresh: M5b-3's drops and L-04 are met at
HEAD. A-03 and E-04 now wait only on in-milestone items.)

**Critical path.** 1a: E-01 (M) → T-01a (L: 840 Java lines of templates plus their tests) with H-01 (L) beside it → 5-7 days. 1b: T-01b (L:
1,227 lines) → T-04's smoke (M) → I-05 (M) → 5-7 days. **Stage 1 is 10-14 agent-days.** `quest-engine` is L and finishes first in 1a;
`dialog-and-rewards` is S-M in the planned case (M5c did its part), M-L in the fallback. (Refresh: the letters are kept as a measure of
size. The measured pace is faster. M5b-3 went from `27726d32c`, 08:37, to its gate commit `5fbb03a08`, 22:22, in about 14 hours: stages
0-1 in about 7 h with five reviewed lanes, then the gate stage. So "agent-days" here are an upper bound on wall time, not a forecast.
m5c-plan.md §5 makes the same correction.)

---

## 9. Header requests expected

| Request | Kind | For |
|---|---|---|
| `QuestDialog::operate(model::QuestEnv&, model::QuestState&) const`, `QuestNpc::operate(…)`, `QuestVar::operate(…)` → `bool`; `QuestConditions::checkConditionOfSet(model::QuestEnv&) const` → `bool`; `QuestOperations::operate(model::QuestEnv&) const` → `bool` (QuestDialog.java:30, QuestNpc.java:28, QuestVar.java:27, QuestConditions.java:29, QuestOperations.java:40) | **additive**, non-virtual (Java does not override them) | T-03 |
| `handlers/template/*.h` ×17 (`AbstractTemplateQuestHandler` and the 16 templates), `task/**` ×6, `QuestSpawnAnalyzer.h` | new files (no request); `fwd.h` already lists them | T-01, E-07, E-08 |
| `handlers/HandlerResult` companion (`fromBoolean(std::optional<bool>)`) | new file — **refresh: spelled `questEngine/handlers/HandlerResultInfo.h`, namespace `::aion::gameserver::questEngine::handlers` (D17a)** | H-06 |
| `AbstractQuestHandler.h`, `QuestEngine.h`, `QuestState.h`, `QuestVars.h` | **none expected** — every Java method is declared (measured: 0 undeclared names). **Refresh: one is needed after all** (the next row). The rest of these headers must stay as declared: the transliterator types its 910 generated handlers against them, and a changed signature means regenerating | E-*, H-* |
| **Refresh — `m5d-h01` (proposed, D17b): the reward list as values.** `AbstractQuestHandler::onBonusApplyEvent` and `QuestEngine::onBonusApplyEvent` take `std::vector<QuestItems>&` (today `const std::vector<const QuestItems*>&`, `AbstractQuestHandler.h:144-145`, `QuestEngine.h:162-163`; the dispatcher body at `QuestEngine.cpp:617-635` changes with it). `QuestService::getRewardItems` returns `std::vector<QuestItems>` (`QuestService.h:39-40`). `BonusService::getQuestBonus` returns `std::optional<QuestItems>` (`BonusService.h:24`) | **signature change on two hub headers** (no layout change; `QuestItems` is already a value type, `QuestItems.h:9-12`) | E-02, E-09; phase 6's 13 bonus-hook handlers (2 refused today, 11 regenerate) |
| `QuestService.h` | **one signature**: `getQuestDrop` takes `const std::unordered_set<runtime::Ptr<model::drop::DropItem>>&` (`QuestService.h:77`), but Java adds to that set (QuestService.java:682-728) — every method is declared, yet this one is unusable. **M5b-3 requests it as m5b3-h03** (a mutable `runtime::RcHashSet<runtime::Ref<DropItem>>&`, m5b3-plan.md:469). M5d depends on h03; if M5b-3 did not land it, E-04 carries the same request. **Refresh: applied** (`QuestService.h:81`); the `getRewardItems` return changes under `m5d-h01` | E-04 (D11) |
| `DialogService.h`, the AI handlers | none (existing declarations; new AI files are the handler lanes' own) | D-04, A-* |
| `BonusService.h`, `AbyssPointsService.h`, `GloryPointsService.h`, `CubeExpandService.h` | none (the bodies exist as `AION_UNPORTED` stubs with their declarations) — **refresh: except `BonusService.h:24`, `m5d-h01`** | E-09 |
| **Refresh:** `QuestStartAction.h`, `ReadAction.h` | none for `canAct`/`act` (`m5b3-h01` declared them). `finishUse` is private and undeclared in each (census), an additive request or a file-local helper, as the lane prefers | E-10 |
| **Manifest** (D1, D2): split P5-06, release the P4-08 lease, two test directories; the A1 file lease (D5); the `BonusService.*` (P5-09) and `CubeExpandService.*` (P5-07) file leases (D14) | build — **refresh:** `BonusService.*` is P5-09a; add the E-10 lease (P5-07); the cube lease only if M5c's P-05 did not land | I-01, I-04 |
| `game-server/config/m5d.properties.example` in the Java tree beside `m5a/m5b` (m5b-plan.md I-01's location) | none | G-03 |

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5d`)

### 10.1 Processes, databases and profile

As m5b2-plan.md §10.1, except:

| Piece | M5d |
|---|---|
| Schemas | `aion_ls_test_m5d_<hash>` / `aion_gs_test_m5d_<hash>` |
| Output | `<bin>/scenario/m5d` |
| `RESOURCE_LOCK` | the same `"aion_game_server_log;aion_login_server_log"`. **Review addition, conditional on another workflow's uncommitted `tests/scenario/ScenarioTests.cmake`:** if its two gate slots commit (`AION_GS_GATE_SLOT_1` = `aion_game_server_log`, `AION_GS_GATE_SLOT_2` = `aion_game_server_slot_2`, `:52-53`), G-03 and G-04 register `gs.scenario.m5d` and `gs.scenario.m5d_geo` with **one** slot, the same for both (they share the schema prefix `m5d`, `:38-42`): the slot with the smaller runtime sum at that time, and add their two runtimes to the comment (`:44-47`: "A new gate joins the slot with the smaller sum, together with its geo variant, and adds its runtime above"). The end-of-directory check `aion_gs_check_gate_slots` (`:55-78`) warns about a server test that holds neither slot and gives it both. G-06 holds both slots |
| Profile | the M5b-2 profile, geo off, plus `gameserver.rates.xp.quest` and `gameserver.rates.kinah.quest` left at their defaults (membership 0 → 1.0), `gameserver.analysis.quest_handlers=false`, `gameserver.character.creation.mode` left at its default 0 (D15); written out as `m5d.properties.example`. **Refresh:** since `4867fbc44` the M5b-2 profile carries `gameserver.rates.drop = 0` (`m5b2.properties.example`), so the kerub and sprigg kills leave no loot. Quest drops are not scaled by that rate (QuestService.java:673 compares `Rnd.chance()` with the drop's own chance), so a later case that needs one works under the same profile. Every kill still sends the unconditional `SM_LOOT_STATUS(LOOT_ENABLE)` (DropRegistrationService.java:104-106; M5b's R3), and Y8's and Y10's kill sequences must allow it |
| Allow-list | `tests/scenario/m5d_partial_allowlist.txt`, three sections as M5b-1's. `QuestEngine.cpp:111` **must not appear** (the site no longer exists); the re-pinned `:115` in §C; §B holds the partials M5d leaves on purpose (the analyzer if E-08 slips; `XmlQuestData::register_` if D10 defers 1127 — never hit by this script, which does not talk to 1127's npcs) |
| Characters | a fresh **Elyos WARRIOR** on account A and a fresh **Asmodian WARRIOR** on **account B** (D15: in the default `creation.mode` 0 a second race on account A is refused with `RESPONSE_OTHER_RACE`, CM_CREATE_CHARACTER.java:94-96, which `M5aScenarioTest.cpp:1368-1375` asserts; the two-account setup is M5a's, `:1298-1307`) |
| Targets | npcs **203049 elpas**, **203057 mires** (`210010000_Poeta.xml:383, 878`), **210133/210134** striped kerub (`:1224`, respawn 15 s); **203500 asak**, **203504 vandar**, **210363/210364** sprigg worker (`220010000_Ishalgen.xml:69, 1740, 1489`) — **all chosen by the oracle**, not hardcoded |

### 10.2 Cases

C1-C3 are M5a's login, create and enter world for the Elyos character. Distances (oracle `m5a-creation` spawn (1212.9, 1044.9, 140.8)):
elpas 12.1 m, mires 74.0 m, the nearest 210133 spot 21.4 m from mires.

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `m5d-quests --race ELYOS --map 210010000 --level 1`, `--quest 1101`, `--quest 1102`, `--quest 1103`; `--race ASMODIANS --map 220010000 --level 1`, `--quest 2101` (no follow-up), `--quest 2102` (follow-up 2103); at C13 and C16, `--exp-path` over the exp events recorded so far. **Refresh:** the commands exist as `m5d-quests --map ID --race R --level N` and `m5d-quest --quest ID`. For `--exp-path` read `m5d-quest --quest 1102 --completed 1101 --exp <the recorded sum>`, whose `followUp.character.levelsSinceEnterWorld` places the level-up (G-01) |
| **C4** | markers at enter world | read both enter-world `SM_NEARBY_QUESTS` (the first-enter one in the `CM_ENTER_WORLD` burst, the `CM_LEVEL_READY` one) |
| **C5** | talk | approach elpas to 3 m; `CM_SHOW_DIALOG(elpas)` |
| **C6** | select and accept 1101 | `CM_DIALOG_SELECT(elpas, 31, 1101)`; `CM_DIALOG_SELECT(elpas, 1002, 1101)`; `CM_CLOSE_DIALOG(elpas)` |
| **C7** | out of range | from 15 m, `CM_SHOW_DIALOG(mires)` |
| **C8** | report 1101 | approach mires to 3 m; `CM_SHOW_DIALOG`; `CM_DIALOG_SELECT(mires, 31, 1101)`; `(mires, 1009, 1101)` |
| **C9** | reward 1101 and the follow-up | `CM_DIALOG_SELECT(mires, 23, 1101)`; then a replayed `(mires, 23, 1101)` |
| **C10** | accept 1102, report too early | `CM_DIALOG_SELECT(mires, 1002, 1102)`; kill one kerub (M5b-1's fight helper); back at mires `(mires, 31, 1102)`, `(mires, 1009, 1102)` |
| **C10b** | journal reward refused | still at mires, 1102 in START with 1 of 3: `CM_DIALOG_SELECT(target 0, 108 = SELECTED_QUEST_AUTO_REWARD, 1102)` (1102 is `can_report="true"`, quest_data.xml:898); then `CM_SHOW_DIALOG(mires)` as the sentinel |
| **C11** | relog mid-quest | `CM_QUIT(0)`, read `player_quests`, re-enter |
| **C12** | finish the kills | kill kerubs until the oracle's count (3) is credited, **and not one more** — the script stops at the third credited kill and kills nothing else before C13 (the Y11 level-up depends on it) |
| **C13** | report and reward 1102 | right after the third credited kill: `(mires, 31, 1102)`, `(mires, 1009, 1102)`, `(mires, 23, 1102)` |
| **C14** | accept and abandon 1103 | `(mires, 1002, 1103)`; `CM_DELETE_QUEST(1103)` |
| **C15** | persistence of everything | `CM_QUIT(0)`, read `player_quests`, re-enter; talk to elpas again |
| **C16** | the Asmodian and an item reward | account B, Asmodian: login, create, enter world (M5a's steps); 2101 asak → vandar (as C5-C9); accept 2102 at vandar; **five** sprigg-worker kills (the fifth after the count of 4 is reached, before reporting); report and reward 2102; relog |
| **C17** | reports and shutdown | the M5a Q8 bar plus the M5d rows |

### 10.3 Assertions

Every row states what it proves, what it cannot, and the mutation it kills.

| # | Case | Assertion | Proves / cannot prove | Kills |
|---|---|---|---|---|
| **Y1** | C4 | **Both** enter-world `SM_NEARBY_QUESTS` of a first enter — the one inside the `CM_ENTER_WORLD` burst (the first-enter `onLevelChange(0, 1)`, PlayerEnterWorldService.java:204 → PlayerController.java:590-591; M5a's first-enter pattern, `M5aScenarioTest.cpp:1043-1044`) and the one at `CM_LEVEL_READY` (`CM_LEVEL_READY.cpp:108`) — decode to **exactly** the oracle's set for a level-1 Elyos in Poeta, compared as sets, with bit 17 set exactly on the quests whose `minlevel_permitted` is 2 or 3; 1101 present, 1102/1103/1104 absent (preconditions). A third, later one (§10.6 (d)) is allowed and must decode to the same set. | **Proves:** the XML registration, spawn → `questIds`, `checkStartConditions` over race, level window and `finished` preconditions, and that the first-enter level change (0 → 1) ran the five `ReportOnLevelUp` handlers without an ERROR (Y14). **Cannot:** the wire order (`JavaHashMapOrder` approximation, header-requests.md sm-b-2) or that the client draws a marker. | `:111` left partial (empty set); `allowedDiffToMinLevel` 2 → 0 (no grey quests); a precondition check dropped (1102 present) |
| **Y2** | C5 | `SM_DIALOG_WINDOW(elpasObj, page 10, quest 0)` and nothing else from the quest engine. | **Proves:** `CM_SHOW_DIALOG` → `onDialogRequest` → `DIALOG_START` → `TalkEventHandler` → `QuestEngine.onDialog(USE_OBJECT)` returning false for every elpas quest → `getStartPageId` → `hasQuestInteraction`. **Cannot:** the HTML the client shows. | `hasQuestInteraction` false (page 1011); a template that consumes `USE_OBJECT` (no window) |
| **Y3** | C6 | `(31, 1101)` → `SM_DIALOG_WINDOW(elpas, 1011, 1101)`; `(1002, 1101)` → in this order `SM_QUEST_ACTION` type 1 (ADD) quest 1101 status **3** step 0, `SM_NEARBY_QUESTS` without 1101, `SM_DIALOG_WINDOW(elpas, 1003, 1101)`; `CM_CLOSE_DIALOG` → `SM_LOOKATOBJECT`. | **Proves:** `DialogService` → `QuestEngine.onDialog(questId)` → `ReportTo` → `sendQuestStartDialog` → `startQuest`; the status byte is `QuestStatus.value()` (START = 3). **Cannot:** that the real client sends 1002 for "accept" (§15 Q1). | `startQuest` skipped (no ADD); the dialog sent before the state (order); status written as the ordinal (0) |
| **Y4** | C7 | From 15 m: an `SM_SYSTEM_MESSAGE` with `STR_DIALOG_TOO_FAR_TO_TALK`'s id and **no** `SM_DIALOG_WINDOW`. | **Proves:** the talk-range gate (talk distance 5 + 1). **Cannot:** the exact boundary. | `isInTalkRange` removed |
| **Y5** | C8 | `(31, 1101)` at mires → `SM_DIALOG_WINDOW(mires, 2375, 1101)`; `(1009, 1101)` → in this order `SM_QUEST_ACTION` type 2 (UPDATE) status **4** step **1**, `SM_NEARBY_QUESTS` (sent by `updateQuestStatus` because the status is now REWARD, AbstractQuestHandler.java:290-296), then `SM_DIALOG_WINDOW(mires, 5, 1101)` (ReportTo.java:95-98 → `sendQuestEndDialog`'s `SELECT_QUEST_REWARD` arm, AbstractQuestHandler.java:465-470). | **Proves:** the end-npc branch, `setQuestVar(1)`, REWARD, the reward-page index (group 0 → page 5). **Cannot:** multi-group rewards (E-06). | a report accepted at the start npc; `getRewardPageByIndex` off by one |
| **Y6** | C9 | `(23, 1101)` → **in Java's order** (QuestService.java:101-116, then AbstractQuestHandler.java:438-450): `SM_INVENTORY_UPDATE_ITEM` (the kinah item — `giveReward` pays kinah before exp, QuestService.java:222-227), an `SM_STATUPDATE_EXP` whose `currentExp` rose by **exactly the oracle's 130**, the `STR_GET_EXP` system message with `params[1] == "130"` (M5b-1's R1(a) decoder), `SM_QUEST_ACTION` UPDATE status **5** step 0 (:112), `SM_NEARBY_QUESTS` now **with 1102** and without 1101 (:116), then **`SM_DIALOG_WINDOW(mires, 1011, 1102)`**. | **Proves:** `finishQuest`, `XP_QUEST` (uncapped; `XP_HUNTING` would pay min(130, 80) = 80), the kinah path through `ItemPacketService`, and the follow-up continuation. **Cannot:** the kinah amount on the wire (the item blob is not decoded here — Y12 checks it after relog). (Refresh: now it can. M5b-3's `ItemDecoders` decode `SM_INVENTORY_UPDATE_ITEM`, so Y6 also asserts the kinah item's count 1,000 → 1,120.) | `Rates::XP_HUNTING` for quests (80); no follow-up (no 1102 window); COMPLETE not set |
| **Y7** | C9 | The replayed `(23, 1101)` produces **exactly one** packet, `SM_DIALOG_WINDOW(mires, 23, 1101)` — the next-page fallback — and **no** exp, inventory or `SM_QUEST_ACTION` packet. | **Proves:** `ReportTo`'s status dispatch for a finished, non-repeatable quest: 1101 is COMPLETE with `complete_count` 1 ≥ `max_repeat_count` 1, so `isStartable()` is false (QuestState.java:117-131), the status is neither START nor REWARD, `onDialogEvent` returns false (ReportTo.java:69-106), `QuestEngine.onDialog` returns false and `DialogService` sends the next page (DialogService.java:283-290). **Cannot:** either `status != REWARD` guard — the replay never reaches `sendQuestEndDialog` (AbstractQuestHandler.java:416-417) or `finishQuest` (QuestService.java:84-85); Y7b and H-07 test those. A second payment appears only if the dispatch **and** both guards were broken. | `DialogService`'s next-page fallback dropped (no window); `ReportTo` dispatching COMPLETE into `sendQuestEndDialog` together with a dropped guard |
| **Y7b** | C10b | The journal `(target 0, 108, 1102)` on a START quest produces **nothing**: the next packet is the sentinel's `SM_DIALOG_WINDOW(mires, 10, 0)`, with no exp, inventory, `SM_QUEST_ACTION` or `SM_NEARBY_QUESTS` before it. | **Proves:** `finishQuest`'s own guard (QuestService.java:84-85), reached directly by the journal branch of `CM_DIALOG_SELECT` (CM_DIALOG_SELECT.java:75-100: `can_report` + an auto-reward action calls `finishQuest` and returns). Without the guard the quest is **silently completed**: the reward group is fixed only in REWARD (QuestService.java:123-124), so `rewards` stays empty (:82, :98-99) and pays nothing, but `setStatus(COMPLETE)` and `SM_QUEST_ACTION` UPDATE status 5 (:108-112) and `SM_NEARBY_QUESTS` (:116) are sent. **Cannot:** the positive journal path (a REWARD quest finished from the journal) — D-02's quest-arm tests. | `finishQuest`'s `status != REWARD` return dropped (UPDATE 1102 status 5 before the sentinel) |
| **Y8** | C10 | `(1002, 1102)` → ADD 1102 status 3; the kill → `SM_QUEST_ACTION` UPDATE 1102 status 3 step **1**; `(31, 1102)` → page 1352; `(1009, 1102)` with 1 of 3 kills → **no** `SM_QUEST_ACTION`, and `SM_DIALOG_WINDOW(mires, 1009, 1102)` ("next page", DialogService.java:289-290). | **Proves:** `MonsterHunt.onKillEvent`'s var update and the kill-total check before REWARD; `DialogService`'s next-page fallback. **Cannot:** a kill credited by a team (P5-10). | the total check dropped (REWARD after one kill); `onKill` not reaching the handler |
| **Y9** | C11 | `player_quests` holds (1101, COMPLETE, complete_count 1) and (1102, START, quest_vars 1); after re-entry `SM_QUEST_LIST` = {1102: status 3, vars 1, count 0} and `SM_QUEST_COMPLETED_LIST` = {1101: count 1, second byte 1}. | **Proves:** `PlayerQuestListDAO` store and load through the now-ported `setPersistentState`/`getQuestVars`/`canRepeat`. **Cannot:** the periodic save path. | NEW not reset after store (a second INSERT → duplicate-key ERROR); `canRepeat` inverted |
| **Y10** | C12 | Kills 2 and 3 → UPDATE step 2 and 3, and no other `SM_QUEST_ACTION`. (Rev 1's "any further kill → no update" moved to Y13: a fourth kerub kill at level 1 would cross 400 exp during the kill and move Y11's level-up out of `finishQuest`; and a kill after C13 would only test the `status == START` check, MonsterHunt.java:177, not the end-var check.) | **Proves:** the kill count reaching the end var and the 6-bit packing for small counts. **Cannot:** counts ≥ 64 (E-06); counting past the end var (Y13). | a kill not credited (no step 2/3) |
| **Y11** | C13 | `(31, 1102)` → page 1352; `(1009, 1102)` → UPDATE status 4, `SM_NEARBY_QUESTS`, page 5; `(23, 1102)` → in Java's order: `SM_INVENTORY_UPDATE_ITEM` (kinah), then — because the oracle's `--exp-path` (refresh: `m5d-quest --exp`, see C0) over the recorded events (130 from 1101, 80 per credited kill, 3 kills: 370 < 400 ≤ 370 + 180) places the level-up **in this reward** — the level-up burst of `onLevelChange(1, 2)` (`SM_ACTION_ANIMATION` LEVEL_UP … `SM_NEARBY_QUESTS`, PlayerController.java:583-591) **before** an `SM_STATUPDATE_EXP` whose exp rose by **180** and whose `maxExp` is level 2's (setExp sends it after `onLevelChange`, PlayerCommonData.java:283-287), `STR_GET_EXP` "180", UPDATE status 5, `SM_NEARBY_QUESTS` with 1103, then **`SM_DIALOG_WINDOW(mires, 1011, 1103)`**. The level-up falls inside `finishQuest` only if exactly 2 or 3 kills were credited after 1101 (130 + 80k < 400 ≤ 310 + 80k); the gate asserts the place the oracle computes from the recorded kills, and fails if it is not this reward (the script went off its path). | **Proves:** the second template kind end to end, the follow-up again, and a level-up **inside** `finishQuest` running `onLevelChanged` → 5 `ReportOnLevelUp` → `startQuest` (refused for level 30+) without an ERROR. **Cannot:** that the level-2 skill autolearn is right (M5e). | an unported body on the level-up path (ERROR, Y14); `XP_HUNTING` for quests (180 capped to 80) |
| **Y12** | C14, C15 | `(1002, 1103)` → ADD 1103; `CM_DELETE_QUEST(1103)` → `SM_QUEST_ACTION` type 3 (ABANDON) quest 1103 and `SM_NEARBY_QUESTS` containing 1103 again; after re-entry `SM_QUEST_LIST` has no 1103, `SM_QUEST_COMPLETED_LIST` = {1101, 1102}, **`SM_INVENTORY_INFO` kinah = oracle start (1,000) + 120 + 400 = 1,520**; `CM_SHOW_DIALOG(elpas)` now answers page **1011** (nothing left to offer). | **Proves:** `abandonQuest` (delete for a never-completed quest), the kinah reward persisted, and `hasQuestInteraction` turning false. **Cannot:** abandoning a repeatable quest (reset to COMPLETE — E-06). | `deleteQuest` skipped (1103 back after relog); kinah credited but not saved |
| **Y13** | C16 | Asmodian, ordered patterns as Y3/Y5/Y6: both enter-world `SM_NEARBY_QUESTS` = the oracle's Ishalgen set, **2101 and 2102 both present** (2102 has no precondition); `CM_SHOW_DIALOG(asak)` → page 10; `(31, 2101)` → 1011; `(1002, 2101)` → ADD 2101 status 3, `SM_NEARBY_QUESTS` without 2101, `(asak, 1003, 2101)`; at vandar `(31, 2101)` → 2375; `(1009, 2101)` → UPDATE status 4 step 1, `SM_NEARBY_QUESTS`, page 5; `(23, 2101)` → `SM_INVENTORY_UPDATE_ITEM` (80 kinah), `SM_STATUPDATE_EXP` +130, `STR_GET_EXP` "130", UPDATE status 5, `SM_NEARBY_QUESTS`, then **`SM_DIALOG_WINDOW(vandar, 10, 0)`** — no follow-up: no startable quest at vandar names 2101 as its `finished` precondition, and 2102 is startable, so `npcHasNewQuest` sends the selection page (AbstractQuestHandler.java:437-457). Then `(31, 2102)` → 1011, `(1002, 2102)` → ADD 2102; kills 1-4 → UPDATE steps 1-4 (the fourth crosses 400 exp during the kill: 130 + 4 × 80); a **fifth** kill of 210363/210364 → **no** `SM_QUEST_ACTION`; `(31, 2102)` → 1352, `(1009, 2102)` → UPDATE status 4, `SM_NEARBY_QUESTS`, page 5; `(23, 2102)` → the reward item update (10 × 169300002, `ItemService.addItem` before `giveReward`, QuestService.java:101-104), kinah, `SM_STATUPDATE_EXP` +180, UPDATE status 5, `SM_NEARBY_QUESTS` with 2103, **`SM_DIALOG_WINDOW(vandar, 1011, 2103)`**; after relog `SM_INVENTORY_INFO` holds **10 × 169300002** and kinah 1,000 + 80 + 120 = 1,200. **Refresh, a correction:** a new Asmodian Warrior already holds **20** × 169300002 (`oracle.py m5a-creation --race ASMODIANS --class WARRIOR`; max stack 10,000, `m5b3-item`). So the reward is an `SM_INVENTORY_UPDATE_ITEM` on that stack, 20 → 30, and after relog the stack is **30**: the oracle's starter count plus the reward, both taken from the oracles. Nothing in the script uses a Bandage. | **Proves:** a second race, `race_permitted`, an **item reward** through `ItemService.addItem`, the follow-up's absence (2101) and presence (2103) decided by preconditions, and `MonsterHunt`'s end-var check (`total <= m.getEndVar()`, MonsterHunt.java:197-198) on a fifth kill. **Cannot:** selectable rewards (`SELECTED_QUEST_REWARDn`) — E-06 and the real client. | reward items dropped; the race filter inverted (2101 offered to the Elyos, Y1); counting past the end var (a step-5 UPDATE); a follow-up that ignores preconditions (a 2102 window after 2101) |
| **Y14** | C17 | The Q8 bar: `unported_trace.txt` empty, census empty, lockdep empty, no watchdog dump, **no ERROR in either log** (this is what catches an exception inside the 28 `catch (const std::exception&)` blocks of `QuestEngine.cpp` and the one in `checkStartConditions`); `partial_trace.txt` ⊆ the allow-list, §A hit ≥ 1, §B hit 0; `live_counts.txt`: `QuestEnv` live 0 with `created > 0`, `QuestState` live 0 after the characters logged out. | **Proves:** nothing on the scripted path fell outside the port, and no quest object leaked. **Cannot:** quests off the path — T-04's smoke covers the other kinds. | any handler that throws into a dispatcher's `catch`; a `QuestEnv` kept by a task |

### 10.4 Mutation proof (the standard)

Each assertion above must be watched failing, with the mutation and both outputs quoted. The minimum set, including what the gate cannot catch:

| Mutation | Must fail | Must stay green |
|---|---|---|
| leave `QuestEngine.cpp:111` partial (no registration) | **Y1**, Y2 (page 1011), everything after | M5a/M5b gates |
| `QuestService::giveReward`: `Rates::XP_HUNTING` instead of `XP_QUEST` | **Y6** (80 instead of 130) | Y3, Y5 |
| `AbstractQuestHandler::sendQuestEndDialog`: drop the follow-up block | **Y6**, **Y11** and **Y13** (no 1011 window for 1102/1103/2103) | Y7 |
| `sendQuestEndDialog`: drop the `status != REWARD` return | **nothing in the gate** — `ReportTo` and `MonsterHunt` reach it only in REWARD; **H-07** must catch it (a START quest with 1009 then sends `SM_DIALOG_WINDOW(npc, DialogPage.NULL, quest)`) | the gate |
| `QuestService::finishQuest`: drop the `status != REWARD` return | **Y7b** (UPDATE 1102 status 5 before the sentinel) | Y6, Y7 |
| `DialogService::handleQuestDialogueOrSendNextPage`: drop the next-page window | **Y7** and **Y8** (no `SM_DIALOG_WINDOW(mires, 23, 1101)` / `(mires, 1009, 1102)`) | Y6 |
| `MonsterHunt::onKillEvent`: drop `total <= m.getEndVar()` | **Y13** (a step-5 UPDATE on the Asmodian's fifth kill) | Y8, Y10 |
| the follow-up loop ignores the `finished` precondition (AbstractQuestHandler.java:446) | **Y13** (`SM_DIALOG_WINDOW(vandar, 1011, 2102)` instead of page 10 after 2101) | Y6 |
| `MonsterHunt::onDialogEvent`: skip the kill-total check on `SELECT_QUEST_REWARD` | **Y8** (REWARD after one kill) | Y10 |
| `QuestVars::setVarById`: shift by 5 instead of 6 | **nothing in the gate** — 1102 counts in var 0, which is not shifted. **E-06's golden vector for var 1 must catch it.** | the gate |
| `QuestState::setPersistentState`: keep NEW after `store` | **Y9/Y14** (duplicate-key ERROR at the second store) | Y3 |
| `QuestService::abandonQuest`: skip `deleteQuest` | **Y12** (1103 back after relog) | Y3 |
| `checkStartConditions`: ignore `race_permitted` | **Y1** (Asmodian quests in the Elyos set) | Y3 |
| `getStartPageId`: drop `hasQuestInteraction` | **Y2** (1011) and not Y12's final 1011 | Y3 |
| `XMLQuests` iterated in `std::unordered_map` order | **nothing in the gate** — the set comparisons and the one-candidate follow-ups do not see order. **T-04's registration-order case must catch it**: mires' `onTalkEvent` list and `questOnEnterWorld` against Java's `HashMap` order from G-01. (Rev 1 named mires' `onQuestStart`, which is a `HashSet` whose order does not depend on registration order at all — risk 5.) | the gate |
| `QuestService::finishQuest`: skip `validateAndFixRewardGroup` | **nothing in the gate** (single-group quests); **E-06** | the gate |
| a `QuestEnv` kept in a static | **Y14** | Y1-Y13 |

### 10.5 The geo gate

`gs.scenario.m5d_geo`: the same script with `gameserver.geodata.enable=true`, `LABELS "scenario;realdata;geo"`, the same lock (review: or, under the two gate slots, the same slot as `gs.scenario.m5d`, §10.1). **Geo changes
nothing on the quest path itself** — `isInTalkRange` has no geo test and no zone-triggered quest is on the start maps (§5, the six
`start_zone` quests are in instance 300610000) — so, as m5b-plan.md §6.4 did, the geo gate is a re-run whose value is the approach and the
fights under geo heights (mires stands 12 m lower than the spawn). Say so in the gate's comment; do not invent a row that cannot fail.

### 10.6 Re-greening the earlier gates

Four things move: (a) the `QuestEngine.cpp:111` row leaves §A of every allow-list (M5a, M5b, M5b-2, and M5b-3/M5c where they exist) in the
join commit, and the `:115` rows are re-pinned there (D3); (b) both enter-world `SM_NEARBY_QUESTS` are no longer empty — the M5a/M5b patterns
name them without decoding them, which is fine; (c) **every first enter world** (0 → 1) and any later level-up now runs five
`ReportOnLevelUp` handlers (§5), every enter world runs the 26 `questOnEnterWorld` handlers, and any kill of 210133 runs `MonsterHunt(1102)`; (d) `SM_NEARBY_QUESTS` can arrive **outside** the enter-world
burst 1.5 s after a quest-start npc's first spawn in the player's instance (`WorldMapInstance.java:123-129`) — a gate with a strict packet
sequence around a fight must tolerate it. Record before/after in the wave report. (Refresh: M5b-3's gate is registered at HEAD, and its
kills of 210133 (`M5b3ScenarioTest`) join (c). M5c's gate talks only to functional npcs, and `getStartPageId` answers 10 for those with or
without quests (DialogPage.java:119-121), so the join changes no page it asserts. Its C19 needs D18, not the join.)

---

## 11. Risks

Ordered by likelihood, with the evidence.

1. **Registration wakes the other gates (lesson 2).** §5 lists 16 entry points; three hit unported bodies on paths every gate runs
   (enter world, the first-enter level change, and — once any quest is saved — login and logout), and once M5c's `CM_SHOW_DIALOG` exists,
   every talk to any npc runs the `onDialogEvent` of every quest in its `onTalkEvent` list — which is why §5's join minimum covers all 16
   templates' state-free hooks, not five kinds. D3 is the mitigation, and the join commit must run all
   earlier gates before it merges. The swallowing `catch` blocks (`QuestEngine.cpp` wraps every dispatcher; `checkStartConditions` catches too)
   turn a missed body into an ERROR line, not a crash — exactly m5b-client-session.md S-1's shape. Y14's "no ERROR" is the only detector.
2. **The upstream milestones may not deliver what M5d assumes (§8.1).** Kinah needs M5b-3's `ItemPacketService`; the gate cannot pass without
   it. The dialog plumbing comes from M5c's stage 0 (D4), the quest drops and their header request h03 from M5b-3's L-04 (D11) — both drafts
   are unreviewed. I-03 checks this before stage 1; if M5b-3 slipped, M5d can run stage 1 but not stage 2. **Refresh:** M5b-3's half is
   retired (all met at `5fbb03a08`, §17.2). M5c's half remains until M5c commits. Its stage 0 is in the working tree, and its plan is
   reviewed and refreshed. A new risk runs the other way: M5c's C19 needs two of M5d's bodies (D18).
3. **Size: the roadmap's 163 is 268 in the chunk and ~318 in the milestone.** The template classes have no file and no site; a lane sizing
   from `grep AION_UNPORTED` would find them on day 3. §4.2 is the list; T-01 is XL, split over 1a and 1b.
4. **The hidden AI prerequisites (D5).** `simple_abyssguard` is 997 spawn spots in the data, **870 spawned at startup** (127 more only in
   siege spawns; 2 in Poeta, 10 in Ishalgen); registering it turns 870 npcs from `DummyNpcAI` into guards that attack enemy npcs of level ≥ 2
   in active map regions — npc-vs-npc fights across the world at startup, a combat surface the census and the gates must see.
   `quest_use_item` is 2,959 spots (2,720 at startup) and lives in a phase-6 chunk (A1).
5. **Registration order decides visible behaviour.** `XMLQuests.getAllQuests()` iterates Java's `HashMap` (XMLQuests.java:31-43), and the
   registration loop follows it, so the order is visible wherever a registered list is an `ArrayList`: `QuestNpc.onTalkEvent` (the first
   handler that consumes a `USE_OBJECT` in `QuestEngine.onDialog`, QuestEngine.java:167-175, and the first REWARD quest in
   `sendQuestEndDialog`'s first loop, AbstractQuestHandler.java:425-436), `onKillEvent` (the order of UPDATEs when two quests share a monster)
   and `questOnEnterWorld` (QuestEngine.java:61, 342). `XMLQuests.cpp:12-18` reproduces the `HashMap` order; T-04 pins it. **The follow-up
   choice is not among them**: it iterates `QuestNpc.onQuestStart`, a `HashSet<Integer>` (QuestNpc.java:15, 27; C++ `runtime::HashSet`,
   `QuestNpc.h:25`) whose order is the hash order of the ids whatever the registration order — for mires 1104, 1102, 1103 — which H-07's
   follow-up case covers. Until phase 6 the lists also lack the Java quests (D9).
6. **Quest state is shared by threads without locks.** A kill updates vars on the npc's death path while a dialog on the packet thread sets
   REWARD; Java has no synchronization in `QuestState` (only `QuestStateList.addQuest/deleteQuest` are `synchronized`). Port faithfully with
   `// java-race`; do not add locks that change ordering.
7. **The oracle and the server read the same XML.** Y1's set and the reward numbers are consistency checks against `quest_data.xml`, not
   retail truth — the same limit m5b2-plan.md X1 states.
8. **The kerub fight is a real fight, and the Y11 level-up depends on the kill count.** 210133 is aggressive (`srange` 7), respawns in 15 s
   at one of 18 spots and, after M5b-2, casts skill 16419. At level 1 every kerub or sprigg-worker kill pays 80 exp (capped), so the
   level-up lands inside 1102's reward only after exactly 2 or 3 credited kills since 1101 (§10.3 Y11). The script therefore kills exactly
   three kerubs and reports at once; a kerub that aggroes on the way is fought only if it attacks, and any kill it causes is recorded and
   moves the oracle's placement, which fails Y11 loudly rather than silently. Budget the gate at 150-240 s (two characters, eight kills,
   three relogs).
9. **`SM_QUEST_ACTION` writes an empty body for `extra_category` quests** (SM_QUEST_ACTION.java writeImpl) — a decoder that insists on a type
   byte fails on those. None is on the gate path; the decoder must accept an empty body.
10. **The reward services of E-09 reach into abyss and legion code** (D14). `addAp` runs `onRankChanged` (rank packets, rank-limited items,
    abyss skills — all ported) and, for a legion member only, the unported `Legion::addContributionPoints`. No M5d character is in a legion;
    T-04's `RelicRewards` / `KillInZone` / `WorkOrders` cases and E-09's unit tests are the only runs of this path in M5d, and no gate plays
    it (no start-map quest pays AP, GP or a bonus).
11. **Refresh — unit cases that pin today's throws turn red when the bodies land.** Three are known (review: the refresh listed two).
    `tests/quest/QuestDropTest.cpp:347` (M5b-3; E-01 rewrites it), `tests/player/PlayerModelBodiesTest.cpp:441` (P4-12, committed in
    `a99ec5fcb`; whoever ports `setPersistentState`, E-01 or M5c under D18, rewrites it under a P4-12 test-file lease) and
    `tests/cm_ak/DialogSelectPacketsTest.cpp:450-457` (M5c, uncommitted; D-02 replaces it together with E-02). Other lanes may add more before M5d branches, so each M5d lane greps the test tree for `UnportedException` next to a quest body
    it ports (`QuestService`, `QuestState`, `QuestVars`, `AbstractQuestHandler`) before it merges.
12. **Refresh — phase 6 builds on M5d's headers.** The transliterator emits 910 handlers typed against today's `AbstractQuestHandler.h`,
    `QuestState.h`, `QuestEnv.h` and `QuestService.h` (phase6-questgen-prototype.md §1.2-§1.3). A signature change M5d makes beyond D17 means
    regenerating them. D17 itself also needs the prototype's `api.PLANNED` entry to match whatever spelling H-06 lands with. Announce every
    quest-header change in `header-requests.md` so the P6-T lane regenerates against it.
13. **Refresh — the gate budget.** The eight earlier scenario gates took 1,746 s in M5b-3's final regate (m5b3-plan.md §19.3; m5c-plan.md
    risk 11 used §18.5's ~1,715 s), and M5c adds 120-200 s. M5d adds 150-240 s, and its geo re-run adds about 260-420 s if it scales as
    `m5b2_geo` did over `m5b2` (301 s against 172 s). That takes `ctest -L scenario` to about **38-43 minutes**, before the two startup
    smokes (34 s and 150 s). The join (I-05) and G-05 each run it once. **Review correction: that is the serial total, the right figure
    only while the single lock stands.** The 1,746 s is exactly the eight gates' runtimes added (65 + 170 + 146 + 314 + 227 + 351 + 172 +
    301). Another workflow's uncommitted `ScenarioTests.cmake` introduces two gate slots, runs `ctest -j 2`, and balances the slots at
    1,062 s and 1,051 s, smokes and M4 check included (`:44-47`). By its rule, M5c's gate (120-200 s, no geo) joins slot 2, which rises to
    1,171-1,251 s. M5d's pair (410-660 s) then joins slot 1, which rises to **1,472-1,722 s**, and the larger slot sets the wall time:
    about 25-29 min, or roughly **28-35 min** with the 16-21 % that a neighbouring run cost in the two measured runs (1,227 s and 1,286 s
    against a 1,062 s slot, `:16-18`). The pair cannot be split, because a gate and its geo variant share a slot (`:38-42`). If the slots
    commit, I-05 and G-05 budget the larger slot; if not, the serial figure above. The runtimes are for a Debug tree, each run alone.

---

## 12. Sizing and the split

| Stage | What a player can do at the end | Chunks | Bodies | Lanes | Duration |
|---|---|---|---|---|---|
| **1a — the engine and the first five kinds** | nothing new yet (`:111` stays partial; the earlier gates see no change) — unit and real-data tests only | P5-06a/b/c, P5-08, P5-15, P5-16, P5-05, P5-SC + leases P5-09/P5-07 files | ~205: engine 37, handler base 89, 6 template classes 32 + 16 `register_`, dialog-and-rewards 18 (E-09 10; +15 if M5c's stage 0 is missing), AIs ~13. **Refresh: ~213**, because E-10 adds 8 to dialog-and-rewards; −3 if M5c's P-05 has taken the cube; E-04's 2 may move in from 1b | **6** | 5-7 days |
| **1b — every template, then the join** | Talk to a quest giver, accept, progress by kills and dialogs, report, get rewards; abandon; the journal survives a relog; quest objects and guard quest givers work | P5-06c, P5-06a, P5-05 + A1 lease | ~69: 11 template classes 43, xmlQuest 20 (D10), `QuestItemNpcAI` 4, group drop variants 2; then I-05 | 3 + integrator | 5-7 days |
| **2 — the gate proves it** | the same, proved; the earlier gates green; `xml_quest` 1127 if it slipped | P5-SC, P5-06c, fixups | fixups + gate | 3 | 3-4 days |
| **3 — phase-6 ready** | the follow tasks for Java quests, the analyzer (the acceptance row, D8); quests in the stress nightly if the user says yes (D16) | P5-06a, P5-14, P5-SC | ~24 | 1-2 | ~2 days |

**Why this order.** Stage 1a has a green point of its own (the engine, base and five-kind tests, E-09's tests, the decoder and oracle
self-tests) and changes nothing the earlier gates see. Stage 1b's green point is T-04's smoke over all 4,184 and every earlier gate run on the
join commit. The D3 join closes stage 1 because stage 2's gate needs the registration. Stage 3 holds what no client can reach in M5d.

**Why not a split by template kinds** (rev 1's fallback). The join needs §5's join minimum for **all 16** templates: every template's
`onDialogEvent` runs on every talk once M5c's `CM_SHOW_DIALOG` exists, `KillInWorld.onEnterWorldEvent` on every enter world for its 10
`invasion_world` quests, `MonsterHunt`'s and `KillSpawned`'s `onKillEvent` on the M5b/M5b-2 gates' kills. The state-free hooks are most of
each class, so a "second wave" of templates could hold only the PvP and distance/aggro hooks — too little to be a wave. The split is therefore
by **time** (1a before any template beyond the five, 1b up to the join), and the only content that may pass the join is D10's `xml_quest`
(behind its own partial) and the PvP hooks. Splitting at the engine (`AbstractQuestHandler` first, templates later) would leave a part with no
green point; 1a avoids that by taking the five kinds with their tests.

**Against the roadmap:** "one 163-body milestone" → **~318 bodies, 4 stages (1a, 1b, 2, 3), 15-20 agent-days (3 to 4 weeks)**, larger than
M5b-1; "~4,184 quests online" → 4,184 registered, **2,511 startable** (at startup-spawned givers), **901 completable with dialogs and kills**
once M5b-3's reward packets exist (795 of them without E-09), 505 more with M5b-3's loot and 135 with its quest-object drops.
(Refresh: ~322 bodies at HEAD, ~307 after M5c's stage 0 commits. M5b-3's reward packets and loot exist now, so the 901, 505 and 135 hinge
only on M5d. The agent-day figures are an upper bound; by M5b-3's measured pace the milestone is days, not weeks. See §8.2.)

---

## 13. Real-client checklist (user, after stage 2)

Prerequisites as m5b-plan.md §10 steps 1-6, with `mygs.properties` from `m5d.properties.example`, M5b-3 landed.

1. Make an Elyos character. On entering Poeta, **quest markers** appear over Elpas (a few steps away) and other quest givers.
2. Talk to Elpas: the dialog lists **"Sleeping on the Job"**. Accept: the journal shows it; Elpas's marker goes away.
3. Walk to Mires (about 75 m, down the slope). Report: the reward window opens; take it: **+120 kinah, +130 exp**. Mires immediately offers
   **"Kerubar Hunt"**.
4. Accept; kill **three striped kerubs** near Mires; the journal counts 1/3, 2/3, 3/3. Return, report: **+400 kinah, +180 exp**, and the
   reward takes you to **level 2** (after exactly three kills; a fourth kill before reporting reaches level 2 during the hunt instead).
   Mires offers **"Grain Thieves"**. Optional: before reporting, try **"Report" from the journal** — with the quest not yet at the reward
   step nothing must happen.
5. Accept "Grain Thieves", then **abandon it from the journal**: it disappears and Mires's marker comes back.
6. Log out in the middle of a quest and back in: the journal and the counters are intact.
7. Try the rest of Poeta: 1104 (after 1103's grain sacks — these are quest objects, D5), 1105/1108 (loot), 1110 Melampus → Namus, 1115/1118
   (several npcs), Oz and Tula (guards, D5), 1127 "Ancient Cube".
8. **Expected gaps, not regressions:** the prologue movie, the campaign missions 1001-1005, "Kalio's Call" (1100), "A New Skill" (1205), and
   1107/1111/1114/1122/1123 are Java-handled (phase 6) — no marker, no dialog for them. 1206/1207 need gathering (D13). Ellino and Madeline
   (1230/1231) do not exist in the world. On the Asmodian side, "Return to Sender" (2107) cannot be started — in Java either: its start
   scroll has no source in the data. (Refresh: using a looted *Namus's Diary* or *Broken Axe Handle* in Poeta, or a *Silver Necklace* or
   *Old Scroll* in Ishalgen, shows the "used" message and starts nothing, because their quests are Java-handled. Before E-10 it logged an
   ERROR.)
9. Make an Asmodian **on a second account** (the default creation mode refuses a second race on one account, D15): Asak → Vandar
   ("On Your Feet!"; Vandar then shows his quest list — "A Bloody Task" was already on offer), then "A Bloody Task" (four sprigg workers) pays
   **10 Bandages**, and Vandar immediately offers "The Sprigg Report". (Refresh: the new character starts with 20 Bandages, so the stack
   shows 30.)
10. Optional, with a GM account that has `DIALOG_INFO` access: every dialog click prints "Quest ID: …, Dialog Action: …" in chat
    (CM_DIALOG_SELECT.java:66-68). **Write down the action ids for "accept", for taking a reward without a choice and for the journal's
    "Report"** — they answer §15 Q1 (the gate uses 1002, 23 and 108).
11. Send `game-server/log/`, `unported_trace.txt`, `partial_trace.txt`, `live_counts.txt` and the summary.

---

## 14. What was measured and what was inferred

**Measured** (parses and greps over the two trees at `c1edb0afb`; re-runnable; the scripts are throw-away and G-01's `--census` replaces them):

- 8,043 quests; 4,184 XML elements in 89 files (10 empty), 15 kinds with the counts of §2.2, 0 duplicate ids, all in `quest_data.xml`; 1,035
  Java handlers (1,030 `super(N)` + 5 via a constant), 95,539 Java lines, 0 overlap with XML; 2,824 with no handler and their categories; the
  1,845 `restricted` flags and that `isRestricted()` has no caller.
- §2.4's start and completion tables, **rev 2**: startup spawns = direct `<spawn>` children of `<spawn_map>` on non-instance maps
  (`world_maps.xml` `instance`), minus `handler="RIFT"`; AI names from `npc_templates.xml`; preconditions by Java's optional/mandatory group
  rule — 2,072 / 439 / 307 / 322 / 498 / 415 / 126 / 5 and 901 / 505 / 586 / 316 / 135 / 30 / 24 / 14; the 310 quests that only service
  spawns keep from being reachable, by source; the reward overlays (760 / 363 / 57 / 2 over 4,184; 687 / 169 / 14 / 1 and 844 over the
  2,511; 106 of the 901); 156 `CHALLENGE_TASK` XML quests, none reachable; the AI spot counts at startup (870, 2,720, 85 `useitem`).
  Under rev 1's all-folder scope and first-group precondition rule the script reproduces rev 1's 2,328 / 487 / 324 / 498 / 415 / 126 / 6.
- §2.5's lists by id, rewards, kill targets, work items, quest objects, AIs and spawn state; that 801032/801033 have no live spawn; that
  182203107 (2107's start item) occurs nowhere in `static_data` or `data/handlers` but its own template and 2107's work item; the 9
  `simple_abyssguard` givers of 16 start-map quests.
- The P5-06 site counts (163 + 2 partial), the per-file split, 8,066 Java lines (`wc -l` over the 79 files), and the 105 invisible bodies
  (method-name comparison against the shell and generated headers, false positives removed by hand; `task/` recounted as 5 + 4 + 4 × 2).
- The reward bodies outside the chunk: `BonusService.cpp:7-17`, `AbyssPointsService.cpp:11-25`, `GloryPointsService.cpp:7-9`,
  `CubeExpandService.cpp:11-37`, `WarehouseService.cpp:24-42` are `AION_UNPORTED`; `ChallengeTaskService.cpp:30-32` too; `Legion.cpp:91` is
  unported; `AbyssRank::addAp/addGp`, `AbyssSkillService::updateSkills`, `Equipment::checkRankLimitItems`, `Player::setCubeLimit`,
  `ItemGroupsData`'s group getters and `PlayerCommonData::addDp` have bodies; `SM_ABYSS_RANK`, `SM_ABYSS_RANK_UPDATE`, `SM_CUBE_UPDATE` have 0
  `AION_UNPORTED`; the chunk owners by `chunks.py owner`.
- The oracle's kill exp for 210134, 210363 and 210364 at level 1 (80 each, `m5b-monster`), and `gameserver.character.creation.mode` 0 with
  its `RESPONSE_OTHER_RACE` branch.
- Every "ported" and "unported" claim of §1, §3 and §5 about a named C++ body, by reading the body; every server packet's 0 unported; the
  missing client packets.
- The registrations of §5 (`aggro_start_npc_ids` and `start_dist_npc_id` occur 0 times; `start_zone` 6 times, all in 300610000; 10
  `report_on_levelup`; 16 invasion-world quests; 198 item-collecting quests with 7xxxxx action items).
- The P4-08 lease's exact coverage (`chunks.py owner` on each file) and `check-ownership`'s owner-or-lessee rule.
- The oracle runs: the Elyos spawn (1212.94, 1044.85, 140.76) and Asmodian spawn (571.04, 2787.34, 299.88), starting kinah 1,000
  (item 182400001); 210133's kill exp at level 1 = 80 (429 capped by `expNeed 400 × 0.2`); the distances of §10.2.
- `XP_QUEST` uncapped vs `XP_HUNTING` capped (Rates.java), quest rates 1.0 for membership 0 (RatesConfig.java).

**Inferred, to be confirmed by a lane:**

- **That the real 4.8 client sends `QUEST_ACCEPT_1` (1002) to accept and `SELECTED_QUEST_NOREWARD` (23) to take a reward with no choice.**
  The templates accept 29, 1002 and 20000 alike; for a reward without selectable items, 8 (`SELECTED_QUEST_REWARD1`) also finishes but logs a
  WARN ("SelectableRewardItem list has no element on index 0", QuestService.java:182) — the gate uses 23. Checklist step 10 settles it.
- That the level-up during 1102's reward reaches no unported body besides those §5 names (M5b-1's real-client session reached level 2 by
  hunting with 0 ERROR lines, which covers `onLevelChange`'s non-quest half).
- That M5c will port the dialog plumbing (D4) and M5b-3 the quest-drop family with h03 (D11). **Both drafts now exist** (m5c-plan.md,
  m5b3-plan.md, rev 1, untracked and unreviewed) and say so; I-03 remains the final check. **Refresh: D11 is measured now** (ported,
  `5fbb03a08`). D4 is written in the working tree, uncommitted and unbuilt, so it stays inferred until M5c commits.
- The effort letters and durations (compared with M5b-1 and M5b-2 lanes by body count and Java lines), and the 1a/1b body split (method
  counts per class, rounded).
- That `SM_INVENTORY_UPDATE_ITEM` is the only packet the kinah reward sends (ItemPacketService.java:191-203 for `CUBE`) — its body is not
  decoded by the gate, which checks the amount after relog instead. (Refresh: M5b-3's `ItemDecoders` can decode it now; Y6.)
- That the 498 "never spawned" start npcs are spawned by phase-6 handlers or events and not by something M5d ports.
- That the fake client's fight helper kills only the target it attacks, so C12 can stop at exactly three credited kills (Y11's level-up
  placement depends on it; the oracle's placement check turns a deviation into a loud failure).

**Claims of the task statement and the roadmap checked:** the 8,043 / 4,184 / 1,035 counts — **confirmed exactly**; "need only the P5-06
engine" — **not true** (§2.4, §3.5, §8.1); "163 bodies" — confirmed as sites, **268 as bodies** (§4.2); "the quest markers
SM_NPC_INFO/SM_QUEST_LIST" — **the marker packet is `SM_NEARBY_QUESTS`** (§3.1); "the P4-08 lease still covers questEngine/** shells" — confirmed,
74 files plus `tests/quest` (§4.4).

---

## 15. Open questions this analysis could not settle without building

1. **The client's dialog action ids** for accept, for a no-choice reward and for the journal's "Report" (§14 first inferred item; checklist
   step 10).
2. **What M5c and M5b-3 will actually deliver** (§8.1) — their drafts say the dialog plumbing (M5c stage 0) and the quest drops with h03
   (M5b-3 L-04); I-03 answers it before stage 1. (Refresh: M5b-3 is answered, all delivered. M5c's stage 0 is in the working tree, and
   D18 is new.)
3. **Whether the 28 `catch (const std::exception&)` blocks in `QuestEngine.cpp` should log the `UnportedException` differently** from a
   Java exception. A faithful port logs both as ERROR, which is what Y14 relies on; decide with the P5-06 deviations owner before the join.
4. **Whether `SM_NEARBY_QUESTS` alone makes the real client draw markers**, or whether it also needs something from the client's own quest data
   that the XML-only registry (D9) changes. Checklist step 1 shows it.
5. **Whether a neighbouring kerub's aggro forces an unscripted kill** before three are credited (respawn spots, `srange` 7). Rev 2 no
   longer tolerates extras (Y10/Y11): the oracle's placement check fails loudly, and the run decides whether the approach path needs moving.
6. **Whether activating 870 startup `simple_abyssguard` npcs (D5) changes any M5a/M5b/M5b2 hit count** that a gate pins (for example a §A
   row hit "at least once" that npc-vs-npc fights now hit much more often, or a §B row they now hit). None is in the Elyos start region;
   Ishalgen has 10 spots. (Refresh: the M5b-3 gate plays Poeta too, so the same holds for it. M5c's gate adds Sanctum.)
7. **Whether gathering belongs to M5c** (D13) — the user's decision. (Refresh: m5c-plan.md D10 asks the same, and both stay open.)
8. **Whether the quest stress run (G-06) is wanted** (D16) — the user's decision: capacity tests are designed with the user, and it adds
   machine load.

---

## 16. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 3 high, 7 medium and 10 low findings, and confirmed the rest of rev 1's
measurements (the 8,043 / 4,184 / 1,035 counts, the 15 kinds, the site counts, the C++ and Java citations, the gate data and rewards). Every
finding was re-checked against the two trees before this revision. All twenty were confirmed and applied; for one (5) rev 2 takes a different
fix than the one proposed, and for one (4) rev 2's re-derived numbers differ slightly from the review's (both explained below).

| # | Finding (severity) | What changed in rev 2 |
|---|---|---|
| 1 | Y7 cannot fail for the guard mutations it claimed (high) | Y7 restated as `ReportTo`'s status dispatch plus the next-page window `(mires, 23, 1101)`; new case C10b / **Y7b**: the journal `(0, 108, 1102)` on a START quest must produce nothing (it kills `finishQuest`'s guard — without it the quest is silently completed with an empty reward); the `sendQuestEndDialog` guard moved to **H-07**; §10.4 rows updated. |
| 2 | An Asmodian cannot be created on account A (high) | **D15**: the Asmodian plays on a second account B (M5a's two-account setup); `creation.mode` stays 0; §10.1, C16 and checklist step 9 updated. |
| 3 | `finishQuest` reward bodies outside M5b-3 have no owner (high) | New hole 5 in §1, §3.5 row 16 rewritten with owners and citations, **E-09 / D14**: M5d ports `BonusService` (P5-09 lease), `addAp`/`onRankChanged`, `addGp` (P5-08), `CubeExpandService` (P5-07 lease) — ~10 bodies. Chosen over "pick clean T-04 quests only" because left unported, AP and GP quests re-pay kinah and exp on every retry (QuestService.java:222-239 before :108). §2.4 shows the completable counts with and without them (901 / 795); T-04 tests `WorkOrders`, `RelicRewards`, `KillInZone` and `FountainRewards` with their rewards; §8.1 names what stays unported (`Legion::addContributionPoints`, `ChallengeTaskService`, `WarehouseService::expand`). |
| 4 | §2.4 counted givers nothing spawns at startup (medium) | §2.4 re-derived over startup spawns only (direct regular spawns on world maps, no `RIFT`), which moves 310 quests to "service-spawned, M5i/M5f". Rev 2 also applies Java's optional/mandatory `<start_conditions>` rule (rev 1 read only the first group). Result: **2,511** reachable (2,072 + 439), 901 dialog-and-kill (795 without E-09). The review's 2,495 (2,059 + 436), ~892 and ~786 differ only by the precondition rule: the review counted every `finished` in every group as required; Java requires one optional group (QuestTemplate.java:170-187). |
| 5 | Y11's level-up depends on the kill count (medium) | C12/C13: exactly three credited kills, report at once; Y11 asserts the level-up place the oracle's new `--exp-path` computes (130 + 80k < 400 ≤ 310 + 80k; refresh: `m5d-quest --exp`, see C0). **Fix taken differently:** the "further kill" check did not move after C13 — after COMPLETE a kill tests only `status == START` (MonsterHunt.java:177), not the end var — it moved to the Asmodian's **fifth** sprigg-worker kill in Y13. |
| 6 | Y13 assumed a 2101 → 2102 follow-up (medium) | D6 and Y13 rewritten: 2101's reward ends on `SM_DIALOG_WINDOW(vandar, 10, 0)`, 2102 is in the enter-world set, 2102's reward opens 2103; G-01's `--quest 2101` answers "no follow-up"; a new mutation row (follow-up ignoring preconditions). |
| 7 | The §12 fallback split and D3's join list missed hooks every run reaches (medium) | §5 now defines the **join minimum** for all 16 templates (constructor, `register()`, `onEnterWorldEvent`, `onLevelChangedEvent`, `onKillEvent`, every `onDialogEvent`, `onUseSkillEvent`, `onEnterZoneEvent`, and — added in this revision — `onItemUseEvent` and `onCanAct`); two new §5 rows (item use, quest object); D10 gains an `XmlQuestData::register_` partial if 1127 slips; §12's split by template kinds replaced (see 9). |
| 8 | The frozen `getQuestDrop` signature cannot add drops (medium) | §9 names the dependency on **m5b3-h03**; D11, E-04 and §3.4 updated. |
| 9 | Risk 5 and H-07 pinned the wrong collection (medium) | Risk 5 restated: registration order is visible in `onTalkEvent`, `onKillEvent` and `questOnEnterWorld`; `onQuestStart` is a `HashSet`. The order test moved to **T-04** (mires' `onTalkEvent` `[1101, 1102, 1103, 1104]`, `questOnEnterWorld`), against G-01's new `--registration-order`. |
| 10 | Stage-1 duration contradicted the effort letters; a lane crossed into another's chunk (medium) | Stage 1 split into **1a** (engine, base, five kinds, dialog-and-rewards, AIs, harness; `:111` stays partial) and **1b** (the other eleven templates, xmlQuest, T-04's smoke, quest objects, then the join): 10-14 agent-days; the milestone is 15-20 (3-4 weeks). T-04 stays in the xml-templates lane throughout. |
| 11 | The level-change hook runs at every first enter world (low) | §5 row, Summary correction 3, D3 and §10.6 corrected; Y1 asserts both enter-world `SM_NEARBY_QUESTS`. |
| 12 | "10 ERROR lines per enter world" is 1 (low) | Corrected in §5 and D3. |
| 13 | Packet order in Y5 and Y6 (low) | Both written as ordered patterns in Java's order (`SM_NEARBY_QUESTS` after the REWARD update; kinah before exp; UPDATE and `SM_NEARBY_QUESTS` before the follow-up window); Y11 and Y13 likewise. |
| 14 | Start-map list corrections (low) | 2107 unstartable in Java too (46 playable, not 47); 2109 needs M5b-3; 1127's work item; 16 quests at 9 `simple_abyssguard` givers (14 without the gathering quests); "526 start npcs" → quests (now 476 under rev 2's scope); citations `210010000_Poeta.xml:812-815` and `npc_templates.xml:1852,1972`. |
| 15 | Small count errors (low) | `task/` 17 bodies (invisible total 105, chunk 268); 188 Java `CM_*`; 8,066 chunk lines; `QuestItemNpcAI` 4 bodies. Also found in this revision: `task/` is 227 lines, not 228. |
| 16 | M5c and M5b-3 drafts exist (low) | D4, D11, E-04, §3.2-§3.3, §3.8, §8.1 and the D-items reconciled with m5c-plan.md stage 0 and m5b3-plan.md L-04; D-01/D-02 port/D-04 become I-03's fallback. The M5c allow-list's `:111` row (m5c-plan.md:559) joins the join commit's list. |
| 17 | Lane dependencies not ordered (low) | §8.2 merge orders: A-02 after D-04; A-03 after A-02, H-02, T-01a and M5b-3's drops; E-04 after L-04. |
| 18 | Allow-list rows pinned by line number (low) | I-05 and §5 re-pin the `:115` rows in every list. |
| 19 | Decisions that belong to the user (low) | D5 recorded as taken by the integrator (to be named in the next progress update, with the 870 / 2,720 startup spots); G-06 offered to the user (D16, §15 Q8). |
| 20 | The P5-06 acceptance row is only partly met (low) | D8 records the analyzer column as a deviation from handlers-and-porting-plan.md:647 until E-08; I-05 writes it into `docs/deviations/P5-06.md`. |

**Found while revising, not in the review:** the AP/GP re-payment on retry (row 3); `CHALLENGE_TASK` quests reaching the unported
`ChallengeTaskService` (none reachable, §2.4); 1,115 of the 4,184 XML quests carry an E-09 reward; the M5c allow-list also holds `:111`.

---

## 17. Refresh, 2026-09-24

Rev 2 was written at `c1edb0afb`. Since then came M5b-2 stage 2 (`50a9158bf`), the "oracles ahead" commit (`83db3742e`), M5b-3 stages 0
and 1 (`4867fbc44`) and M5b-3's gate (`5fbb03a08`, HEAD). This refresh re-measured the plan against HEAD and against m5c-plan.md as
refreshed and reviewed (§15-§16 there), so that M5d can branch once M5c commits. It was **read-only**. Nothing was built or run except
Python:

- `census.py --chunks P5-06,P5-08,P5-15,P5-16,P5-05,P5-09,P5-07,P5-10,P5-11,A1,P4-08 --json --markdown` into the session scratchpad. The
  working tree's manifest has already split P5-09 and lists P5-09a/b/c; P4-08 is a lease without files of its own.
- `grep -c 'AION_UNPORTED('` per file.
- Oracle queries: `m5d-quests` for Poeta and Ishalgen at level 1; `m5d-quest` for 1101, 1102, 1103, 2101 and 2102, and for 1102 with
  `--exp 290` and `--exp 370`; `m5c-craft --skill 30001` on both maps; `m5a-creation --race ASMODIANS --class WARRIOR`;
  `m5b3-item --item 169300002`; `m5b3-drops --survey` on both maps.
- `python -m unittest tests.test_m5d` (38 tests, OK). The whole oracle suite was not run, because another workflow is editing
  `tools/oracle` (m5b3-plan.md §19.6).
- A scratch script (`scratchpad/tierA2/m5d/items_quest2.py`) that lists the `<queststart>`/`<read>` items, the item ids the templates
  register, and which of those drop on the start maps, using the M5b-3 drop oracle's droppable set.

The Java tree's `src/` and `data/` are unchanged since `c1edb0afb` (`git log`: only `config/*.properties.example`), so §2's data stands.
**P5-06 has no uncommitted file.** The working tree does hold M5c's stage 0, M5c's I-01 manifest split, the npc-leak lane's edits and
the untracked questgen prototype. Numbers marked "working tree" come from it.

### 17.1 Counts that moved

| What | Rev 2 (`c1edb0afb`) | HEAD `5fbb03a08` (working tree where marked) | Why |
|---|---|---|---|
| P5-06 `AION_UNPORTED` sites | 163 + 2 partial | **159 + 2** | M5b-3's L-04 ported `getQuestDrop`, `isQuestDrop`, `allowLooting`, `regQuestDropItem` (`QuestService.cpp` 26 → 22) |
| P5-06 bodies no site sees | 105 | **105**; census 109 | census also counts `ConditionOperation`/`ConditionUnionType` `value`/`fromValue`, which `EnumTraits` covers (§4.2) |
| P5-06 total | 268 bodies | **264**; census **269 open** (159 + 1 partial body + 109), 3,517 open Java lines | the four quest-drop bodies |
| P5-06a sites (D1) | 44 | **40** | same |
| `QuestService` bodies ported | 10 of 36 | **14 of 36** | same |
| C++ `CM_*` files | 42 of 188 | **51**; **55** in the working tree | M5b-3's nine; M5c's `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `CM_QUESTION_RESPONSE` (uncommitted) |
| `DialogService` sites | 7 | 7; **1** in the working tree (`DialogService.cpp:314`) | M5c's D-02 (uncommitted); the one left is `MATCH_MAKER` with autogroup on (m5c W-31), off in every gate profile |
| `ActionItemNpcAI` bodies | ~7 | **6** (census) | census count |
| Quest-item actions | not in the plan | **8 bodies, 155 Java lines** (E-10) | m5c-plan.md §3a and m5b3-plan.md O-03 give them to M5d |
| Milestone | ~318 bodies, ~9,320 Java lines | **~322, ~9,340**; ~307 once M5c's stage 0 commits; ~304 with M5c's P-05 | −4 quest drops, +8 E-10, −15 dialog plumbing, −3 cube |
| Stage 1a bodies (§12) | ~205 | **~213** | E-10 |
| G-01 | M, all to write | **S**: `--registration-order` and `--census` only | `oracle.py m5d-quest` / `m5d-quests` (`83db3742e`) answer the rest; `--exp` replaces `--exp-path` |
| Allow-list rows of the join | m5a `:18`/`:19`, m5b `:32`/`:71` | m5a `:18`/`:19`, m5b **`:35`/`:63`**, m5b2 `:25`/`:47`, m5b3 `:24`/`:38` | the lists were edited, and M5b-3's is new |
| Scenario gates before M5d | 6 (+ M5b-3's, M5c's to come) | **8 registered** (+ M5c's 1 to come, no geo variant) | M5b-3's gate committed |
| `ctest -L scenario` | – | **~38-43 min** with M5d, serial (single lock); review: **~28-35 min** under `ctest -j 2` if the two gate slots commit (larger slot 1,472-1,722 s) | 1,746 s today (m5b3-plan.md §19.3) + M5c 120-200 s + M5d's two runs (risk 13, which has both figures) |
| Phase 5 by census | – | 1,643 `AION_UNPORTED` sites, 13 partial sites, 1,451 undeclared; census **Open 3,125** (working tree). Review correction: Open is not their sum (that is 3,107); it counts bodies, partial bodies rather than sites, plus open enum-constant bodies and initializers | includes M5c's uncommitted stage 0; m5c-plan.md §15.1 measured 3,148 at `4867fbc44` (the same kind of figure) |
| Client packets / root AIs without C++ (census) | 148 of 188 / 40 of 43 (roadmap) | 134 of 190 / 39 of 43 (working tree) | M5b-3's nine packets, M5c's four and `PostboxAI` |

### 17.2 The assumptions, checked

| Assumption | From | Status | Evidence, and what it changes here |
|---|---|---|---|
| `ItemPacketService::sendItemPacket` (every kinah reward) | M5b-3 | **met** | 9 of 9 ported (m5c-plan.md §0 A-01). §3.5's hard dependency is gone |
| `ItemService::addItem` (reward, selectable and work items) | M5b-3 | **met** | 10 of 10 ported. H-03's `giveQuestItem` stands on ported code |
| Loot + L-04 + m5b3-h03 (D11) | M5b-3 | **met** | `QuestService.cpp:368-519`, `QuestService.h:81`, `tests/quest/QuestDropTest.cpp`; deviations in `docs/deviations/P5-06.md`. **E-04 shrinks to the two group variants** |
| The `QuestDrop` quest id | M5b-3 (`4867fbc44`) | **met, in a different place than Java** | `QuestsData::afterUnmarshal` sets it at load (`QuestsData.cpp:21-26`); `QuestEngine::init` only reads it (QuestEngine.java:89 sets it). Without it every kill of an npc with a quest drop threw (P5-06.md). For M5d: E-01's `isQuestDrop` case and any future `QuestEngine::reload` (D8) must rely on the holder, not on `init` |
| `CM_USE_ITEM` | M5b-3 | **met** | `CM_USE_ITEM.cpp:71-152`; the item-use wake-up is live now for the 6 registered start items without `<queststart>`; the other 33 wait for E-10 (N1, review correction) |
| `QuestStartAction` | M5b-3 (rev 2's §8.1) | **not met; back to M5d** | the `m5b3-h01` stubs throw; E-10, D19 |
| M5b-3's drops for A-03 | M5b-3 | **met** | `AIActions.cpp:111-113`, `DropService.cpp:162` |
| M5b-3's gate and allow-list | M5b-3 | **met** | `gs.scenario.m5b3`/`_geo` registered; `:111` in §A, `:115` in §C of `m5b3_partial_allowlist.txt` |
| The M5b-2 profile M5d inherits | M5b-2/M5b-3 | **changed** | `gameserver.rates.drop = 0` since `4867fbc44` (§10.1) |
| Decoders and an inventory model | M5b-3 | **available** | `ItemDecoders`, `InventoryModel` (file-local); Y6 decodes the kinah, Y13 the stack |
| npc casts, `removeHideEffects`, `onUseSkill` | M5b-2 | **met** | `50a9158bf` |
| The dialog plumbing (D4) | M5c stage 0 | **written, uncommitted, unbuilt** | the working tree's D-01..D-05 of M5c; I-03 at branch time |
| `CM_QUESTION_RESPONSE` | M5c | **written, uncommitted** | as above (W for M5d) |
| `CubeExpandService` | M5c P-05 | **required in M5c rev 2** (was "optional") | E-09's cube part drops once M5c lands |
| `BonusService`, `AbyssPointsService`, `GloryPointsService` | – | **still M5d's** | M5c's §2.8 leaves them out; its D2 sends AP vendors later. `BonusService` is P5-09a after the split |
| Dialog builders, `SM_DIALOG_WINDOW` decoder, `InventoryModel` lift | M5c G-02 | **planned, not in the tree** | G-02 shrinks if they land first |
| Gathering | M5c D10 | **user, open** | same question as D13 |
| Nothing flows from M5d back into M5c | – | **wrong** | M5c's C19 needs `QuestState::setPersistentState` and `canRepeat` (N4, D18) |
| M5e's use of M5d | m5e-plan.md A-04a-c | **consistent** | `QuestState` setters, the handler helpers, the registration, and decoders for `SM_DIALOG_WINDOW`, `SM_QUEST_ACTION`, `SM_STATUPDATE_EXP`; M5e also routes `CM_PLAY_MOVIE_END` (W here, D-05) |
| `AbyssGuardSimpleAI`'s home | m5b3-plan.md O-05 | **conflicts with D5** | O-05 says M5j, D5 says M5d; the integrator aligns O-05 and m5c W-15 |

### 17.3 Reachability at HEAD — what is new

- **N1 — item use is live, but most quest start items bypass it (corrected after review).** `CM_USE_ITEM.cpp:109-112` calls
  `QuestEngine::onItemUseEvent` for every item without a `QuestStartAction` (CM_USE_ITEM.java:89-90). Today the registry is empty. After
  the join the templates register 39 ids: the 32 `item_order` first work items (ItemOrders.java:41, 47) and 7 `report_to_many` start
  items (ReportToMany.java:55). **Only 6 of them route through `CM_USE_ITEM`**: `item_order` 1323, 16904, 26904, 30007, 30107 and
  `report_to_many` 4914. The other **33** (27 `item_order`, 6 `report_to_many`) carry `<queststart>` for the same quest, so
  `CM_USE_ITEM` skips them, and they reach `onItemUseEvent` only from `QuestStartAction.finishUse` after the cast
  (QuestStartAction.java:82-87), which is E-10. Six more XML quests (5 `report_to`, 1 `item_collecting`) have a `<queststart>` item that
  is not registered; `finishUse` sends them to `onDialog(ASK_QUEST_ACCEPT)`. So **E-10 is the start path of 39 XML quests** (D19), and
  the only one for 38 of them: 11216 (`item_collecting`) also has start npc 799017. §2.4's counts do not change: the other 38 have no start
  npc in their template (§2.4's "no start npc" row, 415), and 11216 is counted by its npc. None of the 39 registered items drops on
  the start maps, and none is an item an earlier gate uses, so the join changes no earlier gate through this path (§5). Measured with
  scratch scripts `tierA2/m5d_review/items_check.py` and `tierA2/m5d_rev/start_items.py`.
- **N2 — `<queststart>` and `<read>` items throw today.** Poeta drops 182200214 *Namus's Diary* (quest 1114) and 182200501 *Broken Axe
  Handle* (1107). Ishalgen drops 182203120 *Silver Necklace* (2122), 182203130 *Old Scroll* (2136; also `<read>`) and 182203116 *Eyvindr
  Logbook* (`<read>`). All four quests are Java-handled. `CM_USE_ITEM` reaches the `m5b3-h01` stubs, which log an ERROR. E-10 ports them,
  and after E-10 these items only print the "used" message, because D9 means no handler answers. Of the 157 `<queststart>` items, 39
  start an XML quest, 78 a Java quest and 40 a quest with no handler. (Review: for those 39, E-10 is more than a stub fix; it is their
  start path, N1.)
- **N3 — quest drops meet quest state at the join.** After the join a character can hold a START `item_collecting` quest. Then
  `isQuestDrop` reads `getQuestVarById(0)` for a drop with a collecting step (`QuestService.cpp:472-478`). E-01 is in the join, so this
  body is ported by then. The unit case that pins today's throw (`QuestDropTest.cpp:347`) must be rewritten by E-01.
- **N4 — M5c's C19 runs into `QuestState`.** A Daeva seeded with (1006, COMPLETE) is loaded through `PlayerQuestListDAO::load` at login
  and again in `PlayerCommonData::updateDaeva` (`PlayerCommonData.cpp:190, 265-289`; PlayerCommonData.java:276, 588-610). Both loads
  reach `QuestState::setPersistentState` (`PlayerQuestListDAO.cpp:70`, unported). The DAO catches it: two ERROR lines, an empty list, no
  Daeva, level 9. With `setPersistentState` ported, the enter world's `SM_QUEST_COMPLETED_LIST` calls `canRepeat`
  (`SM_QUEST_COMPLETED_LIST.cpp:29`), and the logout's `store` calls `setPersistentState(UPDATED)` on every state
  (`PlayerQuestListDAO.cpp:103-105`). m5c-plan.md does not know this. D18. (Review: whoever ports `setPersistentState` also turns
  `tests/player/PlayerModelBodiesTest.cpp:441`, chunk P4-12, red and must rewrite it; D18, E-01, risk 11.)
- **N5 — an M5c unit case pins M5d's throw.** In the working tree, `tests/cm_ak/DialogSelectPacketsTest.cpp:450-457` expects two
  `UnportedException`s from `QuestService::finishQuest` through `CM_DIALOG_SELECT`'s journal branch. E-02 makes it red, and D-02's quest-arm
  tests replace it in the same integration part. The neighbouring case (`:459-464`) stays green, because its fixture registers no quest.
- **N6 — the talk path with M5c's stage 0.** `CM_SHOW_DIALOG` → `onDialogRequest` → `GeneralNpcAI` → `TalkEventHandler` →
  `QuestEngine::onDialog` (empty registry) → `getStartPageId` → `isInteractionAllowed` (ported in the working tree) → the page. No quest
  body is reached before the join. Afterwards the path is §5's Talk row. A functional npc still answers page 10 either way
  (DialogPage.java:119-121), so M5c's gate pages do not move.
- **N7 — every kill in an M5d-profile run sends `SM_LOOT_STATUS(LOOT_ENABLE)`** even with drops at 0 (DropRegistrationService.java:104-106;
  M5b's R3). Y8 and Y10 allow it (§10.1).
- **N8 — the Asmodian starter stack.** The new Asmodian Warrior holds 20 × 169300002, so 2102's reward updates that stack to 30 (Y13
  corrected).
- **N9 — nothing unported behind the use-item AIs on a world map.** `AIActions::handleUseItemFinish` → `GeneralInstanceHandler::
  handleUseItemFinish` is an empty inline (`GeneralInstanceHandler.h:144`).
- **N10 — the bonus reward cannot be owned.** `BonusService.getQuestBonus` returns `new QuestItems(...)` (BonusService.java:36), and the
  C++ list is `std::vector<const QuestItems*>` (`QuestService.h:39-40`). The prototype found it through the event handlers, and HEAD
  confirms it on the XML path: 760 XML quests have a `<bonus>`, all 574 work orders among them. D17(b), `m5d-h01`.

### 17.4 The quest transliterator prototype (phase6-questgen-prototype.md rev 2)

What it says about the API M5d must expose:

- **Stage 1a is also phase 6's API.** 910 of the 1,035 Java handlers transliterate. At most 841 of them call nothing unported outside
  P5-06. Their most-called unported bodies are `sendQuestDialog` (900 files), `sendQuestEndDialog` (870), `QuestState::isStartable` (740),
  `sendQuestStartDialog` (714), `getQuestVarById` (611), `QuestEnv::getTargetId` (589), `updateQuestStatus` (443), `setStatus` (426) and
  `defaultCloseDialog` (334). All of them are E-01, H-01 or H-02.
- **The declared headers are the contract.** The generator types every call against `AbstractQuestHandler.h`, `QuestState.h`,
  `QuestEnv.h`, `QuestService.h` and the hooks' signatures. M5d keeps them as declared, with the two exceptions of D17. The varargs and
  `workItems` gaps become emitter rules, not header requests (prototype §5.2 items 3-4), so §9's "none expected" holds for those.
- **Two header decisions are M5d's** (prototype §11, "M5d owner decisions"): the H-06 spelling, and the reward list as values. D17
  proposes both, and E-02/E-09 need the second anyway (N10).
- **Rows mapped to M5d items:** B02 spawn helpers (32 files) → H-05; B04 timers (16) → E-05, now W for M5d's gate and P for phase 6;
  B03 follow helpers (14) → E-07 (stage 3); B05 `startEventQuest` (3) → E-02; the `fromBoolean` companion (60) → H-06.
- **"Generate now, link after the D3 join"** (prototype §6; phase6-inventory.md) agrees with D3. Generated quests stay out of the gate
  builds until the join.
- **Golden traces run through a harness after M5d** (prototype §8.3). T-04's in-process fixture should be reusable for that.
- **Line numbers of this file cited elsewhere moved.** The prototype cites `m5d-plan.md:31-37`, `:541`, `:556` and `:685-689`, and
  phase6-inventory.md also cites `:115-120`, `:505`, `:506` and `:592`. Those lines are now correction 3, E-02, H-06, the §9 rows, §2.3,
  D4, D5 and A-03 respectively. Cite the ids.

### 17.5 Decisions

- **D4** refreshed: M5c's plumbing is in the working tree; the fallback is expected to stay unused; I-03 at branch.
- **D5** unchanged. m5b3-plan.md O-05 (→ M5j) and m5c-plan.md W-15 (→ "M5d/M5j") should point here; that is the integrator's edit.
- **D11** closed as met; E-04 shrinks.
- **D13 — user, open** (the same question as m5c-plan.md D10). Only the facts moved (§2.5 note).
- **D14** refreshed: `BonusService` is P5-09a; the cube part is M5c's required P-05.
- **D16 — user, open.**
- **D17 (new, proposed for the integrator at I-02):** H-06's spelling and the value-typed reward list (`m5d-h01`).
- **D18 (new, open for the integrator):** M5c's C19 needs `setPersistentState` and `canRepeat`. Recommended: M5c ports them under a
  P5-06 file lease before its stage 3, plus (review) a P4-12 test-file lease to rewrite `PlayerModelBodiesTest.cpp:441`.
- **D19 (new, taken):** M5d ports `QuestStartAction` and `ReadAction` (E-10), the home m5c-plan.md §3a and m5b3-plan.md O-03 already name.
  Review: E-10 is the start path of 39 XML quests (the only one for 38), not just a stub fix (N1).
- **D12** (review, conditional): under the two gate slots, M5d's pair holds one slot (§10.1), and the budget is the larger slot (risk 13).

### 17.6 Work items, lanes and effort

- **New:** E-10 (8 bodies, dialog-and-rewards lane, 1a, a P5-07 file lease).
- **Shrunk:** E-04 (the two group variants; may move into 1a); G-01 (S); G-02 (if M5c's G-02 lands first); D-01, D-02's port and D-04
  (fallbacks, expected unused).
- **Grown:** E-01 (rewrite `QuestDropTest.cpp:347` and, review, `PlayerModelBodiesTest.cpp:441` under a P4-12 test-file lease; close the
  P5-06.md row; un-skip `PlayerDaoTest.QuestStateListLoadAndStore`); E-02 (D17b; merges with D-02's replacement of M5c's case);
  E-09 (P5-09a lease; `std::optional<QuestItems>`); E-10 (review: split "cannot start" case, a registered-start-item case, after E-01);
  T-04 (a reusable fixture; review: item-started cases through E-10); I-03, I-04 and I-05 (the details above; review: the P4-12 lease and
  the two gate-budget figures).
- **Changed need:** E-05 is W for the gate and P for phase 6.
- **Lanes:** the six stage-1a lanes are unchanged in name and chunk. The dialog-and-rewards lane gains E-10 and its lease. The merge
  order gains three rules (§8.2): D17's batch before E-02 and E-09; E-02 together with D-02's tests; E-10 any time after its lease
  (review correction: and after E-01, for `isStartable`).
- **Effort:** the letters are kept as size. By M5b-3's measured pace (about 14 h from its first stage to its gate) they overstate wall
  time.

### 17.7 Citations re-checked

**Moved and corrected in place:**

- `Skill.cpp:753` → `:751`.
- `QuestService.h:77` → `:81` (h03 applied). `QuestService.cpp`'s `finishQuest` is at `:129`.
- `decoders/PacketDecoders.h:494,497` → `:500, :503`; `M5bScenarioTest.cpp:204-219` → `:227-240`.
- `handlers/ai/GeneralNpcAI.cpp:42-48` → `handlers/aion/gameserver/handlers/ai/GeneralNpcAI.cpp:46-47`.
- `m5b_partial_allowlist.txt:32/:71` → `:35/:63`.
- `chunks.cmake:527-531` (A1) → `:543-547` in the working tree.
- `AIEngine.cpp:156-170`: the DummyAI fallback is at `:158-168` (`DummyNpcAI` class at `:54`).
- m5c-plan.md line numbers (`:227-228`, `:258`, `:335`, `:387`, `:559`) are replaced by M5c's item ids where this refresh touched them,
  because that file is being edited.
- T-04's row was split over four physical lines, which ended its table in rendered Markdown. It is one line now; the text is unchanged.

**Re-checked and still right:**

- C++: `QuestEngine.cpp` (950 lines; `:111`, `:115`, `:121`; `:341-350`; 28 `catch (const std::exception&` blocks),
  `NpcController.cpp:262-265, 284-306`, `TalkEventHandler.cpp:34`, `CM_LEVEL_READY.cpp:108, 113`, `PlayerController.cpp:263-270, 298, 312,
  423, 693`, `Storage.cpp:188, 235`, `WorldMapInstance.cpp:77-90`, `DialogPageInfo.cpp:17-29`, `XMLQuests.cpp:12-18`,
  `HandlerRegistry.h:127-133`, `PlayerQuestListDAO.cpp:70, 119, 144`, `PlayerEnterWorldService.cpp:413, 460`,
  `SM_QUEST_COMPLETED_LIST.cpp:29`, `SM_QUEST_LIST.cpp:24`, `PlayerCommonData.cpp:101, 224`, `TitleList.cpp:61`, `NpcFactions.cpp:245`,
  `BonusService.cpp:7-17`, `AbyssPointsService.cpp:11-25`, `GloryPointsService.cpp:7-9`, `CubeExpandService.cpp:11-37`,
  `WarehouseService.cpp:24-42`, `ChallengeTaskService.cpp:30-32`, `Legion.cpp:91`, `M5aScenarioTest.cpp:1044, 1368-1375`,
  `chunks.cmake:153-164, 300-304`, handlers-and-porting-plan.md:647.
- Java: CM_DIALOG_SELECT.java:66-68, 75-107; QuestService.java:77-117, 197-244, 400-447, 666-796, 849-885, 903-933; DialogPage.java:113-125.

### 17.8 Left open for the user

D13 (gathering, the same question as m5c-plan.md D10) and D16 (the quest stress run). D17 and D18 are the integrator's, and D5 was taken
by the integrator (still to be named in a progress update, if it has not been).

### 17.9 What can run in parallel

**Now, with no build, on files no running workflow edits.** The running workflows hold M5c's stage 0 in the tree (review: including its
uncommitted section of `docs/porting/header-requests.md`), `tests/scenario/**` (review: including the two-gate-slot rewrite of
`ScenarioTests.cmake` and `StressTests.cmake`), the other lane's `tools/oracle` edits and the npc-leak lane.

1. G-01's remainder, `--registration-order` and `--census`, in `tools/oracle/m5d/` with tests. Wiring any new flag into `oracle.py` waits
   until the other workflow's oracle edits are committed.
2. The D17 rows (H-06's spelling, `m5d-h01`) drafted for the integrator's decision. Review correction: `header-requests.md` is not free
   now (M5c's stage-0 integrator lane has 32 uncommitted lines in it), so the draft stays in §9 (the `m5d-h01` and H-06 rows) or a scratch
   file, and is appended to `header-requests.md` only after M5c's stage 0 commits.
3. D18's decision, and if M5c takes it, the edit to m5c-plan.md's stage 3 by M5c's owner.
4. `game-server/config/m5d.properties.example` in the Java tree: the M5b-2 profile plus §10.1's keys.
5. Read-only traces: E-10's callees, and per template the `AbstractQuestHandler` helpers its join-minimum hooks call (to size T-01b).
6. The prototype's no-build lanes: questgen emitter rules, `tools/parity`, and writing the golden-trace oracle.
7. The user's D13 and D16, and the integrator's D5 edits to m5b3-plan.md O-05 and m5c-plan.md W-15.

**Once M5c has committed and builds are allowed** (the roadmap runs M5c first): I-01, I-02 (with D17) and I-04, then stage 1a's six lanes
in parallel: quest-engine, handler-base, xml-templates, dialog-and-rewards, quest-npc-ais and gate-harness. A-01, H-05 and H-06 depend
on nothing inside the milestone and start on day 0. Review correction, three that do depend: **E-09** after I-02's D17 batch
(`getQuestBonus` returns `std::optional<QuestItems>`); **E-10** after E-01 (`finishUse` calls `isStartable` for a held quest,
QuestStartAction.java:74-75), unless its tests use only null quest states; **G-02** after M5c's G-02 lands or is ruled out (its builders
and `SM_DIALOG_WINDOW` decoder overlap).

**Before M5c commits, only if the integrator accepts sharing the tree:** E-01's persistence subset (D18), which touches only
`QuestState.cpp` and M5c's lanes do not, plus (review) the rewrite of `tests/player/PlayerModelBodiesTest.cpp:441` under a P4-12
test-file lease, since the port turns that case red. Everything else in M5d either needs M5c's dialog code or a chunk M5c's lanes own (P5-07, P5-08,
P5-15, P5-16, P5-05).

**Not parallel:**

- E-01 merges first.
- The critical path is T-01a → T-01b → T-04's smoke → I-05, the join.
- E-02 and D-02's tests merge in the same part.
- (Review) E-09 and E-02 after I-02's D17 batch; E-10 after E-01; T-04's item-started cases after E-10.
- A-03 comes after A-02, H-02 and T-01a.
- Stage 2 (the gate and the regate) comes after the join.
- Stage 3 comes after the gate.

### 17.10 What the refresh did not do

It built and ran nothing but Python, and it edited only this file. It did not edit m5c-plan.md, m5b3-plan.md, header-requests.md or the
phase-6 documents. It did not re-derive §2.4, because the Java data is unchanged and G-01's `--census` will. It did not trace each
template's join-minimum callees body by body. It did not run the whole oracle suite (only `tests.test_m5d`). It did not check that M5c's
uncommitted code compiles.

### 17.11 Revision after the adversarial review (2026-09-25)

The review found three medium and five low defects in the refresh. All were re-checked against the tree and the Java, and all were
accepted. The corrections are made in place and marked "review":

1. **Item-started quests go through E-10** (§2.2 `item_order`, §3.4, §5 rows and hook table, D19, E-10, T-04, N1, N2, §17.2). Of the 39
   ids registered with `registerQuestItem`, 33 also carry `<queststart>` for the same quest, and `CM_USE_ITEM.cpp:108-112` skips
   `onItemUseEvent` for them (CM_USE_ITEM.java:89-90). Only 6 route directly. Re-measured with `tierA2/m5d_rev/start_items.py`: 27 of the
   32 `item_order` quests (1182 among them, T-04's example) and 6 of the 7 `report_to_many` start items go through
   `QuestStartAction.finishUse`. 38 of the 39 XML quests with a `<queststart>` item have no other start path; 11216 also has start npc
   799017 (`tierA2/m5d_rev/start_npcs.py`). That last point refines the review, which said all 39.
2. **A second committed test pins a throw.** `tests/player/PlayerModelBodiesTest.cpp:441` (P4-12) expects `deleteQuest` to throw through
   `setPersistentState` (E-01, D18, I-04, risk 11, N4). Also found: E-01 un-skips `PlayerDaoTest.QuestStateListLoadAndStore`.
3. **Two gate slots** in the working tree's `ScenarioTests.cmake` (D12, §10.1, §10.5, I-05, G-05, G-06, risk 13, §17.1). The plan keeps
   the single lock as the committed state, and adds the slot rule and the `-j 2` budget as conditional on that file committing.
4. `header-requests.md` is not free (§17.9 item 2). 5. E-09, E-10 and G-02 do have in-milestone dependencies (§8.2, §17.9). 6. E-10's
   "cannot start" test is split into the silent case and the restriction-message case (QuestStartAction.java:69-80). 7. The census total
   is no longer written as a sum (§17.1). 8. Y11 and the review-response row 5 point at `m5d-quest --exp` (C0).

Nothing was built or run except Python (the two scratch scripts above and the reviewer's `m5d_review/items_check.py`).
