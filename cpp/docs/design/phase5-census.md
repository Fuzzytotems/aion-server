# Phase 5 census

**Second edition, 2026-09-23.** Measured on HEAD `83db3742e` plus the working tree of that night. HEAD's game-server sources are those of
`760e8ab5c`; the two newer commits add only plan documents and oracle tools. Five chunks had uncommitted changes while it ran (P5-01, P5-02a, P5-03, P5-07,
P5-08; marked `*`, see §9). This edition replaces the first draft. Six verifiers checked the
draft by hand, and every systematic error they confirmed is now fixed in the script (§8). Tool: `tools/porting/census.py`. **Re-run it
before quoting a number from this page in a plan** (§10).

```
python tools/porting/census.py --self-check
python tools/porting/census.py --json census.json --markdown census.md --history 6f6756c06 29009d778 c1edb0afb 760e8ab5c
```

**Why this exists.** Counting `AION_UNPORTED` understates the work. In M5b-2 the effect classes turned out to have Java overrides that no
C++ header declared. Some were header-only shells with no `.cpp`; others had a `.cpp` that lacked the overrides. The site count jumped the
moment the header batch declared them (§2.1). The roadmap sizes every milestone from the site count plus "~85 undeclared". The census
checks every Java body against the C++ port, in every chunk, so plans can be sized from what is really open.

## 1. The headline

| Phase 5 (P5-00 to P5-16) | |
|---|---:|
| Java: files / lines / bodies in named classes | 1,101 / 98,533 / 6,631 |
| `AION_UNPORTED` sites / `AION_PARTIAL` sites | 1,672 / 19 |
| **Java bodies with no C++ declaration** | **1,606** |
| of them in the 322 Java files that have no C++ file | 1,356 |
| of them in Java files that do have C++ files (the hidden part) | 250 |
| Bodies declared but never defined | 0 |
| Enum constant bodies of an abstract method with no C++ (`AutoGroupType.newAutoInstance`) | 33 |
| Static initializers not ported (`PlayerScript`, `TeamCommand`) | 2 |
| **Open bodies** | **3,315** (2.0 × the site count) |
| Java lines in open bodies | 36,569 of 98,533 (37 %) |
| Ported as a stand-in in another chunk's file (not open; the owner still has to adopt them) | 61 |
| Client packets without a C++ file | 147 of 190 |
| Root AI handlers without a C++ file | 40 of 43 |

Open bodies are counted as bodies, not as sites. They are the Java bodies whose C++ is unported, partial, declared only or missing, plus
the constant bodies and initializers above. The first draft added the site count to the undeclared bodies instead. That formula gives
3,297 on this tree, 18 fewer, and is kept in the JSON as `open.siteFormula`.

**Phase 6** (the 24 handler chunks) has 7,797 bodies in 1,686 Java files. None of those files has a C++ file (§5). **Phase 4** has 96 open
bodies: 63 unported or partial, 30 undeclared and 3 C++-only stubs (§6).

**What it changes.** Across phase 5 the real work is about twice the site count, and the ratio varies a lot between chunks:

- P5-08, P5-09, P5-11 and the effect chunks are close to their site counts.
- P5-13 is 3.5 × its sites, P5-07 2.3 × and P5-10 1.8 ×.
- P5-05 (the root AI handlers) and P5-15/16 (the client packets) have almost no sites, but 197 and 450 open bodies.

Size the plans for M5b-3, M5f, M5g and M5j from §4, not from the roadmap.

## 2. Against the roadmap

| Measure | Roadmap (phase5-roadmap.md) | First draft (`c1edb0afb` + tree) | This edition (`760e8ab5c` + tree) |
|---|---:|---:|---:|
| `AION_UNPORTED` sites in the P5 chunks | 1,863 | 1,682 | 1,672 |
| `AION_PARTIAL` sites | - | 19 | 19 |
| Undeclared bodies | ~85 (effect classes only) | 1,641 | 1,606 |
| Open bodies | 1,863 + ~85 ≈ 1,950 | 3,342 | **3,315** |
| Java lines in open bodies | - | 36,837 | 36,569 |
| Client packets without a C++ file | 148 of 188 | 147 of 190 | 147 of 190 |
| Root AI handlers without a C++ file | 40 of 43 | 40 of 43 | 40 of 43 |
| Java lines in the phase-5 chunks | ~98,500 | 98,533 | 98,533 |

**The honest phase-5 total is 3,315 open bodies**, 1.7 × the roadmap's ≈1,950. They hold 36,569 Java lines. Between the two editions:

