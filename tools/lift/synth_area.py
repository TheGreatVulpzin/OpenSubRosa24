"""Test area cells with small grids and boundary values.

Usage: python -I tools/lift/synth_area.py --run <recording> <work> [cells]
"""
import argparse
import copy
import os
import re
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot
from synth_sys import run_harness

AREA = 0x16d66040
SECTORS = 0x1954d740
BLOCKS = 0x133b2d00
SCRATCH = 0x13d22d60 + 8 + 0x80000
INDEX, TEXTURES, VECTOR = SCRATCH, SCRATCH + 0x100, SCRATCH + 0x200


def cell(x, y, z):
    return (x & 7) + (z & 7) * 8 + (y & 7) * 64


def setup(s):
    a = AREA + 3 * 0x100d6c
    s.write(a + 0x34, 'iii', 2, 2, 2)
    s.write(a + 0x48, 'I', 0x20000000 + INDEX)
    s.write(INDEX, '8i', 0, -1, -1, -1, -1, -1, -1, -1)
    s.write(a + 0x568, 'i512i', 1, *([-1] * 512))
    s.write(a + 0x56c, 'i', 0)
    s.write(a + 0x568 + 0x804, 'i', 0)
    s.write(SECTORS, 'i', 1)
    s.write(SECTORS + 0x6068, 'i', 0)
    s.write(SECTORS + 4, '512I', *([0] * 512))
    s.write(SECTORS + 0x3004, '512I', *([0] * 512))
    s.write(SECTORS + 0x5004, '4i', -1, -1, -1, -1)
    s.write(SECTORS + 0x5014, 'i', 3)
    s.write(TEXTURES, '8H', 1, 0x8002, 3, 0x8004, 5, 6, 7, 8)
    s.write(VECTOR, 'fff', 0.25, 0.5, 0.75)
    s.write(BLOCKS + 0x84 + 0x6c, 'iii', 2, 2, 2)
    s.write(BLOCKS + 0x84 + 0x78, 'i', 4)


