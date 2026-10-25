"""Build data file cases and check loader bounds with the game harness.

Run: python -I tools/formats/test_s4.py RECORDING WORK --data DATA --image IMAGE
The data directory supplies the positive controls. Test files stay in WORK.
"""
import argparse
import copy
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation/lift'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import subrosa_formats as formats
from synth_bonds import Snapshot, PAGE
from synth_buildings import put

PARSER = 0x13d22d60
KEY = PARSER + 8 + 0x80000
BUILDING = 0x1d65b80
BLOCK = 0x537c520
EDITOR = 0xf270a80
NEWSPAPER = 0x9bdd8a0
FILELIST = 0xe219200
ACCOUNT = 0xf437b60


def packed(*values):
    return struct.pack('<%di' % len(values), *values)


def read(snapshot, address, size):
    result = bytearray()
    while size:
        page, offset = snapshot._page(address)
        count = min(size, PAGE - offset)
        result += page[offset:offset + count]
        address += count
        size -= count
    return bytes(result)


def checks_for(address, data):
    return ['%x %x %s' % (address + offset, len(data[offset:offset + PAGE]),
                         data[offset:offset + PAGE].hex())
            for offset in range(0, len(data), PAGE)]


def keep(snapshot, address, size):
    return checks_for(address, read(snapshot, address, size))


def malformed():
    cases = []
    header = packed(10) + bytes(64)
    for axis in range(3):
        for value in (-2147483648, -1, 64, 2147483647):
            sizes = [1, 1, 1]
            sizes[axis] = value
            data = header + packed(*sizes)
            for mode in (0, 1):
                cases.append(('building_size_%d_%d_%d' % (axis, value, mode),
                              'building', data, mode))
    header += packed(1, 1, 1)
    for field in ('materials', 'blocks'):
        for value in (-2147483648, -1, 257, 2147483647):
            data = header + (packed(0) if field == 'blocks' else b'') + packed(value)
            if value == 257:
                data += b'X' * (257 * 64)
            if value != 2147483647:
                data += (packed(0) if field == 'materials' else b'') + bytes(4 * 72 + 384)
            cases.append(('building_%s_%d' % (field, value), 'building', data, 0))
    for field in ('surfaces', 'boxes'):
        for value in (-2147483648, -1, 1025, 2147483647):
            data = packed(2, *([0] * 9), value) if field == 'surfaces' else packed(2, *([0] * 10), value)
            if value == 1025:
                data += b'X' * (1025 * (344 if field == 'surfaces' else 124))
            if field == 'surfaces' and value != 2147483647:
                data += packed(0)
            cases.append(('block_%s_%d' % (field, value), 'block', data, 0))
    for size in (0, 3, 7, 43):
        cases.append(('block_short_%d' % size, 'block', packed(2, *([0] * 11))[:size], 0))
    cases.append(('block_short_surface', 'block', packed(2, *([0] * 9), 1) + bytes(343), 0))
    cases.append(('block_short_box', 'block', packed(2, *([0] * 10), 1) + bytes(123), 0))
    for field, cap in (('intersections', 1024), ('streets', 512), ('buildings', 1024)):
        for value in (-2147483648, -1, cap + 1, 2147483647):
            data = packed(6)
            if field != 'intersections':
                data += packed(1, 7, 8, 9)
            if field == 'buildings':
                data += packed(1, 0, 0, 1, 2, 3) + bytes(32) + bytes(0x100000)
            data += packed(value)
            if value == cap + 1:
                data += b'X' * (value * {'intersections': 12, 'streets': 52, 'buildings': 80}[field])
            if value != 2147483647 and field != 'buildings':
                if field == 'intersections':
                    data += packed(0)
                data += bytes(0x100000) + packed(0)
            cases.append(('city_%s_%d' % (field, value), 'city', data, 0))
    cases.append(('city_short_tiles', 'city', packed(6, 0, 0) + bytes(0xfffff), 0))
    cases.append(('city_short_count', 'city', packed(6) + bytes(2), 0))
    cases.append(('city_short_intersections', 'city', packed(6, 1) + bytes(11), 0))
    cases.append(('city_short_street', 'city', packed(6, 0, 1) + bytes(51), 0))
    cases.append(('city_short_building', 'city', packed(6, 0, 0) + bytes(0x100000) + packed(1) + bytes(79), 0))
    cases.append(('city_extra', 'city', packed(6, 0, 0) + bytes(0x100000) + packed(0) + b'X', 0))
    city_max = packed(6, 1024) + bytes(1024 * 12) + packed(512) + bytes(512 * 52)
    city_max += bytes(0x100000) + packed(1024) + bytes(1024 * 80)
    cases.append(('city_big_file', 'city', city_max + b'X', 0))
    account = formats.ServerAccount((1, 2), 'Test', 1000, 5, 1, 2, 3)
    srk = formats.write_server_srk(formats.ServerSrk(3, 123, [account]))
    for value in (-2147483648, -1, 65536, 2147483647):
        data = packed(3, 123, value)
        if value == 65536:
            data += srk[12:] * value
        cases.append(('accounts_count_%d' % value, 'accounts', data, 0))
    for size in (0, 1, 4, 7, 8, 11, 12, 15, 16, 19, 20, 51, 52, 55, 56, 59, 60, 63, 64, 67, 68, 71):
        cases.append(('accounts_short_%d' % size, 'accounts', srk[:size], 0))
    cases.append(('accounts_short_second', 'accounts', packed(3, 123, 2) + srk[12:] + srk[12:-1], 0))
    for version in (1, 2):
        cases.append(('accounts_short_v%d' % version, 'accounts', packed(version) + srk[4:55], 0))
    cases.append(('accounts_name_end', 'name_end', srk[:20] + b'A' * 32 + srk[52:], 0))
    for size in (0x100000, 0x100001, 0x200000):
        cases.append(('text_large_%d' % size, 'text', b'A' * size, 0))
    for lines in (1013, 1014, 1100):
        cases.append(('newspaper_%d' % lines, 'newspaper', b'A\n' * lines, 0))
    for name in ('../escape', 'a/b', 'a\\b', 'a..b', 'A' * 64, 'A' * 1000, 'A' * 46, 'A' * 49):
        for function in ('building', 'save', 'block'):
            if function == 'block' and len(name) == 46:
                continue
            cases.append(('path_%s_%d' % (function, len(cases)), 'path_' + function,
                          name.encode(), 0))
    cases.append(('list_many', 'list', b'', 0))
    cases.append(('list_dirs', 'dirs', b'', 0))
    cases.append(('block_reuse', 'block_reuse', b'', 0))
    return cases


