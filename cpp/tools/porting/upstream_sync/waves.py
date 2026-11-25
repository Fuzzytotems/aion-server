"""Sorts the commits of the 2026-11 upstream sync (commits.json from collect.py, in the current directory) into port waves and writes
upstream-sync-2026-11.md. The wave lists and overlaps are hand-made for that sync; copy and edit them for the next one."""
import json

W = {
    0: ('No port: Java build, tooling, comments, formatting (verify only)',
        '874a64f28 f856882b3 7181a3864 2deb96507 20096e5d2 222076cb5 80548e389 ca2155e6d b7bc32976 15eaf77b6 2cd926056'),
    1: ('Network and server infrastructure (commons, login server, chat server, logging, startup, shutdown)',
        'e1a399e62 76b2f2bca d9800d686 31ad0571f 459b35094 01aadf926 81e409c5e bce89b2fc 4ca531afc b0317f967 17093650f 55d0af937 '
        'a485569df d22a9af5b 7840be30d b028752d0 8912ec714 546f96bd2 620fc69b0 9d8eeb2f2 36bdccb4d 92da2dd62'),
    2: ('Exploit and validation fixes (the October hardening batch)',
        '65eebace9 06ab87c2f 7e501bb9f 07959a2ef e2b0c35e2 f0c1a3695 ea257e5e9 a17e66f10 54880a691 ba383f739 46e8d00b9 4426db07c '
        '332d54885 6dd3aa507 33aa13cb6 638ed3dd4 68d6df38d c9c8b771a 8550d9a62 c6b9cc0e4 9f87bb2a8 dc1a3a3cd ed247770b ab48e50a8 '
        '567d4af5d dee487c3e 4c2004693 0432aff62 97b2a4d33 2d34af45a a39df86df 90dfeb604 a7092eb97 1a29700f6 e5ea7d94e'),
    3: ('Combat, skills and effects',
        'c22ed33b8 626e1c55e 17a718595 840a6a8d4 924d4ed82 b25a2664c 223b56a0d b87c6c656 558677569 ea7b1c32a 01a5602f1 fc87a3f0c '
        'a31c1dfcb 267ce6033 e7a16c553 3d86e120b 6c6eee658 b55aaf32c b0a597458 cf4b34d9c 38ab3e0ba d2dadc175 18a87505e'),
    4: ('World: zones, movement, falling, flight, channels, weather, duels',
        '206f9b5db 8c231daa1 84d3f7559 4617c9fd2 2b097879f 183723078 a519ffc7b 82a5e0cf9 9b171d237 822ec1fc2 2ee52b1d3 0199b4b03 '
        '374336b3b 0868abf0f 3194c558d 604957f50 30612bc3d 47ab0f3b7'),
    5: ('Items, economy and player systems (item actions, decomposables, crafting, PvP EXP, pets, teleport confirmation)',
        'cc61fb239 f7230c5fa d725054e1 e5c133836 784e0764b d254ea172 bad68ed81 90a740e2d 08f2df617 cb86e78ea 14d9b8acb da540feec '
        '9b15b6df8 eb35dfa5d f16e03ad3 6f5feaceb 83b9e3322 8e97687fd'),
    6: ('Content: quests, instances, AI, GM and player commands',
        '44c8d4387 f3998f526 93d5fa15b 96a005a09 433f42885 db5ae0453 c607b2094 205076446 68eccc7b1 1af948baf 5f9f1cba6 bfb358517 '
        'fb5adb72f 934d568a9 38664c074 ce54b7931 9089d1a81 cd1cfbc9f e4f2f79be 53363eea9 7213fb579 cabd580f3 616cb3c04 4990b3526 a8d182b67'),
}
OVERLAP = {
    'ab48e50a8': 'same bug as our P5-07 correction "a purification takes its kinah" (2026-10-05): compare, keep one',
    'cc61fb239': 'changes TamperingAction and PolishAction, where our P5-07 correction "TamperingAction cannot act without a target" lives',
    '65eebace9': 'changes CosmeticItemAction, where our P5-07 corrections live (coupon deletion, preset skin colour)',
    '9b15b6df8': 'changes TamperingAction and PolishAction (our P5-07 correction)',
    '15eaf77b6': 'changes PanesterraTeam (our P5-12b correction of removeTeamMember)',
    '53363eea9': 'reworks the chat-command framework and //movetoobj (our two C1 corrections; C++-dev //rift all and //damagepercent)',
    'a7092eb97': 'changes Legion.java (near our C1 //legion kick correction)',
    'cd1cfbc9f': 'Eternal Challenge balance: C++-dev has the owner\'s DPS report and boss-engage changes (P5-13)',
    '38664c074': 'custom instance boss drops: C++-dev changed CustomInstanceBossAI (P5-13)',
    'ce54b7931': 'CustomInstanceBossAI: C++-dev changed it (P5-13)',
    '9089d1a81': 'CustomInstanceBossAI: C++-dev changed it (P5-13)',
    '3194c558d': 'revive points: C++-dev changed the custom PvP map respawn (P5-13)',
    '7213fb579': '//access: C++-dev has the C++-only commands (z19)',
}

