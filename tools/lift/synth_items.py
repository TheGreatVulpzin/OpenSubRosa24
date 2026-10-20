"""Test the paths of the item and bullet functions that the recordings may not reach.

Reads a recording snapshot, builds one situation (a phone call, a door, a shot at a head...)
and writes a new snapshot and an args file with the calls of one function, in one folder per
case:

    python -I tools/lift/synth_items.py <recording dir> <out dir> [case,...]
    build/hook/harness.exe build/hook/024d_fixed.exe <out dir>/<case>/sv_snapshot.bin <out dir>/<case>/sv_args.txt <function>

The functions: setupitemtypes, itemweaponsimulation (30ba0), itemlogic (4c8c0),
item_physicssimulation (4cee0), itemsimulation (542a0), human_itemsimulation (4bd60) and
bulletsimulation (48ec0). The output holds the game state of the recording: keep it in build/.
A snapshot is about 120 MB. With --run, the script runs the harness on each case and deletes
the case at once. The cover count is the number of basic blocks of the function that the case
reaches (HARNESS_COVER):

    python -I tools/lift/synth_items.py --run <recording dir> <work dir> [case,...]
"""
import math
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot, find, HUMANS, HUMAN_SIZE, OBJECTS, OBJECT_SIZE, ITEMS, ITEM_SIZE

GAME = 0x90dc540
GAME_TYPE, GAME_STATE = GAME + 0x8, GAME + 0x20
TICKS = 0x90dc578
PLAYERS, PLAYER_SIZE = 0xededd00, 0x2408
ITEMTYPES, ITEMTYPE_SIZE = 0x10357400, 0x198
BULLETS, BULLET_SIZE, NUMOFBULLETS = 0x1409ba0, 0x5c, 0x17529a40

# Item fields.
I_ACTIVE, I_PHYS, I_REST, I_TYPE, I_MASS, I_TTL = 0x0, 0x4, 0x8, 0xc, 0x10, 0x14
I_PHUMAN, I_PITEM, I_SLOT, I_NCHILD, I_CHILD = 0x18, 0x1c, 0x20, 0x24, 0x28
I_POS, I_POSPREV, I_VEL, I_ROT, I_AMOM = 0x48, 0x54, 0x6c, 0x84, 0xa8
I_COOLDOWN, I_AMMO, I_TRIGGER, I_FLAGS, I_FLAGSPREV = 0xd8, 0xe0, 0xe8, 0xec, 0xf0
I_LINKED, I_PHONEID, I_RING, I_INFO, I_NUMBER, I_STATE = 0xf8, 0xfc, 0x100, 0x104, 0x108, 0x110
I_VEHICLE, I_INERTIA, I_INVINERTIA, I_ENDS, I_ENDPOS = 0x114, 0x118, 0x124, 0x17c, 0x194

# Human fields.
H_PLAYER, H_VEHICLE, H_HEALTH, H_PROTECT, H_STUN, H_POS = 0x8, 0x14, 0x28, 0x34, 0x44, 0x48
H_BUTTONS, H_PREV, H_BONES, H_INV, H_BLEED = 0xbc, 0xc0, 0xc8, 0x291c, 0x2b1c
BONE_SIZE = 0xe0

OBJ_FACES, OBJ_NFACES, OBJ_VERTS, OBJ_WHEELS, OBJ_NWHEELS = 0x7b8, 0x7b4, 0x4bbc, 0x5eec, 0x5ee8

IDENTITY = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)


