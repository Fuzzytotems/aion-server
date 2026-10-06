"""goldensample: the golden traces of the transliterated quest handlers that are not in the handler tree (phase6-transliterator.md §3.5, §7).

    python -m tools.gen.questgen.goldensample --stage DIR [--build-dir DIR] [--harness DIR] [--docs DIR] [--only FILE...] [--limit N]
                                              [--edit FILE OLD NEW]... [--jobs N] [--no-run] [--json OUT]

It builds, in DIR (outside the repository), the golden-trace harness of the tree (game-server/tests/quest_handlers_golden) with the
transliterated handlers that are not in the tree added to its table, and runs it:

1. transliterates the Java quest handlers into DIR/src (as `--dry-run --emit` would);
2. writes the oracle document of every transliterated file into DIR/expected (tools/oracle questtrace, from the Java; `--docs` takes a
   directory of documents written before instead), next to copies of the committed ones (tools/oracle/expected/quest);
3. picks the sample: every transliterated file with at least one case whose C++ file is not in the tree (a generated or hand-ported file of
   the tree is the tree's harness's), whose quest is not one the tree holds back (GOLDEN_HELD_BACK) and not in the tree's table;
4. compiles the harness sources with AION_GOLDEN_SAMPLE_TABLE (a header DIR/harness/GoldenSampleTable.h listing the sample) and
   AION_GOLDEN_EXPECTED_DIR (DIR/expected) defined, the tree's Golden*Handlers.cpp and the staged files in batches of 40, with the Debug
   flags of the harness's target, and links them against the libraries of `aion_gs_handlers_quest_q05_tests` in the build directory (which
   must have built that target);
5. runs the executable and summarizes its report by quest, case variant and hook (DIR/summary.json, or --json).

Nothing in the repository is written. The summary separates failed case variants (a comparison of a compared run failed, counted by
the check's name), cases no setup reproduces and vacuous cases (which the tree's harness requires to be listed; a sample has no list),
runs that reach an unported body, quests whose run stopped on an exception (a fixture limit or an untraced document), the registration
trace's findings per quest, other failures (an SEH exception among them), and a crash: the executable's exit code, a missing or partial
gtest report, the test that was running. The exit code is 1 when the run failed, crashed or found a failure of any kind (the review of #79,
item 4), so that a mutant that crashes the run does not look like a mutant that survived.

DIR must not hold anything but a former stage (the marker file .goldensample-stage), unless --force: the tool deletes its src, expected,
harness and obj subdirectories (the review of #79, item 7).
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
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import cli, compilecheck, emit, paths

GOLDEN = paths.CPP_GAME_SERVER / 'tests' / 'quest_handlers_golden'
COMMITTED_DOCS = paths.CPP_ROOT / 'tools' / 'oracle' / 'expected' / 'quest'
TREE_QUEST = paths.CPP_GAME_SERVER / 'handlers' / 'aion' / 'gameserver' / 'handlers' / 'quest'
BATCH = 40
STAGE_MARKER = '.goldensample-stage'
CHECK = re.compile(r'^\[(?P<name>\w+)\] ')

# the harness's report lines (GoldenQuestTraceTest.cpp): a failed comparison, an unlisted unreproducible or vacuous case, an unported reach
FAILED = re.compile(r'^(?P<quest>\d+) (?P<id>[A-Za-z]+#\d+(?:@\S+)?) \(setup: [^;)]*(?:; overlay: .*)?\): (?P<what>.*)$')
NOT_REPRODUCIBLE = re.compile(r'^not reproducible and not listed: (?P<quest>\d+) (?P<id>[A-Za-z]+#\d+(?:@\S+)?): (?P<why>.*)$')
VACUOUS = re.compile(r'^vacuous and not listed: (?P<quest>\d+) (?P<id>[A-Za-z]+#\d+)$')
UNPORTED = re.compile(r'^reaches AION_UNPORTED and not listed: (?P<quest>\d+) (?P<id>\S+): (?P<site>.*)$')
TALLY = re.compile(r'^\[golden\] quest (?P<quest>\d+): (?P<passed>\d+) passed, (?P<failed>\d+) failed, (?P<unrep>\d+) not reproducible of '
                   r'(?P<cases>\d+) cases and their high ends; (?P<runs>\d+) runs compared')


def table_rows(text, macro):
    """[(dir, Class, questId)] of an X-macro table of GoldenHandlers.h"""
    start = text.index(f'#define {macro}(X)')
    end = text.index('// clang-format on', start)
    return [(m[1], m[2], int(m[3])) for m in re.finditer(r'X\((\w+), (\w+), (\d+)\)', text[start:end])]


def held_back(text):
    m = re.search(r'GOLDEN_HELD_BACK\[\] = \{([^}]*)\}', text)
    return {int(x) for x in re.findall(r'\d+', m[1])} if m else set()


def pick_sample(docs, tree_rows, held, tree_quest=TREE_QUEST):
    """[(rel, questId)]: the documents with a case whose handler is not in the tree (see the module comment), in rel order"""
    in_table = {q for _d, _c, q in tree_rows}
    out = []
    for rel, doc in sorted(docs.items()):
        q = doc['questId']
        if not doc['cases'] or q in held or q in in_table or (Path(tree_quest) / rel.replace('.java', '.cpp')).is_file():
            continue
        out.append((rel, q))
    return out


def sample_table(sample):
    """the header AION_GOLDEN_SAMPLE_TABLE names: AION_GOLDEN_SAMPLE_HANDLERS(X) with one X(dir, Class, questId) per sample file"""
    rows = ''.join(f' \\\n\tX({emit.cpp_ident(rel.split("/")[0])}, {Path(rel).stem}, {q})' for rel, q in sample)
    return f'#pragma once\n// goldensample.py: the staged handlers of the sample\n#define AION_GOLDEN_SAMPLE_HANDLERS(X){rows}\n'


def classify(report_text, stdout):
    """the summary of a run: the gtest JSON report and the executable's output"""
    rep = json.loads(report_text) if report_text else {'testsuites': []}
    tallies = {}
    for line in stdout.splitlines():
        m = TALLY.match(line.strip())
        if m:
            tallies[int(m['quest'])] = {k: int(m[k]) for k in ('passed', 'failed', 'unrep', 'cases', 'runs')}
    failed = defaultdict(set)              # quest -> variant ids
    failed_checks = defaultdict(Counter)   # quest -> comparison name
    unrep, vacuous, unported, stopped, other = defaultdict(list), defaultdict(list), defaultdict(list), {}, defaultdict(list)
    registration = []
    for suite in rep['testsuites']:
        for t in suite.get('testsuite', []):
            name = t['name']
            quest = int(name.split('Quest')[-1]) if '/Quest' in name or name.startswith('EveryCaseMatchesTheJavaTrace') else None
            for f in t.get('failures', []):
                text = f['failure']
                lines = [ln for ln in text.splitlines() if ln.strip()]
                body = next((ln for ln in lines if not ln.startswith(('D:', 'C:', 'Google Test trace:', 'Failed', 'Expected', 'Which is'))
                             and ':' in ln), lines[0] if lines else '')
                if suite['name'].endswith('GoldenQuestTraceTest') and t['name'] == 'RegistrationTraceMatchesJavaRegister':
                    registration.append(text)
                    continue
                if quest is None:
                    other[name].append(text)
                    continue
                hit = False
                for ln in lines:
                    ln = ln.strip()
                    for rx, sink in ((FAILED, None), (NOT_REPRODUCIBLE, unrep), (VACUOUS, vacuous), (UNPORTED, unported)):
                        m = rx.match(ln)
                        if not m:
                            continue
                        hit = True
                        if sink is None:
                            failed[quest].add(m['id'])
                            named = CHECK.match(m['what'])
                            failed_checks[quest][named['name'] if named else 'unnamed'] += 1
                        else:
                            sink[quest].append(m['id'] + (': ' + m['why'] if 'why' in m.groupdict() and m['why'] else ''))
                        break
                if not hit:
                    if 'C++ exception' in text:
                        m = re.search(r'C\+\+ exception with description "(.*)" thrown', text)
                        stopped[quest] = (m[1] if m else body.strip())[:300]
                    else:
                        other[f'Quest{quest}'].append(body.strip()[:300])
    by_hook = Counter()
    quests_by_hook = defaultdict(set)
    for q, ids in failed.items():
        for i in ids:
            hook = i.split('#')[0]
            by_hook[hook] += 1
            quests_by_hook[hook].add(q)
    passed = sum(t['passed'] for t in tallies.values())
    return {
        'quests': len(tallies) + len([q for q in stopped if q not in tallies]),
        'ran': len(tallies),
        'variantsPassed': passed,
        'variantsFailed': sum(len(v) for v in failed.values()),
        'runsCompared': sum(t['runs'] for t in tallies.values()),
        'questsWithAFailure': len(failed),
        'questsWithoutFinding': sum(1 for q in tallies if q not in failed and q not in unrep and q not in vacuous and q not in unported),
        'failedByHook': {h: {'variants': n, 'quests': sorted(quests_by_hook[h])} for h, n in by_hook.most_common()},
        'failed': {str(q): sorted(ids) for q, ids in sorted(failed.items())},
        'failedChecks': {str(q): dict(c) for q, c in sorted(failed_checks.items())},
        'notReproducible': {str(q): v for q, v in sorted(unrep.items())},
        'vacuous': {str(q): v for q, v in sorted(vacuous.items())},
        'unported': {str(q): v for q, v in sorted(unported.items())},
        'stopped': {str(q): v for q, v in sorted(stopped.items())},
        'other': dict(other),
        'registration': registration,
        'tallies': {str(q): t for q, t in sorted(tallies.items())},
    }


