"""Find calls of printf-like functions that pass fewer arguments than the format needs.

The decompiler can drop the extra arguments of a variable-argument call. The callee then reads
stack garbage (a wrong number on the screen). This script reads the lifted C of the client, finds the
calls of the printf family and of drawtext, reads the format string from the exe and compares the
number of conversions with the number of arguments.

Usage: python -I instrumentation/client/check_variadic.py [lifted_client.c]
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'original/client/subrosa.exe'
BASE = 0x400000
# name -> index of the format argument, number of fixed arguments (format included)
FUNCS = {
    'sprintf': (1, 2),
    'fprintf': (1, 2),
    'printf': (0, 1),
    'drawtext': (0, 9),
    'sscanf': (1, 2),
}


def load_exe():
    d = EXE.read_bytes()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    secs = []
    for i in range(n):
        o = pe + 24 + opt + 40 * i
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
        secs.append((va, vs, ro, rs))

    def read(va):
        rva = va - BASE
        for v, vs, ro, rs in secs:
            if v <= rva < v + min(vs, rs) or (v <= rva < v + rs):
                return d[ro + rva - v:ro + rva - v + 400].split(b'\0')[0].decode('latin1')
        return None
    return read


def split_args(text):
    """Split the text of a call (inside the parentheses) at the top level commas."""
    args, depth, cur = [], 0, ''
    for ch in text:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            args.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def conversions(fmt):
    n = 0
    for m in re.finditer(r'%(?!%)[-+ #0]*(\*|\d+)?(?:\.(\*|\d+))?[hlLIq]*[a-zA-Z]', fmt):
        n += 1 + (m.group(1) == '*') + (m.group(2) == '*')
    return n


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'src/lifted/client/lifted_client.c'
    text = path.read_text(errors='replace')
    read = load_exe()
    bad = 0
    total = 0
    for name, (fi, fixed) in FUNCS.items():
        for m in re.finditer(r'(?<![\w#])' + name + r'\(', text):
            i = m.end()
            depth, j = 1, i
            while j < len(text) and depth:
                depth += (text[j] == '(') - (text[j] == ')')
                j += 1
            args = split_args(text[i:j - 1])
            if len(args) <= fi:
                continue
            fa = args[fi]
            va = re.search(r'(?:_|DAT_)([0-9a-f]{8})\b', fa)
            if not va or not re.match(r'^(?:&?DAT_|\(char \*\))?[A-Za-z_0-9&() *]*$', fa):
                continue
            fmt = read(int(va.group(1), 16))
            if fmt is None or '%' not in fmt:
                continue
            total += 1
            need, got = conversions(fmt), len(args) - fixed
            if got < need:
                bad += 1
                line = text.count('\n', 0, m.start()) + 1
                print(f'{path.name}:{line}: {name} needs {need} extra argument(s), has {got}: {fmt!r}')
    print(f'{total} calls with a known format checked, {bad} with too few arguments')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