- **The tree moved.** The first draft's working tree became commit `760e8ab5c` (the effect classes). New working-tree changes then
  removed 10 more sites: 9 in P5-07 and 1 in P5-08.
- **The counting changed.** Open bodies are now counted as bodies, not as sites (§1). The 16 C++-only overrides in P5-10 no longer count
  twice.
- **The fixes moved about 60 bodies each way in phase 5** (§8.2).
  - Out of open (62): 43 stand-in methods, 7 renamed companions, 6 data-table accessors, 3 file-local helpers, 2 inlined helpers and 1
    documented omission.
  - Into open (67): 33 enum constant bodies, 30 enum constructors whose data has no C++ home, 2 initializers, and 2 empty constructors of
    classes with no C++.
  - 18 enum constructors whose data sits in the stand-ins are now "stand-in"; they were not open before either.
  - The total barely moves. The distribution between chunks does: P5-02b drops from 6 to 2, P5-08 from 180 to 174, P5-09 from 138 to 133,
    and P5-10 rises from 510 to 524.

### 2.1 What the 1,863 was

`--history` counts the sites at earlier commits and attributes them with today's manifest:

| Commit | | Phase 4 | Phase 5 |
|---|---|---:|---:|
| `6f6756c06` | M5b-2 stage 0 | 75 | 1,867 |
| `29009d778` | stage 1 part 1: the header batch declares the missing overrides | 74 | 2,080 |
| `c1edb0afb` | stage 1 part 2: P5-02a/b ported | 72 | 1,787 |
| `760e8ab5c` (HEAD's sources) | stage 1 part 3: the effect classes | 72 | 1,682 |
| working tree | P5-07 item-packet work in progress, P5-08 | 69 | 1,672 |

- **The roadmap's 1,863 is the stage-0 figure.** 1,867 minus P5-00's 4 sites is 1,863. That P5-00 (the login slice) was left out is an
  inference; the roadmap does not say how it counted.
- **The header batch added 213 sites:** P5-02a +31, P5-03 +64, P5-04 +118. This is the effect that motivated the census. Seen from the
  census side, P5-03/04 now have only 4 undeclared bodies.
- **The fall since then is real porting.** P5-02a/b went from 293 sites to 0. `760e8ab5c` removed 105 more (P5-03 −42, P5-04 −58, P5-01
  −5), and the working tree removes 10 (P5-07 −9, P5-08 −1).

### 2.2 Why ~85 was far too low

The ~85 estimated one kind of hidden work (effect overrides) in one area. The same gap exists in almost every phase-5 chunk, and there
are two more kinds of hidden work that the site count cannot see:

- **Whole files that were never started: 322 files, 1,356 undeclared bodies.** The largest groups:
  - the 147 client packets (450 bodies);
  - the 40 root AI handlers (189);
  - P5-13 (291), P5-10 (178) and P5-06 (99);
  - P5-12a/b (113);
  - small remainders in P5-01, P5-02a, P5-07 and P5-11.
- **Undeclared bodies in files that exist: 250** (§3).

## 3. Phase 5 per chunk

Column notes:

- *Undeclared*: the number in brackets is the part hidden in Java files that already have a C++ file.
- *Header-only shells*: C++ headers with no `.cpp` whose Java bodies are not all defined.
- *Missing classes*: Java files with no C++ file at all, with their bodies in brackets.
- *Client packets / AI handlers*: counted when they have no C++ file.
- *Stand-ins*: bodies ported under another name in another chunk's file. They are not counted as open (§7.2).
- `*`: the chunk was in flux when measured.

| Chunk | Java lines | UNPORTED | PARTIAL | Undeclared (in files with C++) | Header-only shells (bodies) | Missing classes: Java files without C++ (bodies) | Client packets / AI handlers without C++ | Stand-ins | Open bodies | Open Java lines |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P5-00 | 2,570 | 4 | 2 | 0 (0) | 0 (0) | 0 (0) | 0 / 0 | 0 | **6** | 306 |
| P5-01 * | 6,447 | 28 | 0 | 27 (12) | 0 (0) | 4 (15) | 0 / 0 | 16 | **55** | 525 |
| P5-02a * | 6,093 | 0 | 0 | 13 (4) | 0 (0) | 1 (9) | 0 / 0 | 8 | **13** | 140 |
| P5-02b | 3,412 | 0 | 0 | 2 (2) | 0 (0) | 0 (0) | 0 / 0 | 4 | **2** | 6 |
| P5-03 * | 4,422 | 122 | 0 | 4 (4) | 0 (0) | 0 (0) | 0 / 0 | 2 | **126** | 849 |
| P5-04 | 3,908 | 134 | 0 | 0 (0) | 0 (0) | 0 (0) | 0 / 0 | 0 | **134** | 1,042 |
| P5-05 | 6,249 | 0 | 5 | 192 (3) | 0 (0) | 41 (189) | 0 / 40 | 0 | **197** | 1,706 |
| P5-06 | 8,067 | 163 | 2 | 109 (10) | 5 (5) | 25 (99) | 0 / 0 | 2 | **273** | 3,645 |
| P5-07 * | 7,178 | 100 | 0 | 130 (121) | 30 (95) | 2 (9) | 0 / 0 | 3 | **230** | 4,463 |
| P5-08 * | 5,461 | 165 | 2 | 8 (8) | 0 (0) | 1 (0) | 0 / 0 | 5 | **174** | 2,813 |
| P5-09 | 5,961 | 121 | 1 | 11 (11) | 0 (0) | 0 (0) | 0 / 0 | 0 | **133** | 3,163 |
| P5-10 | 7,263 | 294 | 0 | 211 (33) | 0 (0) | 52 (179) | 0 / 0 | 16 | **524** | 3,662 |
| P5-11 | 3,739 | 87 | 3 | 3 (0) | 0 (0) | 1 (3) | 0 / 0 | 0 | **94** | 1,415 |
| P5-12a | 3,553 | 107 | 0 | 63 (4) | 0 (0) | 7 (59) | 0 / 0 | 1 | **170** | 1,753 |
| P5-12b | 4,967 | 198 | 1 | 76 (22) | 0 (0) | 13 (54) | 0 / 0 | 1 | **275** | 2,353 |
| P5-13 | 6,985 | 118 | 3 | 307 (16) | 0 (0) | 28 (291) | 0 / 0 | 1 | **428** | 4,313 |
| P5-14 | 2,981 | 31 | 0 | 0 (0) | 0 (0) | 0 (0) | 0 / 0 | 0 | **31** | 349 |
| P5-SC | 0 | 0 | 0 | 0 (0) | 0 (0) | 0 (0) | 0 / 0 | 0 | **0** | 0 |
| P5-15 | 5,035 | 0 | 0 | 244 (0) | 0 (0) | 78 (246) | 78 / 0 | 2 | **244** | 2,189 |
| P5-16 | 4,242 | 0 | 0 | 206 (0) | 0 (0) | 69 (206) | 69 / 0 | 0 | **206** | 1,877 |
| **total** | 98,533 | 1,672 | 19 | 1,606 (250) | 35 (100) | 322 (1,359) | 147 / 40 | 61 | **3,315** | 36,569 |

The "(1,359)" in the total counts every body in the files without C++. That includes 2 stand-ins and
`PlayerTeamDistributionService.doReward`, whose only C++ is an unported stand-in stub. `census.md` in the scratch output has the
full tables: sizes, statuses, undeclared bodies by kind, and the credits per rule. `census.json` lists every type with its undeclared,
unported, credited (rule and file) and arity-differs bodies.

**Where the 250 hidden bodies are.** These are the undeclared bodies in Java files that already have C++:

- **121 in P5-07.** Most sit in the item-action classes: thirty header shells with no `.cpp`, whose `canAct`/`act` overrides and private
  helpers are undeclared (m5b3-plan §2.3). Also the nested enums of `ItemPacketService`.
- **33 in P5-10.** Enum methods whose companion does not exist, such as `AutoGroupType` and the team enums. Their stand-ins in the packet
  helpers are credited separately.
- **22 in P5-12b and 16 in P5-13.** Enum data and accessors: `RiftEnum`, whose only stand-in is an `AION_UNPORTED` stub, `BaseOccupier`,
  `StageType` and `InstanceScoreType`.
- **12 in P5-01.** `DropRewardEnum` and the stat enums not yet covered by stand-ins.
- **Nested task classes**, for example `AuraEffect.AuraTask`, `FearEffect.FearTask` and `ConfuseEffect.ConfuseTask` (P5-03, 4 in all).
- **The xmlQuest `operate` bodies** and other helpers in P5-06 (10).
- **The rest (32):** P5-09 11 (`Profession`, `AbyssSiegeLevel`, `SiegeResult`), P5-08 8, P5-12a 4, P5-02a 4, P5-05 3 (the
  `SkillAttackManager` helpers) and P5-02b 2.

## 4. Per roadmap milestone

Files go to milestones by path. Where a chunk spans milestones, it is split by file or reported whole:

- **P5-09** is split as m5c-plan D1 does. Drop, rewards, passport, bonus and faction packs, and the guide go to M5b-3; trade, market, mail
  and craft go to M5c.
- **P5-13**: `restrictions/` goes to M5b-3, the rest to M5f.
- **P5-08**: `SkillLearnService`, `ClassChangeService` and `DialogService` go to M5e, and `services/teleport/` to M5f. The rest of P5-08
  (summons, abyss, duel, pvp, kisk, recall, social, ban, player services) has no milestone named in the roadmap and is reported under M5j.
- **Reported whole:**
  - P5-01, P5-03 and P5-04 under M5b-2. M5b-2 takes the magical half of P5-01 and the 34-class effect subset; M5b-3 takes 14 more
    classes, and the rest is deferred (m5b2-plan O-01).
  - P5-07 under M5b-3; M5c continues it.
  - P5-05 under M5j.
- **P5-15/16** get their own row: every milestone pulls in its own packets, and what is left falls to M5j.

| Milestone | Chunks | Java lines | UNPORTED | PARTIAL | Undeclared | Missing classes (files) | Missing packets | Open bodies | Open Java lines |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| M5a login slice (done) | P5-00 | 2,570 | 4 | 2 | 0 | 0 | 0 | **6** | 306 |
| M5b-2 abilities | P5-01, P5-02a, P5-02b, P5-03, P5-04 | 24,282 | 284 | 0 | 46 | 5 | 0 | **330** | 2,562 |
| M5b-3 loot and items | P5-07, P5-09 (drop side), P5-13 (restrictions) | 10,242 | 163 | 1 | 130 | 2 | 0 | **294** | 5,889 |
| M5c vendors and economy | P5-09 (trade, mail, craft) | 3,287 | 65 | 0 | 11 | 0 | 0 | **76** | 1,969 |
| M5d quest engine | P5-06 | 8,067 | 163 | 2 | 109 | 25 | 0 | **273** | 3,645 |
| M5e training and progression | P5-08 (3 services) | 656 | 14 | 0 | 0 | 0 | 0 | **14** | 455 |
| M5f travel and instances | P5-08 (teleport), P5-13 (rest) | 7,650 | 149 | 3 | 307 | 28 | 0 | **459** | 4,741 |
| M5g groups | P5-10 | 7,263 | 294 | 0 | 211 | 52 | 0 | **524** | 3,662 |
| M5h legion and housing | P5-11 | 3,739 | 87 | 3 | 3 | 1 | 0 | **94** | 1,415 |
| M5i siege and world events | P5-12a, P5-12b | 8,520 | 305 | 1 | 139 | 20 | 0 | **445** | 4,106 |
| M5j the rest | P5-05, P5-08 (rest), P5-14 | 12,980 | 144 | 7 | 200 | 42 | 0 | **350** | 3,753 |
| client packets (own row) | P5-15, P5-16 | 9,277 | 0 | 0 | 450 | 147 | 147 | **450** | 4,066 |
| phase 4 (before the roadmap) | 22 chunks | 139,212 | 69 | 0 | 30 | 14 | 0 | **96** | 1,238 |
| phase 6 (after the roadmap) | 24 chunks | 155,115 | 0 | 0 | 7,797 | 1,686 | 0 | **7,805** | 109,415 |

**Readings.**

- **M5b-2 has 330 open bodies.** 256 of them are the `AION_UNPORTED` bodies of P5-03/04 that the header batch declared. Only 46 are
  undeclared, and 30 more exist as stand-ins elsewhere.
- **M5b-3 is 294 bodies, not 163.** 130 are undeclared, and 121 of those are in P5-07 files that already have C++: the 30 item-action
  shells alone hold 95. It also carries the most open Java lines of any milestone: 5,889.
- **M5f is mostly hidden work.** 307 of its 459 bodies are undeclared, and 291 of them sit in the 28 P5-13 files with no C++ file, the
  instance score and reward model among them.
- **M5g is 524 bodies, not 294.** 211 are undeclared, 178 of them in the 52 team, autogroup and challenge files that have no C++ file.
  `AutoGroupType.newAutoInstance` adds 33 enum constant bodies that nothing in C++ implements.
- **M5d grows from 163 sites to 273 bodies.** The quest template handlers have no C++ file (`MonsterHunt`, `ReportTo` and the others in
  `questEngine/handlers/template/`, which holds only `fwd.h`), and neither does the `questEngine/task/` package.
- **The client-packet row (450 bodies) is not sized in the roadmap at all.** Every milestone that adds a packet pays part of it.

## 5. Phase 6: the handlers

No handler has a C++ file yet; the only C++ in phase 6 is the preludes (`QuestPrelude.h`, `InstancePrelude.h`). All 7,797 named bodies are
open. Add 8 static initializers (the butler lists of six Oriel and Pernon quests, and the position tables of `SauroSupplyBaseInstance`
and `KamarBattlefieldInstance`), and 7,805 bodies with 109,415 Java lines are open.

| Group | Chunks | Java files | Java lines | Named bodies | Anonymous-class bodies | Lambdas | Open bodies |
|---|---|---:|---:|---:|---:|---:|---:|
| Quests | Q01-Q14 | 1,035 | 95,539 | 4,061 | 30 | 32 | 4,067 |
| Instances | I1-I6 | 386 | 40,621 | 2,705 | 75 | 562 | 2,707 |
| AI handlers | A1 | 110 | 6,997 | 497 | 16 | 70 | 497 |
| Zones | Z1 | 3 | 135 | 7 | 1 | 2 | 7 |
| Chat commands | C1-C2 | 152 | 11,823 | 527 | 5 | 78 | 527 |
| **total** | 24 chunks | 1,686 | 155,115 | 7,797 | 127 | 744 | **7,805** |

The AI handlers are spread across the chunks. There are 461 in all (`data/handlers/ai/**`): 43 root handlers (P5-05) and 418 in phase-6
chunks (A1 110, I1-I6 308). 458 of the 461 have no C++ file. The per-chunk table is in `census.md` ("Phase 6").

Phase 6 is simpler to size than phase 5. There is no C++ to credit, so the census is a plain Java count, and the verifiers matched it
file by file with independent regex counters (§8).

## 6. Phase 4

Before the roadmap. What is open is small and mostly known:

| Chunk | Open bodies | What |
|---|---:|---|
| P4-15 | 34 | `DredgionScoreWriter` (no C++ file). The score writers' `AION_UNPORTED` bodies, and 11 constructors that throw through the file-local `unportedScoreBase()` in their member initializer lists. |
| P4-09 | 13 | 7 sites. `SpawnsData.loadSpawnsFromTemplateFiles`/`getRelativePath`. 4 documented omissions the comments do not name: the `StaticData` validation-task accessors. |
| P4-11a | 12 | 12 sites (`Npc.queueSkill`, `canSell`, the summon stat containers, ...) |
| P4-05 | 4 | 3 sites (`SellLimit`, `GMService`, `CAPTCHAUtil`). `GameTime.Month.getDays`, which is ported as the `MONTH_DAYS` array but not credited. |
| P4-02b | 5 | `ThreadPoolManager.getTaskCount`/`awaitTermination` and `IDFactory.initializeUsedIds`/`nextValidId`/`release`. These are ported, but folded into other functions without a citation. |
| P4-03 | 5 | 4 `Matrix4f` FloatBuffer methods (documented as "FloatBuffer methods", not by name). `Vector3f.create` (dead code). |
| others | 23 | P4-12 5, P4-16 6, P4-07a 4 (`Spawn`/`SpawnSpotTemplate` marshal hooks), P4-10 4, P4-14 2, P4-11b 1 (the `riftEnumData` stub), P4-17 1 |

Phase 4 also has 71 documented omissions and 52 bodies replaced by design, which are not counted as open. The replacements are T2's
binder, the handler class listeners, Quartz internals and the cron transformer.

## 7. Method

The script's docstring is the reference. In short:

### 7.1 Counting

- **Chunks.** `chunks.py`'s `Ownership` gives the Java files each chunk claims and the C++ files it owns. A `generated/` file counts for the
  chunk that claims the Java file its header names (`GENERATED by tools/xmlgen from X.java`). T2-gen keeps only the 90 generated files with
  no Java class.
- **Java.** Every claimed file is parsed with `tools/gen/javasrc.py`. The census counts classes (top-level, nested, inner, anonymous, local),
  bodies (methods, `@Override` methods, constructors, static and instance initializers) and lambdas. Every `->` that is not a switch-rule
  arrow counts as one lambda.
- **C++.** A declaration-level scanner reads all 6,726 files under `src/`, `handlers/` and `generated/`:
  - It keeps the first branch of every `#if`.
  - It splices the xmlgen member blocks into the class body that includes them.
  - It records classes, data members, enum values, and function declarations and definitions with their normalized parameter types, the
    names they call, and the `AION_UNPORTED`/`AION_PARTIAL` sites in each body, member initializer list included.
  - It reads every comment for citations of Java methods and documented omissions.
  - Sites are counted in code (comments excluded). The raw grep count is kept too; the two are equal in phase 5.

### 7.2 Matching and status

- **Name matching.** Each body of a named Java class is matched to its C++ class:
  - Scope: `aion::gameserver::<package>` plus the class path. `Outer::Inner`, `Outer_Inner` and every mix of the two are accepted.
  - Name: `skeleton.cpp_ident` (`delete_`). A constructor also matches `create`; `run`/`call` also match `operator()`; `iterator` also
    matches `begin`.
  - Arity: the declared parameter range.
- **Overloads.** Within one arity, the Java overloads take distinct declarations, best parameter-type match first (`int` ~ `int32_t`,
  `String` ~ `string_view`, `List`/array ~ `vector`/`span`, class names). This fixed the draft's "same-arity collapse", where
  `TeleportService.teleportTo` ×3 took their unported siblings' status.
  - An out-of-line definition is attached to the declaration with the same parameter types.
  - An inheriting constructor (`using Base::Base;`) serves every Java constructor.
  - A Java overload whose parameters all match the declaration another overload took is "merged" (`PolyArea(Collection)` and
    `PolyArea(Point2D[])` → one `span` constructor).
- **Credit rules**, for bodies that no plain name match finds. The `via` field in the JSON names the rule:
  - *renamed*: a doc comment `Java: Class.method(` directly above a function in the class's own chunk (`houseDoorStateOf`,
    `getMasterQuestIds`), or an enum companion under a derived name in the enum's own files (`brokerPacketTypeId`, `auctionResultOf`).
  - *standIn*: the same, but in another chunk's file (`PacketSupport.h::legionRankId`, `ChatUtil.cpp::replaceUnsupportedCommandChars`). Not
    open, but each is a small task for the owner (adopt it, delete the stand-in). The status is `unported` when the stand-in is itself a
    stub.
  - *fileLocal*: a private or static helper written as a function in the anonymous or `detail` namespace of the class's own files.
  - *inlined*: a comment inside a function of the class's files names the private Java helper it absorbed (`// Java isValidItemId: ...`).
  - *table*: an enum getter whose field sits in a table struct of the enum's files or companion (`AbyssSkillsData.race`).
  - *class alias*: a class marked `// fieldmap-class: <FQCN>` or documented `/** Java: <FQCN> */` directly above it (`StatShieldMasteryFunction`
    in `ShieldMasteryEffect.cpp`).
  - *constructors*: task structs and aggregates with no declared constructor, implicit copy constructors of value types, and empty
    constructors of classes that have C++.
  - *omitted*: named in a comment paragraph of the class's (or its package's) C++ files that documents a deliberate omission ("not ported",
    "not declared", "deviation", "replaces", "unused", "no caller", "does not exist", "has no counterpart"). A paragraph that says "yet",
    "until", "TODO", a milestone or a chunk does not count.
  - *replaced*: Java code the port replaced by design (the `REPLACED` table: T2's loadingutils, the handler class listeners, Quartz internals).
- **Status** of a matched body:
  - *unported*: its definition has `AION_UNPORTED`, directly or through a file-local stub helper it calls (`unportedScoreBase()`,
    `leaguePosition()`);
  - *partial*: `AION_PARTIAL`, or it calls a helper with unported sites;
  - *ported*;
  - *declared only*: no definition.
- **Sites no Java body maps to** are classified instead of counted blindly:
  - *taken*: they already count through a caller;
  - *standIn*: a stub whose doc comment cites a Java method; that body counts in its own chunk;
  - *stale*: a stand-in whose Java method is ported now, so the caller should be rewired (`followStartServiceNewFollowingToTargetCheckTask`,
    `pvpMapServiceIsOnPvPMap`);
  - *inherited*: a C++-only narrowing override of a mapped base function (the 16 in P5-10);
  - *own*: everything else, counted open.
- **Enum constructors** are "enum ctor" (not open) when their data has a C++ home: a companion, explicit enumerator values, a table struct,
  or stand-ins. Otherwise they count as undeclared, kind `enumData`: 30 in phase 5. Treat this as a judgment; a planner who counts the data
  with its accessors can subtract it.
- **Enum constant bodies** implement the enum's method of the same name. When that method is abstract and open, the constant bodies count
  open (the 33 of `AutoGroupType`); otherwise they are "anon in open" (100 in phase 5).
- **Static initializers** are matched by the fields they assign:
  - a field defined in the class's C++ files: ported;
  - next to an "until ... ported" note: partial (`PlayerScript.LUA_SANDBOX_FIX`);
  - an enum's `values()` lookup map that a companion replaces, or a comment that names the static initializer: ported (`ChatType`,
    `DialogAction`);
  - otherwise open (`TeamCommand`).
