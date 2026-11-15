"""Run the pure math functions of the client on made inputs, with the clean code and the original.

No recording calls these functions. Each call gets vectors, matrices and numbers: random values,
edge values (zero, negative zero, tiny, huge, NaN, infinity) and shapes that make a hit (a line that
crosses a triangle or a sphere). The harness compares the result and the memory behind each pointer.

Usage: python -I instrumentation/client/synth_pure.py <recording dir> <work dir> [function,...]
The recording gives the memory image. The functions must be in a list of adopted functions.
"""
import csv
import math
import os
import random
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'client'))
import synth_textures as st  # noqa: E402

# Parameter kinds: v vector in, o vector out, m 3 by 3 matrix, q quaternion, f float, F float out,
# i small int, I int of any size, u mask of 32 bits, s text buffer out, k list of 8 words of 32 bits,
# l count of words (1 to 8), D degree of a curve (0 to 4), w four floats out, P four vectors.
# World kinds, for the collision functions: W a point of the world (x and z from 0 to 2048, y from 0 to 80;
# every fifth call uses the street grid), X a second point within 40 units of the last W (the end of a line),
# r a radius, t a team (-1 to 3), n an object, human or item index (live indices, -1 or 4095).
# Area kinds: a an area (0 to 3), A an area that can be out of range (0 to 3, 4, 5, 0xffffffff), c a block
# coordinate (edges of sectors and chunks, a random one, or -1), B a block word, e a face (-1 to 7).
PROTOTYPES = {
    'area_getblock': 'Accc',
    'area_marksector': 'Accc',
    'area_setblock': 'AcccB',
    'area_setblockcolor': 'AcccB',
    'area_deleteblock': 'Accc',
    'area_interior_setobject': 'AcccB',
    'area_getnextchunk': 'a',
    'area_getnextsector': '',
    'area_createchunk': 'Accc',
    'area_createsector': 'Acccc',
    'lineintersectobject': 'nWXi',
    'lineintersecttrack': 'WXi',
    'lineintersecttrackrail': 'WXi',
    'item_sphereintersect': 'nWr',
    'lineintersectcityobject': 'ooFnWX',
    'lineintersectcityobjecttype': 'ooFnWmXX',
    'lineintersectarea': 'ooFAWX',
    'lineintersectnormalblock': 'ooFWXzBf',
    'lineintersectspecblock': 'ooFWXzBf',
    'setupbody': '',
    'setupbuildingtypes': '',
    'setupcityobjecttypes': '',
    'setupitemtypes': '',
    'setupobjecttypes_cars': '',
    'setupobjecttypes_helicopters': '',
    'setupobjectypes_train': '',
    'area_block_rotatesurface': 'Te',
    'area_copyrotateblock': 'cz',
    'area_block_addbasicsurface': 'cv',
    'area_block_addbeam': 'Tffffffck',
    'area_block_addwindow': 'Tffffcc',
    'area_block_adddoor': 'Tff',
    'area_block_addoutdoor': 'Tff',
    'area_block_addcorner': 'Tff',
    'area_block_addwindowvertical': 'Tii',
    'area_block_addwindowhorizontal': 'Tiiii',
    'area_createinteriorblocks': '',
    'createevent_bullethit': 'iivv',
    'createevent_updateobject': 'niIvv',
    'createevent_updateitem': 'n',
    'createevent_updateiteminfo': 'n',
    'createevent_updateplayer': 'n',
    'createevent_updateplayer_finance': 'n',
    'createevent_updateblock_quad': 'Accccevv',
    'sphereintersectlandscape': 'Wr',
    'lineintersectlandscape': 'WX',
    'sphereintersectlevel': 'Wr',
    'lineintersectlevel': 'WXi',
    'lineintersectall': 'WX',
    'pointinsidebase': 'Wtf',
    'pointinsidebasesafe': 'Wtf',
    'angulardistance': 'ff',
    'calculatebarycentric': 'ovvvv',
    'circleintersecttriangle': 'ooFvvfvvv',
    'geteulerangles2': 'om',
    'lineintersectplane': 'ovFvvv',
    'lineintersectplanetwosided': 'ovFvvv',
    'lineintersectsphere': 'ooFvvvf',
    'lineintersecttriangle': 'ovFvvvvv',
    'lineintersecttriangletwosided': 'ovFvvvvvi',
    'multiply3x3matrices': 'mmm',
    'multiplyquaternion': 'qqq',
    'orientationtoquaternion': 'qm',
    'pointdistanceline': 'vvv',
    'projectorientation': 'mmm',
    'projectvector': 'ovm',
    'rotateorientation': 'mvf',
    'rotatevector': 'vvf',
    'sphereintersectline': 'ooFvfvv',
    'sphereintersecttriangle': 'ovFvfvvv',
    'sphereintersecttrianglefull': 'ooFvfvvv',
    'genrand_int32': '',
    'init_by_array': 'kl',
    'getrandomvector2d': 'off',
    'sprintf_comma': 'sI',
    'tesselatebezier': 'Dfffff',
    'tesselatebezierderivative': 'Dfffff',
    'objecttype_calcquadweights': 'wvP',
    'calculatefriction': 'vvf',
}


