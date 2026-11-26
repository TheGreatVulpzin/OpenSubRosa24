"""Name the functions that a SAMPLE line of the native client points to.

Usage: python -I tools/re/map_address.py <exe.map> <log of the run>

The client prints "SAMPLE base=<address> eip=<address> calls: <addresses>" once a second when
SR_SAMPLE=1. The tool reads the symbols of the link map and prints the function of each address.
"""
import bisect
import re
import sys
from pathlib import Path


def load(path):
    text = Path(path).read_text(encoding="latin-1")
    preferred = int(re.search(r"Preferred load address is ([0-9a-fA-F]+)", text).group(1), 16)
    symbols = []
    for m in re.finditer(r"^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s", text, re.M):
        symbols.append((int(m.group(2), 16), m.group(1)))
    symbols.sort()
    return preferred, symbols


def name(address, base, preferred, symbols):
    mapped = address - base + preferred
    keys = [s[0] for s in symbols]
    i = bisect.bisect_right(keys, mapped) - 1
    if i < 0:
        return "?"
    return f"{symbols[i][1]}+0x{mapped - symbols[i][0]:x}"


def main():
    preferred, symbols = load(sys.argv[1])
    for line in Path(sys.argv[2]).read_text(encoding="latin-1").splitlines():
        m = re.match(r"SAMPLE base=([0-9A-Fa-f]+) eip=([0-9A-Fa-f]+) calls:(.*)", line)
        if not m:
            continue
        base = int(m.group(1), 16)
        parts = [name(int(m.group(2), 16), base, preferred, symbols)]
        parts += [name(int(a, 16), base, preferred, symbols) for a in m.group(3).split()]
        print(" <- ".join(parts))


if __name__ == "__main__":
    main()
