"""Test the level helpers with boundary values and small point sets.

Run each case, then delete its snapshot when it passes. Keep failed cases for inspection.
Usage: python -I instrumentation/lift/synth_level.py --run <recording dir> <work dir> [case,...]
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

    def surface(rva, world_uv):
        def build(snapshot):
            snapshot.write(0x256ad760, 'i', 2)
            snapshot.write(0x256ad760 + 0x110004, 'i', 1)
            for j in range(4):
                for k in range(4):
                    snapshot.write(POINTS + (j * 4 + k) * 12, 'fff',
                                   k * 1.25 + j * 0.125 - 2,
                                   (j * j - 2 * k) * 0.2 + (j % 2) * 0.03125,
                                   j * 1.5 + k * 0.0625)
                    snapshot.write(LIGHT + (j * 4 + k) * 8, 'ff',
                                   k * 0.333 + j * 0.125, j * 0.375 - k * 0.0625)
            for base, size in ((0x761ea20, 12), (0x13c22d40, 8), (0xb0661c0, 12)):
                for i in range(size):
                    snapshot.write(base + i * 4, 'f', 13.25 + i)
            for i in range(64):
                snapshot.write(0x256ad760 + 0x90004 + i * 12 + 8, 'f', -123.25)
            uv = 'V:0' if world_uv else 'M:%x' % LIGHT
            return ['R %x M:%x %s V:%x V:%x V:%x V:%x V:30007' %
                    (rva, POINTS, uv, su & 0xffffffff, du, sv & 0xffffffff, dv)
                    for su, du, sv, dv in ((2, 3, 2, 3), (2, 2, 3, 2), (2, 1, 2, 1),
                                          (0, 2, 2, 1), (2, 1, 0, 2),
                                          (-1, 2, 2, 1), (2, 1, -1, 2), (0, 1, 0, 1))]
        return build

    result['surface'] = ('addbeziersurface', surface(0xf6f0, False))
    result['surface_world_uv'] = ('addbeziersurfacetexture', surface(0xfc40, True))

    def track(snapshot):
        snapshot.write(0x90dc520, 'i', 0)
        snapshot.write(0x9c1d8c0, 'fff', 2048, 0, -1024)
        shapes = [
            (0, [(2100, 5, 100), (2102, 5, 100), (2140, 7, 132), (2138, 7, 132)], 0, 0),
            (1, [(128, 4, 16), (130, 4, 16), (158, 7, 45), (156, 7, 45)], 1.75, -0.625),
            (2, [(128, 4, 16), (130, 4, 16), (158, 7, 45), (156, 7, 45)], 0, 0),
            (0, [(18428, 10, 500), (18430, 10, 500), (18436, 10, 530), (18434, 10, 530)], 0, 0),
            (1, [(64, 0, 8)] * 4, 0, 0),
            (0, [(10000000, 0.125, 3.333), (10000002, 0.25, 1.125),
                 (10000032, 2.333, 30.125), (9999999, 1.666, 32.875)], 0.333, 0.625),
        ]
        lines = []
        for i, (kind, points, drop, rise) in enumerate(shapes):
            address = POINTS + i * 0x100
            for j, point in enumerate(points):
                snapshot.write(address + j * 12, 'fff', *point)
            lines.append('R 25e70 V:%x M:%x V:%x V:%x' % (kind, address, bits(drop), bits(rise)))
        return lines

    def track_loop(snapshot):
        snapshot.write(0x9c1d8c0, 'fff', 2048, 0, -1024)
        snapshot.write(POINTS, 'fff', 2100, 4.25, 100)
        snapshot.write(POINTS + 0x100, 'fff', 128, -0.0, 12)
        return ['R 32fe0 M:%x' % POINTS, 'R 32fe0 M:%x' % (POINTS + 0x100)]

    result['track'] = ('createtrack', track)
    result['track_loop'] = ('buildsomeshit', track_loop)

    city = 0xe299220

    def intersection(snapshot, index, x, y, z, extent=(0, 0, 0, 0), roads=(-1, -1, -1, -1)):
        address = city + 8 + index * 0x68
        snapshot.write(address, 'iii', x, y, z)
        snapshot.write(address + 0xc, 'fff', x * 4, (y + 1) * 4, z * 4)
        snapshot.write(address + 0x18, 'iiii', *roads)
        snapshot.write(address + 0x28, 'iiii', *extent)

    def append_intersections(snapshot):
        snapshot.write(city, 'i', 0)
        for i in range(3):
            snapshot.write(city + 8 + i * 0x68 + 0x40, 'i', 73 + i)
        return ['R 80b0 V:%x V:%x V:%x' % (x & 0xffffffff, y & 0xffffffff, z & 0xffffffff)
                for x, y, z in ((64, 5, 64), (-3, -1, 8), (127, 6, 192))]

    def append_roads(snapshot):
        snapshot.write(city, 'i', 4)
        snapshot.write(city + 0x1a008, 'i', 0)
        for i, point in enumerate(((64, 5, 64), (96, 5, 64), (96, 7, 96), (64, 7, 96))):
            intersection(snapshot, i, *point, extent=(4, 0, 1, 2))
        name = SCRATCH + 0x3000
        for i, value in enumerate(b'Boundary road\0'):
            snapshot.write(name + i, 'B', value)
        return ['R 1ba30 V:%x V:%x V:%x V:%x M:%x' % (a, b, wa, wb, name)
                for a, b, wa, wb in ((0, 1, 2, 3), (1, 2, 1, 1), (2, 3, 3, 2), (3, 3, 1, 1))]

    def build_intersections(snapshot):
        snapshot.write(city, 'i', 2)
        intersection(snapshot, 0, 64, 5, 64, (2, 3, 4, 1), (0, 1, 2, 3))
        intersection(snapshot, 1, 98, 6, 73, roads=(-1, 0, -1, 1))
        return ['R 4d850 V:0', 'R 4d850 V:1']

    def build_roads(snapshot):
        snapshot.write(city, 'i', 10)
        snapshot.write(city + 0x1a008, 'i', 5)
        for offset, value in ((0x28c, 0x1234000b), (0x290, 0x5678000c),
                              (0x294, 0x1000d), (0x298, 0x1000e)):
            snapshot.write(0xf26fd00 + offset, 'I', value)
        roads = [
            (0, (64, 5, 64), (92, 7, 64), 2, 3),
            (0, (64, 9, 112), (92, 7, 112), 1, 1),
            (1, (144, 6, 64), (144, 7, 92), 3, 2),
            (1, (192, 8, 64), (192, 7, 92), 2, 1),
            (0, (64, 5, 160), (88, 5, 160), 2, 0),
        ]
        for i, (axis, a, b, wa, wb) in enumerate(roads):
            intersection(snapshot, i * 2, *a, extent=(2, 2, 2, 2))
            intersection(snapshot, i * 2 + 1, *b, extent=(2, 2, 2, 2))
            snapshot.write(city + 0x1a00c + i * 0x4c, 'iiiii', i * 2, i * 2 + 1, axis, wa, wb)
        return ['R 4da10 V:%x' % i for i in range(len(roads))]

    result['city_intersections'] = ('city_addintersection', append_intersections)
    result['city_roads'] = ('addcitystreet', append_roads)
    result['city_intersection_blocks'] = ('buildcityinterection', build_intersections)
    result['city_street_blocks'] = ('buildcitystreet', build_roads)

    def landscape(snapshot):
        snapshot.write(0x9c1d8c0 + 4, 'f', 7.25)
        return ['R 1bc50']

    def build_level(gametype):
        def build(snapshot):
            editor = 0xf270a80
            snapshot.write(0x90dc540 + 8, 'i', gametype)
            snapshot.write(city + 4, 'i', 0x123456)
            snapshot.write(0x9c1d8c0 + 4, 'f', 7.25)
            snapshot.write(0x12a80c44, '4096s', b'\x7f' * 4096)
            snapshot.write(editor + 0x38, 'i', 0)
            snapshot.write(editor + 0x303c, 'i', 0)
            snapshot.write(editor + 0x10b844, 'i', 0)
            snapshot.write(editor + 0xb844, '1048576s', bytes(1048576))
            if gametype != 1:
                snapshot.write(0xf26fd00 + 0x288, 'I', 0x12340005)
                for kind in range(16):
                    x = 96 + (kind % 4) * 3
                    z = 96 + (kind // 4) * 3
                    low = kind % 3 if kind else 2
                    snapshot.write(editor + 0xb844 + z * 1024 + x, 'B', (kind << 4) | low)
                snapshot.write(editor + 0xb844 + 510 * 1024 + 510, 'B', 0x11)
            return ['R 54e40']
        return build

    result['landscape'] = ('createlandscape', landscape)
    result['level_tiles'] = ('createlevel', build_level(0))
    result['level_single_building'] = ('createlevel', build_level(1))
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
        matched = re.search(r': (\d+) calls same', summary)
        ok = ok and matched is not None and int(matched.group(1)) == len(lines)
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|known differences|(?:\w+ )?crashed|skipped)\b', summary)
        bad += not ok
        print('%s %s: %s%s' % (name, function, summary, '' if ok else ' CHECK'), flush=True)
        if ok:
            shutil.rmtree(folder)
    print('%d case(s) to check' % bad)
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
