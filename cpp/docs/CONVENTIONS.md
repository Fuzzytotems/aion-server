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
- **Monitors are reentrant.** Use `std::recursive_mutex` when a `synchronized` method can re-enter the same object; otherwise `std::mutex`.
  `volatile` → `std::atomic`. `wait`/`notify` → `std::condition_variable`.

## Ownership and lifetimes

- Default to value semantics and `std::unique_ptr`. Use `std::shared_ptr` only where the Java object is genuinely shared across threads
  or async callbacks with unclear end of life (network connections, packets queued for sending).
- Network connections are `std::shared_ptr` (`enable_shared_from_this`). Every async operation holds a reference.
- Game object model ownership is decided at the start of phase 4 and documented here then.
- Java singletons (`getInstance()`) → function-local static (`static T& getInstance()`), or namespace-scope functions for static utility classes.

## Errors and exceptions

- Exceptions stay exceptions. Base class: `aion::commons::utils::Exception` (`aion/commons/utils/Exception.h`; derives from
  `std::runtime_error`, captures a `std::stacktrace` and an optional cause). Common Java types exist there too
  (`IllegalArgumentException`, `IllegalStateException`, `UnsupportedOperationException`, `IndexOutOfBoundsException`, `IOException`).
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
| `ExecuteWrapper`, `RunnableStatsManager` | same names in `utils::concurrent` |

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
- Plain fields are only for values bound before other threads read them (e.g. `DatabaseConfig`, only read at startup).

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
- NULL: numeric getters return 0 and `wasNull()` is true. `getString`/`getBytes` return empty values. Use `getObject<std::string>()`
  (`std::optional`) where Java distinguishes `null`. `getTimestamp`/`getDate` return `std::optional`.

## Network packets

- Connections derive from `network::AConnection<TServerPacket>` and are always created with `std::make_shared` (in the `ConnectionFactory`).
  Java's `getSendMsgQueue()` is the protected `sendMsgQueue`, and `writeData` already runs with `guard` held.
- Client packets derive from `packet::BaseClientPacket<TConnection>` and are handed to `PacketProcessor::executePacket` as `std::unique_ptr`.
  `readD/readH/readC/readS/readB/...` keep their Java names and underflow semantics.
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

## Unported Java features

| Java | Replacement |
|---|---|
| `commons.scripting` (runtime compilation of `data/handlers`) | Handlers compiled into the binary and registered explicitly (see PORTING_PLAN.md › Future notes) |
| `DeadLockDetector` (JVM `ThreadMXBean`) | Not ported (see PORTING_PLAN.md › Future notes) |
| Class loading via config (`ClassTransformer`) | Name → factory registries where needed |
| `AionRejectedExecutionHandler` | Not ported (no thread pool executor in commons). The game server's `ThreadPoolManager` port must keep its policy: if the pool is not shut down, log a warning, then run the task in a new thread if the caller's priority is above normal, otherwise in the calling thread |

Intentional behaviour changes are listed in [DEVIATIONS.md](DEVIATIONS.md).
