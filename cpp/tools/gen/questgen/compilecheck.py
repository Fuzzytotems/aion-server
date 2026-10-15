"""compilecheck: the compile check of the transliterated quest handlers (phase6-questgen-prototype.md §8.1, phase6-transliterator.md §3).

    python -m tools.gen.questgen.compilecheck --stage DIR [--only FILE...] [--jobs N] [--json OUT] [--regscan]
                                              [--prototype-rules | --p6t-rules]

Exit 0 when every emitted file compiles (warnings are counted, not fatal), 1 when one does not.

It transliterates the Java quest handlers (as `--dry-run --emit DIR/src` would), then compiles every emitted file **on its own** with the
flags of a Q chunk's Debug build (cl /c /W4 /permissive- /std:c++latest ..., the include roots of `aion_gs_handlers_quest_*`, the quest
prelude as a precompiled header), and reports, per file, whether it compiles clean, compiles with warnings, or fails, with the first
diagnostics. With `--regscan` it also builds `game-server/tools/regscan` into DIR and runs it over the staged tree with the Java
cross-check (markers, namespaces, file rules, the quest id against Java's `super(...)`).

Nothing is written into the repository and nothing is linked: the check proves the emitted C++ is well-formed and typed against today's
headers, not that it runs (that is the golden-trace harness, tests/quest_handlers_golden). DIR must lie outside the repository. The
compiles run in a pool of N `cl` processes (default 4; the machine is shared). MSVC comes from vcvars64.bat (AION_VCVARS overrides it);
vcpkg's include directory from AION_VCPKG_INCLUDE, else the tree's or the main checkout's vcpkg_installed.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import cli, emit, paths

VCVARS_CANDIDATES = (
    Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat'),
    Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'),
)

# the Debug flags of a Q chunk (build/msvc/game-server/aion_gs_handlers_quest_q05.vcxproj: aion_compiler_options plus CMake's Debug
# defaults); /W4 without /WX, so warnings are counted separately from errors
CL_FLAGS = (
    '/nologo', '/c', '/std:c++latest', '/permissive-', '/utf-8', '/Zc:__cplusplus', '/Zc:preprocessor', '/EHsc', '/bigobj',
    '/W4', '/wd4100', '/wd4251', '/wd4275', '/external:W0', '/Od', '/MDd', '/RTC1',
    '/DWIN32', '/D_DEBUG', '/D_WINDOWS', '/DFMT_SHARED', '/DSPDLOG_SHARED_LIB', '/DSPDLOG_COMPILED_LIB', '/DSPDLOG_FMT_EXTERNAL',
    '/D_WIN32_WINNT=0x0A00', '/DWIN32_LEAN_AND_MEAN', '/DNOGDI', '/DNOMINMAX', '/D_CRT_SECURE_NO_WARNINGS', '/DASIO_STANDALONE',
    '/DASIO_NO_DEPRECATED', '/DAION_CHECKED=1',
)
DIAG = re.compile(r'^(?P<file>.+?)\((?P<line>\d+)(?:,\d+)?\)\s*:\s*(?P<kind>fatal error|error|warning)\s+(?P<code>[A-Z]+\d+)\s*:'
                  r'\s*(?P<msg>.*)$')


def find_vcvars():
    env = os.environ.get('AION_VCVARS')
    if env:
        return Path(env)
    for p in VCVARS_CANDIDATES:
        if p.is_file():
            return p
    raise SystemExit('compilecheck: vcvars64.bat not found (set AION_VCVARS)')


def msvc_env(stage):
    """The environment vcvars64.bat sets up, cached in the stage directory."""
    cache = Path(stage) / 'msvc-env.json'
    if cache.is_file():
        return json.loads(cache.read_text(encoding='utf-8'))
    vcvars = find_vcvars()
    out = subprocess.run(f'"{vcvars}" >nul && set', capture_output=True, text=True, shell=True, check=True).stdout
    env = {}
    for line in out.splitlines():
        k, sep, v = line.partition('=')
        if sep and k:
            env[k] = v
    if 'INCLUDE' not in env:
        raise SystemExit(f'compilecheck: {vcvars} did not set INCLUDE')
    cache.write_text(json.dumps(env), encoding='utf-8')
    return env


def cl_exe(env):
    """cl.exe by full path: CreateProcess searches the parent's PATH, not the one passed in env."""
    path = next((v for k, v in env.items() if k.upper() == 'PATH'), '')
    exe = shutil.which('cl', path=path)
    if exe is None:
        raise SystemExit('compilecheck: cl.exe is not on the vcvars PATH')
    return exe