rows = json.load(open('commits.json', encoding='utf-8'))
byshort = {r['short']: r for r in rows}
assigned = {}
for w, (_, ids) in W.items():
    for i in ids.split():
        assert i not in assigned, i
        assigned[i] = w
missing = [r['short'] for r in rows if r['short'] not in assigned]
extra = [i for i in assigned if i not in byshort]
assert not missing and not extra, (missing, extra)


def code_lines(r):  # changed Java lines, tests excluded
    return sum(a + d for p, a, d in r['files'] if p.endswith('.java') and '/test/' not in p)


def data_lines(r):
    return sum(a + d for p, a, d in r['files'] if p.startswith('game-server/data/') and not p.endswith('.java'))


def wave_rows(w):
    return [r for r in rows if assigned[r['short']] == w]


schema = [r for r in rows if 'xsd' in r['kinds'] or 'sql' in r['kinds']]
out = ['# Upstream sync 2026-11: triage of beyond-aion 4.8 `51d7a19cb..68eccc7b1`', '',
       '152 upstream commits (2026-09-13 to 2026-11-22), merged into the Java tree of `4.8` by PR #425. The C++ port follows the Java of '
       '`51d7a19cb` until the waves below are ported. Made from `git show --numstat` per commit and `cpp/tools/porting/chunks.py owner` '
       'for the C++ chunk that owns each changed Java file (`cpp/tools/porting/upstream_sync`).', '',
       '**Java** is changed Java lines (added + deleted, tests excluded), **Data** changed lines under `game-server/data` other than handlers. '
       '**Chunks** are the owning C++ chunks, most-changed first. The wave of a commit is its main subject; several commits also touch '
       'other waves\' code.', '',
       '## Summary', '', '| Wave | Scope | Commits | Java lines | Data lines |', '|---|---|---|---|---|']
for w, (title, _) in W.items():
    rs = wave_rows(w)
    out.append(f'| {w} | {title} | {len(rs)} | {sum(map(code_lines, rs)):,} | {sum(map(data_lines, rs)):,} |')
out += [f'| | **Total** | **{len(rows)}** | **{sum(map(code_lines, rows)):,}** | **{sum(map(data_lines, rows)):,}** |', '',
        '## Foundation: data formats and database schema', '',
        f'{len(schema)} commits change an XSD or the SQL schema. Their loader, template and DAO parts go first, before any wave: the C++ '
        'servers cannot read the new data or database without them, so the play kit keeps the old data and database until they land. '
        'Their behaviour parts stay in their own wave.', '', '| Commit | Subject | XSD files | SQL files | Wave |', '|---|---|---|---|---|']
for r in schema:
    k = r['kinds']
    out.append(f"| `{r['short']}` | {r['subject'].strip()} | {k['xsd'][0] if 'xsd' in k else ''} | {k['sql'][0] if 'sql' in k else ''} | {assigned[r['short']]} |")
out += ['', '## Overlaps with our corrections and C++-dev changes', '',
        'Compare each before porting: upstream may fix the same bug another way, or change code the owner customised on C++-dev.', '',
        '| Commit | Subject | Overlap |', '|---|---|---|']
for s, why in OVERLAP.items():
    out.append(f"| `{s}` | {byshort[s]['subject'].strip()} | {why} |")
for w, (title, _) in W.items():
    rs = wave_rows(w)
    out += ['', f'## Wave {w}: {title}', '', f'{len(rs)} commits, {sum(map(code_lines, rs)):,} Java lines, {sum(map(data_lines, rs)):,} data lines.', '',
            '| Commit | Date | Subject | Java | Data | Chunks | Notes |', '|---|---|---|---|---|---|---|']
    for r in rs:
        ch = ', '.join(c for c in list(r['chunks'])[:4] if c != 'not')
        notes = [n for n, on in (('XSD', 'xsd' in r['kinds']), ('SQL', 'sql' in r['kinds']), ('overlap', r['short'] in OVERLAP)) if on]
        out.append(f"| `{r['short']}` | {r['date']} | {r['subject'].strip()} | {code_lines(r):,} | {data_lines(r):,} | {ch} | {', '.join(notes)} |")
open('upstream-sync-2026-11.md', 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
for w, (title, _) in W.items():
    rs = wave_rows(w)
    print(w, len(rs), sum(map(code_lines, rs)), sum(map(data_lines, rs)), title[:60])
print('schema commits:', len(schema))
