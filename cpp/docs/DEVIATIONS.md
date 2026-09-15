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

## login-server / encryption (`network/ncrypt`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| `CryptEngine.decrypt` checksum | Loops `i < length - 4` instead of `offset + length - 4`: packets not at read offset 2 are only partly verified | Always verifies the whole packet (identical to Java at offset 2, the first packet of a read) | Java bug; spans have no offset |
| `CryptEngine` threads | Cipher shared by read (decrypt) and write (encrypt/rekey) paths without synchronization | Internal mutex | Data race is UB |
| Invalid encrypt input / empty keys | AIOOBE after partial writes; broken cipher state | `IndexOutOfBounds`/`IllegalState`/`IllegalArgumentException` before anything is modified | Exception safety; unreachable with valid packets |
| `KeyGen` | NPE/null before `init()`; key pair array overwritten on re-init | `IllegalStateException` before `init()`; `shared_ptr<const EncryptedRSAKeyPair>` behind a mutex, handed-out pairs stay valid | No null; memory safety |
| Blowfish session key | JCE `KeyGenerator("Blowfish")` (128 bit) | 16 bytes from OpenSSL `RAND_bytes` | No JCE; equivalent CSPRNG key |
| RSA login data decryption | Inline `Cipher` per 128-byte block in `CM_LOGIN` | `EncryptedRSAKeyPair::decrypt(span)` → `optional` (nullopt where Java catches `GeneralSecurityException`) | Reusable API; same result |

Kept on purpose because it is on the wire: `AionServerPacket` passes `payload size - 2` to `encrypt`, so for payload sizes with `size % 8 == 5`
the checksum overwrites the last payload byte (first packet: `size % 8 == 1` loses it to the XOR key). The checksum check ignores the last
word, so client packets carry the checksum in the last-but-one word; the "unknown/random" trailing fields of `CM_*` packets are that padding.

## login-server / data layer

| Area | Java | C++ | Reason |
|---|---|---|---|
| Unknown config property warnings | Removes the keys referenced by the logback.xml in use | Always removes `Logging::getPropertyKeys("loginserver")` | No logback.xml |
| Logging settings | logback reads `logging.properties` and `myls.properties` itself | `Config::loadLoggingConfig()` reads the same files for `Logging::init` | Logging is configured in code |
| `Account` / `AccountTime` | Unsynchronized fields; `AccountTime` shared and changed in place | One mutex per account; getters return copies; `modifyAccountTime`/`modifyAndStoreAccountTime` do atomic read-modify(-store) | Data races are UB; stored copies must not overwrite newer state (e.g. a ban penalty) |
| `AccountDAO` name column | `static final` chosen at class init | Chosen from `Config::useExternalAuth()` per call | Config must be loaded first; same result |
| `AccountDAO.getLastIp` | `null` for NULL (CM_BAN then throws an NPE: no ban, kick or response) | `""` (CM_BAN bans the given IP, kicks and answers) | Java bug |
| `BannedHddDAO.load` | Zero date is stored as null, SM_HDDBAN_LIST later throws an NPE | Row skipped with a warning | No null timestamps |
| Nullables | `null` accounts, bans, strings, timestamps | `shared_ptr` (nullptr), `optional`, empty strings where the column is `NOT NULL` | No null references |

## login-server / protocols, controllers, startup

