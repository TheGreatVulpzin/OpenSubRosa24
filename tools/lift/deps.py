"""Phase 4 progress: how much the lifted C still depends on the original .exe.

Usage: python -I tools/lift/deps.py [--top N] [--md] [--func NAME] [--target NAME]

The lifted C (src/lifted/lifted_024d.c) still uses the image of the 024d in three ways:
- globals: "#define g_human (*(Human (*)[..])SR_VA(0x..))", the memory of .data in the image.
- calls: "#define humansimulation ((void (__cdecl *)(void))SR_VA(0x451060))", a call through
  the address in the image. When the callee is lifted too (src/lifted/lifted_table.c) or
  written by hand (hook/hooks_024d.c), the call can go to the C function directly. "to C"
  counts those, "to exe" the others.
- inline SR_VA(...) in the code: relocated constants (an address used as a number).
Imports (LF_IMPORT) go to MSVCR100/KERNEL32/WSOCK32 and are counted apart: the platform
layer of Phase 4 replaces them (docs/phase4-inventory.md, section 2).

A standalone server (docs/plano.md, Phase 4) needs all of these at zero.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ROOT / 'tools' / 'lift' / 'targets'  # the same target files as gate.py

BLOCK = re.compile(r'^/\* -+ 0x([0-9a-f]+) \*/$', re.M)
FUNC_DEF = re.compile(r'^#define (\w+) \(\([^;\n]*\(__cdecl \*\)\([^\n]*\)\)SR_VA\(0x([0-9a-f]+)\)\)$', re.M)
DATA_DEF = re.compile(r'^#define (\w+) \(\*\([^\n]*\)SR_VA\(0x([0-9a-f]+)\)\)$', re.M)
IMPORT_DEF = re.compile(r'^#define (\w+) [^\n]*LF_IMPORT\(\d+, "([^"]+)", "([^"]+)"\)\)$', re.M)
INLINE = re.compile(r'SR_VA\(')
NAME = re.compile(r'^\s*\{ "(\w+)", 0x([0-9a-f]+),', re.M)
BASE = 0x400000


def blocks(text):
    marks = list(BLOCK.finditer(text))
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        yield int(m.group(1), 16), text[m.start():end]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--top', type=int, default=15, help='functions with the most dependencies')
    ap.add_argument('--md', action='store_true', help='Markdown table')
    ap.add_argument('--func', help='list the dependencies of one function')
    ap.add_argument('--target', default='ded024d', help='tools/lift/targets/<name>.json')
    a = ap.parse_args()
    t = json.loads((TARGETS / f'{a.target}.json').read_text(encoding='utf-8'))
    LIFTED, TABLE = ROOT / t['lifted_c'], ROOT / t['table']
    HAND = ROOT / t['hand_hooks'] if t.get('hand_hooks') else None

    names = {int(r, 16): n for n, r in NAME.findall(TABLE.read_text(encoding='utf-8'))}
    in_c = set(names) | ({int(r, 16) for _, r in NAME.findall(HAND.read_text(encoding='utf-8'))} if HAND else set())
    rows, globals_used, callees_exe = [], Counter(), Counter()
    for rva, body in blocks(LIFTED.read_text(encoding='utf-8')):
        name = names.get(rva, f'0x{rva:x}')
        code = body
        funcs = {n: int(va, 16) - BASE for n, va in FUNC_DEF.findall(body)}
        data = {n: int(va, 16) - BASE for n, va in DATA_DEF.findall(body)}
        imports = IMPORT_DEF.findall(body)
        for line in body.splitlines():  # inline uses: SR_VA outside the #define lines
            if line.startswith('#'):
                code = code.replace(line, '')
        used = lambda n: re.search(r'\b' + re.escape(n) + r'\b', code) is not None
        funcs = {n: r for n, r in funcs.items() if used(n)}
        data = {n: r for n, r in data.items() if used(n)}
        imports = [i for i in imports if used(i[0])]
        to_c = [n for n, r in funcs.items() if r in in_c]
        to_exe = [n for n, r in funcs.items() if r not in in_c]
        inline = len(INLINE.findall(code))
        rows.append((name, rva, len(data), len(to_c), len(to_exe), inline, len(imports)))
        globals_used.update(data)
        callees_exe.update(to_exe)
        if a.func == name:
            print(f'{name} (0x{rva:x})')
            print('  globals:', ', '.join(sorted(data)) or '-')
            print('  calls to C (lf_ exists):', ', '.join(sorted(to_c)) or '-')
            print('  calls to exe:', ', '.join(sorted(to_exe)) or '-')
            print('  imports:', ', '.join(sorted(f'{d}!{n}' for _, d, n in imports)) or '-')
            print(f'  inline SR_VA: {inline}')
            return 0

    total = [sum(r[i] for r in rows) for i in range(2, 7)]
    free = sum(1 for r in rows if r[2] == r[3] == r[4] == r[5] == 0)
    print(f'{len(rows)} lifted functions, {free} with no use of the image '
          f'(imports apart: {sum(1 for r in rows if not any(r[2:]))} with nothing at all)')
    print(f'uses of the image: {total[0]} global refs ({len(globals_used)} different globals), '
          f'{total[1]} calls to C, {total[2]} calls to the exe ({len(callees_exe)} callees), '
          f'{total[3]} inline SR_VA, {total[4]} imports')
    if callees_exe:
        print('callees still in the exe:', ', '.join(f'{n} ({c})' for n, c in callees_exe.most_common(12)))
    rows.sort(key=lambda r: -(r[2] + r[3] + r[4] + r[5]))
    head = ('function', 'globals', 'calls to C', 'calls to exe', 'inline SR_VA', 'imports')
    if a.md:
        print('\n| ' + ' | '.join(head) + ' |\n|' + ' --- |' * len(head))
        for r in rows[:a.top]:
            print(f'| `{r[0]}` | ' + ' | '.join(str(x) for x in r[2:]) + ' |')
    else:
        print(f'\n{head[0]:32} ' + ' '.join(f'{h:>13}' for h in head[1:]))
        for r in rows[:a.top]:
            print(f'{r[0]:32} ' + ' '.join(f'{x:13}' for x in r[2:]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
