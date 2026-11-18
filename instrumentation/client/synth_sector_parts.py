"""Compare block links, surface strips, mesh allocation and material splits."""
import csv
import math
import os
import re
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation/client'))
import synth_pure as pure
import synth_textures as st

ROW = re.compile(r'^\[\s*\d+/\d+\]\s+(\w+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed\s+(\d+) skipped', re.M)


def main():
    recording, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    addresses = pure.rvas()
    storage = {r['name']: int(r['client_rva'], 16) for r in csv.DictReader(
        (ROOT / 'analysis/client_win/storage.csv').open())}
    failed = []
    for mode in range(4):
        case = work / str(mode)
        case.mkdir(exist_ok=True)
        snapshot = st.Snapshot(recording / 'sv_snapshot.bin')
        calls, counts = [], Counter()

        def write(rva, fmt, *values):
            snapshot.write(rva, fmt, *values)

        def call(name, *values):
            calls.append(st.call(addresses[name], *values))
            counts[name] += 1

        area, sector = storage['area'], 0x1dda5280
        write(area + 0x34, '3i', 1, 1, 1)
        write(area + 0x48, 'I', st.BASE + 0x19e50000)
        write(0x19e50000, 'i', 0)
        write(area + 0x56c, '512i', *([-1] * 512))
        write(area + 0x56c, 'i', 0)
        write(sector, 'i', 1)
        write(sector + 4, '512I', *([0] * 512))
        write(sector + 4, '3I', 0x4000003f, 0xc0000001, 0xc0000001)
        write(sector + 0x804 + 14, 'H', 0x8000 + mode)
        for index in (0, 1, 2, 7, 8, 63, 64, -1):
            call('area_getblocklink', 0, index, 0, 0)
            call('area_getblockmaterial', 0, index, 0, 0)
        for invalid in (-1, 4, 0x7fffffff):
            call('area_getblocklink', invalid, 0, 0, 0)
            call('area_getblockmaterial', invalid, 0, 0, 0)

        temp = storage['tempmesh']
        write(temp, 'i', 4)
        write(temp + 0x110004, 'i', 3)
        write(temp + 0x1a0008, 'i', 0)
        for quad in range(6):
            write(temp + 0x110008 + quad * 16, '4i', 0, 1, 2, 3)
            write(temp + 0x150008 + quad * 4, 'i', 40 + quad)
        for vertex in range(4):
            write(temp + 4 + vertex * 12, '3f', float(vertex & 1), 0.0, float(vertex >> 1))
        for vertex in range(16):
            write(storage['surface'] + vertex * 12, '3f', vertex * .125, vertex * -.25, vertex * .5)
        for position in (0, 1, 2, 3, 4):
            call('area_block_insertquad', 99, position)
        call('area_block_addfloorceil')
        for height in (0.0, .0625, .875, 1.0):
            call('area_block_addfloor', height, mode)
            call('area_block_addceil', height, mode)
        for turns in range(4):
            call('area_block_addwall', turns, .0625)
            for flags in range(4):
                left, right = flags & 1, flags & 2
                width = (0.0, .0625, .3125, 1.0)[mode]
                lower, upper = ((0.0,1.0),(.0625,.875),(.25,.75),(math.nan,math.nan))[mode]
                call('area_block_addwindowvertical_exterior', turns, width, left, right)
                call('area_block_addwindowvertical_interior', turns, width, left, right)
                call('area_block_addwindowhorizontal_exterior', turns, lower, upper, left, right)
                call('area_block_addwindowhorizontal_interior', turns, lower, upper, left, right)

        bitmap = [0] * 1024
        if mode == 1:
            bitmap[:2] = [0xffffffff, 0x7ff]
        elif mode == 2:
            bitmap[:1023] = [0xffffffff] * 1023
            bitmap[1023] = 0xfffffffe
        elif mode == 3:
            bitmap[0] = 0xaaaaaaaf
        write(0x12a7c604, '1024I', *bitmap)
        call('area_getnextsectormesh')
        for first in (0, 16, 496, 512, 1022 * 512 + 496, 1023 * 512):
            for value in (0, 1, 2):
                call('area_setsectormesh', first, value)
        write(sector + 0x5048, 'i', 2)
        write(sector + 0x504c, 'BB2siBB2si', 0, 1, b'AB', 0, 255, 2, b'CD', 16)
        for index in (-1, 0, 8192):
            call('area_removesectormodel', index)
        vertex_data, triangles, materials = 0x19e00000, 0x19e10000, 0x19e20000
        write(sector + 0x502c, '4I', st.BASE + vertex_data, 20, st.BASE + triangles, st.BASE + materials)
        for vertex in range(60):
            write(vertex_data + vertex * 64, '16f', vertex * .125, .25, -vertex * .5,
                  *(float(vertex * 16 + word) for word in range(3, 16)))
        for triangle in range(20):
            indices = ((triangle * 3, triangle * 3 + 1, triangle * 3 + 2)
                       if mode & 1 else (triangle % 4, (triangle + 1) % 4, (triangle + 2) % 4))
            write(triangles + triangle * 12, '3i', *indices)
            write(materials + triangle * 4, 'i', (0, 255, 1, -1)[triangle % 4])
        write(0x29e7d2c0 + 24, 'i', mode & 1)
        call('area_submitsectormodel', 0)
        call('area_submitsectormodel', -1)
        call('area_submitsectormodel', 8192)
        for index, point in enumerate(((0.0,0.0,0.0),(.0001,0.0,0.0),(.00005,.00005,0.0),
                                       (math.nan,0.0,0.0),(math.inf,0.0,0.0))):
            write(vertex_data + index * 64, '3f', *point)
        for first, second in ((0,0),(0,1),(0,2),(0,3),(4,4)):
            call('area_sectorvertequal', 0, first, second)
        call('area_sectorvertequal', -1, 0, 0)
        call('area_sectorvertequal', 8192, 0, 0)
        call('area_resetsector', -1)
        call('area_resetsector', 8192)

        path = case / 'calls.txt'
        path.write_text(''.join(calls), encoding='utf-8', newline='\n')
        snapshot_path = case / 'snapshot.bin'
        snapshot.save(snapshot_path)
        env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build/client_test'),
                   HARNESS_ENV_DIR=str(case / 'env'), SV_LOG_FILE=str(case / 'calls.log'), SV_UI_WIDTH='220')
        env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
        try:
            print(f'{mode}: ' + st.run(snapshot_path, path, ','.join(counts), env), flush=True)
        finally:
            snapshot_path.unlink()
        rows = {name: tuple(map(int, values)) for name, *values in ROW.findall(
            path.with_suffix('.result.txt').read_text(encoding='utf-8'))}
        for name, expected in counts.items():
            print(f'{mode} {name}: {rows.get(name)}', flush=True)
            if rows.get(name) != (expected, 0, 0, 0):
                failed.append((mode, name))
    if failed:
        raise SystemExit(f'Failed cases: {failed}')


if __name__ == '__main__':
    main()
