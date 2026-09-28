# Java → C++ porting conventions

These rules keep the port consistent across modules and sessions. When in doubt, look at how `commons` does it.

## Layout and naming

- A Java file `X/src/com/aionemu/X/<package>/<Class>.java` becomes `cpp/X/src/aion/X/<package>/<Class>.h` (+ `.cpp`).
  Example: `commons/src/com/aionemu/commons/network/AConnection.java` → `cpp/commons/src/aion/commons/network/AConnection.h`.
- Namespaces mirror packages: `com.aionemu.commons.network` → `aion::commons::network`.
- Includes are rooted at `<module>/src`: `#include "aion/commons/network/AConnection.h"`. Use `#pragma once`.
- **Keep Java class and method names** (`PascalCase` types, `camelCase` methods and fields, `SCREAMING_CASE` config constants),
  so the same identifier can be grepped in both trees. Infrastructure that is re-architected (e.g. NIO → Asio) may be renamed,
  but the header comment must name the Java class it replaces.
- **Keyword, reserved-name and macro rule.** One implementation, `cpp_identifier()` in `cpp/tools/gen/dialogaction.py` (it holds the keyword
  and macro lists), is shared by the generators; porters apply the same rule by hand:
  - A Java identifier that is a C++ keyword or a macro that `WindowsMacroGuard.h` cannot remove gets a trailing underscore: `register_()`,
    `delete_()`, namespace segment `template_` (the directory keeps the Java spelling), `DialogAction::NULL_`, `TaskKind::CALLBACK_` (the
    `<windows.h>` `CALLBACK` macro). A porter who writes `NULL` silently gets the C macro 0; the quest parity check should flag a bare `NULL`
    in handlers.
  - A name reserved in C++ (leading `_` plus an upper-case letter, or `__` anywhere) loses its leading underscores, has each underscore run
    collapsed and gets a trailing underscore: `_STR_MSG_Heal_TO_ME` → `STR_MSG_Heal_TO_ME_`, `STR_RESURRECT_DIALOG__SKILL` →
    `STR_RESURRECT_DIALOG_SKILL_`.
  - Java names that collide with C library macros are renamed: `FastMath.FLT_EPSILON` → `FLOAT_EPSILON`, `DBL_EPSILON` → `DOUBLE_EPSILON`,
    `Vector3f.NAN` → `NOT_A_NUMBER`. Grep for the new names when porting code that uses them.
  - A member named like a method of its class or of a runtime base method (`release`, `monitor`, `retain`, `refCount`) gets a trailing
    underscore. In definitions, a parameter that would hide a data member (MSVC C4458) is renamed to `value`; declarations keep the Java name.
- Unit tests: `cpp/X/tests/<package>/<Class>Test.cpp` (GoogleTest).
- Formatting: `.clang-format` (tabs, 150 columns, attached braces, same as the Java code).
- Keep a short header comment on each class saying what it is. Carry over Java authors in an `@author` line where a class is a direct port,
  since the project is GPLv3 and attribution matters.

## Types

| Java | C++ |
|---|---|
| `int` / `long` / `short` / `byte` | `int32_t` / `int64_t` / `int16_t` / `int8_t` (plain `int` is fine for local loop counters) |
| `char` | `char16_t` |
| `boolean` | `bool` |
| `String` | `std::string` (UTF-8). Parameters: `std::string_view` unless the callee stores it |
| `Integer` etc. (nullable) | `std::optional<int32_t>` |
| `byte[]` | `std::vector<uint8_t>` (owning) / `std::span<const uint8_t>` (view) |
| `List` / `ArrayList` | `std::vector` |
| `HashMap` / `HashSet` | `std::unordered_map` / `std::unordered_set` |
| `TreeMap` / `TreeSet` | `std::map` / `std::set` |
| `LinkedHashMap` | insertion-ordered container (vector + index map) |
| `EnumMap` / `EnumSet` | `std::array` indexed by the enum / bitset |
| `enum` | `enum class` + `magic_enum` for `name()`, `valueOf()`, `ordinal()` |
| `java.time.Duration` / millis | `std::chrono::milliseconds` etc. |
| `System.currentTimeMillis()` | `aion::commons::utils::currentTimeMillis()` (wall clock). Use `std::chrono::steady_clock` for measuring intervals |

