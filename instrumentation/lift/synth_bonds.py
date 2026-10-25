"""Test the bond types of bondsimulation2 that no recording reaches.

Reads a recording snapshot, appends bonds of the given types after the last bond, and writes
a new snapshot and an args file with one bondsimulation2 call. Run the harness on the output:

    python -I instrumentation/lift/synth_bonds.py <recording dir> <out dir> [types]
    build/hook/harness.exe build/hook/024d_fixed.exe <out dir>/sv_snapshot.bin <out dir>/sv_args.txt bondsimulation2

Each type gets two bonds: one that pushes (the stretch points along the normal) and one that
pulls (the contact types then return early). The indices come from the snapshot: two humans
with physics, an item with physics, an object (with wheel bodies when there is one) and four
particles. The output holds the game state of the recording: keep it in build/.
"""
import struct
import sys
from pathlib import Path

PAGE = 4096
END = 0xffffffff

# RVAs (image address - 0x400000).
NUMOFBONDS, BONDS, BOND_SIZE = 0x17179a28, 0x17179a40, 0xec
BONDOPT, BONDOPT_SIZE = 0xf8f7b80, 0x40
HUMANS, HUMAN_SIZE = 0x15cbf80, 0x2b64
ITEMS, ITEM_SIZE = 0x91022c0, 0x1ac
OBJECTS, OBJECT_SIZE, NUM_WHEEL_BODIES = 0x5592600, 0x8230, 0x71ec
NUMOFPARTICLES = 0x1954d700 + 0x30


class Snapshot:
    def __init__(self, path):
        d = Path(path).read_bytes()
        self.head = d[:18]
        _, _, self.data_rva, self.size = struct.unpack_from('<IIII', d, 0)
        self.pages, pos = {}, 18
        while True:
            idx = struct.unpack_from('<I', d, pos)[0]
            pos += 4
            if idx == END:
                break
            self.pages[idx] = bytearray(d[pos:pos + PAGE])
            pos += PAGE
        self.tail = d[pos:]

    def _page(self, rva):
        idx, off = divmod(rva - self.data_rva, PAGE)
        assert 0 <= rva - self.data_rva < self.size, hex(rva)
        if idx not in self.pages:
            self.pages[idx] = bytearray(PAGE)
        return self.pages[idx], off

    def read(self, rva, fmt):
        n = struct.calcsize(fmt)
        out = bytearray()
        for k in range(n):
            page, off = self._page(rva + k)
            out.append(page[off])
        return struct.unpack('<' + fmt, bytes(out))

    def write(self, rva, fmt, *values):
        raw = struct.pack('<' + fmt, *values)
        for k, b in enumerate(raw):
            page, off = self._page(rva + k)
            page[off] = b

    def save(self, path):
        out = bytearray(self.head)
        for idx in sorted(self.pages):
            out += struct.pack('<I', idx) + self.pages[idx]
        out += struct.pack('<I', END) + self.tail
        Path(path).write_bytes(out)


def find(snap, base, size, count, test):
    return [i for i in range(count) if test(snap, base + i * size)]


def main():
    rec, out = Path(sys.argv[1]), Path(sys.argv[2])
    types = [int(t) for t in sys.argv[3].split(',')] if len(sys.argv) > 3 else [3, 11, 13, 14, 15]
    snap = Snapshot(rec / 'sv_snapshot.bin')

    with_physics = lambda s, a: s.read(a, 'i')[0] != 0 and s.read(a + 4, 'i')[0] != 0
    humans = find(snap, HUMANS, HUMAN_SIZE, 512, with_physics)
    items = find(snap, ITEMS, ITEM_SIZE, 1024, with_physics)
    objects = find(snap, OBJECTS, OBJECT_SIZE, 1024, lambda s, a: s.read(a, 'i')[0] != 0)
    wheeled = [o for o in objects if snap.read(OBJECTS + o * OBJECT_SIZE + NUM_WHEEL_BODIES, 'i')[0] > 0]
    nparticles = snap.read(NUMOFPARTICLES, 'i')[0]
    print('humans', humans[:4], 'items', items[:4], 'objects', objects[:4], 'wheeled', wheeled[:4],
          'particles', nparticles)
    assert len(humans) >= 2 and items and objects and nparticles >= 4, 'the snapshot lacks bodies'
    h1, h2, item = humans[0], humans[1], items[0]
    obj = (wheeled or objects)[0]

    n = snap.read(NUMOFBONDS, 'i')[0]
    added = 0
    for t in types:
        for rest in (0.02, -1.0):
            b, o = BONDS + n * BOND_SIZE, BONDOPT + n * BONDOPT_SIZE
            for k in range(0, BOND_SIZE, 4):
                snap.write(b + k, 'i', 0)
            for k in range(0, BONDOPT_SIZE, 4):
                snap.write(o + k, 'i', 0)
            snap.write(b + 0x0, 'i', t)
            snap.write(b + 0x8, 'i', 0x10000)
            snap.write(b + 0x14, 'iiii', 0, 1, 2, 3)            # particle_a to particle_d
            snap.write(b + 0x28, 'fff', 0.1, 0.0, 0.05)         # arm
            snap.write(b + 0x34, 'fff', 0.1, 0.05, 0.0)         # local_a
            snap.write(b + 0x40, 'fff', 0.0, 0.1, 0.05)         # local_b
            snap.write(b + 0x4c, 'fff', 0.3, 0.3, 0.4)          # weights
            snap.write(b + 0x60, 'f', rest)                     # rest_length
            snap.write(b + 0x6c, 'ff', 0.0625, 0.25)            # stiffness, damping
            snap.write(b + 0x74, 'fff', 0.0, 1.0, 0.0)          # normal
            snap.write(b + 0x80, 'f', 0.6)                      # friction
            owner = obj if t == 15 else h1
            target = h2 if t == 13 else item
            snap.write(b + 0x90, 'iiii', owner, 3 if t != 15 else 0, target, 5)  # owner, bone_a, target, bone_b
            snap.write(o + 0x8, 'fff', 0.01, 0.02, -0.01)       # dir
            snap.write(o + 0x18, 'f', 0.5)                      # arm_a.x: the gain of type 11
            snap.write(o + 0x34, 'fff', 0.4, 0.6, 0.25)         # share_a, share_b, damping
            n += 1
            added += 1
    snap.write(NUMOFBONDS, 'i', n)
    out.mkdir(parents=True, exist_ok=True)
    snap.save(out / 'sv_snapshot.bin')
    (out / 'sv_args.txt').write_text('R 41de0\n')
    print('added', added, 'bonds; numofbonds', n)


if __name__ == '__main__':
    main()