def print_summary(s, out=sys.stdout):
    p = lambda *a: print(*a, file=out)  # noqa: E731
    p(f"golden sample: {s['quests']} quests, {s['ran']} ran; {s['variantsPassed']} variants passed, {s['variantsFailed']} failed "
      f"({s['questsWithAFailure']} quests); {s['runsCompared']} runs compared; {s['questsWithoutFinding']} quests without a finding")
    for h, v in s['failedByHook'].items():
        p(f"  failed {h}: {v['variants']} variants, quests {v['quests']}")
    p(f"  not reproducible (unlisted): {sum(len(v) for v in s['notReproducible'].values())} variants, {len(s['notReproducible'])} quests")
    p(f"  vacuous (unlisted): {sum(len(v) for v in s['vacuous'].values())} cases, {len(s['vacuous'])} quests")
    p(f"  reaches AION_UNPORTED: {sum(len(v) for v in s['unported'].values())} variants, {len(s['unported'])} quests")
    p(f"  stopped by an exception: {len(s['stopped'])} quests")
    for q, why in list(s['stopped'].items())[:40]:
        p(f'    {q}: {why[:160]}')
    p(f"  registration trace: {len(s['registration'])} findings")
    for r in s['registration'][:20]:
        p('    ' + ' | '.join(ln.strip() for ln in r.splitlines() if ln.strip())[:300])
    checks = Counter()
    for c in s['failedChecks'].values():
        checks.update(c)
    if checks:
        p(f'  failed checks: {dict(checks.most_common())}')
    p(f"  other failures (SEH exceptions among them): {sum(len(v) for v in s['other'].values())}")
    for name, texts in list(s['other'].items())[:20]:
        p(f'    {name}: ' + ' | '.join(ln.strip() for ln in texts[0].splitlines() if ln.strip())[:300])
    if s.get('crash'):
        p(f"  CRASH: {s['crash']}")
    for problem in problems(s):
        p(f'  FAILED: {problem}')