def prepare(snapshot, kind, data, mode, game):
    put(snapshot, KEY, b'test\0')
    files = {}
    checks = []
    if kind in ('building', 'path_building', 'path_save'):
        snapshot.write(BUILDING + 0x12001cc, '3i', 0, 0, 0)
        checks += keep(snapshot, BUILDING - 32, 32)
        checks += keep(snapshot, BUILDING, 64)
        checks += keep(snapshot, BUILDING + 0x36004e4, 0x8008)
        checks += keep(snapshot, BUILDING + 0x36168f8, 64)
        if kind != 'path_save':
            checks += checks_for(BUILDING + 64, packed(24, 24, 4))
            checks += checks_for(BUILDING + 0x4c, bytes(256))
            checks += checks_for(BUILDING + 0x120004c - 256, bytes(256))
        if kind == 'building':
            files['data/building/test.sbb'] = data
        else:
            put(snapshot, KEY, data + b'\0')
            if len(data) < 64:
                files['data/building/' + data.decode().replace('\\', '/') + '.sbb'] = (
                    packed(10) + b'Test'.ljust(64, b'\0') + packed(0, 0, 0, 0, 0) + bytes(384))
        call = 'R %x M:%x%s' % (0x3740 if kind == 'path_save' else 0x13d80, KEY,
                                '' if kind == 'path_save' else ' V:%x' % mode)
    elif kind == 'block_reuse':
        expected = bytearray(read(snapshot, BLOCK, 0x7606c))
        expected[:40] = packed(*range(11, 20), 0)
        expected[0x28 + 4:0x28 + 20] = packed(2, 4, 1, 3)
        for k in range(16):
            values = tuple(range(10 + k * 5, 15 + k * 5))
            expected[0x28 + 20 + k * 12:0x28 + 32 + k * 12] = packed(*values[:3])
            expected[0x28 + 212 + k * 8:0x28 + 220 + k * 8] = packed(*values[3:])
        expected[0x28 + 340:0x28 + 348] = packed(5, 6)
        expected[0x57028:0x5702c] = packed(0)
        box = packed(*range(101, 132))
        expected[0x5702c:0x5702c + len(box)] = box
        checks += keep(snapshot, BLOCK - 32, 32)
        checks += checks_for(BLOCK, expected)
        checks += keep(snapshot, BLOCK + 0x7606c, 32)
        files['data/block/first.sbl'] = (packed(2, *range(1, 10), 1) +
                                         packed(1, 2, 3, 4, 5, 6, *range(10, 90)) + packed(1) + box)
        files['data/block/bad.sbl'] = packed(2, *([0] * 9), 1025) + b'X' * (1025 * 344) + packed(0)
        files['data/block/last.sbl'] = packed(1, *range(11, 20), 0)
        calls = []
        for i, name in enumerate((b'first', b'bad', b'absent', b'last')):
            put(snapshot, KEY + i * 64, name + b'\0')
            calls.append('R 29a0 M:%x' % (KEY + i * 64))
        call = '\n'.join(calls)
    elif kind in ('block', 'path_block'):
        checks += keep(snapshot, BLOCK - 32, 0x7606c + 64)
        if kind == 'block':
            files['data/block/test.sbl'] = data
        else:
            put(snapshot, KEY, data + b'\0')
            if len(data) < 64:
                files['data/block/' + data.decode().replace('\\', '/') + '.sbl'] = packed(2, 2, *([0] * 10))
        call = 'R 29a0 M:%x' % KEY
    elif kind == 'city':
        checks += keep(snapshot, EDITOR - 32, 0x1c6898 + 64)
        files['data/city.sbc'] = data
        call = 'R 8590'
    elif kind in ('accounts', 'name_end'):
        snapshot.write(ACCOUNT + 4, 'i', 0)
        snapshot.write(0x60cd0, 'i', 0)
        snapshot.write(0x60dc0, 'I', 0)
        checks += keep(snapshot, ACCOUNT - 32, 32)
        checks += keep(snapshot, ACCOUNT + 8 + 65536 * 0x4c, 64)
        if kind == 'accounts':
            checks += checks_for(ACCOUNT, packed(0, 0))
            checks += keep(snapshot, ACCOUNT + 8, 65536 * 0x4c)
        else:
            checks += checks_for(ACCOUNT, packed(123, 1))
            checks += checks_for(ACCOUNT + 8 + 8, b'A' * 31 + b'\0')
        files['user/server.srk'] = data
        call = 'R e2e0'
    elif kind == 'text':
        checks += keep(snapshot, PARSER - 32, 32)
        checks += keep(snapshot, PARSER + 0x100008, 256)
        checks += checks_for(PARSER, packed(0, 0) + b'\0')
        files['readme.txt'] = data
        put(snapshot, KEY, b'readme.txt\0')
        call = 'R d020 M:%x' % KEY
    elif kind == 'newspaper':
        snapshot.write(ACCOUNT + 4, 'i', 10)
        for i in range(10):
            snapshot.write(ACCOUNT + 8 + i * 0x4c + 8, '32s', b'Test')
        checks += keep(snapshot, NEWSPAPER - 32, 32)
        checks += keep(snapshot, NEWSPAPER + 0x40004, 64)
        checks += checks_for(NEWSPAPER, packed(1024))
        checks += checks_for(NEWSPAPER + 4 + 1013 * 256, b'Top 10 list\0')
        files['readme.txt'] = data
        call = 'R 185a0'
    else:
        slot = FILELIST + 6 * 0x10004
        checks += keep(snapshot, slot - 32, 32)
        checks += keep(snapshot, slot + 0x10004, 64)
        checks += checks_for(slot, packed(1023))
        checks += checks_for(slot + 4 + 1023 * 64, b'\0')
        for i in range(1025):
            path = game / ('entry_%04d_' % i + 'A' * 80)
            if kind == 'dirs':
                path.mkdir()
            else:
                path.write_bytes(b'')
        put(snapshot, KEY, b'*\0')
        call = 'R d3a0 M:%x V:6 V:%d' % (KEY, kind == 'dirs')
        for i in range(1023):
            checks += checks_for(slot + 4 + i * 64 + 63, b'\0')
    for name, raw in files.items():
        path = game / name
        if not path.resolve().is_relative_to(game.resolve()):
            raise ValueError('Test path is outside the game directory')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    return call, checks


