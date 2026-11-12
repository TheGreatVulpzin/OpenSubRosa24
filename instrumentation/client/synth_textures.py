"""Run the noise, curve, model and object type functions of the client on made inputs.

No recording calls these functions. A case writes a few values into the snapshot of a recording
(or uses it as it is) and calls each function many times. The harness compares the clean function
with the original: the result and the memory. A case snapshot is about 350 MB, so the script
deletes it after the run.

Usage: python -I instrumentation/client/synth_textures.py <recording dir> <work dir> [case,...]
Cases: noise, noiseedge, hermite, frustum, frustum2, draw2d, sprites, simsprites, map, map2, hud, hud2, pi, pi2, pi3, dd3a..dd3e, dd4, dd2, models, objecttypes, textures, landscape, windows, materials. The script sets the environment of the gate itself.
"""
import os
import random
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from synth_bonds import Snapshot  # noqa: E402

HARNESS = str(ROOT / 'build' / 'hook' / 'harness_client.exe')
IMAGE = str(ROOT / 'build' / 'hook' / 'subrosa_fixed.exe')

# Functions (RVA).
SERP, CERP, NOISE1D, NOISE2D = 0xc51b0, 0xc51f0, 0xc5240, 0xc52a0
HERMITE, BEZIER = 0xbc4c0, 0xccca0
NOISE3D, LERPNOISE2D, SERPNOISE2D, CERPNOISE2D = 0xc5300, 0xc5370, 0xc5470, 0xc5570
VERTSEQUAL, ADDVERTEX, ADDBOX = 0xac0d0, 0xb7980, 0xaedb0
EXTRUDE, EXTRUDE2, EXTRUDESCALE = 0xb2c90, 0xb2630, 0xb2e30
SETTRITEXCOORD, SETQUADTEXCOORD = 0xb3150, 0xb31c0
GENERATETEXTURE, SETHEIGHTF = 0xe7ff0, 0xa1aa0
ADDPARTICLE, ADDBOND, ADDFACE, ADDWHEEL, POINTSCALE = 0xeb750, 0xeb810, 0xeb950, 0xebb20, 0xec220
OBJECTTYPEWEIGHT = 0xebe00

# The model in work (RVA): the counts and the arrays that the cases fill.
MODELTEMP = 0x90f7680
NUM_VERTS, VERTS, NUM_TRIS, NUM_QUADS, QUADS = 0x0, 0x8, 0xa0008, 0x20000c, 0x200010
# Free vertices of the model in work, far above the ones a case uses: room for the curve inputs.
SCRATCH = MODELTEMP + VERTS + 7000 * 12

# The list of texture numbers (RVA) and the entries that generatetexture draws: (offset, count).
TEXTURELIST = 0x14311840
SMALL_TEXTURES = [(0x418, 3), (0x424, 4), (0x1034, 8), (0x1054, 10), (0x10d4, 1), (0x10d8, 6), (0x10f0, 3)]

# The texture records (RVA, 0x70 bytes each), the window texture tables and a free area of the image for pixels.
BASE = 0x20000000  # the image address of RVA 0 in the harness
TEXTURES, TEXTURE_SIZE = 0x17b4b840, 0x70
WINDOWTEXTURE, MATERIALTEXTURE, MATERIALLIST = 0xe9df0, 0xe6630, 0xf2033a0
WINDOW_ATLAS, WINDOW_RECTS = 0x103673a0, 0x90558c4
SOURCE_PIXELS = MODELTEMP + 0x240000

# The landscape (RVA): the origin, then 2049 x 2049 heights.
LANDSCAPE = 0x9ba7c20

NAN, INF = float('nan'), float('inf')
EDGES = [0.0, -0.0, 1.0, -1.0, 0.5, 0.25, 0.75, 2.0, 0.001, 0.1, 0.3333333, 0.999999, 123.456, -77.7, 1e10,
         3.4028235e38, -3.4028235e38, 1e-38, 1.4e-45, 1e-41, INF, -INF, NAN]


def bits(value):
    """The 32 bits of a float argument."""
    return struct.unpack('<I', struct.pack('<f', value))[0]


def arg(value):
    if isinstance(value, float):
        return f'V:{bits(value):x}'
    if isinstance(value, tuple):  # a pointer to floats: the harness makes the memory
        raw = struct.pack(f'<{len(value)}f', *value).ljust(256, b'\0')
        return 'P:0:' + raw.hex()
    if isinstance(value, str):  # 'M:<rva>': an address in the image
        return value
    return f'V:{value & 0xffffffff:x}'


