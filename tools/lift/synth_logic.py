"""Test economy, menus, scenarios, bot controls and game reset.

Use --run to delete each snapshot after its test:
    python -I tools/lift/synth_logic.py --run <recording dir> <work dir> [case,...]
The work directory must be inside build/. Each case keeps its result and log.
"""
import argparse
import copy
import os
import re
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/lift'))
from synth_bonds import Snapshot

RVAS = {name: int(value, 16) for name, value in re.findall(
    r'^#define SR_RVA_(\w+)\s+(0x[0-9a-f]+)u',
    (ROOT / 'src/include/sr_globals_024d.h').read_text(encoding='utf-8'), re.M)}
PLAYER, PS = RVAS['PLAYER'], 0x2408
HUMAN, HS = RVAS['HUMAN'], 0x2b64
ITEM, IS = RVAS['ITEM'], 0x1ac
GAME, MARKET, CITY = RVAS['GAME'], RVAS['MARKET'], RVAS['CITY']
WORLD, SCENARIO, SERVER = RVAS['GAME_WORLD'], RVAS['SCENARIO'], RVAS['SERVER']
DEBUG = 0x204020
KNOWN = {'guards_no_spots', 'playerai_seat4'}
MENU_VALUES = [-1, *range(17), 32]


def put(s, address, data):
    for i, value in enumerate(data):
        s.write(address + i, 'B', value)


def clear_players(s):
    for i in range(256):
        s.write(PLAYER + i * PS, 'i', 0)


def clear_humans(s):
    for i in range(512):
        s.write(HUMAN + i * HS, 'i', 0)


