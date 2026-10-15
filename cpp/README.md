# Aion 4.8 server emulator: C++ port

A C++23 port of this repository's Java server (Beyond Aion 4.8). The Java code in the parent directory stays the reference implementation.
Status and roadmap: [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md). Coding rules: [docs/CONVENTIONS.md](docs/CONVENTIONS.md).
Intentional differences from Java: [docs/DEVIATIONS.md](docs/DEVIATIONS.md).

| Module | Status |
|---|---|
| commons | done: ported, reviewed; 402 tests (incl. database integration tests) |
| login-server | done: ported, reviewed; 149 tests; a real 4.8 client logs in |
| chat-server | not started |
| game-server | foundation (phase 4) done: milestone M4 passed (tag `milestone-m4`): config, database, IDFactory, all static data, geo and the world load with counts equal to the independent oracles (`gs.m4.check_static_data`). Runtime kernel, generators/lint/oracles in `tools/`, spine frozen (tag `spine-v1`), then the ported configs, geo, base utilities, templates and holders, world, game objects, controllers, player and item models, 56 DAOs, network and the 239 server packets (6 sites wait for phase 5 headers; the full CTest run of the repository: 1,937 tests). Next: phase 5 systems (stats, skills, AI, quests, services). Status: [phase 4](docs/design/phase4-status.md), [design](docs/design/README.md) |

## Requirements (Windows)

- Windows 10 1903 / Windows Server 2022 or newer (the `std::chrono` time zone database uses the system ICU)
- Visual Studio 2026 with the "Desktop development with C++" workload. It includes MSVC, CMake and vcpkg.
- Git, used by vcpkg to fetch the package registry.
- Python 3.12 or newer (standard library only) for the generators, lints and oracles in `tools/`. Without it, the build still works but the
  tool tests and drift checks are not registered.

Dependencies (Asio, spdlog, fmt, MariaDB Connector/C, OpenSSL, cpr, nlohmann-json, magic_enum, pugixml, GoogleTest) are declared in `vcpkg.json` and
built automatically on the first configure. That first build takes a while; later ones come from vcpkg's binary cache.
CMake finds vcpkg via `CMAKE_TOOLCHAIN_FILE`, then `$VCPKG_ROOT`, then the copy bundled with the latest Visual Studio installation.

## Building

Visual Studio: *File › Open › Folder...* and pick `cpp/`. The presets from `CMakePresets.json` appear in the toolbar.

Command line (any shell; CMake from Visual Studio or a standalone CMake ≥ 3.28):

```bash
cmake --preset msvc
```

```bash
cmake --build --preset msvc-debug
```

```bash
ctest --preset msvc-debug
```

The build output goes to `build/msvc`, and dependencies are installed once into `vcpkg_installed/` (both are git-ignored).

AddressSanitizer build (MSVC `/fsanitize=address`, Debug configuration; the game server kernel tests must pass here too):

```bash
cmake --preset msvc-asan
cmake --build --preset msvc-asan-debug --target aion_gs_runtime_lifetime_tests
build/msvc-asan/game-server/Debug/aion_gs_runtime_lifetime_tests.exe   # or ctest --preset msvc-asan-debug after building everything
```

The ASan runtime DLLs (`clang_rt.asan_dynamic-x86_64.dll`, `clang_rt.asan_dbg_dynamic-x86_64.dll`) are copied next to every test executable
after linking, so tests run from any shell. vcpkg dependencies are not instrumented. Any other build directory can use ASan with
`-DAION_ASAN=ON`. Game server checked builds (`AION_CHECKED`, design §12.5) are on for Debug and RelWithDebInfo (`-DAION_CHECKED_MODE=ON|OFF`
overrides).

Sources are picked up by directory globs. With the Visual Studio generator, a newly added `.cpp` file is only compiled by the **second** build:
the first build just regenerates the projects. Build twice after adding files.

## Running the login server

Run it from the Java module directory, so it uses the same `./config` and `./log` as the Java server (Visual Studio's debugger is set up
this way already):