def call(rva, *args):
    return f'R {rva:x}' + ''.join(' ' + arg(a) for a in args) + '\nZ r:0\n'


def image(rva):
    return f'M:{rva:x}'


def floats(rng):
    """An edge value, or a random value of a small or a large size."""
    kind = rng.random()
    if kind < 0.35:
        return rng.choice(EDGES)
    if kind < 0.8:
        return rng.uniform(-2.0, 2.0)
    return rng.uniform(-1.0, 1.0) * 10.0 ** rng.randint(-30, 30)


def noise_calls():
    """The calls of the noise case, and the edge points (NaN, infinite, outside an int) that only noise_edge_case uses."""
    rng = random.Random(1)
    ends = [0.0, 1.0, -3.5, 1e-41, 3.4028235e38, NAN]
    blend = [(a, b, t) for a in ends for b in ends[:3] for t in EDGES]
    blend += [(floats(rng), floats(rng), floats(rng)) for _ in range(150)]
    ints = list(range(40)) + [-1, -2, 0x7fffffff, 0x80000000, 0xffff, 0x10000, 57, 15731] + \
        [rng.getrandbits(32) for _ in range(80)]
    pairs = [(x, y) for x in range(-3, 6) for y in range(-3, 6)] + \
        [(rng.getrandbits(32), rng.getrandbits(32)) for _ in range(80)]
    curves = [(degree, floats(rng), floats(rng), floats(rng), floats(rng), floats(rng))
              for degree in (1, 2, 3) for _ in range(80)]
    curves += [(degree, t, 1.0, 2.0, 3.0, 4.0) for degree in (1, 2, 3) for t in EDGES]
    triples = [(x, y, z) for x in range(-2, 3) for y in range(-2, 3) for z in range(-2, 3)] +         [(rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)) for _ in range(40)]
    # (x, y, mask): points of a texture, points on a grid line, negative and large points, and edge values.
    masks = [0x3ff, 0x1f, 0xf, 7, 3, 0xffffffff]
    grid = [(rng.uniform(0.0, 1024.0), rng.uniform(0.0, 1024.0), rng.choice(masks)) for _ in range(120)]
    grid += [(float(x), y * 0.25, m) for x in (0, 1, 31, 32, 1023) for y in (0, 3, 127, 4092) for m in (0x1f, 0x3ff)]
    grid += [(rng.uniform(-70.0, 70.0), rng.uniform(-70.0, 70.0), rng.choice(masks)) for _ in range(40)]
    edge = [(floats(rng), floats(rng), rng.choice(masks)) for _ in range(120)]
    return {
        'serp': [call(SERP, *c) for c in blend],
        'cerp': [call(CERP, *c) for c in blend],
        'noise1d': [call(NOISE1D, n) for n in ints],
        'noise2d': [call(NOISE2D, x, y) for x, y in pairs],
        'engine_bezierinterpolation': [call(BEZIER, *c) for c in curves],
        'noise3d': [call(NOISE3D, *c) for c in triples],
        'lerpnoise2d': [call(LERPNOISE2D, *c) for c in grid],
        'serpnoise2d': [call(SERPNOISE2D, *c) for c in grid],
        'cerpnoise2d': [call(CERPNOISE2D, *c) for c in grid],
    }, edge


def noise_edge_case():
    """The three grid noise functions at edge points. A point that is NaN, infinite or outside the range of an int
    gives another value than the game (the shared code converts with 64 bits). The case shows how many."""
    edge = noise_calls()[1]
    return [], {'lerpnoise2d': [call(LERPNOISE2D, *c) for c in edge], 'serpnoise2d': [call(SERPNOISE2D, *c) for c in edge],
                'cerpnoise2d': [call(CERPNOISE2D, *c) for c in edge]}


def hermite_case():
    """Each slot holds out, start, start tangent, end and end tangent (5 vectors)."""
    rng = random.Random(2)
    edits, calls = [], []
    for slot in range(60):
        base = SCRATCH + slot * 60
        values = [floats(rng) for _ in range(15)] if slot >= 4 else [float(k) for k in range(15)]
        edits.append((base, '15f', values))
        t = [0.0, 1.0, 0.5, 0.25][slot] if slot < 4 else floats(rng)
        vectors = [image(base + k * 12) for k in range(5)]
        if slot == 5:
            vectors[0] = vectors[1]  # out is the start point
        calls.append(call(HERMITE, vectors[0], t, vectors[1], vectors[2], vectors[3], vectors[4]))
    return edits, {'hermiteinterpolation': calls}


