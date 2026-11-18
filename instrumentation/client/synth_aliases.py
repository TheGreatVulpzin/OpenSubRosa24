"""Compare vector operations when their pointer arguments share memory."""
import os
import random
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation/client'))
import synth_pure as pure
import synth_textures as st


def main():
    recording, work = Path(sys.argv[1]), Path(sys.argv[2])
    names = sys.argv[3].split(',')
    work.mkdir(parents=True, exist_ok=True)
    snapshot = st.Snapshot(recording / 'sv_snapshot.bin')
    cursor = 0x19e00000
    calls = []
    addresses = pure.rvas()
    for name in names:
        rng = random.Random(sum(map(ord, name)))
        for index in range(12):
            args = pure.make_args(rng, pure.PROTOTYPES[name], index)
            pointers = []
            for position, value in enumerate(args):
                if not isinstance(value, tuple):
                    continue
                raw = struct.pack(f'<{len(value)}f', *value).ljust(256, b'\0')
                snapshot.write(cursor, '256s', raw)
                args[position] = st.image(cursor)
                pointers.append((position, cursor))
                cursor += 288
            if len(pointers) < 2 or pointers[0][0] != 0:
                raise ValueError(f'{name}: two pointer arguments are required')
            target = pointers[-1 if index % 3 == 1 else 1][1]
            args[0] = st.image(target + (4 if index % 3 == 2 else 0))
            calls.append(st.call(addresses[name], *args))
    snapshot_path = work / 'snapshot.bin'
    snapshot.save(snapshot_path)
    args_path = work / 'calls.txt'
    args_path.write_text(''.join(calls), encoding='utf-8', newline='\n')
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build/client_test'),
               HARNESS_ENV_DIR=str(work / 'env'), SV_LOG_FILE=str(work / 'calls.log'), SV_UI_WIDTH='220')
    env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
    try:
        print(st.run(snapshot_path, args_path, ','.join(names), env), flush=True)
    finally:
        snapshot_path.unlink()
    result = args_path.with_suffix('.result.txt').read_text(encoding='utf-8')
    rows = dict((name, tuple(map(int, counts))) for name, *counts in re.findall(
        r'^\[\s*\d+/\d+\]\s+(\w+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed', result, re.M))
    for name in names:
        print(f'{name}: {rows.get(name)}', flush=True)
    if any(rows.get(name) != (12, 0, 0) for name in names):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