- **In flux.** A chunk owning a file that `git status` reports as modified or untracked is measured as the working tree and marked `*`.

## 8. Accuracy

### 8.1 The verification round

Six verifiers each took one shard of chunks (§ Appendix). Each read 71 to 103 classes by hand, 521 in all, and re-counted every chunk of
the shard independently:

- sites: `chunks.py files` plus grep;
- Java: `wc -l` and independent regex method and lambda counters;
- the no-C++ lists: a class search over the whole C++ tree.

On the first draft they found:

- **Exact in every shard:** site counts (code and raw), Java files, lines, bodies and lambdas, the no-C++ lists, the header-only shells,
  the in-flux flags, and all of phase 6.
- **Phase 5, undeclared but not open work: about 72 of 1,641 bodies (4.4 %).** About 23 were ported in the owner's files under another
  name or shape: renamed companions, file-local helpers, fieldmap classes and overload pairing. About 49 were stand-ins in other chunks.
  Small chunks were badly off: in P5-02b 6 of 6 undeclared bodies were not open work, in P5-09 7 of 16, in P5-08 8 of 12.
- **Phase 5, open work missed: 36 bodies.** These were the 33 `AutoGroupType` constant bodies, one stubbed static initializer, and 2
  empty constructors of classes with no C++.
- **Phase 5, double counted: 16 sites.** These were narrowing overrides in P5-10.
- **Wrong statuses: 5 bodies.** These were same-arity overloads: `TeleportService` ×3, `enterWorld` and the `SM_INSTANCE_SCORE`
  constructor.
