"""Run the noise, curve, model and object type functions of the client on made inputs.

No recording calls these functions. A case writes a few values into the snapshot of a recording
(or uses it as it is) and calls each function many times. The harness compares the clean function
with the original: the result and the memory. A case snapshot is about 350 MB, so the script
deletes it after the run.

Usage: python -I instrumentation/client/synth_textures.py <recording dir> <work dir> [case,...]
Cases: noise, hermite, models, objecttypes. The script sets the environment of the gate itself.
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
ADDPARTICLE, ADDBOND, ADDFACE, ADDWHEEL, POINTSCALE = 0xeb750, 0xeb810, 0xeb950, 0xebb20, 0xec220

# The model in work (RVA): the counts and the arrays that the cases fill.
MODELTEMP = 0x90f7680
NUM_VERTS, VERTS, NUM_TRIS, NUM_QUADS, QUADS = 0x0, 0x8, 0xa0008, 0x20000c, 0x200010
# Free vertices of the model in work, far above the ones a case uses: room for the curve inputs.
SCRATCH = MODELTEMP + VERTS + 7000 * 12

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
    grid += [(floats(rng), floats(rng), rng.choice(masks)) for _ in range(40)]
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
    }


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
    }


def run(snapshot, args, name, env):
    p = subprocess.run([HARNESS, IMAGE, str(snapshot), str(args), name], capture_output=True, text=True,
                       errors='replace', timeout=3600, env=env, cwd=ROOT)
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
    allc = {'noise': lambda: ([], noise_calls()), 'hermite': hermite_case, 'models': models_case,
            'objecttypes': objecttypes_case}
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
