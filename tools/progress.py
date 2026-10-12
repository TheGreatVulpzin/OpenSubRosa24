"""Phase 3 progress: how much of the 024d already runs in C.

Read the g_hooks table in hook/hooks_024d.c and the sizes in analysis/024d/functions.csv.
A function counts as "verified" when docs/hooks.md marks **Verified** on its line.

Usage: python -I tools/progress.py [--md]
  --md  print the table in Markdown
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    md = '--md' in sys.argv
    hooks_src = (ROOT / 'hook/hooks_024d.c').read_text(encoding='utf-8')
    hooked = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),', hooks_src))
    lifted_path = ROOT / 'hook/hooks_lifted.c'
    lifted = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),', lifted_path.read_text(encoding='utf-8')))         if lifted_path.exists() else {}
    doc = (ROOT / 'docs/hooks.md').read_text(encoding='utf-8')
    verified = set(re.findall(r'^\| `(\w+)` \|[^\n]*\*\*Verified\*\*', doc, re.M))

    with open(ROOT / 'analysis/024d/functions.csv', encoding='utf-8') as fh:
        rows = list(csv.DictReader(fh))
    size = {int(r['rva'], 16): int(r['size']) for r in rows}
    total_funcs, total_bytes = len(rows), sum(size.values())

    hooked_bytes = sum(size.get(int(rva, 16), 0) for rva in hooked.values())
    verified_bytes = sum(size.get(int(hooked[n], 16), 0) for n in verified if n in hooked)

    lifted_bytes = sum(size.get(int(rva, 16), 0) for rva in lifted.values())
    lines = [
        ('Functions in C (hand-written)', len(hooked), total_funcs, hooked_bytes, total_bytes),
        ('Functions in C (lifted)', len(lifted), total_funcs, lifted_bytes, total_bytes),
        ('Functions in C (total)', len(hooked) + len(lifted), total_funcs, hooked_bytes + lifted_bytes, total_bytes),
        ('Functions verified in the game', len(verified & set(hooked)), total_funcs, verified_bytes, total_bytes),
    ]
    if md:
        print('| Measure | Functions | Code bytes |')
        print('| --- | --- | --- |')
        for label, n, tn, b, tb in lines:
            print(f'| {label} | {n} of {tn} ({100 * n / tn:.1f}%) | {b} of {tb} ({100 * b / tb:.2f}%) |')
    else:
        for label, n, tn, b, tb in lines:
            print(f'{label:32} {n:4}/{tn} functions ({100 * n / tn:5.1f}%)   {b:7}/{tb} bytes ({100 * b / tb:5.2f}%)')


if __name__ == '__main__':
    main()
