"""Compare game helpers on empty, sparse, full and last-slot pools."""
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

ROWS = re.compile(r'^\[\s*\d+/\d+\]\s+(\w+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed', re.M)


def main():
    recording, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    storage = {r['name']: int(r['client_rva'], 16) for r in csv.DictReader(
        (ROOT / 'analysis/client_win/storage.csv').open(encoding='utf-8'))}
    fields = {(r['struct'], r['name']): int(r['offset'], 16) for r in csv.DictReader(
        (ROOT / 'analysis/structs/names.csv').open(encoding='utf-8'))}
    addresses = pure.rvas()
    failures = []
    for mode in ('empty', 'sparse', 'full', 'last'):
        case = work / mode
        case.mkdir(exist_ok=True)
        snapshot = st.Snapshot(recording / 'sv_snapshot.bin')
        calls, counts = [], Counter()

        def write(address, fmt, *values):
            snapshot.write(address, fmt, *values)

        def call(name, *values):
            calls.append(st.call(addresses[name], *values))
            counts[name] += 1

        def player(index, name, fmt, value):
            write(storage['player'] + index * 0x2408 + fields['player', name], fmt, value)

        pools = [('cityobject', 1024, 0x54, 'getnextcityobject'),
                 ('human', 512, 0x2b64, 'getnexthuman'),
                 ('item', 1024, 0x1ac, 'getnextitem'),
                 ('object', 1024, 0x8230, 'getnextobject'),
                 ('player', 256, 0x2408, 'getnextplayer')]
        for name, size, stride, function in pools:
            for index in range(size):
                active = mode in ('full', 'last') or mode == 'sparse' and index < 7
                if mode == 'last' and index == size - 1:
                    active = False
                write(storage[name] + index * stride, 'i', int(active))
            call(function)
        for index in (-1, 0, 1023, 1024):
            call('deletecityobject', index)
        write(storage['numofbullets'], 'i', 0 if mode == 'empty' else 3)
        for index in range(3):
            raw = bytes((position + index * 13) & 255 for position in range(0x5c))
            write(storage['bullet'] + index * 0x5c, '92s', raw)
        for index in (-1, 0, 1, 2, 3):
            call('deletebullet', index)

        for index in range(256):
            player(index, 'team', 'i', index % 3)
            player(index, 'money', 'i', 700)
            player(index, 'stocks', 'i', 2 if index == 0 else 0)
        for index in (0, 1):
            player(index, 'num_actions', 'i', 63 if mode == 'last' else 0)
            player(index, 'num_path', 'i', 63 if mode == 'last' else 0)
            raw = b'Z' * (64 * 0x54)
            player(index, 'actions', f'{len(raw)}s', raw)
        write(0x52daba4, 'i', ('empty', 'sparse', 'full', 'last').index(mode))
        call('player_addaction_item', 0, -2147483648, 2147483647)
        for length in (0, 1, 63, 64, 65, 127):
            call('player_addaction_chat', 0, 'P:0:' + (b'x' * length + b'\0').hex())
        call('addplayerwaypoint', 0, 1.25, -0.0, -7.5)

        account_store = storage['clientaccounts']
        write(account_store + 4, '1048576s', bytes(1048576))
        write(account_store, 'i', 65535 if mode == 'full' else 1 if mode != 'empty' else 0)
        write(account_store + 4, '4i', 111, 0x12345678, 0x76543210, 0x55aa)
        output = 'P:0:' + struct.pack('<I', 0xa5a5a5a5).hex()
        call('createclientaccount', output, output, 111)
        call('createclientaccount', output, output, 222)

        balances = (0, 9999, 10000, 10001, 20000, 2147483647, -2147483648)
        write(storage['accounts'] + 4, 'i', len(balances))
        for index, balance in enumerate(balances):
            write(storage['accounts'] + 8 + index * 0x4c + 0x2c, 'i', balance)
        for index in (-1, *range(len(balances)), len(balances)):
            call('accountdeathtax', index)
        for index in (-1, 0, 255, 256):
            call('playerdebt', index)
        for team in range(3):
            write(storage['game'] + 0x94 + team * 0x117c, 'i', 700)
            for amount in (0, 1, -1, 2147483647, -2147483648):
                call('addpointstoteam', team, amount)
                call('logic_splitteamcash', team, amount)

        write(storage['timerlist'], '84s', b'Z' * 84)
        call('setuptimers')
        write(0x75a5800, 'i', 1)
        write(0x9065334, 'i', 0x12345678)
        write(storage['numofevents'], 'I', 65535 if mode == 'last' else 7)
        write(storage['human'] + fields['human', 'player_id'], 'i', 31)
        call('createevent_updatehuman', 0)
        path = case / 'calls.txt'
        path.write_text(''.join(calls), encoding='utf-8', newline='\n')
        snapshot_path = case / 'snapshot.bin'
        snapshot.save(snapshot_path)
        env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build/client_test'),
                   HARNESS_ENV_DIR=str(case / 'env'), SV_LOG_FILE=str(case / 'calls.log'), SV_UI_WIDTH='220')
        env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
        try:
            print(f'{mode}: ' + st.run(snapshot_path, path, ','.join(counts), env), flush=True)
        finally:
            snapshot_path.unlink()
        result = path.with_suffix('.result.txt').read_text(encoding='utf-8')
        rows = {name: tuple(map(int, values)) for name, *values in ROWS.findall(result)}
        for name, count in counts.items():
            print(f'{mode} {name}: {rows.get(name)}', flush=True)
            if rows.get(name) != (count, 0, 0):
                failures.append((mode, name))
    if failures:
        print(f'Failed cases: {failures}', flush=True)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