def crash_of(report_text, stdout, exit_code):
    """'' when the run ended in order, else what went wrong: a non-zero exit code with no failure in the report, a missing or unreadable
    report, or a report with fewer tests than the run started (the test that was running when the process ended)"""
    started = [ln.split('] ', 1)[1].split(' ')[0] for ln in stdout.splitlines() if ln.startswith('[ RUN      ]')]
    ended = {ln.split('] ', 1)[1].split(' ')[0] for ln in stdout.splitlines() if ln.startswith(('[       OK ]', '[  FAILED  ]', '[  SKIPPED ]'))}
    running = [t for t in started if t not in ended]
    where = f' (running: {running[-1]})' if running else ''
    try:
        rep = json.loads(report_text) if report_text else None
    except json.JSONDecodeError:
        return f'the gtest report is not JSON, exit code {exit_code}{where}'
    if rep is None:
        return f'no gtest report, exit code {exit_code}{where}'
    if rep.get('tests', 0) < len(started):
        return f"a partial gtest report ({rep.get('tests', 0)} of {len(started)} tests), exit code {exit_code}{where}"
    if exit_code != 0 and rep.get('failures', 0) == 0 and rep.get('errors', 0) == 0:
        return f'exit code {exit_code} with no failure in the report{where}'
    return ''