def frustum_case(camera_turn=(0.7, 0.3), seed=7):
    """Boxes and points around a camera. The camera sits at the audio camera record: its place and the three rows of its
    rotation. Each slot holds a box (smallest and largest corner) or a point."""
    import math
    rng = random.Random(seed)
    camera = 0xfefe2a0
    yaw, pitch = camera_turn
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    rows = [(cy, 0.0, -sy), (sy * sp, cp, cy * sp), (sy * cp, -sp, cy * cp)]
    edits = [(camera + 4, '3f', [rng.uniform(-50.0, 50.0) for _ in range(3)]),
             (camera + 0x10, '9f', [v for row in rows for v in row])]
    box_calls, point_calls = [], []
    for slot in range(160):
        base = SCRATCH + slot * 60
        near = rng.choice([5.0, 40.0, 400.0])
        low = [rng.uniform(-near, near) for _ in range(3)]
        high = [low[k] + rng.uniform(0.0, near) for k in range(3)]
        point = [rng.uniform(-near, near) for _ in range(3)]
        edits.append((base, '9f', low + high + point))
        box_calls.append(call(0x79890, image(base), image(base + 12)))
        point_calls.append(call(0x796f0, image(base + 24)))
    return edits, {'boxinsidefrustum': box_calls, 'pointinsidefrustum': point_calls}


def draw2d_case():
    """The 2D shapes, the OpenGL states, the debug boxes and the model passes that no recording reaches."""
    rng = random.Random(11)
    edits, calls = [], {name: [] for name in (
        'drawbitmaptexcoord', 'drawlinebox', 'drawcirclefill', 'enabletwosidedstencil', 'glstatenormal',
        'debug_renderboundingbox', 'debug_renderbox', 'rendersphere', 'rendermodelwire', 'rendermodelnormals')}
    for slot in range(60):
        base = SCRATCH + slot * 120
        edits.append((base, '24f', [floats(rng) if slot > 5 else rng.uniform(-2.0, 2.0) for _ in range(24)]))
        x, y, w, h = (rng.uniform(-100.0, 1100.0) for _ in range(4))
        flags = rng.randrange(16)
        color = [rng.uniform(0.0, 1.0) for _ in range(4)]
        calls['drawbitmaptexcoord'].append(call(0x107c40, rng.randrange(1, 8), x, y, w, h, flags, *color, image(base)))
        calls['drawlinebox'].append(call(0x107d80, x, y, w, h, flags, *color))
        calls['drawcirclefill'].append(call(0x6d990, x, y, abs(w) * 0.1, *color))
        calls['debug_renderboundingbox'].append(call(0x6a4d0, image(base), image(base + 12)))
        calls['debug_renderbox'].append(call(0x6a860, image(base), image(base + 12), w, h, x))
        calls['rendersphere'].append(call(0xd6cb0, image(base), abs(w) * 0.01, rng.randrange(8)))
        model = rng.choice([0, 1, 2, 3, 32, 33, 96])
        calls['rendermodelwire'].append(call(0xd7380, model, image(base), image(base + 48), image(base + 96 - 12), x))
        calls['rendermodelnormals'].append(call(0xae8c0, model, image(base), image(base + 48)))
    calls['enabletwosidedstencil'] = [call(0x82a00)] * 3
    calls['glstatenormal'] = [call(0xdc2f0)] * 3
    return edits, calls


def sprites_case():
    """Start, free and age the flying pieces. The place, the speed and the three rows of the
    orientation sit in the scratch area of the snapshot."""
    rng = random.Random(21)
    edits, calls = [], {'createsprite_model': [], 'deletesprite_model': [], 'sprite_modeltimetolive': []}
    for slot in range(60):
        base = SCRATCH + slot * 160
        edits.append((base, '15f', [rng.uniform(-300.0, 300.0) for _ in range(6)] + [rng.uniform(-1.0, 1.0) for _ in range(9)]))
        vel = image(base + 12) if slot % 4 else 0
        calls['createsprite_model'].append(call(0xe5040, rng.randrange(2), image(base), vel, image(base + 24)))
    for index in [-1, 0, 1, 2, 5, 50, 1000, 20000, 0x7fffffff] * 3:
        calls['deletesprite_model'].append(call(0xe5280, index))
    calls['sprite_modeltimetolive'] = [call(0xe5210)] * 3
    return edits, calls


