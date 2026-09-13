# Intentional deviations from the Java server

This file lists behaviour that differs from the Java reference implementation on purpose. The code marks each one with a `Deviation:` comment.
Any other difference is a bug: code not listed here should behave like Java.

## General

| Area | Java | C++ | Reason |
|---|---|---|---|
| Scripting | Handlers are compiled from `data/handlers` at runtime and can be hot-reloaded | Compiled into the binary | No runtime compiler. See PORTING_PLAN.md › Future notes |
| `DeadLockDetector` | Detects JVM monitor deadlocks via `ThreadMXBean` | Not available | No C++ equivalent |
| Null strings | `String` may be `null` | `std::string` (empty) or `std::optional<std::string>` where null and empty differ | No null references |
| `ExitCode.ERROR` | `ERROR` | `ExitCode::ERROR_` | `<wingdi.h>` defines an `ERROR` macro |

## commons / utils

| Area | Java | C++ | Reason |
|---|---|---|---|
| `Rnd` | `L64X256MixRandom` split per thread. `get(List)` returns the element or null. Streams bound to the creating thread | xoshiro256++ split per thread (different sequences). `get(range)` returns a pointer (lvalue ranges) or `std::optional` (rvalue ranges). Ranges of `int32_t` return the value and throw when empty. Streams are lazy ranges | No null references. The exact Java sequence is not required |
| `NetworkUtils.checkIPMatching` | Octets and range bounds are parsed as signed bytes, so values above 127 throw | Parsed as 0-255. Malformed input returns false | Java bug |
| `NetworkUtils.toHex` | Text column of the last row only printed if `end == capacity` | Always printed | Java bug |
| `RunnableStatsManager` | Keyed by `Class`, Java names. Non-transitive string comparator | Keyed by `std::type_info`, C++ names (`network::aion::serverpackets::SM_X`). Strict weak ordering | `std::stable_sort` needs a strict weak ordering |
| `ExecuteWrapper` | `Runnable` executor | Template over callables / objects with `run()` | No `Runnable` |
| `UncaughtExceptionHandler` | Only the crashing thread dies. `OutOfMemoryError` exits with RESTART | An uncaught exception terminates the process: log, flush, then `quick_exit(RESTART)` for `bad_alloc`, otherwise `quick_exit(ERROR_)` (never `abort()`). Threads catching at their entry point keep Java's behaviour | C++ always terminates on uncaught exceptions; `abort()` can hang on a CRT dialog (Debug) or WER (Release) |
| `ByteBuffer` byte order | `allocate`/`wrap` default to big endian; `slice()` is always big endian | Little endian by default; `slice()` keeps the order. Big endian file readers (e.g. `GeoWorldLoader`) must call `order(BIG_ENDIAN_ORDER)` | Every Aion protocol is little endian |
| `StringUtils` case mapping | Full Unicode case mapping incl. final sigma | Simple case mapping for ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic; special cases İ→i̇ and ß→SS; no final sigma | No ICU; covers the name scripts the servers allow |
| `StringUtils::toUtf8` | Unpaired surrogates encoded as `?` | U+FFFD | Safer for display |
| `parseInt`/`parseLong` (`Numbers.h`) | `Character.digit` accepts all Unicode digits | ASCII digits only | Negligible edge case |
| `InetSocketAddress` bytes | Resolved once at creation (config load) | `resolveAddressBytes()` resolves on each call (IPv4 first) | The host is kept unresolved |
| `PriorityThreadFactory` | Unstarted `Thread` in a `ThreadGroup` | Started `std::jthread`. Priority mapped like HotSpot on Windows | No unstarted threads or thread groups |
| `VersionInfo` | Reads jar manifests; "for Java 25" | Reads the CMake-generated `aion/BuildInfo.h`; "for C++23 (MSVC x.y.z)" | No jars |
| `SystemInfo` | JVM, heap max/allocated/used | Compiler, available CPUs (affinity), physical RAM, committed and working-set memory | No JVM heap |

## commons / logging