- **Phase 4: about 150 of 179 undeclared bodies were not open work.** These were T2's replaced binder, the handler listeners, inheriting
  and aggregate constructors, and documented jME and awt omissions. On the other side, 11 constructors were counted ported but throw
  through a helper, and 8 P4-11b stand-in stubs were counted twice.
- 11 generated files were attributed to the wrong chunk.

**Measured error of the draft:** phase-5 open bodies were overstated by about 52 (−72 − 16 + 36) of 3,342, **1.6 %**. The headline held;
the per-chunk undeclared figures of small chunks did not.

### 8.2 After the fixes

Every systematic error the verifiers confirmed was re-checked against the sources and is now a rule of the script (§7.2). `--self-check`
covers each rule twice: with a synthetic case, and with the verifiers' own answers on the real tree (`LIVE_CHECKS`: 45 known bodies, 3
initializers and 6 aggregate checks, all passing). Of the verifiers' findings, these are left:

| Left | Bodies | Why |
|---|---:|---|
| Phase 5 stand-ins with no citation or name link | 6 | `ItemUpdateType.getMask`/`isSendable`, `ItemDeleteType.fromQuestStatus`, `UnsummonType.getDelayMillis`, `ArtifactStatus.getValue` and `ResourceType.getValue`. These are tables or inline casts in other chunks. |
| Phase 4 folded or renamed without a citation | 11 | P4-02b 5; `GameTime.Month.getDays`; `ServerPacketsOpcodes.getOpcode`/`addPacketOpcode` (a generated variable template); `AionServerPacket()`; `FlyPathType.getId` and its constructor (an inline cast) |
| Phase 4 omissions documented without the Java names | 8 | the 4 `StaticData` validation-task accessors and the 4 `Matrix4f` FloatBuffer methods |
| Double-counted stand-in site | 1 | `riftEnumData` (P4-11b): its comment cites no method, and the `RiftEnum` bodies it stands for are open in P5-12b |