def invoke(folder, game, function, image, clean=False):
    env = {key: value for key, value in os.environ.items() if not key.startswith('HARNESS_')}
    env.update(HARNESS_GAME_DIR=str(game.resolve()), HARNESS_ENV_DIR=str(folder / 'env'), HARNESS_RETRY='0')
    if clean:
        env['HARNESS_CLEAN_CHECKS'] = str(folder / 'checks.txt')
    command = [str(ROOT / 'build/hook/harness.exe'), str(image.resolve()),
               str(folder / 'sv_snapshot.bin'), str(folder / 'sv_args.txt'), function + ',']
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True,
                            text=True, errors='replace', timeout=120)
    output = result.stdout + result.stderr
    (folder / 'result.txt').write_text(output, encoding='utf-8')
    lines = [line for line in output.splitlines() if line.startswith('security:') or 'functions tested:' in line]
    summary = lines[-1] if lines else 'NO RESULT'
    if clean:
        ok = result.returncode == 0 and summary.endswith(', PASS')
    else:
        count = len((folder / 'sv_args.txt').read_text().splitlines())
        ok = result.returncode == 0 and summary.startswith('1 functions tested: %d calls same' % count)
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|known differences|(?:\w+ )?crashed|skipped)\b', summary)
    return ok, summary


