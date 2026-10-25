"""Test building files, name maps and floor plan turns.

Usage: python -I instrumentation/lift/synth_buildings.py --run <recording> <work>
"""
import argparse
import copy
import re
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot, PAGE
from synth_area import run_files, setup as setup_area, AREA, SECTORS, cell as area_cell

BUILDING = 0x1d65b80
CELLS = BUILDING + 0x4c
BUILD = BUILDING + 0x12001d8
FLOORPLAN = 0x2043a0
BLOCKS = 0x133b2d00
BLOCKLIST = 0xb7a8ca0
KEY = 0x13d22d60 + 8 + 0x80000


def put(s, address, data):
    while data:
        page, offset = s._page(address)
        count = min(len(data), PAGE - offset)
        page[offset:offset + count] = data[:count]
        address += count
        data = data[count:]


def clear(s, address, size):
    end = address + size
    while address < end:
        index, offset = divmod(address - s.data_rva, PAGE)
        count = min(end - address, PAGE - offset)
        if index in s.pages:
            s.pages[index][offset:offset + count] = bytes(count)
        address += count


def index(f, r, c):
    return (f * 64 + r) * 64 + c


def cell(outer=0, inner=0, row=0, col=0, cap18=0, cap16=0, color=0, guard=0, prize=0):
    return [outer, inner, row, col, cap18, cap16, *range(0x8100, 0x8110), 0x12345678, color, guard, prize]


def write_cell(s, grid, f, r, c, values):
    s.write(grid + index(f, r, c) * 0x48, '6I16H4I', *values)


def file_bytes(version, sx, sz, floors, cells, areas):
    data = bytearray(struct.pack('<I64s3i', version, b'Test', sx, sz, floors))
    for names in ((b'brick', b'missing'), (b'wide', b'missing')):
        data += struct.pack('<i', len(names))
        for name in names:
            data += struct.pack('<64s', name)
    for f in range(floors):
        for r in range(sz + 1):
            for c in range(sx + 1):
                v = cells.get((f, r, c), [0] * 26)
                data += struct.pack('<2I', *v[:2])
                if version > 6:
                    data += struct.pack('<I', v[4])
                data += struct.pack('<3I16H2I', v[2], v[3], v[5], *v[6:24])
                if version > 9:
                    data += struct.pack('<2I', *v[24:26])
    if version > 8:
        for area in areas:
            data += struct.pack('<6i', *area)
    return bytes(data)


def setup(s):
    clear(s, CELLS, 0x1200000)
    clear(s, BUILD, 0x1200000)
    s.write(BUILDING, '64s3i', b'Test', 3, 2, 2)
    s.write(BUILDING + 0x12001cc, '3i', 3, 2, 2)
    s.write(BUILDING + 0x36004e4, 'i', 2)
    s.write(BUILDING + 0x36044e8, 'i', 2)
    for offset, names in ((0x36004e8, (b'brick', b'missing')), (0x36044ec, (b'wide', b'missing'))):
        for i, name in enumerate(names):
            s.write(BUILDING + offset + i * 64, '64s', name)
    s.write(KEY, '16s', b'test')
    for i in range(16):
        area = (i, 2, i + 1, 2, 3, 1)
        s.write(BUILDING + 0x120004c + i * 24, '6i', *area)
        s.write(BUILDING + 0x24001d8 + i * 24, '6i', *area)


