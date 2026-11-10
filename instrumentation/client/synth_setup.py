"""Run the small setup, debug and timer functions of the client on made states.

No recording calls these functions. A case writes records into the snapshot of a recording and
calls each function with arguments that reach every branch. The case snapshot is about 350 MB,
so the script deletes it after the run.

Usage: python -I instrumentation/client/synth_setup.py <recording dir> <work dir> [case,...]
Cases: jpeg, timers, debug, debugoff, log, ticket, mouseon, mouseoff, tickclient, tickhost,
tickkeys, tickkeys2, tickdebug, tickshift, world, world2, world3, world4, send1, send2, send3, sendnone. The script sets the environment of the gate itself.
"""
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_textures import HARNESS, IMAGE, MODELTEMP, NAN, ROOT, bits, call, floats, image, run  # noqa: E402,F401

sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from synth_bonds import Snapshot  # noqa: E402

# Functions (RVA).
RESET, UPDATEPLAYTICKET = 0x36f0, 0xd5650
RESETTIMER, STARTTIMER = 0xfafe0, 0xfb020
REACTIVATEGAME, RELATIVE_MOUSE = 0x7d060, 0x9065350
CHECKSIMULATION = 0x7d900
HUMANSIMULATIONCLIENT, ITEMSIMULATIONCLIENT, CLIENT_SIMULATION = 0x896d0, 0x99500, 0xc2bb0
CLIENTSEND = 0x68030
SPEEX, VOICE_RING, VOICE_RING_SIZE = 0x12a7d680, 0x10000, 0x9210
LAST_EVENT, INPUT_WHEEL, SERVER_ADDRESS = 0x5350c6c, 0x908aad8, 0x75a5854

# Records of the world (RVA, size of a record).
HUMAN, HUMAN_SIZE = 0x14fc4a0, 0x2b64
OBJECT, OBJECT_SIZE = 0x5519120, 0x8230
ITEM, ITEM_SIZE = 0x908b080, 0x1ac
PLAYER, PLAYER_SIZE = 0xed813a0, 0x2408
GAME_TICK = 0x9065334
# State of the client tick (RVA).
SERVER_ACTIVE, GAME_STATE, GAMETYPE = 0x75a5800, 0x9065320, 0x9065308
PAUSED, STEP_ONCE, SLOW_TICK, LAST_TICK_MS = 0x906533c, 0x9065340, 0x9065344, 0x9065338
VIEW_PLAYER = 0xfefe2a0
KEYBOARD, PREVKEYBOARD = 0x75a5140, 0x1bbe1a0
VOICE_ON, MIC_GAIN, HIDE_HUD, LIGHT_ANGLES = 0x9063218, 0x9063220, 0x908b054, 0x9065348
KEY_V, KEY_EQUALS, KEY_F1, KEY_F4, KEY_F5, KEY_PAGE_UP, KEY_PAGE_DOWN = 25, 46, 58, 61, 62, 75, 78
KEY_LBRACKET, KEY_RBRACKET, KEY_LSHIFT = 47, 48, 225
ADDCROSS, ADDATAINT, ADDATAFLOAT, WRITELOGFILE = 0x6a230, 0x6aaa0, 0x6ab70, 0x6aca0

# Records (RVA).
DEBUG, DEBUGIT = 0x14a8680, 0x127ea8
NUM_LINES, NUM_VALUES, VALUES, LOG_PATH = 0xc, 0x50c80, 0x50c84, 0x52384
VALUE_SIZE = 0x2c
TIMERS, TIMER_SIZE = 0x1332e80, 0x58
JPEG_SIZE, RESTART_INTERVAL = 0x3700, 0x36b8

# Free memory of the image for made records: the end of the model in work.
SCRATCH = MODELTEMP + 0x250000


def text(rva, string):
    """An edit that writes a zero ended string."""
    raw = string.encode() + b'\0'
    return (rva, f'{len(raw)}s', [raw])


def noise(rva, size, rng):
    return (rva, f'{size}B', [rng.getrandbits(8) for _ in range(size)])


