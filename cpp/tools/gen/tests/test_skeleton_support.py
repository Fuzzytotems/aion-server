"""Helpers of the skeleton.py tests: fixture paths, golden-file comparison and a throwaway CMake/MSVC compile check."""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
GEN_DIR = HERE.parent
CPP_ROOT = GEN_DIR.parents[1]
REPO_ROOT = CPP_ROOT.parent
FIXTURES = HERE / 'fixtures' / 'skeleton'
FIXTURE_JAVA = FIXTURES / 'java'
FIXTURE_HANDLERS = FIXTURES / 'handlers'
FIXTURE_CPP = FIXTURES / 'cpp'
FIXTURE_FIELDMAP = FIXTURES / 'fieldmap.json'
EXPECTED = FIXTURES / 'expected'
REAL_JAVA_ROOT = REPO_ROOT / 'game-server' / 'src'
REAL_HANDLERS_ROOT = REPO_ROOT / 'game-server' / 'data' / 'handlers'
REAL_CPP_SRC = CPP_ROOT / 'game-server' / 'src'
REAL_GENERATED = CPP_ROOT / 'game-server' / 'generated'
REAL_COMMONS_SRC = CPP_ROOT / 'commons' / 'src'
VCPKG_INCLUDE = CPP_ROOT / 'vcpkg_installed' / 'x64-windows' / 'include'
UPDATE = os.environ.get('AION_SKELETON_UPDATE') == '1'
VS_CMAKE = Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')

# The runtime kernel's TaskKind::CALLBACK (runtime/base/TaskInfo.h) collides with the <windows.h> CALLBACK macro when a TU includes
# windows.h (through spdlog or Asio) before the kernel headers. Whole-tree draft compile checks parse the kernel headers first (reported
# as an open issue of the kernel, not a generator problem).
KERNEL_FIRST = ('aion/gameserver/runtime/base/TaskInfo.h', 'aion/gameserver/runtime/sched/PinnedCallback.h')


def short_temp_dir(prefix):
    """A temp directory with a short path (MSBuild and Windows MAX_PATH limits)."""
    base = os.environ.get('AION_SKELETON_TMP') or tempfile.gettempdir()
    if len(base) > 60:
        base = 'C:/Temp' if os.name == 'nt' else '/tmp'
    Path(base).mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix=prefix, dir=base))


def find_cmake():
    env = os.environ.get('AION_CMAKE')
    if env and Path(env).is_file():
        return env
    if VS_CMAKE.is_file():
        return str(VS_CMAKE)
    return shutil.which('cmake')


def write_tree(root, files):
    for rel, text in files.items():
        p = Path(root) / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(text.encode('utf-8'))


def compile_check(workdir, include_dirs, sources, forced_includes=()):
    """Configures and builds a static library from `sources` (paths) with the project's MSVC flags plus /WX.

    Returns (ok, output). Nothing is linked, so declared-only functions are fine."""
    cmake = find_cmake()
    if cmake is None:
        raise RuntimeError('cmake not found (set AION_CMAKE)')
    workdir = Path(workdir)
    src_dir = workdir / 'proj'
    build_dir = workdir / 'b'
    src_dir.mkdir(parents=True, exist_ok=True)
    inc = '\n'.join(f'\t"{Path(d).as_posix()}"' for d in include_dirs)
    srcs = '\n'.join(f'\t"{Path(s).as_posix()}"' for s in sources)
    forced = ' '.join(f'"/FI{f}"' for f in forced_includes)
    (src_dir / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.28)
project(skeleton_check LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(skeleton_check STATIC
{srcs}
)
target_include_directories(skeleton_check PRIVATE
{inc}
)
if(MSVC)
	target_compile_options(skeleton_check PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /MP /wd4100 /bigobj {forced})
	target_compile_definitions(skeleton_check PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOGDI NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(skeleton_check PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()
target_compile_definitions(skeleton_check PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED SPDLOG_FMT_EXTERNAL AION_CHECKED=1)
''', encoding='utf-8')
    out = []
    for cmd in ([cmake, '-S', str(src_dir), '-B', str(build_dir)], [cmake, '--build', str(build_dir), '--config', 'Debug']):
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
        out.append(r.stdout)
        if r.returncode != 0:
            return False, '\n'.join(out)
    return True, '\n'.join(out)


def warnings_in(output):
    """MSVC warning/error lines of a build log."""
    return [line for line in output.splitlines() if (': warning ' in line or ': error ' in line) and 'cmake' not in line.lower()[:10]]