def simsprites_case():
    """Run the simulation of the flying pieces on 120 pieces: shells and debris, high and low, fast and slow."""
    rng = random.Random(33)
    pieces = 120
    pool = 0x5379100
    count = 0x12a7d604
    edits = [(count, 'i', [pieces])]
    for k in range(pieces):
        kind = k % 2
        low = k % 3 == 0
        # Near the two places where the recorded game left pieces on the ground, so that the level data is in the snapshot.
        cx, cz = (641.7, 1084.5) if k % 4 < 2 else (653.5, 1066.0)
        pos = [cx + rng.uniform(-2.0, 2.0), 24.5 + (rng.uniform(0.0, 0.4) if low else rng.uniform(0.5, 4.0)), cz + rng.uniform(-2.0, 2.0)]
        speed = 0.1 if low else 0.3
        vel = [rng.uniform(-speed, speed), -rng.uniform(0.05, speed * 2), rng.uniform(-speed, speed)]
        yaw = rng.uniform(0.0, 6.2831)
        c, s = float(__import__('math').cos(yaw)), float(__import__('math').sin(yaw))
        rot = [c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c]
        spin = [rng.uniform(-0.3, 0.3) for _ in range(3)]
        base = pool + k * 0x68
        edits.append((base, '3i', [kind, 0xffff if k % 5 else 100, 1]))
        edits.append((base + 0xc, '3f', pos))
        edits.append((base + 0x18, '3f', vel))
        edits.append((base + 0x24, '9f', rot))
        edits.append((base + 0x48, '3f', spin))
        edits.append((base + 0x58, '2f', [0.0027777778, 0.9975]))
    return edits, {'sprite_model_simulation': [call(0xe52e0)] * 4}


def map_case(wireframe):
    """Draw the map for humans at the middle, at both clamps and without a human, in a driving game
    with checkpoints, with and without the wide view."""
    game, count, checkpoints, wire, view = 0x9065308, 0x908aaec, 0x908aaf0, 0x75a5140 + 11, 0xfefe2a0
    players, humans = 0xed813a0, 0x14fc4a0
    rng = random.Random(5)
    edits = [(game, 'i', [2]), (count, 'i', [6]), (wire, 'B', [wireframe]), (view, 'i', [3])]
    for k in range(6):
        edits.append((checkpoints + k * 12, '3f', [rng.uniform(0.0, 2000.0), 0.0, rng.uniform(0.0, 2000.0)]))
    teams = [0, 1, 2, 3, 4, 5, -1, 7, 2, 2]
    for k, team in enumerate(teams):
        edits.append((players + k * 0x2408, 'i', [1]))
        edits.append((players + k * 0x2408 + 0x64, 'i', [team]))
        edits.append((players + k * 0x2408 + 0x5c, 'I', [rng.randrange(64)]))
    spots = [(400.0, 20.0, 600.0), (-900.0, 20.0, -900.0), (5000.0, 20.0, 5000.0), (300.0, 20.0, 1100.0)]
    for k, (x, y, z) in enumerate(spots):
        edits.append((humans + k * 0x2b64 + 0x48, '3f', [x, y, z]))
        edits.append((humans + k * 0x2b64 + 0x60, 'f', [rng.uniform(0.0, 6.28)]))
    return edits, {'drawmap': [call(0x84700, who) for who in (0, 1, 2, 3, -1, 0)]}


