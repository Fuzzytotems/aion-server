# Chat server port

Status: **ported** (all 61 Java files of `chat-server/src`), built only with `-DAION_BUILD_CHAT_SERVER=ON` until the integrator switches the
default on. Deviations: [../deviations/chat-server.md](../deviations/chat-server.md).

## What is ported

| Java | C++ (`cpp/chat-server/src/aion/chatserver/...`) | Notes |
|---|---|---|
| `ChatServer` (main, shutdown hook) | `ChatServer.h/.cpp`, `src/main.cpp` (executable `aion_chat_server`) | Java startup order; `-Dkey=value` overrides and `--stop-file=<path>` like the login and game servers |
| `configs/Config`, `configs/main/LoggingConfig`, `configs/network/NetworkConfig` | same paths | `./config/main`, `./config/network`, `./config/mycs.properties`; the `CHAT_LOG` logger of logback.xml is set up in code |
| `dao/ChatLogDAO` | `dao/ChatLogDAO` | table `chatlog` of `chat-server/sql/aion_cs.sql` |
| `model/*`, `model/channel/*`, `model/message/Message` | same paths | enums as `enum class` + free functions (`getById`, `getByIdentifier`, `name`) |
| `common/netty/*` (packet bases of the client side) | `common/netty/*` | Netty's `ChannelBuffer` is commons' little endian `ByteBuffer` (`ChannelBuffer.h`) |
| `network/aion/*` (client packets and handler) | `network/aion/*` | 9 client packets, 4 server packets |
| `network/netty/*` (Netty server, coders, handlers, pipeline) | `network/netty/*` | on commons' `NioServer`/`AConnection`; `pipeline/ExecutionHandler` replaces Netty's `ExecutionHandler` + `OrderedMemoryAwareThreadPoolExecutor` |
| `network/gameserver/*`, `network/factories/GsPacketHandlerFactory` | same paths | 4 game server packets in, 2 out |
| `service/*`, `utils/IdFactory` | same paths | thread safe (mutexes where Java relies on unsynchronized fields) |
| (C++ helper) | `utils/Utf16Le` | `new String(bytes, UTF_16LE)` with Java's replacement rules for malformed client input |

The protocol, byte for byte:

| Direction | Packet | Bytes after the uint16 size (little endian) |
|---|---|---|
| GS → CS | `CM_CS_AUTH` (GS: `SM_CS_AUTH`) | `00`, C game server id, S password |
| GS → CS | `CM_PLAYER_AUTH` (GS: `SM_CS_PLAYER_AUTH`) | `01`, D player object id, S account name, S character name (`getName(true)`), D race id, C access level |
| GS → CS | `CM_PLAYER_LOGOUT` (GS: `SM_CS_PLAYER_LOGOUT`) | `02`, D player object id |
| GS → CS | `CM_PLAYER_GAG` (GS: `SM_CS_PLAYER_GAG`) | `03`, D player object id, Q gag time |
| CS → GS | `SM_GS_AUTH_RESPONSE` (GS: `CM_CS_AUTH_RESPONSE`) | `00`, C response (0 authed, 1 wrong password, 2 already registered, also for a wrong password while a game server is registered); if authed: C address length (4 for IPv4, 16 for IPv6), the address bytes of `connect_address`, H port |
| CS → GS | `SM_PLAYER_AUTH_RESPONSE` (GS: `CM_CS_PLAYER_AUTH_RESPONSE`) | `01`, D player object id, C 48, token (16 random bytes + SHA-256 of the account name) |
| client → CS | `CM_CHAT_INI` / `CM_PLAYER_AUTH` (before the login); `CM_CHANNEL_REQUEST`, `CM_CHANNEL_MESSAGE`, `CM_CHANNEL_LEAVE`, `CM_PLAYER_INFO`, `CM_PING`, `CM_CHANNEL_CREATE`, `CM_CHANNEL_JOIN` (after it) | see the `readImpl` of each packet and `tests/support/FakePeers.h` |
| CS → client | `SM_CHAT_INI`, `SM_PLAYER_AUTH_RESPONSE`, `SM_CHANNEL_RESPONSE`, `SM_CHANNEL_MESSAGE` | see `tests/network/ServerPacketBytesTest.cpp` |

