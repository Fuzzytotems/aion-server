# aion_gs_regscan

Build tool that replaces the Java script engines' class discovery (AIEngine, InstanceEngine, ZoneService, QuestEngine, ChatProcessor and the
reflective AionClientPacketFactory table). It scans the ported handler and client packet sources for one-line registration markers, checks
the file rules, and writes the `constinit` registry tables declared in
[`aion/gameserver/handlers/HandlerRegistry.h`](../../src/aion/gameserver/handlers/HandlerRegistry.h).
Design: [handlers-and-porting-plan.md §1](../../../docs/design/handlers-and-porting-plan.md). Dependency-free C++23; the output is not committed.

## Markers

Each handler `.cpp` registers its class with one marker line directly in its package namespace:

| Java | Marker | Table (sorted by) |
|---|---|---|
| `@AIName("aggressive")` | `AION_AI(AggressiveNpcAI, "aggressive");` | `aiHandlerEntries()` (name) |
| `@InstanceID(300110000)` | `AION_INSTANCE_HANDLER(BaranathDredgionInstance, 300110000);` | `instanceHandlerEntries()` (map id) |
| `@ZoneNameAnnotation(value = "A B", questId = 1012)` | `AION_ZONE_HANDLER(_1012SensoryArea, "A B", 1012);` (questId optional) | `zoneHandlerEntries()` (names) |
| `super(1500)` in an `AbstractQuestHandler` | `AION_QUEST_HANDLER(_1500OrdersFromPerento, 1500);` | `questHandlerEntries()` (quest id) |
| `AdminCommand` / `PlayerCommand` / `ConsoleCommand` subclass | `AION_ADMIN_COMMAND(Add);` `AION_PLAYER_COMMAND(Id);` `AION_CONSOLE_COMMAND(Attrbonus);` | `commandEntries()` (kind, class) |
| `packets[48] = new PacketInfo<>(CM_MOVE.class, ...)` | `AION_CLIENT_PACKET(CM_MOVE);` | `clientPacketEntries()` (name) |
| `QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers` | none: `spawn(`/`sp(` calls with literal ids | `npcIdsSpawnedByHandlers()` (id) |

## Rules checked (build errors, printed as `file(line,column): error: message`)

Markers:
- The whole marker on one line, starting the line, ending with `;` (a trailing comment is allowed).
- Literal arguments only: the class name, a plain string literal (printable ASCII, no escapes, no prefix, not raw; zone names separated by
  single spaces), a decimal `int` literal (no sign, suffix, separator or leading zero; map and quest ids > 0).
- At namespace scope in exactly the namespace of the file's directory; never inside a class, function, `#if` block or preprocessor directive;
  never in a header (`.h`); `AION_DETAIL_COMMAND` is internal.
- Handler markers only in their category directory below `aion/gameserver/handlers` (`ai`, `instance`, `zone`, `quest`, `admincommands`,
  `playercommands`, `consolecommands`); `AION_CLIENT_PACKET` only directly in `aion/gameserver/network/aion/clientpackets`.
- The class is defined (`class`/`struct` with a body) in the marker's namespace in one of the scanned files; a class is registered once.
- Keys are unique: AI name, map id, each zone name, quest id (Java would put/warn; duplicates are build errors here).

Handler files (the unity file rules; not applied to client packet sources):
- Every declaration is inside the package namespace: the namespace of the file's directory, where a directory that is a C++ keyword maps to
  the keyword plus `_` (`quest/template` → `...::quest::template_`). Nested, other, and anonymous namespaces are errors.
- No namespace-scope `static`, no `using namespace` anywhere. The one exception is `using namespace aion::gameserver::model::DialogAction;`
  (optionally `::aion::...`) at namespace scope of `aion/gameserver/handlers/quest/QuestPrelude.h` (Java: `import static DialogAction.*`).
- A type name is defined only once per namespace across all scanned files.
- Only `.cpp` and `.h` (`.hpp`, `.cc`, `.cxx`, `.inl`, `.ipp`, `.hh` are rejected); other files (data, notes) are ignored.

Java cross-checks (with `--java-handlers`; the Java class is the C++ path below `aion/gameserver/handlers` with dots, e.g.
`ai.instance.darkPoeta.CalindiFlamelordAI`):
- The key of a marker must equal the key the Java class at the mirrored path registers (`@AIName`, `@InstanceID`, `@ZoneNameAnnotation` value
  and questId, `super(id)` resolved through int constants, the command base class), and must not belong to another Java class.
- A marker whose Java class does not exist is allowed and listed as "unknown to Java" in the report.
- With `--java-client-packet-factory`, every `AION_CLIENT_PACKET` class must be in the Java packet table.
- Java-side inconsistencies (duplicate keys, a quest id that cannot be determined, unsupported annotation arguments) are errors too.

