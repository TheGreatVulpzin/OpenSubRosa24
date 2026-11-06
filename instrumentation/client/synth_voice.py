"""Run calculateplayervoice and scenario_createchase of the client on states that change their paths.

A case writes a few values into the snapshot of a recording and calls the function. The case
snapshot is about 350 MB, so the script deletes it after each run.

Usage: python -I instrumentation/client/synth_voice.py <recording dir> <work dir> [case,...]
Run it with the environment of the gate (docs/client.md: PATH with build/client_deps, HARNESS_GAME_DIR,
HARNESS_RESET=1).
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'lift'))
from synth_bonds import Snapshot  # noqa: E402

BASE = 0x400000  # image address - BASE = RVA

# Globals of the client (image addresses).
VOICE_ON = 0x9463208                      # 0: calculateplayervoice clears the slots and returns
PLAYERS, PLAYER_SIZE = 0xf1813a0, 0x2408  # the first int of a player record is not zero for a live player
PLAYER_HUMAN = 0xf181418                  # the human of the player, -1 for none
MUTED, MUTED_SIZE = 0x12e9688c, 0x9210    # not zero: the player is muted
VOICE, VOICE_LISTENER, VOICE_SLOT = 0x16738654, 0xf170, 0x20  # 8 slots for each listener
CHASE_LIMIT = 0xe6467a8                   # scenario_createchase does nothing when it is 0

CALCULATEPLAYERVOICE, SCENARIO_CREATECHASE = 0xfbe70, 0xdd100
HARNESS = str(Path(__file__).resolve().parents[2] / 'build' / 'hook' / 'harness_client.exe')
IMAGE = str(Path(__file__).resolve().parents[2] / 'build' / 'hook' / 'subrosa_fixed.exe')


def call(rva, *args):
    return f'R {rva:x}' + ''.join(f' V:{a:x}' for a in args) + '\nZ r:0\n'


def cases(player):
    """Each case: (function name, edits, calls). An edit is (image address, format, value)."""
    slot = VOICE  # listener 0, slot 0
    human_slot = [(slot, 'i', 1), (slot + 4, 'i', player), (slot + 8, 'i', 0), (slot + 12, 'i', -1),
                  (slot + 0x1c, 'f', 0.0)]
    item_slot = [(slot, 'i', 1), (slot + 4, 'i', player), (slot + 8, 'i', -1), (slot + 12, 'i', 0),
                 (slot + 0x1c, 'f', 0.0)]
    other = 255 if player != 255 else 254
    ccv = CALCULATEPLAYERVOICE
    return {
        'voice_off': ('calculateplayervoice', [(VOICE_ON, 'i', 0)], [call(ccv, 0, 0), call(ccv, 1, 2)]),
        'voice_human_slot': ('calculateplayervoice', human_slot, [call(ccv, 0, other)]),
        'voice_item_slot': ('calculateplayervoice', item_slot, [call(ccv, 0, other)]),
        'voice_muted': ('calculateplayervoice', human_slot + [(MUTED + player * MUTED_SIZE, 'i', 1)],
                        [call(ccv, 0, other)]),
        'chase_off': ('scenario_createchase', [(CHASE_LIMIT, 'i', 0)], [call(SCENARIO_CREATECHASE, 0, 3)]),
        'chase_1': ('scenario_createchase', [(CHASE_LIMIT, 'i', 1)], [call(SCENARIO_CREATECHASE, 0, 3)]),
        'chase_5': ('scenario_createchase', [(CHASE_LIMIT, 'i', 5)], [call(SCENARIO_CREATECHASE, 0, 3)]),
    }


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
    live = [p for p in range(256) if base.read(PLAYERS + p * PLAYER_SIZE - BASE, 'i')[0] != 0]
    print('live players:', live)
    assert live, 'the snapshot has no live player'
    allc = cases(live[0])
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else list(allc)
    for name in want:
        func, edits, calls = allc[name]
        snap = Snapshot(rec / 'sv_snapshot.bin')
        for addr, fmt, value in edits:
            snap.write(addr - BASE, fmt, value)
        case = work / name
        case.mkdir(exist_ok=True)
        snap.save(case / 'sv_snapshot.bin')
        (case / 'sv_args.txt').write_text(''.join(calls), encoding='utf-8')
        print(f'{name}: {func}:', run(case / 'sv_snapshot.bin', case / 'sv_args.txt', func), flush=True)
        (case / 'sv_snapshot.bin').unlink()


if __name__ == '__main__':
    main()
