---
name: run-aion-cpp
description: Build, run, test, drive, probe and stop the C++ port of the Aion 4.8 servers (cpp/ - aion_login_server, aion_game_server, aion_chat_server) on Windows. Use when asked to build the C++ servers, start or stop the login/game server, probe or drive the running servers, read their logs, run the unit tests or a scenario gate (gs.scenario.m5a, ...), or run one gtest executable.
---

The C++ servers are Windows console programs built with CMake + MSVC into `cpp/build/msvc` (Debug). An agent drives them through
**`cpp/.claude/skills/run-aion-cpp/driver.ps1`** (Windows PowerShell 5.1): `status`, `build`, `start`, `probe`, `logs`, `stop`,
`unit`, `gate`. The real end-to-end driver of the game is a scenario gate (a fake client logs in, creates a character, enters the
world, ...): `driver.ps1 gate m5a`.

**Safety.** The owner plays on this machine with a separate kit (`D:\aion-dev\play`) on the same ports (2106, 9014, 7777) and the
live schemas `aion_ls`/`aion_gs`. Never touch its processes, never kill processes by name, never write to the live schemas yourself.
`start` refuses when a port is taken and says by whom; `stop` only sends Ctrl+C to the PIDs `start` recorded.

## Prerequisites

Visual Studio 2026 with MSVC, and the CMake, CTest and vcpkg that come with it (they are not on PATH). You also need Python 3.12,
the portable MariaDB, and the Java tree's data:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property catalog_productDisplayVersion
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
Get-ChildItem "$vs\VC\Tools\MSVC" -Name
& "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --version | Select-Object -First 1
& "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --version | Select-Object -First 1
& "$vs\VC\vcpkg\vcpkg.exe" version | Select-Object -First 1
py -3.12 --version
& 'D:\aion-dev\mariadb-11.8.9-winx64\bin\mariadb-admin.exe' --defaults-file=D:\aion-dev\mariadb-data\my.ini -u root ping
(Get-ChildItem D:\aion-server\game-server\data\geo -Filter *.geo).Count; Test-Path D:\aion-server\game-server\data\static_data, D:\aion-server\game-server\config\mygs.properties
# -> 18.9.1 / 14.50.35717 14.51.36231 / cmake 4.3.1-msvc1 / ctest 4.3.1-msvc1 / vcpkg 2026-05-27 / Python 3.12.10 / mysqld is alive / 151 / True True
```

If MariaDB is down, `D:\aion-dev\start-mariadb.bat` starts it. `driver.ps1 start` runs that script itself when port 3306 is closed.
That branch has not been tested: MariaDB was already up.

## Setup

On this machine the schemas already exist. To create them from the SQL files, use this command. It was tested on throwaway
names, which were then dropped. **Never run it against `aion_ls`/`aion_gs`/`aion_cs`:**

```powershell
$m = 'D:\aion-dev\mariadb-11.8.9-winx64\bin\mariadb.exe'; $ini = '--defaults-file=D:\aion-dev\mariadb-data\my.ini'
foreach ($s in 'ls', 'gs', 'cs') {
  $db = "aion_skill_tmp_$s"; $sql = "D:/aion-server/$(@{ls='login-server';gs='game-server';cs='chat-server'}[$s])/sql/aion_$s.sql"
  & $m $ini -u root -e "CREATE DATABASE $db"
  & $m $ini -u root $db -e "source $sql"
  & $m $ini -u root -N -e "SELECT '$db', COUNT(*) FROM information_schema.tables WHERE table_schema = '$db'"
}
& $m $ini -u root aion_skill_tmp_ls -e "INSERT INTO gameservers (id, mask, password) VALUES (1, '127.0.0.1', '1234'); SELECT id, mask, password FROM gameservers"
foreach ($s in 'ls', 'gs', 'cs') { & $m $ini -u root -e "DROP DATABASE aion_skill_tmp_$s" }
# -> 8, 61 and 1 tables; the gameservers row (id 1 / 1234) is what lets the game server register with the login server
```

The tests need three empty databases, which they fill themselves:

```powershell
$m = 'D:\aion-dev\mariadb-11.8.9-winx64\bin\mariadb.exe'; & $m --defaults-file=D:\aion-dev\mariadb-data\my.ini -u root -e "CREATE DATABASE IF NOT EXISTS aion_cpp_test; CREATE DATABASE IF NOT EXISTS aion_ls_test; CREATE DATABASE IF NOT EXISTS aion_cs_test; SHOW WARNINGS"
# -> when they exist already: "Note 1007 Can't create database 'aion_cs_test'; database exists" (SHOW WARNINGS covers only the last statement)
```

`unit` and `gate` set the test environment themselves:

| Variable | Value |
|---|---|
| `AION_TEST_DATABASE_URL` | `jdbc:mysql://localhost:3306/aion_cpp_test` |
| `AION_TEST_DATABASE_USER` / `_PASSWORD` | `root` / empty |
| `AION_TEST_LS_DATABASE_URL` | `jdbc:mysql://127.0.0.1:3306/aion_ls_test` |
| `AION_TEST_GS_DATABASE_URL` | `jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8` |
| `AION_TEST_CS_DATABASE_URL` | `jdbc:mysql://127.0.0.1:3306/aion_cs_test` |