def hud_case(shift):
    """Draw the display for players with a phone in each call state, a car, a helicopter, no human,
    items in the hands and in the pack, with and without the hand key and the map."""
    players, humans, items, objects = 0xed813a0, 0x14fc4a0, 0x908b080, 0x5519120
    rng = random.Random(9)
    edits = [(0x75a5140 + 0xe1, 'B', [shift]), (0xfefe2d4, 'f', [2.5]), (0x908aae0, 'i', [shift]),
             (0xb77c594, 'i', [0]), (0x908aad8, 'i', [1]), (0x9065320, 'i', [1])]
    # (human, phone state, info, move mode, input mode, bleeding, vehicle, seat)
    setups = [(0, 0, 0, 0, 2, 0, 0, 0), (1, 1, 0, 1, 3, 1, 0, 0), (2, 2, 0, 2, 0, 0, -1, 0),
              (3, 2, 1234, 1, 2, 1, 1, 1), (4, 3, 555, 0, 3, 0, 1, 0), (5, 1, 77, 2, 2, 0, 1, 0)]
    for k, (human, state, info, move, mode, bleeding, vehicle, seat) in enumerate(setups):
        item = 3 + k
        edits.append((items + item * 0x1ac + 0xc, 'i', [0x17]))
        edits.append((items + item * 0x1ac + 0x104, 'i', [info]))
        edits.append((items + item * 0x1ac + 0x110, 'i', [state]))
        base = humans + human * 0x2b64
        edits.append((base + 0x14, 'i', [vehicle]))
        edits.append((base + 0x18, 'i', [seat]))
        edits.append((base + 0x291c, '2i', [1, item]))
        edits.append((base + 0x2b1c, 'i', [bleeding]))
        player = players + k * 0x2408
        edits.append((player, 'i', [1]))
        edits.append((player + 0x78, 'i', [human]))
        edits.append((player + 0xac, 'i', [move]))
        edits.append((player + 0xc4, 'i', [k % 2]))
        edits.append((player + 0xc8, 'i', [mode]))
        edits.append((player + 0xd4, 'i', [k % 3]))
        edits.append((player + 0x7c, '4f', [rng.uniform(-1.0, 1.0) for _ in range(4)]))
        edits.append((player + 0x9c, 'I', [0x10 if k == 2 else 0]))
        edits.append((player + 0x15ec, 'i', [rng.randrange(0, 200)]))
        for slot in range(7):
            count = rng.randrange(0, 4)
            codes = [rng.choice([0, 1, 2, 12, 13, 20]) | (rng.randrange(-1, 31) << 8) & 0xffffff00 for _ in range(8)]
            edits.append((player + 0x15f8 + slot * 0x24, '9i', [count] + [c - (1 << 32) if c >= 1 << 31 else c for c in codes[:8]]))
    for vehicle in (0, 1):
        edits.append((objects + vehicle * 0x8230 + 0x60, '3f', [rng.uniform(-2.0, 2.0), 0.0, rng.uniform(-2.0, 2.0)]))
        edits.append((objects + vehicle * 0x8230 + 0x5ee0, 'i', [rng.randrange(0, 5000)]))
    # a player with no human
    edits.append((players + 6 * 0x2408, 'i', [1]))
    edits.append((players + 6 * 0x2408 + 0x78, 'i', [-1]))
    return edits, {'drawhud': [call(0x830f0, who) for who in (0, 1, 2, 3, 4, 5, 6, -1)]}


def playerinfo_case(state):
    """List 24 players in every column, with bots, players that are not ready, stocks that rose, fell
    and split, in a game state."""
    players, market, game_state = 0xed813a0, 0x17678bc0, 0x9065320
    rng = random.Random(14)
    edits = [(game_state, 'i', [state])]
    for team in range(8):
        price = [100, 250, 40, 0, 1, 500, 7, 3][team]
        opened = [100.0, 100.0, 100.0, 0.0, 50.0, 500.0, 7.0, 3.0][team]
        edits.append((market + team * 0x14 + 4, '2f', [float(price), opened]))
        edits.append((market + team * 0x14 + 0xc, 'i', [price]))
    for k in range(24):
        base = players + k * 0x2408
        team = [0, 1, 2, 3, 7, -1][k % 6]
        name = ('Player%02d' % k).encode().ljust(32, b'\0')
        edits.append((base, 'i', [1]))
        edits.append((base + 4, '32s', [name]))
        edits.append((base + 0x3c, 'i', [k % 3]))
        edits.append((base + 0x40, 'i', [rng.randrange(0, 200000)]))
        edits.append((base + 0x64, 'i', [team]))
        edits.append((base + 0x6c, 'i', [rng.randrange(0, 2000)]))
        edits.append((base + 0x1a38, 'i', [1 if k % 11 == 10 else 0]))
    return edits, {'drawplayerinfo': [call(0x6dcb0)] * 2}