def vcpkg_include():
    env = os.environ.get('AION_VCPKG_INCLUDE')
    if env:
        return Path(env)
    # a git worktree under <main>/.claude/worktrees/<name>: the main checkout's vcpkg_installed
    for parent in (paths.REPO_ROOT, *paths.REPO_ROOT.parents):
        p = parent / 'cpp' / 'vcpkg_installed' / 'x64-windows' / 'include'
        if p.is_dir():
            return p
    raise SystemExit('compilecheck: no vcpkg include directory (set AION_VCPKG_INCLUDE)')


def include_flags(stage):
    gen = Path(stage) / 'buildgen'            # stands in for build/<dir>/generated (aion/BuildInfo.h, not reached by the handlers)
    gen.mkdir(parents=True, exist_ok=True)
    vc = vcpkg_include()
    roots = [paths.CPP_GAME_SERVER / 'handlers', paths.CPP_ROOT / 'commons' / 'src', gen, paths.CPP_GAME_SERVER / 'src',
             paths.CPP_GAME_SERVER / 'generated']
    return [f'/I{r}' for r in roots] + [f'/external:I{vc}', f'/external:I{vc / "mysql"}']


def parse_diagnostics(output):
    diags = []
    for line in output.splitlines():
        m = DIAG.match(line.strip())
        if m:
            diags.append({'file': m['file'], 'line': int(m['line']), 'kind': m['kind'], 'code': m['code'], 'msg': m['msg'].strip()})
    return diags


def build_pch(stage, env, incs):
    pch_dir = Path(stage) / 'pch'
    pch_dir.mkdir(parents=True, exist_ok=True)
    src = pch_dir / 'prelude_pch.cpp'
    src.write_text(f'#include "{paths.QUEST_PRELUDE}"\n', encoding='utf-8')
    pch = pch_dir / 'prelude.pch'
    cmd = [cl_exe(env), *CL_FLAGS, *incs, f'/Yc{paths.QUEST_PRELUDE}', f'/Fp{pch}', f'/Fo{pch_dir / "prelude_pch.obj"}', str(src)]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=pch_dir)
    if p.returncode != 0:
        raise SystemExit(f'compilecheck: the prelude PCH does not compile:\n{p.stdout}{p.stderr}')
    return pch


def compile_one(src, obj, pch, env, incs):
    cmd = [cl_exe(env), *CL_FLAGS, *incs, f'/Yu{paths.QUEST_PRELUDE}', f'/Fp{pch}', f'/Fo{obj}', str(src)]
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=Path(obj).parent)
    out = p.stdout + p.stderr
    diags = parse_diagnostics(out)
    errors = [d for d in diags if d['kind'] != 'warning']
    warnings = [d for d in diags if d['kind'] == 'warning']
    status = 'error' if p.returncode != 0 or errors else ('warning' if warnings else 'clean')
    try:
        Path(obj).unlink()
    except OSError:
        pass
    return {'status': status, 'errors': errors, 'warnings': warnings, 'seconds': round(time.time() - t0, 1),
            'output': '' if status == 'clean' else out[-4000:]}


def build_regscan(stage, env):
    rs = paths.CPP_GAME_SERVER / 'tools' / 'regscan' / 'src'
    out_dir = Path(stage) / 'regscan'
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / 'aion_gs_regscan.exe'
    if exe.is_file():
        return exe
    srcs = sorted(str(p) for p in rs.glob('*.cpp'))
    cmd = [cl_exe(env), '/nologo', '/std:c++latest', '/permissive-', '/utf-8', '/EHsc', '/O2', '/MD', f'/I{rs}', *srcs, f'/Fe{exe}',
           f'/Fo{out_dir}\\']
    p = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=out_dir)
    if p.returncode != 0:
        raise SystemExit(f'compilecheck: aion_gs_regscan does not build:\n{p.stdout[-3000:]}')
    return exe


def run_regscan(stage, env, src_root):
    """Runs aion_gs_regscan over the staged handler tree (plus the tree's QuestPrelude.h) with the Java cross-check."""
    exe = build_regscan(stage, env)
    prelude = paths.CPP_GAME_SERVER / 'handlers' / paths.QUEST_PRELUDE
    staged_prelude = Path(src_root) / paths.QUEST_PRELUDE
    staged_prelude.parent.mkdir(parents=True, exist_ok=True)
    staged_prelude.write_bytes(prelude.read_bytes())
    out_dir = Path(stage) / 'regscan' / 'out'
    out_dir.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([str(exe), '--out', str(out_dir), '--handlers-root', str(src_root), '--java-handlers',
                        str(paths.JAVA_GAME_SERVER / 'data' / 'handlers')], capture_output=True, text=True, env=env)
    out = p.stdout + p.stderr
    errs = [line for line in out.splitlines() if ': error:' in line]
    return {'exitCode': p.returncode, 'errors': errs, 'summary': out.strip().splitlines()[-1:] if out.strip() else []}