def jpeg_case():
    """Three JPEG records full of noise, with no restart interval, a small one and a large one."""
    rng = random.Random(7)
    edits, calls = [], []
    for i, interval in enumerate((0, 7, 0x7fffffff)):
        base = SCRATCH + i * 0x4000
        edits.append(noise(base, JPEG_SIZE, rng))
        edits.append((base + RESTART_INTERVAL, 'i', [interval]))
        calls.append(call(RESET, image(base)))
    return edits, {'reset': calls}


def timers_case():
    rng = random.Random(8)
    edits = [noise(TIMERS + i * TIMER_SIZE, TIMER_SIZE, rng) for i in (0, 1, 2, 100, 255)]
    names = ['', 'bond', 'x' * 40, 'physics-' * 7 + 'z']
    edits += [text(SCRATCH + 0x100 * k, s) for k, s in enumerate(names)]
    return edits, {
        'resettimer': [call(RESETTIMER, i, image(SCRATCH + 0x100 * k)) for i in (0, 1, 2, 100, 255)
                       for k in range(len(names))],
        'starttimer': [call(STARTTIMER, i) for i in (0, 1, 2, 100, 255)],
    }


def values_edits(count, rng):
    """The first count named values, a mix of integers and floats."""
    edits = [(DEBUG + NUM_VALUES, 'i', [count])]
    for i in range(count):
        base = DEBUG + VALUES + i * VALUE_SIZE
        edits.append((base, '2iI', [i & 1, rng.getrandbits(31), bits(rng.uniform(-9, 9))]))
        edits.append(text(base + 12, f'v{i}'))
    return edits


def debug_case(on=True):
    rng = random.Random(9)
    edits = [(DEBUGIT, 'i', [1 if on else 0]), (DEBUG + NUM_LINES, 'i', [4091])]
    edits += values_edits(30, rng)
    strings = ['v0', 'v7', 'v29', 'n30', 'n31', 'n32', 'n33'] + [f'm{i}' for i in range(110)]
    edits += [text(SCRATCH + 0x1000 + 0x20 * k, s) for k, s in enumerate(strings)]
    point = SCRATCH + 0x100
    edits.append((point, '3f', [1.5, -2.0, 3.25]))
    name = lambda k: image(SCRATCH + 0x1000 + 0x20 * k)  # noqa: E731
    return edits, {
        'debug_addcross': [call(ADDCROSS, image(point), s) for s in (0.5, 1e10, -1.0, NAN)],
        'debug_addataint': [call(ADDATAINT, name(k), rng.getrandbits(31)) for k in range(len(strings))],
        'debug_addatafloat': [call(ADDATAFLOAT, name(k), floats(rng)) for k in range(14)],
    }


def log_case():
    """The log file: the path is a name in the game folder."""
    edits = [(DEBUGIT, 'i', [1]), text(DEBUG + LOG_PATH, 'synthlog.txt'), text(SCRATCH, 'first line'),
             text(SCRATCH + 0x40, '')]
    return edits, {'debug_writelogfile': [call(WRITELOGFILE, image(SCRATCH)), call(WRITELOGFILE, image(SCRATCH + 0x40))]}


def ticket_case():
    return [], {'updateplayticket': [call(UPDATEPLAYTICKET)]}


def reactivate_case(mouse):
    """reactivategame with the relative mouse flag on and off."""
    return [(RELATIVE_MOUSE, 'i', [mouse])], {'reactivategame': [call(REACTIVATEGAME)]}


def tick_edits(active, state, view=-1, debug=0):
    """A client or host that is ready for a tick, with no player in view."""
    return [(SERVER_ACTIVE, 'i', [active]), (GAME_STATE, 'i', [state]), (VIEW_PLAYER, 'i', [view]),
            (DEBUGIT, 'i', [debug]), (LAST_TICK_MS, 'I', [0]), (PAUSED, 'i', [0]), (STEP_ONCE, 'i', [0]),
            (SLOW_TICK, 'i', [0]), (MIC_GAIN, 'i', [100])]


def key_edits(*pressed, held=()):
    """Keys that went down in this tick (pressed) and keys that are down (held)."""
    edits = [(KEYBOARD, '512B', [0] * 512), (PREVKEYBOARD, '512B', [0] * 512)]
    edits += [(KEYBOARD + k, 'B', [1]) for k in tuple(pressed) + tuple(held)]
    return edits


