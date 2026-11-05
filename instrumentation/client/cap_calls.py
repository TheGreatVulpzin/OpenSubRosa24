"""Write sv_args.min.txt: the first calls of each function of a recording.

instrumentation/lift/min_calls.py picks calls by code path and needs a block file of the
binary. The client has none yet, so this script keeps the first N calls of each function
(default 2). A call starts at an "R" line and ends at its "Z" line. The lines between belong to
the call (the "E" lines of the imports).

Usage: python -I instrumentation/client/cap_calls.py <recording folder> [N]
"""
import sys
from collections import Counter
from pathlib import Path


def main():
    folder = Path(sys.argv[1])
    keep = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    seen = Counter()
    out, group, rva, total = [], [], None, 0
    with open(folder / 'sv_args.txt', encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('R '):
                group, rva = [line], line.split()[1]
            elif group:
                group.append(line)
                if line.startswith('Z '):
                    total += 1
                    seen[rva] += 1
                    if seen[rva] <= keep:
                        out.extend(group)
                    group, rva = [], None
    (folder / 'sv_args.min.txt').write_text(''.join(out), encoding='utf-8', newline='')
    kept = sum(min(n, keep) for n in seen.values())
    print(f'{folder.name}: {kept} of {total} calls, {len(seen)} functions')


if __name__ == '__main__':
    main()