Registration follows the Java class listeners: public, non-abstract classes register, top-level classes and public static nested classes
(`pkg.Outer.Inner`) alike. Annotation keys are read from the class itself. Quest handlers (only below `quest/`) and commands are recognized
like `isAssignableFrom`: the superclass chain is resolved over all scanned files (nested member types, single-type imports, the package,
on-demand imports, qualified names) down to `AbstractQuestHandler` (also through the core quest templates such as `MonsterHunt`) or
`AdminCommand`/`PlayerCommand`/`ConsoleCommand`. A chain that ends in an unknown class of `com.aionemu.gameserver.questEngine.handlers` or
`com.aionemu.gameserver.utils.chathandlers`, a cycle, or a direct `ChatCommand` subclass is an error; when the core gains a new handler base
class, add it to `CORE_CLASSES` in `src/JavaScanner.cpp`. For a quest handler, the quest id is the `super(...)` argument of its own
constructor.

## Outputs

In the output directory:
- `Registry.<r>.gen.cpp` for `r` in `ai instance zone quest commands clientpackets npcids`: declarations of the factory functions (by the
  registry's function type, so a marker in another namespace is an unresolved external) and the sorted `constinit` table.
- `Registry.<r>.empty.gen.cpp`: the same accessor with an empty table.
- `registry_report.txt`: files per category, then `registry ported java missing unknownToJava` per registry (ai, instance, zone names, quest,
  admin/player/console commands, client packets, npc ids spawned by handlers), then the missing and unknown keys. The Java-only run over the
  original tree shows `457 / 73 / 5 / 1035 / 101 / 16 / 35 / 186 / 1093`: the phase-6 progress bar.

Files are only rewritten when their content changes; on errors nothing is written. The output contains no timestamps or absolute paths.
The QuestSpawnAnalyzer npc id scan runs over the raw text of `ai`, `instance` and `quest` sources (comments included, matches may span lines,
Java `Matcher.find` semantics with ASCII `\b` and `\d`), so handler ports must keep npc ids in `spawn(`/`sp(` calls literal.

## Command line

```
aion_gs_regscan --out DIR [--handlers-root DIR] [--clientpackets-root DIR] [--java-handlers DIR]
                [--java-client-packet-factory FILE] [--stamp FILE] [--quiet]
```
Exit code 0: success (prints a summary line unless `--quiet`), 1: errors, 2: invalid arguments. A full scan of the Java tree (1,729 files) takes
about 1.5 s in a Debug build.

## CMake: `aion_gs_add_registries()`

Defined in [AionRegscan.cmake](AionRegscan.cmake) (included by this directory; include the file directly if you call it before
`game-server/tools` is added):

```cmake
aion_gs_add_registries(aion_gs_registry
    HANDLERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/handlers"          # contains aion/gameserver/handlers
    CLIENTPACKETS_ROOT "${GS_SRC}"                                 # contains aion/gameserver/network/aion/clientpackets
    JAVA_HANDLERS "${GS_JAVA_DIR}/data/handlers"
    JAVA_CLIENT_PACKET_FACTORY "${GS_JAVA_DIR}/src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java"
    HANDLER_LIBRARIES aion_gs_handlers_ai_core aion_gs_network)    # define the factories
target_link_libraries(aion_game_server PRIVATE aion_gs_registry)        # all tables
target_link_libraries(aion_gs_world_tests PRIVATE aion_gs_registry_empty) # no handlers
```

It creates `<name>_<r>` and `<name>_<r>_empty` static libraries for every registry, the interface libraries `<name>` and `<name>_empty`, the
custom target `<name>_scan` and the variable `<name>_REPORT` (path of the report). The scan runs when the tool or a scanned file changed;
added and removed files are noticed through `CONFIGURE_DEPENDS` globs. All arguments except the name are optional; a given root must contain
its `aion/gameserver/...` directory (configure error otherwise). Do not point `HANDLERS_ROOT` at `game-server/src`: `HandlerRegistry.h`
itself is core code, not a handler file.

Handler libraries with equal file names in different directories (CalindiFlamelordAI.cpp, PadmarashkaAI.cpp) should set
`CXX_SCAN_FOR_MODULES OFF`, otherwise MSBuild warns MSB8074 about its module dependency files.

## Tests

- `aion_gs_regscan_tests`: lexer, marker and file rules, the spawn id pattern, the Java scanner, cross-checks, emitter golden output, the
  fixture trees and the real Java tree (skipped if `AION_GAMESERVER_JAVA_DIR` is missing).
- `aion_gs_regscan_integration_tests`: the tables generated from `tests/fixtures/valid` by `aion_gs_add_registries()`, linked with the fixture
  handlers (stand-in core classes in `tests/fixtures/valid/core`), calling every factory.
- `aion_gs_regscan_empty_tests`: the empty variants.
- CTest `aion_gs_regscan.cli.*`: diagnostics and exit code on `tests/fixtures/invalid`, and the Java tree counts.
