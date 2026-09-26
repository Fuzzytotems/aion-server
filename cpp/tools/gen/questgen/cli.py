"""cli: the driver of the quest transliterator prototype and its dry-run report.

    python -m tools.gen.questgen --dry-run [--emit DIR] [--only FILE...] [--json OUT] [--markdown OUT] [--quiet]

--dry-run   transliterate every Java quest handler (or the --only files) in memory and print the report: files transliterated (tier A:
            core vocabulary only; tier B: at least one API_TABLE row), files refused grouped by their primary reason, the API calls
            used with their counts and whether the C++ side declares them today (and the body status census.py reports), the API gaps
            that block the most files, and the Elyos/Asmodian mirror pairs G2's twin mode could share.
--emit DIR  also write each transliterated file to DIR/aion/gameserver/handlers/quest/<dir>/<Class>.cpp. DIR must lie outside the
            repository (the prototype never writes into the source tree); nothing is compiled.
--only      restrict to these files: paths below data/handlers/quest (`eltnen/_1363ThankingMabangtah.java`), absolute paths or class
            names (`_1363ThankingMabangtah`).
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

from . import api as apimod
from . import emit, mirror, paths

# the inventory's prediction (docs/design/phase6-inventory.md §0 item 3, §7.1)
PREDICTED = {'tierA': 660, 'withRules': (914, 929), 'total': 1035}


def find_files(only=None, quest_dir=None):
    root = Path(quest_dir or paths.JAVA_QUEST_DIR)
    every = sorted(root.rglob('*.java'))
    if not only:
        return every
    by_name = defaultdict(list)
    for f in every:
        by_name[f.stem].append(f)
    out = []
    for o in only:
        p = Path(o)
        if p.is_absolute() and p.is_file():
            out.append(p)
        elif (root / o).is_file():
            out.append(root / o)
        elif by_name.get(p.stem):
            out.extend(by_name[p.stem])
        else:
            raise SystemExit(f'questgen: no quest handler matches {o!r}')
    return sorted(set(out))


def run(files, emit_dir=None, tr=None, pairs=True):
    tr = tr or emit.Transliterator()
    results = []
    for f in files:
        r = tr.transliterate(f)
        results.append(r)
        if emit_dir is not None and r.status == 'ok':
            ns = r.package.split('.')
            out = Path(emit_dir) / 'aion' / 'gameserver' / 'handlers' / Path(*[emit.cpp_ident(s) for s in ns]) / f'{r.klass}.cpp'
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(r.cpp.encode('utf-8'))
            r.out_path = str(out)
    report = summarize(results, tr.api)
    if pairs:
        report['mirrorPairs'] = mirror.pairs_report(files, {r.rel: r for r in results}, tr.quest_dir)
    return results, report


def summarize(results, api):
    total = len(results)
    ok = [r for r in results if r.status == 'ok']
    refused = [r for r in results if r.status != 'ok']
    tier = Counter(r.tier for r in ok)
    by_primary = defaultdict(list)
    for r in refused:
        key = r.primary
        if key in emit.DETAILED:
            key = next(k for k in r.reason_keys() if k.startswith(key + ':'))
        by_primary[key].append(r.rel)
    all_reasons = Counter()
    for r in refused:
        for k in r.reason_keys():
            all_reasons[k.split(':')[0]] += 1
    # API gaps: files each missing API blocks, and files it alone blocks
    gap_files = defaultdict(set)
    gap_only = Counter()
    for r in refused:
        keys = r.reason_keys()
        for k in keys:
            if k.startswith(('api-missing:', 'api-undeclared:')):
                gap_files[k].add(r.rel)
        if len(keys) == 1 and keys[0].startswith(('api-missing:', 'api-undeclared:')):
            gap_only[keys[0]] += 1
    # API usage over the transliterated files
    calls = Counter()
    files_per = Counter()
    status = defaultdict(set)
    undeclared_files = [r.rel for r in ok if r.undeclared]
    for r in ok:
        for k, n in r.apis.items():
            calls[k] += n
            files_per[k] += 1
        for k, s in r.api_status.items():
            status[k] |= s
    tiers = {}
    for r in ok:
        tiers.update(r.api_tier)
    rows = []
    for k in sorted(calls, key=lambda k: (-files_per[k], k)):
        owner, _, member = k.partition('.')
        declared = 'planned' if (owner, member) in apimod.PLANNED else 'yes'
        rows.append({'api': k, 'tier': tiers.get(k, '?'), 'calls': calls[k], 'files': files_per[k], 'declared': declared,
                     'bodies': sorted(status.get(k, ())) or ['-']})
    idioms = Counter()
    for r in ok:
        idioms.update({k: 1 for k in r.idioms})
    hazards = {r.rel: r.hazards for r in ok if r.hazards}
    java_bugs = {r.rel: r.java_bugs for r in ok if r.java_bugs}
    row_files = Counter()
    for r in ok:
        for rid in r.api_rows:
            row_files[rid] += 1
    lines_ok = sum(r.lines for r in ok)
    lines_all = sum(r.lines for r in results)
    rep = {
        'coverage': {'transliterated': len(ok), 'refused': len(refused), 'total': total,
                     'pct': round(100.0 * len(ok) / total, 1) if total else 0.0,
                     'tierA': tier.get('A', 0), 'tierB': tier.get('B', 0),
                     'lines': {'transliterated': lines_ok, 'total': lines_all},
                     'blockedOnPlannedDeclaration': len(undeclared_files),
                     'predicted': PREDICTED},
        'refusedByPrimaryReason': {k: {'files': len(v), 'examples': sorted(v)[:5]} for k, v in
                                   sorted(by_primary.items(), key=lambda kv: (-len(kv[1]), kv[0]))},
        'refusalCategories': dict(all_reasons.most_common()),
        'apiGaps': [{'api': k, 'files': len(v), 'onlyReason': gap_only.get(k, 0), 'examples': sorted(v)[:3]}
                    for k, v in sorted(gap_files.items(), key=lambda kv: (-len(kv[1]), kv[0]))],
        'apiUsed': rows,
        'apiRowsUsed': {r.id: {'title': r.title, 'files': row_files.get(r.id, 0), 'gate': r.gate} for r in apimod.API_TABLE},
        'idioms': dict(idioms.most_common()),
        'hazards': hazards,
        'knownJavaBugsKept': java_bugs,
        'filesBlockedOnPlannedDeclaration': sorted(undeclared_files),
        'files': {r.rel: {'status': r.status, 'tier': r.tier, 'questId': r.quest_id, 'reasons': [f'{c}: {d}' for c, d in r.reasons],
                          'rows': sorted(r.api_rows), 'apis': sorted(r.apis), 'undeclared': sorted(r.undeclared),
                          'unportedBodies': sorted(k for k, s in r.api_status.items() if s & {'unported', 'partial', 'declaredOnly'}),
                          'hazards': r.hazards, 'javaBugs': r.java_bugs, 'out': r.out_path} for r in results},
    }
    return rep


def text_report(rep, verbose=False):
    c = rep['coverage']
    out = []
    out.append(f"questgen dry run: {c['transliterated']} of {c['total']} transliterated ({c['pct']}%), {c['refused']} refused")
    out.append(f"  tier A (core vocabulary only) {c['tierA']}, tier B (API table rows) {c['tierB']}; "
               f"{c['blockedOnPlannedDeclaration']} of them call an API the C++ side does not declare yet (PLANNED)")
    out.append(f"  lines: {c['lines']['transliterated']} of {c['lines']['total']} Java lines")
    p = c['predicted']
    out.append(f"  inventory prediction: tier A {p['tierA']}, with every tier-B rule {p['withRules'][0]}-{p['withRules'][1]} of {p['total']}")
    out.append('')
    out.append('refused, by primary reason:')
    for k, v in rep['refusedByPrimaryReason'].items():
        out.append(f"  {v['files']:4}  {k}    e.g. {', '.join(v['examples'][:2])}")
    out.append('')
    out.append('refusal categories (a file counts once per category):')
    for k, v in rep['refusalCategories'].items():
        out.append(f'  {v:4}  {k}')
    out.append('')
    out.append('API gaps (files blocked; files where it is the only reason):')
    for g in rep['apiGaps'][:40]:
        out.append(f"  {g['files']:4} {g['onlyReason']:4}  {g['api']}")
    out.append('')
    out.append('API calls in the transliterated files (tier, calls, files, declared in C++ today, body status):')
    for r in rep['apiUsed']:
        out.append(f"  {r['tier']:5} {r['calls']:6} {r['files']:5}  {r['declared']:8} {','.join(r['bodies']):28} {r['api']}")
    out.append('')
    out.append('API table rows (files using each):')
    for k, v in rep['apiRowsUsed'].items():
        out.append(f"  {k} {v['files']:4}  {v['title']}  [{v['gate']}]")
    out.append('')
    out.append('idiom rules applied (files):')
    for k, v in rep['idioms'].items():
        out.append(f'  {v:4}  {k}')
    out.append('')
    out.append(f"parity hazards in transliterated files (C++ argument/operand order is unspecified): {len(rep['hazards'])} files")
    for rel, hs in list(rep['hazards'].items())[:10]:
        out.append(f"  {rel}: {'; '.join(hs[:2])}")
    out.append(f"known Java bugs kept and marked `// java-bug kept` (phase6-inventory.md §11, U3): {len(rep['knownJavaBugsKept'])} files")
    for rel, bs in rep['knownJavaBugsKept'].items():
        out.append(f"  {rel}: {'; '.join(bs)}")
    mp = rep.get('mirrorPairs')
    if mp:
        out.append('')
        out.append(f"mirror pairs (Elyos/Asmodian by quest_data.xml race_permitted, literal twins after normalising literals and race names): "
                   f"{mp['pairs']} ({mp['uniquePairs']} unique twins, {mp['pairsInLargerGroups']} paired inside larger equal-structure groups); "
                   f"leading-digit mirrors (1xxx/2xxx, 3xxx/4xxx) {mp['leadingDigitMirrors']}, of which literal twins "
                   f"{mp['leadingDigitMirrorsThatAreLiteralTwins']}")
        for k, v in mp['byOutcome'].items():
            out.append(f'  {v:4}  {k}')
        out.append(f"  N-way exact groups among refused files: {mp['refusedGroups']} groups, {mp['refusedGroupFiles']} files "
                   f"({mp['refusedGroupClones']} clones of a hand-ported representative)")
    if verbose:
        out.append('')
        for rel, f in rep['files'].items():
            if f['status'] != 'ok':
                out.append(f"  REFUSED {rel}: {'; '.join(f['reasons'][:4])}")
    return '\n'.join(out)


def markdown_report(rep):
    c = rep['coverage']
    out = ['# questgen dry run', '',
           f"{c['transliterated']} of {c['total']} quest handlers transliterated ({c['pct']}%): tier A {c['tierA']}, tier B {c['tierB']}; "
           f"{c['refused']} refused.", '', '## Refused, by primary reason', '', '| Files | Reason | Examples |', '|---|---|---|']
    for k, v in rep['refusedByPrimaryReason'].items():
        out.append(f"| {v['files']} | {k} | {', '.join(v['examples'][:3])} |")
    out += ['', '## API gaps', '', '| Files | Only reason | API |', '|---|---|---|']
    for g in rep['apiGaps'][:30]:
        out.append(f"| {g['files']} | {g['onlyReason']} | `{g['api']}` |")
    out += ['', '## API calls used', '', '| API | Tier | Calls | Files | Declared | Bodies |', '|---|---|---|---|---|---|']
    for r in rep['apiUsed']:
        out.append(f"| `{r['api']}` | {r['tier']} | {r['calls']} | {r['files']} | {r['declared']} | {', '.join(r['bodies'])} |")
    return '\n'.join(out) + '\n'


def main(argv=None):
    ap = argparse.ArgumentParser(prog='python -m tools.gen.questgen', description=__doc__.split('\n\n')[0])
    ap.add_argument('--dry-run', action='store_true', help='transliterate in memory and print the report (the default action)')
    ap.add_argument('--emit', metavar='DIR', help='write the generated .cpp files below DIR (outside the repository)')
    ap.add_argument('--only', nargs='+', metavar='FILE', help='only these quest files')
    ap.add_argument('--json', metavar='OUT', help='write the report as JSON')
    ap.add_argument('--markdown', metavar='OUT', help='write the report as Markdown')
    ap.add_argument('--no-pairs', action='store_true', help='skip the mirror-pair analysis')
    ap.add_argument('--verbose', '-v', action='store_true', help='list every refused file with its reasons')
    ap.add_argument('--quiet', '-q', action='store_true', help='print only the coverage line')
    args = ap.parse_args(argv)
    emit_dir = None
    if args.emit:
        emit_dir = Path(args.emit).resolve()
        try:
            emit_dir.relative_to(paths.REPO_ROOT.resolve())
            ap.error(f'--emit {emit_dir} is inside the repository ({paths.REPO_ROOT}); name a directory outside it')
        except ValueError:
            pass
    t0 = time.time()
    files = find_files(args.only)
    _results, rep = run(files, emit_dir, pairs=not args.no_pairs)
    rep['seconds'] = round(time.time() - t0, 1)
    if args.json:
        Path(args.json).write_text(json.dumps(rep, indent=1, default=sorted), encoding='utf-8')
    if args.markdown:
        Path(args.markdown).write_text(markdown_report(rep), encoding='utf-8')
    if args.quiet:
        c = rep['coverage']
        print(f"{c['transliterated']}/{c['total']} transliterated ({c['pct']}%), tier A {c['tierA']}, tier B {c['tierB']}")
    else:
        print(text_report(rep, args.verbose))
    return 0


if __name__ == '__main__':
    sys.exit(main())
