# Phase 6: the quest transliterator (G1), production status

> **Status:** G1 lane, 2026-10-04. Branch `worktree-agent-aee6fb6919a624b8b`, based on C++ `47412f50b` (the summon-packets merge, PR #56),
> worktree build `cpp/build/msvc` (Debug; the main checkout's `vcpkg_installed`). Python-only tooling plus measurements: **no file under
> `game-server/src`, `game-server/handlers` or `game-server/tests` changed, and no new handler is registered in any build.** Everything
> below was run on this tree; where a number comes from a scratch experiment outside the repository, the text says so (§3.5).
>
> Builds on [phase6-questgen-prototype.md](phase6-questgen-prototype.md) (the prototype, rev 2, and its P6-T and B26-B30 notes) and
> [p6q-ascension-route.md](p6q-ascension-route.md) (the 161 generated handlers in the tree and the golden-trace harness). The inventory's
> plan is [phase6-inventory.md](phase6-inventory.md) §7.1 (G1) and §7.6 (how a generated handler is tested).

---

## 0. The answer

1. **The transliterator exists and is the P6-Q generator.** `tools/gen/questgen` (prototype rev 2, P6-T rules, rows B26-B30) is what the
   P6-Q lanes used to generate the 161 handlers now in the tree; this lane extends it rather than writing a second tool (§1).
2. **It now transliterates 972 of the 1,035 Java quest handlers (93.9%; 87,746 of 95,539 Java lines): tier A 708, tier B 264.** The
   inventory predicted tier A 660 and 914-929 with every tier-B rule (§3.1). New in this lane: rule `scheduled-closure` (+27 files: the
   use-bar `Runnable`s and timer lambdas) and API rows B31-B38 (+10 files). The 935 files of the P6-T output are byte-identical, and so are
   the 161 generated files in the tree.
3. **All 972 compile with no error and no warning** (`cl /W4`, a Q chunk's Debug flags, each file on its own), and `aion_gs_regscan` with
   the Java cross-check reports 0 errors over all 972 (§3.2). This is the compile check the prototype could not run (its §8.1). The
   70% bar of `handlers-and-porting-plan.md:558` is met on compiled files with room to spare: 93.9% of all quest handlers.
4. **Behaviour, golden traces:** the 167 golden tests of the tree pass on this branch. In a scratch build of the same harness that adds
   **620 more generated handlers** (every transliterated file that is not in the tree and has an oracle document with a case), **14,304 case
   variants pass and 114 fail, every failing one in one of 7 hooks the harness does not drive yet**; the registration trace finds no
   mismatch. The rest are harness bookkeeping and fixture limits, not handler differences (§3.5).
5. **Behaviour, hand ports:** nine files the tree holds as hand ports now transliterate. Three (`_1007`, `_2009`, `_2007`) are byte-identical
   to the hand port from the class line on; the other six differ only in how the closure captures are spelled (§3.6).
6. **Refused: 63 files**, each with its reasons (§2.4): 22 closures outside a `schedule` call (16 of them the mentor dailies, G2's N-way
   case), 29 API gaps (no gap blocks more than 3 files), 6 static-initializer sets, 3 `List<Integer>` fields, 2 per-player handler fields, 1
   `throw`.
7. **Not landed, by design.** Under the owner's U1/U7 rule a generated handler merges only in its Q chunk with the gate traffic measured
   (the P6-Q slices' method). The recommended next step is to land the remaining chunks that way, after a harness extension for the 7 hooks
   (§5); about 811 files are waiting.
8. **Step 1 done (lane C, 2026-10-05, §7):** the harness drives the 7 hooks and an eighth (`onKillRankedEvent`), models the 9 registration
   kinds and `registerOnKillRanked`, and runs the oracle's scheduled tasks; the sample is a committed tool (`goldensample.py`). Over the 649
   transliterated files outside the tree that have a case: **19,499 variants pass, 0 fail** (17,908 and 114 before), no quest stops, the
   registration trace has no finding.
9. **Step 2, chunk Q08 landed (lane C, 2026-10-05, §8):** the 64 generated gelkmaros and enshar handlers are in the tree (63 at the
   landing, `gelkmaros/_20034` by the follow-up and API row B39), with their golden traces (1,368 variants pass, 0 fail) and
   docs/deviations/Q08.md.
10. **Step 2, chunk Q01 landed (lane C, 2026-10-05, §9):** 81 of the 82 reshanta handlers are in the tree generated, with their golden traces
   (1,493 variants pass, 0 fail) and docs/deviations/Q01.md; questgen gained rows B40-B42 and two fixes (979 of 1,035 transliterated);
   `reshanta/_2759` is a hand port with a per-player kill record (the owner's correction of 2026-10-05, docs/deviations/Q01.md).
11. **Step 2, chunk Q02 landed (lane C, 2026-10-05, §10):** all 59 inggison handlers are in the tree, generated, with their golden traces
   (1,417 variants pass, 0 fail) and docs/deviations/Q02.md; questgen gained rows B43-B44 (981 of 1,035 transliterated).

---

## 1. Relation to the P6-Q generator

The P6-Q "route-gen" generator **is** questgen: the 36 route handlers (Q05, Q09, Q06) and the 125 of slice 2 (Q03, Q10) were emitted with
`python -m tools.gen.questgen --emit` and copied unchanged; `tools/gen/tests/test_questgen_tree.py` regenerates each bannered file from its
Java and requires byte equality. Its harness is `tests/quest_handlers_golden` (the golden traces of `tools/oracle/questtrace`, phase6-inventory
§7.6 item 3). Writing a second transliterator would have duplicated a tool that already covers the brief's tier A, most of tier B and the API
table, has 90 unit tests and is pinned by the drift test, so this lane:

- adds the rules and rows the remaining refusals called for (§2.2, §2.3);
- adds the compile check the prototype could not run (`tools/gen/questgen/compilecheck.py`, §3.2);
- measures behaviour with the existing harness on the corpus outside the tree (§3.5).

The drift test now regenerates with the driver's full rule set (`emit.ALL_RULES`); the 161 files are unchanged by it. `--p6t-rules` and
`--prototype-rules` keep the older rule sets for comparison.

## 2. The rules

### 2.1 Tiers and the vocabulary

As in the prototype (§1.2-§1.4 there): a file is **tier A** when it calls only the core vocabulary (`api.CORE`: the `AbstractQuestHandler`
helpers without spawn and follow, `QuestEnv`, `QuestState`, the state list, `QuestService` start/finish/collect/abandon, registrations,
simple getters, `HandlerResult`, `ZoneName.get`, the close-dialog packet), and **tier B** when it uses at least one API-table row (`api.API_TABLE`,
38 rows now). Every call is typed against the C++ headers (`cppdecl`), the Java overload is checked against C++'s ranking, and a member outside
the vocabulary refuses the file as `api-missing`. The idiom rules of the prototype and P6-T (`varargs-inline`, `work-items`,
`switch-expression`, `nested-array`) are unchanged.

### 2.2 Rule `scheduled-closure` (new, `emit.G1_RULES`)

`jast.Parser(closures=True)` parses a lambda with untyped parameters and an anonymous class that overrides exactly one `void` method into a
`Closure` node; the default parser keeps its refusals, so `tools/oracle`'s extractor (which parses with jast) is unchanged (its 517 tests
pass). The emitter admits a closure in one place only, the task of `ThreadPoolManager.getInstance().schedule(task, delay)`, and emits the
pinned form of `utils/ThreadPoolManager.h`:

```cpp
ThreadPoolManager::getInstance().schedule({this, &env}, [this, player = runtime::Ref<Player>(player), itemObjId, id, &env,
		qs = runtime::Ref<QuestState>(qs)] {
	PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), itemObjId, id, 0, 1, 0), true);
	player->getInventory().decreaseByObjectId(itemObjId, 1);
	giveQuestItem(env, 182201327, 1);
	qs->setQuestVar(1);
	updateQuestStatus(env);
}, 3000);
```

(`eltnen/_1361FindingDrinkingWater.java:49`, emitted on one line.) The captures follow Java's (effectively final locals, by value; an object
by reference) within lint L5's rules for stored lambdas:

| Java capture | C++ | Why |
|---|---|---|
| primitive, enum, `Integer`, `int[]` constant | copy | Java copies the value |
| a local of a class type (`Ptr<T>`) | `name = runtime::Ref<T>(name)` | a `Ptr` is a borrow that ends with the task scope (L5 forbids copying it); the `Ref` keeps the object alive as Java's reference does; a null one throws NullPointerException on use, as Java does |
| a `T&` (the hook's `QuestEnv& env`, `Item& item`) | `&name`, pinned | L5: every `&x` capture is in the pin list |
| the handler | `this`, always pinned; captured when the body reaches a member or helper | the handler is an Immortal (RT-11); a pinned lambda may capture only copies when `this` is not needed |

Inside the body, `run()`'s `return;` is the lambda's, `[[maybe_unused]]` and comments work as elsewhere. **Refused** (category in
brackets): a closure anywhere else (`lambda`/`anonymous-class`; recorded when its method is parsed, so a refused statement before it cannot
hide it, which is how the mentor dailies' `anyMatch(member -> ...)` stays visible), a closure with parameters, an anonymous class that is not
`Runnable.run` or has more than one member, `this` inside an anonymous class (Java's `this` is the Runnable), a `String`, raw pointer or value
capture (`closure-capture`: lint L5 forbids `string_view` copies), more than four pinned references (`Pin::MAX_OWNERS`), and the returned
Future used as a value. 29 files use the rule (row B31): the 27 that only needed it and `beluslan/_4200`, `heiron/_3200`, which also needed
row B33.

`tools/parity` learns the capture spelling (`name = runtime::Ref<T>(name)` inside a capture list is the captured name, not a call; any other
`runtime::Ref<T>(x)` still is a call); all 972 files are at parity with their Java. `lint_concurrency.py` over the 972 emitted files reports no
L5 finding (§3.4).

### 2.3 API rows B31-B38 (new) and configuration flags

| Row | Members | Files | Body |
|---|---|---|---|
| B31 | `ThreadPoolManager.schedule` (spelled by the rule: `schedule` is a member template) | 29 | ported |
| B32 | `TeleportService.teleportToNpc` | 1 (`ishalgen/_2007`) | ported (P6-Q route-hand) |
| B33 | `WorldMapInstance.getNpc` | 2 (`beluslan/_4200`, `heiron/_3200`) | ported |
| B34 | `Storage.isFullSpecialCube` | 2 (`crafting/_29000`, `morheim/_24021`) | ported |
| B35 | `Player.isInGroup` | 1 (`inggison/_10032`) | ported |
| B36 | configuration flags: `CustomConfig.X`, `GroupConfig.X` | 2 (`ascension/_1007`, `_2009`) | ported (configs/main) |
| B37 | `Creature.getAggroList`, `AggroList.addHate` | 1 (`clash_of_destiny/_24030`) | ported |
| B38 | `Player.getTitleList`, `TitleList.addTitle` | 1 (`morheim/_24022`) | ported |

B36 is vocabulary, not a rule: a Java `CustomConfig.X` is the C++ class's `static inline std::atomic<T> X` of the same name, read through its
implicit conversion as the hand ports do (`if (CustomConfig::ENABLE_SIMPLE_2NDCLASS)`); only `bool` and integral flags, and a flag the header
does not declare is `api-missing`. `api.HEADERS` gains `controllers/attack/AggroList.h` and `model/gameobjects/player/title/TitleList.h`.
Rows are vocabulary, so `--p6t-rules` (943 files) and `--prototype-rules` (922) gain the eight row files that need no closure.

Every direct callee of the 972 files has a ported body by `census.py`'s report (the prototype counted 51 unported members; they have been
ported since). Deeper reach is not traced, as before: `TeleportService` still has 7 `AION_UNPORTED` bodies and `InstanceService` 1.

### 2.4 What is refused, and why (63 files)

Primary reason (the `--dry-run -v` report has every file's full list):

| Files | Reason | What it would take |
|---|---|---|
| 22 | `lambda`: a closure outside `schedule` | 16 mentor dailies (`kaisinel_academy/_37000` ... `the_circle/_47113`: `group.getMembers().stream().anyMatch(member -> ...)`, all also `Player.isInGroup` at the same line) are G2's N-way case: one hand port, 15 clones (phase6-inventory §7.2). `_1006`, `_2008` (`forEachNpc`, hand-ported in the tree), `_1114` (`getKnownList().forEachNpc`, hand-ported), `eltnen/_14026` (`Arrays.stream`, `findObject`). A rule for `forEachNpc` / `anyMatch` over a C++ range would need the C++ collection APIs first |
| 29 | `api-missing` | 22 distinct members; the most blocked: `DataManager.SPAWNS_DATA` 3, `new SM_ASCENSION_MORPH` 3 (one the only reason), `HousingService`, `SiegeService`, `broadcastPacketAndReceive`, the bonus reward list (`vector<value:QuestItems>.add`, the header-signature question of the prototype's §5.2 item 2) 2 each; every other gap one file. Each further row unblocks at most 3 files |
| 6 | `initializer`: a `Set<Integer>` of butlers filled in a static block (oriel/pernon housing) | hand port (or G2 for the pairs) |
| 3 | `generic-type`: `List<Integer>` fields (`pangaea/_14220`, `_24220`, `reshanta/_2759`) | hand port |
| 2 | `mutable-field`: per-player state in the singleton handler (`eltnen/_1367`, `morheim/_24026`; the Java races of phase6-inventory §11) | hand port with a decision on the race |
| 1 | `throw` (`pandaemonium/_2900`, hand-ported in the tree) | - |

Mirror pairs (the driver's `mirrorPairs` report): 186 equal-structure pairs, **169 both transliterated, 17 both refused** (G2: hand-port one,
clone the other); the refused files form 12 equal-structure groups of 34 files (22 clones).

## 3. Measurements

### 3.1 Coverage against the prediction

`python -m tools.gen.questgen --dry-run` (about 15 s):

| Rule set | Transliterated | Tier A | Tier B |
|---|---|---|---|
| prototype rev 2 (`--prototype-rules`) | 922 | 699 | 223 |
| P6-T (`--p6t-rules`) | 943 | 708 | 235 |
| **driver default (P6-T + `scheduled-closure`)** | **972 (93.9%)** | **708** | **264** |
| inventory prediction (phase6-inventory §7.1) | 914-929 with every tier-B rule | 660 (classifier) | 254-269 |

Before this lane the three were 916 / 935 / - (phase6-questgen-prototype.md, the B26-B30 note). The tool's tier A is wider than the
classifier's, as the prototype explains (§3 there).

### 3.2 Compile check (`tools/gen/questgen/compilecheck.py`, new)

```
python -m tools.gen.questgen.compilecheck --stage <dir outside the repo> --jobs 4 --regscan --json <out>
```

It emits every transliterated file into the stage, builds the quest prelude as a PCH, and compiles each emitted file **on its own** with the
Debug flags of `aion_gs_handlers_quest_q05.vcxproj` (`/std:c++latest /permissive- /W4 /wd4100 /wd4251 /wd4275 /EHsc /MDd`, the include roots
of the handler libraries, `AION_CHECKED=1`), without `/WX` so that warnings are counted apart from errors; with `--regscan` it compiles
`game-server/tools/regscan` into the stage and runs it with `--java-handlers` (markers, namespaces, the handler file rules and the quest id
against Java's `super(...)`). Result on this branch's HEAD (55 s):

| | Files | Clean | Warnings | Errors |
|---|---|---|---|---|
| tier A | 708 | 708 | 0 | 0 |
| tier B | 264 | 264 | 0 | 0 |
| **all** | **972** | **972** | **0** | **0** |

`aion_gs_regscan`: exit 0, 0 errors, quest 972/1035 matched to Java. The tree's chunks build in unity batches of 16 (AionChunks.cmake); the
golden-sample build of §3.5 compiled the 620 staged files in batches of 40 with no diagnostic, so the batch form holds too for that sample.

### 3.3 Parity

`tools/parity/parity.py tree` over the 972 emitted files: 972 pairs compared, 0 with mismatches; over the tree's handler directories, 178
pairs, 0 mismatches (unchanged).

### 3.4 Lint

`lint_concurrency.py` over the 972 emitted files: 153 errors, **all L2** (`static constexpr std::array` constants where `fieldmap.json`
expects `Ref<Array<int32_t>>`; the known gap of p6q-ascension-route.md §5 item 6, which shows as 19 L2 findings on the tree's handler
directory too). No L5 (stored lambda) finding, so the closure rule's captures pass the lint.

### 3.5 Golden traces (scratch build, outside the repository)

**In the tree:** `ctest -R Golden` on this branch: 167 of 167 pass (163 quests' cases, the registration trace, the negative control).

**The sample.** `tools/oracle/questtrace` writes a document with at least one case for 792 of the 972 transliterated files (it refuses hooks
with closures and expressions it does not model; for 33 files it raises). The sample is every transliterated file that is not in the tree, is not one of the 20 held back
(`GOLDEN_HELD_BACK`) and has a document with at least one case: **620 files, tier A 521, tier B 99**. None of the 29 closure files is in it:
the oracle refuses closures. The documents were written to a scratch directory with `extract.trace_file`; nothing in `tools/oracle` changed.

**The build.** A scratch copy of `GoldenQuestTraceTest.cpp` and `GoldenHandlers.h` with the 620 rows appended to the handler table and
`EXPECTED_DIR` pointed at the scratch documents (the 181 committed ones plus the sample's), the five committed `Golden*Handlers.cpp`, and 16
batch files that `#include` the staged `.cpp` files, compiled with the test target's flags and linked against this branch's
`aion_gs_handlers_quest_q05_tests` libraries (cl and link directly, nothing under `cpp/` written). For the registration trace, the scratch copy
also catches one handler's exception or untraced document and goes on with the next (in the tree, the first one stops the test).

**Case results** (one run, 72,432 compared runs):

| | Variants | Quests |
|---|---|---|
| passed | 14,304 | 620 ran; 391 with no finding at all |
| **failed** | **114** | **14, every failure in a hook the harness does not drive**: `onDredgionRewardEvent` 4 quests, `onUseSkillEvent` 2, `onFailCraftEvent` 2, `onInvisibleTimerEndEvent` 2, `onKillInWorldEvent` 2, `onEnterWindStreamEvent` 1, `onLeaveZoneEvent` 1 (the harness throws "hook X is not driven", so ACTUAL does nothing and the replay of the Java effects differs) |
| not reproducible, not listed | 247 | 72 (no fixture setup reproduces the case's assumptions; the tree lists such cases in `knownNotReproducible`) |
| vacuous, not listed | 232 | 118 (no observable effect in either run; the tree lists them in `knownVacuous`) |
| stopped by a fixture exception | - | 34: 26 kill-ranked quests (`reshanta/_1702` ...: the oracle does not trace `AbyssRankEnum` arguments), 8 above the fixture's level cap ("The given level is higher than possible max") |
| reaches `AION_UNPORTED` | 1 case | `sanctum/_1987` (`WarehouseService::expand`, as 2985 in the tree) |

**Registration trace** over the 161 + 620 handlers: **no mismatch**. Not checked: 33 quests whose `register()` uses a kind the harness does not
model (`registerOnPassFlyingRings`, `OnBonusApply` 11, `OnFailCraft`, `OnKillInWorld`, `QuestSkill`, `OnDredgionReward`, `OnInvisibleTimerEnd`,
`OnEnterWindStream`, `OnLeaveZone`), the 8 above the level cap, and the 26 the oracle does not trace.

So, on the hooks and registrations the harness drives, no compared case of the sample differs from its Java trace. What is not covered
(the undriven hooks, unreproducible and vacuous cases, the fixture's limits) is harness work, listed in §5.

### 3.6 Hand ports the rules now reproduce

Nine files in the tree are hand ports that questgen refused before and emits now; compared from the `class` line on:

| File | Differing lines | What differs |
|---|---|---|
| `ascension/_1007ACeremonyinSanctum`, `_2009ACeremonyinPandaemonium`, `ishalgen/_2007WheresRaeThisTime` | 0 | byte-identical |
| `altgard/_2208`, `_24013`, `heiron/_1643`, `_3200`, `ishalgen/_2002`, `_2136` | 8-34 | the closure: the hand ports pin `Player&` / `QuestState&` references (`{this, &user, &state, &env}`), the rule captures `Ref`s and pins `this` and `env`; line wrapping; one class comment of `_3200` the generated file drops |

Their unit tests (Q03 44, Q09 56, Q10 24 passing; the database-backed cases skip without `AION_TEST_GS_DATABASE_URL`) pass at this branch's
HEAD with the hand ports. Running them against the generated files was not done: it means replacing handler files in the tree, which this lane
does not do.

### 3.7 Test suites

`tools.gen` 426 tests: all pass (the three MSVC skeleton compile tests need `AION_VCPKG_INCLUDE` in a worktree, as their CTest entry sets
it); `tools.parity` 36; `tools.oracle` 517; the golden harness 167.

## 4. How generated handlers reach the server (unchanged)

The owner's U1/U7 rule (owner-decisions.md): generated quest handlers merge only in their own Q chunk, in spare build slots, and a phase-6
change that turns a phase-5 gate red is reverted, not fixed forward; since 2026-09-29 a gate may follow Java-faithful quest traffic, each such
change listed. The P6-Q slices applied it by measuring the gates' quest traffic and holding back handlers that start quests or add markers a
gate does not expect (p6q-ascension-route.md §5, §8.3). This lane registers nothing: the 811 transliterated files without a generated file in
the tree (9 of them hand-ported there, §3.6) stay in the stage, and `test_questgen_tree` keeps pinning the 161 in it.

## 5. Next steps

| Step | Effort (estimated) | Depends on |
|---|---|---|
| **Done (§7):** golden harness: drive the 7 hooks of §3.5 and model the 9 registration kinds; a fixture level above the cap; `AbyssRankEnum` in the oracle (26 kill-ranked quests); keep `knownVacuous` / `knownNotReproducible` per chunk as the slices did | 1-2 days | `tests/quest_handlers_golden` (Q05's), `tools/oracle` (the scenario lane's area: coordinate) |
| **Done (§7):** oracle closure support, so the 29 B31 files get golden cases (they compile, are at parity and match six hand ports) | about 1 day | the oracle's owner |
| Land the remaining chunks the slice-2 way: copy the generated files, add the chunk to the golden table, measure the gate traffic, hold back what a gate does not expect, run unit suite and gates | about 1 day per chunk (slice 2 landed 125 files in one night lane per chunk); 811 files over the 14 Q chunks | a build slot per chunk; the gates' owners for the held-back quests |
| Replace the nine hand ports of §3.6 with generated files (three are already identical) and run their unit tests | 0.5 day | the chunk owners (Q03, Q06, Q09, Q10) |
| A lint rule for generated constant arrays (L2), before the handler tree is linted | 0.5 day | `tools/porting` |
| `api.PLANNED`: `HandlerResult.fromBoolean` is declared now (`HandlerResultInfo.h`, H-06), so the report's "planned" label on 67 files is stale; make it a plain declaration | 1 hour | - |
| The 63 refused files: G2's N-way mode for the 16 mentor dailies and the 17 refused mirror pairs; hand ports for the rest (§2.4) | G2 about 14 h (phase6-inventory §7.1); hand ports about 30 files | G2 |

## 6. Reproduction

From `cpp/`:

```
python -m tools.gen.questgen --dry-run [--json OUT] [-v]                       # 972 of 1,035 (about 15 s)
python -m tools.gen.questgen --dry-run --p6t-rules -q                           # 943
python -m tools.gen.questgen --dry-run --prototype-rules -q                     # 922
python -m tools.gen.questgen.compilecheck --stage <dir> --jobs 4 --regscan      # 972 clean, regscan 0 errors (about 1 min)
python tools/parity/parity.py tree --java-dir ../game-server/data/handlers/quest --cpp-dir <dir>/src/aion/gameserver/handlers/quest
python tools/porting/lint_concurrency.py <dir>/src
cd tools/gen && python -m unittest tests.test_questgen_g1                       # the G1 rules, the corpus, compilecheck (about 25 s)
```

The golden sample of §3.5 was a scratch build (its driver script is not committed: it patches a copy of the harness by text and depends on
the build directory's link line); its method is the paragraph "The build" above, and the harness extension of §5 makes it a committed test.
Since §7 it is `tools/gen/questgen/goldensample.py`.


---

## 7. Phase 6 step 1: the harness extension (lane C, 2026-10-05)

> Branch `lane-c/p6-harness`, based on C++ `bef516884` (the merge of PR #77). Scope: `tools/oracle`, `tools/gen`, the golden harness
> (`game-server/tests/quest_handlers_golden`) and this document. No file under `game-server/src` or `game-server/handlers` changes, no hub
> header, and no generated handler joins the tree (that is step 2).

### 7.1 The state before (measured on `bef516884`, before any change)

The measurement script runs `tools/oracle/questtrace` over the 972 files questgen transliterates (`--dry-run`, driver default rules) and
compares the hooks they override and the registrations they make with what the harness drives (`GoldenQuestTraceTest.cpp`: `callHook` and
`RegistrationTraceMatchesJavaRegister`).

**Hooks.** The corpus overrides 27 hooks; the harness drove 13 (`onDialogEvent`, `onKillEvent`, `onItemUseEvent`, `onLevelChangedEvent`,
`onQuestCompletedEvent`, `onEnterWorldEvent`, `onGetItemEvent`, `onLogOutEvent`, `onEnterZoneEvent`, `onMovieEndEvent`, `onDieEvent`,
`onQuestTimerEndEvent`, `onAtDistanceEvent`) and threw "hook X is not driven by the harness" for any other. The undriven ones:

| Hook | Files | Cases | Hooks the oracle refuses | Note |
|---|---|---|---|---|
| `onKillRankedEvent` | 26 | 26 | 0 | the 26 `reshanta` kill-ranked quests; never reached: their `register()` is refused (below), so the document's `register` is an object and `runDoc` stopped on it (§3.5's "26 kill-ranked quests") |
| `onUseSkillEvent` | 2 | 36 | 0 | talocs_hollow 11468, 21468 |
| `onDredgionRewardEvent` | 4 | 16 | 0 | 3718, 3725, 4718, 4725 |
| `onFailCraftEvent` | 2 | 14 | 0 | crafting 19038, 29038 |
| `onEnterWindStreamEvent` | 1 | 7 | 0 | inggison 11076 |
| `onInvisibleTimerEndEvent` | 4 | 6 | 2 | 20504, 25051 traced; 10506, 25050 refused (a symbolic `setQuestVar`) |
| `onKillInWorldEvent` | 6 | 6 | 4 | 18212, 28212 traced; 13745, 23745, 30051, 30151 refused (`instanceof Player`, a guard over two levels) |
| `onLeaveZoneEvent` | 1 | 5 | 0 | reshanta 24046 |
| `onNpcReachTargetEvent`, `onNpcLostTargetEvent` | 20 each | 0 | 20 each | `defaultFollowEndEvent` |
| `onBonusApplyEvent` | 11 | 0 | 11 | the `List<QuestItems>` parameter |
| `onCanAct`, `onAttackEvent`, `onPassFlyingRingEvent` | 5, 4, 2 | 0 | all | |

So §3.5's "7 hooks" are the first seven rows below `onKillRankedEvent`; the eighth was hidden behind the registration refusal.

**Registration kinds.** The trace modelled 19 kinds (the npc events `addOnQuestStart`, `addOnTalkEvent`, `addOnKillEvent`,
`addOnAttackEvent`, `addOnAtDistanceEvent`, `addOnAddAggroListEvent` and `registerQuestItem`, `registerOnLevelChanged`,
`registerOnQuestCompleted`, `registerOnEnterWorld`, `registerOnGetItem`, `registerOnLogOut`, `registerOnEnterZone`,
`registerAddOnReachTargetEvent`, `registerAddOnLostTargetEvent`, `registerOnDie`, `registerOnQuestTimerEnd`, `registerCanAct`,
`addHandlerSideQuestDrop`) and reported any other as "the registration trace does not model". Not modelled: the nine of §3.5 -
`registerOnBonusApply` (11 files), `registerOnKillInWorld` (6), `registerOnDredgionReward` (4), `registerOnInvisibleTimerEnd` (4),
`registerOnPassFlyingRings` (2), `registerOnFailCraft` (2), `registerQuestSkill` (2), `registerOnEnterWindStream` (1),
`registerOnLeaveZone` (1) - and `registerOnKillRanked` (26), which the oracle did not trace at all (`AbyssRankEnum.X` was not one of its
enum tables: the whole `register()` refused). The two `CustomConfig.ENABLE_SIMPLE_2NDCLASS` registers (1007, 2009) are hand ports in the
tree and out of scope.

**Closures in the oracle.** The extractor parsed with jast's default parser, which raises on a lambda, an anonymous class or a switch
expression; `trace_file` turned that into an `OracleError` for the **whole file**: no document, no `register`, no case, even for hooks
without a closure. Over the 972 files: **33 raised** (24 anonymous `Runnable`s, 6 lambdas, 3 switch expressions), among them the 29 files of
questgen's rule `scheduled-closure` (§2.2) and `_30211`. The oracle had no notion of `ThreadPoolManager.schedule` or of work that runs after
the hook, and the harness never advanced its clock ("the fixture's ManualClock, which nothing advances"). Documents with at least one case:
792 of 972 (18,795 cases), 623 of them for files outside the tree.

**The sample run, reproduced.** `tools/gen/questgen/goldensample.py` (§7.2) run with the harness of `bef516884` (only the two sample hooks
added and the registration trace made to go on after one handler's exception, as the G1 lane's scratch copy did) and the documents of the
unchanged oracle gives §3.5's numbers again: 620 sample files, **17,908 variants passed, 114 failed in 14 quests, every one in the 7 hooks**
(`onUseSkillEvent` 54 variants, `onDredgionRewardEvent` 24, `onFailCraftEvent` 14, `onEnterWindStreamEvent` 7, `onInvisibleTimerEndEvent`
6, `onLeaveZoneEvent` 5, `onKillInWorldEvent` 4); 247 variants not reproducible and 232 cases vacuous (unlisted); 1 run reaching
`AION_UNPORTED` (1987); **34 quests stopped** (26 kill-ranked: `[json.exception.type_error.306]` on the refused `register`; 8 with
`minlevel_permitted="99"` in quest_data.xml: 16940, 18910, 26940, 28910, 38006, 38007, 48006, 48007, "The given level is higher than possible
max"); the registration trace: 54 "does not model" findings over the nine kinds plus the refused and stopped quests. The failures match
§3.5 in count, quests and hooks; the passed count is higher than §3.5's 14,304 (its scratch run counted differently; not investigated).

### 7.2 What changed

**The oracle** (`tools/oracle/questtrace/extract.py`, its README section, 6 tests in `ScheduledTaskTest`):

- It parses with jast's `closures=True`: a closure refuses the hook that builds it, not the file, and so does a construct the parser
  refuses (a switch expression, a method reference). No transliterated file raises any more.
- **`ThreadPoolManager.getInstance().schedule(task, delay)`** with a constant delay is modelled: an effect `ThreadPoolManager.schedule` with
  the delay; the task runs after the hook has returned (Java runs it on a pool thread after the delay), in the order of the delays, on the
  locals it captured, reading the state the hook left; its effects follow the hook's and carry `task` (the index of their schedule effect).
  A task that would throw (Java's pool only logs it), a task that schedules another and a hook that throws after a schedule are refused.
- The item-use vocabulary around those tasks: `PacketSendUtility.broadcastPacket(player, packet, toSelf)` with `new SM_ITEM_USAGE_ANIMATION`,
  and `inventory.decreaseByObjectId`.
- `AbyssRankEnum` is an enum table, so `registerOnKillRanked` is traced.
- `qs.setQuestVar(v)` with a symbolic v the path bounds to 0..63 writes slot 0 and zeroes the other slots (QuestVars.java:52-58); 35 hooks
  were refused for it.

The committed documents are byte-identical (`oracle.py quest-trace check`: 0 problems). Over the 972 files: **0 raise** (33 before), 826
have a case (792), 19,727 cases (18,795), 657 of the files with a case are outside the tree (623). 13 cases in 8 quests have task effects
(2208, 4082, 10501, 10503, 10505, 11466, 20033, 24051).

**The harness** (`GoldenQuestTraceTest.cpp`, `GoldenHandlers.h`):

- `callHook` drives the 8 hooks of §7.1. A zone or int argument is the case's one argument whatever its Java name (the oracle names it after
  the Java parameter: `skillUsedId`, `itemId`, `teleportId`; beluslan/_24051's zone parameter is `name`, which the old lookup of `zoneName`
  missed: the one failure of the first extended run, a harness bug).
- The registration trace models the 10 kinds of §7.1 and fires each engine event (every AbyssRankEnum rank, every map, zone, ring, skill,
  fail-craft item and bonus type the documents name, the three engine-wide lists); `RoutingSpy` records each with what fired it. A
  `register()` the oracle refuses fails its handler and the trace goes on.
- Scheduled tasks: when a case has `ThreadPoolManager.schedule` effects, the generated handler's run advances the fixture's DeterministicExecutor
  by the longest delay after the hook (its tasks run there), and the replay replays the effects of each task at its delay. Both runs
  advance the clock alike; cases without a schedule leave it alone as before.
- Replay rows: the schedule, `broadcastPacket(SM_ITEM_USAGE_ANIMATION)` (the object ids `$playerObjectId`, `$itemObjectId` resolved),
  `decreaseByObjectId`, `defaultOnKillRankedEvent` (3 and 4 arguments), and four helper overloads the corpus outside the tree uses
  (`sendQuestRewardDialog`, `checkItemExistence` with 3 and 10 arguments, `defaultCloseDialog` with a given item, the chain helpers with 7
  or 8 pre-quests): 101 "the replay does not model" variants of §7.1 run now. `SM_ITEM_USAGE_ANIMATION` is compared byte for byte like the
  dialog, quest action and movie packets. The kill-ranked helper has the step overlays of the other step helpers (its start var and its
  finishing kill, endVar - 1).
- Fixture limits of §3.5: a case whose path reads the quester's level runs at that level in every setup (the harness used the quest's
  minimum level before; no tree case reads it), and a quester is at most level 65, the experience table's last (the 8 quests with
  `minlevel_permitted="99"` threw "The given level is higher than possible max").
- `AION_GOLDEN_SAMPLE_TABLE` / `AION_GOLDEN_EXPECTED_DIR` let the out-of-tree sample reuse these sources unchanged; the tree defines neither.

**`tools/gen/questgen/goldensample.py`** (4 tests): the sample of §3.5 as a committed tool (it was an uncommitted scratch script, §6): it
transliterates into a stage directory, traces every file with the oracle, picks the files with a case whose handler is not in the tree,
not held back and not in the table, builds the harness against the build's `aion_gs_handlers_quest_q05_tests` libraries, runs it and
summarizes by quest, variant and hook. `--harness` and `--docs` measured §7.1 with the old harness and documents; `--edit FILE OLD NEW`
builds mutants.

```
python -m tools.gen.questgen.goldensample --stage <dir outside the repo> --jobs 4      # about 25 min: 649 files, 810 documents
```

### 7.3 Before and after

The tree: `ctest -R Golden` 167 of 167 before and after (the tree's documents are unchanged, the negative control passes).

The sample (every transliterated handler outside the tree that has a case):

| | Before (§7.1) | After |
|---|---|---|
| sample files | 620 | **649** (+29, below) |
| documents run (sample and tree) | 781, 34 stopped | **810, 0 stopped** |
| variants passed | 17,908 | **19,499** |
| **variants failed** | **114 in 14 quests** (7 hooks) | **0** |
| runs compared | 89,242 | 96,841 |
| not reproducible, unlisted | 247 variants (72 quests) | 149 (53) |
| vacuous, unlisted | 232 cases (118 quests) | 328 (147) |
| reaches `AION_UNPORTED` | 1 (1987) | 1 (1987, `WarehouseService::expand`, as 2985 in the tree) |
| registration trace | 54 "does not model" findings and the stopped quests | **0 findings** |
| quests without any finding | 552 | 622 |

The +29 sample files (none left the sample) are files that had no case before: 16 raised in the oracle (12 anonymous `Runnable`s: beluslan
24051, 24052, 2620, brusthonin 4082, cygnea 10501, 10503, 10505, eltnen 1361, 1466, gelkmaros 20033, morheim 2393, talocs_hollow 11466; 2
lambdas: enshar 20506, 25023; 2 switch expressions: the_eternal_bastion 18035, 28035, whose item-use hook has a case since the parse no
longer fails the file), and 13 had their only traced hook refused for a symbolic `setQuestVar` (enshar 25022, 25030, 25031, 25032, 25050,
25062, 25070, 25073, inggison 11012, sanctum 3966-3969). The 26 kill-ranked quests were in the sample before: their documents were
written with the refused `register` object and stopped (§7.1).

All 114 variants of §7.1 pass now, and so do the 26 kill-ranked and the 8 level-99 quests. The newly driven hooks' quests and the task
cases, for instance: 3718 (dredgion) 30 variants, 11468 (use skill) 43, 19038 (fail craft) 7, 11076 (wind stream) 7, 20504 (invisible
timer) 31, 24046 (leave zone) 16, 18212 (kill in world) 13, 1702 (kill ranked) 11, 4082 (a task) 32, 24051 (a task in an enter-zone
hook) 12.

The remaining unlisted categories are the harness's known limits, not differences: 118 of the 149 unreproducible variants are COMPLETE
states with canRepeat false of quests whose template always repeats (the reason the tree lists for 1687 and 80217-80220); 15 assume a helper
result no setup produces (4937 #37 #46, 11289 #14, 11460 #20, 21460 #20, 24050 enter-world #2, 80341 nine cases assuming a false
`QuestService.startQuest`); 9 kill their target (`useQuestObject` with die, KILLS_TARGET); 7 call `useQuestItem` (not replayed: its Runnable
needs the used item and its own delay). The 328 vacuous cases are mostly IDLE_END and STEP_NOT_MET paths (§3.5); each landing chunk lists
its own in `knownVacuous` / `knownNotReproducible`, as Q03 and Q10 did.

**The extension fails wrong handlers** (`--edit` mutants, one run over 11 files): a kill-ranked end var (1702), a dredgion var slot (3718),
a fail-craft step (19038), the task's packet in an item-use task (4082) and a task delay (10501, the task no longer runs inside the case's
delay) each fail their cases; a kill rank (1703), a quest skill (11468) and a leave zone (24046) changed in `register()` each fail the
registration trace. 8 of 8 mutants caught.

### 7.4 Failures the extension uncovered

**None in a generated handler.** No variant of the 649 sample files fails, so no generator or handler bug is on the list. The one failure of
the first extended run (24051 `onEnterZoneEvent#1`) was the harness's zone argument lookup (§7.2), fixed in the harness. Still not covered:

- hooks the oracle refuses: 40 follow-helper hooks (`onNpcReachTargetEvent`/`onNpcLostTargetEvent`), the 11 `onBonusApplyEvent` (the
  reward list parameter), 4 `onKillInWorldEvent` with `instanceof Player` and a guard over two levels, `onPassFlyingRingEvent`,
  `onCanAct`, `onAttackEvent`, and the other refusal reasons of §2.4's style (crafting calls 44 hooks, teleports 32, spawns, quest timers,
  kinah, `getQuestVars().getQuestVars()`), plus 3 tasks that would throw (inggison 11031-11033);
- the 3 switch-expression files have their other hooks traced now, but not the hook with the switch (the oracle does not evaluate a switch
  expression; since the review of #79 it does, §7.6);
- `useQuestItem` (7 variants), the canRepeat paths, and the 15 assumed results above.

### 7.5 Proposed order for step 2 (landing the remaining chunks)

The 802 transliterated files without a C++ file in the tree, by Q chunk (`chunks.cmake`), with the sample's result (a "finding" is an
unlisted unreproducible or vacuous case, which the chunk must list when it lands; no chunk has a failed variant). The sample column sums
to the 649 sample files; "Waiting" less "Sample" is the files without a case and the 20 held-back files (`GOLDEN_HELD_BACK`):

| Chunk | Directories | Waiting | Sample (with a case) | No finding | Findings | Variants | Every hook traced |
|---|---|---|---|---|---|---|---|
| Q08 (**landed, §8**) | gelkmaros, enshar | 63 | 55 | 54 | 1 | 1,366 | 40 |
| Q11 | daevanion, sanctum | 72 | 50 | 41 | 9 | 1,604 | 48 |
| Q01 (**landed, §9**) | reshanta | 77 | 76 | 65 | 11 | 1,450 | 61 |
| Q02 (**landed, §10**) | inggison | 57 | 53 | 44 | 9 | 1,411 | 43 |
| Q13 | instances A-K, kaisinel_academy | 84 | 76 | 55 | 21 | 1,753 | 56 |
| Q14 | instances K-W | 73 | 63 | 55 | 8 | 1,591 | 48 |
| Q05 | eltnen, poeta, oriel (rest) | 52 | 46 | 31 | 15 | 1,092 | 35 |
| Q09 | morheim, ishalgen, pernon (rest) | 48 | 43 | 28 | 15 | 968 | 33 |
| Q07 | beshmundir, abyss_entry, silentera_canyon | 64 | 58 | 30 | 28 | 1,560 | 49 |
| Q12 | theobomos, event_quests, cygnea | 70 | 61 | 26 | 35 | 1,483 | 42 |
| Q04 | beluslan, brusthonin | 69 | 62 | 26 | 36 | 1,515 | 52 |
| Q06 | crafting (ascension: none waiting) | 52 | 5 | 5 | 0 | 94 | 3 |
| Q10 | altgard, pandaemonium (rest) | 17 | 1 | 1 | 0 | 8 | 13 |
| Q03 | verteron, heiron (rest) | 4 | 0 | - | - | - | 3 |

Proposed order: **Q08, Q01, Q02, Q14, Q13, Q11, Q05, Q09, Q07, Q12, Q04**, then Q06, pandaemonium's 4212 (Q10's one waiting file that is not held
back: 8 variants, no finding) and the held-back files. The reasoning: fewest findings
per file first, and high-level zones before the zones the gates' characters stand in. Q01, Q02, Q08 and the instance chunks Q13/Q14 are far
from the level-10 Daevas of `gs.scenario.travel` and `gs.scenario.ascension`; Q11 (sanctum) is the Elyos capital, where the ascension gate's
Daeva may stand, so its gate traffic needs the slice-2 measurement before it lands, and Q05 (eltnen) and Q09 (morheim) are the next zones
after the route's (level 20 and up, but their start npcs may appear in a gate's SM_NEARBY_QUESTS). Q06's 52 waiting files are all crafting
(its 16 ascension files are 14 in the tree and 2 questgen refuses); 47 of them have no case (the oracle refuses
`CraftSkillUpdateService.getInstance` in 42, `player.getSkillList()` in 4 and `isFullSpecialCube` in 1) and 5 have (19000, 19002, 19038,
29002, 29038): land the 47 by parity and compile check only, or after an oracle row for crafting. The held-back files (16 of Q10's 17
waiting, Q03's 4) wait for the owner's gate decisions as before (p6q-ascension-route.md §8.3).

### 7.6 The owner's review of #79 (2026-10-05)

Nine items, fixed as new commits on `lane-c/p6-harness`; §7.3 and §7.5 above carry item 8's corrected counts.

| Item | What changed |
|---|---|
| 1. task timing, leftover tasks | each run records a timeline: a snapshot (opcodes, quest states, inventory) after the hook and, for every task delay d of the case, at d - 1 and at d (the clock steps there; the replay replays the task's effects at d). Check `timeline`: a task that runs early, late or inline fails. Check `pendingTasks`: the tasks a run added to the executor and left after its last delay, against Java's run (the replay schedules nothing itself, so Java's count is 0 unless a helper starts an engine task, which then does so in both runs). A count that differs once re-runs the pair: an engine singleton's periodic task (the first item given in a process) starts in whichever run reaches it first. The executor is drained after each run |
| 2. the used item | an item-use case's item is held under one object id (`$itemObjectId`), so `decreaseByObjectId` removes it and the inventory check sees the difference |
| 3. success overlays | `checkItemExistence` (3 and 10 arguments): the item held in its exact count and in count - 1 (the 10-argument form with var 0 at its step); `sendQuestRewardDialog`: the reward npc as the target, with a REWARD state when the path has none |
| 4. goldensample's result | the exit code is kept; a missing, unreadable or partial gtest report is a crash, named with the test that was running; the other failures (SEH among them) are printed; `main()` returns 1 on any failure, finding or crash (a sample always has unlisted vacuous cases, so a sample run exits 1: read the summary) |
| 5. an unknown helper result | `helperResults` is 1, 0 or -1 (did not run, or threw): an assumed false needs a 0 |
| 6. task exceptions | swallowed in both runs as the pool does; test `ATaskExceptionIsThePoolsInBothRuns` |
| 7. `--stage` | refused when the directory holds files and no `.goldensample-stage` marker, unless `--force` |
| 8. doc counts | §7.3's +29, §7.5's sample column (it sums to 649 with Q10's 4212) and the Q06 split |
| 9. nits | the oracle parses switch expressions and switch rules (`_30211`, `_18035`, `_28035` traced; README); `PacketSendUtility.java:88-93`; the jast docstring and the harness's stale texts; each failure names its check (`[timeline] ...`) and goldensample counts all eight by name; the engine-wide registration kinds (bonus apply, invisible timer end, dredgion reward, wind stream) deduplicate as Java's lists do |

**The mutants** (`goldensample --only ... --edit`, `--allow-tree` for the tree's 2208; four runs, each file mutated once per run; the
unmutated five files as the control: 0 failed):

| Mutant | Caught by |
|---|---|
| 24051 task delay 10000 -> 1000 | `onEnterZoneEvent#1` [timeline] |
| 24051 delay -> 0 | `onEnterZoneEvent#1` [timeline] |
| 24051 the task run inline | `onEnterZoneEvent#1` [timeline] |
| 24051 without the `var == 5` guard (a task where Java schedules none) | `onEnterZoneEvent#2` [pendingTasks] 1, Java 0 |
| 2208 without `decreaseByObjectId` | `onItemUseEvent#3` [inventory] [opcodes] [timeline] |
| 3938 `checkItemExistence` nextStep 8 -> 9 | `onDialogEvent#48` with the item held [packets] [questStates] |
| 3938 checkOkId 10000 -> 10002 | `onDialogEvent#48` with the item held [packets] |
| 3938 count 1 -> 2 | `onDialogEvent#48` with the item held [packets] [opcodes] [questStates] [inventory] |
| 18405 reward npc + 1 | `onDialogEvent#5` with the reward npc as the target, `#16` |
| 28405 reward npc + 1 | `onDialogEvent#5` with the reward npc as the target, `#16` |
| 18405 reward index 0 -> 1 | `onDialogEvent#5` with the reward npc as the target [packets] [questStates] |

11 of 11 caught. The harness's own mutants (`TheTaskTimelineFailsATaskAtTheWrongTimeOrWhereJavaHasNone`): a shorter, zero, longer and
inline task fail `timeline`, an extra task fails `pendingTasks`, the right one passes.

**The sample again** (the same command, the stricter harness):

| | Step 1 (§7.3) | After the review |
|---|---|---|
| sample files | 649 | 650 (+`_30211`, traced now) |
| documents run | 810 | 811 |
| variants passed | 19,499 | **19,571** (+72: `_30211` 22, `_18035` and `_28035` 25 each) |
| variants failed | 0 | **0** |
| runs compared | 96,841 | 97,350 (the success overlays) |
| not reproducible, unlisted | 149 (53 quests) | 149 (53) |
| vacuous, unlisted | 328 (147) | 324 (146): 2477 #19, 4937 #41, 18405 #5 and 28405 #5 run with the new overlays |
| other failures (SEH), crashes | not reported | 0, none |

No new failure: the stricter checks found no generator bug, harness limit or Java difference in the 650 files. The ctest of the tree:
`ctest -R Golden` 192 of 192.

## 8. Phase 6 step 2: chunk Q08 landed (lane C, 2026-10-05)

Branch `lane-c/p6-q08`, stacked on `lane-c/p6-harness` (§7). The first chunk of §7.5's order, landed the slice-2 way (p6q-ascension-route.md
§8); the record is docs/deviations/Q08.md.

- **In the tree:** the 63 files questgen transliterates of `gelkmaros/` (44) and `enshar/` (19), its output unedited, in
  `aion_gs_handlers_quest_q08`; the drift test's `Q08` table pins them. `gelkmaros/_20034RescuetheReians` is refused by questgen
  (`DataManager.QUEST_DATA.getQuestById(questId).getName()`: the C++ `QuestTemplate` declares no `getName()`), so it is not in the tree, and
  the landed 20035 cannot start until it is (its chain helpers name 20034).
- **Golden traces:** `SLICE_Q08`, 63 documents, 1,344 cases; the in-tree harness compiles the 63 in (`GoldenQ08Handlers.cpp`): **1,366
  variants pass, 0 fail**, 7,685 runs compared; listed: 21460 onDialogEvent#20 not reproducible, #21 vacuous; 8 quests with every hook
  refused by the oracle (`ORACLE_REFUSES_EVERY_HOOK`). The registration trace passes for the 63. `ctest -R Golden`: 230 of 230.
- **Parity** 63 pairs, 0 mismatches (the whole handler tree: 241 pairs, 0); **compile check** 63 clean, 0 warnings, 0 errors, regscan 0
  errors; the full Debug build has no new warning.
- **Gate impact:** none expected (Q08.md, "Gate impact": every quest needs level 50+, no gate npc or item is registered). **Gates:** `ctest
  -C Debug -j 2 -L "scenario|smoke|geo|m4" -E m5a_stress` under the test-database lock (`gate_lock.py lane-C`), the full Debug build of this
  branch: **60 of 60 passed** (2,854 s; 16 disabled entries not run), every gate server logged "Loaded 4425 quest handlers" (63 more than
  before the chunk); no ERROR line beyond the known ones (gs.scenario.ascension's `onDie()` exceptions of the unported
  `AbyssPointsService::addAp`, the scenario self-tests' deliberate start-up failures). No gate expectation was changed.
- **Not held back:** none. **Generator changes:** none. **Java bugs found:** none.
- **Tool tests:** `tools.oracle` 552, `tools.gen` 430 (the drift test with the 63), `tools.parity` 36: all pass. Static checks:
  `lint_concurrency --werror` 0 errors, `chunks.py check` 0 problems, `census --self-check` 0 failures.
- **Follow-up (2026-10-05):** `gelkmaros/_20034RescuetheReians` landed. The header gap reported above was not one (`QuestTemplate::getName()`
  is declared in the generated `QuestTemplate.xml.inc`); the gap was questgen's: API row B39 reads `DataManager.QUEST_DATA` through its
  `HolderRef`. 973 of 1,035 transliterated, the other files byte-identical; 20034's 2 golden cases pass, the registration trace too; the
  harness 231 of 231. docs/deviations/Q08.md, "Follow-up"; header request p6q08-1 (approved, not needed).

## 9. Phase 6 step 2: chunk Q01 landed (lane C, 2026-10-05)

Branch `lane-c/p6-q01`, stacked on `lane-c/p6-q08` (§8 and its 20034 follow-up). The second chunk of §7.5's order; the record is
docs/deviations/Q01.md.

- **In the tree:** 81 of the 82 reshanta handlers, questgen's output unedited, in `aion_gs_handlers_quest_reshanta`; the drift test's `Q01`
  table pins them. `_2759TenaciousGuardian` (a written `List<Integer>` field: per-player state shared in the singleton, Java's race) is a
  hand port since the owner's decision of 2026-10-05: a correction of the Java code, each player's kill record in the quest's var slot 1
  (docs/deviations/Q01.md; hand-written cases in `ReshantaHandPortsTest.cpp`).
- **Generator:** API rows B40 (`broadcastPacketAndReceive`), B41 (`GameTimeService` / `GameTime.getHour`), B42 (`DataManager.SPAWNS_DATA`,
  the spawn lookup), a `cppdecl` fix (an out-of-line nested class `class A::B` replaced A in the index: dataholders/SpawnsData.h) and an emitter
  rule (a nullable value result kept in a local is a `std::optional` local, its calls through `.value()`). **979 of 1,035** transliterated;
  the other 973 files byte-identical; two event quests of Q12 (50008, 51008) are among the new ones and land with Q12.
- **Golden traces:** `SLICE_Q01`, 81 documents, 1,494 cases: **1,493 variants pass, 0 fail**, 7,143 runs compared; listed: 10 not
  reproducible (canRepeat false on templates that always repeat: 1718, 2718, 3718, 4718), 11 vacuous (IDLE_END in 1799, 1845, 2721, 2724,
  2727, 2767; 24040's reward page); 2798 with every hook refused. The 26 kill-ranked quests are traced and pass. The registration trace
  passes for the 81. `ctest -R GoldenQuest`: 312 of 312.
- **Parity** 81 pairs, 0 mismatches (the handler tree: 323 pairs, 0); **compile check** 979 clean, 0 warnings, regscan 0 errors; the full
  Debug build has no new warning.
- **Gate impact:** none expected (Q01.md: every quest needs level 25+, the two enter-world starts only in Reshanta, the talk registrations
  at Pernos and 203550 answer false without their quest state). **Gates:** every gs.scenario.* gate, one at a time (`ctest -j 1 -R ^gs\.scenario\.`) under the test-database lock (`gate_lock.py lane-C`), on the
  full Debug build of this branch: **15 of 15 passed** (4,461 s; m5a, m5b, m5b2, m5b3 and their geo variants, m5c, travel, ascension 857 s,
  m5d, m5d_geo, m5e, m5e_geo); every gate server logged "Loaded 4507 quest handlers" (Q08 and its follow-up 64, Q01 81 more than before
  the two chunks); no "QE: exception" line. gs.scenario.ascension's routes talk to Pernos and 203550 and m5e's to Pernos: the talk
  registrations of 14044, 24044 and 24046 change nothing there, as reasoned above. No gate expectation was changed.
- **Not held back:** none. **Java bugs found:** none.

## 10. Phase 6 step 2: chunk Q02 landed (lane C, 2026-10-05)

Branch `lane-c/p6-q02`, stacked on `lane-c/p6-q01` (§9). The third chunk of §7.5's order; the record is docs/deviations/Q02.md.

- **In the tree:** all 59 inggison handlers, questgen's output unedited, in `aion_gs_handlers_quest_q02`; the drift test's `Q02` table pins
  them.
- **Generator:** API rows B43 (`WorldMapInstance.getInstanceHandler`, `InstanceHandler.handleUseItemFinish`) and B44
  (`ItemTemplate.getUseArea`) for 10034 and 11118: **981 of 1,035** transliterated, the other 979 files byte-identical.
- **Golden traces:** `SLICE_Q02`, 59 documents, 1,407 cases: **1,417 variants pass, 0 fail**, 6,533 runs compared; listed: 7 not
  reproducible, 11 vacuous; 5 quests with every hook refused. The registration trace passes for the 59. `ctest -R GoldenQuest`: 371 of 371.
- **Java bug kept:** 11001 and 11008 name themselves as the pre-quest of their level hook, which therefore never starts them (Q02.md; both
  start at their npc). Proposed correction for the owner.
- **Parity** 59 pairs, 0 mismatches (the handler tree: 382 pairs, 0); **compile check** 981 clean, 0 warnings, regscan 0 errors; the full
  Debug build has no new warning.
- **Gate impact:** none expected (Q02.md: every quest needs level 50+; 11116's talk registrations at 203784 and 203785, which m5c's economy
  oracle names, answer false without its quest state). **Gates:** WIP - the gs.scenario.* run under the lock was in progress at the handoff (Q02.md).
- **Not held back:** none. **Not in the tree:** none.