**Remaining error on what the verifiers found:** 6 phase-5 bodies of 3,315 (0.2 %) and about 20 phase-4 bodies. The new rules are
heuristics too, and no independent pass has checked them yet. Their risks are in §9, item 2; the `credited` lists in `census.json` make
every credit auditable.

## 9. Known limitations

1. **Matching is heuristic.** It uses class, name, arity and a coarse parameter-type similarity. An unrelated C++ function with the same
   name and arity counts as the port.
2. **The credit rules can over-credit.**
   - A citation above an unrelated function credits it.
   - An omission paragraph can name a method that is only partly omitted.
   - "Merged" can pair two overloads that are really different.
   - A stand-in may cover only part of its Java method. `CM_CHARACTER_EDIT.checkOrRemoveTicket`'s stand-in checks without removing.
   - The "inlined" rule reads a comment inside a body as the port of a private helper.
   Audit the `credited` lists when a chunk's number matters.
3. **Anonymous and local class bodies are not matched individually**, except the enum constant bodies of an abstract method. They count
   with their enclosing member (162 in phase 5, 100 inside open members).
4. **Helper propagation is limited.** It works within one file and by name. Callers in other files keep their status: the P4-11b
   controllers that call a stand-in reach `AION_UNPORTED` at run time, but count as ported.
