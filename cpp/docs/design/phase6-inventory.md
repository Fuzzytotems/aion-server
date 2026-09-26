# Phase 6 inventory: what to generate, what to hand-port, and when

> **Status:** inventory **rev 2**, 2026-09-23, revised after an adversarial review; **§14 lists what the review found and what changed.**
> Rev 1 was a **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2") plus the working tree; rev 2 re-checked the findings
> over HEAD `760e8ab5c` ("M5b-2 stage 1 part 3") plus the working tree. **Nothing was compiled, built or run**, and no repository file other
> than this one was written. The inputs are one clustering run over every Java file under `game-server/data/handlers` and six shard
> analyses (S1-S6) of its output. Their scripts and JSON outputs are in the session scratchpad (§13); they are **not** committed. §1 says
> which numbers were measured and which were estimated.
>
> Inputs: [phase5-roadmap.md](phase5-roadmap.md), [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.10 and §3,
> [m5d-plan.md](m5d-plan.md), and the untracked drafts [m5j-plan.md](m5j-plan.md) (rev 1), [m5e-plan.md](m5e-plan.md) (rev 2),
> [m5b3-plan.md](m5b3-plan.md) and [m5f-plan.md](m5f-plan.md). Their milestone assignments are assumptions, and the drafts were being edited
> while this revision was written: **m5j-plan rev 1 moved several owners that rev 1 of this document relied on** (§8.4). Line numbers in
> those drafts are as read for rev 2; where they drift, the item ids (`A-F2`, `J9`, `E-01`, ...) are the stable reference.
>
> Citation conventions: Java handler files are cited relative to `game-server/data/handlers/` in the Java tree (`D:/aion-server/game-server`).
> Java engine classes and C++ files are cited by file name. C++ files live under `game-server/src/aion/gameserver/`, so `Npc.cpp` is
> `model/gameobjects/Npc.cpp`. Hours are agent-hours unless a row says otherwise.

---

## 0. The answer

The phase-6 corpus is **1,729 Java files / 157,966 lines**: quest 1,035 / 95,539, ai 461 / 33,314, instance 78 / 17,155,
commands 152 / 11,823, zone 3 / 135. The ai total includes the 43 root AI handlers, which are phase-5 files (P5-05, `chunks.cmake:290-298`).

1. **Only the quests are repetitive.** At a Jaccard threshold of 0.8 the corpus forms 1,132 clusters. AI is 93% singletons by lines,
   instances 92%, commands 99%, and those shares barely move between thresholds 1.0 and 0.5 (§2.4). The irregularity is real; it is not
   an artefact of the threshold.
2. **Strict parameter tables** (one skeleton, literals in a row) fit **287 quest files / 22.1k lines**: S1's exact groups, 66
   skeletons and 221 clones of them. That is 23% of the quest lines and 14% of the corpus, and each of the 66 skeletons still has to be
   ported by hand or by G1. The 47 table-verdict clusters hold 365 files / 28.6k lines, but 78 of those files are one-offs inside a
   cluster (§4.1). Outside the quests tables fit only about 25-30 files: 4 shield generators, 9 upper-abyss keeps, 4 flying-ring
   instances, 4 renumbered splinter AI copies with their instance twin, the Rentus pair and a few more. A generator does not pay for any of those;
   copying by hand is as fast (§7.5).
3. **The tool that does pay is a restricted Java-to-C++ quest transliterator**, not a table. Quest handlers are closed-vocabulary state
   machines, but their if/switch nesting and fall-through quirks do not survive flattening into a table (§4.5). S2's statement
   classifier, run over all 1,035 quests during the review, puts **660 (64%) in tier A**, which the core emits with no hand edit. The
   tier-B rules (about 12 idiom rules, a ~25-row API table, S4's ~10 verbs) add **254-269** (§7.1), so G1 emits **914-929 files
   (88-90%)** when fully built, and G2 clones 17 more that G1 refuses. Nothing was compiled; these are parser predictions. The 70% bar of
   the plan's open question (`handlers-and-porting-plan.md:558`, `:687`) is cleared only with the idiom rules **and** the API table:
   tier A alone is 64%, tier A plus the idiom-only files 69%.
4. **What must be hand-ported:** about 90-105 quest files (roughly 12-14k lines) if G1 gets every tier-B rule, about 310 files (36k
   lines) with the idiom rules alone; about 435 of the 461 AI files, about 63 of the 78 instance handlers, 150 of the 152 commands and the
   3 zone handlers. In lines, that is about 71-73k of the 158k (45-46%) with every rule. For AI and instances there is no family
   generator; a statement-level assist emits the literal spawn, message, door, skill and delete lines, 3,020 of S6's 17,420 code lines
   (§7.3). Phase-5 API readiness matters more than any generator.
5. **Effort (estimated, §10):** about **2,470 h** to hand-port everything, against about **2,030-2,075 h** on the generated route in the
   configuration this document recommends (U2 rejected, S1's slot cloner kept as G2's N-way mode, the G3 assist credited as a range). The
   saving, about **395-440 h (16-18%)**, comes mostly from the quests: **902 h → ~585 h (-35%)**. Taking U2 would save 45 h more.
6. **Phase-6 work can start now, interleaved with phase 5 (§8, §9).** The tools have no runtime dependency. About 125 AI files call
   only phase-4 or framework code, and another 181 become portable when M5b-2 lands. Quests can be generated now, and they compile
   against the frozen `AbstractQuestHandler.h` (every Java helper is declared) **except the 71 quests, 38 of them tier A, that call
   `HandlerResult.fromBoolean`**: no C++ file declares it, and its companion header is m5d's H-06, a new file (`m5d-plan.md:556`, `:685`).
   R8 pulls that header into the tooling lane. Quests must stay out of the gate builds until M5d's D3 join (`m5d-plan.md:31-37`), because
   registering them wakes hooks into unported bodies. 85 commands run on today's ports once the chat framework core (about 250 Java lines
   of m5j's J1, which m5j's D1 runs right after M5b-2) lands.
7. **Several blockers are owned only by M5j, too late for the lanes that need them (§8.3, §8.4).** Of the **63 effect classes** outside
   the M5b-2 subset, 6 are data-only subclasses of ported bases; M5b-3 (E-02) and M5e (E-01/E-02, M-03) take **29 as required items** and
   5 more as W/O; the other **23 (951 Java lines) sit only in M5j's J9**. After M5e, 46 of the 76 S5 files that touch the 63, including
   19 of the 28 bosses, still wait for J9 (§5.3). `Npc::queueSkill` (`Npc.cpp:200-210`, 29 boss files) is only in M5j L-03. m5j-plan
   rev 1 also leaves to M5j, because no sibling takes them: instance matchmaking (the 17 dredgion, battlefield and arena handlers), the
   instance score classes, the PvP half of the kill reward (34 quests), `CM_WINDSTREAM`, and the kisk, chest and shifter AIs. In M5d the
   quest timers are W, the follow task P and `CM_PLAY_MOVIE_END` W (m5j A-D4 assumes the same); 24 of the 101 M5d-gated scripted quests
   hit one of them. Three root AIs (`AggressiveNoLootNpcAI`, `SummonerAI`, `UseSkillAndDieAI`) stay in M5j although the first needs only
   phase 4 and the other two only M5b-2's cast path; about 39 handler files sit behind them.

---

## 1. Measured and estimated

| Kind | What |
|---|---|
| **Measured** (scripts over the Java and C++ trees; reproducible, §13) | file and line counts per area and chunk; the 1,132 clusters, their verdicts and the threshold sensitivity; override, registration and base-class counts; the XML quest kinds (4,184 elements) and the zero overlap with Java quest ids; import dependencies per file mapped to chunks with `chunks.py`; the quest vocabulary tiers (a call-count heuristic, but counted); S1's exact groups, slot alignment and the 6 value-substitution conflicts; S2's tier A/B/C per file (assigned by a statement parser, not by compiling); S2's pair alignment (47 literal twins, 5 cosmetic, 14 divergent); S3's step-table fit (34 strict, 236 extended) and its 3,297 return leaves / 9,618 paths; S4's resolution of each call site to a C++ definition with or without `AION_UNPORTED`; S5's idiom census and call-based gates (regex); the effect classes reached through direct skill ids and `npc_templates.xml` skill lists; S6's share of literal statements (4,620 of 17,420 code lines); the `AION_UNPORTED` status of every C++ function cited; mirrors recounted by `race_permitted`; the Poeta/Ishalgen distribution (§9.3) |
| **Estimated or judged** | every effort figure (each shard used its own uncalibrated model, §10.3); generator build costs; "fits a row" counts in S4-S6 (from reading the files); family membership in S4-S6 (assigned by reading, one family per file); effective gates beyond imports (from reading and inference; instance reach is inferred from directory names); the transliterator pass rate (predicted from parsing); the de-duplicated totals, the milestone matrix (§8.1) and the lane plan (§9), which were computed for this document; the milestone of every m5j-plan `A-` row |

Measured for rev 2 (scripts in `rev2/` and the reviewer's `review/`, §13): S2's tier classifier over all 1,035 quests
(`review/tierall.json`); the owner of each of the 63 effect classes in the M5b-3, M5e, M5f and M5j drafts, its first player learn level
and the S5 files still blocked after each milestone (`rev2/effowners.py`); the quests calling `HandlerResult.fromBoolean` (71); the
instance handlers that use score or reward classes (24, with subclasses); the quests in instance-named directories per shard; GoTo's
`addLocation` rows (215).

Corrections to the analysis inputs made for this document: S6 labelled its split "ai/instance 186 / instance 78", but `s6/families.json`
recounts it as **206 AI files / 14,934 lines and 58 instance files / 11,073 lines**. S1 cites `kaisinel_academy/_47106...`; the file is
`quest/the_circle/_47106TurningUpTheAmplifiers.java`. S4's "CM_WINDSTREAM has no milestone" and S1/S3's "the PvP half of
`PvpService.doReward` is unscheduled" are settled by m5j-plan rev 1 as **M5j's**: `CM_WINDSTREAM` rides in its stage 1 (item S-06; A-F2,
triggered by m5f O-02; `m5j-plan.md:76`), and the PvP half is its stage-1 item S-12 (A-I2, triggered because m5i declines it in its D16;
`m5j-plan.md:85`, `:647`). Rev 1 of this document had both in earlier milestones (M5f, M5i), following m5j's rev 0.

---

## 2. Method

### 2.1 Normalisation (`cluster.py`)

- Each file was lexed, and comments, `package` and `import` lines were stripped. Imports were kept as dependency data and mapped to their
  owning chunk through `tools/porting/chunks.py` (`Manifest.claims_java`), imported with bytecode writing off.
- Numbers become `N`, strings `S`, chars `C` and boolean call arguments `B`. Every name the file declares (fields, locals, parameters,
  lambda parameters) becomes `V`, private helper methods `M`, declared types `CLS`. ALL_CAPS constants keep their name with digit runs
  replaced by `#` (`SETPRO1` becomes `SETPRO#`); `STR_*` messages become `STR_*`. `final` and `@Override` are dropped.
- What survives is the structure: keywords, control flow, the base class, the overridden methods (marked `@Override` or named like a
  method of the engine base classes) and every API or inherited call name.

### 2.2 Grouping and clustering

1. Exact groups: a hash of the normalised token stream.
2. Near-duplicates: Jaccard similarity of 5-token shingles, clustered within each area by **star clustering** over the exact-group
   representatives. The centre is the representative with the largest size-weighted degree, and every member is at least T from its
   centre. Single-linkage was rejected because it chains unrelated files together.
3. **T = 0.8**, chosen by reading diffs. At 0.808, `quest/eltnen/_1363ThankingMabangtah.java` and
   `quest/theobomos/_3093RecetteSecretedeQuenelles.java` share the if-ladder skeleton (targetId, then `QUEST_SELECT` / `SETPRO#` /
   `SELECT_QUEST_REWARD`) and differ by one npc step and one give/remove item, so one template with options covers both. Around 0.75
   (`_1363` against `quest/reshanta/_1726ScoutingtheLake.java`) the reward branch moves and layout variants start to merge. At 0.9 only
   one- or two-statement variants remain.

### 2.3 Verdicts, tiers and gates

- **Verdicts:** *table-exact*, 3+ files with one exact structure; *table-near*, 3+ files with mean similarity to the centre ≥ 0.9;
  *template*, 3+ files between 0.8 and 0.9; *pair*, 2 files; singletons are *hand-steptable-candidate* (quests that use only the engine
  vocabulary) or *hand*. A *table-near* cluster still admits members down to T = 0.8 (F01's 67 files have 31 distinct structures, one at
  0.808), so a table-verdict cluster is not a table. The strict table fit is S1's exact groups (§4.1).
- **Quest vocabulary tier:** *engine-only* files call only `AbstractQuestHandler` helpers, `QuestEnv`/`QuestState`/`QuestVars`,
  `qe.register*`/`addOn*`, simple getters and `QuestService.startQuest`/`finishQuest`/`collectItemCheck` (spawn and escort helpers
  excluded). *Light* files make 1-2 other calls; *scripted* files make 3 or more.
- **Import gate:** the latest phase-5 milestone (`phase5-roadmap.md:24-35`) among a file's imported P5 chunks. P5-01/02/03/04 map to
  M5b-2, P5-07/09 to M5c, P5-06 to M5d, P5-08/13 to M5f, P5-10 to M5g, P5-11 to M5h, P5-12a/b to M5i, P5-14/15/16 to M5j. P5-05 counts
  as done for the framework and the 3 ported root AIs (`handlers/ai/{AggressiveNpcAI,GeneralNpcAI,NoActionAI}.cpp`), and as M5j for the
  other 40 roots (`phase5-roadmap.md:13`). Mapping P5-08 to M5f is conservative; part of it lands in M5e.
- **Effective gate:** each shard analysis re-derived the gates from what the code calls. S4 and S5 resolved call sites to C++ bodies;
  S1-S3 and S6 read the files. §8.2 lists where the two gates disagree.

### 2.4 Threshold sensitivity (measured)

| Area | T = 1.0 | 0.9 | **0.8** | 0.7 | 0.6 | 0.5 |
|---|---|---|---|---|---|---|
| quest (clusters / singletons) | 724 / 577 | 579 / 407 | **469 / 295** | 352 / 194 | 232 / 105 | 131 / 45 |
| ai | 447 / 435 | 446 / 433 | **441 / 423** | 431 / 408 | 405 / 372 | 379 / 334 |
| instance | 73 / 70 | 69 / 66 | **68 / 64** | 64 / 58 | 61 / 52 | 56 / 45 |
| commands | 152 / 152 | 151 / 150 | **151 / 150** | 147 / 143 | 144 / 139 | 143 / 137 |

### 2.5 Shards and the six analyses

The clusters were cut into six shards of 24-29k lines, each within -7%/+12% of the 26.3k mean. Each shard was then analysed
independently.

| Shard | Content | Files | Lines | What the analysis did |
|---|---|---|---|---|
| S1 | quest tables (table-exact + table-near) | 365 | 28,576 | per-cluster varying slots, clone coverage, per-family schemas (`s1*.py`) |
| S2 | quest templates + 66 race-mirror pairs | 277 | 24,653 | statement parser and A/B/C tiers, flattened step-table fit, token alignment of pairs (`s2/*.py`) |
| S3 | non-mirror quest pairs + engine-only singletons | 264 | 24,875 | path extractor over every hook, guard/effect classification (`s3/steptable.py`) |
| S4 | scripted quest singletons + all commands + zones | 284 | 29,393 | call sites resolved to C++ `Class::method` status, families by dominant verb (`s4_*.py`) |
| S5 | root/world AI + instance slices I2, I5 | 275 | 24,462 | idiom census, call-based gates, effect classes via skill ids (`s5/*.py`) |
| S6 | instance slices I1, I3, I4, I6 | 264 | 26,007 | family reading, hidden dependencies, literal-statement share (`s6/*.py`) |

### 2.6 Limits

- Similarity measures lexical structure, not semantic equivalence. Every "generatable" family was checked by reading its diffs, and
  several near-twins turned out to differ in behaviour (§4.2, §5.2).
- Import gates are a lower bound in some places (calls through base-class helpers are not imports) and too strict in others (an import
  of a type used only in a signature). §8.2 corrects them.
- The light/scripted split is a call-count heuristic. The S2 tiers come from a parser; no emitted C++ was compiled.
- The effort figures come from six different models (§10.3). Read them as orders of magnitude.

---

## 3. Inventory by area (measured)

### 3.1 Overview

| Area | Files | Lines | Clusters at 0.8 | Singletons | table-exact + near | template | pair | hand |
|---|---|---|---|---|---|---|---|---|
| quest | 1,035 | 95,539 | 469 | 295 | 47 cl / 365 f / 28,576 | 12 / 145 / 12,053 | 115 / 230 / 21,999 | 295 / 295 / 32,911 |
| ai | 461 | 33,314 | 441 | 423 | 1 / 4 / 208 | – | 17 / 34 / 1,985 | 423 / 423 / 31,121 |
| instance | 78 | 17,155 | 68 | 64 | 2 / 10 / 685 | – | 2 / 4 / 664 | 64 / 64 / 15,806 |
| commands | 152 | 11,823 | 151 | 150 | – | – | 1 / 2 / 66 | 150 / 150 / 11,757 |
| zone | 3 | 135 | 3 | 3 | – | – | – | 3 / 3 / 135 |

Files per chunk (`chunks.cmake:456-616`): Q01 82, Q02 59, Q03 78, Q04 69, Q05 73, Q06 68, Q07 64, Q08 64, Q09 71, Q10 75, Q11 74, Q12 76,
Q13 87, Q14 95; A1 110; P5-05 43; I1 42+28, I2 49+10, I3 38+18, I4 68+5, I5 53+10, I6 58+7 (AI + instance); C1 102, C2 50; Z1 3.

### 3.2 Quests

- Every one of the 1,035 files extends `AbstractQuestHandler` directly; none extends a template handler. Hooks:
  `AbstractQuestHandler.java:93-285`. Registration: `QuestEngine.java:707-867`, `QuestNpc.java:39-92`.
- Overrides (files): `register` 1,035, `onDialogEvent` 1,026, `onKillEvent` 225, `onItemUseEvent` 133, `onLevelChangedEvent` 120,
  `onQuestCompletedEvent` 103, `onEnterWorldEvent` 48, `onGetItemEvent` 38, `onLogOutEvent` 36, `onEnterZoneEvent` 36,
  `onKillRankedEvent` 26, `onMovieEndEvent` 25. **490 files override only `register` and `onDialogEvent`.**
- Registrations (files): `addOnTalkEvent` 1,026, `addOnQuestStart` 776, `addOnKillEvent` 223, `registerQuestItem` 139,
  `registerOnLevelChanged` 116, `registerOnQuestCompleted` 103.
- Vocabulary tiers: engine-only **716 files / 59,048 lines**, light 175 / 17,267, scripted 144 / 19,224.
- Most frequent non-engine calls (files): `getController` 65, `TeleportService.teleportTo` 48, `spawnForFiveMinutes` 44,
  `CraftSkillUpdateService` 44, `ThreadPoolManager` 36, `InstanceService.getNextAvailableInstance` 20, quest timers 18, group or
  mentor checks 18.
- **XML overlap: none.** `XMLQuests.java:20-27` registers 16 kinds and 15 are used, 4,184 elements in all: item_collecting 1,681,
  monster_hunt 1,163, work_order 574, report_to 468 and 11 smaller kinds. No quest id has both an XML element and a Java handler
  (`m5d-plan.md:115-120`). Several families look like an XML kind but differ in observable state (§4.5), so none of the Java handlers is
  redundant.
- **Mirrors.** `cluster.py` detects Elyos/Asmodian mirrors by the leading digit (1xxx↔2xxx, 3xxx↔4xxx) and finds 66 pairs
  (12.6k lines). Recounted by `quest_data.xml` `race_permitted`, 32 of the 49 "non-mirror" pairs are mirrors too, so there are about
  **98 mirror pairs**. Only literal twins benefit from port-once-plus-substitution (§7.2).
- Cross-chunk: 69 of the 174 multi-file clusters span several of Q01-Q14. The 12 template clusters span 13 of the 14 Q chunks, and 22 of
  the 66 mirror pairs straddle two chunks. **Any generator must be one tool that feeds every Q chunk.**
- `quest-0020` holds 7 empty `steel_rake` stubs (`steel_rake/_3217ImprisonedGuardian.java:6-18`, "TODO: implement"). Port them as
  stubs so the registry report reaches 1,035 (`handlers-and-porting-plan.md:193`).

### 3.3 AI

- Bases: `AggressiveNpcAI` 136, `NpcAI` 96, `GeneralNpcAI` 81, `ActionItemNpcAI` 45, `AggressiveNoLootNpcAI` 17, `SummonerAI` 11,
  `SiegeNpcAI` 7, `EternalBastionAggressiveNpcAI` 6. `@AIName` in 457 files.
- Overrides (files): `handleSpawned` 174, `handleDied` 135, `handleDespawned` 114, `ask` 109, `handleAttack` 101, `handleBackHome` 94,
  `handleDialogStart` 73, `onEndUseSkill` 60, `handleUseItemFinish` 55, `onDialogSelect` 55, `handleHpPhase` 50.
- Only 38 files sit in multi-file clusters: the 4 `ShieldGenerator*AI` (one exact structure) and 17 pairs, mostly the
  `abyssal_splinter` / `unstableSplinterpath` copies.
- Idioms in S5 (regex census): 117 files schedule timers, 68 cast through `SkillEngine.getSkill`, 58 switch on npc id, 49 hold `Future`
  fields, 46 spawn at literal coordinates, 41 override `ask`, 39 use the `ActionItemNpcAI` use-item protocol, 34 use `HpPhases` or
  hand-rolled HP checks, 25 drive state from `onEndUseSkill`.

### 3.4 Instances

- Bases: `GeneralInstanceHandler` 48, `AbstractInnerUpperAbyssInstance` 6, `BasicPvpInstance` 4, `PvPArenaInstance` 4,
  `DredgionInstance` 3, `CrucibleInstance` 2, `DanuarReliquaryInstance` 2. `@InstanceID` in 73 files.
- Two table-near families: the 6 upper-abyss barracks and chambers, and the 4 flying-ring instances. Two pairs: Danuar Sanctuary and its
  Seized variant (0.912), and Rentus Base and Occupied Rentus Base (0.986).
- 4,620 of S6's 17,420 code lines (27%) are literal one-statement calls or case labels: 2,041 spawns, 463 message sends, 94
  `setDoorState`, 193 skill calls, 229 deletes and 1,600 case/break lines. The share is 50% in the raid one-offs and 77% in
  `instance/ShugoImperialTombInstance.java`.

### 3.5 Commands and zones

- 101 `AdminCommand`, 35 `ConsoleCommand`, 16 `PlayerCommand`. 152 exact structures; the only near pair is `Levelup`/`Leveldown` (0.964).
- Zones: `zone/_1012SensoryArea.java` (a `QuestZoneHandler` observer), `zone/pvpZones/PvPZone.java`, `zone/pvpZones/PvPAreaZone.java`.

---

## 4. Quest families

The CORE quest API every family needs is the P5-06 set: the `AbstractQuestHandler` helpers (88 `AION_UNPORTED` sites,
`AbstractQuestHandler.cpp:50-430`), `QuestState.cpp:37-65`, `QuestVars.cpp:24-33`, `QuestEnv.cpp:48` and `QuestService.cpp:82,219,223,284`.
All arrive with M5d. The dialog path in (`CM_SHOW_DIALOG`, `CM_DIALOG_SELECT`, `CM_CLOSE_DIALOG`, `DialogService`) comes with M5c stage 0 or
M5d D4 (`m5d-plan.md:505`). Reward delivery needs `ItemService::addItem` (`ItemService.cpp:41-65`) and `ItemPacketService::sendItemPacket`
(`ItemPacketService.cpp:19`), both M5b-3. Already ported: `QuestEngine` register/dispatch (`QuestEngine.cpp:710-855`), `QuestNpc::addOn*`
(`QuestNpc.cpp:25-44`), the `AION_QUEST_HANDLER` marker (`HandlerRegistry.h:317-325`; 0 quest handler files today) and the `DialogAction`
constants brought in by `QuestPrelude.h:82`. The tables below list only what a family needs **beyond** that core.

### 4.1 S1: the table-verdict families (365 files, 28,576 lines; 287 in exact groups)

| Family | Clusters | Files | Lines | Gen. | Row / route | Needs beyond the core | Hand h | Gen h |
|---|---|---|---|---|---|---|---|---|
| F01 talk chain (`eltnen/_1363ThankingMabangtah.java:37-68`) | 0001 | 67 | 5,637 | partly, **via G1 only** | 11 exact groups, 36 clones, 20 variants. The row {questId, startNpc, step npcs 1-4, finalVar, stepClose, startableCheck, acceptItem?, rewardRemoveItem?} cannot express the per-step page and `SETPRO` (`theobomos/_3008WickedThiefKelaino.java` uses 1693 with `SETPRO2`) or an extra `updateQuestStatus` (`_3008:48-51`) | – (`SM_DIALOG_WINDOW` done) | 33.5 | 17.3 |
| F02 Beshmundir weapons (`beshmundir/_30235NewSword.java:22-91`) | 0003 | 24 | 2,208 | yes | {questId, startNpc, requiredWeapon, questItem} | kills inside Beshmundir (M5f) | 12.0 | 1.7 |
| F03 crafting exams | 0004, 0012, 0023, 0034 | 44 | 3,381 | yes | EXPERT / MASTER / CONSTRUCTION / GATHER_EXPERT rows; 37 clones | `CraftSkillUpdateService.canLearnMore*` (M5c, `CraftSkillUpdateService.cpp:103,107`) | 30.8 | 6.8 |
| F04 daevanion kill-10 | 0005 | 24 | 2,081 | yes, **via G1 only** | {questId, startNpc, endNpc, mobIds[2..6], noUseObjectPage}: `_19638TroublewithTwos.java:17` has 6 mob ids; all 24 loop over the ids outside `register()` (tier B) | – | 12.0 | 3.9 |
| F05 rank kills | 0008, 0042, 0043 | 26 | 1,394 | yes | {questId, npc, rank, count, rewardAction} | ranked-kill dispatch = PvP half of `PvpService.doReward` (M5j S-12; m5j A-I2, triggered); Reshanta (M5f) | 13.0 | 4.4 |
| F06 ascension dispatches | 0009 | 12 | 924 | yes | {questId, talkNpc, endNpc} | start only after 1007 / 2009 (`quest_data.xml:48056` for 19070, `:8035` for 1913), which follow Ascension 1006 / 2008: M5f (§4.4) | 6.0 | 1.1 |
| F07 mentor dailies (`the_circle/_47106...`, `kaisinel_academy/_37003...`) | 0010, 0026 | 18 | 1,522 | yes, **via G2's N-way mode only** (all 18 contain a closure, e.g. `_37003:59`; G1 refuses them) | KILL / ITEM rows | group + mentor (M5g, P5-10) | 18.0 | 3.8 |
| F08 Dark Poeta kills | 0016 | 8 | 538 | yes | {questId, npc, splitVar, endVar, phases} | Dark Poeta (M5f) | 4.0 | 1.3 |
| F09 growth charms | 0018 | 8 | 690 | yes, **via G1 only** | {questId, startNpc, helperNpc, startItem, powder?, kinah}: one kinah column would silently fix the double charge of `sanctum/_3963...:69-70` and `_3964...:69-70` (§11) | `sendItemPacket` (M5b-3) | 5.6 | 3.7 |
| F10 steel_rake stubs | 0020 | 7 | 133 | yes | {questId, class} | registry only | 0.7 | 0.4 |
| F11 level-up recruits and letters | 0022, 0041, 0057 | 13 | 765 | partly | RECRUIT / LETTER / ORDERS rows | register after the M5d D3 join | 6.5 | 3.8 |
| F12 event bonus | 0024, 0030, 0036 | 15 | 1,022 | yes | {questId, npc, bonusType, workItem?} | `Event.onPlayerLogin` (M5i, `Event.cpp:46`); `BonusService.getQuestBonus` (M5d E-09) | 7.5 | 3.0 |
| F13 spy orders | 0025 | 6 | 546 | yes | {questId, startItem, npc, mobs[3], kinah} | `CM_USE_ITEM` (M5b-3) | 4.2 | 0.9 |
| F14 item-started talk | 0014, 0028 | 13 | 1,192 | partly | F01 row + startItem; 3 clones | `CM_USE_ITEM` (M5b-3); Abyssal Splinter (M5f) | 6.5 | 5.1 |
| F15 mirror talk / turn-in | 13 clusters | 47 | 3,385 | partly | per exact group; 12 clonable | `playQuestMovie` (M5d) | 24.5 | 19.1 |
| F16 zone / instance steps | 0039, 0044, 0045 | 12 | 1,036 | yes | LEVINSHOR / TIAMAT rows | `CM_USE_ITEM` (M5b-3); Tiamat Stronghold (M5f) | 6.8 | 3.0 |
| F17 scripted small | 6 clusters | 21 | 2,122 | no | – | follow task (M5d stage 3); teleport for `_1693` | 18.9 | 15.7 |
| **Total** | 47 | **365** | **28,576** | | 66 representatives + 221 clones + 78 one-offs | | **210.5** | **94.9 + 14** |

**What a strict table covers** (`s1clone.json` `_totals`): 287 files / 22,114 lines sit in exact groups of 2 or more, with **66
distinct skeletons** (5,111 lines), 215 clean clones and 6 clones that need positional maps. That is 23% of the quest lines and 14% of the
corpus. The other 78 files (6,462 lines) are one-offs inside their clusters, and F17 (21 files) is not generatable at all. Each of the 66
skeletons is still ported by hand or emitted by G1.

S1's recommended route was a **slot cloner** (14 h): port the 66 exact-group representatives, then rewrite the literals of each member
slot by slot, aligned by Java token position. It covers 221 files. Substituting by value fails in 6 files (`_3076`, `_1799`, `_47106`,
`_80228`, `_80229`, `_3970`), because one literal plays different roles in different members. Rev 1 of this document said G1 subsumes the
cloner; **the review showed it does not**. S2's classifier puts 222 of the 365 files in tier A (not the 262 "engine-only" files of the
vocabulary tiers), 116 in tier B (40 idiom-only: F04's 24 loops outside `register()`, F08 8, `quest-0022` 6, `quest-0057` 2; 76 with
non-quest API calls) and **27 in tier C because they contain closures**: F07's 18 mentor dailies (`quest-0010`, `quest-0026`;
`kaisinel_academy/_37003CamouflageKillers.java:59` runs a lambda over `group.getMembers().stream().anyMatch` and then calls
`deleteAndScheduleRespawn` and `spawnForFiveMinutes`), 6 anonymous-`Runnable` use-bar quests (`quest-0052`, `quest-0053`) and 3 silentera
quests (`quest-0059`). G1 refuses all 27. **The cloner therefore stays, as G2's N-way mode (§7.2, 14 h)**: 3 representatives
(`_37003`, `_37000`, `_11031`) and 7 one-offs are hand-ported (10 files, 936 lines), and 17 clones (1,508 lines; `_47106` positional)
are cloned. Rev 1's "S1 would drop to ~75 h with G1" is withdrawn.

