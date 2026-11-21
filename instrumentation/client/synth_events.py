"""Compare client event cases with valid ring records and world data."""
import csv
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from synth_bonds import Snapshot

EVENTSIMULATION = 0x704b0
BASE = 0x20000000
IDENTITY = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
GLOBALS = {}
for path in [ROOT / 'analysis/client_win/storage.csv',
             *sorted((ROOT / 'analysis/client_win').glob('group_globals_C*.csv'))]:
    for row in csv.DictReader(path.open(encoding='utf-8')):
        if row.get('client_rva'):
            GLOBALS[row['name']] = int(row['client_rva'], 16)


def event(kind, param=0, attach_type=0, attach_id=0, range_mode=0, age=0):
    return dict(kind=kind, param=param, attach_type=attach_type,
                attach_id=attach_id, range_mode=range_mode, age=age)


def cases():
    rows = [event(0, 0, 7), event(1), event(2, attach_id=-1), event(3, 5, 0, 2),
            event(4, 5, 0), event(5, 7 | (1 << 10) | (6 << 11), -1, -1, 2),
            event(6, 7, 3, 4321, 1),
            event(7, 3 | (1 << 9) | (1 << 8) | (3 << 10), 9, -5000, 0x1db),
            event(8, 3, -1234, 17), event(9, 0), event(10, 0, 1),
            event(11, 2 | (3 << 4) | (1 << 8) | (2 << 10) | (3 << 12),
                  0x12345678, -65535, 1234),
            event(12, 1 | (2 << 1) | (1 << 4) | (4 << 8) | (17 << 16),
                  -123456, 0x43211234, -5678),
            event(13, -1234567, -7654321, 0xffff), event(14),
            event(15, 0, 0, 0x40000001), event(16, range_mode=0), event(17, 9, 1, 3)]
    out = [(f'type_{row["kind"]}', [row], {}) for row in rows]
    out += [(f'hit_{kind}', [event(1, attach_type=kind)], {}) for kind in (1, 2, 3, 9)]
    out += [(f'damage_{kind}', [event(4, 5, kind, 0)], {}) for kind in range(1, 6)]
    out += [
        ('host_all', rows, dict(host=1)),
        ('wrap_all', rows, dict(cursor=65530)),
        ('empty', [], {}),
        ('sound_age_99', [event(9, age=99)], {}),
        ('sound_age_100', [event(9, age=100)], {}),
        ('sound_future', [event(9, age=-1)], {}),
        ('sound_tick_wrap', [event(9, age=99)], dict(tick=3)),
        ('shot_magnum', [rows[0]], dict(weapon=10)),
        ('shot_rifle', [rows[0]], dict(weapon=3)),
        ('expired_effects', [event(0, 0, 7, age=100), event(1, age=100),
                             event(4, 5, 0, age=100), event(16, range_mode=1, age=100)], {}),
        ('player_inactive', [event(7, 3)], {}),
        ('player_no_human', [event(7, 3 | (1 << 9), -1)], {}),
        ('human_remove', [event(17, 9, 0, 3)], {}),
        ('human_no_owner', [event(17, 9, 1, -1)], {}),
        ('human_remove_no_owner', [event(17, 9, 0, -1)], {}),
        ('item_active', [rows[5]], dict(item_active=1)),
        ('item_inactive', [event(5, 7, -1, -1)], {}),
        ('mission_inactive', [event(12, (2 << 1) | (1 << 4))], {}),
        ('mission_site_none', [event(12, 1, 0x1234, 0xffff, -1)], {}),
        ('scenario_count_retains', [rows[11]], dict(scenario_count=16)),
        ('block_no_face', [event(16, range_mode=-1)], {}),
        ('block_plain', [event(16, range_mode=0)], dict(block=0x40000001)),
        ('block_face_pair', [event(16, range_mode=1)], {}),
        ('door_close', [event(10)], {}),
        ('messages', [event(2, attach_type=k, attach_id=0) for k in range(5)], {}),
        ('messages_expired', [event(2, attach_type=k, attach_id=0, age=100) for k in range(5)], {}),
    ]
    return out


