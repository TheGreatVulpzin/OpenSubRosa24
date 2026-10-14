"""Find suspect number conversions in the lifted C.

The original copies bits between a float and an int in many places (a stack slot or a
global that holds a float at one time and an int at another time). Ghidra often prints
these copies as plain assignments. In C an assignment between a float and an int converts
the number, so 0.42 becomes 0. The harness finds these bugs only in functions that a
recording calls. This tool finds them without a recording: MSVC warns about an implicit
conversion between a float and an int (C4244). An explicit cast, such as (int)x or
(float)i, is a real conversion in the p-code (TRUNC, INT2FLOAT) and gives no warning.

Usage: python -I tools/lift/lint.py [file.c]      (default: src/lifted/lifted_024d.c)
Output: one line per warning, grouped by function, and a count per function.
"""
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/lift'))
from lift import load_names, vcvars_line  # noqa: E402

FLOATS = ('float', 'double', 'float10', 'long double')

# Findings checked by hand: bugs of the game itself (docs/lift.md, "Lint").
KNOWN = {('lf_3ccc0', 'uninitialized')}


def warnings(path):
    bat = ROOT / 'build/lift/lint.bat'
    bat.parent.mkdir(parents=True, exist_ok=True)
    crlf = chr(13) + chr(10)
    bat.write_text(vcvars_line() +
                   f'cl /nologo /c /TC /std:c11 /W4 /arch:IA32 /utf-8 /errorReport:none '
                   f'/I "{ROOT / "src/lifted"}" /I "{ROOT / "src/include"}" /Fo"{ROOT / "build/lift/lint.obj"}" "{path}"{crlf}',
                   encoding='mbcs')
    r = subprocess.run(['cmd', '/c', str(bat)], capture_output=True, text=True, encoding='mbcs', errors='replace')
    out = []
    for m in re.finditer(r'\((\d+)\)\s*:\s*warning (C4244|C4242|C4700):\s*(.*)', r.stdout + r.stderr):
        if m.group(2) == 'C4700':
            # A local read before any write: lf_frame holds the stack variables, so a slot
            # that the original fills in a way the lifted code does not (a callee, a push).
            out.append((int(m.group(1)), 'uninitialized', re.findall(r"'([^']*)'", m.group(3))[-1]))
            continue
        types = re.findall(r"'([^']*)'", m.group(3))
        # Only conversions between a float type and an integer type.
        if len(types) >= 2 and (types[-2] in FLOATS) != (types[-1] in FLOATS):
            out.append((int(m.group(1)), types[-2], types[-1]))
    return out


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'src/lifted/lifted_024d.c'
    src = path.read_text(encoding='utf-8').split('\n')
    starts = [(i + 1, m.group(1)) for i, l in enumerate(src)
              if (m := re.match(r'^\S.*\b(lf_[0-9a-f]{5})\(', l))]
    names = load_names()
    found = defaultdict(list)
    for line, a, b in warnings(path):
        fn = next((f for s, f in reversed(starts) if s <= line), '?')
        found[fn].append((line, a, b, src[line - 1].strip()))
    for fn in sorted(found, key=lambda f: -len(found[f])):
        rva = '0x' + fn[3:].lstrip('0') if fn != '?' else '?'
        print(f'{fn} {names.get(rva, "")}: {len(found[fn])}')
        for line, a, b, text in found[fn]:
            print(f'  {line}: {a} -> {b}: {text[:150]}')
    print(f'total: {sum(len(v) for v in found.values())} warnings in {len(found)} functions')
    new = [(fn, w[1]) for fn, ws in found.items() for w in ws if (fn, w[1]) not in KNOWN]
    if new:
        print(f'{len(new)} new finding(s), not in KNOWN')
        sys.exit(1)


if __name__ == '__main__':
    main()