### 4.2 S2: templates and mirror pairs (277 files, 24,653 lines)

| Family | Files | Lines | Tiers A/B/C | Needs beyond the core | Hand h | Tool h | Review h |
|---|---|---|---|---|---|---|---|
| F1 helper-style talk/kill chains (`pandaemonium/_2921LoveAtFirstSight.java:29-83`) | 72 | 5,523 | 72/0/0 | – | 29 | 28 (core, shared) | 5 |
| F2 item-use starts (`gelkmaros/_21137BerokinImageMarble.java:23-73`) | 19 | 1,474 | 17/2/0 | `CM_USE_ITEM` (M5b-3); `HandlerResult.fromBoolean` (M5d H-06) | 8 | 1 | 1.5 |
| F3 manual-style talk chains (`inggison/_11109TheNegotiators.java:32-82`) | 48 | 4,664 | 48/0/0 | – | 24 | 3 | 3.5 |
| F4 auto-start orders (`morheim/_24020AegirsOrders.java:22-65`) | 6 | 392 | 2/4/0 | link after the D3 join | 2.5 | 1 | 0.5 |
| F5 engine-only mirror pairs | 80 | 6,242 | 78/2/0 | ~40 in instance directories (M5f reach); dredgion reward, PvP kills, `CM_PLAY_MOVIE_END` sources | 36 | 2 | 6 |
| F6 scripted mirror twins | 32 | 3,558 | 0/28/4 | teleport, master crafting (M5c), housing (M5h), siege (M5i), follow (M5d stage 3), timers (M5d E-05) | 37 | 11 | 12 |
| F7 divergent scripted pairs | 20 | 2,800 | 0/14/6 | instance entry (M5f), spawn helpers (M5d H-05), a closure (`heiron/_3200PriceOfGoodwill.java:102,134`) | 26.5 | 0 | 18 |
| **Total** | **277** | **24,653** | **217/50/10** | | **163** | **46** | **46.5** |