### Semantics that differ between Java and C++ (bugs hide here)

- **Signed overflow is UB in C++.** Java wraps. Hashes, checksums, crypto and RNG code must use unsigned arithmetic (`uint32_t`) and cast back.
- **`>>>`** → cast to the unsigned type, shift, then cast back.
- **Shift counts:** Java masks the count (`x << 33` == `x << 1` for `int`), while C++ treats it as UB. Mask explicitly when porting such code.
- **`(byte)` / `(short)` narrowing** is well-defined modular in C++20 as well. `b & 0xFF` idioms port directly via `static_cast<uint8_t>`.
- **Integer promotion:** `int8_t` and `int16_t` promote to `int` like in Java, but `char16_t` is unsigned.
- **Strings:** Java `String.length()` counts UTF-16 code units. Where a length limit or index matters for the protocol or game rules
  (names, chat messages), use `StringUtils::utf16Length()`. Case-insensitive comparisons: `StringUtils::equalsIgnoreCase()`.
- **`Math.round`** rounds half up (`floor(x + 0.5)`), unlike `std::round`. **Float → int casts** saturate in Java; in C++ they are UB when out of range.
- **`HashMap` iteration order** is unspecified in both languages, so never depend on it.
- **Float sign of zero under MSVC `/O2 /fp:precise`.** `x < 0 ? -x : x` (Java `FastMath.abs` and similar hand-written abs code) is folded into
  `fabs`, which loses `-0.0f` and the sign of negative NaNs. Debug builds do not show it, so float-sensitive ports need test runs in Release or
  RelWithDebInfo. Write sign-of-zero code with bit operations or `std::signbit`. Float-heavy chunks (geo, `stats/AttackUtil`) should consider
  `/fp:strict`.
- **Float contraction.** Java never contracts `a * b + c` into a fused multiply-add. MSVC `/fp:precise` does not contract since VS 2022, but a
  future GCC or clang build must pass `-ffp-contract=off` to every game server target: inline header code is compiled in the includer's
  translation unit (GCC contracts by default). `aion_gs_geomath` also forces contraction off in its own `.cpp` files (`StrictFp.h`).
- **Monitors are reentrant.** Use `std::recursive_mutex` when a `synchronized` method can re-enter the same object; otherwise `std::mutex`.
  `volatile` → `std::atomic`. `wait`/`notify` → `std::condition_variable`.

## Ownership and lifetimes

- Default to value semantics and `std::unique_ptr`. Use `std::shared_ptr` only where the Java object is genuinely shared across threads
  or async callbacks with unclear end of life (network connections, packets queued for sending).
- Network connections are `std::shared_ptr` (`enable_shared_from_this`). Every async operation holds a reference.
- Game object model ownership is decided at the start of phase 4 and documented here then.
- Java singletons (`getInstance()`) → function-local static (`static T& getInstance()`), or namespace-scope functions for static utility classes.
  In the game server, static-only classes (DAOs, static services) stay classes with static member functions (`skeleton.py` drafts them so).

## Errors and exceptions

- Exceptions stay exceptions. Base class: `aion::commons::utils::Exception` (`aion/commons/utils/Exception.h`; derives from
  `std::runtime_error`, captures a `std::stacktrace` and an optional cause). Common Java types exist there too
  (`IllegalArgumentException`, `IllegalStateException`, `UnsupportedOperationException`, `IndexOutOfBoundsException`, `IOException`,
  `ArithmeticException`).
  Subsystems derive their own (`SQLException`, `TransformationException`, ...).
- Java `new XException(msg, cause)` inside a catch block → `throw XException(msg, std::current_exception())`.
- Java `throw new Error(...)` (fatal startup errors) → throw an `Exception` and let `main` log it and exit.
- Never let an exception escape a thread entry point, an Asio handler or a destructor. Catch it and log it, as the Java code logs
  `Throwable` at those boundaries.

