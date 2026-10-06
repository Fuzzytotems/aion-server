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

## Wave 5a, stage 2 (scenario lane, S-11, F-06, F-08)

| Area | As built | Reason |
|---|---|---|
| Gate shape | The whole gate is one GoogleTest case `M5aScenario.Run`, because it owns one pair of server processes. The discovered case is marked `DISABLED` and `gs.scenario.m5a` runs the same binary with `--gtest_filter=M5aScenario.Run` (LABELS `scenario;realdata`, TIMEOUT 900, RESOURCE_LOCK `aion_game_server_log;aion_login_server_log`, SKIP_REGULAR_EXPRESSION) | §5.10; a discovered case would run the servers a second time under a full `ctest` |
| Case log | Cases run in order through a `CaseLog` that counts the failed assertions of each case, keeps going after a failure inside a case, and prints the case-by-case report of case 8 whether the gate passes or fails. A case whose predecessor failed is reported as "not run" | §5.7 Q8 "the reports of case 8"; the fixup lanes need the case that failed, not only the first assertion |
| Case 0, case 0b | Two cases beyond the plan's 1-8: "the servers start" and "the creation oracle answers". Case 0 turns a startup that never logs "Game server started" into the last startup step plus the first `is not ported yet` and `ERROR` lines of the game server log | Without them a blocked startup fails with a bare timeout, which names no owner |
| `SM_ATREIAN_PASSPORT` | Matched as `[SM_ATREIAN_PASSPORT]` (optional) instead of "present iff the passport is not disabled, read from the GS summary" (§5.8 #29) | `m5a_summary.txt` is written at shutdown, so its `atreianPassportDisabled` value does not exist while case 3 runs |
| `SM_INVENTORY_INFO` count | `ceil(items / 10) + 1`, where `items` is the oracle's item list (the kinah row included). §5.8 #13 writes `ceil((1 + items) / 10) + 1` | `PlayerEnterWorldService.sendItemInfos` splits the kinah item plus the equipped and inventory items; the oracle's list is exactly those rows, kinah among them, so adding one would count it twice |
| Oracle | `tools/oracle/oracle.py` runs as a child process (`Oracle.h`), its JSON answer is parsed with nlohmann/json; without `AION_TEST_PYTHON` the gate prints "gs.scenario.m5a: skipped" | F-05; the oracle must stay an independent implementation, so it is not linked in |
| `ChildProcess` | `waitForLog` scans only the bytes that appeared since the last call, and `findLogLines` / `readLogTail` read the log line by line | A startup that hits an unported body inside `spawnAll` writes a 105 MB log (532,000 lines); re-reading it on every 100 ms poll made the readiness wait quadratic |
| F-08 additions | `SM_NPC_INFO`, `SM_GATHERABLE_INFO`, `SM_QUEST_LIST`, `SM_WAREHOUSE_INFO` and `SM_MACRO_LIST` decoders, written from the Java `writeImpl` like the stage-1 ones. `SM_NPC_INFO` derives the npc equipment entry count from the bit count of the slot mask (`NpcEquippedGear.init` puts one item into each slot it marks) and checks that the two template ids are equal | V1-V4 and V10 need them; F-08 phase 1 stopped after the enter-world packets |

## Wave 5a, stage 2 phase 4 (final fixer: the gate, its decoders and the app/startup chunks)

The stage-2 reviews found that the gate was red on an expectation of its own, and that even green it would prove less than the milestone claims.
These are the changes that answer them; each row says what a run now catches that it did not catch before.

| Area | As built | Reason |
|---|---|---|
| §5.8 `SM_TITLE_INFO` | The enter-world pattern expects `SM_TITLE_INFO{2}` between `SM_QUEST_LIST` and `SM_MOTION`, and m5a-plan.md §5.8 row 7-10 is corrected to match | **The expectation was wrong, not the port.** `PlayerEnterWorldService.java:239` sends `SM_TITLE_INFO(pcd.getTitleId())`, and lines 240-242 then run `if (pcd.getBonusTitleId() != 0) player.getTitleList().setBonusTitle(...)`, whose first statement is `SM_TITLE_INFO(6, bonusTitleId)` (`TitleList.java:88`). A character's bonus title is -1, not 0 (`PlayerCommonData.java:51`; `aion_gs.sql:931` `bonus_title_id int NOT NULL DEFAULT '-1'`), so Java sends it on every enter world. `PlayerEnterWorldService.cpp:462-465` and `TitleList.cpp:98-110` are statement-for-statement identical. This single token failed case 3, and because a failed case skips the rest, cases 4-7 — V1-V5, M0-M3, Q1-Q7 — had never executed against a running server in any lane's run of the whole wave |
| §5.7 Q8 live instances | Split into `strictlyZeroLiveClasses()` (Player, Item, AbyssRank and the interaction tasks: exactly 0) and `perConnectionLiveClasses()` (Account, AccountTime, PlayerAccountData, PlayerCommonData, PlayerAppearance, ConnectionAliveChecker and every `*Storage`: at most one per client still connected when the stop file was written, counted by the harness) | The old strict 0 for Account/PlayerCommonData/`*Storage` demanded a port **less faithful than Java**: `AionConnection.java:239-243` returns from `onDisconnect` before `LoginServer.onDisconnect` while `isShuttingDownSoon()`, and `LoginServer.java:119` is the only place that removes the connection from `loggedInAccounts`, so a connection open at the shutdown holds its account-level objects to process exit — which is exactly what §5.7 Q7 arranges. The bound is not a blanket exemption: account A quits normally in case 6, so its account-level objects leaking would push the count past the one open connection and still fail. Matching row in `P5-14.md` |
| §5.5 V4 | V4 gained the completeness half: every deterministic gather spot within 90 m must have an `SM_GATHERABLE_INFO` of that id at that position, mirroring V3 | A subset assertion alone is satisfied by **zero** `SM_GATHERABLE_INFO`, and the level-ready `(SM_NPC_INFO \| SM_GATHERABLE_INFO)+` is satisfied by the NPCs on their own. That is the state the tree was in for the whole wave (18,432 skipped gatherable spawns). While the site was `AION_PARTIAL` its deliberate absence from `m5a_partial_allowlist.txt` caught it; now that it is ported, V4 was the only remaining guard and could not fail |
| §5.4 V9 | `SM_PLAYER_INFO` (§5.8 #33) is decoded in case 4 and case 7: object id, name, class id, level, HP%, x/y/z, no legion, and an equipment list that is a subset of the equipped inventory rows | `decodePlayerInfo` existed and was unit-tested, but the gate never called it: `SM_PLAYER_INFO` appeared only as a name. It is its own 230-line `writeImpl`, not the `writePlayerInfo` block that `decodeCharacterList` covers, so a shifted field in the packet the real client renders the character from did not fail the gate. Decoding it ends in `expectFullyConsumed`, so its framing is checked too. The equipment assertion is a subset, because `writeEquippedItems` is called with `getEquippedForAppearance()` and writes `getItemSkinTemplate().getTemplateId()` per entry |
| §5.5 V2 | An NPC whose id the oracle knows **only** as fixed spots, standing on none of them, now fails with the sent and the expected positions instead of `continue` | The escape hatch skipped heading, level and HP% for exactly the case V2 exists for. V1 matches by id and oracle distance only and never looks at the coordinates the server sent, so a duplicated-id NPC at a wrong position was caught by nothing |
| §5.4 V7 | The decoded entries are counted in a vector as well as keyed by template id, and the equipment slot is asserted for **every** item, not only the equipped ones | A map collapsed a duplicated item into one entry and still satisfied the size assertion, and the computed `expectedSlot` for a non-equipped item was thrown away. `SM_INVENTORY_INFO.java:56` writes `item.getEquipmentSlot() & 0xFFFF`, which for an item that was never equipped or moved is the untouched `ItemStorage.FIRST_AVAILABLE_SLOT` = 65535 (`ItemStorage.java:16`, `Item.java:45`) |
| §5.7 Q8 allow-list | An entry **with** a line number must match the whole site; only an entry without one is a whole-file wildcard. Rows the run did not hit are printed | `site.starts_with(entry)` let `InstanceService.cpp:133` also accept `:1330`-`:1339` and `BaseService.cpp:18` accept `:180`-`:189`, so a future `AION_PARTIAL` at one of those lines would be allow-listed by accident; and a row stayed green forever after the site it describes was ported |
| §5.7 Q8 reports | Case 8 additionally asserts `exitCode 0` and `knownListNotifyFailures 0` from `m5a_summary.txt`, reads `<log-folder>/server_errors.log`, and reports a login server terminated with exit code 98 unless its log contains "ServerChannels closed." | The game server's exit code was only checked in case 7, which does not run when an earlier case fails. The ERROR scan read only the captured console stream, which logback's `additivity="false"` loggers never reach. Exit code 98 means `GenerateConsoleCtrlEvent` was refused and the harness **killed** the login server, which was indistinguishable from a clean shutdown |
| §5.7 Q8 leak surface | The whole `live_counts_baseline.txt` → `live_counts.txt` difference is printed as diagnostics | `LeakCensus` is fed only by `World::removeObject`, so `census.txt` covers VisibleObjects alone, and the live-count assertions cover a handful of the 148 tracked classes. The diagnostics assert nothing, but they make a growth that no rule covers visible in the run instead of invisible |
| GS log directory | `ScenarioServers::gameServerArguments()` passes `--log-folder=<outputDir>/gs_log` | `Logging::init` archives and **deletes** the `*.log` files it finds, so the gate's child was wiping the shared `game-server/log` — the cross-lane interference four lanes reported, and the directory the user's own play server writes. It also puts `server_errors.log` where Q8 can read it |
| Child processes | Both children are created suspended, assigned to a job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` and resumed | The destructor was the only reaper, so a CTest `TIMEOUT 900` — reachable, since `ScenarioServers` alone budgets a 10 min startup and `Oracle::run` waits up to 20 min — or a crash of the gate left both servers running with their schemas and the log directory. Measured before the fix: killing only the test process left `aion_login_server` and `aion_game_server` alive. The `NUL` stdin handle is now also closed on the error-file failure path |
| Skip vs. pass | `AION_SCENARIO_REQUIRE=1` (set by `-DAION_SCENARIO_REQUIRE=ON` through `ScenarioTests.cmake`) turns every skip reason into a failure | A `ctest` without the database URLs printed "***Skipped" and "100% tests passed" with exit 0, and nothing else in the suite asserts any M5a behaviour end to end, so a default run "proved" M5a by not running it |
| Case 4b (new) | The scripted path sends `CM_CUSTOM_SETTINGS` and `CM_SUBZONE_CHANGE` after the level-ready burst, as its own case that does **not** gate the cases after it | Q8's `notPortedClientPacket` check can only report packets the script itself sent, so a client packet nobody scripts is invisible until a real 4.8 client sends it. Both need no user action, and `CM_SUBZONE_CHANGE` is the client-driven entry into `Player::revalidateZones` and therefore into the siege and vortex zone handlers this wave ported. It is additive coverage, so a failure there must not cost the run its §5.6 and §5.7 evidence |
| Kept, with the limit written down | The "zombie-cut" and "stale pin" rows of Q8 stay, and §5.7 of the plan now says they cannot fire | `gameserver.runtime.zombie_break_minutes` is 30 and `LeakCensus::stalePinAfter` 10 minutes (checked every minute) against a one-to-three-minute run, and `CheckOutput::runFinalCensus` disables the zombie breaker before the final scan. The thresholds are integer minutes (`RuntimeConfig.cpp:13-14`), so they cannot be lowered into a gate run without a config change in another chunk, and lowering them would risk false positives in the one run that must be trusted. D7's periodic machinery is therefore **not** proven at M5a, and the plan says so instead of implying otherwise |
| Oracle `level` | `Oracle.cpp` reads every optional JSON number through a helper that tolerates `null` | Found by the first run that ever reached case 4: `nlohmann::json::value(key, fallback)` **throws** when the key is present but null, and `m5a-spawns` reports `"level": null` for a gather spot, whose template is a `GatherableTemplate` and has no level. Before gatherables were spawned the field never appeared, so this was invisible; it made case 4 die with a bare `type must be number, but is null` |
| §5.6 M2 | The "was announced before" set is built from `SM_GATHERABLE_INFO` object ids as well as `SM_NPC_INFO` ones | Found by the first run that ever reached case 5. A region move leaves gatherables behind exactly as it leaves npcs behind, and the world now holds 18,432 of them, so the server legitimately sends `SM_DELETE` for gatherable object ids the npc-only set did not contain. M2 reported five of them as "never announced" |
| §5.7 Q5 | The relogin waits out `gameserver.character.reentry.time` before `CM_ENTER_WORLD`, exactly as Q3 does, and asserts `SM_ENTER_WORLD_CHECK` is 0 | Found by the first run that ever reached case 6. `PlayerEnterWorldService.java:152-156` answers `SM_ENTER_WORLD_CHECK(Msg.REENTRY_TIME)` and returns while `now - lastOnline < CHARACTER_REENTRY_TIME * 1000`. Q4 logs the character out seconds earlier and the new login takes less than the scenario's 1 second, so the server correctly refused the enter and Q5 failed with "no SM_PLAYER_SPAWN". The server was right and the script was too fast; the new `SM_ENTER_WORLD_CHECK` assertion makes the next such refusal name itself instead of showing up as a missing packet |
| Written down, not fixed | §5.1 of the plan now states that geodata is out of M5a scope; `GameSession.h` records that packet **names** come from the server's own generated opcode table; `ScenarioDatabase.h` records that the DB assertions use the same C++ connector the DAOs wrote with | Each is a real limit of what a green gate proves. Enabling geo in the gate (the checklist keeps it on) would change the startup branch under test and is a stage-3 smoke run of its own; the opcode independence is restored by separate parity tests the gate does not depend on; the connector risk is small because every column is read as a raw string |

## Wave 5a, stage 3 (harness lane): what a killed run leaves behind

The stage-2 reviews left four findings about the process harness. Two were already fixed in stage 2 phase 4 (the rows above) and were re-measured
here rather than re-fixed; the other two are these changes. What a run may **not** damage is now the rule: no other tree's logs, no other tree's
schemas, and no pass with a server that did not shut down.

| Area | As built | Reason |
|---|---|---|
| Login server log directory | `ScenarioServers::startLoginServer` runs the child in `<outputDir>/ls_run`, a working directory of its own with a **copy** of the login server's `config` directory (4 files); `loginLogFolder()` is `<outputDir>/ls_run/log` | The stage-2 fix gave the game server `--log-folder`, but the login server has no such argument: it resolves `./config` (Config.cpp:63-67) and `./log` (`Logging::Config::logFolder` defaults to `"log"`) against its working directory, which was the Java module directory. Measured before this change: a gate run archived the shared `login-server/log` into `archived/2026-09-21 14.04 to 2026-09-21 14.05.zip` — another process's log files — and then wrote its own console log there. `Logging::init` archives and **deletes** what it finds, so two build trees running the gate, or a gate run next to the user's own login server, destroy each other's logs. An `--log-folder` for the login server would be the smaller change, but it is a `cpp/login-server` source change (plan I-04) and the harness can do it alone |
| Schemas of a killed run | Both schemas carry an in-use marker for the whole run (`SchemaLease`: `GET_LOCK("<schema>:in_use")` on a connection of its own), and `createSchemas()` first takes the two markers, then drops every `aion_{gs,ls}_test_m5a_<8 hex>` schema that **no** marker holds and whose youngest table is older than an hour (a schema without tables counts as old) | A CTest `TIMEOUT` or a crash runs no destructor: the job object reaps the two servers, but the run's schemas stayed forever, one pair per build tree. MariaDB drops a session lock when the connection dies, so the marker is exactly the "this run is alive" signal that survives a `TerminateProcess` - inverted. The hour of grace is the second guard, for a run of an **older** binary that takes no marker at all: a gate run cannot outlive `TIMEOUT 900`, so nothing an hour old is a live run. Measured while writing this: a foreign tree's run was 1 minute old and marker-free, and the sweep left it alone |
| Stop problems | `ScenarioServers::stopProblems()` lists a server that did not exit, a non-zero exit code, a login server that had to be **killed** because Windows refused `CTRL_BREAK`, and a server still running at the end; the destructor reports a list nobody read as a GoogleTest failure | Case 7 asserts the game server's exit code, but case 7 does not run when an earlier case failed, and case 8 then reads only the server's own `exitCode` line out of `m5a_summary.txt` - a file the game server writes **before** it exits. The harness now owns the assertion, so it cannot be skipped by a control flow inside the gate. `LoginServerHarnessTest` no longer accepts exit code 98 either |
| `ChildProcess` handles | Every handle in the constructor belongs to a scope guard (`Handle`) | The stage-2 fix closed the `NUL` stdin handle on the one path the review named. The `create_directories` of the error file's parent throws when a path component is a file, and that path closed nothing: measured with the new handle-count test against HEAD `29a01ece1`, 60 failed starts leaked 40 handles (2 per attempt of that case, 0 for the two the stage-2 fix covered) |

**What is still true after a kill.** The schemas of a killed run are dropped by the *next* gate run - in the same tree by `recreate()`, in another
tree by the sweep once they are an hour old - not at the moment of the kill. Dropping them at the kill needs a reaper process outside the job
object that survives the gate and decides on its own that the run is dead; that was rejected here, because a reaper that misjudges a live run
issues `DROP DATABASE` under it, which is a worse failure than a leftover schema.

## M5c stage 0 (gate-parallel lane): two server runs at a time

Every gate used to hold the one lock `"aion_game_server_log;aion_login_server_log"`, so a full `ctest -L "scenario|smoke|geo|m4"` ran its eleven
server runs one after the other (about 2,100 s in a Debug tree). The lock is older than the harness's own files per run; this lane audited what
two concurrent runs still share and replaced it with two slots.

| Area | As built | Reason |
|---|---|---|
| Two gate slots | Every test that starts a real game server holds exactly one of two CTest resource locks, `AION_GS_GATE_SLOT_1` = `aion_game_server_log` and `AION_GS_GATE_SLOT_2` = `aion_game_server_slot_2` (`ScenarioTests.cmake`, "the two gate slots"). Slot 1: `gs.smoke.startup`, `gs.smoke.startup_progress`, `gs.smoke.startup_geo`, `gs.m4.check_static_data`, `gs.scenario.m5a`, `m5a_geo`, `m5b3`, `m5b3_geo` (1,062 s run alone); slot 2: `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo` (1,051 s). `gs.scenario.m5a_stress` holds both; `LoginServerHarnessTest` holds slot 1 instead of the unused `aion_login_server_log` | At most two game servers under any `ctest -j`, balanced by runtime. Slot 1 keeps the historical name because `cmake/AppTests.cmake` (P5-14) registers the smoke tests and the M4 check with exactly that lock, so they are in slot 1 without an edit in another chunk |
| One slot per schema prefix | A gate and its geo variant share a slot; `LoginServerHarnessTest`, whose schema pair has the prefix `m5a`, shares the m5a gates' slot | `ScenarioDatabase::dropAbandonedSchemas` tests the in-use marker (`IS_FREE_LOCK`) before its `DROP`, not under it: two runs of one prefix that start together could drop the pair the other is recreating if a killed run left it behind. The smoke and M4 schemas are never swept |
| Slot guard | `aion_gs_check_gate_slots`, deferred to the end of the game-server directory (`cmake_language(DEFER CALL)`), gives a `gs.scenario.*`, `gs.smoke.*` or `gs.m4.*` test that holds neither slot both slots and a configure warning | A gate registered without a slot would start a third server beside two others; with both it runs alone. Mutation-proven: `gs.scenario.m5a` without its lock configured with the warning and `RESOURCE_LOCK ['aion_game_server_log', 'aion_game_server_slot_2']` |
| HTML cache | `ScenarioServers::gameServerArguments` passes `-Dgameserver.html.cache.file=<outputDir>/html.cache` and `startGameServer` removes that file first; `RunStartupSmoke.cmake` does the same with `<OUTPUT_DIR>/html.cache` and fails a run whose file survives the removal, and a started run that read a cache ("Cache[HTML]: Using cache file") or left no file at least as new as its own start | `HTMLCache::reload` writes `./cache/html.cache` below the shared working directory whenever the file is missing (HTMLCache.java:112-120). Today the write only fails because `game-server/cache` does not exist (a warning in every run); with the directory present, two servers starting together would truncate and read one file. The M4 check never creates `HTMLCache`. The checks are about what the start saw, not what is on disk afterwards (the stage-0 review's two surviving mutants: the harness removing the file only after "Game server started", and the smoke script without the removal and the override, which passed on the previous run's file). `StubGameServer.cmake` therefore does what `HTMLCache` does with the file - logs "Using cache file" for an existing one, writes a missing one and logs "Creating cache file" - and `EveryStartRemovesTheHtmlCacheTheRunBeforeLeft` asserts the second |
| `MethodStats.log` (routed on 2026-09-30) | Every orderly shutdown writes `<log folder>/stats/MethodStats.log`, i.e. into the run's own `--log-folder` | Until 2026-09-30 `RunnableStatsManager::dumpClassStats` hard-coded `./log/stats` (as Java does) and ignored `--log-folder`, so every gate rewrote `game-server/log/stats/MethodStats.log` and two shutdowns at once interleaved it. The commons change takes the folder `Logging::init` was given (`Logging::getLogFolder()`; default `log`, Java's `./log`): DEVIATIONS.md, "commons / utils", and the section "Small tasks 2026-09-30" below |
| Unshared by construction | Log folders, check output, stop files, login-server working directories, oracle output (all below the run's output directory); ports (ephemeral; `NioServer` binds with `SO_EXCLUSIVEADDRUSE`, so a collision fails loudly); schemas (hash of the output directory, in-use marker); a watchdog minidump names its process id | Checked in the harness, `RunStartupSmoke.cmake`, `RunM4Check.cmake`, `tools/oracle` (writes only the paths it is given) and the production writers (`HTMLCache`, `RunnableStatsManager`, `Watchdog`, `Logging`) |
| One ctest process | The slots are CTest resource locks, so they bound the server runs of one `ctest` invocation. Two build trees running their gate sets at once can reach four game servers (about 12.8 GB with geo), and the marker-before-`DROP` window of `dropAbandonedSchemas` is open between them | Operational rule, written into the slot table of `ScenarioTests.cmake`: one tree's gate set at a time. `RunStartupSmoke.cmake` already said so for its own lock ("CTest's RESOURCE_LOCK only serializes one ctest run") |
| `RunStartupSmoke.cmake` is P5-14's file | Edited under this lane's lease; `P5-14.md`, section "M5c stage 0", has pointer rows for its lock (now slot 1) and the HTML cache checks, and the script's header names both documents | The comment at `cmake/AppTests.cmake:44-46` described the single lock; the stage-0 integration (2026-09-25) rewrote it to name gate slot 1 (P5-14's file, recorded in `P5-14.md`) |

**Measured, 2026-09-24** (`ctest -C Debug -j 2 -L "scenario|smoke|geo|m4" -E m5a_stress`, the machine building other trees meanwhile):
1,286 s and 1,227 s of wall clock, against the ~2,100 s the same runs take one after the other; the two slots finished within 27 s and 21 s of
each other. Every pair really overlapped, and the memory sampler never saw more than two `aion_game_server` processes; the peak of two geo
servers together was 6,352 MB of private bytes. Each run is 10-30 % slower beside another (`m5b_geo` 351 → 364 / 424 s).
Both runs had gate failures, all of one kind, and none caused by the parallel runs: the final census wrote `Player <id> 1` (run 1: `m5b`,
`m5b2`, `m5b3_geo`; run 2: `m5b`, `m5b_geo`), while the same shutdown logged "0 objects still tracked" and `live_counts.txt` 0 live Players.
`gs.scenario.m5b` alone, with no other gate running, fails the same way. Each of these runs logs one "Leak census: ... Player ... refcount 3"
line, then a "Leak probe" line from a pool thread, and every passing run has neither. The npc-leak lane's uncommitted `LeakCensus`
holder probe pins the reported object until the probe task runs (`postHolderProbe`, `Pin(candidate.object)`), so the two consecutive
scans of `CheckOutput::runFinalCensus` see a steady count of 1 instead of a logout that is still being reclaimed and falls to 0. M5b-3's
tree logged the same transient line and wrote no leak (m5b3-plan.md, "The census race (fixed)").

**Again, 2026-09-25, after the stage-0 review** (the same command, a fresh `build/msvc` of the tree at 00:30 with the npc-leak lane's code of
00:21): 1,162 s of wall clock, 43 of 46 passed, at most two game servers and two login servers in 217 samples (two in 196 of them), peak
6,328 MB of private bytes for the two servers, free commit down to 3.9 GB while other lanes compiled. The three failures are the same
false leak: `m5b3_geo` (census at 00:40:13, "Leak probe ... no holder found" at 00:40:17) and `m5b` (00:53:13, probe at 00:53:14) exactly as
above; `m5b_geo` shows a second face of it: the probe, walking a geo-built world in a Debug build, was still running when
`ThreadPoolManager` shut down ("success: false in 5001 msec"), so its pin also left the Player alive past the runtime shutdown ("1 objects
still tracked", `live_counts.txt` 1 live Player, a "Live instance leak" ERROR for `AbyssRank`). Nothing in this lane can change that: the probe
belongs to `LeakCensus` (P4-02b) and `WorldLeakProbe` (P4-10), the final census to `CheckOutput` (P5-14).

**Closed, 2026-09-25, with the fixed probe** (the npc-leak lane's holder probe in the form header request m5b3-leak-h01 applies,
which posts no probe for a zero-threshold check such as the final census; `build/msvc` built at 04:33 from the tree as it is now). Each
gate that had failed, alone: `m5b` 239 s, `m5b_geo` 361 s, `m5b2` 173 s, `m5b2_geo` 331 s, `m5b3_geo` 395 s, all passed. Then the same
command twice: 1,220 s and 1,145 s of wall clock, 46 of 46 passed both times, at most two game servers and two login servers (two in 531 of
580 and 548 of 571 samples), peak 6,328 MB of private bytes for the two servers, at least 12.2 GB of physical memory available, other
lanes compiling beside (up to 32 `cl.exe`). Geo gates overlapped each other (`m5a_geo`/`smoke_geo` beside `m5b_geo`; `m5b3_geo` beside
`m5b_geo` and then `m5b2_geo`). `m5b`, `m5b_geo` and `m5b3_geo` still log the transient "Leak census: ... Player ... refcount 3" line at
shutdown, now with no "Leak probe" after it and "0 leaks written", exactly as M5b-3's tree did: the false leak above was the first probe's
pin. Between the two, the first run of this fix (a binary of 01:15-01:20, when the npc-leak lane had taken the probe back out of the tree
into that patch, so the server had no probe at all) had no census failure either, but one geo gate per run failed its "no watchdog dump"
assertion (M5b-3 Y13, M5b-2 X12): `MapRegion::activate`'s instant task (MapRegion.cpp:141, the AI ACTIVATE of the regions a player
enters) ran 9.8 s, 11.1 s and 6.3 s in `m5b3_geo` and 8.9 s in `m5b2_geo`, over the 5 s threshold. The dumps show the thread running, not
waiting for a lock, at 17-27 % of the machine's CPU. It is not the harness's (the task runs inside the server) and did not come back: the
same task takes at most 88-122 ms in each gate alone (`MethodStats.log`), and no gate log of the two runs above has a slow task. The code
those gates executed was the same (the probe runs only after a census report, and none came before the shutdown), and the machine had less
free commit memory then (6-11 GB against 11-14 GB), so the likeliest cause is load, a stall of the server process on a busy machine,
which any watchdog-gated run can meet: a geo gate that fails only on a watchdog dump should be rerun alone before it is read as a
regression.

## M5c stage 0 (harness-a lane): the dialog decoders and builders, the shared inventory model

m5c-plan.md G-02's first part and the harness side of G-01 (the `m5c-economy` oracle is in `tools/oracle`, README "M5c economy oracle").
Nothing here changes a gate's behaviour; the M5b-3 gate compiles against the lifted model unchanged.

| Area | As built | Reason |
|---|---|---|
| `decoders/EconomyDecoders.{h,cpp}` | `SM_DIALOG_WINDOW`, `SM_PRICES`, `SM_TRADELIST`, `SM_SELL_ITEM`, `SM_REPURCHASE`, `SM_QUESTION_WINDOW`, each from its Java `writeImpl` (m5a-plan.md D9). The literal constants are verified (`SM_DIALOG_WINDOW`'s `writeH(0)` at :34 and its last short for every page but MAIL and TOWN_CHALLENGE_TASK, `SM_TRADELIST`'s `writeD(100)`, `SM_REPURCHASE`'s `writeD(1)`, `SM_QUESTION_WINDOW`'s `writeD(0)`), every `? 1 : 0` flag must be 0 or 1, and `SM_QUESTION_WINDOW`'s flag must agree with its range (`rangeOrCooldownSeconds > 0 ? 1 : 0`) | the stage-0 dialog path (X1-X4, X8, X15, X25, X26); the exchange, mail, store and craft packets of G-02 join the same file in stage 1 |
| `SM_QUESTION_WINDOW`'s parameters | A `writeS(null)` and a `writeS("")` are the same lone NUL char on the wire: both decode as `""` | Java writes `String.valueOf(params[i])` or null past the given ones (SM_QUESTION_WINDOW.java:307-308); the decoder cannot and need not tell them apart |
| `GameSession` builders | `CM_QUESTION_RESPONSE` (50), `CM_SHOW_DIALOG` (52), `CM_CLOSE_DIALOG` (53), `CM_DIALOG_SELECT` (54) in their readImpl field order; the fields the server reads and drops are written as 0, and `CM_DIALOG_SELECT` takes its arguments in read order (action, extendedRewardIndex, lastPage, questId, unk) | as the M5b-3 builders |
| `InventoryModel.{h,cpp}` | M5b-3's file-local model lifted out of `M5b3ScenarioTest.cpp` as the plan asks (m5c-plan.md G-02, A-11). Two changes, both additive: the storage and kinah constants became `ModelItem::CUBE`, `ModelItem::REGULAR_WAREHOUSE` and `InventoryModel::KINAH_ITEM_ID` (so they cannot collide with a gate's own `LOCATION_*`/`KINAH_ITEM`), and `followPackets` follows any growing packet list besides a session's recorder, which lets `InventoryModelTest` drive the model without a connection | X16's per-client models; M5b-3's gate keeps its `LOCATION_CUBE`/`LOCATION_WAREHOUSE`, now aliases of the model's |

## M5c stage 1

The rest of m5c-plan.md G-02 and G-01 (harness-b lane), and G-03's oracle accessor. Nothing here changes a gate's behaviour: no gate reads the
new decoders, builders or accessor yet (G-03, stage 2's gate-1 lane).

| Area | As built | Reason |
|---|---|---|
| `decoders/EconomyDecoders.{h,cpp}` | `SM_EXCHANGE_REQUEST`, `SM_EXCHANGE_ADD_ITEM` (the template id written BEFORE the object id, :31-32, the reverse of `SM_REPURCHASE`), `SM_EXCHANGE_ADD_KINAH`, `SM_EXCHANGE_CONFIRMATION`, `SM_MAIL_SERVICE` with its six service ids (0 mailbox state, 1 MailMessage, 2 letter list, 3 letter read, 5 attachment taken, 6 letters deleted; any other id is refused - no constructor sets it), `SM_PRIVATE_STORE` (an empty body is a null store, :27), `SM_PRIVATE_STORE_NAME`, `SM_CRAFT_UPDATE`, `SM_CRAFT_ANIMATION`, `SM_LEARN_RECIPE`, `SM_RECIPE_DELETE`, `SM_RECIPE_LIST`, each from its Java `writeImpl`; the literals are verified (`SM_MAIL_SERVICE`'s `writeC(0)` of the list, the read's AP int and byte, the attachment's `writeC(1)`, the item arm's `writeD(1)`/`writeD(0)`, the 20 zero bytes of a letter without an item; `SM_LEARN_RECIPE`'s and `SM_RECIPE_LIST`'s `writeC(0)`), and so are the Java invariants visible on the wire: the recipient id written twice, a letter's item ids both or neither, the read flag `? 0 : 1`, `SM_CRAFT_UPDATE`'s message id per action with a `writeS(null)` for NORMAL/CRIT_BLUE/CANCELLED, and the morph skill's delay 1000 the constructor forces | G-02 (X9-X14, X17-X21a) |
| `SM_MAIL_SERVICE`'s counts | Service 0 writes the four mailbox counts as shorts; services 3 and 6 pack them into `total + unread * 0x10000` and `express + blackCloud` (:133-134, :173-174), so `MailboxCounts` there holds total, unread and the SUM of the unread express and black-cloud letters (in `unreadExpress`, `unreadBlackCloud` 0) | only the sum is on the wire |
| `SM_MAIL_SERVICE`'s list | `lastPacket` is `count < 0` of `writeH(isLastPacket ? -size : size)`; an empty last part writes -0 == 0 and decodes as not last | the wire cannot tell them apart |
| `SM_EXCHANGE_ADD_ITEM`'s object id | For a part of a stack the item is `ItemFactory.newItem(itemId, count)` (ExchangeService.java:139-143): the packet carries a NEW object id, not the stack's; the test's fixture uses a distinct id | a gate must not match that id against the stack |
| `SM_SKILL_LIST`, `SM_CUBE_UPDATE`, `SM_EMOTION` | not written again: M5a's `decodeSkillList` already reads both forms (the list and the one-skill form with a message), M5b-3's `decodeCubeUpdate` and M5b's `decodeEmotion` exist | G-02 "if M5a's decoder does not cover it" |
| `GameSession` builders | the stage-1 client packets (`CM_BUY_ITEM` 51, `CM_EXCHANGE_REQUEST`/`ADD_ITEM`/`ADD_KINAH`/`LOCK`/`OK`/`CANCEL` 63-64, 66-69, `CM_PRIVATE_STORE` 119, `CM_PRIVATE_STORE_NAME` 120, `CM_SEND_MAIL` 132, `CM_CHECK_MAIL_LIST` 133, `CM_READ_MAIL` 134, `CM_GET_MAIL_ATTACHMENT` 136, `CM_DELETE_MAIL` 137, `CM_TUNE` 235, `CM_SELECT_DECOMPOSABLE` 236, `CM_TUNE_RESULT` 238) and stage 2's `CM_RECIPE_DELETE` 89 and `CM_CRAFT` 141, in their readImpl order. Nothing is clamped: 37 `CM_BUY_ITEM` entries or a count above 20,000 go out as given, so a test can reach the audit arms; only more than 65,535 list entries (which no readUH count can carry) throw `std::invalid_argument`. `CM_CRAFT`'s material count comes before its craft type (:37-38) | K-01, K-02, C-04 byte vectors and G-03 |
| `EconomyOracle.{h,cpp}` | G-03's accessor for `oracle.py m5c-economy`: `EconomyRequest` -> `economyArguments` (one flag per set field, lists repeated, numbers in their shortest round-trip text so a spot is not rounded to six decimals, `--profile` winning over `--no-profile`), `parseEconomy` -> `EconomyAnswer` with every value the oracle writes as null kept `std::nullopt`, and `runEconomy` over `Oracle::run`. Besides the talk, price, item, character and C19 fields, the answer carries the rows' messages: the recovery's and the cube's `yes` (deltas, `EconomyMessage`s with id and value, `SM_CUBE_UPDATE`) and not-enough-kinah messages, the recovery's no-exp message, the removal's base price and message ids, the identification's animation and tune count, the extraction's and the socketing's message ids (with the refusal's message, stone loss and audit line), the equip refusal's message name, restrict_max, item race and not-modelled checks, the craft learn's `supported` and its not-enough-kinah message name (the oracle writes no id for it), and each craft spot's outcome and the other ovens in range. `EconomyOracleTest` parses the real oracle's answer (trimmed, embedded) and asserts every field it reads (a schema perturbing each of the 196 field reads is killed in full); `EachFieldIsReadFromItsOwnKey` edits the real answer's equal siblings apart, `NullsStayNull` swaps in the refusal shapes of a second real answer; `OracleRunTest.TheEconomyBindingAsksTheRealOracleWhatItWasGiven` runs the whole binding against oracle.py (labelled `scenario;realdata` by the existing `^OracleRunTest\.` rule, skipped without `AION_TEST_PYTHON`) | §18.4: "The gate lane writes the C++ accessor"; written here since everything else of the lane was done |
| `tools/oracle` (`m5c/sanctum.py`, `m5c/economy.py`'s `socketing`) | the Daeva seed and its enter-world learn list, the Sanctum master, vendors, ovens and spots, C19's exact kinah, and whether a manastone fits an item (README "M5c economy oracle"). The sockets follow `Item.getSockets`' `isWeapon() \|\| isArmor()` guard: an item of any other equip type (EXTRACT_SWORD, PLUME, ...) has none whatever its `m_slots` (`noSocket`, "Manastone socket overload", the stone lost). `learnedSkills` lists each level's skills in `SkillTreeData.getTemplatesFor`'s order, the race's rows before the PC_ALL ones | G-01's rest |
| The Daeva's stored skills | The oracle models the skills the character had before the seed as the autolearn skills of levels 1 to `--daeva-old-level` of its STARTING class, so it accepts an old level of 1..9 only | a Daeva's own earlier skills are not static data; the gate seeds a Warrior that never was a Daeva |

## M5c stage 1 integration: the wall-clock cron jobs a gate can meet

In the integration's gate run (2026-09-27, a Sunday) `gs.scenario.m5b3` failed only its Y13 bar: its game server was up at **18:50 local
time**, when `CronJobService`'s Ahserion job fires (`gameserver.siege.panesterra.ahserion.time = 0 50 18 ? * SUN`, siege.properties:47;
CronJobService.java:62 schedules it whatever `gameserver.siege.enable` says), and `PanesterraService::startAhserionRaid` is `AION_UNPORTED`
(`PanesterraService.cpp:39`). The unported trace and `server_errors.log` then held that site and its ERROR line. Rerun alone at 19:01 it passed
(142.7 s); nothing in M5c's code was involved. `gs.scenario.m5b2_geo` was up at 18:50 as well but still loading its geo data, so its
`CronService` job was registered after 18:50 for the next Sunday. The same trap has two siblings: `LegionDominionService::startWeeklyCalculation`
(`AION_UNPORTED`, a hard-coded `0 0 9 ? * WED *`, CronJobService.java:70) and the Moltenus spawn (`gameserver.moltenus.time = 0 0 22 ? * SUN`,
not measured). A gate that fails its unported or ERROR bar on one of these three sites at one of these times is rerun before it is read as a
regression. A durable fix - the gate profiles moving the two configurable schedules out of any run, and a seam for the hard-coded one - is
left to the next P5-SC lane (m5c-plan.md §19.7).

## M5c stage 2 (gate-1 lane): the M5c gate part 1, G-06, G-07

m5c-plan.md G-03 part 1 (C0-C18, C20; C19 is stage 3's part 2), G-06 (P5-14, recorded in `P5-14.md`) and G-07, on harness-b's committed
decoders, builders, `InventoryModel` and `EconomyOracle`. Test infrastructure only, no Java counterpart; the rows say where the gate reads the
plan's §10 differently and why.

| Area | As built | Reason |
|---|---|---|
| `gs.scenario.m5c` (`M5cScenarioTest.cpp`) | `M5cScenario.Run`: S-0, C0-C18, C20a, C20 on two accounts online at once (A an Elyos Warrior, B an Elyos Mage), its own output directory `<bin>/scenario/m5c`, schema pair `aion_{gs,ls}_test_m5c_<hash>`, allow-list `m5c_partial_allowlist.txt`. Registered in `ScenarioTests.cmake` on gate slot 2, `LABELS "scenario;realdata"`, `TIMEOUT 2700`, the discovered case DISABLED as for every gate, no geo variant (D12). 206-225 s alone in a Debug tree (2026-09-28), of which S-0 is ~30 s and C0's eight oracle runs ~20-25 s | §10.5 |
| The expectations | `oracle.py m5c-economy` through `EconomyOracle.h` (the talk spots with `--direction 270`, windows, soul healing, cube, Seril's price, the mail commission, identification, extraction, socketing, the equip facts of the seeded items); `oracle.py m5c-trade --npc 798007 --item ID --count N` for SM_PRICES, the two windows, the buy prices, sell rewards, buy-back price and the refusals' message names (parsed file-locally: the accessor has no `m5c-trade` binding); `m5a-creation` for both starter inventories; `m5b3-item` for the TRADEABLE masks. `m5c-economy` and `m5c-trade` run with `--no-profile` and the server's own properties (`m5aProfile()` under the gate's keys) as `--set`, so the owner's `mygs.properties` never reaches their expectations; `m5a-creation` and `m5b3-item` take no profile flag and read no property. The game server itself still loads `./config/mygs.properties` (the harness's behaviour for every gate, "Loading: ./config/mygs.properties" in the gate log): today every key there but `gameserver.network.client.connect_address` (which no expectation reads) is also a `-D` of the gate, but a key the owner adds later would reach the server and not the oracles (left for integration). The system-message ids the oracles do not carry are `SM_SYSTEM_MESSAGE.java`'s, cited at the constant | the plan's "every case's expectations come from oracle.py m5c-economy / m5c-trade" |
| Seeds at the X2 spot | Both characters are created, then their accounts DISCONNECT (`CM_QUIT(0)` at the character list), the band spot is written into `players.x/y/z`, and both log in again (M5a's Q5 relogin: `CM_MAY_LOGIN_INTO_GAME`, 1.5 s for the re-entry time, `CM_ENTER_WORLD`). C13's `recoverexp` and C14's `exp` seeds are written the same way; inventory rows (C14's items, B's kinah, `tune_count = -1`) are read fresh at enter world and only need the character out of the world | m5c0-client-session.md F-3: the account's `players` rows are loaded at connect and saved back at logout, so a seed written at the character list is lost |
| Kinah rows are deltas | X5, X7, X8, X10, X12-X15, X25, X26 compare the kinah update with the kinah the client was last told plus the row's own delta; only X27 ("leaving 0") and X16 (the ledger) are absolute | §10.4's "must stay green" half: with absolute amounts a wrong price in C5 failed every later kinah row. `getTaxes` truncated fails X1, X5, X25, X27 (and X16) and leaves X7, X8, X13, X15, X26 green, as §10.4 lists |
| X16's ledger | Per character and per item id, from `m5a-creation`'s starter items plus every buy, sale, buy-back, exchange, store sale, mail and commission with the oracles' prices; compared with the clients' models before C14's quit, with the `inventory` rows after it (per character and summed over both), with the re-entry's `SM_INVENTORY_INFO`, and again after C20a's quit ("and every later quit", C15-C18 included). The disjointness of the two clients' models is checked after every step of C8-C13 | §10.3 X16 |
| X16's disjointness cannot fire on this script | Every transfer of C8-C13 moves a PART of a stack (10 of 100 potions, 5 of B's bandages, the store's 3 + 2 of 100, 5 mailed potions), and a part becomes a new object id (`ItemFactory.newItem`: ExchangeService.java:139-143, TradeService.java:236, MailService.java:135; the store's `ItemService.addItem(buyer, item, count)` adds by item id, PrivateStoreService.java:165), so no mutant can put one object id into both models. The row is kept (it costs nothing and would catch a whole-stack path); the duplication mutants §10.3 attributes to it are caught by the per-id ledger instead (the review's `x10`, the exchange keeping the giver's potions: X16 fails with `162000002: 210/200`) | the review of 2026-09-28; a whole-stack exchange would change X9-X11's expectations and is left to a later lane if the row is meant to prove anything |
| Two of §10.3's "mutation it kills" entries the gate cannot kill deterministically | X23's "the sockets rolled from `max_enchant_bonus`": the Plainsman's Tunic (item_templates.xml:189490) has `option_slot_bonus` 1 and no `max_enchant_bonus`, so that mutant always rolls 0 sockets, inside the accepted [0, 1] (the review's `x23`: the whole gate passed, 210 s). X27's "the armour count range [1, 3] used for a weapon": `EnchantService.cpp:399` would then give 1-3 stones, and X27's [2, 5] fails only when a 1 is rolled, about one run in three. Both belong to the unit tests of the identification and the extraction (P-07 / E-01); m5c-plan.md §10.3 is corrected at integration | the review of 2026-09-28 |
| X3 and X15 are non-fatal; X8's count from the model | X3's "one `SM_MAIL_SERVICE`" and X15's "an `SM_STATUPDATE_EXP`" were fatal `ASSERT`s, so their mutants skipped C12-C18 (C14-C18) and X22 then failed on the handlers' `created 0`, a cascade into unrelated rows. Both are `EXPECT`s with a guarded read now, as the delta rows are. X8's "the stack back to 100" compared with a literal while C0 only asserts the starter stack holds at least 100; it now compares with the stack's count in A's model before C6's sale | the review of 2026-09-28; the CaseLog note (non-fatal failures keep the later rows running, §10.4's "must stay green" half) |
| X10 reads the removals themselves | Besides the model's counts, A's stack and B's stack must each get a `DEC_ITEM_USE` (0x16) update to the exchanged count - `Storage.decreaseItemCount` of `removeItemsFromInventory` - anywhere in the exchange | `addItem`'s fake `PUT_TO_EXCHANGE` (0x25) update already shows 90 and 15, so a trade that skips the giver's removal left the models right and only X16 failed; with the removal packet X10 fails (mutant `exchange-no-removal`) |
| X2 "and nothing else" | The window's packets outside the async set, an announced npc's `SM_LOOKATOBJECT` (whatever it looks at) and the other character's own `SM_MOVE`/`SM_EMOTION`/`SM_PLAYER_STATE` must be exactly the one `SM_DIALOG_WINDOW` | Java's `onSimpleTalk` sets the dialog npc's target, and `NpcController.onTargetChanged` broadcasts `SM_LOOKATOBJECT(npc, talker)` (NpcController.java:78-93), which the M5a async set only allows with an npc or no target |
| X15's exp | `STR_GET_EXP2` then `STR_SUCCESS_RECOVER_EXPERIENCE` (in order), the kinah, `SM_STATUPDATE_EXP`'s recoverable exp 0 (decoded in the gate file from SM_STATUPDATE_EXP.java:32-38), and after C14's quit `players.exp` = 1,000 and `recoverexp` = 0 | A at level 1 levels up on the 1,000 exp, so the wire's exp is shown within the new level; the stored total is the oracle's delta |
| C12 | B quits to the character list (`CM_QUIT(1)`), not disconnected; the unread flag is read from a new `CM_CHARACTER_LIST` | `getOrLoadPlayerCommonData(name)` loads a fresh copy for a character that is not in the world (PlayerService.java:242-247), so the offline path is taken; `haveUnread` is a query at list time |
| C20a always runs | `cases.run`, not `runCase`: both characters quit (fully) even after a fatal failure, and the rows of the last quit (X16, X23, X26, X28) are read only when the script got there | X22 asserts nobody is online at the end (the per-connection classes bounded by 0, `Item` a strict zero) |
| X22's transfer rows | `m5a_summary.txt`'s rows of `Exchange`, `ExchangeItem`, `TradeList`, `TradeItem`, `RepurchaseList`, `TradePSItem`, `Letter` and the three request handlers the gate answers: live 0 AND created > 0; `CraftingTask` live 0 only (part 1 runs no craft); `PrivateStore` and the bare `RequestResponseHandler` are no rows | G-06, `P5-14.md` "M5c stage 2": an `OwnedPart` and an abstract base are invisible to the counters; `TradePSItem` and `Player` bound the store |
| X23 before identification | The seeded tunic must load unidentified (`EnchantInfoBlobEntry` writes -1 for its sockets and bonus); the equip before identification is not tried | §10.2 C15 equips only after; the refusal (Equipment.java:163-167) is P-07's unit test |
| G-07: the configurable wall-clock cron jobs | The census of every cron job a gate's server schedules (the review of 2026-09-28 found two the lane had missed): six configurable schedules fire into unported code or into the world - the Ahserion raid (Sunday 18:50, `AION_UNPORTED`), the Moltenus spawn (Sunday 22:00, a boss in Reshanta), the housing `AuctionEndTask` (Sunday 12:00, `AION_PARTIAL` at `AuctionEndTask.cpp:81`) and `AuctionAutoFillTask` (Monday 00:00, `AuctionAutoFillTask.cpp:38`, as `gameserver.housing.auction.enable` is true), `AbyssRankUpdateService`'s rank update (daily 00:00, `:37`) and GP loss (daily 12:00, `:58`). `ScenarioServers::m5aProfile` sets all six keys (`gameserver.siege.panesterra.ahserion.time`, `gameserver.moltenus.time`, `gameserver.housing.auction.end_time`, `gameserver.housing.auction.auto_fill.time`, `gameserver.topranking.updaterule`, `gameserver.topranking.daily.gploss.time`) to `0 0 0 1 1 ? 2000,2100,2101`, so every scenario gate inherits them. The lane's `0 0 0 1 1 ? 2100` was replaced: the housing keys are read by `AbstractCronTask`, whose constructor needs the fire time after the next one and a fire time before now (`findLastPlannedRun`, AbstractCronTask.java:108-118) - a single year throws a NullPointerException in `AuctionEndTask.getInstance()` and the server does not start (the gate's `g07-single-year` run below), and a schedule with no past fire time never leaves that loop. `ScenarioServersTest.TheWallClockWorldEventsAreScheduledPastEveryGateRun` pins the six `-D` flags, that a gate's own value still wins, that `CronExpression` accepts the expression with its next fire time decades away and every year inside Quartz's current year + 100; `TheHousingCronTasksAcceptTheWallClockSchedule` builds an `AbstractCronTask` from each housing key and asserts its next run decades ahead and its last planned run decades back (so neither `shouldRunOnStart` runs the task at startup). Not in `m5c.properties.example` or any Java-tree profile | §20.4: a past-only year is refused at startup ("the given trigger will never fire"); the example's lines are what the owner copies into `mygs.properties` to play |
| G-07: the hard-coded jobs, and the rerun rule | No seam: the gate records its server's wall-clock window and, when a hit of `LegionDominionService` is in the unported trace and the window contains a Wednesday 09:00 local time, its failure message names the hit as the hard-coded cron's (`CronJobService.cpp:185-187`) and says to rerun. The failure stays. The census's other jobs no key moves reach no unported code: `QuestEngine`'s reset notice and `AtreianPassportService`'s stamp reset (both daily 09:00) only send packets to the online players, `MaintenanceTask` (Monday 00:00) reaches its `AION_PARTIAL` only for an owned house (no gate owns one), `LimitedItemTradeService`'s jobs reset counters, `PlayerLimitService`'s clears a map, `EventService` checks every 5 minutes with every event disabled. **The rerun rule** of "M5c stage 1 integration" therefore reads: a gate that fails its unported bar on `LegionDominionService` with its server up at Wednesday 09:00, or a packet-exact row with its server up at 09:00 (the two daily notices), is rerun before it is read as a regression; the Ahserion, Moltenus, auction and rank sites no longer fire in a gate run | §20.7 item 2: a production seam would be a deviation only the owner can decide |
| Allow-list | `m5c_partial_allowlist.txt`: §A `QuestEngine.cpp:111`, `BaseService.cpp:18`; §B the four sites of G-07's moved cron jobs (`AbyssRankUpdateService.cpp:37`, `:58`, `AuctionEndTask.cpp:81`, `AuctionAutoFillTask.cpp:38`: only their cron reaches them, so a hit means a profile key no longer reaches the server); §C M5b-3's rows but the two rank rows (all eleven name an `AION_PARTIAL` line) | §10.1 copied the m5b2/m5b3 lists' §A; stages 0-1 added no partial. "§B nothing new" and "§C M5b-3's" are the plan's; the four §B rows are G-07's (the review of 2026-09-28): the rank rows were §C as "a property of the run's LENGTH", but they fire at 00:00 and 12:00 whatever the length (M5a's list has neither, so an M5a run at those times failed) |
| Slot table | `gs.scenario.m5c` 225 s joins slot 2 (1,276 s against slot 1's 1,062 s). The fix round's three runs took 249.0 s (C0's oracles 48 s on a loaded machine), 217.3 s and 210.7 s | §10.1/§10.5 put it in slot 2; the next gate joins slot 1 |

**Mutation proof (§10.4)**, mutant schemata switched by `AION_M5C_MUTANT` in the production files, built once into `build/c2-gate`'s server and
the sources restored by sha256 right after each build (no schema survives in the tree or the final binaries). One gate run per mutant:

| Mutant | Failed | Stayed green | Note |
|---|---|---|---|
| `PricesService::getTaxes` truncates (113 → 112) | X1, X5, X25, X27, X16 | X7, X8, X13, X15, X26 | as §10.4 |
| `TradeList::calculateBuyListPrice` `>=` → `>` | X27 | X5, X6 | |
| `SM_TRADELIST`/`SM_SELL_ITEM` write the ordinal | X4 | X2 | |
| `isInTalkRange` without the "+ 1" | X2 | X1 and every other row | the band spot is refused with STR_DIALOG_TOO_FAR_TO_TALK |
| `Npc::canSell` → false | X4 (and X5, X6, X27, X16: nothing can be bought) | X2 | |
| `isInteractionAllowed` → false | X2 (page 1011), X4 | X1 | the later npc rows cannot run |
| `identifyItem` without the tune count | X23 (the blob's -1s, the equip, `tune_count`) | X24 | |
| `removeManastone` skips `storeManaStones` | X25 (the row survives) | X24 | |
| `breakItem` keeps the weapon | X27, X16 (the last quit) | X26 | |
| the cube's npc expansion not counted | X26 (`SM_CUBE_UPDATE`, `npc_expands`) | X25 | |
| `performSellToShop` deletes every sold stack | X7, X8 | X5 | X16 stays green: the buy-back returns the whole deleted stack, so the counts are restored |
| `ExchangeService::addItem` without the tradeable check | X9, X16 | X10 | |
| `removeItemsFromInventory` skips the removal | X10, X16 (and X11) | X9 | caught by X10 only since the removal-packet check above |
| `confirmExchange` trades without the partner check | X9 | X10, X11 | |
| `getAttachments` leaves the item on the letter | X13 | X14 | X16 stays green: the letter is deleted with its reference, the item is B's |
| `updateRecipientMailbox` skips `updateOfflineMailCounter` | X14 | X13 | |
| `cleanUpExchanges` removes nothing | X22 (`Exchange 2 2`, the census, the ERROR bar), X11 | X10 | |
| "an Exchange kept in `exchanges`" on the trade's path only | **nothing** | all | Java heals it: `CM_QUESTION_RESPONSE` cancels a trading responder's exchange before answering (CM_QUESTION_RESPONSE.java:39-44), which cleans both partners' entries, so X11's next request opens and nothing survives to X22. Only a leak that survives both logouts reaches X22 (the mutant above) |
| (G-06) `zeroLiveClasses` without `Exchange`; with `PrivateStore`; `summaryLiveClasses` without the exchange handler | `CheckOutputTest` (the new cases) | the rest of `CheckOutputTest` | |
| (G-07) the profile without `moltenus.time`; both keys in the year 2000 | `ScenarioServersTest.TheWallClockWorldEventsAreScheduledPastEveryGateRun` | the other 13 `ScenarioServersTest` cases | the lane's two keys; the fix round's six below |
| (review `x6`) `validateBuyItems` returns true | X6 (:1501, message 1300759 instead of 1300335) | every other row | run by the review of 2026-09-28 (`AION_REVIEW_G1_MUTANT`), recorded here |
| (review `x12`) the emptied store is not closed | X12 (:1867, no `EMOTION_CLOSE_PRIVATESHOP` for A, seen by A and by B) | every other row | the review's run |
| (review `x24`) `socketManastoneAct` does not consume the stone | X24 (:2305, the stone not deleted), X16 (the last quit, `167000226: 1/0`) | every other row | the review's run |
| (review `x28`) `enchantItemAct` does not consume the stone | X28 (:2498), X16 (the last quit, `166000191: 5/4`) | every other row | the review's run |
| (review `x23`) the sockets rolled from `max_enchant_bonus` | **nothing** (210 s) | all | not killable by the gate: the tunic has no `max_enchant_bonus` (the row above) |

**The review's fix round** (2026-09-28): schemata switched by `AION_M5C_MUTANT` in `DialogService.cpp`, `RepurchaseService.cpp` and
`ScenarioServers.cpp`, built once into `build/c2-gate`, the sources restored by sha256 right after the build (no schema in the tree or in the
final binaries). One gate run per mutant:

| Mutant | Failed | Stayed green | Note |
|---|---|---|---|
| `x3`: `onCloseDialog` does not close the mailbox state | X3 only (C11 :2016, two `SM_MAIL_SERVICE`) | C12-C18, C20a, C20 (X22's handler rows included) | the review's run had skipped C12-C18 at the fatal `ASSERT` and failed X22 |
| `x15`: `resetRecoverableExp` skipped | X15 only (C13 :2107, no `SM_STATUPDATE_EXP`; C14 :2161-2162, `players.exp` 0 and `recoverexp` 1,000) | C14's X16 and C15-C18, C20a, C20 | the review's run had skipped C14-C18 |
| `x8`: the buy-back adds one potion fewer | X8 (C7 :1583 and the stack count at :1587, 99 against the model's 100), X16 (C14 and the last quit) | every other row | X8 now compares with the stack's count before C6's sale |
| `g07-now`: the auction end, auto fill, rank update and GP loss every minute (seconds 0, 15, 30, 45) | X22 only: the four §B rows hit (`:37` 4 times, `:58`, `AuctionEndTask.cpp:81` and `AuctionAutoFillTask.cpp:38` 3 times each) | S-0 and C0-C20a | the census's jobs do fire into those sites, and the profile keys govern them |
| `g07-single-year`: the housing keys at the lane's `0 0 0 1 1 ? 2100` (the review's suggested fix) | S-0: the game server does not start (`startup step 22: AuctionEndTask.getInstance()`, `NullPointerException: Cron expression has no next fire time`) | – | why FAR_FUTURE_CRON has three years |
| (unit) `g07-no-auction-end`, `g07-no-gploss`, `g07-single-year`, `g07-now` | `TheWallClockWorldEventsAreScheduledPastEveryGateRun` (all four), `TheHousingCronTasksAcceptTheWallClockSchedule` (all but `g07-no-gploss`; `g07-single-year`: "the housing task's constructor throws ... Cron expression has no next fire time") | the other 13 `ScenarioServersTest` cases | |

C19 (X17-X21a, X22's `CraftingTask` created > 0) and §10.4's craft rows are part 2's (stage 3).

## M5c stage 2 integration (2026-09-28)

m5c-plan.md §21. The lane's leftover for the earlier lists is applied: `AbyssRankUpdateService.cpp:37` and `:58` move from section C to
section B of `m5b_partial_allowlist.txt`, `m5b2_partial_allowlist.txt` and `m5b3_partial_allowlist.txt`, as in the m5c list, and their
"property of the run's LENGTH" comments become HISTORY lines (the jobs fire at 00:00 and 12:00 daily, and G-07's keys move both past every
gate run; M5a's flat list has neither row). All 45 rows of the five lists still name an `AION_PARTIAL(` line (checked by a script); stage 2
added and removed no `AION_PARTIAL`, so no row shifted.

| Mutant | Failed | Stayed green | Note |
|---|---|---|---|
| `rank-now`: a schema of `ScenarioServers.cpp` (switched by `AION_C2I_MUTANT`, built once into `build/msvc`'s `aion_gs_scenario_tests`, the file restored by sha256 right after the build) sets `gameserver.topranking.updaterule` and `gameserver.topranking.daily.gploss.time` to `0/15 * * ? * *` | `gs.scenario.m5b` Q1 (both rows hit 12 times), `gs.scenario.m5b2` X12 (11 times), `gs.scenario.m5b3` Y13 (7 times): the six section-B `EXPECT_EQ`s and nothing else | every other row of the three gates | revert: the rebuilt clean binary (no schema string) passes all three gates and `ScenarioServersTest` (18 of 18); the clean two-at-a-time run before it passed them as well (m5c-plan.md §21.6) |

The two-at-a-time run of every gate (§21.6) logged `Scheduled AuctionEndTask with cron expression: 0 0 0 1 1 ? 2000,2100,2101` and the
same expression for the ranking update in every scenario gate, and the shipped schedules in the three smoke tests, as G-07 intends.

## M5c stage 3 (gate-2 lane): the M5c gate part 2, C19

m5c-plan.md G-03 part 2 (§10.2 C19, §10.3 X17-X21a and X22's craft rows, §10.4's craft mutants) on HEAD `5cccfd6a4`, where the M5d engine's
`QuestState` restore (m5c0-client-session.md F-1) and C-01 are merged. Test infrastructure only: `M5cScenarioTest.cpp` and the slot table's
comment in `ScenarioTests.cmake`. No production file and no oracle file changed: `oracle.py m5c-economy`'s `--daeva` and `--craft-*` blocks
(`m5c/sanctum.py`, stage 1) answer every field C19 reads, and `EconomyOracle.h` parses all of them but `craft.learn.yes`. (The review below
added fields to `m5c/sanctum.py`'s blocks, which the gate reads file-locally too; `EconomyOracle.h` is still unchanged.)

| Area | As built | Reason |
|---|---|---|
| C19 | Between C18 and C20a: A disconnects (`CM_QUIT(0)`), `players.old_level` is read, `m5c-economy` runs with the gate's properties (`--no-profile`, `--set`), `--race ELYOS --direction 45 --daeva GLADIATOR --daeva-old-level <old> --craft-recipe 155001381 --craft-tool 150000009 --craft-distance 3/7/12`; the seed is written with the account disconnected - `player_class`, `exp` 126,069, `world_id` 110010000 at the oracle's `seedSpot` (Hestia's near spot), a `player_quests` row (1006, COMPLETE, `complete_count` 1), the kinah row set to `exactKinah` 3,640 and one Inina (two since the review below: the recipe's one and `SURPLUS_ININA`) through `seedInventoryItem`; A logs in again (M5a's Q5) into Sanctum, talks to Hestia, `CM_DIALOG_SELECT(46)`, answers 900852 yes, walks to Luelas' near spot, opens BUY and buys 2 Salt, sends `CM_CRAFT(0, 150000009, 155001381, oven, {152001001: 1, 169400096: 2}, 0)` from 7 m, 12 m and 3 m, waits for the end, sends `CM_RECIPE_DELETE(155001381)`, disconnects and reads the rows. 56-64 s of the run; `gs.scenario.m5c` takes 265-283 s alone (four runs), the slot table says 283 (since the review's fix: 60-71 s and 265-289 s, the slot table says 289) | §10.2 C19; F-3 (the account's `players` rows are loaded at connect and saved at logout) |
| The old level | Read after the disconnect and handed to `--daeva-old-level`, so the learn list is the oracle's for the level the enter world really starts from (`PlayerLeaveWorldService.java:148` stores it, `PlayerEnterWorldService.java:204` reads it). It is 2 in every run: A levels up on C13's 1,000 exp | the oracle's default (1) would name the level-2 Warrior skills A already has |
| `--direction 45` | The craft spots of the oracle lie along `--direction`; along 45 degrees the 7 m and 12 m spots are in no other oven's `checkCraft` range, the default 0 leaves oven 104 in range of the 7 m spot. The 3 m spot has oven 104 in range along every direction tried (0-315 by 45); `CM_CRAFT` names its target, so that does not matter | the `otherToolsInCheckCraftRange` field is there for this choice |
| `craft.learn.yes` | Read file-locally (`parseLearnYes`, from the same JSON text `parseEconomy` reads): the yes's kinah delta (-3,500) and the skill and level it teaches (40001 at 1). `EconomyOracle.h` is not changed | its parser and its test fixture are harness-b's; the gate reads two more fields |
| X21a | The level of the enter world's last `SM_STATS_INFO` against the oracle's 10; the union of the burst's full `SM_SKILL_LIST`s (message 0) against the oracle's 41 skills as sets (missing and extra named), the swap (30001 out, 30002 in) and every level-10 skill named on its own; the burst's `SM_LEARN_RECIPE`s sorted against the three morph recipes, the union of its `SM_RECIPE_LIST`s as a set; `player_recipes` after the quit equal to those three | a Java HashSet's order is not asserted (m5c-plan.md §7); the message-0 filter leaves out a one-skill update |
| X17 | Hestia's window (one `SM_DIALOG_WINDOW`, page not asserted: a quest handler may answer first since M5d, and the oracle does not model it); the question's id, its three parameters - the profession's `ChatUtil.l10n` name compared as the oracle's three UTF-16 code units converted to UTF-8, as the decoder returns them -, sender and range; the yes: one kinah update of the oracle's delta, one one-skill `SM_SKILL_LIST` (40001 at 1, message 1330061, SkillLearnService.java:49), `SM_LEARN_RECIPE`s equal to the oracle's `[155001381]`; the kinah left is exactly the Salt's price; the Salt: one kinah update to **0** and one `SM_INVENTORY_ADD_ITEM` of 2 | §10.3 X17 |
| X18 | 7 m: `STR_COMBINE_TOO_FAR_FROM_TOOL` (1330040), exactly one `SM_CRAFT_UPDATE` (action 4, the skill and the product), exactly `SM_CRAFT_ANIMATION(A, oven, 0, 2)`, no item packet and the component counts unchanged; 12 m: no `SM_CRAFT_UPDATE`, `SM_CRAFT_ANIMATION`, system message or item packet, counts unchanged | §10.3 X18; `CraftService.sendCancelCraft` |
| X19 | The window from `CM_CRAFT` to 1.5 s after `SM_CRAFT_UPDATE(5)`: the first update INIT (skill, product, bars 1000/1000), the second NORMAL with empty bars, the last SUCCESS with a full bar; exactly the three animations (…, 40001, 0), (…, 40001, 1), (…, 0, 2); both component stacks deleted and the counts 0 (since the review below: the Salt's deleted, the Inina's updated to the surplus 1); one `SM_INVENTORY_ADD_ITEM` of 2 Roast Inina; one one-skill `SM_SKILL_LIST` (40001 at 2, message 1330064); the shown exp +141 over the last shown value (the level-10 bar starts at 0) and `players.exp` 126,069 + 141 after the quit | §10.3 X19 |
| X20 | Between the start pair and the end: 4-14 progress updates (the oracle's), each NORMAL or CRIT_BLUE with a growing success bar and failure 0, the last one full; the time from `SM_CRAFT_UPDATE(0)` to `(5)` is `firstTickDelay + n x interval` +-750 ms (since the review below: each gap and the total +-250 ms) and within the oracle's 11-36 s (9-12 updates in 23.5-31.0 s in the runs). The gate waits for the end up to five times the oracle's longest craft plus 15 s, so that a craft that runs long is counted by X20 instead of being cut off by the wait | §10.3 X20; the min-step mutant needs about 35 ticks on average (43 in its run, 108.5 s) |
| X21 | `SM_RECIPE_DELETE(155001381)`; after the quit no 155001381 in `player_recipes` and `player_skills` 40001 at the oracle's level 2 | §10.3 X21 |
| X16 for A | The ledger goes on: the seed (kinah set to 3,640, +1 Inina; +2 since the review below), the learn, the Salt, the craft; compared with the Sanctum enter world's `SM_INVENTORY_INFO`, A's model before C19's quit and C20a's `inventory` rows | §10.3 X16 "and every later quit" |
| X22 | `CraftingTask` and `CraftSkillUpdateService_RequestResponseHandler` join the rows that must be live 0 with created > 0 (`0 1` in every run); part 1's guard (`CraftingTask` live 0 only) is gone | G-03 part 2 |
| The first automated Sanctum entry (W-16) | 362 objects spawned in 110010000 at startup; A's enter world, the talk, the purchase, the three crafts and the quit reached no `AION_UNPORTED` site and no new `AION_PARTIAL` site (the allow-list is unchanged), wrote no ERROR line, and `CheckOutput`'s census and live counts stayed clean | m5c-plan.md §21.2 |

**Mutation proof (§10.4)**, production schemata switched by `AION_M5C_MUTANT` (`c19-*`), built in two batches into `build/c3-gate`'s server
only (the test binary was not rebuilt), the sources restored by sha256 right after each build; one gate run per mutant. The rebuilt clean
binaries hold no schema string.

| Mutant | Failed | Stayed green | Note |
|---|---|---|---|
| `c19-daeva`: `PlayerCommonData::updateDaeva` ignores the quest list | X21a (`SM_STATS_INFO` level 9; skills missing 169, 246, 249, 348, 519, 758, 769, 2891, 2981, 30002, 30003, 40009, extra 30001; no `SM_LEARN_RECIPE`; empty `SM_RECIPE_LIST`), X17 (no question: its fatal row ends C19), X22 (`CraftingTask 0 0`, `CraftSkillUpdateService_RequestResponseHandler 0 0`) | S-0, C0-C18 (X1-X16, X23-X28), C20a | as §10.4 |
| `c19-morph`: `SkillLearnService::onLearnSkill` without `isMorphSkill()` | X21a only (`SM_LEARN_RECIPE`, `SM_RECIPE_LIST`, `player_recipes`) | X17, X18-X21, X22 | as §10.4 |
| `c19-consume-first`: `checkCraft` consumes the materials before its checks | X18 (two `SM_DELETE_ITEM` at 7 m; the counts at 7 m and 12 m), X19 (the 3 m craft is refused for want of components: its fatal row ends C19), X22 (`CraftingTask` created 0) | X17, X21a, part 1 | **§10.4 lists X19 as green; it cannot be with the exact seed** - the refused 7 m craft took the only Inina and Salt (with the review's surplus Inina the only Salt: rerun as `c3f-consume-first` below) |
| `c19-min-step`: `analyzeInteraction` without the 70 minimum | X20 (43 progress updates, 108,502 ms against 36,750) | X19 (product, level, exp), X21, X21a, X22 | as §10.4 |
| `c19-race`: `getAutolearnRecipes` without the race filter (C++ keeps Java's filter in `RecipeData`) | X17 (`SM_LEARN_RECIPE` 155001381 and 155006386), X21a (six morph recipes; `SM_RECIPE_LIST`; `player_recipes`) | X19, X20 | as §10.4 (plus X21a, which §10.3 names) |
| `c19-del-recipe`: `RecipeList::deleteRecipe` without `PlayerRecipesDAO::delRecipe` | X21 (155001381 still stored; the stored set) | X19; `SM_RECIPE_DELETE` is still sent | §10.3 X21 |
| `c19-interval`: cooking gets the morph's interval 200 | X20 (10 updates and the end in 3,001 ms against 26,000; below 11 s) | X19 | §10.3 X20 |
| `c19-taxes`: `getTaxes` truncates (113 -> 112) | X1, X5, X25, X27, X16 as in stage 2, X17 (the Salt costs 138, 2 kinah left), X16 for A after C19 and at the last quit (`182400001: 2/0`) | X19-X21a; X7, X8, X13, X15, X26 | as §10.4 |
| `c19-kinah-gt`: `>=` -> `>` in `calculateBuyListPrice` | X27 (fatal at C18: C19 does not run) | – | the plan's X17 half is hidden behind part 1's fatal row, hence the two targeted mutants below |
| `c19-kinah-gt-sanctum`: the same, for a buyer in 110010000 only | X17 (the Salt refused), then X18/X19 (no Salt, no craft) and X22 (created 0) | part 1, X21a | the exact-kinah purchase kills it |
| `c19-trydecrease-gt-sanctum`: `Storage::tryDecreaseKinah` `>=` -> `>`, for an actor in 110010000 | X17 (no kinah update, no Salt), then X18/X19 and X22 | part 1, X21a, the learn (3,640 > 3,500) | §10.4 "or `Storage::tryDecreaseKinah`" |
| `c19-combo-product`: `finishCrafting` adds the combo product | X19 (160001051 added), X16 (`160001001: 0/2, 160001051: 2/0`, before the quit and at the last quit) | X20, X21, X21a | the INIT update carries the task's template, still the base product |
| `c19-skill-threshold`: `addSkillXp`'s threshold doubled | X19 (no level-up `SM_SKILL_LIST`), X21 (`skill_level` 1) | X20, X21a | §10.3 X19 "the level-up threshold" |
| `c19-no-exp`: `finishCrafting` without `addExp` | X19 (no `SM_STATUPDATE_EXP`; `players.exp`) | X20, X21 | |
| test side, `AION_M5C_TEST_MUTANT=c19-expectations` in the gate source (removed after its build, the file restored by sha256): every C19 expectation no production mutant above kills, shifted by one or negated - the oracle premises, the spawn, the Sanctum ledger, Hestia's window, the question, the learn's packets, the Salt's addition, the component counts, X18's cancel packets and the 12 m silence, X19's INIT/start/end updates, the progress bars, the animations, the deleted stacks, the product, the level-up and `SM_RECIPE_DELETE` | all 65 of those assertion lines, nothing else | S-0, C0-C18, C20a, C20 | |

The clean binaries then passed `gs.scenario.m5c` three times in a row (279.5 s, 270.6 s, 265.0 s; the first run of the lane, with the same
gate less the X20 time bounds and the long wait, 282.7 s): 23 of 23 cases each time, `CraftingTask 0 1`, the unported trace empty.

### The review of the gate-2 lane (2026-09-28) and its fix

The review (approve-with-changes) ran 9 mutants of its own (`AION_C3R_MUTANT`, one build into `build/c3-gate`'s server, the sources restored
and the server rebuilt clean). Three of them are §10.3 C19 mutants the lane had not run, and the gate killed them: `c3r-5m` (the 5 m check
dropped: X18 - no 1330040, three `SM_CRAFT_UPDATE`, two `SM_DELETE_ITEM`, the counts 0 - then X19), `c3r-cost` (the price 3,500 -> 3,400: X17
- the parameter "3400", kinah {240}, the Salt leaving 100 - and X16 in C19 and C20a) and `c3r-swap-skip` (X21a: 30002 missing, 30001 extra).
Six passed: `c3r-consume-twice`, `c3r-delay`, `c3r-speed`, `c3r-cancel-bars`, `c3r-swap-level` and `c3r-learn-anim`. The fix tightens the
gate so that each of these is killed. The table below lists the changes to `M5cScenarioTest.cpp` and `m5c/sanctum.py`.

| Area | As built | Reason |
|---|---|---|
| X20's timing | Checked gap by gap: from the start pair to the first progress update is `firstTickDelay`, and every later gap, including the one to the end, is `interval`. Each gap must be within 250 ms (`TICK_TOLERANCE_MS`). The total `firstTickDelay + n x interval` must also be within 250 ms, and the oracle's 11-36 s bounds get the same 250 ms. In the clean runs and the non-timing mutant runs, no gap was off by more than 2 ms | Review medium 1: 750 ms on the total let a first tick 600 ms late pass |
| The surplus Inina | C19 seeds the recipe's Inina plus `SURPLUS_ININA` (1). X18's counts are 2 Inina and 2 Salt. X19 wants the Salt's stack deleted and the Inina's stack kept: no `SM_DELETE_ITEM` for it, and its last `SM_INVENTORY_UPDATE_ITEM` at count 1. X19's counts are then 1 and 0, and the X16 ledger carries the extra Inina to C20a's rows | Review medium 2: with the exact seed a second consumption finds nothing to take, so §10.3 X19's "materials consumed twice" was an equivalent mutant. This departs from §10.1's exact seed for Inina only. The kinah stays exact, so X17's `>=` rows are unchanged |
| `SM_CRAFT_UPDATE`'s speed, delay and bars | m5c-economy's `craft.recipe` now carries `executionSpeed` and `showBarDelay` (900 and 1200), and `updates`: m5c-craft's rows for the start pair, the end and `sendCancelCraft`. X20 checks every progress update's speed and delay. X19 checks the action, bars, speed and delay of the INIT, start and SUCCESS updates (speed 0, delay 0). X18 checks the same for the cancel update (bars 0 and 0) | Review low 4 |
| X21a's levels and `SM_SKILL_REMOVE` | The oracle's `daeva.enterWorld.skillLevels` and `daevaSwap.smSkillRemove`, read file-locally. Every skill in the burst's message-0 `SM_SKILL_LIST` must be at the oracle's level as the packet shows it: 1 for a normal skill (SkillEntryWriter.java:27), the real level for 30002, 30003 and 40009. There must be exactly one `SM_SKILL_REMOVE(30001, 1, 0)`, where 1 is a tapping skill's `getProfessionFlag()`. After the quit, `player_skills` must equal the oracle's 41 levels plus Cooking at 2. That row catches the normal skills' stored levels, which the packet hides (169, 2865, 2878 and 2891 are at 2) | Review low 5 |
| The CRAFT_LEVEL_UP animation | The yes window's `SM_ACTION_ANIMATION`s must equal the oracle's `learn.yes.animations`: one, `(A, 4, 0)`. X19's window must equal `recipe.skillUpAnimations`: none, because level 2 is not an animation level | Review low 6 |
| The old level | A's level is taken from the last `SM_STATS_INFO` of its connection before C19's quit, and `players.old_level` must equal it (-1 if the connection had none) | Review low 7: the oracle models the stored skills from this value, so a store that was lower, or missing, passed X21a unseen |
| The oracle | `m5c/sanctum.py`: `JavaC19Rules.craft_level_up_animation`, read from ActionAnimation.java, and the new fields above. `tests/test_m5c_sanctum.py` has 27 tests, one of them new (`test_the_craft_updates`), plus new rows in `test_today`, `test_literals_are_read`, the fixture's question test and the real-data tests. The README section is updated | Every number comes from the oracle. `EconomyOracle.h` is untouched, because its parser and fixture are harness-b's; `parseC19Extras` reads the new fields file-locally, as `parseLearnYes` does |

**Mutation proof.** The 12 production schemata are switched by `AION_C3F_MUTANT` (`c3f-*`). They were built in one batch into
`build/c3-gate`'s server only. Right after the build the five sources were restored with Edit and matched their sha256. The server was then
rebuilt clean, with no `AION_C3F` string in the exe. Each mutant had one gate run, with the final test binary less the two guard edits
described under the test-side run below.

| Mutant | Failed | Stayed green |
|---|---|---|
| `c3f-delay`: `AbstractInteractionTask::start` schedules the first tick 600 ms late (the review's `c3r-delay`) | X20: the first gap (1,601 ms) and the total (29,100 ms against 28,500) | Every later gap; C0-C18, C20a, C20 |
| `c3f-interval`: the craft interval +300 ms | X20: all 11 later gaps (2,799-2,801 ms) and the total (31,801 against 28,500) | The first gap; the rest |
| `c3f-consume-twice`: `checkCraft`'s consume loop runs twice (the review's `c3r-consume-twice`) | X19: the Inina stack deleted, the counts (0 and 0 against 1 and 0); X16 before C19's quit and at C20a's last quit | X17, X18, X20, X21, X21a; X19's update row for the Inina stack (the first decrease sends count 1 before the second deletes the stack; the test side proves that row) |
| `c3f-consume-first`: `checkCraft` consumes before its checks (the lane's `c19-consume-first`, rerun with the surplus) | X18: the item packets at 7 m, and the counts at 7 m and 12 m; X19: the 3 m craft is refused for want of Salt, a fatal row that ends C19; X22: `CraftingTask 0 0` | X17, X21a, part 1. With the surplus, X19 still cannot stay green, because the refused 7 m craft takes the only Salt |
| `c3f-speed`: `analyzeInteraction` sets 300 and 500 (the review's `c3r-speed`) | X20: all 9 progress updates' speed and delay | X19, X21 |
| `c3f-cancel-bars`: `sendCancelCraft` with bars 1000 and 1000 (the review's `c3r-cancel-bars`) | X18: the cancel update's success and failure bars | The rest |
| `c3f-swap-level`: 30002 at 30001's level + 1 (the review's `c3r-swap-level`) | X21a: 30002 shown at 2, and `player_skills` | X17-X21 |
| `c3f-no-skill-remove`: the swap without `SM_SKILL_REMOVE` | X21a: no `SM_SKILL_REMOVE` | The rest |
| `c3f-autolearn-level1`: `autoLearnSkills` adds every skill at level 1 | X21a: `player_skills` (169, 2865, 2878 and 2891 at 1). The packet shows every normal skill as 1 anyway | The level shown for 30002, 30003 and 40009 (all at lvl 1) |
| `c3f-learn-anim`: `onLearnSkill` without CRAFT_LEVEL_UP (the review's `c3r-learn-anim`) | X17: no `SM_ACTION_ANIMATION` | The rest |
| `c3f-anim-every-level`: CRAFT_LEVEL_UP at every level of a crafting skill | X19: `SM_ACTION_ANIMATION(A, 4, 0)` at level 2 | X17 |
| `c3f-old-level`: `storeOldCharacterLevel` stores level - 1 | X21a: `old_level` 1 against the 2 A had. X21a's skills and recipes stay green, as the review predicted: the oracle, asked with 1, names the same list | The rest |

**Test side.** `AION_C3F_TEST_MUTANT=c3f-expectations` is an env-guarded schema in the gate source. It was built into the test binary, then
removed with Edit, and the source matched its sha256 again. It shifts every C19 expectation this fix added or changed:

- the old level, the `skillLevels` premise and the shown levels;
- the `SM_SKILL_REMOVE` premise and packet;
- both animation rows;
- X18's component premise;
- the five fields `expectCraftUpdate` compares (action, the two bars, speed and delay), which cover X18's cancel and X19's start pair and end;
- X20's speed and delay, the first gap, the later gaps, the total and the 11-36 s bounds;
- X19's three stack rows (the branch swapped);
- `player_skills`.

All 24 of those assertion lines failed, and nothing else did: C19 alone failed, while S-0, C0-C18, C20a and C20 passed. Before that build,
the two guards this fix had added as `ASSERT`s became non-fatal rows, so no mutant can end C19 early: A's level (`value_or(-1)`) and the
oracle's `smSkillRemove`. Both are among the 24.

**For the plan's owner** (m5c-plan.md is not this lane's to change). §10.3 X19 "materials consumed twice" is proven only with the surplus
Inina, which is a §10.1 seed change. §10.4's consume-first row still cannot keep X19 green (see `c3f-consume-first`). The review's two info
findings are not closed; they are recorded here instead:

- (a) §10.3 X18's "Proves both range checks" is coarser than X2's band spots. With spots at 3, 7 and 12 m, any `checkCraft` range in
  [3, 6.75), a centre-to-centre `checkCraft`, or any `CM_CRAFT` range in [7, 12) keeps X18 and X19 green. A band spot (5.1 m, in range only
  with the radii) was not added. This belongs in X18's "cannot prove".
- (b) At the level difference 0, the interval `2500 - 60 x diff`, its 1200 cap, `lvlBoni`, and the speed and delay difference terms are all
  constants. A mutant of any of them is equivalent here, so they belong in §10.3 X20's "cannot prove". P5-02a's unit tests own that
  arithmetic.

**Runs.** The fixed gate passed on the clean binaries once before the mutants (275.0 s; the gate then lacked the gap statistics in C19's
log line and the two guard edits), and three times in a row after them (288.6 s,
270.9 s and 271.8 s): 23 of 23 cases each time. C19 took 60-71 s, with 10-13 progress updates, and no gap was off by more than 2 ms.
Together with the lane's and the review's runs, the gate takes 265-289 s alone, so the slot table in `ScenarioTests.cmake` now says 289
(slot 2 sums 1,340 s). This answers the review's info finding: the review's 284.2 s was already above the 283 the lane had recorded.

## M5c stage 3 integration (2026-09-28)

m5c-plan.md §22. The items the lane and the fix left for the plan's owner are applied in m5c-plan.md §10.1-§10.5, §11 and §13 (§22.3 lists
them): the surplus Inina in §10.1's seeds, C19 as built, the "as built" and "cannot prove" halves of X17-X22 (X18's range edges, X20's
level-difference terms at Δ 0), and §10.4's consume-first, `>=` → `>` and race-filter rows. Two harness items of the lane:

| Area | As built | Reason |
|---|---|---|
| `OracleRunTest`'s work directories | The skills case (`OracleTest.cpp`) writes its answers to `selftest/oracle-skills`, the economy case (`EconomyOracleTest.cpp`) to `selftest/oracle-economy`. Both used `selftest/oracle` before, and each `Oracle` names its answers `oracle1.json`, `oracle2.json`, … from its own counter (`Oracle::run`), so the two processes `ctest -j` may start together wrote one file. No assertion changed | The lane saw `TheEconomyBindingAsksTheRealOracleWhatItWasGiven` fail with a JSON parse error under `-j 4`. Measured here on the old sources: `ctest -R "^OracleRunTest\." -j 2 --repeat until-fail:4` failed the economy case in its second round (`parse error at line 1999, column 1: … unexpected '{'`, two answers in one file). With the change, `--repeat until-fail:6` passed 12 of 12 |
| `getenv` in the gate sources | Unchanged. `lint_concurrency.py --werror --cycles=core game-server/tests/scenario` reports L8 (`getenv`) in every gate source (M5a, M5b, M5b-2, M5b-3, M5c, the stress run) and in `Oracle.cpp`, `ScenarioDatabase.cpp` and `ScenarioServers.cpp`, and L6/L11 rows elsewhere in the harness | Pre-existing at HEAD (M5c's one call, `AION_SCENARIO_REQUIRE`, has the other gates' shape). The registered lint, `gs.lint.concurrency`, covers `game-server/src` only, which is clean |

The lane's build tree `build/c3-gate` was already gone when this step began; its configure log `build/c3-gate-configure.log` is deleted.
The unit suite and every gate, run two at a time on the integrated tree, are in m5c-plan.md §22.4-§22.5.

## M5d stage 1 (gate-harness lane): the quest decoders, `talk()`, the lifted kill helpers

m5d-plan.md G-02's rest (§18.3). M5c had landed the three dialog builders, the `SM_DIALOG_WINDOW` decoder and `InventoryModel`; this is
what the M5d gate (G-03) still needed. No gate reads the new decoders or `talk()` yet. The M5b, M5b-2 and M5c gates now use the moved
code, with no assertion changed.

| Area | As built | Reason |
|---|---|---|
| `decoders/QuestDecoders.{h,cpp}` | `SM_QUEST_ACTION` in all six types (ADD 14 bytes, UPDATE 13, ABANDON 9, TIMER 10, SHARE 13, UNK 9) and the **empty body** of an `extra_category` quest, which decodes as `QuestAction::empty` (SM_QUEST_ACTION.java:69-71; §11 risk 9: 111 quests, 1209 the first). The literals are verified: the zero byte after the status, ADD's and UPDATE's `writeH(0)`, ADD's last `writeC(0)`, ABANDON's `writeD(0)`, UNK's `writeH(1)`, `writeH(0)`. So are the Java invariants: TIMER's byte must be `timer > 0 ? 1 : 0`, SHARE's int 0 or 1, and the status one of `QuestStatus.value()` 3-6. A type outside 1-6 is refused. The vars-and-flags int is kept raw (`questVarsAndFlags`), with `var(i)` and `highByte()` | The six 6-bit vars take 36 bits and `step \| flags << 24` overlays vars 4-5 with the flags (QuestVars.java:37-47), so the decoder cannot split them. A status check of 3-6 catches a port that writes the ordinal (0-2), which Y3 names as a mutation |
| `SM_NEARBY_QUESTS` | `writeC(0)` is verified. The count is read negated (`-size & 0xFFFF`), and the entries are kept in wire order with the marker bit split off (`ids()`, `notYetAvailableIds()`, `wireValues()`). The decoder refuses a quest id twice (the entries are a `Map`'s keys) and a wire int that is negative or at or above 2^18 | quest_data.xml's highest id is 99002 < 2^17, so any bit above the marker bit 17 means a corrupted body, not a quest. The oracle lists `nearby.xmlOnlyWire` **sorted**, so `wireValues()` equals it only once sorted, and Y1 compares the two as sets; the wire order is the oracle's `nearby.xmlOnlyWireOrder.buckets` (exact for a level-1 Elyos in Poeta: 1105, 132180, 132181, 132199, 132184, 1101) |
| `SM_STATUPDATE_EXP` | Moved out of `M5bScenarioTest.cpp` and `M5cScenarioTest.cpp`, which had one copy each, into `QuestDecoders.h`. The fields keep the Java packet's names (`curBoostExp`, `maxBoostExp`). M5b's copy had called the fourth `currentBoostExp`. No gate read either of the last two | G-02; m5e-plan.md A-04c expects the decoder from here |
| `GameSession` | `CM_DELETE_QUEST` (80) and `buildCM_DELETE_QUEST` (readD questId). `talk(npc, action, questId, quiet = 1 s, limit = 10 s)` sends `CM_DIALOG_SELECT(npc, action, 0, 0, questId)` and returns `TalkOutcome` (`firstPacket`, the burst up to the first gap of `quiet`, `closed`). Target 0 is the journal's form (C10b) | DialogService answers a quest action inside runImpl, so the answer arrives as one burst in the server's order. That is what the Y3/Y5/Y6/Y11/Y13 patterns read. `extendedRewardIndex` and `lastPage` stay 0 as the gate sends them; a reward beyond 15 would need `buildCM_DIALOG_SELECT` directly |
| `FightSupport.{h,cpp}` | `FightRecording`/`recordFight` moved out of `M5bScenarioTest.cpp`. `waitForRespawnAt` moved out of `M5b2ScenarioTest.cpp`, where it was a lambda. The logic is unchanged. The lambda's captures became parameters (`session`, `templateId`), and the two M5b-2 call sites pass `*a.game` and `GATE_MONSTER_NPC_ID`, the default they used before. The file-local `readUntil` the lambda called is copied into `FightSupport.cpp`'s anonymous namespace. The gates keep their own copies for everything else | G-02: the M5d gate's kerub and sprigg kills (C10, C12, C16) use the same helpers as M5b and M5b-2 |

## M5f travel core, early (2026-09-29): the travel gate `gs.scenario.travel` (m5f-plan.md §16)

A gate of its own (no plan names an existing gate for the slice; G-03's `gs.scenario.m5f` is stage 2's), in the same binary, `TEST(TravelScenario,
Run)` in `TravelScenarioTest.cpp`, output `<bin>/scenario/travel`, schema pair `aion_gs_test_travel_<hash>`, its own allow-list
`travel_partial_allowlist.txt` (copied from m5c's: §A the BaseService startup row, §B the four wall-clock cron rows, §C the rest),
`AION_SCENARIO_TRAVEL_PARTIAL_ALLOWLIST`, gate slot 1 (ScenarioTests.cmake's sums updated), labels `scenario;realdata`, TIMEOUT 1800, no Python.

| Case | What |
|---|---|
| S-0 | the servers start (the M5a profile plus `npcshouts.enable=false`, `rates.drop=0`) |
| T1 | A (Elyos) is created, disconnected and seeded as C19 seeds (m5c-plan.md): `player_class GLADIATOR`, `exp 126069` (level 10), `player_quests (1006, COMPLETE)`, 5000 kinah, 1.5 m from Polyidus (203726) in Sanctum; enters Sanctum; `CM_SHOW_DIALOG` -> `SM_DIALOG_WINDOW`; `CM_DIALOG_SELECT(44)` -> `SM_TELEPORT_MAP(npc, 1)`; `CM_TELEPORT_SELECT(npc, 4)` -> `SM_TELEPORT_LOC(3, Verteron, Verteron, 1640.76, 1500.32, 119.70999, 0)`, the kinah down by `getPriceForService(500) = 706`, no `SM_PLAYER_SPAWN` yet; `CM_TELEPORT_ANIMATION_DONE` -> `SM_CHANNEL_INFO`, `SM_PLAYER_SPAWN(Verteron, loc 4)`; `CM_LEVEL_READY` announces Verteron's npcs around the arrival |
| T2 | A opens Mirdiena's (203120, 1.9 m from the arrival) map (`SM_TELEPORT_MAP(npc, 105)`) and selects loc 15: `SM_EMOTION(START_FLYTELEPORT, 7001)` from himself with `FLYING` set and `ACTIVE` unset, 565 kinah (400), no `SM_TELEPORT_LOC`; three `CM_MOVE_IN_AIR` (no `SM_MOVE` of his own); `CM_EMOTION(LAND_FLYTELEPORT)` -> `SM_EMOTION(LAND_FLYTELEPORT)` with `ACTIVE` and without `FLYING`; after the quit `players` holds Verteron at the last `CM_MOVE_IN_AIR` point and `inventory` 5000 - 706 - 565 |
| T3 | B, the Asmodian mirror: quest 2008, Doman (204191) in Pandaemonium, teleportId 50, loc 9 -> Altgard (heading 60), 706 kinah; `players.world_id` 220030000 after the quit |
| T4 | both quit, the servers stop: exit codes, no `AION_UNPORTED`, every `AION_PARTIAL` hit in the allow-list (§A hit, §B not), no ERROR line, empty census, lockdep and watchdog reports, `liveLeaks 0`, no unported client packet |

The numbers are the data rows through the Java arithmetic, not an oracle's (m5f-plan.md G-01, the `m5f-travel` oracle, is stage 1's
gate-harness item). The three client packet bodies and the two decoders (`SM_TELEPORT_MAP`, `SM_TELEPORT_LOC`, from their Java writeImpl) are
file-local; G-02 moves them into `GameSession` and `decoders/TravelDecoders`. Run in this tree (Debug, 2026-09-29): passed in about 45 s.
The first run failed T2's "no SM_MOVE" row on the SM_MOVE of the npcs walking around the arrival; the row now reads only his own object id.
**Mutation** (the `AION_TRV_MUT` schemata of P5-08.md, one build of `aion_game_server`, which inherits the variable from the gate): five
mutants, all killed - M33 (the statue animation inverted: T1 and T3 read animation 4), M06 (the raw price: T1, T2 and T3's kinah and the
stored `inventory` row), M13 (`ACTIVE` not unset: T2's take-off state), M17 (heading 0: T3's `SM_TELEPORT_LOC` and `SM_PLAYER_SPAWN`), M12
(`FLYING` not set: T2's take-off state, and `CM_MOVE_IN_AIR` then moves nothing, so the stored position is the arrival's). Sources restored
byte for byte (sha256), rebuilt, no `AION_TRV_MUT` in a source or a binary; the gate passes again on the rebuilt server.

## The travel and ascension gates integrated (2026-09-29): the gate slots

Branch `integ/asc-travel` (docs/design/p6q-ascension-route.md §7) merges `m5f/travel-core` and `p6q/ascension-route`, which had each put
their gate into slot 1 (`gs.scenario.travel` 45 s, `gs.scenario.ascension` 880 s: 1,987 s against slot 2's 1,340 s together). The merge
rebalances `ScenarioTests.cmake` by its slot table:

| Change | Why |
|---|---|
| `gs.scenario.ascension`: slot 1 -> slot 2 | the long gate goes to the other slot |
| `gs.scenario.m5b`, `gs.scenario.m5b_geo`: slot 2 -> slot 1, together | one schema prefix, one slot (the sweep rule); 578 s |
| `gs.scenario.travel`: stays in slot 1 | |

Sums: slot 1 1,685 s (smoke and M4 367, m5a pair 235, m5b3 pair 460, m5b pair 578, travel 45), slot 2 1,642 s (m5b2 pair 473, m5c 289,
ascension 880). No placement that moves a single pair does better; the best one (21 s apart) moves three. Moving m5b away from m5b2 is safe
for the schema sweep: `dropAbandonedSchemas`'s `LIKE 'aion_gs_test_m5b_%'` also lists m5b2's and m5b3's schemas (`_` is a wildcard), but
`isScenarioSchemaName` decides on the exact prefix and the 8 hex digits (ScenarioDatabase.cpp:24-31), which is why m5b and m5b3 could already
sit in different slots. The next gate joins slot 2. The whole gate set then ran 52 of 52 green in 1,643 s (p6q-ascension-route.md §7).

**What every gate server of the main tree also reads.** The game server loads the Java tree's untracked `config/mygs.properties` after
`config/main/*` (the log line "Loading: ./config/mygs.properties"), so a key a gate does not pass on the command line takes the owner's play
value. On 2026-09-29 that file sets `gameserver.simple.secondclass.enable = true`, so the four ascension handlers (1006, 2008, 1007, 2009)
register only in `gs.scenario.ascension`, which pins the key to `false`; in `gs.scenario.travel` (and m5c) the seeded Daevas get no
1007 / 2009 in the main tree, and do in a tree without the file (CI, a worktree). Both were measured on the travel gate, both pass
(p6q-ascension-route.md §7). Recorded here, not changed: pinning the key in the travel gate is the owner's call (U1/U7).

## Lane H (harness, 2026-09-29): hermetic gate servers and the census drain

Branch `fix/gate-hermetic`, from C++ `a72676184`: the two harness defects of docs/design/p6q-ascension-route.md §7. The game server's test
hook and the drain are rows of P4-01.md and P5-14.md, section "Lane H"; the header requests gh-1 and gh-2 are in header-requests.md.

**1. The operator's play profile never reaches a test server.** Every server a test started in the Java module directory read its untracked
override file (`game-server/config/mygs.properties`: "Loading: ./config/mygs.properties" in every gate log), so a key a gate does not pin took
the owner's value in the main tree and the shipped default in CI and in a worktree (the section "What every gate server of the main tree also
reads" above, and the m5c row's "a key the owner adds later would reach the server and not the oracles (left for integration)"). Now:

| Area | As built | Reason |
|---|---|---|
| `ScenarioServers::gameServerArguments` | Always adds `--ignore-mygs-properties` (`ScenarioServers::IGNORE_MYGS_PROPERTIES`), main.cpp's C++-only test hook: the game server's log says "Ignoring ./config/mygs.properties (C++ test hook --ignore-mygs-properties)" in place of "Loading: ...". Every gate and the stress run build their arguments here | A test-only command line switch keeps the production server exactly Java-faithful (it never passes it) and needs no second working directory: the game server reads `./data` below the module directory too |
| `ScenarioServers::prepareLoginServerDirectory` | The login server's working directory (`ls_run`) is built entry by entry from the module's `config` and leaves `config/myls.properties` (`ScenarioServers::loginServerOverrideFile()`) out, in any letter case of its name (review fix: `std::filesystem::path` compares case-sensitively, Windows opens the file whatever its case); the module directory is only read | The login server already ran in a copy of its config (stage 3), which copied the operator's `myls.properties` along. With the file left out it logs "No override properties found" |
| `startGameServer()` / `startLoginServer()` check the servers' logs (review fix) | Once the server is up, `gameServerProfileProblem` requires "Ignoring ./config/mygs.properties (C++ test hook --ignore-mygs-properties)" and no "Loading: ./config/mygs.properties" in the game server's log, `loginServerProfileProblem` "No override properties found" in the login server's; otherwise the start throws and the gate fails. The stub game server (`StubGameServer.cmake`) logs the line `aion_game_server` would, and with `-Dgameserver.stub.profile=read` reads the file although it got the switch | The review of lane H: nothing in a gate checked its own hermeticity - a harness that dropped the switch for the geo gates (mutant c1) passed `gs.scenario.m5a_geo` with the owner's profile read. Now that gate fails at its start under the same mutant (r19, below) |
| The chat server | Unchanged: `ChatServerProcessTest` (chat-server/tests/e2e) writes its own `config/mycs.properties` over its copy of the module's config, so an operator's file never reached the server; a comment there says so now | Hermetic by construction |
| The smoke tests and the M4 check | `--ignore-mygs-properties` on every server run, and the M4 check's `m4-compare --no-profile` (P5-14.md, "Lane H") | The same profile, read by the other test servers of the tree |
| `HermeticServersTest` (in `ScenarioServersTest.cpp`) | Two cases on the REAL servers, in module directories of their own below the test's output directory: `config` holds a copy of the module's `administration`/`main`/`network` folders (never the owner's override file) and an override file the test writes itself with a value the server cannot load (`gameserver.timezone = Mygs/NoSuchZone`; `loginserver.network.client.logintrybeforeban = mylsNotANumber`). The gate's server (`startGameServer()` / `startLoginServer()` with the offline environment) gets past its configuration to its database step (the game server logs the "Ignoring ..." line and "startup step 2: DatabaseFactory.init()", the login server "No override properties found" and "Failed to initialize pool"; `ls_run` has no `myls.properties`, the module's file is still there); the control - the same arguments without the switch in the same directory, and the login server started in the module directory itself - stops at the value ("Unknown time-zone ID: Mygs/NoSuchZone", "Error parsing \"mylsNotANumber\" as int"), which shows the file was there to be read. No database: 3-5 s per case. Registered like LoginServerHarnessTest: `scenario;realdata`, gate slot 1, TIMEOUT 300 (`ScenarioTests.cmake`) | The task's proof that a gate server does not see a `my*.properties` value, without ever writing into the owner's real config directory. A server run holds a slot (the rule of "the two gate slots") |
| `ScenarioServersTest.TheGameServerGetsTheM5aProfileAndTheScenarioArguments` | Also asserts `--ignore-mygs-properties` exactly once, in main.cpp's spelling | – |
| The gates in the main tree | They now see what CI and a worktree saw: of the owner's profile of 2026-09-29 two keys differ from that, `gameserver.simple.secondclass.enable` (`true` there, the shipped `false` now: the four ascension handlers 1006, 2008, 1007 and 2009 register in every gate, p6q-ascension-route.md §7's second row for the travel gate) and `gameserver.network.client.connect_address` (`127.0.0.1:7777` there, the play server's port, which every gate's login server handed its fake client; now the shipped `${gameserver.network.client.socket_address}`, the gate's own client address). Its other keys equal the shipped defaults or are pinned by `m5aProfile()` or the gate. No gate expectation changed: the whole gate set had already run without the file in the verify worktree, 51 run, 50 passed at once and m5a_geo on its rerun (the census flake that part 2 fixes; p6q-ascension-route.md §3) | – |
| Comments | `M5bScenarioTest.cpp` and `M5b2ScenarioTest.cpp` explained their stated `gameserver.soulsickness.disable = 10` with the profile the server read; now in the past tense, the keys stay | – |

**2. The census drain waits for the tasks already running.** `CheckOutput::drainPools` (P5-14.md, "Lane H") now waits until every pool
thread that ran a task when the drain started has left it, and (since the review fixes below) until every task queued in the instant or the
long-running pool then is done, in place of its barrier tasks; tasks queued or started later are not waited for. The window of
`Player 103881 506` - a 9 s `MapRegion::activate` on one instant pool thread while another one ran the barrier - is the unit case
`CheckOutputTest.TheDrainWaitsForATaskThatIsAlreadyRunningOnAnotherPoolThread`, which failed before the fix (mutant m10 below).

**Measured** (build dir `cpp/build/msvc`, Debug, the test database environment; the owner's `mygs.properties` in place and untouched):

- `gs.scenario.m5a_geo` beside `gs.scenario.m5b2_geo` (`ctest -j 2 -R "^gs\.scenario\.(m5a_geo|m5b2_geo)$"`), twice: **both passed both
  times** (m5a_geo 153 s and 156 s, m5b2_geo 298 s and 282 s), every `census.txt` empty (the header line alone). No drain logged a wait (no
  "Pool drain" line: none of the four censuses met a task running 100 ms or longer).
- `gs.smoke.startup` 30 s, `gs.smoke.startup_progress` 29 s, `gs.m4.check_static_data` 141 s: passed. `gs.scenario.m5a` 61 s,
  `gs.scenario.travel` 40 s, `gs.scenario.m5c` 282 s and `LoginServerHarnessTest` (`-j 2`): passed, censuses empty. Every gate server's log
  has the "Ignoring ./config/mygs.properties ..." line and no "Loading: ./config/mygs.properties", every login server's log "No override
  properties found"; the M4 check's servers now discover the machine's IPv4 for the shipped wildcard connect address ("No IP for Aion client
  advertisement configured, using ...", as in CI), where the owner's profile had set `127.0.0.1:7777`.
- The harness cases (`ctest -L scenario -E "^gs\."`: 39 run, 12 disabled gate shadows), `ChildProcessTest.*` and `ScenarioServersTest.*`,
  `ChatServerProcessTest.*`, `ConfigLoadTest.*`, `CheckOutputTest.*`, `aion_gs_configs_tests` (55) and `aion_gs_app_tests` (40) each in one
  process, `tools.porting` (71) and `test_geo_oracle` (19): passed.

**Mutation** (switch `AION_GH_MUT`: schemata in `configs/Config.cpp`, `main.cpp`, `ScenarioServers.cpp`, `CheckOutput.cpp`, the login server's
`configs/Config.cpp`, `RunStartupSmoke.cmake`, `RunM4Check.cmake` and `tools/oracle/geo/m4.py`, one build; every server and CMake script
inherits the variable from ctest). Every mutant was killed:

| Mutant | What it breaks | Killed by |
|---|---|---|
| m1 | `loadProperties` reads `mygs.properties` although the hook is on | `ConfigLoadTest.TheTestHookLeaves...`, `HermeticServersTest.TheGameServerOfAGate...` (the gate's server stops at the profile's zone) |
| m2 | `loadLoggingConfig` reads it although the hook is on | the same two |
| m3 | main.cpp parses `--ignore-mygs-properties` but does not apply it | `HermeticServersTest.TheGameServerOfAGate...`, `gs.smoke.startup` ("the server did not leave config/mygs.properties out") |
| m4 | `gameServerArguments` leaves the switch out | `ScenarioServersTest.TheGameServerGetsTheM5aProfileAndTheScenarioArguments`, `HermeticServersTest.TheGameServerOfAGate...` |
| m5 | the login server's config copy keeps `myls.properties` | `HermeticServersTest.TheLoginServerOfAGate...` (the file in `ls_run`, no "No override properties found", the value in the log) |
| m6 | `RunStartupSmoke.cmake` leaves the switch out | `gs.smoke.startup` |
| m7 | `RunM4Check.cmake` leaves the switch out | `gs.m4.check_static_data` ("id_factory: the server did not leave config/mygs.properties out", 3 s) |
| m8 | `RunM4Check.cmake` leaves `m4-compare --no-profile` out | `gs.m4.check_static_data` ("items 5 and 6: m4-compare did not say that it left mygs.properties out", 142 s) |
| m9 | `m4.py` reads the profile although `--no-profile` | `test_geo_oracle.M4CompareTest.test_no_profile_leaves_mygs_properties_out` |
| m10 | `drainPools` does not wait for running tasks (the code before the fix) | both drain cases of `CheckOutputTest` |
| m11 | `drainPools` waits until no pool thread runs a task (idle pools) | `CheckOutputTest.TheDrainDoesNotWaitForATaskThatStartedAfterIt` (it waited to its 6 s deadline) |
| m12 | the "LongRunning-" threads are not tracked | `CheckOutputTest.TheDrainWaitsForATaskThatIsAlreadyRunningOnAnotherPoolThread` (the LongRunning case) |
| m13 | the "ScheduledPool-" threads are not tracked | the same case (the ScheduledPool case) |
| m14 | a thread counts as still running its task while it runs any task | `TheDrainDoesNotWaitForATaskThatStartedAfterIt` (the one-thread pool) |
| m16 | `loadProperties` never reads `mygs.properties` | `ConfigLoadTest.TheTestHookLeaves...` (its hook-off half) and six older `ConfigLoadTest` cases |
| m17 | the login server never reads `myls.properties` | `HermeticServersTest.TheLoginServerOfAGate...` (its control) |
| m18 | the game server never reads `mygs.properties` (both loaders) | `HermeticServersTest.TheGameServerOfAGate...` (its control) |

(m15 was not used.) Sources restored byte for byte from saved copies (sha256 checked for all eight files), the stale `m4.cpython-312.pyc` of
m9 removed, everything rebuilt; no `AION_GH_MUT` string is in a source, in `tools/oracle` or in the five rebuilt executables, and the tests
pass again on the rebuilt binaries.

### Review fixes (2026-09-29)

The review of lane H requested changes: part 1 sound, part 2 incomplete. Per finding:

1. **medium - the drain returned while a task queued before it still ran** (another pool thread took the task just before the barrier; the
   reviewer's probe failed 5 of 5). Fixed: the drain waits for the futures of the tasks queued in the instant and long-running pools when it
   starts (`ExecutorBackend::pendingTasks`), then for the running tasks as before; the barriers are gone. The code comment, `CheckOutput.h`
   (gh-2) and P5-14.md say what it guarantees now, and what it cannot see (a task popped before the queue snapshot and not yet published when
   the thread snapshot reads its thread: the few instructions before `runFromExecutor`'s `TaskScope`). `TheDrainWaitsForATaskThatWasQueuedWhenItStarted`
   is the reviewer's probe; it fails on lane H's drain (r0: "drainPools returned after 254 ms", 255 and 257 ms in three runs).
2. **low - h1, the start-not-scope-id identity untested.** Fixed: `TheDrainWaitsForALongTaskThroughItsQuiescentPoints` (r7).
3. **low - h3, the deadline of the running-task wait untested.** Fixed: `TheDrainGivesUpAtItsDeadline`, a running and a queued task (r8).
4. **low - m4-compare: what compare() reads untested; the M4 check checks a note line only.** Fixed: the note is derived from the list of
   files `compare()` read (`property_files`), and `compare()` and `main()` are tested with a profile that changes a predicted instance count
   (r23-r26; P5-14.md, the M4 row).
5. **low - the gates did not check their own hermeticity.** Fixed: `startGameServer()` / `startLoginServer()` check the servers' logs (table
   above); `gs.scenario.m5a_geo` under the reviewer's c1 (r19) now fails at its start; `TheGeoGateTurnsTheGeoDataOn...` also counts the switch.
6. **low - `TheDrainDoesNotWaitForATaskThatStartedAfterIt` could fail on a loaded machine.** Fixed as far as a test without a hook in
   `drainPools` can: the test's backend (`ObservedPoolBackend`, a forwarder of `ThreadPoolBackend`) tells when the drain has copied the queues,
   and the running task queues the second one only then and 200 ms later. Before, the second task was queued 300 ms after the first started,
   so a test thread that stalled that long anywhere before the drain began (a sleep in `waitFor`, a busy machine) saw it queued; now it would
   have to stall 200 ms inside the few instructions between the two snapshots. A timing test: no mutant can show it.
7. **low - the login config copy compared the file name case-sensitively.** Fixed (table above); `HermeticServersTest.TheLoginServerOfAGate...`
   now names the module's file `MyLS.properties`, which the control run of the login server reads; r18 (the old comparison) fails it.
8. **info - surviving reviewer mutants judged equivalent or log-only.** h2 (the calling thread not skipped) and a3 (the hook also drops
   `logging.properties`) and b3 (the switch also reported as unknown) are killed now by `TheDrainDoesNotWaitForTheTaskThatCallsIt` (r9),
   `ConfigLoadTest.TheTestHookKeepsTheShippedLoggingFiles` (r20, r21) and `HermeticServersTest.TheGameServerOfAGate...` (r22). d1 (the copy without
   `recursive`) and d3 (`ls_run` without `logback.xml`) stay as they are: the login server's config is one directory level deep, which
   `std::filesystem::copy` copies without `recursive` too, and the C++ login server does not read `logback.xml` - equivalent.
9. **info - the reviewer's mutant runs overwrote four output directories.** Reran on the restored build (measured below); the shared
   `game-server/log/stats/MethodStats.log` that every gate run rewrites (it ignores `--log-folder`) is out of scope and unchanged (routed
   through `--log-folder` on 2026-09-30, the section "Small tasks 2026-09-30" below).

**Mutation of the review fixes** (switch `AION_GH_MUT`, ids r0-r26; schemata in `CheckOutput.cpp`, `configs/Config.cpp`, `main.cpp`,
`ScenarioServers.cpp` and `tools/oracle/geo/m4.py`, one build of `aion_game_server`, `aion_gs_app_tests`, `aion_gs_configs_tests` and
`aion_gs_scenario_tests`; the whole `CheckOutputTest`, `ConfigLoadTest`, `ScenarioServersTest` or `test_geo_oracle` suite run per mutant):

| Mutant | What it breaks | Killed by |
|---|---|---|
| r0 | lane H's drain: one barrier per pool, no wait for the queued tasks | `CheckOutputTest.TheDrainWaitsForATaskThatWasQueuedWhenItStarted` ("returned after 254 ms") |
| r1 | the queued tasks are not waited for (no barriers either) | the same, and `TheDrainWaitsForTheSingleExecutorsTaskAndItsQueue` |
| r2 | the long-running pool's queued tasks are left out | `TheDrainWaitsForTheSingleExecutorsTaskAndItsQueue` |
| r3 | the instant pool's queued tasks are left out | `TheDrainWaitsForATaskThatWasQueuedWhenItStarted` |
| r4 | the scheduled pool's pending tasks are waited for too | the same (it waited to its 10 s deadline for the not-due and the periodic task) |
| r5 | `installedBackend()` in place of `getInstance()`: the drain no longer creates the pools | `FinalCensusEndsWithTheZeroThresholdBreakerPass` (no breaker body runs) |
| r6 | "SingleExecutor" is no pool thread | `TheDrainWaitsForTheSingleExecutorsTaskAndItsQueue` |
| r7 | a running task is identified by its scope id (the reviewer's h1) | `TheDrainWaitsForALongTaskThroughItsQuiescentPoints` |
| r8 | no deadline in the wait (h3) | `TheDrainGivesUpAtItsDeadline` |
| r9 | the calling thread is not skipped (h2) | `TheDrainDoesNotWaitForTheTaskThatCallsIt` (it waited 3 s, its deadline) |
| r11 | the drain waits for idle pools (m11 on the new code) | `TheDrainDoesNotWaitForATaskThatStartedAfterIt` |
| r12 | a thread counts as running its task while it runs any task (m14 on the new code) | the same |
| r13 | `startGameServer()` does not check the log | `ScenarioServersTest.AGameServerThatReadsTheOperatorsProfileFailsTheStart` |
| r14 | `gameServerProfileProblem` ignores "Loading: ./config/mygs.properties" | `TheProfileChecksReadWhatTheServersLogged`, `AGameServerThatReadsTheOperatorsProfileFailsTheStart` |
| r15 | `gameServerProfileProblem` does not require the "Ignoring" line | `TheProfileChecksReadWhatTheServersLogged` |
| r16 | `loginServerProfileProblem` never reports | the same |
| r17 | `startLoginServer()` does not check the log | **survives** (`ScenarioServersTest`, `HermeticServersTest`, `LoginServerHarnessTest`): the check can only fire when `ls_run` holds an override file, which the copy never puts there (m5, r18 kill the copy's defects); it guards a later change of the copy |
| r18 | the copy compares the file name case-sensitively (lane H's code) | `HermeticServersTest.TheLoginServerOfAGateDoesNotReadTheOperatorsProfile` |
| r19 | a geo gate's server does not get the switch (the reviewer's c1) | `ScenarioServersTest.TheGeoGateTurnsTheGeoDataOnThroughTheSamePropertyOverride`; and `gs.scenario.m5a_geo` itself now fails at its start |
| r20 | with the hook on, the logging settings leave `logging.properties` out (a3) | `ConfigLoadTest.TheTestHookKeepsTheShippedLoggingFiles` |
| r21 | the same for `gameserver.properties` | the same |
| r22 | main.cpp also reports the switch as an unknown argument (b3) | `HermeticServersTest.TheGameServerOfAGateDoesNotReadTheOperatorsProfile` |
| r23 | `compare()` reads the profile whatever its flag (the reviewer's g1) | `test_geo_oracle.M4CompareTest.test_compare_predicts_from_the_properties_it_read`, `test_main_passes_no_profile_to_compare` |
| r24 | `main()` ignores `--no-profile` (g2) | `test_main_passes_no_profile_to_compare` |
| r25 | `main()` never reads the profile (g3: the flag's default inverted) | the same |
| r26 | `profile_note` follows the flag, not the files read | `test_no_profile_leaves_mygs_properties_out` |

(r10 was not used.) Under r19, the reviewer's c1, the real `gs.scenario.m5a_geo` now fails at its start (180 s: "geo 0 ... the game server
read the operator's config/mygs.properties (its log says 'Loading: ./config/mygs.properties')"), where lane H's harness passed it. The five
files were restored byte for byte from saved copies (sha256 checked), the `m4.cpython-312.pyc` of the mutant run removed, the whole tree
rebuilt; no `AION_GH_MUT` or `ghMut` string is in a source, in `tools/oracle`, in `aion_game_server`, `aion_gs_scenario_tests`,
`aion_gs_app_tests`, `aion_gs_configs_tests` or in `aion_gs_app.lib` / `aion_gs_configs.lib`.

**Measured on the restored build** (build dir `cpp/build/msvc`, Debug, the test database environment; the owner's `mygs.properties` in
place and untouched): `aion_gs_app_tests` (45), `aion_gs_configs_tests` (56), `ScenarioServersTest.*` and `ChildProcessTest.*` (23),
`test_geo_oracle` (21) each in one process; `ctest -L scenario -E "^gs\."` 41 run, 12 disabled gate shadows, all passed (HermeticServersTest
and LoginServerHarnessTest with the new log checks among them); `tools.porting`, `tools.oracle` (the whole suite, 279 s) and
`gs.chunks.consistency` passed. Gates and server tests, one `ctest -j 2`, all passed: `gs.smoke.startup_geo` 155 s, `gs.scenario.travel`
44 s, `gs.scenario.m5a` 56 s, `gs.smoke.startup` 28 s, `gs.smoke.startup_progress` 29 s, `gs.m4.check_static_data` 147 s,
`gs.scenario.m5a_geo` 170 s. The three gates' `census.txt` are empty, each `game_server.log` has the "Ignoring ..." line and no "Loading:
./config/mygs.properties", each `login_server.log` "No override properties found", and no drain logged a wait. These runs also rewrite the
four output directories the reviewer's mutant runs had left (`scenario\Debug\m5a_geo`, `m4\Debug`, `gs.smoke.startup\Debug`,
`gs.smoke.startup_progress\Debug`). Not run: `gs.scenario.ascension`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo`, `m5b3`, `m5b3_geo`, `m5c` and the
whole unit suite; every gate starts its servers through the same `startGameServer()` / `startLoginServer()` checks the runs above passed.

## M5d stage 2 (lane G): the M5d gate `gs.scenario.m5d` and `gs.scenario.m5d_geo` (m5d-plan.md G-03, G-04, §10)

`TEST(M5dScenario, Run)` and `TEST(M5dScenarioGeo, Run)` in `M5dScenarioTest.cpp`, one shared body; output `<bin>/scenario/m5d` and
`<bin>/scenario/m5d_geo`, schema pairs `aion_{gs,ls}_test_m5d_<hash>` and `..._m5dgeo_<hash>`, the allow-list `m5d_partial_allowlist.txt`
(`AION_SCENARIO_M5D_PARTIAL_ALLOWLIST`), labels `scenario;realdata` (`;geo`), TIMEOUT 1800 / 2700, both in **gate slot 2** (the smaller sum,
1,642 s against slot 1's 1,685 s, ScenarioTests.cmake's table; the discovered cases are DISABLED as for every gate). Test infrastructure only,
no Java counterpart; the rows say where the gate reads the plan's §10 differently and why.

| Area | As built | Reason |
|---|---|---|
| The cases | S-0, C0 (the oracles), C1-C3 + C4 (Y1), C5 (Y2), C6 (Y3), C7 (Y4), C8 (Y5), C9 (Y6, Y7), C10 (Y8), C10b (Y7b), C11 (Y9), C12 (Y10), C13 (Y11), C14 (Y12's abandon), C15 (Y12's relog and elpas's page 1011), then account B: C16a (Y13's markers, and no quest action at the first enter world, as C4), C16b (2101), C16c (2102's four kills and a fifth), C16d (2102's reward), C16e (Y13's relog), and C17 (Y14) after both disconnected and the servers stopped. The case log is M5b-3's: a non-fatal row failure lets every later case run, so a mutant shows its own rows red and the others green | §10.2-§10.3 |
| The expectations | Every number is an oracle's: `m5d-quests --map M --race R --level 1` (the nearby sets and their grey bits), `m5d-quest --quest ID [--completed ...] [--exp X]` (the page each step answers each action with, the kill run and its count, the first completion's payments, the follow-up window at the end npc and `levelsSinceEnterWorld`), `m5a-creation` (the spawns, and the starter kinah and bandages the Y12 and Y13 ledgers begin with, 1,000 and 20 today: `starterCount`), `m5a-spawns` (where elpas, mires, asak and vandar stand), `m5b-monster` (the kerubs' and sprigg workers' fixed plain spots, level 1's and level 2's exp need). The m5d oracles run with `--profile <output>/m5d_oracle_profile.properties`, which the gate writes from the server's own `-D` keys (`m5aProfile()` under the gate's keys), so the owner's `mygs.properties` reaches no expectation (M5c's `--set`, in the form m5d's CLI takes) | "every number comes from the oracle" (§10.1) |
| The profile | `m5aProfile()` (G-07's wall-clock keys included), M5b-2's keys (`npcshouts.enable=false`, `rates.xp.solo`, `soulsickness.disable`, `rates.drop=0`), and M5d's: `rates.xp.quest` and `rates.kinah.quest` written out at their defaults (`1.0, 2.0`), `analysis.quest_handlers=false`, **`simple.secondclass.enable=false`**; `character.creation.mode` stays 0 (D15). `game-server/config/m5d.properties.example` is **not** written: it is a file of the Java tree's config directory, which this lane may not touch; the profile lives in the gate and here | §10.1, §18.4, §18.7 |
| The Java handlers now registered | Phase 6's slice 1 registers 42 Java handlers (p6q-ascension-route.md §1), so D9's "no Java quest until phase 6" is gone. The expected nearby set is the oracle's `withJava` set restricted to the XML quests and the ids of `REGISTERED_JAVA_QUESTS` (the 42; the held-back 1000, 1100, 2000, 2100 are not in it): for a level-1 Elyos in Poeta that adds 1111 (grey), in Ishalgen nothing. The Elyos's level-up to 2 inside 1102's reward runs the generated `_1205ANewSkill.onLevelChangedEvent`: Y11's pattern holds `ADD 1205 s3`, `NEARBY`, `UPDATE 1205 s4 v1`, `NEARBY` between `LEVEL_UP 2` and onLevelChange's own `NEARBY`, and Y12's quest list after the relog is `{1205: REWARD, var 1}` (player_quests likewise). m5e-plan.md W-24 predicted exactly this `SM_QUEST_ACTION` in the M5d gate | "adapt the gate's cases to what now registers" |
| The order patterns | Each talk's answer is reduced to quest tokens in arrival order (`ADD/UPDATE <quest> s<status> v<vars>`, `ABANDON`, `NEARBY`, `DW <npc> <page> <quest>`, `EXP`, `GET_EXP <n>`, `MSG <id>`, `ITEM <template>x<count>`, `LEVEL_UP <level>`) and compared with the whole expected list, after the async set and the talked npc's `SM_LOOKATOBJECT` are taken out. The level-up's stats, skills and animations are not tokens, so Y11 pins the quest packets' order around them without the M5e skill list. `SM_INVENTORY_UPDATE_ITEM` names the object only; its template is the inventory model's | §10.3's "in Java's order" rows, stated once |
| The talk windows | Read with the gate's own burst collector (quiet measured from the last packet the async set does not explain), not `GameSession::talk`, whose `collectUntilQuiet` ends at the first quiet second of all traffic: beside Ishalgen's walkers every talk ran into its 10 s limit (the first run: 36 s for C16b). The packet is the same `CM_DIALOG_SELECT(npc, action, 0, 0, questId)` | measured, 2026-09-29 |
| The kills | `killAt` rotates over the three fixed plain spots of the kill targets nearest the end npc (kerubs 21-45 m from mires, sprigg workers 28-41 m from vandar): it takes the npc of the first spot whose latest `SM_NPC_INFO` names an object the gate has not killed, and waits for a respawn only when all three are dead. A corpse keeps its object id, a respawn is a new one, and the set of killed ids works across a relog, which `FightSupport`'s `waitForRespawnAt` (one session's recording index) cannot. The HP is logged before each kill and a Warrior below 60 % rests (reads packets) up to 60 s; no run needed it (the Elyos ended each fight at 281-284 of 284 HP, the Asmodian above 230 of 284) | C16c took 94 s waiting at one spot, 57 s rotating |
| C7's range | The character stands 15 m from mires on the line from elpas; the row wants exactly `STR_DIALOG_TOO_FAR_TO_TALK` (1300346) and no window | Y4 |
| Relogs | C11, C15 and C16e disconnect (`CM_QUIT(0)`), read `player_quests`, and log in again; only a first enter asserts the level-ready §5.8 sequence, because a relog near the monster spots reads a decaying corpse's `SM_DELETE` in that burst (the first run's C16e) | not the quest engine's |
| Allow-list | `m5d_partial_allowlist.txt`: §A `BaseService.cpp:18`; §B `QuestEngine.cpp:115` (the profile turns the analysis off) and G-07's four cron rows; §C `PvpMapService.cpp:32`, `PlayerService.cpp:268`. `QuestEngine.cpp:111` no longer exists (I-05) | §10.1, §19.5 |
| Y14 | Beside the Q8 bar: `live_counts.txt`'s `QuestEnv`, `QuestState`, `QuestVars`, `QuestStateList` and `Player` rows live 0 with created > 0 | §10.3 Y14 |

**Mutation proof (§10.4)**, schemata switched by `AION_M5DG_MUT` in nine production files (`QuestEngine.cpp`, `QuestService.cpp`,
`AbstractQuestHandler.cpp`, `DialogService.cpp`, `MonsterHunt.cpp`, `QuestState.cpp`, `DialogPageInfo.cpp`, `PositionUtil.cpp`,
`SM_QUEST_ACTION.cpp`), built once into `build/v`'s `aion_game_server` (the gate's server inherits the variable), the sources restored right
after the build and checked by sha256 (all nine OK), then the whole tree rebuilt clean: no `AION_M5DG_MUT` in a source or in the final
binaries. One `gs.scenario.m5d` run per mutant, 220-380 s each; the case log shows every row:

| Mutant (§10.4 row) | Failed | Stayed green | Note |
|---|---|---|---|
| `no-registration`: `QuestEngine::init` skips the XML registration (the old `:111`) | **Y1** (both sets are `{1111}`, the one Java handler of the map), **Y2** (page 1011), and every later row that reads a quest: Y3, Y5-Y14 | Y4 | 55 assertions |
| `xp-hunting`: `giveReward` pays exp with `Rates::XP_HUNTING` | **Y6** (`GET_EXP 80`, the exp shown 80), Y11 (80; 3 × 80 + 80 still reaches 400, so the level-up stays in the reward), Y13 (2101's 80) | Y3, Y5 and the rest | |
| `no-follow-up`: `sendQuestEndDialog` without the follow-up loop | **Y6**, **Y11**, **Y13** (`DW mires 10 0` / `DW vandar 10 0` instead of 1011 1102, 1011 1103, 1011 2103) | Y7 and the rest | |
| `finish-without-guard`: `finishQuest` without `status != REWARD` | **Y7b** (`UPDATE 1102 s5 v0`, `NEARBY`, then the sentinel: 1102 silently completed, no payment), then Y9-Y12, which read 1102 | Y6, Y7, Y13 | as §10.3 Y7b predicted, packet for packet |
| `no-next-page`: `handleQuestDialogueOrSendNextPage` without its window | **Y7** (the replay answered by nothing), **Y8** (no `DW mires 1009 1102`) | Y6 and the rest | |
| `count-past-end`: `MonsterHunt::onKillEvent` without `total <= endVar` | **Y13** (the fifth kill: `UPDATE 2102 s3 v5`; the report then `s4 v5`) | Y8, Y10 | |
| `follow-up-ignores-finished`: the follow-up takes the first startable, acceptable quest whatever its `<finished>` | **Y13** (`DW vandar 1011 2102` after 2101) | Y6, Y11 | |
| `report-without-kills`: `MonsterHunt::onDialogEvent` without the kill-total check | **Y8** (`UPDATE 1102 s4 v1`, page 5 after one kill), then Y7b (the journal now finishes the REWARD quest, which is right), Y9-Y11 | Y1-Y7, Y12-Y14 | Y10 cannot stay green: 1102 is finished before C12's kills. The first run of this mutant also lost C11's re-entry ("no packet after CM_ENTER_WORLD": the first packet came later than the burst's quiet second); `enterWorld` and `levelReady` now wait up to 30 s for the first packet (`collectAnswer`), and the rerun re-entered |
| `persistent-new-kept`: `setPersistentState(UPDATED)` keeps NEW | **Y12** (C15: 1102 still START in `player_quests`, no 1205 row), **Y14** (`Failed to insert new quests for player ...`: the second store INSERTs again) | Y9 (the first store is the INSERT) | There is no player cache: every enter world builds a new Player and loads its quest list from the database (`PlayerService.cpp:208`, PlayerService.java:102-135). The mutated `setPersistentState(UPDATED)` is also the call `PlayerQuestListDAO::load` makes on every row it loads (`PlayerQuestListDAO.cpp:70`), so after C11's relog the loaded states stay NEW, and C15's quit INSERTs 1101 and 1102 again (the duplicate key) instead of updating them. That is why §10.4's Y9 stays green: C11 reads the first store, which INSERTs either way |
| `abandon-keeps-row`: `abandonQuest` without `deleteQuest` | **Y12** (no 1103 in the nearby set after the abandon; `player_quests` and `SM_QUEST_LIST` keep 1103 START) | Y3 | |
| `race-ignored`: `checkStartConditions` without the `race_permitted` check | **nothing in the gate** | all | Not killable here, against §10.4: no quest a start-map npc starts is of the other race (`m5d-quests` for an Asmodian in Poeta and an Elyos in Ishalgen: both sets empty), and the level-change lists are per race, so the Elyos 1205 never runs for the Asmodian. The unit case `QuestItemActionsTest.ARestrictedQuestIsWarnedAboutBeforeTheUseMessage` (E-10, `:482-483`) kills it: the same schemata built into `aion_gs_itemsvc_tests`, run with the variable, fail that case alone of the suite's 13 (without the variable 13 of 13 pass) |
| `no-quest-interaction`: `getStartPageId` without `hasQuestInteraction` | **Y2** (`DW elpas 1011 0`), and the other first talks: Y5's mires, Y7b's sentinel, Y13's asak | Y12's final page 1011 | |
| `questenv-static`: `QuestEngine::onDialog` keeps its `QuestEnv` for ever | **Y14** (`QuestEnv` 28 live, the census names the Player, the leak ERROR lines) | Y1-Y13 | |
| `talk-range`: `isInTalkRange` always true | **Y4** (`DW mires 10 0` from 15 m) | every other row | |
| `status-ordinal`: `SM_QUEST_ACTION` writes the ordinal | **Y3** (the decoder refuses status 0), and every row with a quest action: Y5, Y6, Y8, Y10-Y13 | Y1, Y2, Y4, Y7, Y7b, Y9, Y14 | |

The four rows §10.4 sends elsewhere (`sendQuestEndDialog`'s own guard: H-07; the var shift: E-06; `XMLQuests` order: T-04; the skipped
reward-group check: E-06) were not run: the gate cannot see them by construction, as §10.4 says.

**G-02's quest decoders, the list and the proof §19.5 left open.** The stage-1 harness's cases: `QuestDecodersTest` (7:
`QuestActionAddIsFourteenBytes`, `QuestActionUpdateIsThirteenBytesAndCarriesTheVarsAndFlags`, `QuestActionAbandonTimerShareAndUnk`,
`QuestActionOfAnExtraCategoryQuestIsEmpty`, `NearbyQuestsOfALevelOneElyosInPoeta`, `NearbyQuestsEmptyAndMalformed`,
`StatUpdateExpIsFiveLongs`), `FightSupportTest` (3) and `GameSessionTalkTest` (4). This gate is the decoders' first reader. Six schemata in
`decoders/QuestDecoders.cpp` (switched by `AION_M5DG_MUT`, built once into `aion_gs_scenario_tests`, the file restored by sha256 right after
the build, the tree rebuilt, no schema string left), `QuestDecodersTest` run per mutant: the status range dropped (killed by the Add and Update
cases, `:54`: statuses 0, 1, 2 and 7 must throw); ADD's last byte not read (the Add case); TIMER's `timer > 0 ? 1 : 0` byte not checked
(`:175`, `:178`); `SM_NEARBY_QUESTS`' count read un-negated (`:269` and the Poeta case); the marker bit kept in the id (`:242-253`, `:276-284`);
`SM_STATUPDATE_EXP`'s recoverable and max exp swapped (`:299`, `:304`). All six killed; the unmutated binary passes 7 of 7. The gate's own
`status-ordinal` mutant is the same refusal end to end (Y3). `FightSupportTest` and `GameSessionTalkTest` were not mutated here: the gate uses
neither `waitForRespawnAt` nor `talk()` (the rows above say why).

**Runs** (build/v, Debug, 2026-09-29, beside another tree's gates): `gs.scenario.m5d` passed in 257 s and `gs.scenario.m5d_geo` in 485 s
(one ctest, the slot runs them one after the other; the geo startup 172 s), each with an empty census, no ERROR line and the allow-list's §A
row hit once and every §B row 0 times; a last run on the final binary (a comment, an unused helper and two unused variables apart) passed in 396 s, its
startup and oracles slowed by the other tree's gates. Both game servers logged "Loaded 4226 quest handlers" (4,184 XML + 42 Java). The harness's unit cases
(the decoders, `GameSession*`, `FightSupport`, `PacketSequence`, `Oracle*`, `InventoryModel`, `AsyncAllowed`, `Scenario*Test`,
`LoginServerHarnessTest`), `QuestItemActionsTest`, `tools.porting` and `gs.chunks.consistency`: 227 of 227 at `-j 4`.

**The review's findings (2026-09-29), closed in the same tree.** The review approved the gate with its own fifteen mutants
(`AION_M5DG_MUT_R`, all killed) and four findings:
1. **`persistent-new-kept`'s cause** (low): the table's row is corrected. There is no player cache: the mutated `setPersistentState(UPDATED)`
   is also what `PlayerQuestListDAO::load` calls on every row it loads, so C11's re-entry holds NEW states and C15's quit INSERTs 1101 and
   1102 again. The kill and its rows were right and stay.
2. **The starter kinah and bandages** (low): C1-C3, C15 (Y12), C16a and C16e (Y13) compared with the literals 1,000 and 20, while the
   header, the expectations row above and §10.3's Y13 refresh say those numbers are `m5a-creation`'s. They now read
   `starterCount(creation, itemId)` over C0's two `OracleCreation` answers (the Elyos's for account A, the Asmodian's for B), and C0 logs
   them. Two schemata switched by `AION_M5DG_MUT` in `PlayerService::newPlayer` (7 more kinah, 3 fewer Bandages for a new character), the
   first one also in `tools/oracle/m5a/creation.py`:
   - `starter-data-shift` stands for a change of `player_initial_data.xml` as both independent readers would see it. **Before:** the lane's
     gate failed at exactly its five literal assertions (C1-C3: 1,007 kinah, not 1,000; C15: 1,527, not 1,520; C16a: 17 Bandages, not 20;
     C16e: 27, not 30, and 1,207, not 1,200), every other case green (234 s). **After:** the new gate passes (236 s, and 292 s in a verbose
     rerun whose C0 logged "Elyos Warrior 1007 kinah; Asmodian Warrior 1007 kinah, 17 x 169300002").
   - `starter-server-shift` (the server alone): the new gate fails at the five changed assertions (1,007 against the oracle's 1,000, 1,527
     against 1,520, 17 against 20, 27 against 30, 1,207 against 1,200) and nowhere else (241 s).
3. **Comments** (low): `ScenarioTests.cmake`'s M5d block says eight kills (one in C10, two in C12, five in C16c), not nine; the slot table's
   header says its runtimes are alone unless an entry says otherwise, and the M5d entry says it was measured beside another tree's gates
   (with the review's 220-286 s and 358 s); m5d-plan.md §19.5 points at §20.1 (slot 2, `:115` in §B).
4. **Coverage** (info):
   - C16a now asserts for the Asmodian what C4 asserts for the Elyos: no `SM_QUEST_ACTION` in the first `CM_ENTER_WORLD` burst, and any
     later `SM_NEARBY_QUESTS` of the two first-enter bursts (§10.6 (d)) decodes to the Ishalgen set. C4 and C16a log the bursts' counts: 1
     and 1 for both races in every run without a quest-start mutant. The schema `min-level-ignored` (`QuestService::checkStartConditions`
     without its min-level test) kills both new assertions (294 s; a first run before the later clause was added, 270 s, killed the first): the Asmodian's first `CM_ENTER_WORLD` burst held
     `ADD 2008 s6`, `ADD 2132 s3`, `UPDATE 2132 s4 v1` and `ADD 23830`-`23834 s4` with 8 `SM_NEARBY_QUESTS`, Ishalgen's own handlers, which C4
     (the Elyos's 1006, 1205 and 13830-13834) cannot see. C4, C11 (Y9), C13 (Y11), C15 (Y12) and C16e went red with it.
   - Not changed, because they are not defects: no run kills 210134 or 210363, since the kill spots are the three fixed plain spots nearest
     the end npc that `m5b-monster` names (all 210133 and 210364); `MonsterHunt::onKillEvent`'s `containsId` is exercised at the first id
     of 1102's list (210133) and the second of 2102's (210364), and a kill of the other two runs the same membership test.
     `player_quests` is compared on `quest_id`, `status`, `quest_vars` and `complete_count`, the columns Y9, Y12 and Y13 name; `reward` is
     the group `validateAndFixRewardGroup` sets (0 for these single-group quests), whose effect the payments of Y6, Y11 and Y13 assert, and
     whose skipped check §10.4 gives to E-06; the times are wall-clock values. `game-server/config/m5d.properties.example` stays unwritten
     (the Java tree's config is off limits; §20.1).

The two production files and the oracle file were restored and checked by sha256 after the mutation runs, the tree was rebuilt, and no
`AION_M5DG_MUT` string is left in a source, a binary or a `__pycache__` file. On the rebuilt binaries (build/v, Debug, 2026-09-29): `gs.scenario.m5d` passed in 241 s and
`gs.scenario.m5d_geo` in 348 s in one ctest, each with an empty census, no ERROR line, §A hit once and every §B row 0 times; C0 logged
1,000 kinah for both Warriors and 20 Bandages for the Asmodian, and both first-enter bursts of both races held one `SM_NEARBY_QUESTS` each.
The harness's unit cases, `QuestItemActionsTest`, `tools.porting` and `gs.chunks.consistency`: 225 of 225 at `-j 4`. `lint_concurrency.py
--werror --cycles=core game-server/src`: 3,819 files, 0 errors, 0 warnings, 0 advisories; `chunks.py check`: 71 chunks, 0 problems.

## The day lanes H and G integrated (2026-09-29)

Branch `integ/day-0929`: `fix/gate-hermetic` (lane H, `6ffcdb187`) with `m5d/stage-2` (lane G, `1a72cf0d4`) merged in, both from C++
`a72676184`. `ScenarioTests.cmake` merged without a conflict (lane H's `HermeticServersTest` registration in slot 1, lane G's two gates and
slot-table entry in slot 2); this file conflicted only because both lanes appended a section here, and both are kept, lane H's first. One
integration fix: `M5dScenarioTest.cpp`'s header still said in the present tense that a gate's server reads the owner's `mygs.properties`,
which lane H ended for every gate (the same comment edit lane H made in the M5b and M5b-2 gates); comment only, the key stays pinned. The
header requests gh-1 (`Config.h`, one static setter) and gh-2 (`CheckOutput.h`, comment only) are small and additive, as recorded.

Measured on the integrated tree (build dir `cpp/build/msvc`, Debug, the test database environment, beside other trees' runs): every target
built twice with 0 errors and 0 warnings (the second build compiled nothing); the unit suite (`-j 4 -LE
"scenario|geo|m4|nightly|stress|smoke"`) 4,677 of 4,677 in 1,558 s (5 disabled, 30 skipped); the gates and server tests (`-j 2 -L
"scenario|smoke|geo|m4" -E m5a_stress`) 58 of 58 in 2,256 s (14 disabled gate shadows), `gs.scenario.m5d` 233 s, `m5d_geo` 364 s, `travel`
58 s, `ascension` 877 s. Every gate's `census.txt` is the header alone, every `game_server.log` has the "Ignoring ./config/mygs.properties"
line and no "Loading: ./config/mygs.properties", every `login_server.log` "No override properties found". The ascension gate's ten
"onDie() exception" lines are `AbyssPointsService::addAp`'s `AION_UNPORTED` (Q06.md), as before.

**The slots, not rebalanced here.** In that run slot 2 (m5b2 pair, m5c, ascension, m5d pair) summed 2,246 s and slot 1 1,736 s, so slot 2
set the wall clock. The best single move is the M5c gate (its own prefix, no geo variant) to slot 1: about 2,017 s against 1,965 s; moving
the m5b2 pair instead gives about 2,226 s against 1,756 s. Left to the owner (the slot table above and ScenarioTests.cmake still say "the
next gate joins slot 1").

## The prologue traffic (P6-Q prologue, 2026-09-29)

The four enter-world quest handlers `_1000Prologue`, `_1100KaliosCall`, `_2000Prologue` and `_2100OrderoftheCaptain` landed on the owner's
answers 3 and 4 of 2026-09-29 (docs/design/owner-decisions.md, "2026-09-29 (answers)": U1/U7 amended for Java-faithful quest traffic, each
gate change listed). A new character's first enter world in Poeta / Ishalgen now carries their Java traffic, and every gate of this
directory that creates one and checks or walks after it was taught it instead of holding the handlers back, except the nightly stress run
(below); nothing else in a gate changed. The shared pieces:

| File | What |
|---|---|
| `PrologueSupport.h/.cpp` (new) | The §5.8 notation of the prologue's packets and three pure checks written from the Java: `expectPrologueMissionLocked` (the first CM_ENTER_WORLD's one SM_QUEST_ACTION adds 1100 / 2100 LOCKED, and SM_QUEST_LIST holds exactly that quest), `expectPrologueStarted` (the first CM_LEVEL_READY's one SM_QUEST_ACTION adds 1000 / 2000 START and its one SM_PLAY_MOVIE plays movie 1 / 2 as a skippable CutSceneMovie with no target) and `expectPrologueMovieEndAnswer` (SM_STATUPDATE_EXP with the reward's 1 exp, STR_GET_EXP2, SM_QUEST_ACTION UPDATE COMPLETE and SM_NEARBY_QUESTS); `endPrologue` runs the second, ends the movie with CM_PLAY_MOVIE_END echoing it and runs the third on the answer |
| `PrologueSupportTest.cpp` (new) | The three checks on hand-built packets: the Java bursts of both races pass with no failure, and each assertion fails, alone, on a burst that differs from Java in exactly its field (35 deviation cases; p6q-ascension-route.md §9.5 has the 28 mutants they kill) |
| `decoders/QuestDecoders.h/.cpp` | `decodePlayMovie` (SM_PLAY_MOVIE.java:27-35) and `decoders::Prologue` (the quest, movie, mission and map of each race, `PROLOGUE_EXP`, `PROLOGUE_EXP_MESSAGE`), with `QuestDecodersTest.PlayMovieIsFifteenBytes` and `ThePrologueQuestsAreTheJavaHandlers` |
| `GameSession.h/.cpp` | `CM_PLAY_MOVIE_END` (packets[81]) and its builder (CM_PLAY_MOVIE_END.java:33-40), with `GameSessionTest.PlayMovieEndBody`. A real client sends it when a movie ends or is skipped; until then the server drops every CM_MOVE (SM_PLAY_MOVIE.java:28 sets WATCHING_CUTSCENE, CM_MOVE.java:159-161) |

The gates (docs/design/p6q-ascension-route.md §9 has each failure and the Java lines): m5a / m5a_geo (the first-enter and level-ready
sequences, V10's quest list, the prologue of the Warrior, the Mage and the geo Warrior), m5b / m5b_geo (the sequences, K2, K3, and R1's exp
over the prologue's 1), m5b2 / m5b2_geo (the sequences, the Warrior's and the Mage's prologue), m5b3 / m5b3_geo (the sequences, C1-C3), m5c
(C1's two characters, X15's exp over the prologue's 1) and the phase-6 gate ascension (E1: the mission START at level 9 and the prologue's
movie ended before E2's walks). `gs.scenario.travel` passes unchanged.

Not taught (p6q-ascension-route.md §9.5 has the detail):

- **`gs.scenario.m5a_stress`** (nightly, DISABLED unless `AION_STRESS_NIGHTLY`). Its Elyos Warriors meet the prologue in each client's
  first round. The run reads that level-ready burst up to SM_CUBE_UPDATE without checking it, so no expectation fails, but it never
  sends CM_PLAY_MOVIE_END: that round's walk is dropped, and quest 1000 stays START. Later rounds get no movie (`_1000Prologue.java:28`
  starts the quest only when the character does not have it) and walk as before. No assertion of the run reads the first walk (drift,
  census, live counts and log scans), but that comes from reading the code, not from a run. The fix is to end the movie after the first
  level ready and wait for the answer's SM_QUEST_ACTION before walking. It needs a stress run to verify, and a stress run needs the
  owner's go-ahead (run-aion-cpp SKILL.md), so it waits for the next nightly.
- **`gs.scenario.m5d` / `m5d_geo`** are on C++ (PR #10, lane G) but not in this change's base (`a72676184`). `M5dScenarioTest.cpp` is
  built on the four handlers being held back, so it has to be taught when the two meet. The places are its header (:21), the
  `REGISTERED_JAVA_QUESTS` filter of the Y1 / Y13 nearby sets (:143), `enterWorldPattern` (:665), `levelReadyPattern` (:682-688, asserted
  :1028), Y13's empty SM_QUEST_LIST (:2234), and ending the movie before `walkTo` / `walkToTalk` (:1062, :1611). The file keeps its
  own helpers on purpose (:35), so whether it includes PrologueSupport is decided in that merge. **Taught at that merge** (branch
  `integ/slice2-prologue` on C++ `51ef338e4`, 2026-09-29): the pair includes `PrologueSupport.h` (the one shared check), expects the
  prologue's packets in both first-enter sequences, ends both characters' movies before the first walk, checks each mission LOCKED, counts
  the prologue's STR_GET_EXP2 in its exp ledger, and reads 1000 / 2000 COMPLETE and 1100 / 2100 LOCKED in Y6, Y9, Y12 and Y13;
  docs/design/p6q-ascension-route.md §10 has each change, the failure it answers and its mutants.

## Small tasks 2026-09-30 (branch `fix/small-quick-4`): the static objects of SM_GATHERABLE_INFO

The owner saw Sanctum's crafting benches appear late in the play session of 2026-09-29; reading found `SM_GATHERABLE_INFO` identical to
Java for a static object, and noted that the gate could not have told otherwise. Two checks are tighter now, and the packet has a byte test
(`tests/sm_ak/StaticObjectGatherableInfoTest.cpp`, P4-16, label `realdata`: oven 104 and workbench 109 spawned by
`StaticObjectSpawnManager::spawnTemplate` from the real world map, item and Statics rows, each body compared with constants taken from the
data files with Java's `Float.parseFloat` bits, never from the port).

| Area | As built | Reason |
|---|---|---|
| `decodeGatherableInfo`'s state | A body whose template id is 300001 must carry 9 or 10, every other body 1 (`decoders::STATIC_DOOR_TEMPLATE_ID`) | It accepted 1, 9 or 10 for any object. Java writes 9 / 10 only for a `StaticDoor` (SM_GATHERABLE_INFO.java:28-35), and a door's template is a `StaticDoorTemplate`, whose `getTemplateId()` is the constant 300001 (StaticDoorTemplate.java:55-57) that no gatherable or item template uses, so the decoder can tell a door from the bytes alone |
| `M5cScenarioTest`'s `staticObject` | Matches C19's oven by template id, static id **and** the oracle's spawn spot (x, y and z exactly; `EconomyTool`); a packet with the template and static id elsewhere is reported (`ADD_FAILURE`) and does not match | It matched the template and static id only, so a static object written at a wrong position passed. The oracle's spot is the f32 of the same XML attribute the server parses (no double rounding for any Sanctum spot, checked) |

Mutation proof (`AION_SQ4_GATHER_MUT`, one build; the two sources restored and sha256-checked, rebuilt, no mutant string left):
`SM_GATHERABLE_INFO` writing the l10n as `2n+1`, the state 9 for a static object, the static id and template id swapped, or x + 1 each fail
both byte cases (`StaticObjectGatherableInfoTest.cpp:253` and `:266`); the decoder accepting 9 for a non-door fails
`VisibilityDecodersTest.GatherableInfo` (:175 and :177); x + 1 in a `gs.scenario.m5c` run fails C19 at the new position check
(`M5cScenarioTest.cpp:985`, "is at (1850.788, ...), not at its spawn spot (1849.788, ...)") and then at `:2966`.

### `MethodStats.log` follows `--log-folder` (the same small tasks)

Every gate run rewrote `game-server/log/stats/MethodStats.log`: `RunnableStatsManager::dumpClassStats(sortBy)` (commons; the game server's
`ShutdownHook` calls it at every orderly shutdown, Java ShutdownHook.java:74) hard-coded Java's `./log/stats` and ignored `--log-folder`.
It now writes `<Logging::getLogFolder()>/stats/MethodStats.log`, the folder of the last `Logging::init`, which is where the appenders' files
go and where `Logging::archiveLogs` already collects `stats/MethodStats.log`. Production passes no `--log-folder`, so its folder stays the
default `log` and the file stays Java's `./log/stats/MethodStats.log` (DEVIATIONS.md, "commons / utils"; header request sq4-1 in
docs/porting/header-requests.md for the new `Logging::getLogFolder()`).

Tests: `RunnableStatsManagerTest.DumpWithoutAFileWritesIntoTheLogFolderOfLogging` (a log folder outside the working directory receives the
file, the working directory gets no `./log`) and `DumpWithoutAFileUnderTheDefaultLogFolderWritesJavasLogStatsFile` (the default folder
gives `./log/stats/MethodStats.log`). Mutation proof (`AION_SQ4_STATS_MUT`, schemata in `Logging.cpp` and `RunnableStatsManager.cpp`, one
build, restored and sha256-checked, rebuilt, no mutant string left): the old hard-coded `./log/stats` fails `:235` and `:236`;
`getLogFolder()` answering `log` whatever init was given fails `:230`, `:235`, `:236`; the file without its `stats` folder fails `:235` and
`:256`; an absolute `getLogFolder()` fails `:251` (lines of `RunnableStatsManagerTest.cpp`). `gs.smoke.startup` on the rebuilt server passed and wrote
`build/msvc/game-server/gs.smoke.startup/Debug/log/stats/MethodStats.log`; `game-server/log/stats/MethodStats.log` kept its time stamp.

## M5e gate (lane 1, 2026-10-04): `gs.scenario.m5e` and `gs.scenario.m5e_geo` (m5e-plan.md G-01..G-05, I-02, M-06, T-03, §10)

`TEST(M5eScenario, Run)` and `TEST(M5eScenarioGeo, Run)` in `M5eScenarioTest.cpp`, one shared body. The output goes to `<bin>/scenario/m5e`
and `<bin>/scenario/m5e_geo`, with the schema pairs `aion_{gs,ls}_test_m5e_<hash>` and the allow-list `m5e_partial_allowlist.txt`
(`AION_SCENARIO_M5E_PARTIAL_ALLOWLIST`).
- Labels: `scenario;realdata` (`;geo`). TIMEOUT: 2700 / 3600.
- Both tests are in **gate slot 1**. The slot table adds their measured runtimes, which make slot 1's sum 2,801 s.
- The discovered cases are DISABLED (`^M5eScenario(Geo)?\.`).
- The profile is `game-server/config/m5e.properties.example` (I-02). It holds M5d's keys and `rates.xp.quest` pinned at its default, with
  `gameserver.simple.secondclass.enable = false` for play (D1, answered 2026-10-04: the retail ascension route). The gate passes the M5d
  gate's keys and `gameserver.simple.secondclass.enable = true`, its only way to seed a class change.

This is test infrastructure only, with no Java counterpart. The rows below say where the gate reads the plan's §10 differently and why. In
every row, the gate follows what Java does.

| Area | As built | Reason |
|---|---|---|
| The oracle (G-01) | `oracle.py m5e-progression` answers for the gate (since the review, `level:L` caps a non-Daeva at 9 as `setExp` does and `create` leaves `old_level` at 0; `m5e-stumble` answers X9g's geo row). **Per step** (`create`, `enter:L`, `level:L`, `class:C`, `action:ID`, `quit`, `seed:CLASS`, `book:ITEM`), it gives the level, the exp thresholds, each skill with its `SM_SKILL_LIST` message id (`addSkill`'s `isNew` walk over the skill tree, SkillTreeData.java:94-162), the 30001 → 30002 swap, the class-change pages and actions, and the base max HP. **Per cast** (`--skill`), it gives the conditions evaluated against the seeded character (weapon, DP, `ride_robot`, the chain, the target), and the charge window (`skill_charge.xml`, `useChargeSkill`). **Per item**, it gives a weapon's group, equip skill and robot id, and a stigma stone's skill and price (`--stigma`: `PricesService.getPriceForService` over m5c's trade config). The npc spots come from `m5b-monster` and `m5a-spawns`, not the plan's `--npc`/`--map`. A book's skill comes from the `book:` step, not `--item`. `tests/test_m5e.py`: 27 cases | G-01, §10.1 |
| The Daeva's alternative page | **asteros 203058**, not Pernos 790001. `DialogPage.getStartPageId`: `hasQuestInteraction` (page 10) wins over the Daeva branch (DialogPage.java:113-126). Pernos is the start npc of 1123 from level 7, since `_1123WheresTutty` registered in phase 6, so he answers 10 both before and after the class change. asteros starts no quest (m5d-quests), so it answers 1011 before and 1352 after | measured on the first run |
| The level-ups | C4-C10 kill the 2-HP junk monster 210340 beside asteros. Each kill gives one exp, which crosses a seeded threshold (`startExp(L) - 1`). Pernos's level-7 monsters (747 HP) killed a level-8 Warrior. A target that walked off ("too far", 1402920) is approached again | measured |
| X2's comparison | The full `SM_SKILL_LIST` is compared by **skill id sets**. `SkillEntryWriter` writes level 1 for every normal skill (SkillEntryWriter.java:27), so a level comparison would compare nothing | Java |
| B's Daevas | B1-B5 are created, enter once at level 1 (X3: no class window), and are then seeded offline. The seed sets the class, `exp = startExp(10)`, `player_quests(1006, COMPLETE)` and the stand. Their first enter world as level 10 runs the load path's `updateDaeva` over the quest list (PlayerCommonData.java:276, 588-610). The seed also sets `inventory.item_skin` together with `item_id`, because `RideRobotEffect` reads the robot id through `Item.getItemSkinTemplate`. The first run, which kept the old skin, answered robot 0 | D8, measured |
| C15 before C13 | The resurrection runs while A1 is a level-10 Gladiator without 563. At level 15, A1's revive drove the lock-order inversion of the findings below (lockdep and ERROR lines). PR #77 corrects it (`CondSkillLauncherEffect.cpp`, the owner's decision of 2026-10-04), so the order is no longer needed for the deadlock; it is kept as the order the gate was measured in | Java's inversion, corrected by #77 |
| 1699 on the living | It is **refused before any cast**: `STR_SKILL_TARGET_IS_NOT_VALID` (1300013). It is not answered with status 16. The revive clears B1's target (PlayerReviveService.java:190-193) and the cast uses the server-side target (PlayerController.java:460), so B1 selects A1 again first (since the review; before it the refusal seen was canUseSkill's "no player target" branch, PlayerRestrictions.java:106-108). Then `PlayerRestrictions.canUseSkill` refuses a skill with a resurrect effect on a living player (PlayerRestrictions.java:105, 110-113, from PlayerController.java:474), so `ResurrectEffect.calculate`'s `isDead` guard is never reached. **§10.4's `resurrect-no-dead-guard` row therefore cannot fail in the gate.** The mutant run (before the review) was green | Java |
| X13's max HP | After the soul sickness, the gate asserts that A1's max HP is **lower** than before, not "the oracle's 70 %". The 8296 penalty's stat functions apply over the gear and passives, which the oracle does not model | measured |
| C12 at level 15 | C12 runs **after C13**. A1 relogs at the village with DP 2,000 and HP at a quarter of the level-15 base max. A run that seeded half had reached the max HP by 758. At a level difference of 10, the junk monster no longer aggroes A1 (CreatureEventHandler.java:107-110), so it does not run at A1 between casts (X9g) | measured |
| The chain (X9) | 769 is cast again, after its 10 s cooldown and up to four times, while its chain status is not success. A dodged or resisted 769 blocks the chain even at `chain_skill_prob` 100 (Skill.java:599-607, 628-631). A 758 whose every effect was dodged or resisted is played again in a new round (since the review) | Java |
| The drains (X9) | Each drain update must be exactly ⌊d × p / 100⌋ of a hit. An update whose HP byte is 100 % may be less, because the update carries `newHp - previousHp` capped at the max HP (CreatureLifeStats.java:185, 191). At least one drain of each skill must come in below the max, so the drain is really measured. **A dodged, resisted or conflicting effect expects no update** (since the review): it has no success effect, so `Effect.applyEffect` returns before the drain is scheduled (Effect.java:545-563, 597-600) | Java; seen in a mutant run and a clean run |
| X9g | The plan says the stumble ends exactly 2 m along the heading without geo. The gate measures it against the target's last **broadcast** position, which is exact only once the npc stands. **The npc goes where its last packet points:** the target of an `SM_MOVE`, or the move controller's target that `SM_NPC_INFO` carries for an npc seen while it walks (SM_NPC_INFO.java:110-112, `NpcMoveController.getTargetX2`). It arrives at the junk monster's walk speed (0.408 m/s, `npc_templates.xml`, only a wait). The cast waits until 1 s after that arrival, 30 s at most. A stumble result moves the npc in the gate's model, because the server sends no `SM_MOVE` for it (StumbleEffect.java:41). The row asserts ≤ 2 m + 0.9 m and "away from A1" (−0.9 m). Without geo it also asserts ≥ 2 m − 0.9 m and z within 0.5 m. **With geo (since the review)** it asserts, for each stumble whose segment `oracle.py m5e-stumble` finds open in the geo data, ≥ 2 m − 0.9 m and z = the oracle's `GeoMap.getZ(x, y, z + 1, z − 2)` at the end within 0.01 m (GeoMap.java:141-147); at least one such stumble must come. "Open" is conservative: no geometry with a `DEFAULT_COLLISIONS` intention crosses the ray of `getClosestCollision` (triangle tests against the geometries whose bounds hold a point of the segment), and the topmost surface stays 0.3 m below the ray at 0.1 m samples. A stumble of a target that may still have been walking, or (geo) on a segment that is not open, is logged, not measured. The budget is a **fixed** 240 s, not derived from X12's 10^-4 rule: a stumble comes from 519's subeffect (once per DP seed) and from the critical proc of 769 / 758 (10 % of a critical, Effect.java:533), and the critical rate is a stat the oracle does not model; the runs below give when the measured stumble came. Each stumble also logs the point it implies: 2 m short of the end, on the line from A1, where `getHeadingTowards` put it | measured: a monster that ran to A1 stops up to 0.6 m short of its `SM_MOVE` target. Before the `SM_NPC_INFO` target was read, a monster first seen mid-walk stood 1.32 m from its broadcast position, and a stumble measured 3.04 m |
| The robot and charge (X15) | Before the charge, B3 is put 4 m from a target. A start refused as too far is retried on a target up to three times. The releases are at 700 ms (2606) and 3,500 ms (2608), the oracle's window | measured |
| The book's cast (X16) | 1417 is cast at a target 6 m away. A refused cast is retried on the next target, three targets at most. Once, on the geo run, 1417 was refused at 10 m | measured |
| The summon (X17) | After `CM_SUMMON_COMMAND(ATTACK, target)`, the gate sends `CM_SUMMON_ATTACK(summon, target, time)` (opcode 203, G-02's builder). The ATTACK command only sets the mode (SummonController). The hit is the client's packet | Java |
| The stigma (X18) | The stone is 140001109 *Crippling Cut* (a RARE Gladiator stone of level 20). The kinah comes from `--stigma`: 25,000 for RARE, through `PricesService`. The slot is STIGMA1 (`1L << 30`). The refusal without 1929 is asserted first, then the equip with `player_quests(1929, COMPLETE)` seeded | §10.3 X18 |
| X14 and X7 | The X7 `SM_FLY_TIME` rows and X14's "no 8998 after the toggle-off" window are EXPECTs, so a mutant shows every row it breaks | the case log |
| X14's non-toggle (since the review) | B2 first casts **1685** *Protectorate's Prayer* on itself (a Chanter's level-10 skill, ACTIVE, a shown one-hour BUFF; C0 checks the oracle has it), and the gate checks an `SM_ABNORMAL_STATE` lists it. Then `CM_TOGGLE_SKILL_DEACTIVATE(1685)` must send no `SM_ABNORMAL_STATE` in 2 s. A port that skipped the toggle guard (CM_TOGGLE_SKILL_DEACTIVATE.java:34-37) would remove the buff and send one. Before, B2 had never cast 1685, so the row could not fail | §10.3 X14 |
| X13's MP (since the review) | The first `SM_STATUPDATE_MP` after the revive carries 35 % of the max MP (`setCurrentMpPercent(35)`, PlayerReviveService.java:197) | §10.3 X13 |
| C15's death (since the review) | A1's death is read from the recording since before the walk to the monster, so a death before `fightUntil` reads (or one that makes the target selection go unanswered) is not missed | measured risk |
| C0 (since the review) | C0 fails when the oracle refuses any cast of B's characters (1699, 1809, 1685, 2767, 2606, 1417, 3706) | G-01 |
| C21 (since the review) | The stop file is written **with A1 online**, as §10.2 C21 has it; B logs out first. After the stop, A1's `players.online` must be 0 | §10.2 |
| X20 (since the review) | The G-07 relation rows, as M5b2's X13 compares them: `live Effect == effectsHeld`, `Effect created > effectsHeld`, `live Skill == skillsHeld`, `live EffectReserved <= effectReservedCapacity` (from `m5a_summary.txt`), plus `Summon` live 0 with `created > 0` and `Player` live 0. Before the review the comment said the census covered the relation rows; it did not | §10.3 X20 |
| C12's rounds (since the review) | A replayed 769 / 758 round waits out 758's 40 s cooldown (skill_templates.xml: `cooldown="400"`) and, like every 758 after 769, 3 s for 769's animation (`player.getNextSkillUse`, CM_CASTSPELL.java:99-105); both refusals (`STR_SKILL_NOT_READY`) were measured. A 758 drain that only reached the max HP (after a replay's minute of regeneration) replays the round. 2981 refused with `STR_SKILL_OBSTACLE` on the geo gate is cast at the next target, three at most. A cast retried after "too far" waits for the target's arrival again before it counts a stumble as measurable (a run's only 519 stumble was lost to it) | measured on the review's runs |
| C20's seed | `player_quests(1929, COMPLETE)` is an upsert (`ON DUPLICATE KEY UPDATE`): once 1929's handler is ported (M5f D13), the level-20 enter world may write the row first | review |
| Allow-list | §A `BaseService.cpp:18`; §B G-07's four cron rows; §C `PvpMapService.cpp:32`, `PlayerService.cpp:268`. The milestone leaves no partial of its own on the path | §10.1 |

**Mutation proof (§10.4).** The schemata are switched by `AION_M5EG_MUT` in 14 production files: `ClassChangeService.cpp`,
`PlayerCommonData.cpp`, `PlayerController.cpp`, `PlayerSkillList.cpp`, `CondSkillLauncherEffect.cpp`, `CM_TOGGLE_SKILL_DEACTIVATE.cpp`,
`Effect.cpp`, `ResurrectEffect.cpp`, `PlayerReviveService.cpp`, `RideRobotEffect.cpp`, `SkillLearnAction.cpp`, `_1205ANewSkill.cpp`,
`SummonsService.cpp` and `HostileUpEffect.cpp`.
1. The schemata were built once into `aion_game_server`. The gate's server inherits the variable.
2. The sources were restored right after the build and checked by sha256: 14 of 14 OK, twice (after the gate build, and again after the unit-test build below).
3. The tree was then rebuilt clean.

There was one `gs.scenario.m5e` run per mutant, in three batches. Batches 1 and 2 ran on the gate as first committed, batch 3 on the final gate. The case log shows every row.

| Mutant (§10.4 row) | Failed | Note |
|---|---|---|
| `skip-update-daeva`: `setClass` without `updateDaeva` | **X7** (no level 10, no Daeva skills, the glide refused, page 1011) | X8 green in its rerun, as §10.4 predicts: the load path promotes. The first run also failed C12 on the chain, before the chain retry existed |
| `offline-daeva-ignores-quests`: `updateDaeva` ignores the loaded quest list | **X8** (the glide refused with 1301059, asteros 1011) | It cascades: B's seeded Daevas are no Daevas either, so B1 cannot resurrect (X13) |
| `learn-from-10`: `learnNewSkills(10, level)` in `setClass` | **X5** (no level-9 passives) | C12's 519 was then refused: no greatsword skill |
| `player-info-to-self` | **X5** only (A1 receives an `SM_PLAYER_INFO`) | |
| `no-learn-on-level`: `onLevelChange` without `learnNewSkills` | **X2**, X5, X7 | |
| `no-class-range-check` | **X4** (A1 became a Sorcerer; O saw class 7), X5, X7, X8 | |
| `dialog-without-level` | **X3** (A1 at level 1 and all five B characters got a class window: pages 2375, 3398, 3398, 3739, 3057, 3057) | |
| `max-level-for-all` | **X3** (level 10 without the class), X4, X5, X7 | |
| `is-new-always` | **X5**, **X7** (1300050 for the skills with a known pre-skill) | X2 green, as predicted |
| `cond-launcher-unported` | **X10** (`SM_ENTER_WORLD_CHECK` 2 at level 15), X19 (unported trace, ERROR, census) | |
| `toggle-no-remove` | **X14** (8998 and `SM_MANTRA_EFFECT` after the toggle-off), **X15**'s last step (no robot 0) | final gate: no other row |
| `aura-task-kept`: `Effect::stopTasks` keeps 1809's task | **X14**, X19/X20 census (the leaked Effect keeps its Player) | |
| `resurrect-no-dead-guard` | **nothing** | Expected: the target filter refuses first (the 1699 row above). On the final gate, its only red row was an X9 drain update capped at the max HP and an X9g walk, both fixed since |
| `revive-skill-zero` | **X13** (no 8296 in SPEC2) | |
| `robot-zero` | **X15** (`SM_RIDE_ROBOT` with robot 0, then no release) | final gate |
| `book-known-ignored` | **X16** (the second book used up) | |
| `no-1205` | **X1** | |
| `release-keeps-summon` | **X19/X20** census (the Summon and its Player kept) | |
| `elyos-select5-swap` | **X5** (O saw class 2), X7, X8 | |
| `asmo-select7-swap` | **nothing in the gate**, as §10.4 says | `aion_gs_playersvc_tests` with the variable fails `ClassChangeServiceTest.TheAsmodianPagesAreShiftedByOneAndCarryTheEngineersAndArtistsOnTheirOwnActions` alone; without it, every case that ran passed (140 passed, 56 skipped without a database) |
| `hostileup-no-hate` | **nothing in the gate**, as §10.4 says | `aion_gs_effects_al_tests` with the variable fails four `DaevaEffectsTest` cases: `InciteRageAddsTemporaryHateForFiveSeconds`, `TheEffectorsDeathCancelsTheTemporaryHatesRemoval`, `TauntAddsItsHateWithoutATask` and `MockingBlastAddsItsTemporaryHateWithoutTheEffectHate` (169 of 173 pass; without it, 173 of 173) |

**The review's mutants (2026-10-05).** Switched by `AION_M5E_REVIEW_MUT` in `StumbleEffect.cpp` and `Effect.cpp`, built once into
`aion_game_server`, the sources restored at once and checked by sha256 (2 of 2 OK), then the whole tree rebuilt; the string is in no binary.

| Mutant | Gate | Failed |
|---|---|---|
| `stumble-origin`: the stumble ends where the target stood (`getClosestCollision` answering the start point) | `m5e_geo` | **X9g**: moved 0 m against the lower bound 1.1 m on open ground |
| `stumble-nogeo`: the 2 m point with z unchanged (a port that ignores geo) | `m5e_geo` | C12 before X9g: no 519 stumble in that run, and a replayed 758 refused with `STR_SKILL_NOT_READY` (the gate's own replay bug, fixed since). Not a measurement of X9g's z row. The z row can only see it where the terrain slopes: the measured open-ground stumbles changed z by 0 to 0.19 m (118.875 → 119.066), and a flat end (0.000-0.005 m) passes it |
| `effect-leak`: `Effect::endEffect` retains the ended Effect | `m5e` | **X20**: `live Effect` 663 against `effectsHeld` 309, `live Skill` 315 against `skillsHeld` 309; also X19 (census, ERROR lines, "removed from the world still alive") |

**Findings.**
- **A lock-order inversion inherited from Java.** **Corrected by PR #77** (`CondSkillLauncherEffect.cpp`, the owner's decision of 2026-10-04, both branches; docs/deviations/P5-03.md "CondSkillLauncherEffect: the lock-order correction"). A level-15 Gladiator with 563 *Determination* (`CondSkillLauncherEffect`) was revived by C15. lockdep reported an inversion, with ERROR lines:
  - One path: the `CondSkillLauncherEffect` action observer → `Effect.endEffect` (`synchronized (this)`, Effect.java:712).
  - The other path: `Effect.startEffect` (`synchronized (this)`, Effect.java:653) → `CreatureGameStats.checkMaxHPChanged` (`synchronized`, CreatureGameStats.java:371) → the observer (`CondSkillLauncherEffect$1`, `synchronized (this)`).

  Java nests the same monitors, so this is a potential deadlock of Java's. The gate still runs C15 before C13; with #77 that is no longer needed for the deadlock.
- **The census of two failed mutant runs** listed a `Player` alive at the stop: `offline-daeva-ignores-quests`, and `hostileup-no-hate` on the final gate. In each, a case before C21 ended fatally, with A1 online in a fight or dead. No passing run shows it. It is not investigated here.

**The review's runs** (build/msvc, Debug, 2026-10-05; the gate changed between them, as the C12 rounds row says):
- On the final commit: `gs.scenario.m5e` passed in 338 s, 360 s and 373 s and failed once on X19's watchdog (below); `gs.scenario.m5e_geo` passed in 494 s and 487 s.
- Before it: m5e passed 4 times (413, 367, 351, 339 s) and failed 3 times: once on X19's watchdog (below), once on X9g (the lost 519
  stumble), and once, the first, on the watchdog while the oracle suite ran beside it; m5e_geo passed 5 times (499, 479, 496, 483, 479 s)
  and failed twice on C12's replay path (fixed since).
- **Open: X19's watchdog on the plain gate.** In every `gs.scenario.m5e` run the server logs `MapRegion::activate` instant tasks of 5-9 s
  (MapRegion.cpp:141) at the enter worlds in Poeta; when one runs past the watchdog's sampling, `watchdog.txt` has a SLOW_TASK and X19
  fails. The geo gate's runs log none. Nothing of the review touches the server; it is not investigated here.
  **Since (fix/region-activate, 2026-10-05):** not every run. The review's archived logs have all activations of a run slow (5.7 s mean) in
  3 of 11 runs and all fast (0.12-0.13 s mean) in the others. The cause is the Debug lock-order validator's thread edge cache, not the
  port of MapRegion (it does Java's work): DEVIATIONS.md, runtime kernel, "Lock-order validator edge cache".
- X9g's measured stumble was 519's in every passing run (1-19 s after C12's first cast); the implied position was 0.002-0.05 m from the
  broadcast one.

**Runs** (build/msvc, Debug, 2026-10-04):
- The geo gates of the earlier milestones (G-05), one run each on this tree: m5a_geo 150 s, m5b_geo 344 s, m5b2_geo 314 s, m5b3_geo 321 s, m5d_geo 376 s; all passed.
- The final gate ran on binaries rebuilt clean after the mutation proof; no `AION_M5EG_MUT` is in the server, the scenario tests, the two unit-test executables, or any `.lib`:
  - `gs.scenario.m5e` passed in 342.6 s and `gs.scenario.m5e_geo` in 483.8 s, in one ctest. `gs.scenario.m5e` passed again in 342.2 s.
  - Each run had an empty census, no ERROR line, §A hit once, every §B row hit 0 times, and §C's PvpMapService once.
  - X9g's implied position was 0.002-0.05 m from the broadcast one in six measured stumbles (four runs). The geo run's stumble changed z with the terrain (118.875 → 118.976).
- Earlier runs, before the last fixes of this lane:
  - The gate as first committed: 328 s and 482 s.
  - With the arrival rule but the half-HP seed: m5e 365 s and 379 s, m5e_geo 481 s.
  - Failing runs, each fixed: X9g 3.04-3.24 m, from a monster seen mid-walk; 1417 refused once on the geo run; a 758 drain capped at the max HP.
- Other checks:
  - The harness's unit cases (GameSession*, ProgressionDecoders*, QuestDecoders*, SkillDecoders*, CombatDecoders*): 86 of 86.
  - `tools/oracle` `tests.test_m5e`: 22 of 22. The whole oracle suite (539, 1 skipped) passed at G-01's commit.
  - `lint_concurrency.py --werror --cycles=core game-server/src`: 3,892 files, 0 errors, 0 warnings, 0 advisories.
  - `chunks.py check`: 71 chunks, 0 problems.
  - `census.py --self-check`: 0 failures, after `SummonMode.getId`'s known answer was updated.

  The batch-3 mutant runs used the gate before the arrival rule, the quarter-HP seed and the capped-drain rule. The mutants were not re-run on the final gate; its changes make the harness wait for the target and measure it more exactly. One rule is looser: an update at 100 % HP may be below its drain, which is what Java sends. One row is new: at least one drain must be measured below the max.

## M5f gate (lane A, 2026-10-05): `gs.scenario.m5f` and `gs.scenario.m5f_geo` (m5f-plan.md G-01..G-05, I-03, §10, §17)

`TEST(M5fScenario, Run)` and `TEST(M5fScenarioGeo, Run)` in `M5fScenarioTest.cpp`, one shared body. The output goes to `<bin>/scenario/m5f`
and `<bin>/scenario/m5f_geo`, with the schema pairs `aion_{gs,ls}_test_m5f_<hash>` / `..._m5fgeo_<hash>` and the allow-list
`m5f_partial_allowlist.txt` (`AION_SCENARIO_M5F_PARTIAL_ALLOWLIST`).
- Labels: `scenario;realdata` (`;geo`). TIMEOUT: 2700 / 3600. Both tests are in **gate slot 2** (the smaller sum: 2,095 s against
  2,801 s); with them slot 2's sum is 2,618 s. The discovered cases are DISABLED (`^M5fScenario(Geo)?\.`).
- The profile is `game-server/config/m5f.properties.example` (I-03, the play values). The gate passes the M5a profile plus
  `gameserver.instance.solo.destroy_delay_seconds = 1` (D11), the fly-path validator off, `gameserver.simple.secondclass.enable = false`
  and geodata per variant.
- The oracle is `oracle.py m5f-travel` (G-01, `tools/oracle/m5f`), the decoders `decoders/TravelDecoders.{h,cpp}` (G-02).

This is test infrastructure only, with no Java counterpart. m5f-plan.md §17.3 lists the rows the gate reads differently from §10 and why;
in every row the gate follows what Java does. Two placements differ from the plan:

| Area | As built | Reason |
|---|---|---|
| G2c | `tests/cm_lz/AscensionPacketsTest.cpp`, `ATeleportResetsTheClientPositionACollisionObserverStartsFrom`, beside the `CM_MOVE_IN_AIR` case that records the client position (P5-16's directory, the ascension lane's file), not `tests/geo` | no fixture of `tests/geo` (P4-04) has a Player; the case needs one with a move controller |
| G2a, G2b (geo on) | not built | both need a Player in a world with the geo meshes loaded, which no unit fixture provides; the geo gate's G2 row counts the material actor, and G2b's geo-off half is `TeleportServiceRestTest`'s `teleportToNpc` case |

**Mutation proof (§10.4).** 22 schemata switched by `AION_M5FG_MUT` in 13 production files (`TeleportService.cpp`,
`PlayerController.cpp`, `BindPointTeleportService.cpp`, `PlayerEnterWorldService.cpp`, `CM_MOVE_IN_AIR.cpp`, `ResurrectAI.cpp`,
`PortalService.cpp`, `InstanceService.cpp`, `PricesService.cpp`, `GeneralInstanceHandler.cpp`, `PlayerReviveService.cpp`,
`WorldMapInstance.cpp`, `Player.cpp`; a handler file takes the switch as an expression, since a handler may have no anonymous namespace).
Built once into `aion_game_server`, `aion_gs_playersvc_tests`, `aion_gs_instance_tests` and `aion_gs_cm_lz_tests`; the sources restored at
once and checked by sha256 (13 of 13 OK, twice); the tree rebuilt; `AION_M5FG_MUT` is in no executable. One `gs.scenario.m5f` run per
gate mutant (17), one run of the named unit suite per unit mutant (5): **all killed** at the rows of m5f-plan.md §17.4's table. Notes:
- `skip-despawn` is killed by **T1** before T10 is reached: without the despawn `SpawnTask.run` returns for the spawned player
  (TeleportService.java:502), so even the hotspot's same-map move never happens.
- `bind-obelisk-position` fails **T6** only: T8 compares with T6's own packet, which the mutant moves as well.
- A mutant that ends a case fatally also fails T20's "Haramel was created" rows (the run never reached Haramel); `skip-leave-instance`
  additionally leaves one `WorldMapInstance` alive (162 against the baseline 161), which T20 sees.
- `EmptyInstanceCheckerTask` ignoring `isRegisteredTeamDisbanded` is not built (a `GeneralTeam` is M5g's, D3).

**Runs** (`build/msvc`, Debug, 2026-10-05):
- Development runs, each fixed: C0's parse of the oracle's null heading of a hotspot; C7's accept refused at 5.07 m (the post-landing move
  went away from the obelisk); T20's live-count row names (`WorldMap2DInstance` / `WorldMap3DInstance`); T13's opening message read before
  it arrived.
- Final gate: `gs.scenario.m5f` passed in 198 s and `gs.scenario.m5f_geo` in 325 s in one ctest. Each: an empty census, no ERROR line, §A
  hit once, every §B row 0, §C's `PvpMapService.cpp:32` once.
- **G-05, the regate** (§10.6), on the final tree after the mutation proof, one `ctest -R '^gs\.scenario\.' -LE stress -j 2` (2,796 s):
  16 of 17 passed - m5a 70 s, m5a_geo 171, m5b 271, m5b_geo 370, m5b2 201, m5b2_geo 304, m5b3 216, m5b3_geo 321, m5c 290, m5d 270, m5d_geo
  365, m5e 486, m5e_geo 536, ascension 924, m5f 283, m5f_geo 359. `gs.scenario.travel` failed in T3 at the **login server** ("CM_LOGIN:
  expected opcode 3, got opcode 1 with reason 16777223", account B refused before any game-server packet) and passed alone right after
  (44 s). Nothing of this lane touches the login server or the travel gate; recorded, not investigated. §10.6 (b)-(d): no earlier gate
  asserted W-08's loud flight master any more (the travel core flipped `DialogServiceTest`'s row), and the earlier gates' packet sequences
  are unchanged (they all pass unmodified).
- Unit: `TravelDecodersTest` 10, `GameSession*`, `AscensionPacketsTest` (with G2c), `BindPointTeleportTest` (with the float vector): 48 of
  48 under the database lock. `tools/oracle`: 569 OK, 1 skipped.
- Static checks: `lint_concurrency.py --werror --cycles=core game-server/src` 3,894 files, 0 errors / warnings / advisories;
  `chunks.py check` 71 chunks, 0 problems; `census.py --self-check` 0 failures.
## M5g party gate (lane B, 2026-10-05): `gs.scenario.m5g` (m5g-plan.md G-01, H-01..H-03, §10.1-§10.4)

`TEST(M5gScenario, Run)` in `M5gScenarioTest.cpp`. The output goes to `<bin>/scenario/m5g`, with the schema pair `aion_{gs,ls}_test_m5g_<hash>`
and the allow-list `m5g_partial_allowlist.txt` (`AION_SCENARIO_M5G_PARTIAL_ALLOWLIST`, a copy of M5e's: the two startup partials).
- Labels: `scenario;realdata`. TIMEOUT 1800. Gate slot 2. No geo variant (m5g-plan.md D12).
- Four accounts online at once: an Elyos Warrior A (the leader), a Mage B, a Priest C and a Scout D beside Poeta's sparkies (210663).
- The profile is `game-server/config/m5g.properties.example` (I-04). The gate passes M5e's keys, `rates.drop = 1000000`, `rates.xp.group =
  "1.5, 3.0"` and the group and alliance `removetime` at 5 s (GP20's offline timeout in one run).
- The oracle is `oracle.py m5g-team` (H-01, `tools/oracle/m5g/team.py`, `tests/test_m5g.py`): the D8 levels (A = B = 3, C = 4, searched so
  that C's share differs from A's and nobody is capped), the exact shares of the three kills, and the loot, team, event, command, message and
  question constants.
- The decoders are `decoders/TeamDecoders.{h,cpp}` with `TeamDecodersTest.cpp` (H-02), each from the Java writeImpl (D9). H-03 is inside the
  gate (`drainAll`, `until` over the four clients).

This is test infrastructure only, with no Java counterpart. In every row, the gate follows what Java does.

| Area | As built | Reason |
|---|---|---|
| Rows scripted | GP1-GP4, GP5b, GP6 (the BUFF slot of 8998), GP8's move half, GP9-GP17, GP17b, GP18-GP23 | §10.2 |
| Rows not scripted | GP5 and GP5c (party chat and its flood rule: `CM_CHAT_MESSAGE_PUBLIC`, `canChat`, `PlayerChatService` are lane A's), GP6b (the aura arm), GP7 (a group buff), GP11b, GP13b (the recall accept), GP15b (a quest share accepted: `checkStartConditions` reads the database in the unit fixture and the gate has no quest of a fitting level beside the sparkies). Their ports are covered by unit tests (`PartyPacketsLzTest`, `RecallServiceTest`) | open, m5g-plan.md §15 |
| The cube | Every character is seeded with `npc_expands = 5`. A, B and C loot three corpses of ten entries before C13, and A's cube of 27 slots filled: the roll winner got `STR_MSG_DICE_INVEN_ERROR`, the corpse kept two entries and its `DropNpc` held A and B at the stop (two Player leaks) | measured, run 3 |
| C13 (GP13, GP14) | The default quality rules (`{0, 2, 2, 2, 2, 2}`); C stands beyond 100 m. The first roll entry is rolled by A and B (the winner is the strictly greater roll, A's on a tie), the second is left to `setPlayersInRoll`'s 17 s pass, measured from the `CM_LOOT_ITEM` that opened the roll (the drain after the prompt made it 16.45 s once), every later one is passed by both. An entry nobody won is free for all: the looter takes it, and the corpse must be deleted at the end | measured, run 2 |
| C19b (GP17b) | B, a Mage seeded with 1 HP, has regenerated a few HP when the case starts (6 of 194 measured). One `CM_ATTACK` did not kill him; the case now fights with `fightUntil` as M5e's X13, up to four sparkies, until B dies | measured, run 2 |

**Runs** (build/msvc, Debug, 2026-10-05): the first runs failed on the rows above (C13's cube and timing, C19b's single attack, and
C20-C23 after them); run 4 passed in 271.8 s with an empty census, no ERROR line, the two startup partials of the allow-list, and
`PlayerGroup`, `PlayerGroupMember`, `PlayerGroupInvite`, `GroupRecruitment`, `Player` and `DropNpc` at 0 live.

## M5g alliance gate (lane B, 2026-10-05): `gs.scenario.m5g_alliance` (m5g-plan.md §16.3 item 7, the alliance part of §10.5)

`TEST(M5gAllianceScenario, Run)` in `M5gAllianceScenarioTest.cpp`, generated once from `M5gScenarioTest.cpp`: its scaffolding (helpers, C0,
C1), its kill and loot helpers (`killNearest`, `teamKill`, `lootEmpty`) and its reports, with the alliance cases in between. Output
`<bin>/scenario/m5g_alliance`, schema prefix `m5ga`, the M5g allow-list, gate slot 2 with `gs.scenario.m5g`, TIMEOUT 1800. The decoders are
`decoders/AllianceDecoders.{h,cpp}` (`SM_ALLIANCE_INFO`, `SM_ALLIANCE_MEMBER_INFO`, `SM_ALLIANCE_READY_CHECK`, each from the Java writeImpl)
with `AllianceDecodersTest.cpp`.

| Area | As built | Reason |
|---|---|---|
| Cases | GA0 (A and B group), GA1 (A invites C: the group dissolves, all three in group 1000), GA2 (B to 1001), GA3 (C vice captain, C invites D), GA4 (the ready-check sequence), GA5 (B's buff: UPDATE_EFFECTS to the others; A's group data to group 1000 only), GA6 (the oracle's four-member share; the corpse looted empty), GA10 (B's timeout, C's leave, D's leave disbands), GA11a (a live alliance at the stop), GA11 (reports) | §10.5 without the league and group-instance rows |
| Not scripted | GA5's alliance chat (lane A's `CM_CHAT_MESSAGE_PUBLIC`), GA7-GA9 (leagues), GA12-GA14 (the group instance) | the league lane and stage 3 |
| The kill helpers | `teamKill` no longer requires D to get no experience, and `lootEmpty`'s party-notice and kinah checks cover all four clients | D is an alliance member here |
| GA6's oracle | a second `m5g-team` call with the four members' levels and one kill of all four in range | the parties' C0 answers three members |
| `MEMBER_GROUP_CHANGE` | told from `JOIN` (both id 5) by the body: the name and nothing after it | SM_ALLIANCE_MEMBER_INFO.java's switch |

**Runs** (build/msvc, Debug, 2026-10-05, branch `lane-b/m5g-alliances`): `gs.scenario.m5g` passed in 275.1 s and `gs.scenario.m5g_alliance`
in 141.9 s in one ctest under the lock (an earlier attempt failed before starting: the ctest listing collided with a concurrent test-target
build). The alliance run: an empty census, lockdep, watchdog and unported trace, no ERROR line; `PlayerAlliance` 2 created, `PlayerAllianceGroup`
8, `PlayerAllianceMember` 8, `PlayerAllianceInvite` 3, all 0 live at the stop. **Mutation proof** (each mutant in the server source, `aion_game_server` rebuilt, the gate under the lock, the source restored): the
inviter's group not dissolved - killed (GA1, then GA3, GA11a, GA11); the group move without its MEMBER_GROUP_CHANGE - killed (GA2 only); the
ready check's START counting the starter - killed (GA4 only); `disband` without the group breaker - killed (GA11 only: `PlayerAllianceGroup`
live).

### GA7-GA9 (the league lane, 2026-10-05, branch `lane-b/m5g-leagues`)

The league cases of §10.5 join the alliance gate: after GA6, C and D leave and form alliance 2; A invites C to a league (GA7: both
alliances see the league of two, positions 0 and 1, and the league's rules FREEFORALL 0 0 2 2 2 2 2); A moves the alliances and back and
makes B his alliance's leader (GA8: the position messages, `STR_UNION_CHANGE_LEADER_TIMEOUT` to every member but B); B expels alliance 2
(GA9: `LEAGUE_EXPELLED` to it, `LEAGUE_EXPEL` and the league of one's `LEAGUE_DISPERSED` to his). GA10 becomes B's timeout disbanding
alliance 1 and C's leave disbanding alliance 2; the reports add `League`, `LeagueMember` and `LeagueInviteEvent` at 0 live. The
`SM_ALLIANCE_INFO` decoder now keeps the league block (positions, alliance ids, captains) and its test covers it.

| Area | As built | Reason |
|---|---|---|
| GA8's move | moved and moved back | a leader's alliance off position 0 makes a later `reorganize` throw, in Java too (docs/deviations/P5-10d.md, proposed correction 2) |
| GA8's leader change | the alliance leader's (17), not the league's (32) | 32 for the league leader's own alliance throws (proposed correction 3); §10.5's GA8 names 17 |

**Runs**: `gs.scenario.m5g_alliance` with GA7-GA9 passed in 140.1 s (one ctest with `gs.scenario.m5g`). In the same ctest
`gs.scenario.m5g` failed once at C12: B got no experience from kill 3 (GP11), which every earlier run counted. The re-run passed in
258.1 s. Nothing on the branch touches the party reward path; the cause (B dead or out of range at that kill) is open. **Mutation proof**: `LeagueMoveEvent` without its position messages - killed (GA8 only); an expel that leaves the league of one alive -
killed (GA9, and GA11: `League` live at the stop).