| Area | Java | C++ | Reason |
|---|---|---|---|
| Configuration | `config/logback.xml` read by logback | `Logging::init(Config)` builds the same appenders in code; the server passes webhook URLs and time zone from its properties | No logback |
| Console | Always ANSI colors; `consoleEncoding` | Colors only on terminals with VT support; Windows console output as UTF-16 | Clean redirected output |
| Exceptions in patterns | Throwable separate from the message (`%msg` vs `%ex`) | Same output: the Logger passes the exception position to layouts through a thread-local; plain spdlog sinks see the message followed by a newline and the exception | spdlog's `log_msg` has no throwable field |
| Log archive on startup | `ZipOutputStream` with zlib, `\` entry separators, archive time as entry time | Own DEFLATE encoder (slightly larger), `/` separators, file modification time | zlib not linked; `/` is what the ZIP spec requires |
| `DiscordChannelAppender` | Separate async appender; regex separator; NPE on null name/avatar; `Retry-After` read as ms; no HTTP timeout | One async sink; literal separator; missing fields left out; `Retry-After` in seconds (per Discord docs); 30 s timeout; non-finite/negative rate-limit headers count as absent, capped at one year | Fixes Java bugs; a hanging request cannot block the queue; no UB float→int conversion |

## commons / configuration

| Area | Java | C++ | Reason |
|---|---|---|---|
| Field binding | Reflection over `@Property`/`@Properties` fields (incl. superclasses); runtime transformer registry | Explicit `bind`/`bindPattern` calls per config class; compile-time `PropertyTransformer<T>` specializations | No reflection |
| Error/debug messages | "Error modifying field X of class Y" | "Error modifying field for property <key>" (same cause chain) | No field names without reflection |
| `bindPattern` | HashMap order decides duplicates; unmatched group gives a null key; java.util.regex | Sorted key order (last wins); unmatched group gives `""`; ECMAScript `std::regex` | Deterministic; no null |
| Empty enum/regex value | Field becomes null | `std::optional<E>` / `std::optional<std::regex>` → nullopt; plain type → `TransformationException` | Value types cannot be null |
| Regex syntax | java.util.regex on UTF-16 | ECMAScript `std::regex` (UTF-8 bytes) or `std::wregex` (UTF-16 code units, Java-like counting) | No java.util.regex |
| `ZoneId` | Any fixed offset | Region IDs, Z/UTC/GMT/UT, whole-hour offsets −12..+14 | `std::chrono::time_zone` has no arbitrary offsets |
| `File` | Separators normalized | `std::filesystem::path` kept as written | Cosmetic only |
| `InetSocketAddress` | Host resolved eagerly; any spelling of the unspecified address is `isAnyLocalAddress()` | Kept unresolved until bind/connect; unscoped spellings of the unspecified address are normalized to `::` / `0.0.0.0` | `utils::InetSocketAddress` design |
| Runtime config reloads | Plain static fields; reference stores are atomic, so reloading is memory safe | Fields rebound while other threads read them are `ConfigValue<T>` (atomic `shared_ptr` snapshot) or `std::atomic<T>`; `CommonsConfig::RUNNABLESTATS_ENABLE` is `std::atomic<bool>` | Assigning strings/containers while they are read is UB in C++ (confirmed by a crashing stress test) |
| Time zone database | Java ships its own tz data; `ZoneId.systemDefault()` never fails | `std::chrono` tzdb needs the Windows ICU (`icu.dll`: Windows 10 1903 / Server 2022+). Without it, or without a determinable system zone, config loading fails with a clear `IllegalStateException` | No zone object exists without the database; a silently wrong zone would shift all schedules |
| Number/boolean parsing | Unicode digits; Unicode case folding | ASCII only | Negligible edge cases |
| `Properties` | Synchronized Hashtable; mutable live defaults; lone surrogates kept; platform line separator in `store` | Unsynchronized; `shared_ptr<const Properties>` defaults; lone surrogates → U+FFFD; `\n` | Value semantics; UTF-8 strings |
| `PropertiesUtils.loadFromDirectory` | File system order; symlinked start dir not followed | Sorted by path; symlinks followed | Deterministic load order |
| `commons.script_compiler.caching.enable` | Config field | Validated but unused | No script compiler |

## commons / database

| Area | Java | C++ | Reason |
|---|---|---|---|
| Prepared statements | Connector/J emulates them client-side (text protocol) | Server-side prepared statements (binary protocol) | Natural Connector/C API; differences negligible for DAOs |
| NULL strings/bytes | `getString`/`getBytes` return null | Empty value + `wasNull()`, or `getObject<std::string>()` → `std::optional`; `getTimestamp`/`getDate` return `std::optional` | C++ convenience |
| ResultSet lifetime | Closed with its statement | Fully materialized; stays valid after statement/connection are gone | RAII, cheap scrolling |
| `ResultSet.relative` | Cursor not clamped | Clamped to before-first/after-last | Connector/J bug |
| Timestamp precision | Nanoseconds | Milliseconds | No DAO uses sub-millisecond values |
| Credentials | Empty HikariConfig user/password override the URL | Empty values count as unset (URL credentials apply) | Missing and empty keys are indistinguishable |
| `characterEncoding` | Any Java charset | Always utf8mb4 (other values warn) | All strings are UTF-8 |
| JDBC URL | Multi-host/load-balancing URLs | First host only | Not used |
| Host resolution | Java prefers IPv4 addresses | Resolved by the port, IPv4 first, then each address tried in order | Windows resolves `localhost` to `::1` first (2 s per connection against an IPv4-only server) |
| `Transaction` | Leaks statements/connection if not committed | Owns its pooled connection; uncommitted work is rolled back by the pool | RAII |
| Pool | HikariCP fills the pool and retires connections in background threads | Created on demand in the calling thread; maxLifetime checked on borrow/return | Simpler, no background thread |
| `Connection.isValid(timeout)` | Honours the timeout | Socket send/receive timeouts set to the timeout around `mysql_ping` (bounds each network wait, not the whole call); the pool validates with `isValid(5)` | No per-call timeout in Connector/C |
| `sslMode=REQUIRED` | Rejected before the authentication response is sent | Same `SQLException` (08001), but checked right after `mysql_real_connect`, i.e. after authentication | Connector/C has no hook between greeting and authentication |
| System time zone without tzdb | `TimeZone.getDefault()` always available | Without `icu.dll` the connection's system zone falls back to the Windows dynamic time zone rules (with a warning); determined once on first use | Falling back to UTC would shift all stored timestamps |
| Before `DatabaseFactory.init` | NullPointerException | `SQLException("DatabaseFactory is not initialized")` | No NPE |
| Not supported | CallableStatement OUT params, updatable result sets, a standalone `Statement` class (plain-SQL batches are covered by `PreparedStatement::addBatch(sql)`) | Not ported | Unused by the servers |

## commons / network

| Area | Java | C++ | Reason |
|---|---|---|---|
| Threads | 0 → one accept+IO dispatcher; N → N IO dispatchers + 1 accept dispatcher | `max(1, N)` Asio IO threads doing both | No separate accept thread needed; 0/1 stay single-threaded as the game server expects |
| `NioServer.shutdown` | Logs counts and returns; stragglers stay connected; threads keep running | Same log lines, then remaining connections are disconnected, IO threads joined, all `onDisconnect` callbacks awaited | C++ must stop its threads; guarantees `onDisconnect` for every connection |
| Oversized packets | Declared size > read buffer hangs the connection (selector spins) | Warning and disconnect | Java bug |
| `writeData` exceptions | Logged; half-written buffer corrupts the stream | Logged and disconnected | Java corrupts the stream |
| Pending close | Disconnect when the queue is empty; SO_LINGER(true, 10); read interest removed | Disconnect when the close packet was handed to the socket, a write failed, or after 2 s; shutdown + close without linger; received data is read and discarded meanwhile | Deterministic close packet delivery; draining avoids an RST that could discard the close packet |
| Disconnect executor | `connect(Executor)` required; `shutdown` does not wait; a rejecting executor loses `onDisconnect` | `connect(DisconnectExecutor)` or `connect()` with own threads (default 2); `shutdown` waits for callbacks; if the executor throws, `onDisconnect` runs on the IO thread | No `Executor`; C++ must join threads; every connection gets its `onDisconnect` |
| Outbound connections | `SocketChannel.open` + `new Connection` + `dispatcher.register` + `initialized()` | `openSocket(address)` + `make_shared<Connection>` + `registerConnection(con)` (registers, calls `initialized()`, starts IO) | Asio has no per-connection dispatcher; keeps Java's order |
| `sendPacket`/`close` before registration | Act on the selection key immediately | Deferred until IO starts after `initialized()`; a connection is never disconnected before it is registered | Lets a `ConnectionFactory` reject a client with `close(packet)` without racing registration |
| Write batching | One socket write per packet | Consecutive packets batched into one write (same bytes and order) | Fewer system calls |
| `initialized()` | May race with the first read; exceptions leave the connection registered | Always before the first read; exceptions close the connection | Deterministic |
| `PacketProcessor` | Linear scan of one global list; no shutdown; exceptions kill workers | Per-connection queues + ready list (same guarantees and checker rules); `shutdown()`; exceptions logged, workers continue | O(1) scheduling; C++ must join threads |
| `PacketProcessor` executor | `Executor` receiving a `Runnable` | `std::function<void(ClientPacketBase&)>` that calls `packet.run()` | No `Runnable` |
| `BaseServerPacket.buf` | Mutable field set during `write()` | No buffer member; static write helpers take the target buffer | One instance can be broadcast concurrently |
| `BasePacket.getPacketName` | final, `getSimpleName()` | virtual, simple name from `typeid` | Compiler names can be overridden |
| `ServerCfg.getIP` | Resolved address | Configured host string (resolved at bind/connect, IPv4 first) | Unresolved `InetSocketAddress` |
| Accept errors | Retried on next select | Logged, retried after 100 ms | No busy loop on e.g. EMFILE |
| Misc | `readB(-1)` → NegativeArraySizeException; `sendPacket(null)` → NPE later; `Object.toString()` | `IllegalArgumentException`; null packets ignored; `toString()` = "<SimpleClassName> <ip>" | No equivalents / better logs |
