"""Run humansimulation and human_itemsimulation of the client on one human at a time.

The snapshot of a recording holds humans that the client never simulates (it gets them from the
server), and some of them point outside the saved memory: the original crashes on them. A case
keeps one human live (the first int of the record is not zero) and clears the first int of every
other human. The case snapshot is about 350 MB, so the script deletes it after each run.

Usage: python -I instrumentation/client/synth_humans.py <recording dir> <work dir> [human,...]
Run it with the environment of the gate (docs/client.md: PATH with build/client_deps, HARNESS_GAME_DIR,
HARNESS_RESET=1).
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lift'))
from synth_bonds import Snapshot  # noqa: E402

# RVAs (image address - 0x400000) and sizes of the client.
HUMANS, HUMAN_SIZE, NUM_HUMANS = 0x14fc4a0, 0x2b64, 0x200
HUMANSIMULATION, HUMAN_ITEMSIMULATION = 0x85b70, 0x8f730

ROOT = Path(__file__).resolve().parents[2]
HARNESS = str(ROOT / 'build' / 'hook' / 'harness_client.exe')
IMAGE = str(ROOT / 'build' / 'hook' / 'subrosa_fixed.exe')


# Fields of a human (the same record layout as the server). A float value is a float field.
VEHICLE, SEAT, DESPAWN, HEALTH = 0x14, 0x18, 0x24, 0x28
MOVE_MODE, INPUT_AXES, BUTTONS, BUTTONS_PREV = 0x3c, 0x9c, 0xbc, 0xc0


def situations():
    """Each case puts the chosen human on foot (vehicle -1) in one situation."""
    foot = [(VEHICLE, -1)]
    return {
        'walk': foot + [(INPUT_AXES + 4, 0.5), (INPUT_AXES + 12, 1.0)],
        'walk_mode1': foot + [(MOVE_MODE, 1), (INPUT_AXES + 12, 1.0)],
        'walk_mode2': foot + [(MOVE_MODE, 2), (INPUT_AXES + 4, -1.0)],
        'crouch': foot + [(BUTTONS, 8), (INPUT_AXES + 12, 1.0)],
        'jump': foot + [(BUTTONS, 4), (BUTTONS_PREV, 0)],
        # Health under 25 changes the size of the human, under 10 changes it again.
        'hurt': foot + [(HEALTH, 20)],
        'hurt_low': foot + [(HEALTH, 5)],
        'dead': foot + [(HEALTH, -10), (DESPAWN, 2000)],
        'despawn': foot + [(HEALTH, -10), (DESPAWN, 1)],
    }


# Objects (vehicles): 0x8230 bytes each. The first int is not zero for a live object.
OBJECTS, OBJECT_SIZE, NUM_SEATS, SEAT0 = 0x5519120, 0x8230, 0x81a8, 0x81ac
OBJECT_POS, OBJECT_ROT = 0x24, 0x3c
BONE0_POS, POS = 0xc8, 0x48


def car_cases(car):
    """Cases with the chosen human in the seat of an object, or next to it. 'at_seat' moves the human."""
    in_car = [(VEHICLE, car), (SEAT, 0), (HEALTH, 100)]
    return {
        'drive': in_car + [(BUTTONS, 0), (BUTTONS_PREV, 0)],
        'exit_start': in_car + [(BUTTONS, 0x800), (BUTTONS_PREV, 0)],
        'exit_hold': in_car + [(BUTTONS, 0x800), (BUTTONS_PREV, 0x800)],
        'die_in_car': in_car[:2] + [(HEALTH, 0)],
        'enter': [(VEHICLE, -1), (BUTTONS, 0x800), (BUTTONS_PREV, 0), 'at_seat'],
    }


def seat_position(snap, car):
    """The world position of seat 0: pos + x rot[0] + y rot[1] + z rot[2]."""
    o = OBJECTS + car * OBJECT_SIZE
    pos = snap.read(o + OBJECT_POS, 'fff')
    rot = snap.read(o + OBJECT_ROT, 'fffffffff')
    seat = snap.read(o + SEAT0, 'fff')
    return [pos[i] + seat[0] * rot[i] + seat[1] * rot[3 + i] + seat[2] * rot[6 + i] for i in range(3)]


def run(snapshot, args, name):
    p = subprocess.run([HARNESS, IMAGE, str(snapshot), str(args), name], capture_output=True, text=True,
                       timeout=600)
    for line in p.stdout.splitlines()[::-1]:
        if 'functions tested' in line:
            return line.split(':', 1)[1].strip()
    return 'no result: ' + p.stdout[-200:].replace('\n', ' ')


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    base = Snapshot(rec / 'sv_snapshot.bin')
    live = [h for h in range(NUM_HUMANS) if base.read(HUMANS + h * HUMAN_SIZE, 'i')[0] != 0]
    print('live humans:', live)
    arg = sys.argv[3] if len(sys.argv) > 3 else ''
    # "situations:<human>" runs the cases of situations() on one human that the original can run.
    # "cars:<human>" runs the cases of car_cases() with the first object that has seats.
    car = None
    if arg.startswith('cars:'):
        cars = [o for o in range(1024) if base.read(OBJECTS + o * OBJECT_SIZE, 'i')[0] != 0
                and base.read(OBJECTS + o * OBJECT_SIZE + NUM_SEATS, 'i')[0] > 0]
        print('objects with seats:', cars)
        assert cars, 'the snapshot has no object with seats'
        car = cars[0]
    edits = situations() if arg.startswith('situations:') else car_cases(car) if car is not None else {None: []}
    want = [int(arg.split(':')[1])] if ':' in arg else ([int(x) for x in arg.split(',')] if arg else live)
    for h, (label, edit) in [(h, e) for h in want for e in edits.items()]:
        snap = Snapshot(rec / 'sv_snapshot.bin')
        for other in range(NUM_HUMANS):
            if other != h:
                snap.write(HUMANS + other * HUMAN_SIZE, 'i', 0)
        for item in edit:
            if item == 'at_seat':
                p = seat_position(snap, car)
                snap.write(HUMANS + h * HUMAN_SIZE + BONE0_POS, 'fff', *p)
                snap.write(HUMANS + h * HUMAN_SIZE + POS, 'fff', *p)
                continue
            off, value = item
            snap.write(HUMANS + h * HUMAN_SIZE + off, 'f' if isinstance(value, float) else 'i', value)
        case = work / (f'h{h}' if label is None else f'h{h}_{label}')
        case.mkdir(exist_ok=True)
        snap.save(case / 'sv_snapshot.bin')
        (case / 'sv_args.txt').write_text(f'R {HUMANSIMULATION:x}\nZ r:0\nR {HUMANSIMULATION:x}\nZ r:0\n'
                                          f'R {HUMAN_ITEMSIMULATION:x} V:{h:x}\nZ r:0\n', encoding='utf-8')
        print(f'{case.name}: humansimulation:', run(case / 'sv_snapshot.bin', case / 'sv_args.txt', 'humansimulation'))
        print(f'{case.name}: human_itemsimulation:',
              run(case / 'sv_snapshot.bin', case / 'sv_args.txt', 'human_itemsimulation'), flush=True)
        (case / 'sv_snapshot.bin').unlink()


if __name__ == '__main__':
    main()
