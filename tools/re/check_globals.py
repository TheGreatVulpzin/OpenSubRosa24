"""Check the global candidates of map_globals.py against the code of the exe.

Two kinds of evidence, both from the exe itself:
- layout: the candidate ends exactly where the next candidate starts (or the
  reverse). Sizes come from the reference binary, so an exact chain is strong.
- access: the code has many 32-bit immediates inside the candidate range.

Status: confirmed (layout), likely (strong votes, size fits, many accesses),
unverified (strong votes, size fits, few accesses), doubtful (the rest).

Usage: python -I tools/re/check_globals.py --exe original/client/subrosa.exe \
    --globals analysis/client_win/globals.csv --out analysis/client_win/globals_checked.csv
"""
import argparse
import array
import bisect
import csv
import struct

ap = argparse.ArgumentParser()
ap.add_argument('--exe', required=True)
ap.add_argument('--globals', required=True)
ap.add_argument('--review', help='CSV rva,name,status,evidence: hand-reviewed rows replace the computed status')
ap.add_argument('--out', required=True)
args = ap.parse_args()

data = open(args.exe, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
text = None
for i in range(nsec):
    name, vsize, va, rsize, roff = struct.unpack_from('<8sIIII', data, pe + 24 + opt_size + 40 * i)
    if name.rstrip(bytes(1)) == b'.text':
        text = data[roff:roff + rsize]

# Every 32-bit value in the code that points into the data sections, at any alignment.
keys = []
for shift in range(4):
    chunk = text[shift:len(text) - ((len(text) - shift) % 4)]
    words = array.array('I')
    words.frombytes(chunk)
    keys.extend(v - base for v in words if 0x110000 <= v - base < 0x40000000)
keys.sort()

with open(args.globals, encoding='utf-8') as fh:
    rows = sorted(csv.DictReader(fh), key=lambda r: int(r['rva'], 16))

out = []
for k, r in enumerate(rows):
    start = int(r['rva'], 16)
    size = int(r['ref_size'], 16)
    votes, second = float(r['votes']), float(r['second'])
    next_start = int(rows[k + 1]['rva'], 16) if k + 1 < len(rows) else None
    prev_end = int(rows[k - 1]['rva'], 16) + int(rows[k - 1]['ref_size'], 16) if k else None
    layout = start + size == next_start or prev_end == start
    fits = next_start is None or start + size <= next_start
    lo = bisect.bisect_left(keys, start)
    hi = bisect.bisect_left(keys, start + size)
    inside = keys[lo:hi]
    exact = sum(1 for v in inside if v == start)
    reach = max((v - start for v in inside), default=0)
    strong = votes >= 10 and votes >= 2 * second and fits
    busy = len(inside) >= 30 and (reach * 2 >= size or (size <= 8 and exact >= 30))
    if layout:
        status = 'confirmed'
    elif strong and busy:
        status = 'likely'
    elif strong:
        status = 'unverified'
    else:
        status = 'doubtful'
    evidence = (f'votes {votes:g} (second {second:g}); layout {"exact" if layout else "none"}; '
                f'size {"fits" if fits else "runs into the next candidate"}; '
                f'{len(inside)} code references, reach {hex(reach)} of {hex(size)}')
    out.append((r['rva'], r['name'], status, evidence))

if args.review:
    with open(args.review, encoding='utf-8') as fh:
        review = {r['rva']: r for r in csv.DictReader(fh)}
    out = [(rva, name, review[rva]['status'], review[rva]['evidence']) if rva in review else (rva, name, status, ev)
           for rva, name, status, ev in out]

with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.writer(fh, lineterminator='\n')
    w.writerow(['rva', 'name', 'status', 'evidence'])
    w.writerows(out)
counts = {}
for row in out:
    counts[row[2]] = counts.get(row[2], 0) + 1
print(counts)
