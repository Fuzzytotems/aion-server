# chat-server: deviations

The chat server port (`cpp/chat-server`, Java `chat-server`). The Java server handles the Aion clients with Netty 3.10.6 and the game server
with commons' NIO server; the C++ port runs both sides on commons' `NioServer`/`AConnection` and reproduces Netty's behaviour on top of it. Every
difference that can be observed from outside (wire, logs, timing, threads) is listed here; the code marks the non-obvious ones with a
`Deviation:` comment. Anything not listed behaves like Java.

## Client connections (Java: Netty pipeline, `network/netty`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| Transport | `NioServerSocketChannelFactory` (a boss thread plus `NIO_READ_WRITE_THREADS + 1` workers), `ServerBootstrap`, a `DefaultChannelGroup` holding the server channel | A commons `NioServer` with `NIO_READ_WRITE_THREADS + 1` IO threads and one listener; the connection factory is `LoginToClientPipeLineFactory::getPipeline` | No Netty. commons frames data exactly like `PacketFrameDecoder` (little endian uint16 size that includes itself) |
| Pipeline | `framedecoder`, `packetdecoder`, `packetencoder`, `executor`, `handler` | `ClientChannelHandler` is the connection: `processData` gets the frame from commons, copies it (Netty's `extractFrame` copies too), passes it through `LoginPacketDecoder` and hands `messageReceived` to the `ExecutionHandler`; `sendPacket` writes the packet into a new 16 KiB buffer on the calling thread and encodes it with `LoginPacketEncoder` | Same byte flow and threading as the Netty pipeline |
| Queued packets | The channel queues the 16 KiB `ChannelBuffer` the packet was written into until the socket took it | Only a copy of the encoded bytes is queued (`AbstractChannelHandler::write`; test `ClientChannelHandlerTest.SendPacketQueuesOnlyTheEncodedBytes`) | Memory: in Java a client that does not read pins 16 KiB per queued packet (an 18-byte `CM_CHAT_INI` queues 16 KiB). Same bytes on the wire |
| Malformed frames | Size &gt; 16384: Netty discards the frame, fires a `TooLongFrameException` (logged "Caught exception from netty") and keeps the connection. Size field &lt; 2: `CorruptedFrameException` after skipping the two bytes, connection kept. Size 2: the empty frame reaches `ClientPacketHandler`, whose `readByte` throws (logged) | commons logs a warning and disconnects in all three cases (test `ClientFramingTest.AFrameLongerThanTheMaximumClosesTheConnection`) | Malformed input only; commons' framing is shared by all servers and has no discard mode |
| `ExecutionHandler` | `OrderedMemoryAwareThreadPoolExecutor(10, 1 MB, 128 MB, 100 ms)`: events of a channel in order, 10 threads, idle threads end after 100 ms, queued bytes above 1 MB per channel or 128 MB in total block the IO thread | `pipeline::ExecutionHandler`: the same ordering (also for the connected and disconnected events), up to 10 threads started on demand (a thread for each channel that becomes ready while no idle thread is left to take it, like `ThreadPoolExecutor` below its core size) and kept until shutdown, no memory limits | No Netty. The limits are back pressure against a flooding client; the event queues are unbounded like every commons send queue, and there is no idle timeout before the login (Java has none either). A per-connection limit is a hardening proposal in docs/design/chat-server-port.md |
| Thread names | Netty workers "New I/O worker #n", executor threads "pool-N-thread-M" | IO threads "ReadWrite-n Dispatcher", event threads "ExecutionHandler-n" | Visible in log lines only |
| Socket options | `child.tcpNoDelay`, `child.keepAlive`, `child.reuseAddress`, `child.connectTimeoutMillis` 100 | commons: `TCP_NODELAY`, listener bound with commons' rules (`SO_EXCLUSIVEADDRUSE` on Windows); no `SO_KEEPALIVE` | commons sets its own options; keep-alive only detects dead peers after hours (the client pings every 10 s) |
| "Listening on ... for Aion game clients" | Logged by `NettyServer` | Logged by commons (logger `com.aionemu.commons.network.NioServer`), same text | The listener belongs to commons |
| `close()` | Closes the socket at once; writes still queued are dropped | commons `close()`: disconnects as soon as the queued packets were written (at most 2 s later). Data received after `close()` is dropped like in Java (`ClientChannelHandler::processData`, test `ClientChannelHandlerTest.FramesReceivedAfterCloseAreDropped`); events received before it still run in both | commons has no immediate close for subclasses. Only packets queued in the same moment differ (a broadcast racing with `CM_PLAYER_LOGOUT`) |
| `close(BaseServerPacket)` | Writes the packet object itself; `LoginPacketEncoder` cannot cast it (`ClassCastException`, logged by `exceptionCaught`) and the `CLOSE` listener closes the channel | Same observable result: the error is logged (a `ClassCastException` stand-in) and the channel closed, nothing is sent | Kept like Java; unused in both |
| Packet too big for the send buffer | `IndexOutOfBoundsException` from the fixed 16 KiB `ChannelBuffer` | `BufferOverflowException` from the 16 KiB `ByteBuffer` | Exception name only; both are thrown in the caller (a broadcast stops at that client, like Java; test `BroadcastTest.MessageTooBigForTheSendBufferReachesNobody`) |
| `exceptionCaught` | Every exception of a pipeline event; `IOException`s ignored | The exceptions of the events (connected, message received, disconnected); commons `IOException`s ignored. Socket errors never get there: commons disconnects silently | commons handles socket errors itself |
| Shutdown | `aionClientChannelGroup.close()` unbinds; `releaseExternalResources()` makes the workers close every client channel at once (their disconnect events run on the executor, which is never shut down, and the JVM exits without waiting for it) | `NioServer::shutdown`: `onServerClose` closes every client (queued packets are still written, at most 5 s), then the `ExecutionHandler` runs the queued events (at most 5 s; test `LifecycleTest.ShutdownClosesConnectedClientsAndRunsTheirDisconnectEvents`). Events still queued after that are discarded ("Discarded N channel events on shutdown"), idle threads are joined, and a thread still running an event (e.g. a chat log insert on a database that does not answer) is detached with a warning and ends after it; it keeps the handler alive, and the process exits without waiting for it (test `ExecutionHandlerTest.ShutdownDoesNotWaitForARunningTaskBeyondItsTimeout`) | C++ must stop its threads, but a hung event must not block the exit |
| `ChatClient` and its handler | Reference each other; the garbage collector frees both | The handler drops its `ChatClient` in `channelDisconnected`, the channel's last event (queued after the events of the data read before the disconnect). ChatService and BroadcastService keep the `ChatClient`, and through it the closed handler, until the game server logs the player out, as Java keeps them reachable (tests `LifecycleTest.*`) | No garbage collector; unobservable (no packet of the channel runs after it) |
## Game server connection (`network/gameserver`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| Packet execution | `PACKET_EXECUTOR`, a cached thread pool: the packets of a game server run concurrently and in no particular order (a `CM_PLAYER_LOGOUT` can run before the `CM_PLAYER_AUTH` sent before it) | `PacketProcessor<GsConnection>(1, 1, 50, 3)` owned by `NettyServer`: in receive order, one at a time | Ordering and memory safety; the login server port does the same |
| Executors | `PACKET_EXECUTOR` shut down in `onServerClose`; a single thread executor for `onDisconnect` | `NettyServer::shutdownAll` shuts the processor down after the server; the server's own disconnect thread | C++ must join its threads |
| `GameServerService.registerGameServer` | Unsynchronized check-then-act: two game servers authenticating at once could both be accepted | Guarded by a mutex | Data race; with one game server nothing changes |

## Model and services

| Area | Java | C++ | Reason |
|---|---|---|---|
| `ChatChannels.getOrCreate` | Looks for a matching channel, then creates one: two clients requesting a new channel at once can create two channels for the same identifier, each chatting in its own | Finding and creating is one step under the channel map's lock (test `ChatChannelsTest.ConcurrentRequestsOfANewChannelGetTheSameOne`) | Java race |
| Iteration order | `ConcurrentHashMap<Integer, ...>` order: ascending ids only until they wrap the table (in a 32-slot table id 32 comes before id 5) | Ascending id | Both orders are unspecified. Visible (1) in which channel `getOrCreate` returns when several match: every job channel created by a "[f:" request (see below) matches the plain class name, and C++ always returns the lowest id, Java the first in its table order; (2) if a send throws in the middle of a broadcast (the clients after it get nothing, as in Java) |
| `ChatClient` threads | Plain fields and unsynchronized `ArrayList`s inside a `ConcurrentHashMap`, used by the game server's packets, the client's packets and other clients' broadcasts | A mutex guards identifier, handler, channels and message times; the gag time is atomic | Data races are undefined behaviour |
| Null (`NullPointerException`) cases | NPE: `name()` of a channel of an unknown race, an unknown channel type in `addChannel`'s switch, a client without `ChatClient` or without identifier | `commons::utils::IllegalStateException` with a text like Java's helpful NPE message | No null references; the same observable result (logged by the packet's `run`, nothing sent) |
| Index errors | `ArrayIndexOutOfBoundsException`, `StringIndexOutOfBoundsException` | `commons::utils::IndexOutOfBoundsException` with Java's message ("Index 1 out of bounds for length 1", "begin 0, end -1, length 4") | Exception name only |
| `Channel.toString()` | `Object.toString()`: class name and identity hash code | Class name and the channel id in hex | No identity hash codes; used in one warning of `CM_CHANNEL_LEAVE` |
| Strings | Java `String` (UTF-16) | UTF-8 `std::string`. Client texts are decoded exactly like `new String(bytes, UTF_16LE)` (`utils::Utf16Le`, including the replacement rules for malformed input; over the network: `ChannelFlowTest.LocalizedJobNamesShareTheChannelOfTheirClass`, `BroadcastTest.ChatLogWritesChannelNameSenderAndText`). The token hashes as many UTF-8 bytes of the account name as it has UTF-16 chars, like Java (`GameServerLinkTest.TheTokenHashesAsManyUtf8BytesAsTheAccountNameHasChars`). Strings from the game server are read by commons' `readS`, which turns a lone surrogate into U+FFFD where Java keeps it, and the account name hashed for the token is then U+FFFD's UTF-8 bytes where Java's `getBytes(UTF_8)` writes '?' | Account and character names are plain text; commons' string rules |
| `NoSuchAlgorithmException` | `MessageDigest.getInstance("SHA-256")` fails | OpenSSL's `EVP_Digest` fails | No JCE; SHA-256 is OpenSSL's |
| Numbers in channel identifiers | `Integer.parseInt` accepts every Unicode decimal digit: "١.٠.AION.KOR" (Arabic-Indic digits) is game server 1, race 0 | commons `utils::parseInt` accepts ASCII digits only: `NumberFormatException` (logged by the packet's `run`, no answer) | Inherited from commons (deviation in `commons/utils/Numbers.h`); the client sends ASCII digits |
| Account name comparison (`equalsIgnoreCase`) | Java's full simple case mapping | commons `StringUtils::equalsIgnoreCase`: simple case mapping for ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic, other characters compared as they are | Inherited from commons (deviation in `commons/utils/StringUtils.h`); account names are ASCII in practice |

## Startup, configuration, logging

| Area | Java | C++ | Reason |
|---|---|---|---|
| logback.xml | Configures the root appenders and the `CHAT_LOG` logger (`log/chat.log`, the chat Discord webhook) | `Logging::init` builds the root appenders, `ChatServer::startup` the `CHAT_LOG` logger (file `chat.log` with `${date} %message%n`, the Discord appender with logback's `%replace` pattern if `chatserver.log.chat.discord.webhook_url` is set); the settings come from `config/main/logging.properties` and `config/mycs.properties` like logback's `<property file>` | No logback |
| Unknown property warnings | Removes the keys referenced as `${...}` in the logback.xml in use | Removes the four Discord keys of the chat server's logback.xml | No logback.xml |
| Unresolvable connect address | The `InetSocketAddress` is created unresolved, `getAddress()` is null and `Config.load` throws a `NullPointerException` | `Config::load` resolves it and throws an `IOException`; `SM_GS_AUTH_RESPONSE` resolves it again each time (commons' `InetSocketAddress`) | Same outcome: the startup fails |
| Database socket timeout | HikariCP with the JDBC driver's default: no socket timeout unless the URL sets one | `DatabaseFactory::init(ChatServer::databaseOptions())`: 60 s unless `database.socket_timeout` (C++ key, milliseconds, 0 = none) or the URL's `socketTimeout` sets one; a negative value fails the startup (tests `ChatServerDatabaseOptionsTest.SocketTimeoutIs60SecondsUnlessConfigured`, `ChatServerProcessTest.TheStartupAppliesTheDatabaseSocketTimeout`) | A chat log insert (`chatserver.log.chat_to_db`) on a database that does not answer must not block an event thread forever |
| Startup failure | The main thread dies; threads started before keep the JVM alive | Logged ("Critical Error - Thread [main] terminated abnormally"), the components started so far are shut down, exit code `ERROR_` (1) | Like the login server port |
| Shutdown hook | JVM shutdown hook | Console control handler (Ctrl+C, Ctrl+Break, closing the console, logoff, shutdown) or SIGINT/SIGTERM | Like the login server port |
| Command line (C++ addition) | none | `-D<key>=<value>` overrides a property (layered over `config/mycs.properties`, the applied keys are logged); `--stop-file=<path>`: the server polls the file every 200 ms and shuts down like on Ctrl+C when it appears (the game server has the same option) | Running next to other servers and from tests that cannot send console events |

## Java behaviour kept on purpose

These look like bugs but are the specification; tests pin them down.

- **The gag has no effect with the game server's packets.** The game server sends the gag *duration* in milliseconds (`ChatBanService`:
  minutes * 60000, and 0 to ungag), while `ChatClient.isGagged` compares the value with the current time as a *point in time*, so a gagged
  player can still chat. `ChatService.gagPlayer` also logs the value as minutes. Ported as is on both sides (test
  `BroadcastTest.GagWithADurationDoesNotGag`; a point in time does gag: `BroadcastTest.GaggedPlayerGetsANoticeAndNobodyElseAnything`). Fixing it
  means changing the protocol on one side; see docs/design/chat-server-port.md.
- **Any game server disconnect sets the service offline**, also a second game server that was rejected as `ALREADY_REGISTERED`: the registered one
  stays connected, and the next one can register (`GameServerLinkTest.DisconnectOfAnyGameServerSetsTheServiceOffline`).
- **A client may post to any channel by id** without being in it; the members get the message (`BroadcastTest.AnyClientCanSendToAChannelById`).
- **A player registered again replaces the first registration** in ChatService and BroadcastService: only the new token logs in, and the
  broadcasts go to the new connection; the old connection is not closed (`ClientAuthTest.ARegistrationAgainReplacesTheFirstOne`).
- **A login racing a logout.** `registerPlayerConnection` looks the player up, then sets the connection AUTHED and adds the `ChatClient` to
  BroadcastService without holding ChatService's lock. A `CM_PLAYER_LOGOUT` in between finds no connection to close ("Received logout event
  without client authentication"), and the client stays authenticated and in BroadcastService until its player id is registered again; likewise
  a logout after a second registration removes the entry by id but does not close the first connection. Java has the same race: the game
  server's packets run in order, but they are not serialized with the client's packets.
- **Access levels 0x80 to 0xFF count as staff**: the access level is a signed byte compared with 0 (`ChannelFlowTest.ChannelOfTheOtherRaceIsRefusedUnlessStaff`).
- **A job channel request with a "[f:" suffix never matches**: the suffix is cut off for the alias lookup but not for the comparison, so each
  such request creates a new channel (`ChannelTest.JobChannelMatchesEveryLocalizedNameOfItsClass`).
- Channels are never removed; `CM_CHANNEL_CREATE` and `CM_CHANNEL_JOIN` are read and ignored; the "not fully read" warning of the client packets
  is logged for every such packet (commons logs it once per opcode).
- A declared content length beyond the frame gives a zero-filled array of that length (Java `readB`), which can make the answer too big for the
  send buffer.