- Tier A (217 files / 17,701 lines) uses only an ~80-name quest vocabulary and plain if/switch/return, and generates with no hand edit.
  Tier B (50 / 5,171) generates after about 12 emitter rules for idioms and ported APIs. Tier C (10 / 1,781) is hand work.
- **A flat step table does not cover the skeletons.** Flattening the 224 vocabulary-clean files into guard→effect rows needs 2,470
  negated atoms; 67 files guard on effectful calls (`if (giveQuestItem(...))`, `if (QuestService.startQuest(env))`), and 10 do a side
  effect and then fall through. A table that keeps the nesting is a serialised Java AST, so the code should be emitted as C++ directly.
- **Mirror pairs:** 52 of the 66 are one port plus one substitution row (47 literal twins, 5 cosmetic). 14 differ in behaviour and must
  be ported file by file, for example `_28036InterrogateKvash.java:47`, an extra `updateQuestStatus`, and `_3200` against `_4200`,
  `UNKNOWN` against `FAILED` on the wrong step (`_4200ASuspiciousCall.java:128-132`). Never merge a pair into one class.

### 4.3 S3: engine-only singletons and non-mirror pairs (264 files, 24,875 lines)

| Family | Files | Lines | Gen. | Extra hooks and needs | Hand h | Gen h |
|---|---|---|---|---|---|---|
| F1 talk-chain singletons (`altgard/_2231SiblingRivalry.java:22-81`) | 83 | 6,965 | yes | dialog only | 50 | 18 |
| F2 mission chains (`altgard/_24011FunnyFloatingFungus.java:31-110`) | 29 | 3,694 | 26 yes / 3 partly | `defaultOnLevelChanged`/`defaultOnQuestCompleted` (M5d); register after the engine bodies | 29 | 13 |
| F3 kill + talk | 23 | 2,171 | yes | `defaultOnKillEvent` overloads | 17 | 7 |
| F4 item-use start/progress | 14 | 1,223 | yes | `CM_USE_ITEM` (M5b-3); `useQuestItem` | 11 | 6 |
| F5 zone / get-item / at-distance / movie triggers | 17 | 1,423 | 16 yes / 1 partly | `CM_PLAY_MOVIE_END` (M5d, W) | 13 | 9 |
| P1 exact-twin pairs | 24 | 1,870 | yes | `quest-0162`: `onKillInWorld` (PvP half, M5j S-12) | 11 | 6 |
| P2 near-twin pairs (real structural variants) | 36 | 3,278 | yes, per file | – | 25 | 9 |
| P3 engine-extension pairs (timers, event start, emotions, escort, flying rings) | 10 | 1,112 | partly | timers E-05, follow E-07 (M5d stage 3), `startEventQuest` | 15 | 8 |
| P4 scripted pairs | 28 | 3,139 | partly | spawn helpers, pinned tasks, escort with `QuestTasks`, geo spawn ring | 48 | 38 |
| **Total** | **264** | **24,875** | | | **219** | **114 + 45 core** |

- A strict 4-column table (npc, var, dialog page, action) expresses only **34 files (2,515 lines, 13%)**. A first-match rule table with
  guards on status, var slot or range, inventory count, zone, used item, movie id, other quest state and class, and about 30 engine
  actions, covers **236 files (21,736 lines, 87%)**. With the 10 light P4 files, which need only pass-through calls to ported APIs, it
  covers **246 (22,793, 92%)**. The hand remainder is **18 files (2,082 lines)**.
- Flattening explodes from 3,297 return leaves to 9,618 paths and loses Java's fall-back quirks. `_2231:70-74` accepts
  `SELECT_QUEST_REWARD` at any var, where `ReportToMany.java:127` requires the last step. S3 therefore also recommends the transliterator
  and keeps the path extractor as a **per-quest oracle** (§7.6).
- Poeta and Ishalgen: S3 holds 18 of the 30 Java quests of those two maps that m5d-plan D9 leaves missing, including the Poeta mission
  chain 1001-1005 (§9.3). 78 XML-template quests name an S3 quest as a `finished` precondition, so porting S3 unblocks them.

### 4.4 S4: scripted quest singletons (129 files, 17,435 lines) and zones

| Family | Files | Lines | Gen. | Needs beyond the core (effective gate) | Hand h |
|---|---|---|---|---|---|
| Q-ASC Ascension 1006 / 2008 | 2 | 592 | no | `ClassChangeService` (M5e), instance creation (M5f), AI `ascensationquestnpc` (A1), `WebRewardService.MaxLevelReward.isPendingAscension` | 12.5 |
| Q-DELAY hand-rolled use bars | 15 | 2,052 | no | pinned `ThreadPoolManager` closures; `CM_USE_ITEM` (M5b-3) | 36.0 |
| Q-ESCORT | 8 | 862 | 6 with verbs | E-07 follow task (M5d stage 3, P); `FollowingNpcAI` for 6 (M5j, or a capital-economy milestone after M5f if the user takes m5j A-C4 (a), `m5j-plan.md:63`) | 17.5 |
| Q-FLIGHT scripted flights | 3 | 298 | one verb | ported (`Player.cpp:634`); flight paths at run time (M5f) | 4.2 |
| Q-INST campaign instances | 11 | 2,435 | no | `InstanceService.getNextAvailableInstance` (`InstanceService.cpp:81-93`, M5f); `CM_PLAY_MOVIE_END` for 4 | 46.0 |
| Q-HPTRIG attack/approach triggers | 3 | 312 | one verb | ported callers | 5.0 |
| Q-CHECKS one extra check | 28 | 3,236 | 25 with verbs | `fromBoolean` (H-06); `ItemService::addItem` for `_14014` (M5b-3) | 53.5 |
| Q-SKILL | 1 | 79 | one verb | `SkillEngine.cpp:112-131` ported | 1.8 |
| Q-SPAWN spawn / despawn steps | 25 | 2,595 | 14 with verbs | spawn helpers H-05; AIs `following`, `onedmg_passive`, `quest_use_npc`, `quest_start_use_item` for 5 | 41.8 |
| Q-TELE teleport steps | 24 | 3,881 | 13 with verbs | 18 call only ported `worldId` overloads (`TeleportService.cpp:236-282`) → M5d; 4 need the unported delegations `:251-271` and `_2007` needs `teleportToNpc` → M5f; `_11076` needs `CM_WINDSTREAM` (M5j stage 1, m5j A-F2) | 71.8 |
| Q-TIMED quest timers | 9 | 1,093 | 5 with verbs | E-05 `questTimerStart/End` (M5d, only W today) | 19.8 |
| **Quests total** | **129** | **17,435** | ~63-78 | effective: M5d 101, M5f 18, M5j 10 | **309.75** (per file; the rounded rows sum to 309.9) |
| Z-ZONES | 3 | 135 | no | `_1012`: QuestState (M5d); `PvPZone`: `PlayerReviveService.duelRevive` (M5f); `PvPAreaZone`: `teleportTo` instanceId delegation | 3.5 |

- No family here is a parameter table; the sub-families have mean pairwise Jaccard 0.38-0.54. The idiom repeats, but the dialog ladders
  around it do not. If the step vocabulary of §7.1 gains about 10 verbs (teleport, spawn/despawn, timer, escort, flight, cheap checks),
  **63 named files** (78 by S4's verb count) fit, at 119.5 hand-hours. The other ~51 (18 with scheduled closures, 13 that open instances
  or change class, 20 with 2-3 verbs) are hand ports.
- Mirrors worth porting once and diffing: 1006/2008 (0.70), 1922/2947 (0.68), 1007/2009 (0.67).
- Ascension 1006/2008 gates **all** progression past level 9 (`m5c-plan.md:93-100`). `isPendingAscension` is a set lookup
  (`WebRewardService.java:102-104`) that is false unless web rewards granted the pending state, so the quest needs only that one method,
  not the J12 custom-content port.

### 4.5 What the quest analyses agree on

- **Emit C++ statement trees, one `.cpp` per Java file; do not build a runtime table.** A runtime interpreter would break the porting
  rules (one `.cpp` per Java file, literals stay literals, markers, `tools/parity`; `handlers-and-porting-plan.md:550-556`), and a flat
  table loses behaviour (§4.2, §4.3).