def cases():
    result = {}
    areas = [(i, 2, i + 1, 2, 3, 1) for i in range(16)]
    cells = {(f, r, c): cell(0x40000001, 0xffff, 1, 12, 1, f, 0xabcdef, (r + c) % 5, 1)
             for f in range(2) for r in range(3) for c in range(4)}

    def load(version, header=0):
        def build(s):
            data = file_bytes(version, 3, 2, 2, cells, areas)
            return ['R 13d80 M:%x V:%x' % (KEY, header)], {'data/building/test.sbb': data}, None
        return build

    for version in range(6, 11):
        result['load_v%d' % version] = ('building_load', load(version))
    result['load_header'] = ('building_load', load(10, 1))
    result['load_bad_version'] = ('building_load', load(5))
    result['load_missing'] = ('building_load', lambda s: (['R 13d80 M:%x V:0' % KEY], {}, None))

    def save(s):
        selected = {(0, 0, 0): cell(0x40000000, 0xffff, 12, 9, 1, 1, 0xabcdef, 4, 1),
                    (2, 3, 4): cell(0x80000002, 0x80000003, 2, 3, 0, 1, 15, 2, 0)}
        for position, value in selected.items():
            write_cell(s, CELLS, *position, value)
        return ['R 3740 M:%x' % KEY], {'data/building/test.sbb': b''}, file_bytes(10, 5, 4, 3, selected, areas)

    result['save_grow'] = ('building_save', save)

    def map_names(s):
        s.write(0xf26fd00, 'i', 4)
        s.write(BLOCKLIST, 'i', 4)
        for i, name in enumerate((b'brick', b'other', b'brick', b'last')):
            s.write(0x16d50038 + i * 0x58, '68s', name)
        for i, name in enumerate((b'wide', b'other', b'wide', b'last')):
            s.write(BLOCKS + i * 0x84, '64s', name)
        for i, (outer, inner) in enumerate(((0x83000000, 0x82000000), (0x80000001, 0x40000001), (0x40000000, 0))):
            write_cell(s, CELLS, 0, 0, i, cell(outer, inner, 12, 8, 1, 1, 19, 4, 1))
            s.write(BUILD + index(0, 0, i) * 0x48 + 0x38, 'I', 0xfedcba98)
        return ['R 3d70'], {}, None

    result['setup_name_maps'] = ('building_setupbuild', map_names)

    def floorplan(door):
        def build(s):
            s.write(BLOCKS + 7 * 0x84 + 0x80, 'i', door)
            for f in range(2):
                for r in range(3):
                    for c in range(4):
                        inner = (0, 0xffff, 7, 8)[c]
                        write_cell(s, BUILD, f, r, c, cell(0x40000000, inner, (r * 4 + c) % 13,
                                                          (r * 4 + c + 1) % 13, 1, c & 1, 17, c, r))
            return ['R 41c0'], {}, None
        return build

    result['floorplan_door'] = ('building_setupfloorplannewer', floorplan(1))
    result['floorplan_solid'] = ('building_setupfloorplannewer', floorplan(0))

    def rotate(s):
        s.write(BUILDING + 0x12001cc, '3i', 4, 4, 2)
        s.write(BLOCKLIST + 4, '2i', 8, 16)
        for i in range(32):
            s.write(BLOCKS + i * 0x84 + 0x74, 'i', 2 if i in (8, 11, 16) else 1)
        for f in range(2):
            for r in range(5):
                for c in range(5):
                    outer = (0x83000008, 0x8000000b, 0x80000010, 0x40000000, 0)[c]
                    write_cell(s, BUILD, f, r, c, cell(outer, 0x80000009, 12, 9, 1, f, 15, c, r))
        return ['R 4450'], {}, None

    result['rotate_wide_turns_guards'] = ('building_rotatefloorplan', rotate)

    def world_state(s, sx=3, sz=2, floors=1):
        setup_area(s)
        clear(s, FLOORPLAN, 0x1200000)
        for neighbour in (-64, -63, -1):
            s.write(FLOORPLAN + neighbour * 0x48 + 8, '2i', 0, 0)
        s.write(BUILDING + 0x12001cc, '3i', sx, sz, floors)
        s.write(0xe299220 + 0x2f998, 'i', 2)
        s.write(0xe299220, 'i', 3)
        for i, (ix, iz) in enumerate(((1, 3), (5, 3), (3, 7))):
            address = 0xe299220 + 8 + i * 0x68
            s.write(address, '3i', ix, 0, iz)
            s.write(address + 0x18, '4i', 10 + i, 20 + i, -1, -1)
        for team in range(5):
            address = 0x90dc540 + team * 0x117c
            s.write(address + 0x88, 'f', 0.33333334 + team * 0.3125)
            s.write(address + 0x4d0, 'i', 1)
        s.write(BLOCKLIST + 0x1c4, 'i', 200)
        s.write(BLOCKLIST + 0x1d0, 'i', 203)
        for block in (7, 8, 123, 200, 201, 202, 203):
            s.write(BLOCKS + block * 0x84 + 0x6c, '3i', 1, 1, 1)
            s.write(BLOCKS + block * 0x84 + 0x78, 'i', 0)

    def world_call(sx, sz, floors, kind):
        return ['R 46c30 V:3 V:3 V:3 V:3 V:%x V:%x V:%x V:%x' % (sx, sz, floors, kind)]

    def place_type(kind, active):
        def build(s):
            world_state(s)
            s.write(0x761ea60, 'i', active)
            for r in range(2):
                for c in range(3):
                    values = cell(0x40000000, 200 + c, 0, 0, 0, 0, 0x1234, c + 1, r == 1)
                    s.write(FLOORPLAN + index(0, r, c) * 0x48, '7I16H3I',
                            *values[:6], 1, *values[6:22], *values[23:26])
            return world_call(3, 2, 1, kind), {}, None
        return build

    for kind in range(16):
        for active in (0, 1):
            result['place_type_%d_server_%d' % (kind, active)] = ('building_buildfloorplan', place_type(kind, active))

    def door_turn(turn):
        def build(s):
            world_state(s, 1, 1)
            s.write(FLOORPLAN, '2I', 0x40000000, 200 + turn)
            return world_call(1, 1, 1, 0), {}, None
        return build

    for turn in range(4):
        result['base_door_%d' % turn] = ('building_buildfloorplan', door_turn(turn))

    def guards(s):
        world_state(s, 7, 5, 2)
        for f in range(2):
            for r in range(5):
                for c in range(7):
                    address = FLOORPLAN + index(f, r, c) * 0x48
                    s.write(address + 0x40, '2i', 1 + (c & 3), 1)
        return world_call(7, 5, 2, 10), {}, None

    result['guard_limit_and_empty_prize'] = ('building_buildfloorplan', guards)

    def no_intersections(s):
        world_state(s, 1, 1)
        s.write(0xe299220, 'i', 0)
        return world_call(1, 1, 1, 9), {}, None

    result['no_intersections'] = ('building_buildfloorplan', no_intersections)

    def interior(wall, neighbour, existing=False, overlap=False):
        def build(s):
            world_state(s, 1, 1)
            p = FLOORPLAN
            s.write(p, '7I', 0x40000000, 0xffff, 0, 0, 2 if overlap else 0, 3 if overlap else 0, 1)
            offsets = ((-1, 8), (-64, 12), (1, 8), (-63, 12), (65, 8), (65, 12), (63, 8), (64, 12))
            relative, offset = offsets[neighbour]
            s.write(p + relative * 0x48 + offset, 'i', wall)
            code = ((((2 if overlap else 0) * 2 | 1) * 2 | (3 if overlap else 0)) << 16)
            if 1 <= wall <= 9:
                code |= (0xb, 0xb, 0xb0, 0xb0, 0xb00, 0xb00, 0xb000, 0xb000)[neighbour]
            s.write(0x80ceb20 + (code & 0x3fffff) * 4, 'i', 123)
            if existing:
                s.write(p, 'I', 0)
                s.write(SECTORS + 0x3004 + area_cell(3, 3, 3) * 4, 'I', 0x80000007)
            return world_call(1, 1, 1, 9), {}, None
        return build

    for wall in (*range(1, 10), 12):
        for neighbour in range(8):
            result['interior_wall_%d_neighbour_%d' % (wall, neighbour)] = (
                'building_buildfloorplan', interior(wall, neighbour))
    result['interior_existing'] = ('building_buildfloorplan', interior(1, 0, existing=True))
    result['interior_overlapping_fields'] = ('building_buildfloorplan', interior(1, 0, overlap=True))

    def direct_wall(wall, side):
        def build(s):
            world_state(s, 1, 1)
            s.write(FLOORPLAN, '7I', 0x40000000, 0xffff, 0, 0, 0, 0, 1)
            relative, offset = ((0, 8), (1, 12), (64, 8), (0, 12))[side]
            s.write(FLOORPLAN + relative * 0x48 + offset, 'i', wall)
            code = 0x20000 | (wall << (side * 4))
            s.write(0x80ceb20 + code * 4, 'i', 123)
            return world_call(1, 1, 1, 9), {}, None
        return build

    for wall in (*range(1, 10), 12):
        for side in range(4):
            result['interior_direct_wall_%d_side_%d' % (wall, side)] = (
                'building_buildfloorplan', direct_wall(wall, side))

    def uncached(s):
        lines, files, expected = interior(1, 0)(s)
        s.write(0x80ceb20 + 0x2000b * 4, 'i', -1)
        return lines, files, expected

    result['interior_uncached'] = ('building_buildfloorplan', uncached)

    def delete_edge(center):
        def build(s):
            setup_area(s)
            s.write(SECTORS + 0x5024, 'i', 0)
            clear(s, SECTORS + 0x6068, 0x6068)
            s.write(SECTORS + 0x6068, 'i', 1)
            s.write(SECTORS + 0x6068 + 0x5014, 'i', 3)
            x, y, z = center
            points = [(x, y, z)] + [(x + dx, y + dy, z + dz) for dx, dy, dz in
                      ((0, -1, 0), (0, 1, 0), (0, 0, 1), (0, 0, -1), (-1, 0, 0), (1, 0, 0))]
            for xx, yy, zz in points:
                if min(xx, yy, zz) < 0:
                    continue
                slot = area_cell(xx >> 3, yy >> 3, zz >> 3)
                sector = 0 if slot == 0 else 1
                s.write(AREA + 3 * 0x100d6c + 0x56c + slot * 4, 'i', sector)
                s.write(SECTORS + sector * 0x6068 + 4 + area_cell(xx, yy, zz) * 4, 'I', 0x40000000)
            s.write(SECTORS + 0x3004 + area_cell(x, y, z) * 4, 'I', 0x80000007)
            return ['R 29970 V:3 V:%x V:%x V:%x' % center], {}, None
        return build

    for axis in range(3):
        for edge in (0, 7):
            center = [3, 3, 3]
            center[axis] = edge
            result['delete_edge_%d_%d' % (axis, edge)] = ('area_deleteblock', delete_edge(tuple(center)))
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
        setup(snapshot)
        lines, files, expected = build(snapshot)
        folder = args.work / name
        folder.mkdir(parents=True, exist_ok=True)
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        summary, file_ok = run_files(folder, function, files)
        if expected is not None:
            outputs = list((folder / 'env').rglob('test.sbb'))
            file_ok = len(outputs) == 2 and all(path.read_bytes() == expected for path in outputs)
            print('%s file bytes: %s (%d bytes)' % (name, 'same' if file_ok else 'FAILED', len(expected)), flush=True)
        ok = file_ok and summary.startswith('1 functions tested: %d calls same' % len(lines))
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|known differences|(?:\w+ )?crashed|skipped)\b', summary)
        bad += not ok
        print('%s %s: %s%s' % (name, function, summary, '' if ok else ' CHECK'), flush=True)
        (args.work / (name + '.log')).write_text(summary + '\n')
        if ok:
            shutil.rmtree(folder)
        else:
            break
    print('%d case(s) to check' % bad)
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
