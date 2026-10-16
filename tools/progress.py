"""Phase 3 progress: how much of the 024d already runs in C.

Read the g_hooks table in hook/hooks_024d.c, the lifted table in src/lifted/lifted_table.c and
the sizes in analysis/024d/functions.csv. "Live" lifted functions replace the original in the
server (hook/hooks_lifted.c). "Harness only" functions have side effects: the live server keeps
the original, and only the offline harness runs the lifted body (docs/side-effects.md).
A function counts as "verified in the game" when docs/hooks.md marks **Verified** on its line.
When build/gate/ded024d/baseline.json exists, the harness lines come from the last gate.

Usage: python -I tools/progress.py [--md]
  --md  print the table in Markdown
"""
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    md = '--md' in sys.argv
    hooks_src = (ROOT / 'hook/hooks_024d.c').read_text(encoding='utf-8')
    hooked = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),', hooks_src))
    live = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),',
                           (ROOT / 'hook/hooks_lifted.c').read_text(encoding='utf-8')))
    lifted = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),',
                             (ROOT / 'src/lifted/lifted_table.c').read_text(encoding='utf-8')))
    side_only = {n: r for n, r in lifted.items() if n not in live}
    doc = (ROOT / 'docs/hooks.md').read_text(encoding='utf-8')
    verified = set(re.findall(r'^\| `(\w+)` \|[^\n]*\*\*Verified\*\*', doc, re.M))

    with open(ROOT / 'analysis/024d/functions.csv', encoding='utf-8') as fh:
        rows = list(csv.DictReader(fh))
    size = {int(r['rva'], 16): int(r['size']) for r in rows}
    total_funcs, total_bytes = len(rows), sum(size.values())

    hooked_bytes = sum(size.get(int(rva, 16), 0) for rva in hooked.values())
    verified_bytes = sum(size.get(int(hooked[n], 16), 0) for n in verified if n in hooked)

    def nbytes(names):
        return sum(size.get(int(rva, 16), 0) for rva in names.values())

    in_c = {**lifted, **hooked}
    lines = [
        ('Functions in C (hand-written)', len(hooked), total_funcs, nbytes(hooked), total_bytes),
        ('Functions in C (lifted, live)', len(live), total_funcs, nbytes(live), total_bytes),
        ('Functions in C (lifted, harness only)', len(side_only), total_funcs, nbytes(side_only), total_bytes),
        ('Functions in C (total)', len(in_c), total_funcs, nbytes(in_c), total_bytes),
        ('Functions verified in the game', len(verified & set(hooked)), total_funcs, verified_bytes, total_bytes),
    ]
    gate = ROOT / 'build/gate/ded024d/baseline.json'
    if gate.exists():
        runs = json.loads(gate.read_text(encoding='utf-8'))
        same = {n for r in runs.values() for n, f in r['funcs'].items() if f['same']}
        differ = {n for r in runs.values() for n, f in r['funcs'].items() if f['different'] or f['crashed']}
        tested = {n: r for n, r in in_c.items() if n in same}
        clean = {n: r for n, r in tested.items() if n not in differ}
        lines += [
            ('Functions in C with a "same" call', len(tested), total_funcs, nbytes(tested), total_bytes),
            ('... and no difference on any recording', len(clean), total_funcs, nbytes(clean), total_bytes),
        ]
    if md:
        print('| Measure | Functions | Code bytes |')
        print('| --- | --- | --- |')
        for label, n, tn, b, tb in lines:
            print(f'| {label} | {n} of {tn} ({100 * n / tn:.1f}%) | {b} of {tb} ({100 * b / tb:.2f}%) |')
    else:
        for label, n, tn, b, tb in lines:
            print(f'{label:40} {n:4}/{tn} functions ({100 * n / tn:5.1f}%)   {b:7}/{tb} bytes ({100 * b / tb:5.2f}%)')


if __name__ == '__main__':
    main()
