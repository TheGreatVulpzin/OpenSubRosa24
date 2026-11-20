"""Run unpackevent and unpackobjectpacket of the client on valid packets.

The functions read a packet that the server sent. A random buffer is not a valid packet, so the
harness shows differences for damaged packets only. Each case puts one valid event, or the lists of
humans, objects, items and city objects, in the receive buffer of the snapshot and calls the function.
The case snapshot is about 350 MB, so the script deletes it after each run.

Usage: python -I instrumentation/client/synth_net.py <recording dir> <work dir> [case,...]
Run it with the environment of the gate (docs/client.md: PATH with build/client_deps, HARNESS_GAME_DIR,
HARNESS_RESET=1). The functions must be in an adoption list.
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
sys.path.insert(0, str(ROOT / 'tools' / 'net'))
from synth_bonds import Snapshot  # noqa: E402
import srpacket as sp  # noqa: E402

HARNESS = str(ROOT / 'build' / 'hook' / 'harness_client.exe')
IMAGE = str(ROOT / 'build' / 'hook' / 'subrosa_fixed.exe')

# RVAs of the client.
BSD = 0x19d851a0
RECV_SIZE, RECV_POS, RECV_BIT, RECV_BUF = BSD + 0x10064, BSD + 0x10068, BSD + 0x1006c, BSD + 0x10070
UNPACKEVENT, UNPACKOBJECTPACKET = 0x73aa0, 0xc1dc0

SAMPLE_EVENTS = {
    0: dict(weapon=3, shooter=77, pos=(1.0, 2.0, 3.0), vel=(0.5, -0.25, 9.0)),
    1: dict(kind=2, target=40, pos=(4.0, 5.0, 6.0), normal=(0.0, 1.0, 0.0)),
    2: dict(text=b'hello from the server', attach_type=0, attach_id=-1, range_mode=1),
    3: dict(object=55, object_type=9, variant=-1),
    4: dict(object=56, arg_a=3, arg_b=700, vec_a=(1.0, 1.5, 2.0), vec_b=(3.0, 3.5, 4.0)),
    5: dict(item_key=0x12345, parent_human=-1, parent_item=7, parent_slot=3),
    6: dict(item=100, call_state=3, info=4321, info2=1),
    7: dict(packed=0x1234, human=-1, money=-5000, look=0x2abc, name=b'Playername'),
    8: dict(player=9, money=123456, stocks=-4),
    9: dict(sound=12, pos=(7.0, 8.0, 9.0), volume=0.75, pitch=1.25),
    10: dict(door=2, open=1),
    11: dict(a=1, b=-2, c=3, d=-4),
    12: dict(a=10, b=-20, c=30, d=-40),
    13: dict(a=100, b=200, c=300),
    14: dict(a=5, b=6, c=7, d=0x12345),
    15: dict(a=11, b=12, c=13),
    16: dict(a=1, b=-2, c=3, d=-1, vec_a=(1.0, 2.0, 3.0), vec_b=(0.0, 0.0, 1.0)),
    17: dict(human=300, active=1, player=-1),
}


def event_bytes(ev):
    w = sp.PacketWriter()
    ev.write(w)
    return w.getvalue()


def event_cases():
    """One valid event of each type, and some packets that end early."""
    out = {}
    head = b'7DFP'
    for t, fields in SAMPLE_EVENTS.items():
        out['unpackevent_%d' % t] = (UNPACKEVENT, head + event_bytes(sp.Event(t, 5000, fields)))
    out['unpackevent_filler'] = (UNPACKEVENT, head + event_bytes(sp.Event(sp.EV_FILLER)))
    out['unpackevent_nego'] = (UNPACKEVENT, head + event_bytes(sp.Event(3, 9, dict(object=1, object_type=2, variant=15))))
    out['unpackevent_text_empty'] = (UNPACKEVENT, head + event_bytes(sp.Event(2, 9, dict(text=b''))))
    out['unpackevent_text_long'] = (UNPACKEVENT, head + event_bytes(sp.Event(2, 9, dict(text=b'x' * 63))))
    return out


def world_blocks():
    h = 3
    pos = (10.0, 20.0, 30.0)
    oa, ob = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)
    joints = [(0.1 * k, -0.2, 0.3) for k in range(14)]
    return dict(
        humans=[sp.HumanBlock(h, 0, -1, 2, 80, 1, pos, oa, ob, joints)],
        humans_car=[sp.HumanBlock(h, 0, 3, 1, 50, 0, pos, (0.0, 0.6, 0.8), (0.0, -0.8, 0.6), joints[:8])],
        objects=[sp.ObjectBlock(5, 0, pos, oa, ob, [0.1, -0.2, 0.3, 0.4, 0.5, -0.6],
                                [0.1, -0.2, 0.3, 0.4, 0.5, -0.6, 0.7, 0.9], (0.5, -0.25, 1.0), 1234)],
        objects1=[sp.ObjectBlock(6, 1, pos, (0.0, 0.0, 1.0), (1.0, 0.0, 0.0), [0.0] * 6, [0.0] * 8,
                                 (0.0, 0.0, 0.0), 77)],
        objects2=[sp.ObjectBlock(7, 2, pos)],
        items=[sp.ItemBlock(10, pos, oa, ob), sp.ItemBlock(11, pos, ob, oa, (3, 4, 0x1234567))],
        cityobjects=[sp.CityObjectBlock(9, pos, oa, ob, 3, 4, -0.5)],
    )


def object_cases():
    """The four lists of the object packet."""
    blocks = world_blocks()
    out = {}

    def case(name, humans=(), objects=(), items=(), cityobjects=()):
        w = sp.PacketWriter()
        for lst in (humans, objects, items, cityobjects):
            w.write_bits(len(lst), 8)
            for b in lst:
                b.write(w)
        out[name] = (UNPACKOBJECTPACKET, w.getvalue())
    case('unpackobjectpacket_humans', blocks['humans'])
    case('unpackobjectpacket_humans_car', blocks['humans_car'])
    case('unpackobjectpacket_objects', objects=blocks['objects'])
    case('unpackobjectpacket_objects1', objects=blocks['objects1'])
    case('unpackobjectpacket_objects2', objects=blocks['objects2'])
    case('unpackobjectpacket_items', items=blocks['items'])
    case('unpackobjectpacket_cityobjects', cityobjects=blocks['cityobjects'])
    case('unpackobjectpacket_all', blocks['humans'], blocks['objects'] + blocks['objects1'] + blocks['objects2'],
         blocks['items'], blocks['cityobjects'])
    case('unpackobjectpacket_empty')
    return out


def run(snapshot, args, name):
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build' / 'client_test'))
    env['PATH'] = str(ROOT / 'build' / 'client_deps') + os.pathsep + env.get('PATH', '')
    p = subprocess.run([HARNESS, IMAGE, str(snapshot), str(args), name], capture_output=True, text=True,
                       errors='replace', timeout=900, env=env, cwd=ROOT)
    for line in p.stdout.splitlines()[::-1]:
        if 'functions tested' in line or ' same ' in line and 'different' in line:
            return line.strip()
    return 'no result: ' + p.stdout[-200:].replace('\n', ' ')


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    work.mkdir(parents=True, exist_ok=True)
    cases = {}
    cases.update(event_cases())
    cases.update(object_cases())
    for name, (rva, data) in cases.items():
        if want and name not in want:
            continue
        snap = Snapshot(rec / 'sv_snapshot.bin')
        for i, b in enumerate(data):
            snap.write(RECV_BUF + i, 'B', b)
        snap.write(RECV_SIZE, 'i', len(data))
        snap.write(RECV_POS, 'i', 4 if rva == UNPACKEVENT else 0)
        snap.write(RECV_BIT, 'i', 0)
        case = work / name
        case.mkdir(exist_ok=True)
        snap.save(case / 'sv_snapshot.bin')
        line = 'R %x' % rva + (' V:1234' if rva == UNPACKEVENT else '') + '\nZ r:0\n'
        (case / 'sv_args.txt').write_text(line, encoding='utf-8', newline='\n')
        func = 'unpackevent' if rva == UNPACKEVENT else 'unpackobjectpacket'
        print(f'{name}: {run(case / "sv_snapshot.bin", case / "sv_args.txt", func)}', flush=True)
        (case / 'sv_snapshot.bin').unlink()


if __name__ == '__main__':
    main()
