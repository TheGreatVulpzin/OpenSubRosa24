"""Print the object type records of a recording snapshot.

    python -I tools/lift/dump_types.py <snapshot.bin> [type ...] [--full]

One block for each type: the scalars, the particles, the bonds, the faces, the quads, the wheels
and the seats. Dump the snapshot before and after a rewrite and compare the two texts.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot

TYPES, TYPE_SIZE = 0xac51bc0, 0x828c
T_STATE, T_NAME, T_PRICE, T_MASS, T_RADIUS = 0x0, 0xc, 0x2c, 0x30, 0x34
T_NPART, T_PART = 0x38, 0x3c
T_NFRAME, T_WX, T_WZ = 0x53c, 0x540, 0x640
T_NBOND, T_BOND = 0x740, 0x744
T_NFACE, T_FACE = 0x1344, 0x1348
T_NQUAD, T_QUAD = 0x5348, 0x534c
T_POWER, T_NWHEEL, T_WHEEL = 0x7758, 0x775c, 0x7760
T_AXLE, T_WHEELBASE, T_NSEAT, T_SEAT, T_DOOR = 0x7fe0, 0x7fe4, 0x807c, 0x8080, 0x8288


def dump(s, t, full):
    a = TYPES + t * TYPE_SIZE
    rd = lambda off, fmt: s.read(a + off, fmt)
    name = bytes(rd(T_NAME, '32B')).split(b'\0')[0].decode('latin1')
    print('type %d "%s" state %d price %d mass %r radius %r power %r' % (
        t, name, rd(T_STATE, 'i')[0], rd(T_PRICE, 'i')[0], rd(T_MASS, 'f')[0], rd(T_RADIUS, 'f')[0],
        rd(T_POWER, 'f')[0]))
    npart, nbond, nface = rd(T_NPART, 'i')[0], rd(T_NBOND, 'i')[0], rd(T_NFACE, 'i')[0]
    nquad, nwheel, nseat = rd(T_NQUAD, 'i')[0], rd(T_NWHEEL, 'i')[0], rd(T_NSEAT, 'i')[0]
    print('  particles %d frame %d bonds %d faces %d quads_end %d wheels %d seats %d door %d axle %r wheelbase %r' % (
        npart, rd(T_NFRAME, 'i')[0], nbond, nface, nquad, nwheel, nseat, rd(T_DOOR, 'i')[0],
        rd(T_AXLE, 'f')[0], rd(T_WHEELBASE, 'f')[0]))
    if not full:
        return
    for k in range(npart):
        print('  P%d' % k, rd(T_PART + k * 0x14 + 4, 'fff'), rd(T_PART + k * 0x14 + 0x10, 'f')[0])
    for k in range(nbond):
        print('  B%d' % k, rd(T_BOND + k * 0xc, 'iii'))
    for k in range(nface):
        print('  F%d' % k, rd(T_FACE + k * 0x40, 'i' * 9 + 'f' * 6 + 'i'))
    for k in range(min(max(nquad - npart, 0), 256)):
        print('  Q%d' % k, rd(T_QUAD + k * 0x20, 'iiiiffff'))
    for k in range(nwheel):
        print('  W%d' % k, rd(T_WHEEL + k * 0x44, 'iii' + 'f' * 12 + 'ii'))
    for k in range(nseat):
        print('  S%d' % k, rd(T_SEAT + k * 12, 'fff'))
    print('  frame_x', rd(T_WX, '%df' % npart))
    print('  frame_z', rd(T_WZ, '%df' % npart))


def main():
    args = [x for x in sys.argv[1:] if x != '--full']
    full = '--full' in sys.argv
    s = Snapshot(args[0])
    types = [int(x) for x in args[1:]] or range(14)
    for t in types:
        dump(s, t, full)


if __name__ == '__main__':
    main()