def rvas():
    out = {}
    for row in csv.DictReader((ROOT / 'analysis/client_win/functions.csv').open(encoding='utf-8')):
        if row['name']:
            out[row['name']] = int(row['rva'], 16)
    return out


def number(rng, kind):
    """One float: mostly small random values, sometimes an edge value."""
    pick = rng.random()
    if pick < 0.12:
        # SYNTH_FINITE=1 keeps NaN, infinity and huge values out of the arguments.
        if os.environ.get('SYNTH_FINITE'):
            return rng.uniform(-6.0, 6.0)
        return rng.choice(st.EDGES)
    if pick < 0.2:
        return rng.choice([0.0, 1.0, -1.0, 0.5])
    return rng.uniform(-6.0, 6.0)


def unit(rng):
    v = [rng.uniform(-1.0, 1.0) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def orientation(rng):
    """Three orthogonal unit rows, or random junk now and then."""
    if rng.random() < 0.2:
        return [number(rng, 'f') for _ in range(9)]
    a = unit(rng)
    b = unit(rng)
    c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    n = math.sqrt(sum(x * x for x in c)) or 1.0
    c = [x / n for x in c]
    d = [c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0]]
    return a + c + d


def make_args(rng, kinds, index):
    """Build the arguments of one call. Every few calls use shapes that cross, so the hit paths run."""
    args = []
    hit = index % 3 == 0
    base = [rng.uniform(-3.0, 3.0) for _ in range(3)]
    last_world = (1000.0, 10.0, 1000.0)
    for kind in kinds:
        if kind == 'T':
            args.append(rng.choice([0, 1, 2, 3, 0, 1, 2, 3, 4, -1]))
        elif kind == 'z':
            args.append('P:0:' + struct.pack('<3i', *[rng.choice([1, 2, 4, 8, 3, 0]) for _ in range(3)]).hex())
        elif kind == 'a':
            args.append(rng.choice([0, 1, 2, 3]))
        elif kind == 'A':
            args.append(rng.choice([0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0xffffffff]))
        elif kind == 'c':
            args.append(rng.choice([0, 1, 7, 8, 63, 64, 65, 127, 128, 255, 256, 511, -1, rng.randrange(0, 600),
                                    rng.randrange(0, 600), rng.randrange(0, 70)]))
        elif kind == 'B':
            args.append(rng.choice([0, 1, 2, 0xff, 0xffff, 0xffffffff, rng.randrange(0, 2**32), rng.randrange(0, 2**16)]))
        elif kind == 'e':
            args.append(rng.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7]))
        elif kind == 'W':
            x, z = rng.uniform(0, 2048), rng.uniform(0, 2048)
            if index % 5 == 0:
                x, z = round(x / 8) * 8.0, round(z / 8) * 8.0
            last_world = (x, rng.uniform(0, 80), z)
            args.append(last_world)
        elif kind == 'X':
            args.append(tuple(last_world[k] + rng.uniform(-40, 40) for k in range(3)))
        elif kind == 'r':
            args.append(rng.choice([0.0, 0.05, 0.25, 0.5, 1.0, 2.0, 4.0]))
        elif kind == 't':
            args.append(rng.choice([-1, 0, 1, 2, 3]))
        elif kind == 'n':
            args.append(rng.choice([-1, 0, 1, 2, 3, 5, 9, 4095]))
        elif kind == 'o':
            args.append(tuple([123.5] * 3))
        elif kind == 'F':
            args.append(tuple([77.25]))
        elif kind == 'v':
            if hit and rng.random() < 0.7:
                args.append(tuple(base[k] + rng.uniform(-2.5, 2.5) for k in range(3)))
            else:
                args.append(tuple(number(rng, 'f') for _ in range(3)))
        elif kind == 'm':
            args.append(tuple(orientation(rng)))
        elif kind == 'q':
            args.append(tuple(number(rng, 'f') for _ in range(4)))
        elif kind == 'f':
            args.append(abs(number(rng, 'f')) if rng.random() < 0.5 else number(rng, 'f'))
        elif kind == 'i':
            args.append(rng.choice([0, 1, 2, -1]))
        elif kind == 'I':
            args.append(rng.choice([0, 1, -1, 999, 1000, 123456, 1000000, 2147483647, -2147483648, rng.randrange(-10**9, 10**9)]))
        elif kind == 'u':
            args.append(rng.choice([0, 0xff, 0xffff, 0x3ff, 0xffffffff, rng.randrange(0, 2**32)]))
        elif kind == 's':
            args.append('P:0:' + (b'U' * 64).hex())
        elif kind == 'k':
            args.append('P:0:' + b''.join(struct.pack('<I', rng.randrange(0, 2**32)) for _ in range(8)).hex())
        elif kind == 'D':
            args.append(rng.choice([0, 1, 2, 3, 4]))
        elif kind == 'E':
            args.append(rng.choice([1, 2, 3]))
        elif kind == 'w':
            args.append(tuple([55.5] * 4))
        elif kind == 'P':
            args.append(tuple(number(rng, 'f') for _ in range(12)))
        elif kind == 'l':
            args.append(rng.randrange(1, 9))
    return args


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else list(PROTOTYPES)
    addresses = rvas()
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build' / 'client_test'),
               HARNESS_ENV_DIR=str(work / 'env'), SV_UI_WIDTH='220')
    env['PATH'] = str(ROOT / 'build' / 'client_deps') + os.pathsep + env.get('PATH', '')
    # SYNTH_CALLS: the number of calls for each function (90 by default, 30 to look around fast).
    # SYNTH_JOBS: the number of functions that run at the same time (1 by default, 2 is safe).
    count = int(os.environ.get('SYNTH_CALLS', '90'))
    jobs = max(1, int(os.environ.get('SYNTH_JOBS', '1')))

    def run_one(item):
        # "name=kinds" replaces the prototype of that function for this run.
        name, _, kinds = item.partition('=')
        proto = kinds if kinds else PROTOTYPES[name]
        rng = random.Random(sum(map(ord, name)))
        calls = []
        for index in range(count):
            args = make_args(rng, proto, index)
            calls.append(st.call(addresses[name], *args))
        path = work / f'{name}.txt'
        path.write_text(''.join(calls), encoding='utf-8', newline='\n')
        own = dict(env, HARNESS_ENV_DIR=str(work / f'env_{name}'), SV_LOG_FILE=str(work / f'{name}.log'))
        return f'{name}: {len(calls)} calls: ' + st.run(rec / 'sv_snapshot.bin', path, name, own)

    if jobs == 1:
        for item in want:
            print(run_one(item), flush=True)
    else:
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            for line in pool.map(run_one, want):
                print(line, flush=True)


if __name__ == '__main__':
    main()
