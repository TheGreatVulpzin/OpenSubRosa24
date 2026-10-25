"""Test the vehicle paths of the object functions that the recordings do not reach.

Reads a recording snapshot, builds one situation (a flying helicopter, a car with wheel
bodies, a deleted object, two touching cars...) and writes a new snapshot and an args file
with the calls, in one folder per case:

    python -I instrumentation/lift/synth_vehicles.py <recording dir> <out dir> [case,...]
    build/hook/harness.exe build/hook/024d_fixed.exe <out dir>/<case>/sv_snapshot.bin <out dir>/<case>/sv_args.txt

With --run the script runs the harness on each case and deletes the case at once (a snapshot
is about 120 MB). Run it with the exact float compare too: put "verify_float_ulps=0" under
[hooks] in build/hook/subvulpes.ini, and delete the file afterwards.

    python -I instrumentation/lift/synth_vehicles.py --run [--no-cover] <recording dir> <work dir> [case,...]

The cases:
  heli_flight    object_rotorsimulation, objectsimulation_heli with a driver, objectsimulation
  wheel_bodies   a type 1 car: objectsimulation_car, objectenginesimulation2, objectwheelsimulation2,
                 objectwheelcollision2, objectsimulation
  wheel_pass1    the same with the solver pass 1 (the wheel shares come from the wheel records)
  delete         objecttimetolive removes an object with a human inside
  contact_car    a car pushed into another car, a particle below the ground
"""
import copy
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot, find, HUMANS, HUMAN_SIZE, OBJECTS, OBJECT_SIZE

PARTICLES, PARTICLE_SIZE = 0xb3e7200, 0xf0
BOND_PASS = 0x203dd0
ROOT = Path(__file__).resolve().parents[2]

# Object fields.
O_TYPE, O_STATE, O_HEALTH, O_TTL, O_KEY_AWAY, O_POS, O_ROT = 0x4, 0x8, 0xc, 0x1c, 0x20, 0x24, 0x3c
O_NPART, O_PART = 0xa8, 0xac
O_INPUT, O_BUTTONS = 0x5bbc, 0x5bdc
O_NUM_DRIVEN, O_DRIVEN = 0x5e90, 0x5e94
O_WHEELS, O_NWHEELS, WHEEL_SIZE = 0x5eec, 0x5ee8, 0x98
O_ROTOR, ROTOR_SIZE = 0x8074, 0x24
O_NBODIES, O_BODIES, BODY_SIZE = 0x71ec, 0x71f0, 0xe8
# Wheel fields.
W_PARTA, W_PARTB, W_WA, W_WB, W_POS = 0xc, 0x10, 0x14, 0x18, 0x38
W_SHARE_WHEEL, W_SHARE_BODY, W_SPRING_OFFSET = 0x84, 0x88, 0x8c
# Human fields.
H_VEHICLE, H_SEAT, H_PLAYER = 0x14, 0x18, 0x8


class Case:
    def __init__(self, base, ctx):
        self.s = copy.deepcopy(base)
        self.ctx = ctx

    def obj(self, i):
        return OBJECTS + i * OBJECT_SIZE

    def part(self, o, k):
        """Address of particle k of object o."""
        idx = self.s.read(self.obj(o) + O_PART + 4 * k, 'i')[0]
        return PARTICLES + idx * PARTICLE_SIZE

    def particle_pos(self, o, k):
        return self.s.read(self.part(o, k) + 0x14, 'fff')

    def driver(self, o):
        """The first human with a player sits in object o as the driver."""
        h = HUMANS + self.ctx['h'] * HUMAN_SIZE
        self.s.write(h + H_VEHICLE, 'i', o)
        self.s.write(h + H_SEAT, 'i', 0)

    def reset_particle_lasts(self, o):
        """Particle.vel_last = vel: the object feels no impact in the first tick."""
        n = self.s.read(self.obj(o) + O_NPART, 'i')[0]
        for k in range(n):
            a = self.part(o, k)
            self.s.write(a + 0x44, 'fff', *self.s.read(a + 0x2c, 'fff'))


def call(rva, *a):
    return 'R %x' % rva + ''.join(' V:%x' % (x & 0xffffffff) for x in a)