5. **Initializers.** An initializer that assigns no field of its class is "unassessed" (`GameServer`, `StatCapUtil`, `ServerPacketsOpcodes`).
   A stubbed initializer is caught only through an unported note next to the field.
6. **The C++ scanner is heuristic:**
   - it keeps only the first branch of each `#if`;
   - its macro and template-argument rules are guesses;
   - prefixed raw strings are not handled.

   Files with unbalanced brackets are listed in `cppIndex.unbalanced`; there were none in this run.
7. **The working tree was changing during the run.** Other agents committed `760e8ab5c` during this session and were editing P5-01, P5-02a,
   P5-03, P5-07 and P5-08. Those rows are a snapshot.
8. **Sites are occurrences.** The UNPORTED column counts occurrences, as the roadmap's grep did; open bodies count bodies.
9. **Milestone splits are by file path only** (§4). P5-01, P5-03/04 and P5-07 are reported whole, and the client packets are one row.
10. **Only `@Override`-annotated methods count as overrides.** Accessor detection uses only xmlgen's patterns.
11. **The shards weigh by Java lines.** P4-06 (`SM_SYSTEM_MESSAGE`, 28,959 lines of generated factories) weighs far more than it costs to
    verify.

## 10. Re-running it before a milestone plan

1. **Check the tree.** Run `git status` and note which chunks of the milestone are in flux. A clean tree, or at least committed milestone
   chunks, gives numbers that the next person can reproduce.
