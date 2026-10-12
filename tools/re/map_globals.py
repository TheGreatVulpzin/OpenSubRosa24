"""Map global variables of a binary without symbols from already matched functions.

For each pair of functions (target, reference) with the same name, each access
of the target to address A and each access of the reference to "symbol + offset"
cast a vote: "symbol starts at A - offset in the target". The right address
collects votes from many functions. The wrong ones stay scattered.

The i386 Mach-O is the best reference: 32-bit, so the struct offsets match
Windows. The x64 ELF comes in as a second opinion.

Usage:
  python -I tools/re/map_globals.py --target build/features/ded024d.jsonl \
      --names analysis/024d/functions.csv \
      --ref build/features/mac_i386.jsonl --ref build/features/linux_x64.jsonl \
      --out analysis/024d/globals.csv
"""
import argparse
import bisect
import csv
import json
import os
from collections import Counter, defaultdict

# Symbols that are not from the game.
SKIP = ('__', '_GLEE', 'GLee', '_glee', 'stbi', 'z_', 'completed.', 'dtor_idx', 'object.', '_DYNAMIC',
        '_GLOBAL_OFFSET', 'environ', 'stdout', 'stderr', 'stdin')


def load_features(path):
    funcs = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            n = f['name']
            if n.startswith('_') and not n.startswith('__'):
                n = n[1:]
            f['name'] = n
            funcs[n] = f
    return funcs


class Symbols:
    def __init__(self, path):
        rows = []
        with open(path, encoding='utf-8') as fh:
            for line in fh:
                rva, name = line.rstrip('\n').split('\t')
                if name.startswith('_') and not name.startswith('__'):
                    name = name[1:]
                rows.append((int(rva, 16), name))
        rows.sort()
        self.addrs = [r[0] for r in rows]
        self.names = [r[1] for r in rows]

    def containing(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return None
        name = self.names[i]
        if name.startswith(SKIP):
            return None
        return name, addr - self.addrs[i]

    def size(self, name):
        i = self.names.index(name)
        return self.addrs[i + 1] - self.addrs[i] if i + 1 < len(self.addrs) else 0x1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--names', required=True, help='matcher CSV with the target names')
    ap.add_argument('--ref', action='append', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument("--min-votes", type=float, default=3)
    args = ap.parse_args()

    target = {}
    with open(args.target, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            target[f['rva']] = f
    with open(args.names, encoding='utf-8') as fh:
        named = {r['rva']: r['name'] for r in csv.DictReader(fh) if r['name']}

    votes = defaultdict(Counter)  # symbol -> {base in the target: votes}
    sizes = {}
    origin = defaultdict(set)
    for ref_path in args.ref:
        ref = load_features(ref_path)
        syms = Symbols(ref_path + '.symbols.tsv')
        tag = 'mac' if 'mac' in ref_path else 'linux'
        ref_tag = os.path.basename(ref_path).split('.')[0]
        for rva, name in named.items():
            r = ref.get(name)
            if not r:
                continue
            tglob = {int(a, 16) for a in target[rva]['globals']}
            rglob = set()
            for a in r['globals']:
                c = syms.containing(int(a, 16))
                if c:
                    rglob.add(c)
            if not tglob or not rglob:
                continue
            # Each pair of functions votes at most once for each (symbol, base).
            # A right base gets votes from many different functions.
            # A wrong base is a coincidence and almost never repeats.
            pair_votes = set()
            for sym, off in rglob:
                for a in tglob:
                    pair_votes.add((sym, a - off))
                if sym not in sizes:
                    sizes[sym] = syms.size(sym)
            # The Mach-O is 32-bit like Windows: its offsets are the most reliable.
            weight = 1.5 if tag == 'mac' else 1.0
            for sym, base in pair_votes:
                votes[sym][base] += weight
                origin[sym].add(ref_tag)

    rows = []
    taken = {}
    for sym, cnt in votes.items():
        ranked = cnt.most_common()
        base, v = ranked[0]
        # Neighbors (+4, +8...) get votes from offset differences between 32 and 64 bits.
        # The real competitor is the best candidate far from the chosen base.
        second = next((c for b, c in ranked[1:] if abs(b - base) > 0x40), 0)
        if v >= args.min_votes and v >= 2 * second and base > 0:
            rows.append((base, sym, round(v, 2), round(second, 2), sizes[sym], '+'.join(sorted(origin[sym]))))

    # Resolve collisions: two symbols at the same address -> the one with more votes stays.
    rows.sort(key=lambda r: -r[2])
    final = []
    for r in rows:
        if r[0] in taken:
            continue
        taken[r[0]] = r[1]
        final.append(r)
    final.sort()

    with open(args.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'votes', 'second', 'ref_size', 'refs'])
        for base, sym, v, second, size, refs in final:
            w.writerow([f'0x{base:x}', sym, v, second, f'0x{size:x}', refs])
    print(f'{len(final)} globals mapped')


if __name__ == '__main__':
    main()
