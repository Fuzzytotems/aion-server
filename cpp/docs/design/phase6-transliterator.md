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
| Golden harness: drive the 7 hooks of §3.5 and model the 9 registration kinds; a fixture level above the cap; `AbyssRankEnum` in the oracle (26 kill-ranked quests); keep `knownVacuous` / `knownNotReproducible` per chunk as the slices did | 1-2 days | `tests/quest_handlers_golden` (Q05's), `tools/oracle` (the scenario lane's area: coordinate) |
| Oracle closure support, so the 29 B31 files get golden cases (they compile, are at parity and match six hand ports) | about 1 day | the oracle's owner |
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