def problems(s):
    """every reason the run is not clean: the summary's failures of any kind, a crash, a non-zero exit code"""
    out = []
    if s.get('crash'):
        out.append('the run crashed: ' + s['crash'])
    elif s.get('exitCode', 0) != 0:
        out.append(f"exit code {s['exitCode']}")
    for key, what in (('failed', 'quests with a failed variant'), ('notReproducible', 'quests with an unlisted unreproducible case'),
                      ('vacuous', 'quests with an unlisted vacuous case'), ('unported', 'quests reaching AION_UNPORTED'),
                      ('stopped', 'quests stopped by an exception'), ('other', 'other failures')):
        if s.get(key):
            out.append(f'{len(s[key])} {what}')
    if s.get('registration'):
        out.append(f"{len(s['registration'])} registration trace findings")
    return out


def flags(stage, build_dir, java_dir, table, expected):
    vc = compilecheck.vcpkg_include()
    return [*compilecheck.CL_FLAGS, f'/DAION_GAMESERVER_JAVA_DIR="{java_dir}"', '/DGTEST_LINKED_AS_SHARED_LIBRARY=1',
            f'/DAION_GOLDEN_SAMPLE_TABLE="{table.as_posix()}"', f'/DAION_GOLDEN_EXPECTED_DIR="{expected.as_posix()}"',
            f'/I{stage / "src"}', f'/I{paths.CPP_ROOT / "commons" / "src"}', f'/I{build_dir / "generated"}',
            f'/I{paths.CPP_GAME_SERVER / "src"}', f'/I{paths.CPP_GAME_SERVER / "generated"}', f'/I{paths.CPP_GAME_SERVER / "handlers"}',
            f'/external:I{vc}', f'/external:I{vc / "mysql"}']


def link_libraries(build_dir):
    """the Debug libraries of aion_gs_handlers_quest_q05_tests (its .vcxproj), absolute"""
    gs = build_dir / 'game-server'
    vcx = (gs / 'aion_gs_handlers_quest_q05_tests.vcxproj').read_text(encoding='utf-8')
    block = vcx[vcx.index("'$(Configuration)|$(Platform)'=='Debug|x64'"):]
    deps = re.search(r'<AdditionalDependencies>(.*?)</AdditionalDependencies>', block).group(1).split(';')
    libs = []
    for d in deps:
        if not d or d.startswith('%'):
            continue
        p = Path(d)
        libs.append(str(p if p.is_absolute() else (gs / p).resolve()) if ('\\' in d or '/' in d) else d)
    return libs


def trace_docs(rels, out_dir):
    """{rel: document} of tools/oracle's questtrace for these files (a file the oracle raises on is left out), written to out_dir"""
    sys.path.insert(0, str(paths.CPP_ROOT / 'tools' / 'oracle'))
    from questtrace import extract  # noqa: E402  (tools/oracle; the oracle never imports the generator, this tool imports the oracle)
    from staticdata_oracle import OracleError  # noqa: E402
    tables = extract.Tables()
    docs, raised = {}, {}
    for rel in rels:
        try:
            doc = extract.trace_file(tables, paths.JAVA_QUEST_DIR / rel, rel)
        except OracleError as e:
            raised[rel] = str(e)
            continue
        (out_dir / f"{doc['questId']}.json").write_bytes(extract.dumps(doc).encode('utf-8'))
        docs[rel] = doc
    return docs, raised


