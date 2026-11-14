"""Export a part of the Sub Rosa city from a client recording snapshot for the render demo.

The client keeps the built city in block sectors: each sector holds 8 x 8 x 8 cells, and each
cell holds a word (see docs/game/area.md). This script reads the sectors of the snapshot and
writes the cells near a center point to a small scene file.

Usage:
  python -I demo/render/export_scene.py <sv_snapshot.bin> <out.scene> [--stats] [--center x,z] [--radius cells]

Scene file (little-endian):
  'SRSC', int version (3), float block size, int cell count,
  then per cell: int16 x, int16 y, int16 z, uint8 kind, uint8 faces, 6 uint8 materials (one for each face).
  A face material comes from the texture value of the face: the low 6 bits, or for a value with
  bit 15 set, the low byte.
  kind: 1 = normal cube, 2 = mesh block (special or link). faces: visible faces of a cube
  (bits 0..5 = y-, z+, x-, z-, x+, y+), 63 for a mesh block.
  Then int object count, then per object: int type, 3 floats position, 9 floats rotation rows.
  Positions are in world units, with the center cell moved to x = z = 0.
  Then 64 material textures: int width, int height (64), RGBA bytes. Each one is the texture that
  texturelist.material[i] selects, scaled down by averaging.
"""
import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from synth_bonds import Snapshot  # noqa: E402

SECTORS, SECTOR_SIZE, SECTOR_BASE = 8192, 0x6068, 0x1dda5280
AREA_BLOCK_SIZE = 0x17265600 + 0x40
OBJECTS, OBJECT_SIZE = 0x5519120, 0x8230
TEXTURELIST_MATERIAL, TEXTURES, TEXTURE_SIZE = 0x14311840 + 4, 0x17b4b840, 0x70
MATERIALS, SMALL = 64, 64


def material_of(value):
    return (value & 0xff if value & 0x8000 else value) & 63


def heap_regions(snap):
    regions, t, pos = [], snap.tail, 0
    while pos + 8 <= len(t):
        base, size = struct.unpack_from('<II', t, pos)
        pos += 8
        regions.append((base, size, pos))
        pos += size
    return regions


def read_absolute(snap, regions, image_base, address, size):
    if image_base <= address < image_base + snap.data_rva + snap.size:
        return read_bytes(snap, address - image_base, size)
    for base, length, offset in regions:
        if base <= address and address + size <= base + length:
            return snap.tail[offset + address - base:offset + address - base + size]
    return None