class Case:
    """One snapshot with helpers to write items, humans and bullets."""

    def __init__(self, rec, ctx):
        self.s = Snapshot(rec / 'sv_snapshot.bin')
        self.ctx = ctx
        self.next_free = 0

    def free_item(self):
        i = self.ctx['free'][self.next_free]
        self.next_free += 1
        return i

    def item(self, i, t, pos, **kw):
        """Make item i of type t at pos. Fields: a dict of offset to value, in kw['f'] (ints or
        floats) and the keys active, rot, vel, ang_momentum."""
        s, a = self.s, ITEMS + i * ITEM_SIZE
        for k in range(0, ITEM_SIZE, 4):
            s.write(a + k, 'i', 0)
        T = ITEMTYPES + t * ITEMTYPE_SIZE
        mass = s.read(T + 4, 'f')[0]
        inv = s.read(T + 0x8c, 'fff')
        s.write(a + I_ACTIVE, 'i', 1)
        s.write(a + I_PHYS, 'i', 1)
        s.write(a + I_TYPE, 'i', t)
        s.write(a + I_MASS, 'f', mass)
        s.write(a + I_TTL, 'i', 0xffff)
        s.write(a + I_PHUMAN, 'i', -1)
        s.write(a + I_PITEM, 'i', -1)
        s.write(a + I_AMMO, 'i', s.read(T + 0x1c, 'i')[0])
        s.write(a + I_LINKED, 'i', -1)
        s.write(a + I_VEHICLE, 'i', -1)
        s.write(a + I_POS, 'fff', *pos)
        s.write(a + I_POSPREV, 'fff', *pos)
        s.write(a + I_VEL, 'fff', *kw.get('vel', (0.0, 0.0, 0.0)))
        s.write(a + I_ROT, 'f' * 9, *kw.get('rot', IDENTITY))
        s.write(a + I_AMOM, 'fff', *kw.get('ang_momentum', (0.0, 0.0, 0.0)))
        s.write(a + I_INVINERTIA, 'fff', *inv)
        s.write(a + I_INERTIA, 'fff', *[1.0 / x if x else 0.0 for x in inv])
        for off, v in kw.get('f', {}).items():
            s.write(a + off, 'f' if isinstance(v, float) else 'i' if v < 0x80000000 else 'I', v)
        return a

    def set_item(self, i, fields):
        for off, v in fields.items():
            self.s.write(ITEMS + i * ITEM_SIZE + off, 'f' if isinstance(v, float) else 'i', v)

    def human(self, off, fmt, *v):
        self.s.write(HUMANS + self.ctx['h'] * HUMAN_SIZE + off, fmt, *v)

    def bone_pos(self, b, h=None):
        h = self.ctx['h'] if h is None else h
        return self.s.read(HUMANS + h * HUMAN_SIZE + H_BONES + b * BONE_SIZE, 'fff')

    def head_rot(self):
        return self.s.read(HUMANS + self.ctx['h'] * HUMAN_SIZE + H_BONES + 2 * BONE_SIZE + 0x30, 'f' * 9)

    def inventory(self, slot, items):
        base = H_INV + slot * 0x24
        self.human(base, 'i', len(items))
        for k, it in enumerate(items):
            self.human(base + 4 + 4 * k, 'i', it)

    def hold(self, i, slot):
        """Item i is held in inventory slot `slot` of the test human."""
        self.set_item(i, {I_PHUMAN: self.ctx['h'], I_SLOT: slot})
        self.inventory(slot, [i])

    def bullet(self, t, pos, vel, owner=-1, ttl=600):
        s, n = self.s, self.s.read(NUMOFBULLETS, 'i')[0]
        a = BULLETS + n * BULLET_SIZE
        for k in range(0, BULLET_SIZE, 4):
            s.write(a + k, 'i', 0)
        s.write(a, 'i', t)
        s.write(a + 4, 'i', ttl)
        s.write(a + 8, 'i', owner)
        s.write(a + 0xc, 'f', s.read(0xf437320 + t * 0x18, 'f')[0])
        s.write(a + 0x10, 'f', 0.0027222223579883575)
        s.write(a + 0x14, 'fff', *pos)
        s.write(a + 0x20, 'fff', *[pos[k] - vel[k] for k in range(3)])
        s.write(a + 0x2c, 'fff', *vel)
        s.write(NUMOFBULLETS, 'i', n + 1)

    def aim(self, t, target, direction, speed, owner=-1):
        """A bullet that crosses `target` in this tick, moving along `direction`."""
        d = math.sqrt(sum(x * x for x in direction))
        v = [x / d * speed for x in direction]
        self.bullet(t, [target[k] + v[k] * 0.5 for k in range(3)], v, owner)
        # bulletsimulation moves pos by vel, so start `speed/2` before the target.
        a = BULLETS + (self.s.read(NUMOFBULLETS, 'i')[0] - 1) * BULLET_SIZE
        self.s.write(a + 0x14, 'fff', *[target[k] - v[k] * 0.5 for k in range(3)])
        self.s.write(a + 0x20, 'fff', *[target[k] - v[k] * 0.5 for k in range(3)])


