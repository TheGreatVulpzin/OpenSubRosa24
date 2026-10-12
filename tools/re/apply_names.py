"""Apply reviewed name proposals to analysis/024d/functions.csv and manual_names.csv.

Input: CSV files with the columns rva,name,confidence,evidence (other columns are ignored),
for example analysis/024d/names_proposed.csv and names_suspect.csv.

- functions.csv: the row of each RVA gets the name, source "review", the confidence and
  the evidence.
- manual_names.csv: proposals with a confidence >= --min-manual (default 0.85) are added
  or replaced, so a later run of tools/re/name_all.sh keeps them.

The script stops before it writes anything if two RVAs end up with the same name.

Usage: python -I tools/re/apply_names.py <proposals.csv> [...] [--min-manual 0.85]
"""
import argparse
import csv
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FUNCS = ROOT / 'analysis/024d/functions.csv'
MANUAL = ROOT / 'analysis/024d/manual_names.csv'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--min-manual', type=float, default=0.85)
    args = ap.parse_args()

    with open(FUNCS, encoding='utf-8') as fh:
        reader = csv.DictReader(fh)
        fields = reader.fieldnames
        rows = list(reader)
    by_rva = {r['rva']: r for r in rows}

    props = []
    for path in args.files:
        with open(path, encoding='utf-8') as fh:
            props += list(csv.DictReader(fh))
    missing = [p['rva'] for p in props if p['rva'] not in by_rva]
    if missing:
        raise SystemExit(f'unknown RVAs: {missing}')

    names = {r['rva']: r['name'] for r in rows}
    for p in props:
        names[p['rva']] = p['name']
    dups = [n for n, k in Counter(v for v in names.values() if v).items() if k > 1]
    if dups:
        raise SystemExit(f'duplicate names: {dups}')

    for p in props:
        r = by_rva[p['rva']]
        r['name'], r['source'], r['confidence'], r['evidence'] = p['name'], 'review', p['confidence'], p['evidence']
    with open(FUNCS, 'w', newline='', encoding='utf-8') as fh:
        w = csv.DictWriter(fh, fieldnames=fields, lineterminator='\n')
        w.writeheader()
        w.writerows(rows)

    with open(MANUAL, encoding='utf-8') as fh:
        manual = list(csv.DictReader(fh))
    keep = {p['rva']: p for p in props if float(p['confidence']) >= args.min_manual}
    manual = [m for m in manual if m['rva'] not in keep]
    manual += [{'rva': p['rva'], 'name': p['name'], 'evidence': p['evidence']} for p in keep.values()]
    with open(MANUAL, 'w', newline='', encoding='utf-8') as fh:
        w = csv.DictWriter(fh, fieldnames=['rva', 'name', 'evidence'], lineterminator='\n')
        w.writeheader()
        w.writerows(manual)
    print(f'{len(props)} names applied to functions.csv, {len(keep)} kept in manual_names.csv')


if __name__ == '__main__':
    main()
