"""Compare keyboard, mouse, item, player and camera controls on constructed input states."""
import argparse
import atexit
import copy
import itertools
import os
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lift'))
from synth_bonds import PAGE, Snapshot

ROOT = Path(__file__).resolve().parents[2]
IMAGE_BASE = 0x20000000
KEYS, PREVIOUS_KEYS = 0x75a5140, 0x1bbe1a0
MOUSE, PREVIOUS_MOUSE = 0x19c07c80, 0xb733100
PLAYER, PLAYER_BYTES = 0xed813a0, 0x2408
HUMAN, HUMAN_BYTES = 0x14fc4a0, 0x2b64
ITEM, ITEM_BYTES = 0x908b080, 0x1ac
CAMERA = 0xf3eafe0
CAMERA_COUNT, CAMERA_BYTES = 16, 0x1c
FUNCTIONS = {
    'checkkeyboard': (0xfec50, ()), 'checkmouse': (0xfeca0, ()),
    'checkmouserelative': (0xfedb0, ()), 'debug_getinputs': (0x69190, ()),
    'getinputs': (0x90bf0, (0,)), 'getinputs_human': (0x91920, (0,)),
    'getinputs_car': (0x924f0, (0,)), 'getinputs_heli': (0x93130, (0,)),
    'getinputs_phone': (0x93a00, (0, 0)), 'getinputs_money': (0x93d30, (0, 0)),
    'getinputs_door': (0x93dd0, (0,)), 'movecamera': (0x7d0e0, (0,)),
    'movecamera2': (0x7d4c0, (1,)),
}
RESULT = re.compile(r'^\[\s*\d+/\d+\]\s+(\w+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed', re.M)
TIMING = re.compile(r'^time (\w+) (\d+) (\d+) (\d+)', re.M)
CONTROL_KEYS = [4, 6, 7, 8, 9, 20, 21, 22, 23, 24, 26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
                40, 41, 43, 44, 53, 76, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 224, 225]
DEBUG_KEYS = [5, 12, 18, 19, 24, 35, 36, 37, 38, 39, 59, 63, 67, 68, 73]


def call(name, values=None):
    rva, defaults = FUNCTIONS[name]
    args = defaults if values is None else values
    return f'R {rva:x}' + ''.join(f' V:{value & 0xffffffff:x}' for value in args) + '\nZ r:0\n'


def initial_edits():
    edits = [(KEYS, '512s', bytes(512)), (PREVIOUS_KEYS, '512s', bytes(512)),
             (MOUSE, '11i', *([0] * 11)), (PREVIOUS_MOUSE, '11i', *([0] * 11)),
             (0x29e752a0, 'i', 0), (0xb77c594, '3i', 0, 0, 0),
             (0x90631ec, 'i', 0), (0xb7330c0, 'ii', 1600, 900),
             (0x1dda5248, 'ff', 0.0, 0.0), (0x75a5800, 'i', 0),
             (0x127ea8, 'i', 0), (0x52daba0, 'i', 0), (0x5350f20, 'i', 0),
             (0x9065304, 'ii', 0, 0), (0x9065318, 'i', 0), (0x1b9b4e0, 'i', 0),
             (0x9065320, 'i', 0), (0x908aadc, 'i', 1),
             (0x52dabb0, '64s', bytes(64))]
    for offset, fmt, values in [
        (0x78, 'i', (0,)), (0x7c, '7f', (0.2, 0.1, -0.3, 0.25, 0.3, -0.2, 0.125)),
        (0x9c, 'I', (0,)), (0xac, 'iff', (1, 0.125, 0.25)),
        (0xc0, '9i', (0, 1, 1, 0, 0, 0, 0, 0, 0)), (0xe4, 'ii', (0, 0)),
    ]:
        edits.append((PLAYER + offset, fmt, *values))
    edits += [(HUMAN, 'i', 1), (HUMAN + 0x14, 'ii', 0, 0), (HUMAN + 0x28, 'i', 100),
              (HUMAN + 0x60, 'ff', 0.3, -0.2), (HUMAN + 0xc8, 'fff', 0.0, 0.0, 0.0),
              (HUMAN + 0x288, 'fff', 0.125, 1.0, 0.25),
              (HUMAN + 0x2b8, '9f', 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0),
              (HUMAN + 0x291c, 'i', 1), (HUMAN + 0x2920, 'i', 0),
              (HUMAN + 0x2940, 'i', 1), (HUMAN + 0x2944, 'i', 0),
              (HUMAN + 0x2a3c, 'fff', 0.5, 0.0, 0.0), (HUMAN + 0x2b2c, 'i', 0),
              (ITEM, 'i', 1), (ITEM + 0xc, 'i', 0), (ITEM + 0x48, 'fff', 0.5, 0.0, 0.0),
              (ITEM + 0x84, '9f', 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0),
              (ITEM + 0x104, 'i', 1), (ITEM + 0x110, 'i', 1), (ITEM + 0x134, 'i', 3),
              (0x5519124, 'i', 0), (0xabdbf20, 'i', 1)]
    for index in range(2):
        edits.append((CAMERA + index * 0x1c, '7f', 12.25, -0.5, -3.75, 0.3, -0.2, 0.25, 0.5))
    return edits


