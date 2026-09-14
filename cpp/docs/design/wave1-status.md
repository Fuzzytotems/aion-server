# Wave 1 status (tooling wave)

> Implemented 2026-09-13/14 by a workflow of 10 implementation agents, 2 reviewers and 2 fixers against
> [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.4, [static-data.md](static-data.md) and
> [runtime-architecture.md](runtime-architecture.md). Where the result departs from the designs, each design has a "Wave 1 implementation
> notes" section (runtime-architecture.md §22); Java-visible differences are in [DEVIATIONS.md](../DEVIATIONS.md) under game-server.
> Spine step S0a (2026-09-14) resolved several open issues below; they are marked "Resolved (S0a)" and described in
> [spine-status.md](spine-status.md).

The reviewers found 15 issues (3 high: `counter_skill` values broke the generated skill binder, `skeleton --fwd` declared 164 xmlgen structs
as `class`, `AbstractAI` was forward-declared as a template). All were fixed with tests, together with two open issues (unchecked required
elements, JAXB's null for unknown enum values).

## Delivered components

Test counts are the last reported runs (after the fixes where the suite was re-run).

| Component | Location | Tests (as reported) |
|---|---|---|
| Java source parser (`javasrc.py`: tokenizer, declarations, body helpers, `ProjectIndex` resolver) | `cpp/tools/gen/javasrc.py` | 97 (lexer 27, declarations 31, bodies 21, resolver 14, real tree 4) |
| Field mapping, escape analysis, parts, cycles (`fieldmap.py`) | `cpp/tools/gen/fieldmap.py`; outputs and hand-owned `fieldmap.toml`/`cycles.toml` in `cpp/game-server/generated/concurrency/` | 49 (kinds 17, fields 16, callbacks 14, real tree 2) plus fixer tests |
| Forward headers and drafts (`skeleton.py`) | `cpp/tools/gen/skeleton.py`; fixtures `cpp/tools/gen/tests/fixtures/skeleton/**` (owned by the skeleton tool) | 32 (fixtures, real tree, MSVC `/W4 /WX` compile checks) |
| sysmsg, DialogAction, opcode generators | `cpp/tools/gen/{sysmsg,dialogaction,opcodes}.py`; outputs `game-server/src/aion/gameserver/network/aion/{ServerPacketsOpcodes.gen.h, ClientPacketInfo.gen.inc, serverpackets/SM_SYSTEM_MESSAGE.gen*}`, `model/DialogAction.h/.gen.cpp` | 74 (opcodes 25, DialogAction 18, sysmsg 21, real tree 10) |
| `tools/gen` suite (all of the above) | CTest `tools.gen` | 264 after the fixes |
| Concurrency lint (L1-L20, W0) | `cpp/tools/porting/lint_concurrency.py`; CTest `tools.porting`, `gs.lint.concurrency` | 32 (rules 29, calibration 3) |
| JAXB replacement generator (`xmlgen`) | `cpp/tools/xmlgen`; outputs `cpp/game-server/generated/` (2,911 files, `xmlmodel.json`, `staticdata-classes.json`, `xmlgen-report.md`) | 34 after the fixes (rules, emit, real tree incl. drift, V1 and the real-data enum/required walk) |
| Static data oracles V1-V4 | `cpp/tools/oracle` (`oracle.py`, `staticdata_oracle/`, committed `expected/`) | 58 (1 skipped: case collisions on NTFS) |
| XML binder runtime and import resolver | `game-server/src/aion/gameserver/dataholders/loadingutils` (`aion_gs_xml`) | `aion_gs_xml_tests` 60 (incl. 6 generated slice tests and real-data totals) |
| Game server configs (P4-01) | `game-server/src/aion/gameserver/configs` (`aion_gs_configs`) | `aion_gs_configs_tests` 52 |
| Geo math (P4-03) | `game-server/src/aion/gameserver/geoEngine/math` (`aion_gs_geomath`) | `aion_gs_geomath_tests` 77 (Debug, RelWithDebInfo, Release) plus CTest `gs.geomath.golden_drift` |
| Game client crypt (P4-15a) | `game-server/src/aion/gameserver/network/{Crypt,EncryptionKeyPair}` (`aion_gs_network_crypt`) | `aion_gs_network_crypt_tests` 31 (incl. checks of the generated opcode, DialogAction and sysmsg files) |
| Handler registry and `AION_UNPORTED` (P4-02b) | `game-server/src/aion/gameserver/handlers/HandlerRegistry.h` (`aion_gs_handler_registry`, header-only since S0a); S0a moved `Unported.h/.cpp` to `runtime/base` (`aion_gs_runtime_base`) | `aion_gs_handler_registry_tests` 5; the 6 `UnportedTest` cases moved to `aion_gs_runtime_base_tests` (11 before S0a) |
| Registry scanner `aion_gs_regscan` | `cpp/game-server/tools/regscan` (`AionRegscan.cmake`, README) | 42 unit, 5 integration, 1 empty, 3 CLI |
| Commons additions | `RunnableStatsManager::handleStats(key, ...)`, `Rnd::seedCurrentThreadForTests`, `DatabaseConfig::DATABASE_SOCKET_TIMEOUT`, `DatabaseFactory::Options` | commons core 151, configuration 91, database 64 (21 skipped without a database) |

## Open issues

### S0a: chunk manifest and spine skeleton

- The generated tree has no repository build target: only the slice's `.ipp` files compile, in `aion_gs_xml_tests`, through the include
  bridge. Create the static data library (compile `generated/**/*.bind.cpp`, add `generated/` as include root) and remove
  `generated/xmlgen-include-root.inc` afterwards. The full-tree compile was proven only in a scratch project with scaffolded shells.
  **Resolved (S0a):** `aion_gs_staticdata` (chunk T2-gen) compiles all of `generated/`, the shells compile in their chunk libraries, and
  `generated/` is an include root on `aion_gs_build_options`; the bridge is removed and no longer generated. The full tree builds with `/W4 /WX`
  and no warnings against the real `src/` shells; a strict load of all static data without hooks gives V3 totals equal to
  `tools/oracle/expected/totals.json` (11.4 s Debug, with a no-op `AION_UNPORTED`).
- Assign the generated files in `chunks.cmake`: `DialogAction.h/.gen.cpp` to the model chunk, `ServerPacketsOpcodes.gen.h` and
  `ClientPacketInfo.gen.inc` to P4-15, `SM_SYSTEM_MESSAGE.gen*.cpp` to P4-06. **Resolved (S0a):** DialogAction to P4-05, the opcode tables to
  P4-15, `SM_SYSTEM_MESSAGE.*` to P4-06 with `COMPILE_WHEN_EXISTS` (header-only until the hand-written `SM_SYSTEM_MESSAGE.h` exists).
- Call `aion_gs_add_registries()` with `HANDLERS_ROOT` = `game-server/handlers`, set `CXX_SCAN_FOR_MODULES OFF` on handler libraries and link
  `<name>_empty` in unit tests (handlers-and-porting-plan.md amendments §8). **Resolved (S0a):** done; `CXX_SCAN_FOR_MODULES` is off on every
  chunk library, and the `_empty` registry libraries compile configure-time tables that do not depend on the scan (`gs.registry.empty_tables`).
- Decide whether `Unported.h/.cpp` move out of `aion_gs_handler_registry` into a core target, so core code need not link the registry
  library. If so, update `skeleton.DEFAULT_UNPORTED_HEADER` and xmlgen `scaffold.UNPORTED_HEADER`. **Resolved (S0a):** moved to
  `runtime/base/Unported.h/.cpp` (`aion_gs_runtime_base`, namespace `aion::gameserver::runtime`, wave-1 names re-exported into
  `aion::gameserver::handlers`); both generator defaults use the new path.
- Real-tree tests (XML runtime, regscan) read `AION_GAMESERVER_JAVA_DIR` and skip without it; they have no CTest label yet. **Resolved (S0a):**
  label `realdata` (with `AION_GAMESERVER_JAVA_DIR` in the test environment) on the real-tree GoogleTest cases, `aion_gs_regscan.cli.java_tree`,
  `gs.chunks.consistency`, `gs.smoke.startup` and the whole `tools.*` Python suites; `ctest -LE realdata` is the fast run.
- S0a notes: the forward headers (252 committed `fwd.h` with a drift check), all enums through xmlgen, the behaviour class shells and the
  category preludes are done. `xmlgen.py check` now also runs the `src/` conflict checks (exit 2), including hand-written definitions of
  nested or secondary top-level generated enums in the header of their Java file, so the `tools.xmlgen` drift test catches an outdated
  skeleton draft.
  `gs.smoke.startup` is opt-in: set `AION_TEST_GS_DATABASE_URL` (e.g. `jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8`,
  optional `AION_TEST_GS_DATABASE_USER`/`_PASSWORD`), otherwise it is reported as skipped. The commons DB integration tests need
  `AION_TEST_DATABASE_USER=root` as well as `AION_TEST_DATABASE_URL`.

### S0b: hub headers

- `HandlerRegistry.h` forward-declares the hub classes: `AbstractAI`, `Creature`, `InstanceHandler`, `WorldMapInstance`, `ZoneHandler`,
  `QuestZoneHandler`, `AbstractQuestHandler`, the four command classes, `AionClientPacket` and `StateSet` must be non-template classes in
  those namespaces, or the header needs a change request.
- `AITemplate<T>` must declare `using OwnerType = T;` by hand; `skeleton.py` drafts `AbstractAI` as a non-template class but does not
  generate the alias.
- Cycle review: `cycles_report.md` has 687 unresolved edges (400 with suggestions), including the capture edges `Effect$1#this` and
  `Effect$2#this`; `cycles.toml` has only the 9 resolutions stated in the design. Enable `lint_concurrency.py --cycles` in CTest afterwards.
- Check for per-run singleton services besides AhserionRaid and add them to `fieldmap.toml [settings] per_run_services` (67
  `SingletonHolder` services are now Immortal).
- `PlayerGroupLeavedEvent` is K4 by analysis (runtime-architecture.md §14.2(d)): change the port (capture fields, K5 override) or keep K4.
- fieldmap heuristics to review: 89 guessed collection implementations, 58 external value types (Timestamp, JobDetail, Date, ...) without
  a C++ decision, 3 unresolved field types (`java.awt.geom` in Polygon2D).
- Drafts still carry TODO lines from the member data: `Field<JobDetail>`, a by-value `CronExpression`, `bool(?)` callback signatures, Java
  literal initializers such as `2f`, function declarations given as members. Reference members of template classes are bound through
  `unportedArgument<T>()` stub constructors, which S0b replaces.

### P4 static data (P4-07/08/09)

- `tools/xmlgen/emit.py` still emits `static_cast<void>(value); return true;` for `[ignore_attributes]`; add `c.ignoreAttribute();` so
  `BindStats` reports them (e.g. `item_template@cName`, 102,009 times) as ignored. `byTag` totals are unchanged. **Resolved (S0a):** emitted,
  with tests in `tools/xmlgen/tests/test_s0a.py`.
- The `xmlgen.toml` template for `NpcEquippedGearAdapter` assigns `o.{member} = std::make_unique<...>` and so destroys the previous gear on a
  repeated `<equipment>` in lenient mode, leaving its IDREF slots and task dangling. Use `c.replaceSingle(o.{member}, ..., e);` (also makes
  a repeat a strict-mode error). **Resolved (S0a):** the template uses `c.replaceSingle`, with tests in `tools/xmlgen/tests/test_s0a.py`.
- The class adapter calls `o.equipment->init(c.load())` during binding even with `runHooks=false`, and the `NpcEquippedGear` shell's `init`
  is `AION_UNPORTED`. Until P4-13 ports it, `npc_templates` cannot be loaded (the hooks and `setXmlUid`/`setXmlName` are unported too, so no
  real load passes before P4-07/08/09/13).
- `DataManager::init` must keep `LoadContext::takeRetired()` alive for the life of the process (resolved IDREFs can point into it).
- The test shells in `tests/xml/generated` port only part of the hooks; replace them with the real `src/` classes and drop the include
  bridge. `GeneratedSliceTest` was adapted to the new counting rules (first root only, skipped roots compared with the oracle). S0a dropped the
  bridge; the 7 test shells remain and duplicate `src/` shells, so `aion_gs_xml_tests` must not link `aion_gs_staticdata` until they are gone.
- 13 `String` fields that Java compares with null (e.g. `WorldMapTemplate.name`, `WalkerTemplate.rowValues`, `NpcTemplate.ai`) need an
  `[optional_strings]` entry or hand-written handling (listed in `xmlgen-report.md`).
- 54 enum companion headers (constructor data and methods, e.g. ZoneAttributes ids, `TribeClass.isGuard`) are hand-written work, listed in
  the report. **S0a:** with the core enums generated, 153 enums need companions (54 JAXB + 99 core), 4 of them with constant-specific bodies;
  40 nested core enums need `using Inner = Outer_Inner;` in a hand-written outer class (skeleton drafts write it).
- Not generated yet: V5 dump visitor, the holder dependency table for `HolderRegistration`, handler-private XML roots, and a test that
  constructs all 287 `@XmlElements` choices (needs all behaviour shells). **S0a:** the choices test is unblocked (the shells exist in `src/`).
- `[lenient_enums]` (warn-once, `std::nullopt`) has no C++ unit test, because no slice class has a lenient enum.
- Replace the placeholder config enums in `configs/detail/ConfigEnums.h` with the generated `ItemQuality`, `NpcRating` and `HouseType` once
  they build (commons `EnumTransformer` needs `EnumTraits` support first); `AbyssRankEnum` comes from its own port.
- `XmlParent::as<T>()` matches the exact type only; the 2 parent-using hooks (HouseAddress → HousingLand, SpawnsData → EventTemplate) must
  pass exact types.
- Measure in the slice: binding throughput, `collectStats` hashing cost and transient DOM memory of parallel parsing (item_templates is one
  57 MB file). `load()` called from a ForkJoin helper thread parses serially.
- pugixml does not report duplicate attributes or undefined entity references; only the expat-based Python V3/V4 tools catch them (deferred).
- JAXB RI behaviour that cannot be run (byte narrowing, lenient number parsing) stays covered only by the census proving the data never
  reaches it.
- Count oracle: crash paths not modelled (AutoGroup npc ids of recruitable groups, Float unboxing in cylinder/semisphere areas, EventData
  date validation, ItemRaceEntry cross-holder checks, empty-list NPEs of house spawns, shout lists and walker versions).
- The 4 config and schedule XML roots have no XSD, so V1 cannot check them.
- Rerun `fieldmap.py` whenever xmlgen regenerates: its committed outputs depend on `staticdata-classes.json` (`test_fieldmap_real` fails on
  drift).

### P4-06 packets (and P4-15)

- `SM_SYSTEM_MESSAGE.gen0..7.cpp` need the hand-written `SM_SYSTEM_MESSAGE.h` (after the S0b `AionServerPacket`) and a Java-exact
  `toJavaString(float)`: use geomath's `JavaFloat::toString` (link geomath or move it to commons). The test stub is exact only for fixed
  notation; `STR_CMD_LOCATION_DESC` is the only float factory. Since S0a the manifest keeps `SM_SYSTEM_MESSAGE.gen0..7.cpp` header-only
  (`COMPILE_WHEN_EXISTS`) until `SM_SYSTEM_MESSAGE.h` exists.
- When the real header lands, remove or move the `tests/network_crypt` stub `SM_SYSTEM_MESSAGE.h` (it shadows the real header),
  `GeneratedSysMsgTest.cpp` and `GeneratedSysMsgDefinitions0..7.cpp`; move the DialogAction and opcode tests to their chunks.
- `ServerPacketTraits.gen.h` (PER_RECIPIENT and non-cacheable packet lists, planned opcodes.py extension) is not implemented: it needs
  `writeImpl` analysis for `con` use including helpers and abstract bases, the hand-decided SM_GROUP/ALLIANCE_MEMBER_INFO and the §8.5 list.
- `ClientPacketInfo.gen.inc` needs its consumer macro in `AionClientPacketFactory` (P4-15).
- `SM_SYSTEM_MESSAGE.gen.h` (600 KB, half javadoc) and `DialogAction.h` (304 KB) are included by about 240 and 1,040 files; use a PCH or
  drop the javadoc from the generated header (one line in `sysmsg.render_header`) if compile time suffers.

### Runtime kernel

- `TaskKind::CALLBACK` (`runtime/base/TaskInfo.h`, used by `sched/PinnedCallback.h`) collides with the `<windows.h>` `CALLBACK` macro: a TU
  that includes windows.h (through spdlog or Asio) before the kernel headers does not compile. Rename it (e.g. `CALLBACK_`) or undefine the
  macro; the skeleton compile tests work around it (`KERNEL_FIRST`). **Resolved (S0a):** renamed to `TaskKind::CALLBACK_`;
  `WindowsHeadersFirstTest` includes `<windows.h>` before every public kernel header, and the `KERNEL_FIRST` workaround is removed from the
  `tools/gen` tests (`tools.gen` passes without it).
- `runtime/base/Exceptions.h` lacks `java.lang.ArithmeticException` (Rates.java catches it); `geoEngine/math/Matrix4f.h` declares one that
  should become an alias. **Resolved (S0a):** `commons::utils::ArithmeticException` in `aion/commons/utils/Exception.h`
  (`ExceptionTest.JavaTypeHierarchy`), re-exported by `runtime/base/Exceptions.h`; `Matrix4f.h` uses the alias.
- `DeterministicExecutor` could seed with `Rnd::seedCurrentThreadForTests` instead of assigning `Rnd::generator()` directly. **Resolved (S0a):**
  it does, and restores the previous generator state.

### Later

- `//configure` introspection (named field registry, `toJavaString`, commons `ConfigurableProcessor::describe`) is open for P4-01b/C2;
  `AION_BIND` is the hook.
- `GameServer::main` (P5-14) must call `DatabaseFactory::init(DatabaseFactory::gameServerOptions())`; until then the socket-timeout
  requirement is not wired. **S0a:** the interim `main.cpp` calls it; P5-14 keeps the call when it replaces the file.
- Commons `DatabaseConfig` fields are plain and rebound by `Config::load`; safe only while the database layer reads them at startup.
- Confirm the `RuntimeConfig` key names (runtime-architecture.md §10) before shipping a C++ `config/` directory.
- Not run under the sanitizer presets: `ConfigLoadTest.ReadersStayValidDuringConcurrentReloads` (msvc-asan) and `Unported`'s lock-free site
  list (ASan/TSan).
- `gtest_discover_tests` (PRE_TEST, `WORKING_DIRECTORY` = test source dir in `cmake/AionCompilerOptions.cmake`) writes
  `cmake_test_discovery_*.json` into test source directories on every ctest run; they are git-ignored now, but the output could move to the
  build directory. **Resolved (S0a):** tests run and are discovered in `<build>/test_work/<target>`; the stale files were deleted.
- regscan: the Java cross-check matches classes by mirrored path (a handler ported to another directory is "unknown to Java"), and the
  token-based scanner does not see macros that expand to namespaces, classes or markers.
- javasrc limits: annotations before a local class are not attached, pattern-variable scopes are block-approximated, role `expr` does not
  separate type names before `.`, members inherited from JDK/third-party supertypes are unresolved (43 references), local classes resolve
  only inside their declaring span, Unicode escapes outside literals raise.
- Lint scanner limits: no preprocessor evaluation, approximate operator overloads and complex templates, L2 compares normalized text. The
  `bench/` calibration findings (driver globals, fake-connection LeafMutex, ...) need waivers or acceptance by the bench owner.
- Geo math: `sin/cos/tan/exp/log/pow` cannot match HotSpot bit for bit; golden values come from an independent Python model (a shared
  misreading would go unnoticed). Other float-heavy chunks must test in Release too.
- The skeleton full-tree draft compile (`AION_SKELETON_FULL_COMPILE=1`, 4-6 minutes) is not part of the default CTest run.
- Large committed outputs: `fieldmap.json` (7.1 MB; K1/K2 entries could be trimmed) and `census.json` (919 KB; a flags-only form could
  replace it) if review churn becomes a problem.
