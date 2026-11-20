"""List the client functions that are not adopted, by state.

States:
  pending   a clean function is written and listed in an adopted_*_pending.txt file
  unwritten no clean definition of the name exists in src outside src/lifted
  lifted    a clean definition exists, but no adoption list names it

Usage: python -I tools/re/client_left.py [--group C10] [--min-size 0]
The groups of C9 (GLee) are left out. The list is sorted by size, largest first.
"""
import argparse
import csv
import glob
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def adoption_names():
    adopted, pending = set(), set()
    for name in glob.glob(str(ROOT / 'analysis/client_win/adopted*.txt')):
        target = pending if name.endswith('_pending.txt') else adopted
        for line in open(name, encoding='utf-8'):
            line = line.strip()
            if line and not line.startswith('#'):
                target.add(line.split()[0])
    return adopted, pending


def defined_names():
    source = ''
    for path in (ROOT / 'src').rglob('*.c'):
        if 'lifted' in path.parts:
            continue
        source += path.read_text(encoding='utf-8', errors='replace') + '\n'
    return set(re.findall(r'^[A-Za-z_][^\n;{}]*?\b([A-Za-z_]\w*)\s*\(', source, flags=re.M))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--group')
    parser.add_argument('--min-size', type=int, default=0)
    args = parser.parse_args()
    adopted, pending = adoption_names()
    defined = defined_names()
    rows = list(csv.DictReader((ROOT / 'analysis/client_win/groups.csv').open(encoding='utf-8')))
    rows = [r for r in rows if r['group'] != 'C9' and (not args.group or r['group'] == args.group)]
    counts = {'adopted': 0, 'pending': 0, 'unwritten': 0, 'lifted': 0}
    listing = {'pending': [], 'unwritten': [], 'lifted': []}
    for row in rows:
        name, size = row['name'], int(row['size'])
        if name in adopted:
            counts['adopted'] += 1
        elif name in pending:
            state = 'pending'
        elif name not in defined:
            state = 'unwritten'
        else:
            state = 'lifted'
        if name not in adopted:
            counts[state] += 1
            if size >= args.min_size:
                listing[state].append((size, name, row['group'], row['recorded_calls']))
    print(counts)
    for state, items in listing.items():
        items.sort(reverse=True)
        print(f'== {state}: {len(items)} functions, {sum(i[0] for i in items)} bytes')
        print(', '.join(f'{n} {s} {g}' + (f' ({c}c)' if c not in ('', '0') else '') for s, n, g, c in items))


if __name__ == '__main__':
    main()
