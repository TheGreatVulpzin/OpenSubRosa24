"""Test the level helpers with boundary values and small point sets.

Run each case, then delete its snapshot when it passes. Keep failed cases for inspection.
Usage: python -I tools/lift/synth_level.py --run <recording dir> <work dir> [case,...]
"""
import argparse
import copy
import re
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot
from synth_sys import run_harness

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = 0x13d22d60 + 8 + 0x80000
OUT, POINTS, LIGHT = SCRATCH, SCRATCH + 0x1000, SCRATCH + 0x2000


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def cases():
    result = {}

    def noise(rva):
        def build(snapshot):
            return ['R %x V:%x V:%x V:%x' % (rva, bits(x), bits(y), mask)
                    for x, y in ((0, 0), (0.25, 0.75), (-0.25, -1.75),
                                 (-255.125, 256.375), (17.333, -42.666))
                    for mask in (0xffffffff, 0xff)]
        return build

    result['lerpnoise2d'] = ('lerpnoise2d', noise(0x9870))
    result['cerpnoise2d'] = ('cerpnoise2d', noise(0x9be0))

    def bezier(rva):
        def build(snapshot):
            return ['R %x V:%x %s' % (rva, degree, ' '.join('V:%x' % bits(v)
                    for v in (t, -2.125, 0.333, 3.875, 9.0625)))
                    for degree in (0, 1, 2, 3, 4)
                    for t in (-0.375, 0, 0.333, 1, 1.25)]
        return build

    result['bezier'] = ('tesselatebezier', bezier(0x2560))
    result['bezier_derivative'] = ('tesselatebezierderivative', bezier(0x2620))

    def weights(points, light):
        def build(snapshot):
            for i, point in enumerate(points):
                snapshot.write(POINTS + i * 12, 'fff', *point)
                snapshot.write(OUT + i * 4, 'I', 0xaaaaaaaa)
            snapshot.write(LIGHT, 'fff', *light)
            return ['R 28c90 M:%x V:%x M:%x M:%x' % (OUT, len(points), POINTS, LIGHT)]
        return build

    result['weights_plane'] = ('calculatepointscale', weights(
        [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)], (0.25, 0, 0.75)))
    result['weights_five'] = ('calculatepointscale', weights(
        [(-1.25, 0.125, 0), (2, 0, 0.333), (0, 2.75, 0), (0, 0, 3.125),
         (0.75, -0.125, 0.625)], (0.125, 0.375, 0.625)))
    result['weights_repeated'] = ('calculatepointscale', weights(
        [(0, 0, 0), (0, 0, 0), (1, 1, 1), (2, 2, 2)], (0, 0, 0)))
    result['weights_threshold'] = ('calculatepointscale', weights(
        [(0, 0.01, 0), (0, 0, 0), (1, 0, 0), (0, 0, 1)], (0.25, 0.005, 0.25)))
    result['weights_rounding'] = ('calculatepointscale', weights(
        [(0.00125, 0.000125, 0.3333), (10000000, 0.1, -10000000),
         (0.333, 0.00001, 2000.25), (-100.0625, 800.125, 0.666),
         (3.14159, -0.125, 0.625)], (0.125, 0.375, 0.625)))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', required=True)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('cases', nargs='?')
    args = parser.parse_args()
    selected = set(args.cases.split(',')) if args.cases else None
    available = cases()
    if selected and selected - available.keys():
        parser.error('Unknown cases: ' + ', '.join(sorted(selected - available.keys())))
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    bad = 0
    for name, (function, build) in available.items():
        if selected and name not in selected:
            continue
        snapshot = copy.deepcopy(base)
        lines = build(snapshot)
        folder = args.work / name
        folder.mkdir(parents=True, exist_ok=True)
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        summary = run_harness(folder, function)
        (args.work / (name + '.log')).write_text(summary + '\n', encoding='utf-8')
        ok = summary.startswith('1 functions tested')
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|(?:\w+ )?crashed|skipped)\b', summary)
        bad += not ok
        print('%s %s: %s%s' % (name, function, summary, '' if ok else ' CHECK'), flush=True)
        if ok:
            shutil.rmtree(folder)
    print('%d case(s) to check' % bad)
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
