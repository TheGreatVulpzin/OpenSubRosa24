"""Compare model files, appended geometry and aliased curve output."""
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


def pe_bytes(rva, size):
    data = Path(st.IMAGE).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    sections = struct.unpack_from('<H', data, pe + 6)[0]
    optional = struct.unpack_from('<H', data, pe + 20)[0]
    for index in range(sections):
        entry = pe + 24 + optional + index * 40
        virtual_size, address, raw_size, raw = struct.unpack_from('<IIII', data, entry + 8)
        if address <= rva < address + max(virtual_size, raw_size):
            offset = raw + rva - address
            return data[offset:offset + size]
    raise ValueError(hex(rva))


def main():
    recording, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    addresses = pure.rvas()
    signature_ptr = struct.unpack('<I', pe_bytes(0x1234dc, 4))[0]
    signature = pe_bytes(signature_ptr - st.BASE, 4)
    failed = []
    for mode in range(4):
        case = work / str(mode)
        case.mkdir(exist_ok=True)
        game = case / 'game'
        game.mkdir(exist_ok=True)
        directory = game if mode == 3 else game / 'model'
        directory.mkdir(exist_ok=True)
        snapshot = st.Snapshot(recording / 'sv_snapshot.bin')
        calls, counts = [], Counter()

        def write(rva, fmt, *values):
            snapshot.write(rva, fmt, *values)

        def call(name, *values):
            calls.append(st.call(addresses[name], *values))
            counts[name] += 1

        def text(value):
            return 'P:0:' + (value.encode() + b'\0').hex()

        arrays = [(8, '9f', [0,0,0, 1,0,0, 0,1,0]),
                  (16, '9f', [0,0,1] * 3), (20, '9f', [1,0,0] * 3),
                  (32, '3i', [0,1,2]), (36, '3f', [0,0,1]), (40, '3i', [-1,-1,-1])]
        payload = b''.join(struct.pack('<' + fmt, *values) for _, fmt, values in arrays)
        for name, magic, version in [('valid.csm', signature, 1), ('magic.csm', b'NOPE', 1),
                                     ('version.csm', signature, 2)]:
            (directory / name).write_bytes(magic + struct.pack('<4i', version, 3, 8, 1) + payload)
        model = 900
        record = 0xaff05a0 + model * 0xe04
        write(record, 'ii', 3, 8)
        write(record + 0x1c, 'i', 1)
        for offset in (8, 12, 16, 20, 24, 32, 36, 40, 48):
            write(record + offset, 'I', 0)
        for index, (offset, fmt, values) in enumerate(arrays):
            rva = 0x19e80000 + index * 256
            write(rva, fmt, *values)
            write(record + offset, 'I', st.BASE + rva)
        for filename in ('output.csm', 'missing/subdir.csm'):
            call('savemodelcsm', model, text(filename))
        for offset in (8, 12, 16, 20, 24, 32, 36, 40, 48):
            write(record + 0xe04 + offset, 'I', 0)
        for filename in ('valid.csm', 'magic.csm', 'version.csm', 'missing.csm'):
            call('loadmodelcsm', model + 1, text(filename))

        temp = st.MODELTEMP
        write(temp, 'ii', 4, 8)
        write(temp + 0xa0008, 'i', 2)
        write(temp + 0x20000c, 'i', 0)
        write(temp + 0x260010, 'i', 0)
        for vertex in range(4):
            write(temp + 8 + vertex * 12, '3f', float(vertex & 1), float(vertex >> 1), 0.0)
        write(temp + 0xa000c, '6i', 0, 1, 2, 1, 3, 2)
        write(temp + 0x200010, '4i', 0, 1, 3, 2)
        rotation = (1.0,0.0,0.0, 0.0,0.0,1.0, 0.0,-1.0,0.0)
        for face in (-1, 0, 1, 2, 3, 4, 5, 6):
            call('addhalfboxtomodel', (0.25,-0.5,1.0), rotation, 0.125, 0.25, 0.5, face)
        call('addwaisttomodel', (-1.0,0.25,0.5), 0.25, 0.125, 0.5)
        for vertex in range(4):
            write(0x1bbfe04 + vertex * 28, 'ii8s3f', 77, 9, b'padding!', -1.0, -2.0, -3.0)
            call('sethumanmodelvert', vertex, mode, 0.0, -0.0, 0.125)
            call('sethumanmodelvertface', vertex, mode + 15, -0.5, 0.25, 1.0)
        for point in range(38):
            write(0x1bf7f60 + point * 12, '3f', point * 0.125, -point * 0.25, (point % 3) * 0.5)
        indices = list(range(10)) if mode & 1 else list(range(37, 27, -1))
        write(0x19e90000, '10i', *indices)
        for weights in ((1.0,0.0,0.0), (0.0,1.0,0.0), (0.0,0.0,1.0), (0.125,0.25,0.625)):
            output = 0x1bf7f60 + indices[mode] * 12 if mode & 2 else 0x19e91000
            call('tesselatebeziertriangle', st.image(output), *weights, st.image(0x19e90000))

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
        raise SystemExit(f'Failed cases: {failed}')


if __name__ == '__main__':
    main()