## Logging

```cpp
// logger name = fully qualified Java class name, so log output and per-logger configuration match the Java server
static const auto log = aion::commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.AccountDAO");

log.info("Loaded {} accounts", count);             // fmt-style formatting (Java: string concatenation)
log.warn("Plain message");
log.error("Could not load account for: " + name, e); // message + exception (logs type, what(), stack trace and causes)
log.error("Could not save house {}", houseId, e);    // slf4j form: the trailing exception is not a format argument
catch (...) { log.warnCurrentException("msg"); }     // Java: catch (Throwable t) { log.warn(msg, t); } (all levels exist)
if (log.isDebugEnabled()) ...
```

- Loggers can be created before `Logging::init` (e.g. as statics); they log to stderr until then. `Logging::init` registers
  `Logging::shutdown` with `std::atexit` (Java: shutdown hook).
- Per-logger levels and extra sinks (logback `<logger>` elements) are set via `LoggerFactory::configure`, with logback semantics: the
  level is inherited from the nearest configured ancestor, and sinks accumulate up to the first `additive = false`. Tests remove their
  configuration again with `LoggerFactory::removeConfig`.
- Code that runs during static initialization or destruction (constructors/destructors of static objects, e.g. a static `PacketProcessor`)
  must not log through a namespace-scope `static const auto log = ...`, whose destruction order is unspecified. Use a leaked
  function-local accessor instead: `const Logger& log() { static const auto* l = new Logger(LoggerFactory::getLogger("...")); return *l; }`.
- Java's `removePropertiesUsedInLogbackXml` becomes `Logging::getPropertyKeys("gameserver")` plus the server's own logging keys.

## Utilities (`aion_commons_core`)

| Java | C++ |
|---|---|
| `java.nio.ByteBuffer` | `utils::ByteBuffer`. **Little endian by default**; readers of big endian files (e.g. `GeoWorldLoader`) call `order(ByteOrder::BIG_ENDIAN_ORDER)` after `wrap`/`allocate` |
| `String.substring(begin, end)` on names/chat | `StringUtils::substring(s, begin, end)` with UTF-16 indexes. Never use `std::string::substr` with Java character indexes |
| `toLowerCase`/`toUpperCase`/`equalsIgnoreCase` | `StringUtils::` versions (Unicode aware for Latin, Greek, Cyrillic) |
| `Integer.parseInt`/`Long.parseLong` | `utils::parseInt`/`parseLong` (`Numbers.h`), throwing `utils::NumberFormatException` |
| `System.currentTimeMillis()`/`nanoTime()` | `utils::currentTimeMillis()`/`nanoTime()` (`TimeUtils.h`) |
| `Thread.setName` | `utils::concurrent::setCurrentThreadName()` (names appear in log lines) |
| `addr.getAddress().getAddress()` / `isAnyLocalAddress()` | `InetSocketAddress::resolveAddressBytes()` / `isAnyLocalAddress()` |
| `ExecuteWrapper`, `RunnableStatsManager` | same names in `utils::concurrent`. For type-erased tasks and lambdas: `RunnableStatsManager::handleStats(std::string_view key, method, nanos)`; entries are keyed by the displayed name |

**`windows.h` macros.** `NOGDI`, `WIN32_LEAN_AND_MEAN` and `NOMINMAX` are defined globally. Every header or source that includes Asio or
Windows headers must include `"aion/commons/utils/WindowsMacroGuard.h"` as its last include. It removes `DELETE`, `IGNORE`, `IN`/`OUT`/
`OPTIONAL`, `near`/`far` and similar macros that collide with game enum values.

## Configuration (`@Property`)

C++ has no reflection, so each config class has a `bind` function listing the same keys and defaults. The parsing rules are Java's.

