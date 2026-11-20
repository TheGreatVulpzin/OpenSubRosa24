"""Compare state changes for events, packed pools, checkpoints and packet cursors."""
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_textures as st

GAME = 0x9065300
HUMANS = 0x14fc4a0
OBJECTS = 0x5519120
PARTICLES = 0xb3715a0
ITEMS = 0x908b080
SOCKET = 0x19d851a0


def cases():
    host = [(0x75a5800, 'i', (1,)), (0xf3ca9b8, 'I', (65535,))]
    words = [(GAME + 12, 'I', (3,)), (GAME + 52, 'I', (12345,))]
    for team in range(3):
        words.append((GAME + 0x90 + team * 0x117c, 'I', (team + 1,)))
        words.append((0x90882dc + team * 0x50, '4I', (0x12345678, 0xffffffff, 0x8000, team)))
        words.append((0x90882dc + team * 0x50 + 0x44, 'I', (0xffff,)))
    yield 'scenario', host + words, 'createevent_updatescenario', 0x728b0, [(team,) for team in range(3)]
    yield 'interior', host, 'createevent_updateblock_interior', 0x72d10, [
        (0, 1, 2, 3, 0x12345678), (3, -1, 65535, 65536, 0xffffffff)]

    box = [(GAME + 0x25978, '6f', (-1, -2, -3, 1, 2, 3))]
    points = [(-1., 0., -3.), (0., 0., 0.), (1., 0., 0.), (0., 0., 3.),
              (-1.0001, 0., 0.), (0., 0., -3.0001), (float('nan'), 0., 0.),
              (0., 0., float('nan')), (float('inf'), 0., 0.)]
    yield 'checkpoint', box, 'pointinsidecheckpoint', 0xa4420, [(point, 0) for point in points]

    human = [(HUMANS, '11108s', (bytes(11108),)), (HUMANS, 'i', (1,)),
             (HUMANS + 8, 'ii', (-1, -1)), (HUMANS + 40, 'i', (40,))]
    yield 'damage', host + human, 'human_applydamage', 0x89f50, [
        (0, damage, (1., 2., 3.), (4., 5., 6.)) for damage in (0, 1, 39, 40, 41, -1, 2147483647, -2147483648)]

    protection = [(HUMANS + index * 11108 + 0x34, 'i', (4800,)) for index in range(512)]
    yield 'physics_protection', protection, 'physicssimulation', 0xd4700, [()]

    socket = [(SOCKET + 0x5c, 'ii', (0, 5)), (SOCKET + 0x64, '16s', (b'\xa5' * 16,)),
              (SOCKET + 0x10064, 'iii', (256, 0, 3)),
              (SOCKET + 0x10070, '256s', (bytes((k * 37 + 23) & 255 for k in range(256)),))]
    word = 'P:0:' + struct.pack('<I', 0xfedcba98).hex()
    for name, rva in [('pbitwritescale', 0xfe920), ('pbitreadscale', 0xfea40)]:
        args = [(word, bits, scale) for bits in (0, 1, 7, 8, 9, 16, 31, 32, 33, -1)
                for scale in (1, 3, 31, 0xffffffff)]
        yield name, socket, name, rva, args
        full = [(SOCKET + 0x5c, 'i', (65536,)), (SOCKET + 0x10068, 'i', (65536,))]
        yield name + '_full', socket + full, name, rva, [(word, 17, 3)]
    yield 'sign', socket, 'pbitwritesign', 0xfebd0, [(word, bits) for bits in (0, 1, 7, 8, 9, 31, 32)]
    yield 'reset', socket, 'resetsendpacket', 0xfe4f0, [()]

    item = [(ITEMS, '428s', (bytes(428),)), (ITEMS, 'i', (1,)),
            (ITEMS + 12, 'i', (24,)), (ITEMS + 24, 'ii', (-1, -1)),
            (ITEMS + 36, 'ii', (1, 1)), (ITEMS + 0x114, 'i', (0,)),
            (ITEMS + 428 + 28, 'i', (0,)), (OBJECTS + 28, 'i', (7,))]
    yield 'item_key', host + item, 'deleteitem', 0x971c0, [(0,), (-1,), (1024,)]
    newspaper = [(ITEMS + 12, 'i', (26,)), (GAME + 0x25d5c, 'i', (1,))]
    yield 'item_newspaper', host + item + newspaper, 'deleteitem', 0x971c0, [(0,)]

    moved = [(0x1dda5270, 'i', (3,)), (PARTICLES, '720s', (bytes(720),)),
             (PARTICLES + 480 + 0x50, 'ii', (1, 0)),
             (PARTICLES + 480 + 0xd4, 'ii', (1, 0)),
             (0x1779b830, 'iii', (2, 2, 2)),
             (OBJECTS + 0xa8, 'iii', (2, 2, 2)),
             (HUMANS, 'i', (1,)), (HUMANS + 16, 'i', (2,)),
             (HUMANS + 11108, 'i', (0,)), (HUMANS + 11108 + 16, 'i', (2,))]
    yield 'particle', moved, 'deleteparticle', 0xd43c0, [(0,), (2,), (-1,), (3,)]
    object_edits = [(HUMANS + 20, 'i', (0,)), (HUMANS + 11108 + 20, 'i', (0,))]
    yield 'object', host + moved + object_edits, 'deleteobject', 0xc6ff0, [(0,), (-1,), (1024,)]


def main():
    recording, work = map(Path, sys.argv[1:3])
    selected = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    work.mkdir(parents=True, exist_ok=True)
    failed = False
    for label, edits, name, rva, arguments in cases():
        if selected and label not in selected:
            continue
        snapshot = st.Snapshot(recording / 'sv_snapshot.bin')
        for address, fmt, values in edits:
            snapshot.write(address, fmt, *values)
        scratch = 0x19e00000
        prepared = []
        for values in arguments:
            values = list(values)
            for index, value in enumerate(values):
                if isinstance(value, tuple):
                    raw = struct.pack('<' + 'f' * len(value), *value)
                elif isinstance(value, str) and value.startswith('P:'):
                    raw = bytes.fromhex(value.split(':', 2)[2])
                else:
                    continue
                snapshot.write(scratch, str(len(raw)) + 's', raw)
                values[index] = st.image(scratch)
                scratch += 256
            prepared.append(values)
        path = work / 'snapshot.bin'
        snapshot.save(path)
        del snapshot
        calls = work / (label + '.txt')
        calls.write_text(''.join(st.call(rva, *args) for args in prepared), encoding='utf-8')
        env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(st.ROOT / 'build/client_test'),
                   HARNESS_ENV_DIR=str(work / ('env_' + label)), SV_LOG_FILE=str(work / (label + '.log')))
        env['PATH'] = str(st.ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
        result = st.run(path, calls, name, env)
        path.unlink()
        print(label + ': ' + name + ': ' + result, flush=True)
        match = re.fullmatch(r'([0-9]+) calls same, 0 different, 0 known differences, 0 lifted crashed, 0 skipped', result)
        failed |= not match or int(match[1]) != len(arguments)
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
