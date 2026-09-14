# Porting plan: Aion 4.8 server emulator, Java → C++

The Java server (`../commons`, `../login-server`, `../chat-server`, `../game-server`) stays untouched and is the reference implementation.
The C++ port lives in `cpp/` on the `C++` branch.

## Goals

- **A fun project to tinker with.** It is not a production server, so modern C++ that is pleasant to read beats a 1:1 transliteration.
- **Faithful behaviour and a byte-identical wire protocol**, so the unmodified Aion 4.8 client works against it.
- **Windows first** (MSVC/Visual Studio, local play), but portable: no Win32 API outside thin wrappers, so Linux can follow later.
- **The same data and config files as the Java server** (`config/*.properties`, `data/static_data/**/*.xml`, `data/geo`, SQL schemas).

## Size (at the time of planning, Java lines)

| Module | Files | Lines |
|---|---|---|
| commons | 86 | 7.2k |
| login-server | 88 | 6.9k |
| chat-server | 61 | 2.8k |
| game-server core (`src/`) | 2,312 | 235k |
| game-server handlers (`data/handlers`: quests, AI, instances, commands) | 1,729 | 158k |

## Phases

| # | Scope | Done when | Status |
|---|---|---|---|
| 0 | Build skeleton: CMake + vcpkg manifest, presets, conventions docs | `cmake --preset msvc` configures and builds | ✅ 2026-09-12 |
| 1 | `commons`: logging, config, database, networking, utilities | Unit tests pass; network integration test passes | ✅ 2026-09-12: ported, adversarially reviewed (52 findings fixed/resolved), 394 tests |
| 2 | `login-server` | A real 4.8 client logs in through the C++ login server | ✅ 2026-09-12: ported, reviewed (13 findings resolved), 147 tests; real 4.8 client logged in and got the server list |
| 3 | `chat-server` | Chat works with the game server | |
| 4 | game-server foundation: static data (JAXB replacement), geo, world, object model, DAOs, network packets | All static data loads and counts match the Java server | in progress: design done; runtime kernel (P1-P4) implemented and tested ([status](design/runtime-kernel-status.md)); next: tooling wave (generators, lint), spine |
| 5 | game-server systems: skills, stats, quests engine, services, AI framework | Log in, walk around, fight a mob | |
| 6 | Handlers: quests, AI, instances, admin/player commands | Mostly mechanical, done in parallel batches | |

Tooling needed by later phases:
- **XML loader generator (phase 4):** reads the JAXB annotations of all ~787 JAXB files (templates, skillengine, dataholders, questEngine, ...)
  and writes member blocks plus pugixml binders (see design/static-data.md).
- **No Java reference runs:** the user decided against installing a JDK, so the Java server is a read-only reference. Protocol code is
  verified with standard test vectors, client-side inverse implementations, fake client/server end-to-end tests, adversarial review, and
  finally the real Aion 4.8 client.
- **A local MariaDB/MySQL server** to run the servers and the database integration tests: portable MariaDB 11.8 LTS in `D:\aion-dev`
  (see README › Local database).

## Key decisions

| Topic | Decision |
|---|---|
| Language / toolchain | C++23, MSVC (VS 2026), CMake ≥ 3.28, vcpkg manifest mode |
| Networking (Java NIO selector threads) | Standalone Asio, one `io_context` with N threads, one strand per connection |
| Packet execution (`PacketProcessor`) | Thread pool with per-connection serial execution (same ordering guarantees as Java) |
| Database (JDBC + HikariCP) | JDBC-shaped C++ API (`Connection`/`PreparedStatement`/`ResultSet`) over MariaDB Connector/C with a small pool, so DAOs port almost line by line |
| Configuration (`@Property` reflection) | Explicit binding calls (`processor.bind("key", FIELD, "default")`) with the same parsing rules as the Java transformers |
| Logging (slf4j + logback) | spdlog with a Java-like `Logger` facade; logback.xml appenders re-created in code |
| JSON (fastjson2) | nlohmann-json |
| HTTP (java.net.http, Discord webhooks, external auth) | cpr |
| Crypto (JCE RSA/SHA-1/Blowfish) | OpenSSL for RSA/SHA-1; the project's own Blowfish implementation is ported as-is |
| XML (JAXB) | pugixml plus generated loaders (phase 4) |
| Scheduling (Quartz cron) | a cron expression parser (phase 4/5) |
| Strings | `std::string` holding UTF-8 everywhere; UTF-16LE only at the wire boundary |
| Script handlers (runtime `javax.tools` compilation) | Compiled into the game server binary with explicit registration (see below and the handler design) |
| Game server runtime (GC, free threading) | Free-threaded like Java: atomic intrusive `Ref<T>` with task-scoped borrows and an epoch Reclaimer, `Field<T>`/`Monitor` derived from Java modifiers, eager packet serialization. See [design/runtime-architecture.md](design/runtime-architecture.md) |
| Static data (JAXB) | Python generator from the Java annotations; generated member blocks included in hand-written classes; census-based verification. See [design/static-data.md](design/static-data.md) |
| Handler registration | One-line marker macros + build-time registry scanner. See [design/handlers-and-porting-plan.md](design/handlers-and-porting-plan.md) |
| Config introspection for `//configure` (Java: reflection on config fields by name) | To be decided with the handler registration design in phase 4/6: field names on `bind`, per-class field enumeration, value-to-string |

## Future notes (not planned now)

- **Hot reload of quests/AI/instance handlers.** The Java server compiles `data/handlers` at startup and can reload them at runtime.
  The C++ port deliberately compiles handlers into the binary for now (simpler and debuggable).
  Options to revisit in the distant future:
  1. Build handlers as a separate shared library (`aion_handlers.dll`/`.so`) with a C ABI registration entry point. Reload means unregister
     everything, unload, then load the new library. It is fragile: no handler object, vtable or `std::function` may outlive the unload.
  2. Embed a scripting language (Lua via sol2) for quests/AI. This needs rewriting the handlers, but gives true hot reload.
  3. Live++ or similar tooling for development-time patching only.
- **Linux build**: keep the code portable; add GCC/Clang presets once the login server runs.
- `DeadLockDetector` (JVM `ThreadMXBean`) has no C++ equivalent. It could be replaced by a watchdog on the game loop/thread pools.