def display_case(gametype, state, round_mode, debugit=0):
    """Draw the display of the game with the stock page, a mission of every kind, the timers of the
    game types, the player list and the chat lines."""
    game, view, players, humans, market = 0x9065300, 0xfefe2a0, 0xed813a0, 0x14fc4a0, 0x17678bc0
    rng = random.Random(gametype * 10 + state)
    edits = [(game + 8, 'i', [gametype]), (game + 0xc, 'i', [round_mode]), (game + 0x20, 'i', [state]),
             (game + 0x24, 'i', [rng.randrange(0, 200000)]), (0x908aae0, 'i', [1]), (0x908ac70, 'i', [rng.randrange(0, 90000)]),
             (0x908ac74, 'i', [rng.randrange(0, 50)]), (view, 'i', [0]), (0x127ea8, 'i', [debugit]),
             (0x102feb48, 'i', [debugit]), (0x29e7d320, 'i', [rng.randrange(0, 5000000)])]
    for team in range(8):
        edits.append((market + team * 0x14 + 0xc, 'i', [rng.randrange(1, 900)]))
    edits.append((players, 'i', [1]))
    edits.append((players + 0x64, 'i', [state % 4]))
    edits.append((players + 0x40, 'i', [rng.randrange(0, 90000)]))
    edits.append((players + 0x6c, 'i', [rng.randrange(0, 900)]))
    edits.append((players + 0x78, 'i', [0]))
    edits.append((players + 0xd4, 'i', [2]))
    edits.append((humans + 0x28, 'i', [100]))
    team_base = game + 0xcc + (state % 4) * 0x117c
    for k in range(16):
        base = team_base + k * 0x40
        edits.append((base, 'i', [1 if k < 9 else 0]))
        edits.append((base + 8, 'i', [k % 7]))
        edits.append((base + 0x10, 'i', [(k + 1) % 5]))
        edits.append((base + 0x20, 'i', [k % 10]))
        edits.append((base + 0x24, 'i', [rng.randrange(1000, 90000)]))
        edits.append((base + 0x28, 'i', [rng.randrange(0, 4000000)]))
        edits.append((base + 0x2c, 'i', [-1 if k % 4 == 0 else rng.randrange(0, 50)]))
        edits.append((base + 0x30, 'i', [rng.randrange(0, 50000)]))
    chat = 0x129660
    edits.append((0xf3eafc0, 'I', [40]))
    for k in range(16):
        base = chat + (30 + k) * 0x98
        text = ('Line %d of the chat' % k).encode().ljust(128, b'\0')
        edits.append((base, '6i', [k % 5, 0, rng.randrange(-5, 300), rng.randrange(-5, 300), rng.choice([-1, 0]), 0]))
        edits.append((base + 0x18, '128s', [text]))
    return edits, {'drawdisplay': [call(0x6baa0)] * 2}


def models_case():
    tiny = 0.0001
    verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
             (0.3, -1.7, 2.25), (4.125, -1.5, 2.0), (4.0, 3.3, 1.75), (0.1, 3.0, 2.6),
             (tiny * 0.5, 0, 0), (tiny, 0, 0), (tiny * 0.999, tiny * 0.999, -tiny * 0.999), (NAN, 0, 0),
             (1, -tiny * 0.9, 0), (1 + 1e-6, 1 - 1e-6, 1e-6), (1e30, -1e30, 1e-30), (-0.0, 0, tiny * 1.001)]
    edits = [(MODELTEMP + NUM_VERTS, 'i', [len(verts)]), (MODELTEMP + NUM_TRIS, 'i', [0]),
             (MODELTEMP + NUM_QUADS, 'i', [2]), (MODELTEMP + QUADS, '8i', list(range(8)))]
    for k, v in enumerate(verts):
        edits.append((MODELTEMP + VERTS + k * 12, '3f', [float(x) for x in v]))
    n = len(verts)
    points = [((1.0, 1.0, 0.0), tiny), ((1.00005, 1.0, 0.0), tiny), ((1.00005, 1.0, 0.0), 0.001),
              ((9.0, 8.0, 7.0), tiny), ((9.0, 8.0, 7.0), tiny), ((0.0, 0.0, 0.0), 0.0), ((0.0, 0.0, 0.0), -1.0),
              ((0.5, 0.5, 0.5), INF), ((0.5, 0.5, 0.5), NAN), ((NAN, 0.0, 0.0), 1.0), ((1e30, -1e30, 0.0), 1e25)]
    boxes = [((0.0, 0.0, 0.0), 1.0, 1.0, 1.0), ((1.5, -2.25, 3.0), 0.5, 0.125, 2.0),
             ((0.1, 0.2, 0.3), 0.0, -1.0, 1e-3), ((1e20, 1.0, -1e-20), 1.0, 1e20, 1e-20), ((NAN, 0.0, 1.0), 1.0, INF, 0.0)]
    offsets = [(0.0, 0.0, 1.0), (0.1, -0.2, 0.3), (1e10, 1e-10, -3.0)]
    scales = [(1.0, 1.0, 1.0), (0.5, 2.0, 0.875), (-1.0, 0.0, 1e10)]
    coords = [(0, 0.0, 0.0), (1, 0.25, 0.75), (5, -1.5, 1e10), (7, 1e-40, 3.0)]
    return edits, {
        'modeltempvertsequal': [call(VERTSEQUAL, a, b) for a in range(n) for b in range(n)],
        'modeltemp_addvertex': [call(ADDVERTEX, p, t) for p, t in points],
        'addboxtomodel': [call(ADDBOX, *b) for b in boxes],
        'model_extrudequad': [call(EXTRUDE, q, o) for q in (0, 1, 2) for o in offsets],
        'model_extrudequad2': [call(EXTRUDE2, q) for q in (0, 1, 0, 2, 5)],
        'model_extrudequadscale': [call(EXTRUDESCALE, q, o, *s) for q in (0, 1, 3) for o, s in zip(offsets, scales)],
        'model_settritexcoord': [call(SETTRITEXCOORD, *c) for c in coords],
        'model_setquadtexcoord': [call(SETQUADTEXCOORD, *c) for c in coords],
    }


