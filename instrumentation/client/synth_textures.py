"""Run the noise, curve, model and object type functions of the client on made inputs.

No recording calls these functions. A case writes a few values into the snapshot of a recording
(or uses it as it is) and calls each function many times. The harness compares the clean function
with the original: the result and the memory. A case snapshot is about 350 MB, so the script
deletes it after the run.

Usage: python -I instrumentation/client/synth_textures.py <recording dir> <work dir> [case,...]
Cases: noise, noiseedge, hermite, frustum, frustum2, draw2d, sprites, simsprites, map, map2, hud, hud2, pi, pi2, pi3, dd3a..dd3e, dd4, dd2, models, objecttypes, textures, landscape, windows, materials. The script sets the environment of the gate itself.
Set SV_SYNTH_REPEAT to repeat each call list for timing samples.
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
LOADTEXTUREPARTIAL, LOADED_IMAGE, IMAGE_NAME = 0x10a7c0, 0x19da5220, 0xb733140
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


def effects_case(mode=0):
    """Exercise particles, shards, tire strips and all four checkpoint sides."""
    rng = random.Random(91)
    identity = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    edits = [(0x75a5120, 'i', [36]), (0x75a58a0, 'i', [16000 if mode == 4 else 0]),
             (0x75f58a4, 'i', [16000 if mode == 4 else 0]),
             (0xb73312c, 'I', [0x3fff if mode else 0]),
             (0xb77c580, 'I', [0x3fff if mode else 0]),
             (0xfefe2a0, 'i', [0]), (0xfefe2b0, '9f', identity),
             (0x908ac70, 'ii', [mode, 4]), (0xed81400, 'i', [2])]
    for index in range(36):
        base = 0xf9ae260 + index * 0x94
        height = 25.0 if mode >= 3 else 24.5 if mode else 30.0
        velocity = [1.0, -0.75, 0.5] if mode >= 3 else [0.5 if mode == 2 else 0.03, -0.1, 0.25 if mode == 2 else 0.01]
        rotation = identity
        corner_velocity = [0.01, -0.02, 0.03] * 3
        spin = [0.0, 1.0, 0.0]
        if mode >= 3 and index % 6 == 4:
            rotation = [641.7, 25.0, 1084.5, 642.7, 25.0, 1084.5, 641.7, 25.0, 1085.5]
            corner_velocity = [1.0, -0.75, 0.5, 0.5, -0.75, 0.25, 0.25, -0.75, 0.5]
            spin = [1.0, 1.4142135623730951, 1.0]
        edits += [(base, '148s', [bytes(148)]),
                  (base, 'iif', [index % 6, 60000 if index % 6 == 4 else 10 + index, 0.0027222223579883575]),
                  (base + 0xc, '3f', [641.7 + rng.uniform(-1, 1), height, 1084.5]),
                  (base + 0x18, '3f', velocity),
                  (base + 0x30, '9f', rotation), (base + 0x54, '9f', corner_velocity),
                  (base + 0x78, '6f', spin + [0.1, 0.9975, 0.0625])]
    for index in range(4):
        edits.append((0x908ac78 + index * 0x1c, '6fi', [640.0 + index, 24.0, 1080.0, 642.0 + index, 26.0, 1082.0, index]))
    for index in range(1024):
        edits.append((0x5519120 + index * 0x8230, 'i', [int(index == 0)]))
        edits.append((0x908b080 + index * 0x1ac, 'i', [0]))
    edits += [(0x551f008, 'i', [2])]
    for index in range(2):
        wheel = 0x551f00c + index * 0x98
        edits += [(wheel + 0x38, '3f', [641.7 + index, 24.5, 1084.5]),
                  (wheel + 0x70, 'f3ff', [0.125 if mode else 0.03125, 641.6 + index, 24.5, 1084.4, 0.0625])]
    return edits, {
        'spritesimulation': [call(0xe2790)] * 3,
        'rendersprites': [call(0xe3b10)] * 3,
        'tiremark_simulation': [call(0xe5f80)] * 3,
        'tiremark_render': [call(0xe64a0)] * 3,
        'rendercheckpoints': [call(0xd7470)] * 3,
    }


def glass_case(full=False):
    """Break two window faces with an empty or nearly full particle pool."""
    edits = [(0x75a5120, 'i', [16380 if full else 0]),
             (0x5519124, 'i', [0]), (0x17c040a4, 'i', [0]),
             (0x5519144, '3f', [641.7, 28.0, 1084.5]),
             (0x551915c, '9f', [1, 0, 0, 0, 1, 0, 0, 0, 1]),
             (0x5519180, '3f', [0.01, 0.02, 0.03]),
             (0xaff06a0, 'i', [2]),
             (0xaff06a4, '12fi', [0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 7]),
             (0xaff06d8, '12fi', [0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 8]),
             (SCRATCH, '6f', [642.0, 28.25, 1084.5, 0.1, -0.2, 0.3])]
    return edits, {'createwindowsprites': [call(0xe1ac0, 0, face, image(SCRATCH), image(SCRATCH + 12)) for face in (7, 8, -1)]}


def muzzle_case():
    """Draw flashes with both axial scale factors and several countdown values."""
    vertices = SCRATCH + 512
    triangles = vertices + 128
    edits = [(0x9065260, 'iIiI', [4, BASE + vertices, 2, BASE + triangles]),
             (vertices, '12f', [0, 0, 0, 0.125, 0, 1, 0, 0.125, -0.5, -0.125, 0, -1]),
             (triangles, '6i', [0, 1, 2, 1, 2, 3])]
    for index in range(1024):
        edits.append((0x908b080 + index * 0x1ac, 'i', [int(index < 4)]))
    for index, life in enumerate((0, 1, 4, 16)):
        base = 0x908b080 + index * 0x1ac
        edits += [(base + 0xc, 'i', [index]),
                  (base + 0x48, '3f', [10.0 + index, 2.0, -3.0]),
                  (base + 0x84, '9f', [1, 0, 0, 0, 1, 0, 0, 0, 1]),
                  (base + 0xf4, 'i', [life]),
                  (0x1034dbd0 + index * 0x198, 'f', [-0.625])]
    return edits, {'rendermuzzleflashes': [call(0xe4bc0)] * 3}


def pickup_case(mode=0):
    """Select the nearest free item, or leave the selection when no player is in view."""
    edits = [(0xfefe2a0, 'i', [-1 if mode == 2 else 0]),
             (0xed81418, 'i', [0]), (0xed8143c, 'I', [0x10 if mode else 0]),
             (0x14fedbc, 'i', [0]), (0x14fede0, 'i', [1 if mode else 0]),
             (0x14fc728, '3f', [10.0, 2.0, 3.0]),
             (0x14fc770, '3f', [0.0, 0.0, 1.0])]
    for index in range(1024):
        base = 0x908b080 + index * 0x1ac
        edits.append((base, 'i', [int(index < 4)]))
        if index < 4:
            edits += [(base + 0x18, 'ii', [-1, index - 1 if index == 3 else -1]),
                      (base + 0x48, '3f', [10.2 - index * 0.05, 2.0, 2.0])]
    return edits, {'selectnearitem': [call(0x84f00)] * 3}


def landed_case():
    """Draw two tire strips across the ring boundary and four vertices in each landed list."""
    edits = [(0x75a5120, 'i', [0]), (0x75a58a0, 'i', [4]), (0x75f58a4, 'i', [4]),
             (0xb73312c, 'I', [1]), (0xb77c580, 'I', [0x3fff])]
    for corner, (x, z) in enumerate(((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))):
        edits += [(0x75a58a4 + corner * 20, '5f', [x, 24.0, z, x, z]),
                  (0x75f58a8 + corner * 24, '6f', [x, 24.0, z, 0.0, 1.0, 0.0])]
        for strip in (0x3fff, 0):
            edits.append((0x1036c5e4 + strip * 68 + corner * 16, '4f', [x + strip, 24.0, z, (corner + 1) * 0.25]))
    return edits, {'tiremark_render': [call(0xe64a0)] * 3, 'rendersprites': [call(0xe3b10)] * 3}


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


def sector_shadow_case(light_y=0.75, buffers=0, ready=0):
    """Build side faces and caps, with both buffer paths and ordered light tests."""
    sectors, scratch = 0x1dda5280, MODELTEMP + 0x240000
    edits = [(sectors + i * 0x6068, 'i', [0]) for i in range(8192)]
    edits += [(0x19c0cd78, '3f', [0.25, light_y, -0.5]),
              (0x29e7d2d8, 'i', [buffers]), (0xfefe284, 'i', [ready]),
              (0x1033fd28, 'i', [0]), (0x1b7b4e0, 'I', [237]),
              (0xfbfe280, 'i', [6]), (0x17265640, 'f', [0.75])]
    vertices, triangles, normals, edges = scratch, scratch + 0x200, scratch + 0x300, scratch + 0x400
    for i, point in enumerate(((0.1, 1.7, -2.1), (3.3, 1.25, 0.4), (-1.2, 1.5, 2.7), (4.0, -0.0, 0.25))):
        edits.append((vertices + 64 * i, '3f', point))
    edits += [(triangles, '9i', [0, 1, 2, 1, 2, 3, 3, 0, 1]),
              (normals, '9f', [0.0, -1.0, 0.0, 0.0, 1.0, 0.0, NAN, 0.0, 0.0]),
              (edges, '27f', [0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0] * 3)]
    for index, height in ((0, 0), (3, 7)):
        sector = sectors + index * 0x6068
        edits += [(sector, 'i', [1]), (sector + 0x5014, '4i', [0, 2, height, 4]),
                  (sector + 0x502c, 'I', [BASE + vertices]), (sector + 0x5030, 'i', [3]),
                  (sector + 0x5034, 'I', [BASE + triangles]), (sector + 0x503c, 'I', [BASE + normals]),
                  (sector + 0x5044, 'I', [BASE + edges])]
    return edits, {'render_buildareasectorshadow': [call(0xd8290)] * 3,
                   'render_areasectorshadow': [call(0xd8ae0)] * 3}


def frame_models_case(debug=0):
    """Draw sorted entries with two strides, shared color textures and all render modes."""
    entries, models, scratch = 0x19c110b8, 0xaff05a0, MODELTEMP + 0x240000
    edits = [(0x14a8688, 'i', [debug]), (0x19c0d0b4, 'i', [4]),
             (0x19c0d0b8, '4i', [2, 0, 3, 1]), (0x19c0cd78, '4f', [0.2, 0.75, -0.5, 1.0])]
    for index, stride in ((0, 8), (1, 12)):
        model = models + index * 0xe04
        edits += [(model + 4, 'i', [stride]), (model + 0x18, 'I', [BASE + scratch + index * 0x1000]),
                  (model + 0x1c, 'i', [2])]
        data = [float((i * 7) % 17) / 13.0 for i in range(stride * 6)]
        edits.append((scratch + index * 0x1000, f'{len(data)}f', data))
    for i in range(4):
        entry = entries + i * 100
        edits += [(entry, 'i', [i % 2]), (entry + 8, '3i', [2 if i == 1 else 0, 1 if i < 3 else 2, 3 + i]),
                  (entry + 0x1c, '3f', [float(i), 2.5, -3.75]),
                  (entry + 0x28, '9f', [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]),
                  (entry + 0x58, '3f', [0.1 * i, 0.5, 0.75])]
    return edits, {'rendermodellist': [call(0xd6e40, mode) for mode in (0, 1, 2, -1)],
                   'frame_rendermessages': [call(0x78c30)] * 3}


def human_model_case():
    """Skin six corners through bones and paired part offsets and scales."""
    human, model, refs = 0x14fc4a0, 0xaff05a0, 0x1bdbe04
    scratch = MODELTEMP + 0x240000
    rng = random.Random(31)
    edits = [(model + 4, 'i', [12]), (model + 0x18, 'I', [BASE + scratch]),
             (model + 0x1c, 'i', [2]), (model + 0x24, 'I', [BASE + scratch + 0x1000])]
    for i in range(16):
        bone = human + 0xc8 + i * 0xe0
        edits += [(bone, '3f', [rng.uniform(-5, 5) for _ in range(3)]),
                  (bone + 0x30, '9f', [rng.uniform(-1, 1) for _ in range(9)])]
    edits.append((human + 0x2a5c, '12f', [0.125, -0.375, 0.3, 1.5, 0.25, 100.0, -2.0, 0.75, 1.25, 0.5, -1.25, 0.0]))
    for i, bone in enumerate((0, 3, 15, 16, 17, 16)):
        edits += [(refs + i * 28, '2i', [bone, i % 4]),
                  (refs + i * 28 + 0x10, '3f', [rng.uniform(-2, 2) for _ in range(3)])]
    alternate = []
    for address, fmt, values in edits:
        if human <= address < human + 0x2b64:
            copied = list(values)
            if fmt == '3f':
                copied[0] += 2.75
                copied[1] -= 1.5
            alternate.append((address + 3 * 0x2b64, fmt, copied))
    edits += alternate
    alternate_model = model + 17 * 0xe04
    edits += [(alternate_model + 4, 'i', [8]),
              (alternate_model + 0x18, 'I', [BASE + scratch + 0x2000]),
              (alternate_model + 0x1c, 'i', [2]),
              (alternate_model + 0x24, 'I', [BASE + scratch + 0x3000])]
    return edits, {'buildhumanmodel': [call(0xbc5c0, human_index, model_index)
                                      for human_index, model_index in ((0, 0), (3, 17), (3, 0))]}


def video_modes_case(duplicate=False):
    """Append a display mode, or replace a stored height without replacing its depth."""
    edits = [(0x102feb44, 'i', [2 if duplicate else 0])]
    if duplicate:
        edits.append((0x52ceb80, '6i', [800, 600, 16, 1600, 720, 24]))
    return edits, {'listvideomodes': [call(0x1061e0)] * 3}


def screenshot_case(width=3, height=2):
    """Write a small TGA, an empty TGA, or reject a size above the pixel buffer limit."""
    pixels = [0xff102030, 0x80405060, 0x00708090, 0xffabcdef, 0x00112233, 0x44556677]
    edits = [(0xb7330c0, '2i', [width, height]), (0x13e61840, '6I', pixels)]
    return edits, {'screenshot': [call(0x106790)] * 3}



def depth_case(framebuffer=0, occlusion=0, debug=0):
    """Select depth attachments, stencil shadows and both blur directions."""
    edits = [(0x90631e8, 'i', [framebuffer]), (0x9063200, 'i', [occlusion]),
             (0x127ea8, 'i', [debug]), (0x29e752a0, 'i', [0]), (0x75a514e, 'B', [debug]),
             (0x14a8688, 'i', [0]), (0x19c04c60, 'i', [0]), (0x19c0d0b4, 'i', [0]),
             (0x12a7d604, 'i', [0]), (0x76558a8, 'i', [0]), (0x7e558ac, 'i', [0]),
             (0xfbfe280, 'i', [0]), (0x147129ac, 'i', [0]), (0x1033fd30, 'i', [0]),
             (0x1033fd28, 'i', [0]), (0x1033fd24, 'i', [0]), (0x29e7d2d8, 'i', [0]),
             (0x17c04860, '128i', [0] * 128), (0xb7330c0, '2i', [1024, 640]),
             (0x19c07cc8, '2f', [1024.0, 640.0]),
             (0x19c750dc, '8f', [0.125, -0.375, 0.75, 2.25, -1.25, 0.3, 100.0, 0.0]),
             (TEXTURELIST + 0x404, '4i', [0, 1, 2, 3]), (TEXTURELIST + 0x1110, 'i', [4])]
    for i in range(1024):
        edits.append((0x19c07d74 + i * 20, 'i', [10]))
    for i in range(5):
        edits.append((TEXTURES + i * TEXTURE_SIZE + 0x2c, 'I', [500 + i]))
    return edits, {'renderframedepth': [call(0x77c70)] * 3}


def depth_scene_case(world_view=0):
    """Draw frame entries and flying pieces, including hidden shadow casters."""
    edits, _ = depth_case(1, 1)
    entries, _ = frame_models_case()
    edits += entries
    edits += [(0x14a8688, 'i', [world_view]), (0xb7330c0, '2i', [1600, 900]),
              (0x19c07cc8, '2f', [800.75, 450.25]),
              (0x19c110b8 + 100, 'i', [2]), (0x19c110b8 + 108, 'I', [3]),
              (0x19c110b8 + 200, 'i', [3]), (0x19c110b8 + 208, 'I', [1]),
              (0x12a7d604, 'i', [2]), (0x17c046a4, 'i', [2]), (0x17c047b4, 'i', [3])]
    empty = MODELTEMP + 0x248000
    edits.append((empty, '512s', [bytes(512)]))
    for index in (2, 3):
        model = 0xaff05a0 + index * 0xe04
        edits += [(model, '52s', [bytes(52)]), (model + 4, 'i', [8])]
        for offset in (8, 12, 16, 20, 24, 32, 36, 40, 48):
            edits.append((model + offset, 'I', [BASE + empty]))
    for index in range(2):
        piece = 0x5379100 + index * 0x68
        edits += [(piece, '3i', [index, 80, 1]),
                  (piece + 12, '3f', [float(index), 1.25, -3.75]),
                  (piece + 0x24, '9f', [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0])]
    return edits, {'renderframedepth': [call(0x77c70)] * 3}


def debug_display_case(mode=0, server=0):
    """Draw every debug page with named values, timers, players and network counters."""
    players, debug, timers = 0xed813a0, 0x14a8680, 0x1332e80
    edits = [(debug + 4, 'i', [mode]), (0x14f87a4, 'f', [59.875]),
             (0x14f88ac, 'f', [16.625]), (0x14f88b0, 'i', [999]),
             (0x14f88b4, '16i', [i * 271 - 17 for i in range(16)]),
             (debug + 0x50c60, '3i', [1, -1, 2]), (0x102feb48, 'i', [1]),
             (0x12a7d620, 'i', [68]), (0x75a5800, 'i', [server]),
             (0x75a580c, '2i', [347, 2047]), (0x1b9b4e0, 'i', [3]),
             (0x906531c, 'i', [23]), (0x1dda5244, 'i', [22]),
             (0xf3ca9b8, 'I', [1234567]), (debug + 0x50344, 'i', [317]),
             (0x16338614, 'i', [-430])]
    for i in range(256):
        edits += [(players + i * 0x2408, 'i', [1 if i < 6 else 0]),
                  (players + i * 0x2408 + 0x1a38, 'i', [i % 3 == 0])]
        if i < 6:
            edits.append((players + i * 0x2408 + 4, '32s', [f'Debug player {i}'.encode().ljust(32, b'\0')]))
    edits.append((debug + 0x50c80, 'i', [4]))
    for i, (kind, integer, value) in enumerate(((0, -123, 0.0), (1, 0, 0.1234567),
                                              (1, 0, -17.375), (2, 99, 0.5))):
        record = debug + 0x50c84 + i * 44
        edits += [(record, '2if', [kind, integer, value]),
                  (record + 12, '32s', [f'Debug value {i}'.encode().ljust(32, b'\0')])]
    for i in list(range(16)) + list(range(64, 68)):
        timer = timers + i * 88
        edits += [(timer, '64s', [f'Debug timer {i}'.encode().ljust(64, b'\0')]),
                  (timer + 0x48, '3I', [12345 * (i + 1), i % 3, i * 17])]
    for i in range(8):
        edits.append((0xec38140 + i * 0x2924c, 'i', [i * 19]))
    return edits, {'displaydebuginfo': [call(0x69440)] * 3}


def debug_float_case(negative=False):
    """Draw fractional values with zero, infinity and NaN inputs."""
    edits, functions = debug_display_case(1)
    values = (-INF, -0.0) if negative else (NAN, INF)
    edits += [(0x14f87a4, 'f', [values[0]]), (0x14f88ac, 'f', [values[1]])]
    for index, value in enumerate(values, 1):
        edits.append((0x14a8680 + 0x50c84 + index * 44 + 8, 'f', [value]))
    return edits, functions


def frame_case(framebuffer=0, occlusion=0, display=0, world=0, shadow_key=0, features=()):
    """Draw a whole frame with an empty scene, then turn on single features of the passes.

    The camera moves far from the city, so the sector list stays empty. Humans, objects and
    items are off, so the frame list holds no model."""
    edits, _ = depth_case(framebuffer, occlusion, shadow_key)
    humans, objects, items, cityobjects = 0x14fc4a0, 0x5519120, 0x908b080, 0x16323600
    for i in range(512):
        edits.append((humans + i * 0x2b64, 'i', [0]))
    for i in range(1024):
        edits += [(objects + i * 0x8230, 'i', [0]), (items + i * 0x1ac, 'i', [0]),
                  (cityobjects + i * 0x54, 'i', [0])]
    edits += [(0xfefe2a4, '3f', [100000.0, 5000.0, -100000.0]), (0x14a8680, 'i', [display]),
              (0x14a8688, 'i', [world]), (0x17b4b820, 'i', [0]), (0x1779b808, 'i', [0]),
              (0x14c0690, 'i', [0]), (0x14a868c, 'i', [0]), (0x2a7593c0, 'i', [-1]),
              (0x9065308, 'i', [0]), (0x75a5154, 'B', [0]), (0x90652e0, 'i', [0])]
    if 'race' in features:
        edits += [(0x9065308, 'i', [2]), (0x908aaec, 'i', [3]),
                  (0x908aaf0, '9f', [100010.0, 5000.0, -100000.0, 99990.0, 5001.0, -100004.0,
                                     100000.0, 4990.0, -99980.0]),
                  (0xfefe2a0, 'i', [1]), (0xed813a0 + 0x2408 + 0x5c, 'I', [0b101])]
    if 'race_no_view' in features:
        edits += [(0x9065308, 'i', [2]), (0x908aaec, 'i', [2]), (0xfefe2a0, 'i', [-1]),
                  (0x908aaf0, '6f', [100002.0, 5000.0, -100000.0, 100004.0, 5000.0, -100000.0])]
    if 'lines' in features:
        lines = [float((i * 37) % 23) - 11.0 for i in range(6 * 5)]
        edits += [(0x14a868c, 'i', [5]), (0x14a8690, f'{len(lines)}f', lines)]
        spheres = []
        for i in range(3):
            spheres += [100000.0 + i, 5000.0, -100000.0 - i, 0.25 * i, 0.5, 1.0, 0.5 + i]
        edits += [(0x14c0690, 'i', [3]), (0x14c0694, f'{len(spheres)}f', spheres)]
    if 'bullets' in features:
        edits.append((0x17b4b820, 'i', [2]))
        for i in range(2):
            edits.append((0x1338680 + i * 0x5c + 0x14, '6f', [100000.0 + i, 5000.0, -100000.0,
                                                            100001.0 + i, 5001.0, -100002.0]))
    if 'bonds' in features:
        bonds = 0x1779b820
        edits += [(0x1779b808, 'i', [3]),
                  (bonds, 'i', [0]), (bonds + 0x14, '2i', [3, 7]),
                  (bonds + 0xec, 'i', [9]), (bonds + 0xec + 0x90, '3i', [2, 4, 5]),
                  (bonds + 0x1d8, 'i', [5])]
    if 'names' in features:
        edits += [(0x75a5154, 'B', [1]), (0x29e752a0, 'i', [0])]
    if 'shadow_key' in features:
        edits += [(0x75a514e, 'B', [1]), (0x127ea8, 'i', [1]), (0x29e752a0, 'i', [0])]
    return edits, {'renderframe': [call(0x75f10)] * 3}


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


def loaders_case():
    """Copy parts of a small image into a texture. The image and its path are in memory, so no file
    loads. Every part stays inside the image: a part outside it keeps the memory of the new buffer,
    and that memory differs between two runs. One call has a path with no file."""
    rng = random.Random(11)
    width, height = 6, 4
    pixels = [rng.getrandbits(32) for _ in range(width * height)]
    name = b'a.png\0'
    texture = 4000
    record = TEXTURES + texture * TEXTURE_SIZE
    edits = [
        (LOADED_IMAGE, '2i', [width, height]),
        (LOADED_IMAGE + 8, f'{len(pixels)}I', pixels),
        (IMAGE_NAME, f'{len(name)}B', list(name)),
        (record + 0x30, 'I', [0]),
    ]
    path = 'P:0:' + name.hex()
    other = 'P:0:' + b'no_such_file.png\0'.hex()
    parts = [(0, 0, 6, 4), (0, 0, 4, 4), (1, 1, 4, 2), (2, 0, 3, 3), (5, 3, 1, 1), (0, 0, 1, 1)]
    calls = [call(LOADTEXTUREPARTIAL, texture, path, x, y, w, h) for (x, y, w, h) in parts]
    calls.append(call(LOADTEXTUREPARTIAL, texture, other, 0, 0, 2, 2))
    return edits, {'loadtexturepartial': calls}


LIGHT_POSITION = 0x19c0cd78
CAMERA = 0xfefe2a0  # the audio camera record: +4 place, +0x10 three rotation rows


def sky_case(sun=(0.3, 0.8, 0.52)):
    """The sun and sky colors for one sun direction, plus direct calls of the two helpers."""
    rng = random.Random(11)
    edits = [(LIGHT_POSITION, '3f', list(sun))]
    ray, sun_color = [], []
    for slot in range(40):
        base = SCRATCH + slot * 48
        start = [rng.uniform(-300.0, 300.0), rng.uniform(0.0, 500.0), rng.uniform(-300.0, 300.0)]
        direction = [rng.uniform(-1.0, 1.0) for _ in range(3)]
        length = sum(v * v for v in direction) ** 0.5 or 1.0
        direction = [v / length for v in direction]
        end = [rng.uniform(-900.0, 900.0) for _ in range(3)]
        edits.append((base, '9f', start + direction + end))
        ray.append(call(0xe0b00, image(base), image(base + 12), rng.choice([512.0, 100.0, 0.5, 1e4])))
        sun_color.append(call(0xe0ea0, image(base + 36), image(base), image(base + 24)))
    sky_out = SCRATCH + 40 * 48
    return edits, {'raydistancesphere': ray, 'getsuncolor': sun_color,
                   'getsuncolors': [call(0xe0720)], 'getskycolor': [call(0xe0bc0, image(sky_out))]}


def lod_case(rec, place=(0.5, 0.5), turn=(0.7, 0.3), height=40.0):
    """The landscape detail levels for one camera. place is the camera place in landscape
    widths (32 sectors of 512 units) from the landscape origin. A NaN place is a NaN distance."""
    import math
    ox, oy, oz = Snapshot(rec / 'sv_snapshot.bin').read(LANDSCAPE, '3f')
    yaw, pitch = turn
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    rows = [(cy, 0.0, -sy), (sy * sp, cp, cy * sp), (sy * cp, -sp, cy * cp)]
    x, z = place
    edits = [(CAMERA + 4, '3f', [ox + x * 16384.0, oy + height, oz + z * 16384.0]),
             (CAMERA + 0x10, '9f', [v for row in rows for v in row])]
    return edits, {'setupframelandscapelod': [call(0xd92a0)]}


FRAME_MODEL_COUNT = 0x19c0d0b4


def addmodel_case(full=False):
    """Models added to the frame list: four texture numbers, a flag word, a place and a rotation.
    With full, the list holds 4096 models and each call does nothing."""
    rng = random.Random(13)
    edits = [(FRAME_MODEL_COUNT, 'i', [4096 if full else 0])]
    calls = []
    for slot in range(24):
        base = SCRATCH + slot * 64
        edits.append((base, '4i', [rng.randint(-5, 400) for _ in range(4)]))
        edits.append((base + 16, '12f', [floats(rng) for _ in range(12)]))
        calls.append(call(0x75d90, rng.randint(0, 600), image(base), rng.choice([0, 1, 3]),
                          image(base + 16), image(base + 28)))
    return edits, {'frame_addmodel': calls}



def rotate_case():
    """Turns of random matrices: the axis is a unit vector, a row of the matrix or a random
    vector, and the angle is random or an edge value."""
    import math
    rng = random.Random(17)
    angles = [0.0, math.pi, -math.pi, 1e10, NAN, 1e-30, 6.0]
    edits, calls = [], []
    for slot in range(48):
        base = SCRATCH + slot * 64
        rows = [rng.uniform(-1.0, 1.0) for _ in range(9)] if slot % 4 else [floats(rng) for _ in range(9)]
        axis = [rng.uniform(-1.0, 1.0) for _ in range(3)]
        if slot % 3 == 0:
            length = sum(v * v for v in axis) ** 0.5 or 1.0
            axis = [v / length for v in axis]
        edits += [(base, '9f', rows), (base + 36, '3f', axis)]
        target = image(base + 12 * (slot % 3)) if slot % 5 == 0 else image(base + 36)
        angle = angles[slot % len(angles)] if slot % 2 else rng.uniform(-7.0, 7.0)
        calls.append(call(0xe4f10, image(base), target, angle))
    return edits, {'rotateorientationfast': calls}


MENU_ITEMS, MENU_COUNT, MENU_INDEX = 0x5350d00, 0x9065240, 0xfefe288
MOUSE, MOUSE_PREVIOUS, KEYS, KEYS_PREVIOUS, KEYS_BLOCKED, FONTNUM = (
    0x19c07c80, 0xb733100, 0x75a5140, 0x1bbe1a0, 0x29e752a0, 0x127eac)


def menu_case(click=True, blocked=0):
    """The menu items: twelve random items of each kind under a mouse and keys, three ticks of
    checkmenuitems, an empty item, a reset, the two callbacks and text widths."""
    rng = random.Random(19 + click + 2 * blocked)
    values = SCRATCH + 0x800
    texts = SCRATCH + 0x900
    words = [b'Back', b'Join game', b'', b'A' * 70, bytes(range(0x80, 0xc0)), b'Options\x01\xff']
    edits = [(MENU_COUNT, 'i', [12]), (MOUSE, '2i', [300, 200]), (MOUSE + 0x10, 'i', [1 if click else 0]),
             (MOUSE_PREVIOUS + 0x10, 'i', [0]), (KEYS_BLOCKED, 'i', [blocked]), (FONTNUM, 'i', [rng.choice([0, 1])]),
             (values, '8i', [rng.randint(-3, 7) for _ in range(8)])]
    for slot, word in enumerate(words):
        edits.append((texts + slot * 0x50, f'{len(word) + 1}s', [word + b'\0']))
    for key in (30, 40, 41, 44, 81, 82):
        edits += [(KEYS + key, 'B', [rng.randint(0, 1)]), (KEYS_PREVIOUS + key, 'B', [rng.randint(0, 1)])]
    for i in range(12):
        base = MENU_ITEMS + i * 0x284
        kind = [0, 0, 3, 4, 1, 5][i % 6]
        box = [rng.choice([250.0, 300.0, 290.0, NAN, 310.0]), rng.choice([150.0, 200.0, 199.5, 220.0]),
               rng.choice([10.0, 50.0, 0.0, -5.0, INF]), rng.choice([10.0, 60.0, 1.0])]
        edits += [(base, 'i', [kind]), (base + 0x1d8, '2f', box[:2]), (base + 0x1f8, '2f', box[2:]),
                  (base + 0x200, '6i', [rng.choice([0, 30, 40, 41, 44]), rng.choice([0, 0, 81, 82]),
                                        rng.randint(0, 1), int(rng.random() < 0.2), 0, 0]),
                  (base + 0x21c, 'f', [floats(rng)]), (base + 0x220, '2i', [rng.randint(0, 1), 0]),
                  (base + 0x228, 'I', [rng.choice([1, 2, 4, 3])]), (base + 0x274, 'i', [i + 1]),
                  (base + 0x27c, 'I', [0x20000000 + values + 4 * (i % 8)]),
                  (base + 0x280, 'I', [0x20000000 + rng.choice([0x103dd0, 0x103e30]) if kind in (3, 4) else 0])]
    text = [image(texts + slot * 0x50) for slot in range(len(words))]
    check = [call(0x101be0) for _ in range(3)]
    made = [call(0x1036d0), call(0x103970)]
    callbacks = []
    for i in (2, 3, 8, 9):
        callbacks += [(call(0x103dd0) if i % 6 == 2 else call(0x103e30))]
    widths = [call(0x109540, t, rng.choice([1.0, 0.5, 24.0, floats(rng)])) for t in text]
    edits.append((MENU_INDEX, 'i', [2]))
    return edits, {'checkmenuitems': check, 'resetmenuitems': [made[1]], 'createmenuitemempty': [made[0]],
                   'menutoggle': [callbacks[0]], 'menuset': [callbacks[1]], 'text_getlength': widths}


def menu_build_case():
    """createmenuitem, createmenubutton and setmenuiteminfo, each in its own list."""
    rng = random.Random(23)
    texts = SCRATCH + 0x900
    words = [b'Back', b'Join game', b'', b'A' * 70, bytes(range(0x80, 0xc0))]
    edits = [(MENU_COUNT, 'i', [5])]
    for slot, word in enumerate(words):
        edits.append((texts + slot * 0x50, f'{len(word) + 1}s', [word + b'\0']))
    text = [image(texts + slot * 0x50) for slot in range(len(words))]
    item, button, info = [], [], []
    for i in range(24):
        item.append(call(0x102080, text[i % 5], floats(rng), floats(rng), rng.choice([16.0, 24.0, floats(rng)]),
                         rng.choice([0, 1, 2, 4, 8, 5, 10, 15]), floats(rng), 1.0, 0.5, floats(rng)))
        button.append(call(0x1024e0, text[i % 5], floats(rng), floats(rng), rng.choice([100.0, floats(rng)]),
                           rng.choice([32.0, floats(rng)]), floats(rng), rng.choice([0, 1, 2, 4, 8, 9, 6]),
                           1.0, floats(rng), 1.0, floats(rng)))
        info.append(call(0x102960, text[i % 5], floats(rng), floats(rng), 12.0, i, 1.0, floats(rng), 0.0, 1.0))
    return edits, {'createmenuitem': item, 'createmenubutton': button, 'setmenuiteminfo': info}


def framelist_case(extra=()):
    """buildframelist on the scene of the recording: its humans, vehicles, items and street
    objects. extra adds synthetic records: 'bills' (a free stack and a stack in a hand),
    'elevator' (a street elevator with its doors), 'types' (one free item of each type), 'view' (the local player views human 0),
    'bullets' (two bullets), 'wreck' (vehicle 0 has no health), 'heli' (a helicopter)."""
    items, cityobjects, objects = 0x908b080, 0x16323600, 0x5519120
    ident = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    edits = []
    if 'bills' in extra:
        for slot, (human, hand, count) in enumerate([(-1, 0, 4), (0, 1, 3), (0, 0, 2)]):
            base = items + (1000 + slot) * 0x1ac
            edits += [(base, 'i', [1]), (base + 0xc, 'i', [16]), (base + 0x18, '3i', [human, -1, hand]),
                      (base + 0x48, '3f', [10.0 + slot, 30.0, 12.0]), (base + 0x84, '9f', ident),
                      (base + 0x134, '3I', [count, count - 1, 0o5321 >> slot])]
    if 'types' in extra:
        for t in range(27):
            base = items + (900 + t) * 0x1ac
            edits += [(base, 'i', [1]), (base + 0xc, 'i', [t]), (base + 0x18, '3i', [-1, -1, 0]),
                      (base + 0x48, '3f', [float(t), 30.0, 14.0]), (base + 0x84, '9f', ident),
                      (base + 0x10c, 'i', [t & 1])]
    if 'elevator' in extra:
        base = cityobjects + 1000 * 0x54
        edits += [(base, '2i', [1, 2]), (base + 8, '3f', [20.0, 30.0, 20.0]), (base + 0x20, '9f', ident),
                  (base + 0x50, 'f', [1.25])]
    if 'view' in extra:
        edits += [(0xfefe2a0, 'i', [0]), (0xed813a0 + 0x78, 'i', [0]), (0x14fc4a0, 'i', [1]),
                  (0x14fc4a0 + 0x28, 'i', [50]), (0x127ea8, 'i', [0])]
    if 'bullets' in extra:
        edits.append((0x17b4b820, 'i', [2]))
        for i in range(2):
            edits.append((0x1338680 + i * 0x5c, 'i', [i]))
            edits.append((0x1338680 + i * 0x5c + 0x14, '3f', [5.0 + i, 31.0, 7.0]))
            edits.append((0x1338680 + i * 0x5c + 0x38, '9f', ident))
    if 'wreck' in extra:
        edits.append((objects + 0xc, 'i', [0]))
    if 'heli' in extra:
        edits += [(objects + 1000 * 0x8230, '3i', [1, 11, 1]), (objects + 1000 * 0x8230 + 0xc, 'i', [100]),
                  (objects + 1000 * 0x8230 + 0x24, '3f', [15.0, 35.0, 15.0]),
                  (objects + 1000 * 0x8230 + 0x3c, '9f', ident),
                  (objects + 1000 * 0x8230 + 0x5eec + 0x30, 'f', [0.7])]
    return edits, {'buildframelist': [call(0x74890)] * 2}


FINDSTRING, CHECKSTRING, GETSTRING, CS_GETLINE = 0xfd800, 0xfd870, 0xfdb20, 0xfdba0
PARSER = 0x13d5e700


def parser_case(which):
    """The text parser on a made text: the keys of a settings file. The cursor is an edit for each function,
    and the calls of one function follow each other. A key that is not in the text moves the cursor past
    the end. A key with no letters, and a line longer than the buffer of the call, are in the lists."""
    text = (b'screenwidth=1024\r\nname=Agent 7\r\nfullscreen=1\r\nnotanumber=abc\r\nneg=-42\r\n'
            b'long=' + b'x' * 70 + b'\r\nlast=9')
    base = [(PARSER + 8, f'{len(text) + 1}B', list(text) + [0]), (PARSER + 4, 'i', [len(text)])]

    def key(text_key):
        return 'P:0:' + (text_key + b'\0').hex()

    def out():
        return 'P:0:' + (b'\xaa' * 64).hex()

    if which == 'findstring':
        keys = [b'screenwidth=', b'name=', b'fullscreen=', b'neg=', b'last=', b'missing=', b'', b'x']
        return base + [(PARSER, 'i', [0])], {'findstring': [call(FINDSTRING, key(k)) for k in keys]}
    if which == 'checkstring':
        keys = [b'screenwidth=', b'screenwidt', b'name=', b'', b'screenwidth=1024\r\nname=Agent 7\r\nfullscreen=1\r\nzzz']
        return base + [(PARSER, 'i', [0])], {'checkstring': [call(CHECKSTRING, key(k)) for k in keys]}
    if which == 'getstring':
        start = text.index(b'name=') + 5
        calls = [call(GETSTRING, out(), 30), call(GETSTRING, out(), 3), call(GETSTRING, out(), 0), call(GETSTRING, out(), 64)]
        return base + [(PARSER, 'i', [start])], {'getstring': calls}
    calls = [call(CS_GETLINE, out(), 64), call(CS_GETLINE, out(), 8), call(CS_GETLINE, out(), 64), call(CS_GETLINE, out(), 64)]
    return base + [(PARSER, 'i', [0])], {'cs_getline': calls}


def run(snapshot, args, name, env):
    p = subprocess.run([HARNESS, IMAGE, str(snapshot), str(args), name], capture_output=True, text=True,
                       errors='replace', timeout=3600, env=env, cwd=ROOT)
    if os.environ.get('HARNESS_DEBUG'):
        print(p.stdout[-3000:])
        print(p.stderr[-3000:])
    suffix = '.bench.txt' if env.get('HARNESS_TIME') else '.result.txt'
    Path(args).with_suffix(suffix).write_text(p.stdout + p.stderr, encoding='utf-8')
    if env.get('HARNESS_TIME'):
        for line in p.stdout.splitlines():
            parts = line.split()
            if len(parts) == 5 and parts[:2] == ['time', name]:
                count, elapsed, reference = map(int, parts[2:])
                ratio = elapsed / reference if reference else float('nan')
                return f'{count} calls; ratio {ratio:.3f}'
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
            'effects': effects_case, 'effects_ground': lambda: effects_case(1),
            'effects_slide': lambda: effects_case(2),
            'effects_land': lambda: effects_case(3), 'effects_land_full': lambda: effects_case(4),
            'landed': landed_case,
            'glass': glass_case, 'glass_full': lambda: glass_case(True),
            'muzzle': muzzle_case, 'pickup': pickup_case,
            'pickup_held': lambda: pickup_case(1), 'pickup_no_view': lambda: pickup_case(2),
            'shadow': sector_shadow_case, 'shadow_buffer': lambda: sector_shadow_case(0.75, 1),
            'shadow_ready': lambda: sector_shadow_case(0.75, 1, 1),
            'shadow_flat': lambda: sector_shadow_case(0.125), 'shadow_negative': lambda: sector_shadow_case(-1.0),
            'shadow_nan': lambda: sector_shadow_case(NAN),
            'frame_models': frame_models_case, 'frame_models_debug': lambda: frame_models_case(1),
            'human_model': human_model_case, 'video_modes': video_modes_case,
            'video_duplicate': lambda: video_modes_case(True),
            'screenshot': screenshot_case, 'screenshot_empty': lambda: screenshot_case(0, 0),
            'screenshot_large': lambda: screenshot_case(1281, 960),
            'screenshot_tall': lambda: screenshot_case(1280, 961),
            'screenshot_collision': screenshot_case,
            'depth': depth_case, 'depth_fbo': lambda: depth_case(1),
            'depth_ao': lambda: depth_case(0, 1), 'depth_fbo_ao': lambda: depth_case(1, 1),
            'depth_debug': lambda: depth_case(0, 0, 1), 'depth_fbo_debug': lambda: depth_case(1, 1, 1),
            'depth_scene': depth_scene_case, 'depth_scene_world': lambda: depth_scene_case(1),
            'debug0': debug_display_case, 'debug1': lambda: debug_display_case(1),
            'debug2': lambda: debug_display_case(2), 'debug3': lambda: debug_display_case(3),
            'debug4': lambda: debug_display_case(4), 'debug5': lambda: debug_display_case(5),
            'debug5_server': lambda: debug_display_case(5, 1),
            'debug_float': debug_float_case, 'debug_float_negative': lambda: debug_float_case(True),
            'frame': frame_case, 'frame_fbo': lambda: frame_case(1), 'frame_ao': lambda: frame_case(1, 1),
            'frame_display': lambda: frame_case(display=1), 'frame_shadow_key': lambda: frame_case(shadow_key=1),
            'frame_shadow_key_fbo': lambda: frame_case(1, 0, 0, 0, 1),
            'frame_world': lambda: frame_case(world=1, features=('lines', 'bonds')),
            'frame_world_off': lambda: frame_case(features=('lines', 'bonds')),
            'frame_race': lambda: frame_case(features=('race',)),
            'frame_race_no_view': lambda: frame_case(features=('race_no_view',)),
            'frame_names': lambda: frame_case(features=('names',)),
            'parser_find': lambda: parser_case('findstring'), 'parser_check': lambda: parser_case('checkstring'),
            'parser_get': lambda: parser_case('getstring'), 'parser_line': lambda: parser_case('cs_getline'),
            'objecttypes': objecttypes_case, 'loaders': loaders_case, 'textures': lambda: textures_case(rec),
            'landscape': lambda: landscape_case(rec), 'windows': lambda: windows_case(rec),
            'materials': lambda: materials_case(rec),
            'sky': sky_case, 'sky_low': lambda: sky_case((0.97, 0.05, 0.2)),
            'sky_below': lambda: sky_case((0.6, -0.3, 0.74)), 'sky_up': lambda: sky_case((0.0, 1.0, 0.0)),
            'sky_nan': lambda: sky_case((NAN, 0.5, 0.5)),
            'lod': lambda: lod_case(rec), 'lod_corner': lambda: lod_case(rec, (0.02, 0.98), (2.4, 0.1)),
            'lod_outside': lambda: lod_case(rec, (-0.3, 1.4), (0.0, 0.0)),
            'lod_down': lambda: lod_case(rec, (0.5, 0.5), (1.0, 1.5), 3000.0),
            'lod_nan': lambda: lod_case(rec, (NAN, 0.5)),
            'rotate': rotate_case,
            'menu': menu_case, 'menu_keys': lambda: menu_case(False), 'menu_blocked': lambda: menu_case(True, 1),
            'menu_build': menu_build_case,
            'addmodel': addmodel_case, 'addmodel_full': lambda: addmodel_case(True),
            'framelist': framelist_case, 'framelist_bills': lambda: framelist_case(('bills', 'elevator', 'types')),
            'framelist_view': lambda: framelist_case(('view', 'bullets', 'wreck', 'heli'))}
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else list(allc)
    repeat = max(1, int(os.environ.get('SV_SYNTH_REPEAT', '1')))
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
        case_env = dict(env)
        if name.startswith('screenshot'):
            game = case / 'game'
            game.mkdir(exist_ok=True)
            case_env['HARNESS_GAME_DIR'] = str(game)
            if name == 'screenshot_collision':
                for number in range(3):
                    (game / f'scsht{number:03d}.tga').write_bytes(b'existing screenshot')
        for function, calls in functions.items():
            calls = calls * repeat
            args = case / f'{function}.txt'
            args.write_text(''.join(calls), encoding='utf-8', newline='\n')
            print(f'{name}: {function}: {len(calls)} calls:', run(snapshot, args, function, case_env), flush=True)
        if edits:
            snapshot.unlink()


if __name__ == '__main__':
    main()
