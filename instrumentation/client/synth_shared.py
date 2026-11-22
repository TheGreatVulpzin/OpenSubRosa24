"""Compare shared file, mesh and network operations on valid client records."""
import csv
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation/lift'))
sys.path.insert(0, str(ROOT / 'tools/net'))
from synth_bonds import Snapshot
from srpacket import PacketWriter

FUNCTIONS = ('building_save', 'clientrecv', 'leveleditor_load', 'leveleditor_save', 'addbeziersurface', 'playerai')
GLOBALS = {r['name']: int(r['client_rva'], 16) for r in csv.DictReader((ROOT / 'analysis/client_win/storage.csv').open()) if r['status'] != 'dropped'}
RVAS = {r['name']: int(r['rva'], 16) for r in csv.DictReader((ROOT / 'analysis/client_win/functions.csv').open())}
BASE = 0x20000000


def clear(snap, address, size):
    while size:
        page, offset = snap._page(address)
        length = min(size, 4096 - offset)
        page[offset:offset + length] = bytes(length)
        address += length
        size -= length


def level_bytes(variant):
    data = bytearray(struct.pack('<ii', 6, variant))
    for i in range(variant):
        data += struct.pack('<3i', 17 + i, 29 + i, 43 + i)
    data += struct.pack('<i', variant)
    for i in range(variant):
        data += struct.pack('<5i32s', i, (i + 1) % max(variant, 1), 2, 3, 4, b'Audit street')
    data += bytes([variant]) * 0x100000
    data += struct.pack('<i', variant)
    for i in range(variant):
        data += struct.pack('<64s4i', b'audit_building', 11 + i, 23 + i, 37 + i, i % 4)
    return data


def packet(kind, variant):
    writer = PacketWriter()
    writer.write_bits(kind, 8)
    if kind == 6:
        writer.write_bytes(struct.pack('<i', 97))
        for value, bits in ((3, 4), (1, 4), (123, 16), (456, 16)):
            writer.write_bits(value, bits)
    elif kind == 8:
        for channel in range(8):
            writer.write_bits(int(channel == variant), 1)
            if channel != variant:
                continue
            writer.write_bits(255, 8)
            writer.write_bits(255, 8)
            for frame in (61, 62, 63, 0):
                writer.write_bits(frame, 6)
                writer.write_bits(3, 8)
                writer.write_bits(2, 2)
                writer.write_bytes(bytes((frame, 17, 29)))
    elif kind == 5:
        writer.write_bytes(struct.pack('<2i', 1234, 5001))
        writer.write_bits(1, 4)
        for i in range(32):
            writer.write_bits(i % 2, 1)
        for value, bits in ((777, 16), (123456, 30), (5, 6), (7, 6), (9, 6), (variant, 8),
                            (1023, 10), (123, 16), (456, 16)):
            writer.write_bits(value, bits)
        writer.write_bytes(struct.pack('<3f', 0.125, -0.25, 0.5))
        writer.write_bits(0, 8)
        writer.write_bits(0, 16)
        writer.write_bytes(struct.pack('<i', 98765))
        for value, bits in ((7, 8), (91, 8), (23, 6), (51, 8)):
            writer.write_bits(value, bits)
        for i in range(7):
            writer.write_bits(1, 4)
            writer.write_bits(100 + i, 16)
        for i in range(4):
            writer.write_bits(0, 8)
        writer.write_bits(321, 16)
        writer.write_bits(0, 6)
        writer.write_bits(0, 16)
    return writer.getvalue()