## Build and test

```bash
cmake -S cpp -B cpp/build/<dir> -G "Visual Studio 18 2026" -A x64 -DAION_BUILD_CHAT_SERVER=ON
cmake --build cpp/build/<dir> --config Debug --target aion_chat_server aion_chatserver_model_tests aion_chatserver_network_tests aion_chatserver_data_tests aion_chatserver_e2e_tests
AION_TEST_CS_DATABASE_URL="jdbc:mysql://127.0.0.1:3306/aion_cs_test" AION_TEST_CS_DATABASE_USER=root AION_TEST_CS_DATABASE_PASSWORD= \
  ctest --test-dir cpp/build/<dir> -C Debug -L chatserver
```

All chat server tests carry the label `chatserver` (124: 44 model, 76 network, 2 data, 2 end-to-end); the ones that need the test schema
(`ChatLogDAOTest.*` and the end-to-end `ChatServerProcessTest.*`, which start the real executable) also carry `realdata`. They FAIL without
`AION_TEST_CS_DATABASE_URL` (the message names the variable); `AION_CS_ALLOW_DATABASE_SKIP=1` turns that into a skip, and
`ctest -L chatserver -LE realdata` runs everything else. The database of the URL is created if missing and its tables come from
`chat-server/sql/aion_cs.sql`; test processes serialize on the MariaDB lock `aion_cs_test:<database>`. Every network test binds 127.0.0.1 on
ports chosen at run time.

## Running it next to the login and game servers (real 4.8 client in C:/Aion)

1. Database: the schema `aion_cs` from `chat-server/sql/aion_cs.sql` (already present on the development machine, see cpp/README.md › Local
   database). The chat log table is only written with `chatserver.log.chat_to_db = true`. Pooled connections get a socket timeout of 60 s
   (C++ key `database.socket_timeout` in milliseconds, or `socketTimeout` in `database.url`; 0 = none), so a hung database cannot block the
   event threads and the shutdown forever.
2. `chat-server/config/mycs.properties` (the file is git-ignored in the Java project; the C++ server reads the same one):
   ```properties
   # the game server authenticates with this password
   chatserver.network.gameserver.password = chatpass
   # the address the Aion client connects to; 0.0.0.0 (the default, following socket_address) picks a local IPv4 address and logs it.
   # For a client on the same machine: 127.0.0.1:10241
   chatserver.network.client.connect_address = 127.0.0.1:10241
   ```
   The server listens on `0.0.0.0:10241` for clients and `0.0.0.0:9021` for the game server (`config/network/network.properties`). The login
   server uses 2106/9014 and the game server 7777, so the three do not collide.
3. Start it from the Java module directory, like the login server:
   ```bash
   cd chat-server && ../cpp/build/msvc/chat-server/Debug/aion_chat_server.exe
   ```
   Stop it with Ctrl+C (or `--stop-file=<path>` and create the file). Logs go to `chat-server/log` (`server_console.log`, `chat.log`, ...).
4. Order: login server, chat server, game server. The game server connects to the chat server during its startup (and retries every 10 s if
   the chat server is not up; see below), then accepts clients.
5. Client: unchanged (`bin64\aion.bin -ip:127.0.0.1 -port:2106 -loginex`); it learns the chat server's address from the game server.

## What the game server needs to switch its chat link on

The link itself is ported (chunk P4-15, `game-server/src/aion/gameserver/network/chatserver/**`) and checked against this port byte by byte
(see "Protocol check" below). It is off by configuration:

