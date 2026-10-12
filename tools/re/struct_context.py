"""Show the pseudo-C lines where a struct field appears.

Usage:
  python -I tools/re/struct_context.py build/decomp/mac_i386_all.c player 0x2408 0x9c 0xd4 [--n 6]
"""
import argparse
import re
from collections import defaultdict
from pathlib import Path

FUNC_HDR = re.compile(r'^// rva (0x[0-9a-f]+)\s+(\S+)', re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('decomp')
    ap.add_argument('struct')
    ap.add_argument('size')
    ap.add_argument('offsets', nargs='+')
    ap.add_argument('--n', type=int, default=6, help='lines per offset')
    args = ap.parse_args()
    size = int(args.size, 16)
    text = Path(args.decomp).read_text(encoding='utf-8', errors='replace')
    hdrs = list(FUNC_HDR.finditer(text))
    g = re.escape(args.struct)
    for off_s in args.offsets:
        off = int(off_s, 16)
        # Accepts "+ 0x9c" and "+ 156" right after the index * size.
        pat = re.compile(r'_?' + g + r'(?:_[0-9a-f]{8})? \+ [\w()]+ \* (?:0x%x|%d) \+ (?:0x%x|%d)\b' % (size, size, off, off))
        hits = defaultdict(list)
        for a, b in zip(hdrs, hdrs[1:] + [None]):
            body = text[a.end():b.start() if b else len(text)]
            for line in body.splitlines():
                if pat.search(line):
                    hits[a.group(2)].append(line.strip())
        print(f'== {args.struct} +0x{off:x}: {sum(map(len, hits.values()))} lines in {len(hits)} functions')
        shown = 0
        for fn, lines in sorted(hits.items(), key=lambda x: -len(x[1])):
            for ln in lines[:2]:
                print(f'   {fn[:28]:28} {ln[:150]}')
                shown += 1
            if shown >= args.n:
                break


if __name__ == '__main__':
    main()
