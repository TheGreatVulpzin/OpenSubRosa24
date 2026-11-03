"""Name open functions by the set of named functions they call.

match_functions.py weighs size and instruction features, so a big function whose
size changed between compilers stays open even when it calls the same named
functions. This pass compares the sets of callee names. A name is taken when
the best reference function shares at least 4 callees, the Jaccard score is at
least 0.7, the next different name is at least 0.2 lower, and the constants of
the two functions overlap. A score of 0.6 is enough when two references with
the same name agree.

Usage: python -I tools/re/match_callees.py --target build/features/client_win.jsonl \
    --names names.csv --ref build/features/linux_x64.jsonl \
    --ref build/features/ded024d.jsonl:analysis/024d/functions.csv --out result.csv
A reference with a CSV takes its names from the CSV. Without a CSV it takes the
names of the export (ELF symbols). The result has the format of match_functions.py.
Only open functions change.
"""
import argparse
import csv
import json

ap = argparse.ArgumentParser()
ap.add_argument('--target', required=True)
ap.add_argument('--names', required=True, help='CSV of match_functions.py with the names found so far')
ap.add_argument('--ref', action='append', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--min-callees', type=int, default=4)
ap.add_argument('--min-jaccard', type=float, default=0.7)
ap.add_argument('--margin', type=float, default=0.2)
ap.add_argument('--min-constants', type=float, default=0.2, help='Jaccard of the integer and float constants')
args = ap.parse_args()


def load(path):
    with open(path, encoding='utf-8') as fh:
        return [json.loads(line) for line in fh]


def callee_names(funcs, name_of):
    out = {}
    for f in funcs:
        names = set()
        for c in f['calls']:
            if c.startswith('EXT:'):
                names.add(c)
            else:
                n = name_of.get(int(c, 16))
                if n:
                    names.add(n)
        out[f['rva']] = names
    return out


def constants(f):
    return {str(v) for v in f['ints']} | {str(v) for v in f['floats']}


with open(args.names, encoding='utf-8') as fh:
    rows = list(csv.DictReader(fh))
fields = list(rows[0].keys())
by_rva = {r['rva']: r for r in rows}
target = load(args.target)
target_names = {int(r['rva'], 16): r['name'] for r in rows if r['name']}
target_calls = callee_names(target, target_names)
used = set(target_names.values())

refs = []  # (name, callee set, constants, size)
for spec in args.ref:
    path, _, csv_path = spec.partition(':')
    funcs = load(path)
    if csv_path:
        with open(csv_path, encoding='utf-8') as fh:
            name_of = {int(r['rva'], 16): r['name'] for r in csv.DictReader(fh) if r['name']}
    else:
        name_of = {int(f['rva'], 16): f['name'] for f in funcs if not f['name'].startswith('FUN_')}
    calls = callee_names(funcs, name_of)
    for f in funcs:
        name = name_of.get(int(f['rva'], 16))
        if name and not name.startswith(('GLee', '__GLee')):
            refs.append((name, calls[f['rva']], constants(f), f['size']))

taken = []
for f in sorted(target, key=lambda f: -f['size']):
    row = by_rva[f['rva']]
    mine = target_calls[f['rva']]
    if row['name'] or len(mine) < args.min_callees:
        continue
    my_constants = constants(f)
    best = {}
    for name, theirs, their_constants, size in refs:
        if name in used or len(theirs) < args.min_callees:
            continue
        shared = len(mine & theirs)
        if shared < args.min_callees:
            continue
        score = shared / len(mine | theirs)
        union = my_constants | their_constants
        const_score = len(my_constants & their_constants) / len(union) if union else 1.0
        if const_score < args.min_constants:
            continue
        entry = best.setdefault(name, [0, 0, 0])
        if score > entry[0]:
            entry[0], entry[1] = score, const_score
        entry[2] += 1
    ranked = sorted(best.items(), key=lambda kv: -kv[1][0])
    if not ranked:
        continue
    name, (score, const_score, agree) = ranked[0]
    margin_ok = len(ranked) == 1 or score - ranked[1][1][0] >= args.margin
    if not margin_ok:
        continue
    if score < args.min_jaccard and not (score >= 0.6 and agree >= 2):
        continue
    taken.append((score, f['rva'], f['size'], name, const_score))

# A reference name goes to the function with the best score.
taken.sort(reverse=True)
final = {}
for score, rva, size, name, const_score in taken:
    if name in final:
        continue
    final[name] = (rva, score, const_score, size)
for name, (rva, score, const_score, size) in final.items():
    by_rva[rva].update(name=name, source='callees', confidence=round(score, 3),
                       evidence=f'callee jaccard {score:.2f}, constants {const_score:.2f}, size {size}')
    print(rva, size, name, round(score, 2), round(const_score, 2))

with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.DictWriter(fh, fieldnames=fields, lineterminator='\n')
    w.writeheader()
    w.writerows(rows)
print(f'{len(final)} names from callees')