def objecttypes_case():
    """Types 100 and 101 are empty. Type 0 is a car with particles."""
    faces = [(100, 4, 0, 1, 2, 3, 1.0, 8.0), (100, 3, 4, 5, 6, 0, 1.0, 256.0), (101, 4, 9, 8, 7, 6, 0.5, 0.0),
             (100, 4, 2, 3, 4, 5, 1.0, 1.0)]
    bonds = [(100, 1, 3, 4), (100, 1, 3, 4), (100, 2, 4, 3), (100, 0, 5, 5), (100, 0, 5, 5), (100, 3, 0, 1),
             (101, 1, 3, 4), (100, 1, 1, 0)]
    wheels = [(0, 0, 0, 1, (0.5, 0.5), 0.375, 20.0, 1.5, -0.25, 100.0, 10.0, 0.5),
              (0, 1, 2, 5, (0.25, 0.75), 0.5, 1.0, 0.0, 0.125, 0.0, 0.0, 1.0),
              (0, 0, 3, 3, (1.0, 0.0), 1e-3, 1e3, 2.0, 0.0, 1.0, 1.0, 0.0)]
    scale = [call(POINTSCALE, image(SCRATCH), count, image(MODELTEMP + VERTS + first * 12), target)
             for count, first in ((4, 0), (5, 3), (6, 20), (8, 40)) for target in ((0.5, 0.5, 2.0), (3.0, -1.0, 0.25))]
    return [], {
        'objecttype_addparticle': [call(ADDPARTICLE, t, x, y, z, m) for t, x, y, z, m in
                                   ((100, 1.0, 2.0, 3.0, 0.5), (100, -1.5, 0.0, 1e-3, 2.0), (101, 0.0, 0.0, 0.0, 0.0),
                                    (100, 1e10, -1e10, 0.25, 1.0))],
        'objecttype_addbond': [call(ADDBOND, *b) for b in bonds],
        'objecttype_addface': [call(ADDFACE, *f) for f in faces],
        'objecttype_addwheel': [call(ADDWHEEL, *w) for w in wheels],
        'calculatepointscale': scale,
        'setupobjecttypeweight': [call(OBJECTTYPEWEIGHT, t) for t in range(24)],
    }


def textures_case(rec):
    """Draw each small texture of the recording once, with the arguments of the start code."""
    snap = Snapshot(rec / 'sv_snapshot.bin')
    numbers = []
    for offset, count in SMALL_TEXTURES:
        numbers += snap.read(TEXTURELIST + offset, f'{count}i')
    return [], {'generatetexture': [call(GENERATETEXTURE, n, 1, 0x812f, 0x812f, 0x2600, 0x2600) for n in numbers]}


def landscape_case(rec):
    """Set heights at grid points inside the grid, on its borders, outside it and at edge values."""
    rng = random.Random(3)
    ox, _, oz = Snapshot(rec / 'sv_snapshot.bin').read(LANDSCAPE, '3f')
    cells = [(0, 0), (5, 7), (2047, 2047), (2048, 10), (10, 2048), (-1, 3), (3, -1), (2047, 0), (0, 2047),
             (0.999, 0.001), (-0.5, -0.5), (2047.99, 2047.99), (1e6, 3), (3, -1e6)]
    cells += [(rng.uniform(-100.0, 2150.0), rng.uniform(-100.0, 2150.0)) for _ in range(60)]
    points = [(ox + x * 8.0, rng.uniform(-50.0, 50.0), oz + z * 8.0) for x, z in cells]
    points += [(a, 0.0, b) for a in (NAN, INF, -INF, 3e9, -3e9, 1e30) for b in (oz + 80.0, NAN)]
    points += [(ox + 80.0, 0.0, b) for b in (NAN, INF, -INF, 3e9, -3e9, 1e30)]
    return [], {'landscape_setheightf': [call(SETHEIGHTF, p, floats(rng)) for p in points]}


