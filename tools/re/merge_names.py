"""Merge matcher CSVs. The first CSV has priority. When two CSVs give
different names to the same function, the evidence records the divergence.

Usage: python -I tools/re/merge_names.py --out final.csv [--manual manual.csv] primary.csv secondary.csv [...]
In the manual CSV, the name "-" removes the matcher name.
"""
import argparse
import csv

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True)
ap.add_argument('--manual', help='CSV rva,name,evidence with hand-reviewed names (highest priority)')
ap.add_argument('inputs', nargs='+')
args = ap.parse_args()

tables = []
for p in args.inputs:
    with open(p, encoding='utf-8') as fh:
        tables.append(list(csv.DictReader(fh)))

final = {r['rva']: dict(r) for r in tables[0]}
used = {r['name'] for r in final.values() if r['name']}
for table in tables[1:]:
    for r in table:
        cur = final[r['rva']]
        if not r['name']:
            continue
        if not cur['name']:
            if r['name'] not in used:
                final[r['rva']] = dict(r)
                used.add(r['name'])
        elif cur['name'] != r['name'] and 'diverge' not in cur['evidence']:
            cur['evidence'] += f'; diverge: {r["name"]}'

if args.manual:
    with open(args.manual, encoding='utf-8') as fh:
        for m in csv.DictReader(fh):
            if m['name'] == '-':  # wrong matcher name: remove
                cur = final[m['rva']]
                cur.update(name='', source='manual', confidence='', evidence=f"removed: {cur['name']}. {m['evidence']}")
                continue
            for r in final.values():  # the name now belongs only to this function
                if r['name'] == m['name'] and r['rva'] != m['rva']:
                    r['name'], r['source'] = '', ''
            cur = final[m['rva']]
            old = cur['name']
            cur.update(name=m['name'], source='manual', confidence='1.0', evidence=m['evidence'])
            if old and old != m['name']:
                cur['evidence'] += f' (matcher said {old})'

with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.DictWriter(fh, fieldnames=list(tables[0][0].keys()), lineterminator='\n')
    w.writeheader()
    for rva in sorted(final, key=lambda r: int(r, 16)):
        w.writerow(final[rva])

named = sum(1 for r in final.values() if r['name'])
div = sum(1 for r in final.values() if 'diverge' in r['evidence'])
print(f'{named}/{len(final)} named, {div} with divergence')