- **Do not convert Java handlers into XML rows.** The nearest kinds differ in observable state:
  - F01 against `report_to_many`: Java does not check the step at the report npc (`_1363:43-45`), and it closes a step with
    `SM_DIALOG_WINDOW(obj,10)` sent directly (`_1363:59-62`) where `closeDialogWindow` sends (0,0) (`AbstractQuestHandler.java:359-361`).
    None of the 67 F01 files calls `defaultCloseDialog`.
  - S2-F1 (`quest-0002`, 46 files) against `report_to_many`: every file closes through `defaultCloseDialog`, which fires
    `AIEventType.DIALOG_FINISH` (`AbstractQuestHandler.java:525`); `ReportToMany.java:115-118` does not. (Rev 1 put this difference
    under F01.)
  - F04 against `monster_hunt`: Java leaves var1 at 9 where `MonsterHunt.java:212-216` writes 10.
  - F11 against `report_on_levelup`: Java starts in START, `ReportOnLevelUp.java:63` in REWARD.
  - S2-F2 against `item_order`: `ItemOrders.java:68-72` refuses the accept when the item is missing; Java does not check.
- **Keep the Java registration order**, because it orders the npcs' quest lists (S1 F01 risk).
- **Link the quest libraries only after M5d's D3 join.** Registering onLevelChanged/onEnterWorld handlers (29 in S3-F2 alone; F11, S2-F4,
  the prologues `_1000`/`_2000`) makes every fresh character's first enter world call `QuestService::startQuest`
  (`m5d-plan.md:31-37`).

---

## 5. AI and instance families

### 5.1 S5: root AIs, world AI and instance slices I2/I5 (275 files, 24,462 lines)

