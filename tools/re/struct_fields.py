"""Collect struct fields from the Ghidra pseudo-C.

Look for accesses to global arrays of structs, such as:
  *(int *)(player + (long)i * 0x2408 + 0x24)          offset in bytes
  (&_player)[i * 0x902 + 9]                            offset in elements
  lVar5 = (long)i * 0x2408; ... *(float *)(player + lVar5 + 0x28)

For each (global, record size) it collects: offset, types seen, reads,
writes and the functions that use it.

Usage:
  python -I tools/re/struct_fields.py build/decomp/mac_i386_all.c \
      --struct player=0x2408 --struct human=0x2b64 --out analysis/structs/mac
  python -I tools/re/struct_fields.py build/decomp/mac_i386_all.c --discover
"""
import argparse
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

FUNC_HDR = re.compile(r'^// rva (0x[0-9a-f]+)\s+(\S+)', re.M)
NUM = r'(?:0x[0-9a-f]+|\d+)'
CAST = r'(?:\*\((?P<type>[\w ]+?) ?\*\))?'
GLOB = r'&?_?(?P<g>[a-zA-Z_]\w*)'

# 1. global + idx * stride + off
P_BYTES = re.compile(CAST + r'\(?' + GLOB + r' \+ (?:\(long\))?(?:\(\w+\))?(?P<idx>\w+) \* (?P<stride>' + NUM + r') \+ (?P<off>' + NUM + r')\)?')
# 2. (&global)[idx * k + m]  -> elements
P_ELEMS = re.compile(r'\(&_?(?P<g>[a-zA-Z_]\w*)\)\[(?:\(long\))?(?P<idx>\w+) \* (?P<k>' + NUM + r') \+ (?P<m>' + NUM + r')\]')
# 3. var = (long)idx * stride ;   global + var + off
P_VAR = re.compile(r'(?P<var>[a-zA-Z_]\w*) = (?:\(long\))?(?:\(\w+\))?\w+ \* (?P<stride>' + NUM + r');')
P_VARUSE = re.compile(CAST + r'\(?' + GLOB + r' \+ (?P<var>[a-zA-Z_]\w*) \+ (?P<off>' + NUM + r')\)?')
# Write: the access is on the left of a plain "=".
ASSIGN_AFTER = re.compile(r'^\s*\)*\s*=[^=]')

# Element size in P_ELEMS from the type Ghidra gave to the global.
ELEM_SIZE = {'char': 1, 'undefined1': 1, 'byte': 1, 'short': 2, 'undefined2': 2, 'ushort': 2,
             'int': 4, 'uint': 4, 'float': 4, 'undefined4': 4, 'long': 8, 'double': 8, 'undefined8': 8}


def gname(g):
    """PTR__player_0019cc10 (Mach-O, access through a pointer) -> player."""
    g = re.sub(r'^PTR__+', '', g)
    return re.sub(r'_[0-9a-f]{8}$', '', g)


def num(s):
    return int(s, 16) if s.startswith('0x') else int(s)


def functions(text):
    hdrs = list(FUNC_HDR.finditer(text))
    for a, b in zip(hdrs, hdrs[1:] + [None]):
        yield a.group(2), text[a.end():b.start() if b else len(text)]


def scan(text, structs):
    """structs: {global: size}. Returns {global: {offset: data}}."""
    out = defaultdict(lambda: defaultdict(lambda: {'types': Counter(), 'reads': 0, 'writes': 0, 'funcs': set()}))

    def add(g, off, typ, fname, body, end):
        size = structs[g]
        off %= size  # access to element i+1 (unrolled loop) falls in the same field
        e = out[g][off]
        e['types'][typ or '?'] += 1
        e['funcs'].add(fname)
        if ASSIGN_AFTER.match(body[end:end + 8]):
            e['writes'] += 1
        else:
            e['reads'] += 1

    for fname, body in functions(text):
        for m in P_BYTES.finditer(body):
            g = gname(m.group('g'))
            if g in structs and num(m.group('stride')) == structs[g]:
                add(g, num(m.group('off')), m.group('type'), fname, body, m.end())
        for m in P_ELEMS.finditer(body):
            g = gname(m.group('g'))
            if g in structs:
                k, mm = num(m.group('k')), num(m.group('m'))
                if structs[g] % k == 0:
                    es = structs[g] // k
                    add(g, mm * es, {1: 'char', 2: 'short', 4: 'int', 8: 'long'}.get(es, '?'), fname, body, m.end())
        vars_ = {m.group('var'): num(m.group('stride')) for m in P_VAR.finditer(body)}
        for m in P_VARUSE.finditer(body):
            g, v = gname(m.group('g')), m.group('var')
            if g in structs and vars_.get(v) == structs[g]:
                add(g, num(m.group('off')), m.group('type'), fname, body, m.end())
    return out


def discover(text):
    """List the most frequent (global, stride)."""
    c = Counter()
    for m in P_BYTES.finditer(text):
        c[(gname(m.group('g')), num(m.group('stride')))] += 1
    for m in P_ELEMS.finditer(text):
        c[(gname(m.group('g')), f"{m.group('k')} elems")] += 1
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('decomp')
    ap.add_argument('--struct', action='append', default=[], help='global=size')
    ap.add_argument('--discover', action='store_true')
    ap.add_argument('--out', help='output folder for the CSVs')
    args = ap.parse_args()
    text = Path(args.decomp).read_text(encoding='utf-8', errors='replace')

    if args.discover:
        for (g, s), n in discover(text).most_common(60):
            print(f'{n:6} {g} {s if isinstance(s, str) else hex(s)}')
        return

    structs = {}
    for s in args.struct:
        g, size = s.split('=')
        structs[g] = num(size)
    res = scan(text, structs)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for g, fields in res.items():
        with open(out / f'{g}.csv', 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, lineterminator='\n')
            w.writerow(['offset', 'types', 'reads', 'writes', 'n_funcs', 'funcs'])
            for off in sorted(fields):
                e = fields[off]
                types = ' '.join(f'{t}:{n}' for t, n in e['types'].most_common())
                funcs = sorted(e['funcs'])
                w.writerow([f'0x{off:x}', types, e['reads'], e['writes'], len(funcs), ' '.join(funcs[:12])])
        print(f'{g}: {len(fields)} fields')


if __name__ == '__main__':
    main()
