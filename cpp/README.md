# Aion 4.8 server emulator: C++ port

A C++23 port of this repository's Java server (Beyond Aion 4.8). The Java code in the parent directory stays the reference implementation.
Status and roadmap: [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md). Coding rules: [docs/CONVENTIONS.md](docs/CONVENTIONS.md).
Intentional differences from Java: [docs/DEVIATIONS.md](docs/DEVIATIONS.md).

| Module | Status |
|---|---|
| commons | done: ported, reviewed; 394 tests (incl. database integration tests) |
| login-server | done: ported, reviewed; 147 tests; a real 4.8 client logs in |
| chat-server | not started |
| game-server | in progress: design done ([docs/design](docs/design/README.md)); runtime kernel done (422 tests, 30-minute ASan and checked stress gates, benchmark passed) |

## Requirements (Windows)

- Windows 10 1903 / Windows Server 2022 or newer (the `std::chrono` time zone database uses the system ICU)
- Visual Studio 2026 with the "Desktop development with C++" workload. It includes MSVC, CMake and vcpkg.
- Git, used by vcpkg to fetch the package registry.

Dependencies (Asio, spdlog, fmt, MariaDB Connector/C, OpenSSL, cpr, nlohmann-json, magic_enum, GoogleTest) are declared in `vcpkg.json` and
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

Game client: see the repository's main README (Aion 4.8 NA client, `version.dll` patch, `start.bat` with
`bin64ion.bin -ip:127.0.0.1 -port:2106 -loginex`).

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