## Build

```powershell
$d = 'D:\aion-server\cpp\.claude\skills\run-aion-cpp\driver.ps1'
powershell -NoProfile -ExecutionPolicy Bypass -File $d build
powershell -NoProfile -ExecutionPolicy Bypass -File $d build -Targets aion_loginserver_crypto_tests,aion_gs_geomath_tests
```

The driver checks `build/msvc`'s CMake cache. It configures with `cmake --preset msvc -DAION_BUILD_CHAT_SERVER=ON` when the cache is
missing or has the chat server OFF, then builds Debug with `--parallel 4 -- -p:CL_MPCount=2 -nr:false`. The default targets are
`aion_game_server,aion_login_server`. An up-to-date build takes 4-10 s. The driver refuses to build while any process runs from
`build/msvc` (its binaries would be locked) or while another MSBuild or cmake works on `build/msvc`. Build only once at a time:
other agents build in private `cpp/build/*` dirs.

## Run (agent path)

This is the full cycle, as it was run:

```powershell
$d = 'D:\aion-server\cpp\.claude\skills\run-aion-cpp\driver.ps1'
powershell -NoProfile -ExecutionPolicy Bypass -File $d status
powershell -NoProfile -ExecutionPolicy Bypass -File $d start
powershell -NoProfile -ExecutionPolicy Bypass -File $d probe
powershell -NoProfile -ExecutionPolicy Bypass -File $d logs -Tail 3
powershell -NoProfile -ExecutionPolicy Bypass -File $d logs -Errors
powershell -NoProfile -ExecutionPolicy Bypass -File $d logs -Server ls -Tail 6
powershell -NoProfile -ExecutionPolicy Bypass -File $d stop
```

What you should see:

- `start`: the login server listens on 2106 and 9014 in about 2 s. Then the game server prints `READY: ... Game server started in 145 seconds.`
  (145-148 s with geodata, 0 ERROR and 67 WARN lines).
- `probe`: `2106 ... first packet 202 bytes ...: CA 00 ...` (SM_INIT, encrypted) and `7777 ... first packet 11 bytes ...: 0B 00 C8 01 ...` (SM_KEY).
- `logs`: a header line with the ERROR/WARN counts, then the tail. `-Errors` prints `(no ERROR lines)` on a clean start. The login server's
  log shows `Gameserver #1 is now online` and the probe's `Connection attempt from: 127.0.0.1`.
- `stop`: `gs stopped after 3.1 s, exit code 0`, then the same for `ls`.

The servers keep running after the tool call returns, in their own minimized console windows. `start` reads the Java tree's configs,
with working directories `D:\aion-server\login-server` and `D:\aion-server\game-server`, including the owner's
`config\mygs.properties`.

| command | what it does |
|---|---|
| `status` | owners of 2106/9014/7777/3306, MariaDB ping, every `aion_*` process and where it runs from (build/msvc, PLAY KIT, other) |
| `build [-Targets a,b]` | configure if needed, then build (see Build) |
| `start [-TimeoutSec 420]` | refuses if a port is taken; starts MariaDB if down, then the login server, waits for 2106/9014, then the game server, waits for "Game server started"; PIDs go to `cpp\build\msvc\.run-aion-cpp\servers.json` |
| `probe [-Force]` | TCP connects to 2106 and 7777 and prints each server's first packet; it will not probe the play kit's servers |
| `logs [-Server gs\|ls] [-Errors] [-Tail n]` | tail of `game-server\log\server_console.log` or `login-server\log\server_console.log`, with ERROR/WARN counts |
| `stop` | Ctrl+C (through `send-ctrl-c.ps1`) to the recorded PIDs, game server first, and waits up to 180 s; never kills |
| `unit [-Filter re] [-Jobs 4] [-Force]` | `ctest -LE "scenario\|geo\|m4\|nightly\|stress\|smoke"` in build/msvc with the test env |
| `gate <name> [-Force]` | `ctest -R ^gs\.(scenario\|smoke)\.<name>$`; prints the verdict, the time, the output directory, the census and the summary |