def positive(base, work, data, image, city_only=False):
    groups = [('buildings', 'building_load', sorted(data.glob('building/*.sbb')), 0x13d80),
              ('blocks', 'loadblock', sorted(data.glob('block/*.sbl')), 0x29a0),
              ('city', 'leveleditor_load', sorted(data.glob('*.sbc')), 0x8590)]
    failed = 0
    for name, function, paths, rva in groups:
        if city_only and name != 'city':
            continue
        if not paths:
            raise ValueError('No data files for ' + name)
        snapshot = copy.deepcopy(base)
        snapshot.write(BUILDING + 0x12001cc, '3i', 0, 0, 0)
        if name == 'city':
            for offset in (0x38, 0x303c, 0x10b844):
                snapshot.write(EDITOR + offset, 'i', -7)
        folder = work / ('valid_' + name)
        folder.mkdir(parents=True, exist_ok=True)
        lines = []
        for i, path in enumerate(paths):
            if name == 'city':
                lines.append('R %x' % rva)
                formats.read_sbc(path.read_bytes())
            else:
                address = KEY + i * 64
                put(snapshot, address, path.stem.encode('ascii') + b'\0')
                lines.append('R %x M:%x%s' % (rva, address, ' V:0' if name == 'buildings' else ''))
                (formats.read_sbb if name == 'buildings' else formats.read_sbl)(path.read_bytes())
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        ok, summary = invoke(folder, data.parent, function, image)
        failed += not ok
        print('valid %s: %s %s' % (name, 'PASS' if ok else 'FAIL', summary), flush=True)
        (folder / 'sv_snapshot.bin').unlink()
    return failed