def add(v, w, k=1.0):
    return [v[i] + w[i] * k for i in range(3)]


def cases(c):
    """name -> (function name, lambda that edits c and returns the list of arg lines)."""
    ctx, out = c.ctx, {}
    h = ctx['h']
    pos = list(c.bone_pos(2))
    above = add(pos, (0.0, 1.5, 0.0))

    def gun_state(t, f=None):
        i = c.free_item()
        c.item(i, t, pos, f={**{I_AMMO: 5, I_COOLDOWN: 0, I_TRIGGER: 1, I_FLAGS: 1}, **(f or {})})
        c.hold(i, 0)
        return i

    def rec_player(top=0, team=1):
        p = HUMANS and c.s.read(HUMANS + h * HUMAN_SIZE + H_PLAYER, 'i')[0]
        c.s.write(PLAYERS + p * PLAYER_SIZE + 0x64, 'i', team)
        c.s.write(PLAYERS + p * PLAYER_SIZE + 0x70, 'i', top)
        return p

    def call(rva, *a):
        return ['R %x' % rva + ''.join(' V:%x' % (x & 0xffffffff) for x in a)]

    # itemweaponsimulation
    def gun_fire():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(0))
    def gun_fire_smg():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(8))
    def magnum_semi():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(4, {I_TRIGGER: 5}))
    def magnum_ramp():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(4, {I_TRIGGER: 2}))
    def gun_cooldown():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(0, {I_COOLDOWN: 5}))
    def gun_release():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(10, {I_FLAGS: 0, I_TRIGGER: 4, I_COOLDOWN: 9}))
    def last_round():
        c.s.write(GAME_STATE, 'i', 2)
        return call(0x30ba0, gun_state(0, {I_AMMO: 1}))
    def reload_chamber():
        c.s.write(GAME_STATE, 'i', 2)
        g = gun_state(0, {I_AMMO: 0, I_TRIGGER: 0})
        m = c.free_item()
        c.item(m, 1, pos, f={I_AMMO: 10, I_PITEM: g, I_SLOT: 2})
        c.set_item(g, {I_NCHILD: 1, I_CHILD: m})
        return call(0x30ba0, g)
    def team_top():
        c.s.write(GAME_STATE, 'i', 1)
        rec_player(top=1)
        return call(0x30ba0, gun_state(2))
    def team_not_top():
        c.s.write(GAME_STATE, 'i', 1)
        rec_player(top=0)
        return call(0x30ba0, gun_state(2))

    # itemlogic
    def phone_pair(a_state, b_state, a_number=0, f=None):
        a, b = c.free_item(), c.free_item()
        c.item(a, 23, add(pos, (0.5, 0, 0)), f={**{I_STATE: a_state, I_NUMBER: a_number, I_PHONEID: 1111}, **(f or {})})
        c.item(b, 23, add(pos, (-0.5, 0, 0)), f={I_STATE: b_state, I_PHONEID: 2222})
        return a, b
    def phone_dial():
        phone_pair(1, 0, a_number=2222)
        return call(0x4c8c0)
    def phone_dial_use():
        phone_pair(0, 0, f={I_FLAGS: 1})
        return call(0x4c8c0)
    def phone_off_use():
        phone_pair(1, 0, f={I_FLAGS: 1})
        return call(0x4c8c0)
    def phone_ring_timeout():
        a, b = phone_pair(2, 2)
        c.set_item(a, {I_LINKED: b, I_RING: 0x2ff})
        c.s.write(TICKS, 'i', 0x80)
        return call(0x4c8c0)
    def phone_ring_sound():
        a, b = phone_pair(2, 0)
        c.set_item(a, {I_LINKED: b, I_RING: 3})
        c.s.write(TICKS, 'i', 0x100)
        return call(0x4c8c0)
    def phone_lost():
        a, b = phone_pair(3, 0)
        c.set_item(a, {I_LINKED: b})
        c.set_item(b, {0x0: 0})
        return call(0x4c8c0)
    def phone_answer():
        a, b = phone_pair(2, 2, f={I_FLAGS: 1})
        c.set_item(b, {I_LINKED: a})
        return call(0x4c8c0)
    def phone_hangup():
        a, b = phone_pair(3, 3, f={I_FLAGS: 1})
        c.set_item(a, {I_LINKED: b})
        c.set_item(b, {I_LINKED: a})
        return call(0x4c8c0)
    def briefcase():
        a, b = c.free_item(), c.free_item()
        c.item(a, 13, pos, f={I_FLAGS: 1})
        c.item(b, 14, pos, f={I_FLAGS: 1})
        return call(0x4c8c0)
    def bandage():
        c.human(H_HEALTH, 'i', 5)
        c.human(H_BLEED, 'i', 1)
        a = c.free_item()
        c.item(a, 12, pos, f={I_FLAGSPREV: 1, I_AMMO: 2})
        return call(0x4c8c0)
    def bandage_last():
        c.human(H_HEALTH, 'i', 50)
        c.human(H_BLEED, 'i', 1)
        a = c.free_item()
        c.item(a, 12, pos, f={I_FLAGSPREV: 1, I_AMMO: 1})
        return call(0x4c8c0)
    def key_in_car():
        a = c.free_item()
        c.human(H_VEHICLE, 'i', ctx['car'])
        c.item(a, 24, pos, f={I_VEHICLE: ctx['car'], I_PHUMAN: h})
        return call(0x4c8c0)
    def key_away():
        a = c.free_item()
        c.item(a, 24, pos, f={I_VEHICLE: ctx['car']})
        return call(0x4c8c0)
    def ttl_loose():
        a, b = c.free_item(), c.free_item()
        c.item(a, 26, pos, f={I_TTL: 20000})
        c.item(b, 26, pos, f={I_TTL: 100, I_NCHILD: 1})
        return call(0x4c8c0)
    def ttl_gametype3():
        c.s.write(GAME_TYPE, 'i', 3)
        a = c.free_item()
        c.item(a, 26, pos, f={I_TTL: 100})
        return call(0x4c8c0)
    def ttl_held():
        a = c.free_item()
        c.item(a, 26, pos, f={I_TTL: 700})
        c.hold(a, 3)
        return call(0x4c8c0)

    # item_physicssimulation and itemsimulation: the door
    def door(vel=(0.0, 0.0, 0.0), at=None, rest=0, amom=(0.0, 0.0, 0.0), parent=None):
        i = c.free_item()
        p = at or add(pos, (0.0, 0.0, 0.0))
        ends = [(-0.5, -1.0, 0.0), (-0.5, 1.0, 0.0)]
        endpos = [add(p, e) for e in ends]
        c.item(i, 25, p, vel=vel, ang_momentum=amom, f={I_REST: rest})
        for k in range(2):
            c.s.write(ITEMS + i * ITEM_SIZE + I_ENDS + 12 * k, 'fff', *ends[k])
            c.s.write(ITEMS + i * ITEM_SIZE + I_ENDPOS + 12 * k, 'fff', *add(endpos[k], (0.01 * k, 0.02, 0.0)))
        return i
    def door_physics():
        return call(0x4cee0, door(vel=(0.01, 0.0, 0.02), amom=(0.0, 0.001, 0.0)))
    def door_hit_human():
        return call(0x4cee0, door(at=list(c.bone_pos(3))))
    def door_sim():
        door(vel=(0.0, 0.0, 0.01), amom=(0.0, 0.002, 0.0))
        return call(0x542a0)

    # itemsimulation: a loose item
    def loose_fall():
        a = c.free_item()
        c.item(a, 26, above, vel=(0.01, -0.05, 0.0))
        return call(0x542a0)
    def loose_spin():
        a = c.free_item()
        c.item(a, 13, above, vel=(0.0, 0.0, 0.0), ang_momentum=(0.2, 0.1, 0.3))
        return call(0x542a0)
    def loose_ground():
        a = c.free_item()
        low = add(c.bone_pos(0), (0.0, -0.5, 0.0))
        c.item(a, 13, low, vel=(0.0, -0.02, 0.01), ang_momentum=(0.05, 0.0, 0.02))
        return call(0x542a0)
    def loose_rest():
        a = c.free_item()
        c.item(a, 26, above, f={I_REST: 1})
        return call(0x542a0)
    def loose_void():
        a = c.free_item()
        c.item(a, 26, (0.0, -40.0, 0.0), f={I_TTL: 500})
        return call(0x542a0)
    def cash_in_case():
        a, b, d = c.free_item(), c.free_item(), c.free_item()
        c.item(a, 13, above)
        c.item(b, 15, above, f={I_PITEM: a, I_SLOT: 2})
        c.item(d, 16, above, f={I_PITEM: a, I_SLOT: 3})
        return call(0x542a0)
    def disk_in_case():
        a, b = c.free_item(), c.free_item()
        c.item(a, 14, above, rot=(0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
        c.item(b, 17, above, f={I_PITEM: a, I_SLOT: 2})
        return call(0x542a0)
    def mag_in_gun():
        a, b = c.free_item(), c.free_item()
        c.item(a, 0, above)
        c.item(b, 1, above, f={I_PITEM: a, I_SLOT: 0})
        return call(0x542a0)
    def held_item():
        a = c.free_item()
        c.item(a, 26, above, vel=(0.0, -0.01, 0.0))
        c.hold(a, 0)
        return call(0x542a0)
    def held_pocket():
        a = c.free_item()
        c.item(a, 26, above, vel=(0.0, -0.01, 0.0))
        c.hold(a, 3)
        return call(0x542a0)
    def countdown():
        a = c.free_item()
        c.item(a, 26, above, f={0xf4: 5, I_REST: 1})
        return call(0x542a0)

    # human_itemsimulation
    def hand_call():
        return call(0x4bd60, h)
    def pickup():
        r = c.head_rot()
        c.human(H_BUTTONS, 'i', 0)
        c.human(H_PREV, 'i', 1)
        a = c.free_item()
        c.item(a, 26, add(pos, r[6:9], -0.5))
        return hand_call()
    def pickup_far():
        c.human(H_BUTTONS, 'i', 0)
        c.human(H_PREV, 'i', 1)
        a = c.free_item()
        c.item(a, 26, add(pos, (3.0, 0, 0)))
        return hand_call()
    def door_grab():
        r = c.head_rot()
        c.human(H_BUTTONS, 'i', 0)
        c.human(H_PREV, 'i', 1)
        c.human(0x9c, 'f', 0.4)
        door(at=add(pos, r[6:9], -0.5))
        return hand_call()
    def door_held():
        i = door()
        c.hold(i, 0)
        c.human(H_BUTTONS, 'i', 0x1001)
        c.human(0x9c, 'f', 0.7)
        return hand_call()
    def reach():
        c.human(H_BUTTONS, 'i', 1)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def hand_slot_pick():
        a = c.free_item()
        c.item(a, 26, pos)
        c.s.write(ITEMS + a * ITEM_SIZE + I_PHUMAN, 'i', h)
        c.s.write(ITEMS + a * ITEM_SIZE + I_SLOT, 'i', 2)
        c.inventory(2, [a])
        c.human(H_BUTTONS, 'i', 0x40)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def unload():
        g, m = c.free_item(), c.free_item()
        c.item(g, 0, pos, f={I_NCHILD: 1, I_CHILD: m, I_PHUMAN: h, I_SLOT: 1})
        c.item(m, 1, pos, f={I_PITEM: g, I_SLOT: 2})
        c.inventory(1, [g])
        c.human(H_BUTTONS, 'i', 0x1000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def unload_pocket():
        g, m = c.free_item(), c.free_item()
        c.item(g, 8, pos, f={I_NCHILD: 1, I_CHILD: m, I_PHUMAN: h, I_SLOT: 1})
        c.item(m, 9, pos, f={I_PITEM: g, I_SLOT: 2})
        c.inventory(1, [g])
        c.human(H_BUTTONS, 'i', 0x1000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def load():
        g, m = c.free_item(), c.free_item()
        c.item(g, 0, pos)
        c.hold(g, 1)
        c.item(m, 1, pos)
        c.hold(m, 0)
        c.human(H_BUTTONS, 'i', 0x1000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def load_wrong():
        g, m = c.free_item(), c.free_item()
        c.item(g, 4, pos)
        c.hold(g, 1)
        c.item(m, 1, pos)
        c.hold(m, 0)
        c.human(H_BUTTONS, 'i', 0x1000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def hold_gun():
        g = c.free_item()
        c.item(g, 0, pos)
        c.hold(g, 0)
        c.human(H_BUTTONS, 'i', 0x1001)
        return hand_call()
    def slot_keys():
        g = c.free_item()
        c.item(g, 8, pos)
        c.hold(g, 0)
        c.human(H_BUTTONS, 'i', 0x80)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def drop():
        g = c.free_item()
        c.item(g, 26, pos)
        c.hold(g, 0)
        c.human(H_BUTTONS, 'i', 0x20)
        return hand_call()
    def swap_one():
        g = c.free_item()
        c.item(g, 26, pos)
        c.hold(g, 1)
        c.human(H_BUTTONS, 'i', 0x2000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def swap_other():
        g = c.free_item()
        c.item(g, 26, pos)
        c.hold(g, 0)
        c.human(H_BUTTONS, 'i', 0x2000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def swap_both():
        g, d = c.free_item(), c.free_item()
        c.item(g, 26, pos)
        c.hold(g, 0)
        c.item(d, 13, pos)
        c.hold(d, 1)
        c.human(H_BUTTONS, 'i', 0x2000)
        c.human(H_PREV, 'i', 0)
        return hand_call()
    def pockets_move():
        g, d = c.free_item(), c.free_item()
        c.item(g, 8, pos)
        c.hold(g, 4)
        c.item(d, 13, pos)
        c.hold(d, 2)
        return hand_call()

    # bulletsimulation
    def shot(bone, hp=100, t=2, tax=0, owner=None, speed=0.8):
        c.human(H_HEALTH, 'i', hp)
        target = list(c.bone_pos(bone))
        own = ctx['owner'] if owner is None else owner
        c.aim(t, target, (0.3, -0.1, 1.0), speed, own)
        return call(0x48ec0)
    def bullet_human():
        return shot(3)
    def bullet_head():
        return shot(2)
    def bullet_head_low():
        return shot(2, t=1, speed=0.3)
    def bullet_limb():
        return shot(10, t=3, speed=1.2)
    def bullet_dying():
        return shot(3, hp=4)
    def bullet_slow():
        return shot(3, speed=0.01)
    def bullet_own_hand():
        return shot(5, owner=ctx['hpl'])
    def team_tax_3():
        c.s.write(GAME_TYPE, 'i', 3)
        return shot(3)
    def team_tax_4():
        c.s.write(GAME_TYPE, 'i', 4)
        c.human(H_PROTECT, 'i', 100)
        c.s.write(PLAYERS + ctx['hpl'] * PLAYER_SIZE + 0x64, 'i', 7)
        return shot(3)
    def bullet_immune():
        c.s.write(PLAYERS + ctx['hpl'] * PLAYER_SIZE + 0x44, 'i', 1)
        return shot(3)
    def bullet_level():
        low = add(c.bone_pos(0), (0.0, -2.0, 0.0))
        c.aim(2, low, (0.1, -1.0, 0.1), 3.0, ctx['owner'])
        return call(0x48ec0)

    def obj_face_target(face):
        o = OBJECTS + ctx['car'] * OBJECT_SIZE
        v = c.s.read(o + OBJ_FACES + face * 0x40 + 4, 'iiii')
        pts = [c.s.read(o + OBJ_VERTS + k * 0x10, 'fff') for k in v[:3]]
        cen = [sum(p[i] for p in pts) / 3 for i in range(3)]
        e1 = [pts[1][i] - pts[0][i] for i in range(3)]
        e2 = [pts[2][i] - pts[0][i] for i in range(3)]
        n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        return o, cen, n
    def face_shot(kw):
        face = ctx['face']
        o, cen, n = obj_face_target(face)
        for off, v in kw.items():
            c.s.write(o + OBJ_FACES + face * 0x40 + off, 'f' if isinstance(v, float) else 'i', v)
        c.aim(2, cen, [-x for x in n], 0.6, ctx['owner'])
        return call(0x48ec0)
    def bullet_face():
        return face_shot({0x38: 0})
    def bullet_window():
        return face_shot({0x38: 1, 0x34: 2.0, 0x3c: 6})
    def bullet_window_break():
        return face_shot({0x38: 1, 0x34: 2.0, 0x3c: 3})
    def bullet_window_armor():
        return face_shot({0x38: 1, 0x34: 9.0, 0x3c: 6})
    def bullet_wheel():
        o = OBJECTS + ctx['car'] * OBJECT_SIZE
        w = c.s.read(o + OBJ_WHEELS + 0x38, 'fff')
        c.s.write(o + OBJ_WHEELS + 8, 'i', 1)
        c.aim(2, w, (1.0, 0.0, 0.0), 0.8, ctx['owner'])
        return call(0x48ec0)
    def bullet_wheel_pop():
        o = OBJECTS + ctx['car'] * OBJECT_SIZE
        w = c.s.read(o + OBJ_WHEELS + 0x38, 'fff')
        c.s.write(o + OBJ_WHEELS + 8, 'i', 1)
        c.aim(1, w, (1.0, 0.0, 0.0), 0.8, ctx['owner'])
        return call(0x48ec0)
    def bullet_many():
        for k in range(4):
            shot(2 + k, t=k % 4)
        return call(0x48ec0)

    def setup_types():
        return call(0x6b30)

    names = {k: v for k, v in locals().items() if callable(v) and k in FUNCS_OF}
    return names


# Function of each case: used for the harness filter in the log.
FUNCS_OF = set('''gun_fire gun_fire_smg magnum_semi magnum_ramp gun_cooldown gun_release last_round reload_chamber
team_top team_not_top phone_dial phone_dial_use phone_off_use phone_ring_timeout phone_ring_sound phone_lost
phone_answer phone_hangup briefcase bandage bandage_last key_in_car key_away ttl_loose ttl_gametype3 ttl_held
door_physics door_hit_human door_sim loose_fall loose_spin loose_ground loose_rest loose_void cash_in_case
disk_in_case mag_in_gun held_item held_pocket countdown pickup pickup_far door_grab door_held reach hand_slot_pick
unload unload_pocket load load_wrong hold_gun slot_keys drop swap_one swap_other swap_both pockets_move
bullet_human bullet_head bullet_head_low bullet_limb bullet_dying bullet_slow bullet_own_hand team_tax_3
team_tax_4 bullet_immune bullet_level bullet_face bullet_window bullet_window_break bullet_window_armor
bullet_wheel bullet_wheel_pop bullet_many setup_types'''.split())


ROOT = Path(__file__).resolve().parents[2]
FUNC_NAMES = {0x6b30: 'setupitemtypes', 0x30ba0: 'itemweaponsimulation', 0x4c8c0: 'itemlogic',
              0x4cee0: 'item_physicssimulation', 0x542a0: 'itemsimulation',
              0x4bd60: 'human_itemsimulation', 0x48ec0: 'bulletsimulation'}


def run_harness(d, rva):
    """Run the harness on one case. Returns the result line and the number of blocks reached."""
    cmd = [str(ROOT / 'build/hook/harness.exe'), str(ROOT / 'build/hook/024d_fixed.exe'),
           str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt'), FUNC_NAMES[rva]]
    env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'))
    cover = dict(env, HARNESS_COVER=str(ROOT / 'build/lift/blocks.txt'))
    out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=cover, cwd=ROOT).stdout
    blocks = [int(ln.split()[4]) for ln in out.splitlines() if ln.startswith('cover ') and len(ln.split()) > 4]
    out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=env, cwd=ROOT).stdout
    res = [ln for ln in out.splitlines() if 'functions tested' in ln]
    return (res[-1] if res else 'NO RESULT: ' + out[-300:]), (blocks[0] if blocks else 0)


def main():
    run = '--run' in sys.argv
    if run:
        sys.argv.remove('--run')
    rec, out = Path(sys.argv[1]), Path(sys.argv[2])
    base = Snapshot(rec / 'sv_snapshot.bin')
    with_physics = lambda s, a: s.read(a, 'i')[0] != 0 and s.read(a + 4, 'i')[0] != 0
    humans = [h for h in find(base, HUMANS, HUMAN_SIZE, 512, with_physics)
              if base.read(HUMANS + h * HUMAN_SIZE + H_PLAYER, 'i')[0] != -1]
    owners = [p for p in range(256) if base.read(PLAYERS + p * PLAYER_SIZE, 'i')[0] != 0]
    cars = [o for o in find(base, OBJECTS, OBJECT_SIZE, 1024, lambda s, a: s.read(a, 'i')[0] != 0)
            if base.read(OBJECTS + o * OBJECT_SIZE + OBJ_NWHEELS, 'i')[0] > 0
            and base.read(OBJECTS + o * OBJECT_SIZE + OBJ_NFACES, 'i')[0] > 0]
    free = [i for i in range(1023, 0, -1) if base.read(ITEMS + i * ITEM_SIZE, 'i')[0] == 0]
    assert humans and cars and owners and len(free) > 20, 'the snapshot lacks a human, a car or free items'
    h = humans[0]
    hpl = base.read(HUMANS + h * HUMAN_SIZE + H_PLAYER, 'i')[0]
    owner = next((p for p in owners if p != hpl), hpl)
    ctx = dict(h=h, hpl=hpl, owner=owner, car=cars[0], free=free, face=0)
    print('human', h, 'player', hpl, 'owner', owner, 'object', ctx['car'])
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    for name in sorted(FUNCS_OF):
        if want and name not in want:
            continue
        c = Case(rec, ctx)
        make = cases(c)[name]
        args = make()
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        c.s.save(d / 'sv_snapshot.bin')
        (d / 'sv_args.txt').write_text('\n'.join(args) + '\n')
        if run:
            rva = int(args[0].split()[1], 16)
            result, blocks = run_harness(d, rva)
            print('%-22s %-24s blocks %3d  %s' % (name, FUNC_NAMES[rva], blocks, result), flush=True)
            shutil.rmtree(d, ignore_errors=True)
        else:
            print('case', name, args[0])


if __name__ == '__main__':
    main()