From Git Bash the same works: `powershell -NoProfile -ExecutionPolicy Bypass -File 'D:\aion-server\cpp\.claude\skills\run-aion-cpp\driver.ps1' status`.

## Direct invocation (one gtest executable)

Most changes here touch internals. To test one, build only its target (`build -Targets <target>`, above), then run the executable
from `build\msvc\<module>\Debug\`. The target name is the executable's name without `.exe`. A game-server library `aion_gs_<x>` has its
tests in `aion_gs_<x>_tests` (cpp/game-server/cmake/AionChunks.cmake); the others are declared by `aion_add_tests(<target> <dir>)` in
`cpp/<module>/CMakeLists.txt`. To list the executables that exist:

```powershell
Get-ChildItem D:\aion-server\cpp\build\msvc -Recurse -Filter '*_tests.exe' | Where-Object { $_.DirectoryName -like '*\Debug' } | ForEach-Object { $_.FullName }
# -> 62 paths, e.g. ...\login-server\Debug\aion_loginserver_crypto_tests.exe, ...\game-server\Debug\aion_gs_geomath_tests.exe
```

```bash
cd /d/aion-server/cpp/build/msvc/login-server/Debug && ./aion_loginserver_crypto_tests.exe --gtest_list_tests | head -30
```

```powershell
& D:\aion-server\cpp\build\msvc\login-server\Debug\aion_loginserver_crypto_tests.exe --gtest_filter='BlowfishCipherTest.*'
# -> [  PASSED  ] 11 tests.
```

Tests that need a database skip themselves unless the `AION_TEST_*` variables above are set, and the executable still exits 0
(for `BannedIpDAOTest.*`: `[  PASSED  ] 0 tests.`, then `[  SKIPPED ] 3 tests`). Run them through `unit -Filter`, or set the variables in your shell first
(`aion_loginserver_data_tests` holds the login server's DAO tests):

```powershell
$env:AION_TEST_DATABASE_USER = 'root'; $env:AION_TEST_LS_DATABASE_URL = 'jdbc:mysql://127.0.0.1:3306/aion_ls_test'
& D:\aion-server\cpp\build\msvc\login-server\Debug\aion_loginserver_data_tests.exe --gtest_filter='BannedIpDAOTest.*' | Select-Object -Last 3
# -> DatabasePool log lines (stderr, not filtered), then ... [  PASSED  ] 3 tests.
```

## Run (human path)

This was not run in this session, because it needs the owner's client and account. Start the servers with `driver.ps1 start` (or the
owner's `D:\aion-dev\play\start-servers.bat`). Then run `C:\Aion\start.bat`, which runs
`bin64\aion.bin -ip:127.0.0.1 -port:2106 -loginex` with the Beyond Aion `version.dll`. Stop with `driver.ps1 stop`.

## Test

```powershell
$d = 'D:\aion-server\cpp\.claude\skills\run-aion-cpp\driver.ps1'
powershell -NoProfile -ExecutionPolicy Bypass -File $d unit -Filter '^BlowfishCipherTest\.'
powershell -NoProfile -ExecutionPolicy Bypass -File $d unit -Filter '^(BannedIpDAOTest|AccountDAOTest)\.'
powershell -NoProfile -ExecutionPolicy Bypass -File $d gate m5a
powershell -NoProfile -ExecutionPolicy Bypass -File $d gate startup
# -> 11/11 in 0.5 s; 12/12 in 10 s (database); gate m5a: PASSED in 55-59 s, census clean; gate startup: PASSED in 29 s
```

Measured times: `m5a` 54.6-58.7 s and `startup` 29.1-29.3 s. The other gates were not run here. Their times alone, from
cpp/docs/design/m5c-plan.md §22.5: `m5a_geo` 157 s, `m5b` 223 s, `m5b_geo` 333 s, `m5b2` 165 s, `m5b2_geo` 292 s, `m5b3` 142 s,
`m5b3_geo` 326 s, `m5c` 289 s, `startup_geo` 152 s, `startup_progress` 33 s.

`unit` without `-Filter` runs the whole suite. It was not run here. The same document (§22.4) records 4,299 tests in 995 s at
`-j 6`, 0 failed; the driver defaults to `-j 4`.

A gate's output goes to `cpp\build\msvc\game-server\scenario\Debug\<gate>\`: `game_server.log`, `login_server.log`, `gs_log\`,
`check\census.txt` (a header line alone means clean) and `check\*_summary.txt`. A smoke gate writes to
`...\game-server\gs.smoke.<name>\Debug\`. Gates start their own servers on ephemeral ports with their own schemas, so they can run
while `start`'s servers are up.

## Gotchas

- **After a `--`, Windows PowerShell 5.1 splits `-p:Name=Value` for native programs** (into `-p:` and `Name=Value`; without the `--`
  it does not). So `cmake --build ... -- -p:CL_MPCount=2 -nr:false` fails with `MSB1005: Specify a property and its value`. Quote
  each switch (`'-p:CL_MPCount=2' '-nr:false'`); the driver does.
- **A `ctest -R` that matches nothing exits 0.** "No tests were found!!!" goes to stderr, so a typo looks green. The driver passes
  `--no-tests=error` and also requires the `N% tests passed` line.
- **`-R gs.scenario.m5a` also matches `m5a_geo` and `m5a_stress`.** `gate` anchors the regex. It refuses stress runs, which need the
  owner's go-ahead.
- **Never run two ctest processes in one build directory.** They corrupt its GoogleTest discovery files
  (cpp/docs/design/m5b3-plan.md §19.2, "Process note", which also records the repair). `unit` and `gate` refuse while another ctest names `build\msvc` or has no `--test-dir`. Use `-Force` only
  when that ctest certainly runs in another directory.
- **An old cache keeps `AION_BUILD_CHAT_SERVER=OFF`.** The default became ON in 27726d32c, but `option()` never overrides a cached
  value. `build` reconfigures with `-DAION_BUILD_CHAT_SERVER=ON`.
- **`start` uses the live schemas and rotates the owner's logs.** It reads the same configs as the play kit, so do experiments in
  gates, which use their own schemas. Each game server start zips the previous run's logs, including a play session's, into
  `game-server\log\archived\<from> to <to>.zip`.
- **Database edits made while a client is connected are lost.** The game server loads an account's characters when the client
  connects and saves them at logout (`D:\aion-dev\play\db.ps1`, found 2026-09-25). Close the client completely first.
- **A new `.cpp` is compiled only by the second build.** Sources are globbed and the VS generator regenerates first (cpp/README.md).
- **Python writes CRLF on Windows in text mode, but cpp/ is LF.** `cpp/.gitattributes` enforces LF and the drift tests compare bytes.
  Use `open(..., newline='')` or the Write/Edit tools.
- **Wall-clock jobs can hit a gate.** LegionDominion's hard-coded Wednesday 09:00 cron can hit a gate that is running at that time
  (cpp/docs/design/m5c-plan.md G-07). Rerun the gate.
- **tools.gen's compile tests in a worktree** take vcpkg's include directory from `AION_VCPKG_INCLUDE`, which the tools' CTest
  entries set from the build's `VCPKG_INSTALLED_DIR` (PR #2). Run them through ctest, or set it yourself when you run pytest
  directly in a worktree that has no `cpp/vcpkg_installed`.
- **A play kit copied from build/msvc holds build/msvc's server PDBs open.** Its binaries name the PDBs of build/msvc, and the
  stack-trace symbolizer opens them, so while the kit runs, linking `aion_game_server` or `aion_login_server` in build/msvc fails with
  `LNK1201` (found 2026-09-28). `status` and `build` only see processes that run *from* build/msvc. Never stop the kit: wait for the
  owner to log out, or link the server alone with `'-p:BuildProjectReferences=false'` and `_LINK_=/PDB:<name>_alt.pdb` in the
  environment, and delete that PDB afterwards.

## Troubleshooting

- **`driver: port 2106 (login server, clients) is taken by pid N ... [build/msvc]. Refusing to start.`**: something already listens.
  `status` shows who. If it is the PLAY KIT, the owner is playing: wait. Never stop it.
- **`servers started by this driver are still running (ls, gs); run driver.ps1 stop first`**: run `stop`.
- **`processes run from build/msvc and lock its binaries: aion_game_server (pid ...)`**: `stop` the driver's servers, or wait for the
  gate that is running.
- **`LNK1201: error writing to program database '...ion_game_server.pdb'`**: the play kit (or another copy of the servers) runs
  and holds the PDB open. See Gotchas; never stop the kit.
- **`a ctest runs that may be in build/msvc (pid ...)`**: wait for it to finish (or use `-Force` if it is another directory).
- **`MSBUILD : error MSB1005: Specify a property and its value`**: MSBuild switches were passed unquoted from PowerShell (see Gotchas).
- **`no unit test matches '<re>'` / `no gate '<name>'. Known: m5a, m5a_geo, ...`**: ctest test names are `Suite.Case`
  (`--gtest_list_tests` shows them). Gate names are the part after `gs.scenario.` or `gs.smoke.`.
