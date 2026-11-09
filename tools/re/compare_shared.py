"""Compare the machine code of the functions that the server and the client both have.

The server (024d) and the client (0.24b) come from the same source, but not from the same
revision. For each function with the same name in both executables, this script reads the bytes and
reports the size and the share of equal bytes at the same positions. Operands that hold addresses
differ in the two files, so a share near 0.7 or more with an equal size means the same compiled code.
A different size means a different code revision or a wrong name.

Usage: python -I tools/re/compare_shared.py [--out analysis/client_win/shared_code.csv]
Output: name, server_rva, client_rva, server_size, client_size, equal_share, verdict
Verdict: same (equal size, share 0.6 or more), near (size within 10 percent), different, wrong
(size ratio under 0.5: the name is probably wrong).
"""
import argparse
import csv
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[2]


def reader(path):
    data = Path(path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(n):
        off = pe + 24 + opt + i * 40
        size, addr, rawsize, rawoff = struct.unpack_from('<IIII', data, off + 8)
        sections.append((addr, size, data[rawoff:rawoff + rawsize]))

    def read(rva, size):
        for base, extent, raw in sections:
            if base <= rva and rva + size <= base + extent:
                return (raw[rva - base:rva - base + size] + bytes(size))[:size]
        return bytes(size)
    return read


def names(path):
    out = {}
    for r in csv.DictReader(open(path, encoding='utf-8')):
        if r['name']:
            out[r['name']] = (int(r['rva'], 16), int(r['size']))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(ROOT / 'analysis/client_win/shared_code.csv'))
    args = ap.parse_args()
    sread = reader(ROOT / 'original/blackhouser/subrosadedicated024d.exe')
    cread = reader(ROOT / 'original/client/subrosa.exe')
    sn = names(ROOT / 'analysis/024d/functions.csv')
    cn = names(ROOT / 'analysis/client_win/functions.csv')
    rows = []
    for name, (srva, ssize) in sn.items():
        if name not in cn:
            continue
        crva, csize = cn[name]
        a, b = sread(srva, ssize), cread(crva, csize)
        n = min(len(a), len(b))
        share = sum(1 for i in range(n) if a[i] == b[i]) / max(len(a), len(b), 1)
        ratio = min(ssize, csize) / max(ssize, csize, 1)
        if ssize == csize and share >= 0.6:
            verdict = 'same'
        elif ratio >= 0.9:
            verdict = 'near'
        elif ratio < 0.5:
            verdict = 'wrong'
        else:
            verdict = 'different'
        rows.append([name, hex(srva), hex(crva), ssize, csize, round(share, 3), verdict])
    rows.sort(key=lambda r: (r[6], r[0]))
    with open(args.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh)
        w.writerow(['name', 'server_rva', 'client_rva', 'server_size', 'client_size', 'equal_share', 'verdict'])
        w.writerows(rows)
    from collections import Counter
    print(dict(Counter(r[6] for r in rows)), len(rows), 'functions with the same name')


if __name__ == '__main__':
    main()
