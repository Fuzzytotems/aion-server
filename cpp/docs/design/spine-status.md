# Spine status (S0a-S0c)

> The serial spine of [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.5. Where the result departs from the designs, each design
> has an "S0a implementation notes" section (handlers-and-porting-plan.md, static-data.md) or §23 (runtime-architecture.md); Java-visible
> differences are in [DEVIATIONS.md](../DEVIATIONS.md) under game-server.

## S0a: mechanical skeleton

> Implemented 2026-09-14 by a workflow of 3 parallel agents (kernel, staticdata, skeleton), 1 integrator agent (manifest), 2 reviewers (build
> and code lenses) and 3 fixers (kernel, generators, manifest).

The whole game server builds from the chunk manifest, `aion_game_server` links every chunk library and the registries, and a real run against
MariaDB stops at the first unported function (`DataManager::getInstance`) after an orderly shutdown. The reviewers found 15 issues (6 medium,
9 low): 13 were fixed with tests (one was a duplicate), geo not being a leaf was kept and documented, and the `NpcEquippedGear` ownership
contradiction is deferred until the static data runtime has a `Ref<T>` overload of `BindContext::replaceSingle`.

### Delivered components

Test counts are the last reported runs (after the fixes where the suite was re-run). Paths below `runtime/` are relative to
`game-server/src/aion/gameserver/`.

| Component | Location | Verification |
|---|---|---|
| `TaskKind::CALLBACK_` (was `CALLBACK`, clashed with the `<windows.h>` macro) | `game-server/src/aion/gameserver/runtime/base/TaskInfo.h`, `sched/PinnedCallback.h` | `WindowsHeadersFirstTest` (`<windows.h>`, then every public kernel header) |
| `java.lang.ArithmeticException` | `commons/src/aion/commons/utils/Exception.h`; re-exported by `runtime/base/Exceptions.h`, alias in `geoEngine/math/Matrix4f.h` | `ExceptionTest.JavaTypeHierarchy`; commons core 152, geomath 77 |
| `AION_UNPORTED` in the kernel | `runtime/base/Unported.h/.cpp` (`aion_gs_runtime_base`, namespace `aion::gameserver::runtime`; wave-1 names re-exported into `aion::gameserver::handlers`) | `aion_gs_runtime_base_tests` 15 (7 `UnportedTest`, incl. `WaveOneHandlerNamesStayAvailable`); `aion_gs_handler_registry_tests` 5 |
| Kernel start/shutdown | `runtime/services/RuntimeLifecycle.h/.cpp` (`aion_gs_runtime_services`; runtime-architecture.md §23) | `RuntimeLifecycleTest` 10 (deterministic backend and real pools, rollback, rules); services 77 |
| `Pin` counts pinned parts | `runtime/sched/Pin.h` (`Pin::part(i)`) | `PinPartTest` 3; sched 83 (2 skipped) |
| `DeterministicExecutor` seeding | `runtime/sched/DeterministicExecutor.cpp` (`Rnd::seedCurrentThreadForTests`) | sched tests |
| Core enums through xmlgen | `generated/aion/gameserver/<pkg>/<Enum>.h`; `tools/xmlgen/xmlgen.toml` (`core_enums`, `[hand_written_enums]`) | `tools.xmlgen` (16 new tests in `tests/test_s0a.py`); review script compared all 256 enums with their Java sources (names, ordinals, underlying types) |
| Behaviour class shells | 543 `game-server/src/**/X.h`, 111 `.cpp`, `model/items/NpcEquippedGear.h/.cpp`; hierarchy roots derive `runtime::StaticTemplate` | `tools/xmlgen/tests/test_real_tree.py`; review script checked all top-level shells against `xmlmodel.json` and Java; `GeneratedSliceTest` static_asserts; built `/W4 /WX` |
| xmlgen fixes | `tools/xmlgen/emit.py` (`c.ignoreAttribute()`), `xmlgen.toml` (`NpcEquippedGearAdapter` uses `c.replaceSingle`), `xmlgen.py check --src` (conflict checks) | `tests/test_s0a.py`; strict load of all static data without hooks: V3 totals equal |
| Forward headers | 252 `game-server/src/aion/gameserver/<pkg>/fwd.h` | `test_committed_forward_headers_are_current` (drift), `test_compile_each_forward_header` (`/W4 /WX`); review: 968 declarations with C++ definitions, 0 mismatches |
| Skeleton drafts | `tools/gen/skeleton.py`: default `--unported-header` `aion/gameserver/runtime/base/Unported.h`; follow hand-written definitions; never define generated enums | `tools.gen` (`test_hand_written_definitions_win`, `GeneratorAwareTest`, `test_generated_enums_are_generator_owned`); `KERNEL_FIRST` workaround removed |
| Handler preludes | `game-server/handlers/aion/gameserver/handlers/{ai/AiPrelude.h, instance/InstancePrelude.h, quest/QuestPrelude.h, zone/ZonePrelude.h, admincommands/AdminCommandsPrelude.h, playercommands/PlayerCommandsPrelude.h, consolecommands/ConsoleCommandsPrelude.h}`; `CommandPrelude.h` (PCH of the command library) | `tools/gen/tests/test_handler_preludes.py`; header check |
| Chunk manifest | `game-server/chunks.cmake`; `game-server/cmake/AionChunks.cmake` (syntax, checks, targets, unity groups, header check), `cmake/CheckChunks.cmake` (script mode) | configure-time ownership check; `gs.chunks.consistency`; `tools/porting/tests/test_chunks.py` 26 |
| Ownership tool | `tools/porting/chunks.py` (stdlib) | `test_chunks.py` (parser, globs, fixture rules, CMake parity on a fixture and the real tree, a configured fixture project) |
| Static data library | `aion_gs_staticdata` (chunk T2-gen, all of `generated/`, 89 binder TUs); shells in their package chunk libraries | full Debug build, 0 warnings |
| Registries | `aion_gs_add_registries(aion_gs_registry HANDLERS_ROOT game-server/handlers ...)`; `_empty` tables written at configure time (`game-server/tools/regscan/AionRegscan.cmake`) | `gs.registry.empty_tables`, `EmptyRegistryTest`; regscan QuestPrelude directive fixtures |
| Executable | `game-server/src/main.cpp` (MAIN of P5-14), `aion_game_server` | real run: exit 1 at `DataManager::getInstance` after "Runtime shut down: ..."; `gs.smoke.startup` |
| Header check | `aion_gs_header_check` (`game-server/CMakeLists.txt`) | 519 TUs, `/W4 /WX`, in ALL |
| Test infrastructure | `cmake/AionCompilerOptions.cmake` (`aion_add_tests`: `<build>/test_work/<target>`, `EXTRA_DIRS`); labels `realdata`, `smoke` | full ctest (below) |
| Lint fix | `tools/porting/lint_concurrency.py` (opaque `enum class X : std::uint8_t;` is no namespace-scope variable) | `test_lint_rules`; `lint_concurrency --werror game-server/src`: 1,147 files, 0 findings |

### Fixed decisions S0a used

1. **Unported location.** `AION_UNPORTED` moved from `aion/gameserver/handlers/Unported.h/.cpp` to `aion/gameserver/runtime/base/Unported.h/.cpp`
   (`aion_gs_runtime_base`), so core code need not link the handler registry. The macro, `UnportedException`, the trace API and format stay;
   the namespace is `aion::gameserver::runtime` with using-declarations of the wave-1 names in `aion::gameserver::handlers`.
2. **ArithmeticException** lives in commons `aion/commons/utils/Exception.h` next to the other Java exception types (geomath depends on commons
   core only); the runtime and `Matrix4f.h` use that type.
3. **Core enums through xmlgen.** Every enum of `game-server/src` is emitted by xmlgen's enum emitter with the shape and location rules of the
   JAXB enums. Handler-private enums stay with the handlers (phase 6). Behaviour goes into hand-written companion headers.
4. **Shells.** `xmlgen.py scaffold --all` writes the hand-owned shells of all behaviour classes into `src/` once, never overwriting a file;
   hooks and annotated setters are `AION_UNPORTED` stubs.
5. **fwd.h.** `skeleton.py --fwd` writes `src/aion/gameserver/<pkg>/fwd.h` for every core package; the files are committed with a drift check.
6. **Manifest.** `cpp/game-server/chunks.cmake` (§2.2). Existing library names stay where a chunk equals an existing library
   (`aion_gs_runtime_*`, `aion_gs_configs`, `aion_gs_geomath`, `aion_gs_network_crypt`, `aion_gs_xml`, `aion_gs_handler_registry`); other targets
   use the names of the design tables.

### Key numbers

| Item | Value |
|---|---|
| Manifest | 65 chunks, 74 parts, 70 static libraries (stage 2 had 73 parts; the fixer added the P4-02b `LEASE` part for the kernel test support) |
| Ownership | 4,208 C++ files below `src/`, `handlers/`, `generated/`; 111 test files; 4,039 Java files; each owned or claimed exactly once |
| Generated tree | 3,055 files owned by xmlgen (3,061 in `generated/` with the `fieldmap.py` outputs); 256 enum headers (115 JAXB, 141 core); 744 classes (195 data-only, 549 behaviour); 89 binder TUs |
| Shells | 543 headers + 111 `.cpp` + `NpcEquippedGear.h/.cpp`; 240 hierarchy roots derive `StaticTemplate`; 432 classes header-only |
| Forward headers | 252 `fwd.h`: 1,881 class, 199 struct, 210 enum, 36 template declarations |
| Enum companions | 153 enums need hand-written companions (54 JAXB + 99 core; 4 with constant-specific bodies); 40 nested core enums need a hand-written alias |
| Header check | 519 TUs: 252 `fwd.h`, 256 enum headers, 8 preludes, `HandlerRegistry.h`, `Unported.h`, `RuntimeLifecycle.h`; about 7 s wall |
| Configure | fresh 13.3 s wall (configuring 9.6 s, generating 3.5 s); reconfigure about 8 s; chunk check alone about 0.6 s (script mode) |
| Build (Debug, `--parallel 12`) | from scratch 90 s wall, 134 projects, 1,525 TUs, 0 warnings; no-op build about 6 s. Static data part: 201 TUs, 322 CPU-s; `dataholders.bind.cpp` about 10 s incremental. `Unported.h` is included by 145 TUs; touching one shell header recompiles 6 TUs, one `fwd.h` only its header-check TU |
| Static data load (proof) | all 92 imports, strict, `runHooks=false`, no-op `AION_UNPORTED`: 11.4 s Debug, V3 totals equal |
| Real server run | 0.79 s wall to the unported `DataManager`, exit code 1, orderly shutdown |
| Full ctest, fix-kernel run | 1,309 registered, 5 disabled, 1,304 run: 100% passed (47 skipped), 192.8 s |
| Full ctest, fix-generators run | 1,310 registered, 5 disabled, 1,305 run: 100% passed (95 skipped: DB, realdata, stress); `tools.gen` 205 s, `tools.xmlgen` 22 s |
| Full ctest, fix-manifest run (`--parallel 8`) | 1,305 run: 1,283 passed (73 skipped), 22 failed on the first run (21 `DatabaseIntegrationTest` without `AION_TEST_DATABASE_USER`, `MonitorTest.EventualFairnessAgainstABargingThread` under load), all 22 passed on the rerun; `gs.smoke.startup` passed with `AION_TEST_GS_DATABASE_URL`; `test_chunks.py` 26 |
| Separate checks | `lint_concurrency --werror game-server/src`: 1,147 files, 0 findings; `skeleton.py --fwd --check`: 252 files, 0 problems; `xmlgen.py check`: 3,055 files up to date, no `src/` conflicts |

### Using the new tools

**Ownership** (from `cpp/`, Python 3.12; paths may be C++ or Java):
- `python tools/porting/chunks.py owner <path>...` names the owning chunk part (and leases).
- `python tools/porting/chunks.py files <chunk>` / `java <chunk>` lists a chunk's C++ or Java files; `list` lists all parts.
- `python tools/porting/chunks.py check` runs the configure-time checks without a build.
- `python tools/porting/chunks.py check-ownership <chunk> <base>..<head>` lists changed files the chunk may not change.
- `python tools/porting/chunks.py verify-json <build>/game-server/chunks.json` compares with CMake's result.
- Without a build directory: `cmake -DJSON=out.json -P game-server/cmake/CheckChunks.cmake`.
- `<build>/game-server/chunks.json` lists every part with its files, Java files, resolved `tests` directory and `testSupport` directories,
  plus the totals (`fileCount`, `javaFileCount`, `testFileCount`).
- A new chunk test directory is `game-server/tests/<TESTS>`; it becomes `<target>_tests` automatically. Only the integrator edits
  `chunks.cmake`; a new file fails configure until it is assigned.

**Build and header check:**
- `cmake --preset msvc -B build/<dir>`, then `cmake --build build/<dir> --config Debug --parallel 12`. Targets: `aion_game_server`,
  `aion_gs_header_check` (in ALL; `--target aion_gs_header_check` alone), `aion_gs_core` (all core libraries), `aion_gs_core_deps` (leaf
  libraries, commons, build options), `aion_gs_handlers`, `aion_gs_registry`/`aion_gs_registry_empty`. Configure prints the header check
  counts ("aion_gs_header_check: N forward headers, ...").
- Build directories configured before S0a need a reconfigure (the old library helper, the include bridge, `TESTS_DIR` and the empty registry
  source location are gone; 395 generated headers and 240 shells changed).

**Tests:**
- `ctest -C Debug` in the build directory. `-L realdata` runs the tests that read the Java tree (real-data GoogleTest cases,
  `aion_gs_regscan.cli.java_tree`, `gs.chunks.consistency`, `gs.smoke.startup`, the `tools.*` suites); `-LE realdata` is the fast run
  without the Java checkout (it also skips the tool suites' fixture tests). `-L smoke` runs `gs.smoke.startup`.
- `gs.smoke.startup` runs only with `AION_TEST_GS_DATABASE_URL` set, e.g.
  `jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8` (optional `AION_TEST_GS_DATABASE_USER`/`AION_TEST_GS_DATABASE_PASSWORD`,
  default root without password); the URL is passed as `-Ddatabase.*` overrides, so the test never touches `aion_gs`. Without it the test
  is reported as skipped.
- Commons DB integration tests: `AION_TEST_DATABASE_URL=jdbc:mysql://127.0.0.1:3306/aion_cpp_test` and `AION_TEST_DATABASE_USER=root`.
  Login server DB tests: `AION_TEST_LS_DATABASE_URL`.

**Running the server:** working directory `D:\aion-server\game-server`; `aion_game_server.exe [-Dkey=value ...]` (overrides layered over
`mygs.properties`). It exits with 1 at the unported `DataManager`.

**Regenerating:** `python tools/gen/skeleton.py --fwd --out game-server/src` (after adding a Java class, after xmlgen regenerates, or when a
hand-written header changes a class key; `--check` is the drift check); `python tools/xmlgen/xmlgen.py generate | check`;
`python tools/xmlgen/xmlgen.py scaffold --all` (missing shells only).

### Open issues

#### S0b: hub headers

- The `DataManager` stub has only a static `getInstance()` and a private constructor; S0b replaces it with the `HolderRef` layout and `init()`.
- No core PCH (`CorePch.h`) yet; `aion_gs_chunk` supports `PCH`, so S0b or a phase-4 chunk can add one.
- The preludes hold no using-declarations yet. Each goes into the namespace of its prelude's own package, and a prelude must not re-export a
  name that a Java type of its category declares (`test_handler_preludes`).
- Drafts omit `override` where the hand-written C++ base does not declare the method yet (comment "@Override: the hand-written C++ base does
  not declare it (yet)"); hub headers should declare those methods.
- The wave-1 S0b items stay open (hub class shapes that `HandlerRegistry.h` forward-declares, `AITemplate<T>::OwnerType`, the cycle review,
  per-run services, draft TODOs): [wave1-status.md](wave1-status.md).

#### S0c: declaration headers

- Rerun `skeleton.py --fwd --out game-server/src` after adding classes; the drift test fails otherwise. Generated nested enums are not
  forward-declared; spell them `Outer_Inner` with their generated header.
- Headers at the mirrored path of a Java class are already owned by the chunk that claims the Java file; a C++-only file needs a manifest entry
  first (P5-14's 16 direct services, for example, are an explicit list).
- `tools.gen` takes 157-205 s and dominates ctest: `RealTreeForwardHeadersTest.test_compile_with_existing_headers` builds one TU per existing
  header (544 shells included, about 2 minutes). Merge TUs if the run time becomes a problem.

#### P4 chunks

- Hand-written companion headers for 153 enums (54 JAXB + 99 core; `AutoGroupType`, `Rates`, `ItemInfoBlob.ItemBlobType` and
  `SM_CUSTOM_PACKET.PacketElementType` have constant-specific bodies) and the `using Inner = Outer_Inner;` aliases of 40 nested core enums in
  hand-written outer classes (`LegionHistoryAction.Type` stays `LegionHistoryAction_Type`).
- All 111 afterUnmarshal hooks, the annotated setters (`ItemTemplate`/`NpcTemplate::setXmlUid`, `ZoneTemplate::setXmlName`) and
  `NpcEquippedGear::init` are `AION_UNPORTED`. The setters and `init` run during binding even with `runHooks=false`, so no real static data
  load passes until P4-07/08/09/13 port them.
- `NpcEquippedGear` lifetime: the proposed decision (RefCounted, `Ref<NpcEquippedGear>` in `NpcTemplate`, static-data.md S0a notes) needs
  `template <class T> void BindContext::replaceSingle(runtime::Ref<T>&, runtime::Ref<T>, pugi::xml_node)` in
  `dataholders/loadingutils/BindContext.h` (same rules as the `unique_ptr` overload). Then `xmlgen.toml [adapters]`, the cppmodel return kind
  and the adapter target shell switch to `Ref`, and xmlgen regenerates.
- 432 shells are header-only; a porter adds the `.cpp` with the first ported method. Generators never touch hand-edited shells; a new root
  class needs `scaffold --all`.
- The 7 test shells in `tests/xml/generated` duplicate `src/` shells; `aion_gs_xml_tests` must not link `aion_gs_staticdata` until the slice
  test uses the real classes.
- `SM_SYSTEM_MESSAGE.gen0..7.cpp` stay header-only (`COMPILE_WHEN_EXISTS`) until P4-06 adds `SM_SYSTEM_MESSAGE.h`; the `tests/network_crypt`
  copies remain until then.
- P4-04 geo is not a leaf; it may add `DEPENDS` once its uses of DataManager, ZoneService, SiegeService and others are callbacks.
- `main.cpp` passes no `usedIds` sources (P4-14 DAOs) and no cleaner action (RespawnService) to `RuntimeLifecycle`.
- `tests/support/` (FakeGameClient, P4-15) needs an owner through `TEST_SUPPORT` when it is created.

#### Kernel

- `RuntimeLifecycle` ran only in Debug (not under msvc-asan or RelWithDebInfo).
- The Pin workaround comment in `runtime/lifetime/Parts.h` is outdated (Pin now retains the part).
- `UnportedTest` hard-codes the source lines of its `AION_UNPORTED` sites (20/24).
- `MonitorTest.EventualFairnessAgainstABargingThread` failed once under `ctest --parallel 8` and passed alone.
- Watchdog `slowTaskExemptKinds`/`stallExemptKinds` have no config keys (`RuntimeConfig::watchdogConfig`, `aion_gs_configs`).
- Unchanged from [runtime-kernel-status.md](runtime-kernel-status.md): minidump helper process, other-thread stacks, LOGGING leaf rank, CHM
  same-map cross-stripe nesting (lint L20) and per-creature CHM memory, StressHarness workaround and DISABLED reproducer, SyncCosts/TaskScope
  benches.

#### Later

- I1-I6 membership is a directory-name heuristic, not the npc-id mapping script; re-check it before phase 6 (only `chunks.cmake` changes).
- msvc-asan was configured with the manifest but not built; login server DB tests stay skipped without `AION_TEST_LS_DATABASE_URL`.
- `gs.smoke.startup` writes its logs into the Java tree (`game-server/log`, git-ignored); isolating them needs a log-directory override in
  Logging/Config.
- The `realdata` label covers whole tool suites; a precise split needs separate real-tree test entries in each tool.
- Handler unity groups assume one `.cpp` per Java file at the mirrored path; a C++-only helper sorts into the group of the preceding Java
  name, and a directory without Java files is one unbounded group.
- `EventService` (P5-12b) or `GameServer::main` (P5-14) must keep the `-D` overrides when EventService registers its event-properties
  provider, otherwise they stop applying after the first event reload.
- CMake's generate step takes about 3.5 s because `aion_gs_staticdata` lists about 3,000 generated headers for the IDE; they can be dropped from
  the source list without affecting the ownership check.
- `gs.lint.concurrency` covers only `game-server/src`, not `handlers/` (M6 expects handler chunks to be lint-clean).
- regscan (optional): leave `*Prelude.h` files out of the per-category file counts of the registry report.
- Stale comments: `game-server/CMakeLists.txt` still gives the header check as 516 TUs (519 with 8 preludes); the C1 comment in `chunks.cmake`
  could read "(+ the command PCH and preludes)".

## S0b: hub headers

## S0c: declaration headers