def material_texture(snap, regions, image_base, material):
    """The RGBA pixels of the texture of a material, scaled down to SMALL x SMALL."""
    texture = snap.read(TEXTURELIST_MATERIAL + 4 * material, 'i')[0]
    record = TEXTURES + texture * TEXTURE_SIZE
    width, height = snap.read(record, '2i')
    pointer = snap.read(record + 0x30, 'I')[0]
    pixels = None
    if pointer and 0 < width <= 2048 and 0 < height <= 2048:
        pixels = read_absolute(snap, regions, image_base, pointer, width * height * 4)
    out = bytearray(SMALL * SMALL * 4)
    if not pixels:
        out[:] = bytes([128, 128, 128, 255]) * (SMALL * SMALL)
        return out
    sx, sy = max(1, width // SMALL), max(1, height // SMALL)
    for y in range(SMALL):
        for x in range(SMALL):
            acc = [0, 0, 0, 0]
            for dy in range(sy):
                row = ((y * height // SMALL + dy) % height) * width
                for dx in range(sx):
                    i = (row + (x * width // SMALL + dx) % width) * 4
                    acc[0] += pixels[i]
                    acc[1] += pixels[i + 1]
                    acc[2] += pixels[i + 2]
                    acc[3] += pixels[i + 3]
            n = sx * sy
            o = (y * SMALL + x) * 4
            out[o:o + 4] = bytes(v // n for v in acc)
    return out


def read_bytes(snap, rva, n):
    out = bytearray()
    page_size = 0x1000
    while n:
        page, off = snap._page(rva)
        take = min(n, page_size - off)
        out += page[off:off + take]
        rva += take
        n -= take
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snapshot')
    ap.add_argument('out')
    ap.add_argument('--stats', action='store_true')
    ap.add_argument('--center', default='')
    ap.add_argument('--radius', type=int, default=96)
    args = ap.parse_args()
    snap = Snapshot(args.snapshot)
    block = snap.read(AREA_BLOCK_SIZE, 'f')[0]
    cells = []
    kinds = Counter()
    for s in range(SECTORS):
        base = SECTOR_BASE + s * SECTOR_SIZE
        raw = read_bytes(snap, base, SECTOR_SIZE)
        if struct.unpack_from('<i', raw, 0)[0] == 0:
            continue
        words = struct.unpack_from('<512I', raw, 4)
        textures = struct.unpack_from('<4096H', raw, 0x804)
        px, py, pz = struct.unpack_from('<3i', raw, 0x5018)
        for i, w in enumerate(words):
            if w == 0:
                continue
            kind = w >> 30
            kinds[kind] += 1
            x = px * 8 + (i & 7)
            z = pz * 8 + ((i >> 3) & 7)
            y = py * 8 + (i >> 6)
            if kind == 1:
                faces = w & 63
                if faces == 0:
                    continue
                kind, faces = 1, faces
            else:
                kind, faces = 2, 63
            cells.append((x, y, z, kind, faces, tuple(material_of(textures[i * 8 + f]) for f in range(6))))
    if args.stats:
        xs = [c[0] for c in cells]
        ys = [c[1] for c in cells]
        zs = [c[2] for c in cells]
        print('block size', block)
        print('word kinds', dict(kinds))
        print('cells kept', len(cells))
        if cells:
            print('x', min(xs), max(xs), 'y', min(ys), max(ys), 'z', min(zs), max(zs))
    if args.center:
        cx, cz = (int(v) for v in args.center.split(','))
    else:
        mesh = [c for c in cells if c[1] > 8] or cells
        cx = sorted(c[0] for c in mesh)[len(mesh) // 2]
        cz = sorted(c[2] for c in mesh)[len(mesh) // 2]
    near = [c for c in cells if abs(c[0] - cx) <= args.radius and abs(c[2] - cz) <= args.radius]
    out = bytearray(b'SRSC' + struct.pack('<ifi', 3, block, len(near)))
    for x, y, z, kind, faces, materials in near:
        out += struct.pack('<hhhBB6B', x - cx, y, z - cz, kind, faces, *materials)
    objects = []
    for i in range(1024):
        base = OBJECTS + i * OBJECT_SIZE
        if snap.read(base, 'i')[0] == 0:
            continue
        kind = snap.read(base + 4, 'i')[0]
        pos = snap.read(base + 0x24, '3f')
        rot = snap.read(base + 0x3c, '9f')
        if abs(pos[0] / block - cx) <= args.radius and abs(pos[2] / block - cz) <= args.radius:
            objects.append((kind, pos[0] - cx * block, pos[1], pos[2] - cz * block, rot))
    out += struct.pack('<i', len(objects))
    for kind, x, y, z, rot in objects:
        out += struct.pack('<i3f9f', kind, x, y, z, *rot)
    regions = heap_regions(snap)
    image_base = struct.unpack_from('<I', snap.head, 4)[0]
    for m in range(MATERIALS):
        out += struct.pack('<ii', SMALL, SMALL) + material_texture(snap, regions, image_base, m)
    Path(args.out).write_bytes(out)
    print(f'center {cx},{cz}: {len(near)} cells and {len(objects)} objects written to {args.out}')
    for kind, x, y, z, rot in objects:
        print(f'  object type {kind} at {x:.1f} {y:.1f} {z:.1f}')


if __name__ == '__main__':
    main()
