"""Map the unnamed globals of the 024d to symbols by the functions that use them.

map_globals.py needs the same offset in both binaries. That fails for scalars that only
one function touches, and for symbols whose layout differs between 32 and 64 bits. This
tool compares sets instead: the 024d functions that touch an address, and the reference
functions (same names) that touch a symbol. An address and a symbol that the same
functions use are the same variable.

Steps:
1. Known regions (globals.csv, manual_globals.csv and --known) are taken out.
2. For each other .data address A of the 024d: F(A) = names of the functions that use it.
3. For each data symbol S of the references: G(S) = names of the functions that use it,
   limited to the names that the 024d also has.
4. score(A, S) = |F & G| / |F | G|. Each address takes its best symbol when the score
   is high enough and clearly above the second best (--margin).
   With --gap, near addresses form one block first. A block has more using functions, so
   its score is more reliable than the score of one address.
5. For each symbol: its addresses must fit in the symbol size. The base is the lowest
   address, so it is a lower bound when the code never touches offset 0.

Usage:
  python -I tools/re/map_regions.py --out build/re/regions.csv [--min-score 0.5]
"""
import argparse
import bisect
import csv
import json
import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FEAT = ROOT / 'build/features'
SERVER_ELF = ROOT / 'original/vanilla/0.24c_Dedicated_Linux/Sub Rosa 0.24c Dedicated Linux/subrosadedicated024c'
REFS = ['mac_i386', 'linux_x64', 'ded024c_linux', 'ded024b_linux']
DATA_START, DATA_END = 0x60000, 0x25ef97e0


