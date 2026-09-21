# P5-SC scenario harness: deviations and notes

The chunk has no Java counterpart: `tests/scenario` is the C++-only M5a scenario harness (m5a-plan.md §5, F-04, F-06, F-08, G-01).

## Wave 5a, stage 1 (app-harness lane, F-04)

| Area | As built | Reason |
|---|---|---|
| Processes | `ScenarioServers` starts `aion_login_server` (own process group, stopped with `CTRL_BREAK_EVENT`, terminated with exit code 98 if Windows refuses the event because no console is shared) and `aion_game_server` (M5a profile of D1, `--stop-file`, `--check-output`) as child processes whose stdout and stderr go to `login_server.log` / `game_server.log` in the output directory; the destructor terminates a server that still runs | Plan D4; a failing test leaves no server behind |
| Readiness | Login server: "Listening on 127.0.0.1:<port>" for both ports. Game server: "Game server started", then the login server's "Gameserver #1 is now online" | §5.1 "Readiness" (the authenticated link is logged by the login server) |
| URLs | The server of the environment URL, the schema `aion_{ls,gs}_test_m5a_<FNV-1a of the output directory>` and the query of the server's `config/network/database.properties` (`serverTimezone`, `characterEncoding`); credentials as `-Ddatabase.user/password` | §5.1 "URLs"; parallel build trees use their own schemas |
| Schemas | Dropped and created from `sql/aion_ls.sql` / `sql/aion_gs.sql` under a MariaDB lock named like the schema, statements split like the login server test database helper; `gameservers (1, '127.0.0.1', '1234')` | §5.1 "Schemas" |
| `FakeLoginClient` | Packet layouts from the Java login server packets; SM_SERVER_LIST reads a 4-byte address (the C++ login server writes 0.0.0.0 for a game server that never connected, Java would fail on a null address) | Independent of the C++ login server |
| `GameSession` | Client packet bodies from the Java `readImpl` methods (leading fields only, as the real client's trailing bytes are ignored), fixed MAC `0A-1B-2C-3D-4E-5F` and HDD serial `M5ASCENARIO0001`; server packets recorded by Java class name from `ServerPacketsOpcodes.gen.h` (a table, not the packet classes) | D9: no `serverpackets/` include |
| `PacketSequence` | The plan's notation (`T`, `T{n}`, `T{a..b}`, `T+`, `T*`, `[T]`, `(A \| B)+`) matched by an NFA simulation; async-allowed packets are matched first and skipped otherwise | §5.8, §5.9 |
| Self-tests | `ScenarioServersTest` runs `StubGameServer.cmake` (CMake as the stub game server: startup lines, stop file, reports); `LoginServerHarnessTest` runs the real login server with `FakeLoginClient` (database variables required) | Plan §4: harness self-tests with a stub GS |
