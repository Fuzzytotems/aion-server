# Spine status (S0a-S0c)

> The serial spine of [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.5. Where the result departs from the designs, each design
> has "S0a implementation notes" and "S0b and S0c implementation notes" sections (handlers-and-porting-plan.md, static-data.md) or §23 and §24
> (runtime-architecture.md); the header rules are [hub-headers.md](hub-headers.md); Java-visible differences are in
> [DEVIATIONS.md](../DEVIATIONS.md) under game-server. Sections: S0a, S0b, S0c, Freeze (gates, ledger, exceptions, header requests, open
> issues by chunk).

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
  **Resolved (S0b):** 92 `HolderRef`/`MutableHolderRef` members and a private `init()` (static-data.md §3.3); `getInstance()` stays
  `AION_UNPORTED` on purpose (the smoke test matches the site).
- No core PCH (`CorePch.h`) yet; `aion_gs_chunk` supports `PCH`, so S0b or a phase-4 chunk can add one. Still open (the handler preludes are
  PCHs; the review measured `Creature.h` in 186 TUs and `EnumTraits.h` in 854).
- The preludes hold no using-declarations yet. Each goes into the namespace of its prelude's own package, and a prelude must not re-export a
  name that a Java type of its category declares (`test_handler_preludes`). **Resolved (S0b):** generated from the Java imports
  (handlers-and-porting-plan.md §1.2).
- Drafts omit `override` where the hand-written C++ base does not declare the method yet (comment "@Override: the hand-written C++ base does
  not declare it (yet)"); hub headers should declare those methods. **Resolved (S0b/S0c):** the hubs declare every Java method, and drafts
  detect overrides by erased parameter types.
- The wave-1 S0b items stay open (hub class shapes that `HandlerRegistry.h` forward-declares, `AITemplate<T>::OwnerType`, the cycle review,
  per-run services, draft TODOs): [wave1-status.md](wave1-status.md). **Resolved (S0b):** see wave1-status.md.

#### S0c: declaration headers

- Rerun `skeleton.py --fwd --out game-server/src` after adding classes; the drift test fails otherwise. Generated nested enums are not
  forward-declared; spell them `Outer_Inner` with their generated header. (Still the rule; S0c needed no `fwd.h` change.)
- Headers at the mirrored path of a Java class are already owned by the chunk that claims the Java file; a C++-only file needs a manifest entry
  first (P5-14's 16 direct services, for example, are an explicit list). (Applied in S0c: every new file resolves to one chunk.)
- `tools.gen` takes 157-205 s and dominates ctest: `RealTreeForwardHeadersTest.test_compile_with_existing_headers` builds one TU per existing
  header (544 shells included, about 2 minutes). Merge TUs if the run time becomes a problem. Still open: 278 s in the freeze run, now with
  about 1,650 headers.

#### P4 chunks

- Hand-written companion headers for 153 enums (54 JAXB + 99 core; `AutoGroupType`, `Rates`, `ItemInfoBlob.ItemBlobType` and
  `SM_CUSTOM_PACKET.PacketElementType` have constant-specific bodies) and the `using Inner = Outer_Inner;` aliases of 40 nested core enums in
  hand-written outer classes (`LegionHistoryAction.Type` stays `LegionHistoryAction_Type`).
- All 111 afterUnmarshal hooks, the annotated setters (`ItemTemplate`/`NpcTemplate::setXmlUid`, `ZoneTemplate::setXmlName`) and
  `NpcEquippedGear::init` are `AION_UNPORTED`. The setters and `init` run during binding even with `runHooks=false`, so no real static data
  load passes until P4-07/08/09/13 port them. Still open, except the `QuestsData` and `NpcSkillData` hooks (ported in S0c), the P4-07a
  hooks and setters (item, npc, spawns, world, zone, stats, pet; wave 3a-1) and the `SkillData`, `ItemSetData`, `WorldMapsData` hooks (3a-2
  pre-stage).
- `NpcEquippedGear` lifetime: the proposed decision (RefCounted, `Ref<NpcEquippedGear>` in `NpcTemplate`, static-data.md S0a notes) needs
  `template <class T> void BindContext::replaceSingle(runtime::Ref<T>&, runtime::Ref<T>, pugi::xml_node)` in
  `dataholders/loadingutils/BindContext.h` (same rules as the `unique_ptr` overload). Then `xmlgen.toml [adapters]`, the cppmodel return kind
  and the adapter target shell switch to `Ref`, and xmlgen regenerates. **Resolved (S0c):** implemented as proposed (static-data.md S0b/S0c
  notes); S0b's interim `shared_ptr<const NpcEquippedGear>` in `Npc.h` is replaced by `Field<Ref<NpcEquippedGear>>`.
- 432 shells are header-only; a porter adds the `.cpp` with the first ported method. Generators never touch hand-edited shells; a new root
  class needs `scaffold --all`. (S0c added 109 effect shell `.cpp` files for the `applyEffect` overrides.)
- The 7 test shells in `tests/xml/generated` duplicate `src/` shells; `aion_gs_xml_tests` must not link `aion_gs_staticdata` until the slice
  test uses the real classes.
- `SM_SYSTEM_MESSAGE.gen0..7.cpp` stay header-only (`COMPILE_WHEN_EXISTS`) until P4-06 adds `SM_SYSTEM_MESSAGE.h`; the `tests/network_crypt`
  copies remain until then. **Resolved (S0c):** `SM_SYSTEM_MESSAGE.h` exists, the generated definitions compile in `aion_gs_sysmsg`, the stub
  and definition copies are removed and `GeneratedSysMsgTest` is in `tests/sysmsg`.
- P4-04 geo is not a leaf; it may add `DEPENDS` once its uses of DataManager, ZoneService, SiegeService and others are callbacks.
- `main.cpp` passes no `usedIds` sources (P4-14 DAOs) and no cleaner action (RespawnService) to `RuntimeLifecycle`.
- `tests/support/` (FakeGameClient, P4-15) needs an owner through `TEST_SUPPORT` when it is created. **Resolved (wave 3a-1):** P4-15 owns
  it (`TEST_SUPPORT support`, `TEST_INCLUDES support`; header request network-4).

#### Kernel

- `RuntimeLifecycle` ran only in Debug (not under msvc-asan or RelWithDebInfo).
- The Pin workaround comment in `runtime/lifetime/Parts.h` is outdated (Pin now retains the part). **Resolved:** the comment now says that
  `Pin(&part)` retains the part (and with it the owner).
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
- `gs.lint.concurrency` covers only `game-server/src`, not `handlers/` (M6 expects handler chunks to be lint-clean). Still open; since S0c it
  runs `--werror --cycles=core`.
- regscan (optional): leave `*Prelude.h` files out of the per-category file counts of the registry report.
- Stale comments: `game-server/CMakeLists.txt` still gives the header check as 516 TUs (519 with 8 preludes); the C1 comment in `chunks.cmake`
  could read "(+ the command PCH and preludes)". **Resolved:** both comments are updated; the header check comment still says "S0b adds up to
  64 hub headers" (74 with the spine headers).

## S0b: hub headers

> Implemented 2026-09-14 by a workflow of 13 agents: 1 pattern agent (style guide, reference hubs, skeleton.py rules), 5 parallel hub groups
> (objects, world, controllers, stats, handlers), 1 cycle and part reviewer, 3 reviewers (prototype, fidelity and runtime lenses) and 3 fixers
> (objects-world, controllers-stats, handlers).

The 59 hub classes have their full member layout from `fieldmap.py`, every Java method declared under its Java name in Java order, trivial
bodies ported and everything else `AION_UNPORTED`. The cycle review resolved every retaining-cycle edge outside the handler scripts and defined
`LogoutBreakers`. The reviewers found 33 issues (9 high, 13 medium, 11 low); the fixers reported 42 outcomes: 37 fixed, 4 deferred (all
resolved in S0c: the AionClientPacket include, the EffectTemplate virtual getters, the member-type guards, the lint and fieldmap waivers) and 1
rejected (handler parameter nullability, already following §5.1 evidence). What was left went into the 157-item ledger
`build/workflows/s0b-followups.json`, worked off by S0c (see Freeze).

### Delivered components

Paths are relative to `game-server/src/aion/gameserver/` unless they start with `game-server/` or `tools/`.

| Component | Location | Verification |
|---|---|---|
| Style guide | `docs/design/hub-headers.md` (extended through S0c) | applied by every group; checklist §15 |
| Reference hubs `AionObject` (auto-release ids pushed to `CleanerQueue`, `equals`/`hashCode`), `Persistable` (`NEW`/`CHANGED`/`DELETED` built at static initialization), `VisibleObject` (`create<T>`, `CreateKey`, `postConstruct`, `SelfOrRef` target, `ZombieBreakable`, `breakTarget`) | `model/gameobjects/` | `aion_gs_objects_tests` (`AionObjectTest` 3, later `ObjectsHubTest`, `SpinePrototypeTest`) |
| Objects hubs: Creature, Npc, Summon, Player, PlayerCommonData, Item, IStorage, Storage, TemporaryPlayerTeam, SpawnTemplate, SpawnGroup, House; spine headers Expirable, L10n, GeneralTeam, TeamMember | `model/gameobjects/**`, `model/items/storage/`, `model/team/`, `model/templates/spawns/`, `model/house/`, `model/Expirable.h`, `model/templates/L10n.h` | method coverage script: no Java method missing (Player 239, Item 126, PlayerCommonData 90, Creature 83, ...); declaration/definition scan clean after the fix |
| World hubs: World, WorldPosition, WorldMapInstance, MapRegion, KnownList, ZoneInstance, ZoneName; network: AionConnection, AionServerPacket, AionClientPacket; PacketSendUtility, DataManager, SpawnEngine, GeoService; spine headers `StateSet.h`, `SerializedBody.h` | `world/**`, `network/aion/`, `utils/PacketSendUtility.h`, `dataholders/DataManager.h`, `spawnengine/`, `world/geo/` | `/W4 /WX` probe TU (packet templates, StateSet constexpr, `AION_CLIENT_PACKET`); linked checked-build probe of `ZoneName::NONE` static initialization; `gs.smoke.startup` |
| Controllers: VisibleObjectController, CreatureController, NpcController, PlayerController, ObserveController, ActionObserver, EffectController, AggroList, CreatureMoveController | `controllers/**` | scratch compile against stand-in headers; 163 stubs |
| Stats and skills: Stat2, CreatureGameStats, CreatureLifeStats, Effect (with `Effect_ForceType`), Skill, SkillEngine; hand parts of the SkillTemplate and EffectTemplate shells; spine header StatOwner | `model/stats/**`, `skillengine/**`, `model/stats/calc/StatOwner.h` | checked-build executable for the `ForceType` interning; 295 stubs |
| Handler-facing hubs: AI, AbstractAI, AITemplate<T>, NpcAI, InstanceHandler, GeneralInstanceHandler, ZoneHandler, QuestZoneHandler, QuestEngine, AbstractQuestHandler, QuestEnv, QuestState, ChatCommand, AdminCommand, PlayerCommand, ConsoleCommand; spine headers GameEngine, GeneralZoneHandler | `ai/`, `instance/handlers/`, `world/zone/handler/`, `questEngine/**`, `utils/chathandlers/`, `model/GameEngine.h` | `HandlerRegistry.h` compiles unchanged; one sample handler per category with its real marker; `SpinePrototypeTest` creates the commands through their factories and checks Java's syntax info |
| Preludes with using-declarations and hub includes | `game-server/handlers/aion/gameserver/handlers/**/*Prelude.h` | `tools/gen/tests/test_handler_preludes.py` 6 |
| Cycle review: 310 new `cycles.toml` resolutions, kind conventions, `fieldmap.py` graph fixes, `LogoutBreakers` step tables | `game-server/generated/concurrency/{cycles.toml,fieldmap.toml}`, `model/gameobjects/player/LogoutBreakers.h` (P4-12) | `RetainingGraphTest`; tools.gen real-tree test that no core edge loses its resolution; step tables checked against `cycles.toml` |
| skeleton.py: hub rules in drafts, `--guards [--freeze]`, `SPINE_HEADERS`, lock classes, override detection | `tools/gen/skeleton.py`, fixtures | tools.gen (`SpineGuardsTest`, hub-rules fixtures); all `@hubs` drafts with dependencies compile at `/W4 /WX` (412 files, 206 TUs) |
| Header check lists | `game-server/CMakeLists.txt`: `gs_hub_headers` (64: `skeleton.py HUBS` without generated enums, plus ThreadPoolManager.h and IDFactory.h) and `gs_spine_headers` (10), each a `CONFIGURE_DEPENDS` glob of exactly that file | configure prints "74 of 74 hub and spine headers"; `test_skeleton_tree` keeps both lists equal to `HUBS`/`SPINE_HEADERS` |
| Forward headers | the 14 `fwd.h` of packages whose generics became non-template classes (template heads removed) | `skeleton.py --fwd --check` 252 files, 0 problems |

### Mapping decisions that matter to porters

1. **Signatures** ([hub-headers.md §5](hub-headers.md#5-reference-kinds-in-signatures)): object parameters `X&`, `Ptr<X>` only on direct null
   evidence (§5.1); returns `Ptr<X>`; part, owner and singleton accessors `X&` (or `Ptr` where Java checks the part for null); factories
   `Ref<X>`, new parts `std::unique_ptr<X>`. Nullable enums, boxed values and Timestamps are `std::optional` (§6); a nullable packet parameter
   is a borrowed `SM_X*`.
2. **Generics** ([§8](hub-headers.md#8-generics)): project-bounded generics are erased to one non-template class (`CreatureController`,
   `GeneralTeam`, `TeamMember` with `AionObject`); `AITemplate<T>` (with `OwnerType`) and unbounded utilities stay templates. Subclasses
   binding a type variable redeclare narrowing accessors; cast-only Java overrides do not make the base virtual.
3. **Construction** ([§10](hub-headers.md#10-construction-parts-owners)): `VisibleObject::create<T>` with `CreateKey` and `postConstruct`;
   `Creature::postConstruct` and `AIEngine::newAI` are ported (handlers-and-porting-plan.md §1.7 as built). `this` is usable during
   construction (`makeRef` pre-counts). Java abstract classes have protected constructors.
4. **Interfaces held by `Ref`** declare `retain`/`release` ([§9.2](hub-headers.md#92-interfaces)); quest handlers and commands are Immortal,
   stored as raw pointers (`QuestEngine.questHandlers`); `AbstractAI::setRegistryEntry`/`getRegistryEntry` replace `@AIName`.
5. **Name hiding and access** ([§9.1](hub-headers.md#91-declarations)): `using Base::name;` wherever an override hides overloads; a
   Java-private overload named by it is protected.
6. **Ported beyond trivial accessors** ([§2](hub-headers.md#2-what-s0b-ports-and-what-stays-aion_unported)): empty bodies, pass-throughs,
   layout constructors, part plumbing, static initializers (`ZoneName::NONE` and `Effect_ForceType` interning open a STARTUP `TaskScope`).
7. **Packets and connections** ([§12](hub-headers.md#12-packets-and-connections), runtime-architecture.md §8.2 as built): eager
   `serialize`, `opcodeOf<SM_X>` constructors, `StateSet`, the `SerializedBody` send queue, `shared_ptr` packets for delayed sends.
8. **Parts and cycles** (runtime-architecture.md §5.1, §5.3): SpawnTemplate is an OwnedPart of SpawnGroup (with `detachedTemplates` for
   Town.java:138); MapRegion is a part of WorldMapInstance; Player friend list, emotions, motions, npc factions, title list and store,
   Creature.transformModel and Item.idianStone are `PartSlot`s; `Player.legionStorageProxy` is a C++-only RECLAIMER slot; `LogoutBreakers`
   L1-L7, `onDelete` D1-D6 and 12 zombie-safe edges; instance destroy detaches the handler, clears `startPos` and the registered team.
9. **Data decisions of the cycle review:** guessed collections are kept where the port only reads them and corrected where semantics differ
   (`Creature.skillCoolDowns` ConcurrentHashMap; `EffectController.passiveEffectMap`, emotions and motions LinkedHashMap; Item mana and
   fusion stones TreeSet by slot; `NpcEquippedGear.items` TreeMap; `WatchmanHokuruki.randomPositions` synchronized list). External types:
   Timestamp/Date → `commons::database::Timestamp`, `JobDetail` → `Ref`, `CronExpression` by value, `WeakReference` → `Ref`; Cleaner,
   schedulers, thread pools, `Constructor<T>`, XML factories and `Random` are not members. `Polygon2D` uses value types `Rectangle2D` and
   `Path2D`. No per-run service besides AhserionRaid; the three leave events are K5.
10. **DataManager** (static-data.md §3.3 as built): 92 `HolderRef` members; `getInstance()` stays the `AION_UNPORTED` site of the smoke test.

### Key numbers

| Item | Value |
|---|---|
| Hub classes | 59 (header check: 64 hub headers + 10 spine headers = 74) |
| Size by group (as reported) | objects 12 hubs + 4 spine headers, about 2,650 header lines, 16 `.cpp`; world about 1,900 header and 1,600 `.cpp` lines; controllers 18 files, about 1,500 lines, 163 stubs; stats 17 files, 3,280 lines, 295 stubs; handlers 16 hubs + 2 bases, about 1,900 header and 1,150 `.cpp` lines, about 190 stubs |
| Drafts after the pattern stage | `TODO(signature)` 97 → 45, `TODO(fieldmap)` 26 → 17; object parameters 1,584 `X&` vs 323 `Ptr<X>`; `@hubs --with-dependencies` 412 files, 206 TUs, about 20 s compile |
| Prelude using-declarations | ai 243, instance 101, quest 65, zone 18, admincommands 229, playercommands 52, consolecommands 59 |
| Cycles before / after the review | 696 edges, 97 components, 687 unresolved, 9 resolutions → 695 edges, 12 components, 376 unresolved (all under `game-server/data/handlers`), 319 resolutions (127 accepted, 95 java-hook, 63 cpp-breaker, 22 interim part, 12 zombie-safe), 0 stale |
| After the fixers | 673 edges, 11 components, 22 interim part keys replaced by `fieldmap.toml` part overrides; 77 `accepted: cut elsewhere` edges proven cycle-free by the scratch checker |
| fieldmap effects | Ref-to-part retention +19 edges, captured singletons −13 false edges; K4 2,207 → 2,204, K5 591 → 594, parts 25 → 27, `fieldmap.toml` overrides 5 → 23 in the review (the controllers-stats fixer added the overrides matching the other groups' headers and the part set) |
| Reviews | 33 findings: prototype 9 (4 high), fidelity 11 (2 high), runtime 13 (3 high) |
| Final S0b verification (fixers) | fresh configure; two Debug builds, 0 warnings; header check 74/74; `lint_concurrency --werror` 1,279 files, 0 findings; `skeleton.py --guards` 42 guards, 9 open S0b transition guards (allowed until the freeze); `fieldmap.py --check`, `chunks.py check`, `--fwd --check` clean; full ctest 1,313/1,313 (149 skipped, 5 disabled) |

## S0c: declaration headers

> Implemented 2026-09-14 by a workflow of 14 agents: stage 1 with 6 lanes (infra, xmlgen, model-a, model-b, services-a, services-b), stage 2
> with 3 (server packets A-K and L-Z, hub integration), 3 freeze reviewers (fidelity, freeze, infra lenses) and 2 fixers (headers, infra),
> followed by 1 finalize agent.

Every core class that a hub, service, DAO or packet names now has a declaration header: the missing member types, 128 services, 56 DAOs, 239
server packets and the engines. The hub integration removed every guard and set `SPINE_FROZEN`, and `SpinePrototypeTest` creates an Npc
through the real constructor chain. The three reviewers found 28 issues (4 high, 10 medium, 14 low) and each judged the tree not yet
freeze-ready, mainly for two ownership contradictions (PlayerAllianceGroup, ChargeInfo), the fieldmap model disagreeing with the headers, and
a lint regression; the fixers fixed all 28 (one part of the ChargeInfo finding was rejected with reasons) and deferred one item, the K4
classification of scheduled Runnables, which the finalize stage did.

### Delivered components

| Component | Location | Verification |
|---|---|---|
| Infra: `Pin`/`IsStaticTemplate` for classes that are RefCounted and templates; `BaseClientPacket` binds `toString` in `setConnection`, so `AionClientPacket.h` no longer pulls Asio and `<windows.h>`; lint rule changes; `fieldmap.py` erasure, external spellings, own-type constants, lock-class output, `[captures]` and `drop = true` | `runtime/lifetime/RefCounted.h`, `runtime/sched/Pin.h`, commons `network/packet/BaseClientPacket.h`, `tools/porting/lint_concurrency.py`, `tools/gen/fieldmap.py` | tools.porting 65, fieldmap suites 66, `BaseClientPacketTest`, `SchedContractTest` |
| xmlgen: virtual getters for Java-overridden accessors; `L10n` and `StatOwner` bases on shells; `QuestsData` hook and `getQuestById`; SpawnSpotTemplate and Spawn accessors; `NpcEquippedGear` RefCounted with the `BindContext` `Ref` overload; `applyEffect` overrides in 110 effect shells (109 new `.cpp`); DataManager waivers removed | `tools/xmlgen`, `generated/`, `model/templates/**`, `skillengine/effect/**`, `dataholders/` | tools.xmlgen 61; `aion_gs_xml_tests` 60; xmlgen check 3,055 files |
| Model lane A: 50 listed member types plus 24 closure classes (AccountTime to SiegeSpawnTemplate), `SummonedObject` and the first enum companion `StorageTypeInfo.h`: 151 files, 9,672 lines, 530 stubs | `model/account/`, `model/gameobjects/**`, `model/items/**`, `model/house/`, `model/team/**`, `model/templates/spawns/siegespawns/` | `aion_gs_player_tests` 5, `aion_gs_items_tests` 2 (as reported by the lane) |
| Model lane B: 64 classes, 124 files, about 7,360 lines; `AIEngine::newAI` ported; `SM_SYSTEM_MESSAGE.h` (constructors, `toJavaString`, variadic `Object...` constructor) with the generated factories compiled in `aion_gs_sysmsg` | `controllers/**`, `ai/`, `instance/InstanceEngine.h`, `geoEngine/**`, `model/skill/`, `model/stats/**`, `questEngine/`, `skillengine/`, `spawnengine/`, `world/`, `network/aion/serverpackets/SM_SYSTEM_MESSAGE.h` | `aion_gs_sysmsg_tests` 4 (moved `GeneratedSysMsgTest`), `aion_gs_network_crypt_tests` 27 |
| Services A-L: 69 classes, 138 files, about 7,070 lines | `services/**` | lint, header check |
| Services M-Z and DAOs: 59 services + 56 DAOs, 230 files (4,204 header and 5,938 `.cpp` lines), 852 stubs; SQL constants in the DAO `.cpp` files | `services/**`, `dao/` | completeness scripts: nothing missing |
| Server packets A-K: 112 packets + `AbstractPlayerInfoPacket`, `AbstractHouseInfoPacket`; 228 files; 177 constructors, 148 ported; 16 `recipients()` overrides | `network/aion/serverpackets/` (P4-16; the Abstract bases P4-17) | standalone `/W4 /WX` compile of the 114 headers |
| Server packets L-Z: 125 packets, 250 files; 194 constructors, 131 ported; 10 `recipients()` overrides | `network/aion/serverpackets/` (P4-17) | lint L10 |
| Hub integration: RR-16 tags on the remaining 41 hub members, 128 unused waivers removed, `Ref<NpcEquippedGear>` in Npc/Creature, `EffectTemplate::applyEffect = 0`, NpcAI types and protected constructors, `VisibleObjectController::onDelete` → `LogoutBreakers::onDelete`, SpawnTemplate/SpawnGroup spot constructors, quest handler constructors; 83 declaration headers and 82 sources nobody had scheduled; all guards removed; `SPINE_FROZEN = True`; `skeleton.py --definitions` | hubs, `tests/objects/SpinePrototypeTest.cpp`, `tools/gen/skeleton.py` | `SpinePrototypeTest` (Npc through `create<Npc>`, AIEngine, unknown AI name, `NO_AI`, `//ai set` factory, Effect, pinned AI part, LeakCensus at zero; quest, quest zone, general zone handlers; commands); `HubDefinitionsTest` |
| Headers fixer: `StatFunction` implements `IStatFunction` (`RcStatFunction<T>`, `StatFunctionProxy.h`, `ofTemplate`); `PlayerAllianceGroup.alliance` a `Ref`; `ChargeInfo` protected destructor; `SM_CREATE_CHARACTER` per recipient; nullable signatures; LifeStats and team narrowing accessors; `BoundRadius::intern`; Player account scenario | `model/stats/calc/functions/`, `model/team/**`, `model/items/ChargeInfo.h`, `model/templates/BoundRadius.h`, `model/account/` | `SpinePrototypeTest.PlayerAccountDataIsAPartOfItsAccountAndInternsItsBoundRadius` |
| Infra fixer: lint Immortal names per class, RR-16 as a warning, `OwnerRef` placement, L5 precision, `Outer::Inner` mapping; `fieldmap.py` `[bases]`, shared part types dropped, `*Holder` singletons, K5 elements as values; about 70 `fieldmap.toml` overrides matching the frozen headers | `tools/porting/lint_concurrency.py`, `tools/gen/fieldmap.py`, `game-server/generated/concurrency/` | tools.porting 68; `test_fieldmap_fields/kinds/real`, `test_skeleton_fixtures` |
| Finalize: 65 `// fieldmap:` waivers turned into `// fieldmap.toml:` notes, 13 unused L7 waivers deleted; `[cpp_members]`, `[captures] drop = true`; task-object inference; 16 core Runnables K4 (`[kinds]`), 4 more made RefCounted; 11 new core cycle edges resolved | `tools/gen/fieldmap.py`, `lint_concurrency.py`, `fieldmap.toml`, `cycles.toml`, services `.cpp` files | `TaskObjectsTest` 3, `CppMembersTest` 3, `test_l2_cpp_only_members`; unused-waiver script 0 |

### Mapping decisions that matter to porters

1. **Deviations from `fieldmap.py` are `fieldmap.toml` decisions** ([hub-headers.md §4](hub-headers.md#4-members), runtime-architecture.md
   §3.2.2 as built), not waivers: no `// fieldmap:` waiver is left in `game-server/src`. Lock classes, `OwnerRef` placement and C++-only
   retaining members are checked without waivers.
2. **Scheduled task classes** ([§7.3](hub-headers.md#73-callbacks), runtime-architecture.md §22, §24): a Runnable handed to a scheduler is
   never K5; K4 ones are RefCounted with `create()`, K3 immutable ones are `TaskStruct`s.
3. **One lifetime base per class tree** ([§9.2](hub-headers.md#92-interfaces), [§10.2](hub-headers.md#102-parts)): a class also held by
   `Ref` is no part (`ChargeInfo`, `PlayerAllianceGroup`); mixed hierarchies get no base (`ItemStone`); `StatFunction` serves static data and
   run-time functions through no-op `retain`/`release` and `RcStatFunction<T>`.
4. **Statics** ([§11.1](hub-headers.md#111-statics)): nothing reads configs or static data at static initialization; `static final`
   RefCounted objects are never-released `Ref`s; literal arrays are `constexpr std::array`.
5. **Packets** ([§12](hub-headers.md#12-packets-and-connections)): `opcodeOf` constructors, protected opcode constructors for packet bases,
   out-of-line destructors, cached `shared_ptr` constants, hand-written `recipients()` checked by L10 (no `ServerPacketTraits.gen.h`).
6. **Collections in signatures** ([§7.1](hub-headers.md#71-collections)): shims by non-const reference for callee-filled collections,
   `Ref` elements when a returned list holds the only references, `std::map` where Java's order matters.
7. **Nested types across headers** ([§3.1](hub-headers.md#31-header), [§9.3](hub-headers.md#93-nested-inner-anonymous-and-local-classes)):
   DAO and model headers are included for nested record types; `Effect.ForceType` is hoisted to `Effect_ForceType.h`.
8. **Two-phase construction** ([§10.1](hub-headers.md#101-visibleobjectcreatet-and-postconstruct)): `WorldMap2DInstance`/`3DInstance`
   `create()` calls `initMapRegions()`; `PenaltySkill` calls its own `initializeSkillMethod()`.
9. **Run-time template objects:** `BoundRadius::intern` (DEVIATIONS); `NpcEquippedGear` is RefCounted and shared by templates and Npcs
   (static-data.md S0b/S0c notes).
10. **Handler creation:** quest handlers read `QuestsData::getQuestById`; tests publish quest data with `DataManager::QUEST_DATA.publish(...)`
    ([§3.5](hub-headers.md#35-freeze-gates)).

### Key numbers

| Item | Value |
|---|---|
| New declaration classes | member types 139 (model A 75, model B 64) + 83 integrator headers; services 128 (69 + 59); DAOs 56; server packets 239 (114 + 125, including the 2 Abstract bases) |
| New files by lane | model A 151, model B 124, services A-L 138, services M-Z and DAOs 230, packets A-K 228, packets L-Z 250, integrator 165, effect shell `.cpp` 109 |
| Tree at the freeze (`game-server/src`) | 1,648 headers, 1,037 `.cpp`; 5,201 `AION_UNPORTED();` stubs in 901 `.cpp` files; 177 core constructors still unported (Freeze) |
| Guards | 71 (30 open) and 52 missing headers at the start of hub integration (plus 20 from the packet lanes) → 0 guards, 0 missing |
| Definitions | `skeleton.py --definitions`: 82 hub and spine headers, about 1,840 declarations, 0 undefined |
| Lint | 1,965 files, 43 RR-16 advisories (stage 1) → 2,677 files, 4 advisories (integration) → 2,685 files, 0 errors, 0 warnings, 0 advisories (`--werror --cycles=core`, finalize) |
| Waivers | 221 unused waivers found by infra; 128 removed by integration, 88 by the headers fixer, the last 65 `// fieldmap:` and 13 `// lint: L7` by finalize |
| `fieldmap.json` | `externalType` flags 58 → 28; unresolved field types 0; cycle edges needing a resolution 681 in 10 components, 376 unresolved (handler scripts), 305 resolutions; after the freeze verification 686 in 12 components, 376 unresolved, 310 resolutions |
| Build | clean configure 17 s; full Debug build 2,420 TUs in 220 s (review run; about 3 min at `--parallel 8`); second build compiles nothing; 0 warnings |
| Rebuild cost of one header (review, touch) | `Creature.h` 190 TUs, 76 s; `Player.h` 119 TUs, 43 s; `AionServerPacket.h` 248 TUs, 24 s; `ItemService.h` 2 TUs, 10 s. `VisibleObject.h` is in 216 TUs, `Creature.h` in 186, `EnumTraits.h` in 854 |
| Reviews | 28 findings: fidelity 10 (2 high), freeze 9 (1 high), infra 9 (1 high); fixers: headers 15 fixed, infra 13 fixed |
| Final verification (finalize) | fresh configure; two full Debug builds, 0 warnings; `aion_gs_header_check` 0 warnings; lint as above; `skeleton.py --guards --freeze` and `--fwd --check` (252 files); `fieldmap.py --check`; `xmlgen.py check` (3,055 files); `chunks.py check`; full ctest with the DB variables: 1,327/1,327 passed, 73 skipped, 310.5 s (`tools.gen` 278 s) |

## Freeze

The spine is ready to freeze on the working tree of 2026-09-14. Nothing is committed yet and the tag `spine-v1` is not created; the
integrator commits and tags (handlers-and-porting-plan.md §2.5).

### Freeze verification

After the finalize stage two independent read-only verifiers checked the tree: an ownership-graph verifier (its own C++ header parser,
2,884 classes, the retaining graph compared with `fieldmap.json`, `parts.json` and `cycles.toml`) and a freeze verifier (all gates, virtual
dispatch of every hub and S0c class against the Java override graph with compile-time `static_assert`s, const correctness of virtual
families, constructor reachability). They reported 12 findings (graph: 3 medium, 6 low; freeze: 1 medium, 2 low); one fixer verified each
against Java and fixed all 12 (the RunnableRunner half of the JobDetail finding was rejected):

| Finding | Outcome |
|---|---|
| `Spatial.parent` ↔ `Node.children` was a real object cycle: the nodes `GeoWorldLoader` builds and drops (mesh prototypes, `a\|b` originals, DespawnableNode copies) would never be freed | `Spatial.parent` is a non-retaining `Field<Node*>` (`fieldmap.toml`; the parent owns its children); `Node.children` is `accepted: no instance cycle` |
| `ChargeInfo.item` (`OwnerRef<Item>` outside a part) was safe only by a comment | re-checked against Java (conditioningInfo is nulled only for inventory items); now checked: `fieldmap.toml` `holders`/`accessor`, lint L3 rejects other members, stored-lambda captures and pins naming ChargeInfo and reads of `item` outside `getItem()`, whose body is an `AION_CHECK("C4", item.isManaged(), ...)` |
| The `LifeStatsRestoreService` task cycles (holder → Future → task → lifeStats part → Creature) were invisible | `fieldmap.py` follows a Future returned to the caller to the field the caller assigns; 4 new edges resolved as java-hooks (`cancelRestoreTask`, `cancelFpReduce`, `cancelFpRestore`) |
| `IStatFunction::compareTo() const` could not call the non-const `getPriority()` | `getPriority()` and `isBonus()` are `const` on IStatFunction and all 8 implementors (the priorities call `isBonus()`); `compareTo` is ported |
| `DropNpc.lootingTeam` and `Item.currentModifiers` retained in C++ without a model edge | `[fields]` overrides take `retains = [...]` |
| `JobDetail` handles retained their job's captures invisibly | `[stored_callback_apis] handle = true` on `CronService.schedule`: the class keeping the returned JobDetail owns the callback (3 singleton holders, no cycle). Rejected: modelling `RunnableRunner` as `shared_ptr` in fieldmap, since `services/cron/` is kernel area whose Java classes are re-architected (lint skips L1-L3 there) |
| Six `accepted: cut elsewhere` reasons did not name every edge their cycles pass | the reasons name `RVController.slave`/`passedPlayers` and `LegionStorageProxy.storage` (no instance cycle); rerunning the verifier's residual graph leaves only `server lifetime` cycles |
| `RecallService` `request.timeout = schedule(...)` was not a Future holder | qualified field writes resolve the qualifier's type; the new edge (and a same-shaped one in `XmlMerger`) is `accepted: one-shot task` |
| `Crypt.packetKey` differed from the model spelling | `fieldmap.toml` override `std::optional<EncryptionKeyPair>` |
| `ItemStone::getL10nId() const` could not call `getItemTemplate()` | `getItemTemplate() const` |
| `HostileUpEffect.tempHate` had no per-cast slot | C++-only `Effect::hostileUpTempHate` with accessors (`[cpp_members]`, DEVIATIONS, static-data.md S0b/S0c notes) |

The freeze verifier found virtual dispatch sound everywhere else: 732 must-virtual methods in 111 classes declared virtual or pure, 1,639
overriders marked `override`/`final`, no missing or ambiguous interface base, no non-virtual destructor in a polymorphic class.

### Gates

| Gate ([hub-headers.md §3.5](hub-headers.md#35-freeze-gates)) | Result on the final tree |
|---|---|
| No `__has_include` guard; every member-type header exists | `skeleton.py --guards --freeze`: 0 guards, 0 missing; `SPINE_FROZEN = True`, so any new guard fails tools.gen |
| Every hub and spine member function defined | `skeleton.py --definitions` (tools.gen `HubDefinitionsTest`): 0 undefined |
| Prototype | `SpinePrototypeTest` runs its Npc scenario unskipped (7 tests, nothing compiled out); the Player scenario stops, as expected, at PetList. The only test double for game logic: the Npc's life stats are a `CreatureLifeStats` test subclass (1,200 HP, 300 MP) supplied through the virtual `setupStatContainers`, because `NpcLifeStats` reads the unported stat calculation (P5-01) |
| Builds | fresh configure, two full Debug builds and `aion_gs_header_check` (74/74) with 0 warnings |
| Lint | `lint_concurrency.py --werror --cycles=core game-server/src`: 0 findings; lock classes enforced as warnings; 0 unused waivers |
| Generators | `fieldmap.py --check`, `skeleton.py --fwd --check`, `xmlgen.py check`, `chunks.py check` clean |
| Tests | full ctest 1,327/1,327 passed (73 skipped), including `tools.gen`, `tools.porting`, `tools.xmlgen`, `gs.lint.concurrency`, `gs.smoke.startup` and the database integration tests; rerun after the freeze verification fixes (fresh configure, two Debug builds with 0 warnings, all generator and lint gates): 1,327/1,327 passed, 73 skipped, 373 s (`tools.gen` 336 s) |

### Ledger outcome

The S0b ledger (`build/workflows/s0b-followups.json`: 85 change requests, 67 open issues, 5 deferred findings) has an outcome for all 157
items, counting an item as done if any S0c lane changed something for it, else resolved if a lane showed it already true: **111 done, 34
resolved, 12 deferred**. The freeze reviewers flagged outcomes as missing or unconvincing for S0B-009, 016, 019, 030, 035, 040, 076, 093,
107, 111, 144 and 149; the fixers closed them except S0B-035 (no `toJavaString` overloads for object parameters, P4-06) and S0B-093 (the four
C++-only breaker helpers are not `noexcept` yet, harmless while `LogoutBreakers` catches; they become `noexcept` when ported). The deferred
items:

| Id | Item | Owner |
|---|---|---|
| S0B-039 | `AionConnection` send queue: ordered insertion into a `std::deque` is O(n) and 29-32% of inserts arrive out of sequence under parallel senders (runtime-architecture.md §21); may need a heap or per-thread batches, a different member type is a header request | P4-15 |
| S0B-042, S0B-118 | `WorldMap2DInstance`/`WorldMap3DInstance`: `std::unique_ptr<MapRegion> createMapRegion(int32_t) override`, `initMapRegions` through `regions.put`, `using WorldMapInstance::getRegion;`, and `create()` calling `initMapRegions()` | P4-10 |
| S0B-052 | Java bug to keep (D6): `VisibleObject.setTarget` assigns the target before calling `onTargetChanged(target, creature)`, so the old target always equals the new one (VisibleObject.java:206-209) | P4-11a |
| S0B-056 | Confirm the nullability of `onDie(Creature& lastAttacker)` (guarded by `requireNonNull`) and `addHate(Creature&)` when porting the bodies | P5-01 |
| S0B-064, S0B-139 | Memory of per-creature `ConcurrentHashMap`s (`CreatureGameStats.stats`: 16 stripe Monitors per creature) | kernel ([runtime-kernel-status.md](runtime-kernel-status.md)) |
| S0B-092 | WorldMapInstance/MapRegion parts and the C++-only instance methods: only a services lane reported it (deferred), but the freeze review found `PartMap regions`, `createMapRegion`, `detachInstanceHandler` and `releaseRegisteredTeam` in the header, so the header side is done | P4-10 (bodies) |
| S0B-099 | The `InstanceService.destroyInstance` port calls `detachInstanceHandler()`, `setStartPos(nullptr)` and `releaseRegisteredTeam()` after `onInstanceDestroy()`, and removes the instance from `InstanceScaler.scalings` | P5-13 |
| S0B-100 | Leave events are K5: create them on the stack; the INSTANCE_KICK task captures `Ref<Player>` and the team instead of `this` | P5-10 |
| S0B-102 | Cycle resolutions trust Java lifecycle hooks read by hand; scenario tests must confirm each hook runs on every path | P7 LeakCensus scenarios |
| S0B-105 | Type-level expansions keep the giant component large; handler porters mark type-only cycles `accepted: no instance cycle` with a reason | phase-6 handler chunks |

### Freeze exceptions

What the frozen spine knowingly does not do yet:

| Exception | Owner |
|---|---|
| `create<Player>` stops in the Player constructor at `PetList`, whose Java constructor loads pets through `PlayerPetsDAO`; after it, `postConstruct` creates `PlayerGameStats`/`PlayerLifeStats`, which need static data and the stat calculation. `SpinePrototypeTest` covers Account, PlayerCommonData, PlayerAppearance and the PlayerAccountData part with its interned BoundRadius, and asserts `UnportedException` at PetList. **Wave 3a-1:** PetList is ported and pets load in `Player::postConstruct` (Java order); `create<Player>` works in tests with the C++-only `PetList::setPlayerPetsLoaderForTests` seam and stat doubles, and stops without them at `PlayerPetsDAO` and the `PlayerGameStats` constructor ([phase4-status.md](phase4-status.md)) | PlayerPetsDAO P4-14, stats P5-01 |
| No World, WorldMap or WorldMapInstance object can be built: their constructors are `AION_UNPORTED`, and `world/WorldMapInstanceFactory.h` and `world/zone/ZoneService.h` have no declaration header. So no instance handler object is created (only the factory type is checked) | world P4-10; InstanceEngine/InstanceService P5-13 |
| 177 core constructors had `AION_UNPORTED` bodies at the freeze because they read static data, configs, DAOs or services; their objects cannot be created and the Immortal singletons among them throw from `getInstance()`. **Ported in wave 3a-1** (removed from the list below): P4-05 Announcement, GameTime; P4-07a SpawnGroup (4); P4-11a AssembledNpc, BrokerItem (2), Letter, ServerWideGroup and the two template-pointer Item constructors; P4-12 AbyssRank, PetCommonData, PetList, NpcFaction; P4-15 AionConnection, ConnectionAliveChecker. `Kisk` and the Item DAO constructor are ported but still reach `AION_UNPORTED` through the `NPC_DATA`/`ITEM_DATA` lookups (P4-09; `NpcData::getNpcTemplate` exists since the 3a-2 pre-stage). Still unported by owner: P4-04 Node; P4-10 GameTimeService, WeatherService, AbstractPeriodicTaskManager, World, WorldMap, WorldMapInstance; P4-11b RVController (2), SiegeWeaponController, AbstractCollisionObserver; P4-13 DropItem, EnchantEffect, TemperingEffect, ChargeInfo, IdianStone, ItemStone, ManaStone, RandomBonusEffect; P4-16 29 and P4-17 59 packet constructors; P5-01 DamageList, TeamDamageList, PlayerGameStats; P5-02 PlayerSkillEntry, Skill (2); P5-08 PvpService, AbyssRankingCache; P5-09 AtreianPassportService, BonusPackService, BrokerService, FactionPackService, CraftSkillUpdateService, AdventService, StarterKitService; P5-10 AGPlayer, LookingForParty, ChallengeTask (2), PlayerGroup, Legion, LegionWarehouse; P5-11 House, HouseBids, LegionDominionLocation, Town (2), HousingBidService, HousingService, TownService; P5-12a SiegeShield, ShieldService, SiegeService, Assault; P5-12b Base, BaseLocation, BaseService, EventBuffHandler; P5-13 PeriodicInstanceManager, PlayerTransferService; P5-14 AdminService, AnnouncementService, CronJobService, CuringZoneService, DebugService, FlyRingService, PeriodicSaveTask, PeriodicSaveService, RoadService, SurveyService. File and line list: `build/spine-finalize-work/unported_ctors.txt` | the listed chunks |
| The cycle graph does not see a task held by a `CreatureController.addTask` call it cannot attribute (the Npc SHOUT and player TELEPORT controller tasks), a task object kept only in a local or `FutureTask` wrapper, or captures inside the `PinnedCallback` of a C++-only class. They rely on "Tasks: run/cancel; `onDelete` → `cancelAllTasks`" (runtime-architecture.md §5.1); the class comments state the cancel points. (The `LifeStatsRestoreService` Futures returned to the caller are followed since the freeze verification) | NpcShoutsService P5-14, TeleportService P5-08, CreatureController P4-11b |
| C++-only breakers named in `cycles.toml` exist only as declarations: the `PlayerAlliance.groups` clearing in the `PlayerAllianceService.disband` port, `WorldMapInstance::detachInstanceHandler` in `destroyInstance` (the `LogoutBreakers` `run`/`onDelete` bodies and the zombie breaker are ported since wave 3a-1; steps L4 and L7 are skipped until `ObserveController::clearWithoutNotify` and `PlayerController::breakStanceObserver`, P4-11b, are ported) | P5-10, P5-13, P4-11b |
| Handler scripts: `--cycles=core` skips cycle edges under `game-server/data/handlers` (376 unresolved, including new handler task objects such as `ArtifactAI.ArtifactUseSkill`); `gs.lint.concurrency` covers only `game-server/src` | phase-6 handler chunks (e.g. P5-05 `aion_gs_handlers_ai_core` for `ai/ArtifactAI`) |
| More creation limits (freeze verification, measured by running the constructors): a plain `create<Npc>` stops in `NpcLifeStats` at the unported stat calculation (the prototype uses a `CreatureLifeStats` double); `Summon` needs a Player master and then stops at `SummonLifeStats` and `Summon::setAlwaysResistElement`; `House(objectId, building, address, instanceId)` constructs but its `postConstruct` stops at `resetDoorState`/`setPersistentState`; `ZoneInstance` tests needed an `Area` double because no concrete area header existed (**resolved in wave 3a-1:** P4-05 ported `PolyArea`, `CylinderArea`, `SphereArea`, `RectangleArea`, `SemisphereArea`) | stats P5-01; Summon P4-11a; House P5-11 |
| Handler bases without a declaration header: `world/zone/handler/AdvancedZoneHandler.h` (implemented by the PvPZone handler) and `ai/HpPhases.h` (`HpPhases.PhaseHandler`, implemented by 49 handler AIs). Both are new additive headers, not changes to frozen classes | AdvancedZoneHandler P4-10; HpPhases P5-05 |
| Effect shells owned by P5-03 declare no behaviour virtuals yet that P5-04 effects override (`DamageEffect` 7 methods, `BufEffect.startEffect`, `AbstractOverTimeEffect.startEffect`, `AbstractHealEffect`); P4-08 (the lease that declares behaviour stubs in phase 4) must declare them, or P5-04 needs cross-chunk requests | P4-08 |
| K3 task objects (`DecayTask`, `GeneralUpdateTask`, `ItemUpdateTask`, `SurveyService.TaskUpdate`, `SiegeStartRunnable`, `WorldRaidRunnable`, `RiftOpenRunnable`, the `Offline*Checker`s) are printed by `fieldmap.json` with base RefCounted but stay immutable `TaskStruct` values; the lint does not check the base | RespawnService P4-10, PlayerEnterWorldService P5-00, SurveyService P5-14, siege P5-12a, worldraid and rift P5-12b |

### Header requests after the freeze

From the tag on, frozen headers change only through header requests ([hub-headers.md §14](hub-headers.md#14-header-requests-after-the-freeze)):
the requesting chunk files the exact declaration change with Java evidence, marked additive (a C++-only helper, a missing overload or
narrowing accessor, with an `AION_UNPORTED` stub; batched daily without review) or layout/signature (member type, parameter kind,
virtual-ness, access; reviewed, because dependent chunks recompile). A request that changes a K3/K4 member also updates `fieldmap.toml` and,
for a new retaining edge, `cycles.toml`. Until it lands the chunk ports against the requested signature and marks the call
`// header-request: <entry>`. Bodies, and new files the chunk owns, need no request. No `__has_include` guard may be used to wait for a header.
Known candidates: ordered containers for the hash-ordered packet members (DEVIATIONS), the `AionConnection` send queue (S0B-039), a per-send
`SM_MACRO_RESULT`, and the additive P5-01 declaration headers for `PlayerStatFunctions`, `StatWeaponMasteryFunction`,
`StatArmorMasteryFunction` and `StatShieldMasteryFunction`.

### Open issues by chunk

Phase 4:
- **P4-04 geo:** `Node(String)` and so `GeoMap(mapId)` need the `CollisionIntention` companion; geo is not a leaf until its DataManager,
  ZoneService and SiegeService uses are callbacks (S0a item). `Spatial.parent` is non-retaining: `attachChild`/`detachChildAt` set and clear
  it, and nothing may keep a child of a node the loader drops.
Wave 3a-1 ported P4-05, P4-06, P4-07a, P4-11a, P4-12 and P4-15; their current state and remaining needs are in
[phase4-status.md](phase4-status.md). The entries below are corrected for it.

- **P4-05 base:** ~~the `ChatType` companion and `utils::simpleClassName`~~ exist since wave 3a-1 (`ChatTypeInfo.h`, `utils::simpleClassName`
  and 20 more companions); the local copies in `network/detail/SystemMessageL10n.h` (`chatTypeIdOf`) and `AIEngine.cpp`
  (`simpleClassNameOf`, P5-05) still have to switch. Remaining enum companions belong to their chunks (pattern `StorageTypeInfo.h`).
- **P4-06 sysmsg:** ~~`writeImpl`, the 3 hand factories and object `toJavaString` overloads~~ ported in wave 3a-1 (S0B-035 closed); the
  separate `toJavaString` sets of SM_QUESTION_WINDOW and SM_CLOSE_QUESTION_WINDOW would still format a `bool` as "1" (no caller passes one).
- **P4-07a/b templates:** ~~`setXmlUid`/`setXmlName` and `SpawnSpotTemplate::afterUnmarshal`~~ ported (P4-07a, wave 3a-1); the real zones
  import still needs `ZoneName::createOrGet` (P4-10); `ExtractedItemsCollection.getChance` (P4-07b) stays non-virtual until a `Chance`
  interface exists.
- **P4-09 dataholders:** `QuestsData::getQuestTemplates` (Java HashMap order) and `getQuestsByNpcFaction` are unported.
- **P4-10 world:** S0B-042/118; the World/WorldMap/WorldMapInstance constructors and the missing `WorldMapInstanceFactory.h`/`ZoneService.h`
  (freeze exception).
- **P4-11a objects:** S0B-052 (kept in wave 3a-1); ~~`Kisk` template lifetime~~ resolved (one immortal default `KiskStatsTemplate`,
  docs/deviations/P4-11a.md); `ArtifactAssault`/`FortressAssault` (P5-12a) and the `SummonedObject`, `Trap`, `Kisk`, `SummonedHouseNpc`
  constructors still reach `AION_UNPORTED` in their base initializer (`NPC_DATA` lookup; `NpcData::getNpcTemplate` exists since the 3a-2
  pre-stage, `NpcKnownList.h` P4-10 is still missing for Trap).
- **P4-12 player:** 9 `AION_UNPORTED` left (ExpireTimerTask P5-14, stat listeners P5-01, ItemUseObserver P4-11b); `create<Player>` needs the
  pet loader seam and stat doubles in tests (freeze exception above).
- **P4-11b controllers:** `MaterialSkillTask` is K4 and must be written as a RefCounted class (the forward declaration in
  `AbstractMaterialSkillActor.h` is compatible); the `cycles.toml` java-hook reasons cite Java lines (AbstractMaterialSkillActor.java:49-53)
  to keep in sync.
- **P4-13 items:** `NpcEquippedGear::init(LoadContext&)` is unported; `ChargeInfo` bodies read the item through `getItem()` (lint L3), and
  no port may hold `Ref<ChargeInfo>` outside `Item.conditioningInfo` and the ObserveController.
- **P4-14 DAO:** record `hashCode` with String or enum components (`RankingListPlayer`, `RankingListLegion`, `Bookmark`,
  `PlayerAndLegionInfo`, `MultiClientingService.Identifiers`) needs a Java-compatible `String.hashCode` helper; `main.cpp` passes no `usedIds`
  sources yet.
- **P4-15 network:** S0B-039 (unchanged; measure first); ~~`SM_KEY` serialized in `AionConnection::initialized`~~ done in wave 3a-1 (a
  private `SM_KEY` subclass until P4-16 ports `writeImpl`); ~~`tests/support/` owner~~ P4-15 (`TEST_SUPPORT support`, header request
  network-4).
- **P4-16/P4-17 packets:** overloads that differ only in integer width or `bool` need exactly typed arguments (`SM_BROKER_SERVICE`,
  `SM_UPGRADE_ARCADE`, `SM_MOTION`, `SM_TITLE_INFO`), and `SM_FIND_GROUP` takes `std::vector<Ptr<FindGroupEntry>>`; `SM_MACRO_RESULT`'s
  shared static packets stay valid only while `serialize` is read-only.

Phase 5:
- **P5-00 login slice:** `GeneralUpdateTask`/`ItemUpdateTask` are K3 `TaskStruct`s; the `LogoutBreakers` scope guard in `leaveWorld`.
- **P5-01 stats:** S0B-056; the additive stat function headers (above); per-creature map memory with the kernel (S0B-064).
- **P5-02 skills:** `PenaltySkill` calls its own `initializeSkillMethod()`; the non-empty `NpcSkillList(Npc&)` branch needs
  `NpcSkillTemplateEntry`; `Effect.stopTasks` is the java-hook for AuraTask, ConfuseTask and FearTask.
- **P5-03/P5-04 effects:** `AuraTask`, `ConfuseTask` and `FearTask` are written as K4 RefCounted classes; `HostileUpEffect::calculate`
  stores `tempHate` with `Effect::setHostileUpTempHate`, and `applyEffect` and its task read it from the effect.
- **P5-05 AI:** `FollowSummonTaskAI`, `AggroNotifier` and `SimpleCheckedAttackAction` are written as K4 RefCounted classes.
- **P5-06 quest:** `FollowingNpcCheckTask` and the `DestinationChecker` tree are written as K4 RefCounted classes.
- **P5-08 player services:** `TeleportService.SpawnTask` (a local `FutureTask`) is not followed by the task inference (the
  `LifeStatsRestoreService` tasks are, with java-hook resolutions); the `VeteranRewardService` reward lists are default-constructed and must be filled
  without reaching `AION_UNPORTED` at static initialization.
- **P5-10 team:** S0B-100; port the narrowing accessors `getMember`/`removeMember`/`getLeader` of PlayerGroup, PlayerAlliance,
  PlayerAllianceGroup and League as `runtime::cast<M>(Base::method(...))` once `PlayerGroupMember.h`, `PlayerAllianceMember.h` and
  `LeagueMember.h` exist, and PlayerAlliance's constructor with `groups.put(groupId, PlayerAllianceGroup::create(*this, groupId))`; the disband
  breaker (DEVIATIONS entry exists).
- **P5-12a siege:** `ShieldService.IGNORED_SHIELDS_BY_MAP_ID` (a `Map.of` literal) is default-constructed; `ArtifactSiege.h`/`FortressSiege.h`
  are missing.
- **P5-13 instance:** S0B-099; `InstanceBuffTask` and `EmptyInstanceCheckerTask` hooks (InstanceBuff.java:61-66, InstanceService.java:83-84
  cited in `cycles.toml`); neural network: `Link.input`/`output` are `Final<PlayerModelLink*>`, so `PlayerModel` must own its neurons for the
  lifetime of the links.
- **P5-14 misc:** `NpcShoutsService` SHOUT tasks and `SurveyService.TaskUpdate` (K3 `TaskStruct`); the ported `GameServer::main` keeps the S0a
  items (`-D` overrides with EventService, P5-12b; used-id sources).

Later and tools:
- **Phase 6 and P7:** S0B-105 and the 376 handler cycle edges; S0B-102 LeakCensus scenarios, which must exempt never-released static objects
  such as `ItemService::DEFAULT_UPDATE_PREDICATE`.
- **fieldmap.py:** it prints base RefCounted for K3 task objects that the port writes as `TaskStruct`s (emit a TaskStruct base and teach
  `skeleton.py`, or keep the convention); task-object inference does not follow tasks held in locals and `FutureTask` wrappers; there is no
  automatic check that the edges an `accepted: cut elsewhere` reason names break every cycle through it (the freeze verification checked
  them with an independent graph); the name-based `addTask` resolution picked the wrong owner for `RespawnService.DecayTask` (harmless there, but lambda
  tasks use it too); the erasure rule reads only Java, so a hand-erased generic with an unbounded type variable other than `TeamMember` would
  still print its type variable.
- **Review:** the Bash tool rewrote `\b`/`\t` escapes in heredocs during the finalize session; the edits were redone and a scan found no
  control characters, but skim the diffs of `tools/gen/fieldmap.py` and `tools/porting/lint_concurrency.py` before committing.
- **Hand-mapped nullability** of String, enum, boxed and K5-by-value types was reviewed unevenly, and not line by line for the 83 integrator
  headers (`ChallengeTask`, `Announcement`, `ArtifactLocation`, ...); porters apply hub-headers.md §5.1/§6 evidence when they port a body and
  file a signature request where it differs.
- **Cosmetic:** about 140 lines in service headers exceed 150 columns (clang-format corrupts `AION_LOCK_CLASS(X::f#stripe)`); the header
  check comment in `game-server/CMakeLists.txt` still says "S0b adds up to 64 hub headers"; the `cycles.toml` header still explains the
  unused `part` kind.
