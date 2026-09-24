# M5e work plan (training and progression)

> **Status:** plan **rev 2**, 2026-09-23, revised after an adversarial review; **§14 lists what the review found and what changed.** Rev 1 was
> a **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2: the cast engine and the effect core") plus the working tree, whose
> uncommitted M5b-2 part-3 lanes (P5-01, P5-03, P5-04: `AttackUtil.cpp`, `StatFunctions.cpp` and 36 `skillengine/effect/*` files) were the state
> this plan counts as "done" for the M5b-2 effect subset (assumption **A-01**). Those lanes have since landed as HEAD `760e8ab5c` ("M5b-2 stage 1
> part 3: the effect classes; passives and post-spawn skills go live", 22:58); rev 2 re-measured over that HEAD, whose effect files are clean,
> and the direct counts of rev 1 came out identical. **Nothing was compiled, built or run for this plan**; the oracle was not run either. C++
> statements come from reading both trees, from `game-server/chunks.cmake` and `tools/porting/chunks.py owner`, and from counting
> `AION_UNPORTED(` / `AION_PARTIAL(` call sites. Data statements come from throw-away parses of `data/static_data` (ElementTree, so XML comments
> are skipped as the server skips them) and of the Java sources through `tools/gen/javasrc.py`; the scripts are listed in §12. §12 separates
> what was **measured** from what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md). Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 5,
> [m5b3-plan.md](m5b3-plan.md) rev 2, [m5c-plan.md](m5c-plan.md) rev 2 (rev 1 when this plan's rev 1 was written), [m5d-plan.md](m5d-plan.md)
> rev 2 (all drafted today, none implemented),
> [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.10 (the phase-6 chunks), `generated/concurrency/cycles.toml`.
>
> **Four corrections to the roadmap row, in order of how much they change the milestone.**
>
> 1. **Aion 4.8 has no skill trainer.** No dialog action teaches a class skill: the only npc-taught skills are the **profession skills** of the
>    crafting and gathering masters (`GATHER_SKILL_LEVELUP` 45, `COMBINE_SKILL_LEVELUP` 46, DialogAction.java:60-61 →
>    `CraftSkillUpdateService.learnSkill`, DialogService.java:199-202, from level 10 for kinah, CraftSkillUpdateService.java:83-114), which are
>    **M5c's** (m5c-plan.md C-01); no client packet teaches one (none of the 188 Java `CM_*` classes; m5b2-plan.md §2.7 already noted
>    "`CM_LEARN_SKILL` … no Java class in 4.8"), and no npc on either start map has a skill function dialog (§2.2). **3,525 of the 4,693 `skill_tree.xml` rows are
>    `autolearn="true"`** and are learned by `SkillLearnService.learnNewSkills` on every level change, which **is already ported and already
>    runs**. What the start maps call trainers are the six **class masters** per race (kunandes, gilden, esera, kagas, lysander, cecilia in
>    Poeta), whose only job is the level-2/3 tutorial quest **"A New Skill" (1205 / 2132)** — two 135-line Java handlers (phase 6). §2.2.
> 2. **The class change is a Java mission quest that needs M5f.** The retail route is quest **1006 / 2008 "Ascension"** (`_1006Ascension.java`,
>    `_2008Ascension.java`, phase-6 chunk Q06), and it teleports into the solo instances Karamatis B (310020000) and Ataxiar B (320020000)
>    through `InstanceService.getNextAvailableInstance` and a flight teleport (`_1006Ascension.java:97-99, 157-175`) — the instance engine and
>    the instance teleport are M5f's (`InstanceService.cpp:81-95`, `TeleportService.cpp:251-271`, all `AION_UNPORTED`). **M5e cannot deliver
>    the retail class change before M5f.** It can deliver the Java server's own alternative, `gameserver.simple.secondclass.enable`
>    (CustomConfig.java:66-69: a class-selection window at login and a journal-dialog choice), and `ClassChangeService`, which both routes call
>    (**D1, the user's decision**).
> 3. **The class change is 7 bodies; what it wakes is ~205.** A Daeva of levels 10-20 uses **364 class skills, which launch 22 more through
>    their effects** (subeffects, provoked, delayed and signet skills), **needing 39 effect classes that are not ported** (88 sites; rev 1 said 34
>    and 71 because it did not follow the launched skills, §2.4), the monsters of the level 10-20 maps need 7 more, and **a level-15 Gladiator
>    cannot enter the world** until `CondSkillLauncherEffect` is ported: its passive 563 *Determination* is applied by
>    `activatePassiveSkillEffects` at enter world, which does not catch (§2.4). The milestone is **mainly P5-03/P5-04, not P5-08**: it takes
>    **25 of P5-08's 166 sites**.
> 4. **Dialog is M5c's** (agreed with m5c-plan.md §3); **summons belong here** (the Spiritmaster's level-10 kit is a summon); **stigma's
>    service belongs here but its gameplay gate does not** (the slot quests 1929 / 2900 are Java handlers in Sanctum and Pandaemonium, after
>    M5f).

---

## 0. What this plan assumes the earlier milestones deliver

M5e runs after M5b-2, M5b-3, M5c and M5d (roadmap rows 1-4). None of M5b-3, M5c, M5d is implemented; their plans are drafts of today. Every
work item, case and checklist step that stands on one of these rows carries its id, so the plan can be re-verified in one pass when M5e
branches.

| Id | Assumed delivered by | What | Where it is today | Items that depend on it | If it was not delivered |
|---|---|---|---|---|---|
| **A-01** | M5b-2 (all stages) | the 34 + 4 effect classes of m5b2-plan.md §2.4 and D13 ported, **including `StumbleEffect`** (the 8218 *Stumble* of 519's subeffect and of the greatsword critical proc, §2.4); the O-09 and D7 hold-backs removed (passive skills apply at enter world); `CM_CASTSPELL`, `CM_REMOVE_ALTERED_STATE`; the npc skill rotation (`SkillAttackManager`, `GeneralNpcAI::chooseSkillAttack`, m5b2 N-01/N-02); `tests/scenario/decoders/SkillDecoders`; `gs.scenario.m5b2` green | the 38 classes have **0 `AION_UNPORTED` at HEAD `760e8ab5c`** (part 3 committed; measured, §12); both packets exist; the rotation is stage 2 of M5b-2, not landed | E-01, E-02 (the "not ported" lists are computed against it), C-02, every gate case, X9g, X12 | the lists of §2.4 grow by whatever M5b-2 leaves; X12 has no npc cast |
| **A-02a** | M5b-3 (m5b3-plan.md D6, h01) | the item-action API: `canAct` / `act` declared on all 32 bound action classes **including `SkillLearnAction`** (as `AION_UNPORTED` stubs), `ItemActions::getItemActions`, `CM_USE_ITEM` | `SkillLearnAction.h` declares nothing (measured; `AbstractItemAction.h` has no virtual) | C-03, X16 | C-03 files the header request itself (§7) |
| **A-02b** | M5b-3 | `ItemService` / `ItemPacketService` (every inventory change sends through them) | 11 + 9 `AION_UNPORTED` (m5b3-plan.md §1) | the item-cost skills 246 / 249 (`ItemUseAction.java:30-49` → `decreaseByItemId`), C-03's book deletion, stage-3 stigma kinah | those skills and books throw; the checklist says so |
| **A-02c** | M5b-3 (m5b3-plan.md E-02 R, E-03 W; its D7) | E-02: `ProcAtkInstantEffect`, `PoisonEffect`, `SilenceEffect`, `BlindEffect` (+ its observer), `ParalyzeEffect`; E-03 (W, recommended): `DispelEffect`, `MpAttackInstantEffect` (and `FearEffect`, not on M5e's path) | 2 + 5 + 4 + 4 + 4 / 1 + 2 `AION_UNPORTED` | E-01, E-02, T-02, X12 | **E-01 / E-02 grow by 19 sites + 1 inner body** (§2.4 lists them apart; `ProcAtkInstantEffect` is reached by the Bard at 10 and the Chanter at 16 only through launched skills); **T-02 keeps `MpAttackInstantEffect`** (2) if M5b-3 left it W |
| **A-02d** | M5b-3 (T-06) | `CM_EQUIP_ITEM`, `StigmaService::notifyEquipAction` whole | no file / `StigmaService.cpp:30` | stage-3 stigma (T-01, X18) only — rev 2's weapon seeds (D8) equip offline and need no packet | stigma lane takes `notifyEquipAction` (+1 body) |
| **A-03a** | M5c (m5c-plan.md D-02, D-03) | `CM_SHOW_DIALOG`, `CM_CLOSE_DIALOG`, **`CM_DIALOG_SELECT` ported whole, including the target-0 journal arm and its `ENABLE_SIMPLE_2NDCLASS` call** (CM_DIALOG_SELECT.java:75-107), `DialogService` whole | no files; `DialogService.cpp` 7 `AION_UNPORTED` | C-01's live path, X1, X3-X8, C-05 | M5e ports the three packets (+~6 bodies) — m5d-plan.md D4's rule, one port not two |
| **A-03b** | M5c (C-01, W-06) | `RecipeService::autoLearnRecipes` — **every character reaching level 10 autolearns 30003 and 40009** from `craft_skill_tree.xml` (m5c-plan.md:290) | `RecipeService.cpp:15` `AION_UNPORTED` | X7 (the first Daeva level), every level-10 character | the level-10 kill throws inside `onLevelChange`; M5e must take the body |
| **A-03c** | M5c (D5, X21a) | the Daeva seed (class + `player_quests(1006, COMPLETE)` + exp) and the offline level change at enter world (W-20) gated once | – | D8's seeds | – |
| **A-04a** | M5d (T-items) | `QuestState` setters (`setStatus`, `setQuestVar`, `setRewardGroup`, `QuestState.cpp:37-85`), `QuestVars`, `QuestStateList.addQuest` | 13 + 3 `AION_UNPORTED` | C-01 (`completeAscensionQuest`), X5 | C-01 cannot finish; the class change does not make a Daeva |
| **A-04b** | M5d (H-01, E-items) | `AbstractQuestHandler` dialog helpers (`sendQuestDialog`, `sendQuestEndDialog`, `updateQuestStatus`), `QuestService::startQuest` / `finishQuest`, the XML registration, `QuestEngine::onLevelChanged` handlers | 88 of 91 / 26 `AION_UNPORTED` | C-05 (the trainer quests), X1 | C-05 waits; the trainer case of the gate drops |
| **A-04c** | M5d (G-02) | `tests/scenario/decoders/QuestDecoders` (`SM_DIALOG_WINDOW`, `SM_QUEST_ACTION`, `SM_STATUPDATE_EXP`) | not in `decoders/` (measured); `SM_QUEST_COMPLETED_LIST` **is** decoded already (`decoders/PacketDecoders.h:267-280, 494`) | G-02, X8 | G-02 writes them |
| **A-05** | M5b-1 | experience and DP on a kill (`NpcController.cpp:262-268`), the fight helper, respawn, the HP seed (m5b-plan.md D12) | ported and gated | X1-X3, X7, X9, X13 | – |
| **A-06** | M5a | creation of all six starting classes (`CM_CREATE_CHARACTER.java:92` refuses only non-starting classes); `oracle.py m5a-creation --class` for any of them | ported | the B-account characters of §10 | – |
| **A-07** | M5b-2 G-07 | `CheckOutput::zeroLiveClasses` names `Effect`, `EffectReserved`, `Skill` | P5-14 | G-06 adds `Summon` | G-06 adds all four |

**Re-verification at branch time:** grep each named file for `AION_UNPORTED(`; run `python tools/porting/chunks.py owner` on each new file; re-run
the §12 scripts (they read the working tree, so they give the new "not ported" lists directly).

---

## 1. Summary

**Everything that makes a character progress already runs; nothing that a Daeva does after it does.** M5a had to port the level machinery for
character creation and M5b-1 for experience, so the path from a kill to a new skill on the bar is ported end to end:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `SkillLearnService` — all 7 bodies: `onLearnSkill`, `learnNewSkills` (incl. the 30001 → 30002 swap), `autoLearnSkills`, `learnSkillBook`, `removeSkill` | `services/SkillLearnService.cpp:34-135` (Java SkillLearnService.java:22-112) |
| `PlayerSkillList` (the `isNew` rule that picks the "you learned" message), `PlayerSkillEntry`, `SkillTreeData` | `model/skill/PlayerSkillList.cpp`, 0 unported; PlayerSkillList.java:57-77 |
| The level machinery: `PlayerCommonData::addExp` / `setExp` (the non-Daeva cap at 9) / `updateDaeva` / `setDp` | `PlayerCommonData.cpp:187-205, 224-235, 265-290` (Java :167-221, 273-288, 463-470, 588-610) |
| `PlayerController::onLevelChange` and `upgradePlayer`, the enter-world level change and passive activation | `PlayerController.cpp:670-714`; `PlayerEnterWorldService.cpp:395, 411-425, 663-671` |
| `DialogPage.getStartPageId` **with the Daeva branch** (page 1352 for 12 Poeta and 35 Ishalgen npcs, 47 in all) | `model/DialogPageInfo.cpp:17-29, 31-80` (DialogPage.java:113-176; the list :128-176) |
| Flight and gliding for Daevas: `FlyController`, `CM_MOVE`'s glide bit, `CM_EMOTION` fly, the FP reduce/restore tasks | `controllers/FlyController.cpp` (0), `CM_MOVE.cpp` (0), `CM_EMOTION.cpp` (0), `PlayerLifeStats.cpp:203-255` |
| The charge-skill engine, stances, toggle activation | `CreatureController.cpp:486-505` (`useChargeSkill`), `PlayerController.cpp:785-801`, `skillengine/model/ChargeSkill.cpp` (m5b2 S-07) |
| The summon object: `Summon`, `SummonController`, `SummonMoveController`, `VisibleObjectSpawner::spawnSummon/Homing/Servant/Trap` | 0 unported each (measured) |
| `StigmaService::onPlayerLogin`, `addLinkedStigmaSkills` | `services/StigmaService.cpp:34-109` |
| Every server packet on the path: `SM_ACTION_ANIMATION`, `SM_PLAYER_INFO`, `SM_DIALOG_WINDOW`, `SM_QUEST_ACTION`, `SM_SKILL_LIST`, `SM_SKILL_REMOVE`, `SM_STATUPDATE_EXP`, `SM_STATUPDATE_DP`, `SM_ASCENSION_MORPH`, `SM_RESURRECT`, `SM_MANTRA_EFFECT`, `SM_RIDE_ROBOT`, `SM_FLY_TIME`, `SM_SUMMON_{PANEL,UPDATE,PANEL_REMOVE,OWNER_REMOVE,USESKILL}`, `SM_TRANSFORM(_IN_SUMMON)` | 0 `AION_UNPORTED` in each (measured). **But `SM_SUMMON_PANEL` and `SM_SUMMON_UPDATE` construct through `SummonGameStats`, 12 of whose 12 bodies are unported** — m5c-plan.md finding 2's trap, one layer down |
| The configuration and the enter-world hook of the simple class change | `CustomConfig.cpp:18` binds `gameserver.simple.secondclass.enable` (default false); `PlayerEnterWorldService.cpp:559-560` calls `showClassChangeDialog` |

**What is empty, measured:**

| # | Hole | Chunk | Size |
|---|---|---|---|
| 1 | **`ClassChangeService`** — 7 of 7 bodies (`ClassChangeService.cpp:7-33`) | P5-08 | 7 sites, 166 Java lines |
| 2 | **The Daeva's effect classes**, levels 10-20, all 11 advanced classes, **with the skills their effects launch**, plus the monsters of Verteron and Altgard: **36 classes** after A-02c (+5 if M5b-3 does not deliver them) | P5-03 (21), P5-04 (15) | **82 sites + 8 inner bodies**, 1,741 Java lines |
| 3 | **The trainer quests** "A New Skill" `_1205ANewSkill`, `_2132ANewSkill` — no C++ file (the C++ quest-handler tree holds only `QuestPrelude.h`) | Q05, Q09 (phase 6, leased) | 6 bodies, 270 Java lines |
| 4 | **The Daeva's two client packets**: `CM_TOGGLE_SKILL_DEACTIVATE` (14 toggles by level 20), `CM_USE_CHARGE_SKILL` (6 charge skills) — no file | P5-16 | 4 bodies, 75 Java lines |
| 5 | **Resurrection by a player**: `ResurrectEffect` (in hole 2) and `PlayerReviveService::skillRevive` | P5-08 | 1 site |
| 6 | **Skill books**: `SkillLearnAction.canAct` / `act` / `validateClass` | P5-07 | 2 stubs after A-02a + 1 private helper |
| 7 | **Summons** (stage 3): `SummonsService` 13, `TrapService` 2, **`SummonRelease` 2**, `SummonGameStats` 12, `SummonLifeStats` 1, 6 effect classes (9 + 1 inner, **with `SpellAtkDrainInstantEffect`**, which the earth spirit's order skill needs, §2.5), the `servant` / `trap` / `homing` AIs (no file, 18 bodies), `CM_SUMMON_*` × 5 (no file, 10 bodies) | P5-08, P5-01, P5-04, P5-05, P5-16 | **68 bodies**, ~1,400 Java lines |
| 8 | **Stigma** (stage 3, optional): `StigmaService` 10 bodies (without M5b-3's `notifyEquipAction` and M5c's `chargeStigma`), 10 stigma-only effect classes (19 + 5 inner; `MpAttackInstantEffect` 2 unless M5b-3's E-03 took it) | P5-07, P5-03, P5-04 | **34 bodies**, ~780 Java lines |

**Total: ~213 bodies — 160 `AION_UNPORTED` sites and 53 bodies no site count can see (25 %) — over ~4,500 Java lines**, in three stages:
stage 1 (the class change and the Daeva, ~111 bodies), stage 2 (the gate), stage 3 (summons and stigma, ~102 bodies). That is M5c's size
(~195 bodies, m5c-plan.md rev 2 :77), not M5b-2's (~521). The retail ascension (25 bodies, 974 Java lines in Q06) is **not** in it (D1, D13).

**Five findings shape the plan.**

1. **The class change itself is small and is shared by both routes** (§2.3). `ClassChangeService.setClass` is what `_1006Ascension.setPlayerClass`
   calls (`_1006Ascension.java:238-244`) and what the simple route calls (ClassChangeService.java:31-34). Porting it is never wasted, whatever
   the user answers to D1.
2. **The Daeva wake-up is broad, and one piece of it is an enter-world blocker** (§2.4, §2.10 W-07). A Gladiator learns 563 *Determination*
   (`condskilllauncher`) at level 15; `SkillLearnService.onLearnSkill` applies a new passive at once (SkillLearnService.java:35-36) and
   `activatePassiveSkillEffects` re-applies it at every enter world without a `catch` (`PlayerEnterWorldService.cpp:663-671`). It is the only
   passive of levels 9-20 outside the ported set, and it must land before any gate or player holds a level-15 Gladiator.
3. **The effect subset is the milestone, again** (§2.4). Levels 10-20 of the 11 classes need 39 unported classes once the skills their effects
   launch are followed; stopping at level 20 is the natural boundary (the first stigma slot and the Eltnen/Morheim monsters begin there; §2.8).
   The rule of m5b2-plan.md D6 stays: every other class throws.
4. **Becoming a Daeva switches on code that has never run** (§2.10): gliding and flight, the FP tasks, DP, the page-1352 dialogs — ported and
   dormant — and **the teleporters of both start maps, whose Daeva branch calls the unported `TeleportService::showMap`** (DialogService.java:187-197).
5. **The only "talk to a trainer" moment of 4.8 is a phase-6 quest pair, and it is cheap** (§2.2): pulling `_1205ANewSkill` / `_2132ANewSkill`
   forward (D3) gives the milestone its trainer dialog and makes it the **first Java quest handler** the C++ registry
   (`AION_QUEST_HANDLER`, `src/aion/gameserver/handlers/HandlerRegistry.h:319`) ever holds.

---

## 2. The paths, end to end

"ported" means the C++ body exists and contains no `AION_UNPORTED`; **U** means `AION_UNPORTED`; **no file** means neither `.h` nor `.cpp`.

### 2.1 From a kill to a new skill (levels 1-9) — ported, never gated past level 2

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Kill → `NpcController.doReward` → `addExp(rewardXp, XP_HUNTING, name)` and `addDp(rewardDp)` | NpcController.java:222-236 | ported (`NpcController.cpp:262-268`) | P4-11b |
| 2 | `addExp`: repose (level ≥ 10), salvation (level ≥ 15), `setExp`, `STR_GET_EXP*`; **at level 9 with exp ≥ startExp(10): `STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1`** | PlayerCommonData.java:167-221 | ported | P4-12 |
| 3 | `setExp`: `maxLevel = isDaeva ∨ (offline ∧ (updateDaeva() ∨ exp > startExp(10))) ? 66 : 10`; exp clamped to startExp(maxLevel); level = min(levelForExp, maxLevel − 1) → **a non-Daeva stops at level 9 with a full bar (126,069 exp)**; `onLevelChange`; `SM_STATUPDATE_EXP` | PlayerCommonData.java:273-288; player_experience_table.xml:3-23 | ported (`PlayerCommonData.cpp:187-205`) | P4-12 |
| 4 | `onLevelChange(old, new)`: ratio, `updateStatsTemplate`, `updateMaxRepose`, `resetSalvationPoints`, `upgradePlayer` (`synchronizeWithMaxStats`, `updateStatsVisually`; team / legion updates), `SM_ACTION_ANIMATION(LEVEL_UP, level)`, `NpcFactions.onLevelUp`, `QuestEngine.onLevelChanged`, `updateNearbyQuests`, guides, **`learnNewSkills(old+1, new)`**, bonus/faction packs (level 65), starter kit (off) | PlayerController.java:568-611 | ported (`PlayerController.cpp:670-714`); `QuestEngine::onLevelChanged` dispatches to whatever M5d registers (A-04b) | P4-11b |
| 5 | `learnNewSkills`: levels `to` down to `from`; below 10 an advanced class also learns its starting class's rows; at `to ≥ 10` a Daeva swaps 30001 for 30002 | SkillLearnService.java:60-75, 84-93 | ported (`SkillLearnService.cpp:80-113`) | P5-08 |
| 6 | `PlayerSkillList.addSkill`: a known lower level is replaced; **`isNew` is false if the skill's pre-skill (`skillLearn`) is known** → `onLearnSkill` → `SM_SKILL_LIST(skill, isNew ? 1300050 : 0)` if spawned; **a passive is applied at once** (`applyEffectDirectly`); crafting and morph skills → `RecipeService.autoLearnRecipes` | PlayerSkillList.java:57-77; SkillLearnService.java:22-55 | ported; the recipe call is **U** (A-03b) | P5-02b / P5-08 |
| 7 | At enter world: `activatePassiveSkillEffects` (before the connection is set), then `onLevelChange(PlayerDAO.getOldCharacterLevel, level)` for a level changed offline, the DP reset after 5 minutes offline, then the full split `SM_SKILL_LIST` | PlayerEnterWorldService.java:186, 204, 215, 229-230 | ported (`PlayerEnterWorldService.cpp:395, 411-425`) | P5-00 |

**Measured for the gate** (rev 2, with a model of the Java walk, §12 `m5e_isnew.py`): an Elyos Warrior learns 2877 at 3, 139 + 2890 at 5, 2865
at 6, 2903 at 7, 2878 at 8, **138 *Boost Parry I* (passive) at 9**, nothing at 2 and 4 — **all with message 1300050**. Rev 1 said 0 for 2865 and
2878 because their `skillLearn` pre-skills (2864, 2877) are known; but `addSkill` does not read the skill's own row: it walks
`SkillTreeData.getSkillsForSkill` from the **highest skill of the stack** (`getHighestSkill`, by `lvl`) **for the player's current class**
(PlayerSkillList.java:67-73; SkillTreeData.java:94-127, 137-162). The tops of WA_ROBUSTHIT (2876) and WA_ROBUSTBLOW (2889) have only GLADIATOR
and TEMPLAR rows (`skill_tree.xml:2892-2893, 2916-2917`), so for a Warrior the walk is empty and `isNew` stays true. The oracle must port
`getHighestSkill` + `createSkillTree` exactly (G-01). The level thresholds are `startExp(L) = experience[L − 1]`: 400, 1,433, 3,820, 9,054,
17,655, 30,978, 52,010, **82,982 (level 9), 126,069 (level 10)**, 182,252, … 649,169 (15), … 2,314,771 (20) (PlayerExperienceTable.java:29-34;
player_experience_table.xml:3-23).

**`players.old_level` decides what an offline level change teaches.** It is written only at leave world, with the current level
(PlayerLeaveWorldService.java:148 → `PlayerDAO.storeOldCharacterLevel`), and `onLevelChange(old, new)` returns at once when the two are equal
(PlayerController.java:569-570). A gate that seeds `exp` for level 8 **and** `old_level` 8 therefore teaches nothing of levels 3-8, and every
later `isNew` (2891 at 10 needs 2890 of level 5) changes with it. The gate leaves `old_level` as the server stored it (§10.2 C5).

**Every effect class a starting class meets on the way to 9 is ported** (A-01): the six starting classes' autolearn rows of levels 1-10, 64
skills over 21 leaf classes, leave nothing unported in the working tree (§12). That includes ENGINEER and ARTIST, which m5b2-plan.md §2.4 left
out of its count.

### 2.2 The trainer: there is no learn list, there is a tutorial quest

| Question | Measured answer |
|---|---|
| A dialog action that teaches a skill? | **no class skill**: `DialogAction.java:14-140` lists the function actions 1-125; the only skill arms of `DialogService.onDialogSelect` are `GATHER_SKILL_LEVELUP` 45 / `COMBINE_SKILL_LEVELUP` 46 (DialogAction.java:60-61; DialogService.java:199-202), the crafting and gathering masters' **profession skills** for kinah from level 10 (CraftSkillUpdateService.java:83-114) — **M5c's** (m5c-plan.md C-01) |
| A client packet that learns a skill? | none of the 188 `CM_*` classes; the 42 C++ `CM_*` files include none either |
| A skill function npc on the start maps? | no: the Poeta and Ishalgen npcs with `func_dialogs` are merchants (`2 3`), soul healers (`35`), teleporters and flight masters (`44`), manastone removers (`42`) and cube expanders (`47`) — 12 per map, §12 `m5e_poeta_npcs.py` |
| How skills are learned | `autolearn` on level change: **3,525 of 4,693 rows**; stigma: 1,131 rows (one of them also autolearn); **skill books: 38 rows**, items 169500916-169500953, sold by no goods list (`goodslists*.xml`), found only in `decomposable_items.xml` (two Sorcerer books), `EmpyreanCrucibleInstance.java` (one) and a quest reward (Homeward Bound 295/296) |
| The "trainers" | the class masters **203087 kunandes (Warrior), 203088 gilden (Scout), 203089 esera (Mage), 203090 kagas (Priest), 801210 lysander (Engineer), 801211 cecilia (Artist)**, one spawn each in `210010000_Poeta.xml`, `ai="general"`; Ishalgen 203527-203530, 801218, 801219 (`_2132ANewSkill.java:26-31`) |
| What they do | **quest 1205 "A New Skill"** (ELYOS, `minlevel_permitted="2"`, IMPORTANT, reward 275 exp) and **2132** (ASMODIANS, level 3): `onLevelChangedEvent` starts the quest and sets it straight to REWARD with the class's quest var and reward group (`_1205ANewSkill.java:35-71`); talking to the class's master shows the class page (1011 / 1352 / 1693 / 2034 / 2375 / 2716) and `sendQuestEndDialog` pays (`:73-133`). The skills themselves came from autolearn |

**So "learn skills from trainers" is, in 4.8: the autolearn (ported, gated here for the first time past level 2), the class masters' tutorial
quest (C-05), and skill books (C-03).** The gate asserts all three (X2 and X7 for the autolearn, X1 for the class master, X16 for a book).

### 2.3 The level-9 wall and the class change

A starting class cannot pass level 9 (§2.1 row 3). It becomes a Daeva when it has an advanced class **and** quest 1006 / 2008 is COMPLETE
(`updateDaeva`, PlayerCommonData.java:588-610). Two routes set both; both end in `ClassChangeService.setClass`.

**Route S — the Java server's simple route** (`gameserver.simple.secondclass.enable = true`; default false, `config/main/custom.properties:32`):

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| S1 | Enter world, after the mail/housing/broker block: `if (ENABLE_SIMPLE_2NDCLASS) showClassChangeDialog(player)` | PlayerEnterWorldService.java:320-321 | ported call (`PlayerEnterWorldService.cpp:559-560`) | P5-00 |
| S2 | `showClassChangeDialog`: level ≥ 9 and a starting class → `SM_DIALOG_WINDOW(0, getClassSelectionDialogPageId(race, class), race == ELYOS ? 1006 : 2008)` | ClassChangeService.java:23-29 | **U** (`ClassChangeService.cpp:7`) | P5-08 |
| S3 | Page ids: Elyos WARRIOR 2375, SCOUT 2716, MAGE 3057, PRIEST 3398, ENGINEER 3739, ARTIST 4080; Asmodian 3057, 3398, 3739, 4080, 3569, 3910 | ClassChangeService.java:90-107 | **U** | P5-08 |
| S4 | The client answers **`CM_DIALOG_SELECT(targetObjectId 0, dialogActionId, 0, 0, questId 1006, 0)`**: the target-0 arm looks up the quest template, tries `can_report`, then `QuestEngine.onDialog(env)` — **false, because no handler is registered for 1006** (`_1006Ascension.register` returns at once when the key is on, `_1006Ascension.java:48-50`; and it is a phase-6 handler anyway) — then `if (ENABLE_SIMPLE_2NDCLASS && (questId == 1006 ∨ 2008)) changeClassToSelection(player, dialogActionId)` | CM_DIALOG_SELECT.java:75-107; QuestEngine.java:152-182 | **no file** (M5c, A-03a); `QuestEngine::onDialog` ported | P5-15 |
| S5 | `changeClassToSelection`: `setClass(player, getSelectedPlayerClass(race, action), validate = true, updateDaevaStatus = true)`, then **`SM_DIALOG_WINDOW(0, 0)` whatever the result** | ClassChangeService.java:31-34 | **U** | P5-08 |
| S6 | The action → class table: Elyos `SELECT5_1` 2376 GLADIATOR, `SELECT5_2` 2461 TEMPLAR, `SELECT6_1` 2717 ASSASSIN, `SELECT6_2` 2802 RANGER, `SELECT7_1` 3058 SORCERER, `SELECT7_2` 3143 SPIRIT_MASTER, `SELECT8_1` 3399 CLERIC, `SELECT8_2` 3484 CHANTER, `SELECT9_1` 3740 GUNNER, `SELECT9_2` 3825 RIDER, `SELECT10_1` 4081 BARD; Asmodian shifted by one page, Gunner/Rider/Bard on `SELECT8_3_1` 3570, `SELECT8_3_2` 3591, `SELECT9_3_1` 3911 | ClassChangeService.java:109-164; DialogAction.java:1517-3307 | **U** | P5-08 |
| S7 | `setClass`: with `validate`, refuse a non-starting class ("You already switched class") and any class outside `(id, id + 2]` ("Invalid class chosen") — both through `sendMessage`, i.e. `SM_MESSAGE`; then `setPlayerClass`, `updateStatsTemplate`, `upgradePlayer` (**`SM_STATS_INFO` to self** through `updateStatsVisually`, PlayerController.java:601-604, PlayerGameStats.java:51-53, 331-332), **`SM_ACTION_ANIMATION(CLASS_CHANGE, level)` to self and the sighted** (`toSelf` true; wire id **4**, the same as `CRAFT_LEVEL_UP`, ActionAnimation.java:16-17), **`SM_PLAYER_INFO` to the sighted only** — the two-argument `broadcastPacket(VisibleObject, …)` walks the known list and skips the object itself (ClassChangeService.java:75-76; PacketSendUtility.java:98-100), so **the player who changed class receives no `SM_PLAYER_INFO`** — then **`learnNewSkills(9, level)`**; with `updateDaevaStatus`: `completeAscensionQuest` and `updateDaeva` | ClassChangeService.java:51-88 | **U** | P5-08 |
| S8 | `completeAscensionQuest`: a new `QuestState(1006 / 2008, COMPLETE)` + `SM_QUEST_ACTION(ADD)`, or the existing one set COMPLETE; var 0, reward group 0; `SM_QUEST_ACTION(UPDATE)` | ClassChangeService.java:36-49 | **U**; its `QuestState` setters are M5d's (A-04a) | P5-08 / P5-06 |
| S9 | The next exp gain: `setExp` sees `isDaeva`, the stored 126,069 is under startExp(66) → **level 10**, `onLevelChange(9, 10)` → the class's level-10 skills (§2.4) | PlayerCommonData.java:273-288 | ported | P4-12 |

**What S7's `learnNewSkills(9, 9)` teaches, measured:** 18 distinct passives over the 11 classes (49 rows) — `wpnmastery`, `armormastery`,
`shieldmastery`, `wpndual` only, **all four ported** (A-01). For a Gladiator: 44, 46, 48, 49, 50 (message 0: the walk from the top of each
stack, for the class GLADIATOR, reaches a known pre-skill among 37, 39, 41, 42, 43) and 45, 51, 52, 53, 54 (message 1300050) — confirmed with the
§2.1 model (`m5e_isnew.py`). **51 is the greatsword skill** (`ItemGroup.GREATSWORD` requires 51, ItemGroup.java:17), which rev 2's X9 seed
depends on (D8). The starting-class masteries stay: 37 and 44 have different stacks (`P_EQUIP_ENHANCEDSWORD` against `P_EQUIP_SWORD`,
`skill_templates.xml`), so both apply.

**A Java quirk the port keeps (D11):** `changeClassToSelection` checks no level (ClassChangeService.java:31-34; only `showClassChangeDialog`
checks level ≥ 9). With the key on, a level-1 Warrior that sends `CM_DIALOG_SELECT(0, 2376, …, 1006)` becomes a level-1 Gladiator *and* a
Daeva. Faithful; recorded, not fixed.

**Route R — the retail route, quest 1006 / 2008 (phase 6, Q06):**

| Needs | Where | Owner |
|---|---|---|
| The handler pair: `_1006Ascension` (9 bodies, 287 lines), `_2008Ascension` (8, 307); the follow-ups `_1007ACeremonyinSanctum` (4, 186), `_2009ACeremonyinPandaemonium` (4, 194) | `data/handlers/quest/ascension/` | Q06 (no C++ file) |
| `defaultOnLevelChangedEvent` with **no pre-quests** — 1006 starts at level 7 (mission `minLvlDiff` 2) whatever 1001-1005 say | `_1006Ascension.java:272-275`; AbstractQuestHandler.java:988-1013 | M5d (A-04b) |
| A quest item used inside the item-use zone `LF1_ITEMUSEAREA_Q1006` | `_1006Ascension.java:194-207` | M5b-3 `CM_USE_ITEM` + M5d `onItemUseEvent` |
| **The solo instance Karamatis B (310020000) / Ataxiar B (320020000)**: `InstanceService.getNextAvailableInstance` + `TeleportService.teleportTo(player, instance, …)` | `_1006Ascension.java:97-99`; `_2008Ascension.java:131-132` | **M5f**: `InstanceService.cpp:81-95` U, `TeleportService.cpp:260-271` U |
| **A flight teleport** (skill 281, FLYING state, `SM_EMOTION(START_FLYTELEPORT, 1001)`, flight path 1001) and 43 s later four spawned raiders with hate | `_1006Ascension.java:153-178` | M5f (flight paths); M5d (quest spawns) |
| Movies (14, 151), `SM_ASCENSION_MORPH`, `onDieEvent` / `onEnterWorldEvent` resets | `_1006Ascension.java:140, 228, 246-270` | ported packets; M5d `CM_PLAY_MOVIE_END` (W there) |
| The class choice: Pernos 790001 `SETPRO4` → page, `SETPRO5..15` → `setPlayerClass` → **`ClassChangeService.setClass(player, class)`** (validate, no Daeva update; the quest's own COMPLETE does that through `onQuestCompletedEvent` → `updateDaeva`) | `_1006Ascension.java:104-130, 238-244, 278-285` | **P5-08 — this plan (C-01)** |

**Route R cannot run before M5f.** Every in-map teleport it uses is ported (`TeleportService.cpp:236-249, 274-282`), but the instance half is not,
and the instance is the quest. D1 and D13 carry the consequence.

### 2.4 After the class change: what a Daeva of levels 10-20 uses

Measured over `skill_tree.xml` × `skill_templates.xml` × `Effects.java`, closed under `extends`, **and, in rev 2, closed under the skills an
effect launches**, against HEAD `760e8ab5c` (§12 `m5e_effects.py`, `m5e_detail.py`, `m5e_l10.py`, `m5e_union.py`, rev 2's `m5e_refs.py`,
`m5e_groups.py`, `m5e_leaves.py`). "Unported" = a class with an `AION_UNPORTED` site or an undeclared Java method.

**The launched skills (lesson 2, missed by rev 1).** An effect starts another skill's effects through `subeffect skill_id` (every effect:
`EffectTemplate.calculateSubEffect`, chance 100 by default, EffectTemplate.java:398-433, SubEffect.java:15-18), `delayedskill`, `provoker`,
`skilllauncher`, `condskilllauncher`, `aura` and `resurrect` / `rebirth` `skill_id` (DelayedSkillEffect.java:25, ProvokerEffect.java:62,
SkillLauncherEffect.java:23, CondSkillLauncherEffect.java:46, AuraEffect.java:71, ResurrectEffect.java:28 → `updateSoulSickness`), and
`carvesignet signet_id` … `signet_id + signet_cap − 1` (CarveSignetEffect.java:43). None of these goes through a skill's start conditions. Followed
to a fixpoint they add 4 skills at level 10, 22 over levels 9-20, 4 for the monsters and 5 for the stigma skills, and they reach **seven
unported classes a direct closure misses** — six new to this plan, and `PoisonEffect`, which rev 1 had only in the monsters' row:

| Class (sites, chunk) | Reached through | First level |
|---|---|---|
| `ProcAtkInstantEffect` (2, P5-04; **M5b-3's E-02, A-02c**) | Bard 4285 *Syncopated Echo* → `delayedskill` 8905 (and 4286 / 4287 → 8906 / 8907); Chanter 1627 *Promise of Earth* → `provoker` 8963 | 10 (Bard), 16 (Chanter) |
| `SignetEffect` (2, P5-04) | every `carvesignet`: Assassin 3385 / 3386 *Rune Carve* (13 / 18) → 8303-8305, 3417 *Fang Strike* (16) → 8303-8307; stigma 3396 *Sigil Strike* → 8303-8305 | 13 |
| `PoisonEffect` (5, P5-04; A-02c) | Assassin 3481 *Apply Deadly Poison* → `provoker` 9100 (it was already in the npc row) | 15 |
| `PulledEffect` (4, P5-04; **reads geo**) | Templar 3123 *Aether Leash* → `subeffect` 8441 *Capture* | 16 |
| `SimpleRootEffect` (4, P5-04; **reads geo**) | Assassin 3417 → `subeffect` 8219 | 16 |
| `SpinEffect` (4, P5-04) | monsters only: Verteron 16625 *Redirect Attack* (veteran tursin scouts 210183 / 210184, levels 17-18, 54 spots; crack tursin trackers 210185 / 210186; 210310, 210314) and 17091 *Powerful Wind* (frost wind spirit 210711; Altgard's cyclone spirits 210523 / 210578, against flying targets only) → `subeffect` 8223 *Spin* | – |
| `MpAttackInstantEffect` (2, P5-04; M5b-3's E-03, W) | stigma 2825 *Leeching Steel* (Rider, 20) → `provoker` 9011 | stigma |

| Scope | Skills (+ launched) | Passive / toggle / charge | Leaf classes | With bases | **Unported classes** | Sites | Java LOC |
|---|---|---|---|---|---|---|---|
| the class change (11 classes, level 9) | 18 (+0) | 18 / 0 / 0 | 4 | 6 | **0** | 0 | 0 |
| the first Daeva level (level 10) | 86 (+4: 8218, 8296, 8905, 8998) | 19 / 3 / 1 | 40 | 47 | **18** (rev 1: 17) | 40 | 946 |
| levels 9-20 | 364 (+22) | 49 / 14 / 6 | 68 | 76 | **39** (rev 1: 34) | 88 | 1,965 |
| … of which summons | – | – | – | – | 5 | 8 | 289 |
| monsters of Verteron + Altgard (§2.8) | 90 + 80 = 115 distinct (+4) | – | 27 | 33 | 12 (7 not in the row above) | 33 | 553 |
| skill books, level ≤ 20 (10 rows) | 10 (+0) | – | 5 | 9 | 4 (all in the class rows) | 12 | 194 |
| **the union** | | | | | **46** (rev 1: 41) | **109** | **2,275** |
| stigma skills, level ≤ 20 (§2.7) | 108 (+5) | 0 / 3 / 0 | 50 | 60 | 32 (11 not in the union) | 68 | 1,511 |

**The first level each unported class is reached by any advanced class:** level 10 — `AuraEffect`, `DashEffect`, `DeformEffect`,
`DelayedSkillEffect`, `DispelEffect`, `HostileUpEffect`, `MPHealInstantEffect` (every class: 249 *MP Recovery*), `OneTimeBoostSkillAttackEffect`,
`PetOrderUseUltraSkillEffect`, **`ProcAtkInstantEffect`**, `ResurrectEffect`, `RideRobotEffect`, `SkillAtkDrainInstantEffect`,
`SkillCooltimeResetEffect`, `SleepEffect`, `SummonEffect`, `SummonHomingEffect`, `TargetChangeEffect`; 13 — `AbstractDispelEffect`,
`CarveSignetEffect`, `DispelDebuffEffect`, `RandomMoveLocEffect`, `SignetBurstEffect`, **`SignetEffect`**, `SummonTrapEffect`; 15 —
**`CondSkillLauncherEffect`**, **`PoisonEffect`**, `SummonServantEffect`; 16 — `DispelBuffCounterAtkEffect`, `MPShieldEffect`, **`PulledEffect`**,
**`SimpleRootEffect`**; 19 — `BackDashEffect`, `DelayedSpellAttackInstantEffect`, `DispelDebuffPhysicalEffect`, `ParalyzeEffect`; 20 —
`AlwaysParryEffect`, `BindEffect`, `BoostSkillCastingTimeEffect`.

**Per class, levels 10-20** (skills, unported classes with the level first reached; rev 2 with launched skills): Gladiator 48, 5
(`MPHealInstant`@10, `SkillAtkDrainInstant`@10, `HostileUp`@10, `TargetChange`@10, **`CondSkillLauncher`@15**); Templar 50, **4** (+`Pulled`@16);
Assassin 40, **11** (+`Signet`@13, `Poison`@15 through A-02c, `SimpleRoot`@16); Ranger 52, 6; Sorcerer 45, 8; Spiritmaster 50, 7 (three summon
classes at 10: `SummonEffect`, `SummonHomingEffect`, `PetOrderUseUltraSkillEffect`); Cleric 50, 7; Chanter 46, **6** (+`ProcAtkInstant`@16);
Gunner 34, 3; Rider (Aethertech) 53, 7; Bard (Songweaver) 44, **9** (+`ProcAtkInstant`@10).

**The only passive of levels 9-20 that needs an unported class is the Gladiator's 563 *Determination* at level 15** (`condskilllauncher`).
Passives are applied at learn time and at every enter world; neither catches. It is W-07 and it decides D7's commit order.

**The level-10 kit that the gate uses** (`m5e_l10.py`), **with the start and use conditions rev 1 did not check** (all `skill_templates.xml`):

| Skill | Effects | Conditions that decide whether the gate's cast is accepted |
|---|---|---|
| Gladiator 519 *Explosion of Rage* | `skillatk` + **`subeffect` 8218 *Stumble*** (StumbleEffect: ported, **reads geo**) | `dp` 2,000 (+ `dpuse`), weapon GREATSWORD DAGGER MACE POLEARM STAFF **SWORD** — the created Warrior's Training Sword 100000094 (`item_group="SWORD"`, item_templates.xml:375; player_initial_data.xml:8) qualifies |
| Gladiator **769 *Absorbing Fury*** | `skillatkdraininstant` hp_percent 10 | weapon **GREATSWORD POLEARM**; `chain category="W_CHAINC_1TH_1"` (a chain opener); `chain_skill_prob` 100 |
| Gladiator 758 *Roiling Hack* | `skillatkdraininstant` hp_percent 30 | weapon **GREATSWORD POLEARM**; `chain category="W_CHAINC_2TH_1" precategory="W_CHAINC_1TH_1"`: `ChainCondition.validate` refuses it unless the current or previous chain skill is a W_CHAINC_1TH_1 (ChainCondition.java:32-52) — i.e. 769 was cast and its chain roll succeeded (Skill.java:627-637; 769 sets no `time`, so `ChainSkills.updateChain(…, 0)` never expires, ChainSkills.java:33-42, 58-60). Both 769 and 758 are `target_type="AREA"` with `first_target="ME"` and `effective_range="7"` |
| Gladiator 2981 *Taunt* | `hostileup` + `targetchange` (`preeffect_prob="20"`) | none |
| Cleric / Chanter **1699 *Light of Resurrection*** | `resurrect skill_id="8296"` (8296 *Soul Sickness*, stack CL_RESURRECTDEBUFF: MAXHP / MAXMP −30 %, speed −50 %) | start condition `target` PC |
| Chanter **1809 *Celerity Mantra*** | `aura skill_id="8998"`, every 6,500 ms (AuraEffect.java:76) | TOGGLE, **`tslot="NOSHOW"`**: never listed in `SM_ABNORMAL_STATE` (EffectController.java:702-703; PlayerEffectController.java:85-88, 118-119). What the caster sees is **8998 *Celerity Mantra Effect*** (`tslot="CHANT"`, `statup duration2="6500"`), re-applied each tick; others see `SM_MANTRA_EFFECT`, which **skips the caster** (`broadcastPacket(effector, …)` with a `Creature`, AuraEffect.java:67; PacketSendUtility.java:98-100) |
| Rider **2767 *Embark*** | `riderobot` (+ stat boosts): `startEffect` sets the robot id from the **main-hand weapon's skin** (RideRobotEffect.java:23-26) and sends `SM_RIDE_ROBOT` to self and the sighted | TOGGLE; weapon **KEYBLADE** — the created Engineer's weapon is 101800181 *Pistol for Training*, a GUN (player_initial_data.xml:77-80; item_templates.xml:137287); keyblades carry the id in `robot=` (e.g. 102100181 → 2500002) |
| Rider **2606 *Kinetic Slam*** | `spellatkinstant` | CHARGE (`skillcharge`); **use condition `ride_robot`**: `RideRobotCondition` is `Player.isInRobotMode()` = robot id ≠ 0 (RideRobotCondition.java:18-24; Player.java:1611-1613) — **only after 2767** |
| Sorcerer 1417 *Curse of Roots* | `deform` + `sleep` | – |
| Spiritmaster **3706 *Summon: Fire Spirit*** | `summon` 833343 + `dispel` | – |
| every class 246 / 249 | `itemuse` of 169300003 *Lesser Odella Powder* (1 / 2) | the item (A-02b) |

**A cast the gate plans is a cast the oracle has checked** (G-01): rev 1 planned two casts (758, 2606) that a correct server refuses.

**The other Daeva mechanics** (all ported, all dormant until a Daeva exists): DP — `setDp` returns for a starting class (PlayerCommonData.java:463-470),
26 DP skills of levels 10-20 (`<dp>` start condition + `dpuse` action), DP from kills (NpcController.java:233); toggles and stances
(`Effect::activateToggleSkill`, `PlayerController.cpp:785-801`); the charge engine; gliding (`CM_MOVE` type & `GLIDE` 0x04 →
`FlyController.switchToGliding`, CM_MOVE.java:95-98; `canGlide` refuses a non-Daeva with `STR_GLIDE_ONLY_DEVA_CAN`, FlyController.java:138-146)
and flight in FLY zones (FlyController.java:92-110); repose energy from level 10 and salvation from 15 (PlayerCommonData.java:232-238).

**The inner bodies no count shows** (lesson 1): 14 anonymous callbacks and one inner task (`AuraEffect.AuraTask.run`), **15 bodies in 13 of
the 57 classes** (the union's 46 and the 11 stigma-only) — `AlwaysBlockEffect`, `AlwaysParryEffect`, `BlindEffect` (`checkAttackerStatus` only,
BlindEffect.java:41), `AuraEffect.AuraTask.run`, `CondSkillLauncherEffect` (`hpChanged`, `onRemoved`), `OneTimeBoostSkillAttackEffect` (two
multiplier overrides), `RideRobotEffect` (`unequip`), `SummonHomingEffect` (`attack`), `CaseHealEffect`, `HealCastorOnAttackedEffect`,
`MagicCounterAtkEffect`, `OneTimeBoostSkillCriticalEffect`, `SummonSkillAreaEffect`. Every other method of the 57 is declared: the M5b-2 header
batch (shells-2) closed the rest (`m5e_effund.py`; the six classes rev 2 added have no inner class and no undeclared method, measured).

**The stage-1 set, per chunk, after A-02c** (sites + inner bodies; `m5e_groups.py`):

| Chunk | Classes |
|---|---|
| **P5-03**, 21 classes, **44 + 5** | `AbstractDispelEffect` 1, `AlwaysBlockEffect` 2+1, `AlwaysParryEffect` 2+1, `AuraEffect` 5+1, `BackDashEffect` 1, `BindEffect` 4, `BoostSkillCastingTimeEffect` 1, `CarveSignetEffect` 1, **`CondSkillLauncherEffect` 2+2**, `CurseEffect` 3, `DashEffect` 1, `DeformEffect` 4, `DelayedSkillEffect` 2, `DelayedSpellAttackInstantEffect` 2, `DispelBuffCounterAtkEffect` 4, `DispelDebuffEffect` 1, `DispelDebuffPhysicalEffect` 1, `DispelEffect` 1 (A-02c: M5b-3 marks it W), `FallEffect` 2, `FpAttackInstantEffect` 2, `HostileUpEffect` 2 — 979 Java lines |
| **P5-04**, 15 classes, **38 + 3** | `MPHealInstantEffect` 4, `MPShieldEffect` 2, `OneTimeBoostSkillAttackEffect` 2+2, **`PulledEffect` 4**, `RandomMoveLocEffect` 2, **`ResurrectEffect` 2**, `RideRobotEffect` 3+1, `SignetBurstEffect` 2, **`SignetEffect` 2**, **`SimpleRootEffect` 4**, `SkillAtkDrainInstantEffect` 1, `SkillCooltimeResetEffect` 1, `SleepEffect` 4, **`SpinEffect` 4**, `TargetChangeEffect` 1 — 762 Java lines (rev 2 added `PulledEffect`, `SignetEffect`, `SimpleRootEffect` and `SpinEffect`, which only launched skills reach) |
| from M5b-3 (A-02c) | `BlindEffect` 4+1 (P5-03), `PoisonEffect` 5, `SilenceEffect` 4, `ParalyzeEffect` 4, **`ProcAtkInstantEffect` 2** (P5-04) — 245 Java lines |

**What these classes call outside `skillengine/`** (lesson 2, read from the Java bodies): **the geo readers** `GeoService.getClosestCollision` /
`findMovementCollision` / `canSee` and `World.updatePosition` (Dash, BackDash, RandomMoveLoc, **Pulled** — PulledEffect.java:38, 51 — and
**SimpleRoot** — SimpleRootEffect.java:43; the ported `StumbleEffect.calculate` already does, StumbleEffect.java:69, and every class reading
`GeoService` in `skillengine/effect` is listed in G-04), `SkillEngine.applyEffect` / `applyEffectDirectly` /
`applyEffectsDirectly` (ported), `EffectController.removeByDispelEffect` / `dispelBuffCounterAtkEffect` / `removePetOrderUnSummonEffects` /
`removeHideEffects` (ported, K-02 of M5b-2), `AttackUtil.calculateSkillResult` / `calculateMagicalOverTimeSkillResult` (ported, F-06),
`PlayerLifeStats.reduceFp` (ported), `SM_RESURRECT`, `SM_RIDE_ROBOT`, `SM_MANTRA_EFFECT`, `SM_SKILL_COOLDOWN` (ported) — and, for the summon
classes only, `SummonsService.createSummon` and `TrapService.registerTrap` (**U**; `SummonServantEffect` and `SummonTrapEffect` read geo as well).
`AuraEffect.onPeriodicAction`'s team arm reaches
`PlayerGroup.getOnlineMembers` (AuraEffect.java:52-60; M5g) and is dormant for a player without a team.

### 2.5 Summons

Three classes summon by level 20: the Spiritmaster (3706 *Fire Spirit* at 10, wind 13, earth 16, water 19; 3809 *Wind Servant* `summonhoming`
at 10), the Cleric (4105 *Holy Servant* `summonservant` at 15), the Ranger (914 / 925 *Spike Trap*, 1027 *Poisoning Trap* `summontrap` at 13-18)
(§12 `m5e_summon.py`). The spirits' npc templates (833287-833348) have **no `ai`** — a Summon is driven by its master's `CM_SUMMON_*` packets;
the servant, homing and trap npcs use the `servant`, `homing` and `trap` AIs (`data/handlers/ai/{ServantNpcAI,HomingNpcAI,TrapNpcAI}.java`,
98 + 39 + 103 lines, **no C++ file**; `AIEngine` falls back to `DummyNpcAI` with `gameserver.dev.missing_ai_handlers=warn`, so a servant would
stand and do nothing, silently).

| Piece | C++ today | Chunk |
|---|---|---|
| `SummonsService` — `createSummon`, `release`, the modes, `ReleaseSummonTask` (4 bodies, declared in the `.cpp`) | **13 U** (`SummonsService.cpp:50-100`) | P5-08 |
| **`SummonRelease.isCancelableByMaster`, `cancel`** — the pending release: `Summon.registerRelease` → `pendingRelease.cancel()`, `cancelReleaseByMaster`, `isReleaseUncancelable` (Summon.java:210-240), called from `SummonController.onAttack` (a monster hitting the spirit during a pending release, SummonController.java:100) and `SummonsService.doMode`'s COMMAND arm (SummonsService.java:192-198) | **2 U** (`model/summons/SummonRelease.cpp:23, 27`) | P5-08 |
| `TrapService.registerTrap`, `unregisterTrap` | 2 U | P5-08 |
| `SummonGameStats` (12), `SummonLifeStats.triggerRestoreTask` (1) — **`SM_SUMMON_PANEL` / `SM_SUMMON_UPDATE` read them on construction** | 13 U | P5-01 |
| `SummonEffect` 2, `SummonHomingEffect` 1+1, `SummonServantEffect` 2, `SummonTrapEffect` 1, `PetOrderUseUltraSkillEffect` 2, **`SpellAtkDrainInstantEffect` 1** (the pet skills, below) | 9 U + 1 inner | P5-04 |
| **The spirits' own skills** (lesson 2 for stage 3): a Summon casts only `pet_skills.xml` skills (`petHasSkill`, SummonController.java:128; CM_SUMMON_CASTSPELL.java:79; `getPetOrderSkill`, PetOrderUseUltraSkillEffect.java:40). The 34 npcs the level ≤ 20 summon skills create have 168 pet-skill rows; the 28 whose order skill is learned by 20 (3837 *Spirit Disturbance*, Spiritmaster 10; 3852) need **one** unported class: 22197 *Command: Earth Disturbance* (earth spirit 833288) is `spellatkdraininstant` (`pet_skills.xml:379`; §12 `m5e_pets.py`) | `SpellAtkDrainInstantEffect` 1 U — rev 1 had it only in the optional T-02, so declining D6 left a spirit order that throws | P5-04 (M-03) |
| `ServantNpcAI` 7, `TrapNpcAI` 8, `HomingNpcAI` 3 | no file | P5-05 (`handlers/ai`) |
| `CM_SUMMON_COMMAND` (0x015C), `CM_SUMMON_MOVE` (0x016C), `CM_SUMMON_EMOTION` (0x016D), `CM_SUMMON_ATTACK` (0x016E), `CM_SUMMON_CASTSPELL` (0x0190) | no file (`ClientPacketInfo.gen.inc:112, 173-176`) | P5-16 |
| `Summon`, `SummonController`, `SummonMoveController`, `VisibleObjectSpawner::spawn*` | ported | – |
| `cycles.toml` rows | **exist**: `Summon.master`, `Summon.pendingRelease`, `Summon.skillOrders`, `Player.summon`, `ReleaseSummonTask.{release,summon}`, `SummonHomingEffect$1#homing` (`cycles.toml:135-137, 161, 277-278, 324`) | – |

### 2.6 Skill books

`CM_USE_ITEM` (M5b-3) → `SkillLearnAction.canAct` (level, class or its starting class, item race, not yet known) → `act`:
`SM_ITEM_USAGE_ANIMATION`, `SkillLearnService.learnSkillBook` (ported), `STR_USE_ITEM`, delete the book (SkillLearnAction.java:31-69). After
A-02a the two virtuals are declared stubs; `validateClass` is a private helper that becomes a `.cpp`-local function. **10 book rows are at level
≤ 20**: Ranger transformations 1 / 5 / 9 / 13 (level 10), Sorcerer sleeps 17 / 18 (level 10), Cleric summons 21 / 25 / 29 / 33 (level 16).

### 2.7 Stigma

| Step | Java | C++ today |
|---|---|---|
| Slots: `getPossibleStigmaCount` = 0 **unless quest 1929 (Elyos) / 2900 (Asmodian) is complete** (or at var 98 / 99), then 1 below level 30, 2 below 40, 3 above; membership `gameserver.quest.stigma.slot` (default 10) bypasses | StigmaService.java:274-307; MembershipConfig.java:34-35 | U (`StigmaService.cpp:123-128`) |
| The quests: `_1929ASliverofDarkness` (303 lines, Sanctum), `_2900NoEscapingDestiny` (277, Pandaemonium) | Q11 / Q10 | no file; the capitals need M5f's teleports |
| The stigma master: `OPEN_STIGMA_WINDOW` (4) → `sendDialogWindow` | DialogService.java:99, 118, 293-296 | M5c (A-03a) |
| Equipping: `CM_EQUIP_ITEM` → `Equipment.equipItem` → `notifyEquipAction`: slot check, **25,000 kinah** (50,000 LEGEND, 100,000 UNIQUE; 1,000 in the two mission instances) through `PricesService`, `addStigmaSkills` → `learnTemporarySkill` (message 1300401) | StigmaService.java:42-86, 423-430 | `notifyEquipAction` M5b-3 (A-02d); `addStigmaSkills`, `removeStigmaSkills` U |
| At login: `onPlayerLogin` re-checks each equipped stone and re-adds the skills | StigmaService.java:88-132 | ported (`StigmaService.cpp:34-83`), its callees `isPossibleEquippedStigma`, `addStigmaSkills` U |
| Data | 394 stigma stones, **210 of them level 20**; 109 stigma rows at level 20 | `item_templates.xml`, `skill_tree.xml` |

**Stigma's service and effect classes fit M5e's lanes; its gameplay does not reach a real player before M5f** (D6).

### 2.8 Levelling beyond the start maps

The start maps stop at level 10: Poeta's hostile npcs are levels 1-10 (24 npc ids at 10) plus a few guards and bosses at 20-50; Ishalgen's the
same (§12 `m5e_maps.py`). **The level 10-20 maps are Verteron (Elyos, 324 npc ids over 2,328 spots, hostile levels 10-20) and Altgard (Asmodian,
402 over 2,464)**; Eltnen and Morheim are 20-40. A Daeva gets there through the capitals' teleporters or the Poeta/Ishalgen teleporter
(`AIRLINE_SERVICE` for Daevas, DialogService.java:187-197) — **M5f**. Until M5f the gate seeds positions (D8) and the real client relocates by SQL
(§11), as m5c-plan.md did for Sanctum.

Their monsters' skills (all `npc_skills` files, `npc_ids` lists expanded): Verteron 134 npc ids with skills, 90 skill ids, 9 unported classes;
Altgard 171 / 80 / 7; **union 11 classes, 29 sites directly, and 12 classes, 33 sites with the launched skills** — `AbstractDispelEffect`,
`AlwaysBlockEffect`, `BlindEffect`, `CurseEffect`, `DispelDebuffEffect`, `DispelEffect`, `FallEffect`, `FpAttackInstantEffect`, `PoisonEffect`,
`SilenceEffect`, `SleepEffect`, and **`SpinEffect`** (rev 2: the `subeffect` 8223 *Spin* of 16625 *Redirect Attack* and 17091 *Powerful Wind*,
§2.4). Examples the gate can use (`m5e_vert.py`): **210266 swamp starturtle** (level 10, 7 spots, 16423 *Crouch* → `AlwaysBlockEffect`),
**210076 spy** (9, 14 spots, 16782 *Poison Sword* → `PoisonEffect`), 210128 long-legged arachna (11, 26 spots, 17037 *Deadly Poison*), 210354
veteran tursin healer (17, 16633 *Blindness*), 210183 / 210184 veteran tursin scout (17-18, 30 + 24 spots, 16625 → *Spin*). Every one is
`aggressive` with `prob="25"`.

**What else a level 10-20 character needs, and who gives it:** teleporters, flight paths, windstreams, obelisk binding (`ResurrectAI`,
`resurrect` on 2 Poeta and 5 Verteron npcs, no C++ file) — **M5f**; Verteron's XML quests and its 33 `simple_abyssguard` givers — **M5d**
(its D5); Daeva gear and loot — **M5b-3 / M5c**; crafting from level 10 — **M5c**; groups — **M5g**; the post-ascension missions (1007 / 2009 in
the capitals, the Verteron campaign) — **phase 6 after M5f**.

### 2.9 Status by area (measured)

| Area | Chunk | `AION_UNPORTED` on the path | Invisible bodies (lesson 1) | Java LOC |
|---|---|---|---|---|
| `ClassChangeService` | P5-08 | 7 | 0 (all 7 declared) | 166 |
| `PlayerReviveService::skillRevive` | P5-08 | 1 (of the file's 9) | 0 | ~20 |
| effect classes, stage 1 | P5-03 / P5-04 | 44 / 38 | 5 / 3 inner | 979 / 762 |
| `SkillLearnAction` | P5-07 | 0 today; 2 stubs after A-02a | 3 today (`canAct`, `act`, `validateClass`) | 71 |
| `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_USE_CHARGE_SKILL` | P5-16 | 0 | 4 (no file) | 75 |
| `_1205ANewSkill`, `_2132ANewSkill` | Q05 / Q09 | 0 | 6 (no file) | 270 |
| summons (stage 3) | P5-08, P5-01, P5-04, P5-05, P5-16 | 17 + 13 + 9 | 1 inner + 18 AI + 10 packet | ~1,400 |
| stigma (stage 3) | P5-07, P5-03, P5-04 | 10 + 10 + 9 | 2 + 3 inner | ~780 |
| **Total** | | **160** | **53** | **~4,500** |

**P5-08 in the roadmap's sense:** the chunk has **166** sites (measured, the roadmap's figure); M5e takes **25** (`ClassChangeService` 7,
`skillRevive` 1, `SummonsService` 13, `SummonRelease` 2, `TrapService` 2). The rest is M5c's (`DialogService` 7), M5f's (teleport 21 + 13 + 4,
`PlayerReviveService` kisk/instance), M5g's and M5j's. `SkillLearnService` — the roadmap's "skill learn" — is **already 0**.

**Undeclared Java methods on the path, measured** (`m5e_undeclared.py`): `ClassChangeService`, `SkillLearnService`, `StigmaService` (but for
`chargeStigma`'s anonymous task, 2, M5c's), `SummonsService`, `TrapService`, `SummonGameStats`, `SummonLifeStats`, `Summon`, `SummonController`,
`PlayerReviveService`, `PlayerCommonData`, `PlayerSkillList`: **0**. `SkillLearnAction` 3; the 7 client packets 2 each; the 3 AIs 18; the 2
trainer quests 6; the 57 effect classes 15 inner (§2.4).

### 2.10 What wakes up (lesson 2)

Traced from every entry point the milestone turns on — the enter world with the key, the journal dialog, the first Daeva level, the Daeva's
casts, its enter world, its packets, its dialogs, its movement — to the first unported or partial body. **W** = reached by M5e's own path, closed
or deliberately loud; **D** = dormant (a state the gate and the start maps do not produce), named for the next milestone.

| # | Reached from | First unported / partial body | Kind | Resolution |
|---|---|---|---|---|
| W-01 | enter world, key on, level ≥ 9 starting class | `ClassChangeService::showClassChangeDialog` (`ClassChangeService.cpp:7`) | W | C-01 |
| W-02 | `CM_DIALOG_SELECT(0, action, 1006 / 2008)`, key on | `changeClassToSelection` → `setClass` → `getSelectedPlayerClass` (`:11-33`) | W | C-01 (the packet: A-03a) |
| W-03 | `setClass` → `completeAscensionQuest` | `QuestState::setStatus` / `setQuestVar` / `setRewardGroup` (`QuestState.cpp:45-61`) | W | A-04a |
| W-04 | `setClass` → `upgradePlayer` for a player in a team / a legion | `standins::teamStatUpdaterAdd`; `LegionService::updateMemberInfo` (63 U in `LegionService.cpp`) | D | M5g / M5h |
| W-05 | the first Daeva level → `learnNewSkills(10, 10)` → 30003 / 40009 | `RecipeService::autoLearnRecipes` (`RecipeService.cpp:15`) | W | A-03b |
| W-06 | a Daeva casts a level 10-20 class skill, **or a skill it casts launches another** (subeffect, provoker, delayed skill, signet, aura, resurrect; §2.4) | the 36 (+5) classes of §2.4 | W | E-01, E-02 |
| W-07 | **a Gladiator reaching 15 online, or entering the world at ≥ 15** | `CondSkillLauncherEffect::applyEffect` / `startEffect` — **uncaught at enter world** (`PlayerEnterWorldService.cpp:663-671`); online it throws out of `onLevelChange` inside the kill reward | **W, blocker** | E-01; D7 orders it |
| W-08 | a Daeva switches a toggle off (14 toggles by 20) | `CM_TOGGLE_SKILL_DEACTIVATE` — no file: the client's packet is unknown, a mantra runs until logout | W | C-04 |
| W-09 | a Daeva releases a charge skill (6 by 20) | `CM_USE_CHARGE_SKILL` — no file: the charge never fires | W | C-04 |
| W-10 | **a Daeva at the Poeta / Ishalgen teleporter (203194, 203679, `func_dialogs="44"`)** — a non-Daeva got page `NO_RIGHT` | `TeleportService::showMap` (`TeleportService.cpp:284-285`) | **W, loud** | stays U until M5f; the checklist says so |
| W-11 | a Daeva glides (`CM_MOVE` & 0x04) | `FlyController::switchToGliding`, `PlayerLifeStats::triggerFpReduce`, the FP task (ported, never run) | W | X7 |
| W-12 | a Daeva flies (`CM_EMOTION` FLY) in a FLY zone | `FlyController::startFly` (ported) | D on the start maps (no FLY zone checked) | M5f |
| W-13 | a Daeva talks to one of the 47 "alternative dialog" npcs (12 Poeta + 35 Ishalgen, DialogPage.java:128-176) | `getStartPageId` → page 1352 (ported) | W | X7, X8 |
| W-14 | a Daeva's DP: kills, `setDp`, `SM_STATUPDATE_DP`, the DP skills, the 5-minute reset | ported | W | X8, X9 |
| W-15 | the item-cost skills 246 / 249 of every class at 10 | `Storage::decreaseByItemId` → `ItemPacketService` | W | A-02b |
| W-16 | 1699 on a dead player, then `CM_REVIVE(SKILL_REVIVE)` | `ResurrectEffect` (2 U); `PlayerReviveService::skillRevive` (`PlayerReviveService.cpp:44-45`) | W | E-02, C-02 |
| W-17 | `skillRevive` for a player in `ResPostState` | `TeleportService::teleportTo(player, world, instance, x, y, z)` (`TeleportService.cpp:251`) | D (needs `ResurrectPositionalEffect`, not in scope) | M5f |
| W-18 | a summon skill (Spiritmaster 10, Ranger 13, Cleric 15) | `SummonsService::createSummon`, `TrapService::registerTrap` (U); the three AIs (no file → `DummyNpcAI`, silent) | W | stage 3 |
| W-19 | a skill book | `SkillLearnAction::canAct` stub (A-02a) | W | C-03 |
| W-20 | a stigma stone equipped | `getPossibleStigmaCount` (U) via A-02d | W | stage 3 |
| W-21 | a Daeva in Verteron / Altgard | the 11 npc-skill classes of §2.8 | W | E-01, E-02, A-02c |
| W-22 | **the first player spawn in Verteron / Altgard in any automated run** (zones, `validateFortressZone`, region load) | unknown — no gate has spawned a player outside the start maps and Sanctum. **Measured so far** (`m5e_vert_ai.py`): the C++ tree registers three AIs (`aggressive`, `general`, `noaction`); Verteron's npcs with no C++ handler, which `DummyNpcAI` stands in for under `missing_ai_handlers=warn`, are `simple_abyssguard` 33, `quest_use_item` 21, `resurrect` 5, `useitem` 4, `portal` 3, `polorserin` 2 and one each of `postbox`, `naia`, `following`, `speaker`, `quest_start_use_item`; Altgard's `simple_abyssguard` 51, `quest_use_item` 20, `resurrect` 5, `portal` 3, `following` 2, `useitem` 2, `postbox` 1, `onedmg_passive` 1. The FACTION_JOIN / SEPARATE npcs (`func_dialogs="68 69"`, DialogService.java:226-231 → `NpcFactions::enterGuild`, ported, P4-12) are georg 799813 (Verteron) and gallum 799847 (Altgard), behind M5c's dialog | **unknown** beyond that | the gate lane measures it first (§13 item 3); every AI above is a `warn` line, not a throw |
| W-23 | `AuraEffect` of a player in a group | `PlayerGroup::getOnlineMembers` | D | M5g |
| W-24 | **C-05's registration: every level change of every character runs `_1205ANewSkill` / `_2132ANewSkill.onLevelChangedEvent`** (`registerOnLevelChanged`, `_1205ANewSkill.java:24-25, 35-71`), **including the enter world's offline level change** (`PlayerEnterWorldService.cpp:413`, after the connection is set, PlayerEnterWorldService.java:186, 204) | `QuestService::startQuest` (A-04b) — and a new `SM_QUEST_ACTION` in: the level-2 kill stream of **`gs.scenario.m5b` and `gs.scenario.m5d`**; the **enter-world burst of M5c's seeded Daeva** (C19 / X21a, level 1 → 10 offline, m5c-plan.md D5); and **the enter-world bursts of M5e's own B characters** (§10.2) | W | G-05 re-greens them; D7 merges the registration with the gate |
| W-27 | a hit that stumbles: 519's `subeffect` 8218, or a critical skill hit (`skillatkdraininstant` / `skillatk`) or auto-attack with a greatsword, polearm or staff (`SkillEngine.createCriticalProcEffect`, SkillEngine.java:193-218) | `StumbleEffect.calculate` → `GeoService.getClosestCollision` (ported, StumbleEffect.java:69) | W (ported, geo) | X9g |
| W-25 | the simple route for a level-1 character (no level check) | ported faithfully | D (Java quirk) | D11 |
| W-26 | an Ishalgen "alternative dialog" npc whose AI is unported (megin 203524, alfrigh 203543: `simple_abyssguard`, m5c-plan.md W-15) | `DummyNpcAI` | D | M5d D5 |

### 2.11 The client packets (lesson 4)

Measured against `network/aion/clientpackets`: **42 C++ `CM_*` classes of 188, so 146 have no file** (the roadmap's 148 minus M5b-2's two;
the task text's "147" is one off). What M5e must add:

| Need | Packet (opcode, `ClientPacketInfo.gen.inc` line) | Java lines | Chunk | Why |
|---|---|---|---|---|
| **R** stage 1 | `CM_TOGGLE_SKILL_DEACTIVATE` (0x00C5, :46) | 43 | P5-16 | W-08: a toggle cannot be switched off without it |
| **R** stage 1 | `CM_USE_CHARGE_SKILL` (0x018D, :194) | 32 | P5-16 | W-09 |
| **R** stage 3 | `CM_SUMMON_COMMAND` (0x015C, :112), `CM_SUMMON_MOVE` (0x016C, :173), `CM_SUMMON_EMOTION` (0x016D, :174), `CM_SUMMON_ATTACK` (0x016E, :175), `CM_SUMMON_CASTSPELL` (0x0190, :176) | 42 + 103 + 69 + 52 + 85 | P5-16 | a spirit is driven only by its master's packets |
| O | `CM_TITLE_SET` (0x012E, :126), `CM_BONUS_TITLE` (0x018C, :193) | 34 + 34 | P5-16 | titles come from quest rewards (1002, 1005, 2007 in `quest_data.xml`); `TitleList` has 1 U. M5d or M5j |
| entry criterion | `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG` (M5c); `CM_USE_ITEM`, `CM_EQUIP_ITEM` (M5b-3); `CM_DELETE_QUEST` (M5d) | – | – | A-02, A-03, A-04 |
| deferred with route R | `CM_PLAY_MOVIE_END` (0x0134, :88) | 57 | P5-16 | W in M5d; the ascension plays movies 14 and 151 |

**Server packets: none to write.** Decoders to write (G-02): `SM_ACTION_ANIMATION` (D target, H action id, D level-or-object,
SM_ACTION_ANIMATION.java:27-31), `SM_SKILL_REMOVE` (H skill, C level, C type, :23-26), `SM_STATUPDATE_DP` (H, :23-24), `SM_RESURRECT` (S name,
H skill, D 0, :25-28), `SM_MANTRA_EFFECT` (D 0, D effector, H sub-effect, :21-24), `SM_RIDE_ROBOT` (D, D, :25-27), `SM_FLY_TIME` (D, D, :20-22),
`SM_MESSAGE` (C chat type, C race, D sender, S name, S message, SM_MESSAGE.java:135-147), and for stage 3 `SM_SUMMON_PANEL` (:20-31),
`SM_SUMMON_PANEL_REMOVE`, `SM_SUMMON_UPDATE`. `SM_SKILL_LIST` with a message id, `SM_PLAYER_INFO` with its class id and
`SM_QUEST_COMPLETED_LIST` are already decoded (`decoders/PacketDecoders.h:257-265` `SkillList`; `:198-208` `PlayerInfo`, `classId` at :208 — not
`:121-141`, which is `PlayerInfoBlock`, the character-list block; `:267-280, 494` `QuestCompletedList`). **An effect on a monster reaches a
client only as `SM_ABNORMAL_EFFECT`, never as `SM_ABNORMAL_STATE`** (SkillDecoders.h:21-25), and `SM_MANTRA_EFFECT` never reaches its caster
(§2.4): X14 and X16 are written accordingly.

---

## 3. Where this plan disagrees with the roadmap and the sibling plans

| Source | This plan | Why |
|---|---|---|
| roadmap row 5: "Learn skills from trainers" | **no trainer exists**; the milestone gates the autolearn, ports the class masters' tutorial quest (D3) and skill books | §2.2 |
| roadmap: "class change" | **route S now (D1), route R after M5f** (D13) | §2.3: the ascension is an instance quest |
| roadmap: "chunks, mainly P5-08" | **P5-03 / P5-04 carry 110 of the 160 sites** (54 + 56); P5-08 25 | §2.9 |
| roadmap: "dialog" in M5e | **M5c** (m5c-plan.md §3, D4) | agreed; A-03a |
| roadmap order M5e → M5f | kept; route R and the stigma quests are named as M5f's (D13) | a reorder is the user's call (D1 option b) |
| m5b2-plan.md O-05: summons to "a summons milestone" | **M5e stage 3** (D5) | the Spiritmaster's level-10 kit; no roadmap row owns summons |
| m5b3-plan.md O-02: stigma to M5c | **M5e stage 3, optional** (D6); **m5c-plan.md rev 2 agrees**: its §3a hands `StigmaService` (11 + 2 inner) and `SkillLearnAction` (3) to M5e (m5c-plan.md:399) and marks the stigma arm of `CM_MANASTONE` M5e's (W-21, :306) | the service and the effect classes are in M5e's lanes |
| m5b3-plan.md rev 2 E-02 / E-03 | agreed and assumed (A-02c): `ProcAtkInstantEffect` (R there) and `MpAttackInstantEffect` (W there), which rev 2 of this plan found on M5e's path through launched skills, stay M5b-3's; M5e takes them only if M5b-3 did not | one port, not two |
| m5b2-plan.md §2.4: "four starting classes" | **six**: ENGINEER and ARTIST are starting classes (`player_initial_data.xml:77, 95`; `CM_CREATE_CHARACTER.java:92`); their level 1-10 skills are all ported already | §2.1 |
| m5d-plan.md D-02: the `ENABLE_SIMPLE_2NDCLASS` arm "W, M5e" | agreed; M5e turns it on in its profile | D1, D9 |

---

## 4. Decisions

Decisions the integrator takes under the standing instruction unless marked **user**.

| # | Decision | Why |
|---|---|---|
| **D1** | **user** — **How a player changes class in M5e.** (a) **recommended**: the Java server's simple route, `gameserver.simple.secondclass.enable = true` in the M5e gate profile and the real-client profile; the retail missions 1006 / 2008 (+ 1007 / 2009) become a named M5f work item (D13). (b) swap M5e and M5f in the roadmap and port route R in M5e. (c) keep the order and pull the solo-instance subset of the instance engine into M5e. **Until the user answers, the plan proceeds with (a)**, because every body of (a) is also on the path of (b) and (c): `setClass`, `getClassSelectionDialogPageId` and the Daeva's whole skill set are what route R needs too (`_1006Ascension.java:105, 238-244`). | §2.3. (a) is a Java code path, not an invention, but it changes what a real player sees (a window at login instead of a quest), which is why it is the user's |
| **D2** | **No trainer is invented.** "Learn skills" is gated as the autolearn on level change (X2, X7), the class masters' quest (D3, X1) and a skill book (X16). | §2.2 |
| **D3** | **`_1205ANewSkill` and `_2132ANewSkill` are pulled forward from phase 6** into M5e under leases of Q05 / Q09, as m5d-plan.md D5 pulled a 75-line AI forward. They are the first two entries of the `AION_QUEST_HANDLER` registry. | 270 Java lines; they need only M5d's helpers (A-04b); they are the one "talk to your trainer" moment of 4.8; and they prove the phase-6 handler path (registration order, `putIfAbsent`, the library link, §8 risk 7) a milestone early |
| **D4** | **The effect scope is closed at level 20**: the 11 advanced classes' autolearn skills of levels 9-20, the skill books of level ≤ 20, the monsters of Verteron and Altgard, and (stage 3) the stigma skills of level 20 and the pet skills whose order skill is learned by 20 — **each set closed under the skills its effects launch** (§2.4). **Every other effect class stays `AION_UNPORTED` and throws** (m5b2-plan.md D6). | Level 20 is where the first stigma slot, Eltnen and Morheim begin; §2.4's table shows the cost growing past it |
| **D5** | **Summons are M5e's, in stage 3.** | the Spiritmaster's level-10 kit is two summons; the Cleric's 15 and the Ranger's 13 are summons; `cycles.toml` already specifies the lifecycle (§2.5) |
| **D6** | **Stigma's service and its 10 stigma-only effect classes are an optional stage-3 lane; recommended taken.** Its gameplay reaches a real player only after M5f (quests 1929 / 2900 in the capitals, D13); the gate seeds the quest (D8). | the lanes that port the classes are open anyway; deferring spreads P5-03 / P5-04 work over two more milestones |
| **D7** | **Commit order** (the M5b-2 D11 rule: every commit green): (1) E-01 / E-02 before any gate profile turns the key on or seeds a Daeva ≥ 15 — **`CondSkillLauncherEffect` is a blocker, not a feature**; (2) C-01 may land at any time, because the key is off in every existing profile; (3) **C-05's two `AION_QUEST_HANDLER` entries are written and unit-tested in stage 1 but held out of the stage-1 merges; the integrator merges them in stage 2, in one commit with G-03 (whose X1 needs them) and G-05 (the re-green of every gate whose characters change level, W-24)**, as m5d-plan.md D3 did for the XML registration. Rev 1 put C-05 in stage 1 and "last, with G-05", which made stage 1 unable to close before stage 2 ended. | W-07, W-24 |
| **D8** | **The gate seeds what M5e does not own**, with `ScenarioDatabase::execute` while the character is offline: `players.exp` (**`players.old_level` is left as the server stored it at leave world**, §2.1), `players.player_class` + a `player_quests(1006, 'COMPLETE')` row for the B-account Daevas (m5c-plan.md D5's recipe), `players.dp`, `player_life_stat.hp`, a `player_quests(1929, 'COMPLETE')` row for stigma; **positions** (`players.x/y/z/world_id`), all chosen by the oracle: beside **kunandes** 203087 (Poeta spawn file :759-760, 840.494 / 1217.09) for C4, beside **Pernos** 790001 (:926-927, 241.094 / 1639.46) for C6-C11 — both within talk range, which `CM_SHOW_DIALOG` and `CM_DIALOG_SELECT` check (NpcController.java:254, 267) — the observer's spot beside A1 (C6-C10, C16), Verteron; **item rows** above `gameserver.idfactory.wrap_at` (a book, a stigma stone, Odella powder) and **two equipped weapons**: for A1, after C8, a **greatsword** in the main hand in place of the Training Sword's equipped row (its equip skill 51 comes with the class change; without it `Equipment.onLoadHandler` puts the weapon back into the cube, Equipment.java:303-314, 436-440; ItemGroup.java:17), and for B3 a **keyblade** in place of the Pistol for Training (its equip skill 112 is an ENGINEER level-1 skill, `skill_tree.xml:112`; ItemGroup.java:29). The oracle names both item ids and B3's robot id. | levelling to 20, travel, the capital quests and equipping by packet (A-02d) are other milestones' features; the precedents are m5b-plan.md D12, m5b2-plan.md D3, m5c-plan.md D5 |
| **D9** | **Gate profile** = the M5d profile + `gameserver.simple.secondclass.enable = true` + **`gameserver.rates.xp.quest = 1.0, 2.0` pinned** (the default, RatesConfig.java:36; applied without a cap by `Rates.XP_QUEST`, Rates.java:29-33, from QuestService.java:224-226, so X1's reward is exact for a membership-0 account), written as `game-server/config/m5e.properties.example` in the Java tree, as `m5b.properties.example:52-54` pins the solo rate. | D1 (a); X1 |
| **D10** | **Exact assertions**: levels, exp values and thresholds, skill ids, message ids, page ids, action ids, class ids, quest status bytes, DP cost, kinah cost, the toggle's tick period (6,500 ms), the charge thresholds (`skill_charge.xml`). **Bounds**: damage, heal, drain, the npc cast's probability. | m5b2-plan.md D8's split |
| **D11** | **The simple route's missing level check is ported as Java has it** and recorded in `docs/deviations/P5-08.md` as Java behaviour; a unit test names it. | faithfulness (m5b2-plan.md D9); W-25 |
| **D12** | **New gate `gs.scenario.m5e`** (+ `_geo`, which has a row of its own, X9g: the gate's 519 and its greatsword criticals stumble the target through the geo-reading `StumbleEffect`, W-27) in the same binary and under the same `RESOURCE_LOCK`; every earlier gate is kept and re-run. | the M5b-2 D10 argument; rev 1 thought nothing on the path read geo |
| **D13** | **Route R and the stigma quests are recorded as M5f's**: `_1006Ascension`, `_2008Ascension`, `_1007ACeremonyinSanctum`, `_2009ACeremonyinPandaemonium` (Q06, 25 bodies, 974 lines) and `_1929ASliverofDarkness`, `_2900NoEscapingDestiny` (Q11 / Q10, 21 bodies, 580 lines). **Recommendation to the M5f planner: make Karamatis B the instance engine's first gate.** | a solo instance, a flight teleport and a boss fight in one quest the player already has |

---

## 5. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional. The
**Assumes** column names the §0 rows an item stands on.

### Integrator

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | The Q05 / Q09 leases for C-05 (one file each), and **a test home for them**: Q05 and Q09 have no `TESTS` directory (`chunks.cmake:472-491`) — either a `TESTS` keyword or the tests in `tests/quest` (P5-06) under a lease. **The check (§8 risk 7):** after C-05 is merged, `aion_gs_regscan`'s quest table (`Registry.*.gen.cpp`) lists `_1205ANewSkill` / 1205 and `_2132ANewSkill` / 2132, and `QuestEngine::init` creates both — it throws on a quest-id mismatch (`QuestEngine.cpp:102-108`). Registration is by regscan's constinit tables, not static initialisers (HandlerRegistry.h:33-41; the server links `aion_gs_handlers` and `aion_gs_registry`, `game-server/CMakeLists.txt:95-106`), so a marker in the wrong namespace is a link error, not a silent absence. | – | R | S |
| I-02 | `game-server/config/m5e.properties.example` in the Java tree (D9, with the quest-XP pin). | – | R | S |
| I-03 | Re-run the §12 scripts at branch time and replace §2.4's lists with their output (A-01, A-02c) — **including the launched-skill closure** (`m5e_refs.py`) and the pet skills (`m5e_pets.py`). | – | R | S |
| I-04 | **E-03 moved here**: the `cycles.toml` / `fieldmap.toml` review for the bodies E-01 / E-02 write. `generated/concurrency/cycles.toml` has **no chunk owner** (`chunks.py owner`: "no owner"), so two parallel lanes cannot share it; the effect lanes send the integrator their rows. | E-01, E-02 | R | S |
| I-05 | **Merge C-05 with G-03 and G-05 in one commit** (D7 (3)). | C-05, G-03, G-05 | R | S |

### Stage 1 — the class change and the Daeva

| Id | What | Java refs | Deps | Assumes | Need | Eff |
|---|---|---|---|---|---|---|
| **C-01** | **`ClassChangeService`** — all 7 bodies (`ClassChangeService.cpp:7-33`). Tests in `tests/playersvc`: `getSelectedPlayerClass` for both races × every `SELECT*` id + an unknown id (null); `getClassSelectionDialogPageId` for 6 classes × 2 races + an advanced class (0); `setClass`'s `validate` table (starting → +1, +2 accepted; +0, +3, another family, an advanced class refused, with the two messages); `updateDaevaStatus` true/false; the D11 level-1 case. | ClassChangeService.java:23-164 | – | A-04a | R | M |
| **C-02** | `PlayerReviveService::skillRevive` (the `getResStatus` audit, `revive(35, 35, soul sickness, skill)`, `STR_REBIRTH_MASSAGE_ME`, the fly-before-death arm, the prison and ResPost arms). Test in `tests/playersvc`. | PlayerReviveService.java:42-62 | – | – | R | S |
| **C-03** | **`SkillLearnAction`**: `canAct`, `act`, `validateClass` (`.cpp`-local). Test in `tests/itemsvc`: the four refusals (level, class incl. the starting-class rule, race, known), and the learn. | SkillLearnAction.java:31-69 | – | A-02a, A-02b | R | S |
| **C-04** | **`CM_TOGGLE_SKILL_DEACTIVATE`** (readUH skill, two readH; the non-toggle/non-stance audit; `removeEffect`; `stopStance`) and **`CM_USE_CHARGE_SKILL`** (empty read; the casting-skill and `isCharge` checks; `useChargeSkill(skill, now − castStartTime)`), with `AION_CLIENT_PACKET` markers, byte vectors and run tests in `tests/cm_lz` over `InWorldPacketRunSupport.h`. | CM_TOGGLE_SKILL_DEACTIVATE.java:23-42; CM_USE_CHARGE_SKILL.java:19-32 | – | A-01 | R | S |
| **C-05** | **`_1205ANewSkill`, `_2132ANewSkill`** (register, `onLevelChangedEvent`, `onDialogEvent`) under the I-01 leases; `AION_QUEST_HANDLER(_1205ANewSkill, 1205)` etc. **Written and unit-tested in stage 1; merged in stage 2 with G-03 and G-05 (D7 (3), I-05).** | `data/handlers/quest/poeta/_1205ANewSkill.java`, `…/ishalgen/_2132ANewSkill.java` | I-01 | A-04b | R | S |
| **E-01** | **P5-03, 21 classes, 44 sites + 5 inner** (§2.4's table) with tests in `tests/effects_al` on M5b-2's `EffectClassTestSupport.h`: per class `calculate` → `applyEffect` → `startEffect` → `endEffect`, the abnormal state set and unset, the stat function added and removed, **the observer attached and removed** (the inner callbacks), and `AuraEffect`'s periodic task cancelled by `endEffect` on a `DeterministicExecutor`. `CondSkillLauncherEffect` first (D7). If A-02c failed: + `BlindEffect`. | skillengine/effect/[A-L]*.java | – | A-01, A-02c | R | L |
| **E-02** | **P5-04, 15 classes, 38 sites + 3 inner** — rev 1's 11 plus **`PulledEffect` 4, `SignetEffect` 2, `SimpleRootEffect` 4, `SpinEffect` 4**, reached only through launched skills (§2.4) — tests in `tests/effects_mz`; mutation-proven for the arithmetic ones (`SkillAtkDrainInstantEffect`, `MPHealInstantEffect`, `OneTimeBoostSkillAttackEffect`) and for `ResurrectEffect.calculate`'s `isDead` guard; `PulledEffect` and `SimpleRootEffect` read geo (G-04's list) and get a `tests/geo`-fixture case each. If A-02c failed: + `PoisonEffect`, `SilenceEffect`, `ParalyzeEffect`, **`ProcAtkInstantEffect`** (19 sites with `BlindEffect`'s 4 in E-01). | skillengine/effect/[M-Z]*.java | – | A-01, A-02c | R | M |
| **E-03** | *(moved to the integrator, I-04)* `cycles.toml` / `fieldmap.toml` review: `AuraEffect.AuraTask.effect` (`cycles.toml:298`), `CondSkillLauncherEffect$1` (`:301-302`), `RideRobotEffect$1` (`:321`) are the specification; add rows for any inner callback the report gains. | cycles.toml | E-01, E-02 | – | R | S |
| **G-01** | **`tools/oracle` command `m5e-progression --race R --class C [--from-level A --to-level B] [--new-class X] [--daeva] [--known-skills FILE] [--npc ID --map ID] [--item ID]`**: the thresholds `startExp(L)` and the non-Daeva cap; `learnNewSkills(from, to)` for a class **with the Daeva rules** (extend `m5a/creation.py`'s `learn_new_skills`, which refuses `to_level ≥ 10` today, `creation.py:219-221`), with **per skill the `SM_SKILL_LIST` message id** from `PlayerSkillList.addSkill`'s `isNew` rule — **a port of `SkillTreeData.getHighestSkill` + `getTemplatesForSkill` + `createSkillTree` for the player's class at the time of the add** (SkillTreeData.java:94-162; §2.1), cross-checked against `m5e_isnew.py`'s table, and **optionally seeded with the character's actual skill list** (`--known-skills`, read from the enter-world `SM_SKILL_LIST`) rather than an assumed history — and `SkillLearnService.sendPacket`; the 30001 → 30002 swap; the class-change page ids and action ids; the base `maxHp` / `maxMp` of a class at a level (`max_hp` / `max_mp` of `creation.py:145-178` with the class multipliers); per skill the `m5b2-skills` constants (cast, DP / MP / item cost, effect classes, durations) plus the charge thresholds of `skill_charge.xml` **and the launched skills' ids** (1699 → 8296, 1809 → 8998 with its `duration2`, 519 → 8218); **per cast the gate plans, its start and use conditions evaluated against the seeded character** (weapon group, chain category / precategory, `ride_robot`, `dp`, `target`) — a refused cast fails the oracle, not the gate; **the weapons of D8** (a greatsword whose equip skill A1 has after C8; a keyblade and its `robot` id for B3); per npc its spots, level and skills; per item a book's skill or a stigma stone's skill group and price; the kill, talk and observer spots of D8. `tools/oracle` tests. | – | – | – | R | M |
| **G-02** | `GameSession` builders: `CM_DIALOG_SELECT` with target 0 (reuse M5c/M5d's), `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_USE_CHARGE_SKILL`, `CM_MOVE` with the glide bit; decoders `tests/scenario/decoders/ProgressionDecoders.{h,cpp}` for the eight packets of §2.11 (written from `writeImpl`, no `serverpackets/` include, body consumed exactly) + `ProgressionDecodersTest.cpp`. | – | – | A-04c | R | M |

### Stage 2 — the gate

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-03** | `TEST(M5eScenario, Run)`: the cases of §10, output `<bin>/scenario/m5e`, schemas `aion_{ls,gs}_test_m5e_<hash>`, the same `RESOURCE_LOCK`, `tests/scenario/m5e_partial_allowlist.txt`, registration `gs.scenario.m5e` in `ScenarioTests.cmake`. Developed against a tree with C-05 applied; merged with it (I-05). | stage 1 (C-05 written) | R | L |
| **G-04** | `gs.scenario.m5e_geo`: the same script with geo on, **and row X9g** (in the gate lane after G-05). The geo readers in `skillengine/effect` (grep `GeoService`) are `BackDashEffect`, `ConfuseEffect`, `DashEffect`, `FearEffect`, `MoveBehindEffect`, `OpenAerialEffect`, `PulledEffect`, `RandomMoveLocEffect`, `SimpleRootEffect`, `StaggerEffect`, `StumbleEffect`, `SummonServantEffect`, `SummonTotemEffect`, `SummonTrapEffect`, `TargetTeleportEffect`. **On the scripted path: `StumbleEffect`** (ported), through 519's `subeffect` 8218 and the greatsword's critical proc (W-27) — rev 1's "only the dash family reads geo, and the gate casts none" was wrong. Of stage 1's classes, Dash, BackDash, RandomMoveLoc, Pulled and SimpleRoot read geo but the gate casts none of them: each gets a `tests/geo`-fixture unit case (E-01, E-02). | G-03 | R | S |
| **G-05** | **Re-green `gs.scenario.m5a`, `m5b`, `m5b2`, `m5b3`, `m5c`, `m5d`** — in the gate lane, right after G-03 (one P5-SC lane, serialized), merged with C-05 and G-03 by I-05. Two things move: (a) C-05 registers 1205 / 2132, so **every Elyos character's change to level ≥ 2 (Asmodian ≥ 3) now starts a quest and sends `SM_QUEST_ACTION`** (W-24): the M5b-1 fight reaches level 2 (m5b-client-session.md: 400 exp); M5d's Y11 reaches level 2 inside `finishQuest`; **M5c's seeded Daeva (C19 / X21a, level 1 → 10 offline) gets it in its enter-world burst**; and so do M5e's own B characters (§10.2); (b) the new effect classes change nothing else on those gates' paths (no Daeva cast, no level ≥ 10 skill) — **verify, do not assume**. Record before/after packet counts in the wave report. | G-03, C-05 | R | M |

### Stage 3 — summons and stigma

| Id | What | Java refs | Deps | Assumes | Need | Eff |
|---|---|---|---|---|---|---|
| **M-01** | `SummonsService` (13, incl. `ReleaseSummonTask` ×4), **`SummonRelease` (2: `isCancelableByMaster`, `cancel`)** and `TrapService` (2), P5-08; tests in `tests/playersvc`: create / release / the modes, the release races of `Summon.registerRelease` on a `DeterministicExecutor` (a scheduled release superseded by an instant one, a started release that cannot be cancelled, `cancelReleaseByMaster` from `doMode`, `isReleaseUncancelable` from `SummonController.onAttack`), and the `Player.summon` ↔ `Summon.master` cut (`cycles.toml:135, 161`). | SummonsService.java:1-220; SummonRelease.java; Summon.java:210-240; TrapService.java | – | – | R | M |
| **M-02** | `SummonGameStats` (12), `SummonLifeStats` (1), P5-01; tests in `tests/stats` against the Java arithmetic. | SummonGameStats.java; SummonLifeStats.java | – | – | R | M |
| **M-03** | `SummonEffect`, `SummonHomingEffect` (+1 inner), `SummonServantEffect`, `SummonTrapEffect`, `PetOrderUseUltraSkillEffect`, **`SpellAtkDrainInstantEffect`** (the earth spirit's 22197 on order 3837, §2.5 — moved here from T-02 so that declining D6 leaves no throwing order), P5-04; `tests/effects_mz`. **Lesson 2 for stage 3**: re-run `m5e_pets.py` (the pet skills whose order skill is learned by 20, closed under launched skills) at branch time. | skillengine/effect/Summon*.java, SpellAtkDrainInstantEffect.java | M-01 | A-01 | R | S |
| **M-04** | `ServantNpcAI`, `TrapNpcAI`, `HomingNpcAI` (new files, `AION_AI(…, "servant" / "trap" / "homing")`), P5-05; `tests/handlers_ai_core`. | data/handlers/ai/{Servant,Trap,Homing}NpcAI.java | M-01 | – | R | M |
| **M-05** | `CM_SUMMON_COMMAND`, `CM_SUMMON_MOVE`, `CM_SUMMON_EMOTION`, `CM_SUMMON_ATTACK`, `CM_SUMMON_CASTSPELL` (10 bodies), P5-16; byte vectors + run tests in `tests/cm_lz`. | clientpackets/CM_SUMMON_*.java | M-01 | – | R | M |
| **M-06** | The summon cases of §10 (X17), decoders `SM_SUMMON_PANEL` / `_UPDATE` / `_PANEL_REMOVE`; **G-06**: `CheckOutput::zeroLiveClasses` += `Summon` (P5-14 lease or its owner). | – | M-01..M-05 | A-07 | R | M |
| **T-01** | `StigmaService`: `removeLinkedStigmaSkills`, `getLinkedStigmaLearnSkill`, `isEquipped` ×2, `getPossibleStigmaCount`, `isCompleteQuest`, `getPossibleAdvancedStigmaCount`, `isPossibleEquippedStigma`, `addStigmaSkills`, `removeStigmaSkills` (10), P5-07; `tests/itemsvc`. | StigmaService.java:134-307, 326-445 | – | A-02d, A-04a | O (D6) | M |
| **T-02** | The 10 stigma-only classes: P5-03 `CaseHealEffect` 5+1, `ConvertHealEffect` 2, `DispelBuffEffect` 1, `HealCastorOnAttackedEffect` 2+1; P5-04 `MagicCounterAtkEffect` 2+1, **`MpAttackInstantEffect` 2** (2825 *Leeching Steel* → provoker 9011; only if M5b-3's E-03 left it W, A-02c), `OneTimeBoostSkillCriticalEffect` 2+1, `SummonSkillAreaEffect` 1+1, `SwitchHostileEffect` 1, `SwitchHpMpEffect` 1 (`SpellAtkDrainInstantEffect` moved to M-03). | – | M-01 (skill area) | A-01, A-02c | O (D6) | M |
| **T-03** | The stigma case of §10 (X18). | – | T-01, T-02 | A-02d | O | S |

### Deferred

| Id | What | Milestone |
|---|---|---|
| O-01 | Route R: `_1006Ascension`, `_2008Ascension`, `_1007ACeremonyinSanctum`, `_2009ACeremonyinPandaemonium` (D13) | **M5f** (end) |
| O-02 | The stigma slot quests `_1929ASliverofDarkness`, `_2900NoEscapingDestiny` | after M5f (phase 6 or M5f-end) |
| O-03 | Obelisk binding (`ResurrectAI`, 3 bodies + 1 inner) | M5f |
| O-04 | `CM_TITLE_SET`, `CM_BONUS_TITLE`, `TitleList`'s last body | M5d or M5j |
| O-05 | `PlayerReviveService` rebirth / item / kisk / instance revives (8) | M5f / M5j |
| O-06 | Effect classes of levels 21+ and of the maps past Verteron / Altgard (Eltnen 24, Morheim 25 unported classes, §12) | M5f and after, as the classes a real player reaches |
| O-07 | `AuraEffect`'s team arm (W-23) | M5g |
| O-08 | `StigmaService.chargeStigma` (+2 inner) | M5c (the `CM_MANASTONE` arm, m5b3-plan.md D8) |

---

## 6. Lanes

At most 6 lanes per stage; chunks disjoint within a stage.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 1 | **progression** | P5-08, P5-07, P5-16, Q05 + Q09 (leases, I-01) | C-01..C-04; C-05 written and unit-tested, **not merged** (D7 (3)) | `tests/playersvc`, `tests/itemsvc`, `tests/cm_lz`, the I-01 test home |
| 1 | **effects-al** | P5-03 | E-01 (sends its `cycles.toml` rows to I-04) | `tests/effects_al` |
| 1 | **effects-mz** | P5-04 | E-02 (sends its `cycles.toml` rows to I-04) | `tests/effects_mz` |
| 1 | **gate-harness** | P5-SC, `tools/oracle` | G-01, G-02 | `tools.oracle`, decoder self-tests |
| 1 | *integrator* | `cycles.toml` / `fieldmap.toml` (no chunk owner) | I-01..I-04 | – |
| 2 | **gate** — the stage's **only** P5-SC lane | P5-SC, `tools/oracle` | G-03, then G-05 (both merged with C-05 by I-05), then G-04, serialized | `gs.scenario.m5e`, `_geo`, the six earlier gates, `gs.smoke.startup` |
| 2 | **fixups** | whichever stage-1 chunks the gate names (never P5-SC) | – | owning tests + gate |
| 2 | *integrator* | Q05 / Q09 (leases) | I-05: C-05 + G-03 + G-05 in one commit | – |
| 3 | **summon-core** | P5-08, P5-01 | M-01, M-02 | `tests/playersvc`, `tests/stats` |
| 3 | **effects** | P5-03, P5-04 | M-03, T-02 | `tests/effects_al`, `tests/effects_mz` |
| 3 | **summon-ai** | P5-05 | M-04 | `tests/handlers_ai_core` |
| 3 | **summon-packets** | P5-16 | M-05 | `tests/cm_lz` |
| 3 | **stigma** (O) | P5-07 | T-01 | `tests/itemsvc` |
| 3 | **gate-3** | P5-SC, P5-14 (lease) | M-06, G-06, T-03 | `gs.scenario.m5e` |

**Notes.** Stage 1 is four lanes and could run with fewer agents: `progression` is five S/M items; the long pole is `effects-al` (21 classes,
`AuraEffect`'s periodic task and `CondSkillLauncherEffect`'s observer pair), with `effects-mz` (15 classes, two geo readers) close behind.
**Merge order:** stage 1 — E-01's `CondSkillLauncherEffect` first → the rest of E-01 / E-02 → C-01..C-04 → G-01 / G-02 in parallel → I-04;
**stage 1 closes without C-05**. Stage 2 — the gate lane develops G-03 on a tree with C-05 applied; **I-05 merges C-05 + G-03 + G-05 as one
commit** (G-05's re-green is part of it, because C-05 changes the earlier gates' packet streams, W-24); G-04 follows. Rev 1 had C-05 "last in
stage 1, with G-05", a stage-1 item waiting for a stage-2 one, and two lanes on P5-SC in stage 2. In stage 3 the `effects` lane merges M-03
before M-06 needs it; `summon-core` is the long pole.

---

## 7. Header requests expected

Bodies never need a request (hub-headers.md §14).

| Request | Kind | For |
|---|---|---|
| **None for the effect classes**: the 57 classes declare every Java method except the 15 inner bodies (14 callbacks + `AuraTask`), which are `.cpp`-local callback structs (m5b2-plan.md §7's rule); the six rev 2 added declare all theirs (measured) | – | E-01, E-02, M-03, T-02 |
| `SkillLearnAction.h`: **none if A-02a landed** (M5b-3's h01 declares `canAct` / `act` on all 32 bound classes); otherwise the two `override` declarations, additive | additive or none | C-03 |
| New files, no request: `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_USE_CHARGE_SKILL`, `CM_SUMMON_*` ×5 (P5-16); `ServantNpcAI`, `TrapNpcAI`, `HomingNpcAI` (P5-05 `handlers/ai`); `_1205ANewSkill`, `_2132ANewSkill` (leased Q05 / Q09) | new files | C-04, C-05, M-04, M-05 |
| `ClassChangeService.h`, `PlayerReviveService.h`, `SummonsService.h`, `TrapService.h`, `SummonGameStats.h`, `SummonLifeStats.h`, `StigmaService.h`: **none** (0 undeclared, measured) | – | C-01, C-02, M-01, M-02, T-01 |
| P5-14 `CheckOutput`: `zeroLiveClasses` += `Summon` | additive | G-06 |
| Manifest: a `TESTS` home for the leased quest handlers (I-01) | build | C-05 |
| `game-server/config/m5e.properties.example` (Java tree) | none | I-02 |

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence for each.

1. **D1 is a real fork, and the user may answer (b).** Route R needs M5f's instance engine and flight teleport (§2.3). If the user wants the
   retail ascension in M5e, M5e either moves after M5f or pulls the solo-instance subset forward (`InstanceService` 12 sites, the instance
   `teleportTo` overloads, the flight path) — a milestone of its own. The plan's stage 1 is useful under every answer; its gate profile
   (the key) is the part that changes.
2. **The Daeva wake-up is immediate and broad** (§2.10). The first gate that seeds or makes a Daeva switches on gliding, DP, page 1352, the
   teleporter's `showMap` arm, the craft autolearn (A-03b) and the level-10 class skills at once. **W-07 is the one that fails silently in a
   real session**: a Gladiator who reaches 15 before E-01 lands cannot log back in. D7's order is the mitigation; the gate's X10 is the check.
3. **Summons are a new object lifecycle under refcounting.** A `Summon` and its master reference each other (`Summon.master`, `Player.summon`),
   a release is a scheduled task that may race the master's own release (`Summon.registerRelease`), and the servant / trap / homing npcs live on
   timers (`SummonServantEffect` schedules the delete). `cycles.toml` specifies every cut (§2.5); **no gate has ever spawned a Summon**, so the
   census row of G-06 and M-01's race tests are the only checks.
4. **The oracle's `isNew` model decides a dozen exact assertions — and rev 1 got it wrong twice.** `PlayerSkillList.addSkill` computes `isNew`
   from `SkillTreeData.getSkillsForSkill`, which starts at the **highest skill of the stack** for the **player's current class**
   (PlayerSkillList.java:65-73; SkillTreeData.java:94-162); rev 1 read the skill's own `skillLearn` and claimed message 0 for 2865 / 2878, and
   its C5 seed (`old_level` 8) skipped levels 3-8, which would have made 2891 new at 10 (§2.1). A wrong oracle makes a right server fail X2 / X5
   / X7. G-01 ports the walk exactly, can take the character's actual skill list as input (`--known-skills`), and is cross-checked against
   `m5e_isnew.py` and the M5a gate's enter-world skill list.
5. **The invisible bodies, again.** 53 of ~213 bodies (25 %) have no `AION_UNPORTED` site today: 15 inner bodies, 18 AI bodies, 14 packet
   bodies, 6 quest bodies. A lane sized from `grep -c` under-counts `effects-al` by 5 and stage 3 by 29. **And the site counts themselves
   understated the effect scope by 16 sites (rev 1) until the launched skills were followed** (§2.4): a closure over `skill_tree` alone is not
   lesson 2.
6. **Two "0-unported" packets throw on construction** — `SM_SUMMON_PANEL`, `SM_SUMMON_UPDATE` read `SummonGameStats` (12 U). Any summon test
   that builds them before M-02 lands throws in the packet constructor, not in the service.
7. **C-05 is the first phase-6 quest handler to hold a source.** Registration is not by static initialisers: `aion_gs_regscan` scans the
   `AION_QUEST_HANDLER` markers into constinit tables that reference each handler's factory (HandlerRegistry.h:33-41), the server links
   `aion_gs_handlers` and `aion_gs_registry` (`game-server/CMakeLists.txt:95-106`), and `QuestEngine::init` instantiates every entry and checks its quest id
   (`QuestEngine.cpp:102-108`). A misplaced marker is therefore a **link error**, not a silent absence. What can still go wrong is a Q05 / Q09
   source the manifest does not list (regscan never sees it). I-01's check: regscan's quest table lists the two entries, and `QuestEngine`
   creates both.
8. **The real 4.8 client and route S.** `SM_DIALOG_WINDOW(0, 2375, 1006)` outside the quest's own dialog is what the Java server sends; nobody
   here has seen a 4.8 client draw it. If it does not, route S is unusable from a real client even though the gate is green (§11 step 4 finds
   out; §13 item 1).
9. **`AuraEffect`'s 6.5-second task holds an `Effect`** (`cycles.toml:298`) and runs `SkillEngine.applyEffect` on another thread every tick; a
   mantra left on at logout is the leak shape of m5b2-plan.md §8 item 2. E-01's executor test and X14's `CM_TOGGLE_SKILL_DEACTIVATE` are the
   checks.
10. **Stats after a class change are only partly modelled.** The oracle models the base template (`PlayerClass.createStatsTemplate`, the
    health multiplier 400 → 440 for a Gladiator, PlayerClass.java:16-17); it does not model passives (`creation.py:160`), and after a class
    change both the starting masteries and the advanced ones apply (different stacks, §2.3). X2 and X5 assert the base only.
11. **Gate runtime.** Two accounts, up to seven characters, ~13 enter worlds, a 28-second mantra watch, a DoT, a death and a resurrection:
    budget **170-220 s**, keep the lock; stage 3 adds ~40 s.
12. **Every data number here is a throw-away parse** (m5b2-plan.md risk 14). G-01 takes them over before a gate asserts one.
13. **A gate cast that a correct server refuses is a gate that teaches the port to be wrong.** Rev 1 planned 758 without its chain opener or a
    greatsword and 2606 without the robot, and asserted a mantra packet its caster never receives and an `SM_PLAYER_INFO` its subject never
    receives (§14). Each would have failed against Java and invited a lane to "fix" the port. G-01's condition check (every planned cast
    evaluated against the seeded character) and the observer character (§10.1) are the structural answers; the gate lane reads the Java
    send path (`sendPacket` / `broadcastPacket` / `broadcastPacketAndReceive` / `toSelf`) for every packet it asserts at a given client.

---

## 9. The split: three stages, and why in this order

| Stage | What a player can do at the end | Chunks | Bodies | Lanes |
|---|---|---|---|---|
| **1 — the class change and the Daeva** | Reach 9 with the right skills; hit the level-9 wall; change class (route S); become a Daeva, reach 10-20, use the new class's skills incl. DP skills, chains, toggles, the robot and charge skills, resurrection and books; glide; fight Verteron / Altgard monsters. (The class master's tutorial is written here and goes live with the gate, D7 (3).) | P5-08, P5-07, P5-16, P5-03, P5-04, Q05 / Q09 (lease), P5-SC | **~111** (92 sites + 19 invisible) | 4 + integrator |
| **2 — the gate proves it** | the same, proven, plus the class master's tutorial; the earlier gates green again | P5-SC (one lane), Q05 / Q09 (integrator) | ~0 + fixups | 1 + fixups |
| **3 — summons (and stigma)** | a Spiritmaster summons and commands its spirits, a Cleric's servant heals, a Ranger's traps fire; (O) a level-20 character equips a stigma | P5-08, P5-01, P5-04, P5-05, P5-16, P5-03, P5-07, P5-14, P5-SC | **~102** (68 + 34) | 6 |

1. **Stage 1 before summons**, because summons are the only part with a new object lifecycle (risk 3) and the rest of the Daeva does not need
   them; the Spiritmaster is playable without spirits, badly, and the checklist says so.
2. **The gate is its own stage** because it needs every stage-1 lane and it re-greens six gates (G-05, W-24).
3. **If stage 1 has to split**, split it at the level: land C-01..C-04 and the **level-10** classes (18 with launched skills, of which 5 are
   M5b-3's or summons: `DispelEffect`, `ProcAtkInstantEffect`, and the three summon classes) with `CondSkillLauncherEffect` pulled into it (W-07),
   gate the class change and the level-10 kit, and take the levels 11-20 classes and the Verteron npcs in a second wave. Do not split
   `ClassChangeService` from the level-10 classes: a Daeva without its level-10 skills is a gate with nothing to cast.
4. **Pace, measured** (git log): M5b-1's stage 1 landed ~14 h after its plan (`5f65cb14f` 09-22 02:29 → `340c05c5e` 16:40) and its stage 2 ~10 h
   after that (`23c4e6485` 09-23 02:32); M5b-2's stage 0 and parts 1-3 landed ~4 h apart (`6f6756c06` 10:41, `29009d778` 14:36, `c1edb0afb`
   18:49, `760e8ab5c` 22:58). A ~110-body stage over four lanes is the size of an M5b-2 part or two, so the effort letters of §5 (agent-days)
   overstate wall-clock time; they remain the relative sizing between items.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5e`)

### 10.1 Processes, databases and profile

| Piece | M5e |
|---|---|
| Schemas | `aion_ls_test_m5e_<hash>` / `aion_gs_test_m5e_<hash>`, same `SchemaLease` and sweep |
| Output | `<bin>/scenario/m5e`, its own `gs_log` and `ls_run` |
| `RESOURCE_LOCK` | the same `"aion_game_server_log;aion_login_server_log"` |
| Profile | `m5e.properties.example` = the M5d profile + `gameserver.simple.secondclass.enable = true` + the quest-XP pin (D9) |
| Allow-list | `tests/scenario/m5e_partial_allowlist.txt`: §A the startup rows of the earlier gates that remain; §B the partials the milestone leaves (none planned); §C the timing rows |
| Accounts | **A**: an Elyos Warrior **A1** (created in the run). **B**: Elyos **B1** PRIEST → CLERIC, **B2** PRIEST → CHANTER, **B3** ENGINEER → RIDER, **B4** MAGE → SORCERER; stage 3: **B5** MAGE → SPIRIT_MASTER. B's characters are created as starting classes, then seeded offline to the advanced class + `player_quests(1006, 'COMPLETE')` + level 10 (D8); their first enter world at level 10 starts 1205 at REWARD (W-24), which no row asserts against |
| **Observers** (rev 2) | **Some packets never reach the character they are about** — `SM_PLAYER_INFO` of a class change (ClassChangeService.java:76) and `SM_MANTRA_EFFECT` (AuraEffect.java:67) go to the known list only (PacketSendUtility.java:98-100). They are asserted at **a character of the other account in sight**: **O = B1** beside A1 at Pernos for C6-C10 (it enters after the seed, and the gate waits until O has seen A1's visibility `SM_PLAYER_INFO` before C7), and **A1** beside B2 in C16 |
| Targets and spots | chosen by the oracle, never hardcoded (D8): a Poeta kill target at A1's level (M5b-1's 210663 or 210133) near each talk spot, or a walk between them with the M5b-1 movement helper; **Pernos 790001** (241.094, 1639.46, 100.375, Poeta spawn file :926-927) for the page-1352 rows (his quest 1006 is unregistered, so `hasQuestInteraction` is false — confirm with the M5d oracle); **kunandes 203087** (840.494, 1217.09, 119.068, :759-760) for the trainer — every talk spot within `isInTalkRange` (NpcController.java:254, 267); a Verteron npc of §2.8 for X12 |

### 10.2 Cases

C1-C3 are M5a cases 1-4 replayed (login, create, enter, level ready) for A1 and for B's characters.

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `oracle.py m5e-progression` for (ELYOS, WARRIOR, 1 → 10, `--new-class GLADIATOR`), each B character, the npc, item and weapon ids, the spots; **every cast of C12-C19 checked against its conditions** (G-01) |
| **C4** | the trainer | A1 seeded offline to level 1 with exp startExp(2) − 1 at the oracle's spot near kunandes; enter; kill once → level 2 → quest 1205 starts at REWARD; `CM_SHOW_DIALOG(kunandes)` within talk range; `CM_DIALOG_SELECT(kunandes, …, 1205)` as the oracle names |
| **C5** | the last starting-class level | quit (the server stores `old_level` 2); seed exp startExp(9) − 1 and **leave `old_level` alone**; enter — `onLevelChange(2, 8)` learns levels 3-8 before the spawn, so they appear only in the full `SM_SKILL_LIST` (§2.1); kill once |
| **C6** | the wall | quit (`old_level` 9 stored); seed exp startExp(10) − 1 and A1's position beside Pernos; enter (**the key shows the class window in this burst**); **O (B1) enters beside A1**; kill once; `CM_MOVE` with the glide bit; `CM_SHOW_DIALOG(Pernos)` |
| **C7** | a wrong choice | `CM_DIALOG_SELECT(0, 3058 SELECT7_1, 0, 0, 1006, 0)` |
| **C8** | the class change | `CM_DIALOG_SELECT(0, 2376 SELECT5_1, 0, 0, 1006, 0)`, watched by O |
| **C9** | a second change | `CM_DIALOG_SELECT(0, 2461 SELECT5_2, 0, 0, 1006, 0)` |
| **C10** | the first Daeva level | kill once; `CM_MOVE` glide; `CM_SHOW_DIALOG(Pernos)`; O quits |
| **C11** | the Daeva survives a relog | quit; seed `players.dp = 2000`, `player_life_stat.hp` low and **the greatsword equipped in place of the Training Sword** (D8; A1 has 51 since C8); enter within 5 minutes; `CM_MOVE` glide; `CM_SHOW_DIALOG(Pernos)` |
| **C12** | the Gladiator's kit | on a target within 7 m and no other enemy in range: **769 *Absorbing Fury*, then 758 *Roiling Hack* after 769's `SM_CASTSPELL_RESULT`** (the chain); 519 *Explosion of Rage*; 2981 *Taunt*; then auto-attack with the greatsword until the target dies or X9g's budget is spent |
| **C13** | the level-15 enter world | quit (`old_level` 10 stored); seed exp startExp(15); enter; quit; enter again |
| **C14** | Verteron | quit; seed A1 at the oracle's Verteron spot; enter; fight the oracle's npc until its skill lands or the §10.3 X12 budget is spent |
| **C15** | resurrection | seed A1's HP to 1 and B1 beside A1 (same map, oracle spot); both enter; A1 is killed; B1 `CM_TARGET_SELECT(A1)`, `CM_CASTSPELL(1699, A1)`; A1 `CM_REVIVE(SKILL_REVIVE)`; then B1 casts 1699 on a living B-side target |
| **C16** | a toggle | B1 quits; B2 enters **beside A1** (A1 is the observer); `CM_CASTSPELL(1809)`; wait 14 s; `CM_TOGGLE_SKILL_DEACTIVATE(1809)`; wait 14 s; `CM_TOGGLE_SKILL_DEACTIVATE(1685)` (not a toggle) |
| **C17** | the robot and a charge | B3, seeded with **a keyblade equipped** (D8), enters; `CM_CASTSPELL(2767)` *Embark*; after its `SM_RIDE_ROBOT`, `CM_CASTSPELL(2606)`, `CM_USE_CHARGE_SKILL` after the oracle's first threshold; again after the last; `CM_TOGGLE_SKILL_DEACTIVATE(2767)` |
| **C18** | a skill book | B4 seeded with book 169500932; enters; `CM_USE_ITEM(book)`; `CM_USE_ITEM` of a second copy; `CM_CASTSPELL(1417)` on a monster |
| **C19** | (stage 3) summons | B5 enters; `CM_CASTSPELL(3706)`; `CM_SUMMON_COMMAND(attack, monster)`; `CM_SUMMON_COMMAND(release)` |
| **C20** | (stage 3, O) stigma | A1 seeded to level 20 + `player_quests(1929, COMPLETE)` + a level-20 Gladiator stone + kinah; enters; `CM_EQUIP_ITEM(stone, stigma slot)`; quit, enter |
| **C21** | reports and shutdown | the M5a Q8 bar + the M5e rows; the stop file with a character online |

### 10.3 Assertions

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **X1** | C4 | the level-2 kill sends `SM_ACTION_ANIMATION(A1, 0 LEVEL_UP, 2)` and `SM_STATUPDATE_EXP`; `SM_QUEST_ACTION` ADD then UPDATE for **1205 with status REWARD** (M5d's decoder); at kunandes `SM_DIALOG_WINDOW(kunandes, 1011, 1205)`; the reward pays **275 × the membership-0 quest rate of the profile** (1.0 pinned, D9) — **+275 exp** exactly | **Proves:** C-05's registration and `onLevelChangedEvent`, the Warrior's page (`_1205ANewSkill.java:84-90`). **Cannot:** the other five classes' pages (unit test) | the handler unregistered (no 1205 at all, risk 7); the class check inverted (no window) |
| **X2** | C5 | the enter world's full `SM_SKILL_LIST` holds the oracle's level 1-8 list (2877, 139, 2890, 2865, 2903, 2878 included); the kill sends `SM_ACTION_ANIMATION(A1, 0, 9)`, then **one** `SM_SKILL_LIST(138, messageId 1300050)`, and an `SM_STATS_INFO` whose base max HP is the oracle's WARRIOR level-9 value | **Proves:** the offline level change of the enter world (levels 3-8), the online level-up with `learnNewSkills(9, 9)` and the `isNew` message. **Cannot:** the multi-level online path (C13 is offline) | `onLevelChange` without `learnNewSkills` (no 138; nothing of 3-8); `sendPacket` with the two message ids swapped (138 with 0). Rev 1's "loop from `old`" mutation is dropped: `addSkill` returns false for a known skill at the same level (PlayerSkillList.java:61-62), so it sends nothing X2 could see |
| **X3** | C6 | before the kill: `SM_DIALOG_WINDOW(0, 2375, 1006)` in the enter-world burst; after the kill: `SM_STATUPDATE_EXP` with the full bar (exp shown = exp needed), **`STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1`**, and **no** `SM_ACTION_ANIMATION`; the glide gets **`STR_GLIDE_ONLY_DEVA_CAN`**; Pernos answers page **1011**. None of B's level-1 characters gets any `SM_DIALOG_WINDOW(0, …)` | **Proves:** S1-S3, the non-Daeva cap, `canGlide`'s refusal, the pre-Daeva page. **Cannot:** that a real client draws the window (risk 8) | `showClassChangeDialog` without its level check (B's characters get it); `maxLevel` 66 for everyone (a level 10) |
| **X4** | C7 | at A1: `SM_MESSAGE` "Invalid class chosen", then `SM_DIALOG_WINDOW(0, 0)`, and **no** `SM_ACTION_ANIMATION`; at O: **no** `SM_ACTION_ANIMATION` or `SM_PLAYER_INFO` about A1 | **Proves:** the `(id, id + 2]` guard and the unconditional close (ClassChangeService.java:33, 66-69) | `validate` skipped: a Warrior becomes a Sorcerer |
| **X5** | C8 | **at A1**, in this order: an `SM_STATS_INFO` (from `upgradePlayer`, PlayerController.java:601-604) with the GLADIATOR level-9 base max HP; `SM_ACTION_ANIMATION(A1, 4, 9)`; **ten** `SM_SKILL_LIST` for 44, 45, 46, 48, 49, 50, 51, 52, 53, 54 with the oracle's message ids (0 for 44, 46, 48, 49, 50; 1300050 for the rest, §2.3); `SM_QUEST_ACTION` ADD 1006 **COMPLETE**, then UPDATE; `SM_DIALOG_WINDOW(0, 0)`; and **no `SM_PLAYER_INFO`** between the `CM_DIALOG_SELECT` and the `SM_DIALOG_WINDOW(0, 0)`. **At O**: `SM_ACTION_ANIMATION(A1, 4, 9)` and **`SM_PLAYER_INFO` for A1 with class id 1** (its visibility `SM_PLAYER_INFO` before C7 had 0) | **Proves:** S5-S8 end to end with Java's recipients (ClassChangeService.java:75-76; PacketSendUtility.java:98-100), `learnNewSkills(9, level)` with the starting-class rule, `completeAscensionQuest`. **Cannot:** the other ten classes (C-01's unit table); wire id 4 is also `CRAFT_LEVEL_UP`, so the row proves the id, not the name | `learnNewSkills(10, …)` (no passives); the quest ADD skipped; **`SM_PLAYER_INFO` sent to the player too** (an unfaithful `toSelf`: A1 receives one); `SM_PLAYER_INFO` not broadcast (O sees class 0) |
| **X6** | C9 | at A1: `SM_MESSAGE` "You already switched class", `SM_DIALOG_WINDOW(0, 0)`; at O: no further `SM_PLAYER_INFO` about A1, so its class stays 1 | **Proves:** the starting-class guard | the guard dropped: a Gladiator becomes a Templar (O sees class 2) |
| **X7** | C10 | the kill sends `SM_ACTION_ANIMATION(A1, 0, 10)`; `SM_SKILL_LIST` for the nine level-10 Gladiator skills 169, 246, 249, 348, 519, 758, 769, 2891, 2981 with the oracle's message ids (0 for 169 and 2891, whose walks reach the known 140 and 2890, `skill_tree.xml:219, 2919`; 1300050 for the rest — `m5e_isnew.py`, which holds only because C5 let the enter world learn levels 3-8); **`SM_SKILL_REMOVE(30001)` and `SM_SKILL_LIST(30002)`**; the craft skills 30003 / 40009 (A-03b); the glide now gets **no** refusal and `SM_FLY_TIME` shows FP falling within 3 s; Pernos answers page **1352** | **Proves:** `updateDaeva` took effect online (the cap lifted), the Daeva rules of `learnNewSkills`, W-11, W-13. **Cannot:** flight in a FLY zone (W-12) | `updateDaeva` not called in `setClass`: level stays 9, and every later row fails with it |
| **X8** | C11 | after the relog: the `CM_LEVEL_READY` answer's `SM_PLAYER_INFO` (CM_LEVEL_READY.java:52) has **class id 1**; `SM_STATUPDATE_EXP` puts A1 at level **10**; `SM_QUEST_COMPLETED_LIST` holds **1006** (already decoded, A-04c); DP **2,000** (`SM_STATUPDATE_DP` / `SM_STATS_INFO`); and **two rows only a Daeva gets**: the glide draws **no** `STR_GLIDE_ONLY_DEVA_CAN` (FlyController.java:137-141 reads `isDaeva`), and Pernos answers page **1352** (DialogPage.java:122-123) | **Proves:** the load path's `updateDaeva` (PlayerCommonData.java:276 with `online` false, :588-610) and the persistence of class and quest. **Cannot:** the 5-minute DP reset (unit test). Level 10 alone proves nothing: with exp above startExp(10) the load path allows 66 even when `updateDaeva` refuses (:276) | `updateDaeva` refusing on the quest list it loads (`PlayerQuestListDAO.load`, :597-601): level 10 but no Daeva, so the glide is refused and Pernos answers 1011 |
| **X9** | C12 | 769, then 758 (the chain): each hit's damage `d` on the target (its `SM_CASTSPELL_RESULT` HP entry) is followed ~1 s later by an HP increase of A1 of **exactly ⌊d × p / 100⌋**, p = 10 for 769 and 30 for 758 (SkillAtkDrainInstantEffect.java: `increaseHp(ABSORBED_HP, …)` scheduled after 1,000 ms; the C11 HP seed keeps the cap from binding); 758 is **accepted** (no `SM_CASTSPELL_RESULT` refusal); 519: `SM_STATUPDATE_DP` falls by **exactly 2,000** and the target takes damage; 2981: `SM_CASTSPELL_RESULT` with a non-16 status | **Proves:** `SkillAtkDrainInstantEffect`, the chain condition on a Daeva (769 opens, 758 follows), the DP condition and `DpUseAction` on a Daeva, `HostileUpEffect` applied. **Cannot:** `TargetChangeEffect` (20 % `preeffect_prob`) — a unit test | the drain added to the target; the drain percentage of the wrong effect; `setDp` still refusing (no DP: 519 refused) |
| **X9g** | C12, both gates | every `SM_CASTSPELL_RESULT` of C12 whose spell status is **1 (STUMBLE)** carries a target location (SM_CASTSPELL_RESULT.java:145-152) whose horizontal distance from the target's position before the hit is **≤ 2.0 m + 0.01** and which lies away from A1 (StumbleEffect.java:66-70: 2 m along the heading from A1 to the target). **Geo off:** exactly that 2 m point, z unchanged (GeoMap.java:137-147 with no geometry finds no collision and no ground). **Geo on:** ≤ 2 m, and where `tools/oracle/geo`'s probe finds open ground on the segment, z = its `GeoMap.getZ` at that point. The rotation of 519 (one per DP seed) and the greatsword criticals give the stumbles; the gate fights until one stumble or a budget of attack decisions for which a miss is below 10^-4 at the oracle's critical and resist rates (X12's rule) | **Proves:** W-27: the geo read on the path, and that `m5e_geo` is not a plain re-run (G-04). **Cannot:** a stop against a wall (the oracle picks open ground) | the stumble target computed without geo in `_geo` (z off the terrain); `getClosestCollision` returning the origin |
| **X10** | C13 | the first enter world: `SM_ACTION_ANIMATION(A1, 0, 15)` and the full `SM_SKILL_LIST` containing **563**; the second enter world completes (`SM_ENTER_WORLD_CHECK`, `CM_LEVEL_READY` answered) with no ERROR line | **Proves:** W-07 closed — the offline level change and `activatePassiveSkillEffects` with `CondSkillLauncherEffect`. **Cannot:** the launcher firing (it needs HP below its threshold — `tests/effects_al`) | `CondSkillLauncherEffect` unported: the enter world throws and no `SM_ENTER_WORLD_CHECK` arrives |
| **X11** | C14 | the enter world into Verteron completes with no ERROR and no unported trace | **Proves:** W-22's first measurement. **Cannot:** anything about the map's other systems | – |
| **X12** | C14 | in the fight's recording: an `SM_CASTSPELL` from the npc with the oracle's skill id, and the matching effect on A1 (`SM_ABNORMAL_STATE` for a debuff, periodic `SM_ATTACK_STATUS` for a DoT) or on the npc (`SM_ABNORMAL_EFFECT` for *Crouch*) | **Proves:** the npc half of §2.8 through M5b-2's rotation. **Cannot:** the 25 % `prob` — the gate asserts *at least one* over enough attack decisions that `0.75^n < 10^-4` (m5b2-plan.md X9's rule) | a class of §2.8 left unported (ERROR, X19) |
| **X13** | C15 | B1's 1699 on dead A1: A1 receives **`SM_RESURRECT(name B1, skill 1699)`**; A1's `CM_REVIVE(SKILL_REVIVE)` revives it: the first `SM_STATUPDATE_HP` / `_MP` after it carry **35 %** of the max HP / MP in force when `revive` sets them (PlayerReviveService.java:196-197), **before** the soul sickness lowers the max; then **soul sickness 8296** — the skill 1699's `resurrect skill_id` names (skill_templates.xml, 1699; ResurrectEffect.java:28 → `revive(player, 35, 35, true, player.getResurrectionSkill())`, PlayerReviveService.java:47 → `updateSoulSickness`, which falls back to 8291 only for skill id 0, PlayerController.java:730-731) — in A1's `SM_ABNORMAL_STATE` (`tslot="SPEC2"`, shown), after which A1's max HP is the oracle's 70 %; 1699 on a living target: status **16** and **no** `SM_RESURRECT` | **Proves:** `ResurrectEffect` incl. its `isDead` guard and its stored skill, `skillRevive`. **Cannot:** the ResPost arm (W-17) | `calculate` without the guard (a living player gets the offer); `skillRevive` at `revive(20, 20)`; the resurrection skill not stored (8291 instead of 8296) |
| **X14** | C16 | **At B2**: after `CM_CASTSPELL(1809)`, `SM_ABNORMAL_STATE`s that **never list 1809** (`tslot="NOSHOW"`, §2.4) and that list **8998** (*Celerity Mantra Effect*, `tslot="CHANT"`, 6,500 ms) re-applied at least twice, 6.5 ± 0.5 s apart; after `CM_TOGGLE_SKILL_DEACTIVATE(1809)`: within 7 s an `SM_ABNORMAL_STATE` without 8998, and **none listing 8998 from toggle-off + 7 s to + 14 s**. **At A1 (the observer)**: **≥ 2 `SM_MANTRA_EFFECT(B2, 8998)` 6.5 ± 0.5 s apart** before the toggle-off and **none from toggle-off + 0.5 s** (B2 itself never receives one, AuraEffect.java:67). `CM_TOGGLE_SKILL_DEACTIVATE(1685)` (not a toggle) changes nothing | **Proves:** `AuraEffect`'s solo arm and task, C-04's toggle half, its audit. **Cannot:** the team arm (W-23) | the packet not removing the effect and the task not cancelled by `endEffect` (both: mantras and 8998 keep coming; the second also X20's census). Rev 1's row asserted a 1809 icon and caster-side mantra packets that Java never sends |
| **X15** | C17 | 2767: an `SM_RIDE_ROBOT` for B3 with the oracle's **non-zero robot id** (the keyblade's `robot`); then each release of 2606 sends `SM_CASTSPELL_RESULT` with the charged skill the oracle picks for that charge time (`skill_charge.xml`); after `CM_TOGGLE_SKILL_DEACTIVATE(2767)`: an `SM_RIDE_ROBOT` with robot id **0** (RideRobotEffect.java:42-43) | **Proves:** `RideRobotEffect` start and end, the `ride_robot` use condition passing, C-04's charge half and `useChargeSkill`, the toggle half on a second toggle. **Cannot:** exact timing at a threshold (a unit test with a `ManualClock`) | `chargeTimeMillis` measured from the wrong start (always the first skill); the robot id not taken from the weapon (0: 2606 refused) |
| **X16** | C18 | the first book: `SM_ITEM_USAGE_ANIMATION`, `SM_SKILL_LIST(18, 1300050)`, `STR_USE_ITEM`, the book deleted; the second copy: refused (no `SM_SKILL_LIST`, the item kept); 1417 on a monster: **`SM_ABNORMAL_EFFECT`** with 1417 on it (an effect on a monster is never an `SM_ABNORMAL_STATE`, SkillDecoders.h:21-25) | **Proves:** C-03, `SleepEffect` + `DeformEffect`. **Cannot:** the class / race refusals (C-03's unit test) | `canAct` without the "known" check (a second learn and a lost book) |
| **X17** | C19 | `SM_SUMMON_PANEL` with the spirit's object id and life time; an `SM_NPC_INFO` for npc 833343; after the attack command an `SM_ATTACK_STATUS` on the monster **whose attacker is the spirit**; after release `SM_SUMMON_PANEL_REMOVE` and the spirit's `SM_DELETE` | **Proves:** stage 3's chain. **Cannot:** servant, homing and trap (unit tests, M-04) | `createSummon` not setting `master.summon` (release finds nothing) |
| **X18** | C20 | `CM_EQUIP_ITEM` into the stigma slot: the inventory kinah falls by the oracle's price (25,000 × the price modifiers), `SM_SKILL_LIST` with the stone's skill and message **1300401**; after the relog the skill is in the full list; the same without the 1929 row: refused, kinah unchanged | **Proves:** T-01 and the `isCompleteQuest` gate. **Cannot:** linked stigmas (6 stones, level 55) | `getPossibleStigmaCount` ignoring the quest |
| **X19** | C21 | the M5a Q8 bar: `unported_trace.txt` empty, census empty, no ERROR in either log, `partial_trace.txt` ⊆ the allow-list | **Proves:** nothing on the scripted path left the D4 scope. **Cannot:** the skills off the path | any class outside the scope on the path |
| **X20** | C21 | `live_counts.txt`: `Effect` 0 with `created > 0`, `Skill` 0; stage 3: **`Summon` 0 with `created > 0`** | **Proves:** mantra, drain, DoT, summon lifetimes all ended. **Cannot:** a leak bounded by a live creature | the aura task not cancelled; `Player.summon` not cleared on release |

### 10.4 Mutation proof (the minimum set)

| Mutation | Must fail | Must stay green |
|---|---|---|
| `setClass`: skip `updateDaeva` | **X7** (level stays 9 online) — **not X8**: the quest row is still written, so the load path's `updateDaeva()` promotes the character at relog (PlayerCommonData.java:276) and X8 is green again | X4, X5, X6, X8 |
| `updateDaeva`: ignore the quest list it loads for an offline player (`PlayerQuestListDAO.load`, :600) | **X8** (level 10 from the exp rule, but no Daeva: the glide is refused, Pernos answers 1011) | X7 (online, it reads the player's own list) |
| `setClass`: `learnNewSkills(10, level)` | **X5** (no level-9 passives) | X4 |
| `setClass`: `SM_PLAYER_INFO` with `toSelf` (an unfaithful "fix") | **X5** (A1 receives an `SM_PLAYER_INFO`) | X4, X6 |
| `onLevelChange`: no `learnNewSkills` | **X2** (no 138; nothing of 3-8 in the full list), X7 | X1, X3 |
| `setClass`: drop the `(id, id + 2]` check | **X4** | X5 |
| `showClassChangeDialog`: drop `level >= 9` | **X3** (B's level-1 characters get the window) | X5 |
| `PlayerCommonData::setExp`: `maxLevel` 66 for everyone | **X3** (a level 10 without a class change) | X1, X2 |
| `PlayerSkillList::addSkill`: `isNew` always true | **X5** (the five skills with a known pre-skill get 1300050 instead of 0) and X7 (169, 2891) — **not X2**: 138 has no pre-skill, so it is new either way | X2 |
| `CondSkillLauncherEffect::applyEffect` → `AION_UNPORTED` | **X10** | X1-X9 |
| `CM_TOGGLE_SKILL_DEACTIVATE`: do not call `removeEffect` | **X14** (A1 keeps receiving `SM_MANTRA_EFFECT(B2, 8998)`; B2 keeps receiving 8998) and X15's last step (robot id stays non-zero) | X9, X13 |
| `AuraEffect::endEffect` / `Effect::stopTasks` skipping the aura task | **X14** (mantras at the observer and 8998 at B2 after the toggle-off) and **X20** | X9, X15 |
| `ResurrectEffect::calculate`: drop the `isDead` guard | **X13** | X9 |
| `skillRevive`: pass 0 as the resurrection skill | **X13** (8291 instead of 8296) | X9 |
| `RideRobotEffect::startEffect`: robot id 0 | **X15** (2606 refused by `ride_robot`) | X14 |
| `SkillLearnAction::canAct`: drop the "already known" check | **X16** | X13 |
| `_1205ANewSkill`: not registered | **X1** | X2-X20 |
| `SummonsService::release`: do not clear `master.summon` | **X20** (stage 3) and a second summon refused | X17's first half |
| `getSelectedPlayerClass`: the Elyos `SELECT5_1` / `SELECT5_2` swapped | **X5** (class id 2, not 1) | X4 |
| `getSelectedPlayerClass`: the Asmodian `SELECT7_1` / `SELECT7_2` swapped | **nothing in the gate** (every class change in it is Elyos) — **C-01's unit table must catch it** | the gate |
| `HostileUpEffect`: add no hate | **nothing in the gate** (one attacker) — `tests/effects_al` over `AggroList` | the gate |

### 10.5 The geo gate

`gs.scenario.m5e_geo`: the same script, `gameserver.geodata.enable = true`, `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`, the same lock.
**It has a row of its own, X9g** — rev 1's premise ("only the dash family reads geo, and the gate casts none") was false: 519's `subeffect`
8218 and every greatsword critical run `StumbleEffect.calculate` → `GeoService.getClosestCollision` (W-27). The other geo readers of stage 1
(Dash, BackDash, RandomMoveLoc, Pulled, SimpleRoot) are not cast by the gate and get `tests/geo`-fixture unit cases (E-01, E-02); stage 3's
`SummonServantEffect` / `SummonTrapEffect` read geo on the servant's and the trap's spawn point and get the same.

---

## 11. Real-client checklist (user, after stage 2; steps 11-12 after stage 3)

Prerequisites as m5b-plan.md §10 steps 1-6, with `mygs.properties` from `m5e.properties.example` (**the key must be on**, D1). The M5b-3, M5c
and M5d checklists are assumed done.

1. Make an Elyos Warrior. At level 2 the journal shows **"A New Skill"** ready to report; talk to **Kunandes** (the Warrior master in Poeta):
   a class page opens; take the reward (+275 exp). Make an Asmodian on a second account and do the same at level 3 (2132).
2. Level to 9 (quests of M5d, kills, or `UPDATE players SET exp = 126068` while offline). At each level a "you have learned" message appears
   for new skills; the level-up effect plays.
3. At 9 with a full bar, kill once more: **the bar stays full, the level stays 9**, a message says the ascension quest is not finished. Try to
   glide (jump + glide key): "only Daevas can glide".
4. **Log out and back in: a class-selection window opens** (Gladiator / Templar for a Warrior). **Write down whether it appears at all** —
   risk 8. Pick one: the class-change effect plays, new passive skills appear; **a second account standing nearby sees the new class**. Java
   sends the class change's `SM_PLAYER_INFO` to the others only (§2.3 S7), so **write down whether your own class icon changes before the next
   relog** — that is the 4.8 client's behaviour against Java's packets, not a port bug.
5. Kill a monster: **level 10**; a batch of new skills; the gathering skill becomes the Daeva version; **a DP bar appears**.
6. Glide: it works now and the flight-time bar drains. Talk to **Pernos**: a different dialog page than before (1352).
7. Use the new skills. *Absorbing Fury* and *Roiling Hack* need a **greatsword or polearm** (the Training Sword will not do; buy one from an
   M5c vendor or put one in by SQL — the equip needs the Greatsword skill the class change taught), and *Roiling Hack* only follows *Absorbing
   Fury* (a chain): both heal you by part of the damage. *Explosion of Rage* needs DP (earn it with kills) and may knock the target back. **Do
   not click the Poeta teleporter (daines)**: its Daeva branch is M5f's `showMap` and throws (W-10).
8. With other classes (SQL while offline: `player_class`; `INSERT INTO player_quests (player_id, quest_id, status) VALUES (<id>, 1006,
   'COMPLETE');` — **the status by name**: the column is `enum('LOCKED','START','REWARD','COMPLETE')` (sql/aion_gs.sql:789, read by name,
   `PlayerQuestListDAO.cpp:68`), so a numeric 3 would store `REWARD` and give a non-Daeva; `exp` for level 10): a Chanter's mantra shows the
   *Celerity Mantra Effect* icon, ticks, and **switches off** when clicked again (the second account sees the mantra effect); an Aethertech
   **with a cipher-blade** casts *Embark* (the robot appears), then *Kinetic Slam* charges and fires — without the robot it is refused; a
   Sorcerer's *Curse of Roots* sleeps a monster; **a Cleric resurrects a dead character of the second account**, who accepts, stands up at 35 %
   and gets *Soul Sickness*.
9. **Relocate to Verteron by SQL** (`world_id 210030000` and a spot from the oracle; no teleporter until M5f): fight arachnas and spies —
   poison and blindness debuffs; turtles that *Crouch*; veteran tursin scouts (17-18) that spin you.
10. SQL a Gladiator to level 15 and log in: it must log in (W-07).
11. After stage 3: a Spiritmaster summons a **Fire Spirit**, orders it to attack, dismisses it; a Cleric at 15 summons a Holy Servant; a
    Ranger at 13 lays a trap and a monster walks into it.
12. After stage 3 (if D6 is taken): SQL a level-20 character with quest 1929 complete and a stigma stone; open the stigma master's window
    (a stigma npc in Sanctum after M5f, or relocate); equip it; the kinah drops by 25,000 and the stigma skill appears.
13. **Expected gaps, not regressions:** the ascension missions 1006 / 2008 do not start (route R is M5f's, D13); the teleporters throw for
    Daevas (W-10); obelisk binding does nothing (O-03); any skill above level 20 or any Eltnen / Morheim monster may throw (D4) — write down the
    skill ids from `unported_trace.txt`, they are the next milestone's list.
14. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt`, `partial_trace.txt` and `unported_trace.txt`.

---

## 12. What was measured and what was inferred

**Measured** (read-only scripts over the two trees and the working tree, re-runnable; in the session scratchpad `plans/`: `m5e_effects.py`,
`m5e_detail.py`, `m5e_l10.py`, `m5e_union.py`, `m5e_effund.py`, `m5e_undeclared.py`, `m5e_maps.py`, `m5e_vert.py`, `m5e_summon.py`,
`m5e_books.py`, `m5e_stig.py`, `m5e_charge2.py`, `m5e_war.py`, `m5e_poeta_npcs.py`; **rev 2:** `m5e_refs.py` (the launched-skill closure),
`m5e_groups.py`, `m5e_leaves.py`, `m5e_isnew.py` (the `isNew` walk), `m5e_pets.py` (the pet skills), `m5e_vert_ai.py` (the AIs without a
handler)):

- `skill_tree.xml`: 4,693 rows, 3,525 autolearn, 1,131 stigma, 38 book rows; the book items' sources; the six class masters per race and their
  spawns; the start maps' function npcs.
- Every scope row of §2.4 (skills, passives, toggles, charges, leaf classes, closures, unported classes, sites, Java LOC), **with and without
  the launched skills** (rev 2), the per-level first reach, the per-class counts, the single passive blocker (563, still the only one with
  launched skills followed), the level-10 kit **and its start / use conditions**, the 18 class-change passives (49 rows) and their message ids.
- **The `isNew` message ids of the Warrior's path** (levels 1-10 and the class change) with a model of `getHighestSkill` + `createSkillTree`
  (`m5e_isnew.py`), and the effect of the rev-1 C5 seed on them.
- The 57 classes' `AION_UNPORTED` counts at HEAD `760e8ab5c`, the 15 inner bodies, and **0** undeclared methods otherwise.
- The weapons: the created Warrior's and Engineer's starting weapons and their item groups; the equip skills of `ItemGroup` (51 for a
  greatsword, 112 / 115 for a keyblade) and who learns them; the keyblades' `robot` ids.
- Where the class change's, the aura's and the robot's packets go (`sendPacket` / `broadcastPacket` / `broadcastPacketAndReceive`), from the
  Java bodies.
- `SummonRelease`'s 2 sites; the pet skills of the level ≤ 20 summons (168 rows, 28 usable by 20, one unported class).
- The geo readers of `skillengine/effect` (15 classes, grep of `GeoService`).
- The Verteron / Altgard AIs without a C++ handler, and the C++ tree's three AI markers.
- The git log of M5b-1 and M5b-2 (the pace of §9 item 4).
- Verteron / Altgard / Eltnen / Morheim / Poeta / Ishalgen: npc ids, spots, AI names, level histograms, npc-skill classes (all `npc_skills`
  files, `npc_ids` expanded).
- P5-08's 166 sites per file; `ClassChangeService` 7, `StigmaService` 12, `SummonsService` 13, `TrapService` 2, `SummonGameStats` 12,
  `SummonLifeStats` 1, `PlayerReviveService` 9, `TeleportService` 21, `InstanceService` 12; `PlayerCommonData`, `SkillLearnService`,
  `PlayerSkillList`, `FlyController`, `CM_MOVE`, `CM_EMOTION`, `Summon`, `SummonController`, the listed server packets: 0.
- 42 C++ `CM_*` files against 188 Java classes; the opcodes and lines of `ClientPacketInfo.gen.inc`.
- The ascension handlers' calls into `InstanceService`, `TeleportService`, the flight teleport, the item-use zone (grep of the Java bodies).
- The quest data of 1001-1007, 2001-2009, 1205, 2132, 1929 (`quest_data.xml`); the experience table; the config defaults
  (`custom.properties:32`, `CustomConfig.cpp:18`, MembershipConfig.java:34-41).
- `cycles.toml` rows for summons and the M5e effect classes (§2.5, E-03).

**Inferred, to confirm before relying on it:**

- **That A-01 to A-04 hold at branch time.** Four milestones stand between this plan and its branch; §0 is written so the check is a grep.
- **That `DispelEffect`, `ProcAtkInstantEffect` and `MpAttackInstantEffect` are M5b-3's** — m5b3-plan.md rev 2 takes `ProcAtkInstantEffect` in
  its E-02 (R) and `DispelEffect` / `MpAttackInstantEffect` in its E-03 (W, recommended). `DispelEffect` is counted in E-01 here; the other two
  are counted apart (A-02c, T-02).
- **That route S's window is drawn by the 4.8 client** (risk 8). Not verifiable read-only.
- **That no earlier gate's path reaches a new class** (G-05 (b)). Reasoned from the gates' characters (no Daeva cast, no level ≥ 10 skill
  cast), not run; M5c's C19 Gladiator is the one to watch.
- **That the quest registry table picks up the Q05 / Q09 sources** (risk 7) — the mechanism is read from `HandlerRegistry.h` and the
  CMake, not run.
- **The effort letters**, from body counts and Java LOC against M5b-2's and M5c's items; §9 item 4 gives the measured wall-clock pace they
  overstate.
- **That the gate's greatsword crits and 519 stumble often enough for X9g** — the npc's `STUMBLE_RESISTANCE` and A1's critical rate are
  G-01's to compute, not measured here.
- **That the npc of X12 fires within the gate's budget** — `prob` 25 per entry, the decision rate not modelled (m5b2-plan.md §13 item 6).
- **That the stats template alone predicts X2 / X5's base max HP** — the passives touch no `MAXHP` at levels 9 (`wpnmastery`,
  `armormastery`, `shieldmastery`, `wpndual`, `statboost` 138), per m5b2-plan.md D2's argument, not re-checked for every class.

**Claims of the roadmap and the siblings this analysis checked:** roadmap P5-08 = 166 (confirmed), "147 packets" (146 measured), "trainers"
(none exist but the profession masters, M5c's), "class change" in M5e (retail needs M5f); m5b2-plan.md "four starting classes" (six);
m5b3-plan.md O-02 "stigma → M5c" (m5c-plan.md rev 2 now hands stigma to M5e, :399); m5b3-plan.md rev 2 E-02 / E-03 (two classes of this
plan's path, A-02c); m5c-plan.md W-06 / W-20 and D5 (confirmed and reused); m5c-plan.md rev 2's size (~195 bodies, :77); m5d-plan.md D-02
(the key's arm is M5e's, confirmed).

---

## 13. Open questions this analysis could not settle without building

1. **Does a 4.8 client open the class-selection window from `SM_DIALOG_WINDOW(0, page, 1006)` at login, and which action ids does it send?**
   The real-client session answers it (§11 step 4); if it does not, D1 (a) has no real-client path and (b) / (c) become the only ones.
2. **Whether Pernos's dialog needs a quest state** — he stands at (241.094, 1639.46, 100.375) (Poeta spawn file :926-927); `hasQuestInteraction`
   must be false for him in C6, C10 and C11; the M5d oracle decides.
3. **What the first player spawn in Verteron wakes** (W-22, measured in part). The gate lane should run C14 alone first, with the unported
   trace, before writing X11-X12.
4. **Whether the fake client records another player's `SM_PLAYER_INFO` and keeps its class id** (the observer rows of X4-X6). Rev 1's
   question — whether an `SM_PLAYER_INFO` about oneself disturbs the harness — is moot: Java sends A1 none at the class change (§2.3 S7), and
   the one at `CM_LEVEL_READY` (CM_LEVEL_READY.java:52) is the M5a harness's own path.
5. **How long a Verteron fight must last for X12** (question 6 of m5b2-plan.md §13, on a new npc).
6. **Whether `skill_charge.xml`'s thresholds are scaled by cast speed at level 10** (`getCastSpeedForAnimationBoostAndChargeSkills`,
   `CreatureController.cpp:491`) — the oracle needs the factor for X15; a level-10 Aethertech without buffs should have 1.0 (and *Embark*'s
   stat boosts change no cast speed, `skill_templates.xml`, 2767).

---

## 14. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 19 findings (5 high, 6 medium, 8 low) and confirmed the rest of the plan's
measurements (the skill-tree counts, the 146 packets, `ClassChangeService`'s 7 sites, P5-08's 166, the direct closure, the maps, the thresholds,
route S's chain, the page and action ids, the lane disjointness). Every finding was re-checked against the Java source, the data and the C++
tree before it was applied; all 19 were confirmed. Three were applied with a refinement, none was rejected.

| # | Severity | Finding | Re-check | What changed |
|---|---|---|---|---|
| 1 | high | X9: 758 *Roiling Hack* is a chain follower and needs a greatsword or polearm; A1 holds a Training Sword | confirmed: `chain precategory="W_CHAINC_1TH_1"`, weapon GREATSWORD POLEARM; 769 is the opener, Gladiator 10; ChainCondition.java:32-52. Refinement: 769 sets no chain `time`, so 758 need not follow "within 3 s" (ChainSkills.java:42, 59-60) — the gate casts it after 769's result | §2.4 kit table; D8 (a greatsword seeded after C8 — its equip skill 51 comes with the class change, Equipment.java:303-314, 436-440); C11, C12 (769 then 758); X9 (exact drain ⌊d × p / 100⌋ for both); G-01's condition check; risk 13; §11 step 7 |
| 2 | high | X15: 2606 needs the robot (`ride_robot`), which only 2767 *Embark* with a keyblade gives | confirmed: RideRobotCondition.java:18-20; RideRobotEffect.java:23-26; the Engineer's Pistol for Training is a GUN | D8 (a keyblade for B3; 112 is an ENGINEER level-1 skill); C17 (2767, then 2606, then toggle 2767 off); X15 (`SM_RIDE_ROBOT` with the oracle's robot id, then 0); 10.4 row; §11 step 8 |
| 3 | high | X14: the caster never receives `SM_MANTRA_EFFECT`, and 1809 is NOSHOW | confirmed: AuraEffect.java:67 with a `Creature` → PacketSendUtility.java:98-100; `tslot="NOSHOW"`, EffectController.java:702-703, PlayerEffectController.java:85-88, 118-119; SkillDecoders.h:25 already said so | §2.4 kit table; §10.1 observers; C16 (B2 beside A1, 14 s after the toggle-off); X14 (8998 at B2, `SM_MANTRA_EFFECT(B2, 8998)` at A1); 10.4 rows |
| 4 | high | X5: `SM_PLAYER_INFO` of the class change goes to others only; `SM_STATS_INFO` comes before the animation | confirmed: ClassChangeService.java:75-76; PacketSendUtility.java:98-100; PlayerController.java:601-604 → PlayerGameStats.java:51-53, 331-332 | §2.3 S7; §10.1 observer O = B1; C6-C10; X4, X5, X6 at A1 and at O; the "wrong port" column now names the unfaithful `toSelf`; 10.4 row; §11 step 4; §13 item 4 |
| 5 | high | Lesson 2 missed the skills effects launch (subeffect, delayed, provoker, signets) | confirmed with an independent closure (`m5e_refs.py`): level 10 → 18 classes / 40 sites; levels 9-20 → **39** / 88 (the review said 38: `PoisonEffect` is now also reached by 3481 → 9100, an A-02c class); union **46 / 109** (as the review); stigma-only 11. **Refinement:** `ProcAtkInstantEffect` is M5b-3's E-02 (R, m5b3-plan.md:459) and `MpAttackInstantEffect` its E-03 (W) — so they sit in A-02c / T-02, not in E-02, which grows to **38 + 3** (not 40 + 3); per class Assassin **11** (the review said 10) | header correction 3; §0 A-02c; §1 holes and totals (~213 bodies, 160 sites); §2.4 (the launched-skill table, the scope table, first reach, per class, stage-1 set); §2.8; §2.9; W-06; D4; E-02; T-02; §9; §12; risk 5 |
| 6 | medium | The `isNew` ids of §2.1 are wrong, and C5's `old_level` 8 seed breaks X7 | confirmed with a model of the Java walk (`m5e_isnew.py`): 2865 / 2878 → 1300050; with `old_level` 8, 2891 turns 1300050 at 10; PlayerController.java:569-570 | §2.1 (and the `old_level` paragraph); C5, C6, C13 (`old_level` left as stored); X2 (the enter world's full list), X7 (ids spelled out); G-01 ports the walk and takes `--known-skills`; risk 4 |
| 7 | medium | X13: the soul sickness is 8296, not 8291 | confirmed: 1699's `resurrect skill_id="8296"`; ResurrectEffect.java:28; PlayerReviveService.java:47; PlayerController.java:730-731 | X13 (8296, the 35 % measured before the MAXHP −30 %); kit table; G-01; 10.4 row |
| 8 | medium | X8 and 10.4 claim kills the load path makes impossible | confirmed: PlayerCommonData.java:276 allows 66 for exp above startExp(10) | X8 (class from `CM_LEVEL_READY`'s `SM_PLAYER_INFO`, 1006 from `SM_QUEST_COMPLETED_LIST`, and two Daeva-only rows: glide, page 1352); 10.4 ("skip `updateDaeva`" no longer claims X8; a new `updateDaeva` offline mutation does) |
| 9 | medium | Stage order is circular; stage 2 had two lanes on P5-SC | confirmed | D7 (3); I-05 (C-05 + G-03 + G-05 in one commit); §6 (stage 2 = one P5-SC lane + fixups, stage 1 closes without C-05); G-03 / G-05 deps; §9 |
| 10 | medium | The geo premise is wrong: 519's subeffect 8218 reads geo | confirmed: StumbleEffect.java:69; SkillEngine.java:193-218 (the greatsword critical proc) | W-27; G-04 (the 15 geo readers listed); X9g (a stumble-position row for both gates); D12; §10.5; E-01 / E-02 geo-fixture cases |
| 11 | medium | `SummonRelease`'s two bodies are missing from stage 3 | confirmed: `SummonRelease.cpp:23, 27`, P5-08; Summon.java:210-240; SummonController.java:100; SummonsService.java:192-198 | §2.5; M-01 (+ release-race tests); P5-08 share 25; stage 3 ~102 |
| 12 | low | M-03 needs `SpellAtkDrainInstantEffect` even without stigma | confirmed with the pet-skill closure (`m5e_pets.py`): 22197 on order 3837 is the only unported class | §2.5; M-03; T-02 |
| 13 | low | The checklist's SQL writes REWARD | confirmed: sql/aion_gs.sql:789; QuestStatus.java:11-14 | §11 step 8 (`'COMPLETE'` by name); D8 |
| 14 | low | Counts and citations | confirmed: 12 + 35 = 47 npcs; `PlayerInfo` at PacketDecoders.h:198-208; 14 callbacks + 1 task in 13 classes; `BlindEffect` has only `checkAttackerStatus`; 49 rows; M5c ~195; X16's monster effect is `SM_ABNORMAL_EFFECT` | §1, W-13, §2.3, §2.4, §2.11, X16, §1 closing sentence |
| 15 | low | Correction 1 overstated "no dialog teaches a skill" | confirmed: DialogAction.java:60-61; DialogService.java:199-202 | header correction 1; §2.2 |
| 16 | low | Risk 7 / I-01 describe static initialisers | confirmed: HandlerRegistry.h:33-41; game-server/CMakeLists.txt:95-106; QuestEngine.cpp:102-108 | risk 7; I-01; §12 |
| 17 | low | W-24 understates what the registration wakes | confirmed: `PlayerEnterWorldService.cpp:413`, after the connection is set | W-24; G-05 (a); §10.1 |
| 18 | low | X1's rate, X2's dead mutation, the talk-range positions | confirmed: RatesConfig.java:36, Rates.java:29-33 (no cap); PlayerSkillList.java:61-62; NpcController.java:254, 267; Pernos :926-927 | D9 (quest-XP pin); X1; X2 (new mutations); D8 and §10.1 (spots) |
| 19 | low | W-22, E-03 and the effort letters could use available measurements | confirmed: the AI counts (`m5e_vert_ai.py`), `cycles.toml` has no owner, the git log | W-22; E-03 → I-04; §9 item 4; §12 |

**Also changed, not from the review:** HEAD moved to `760e8ab5c` during the revision (M5b-2 stage 1 part 3 committed); rev 2 re-measured
over it and the direct numbers did not move (status line, A-01). m5c-plan.md rev 2 now hands stigma and `SkillLearnAction` to M5e (§3), and
m5b3-plan.md rev 2's effect subset overlaps this plan's new classes (§3, A-02c). Risk 13 records the lesson of findings 1-4: every planned
cast and every asserted recipient is read from Java before a row is written.

**Findings rejected:** none. **Applied with a refinement:** 1 (no 3-second window is needed), 5 (two classes stay M5b-3's; the level 9-20
count is 39 and the Assassin's 11 with `PoisonEffect`, which the launched skills now reach), 10 (the geo row is bounded rather than exact under
geo, because `tools/oracle/geo` has `getZ` probes but no `getClosestCollision`).