def cases():
    out = {}

    def add(name, func, build):
        out[name] = func, build

    add('economy_snapshot', 'economysimulation', lambda s, d: ['R 53a0'])

    def economy(price, cash=300):
        def build(s, d):
            clear_players(s)
            s.write(PLAYER, 'i', 1)
            for off, value in ((0x3c, 1), (0x64, 0), (0x6c, 123), (0x78, -1), (0x70, 1)):
                s.write(PLAYER + off, 'i', value)
            s.write(GAME + 0x98, 'i', cash)
            s.write(MARKET + 4, 'f', price)
            return ['R 53a0']
        return build
    for price in (100.0, 499.99, 9.99):
        add('economy_%g' % price, 'economysimulation', economy(price))
    add('economy_floor', 'economysimulation', economy(9.99, cash=0))

    def driving(x):
        def build(s, d):
            clear_players(s)
            s.write(PLAYER, 'i', 1)
            s.write(PLAYER + 0x78, 'i', 0)
            s.write(PLAYER + 0x58, 'ii', 0, 0)
            s.write(GAME + 0x24, 'i', 100)
            s.write(GAME + 0x257f0, 'i', 3)
            s.write(GAME + 0x257f4, 'fff', 0, 0, 0)
            s.write(GAME + 0x25800, 'III', 0x7fc12345, 0, 0)
            s.write(GAME + 0x2580c, 'fff', 100, 0, 0)
            s.write(HUMAN + 0x48, 'fff', x, 0, 0)
            return ['R 1d3b0']
        return build
    for x in (3.0, 4.0):
        add('driving_%g' % x, 'logicsimulation_driving', driving(x))

    def clients(load):
        def build(s, d):
            files = d / 'files'
            files.mkdir(parents=True, exist_ok=True)
            if load:
                (files / 'client.srk').write_bytes(struct.pack('<10i', 1, 2, 11, 22, 33, 44, 55, 66, 77, 88))
            path = str(files).encode() + b'\0'
            put(s, 0x916d2e0, path)
            put(s, 0x916d4e0, path)
            s.write(0x1b497a0, 'i', 9 if load else 2)
            for i in range(8):
                s.write(0x1b497a4 + 4 * i, 'i', 11 * i + 3)
            return ['R 12f0' if load else 'R 11d0']
        return build
    add('client_save_two', 'saveaccountsclient', clients(False))
    add('client_load_two', 'loadaccountsclient', clients(True))

    def cash_nan(s, d):
        clear_players(s)
        s.write(PLAYER, 'i', 1)
        s.write(PLAYER + 0x64, 'i', 0)
        s.write(PLAYER + 0x6c, 'i', 0)
        s.write(MARKET, 'i', 0)
        return ['R 8cf0 V:0 V:64']
    add('cash_zero_shares', 'logic_addteamcash', cash_nan)

    def threats(levels, player=0):
        def build(s, d):
            clear_humans(s)
            for i in range(3):
                s.write(HUMAN + i * HS + 0x48, 'fff', i, 0, 0)
            p = PLAYER + player * PS
            s.write(p + 0x2168, 'ii', len(levels), 9)
            for i in range(2):
                t = p + 0x2170 + i * 0x18
                s.write(t, 'i', i + 1)
                s.write(t + 4, 'III', 0x7f812345, 0x3f800000, 0)
                s.write(t + 0x10, 'Ii', levels[i] if i < len(levels) else 0, 600)
            return ['R 3d500 V:%x V:0' % player]
        return build
    threat_cases = [('empty', []), ('low', [0x3f666666]), ('near', [0x3f6739d1]),
                    ('below', [0x3f6739cd]), ('rounded_edge', [0x3f6739ce]),
                    ('above', [0x3f6739cf]), ('qnan', [0x7fc12345]),
                    ('snan', [0x7f812345]), ('move_snan', [0x3f000000, 0x7f812345])]
    for name, levels in threat_cases:
        add('threat_' + name, 'ai_assessthreats', threats(levels))

    def all_threats(s, d):
        lines = []
        for player, (_, levels) in enumerate(threat_cases):
            lines.extend(threats(levels, player)(s, d))
        return lines
    add('threat_limits', 'ai_assessthreats', all_threats)

    def menu(menu_id, human, shop=4, first=0):
        def build(s, d):
            if first == 0:
                clear_players(s)
            s.write(GAME + 8, 'i', 3)
            s.write(GAME + 0x20, 'i', 1)
            s.write(CITY + 0x2f9dc, 'i', shop)
            s.write(CITY + 0x302a4, 'i', 2)
            s.write(CITY + 0x302a8, 'iiiiii', 0, 100, 0, 1, 100, 1)
            values = MENU_VALUES
            for i, value in enumerate(values, first):
                p = PLAYER + i * PS
                s.write(p, 'i', 1)
                s.write(p + 0x40, 'i', 100000)
                s.write(p + 0x4c, 'iii', 0, 0, 0)
                s.write(p + 0x64, 'i', 0)
                s.write(p + 0x6c, 'i', 100)
                s.write(p + 0x78, 'i', i if human else -1)
                s.write(p + 0xd4, 'i', menu_id)
                s.write(p + 0xd8, 'i', 0)
                s.write(p + 0xe4, 'ii', 1, 0)
                s.write(p + 0xec, 'iii', 0, menu_id, value)
                if human:
                    h = HUMAN + i * HS
                    if menu_id in (6, 7):
                        low = s.read(GAME + 0x58, 'fff')
                        high = s.read(GAME + 0x64, 'fff')
                        pos = tuple((a + b) * 0.5 for a, b in zip(low, high))
                    else:
                        pos = (10000, 10000, 10000)
                    s.write(h + 0x48, 'fff', *pos)
                    for slot in range(8):
                        s.write(h + 0x291c + slot * 0x24, 'i', 0)
            return ['R 55700 V:%x' % i for i in range(first, first + len(values))]
        return build
    for menu_id in range(13):
        for human in (False, True):
            add('menu_%02d_%s' % (menu_id, 'human' if human else 'none'),
                'logic_playeractions', menu(menu_id, human))
    def all_menus(human):
        def build(s, d):
            lines = []
            for menu_id in range(13):
                lines.extend(menu(menu_id, human, first=menu_id * len(MENU_VALUES))(s, d))
            return lines
        return build
    add('menus_none', 'logic_playeractions', all_menus(False))
    add('menus_human', 'logic_playeractions', all_menus(True))
    for shop in (2, 7):
        add('shop_%d' % shop, 'logic_playeractions', menu(10, True, shop))

    def reopen_ring(s, d):
        menu(10, True)(s, d)
        s.write(PLAYER + 0xe4, 'ii', 1, 62)
        for slot, menu_id, value in ((62, 10, 1), (63, 9, 1), (0, 10, 0)):
            s.write(PLAYER + 0xec + slot * 0x54, 'iii', 0, menu_id, value)
        return ['R 55700 V:0']
    add('menu_reopen_ring', 'logic_playeractions', reopen_ring)

    def shop_priority(types, old_menu=0, nan=False, outside=False):
        def build(s, d):
            s.write(PLAYER + 0x78, 'i', 0)
            s.write(PLAYER + 0x64, 'i', 7)
            s.write(PLAYER + 0xd4, 'i', old_menu)
            s.write(HUMAN + 0x48, 'fff', *((10000, 10000, 10000) if outside else (0, 0, 0)))
            s.write(CITY + 0x2f998, 'i', len(types))
            for i, typ in enumerate(types):
                b = CITY + i * 0x9dc
                s.write(b + 0x2f9dc, 'i', typ)
                s.write(b + 0x2f9f8, 'fff', -1, -1, -1)
                s.write(b + 0x2fa04, 'fff', 1, 1, 1)
            if nan:
                s.write(CITY + 0x2f9f8, 'I', 0x7fc12345)
            return ['R 94a0 V:0']
        return build
    add('bank_before_shop', 'logic_player', shop_priority([5, 4, 0]))
    add('shop_before_bank', 'logic_player', shop_priority([4, 5, 0]))
    add('bank_only', 'logic_player', shop_priority([5, 0, 0]))
    add('shop_keep_menu', 'logic_player', shop_priority([5, 4, 0], old_menu=8))
    add('shop_nan_bank', 'logic_player', shop_priority([5, 4, 0], nan=True))
    add('shop_no_match', 'logic_player', shop_priority([5, 4, 0], outside=True))

    def reset(mode, debug):
        def build(s, d):
            s.write(SERVER, 'i', 1)
            s.write(GAME + 8, 'i', mode)
            s.write(DEBUG, 'i', debug)
            return ['R 590f0']
        return build
    for mode in range(5):
        for debug in (0, 1):
            add('reset_%d_debug%d' % (mode, debug), 'resetgame', reset(mode, debug))

    def reset_client(s, d):
        lines = reset(3, 1)(s, d)
        s.write(SERVER, 'i', 0)
        s.write(0x1954d700 + 4, 'i', 123)
        return lines
    add('reset_client', 'resetgame', reset_client)

    def setup_names(s, d):
        for i in (0, 23, 25, 30, 35, 39, 42, 43):
            put(s, 0x16d50024 + i * 0x58 + 0x14, b'A' * 68)
        return ['R 502a0']
    add('setup_dirty_names', 'setupgame', setup_names)

    def world_start(s, d):
        s.write(GAME + 0x20, 'i', 1)
        s.write(DEBUG, 'i', 1)
        return ['R 5b9b0']
    add('world_start_debug', 'logicsimulation_world', world_start)

    def scenarios(n):
        def build(s, d):
            for team in range(5):
                s.write(GAME + 0x90 + team * 0x117c, 'i', 2 if team < n else 0)
                for slot in range(16):
                    s.write(GAME + 0xd0 + team * 0x117c + slot * 0x40, 'i', 0)
            s.write(WORLD, 'i', 1800000)
            return ['R 5aa60']
        return build
    for n in range(1, 6):
        add('scenario_teams%d' % n, 'createscenario_world', scenarios(n))

    def acquisition(buildings):
        def build(s, d):
            s.write(CITY + 0x2f998, 'i', buildings)
            for i in range(buildings):
                s.write(CITY + 0x2f9dc + i * 0x9dc, 'i', 0)
            s.write(SCENARIO + 0xc, 'i', 0)
            return ['R 58ef0 V:0 V:1 M:%x' % (SCENARIO + 0xc)]
        return build
    add('acquisition_no_buildings', 'createmission_acquisition', acquisition(0))
    add('acquisition_no_type3', 'createmission_acquisition', acquisition(2))

    def guards(s, d):
        s.write(CITY + 0x2fba0, 'i', 0)
        return ['R 58320 V:0 V:16 V:1']
    add('guards_no_spots', 'scenario_createguards', guards)

    def seat4(s, d):
        clear_humans(s)
        s.write(PLAYER + 0x78, 'i', 0)
        s.write(PLAYER + 0x215c, 'i', 0)
        s.write(PLAYER + 0x2168, 'i', 0)
        s.write(PLAYER + 0x2170, 'i', 0)
        s.write(HUMAN + 0x48, 'fff', 0, 0, 0)
        s.write(HUMAN + 0x14, 'ii', 0, 4)
        for slot in range(8):
            s.write(HUMAN + 0x291c + slot * 0x24, 'i', 0)
        return ['R 4f720 V:0']
    add('playerai_seat4', 'playerai', seat4)

    def leavers(two):
        def build(s, d):
            clear_players(s)
            s.write(GAME + 0x20, 'i', 2)
            s.write(GAME + 0x25d60, 'i', 8)
            s.write(WORLD, 'i', 215990 if not two else 216110)
            for i in range(2 if two else 1):
                p = PLAYER + i * PS
                s.write(p, 'i', 1)
                for off, val in ((0x64, 0), (0x6c, 0), (0x78, i), (0x9c, 0x40000),
                                 (0xbc, 179), (0x1a38, 0)):
                    s.write(p + off, 'i', val)
                s.write(HUMAN + i * HS + 0x48, 'fff', 10000, 10000, 10000)
            s.write(MARKET, 'if', 100, 123.5)
            s.write(GAME + 0x9c, 'i', -1)
            return ['R 5b9b0']
        return build
    add('world_leaver_cap', 'logicsimulation_world', leavers(False))
    add('world_two_leavers', 'logicsimulation_world', leavers(True))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run', action='store_true')
    ap.add_argument('recording', type=Path)
    ap.add_argument('work', type=Path)
    ap.add_argument('cases', nargs='?')
    args = ap.parse_args()
    work = args.work.resolve()
    if not work.is_relative_to((ROOT / 'build').resolve()):
        ap.error('The work directory must be inside build/.')
    protected = [ROOT / 'build/recordings', ROOT / 'build/gate', args.recording.resolve()]
    if any(work.is_relative_to(path.resolve()) for path in protected):
        ap.error('The work directory must be outside the recordings and gate directories.')
    available = cases()
    selected = set(args.cases.split(',')) if args.cases else {
        name for name in available if not re.match(r'(?:menu_\d\d_|threat_(?!limits))', name)}
    if selected - available.keys():
        ap.error('Unknown case: ' + ','.join(sorted(selected - available.keys())))
    if not args.run and len(selected) != 1:
        ap.error('Use --run for more than one case.')
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    failed = 0
    for name, (func, build) in available.items():
        if name not in selected:
            continue
        d = work / name
        d.mkdir(parents=True, exist_ok=True)
        started = time.perf_counter()
        snap = copy.deepcopy(base)
        lines = build(snap, d)
        snapshot = d / 'sv_snapshot.bin'
        if snapshot.exists():
            raise RuntimeError('The case snapshot already exists: ' + str(snapshot))
        try:
            snap.save(snapshot)
        except BaseException:
            snapshot.unlink(missing_ok=True)
            raise
        (d / 'sv_args.txt').write_text('\n'.join(lines) + '\n', encoding='ascii')
        if not args.run:
            print(name, func, len(lines), 'calls', flush=True)
            continue
        env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'), SV_LOG_FILE=str(d / 'verify.log'))
        if name in KNOWN:
            env['HARNESS_DEBUG'] = '1'
        try:
            harness_started = time.perf_counter()
            p = subprocess.run([str(ROOT / 'build/hook/harness.exe'), str(ROOT / 'build/hook/024d_fixed.exe'),
                                str(snapshot), str(d / 'sv_args.txt'), func], cwd=ROOT, env=env,
                               capture_output=True, text=True, errors='replace')
            (d / 'harness.txt').write_text(p.stdout + p.stderr, encoding='utf-8')
            summaries = [line for line in p.stdout.splitlines() if 'functions tested' in line]
            summary = summaries[-1] if summaries else 'NO RESULT'
            match = re.search(r'(\d+) calls same, (\d+) different, \d+ known differences, (\d+) \w+ crashed, (\d+) skipped', summary)
            ok = bool(match and (p.returncode == 0 or (name in KNOWN and p.returncode == 3)))
            if match:
                same, diff, crashed, skipped = map(int, match.groups())
                ok = ok and same + diff == len(lines) and crashed == skipped == 0
                ok = ok and (diff == 0 or name in KNOWN)
            failed += not ok
            note = ' EXPECTED CASE' if name in KNOWN else ''
            elapsed = ' %.1fs total, %.1fs harness' % (time.perf_counter() - started, time.perf_counter() - harness_started)
            print(name, func, summary, note, '' if ok else 'CHECK', elapsed, flush=True)
            if not ok:
                print('stopped at:', name, flush=True)
                return 1
        finally:
            snapshot.unlink(missing_ok=True)
    if args.run:
        print('cases failed:', failed)
    return bool(failed)


if __name__ == '__main__':
    sys.exit(main())
