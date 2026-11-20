"""Compare image reads, file loads, table bounds and text helpers on controlled states."""
import csv
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
    selected = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else set()
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

        def write(address, fmt, *values):
            snapshot.write(address, fmt, *values)

        def call(name, *values):
            if not selected or name in selected:
                calls.append(st.call(addresses[name], *values))
                counts[name] += 1

        def text(value):
            return 'P:0:' + (value + b'\0').hex()

        for index, length in enumerate((0, 1, 2, 4)):
            reader, data = 0x19e40000 + index * 128, 0x19e48000 + index * 16
            write(reader, '7I', 0, 0, 0, 0, 0, st.BASE + data, st.BASE + data + length)
            write(data, '4B', 0x12, 0x34, 0x56, 0x78)
            call('get16', st.image(reader))
            call('get16le', st.image(reader))
        for index, (cached, data) in enumerate(((255, b'\xff\xff\xd8\0'), (255, b'Z\0'),
                                                (255, b'\xff\0'), (0xd9, b'Z\0'))):
            jpeg, buffer = 0x19e50000 + index * 0x4000, 0x19e70000 + index * 16
            write(jpeg, '7I', 0, 0, 0, 0, 0, st.BASE + buffer, st.BASE + buffer + len(data))
            write(jpeg + 0x369c, 'B', cached)
            write(buffer, f'{len(data)}s', data)
            call('get_marker', st.image(jpeg))
        call('e', text(b'decoder failure'))

        city = storage['city']
        write(city, 'i', 3)
        write(city + 0x1a008, 'i', 0)
        write(city + 0x2380c, 'i', 0)
        for index, position in enumerate(((0, 4, 0), (20, 3, 4), (4, 6, 24))):
            entry = city + 8 + index * 0x68
            write(entry, '3i', *position)
            write(entry + 0x18, '4i', -1, -1, -1, -1)
            write(entry + 0x28, '4i', 1, 2, 3, 4)
        for first, second in ((0, 1), (1, 0), (0, 2), (2, 1)):
            call('addcitystreet', first, second, mode, 2, text(b'road'))
            call('addcityblock', first, second, 7, 9)

        for pattern, name in ((b'a*b', b'axxb'), (b'*.png', b'image.png'), (b'*', b'anything'),
                              (b'long', b'lo'), (b'short', b'shorter'), (b'ab', b'ac'),
                              (b'*a', b'a'), (b'', b'a'), (b'a', b'')):
            call('checkfilespec', text(pattern), text(name))
        parser_text = (b'xx -0.125 tail', b'xx -2147483648.5 tail', b'xx ' + b'9' * 255, b'xx 0.0 tail')[mode]
        write(storage['parser'], 'ii', 0, len(parser_text))
        write(storage['parser'] + 8, f'{len(parser_text) + 1}s', parser_text + b'\0')
        call('getfloat')
        call('resetparser')
        for team in range(3):
            for slot in range(16):
                active = mode == 1 or mode == 2 and slot % 2 == 1 or mode == 3 and slot != 15
                write(0x90653cc + team * 0x117c + slot * 64, 'iii', int(active), 0, 4 if slot == 15 else 2)
            call('scenario_getnextmission', team)
            call('scenario_isseekmission', team)
        for human, vehicle in enumerate((-1, 0, 1023)):
            write(storage['human'] + human * 0x2b64 + 0x14, 'i', vehicle)
            write(storage['human'] + human * 0x2b64 + 0x1c, 'i', 77)
            call('setuphumanangles', human)

        atlas = 0x103673a0
        write(atlas, 'i', (0, 1, 63, 64)[mode])
        for cell in range(64):
            write(atlas + 4 + cell * 8, 'ii', cell, cell % 3)
        for material, window in ((0, 0), (62, 2), (63, 0), (88, -1)):
            call('addwindowatlas', material, window)
        for x, wall in enumerate(range(-1, 11)):
            write(storage['floorplan'] + x * 72 + 8, 'ii', wall, 9 - wall)
            for direction in (0, 1):
                for fixed in (0, 1):
                    call('building_iswindow', x, 0, 0, direction, fixed)
        building = storage['building']
        write(building + 0x36004e4, 'i', 0 if mode == 0 else 3)
        write(storage['materiallist'], 'i', 3)
        write(building + 0x36154f8, '256i', *([-7] * 256))
        for index, name in enumerate((b'zero', b'dup', b'absent')):
            write(building + 0x36004e8 + index * 64, '64s', name)
        for index, name in enumerate((b'zero', b'dup', b'dup')):
            write(0x1724f600 + index * 0x58 + 0x18, '64s', name)
        call('building_reloadtextures')

        write(0xf3eafc0, 'i', (0, 255, 1, 127)[mode])
        for kind in (0, 1, 4, -1):
            call('createmessage', kind, text(b'message'), -1, 7)
        paper = storage['newspaper']
        write(paper, 'i', (0, 1024, 333, 1023)[mode])
        write(paper + 0x40004, 'i', 77)
        for line in (-1, 0, 1023, 1024):
            for column in (0, 1, 2):
                call('newspaper_addline', 'P:0:' + bytes(range(64)).hex(), line, column)
        for letters in (b'', b'abc', b'\x80\xffA', b'x' * 1001):
            call('gettextlength', text(letters), 0.125)
        write(0xf9ae248, 'Q', (0, 1, 0x0123456789abcdef, 0xffffffffffffffff)[mode])
        call('getplayticket')
        write(0xb7315a0, '37I', *([0xdeadbeef] * 37))
        call('initWGLLoadFunctions')
        call('checkbigendian')
        write(0x29e7d2c0 + 8 * 4, 'ii', int(mode in (2, 3)), int(mode in (1, 3)))
        call('settwosidedstencil', 0x207, 0x203, 3, 0xff, 0x1e00, 0x8507, 0x8508, 0x1e00, 0x8508, 0x8507)

        game = case / 'game'
        game.mkdir(exist_ok=True)
        width, height = (3, 4)[mode % 2], 2
        row = bytes(range(width * 3)).ljust((width * 3 + 3) & ~3, b'\0')
        pixels = row * height
        bmp = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
        bmp += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 0, 0, 0, 0) + pixels
        (game / 'tiny.bmp').write_bytes(bmp)
        write(st.TEXTURES + 4000 * st.TEXTURE_SIZE + 0x30, 'I', 0)
        for path, mipmap in ((b'tiny.bmp', 0), (b'tiny.bmp', 1), (b'no_such_file.bmp', 0)):
            call('loadtexture2', 4000, text(path), mipmap, 0x812f, 0x812f, 0x2601, 0x2601)
        path = case / 'calls.txt'
        path.write_text(''.join(calls), encoding='utf-8', newline='\n')
        snapshot_path = case / 'snapshot.bin'
        snapshot.save(snapshot_path)
        env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(game),
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
        print(f'Failed cases: {failed}')
        raise SystemExit(1)


if __name__ == '__main__':
    main()