def boundaries(base, work, image, city_only=False):
    cases = []
    for axis in range(3):
        sizes = [0, 0, 1]
        sizes[axis] = 63
        cells = sizes[2] * (sizes[0] + 1) * (sizes[1] + 1)
        raw = packed(10) + bytes(64) + packed(*sizes, 0, 0) + bytes(cells * 72 + 384)
        cases.append(('building_edge_%d' % axis, 'building_load', 'data/building/test.sbb', raw,
                      'R 13d80 M:%x V:0' % KEY))
    raw = packed(10) + bytes(64) + packed(0, 0, 0, 256) + bytes(256 * 64)
    raw += packed(256) + bytes(256 * 64 + 384)
    cases.append(('building_names_256', 'building_load', 'data/building/test.sbb', raw,
                  'R 13d80 M:%x V:0' % KEY))
    for field in ('surfaces', 'boxes'):
        raw = packed(2, *([0] * 9), 1024 if field == 'surfaces' else 0)
        if field == 'surfaces':
            raw += bytes(1024 * 344)
        raw += packed(1024 if field == 'boxes' else 0)
        if field == 'boxes':
            raw += bytes(1024 * 124)
        cases.append(('block_%s_1024' % field, 'loadblock', 'data/block/test.sbl', raw,
                      'R 29a0 M:%x' % KEY))
    raw = packed(6, 1024) + bytes(1024 * 12) + packed(512) + bytes(512 * 52)
    raw += bytes(0x100000) + packed(1024) + bytes(1024 * 80)
    cases.append(('city_limits', 'leveleditor_load', 'data/city.sbc', raw, 'R 8590'))
    for version in (1, 2, 3):
        record = struct.pack('<II32si', 1, 2, b'Test', 1000)
        if version > 1:
            record += packed(5, 0, 0, 0)
        cases.append(('accounts_v%d' % version, 'loadaccountsserver', 'user/server.srk',
                      packed(version, 123, 1) + record, 'R e2e0'))
    for count in (0, 65535):
        record = struct.pack('<II32si4i', 1, 2, b'Test', 1000, 5, 1, 2, 3)
        cases.append(('accounts_%d' % count, 'loadaccountsserver', 'user/server.srk',
                      packed(3, 123, count) + record * count, 'R e2e0'))
    cases.append(('text_limit', 'loadtextfile', 'test', b'A' * 0xfffff, 'R d020 M:%x' % KEY))
    failed = 0
    for name, function, path, raw, call in cases:
        if city_only and name != 'city_limits':
            continue
        folder = work / name
        game = folder / 'game'
        (game / path).parent.mkdir(parents=True, exist_ok=True)
        (game / path).write_bytes(raw)
        snapshot = copy.deepcopy(base)
        snapshot.write(BUILDING + 0x12001cc, '3i', 0, 0, 0)
        if name == 'city_limits':
            for offset in (0x38, 0x303c, 0x10b844):
                snapshot.write(EDITOR + offset, 'i', -7)
        put(snapshot, KEY, b'test\0')
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text(call + '\n')
        ok, summary = invoke(folder, game, function, image)
        failed += not ok
        print('valid %s: %s %s' % (name, 'PASS' if ok else 'FAIL', summary), flush=True)
        (folder / 'sv_snapshot.bin').unlink()
        if not game.resolve().is_relative_to(work):
            raise ValueError('Test directory is outside WORK')
        shutil.rmtree(game)
    return failed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--cases', help='case names, separated by commas')
    parser.add_argument('--positive-only', action='store_true')
    parser.add_argument('--city-only', action='store_true')
    args = parser.parse_args()
    work = args.work.resolve()
    if not work.is_relative_to(ROOT / 'build'):
        parser.error('WORK must be inside build/')
    work.mkdir(parents=True, exist_ok=True)
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    failures = 0
    selected = set(args.cases.split(',')) if args.cases else None
    cases = malformed()
    if args.city_only:
        cases = [case for case in cases if case[1] == 'city']
    if selected and selected - {case[0] for case in cases}:
        parser.error('Unknown cases')
    count = 0
    if not args.positive_only:
        for name, kind, raw, mode in cases:
            if selected and name not in selected:
                continue
            folder = work / name
            folder.mkdir(parents=True, exist_ok=True)
            game = folder / 'game'
            game.mkdir(exist_ok=True)
            snapshot = copy.deepcopy(base)
            call, checks = prepare(snapshot, kind, raw, mode, game)
            snapshot.save(folder / 'sv_snapshot.bin')
            (folder / 'sv_args.txt').write_text(call + '\n')
            (folder / 'checks.txt').write_text('\n'.join(checks) + '\n')
            function = {'building': 'building_load', 'path_building': 'building_load', 'path_save': 'building_save',
                        'block': 'loadblock', 'path_block': 'loadblock', 'city': 'leveleditor_load',
                        'block_reuse': 'loadblock',
                        'accounts': 'loadaccountsserver', 'name_end': 'loadaccountsserver', 'text': 'loadtextfile',
                        'newspaper': 'newspaper_load', 'list': 'listfiles', 'dirs': 'listfiles'}[kind]
            before = {path.relative_to(game): path.read_bytes() for path in game.rglob('*') if path.is_file()}
            ok, summary = invoke(folder, game, function, args.image, clean=True)
            after = {path.relative_to(game): path.read_bytes() for path in game.rglob('*') if path.is_file()}
            writes = [path for path in (folder / 'env').rglob('*') if path.is_file()]
            ok = ok and before == after and not writes
            failures += not ok
            count += 1
            print('%s: %s %s' % (name, 'PASS' if ok else 'FAIL', summary), flush=True)
            (folder / 'sv_snapshot.bin').unlink()
            (folder / 'checks.txt').unlink()
            if not game.resolve().is_relative_to(work):
                raise ValueError('Test directory is outside WORK')
            shutil.rmtree(game)
            if not ok:
                break
    if not selected and not failures:
        failures += positive(base, work, args.data.resolve(), args.image, args.city_only)
        failures += boundaries(base, work, args.image, args.city_only)
    print('data security: %d cases, %d failed' % (count, failures), flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