def elf_data_symbols(path):
    """{name: size} of the .data/.bss objects of a 64-bit ELF."""
    d = path.read_bytes()
    shoff = struct.unpack_from('<Q', d, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3a)
    secs = [struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize) for i in range(shnum)]
    sh = secs[shstrndx]

    def cstr(base, off):
        return d[base + off:d.index(b'\0', base + off)].decode()

    names = {i: cstr(sh[4], s[0]) for i, s in enumerate(secs)}
    sym = next(s for s in secs if s[1] == 2)
    strtab = secs[sym[6]]
    out = {}
    for i in range(sym[5] // 24):
        st_name, info, _o, shn, _v, size = struct.unpack_from('<IBBHQQ', d, sym[4] + i * 24)
        if info & 0xf == 1 and names.get(shn) in ('.data', '.bss'):
            out[cstr(strtab[4], st_name)] = size
    return out


class Symbols:
    def __init__(self, path):
        rows = []
        for line in path.read_text(encoding='utf-8').splitlines():
            rva, name = line.split('\t')
            if name.startswith('_') and not name.startswith('__'):
                name = name[1:]
            rows.append((int(rva, 16), name))
        rows.sort()
        self.addrs = [r[0] for r in rows]
        self.names = [r[1] for r in rows]

    def containing(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        return self.names[i] if i >= 0 else None

    def span(self, name):
        i = self.names.index(name)
        return self.addrs[i + 1] - self.addrs[i] if i + 1 < len(self.addrs) else 0


def strip(n):
    return n[1:] if n.startswith('_') and not n.startswith('__') else n


def load(path):
    with open(path, encoding='utf-8') as fh:
        return [json.loads(line) for line in fh]


def known_regions(extra):
    rows = []
    for p in ('analysis/024d/globals.csv', 'analysis/024d/manual_globals.csv'):
        with open(ROOT / p, encoding='utf-8') as fh:
            for r in csv.DictReader(fh):
                rows.append((r['name'], int(r['rva'], 16)))
    for item in extra:
        name, rva = item.split('=')
        rows.append((name, int(rva, 16)))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--min-score', type=float, default=0.5)
    ap.add_argument('--margin', type=float, default=0.2, help='best score minus second score, at least')
    ap.add_argument('--min-funcs', type=int, default=2, help='an address needs this many using functions')
    ap.add_argument('--known', action='append', default=[], help='name=0xrva, a region known from a doc')
    ap.add_argument('--gap', type=lambda x: int(x, 0), default=0,
                    help='join addresses less than this far apart into one block, scored as a whole')
    ap.add_argument('--test', action='store_true', help='run on the known regions and report the hits')
    a = ap.parse_args()

    server = elf_data_symbols(SERVER_ELF)
    with open(ROOT / 'analysis/024d/functions.csv', encoding='utf-8') as fh:
        name_of = {int(r['rva'], 16): r['name'] for r in csv.DictReader(fh) if r['name']}
    names024 = set(name_of.values())

    # Reference: symbol -> functions that use it (only names the 024d has).
    users = defaultdict(set)
    mac_span = {}
    for ref in REFS:
        syms = Symbols(FEAT / f'{ref}.jsonl.symbols.tsv')
        for f in load(FEAT / f'{ref}.jsonl'):
            fn = strip(f['name'])
            if fn not in names024:
                continue
            for g in f['globals']:
                s = syms.containing(int(g, 16))
                if s in server:
                    users[s].add(fn)
                    if ref == 'mac_i386' and s not in mac_span:
                        mac_span[s] = syms.span(s)

    # 024d: address -> functions that use it.
    known = known_regions(a.known)
    known_names = {n for n, _ in known}
    size = {s: (mac_span.get(s) or server[s]) for s in server}
    regions = sorted((rva, rva + max(size.get(n, 4), 4), n) for n, rva in known)
    starts = [r[0] for r in regions]

    def in_known(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return regions[i][2] if i >= 0 and addr < regions[i][1] else None

    uses = defaultdict(set)
    for f in load(FEAT / 'ded024d.jsonl'):
        fn = name_of.get(int(f['rva'], 16))
        if not fn:
            continue
        for g in f['globals']:
            addr = int(g, 16)
            if DATA_START <= addr < DATA_END:
                uses[addr].add(fn)

    if a.test:
        cands = {s for s in users if s in known_names}
        addrs = {ad: fs for ad, fs in uses.items() if in_known(ad)}
    else:
        cands = {s for s in users if s not in known_names}
        addrs = {ad: fs for ad, fs in uses.items() if not in_known(ad)}

    members = {ad: [ad] for ad in addrs}
    if a.gap:
        blocks = []
        for ad in sorted(addrs):
            if blocks and ad - blocks[-1][-1] < a.gap:
                blocks[-1].append(ad)
            else:
                blocks.append([ad])
        members = {b[0]: b for b in blocks}
        addrs = {b[0]: set().union(*(addrs[x] for x in b)) for b in blocks}

    best = {}
    for ad, fs in addrs.items():
        if len(fs) < a.min_funcs:
            continue
        scored = sorted(((len(fs & users[s]) / len(fs | users[s]), s) for s in cands), reverse=True)
        second = scored[1][0] if len(scored) > 1 else 0
        if scored and scored[0][0] >= a.min_score and scored[0][0] - second >= a.margin:
            best[ad] = (scored[0][1], scored[0][0], second)

    if a.test:
        hit = sum(1 for ad, (s, _sc, _se) in best.items() if in_known(ad) == s)
        print(f'test: {len(best)} addresses scored, {hit} given the right symbol')
        for ad, (s, sc, se) in sorted(best.items()):
            if in_known(ad) != s:
                print(f'  miss 0x{ad:x} {in_known(ad)} -> {s} ({sc:.2f}, second {se:.2f})')
        return

    by_sym = defaultdict(list)
    for ad, (s, sc, se) in best.items():
        by_sym[s] += [(m, sc, se) for m in members[ad]]
    rows = []
    for s, lst in by_sym.items():
        lst.sort()
        base, top = lst[0][0], lst[-1][0]
        fits = top - base < max(size[s], 4)
        rows.append((base, s, len(lst), round(min(x[1] for x in lst), 2), round(max(x[2] for x in lst), 2),
                     f'0x{size[s]:x}', f'0x{top - base:x}', 'yes' if fits else 'NO', len(users[s])))
    rows.sort()
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'addresses', 'min_score', 'max_second', 'size_ref', 'span_024d', 'fits',
                    'ref_users'])
        for r in rows:
            w.writerow([f'0x{r[0]:x}', *r[1:]])
    print(f'{len(rows)} symbols for {len(best)} of {len(addrs)} unknown addresses -> {out}')


if __name__ == '__main__':
    main()
