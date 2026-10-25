"""Test the object creation, the type setups and the car AI on situations that no recording has.

The recordings take the snapshot after setupgame, so the type setups always run a second time
on full lists. The fresh_* cases clear the type records first: that is the first run of the
real server. Run it like synth_vehicles.py:

    python -I instrumentation/lift/synth_types.py [--run] [--no-cover] <recording dir> <work dir> [case,...]

The cases:
  fresh_cars, fresh_heli, fresh_train   clear the types, then one setup
  fresh_all                             clear the types, then the three setups in the game order
  obj_car, obj_vel, obj_type1, obj_heli, obj_full, obj_noserver, obj_rot   createobject
  vehicle, vehicle_rot                  createvehicle
  ai_*, nwp_*                           ai_playercar and ai_car_nextwaypoint
"""
import copy
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot, OBJECTS, OBJECT_SIZE

ROOT = Path(__file__).resolve().parents[2]
TYPES, TYPE_SIZE, NUM_TYPES = 0xac51bc0, 0x828c, 128
POINTTEMP, POINTTEMP_SIZE = 0x1c497c0, 0x1800
SERVER = 0x761ea60
PLAYERS, PLAYER_SIZE = 0xededd00, 0x2408
CITY, ISEC, ISEC_SIZE, ROAD, ROAD_SIZE = 0xe299220, 0x8, 0x68, 0x1a00c, 0x4c
# Player fields.
P_BOT, P_CHASE, P_COOLDOWN, P_WAYPOINT, P_WAYPOINTS = 0x1a38, 0x1a3c, 0x1a40, 0x1a44, 0x1a4c
P_DEST_X, P_DEST_Z, P_WAIT_ISEC, P_WAIT_LIGHT, P_STUCK = 0x214c, 0x2150, 0x2154, 0x2158, 0x2160
WP_SIZE, WP_ROAD, WP_DIR, WP_LANE, WP_TURN = 0x1c, 0xc, 0x10, 0x14, 0x18
# Object fields.
O_POS, O_ROT, O_VEL, O_RPM, O_GEAR = 0x24, 0x3c, 0x60, 0x5e50, 0x5e6c


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def vec(*v):
    return struct.pack('<%df' % len(v), *v)


def call(rva, *a):
    out = 'R %x' % rva
    for x in a:
        if isinstance(x, bytes):
            out += ' P:19fc24:' + x.hex()
        elif x is None:
            out += ' V:0'
        else:
            out += ' V:%x' % (x & 0xffffffff)
    return out


def zero(s, rva, n):
    for k in range(n):
        s.write(rva + k, 'B', 0)


def clear_types(s):
    zero(s, TYPES, TYPE_SIZE * NUM_TYPES)
    zero(s, POINTTEMP, POINTTEMP_SIZE)


IDENT = vec(1, 0, 0, 0, 1, 0, 0, 0, 1)
TURNED = vec(0.8, 0, -0.6, 0, 1, 0, 0.6, 0, 0.8)
NEG_ZERO = vec(-0.0, 0, 1, 0, 1, -0.0, -1, 0, -0.0)
POS = vec(10.5, 2.25, -7.75)
VEL = vec(1.0, 0.5, -2.0)


class Ctx:
    def __init__(self, base, bot, car):
        self.base, self.bot, self.car = base, bot, car