| Area | Java | C++ | Reason |
|---|---|---|---|
| Game server packet execution | Cached thread pool: packets of one game server run concurrently and unordered | `PacketProcessor<GsConnection>(4, 8, 50, 3)`: per game server in receive order; `CM_GS_PONG` runs directly on the IO thread | Memory safety and ordering; a pong behind a slow packet must not trigger the ping timeout |
| Executors and shutdown | Static executors, shut down in `onServerClose` | `NetConnector` owns packet processors, ping scheduler and 8 disconnect threads; joined on shutdown; restartable | C++ must join threads; in-process tests |
| `LoginConnection` session id | `Object.hashCode()` | Random int in [1, INT_MAX] | No identity hash |
| `LoginConnection.onDisconnect` | Removes the account id from `accountsOnLS` whichever connection is mapped | Only if it maps to this connection; logins that finish after a disconnect clean up after themselves (`AccountAttachScope`) | Java bugs: a kicked client could remove the new login; stale connections kept accounts "logged in" |
| `GameServerInfo` accounts | Added unconditionally; connection and accounts cleared separately on disconnect | Added only while that connection is active; connection + accounts cleared atomically | A packet finishing after a disconnect left the account "already logged in" until restart |
| `GameServerTable` | HashMap order; unsynchronized registration; NPE races on kick | Ordered by id; registration serialized; kick skipped if the game server just left | Determinism; data races |
| `CM_BAN` | Penalty only in the DB unless the account is on a game server (a later logout of the LS/reconnecting copy lifts it) | Penalty also set on the in-memory account found on the LS or in the reconnect state; `kickAccount` also drops pending fast reconnects | Java bug |
| `SM_ACCOUNT_AUTH_RESPONSE` | Looks the account up again while writing (NPE if it left) | Account time snapshot passed to the constructor | Race between packet processing and IO write |
| `SM_SERVER_LIST` | NPE without character counts; endless byte loop at id 127 | Missing counts = empty; int loop counter | Java bugs |
| `CM_PTRANSFER_CONTROL` | Service call in `readImpl` (IO thread) | In `runImpl` (packet processor) | No database work on IO threads |
| `CM_ACCOUNT_LIST` | Negative/huge counts → exceptions or OOM | Invalid count throws, packet not executed | Memory safety |
| Ban list packets | Iterate live unsynchronized maps while writing | Copy taken under the controller mutex | Data race |
| `ExternalAuth` | No timeout; fastjson2 | cpr with 30 s timeout, no redirects; nlohmann-json mimicking fastjson's lenient parsing | A hanging auth server must not block packet threads |
| Scheduled tasks / `PingPongTask` | Exceptions silently stop periodic tasks; NPE if the GS info is gone | Exceptions logged, task stops; "Gameserver #null connection died" | No exceptions escaping threads |
| NPE cases | `NullPointerException` | `IllegalState`/`IllegalArgumentException` with the same observable result (logged, no response) | No NPE |
| Startup failure | Main thread dies, started threads keep the JVM alive | Logged, started components shut down, exit code `ERROR_` | The process must not hang |
| Command line / shutdown hook | No arguments; JVM shutdown hook | `-Dkey=value` overrides config properties (C++ addition); `SetConsoleCtrlHandler` (Ctrl+C, close, logoff, shutdown) or SIGINT/SIGTERM | Run against scratch databases; no JVM hooks |

## game-server / runtime kernel

The free-threaded runtime (design D1) replaces JVM guarantees; its design-level deviations are listed in
[design/runtime-architecture.md](design/runtime-architecture.md) §18 and apply as they are implemented. Behaviour-visible deviations of the
kernel implemented so far:

