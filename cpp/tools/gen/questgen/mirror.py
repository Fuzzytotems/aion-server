"""mirror: the Elyos/Asmodian twins G2's twin mode could share (docs/design/phase6-inventory.md §7.2).

A pair is two quest handlers whose quests quest_data.xml permits to different races (ELYOS / ASMODIANS) and whose token streams are equal
once literals (numbers, strings, chars), the class name and the race identifiers (ELYOS, ASMODIANS, ASMODIAN, ELYO) are normalised: a
literal twin, portable once plus a substitution row {literalMap, identMap, classRename, questId}. Within an equal-structure group the
Elyos and Asmodian files are paired in quest-id order.

The report splits the pairs by what G1 did with them: both transliterated (G2 unnecessary: the generator emits both files), both refused
(G2's case: hand-port one, clone the other), one of each (the refusal is in a literal-dependent part, e.g. an API only one twin calls;
worth reading). It also counts the N-way groups among the refused files: equal-structure groups of 2 or more refused files, which S1's
slot cloner (G2's N-way mode) clones from one hand-ported representative.
"""
from __future__ import annotations

import re
from collections import defaultdict
from functools import lru_cache
from pathlib import Path

from . import paths

import javasrc  # noqa: E402

RACE_WORDS = frozenset(['ELYOS', 'ASMODIANS', 'ASMODIAN', 'ELYO', 'PC_LIGHT', 'PC_DARK'])
_QUEST = re.compile(r'<quest\s[^>]*>')
_ATTR = re.compile(r'(\w+)="([^"]*)"')


@lru_cache(maxsize=2)
def races(xml=str(paths.QUEST_DATA_XML)):
    """quest id -> race_permitted (ELYOS, ASMODIANS, PC_ALL, ...) from quest_data.xml"""
    out = {}
    p = Path(xml)
    if not p.is_file():
        return out
    for m in _QUEST.finditer(p.read_text(encoding='utf-8', errors='replace')):
        a = dict(_ATTR.findall(m.group(0)))
        if 'id' in a and a['id'].isdigit():
            out[int(a['id'])] = a.get('race_permitted', 'PC_ALL')
    return out


def normalized(path):
    """(structure key, literal texts in token order) of a Java file: package and imports dropped, literals, the class name and race
    identifiers replaced by placeholders"""
    text = Path(path).read_text(encoding='utf-8-sig', errors='replace')
    T = javasrc.tokenize(text, str(path))
    cls = Path(path).stem
    key = []
    lits = []
    i = 0
    n = len(T.text) - 1
    # skip `package ...;` and `import ...;`
    while i < n and T.text[i] in ('package', 'import'):
        while i < n and T.text[i] != ';':
            i += 1
        i += 1
    for j in range(i, n):
        k, t = T.kind[j], T.text[j]
        if k in (javasrc.INT, javasrc.FLOAT, javasrc.STRING, javasrc.CHAR, javasrc.TEXTBLOCK):
            key.append('#' + k)
            lits.append(t)
        elif t == cls:
            key.append('$CLASS')
        elif t in RACE_WORDS:
            key.append('$RACE')
            lits.append(t)
        else:
            key.append(t)
    return '\x1f'.join(key), lits


def quest_id_of(path):
    m = re.match(r'_(\d+)', Path(path).stem)
    return int(m.group(1)) if m else None


def pairs_report(files, results_by_rel, quest_dir=None):
    quest_dir = Path(quest_dir or paths.JAVA_QUEST_DIR)
    race = races()
    groups = defaultdict(list)
    info = {}
    for f in files:
        f = Path(f)
        try:
            rel = f.resolve().relative_to(quest_dir.resolve()).as_posix()
        except ValueError:
            rel = f.name
        key, lits = normalized(f)
        qid = quest_id_of(f)
        info[rel] = (qid, race.get(qid, '?'), lits)
        groups[key].append(rel)
    pairs = []
    unique = 0
    for key, members in groups.items():
        ely = sorted((m for m in members if info[m][1] == 'ELYOS'), key=lambda m: info[m][0] or 0)
        asm = sorted((m for m in members if info[m][1] == 'ASMODIANS'), key=lambda m: info[m][0] or 0)
        if len(ely) == 1 and len(asm) == 1:
            unique += 1
        for e, a in zip(ely, asm):
            le, la = info[e][2], info[a][2]
            diff = sum(1 for x, y in zip(le, la) if x != y)
            pairs.append((e, a, diff))
    outcome = defaultdict(int)
    examples = defaultdict(list)
    for e, a, diff in pairs:
        re_, ra = results_by_rel.get(e), results_by_rel.get(a)
        se = re_.status if re_ else '?'
        sa = ra.status if ra else '?'
        if se == 'ok' and sa == 'ok':
            k = 'both transliterated (G1 emits both; G2 unnecessary)'
        elif se != 'ok' and sa != 'ok':
            k = 'both refused (G2: hand-port one, clone the other)'
        else:
            k = 'one transliterated, one refused'
        outcome[k] += 1
        if len(examples[k]) < 5:
            examples[k].append(f'{e} <-> {a} ({diff} literal slots differ)')
    # leading-digit mirrors (1xxx<->2xxx, 3xxx<->4xxx, 1xxxx<->2xxxx, ...) for comparison with the inventory's 66 / ~98
    by_id = {info[r][0]: r for r in info if info[r][0] is not None}
    digit_pairs = 0
    digit_twins = 0
    twin_set = {(e, a) for e, a, _ in pairs}
    for qid, rel in by_id.items():
        s = str(qid)
        if s[0] in '13':
            other = int(str(int(s[0]) + 1) + s[1:])
            if other in by_id:
                digit_pairs += 1
                if (rel, by_id[other]) in twin_set:
                    digit_twins += 1
    # N-way groups among refused files
    refused_groups = 0
    refused_files = 0
    for key, members in groups.items():
        ref = [m for m in members if results_by_rel.get(m) is not None and results_by_rel[m].status != 'ok']
        if len(ref) >= 2:
            refused_groups += 1
            refused_files += len(ref)
    return {
        'pairs': len(pairs),
        'uniquePairs': unique,
        'pairsInLargerGroups': len(pairs) - unique,
        'byOutcome': dict(sorted(outcome.items(), key=lambda kv: -kv[1])),
        'examples': dict(examples),
        'leadingDigitMirrors': digit_pairs,
        'leadingDigitMirrorsThatAreLiteralTwins': digit_twins,
        'refusedGroups': refused_groups,
        'refusedGroupFiles': refused_files,
        'refusedGroupClones': refused_files - refused_groups,
        'pairList': [{'elyos': e, 'asmodians': a, 'literalSlotsDiffering': d,
                      'outcome': ('ok' if results_by_rel.get(e) and results_by_rel[e].status == 'ok' else 'refused') + '/' +
                                 ('ok' if results_by_rel.get(a) and results_by_rel[a].status == 'ok' else 'refused')}
                     for e, a, d in sorted(pairs)],
    }