| Family | Files | Lines | Gen. | Main needs | Hand h | Gen h |
|---|---|---|---|---|---|---|
| F01 root AIs already in C++ | 3 | 206 | – | `GeneralNpcAI::chooseSkillAttack` stays `AION_PARTIAL` (`GeneralNpcAI.cpp:122`) until M5b-2 N-02 | 0 | 0 |
| F02 policy-only (`ask`/damage/stat overrides) | 19 | 514 | partly | 14 of 19 portable today; 9 of them are roots used by 12 other S5 files | 14.25 | 4 |
| F03 root interaction bases (`ActionItemNpcAI`, `ChestAI`, `KiskAI`, `PostboxAI`, ...) | 15 | 1,002 | no | `DialogService::isInteractionAllowed` (`DialogService.cpp:27`, M5c); drops (M5b-3); kisk (M5j stage 2, m5j A-F2) | 27.25 | 0 |
| F04 root behaviour bases (`SummonerAI`, `BombAI`, `TrapNpcAI`, `ServantNpcAI`, ...) | 14 | 1,066 | no | M5b-2 cast path; `SummonerAI` is the base of 5 bosses | 29.0 | 0 |
| F05 Illuminary Obelisk shield generators | 5 | 353 | yes | all ported; 4 rows over one base (§7.5) | 10.25 | 4 |
| F06 cast-and-expire minions | 30 | 1,252 | partly | M5b-2 cast; `Npc::queueSkill`; 7 cast effects outside the subset | 36.0 | 12 |
| F07 spawn on death or timer | 19 | 815 | partly | 15 portable today | 21.75 | 5 |
| F08 proximity triggers | 8 | 424 | no | an `ActionObserver` subclass for `GaleCycloneAI` | 11.25 | 0 |
| F09 boss scripts | 42 | 5,583 | no | M5b-2 cast (32); `queueSkill` (13); effect classes outside M5b-2 (28; 19 still wait for M5j's J9 after M5e, §5.3) | 156.75 | 0 |
| F10 dialog npcs (buffers, key teleporters, stage triggers) | 19 | 1,133 | partly | `QuestEngine::onDialog` (M5d); teleport (M5f); crucible score (M5j L-03 today, R7) | 30.5 | 9 |
| F11 portals and gates (`PortalAI`, `PortalDialogAI`) | 15 | 996 | no | `PortalService` (`PortalService.cpp:10,15,63`, M5f); auto-group arms (matchmaking: M5j X-01 today, R11). They serve 609 npc templates | 26.25 | 0 |
| F12 use-item npcs | 15 | 922 | no | `ActionItemNpcAI` root (M5d D5); `QuestItemNpcAI` already in M5d (`m5d-plan.md:592`) | 25.25 | 0 |
| F13 siege, base, Panesterra, events | 33 | 2,807 | no | `SiegeService` (partly ported), `BaseService`, `PanesterraService`, `AhserionRaid` (M5i) | 75.75 | 0 |
| F14 walkers, escorts, leashes | 10 | 647 | no | `WalkManager` ported; `NpcShoutsService` (M5j) for `NaiaAI` | 17.25 | 0 |
| F15 instance room mechanics | 8 | 660 | no | their instance handler | 18.75 | 0 |
| F16 instance handlers of I2/I5 | 20 | 6,082 | partly | `GeneralInstanceHandler` helpers (`GeneralInstanceHandler.cpp:35-66`), `InstanceService` (M5f); score classes (M5j L-03 today, R7); Stonespear also M5h | 208.5 | 10 |
| **Total** | **275** | **24,462** | | | **708.75** | **~675 route** (~630 with U2) |

Rows written by hand into a few data-driven C++ classes (SkillMinionAI, SpawnOnEventAI, BuffGiverAI, KeyTeleporterAI, StageTriggerAI)
could absorb about 58 small files and save about 45 h. That conflicts with the one-`.cpp`-per-Java-file rule (decision U2, §12), which
this document recommends keeping. S5's own ~630 h route (709 − 45 − 7 − 20) assumed the classes; rev 2 uses **~675 h** as the baseline
and shows U2 as an optional −45 h.

### 5.2 S6: instance slices I1/I3/I4/I6 (264 files: 58 instance / 11,073 lines, 206 AI / 14,934 lines)

| Family | Files | Lines | Gen. | Main needs | Hand h | Gen h |
|---|---|---|---|---|---|---|
| F-I01 upper-abyss keeps (`AbstractInnerUpperAbyssInstance.java:40-196` + 9 subclasses) | 10 | 674 | yes (9) | instance engine (M5f); `AbyssPointsService.addAp` (M5d E-09) | 14.4 | 2.5 |
| F-I02 flying-ring timed chests (`abyss/AsteriaInstance.java:22-68`) | 5 | 361 | yes (4) | instance engine; `FlyRing` ported | 8.2 | 1.5 |
| F-I03 create-time random spawns | 4 | 149 | partly | instance engine | 3.8 | 0 |
| F-I04 small scripted dungeons | 10 | 1,006 | no | `instanceRevive` (`PlayerReviveService.cpp:102-108`), instance teleports, `CM_PLAY_MOVIE_END` | 23.1 | 0 |
| F-I05 Abyssal Splinter / Unstable Splinterpath | 2 | 422 | partly | port one, derive the other by diff | 10.3 | 0.5 |
| F-I06 Dredgion family | 4 | 808 | no | `AutoGroupService` entry (M5j X-01 today, R11); `DredgionRoom` and score classes (no C++ file; M5j L-03 today, R7) | 19.6 | 0 |
| F-I07 PvP battlefields | 5 | 1,266 | no | the same entry (R11); `PvpInstanceScore` (no file); score writer | 31.4 | 0 |
| F-I08 PvP arenas | 8 | 1,395 | partly | the same entry (R11); `PvPArenaScore` (no file); `SM_INSTANCE_SCORE` arena ctor (`SM_INSTANCE_SCORE.cpp:35`) | 31.0 | 0 |
| F-I09 raid-scale one-offs (Shugo Tomb, Eternal Bastion, Drakenspire, Vault, Dragon Lords' Refuge, ...) | 10 | 4,992 | no | instance engine; `NormalScore` (no file; R7) for Eternal Bastion and the Vault; `CM_INSTANCE_LEAVE`; `CM_OPEN_STATICDOOR` (M5j, not blocking) | 130.5 | 0 |
| F-A01 splinter AI copies | 21 | 1,685 | partly | 4 id-only copies; `queueSkill`; `SummonerAI` root | 41.9 | 2.0 |
| F-A02 skill tickers and timed hazards (`BladeStormAI.java:15-43`) | 30 | 1,669 | partly | M5b-2 cast; `UseSkillAndDieAI` root for 2 | 48.1 | 0 |
| F-A03 proximity auras (`EarthQuakeAI.java:16-58`) | 10 | 492 | partly | M5b-2 cast: all 10 cast (`EarthQuakeAI.java:36`, `AIActions.useSkill`); `EffectController::hasAbnormalEffect` ported | 12.9 | 0 |
| F-A04 dialog npcs | 15 | 906 | no | `CM_SHOW_DIALOG`/`CM_DIALOG_SELECT` (M5c stage 0) | 21.8 | 0 |
| F-A05 use-item npcs | 31 | 1,709 | no | `ActionItemNpcAI` (M5d) 24; `ShifterAI`/`ChestAI` (M5j stage 3 per m5j A-F3, unless R1) 5; `PortalAI` (A1) 1 | 42.5 | 0 |
| F-A06 HP-phase bosses | 24 | 2,600 | no | `HpPhases` ported; `queueSkill` in 6 | 63.1 | 0 |
| F-A07 timed-task bosses | 22 | 2,390 | no | `AggressiveNoLootNpcAI` for 7; `queueSkill` in 4 | 64.8 | 0 |
| F-A08 walkers and wave spawners | 21 | 1,532 | no | walker data and geo; `OneDmgAI` for 1 | 39.7 | 0 |
| F-A09 `SummonerAI` subclasses | 6 | 465 | no | `SummonerAI` root (146 lines, phase-4 deps) | 13.9 | 0 |
| F-A10 trivial overrides | 19 | 706 | no | `AggressiveNoLootNpcAI` for 5 | 17.3 | 0 |
| F-A11 custom content (config-off) | 7 | 780 | no | decision D12 (`m5j-plan.md:589`) | 17.3 | 0 |
| **Total** | **264** | **26,007** | | | **~656** | **~573-603 route** |

Only 23 files (about 1,500 lines) can come from a table or a copy, saving about 23 h (3.5%). S6 credited a statement-level
transliteration pass with 92 h; rev 2 credits **30-60 h** (§7.3), so the route is a range.

### 5.3 Cross-cutting AI and instance findings

- **Root AIs.** 49 S5 files and about 53 S6 files depend on a root AI that has no C++ file. m5d D5 already pulls `ActionItemNpcAI`,
  `AbyssGuardSimpleAI` and `QuestItemNpcAI` into M5d (`m5d-plan.md:506`). m5j-plan rev 1 (§2.5, `m5j-plan.md:333-352`) puts `ResurrectAI`,
  `PortalAI` and `PortalDialogAI` in M5f (A-F1) and `ButlerAI`/`HouseSignAI` in M5h (A-H1). **Everything else is M5j's**: `summoner`,
  `aggressive_no_loot`, `useSkillAndDie`, `bomb` and the other roots rev 1 of this document already had there, and, because no sibling
  takes them, the kisks (A-F2, stage 2), `ChestAI`, `HiddenTeleportNpcAI` and `ShifterAI` (A-F3) and `OneDmgAI`/`OneDmgNoActionAI` (A-H2),
  all in stage 3. 14 handler AIs extend one of those seven roots (chest 4, shifter 3, `OneDmgAI` 4, `OneDmgNoActionAI` 3; for example
  `instance/pvpArenas/HarmonyShifterAI`, `instance/rakes/SteelRakeKeyBoxAI`, `events/FakeCakeAI`). A missing AI name throws `IllegalArgumentException` (`AIEngine.cpp:155-170`) unless
  `gameserver.dev.missing_ai_handlers=warn`, which substitutes `DummyNpcAI` and logs one warning per name. The gap is loud by default and
  quiet only in that development profile.
- **Effect classes.** The skill ids these scripts cast (182 ids in 97 S5 files), together with the `npc_skills` lists of their npcs,
  need **63 effect classes outside the 34-class M5b-2 subset**, 2,611 Java lines in all. The most common are Silence, Paralyze, Poison,
  Dispel, Blind, Fear, CloseAerial, Deform and Sleep. 76 S5 files (7,171 lines) touch them, including 28 of the 42 bosses. M5b-2 keeps
  them `AION_UNPORTED` (`m5b2-plan.md:348`, D6) and O-01 (`:453`) defers them to "M5b-4 or phase 6". **Rev 1 said no plan owns them;
  that was wrong.** Their owners in the current drafts (`rev2/effowners.py`):

  | Owner | Classes | Which |
  |---|---|---|
  | none needed: data-only subclasses of ported bases (no `.cpp`; `BufEffect.cpp` and `TransformEffect.cpp` have 0 `AION_UNPORTED`) | 6 | APBoost, AbsoluteSnare, DRBoost, DeboostHeal, ShapeChange, SkillXPBoost |
  | M5b-3 E-02, R (`m5b3-plan.md:459`) | 5 | Blind, Paralyze, Poison, ProcAtkInstant, Silence |
  | M5e E-01/E-02, R (`m5e-plan.md:314-315`), and M-03, R (`:561`) | 21 + 3 | AbstractDispel, AlwaysBlock, Bind, BoostSkillCastingTime, CarveSignet, Curse, Deform, DelayedSkill, DelayedSpellAttackInstant, DispelBuffCounterAtk, DispelDebuff, DispelDebuffPhysical, Dispel, Fall, FpAttackInstant, HostileUp, MPHealInstant, SignetBurst, SkillAtkDrainInstant, Sleep, TargetChange; Summon, SummonServant, SpellAtkDrainInstant |
  | W/O: M5b-3 E-03, W (`m5b3-plan.md:460`); M5e T-02, O (`m5e-plan.md:566`) | 2 + 3 | Fear, MpAttackInstant; ConvertHeal, DispelBuff, MagicCounterAtk |
  | **M5j J9 only** (stage 3; `m5j-plan.md:112`, `:252-257`) | **23** (951 Java lines, 54 sites) | npc- or item-only: AbsoluteStatToPCBuff, AbstractAbsoluteStat, BuffStun, Confuse, DPHeal, DelayedFpAtkInstant, Disease, DispelNpcBuff, MpAttack, NoFly, NoReduceSpellATKInstant, OpenAerial, TargetTeleport, XPBoost; player skills first learned at levels 25-55, outside M5e's level-20 closure: BuffBind, CloseAerial, DispelDebuffMental, FPHeal, FPHealInstant, FpAttack, Protect, Search, SpellAtkDrain |

  After M5b-3's and M5e's required items, **46 of the 76 S5 files still need a J9 class, including 19 of the 28 bosses** (40 and 15 if
  every W/O item is taken). M5f's optional X-05b (`m5f-plan.md:454`, the Eltnen/Morheim monster classes) would take 7 of the 23
  (Disease, Confuse, FpAttack, DispelDebuffMental, CloseAerial, DelayedFpAtkInstant, Protect); with it, 32 files and 13 bosses remain.
  The review proposed "43 in M5e, 20 in J9", counting every class a player skill of any level reaches, through m5j's rev-0 wording of A-E4
  ("every skill its trainers teach"). M5e's D4 closes its scope at level 20 (`m5e-plan.md:504`), and m5j rev 1 now words A-E4 the same
  way (`m5j-plan.md:73`), so 9 player classes stay in J9.
- **Callbacks.** 117 S5 files schedule timers, and the raid instances keep lists of `Future`s. Every capture must be pinned (at most 4
  pin slots, no `[&]`/`[=]`, `conventions-game-server.md:12`), and back-references need `cycles.toml` entries
  (`handlers-and-porting-plan.md:806`). This rule, not the data, is the main per-file hazard.
- **Header requests.** `NpcAI::getSpawnTemplate` is non-virtual (`NpcAI.h:44`), but 5 Java AIs override it covariantly
  (`SiegeNpcAI`, `BaseProtectorAI`, `ArtifactAI`, `GateRepairAI`, `Kisk`); `SiegeWeaponAI` derives from `AITemplate` directly.
- **Probably dead AI names.** 9 files (521 lines) declare an AI name that no npc template, spawn, event file or handler references:
  `StartTeleportAI`, `IncarnateAI`, `GuardianGeneralAI`, `DynatoumHealerAI`, `RvrBossAI`, `AhserionAssaultPod`, `FakeCakeAI`,
  `AhserionAssaultCommanderAI`, `BollvigAI` (decision U4).
- **Existing inheritance is the parameterisation.** `DanuarReliquaryInstance` (→ `_L`, Infernal), `IlluminaryObeliskInstance`,
  `CrucibleInstance`, `DynatoumAI`, `AdvanceCorridorAI`, `PortalAI`→`PortalDialogAI` and `SiegeNpcAI` already factor their families.
  Keep the hierarchy and port each base before its subclasses.

---

## 6. Commands (S4: 152 files, 11,823 lines)

| Family | Files | Lines | Runs on today's ports once the chat framework exists | Otherwise waits for | Hand h |
|---|---|---|---|---|---|
| C-CHAR cheats and toggles (incl. Levelup/Leveldown) | 32 | 1,522 | 28 | M5e 3, M5f 1 | 26.2 |
| C-INFO inspection and chat | 18 | 1,143 | 16 | M5g 1, M5i 1 | 16.8 |
| C-ITEMS grants and edits | 18 | 1,709 | 5 | M5b-3 7, M5c 5, M5f 1 | 26.2 |
| C-LEGION legion, house, auction, pet | 6 | 599 | 0 | M5f 1, M5g 1, M5h 2, M5j 2 | 10.2 |
| C-MOD ban, prison, gag, kick, rename | 20 | 1,135 | 12 | M5c 1, M5j 7 (`PunishmentService`, `ChatBanService`) | 16.8 |
| C-QUEST quest control | 5 | 511 | 3 | M5d 2 | 7.2 |
| C-SKILLS skills and titles | 11 | 868 | 7 | M5c 1, handler-local JAXB 3 | 12.8 |
| C-SPAWNEDIT spawn write-back | 4 | 371 | 1 | `SpawnsData::saveSpawn` (`SpawnsData.cpp:212`, unscheduled) 3 | 6.0 |
| C-SYS Configure, Ai, Debug, Reload, Send, Stat, ... | 11 | 1,403 | 5 | tooling 3, M5b-2 1, M5c 1, M5d 1 | 32.0 |
| C-TELE GM movement (GoTo's 215 location rows) | 13 | 1,236 | 6 | M5f 6, M5g 1 | 19.8 |
| C-WORLD siege, raids, rifts, instances, headhunting | 14 | 1,326 | 2 | M5i 6, M5g 2, others 4 (M5f, M5b-3, M5j, M5c 1 each) | 23.5 |
| **Total** | **152** | **11,823** | **85 (4,703 lines)** | M5b-3 8, M5c 9, M5d 3, M5e 3, M5f 10, M5g 5, M5h 2, M5i 7, M5j 10, tooling 9, M5b-2 1 | **197.5** |

- All 152 import P5-14, because the chat framework is unported: 13 bodies (`ChatCommand.cpp:67-131`, `Admin/Console/PlayerCommand.cpp`)
  and the entry packets `CM_CHAT_MESSAGE_PUBLIC` and `AbstractGmCommandPacket`, about 250 Java lines. `ChatProcessor::handleChatCommand` is
  already ported (`ChatProcessor.cpp:110-123`). m5j-plan proposes the same move: the framework in its stage 0 (J1, `m5j-plan.md:104`,
  `:193`), run right after M5b-2 (D1, `:578`), a 40-command minimal set (§5.2, `:520`), 41 commands riding with their milestones (§5.3,
  `:544`) and 71 left for its later stages (§5.4, `:564`). The "gate" columns above are S4's, measured against m5j's rev 0 A-rows.
- Only the shells (class, marker, constructor strings, `info()`) and GoTo's rows are mechanical, about 7% of the lines (§7.4).
- System commands keep the plan's replacements (`handlers-and-porting-plan.md:299-311`). `Configure` needs
  `ConfigurableProcessor::describe` and `Debug` needs `NioServer::snapshotConnections`; both are absent. `Reload` stays stubbed (D3).

---

## 7. Generator proposals

### 7.1 G1: the restricted quest transliterator (recommended)

**Scope.** The quest vocabulary (about 80 names: `AbstractQuestHandler` helpers, `QuestState`/`QuestEnv`/`QuestVars`,
`QuestService` start/finish/collect, `qe.register*`/`addOn*`, `DialogAction`/`QuestStatus`/`HandlerResult`) plus a table of about 25
ported non-quest APIs, with their receiver kind, for tier B.

**Extraction.** Parse each Java file with `tools/gen/javasrc.py`. The S2 prototype is `s2/qparse.py`, the S3 prototype
`s3/steptable.py`. Build one record per file:

- `questId` (from `super(N)` or a constant), the class, and the directory, which gives the namespace and the Q chunk;
- constants: `[name, type, value]`;
- `register`: the ordered calls (`registerQuestNpc(npc)->addOnQuestStart|addOnTalkEvent|addOnKillEvent`, `registerQuestItem`,
  `registerOn*`, for-each over a literal `int[]`);
- the preamble: target mode (`env.getTargetId()` or `instanceof Npc → getNpcId()`, which differ for items and players,
  `QuestEnv.java:94-96`), dialog var and var aliases;
- one statement tree per hook: if/else, switch with case labels and break or fall-through, effects, `return expr`.

Anything outside the vocabulary makes the file tier B (a known API or idiom rule applies) or tier C (the file is refused and hand-ported).

**Emitter mappings** (S2, S3):

| Java | C++ |
|---|---|
| member access | receiver table: `Ptr` → `->`, reference → `.` (`QuestEngine::registerQuestNpc` returns `Ptr<QuestNpc>`; `QuestEnv::getPlayer` returns a `Ptr`; `Player::getInventory` returns `Storage&`) |
| `null`, `QuestStatus.X`, `register()` | `nullptr`, `QuestStatus::X`, `register_() override` |
| `int[]` local, `int...` varargs | `static constexpr std::array` or an inline braced list for the span/initializer_list overloads (`AbstractQuestHandler.cpp:208,216,342`) |
| `new QuestEnv(null, player, questId)` | `*QuestEnv::create(nullptr, player, questId)` (`QuestEnv.h:43`) |
| `WorldMapType.X.getId()`, `getStartingClass` | the enum companions: free functions (`WorldMapTypeInfo.h:63`, `PlayerClassInfo.h:92-93`) |
| `instanceof Npc` | `runtime::as<Npc>` |
| `new SM_DIALOG_WINDOW(...)` | pass by value (`TalkEventHandler.cpp:42-50`) |
| `ZoneName.get(...) ==`, `.equals` | pointer identity |
| `(float) 262.9` | `static_cast<float>(262.9)`, never `262.9f` (keeps double-then-float rounding) |
| declarations inside case bodies | wrapped in braces |
| `return switch (x) { case A -> e; }` | a switch with returns |
| case bodies without break | emitted verbatim, never normalised (`theobomos/_3102TheDisappearingStatue.java:71-84`) |
| the marker | `AION_QUEST_HANDLER(Class, id)` with an int literal (`HandlerRegistry.h:317-325`) |

**Coverage (predicted from parsing, nothing compiled).** Rev 2 derives it from one classifier: S2's `classify.analyse_file`, which the
review ran over all 1,035 quests (`review/tierall.json`; ST = tier A, TL = idiom rules only, TLAPI = non-quest API calls, HAND = tier C),
with S2's, S3's and S4's own shard judgments for tier B.

| Shard | Tier A (classifier) | Tier B | Refused by G1 | Source of the tier-B count |
|---|---|---|---|---|
| S1 | 222 | 116 (TL 40, TLAPI 76) | 27 (closures; 17 go to G2's N-way mode) | classifier |
| S2 | 217 | 50 (classifier 52) | 10 | S2 |
| S3 | 221 (S3's own rule table: 236) | 25 (classifier 36) | 18 | S3 (246 − 221) |
| S4 | 0 | 63-78, once about 10 verbs exist (teleport, spawn, despawn, timer start/end, follow start/end, flight, item-count, kinah, cube-full, inside-zone) | 51-66 | S4 |
| **All** | **660 (64%)** | **254-269** | **106-121** | |

**G1 emits 660 files with the core alone and 914-929 (88-90%) with every tier-B rule**; G2 clones 17 of the refused S1 files, which
leaves about 90-105 quests for hand-porting. Tier A plus the idiom-only files (TL, 50) is 710 (69%), so **the 70% bar needs the ~25-row
API table as well as the idiom rules**. Rev 1's "800-915" could not be rebuilt from its parts (it counted S1's 262 "engine-only" files
as tier A; the classifier says 222).

**Cost (estimated).** Core: 28 h (S2) to 45 h (S3; range 40-60). Tier-B API/idiom table and twin tool: 18 h. G2's N-way mode (S1's slot
cloner): 14 h. S4 verbs: 15 h. The golden oracle (§7.6) adds about 10 h to productionise. The one-function `HandlerResult` companion
(m5d H-06; R8) is needed before the 71 files that call `HandlerResult.fromBoolean` compile, 38 of them tier A (S2 23, S3 10, S1 5).
Order: prototype on the 20 quests the plan asks for (`handlers-and-porting-plan.md:558`). S2 proposes tier-A files from S2-F1 and
S2-F3 plus `_1000Prologue`. Apply the 70% bar, then generate tier A, then tier B per Q chunk.

**Commit policy.** Generated files are committed (`handlers-and-porting-plan.md:338`), and each lands in its owning Q chunk. The
integrator runs the generator; lanes own the hand-finished files.

### 7.2 G2: the twin tool (mirror pairs, scripted twins and N-way clone groups)

- **Row:** {groupId, portedFile, twinFile, literalMap [{from, to, tokenPositions?}], identMap [`Race.ELYOS`→`Race.ASMODIANS`],
  classRename, questId}. The map is extracted by aligning the two Java token streams (`s2/pairs.py`). **Substitute by slot position,
  not by value**: 6 S1 files and S2's `0126`/`0143` need positional maps. **N-way mode** (S1's slot cloner, kept in rev 2): for an exact
  group of N files, one representative is ported and each other member gets a row against it.
- **Refusal:** the tool refuses a pair when the alignment shows any non-literal edit. That catches the 14 divergent S2 pairs and the
  quirky copies of S6-F-A01 (`PazuzuAI.java:54`, different delays).
- **Use:** only for twins the transliterator cannot emit: the 16 scripted S2 twin pairs, the S3 P1/P4 exact pairs, **the S1 exact groups
  G1 refuses** (the 27 closure files of §4.1: 17 clones of 3 representatives), the 4 id-only splinter AI copies and the splinter instance.
  For tier A/B pairs and groups the generator simply emits every file.
- **Literal order:** substituting by Java token position works on the hand-ported C++ only if that port keeps the Java order of the
  literals. A file G2 will clone must therefore be ported with its literals in Java order, or with each literal tagged by an anchor
  comment (`/*L17*/`), and the twin's parity check compares the **ordered** literal sequence as well as the multiset (§7.6, item 4). A
  multiset check alone cannot see two literals swapped between positions, and the AI and instance twins have no path oracle to catch it.
- **Invariant:** each twin stays its own `.cpp` with its own marker. For cross-chunk twins, B is derived from A's merged `.cpp` when B's
  wave runs.

### 7.3 G3: statement-transliteration assist for AI and instance code

Not a family generator. It is a statement-level pass on `javasrc.py` bodies that emits the data-like lines: `spawn(npcId, x, y, z, (byte) h)`,
`sp(...)`, `sendMsg(STR_...)`, `setDoorState`, `teleportTo(...)`, `deleteAliveNpcs(...)` and case labels, keeping npc ids literal for the
`QuestSpawnAnalyzer` parity (`handlers-and-porting-plan.md:291-297`). About 10 h to build (S5). S5 credits it with about 20 h.

**What it saves in S6 is a range, not 92 h.** S6 costs a file at 1.25 × (0.3 + code lines/50 + surcharges), so removing its 4,620
qualifying lines "saves" up to ~115 h however cheap those lines are, and S6 booked 92 h of it. But literal spawn and message lines are the
cheapest lines to port by hand, the generated lines still need review, and the 1,600 case/break lines sit inside switches that are
written by hand anyway. Rev 2 credits only the other **3,020 lines** (2,041 spawns, 463 message sends, 94 `setDoorState`, 193 skill
calls, 229 deletes): at most **60 h** (S6's own 80% of their 75 h model cost), probably about half that, so **30-60 h**. S6's route
becomes 573-603 h. The "30-40% of instance work" of a syntax-directed version is S6's unverified guess and is not in any route.

### 7.4 G4: command shells and the GoTo table

It emits the class, marker, constructor strings and `info()` for 152 commands, plus GoTo's **215** `addLocation` rows
{identifiers[1..4], map, x, y, z, h?} (`admincommands/GoTo.java:127-409`; each row carries 1 to 4 lookup names, and the two overloads at
`:412` and `:416` differ in the heading). About 6.4 h; it saves about 7% of the command lines. The `register()` bodies and command
constructors (1,556 + 830 lines) are mechanical either way.

### 7.5 Families where a row list works but a generator does not pay

| Family | Rows | Recommendation |
|---|---|---|
| S5-F05 shield generators | 4 rows over `ShieldGeneratorAI` {gate message, shout message} | hand-port the base, copy the rows. Keep the quirks: East, South and West all shout `N_WAVE_01_BEGIN` (`ShieldGenerator{East,South,West}AI.java:45`; only North shouts its own wave, 04), and `getGateMsg` (`:34`) has no caller |
| S6 F-I01 upper-abyss keeps | 9 rows {mapId, boss/chest/door/keymaster/timer ids, 13 door kills, chest triggers, easy-mode artifact, statue rewards} | hand-port the base (3 h), copy the subclasses (~7 h in all against 14.4 h) |
| S6 F-I02 flying rings | 4 rows {mapId, ring, 9 coordinates, radius, delete ids, chest ids, **chest position and heading**} (`abyss/AsteriaInstance.java:67` spawns at 512.8, 565.35, 198, heading 60; `abyss/SulfurTreeNestInstance.java:67` at 482.87, 474.07, 163.16, heading 90) | copy by hand |
| S5 F16 Rentus pair | an id struct of about 15 npc ids | one port plus a second class. Keep two `.cpp` files unless U2 allows sharing |
| S1 F10 steel_rake stubs | 7 rows {questId} | G1 emits them trivially |

Rejected: a runtime step-table interpreter (§4.5); XML conversion of Java quests (§4.5); a shared C++ helper **base class** for the S6
skill tickers (saves under 10 h and deviates from the per-file rule). S5's data-driven helper classes are decision U2.

### 7.6 How a generated handler is tested against its Java

The plan's verification has no Java runtime (`handlers-and-porting-plan.md:608`), so every oracle is built statically from the Java source.

1. **Structural parity (`tools/parity`, per file, every handler).** Multisets of integer and string literals, `DialogAction` names,
   `STR_*` names, API call names, schedule counts and the spawn npc id set (`handlers-and-porting-plan.md:613-618`). It must be green
   before merge (`:556`). **`tools/parity` does not exist yet** (`ls tools/` shows gen, oracle, porting, xmlgen), so it belongs to the
   tooling lane.
2. **Registration trace (quests, runnable today).** The ordered (npc or item id, event kind) list that `register()` produces, compared
   with the Java statement order. `QuestEngine::registerQuestNpc` and `QuestNpc::addOn*` are already ported (`QuestEngine.cpp:710-717`,
   `QuestNpc.cpp:25-44`), so this runs before M5d.
3. **Golden trace (quests).** The oracle productionises the S3 path extractor. For every return leaf of every hook it records:
   - a case: hook, target npc, quest status, var slots, dialog action, inventory counts named by guards, other-quest states;
   - the expected effects in order: dialog page sent, var and status writes, item gives and removes, movie, return value.

   Guards are equalities, ranges and their negations, so a satisfying assignment is picked directly. The output is
   `tools/oracle/expected/quest/<id>.json`. After M5d, a harness test drives each case through the real engine on the
   `GameServerHarness` (ManualClock, seeded Rnd) and compares packets (`SM_DIALOG_WINDOW` page, `SM_QUEST_ACTION`), the `QuestState`
   afterwards and the inventory delta. Because the oracle is written from Java, not from the port, it satisfies the roadmap's mutation
   standard: a flipped page id or var must fail a case. Before M5d, a test-only target could link a recording double of
   `AbstractQuestHandler`/`QuestState` in place of the unported bodies and compare call traces. That is optional; the helpers are
   non-virtual, so it has to be a link seam.
4. **Twins.** The oracle and the parity check always run on the twin's **own** Java file, never on A's. For every G2 output, parity also
   compares the **ordered** sequence of integer and string literals of the C++ file with the Java file's (anchors, where used, fix the
   pairing), because a multiset check cannot see two literals swapped between positions. For the AI and instance twins (the splinter
   copies and the splinter instance) this is the only check of the substitution, since they have no path oracle.
5. **AI and instances.** There is no path oracle, because the control flow is time-driven. Use parity (including schedule counts and
   spawn ids), the AI smoke (every AI name through SPAWNED → CREATURE_SEE → ATTACKED → DIED → DESPAWNED with 10 min of virtual time,
   ASan; `handlers-and-porting-plan.md:646`) and the instance smoke (each of the 73 maps: create, enter, 30 min, destroy; `:651`), plus
   scenario tests for bosses on a manual clock.
6. **Commands.** The command smoke: every command with `help` and with no parameters (`handlers-and-porting-plan.md:652`).

---

## 8. Phase-5 prerequisites per family

### 8.1 What becomes portable after which milestone (estimated)

"Portable" means that the bodies its smoke test reaches exist (`handlers-and-porting-plan.md:339`). "Playable" can come later, for
example an instance AI before its instance. AI counts are S5's call-based gates plus S6's import gates as S6 corrected them. They assume
the missing root bases are ported first (R1, §8.3).

| After | Quests (effective gate) | AI (461 incl. 43 roots) | Instances (78) | Commands (152) / zones | Notes |
|---|---|---|---|---|---|
| **now** (phase 4 + framework) | generate all tier A/B; compile them, except the 71 `fromBoolean` quests until H-06's header exists (R8); registration traces; **no linking into gate builds** | **~125 files / ~185 h** (S5 88, S6 ~37) | – | – | the tools of §7 have no runtime dependency |
| **M5b-2** | – | **~181 / ~410 h** (S5 84, S6 ~97); about 74 of them finish only after M5e's effect lanes or M5j's J9 (§5.3) | – | `//stat` | `SkillEngine.cpp`, `Skill.cpp` and `EffectController.cpp` have 0 `AION_UNPORTED` sites today |
| chat framework (J1) | – | – | – | **85** | recommended early (R6; m5j D1 agrees) |
| **M5b-3** | reward items and `CM_USE_ITEM` (prerequisites, not a gate) | 1 | – | 8 | effect classes E-02: 5 of the 63 |
| **M5c** | crafting-exam checks (F03) | ~18 / ~25 h (`DialogService`, dialog packets) | – | 9 | |
| **M5d** (after the D3 join) | **~761**: S1 ~246, S2 ~193, S3 ~221, S4 101 | ~31 / ~52 h (`ActionItemNpcAI`-based use-item npcs) | – | 3; zone `_1012` | only if E-05, E-07 and `CM_PLAY_MOVIE_END` are required (R4) |
| **M5e** | Ascension half-ready (`ClassChangeService`) | effect classes E-01/E-02, M-03: 24 of the 63; 30 of the 76 effect-touching S5 files complete | – | 3 | |
| **M5f** | **~171** instance-reach quests (S1 60, including F06's 12 dispatches behind Ascension; S2 ~56; S3 37; S4 18) | ~32 / ~65 h (portals, resurrect); the kisk-, chest- and shifter-based ones only if R1 takes their roots | **59 / ~356 h**; the 6 scored ones (crucible 3, Dark Poeta, Eternal Bastion, the Emperor's Vault) finish only with R7 | 10; zones 2 | the S6 instance AIs become playable |
| **M5g** | ~18 (F07 mentor) | ~10 / ~20 h | 18 / ~94 h (the 17 matchmade dredgion, battlefield and arena handlers, Taloc's score) **only if R11** brings `AutoGroupService` to P6-G, else M5j stage 4 | 5 | m5j A-G2: M5g does not take `AutoGroupService` |
| **M5h** | ~9 (housing) | ~7 / ~14 h | 1 (Stonespear, 30.75 h; also R7) | 2 | |
| **M5i** | ~19 (F12 events 15, S3 events 4) | ~31 / ~67 h (siege, base, Panesterra) | – | 7 | m5i declines the PvP half (its D16) |
| **M5j** | ~54: S4 10 (escorts on `FollowingNpcAI`, `_11076`'s windstream); F05 26 and ~8 PvP kills (S-12, stage 1); ~10 dredgion and battlefield quests (X-01, stage 4) | ~25 / ~44 h (the three M5j roots, `NpcShoutsService`), plus the kisk, chest, shifter and one-damage roots with their 14 subclasses (stages 2-3) unless R1 | the matchmade handlers (X-01) and the score classes and writers (L-03) unless R7 / R11 | 10 | R1, R7, R11 and R12 move most of this earlier |
| **M5j J9** (stage 3) | – | the 23 J9 effect classes: 46 S5 files, 19 of the 28 bosses (40 / 15 with every W/O item, 32 / 13 with M5f's X-05b) | – | – | R3 |
| unscheduled | – | – | – | tooling 9 | |

The quest rows are approximate and sum to about 1,030. Rev 2 moved 37 S3 quests out of M5d by the same directory rule rev 1 applied to
S1 and S2 (beshmundir 14, tiamat_stronghold 6, udas_temple 5, rentus_base 4, abyssal_splinter 3, esoterrace 2, nightmare_circus 1 to
M5f; terath_dredgion 2 to matchmaking), moved F06 from M5e to M5f, and moved the dredgion and battlefield directories of S2
(chantra_dredgion 6, iron_wall_warfront 2, which include S2's "dredgion reward 4") and the PvP-half quests to M5j (§8.4).

### 8.2 Where the import gate is wrong

| Direction | Files | Evidence |
|---|---|---|
| too strict | 18 S4 quests gated M5f by import call only the `worldId` `teleportTo` overloads, which are ported | `TeleportService.cpp:236-282`; `s4_result.json` (import M5f, effective M5d: 18, all Q-TELE) |
| too strict | 18 S5 AIs import `skillengine.model.Effect` only for the `modifyDamage` signature | S5 `gates.json` |
| too strict | S2 `quest-0068` and S3 `quest-0148` teleport overloads | `TeleportService.cpp:241→274`, `:246-282` |
| too strict | 32 S5 and 53 S6 "M5j" AIs are M5j only because of a missing root base. By what they call, the 32 S5 files are 7 phase 4, 5 M5b-2, 1 M5b-3, 6 M5c, 1 M5d, 5 M5f, 4 M5g, 1 M5h, 1 M5i and only 1 M5j | S5 notes; m5j-plan re-homing |
| too lax | 11 S5 AIs "done" by import cast through `AIActions.useSkill` or an inherited helper (M5b-2); 23 S6 files likewise (they call `useSkill`, `SkillEngine` or `queueSkill`: F-A02 10, F-A03 7, F-A01 3, F-A07 2, F-A06 1) | S5 notes; `s6/families.json` re-read for rev 2 |
| too lax | 26 F05 rank-kill quests: no ranked kill is dispatched before the PvP half of `doReward` | `PvpService.cpp:61-85`; `m5j-plan.md:85` (A-I2, triggered: M5j S-12) |
| too lax | 15 F12 event quests: nothing starts an event quest before `Event` (P5-12b) | `Event.cpp:46` |
| too lax | ~48 S1, ~64 S2 and 37 S3 quests are playable only inside instances (S1: Beshmundir 24, Dark Poeta 8, Tiamat 8, Abyssal Splinter 5, ...; S3: beshmundir 14, tiamat_stronghold 6, udas_temple 5, rentus_base 4, abyssal_splinter 3, esoterrace 2, terath_dredgion 2, nightmare_circus 1). The dredgion and battlefield ones (S2 8, S3 2) also need matchmaking | directory names (inferred); rev 1 applied the rule to S1 and S2 only |
| too lax | 12 F06 dispatch quests start only after 1007 / 2009, which follow Ascension 1006 / 2008 (instance and instance teleport: M5f) | `quest_data.xml:48056`, `:8035`; `_1006Ascension.java:98-99`; `InstanceService.cpp:81-93`, `TeleportService.cpp:260-271` |
| too lax | 35 S6 files (dredgion, battlefields, arenas and their AIs) are entered only through `AutoGroupService`, which no milestone before M5j stage 4 takes | `AutoGroupType.java:13-56`; `m5j-plan.md:80` (A-G2), `:685` (X-01) |
| too lax | 45 S2 and 32 S4 files override `onItemUseEvent`, whose only source is `CM_USE_ITEM` (no C++ file) | `m5b3-plan.md:122` |
| too lax | 16 S4 quests need `CM_PLAY_MOVIE_END`, 14 need quest timers, 8 need the follow task; 8 escorts also need `FollowingNpcAI` | `m5d-plan.md:544, 583, 615` |

### 8.3 Requests to the phase-5 plans

| # | Request | Unblocks | Owner today | Recommendation |
|---|---|---|---|---|
| R1 | Port the missing root AIs early: `AggressiveNoLootNpcAI` (24 lines, phase 4 only), `SummonerAI` (146; casts through `AIActions.useSkill`, `SummonerAI.java:102`) and `UseSkillAndDieAI` (66; `SkillEngine.getSkill(...).useSkill()`, `UseSkillAndDieAI.java:47-49`), which need M5b-2's cast path; the 9 policy roots; and the roots m5j rev 1 inherited: `ChestAI`, `ShifterAI`, `HiddenTeleportNpcAI`, `OneDmgAI`/`OneDmgNoActionAI` | ~39 files on the three M5j roots (S5 12, S6 27); 12 S5 files on the policy roots; 14 subclasses of the inherited roots | M5j (§2.5, `m5j-plan.md:345-352`; A-F3 `:77`, A-H2 `:83`) | a small P5-05 lane now for the phase-4 roots, after M5b-2 for the casting ones, or with M5f's instance lane |
| R2 | `Npc::queueSkill` ×3 (`Npc.cpp:200-210`) | 13 S5 + 16 S6 bosses (42 call sites in S6) | M5j L-03 only (`m5j-plan.md:688`) | move into M5f's instance lane |
| R3 | **The 23 effect classes that only M5j's J9 owns** (951 Java lines, 54 `AION_UNPORTED` sites; §5.3's table): 14 npc- or item-only, 9 player classes past M5e's level-20 closure. M5b-3 E-02 and M5e E-01/E-02/M-03 already take 29 of the 63 as R; 6 need no port | 46 S5 files that still wait after M5e, including 19 of the 28 bosses; S6 bosses likewise | M5j J9, stage 3 (`m5j-plan.md:112`, `:252-257`); M5f X-05b (O) would take 7 | take X-05b as R in M5f and put the other 16 in a P5-03/P5-04 lane before the P6-I lanes; take M5b-3 E-03 and M5e T-02 as R |
| R4 | Make M5d E-05 (timers), E-07 (`task/**` follow) and D-05 `CM_PLAY_MOVIE_END` required | 24 of the 101 M5d-gated S4 quests, S3 escorts and timers, the prologues 1000/2000 | W / P / W (`m5d-plan.md:544, 615, 583`); m5j A-D4 (`m5j-plan.md:68`) assumes the same | R; E-05 is `QuestService.java:811-847`, cheap |
| R5 | The five one-line `TeleportService` delegations (`TeleportService.cpp:251-271`) | 4 quests and `PvPAreaZone` move to M5d; several AIs | M5f A-F1 (`m5j-plan.md:75`) | pull forward, trivially |
| R6 | The chat framework core of J1 (about 250 Java lines; m5j's J1 with GM login is ~1,100, `m5j-plan.md:104`) | 85 commands on today's ports; GM tools in every later real-client session | M5j stage 0, which m5j's D1 runs right after M5b-2 (`m5j-plan.md:578`) | agreed: before or with M5b-3 |
| R7 | `GeneralInstanceHandler` helpers, `InstanceEngine`, and the instance score/reward/position classes (J13) with the score writers | all 78 instance handlers; the 24 that use score or reward classes (17 matchmade, crucible 3, Dark Poeta, Eternal Bastion, Stonespear, the Emperor's Vault) | handlers and engine: M5f (A-F1); **score classes: M5j stage 4** (L-03), because m5f O-03 sends them to "M5g / M5i" and neither takes them (A-F4, triggered, `m5j-plan.md:78`) | move the score classes and writers into M5f |
| R8 | `HandlerResult::fromBoolean` (H-06), a one-function companion header | **71 quests** (38 tier A: S2 23, S3 10, S1 5); rev 1 said "14 S4 quests, S2-F2, S3-F4" | M5d, R (`m5d-plan.md:556`, a new file, `:685`) | pull the header into P6-T now (or into M5d's first stage), so that tier A compiles |
| R9 | `WebRewardService.MaxLevelReward.isPendingAscension` (a set lookup) | Ascension 1006/2008 | J12, D12 (`m5j-plan.md:267-269`, `:589`) | port with M5e or M5f, separately from J12 |
| R10 | `NpcAI::getSpawnTemplate` covariance (`NpcAI.h:44`) | 5 AIs | – | header request: a typed helper per subclass |
| R11 | Instance matchmaking: `AutoGroupService` + `CM_AUTO_GROUP` | the 17 matchmade instance handlers, their ~35 S6 files, ~10 dredgion and battlefield quests | M5j stage 4 X-01 (`m5j-plan.md:80`, `:685`); m5g O-01 defers it "with the phase-6 handlers of the matched instances" | port it in P6-G together with those handlers, as m5g O-01 suggests, after M5g |
| R12 | The PvP half of `PvpService::doReward` | F05's 26 rank-kill quests, ~8 PvP-kill quests (S2, S3's `quest-0162`) | M5j stage 1 S-12 (A-I2, triggered; m5i D16 declines it) | M5i, where rifts first let a player kill a player, or M5j stage 1 early |

### 8.4 What m5j-plan rev 1 changed (edited during the review)

m5j-plan was revised while this document was reviewed. Its rev 1 marks several sibling deferrals "**triggered**": the sibling draft says it
will not deliver, and the work is M5j's (its D15). Rev 2 of this document follows it:

| Item | Rev 1 of this document (m5j rev 0) | Now (m5j rev 1) | Effect here |
|---|---|---|---|
| `CM_WINDSTREAM` | M5f (A-F1) | M5j stage 1, S-06 (A-F2; m5f O-02), `m5j-plan.md:76` | `_11076` back to M5j (S4's own gate) |
| kisks; `ChestAI`, `ShifterAI`, `HiddenTeleportNpcAI` | M5f | M5j stages 2 and 3 (A-F2, A-F3), `:76-77` | 12 AI files (4 roots, `InvisiblekiskAI`, 4 chest and 3 shifter subclasses) leave M5f unless R1 |
| `OneDmgAI`, `OneDmgNoActionAI` | M5h | M5j stage 3 (A-H2), `:83` | 9 AI files (2 roots, 7 subclasses) leave M5h unless R1 |
| instance score classes, `PvPArenaService`, `PeriodicInstanceManager` | M5f (A-F4) | M5j stage 4, L-03 / X-01 (A-F4; m5f O-03), `:78` | R7 now asks for a move, not a "keep" |
| `AutoGroupService` (matchmaking) | M5g | M5j stage 4, X-01 (A-G2; m5g O-01), `:80`, `:685` | the M5g instance row is conditional; new R11 |
| PvP half of `doReward` | M5i (A-I2) | M5j stage 1, S-12 (A-I2; m5i D16), `:85`, `:647` | 34 quests move to M5j; new R12 |
| M5e's effect classes (A-E4) | "every skill its trainers teach" | the 32 Daeva classes (E-01/E-02) and optional stigma (T-02), `:73` | J9 keeps 23 of the 63 (§5.3) |
| `CM_PLAY_MOVIE_END` (A-D4) | – | M5d, W (`:68`) | R4 unchanged |
| `ResurrectAI`, `PortalAI`, `PortalDialogAI` | M5f | M5f (A-F1), `:75` | none |

---

## 9. Proposed phase-6 lane plan

### 9.1 Constraints

- `phase5-roadmap.md` says phase 6 comes **after** phase 5, and that the phase-5 milestones run without stopping. Interleaving is
  therefore decision U7. The proposal: phase-6 lanes take at most 1-2 of the 6 lane slots of a phase-5 wave, or run between milestones.
  The integrator should see no more than about 10 concurrent phase-6 branches (`handlers-and-porting-plan.md:586`).
- Quests run in three waves of 6/6/2 Q chunks, A1 and I1-I6 in two waves after P5-13, C1/C2 last (`handlers-and-porting-plan.md:805`).
  This plan keeps the quest waves and moves AI and commands earlier.
- Chunk ownership is per file. An AI file owned by I-chunk *n* may land before its instance handler, because the AI smoke does not need
  the instance.

### 9.2 Lanes (estimated)

| Lane | Starts after | Scope | Files | Hand h | Route |
|---|---|---|---|---|---|
| **P6-T** tooling | now | G1 core + tier-B table + twin tool with its N-way mode + S4 verbs; the `HandlerResult` companion header if M5d has not landed it (R8); the golden quest oracle; `tools/parity` with the ordered-literal check; G3 assist; G4 command shells; the 20-quest prototype and its go/no-go | – | ~118 + `tools/parity` | build |
| P5-05 roots (request R1) | now for the phase-4 roots; the casting roots after M5b-2 | the 40 missing roots (S5 F03/F04, the 9 policy roots in F02, a few in F13), including the chest, shifter, kisk and one-damage roots m5j rev 1 inherited; a phase-5 chunk, counted inside the AI lanes below | 40 | ~70 | hand |
| **P6-A0** | now | AI files calling only phase-4 or framework code (A1 + the `ai/instance` parts of I1-I6) | ~125 | ~185 | hand + G3 |
| **P6-A1** | M5b-2 | AI files casting through `SkillEngine`/`AIActions.useSkill`: minions, skill tickers, auras, bosses | ~181 | ~410 | hand; ~74 finished only after M5e's effect lanes or J9 (R3) |
| **P6-Q1..Q3** | M5d D3 join (generation earlier) | all 1,035 quests in waves of 6/6/2 Q chunks: generated tiers first, then the chunk's hand quests; Poeta/Ishalgen first (§9.3). **The only lane that ports quests**; the lanes below only playtest theirs | 1,035 | 902 hand / ~493 + tools | G1 + G2 + hand |
| **P6-C** | J1 (R6) | 85 framework-only commands, then 41 with their milestones (m5j §5.3), C2 system commands last | 152 | ~198 (−14 with G4) | hand + G4 |
| P6-D | M5b-3 / M5c / M5d | dialog and use-item AIs (mostly S5 F10, F12; S6 F-A04, F-A05) after `ActionItemNpcAI` | ~50 | ~80 | hand |
| **P6-I1..I6** | M5f | instance handlers of each I slice (S5 F16 without Stonespear, S6 F-I01..F-I05, F-I09), room AIs, portals (A1 F11); the playtests of instance-reach quests. The 6 scored handlers need R7 | 59 handlers + ~32 AIs | ~420 | hand + G3 |
| P6-G | M5g, and `AutoGroupService` (R11) | dredgion, battlefields, arenas (F-I06..F-I08), Taloc score; ~10 group-dependent AIs; `AutoGroupService` itself if R11 is taken; playtests of the F07 mentor quests (ported in P6-Q) | 28 | ~114 (+ `AutoGroupService` under R11) | hand |
| P6-H/I | M5h / M5i | Stonespear; siege, base, Panesterra AIs (S5 F13); playtests of the housing and siege quests (ported in P6-Q) | ~39 | ~112 | hand |
| P6-J | M5j / D12 | AIs on the M5j roots if R1 is refused, `NaiaAI`, `FollowingNpcAI` escorts, custom content F-A11 (if D12 says port); the PvP-kill and matchmade quests become playable here unless R11 / R12 | ~25 | ~44 | hand |

Rev 1 listed "18 quests" in P6-G's file count and "housing, siege and PvP-kill quests" in P6-H/I's scope as well as in P6-Q. The hours
were not double-counted (P6-G's ~114 h and P6-H/I's ~112 h are AI and instance hours only), but the file counts were; rev 2 counts
every quest in P6-Q only.

### 9.3 Order inside the quest lanes

1. **The 30 Poeta/Ishalgen Java quests** that m5d D9 leaves missing: S3 18 (the Poeta mission chain 1001-1005, `_2132`/`_1205`, ...),
   S4 6 (`_1002`, `_1114`, `_2002`, `_2004`, `_2007`, `_2136`), S1 3 (`_2125`, `_2135`, `_1100`) and S2 3 (`_1000`/`_2000` Prologue,
   `_2100`). Land the prologues and the F4 orders with the join; `_1002` and `_2002` need M5f.
2. Then each Q chunk: the generated tier A/B files, the twins, the hand quests. S3 first where it matters, because 78 XML-template quests
   name an S3 quest as a precondition, but only S3's ~221 M5d-reachable files: its 37 files in instance directories (beshmundir 14,
   tiamat_stronghold 6, udas_temple 5, ...) cannot be played before M5f, or before matchmaking for the 2 in terath_dredgion.
3. Ascension 1006/2008 as soon as M5e and M5f are in; it is the critical path past level 9. 1007/2009 and the 12 F06 dispatches follow it.

---

## 10. Effort comparison (estimated)

### 10.1 Per shard, as reported

| Shard | Files | Lines | Hand-port all (h) | Generated route (h) | One-time tools in the route | Source |
|---|---|---|---|---|---|---|
| S1 | 365 | 28,576 | 210.5 | 109 | 14 (slot cloner = G2's N-way mode) | S1 |
| S2 | 277 | 24,653 | 163 | 93 (92.5) | 46 (28 core + 18) | S2 |
| S3 | 264 | 24,875 | 219 | 159 | 45 (core) | S3 |
| S4 quests | 129 | 17,435 | 309.75 | ~253 | 15 (verbs) | derived here: the 63 named fits (119.5 h) at 40% review cost |
| S4 commands + zones | 155 | 11,958 | 201 | ~194 | 6.4 (shells) | derived here: 7% of the lines |
| S5 | 275 | 24,462 | 708.75 | ~675 (~630 with U2) | ~10 (assist) | S5, without its helper classes (U2 rejected) |
| S6 | 264 | 26,007 | ~656 | ~573-603 | shares G3 | derived here: −23 h tables, −30-60 h transliteration (§7.3; rev 1: −92 h) |
| **Total** | **1,729** | **157,966** | **~2,468** | ~2,056-2,086 (tools counted twice) | | |

### 10.2 De-duplicated

The baseline is the configuration this document recommends: U2 rejected (§12), the slot cloner kept as G2's N-way mode (§4.1), the G3
credit in S6 as a range (§7.3).

- The transliterator core is counted once, at 45 h: subtract S2's 28 h. The slot cloner is **not** subtracted, because G1 refuses the 27
  S1 files it clones. **Generated route ≈ 2,028-2,058 h.**
- If the core costs 60 h: +15 h. **Range ≈ 2,030-2,075 h against ~2,468 h by hand**, a saving of about **395-440 h (16-18%)**. Taking U2
  (S5's data-driven helper classes) would save 45 h more.
- **Quests:** 902 h by hand → **~585 h** (S1 94.9 + S2 46.5 + S3 114 + S4 238 + tools 92 [45 core + 18 + 15 verbs + 14 N-way mode];
  −35%; ~600 h, −33%, with a 60 h core). **AI + instances:** 1,365 h → **1,248-1,278 h (−6 to −9%)**; −45 h more with U2.
  **Commands + zones:** 201 → ~194 h.
- Rev 1's figures were 1,940-2,000 h, a 470-530 h saving, quests −37% and AI + instances −11-14%. They assumed U2, subtracted the
  cloner that G1 cannot replace, and took S6's 92 h at face value (§14).

### 10.3 Why the totals are soft

Each shard used its own model, and none was calibrated against measured porting rates:

| Shard | Model |
|---|---|
| S1 | 0.5 / 0.7 / 1.0 h per engine-only / light / scripted file including self-review; 0.05 h per clone; adversarial review adds ~30% to both routes |
| S2 | 0.4-0.5 h per engine-only file, 1.0 h scripted, 2-3 h for instance or thread flows; 0.075 h per generated file |
| S3 | agent-lane hours including review |
| S4 | ~60 Java lines per agent-hour plus idiom surcharges, in line with M5d's ~9,320 lines in 15-20 agent-days (`m5d-plan.md:867`) |
| S5 | engineer-hours by line bands with multipliers for timers, HP phases, observers and score code, **excluding review** |
| S6 | 1.25 × (0.3 + code lines/50 + surcharges), about 40 lines/h. Line-linear: any removed line saves the same, which overstates what generating the cheapest lines saves (§7.3) |

The implied rates range from 35 lines/h (S5) to 151 lines/h (S2). Quests are cheaper per line, but the spread is also a model effect.
Read the ratios (quests −35%, AI + instances −6 to −9%) with more confidence than the absolute hours.

---

## 11. Parity hazards and Java bugs found

The default is a faithful port with a note in the deviation file; D6 (`README.md`) covers races, and U3 covers logic bugs. A generator
or a copy would "fix" some of these silently.

| File | Issue |
|---|---|
| `quest/beshmundir/_30348ImprovedAethercannon.java:41` | requires 100100716 (the Improved Mace, as `_30343ImprovedMace.java:41`); `quest_data.xml:66246` collects 101900655 |
| `quest/the_circle/_47106TurningUpTheAmplifiers.java:31` | registers kills of 217173 but counts and spawns 217175 (`:36`, `:62`), so the quest cannot complete |
| `quest/sanctum/_3963GrowthFlorasThirdCharm.java:69-70`, `_3964...:69-70` | `tryDecreaseKinah` already takes the kinah (`Storage.java:82-88`), then `decreaseKinah` takes it again. A F09 row with one `kinah` column would silently fix it; F09 goes through G1 only (§4.1) |
| `quest/eltnen/_1367MabangtahsFeast.java`, `quest/morheim/_24026AHandfromEachSide.java` | per-player state in fields of the singleton handler (`// java-race`) |
| `instance/abyss/UnstableSplinterpathInstance.java:62-63, :74, :83` | swapped boss ids and stale deletes of 281907/281908 |
| `ai/instance/abyssal_splinter/PazuzuAI.java:54` | builds a skill it never uses |
| `ai/instance/theShugoEmperorsVault/RuthlessJabaraki.java:18` vs `:34-39` | declares phases 96/80/60/45/40/35 but switches on 95/80/65/50/35/20; only 80 and 35 fire |
| `instance/abyss/AbstractInnerUpperAbyssInstance.java:177-178` | a second `spawnChests` dereferences null: a caught NPE in Java, a crash in C++ |
| `instance/TheobomosLabInstance.java` `removeBuff` | `getNpc(214668).getEffectController()` with no null check |
| `ai/instance/.../TahabataAltarAI.java` | the debuff switch keys on 283116/283118 while the range check uses 283253/283255, so it always uses skill 0 |
| `ai/instance/drakenspire/OrissanAI.java` | `handleDied` calls `super.handleDespawned()` |
| `instance/pvparenas/ArenaOfGloryInstance.java` | `setRewardItem1` twice in one branch; the second wins |
| `ai/instance/illuminaryObelisk/ShieldGenerator{East,South,West}AI.java:45` | all three shout `N_WAVE_01_BEGIN` (North shouts wave 04); `getGateMsg` (`:34`) has no caller. Rev 1 named only East and West |
| `quest/heiron/_3200PriceOfGoodwill.java` vs `quest/beluslan/_4200ASuspiciousCall.java:128-132` | `UNKNOWN` vs `FAILED` on the wrong step; not a bug, but a twin tool must not merge it |

---

## 12. Decisions for the user

| # | Decision | Recommendation |
|---|---|---|
| U1 | Adopt the quest transliterator (the plan's open question, `handlers-and-porting-plan.md:687`) | **yes**: prototype on 20 quests with the 70% bar, then G1 + G2 for all Q chunks. The bar needs the idiom rules and the API table in the prototype: tier A alone is 64% (§7.1) |
| U2 | S5's data-driven helper classes (one C++ class, many AI names) against the rule "one `.cpp` per Java file" (`handlers-and-porting-plan.md:550-556`) | **keep the rule**. The saving is ≤ 45 h, and parity, regscan and review all work per file. The effort baseline (§10.2) already assumes this |
| U3 | Java logic bugs (§11): faithful port plus a deviation note, or fix | faithful plus note, except crash-in-C++ cases (null derefs), which get a guard and a note |
| U4 | The 9 probably-dead AI names (521 lines) | port last; drop only if the registry report may show them missing |
| U5 | Custom content (D12): 7 S6 AIs, `CustomInstance`, the custom-instance commands | follow D12 (`m5j-plan.md:589`) |
| U6 | The 7 empty `steel_rake` stubs | port as stubs (the registry must reach 1,035) |
| U7 | Interleave phase-6 lanes with phase 5 (§9.1), against the roadmap's "phase 6 after" | interleave P6-T, P6-A0 and P5-05 roots now, the rest as their milestones land |
| U8 | Where instance matchmaking (`AutoGroupService`) is ported: M5j stage 4 (m5j X-01) or with the matched instances' handlers in P6-G (R11, as m5g O-01 suggests) | **P6-G**: the 17 matchmade handlers and ~10 quests are only playable with it, and m5g already points there |
| U9 | Which milestone takes the 23 J9-only effect classes (R3) | M5f's X-05b as R for 7, a P5-03/P5-04 lane before the P6-I lanes for the other 16 |

---

## 13. Artefacts and reproduction

The scratch directory is
`C:/Users/esfis/AppData/Local/Temp/claude/D--aion-server-cpp/74d1a1a6-c2db-49e0-adaf-01855c6eab15/scratchpad/phase6/` (session-local,
not committed):

- clustering: `cluster.py` → `clusters.json` (per cluster: members, exact groups with parameter-column counts, overrides, dependencies,
  verdict, gates; per file: tier, dependencies, gate), `shards.py` → `shards.json`, `perarea.py` → `perarea.json`;
- S1: `s1.py` / `s1_out.txt` (varying slots), `s1clone.py` / `s1clone.json` (clone coverage and conflicts), `s1fam.py` / `s1fam.json`,
  `cppstat.py`;
- S2: `s2/qparse.py`, `classify.py` → `s2_files.json` (tiers), `flatten.py` → `flat.json`, `pairs.py` / `pairs_norm.py`, `families.json`,
  `cppstatus.json`, `result.json`;
- S3: `s3/steptable.py` → `steptable.json` (the path extractor, oracle prototype), `analyze.py`, `fam.py`, `portstate.py`, `s3.json`;
- S4: `s4_calls.py`, `s4_apis.py` → `s4_apis.json` (per-file API resolution with C++ file:line), `s4_output.json`, `cppindex.json`;
- S5: `s5/families.json`, `gates.json`, `effort.json`, `skills.json`, `npcskills.json`, `fnmap.json` (C++ `Class::method` →
  ported/UNPORTED/PARTIAL), `q.py`;
- S6: `s6/families.json` (per-member lines, hours, gates), `hidden.json`, `files.tsv`, `result.json`;
- the review: `review/recluster.py`, `compare.py`, `tierall.py` → `tierall.json` (S2's classifier over all 1,035 quests),
  `playereffects.py`, `arith.py`;
- rev 2: `rev2/effowners.py` → `effowners.json` (owner, first player level and blocked S5 files per effect class), `rev2/handcount.py`
  (quests left for hand-porting under the two G1 configurations).

Nothing in the repository was built, run or modified except this file. The scripts only read the Java and C++ trees, with bytecode
writing off.

---

## 14. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 1 high, 6 medium and 7 low findings. Each was re-checked against the
sources before it was applied. The review also verified, and rev 2 keeps: the corpus and chunk counts, the clustering (an independent
re-clustering reproduced it), the hook, base-class and XML-kind counts, the C++ `AION_UNPORTED` citations, the m5d citations, the S4
gate counts and the §11 hazards.

| # | Sev. | Finding | Verdict | What changed |
|---|---|---|---|---|
| 1 | high | "No plan owns the 63 effect classes" is wrong; m5j's J9 owns them and 43 arrive with M5e (A-E4) | **accepted in part.** The ownership claim was wrong. The split was not: m5e-plan D4 (`:504`) closes M5e's effect scope at level 20, and m5j rev 1 rewords A-E4 to M5e's 32 Daeva classes (`m5j-plan.md:73`), so 9 player classes (first learned at 25-55) stay in J9. Also, 6 of the 63 are data-only subclasses of ported bases and need no port | §0 item 7, §5.3 (owner table), §8.1 (M5b-3, M5e and J9 rows), R3 (the 23 J9-only classes), U9 |
| 2 | medium | 365 files "in a row" are table-verdict clusters; the strict fit is 287 / 22.1k | accepted | §0 item 2, §2.3, §4.1 |
| 3 | medium | Without the slot cloner, no tool covers the 27 S1 closure files; tier A is 222, not 262 | accepted | cloner kept as G2's N-way mode (§4.1, §7.2); F07 marked "via G2 only"; the 27 in G1's refusal count (§7.1); the 14 h stays in the route and "S1 → ~75 h" is withdrawn (§10.2) |
| 4 | medium | S3 used import gates while S1 and S2 were corrected by directory | accepted: 37 S3 files. 35 go to M5f; the 2 in terath_dredgion also need matchmaking. The review's ~775 / ~168 becomes ~761 / ~171 once F06 (finding 8) and the dredgion and battlefield directories (§8.4) move too | §8.1, §8.2, §9.3 |
| 5 | medium | "Compile all tier A/B now" fails for the 71 quests calling `HandlerResult.fromBoolean` | accepted (38 of the 71 are tier A) | §0 item 6, §7.1, §8.1 "now" row, R8, P6-T |
| 6 | medium | The low end of the route assumes U2, which §12 rejects | accepted | §0 item 5, §5.1, §10: U2 is now an optional −45 h |
| 7 | medium | G3's 92 h comes from S6's line-linear model | accepted | §7.3 credits 30-60 h for the 3,020 non-case lines; S6's route is 573-603 h |
| 8 | low | F06 dispatches are not reachable at M5e | accepted | F06 moved to M5f (§4.1, §8.1, §8.2) |
| 9 | low | The `DIALOG_FINISH` difference belongs to S2-F1, not F01 | accepted | §4.5 |
| 10 | low | Several row schemas cannot express their members | accepted | F01, F04, F09 "via G1 only"; F-I02 gains chest position and heading; the shield quirk is 3 of 4; GoTo has 215 rows with 1-4 identifiers (§4.1, §7.4, §7.5, §11) |
| 11 | low | Arithmetic: C-WORLD, S2 review column, S4 hand column, G1 range, Q-TELE | accepted | C-WORLD "others 4"; S2 review 46.5; S4 309.75 per file (rows 309.9); the G1 range derived part by part (§7.1); Q-TELE 18 |
| 12 | low | Several "needs" entries omit the cast path; the AI fallback is not silent | accepted (23 S6 files, not 21) | F-A03, R1, §8.2, §5.3; the "now" and M5b-2 AI counts move by 2 |
| 13 | low | m5j A- rows applied selectively; lanes double-count quests | **accepted in part.** A-D4 now says `CM_PLAY_MOVIE_END` is W in M5d (`m5j-plan.md:68`), which matches R4; it is cited. The lanes double-counted files and scope, not hours: P6-G's ~114 h and P6-H/I's ~112 h were AI and instance hours only | R4, §9.2 (quests in P6-Q only) |
| 14 | low | G2's positional substitution has no oracle for AI and instance twins | accepted | §7.2 (literal order or anchors), §7.6 item 4 (ordered-literal parity) |

**Beyond the findings.** While the review ran, m5j-plan was revised to its rev 1. Re-reading it for finding 1 showed that it now leaves
to M5j several items that rev 1 of this document placed in M5f, M5g, M5h or M5i: `CM_WINDSTREAM`, the kisk, chest and shifter roots,
the one-damage roots, the instance score classes, instance matchmaking and the PvP half of the kill reward. §8.4 lists them. Rev 2
follows the new owners in §1, §4, §5, §8 and §9, and adds requests R11 (matchmaking with the P6-G lane) and R12 (the PvP half) and
decisions U8 and U9.

**Headline numbers, rev 1 → rev 2.** Strict-table fit: 365 files / 28.6k lines → **287 / 22.1k (66 skeletons)**. G1 coverage: 800-915
→ **660 with the core, 914-929 with every rule, + 17 by G2**. Hand-ported quests: 120-230 → **about 90-105**. Route: 1,940-2,000 h →
**2,030-2,075 h** against ~2,468 h by hand; saving 470-530 h (≈20%) → **395-440 h (16-18%)**. Quests −37% → **−35%**; AI and
instances −11-14% → **−6 to −9%**. M5d-reachable quests ~810 → **~761**; M5f ~133 → **~171**. Effect classes with no owner: 63 →
**0 (23 only in M5j's J9)**.
