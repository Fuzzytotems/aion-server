"""Collects an upstream sync delta per commit: its footprint by kind and the C++ chunks owning the Java files it changes.

python collect.py BASE TIP OUT.json   e.g. python collect.py 51d7a19cb upstream/4.8 commits.json (then waves.py)
"""
import json, re, subprocess, sys
from pathlib import Path
from collections import defaultdict
REPO = str(Path(__file__).resolve().parents[4])
BASE, TIP = sys.argv[1], sys.argv[2]
def git(*a): return subprocess.run(['git', '-C', REPO, *a], capture_output=True, text=True, encoding='utf-8', errors='replace', check=True).stdout
commits = git('log', '--reverse', '--format=%H%x09%h%x09%ad%x09%an%x09%P%x09%s', '--date=short', f'{BASE}..{TIP}').splitlines()
rows, java_paths = [], set()
for line in commits:
    full, short, date, author, parents, subject = line.split('\t', 5)
    merge = len(parents.split()) > 1
    files = []
    if not merge:
        for l in git('show', '--numstat', '--format=', '-M', full).splitlines():
            a, d, p = l.split('\t', 2)
            if ' => ' in p:  # rename: take the new path
                p = re.sub(r'\{([^}]*) => ([^}]*)\}', r'\2', p).split(' => ')[-1].replace('//', '/')
            files.append((p, 0 if a == '-' else int(a), 0 if d == '-' else int(d)))
    kinds = defaultdict(lambda: [0, 0, 0])
    for p, a, d in files:
        if p.endswith('.xsd'): k = 'xsd'
        elif p.endswith('.sql'): k = 'sql'
        elif '/config/' in p and p.endswith('.properties'): k = 'config'
        elif p.startswith('game-server/data/handlers/') and p.endswith('.java'): k = 'handlers'
        elif p.startswith('game-server/data/'): k = 'data'
        elif p.startswith('game-server/src/') and p.endswith('.java'): k = 'gs_java'
        elif p.endswith('.java') and '/test/' in p: k = 'java_tests'
        elif p.startswith(('login-server/', 'chat-server/', 'commons/')): k = p.split('/')[0]
        else: k = 'other'
        kinds[k][0] += 1; kinds[k][1] += a; kinds[k][2] += d
        if k in ('gs_java', 'handlers'): java_paths.add(p)
    rows.append(dict(hash=full, short=short, date=date, author=author, subject=subject, merge=merge, files=files, kinds=dict(kinds)))
# owners of every Java file, in batches
owners = {}
paths = sorted(java_paths)
for i in range(0, len(paths), 200):
    out = subprocess.run([sys.executable, f'{REPO}/cpp/tools/porting/chunks.py', 'owner', *paths[i:i+200]], cwd=REPO, capture_output=True, text=True).stdout
    for l in out.splitlines():
        m = re.match(r'(.*?): (\S+)', l)
        if m: owners['game-server/' + m.group(1)] = m.group(2)
for r in rows:
    ch = defaultdict(int)
    for p, a, d in r['files']:
        if p in owners: ch[owners[p]] += a + d
    r['chunks'] = dict(sorted(ch.items(), key=lambda kv: -kv[1]))
json.dump(rows, open(sys.argv[3], 'w', encoding='utf-8'), indent=1)
print(len(rows), 'commits,', sum(r['merge'] for r in rows), 'merges,', len(paths), 'java files,', len(set(owners.values())), 'chunks')