```cpp
struct Config {                                   // static fields like the Java class
	static inline utils::InetSocketAddress CLIENT_SOCKET_ADDRESS;
	static inline int32_t LOGIN_TRY_BEFORE_BAN = 0;
	static inline std::optional<ItemQuality> MIN_QUALITY;   // Java field that may be null -> std::optional
	static inline std::map<AbyssRankEnum, int32_t> RANK_POINTS;
	static void bind(ConfigurableProcessor& p);
};

void Config::bind(ConfigurableProcessor& p) {
	p.bind("loginserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:2106"); // @Property(key, defaultValue)
	p.bind("some.key.without.default", LOGIN_TRY_BEFORE_BAN);                                   // missing key: field keeps its value
	p.bindPattern("^gameserver\\.rank\\.(.+)\\.points$", RANK_POINTS);                           // @Properties(keyPattern), ECMAScript regex
}

// Java: ConfigurableProcessor.process(properties, Config.class, CommonsConfig.class, DatabaseConfig.class)
std::set<std::string> unused = ConfigurableProcessor::process(properties, {&Config::bind, &CommonsConfig::bind, &DatabaseConfig::bind});
```

**Fields that can change while the server runs.** The game server rebinds configs at runtime (`Config.load()` on event start/stop,
`//reload`). Java readers are safe during a reload because a reference store is atomic. Assigning a `std::string`/`vector`/`map`/`regex`
while another thread reads it is undefined behaviour. Rules:
- A non-scalar field rebound at runtime is a `configuration::ConfigValue<T>`. Read a snapshot with `auto v = X.get();` and keep `v` in a
  local while using it. Never write `for (auto& e : *X.get())`, which dangles.
- A scalar field (bool, number, enum) rebound at runtime is a `std::atomic<T>`.
- Plain fields are only for values bound before other threads read them (e.g. `DatabaseConfig`, only read at startup). A later runtime reader
  of such a field needs `std::atomic`/`ConfigValue` first.

**Game server config classes** bind with `AION_BIND(p, "gameserver.key", FIELD, "default")` and `AION_BIND_PATTERN(p, "pattern", FIELD)` from
`configs/detail/Bind.h` (the macros are the hook for the later `//configure` introspection). Every field is `std::atomic<T>` or
`ConfigValue<T>`, with no startup-only exceptions (rationale in `configs/detail/ConfigSupport.h`).

Supported field types: all integer types, `float`, `double`, `bool`, `char16_t`, `std::string`, enums (magic_enum), `std::optional<T>`,
`std::vector`/`std::set`/`std::unordered_set` (comma-separated), maps (for `bindPattern`), `std::filesystem::path`, `utils::InetSocketAddress`,
`std::regex`/`std::wregex` (use `std::wregex` where Java counts characters, e.g. name patterns), and `const std::chrono::time_zone*`.
Server-specific types (e.g. `CronExpression`) are added by specializing `configuration::transformers::PropertyTransformer<T>`.

## Database (JDBC → `aion::commons::database`)

DAOs keep their structure. `try-with-resources` becomes RAII:

```cpp
auto con = DatabaseFactory::getConnection();                         // returned to the pool on destruction
auto st = con->prepareStatement("SELECT * FROM account_data WHERE id = ?");
st->setInt(1, id);                                                    // 1-based like JDBC
auto rs = st->executeQuery();
if (rs->next()) {
	account.setName(rs->getString("name"));
}
// DB helpers with lambdas:
DB::insertUpdate("UPDATE account_data SET last_ip = ? WHERE id = ?", [&](PreparedStatement& st) {
	st.setString(1, ip);
	st.setInt(2, accountId);
	st.execute();
});
```

- Java `Statement.addBatch(sql)` with plain SQL → `PreparedStatement::addBatch(std::string_view sql)`.
- `rs->getString` on DATETIME/TIMESTAMP/TIME columns formats like Connector/J (`yyyy-MM-dd HH:mm:ss[.fff]`, `HH:mm:ss`).
- Game server: `GameServer::main` calls `DatabaseFactory::init(DatabaseFactory::gameServerOptions())`, which requires a socket timeout > 0
  (`database.socket_timeout`, 60 s by default). The login server keeps `init()`.
- NULL: numeric getters return 0 and `wasNull()` is true. `getString`/`getBytes` return empty values. Use `getObject<std::string>()`
  (`std::optional`) where Java distinguishes `null`. `getTimestamp`/`getDate` return `std::optional`.

