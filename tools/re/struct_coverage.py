"""Measure how many struct accesses in the server code fall in named fields.

Server = functions that exist in the Linux 0.24c server (with symbols).
Accesses = count of reads + writes in analysis/structs/mac/<struct>.csv,
only from the server functions.

Usage: python -I tools/re/struct_coverage.py [--top 15]
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def server_functions():
    names = set()
    with open(ROOT / 'build/features/ded024c_linux.jsonl', encoding='utf-8') as fh:
        for line in fh:
            names.add(json.loads(line)['name'])
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--top', type=int, default=10, help='show the N most used unnamed offsets')
    args = ap.parse_args()
    server = server_functions()

    named = {}
    with open(ROOT / 'analysis/structs/names.csv', encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            named.setdefault(r['struct'], []).append((int(r['offset'], 16), r['type'], int(r['count'] or 1)))
    size = {'int': 4, 'uint32_t': 4, 'float': 4, 'char': 1, 'int16_t': 2, 'vec3': 12, 'Bone': 0xe0,
            'InventorySlot': 0x24}

    total_all = named_all = 0
    for p in sorted((ROOT / 'analysis/structs/mac').glob('*.csv')):
        s = p.stem
        ranges = [(o, o + size.get(t, 4) * c) for o, t, c in named.get(s, [])]
        tot = nm = 0
        missing = Counter()
        with open(p, encoding='utf-8') as fh:
            for r in csv.DictReader(fh):
                funcs = [f.lstrip('_') for f in r['funcs'].split()]
                if not any(f in server for f in funcs):
                    continue
                n = int(r['reads']) + int(r['writes'])
                off = int(r['offset'], 16)
                tot += n
                if any(a <= off < b for a, b in ranges):
                    nm += n
                else:
                    missing[off] += n
        total_all += tot
        named_all += nm
        pct = 100 * nm / tot if tot else 100
        top = ', '.join(f'+0x{o:x}({n})' for o, n in missing.most_common(args.top))
        print(f'{s:11} {nm:5}/{tot:5} = {pct:5.1f}%   unnamed: {top}')
    print(f'{"TOTAL":11} {named_all:5}/{total_all:5} = {100 * named_all / total_all:5.1f}%')


if __name__ == '__main__':
    main()