def cases():
    all_functions = list(FUNCTIONS)
    yield 'idle', [], all_functions, {}
    for key in sorted(set(CONTROL_KEYS + DEBUG_KEYS + [229])):
        for state in ('press', 'hold', 'release'):
            edits = [(KEYS + key, 'B', int(state != 'release')),
                     (PREVIOUS_KEYS + key, 'B', int(state != 'press'))]
            yield f'key_{key}_{state}', edits, all_functions, {}
    pairs = [(4, 7), (26, 22), (20, 29), (224, 225), (225, 26), (224, 26),
             (30, 89), (39, 98), (35, 36), (73, 63), (40, 41), (44, 225)]
    paired_keys = sorted(set(CONTROL_KEYS + DEBUG_KEYS + [229]))
    pairs += list(zip(paired_keys, paired_keys[1:] + paired_keys[:1]))
    for first, second in sorted(set(pairs)):
        yield f'keys_{first}_{second}', [(KEYS + first, 'B', 1), (KEYS + second, 'B', 1)], all_functions, {}
    for key in CONTROL_KEYS + [229]:
        yield f'polled_key_{key}', [], ['checkkeyboard', 'getinputs'], {'keys': [key]}
    for left, right in itertools.product((0, 1), repeat=2):
        for movement in (0, 1, -1, 4096, -4096):
            edits = [(MOUSE, '6i', movement, -movement, movement, -movement, left, right),
                     (MOUSE + 0x28, 'I', left | right << 2),
                     (PREVIOUS_MOUSE + 0x10, 'ii', 1 - left, 1 - right)]
            yield f'mouse_{left}_{right}_{movement}', edits, all_functions, {}
    for up, down in ((1, 0), (0, 1), (3, 2), (-1, -2)):
        yield f'wheel_{up}_{down}', [(MOUSE + 0x20, 'ii', up, down)], all_functions, {}
    for field, address, values in (
        ('blocked', 0x29e752a0, (1,)), ('cursor', 0xb77c594, (1,)),
        ('no_human', PLAYER + 0x78, (-1,)), ('no_vehicle', HUMAN + 0x14, (-1,)),
        ('hand_count', HUMAN + 0x291c, (0,)), ('pitch_inverted', 0x90631ec, (1,)),
        ('mode', PLAYER + 0xcc, (0, 2, 3)), ('server', 0x75a5800, (1,)),
        ('display_mode', 0x14a8684, (5,)), ('menu', PLAYER + 0xd4, (1, 2)),
        ('pause', 0x9065304, (1, 2)), ('connections', 0x9065318, (1,)),
    ):
        for value in values:
            edits = [(address, 'i', value)]
            if field == 'server':
                edits += [(KEYS + key, 'B', 1) for key in (12, 18, 19)]
            if field == 'display_mode':
                edits += [(KEYS + 68, 'B', 1)]
            yield f'{field}_{value}', edits, all_functions, {}
    for kind in (16, 23, 25):
        edits = [(ITEM + 0xc, 'i', kind), (PLAYER + 0xd0, 'i', -1),
                 (KEYS + 30, 'B', 1), (MOUSE + 0x20, 'ii', 2, 1)]
        yield f'held_item_{kind}', edits, ['getinputs_human', 'getinputs_car', 'getinputs_door'], {}
    for value in (-10.0, -0.125, 0.0, 0.125, 10.0, float('nan')):
        edits = [(PLAYER + 0x88, 'f', value), (PLAYER + 0x80, 'f', value),
                 (PLAYER + 0x7c, 'f', value), (PLAYER + 0x84, 'f', value),
                 (PLAYER + 0x8c, 'f', value), (PLAYER + 0x90, 'f', value),
                 (0x1dda5248, 'ff', value, value)]
        yield f'axes_{value}', edits, ['getinputs_car', 'getinputs_human', 'getinputs_heli'], {}
    for function, mode in (('getinputs_human', 1), ('getinputs_car', 2), ('getinputs_heli', 3)):
        for value in (-10.0, -1.0, -0.375, -0.125, -0.0, 0.125, 0.375, 0.5, 1.5, 10.0, float('nan')):
            edits = [(PLAYER + 0xcc, 'i', mode), (PLAYER + 0x7c, '7f', *([value] * 7)),
                     (0x1dda5248, 'ff', value, value)]
            yield f'{function}_axis_{value}', edits, [function], {}
    for value in (-2.0, 0.0, 0.875, 32.0, 64.0, float('nan')):
        edits = [(CAMERA + 0x18, 'f', value), (CAMERA + 0x1c + 0x18, 'f', value),
                 (PLAYER + 0xb0, 'f', value), (PLAYER + 0xb4, 'f', value)]
        yield f'zoom_{value}', edits, ['movecamera', 'movecamera2', 'getinputs_human', 'getinputs_car', 'getinputs_heli'], {}
    for function, mode in (('getinputs_human', 1), ('getinputs_car', 2), ('getinputs_heli', 3)):
        for right in (0, 1):
            yield f'{function}_signaling_nan_{right}', [
                (PLAYER + 0xcc, 'i', mode), (PLAYER + 0x7c, '8I',
                    0x7f800001, 0xff800001, 0x7f800001, 0xff800001,
                    0x7f800001, 0xff800001, 0x7f800001, 0xff800001),
                (MOUSE + 0x14, 'i', right), (MOUSE + 0x28, 'I', right << 2),
                (0x1dda5248, 'II', 0x7f800001, 0xff800001)], [function], {}
    for state, (target, zoom) in enumerate(((0.1234567, 0.2531488), (0.31415927, 0.27182818),
                                           (0.12345678, 0.8765432), (0.33333334, 0.6666667))):
        for right in (0, 1):
            yield f'fractional_zoom_{state}_{right}', [
                (PLAYER + 0xb0, 'ff', target, zoom), (PLAYER + 0xcc, 'i', 2),
                (MOUSE + 8, 'ii', 359, -719), (MOUSE + 0x14, 'i', right)], [
                    'getinputs_human', 'getinputs_car', 'getinputs_heli'], {}
    for key in (4, 7, 22, 26):
        for right in (0, 1):
            edits = [(PLAYER + 0xcc, 'i', 2), (PLAYER + 0x7c, '4f', *([float('nan')] * 4)),
                     (KEYS + key, 'B', 1), (MOUSE + 0x14, 'i', right)]
            yield f'car_nan_{key}_{right}', edits, ['getinputs_car'], {}
    for seat, vehicle_state in ((0, 1), (1, 2), (0, 2)):
        yield f'vehicle_{seat}_{vehicle_state}', [(HUMAN + 0x18, 'i', seat),
                                                (0xabdbf20, 'i', vehicle_state)], ['getinputs'], {}
    yield 'absent_player', [], ['getinputs'], {'getinputs': (-1,)}
    yield 'door_near', [(HUMAN + 0xc8, 'fff', 1.0, 0.0, 0.0)], ['getinputs_door'], {}
    yield 'door_empty', [(HUMAN + 0x291c, 'i', 0)], ['getinputs_door'], {}
    yield 'mouse_noncanonical_release', [(MOUSE + 0x10, 'ii', 2, -1)], ['checkmouse', 'checkmouserelative'], {}
    for width, height in ((1024, 640), (1, 1), (0, 0)):
        yield f'screen_{width}_{height}', [(0xb7330c0, 'ii', width, height),
                                          (MOUSE, 'ii', 2147483647, -2147483648)], ['checkmouse'], {}
    for function in ('checkkeyboard', 'getinputs'):
        yield f'{function}_shift_alias', [(KEYS + 225, 'B', 0)], [function], {'keys': [229]}
    yield 'player_menu_action', [(PLAYER + 0xe0, 'i', 3)], ['getinputs'], {}
    yield 'capture_transition', [(0xb77c598, 'i', 1)], ['getinputs'], {}
    yield 'chat_send', [(0x52daba0, 'i', 1), (0x52dabb0, '6s', b'hello\0')], ['getinputs'], {}
    for game_type, menu, previous_menu in ((4, 1, 0), (4, 0, 1), (3, 1, 2)):
        yield f'player_menu_{game_type}_{menu}_{previous_menu}', [
            (0x9065308, 'i', game_type), (PLAYER + 0xd4, 'i', menu),
            (PLAYER + 0xdc, 'i', previous_menu), (PLAYER + 0x78, 'i', -1)], ['getinputs'], {}
    yield 'connection_menu', [(0x9065304, 'i', 1), (0x9065318, 'i', 1),
                              (0x1b9b4e0, 'i', 1), (0x16338610, 'i', -1)], ['getinputs'], {}
    for team in (-1, 0, 2, 3):
        yield f'spectator_team_{team}', [(0x9065308, 'i', 3), (0x908aadc, 'i', 0),
                                        (0x908aad8, 'i', -1), (HUMAN + 0x2b2c, 'i', team),
                                        (PLAYER + 0x78, 'i', -1)], ['getinputs'], {'keys': [44]}
    for health in (0, -1):
        yield f'body_health_{health}', [(HUMAN + 0x28, 'i', health)], ['getinputs'], {}
    yield 'blocked_fire', [(0xb77c59c, 'i', 1), (MOUSE + 0x10, 'i', 1), (MOUSE + 0x28, 'I', 1)], [
        'getinputs_human', 'getinputs_car', 'getinputs_heli'], {}
    yield 'phone_inactive', [(ITEM + 0xc, 'i', 23), (ITEM + 0x110, 'i', 0),
                             (KEYS + 30, 'B', 1)], ['getinputs_human', 'getinputs_car'], {}
    yield 'phone_info_independent', [(ITEM + 0xc, 'i', 23), (ITEM + 0x104, 'i', 0),
                                     (KEYS + 30, 'B', 1)], ['getinputs_human', 'getinputs_car'], {}
    yield 'phone_all_digits', [(ITEM + 0xc, 'i', 23)] + [
        (KEYS + key, 'B', 1) for key in list(range(89, 99)) + list(range(30, 40))], [
            'getinputs_phone', 'getinputs_human', 'getinputs_car'], {}
    yield 'debug_blocked_keys', [(0x29e752a0, 'i', 1)] + [(KEYS + key, 'B', 1) for key in DEBUG_KEYS], [
        'debug_getinputs'], {}
    yield 'camera_nan_angles', [(CAMERA + offset, 'ff', float('nan'), float('nan')) for offset in (0xc, 0x28)] + [
        (KEYS + 26, 'B', 1)], ['movecamera', 'movecamera2'], {}
    yield 'door_nan_angle', [(PLAYER + 0x7c, 'f', float('nan'))], ['getinputs_door'], {}
    yield 'keyboard_copy_limit', [(KEYS + 322, 'B', 2), (KEYS + 323, 'B', 5), (KEYS + 511, 'B', 7),
                                   (PREVIOUS_KEYS + 323, 'B', 3), (PREVIOUS_KEYS + 511, 'B', 9)], [
        'checkkeyboard'], {'keys': [322, 323, 511]}
    yield 'mouse_button_mask', [(MOUSE + 0x28, 'I', 0x12345)], ['checkmouse', 'checkmouserelative'], {}
    for name, mode, vehicle, vehicle_state in (('human_active', 1, -1, 1),
                                               ('car_active', 2, 0, 1), ('heli_active', 3, 0, 2)):
        function = ('getinputs_human', 'getinputs_car', 'getinputs_heli')[mode - 1]
        yield name, [(PLAYER + 0xcc, 'i', mode), (HUMAN + 0x14, 'i', vehicle),
                     (0xabdbf20, 'i', vehicle_state)], [function, 'getinputs'], {}
    yield 'camera_last_slot', [(CAMERA + (CAMERA_COUNT - 1) * CAMERA_BYTES, '7f',
                                1.25, -2.5, 3.75, 0.3, -0.2, 0.25, 0.5),
                               (KEYS + 26, 'B', 1), (KEYS + 20, 'B', 1)], ['movecamera', 'movecamera2'], {
                                   'movecamera': (CAMERA_COUNT - 1,), 'movecamera2': (CAMERA_COUNT - 1,)}
    for buttons in (0, 1):
        edits = [(0x9065304, 'i', 1), (0x9065318, 'i', 1), (0x1b9b4e0, 'i', 1),
                 (0x75a5800, 'i', 1), (0x16338600, 'I', 0x0100007f), (0x16338610, 'i', -1),
                 (0x103675a4, 'i', 0), (MOUSE, 'ii', 50, 200), (MOUSE + 0x10, 'i', 1 - buttons),
                 (MOUSE + 0x28, 'I', buttons)]
        yield f'connection_admin_{buttons}', edits, ['getinputs'], {}
    for name, game_type, state, menu, cursor in (('round_end_menu', 3, 3, 1, 1),
                                                ('round_start_no_body', 0, 1, 0, 0),
                                                ('world_no_body_start', 4, 1, 0, 0)):
        yield name, [(0x9065308, 'i', game_type), (0x9065320, 'i', state),
                     (PLAYER + 0xd4, 'i', menu), (PLAYER + 0x78, 'i', -1),
                     (0xb77c594, 'i', cursor)], ['getinputs'], {}
    yield 'human_copy_signaling_nan', [(HUMAN + 0x60, 'II', 0x7f800001, 0xff800001),
                                       (0xb77c594, 'i', 1)], ['getinputs_human'], {}
    yield 'debug_copy_signaling_nan', [(HUMAN + 0x288, 'III', 0x7f800001, 0xff800001, 0x80000000),
                                       (KEYS + 73, 'B', 1), (PLAYER + 0x40, 'i', 0)], ['debug_getinputs'], {}