def prepare(snap, name, variant, game):
    args = ''
    packets = []
    if name == 'building_save':
        address = GLOBALS['building']
        clear(snap, address, 0x12001cc)
        snap.write(address, '64s3i', b'Audit building', variant, variant, variant)
        if variant:
            cell = address + 0x4c + (variant - 1) * (0x48000 + 0x1200 + 0x48)
            snap.write(cell, '18I', *range(1, 19))
        snap.write(address + 0x36004e4, 'i64s', 1, b'concrete')
        snap.write(address + 0x36044e8, 'i64s', 1, b'brick')
        scratch = 0x19e20000
        snap.write(scratch, '64s', b'audit_building')
        args = f' M:{scratch:x}'
    elif name.startswith('leveleditor_'):
        address = GLOBALS['level_editor']
        clear(snap, address + 0x38, 0x12084c)
        raw = level_bytes(variant)
        (game / 'data/city.sbc').write_bytes(raw)
        if name == 'leveleditor_save':
            offset = 4
            for field, stride in ((0x38, 12), (0x303c, 52)):
                count = struct.unpack_from('<i', raw, offset)[0]
                snap.write(address + field, 'i', count)
                offset += 4
                snap.write(address + field + 4, f'{count * stride}s', raw[offset:offset + count * stride])
                offset += count * stride
            snap.write(address + 0xb844, '1048576s', raw[offset:offset + 0x100000])
            offset += 0x100000
            snap.write(address + 0x10b844, 'i', variant)
            offset += 4
            for i in range(variant):
                snap.write(address + 0x10b84c + i * 84, '80s', raw[offset:offset + 80])
                offset += 80
    elif name == 'addbeziersurface':
        address = GLOBALS['tempmesh']
        snap.write(address, 'i', 0)
        snap.write(address + 0x110004, 'i', 0)
        points = [(float(x), float((x * y + variant) % 3), float(y)) for y in range(4) for x in range(4)]
        uv = [(x / 3.0, y / 3.0) for y in range(4) for x in range(4)]
        scratch = 0x19e20000
        snap.write(scratch, '48f', *(v for p in points for v in p))
        snap.write(scratch + 256, '32f', *(v for p in uv for v in p))
        args = f' M:{scratch:x} M:{scratch + 256:x}'
        args += ''.join(f' V:{v:x}' for v in (variant + 1, variant + 1, variant + 1, variant + 1, 0x1234))
    elif name == 'clientrecv':
        socket = GLOBALS['bsdsocket']
        snap.write(socket, '2i', 0, 1)
        snap.write(socket + 0x24, 'i', 7)
        snap.write(socket + 0x54, 'i', 16)
        snap.write(GLOBALS['game'] + 0x1c, 'i', 1234)
        snap.write(GLOBALS['game'] + 0x34, 'i', 5000)
        snap.write(GLOBALS['area_sector'] + 0x28, 'i', (0, 1199, 1200)[variant % 3])
        clear(snap, GLOBALS['human'], 512 * 0x2b64)
        clear(snap, GLOBALS['item'], 1024 * 0x1ac)
        packets = [] if variant < 3 else [packet((6, 8, 5)[(variant - 3) % 3], variant % 3)]
    elif name == 'playerai':
        human, player = GLOBALS['human'], GLOBALS['player']
        clear(snap, human, 512 * 0x2b64)
        clear(snap, player, 0x2408)
        snap.write(player, 'i', 1)
        snap.write(player + 0x78, 'i', 0)
        snap.write(player + 0x2168, '2i', 1, 0)
        snap.write(player + 0x2170, 'i3ffi', 1, 200.0, 0.0, -200.0, 16.0, 600)
        snap.write(human, 'i', 1)
        snap.write(human + 0x14, 'i', -1)
        snap.write(human + 0x28, 'i', 100)
        snap.write(human + 0xc8 + 0x40, 'f', 1.0)
        snap.write(human + 0x2b64 + 0x30, '3f', 200.0, 0.0, -200.0)
        snap.write(0x14c0690, 'i', 0)
        args = ' V:0'
    return args, packets


def run_cases(rec, work, name, count=30):
    rec, work = Path(rec), Path(work)
    work.mkdir(parents=True, exist_ok=True)
    game = work / 'game'
    (game / 'data/building').mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_DEBUG='2', HARNESS_GAME_DIR=str(game), HARNESS_ENV_DIR=str(work / 'env'))
    env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
    variants = 6 if name == 'clientrecv' else 3
    passed = True
    selected = [int(v) for v in os.environ.get('SYNTH_SHARED_CASES', ','.join(map(str, range(variants)))).split(',')]
    for variant in selected:
        snap = Snapshot(rec / 'sv_snapshot.bin')
        args, packets = prepare(snap, name, variant, game)
        snapshot = work / 'snapshot.bin'
        snap.save(snapshot)
        del snap
        line = f'R {RVAS[name]:x}{args}\n'
        for data in packets:
            line += f'E recvfrom V:7 V:10000 V:0 V:10 o:{data.hex()} o:02006bc07f0000010000000000000000 r:{len(data):x}\n'
        line += 'Z r:0\n'
        calls = work / 'calls.txt'
        calls.write_text(line * max(1, count // variants), encoding='ascii')
        result = subprocess.run([str(ROOT / 'build/hook/harness_client.exe'), str(ROOT / 'build/hook/subrosa_fixed.exe'), str(snapshot), str(calls), name], env=env, cwd=ROOT, capture_output=True, text=True, errors='replace', timeout=180)
        (work / f'case_{variant}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        matches = re.findall(r'(\d+) calls same, (\d+) different, (\d+) known differences, (\d+) \w+ crashed', result.stdout)
        ok = bool(matches and int(matches[-1][0]) > 0 and all(int(v) == 0 for v in matches[-1][1:]) and result.returncode == 0)
        passed &= ok
        summary = next((line.strip() for line in result.stdout.splitlines() if 'functions tested:' in line), 'no comparison')
        print(f'{name} case {variant}: {summary}', flush=True)
        snapshot.unlink()
    return passed


if __name__ == '__main__':
    names = sys.argv[3].split(',') if len(sys.argv) > 3 else FUNCTIONS
    results = [run_cases(sys.argv[1], Path(sys.argv[2]) / name, name) for name in names]
    raise SystemExit(0 if all(results) else 1)