def materials_case(rec):
    """Make the texture of every material of the list once, so each picture of the generator runs."""
    snap = Snapshot(rec / 'sv_snapshot.bin')
    count = min(snap.read(MATERIALLIST, 'i')[0], 256)
    return [], {'generatematerialtexture': [
        call(MATERIALTEXTURE, m, m & 1, 0x2901, 0x2901, 0x2601, 0x2703) for m in range(count)]}


def windows_case(rec):
    """Make the window texture from made sources: the snapshot holds no pixels for the textures
    of the recording, so the case puts the second level of 4 material textures in the image."""
    rng = random.Random(5)
    snap = Snapshot(rec / 'sv_snapshot.bin')
    material = lambda m: snap.read(TEXTURELIST + 4 + 4 * m, 'i')[0]  # noqa: E731
    sources = [1, 2, 3, 4]
    edits = []
    for i, m in enumerate(sources):
        pixels = SOURCE_PIXELS + i * 0x4000
        words = [rng.getrandbits(32) for _ in range(64 * 64)]
        edits.append((pixels, f'{len(words)}I', words))
        record = TEXTURES + material(m) * TEXTURE_SIZE
        edits.append((record, 'i', [128]))
        edits.append((record + 0x34, 'I', [pixels + BASE]))
    rects = []
    for i in range(8):
        left, width, top, height = (rng.choice([0.0, 0.125, 0.25, 0.5, 0.0625]) for _ in range(4)) if i else (0.25, 0.5, 0.25, 0.5)
        rects += [i % 2, bits(left), bits(width), bits(top), bits(height), 0]
    edits.append((WINDOW_RECTS, '48I', rects))
    cells = [(sources[i % 4], i % 8) for i in range(40)]
    edits.append((WINDOW_ATLAS, 'i', [len(cells)]))
    edits.append((WINDOW_ATLAS + 4, f'{2 * len(cells)}i', [v for c in cells for v in c]))
    return edits, {'generatematerialwindowtexture': [
        call(WINDOWTEXTURE, m, mip, 0x812f, 0x812f, 0x2601, 0x2703) for m in (5, 6) for mip in (1, 0)]}


def run(snapshot, args, name, env):
    p = subprocess.run([HARNESS, IMAGE, str(snapshot), str(args), name], capture_output=True, text=True,
                       errors='replace', timeout=3600, env=env, cwd=ROOT)
    if os.environ.get('HARNESS_DEBUG'):
        print(p.stdout[-3000:])
        print(p.stderr[-3000:])
    for line in p.stdout.splitlines()[::-1]:
        if 'functions tested' in line:
            return line.split(':', 1)[1].strip()
    return 'no result: ' + p.stdout[-300:].replace('\n', ' ')


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build' / 'client_test'),
               HARNESS_ENV_DIR=str(work / 'env'), SV_UI_WIDTH='220')
    env['PATH'] = str(ROOT / 'build' / 'client_deps') + os.pathsep + env.get('PATH', '')
    allc = {'noise': lambda: ([], noise_calls()[0]), 'noiseedge': noise_edge_case, 'hermite': hermite_case, 'frustum': frustum_case, 'frustum2': lambda: frustum_case((0.0, 0.0), 8), 'draw2d': draw2d_case, 'sprites': sprites_case, 'simsprites': simsprites_case, 'map': lambda: map_case(0), 'map2': lambda: map_case(1), 'hud': lambda: hud_case(0), 'hud2': lambda: hud_case(1), 'pi': lambda: playerinfo_case(3), 'pi2': lambda: playerinfo_case(0), 'pi3': lambda: playerinfo_case(1), 'dd3a': lambda: display_case(3, 0, 0), 'dd3b': lambda: display_case(3, 1, 3), 'dd3c': lambda: display_case(3, 2, 3), 'dd3d': lambda: display_case(3, 2, 0), 'dd3e': lambda: display_case(3, 3, 3, 1), 'dd4': lambda: display_case(4, 1, 0, 1), 'dd2': lambda: display_case(2, 2, 0), 'models': models_case,
            'objecttypes': objecttypes_case, 'textures': lambda: textures_case(rec),
            'landscape': lambda: landscape_case(rec), 'windows': lambda: windows_case(rec),
            'materials': lambda: materials_case(rec)}
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