def cases(c):
    ctx, out = c.ctx, {}
    s = c.s

    def heli_flight():
        o = ctx['heli']
        a = c.obj(o)
        s.write(a + O_HEALTH, 'i', 100)
        s.write(a + O_KEY_AWAY, 'i', 0)
        s.write(a + O_INPUT, 'ffff', 0.5, 0.3, -0.4, 0.8)
        s.write(a + 0x5ee0, 'i', 1200)                 # rpm_net
        c.driver(o)
        pos = s.read(a + O_POS, 'fff')
        spots = [(4, 1, 0), (-4, 1, 0), (0, 1, 4), (0, 1, -4)]
        for k in range(6):
            r = a + O_ROTOR + k * ROTOR_SIZE
            if k < 4:
                p = [pos[i] + spots[k][i] for i in range(3)]
            else:
                p = list(c.particle_pos(o, 20 + k - 4))
            s.write(r, 'fff', *p)
            s.write(r + 0xc, 'ffffff', 0.01 * k, 0.0, 0.02, 0.01, 0.0, 0.0)
        c.reset_particle_lasts(o)
        return [call(0x24f60, o), call(0x240e0, o), call(0x56710)]

    def wheel_bodies(bondpass=0):
        def make():
            o = ctx['car']
            a = c.obj(o)
            s.write(a + O_TYPE, 'i', 1)
            s.write(a + O_STATE, 'i', 1)
            s.write(a + O_HEALTH, 'i', 100)
            s.write(a + O_KEY_AWAY, 'i', 0)
            s.write(a + O_INPUT, 'ffff', 0.6, 0.4, 0.8, 0.7)
            s.write(a + O_BUTTONS, 'I', 0)
            s.write(a + O_NBODIES, 'i', 2)
            s.write(a + O_NUM_DRIVEN, 'i', 2)
            s.write(a + O_DRIVEN, 'ii', 0, 1)
            rot = s.read(a + O_ROT, 'f' * 9)
            for k in range(2):
                b = a + O_BODIES + k * BODY_SIZE
                wpos = s.read(a + O_WHEELS + k * WHEEL_SIZE + W_POS, 'fff')
                for off in range(0, BODY_SIZE, 4):
                    s.write(b + off, 'i', 0)
                s.write(b, 'fff', *wpos)
                s.write(b + 0x24, 'f' * 9, *rot)
                s.write(b + 0x48, 'fff', 0.02, -0.03, 0.04)       # ang_momentum
                s.write(b + 0x54, 'fff', 0.1, 0.2, -0.1)          # w
                s.write(b + 0x6c, 'f', 0.5)                       # axle_inv_inertia
                s.write(b + 0x78, 'fff', 1.0, 1.0, 1.0)           # inv_inertia
                s.write(b + 0x84, 'fff', 1.0, 0.5, 0.25)          # mass, radius, half_width
                s.write(b + 0x90, 'iiii', 0, 1, 2, 3)
                s.write(b + 0xa0, 'ffff', 0.25, 0.25, 0.25, 0.25)
            if bondpass:
                for k in range(s.read(a + O_NWHEELS, 'i')[0]):
                    w = a + O_WHEELS + k * WHEEL_SIZE
                    s.write(w + W_SHARE_WHEEL, 'ff', 0.3, 0.7)
                    s.write(w + W_SPRING_OFFSET, 'fff', 0.01, -0.02, 0.005)
            s.write(BOND_PASS, 'i', bondpass)
            c.reset_particle_lasts(o)
            return [call(0x272b0, o), call(0x324c0), call(0x22f90), call(0x4e2c0, o, 0),
                    call(0x4e2c0, o, 1), call(0x56710)]
        return make

    def delete():
        o = ctx['dead']
        s.write(c.obj(o) + O_TTL, 'i', 1)
        h = HUMANS + ctx['h'] * HUMAN_SIZE
        s.write(h + H_VEHICLE, 'i', o)
        s.write(h + H_SEAT, 'i', 0)
        return [call(0x32340)]

    def contact_car():
        a, b = ctx['car_a'], ctx['car_b']
        n = s.read(c.obj(a) + O_NPART, 'i')[0]
        pa = [sum(c.particle_pos(a, k)[i] for k in range(n)) / n for i in range(3)]
        m = s.read(c.obj(b) + O_NPART, 'i')[0]
        pb = [sum(c.particle_pos(b, k)[i] for k in range(m)) / m for i in range(3)]
        for k in range(n):
            q = c.part(a, k) + 0x14
            p = s.read(q, 'fff')
            s.write(q, 'fff', *[p[i] + pb[i] - pa[i] for i in range(3)])
        # A particle of a third car below the ground.
        q = c.part(ctx['car_c'], 0) + 0x14
        p = s.read(q, 'fff')
        s.write(q, 'fff', p[0], p[1] - 3.0, p[2])
        return [call(0x56710)]

    out.update(heli_flight=heli_flight, wheel_bodies=wheel_bodies(0), wheel_pass1=wheel_bodies(1),
               delete=delete, contact_car=contact_car)
    return out


def run_harness(d, cover=True):
    """Run the harness on one case. Returns the result line and the cover lines."""
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


def main():
    run = '--run' in sys.argv
    if run:
        sys.argv.remove('--run')
    cover = '--no-cover' not in sys.argv
    if not cover:
        sys.argv.remove('--no-cover')
    rec, out = Path(sys.argv[1]), Path(sys.argv[2])
    base = Snapshot(rec / 'sv_snapshot.bin')
    objs = find(base, OBJECTS, OBJECT_SIZE, 1024, lambda s, a: s.read(a, 'i')[0] != 0)
    humans = [h for h in find(base, HUMANS, HUMAN_SIZE, 512, lambda s, a: s.read(a, 'i')[0] != 0)
              if base.read(HUMANS + h * HUMAN_SIZE + H_PLAYER, 'i')[0] != -1]
    heli = next((o for o in objs if base.read(OBJECTS + o * OBJECT_SIZE + O_TYPE, 'i')[0] == 11), 0x26)
    ctx = dict(h=humans[0], heli=heli, car=0xb, dead=0x15, car_a=0xc, car_b=0xd, car_c=0x9)
    for key in ('heli', 'car', 'dead', 'car_a', 'car_b', 'car_c'):
        assert ctx[key] in objs, 'object %s missing in the snapshot' % hex(ctx[key])
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    for name in cases(Case(base, ctx)):
        if want and name not in want:
            continue
        c = Case(base, ctx)
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
