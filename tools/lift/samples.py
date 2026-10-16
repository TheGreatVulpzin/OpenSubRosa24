"""Count the samples of the standalone sampling profiler (SV_SAMPLE) by function.

The first line of the samples file is "base <module base>". The .map file of the linker
gives the start of each function at the preferred base. A sample falls in the function with
the highest start below it.

Usage: python -I tools/lift/samples.py <samples.txt> [build/hook/subvulpes-server.map] [--top 30]
"""
import bisect
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load_map(path):
    text = Path(path).read_text(encoding='utf-8', errors='replace')
    pref = int(re.search(r'Preferred load address is ([0-9a-f]+)', text).group(1), 16)
    syms = []
    for m in re.finditer(r'^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+f?\s', text, re.M):
        syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return pref, syms


def main():
    argv = sys.argv[1:]
    if '--top' in argv:
        k = argv.index('--top')
        argv = argv[:k] + argv[k + 2:]
    args = argv
    top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 30
    samples = Path(args[0]).read_text().split()
    mapfile = args[1] if len(args) > 1 else ROOT / 'build/hook/subvulpes-server.map'
    pref, syms = load_map(mapfile)
    addrs = [s[0] for s in syms]
    base = int(samples[1], 16) if samples[0] == 'base' else pref
    counts = Counter()
    for s in samples[2:] if samples[0] == 'base' else samples:
        va = int(s, 16) - base + pref
        i = bisect.bisect_right(addrs, va) - 1
        counts[syms[i][1] if i >= 0 and va - addrs[i] < 0x100000 else f'outside {int(s, 16):08x}'] += 1
    total = sum(counts.values())
    print(f'{total} samples')
    for name, n in counts.most_common(top):
        print(f'{100 * n / total:6.1f}%  {n:7}  {name}')


if __name__ == '__main__':
    main()