def cases(c):
    s = c.s
    out = {}

    def fresh(*rvas):
        def make():
            clear_types(s)
            return [call(r) for r in rvas]
        return make

    def obj(type_, rot=IDENT, vel=None, color=5, server=1, full=False):
        def make():
            s.write(SERVER, 'i', server)
            if full:
                for k in range(1024):
                    s.write(OBJECTS + k * OBJECT_SIZE, 'i', 1)
            return [call(0x31800, type_, POS, vel, rot, color)]
        return make

    def vehicle(rot):
        def make():
            return [call(0x3ccc0, 0, POS, rot)]
        return make

    # The AI cases.
    def pl(k=None):
        return PLAYERS + (c.bot if k is None else k) * PLAYER_SIZE

    def ob():
        return OBJECTS + c.car * OBJECT_SIZE

    def wp():
        p = pl()
        k = s.read(p + P_WAYPOINT, 'i')[0]
        return p + P_WAYPOINTS + k * WP_SIZE

    def next_isec():
        w = wp()
        road, d = s.read(w + WP_ROAD, 'i')[0], s.read(w + WP_DIR, 'i')[0]
        ends = s.read(CITY + ROAD + road * ROAD_SIZE, 'ii')
        return ends[d ^ 1]

    def ai(fn):
        def make():
            fn()
            return [call(0xe4d0, c.bot, c.car), call(0xe4d0, c.bot, c.car), call(0x1610, c.bot)]
        return make

    def chase():
        s.write(pl() + P_CHASE, 'i', 1)
        s.write(pl() + P_COOLDOWN, 'i', 0)
        pos = s.read(ob() + O_POS, 'fff')
        rot = s.read(ob() + O_ROT, 'f' * 9)
        other = OBJECTS + (c.car + 1) * OBJECT_SIZE
        s.write(other, 'i', 1)
        s.write(other + O_POS, 'fff', *[pos[i] + 5 * rot[6 + i] for i in range(3)])
        s.write(other + O_ROT, 'f' * 9, *rot)
        s.write(other + O_VEL, 'fff', 0.1, 0.0, 0.2)

    def stuck(n):
        def f():
            s.write(pl() + P_STUCK, 'i', n)
        return f

    def slow():
        s.write(pl() + P_STUCK, 'i', 10)
        s.write(ob() + O_VEL, 'fff', 0.0, 0.0, 0.0)

    def red():
        i = next_isec()
        for d in range(4):
            s.write(CITY + ISEC + i * ISEC_SIZE + 0x48 + 4 * d, 'i', 1)
        pos = s.read(CITY + ISEC + i * ISEC_SIZE + 0xc, 'fff')
        s.write(ob() + O_POS, 'fff', pos[0] + 3.0, pos[1], pos[2] + 3.0)

    def light():
        i = next_isec()
        j = (i + 7) % 100
        s.write(pl() + P_WAIT_ISEC, 'i', j)
        s.write(pl() + P_WAIT_LIGHT, 'i', s.read(CITY + ISEC + j * ISEC_SIZE + 0x38, 'i')[0])
        s.write(ob() + O_POS, 'fff', 0.0, 0.0, 0.0)

    def gear(rpm):
        def f():
            s.write(ob() + O_RPM, 'f', rpm)
            s.write(ob() + O_GEAR, 'i', 2)
        return f

    def dest():
        i = next_isec()
        s.write(pl() + P_DEST_X, 'i', s.read(CITY + ISEC + i * ISEC_SIZE, 'i')[0])
        s.write(pl() + P_DEST_Z, 'i', s.read(CITY + ISEC + i * ISEC_SIZE + 8, 'i')[0])

    def reroll():
        i = next_isec()
        w = wp()
        road = s.read(w + WP_ROAD, 'i')[0]
        for d in range(4):
            a = CITY + ISEC + i * ISEC_SIZE + 0x18 + 4 * d
            if s.read(a, 'i')[0] != road:
                s.write(a, 'i', -1)

    def nwp(fn):
        def make():
            fn()
            return [call(0x1610, c.bot)] * 3
        return make

    out.update(
        fresh_cars=fresh(0x34c10), fresh_heli=fresh(0x3a610), fresh_train=fresh(0x3b0c0),
        fresh_all=fresh(0x34c10, 0x3a610, 0x3b0c0),
        obj_car=obj(0), obj_vel=obj(0, vel=VEL), obj_type1=obj(1), obj_heli=obj(11),
        obj_train=obj(12), obj_bogie=obj(13),
        obj_full=obj(0, full=True), obj_noserver=obj(0, server=0), obj_rot=obj(0, rot=NEG_ZERO),
        obj_turned=obj(0, rot=TURNED, vel=VEL),
        vehicle=vehicle(IDENT), vehicle_rot=vehicle(TURNED),
        ai_plain=ai(lambda: None), ai_chase=ai(chase), ai_stuck_b4=ai(stuck(0xb4)),
        ai_stuck_118=ai(stuck(0x118)), ai_stuck_13f=ai(stuck(0x13f)), ai_stuck_slow=ai(slow),
        ai_red=ai(red), ai_light=ai(light), ai_gear_up=ai(gear(9.0)), ai_gear_down=ai(gear(4.0)),
        nwp_dest=nwp(dest), nwp_reroll=nwp(reroll),
    )
    return out


def run_harness(d, cover=True):
    cmd = [str(ROOT / 'build/hook/harness.exe'), str(ROOT / 'build/hook/024d_fixed.exe'),
           str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt')]
    env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'))
    covers = []
    if cover:
        cenv = dict(env, HARNESS_COVER=str(ROOT / 'build/lift/blocks.txt'))
        out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=cenv, cwd=ROOT).stdout
        covers = [ln.strip() for ln in out.splitlines() if ln.startswith('cover ')]
    out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=env, cwd=ROOT).stdout
    res = [ln for ln in out.splitlines() if 'functions tested' in ln]
    diffs = [ln for ln in out.splitlines() if 'DIFF' in ln][:6]
    return (res[-1] if res else 'NO RESULT: ' + out[-300:]), covers, diffs


class CaseBase:
    """Holds the snapshot copy (cases() reads the attribute s)."""

    def __init__(self, base, ctx):
        self.s = copy.deepcopy(base)
        self.bot, self.car = ctx.bot, ctx.car


def main():
    argv = sys.argv[1:]
    run = '--run' in argv
    cover = '--no-cover' not in argv
    argv = [a for a in argv if not a.startswith('--')]
    rec, out = Path(argv[0]), Path(argv[1])
    base = Snapshot(rec / 'sv_snapshot.bin')
    # A bot and its car: the arguments of the first ai_playercar call of the recording.
    bot = car = None
    for ln in (rec / 'sv_args.txt').open():
        if ln.startswith('R e4d0 '):
            bot, car = [int(x[2:], 16) for x in ln.split()[2:4]]
            break
    ctx = Ctx(base, bot, car)
    want = argv[2].split(',') if len(argv) > 2 else None
    names = cases(CaseBase(base, ctx)).keys()
    for name in names:
        if want and name not in want:
            continue
        if name.startswith(('ai_', 'nwp_')) and (bot is None or car is None):
            print('skip', name, '(no bot in this snapshot)')
            continue
        c = CaseBase(base, ctx)
        args = cases(c)[name]()
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        c.s.save(d / 'sv_snapshot.bin')
        (d / 'sv_args.txt').write_text('\n'.join(args) + '\n')
        if run:
            result, covers, diffs = run_harness(d, cover)
            print('%-14s %s' % (name, result), flush=True)
            for ln in covers + diffs:
                print('    ' + ln)
            shutil.rmtree(d, ignore_errors=True)
        else:
            print('case', name, len(args), 'calls')


if __name__ == '__main__':
    main()
