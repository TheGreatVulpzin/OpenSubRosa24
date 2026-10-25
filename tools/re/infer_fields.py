"""Turn the field accesses of instrumentation/ghidra/InferFields.java into typed fields.

For each (global, offset, size) the instructions vote: float, double, fint (FPU integer),
signed, unsigned or int (no sign information). The rules:
  only float (4 bytes)             -> float
  only double (8 bytes)            -> double
  float together with int votes    -> no type (raw bytes: a copy of a float is often an
                                      integer MOV, so a float vote with MOVs is still a
                                      float, but a float vote with a compare is not)
  integer votes, 4 bytes           -> int (uint32_t when only "unsigned")
  integer votes, 2 bytes           -> int16_t, 1 byte -> char (uint8_t when only unsigned)
An offset with two access sizes, or with float and signed/unsigned/fint votes, is
"mixed" and gets no field: it shows in the report for a person to check.

Output: analysis/structs/024d/<global>.csv (offset, type, uses, n_funcs, funcs) for
tools/re/gen_types.py, which makes unk_XXXX fields from it. --test compares the result with
the fields of analysis/structs/names.csv and prints each disagreement.

Usage:
  python -I tools/re/infer_fields.py [--uses build/re/field_uses.tsv] [--test]
"""
import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'analysis/structs/024d'
INT_KINDS = {'fint', 'signed', 'unsigned', 'int'}


def decide(sizes, kinds):
    """(type, why) for one offset. type None: no field."""
    if len(sizes) != 1:
        return None, 'mixed sizes ' + '/'.join(str(s) for s in sorted(sizes))
    size = next(iter(sizes))
    has_float = kinds['float'] or kinds['double']
    strong_int = kinds['fint'] or kinds['signed'] or kinds['unsigned']
    if has_float and strong_int:
        return None, 'mixed float and integer'
    if kinds['double'] and size == 8:
        return 'double', ''
    if kinds['float'] and size == 4:
        return 'float', ''
    if has_float:
        return None, f'float vote on {size} bytes'
    if size == 4:
        return ('uint32_t' if kinds['unsigned'] and not kinds['signed'] else 'int'), ''
    if size == 2:
        return 'int16_t', ''
    if size == 1:
        return ('uint8_t' if kinds['unsigned'] and not kinds['signed'] else 'char'), ''
    return None, f'size {size}'


def load(path):
    uses = defaultdict(lambda: {'sizes': set(), 'kinds': Counter(), 'funcs': set(), 'n': 0})
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            g, off, size, kind, func = line.rstrip('\n').split('\t')
            u = uses[g, int(off, 16)]
            u['sizes'].add(int(size))
            u['kinds'][kind] += 1
            u['funcs'].add(func)
            u['n'] += 1
    return uses


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--uses', default=str(ROOT / 'build/re/field_uses.tsv'))
    ap.add_argument('--test', action='store_true', help='compare with the fields of names.csv')
    a = ap.parse_args()
    uses = load(a.uses)
    result = {k: decide(u['sizes'], u['kinds']) for k, u in uses.items()}

    if a.test:
        known = {}
        with open(ROOT / 'analysis/structs/names.csv', encoding='utf-8') as fh:
            for r in csv.DictReader(fh):
                if not r['count']:
                    known[r['struct'], int(r['offset'], 16)] = (r['name'], r['type'])
        same = differ = 0
        for k, (name, typ) in sorted(known.items()):
            if k not in result or result[k][0] is None:
                continue
            got = result[k][0]
            ok = got == typ or {got, typ} <= {'int', 'uint32_t'} or {got, typ} <= {'char', 'uint8_t'}
            same += ok
            differ += not ok
            if not ok:
                print(f'  {k[0]}+0x{k[1]:x} {name}: names.csv {typ}, inferred {got} {dict(uses[k]["kinds"])}')
        print(f'test: {same} fields agree, {differ} differ')
        return 0

    by_glob = defaultdict(list)
    mixed = []
    for (g, off), (typ, why) in result.items():
        u = uses[g, off]
        if typ is None:
            mixed.append((g, off, why, dict(u['kinds'])))
            continue
        by_glob[g].append((off, typ, u['n'], len(u['funcs']), ' '.join(sorted(u['funcs']))))
    OUT.mkdir(parents=True, exist_ok=True)
    for g, rows in by_glob.items():
        with open(OUT / f'{g}.csv', 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, lineterminator='\n')
            w.writerow(['offset', 'type', 'uses', 'n_funcs', 'funcs'])
            for off, typ, n, nf, funcs in sorted(rows):
                w.writerow([f'0x{off:x}', typ, n, nf, funcs])
    print(f'{sum(len(r) for r in by_glob.values())} typed offsets in {len(by_glob)} globals -> {OUT}')
    print(f'{len(mixed)} offsets without a type (check by hand):')
    for g, off, why, kinds in sorted(mixed)[:40]:
        print(f'  {g}+0x{off:x}: {why} {kinds}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