| Key (game-server/config) | Default | Set to |
|---|---|---|
| `gameserver.chatserver.enable` (main/gameserver.properties) | `false` | `true`: `GameServer.cpp` startup step "ChatServer.connect(nioServer)" connects |
| `gameserver.network.chat.address` (network/network.properties) | `localhost:9021` | the chat server's game server address |
| `gameserver.network.chat.password` (network/network.properties) | empty | `chatserver.network.gameserver.password` of the chat server |
| `gameserver.chatserver.min_level` (main/gameserver.properties) | 10 | minimum level to write in channels, sent to the client in `SM_VERSION_CHECK` |

How the client finds and uses the chat server:

1. At game server startup `ChatServer::connect` opens the link and sends `SM_CS_AUTH`; `SM_GS_AUTH_RESPONSE` carries
   `chatserver.network.client.connect_address`, which `CM_CS_AUTH_RESPONSE` stores as the public address (`ChatServer::setPublicAddress`).
2. **`SM_VERSION_CHECK`** (the game server's first packet to a connecting client) announces it: `H ChatServersCount` (1 once the link is
   authenticated), `C 0`, the 4 IPv4 bytes, `H port`. A client that connected before the link came up sees no chat server until it reconnects.
   The address must be IPv4 (4 bytes, no length byte in this packet).
3. In the world the client sends **`CM_CHAT_AUTH`** (packet 174, opcode 0x0171, state IN_GAME); the game server sends `SM_CS_PLAYER_AUTH`, the
   chat server answers with the token, and `CM_CS_PLAYER_AUTH_RESPONSE` passes it to the client in **`SM_CHAT_INIT`** (D 48, token).
4. The client connects to the chat server: `CM_CHAT_INI`, `CM_PLAYER_AUTH` (object id, "&lt;name&gt;@&lt;identifier&gt;", lower case account
   name, token), then `CM_CHANNEL_REQUEST` for its map, trade, LFG, class and language channels after entering the world and every teleport.
5. On logout `PlayerLeaveWorldService` sends `SM_CS_PLAYER_LOGOUT`; the chat server closes the client's chat connection.

Open items on the game server side before the link is switched on:

- **`ChatBanService` is `AION_UNPORTED`** (`services/ban/ChatBanService.cpp`), and `CM_CS_PLAYER_AUTH_RESPONSE.runImpl` calls
  `ChatBanService::isBanned` right after sending `SM_CHAT_INIT`: every chat login would hit the unported body (the client still gets its token,
  but the scenario gates count the hit). `isBanned` and `getBanMinutes` must be ported first.
- `GameServer::isShutdownScheduled` in `ChatServerConnection::onDisconnect` (m5a-plan.md S-12) decides whether a lost link is reconnected.
- The gag semantics (Java behaviour, kept on both sides): the game server sends a duration, the chat server compares it with the current time,
  so gags have no effect in Java either; `PunishmentService` even passes minutes where `ChatBanService.banPlayer` expects milliseconds. A fix
  belongs to both servers at once (e.g. the game server sends `System.currentTimeMillis() + duration`), and is a deliberate protocol change.

## Hardening proposals (not in Java, not done)

The port keeps Java's exposure to a misbehaving client, apart from queuing only the encoded bytes of a sent packet (Java keeps the whole
16 KiB buffer; see the deviations):

- **Per-connection limits.** The event queue of a connection (`ExecutionHandler`) and its send queue are unbounded; Netty's executor limits
  (1 MB per channel, 128 MB in total) are not reproduced. A client that floods frames or never reads its answers grows them until it
  disconnects. Proposal: disconnect a connection whose queued events or queued send bytes exceed a limit (e.g. 1 MB), logged like commons'
  oversized-packet warning.
- **Idle timeout before the login.** A connection that never sends `CM_PLAYER_AUTH` stays open (Java too). Proposal: close connections that
  are not AUTHED after e.g. 60 s.

Both are behaviour changes and belong in a deliberate follow-up, together with the commons additions listed in the port's report (a
per-key ordered executor, an immediate close for subclasses).

## Protocol check against the game server (read-only)

