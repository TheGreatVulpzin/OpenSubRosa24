"""Compare voice selection with free, full and unordered speaker lists.

Usage: python -I instrumentation/client/synth_audio.py <recording dir> <work dir> [case,...]
Each case removes its snapshot after the comparison.
"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lift'))
from synth_bonds import Snapshot

ROOT = Path(__file__).resolve().parents[2]
CONNECTION = 0x16338600
CONNECTION_BYTES = 0xf170
VOICE_OFFSET = 0x54
SLOT_BYTES = 0x20
ADDVOICE = 0xfc670
GETNEXTVOICE = 0xfc720
RESULT = re.compile(r'(\d+) same\s+(\d+) different\s+(\d+) crashed')


def float_bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def call(rva, *values):
    return f'R {rva:x}' + ''.join(f' V:{value & 0xffffffff:x}' for value in values) + '\nZ r:0\n'


def cases():
    for listener in (0, 1, 255):
        for free in (0, 3, 7, -1):
            yield f'listener_{listener}_free_{free}', listener, free, 0.0, list(range(8, 0, -1)), 2.0
    for name, key, distances, distance in (
        ('equal', 0.0, [8.0] * 8, 8.0),
        ('nearer', 0.0, [8.0] * 8, 2.0),
        ('farther', 0.0, [8.0] * 8, 16.0),
        ('blocked_key', 65536.0, [8.0] * 8, 2.0),
        ('nan_key', float('nan'), [8.0] * 8, 2.0),
        ('nan_slot', 0.0, [float('nan')] + [8.0] * 7, 2.0),
        ('nan_distance', 0.0, [8.0] * 8, float('nan')),
    ):
        yield name, 0, -1, key, distances, distance


def main():
    recording, work = map(Path, sys.argv[1:3])
    work.mkdir(parents=True, exist_ok=True)
    failed = False
    passed = 0
    visited = 0
    wanted = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    for name, listener, free, key, distances, distance in cases():
        if wanted and name not in wanted:
            continue
        visited += 1
        snapshot = Snapshot(recording / 'sv_snapshot.bin')
        base = CONNECTION + listener * CONNECTION_BYTES
        snapshot.write(base + 0x50, 'f', key)
        for index, priority in enumerate(distances):
            slot = base + VOICE_OFFSET + index * SLOT_BYTES
            snapshot.write(slot, 'iiii', int(index != free), 73, -1, -1)
            snapshot.write(slot + 0x1c, 'f', priority)
        folder = work / name
        folder.mkdir(exist_ok=True)
        path = folder / 'sv_snapshot.bin'
        snapshot.save(path)
        arguments = folder / 'sv_args.txt'
        arguments.write_text(call(GETNEXTVOICE, listener, float_bits(distance)) +
                             call(ADDVOICE, listener, 91, 12, -1, float_bits(distance)), encoding='utf-8')
        try:
            result = subprocess.run([str(ROOT / 'build/hook/harness_client.exe'),
                                     str(ROOT / 'build/hook/subrosa_fixed.exe'), str(path), str(arguments),
                                     'getnextvoice,addvoice'], capture_output=True, text=True, timeout=600,
                                    env=dict(os.environ, SV_UI_WIDTH='220',
                                             HARNESS_ENV_DIR=str(folder.resolve() / 'env')))
            (folder / 'result.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
            totals = [tuple(map(int, match)) for match in RESULT.findall(result.stdout)]
            good = result.returncode == 0 and len(totals) >= 2 and all(same > 0 and diff == crash == 0
                                                                       for same, diff, crash in totals)
            failed |= not good
            passed += int(good)
            print(f'{name}: addvoice/getnextvoice: {"same" if good else "FAILED"}: ' + str(totals), flush=True)
        finally:
            path.unlink(missing_ok=True)
    print(f'addvoice: {passed} calls same; getnextvoice: {passed} calls same; {visited - passed} failed cases')
    return int(failed or not visited)


if __name__ == '__main__':
    sys.exit(main())