def cases():
    result = {}

    def add(name, function, build):
        result[name] = ('cells', function, build)

    def normal(s):
        for dx, dy, dz in ((0, -1, 0), (0, 1, 0), (0, 0, 1),
                           (0, 0, -1), (-1, 0, 0), (1, 0, 0)):
            s.write(SECTORS + 4 + cell(3 + dx, 3 + dy, 3 + dz) * 4, 'I', 0x4000003f)
        return ['R 3da50 V:3 V:3 V:3 V:3 V:40000000 M:%x' % TEXTURES]

    add('normal_neighbours', 'area_createblock', normal)
    add('normal_floor', 'area_createblock', lambda s:
        ['R 3da50 V:3 V:3 V:0 V:3 V:40000000 M:%x' % TEXTURES])
    add('special_new_chunk', 'area_createblock', lambda s:
        ['R 3da50 V:3 V:41 V:3 V:3 V:80000001 M:%x' % TEXTURES])
    add('unsupported_kind', 'area_createblock', lambda s:
        ['R 3da50 V:3 V:41 V:3 V:3 V:20000000 M:%x' % TEXTURES])
    add('zero_word', 'area_createblock', lambda s:
        ['R 3da50 V:3 V:41 V:3 V:3 V:0 M:%x' % TEXTURES])

    def delete(s):
        normal(s)
        s.write(SECTORS + 4 + cell(3, 3, 3) * 4, 'I', 0x40000000)
        return ['R 29970 V:3 V:3 V:3 V:3']

    add('delete_neighbours', 'area_deleteblock', delete)

    def release(rva, extra):
        def build(s):
            s.write(SECTORS + 0x5004, '4i', 5, 5, 5, 5)
            s.write(0xb7e91e0 + 4 + 5 * 0x2a30, 'i', 0)
            return ['R %x V:3 V:3 V:3 V:3 %s' % (rva, extra)]
        return build

    add('release_setblock', 'area_interior_setblock', release(0x2dc0, 'V:80000001'))
    add('release_setobject', 'area_interior_setobject', release(0x2f00, 'V:409'))
    add('release_createblock', 'area_interior_createblock',
        release(0x121d0, 'V:80000001 M:%x' % TEXTURES))
    add('release_other_kind', 'area_interior_createblock',
        release(0x121d0, 'V:40000000 M:%x' % TEXTURES))

    for name, x, word, face in [('break_bad_face', 3, 0x80000001, 4),
                               ('break_normal', 3, 0x40000000, 0),
                               ('break_negative_x', -1, 0x80000001, 0),
                               ('break_odd_face', 3, 0x80000001, 1)]:
        add(name, 'area_breakblock', lambda s, x=x, word=word, face=face:
            ['R 2c020 V:3 V:%x V:3 V:3 V:%x V:%x M:%x M:%x' %
             (x & 0xffffffff, word, face, VECTOR, VECTOR)])

    def full_sectors(s):
        for i in range(8192):
            s.write(SECTORS + i * 0x6068, 'i', 1)
        return ['R f4a0 V:3 V:0 V:1 V:1 V:1']

    def full_chunks(s):
        for i in range(512):
            s.write(AREA + 3 * 0x100d6c + 0x568 + i * 0x804, 'i', 1)
        return ['R f620 V:3 V:1 V:1 V:1']

    add('full_sectors', 'area_createsector', full_sectors)
    add('full_chunks', 'area_createchunk', full_chunks)
    add('create_small_area', 'createarea', lambda s:
        ['R f2b0 V:3 M:%x V:1 V:1 V:1 V:3eaaaaab' % VECTOR])
    def file_case(version):
        def build(s):
            s.write(SCRATCH + 0x300, '16s', b'sv_area_test\0')
            data = struct.pack('<i', version)
            if version in (1, 2):
                data += struct.pack('<10i', 2, 3, 4, 1, 2, 3, 4, 5, 6, 1)
                data += struct.pack('<6i', 1, 1, 1, 1, 7, 1)
                for k in range(16):
                    data += struct.pack('<5f', k * 0.125, k % 4, k // 4, k * 0.25, k * 0.375)
                if version == 2:
                    data += struct.pack('<i', 1)
                    for k in range(8):
                        data += struct.pack('<3f', k & 1, (k >> 1) & 1, (k >> 2) & 1)
                    data += struct.pack('<7i', 1, 2, 3, 4, 5, 6, 0x3f)
            return (['R 29a0 M:%x' % (SCRATCH + 0x300)],
                    {'data/block/sv_area_test.sbl': data})
        return build

    for version in (1, 2, 3):
        result['block_version_%d' % version] = ('files', 'loadblock', file_case(version))

    result['city_bad_version'] = ('files', 'leveleditor_load', lambda s:
        (['R 8590'], {'data/city.sbc': struct.pack('<i', 5)}))

    def city_save(s):
        editor = 0xf270a80
        s.write(editor + 0x38, 'i', 1)
        s.write(editor + 0x3c, 'iii', 3, 5, 7)
        s.write(editor + 0x303c, 'i', 1)
        s.write(editor + 0x3040, '5i32s', 0, 0, 1, 2, 3, b'Test Street\0')
        s.write(editor + 0x10b844, 'i', 1)
        s.write(editor + 0x10b84c, '64s4i', b'Test Building\0', 3, 5, 7, 2)
        expected = struct.pack('<5i', 6, 1, 3, 5, 7)
        expected += struct.pack('<6i32s', 1, 0, 0, 1, 2, 3, b'Test Street\0')
        expected += s.read(editor + 0xb844, '1048576s')[0]
        expected += struct.pack('<i64s4i', 1, b'Test Building\0', 3, 5, 7, 2)
        return (['R 8320'], {'data/city.sbc': expected})

    result['city_save'] = ('files', 'leveleditor_save', city_save)

    def mesh(s):
        t = 0x256ad760
        s.write(t, 'i', 24)
        s.write(t + 0x110004, 'i', 6)
        s.write(t + 0x1a0008, 'i', 1)
        for i in range(24):
            for off in (4, 0x30004, 0x60004, 0x90004):
                s.write(t + off + i * 12, 'fff', i * 0.0625 - 0.75, i * 0.125, 1.375 - i * 0.03125)
        for q in range(6):
            s.write(t + 0x110008 + q * 16, '4i', *range(q * 4, q * 4 + 4))
            s.write(t + 0x150008 + q * 4, 'I', ((q + 1) << 16) | (q % 4 + 1))
        s.write(t + 0x1a000c, '3i', 0, 1, 2)
        s.write(t + 0x1d000c, 'I', 0xf0005)

    def setup_block(s):
        mesh(s)
        return ['R 1e50 V:5']

    def rotate_block(s):
        mesh(s)
        s.write(BLOCKS + 4 * 0x84 + 0x6c, 'iii', 2, 3, 4)
        return ['R 26d0 V:5 M:%x' % (BLOCKS + 4 * 0x84 + 0x6c)]

    result['mesh_setup_sides'] = ('mesh', 'area_setupsblock', setup_block)
    result['mesh_rotate'] = ('mesh', 'area_copyrotateblock', rotate_block)

    def surface_turns(s):
        for i in range(16):
            s.write(0xb7e90a0 + i * 12, 'fff', i * 0.0625, -0.125, 1.375 - i * 0.03125)
        return ['R 2cf0 V:%x V:%x' % (turns & 0xffffffff, side)
                for turns in (-1, 0, 1, 2, 3, 4)
                for side in (0, 0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x60000, 0x70000)]

    result['surface_turns'] = ('mesh', 'area_block_rotatesurface', surface_turns)

    def box(s):
        s.write(0x256ad760, 'i', 2)
        s.write(0x256ad760 + 0x110004, 'i', 1)
        values = (-0.33333334, 0.0625, 0.875, 1.3333334, 0.9375, 0.03125)
        return ['R 10950 ' + ' '.join('V:%x' % struct.unpack('<I', struct.pack('<f', v))[0]
                                    for v in values) + ' V:7']

    def beam(s):
        s.write(0x256ad760, 'i', 2)
        s.write(0x256ad760 + 0x110004, 'i', 1)
        s.write(TEXTURES, '6I', 1, 2, 3, 4, 5, 6)
        for i in range(16):
            s.write(0xb7e90a0 + i * 12, 'fff', i * 0.0625, -0.125, 1.375 - i * 0.03125)
        values = (-0.33333334, 0.0625, 0.875, 1.3333334, 0.9375, 0.03125)
        suffix = ' '.join('V:%x' % struct.unpack('<I', struct.pack('<f', v))[0] for v in values)
        return ['R 11d70 V:%x %s V:3f M:%x' % (turns, suffix, TEXTURES) for turns in range(4)]

    result['box_rounding'] = ('mesh', 'area_block_addbox', box)
    result['beam_rounding'] = ('mesh', 'area_block_addbeam', beam)
    return result


def run_files(folder, function, files):
    game = folder / 'game'
    game.mkdir(exist_ok=True)
    for name, data in files.items():
        path = game / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    previous_game = os.environ.get('HARNESS_GAME_DIR')
    os.environ['HARNESS_GAME_DIR'] = str(game.resolve())
    try:
        summary = run_harness(folder, function)
        file_ok = True
        if function == 'leveleditor_save':
            expected = files['data/city.sbc']
            outputs = list((folder / 'env').rglob('city.sbc'))
            file_ok = len(outputs) == 2 and all(path.read_bytes() == expected for path in outputs)
            print('city_save file bytes: %s (%d bytes in each run)' %
                  ('same' if file_ok else 'FAILED', len(expected)), flush=True)
        return summary, file_ok
    finally:
        if previous_game is None:
            del os.environ['HARNESS_GAME_DIR']
        else:
            os.environ['HARNESS_GAME_DIR'] = previous_game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', required=True)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('cases', nargs='?')
    args = parser.parse_args()
    selected = set(args.cases.split(',')) if args.cases else None
    available = cases()
    choices = set(available) | {entry[0] for entry in available.values()}
    if selected and selected - choices:
        parser.error('Unknown cases: ' + ', '.join(sorted(selected - choices)))
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    bad = 0
    for name, (group, function, build) in available.items():
        if selected and name not in selected and group not in selected:
            continue
        snapshot = copy.deepcopy(base)
        setup(snapshot)
        lines = build(snapshot)
        files = None
        if isinstance(lines, tuple):
            lines, files = lines
        folder = args.work / name
        folder.mkdir(parents=True, exist_ok=True)
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        file_ok = True
        if files is None:
            summary = run_harness(folder, function)
        else:
            summary, file_ok = run_files(folder, function, files)
        (args.work / (name + '.log')).write_text(summary + '\n', encoding='utf-8')
        matched = re.search(r': (\d+) calls same', summary)
        ok = file_ok and summary.startswith('1 functions tested') and matched is not None
        ok = ok and int(matched.group(1)) == len(lines)
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|known differences|(?:\w+ )?crashed|skipped)\b', summary)
        bad += not ok
        print('%s %s: %s%s' % (name, function, summary, '' if ok else ' CHECK'), flush=True)
        if ok:
            shutil.rmtree(folder)
    print('%d case(s) to check' % bad)
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