| Packet | Game server (C++) | Chat server (C++) | Result |
|---|---|---|---|
| auth | `SM_CS_AUTH`: `CsServerPacket::write` H 0, C 0x00, `writeC(GAMESERVER_ID)`, `writeS(CHAT_PASSWORD)` | `CM_CS_AUTH`: opcode 0x00 in CONNECTED, `readC`, `readS` | match |
| player auth | `SM_CS_PLAYER_AUTH`: C 0x01, D object id, S account, S `getName(true)`, D `raceIdOf(race)`, C access level | `CM_PLAYER_AUTH`: D, S, S, D, C | match |
| logout | `SM_CS_PLAYER_LOGOUT`: C 0x02, D object id | `CM_PLAYER_LOGOUT`: D | match |
| gag | `SM_CS_PLAYER_GAG`: C 0x03, D object id, Q gag time | `CM_PLAYER_GAG`: D, Q | bytes match; semantics differ (duration vs point in time, Java behaviour) |
| auth response | `CM_CS_AUTH_RESPONSE`: `readC`; if 0: `readUC` length, `readB`, `readUH` port | `SM_GS_AUTH_RESPONSE`: C 0, C response, if authed C address length (4 for IPv4), address, H port | match |
| player auth response | `CM_CS_PLAYER_AUTH_RESPONSE`: `readD`, `readUC` length, `readB` | `SM_PLAYER_AUTH_RESPONSE`: C 1, D id, C 48, token | match |

Framing matches on both sides (uint16 little endian size including itself); the game server's read buffer (16 KiB) is far above the largest
chat server packet (56 bytes).

## Proposed three-server gate (tests/scenario, once that directory is free)

A `gs.scenario.chat` gate next to `gs.scenario.m5a`, built on the scenario harness (`ScenarioServers`, `ChildProcess`, `GameSession`):

1. Start the login server as today, the chat server as a `ChildProcess` (`aion_chat_server --stop-file=<dir>/cs.stop` in a scratch copy of
   `chat-server/` with `mycs.properties`: 127.0.0.1 on ports probed at run time, a test password, `chatserver.log.chat_to_db = true`,
   `database.url` of an `aion_cs_test` schema), then the game server with `-Dgameserver.chatserver.enable=true`,
   `-Dgameserver.network.chat.address=127.0.0.1:<cs game server port>`, `-Dgameserver.network.chat.password=<test password>`.
2. Wait for "Gameserver #1 is now online" in the chat server's log and "Connected to chat server" in the game server's.
3. A fake client logs in and enters the world (`GameSession`, as in M5a); assert `SM_VERSION_CHECK` announces one chat server at 127.0.0.1 and
   the chat client port.
4. Send `CM_CHAT_AUTH`; expect `SM_CHAT_INIT` with a 48-byte token.
5. Open the chat connection with `FakeChatClient` (`cpp/chat-server/tests/support/FakePeers.h`, header-only; include it like the login client
   crypto): `CM_CHAT_INI`, `CM_PLAYER_AUTH` with the object id, "&lt;getName(true)&gt;@AION", the lower case account name and the token; expect
   `SM_PLAYER_AUTH_RESPONSE`.
6. Join the map channel (`@<U+0001>public_<map>` + `<U+0001>1.<race>.AION.KOR`), a second fake player does the same, one talks, the other
   receives exactly Java's `SM_CHANNEL_MESSAGE`; the `chatlog` row exists.
7. Log both players out through the game server (`CM_QUIT`): the chat server closes their chat connections ("Player[id=...] logged out").
8. Stop the game server (stop file), the chat server (stop file) and the login server; all exit with 0; no unported hits.

Labels `scenario;chatserver;realdata`, the scenario resource lock (one game server at a time), and the same "fail when a prerequisite is
missing" rule as the other milestone gates. Prerequisites: `ChatBanService.isBanned/getBanMinutes` ported (see above), the chat server target
built (`AION_BUILD_CHAT_SERVER=ON`).