def changed_snapshot(base, edits):
    snapshot = copy.copy(base)
    snapshot.pages = base.pages.copy()
    changed = set()
    for address, fmt, *values in edits:
        for byte in range(struct.calcsize('<' + fmt)):
            page = (address + byte - base.data_rva) // PAGE
            if page not in changed and page in snapshot.pages:
                snapshot.pages[page] = bytearray(snapshot.pages[page])
                changed.add(page)
        snapshot.write(address, fmt, *values)
    return snapshot


class SnapshotFile:
    """Write changed pages into a reusable snapshot file."""

    def __init__(self, path):
        self.path = path
        self.indices = ()
        self.offsets = {}
        self.changed = set()

    def save(self, snapshot, base):
        indices = tuple(sorted(snapshot.pages))
        changed = {index for index, page in snapshot.pages.items() if page is not base.pages.get(index)}
        if indices != self.indices or not self.path.exists():
            snapshot.save(self.path)
            self.indices = indices
            self.offsets = {index: len(snapshot.head) + order * (PAGE + 4) + 4
                            for order, index in enumerate(indices)}
        else:
            with self.path.open('r+b') as output:
                for index in self.changed | changed:
                    output.seek(self.offsets[index])
                    output.write(snapshot.pages[index])
        self.changed = changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('--funcs', help='Function names; the input manifest supplies the default')
    parser.add_argument('--cases', help='Comma-separated case names')
    parser.add_argument('--bench', action='store_true')
    parser.add_argument('--repeat', type=int, default=1, help='Calls of each function per case')
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error('--repeat must be positive')
    if args.funcs:
        selected = set(args.funcs.split(','))
    else:
        selected = {line.split()[0] for line in (ROOT / 'analysis/client_win/adopted_C4.txt').read_text().splitlines()
                    if line.strip() and not line.startswith('#')}
    wanted = set(args.cases.split(',')) if args.cases else None
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    if struct.unpack_from('<I', base.head, 4)[0] != IMAGE_BASE:
        parser.error('The snapshot must use image base 0x20000000')
    args.work.mkdir(parents=True, exist_ok=True)
    path = args.work / 'sv_input_snapshot.bin'
    output = SnapshotFile(path)
    atexit.register(path.unlink, missing_ok=True)
    totals, failures, timings = Counter(), [], Counter()
    for name, edits, functions, overrides in cases():
        functions = [function for function in functions if function in selected]
        if not functions or wanted is not None and name not in wanted:
            continue
        folder = args.work / name
        folder.mkdir(exist_ok=True)
        snapshot = changed_snapshot(base, initial_edits() + edits)
        keys = overrides.get('keys', [key for key, state in enumerate(snapshot.read(KEYS, '512B')) if state])
        mouse = snapshot.read(MOUSE, '11i')
        output.save(snapshot, base)
        del snapshot
        arguments = folder / 'sv_args.txt'
        arguments.write_text(''.join(call(function, overrides.get(function)) * args.repeat
                                     for function in functions), encoding='utf-8')
        env = dict(os.environ, HARNESS_RESET='1', SV_UI_WIDTH='220',
                   HARNESS_ENV_DIR=str(folder.resolve() / 'env'),
                   SV_LOG_FILE=str(folder.resolve() / 'verify.log'),
                   HARNESS_GAME_DIR=str(ROOT / 'build/client_test'),
                   PATH=str(ROOT / 'build/client_deps') + os.pathsep + os.environ['PATH'],
                   TEMP='E:/tmp', TMP='E:/tmp',
                   SV_INPUT_KEYS=','.join(map(str, keys)),
                   SV_INPUT_MOUSE=','.join(map(str, (*mouse[:4], mouse[10]))))
        if args.bench:
            env['HARNESS_TIME'] = '1'
        result = subprocess.run([str(ROOT / 'build/hook/harness_client.exe'),
                                 str(ROOT / 'build/hook/subrosa_fixed.exe'), str(path), str(arguments),
                                 ','.join(functions)], capture_output=True, text=True, errors='replace',
                                timeout=600, env=env, cwd=ROOT)
        (folder / ('bench.txt' if args.bench else 'result.txt')).write_text(
            result.stdout + result.stderr, encoding='utf-8')
        if args.bench:
            rows = TIMING.findall(result.stdout)
            for function, calls, new, reference in rows:
                for metric, value in (('calls', calls), ('new', new), ('reference', reference)):
                    timings[function, metric] += int(value)
            good = result.returncode == 0 and {row[0] for row in rows} == set(functions)
        else:
            rows = RESULT.findall(result.stdout)
            good = result.returncode == 0 and {row[0] for row in rows} == set(functions)
            for function, same, different, crashed in rows:
                totals[function] += int(same)
                good &= int(same) == args.repeat and int(different) == int(crashed) == 0
        if not good:
            failures.append(name)
        print(f'{name}: {"same" if good else "FAILED"}', flush=True)
    path.unlink(missing_ok=True)
    for function in sorted(selected):
        if args.bench:
            count, reference = timings[function, 'calls'], timings[function, 'reference']
            ratio = timings[function, 'new'] / reference if reference else float('nan')
            print(f'{function}: {count} calls; ratio {ratio:.3f}')
        else:
            print(f'{function}: {totals[function]} calls same')
    print(f'{len(failures)} failed cases: {",".join(failures)}')
    return int(bool(failures) or not totals and not timings)


if __name__ == '__main__':
    sys.exit(main())