def main(argv=None):
    ap = argparse.ArgumentParser(prog='python -m tools.gen.questgen.goldensample', description=__doc__.split('\n\n')[0])
    ap.add_argument('--stage', required=True, metavar='DIR', help='scratch directory outside the repository')
    ap.add_argument('--build-dir', default=str(paths.CPP_ROOT / 'build' / 'msvc'), help='the build with aion_gs_handlers_quest_q05_tests')
    ap.add_argument('--harness', default=str(GOLDEN), help="the harness sources (default: the tree's)")
    ap.add_argument('--docs', metavar='DIR', help='oracle documents written before (default: traced now)')
    ap.add_argument('--only', nargs='+', metavar='FILE', help='only these quest files')
    ap.add_argument('--limit', type=int, default=0, help='the first N files of the sample')
    ap.add_argument('--jobs', '-j', type=int, default=4)
    ap.add_argument('--edit', nargs=3, action='append', default=[], metavar=('FILE', 'OLD', 'NEW'),
                    help='replace OLD (once) by NEW in the emitted C++ of FILE: a mutant, which the run must fail')
    ap.add_argument('--allow-tree', action='store_true',
                    help='with --only: also files whose C++ is in the tree (a mutant of a hand-ported or tree file: compiled from the stage)')
    ap.add_argument('--force', action='store_true', help='use DIR even if it holds other files than a former stage')
    ap.add_argument('--no-run', action='store_true', help='build only')
    ap.add_argument('--json', metavar='OUT', help='the summary (default DIR/summary.json)')
    args = ap.parse_args(argv)
    stage = Path(args.stage).resolve()
    try:
        stage.relative_to(paths.REPO_ROOT.resolve())
        ap.error(f'--stage {stage} is inside the repository; name a directory outside it')
    except ValueError:
        pass
    build_dir, harness = Path(args.build_dir).resolve(), Path(args.harness).resolve()
    if not stage_is_ours(stage) and not args.force:
        ap.error(f'--stage {stage} holds files and is no former stage (no {STAGE_MARKER}): name an empty or new directory, or pass --force')
    t0 = time.time()
    stage.mkdir(parents=True, exist_ok=True)
    (stage / STAGE_MARKER).write_text('goldensample.py stage: its src, expected, harness and obj are rewritten by every run\n', encoding='utf-8')
    for d in ('src', 'expected', 'harness', 'obj'):
        shutil.rmtree(stage / d, ignore_errors=True)
        (stage / d).mkdir(parents=True)
    results, _rep = cli.run(cli.find_files(args.only), stage / 'src', pairs=False, rules=emit.ALL_RULES)
    ok = [r.rel for r in results if r.status == 'ok']
    for rel, old, new in args.edit:
        out = next(Path(r.out_path) for r in results if r.rel == rel and r.status == 'ok')
        text = out.read_text(encoding='utf-8')
        if text.count(old) != 1:
            ap.error(f'--edit: {old!r} occurs {text.count(old)} times in {out}')
        out.write_text(text.replace(old, new), encoding='utf-8')
    expected = stage / 'expected'
    if args.docs:
        docs = {}
        for f in Path(args.docs).glob('*.json'):
            doc = json.loads(f.read_text(encoding='utf-8'))
            if doc.get('java') in ok:
                docs[doc['java']] = doc
                shutil.copy(f, expected / f.name)
        raised = {}
    else:
        docs, raised = trace_docs(ok, expected)
    for f in COMMITTED_DOCS.glob('*.json'):
        shutil.copy(f, expected / f.name)
    text = (harness / 'GoldenHandlers.h').read_text(encoding='utf-8')
    sample = pick_sample(docs, table_rows(text, 'AION_GOLDEN_GENERATED_HANDLERS'), held_back(text),
                         tree_quest=Path(args.stage) / 'no-tree' if args.allow_tree and args.only else TREE_QUEST)
    if args.limit:
        sample = sample[:args.limit]
    keep = {f'{q}.json' for _r, q in sample} | {f.name for f in COMMITTED_DOCS.glob('*.json')}
    for f in expected.glob('*.json'):
        if f.name not in keep:
            f.unlink()                     # the harness drives every document of the directory that the table names
    print(f'goldensample: {len(ok)} transliterated, {len(docs)} traced ({len(raised)} raised), sample {len(sample)} files', flush=True)
    table = stage / 'harness' / 'GoldenSampleTable.h'
    table.write_text(sample_table(sample), encoding='utf-8')
    units = []
    for k in range(0, len(sample), BATCH):
        u = stage / 'harness' / f'GoldenSample{k // BATCH}.cpp'
        u.write_text(''.join(f'#include "aion/gameserver/handlers/quest/{rel[:-5]}.cpp"\n' for rel, _q in sample[k:k + BATCH]), encoding='utf-8')
        units.append(u)
    sources = [harness / 'GoldenQuestTraceTest.cpp'] + sorted(harness.glob('Golden*Handlers.cpp')) + units
    env = compilecheck.msvc_env(stage)
    cl = compilecheck.cl_exe(env)
    # the harness's own directory first; the tree's for its relative includes (../quest_handlers/...) when --harness is a copy
    fl = flags(stage, build_dir, (paths.REPO_ROOT / 'game-server').as_posix(), table, expected) + [f'/I{harness}', f'/I{GOLDEN}']

    def comp(src):
        obj = stage / 'obj' / (Path(src).stem + '.obj')
        p = subprocess.run([cl, *fl, f'/Fo{obj}', str(src)], capture_output=True, text=True, env=env, cwd=stage / 'obj')
        return src, obj, p.returncode, compilecheck.parse_diagnostics(p.stdout + p.stderr)

    objs = []
    with ThreadPoolExecutor(max(1, args.jobs)) as pool:
        for src, obj, rc, diags in pool.map(comp, sources):
            errors = [d for d in diags if d['kind'] != 'warning']
            if rc != 0 or diags:
                print(f'  {Path(src).name}: exit {rc}, {len(errors)} errors, {len(diags) - len(errors)} warnings', flush=True)
                for d in diags[:5]:
                    print(f"    {d['file']}({d['line']}): {d['kind']} {d['code']}: {d['msg'][:200]}")
            if rc != 0:
                return 1
            objs.append(str(obj))
    exe = stage / 'golden_sample.exe'
    link = str(Path(cl).parent / 'link.exe')
    p = subprocess.run([link, '/nologo', f'/OUT:{exe}', '/SUBSYSTEM:CONSOLE', '/machine:x64', '/INCREMENTAL:NO', *objs,
                        *link_libraries(build_dir)], capture_output=True, text=True, env=env, cwd=stage)
    if p.returncode != 0:
        print((p.stdout + p.stderr)[-3000:])
        return 1
    (stage / 'sample.json').write_text(json.dumps([{'rel': r, 'questId': q} for r, q in sample], indent=1), encoding='utf-8')
    print(f'  built {exe} ({round(time.time() - t0)} s)', flush=True)
    if args.no_run:
        return 0
    run_env = dict(os.environ)
    run_env['PATH'] = str(build_dir / 'game-server' / 'Debug') + os.pathsep + run_env.get('PATH', '')
    report = stage / 'report.json'
    report.unlink(missing_ok=True)
    with open(stage / 'stdout.txt', 'w', encoding='utf-8', errors='replace') as out:
        rc = subprocess.run([str(exe), f'--gtest_output=json:{report}', '--gtest_filter=*GoldenQuestCases*:*RegistrationTrace*'], stdout=out,
                            stderr=subprocess.STDOUT, env=run_env, cwd=stage).returncode
    report_text = report.read_text(encoding='utf-8', errors='replace') if report.is_file() else ''
    stdout = (stage / 'stdout.txt').read_text(encoding='utf-8', errors='replace')
    try:
        summary = classify(report_text, stdout)
    except json.JSONDecodeError:
        summary = classify('', stdout)
    summary['exitCode'] = rc
    summary['crash'] = crash_of(report_text, stdout, rc)
    summary['sampleFiles'] = len(sample)
    summary['seconds'] = round(time.time() - t0)
    Path(args.json or stage / 'summary.json').write_text(json.dumps(summary, indent=1), encoding='utf-8')
    print_summary(summary)
    return 1 if problems(summary) else 0


def stage_is_ours(stage):
    """a stage the tool may rewrite: missing, empty, or marked by an earlier run"""
    return not stage.exists() or (stage.is_dir() and (not any(stage.iterdir()) or (stage / STAGE_MARKER).is_file()))


if __name__ == '__main__':
    sys.exit(main())