def first_error_class(r):
    """A short class for a failed file: the first error's code and a normalised message."""
    if not r['errors']:
        return 'no diagnostic (exit code)'
    d = r['errors'][0]
    msg = re.sub(r"'[^']*'", "'…'", d['msg'])
    return f"{d['code']}: {msg[:90]}"


def main(argv=None):
    ap = argparse.ArgumentParser(prog='python -m tools.gen.questgen.compilecheck', description=__doc__.split('\n\n')[0])
    ap.add_argument('--stage', required=True, metavar='DIR', help='scratch directory outside the repository')
    ap.add_argument('--only', nargs='+', metavar='FILE', help='only these quest files')
    ap.add_argument('--jobs', '-j', type=int, default=4)
    ap.add_argument('--json', metavar='OUT', help='write the per-file results as JSON')
    ap.add_argument('--regscan', action='store_true', help='also build and run aion_gs_regscan over the staged tree')
    ap.add_argument('--prototype-rules', action='store_true', help="the prototype's rules only (rev 2)")
    ap.add_argument('--p6t-rules', action='store_true', help='the P6-T rules only (no G1 rule)')
    args = ap.parse_args(argv)
    stage = Path(args.stage).resolve()
    try:
        stage.relative_to(paths.REPO_ROOT.resolve())
        ap.error(f'--stage {stage} is inside the repository; name a directory outside it')
    except ValueError:
        pass
    stage.mkdir(parents=True, exist_ok=True)
    src_root = stage / 'src'
    t0 = time.time()
    files = cli.find_files(args.only)
    rules = frozenset() if args.prototype_rules else (emit.P6T_RULES if args.p6t_rules else emit.ALL_RULES)
    results, rep = cli.run(files, src_root, pairs=False, rules=rules)
    ok = [r for r in results if r.status == 'ok']
    env = msvc_env(stage)
    incs = include_flags(stage)
    pch = build_pch(stage, env, incs)
    obj_dir = stage / 'obj'
    obj_dir.mkdir(exist_ok=True)

    def job(r):
        obj = obj_dir / (r.rel.replace('/', '__').replace('.java', '.obj'))
        return r, compile_one(r.out_path, obj, pch, env, incs)

    per_file = {}
    done = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for r, res in pool.map(job, ok):
            done += 1
            per_file[r.rel] = {'tier': r.tier, 'rows': sorted(r.api_rows), **res}
            if res['status'] == 'error':
                print(f'  [{done}/{len(ok)}] ERROR {r.rel}: {first_error_class(res)}', flush=True)
    by_status = Counter(v['status'] for v in per_file.values())
    by_tier = {t: Counter(v['status'] for v in per_file.values() if v['tier'] == t) for t in ('A', 'B')}
    classes = Counter(first_error_class(v) for v in per_file.values() if v['status'] == 'error')
    warn_codes = Counter(w['code'] for v in per_file.values() for w in v['warnings'])
    summary = {
        'javaFiles': len(results), 'transliterated': len(ok), 'tierA': rep['coverage']['tierA'], 'tierB': rep['coverage']['tierB'],
        'compiled': dict(by_status), 'byTier': {t: dict(c) for t, c in by_tier.items()},
        'errorClasses': dict(classes.most_common()), 'warningCodes': dict(warn_codes.most_common()),
        'seconds': round(time.time() - t0, 1),
    }
    if args.regscan:
        summary['regscan'] = run_regscan(stage, env, src_root)
    if args.json:
        Path(args.json).write_text(json.dumps({'summary': summary, 'files': per_file}, indent=1), encoding='utf-8')
    print(f"compile check: {len(ok)} of {len(results)} transliterated (tier A {summary['tierA']}, tier B {summary['tierB']}); "
          f"clean {by_status.get('clean', 0)}, warnings {by_status.get('warning', 0)}, errors {by_status.get('error', 0)}")
    for t, c in by_tier.items():
        print(f"  tier {t}: clean {c.get('clean', 0)}, warnings {c.get('warning', 0)}, errors {c.get('error', 0)}")
    for k, n in classes.most_common(25):
        print(f'  {n:4}  {k}')
    if warn_codes:
        print(f"  warning codes: {dict(warn_codes.most_common(10))}")
    if args.regscan:
        rs = summary['regscan']
        print(f"  regscan: exit {rs['exitCode']}, {len(rs['errors'])} errors {rs['summary']}")
        for e in rs['errors'][:10]:
            print(f'    {e}')
    print(f"  {summary['seconds']} s")
    return 0 if by_status.get('error', 0) == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
