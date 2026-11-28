# chat-server: deviations

The chat server port (`cpp/chat-server`, Java `chat-server`). Since upstream 459b35094 the Java server handles both ports with Netty 4.2
(`network/NetConnector`, `network/netty`: `NettyConnection`, `ConnectionHandler`, `NettyPipeline`, `PacketEncoder`); the C++ port runs both on
one commons `NioServer` (asio) and reproduces the Netty pipeline on top of it (`network/netty/ConnectionHandler`, in commons since upstream bce89b2fc, which moved the Netty classes there for the login server). Every difference that can be
observed from outside (wire, logs, timing, threads) is listed here; the code marks the non-obvious ones with a `Deviation:` comment. Anything
not listed behaves like Java.

## Connections (Java: `NetConnector`, `network/netty`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| Transport | Netty: one acceptor thread, `max(1, NIO_READ_WRITE_THREADS)` I/O threads, the epoll transport on Linux (NIO elsewhere, logged at debug level), a `DefaultChannelGroup` of all channels | One commons `NioServer` with `max(1, NIO_READ_WRITE_THREADS)` IO threads that accept and do the IO alike, and one listener per port | No Netty |
| Pipeline | `LengthFieldBasedFrameDecoder`, `PacketEncoder`, `ConnectionHandler` | `ConnectionHandler` is the channel: a commons connection whose framing is the frame decoder (read buffer = maximum packet size), whose send queue holds the encoded packets, and which forwards to its `NettyConnection` | Same byte flow and the same rules |
| Invalid frames | `TooLongFrameException` / `CorruptedFrameException`: "&lt;connection&gt; sent an invalid packet, disconnecting: &lt;Netty's message&gt;"; an empty frame: "sent an empty packet, disconnecting" | The same texts (`ConnectionHandler::onPacketTooLarge`, `onEmptyPacket`; tests `ClientFramingTest.*`). A size header of 0 to 2 bytes is only recognized once a third byte arrived (commons' framing waits for more than the header) | commons' framing |
| Encoding | `PacketEncoder` writes the packet into the outbound buffer on the channel's event loop | The sending thread writes it into a buffer of the maximum packet size and queues a copy of the encoded bytes, in send order (the guard of the connection is held). A packet that does not fit logs "Error in connection of ..., disconnecting" and closes the connection, like the `EncoderException` in Netty | Packets are written in the order they are sent either way |
| Writability | `WriteBufferWaterMark` 32/64 KiB of the outbound buffer; `bytesBeforeWritable` | The same marks over the bytes queued or handed to the socket and not written yet | No Netty |
| Send stall timeout | `channelWritabilityChanged` schedules a check on the event loop | An asio timer on the connection's strand, the same rule and message | No Netty |
| `close()` | Closes the channel at once; queued writes fail | Drops the queued packets and disconnects once the bytes already handed to the socket were written (at most 2 s later); data received after it is not processed | commons' close |
| Packet executors | `DefaultEventExecutorGroup(8, "PacketProcessor")`: every connection is bound to one of the 8 single threaded executors (`next()`), so a slow packet also delays the connections sharing its thread | `PacketExecutorGroup(8)`: the tasks of a connection run one after another in order, on up to 8 threads started on demand; a connection waits only for its own packets (test `PacketExecutorGroupTest.*`) | Same ordering guarantee, no head-of-line blocking between connections |
| Thread names | "Acceptor-n-m", "ReadWrite-n-m", "PacketProcessor-n-m" | "AcceptReadWrite Dispatcher" / "ReadWrite-n Dispatcher", "PacketProcessor-n", "DisconnectExecutor-0" | Visible in log lines only |
| "Listening on ... for ..." | Logged by `NetConnector` | Logged by commons (logger `com.aionemu.commons.network.NioServer`), same text | The listener belongs to commons |
| Shutdown | `channels.close()`, then the event loop groups and the packet executors shut down gracefully (quiet period 100 ms, timeout 3 s) | `NioServer::shutdown` closes every connection (its disconnect callback is queued after its packets), then the packet executors run what is queued for at most 3 s. Tasks still queued after that are discarded ("Discarded N tasks on shutdown"), and a thread still running a task (e.g. a chat log insert on a database that does not answer) is detached with a warning (tests `LifecycleTest.ShutdownClosesConnectedClientsAndRunsTheirDisconnectCallbacks`, `PacketExecutorGroupTest.ShutdownDoesNotWaitForARunningTaskBeyondItsTimeout`) | C++ must stop its threads, but a hung task must not block the exit |
| `ChatClient` and its connection | Reference each other; the garbage collector frees both | `AionConnection::onDisconnect`, the connection's last task, drops its `ChatClient`. ChatService and BroadcastService keep the `ChatClient`, and through it the closed connection, until the game server logs the player out, as Java keeps them reachable (tests `LifecycleTest.*`) | No garbage collector; unobservable (no packet of the connection runs after it) |
| `ConnectionHandler` and its connection | The channel's pipeline holds the connection, the connection holds the channel | The handler holds its connection until the disconnect and then hands it to the disconnect task, so only the connection keeps the handler afterwards | No reference cycle |

## Game server connection (`network/gameserver`)

| Area | Java | C++ | Reason |
|---|---|---|---|
| `GameServerService.registerGameServer` | Unsynchronized check-then-act: two game servers authenticating at once could both be accepted | Guarded by a mutex | Data race; with one game server nothing changes |

## Model and services

| Area | Java | C++ | Reason |
|---|---|---|---|
| `ChatChannels.getOrCreate` | Looks for a matching channel, then creates one: two clients requesting a new channel at once can create two channels for the same identifier, each chatting in its own | Finding and creating is one step under the channel map's lock (test `ChatChannelsTest.ConcurrentRequestsOfANewChannelGetTheSameOne`) | Java race |
| Iteration order | `ConcurrentHashMap<Integer, ...>` order: ascending ids only until they wrap the table (in a 32-slot table id 32 comes before id 5) | Ascending id | Both orders are unspecified. Visible (1) in which channel `getOrCreate` returns when several match: every job channel created by a "[f:" request (see below) matches the plain class name, and C++ always returns the lowest id, Java the first in its table order; (2) if a send throws in the middle of a broadcast (the clients after it get nothing, as in Java) |
| `ChatClient` threads | Plain fields and unsynchronized `ArrayList`s inside a `ConcurrentHashMap`, used by the game server's packets, the client's packets and other clients' broadcasts | A mutex guards identifier, connection, channels and message times; the gag time is atomic | Data races are undefined behaviour |
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
- Channels are never removed; `CM_CHANNEL_CREATE` and `CM_CHANNEL_JOIN` are read and ignored.