```bash
cd ../login-server && ../cpp/build/msvc/login-server/Debug/aion_login_server.exe
```

Config properties can be overridden on the command line, e.g. `-Ddatabase.url=jdbc:mysql://localhost:3306/other_db`. Stop it with Ctrl+C.
For a game server to show up in the client's server list, register it in `aion_ls.gameservers` (id, IP mask, password; the Java game server
defaults to id 1 and password 1234).

## Running the game server

Not playable yet. `aion_game_server` runs the ported part of Java's startup (config, database, IDFactory, static data, zones, geo and the world),
logs "M4 startup sequence complete", shuts down in order and exits with code 0; a function that is not ported yet stops it with a log line
naming the site and exit code 1. Run it from the Java module directory:

```bash
cd ../game-server && ../cpp/build/msvc/game-server/Debug/aion_game_server.exe
```

Game client: see the repository's main README (Aion 4.8 NA client, `version.dll` patch, `start.bat` with
`bin64\aion.bin -ip:127.0.0.1 -port:2106 -loginex`).

## Local database

The servers use the same MySQL/MariaDB schemas as the Java server (`../login-server/sql/aion_ls.sql`, `../chat-server/sql/aion_cs.sql`,
`../game-server/sql/aion_gs.sql`) and the same defaults in `config/network/database.properties`: `localhost:3306`, user `root`, empty password.

On the development machine, a portable MariaDB 11.8 LTS lives in `D:\aion-dev` (outside the repository). It is not a Windows service and only
listens on 127.0.0.1:

- `D:\aion-dev\start-mariadb.bat` starts it in a minimized console window; `D:\aion-dev\stop-mariadb.bat` shuts it down cleanly.
- The databases `aion_ls`, `aion_cs` and `aion_gs` are created from the schema files; `aion_cpp_test` is a scratch database for integration tests.
- Config: `D:\aion-dev\mariadb-data\my.ini` (utf8mb4 with `utf8mb4_general_ci`, so the Java server works against it too). Log: `mariadb-data\mariadb.err`.

Database integration tests are skipped unless these environment variables are set, e.g. in Git Bash:

```bash
AION_TEST_DATABASE_URL="jdbc:mysql://localhost:3306/aion_cpp_test" AION_TEST_DATABASE_USER=root AION_TEST_DATABASE_PASSWORD= ctest --preset msvc-debug
```

The login server tests additionally need `AION_TEST_LS_DATABASE_URL` (e.g. `jdbc:mysql://127.0.0.1:3306/aion_ls_test`), and the game server startup
smoke test `gs.smoke.startup` needs `AION_TEST_GS_DATABASE_URL` (e.g. `jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8`).
Tests that read the Java checkout carry the CTest label `realdata`; `ctest --preset msvc-debug -LE realdata` skips them.

## Milestone tests fail when their prerequisites are missing

The milestone tests — `gs.scenario.m5a` (the M5a gate), `gs.m4.check_static_data`, `gs.smoke.startup` and `gs.smoke.startup_geo` — **fail** when a
prerequisite is missing instead of reporting themselves skipped. CTest counts a skipped test as passed, so a plain `ctest` without the
`AION_TEST_*` database URLs used to report the milestone green without ever running it. The prerequisites are the database URLs above, a Python
3.12 interpreter (the `tools/oracle` oracles), and the Java checkout with its `game-server/data/geo`; each failure message names the variable and
the value to set.

To run the suite without them anyway, opt out explicitly — either for one run (the `cmake -P` driven tests read it at test time):

```bash
AION_GS_ALLOW_MILESTONE_SKIP=1 ctest --preset msvc-debug
```

or for a whole build directory, which is also the only opt-out the scenario gate honours (its requirement is fixed at configure time):

```bash
cmake --preset msvc -DAION_GS_ALLOW_MILESTONE_SKIP=ON
```

The tests are then reported as `Skipped` again, with the same reason in their output. `gs.smoke.startup_geo` is the geo-enabled startup
(label `geo`, about 5 GB and a few minutes in a Debug build): `ctest -L geo` runs only it, `ctest -LE geo` leaves it out.
