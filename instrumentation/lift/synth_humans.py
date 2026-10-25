"""Test the paths of humansimulation that the recordings may not reach.

Reads a recording snapshot, puts one human into a given situation, and writes a new snapshot
and an args file with one humansimulation call, in one folder per case:

    python -I instrumentation/lift/synth_humans.py <recording dir> <out dir> [case,...]
    build/hook/harness.exe build/hook/024d_fixed.exe <out dir>/<case>/sv_snapshot.bin <out dir>/<case>/sv_args.txt humansimulation

The output holds the game state of the recording: keep it in build/.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot, find, HUMANS, HUMAN_SIZE, OBJECTS, OBJECT_SIZE

GAME_TICKS = 0x90dc540 + 0x38
NUM_SEATS = 0x81a8

# Human fields.
PLAYER, ACCOUNT, VEHICLE, SEAT, DESPAWN, HEALTH = 0x8, 0xc, 0x14, 0x18, 0x24, 0x28
EXIT_TIMER, BUTTONS, BUTTONS_PREV = 0x30, 0xbc, 0xc0
INVENTORY0_COUNT, BLEEDING = 0x291c, 0x2b1c
MOVE_MODE, INPUT_AXES, POS, BONE0_POS = 0x3c, 0x9c, 0x48, 0xc8


def cases(h, car):
    """Each case: a list of (offset in the human, value), with a float value for a float field.
    'at_seat' puts bone 0 and pos at seat 0 of the object."""
    in_car = [(VEHICLE, car), (SEAT, 0), (HEALTH, 100)]
    return {
        'drive': in_car + [(EXIT_TIMER, 0), (BUTTONS, 0), (BUTTONS_PREV, 0)],
        'drive_item': in_car + [(EXIT_TIMER, 0), (INVENTORY0_COUNT, 1)],
        'exit_start': in_car + [(EXIT_TIMER, 0), (BUTTONS, 0x800), (BUTTONS_PREV, 0)],
        'exit': in_car + [(EXIT_TIMER, 60), (BUTTONS, 0x800), (BUTTONS_PREV, 0x800)],
        'exit_slow': in_car + [(EXIT_TIMER, 5), (BUTTONS, 0x800), (BUTTONS_PREV, 0x800)],
        'die_in_car': in_car[:2] + [(HEALTH, 0)],
        'dead_player': [(VEHICLE, -1), (HEALTH, -10), (DESPAWN, 2000)],
        'dead_npc': [(VEHICLE, -1), (HEALTH, -10), (DESPAWN, 2000), (PLAYER, -1), (ACCOUNT, 0)],
        'bleed': [(VEHICLE, -1), (HEALTH, 50), (BLEEDING, 1)],
        'bleed_low': [(VEHICLE, -1), (HEALTH, 8), (BLEEDING, 1)],
        'despawn': [(VEHICLE, -1), (PLAYER, -1), (DESPAWN, 1)],
        'walk': [(VEHICLE, -1), (INPUT_AXES + 4, 0.5), (INPUT_AXES + 12, 1.0)],
        'walk_mode1': [(VEHICLE, -1), (MOVE_MODE, 1), (INPUT_AXES + 12, 1.0)],
        'walk_mode2': [(VEHICLE, -1), (MOVE_MODE, 2), (INPUT_AXES + 4, -1.0)],
        'crouch': [(VEHICLE, -1), (BUTTONS, 8), (INPUT_AXES + 12, 1.0)],
        'jump': [(VEHICLE, -1), (BUTTONS, 4), (BUTTONS_PREV, 0)],
        'enter': [(VEHICLE, -1), (BUTTONS, 0x800), (BUTTONS_PREV, 0), 'at_seat'],
    }


def seat_position(snap, car):
    """The world position of seat 0 of an object: pos + x rot[0] + y rot[1] + z rot[2]."""
    o = OBJECTS + car * OBJECT_SIZE
    pos = snap.read(o + 0x24, 'fff')
    rot = snap.read(o + 0x3c, 'fffffffff')
    seat = snap.read(o + 0x81ac, 'fff')
    return [pos[i] + seat[0] * rot[i] + seat[1] * rot[3 + i] + seat[2] * rot[6 + i] for i in range(3)]


def main():
    rec, out = Path(sys.argv[1]), Path(sys.argv[2])
    base = Snapshot(rec / 'sv_snapshot.bin')
    with_physics = lambda s, a: s.read(a, 'i')[0] != 0 and s.read(a + 4, 'i')[0] != 0
    humans = find(base, HUMANS, HUMAN_SIZE, 512, with_physics)
    cars = [o for o in find(base, OBJECTS, OBJECT_SIZE, 1024, lambda s, a: s.read(a, 'i')[0] != 0)
            if base.read(OBJECTS + o * OBJECT_SIZE + NUM_SEATS, 'i')[0] > 0]
    assert humans and cars, 'the snapshot lacks a human or an object with seats'
    h, car = humans[0], cars[0]
    print('human', h, 'object', car)
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    for name, edits in cases(h, car).items():
        if want and name not in want:
            continue
        snap = Snapshot(rec / 'sv_snapshot.bin')
        for edit in edits:
            if edit == 'at_seat':
                p = seat_position(snap, car)
                snap.write(HUMANS + h * HUMAN_SIZE + BONE0_POS, 'fff', *p)
                snap.write(HUMANS + h * HUMAN_SIZE + POS, 'fff', *p)
                continue
            off, value = edit
            snap.write(HUMANS + h * HUMAN_SIZE + off, 'f' if isinstance(value, float) else 'i', value)
        if name.startswith('bleed'):
            ticks = snap.read(GAME_TICKS, 'i')[0]
            snap.write(GAME_TICKS, 'i', (ticks & ~0xff) | ((h * h * h) & 0xff))
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        snap.save(d / 'sv_snapshot.bin')
        (d / 'sv_args.txt').write_text('R 51060\n')
        print('case', name)


if __name__ == '__main__':
    main()