2. **Run the checks.** `python tools/porting/census.py --self-check` takes about 30 s. A synthetic failure means a rule broke. A live-tree
   failure usually means the port moved (a stand-in adopted, a helper declared): update `LIVE_CHECKS` if the new answer is right.
3. **Run the census.**
   `python tools/porting/census.py --json <scratch>/census.json --markdown <scratch>/census.md --history <last milestone commit> HEAD`
   takes about 15 s.
4. **Size from the milestone row** (`milestones.<id>` in the JSON): open bodies and open Java lines, not sites. The work list per chunk is
   in `chunks.<id>.types`:
   - `undeclared`, `unported`: the open bodies;
   - `credited`, with rule and file: stand-ins become adopt-and-delete tasks for the plan;
   - `abstract`: abstract enum methods;
   - `initializers`.

   The helper sites are in `chunks.<id>.helperSites`; plan the `stale` ones as rewiring.
5. **Quote the numbers with their HEAD** and the in-flux chunks.
6. **Encode new false positives.** When a plan finds one, add the rule to `census.py`, a synthetic case to `self_check`, and the body to
   `LIVE_CHECKS`. Do not correct the numbers by hand.

## Appendix: verification shards

Six shards of similar Java size. They are balanced with LPT on Java lines, and each lists its chunks in manifest order. A verifier runs
`census.py --chunks <ids> --json ...` and checks the per-type records against the sources.

| Shard | Java lines | Chunks | Classes read |
|---|---:|---|---:|
| 1 | 65,819 | P4-06, P4-10, P5-00, P5-05, P5-11, P5-15, Q04, Q11 | 71 |
| 2 | 66,096 | P4-05, P5-08, P5-09, P5-10, P5-14, Q05, Q08, Q12, I4, C2 | 81 |
| 3 | 66,199 | P4-04, P4-07b, P4-09, P4-11b, P4-15, P5-02b, Q03, Q06, I5, I6 | 96 |
| 4 | 64,669 | P4-15a, T2, P4-11a, P4-12, P4-14, P4-17, P5-01, P5-12a, P5-12b, P5-13, I3, C1 | 103 |
| 5 | 65,522 | P4-01, P4-03, P4-07a, P4-13, P5-03, P5-06, Q01, Q07, Q10, Q14, I1 | 91 |
| 6 | 64,555 | P4-02a, P4-02b, T2-gen, P4-16, P5-02a, P5-04, P5-07, P5-SC, P5-16, Q02, Q09, Q13, A1, I2, Z1 | 79 |