| Area | Java | C++ | Reason |
|---|---|---|---|
| Object lifetime | Garbage collector | Atomic intrusive `Ref<T>`, borrows valid until the task ends, epoch Reclaimer frees later; C++-only cycle breakers; zombie breaker cuts edges of objects out of the world > 30 min with a warning (D7) | No GC |
| Executors after shutdown | `AionRejectedExecutionHandler` drops tasks silently | Submissions are cancelled and dropped (a `get()` on them does not block forever) | Deterministic shutdown |
| Periodic tasks | `RunnableWrapper` logs exceptions, the task keeps running | Same; runs more than 10 periods (and ≥ 2 s) behind are coalesced into one and realigned | Avoid catch-up storms (e.g. after a debugger pause) |
| Cancelled tasks | Captured objects stay referenced until the queue drops the task | Captures are released immediately on cancel | Earlier reclamation |
| `getDelay()` | Only on scheduled futures | Returns 0 for tasks that are not scheduled | `Future` is one type |
| Rejection policy (instant pool) | Caller thread priority (Java) | Recorded Java-style thread priority of kernel threads | Portable |
| LS/CS link packets | Unordered on the general pool | In order per link (`SerialExecutor`) | Ordering |
| Object IDs | Lowest free ID reused immediately (after GC for auto-release objects) | Monotone cursor wrapping at 2^27, released IDs quarantined ≥ 300 s (also across a wrap); a double release while quarantined warns | ABA safety for handlers holding IDs |
| Cron (Quartz subset) | Quartz: L/W/# supported; misfires within 60 s catch up one by one; searches up to +100 years | L/W/# rejected (unused); missed fire times always collapse into one run; nonexistent local times skipped, ambiguous ones fire once (earlier instant); stricter parser; searches stop after 2299; `findJobs` by exact type | Only the used subset; deterministic |
| Deadlock detection | `DeadLockDetector`: dump and exit RESTART | Watchdog: lock wait-graph cycle confirmed in two consecutive checks, dump (Windows minidump with all stacks) and keep running (D5); STALL measured since the task's last `quiescentPoint`; long-running/startup/main kinds exempt | D5; avoid false positives |
| `synchronized` | JVM monitor, not fair; `wait`/`notify` | Reentrant `Monitor` with eventual fairness (starvation mode after 1 ms); no `wait`/`notify` (unused by the server) | Prevent starvation |
| `StampedLock` / fair `Semaphore` | Queue-based, writer preference / strict FIFO | No writer preference (avoids EffectController's nested read-lock deadlock); fair semaphore approximate | Simpler; unused features omitted |
| Plain collections in shared objects | Unsynchronized, `ConcurrentModificationException`, may corrupt | Internally synchronized shims, snapshot iteration, no CME; `HashMap` iteration in insertion order; modifying a collection from its own element's equals/hashCode/comparator throws `IllegalStateException`; `ArrayList.sort` with a throwing comparator leaves the list unchanged | Memory safety |
| `HashMap`/`TreeMap` compute callbacks | Any structural change → CME | Same-key recursive update throws `IllegalStateException("Recursive update")` (like CHM); other keys may change | Consistent with CHM |
| `ConcurrentHashMap` | Per-bin locks; `snapshot`-free weakly consistent iteration | 16 stripes with reentrant stripe Monitors for callbacks; lock-free weakly consistent reads and iteration; nested writes to another key of the same map inside a compute callback can deadlock across stripes, so such Java sites are ported without the nesting (design §21, lint L20) | Java's CHM contract forbids them |
| `ConcurrentLinkedQueue`/`Deque` | Lock-free | Monitor-guarded (`isEmpty`/`size` lock-free) | Interior `remove(Object)` with epoch reclamation |
| `String` fields | Nullable | `Field<std::string>`: null equals empty | No null strings |

## game-server / configs

| Area | Java | C++ | Reason |
|---|---|---|---|
| `Config.load` concurrency | Unsynchronized: event start/stop and `//reload config` can overlap | Serialized by a `runtime::Monitor`; the event provider and file reading run outside the lock | D6 race fix: data race on commons' plain `DatabaseConfig` fields and the `CLIENT_CONNECT_ADDRESS` read-modify-write (`ConfigLoadTest.ReadersStayValidDuringConcurrentReloads`) |
| Missing key without default | Field stays null (e.g. `DISABLE_RANGE_CHECK_MAPS.contains` throws an NPE) | Field keeps its initial value: empty collection, `nullptr` or `std::nullopt` | No null references; the shipped config has every such key |
| `Config.load(allowedConfigs)` error | `IllegalArgumentException` naming the class | "Config bind function is not an allowed config" | Bind function pointers instead of `Class` objects |
| Event config properties | Read from `EventService` | `Config::setEventConfigPropertiesProvider`; no event properties until `EventService` is ported and registers | Not ported yet |
| Command line (`aion_game_server`) | No arguments | `-Dkey=value` overrides (C++ addition, S0a `main.cpp`) are registered as the event-properties provider, so they are layered over `mygs.properties`, warned like other unknown keys and kept across later `Config::load` calls; other arguments are warned and ignored. `EventService` (P5-12b) must merge them into its provider | Run against scratch databases (`gs.smoke.startup`) |
| Unknown property warnings | Removes the keys referenced by logback.xml | Always removes `Logging::getPropertyKeys("gameserver")` | No logback.xml (as in the login server) |
| Logging settings | logback reads `gameserver.properties`, `logging.properties`, `mygs.properties` | `Config::loadLoggingConfig()` reads the same files for `Logging::init` | Logging is configured in code |
| `RuntimeConfig` | – | C++-only config class after Java's `Config.CONFIGS` classes; keys in design/runtime-architecture.md §10, all optional | Runtime kernel settings |
| `database.socket_timeout` | No such key (only the URL's `socketTimeout`) | C++-only optional key in milliseconds (commons `DatabaseConfig`). Precedence: key, then URL `socketTimeout`, then the server default; the game server uses a 60 s default and rejects 0 (`DatabaseFactory::gameServerOptions()`); the login server keeps Java behaviour | Database calls run inline on pool threads and must end (runtime-architecture.md §2.6) |

## game-server / geo math (`geoEngine/math`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| `FastMath.sin/cos/tan/exp/log/pow` | HotSpot `Math` intrinsics | C runtime; may differ by up to 1 ulp. `asin/acos/atan/atan2` are bit-exact through the fdlibm port `geoEngine/math/StrictMath` | Intrinsics are not bit-reproducible in Java either; only rarely used paths depend on them (fromAngleAxis, angleRotation, rotateAroundOrigin, spherical conversions) |
| Constant names | `FastMath.FLT_EPSILON`, `DBL_EPSILON`, `Vector3f.NAN` | `FLOAT_EPSILON`, `DOUBLE_EPSILON`, `NOT_A_NUMBER` | `<cfloat>`/`<cmath>` macros |
| `Ray(Vector3f, Vector3f)` | Keeps the caller's vector objects | Copies them; in-place mutation through `getOrigin()`/`getDirection()` still works | Value types; GeoMap, AbstractCollisionObserver and BIHNode do not observe the difference |
| Null arguments | Null store/result parameters allocate; `set(null)` gives identity; Vector2f warns | Overloads without the parameter; no null branches | References cannot be null |
| Not ported | `FastMath.rand/nextRandomFloat/nextRandomInt`, FloatBuffer methods, `Vector2f` externalization, `getClassTag`, `Vector3f.create`, `Matrix4f.fromFrustum`/`set(float[][])`, `equalIdentity` | – | JVM-specific or unused |

## game-server / static data runtime (`dataholders/loadingutils`)

Design-level deviations of the static data design are listed in [design/static-data.md](design/static-data.md) (DEVIATIONS entries and
amendments §8) and apply as they are implemented. Implemented in wave 1:

| Area | Java | C++ | Reason |
|---|---|---|---|
| Merged cache | XmlMerger writes `./cache/static_data.xml` with CRC metadata | No merged file; imports are resolved and bound directly | The cache exists only for JAXB |
| Directory import order | `Files.find` order of the file system (NTFS: uppercase ordinal; Linux: readdir) | Depth-first, uppercase-ordinal names on every platform | Deterministic; equals NTFS (checked for all 12 directories) |
| Strict mode (tests, CI) | JAXB ignores unknown attributes and unexpected text; repeated single elements and duplicate XmlIDs: last wins; XmlMerger merges different root tags of a directory import | Errors: unknown attributes, non-whitespace text in object elements, repeated single elements and wrappers, duplicate XmlIDs, mixed root tags, unknown `<import>` attributes. Lenient mode (server runtime) warns once and keeps the JAXB/XmlMerger behaviour | XSD parity; finds data mistakes |
| Required values | JAXB never enforces `required = true` (the startup XSD check catches most) | Enforced in both modes, except the 16 `xmlgen.toml [unenforced_required]` entries the data violates | No XSD validation at runtime |
| Unknown enum constants | JAXB gives null | Error, except `[lenient_enums]` (`SkillTemplate.counterSkill`: `std::nullopt` and a warning) | Stricter parsing proven unreachable by the census |
| Numbers | Float overflow gives Infinity/0; an empty number gives 0 (JAXB) | Errors | Malformed data fails loudly |
| `LocalDateTime` | Signed or longer years, nanoseconds | 4-digit unsigned years, millisecond precision (sub-millisecond digits must be zero) | `local_time<milliseconds>`; the data uses neither |
| File encoding and form | XmlMerger reads files with `FileReader` and copies inline holder elements of `static_data.xml` | UTF-8 only; inline holder elements, default-namespaced elements (`xmlns="uri"`) and directory imports without `.xml` files are rejected | Not used by the data |
| XSD-only attributes | Ignored by JAXB | Consumed without a member (`[ignore_attributes]`, 5 attributes) | Same result |
| Abstract `@XmlElements` choices | Cannot be instantiated | Left out of the factories (`<buf>` → `BufEffect`) | Same result, found at build time |
| Bound private nested classes | Private | Public | `XmlBinding` is specialized at namespace scope |
| `Loaded N npc templates` | `npcData.size()` logged while `NpcData.init` may still run asynchronously (can show a smaller number) | `NpcData::init` runs inline; always 63,287 (P4-09) | Deterministic |

## game-server / network crypt and generated packet tables

| Area | Java | C++ | Reason |
|---|---|---|---|
| `Crypt.encrypt/decrypt` without a key | NullPointerException | `IllegalStateException` | No NPE |
| `EncryptionKeyPair.decrypt` of an empty packet | XORs the byte just past the packet | Returns false and touches nothing | Memory safety; the dispatcher never passes an empty packet |
| `SM_SYSTEM_MESSAGE` parameters | Formatted with `toString` in `writeImpl`; null strings allowed | Formatted when the packet is constructed; string parameters are `std::string_view` (no null) | Generated factories; no null references |
| `Object...` parameters of `SM_SYSTEM_MESSAGE`, `SM_QUESTION_WINDOW`, `SM_CLOSE_QUESTION_WINDOW` (S0c) | Formatted in `writeImpl`; a `null` element prints `null` | Formatted into strings at construction; a null parameter is written as an empty string (sysmsg.py contract) | No `Object` in C++; no caller passes null |
| Packet members iterated from hash containers (S0c headers: `SM_NEARBY_QUESTS`, `SM_MOTION.activeMotions`, `SM_TOWNS_LIST`, `SM_SIEGE_LOCATION_INFO`, `SM_RECIPE_COOLDOWN`, `SM_RECIPE_LIST`) | `HashMap`/`HashSet` iteration order | `std::unordered_map`/`unordered_set` order may differ, so entries can be written in another order (`SM_RIFT_ANNOUNCE` and `SM_SHOW_BRAND` use `std::map`, which equals Java's order there) | Layout from `fieldmap.json`; an ordered container is a header request if the client cares |
| `STR_MSG_MERCHANT_PET_GET_SELL_ITEM` | Returns `AionServerPacket` | Returns `SM_SYSTEM_MESSAGE` | Uniform generated factories |
| Reserved factory names | `_STR_MSG_Heal_TO_ME`, `STR_RESURRECT_DIALOG__SKILL/ITEM/BIND/5MIN/30MIN`, `STR_RESURRECTOTHER_DIALOG__5MIN`, `STR_ERROR_CHANGE_WEAPON_SKIN__*` (3), `STR_SKILL_CAN_NOT_*__WHILE_IN_CURRENT_STANCE` (2) | Leading `_` stripped, `__` collapsed, trailing `_` (`STR_MSG_Heal_TO_ME_`); only `CM_EMOTION` calls two of them | Reserved identifiers in C++ (CONVENTIONS) |
| `DialogAction` | Final class with `int` constants; `nameOf` returns null; constant `NULL` | Namespace of `inline constexpr int32_t`; `nameOf` returns `std::optional<std::string_view>`; `NULL_`; `entries()` added | `NULL` is a C macro |

## game-server / handler registry

| Area | Java | C++ | Reason |
|---|---|---|---|
| Handler discovery | Class listeners over the compiled `data/handlers` at startup | Marker macros; `aion_gs_regscan` writes the tables at build time | Handlers are compiled in |
| Duplicate keys | AI names and map ids: `put`, last wins; zone names, quest ids: warning | Build errors | Deterministic registration; the Java tree has no duplicates |
| QuestSpawnAnalyzer | Regex scan of `data/handlers/**/*.java` at startup | Same pattern over the compiled-in C++ handler sources at build time (`npcIdsSpawnedByHandlers()`) | No sources at runtime |
| Unported code | – | `AION_UNPORTED()` throws `UnportedException` (an `UnsupportedOperationException`) and logs the site once | Partial port links from day one |

## game-server / object model (planned member mapping, `fieldmap.toml`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| `Creature.ai` | `final` (replaced reflectively by `//ai set`) | `PartSlot<AbstractAI>` (RetireTo::OWNER); the retired AI stays alive with the creature | `//ai set` without reflection (runtime-architecture.md §18 item 9) |
| `WorldMapInstance.instanceHandler` | `final` | `Field<Ref<InstanceHandler>>`, detached in `destroyInstance` | Breaks the instance/handler cycle (§18 item 8) |

Declared in the spine headers (S0b/S0c) and effective when the owning chunk ports the body (design: runtime-architecture.md §5.3, §24):

| Area | Java | C++ | Reason |
|---|---|---|---|
| C++-only cycle breakers | Garbage collector frees unreachable cycles | Logout (`LogoutBreakers::run`, also after a throwing enter-world) clears, without notifications or packets: target (no `onTargetChanged`), kisk, the storage actors, stance and ride observers, IdianStone listeners of equipped items, observers and attack-calc observers. Deleting an object (`VisibleObjectController::onDelete`) clears its target; for Creatures the observers, the effect maps (without ending the effects) and the stat functions owned by effects; for Npcs the walker group. `Skill.removeObservers` also clears `firstTargetDieObserver`; `Effect.endEffect` also resets `designatedDispelEffect`; `GatheringTask` resets `gathererObserver`; `House.resetRegistry` clears the dropped registry's objects and decorations; `InstanceService.destroyInstance` detaches the handler and clears `startPos` and the registered team; `PlayerAllianceService.disband` clears the alliance's groups | No GC (D1); each is listed in `cycles.toml` |
| `HostileUpEffect.tempHate` | Template field written by `calculate` and read by `applyEffect` and its delayed task; concurrent casts of the same effect template overwrite each other's value | Per cast: `Effect::hostileUpTempHate` holds the value `calculate` computed for that effect | Static data is immutable; removes the cross-cast race |
| Run-time `BoundRadius` | `updateBoundingRadius` creates a new object per call | `BoundRadius::intern`: one immortal object per distinct radius, kept for the process lifetime | `PlayerCommonData` holds template pointers (`const BoundRadius*`) |
| `PlayerScript.LUA_SANDBOX_FIX` | Compressed script built by the static initializer (CompressUtil, zlib) | Null until CompressUtil is ported | A static initializer must not reach `AION_UNPORTED` |
| `hashCode` of `LegionHistoryEntry` / `PlayerScript` | Identity hash of the enum constant / of the byte array | Enum ordinal / address of the byte array | No identity hash in C++; only hash iteration order can differ |
| `EffectReserved.compareTo` ties | `hashCode() - o.hashCode()` of identity hashes (may overflow) | Equal positions ordered by object address | Consistent total order without overflow; only the order of distinct objects at the same position differs |
| `EffectController` passive map before the first passive effect | `Collections.emptyMap()`: a stray `put` throws `UnsupportedOperationException` | Null map: a stray `put` throws `NullPointerException` | One shared empty map would make every creature lock the same Monitor |
| `Collidable.collideWith` | Takes a `Collidable`; non-Ray arguments throw `UnsupportedCollisionException` | Takes `math::Ray&`; the branches do not exist | Every caller passes a Ray (a value type) |
