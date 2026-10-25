"""Take the x87 out of the bit copies in a file of src/game/ (hand-written from lifted C).

The lifted C moves int values that live in float variables or float arrays with LF_BITS_U
and LF_BITS_F. Both go through a float argument, so the value passes the x87. A small
integer as a float is a denormal, and an x87 load of a denormal takes about 100 cycles.

The script changes three forms where the operand is an lvalue (a variable, *p or p[i]):
  LF_BITS_U(x)             ->  LF_UINT(x)                (an int load of the same bits)
  x = LF_BITS_F(e);        ->  LF_SET_BITS(x, e);        (an int store of the same bits)
  fVarN = <lvalue>;        ->  LF_COPY(fVarN, <lvalue>); (a pure float copy, done as an int)
The bits are the same in each case, so the code does the same. The macros are in
src/lifted/lift_support.h.

Usage: python -I instrumentation/lift/int_bits.py src/game/objects.c [...]
"""
import re
import sys
from pathlib import Path

# A simple lvalue: name, *name, name[expr], or a vec3 member access through a cast.
LVALUE = r'(?:\*\w+|\w+(?:\[[^\[\]\n]+\])?)'

U_RE = re.compile(r'LF_BITS_U\((' + LVALUE + r')\)')
SET_RE = re.compile(r'^(\s*)(' + LVALUE + r') = LF_BITS_F\((.+)\);$', re.M)
COPY_RE = re.compile(r'^(\s*)(fVar\d+) = (' + LVALUE + r');$', re.M)


def main():
    for name in sys.argv[1:]:
        p = Path(name)
        s = p.read_text(encoding='utf-8')
        s, n_set = SET_RE.subn(lambda m: f'{m.group(1)}LF_SET_BITS({m.group(2)}, {m.group(3)});', s)
        s, n_u = U_RE.subn(lambda m: f'LF_UINT({m.group(1)})', s)
        # Only copies of a float lvalue: a constant or a call is not an lvalue.
        s, n_copy = COPY_RE.subn(lambda m: f'{m.group(1)}LF_COPY({m.group(2)}, {m.group(3)});', s)
        p.write_text(s, encoding='utf-8')
        print(f'{name}: {n_u} LF_UINT, {n_set} LF_SET_BITS, {n_copy} LF_COPY')


if __name__ == '__main__':
    main()
