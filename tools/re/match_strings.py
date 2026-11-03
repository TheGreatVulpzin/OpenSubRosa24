"""Name unnamed functions by their set of strings.

match_functions.py weighs size and instruction features, so a function whose
size changed a lot between compilers stays open even when it holds the same
strings. This pass compares only the strings. A name is taken when the best
reference function shares at least 2 strings, the Jaccard score is at least
0.6, and the next different name is at least 0.2 lower.

Usage: python -I tools/re/match_strings.py --target build/features/client_win.jsonl \
    --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl \
    --names previous.csv --out result.csv
The result has the format of match_functions.py. Only open functions change.
"""
import argparse
import csv
import json

ap = argparse.ArgumentParser()
ap.add_argument('--target', required=True)
ap.add_argument('--ref', action='append', required=True)
ap.add_argument('--names', required=True, help='CSV of match_functions.py with the names found so far')
ap.add_argument('--out', required=True)
ap.add_argument('--min-jaccard', type=float, default=0.6)
ap.add_argument('--margin', type=float, default=0.2)
args = ap.parse_args()


def load(path):
    with open(path, encoding='utf-8') as fh:
        return [json.loads(line) for line in fh]


with open(args.names, encoding='utf-8') as fh:
    rows = list(csv.DictReader(fh))
fields = list(rows[0].keys())
used = {r['name'] for r in rows if r['name']}
by_rva = {r['rva']: r for r in rows}

refs = []
for path in args.ref:
    for f in load(path):
        # The Mach-O names have one more underscore than the ELF names.
        name = f['name'][1:] if 'mac' in path and f['name'].startswith('_') else f['name']
        if f['strings'] and not name.startswith(('GLee_Lazy', 'FUN_')):
            refs.append((name, set(f['strings'])))

taken = 0
for f in sorted(load(args.target), key=lambda f: -f['size']):
    row = by_rva[f['rva']]
    strings = set(f['strings'])
    if row['name'] or len(strings) < 2:
        continue
    best = {}
    for name, ref_strings in refs:
        if name in used:
            continue
        shared = len(strings & ref_strings)
        if shared >= 2:
            score = shared / len(strings | ref_strings)
            best[name] = max(best.get(name, 0), score)
    ranked = sorted(best.items(), key=lambda kv: -kv[1])
    if not ranked or ranked[0][1] < args.min_jaccard:
        continue
    if len(ranked) > 1 and ranked[0][1] - ranked[1][1] < args.margin:
        continue
    name, score = ranked[0]
    row.update(name=name, source='strings', confidence=round(score, 3), evidence=f'{len(strings)} strings, jaccard {score:.2f}')
    used.add(name)
    taken += 1

with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.DictWriter(fh, fieldnames=fields, lineterminator='\n')
    w.writeheader()
    w.writerows(rows)
print(f'{taken} names from strings')