def tick_case(active, state, input_calls, keys=(), held=(), debug=0, gametype=None):
    edits = tick_edits(active, state, debug=debug) + key_edits(*keys, held=held)
    if gametype is not None:
        edits.append((GAMETYPE, 'i', [gametype]))
    return edits, {'checksimulation': [call(CHECKSIMULATION, a) for a in input_calls]}


def floats_edit(rva, count, rng, low=-2.0, high=2.0):
    return (rva, f'{count}f', [rng.uniform(low, high) for _ in range(count)])


def world_case(seed=11):
    """Humans on foot and in vehicles, objects and items with all kinds of holders. The function
    that a case calls runs on the same records, so the three smoothing functions share it."""
    rng = random.Random(seed)
    edits = [(GAME_TICK, 'I', [1000])]
    buttons = [0, 0x800, 0x800, 0]
    for p, b in enumerate(buttons):
        edits.append((PLAYER + p * PLAYER_SIZE + 0x9c, 'I', [b]))
    # (vehicle, seat, player, exit_timer, net_tick, net_tick_prev)
    humans = [(-1, 0, -1, 0, 990, 980), (-1, 0, 1, 5, 1000, 990), (0, 0, 0, 0, 995, 990), (1, 1, 1, 10, 900, 900),
              (2, 2, 2, 30, 990, 990), (3, 0, 3, 0, 1500, 1400), (0, 3, 1, 2, 999, 1)]
    for h, (vehicle, seat, player, exit_timer, net_tick, net_tick_prev) in enumerate(humans):
        base = HUMAN + h * HUMAN_SIZE
        edits.append(noise(base, HUMAN_SIZE, rng))
        edits.append((base, 'i', [1]))
        edits.append((base + 8, 'i', [player]))
        edits.append((base + 0x14, '3i', [vehicle, seat, 0]))
        edits.append((base + 0x30, 'i', [exit_timer]))
        edits.append(floats_edit(base + 0xc8, 15 * 0xe0 // 4, rng))
        edits.append((base + 0x2b44, 'i', [net_tick]))
        edits.append(floats_edit(base + 0x2b48, 3, rng, -50, 50))
        edits.append((base + 0x2b54, 'i', [net_tick_prev]))
        edits.append(floats_edit(base + 0x2b58, 3, rng, -50, 50))
    for o in range(4):
        base = OBJECT + o * OBJECT_SIZE
        edits.append(noise(base, 0x100, rng))
        edits.append((base, 'i', [1]))
        edits.append((base + 4, 'i', [o]))
        edits.append(floats_edit(base + 0x24, 3 + 9, rng, -9, 9))
        edits.append((base + 0x8210, 'i', [990 + o * 3]))
        edits.append(floats_edit(base + 0x8214, 3, rng, -50, 50))
        edits.append((base + 0x8220, 'i', [980]))
        edits.append(floats_edit(base + 0x8224, 3, rng, -50, 50))
    # (type, countdown, parent_item, parent_human, parent_slot)
    items = [(1, 5, -1, -1, 0), (0xf, 0, 2, -1, 3), (0xd, 0, -1, -1, 0), (0x10, 0, 2, -1, 4), (0xe, 3, -1, -1, 0),
             (0xf, 0, 4, 1, 1), (0x3, 0, 0, 3, 2), (0x10, 0, 1, 4, 5), (0xf, 0, 6, -1, 2)]
    for n, (kind, countdown, parent_item, parent_human, slot) in enumerate(items):
        base = ITEM + n * ITEM_SIZE
        edits.append(noise(base, ITEM_SIZE, rng))
        edits.append((base, 'i', [1]))
        edits.append((base + 0xc, 'i', [kind]))
        edits.append((base + 0x18, '3i', [parent_human, parent_item, slot]))
        edits.append((base + 0xf4, 'i', [countdown]))
        edits.append(floats_edit(base + 0x48, 3, rng, -9, 9))
        edits.append(floats_edit(base + 0x84, 9, rng))
    functions = {'humansimulationclient': [call(HUMANSIMULATIONCLIENT)],
                 'itemsimulationclient': [call(ITEMSIMULATIONCLIENT)],
                 'client_simulation': [call(CLIENT_SIMULATION)]}
    return edits, functions


def send_case(seed, view, actions, tail, head):
    """A client with a player in view, a ring of queued actions and a ring of voice frames."""
    rng = random.Random(seed)
    base = PLAYER + max(view, 0) * PLAYER_SIZE
    edits = [(VIEW_PLAYER, 'i', [view]), (SERVER_ADDRESS, 'IH', [0x0100007f, 27020]),
             (LAST_EVENT, 'I', [rng.getrandbits(32)]), (INPUT_WHEEL, 'i', [rng.randint(-100, 100)])]
    edits.append(noise(base, PLAYER_SIZE, rng))
    edits.append((base + 0xe4, '2i', [actions, tail]))
    edits.append((base + 0xc8, 'i', [rng.randint(0, 3)]))
    # The actions: menu, chat and item, then noise kinds.
    for n in range(64):
        kind = n % 4
        record = base + 0xec + n * 0x54
        edits.append((record, '3i', [kind, rng.randint(0, 60) if kind == 1 else rng.randint(0, 255), rng.randint(0, 255)]))
    ring = SPEEX + VOICE_RING + max(view, 0) * VOICE_RING_SIZE
    edits.append((ring + 0x5000, 'i', [head]))
    edits.append((ring + 0x5008, '64i', [rng.randint(0, 255) for _ in range(64)]))
    edits.append((ring + 0x5108, '64i', [rng.randint(0, 3) for _ in range(64)]))
    edits.append(noise(ring + 0x5208, 64 * 256, rng))
    edits.append((ring + 0x920c, 'i', [rng.randint(0, 1)]))
    return edits, {'clientsend': [call(CLIENTSEND)]}


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build' / 'client_test'),
               HARNESS_ENV_DIR=str(work / 'env'), SV_UI_WIDTH='220')
    env['PATH'] = str(ROOT / 'build' / 'client_deps') + os.pathsep + env.get('PATH', '')
    allc = {'jpeg': jpeg_case, 'timers': timers_case, 'debug': debug_case,
            'debugoff': lambda: debug_case(False), 'log': log_case, 'ticket': ticket_case,
            'mouseon': lambda: reactivate_case(1), 'mouseoff': lambda: reactivate_case(0),
            'send1': lambda: send_case(1, 0, 10, 4, 20), 'send2': lambda: send_case(2, 1, 3, 60, 3),
            'send3': lambda: send_case(3, 2, 40, 10, 0), 'sendnone': lambda: send_case(4, -1, 5, 1, 7),
            'world': world_case, 'world2': lambda: world_case(12), 'world3': lambda: world_case(13),
            'world4': lambda: world_case(14),
            'tickclient': lambda: tick_case(0, 3, (0, 1)),
            'tickhost': lambda: tick_case(1, 3, (0, 1)),
            'tickkeys': lambda: tick_case(1, 3, (1,), keys=(KEY_F1, KEY_F4, KEY_PAGE_UP)),
            'tickkeys2': lambda: tick_case(1, 3, (1,), keys=(KEY_PAGE_DOWN, KEY_EQUALS), debug=1),
            'tickdebug': lambda: tick_case(1, 3, (1,), keys=(KEY_V, KEY_F5), held=(KEY_LBRACKET,), debug=1, gametype=4),
            'tickshift': lambda: tick_case(1, 3, (1,), keys=(KEY_F5,), held=(KEY_LSHIFT, KEY_RBRACKET), debug=1)}
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else list(allc)
    for name in want:
        edits, functions = allc[name]()
        case = work / name
        case.mkdir(exist_ok=True)
        snapshot = rec / 'sv_snapshot.bin'
        if edits:
            snap = Snapshot(snapshot)
            for rva, fmt, values in edits:
                snap.write(rva, fmt, *values)
            snapshot = case / 'sv_snapshot.bin'
            snap.save(snapshot)
        for function, calls in functions.items():
            args = case / f'{function}.txt'
            args.write_text(''.join(calls), encoding='utf-8', newline='\n')
            print(f'{name}: {function}: {len(calls)} calls:', run(snapshot, args, function, env), flush=True)
        if edits:
            snapshot.unlink()


if __name__ == '__main__':
    main()
