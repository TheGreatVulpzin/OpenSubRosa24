"""Compare the bond solver on each client snapshot and on isolated bond types.

Run: python -I instrumentation/client/synth_bond_solver.py <work dir> [--calls N] [--types]
Each call starts from the same snapshot. Case snapshots are removed after use.
"""
import argparse
import csv
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation/client'))
import synth_textures as st


def addresses():
    with (ROOT / 'analysis/client_win/storage.csv').open(encoding='utf-8') as stream:
        return {r['name']: int(r['client_rva'], 16) for r in csv.DictReader(stream)}


def type_case(snapshot, data, kind, mode=0):
    """Supply two bones, four particles, one item and one wheel body."""
    for name, size, count in [('human', 0x2b64, 512), ('object', 0x8230, 1024), ('item', 0x1ac, 1024)]:
        for i in range(count):
            snapshot.write(data[name] + i * size, 'i', 0)
    snapshot.write(data['area_sector'] + 0x30, 'i', 4)
    snapshot.write(data['numofbonds'], 'i', 1)
    snapshot.write(data['numofstaticbonds'], 'i', 1)
    for h in range(2):
        base = data['human'] + h * 0x2b64
        snapshot.write(base, 'ii', 1, 1)
        snapshot.write(base + 0x14, 'i', -1)
        snapshot.write(base + 0x28, 'i', 100)
        snapshot.write(base + 0x38, 'i', 0)
        for j in range(15):
            bone = base + 0xc8 + j * 0xe0
            snapshot.write(bone, '3f', h * 0.4 + j * 0.01, 1.0, 0.0)
            snapshot.write(bone + 0x18, '3f', 0.01 * (j + 1), -0.02, 0.03)
            snapshot.write(bone + 0x30, '9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)
            snapshot.write(bone + 0x54, '6f', 0.001, -0.002, 0.003, 0.01, 0.02, -0.03)
            snapshot.write(bone + 0x8c, 'f', 1.5 + j * 0.125)
            snapshot.write(bone + 0x90, '6f', 1, 2, 3, 1, 0.5, 0.25)
            snapshot.write(bone + 0xa8, '9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)
            snapshot.write(bone + 0xcc, 'f', float(j % 3 != 2))
    for i in range(4):
        base = data['particle'] + i * 0xf0
        snapshot.write(base + 8, 'f', 1 + i * 0.5)
        snapshot.write(base + 0x14, '3f', i * 0.125, 1.0, 0.0)
        snapshot.write(base + 0x2c, '3f', i * 0.01, -0.02, 0.03)
    item = data['item']
    snapshot.write(item, 'ii', 1, 1)
    snapshot.write(item + 0x10, 'f', 2.0)
    snapshot.write(item + 0x48, '3f', 0.5, 1.0, 0)
    snapshot.write(item + 0x6c, '3f', 0.03, -0.02, 0.01)
    snapshot.write(item + 0x84, '9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)
    snapshot.write(item + 0xa8, '6f', 0.001, 0.002, -0.003, 0.01, -0.02, 0.03)
    snapshot.write(item + 0x124, '3f', 1, 0.5, 0.25)
    obj = data['object']
    snapshot.write(obj, '33328s', bytes(33328))
    snapshot.write(obj, 'i', int(kind in (5, 15)))
    snapshot.write(obj + 0x1c, 'i', 0)
    snapshot.write(obj + 0x3c, '9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)
    snapshot.write(obj + 0x71ec, 'i', int(kind == 15))
    wheel = obj + 0x71f0
    snapshot.write(wheel + 0xc, '3f', 0.01, -0.02, 0.03)
    snapshot.write(wheel + 0x24, '9f', 1, 0, 0, 0, 1, 0, 0, 0, 1)
    snapshot.write(wheel + 0x48, '6f', 0.001, -0.002, 0.003, 0.01, 0.02, -0.03)
    snapshot.write(wheel + 0x78, '3f', 1, 0.5, 0.25)
    snapshot.write(wheel + 0x84, 'f', 2)
    snapshot.write(wheel + 0xa0, '4f', 0.25, 0.25, 0.25, 0.25)
    tire = obj + 0x5eec
    snapshot.write(tire + 0x1c, '2f', 2, 0.25)
    snapshot.write(tire + 0x2c, 'f', 0.125)
    snapshot.write(tire + 0x44, '3f', 0.01, -0.02, 0.03)
    bond, opt = data['bond'], data['bondopt']
    snapshot.write(bond, '236s', bytes(236))
    snapshot.write(opt, '64s', bytes(64))
    snapshot.write(bond, 'i', kind)
    snapshot.write(bond + 0xc, 'i', mode)
    snapshot.write(bond + 0x14, '4i', 0, 1, 2, 3)
    snapshot.write(bond + 0x28, '9f', 0.1, 0, 0.05, 0.1, 0.05, 0, 0, 0.1, 0.05)
    snapshot.write(bond + 0x4c, '3f', 0.3, 0.3, 0.4)
    snapshot.write(bond + 0x60, 'f', 0.02)
    snapshot.write(bond + 0x6c, '2f', 0.0625, 0.25)
    snapshot.write(bond + 0x74, '3f', 0, 1, 0)
    snapshot.write(bond + 0x80, 'f', 0.6)
    target = 1 if kind in (8, 9, 13, 16) else 0
    snapshot.write(bond + 0x90, '4i', 0, 0, target, 1)
    snapshot.write(bond + 0xb0, '3f', 1, 0, 0)
    snapshot.write(opt + 8, '3f', 0.01, 0.02, -0.01)
    snapshot.write(opt + 0x18, 'f', 0.5)
    snapshot.write(opt + 0x34, '3f', 0.4, 0.6, 0.25)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('work', type=Path)
    parser.add_argument('--calls', type=int, default=30)
    parser.add_argument('--types', action='store_true')
    parser.add_argument('--cases', help='comma-separated case names')
    parser.add_argument('--time', action='store_true')
    parser.add_argument('--function', choices=('bondsimulation2', 'physicssimulation'), default='bondsimulation2')
    args = parser.parse_args()
    if args.calls < 1:
        parser.error('--calls must be positive')
    if args.types and args.function != 'bondsimulation2':
        parser.error('--types requires bondsimulation2')
    args.work.mkdir(parents=True, exist_ok=True)
    with (ROOT / 'analysis/client_win/functions.csv').open(encoding='utf-8') as stream:
        rva = next(int(r['rva'], 16) for r in csv.DictReader(stream) if r['name'] == args.function)
    data = addresses()
    records = sorted((ROOT / 'build/recordings').glob('client-*/sv_snapshot.bin'))
    if not records:
        raise SystemExit('No client snapshots found')
    failed = 0
    tested = 0
    for source in records:
        cases = [('snapshot', None, 0)]
        if args.types and 'play2' in source.parent.name:
            cases += [(f'type_{k}', k, 0) for k in (0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 13, 14, 15, 16)]
            cases += [(f'mode_{m:x}', 0, m) for m in (3, 0x100, 0x10000, -1)]
            cases += [('balance', 0, 0), ('joint_mask', 8, 0), ('muscle_arm', 9, 0), ('muscle_dead', 9, 0)]
            cases += [('spring_diagonal', 16, 0)]
        for name, kind, mode in cases:
            if args.cases and name not in args.cases.split(','):
                continue
            work = args.work / source.parent.name / name
            work.mkdir(parents=True, exist_ok=True)
            snapshot = source
            if kind is not None:
                snap = st.Snapshot(source)
                type_case(snap, data, kind, mode)
                if name == 'balance':
                    human = data['human']
                    snap.write(human + 0x38, 'i', 1)
                    snap.write(human + 0x60, 'f', 0.2)
                    snap.write(human + 0x8c, 'f', 0.7)
                    snap.write(human + 0xbc, 'I', 8)
                    snap.write(human + 0x2b20, 'f', 1.0)
                    snap.write(human + 0xc8 + 0x30, '9f', 1, 0, 0, 0, 0.8, 0.6, 0, -0.6, 0.8)
                if name == 'joint_mask':
                    snap.write(data['bond'] + 0x98, 'i', 2)
                if name in ('muscle_arm', 'muscle_dead'):
                    snap.write(data['bond'] + 0x98, 'i', 4)
                if name == 'muscle_dead':
                    snap.write(data['human'] + 0x28, 'i', 0)
                if name == 'spring_diagonal':
                    bone = data['human'] + 0x2b64 + 0xc8 + 0xe0
                    snap.write(bone, '3f', 0.43, 0.87, 0.11)
                    snap.write(bone + 0x18, '3f', -0.017, 0.023, -0.031)
                snapshot = work / 'sv_snapshot.bin'
                snap.save(snapshot)
            calls = work / f'{args.function}.txt'
            tested += 1
            calls.write_text(st.call(rva) * args.calls, encoding='utf-8', newline='\n')
            env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build/client_test'),
                       HARNESS_ENV_DIR=str(work / 'env'), SV_UI_WIDTH='220')
            env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
            if args.time:
                env['HARNESS_TIME'] = '1'
            result = st.run(snapshot, calls, args.function, env)
            print(f'{source.parent.name}/{name}: {result}', flush=True)
            match = re.search(r'(\d+) calls same, (\d+) different, (\d+) known differences, (\d+) lifted crashed', result)
            if args.time:
                failed += not result.startswith(f'{args.calls} calls; ratio ')
            else:
                failed += not (match and int(match[1]) == args.calls and all(int(match[i]) == 0 for i in (2, 3, 4)))
            if snapshot != source:
                snapshot.unlink()
    print(f'{args.function}: {tested} cases, {failed} failed', flush=True)
    raise SystemExit(bool(failed or not tested))


if __name__ == '__main__':
    main()