## Network packets

- Connections derive from `network::AConnection<TServerPacket>` and are always created with `std::make_shared` (in the `ConnectionFactory`).
  Java's `getSendMsgQueue()` is the protected `sendMsgQueue`, and `writeData` already runs with `guard` held.
- Client packets derive from `packet::BaseClientPacket<TConnection>` and are handed to `PacketProcessor::executePacket` as `std::unique_ptr`.
  `readD/readH/readC/readS/readB/...` keep their Java names and underflow semantics. `BaseClientPacket` needs the complete connection type
  only where `setConnection()` is called (it binds the connection's `toString` there), so a server-specific packet base header may
  forward-declare its connection (`AionClientPacket.h`).
- Server packets derive from `packet::BaseServerPacket`. Their write methods take the target buffer: `writeD(buf, value)`, `writeS(buf, text)`.
  A packet has no buffer member, so one instance (`std::shared_ptr`) can be broadcast to many connections at once.
- `readS`/`writeS` convert between UTF-16LE on the wire and UTF-8 `std::string`.
- In `processData`, attach client packets to their connection with `packet->setConnection(sharedFromThis())`. `sharedFromThis()` returns the
  concrete connection type; never `static_pointer_cast` `shared_from_this()` by hand.
- Outbound connections (Java `SocketChannel.open` + `dispatcher.register` + `initialized()`):
  `lsCon = std::make_shared<LoginServerConnection>(nioServer.openSocket(address), nioServer); nioServer.registerConnection(lsCon);`
  Assign the field between the two calls, as Java does.
- Java `nioServer.connect(executor)` → `nioServer.connect([](std::function<void()> task) { ... })` adapting the server's executor, or
  `connect()` to use the server's own disconnect threads. Pass enough threads if `onDisconnect` does blocking database work (login server).
- A `ConnectionFactory` may reject a client with `close(closePacket)` and still return the connection; returning `nullptr` closes the socket
  without a packet.

## Servers: testing and running

- Server executables accept `-Dkey=value` arguments that override config properties (login server: `LoginServer::main`).
- `aion_game_server` runs with the Java module directory `game-server/` as working directory (`./config`, `./log`). Its `-Dkey=value`
  overrides are layered over `mygs.properties` through the event-properties layer of `Config::load`, so they survive reloads.
- Database tests are opt-in through environment variables and skipped without them: commons DB integration tests need
  `AION_TEST_DATABASE_URL=jdbc:mysql://127.0.0.1:3306/aion_cpp_test` and `AION_TEST_DATABASE_USER=root`; login server DB tests
  `AION_TEST_LS_DATABASE_URL`; `gs.smoke.startup` `AION_TEST_GS_DATABASE_URL` (e.g.
  `jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8`, optional `AION_TEST_GS_DATABASE_USER`/`_PASSWORD`, default root without
  password).
- CTest labels: `realdata` marks tests that read the Java tree (real-data GoogleTest cases, `gs.chunks.consistency`, the `gs.smoke.*` tests and
  the whole `tools.gen`, `tools.oracle`, `tools.porting` and `tools.xmlgen` suites), so `ctest -C Debug -LE realdata` is the fast run without
  the Java checkout. `smoke` marks the tests that start `aion_game_server` against the test database. `geo` marks `gs.smoke.startup_geo`
  alone - the one run with `gameserver.geodata.enable=true`, about 3 minutes and 5 GB, which is the only automated coverage of any zone
  handler; `ctest -L geo` runs it on its own and `-LE geo` drops it.
- Milestone tests do not skip silently any more (stage 3): `gs.smoke.startup`, `gs.smoke.startup_geo`, `gs.m4.check_static_data` and
  `gs.scenario.m5a` FAIL when a prerequisite (database URL, Python, the Java checkout) is missing, and the message names what to set. A run
  without those prerequisites is opted out explicitly with `-DAION_GS_ALLOW_MILESTONE_SKIP=ON` or `AION_GS_ALLOW_MILESTONE_SKIP=1`, which
  turns the failure back into a skip. Before that a default `ctest` reported the milestone green without having run it.
- Server state that is static in Java (controllers, tables, singletons) stays static. Tests therefore use unique account names and client IPs,
  restart the network component after config changes, and take the cross-process database lock
  (`tests/support/LoginServerTestDatabase.h`: `lockForProcess()`/`recreateSchema()`) before touching a shared test schema.
- Client packets in tests: the "unknown/random" trailing fields of Java `CM_*` readImpls are padding, checksum and the ignored last word.
  Test builders write only the leading fields and let the client crypto helper add the rest, so the sizes match the real client.

## Random numbers (`Rnd`)

`utils::Rnd` mirrors the Java API (`Rnd::get(min, max)` inclusive, `nextInt(bound)` exclusive, `chance()`, `nextFloat()`...).
`Rnd.get(list)` becomes `Rnd::get(vector)`. It returns a pointer to a random element, or `nullptr` if the vector is empty.
`Rnd::get` on a range of `int32_t` has Java `int[]` semantics: it returns the value and throws when empty. For Java `Rnd.get(List<Integer>)`
whose `null` result is checked, use `Rnd::getOptional(vector)`.
Tests seed the calling thread's generator with `Rnd::seedCurrentThreadForTests(seed)`.

## Game server: drafts, handlers and lint

Details: [design/handlers-and-porting-plan.md](design/handlers-and-porting-plan.md) and
[design/conventions-game-server.md](design/conventions-game-server.md). Header declarations (members, signatures, construction, statics,
packets) follow [design/hub-headers.md](design/hub-headers.md), the rules the frozen spine headers were written by.

**Spine headers** (frozen after S0c; changes go through header requests, hub-headers.md §14):
- Generic erasure: a Java generic whose type parameters all have a project bound is one non-template C++ class, and `fwd.h` declares it as a
  class (`CreatureController`, `GeneralTeam`); `AITemplate<T>` and unbounded utility generics stay templates.
- Object parameters are `X&`, or `runtime::Ptr<X>` only on evidence that Java passes or checks `null` (hub-headers.md §5.1); returns are
  `Ptr<X>`, part/owner/singleton accessors `X&`, factories `Ref<X>`.
- Visible objects are created with `VisibleObject::create<T>(...)` (a `CreateKey` passkey constructor plus `postConstruct()`), never `new`.
- A Java override whose body only casts `super.m()` is a non-virtual narrowing redeclaration with the narrower type; it does not make the
  base method virtual.
- No `__has_include` guards in game-server code (`skeleton.py --guards --freeze` in tools.gen).

**Chunk ownership** (design §2.2; syntax in `cpp/game-server/cmake/AionChunks.cmake`):
- `cpp/game-server/chunks.cmake` assigns every file to a chunk and is owned by the integrator; nobody else edits it.
- `python tools/porting/chunks.py owner <path>` (from `cpp/`, C++ or Java path) names the owner; `chunks.py check-ownership <chunk>
  <base>..<head>` checks a branch (it replaces the planned `check_ownership.py`). A chunk may change its own files, files leased to it
  (`LEASE`), its test directories (`TESTS`, `TEST_SUPPORT`, leased `TEST_SUPPORT`) and `docs/deviations/<chunk>.md`.
- Every `.cpp`/`.h`/`.ipp`/`.inc` below `src/`, `handlers/`, `generated/` and `tests/` needs exactly one owner: a new file fails configure
  until the manifest assigns it (ask the integrator).

**Generated drafts and forward headers** (`cpp/tools/gen/skeleton.py`):
- Forward headers are committed: `#include "aion/gameserver/<pkg>/fwd.h"`. Regenerate with `python tools/gen/skeleton.py --fwd --out
  game-server/src` after adding a Java class, after xmlgen regenerates or when a hand-written header changes a class key; the drift test in
  `tools.gen` fails otherwise.
- Every enum of `game-server/src` is generated by xmlgen (`generated/aion/gameserver/<pkg>/<Enum>.h`, nested `Outer_Inner.h`) and
  forward-declared with the underlying type of that definition (`std::uint8_t` up to 256 constants, else `std::uint16_t`). Drafts never define
  such an enum: a nested one becomes `using Inner = ::ns::Outer_Inner;`, a secondary top-level one an include of its generated header, and
  other files spell a nested generated enum `Outer_Inner` with its generated header. Explicit draft selectors of generated enums are refused.
- Java nested types stay nested (`Outer::Inner`). Private members of Java nested classes become public, because Java lets the whole
  top-level class use them. A nested class deriving from its outer class is defined after the outer class.
- Drafts follow hand-written C++ definitions (xmlgen shells, kernel ports): the runtime base comes from their base clause, and `override` is
  emitted only for methods their header or member blocks declare.
- Draft marker comments that reviewers resolve: `TODO(fieldmap)`, `TODO(signature)`, `TODO(callbacks)`, `TODO(enum)`, `TODO(logger)`,
  `TODO(xmlgen)`. Regenerating with `--draft` overwrites hand edits.

**Handler files and registration** (checked by `aion_gs_regscan`, rules in `cpp/game-server/tools/regscan/README.md`):
- Each handler `.cpp` registers its class with one marker line at namespace scope, in the package namespace of its directory:
  `AION_AI(Class, "name");`, `AION_INSTANCE_HANDLER(Class, mapId);`, `AION_ZONE_HANDLER(Class, "names"[, questId]);`,
  `AION_QUEST_HANDLER(Class, questId);`, `AION_ADMIN_COMMAND(Class);` / `AION_PLAYER_COMMAND` / `AION_CONSOLE_COMMAND`, and
  `AION_CLIENT_PACKET(CM_X);` (only in `network/aion/clientpackets`).
- Marker syntax: the whole marker on one line, starting the line and ending with `;`; literal arguments only (plain printable-ASCII string
  without escapes, zone names separated by single spaces; decimal `int` without sign, suffix or leading zero); never in a header, inside a
  class or function, in an `#if` block or a preprocessor directive.
- File rules (unity-safe): every declaration inside the package namespace (a keyword directory maps to `keyword_`, e.g. `quest/template` →
  `...::quest::template_`); no nested, other or anonymous namespaces; no namespace-scope `static`; no `using namespace` (the only exception is
  the quest prelude's `using namespace aion::gameserver::model::DialogAction;`); a type name defined once per namespace; only `.cpp` and `.h`.
- Handler files include the prelude of their category first: `ai/AiPrelude.h`, `instance/InstancePrelude.h`, `quest/QuestPrelude.h`,
  `zone/ZonePrelude.h`, `admincommands/AdminCommandsPrelude.h`, `playercommands/PlayerCommandsPrelude.h`,
  `consolecommands/ConsoleCommandsPrelude.h` (all under `aion/gameserver/handlers/`; `CommandPrelude.h` is only the PCH of the command
  library). Preludes provide the markers and `AION_UNPORTED`. A prelude never re-exports a name that a Java type of its category declares
  (checked by `tools/gen/tests/test_handler_preludes.py`).
- Unported bodies are `AION_UNPORTED();` (`aion/gameserver/runtime/base/Unported.h`, library `aion_gs_runtime_base`, namespace
  `aion::gameserver::runtime`; the wave-1 names in `aion::gameserver::handlers` stay available as using-declarations). It throws, so never put
  it in a `noexcept` function.
- Npc ids in `spawn(`/`sp(` calls stay literal, including ternaries: the QuestSpawnAnalyzer replacement scans the raw source text at build time.

**Concurrency lint waivers** (`tools/porting/lint_concurrency.py`; on the finding's line or the line above, reason mandatory, W0 otherwise):
`// confined: <reason>`, `// fieldmap: <reason>`, `// lockdep: <reason>`, `// quiescent-safe: <reason>`, `// lint: L5,L12 <reason>`.
`// fieldmap-class: <Java FQN>` on or above a class maps it to a Java class explicitly. A member spelled exactly like a `fieldmap.toml`
decision needs no waiver (write `// fieldmap.toml: <reason>` as a plain note). Not waivable: missing lock classes (`AION_LOCK_CLASS`),
`OwnerRef` members outside `OwnedPart` classes, and C++-only retaining members missing from `fieldmap.toml [cpp_members]`. CTest runs
`lint_concurrency.py --werror --cycles=core game-server/src`.

## Static data (game server)

Design: [design/static-data.md](design/static-data.md); binder contract: `dataholders/loadingutils/XmlBinding.h`.
- The XML runtime namespace is `aion::gameserver::xml` (adapters in `xml::adapters`). Game headers include only `XmlBindingFwd.h` (and
  `EnumTraits.h` for enums); binder headers are included only by binder translation units. Generated enums use `xml::EnumTraits` for names
  and lookups instead of magic_enum.
- A behaviour class header includes the generated prelude `X.xml.h` before the class, and `#include "X.xml.inc"` is the first line of the
  class body, followed by an explicit access specifier.
- Every top-level behaviour class has a hand-owned shell in `game-server/src`, written once by `xmlgen.py scaffold --all` (S0a): the header
  always, the `.cpp` only where there are hooks or annotated setters (`AION_UNPORTED` stubs including `runtime/base/Unported.h`). A porter adds
  the `.cpp` with the first ported method. A hierarchy root derives `::aion::gameserver::runtime::StaticTemplate`. Rerun `scaffold --all`
  when a behaviour class is added; `tools/xmlgen/tests/test_real_tree.py` fails when one is missing.
- Nested behaviour classes are defined after the outer class. Nested enums and nested data-only classes are generated as `Outer_Inner`
  and aliased in the outer class; when the outer class is not generated, its hand-written header declares `using Inner = Outer_Inner;`
  (listed in `xmlgen-report.md`; `LegionHistoryAction.Type` is nested in an enum and stays `LegionHistoryAction_Type`). Members that clash
  with a method name get a trailing `_`. Java package-private becomes public.
- Bound objects must never move or be copied after binding: XmlIDs and IDREF slots record addresses. Class-level adapters bind their value
  type on the heap and store the target with `c.replaceSingle(o.member, std::make_unique<Target>(std::move(value)), e)`, never a plain
  assignment.
- An `@XmlElements` base class has a virtual destructor (static_assert). A deliberately ignored attribute is
  `static_cast<void>(value); c.ignoreAttribute(); return true;`.
- Hooks report errors and warnings with `LoadContext::fail`/`warn`, which add the file location.

## Python tools

- Python 3.12, standard library only. Each tool lives in `cpp/tools/<tool>` with tests in `tests/test_<tool>*.py` and a `tests/__init__.py`,
  so `python -m unittest discover -s tests -t .` works from the tool directory; `cpp/tools/CMakeLists.txt` registers that command as CTest
  `tools.<tool>`. Drift checks of test data outside `cpp/tools` need their own CTest (e.g. `gs.geomath.golden_drift`).
- Generators import the shared Java front end as `import javasrc` from `cpp/tools/gen` and pass declarations or `Span`s as resolution
  context. The `javasrc.py` module docstring is the API reference.
- Generated outputs are committed, deterministic (sorted, `\n` line ends) and have a `--check` (or `check`) mode that tests use as a drift check.

## Unported Java features

| Java | Replacement |
|---|---|
| `commons.scripting` (runtime compilation of `data/handlers`) | Handlers compiled into the binary and registered explicitly (see PORTING_PLAN.md › Future notes) |
| `DeadLockDetector` (JVM `ThreadMXBean`) | Not ported (see PORTING_PLAN.md › Future notes) |
| Class loading via config (`ClassTransformer`) | Name → factory registries where needed |
| `AionRejectedExecutionHandler` | Not ported (no thread pool executor in commons). The game server's `ThreadPoolManager` port must keep its policy: if the pool is not shut down, log a warning, then run the task in a new thread if the caller's priority is above normal, otherwise in the calling thread |

Intentional behaviour changes are listed in [DEVIATIONS.md](DEVIATIONS.md).