def prepare(snap, events, host=0, cursor=0, tick=5000, weapon=6, item_active=0,
            scenario_count=0, block=0x80000000):
    def write(name, offset, fmt, *values):
        snap.write(GLOBALS[name] + offset, fmt, *values)
    write('server', 0, 'i', host)
    write('game', 0x34, 'I', tick)
    write('game', 0x22fd4, 'i', scenario_count)
    write('last_event', 0, 'I', cursor)
    write('numofevents', 0, 'I', (cursor + len(events)) & 65535)
    write('sprite_count', 0, 'i', 0)
    write('sprite_model_count', 0, 'i', 0)
    write('numofbullets', 0, 'i', 0)
    write('audio_sound_count', 0, 'i', 0)
    write('audio_source_limit', 0, 'i', 32)
    write('audio_enabled', 0, 'i', 1)
    write('sound_buffers', 4, 'i', 1)
    write('audio_camera', 4, '3f', 1.0, 2.0, 3.0)
    write('soundlist', 4, '47i', *([0] * 47))
    write('chat_head', 0, 'I', 0)
    write('item', 7 * 0x1ac, 'i', item_active)
    write('item', 7 * 0x1ac + 12, 'i', weapon)
    write('item', 7 * 0x1ac + 0x84, '9f', *IDENTITY)
    write('object', 5 * 0x8230 + 4, 'i', 0)
    write('object', 5 * 0x8230 + 0x24, '3f', 1.0, 2.0, 3.0)
    write('object', 5 * 0x8230 + 0x3c, '9f', *IDENTITY)
    write('objecttype', 0x1344, 'i', 2)
    write('objecttype', 0x775c, 'i', 2)
    for wheel in range(2):
        write('objecttype', 0x7760 + wheel * 0x44 + 0x18, 'f', 0.75)
        write('objecttype', 0x7760 + wheel * 0x44 + 0x40, 'i', wheel)
    write('modellist', 4, 'i', 0)
    write('model', 0x100, 'i', 1)
    write('model', 0x104, '12fi', 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0)
    scratch = 0x19e20000
    snap.write(scratch, 'i', 0)
    write('area', 0x34, '3i2fI', 1, 1, 1, 1.0, 1.0, BASE + scratch)
    write('area', 0x568, 'i512i', 1, *([0] + [-1] * 511))
    write('area_sector', 0x40, 'iI', 1, block)
    write('area_sector', 0x40 + 0x3004, 'I', block)
    write('area_sector', 0x40 + 0x5004, '4i', -1, -1, -1, -1)
    write('area_block', 0x40, 'iI', 4, BASE + scratch + 16)
    write('area_block', 0x54, 'iI', 2, BASE + scratch + 80)
    snap.write(scratch + 16, '12f', 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0)
    snap.write(scratch + 80, '8i', 0, 1, 2, 3, 0, 1, 2, 3)
    for door in range(2):
        write('game', 0xac + door * 16, '4i', 0, 0, 0, 0)
    for index, data in enumerate(events):
        slot = (cursor + index) & 65535
        record = bytearray(128)
        struct.pack_into('<iI6f4i2f', record, 0, data['kind'], (tick - data['age']) & 0xffffffff,
                         1.0, 2.0, 3.0, 0.0, 0.0, 1.0, data['param'], data['attach_type'],
                         data['attach_id'], data['range_mode'], 0.75, 1.25)
        record[64:64 + 11] = b'event test' + bytes(1)
        write('event', slot * 128, '128s', record)


def run_cases(rec, work, count=30):
    rec, work = Path(rec), Path(work)
    work.mkdir(parents=True, exist_ok=True)
    selected = cases()
    selected += [(f'repeat_{i}', *selected[i % len(selected)][1:])
                 for i in range(max(0, count - len(selected)))]
    base = Snapshot(rec / 'sv_snapshot.bin')
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build/client_test'))
    env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env.get('PATH', '')
    ok = 0
    for name, events, options in selected:
        snap = Snapshot.__new__(Snapshot)
        snap.__dict__ = dict(base.__dict__)
        snap.pages = {key: bytearray(value) for key, value in base.pages.items()}
        prepare(snap, events, **options)
        snapshot = work / 'sv_snapshot.bin'
        args = work / 'sv_args.txt'
        snap.save(snapshot)
        args.write_text(f'R {EVENTSIMULATION:x}\nZ r:0\n', encoding='ascii')
        result = subprocess.run([str(ROOT / 'build/hook/harness_client.exe'),
                                 str(ROOT / 'build/hook/subrosa_fixed.exe'),
                                 str(snapshot), str(args), 'eventsimulation'],
                                capture_output=True, text=True, errors='replace', env=env, cwd=ROOT,
                                timeout=120)
        (work / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        matches = re.findall(r'(\d+) calls same, (\d+) different, (\d+) known differences, (\d+) \w+ crashed', result.stdout)
        passed = bool(matches and int(matches[-1][0]) > 0 and matches[-1][2] == '0'
                      and matches[-1][1] == '0' and matches[-1][3] == '0' and result.returncode == 0)
        ok += passed
        summary = next((line.strip() for line in result.stdout.splitlines()
                        if 'functions tested:' in line), 'no comparison')
        print(f'{name}: {summary}', flush=True)
        snapshot.unlink()
        if not passed:
            print(result.stdout[-1600:], flush=True)
    print(f'eventsimulation: {ok}/{len(selected)} cases same', flush=True)
    return ok == len(selected)


if __name__ == '__main__':
    raise SystemExit(0 if run_cases(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 30) else 1)
