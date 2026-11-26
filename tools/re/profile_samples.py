"""Count the samples of a run of the native client by function.

Usage: python -I tools/re/profile_samples.py <exe.map> <log of the run> [top]

Run the client with SR_SAMPLE=1 and SR_SAMPLE_MS=10 (one sample each 10 ms). The log has a
"SAMPLE base=... eip=... calls: ..." line for each sample. The tool prints two lists: the
functions where the main thread was (self time) and the functions that were on the stack
(inclusive time), with the share of all samples.
"""
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from map_address import load, name  # noqa: E402


def function_of(address, base, preferred, symbols):
    """The function of an address. An address outside the executable (a DLL of the system or the
    graphics driver) is "[outside the executable]"."""
    mapped = address - base + preferred
    if mapped < preferred or mapped > symbols[-1][0] + 0x20000:
        return "[outside the executable]"
    return name(address, base, preferred, symbols).split("+")[0]


def main():
    preferred, symbols = load(sys.argv[1])
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
    own, inclusive, total = Counter(), Counter(), 0
    for line in Path(sys.argv[2]).read_text(encoding="latin-1").replace("CRASH", "\nCRASH").splitlines():
        m = re.match(r"SAMPLE base=([0-9A-Fa-f]+) eip=([0-9A-Fa-f]+) calls:(.*)", line)
        if not m:
            continue
        base = int(m.group(1), 16)
        names = [function_of(int(m.group(2), 16), base, preferred, symbols)]
        names += [function_of(int(a, 16), base, preferred, symbols) for a in m.group(3).split()]
        total += 1
        own[names[0]] += 1
        for fn in set(names):
            inclusive[fn] += 1
    print(f"{total} samples")
    for title, counter in (("Self time", own), ("On the stack", inclusive)):
        print(f"\n{title}:")
        for fn, count in counter.most_common(top):
            print(f"  {100.0 * count / max(total, 1):5.1f}%  {count:6}  {fn}")


if __name__ == "__main__":
    main()
